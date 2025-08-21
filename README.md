# 3D Aquarium - OpenGL Simulation

A beautiful real-time 3D aquarium simulation built with OpenGL 4.1, featuring realistic fish behavior, water effects, and advanced lighting.

## Features

- **Rich Fish Diversity**: Eight species of fish (Clownfish, Neon Tetra, Zebra Danio, Angelfish, Goldfish, Betta, Guppy, Platy) with realistic flocking behavior
- **Advanced Water Rendering**: Realistic water surface with refraction and caustics
- **Dynamic Lighting**: Image-Based Lighting (IBL) with HDR environment maps
- **Particle Effects**: Rising bubbles with realistic physics
- **Rich Decorative Elements**: Plants, rocks, corals, shells, and driftwood with natural variation
- **Modern Graphics**: PBR materials, HDR rendering, and tone mapping

## Prerequisites

- **macOS** (this version is specifically designed for macOS OpenGL 4.1)
- **CMake** 3.16 or higher
- **C++17** compatible compiler (Xcode Command Line Tools)
- **Git** (for dependency fetching)

## Installation

### 1. Install Dependencies

If you don't have Homebrew installed, install it first:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Install CMake:

```bash
brew install cmake
```

### 2. Build the Project

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(sysctl -n hw.ncpu)
```

### 3. Run the Aquarium

```bash
./Aquarium
```

## Controls

- **WASD**: Move camera forward/left/backward/right
- **Q/E**: Move camera down/up
- **Mouse**: Look around (camera is locked to mouse movement)
- **Shift**: Hold for faster movement
- **F1**: Toggle wireframe mode
- **Escape**: Exit the application

## Project Structure

```
3daquarium/
├── CMakeLists.txt          # Build configuration
├── src/
│   └── main.cpp           # Main application code
└── shaders/               # GLSL shader files
    ├── basic.vert/frag    # Basic PBR material shader
    ├── water.vert/frag    # Water surface shader
    ├── fish.vert/frag     # Fish rendering shader
    ├── bubbles.vert/frag  # Bubble particle shader
    ├── plant.vert/frag    # Plant rendering shader
    ├── tonemap.vert/frag  # HDR to LDR conversion
    └── ibl_*.frag         # Image-based lighting shaders
```

## Technical Details

### Graphics Pipeline

1. **HDR Rendering**: Scene is rendered to a high dynamic range framebuffer
2. **Image-Based Lighting**: Procedural HDR environment map with irradiance and prefiltered cubemaps
3. **PBR Materials**: Physically-based rendering with BRDF lookup tables
4. **Water Effects**: Screen-space refraction and caustics simulation
5. **Tone Mapping**: ACES filmic tone mapping for final output

### Fish Behavior

The fish use a flocking algorithm with three main behaviors:

- **Alignment**: Fish tend to swim in the same direction as nearby fish
- **Cohesion**: Fish are attracted to the center of nearby fish
- **Separation**: Fish avoid getting too close to each other

### Performance

- **Instanced Rendering**: Fish and plants are rendered using GPU instancing
- **Efficient Geometry**: Optimized mesh generation for all objects
- **Modern OpenGL**: Uses OpenGL 4.1 core profile features

## Customization

You can modify various parameters in `src/main.cpp`:

- **Fish Count**: Change `N_CLOWN`, `N_NEON`, `N_DANIO`, `N_ANGELFISH`, `N_GOLDFISH`, `N_BETTA`, `N_GUPPY`, `N_PLATY` for different fish populations
- **Tank Size**: Modify `TANK_EXTENTS` to change aquarium dimensions (now 50% larger!)
- **Water Level**: Adjust `waterY` to change water height
- **Lighting**: Modify `lightDir`, `exposure`, and fog parameters

## Troubleshooting

### Build Issues

1. **CMake not found**: Install CMake via Homebrew: `brew install cmake`
2. **Compiler errors**: Ensure Xcode Command Line Tools are installed: `xcode-select --install`
3. **OpenGL errors**: This project requires macOS with OpenGL 4.1 support

### Runtime Issues

1. **Window doesn't appear**: Check that your graphics drivers support OpenGL 4.1
2. **Poor performance**: Try reducing fish count or window size
3. **Shader compilation errors**: Ensure all shader files are present in the `shaders/` directory

## Dependencies

This project automatically downloads and builds:

- **GLFW 3.3.9**: Window management and input handling
- **GLM 1.0.1**: Mathematics library for graphics

## License

This project is provided as-is for educational and entertainment purposes.

## Contributing

Feel free to submit issues or pull requests to improve the aquarium simulation!
