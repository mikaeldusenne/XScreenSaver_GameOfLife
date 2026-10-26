#include "figures.h"

#include <string.h>

/* Canonical seeds in compact RLE: o = alive, b = dead, $ = next row.
 * Only these built-in strings are decoded; no file or network access.
 * Background and references: FIGURES.md. */
const struct figure figures[] = {
#include "figures_data.inc"
};

const size_t figure_count = sizeof(figures) / sizeof(*figures);

const struct figure *
figure_find(const char *name)
{
    for (size_t i = 0; i < figure_count; ++i)
        if (!strcmp(name, figures[i].name)) return &figures[i];
    return NULL;
}

bool
figure_stamp(struct life *life, const struct figure *figure,
             int left, int top, unsigned transform)
{
    int width = transform & 1 ? figure->height : figure->width;
    int height = transform & 1 ? figure->width : figure->height;
    if (left < 0 || top < 0 || left > life->columns - width || top > life->rows - height)
        return false;
    int x = 0, y = 0, run = 0;
    for (const char *p = figure->rle; *p && *p != '!'; ++p) {
        if (*p >= '0' && *p <= '9') {
            run = run * 10 + (*p - '0');
            continue;
        }
        if (!run) run = 1;
        if (*p == '$') {
            y += run;
            x = 0;
        } else {
            for (int i = 0; i < run; ++i, ++x) {
                if (x >= figure->width || y >= figure->height) return false;
                if (*p != 'o') continue;
                int u = transform & 4 ? figure->width - 1 - x : x;
                int tx = u, ty = y;
                switch (transform & 3) {
                case 1: tx = figure->height - 1 - y; ty = u; break;
                case 2: tx = figure->width - 1 - u; ty = figure->height - 1 - y; break;
                case 3: tx = y; ty = figure->width - 1 - u; break;
                }
                life->cells[(top + ty + 1) * life->stride + left + tx + 1] = LIFE_ALIVE;
            }
        }
        run = 0;
    }
    return true;
}

static bool
empty_area(const struct life *life, int left, int top, int width, int height)
{
    for (int y = top; y < top + height; ++y)
        for (int x = left; x < left + width; ++x)
            if (life->cells[(y + 1) * life->stride + x + 1]) return false;
    return true;
}

unsigned
figures_scatter(struct life *life)
{
    enum { GAP = 6, MAX_FIGURES = 128 };
    life_clear(life);
    unsigned wanted = (unsigned)(life->columns * life->rows) / 1200;
    if (!wanted) wanted = 1;
    if (wanted > MAX_FIGURES) wanted = MAX_FIGURES;
    unsigned placed = 0;
    unsigned budget = (unsigned)(life->columns * life->rows) / 20;
    if (budget < 4) budget = 4;
    /* Bounded rejection sampling: no overlap, six-cell breathing room. */
    for (unsigned attempt = 0; attempt < wanted * 20 && placed < wanted; ++attempt) {
        const struct figure *f = &figures[life_random(life) % figure_count];
        if (f->population > budget) continue;
        unsigned transform = life_random(life) % 8;
        int width = transform & 1 ? f->height : f->width;
        int height = transform & 1 ? f->width : f->height;
        if (width + 2 * GAP > life->columns || height + 2 * GAP > life->rows) continue;
        int left = GAP + (int)(life_random(life) % (unsigned)(life->columns - width - 2 * GAP + 1));
        int top = GAP + (int)(life_random(life) % (unsigned)(life->rows - height - 2 * GAP + 1));
        if (!empty_area(life, left - GAP, top - GAP, width + 2 * GAP, height + 2 * GAP)) continue;
        figure_stamp(life, f, left, top, transform);
        budget -= f->population;
        ++placed;
    }
    /* Tiny previews still get one whole figure if any can fit. */
    if (!placed) {
        size_t start = life_random(life) % figure_count;
        for (size_t i = 0; i < figure_count; ++i) {
            const struct figure *f = &figures[(start + i) % figure_count];
            if (f->population > budget) continue;
            if (figure_stamp(life, f, (life->columns - f->width) / 2,
                             (life->rows - f->height) / 2, 0)) return 1;
        }
    }
    return placed;
}
