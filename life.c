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
    int *dirty_left = calloc((size_t)rows, sizeof(*dirty_left));
    int *dirty_right = calloc((size_t)rows, sizeof(*dirty_right));
    if (!cells || !previous || !dirty_left || !dirty_right) {
        free(cells);
        free(previous);
        free(dirty_left);
        free(dirty_right);
        return false;
    }
    life_free(life);
    life->columns = columns;
    life->rows = rows;
    life->stride = columns + 2;
    life->length = length;
    life->cells = cells;
    life->previous = previous;
    life->dirty_left = dirty_left;
    life->dirty_right = dirty_right;
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
    const uint64_t ones = UINT64_C(0x0101010101010101);
    const uint64_t low = UINT64_C(0x7f7f7f7f7f7f7f7f);
    for (int y = 1; y <= rows; ++y) {
        int left = columns, right = 0;
        int x = 1;
        /* Eight independent byte lanes in a plain uint64_t. Each neighbor
         * contributes 0 or 1 per byte; sums never carry into the next cell.
         * memcpy permits unaligned loads and works on either byte order. */
        for (; x + 7 <= columns; x += 8) {
            int i = y * stride + x;
            uint64_t value, neighbors = 0, word;
            memcpy(&value, a + i, 8);
            if (!fade) {
                const int offsets[] = {-stride - 1, -stride, -stride + 1,
                    -1, 1, stride - 1, stride, stride + 1};
                for (int k = 0; k < 8; ++k) {
                    memcpy(&word, a + i + offsets[k], 8);
                    neighbors += (word >> 5) & ones;
                }
            }
            uint64_t alive = (value >> 5) & ones;
            /* In each byte, (count XOR target) + 127 has its high bit clear
             * exactly when count == target. All values remain below 256. */
            uint64_t two = (~((neighbors ^ (ones * 2)) + low) >> 7) & ones;
            uint64_t three = (~((neighbors ^ (ones * 3)) + low) >> 7) & ones;
            uint64_t next = fade ? 0 : three | (alive & two);
            uint64_t live_mask = alive * 255, next_mask = next * 255;
            /* Saturating age (max 63) and decay (min 0), still per byte. */
            uint64_t age = value + ones;
            age -= (age >> 6) & ones;
            uint64_t decay = value - (((value + low) >> 7) & ones);
            word = (next_mask & ((age & live_mask) | (ones * LIFE_ALIVE & ~live_mask))) |
                (~next_mask & ((ones * trail & live_mask) | (decay & ~live_mask)));
            memcpy(b + i, &word, 8);
            if (word != value) {
                if (left == columns) left = x - 1;
                right = x + 7;
            }
            population += (unsigned)((next * ones) >> 56);
            changes += (unsigned)(((next ^ alive) * ones) >> 56);
            /* Preserve the original hash in memory order, including on big endian. */
            if (next) for (int k = 0; k < 8; ++k)
                if (b[i + k] >= LIFE_ALIVE)
                    hash = (hash ^ (uint64_t)(i + k)) * UINT64_C(1099511628211);
        }
        /* At most seven trailing cells, also covers worlds narrower than eight. */
        for (; x <= columns; ++x) {
            int i = y * stride + x;
            uint8_t value = a[i];
            bool alive = value >= LIFE_ALIVE;
            unsigned neighbors =
                (a[i - stride - 1] >= LIFE_ALIVE) + (a[i - stride] >= LIFE_ALIVE) +
                (a[i - stride + 1] >= LIFE_ALIVE) + (a[i - 1] >= LIFE_ALIVE) +
                (a[i + 1] >= LIFE_ALIVE) + (a[i + stride - 1] >= LIFE_ALIVE) +
                (a[i + stride] >= LIFE_ALIVE) + (a[i + stride + 1] >= LIFE_ALIVE);
            if (!fade && (neighbors == 3 || (alive && neighbors == 2))) {
                ++population;
                changes += !alive;
                b[i] = alive ? (value < LIFE_COLORS - 1 ? value + 1 : value) : LIFE_ALIVE;
                hash = (hash ^ (uint64_t)i) * UINT64_C(1099511628211);
            } else {
                changes += alive;
                b[i] = alive ? (uint8_t)trail : (value ? value - 1 : 0);
            }
            if (b[i] != value) {
                if (left == columns) left = x - 1;
                right = x;
            }
        }
        life->dirty_left[y - 1] = left;
        life->dirty_right[y - 1] = right;
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
    free(life->dirty_left);
    free(life->dirty_right);
    life->cells = life->previous = NULL;
    life->dirty_left = life->dirty_right = NULL;
    life->length = 0;
}
