#define MIECS_IMPLEMENTATION
#include "miecs.h"
#include "basic_components.h"
#include "basic_systems.h"
#include <raylib.h>
#include <raymath.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "discrete_coordinate.h"
#include "hero_control.h"
#include "map.h"
#include "solver.h"
#include "globals.h"

typedef struct {
    Vector2 anchor;
    float amp_x;
    float amp_y;
    float speed_x;
    float speed_y;
    float phase_x;
    float phase_y;
} FloatingSquare;

static float randf(float min_v, float max_v)
{
    float t = (float)GetRandomValue(0, 10000) / 10000.0f;
    return min_v + (max_v - min_v) * t;
}

int main(void)
{
    InitWindow(window_width, window_height, "Notgate");
    InitAudioDevice();
    Music bgm = {0};
    bool bgm_loaded = false;
    if (FileExists("assets/sounds/bgm.wav")) {
        bgm = LoadMusicStream("assets/sounds/bgm.wav");
        SetMusicVolume(bgm, bgm_volume);
        PlayMusicStream(bgm);
        bgm_loaded = true;
    }
    SetTargetFPS(60);
    RenderTexture2D scene_target = LoadRenderTexture(window_width, window_height);
    SetTextureFilter(scene_target.texture, TEXTURE_FILTER_BILINEAR);
    Shader postprocess_shader = LoadShader(0, "assets/shaders/postprocess.fs");
    bool use_postprocess = postprocess_shader.id != 0;
    int post_vignette_loc = -1;
    int post_bloom_threshold_loc = -1;
    int post_bloom_strength_loc = -1;
    if (use_postprocess) {
        post_vignette_loc = GetShaderLocation(postprocess_shader, "uVignetteStrength");
        post_bloom_threshold_loc = GetShaderLocation(postprocess_shader, "uBloomThreshold");
        post_bloom_strength_loc = GetShaderLocation(postprocess_shader, "uBloomStrength");

        float vignette_strength = 0.35f;
        float bloom_threshold = 0.75f;
        float bloom_strength = 0.55f;
        if (post_vignette_loc >= 0) {
            SetShaderValue(postprocess_shader, post_vignette_loc, &vignette_strength, SHADER_UNIFORM_FLOAT);
        }
        if (post_bloom_threshold_loc >= 0) {
            SetShaderValue(postprocess_shader, post_bloom_threshold_loc, &bloom_threshold, SHADER_UNIFORM_FLOAT);
        }
        if (post_bloom_strength_loc >= 0) {
            SetShaderValue(postprocess_shader, post_bloom_strength_loc, &bloom_strength, SHADER_UNIFORM_FLOAT);
        }
    }

    miecs_world *world = miecs_world_create();
    RegisterBasicComponents(world);
    RegisterHeroControlComponent(world);
    RegisterDiscreteCoordinateComponent(world);

    map_init(world);
    FloatingSquare *ambient_squares = NULL;
    if (ambient_floating_square_count > 0) {
        ambient_squares = (FloatingSquare *)malloc(sizeof(FloatingSquare) * ambient_floating_square_count);
        if (ambient_squares) {
            for (int i = 0; i < ambient_floating_square_count; ++i) {
                ambient_squares[i].anchor = (Vector2){
                    randf(24.0f, (float)window_width - 24.0f),
                    randf(24.0f, (float)window_height - 24.0f)
                };
                ambient_squares[i].amp_x = randf(6.0f, 24.0f);
                ambient_squares[i].amp_y = randf(6.0f, 24.0f);
                ambient_squares[i].speed_x = randf(0.25f, 0.65f);
                ambient_squares[i].speed_y = randf(0.20f, 0.55f);
                ambient_squares[i].phase_x = randf(0.0f, 6.28318f);
                ambient_squares[i].phase_y = randf(0.0f, 6.28318f);
            }
        }
    }

    bool command_visible = false;
    char command_buffer[32] = {0};
    int command_len = 0;
    char hint_text[64] = {0};
    float hint_timer = 0.0f;
    float ambient_time = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        ambient_time += dt;
        if (bgm_loaded) {
            UpdateMusicStream(bgm);
            if (!IsMusicStreamPlaying(bgm)) {
                PlayMusicStream(bgm);
            }
        }

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
                if (strcmp(command_buffer, "/solve") == 0) {
                    solver_start(world, 10000000);
                    if (solver_status() == SOLVER_ERROR) {
                        snprintf(hint_text, sizeof(hint_text), "solve error");
                        hint_timer = 2.0f;
                    }
                } else {
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
                            solver_reset();
                            map_load_level(world, level);
                        } else {
                            snprintf(hint_text, sizeof(hint_text), "level not found");
                            hint_timer = 2.0f;
                        }
                    }
                }

                command_visible = false;
                command_len = 0;
                command_buffer[0] = '\0';
            }
        }

        if (!command_visible && IsKeyPressed(KEY_R)) {
            solver_reset();
            map_load_level(world, current_level);
        }

        if (!command_visible && solver_status() != SOLVER_RUNNING) {
            HeroControlSystem(world);
        }

        if (solver_status() == SOLVER_RUNNING) {
            solver_update(1000);
            if (solver_status() == SOLVER_TRUE) {
                snprintf(hint_text, sizeof(hint_text), "true");
                hint_timer = 3.0f;
                printf("solve=true, steps=%d, path=%s\n", solver_solution_length(), solver_solution_cn());
            } else if (solver_status() == SOLVER_FALSE) {
                snprintf(hint_text, sizeof(hint_text), "false");
                hint_timer = 3.0f;
            } else if (solver_status() == SOLVER_REACH_LIMIT) {
                snprintf(hint_text, sizeof(hint_text), "reach limit");
                hint_timer = 3.0f;
            } else if (solver_status() == SOLVER_ERROR) {
                snprintf(hint_text, sizeof(hint_text), "solve error");
                hint_timer = 3.0f;
            }
        }

        DiscreteCoordinateSystem(world);
        WalkAnimationSystem(world, dt);
        map_particle_effect_system(world, dt);
        ParticleUpdateSystem(world, dt);

        if (hint_timer > 0.0f) {
            hint_timer -= dt;
            if (hint_timer < 0.0f) {
                hint_timer = 0.0f;
            }
        }

        BeginTextureMode(scene_target);
        ClearBackground((Color){45, 58, 65, 255});

        if (ambient_squares) {
            Color ambient_color = (Color){114, 199, 216, 112};
            const float square_radius = 3.0f;
            const float square_size = square_radius * 2.0f;
            for (int i = 0; i < ambient_floating_square_count; ++i) {
                float px = ambient_squares[i].anchor.x
                    + sinf(ambient_time * ambient_squares[i].speed_x + ambient_squares[i].phase_x) * ambient_squares[i].amp_x;
                float py = ambient_squares[i].anchor.y
                    + cosf(ambient_time * ambient_squares[i].speed_y + ambient_squares[i].phase_y) * ambient_squares[i].amp_y;
                DrawRectangleV((Vector2){px - square_radius, py - square_radius}, (Vector2){square_size, square_size}, ambient_color);
            }
        }

        SpriteDrawingSystem(world);
        ParticleDrawingSystem(world);

        if (command_visible) {
            DrawRectangle(16, 16, 300, 36, Fade(BLACK, 0.7f));
            DrawText(command_buffer, 24, 24, 20, RAYWHITE);
        }
        if (solver_status() == SOLVER_RUNNING) {
            char solving_text[128];
            snprintf(
                solving_text,
                sizeof(solving_text),
                "solving... time=%.2fs paths=%d",
                solver_elapsed(),
                solver_searched_paths());
            DrawRectangle(16, 96, 420, 28, Fade(BLACK, 0.6f));
            DrawText(solving_text, 24, 102, 18, RAYWHITE);
        }
        if (hint_timer > 0.0f) {
            DrawRectangle(16, 56, 220, 28, Fade(BLACK, 0.6f));
            DrawText(hint_text, 24, 62, 18, RAYWHITE);
        }

        DrawText("WASD to move, Z to undo, R to reset", window_width - 420, window_height - 40, 20, GRAY);

        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        Rectangle source = {0.0f, 0.0f, (float)scene_target.texture.width, -(float)scene_target.texture.height};
        if (use_postprocess) {
            BeginShaderMode(postprocess_shader);
            DrawTextureRec(scene_target.texture, source, (Vector2){0.0f, 0.0f}, WHITE);
            EndShaderMode();
        } else {
            DrawTextureRec(scene_target.texture, source, (Vector2){0.0f, 0.0f}, WHITE);
        }
        EndDrawing();
    }

    solver_reset();
    miecs_world_destroy(world);
    map_audio_shutdown();
    if (bgm_loaded) {
        StopMusicStream(bgm);
        UnloadMusicStream(bgm);
    }
    free(ambient_squares);
    if (use_postprocess) {
        UnloadShader(postprocess_shader);
    }
    UnloadRenderTexture(scene_target);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
