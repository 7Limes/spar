# spar

A small 2D particle library.

## Usage

To use this library, you must define a `draw_particle` function, which has the following signature:

```c
// Draw a particle at position (x, y) with `rotation` in degrees and a specified `size` with color (r, g, b, a)
// The user may also attach additional data, such as a sprite, via the `handle` argument, which can be assigned to a system.
void draw_particle(
    void *handle,
    float x, float y, float rotation, float size,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a
);
```

If you are using Raylib or SDL2, these functions have already been defined and you do not need to define your own.

## Quick Start (Raylib)

```c
#include <raylib.h>
#include "spar.h"

int main() {
    // Initialize raylib
    InitWindow(800, 800, "particle test");
    SetTargetFPS(60);

    Texture2D tex = LoadTexture("assets/shield.png");

    // Configure particle system
    ParticleSystem *system = create_particle_system(FLAG_GROW);
    system->handle = &tex;  // Assign the particle sprite

    system->position = (ParVec2) {400, 400};
    system->from_size = 0.5;
    system->to_size = 0.0;

    system->from_color = (ParColor) {0, 255, 0, 255};
    system->to_color = (ParColor) {50, 50, 255, 0};
    
    system->emission_speed = 5;
    system->emission_rate = 20;  // 20 particles per second

    system->emission_shape_type = SHAPE_ELLIPSE;
    system->emission_shape.ellipse = (EmissionShapeEllipse) {50, 50};

    system->lifetime = 1.0;

    // Main loop
    while (!WindowShouldClose()) {
        
        update_particle_system(system, GetFrameTime());

        BeginDrawing();

        ClearBackground(RAYWHITE);
        draw_particle_system(system);

        EndDrawing();
    }

    CloseWindow();
    free_particle_system(system);

    return 0;
}
```