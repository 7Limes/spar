// spargui v1.0.0 - A simple GUI app for creating effects with spar.
// June 15 2026
// by Miles Burkart

#include <stdlib.h>
#include <raylib.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include "../../spar.h"
#include "da.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "raymath.h"
#include "tinyfiledialogs.h"


#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))


typedef void (*Callback)(void);

typedef enum {
    RESIZE_NONE,
    RESIZE_LEFT,
    RESIZE_RIGHT
} ResizingUI;

typedef enum {
    EDIT_NONE,
    EDIT_WIDTH,
    EDIT_HEIGHT,
    EDIT_P1,
    EDIT_P2
} ShapeEditState;

typedef enum {
    EVENT_NONE,
    EVENT_PRESSED,
    EVENT_HOVERED
} ButtonEvent;

typedef struct {
    Texture2D texture;
    char path[256];
} NamedTexture;

typedef struct {
    ParticleSystem *system;
    char name[64];
} SystemContainer;

typedef struct {
    const char* name;
    Callback func;
} MenubarEntry;

typedef struct {
    Rectangle top, bottom, left, right;
} RectangleEdges;


const char *SPRITES_DIRECTORY = "sprites";
const char *EFFECT_FILE_TYPES[] = {"*.spef"};
const char *IMAGE_FILE_TYPES[] = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif"};
const uint32_t FILE_SIGNATURE = 0x72617073;  // "spar"

const char *EMISSION_SHAPES = "Rectangle;Ellipse;Ring;Line";

// UI constants
const float FLOAT_ENTRY_INCREMENT = 0.1F;

const int PANEL_TITLE_HEIGHT = 23;
const int PANEL_MIN_SIZE = 100;

const int RESIZE_PADDING = 5;
const int SMALL_PADDING = 5;

const int CIRCLE_GIZMO_SIZE = 10;

const int MENUBAR_HEIGHT = 20;
const int MENUBAR_BUTTON_WIDTH = 70;

const int EFFECT_BUTTON_HEIGHT = 60;

const int SPRITES_PANEL_Y = 65;
const int SPRITES_PANEL_HEIGHT = 175;
const int SPRITE_SQUARE_SIZE = 50;

const int EMISSION_PANEL_Y = SPRITES_PANEL_Y+SPRITES_PANEL_HEIGHT;
const int EMISSION_PANEL_HEIGHT = PANEL_TITLE_HEIGHT+150;

const int LIFETIME_PANEL_Y = EMISSION_PANEL_Y+EMISSION_PANEL_HEIGHT;
const int LIFETIME_PANEL_HEIGHT = PANEL_TITLE_HEIGHT+20;

const int SIZE_PANEL_Y = LIFETIME_PANEL_Y+LIFETIME_PANEL_HEIGHT;
const int SIZE_PANEL_HEIGHT = PANEL_TITLE_HEIGHT+20;

const int ANGULAR_SPEED_PANEL_Y = SIZE_PANEL_Y+SIZE_PANEL_HEIGHT;
const int ANGULAR_SPEED_PANEL_HEIGHT = PANEL_TITLE_HEIGHT+20;

const int COLOR_PANEL_Y = ANGULAR_SPEED_PANEL_Y+ANGULAR_SPEED_PANEL_HEIGHT;
const int COLOR_PANEL_HEIGHT = PANEL_TITLE_HEIGHT+85;

const int FORCE_PANEL_Y = COLOR_PANEL_Y+COLOR_PANEL_HEIGHT;
const int FORCE_PANEL_HEIGHT = PANEL_TITLE_HEIGHT+50;

const int STATS_PANEL_WIDTH = 150;
const int STATS_PANEL_HEIGHT = 100;
const float STATS_PANEL_ANIM_DURATION = 0.75F;

// Color constants

const Color COL_SELECTED = {100, 100, 255, 128};
const Color COL_PREVIEW = {150, 220, 250, 128};
const Color COL_HOVERED =  {0, 255, 0, 128};

// Particle constants
const float MAX_EMISSION_RATE = 100000;

// Menubar functions
void export_button_callback();
void import_button_callback();

const MenubarEntry MENUBAR_ENTRIES[] = {
    {"Export", export_button_callback},
    {"Import", import_button_callback}
};
const size_t MENUBAR_ENTRY_COUNT = sizeof(MENUBAR_ENTRIES) / sizeof(MenubarEntry);


// UI globals
int left_panel_width = 150;
int right_panel_width = 250;
ResizingUI current_resizing_ui = RESIZE_NONE;
ShapeEditState emission_shape_edit_state = EDIT_NONE;

int selected_system_index = -1;
int hovered_system_index = -1;

char tooltip[128];

int focused_text_box = -1;
int tab_focused_text_box = -1;  // Used for tab navigation
int text_box_counter = 0;
bool text_box_was_clicked = false;
char text_box_buffer[64];

// Values for animating the stats panel
float stats_panel_anim_timer = 0;
float stats_panel_anim_dest = 0;

// App globals
DynamicArray system_containers;
DynamicArray sprite_textures;
double systems_update_time = 0, systems_draw_time = 0;


int write_u16_le(FILE *f, uint16_t value) {
    uint8_t bytes[2];
    bytes[0] = (uint8_t)(value & 0xFF);
    bytes[1] = (uint8_t)((value >> 8) & 0xFF);
    return fwrite(bytes, 1, 2, f) == 2 ? 0 : -1;
}

int write_u32_le(FILE *f, uint32_t value) {
    uint8_t bytes[4];
    bytes[0] = (uint8_t)(value & 0xFF);
    bytes[1] = (uint8_t)((value >> 8)  & 0xFF);
    bytes[2] = (uint8_t)((value >> 16) & 0xFF);
    bytes[3] = (uint8_t)((value >> 24) & 0xFF);
    return fwrite(bytes, 1, 4, f) == 4 ? 0 : -1;
}

int write_f32_le(FILE *f, float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    return write_u32_le(f, bits);
}

uint8_t next_u8(uint8_t **buf) {
    return *(*buf)++;
}

uint16_t next_u16_le(uint8_t **buf) {
    uint16_t result =  (uint16_t)(*buf)[0] |
                       (uint16_t)(*buf)[1] << 8;
    *buf += sizeof(uint16_t);
    return result;
}

uint32_t next_u32_le(uint8_t **buf) {
    uint32_t result =  (uint32_t)(*buf)[0]       |
                       (uint32_t)(*buf)[1] << 8  |
                       (uint32_t)(*buf)[2] << 16 |
                       (uint32_t)(*buf)[3] << 24;
    *buf += sizeof(uint32_t);
    return result;
}

float next_f32_le(uint8_t **buf) {
    uint32_t bits = next_u32_le(buf);
    float value;
    memcpy(&value, &bits, sizeof(float));
    return value;
}


int set_bitflag(int flags, int flag, int value) {
    return value ? (flags | flag) : (flags & ~flag);
}


float float_move_towards(float current, float target, float delta) {
    if (fabsf(target - current) <= delta) return target;
    return current + (target > current ? delta : -delta);
}


