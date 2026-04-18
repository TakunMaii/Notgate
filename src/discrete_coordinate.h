#ifndef DISCRETE_COORDINATE_H
#define DISCRETE_COORDINATE_H

#include "miecs.h"
#include "basic_components.h"

typedef struct {
    int x;
    int y;
} DiscreteCoordinate;

float discrete_origin_x = 0.0f;
float discrete_origin_y = 0.0f;
float discrete_cell_size = 32.0f;

miecs_component_type DiscreteCoordinate_type;

void RegisterDiscreteCoordinateComponent(miecs_world *world)
{
    DiscreteCoordinate_type = miecs_component_register(world, "DiscreteCoordinate", sizeof(DiscreteCoordinate));
}

void SetDiscreteCoordinate(float origin_x, float origin_y, float cell_size)
{
    discrete_origin_x = origin_x;
    discrete_origin_y = origin_y;
    discrete_cell_size = cell_size;
}

void DiscreteCoordinateSystem(miecs_world *world)
{
    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 2, DiscreteCoordinate_type, Position_type);
    while (miecs_view_next(&it, &e)) {
        DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_get(world, e, DiscreteCoordinate_type);
        Position *pos = (Position *)miecs_component_get(world, e, Position_type);

        pos->x = discrete_origin_x + dc->x * discrete_cell_size;
        pos->y = discrete_origin_y + dc->y * discrete_cell_size;
    }
}


#endif