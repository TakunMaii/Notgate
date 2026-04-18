#define MIECS_IMPLEMENTATION
#include "miecs.h"
#include "basic_components.h"
#include "basic_systems.h"
#include <raylib.h>
#include <raymath.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "discrete_coordinate.h"
#include "hero_control.h"
#include "map.h"
#include "globals.h"

int main(void)
{
    InitWindow(window_width, window_height, "Repeater and Notgate");
    SetTargetFPS(60);

    miecs_world *world = miecs_world_create();
    RegisterBasicComponents(world);
    RegisterHeroControlComponent(world);
    RegisterDiscreteCoordinateComponent(world);

    map_init(world);

    bool command_visible = false;
    char command_buffer[32] = {0};
    int command_len = 0;
    char hint_text[64] = {0};
    float hint_timer = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        bool opened_this_frame = false;
        if (!command_visible && IsKeyPressed(KEY_SLASH)) {
            command_visible = true;
            command_buffer[0] = '/';
            command_buffer[1] = '\0';
            command_len = 1;
            opened_this_frame = true;
        }

        if (command_visible) {
            if (!opened_this_frame) {
                int ch = GetCharPressed();
                while (ch > 0) {
                    if (ch >= 32 && ch <= 126 && command_len < (int)sizeof(command_buffer) - 1) {
                        command_buffer[command_len++] = (char)ch;
                        command_buffer[command_len] = '\0';
                    }
                    ch = GetCharPressed();
                }
            }

            if (IsKeyPressed(KEY_BACKSPACE) && command_len > 0) {
                command_len--;
                command_buffer[command_len] = '\0';
                if (command_len == 0) {
                    command_visible = false;
                }
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                command_visible = false;
                command_len = 0;
                command_buffer[0] = '\0';
            } else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                bool format_ok = command_len >= 2 && command_buffer[0] == '/';
                for (int i = 1; i < command_len && format_ok; ++i) {
                    if (!isdigit((unsigned char)command_buffer[i])) {
                        format_ok = false;
                    }
                }

                if (!format_ok) {
                    snprintf(hint_text, sizeof(hint_text), "syntax error");
                    hint_timer = 2.0f;
                } else {
                    int level = atoi(command_buffer + 1);
                    if (level > 0 && level_file_exists(level)) {
                        map_load_level(world, level);
                    } else {
                        snprintf(hint_text, sizeof(hint_text), "level not found");
                        hint_timer = 2.0f;
                    }
                }

                command_visible = false;
                command_len = 0;
                command_buffer[0] = '\0';
            }
        }

        if (!command_visible) {
            HeroControlSystem(world);
        }

        DiscreteCoordinateSystem(world);
        map_particle_effect_system(world, dt);
        ParticleUpdateSystem(world, dt);

        if (hint_timer > 0.0f) {
            hint_timer -= dt;
            if (hint_timer < 0.0f) {
                hint_timer = 0.0f;
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        SpriteDrawingSystem(world);
        ParticleDrawingSystem(world);

        if (command_visible) {
            DrawRectangle(16, 16, 300, 36, Fade(BLACK, 0.7f));
            DrawText(command_buffer, 24, 24, 20, RAYWHITE);
        }
        if (hint_timer > 0.0f) {
            DrawRectangle(16, 56, 220, 28, Fade(BLACK, 0.6f));
            DrawText(hint_text, 24, 62, 18, RAYWHITE);
        }

        EndDrawing();
    }

    miecs_world_destroy(world);
    CloseWindow();
    return 0;
}
