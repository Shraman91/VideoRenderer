#include "video_renderer.hpp"
#include "charset.hpp"
#include "terminal.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <signal.h>
#endif

namespace ascii {

// Fast integer to string formatting (0-255) without heap allocation
inline void append_uint8(std::string& s, uint8_t v) {
    if (v >= 100) {
        s.push_back(static_cast<char>('0' + (v / 100)));
        v %= 100;
        s.push_back(static_cast<char>('0' + (v / 10)));
        s.push_back(static_cast<char>('0' + (v % 10)));
    } else if (v >= 10) {
        s.push_back(static_cast<char>('0' + (v / 10)));
        s.push_back(static_cast<char>('0' + (v % 10)));
    } else {
        s.push_back(static_cast<char>('0' + v));
    }
}

inline void append_int(std::string& s, int val) {
    if (val >= 1000) {
        s.append(std::to_string(val));
    } else if (val >= 100) {
        s.push_back(static_cast<char>('0' + (val / 100)));
        val %= 100;
        s.push_back(static_cast<char>('0' + (val / 10)));
        s.push_back(static_cast<char>('0' + (val % 10)));
    } else if (val >= 10) {
        s.push_back(static_cast<char>('0' + (val / 10)));
        s.push_back(static_cast<char>('0' + (val % 10)));
    } else if (val >= 0) {
        s.push_back(static_cast<char>('0' + val));
    }
}

inline void append_cursor_pos(std::string& s, int row, int col) {
    s.append("\033[");
    append_int(s, row);
    s.push_back(';');
    append_int(s, col);
    s.push_back('H');
}

inline void append_fg(std::string& s, uint8_t r, uint8_t g, uint8_t b) {
    s.append("\033[38;2;");
    append_uint8(s, r);
    s.push_back(';');
    append_uint8(s, g);
    s.push_back(';');
    append_uint8(s, b);
    s.push_back('m');
}

inline void append_bg(std::string& s, uint8_t r, uint8_t g, uint8_t b) {
    s.append("\033[48;2;");
    append_uint8(s, r);
    s.push_back(';');
    append_uint8(s, g);
    s.push_back(';');
    append_uint8(s, b);
    s.push_back('m');
}

#ifdef _WIN32
static std::string find_ffplay() {
    // 1. Check directory of current executable
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH)) {
        std::string dir(exe_path);
        size_t last_slash = dir.find_last_of("\\/");
        if (last_slash != std::string::npos) {
            std::string candidate = dir.substr(0, last_slash + 1) + "ffplay.exe";
            DWORD attr = GetFileAttributesA(candidate.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                return candidate;
            }
        }
    }
    // 2. Check MSYS2 / MinGW installation default
    DWORD attr = GetFileAttributesA("C:\\msys64\\mingw64\\bin\\ffplay.exe");
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return "C:\\msys64\\mingw64\\bin\\ffplay.exe";
    }
    // 3. Fallback to PATH
    return "ffplay";
}
#endif

#ifdef _WIN32
typedef LONG (NTAPI *pfnNtSuspendProcess)(HANDLE);
typedef LONG (NTAPI *pfnNtResumeProcess)(HANDLE);
static pfnNtSuspendProcess g_pNtSuspendProcess = nullptr;
static pfnNtResumeProcess g_pNtResumeProcess = nullptr;

static void init_nt_audio_funcs() {
    static bool initialized = false;
    if (initialized) return;
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll) {
        FARPROC pSuspend = GetProcAddress(ntdll, "NtSuspendProcess");
        FARPROC pResume = GetProcAddress(ntdll, "NtResumeProcess");
        std::memcpy(&g_pNtSuspendProcess, &pSuspend, sizeof(pSuspend));
        std::memcpy(&g_pNtResumeProcess, &pResume, sizeof(pResume));
    }
    initialized = true;
}

static HANDLE get_audio_job_object() {
    static HANDLE hJob = nullptr;
    if (!hJob) {
        hJob = CreateJobObjectA(nullptr, nullptr);
        if (hJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        }
    }
    return hJob;
}
#endif

class AudioPlayer;
static std::atomic<AudioPlayer*> g_active_audio_player{nullptr};

static void stop_global_audio();

#ifdef _WIN32
static BOOL WINAPI audio_console_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT || ctrl_type == CTRL_CLOSE_EVENT) {
        stop_global_audio();
        Terminal::restore();
        ExitProcess(0);
    }
    return FALSE;
}
#endif

