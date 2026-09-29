# MSYS2 Setup Guide for ASCII Art Renderer

## Prerequisites

You have MSYS2 installed at `C:\msys64`. Now we need to set up the MinGW-w64 toolchain and dependencies.

## Installation Steps

### 1. Launch MSYS2 MinGW 64-bit Shell

**Important:** Use the **MSYS2 MinGW 64-bit** shell, NOT the MSYS2 MSYS shell.

- Start Menu → MSYS2 MinGW 64-bit
- Or run: `C:\msys64\mingw64.exe`

### 2. Update MSYS2 package database

```bash
pacman -Syu
```

If it asks to close the terminal, close it and reopen, then run:

```bash
pacman -Su
```

### 3. Install build tools and dependencies

```bash
pacman -S mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-ffmpeg \
          mingw-w64-x86_64-pkg-config \
          make
```

This installs:
- `g++` and `gcc` (MinGW-w64 compilers)
- `cmake` (build system)
- `ninja` (fast build tool, optional but recommended)
- `ffmpeg` libraries (for video support)
- `pkg-config` (for finding FFmpeg)
- `make` (GNU Make)

### 4. Verify installation

```bash
g++ --version
cmake --version
pkg-config --modversion libavformat
```

All three commands should output version information.

## Building the Project

From the MSYS2 MinGW 64-bit shell, navigate to the project directory:

```bash
cd "/c/Users/vkdgs/OneDrive/Desktop/Projects/RGB Video Renderer"
```

### Build with Make

```bash
mkdir build
cd build
cmake -G "MSYS Makefiles" ..
make -j$(nproc)
```

### Build with Ninja (faster)

```bash
mkdir build
cd build
cmake -G Ninja ..
ninja
```

The compiled binary will be at `build/ascii-art.exe`.

## Testing

### Test image rendering

Download a test image first:

```bash
curl -sL https://picsum.photos/800/600 -o test.jpg
./ascii-art.exe image test.jpg
```

### Test procedural demos

```bash
# List available demos
./ascii-art.exe list

# Run the spinning cube
./ascii-art.exe demo cube

# Run the donut
./ascii-art.exe demo donut --charset detailed
```

Press Ctrl+C to exit demos.

## Current Status

✅ **Working:**
- `list` - List available demos
- `image` - Render PNG/JPG images as ASCII
- `demo cube` - Rotating 3D wireframe cube
- `demo donut` - Classic spinning torus

🚧 **Coming next:**
- `video` - Video playback with FFmpeg (requires FFmpeg to be compiled in)

## Troubleshooting

### "command not found" errors

Make sure you're using the **MSYS2 MinGW 64-bit** shell, not the regular MSYS2 shell or Windows CMD/PowerShell.

### CMake can't find FFmpeg

Install pkg-config if not already installed:

```bash
pacman -S mingw-w64-x86_64-pkg-config
```

### Linking errors

Make sure you installed the MinGW-w64 toolchain, not the MSYS toolchain. The package names should start with `mingw-w64-x86_64-`.

## Path Setup (Optional)

To use the binary from anywhere, you can:

1. Add the build directory to your PATH in MSYS2:
   ```bash
   export PATH="$PATH:/c/Users/vkdgs/OneDrive/Desktop/Projects/RGB Video Renderer/build"
   ```

2. Or install it system-wide:
   ```bash
   cd build
   cmake --install . --prefix /mingw64
   ```
