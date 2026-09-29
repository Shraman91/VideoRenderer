#pragma once

#include <string>
#include <memory>

namespace ascii {

struct ImageData {
    int width;
    int height;
    int channels;
    std::unique_ptr<unsigned char[]> data;
};

class ImageRenderer {
public:
    // Load image from file
    static std::unique_ptr<ImageData> load_image(const std::string& path);

    // Render image to ASCII
    static std::string render(
        const ImageData& img,
        int target_width = 0,   // 0 = auto (terminal width)
        int target_height = 0,  // 0 = auto (from aspect ratio)
        const std::string& charset_name = "classic",
        bool invert = false
    );

private:
    static float pixel_brightness(const unsigned char* pixel, int channels);
};

} // namespace ascii
