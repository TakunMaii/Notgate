#ifndef SOLVER_H
#define SOLVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "map.h"

typedef enum {
    SOLVER_IDLE = 0,
    SOLVER_RUNNING,
    SOLVER_TRUE,
    SOLVER_FALSE,
    SOLVER_REACH_LIMIT,
    SOLVER_ERROR,
} SolverStatus;

typedef struct {
    SolverStatus status;
    int path_limit;
    int searched_paths;
    double start_time;
    double elapsed;

    int cell_count;
    int bitset_bytes;
    int dynamic_bytes;
    int state_bytes;

    size_t queue_capacity;
    unsigned char **queue;
    size_t queue_head;
    size_t queue_tail;

    size_t visited_capacity;
    unsigned char **visited_states;
    uint64_t *visited_hashes;
    size_t visited_count;
} SolverContext;

SolverContext solver_ctx = {0};

uint64_t solver_hash_state(const unsigned char *data, int len)
{
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < len; ++i) {
        h ^= (uint64_t)data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

void solver_pack_bits(unsigned char *dst, const bool *src, int count)
{
    memset(dst, 0, (count + 7) / 8);
    for (int i = 0; i < count; ++i) {
        if (src[i]) {
            dst[i / 8] |= (unsigned char)(1u << (i % 8));
        }
    }
}

void solver_unpack_bits(bool *dst, const unsigned char *src, int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] = (src[i / 8] & (unsigned char)(1u << (i % 8))) != 0;
    }
}

void solver_encode_state(
    unsigned char *out,
    int hero_x,
    int hero_y,
    const DynamicTileType *dynamic_types,
    const bool *static_activated,
    const bool *dynamic_activated)
{
    out[0] = (unsigned char)(hero_x & 0xFF);
    out[1] = (unsigned char)((hero_x >> 8) & 0xFF);
    out[2] = (unsigned char)(hero_y & 0xFF);
    out[3] = (unsigned char)((hero_y >> 8) & 0xFF);

    int offset_dynamic = 4;
    int offset_static_bits = offset_dynamic + solver_ctx.dynamic_bytes;
    int offset_dynamic_bits = offset_static_bits + solver_ctx.bitset_bytes;

    memcpy(out + offset_dynamic, dynamic_types, solver_ctx.dynamic_bytes);
    solver_pack_bits(out + offset_static_bits, static_activated, solver_ctx.cell_count);
    solver_pack_bits(out + offset_dynamic_bits, dynamic_activated, solver_ctx.cell_count);
}

void solver_decode_state(
    const unsigned char *in,
    int *hero_x,
    int *hero_y,
    DynamicTileType *dynamic_types,
    bool *static_activated,
    bool *dynamic_activated)
{
    *hero_x = (int)(in[0] | (in[1] << 8));
    *hero_y = (int)(in[2] | (in[3] << 8));

    int offset_dynamic = 4;
    int offset_static_bits = offset_dynamic + solver_ctx.dynamic_bytes;
    int offset_dynamic_bits = offset_static_bits + solver_ctx.bitset_bytes;

    memcpy(dynamic_types, in + offset_dynamic, solver_ctx.dynamic_bytes);
    solver_unpack_bits(static_activated, in + offset_static_bits, solver_ctx.cell_count);
    solver_unpack_bits(dynamic_activated, in + offset_dynamic_bits, solver_ctx.cell_count);
}

bool solver_state_is_goal(int hero_x, int hero_y, const bool *static_activated)
{
    if (hero_x < 0 || hero_x >= map_width || hero_y < 0 || hero_y >= map_height) {
        return false;
    }
    int idx = hero_y * map_width + hero_x;
    return map_static_tiles[idx] == STATIC_TILE_PORTAL && static_activated[idx];
}

bool solver_cell_has_not_signal(const DynamicTileType *dynamic_types, int idx)
{
    return map_static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL || dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL;
}

