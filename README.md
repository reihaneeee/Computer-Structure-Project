# Raylib Pong: C++ vs. x86 SSE Performance Comparison

This project is a classic Pong game built with the [Raylib](https://www.raylib.com/) library. Its primary purpose is not just to be a game, but to serve as a practical experiment to measure and compare the performance of critical game logic written in high-level C++ versus low-level, hand-optimized x86 inline assembly (using SSE instructions).

## 🚀 Features

* **Classic Pong Gameplay:** A two-player Pong game with one player-controlled paddle and one AI-controlled paddle.
* **Dynamic Path Selection:** Press `SPACE` to cycle the ball's trajectory between three different paths:
    * **Path 0:** Straight
    * **Path 1:** Convex (curves upward)
    * **Path 2:** Concave (curves downward)
* **Live Performance Switching:** Switch between physics calculation modes *during gameplay*:
    * **`1` Key:** Use the standard **High-Level C++** functions (`UpdateHL`).
    * **`2` Key:** Use the **x86 Assembly (SSE)** optimized functions (`UpdateASM`).
* **Visual Flair:** The ball is rendered with colorful, rotating stripes that change direction on impact.
* **Real-time Benchmarking:** The game continuously tracks the execution time of the update functions. It prints a detailed performance report to the console every 60 seconds, comparing the average execution time (in microseconds) of the C++ vs. Assembly versions.

## 🔬 The Core Concept: C++ vs. Assembly

The goal of this project was to identify computationally-intensive parts of the game loop and optimize them using assembly. Two key functions were targeted:

1.  **`UpdateBall`**: This function is called every frame to calculate the ball's new `x`, `y` position and its `rotation_angle` based on its speed and the selected trajectory.
2.  **`ClampBallY`**: This function handles the physics for the ball bouncing off the top and bottom walls, reversing its vertical speed.

These functions were implemented in two versions:

* **High-Level (`HL_MODE`):** Written in standard C++, using `float` arithmetic for physics calculations.
* **Assembly (`ASM_MODE`):** Re-written using GCC inline assembly (AT&T syntax) to leverage **SSE (Streaming SIMD Extensions)** instructions. These instructions (`movss`, `cvtsi2ss`, `addss`, `subss`, `ucomiss`) allow for faster, specialized floating-point operations directly on CPU registers.

By switching between modes, we can get a real-world measurement of the performance difference.

## Here are the results
sample run1:
Performance Report (Last 60 seconds):
High-Level Mode:
  UpdateHL Calls: 5087
  Total UpdateHL Time: 0.0141017s
  Average UpdateHL Time/Call: 2.77211 µs
  Total ClampBallY Time: 0.00163769s
  Average ClampBallY Time/Call: 0.321936 µs
Assembly Mode:
  UpdateASM Calls: 303
  Total UpdateASM Time: 0.000232383s
  Average UpdateASM Time/Call: 0.766941 µs
  Total asmClampBallY Time: 6.4586e-05s
  Average asmClampBallY Time/Call: 0.213155 µs

sample run2:
Performance Report (Last 60 seconds):
High-Level Mode:
  UpdateHL Calls: 3036
  Total UpdateHL Time: 0.00518221s
  Average UpdateHL Time/Call: 1.70692 µs
  Total ClampBallY Time: 0.000919205s
  Average ClampBallY Time/Call: 0.302768 µs
Assembly Mode:
  UpdateASM Calls: 2352
  Total UpdateASM Time: 0.00199205s
  Average UpdateASM Time/Call: 0.846962 µs
  Total asmClampBallY Time: 0.000696195s
  Average asmClampBallY Time/Call: 0.296001 µs

## 🎮 Controls

* **`Up Arrow`**: Move your paddle up.
* **`Down Arrow`**: Move your paddle down.
* **`1`**: Switch to **High-Level (C++)** physics.
* **`2`**: Switch to **Assembly (SSE)** physics.
* **`SPACE`**: Cycle through ball path options (0, 1, or 2).

## 🛠️ Building and Running

### Prerequisites

1.  **A C++ Compiler:** This code uses GCC-specific inline assembly. You must use `g++` (Linux/MinGW) or `clang`. It **will not** compile with MSVC (Visual Studio).
2.  **Raylib:** You must have the Raylib library installed on your system.
3.  **An x86-64 CPU:** The assembly code is specific to the x86 instruction set.

### Compile Command (Linux/macOS)

You can compile the project by linking against Raylib. A typical command on a Linux system would be:

```bash
g++ main.cpp -o pong_game -lraylib -lm -ldl -lrt -lX11

(Windows with MinGW)
g++ main.cpp -o pong_game.exe -I<path-to-raylib-include> -L<path-to-raylib-lib> -lraylib -lopengl32 -lgdi32 -lwinmm
