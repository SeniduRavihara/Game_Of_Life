# Conway's Game of Life (Interactive GUI with CUDA GPU & Multi-Threaded CPU)

A feature-complete, modern graphical application of **Conway's Game of Life** built with standard **C++14**, **SDL2**, and **NVIDIA CUDA**. 

It features an embedded graphical user interface (GUI) with interactive controls, pattern libraries, canvas zooming/panning, and a dual-engine architecture that runs out-of-the-box on standard CPUs (via OpenMP) while seamlessly utilizing NVIDIA CUDA GPUs when hardware acceleration is available.

---

## Key Features

- **Modern Embedded GUI**:
  - **Header Bar**: Displays current active compute engine badge, real-time FPS counter, generation count, live population count, and grid resolution.
  - **Sidebar Control Panel**: Organized control sections for simulation playback, speed presets, drawing tools, brush sizes, and pattern libraries.
  - **Bottom Status Bar**: Live cursor grid coordinate tracking, cell state inspector, active tool indicator, and zoom level.
  - **Zero External Font Dependencies**: Built-in 8x8 font atlas rendered directly with hardware-accelerated SDL2.
- **Dual-Engine Architecture**:
  - **Multi-Threaded CPU Engine (OpenMP)**: Highly optimized parallel stencil calculation with toroidal wrapping and parallel reduction for live cell counting. Runs on any computer (Intel, AMD, etc.) without requiring an NVIDIA GPU.
  - **CUDA GPU Hardware Acceleration**: Utilizes 2D thread blocks (`16x16`), on-device ARGB pixel generation, pinned host memory (`cudaMallocHost`), on-GPU pseudorandom hash initialization, and GPU-side brush drawing.
  - **Dynamic Switching**: Switch between CPU and GPU engines on the fly from the GUI while preserving the grid state!
- **Interactive Canvas with Camera**:
  - **Zoom**: Smooth zooming in and out (10% to 3200%) centered on mouse cursor via scroll wheel.
  - **Pan**: Smooth dragging across large worlds with the middle mouse button.
  - **View Presets**: Instant "Reset View (1:1)" and "Fit Canvas" buttons.
- **Drawing Tools & Pattern Stamp Library**:
  - **Pen Mode**: Click or drag with the left mouse button to paint living cells.
  - **Erase Mode**: Click or drag with right mouse button (or Pen in Erase mode) to kill cells.
  - **Configurable Brush Sizes**: 1x1, 3x3, 5x5 brush diameters.
  - **Pattern Presets**: Stamp classic Conway patterns directly into the universe:
    - *Glider* (traveling spaceship)
    - *Gosper Glider Gun* (infinite factory producing moving gliders)
    - *Pulsar* (period-3 oscillator)
    - *Pentadecathlon* (period-15 oscillator)
    - *Acorn* (methuselah evolving for over 5,000 generations)
    - *Diehard* (vanishes after 130 generations)
    - Live translucent placement ghost preview follows your cursor before stamping!
- **Automatic Build Detection**:
  - Running `make` automatically inspects your environment. If `nvcc` is detected, it compiles with full CUDA hardware acceleration. If not (such as on standard laptops), it automatically compiles with the multi-threaded CPU engine without errors.

---

## User Interface & Layout

