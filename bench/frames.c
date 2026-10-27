/* Benchmark-only instrumentation. The shipped binary never links this file. */
#define _POSIX_C_SOURCE 200809L
#undef XFlush
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if LIFE_HAVE_GPU
#include <epoxy/gl.h>
#include <epoxy/glx.h>
#endif

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
#if LIFE_HAVE_GPU
    /* Confirm completion without forcing the driver to wait after every draw.
     * GPU frames are timestamped at submission and recorded on a later flush
     * once their fence signals. At most eight frames may remain outstanding. */
    if (glXGetCurrentContext()) {
        static GLsync pending[8];
        static double submitted[8];
        static unsigned head, count;
        while (count) {
            GLenum status = glClientWaitSync(pending[head], 0, 0);
            if (status == GL_WAIT_FAILED) exit(EXIT_FAILURE);
            if (status == GL_TIMEOUT_EXPIRED) break;
            glDeleteSync(pending[head]);
            if (fwrite(&submitted[head], sizeof(double), 1, frames) != 1) exit(EXIT_FAILURE);
            head = (head + 1) % 8;
            --count;
        }
        if (count == 8) {
            fputs("GPU benchmark exceeded eight unfinished frames\n", stderr);
            exit(EXIT_FAILURE);
        }
        unsigned tail = (head + count++) % 8;
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        submitted[tail] = (double)t.tv_sec + (double)t.tv_nsec / 1e9;
        pending[tail] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
        if (fflush(frames)) exit(EXIT_FAILURE);
        return result;
    }
#endif
    /* Include server processing; do not mistake a growing X queue for FPS. */
    XSync(display, False);
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    double timestamp = (double)t.tv_sec + (double)t.tv_nsec / 1e9;
    if (fwrite(&timestamp, sizeof(timestamp), 1, frames) != 1 || fflush(frames))
        exit(EXIT_FAILURE);
    return result;
}
