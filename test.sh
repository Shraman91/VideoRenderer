#!/bin/bash

# Test script for ASCII Art Renderer
# Run from MSYS2 MinGW 64-bit shell

echo "=== ASCII Art Renderer Test Suite ==="
echo ""

cd "$(dirname "$0")"

# Check if binary exists
if [ ! -f "build/ascii-art.exe" ]; then
    echo "Error: build/ascii-art.exe not found. Run cmake and ninja first."
    exit 1
fi

echo "✓ Binary found at build/ascii-art.exe"
echo ""

# Test 1: List command
echo "Test 1: List demos"
./build/ascii-art.exe list
echo ""

# Test 2: Help command
echo "Test 2: Help output"
./build/ascii-art.exe --help | head -n 15
echo ""

# Test 3: Download and render image
echo "Test 3: Image rendering (classic charset)"
if [ ! -f "test.jpg" ]; then
    curl -sL "https://picsum.photos/80/60" -o test.jpg
fi
./build/ascii-art.exe image test.jpg --width 40 | head -n 10
echo ""

# Test 4: Image with detailed charset
echo "Test 4: Image rendering (detailed charset)"
./build/ascii-art.exe image test.jpg --width 40 --charset detailed | head -n 10
echo ""

# Test 5: Image with invert
echo "Test 5: Image rendering (inverted)"
./build/ascii-art.exe image test.jpg --width 40 --invert | head -n 10
echo ""

# Test 6: Save to file
echo "Test 6: Save to file"
./build/ascii-art.exe image test.jpg -o test_output.txt --width 30
if [ -f "test_output.txt" ]; then
    echo "✓ Output file created successfully"
    wc -l test_output.txt
else
    echo "✗ Output file not created"
fi
echo ""

echo "=== All tests completed ==="
echo ""
echo "To test procedural demos interactively (press Ctrl+C to exit):"
echo "  ./build/ascii-art.exe demo cube"
echo "  ./build/ascii-art.exe demo donut --charset detailed"
echo ""
echo "To test video playback (requires a video file):"
echo "  ./build/ascii-art.exe video movie.mp4"
echo "  ./build/ascii-art.exe video clip.mp4 --color --fps 24"
