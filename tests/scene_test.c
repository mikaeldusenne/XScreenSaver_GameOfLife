#include "scene.h"

#include <assert.h>
#include <stdio.h>

static void
start(struct life *life, struct scene *scene, const char *name, bool single, unsigned seconds)
{
    life_clear(life);
    const struct figure *figure = figure_find(name);
    assert(figure);
    assert(figure_stamp(life, figure, 40, 40, 0));
    assert(scene_reset(scene, life, single ? figure : NULL, 15, seconds));
}

static unsigned
finish(struct life *life, struct scene *scene, unsigned limit)
{
    for (unsigned generation = 1; generation <= limit; ++generation) {
        life_step(life, 18, false);
        if (scene_finished(scene, life)) return generation;
    }
    return 0;
}

int
main(void)
{
    struct life life = {0};
    struct scene scene = {0};
    assert(life_resize(&life, 320, 180));

    start(&life, &scene, "block", true, 0);
    assert(finish(&life, &scene, 151) == 150);
    assert(life.population == 4 && life.changes == 0);
    start(&life, &scene, "blinker", true, 0);
    assert(finish(&life, &scene, 181) == 180);
    assert(life.population == 3 && life.changes == 4);
    start(&life, &scene, "104p177", true, 0);
    assert(finish(&life, &scene, 709) == 708); /* Four complete long cycles. */

    start(&life, &scene, "block", true, 20);
    assert(finish(&life, &scene, 301) == 300); /* Explicit duration overrides adaptation. */
    start(&life, &scene, "diehard", true, 0);
    assert(finish(&life, &scene, 200) == 159); /* 130 generations + 2 empty seconds. */

    start(&life, &scene, "block", false, 0);
    assert(finish(&life, &scene, 226) == 225); /* Random and mixed worlds use inactivity. */
    start(&life, &scene, "glider", false, 0);
    assert(!finish(&life, &scene, 450)); /* Moving into new space is still activity. */

    start(&life, &scene, "worker-bee", false, 0);
    assert(figure_stamp(&life, figure_find("tumbler"), 100, 40, 0));
    assert(figure_stamp(&life, figure_find("octagon-2"), 160, 40, 0));
    assert(scene_reset(&scene, &life, NULL, 15, 0));
    /* Their joint period is 630, beyond the hash history; stale exploration
     * must still retire the world without waiting for the five-minute ceiling. */
    unsigned settled = finish(&life, &scene, 1350);
    assert(settled >= 225 && settled < 1350);

    start(&life, &scene, "simkin-glider-gun", true, 0);
    assert(!finish(&life, &scene, 1349)); /* At least 90 seconds for production. */
    start(&life, &scene, "bunnies", true, 0);
    assert(!finish(&life, &scene, 1350)); /* Do not cut off the initial expansion. */

    const char *names[] = {"block", "pulsar", "104p177", "bunnies", "simkin-glider-gun", "gunstar-2"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(*names); ++i) {
        const struct figure *f = figure_find(names[i]);
        int size = scene_cell_size(1920, 1054, 0, f);
        assert((1920 / size) * (1054 / size) <= 64000);
        assert(1920 / size >= f->width + 24);
        assert(1054 / size >= f->height + 24);
        size = scene_cell_size(960, 574, 128, f);
        assert(960 / size >= f->width + 24 && 574 / size >= f->height + 24);
        assert(scene_cell_size(1, 1, 0, f) == 1);
    }
    int size = scene_cell_size(1920, 1054, 0, figure_find("bunnies"));
    assert(1920 / size >= 256 && 1054 / size >= 144);
    scene_free(&scene);
    life_free(&life);
    puts("Scenes: adaptive cycles, production, expansion, inactivity, manual duration and breathing room passed.");
    return 0;
}
