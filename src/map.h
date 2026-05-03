#ifndef MAP_INCLUDE_H
#define MAP_INCLUDE_H

#include "miecs.h"
#include "basic_components.h"
#include "discrete_coordinate.h"
#include "hero_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "globals.h"

miecs_entity hero_entity;

typedef enum {
    STATIC_TILE_EMPTY = 0,
    STATIC_TILE_WALL,
    STATIC_TILE_PORTAL,
    STATIC_TILE_FIXED_SIGNAL_SOURCE,
    STATIC_TILE_FIXED_REPEATER,
    STATIC_TILE_FIXED_NOT_SIGNAL,
} StaticTileType;

typedef enum {
    DYNAMIC_TILE_NONE = 0,
    DYNAMIC_TILE_BOX,
    DYNAMIC_TILE_SIGNAL_SOURCE,
    DYNAMIC_TILE_REPEATER,
    DYNAMIC_TILE_NOT_SIGNAL,
} DynamicTileType;

#include "rules_kernel.h"

int map_width;
int map_height;
StaticTileType *map_static_tiles;
miecs_entity *map_static_entities;
bool *map_static_activated;
miecs_entity *map_dynamic_entities;
DynamicTileType *map_dynamic_types;
bool *map_dynamic_activated;
int current_level;

Texture2D portal_inactive_texture = {0};
Texture2D portal_active_texture = {0};
Texture2D repeater_inactive_texture = {0};
Texture2D repeater_active_texture = {0};
Texture2D fixed_repeater_inactive_texture = {0};
Texture2D fixed_repeater_active_texture = {0};
Texture2D not_signal_inactive_texture = {0};
Texture2D not_signal_active_texture = {0};
Texture2D fixed_not_signal_inactive_texture = {0};
Texture2D fixed_not_signal_active_texture = {0};
Sound sfx_active = {0};
Sound sfx_inactive = {0};
Sound sfx_move = {0};
Sound sfx_transport = {0};
bool map_sounds_loaded = false;
bool map_activation_sfx_enabled = true;

typedef struct {
    int hero_x;
    int hero_y;
    miecs_entity *dynamic_entities;
    DynamicTileType *dynamic_types;
    bool *static_activated;
    bool *dynamic_activated;
} UndoState;

UndoState *undo_stack = NULL;
int undo_stack_count = 0;
int undo_stack_capacity = 0;
float map_particle_spawn_timer = 0.0f;

