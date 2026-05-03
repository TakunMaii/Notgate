#ifndef BASIC_SYSTEMS_H_INCLUDED
#define BASIC_SYSTEMS_H_INCLUDED

#include <raylib.h>
#include "miecs.h"
#include "basic_components.h"
#include <math.h>
#include <stdlib.h>

void VelocitySystem(miecs_world *world, float delta_time)
{
    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 2, Position_type, Velocity_type);
    while (miecs_view_next(&it, &e)) {
        Position *p = miecs_component_get(world, e, Position_type);
        Velocity *v = miecs_component_get(world, e, Velocity_type);

        p->x += v->vx * delta_time;
        p->y += v->vy * delta_time;
    }
}

void AccelarationSystem(miecs_world *world, float delta_time)
{
    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 2, Velocity_type, Acceleration_type);
    while (miecs_view_next(&it, &e)) {
        Velocity *v = miecs_component_get(world, e, Velocity_type);
        Acceleration *a = miecs_component_get(world, e, Acceleration_type);

        v->vx += a->ax * delta_time;
        v->vy += a->ay * delta_time;
    }
}

void _quick_sort(miecs_entity *entities, int left, int right, miecs_world *world)
{
    if (left >= right) return;

    Sprite *pivotSprite = miecs_component_get(world, entities[right], Sprite_type);
    int pivotLayer = pivotSprite->layer;
    int i = left - 1;

    for (int j = left; j < right; ++j) {
        Sprite *s = miecs_component_get(world, entities[j], Sprite_type);
        if (s->layer < pivotLayer) {
            i++;
            // swap entities[i] and entities[j]
            miecs_entity temp = entities[i];
            entities[i] = entities[j];
            entities[j] = temp;
        }
    }
    // swap entities[i + 1] and entities[right] (pivot)
    miecs_entity temp = entities[i + 1];
    entities[i + 1] = entities[right];
    entities[right] = temp;

    _quick_sort(entities, left, i, world);
    _quick_sort(entities, i + 2, right, world);
}

void SpriteDrawingSystemPro(miecs_world *world, float camera_x, float camera_y)
{
    // find all entities with Position and Sprite components
    miecs_entity *entities = (miecs_entity *)malloc(sizeof(miecs_entity) * world->entity_count);
    int count = 0;

    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 2, Position_type, Sprite_type);
    while (miecs_view_next(&it, &e)) {
        entities[count++] = e;
    }

    // Quick sort entities by Sprite.layer
    _quick_sort(entities, 0, count - 1, world);

    const Color shadow_color = (Color){20, 24, 32, 120};
    const float shadow_offset_x = 6.0f;
    const float shadow_offset_y = 6.0f;

    // pass 1: draw sprite shadows
    for (int k = 0; k < count; ++k) {
        Sprite *s = miecs_component_get(world, entities[k], Sprite_type);
        Position *p = miecs_component_get(world, entities[k], Position_type);
        Scale *sc = miecs_component_get(world, entities[k], Scale_type);
        Rotation *r = miecs_component_get(world, entities[k], Rotation_type);
        float scale_x = sc ? sc->scale_x : 1.0f;
        float scale_y = sc ? sc->scale_y : 1.0f;
        float rotation = r ? r->rotationInDegrees : 0.0f;

        BeginShaderMode(s->shader);
        Rectangle sourceRec = s->sourceRec;
        int sign = s->flipX ? -1 : 1;
        sourceRec.width *= sign; // flip by negating width
        Rectangle destRec = {
            p->x - camera_x + shadow_offset_x,
            p->y - camera_y + shadow_offset_y,
            (float)(scale_x * s->sourceRec.width),
            (float)(scale_y * s->sourceRec.height)
        };
        Vector2 origin = { scale_x * s->sourceRec.width * 0.5f, scale_y * s->sourceRec.height * 0.5f };
        DrawTexturePro(s->texture, sourceRec, destRec, origin, rotation, shadow_color);
        EndShaderMode();
    }

    // pass 2: draw entities in sorted order
    for (int k = 0; k < count; ++k) {
        Sprite *s = miecs_component_get(world, entities[k], Sprite_type);
        Position *p = miecs_component_get(world, entities[k], Position_type);
        // sc and r may be NULL
        Scale *sc = miecs_component_get(world, entities[k], Scale_type);
        Rotation *r = miecs_component_get(world, entities[k], Rotation_type);
        float scale_x = sc ? sc->scale_x : 1.0f;
        float scale_y = sc ? sc->scale_y : 1.0f;
        float rotation = r ? r->rotationInDegrees : 0.0f;

        BeginShaderMode(s->shader);
        Rectangle sourceRec = s->sourceRec;
        int sign = s->flipX ? -1 : 1;
        sourceRec.width *= sign; // flip by negating width
        Rectangle destRec = { p->x - camera_x, p->y - camera_y,
            (float)(scale_x * s->sourceRec.width), (float)(scale_y * s->sourceRec.height) };
        Vector2 origin = { scale_x * s->sourceRec.width * 0.5f, scale_y * s->sourceRec.height * 0.5f };
        DrawTexturePro(s->texture, sourceRec, destRec, origin, rotation, WHITE);
        EndShaderMode();
    }

    free(entities);
}

