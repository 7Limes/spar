#include <raylib.h>
#include "spar.h"


int main(void) {
    srand(time(NULL));

    InitWindow(800, 800, "spar Raylib Example");
    SetTargetFPS(60);

    Texture2D tex = LoadTexture("../assets/shield.png");

    ParticleSystem *system = create_particle_system(FLAG_GROW | FLAG_SHRINK);
    system->handle = &tex;

    system->position = (ParVec2) {400, 400};
    system->from_size = 0.1;
    system->to_size = 0.0;

    system->from_color = (ParColor) {69, 95, 247, 255};
    system->to_color = (ParColor) {143, 214, 252, 0};
    
    system->emission_speed = 10;
    system->emission_speed_var = 5;
    system->emission_angle_var = 0.7;
    system->emission_angle = -1.57079632679;
    system->emission_rate = 100;

    system->emission_shape_type = SHAPE_ELLIPSE;
    system->emission_shape.ellipse = (EmissionShapeEllipse) {20, 20};

    system->lifetime = 2.0;
    system->lifetime_var = 0.5;

    system->force = (ParVec2) {0, 0.2};
    system->angular_speed_var = 5.0;

    while (!WindowShouldClose()) {

        update_particle_system(system, GetFrameTime());

        BeginDrawing();

        ClearBackground((Color) {30, 30, 30, 255});
        draw_particle_system(system);

        EndDrawing();
    }

    CloseWindow();

    free_particle_system(system);

    return 0;
}
