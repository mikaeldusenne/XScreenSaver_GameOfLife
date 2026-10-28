#include "duel.h"
#include "figures.h"

#include <string.h>

struct duel_piece duel_pieces[DUEL_PIECES] = {
    {.name = "cell", .cost = 2},
    {.name = "block", .cost = 4},
    {.name = "eater-1", .cost = 8},
    {.name = "glider", .cost = 7},
    {.name = "lwss", .cost = 14, .unlock = 3},
    {.name = "gosper-glider-gun", .cost = 60, .unlock = 8}
};

bool
duel_catalog_init(void)
{
    struct life seed = {0};
    if (!life_resize(&seed, 40, 40)) return false;
    duel_pieces[0].width = duel_pieces[0].height = duel_pieces[0].count = 1;
    for (unsigned k = 1; k < DUEL_PIECES; ++k) {
        struct duel_piece *p = &duel_pieces[k];
        const struct figure *f = figure_find(p->name);
        life_clear(&seed);
        if (!f || f->population > DUEL_POINTS || !figure_stamp(&seed, f, 0, 0, 0)) {
            life_free(&seed);
            return false;
        }
        p->width = (unsigned)f->width;
        p->height = (unsigned)f->height;
        p->count = 0;
        for (unsigned y = 0; y < p->height; ++y)
            for (unsigned x = 0; x < p->width; ++x)
                if (seed.cells[(y + 1) * (unsigned)seed.stride + x + 1] >= LIFE_ALIVE) {
                    p->xy[p->count][0] = (uint8_t)x;
                    p->xy[p->count++][1] = (uint8_t)y;
                }
    }
    life_free(&seed);
    return true;
}

void
duel_point(const struct duel_piece *p, unsigned transform, unsigned i,
           unsigned *x, unsigned *y)
{
    unsigned u = p->xy[i][0], v = p->xy[i][1];
    if (transform & 4) u = p->width - 1 - u;
    switch (transform & 3) {
    case 1: *x = p->height - 1 - v; *y = u; break;
    case 2: *x = p->width - 1 - u; *y = p->height - 1 - v; break;
    case 3: *x = v; *y = p->width - 1 - u; break;
    default: *x = u; *y = v; break;
    }
}

void
duel_init(struct duel *g)
{
    memset(g, 0, sizeof(*g));
    g->energy[0] = g->energy[1] = 40;
    g->round = 1;
    /* Symmetric starting colonies. Bounded arena: nothing wraps across edges. */
    for (unsigned p = 0; p < 2; ++p)
        for (unsigned y = 39; y < 41; ++y)
            for (unsigned x = 0; x < 2; ++x)
                g->cells[y * DUEL_W + (p ? 84 : 42) + x] = (uint8_t)(p + 1);
    g->population[0] = g->population[1] = 4;
}

const char *
duel_place(struct duel *g, unsigned player, struct duel_order o)
{
    if (player > 1 || g->phase != DUEL_PLANNING || g->ready[player]) return "not-planning";
    if (o.piece >= DUEL_PIECES || o.transform > 7 || o.x >= DUEL_W || o.y >= DUEL_H)
        return "invalid-placement";
    const struct duel_piece *p = &duel_pieces[o.piece];
    if (g->count[player] >= DUEL_PLANS) return "plan-full";
    if (g->xp[player] < p->unlock) return "locked";
    if (p->cost > g->energy[player] - g->reserved[player]) return "energy";
    uint8_t occupied[DUEL_CELLS];
    memcpy(occupied, g->cells, sizeof(occupied));
    for (unsigned k = 0; k < g->count[player]; ++k) {
        struct duel_order a = g->plans[player][k];
        const struct duel_piece *f = &duel_pieces[a.piece];
        for (unsigned i = 0; i < f->count; ++i) {
            unsigned x, y;
            duel_point(f, a.transform, i, &x, &y);
            occupied[(a.y + y) * DUEL_W + a.x + x] = 1;
        }
    }
    for (unsigned i = 0; i < p->count; ++i) {
        unsigned x, y;
        duel_point(p, o.transform, i, &x, &y);
        x += o.x; y += o.y;
        if (x >= DUEL_W || y >= DUEL_H) return "outside";
        if ((!player && x >= 48) || (player && x < 80)) return "home-only";
        if (occupied[y * DUEL_W + x]) return "occupied";
    }
    g->plans[player][g->count[player]++] = o;
    g->reserved[player] += p->cost;
    return NULL;
}

const char *
duel_undo(struct duel *g, unsigned player, bool all)
{
    if (player > 1 || g->phase != DUEL_PLANNING || g->ready[player]) return "not-planning";
    if (all) { g->count[player] = g->reserved[player] = 0; }
    else if (g->count[player]) {
        unsigned piece = g->plans[player][--g->count[player]].piece;
        g->reserved[player] -= duel_pieces[piece].cost;
    }
    return NULL;
}