void SpriteDrawingSystem(miecs_world *world)
{
    SpriteDrawingSystemPro(world, 0.0f, 0.0f);
}

void SpriteAnimationSystem(miecs_world *world, float delta_time)
{
    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 2, SpriteAnimation_type, Sprite_type);
    while (miecs_view_next(&it, &e)) {
        SpriteAnimation *sa = miecs_component_get(world, e, SpriteAnimation_type);
        Sprite *s = miecs_component_get(world, e, Sprite_type);

        sa->_timer += delta_time;
        if (sa->_timer >= sa->frameTime * sa->frameCount) {
            sa->_timer -= sa->frameTime * sa->frameCount;
        }
        int frame = (int)(sa->_timer / sa->frameTime);
        s->texture = sa->spriteSheet;
        s->sourceRec = (Rectangle){
            .x = frame * (sa->spriteSheet.width / sa->frameCount),
            .y = 0,
            .width = sa->spriteSheet.width / sa->frameCount,
            .height = sa->spriteSheet.height
        };
    }
}

void ParticleUpdateSystem(miecs_world *world, float delta_time)
{
    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 1, ParticleCollection_type);
    while (miecs_view_next(&it, &e)) {
        ParticleCollection *pc = miecs_component_get(world, e, ParticleCollection_type);
        for (int i = 0; i < pc->count; ++i) {
            pc->particles[i].life -= delta_time;
            if (pc->particles[i].life > 0) {
                pc->particles[i].position.x += pc->particles[i].velocity.x * delta_time;
                pc->particles[i].position.y += pc->particles[i].velocity.y * delta_time;
            }
            else {
                // remove dead particle by swapping with the last one
                pc->particles[i] = pc->particles[pc->count - 1];
                pc->count--;
                i--; // check the swapped particle in the next iteration
            }
        }

        if(pc->count <= 0) {
            miecs_entity_destroy(world, e);
        }
    }
}


void ParticleDrawingSystemPro(miecs_world *world, float camera_x, float camera_y)
{
    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 1, ParticleCollection_type);
    while (miecs_view_next(&it, &e)) {
        ParticleCollection *pc = miecs_component_get(world, e, ParticleCollection_type);
        for (int i = 0; i < pc->count; ++i) {
            DrawCircleV((Vector2){ pc->particles[i].position.x - camera_x, pc->particles[i].position.y - camera_y },
            pc->particles[i].radius, pc->particles[i].color);
        }
    }
}

void ParticleDrawingSystem(miecs_world *world)
{
    ParticleDrawingSystemPro(world, 0.0f, 0.0f);
}

