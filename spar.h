/*
    spar.h - A small 2D particle library
    v0.1.0
    
    by Miles Burkart
    https://github.com/7Limes
*/

#include <stdlib.h>
#include <math.h>


#ifndef __SPAR_H
#define __SPAR_H


#define __PAR_CAPACITY 128

#define __PAR_EMISSION_RATE 10

#define __PAR_EMISSION_ANGLE 0
#define __PAR_EMISSION_SPEED 1

#define __PAR_LIFETIME 0.5

#define __PAR_SIZE 1


#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif


typedef struct ParVec2 {
    float x, y;
} ParVec2;

typedef struct ParColor {
    unsigned char r, g, b, a;
} ParColor;


typedef struct Particle {
    ParVec2 position, velocity;
    float angular_speed;
    float rotation;
    float lifetime;
} Particle;


typedef struct EmissionShapeRect {
    float width, height;
} EmissionShapeRect;

typedef struct EmissionShapeEllipse {
    float width, height;
} EmissionShapeEllipse;

typedef struct EmissionShapeRing {
    float width, height, thickness;
} EmissionShapeRing;

typedef struct EmissionShapeLine {
    ParVec2 start, end;
    float thickness;
} EmissionShapeLine;

typedef union EmissionShape {
    EmissionShapeRect rect;
    EmissionShapeEllipse ellipse;
    EmissionShapeRing ring;
    EmissionShapeLine line;
} EmissionShape;

typedef enum EmissionShapeType {
    SHAPE_RECT,     // A filled rectangle
    SHAPE_ELLIPSE,  // A filled ellipse
    SHAPE_RING,     // A hollow ellipse
    SHAPE_LINE      // A line with a thickness
} EmissionShapeType;

typedef enum ParticleSystemFlags {
    FLAG_GROW = 1 << 0,    // Increase particle array capacity if necessary
    FLAG_SHRINK = 1 << 1   // Decrease particle array capacity if possible
} ParticleSystemFlags;

typedef float (*ParEasingFunction)(float);


// A system of particles
typedef struct ParticleSystem {
    Particle *_particles;           // The array of particles
    size_t count, capacity;        // The length and capacity of the particle array

    int flags;                     // Particle system flags

    float emission_rate;           // The number of particles to create per second
    float _accumulated_time;       // Time accumulator for spawning particles

    int emitting;                              // Whether the system is currently emitting
    EmissionShapeType emission_shape_type;     // The type of emission shape
    EmissionShape emission_shape;              // Emission shape data
    float emission_angle, emission_angle_var;  // The angle to emit towards
    float emission_speed, emission_speed_var;  // The initial speed to apply to each particle

    float lifetime, lifetime_var;  // The lifetime of each particle in seconds

    ParVec2 position;  // The position offset to apply to the effect
    ParVec2 force;     // The constant force to apply to each particle
    float drag;        // The force to apply against each particle's motion

    float angular_speed, angular_speed_var;  // The initial angular speed to apply to each particle

    float from_size, to_size;     // The particle sizes
    ParEasingFunction size_func;  // The easing function to apply to size

    ParColor from_color, to_color;    // The particle colors
    ParEasingFunction color_func;     // The easing function to apply to color 

    void *handle;  // Opaque pointer to allow user to attach data, such as a sprite

} ParticleSystem;


// Returns a random float in the range `[0, 1)`
static inline float __rand_float() {
    int r = 0x3F800000 | (rand() & 0x007FFFFF);
    float f = *(float*) &r;
    return f - 1;
}


// Returns a random float in the range `[lower, upper)`
static inline float __rand_float_range(float min, float max) {
    return min + (__rand_float() * (max - min));
}


// Returns a random float in the range `(base-var/2, base+var/2)`
static inline float __vary_float(float base, float var) {
    float rvar = (__rand_float() - 0.5) * var;  // Rand value in [-var/2, var/2)
    return base + rvar;
}


// Clamps `value` between `min` and `max`
static inline float __clamp_float(float value, float min, float max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}


// Linearly interpolates between `from` and `to`
static inline float __lerp(float t, float from, float to) {
    return (1-t) * from + t*to; 
}


// Moves `v` towards `target` by `amount`
// From https://github.com/raysan5/raylib/blob/master/src/raymath.h
static inline ParVec2 __vec2_move_towards(ParVec2 v, ParVec2 target, float amount) {
    ParVec2 result = {0};

    float dx = target.x - v.x;
    float dy = target.y - v.y;
    float value = (dx*dx) + (dy*dy);

    if ((value == 0) || ((amount >= 0) && (value <= amount*amount))) {
        return target;
    }

    float dist = sqrtf(value);

    return (ParVec2) {
        v.x + dx/dist*amount,
        v.y + dy/dist*amount
    };
}


