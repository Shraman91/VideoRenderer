#include "terminal.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

namespace ascii {

static volatile bool should_exit = false;

static void signal_handler(int signal) {
    if (signal == SIGINT) {
        Terminal::show_cursor();
        std::cout << Terminal::reset_color() << std::endl;
        should_exit = true;
        std::exit(0);
    }
}

bool Terminal::get_size(int& width, int& height) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return true;
    }
    // Fallback for MSYS2/mintty - try TIOCGWINSZ
    #ifdef TIOCGWINSZ
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
        width = ws.ws_col;
        height = ws.ws_row;
        return true;
    }
    #endif
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
        width = ws.ws_col;
        height = ws.ws_row;
        return true;
    }
#endif
    // Default fallback
    width = 80;
    height = 24;
    return false;
}

void Terminal::clear() {
    std::cout << "\033[2J";
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
}

bool Terminal::supports_truecolor() {
    // Check COLORTERM environment variable
    const char* colorterm = std::getenv("COLORTERM");
    if (colorterm && (std::string(colorterm).find("truecolor") != std::string::npos ||
                      std::string(colorterm).find("24bit") != std::string::npos)) {
        return true;
    }

    // MSYS2/mintty supports truecolor
    const char* term = std::getenv("TERM");
    if (term && std::string(term).find("xterm") != std::string::npos) {
        return true;
    }

    return false;
}

std::string Terminal::rgb_color(int r, int g, int b) {
    return "\033[38;2;" + std::to_string(r) + ";" +
           std::to_string(g) + ";" + std::to_string(b) + "m";
}

std::string Terminal::reset_color() {
    return "\033[0m";
}

void Terminal::enable_alternate_buffer() {
    std::cout << "\033[?1049h";
}

void Terminal::disable_alternate_buffer() {
    std::cout << "\033[?1049l";
}

void Terminal::set_synchronized_output(bool enabled) {
    if (enabled) {
        std::cout << "\033[?2026h";
    } else {
        std::cout << "\033[?2026l";
    }
}

} // namespace ascii