void _seperateCollision(Collision *c1, Collision *c2, Position *p1, Position *p2)
{
    // This function is called when two non-trigger entities collide.
    if(c1->isStatic && c2->isStatic) {
        // if both are static, do nothing (they are already separated)
        return;
    }
    else if(c1->isStatic && !c2->isStatic) {
        // if c1 is static and c2 is dynamic, move c2 out of collision
        float overlapX = (c1->width + c2->width) * 0.5f - fabsf((p1->x + c1->biasX) - (p2->x + c2->biasX));
        float overlapY = (c1->height + c2->height) * 0.5f - fabsf((p1->y + c1->biasY) - (p2->y + c2->biasY));
        if (overlapX < overlapY) {
            p2->x += (p2->x > p1->x) ? overlapX : -overlapX;
        } else {
            p2->y += (p2->y > p1->y) ? overlapY : -overlapY;
        }
    }
    else if(!c1->isStatic && c2->isStatic) {
        // if c1 is dynamic and c2 is static, move c1 out of collision
        float overlapX = (c1->width + c2->width) * 0.5f - fabsf((p1->x + c1->biasX) - (p2->x + c2->biasX));
        float overlapY = (c1->height + c2->height) * 0.5f - fabsf((p1->y + c1->biasY) - (p2->y + c2->biasY));
        if (overlapX < overlapY) {
            p1->x += (p1->x > p2->x) ? overlapX : -overlapX;
        } else {
            p1->y += (p1->y > p2->y) ? overlapY : -overlapY;
        }
    }
    else {
        // if both are dynamic, move both out of collision
        float overlapX = (c1->width + c2->width) * 0.5f - fabsf((p1->x + c1->biasX) - (p2->x + c2->biasX));
        float overlapY = (c1->height + c2->height) * 0.5f - fabsf((p1->y + c1->biasY) - (p2->y + c2->biasY));
        if (overlapX < overlapY) {
            float move = overlapX * 0.5f;
            p1->x += (p1->x > p2->x) ? move : -move;
            p2->x += (p2->x > p1->x) ? move : -move;
        } else {
            float move = overlapY * 0.5f;
            p1->y += (p1->y > p2->y) ? move : -move;
            p2->y += (p2->y > p1->y) ? move : -move;
        }
    }
}

void CollisionUpdateSystem(miecs_world *world)
{
    miecs_entity *entities = (miecs_entity *)malloc(sizeof(miecs_entity) * world->entity_count);
    int count = 0;

    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 2, Collision_type, Position_type);
    while (miecs_view_next(&it, &e)) {
        entities[count++] = e;
    }

    for(int i = 0; i < count; ++i) {
        for(int j = i + 1; j < count; ++j) {
            Collision *c1 = miecs_component_get(world, entities[i], Collision_type);
            Collision *c2 = miecs_component_get(world, entities[j], Collision_type);
            Position *p1 = miecs_component_get(world, entities[i], Position_type);
            Position *p2 = miecs_component_get(world, entities[j], Position_type);

            Rectangle r1 = { p1->x + c1->biasX - c1->width * 0.5, p1->y + c1->biasY - c1->height * 0.5, c1->width, c1->height };
            Rectangle r2 = { p2->x + c2->biasX - c2->width * 0.5, p2->y + c2->biasY - c2->height * 0.5, c2->width, c2->height };

            if (CheckCollisionRecs(r1, r2)) {
                if (c1->onCollision) c1->onCollision(world, entities[i], entities[j]);
                if (c2->onCollision) c2->onCollision(world, entities[j], entities[i]);

                if(!c1->isTrigger && !c2->isTrigger) {
                    _seperateCollision(c1, c2, p1, p2);
                }
            }
        }
    }

    free(entities);
}

// For debugging: Draw rectangles for entities with Collision component
void CollisionDrawingSystemPro(miecs_world *world, float camera_x, float camera_y)
{
    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, world, 2, Position_type, Collision_type);
    while (miecs_view_next(&it, &e)) {
        Position *p = miecs_component_get(world, e, Position_type);
        Collision *c = miecs_component_get(world, e, Collision_type);
        Rectangle rect = { p->x + c->biasX - camera_x - c->width * 0.5, p->y + c->biasY - camera_y - c->height * 0.5, c->width, c->height };
        DrawRectangleLinesEx(rect, 1.0f, c->isTrigger ? (Color){255, 0, 0, 155} : (Color){0, 0, 255, 155});
    }
}

void CollisionDrawingSystem(miecs_world *world)
{
    CollisionDrawingSystemPro(world, 0, 0);
}

#endif