bool solver_try_push_chain(
    DynamicTileType *dynamic_types,
    bool *dynamic_activated,
    int start_x,
    int start_y,
    int dx,
    int dy)
{
    int cursor_x = start_x;
    int cursor_y = start_y;

    if (cursor_x < 0 || cursor_x >= map_width || cursor_y < 0 || cursor_y >= map_height) {
        return false;
    }

    int cursor_idx = cursor_y * map_width + cursor_x;
    if (dynamic_types[cursor_idx] == DYNAMIC_TILE_NONE) {
        return false;
    }

    while (cursor_x >= 0 && cursor_x < map_width && cursor_y >= 0 && cursor_y < map_height) {
        int idx = cursor_y * map_width + cursor_x;
        if (dynamic_types[idx] == DYNAMIC_TILE_NONE) {
            break;
        }
        cursor_x += dx;
        cursor_y += dy;
    }

    if (cursor_x < 0 || cursor_x >= map_width || cursor_y < 0 || cursor_y >= map_height) {
        return false;
    }

    int target_idx = cursor_y * map_width + cursor_x;
    if (map_static_tiles[target_idx] != STATIC_TILE_EMPTY) {
        return false;
    }

    for (int x = cursor_x - dx, y = cursor_y - dy; x != start_x - dx || y != start_y - dy; x -= dx, y -= dy) {
        int src_idx = y * map_width + x;
        int dst_idx = (y + dy) * map_width + (x + dx);
        dynamic_types[dst_idx] = dynamic_types[src_idx];
        dynamic_activated[dst_idx] = dynamic_activated[src_idx];
        if (dynamic_types[dst_idx] == DYNAMIC_TILE_NOT_SIGNAL) {
            dynamic_activated[dst_idx] = true;
        }
        dynamic_types[src_idx] = DYNAMIC_TILE_NONE;
        dynamic_activated[src_idx] = false;
    }

    return true;
}

