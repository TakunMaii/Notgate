#ifndef HERO_CONTROL_H
#define HERO_CONTROL_H

#include <raylib.h>
#include "miecs.h"
#include "discrete_coordinate.h"

typedef struct {
    int unused;
} HeroControl;

typedef struct {
    bool is_walking;
    float timer;
    float duration;
    float amplitude;
    float base_scale;
} WalkAnimation;

miecs_component_type HeroControl_type;
miecs_component_type WalkAnimation_type;

void RegisterHeroControlComponent(miecs_world *world)
{
    HeroControl_type = miecs_component_register(world, "HeroControl", sizeof(HeroControl));
    WalkAnimation_type = miecs_component_register(world, "WalkAnimation", sizeof(WalkAnimation));
}

void hero_try_move_horizontal(miecs_world *world, miecs_entity e, int dx);
void hero_try_move_vertical(miecs_world *world, miecs_entity e, int dy);
bool hero_try_undo(miecs_world *world, miecs_entity e);
void WalkAnimationSystem(miecs_world *world, float delta_time);

void HeroControlSystem(miecs_world *world)
{
    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 2, HeroControl_type, DiscreteCoordinate_type);
    while (miecs_view_next(&it, &e)) {
        static bool pressed[5] = {0, 0, 0, 0, 0}; /* W, S, A, D, Z */
        static float z_hold_time = 0.0f;
        static float z_repeat_timer = 0.0f;
        static bool z_repeat_blocked = false;
        float dt = GetFrameTime();

        if (IsKeyDown(KEY_W) && !pressed[0]) {
            hero_try_move_vertical(world, e, -1);
            pressed[0] = true;
        }
        if (IsKeyDown(KEY_S) && !pressed[1]) {
            hero_try_move_vertical(world, e, 1);
            pressed[1] = true;
        }
        if (IsKeyDown(KEY_A) && !pressed[2]) {
            hero_try_move_horizontal(world, e, -1);
            pressed[2] = true;
        }
        if (IsKeyDown(KEY_D) && !pressed[3]) {
            hero_try_move_horizontal(world, e, 1);
            pressed[3] = true;
        }
        if (IsKeyDown(KEY_Z) && !pressed[4]) {
            bool undone = hero_try_undo(world, e);
            pressed[4] = true;
            z_hold_time = 0.0f;
            z_repeat_timer = 0.0f;
            z_repeat_blocked = !undone;
        } else if (IsKeyDown(KEY_Z) && pressed[4] && !z_repeat_blocked) {
            z_hold_time += dt;
            if (z_hold_time >= 1.0f) {
                z_repeat_timer += dt;
                while (z_repeat_timer >= 0.1f) {
                    bool undone = hero_try_undo(world, e);
                    z_repeat_timer -= 0.1f;
                    if (!undone) {
                        z_repeat_blocked = true;
                        z_repeat_timer = 0.0f;
                        break;
                    }
                }
            }
        }

        if (!IsKeyDown(KEY_W)) pressed[0] = false;
        if (!IsKeyDown(KEY_S)) pressed[1] = false;
        if (!IsKeyDown(KEY_A)) pressed[2] = false;
        if (!IsKeyDown(KEY_D)) pressed[3] = false;
        if (!IsKeyDown(KEY_Z)) {
            pressed[4] = false;
            z_hold_time = 0.0f;
            z_repeat_timer = 0.0f;
            z_repeat_blocked = false;
        }
    }
}

void WalkAnimationSystem(miecs_world *world, float delta_time)
{
    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 2, WalkAnimation_type, Scale_type);
    while (miecs_view_next(&it, &e)) {
        WalkAnimation *wa = miecs_component_get(world, e, WalkAnimation_type);
        Scale *s = miecs_component_get(world, e, Scale_type);

        if (!wa->is_walking) return;

        wa->timer += delta_time;
        float progress = wa->timer / wa->duration;

        // Use sine wave: X and Y are complementary to maintain roughly constant area
        float factor = sinf(progress * 3.14159265f) * wa->amplitude;
        s->scale_x = wa->base_scale + factor;
        s->scale_y = wa->base_scale - factor;

        if (wa->timer >= wa->duration) {
            wa->is_walking = false;
            s->scale_x = wa->base_scale;
            s->scale_y = wa->base_scale;
        }
    }
}

#endif
