#pragma once

#include <string>
#include <vector>

namespace ascii {

// Character ramps for different visual styles
struct Charset {
    std::string ramp;
    std::string name;
};

// Predefined character sets
extern const Charset CHARSET_CLASSIC;
extern const Charset CHARSET_DETAILED;
extern const Charset CHARSET_LETTERS;

// Get charset by name
const Charset* get_charset(const std::string& name);

// Convert brightness (0.0-1.0) to ASCII character
char brightness_to_char(float brightness, const std::string& ramp, bool invert = false);

} // namespace ascii