void solver_recalculate_activation(DynamicTileType *dynamic_types, bool *static_activated, bool *dynamic_activated)
{
    int cell_count = solver_ctx.cell_count;
    bool prev_static_activated[cell_count];
    bool prev_dynamic_activated[cell_count];
    bool const_emitter[cell_count];
    bool const_hits_not[cell_count];
    bool force_not_off[cell_count];
    bool emit_curr[cell_count];
    bool emit_next[cell_count];
    bool is_variable[cell_count];
    int component_id[cell_count];
    int queue[cell_count];
    unsigned char history[24 * cell_count];
    int component_size[cell_count];
    int component_hit_count[cell_count];

    memcpy(prev_static_activated, static_activated, sizeof(bool) * cell_count);
    memcpy(prev_dynamic_activated, dynamic_activated, sizeof(bool) * cell_count);
    memset(const_emitter, 0, sizeof(const_emitter));
    memset(const_hits_not, 0, sizeof(const_hits_not));
    memset(force_not_off, 0, sizeof(force_not_off));
    memset(emit_curr, 0, sizeof(emit_curr));
    memset(emit_next, 0, sizeof(emit_next));
    memset(is_variable, 0, sizeof(is_variable));
    memset(component_id, 0xFF, sizeof(component_id));
    memset(history, 0, sizeof(history));
    memset(component_size, 0, sizeof(component_size));
    memset(component_hit_count, 0, sizeof(component_hit_count));

    for (int idx = 0; idx < cell_count; ++idx) {
        if (static_is_activatable(map_static_tiles[idx])) {
            static_activated[idx] = false;
        }
        if (dynamic_is_activatable(dynamic_types[idx])) {
            dynamic_activated[idx] = false;
        }
    }

    int q_head = 0;
    int q_tail = 0;
    for (int y = 0; y < map_height; ++y) {
        for (int x = 0; x < map_width; ++x) {
            int idx = y * map_width + x;
            if (map_static_tiles[idx] == STATIC_TILE_FIXED_SIGNAL_SOURCE || dynamic_types[idx] == DYNAMIC_TILE_SIGNAL_SOURCE) {
                if (!const_emitter[idx]) {
                    const_emitter[idx] = true;
                    queue[q_tail++] = idx;
                }
            }
        }
    }

    while (q_head < q_tail) {
        int emitter_idx = queue[q_head++];
        int ex = emitter_idx % map_width;
        int ey = emitter_idx / map_width;

        for (int dy = -3; dy <= 3; ++dy) {
            int max_abs_dx = 3 - abs(dy);
            for (int dx = -max_abs_dx; dx <= max_abs_dx; ++dx) {
                int tx = ex + dx;
                int ty = ey + dy;
                if (tx < 0 || tx >= map_width || ty < 0 || ty >= map_height) {
                    continue;
                }
                int idx = ty * map_width + tx;
                if (map_static_tiles[idx] == STATIC_TILE_PORTAL) {
                    static_activated[idx] = true;
                }
                if (map_static_tiles[idx] == STATIC_TILE_FIXED_REPEATER && !const_emitter[idx]) {
                    const_emitter[idx] = true;
                    queue[q_tail++] = idx;
                }
                if (dynamic_types[idx] == DYNAMIC_TILE_REPEATER && !const_emitter[idx]) {
                    const_emitter[idx] = true;
                    queue[q_tail++] = idx;
                }
                if (solver_cell_has_not_signal(dynamic_types, idx)) {
                    const_hits_not[idx] = true;
                }
            }
        }
    }

    int component_count = 0;
    for (int idx = 0; idx < cell_count; ++idx) {
        if (!solver_cell_has_not_signal(dynamic_types, idx) || component_id[idx] >= 0) {
            continue;
        }

        int local_head = 0;
        int local_tail = 0;
        queue[local_tail++] = idx;
        component_id[idx] = component_count;

        while (local_head < local_tail) {
            int current = queue[local_head++];
            int cx = current % map_width;
            int cy = current / map_width;
            component_size[component_count]++;
            if (const_hits_not[current]) {
                component_hit_count[component_count]++;
            }

            for (int other = 0; other < cell_count; ++other) {
                if (component_id[other] >= 0 || !solver_cell_has_not_signal(dynamic_types, other)) {
                    continue;
                }
                int ox = other % map_width;
                int oy = other / map_width;
                if (abs(cx - ox) + abs(cy - oy) <= 3) {
                    component_id[other] = component_count;
                    queue[local_tail++] = other;
                }
            }
        }

        component_count++;
    }

    for (int idx = 0; idx < cell_count; ++idx) {
        if (!solver_cell_has_not_signal(dynamic_types, idx)) {
            continue;
        }
        int cid = component_id[idx];
        int size = component_size[cid];
        int hits = component_hit_count[cid];
        bool hit = const_hits_not[idx];
        if (size <= 1) {
            force_not_off[idx] = hit;
        } else if (hits > 0 && hits < size) {
            force_not_off[idx] = hit;
        } else {
            force_not_off[idx] = false;
        }
    }

    for (int idx = 0; idx < cell_count; ++idx) {
        bool has_static_repeater = map_static_tiles[idx] == STATIC_TILE_FIXED_REPEATER;
        bool has_dynamic_repeater = dynamic_types[idx] == DYNAMIC_TILE_REPEATER;
        bool has_static_not = map_static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL;
        bool has_dynamic_not = dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL;
        bool has_signal = map_static_tiles[idx] == STATIC_TILE_FIXED_SIGNAL_SOURCE || dynamic_types[idx] == DYNAMIC_TILE_SIGNAL_SOURCE;

        if (has_signal) {
            emit_curr[idx] = true;
            is_variable[idx] = false;
            continue;
        }
        if (has_static_repeater || has_dynamic_repeater) {
            if (const_emitter[idx]) {
                emit_curr[idx] = true;
                is_variable[idx] = false;
            } else {
                bool prev_on = has_static_repeater ? prev_static_activated[idx] : prev_dynamic_activated[idx];
                emit_curr[idx] = prev_on;
                is_variable[idx] = true;
            }
            continue;
        }
        if (has_static_not || has_dynamic_not) {
            if (force_not_off[idx]) {
                emit_curr[idx] = false;
                is_variable[idx] = false;
            } else {
                bool prev_off = has_static_not ? prev_static_activated[idx] : prev_dynamic_activated[idx];
                emit_curr[idx] = !prev_off;
                is_variable[idx] = true;
            }
        }
    }

    int history_count = 0;
    bool settled = false;
    for (int iter = 0; iter < 24; ++iter) {
        for (int idx = 0; idx < cell_count; ++idx) {
            history[history_count * cell_count + idx] = (is_variable[idx] && emit_curr[idx]) ? 1 : 0;
        }
        history_count++;

        for (int idx = 0; idx < cell_count; ++idx) {
            emit_next[idx] = emit_curr[idx];
            if (!is_variable[idx]) {
                continue;
            }

            int x = idx % map_width;
            int y = idx / map_width;
            bool has_input = false;
            for (int dy = -3; dy <= 3 && !has_input; ++dy) {
                int max_abs_dx = 3 - abs(dy);
                for (int dx = -max_abs_dx; dx <= max_abs_dx; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    int tx = x + dx;
                    int ty = y + dy;
                    if (tx < 0 || tx >= map_width || ty < 0 || ty >= map_height) {
                        continue;
                    }
                    int src = ty * map_width + tx;
                    if (emit_curr[src]) {
                        has_input = true;
                        break;
                    }
                }
            }

            if (map_static_tiles[idx] == STATIC_TILE_FIXED_REPEATER || dynamic_types[idx] == DYNAMIC_TILE_REPEATER) {
                emit_next[idx] = has_input;
            } else if (map_static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL || dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL) {
                emit_next[idx] = !has_input;
            }
        }

        bool changed = false;
        for (int idx = 0; idx < cell_count; ++idx) {
            if (is_variable[idx] && emit_next[idx] != emit_curr[idx]) {
                changed = true;
                break;
            }
        }
        if (!changed) {
            settled = true;
            break;
        }

        int cycle_start = -1;
        for (int t = 0; t < history_count; ++t) {
            bool equal = true;
            for (int idx = 0; idx < cell_count; ++idx) {
                if (!is_variable[idx]) {
                    continue;
                }
                bool state_t = history[t * cell_count + idx] != 0;
                if (state_t != emit_next[idx]) {
                    equal = false;
                    break;
                }
            }
            if (equal) {
                cycle_start = t;
                break;
            }
        }

        if (cycle_start >= 0) {
            for (int idx = 0; idx < cell_count; ++idx) {
                if (!is_variable[idx]) {
                    continue;
                }
                bool first = history[cycle_start * cell_count + idx] != 0;
                bool oscillating = false;
                for (int t = cycle_start + 1; t < history_count; ++t) {
                    bool state_t = history[t * cell_count + idx] != 0;
                    if (state_t != first) {
                        oscillating = true;
                        break;
                    }
                }
                if (!oscillating && emit_next[idx] != first) {
                    oscillating = true;
                }
                emit_curr[idx] = oscillating ? false : emit_next[idx];
            }
            settled = true;
            break;
        }

        memcpy(emit_curr, emit_next, sizeof(bool) * cell_count);
    }

    if (!settled) {
        for (int idx = 0; idx < cell_count; ++idx) {
            if (is_variable[idx]) {
                emit_curr[idx] = false;
            }
        }
    }

    for (int idx = 0; idx < cell_count; ++idx) {
        if (map_static_tiles[idx] == STATIC_TILE_PORTAL) {
            static_activated[idx] = false;
        }

        if (map_static_tiles[idx] == STATIC_TILE_FIXED_REPEATER) {
            static_activated[idx] = emit_curr[idx];
        } else if (dynamic_types[idx] == DYNAMIC_TILE_REPEATER) {
            dynamic_activated[idx] = emit_curr[idx];
        }

        if (map_static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL) {
            static_activated[idx] = !emit_curr[idx];
        } else if (dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL) {
            dynamic_activated[idx] = !emit_curr[idx];
        }
    }

    for (int y = 0; y < map_height; ++y) {
        for (int x = 0; x < map_width; ++x) {
            int idx = y * map_width + x;
            if (map_static_tiles[idx] != STATIC_TILE_PORTAL) {
                continue;
            }
            bool on = false;
            for (int dy = -3; dy <= 3 && !on; ++dy) {
                int max_abs_dx = 3 - abs(dy);
                for (int dx = -max_abs_dx; dx <= max_abs_dx; ++dx) {
                    int tx = x + dx;
                    int ty = y + dy;
                    if (tx < 0 || tx >= map_width || ty < 0 || ty >= map_height) {
                        continue;
                    }
                    int src = ty * map_width + tx;
                    if (emit_curr[src]) {
                        on = true;
                        break;
                    }
                }
            }
            static_activated[idx] = on;
        }
    }
}

