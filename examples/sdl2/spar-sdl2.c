#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdint.h>
#include "spar.h"

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "spar SDL2 Example",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800,
        800,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    IMG_Init(IMG_INIT_PNG);
    SDL_Texture *texture = IMG_LoadTexture(renderer, "../assets/shield.png");
    SDLSpriteHandle handle = {renderer, texture};

    ParticleSystem *system = create_particle_system(FLAG_GROW | FLAG_SHRINK);
    system->handle = &handle;

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

    int running = 1;
    SDL_Event event;

    Uint32 target_frame_time = 1000 / 60.0;
    Uint64 last_frame_time = 0, start_frame_time = 0;
    int32_t delta_ms = 0;

    while (running) {
        start_frame_time = SDL_GetTicks64();
        delta_ms = start_frame_time - last_frame_time;
        last_frame_time = start_frame_time;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = 0;
            }
        }

        update_particle_system(system, delta_ms / 1000.0);

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        draw_particle_system(system);

        SDL_RenderPresent(renderer);

        uint64_t frame_time = SDL_GetTicks64() - start_frame_time;
        if (frame_time < target_frame_time) {
            SDL_Delay(target_frame_time - frame_time);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    free_particle_system(system);

    return 0;
}