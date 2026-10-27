#include "life.h"

#include <stdlib.h>
#include <string.h>

bool
life_resize(struct life *life, int columns, int rows)
{
    if (columns < 1 || rows < 1 || columns > 32767 || rows > 32767)
        return false;
    size_t length = (size_t)(columns + 2) * (size_t)(rows + 2);
    if (length > 4 * 1024 * 1024)
        return false;
    uint8_t *cells = calloc(length, 1);
    uint8_t *previous = calloc(length, 1);
    if (!cells || !previous) {
        free(cells);
        free(previous);
        return false;
    }
    life_free(life);
    life->columns = columns;
    life->rows = rows;
    life->stride = columns + 2;
    life->length = length;
    life->cells = cells;
    life->previous = previous;
    return true;
}

uint32_t
life_random(struct life *life)
{
    uint32_t x = life->random ? life->random : 1;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return life->random = x;
}

void
life_clear(struct life *life)
{
    memset(life->cells, 0, life->length);
    memset(life->previous, 0, life->length);
    life->hash = 0;
    life->population = life->changes = 0;
}

void
life_seed(struct life *life)
{
    life_clear(life);
    for (int y = 1; y <= life->rows; ++y)
        for (int x = 1; x <= life->columns; ++x)
            life->cells[y * life->stride + x] =
                life_random(life) % 100 < 30 ? LIFE_ALIVE : 0;
}

void
life_step(struct life *life, unsigned trail, bool fade)
{
    uint8_t *a = life->cells, *b = life->previous;
    int stride = life->stride, columns = life->columns, rows = life->rows;
    if (trail >= LIFE_ALIVE)
        trail = LIFE_ALIVE - 1;

    /* A one-cell halo wraps the world without modulo in the inner loop. */
    for (int y = 1; y <= rows; ++y) {
        a[y * stride] = a[y * stride + columns];
        a[y * stride + columns + 1] = a[y * stride + 1];
    }
    memcpy(a, a + rows * stride, (size_t)stride);
    memcpy(a + (rows + 1) * stride, a + stride, (size_t)stride);

    uint64_t hash = LIFE_EMPTY_HASH;
    unsigned population = 0, changes = 0;
    for (int y = 1; y <= rows; ++y) {
        /* Reuse vertical sums as the three-column window slides right. */
        int first = y * stride;
        unsigned left = (a[first - stride] >= LIFE_ALIVE) +
            (a[first] >= LIFE_ALIVE) + (a[first + stride] >= LIFE_ALIVE);
        unsigned middle = (a[first - stride + 1] >= LIFE_ALIVE) +
            (a[first + 1] >= LIFE_ALIVE) + (a[first + stride + 1] >= LIFE_ALIVE);
        for (int x = 1; x <= columns; ++x) {
            int i = y * stride + x;
            uint8_t value = a[i];
            bool alive = value >= LIFE_ALIVE;
            unsigned right = (a[i - stride + 1] >= LIFE_ALIVE) +
                (a[i + 1] >= LIFE_ALIVE) +
                (a[i + stride + 1] >= LIFE_ALIVE);
            unsigned neighbors = left + middle + right - alive;
            left = middle;
            middle = right;
            if (!fade && (neighbors == 3 || (alive && neighbors == 2))) {
                ++population;
                changes += !alive;
                b[i] = alive ? (value < LIFE_COLORS - 1 ? value + 1 : value)
                             : LIFE_ALIVE;
                /* Hash occupancy only: age and trails do not affect Life. */
                hash = (hash ^ (uint64_t)i) * UINT64_C(1099511628211);
            } else {
                changes += alive;
                b[i] = alive ? (uint8_t)trail : (value ? value - 1 : 0);
            }
        }
    }
    life->cells = b;
    life->previous = a;
    life->hash = hash;
    life->population = population;
    life->changes = changes;
}

void
life_free(struct life *life)
{
    free(life->cells);
    free(life->previous);
    life->cells = life->previous = NULL;
    life->length = 0;
}
