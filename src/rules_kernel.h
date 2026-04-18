#ifndef RULES_KERNEL_H
#define RULES_KERNEL_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef void (*rules_on_shift_cb)(int src_x, int src_y, int dst_x, int dst_y, void *user);

static inline bool rules_in_bounds(int width, int height, int x, int y)
{
    return x >= 0 && x < width && y >= 0 && y < height;
}

static inline int rules_idx(int width, int x, int y)
{
    return y * width + x;
}

static inline bool rules_static_is_activatable(StaticTileType type)
{
    return type == STATIC_TILE_PORTAL || type == STATIC_TILE_FIXED_REPEATER || type == STATIC_TILE_FIXED_NOT_SIGNAL;
}

static inline bool rules_dynamic_is_activatable(DynamicTileType type)
{
    return type == DYNAMIC_TILE_REPEATER || type == DYNAMIC_TILE_NOT_SIGNAL;
}

static inline bool rules_cell_has_not_signal(
    const StaticTileType *static_tiles,
    const DynamicTileType *dynamic_types,
    int idx)
{
    return static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL || dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL;
}

static inline bool rules_cell_can_emit_signal(
    const StaticTileType *static_tiles,
    const DynamicTileType *dynamic_types,
    const bool *static_activated,
    const bool *dynamic_activated,
    int idx)
{
    if (static_tiles[idx] == STATIC_TILE_FIXED_SIGNAL_SOURCE) {
        return true;
    }
    if (dynamic_types[idx] == DYNAMIC_TILE_SIGNAL_SOURCE) {
        return true;
    }
    if (static_tiles[idx] == STATIC_TILE_FIXED_REPEATER && static_activated[idx]) {
        return true;
    }
    if (dynamic_types[idx] == DYNAMIC_TILE_REPEATER && dynamic_activated[idx]) {
        return true;
    }
    if (static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL && !static_activated[idx]) {
        return true;
    }
    if (dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL && !dynamic_activated[idx]) {
        return true;
    }
    return false;
}

static inline bool rules_try_push_chain(
    int width,
    int height,
    const StaticTileType *static_tiles,
    DynamicTileType *dynamic_types,
    bool *dynamic_activated,
    int start_x,
    int start_y,
    int dx,
    int dy,
    rules_on_shift_cb on_shift,
    void *on_shift_user)
{
    int cursor_x = start_x;
    int cursor_y = start_y;
    if (!rules_in_bounds(width, height, cursor_x, cursor_y)) {
        return false;
    }

    int cursor_idx = rules_idx(width, cursor_x, cursor_y);
    if (dynamic_types[cursor_idx] == DYNAMIC_TILE_NONE) {
        return false;
    }

    while (rules_in_bounds(width, height, cursor_x, cursor_y)
           && dynamic_types[rules_idx(width, cursor_x, cursor_y)] != DYNAMIC_TILE_NONE) {
        cursor_x += dx;
        cursor_y += dy;
    }

    if (!rules_in_bounds(width, height, cursor_x, cursor_y)) {
        return false;
    }

    int target_idx = rules_idx(width, cursor_x, cursor_y);
    if (static_tiles[target_idx] != STATIC_TILE_EMPTY) {
        return false;
    }

    for (int x = cursor_x - dx, y = cursor_y - dy; x != start_x - dx || y != start_y - dy; x -= dx, y -= dy) {
        int src_idx = rules_idx(width, x, y);
        int dst_x = x + dx;
        int dst_y = y + dy;
        int dst_idx = rules_idx(width, dst_x, dst_y);

        dynamic_types[dst_idx] = dynamic_types[src_idx];
        dynamic_activated[dst_idx] = dynamic_activated[src_idx];
        if (dynamic_types[dst_idx] == DYNAMIC_TILE_NOT_SIGNAL) {
            dynamic_activated[dst_idx] = true;
        }
        dynamic_types[src_idx] = DYNAMIC_TILE_NONE;
        dynamic_activated[src_idx] = false;

        if (on_shift) {
            on_shift(x, y, dst_x, dst_y, on_shift_user);
        }
    }

    return true;
}

