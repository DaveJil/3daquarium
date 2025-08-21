#!/bin/bash

# 3D Aquarium Build Script
# This script automates the build process for the 3D aquarium project

set -e  # Exit on any error

echo "🐠 Building 3D Aquarium..."

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: CMakeLists.txt not found. Please run this script from the project root directory."
    exit 1
fi

# Check if CMake is installed
if ! command -v cmake &> /dev/null; then
    echo "❌ CMake not found. Installing via Homebrew..."
    if ! command -v brew &> /dev/null; then
        echo "❌ Homebrew not found. Please install Homebrew first:"
        echo "   /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi
    brew install cmake
fi

# Create build directory
echo "📁 Creating build directory..."
rm -rf build
mkdir -p build
cd build

# Configure with CMake
echo "⚙️  Configuring with CMake..."
cmake ..

# Build the project
echo "🔨 Building project..."
make -j$(sysctl -n hw.ncpu)

echo "✅ Build completed successfully!"
echo ""
echo "🎮 To run the aquarium:"
echo "   cd build && ./Aquarium"
echo ""
echo "📖 Controls:"
echo "   WASD - Move camera"
echo "   Q/E - Move up/down"
echo "   Mouse - Look around"
echo "   Shift - Faster movement"
echo "   F1 - Toggle wireframe"
echo "   Escape - Exit"