// Loads a new sprites and adds it to the `sprite_textures` array.
// If the sprite is already loaded or was successfully added, returns its index.
int add_sprite(const char *sprite_path) {
    struct stat s;
    int stat_result = stat(sprite_path, &s);
    if (stat_result) {
        // Failed to get info
        return -1;
    }
    if (!(s.st_mode & __S_IFREG)) {
        // Not a file
        return -2;
    }

    // Check if sprite is already loaded
    for (int i = 0; i < sprite_textures.size; i++) {
        NamedTexture *check_tex = da_get(&sprite_textures, i);
        if (strcmp(check_tex->path, sprite_path) == 0) {
            return i;
        }
    }
    
    // Create and append the texture
    NamedTexture *tex = malloc(sizeof(NamedTexture));
    strncpy(tex->path, sprite_path, sizeof(tex->path));
    tex->texture = LoadTexture(tex->path);
    
    da_append(&sprite_textures, tex);

    return sprite_textures.size-1;
}


int load_sprites_from_directory() {
    struct dirent *entry;

    DIR *dir = opendir(SPRITES_DIRECTORY);
    if (dir == NULL) {
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[256];
        snprintf(path, 256, "%s/%s", SPRITES_DIRECTORY, entry->d_name);
        realpath(path, path);  // Convert to real path;

        add_sprite(path);
    }

    return 0;
}


void format_float(char *buf, size_t size, float val) {
    snprintf(buf, size, "%.3f", val);

    char *dot = strchr(buf, '.');
    if (dot) {
        char *end = buf + strlen(buf) - 1;
        // Trim trailing zeros
        while (end > dot && *end == '0') {
            *end-- = '\0';
        }
        if (end == dot) {
            *dot = '\0';
        }
    }
}


// Tooltip functions
void clear_tooltip() {
    tooltip[0] = '\0';
}

void show_tooltip(const char *text) {
    if (text == NULL) {
        clear_tooltip();
    }
    else {
        snprintf(tooltip, sizeof(tooltip), "%s", text);
    }
}


// `GuiButton` that returns a different code depending on the event
ButtonEvent Button(Rectangle bounds, const char *text) {
    if (GuiButton(bounds, text)) {
        return EVENT_PRESSED;
    }
    if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
        return EVENT_HOVERED;
    }
    return EVENT_NONE;
}

// `GuiButton` that shows a tooltip when hovered
int TooltipButton(Rectangle bounds, const char *text, const char *tooltip_text) {
    ButtonEvent event = Button(bounds, text);
    if (event == EVENT_HOVERED) {
        show_tooltip(tooltip_text);
    }
    return event == EVENT_PRESSED;
}


// Helper for handling focused text boxes
void TextBox(Rectangle bounds, char *text, int max_length) {
    if (CheckCollisionPointRec(GetMousePosition(), bounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        focused_text_box = text_box_counter;
        text_box_was_clicked = true;
        strcpy(text_box_buffer, text);
    }
    else if (tab_focused_text_box != -1) {
        focused_text_box = tab_focused_text_box;
    }

    if (focused_text_box == text_box_counter) {
        if (tab_focused_text_box == text_box_counter) {
            tab_focused_text_box = -1;
            strcpy(text_box_buffer, text);
        }
        // Use text_box_buffer when focused
        bool enter_pressed = GuiTextBox(bounds, text_box_buffer, max_length, focused_text_box == text_box_counter);
        strcpy(text, text_box_buffer);
        if (enter_pressed) {
            focused_text_box = -1;
        }
    }
    else {
        GuiTextBox(bounds, text, max_length, focused_text_box == text_box_counter);
    }

    text_box_counter++;
}


// Float text entry
void FloatEntry(Rectangle bounds, float *value) {
    static char buffer[16];
    format_float(buffer, sizeof(buffer)-1, *value);
    
    TextBox(bounds, buffer, sizeof(buffer)-1);
    *value = atof(buffer);

    // Increase / decrease with scroll
    if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
        float mouse_wheel = GetMouseWheelMove();
        if (mouse_wheel < 0) {
            *value -= FLOAT_ENTRY_INCREMENT;
        }
        else if (mouse_wheel > 0) {
            *value += FLOAT_ENTRY_INCREMENT;
        }
    }
}


// GuiLabel wrapper with printf formatting support
void GuiLabelF(Rectangle bounds, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char text[256];
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    GuiLabel(bounds, text);
}


float ease_in_out(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}


// Calls `BeginScissorMode` using a Rectangle struct
void BeginScissorRect(Rectangle rect) {
    BeginScissorMode(rect.x, rect.y, rect.width, rect.height);
}


RectangleEdges get_rectangle_edges_with_padding(Rectangle rect, float padding) {
    return (RectangleEdges) {
        .top = (Rectangle) {rect.x, rect.y-padding, rect.width, padding*2},
        .bottom = (Rectangle) {rect.x, rect.y+rect.height-padding, rect.width, padding*2},
        .left = (Rectangle) {rect.x-padding, rect.y, padding*2, rect.height},
        .right = (Rectangle) {rect.x+rect.width-padding, rect.y, padding*2, rect.height}
    };
}


SystemContainer* add_system_container() {
    SystemContainer *container = malloc(sizeof(SystemContainer));
    strcpy(container->name, "Effect");

    ParticleSystem *system = create_particle_system(FLAG_GROW | FLAG_SHRINK);
    system->position = (ParVec2) {GetScreenWidth()/2, GetScreenHeight()/2};
    container->system = system;
    da_append(&system_containers, container);

    return container;
}


void delete_system_container(size_t index) {
    SystemContainer *container = da_get(&system_containers, index);
    free_particle_system(container->system);
    free(container);
    da_pop(&system_containers, index);
}


void copy_system_container(size_t index) {
    SystemContainer *container = da_get(&system_containers, index);
    SystemContainer *copied_container = add_system_container();

    strncpy(copied_container->name, container->name, sizeof(copied_container->name));
    Particle *saved_particle_array = copied_container->system->_particles;
    size_t saved_capacity = copied_container->system->capacity;
    
    *copied_container->system = *container->system;  // Copy struct
    copied_container->system->_particles = saved_particle_array;
    copied_container->system->count = 0;
    copied_container->system->capacity = saved_capacity;
}


ParticleSystem *get_selected_system() {
    return ((SystemContainer*) da_get(&system_containers, selected_system_index))->system;
}


void add_sprite_button_callback() {
    const char *file_path = tinyfd_openFileDialog("Import Effect", "", sizeof(IMAGE_FILE_TYPES)/sizeof(char*), IMAGE_FILE_TYPES, "Images", false);
    if (!file_path) {
        return;
    }

    add_sprite(file_path);
}


void draw_menubar() {
    int half_width = left_panel_width/2;
    for (size_t i = 0; i < MENUBAR_ENTRY_COUNT; i++) {
        MenubarEntry entry = MENUBAR_ENTRIES[i];
        if (GuiButton((Rectangle) {i*half_width, 0, half_width, MENUBAR_HEIGHT}, entry.name)) {
            entry.func();
        }
    }
}