bool solver_visited_insert(unsigned char *state)
{
    uint64_t h = solver_hash_state(state, solver_ctx.state_bytes);
    size_t mask = solver_ctx.visited_capacity - 1;
    size_t idx = (size_t)(h & mask);

    while (1) {
        if (!solver_ctx.visited_states[idx]) {
            solver_ctx.visited_states[idx] = state;
            solver_ctx.visited_hashes[idx] = h;
            solver_ctx.visited_count++;
            return true;
        }
        if (solver_ctx.visited_hashes[idx] == h
            && memcmp(solver_ctx.visited_states[idx], state, solver_ctx.state_bytes) == 0) {
            return false;
        }
        idx = (idx + 1) & mask;
    }
}

void solver_release_memory(void)
{
    if (solver_ctx.visited_states) {
        for (size_t i = 0; i < solver_ctx.visited_capacity; ++i) {
            free(solver_ctx.visited_states[i]);
        }
    }
    free(solver_ctx.queue);
    free(solver_ctx.visited_states);
    free(solver_ctx.visited_hashes);
    solver_ctx.queue = NULL;
    solver_ctx.visited_states = NULL;
    solver_ctx.visited_hashes = NULL;
    solver_ctx.queue_capacity = 0;
    solver_ctx.visited_capacity = 0;
    solver_ctx.queue_head = 0;
    solver_ctx.queue_tail = 0;
}

