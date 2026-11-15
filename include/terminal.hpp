#pragma once

#include <string>
#include <cstddef>

namespace ascii {

// Terminal utilities
class Terminal {
public:
    // Initialize Windows Console / Terminal (VT processing, UTF-8 CP, timer precision)
    static void initialize();
    static void restore();

    // Maximize console window on Windows
    static void maximize_window();

    // Fast atomic write to terminal
    static void write_raw(const char* data, size_t size);

    // Get terminal dimensions
    static bool get_size(int& width, int& height);

    // Clear screen
    static void clear();

    // Move cursor to top-left
    static void reset_cursor();

    // Hide/show cursor
    static void hide_cursor();
    static void show_cursor();

    // Setup signal handlers for clean exit (Ctrl+C)
    static void setup_signal_handlers();

    // Check if terminal supports 24-bit color
    static bool supports_truecolor();

    // ANSI color escape code
    static std::string rgb_color(int r, int g, int b);
    static std::string reset_color();

    // Fast rendering optimizations
    static void enable_alternate_buffer();
    static void disable_alternate_buffer();
    static void set_synchronized_output(bool enabled);
};

} // namespace ascii