void draw_left_panel() {
    int height = GetScreenHeight();
    int half_width = left_panel_width/2;
    int quarter_width = left_panel_width/4;
    int half_button_height = EFFECT_BUTTON_HEIGHT/2;

    static Vector2 scroll;
    Rectangle view = {0};
    Rectangle content_view = {0, PANEL_TITLE_HEIGHT, left_panel_width-2, system_containers.size*EFFECT_BUTTON_HEIGHT};

    Rectangle effects_rect = {0, MENUBAR_HEIGHT, left_panel_width, height-40};

    // Left panel
    GuiScrollPanel(effects_rect, "Effects", content_view, &scroll, &view);
    if (GuiButton((Rectangle) {0, height-40, left_panel_width, 40}, "New Effect")) {
        add_system_container();
    }

    BeginScissorRect(view);

    hovered_system_index = -1;
    bool system_deleted_this_frame = false;

    for (int i = 0; i < system_containers.size; i++) {
        SystemContainer *container = da_get(&system_containers, i);

        int saved_color = GuiGetStyle(BUTTON, BASE_COLOR_NORMAL);
        if (i == selected_system_index) {
            GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, GuiGetStyle(BUTTON, BASE_COLOR_PRESSED));
        }
        
        float button_y = i * EFFECT_BUTTON_HEIGHT + PANEL_TITLE_HEIGHT + MENUBAR_HEIGHT + scroll.y;
        Rectangle effect_button_rect = {scroll.x, button_y, half_width, EFFECT_BUTTON_HEIGHT};
        ButtonEvent effect_event = Button(effect_button_rect, container->name);
        if (effect_event == EVENT_PRESSED) {
            if (selected_system_index == i) {
                selected_system_index = -1;
            }
            else {
                selected_system_index = i;
            }
        }
        else if (effect_event == EVENT_HOVERED) {
            hovered_system_index = i;
        }

        GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, saved_color);

        // Copy button
        if (TooltipButton((Rectangle) {half_width+scroll.x, button_y, quarter_width, half_button_height}, "#16#", "Copy")) {
            copy_system_container(i);
        }

        // Delete button
        if (TooltipButton((Rectangle) {half_width+scroll.x, button_y+half_button_height, quarter_width, half_button_height}, "#9#", "Delete")) {
            if (!system_deleted_this_frame) {
                selected_system_index = -1;
                delete_system_container(i);
                i--;
                system_deleted_this_frame = true;

            }
        }
        
        // Move up button
        if (TooltipButton((Rectangle) {quarter_width*3+scroll.x, button_y, quarter_width, half_button_height}, "#117#", "Move up")) {
            if (i > 0) {
                void *above = da_get(&system_containers, i-1);
                da_set(&system_containers, i, above);
                da_set(&system_containers, i-1, container);
                selected_system_index = -1;
            }
        }

        // Move down button
        if (TooltipButton((Rectangle) {quarter_width*3+scroll.x, button_y+half_button_height, quarter_width, half_button_height}, "#116#", "Move down")) {
            if (i < system_containers.size-1) {
                void *below = da_get(&system_containers, i+1);
                da_set(&system_containers, i, below);
                da_set(&system_containers, i+1, container);
                selected_system_index = -1;
            }
        }
    }
    EndScissorMode();
}


void draw_details_panel(int right_panel_x, SystemContainer *container) {
    char str_buffer[64];
    int quarter_width = right_panel_width/4;

    GuiLabel((Rectangle) {right_panel_x+5, PANEL_TITLE_HEIGHT, quarter_width, 20}, "Name:");
    strncpy(str_buffer, container->name, 64);
    TextBox((Rectangle) {right_panel_x+quarter_width, PANEL_TITLE_HEIGHT, quarter_width*3, 20}, str_buffer, sizeof(str_buffer));

    bool ordered_remove_enabled = container->system->flags & FLAG_ORDERED;
    GuiCheckBox((Rectangle) {right_panel_x+SMALL_PADDING, PANEL_TITLE_HEIGHT+21, 20, 20}, "Ordered Remove", &ordered_remove_enabled);
    container->system->flags = set_bitflag(container->system->flags, FLAG_ORDERED, ordered_remove_enabled);

    bool reverse_draw_enabled = container->system->flags & FLAG_REVERSE_DRAW;
    GuiCheckBox((Rectangle) {right_panel_x+quarter_width*2+SMALL_PADDING, PANEL_TITLE_HEIGHT+21, 20, 20}, "Reverse Draw", &reverse_draw_enabled);
    container->system->flags = set_bitflag(container->system->flags, FLAG_REVERSE_DRAW, reverse_draw_enabled);

    if (strlen(str_buffer) == 0) {
        strncpy(container->name, "Effect", 64);
    }
    else {
        strncpy(container->name, str_buffer, 64);
    }
}


void draw_sprites_panel(int right_panel_x) {
    static Vector2 scroll;
    Rectangle view = {0};
    int quarter_width = right_panel_width/4;

    int sprites_per_row = right_panel_width / SPRITE_SQUARE_SIZE;
    int sprite_rows = ceilf((float) sprite_textures.size / sprites_per_row);

    Rectangle content_view = {right_panel_x, SPRITES_PANEL_Y+PANEL_TITLE_HEIGHT, right_panel_width, sprite_rows*SPRITE_SQUARE_SIZE};
    GuiScrollPanel((Rectangle) {right_panel_x, SPRITES_PANEL_Y, right_panel_width, SPRITES_PANEL_HEIGHT}, "Sprite", content_view, &scroll, &view);

    if (GuiButton((Rectangle) {right_panel_x+quarter_width*3, SPRITES_PANEL_Y+2, quarter_width, 20}, "Add Sprite")) {
        add_sprite_button_callback();
    }

    BeginScissorRect(view);
    int saved_border_width = GuiGetStyle(BUTTON, BORDER_WIDTH);
    GuiSetStyle(BUTTON, BORDER_WIDTH, 0);

    for (size_t i = 0; i < sprite_textures.size; i++) {
        int square_x = i % sprites_per_row * SPRITE_SQUARE_SIZE + right_panel_x + scroll.x;
        int square_y = i / sprites_per_row * SPRITE_SQUARE_SIZE + SPRITES_PANEL_Y+PANEL_TITLE_HEIGHT + scroll.y;

        NamedTexture *tex = da_get(&sprite_textures, i);
        
        Rectangle button_rect = {square_x, square_y, SPRITE_SQUARE_SIZE, SPRITE_SQUARE_SIZE};
        int saved_base_color = GuiGetStyle(BUTTON, BASE_COLOR_NORMAL);

        int mouse_is_in_view = CheckCollisionPointRec(GetMousePosition(), view);

        ParticleSystem *system = get_selected_system();
        if (system->handle == &tex->texture) {
            GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, GuiGetStyle(BUTTON, BASE_COLOR_PRESSED));
        }
        if (GuiButton(button_rect , "") && mouse_is_in_view) {
            system->handle = &tex->texture;
        }
        GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, saved_base_color);

        Rectangle source = {0, 0, (float) tex->texture.width, (float) tex->texture.height};
        DrawTexturePro(tex->texture, source, button_rect, (Vector2) {0, 0}, 0.0f, WHITE);

        if (CheckCollisionPointRec(GetMousePosition(), button_rect) && mouse_is_in_view) {
            show_tooltip(GetFileNameWithoutExt(tex->path));
        }
    }

    GuiSetStyle(BUTTON, BORDER_WIDTH, saved_border_width);
    EndScissorMode();
}


