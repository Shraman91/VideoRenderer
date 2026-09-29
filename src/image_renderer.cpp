#include "image_renderer.hpp"
#include "charset.hpp"
#include "terminal.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <cmath>
#include <sstream>
#include <algorithm>

namespace ascii {

std::unique_ptr<ImageData> ImageRenderer::load_image(const std::string& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "Error: Failed to load image '" << path << "': "
                  << stbi_failure_reason() << std::endl;
        return nullptr;
    }

    auto img = std::make_unique<ImageData>();
    img->width = width;
    img->height = height;
    img->channels = channels;
    img->data = std::unique_ptr<unsigned char[]>(data);

    return img;
}

float ImageRenderer::pixel_brightness(const unsigned char* pixel, int channels) {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    if (channels == 1) {
        // Grayscale
        r = g = b = pixel[0] / 255.0f;
    } else if (channels == 2) {
        // Grayscale + Alpha
        r = g = b = pixel[0] / 255.0f;
        a = pixel[1] / 255.0f;
    } else if (channels == 3) {
        // RGB
        r = pixel[0] / 255.0f;
        g = pixel[1] / 255.0f;
        b = pixel[2] / 255.0f;
    } else if (channels >= 4) {
        // RGBA
        r = pixel[0] / 255.0f;
        g = pixel[1] / 255.0f;
        b = pixel[2] / 255.0f;
        a = pixel[3] / 255.0f;
    }

    // Perceptual luminance formula (ITU-R BT.709)
    float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    return luminance * a;
}

std::string ImageRenderer::render(
    const ImageData& img,
    int target_width,
    int target_height,
    const std::string& charset_name,
    bool invert
) {
    const Charset* cs = get_charset(charset_name);
    const std::string& ramp = cs->ramp;

    // Terminal aspect ratio correction: terminal font cells are ~2x taller than wide
    const float char_aspect_ratio = 0.5f;

    // Determine target dimensions
    if (target_width <= 0 && target_height <= 0) {
        int term_w = 80, term_h = 24;
        Terminal::get_size(term_w, term_h);

        // Use full terminal width by default (or fit within height if constrained)
        int max_w = std::max(10, term_w);
        int max_h = std::max(5, term_h - 2);

        float img_ratio = static_cast<float>(img.height) / static_cast<float>(img.width);
        target_width = max_w;
        target_height = static_cast<int>(target_width * img_ratio * char_aspect_ratio + 0.5f);

        if (target_height > max_h) {
            target_height = max_h;
            target_width = static_cast<int>(target_height / (img_ratio * char_aspect_ratio) + 0.5f);
        }
    } else if (target_width > 0 && target_height <= 0) {
        float img_ratio = static_cast<float>(img.height) / static_cast<float>(img.width);
        target_height = static_cast<int>(target_width * img_ratio * char_aspect_ratio + 0.5f);
    } else if (target_height > 0 && target_width <= 0) {
        float img_ratio = static_cast<float>(img.height) / static_cast<float>(img.width);
        target_width = static_cast<int>(target_height / (img_ratio * char_aspect_ratio) + 0.5f);
    }

    target_width = std::max(1, target_width);
    target_height = std::max(1, target_height);

    std::ostringstream oss;

    // Sample pixels using area-averaging (box filter) for higher visual quality
    for (int y = 0; y < target_height; ++y) {
        float src_y0 = (static_cast<float>(y) / target_height) * img.height;
        float src_y1 = (static_cast<float>(y + 1) / target_height) * img.height;

        int iy0 = std::clamp(static_cast<int>(src_y0), 0, img.height - 1);
        int iy1 = std::clamp(static_cast<int>(std::ceil(src_y1)), iy0 + 1, img.height);

        for (int x = 0; x < target_width; ++x) {
            float src_x0 = (static_cast<float>(x) / target_width) * img.width;
            float src_x1 = (static_cast<float>(x + 1) / target_width) * img.width;

            int ix0 = std::clamp(static_cast<int>(src_x0), 0, img.width - 1);
            int ix1 = std::clamp(static_cast<int>(std::ceil(src_x1)), ix0 + 1, img.width);

            float total_brightness = 0.0f;
            int sample_count = 0;

            for (int sy = iy0; sy < iy1; ++sy) {
                for (int sx = ix0; sx < ix1; ++sx) {
                    const unsigned char* pixel = &img.data[(sy * img.width + sx) * img.channels];
                    total_brightness += pixel_brightness(pixel, img.channels);
                    sample_count++;
                }
            }

            float avg_brightness = (sample_count > 0) ? (total_brightness / sample_count) : 0.0f;
            char ch = brightness_to_char(avg_brightness, ramp, invert);
            oss << ch;
        }
        oss << '\n';
    }

    return oss.str();
}

} // namespace ascii
