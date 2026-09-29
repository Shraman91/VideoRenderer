#pragma once

#include <string>

namespace ascii {

enum class RenderMode {
    Blocks,  // Half-block high-definition 24-bit truecolor (photorealistic)
    Ascii,   // Colored ASCII character glyphs
    Solid    // Full block █ glyphs
};

struct VideoOptions {
    std::string path;
    bool color = false;
    RenderMode mode = RenderMode::Blocks;
    int fps = 0;              // 0 = detect from source or default 30
    int width = 0;            // 0 = terminal width
    int height = 0;           // 0 = auto from aspect ratio
    bool fullscreen = false;  // Use full terminal dimensions
    float smoothing = 0.0f;   // Temporal smoothing factor [0.0 - 1.0]
    int quant = 0;            // Color quantization step (0 = disabled)
    int max_frame_skip = 5;   // Maximum frames to drop when falling behind
    bool no_audio = false;
    int audio_delay = 0;      // ms
    std::string charset = "classic";
    bool invert = false;
};

class VideoRenderer {
public:
    // Play video in terminal
    static int play(const VideoOptions& opts);
};

} // namespace ascii