void draw_tooltip() {
    if (strlen(tooltip) > 0) {
        Vector2 mouse_pos = GetMousePosition();
        int rect_width = MeasureText(tooltip, GuiGetStyle(DEFAULT, TEXT_SIZE)) + SMALL_PADDING*2;

        float rect_x = Clamp(mouse_pos.x, 0, GetScreenWidth()-rect_width);
        Rectangle rect = {rect_x, mouse_pos.y+20, rect_width, 20};
        GuiDrawRectangle(rect, 1, (Color) {100, 100, 100, 128}, (Color) {100, 100, 100, 80});
        GuiLabel((Rectangle) {rect.x+SMALL_PADDING/2, rect.y, rect.width, rect.height}, tooltip);
    }
}


void draw_emission_panel(int right_panel_x, ParticleSystem *current_system) {
    Rectangle content_view = {right_panel_x, EMISSION_PANEL_Y+PANEL_TITLE_HEIGHT, right_panel_width, EMISSION_PANEL_HEIGHT};
    int quarter_width = right_panel_width/4;

    GuiPanel((Rectangle) {right_panel_x, EMISSION_PANEL_Y, right_panel_width, EMISSION_PANEL_HEIGHT}, "Emission");

    GuiCheckBox((Rectangle) {GetScreenWidth()-25, EMISSION_PANEL_Y+2, 20, 20}, "", (bool*) &current_system->emitting);

    // Rate
    GuiLabel((Rectangle) {right_panel_x, content_view.y, right_panel_width, 20}, "Rate:");
    FloatEntry((Rectangle) {right_panel_x+40, content_view.y, right_panel_width-40, 20}, &current_system->emission_rate);
    current_system->emission_rate = MIN(current_system->emission_rate, MAX_EMISSION_RATE);

    // Angle
    GuiLabel((Rectangle) {right_panel_x, content_view.y+20, quarter_width, 20}, "Angle:");
    FloatEntry((Rectangle) {right_panel_x+quarter_width, content_view.y+20, quarter_width, 20}, &current_system->emission_angle);
    GuiLabel((Rectangle) {right_panel_x+quarter_width*2, content_view.y+20, quarter_width, 20}, "Var:");
    FloatEntry((Rectangle) {right_panel_x+quarter_width*3, content_view.y+20, quarter_width, 20}, &current_system->emission_angle_var);
    current_system->emission_angle_var = MAX(current_system->emission_angle_var, 0);

    // Speed
    GuiLabel((Rectangle) {right_panel_x, content_view.y+40, quarter_width, 20}, "Speed:");
    FloatEntry((Rectangle) {right_panel_x+quarter_width, content_view.y+40, quarter_width, 20}, &current_system->emission_speed);
    GuiLabel((Rectangle) {right_panel_x+quarter_width*2, content_view.y+40, quarter_width, 20}, "Var:");
    FloatEntry((Rectangle) {right_panel_x+quarter_width*3, content_view.y+40, quarter_width, 20}, &current_system->emission_speed_var);
    current_system->emission_speed_var = MAX(current_system->emission_speed_var, 0);

    // Shape
    GuiLabel((Rectangle) {right_panel_x, content_view.y+60, quarter_width, 20}, "Shape:");
    GuiComboBox((Rectangle) {right_panel_x+quarter_width, content_view.y+60, quarter_width*3, 20}, EMISSION_SHAPES, (int*) &current_system->emission_shape_tag);

    if (current_system->emission_shape_tag != SHAPE_LINE) {
        // Draw width and height labels
        GuiLabel((Rectangle) {right_panel_x, content_view.y+80, quarter_width, 20}, "Width:");
        GuiLabel((Rectangle) {right_panel_x+quarter_width*2, content_view.y+80, quarter_width, 20}, "Height:");
    }

    switch (current_system->emission_shape_tag) {
        case SHAPE_RECT:
            FloatEntry((Rectangle) {right_panel_x+quarter_width, content_view.y+80, quarter_width, 20}, &current_system->emission_shape.rect.width);
            FloatEntry((Rectangle) {right_panel_x+quarter_width*3, content_view.y+80, quarter_width, 20}, &current_system->emission_shape.rect.height);
            break;
        case SHAPE_ELLIPSE:
            FloatEntry((Rectangle) {right_panel_x+quarter_width, content_view.y+80, quarter_width, 20}, &current_system->emission_shape.ellipse.width);
            FloatEntry((Rectangle) {right_panel_x+quarter_width*3, content_view.y+80, quarter_width, 20}, &current_system->emission_shape.ellipse.height);
            break;
        case SHAPE_RING:
            FloatEntry((Rectangle) {right_panel_x+quarter_width, content_view.y+80, quarter_width, 20}, &current_system->emission_shape.ring.width);
            FloatEntry((Rectangle) {right_panel_x+quarter_width*3, content_view.y+80, quarter_width, 20}, &current_system->emission_shape.ring.height);
            GuiLabel((Rectangle) {right_panel_x, content_view.y+100, quarter_width, 20}, "Thickness:");
            FloatEntry((Rectangle) {right_panel_x+quarter_width, content_view.y+100, quarter_width, 20}, &current_system->emission_shape.ring.thickness);
            break;
        case SHAPE_LINE:
            GuiLabel((Rectangle) {right_panel_x, content_view.y+80, quarter_width, 20}, "x1:");
            FloatEntry((Rectangle) {right_panel_x+quarter_width, content_view.y+80, quarter_width, 20}, &current_system->emission_shape.line.start.x);
            GuiLabel((Rectangle) {right_panel_x+quarter_width*2, content_view.y+80, quarter_width, 20}, "y1:");
            FloatEntry((Rectangle) {right_panel_x+quarter_width*3, content_view.y+80, quarter_width, 20}, &current_system->emission_shape.line.start.y);

            GuiLabel((Rectangle) {right_panel_x, content_view.y+100, quarter_width, 20}, "x2:");
            FloatEntry((Rectangle) {right_panel_x+quarter_width, content_view.y+100, quarter_width, 20}, &current_system->emission_shape.line.end.x);
            GuiLabel((Rectangle) {right_panel_x+quarter_width*2, content_view.y+100, quarter_width, 20}, "y2:");
            FloatEntry((Rectangle) {right_panel_x+quarter_width*3, content_view.y+100, quarter_width, 20}, &current_system->emission_shape.line.end.y);

            GuiLabel((Rectangle) {right_panel_x, content_view.y+120, quarter_width, 20}, "Thickness:");
            FloatEntry((Rectangle) {right_panel_x+quarter_width, content_view.y+120, quarter_width, 20}, &current_system->emission_shape.line.thickness);
            break;
    }
}


