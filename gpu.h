#ifndef GPU_H
#define GPU_H
#include "life.h"
#include <X11/Xlib.h>
#ifndef LIFE_HAVE_GPU
#define LIFE_HAVE_GPU 0
#endif
/* Optional GLX/OpenGL engine; CPU mode never creates an OpenGL context. */
struct gpu;
struct gpu_stats { unsigned population, changes, fresh; uint64_t hash; };
struct gpu *gpu_open(Display *display, Window window);
const char *gpu_error(void);
const char *gpu_renderer(const struct gpu *gpu);
void gpu_palette(struct gpu *gpu, const float *rgb);
bool gpu_seed(struct gpu *gpu, const struct life *life, const uint8_t *visited);
bool gpu_step(struct gpu *gpu, unsigned trail, bool fade, bool sample);
/* Observe a dispatched generation before issuing the next one. */
bool gpu_observe(struct gpu *gpu, struct gpu_stats *stats);
bool gpu_caption(struct gpu *gpu, Pixmap bitmap, int width, int height);
void gpu_draw(struct gpu *gpu, int width, int height, int grid_height, int size, int left, int top,
              bool rounded);
void gpu_close(struct gpu *gpu);
#endif