```
+---------------------------------------------------------------------------------------------------------+
| [Header] CONWAY'S GAME OF LIFE | [ENGINE: CPU (OpenMP)] | GEN: 1,420 | POP: 8,310 | FPS: 60.0 | 512x512 |
+------------------------------------------------------------+--------------------------------------------+
|                                                            | --- SIMULATION CONTROLS ---                |
|                                                            | [ > PLAY / || PAUSE ]                      |
|                     INTERACTIVE CANVAS                     | [ STEP (S) ]          [ CLEAR (C) ]        |
|                                                            | [ RANDOMIZE (20%) ]                        |
|               - Left-Click / Drag: Draw                    |                                            |
|               - Right-Click / Drag: Erase                  | --- EXECUTION SPEED ---                    |
|               - Mouse Wheel: Zoom (10% - 3200%)            | [ << SLOW ]   [ 60 FPS ]   [ FAST >> ]     |
|               - Middle-Click / Drag: Pan Canvas            | [ MAX SPEED (0ms DELAY) ]                  |
|               - Translucent Stamp Preview                  |                                            |
|                                                            | --- TOOLS & BRUSH ---                      |
|                                                            | [ PEN ]       [ ERASE ]     [ STAMP ]      |
|                                                            | [ Size 1 ]    [ Size 3 ]    [ Size 5 ]     |
|                                                            |                                            |
|                                                            | --- PATTERN STAMP LIBRARY ---              |
|                                                            | [ Glider ]           [ Gosper Gun ]        |
|                                                            | [ Pulsar ]           [ Pentadecathlon ]    |
|                                                            | [ Acorn ]            [ Diehard ]           |
|                                                            |                                            |
|                                                            | --- COMPUTE ENGINE ---                     |
|                                                            | [ SWITCH ENGINE ]                          |
|                                                            |                                            |
|                                                            | --- CAMERA / VIEW ---                      |
|                                                            | [ RESET VIEW ]       [ FIT CANVAS ]        |
+------------------------------------------------------------+--------------------------------------------+
| [Status Bar] Ready | COORD: (X: 184, Y: 295) | CELL: ALIVE | ZOOM: 100%                                 |
+---------------------------------------------------------------------------------------------------------+
```

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `[Space]` | **Play / Pause** simulation |
| `[S]` | **Single-step** 1 generation (when paused) |
| `[R]` | **Randomize** grid (20% alive density) |
| `[C]` | **Clear** entire grid (kill all cells) |
| `[+ / = / Up Arrow]` | **Increase simulation speed** (decrease delay) |
| `[- / Down Arrow]` | **Decrease simulation speed** (increase delay) |
| `[Left-Click + Drag]` | Paint living cells or stamp selected pattern |
| `[Right-Click + Drag]`| Erase cells |
| `[Middle-Click + Drag]`| Pan camera view |
| `[Scroll Wheel]` | Zoom in / out centered at mouse cursor |
| `[Esc / Q]` | Exit application |

---

## Building and Running

### 1. Automatic Build (Recommended)

Just run `make`. It will detect whether your machine has the NVIDIA CUDA Toolkit (`nvcc`):
```bash
make
./game_of_life
```

You can optionally specify grid dimensions (e.g. 512x512, 800x600, 1024x1024):
```bash
./game_of_life 800 600
```

### 2. Desktop Application Installation (Installable App)

To install Conway's Game of Life as a native desktop application with an icon in your application launcher and on your desktop:

```bash
make install
```
This automatically:
- Installs the executable to `~/.local/bin/game_of_life`
- Installs the high-resolution app icon to `~/.local/share/icons/hicolor/scalable/apps/game-of-life.svg`
- Registers the desktop entry to `~/.local/share/applications/game-of-life.desktop`
- Adds a shortcut right onto your `Desktop` (`~/Desktop/game-of-life.desktop`)

You can now launch the app from:
1. Your **GNOME / Desktop Application Menu** (press the `Super`/Windows key and search `Game of Life`).
2. Double-clicking the **Desktop icon**.
3. Running `game_of_life` from any terminal.

To cleanly uninstall at any time:
```bash
make uninstall
```

### 3. Explicit Target Builds

- **Force Multi-Threaded CPU build** (runs on any PC with `g++` and SDL2):
  ```bash
  make cpu
  ./game_of_life
  ```
- **Force CUDA GPU build** (requires an NVIDIA GPU and `nvcc`):
  ```bash
  make cuda
  ./game_of_life
  ```

---

## Project Structure

```
Game_Of_Life/
├── include/                   # Header files (.h)
│   ├── engine.h               # Abstract SimulationEngine interface & CPU engine
│   ├── engine_cuda.h          # CUDA GPU engine declaration
│   ├── font8x8.h              # Embedded 8x8 font atlas & text renderer
│   ├── gui.h                  # GUI widgets, layout, themes, camera
│   └── patterns.h             # Conway patterns (Glider, Gosper Gun, Pulsar, etc.)
├── src/                       # Source implementation files (.cpp / .cu)
│   ├── engine_cpu.cpp         # OpenMP multi-threaded CPU engine
│   ├── engine_cuda.cu         # CUDA GPU kernels and memory management
│   ├── engine_factory.cpp     # Engine detection & factory instantiation
│   ├── font8x8.cpp            # Font bitmap data and atlas initialization
│   ├── gui.cpp                # Modern dark UI widgets, layout, buttons, camera
│   └── main.cpp               # Master application loop, SDL2 initialization
├── build/                     # Compiled object files (.o) [auto-generated]
├── bin/                       # Output binaries [auto-generated]
│   └── game_of_life           # Compiled executable
├── game_of_life               # Convenience symlink to bin/game_of_life
├── Makefile                   # Multi-target auto-detecting build system
├── .gitignore                 # Git ignore for build artifacts and binaries
└── README.md                  # Comprehensive documentation
```
