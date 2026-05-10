# TRON C-Racer

A fast-paced light cycle arena game built in C with OpenGL 4.6.  
Face up to six AI opponents in a neon-drenched arena, dodge trails, and be the last one standing.

## Features

- **Player Bike** – steer with keyboard, activate a speed burst (Dash), and leave a glowing trail.
- **AI Opponents** – six distinct AI personalities that coordinate, hunt, and defend.
- **Trail Collisions** – realistic trail fading, grace period at spawn, and hitbox-based collision.
- **Dash Mechanic** – temporary 1.5x speed boost with cooldown and visual sparks.
- **Reflections** – blurred screen-space reflections on the semi-transparent arena floor.
- **Dynamic Water** – Gerstner wave ocean surrounding the arena with Fresnel and moon specular.
- **Skybox** – cubemap night sky rotated to match the moon position.
- **Particles** – pillar energy wisps, death explosions, and dash sparks.
- **Multiple Camera Modes** – follow cam (zoomable), top-down orthographic, and free camera.
- **In-Game UI** – main menu, settings (brightness / volume), pause menu, countdown, and game-over screen.
- **GLB Model Support** – optional high-detail bike models loaded from .glb files with PBR materials.

## Controls

| Key | Action |
|-----|--------|
| `A` / `D` | Turn left / right |
| `Left Shift` | Dash (speed burst) |
| `ESC` | Pause / back in menus |
| `F1` | Open controls overlay |
| `F3` / `F4` | Brightness - / + |
| `TAB` | Toggle free camera |
| `WASD` + Mouse | Free camera movement |
| `Space` / `Ctrl` | Free cam up / down |
| `Scroll Wheel` | Zoom follow camera |
| `C` | Toggle top-down view |
| `R` | Quick restart round |
| `W` / `S` or Arrows | Menu navigation |
| `Enter` / `Space` | Confirm menu selection |

## Build

### Requirements

- C11 compiler (GCC, Clang, MSVC)
- CMake 3.16+
- OpenGL 4.6 capable GPU
- Libraries (bundled or system-installed):
  - [GLFW](https://www.glfw.org/) 3.x
  - [cglm](https://github.com/recp/cglm) (OpenGL Mathematics)
  - [glad](https://glad.dav1d.de/) (OpenGL loader)
  - [stb_image](https://github.com/nothings/stb) (image loading, header-only)
  - [cgltf](https://github.com/jkuhlmann/cgltf) (glTF loader, header-only)

## Credits

- GLFW – window and input
- cglm – mathematics library
- glad – OpenGL loader
- stb_image – image loading (Sean Barrett)
- cgltf – glTF 2.0 parser (Johannes Kuhlmann)
- jsmn – JSON tokenizer used by cgltf (Serge Zaitsev)

Game and rendering code written from scratch.