void draw_lifetime_panel(int right_panel_x, ParticleSystem *current_system) {
    GuiPanel((Rectangle) {right_panel_x, LIFETIME_PANEL_Y, right_panel_width, LIFETIME_PANEL_HEIGHT}, "Lifetime");
    
    int content_y = LIFETIME_PANEL_Y+PANEL_TITLE_HEIGHT;
    int quarter_width = right_panel_width/4;

    Rectangle duration_rect = {right_panel_x, content_y, quarter_width, 20};
    Rectangle variation_rect = {right_panel_x+quarter_width*2, content_y, quarter_width, 20};
    GuiLabel(duration_rect, "Duration:");
    GuiLabel(variation_rect, "Var:");

    FloatEntry((Rectangle) {duration_rect.x+quarter_width, duration_rect.y, duration_rect.width, duration_rect.height}, &current_system->lifetime);
    current_system->lifetime = MAX(current_system->lifetime, 0);

    FloatEntry((Rectangle) {variation_rect.x+quarter_width, variation_rect.y, variation_rect.width, variation_rect.height}, &current_system->lifetime_var);
    current_system->lifetime_var = MAX(current_system->lifetime_var, 0);
}


void draw_size_panel(int right_panel_x, ParticleSystem *current_system) {
    GuiPanel((Rectangle) {right_panel_x, SIZE_PANEL_Y, right_panel_width, SIZE_PANEL_HEIGHT}, "Size");
    
    int content_y = SIZE_PANEL_Y+PANEL_TITLE_HEIGHT;
    int quarter_width = right_panel_width/4;

    Rectangle from_rect = {right_panel_x, content_y, quarter_width, 20};
    Rectangle to_rect = {right_panel_x+quarter_width*2, content_y, quarter_width, 20};
    GuiLabel(from_rect, "From:");
    GuiLabel(to_rect, "To:");

    FloatEntry((Rectangle) {from_rect.x+quarter_width, from_rect.y, from_rect.width, from_rect.height}, &current_system->from_size);
    current_system->from_size = MAX(current_system->from_size, 0);

    FloatEntry((Rectangle) {to_rect.x+quarter_width, to_rect.y, to_rect.width, to_rect.height}, &current_system->to_size);
    current_system->to_size = MAX(current_system->to_size, 0);
}

void draw_angular_speed_panel(int right_panel_x, ParticleSystem *current_system) {
    GuiPanel((Rectangle) {right_panel_x, ANGULAR_SPEED_PANEL_Y, right_panel_width, ANGULAR_SPEED_PANEL_HEIGHT}, "Angular Speed");
    
    int content_y = ANGULAR_SPEED_PANEL_Y+PANEL_TITLE_HEIGHT;
    int quarter_width = right_panel_width/4;

    Rectangle speed_rect = {right_panel_x, content_y, quarter_width, 20};
    Rectangle variation_rect = {right_panel_x+quarter_width*2, content_y, quarter_width, 20};
    GuiLabel(speed_rect, "Speed:");
    GuiLabel(variation_rect, "Var:");

    FloatEntry((Rectangle) {speed_rect.x+quarter_width, speed_rect.y, speed_rect.width, speed_rect.height}, &current_system->angular_speed);

    FloatEntry((Rectangle) {variation_rect.x+quarter_width, variation_rect.y, variation_rect.width, variation_rect.height}, &current_system->angular_speed_var);
    current_system->angular_speed_var = MAX(current_system->angular_speed_var, 0);
}


void draw_color_panel(int right_panel_x, ParticleSystem *current_system) {
    GuiPanel((Rectangle) {right_panel_x, COLOR_PANEL_Y, right_panel_width, COLOR_PANEL_HEIGHT}, "Color");

    int content_y = COLOR_PANEL_Y+PANEL_TITLE_HEIGHT+SMALL_PADDING;
    int half_width = right_panel_width/2;

    Rectangle from_rect = {right_panel_x, content_y, half_width-40, GuiGetStyle(DEFAULT, TEXT_SIZE)};
    Rectangle to_rect = {right_panel_x+half_width, content_y, half_width-40, GuiGetStyle(DEFAULT, TEXT_SIZE)};
    GuiLabel(from_rect, "From:");
    GuiLabel(to_rect, "To:");
    
    GuiColorPicker((Rectangle) {from_rect.x, from_rect.y+15, from_rect.width, 50}, "From", &current_system->from_color);
    GuiColorPicker((Rectangle) {to_rect.x, to_rect.y+15, to_rect.width, 50}, "To", &current_system->to_color);

    // Opacity sliders
    float opacity_value = current_system->from_color.a;
    GuiSlider((Rectangle) {from_rect.x, from_rect.y+64, from_rect.width, 15}, "", "", &opacity_value, 0, 255);
    current_system->from_color.a = opacity_value;

    opacity_value = current_system->to_color.a;
    GuiSlider((Rectangle) {to_rect.x, to_rect.y+64, to_rect.width, 15}, "", "", &opacity_value, 0, 255);
    current_system->to_color.a = opacity_value;
}


void draw_force_panel(int right_panel_x, ParticleSystem *current_system) {
    GuiPanel((Rectangle) {right_panel_x, FORCE_PANEL_Y, right_panel_width, FORCE_PANEL_HEIGHT}, "Force");

    int content_y = FORCE_PANEL_Y+PANEL_TITLE_HEIGHT+SMALL_PADDING;
    int quarter_width = right_panel_width/4;

    Rectangle x_rect = {right_panel_x, content_y, quarter_width, 20};
    Rectangle y_rect = {right_panel_x+quarter_width*2, content_y, quarter_width, 20};
    Rectangle drag_rect = {x_rect.x, x_rect.y+20, x_rect.width, x_rect.height};
    GuiLabel(x_rect, "X:");
    GuiLabel(y_rect, "Y:");
    GuiLabel(drag_rect, "Drag:");

    FloatEntry((Rectangle) {x_rect.x+quarter_width, x_rect.y, x_rect.width, x_rect.height}, &current_system->force.x);

    FloatEntry((Rectangle) {y_rect.x+quarter_width, y_rect.y, y_rect.width, y_rect.height}, &current_system->force.y);

    FloatEntry((Rectangle) {drag_rect.x+quarter_width, drag_rect.y, drag_rect.width, drag_rect.height}, &current_system->drag);
}


void draw_right_panel() {
    int width = GetScreenWidth();
    int height = GetScreenHeight();

    int right_panel_x = width-right_panel_width;
    GuiPanel((Rectangle) {right_panel_x, 0, right_panel_width, height}, "Edit System");

    if (selected_system_index != -1) {
        SystemContainer *container = da_get(&system_containers, selected_system_index);
        ParticleSystem *system = container->system;

        draw_details_panel(right_panel_x, container);
        draw_sprites_panel(right_panel_x);
        draw_emission_panel(right_panel_x, system);
        draw_lifetime_panel(right_panel_x, system);
        draw_size_panel(right_panel_x, system);
        draw_angular_speed_panel(right_panel_x, system);
        draw_color_panel(right_panel_x, system);
        draw_force_panel(right_panel_x, system);
    }
}


