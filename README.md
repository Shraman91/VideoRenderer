# ASCII Art Renderer

A C++ command-line tool that renders images, videos, and procedural 3D demos as ASCII art in your terminal.

## Features

- **Image rendering** - Convert PNG/JPG images to ASCII art
- **Video playback** - Stream videos as real-time ASCII animation with optional color
- **Procedural demos** - Watch rotating 3D objects (cube, donut) rendered in ASCII
- Multiple character sets (classic, detailed, letters)
- Color support via ANSI 24-bit escape codes
- Aspect-ratio correction for terminal cells

## Requirements (MSYS2/MinGW on Windows)

This project is built for MSYS2 with the MinGW-w64 toolchain. Install dependencies:

```bash
# Update MSYS2 package database
pacman -Syu

# Install build tools
pacman -S mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-ffmpeg \
          make
```

## Build

```bash
mkdir build
cd build
cmake -G "MSYS Makefiles" ..
make -j$(nproc)
```

Or with Ninja (faster):

```bash
mkdir build
cd build
cmake -G Ninja ..
ninja
```

## Usage

### List available demos
```bash
./ascii-art list
```

### Render an image
```bash
./ascii-art image path/to/image.png
./ascii-art image photo.jpg --width 80 --charset detailed
./ascii-art image logo.png --invert -o output.txt
```

Options:
- `--width N` - Set output width in characters (default: terminal width)
- `--height N` - Set output height in characters (default: auto from aspect ratio)
- `--charset classic|detailed|letters` - Character ramp to use
- `--invert` - Invert brightness mapping
- `-o, --output PATH` - Write to file instead of stdout

### Render a video
```bash
./ascii-art video movie.mp4
./ascii-art video clip.avi --color --fps 30 --width 120
./ascii-art video video.mkv --no-audio --invert
```

Options:
- `--color` - Enable 24-bit color output
- `--fps N` - Target playback framerate (default: source fps)
- `--width N` - Output width in characters
- `--smoothing N` - Temporal smoothing factor (0-1, default: 0)
- `--quant N` - Color quantization level for --color mode
- `--max-frame-skip N` - Maximum consecutive frames to drop when behind (default: 5)
- `--no-audio` - Disable audio playback
- `--audio-delay N` - Audio sync offset in milliseconds
- `--charset classic|detailed|letters` - Character ramp
- `--invert` - Invert brightness

### Run a procedural demo
```bash
./ascii-art demo cube
./ascii-art demo donut --width 80 --charset detailed
```

Press Ctrl+C to exit.

## Architecture

- `src/main.cpp` - CLI entry point and subcommand dispatch
- `src/image_renderer.cpp` - Image decoding and ASCII conversion
- `src/video_renderer.cpp` - FFmpeg integration and video playback
- `src/terminal.cpp` - Terminal size detection, ANSI codes, signal handling
- `src/demos/` - Procedural demo implementations (cube, donut, etc.)
- `third_party/` - Vendored single-header libraries (stb_image.h)

## Adding New Demos

1. Create a new file in `src/demos/your_demo.cpp`
2. Implement the `DemoRenderer` interface
3. Register it in `src/demos/demo_registry.cpp`

Each demo receives frame index, dimensions, and character ramp, and returns a rendered frame string.

## License

MIT