void
duel_begin(struct duel *g)
{
    if (g->phase != DUEL_PLANNING) return;
    /* Both homes are disjoint; all plans enter on this same generation. */
    for (unsigned p = 0; p < 2; ++p) {
        for (unsigned k = 0; k < g->count[p]; ++k) {
            struct duel_order o = g->plans[p][k];
            const struct duel_piece *piece = &duel_pieces[o.piece];
            for (unsigned i = 0; i < piece->count; ++i) {
                unsigned x, y;
                duel_point(piece, o.transform, i, &x, &y);
                g->cells[(o.y + y) * DUEL_W + o.x + x] = (uint8_t)(p + 1);
            }
        }
        g->energy[p] -= g->reserved[p];
        g->count[p] = g->reserved[p] = 0;
        g->ready[p] = false;
    }
    g->phase = DUEL_RUNNING;
    g->step = 0;
    g->population[0] = g->population[1] = 0;
    for (unsigned i = 0; i < DUEL_CELLS; ++i)
        if (g->cells[i]) ++g->population[g->cells[i] - 1];
}

void
duel_step(struct duel *g)
{
    if (g->phase != DUEL_RUNNING) return;
    g->population[0] = g->population[1] = 0;
    for (int y = 0; y < DUEL_H; ++y) for (int x = 0; x < DUEL_W; ++x) {
        unsigned neighbors = 0, blue = 0;
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            int nx = x + dx, ny = y + dy;
            if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= DUEL_W || ny >= DUEL_H) continue;
            unsigned v = g->cells[ny * DUEL_W + nx];
            neighbors += v != 0; blue += v == 1;
        }
        unsigned i = (unsigned)(y * DUEL_W + x), v = g->cells[i], next = 0;
        if (v && (neighbors == 2 || neighbors == 3)) next = v;
        else if (!v && neighbors == 3) next = blue >= 2 ? 1 : 2;
        g->scratch[i] = (uint8_t)next;
        if (next) ++g->population[next - 1];
    }
    memcpy(g->cells, g->scratch, sizeof(g->cells));
    ++g->generation; ++g->step;
    unsigned held[2] = {0, 0};
    for (unsigned z = 0; z < 3; ++z) {
        unsigned count[2] = {0, 0};
        for (unsigned y = 8 + z * 24; y < 24 + z * 24; ++y)
            for (unsigned x = 54; x < 74; ++x) {
                unsigned v = g->cells[y * DUEL_W + x];
                if (v) ++count[v - 1];
            }
        unsigned owner = 0;
        if (count[0] >= 3 && count[0] > count[1]) owner = 1;
        if (count[1] >= 3 && count[1] > count[0]) owner = 2;
        g->zones[z] = owner;
        if (owner) ++held[owner - 1];
    }
    for (unsigned p = 0; p < 2; ++p) {
        if (held[p] >= 2) ++g->score[p];
        if (g->score[p] >= DUEL_TARGET) { g->winner = p + 1; g->phase = DUEL_FINISHED; }
    }
    if (g->phase == DUEL_FINISHED || g->step < DUEL_STEPS) return;
    if (g->round == DUEL_ROUNDS) {
        g->winner = g->score[0] == g->score[1] ? 3 : (g->score[0] > g->score[1] ? 1 : 2);
        g->phase = DUEL_FINISHED;
        return;
    }
    for (unsigned p = 0; p < 2; ++p) {
        g->energy[p] += DUEL_INCOME;
        if (g->energy[p] > DUEL_ENERGY) g->energy[p] = DUEL_ENERGY;
        /* Participation unlocks tools for both players; territory adds at most 2 XP. */
        g->xp[p] += 1 + (held[p] > 2 ? 2 : held[p]);
    }
    ++g->round;
    g->phase = DUEL_PLANNING;
}

uint64_t
duel_checksum(const struct duel *g)
{
    uint64_t h = UINT64_C(14695981039346656037);
    for (unsigned i = 0; i < DUEL_CELLS; ++i) h = (h ^ g->cells[i]) * UINT64_C(1099511628211);
    const unsigned values[] = {g->generation, g->round, g->phase, g->winner,
        g->energy[0], g->energy[1], g->xp[0], g->xp[1], g->score[0], g->score[1]};
    for (unsigned i = 0; i < sizeof(values) / sizeof(*values); ++i)
        for (unsigned b = 0; b < 4; ++b) h = (h ^ ((values[i] >> (b * 8)) & 255)) * UINT64_C(1099511628211);
    return h;
}
