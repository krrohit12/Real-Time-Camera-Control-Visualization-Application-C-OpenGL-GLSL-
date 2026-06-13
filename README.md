# Camera App — Real-Time Camera Control & Visualization

A desktop application that captures live webcam video, renders it with OpenGL,
applies GLSL shader effects in real time, and supports snapshot / video recording.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  Main Thread                                                    │
│  ┌───────────┐   ┌──────────────┐   ┌──────────────────────┐  │
│  │Application│──▶│  Renderer    │──▶│  ShaderManager       │  │
│  │           │   │  (OpenGL)    │   │  6 GLSL effects      │  │
│  │           │   └──────────────┘   └──────────────────────┘  │
│  │           │   ┌──────────────┐                              │
│  │           │──▶│ ControlPanel │ (Dear ImGui)                 │
│  └─────┬─────┘   └──────────────┘                              │
│        │                                                        │
└────────┼────────────────────────────────────────────────────────┘
         │
         │  ThreadSafeQueue<CameraFrame>
         │
┌────────▼────────────────────────────────────────────────────────┐
│  Capture Thread                                                 │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  CameraManager  ──▶  BGR→RGB ──▶  push to queue          │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│  Snapshot Thread          Recording Thread                      │
│  SnapshotManager          RecordingManager                      │
│  (imwrite worker)         (VideoWriter worker)                  │
└─────────────────────────────────────────────────────────────────┘
```

## Dependencies

| Library    | Version | Purpose             |
|------------|---------|---------------------|
| OpenCV     | 4.x     | Camera capture, I/O |
| GLFW       | 3.x     | Window + OpenGL ctx |
| GLAD       | 0.1.36  | OpenGL loader       |
| Dear ImGui | 1.90.5  | UI                  |

## Build

### macOS (Homebrew)

```bash
brew install opencv glfw cmake

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build . -j$(sysctl -n hw.logicalcpu)
open bin/CameraApp.app
```

### Linux (apt)

```bash
sudo apt install libopencv-dev libglfw3-dev cmake build-essential

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./bin/CameraApp
```

### Windows (vcpkg)

```powershell
vcpkg install opencv glfw3
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

> **macOS note:** The app must be launched as a `.app` bundle (via `open`) so that
> macOS grants camera permission. Running the raw binary directly will produce no window.

## UI Controls

All interactions are through the control panel on the left side of the window.

| Panel            | Controls                                              |
|------------------|-------------------------------------------------------|
| Camera           | Brightness, Contrast, Exposure, Gain, Resolution, FPS |
| Shader Effects   | Normal, Grayscale, Sepia, Edge Detection, Blur, Brightness/Contrast |
| Snapshot         | Take Snapshot button — saves timestamped PNG          |
| Recording        | Start / Stop — saves MP4 (falls back to AVI)          |

## GLSL Effects

1. **Normal** — passthrough, no processing
2. **Grayscale** — ITU-R BT.601 luminance conversion
3. **Sepia** — warm tone colour matrix
4. **Edge Detection** — Sobel operator with adjustable strength
5. **Blur** — box blur with adjustable radius (1–10)
6. **Brightness/Contrast** — linear brightness and contrast adjustment

Snapshots and recordings always capture the frame **with the active shader effect applied**,
exactly matching what is visible on screen.

## Threading Model

```
Main Thread      Capture Thread      Snapshot Thread    Recording Thread
     │                 │                    │                  │
     │  glfwPollEvents │                    │                  │
     │  renderFrame()  │  cap.read()        │                  │
     │                 │  BGR→RGB           │                  │
     │                 │  queue.push()      │                  │
     │  queue.tryPop() │                    │                  │
     │  uploadFrame()  │                    │                  │
     │  draw()         │                    │                  │
     │  glReadPixels() │                    │                  │
     │  snapshot ──────┼──────────────────▶ imwrite()          │
     │  rec frame ─────┼──────────────────────────────────▶ write()
     │  ImGui          │                    │                  │
     │  swapBuffers()  │                    │                  │
```

- **Main Thread** — render loop, OpenGL calls, ImGui UI (only thread that touches OpenGL)
- **Capture Thread** — continuously reads webcam frames, converts BGR→RGB, pushes to queue
- **Snapshot Thread** — sleeps until a snapshot is requested, saves PNG, goes back to sleep
- **Recording Thread** — sleeps until a frame is pushed, writes to MP4, goes back to sleep

## Known Platform Limitations

| Feature | macOS | Linux | Windows |
|---------|-------|-------|---------|
| Camera hardware Brightness/Contrast/Exposure/Gain | Not supported (AVFoundation limitation) | Supported via V4L2 | Supported via DirectShow |
| Resolution change | Supported | Supported | Supported |
| FPS change | Supported | Supported | Supported |
| MP4 recording | Supported | Supported | Supported |

On macOS the four hardware camera controls show as **N/A** — this is a platform
limitation of the AVFoundation backend in OpenCV, not a bug.

## Project Structure

```
Camera App/
├── src/
│   ├── main.cpp
│   ├── Application.h/cpp        Main loop, coordinates all subsystems
│   ├── CameraManager.h/cpp      Webcam capture on a background thread
│   ├── ThreadSafeQueue.h        Thread-safe queue between capture and render
│   ├── Renderer.h/cpp           OpenGL texture, quad rendering, frame readback
│   ├── ShaderManager.h/cpp      Compiles and manages GLSL shader programs
│   ├── ControlPanel.h/cpp       ImGui UI panels
│   ├── SnapshotManager.h/cpp    Async PNG saving on a background thread
│   └── RecordingManager.h/cpp   Async MP4 encoding on a background thread
├── shaders/
│   ├── passthrough.vert
│   ├── normal.frag
│   ├── grayscale.frag
│   ├── sepia.frag
│   ├── edge_detection.frag
│   ├── blur.frag
│   └── brightness_contrast.frag
├── CMakeLists.txt
├── Info.plist.in                macOS bundle + camera permission declaration
```

