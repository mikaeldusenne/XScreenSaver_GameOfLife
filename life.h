#ifndef LIFE_H
#define LIFE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Dead cells carry a fading trail; living cells carry their age. */
enum { LIFE_ALIVE = 32, LIFE_COLORS = 64 };
#define LIFE_EMPTY_HASH UINT64_C(14695981039346656037)

struct life {
    int columns, rows, stride;
    size_t length;
    uint8_t *cells, *previous;
    /* Conservative changed span per row, [left, right); empty when left >= right. */
    int *dirty_left, *dirty_right;
    uint32_t random;
    uint64_t hash;
    unsigned population, changes;
};

bool life_resize(struct life *life, int columns, int rows);
uint32_t life_random(struct life *life);
void life_clear(struct life *life);
void life_seed(struct life *life);
void life_step(struct life *life, unsigned trail, bool fade);
void life_free(struct life *life);

#endif