void draw_stats_panel() {
    int full_width = MIN(STATS_PANEL_WIDTH, GetScreenWidth()-left_panel_width-right_panel_width);
    
    size_t particle_count = 0;
    for (size_t i = 0; i < system_containers.size; i++) {
        SystemContainer *container = da_get(&system_containers, i);
        particle_count += container->system->count;
    }

    stats_panel_anim_timer = float_move_towards(stats_panel_anim_timer, stats_panel_anim_dest, GetFrameTime());
    float t = stats_panel_anim_timer/STATS_PANEL_ANIM_DURATION;
    float t1 = ease_in_out(Clamp(t/0.75, 0, 1));
    float t2 = ease_in_out(Clamp((t-0.75)*4, 0, 1));
    float width = full_width*t1;
    DrawRectangle(left_panel_width, 0, width, STATS_PANEL_HEIGHT*t1, (Color) {130, 130, 130, 64});

    if (t == 0) {
        if (TooltipButton((Rectangle) {left_panel_width, 0, 20, 20}, "#115#", "Show Stats")) {
            stats_panel_anim_timer = 0;
            stats_panel_anim_dest = STATS_PANEL_ANIM_DURATION;
        }
    }
    else if (t > 0) {
        if (TooltipButton((Rectangle) {MAX(left_panel_width+width-20, left_panel_width), 0, 20, (STATS_PANEL_HEIGHT-20)*t1+20}, "#114#", "Hide Stats")) {
            stats_panel_anim_timer = STATS_PANEL_ANIM_DURATION;
            stats_panel_anim_dest = 0;
        }

        if (t > 0.75) {
            int saved_text_color = GuiGetStyle(LABEL, TEXT_COLOR_NORMAL);
            Color text_color = ColorAlpha(GetColor(saved_text_color), t2);
            GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(text_color));

            GuiLabelF((Rectangle) {left_panel_width, 0, width, 20}, "FPS: %d", GetFPS());
            GuiLabelF((Rectangle) {left_panel_width, 20, width, 20}, "Particles: %d", particle_count);
            GuiLabelF((Rectangle) {left_panel_width, 40, width, 20}, "Update Time: %.2f ms", systems_update_time*1000);
            GuiLabelF((Rectangle) {left_panel_width, 60, width, 20}, "Draw Time: %.2f ms", systems_draw_time*1000);
            GuiLabelF((Rectangle) {left_panel_width, 80, width, 20}, "Total: %.2f ms", (systems_update_time+systems_draw_time)*1000);

            GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, saved_text_color);
        }
    }

}


void draw_ui() {
    draw_menubar();
    draw_left_panel();
    draw_right_panel();
    draw_stats_panel();
    draw_tooltip();
}


Rectangle get_view_rect() {
    return (Rectangle) {
        left_panel_width, 0,
        GetScreenWidth()-right_panel_width-left_panel_width, GetScreenHeight()
    };
}


void resize_ui_tick() {
    int width = GetScreenWidth();
    int height = GetScreenHeight();

    Vector2 mouse_pos = GetMousePosition();
    
    if (IsMouseButtonUp(MOUSE_LEFT_BUTTON)) {
        current_resizing_ui = RESIZE_NONE;
        SetMouseCursor(MOUSE_CURSOR_ARROW);
    }

    if (CheckCollisionPointRec(mouse_pos, (Rectangle) {left_panel_width-RESIZE_PADDING, 0, RESIZE_PADDING*2, height})) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            current_resizing_ui = RESIZE_LEFT;
        }
    }
    else if (CheckCollisionPointRec(mouse_pos, (Rectangle) {width-right_panel_width-RESIZE_PADDING, 0, RESIZE_PADDING*2, height})) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            current_resizing_ui = RESIZE_RIGHT;
        }
    }


    switch (current_resizing_ui) {
        case RESIZE_LEFT:
            left_panel_width = MAX(mouse_pos.x, PANEL_MIN_SIZE);
            break;
        case RESIZE_RIGHT:
            right_panel_width = MAX(width-mouse_pos.x, PANEL_MIN_SIZE);
            break;
    }
}


void select_system_tick() {
    Vector2 mouse_pos = GetMousePosition();

    for (size_t i = 0; i < system_containers.size; i++) {
        SystemContainer *container = da_get(&system_containers, i);
        Vector2 system_pos = {container->system->position.x, container->system->position.y};

        if (i == selected_system_index) {
            DrawCircle(system_pos.x, system_pos.y, CIRCLE_GIZMO_SIZE, COL_SELECTED);
        }

        if (i == hovered_system_index) {
            DrawCircle(system_pos.x, system_pos.y, CIRCLE_GIZMO_SIZE, COL_PREVIEW);
        }

        if (Vector2Distance(mouse_pos, system_pos) < CIRCLE_GIZMO_SIZE) {
            DrawCircle(system_pos.x, system_pos.y, CIRCLE_GIZMO_SIZE, COL_HOVERED);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                selected_system_index = i;
                break;
            }
        }
    }
}


void move_system_tick() {
    if (current_resizing_ui != RESIZE_NONE) {
        // Don't move if resizing a panel
        return;
    }

    if (selected_system_index != -1) {
        Vector2 mouse_pos = GetMousePosition();
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse_pos, get_view_rect())) {
            ParticleSystem *system = get_selected_system();
            system->position = (ParVec2) {mouse_pos.x, mouse_pos.y};
        }
    }
}


void emission_shape_gizmos_tick() {
    if (selected_system_index == -1) {
        return;
    }

    ParticleSystem *system = get_selected_system();

    switch (system->emission_shape_tag) {
        case SHAPE_RECT:
            DrawRectangleLines(
                system->position.x - system->emission_shape.rect.width/2, system->position.y - system->emission_shape.rect.height/2,
                system->emission_shape.rect.width, system->emission_shape.rect.height, (Color) {130, 130, 130, 64}
            );
            break;
        case SHAPE_ELLIPSE:
            DrawEllipseLines(
                system->position.x, system->position.y,
                system->emission_shape.ellipse.width/2, system->emission_shape.ellipse.height/2, (Color) {130, 130, 130, 64}
            );
            break;
        case SHAPE_RING:
            float half_thickness = system->emission_shape.ring.thickness/2;
            DrawEllipseLines(
                system->position.x, system->position.y,
                system->emission_shape.ring.width/2*(1-half_thickness), system->emission_shape.ring.height/2*(1-half_thickness), (Color) {130, 130, 130, 64}
            );
            DrawEllipseLines(
                system->position.x, system->position.y,
                system->emission_shape.ring.width/2*(1+half_thickness), system->emission_shape.ring.height/2*(1+half_thickness), (Color) {130, 130, 130, 64}
            );
            break;
        case SHAPE_LINE:
            EmissionShapeLine line = system->emission_shape.line;
            DrawLineEx(
                (Vector2) {line.start.x+system->position.x, line.start.y+system->position.y},
                (Vector2) {line.end.x+system->position.x, line.end.y+system->position.y},
                MAX(line.thickness, 1), (Color) {130, 130, 130, 64}
            );
            break;
    }

    Vector2 mouse_pos = GetMousePosition();

    if (system->emission_shape_tag == SHAPE_LINE) {
        // Move line endpoints
        EmissionShapeLine shape_line = system->emission_shape.line;
        Vector2 p1 = {shape_line.start.x+system->position.x, shape_line.start.y+system->position.y};
        Vector2 p2 = {shape_line.end.x+system->position.x, shape_line.end.y+system->position.y};
        if (CheckCollisionPointCircle(mouse_pos, p1, 5)) {
            DrawCircle(p1.x, p1.y, 5, COL_HOVERED);
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                emission_shape_edit_state = EDIT_P1;
            }
        }
        else if (CheckCollisionPointCircle(mouse_pos, p2, 5)) {
            DrawCircle(p2.x, p2.y, 5, COL_HOVERED);
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                emission_shape_edit_state = EDIT_P2;
            }
        }
    }
    else {
        // Resize width/height
        EmissionShapeRect shape_rect = system->emission_shape.rect;
        Rectangle rect = {system->position.x-shape_rect.width/2, system->position.y-shape_rect.height/2, shape_rect.width, shape_rect.height};
        RectangleEdges edges = get_rectangle_edges_with_padding(rect, 5);
        if (CheckCollisionPointRec(mouse_pos, edges.top) || CheckCollisionPointRec(mouse_pos, edges.bottom)) {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                emission_shape_edit_state = EDIT_HEIGHT;
            }
        }
        else if (CheckCollisionPointRec(mouse_pos, edges.left) || CheckCollisionPointRec(mouse_pos, edges.right)) {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                emission_shape_edit_state = EDIT_WIDTH;
            }
        }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        emission_shape_edit_state = EDIT_NONE;
    }

    Vector2 adjusted_pos = Vector2Subtract(mouse_pos, (Vector2) {system->position.x, system->position.y});
    switch (emission_shape_edit_state) {
        case EDIT_HEIGHT:
            system->emission_shape.rect.height = fabs(mouse_pos.y-system->position.y)*2;
            break;
        case EDIT_WIDTH:
            system->emission_shape.rect.width = fabs(mouse_pos.x-system->position.x)*2;
            break;
        case EDIT_P1:
            system->emission_shape.line.start = (ParVec2) {adjusted_pos.x, adjusted_pos.y};
            break;
        case EDIT_P2:
            system->emission_shape.line.end = (ParVec2) {adjusted_pos.x, adjusted_pos.y};
            break;
    }
}


