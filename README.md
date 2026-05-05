# prizm_racing
A 3D, multiplayer racing game for Casio fx-CG 10/20/50 calculators (Prizm series), also playable on PC.

![Language](https://img.shields.io/badge/Language-C++-blue.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

## Pictures
![Video](resources/video.gif)  
*Now with textures since version 1.4:*
![Newer graphics](resources/picture1.png)

## Features
- **3D Graphics:** Real-time 3D rendering with a custom rasterizer.
- **Multiplayer:** Compete against a friend by connecting two calculators via the 3-pin cable.
- **Game Modes:** Free Roam, Multiplayer Race, and Time Trial.
- **Minimap:** Real-time track overview and player positions.
- **Cross-platform:** Runs on Casio Prizm (sh3/sh4), gint-compatible calculators, and PC (via SDL2).
- **Performance:** Runs at approximately 18-20 FPS on original hardware.

## Game Modes
- **Free Roam:** The default mode. Explore the track and practice your driving.
- **Multiplayer Race:** Press `F1` to request a race. Once both players accept, a 5-lap race begins.
- **Time Trial:** Press `F2` to start a solo timed race. Includes session best records and lap timing.

## Controls

### Casio Prizm / gint
| Action | Key(s) |
| :--- | :--- |
| **Accelerate** | `Up` or `8` |
| **Brake / Reverse** | `Down` or `5` |
| **Turn Left** | `Left` or `4` |
| **Turn Right** | `Right` or `6` |
| **Start Multi Race** | `F1` |
| **Start Time Trial** | `F2` |
| **Return to Menu** | `EXE` (after race) |
| **Exit Game** | `MENU` |

### PC (SDL2)
| Action | Key |
| :--- | :--- |
| **Accelerate** | `Up Arrow` |
| **Brake / Reverse** | `Down Arrow` |
| **Turn Left** | `Left Arrow` |
| **Turn Right** | `Right Arrow` |
| **Start Multi Race** | `F1` |
| **Start Time Trial** | `F2` |
| **Return to Menu** | `Space` |
| **Exit Game** | `Tab` or `Esc` |

**Tip**: If you use `left`/`right` to turn and `8` to accelerate on the calculator, you can turn more comfortably while accelerating.

## How to install
1. Download the latest release from the [releases page](https://github.com/Duarte_Coelho/prizm_racing/releases).
2. Extract the `.g3a` file.
3. Connect your calculator to your PC via USB and select `USB Flash` mode (`F1`).
4. Copy the `.g3a` file to the calculator's storage.

## How to build

### 1. Prizm SDK version (with multiplayer support)
#### Linux
- Set up the [PrizmSDK](https://prizm.cemetech.net/Tutorials/PrizmSDK_Setup_Guide/).
- Set the `FXCGSDK` environment variable to your installation path.
- Run `make prizm`.

#### Windows
- Download the [Prizm SDK](https://github.com/Jonimoose/libfxcg/releases).
- Ensure the path has no spaces.
- Place the project in `PrizmSDK/projects/prizm_racing`.
- Run `..\..\bin\make.exe prizm`.

### 2. gint version
This version uses the modern [gint](https://gitea.planet-casio.com/Lephenixnoir/gint) library.
- Install `gint` and `libprof`.
- Run `make gint`.

### 3. PC version (SDL2)
Useful for testing or playing without a calculator.
- Install SDL2 development libraries (`libsdl2-dev` on Debian/Ubuntu).
- Run `make sdl`.
- The executable will be generated at `sdl/racing`.

## Technical information

### 3D Rendering
- **Custom Rasterizer:** Located in `src/rasterizer.cpp` and `src/drawTriangle.h`.
- **Clipping:** Triangles are clipped against the screen boundaries to prevent rendering errors.
- **Fixed-Point Math:** Since the calculator lacks an FPU, all calculations use fixed-point arithmetic (`src/fp.h`) for performance.
- **Optimization:** Frustum culling is used to skip objects far from the camera.
- **Rasterization:** Uses a scan-line algorithm.
    - **Flat Shading:** One color per triangle to maintain FPS.
    - **Texturing:** Linear approximation of texture coordinates (non-perspective correct, optimized for the calculator's small screen).

### Multiplayer
- **Communication:** Uses the 3-pin serial port (`src/main.cpp`).
- **Protocol:** A simple custom byte-stream protocol ensures synchronization between devices.
- **Lag:** The game keeps multiplayer simple by sending raw position data. Minor "ghosting" may occur where opponents appear slightly behind their actual position.

## Notes/Bugs
- **Framerate dependency:** Car physics are partially linked to framerate. Performance may vary slightly if overclocking.
- **Connection Tip:** Connect the 3-pin cable only while the game is running on both calculators to avoid the OS attempting to enter file transfer mode.

## Links
- [Cemetech Forum Post](https://www.cemetech.net/forum/viewtopic.php?t=18915)
- [Cemetech Archive](http://ceme.tech/DL2319)

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
