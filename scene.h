#ifndef SCENE_H
#define SCENE_H

#include "figures.h"

enum { SCENE_HISTORY = 256 };

struct scene {
    uint8_t *visited;
    size_t length;
    uint64_t history[SCENE_HISTORY];
    unsigned generation, repeated, empty, stale, changes;
    unsigned minimum, maximum, fixed, fps, idle_seconds;
    bool compact;
};

/* Automatic zoom leaves room to evolve; an explicit size still fits the seed. */
int scene_cell_size(int width, int height, int requested, const struct figure *figure);
bool scene_reset(struct scene *scene, const struct life *life,
                 const struct figure *figure, unsigned fps, unsigned seconds);
/* Call once per simulated generation. Hidden windows do not consume this budget. */
bool scene_finished(struct scene *scene, const struct life *life);
void scene_free(struct scene *scene);

#endif