// Applies `func` to `value` or returns `value` if `func` is NULL
static inline float __apply_func(ParEasingFunction func, float value) {
    if (func == NULL) {
        return value;
    }
    return func(value);
}


// Returns the position of a particle based on its emission shape and base position
static inline ParVec2 __get_particle_position(EmissionShapeType shape_type, EmissionShape shape, ParVec2 base_pos) {
    ParVec2 pos = {0, 0};

    float angle, scale;
    switch (shape_type) {
        case SHAPE_RECT:
            EmissionShapeRect rect = shape.rect;
            pos.x = __vary_float(0, rect.width);
            pos.y = __vary_float(0, rect.height);
            break;
        
        case SHAPE_ELLIPSE:
            EmissionShapeEllipse ellipse = shape.ellipse;
            angle = __rand_float() * 2 * M_PI;
            scale = sqrt(__rand_float());
            pos.x = ellipse.width / 2 * scale * cos(angle);
            pos.y = ellipse.height / 2 * scale * sin(angle);
            break;
        
        case SHAPE_RING:
            EmissionShapeRing ring = shape.ring;
            angle = __rand_float() * 2 * M_PI;
            if (ring.thickness < 0) {
                scale = 1;
            }
            else {
                scale = __rand_float_range(1 - ring.thickness/2, 1 + ring.thickness/2);
            }
            pos.x = ring.width / 2 * scale * cos(angle);
            pos.y = ring.height / 2 * scale * sin(angle);
            break;
        
        case SHAPE_LINE:
            EmissionShapeLine line = shape.line;
            float slope = (line.end.y - line.start.y) / (line.end.x - line.start.x);
            float intercept = line.start.y - slope * line.start.x;
            pos.x = (line.end.x - line.start.x) * __rand_float() + line.start.x;
            pos.y = slope * pos.x + intercept;

            if (line.thickness > 0) {
                float invslope = -1 / slope;
                float invintercept = pos.y - invslope * pos.x;
                float pline_dx = cos(atanf(invslope)) * line.thickness / 2;
                float pline_x1 = pos.x - pline_dx;
                float pline_x2 = pos.x + pline_dx;
                pos.x = (pline_x2 - pline_x1) * __rand_float() + pline_x1;
                pos.y = invslope * pos.x + invintercept;
            }

            break;
    }

    pos.x += base_pos.x;
    pos.y += base_pos.y;

    return pos;
}


// Resizes a system's `particles` array
static inline void __resize_array(ParticleSystem *system, size_t new_capacity) {
    system->capacity = new_capacity;
    Particle *new_array = calloc(system->capacity, sizeof(Particle));

    for (size_t i = 0; i < system->count; i++) {
        new_array[i] = system->_particles[i];
    }

    free(system->_particles);
    system->_particles = new_array;
}


// Adds a new particle to `system`
static inline void __add_particle(ParticleSystem *system) {
    if (system->count >= system->capacity) {
        if (system->flags & FLAG_GROW) {
            __resize_array(system, system->capacity * 2);
        }
        else {
            return;
        }
    }

    Particle *par = &system->_particles[system->count];

    par->position = __get_particle_position(system->emission_shape_type, system->emission_shape, system->position);

    float angle = __vary_float(system->emission_angle, system->emission_angle_var);
    float speed = __vary_float(system->emission_speed, system->emission_speed_var);
    par->velocity.x = cos(angle) * speed;
    par->velocity.y = sin(angle) * speed;

    par->angular_speed = __vary_float(system->angular_speed, system->angular_speed_var);

    par->lifetime = __vary_float(system->lifetime, system->lifetime_var);

    system->count++;
}


static inline int __update_particle(ParticleSystem *system, Particle *particle, float delta) {
    particle->lifetime -= delta;

    particle->velocity.x += system->force.x;
    particle->velocity.y += system->force.y;

    if (system->drag != 0) {
        particle->velocity = __vec2_move_towards(particle->velocity, (ParVec2) {0, 0}, system->drag);
    }

    particle->position.x += particle->velocity.x;
    particle->position.y += particle->velocity.y;

    particle->rotation += particle->angular_speed;

    return particle->lifetime < 0;
}


