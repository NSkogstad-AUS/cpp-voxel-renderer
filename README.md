# C++ Voxel Renderer

Small OpenGL playground that renders voxel chunks and a simple heightmap terrain. The app is written in C++17 with GLFW, GLEW, GLM, and GLUT, and drives a free-fly camera (WASD + mouse look, Shift to sprint, Esc to toggle cursor). On startup it loads a grayscale heightmap image into vertex data, draws chunked cubes with both filled faces and wireframe outlines, and lets you explore the scene in real time.

Build on macOS with Homebrew toolchains via `make build`, which links the GLFW, GLEW, GLM, and freeglut installs detected by `brew --prefix`. The executable is emitted to `run/app`.

make clean
make build
run/app

There's also a imbedded gui within the application, utilising the 'imgui' library, with multiple different setting sliders for real-time modifications to the terrain. (Implemented just so it's easier for me to see how the terrain reacts to different settings)

## Current view of the Application
![Renderer window](/README_files/window_image.png)
