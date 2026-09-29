#include "charset.hpp"
#include <map>

namespace ascii {

// Classic ASCII ramp (10 levels)
const Charset CHARSET_CLASSIC = {
    " .:-=+*#%@",
    "classic"
};

// Detailed ASCII ramp with more gradations
const Charset CHARSET_DETAILED = {
    " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$",
    "detailed"
};

// Letters only (creative mode)
const Charset CHARSET_LETTERS = {
    " .oO0@",
    "letters"
};

static const std::map<std::string, const Charset*> charset_map = {
    {"classic", &CHARSET_CLASSIC},
    {"detailed", &CHARSET_DETAILED},
    {"letters", &CHARSET_LETTERS}
};

const Charset* get_charset(const std::string& name) {
    auto it = charset_map.find(name);
    if (it != charset_map.end()) {
        return it->second;
    }
    return &CHARSET_CLASSIC; // default
}

char brightness_to_char(float brightness, const std::string& ramp, bool invert) {
    if (invert) {
        brightness = 1.0f - brightness;
    }

    // Clamp brightness to [0, 1]
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;

    // Map to ramp index
    int idx = static_cast<int>(brightness * (ramp.size() - 1) + 0.5f);
    if (idx < 0) idx = 0;
    if (idx >= static_cast<int>(ramp.size())) idx = ramp.size() - 1;

    return ramp[idx];
}

} // namespace ascii
