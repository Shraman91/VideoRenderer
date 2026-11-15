#include "terminal.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <thread>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
    #include <timeapi.h>
    #include <io.h>
#else
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

namespace ascii {

static volatile bool should_exit = false;

#ifdef _WIN32
static DWORD orig_out_mode = 0;
static DWORD orig_in_mode = 0;
static UINT orig_cp = 0;
static UINT orig_output_cp = 0;
static bool console_initialized = false;
static bool has_orig_out_mode = false;
static bool has_orig_in_mode = false;
#endif

static void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        Terminal::restore();
        should_exit = true;
        std::exit(0);
    }
}

void Terminal::initialize() {
#ifdef _WIN32
    if (console_initialized) return;

    // Set Windows multimedia timer resolution to 1ms for ultra-smooth framerate
    timeBeginPeriod(1);

    // Save and set UTF-8 code page so unicode blocks (▀, █) render properly
    orig_cp = GetConsoleCP();
    orig_output_cp = GetConsoleOutputCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // Enable Virtual Terminal Processing for ANSI escape codes in Windows Console/Terminal
    HANDLE hOut = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &orig_out_mode)) {
        has_orig_out_mode = true;
        DWORD out_mode = orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
        SetConsoleMode(hOut, out_mode);
    }
    if (hOut != INVALID_HANDLE_VALUE && hOut != GetStdHandle(STD_OUTPUT_HANDLE)) {
        CloseHandle(hOut);
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE && GetConsoleMode(hIn, &orig_in_mode)) {
        has_orig_in_mode = true;
        DWORD in_mode = orig_in_mode | ENABLE_VIRTUAL_TERMINAL_INPUT;
        SetConsoleMode(hIn, in_mode);
    }

    setup_signal_handlers();
    std::atexit(Terminal::restore);
    console_initialized = true;
#else
    setup_signal_handlers();
#endif
}

void Terminal::restore() {
#ifdef _WIN32
    if (!console_initialized) return;

    show_cursor();
    std::cout << reset_color();
    disable_alternate_buffer();
    set_synchronized_output(false);
    std::cout << "\033[?7h" << std::flush; // Re-enable line wrapping

    HANDLE hOut = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    if (hOut != INVALID_HANDLE_VALUE && has_orig_out_mode) {
        SetConsoleMode(hOut, orig_out_mode);
    }
    if (hOut != INVALID_HANDLE_VALUE && hOut != GetStdHandle(STD_OUTPUT_HANDLE)) {
        CloseHandle(hOut);
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE && has_orig_in_mode) {
        SetConsoleMode(hIn, orig_in_mode);
    }

    if (orig_output_cp != 0) {
        SetConsoleOutputCP(orig_output_cp);
    }
    if (orig_cp != 0) {
        SetConsoleCP(orig_cp);
    }

    timeEndPeriod(1);
    console_initialized = false;
#else
    show_cursor();
    std::cout << reset_color();
    disable_alternate_buffer();
    set_synchronized_output(false);
    std::cout << "\033[?7h" << std::flush;
#endif
}