// Allocates and returns a new particle system
ParticleSystem *create_particle_system(int flags) {
    ParticleSystem *system = calloc(1, sizeof(ParticleSystem));
    system->flags = flags;

    system->count = 0;
    system->capacity = __PAR_CAPACITY;
    system->_particles = calloc(system->capacity, sizeof(Particle));

    system->emitting = 1;
    system->emission_rate = __PAR_EMISSION_RATE;

    system->emission_angle = __PAR_EMISSION_ANGLE;
    system->emission_speed = __PAR_EMISSION_SPEED;

    system->lifetime = __PAR_LIFETIME;
    system->lifetime_var = 0;

    system->from_size = __PAR_SIZE;
    system->to_size = __PAR_SIZE;

    system->from_color = (ParColor) {255, 255, 255, 255};
    system->to_color = (ParColor) {255, 255, 255, 255};

    return system;
}


// Frees an existing particle system
void free_particle_system(ParticleSystem *system) {
    free(system->_particles);
    free(system);
}


// Updates a particle system
void update_particle_system(ParticleSystem *system, float delta) {

    if (system->emitting) {
        // Spawn new particles
        system->_accumulated_time += delta;
        size_t particles_to_spawn = system->_accumulated_time * system->emission_rate;
        if (particles_to_spawn > 0) {
            for (size_t i = 0; i < particles_to_spawn; i++) {
                __add_particle(system);
            }
            system->_accumulated_time -= (float) particles_to_spawn / system->emission_rate;
        }
    }

    // Update particles
    for (size_t i = 0; i < system->count; i++) {
        int should_delete = __update_particle(system, &system->_particles[i], delta);
        if (should_delete) {
            // Swap and pop
            system->_particles[i] = system->_particles[system->count-1];
            system->count--;
            i--;

            if (system->flags & FLAG_SHRINK && system->count < system->capacity / 2) {
                __resize_array(system, system->capacity / 2);
            }
        }
    }
}


// Draws a single particle.
// This function is to be defined by the user
void draw_particle(
    void *handle,
    float x, float y, float rotation, float size,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a
);


// Draws a particle system
void draw_particle_system(ParticleSystem *system) {
    for (size_t i = 0; i < system->count; i++) {
        Particle *par = &system->_particles[i];
        float t = 1 - __clamp_float(par->lifetime / system->lifetime, 0, 1);
        float t_size = __apply_func(system->size_func, t);
        float t_color = __apply_func(system->color_func, t);

        float size = __lerp(t_size, system->from_size, system->to_size);

        ParColor color = {
            (1 - t_color)*system->from_color.r + t_color*system->to_color.r,
            (1 - t_color)*system->from_color.g + t_color*system->to_color.g,
            (1 - t_color)*system->from_color.b + t_color*system->to_color.b,
            (1 - t_color)*system->from_color.a + t_color*system->to_color.a,
        };
        
        draw_particle(
            system->handle,
            par->position.x, par->position.y, par->rotation, size,
            color.r, color.g, color.b, color.a
        );
    }
}

// Builtin draw_particle definitions for common libraries
#ifndef SPAR_OVERRIDE_BUILTINS

// Raylib
// `handle`: Texture2D
#ifdef RAYLIB_H
void draw_particle(
    void *handle,
    float x, float y, float rotation, float size,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a
) {
    Texture2D *sprite = handle;
    Rectangle source = {0, 0, sprite->width, sprite->height};
    Rectangle dest = {x, y, source.width*size, source.height*size};
    Vector2 origin = {source.width / 2 * size, source.height / 2 * size};
    Color color = {r, g, b, a};
    DrawTexturePro(*sprite, source, dest, origin, rotation, color);
}
#endif

// SDL2
// `handle`: SDLSpriteHandle
#ifdef SDL_h_

typedef struct {
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
} SDLSpriteHandle;

void draw_particle(
    void *handle,
    float x, float y, float rotation, float size,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a
) {
    SDLSpriteHandle *sprite = handle;

    int tex_w, tex_h;
    SDL_QueryTexture(sprite->texture, NULL, NULL, &tex_w, &tex_h);

    SDL_Rect source = {0, 0, tex_w, tex_h};
    SDL_FRect dest = {
        x - (tex_w * size) / 2.0f,
        y - (tex_h * size) / 2.0f,
        tex_w * size,
        tex_h * size
    };
    SDL_FPoint origin = {(tex_w * size) / 2.0f, (tex_h * size) / 2.0f};

    SDL_SetTextureColorMod(sprite->texture, r, g, b);
    SDL_SetTextureAlphaMod(sprite->texture, a);
    SDL_RenderCopyExF(sprite->renderer, sprite->texture, &source, &dest, rotation, &origin, SDL_FLIP_NONE);
}
#endif

#endif  // End builtins
#endif  // End header
