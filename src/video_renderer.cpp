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
#include <cmath>
#include <algorithm>
#include <sstream>
#include <cstdio>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif

namespace ascii {

// Audio subprocess
class AudioPlayer {
public:
    AudioPlayer(const std::string& path, int delay_ms, bool no_audio)
        : path_(path), delay_ms_(delay_ms), no_audio_(no_audio) {
#ifdef _WIN32
        process_handle_ = nullptr;
#else
        pid_ = -1;
#endif
    }

    ~AudioPlayer() { stop(); }

    void start() {
        if (no_audio_) return;
        if (delay_ms_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
        }

#ifdef _WIN32
        std::string cmd = "ffplay -nodisp -autoexit -loglevel quiet";
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

    void stop() {
#ifdef _WIN32
        if (process_handle_) {
            TerminateProcess(process_handle_, 0);
            CloseHandle(process_handle_);
            process_handle_ = nullptr;
        }
#else
        if (pid_ > 0) {
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
#ifdef _WIN32
    HANDLE process_handle_;
#else
    pid_t pid_;
#endif
};

int VideoRenderer::play(const VideoOptions& opts) {
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgb_frame = nullptr;
    AVPacket* packet = nullptr;
    uint8_t* rgb_buffer = nullptr;

    int video_stream_idx = -1;
    int ret = 0;

    // Suppress FFmpeg error messages that pollute terminal output
    av_log_set_level(AV_LOG_QUIET);

    ret = avformat_open_input(&format_ctx, opts.path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        std::cerr << "Error: Could not open '" << opts.path << "'\n";
        return 1;
    }

    ret = avformat_find_stream_info(format_ctx, nullptr);
    if (ret < 0) {
        std::cerr << "Error: Could not find stream info\n";
        avformat_close_input(&format_ctx);
        return 1;
    }

    for (unsigned i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }

    if (video_stream_idx == -1) {
        std::cerr << "Error: No video stream\n";
        avformat_close_input(&format_ctx);
        return 1;
    }

    AVStream* video_stream = format_ctx->streams[video_stream_idx];
    AVCodecParameters* codec_params = video_stream->codecpar;

    const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        std::cerr << "Error: Codec not supported\n";
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

    // Multi-threaded decoding
    codec_ctx->thread_count = 0;

    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        std::cerr << "Error: Failed to open codec\n";
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return 1;
    }

    // Dimensions - get terminal size BEFORE any output
    int term_w = 80, term_h = 24;
    bool got_size = Terminal::get_size(term_w, term_h);

    // Debug: If terminal size detection failed, try harder
    if (!got_size || term_w < 40 || term_h < 10) {
        // Fallback: assume reasonably large terminal
        term_w = 120;
        term_h = 30;
    }

    int target_width = opts.width;
    int target_height = opts.height;

    // Fullscreen mode: use entire terminal BUT cap for performance
    if (opts.fullscreen) {
        // Cap fullscreen at 120 columns for smooth playback
        // Terminal I/O is the bottleneck, not CPU
        target_width = std::min(term_w, 120);
        target_height = std::min(term_h - 1, 35);
    } else {
        if (target_width <= 0) target_width = std::min(term_w, 100);
        if (target_height <= 0) {
            float aspect = static_cast<float>(codec_ctx->height) / codec_ctx->width;
            target_height = static_cast<int>(target_width * aspect * 0.5f);
            target_height = std::min(target_height, term_h - 1);
        }
    }

    target_width = std::max(10, target_width);
    target_height = std::max(5, target_height);

    // Scaling - use POINT (nearest neighbor) for speed in fullscreen mode
    int sws_flags = (target_width > 100) ? SWS_POINT : SWS_BILINEAR;

    sws_ctx = sws_getContext(
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        target_width, target_height, AV_PIX_FMT_RGB24,
        sws_flags, nullptr, nullptr, nullptr
    );

    if (!sws_ctx) {
        std::cerr << "Error: Failed to create scaler\n";
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return 1;
    }

    frame = av_frame_alloc();
    rgb_frame = av_frame_alloc();
    packet = av_packet_alloc();

    if (!frame || !rgb_frame || !packet) {
        std::cerr << "Error: Failed to allocate frames\n";
        if (frame) av_frame_free(&frame);
        if (rgb_frame) av_frame_free(&rgb_frame);
        if (packet) av_packet_free(&packet);
        sws_freeContext(sws_ctx);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return 1;
    }

    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, target_width, target_height, 1);
    rgb_buffer = static_cast<uint8_t*>(av_malloc(buffer_size));
    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buffer,
                        AV_PIX_FMT_RGB24, target_width, target_height, 1);

    // FPS
    double source_fps = av_q2d(video_stream->avg_frame_rate);
    if (source_fps <= 0.0 || source_fps > 120.0) {
        source_fps = av_q2d(video_stream->r_frame_rate);
    }
    if (source_fps <= 0.0 || source_fps > 120.0) {
        source_fps = 30.0;
    }

    double target_fps = opts.fps > 0 ? opts.fps : source_fps;

    // Smart FPS adjustment based on resolution
    // Terminal I/O is the bottleneck, not CPU power
    if (opts.fps == 0) {  // Only auto-adjust if user didn't specify FPS
        int pixel_count = target_width * target_height;
        if (pixel_count > 3500) {
            target_fps = 20;  // Large render: cap at 20 FPS
        } else if (pixel_count > 2500) {
            target_fps = 24;  // Medium render: 24 FPS
        } else {
            target_fps = std::min(30.0, target_fps);  // Small render: up to 30 FPS
        }
    }

