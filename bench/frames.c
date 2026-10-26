/* Benchmark-only instrumentation. The shipped binary never links this file. */
#define _POSIX_C_SOURCE 200809L
#undef XFlush
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int
bench_flush(Display *display)
{
    static FILE *frames;
    if (!frames) {
        const char *path = getenv("LIFE_BENCH_FRAMES");
        frames = path ? fopen(path, "wb") : NULL;
        if (!frames) exit(EXIT_FAILURE);
    }
    int result = XFlush(display);
    /* Include server processing; do not mistake a growing X queue for FPS. */
    XSync(display, False);
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    double timestamp = (double)t.tv_sec + (double)t.tv_nsec / 1e9;
    if (fwrite(&timestamp, sizeof(timestamp), 1, frames) != 1 || fflush(frames))
        exit(EXIT_FAILURE);
    return result;
}
