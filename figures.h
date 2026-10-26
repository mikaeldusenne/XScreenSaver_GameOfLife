#ifndef FIGURES_H
#define FIGURES_H

#include "life.h"

struct figure {
    const char *name, *kind;
    int width, height;
    unsigned population, period;
    const char *rle;
};

extern const struct figure figures[];
extern const size_t figure_count;

const struct figure *figure_find(const char *name);
/* Stamp one complete figure, without wrapping; 0..7 selects its symmetry. */
bool figure_stamp(struct life *life, const struct figure *figure,
                  int left, int top, unsigned transform);
/* Clear the world and scatter separated figures; return their count. */
unsigned figures_scatter(struct life *life);

#endif