void solver_reset(void)
{
    solver_release_memory();
    solver_ctx.status = SOLVER_IDLE;
    solver_ctx.path_limit = 0;
    solver_ctx.searched_paths = 0;
    solver_ctx.start_time = 0.0;
    solver_ctx.elapsed = 0.0;
    solver_ctx.cell_count = 0;
    solver_ctx.bitset_bytes = 0;
    solver_ctx.dynamic_bytes = 0;
    solver_ctx.state_bytes = 0;
    solver_ctx.visited_count = 0;
}

size_t solver_next_pow2(size_t v)
{
    size_t p = 1;
    while (p < v) {
        p <<= 1;
    }
    return p;
}

void solver_start(miecs_world *world, int path_limit)
{
    solver_reset();

    if (!map_static_tiles || !map_dynamic_types || map_width <= 0 || map_height <= 0) {
        solver_ctx.status = SOLVER_ERROR;
        return;
    }

    solver_ctx.path_limit = path_limit;
    solver_ctx.cell_count = map_width * map_height;
    solver_ctx.bitset_bytes = (solver_ctx.cell_count + 7) / 8;
    solver_ctx.dynamic_bytes = (int)(sizeof(DynamicTileType) * solver_ctx.cell_count);
    solver_ctx.state_bytes = 4 + solver_ctx.dynamic_bytes + solver_ctx.bitset_bytes + solver_ctx.bitset_bytes;
    solver_ctx.queue_capacity = (size_t)path_limit + 1;
    solver_ctx.visited_capacity = solver_next_pow2((size_t)path_limit * 2 + 16);

    solver_ctx.queue = (unsigned char **)calloc(solver_ctx.queue_capacity, sizeof(unsigned char *));
    solver_ctx.visited_states = (unsigned char **)calloc(solver_ctx.visited_capacity, sizeof(unsigned char *));
    solver_ctx.visited_hashes = (uint64_t *)calloc(solver_ctx.visited_capacity, sizeof(uint64_t));
    if (!solver_ctx.queue || !solver_ctx.visited_states || !solver_ctx.visited_hashes) {
        solver_ctx.status = SOLVER_ERROR;
        solver_release_memory();
        return;
    }

    DiscreteCoordinate *hero_dc = (DiscreteCoordinate *)miecs_component_get(world, hero_entity, DiscreteCoordinate_type);
    if (!hero_dc) {
        solver_ctx.status = SOLVER_ERROR;
        solver_release_memory();
        return;
    }

    unsigned char *initial = (unsigned char *)malloc(solver_ctx.state_bytes);
    if (!initial) {
        solver_ctx.status = SOLVER_ERROR;
        solver_release_memory();
        return;
    }
    solver_encode_state(initial, hero_dc->x, hero_dc->y, map_dynamic_types, map_static_activated, map_dynamic_activated);
    solver_visited_insert(initial);
    solver_ctx.queue[solver_ctx.queue_tail++] = initial;

    if (solver_state_is_goal(hero_dc->x, hero_dc->y, map_static_activated)) {
        solver_ctx.status = SOLVER_TRUE;
        solver_ctx.elapsed = 0.0;
        return;
    }

    solver_ctx.start_time = GetTime();
    solver_ctx.elapsed = 0.0;
    solver_ctx.status = SOLVER_RUNNING;
}

