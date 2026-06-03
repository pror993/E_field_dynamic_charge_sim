# Liénard-Wiechert Field Simulator

A 2D interactive sandbox built in C++ and OpenGL to visualize relativistic electrodynamics in real-time. Instead of cheating with instantaneous Coulomb fields, this engine tracks the actual history of moving charges to compute retarded time fields on the fly.

---

## The Physics Under the Hood

* **Retarded Time Tracking:** Electromagnetic fields propagate at the speed of light $c$. To find the field at any point right now, the engine searches backwards through a particle's historical states (`std::deque`) to solve $t_r = t - \frac{r}{c}$.
* **Sub-Frame Interpolation:** Since physics runs on discrete ticks, the exact retarded time usually falls between frames. The engine uses linear interpolation (`glm::mix`) between past frames to eliminate field jitter.
* **Lorentz Force Engine:** Particles mutually interact using the full $F = q(E + v \times B)$ equation. The engine implements a fixed-timestep accumulator loop to keep the differential equations numerically stable.

---

## Features

* **Interactive Spawning:** Left-click anywhere on the canvas to spawn a point charge with a rightward initial velocity.
* **Dynamic Cursor Probe:** Hover your mouse anywhere to sample the net electric field vector direction and magnitude via a live-scaling arrow.
* **Live UI Control Panel:** Powered by Dear ImGui. Tweak the simulated speed of light ($c$), the electrostatic constant ($k_e$), and collision softening parameters on the fly to warp the fields in real-time.

---

## Coordinate System & Units

To keep the math clean, the entire simulation uses a **Pixel-Second** unit system where **1 unit = 1 pixel**:
* **Position:** Screen space boundaries are `(0,0)` at the bottom-left to `(800,800)` at the top-right.
* **Velocity ($c$):** Measured in pixels per second.
* **Physics Step (`PHYSICS_DT`):** Set to `0.002s` (runs multiple times per visual frame) to handle close-range inverse-square acceleration spikes without exploding.

---

## Tech Stack

* **Language:** C++17
* **Graphics API:** OpenGL 3.3 (Core Profile)
* **Windowing & Context:** GLFW + GLAD
* **Math:** GLM (OpenGL Mathematics)
* **UI:** Dear ImGui

---

## Setup & Compilation

### Dependencies
Ensure you have the required development libraries installed (X11/Mesa/GLFW):
```bash
sudo apt install -y build-essential cmake pkg-config libglfw3-dev libglm-dev mesa-common-dev libgl1-mesa-dev
```
### Build and run 
Compile all the core components along with the ImGui backends:

```bash
g++ main.cpp glad.c imgui*.cpp -o simulator -I./include -lglfw -lGL -ldl
```
If running inside WSL2 or environments with driver routing warnings, force software rendering to bypass the Mesa loader errors:

```bash
LIBGL_ALWAYS_SOFTWARE=1 ./simulator
```