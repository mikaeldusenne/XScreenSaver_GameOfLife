/* Fixed-generation, CPU-only benchmark. No X11 or frame-rate throttling. */
#define _POSIX_C_SOURCE 200809L
#include "life.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static unsigned long
argument(const char *text, unsigned long maximum)
{
    char *end;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno || !*text || *text == '-' || *end || value > maximum) {
        fprintf(stderr, "Invalid benchmark argument: %s\n", text);
        exit(EXIT_FAILURE);
    }
    return value;
}

static double
cpu_time(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double)time.tv_sec + (double)time.tv_nsec / 1e9;
}

int
main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s columns rows generations seed\n", argv[0]);
        return EXIT_FAILURE;
    }
    int columns = (int)argument(argv[1], 32767), rows = (int)argument(argv[2], 32767);
    unsigned long generations = argument(argv[3], 10000000);
    uint32_t seed = (uint32_t)argument(argv[4], UINT32_MAX);
    struct life life = {.random = seed};
    if (!generations || !life_resize(&life, columns, rows)) return EXIT_FAILURE;
    life_seed(&life);
    for (int i = 0; i < 32; ++i) life_step(&life, 18, false);
    double start = cpu_time();
    for (unsigned long i = 0; i < generations; ++i) life_step(&life, 18, false);
    double elapsed = cpu_time() - start;
    printf("{\"columns\":%d,\"rows\":%d,\"generations\":%lu,\"seed\":%" PRIu32 ","
           "\"ms_per_generation\":%.6f,\"hash\":\"%016" PRIx64 "\",\"population\":%u}\n",
           columns, rows, generations, seed, elapsed * 1000 / (double)generations,
           life.hash, life.population);
    life_free(&life);
    return EXIT_SUCCESS;
}
