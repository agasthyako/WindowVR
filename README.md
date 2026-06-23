# Real-World Head-Tracking Window VR (Desktop Hologram)

This project implements a **desktop hologram effect** (greatly inspired by Johnny Lee's YouTube [video](https://www.youtube.com/watch?v=Jd3-eiid-Uw), but using AI and Vision instead of rudimentary sensors.
The repository is divided into two components linked through an UDP Port:

1. **The Tracker (Python):** A MediaPipe-powered computer vision backend that converts 2D video frames to real-world 3D coordinates (in meters) and stabilizes them using a low-pass filter.
2. **The Renderer (C++ / OpenGL):** OpenGL 3.3 graphics loop that captures head-tracking data over UDP to manipulate perspective matrices via custom off-axis projection geometry (`glm::frustum`).

---

## Features

* **Metric Coordinate Extraction:** Infers physical 3D positioning $(X, Y, Z)$ in meters relative to the camera lens by utilizing average human anatomy constraints matched against the camera's Horizontal Field of View (HFOV).
* **OneEuro Filtering:** Implements an adaptive low-pass filter on the highly sensitive depth ($Z$) axis. It dynamically cuts off frequencies to eliminate stationary jitter without inviting lag for fast head movements.
* **Asymmetric Frustum Math:** Computes dynamic off-axis projection matrices based on the physical physical dimensions of the monitor (e.g., matching a 14" display footprint) so perspective changes natively as you move sideways, up, or down.
* **Decoupled Architecture:** Telemetry is decoupled from rendering logic over non-blocking UDP sockets (`127.0.0.1:5005`), ensuring zero engine stutters even if video framerates drop.


---

## System Architecture Diagram

```text
 ┌─────────────────────────┐               ┌─────────────────────────┐
 │   Python Tracker app    │               │    C++ Renderer App     │
 │ ─────────────────────── │               │ ─────────────────────── │
 │  Webcam Frame Stream    │               │  Non-blocking UDP Socket│
 │           │             │               │           │             │
 │           ▼             │               │           ▼             │
 │  MediaPipe Face Mesh    │               │  Update Camera Instance │
 │           │             │   Local UDP   │           │             │
 │           ▼             │  Port: 5005   │           ▼             │
 │  3D Metric Projection   │──────────────>│ Calculate Off-Axis      │
 │           │             │  [X, Y, Z]    │ Projection Matrix       │
 │           ▼             │               │           │             │
 │  OneEuro Filter (Z)     │               │           ▼             │
 └─────────────────────────┘               │ Update MVP / Draw Scene │
                                           └─────────────────────────┘

```

---

## Project Structure

```text
├── tracker.py          # Python script containing MediaPipe/OpenCV pipelines & OneEuro filtering
├── face_landmarker.task # Binary face-mesh model asset required by MediaPipe Tasks API
├── Camera.h            # C++ class definition for the mathematical projection/view modifications
├── Camera.cpp          # Matrix computations implementing math for an off-axis viewing window
└── main.cpp            # C++ graphics hub handling glad, glfw, UDP parsing, and the wireframe render loop

```

---

## Requirements & Installation

### 1. Python Tracker Environment

Install the following packages into your environment:

```bash
pip install opencv-python mediapipe numpy

```

> **Crucial File Requirement:** You must download Google's pre-trained **`face_landmarker.task`** bundle from the MediaPipe vision suite guide and save it in the root folder alongside `tracker.py`.

### 2. C++ Renderer Environment

Ensure you have an environment set up to compile modern C++11/C++14 applications linked against these dependencies:

* **GLFW 3** & **GLAD** (OpenGL Loader)
* **GLM** (OpenGL Mathematics Library)

---

## Execution

To see the holographic window effect operational:

1. **Step 1: Compile & Launch C++ Renderer:** Initialize Host Port.
Compile your build environment files (`main.cpp`, `Camera.cpp`). Run the output executable. A window titled "Window VR" will appear displaying a wireframe depth room with a small cube floating inside. It will sit patiently waiting for socket payloads.


2. **Step 2: Start Python Tracker:** Establish Live Stream.
In an adjacent console instance, boot up the tracking core:

```bash
python tracker.py

```

Your webcam indicator light will turn on. The script automatically isolates your face landmarks and shows an overlay tracking window.

1. **ALTERNATIVE/PREFERRED: Run Everything All At Once**
```bash
python3 tracker.py &
TRACKER_PID=$!
cmake --build build && ./build/WindowVR
kill $TRACKER_PID
```
---

## Configuration & Calibration

Because this setup hinges on tracking physical, real-world metrics, you should tweak a few configurations in the source code to fit your local hardware profile perfectly:

### In `tracker.py`:

* **`hfov_degrees=75`**: Change this number to match the exact specifications of your specific webcam model's Horizontal Field of View.

### In `Camera.cpp` / `main.cpp`:

* **`screenW = 0.312f` / `screenH = 0.196f**`: These represent the dimensions of your rendering display area in **meters** (the default values match a standard 14" MacBook display footprint). Measure your screen glass dimensions and update these float flags to prevent visual coordinate scaling errors!

---

## Math Explanation

Standard game cameras use a symmetric viewport centered precisely along the forward look-vector. For virtual holographic displays, the screen behaves as a fixed tracking window. As your head moves away from center, the near projection bounds must morph asymmetrically.

The engine changes the near clipping boundaries ($l, r, b, t$) dynamically based on your physical metric head coordinate $(x, y, z)$:

$$l = \frac{(\text{left}_S - \text{head}_x) \cdot \text{near}}{\text{head}_z}$$

$$r = \frac{(\text{right}_S - \text{head}_x) \cdot \text{near}}{\text{head}_z}$$

$$b = \frac{(\text{bottom}_S - \text{head}_y) \cdot \text{near}}{\text{head}_z}$$

$$t = \frac{(\text{top}_S - \text{head}_y) \cdot \text{near}}{\text{head}_z}$$

Where $X_S$ bounds represent the static physical dimensions of your monitor split evenly down the central origin.
