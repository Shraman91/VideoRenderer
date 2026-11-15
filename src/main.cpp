#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <thread>
#include <chrono>

#include "image_renderer.hpp"
#include "demo_renderer.hpp"
#include "terminal.hpp"
#include "charset.hpp"

#ifdef HAVE_FFMPEG
#include "video_renderer.hpp"
#endif

void print_usage() {
    std::cout << "ASCII Art Renderer - Convert images, videos, and 3D demos to ASCII\n\n"
              << "Usage:\n"
              << "  ascii-art list\n"
              << "  ascii-art image <path> [options]\n"
              << "  ascii-art video <path> [options]\n"
              << "  ascii-art demo <name> [options]\n\n"
              << "Commands:\n"
              << "  list                    List available procedural demos\n"
              << "  image <path>            Render an image as ASCII art\n"
              << "  video <path>            Play a video as ASCII animation\n"
              << "  demo <name>             Run a procedural 3D demo\n\n"
              << "Image options:\n"
              << "  --width N               Output width in characters\n"
              << "  --height N              Output height in characters\n"
              << "  --charset NAME          Character set: classic, detailed, letters\n"
              << "  --invert                Invert brightness mapping\n"
              << "  -o, --output PATH       Write to file instead of stdout\n\n"
              << "Video options:\n"
              << "  --color                 Enable 24-bit color output\n"
              << "  --fullscreen            Use full terminal dimensions\n"
              << "  --blocks                Use half-block mode (photorealistic, best quality)\n"
              << "  --mode MODE             Render mode: blocks|ascii|solid (default: blocks)\n"
              << "  --fps N                 Target framerate\n"
              << "  --width N               Output width in characters\n"
              << "  --height N              Output height in characters\n"
              << "  --smoothing N           Temporal smoothing (0-1)\n"
              << "  --quant N               Color quantization level\n"
              << "  --max-frame-skip N      Max consecutive dropped frames\n"
              << "  --no-audio              Disable audio playback\n"
              << "  --audio-delay N         Audio sync offset (ms)\n"
              << "  --charset NAME          Character set (for ascii mode)\n"
              << "  --invert                Invert brightness\n\n"
              << "Demo options:\n"
              << "  --width N               Output width in characters\n"
              << "  --height N              Output height in characters\n"
              << "  --charset NAME          Character set\n"
              << "  --fps N                 Target framerate (default: 30)\n\n"
              << "Examples:\n"
              << "  ascii-art image photo.jpg --width 100\n"
              << "  ascii-art video movie.mp4 --color --fullscreen --fps 30\n"
              << "  ascii-art video movie.mp4 --blocks --fps 30 --width 120\n"
              << "  ascii-art video clip.mp4 --color --mode ascii --charset detailed\n"
              << "  ascii-art demo cube --charset detailed\n";
}

struct ImageOptions {
    std::string path;
    int width = 0;
    int height = 0;
    std::string charset = "classic";
    bool invert = false;
    std::string output;
};

struct DemoOptions {
    std::string name;
    int width = 0;
    int height = 0;
    std::string charset = "classic";
    int fps = 30;
};

#ifdef HAVE_FFMPEG
bool parse_video_args(int argc, char** argv, ascii::VideoOptions& opts) {
    if (argc < 3) return false;
    opts.path = argv[2];
    bool mode_set = false;
    bool charset_set = false;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--color") {
            opts.color = true;
        } else if (arg == "--fullscreen" || arg == "-f") {
            opts.fullscreen = true;
        } else if (arg == "--blocks") {
            opts.color = true;
            opts.mode = ascii::RenderMode::Blocks;
            mode_set = true;
        } else if (arg == "--mode" && i + 1 < argc) {
            std::string mode_str = argv[++i];
            mode_set = true;
            if (mode_str == "blocks") {
                opts.mode = ascii::RenderMode::Blocks;
                opts.color = true;
            } else if (mode_str == "ascii") {
                opts.mode = ascii::RenderMode::Ascii;
            } else if (mode_str == "solid") {
                opts.mode = ascii::RenderMode::Solid;
                opts.color = true;
            }
        } else if (arg == "--fps" && i + 1 < argc) {
            opts.fps = std::atoi(argv[++i]);
        } else if (arg == "--width" && i + 1 < argc) {
            opts.width = std::atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            opts.height = std::atoi(argv[++i]);
        } else if (arg == "--smoothing" && i + 1 < argc) {
            opts.smoothing = std::atof(argv[++i]);
        } else if (arg == "--quant" && i + 1 < argc) {
            opts.quant = std::atoi(argv[++i]);
        } else if (arg == "--max-frame-skip" && i + 1 < argc) {
            opts.max_frame_skip = std::atoi(argv[++i]);
        } else if (arg == "--no-audio") {
            opts.no_audio = true;
        } else if (arg == "--audio-delay" && i + 1 < argc) {
            opts.audio_delay = std::atoi(argv[++i]);
        } else if (arg == "--charset" && i + 1 < argc) {
            opts.charset = argv[++i];
            charset_set = true;
        } else if (arg == "--invert") {
            opts.invert = true;
        }
    }

    if (charset_set && !mode_set) {
        opts.mode = ascii::RenderMode::Ascii;
    }
    if (opts.mode == ascii::RenderMode::Blocks || opts.mode == ascii::RenderMode::Solid) {
        opts.color = true;
    }
    return true;
}
#endif

