#!/bin/bash
# Dependency check script for ASCII Art Renderer
# Run this from MSYS2 MinGW 64-bit shell

echo "=== ASCII Art Renderer - Dependency Check ==="
echo ""

# Color codes for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

all_good=true

echo "Checking build tools..."
for cmd in g++ gcc cmake ninja pkg-config; do
    if command -v $cmd >/dev/null 2>&1; then
        version=$(command $cmd --version 2>/dev/null | head -n1 || echo "version unknown")
        echo -e "  ${GREEN}✓${NC} $cmd found: $version"
    else
        echo -e "  ${RED}✗${NC} $cmd NOT FOUND"
        all_good=false
    fi
done

echo ""
echo "Checking FFmpeg tools..."
for cmd in ffmpeg ffplay; do
    if command -v $cmd >/dev/null 2>&1; then
        version=$($cmd -version 2>/dev/null | head -n1 || echo "version unknown")
        echo -e "  ${GREEN}✓${NC} $cmd found: $version"
    else
        echo -e "  ${RED}✗${NC} $cmd NOT FOUND"
        all_good=false
    fi
done

echo ""
echo "Checking FFmpeg development libraries..."
for lib in libavformat libavcodec libswscale libavutil; do
    if pkg-config --exists $lib 2>/dev/null; then
        version=$(pkg-config --modversion $lib)
        echo -e "  ${GREEN}✓${NC} $lib found (v$version)"
    else
        echo -e "  ${RED}✗${NC} $lib NOT FOUND"
        all_good=false
    fi
done

echo ""
echo "Checking MSYS2 packages..."
required_packages=(
    "mingw-w64-x86_64-gcc"
    "mingw-w64-x86_64-cmake"
    "mingw-w64-x86_64-ninja"
    "mingw-w64-x86_64-ffmpeg"
    "mingw-w64-x86_64-pkg-config"
)

for pkg in "${required_packages[@]}"; do
    if pacman -Q "$pkg" >/dev/null 2>&1; then
        version=$(pacman -Q "$pkg" | awk '{print $2}')
        echo -e "  ${GREEN}✓${NC} $pkg ($version)"
    else
        echo -e "  ${RED}✗${NC} $pkg NOT INSTALLED"
        all_good=false
    fi
done

echo ""
if [ "$all_good" = true ]; then
    echo -e "${GREEN}✓ All dependencies are installed!${NC}"
    echo ""
    echo "You can now build the project:"
    echo "  mkdir -p build && cd build"
    echo "  cmake -G Ninja .."
    echo "  ninja"
    exit 0
else
    echo -e "${RED}✗ Missing dependencies detected${NC}"
    echo ""
    echo "Install missing packages with:"
    echo "  pacman -S mingw-w64-x86_64-gcc \\"
    echo "            mingw-w64-x86_64-cmake \\"
    echo "            mingw-w64-x86_64-ninja \\"
    echo "            mingw-w64-x86_64-ffmpeg \\"
    echo "            mingw-w64-x86_64-pkg-config"
    echo ""
    echo -e "${YELLOW}NOTE:${NC} Make sure you're running this in the MSYS2 MinGW 64-bit shell,"
    echo "      NOT the MSYS2 MSYS shell or Windows CMD/PowerShell."
    exit 1
fi