// Audio subprocess player with bulletproof process lifecycle
class AudioPlayer {
public:
    AudioPlayer(const std::string& path, int delay_ms, bool no_audio)
        : path_(path), delay_ms_(delay_ms), no_audio_(no_audio),
          stop_requested_(false), is_paused_(false) {
#ifdef _WIN32
        process_handle_ = nullptr;
        init_nt_audio_funcs();
        static bool handler_set = false;
        if (!handler_set) {
            SetConsoleCtrlHandler(audio_console_ctrl_handler, TRUE);
            handler_set = true;
        }
#else
        pid_ = -1;
#endif
        g_active_audio_player.store(this);
        static bool atexit_set = false;
        if (!atexit_set) {
            std::atexit(stop_global_audio);
            atexit_set = true;
        }
    }

    ~AudioPlayer() {
        stop();
        AudioPlayer* expected = this;
        g_active_audio_player.compare_exchange_strong(expected, nullptr);
    }

    void start() {
        if (no_audio_ || stop_requested_) return;

        if (delay_ms_ > 0) {
            auto end_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms_);
            while (std::chrono::steady_clock::now() < end_time) {
                if (stop_requested_) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        if (stop_requested_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_requested_) return;

#ifdef _WIN32
        std::string ffplay_bin = find_ffplay();
        std::string cmd = "\"" + ffplay_bin + "\" -nodisp -autoexit -loglevel quiet";
        if (delay_ms_ < 0) {
            cmd += " -ss " + std::to_string(std::abs(delay_ms_) / 1000.0f);
        }
        cmd += " \"" + path_ + "\"";

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};

        if (CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()), nullptr, nullptr,
                          FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            process_handle_ = pi.hProcess;
            CloseHandle(pi.hThread);

            // Assign to job object: Windows kernel will kill child immediately if parent exits or crashes
            HANDLE hJob = get_audio_job_object();
            if (hJob) {
                AssignProcessToJobObject(hJob, pi.hProcess);
            }
        }
#else
        pid_t pid = fork();
        if (pid == 0) {
            execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet",
                   path_.c_str(), nullptr);
            _exit(1);
        } else if (pid > 0) {
            pid_ = pid;
        }
#endif
    }

    void pause() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_paused_) return;

#ifdef _WIN32
        if (process_handle_ && g_pNtSuspendProcess) {
            g_pNtSuspendProcess(process_handle_);
        }
#else
        if (pid_ > 0) {
            kill(pid_, SIGSTOP);
        }
#endif
        is_paused_ = true;
    }

    void resume() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_paused_) return;

#ifdef _WIN32
        if (process_handle_ && g_pNtResumeProcess) {
            g_pNtResumeProcess(process_handle_);
        }
#else
        if (pid_ > 0) {
            kill(pid_, SIGCONT);
        }
#endif
        is_paused_ = false;
    }

    void stop() {
        stop_requested_ = true;
        std::lock_guard<std::mutex> lock(mutex_);

#ifdef _WIN32
        if (process_handle_) {
            if (is_paused_ && g_pNtResumeProcess) {
                // Resume before terminating to avoid deadlocks
                g_pNtResumeProcess(process_handle_);
                is_paused_ = false;
            }
            TerminateProcess(process_handle_, 0);
            WaitForSingleObject(process_handle_, 300);
            CloseHandle(process_handle_);
            process_handle_ = nullptr;
        }
#else
        if (pid_ > 0) {
            if (is_paused_) {
                kill(pid_, SIGCONT);
                is_paused_ = false;
            }
            kill(pid_, SIGTERM);
            waitpid(pid_, nullptr, 0);
            pid_ = -1;
        }
#endif
    }

private:
    std::string path_;
    int delay_ms_;
    bool no_audio_;
    std::atomic<bool> stop_requested_;
    bool is_paused_;
    std::mutex mutex_;
#ifdef _WIN32
    HANDLE process_handle_;
#else
    pid_t pid_;
#endif
};

static void stop_global_audio() {
    AudioPlayer* p = g_active_audio_player.exchange(nullptr);
    if (p) {
        p->stop();
    }
}