void solver_finish(SolverStatus status)
{
    solver_ctx.status = status;
    solver_ctx.elapsed = GetTime() - solver_ctx.start_time;
    solver_release_memory();
}

void solver_update(int max_steps)
{
    if (solver_ctx.status != SOLVER_RUNNING) {
        return;
    }

    int cell_count = solver_ctx.cell_count;
    DynamicTileType dynamic_curr[cell_count];
    DynamicTileType dynamic_next[cell_count];
    bool static_curr[cell_count];
    bool dynamic_act_curr[cell_count];
    bool static_next[cell_count];
    bool dynamic_act_next[cell_count];
    int hero_x = 0;
    int hero_y = 0;

    static const int dirs[4][2] = {
        {0, 1}, {0, -1}, {-1, 0}, {1, 0}
    };

    for (int step = 0; step < max_steps; ++step) {
        if (solver_ctx.queue_head >= solver_ctx.queue_tail) {
            solver_finish(SOLVER_FALSE);
            return;
        }

        unsigned char *state = solver_ctx.queue[solver_ctx.queue_head++];
        solver_decode_state(state, &hero_x, &hero_y, dynamic_curr, static_curr, dynamic_act_curr);
        solver_ctx.searched_paths++;

        if (solver_ctx.searched_paths >= solver_ctx.path_limit) {
            solver_finish(SOLVER_REACH_LIMIT);
            return;
        }

        for (int d = 0; d < 4; ++d) {
            int dx = dirs[d][0];
            int dy = dirs[d][1];
            int new_x = hero_x + dx;
            int new_y = hero_y + dy;
            if (new_x < 0 || new_x >= map_width || new_y < 0 || new_y >= map_height) {
                continue;
            }

            memcpy(dynamic_next, dynamic_curr, sizeof(dynamic_next));
            memcpy(static_next, static_curr, sizeof(static_next));
            memcpy(dynamic_act_next, dynamic_act_curr, sizeof(dynamic_act_next));

            bool moved = false;
            int target_idx = new_y * map_width + new_x;
            if (dynamic_next[target_idx] != DYNAMIC_TILE_NONE) {
                if (solver_try_push_chain(dynamic_next, dynamic_act_next, new_x, new_y, dx, dy)) {
                    moved = true;
                }
            } else {
                bool can_walk = false;
                if (map_static_tiles[target_idx] == STATIC_TILE_EMPTY) {
                    can_walk = true;
                } else if (map_static_tiles[target_idx] == STATIC_TILE_PORTAL && static_next[target_idx]) {
                    can_walk = true;
                }
                if (can_walk) {
                    moved = true;
                }
            }

            if (!moved) {
                continue;
            }

            solver_recalculate_activation(dynamic_next, static_next, dynamic_act_next);

            if (solver_state_is_goal(new_x, new_y, static_next)) {
                solver_finish(SOLVER_TRUE);
                return;
            }

            unsigned char *next_state = (unsigned char *)malloc(solver_ctx.state_bytes);
            if (!next_state) {
                solver_finish(SOLVER_ERROR);
                return;
            }
            solver_encode_state(next_state, new_x, new_y, dynamic_next, static_next, dynamic_act_next);

            if (solver_visited_insert(next_state)) {
                if (solver_ctx.queue_tail >= solver_ctx.queue_capacity) {
                    free(next_state);
                    solver_finish(SOLVER_REACH_LIMIT);
                    return;
                }
                solver_ctx.queue[solver_ctx.queue_tail++] = next_state;
            } else {
                free(next_state);
            }
        }
    }

    solver_ctx.elapsed = GetTime() - solver_ctx.start_time;
}

SolverStatus solver_status(void)
{
    return solver_ctx.status;
}

double solver_elapsed(void)
{
    if (solver_ctx.status == SOLVER_RUNNING) {
        return GetTime() - solver_ctx.start_time;
    }
    return solver_ctx.elapsed;
}

int solver_searched_paths(void)
{
    return solver_ctx.searched_paths;
}

#endif
