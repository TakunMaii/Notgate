#define MIECS_IMPLEMENTATION
#include "miecs.h"
#include "basic_components.h"
#include "basic_systems.h"
#include <raylib.h>
#include <raymath.h>
#include "discrete_coordinate.h"
#include "hero_control.h"
#include "map.h"
#include "globals.h"

int main(void)
{
    InitWindow(window_width, window_height, "Repeater");
    SetTargetFPS(60);

    miecs_world *world = miecs_world_create();
    RegisterBasicComponents(world);
    RegisterHeroControlComponent(world);
    RegisterDiscreteCoordinateComponent(world);

    map_init(world);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        HeroControlSystem(world);
        DiscreteCoordinateSystem(world);

        BeginDrawing();
        ClearBackground(RAYWHITE);

        SpriteDrawingSystem(world);

        EndDrawing();
    }

    miecs_world_destroy(world);
    CloseWindow();
    return 0;
}