static void calculate_dimensions(RenderMode mode, const VideoOptions& opts,
                                double aspect,
                                int term_w, int term_h,
                                int& render_w, int& render_h,
                                int& pad_x, int& pad_y) {
    int avail_w = std::max(10, term_w);
    int avail_h = std::max(4, term_h - 2); // Reserve 2 lines so cursor never triggers terminal scroll

    if (mode == RenderMode::Blocks) {
        // Half-block mode: 2 vertical pixels per terminal cell row
        int avail_h_px = avail_h * 2;

        if (opts.width > 0 && opts.height > 0) {
            render_w = opts.width;
            render_h = opts.height * 2;
        } else if (opts.width > 0) {
            render_w = opts.width;
            render_h = static_cast<int>(render_w / aspect);
        } else if (opts.fullscreen) {
            // Fullscreen: fill the maximum screen area while preserving aspect ratio
            render_w = avail_w;
            render_h = static_cast<int>(render_w / aspect);
            if (render_h > avail_h_px) {
                render_h = avail_h_px;
                render_w = static_cast<int>(render_h * aspect);
            }
        } else {
            // Default windowed: fit inside terminal width, capped comfortably
            int max_w = std::min(avail_w, 140);
            render_w = max_w;
            render_h = static_cast<int>(render_w / aspect);
            if (render_h > avail_h_px) {
                render_h = avail_h_px;
                render_w = static_cast<int>(render_h * aspect);
            }
        }

        // Half-blocks require an even vertical pixel count
        if (render_h % 2 != 0) render_h--;
        render_w = std::max(2, std::min(render_w, avail_w));
        render_h = std::max(2, std::min(render_h, avail_h_px));

        int cell_rows = render_h / 2;
        pad_x = std::max(0, (term_w - render_w) / 2);
        pad_y = std::max(0, (term_h - cell_rows) / 2);
    } else {
        // Ascii or Solid mode: 1 pixel per terminal cell (aspect ratio correction ~0.5)
        if (opts.width > 0 && opts.height > 0) {
            render_w = opts.width;
            render_h = opts.height;
        } else if (opts.width > 0) {
            render_w = opts.width;
            render_h = static_cast<int>(render_w / aspect * 0.5);
        } else if (opts.fullscreen) {
            render_w = avail_w;
            render_h = static_cast<int>(render_w / aspect * 0.5);
            if (render_h > avail_h) {
                render_h = avail_h;
                render_w = static_cast<int>(render_h * aspect * 2.0);
            }
        } else {
            int max_w = std::min(avail_w, 120);
            render_w = max_w;
            render_h = static_cast<int>(render_w / aspect * 0.5);
            if (render_h > avail_h) {
                render_h = avail_h;
                render_w = static_cast<int>(render_h * aspect * 2.0);
            }
        }

        render_w = std::max(2, std::min(render_w, avail_w));
        render_h = std::max(2, std::min(render_h, avail_h));

        pad_x = std::max(0, (term_w - render_w) / 2);
        pad_y = std::max(0, (term_h - render_h) / 2);
    }
}

