#include "life.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *
cell(struct life *life, int x, int y)
{
    return &life->cells[(y + 1) * life->stride + x + 1];
}

/* Deliberately simple oracle: modulo boundaries, no halo or buffer swapping. */
static void
reference(uint8_t *board, int width, int height, unsigned trail, bool fade)
{
    uint8_t *next = calloc((size_t)width * height, 1);
    assert(next);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int neighbors = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx || dy)
                        neighbors += board[((y + dy + height) % height) * width +
                                           (x + dx + width) % width] >= LIFE_ALIVE;
            int i = y * width + x;
            unsigned value = board[i];
            bool alive = value >= LIFE_ALIVE;
            if (!fade && (neighbors == 3 || (alive && neighbors == 2)))
                next[i] = (uint8_t)(alive ? (value < 63 ? value + 1 : 63) : LIFE_ALIVE);
            else
                next[i] = (uint8_t)(alive ? trail : (value ? value - 1 : 0));
        }
    }
    memcpy(board, next, (size_t)width * height);
    free(next);
}

static void
compare_reference(int width, int height, unsigned trail)
{
    struct life life = {.random = 42};
    assert(life_resize(&life, width, height));
    life_seed(&life);
    uint8_t *board = malloc((size_t)width * height);
    assert(board);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            board[y * width + x] = *cell(&life, x, y);
    for (int step = 0; step < 200; ++step) {
        bool fade = step >= 160;
        reference(board, width, height, trail, fade);
        life_step(&life, trail, fade);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                assert(board[y * width + x] == *cell(&life, x, y));
    }
    free(board);
    life_free(&life);
}

static void
patterns(void)
{
    struct life life = {0};
    assert(life_resize(&life, 8, 8));
    *cell(&life, 3, 3) = *cell(&life, 4, 3) = LIFE_ALIVE;
    *cell(&life, 3, 4) = *cell(&life, 4, 4) = LIFE_ALIVE;
    life_step(&life, 18, false);
    uint64_t block_hash = life.hash;
    for (int i = 0; i < 100; ++i) {
        life_step(&life, 18, false);
        assert(life.hash == block_hash);
    }
    assert(*cell(&life, 3, 3) == LIFE_COLORS - 1);
    for (int i = 0; i < 20; ++i) life_step(&life, 18, true);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) assert(*cell(&life, x, y) == 0);

    *cell(&life, 2, 3) = *cell(&life, 3, 3) = *cell(&life, 4, 3) = LIFE_ALIVE;
    life_step(&life, 0, false);
    assert(*cell(&life, 3, 2) >= LIFE_ALIVE);
    assert(*cell(&life, 3, 3) >= LIFE_ALIVE);
    assert(*cell(&life, 3, 4) >= LIFE_ALIVE);
    assert(*cell(&life, 2, 3) == 0);
    life_step(&life, 0, false);
    assert(*cell(&life, 2, 3) >= LIFE_ALIVE);
    assert(*cell(&life, 4, 3) >= LIFE_ALIVE);

    assert(life_resize(&life, 8, 8));
    /* A glider crosses both seams after four generations. */
    const int glider[][2] = {{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}};
    for (int i = 0; i < 5; ++i)
        *cell(&life, (glider[i][0] + 6) % 8, (glider[i][1] + 6) % 8) = LIFE_ALIVE;
    for (int i = 0; i < 4; ++i) life_step(&life, 18, false);
    for (int i = 0; i < 5; ++i)
        assert(*cell(&life, (glider[i][0] + 7) % 8, (glider[i][1] + 7) % 8) >= LIFE_ALIVE);
    uint8_t *saved = life.cells;
    assert(!life_resize(&life, 0, 8));
    assert(!life_resize(&life, 32767, 32767));
    assert(life.cells == saved);
    life_free(&life);
    life_free(&life);
}

int
main(void)
{
    patterns();
    const int dimensions[][2] = {{1, 1}, {1, 19}, {19, 1}, {2, 2}, {3, 2}, {17, 13}, {127, 83}};
    const unsigned trails[] = {0, 18, 31};
    for (size_t i = 0; i < sizeof(dimensions) / sizeof(*dimensions); ++i)
        for (size_t j = 0; j < sizeof(trails) / sizeof(*trails); ++j)
            compare_reference(dimensions[i][0], dimensions[i][1], trails[j]);
    puts("Life: patterns, toroidal edges, trails, fading, resizing and 4,200 oracle comparisons passed.");
    return 0;
}
