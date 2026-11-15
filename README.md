# RGB Video & ASCII Art Renderer

A high-performance C++ command-line tool that renders videos, images, and procedural 3D demos with photorealistic 24-bit TrueColor in **Windows Terminal**, **PowerShell**, and **Command Prompt** (as well as MSYS2 / Linux).

---

## Features

- **Photorealistic Half-Block RGB Video (`--mode blocks`)** - Uses unicode upper half-blocks (`▀`) with 24-bit foreground and background colors to deliver **double vertical resolution** and square pixels.
- **Native Windows Terminal / PowerShell / CMD Support** - Fully configures Windows Virtual Terminal Processing (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`), UTF-8 code page, and high-precision timers.
- **Ultra-Smooth, Tearing-Free Playback** -
  - Windows 1ms multimedia timer (`timeBeginPeriod`).
  - PTS presentation timestamp tracking for exact audio/video synchronization without drift.
  - Sub-millisecond hybrid sleep (coarse sleep + fine spin-wait yield).
  - Atomic direct-to-console writes (`WriteFile`) with DEC synchronized update markers (`\033[?2026h`).
  - State-cached ANSI escape sequence generator that skips redundant color codes.
- **True Fullscreen Mode (`--fullscreen` or `-f`)** -
  - Automatically maximizes the Windows console window.
  - Dynamically fills the full terminal width and height while preserving the video's aspect ratio with centered letterboxing/pillarboxing.
  - Dynamically adapts if you resize the terminal or press `F11` in Windows Terminal.
- **Multiple Render Modes**:
  - `blocks` - Half-block 24-bit TrueColor (highest resolution & photorealistic, default).
  - `ascii` - Classic or colored ASCII character glyphs with luminance ramps.
  - `solid` - Full-block (`█`) 24-bit color cells.
- **Synchronized Audio Playback** - Seamless background audio powered by `ffplay` with automatic binary discovery.
- **Interactive Controls** - Pause, toggle fullscreen, cycle render modes, and cleanly quit on the fly.

---

## Running in Windows Terminal, PowerShell, or CMD

You do **not** need to open an MSYS2/MinGW terminal! You can run directly in **Windows Terminal**, **PowerShell**, or **CMD**:

### Option 1: Use the included launchers (Instant, zero configuration)

From PowerShell:
```powershell
.\ascii-art.ps1 video dance.mp4 --fullscreen
```

From Command Prompt or PowerShell:
```cmd
ascii-art video dance.mp4 --fullscreen
# or
.\ascii-art.bat video dance.mp4 --fullscreen
```

### Option 2: Add MinGW64 to your User PATH (Run `ascii-art.exe` anywhere)

To run `build\ascii-art.exe` directly from any terminal window without batch wrappers, add `C:\msys64\mingw64\bin` to your Windows User PATH:

In PowerShell:
```powershell
[Environment]::SetEnvironmentVariable("PATH", [Environment]::GetEnvironmentVariable("PATH", "User") + ";C:\msys64\mingw64\bin", "User")
```
After restarting your terminal, you can run:
```powershell
.\build\ascii-art.exe video dance.mp4 --fullscreen
```

---

## Usage

### 1. Video Playback

```powershell
# Fullscreen high-definition RGB half-block playback (default, smoothest quality)
.\ascii-art.ps1 video dance.mp4 --fullscreen

# Video in a standard window with custom width
.\ascii-art.ps1 video dragon.mp4 --width 140

# Colored ASCII character mode
.\ascii-art.ps1 video dance.mp4 --mode ascii --color --fullscreen

# Classic monochrome ASCII mode
.\ascii-art.ps1 video dance.mp4 --mode ascii --charset detailed

# Full block mode
.\ascii-art.ps1 video dance.mp4 --mode solid --fullscreen
```

#### Video Options:
- `--fullscreen`, `-f` - Maximize the console window and scale to full terminal size
- `--mode blocks|ascii|solid` - Render mode (default: `blocks`)
- `--blocks` - Shortcut for `--mode blocks --color`
- `--color` - Enable 24-bit RGB truecolor output
- `--fps N` - Target framerate (default: source video FPS)
- `--width N` - Output width in character columns
- `--height N` - Output height in rows
- `--smoothing N` - Temporal frame blending [0.0 - 0.95]
- `--quant N` - Color quantization step (e.g. 8, 16)
- `--max-frame-skip N` - Maximum consecutive frames to drop when falling behind (default: 5)
- `--no-audio` - Disable audio playback
- `--audio-delay N` - Audio sync offset in milliseconds (+/- ms)
- `--charset classic|detailed|letters` - Character ramp for ascii mode
- `--invert` - Invert brightness mapping

#### Interactive Keyboard Controls (During Playback):
- `Space` - **Pause / Resume** playback
- `f` - **Maximize window / Fullscreen**
- `m` - **Cycle render modes** (`blocks` ➔ `ascii` ➔ `solid`)
- `q` or `Esc` - **Exit cleanly** and restore terminal settings
- `F11` (in Windows Terminal) - Enter true borderless fullscreen

---

### 2. Image Rendering

```powershell
.\ascii-art.ps1 image test.jpg --width 100
.\ascii-art.ps1 image photo.png --charset detailed -o output.txt
```

---

### 3. Procedural 3D Demos

```powershell
# List demos
.\ascii-art.ps1 list

# Run rotating 3D wireframe cube
.\ascii-art.ps1 demo cube

# Run spinning torus (donut)
.\ascii-art.ps1 demo donut --charset detailed
```

---

## Building from Source

Dependencies (installed once via MSYS2):
```bash
pacman -S mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-ffmpeg \
          mingw-w64-x86_64-pkg-config
```

Build with Ninja:
```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
cd build
cmake -G Ninja ..
ninja
```

---

## Architecture

- `src/terminal.cpp` - Console mode configuration (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`), UTF-8 code page setup, 1ms timer precision, atomic console write (`WriteFile`), and window maximization.
- `src/video_renderer.cpp` - FFmpeg decoding, bilinear scaling, PTS presentation timing, half-block 24-bit truecolor rasterizer, state-cached ANSI escape generator, and `ffplay` audio synchronization.
- `src/main.cpp` - Command-line interface and subcommand dispatch.
- `src/image_renderer.cpp` - Image loading and ASCII conversion.
- `src/demos/` - Procedural 3D math demos (cube, donut, sphere, planet).

---

## License

MIT
