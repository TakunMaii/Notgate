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
    unsigned char *state;
    int parent_node;
    char move;
} SolverNode;

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
    int *queue;
    size_t queue_head;
    size_t queue_tail;

    SolverNode *nodes;
    size_t nodes_capacity;
    size_t nodes_count;

    size_t visited_capacity;
    unsigned char **visited_states;
    uint64_t *visited_hashes;
    int *visited_node_indices;
    size_t visited_count;

    char *solution_moves;
    int solution_len;
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

bool solver_try_push_chain(
    DynamicTileType *dynamic_types,
    bool *dynamic_activated,
    int start_x,
    int start_y,
    int dx,
    int dy)
{
    return rules_try_push_chain(
        map_width,
        map_height,
        map_static_tiles,
        dynamic_types,
        dynamic_activated,
        start_x,
        start_y,
        dx,
        dy,
        NULL,
        NULL);
}

void solver_recalculate_activation(DynamicTileType *dynamic_types, bool *static_activated, bool *dynamic_activated)
{
    rules_recalculate_activation(
        map_width,
        map_height,
        map_static_tiles,
        dynamic_types,
        static_activated,
        dynamic_activated);
}

bool solver_visited_insert(unsigned char *state, int node_index)
{
    uint64_t h = solver_hash_state(state, solver_ctx.state_bytes);
    size_t mask = solver_ctx.visited_capacity - 1;
    size_t idx = (size_t)(h & mask);

    while (1) {
        if (!solver_ctx.visited_states[idx]) {
            solver_ctx.visited_states[idx] = state;
            solver_ctx.visited_hashes[idx] = h;
            solver_ctx.visited_node_indices[idx] = node_index;
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

void solver_build_solution(int parent_node, char final_move)
{
    free(solver_ctx.solution_moves);
    solver_ctx.solution_moves = NULL;
    solver_ctx.solution_len = 0;

    int cap = (int)solver_ctx.nodes_count + 2;
    char *rev = (char *)malloc((size_t)cap);
    if (!rev) {
        return;
    }

    int n = 0;
    if (final_move) {
        rev[n++] = final_move;
    }
    int node = parent_node;
    while (node >= 0 && n < cap - 1) {
        char mv = solver_ctx.nodes[node].move;
        if (mv) {
            rev[n++] = mv;
        }
        node = solver_ctx.nodes[node].parent_node;
    }

    char *out = (char *)malloc((size_t)n + 1);
    if (!out) {
        free(rev);
        return;
    }
    for (int i = 0; i < n; ++i) {
        out[i] = rev[n - 1 - i];
    }
    out[n] = '\0';
    free(rev);

    solver_ctx.solution_moves = out;
    solver_ctx.solution_len = n;
}

void solver_release_memory(void)
{
    if (solver_ctx.visited_states) {
        for (size_t i = 0; i < solver_ctx.visited_capacity; ++i) {
            free(solver_ctx.visited_states[i]);
        }
    }
    free(solver_ctx.queue);
    free(solver_ctx.nodes);
    free(solver_ctx.visited_states);
    free(solver_ctx.visited_hashes);
    free(solver_ctx.visited_node_indices);
    solver_ctx.queue = NULL;
    solver_ctx.nodes = NULL;
    solver_ctx.visited_states = NULL;
    solver_ctx.visited_hashes = NULL;
    solver_ctx.visited_node_indices = NULL;
    solver_ctx.queue_capacity = 0;
    solver_ctx.visited_capacity = 0;
    solver_ctx.nodes_capacity = 0;
    solver_ctx.queue_head = 0;
    solver_ctx.queue_tail = 0;
    solver_ctx.nodes_count = 0;
}

void solver_reset(void)
{
    solver_release_memory();
    free(solver_ctx.solution_moves);
    solver_ctx.solution_moves = NULL;
    solver_ctx.solution_len = 0;
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
    solver_ctx.nodes_capacity = (size_t)path_limit + 1;
    solver_ctx.visited_capacity = solver_next_pow2((size_t)path_limit * 2 + 16);

    solver_ctx.queue = (int *)calloc(solver_ctx.queue_capacity, sizeof(int));
    solver_ctx.nodes = (SolverNode *)calloc(solver_ctx.nodes_capacity, sizeof(SolverNode));
    solver_ctx.visited_states = (unsigned char **)calloc(solver_ctx.visited_capacity, sizeof(unsigned char *));
    solver_ctx.visited_hashes = (uint64_t *)calloc(solver_ctx.visited_capacity, sizeof(uint64_t));
    solver_ctx.visited_node_indices = (int *)calloc(solver_ctx.visited_capacity, sizeof(int));
    if (!solver_ctx.queue || !solver_ctx.nodes || !solver_ctx.visited_states || !solver_ctx.visited_hashes || !solver_ctx.visited_node_indices) {
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
    solver_ctx.nodes[0].state = initial;
    solver_ctx.nodes[0].parent_node = -1;
    solver_ctx.nodes[0].move = 0;
    solver_ctx.nodes_count = 1;
    solver_visited_insert(initial, 0);
    solver_ctx.queue[solver_ctx.queue_tail++] = 0;

    if (solver_state_is_goal(hero_dc->x, hero_dc->y, map_static_activated)) {
        solver_build_solution(-1, 0);
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
    static const char dir_moves[4] = {'U', 'D', 'L', 'R'};

    for (int step = 0; step < max_steps; ++step) {
        if (solver_ctx.queue_head >= solver_ctx.queue_tail) {
            solver_finish(SOLVER_FALSE);
            return;
        }

        int current_node = solver_ctx.queue[solver_ctx.queue_head++];
        unsigned char *state = solver_ctx.nodes[current_node].state;
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

            char move_char = dir_moves[d];
            if (solver_state_is_goal(new_x, new_y, static_next)) {
                solver_build_solution(current_node, move_char);
                solver_finish(SOLVER_TRUE);
                return;
            }

            unsigned char *next_state = (unsigned char *)malloc(solver_ctx.state_bytes);
            if (!next_state) {
                solver_finish(SOLVER_ERROR);
                return;
            }
            solver_encode_state(next_state, new_x, new_y, dynamic_next, static_next, dynamic_act_next);

            if (solver_ctx.nodes_count >= solver_ctx.nodes_capacity || solver_ctx.queue_tail >= solver_ctx.queue_capacity) {
                free(next_state);
                solver_finish(SOLVER_REACH_LIMIT);
                return;
            }

            int next_node = (int)solver_ctx.nodes_count;
            if (solver_visited_insert(next_state, next_node)) {
                solver_ctx.nodes[next_node].state = next_state;
                solver_ctx.nodes[next_node].parent_node = current_node;
                solver_ctx.nodes[next_node].move = move_char;
                solver_ctx.nodes_count++;
                solver_ctx.queue[solver_ctx.queue_tail++] = next_node;
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

const char *solver_solution_moves(void)
{
    return solver_ctx.solution_moves ? solver_ctx.solution_moves : "";
}

int solver_solution_length(void)
{
    return solver_ctx.solution_len;
}

const char *solver_solution_cn(void)
{
    static char text[8192];
    text[0] = '\0';

    const char *moves = solver_solution_moves();
    int len = solver_solution_length();
    if (len <= 0) {
        return text;
    }

    size_t pos = 0;
    for (int i = 0; i < len; ++i) {
        const char *w = "";
        if (moves[i] == 'U') w = "S";
        else if (moves[i] == 'D') w = "W";
        else if (moves[i] == 'L') w = "A";
        else if (moves[i] == 'R') w = "D";
        size_t wl = strlen(w);
        if (pos + wl + 2 >= sizeof(text)) {
            break;
        }
        memcpy(text + pos, w, wl);
        pos += wl;
        if (i + 1 < len) {
            text[pos++] = ' ';
        }
    }
    text[pos] = '\0';
    return text;
}

#endif