// Menubar functions
void export_button_callback() {
    const char *file_path = tinyfd_saveFileDialog("Export Effect", "", sizeof(EFFECT_FILE_TYPES)/sizeof(char*), EFFECT_FILE_TYPES, "Spar effects");

    if (!file_path) {
        return;
    }

    // Clear existing file
    FILE *clearfile = fopen(file_path, "w");
    if (clearfile != NULL) {
        fclose(clearfile);
    }

    FILE *outfile = fopen(file_path, "a");
    if (outfile == NULL) {
        return;
    }

    // Create textures array
    DynamicArray texture_palette;
    da_create(&texture_palette);

    for (size_t i = 0; i < system_containers.size; i++) {
        SystemContainer *container = da_get(&system_containers, i);
        ParticleSystem *system = container->system;
        bool seen_texture = false;
        for (size_t j = 0; j < texture_palette.size; j++) {
            NamedTexture *tex = da_get(&texture_palette, j);
            if (&tex->texture == system->handle) {
                seen_texture = true;
                break;
            }
        }

        if (!seen_texture) {
            for (size_t j = 0; j < sprite_textures.size; j++) {
                NamedTexture *tex = da_get(&sprite_textures, j);
                if (&tex->texture == system->handle) {
                    da_append(&texture_palette, tex);
                    break;
                }
            }
        }
    }

    // Signature
    write_u32_le(outfile, FILE_SIGNATURE);

    // Write texture palette
    write_u16_le(outfile, (uint16_t) texture_palette.size);
    for (size_t i = 0; i < texture_palette.size; i++) {
        NamedTexture *tex = da_get(&texture_palette, i);

        size_t path_length = strlen(tex->path);
        write_u32_le(outfile, (uint32_t) path_length);
        fprintf(outfile, tex->path);
    }

    // Write effect count
    write_u16_le(outfile, (uint16_t) system_containers.size);

    for (size_t i = 0; i < system_containers.size; i++) {
        SystemContainer *container = da_get(&system_containers, i);
        ParticleSystem *system = container->system;

        // Get index of the texture in the palette
        // Offset by 1 so that an index of 0 means a NULL texture
        size_t palette_index = 0;
        for (size_t j = 1; j <= texture_palette.size; j++) {
            NamedTexture *tex = da_get(&texture_palette, j-1);
            if (&tex->texture == system->handle) {
                palette_index = j;
                break;
            }
        }

        // Write effect name
        size_t name_length = strlen(container->name);
        fputc((uint8_t) name_length, outfile);
        fprintf(outfile, container->name);

        write_u32_le(outfile, system->flags);

        // Emission rate, is emitting
        write_f32_le(outfile, system->emission_rate);
        fputc((uint8_t) system->emitting, outfile);

        // Emission shape
        fputc((uint8_t) system->emission_shape_tag, outfile);
        switch (system->emission_shape_tag) {
            case SHAPE_RECT:
                write_f32_le(outfile, system->emission_shape.rect.width);
                write_f32_le(outfile, system->emission_shape.rect.height);
                break;
            case SHAPE_ELLIPSE:
                write_f32_le(outfile, system->emission_shape.ellipse.width);
                write_f32_le(outfile, system->emission_shape.ellipse.height);
                break;
            case SHAPE_RING:
                write_f32_le(outfile, system->emission_shape.ring.width);
                write_f32_le(outfile, system->emission_shape.ring.height);
                write_f32_le(outfile, system->emission_shape.ring.thickness);
                break;
            case SHAPE_LINE:
                write_f32_le(outfile, system->emission_shape.line.start.x);
                write_f32_le(outfile, system->emission_shape.line.start.y);
                write_f32_le(outfile, system->emission_shape.line.end.x);
                write_f32_le(outfile, system->emission_shape.line.end.y);
                write_f32_le(outfile, system->emission_shape.line.thickness);
                break;
        }

        // Emission angle and speed
        write_f32_le(outfile, system->emission_angle);
        write_f32_le(outfile, system->emission_angle_var);
        write_f32_le(outfile, system->emission_speed);
        write_f32_le(outfile, system->emission_speed_var);

        // Lifetime
        write_f32_le(outfile, system->lifetime);
        write_f32_le(outfile, system->lifetime_var);

        // Position, force, drag
        write_f32_le(outfile, system->position.x);
        write_f32_le(outfile, system->position.y);
        write_f32_le(outfile, system->force.x);
        write_f32_le(outfile, system->force.y);
        write_f32_le(outfile, system->drag);

        // Angular speed
        write_f32_le(outfile, system->angular_speed);
        write_f32_le(outfile, system->angular_speed_var);

        // Size
        write_f32_le(outfile, system->from_size);
        write_f32_le(outfile, system->to_size);

        // Color
        fputc(system->from_color.r, outfile);
        fputc(system->from_color.g, outfile);
        fputc(system->from_color.b, outfile);
        fputc(system->from_color.a, outfile);
        fputc(system->to_color.r, outfile);
        fputc(system->to_color.g, outfile);
        fputc(system->to_color.b, outfile);
        fputc(system->to_color.a, outfile);

        // Texture index
        write_u16_le(outfile, (uint16_t) palette_index);
    }

    da_free(&texture_palette);
    fclose(outfile);
    printf("Effect exported to %s\n", file_path);
}