int VideoRenderer::play(const VideoOptions& opts) {
    // 1. Initialize terminal environment for Windows Terminal / Console
    Terminal::initialize();

    if (opts.fullscreen) {
        Terminal::maximize_window();
    }

    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgb_frame = nullptr;
    AVPacket* packet = nullptr;
    uint8_t* prev_rgb_buffer = nullptr;

    int video_stream_idx = -1;
    int ret = 0;

    av_log_set_level(AV_LOG_QUIET);

    std::string video_path = opts.path;
    ret = avformat_open_input(&format_ctx, video_path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        std::string fallback_path = "../" + opts.path;
        if (avformat_open_input(&format_ctx, fallback_path.c_str(), nullptr, nullptr) >= 0) {
            video_path = fallback_path;
            ret = 0;
        } else {
            std::cerr << "Error: Could not open '" << opts.path << "'\n";
            return 1;
        }
    }

    ret = avformat_find_stream_info(format_ctx, nullptr);
    if (ret < 0) {
        std::cerr << "Error: Could not find stream info in '" << video_path << "'\n";
        avformat_close_input(&format_ctx);
        return 1;
    }

    for (unsigned i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = static_cast<int>(i);
            break;
        }
    }

    if (video_stream_idx == -1) {
        std::cerr << "Error: No video stream found in '" << opts.path << "'\n";
        avformat_close_input(&format_ctx);
        return 1;
    }

    AVStream* video_stream = format_ctx->streams[video_stream_idx];
    AVCodecParameters* codec_params = video_stream->codecpar;

    const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        std::cerr << "Error: Video codec not supported\n";
        avformat_close_input(&format_ctx);
        return 1;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "Error: Failed to allocate codec context\n";
        avformat_close_input(&format_ctx);
        return 1;
    }

    avcodec_parameters_to_context(codec_ctx, codec_params);
    codec_ctx->thread_count = 0; // Auto multi-threaded decoding

    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        std::cerr << "Error: Failed to open codec\n";
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return 1;
    }

    // Video aspect ratio
    double aspect = static_cast<double>(codec_ctx->width) / codec_ctx->height;
    if (video_stream->sample_aspect_ratio.num > 0 && video_stream->sample_aspect_ratio.den > 0) {
        aspect *= av_q2d(video_stream->sample_aspect_ratio);
    }

    // Query terminal dimensions
    int term_w = 80, term_h = 24;
    Terminal::get_size(term_w, term_h);

    RenderMode current_mode = opts.mode;
    int render_w = 0, render_h = 0;
    int pad_x = 0, pad_y = 0;

    calculate_dimensions(current_mode, opts, aspect, term_w, term_h,
                         render_w, render_h, pad_x, pad_y);

    frame = av_frame_alloc();
    rgb_frame = av_frame_alloc();
    packet = av_packet_alloc();

    if (!frame || !rgb_frame || !packet) {
        std::cerr << "Error: Failed to allocate FFmpeg frames\n";
        if (frame) av_frame_free(&frame);
        if (rgb_frame) av_frame_free(&rgb_frame);
        if (packet) av_packet_free(&packet);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return 1;
    }

    // Allocate rgb_frame with 32-byte SIMD alignment to guarantee zero heap corruption in sws_scale
    int buffer_size = av_image_alloc(
        rgb_frame->data, rgb_frame->linesize,
        render_w, render_h, AV_PIX_FMT_RGB24, 32
    );

    if (buffer_size < 0) {
        std::cerr << "Error: Failed to allocate image buffer\n";
        av_frame_free(&rgb_frame);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return 1;
    }

    // Scaling context (Bilinear for anti-aliasing)
    sws_ctx = sws_getContext(
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        render_w, render_h, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!sws_ctx) {
        std::cerr << "Error: Failed to create software scaler\n";
        av_freep(&rgb_frame->data[0]);
        av_frame_free(&rgb_frame);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return 1;
    }

    if (opts.smoothing > 0.0f) {
        prev_rgb_buffer = static_cast<uint8_t*>(av_mallocz(buffer_size));
    }

    // Framerate determination
    double source_fps = av_q2d(video_stream->avg_frame_rate);
    if (source_fps <= 0.0 || source_fps > 120.0) {
        source_fps = av_q2d(video_stream->r_frame_rate);
    }
    if (source_fps <= 0.0 || source_fps > 120.0) {
        source_fps = 30.0;
    }
    double target_fps = opts.fps > 0 ? opts.fps : source_fps;
    auto frame_duration = std::chrono::duration<double>(1.0 / target_fps);
    double time_base = av_q2d(video_stream->time_base);

    const Charset* cs = get_charset(opts.charset);
    const std::string& ramp = cs->ramp;

    // Audio setup
    std::thread audio_thread;
    AudioPlayer audio_player(video_path, opts.audio_delay, opts.no_audio);

    // Terminal display setup
    Terminal::hide_cursor();
    Terminal::enable_alternate_buffer();
    Terminal::set_synchronized_output(true);
    Terminal::clear();
    // Disable autowrap so rightmost characters never scroll
    std::cout << "\033[?7l" << std::flush;

    if (!opts.no_audio) {
        audio_thread = std::thread([&audio_player]() { audio_player.start(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    auto playback_start = std::chrono::steady_clock::now();
    int64_t start_pts = AV_NOPTS_VALUE;
    int frame_index = 0;
    int consecutive_skipped = 0;

    // Pre-allocated frame buffer for zero memory allocation during playback
    std::string frame_str;
    frame_str.reserve(render_w * (render_h / 2) * 36 + 16384);

    bool should_stop = false;

    // Main decode and presentation loop
    while (!should_stop && av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_idx) {
            ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0) {
                av_packet_unref(packet);
                continue;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    break;
                }

                // Interactive keyboard check
#ifdef _WIN32
                if (_kbhit()) {
                    int ch = _getch();
                    if (ch == 'q' || ch == 'Q' || ch == 27) { // q or ESC
                        should_stop = true;
                        audio_player.stop();
                        break;
                    } else if (ch == ' ') { // pause
                        audio_player.pause();
                        auto pause_start = std::chrono::steady_clock::now();
                        while (true) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(40));
                            if (_kbhit()) {
                                int rk = _getch();
                                if (rk == 'q' || rk == 'Q' || rk == 27) {
                                    should_stop = true;
                                    audio_player.stop();
                                    break;
                                }
                                break;
                            }
                        }
                        auto pause_end = std::chrono::steady_clock::now();
                        playback_start += (pause_end - pause_start);
                        if (!should_stop) {
                            audio_player.resume();
                        }
                    } else if (ch == 'm' || ch == 'M') { // cycle modes
                        if (current_mode == RenderMode::Blocks) current_mode = RenderMode::Ascii;
                        else if (current_mode == RenderMode::Ascii) current_mode = RenderMode::Solid;
                        else current_mode = RenderMode::Blocks;

                        int new_w = 0, new_h = 0;
                        Terminal::get_size(term_w, term_h);
                        calculate_dimensions(current_mode, opts, aspect, term_w, term_h,
                                             new_w, new_h, pad_x, pad_y);
                        if (new_w != render_w || new_h != render_h) {
                            render_w = new_w;
                            render_h = new_h;
                            sws_freeContext(sws_ctx);
                            sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                                                     render_w, render_h, AV_PIX_FMT_RGB24,
                                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
                            if (rgb_frame->data[0]) av_freep(&rgb_frame->data[0]);
                            buffer_size = av_image_alloc(rgb_frame->data, rgb_frame->linesize,
                                                         render_w, render_h, AV_PIX_FMT_RGB24, 32);
                            if (opts.smoothing > 0.0f) {
                                if (prev_rgb_buffer) av_free(prev_rgb_buffer);
                                prev_rgb_buffer = static_cast<uint8_t*>(av_mallocz(buffer_size));
                            }
                            frame_str.reserve(render_w * (render_h / 2) * 36 + 16384);
                            Terminal::clear();
                        }
                    } else if (ch == 'f' || ch == 'F') { // Fullscreen
                        Terminal::maximize_window();
                        int new_w = 0, new_h = 0;
                        Terminal::get_size(term_w, term_h);
                        VideoOptions fs_opts = opts;
                        fs_opts.fullscreen = true;
                        calculate_dimensions(current_mode, fs_opts, aspect, term_w, term_h,
                                             new_w, new_h, pad_x, pad_y);
                        if (new_w != render_w || new_h != render_h) {
                            render_w = new_w;
                            render_h = new_h;
                            sws_freeContext(sws_ctx);
                            sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                                                     render_w, render_h, AV_PIX_FMT_RGB24,
                                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
                            if (rgb_frame->data[0]) av_freep(&rgb_frame->data[0]);
                            buffer_size = av_image_alloc(rgb_frame->data, rgb_frame->linesize,
                                                         render_w, render_h, AV_PIX_FMT_RGB24, 32);
                            if (opts.smoothing > 0.0f) {
                                if (prev_rgb_buffer) av_free(prev_rgb_buffer);
                                prev_rgb_buffer = static_cast<uint8_t*>(av_mallocz(buffer_size));
                            }
                            frame_str.reserve(render_w * (render_h / 2) * 36 + 16384);
                            Terminal::clear();
                        }
                    }
                }
#endif

                // Periodic window resize check (only after playback is underway)
                if (frame_index > 0 && frame_index % 30 == 0) {
                    int cur_w = 0, cur_h = 0;
                    if (Terminal::get_size(cur_w, cur_h) && (cur_w != term_w || cur_h != term_h)) {
                        term_w = cur_w;
                        term_h = cur_h;
                        int new_w = 0, new_h = 0;
                        calculate_dimensions(current_mode, opts, aspect, term_w, term_h,
                                             new_w, new_h, pad_x, pad_y);
                        if (new_w != render_w || new_h != render_h) {
                            render_w = new_w;
                            render_h = new_h;
                            sws_freeContext(sws_ctx);
                            sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                                                     render_w, render_h, AV_PIX_FMT_RGB24,
                                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
                            if (rgb_frame->data[0]) av_freep(&rgb_frame->data[0]);
                            buffer_size = av_image_alloc(rgb_frame->data, rgb_frame->linesize,
                                                         render_w, render_h, AV_PIX_FMT_RGB24, 32);
                            if (opts.smoothing > 0.0f) {
                                if (prev_rgb_buffer) av_free(prev_rgb_buffer);
                                prev_rgb_buffer = static_cast<uint8_t*>(av_mallocz(buffer_size));
                            }
                            frame_str.reserve(render_w * (render_h / 2) * 36 + 16384);
                            Terminal::clear();
                        }
                    }
                }

                // PTS-based presentation timestamp calculation
                double target_sec = 0.0;
                int64_t best_pts = frame->best_effort_timestamp;
                if (best_pts != AV_NOPTS_VALUE && best_pts >= 0) {
                    if (start_pts == AV_NOPTS_VALUE) {
                        start_pts = best_pts;
                    }
                    target_sec = (best_pts - start_pts) * time_base;
                } else {
                    target_sec = frame_index / target_fps;
                }

                auto target_time = playback_start + std::chrono::duration<double>(target_sec);
                auto now = std::chrono::steady_clock::now();

                // Dynamic frame drop if falling behind real time
                if (now > target_time + frame_duration && consecutive_skipped < opts.max_frame_skip) {
                    consecutive_skipped++;
                    frame_index++;
                    continue;
                }
                consecutive_skipped = 0;

                // Scale decoded frame to target dimensions
                sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height,
                          rgb_frame->data, rgb_frame->linesize);

                // Temporal smoothing if enabled
                if (opts.smoothing > 0.0f && prev_rgb_buffer) {
                    float alpha = std::clamp(opts.smoothing, 0.0f, 0.95f);
                    float inv_alpha = 1.0f - alpha;
                    uint8_t* cur_data = rgb_frame->data[0];
                    for (int i = 0; i < buffer_size; ++i) {
                        uint8_t cur = cur_data[i];
                        uint8_t blended = static_cast<uint8_t>(cur * inv_alpha + prev_rgb_buffer[i] * alpha);
                        cur_data[i] = blended;
                        prev_rgb_buffer[i] = blended;
                    }
                }

                // Construct frame buffer atomically
                frame_str.clear();
                // Synchronized update start
                frame_str.append("\033[?2026h");

                const uint8_t* data = rgb_frame->data[0];
                int stride = rgb_frame->linesize[0];

                if (current_mode == RenderMode::Blocks) {
                    // Ultra-smooth Half-Block 24-bit TrueColor rendering
                    int num_lines = render_h / 2;
                    int prev_fg_r = -1, prev_fg_g = -1, prev_fg_b = -1;
                    int prev_bg_r = -1, prev_bg_g = -1, prev_bg_b = -1;

                    for (int line = 0; line < num_lines; ++line) {
                        // Position cursor directly at (row, col) - never use \n to prevent scrolling
                        append_cursor_pos(frame_str, 1 + pad_y + line, 1 + pad_x);

                        const uint8_t* top_row = data + (line * 2) * stride;
                        const uint8_t* bot_row = data + (line * 2 + 1) * stride;

                        for (int x = 0; x < render_w; ++x) {
                            int idx = x * 3;
                            int r1 = top_row[idx];
                            int g1 = top_row[idx + 1];
                            int b1 = top_row[idx + 2];
                            int r2 = bot_row[idx];
                            int g2 = bot_row[idx + 1];
                            int b2 = bot_row[idx + 2];

                            if (opts.quant > 1) {
                                r1 = (r1 / opts.quant) * opts.quant;
                                g1 = (g1 / opts.quant) * opts.quant;
                                b1 = (b1 / opts.quant) * opts.quant;
                                r2 = (r2 / opts.quant) * opts.quant;
                                g2 = (g2 / opts.quant) * opts.quant;
                                b2 = (b2 / opts.quant) * opts.quant;
                            }

                            if (r1 == r2 && g1 == g2 && b1 == b2) {
                                // Same color on both halves: space with background color
                                if (r1 != prev_bg_r || g1 != prev_bg_g || b1 != prev_bg_b) {
                                    append_bg(frame_str, r1, g1, b1);
                                    prev_bg_r = r1; prev_bg_g = g1; prev_bg_b = b1;
                                }
                                frame_str.push_back(' ');
                            } else {
                                if (r1 != prev_fg_r || g1 != prev_fg_g || b1 != prev_fg_b) {
                                    append_fg(frame_str, r1, g1, b1);
                                    prev_fg_r = r1; prev_fg_g = g1; prev_fg_b = b1;
                                }
                                if (r2 != prev_bg_r || g2 != prev_bg_g || b2 != prev_bg_b) {
                                    append_bg(frame_str, r2, g2, b2);
                                    prev_bg_r = r2; prev_bg_g = g2; prev_bg_b = b2;
                                }
                                // Unicode upper half block ▀
                                frame_str.append("\xE2\x96\x80");
                            }
                        }

                        if (prev_bg_r != -1) {
                            frame_str.append("\033[49m");
                            prev_bg_r = -1;
                        }
                    }
                } else if (current_mode == RenderMode::Solid) {
                    // Full block █ rendering
                    int prev_fg_r = -1, prev_fg_g = -1, prev_fg_b = -1;

                    for (int line = 0; line < render_h; ++line) {
                        append_cursor_pos(frame_str, 1 + pad_y + line, 1 + pad_x);

                        const uint8_t* row = data + line * stride;
                        for (int x = 0; x < render_w; ++x) {
                            int idx = x * 3;
                            int r = row[idx];
                            int g = row[idx + 1];
                            int b = row[idx + 2];

                            if (opts.quant > 1) {
                                r = (r / opts.quant) * opts.quant;
                                g = (g / opts.quant) * opts.quant;
                                b = (b / opts.quant) * opts.quant;
                            }

                            if (r != prev_fg_r || g != prev_fg_g || b != prev_fg_b) {
                                append_fg(frame_str, r, g, b);
                                prev_fg_r = r; prev_fg_g = g; prev_fg_b = b;
                            }
                            frame_str.append("\xE2\x96\x88");
                        }
                    }
                } else {
                    // Ascii glyph rendering (color or grayscale)
                    int prev_fg_r = -1, prev_fg_g = -1, prev_fg_b = -1;

                    for (int line = 0; line < render_h; ++line) {
                        append_cursor_pos(frame_str, 1 + pad_y + line, 1 + pad_x);

                        const uint8_t* row = data + line * stride;
                        for (int x = 0; x < render_w; ++x) {
                            int idx = x * 3;
                            int r = row[idx];
                            int g = row[idx + 1];
                            int b = row[idx + 2];

                            if (opts.quant > 1) {
                                r = (r / opts.quant) * opts.quant;
                                g = (g / opts.quant) * opts.quant;
                                b = (b / opts.quant) * opts.quant;
                            }

                            int lum = (r * 54 + g * 183 + b * 19) >> 8;
                            char ch = brightness_to_char(lum / 255.0f, ramp, opts.invert);

                            if (opts.color) {
                                if (r != prev_fg_r || g != prev_fg_g || b != prev_fg_b) {
                                    append_fg(frame_str, r, g, b);
                                    prev_fg_r = r; prev_fg_g = g; prev_fg_b = b;
                                }
                            }
                            frame_str.push_back(ch);
                        }
                    }
                }

                // Reset color and end synchronized update
                frame_str.append("\033[0m\033[?2026l");

                // Single atomic write to console device
                Terminal::write_raw(frame_str.data(), frame_str.size());

                frame_index++;

                // Ultra-smooth hybrid sleep (coarse sleep + fine spin-wait yield)
                now = std::chrono::steady_clock::now();
                if (now < target_time) {
                    auto sleep_dur = target_time - now;
                    if (sleep_dur > std::chrono::milliseconds(3)) {
                        std::this_thread::sleep_for(sleep_dur - std::chrono::milliseconds(2));
                    }
                    while (std::chrono::steady_clock::now() < target_time) {
                        std::this_thread::yield();
                    }
                }
            }
        }
        av_packet_unref(packet);
    }

    // Cleanup resources
    audio_player.stop();
    if (audio_thread.joinable()) {
        audio_thread.join();
    }

    if (rgb_frame && rgb_frame->data[0]) {
        av_freep(&rgb_frame->data[0]);
    }
    if (prev_rgb_buffer) av_free(prev_rgb_buffer);
    av_frame_free(&rgb_frame);
    av_frame_free(&frame);
    av_packet_free(&packet);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    // Restore terminal configuration
    Terminal::restore();

    return 0;
}

} // namespace ascii
