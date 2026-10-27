#ifndef JOURNAL_H
#define JOURNAL_H

#include "figures.h"

enum mode { MODE_RANDOM, MODE_FIGURE, MODE_RANDOM_FIGURES, MODE_ALTERNATE };

/* life1 describes the generator, including the 32-step random warmup. */
struct entry {
    enum mode mode;
    const struct figure *figure;
    uint32_t random;
    int columns, rows;
    unsigned trail;
    uint64_t checksum;
};

char *journal_path(void);
uint64_t journal_checksum(const struct life *life);
bool journal_append(const char *path, const struct entry *entry);
/* Physical line number, or zero for the last complete line. NULL on success. */
const char *journal_read(const char *path, unsigned long line, struct entry *entry);

#endif