void import_button_callback() {
    const char *file_path = tinyfd_openFileDialog("Import Effect", "", sizeof(EFFECT_FILE_TYPES)/sizeof(char*), EFFECT_FILE_TYPES, "Spar effects", false);

    if (!file_path || !FileExists(file_path)) {
        return;
    }

    // Clear current effects
    while (system_containers.size > 0) {
        delete_system_container(0);
    }
    selected_system_index = -1;

    int data_size = 0;
    uint8_t *file_data = LoadFileData(file_path, &data_size);
    uint8_t *data_iter = file_data;

    if (next_u32_le(&data_iter) != FILE_SIGNATURE) {
        printf("Invalid file signature\n");
        return;
    }

    // Add sprites to palette
    DynamicArray texture_indices;
    da_create(&texture_indices);

    uint16_t palette_size = next_u16_le(&data_iter);
    for (uint16_t i = 0; i < palette_size; i++) {
        char sprite_path[256];
        uint32_t path_length = next_u32_le(&data_iter);
        memcpy(sprite_path, data_iter, path_length);
        data_iter += path_length;
        sprite_path[path_length] = '\0';

        int add_result = add_sprite(sprite_path);
        if (add_result < 0) {
            printf("Unable to load sprite \"%s\"", sprite_path);
        }
        else {
            da_append(&texture_indices, (void*) (size_t) add_result);
        }
    }

    uint16_t effect_count = next_u16_le(&data_iter);
    for (uint16_t i = 0; i < effect_count; i++) {
        add_system_container();
        SystemContainer *container = da_get(&system_containers, i);
        ParticleSystem *system = container->system;

        // Set name
        uint8_t name_length = next_u8(&data_iter);
        for (uint8_t i = 0; i < name_length; i++) {
            uint8_t next = next_u8(&data_iter);
            container->name[i] = next;
        }
        container->name[name_length] = '\0';

        system->flags = next_u32_le(&data_iter);

        system->emission_rate = next_f32_le(&data_iter);
        system->emitting = (int) next_u8(&data_iter);

        system->emission_shape_tag = (EmissionShapeTag) next_u8(&data_iter);
        switch (system->emission_shape_tag) {
            case SHAPE_RECT:
                system->emission_shape.rect.width = next_f32_le(&data_iter);
                system->emission_shape.rect.height = next_f32_le(&data_iter);
                break;
            case SHAPE_ELLIPSE:
                system->emission_shape.ellipse.width = next_f32_le(&data_iter);
                system->emission_shape.ellipse.height = next_f32_le(&data_iter);
                break;
            case SHAPE_RING:
                system->emission_shape.ring.width = next_f32_le(&data_iter);
                system->emission_shape.ring.height = next_f32_le(&data_iter);
                system->emission_shape.ring.thickness = next_f32_le(&data_iter);
                break;
            case SHAPE_LINE:
                system->emission_shape.line.start.x = next_f32_le(&data_iter);
                system->emission_shape.line.start.y = next_f32_le(&data_iter);
                system->emission_shape.line.end.x = next_f32_le(&data_iter);
                system->emission_shape.line.end.y = next_f32_le(&data_iter);
                system->emission_shape.line.thickness = next_f32_le(&data_iter);
                break;
        }

        system->emission_angle = next_f32_le(&data_iter);
        system->emission_angle_var = next_f32_le(&data_iter);
        system->emission_speed = next_f32_le(&data_iter);
        system->emission_speed_var = next_f32_le(&data_iter);

        system->lifetime = next_f32_le(&data_iter);
        system->lifetime_var = next_f32_le(&data_iter);

        system->position.x = next_f32_le(&data_iter);
        system->position.y = next_f32_le(&data_iter);
        system->force.x = next_f32_le(&data_iter);
        system->force.y = next_f32_le(&data_iter);
        system->drag = next_f32_le(&data_iter);

        system->angular_speed = next_f32_le(&data_iter);
        system->angular_speed_var = next_f32_le(&data_iter);

        system->from_size = next_f32_le(&data_iter);
        system->to_size = next_f32_le(&data_iter);
        
        system->from_color.r = next_u8(&data_iter);
        system->from_color.g = next_u8(&data_iter);
        system->from_color.b = next_u8(&data_iter);
        system->from_color.a = next_u8(&data_iter);
        system->to_color.r = next_u8(&data_iter);
        system->to_color.g = next_u8(&data_iter);
        system->to_color.b = next_u8(&data_iter);
        system->to_color.a = next_u8(&data_iter);

        uint16_t palette_index = next_u16_le(&data_iter);
        if (palette_index > 0) {
            size_t texture_index = (size_t) da_get(&texture_indices, palette_index-1);
            NamedTexture *tex = da_get(&sprite_textures, texture_index);
            system->handle = &tex->texture;
        }
    }

    da_free(&texture_indices);
    UnloadFileData(file_data);
}


void free_systemcontainer_ptr(void *container) {
    free_particle_system(((SystemContainer*) container)->system);
    free(container);
}

void unload_namedtexture_ptr(void *tex) {
    UnloadTexture(((NamedTexture*) tex)->texture);
    free(tex);
}

int main() {
    srand(time(NULL));

    da_create(&system_containers);
    da_create(&sprite_textures);
    
    InitWindow(1280, 720, "spargui");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    load_sprites_from_directory();

    while (!WindowShouldClose()) {
        text_box_counter = 0;
        text_box_was_clicked = false;
        clear_tooltip();

        if (selected_system_index >= system_containers.size) {
            // Safeguard against segfaults
            selected_system_index = -1;
        }

        resize_ui_tick();

        // Update systems
        double update_start_time = GetTime();
        float frame_time = GetFrameTime();
        for (size_t i = 0; i < system_containers.size; i++) {
            SystemContainer *container = da_get(&system_containers, i);
            update_particle_system(container->system, frame_time);
        }
        systems_update_time = GetTime() - update_start_time;

    
        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginScissorRect(get_view_rect());

        // Draw systems in reverse order so that the first effects are drawn on top
        double draw_start_time = GetTime();
        for (long i = system_containers.size-1; i >= 0; i--) {
            SystemContainer *container = da_get(&system_containers, i);
            draw_particle_system(container->system);
        }
        systems_draw_time = GetTime() - draw_start_time;
        
        EndScissorMode();

        select_system_tick();
        emission_shape_gizmos_tick();
        if (emission_shape_edit_state == RESIZE_NONE) {
            move_system_tick();
        }

        draw_ui();

        EndDrawing();

        // Use tab to navigate text boxes
        if (focused_text_box != -1) {
            if (IsKeyPressed(KEY_TAB)) {
                int increment = IsKeyDown(KEY_LEFT_SHIFT) ? -1 : 1;
                tab_focused_text_box = Wrap(focused_text_box+increment, 0, text_box_counter);
            }
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !text_box_was_clicked) {
            // Deselect focused text box
            focused_text_box = -1;
        }
    }

    da_free_all(&system_containers, free_systemcontainer_ptr);
    da_free_all(&sprite_textures, unload_namedtexture_ptr);

    CloseWindow();
}