static inline void rules_recalculate_activation(
    int width,
    int height,
    const StaticTileType *static_tiles,
    DynamicTileType *dynamic_types,
    bool *static_activated,
    bool *dynamic_activated)
{
    int cell_count = width * height;
    bool *prev_static_activated = (bool *)malloc(sizeof(bool) * cell_count);
    bool *prev_dynamic_activated = (bool *)malloc(sizeof(bool) * cell_count);
    bool *const_emitter = (bool *)calloc(cell_count, sizeof(bool));
    bool *const_hits_not = (bool *)calloc(cell_count, sizeof(bool));
    bool *force_not_off = (bool *)calloc(cell_count, sizeof(bool));
    bool *emit_curr = (bool *)calloc(cell_count, sizeof(bool));
    bool *emit_next = (bool *)calloc(cell_count, sizeof(bool));
    bool *is_variable = (bool *)calloc(cell_count, sizeof(bool));
    int *component_id = (int *)malloc(sizeof(int) * cell_count);
    int *queue = (int *)malloc(sizeof(int) * cell_count);
    unsigned char *history = (unsigned char *)calloc(24 * cell_count, sizeof(unsigned char));
    int *component_size = (int *)calloc(cell_count, sizeof(int));
    int *component_hit_count = (int *)calloc(cell_count, sizeof(int));

    if (!prev_static_activated || !prev_dynamic_activated || !const_emitter || !const_hits_not || !force_not_off
        || !emit_curr || !emit_next || !is_variable || !component_id || !queue || !history
        || !component_size || !component_hit_count) {
        free(prev_static_activated);
        free(prev_dynamic_activated);
        free(const_emitter);
        free(const_hits_not);
        free(force_not_off);
        free(emit_curr);
        free(emit_next);
        free(is_variable);
        free(component_id);
        free(queue);
        free(history);
        free(component_size);
        free(component_hit_count);
        return;
    }

    memcpy(prev_static_activated, static_activated, sizeof(bool) * cell_count);
    memcpy(prev_dynamic_activated, dynamic_activated, sizeof(bool) * cell_count);

    for (int idx = 0; idx < cell_count; ++idx) {
        if (rules_static_is_activatable(static_tiles[idx])) {
            static_activated[idx] = false;
        }
        if (rules_dynamic_is_activatable(dynamic_types[idx])) {
            dynamic_activated[idx] = false;
        }
        component_id[idx] = -1;
    }

    int q_head = 0;
    int q_tail = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = rules_idx(width, x, y);
            if (static_tiles[idx] == STATIC_TILE_FIXED_SIGNAL_SOURCE || dynamic_types[idx] == DYNAMIC_TILE_SIGNAL_SOURCE) {
                if (!const_emitter[idx]) {
                    const_emitter[idx] = true;
                    queue[q_tail++] = idx;
                }
            }
        }
    }

    while (q_head < q_tail) {
        int emitter_idx = queue[q_head++];
        int ex = emitter_idx % width;
        int ey = emitter_idx / width;

        for (int dy = -3; dy <= 3; ++dy) {
            int max_abs_dx = 3 - abs(dy);
            for (int dx = -max_abs_dx; dx <= max_abs_dx; ++dx) {
                int tx = ex + dx;
                int ty = ey + dy;
                if (!rules_in_bounds(width, height, tx, ty)) {
                    continue;
                }
                int idx = rules_idx(width, tx, ty);
                if (static_tiles[idx] == STATIC_TILE_PORTAL) {
                    static_activated[idx] = true;
                }
                if (static_tiles[idx] == STATIC_TILE_FIXED_REPEATER && !const_emitter[idx]) {
                    const_emitter[idx] = true;
                    queue[q_tail++] = idx;
                }
                if (dynamic_types[idx] == DYNAMIC_TILE_REPEATER && !const_emitter[idx]) {
                    const_emitter[idx] = true;
                    queue[q_tail++] = idx;
                }
                if (rules_cell_has_not_signal(static_tiles, dynamic_types, idx)) {
                    const_hits_not[idx] = true;
                }
            }
        }
    }

    int component_count = 0;
    for (int idx = 0; idx < cell_count; ++idx) {
        if (!rules_cell_has_not_signal(static_tiles, dynamic_types, idx) || component_id[idx] >= 0) {
            continue;
        }

        int local_head = 0;
        int local_tail = 0;
        queue[local_tail++] = idx;
        component_id[idx] = component_count;

        while (local_head < local_tail) {
            int current = queue[local_head++];
            int cx = current % width;
            int cy = current / width;
            component_size[component_count]++;
            if (const_hits_not[current]) {
                component_hit_count[component_count]++;
            }

            for (int other = 0; other < cell_count; ++other) {
                if (component_id[other] >= 0 || !rules_cell_has_not_signal(static_tiles, dynamic_types, other)) {
                    continue;
                }
                int ox = other % width;
                int oy = other / width;
                if (abs(cx - ox) + abs(cy - oy) <= 3) {
                    component_id[other] = component_count;
                    queue[local_tail++] = other;
                }
            }
        }
        component_count++;
    }

    for (int idx = 0; idx < cell_count; ++idx) {
        if (!rules_cell_has_not_signal(static_tiles, dynamic_types, idx)) {
            continue;
        }
        int cid = component_id[idx];
        if (cid < 0) {
            continue;
        }
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
        bool has_static_repeater = static_tiles[idx] == STATIC_TILE_FIXED_REPEATER;
        bool has_dynamic_repeater = dynamic_types[idx] == DYNAMIC_TILE_REPEATER;
        bool has_static_not = static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL;
        bool has_dynamic_not = dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL;
        bool has_signal = static_tiles[idx] == STATIC_TILE_FIXED_SIGNAL_SOURCE || dynamic_types[idx] == DYNAMIC_TILE_SIGNAL_SOURCE;

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
            int x = idx % width;
            int y = idx / width;
            bool has_input = false;
            for (int dy = -3; dy <= 3 && !has_input; ++dy) {
                int max_abs_dx = 3 - abs(dy);
                for (int dx = -max_abs_dx; dx <= max_abs_dx; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    int tx = x + dx;
                    int ty = y + dy;
                    if (!rules_in_bounds(width, height, tx, ty)) {
                        continue;
                    }
                    int src = rules_idx(width, tx, ty);
                    if (emit_curr[src]) {
                        has_input = true;
                        break;
                    }
                }
            }

            if (static_tiles[idx] == STATIC_TILE_FIXED_REPEATER || dynamic_types[idx] == DYNAMIC_TILE_REPEATER) {
                emit_next[idx] = has_input;
            } else if (static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL || dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL) {
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
        if (static_tiles[idx] == STATIC_TILE_PORTAL) {
            static_activated[idx] = false;
        }
        if (static_tiles[idx] == STATIC_TILE_FIXED_REPEATER) {
            static_activated[idx] = emit_curr[idx];
        } else if (dynamic_types[idx] == DYNAMIC_TILE_REPEATER) {
            dynamic_activated[idx] = emit_curr[idx];
        }

        if (static_tiles[idx] == STATIC_TILE_FIXED_NOT_SIGNAL) {
            static_activated[idx] = !emit_curr[idx];
        } else if (dynamic_types[idx] == DYNAMIC_TILE_NOT_SIGNAL) {
            dynamic_activated[idx] = !emit_curr[idx];
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = rules_idx(width, x, y);
            if (static_tiles[idx] != STATIC_TILE_PORTAL) {
                continue;
            }
            bool on = false;
            for (int dy = -3; dy <= 3 && !on; ++dy) {
                int max_abs_dx = 3 - abs(dy);
                for (int dx = -max_abs_dx; dx <= max_abs_dx; ++dx) {
                    int tx = x + dx;
                    int ty = y + dy;
                    if (!rules_in_bounds(width, height, tx, ty)) {
                        continue;
                    }
                    int src = rules_idx(width, tx, ty);
                    if (emit_curr[src]) {
                        on = true;
                        break;
                    }
                }
            }
            static_activated[idx] = on;
        }
    }

    free(prev_static_activated);
    free(prev_dynamic_activated);
    free(const_emitter);
    free(const_hits_not);
    free(force_not_off);
    free(emit_curr);
    free(emit_next);
    free(is_variable);
    free(component_id);
    free(queue);
    free(history);
    free(component_size);
    free(component_hit_count);
}

#endif
