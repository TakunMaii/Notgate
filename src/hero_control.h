#ifndef HERO_CONTROL_H
#define HERO_CONTROL_H

#include "miecs.h"
#include "discrete_coordinate.h"

typedef struct {
    int __unused;
} HeroControl;

miecs_component_type HeroControl_type;

void RegisterHeroControlComponent(miecs_world *world)
{
    HeroControl_type = miecs_component_register(world, "HeroControl", sizeof(HeroControl));
}

void hero_try_move_horizontal(miecs_world *world, miecs_entity e, int dx);
void hero_try_move_vertical(miecs_world *world, miecs_entity e, int dy);
bool hero_try_undo(miecs_world *world, miecs_entity e);

void HeroControlSystem(miecs_world *world)
{
    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 2, HeroControl_type, DiscreteCoordinate_type);
    while (miecs_view_next(&it, &e)) {
        DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_get(world, e, DiscreteCoordinate_type);

        static bool pressed[5] = {0, 0, 0, 0, 0}; /* W, S, A, D, Z */

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
            hero_try_undo(world, e);
            pressed[4] = true;
        }

        if (!IsKeyDown(KEY_W)) pressed[0] = false;
        if (!IsKeyDown(KEY_S)) pressed[1] = false;
        if (!IsKeyDown(KEY_A)) pressed[2] = false;
        if (!IsKeyDown(KEY_D)) pressed[3] = false;
        if (!IsKeyDown(KEY_Z)) pressed[4] = false;
    }
}

#endif