bool level_file_exists(int level)
{
    char file[64];
    snprintf(file, sizeof(file), "assets/maps/map%d.txt", level);
    FILE *f = fopen(file, "r");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

void map_next_level(miecs_world *world);
void map_recalculate_activation(miecs_world *world);
void update_activation_visuals(miecs_world *world);
void map_particle_effect_system(miecs_world *world, float dt);
bool undo_stack_push(miecs_world *world, miecs_entity hero);
void undo_stack_discard_latest(void);
void undo_stack_clear(void);
bool hero_try_undo(miecs_world *world, miecs_entity e);
void map_audio_init(void);
void map_audio_shutdown(void);

void map_audio_init(void)
{
    if (map_sounds_loaded || !IsAudioDeviceReady()) {
        return;
    }

    sfx_active = LoadSound("assets/sounds/active.wav");
    sfx_inactive = LoadSound("assets/sounds/inactive.wav");
    sfx_move = LoadSound("assets/sounds/move.wav");
    sfx_transport = LoadSound("assets/sounds/transport.wav");

    SetSoundVolume(sfx_active, sfx_active_volume);
    SetSoundVolume(sfx_inactive, sfx_inactive_volume);
    SetSoundVolume(sfx_move, sfx_move_volume);
    SetSoundVolume(sfx_transport, sfx_transport_volume);
    map_sounds_loaded = true;
}

void map_audio_shutdown(void)
{
    if (!map_sounds_loaded) {
        return;
    }
    UnloadSound(sfx_active);
    UnloadSound(sfx_inactive);
    UnloadSound(sfx_move);
    UnloadSound(sfx_transport);
    map_sounds_loaded = false;
}

int map_index(int x, int y)
{
    return y * map_width + x;
}

bool map_in_bounds(int x, int y)
{
    return x >= 0 && x < map_width && y >= 0 && y < map_height;
}

float random_0_1(void)
{
    return (float)GetRandomValue(0, 10000) / 10000.0f;
}

bool is_particle_emitter_at(int idx)
{
    if (map_static_tiles[idx] == STATIC_TILE_FIXED_SIGNAL_SOURCE) {
        return true;
    }
    if (map_dynamic_types[idx] == DYNAMIC_TILE_SIGNAL_SOURCE) {
        return true;
    }
    if (map_static_tiles[idx] == STATIC_TILE_FIXED_REPEATER && map_static_activated[idx]) {
        return true;
    }
    if (map_dynamic_types[idx] == DYNAMIC_TILE_REPEATER && map_dynamic_activated[idx]) {
        return true;
    }
    if (map_static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL && !map_static_activated[idx]) {
        return true;
    }
    if (map_dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL && !map_dynamic_activated[idx]) {
        return true;
    }
    return false;
}

void spawn_emitter_particles(miecs_world *world, int emitter_x, int emitter_y)
{
    static const int boundary_offsets[12][2] = {
        {0, 3}, {1, 2}, {2, 1}, {3, 0}, {2, -1}, {1, -2},
        {0, -3}, {-1, -2}, {-2, -1}, {-3, 0}, {-2, 1}, {-1, 2}
    };

    miecs_entity e = miecs_entity_create(world);
    ParticleCollection *pc = (ParticleCollection *)miecs_component_add(world, e, ParticleCollection_type);
    pc->count = 0;

    float cell_size = discrete_cell_size;
    float center_x = discrete_origin_x + emitter_x * cell_size;
    float center_y = discrete_origin_y + emitter_y * cell_size;
    float particle_radius = cell_size * 0.07f;
    float jitter = cell_size * 0.2f;
    float speed = cell_size * 0.35f;
    Color color = (Color){95, 205, 228, 210};

    for (int i = 0; i < 6 && pc->count < MAX_PARTICLES; ++i) {
        int pick = GetRandomValue(0, 11);
        int dx = boundary_offsets[pick][0];
        int dy = boundary_offsets[pick][1];

        float px = center_x + dx * cell_size + (random_0_1() - 0.5f) * 2.0f * jitter;
        float py = center_y + dy * cell_size + (random_0_1() - 0.5f) * 2.0f * jitter;
        float vx = center_x - px;
        float vy = center_y - py;
        float len = sqrtf(vx * vx + vy * vy);
        if (len < 0.0001f) {
            continue;
        }
        vx = (vx / len) * speed;
        vy = (vy / len) * speed;

        pc->particles[pc->count].life = 0.9f + random_0_1() * 0.6f;
        pc->particles[pc->count].velocity = (Vector2){vx, vy};
        pc->particles[pc->count].position = (Vector2){px, py};
        pc->particles[pc->count].radius = particle_radius;
        pc->particles[pc->count].color = color;
        pc->count++;
    }

    if (pc->count == 0) {
        miecs_entity_destroy(world, e);
    }
}

void map_particle_effect_system(miecs_world *world, float dt)
{
    if (!map_static_tiles || !map_dynamic_types || !map_static_activated || !map_dynamic_activated) {
        return;
    }

    map_particle_spawn_timer += dt;
    const float spawn_interval = 0.12f;
    if (map_particle_spawn_timer < spawn_interval) {
        return;
    }
    map_particle_spawn_timer -= spawn_interval;

    for (int y = 0; y < map_height; ++y) {
        for (int x = 0; x < map_width; ++x) {
            int idx = map_index(x, y);
            if (is_particle_emitter_at(idx)) {
                spawn_emitter_particles(world, x, y);
            }
        }
    }
}

void ensure_activation_textures_loaded(void)
{
    if (portal_inactive_texture.id == 0) {
        portal_inactive_texture = LoadTexture("assets/art/portal_inactive.png");
    }
    if (portal_active_texture.id == 0) {
        portal_active_texture = LoadTexture("assets/art/portal_active.png");
    }
    if (repeater_inactive_texture.id == 0) {
        repeater_inactive_texture = LoadTexture("assets/art/repeater_inactive.png");
    }
    if (repeater_active_texture.id == 0) {
        repeater_active_texture = LoadTexture("assets/art/repeater_active.png");
    }
    if (fixed_repeater_inactive_texture.id == 0) {
        fixed_repeater_inactive_texture = LoadTexture("assets/art/fixed_repeater_inactive.png");
    }
    if (fixed_repeater_active_texture.id == 0) {
        fixed_repeater_active_texture = LoadTexture("assets/art/fixed_repeater_active.png");
    }
    if (not_signal_inactive_texture.id == 0) {
        not_signal_inactive_texture = LoadTexture("assets/art/not_signal_inactive.png");
    }
    if (not_signal_active_texture.id == 0) {
        not_signal_active_texture = LoadTexture("assets/art/not_signal_active.png");
    }
    if (fixed_not_signal_inactive_texture.id == 0) {
        fixed_not_signal_inactive_texture = LoadTexture("assets/art/fixed_not_signal_inactive.png");
    }
    if (fixed_not_signal_active_texture.id == 0) {
        fixed_not_signal_active_texture = LoadTexture("assets/art/fixed_not_signal_active.png");
    }
}

bool static_is_blocking(StaticTileType type)
{
    return type != STATIC_TILE_EMPTY;
}

bool static_is_walkable_for_hero(StaticTileType type, bool activated)
{
    if (type == STATIC_TILE_EMPTY) {
        return true;
    }
    if (type == STATIC_TILE_PORTAL) {
        return activated;
    }
    return false;
}

bool static_is_activatable(StaticTileType type)
{
    return type == STATIC_TILE_PORTAL || type == STATIC_TILE_FIXED_REPEATER || type == STATIC_TILE_FIXED_NOT_SIGNAL;
}

bool dynamic_is_activatable(DynamicTileType type)
{
    return type == DYNAMIC_TILE_REPEATER || type == DYNAMIC_TILE_NOT_SIGNAL;
}

void hero(miecs_world *world, int x, int y)
{
    hero_entity = miecs_entity_create(world);
    Position *p = (Position *)miecs_component_add(world, hero_entity, Position_type);
    *p = (Position){ .x = 0.0f, .y = 0.0f };

    DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_add(world, hero_entity, DiscreteCoordinate_type);
    *dc = (DiscreteCoordinate){ .x = x, .y = y };

    HeroControl *hc = (HeroControl *)miecs_component_add(world, hero_entity, HeroControl_type);
    *hc = (HeroControl){ .unused = 0 };

    Sprite *s = (Sprite *)miecs_component_add(world, hero_entity, Sprite_type);
    *s = (Sprite){
        .texture = LoadTexture("assets/art/hero.png"),
        .sourceRec = (Rectangle){ 0, 0, 16, 16 },
        .layer = 0,
        .shader = LoadMaterialDefault().shader,
        .flipX = false,
    };

    Scale *sc = (Scale *)miecs_component_add(world, hero_entity, Scale_type);
    *sc = (Scale){ .scale_x = 2.0f, .scale_y = 2.0f };

    WalkAnimation *wa = (WalkAnimation *)miecs_component_add(world, hero_entity, WalkAnimation_type);
    *wa = (WalkAnimation){ .is_walking = false, .timer = 0.0f, .duration = 0.15f, .amplitude = 0.3f };
}

miecs_entity wall(miecs_world *world, int x, int y)
{
    static Texture2D wall_texture = {0};
    if (wall_texture.id == 0) {
        wall_texture = LoadTexture("assets/art/wall.png");
    }

    miecs_entity e = miecs_entity_create(world);
    Position *p = (Position *)miecs_component_add(world, e, Position_type);
    *p = (Position){ .x = 0.0f, .y = 0.0f };

    DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_add(world, e, DiscreteCoordinate_type);
    *dc = (DiscreteCoordinate){ .x = x, .y = y };

    Sprite *s = (Sprite *)miecs_component_add(world, e, Sprite_type);
    *s = (Sprite){
        .texture = wall_texture,
        .sourceRec = (Rectangle){ 0, 0, 16, 16 },
        .layer = 1,
        .shader = LoadMaterialDefault().shader,
        .flipX = false,
    };

    Scale *sc = (Scale *)miecs_component_add(world, e, Scale_type);
    *sc = (Scale){ .scale_x = 2.0f, .scale_y = 2.0f };
    return e;
}

miecs_entity portal(miecs_world *world, int x, int y)
{
    ensure_activation_textures_loaded();

    miecs_entity e = miecs_entity_create(world);
    Position *p = (Position *)miecs_component_add(world, e, Position_type);
    *p = (Position){ .x = 0.0f, .y = 0.0f };

    DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_add(world, e, DiscreteCoordinate_type);
    *dc = (DiscreteCoordinate){ .x = x, .y = y };

    Sprite *s = (Sprite *)miecs_component_add(world, e, Sprite_type);
    *s = (Sprite){
        .texture = portal_inactive_texture,
        .sourceRec = (Rectangle){ 0, 0, 16, 16 },
        .layer = -1,
        .shader = LoadMaterialDefault().shader,
        .flipX = false,
    };

    Scale *sc = (Scale *)miecs_component_add(world, e, Scale_type);
    *sc = (Scale){ .scale_x = 2.0f, .scale_y = 2.0f };
    return e;
}

miecs_entity fixed_signal_source(miecs_world *world, int x, int y)
{
    static Texture2D texture = {0};
    if (texture.id == 0) {
        texture = LoadTexture("assets/art/fixed_signal_source.png");
    }

    miecs_entity e = miecs_entity_create(world);
    Position *p = (Position *)miecs_component_add(world, e, Position_type);
    *p = (Position){ .x = 0.0f, .y = 0.0f };

    DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_add(world, e, DiscreteCoordinate_type);
    *dc = (DiscreteCoordinate){ .x = x, .y = y };

    Sprite *s = (Sprite *)miecs_component_add(world, e, Sprite_type);
    *s = (Sprite){
        .texture = texture,
        .sourceRec = (Rectangle){ 0, 0, 16, 16 },
        .layer = 1,
        .shader = LoadMaterialDefault().shader,
        .flipX = false,
    };

    Scale *sc = (Scale *)miecs_component_add(world, e, Scale_type);
    *sc = (Scale){ .scale_x = 2.0f, .scale_y = 2.0f };
    return e;
}

miecs_entity fixed_repeater(miecs_world *world, int x, int y)
{
    ensure_activation_textures_loaded();

    miecs_entity e = miecs_entity_create(world);
    Position *p = (Position *)miecs_component_add(world, e, Position_type);
    *p = (Position){ .x = 0.0f, .y = 0.0f };

    DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_add(world, e, DiscreteCoordinate_type);
    *dc = (DiscreteCoordinate){ .x = x, .y = y };

    Sprite *s = (Sprite *)miecs_component_add(world, e, Sprite_type);
    *s = (Sprite){
        .texture = fixed_repeater_inactive_texture,
        .sourceRec = (Rectangle){ 0, 0, 16, 16 },
        .layer = 1,
        .shader = LoadMaterialDefault().shader,
        .flipX = false,
    };

    Scale *sc = (Scale *)miecs_component_add(world, e, Scale_type);
    *sc = (Scale){ .scale_x = 2.0f, .scale_y = 2.0f };
    return e;
}

miecs_entity fixed_not_signal(miecs_world *world, int x, int y)
{
    ensure_activation_textures_loaded();

    miecs_entity e = miecs_entity_create(world);
    Position *p = (Position *)miecs_component_add(world, e, Position_type);
    *p = (Position){ .x = 0.0f, .y = 0.0f };

    DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_add(world, e, DiscreteCoordinate_type);
    *dc = (DiscreteCoordinate){ .x = x, .y = y };

    Sprite *s = (Sprite *)miecs_component_add(world, e, Sprite_type);
    *s = (Sprite){
        .texture = fixed_not_signal_inactive_texture,
        .sourceRec = (Rectangle){ 0, 0, 16, 16 },
        .layer = 1,
        .shader = LoadMaterialDefault().shader,
        .flipX = false,
    };

    Scale *sc = (Scale *)miecs_component_add(world, e, Scale_type);
    *sc = (Scale){ .scale_x = 2.0f, .scale_y = 2.0f };
    return e;
}

void dynamic_unit(miecs_world *world, int x, int y, DynamicTileType type)
{
    static Texture2D box_texture = {0};
    static Texture2D signal_source_texture = {0};

    if (box_texture.id == 0) {
        box_texture = LoadTexture("assets/art/box.png");
    }
    if (signal_source_texture.id == 0) {
        signal_source_texture = LoadTexture("assets/art/signal_source.png");
    }
    ensure_activation_textures_loaded();

    Texture2D texture = box_texture;
    if (type == DYNAMIC_TILE_SIGNAL_SOURCE) {
        texture = signal_source_texture;
    } else if (type == DYNAMIC_TILE_REPEATER) {
        texture = repeater_inactive_texture;
    } else if (type == DYNAMIC_TILE_NOT_SIGNAL) {
        texture = not_signal_inactive_texture;
    }

    miecs_entity e = miecs_entity_create(world);
    Position *p = (Position *)miecs_component_add(world, e, Position_type);
    *p = (Position){ .x = 0.0f, .y = 0.0f };

    DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_add(world, e, DiscreteCoordinate_type);
    *dc = (DiscreteCoordinate){ .x = x, .y = y };

    Sprite *s = (Sprite *)miecs_component_add(world, e, Sprite_type);
    *s = (Sprite){
        .texture = texture,
        .sourceRec = (Rectangle){ 0, 0, 16, 16 },
        .layer = 1,
        .shader = LoadMaterialDefault().shader,
        .flipX = false,
    };

    Scale *sc = (Scale *)miecs_component_add(world, e, Scale_type);
    *sc = (Scale){ .scale_x = 2.0f, .scale_y = 2.0f };

    int idx = map_index(x, y);
    map_dynamic_entities[idx] = e;
    map_dynamic_types[idx] = type;
    map_dynamic_activated[idx] = false;
}

typedef struct {
    miecs_world *world;
} MapPushShiftCtx;

void on_map_push_shift(int src_x, int src_y, int dst_x, int dst_y, void *user)
{
    MapPushShiftCtx *ctx = (MapPushShiftCtx *)user;
    int src_idx = map_index(src_x, src_y);
    int dst_idx = map_index(dst_x, dst_y);
    miecs_entity moved = map_dynamic_entities[src_idx];
    if (moved) {
        DiscreteCoordinate *box_dc = (DiscreteCoordinate *)miecs_component_get(ctx->world, moved, DiscreteCoordinate_type);
        box_dc->x = dst_x;
        box_dc->y = dst_y;
    }
    map_dynamic_entities[dst_idx] = moved;
    map_dynamic_entities[src_idx] = 0;
}

bool try_push_chain(miecs_world *world, int start_x, int start_y, int dx, int dy)
{
    MapPushShiftCtx ctx = { .world = world };
    return rules_try_push_chain(
        map_width,
        map_height,
        map_static_tiles,
        map_dynamic_types,
        map_dynamic_activated,
        start_x,
        start_y,
        dx,
        dy,
        on_map_push_shift,
        &ctx);
}

bool undo_stack_push(miecs_world *world, miecs_entity hero)
{
    if (!map_dynamic_entities || !map_dynamic_types) {
        return false;
    }

    if (undo_stack_count >= undo_stack_capacity) {
        int new_capacity = undo_stack_capacity > 0 ? undo_stack_capacity * 2 : 32;
        UndoState *new_stack = (UndoState *)realloc(undo_stack, sizeof(UndoState) * new_capacity);
        if (!new_stack) {
            fprintf(stderr, "Failed to grow undo stack\n");
            return false;
        }
        undo_stack = new_stack;
        undo_stack_capacity = new_capacity;
    }

    int cell_count = map_width * map_height;
    UndoState state = {0};
    state.dynamic_entities = (miecs_entity *)malloc(sizeof(miecs_entity) * cell_count);
    state.dynamic_types = (DynamicTileType *)malloc(sizeof(DynamicTileType) * cell_count);
    state.static_activated = (bool *)malloc(sizeof(bool) * cell_count);
    state.dynamic_activated = (bool *)malloc(sizeof(bool) * cell_count);
    if (!state.dynamic_entities || !state.dynamic_types || !state.static_activated || !state.dynamic_activated) {
        free(state.dynamic_entities);
        free(state.dynamic_types);
        free(state.static_activated);
        free(state.dynamic_activated);
        fprintf(stderr, "Failed to allocate undo snapshot\n");
        return false;
    }

    memcpy(state.dynamic_entities, map_dynamic_entities, sizeof(miecs_entity) * cell_count);
    memcpy(state.dynamic_types, map_dynamic_types, sizeof(DynamicTileType) * cell_count);
    memcpy(state.static_activated, map_static_activated, sizeof(bool) * cell_count);
    memcpy(state.dynamic_activated, map_dynamic_activated, sizeof(bool) * cell_count);

    DiscreteCoordinate *hero_dc = (DiscreteCoordinate *)miecs_component_get(world, hero, DiscreteCoordinate_type);
    state.hero_x = hero_dc->x;
    state.hero_y = hero_dc->y;

    undo_stack[undo_stack_count] = state;
    undo_stack_count++;
    return true;
}

void undo_stack_discard_latest(void)
{
    if (undo_stack_count <= 0) {
        return;
    }

    undo_stack_count--;
    free(undo_stack[undo_stack_count].dynamic_entities);
    free(undo_stack[undo_stack_count].dynamic_types);
    free(undo_stack[undo_stack_count].static_activated);
    free(undo_stack[undo_stack_count].dynamic_activated);
    undo_stack[undo_stack_count].dynamic_entities = NULL;
    undo_stack[undo_stack_count].dynamic_types = NULL;
    undo_stack[undo_stack_count].static_activated = NULL;
    undo_stack[undo_stack_count].dynamic_activated = NULL;
}

void undo_stack_clear(void)
{
    while (undo_stack_count > 0) {
        undo_stack_discard_latest();
    }
    free(undo_stack);
    undo_stack = NULL;
    undo_stack_capacity = 0;
}

bool hero_try_move(miecs_world *world, miecs_entity e, int dx, int dy)
{
    DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_get(world, e, DiscreteCoordinate_type);
    int new_x = dc->x + dx;
    int new_y = dc->y + dy;
    if (!map_in_bounds(new_x, new_y)) {
        return false;
    }

    int target_idx = map_index(new_x, new_y);
    bool snapshot_pushed = undo_stack_push(world, e);
    bool moved = false;
    if (map_dynamic_types[target_idx] != DYNAMIC_TILE_NONE) {
        if (try_push_chain(world, new_x, new_y, dx, dy)) {
            dc->x = new_x;
            dc->y = new_y;
            moved = true;
        }
    } else if (static_is_walkable_for_hero(map_static_tiles[target_idx], map_static_activated[target_idx])) {
        dc->x = new_x;
        dc->y = new_y;
        moved = true;
    }

    if (!moved) {
        if (snapshot_pushed) {
            undo_stack_discard_latest();
        }
        return false;
    }

    // Trigger walk animation
    WalkAnimation *wa = miecs_component_get(world, e, WalkAnimation_type);
    if (wa) {
        wa->is_walking = true;
        wa->timer = 0.0f;
    }

    if (map_sounds_loaded) {
        PlaySound(sfx_move);
    }
    map_recalculate_activation(world);

    if (map_static_tiles[target_idx] == STATIC_TILE_PORTAL && map_static_activated[target_idx]) {
        if (map_sounds_loaded) {
            PlaySound(sfx_transport);
        }
        map_next_level(world);
    }
    return true;
}

bool hero_try_undo(miecs_world *world, miecs_entity e)
{
    if (undo_stack_count <= 0 || !map_dynamic_entities || !map_dynamic_types) {
        return false;
    }

    UndoState state = undo_stack[undo_stack_count - 1];
    undo_stack_count--;

    int cell_count = map_width * map_height;
    memcpy(map_dynamic_entities, state.dynamic_entities, sizeof(miecs_entity) * cell_count);
    memcpy(map_dynamic_types, state.dynamic_types, sizeof(DynamicTileType) * cell_count);
    memcpy(map_static_activated, state.static_activated, sizeof(bool) * cell_count);
    memcpy(map_dynamic_activated, state.dynamic_activated, sizeof(bool) * cell_count);

    DiscreteCoordinate *hero_dc = (DiscreteCoordinate *)miecs_component_get(world, e, DiscreteCoordinate_type);
    hero_dc->x = state.hero_x;
    hero_dc->y = state.hero_y;

    for (int y = 0; y < map_height; ++y) {
        for (int x = 0; x < map_width; ++x) {
            int idx = map_index(x, y);
            if (map_dynamic_entities[idx]) {
                DiscreteCoordinate *dc = (DiscreteCoordinate *)miecs_component_get(world, map_dynamic_entities[idx], DiscreteCoordinate_type);
                dc->x = x;
                dc->y = y;
            }
        }
    }

    free(state.dynamic_entities);
    free(state.dynamic_types);
    free(state.static_activated);
    free(state.dynamic_activated);

    map_recalculate_activation(world);
    return true;
}

void hero_try_move_horizontal(miecs_world *world, miecs_entity e, int dx)
{
    hero_try_move(world, e, dx, 0);
}

void hero_try_move_vertical(miecs_world *world, miecs_entity e, int dy)
{
    hero_try_move(world, e, 0, dy);
}

bool is_not_signal_static(StaticTileType type)
{
    return type == STATIC_TILE_FIXED_NOT_SIGNAL;
}

bool is_not_signal_dynamic(DynamicTileType type)
{
    return type == DYNAMIC_TILE_NOT_SIGNAL;
}

bool cell_has_not_signal(int idx)
{
    return is_not_signal_static(map_static_tiles[idx]) || is_not_signal_dynamic(map_dynamic_types[idx]);
}

bool cell_can_emit_signal(int idx)
{
    return rules_cell_can_emit_signal(
        map_static_tiles,
        map_dynamic_types,
        map_static_activated,
        map_dynamic_activated,
        idx);
}

void update_activation_visuals(miecs_world *world)
{
    ensure_activation_textures_loaded();

    int cell_count = map_width * map_height;
    for (int idx = 0; idx < cell_count; ++idx) {
        if (map_static_tiles[idx] == STATIC_TILE_PORTAL && map_static_entities[idx]) {
            Sprite *sprite = (Sprite *)miecs_component_get(world, map_static_entities[idx], Sprite_type);
            if (sprite) {
                sprite->texture = map_static_activated[idx] ? portal_active_texture : portal_inactive_texture;
            }
        } else if (map_static_tiles[idx] == STATIC_TILE_FIXED_REPEATER && map_static_entities[idx]) {
            Sprite *sprite = (Sprite *)miecs_component_get(world, map_static_entities[idx], Sprite_type);
            if (sprite) {
                sprite->texture = map_static_activated[idx] ? fixed_repeater_active_texture : fixed_repeater_inactive_texture;
            }
        } else if (map_static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL && map_static_entities[idx]) {
            Sprite *sprite = (Sprite *)miecs_component_get(world, map_static_entities[idx], Sprite_type);
            if (sprite) {
                sprite->texture = map_static_activated[idx] ? fixed_not_signal_active_texture : fixed_not_signal_inactive_texture;
            }
        }

        if (map_dynamic_types[idx] == DYNAMIC_TILE_REPEATER && map_dynamic_entities[idx]) {
            Sprite *sprite = (Sprite *)miecs_component_get(world, map_dynamic_entities[idx], Sprite_type);
            if (sprite) {
                sprite->texture = map_dynamic_activated[idx] ? repeater_active_texture : repeater_inactive_texture;
            }
        } else if (map_dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL && map_dynamic_entities[idx]) {
            Sprite *sprite = (Sprite *)miecs_component_get(world, map_dynamic_entities[idx], Sprite_type);
            if (sprite) {
                sprite->texture = map_dynamic_activated[idx] ? not_signal_active_texture : not_signal_inactive_texture;
            }
        }
    }
}

void map_recalculate_activation(miecs_world *world)
{
    if (!map_static_tiles || !map_dynamic_types || !map_static_activated || !map_dynamic_activated) {
        return;
    }

    int cell_count = map_width * map_height;
    bool *prev_static_activated = (bool *)malloc(sizeof(bool) * cell_count);
    bool *prev_dynamic_activated = (bool *)malloc(sizeof(bool) * cell_count);
    bool has_activated = false;
    bool has_deactivated = false;
    bool tracking_ok = prev_static_activated && prev_dynamic_activated;
    if (tracking_ok) {
        memcpy(prev_static_activated, map_static_activated, sizeof(bool) * cell_count);
        memcpy(prev_dynamic_activated, map_dynamic_activated, sizeof(bool) * cell_count);
    } else {
        free(prev_static_activated);
        free(prev_dynamic_activated);
        prev_static_activated = NULL;
        prev_dynamic_activated = NULL;
    }

    rules_recalculate_activation(
        map_width,
        map_height,
        map_static_tiles,
        map_dynamic_types,
        map_static_activated,
        map_dynamic_activated);

    if (tracking_ok && map_activation_sfx_enabled) {
        for (int idx = 0; idx < cell_count; ++idx) {
            if (static_is_activatable(map_static_tiles[idx])) {
                bool before = prev_static_activated[idx];
                bool after = map_static_activated[idx];
                if (!before && after) {
                    has_activated = true;
                } else if (before && !after) {
                    has_deactivated = true;
                }
            }
            if (dynamic_is_activatable(map_dynamic_types[idx])) {
                bool before = prev_dynamic_activated[idx];
                bool after = map_dynamic_activated[idx];
                if (!before && after) {
                    has_activated = true;
                } else if (before && !after) {
                    has_deactivated = true;
                }
            }
            if (has_activated && has_deactivated) {
                break;
            }
        }
    }
    free(prev_static_activated);
    free(prev_dynamic_activated);

    update_activation_visuals(world);
    if (map_sounds_loaded) {
        if (has_activated) {
            PlaySound(sfx_active);
        }
        if (has_deactivated) {
            PlaySound(sfx_inactive);
        }
    }
}

void map_load(miecs_world *world, const char *file)
{
    undo_stack_clear();
    map_particle_spawn_timer = 0.0f;

    FILE *f = fopen(file, "r");
    if (!f) {
        fprintf(stderr, "Failed to open map file: %s\n", file);
        return;
    }

    if (map_static_tiles) {
        miecs_view_iter it;
        miecs_entity e;
        miecs_view_begin(&it, world, 2, Sprite_type, Scale_type);
        while (miecs_view_next(&it, &e)) {
            miecs_entity_destroy(world, e);
        }
        miecs_view_begin(&it, world, 1, ParticleCollection_type);
        while (miecs_view_next(&it, &e)) {
            miecs_entity_destroy(world, e);
        }
        free(map_static_tiles);
        free(map_static_entities);
        free(map_static_activated);
        free(map_dynamic_entities);
        free(map_dynamic_types);
        free(map_dynamic_activated);
        map_static_tiles = NULL;
        map_static_entities = NULL;
        map_static_activated = NULL;
        map_dynamic_entities = NULL;
        map_dynamic_types = NULL;
        map_dynamic_activated = NULL;
    }

    int width = 0;
    int height = 0;
    int current_width = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (current_width > width) {
                width = current_width;
            }
            height++;
            current_width = 0;
        } else {
            current_width++;
        }
    }
    if (current_width > 0) {
        if (current_width > width) {
            width = current_width;
        }
        height++;
    }
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Failed to read map content from file: %s\n", file);
        fclose(f);
        return;
    }

    map_width = width;
    map_height = height;
    int cell_count = map_width * map_height;

    map_static_tiles = (StaticTileType *)malloc(sizeof(StaticTileType) * cell_count);
    map_static_entities = (miecs_entity *)calloc(cell_count, sizeof(miecs_entity));
    map_static_activated = (bool *)calloc(cell_count, sizeof(bool));
    map_dynamic_entities = (miecs_entity *)calloc(cell_count, sizeof(miecs_entity));
    map_dynamic_types = (DynamicTileType *)calloc(cell_count, sizeof(DynamicTileType));
    map_dynamic_activated = (bool *)calloc(cell_count, sizeof(bool));
    if (!map_static_tiles || !map_static_entities || !map_static_activated || !map_dynamic_entities || !map_dynamic_types || !map_dynamic_activated) {
        fprintf(stderr, "Failed to allocate map memory\n");
        free(map_static_tiles);
        free(map_static_entities);
        free(map_static_activated);
        free(map_dynamic_entities);
        free(map_dynamic_types);
        free(map_dynamic_activated);
        map_static_tiles = NULL;
        map_static_entities = NULL;
        map_static_activated = NULL;
        map_dynamic_entities = NULL;
        map_dynamic_types = NULL;
        map_dynamic_activated = NULL;
        fclose(f);
        return;
    }

    for (int idx = 0; idx < cell_count; ++idx) {
        map_static_tiles[idx] = STATIC_TILE_EMPTY;
    }

    rewind(f);
    int x = 0;
    int y = 0;
    while ((c = fgetc(f)) != EOF && y < height) {
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            while (x < width) {
                map_static_tiles[map_index(x, y)] = STATIC_TILE_EMPTY;
                x++;
            }
            y++;
            x = 0;
            continue;
        }

        if (x >= width) {
            continue;
        }

        char tile = (char)c;
        int idx = map_index(x, y);
        switch (tile) {
            case ' ': { map_static_tiles[idx] = STATIC_TILE_EMPTY; } break;
            case '#': {
                map_static_tiles[idx] = STATIC_TILE_WALL;
                map_static_entities[idx] = wall(world, x, y);
            } break;
            case '@': {
                map_static_tiles[idx] = STATIC_TILE_EMPTY;
                hero(world, x, y);
            } break;
            case 'o': {
                map_static_tiles[idx] = STATIC_TILE_EMPTY;
                dynamic_unit(world, x, y, DYNAMIC_TILE_BOX);
            } break;
            case 'P': {
                map_static_tiles[idx] = STATIC_TILE_PORTAL;
                map_static_entities[idx] = portal(world, x, y);
            } break;
            case 's': {
                map_static_tiles[idx] = STATIC_TILE_EMPTY;
                dynamic_unit(world, x, y, DYNAMIC_TILE_SIGNAL_SOURCE);
            } break;
            case 'r': {
                map_static_tiles[idx] = STATIC_TILE_EMPTY;
                dynamic_unit(world, x, y, DYNAMIC_TILE_REPEATER);
            } break;
            case 'n': {
                map_static_tiles[idx] = STATIC_TILE_EMPTY;
                dynamic_unit(world, x, y, DYNAMIC_TILE_NOT_SIGNAL);
            } break;
            case 'S': {
                map_static_tiles[idx] = STATIC_TILE_FIXED_SIGNAL_SOURCE;
                map_static_entities[idx] = fixed_signal_source(world, x, y);
            } break;
            case 'R': {
                map_static_tiles[idx] = STATIC_TILE_FIXED_REPEATER;
                map_static_entities[idx] = fixed_repeater(world, x, y);
            } break;
            case 'N': {
                map_static_tiles[idx] = STATIC_TILE_FIXED_NOT_SIGNAL;
                map_static_entities[idx] = fixed_not_signal(world, x, y);
            } break;
            default: {
                fprintf(stderr, "Unknown tile '%c' at (%d, %d) in file: %s\n", tile, x, y, file);
                fclose(f);
                return;
            }
        }
        x++;
    }

    if (y < height) {
        while (x < width) {
            map_static_tiles[map_index(x, y)] = STATIC_TILE_EMPTY;
            x++;
        }
        y++;
        while (y < height) {
            for (int fill_x = 0; fill_x < width; ++fill_x) {
                map_static_tiles[map_index(fill_x, y)] = STATIC_TILE_EMPTY;
            }
            y++;
        }
    }

    fclose(f);

    float map_original_width = map_width * 16.0f;
    float map_original_height = map_height * 16.0f;
    float map_target_width = window_width * 0.8f;
    float map_target_height = window_height * 0.8f;

    float sprite_scale = fminf(map_target_width / map_original_width, map_target_height / map_original_height);
    float map_origin_x = (window_width - map_original_width * sprite_scale) / 2.0f + 8.0f * sprite_scale;
    float map_origin_y = (window_height - map_original_height * sprite_scale) / 2.0f + 8.0f * sprite_scale;
    SetDiscreteCoordinate(map_origin_x, map_origin_y, 16.0f * sprite_scale);
    {
        miecs_view_iter it;
        miecs_entity e;
        miecs_view_begin(&it, world, 2, Sprite_type, Scale_type);
        while (miecs_view_next(&it, &e)) {
            Scale *sc = (Scale *)miecs_component_get(world, e, Scale_type);
            sc->scale_x = sprite_scale;
            sc->scale_y = sprite_scale;
        }
    }
    // Update hero's walk animation base scale
    WalkAnimation *wa = miecs_component_get(world, hero_entity, WalkAnimation_type);
    if (wa) {
        wa->base_scale = sprite_scale;
    }

    bool previous_activation_sfx = map_activation_sfx_enabled;
    map_activation_sfx_enabled = false;
    map_recalculate_activation(world);
    map_activation_sfx_enabled = previous_activation_sfx;
}

void map_init(miecs_world *world)
{
    map_audio_init();
    map_static_tiles = NULL;
    map_static_entities = NULL;
    map_static_activated = NULL;
    map_dynamic_entities = NULL;
    map_dynamic_types = NULL;
    map_dynamic_activated = NULL;
    undo_stack = NULL;
    undo_stack_count = 0;
    undo_stack_capacity = 0;
    map_particle_spawn_timer = 0.0f;
    map_activation_sfx_enabled = true;
    current_level = 1;
    map_load(world, "assets/maps/map1.txt");
}

void map_load_level(miecs_world *world, int level)
{
    char file[64];
    snprintf(file, sizeof(file), "assets/maps/map%d.txt", level);
    current_level = level;
    map_load(world, file);
}

void map_next_level(miecs_world *world)
{
    int next_level = current_level + 1;
    if (!level_file_exists(next_level)) {
        next_level = 1;
    }
    map_load_level(world, next_level);
}

#endif
