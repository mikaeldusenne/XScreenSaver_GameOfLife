#include "scene.h"

#include <stdlib.h>
#include <string.h>

static int max(int a, int b) { return a > b ? a : b; }
static int min(int a, int b) { return a < b ? a : b; }

static bool
kind(const struct figure *figure, const char *name)
{
    return figure && !strcmp(figure->kind, name);
}

int
scene_random_cell_size(int width, int height, int requested)
{
    if (requested) return requested;
    int size = 2;
    while ((width / size) * (height / size) > 256000) ++size;
    return size;
}

int
scene_cell_size(int width, int height, int requested, const struct figure *figure)
{
    int cheap = 4;
    while ((width / cheap) * (height / cheap) > 64000) ++cheap;
    int size = requested ? requested : cheap;
    if (!figure) return size;
    if (!requested) {
        int columns, rows, zoom;
        if (kind(figure, "still life") || kind(figure, "oscillator")) {
            columns = figure->width + max(24, figure->width);
            rows = figure->height + max(24, figure->height);
            zoom = 32;
        } else if (kind(figure, "spaceship")) {
            columns = max(96, figure->width + 64);
            rows = max(80, figure->height + 64);
            zoom = 24;
        } else if (kind(figure, "gun") || kind(figure, "puffer") || kind(figure, "rake")) {
            columns = max(200, figure->width + 96);
            rows = max(120, figure->height + 96);
            zoom = 16;
        } else {
            columns = max(256, figure->width + 96);
            rows = max(144, figure->height + 96);
            zoom = 12;
        }
        size = max(cheap, min(zoom, min(width / columns, height / rows)));
    }
    /* Even a requested close-up keeps twelve cells free on every side. */
    size = min(size, min(width / (figure->width + 24), height / (figure->height + 24)));
    return max(1, size);
}

bool
scene_reset(struct scene *scene, const struct life *life,
            const struct figure *figure, unsigned fps, unsigned seconds)
{
    if (scene->length != life->length) {
        uint8_t *visited = realloc(scene->visited, life->length);
        if (!visited) return false;
        scene->visited = visited;
        scene->length = life->length;
    }
    memset(scene->visited, 0, scene->length);
    memset(scene->history, 0, sizeof(scene->history));
    scene->generation = scene->repeated = scene->empty = scene->stale = scene->changes = 0;
    for (int y = 1; y <= life->rows; ++y)
        for (int x = 1; x <= life->columns; ++x) {
            int i = y * life->stride + x;
            if (life->cells[i] >= LIFE_ALIVE) {
                scene->visited[i] = 1;
            }
        }
    scene->fps = fps;
    scene->fixed = seconds * fps;
    scene->minimum = 15 * fps;
    scene->maximum = 300 * fps;
    scene->idle_seconds = 20;
    scene->compact = kind(figure, "still life") || kind(figure, "oscillator");
    if (scene->compact) {
        scene->minimum = kind(figure, "still life") ? 10 * fps : 12 * fps;
        if (figure->period * 4 > scene->minimum) scene->minimum = figure->period * 4;
        scene->maximum = scene->minimum;
    } else if (kind(figure, "spaceship")) {
        scene->minimum = 20 * fps;
        scene->maximum = 60 * fps;
        if (figure->period * 4 > scene->maximum) scene->maximum = figure->period * 4;
    } else if (kind(figure, "gun") || kind(figure, "puffer") || kind(figure, "rake")) {
        scene->minimum = 90 * fps;
    } else if (figure) {
        scene->minimum = (unsigned)max(512, 30 * (int)fps);
        scene->maximum = 1200 * fps;
        scene->idle_seconds = 30;
    }
    return true;
}

bool
scene_finished(struct scene *scene, const struct life *life)
{
    bool repeated = false;
    unsigned count = scene->generation < SCENE_HISTORY ? scene->generation : SCENE_HISTORY;
    for (unsigned i = 0; i < count; ++i)
        if (scene->history[i] == life->hash) repeated = true;
    scene->history[scene->generation % SCENE_HISTORY] = life->hash;
    scene->repeated = repeated ? scene->repeated + 1 : 0;
    scene->empty = life->population ? 0 : scene->empty + 1;
    ++scene->generation;
    if (scene->empty >= scene->fps * 2) return true;
    if (scene->fixed) return scene->generation >= scene->fixed;
    if (scene->generation >= scene->maximum) return true;
    scene->changes += life->changes;

    /* Once per second, detect exploration and low activity. This ignores
     * trails and needs no extra work in the per-cell simulation loop. */
    if (scene->generation % scene->fps == 0) {
        unsigned fresh = 0;
        for (int y = 1; y <= life->rows; ++y)
            for (int x = 1; x <= life->columns; ++x) {
                int i = y * life->stride + x;
                if (life->cells[i] >= LIFE_ALIVE && !scene->visited[i]) {
                    scene->visited[i] = 1;
                    ++fresh;
                }
            }
        unsigned tolerance = (unsigned)max(2, (int)life->population / 100);
        unsigned low_activity = (unsigned)max(8, life->columns * life->rows / 100);
        bool quiet = fresh <= tolerance && scene->changes / scene->fps <= low_activity;
        scene->stale = quiet ? scene->stale + 1 : 0;
        scene->changes = 0;
    }
    return scene->generation >= scene->minimum && !scene->compact &&
        (scene->repeated >= scene->fps * 8 || scene->stale >= scene->idle_seconds);
}

void
scene_free(struct scene *scene)
{
    free(scene->visited);
    scene->visited = NULL;
    scene->length = 0;
}