    auto frame_duration = std::chrono::duration<double>(1.0 / target_fps);

    const Charset* cs = get_charset(opts.charset);
    const std::string& ramp = cs->ramp;

    // Audio - start immediately without delay to sync with video
    std::thread audio_thread;
    AudioPlayer audio_player(opts.path, opts.audio_delay, opts.no_audio);

    // Terminal setup with double buffering
    Terminal::setup_signal_handlers();
    Terminal::hide_cursor();

    // Enable synchronized output mode to prevent tearing (DEC private mode 2026)
    std::cout << "\033[?2026h" << std::flush;

    // Use alternate screen buffer for clean rendering
    std::cout << "\033[?1049h" << std::flush;

    Terminal::clear();

    // Disable line wrapping to prevent unwanted line breaks
    std::cout << "\033[?7l" << std::flush;

    // Start audio AFTER terminal setup but BEFORE starting frame timer
    if (!opts.no_audio) {
        audio_thread = std::thread([&audio_player]() { audio_player.start(); });
        // Give audio a tiny head start to account for audio buffer latency
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    auto playback_start = std::chrono::steady_clock::now();
    int frame_index = 0;
    int consecutive_skipped = 0;

    // Main decode loop
    while (av_read_frame(format_ctx, packet) >= 0) {
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

                // Scale
                sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height,
                         rgb_frame->data, rgb_frame->linesize);

                // Timing
                auto target_time = playback_start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    frame_index * frame_duration
                );
                auto now = std::chrono::steady_clock::now();

                if (now > target_time + frame_duration && consecutive_skipped < opts.max_frame_skip) {
                    consecutive_skipped++;
                    frame_index++;
                    continue;
                }

                consecutive_skipped = 0;

                // Render
                std::ostringstream oss;

                // Use synchronized update begin/end markers to prevent tearing
                oss << "\033[?2026h";  // Begin synchronized update
                oss << "\033[H";        // Home cursor (top-left) - CRITICAL: prevents vertical drift

                const uint8_t* data = rgb_frame->data[0];

                if (opts.color) {
                    // Ultra-optimized color rendering for fullscreen
                    int prev_r = -1, prev_g = -1, prev_b = -1;

                    for (int y = 0; y < target_height; y++) {
                        const uint8_t* row_data = data + (y * target_width) * 3;

                        for (int x = 0; x < target_width; x++) {
                            int idx = x * 3;
                            int r = row_data[idx];
                            int g = row_data[idx + 1];
                            int b = row_data[idx + 2];

                            // Quantization for speed
                            if (opts.quant > 0) {
                                r = (r / opts.quant) * opts.quant;
                                g = (g / opts.quant) * opts.quant;
                                b = (b / opts.quant) * opts.quant;
                            }

                            // Faster color boost (simpler math)
                            r = std::min(255, (r * 11) / 10);  // 10% brightness boost
                            g = std::min(255, (g * 11) / 10);
                            b = std::min(255, (b * 11) / 10);

                            // Fast luminance
                            int lum = (r + r + g + g + g + b) >> 3; // Approximation of 0.2126*r + 0.7152*g + 0.0722*b

                            // Simplified character selection for speed
                            char ch;
                            if (lum < 26) {
                                ch = ' ';
                            } else if (lum < 77) {
                                ch = '.';
                            } else if (lum < 128) {
                                ch = ':';
                            } else if (lum < 179) {
                                ch = '=';
                            } else if (lum < 230) {
                                ch = '#';
                            } else {
                                ch = '@';
                            }

                            // Only emit color change
                            if (r != prev_r || g != prev_g || b != prev_b) {
                                oss << "\033[38;2;" << r << ";" << g << ";" << b << "m";
                                prev_r = r; prev_g = g; prev_b = b;
                            }

                            oss << ch;
                        }
                        oss << "\033[0m\n";
                        prev_r = prev_g = prev_b = -1;
                    }
                } else {
                    // Grayscale
                    for (int y = 0; y < target_height; y++) {
                        for (int x = 0; x < target_width; x++) {
                            int idx = (y * target_width + x) * 3;
                            float r = data[idx] / 255.0f;
                            float g = data[idx + 1] / 255.0f;
                            float b = data[idx + 2] / 255.0f;
                            float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                            oss << brightness_to_char(lum, ramp, opts.invert);
                        }
                        oss << '\n';
                    }
                }

                // End synchronized update - this ensures the entire frame is drawn atomically
                oss << "\033[?2026l";

                // Write entire frame in ONE syscall for perfectly smooth rendering
                std::string frame_str = oss.str();
                std::fwrite(frame_str.data(), 1, frame_str.size(), stdout);
                std::fflush(stdout);

                frame_index++;

                // Sleep
                now = std::chrono::steady_clock::now();
                if (now < target_time) {
                    std::this_thread::sleep_for(target_time - now);
                }
            }
        }
        av_packet_unref(packet);
    }

    // Cleanup
    audio_player.stop();
    if (audio_thread.joinable()) {
        audio_thread.join();
    }

    if (rgb_buffer) av_free(rgb_buffer);
    av_frame_free(&rgb_frame);
    av_frame_free(&frame);
    av_packet_free(&packet);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    // Restore terminal state
    std::cout << "\033[?2026l";  // Disable synchronized output
    std::cout << "\033[?7h";     // Re-enable line wrapping
    std::cout << "\033[?1049l";  // Exit alternate screen
    Terminal::show_cursor();
    std::cout << "\033[0m" << std::endl;

    return 0;
}

} // namespace ascii