bool parse_image_args(int argc, char** argv, ImageOptions& opts) {
    if (argc < 3) return false;
    opts.path = argv[2];

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--width" && i + 1 < argc) {
            opts.width = std::atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            opts.height = std::atoi(argv[++i]);
        } else if (arg == "--charset" && i + 1 < argc) {
            opts.charset = argv[++i];
        } else if (arg == "--invert") {
            opts.invert = true;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            opts.output = argv[++i];
        }
    }
    return true;
}

bool parse_demo_args(int argc, char** argv, DemoOptions& opts) {
    if (argc < 3) return false;
    opts.name = argv[2];

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--width" && i + 1 < argc) {
            opts.width = std::atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            opts.height = std::atoi(argv[++i]);
        } else if (arg == "--charset" && i + 1 < argc) {
            opts.charset = argv[++i];
        } else if (arg == "--fps" && i + 1 < argc) {
            opts.fps = std::atoi(argv[++i]);
        }
    }
    return true;
}

int cmd_image(int argc, char** argv) {
    ImageOptions opts;
    if (!parse_image_args(argc, argv, opts)) {
        std::cerr << "Error: image command requires a file path\n"
                  << "Usage: ascii-art image <path> [options]\n";
        return 1;
    }

    auto img = ascii::ImageRenderer::load_image(opts.path);
    if (!img) {
        return 1;
    }

    std::string result = ascii::ImageRenderer::render(
        *img, opts.width, opts.height, opts.charset, opts.invert
    );

    if (opts.output.empty()) {
        std::cout << result;
    } else {
        std::ofstream out(opts.output);
        if (!out) {
            std::cerr << "Error: Failed to write to '" << opts.output << "'\n";
            return 1;
        }
        out << result;
        std::cout << "Saved ASCII art to '" << opts.output << "'\n";
    }

    return 0;
}

int cmd_demo(int argc, char** argv) {
    DemoOptions opts;
    if (!parse_demo_args(argc, argv, opts)) {
        std::cerr << "Error: demo command requires a demo name\n"
                  << "Usage: ascii-art demo <name> [options]\n"
                  << "Run 'ascii-art list' to see available demos\n";
        return 1;
    }

    auto demo = ascii::DemoRegistry::instance().create(opts.name);
    if (!demo) {
        std::cerr << "Error: Unknown demo '" << opts.name << "'\n"
                  << "Run 'ascii-art list' to see available demos\n";
        return 1;
    }

    // Determine dimensions
    int width = opts.width, height = opts.height;
    if (width <= 0 || height <= 0) {
        ascii::Terminal::get_size(width, height);
        if (opts.width > 0) width = opts.width;
        if (opts.height > 0) height = opts.height;
        height = std::max(5, height - 1); // Leave room for status
    }

    const ascii::Charset* cs = ascii::get_charset(opts.charset);
    const std::string& ramp = cs->ramp;

    // Setup terminal
    ascii::Terminal::setup_signal_handlers();
    ascii::Terminal::hide_cursor();
    ascii::Terminal::clear();

    int frame = 0;
    const auto frame_duration = std::chrono::milliseconds(1000 / opts.fps);

    while (true) {
        auto frame_start = std::chrono::steady_clock::now();

        std::string output = demo->render_frame(frame, width, height, ramp);

        ascii::Terminal::reset_cursor();
        std::cout << output << std::flush;

        ++frame;

        // Frame timing
        auto frame_end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);
        if (elapsed < frame_duration) {
            std::this_thread::sleep_for(frame_duration - elapsed);
        }
    }

    ascii::Terminal::show_cursor();
    return 0;
}

int cmd_list() {
    ascii::DemoRegistry::instance().list_demos();
    return 0;
}

int cmd_video(int argc, char** argv) {
#ifdef HAVE_FFMPEG
    ascii::VideoOptions opts;
    if (!parse_video_args(argc, argv, opts)) {
        std::cerr << "Error: video command requires a file path\n"
                  << "Usage: ascii-art video <path> [options]\n";
        return 1;
    }

    return ascii::VideoRenderer::play(opts);
#else
    std::cerr << "Error: Video support not compiled in\n"
              << "Rebuild with FFmpeg: pacman -S mingw-w64-x86_64-ffmpeg\n";
    return 1;
#endif
}

int main(int argc, char** argv) {
    ascii::Terminal::initialize();

    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "list") {
        return cmd_list();
    } else if (command == "image") {
        return cmd_image(argc, argv);
    } else if (command == "video") {
        return cmd_video(argc, argv);
    } else if (command == "demo") {
        return cmd_demo(argc, argv);
    } else if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    } else {
        std::cerr << "Error: Unknown command '" << command << "'\n";
        print_usage();
        return 1;
    }
}