void Terminal::maximize_window() {
#ifdef _WIN32
    // Send standard VT sequence to maximize window (supported by Windows Terminal, XTerm, Mintty)
    std::cout << "\033[9;1t" << std::flush;

    // Only call Win32 ShowWindow on legacy conhost when NOT running under Windows Terminal (ConPTY)
    // Calling ShowWindow on ConPTY's fake window causes OpenConsole to crash!
    if (!std::getenv("WT_SESSION")) {
        HWND hwnd = GetConsoleWindow();
        if (hwnd) {
            ShowWindow(hwnd, SW_MAXIMIZE);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
#else
    std::cout << "\033[9;1t" << std::flush;
#endif
}

void Terminal::write_raw(const char* data, size_t size) {
#ifdef _WIN32
    static HANDLE hOut = INVALID_HANDLE_VALUE;
    if (hOut == INVALID_HANDLE_VALUE) {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    if (hOut != INVALID_HANDLE_VALUE) {
        const size_t CHUNK_SIZE = 32768; // 32 KB chunk size safe for all Windows console drivers
        size_t offset = 0;
        while (offset < size) {
            DWORD to_write = static_cast<DWORD>(std::min(size - offset, CHUNK_SIZE));
            DWORD written = 0;
            if (!WriteFile(hOut, data + offset, to_write, &written, nullptr) || written == 0) {
                break;
            }
            offset += written;
        }
        return;
    }
#endif
    std::fwrite(data, 1, size, stdout);
    std::fflush(stdout);
}

bool Terminal::get_size(int& width, int& height) {
#ifdef _WIN32
    HANDLE hOut = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    bool need_close = true;
    if (hOut == INVALID_HANDLE_VALUE) {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        need_close = false;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

        // In Windows Terminal / ConPTY, check dwSize if srWindow is abnormal
        if (height <= 2 && csbi.dwSize.Y > 2) {
            height = csbi.dwSize.Y;
        }
        if (width <= 2 && csbi.dwSize.X > 2) {
            width = csbi.dwSize.X;
        }

        if (need_close) {
            CloseHandle(hOut);
        }
        if (width > 0 && height > 0) {
            return true;
        }
    } else {
        if (need_close && hOut != INVALID_HANDLE_VALUE) {
            CloseHandle(hOut);
        }
    }

    #ifdef TIOCGWINSZ
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col > 0 && ws.ws_row > 0) {
        width = ws.ws_col;
        height = ws.ws_row;
        return true;
    }
    #endif
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col > 0 && ws.ws_row > 0) {
        width = ws.ws_col;
        height = ws.ws_row;
        return true;
    }
#endif

    // Check environment variables as fallback
    const char* env_cols = std::getenv("COLUMNS");
    const char* env_lines = std::getenv("LINES");
    if (env_cols && env_lines) {
        width = std::atoi(env_cols);
        height = std::atoi(env_lines);
        if (width > 0 && height > 0) return true;
    }

    // Default fallback
    width = 80;
    height = 24;
    return false;
}

void Terminal::clear() {
    std::cout << "\033[2J\033[3J\033[H" << std::flush;
}

void Terminal::reset_cursor() {
    std::cout << "\033[H";
}

void Terminal::hide_cursor() {
    std::cout << "\033[?25l";
}

void Terminal::show_cursor() {
    std::cout << "\033[?25h";
}

void Terminal::setup_signal_handlers() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
}

bool Terminal::supports_truecolor() {
#ifdef _WIN32
    // Windows 10/11 Windows Terminal, PowerShell, and Command Prompt support 24-bit TrueColor
    return true;
#else
    const char* colorterm = std::getenv("COLORTERM");
    if (colorterm && (std::string(colorterm).find("truecolor") != std::string::npos ||
                      std::string(colorterm).find("24bit") != std::string::npos)) {
        return true;
    }

    const char* term = std::getenv("TERM");
    if (term && std::string(term).find("xterm") != std::string::npos) {
        return true;
    }

    return false;
#endif
}

std::string Terminal::rgb_color(int r, int g, int b) {
    return "\033[38;2;" + std::to_string(r) + ";" +
           std::to_string(g) + ";" + std::to_string(b) + "m";
}

std::string Terminal::reset_color() {
    return "\033[0m";
}

void Terminal::enable_alternate_buffer() {
    std::cout << "\033[?1049h" << std::flush;
}

void Terminal::disable_alternate_buffer() {
    std::cout << "\033[?1049l" << std::flush;
}

void Terminal::set_synchronized_output(bool enabled) {
    if (enabled) {
        std::cout << "\033[?2026h" << std::flush;
    } else {
        std::cout << "\033[?2026l" << std::flush;
    }
}

} // namespace ascii
