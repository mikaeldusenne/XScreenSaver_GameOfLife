#ifndef LIFE_DUEL_H
#define LIFE_DUEL_H

#include <stdbool.h>
#include <stdint.h>

/* Versioned, integer-only match rules. The screensaver's engine is unchanged. */
enum { DUEL_RULES = 1, DUEL_W = 128, DUEL_H = 80, DUEL_CELLS = DUEL_W * DUEL_H,
       DUEL_PIECES = 6, DUEL_POINTS = 64, DUEL_PLANS = 64,
       DUEL_ENERGY = 100, DUEL_INCOME = 16, DUEL_STEPS = 16,
       DUEL_ROUNDS = 30, DUEL_TARGET = 160 };
enum duel_phase { DUEL_WAITING, DUEL_PLANNING, DUEL_RUNNING, DUEL_FINISHED };
struct duel_piece {
    const char *name;
    unsigned cost, unlock, count, width, height;
    uint8_t xy[DUEL_POINTS][2];
};
struct duel_order { unsigned piece, x, y, transform; };
struct duel {
    uint8_t cells[DUEL_CELLS], scratch[DUEL_CELLS];
    struct duel_order plans[2][DUEL_PLANS];
    unsigned count[2], reserved[2], energy[2], xp[2], score[2], population[2];
    unsigned generation, round, step, winner, zones[3];
    bool ready[2];
    enum duel_phase phase;
};

extern struct duel_piece duel_pieces[DUEL_PIECES];
bool duel_catalog_init(void);
void duel_init(struct duel *game);
void duel_point(const struct duel_piece *piece, unsigned transform, unsigned i,
                unsigned *x, unsigned *y);
const char *duel_place(struct duel *game, unsigned player, struct duel_order order);
const char *duel_undo(struct duel *game, unsigned player, bool all);
void duel_begin(struct duel *game);
void duel_step(struct duel *game);
uint64_t duel_checksum(const struct duel *game);

#endif
