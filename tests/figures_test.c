#include "figures.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct reference {
    const char *name;
    uint64_t hash16;
    unsigned cells, period;
    int dx, dy, left, top, right, bottom;
};

static const struct reference references[] = {
#include "figures_expected.inc"
};

static unsigned
population(const struct life *life)
{
    unsigned count = 0;
    for (int y = 1; y <= life->rows; ++y)
        for (int x = 1; x <= life->columns; ++x)
            count += life->cells[y * life->stride + x] >= LIFE_ALIVE;
    return count;
}

static void
same_occupancy(const struct life *a, const struct life *b)
{
    assert(a->length == b->length);
    for (int y = 1; y <= a->rows; ++y)
        for (int x = 1; x <= a->columns; ++x) {
            int i = y * a->stride + x;
            assert((a->cells[i] >= LIFE_ALIVE) == (b->cells[i] >= LIFE_ALIVE));
        }
}

static void
library(void)
{
    assert(figure_count == sizeof(references) / sizeof(*references));
    assert(!figure_find("not-a-figure"));
    struct life life = {0};
    assert(life_resize(&life, 128, 128));
    for (size_t i = 0; i < figure_count; ++i) {
        const struct figure *f = &figures[i];
        assert(!strcmp(f->name, references[i].name));
        assert(f->period == references[i].period);
        assert(figure_find(f->name) == f);
        for (unsigned transform = 0; transform < 8; ++transform) {
            life_clear(&life);
            int width = transform & 1 ? f->height : f->width;
            int height = transform & 1 ? f->width : f->height;
            assert(figure_stamp(&life, f, 128 - width, 128 - height, transform));
            assert(population(&life) == references[i].cells);
            assert(population(&life) == f->population);
            for (int y = 0; y < 128; ++y)
                for (int x = 0; x < 128; ++x)
                    if (life.cells[(y + 1) * life.stride + x + 1])
                        assert(x >= 128 - width && y >= 128 - height);
            life_clear(&life);
            assert(!figure_stamp(&life, f, 129 - width, 128 - height, transform));
            assert(!figure_stamp(&life, f, -1, 0, transform));
            assert(population(&life) == 0);
        }
    }
    life_free(&life);
}

static void
evolution(void)
{
    struct life actual = {0}, expected = {0};
    assert(life_resize(&actual, 128, 128));
    assert(life_resize(&expected, 128, 128));
    /* Every seed is compared to an independent unbounded B3/S23 oracle. */
    for (size_t i = 0; i < figure_count; ++i) {
        const struct reference *ref = &references[i];
        const struct figure *f = figure_find(ref->name);
        assert(life_resize(&actual, 256, 256));
        assert(figure_stamp(&actual, f, 64, 64, 0));
        for (int step = 0; step < 16; ++step) life_step(&actual, 18, false);
        if (actual.hash != ref->hash16) fprintf(stderr, "Oracle mismatch: %s\n", ref->name);
        assert(actual.hash == ref->hash16);
        if (!ref->period) continue;
        /* Recorded envelope includes every phase and the complete displacement. */
        int width = ref->right - ref->left + 8, height = ref->bottom - ref->top + 8;
        int left = 4 - ref->left, top = 4 - ref->top;
        assert(life_resize(&actual, width, height));
        assert(life_resize(&expected, width, height));
        assert(figure_stamp(&actual, f, left, top, 0));
        assert(figure_stamp(&expected, f, left + ref->dx, top + ref->dy, 0));
        for (unsigned step = 0; step < ref->period; ++step) life_step(&actual, 18, false);
        same_occupancy(&actual, &expected);
    }
    assert(life_resize(&actual, 128, 128));
    life_clear(&actual);
    assert(figure_stamp(&actual, figure_find("gosper-glider-gun"), 40, 40, 0));
    for (int cycle = 1; cycle <= 3; ++cycle) {
        for (int step = 0; step < 30; ++step) life_step(&actual, 18, false);
        assert(population(&actual) == 36 + 5 * (unsigned)cycle);
    }
    life_clear(&actual);
    assert(figure_stamp(&actual, figure_find("diehard"), 40, 40, 0));
    for (int step = 0; step < 129; ++step) life_step(&actual, 18, false);
    assert(population(&actual) > 0);
    life_step(&actual, 18, false);
    assert(population(&actual) == 0);
    life_free(&actual);
    life_free(&expected);
}

static void
scatter(void)
{
    struct life a = {.random = 42}, b = {.random = 42};
    assert(life_resize(&a, 320, 180));
    assert(life_resize(&b, 320, 180));
    unsigned count = figures_scatter(&a);
    assert(count > 10 && count <= 48);
    assert(figures_scatter(&b) == count);
    assert(!memcmp(a.cells, b.cells, a.length));
    assert(population(&a) >= count);
    assert(population(&a) <= 320 * 180 / 20);
    for (int y = 0; y < 180; ++y)
        for (int x = 0; x < 320; ++x)
            if (x < 6 || y < 6 || x >= 314 || y >= 174)
                assert(a.cells[(y + 1) * a.stride + x + 1] == 0);
    b.random = 2026;
    figures_scatter(&b);
    assert(memcmp(a.cells, b.cells, a.length));
    /* Dense and large additions must respect the same initial density budget. */
    for (unsigned seed = 1; seed <= 64; ++seed) {
        a.random = seed;
        assert(figures_scatter(&a) > 0);
        assert(population(&a) <= 320 * 180 / 20);
    }
    assert(life_resize(&a, 2, 2));
    assert(figures_scatter(&a) == 1 && population(&a) > 0 && population(&a) <= 4);
    assert(life_resize(&a, 1, 1));
    assert(figures_scatter(&a) == 0 && population(&a) == 0);
    life_free(&a);
    life_free(&b);
}

int
main(void)
{
    library();
    evolution();
    scatter();
    printf("Figures: %zu seeds, 8 symmetries, independent 16-generation oracle, full periods/speeds, gun, diehard and sparse deterministic placement passed.\n",
           figure_count);
    return 0;
}
