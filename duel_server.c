#define _POSIX_C_SOURCE 200809L
#include "duel.h"

#include <libwebsockets.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef LIFE_DUEL_WEB_DIR
#define LIFE_DUEL_WEB_DIR "web"
#endif
#ifndef LIFE_DUEL_WEB_SOURCE
#define LIFE_DUEL_WEB_SOURCE "web"
#endif

enum { ROOMS = 16, INPUT = 512, OUTPUT = 8192, REPLAY_LIMIT = 131072 };
struct client;
struct room {
    char code[9], token[2][33], path[1024];
    struct duel game;
    struct client *clients[2];
    FILE *journal;
    double deadline, paused, touched;
    bool started, logged_finish;
};
struct client {
    struct lws *wsi;
    struct room *room;
    unsigned player, seq, received;
    double rate_window;
    size_t input_length;
    char input[INPUT], error[128];
    bool catalog, welcome, state, download;
};
static struct room rooms[ROOMS];
static struct lws_context *context;
static lws_sorted_usec_list_t timer;
static volatile sig_atomic_t stopping;
static unsigned planning_seconds = 20;
static char replay_dir[900];
static char catalog[OUTPUT];

static double
clock_now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static void
stop(int sig)
{
    (void)sig;
    stopping = 1;
}

static bool
numeric(const char *s, unsigned *out)
{
    if (!s || !*s) return false;
    for (const char *p = s; *p; ++p) if (*p < '0' || *p > '9') return false;
    char *end;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno || *end || v > UINT32_MAX) return false;
    *out = (unsigned)v;
    return true;
}

static bool
append(char *out, size_t cap, size_t *used, const char *fmt, ...)
{
    if (*used >= cap) return false;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out + *used, cap - *used, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap - *used) { *used = cap; return false; }
    *used += (size_t)n;
    return true;
}

static void
notify(struct room *r)
{
    for (unsigned p = 0; p < 2; ++p) if (r->clients[p]) {
        r->clients[p]->state = true;
        lws_callback_on_writable(r->clients[p]->wsi);
    }
}

static void
reject(struct client *c, const char *error)
{
    snprintf(c->error, sizeof(c->error), "%s", error);
    c->state = c->room != NULL;
    lws_callback_on_writable(c->wsi);
}

static bool
random_hex(char *out, unsigned bytes)
{
    unsigned char data[16];
    const char hex[] = "0123456789ABCDEF";
    if (bytes > sizeof(data) || lws_get_random(context, data, bytes) != bytes) return false;
    for (unsigned i = 0; i < bytes; ++i) {
        out[i * 2] = hex[data[i] >> 4]; out[i * 2 + 1] = hex[data[i] & 15];
    }
    out[bytes * 2] = 0;
    return true;
}

static struct room *
find_room(const char *code)
{
    for (unsigned i = 0; i < ROOMS; ++i)
        if (rooms[i].code[0] && !strcmp(rooms[i].code, code)) return &rooms[i];
    return NULL;
}

static bool
flush_journal(struct room *r)
{
    if (!r->journal || (!ferror(r->journal) && fflush(r->journal) == 0)) return true;
    fprintf(stderr, "duel: replay write failed for room %s; stopping this match\n", r->code);
    fclose(r->journal); r->journal = NULL;
    r->game.phase = DUEL_FINISHED; r->game.winner = 3; r->logged_finish = true;
    for (unsigned p = 0; p < 2; ++p) if (r->clients[p]) reject(r->clients[p], "replay-write-failed");
    return false;
}

static void
checkpoint(struct room *r)
{
    if (r->journal) fprintf(r->journal, "check\t%u\t%016" PRIx64 "\n",
                            r->game.generation, duel_checksum(&r->game));
    flush_journal(r);
}

static void
finish(struct room *r, const char *reason)
{
    if (r->logged_finish) return;
    r->logged_finish = true;
    if (r->journal) fprintf(r->journal, "finish\t%u\t%u\t%s\n",
                            r->game.generation, r->game.winner, reason);
    checkpoint(r);
}

static bool
new_room(struct room *r)
{
    memset(r, 0, sizeof(*r));
    char code[9];
    do { if (!random_hex(code, 4)) return false; } while (find_room(code));
    memcpy(r->code, code, sizeof(code));
    if (!random_hex(r->token[0], 16)) { r->code[0] = 0; return false; }
    duel_init(&r->game);
    r->touched = clock_now();
    if (replay_dir[0]) {
        snprintf(r->path, sizeof(r->path), "%s/%s.duel", replay_dir, code);
        /* Never overwrite a previous match, even across server restarts. */
        r->journal = fopen(r->path, "wx");
        if (!r->journal) { r->code[0] = 0; return false; }
        fprintf(r->journal, "duel1\t%d\t%d\t%d\n", DUEL_RULES, DUEL_W, DUEL_H);
        if (!flush_journal(r)) { r->code[0] = 0; return false; }
    }
    return true;
}

static void
attach(struct client *c, struct room *r, unsigned p)
{
    c->room = r; c->player = p; c->seq = 0;
    r->clients[p] = c; r->touched = clock_now();
    c->welcome = true;
    if (r->clients[0] && r->clients[1]) {
        if (!r->started) {
            r->started = true; r->game.phase = DUEL_PLANNING;
            r->deadline = clock_now() + planning_seconds;
        } else if (r->paused) {
            r->deadline += clock_now() - r->paused;
            r->paused = 0;
        }
    }
    notify(r);
}

static void
command(struct client *c)
{
    char *tokens[9], *save = NULL;
    unsigned n = 0;
    for (char *p = strtok_r(c->input, " \t\r\n", &save); p; p = strtok_r(NULL, " \t\r\n", &save)) {
        if (n == 9) { reject(c, "invalid-command"); return; }
        tokens[n++] = p;
    }
    if (!n) { reject(c, "invalid-command"); return; }
    if (!c->room) {
        if (n == 1 && !strcmp(tokens[0], "create")) {
            for (unsigned i = 0; i < ROOMS; ++i) if (!rooms[i].code[0]) {
                if (!new_room(&rooms[i])) { reject(c, "cannot-create-room"); return; }
                attach(c, &rooms[i], 0); return;
            }
            reject(c, "server-full"); return;
        }
        if ((n == 2 && !strcmp(tokens[0], "join")) || (n == 3 && !strcmp(tokens[0], "resume"))) {
            struct room *r = find_room(tokens[1]);
            if (!r) { reject(c, "room-not-found"); return; }
            if (n == 2) {
                if (r->token[1][0] || r->game.phase == DUEL_FINISHED) { reject(c, "room-full"); return; }
                if (!random_hex(r->token[1], 16)) { reject(c, "cannot-create-room"); return; }
                attach(c, r, 1); return;
            }
            for (unsigned p = 0; p < 2; ++p) if (!strcmp(tokens[2], r->token[p]) && !r->clients[p]) {
                attach(c, r, p); return;
            }
            reject(c, "cannot-resume"); return;
        }
        reject(c, "join-first"); return;
    }
    struct room *r = c->room;
    struct duel *g = &r->game;
    if (n == 1 && !strcmp(tokens[0], "replay")) {
        if (!r->path[0]) { reject(c, "replay-disabled"); return; }
        if (g->phase != DUEL_FINISHED) { reject(c, "match-not-finished"); return; }
        c->download = true; lws_callback_on_writable(c->wsi); return;
    }
    unsigned seq, round;
    if (n < 3 || !numeric(tokens[1], &seq) || !numeric(tokens[2], &round) || !seq) {
        reject(c, "invalid-command"); return;
    }
    if (seq <= c->seq) { c->state = true; lws_callback_on_writable(c->wsi); return; }
    c->seq = seq;
    if (!r->clients[0] || !r->clients[1]) { reject(c, "waiting-player"); return; }
    if (round != g->round) { reject(c, "stale-round"); return; }
    if (g->phase != DUEL_PLANNING || g->ready[c->player]) { reject(c, "not-planning"); return; }
    const char *error = NULL;
    if (n == 7 && !strcmp(tokens[0], "place")) {
        struct duel_order order;
        if (!numeric(tokens[3], &order.piece) || !numeric(tokens[4], &order.x) ||
            !numeric(tokens[5], &order.y) || !numeric(tokens[6], &order.transform)) error = "invalid-placement";
        else error = duel_place(g, c->player, order);
    } else if (n == 3 && !strcmp(tokens[0], "undo")) error = duel_undo(g, c->player, false);
    else if (n == 3 && !strcmp(tokens[0], "clear")) error = duel_undo(g, c->player, true);
    else if (n == 3 && !strcmp(tokens[0], "ready")) g->ready[c->player] = true;
    else error = "invalid-command";
    if (error) reject(c, error);
    notify(r);
}

static size_t
snapshot(struct client *c, char *out)
{
    struct room *r = c->room;
    struct duel *g = &r->game;
    static const char *phases[] = {"waiting", "planning", "running", "finished"};
    unsigned char packed[DUEL_CELLS / 4] = {0};
    char encoded[DUEL_CELLS / 3 + 8];
    for (unsigned i = 0; i < DUEL_CELLS; ++i) packed[i / 4] |= (unsigned char)(g->cells[i] << ((i % 4) * 2));
    lws_b64_encode_string((const char *)packed, sizeof(packed), encoded, sizeof(encoded));
    double remaining = r->deadline - (r->paused ? r->paused : clock_now());
    if (remaining < 0 || g->phase != DUEL_PLANNING) remaining = 0;
    size_t used = 0;
    append(out, OUTPUT, &used,
        "{\"type\":\"state\",\"phase\":\"%s\",\"round\":%u,\"generation\":%u,\"winner\":%u,"
        "\"connected\":[%s,%s],\"ready\":[%s,%s],\"energy\":[%u,%u],\"xp\":[%u,%u],"
        "\"score\":[%u,%u],\"population\":[%u,%u],\"zones\":[%u,%u,%u],"
        "\"remaining\":%u,\"reserved\":%u,\"seq\":%u,\"plans\":[",
        phases[g->phase], g->round, g->generation, g->winner,
        r->clients[0] ? "true" : "false", r->clients[1] ? "true" : "false",
        g->ready[0] ? "true" : "false", g->ready[1] ? "true" : "false",
        g->energy[0], g->energy[1], g->xp[0], g->xp[1], g->score[0], g->score[1],
        g->population[0], g->population[1], g->zones[0], g->zones[1], g->zones[2],
        (unsigned)(remaining * 1000), g->reserved[c->player], c->seq);
    /* Never transmit the other player's drafts, reserved energy or credentials. */
    for (unsigned i = 0; i < g->count[c->player]; ++i) {
        struct duel_order o = g->plans[c->player][i];
        append(out, OUTPUT, &used, "%s[%u,%u,%u,%u]", i ? "," : "", o.piece, o.x, o.y, o.transform);
    }
    append(out, OUTPUT, &used, "],\"grid\":\"%s\"}", encoded);
    return used < OUTPUT ? used : 0;
}

static void
make_catalog(void)
{
    size_t used = 0;
    append(catalog, sizeof(catalog), &used,
           "{\"type\":\"catalog\",\"rules\":%d,\"width\":%d,\"height\":%d,"
           "\"steps\":%d,\"rounds\":%d,\"target\":%d,\"planningSeconds\":%u,\"pieces\":[",
           DUEL_RULES, DUEL_W, DUEL_H, DUEL_STEPS, DUEL_ROUNDS, DUEL_TARGET, planning_seconds);
    for (unsigned k = 0; k < DUEL_PIECES; ++k) {
        struct duel_piece *p = &duel_pieces[k];
        append(catalog, sizeof(catalog), &used,
               "%s{\"name\":\"%s\",\"cost\":%u,\"unlock\":%u,\"width\":%u,\"height\":%u,\"cells\":[",
               k ? "," : "", p->name, p->cost, p->unlock, p->width, p->height);
        for (unsigned i = 0; i < p->count; ++i)
            append(catalog, sizeof(catalog), &used, "%s[%u,%u]", i ? "," : "", p->xy[i][0], p->xy[i][1]);
        append(catalog, sizeof(catalog), &used, "]}");
    }
    append(catalog, sizeof(catalog), &used, "]}");
}

static int
callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    struct client *c = user;
    switch (reason) {
    case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION: {
        char origin[256], host[256];
        if (lws_hdr_total_length(wsi, WSI_TOKEN_ORIGIN)) {
            if (lws_hdr_copy(wsi, origin, sizeof(origin), WSI_TOKEN_ORIGIN) <= 0 ||
                lws_hdr_copy(wsi, host, sizeof(host), WSI_TOKEN_HOST) <= 0) return 1;
            const char *authority = !strncmp(origin, "https://", 8) ? origin + 8 :
                                    !strncmp(origin, "http://", 7) ? origin + 7 : NULL;
            if (!authority || strcmp(authority, host)) return 1;
        }
        break;
    }
    case LWS_CALLBACK_ESTABLISHED:
        c->wsi = wsi; c->catalog = true; c->rate_window = clock_now();
        lws_callback_on_writable(wsi);
        break;
    case LWS_CALLBACK_RECEIVE:
        if (lws_frame_is_binary(wsi) || len + c->input_length >= sizeof(c->input) || memchr(in, 0, len)) return -1;
        memcpy(c->input + c->input_length, in, len); c->input_length += len;
        if (lws_is_final_fragment(wsi) && !lws_remaining_packet_payload(wsi)) {
            double now = clock_now();
            if (now - c->rate_window >= 1) { c->received = 0; c->rate_window = now; }
            if (++c->received > 60) return -1;
            c->input[c->input_length] = 0;
            command(c); c->input_length = 0;
        }
        break;
    case LWS_CALLBACK_SERVER_WRITEABLE: {
        unsigned char buffer[LWS_PRE + OUTPUT];
        char *out = (char *)buffer + LWS_PRE;
        size_t n = 0;
        if (c->catalog) { c->catalog = false; n = strlen(catalog); memcpy(out, catalog, n); }
        else if (c->welcome && c->room) {
            c->welcome = false;
            n = (size_t)snprintf(out, OUTPUT, "{\"type\":\"welcome\",\"room\":\"%s\",\"token\":\"%s\",\"player\":%u}",
                                 c->room->code, c->room->token[c->player], c->player);
        } else if (c->error[0]) {
            n = (size_t)snprintf(out, OUTPUT, "{\"type\":\"error\",\"message\":\"%s\"}", c->error);
            c->error[0] = 0;
        } else if (c->state && c->room) {
            c->state = false; n = snapshot(c, out);
        } else if (c->download && c->room) {
            c->download = false;
            FILE *f = fopen(c->room->path, "rb");
            unsigned char *data = malloc(LWS_PRE + REPLAY_LIMIT + 1);
            if (!f || !data) { if (f) fclose(f); free(data); reject(c, "replay-unavailable"); return 0; }
            size_t bytes = fread(data + LWS_PRE, 1, REPLAY_LIMIT + 1, f);
            bool failed = ferror(f) || bytes > REPLAY_LIMIT;
            fclose(f);
            int sent = failed ? -1 : lws_write(wsi, data + LWS_PRE, bytes, LWS_WRITE_BINARY);
            free(data);
            return sent < 0 || (size_t)sent < bytes ? -1 : 0;
        }
        if (n && (size_t)lws_write(wsi, (unsigned char *)out, n, LWS_WRITE_TEXT) != n) return -1;
        if (c->catalog || c->welcome || c->error[0] || c->state || c->download) lws_callback_on_writable(wsi);
        break;
    }
    case LWS_CALLBACK_CLOSED:
        if (c->room && c->room->clients[c->player] == c) {
            struct room *r = c->room;
            r->clients[c->player] = NULL; r->touched = clock_now();
            if (r->started && r->game.phase != DUEL_FINISHED && !r->paused) r->paused = clock_now();
            notify(r);
        }
        break;
    default: break;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    {"http", lws_callback_http_dummy, 0, 0, 0, NULL, 0},
    {"life-duel-v1", callback, sizeof(struct client), INPUT, 0, NULL, 0},
    {NULL, NULL, 0, 0, 0, NULL, 0}
};

static void
tick(lws_sorted_usec_list_t *sul)
{
    (void)sul;
    static unsigned ticks;
    double now = clock_now();
    for (unsigned i = 0; i < ROOMS; ++i) {
        struct room *r = &rooms[i];
        if (!r->code[0]) continue;
        if (!r->clients[0] && !r->clients[1] && now - r->touched > 300) {
            if (r->journal) fclose(r->journal);
            memset(r, 0, sizeof(*r)); continue;
        }
        struct duel *g = &r->game;
        if (r->paused && g->phase != DUEL_FINISHED && now - r->paused >= 30) {
            g->winner = r->clients[0] ? 1 : r->clients[1] ? 2 : 3;
            g->phase = DUEL_FINISHED; finish(r, "disconnect"); notify(r);
        }
        if (!r->clients[0] || !r->clients[1] || g->phase == DUEL_FINISHED) continue;
        if (g->phase == DUEL_PLANNING && (now >= r->deadline || (g->ready[0] && g->ready[1]))) {
            if (r->journal) {
                fprintf(r->journal, "turn\t%u\t%u\n", g->round, g->generation);
                for (unsigned p = 0; p < 2; ++p) for (unsigned k = 0; k < g->count[p]; ++k) {
                    struct duel_order o = g->plans[p][k];
                    fprintf(r->journal, "place\t%u\t%u\t%u\t%u\t%u\n", p, o.piece, o.x, o.y, o.transform);
                }
                fputs("run\n", r->journal);
            }
            if (!flush_journal(r)) { notify(r); continue; }
            duel_begin(g); notify(r);
        } else if (g->phase == DUEL_RUNNING) {
            duel_step(g);
            if (g->phase == DUEL_PLANNING) { r->deadline = now + planning_seconds; checkpoint(r); }
            if (g->phase == DUEL_FINISHED) finish(r, g->round == DUEL_ROUNDS ? "limit" : "domination");
            notify(r);
        } else if (ticks % 10 == 0) notify(r);
    }
    ++ticks;
    lws_sul_schedule(context, 0, &timer, tick, 100 * LWS_US_PER_MS);
}

static bool
make_directories(const char *path)
{
    char copy[900];
    if (!*path || strlen(path) >= sizeof(copy)) return false;
    snprintf(copy, sizeof(copy), "%s", path);
    for (char *p = copy + 1; ; ++p) if (!*p || *p == '/') {
        char end = *p; *p = 0;
        if (mkdir(copy, 0700) && errno != EEXIST) return false;
        *p = end; if (!end) return true;
    }
}

static int
verify_replay(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 1; }
    struct duel g;
    duel_init(&g); g.phase = DUEL_PLANNING;
    char line[512], *v[8], *save;
    unsigned number = 0, checks = 0;
    bool header = false, valid = true;
    while (fgets(line, sizeof(line), f)) {
        ++number;
        if (!strchr(line, '\n')) { valid = false; break; }
        unsigned n = 0;
        save = NULL;
        for (char *p = strtok_r(line, "\t\r\n", &save); p; p = strtok_r(NULL, "\t\r\n", &save)) {
            if (n == 8) { valid = false; break; } v[n++] = p;
        }
        if (!valid || !n) { valid = false; break; }
        unsigned a, b, c, d, e;
        if (!header) {
            valid = n == 4 && !strcmp(v[0], "duel1") && numeric(v[1], &a) && a == DUEL_RULES &&
                numeric(v[2], &b) && b == DUEL_W && numeric(v[3], &c) && c == DUEL_H;
            header = true;
        } else if (!strcmp(v[0], "turn")) {
            valid = n == 3 && numeric(v[1], &a) && numeric(v[2], &b) &&
                a == g.round && b == g.generation && g.phase == DUEL_PLANNING;
        } else if (!strcmp(v[0], "place")) {
            valid = n == 6 && numeric(v[1], &a) && numeric(v[2], &b) && numeric(v[3], &c) &&
                numeric(v[4], &d) && numeric(v[5], &e);
            if (valid) valid = !duel_place(&g, a, (struct duel_order){b, c, d, e});
        } else if (!strcmp(v[0], "run")) {
            valid = n == 1 && g.phase == DUEL_PLANNING;
            if (valid) duel_begin(&g);
        } else if (!strcmp(v[0], "check") || !strcmp(v[0], "finish")) {
            bool ending = !strcmp(v[0], "finish");
            valid = n == (ending ? 4u : 3u) && numeric(v[1], &a) && a >= g.generation && a <= g.generation + DUEL_STEPS;
            if (valid) {
                while (g.phase == DUEL_RUNNING && g.generation < a) duel_step(&g);
                valid = g.generation == a;
                if (ending) {
                    valid = valid && numeric(v[2], &b) && b >= 1 && b <= 3;
                    if (valid && !strcmp(v[3], "disconnect")) { g.winner = b; g.phase = DUEL_FINISHED; }
                    else valid = valid && g.phase == DUEL_FINISHED && g.winner == b;
                } else {
                    char *end; errno = 0;
                    uint64_t hash = strtoull(v[2], &end, 16);
                    valid = valid && !errno && !*end && strlen(v[2]) == 16 && hash == duel_checksum(&g);
                    ++checks;
                }
            }
        } else valid = false;
        if (!valid) break;
    }
    valid = valid && header && checks && !ferror(f);
    fclose(f);
    if (!valid) { fprintf(stderr, "Invalid replay at line %u\n", number); return 1; }
    printf("Replay verified: %u generations, %u checkpoints, winner %u%s\n", g.generation, checks, g.winner,
           g.phase == DUEL_FINISHED ? "" : " (unfinished match)");
    return 0;
}

int
main(int argc, char **argv)
{
    const char *bind = "127.0.0.1", *web = LIFE_DUEL_WEB_DIR, *replay = NULL;
    unsigned port = 7681;
    bool logging = true;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!strcmp(arg, "--help")) {
            puts("Life Duel — an authoritative, two-player Life server\n"
                 "Usage: life-duel-server [--bind ADDRESS] [--port PORT]\n"
                 "  --bind ADDRESS          Listen address (default: 127.0.0.1; LAN: 0.0.0.0)\n"
                 "  --port PORT             HTTP + WebSocket port (default: 7681)\n"
                 "  --web-root DIRECTORY    Browser client assets\n"
                 "  --replay-dir DIRECTORY  Match journals (default: XDG state/game-of-life/duels)\n"
                 "  --no-replays            Disable match recording\n"
                 "  --replay FILE           Verify and replay a journal on the CPU, then exit\n"
                 "  --planning-seconds N    Planning deadline, 1..300 (default: 20)\n"
                 "Open the printed URL. Create a room and share its link with a friend.");
            return 0;
        }
        if (!strcmp(arg, "--no-replays")) { logging = false; continue; }
        if (++i == argc) { fprintf(stderr, "Missing value for %s\n", arg); return 1; }
        if (!strcmp(arg, "--bind")) bind = argv[i];
        else if (!strcmp(arg, "--web-root")) web = argv[i];
        else if (!strcmp(arg, "--replay")) replay = argv[i];
        else if (!strcmp(arg, "--replay-dir")) {
            if (strlen(argv[i]) >= sizeof(replay_dir)) return 1;
            snprintf(replay_dir, sizeof(replay_dir), "%s", argv[i]);
        } else if (!strcmp(arg, "--port")) {
            if (!numeric(argv[i], &port) || !port || port > 65535) return 1;
        } else if (!strcmp(arg, "--planning-seconds")) {
            if (!numeric(argv[i], &planning_seconds) || !planning_seconds || planning_seconds > 300) return 1;
        } else { fprintf(stderr, "Unknown option: %s\n", arg); return 1; }
    }
    if (!duel_catalog_init()) { fputs("Cannot load figure library\n", stderr); return 1; }
    if (replay) return verify_replay(replay);
    umask(0077);
    if (logging) {
        if (!replay_dir[0]) {
            const char *state = getenv("XDG_STATE_HOME"), *home = getenv("HOME");
            int n;
            if (state && *state == '/') n = snprintf(replay_dir, sizeof(replay_dir), "%s/game-of-life/duels", state);
            else if (home && *home == '/') n = snprintf(replay_dir, sizeof(replay_dir), "%s/.local/state/game-of-life/duels", home);
            else { fputs("Set --replay-dir or --no-replays\n", stderr); return 1; }
            if (n < 0 || (size_t)n >= sizeof(replay_dir)) return 1;
        }
        if (!make_directories(replay_dir)) { perror("replay directory"); return 1; }
    } else replay_dir[0] = 0;
    if (!strcmp(web, LIFE_DUEL_WEB_DIR) && access(web, R_OK)) web = LIFE_DUEL_WEB_SOURCE;
    if (access(web, R_OK)) { fprintf(stderr, "Cannot read web assets: %s\n", web); return 1; }
    make_catalog();
    struct lws_http_mount mount = {.mountpoint = "/", .origin = web, .def = "index.html",
                                  .origin_protocol = LWSMPRO_FILE, .mountpoint_len = 1};
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = (int)port; info.iface = bind; info.protocols = protocols; info.mounts = &mount;
    info.gid = -1; info.uid = -1;
    info.fd_limit_per_thread = 96;
    info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
    static const lws_retry_bo_t retry = {.secs_since_valid_ping = 3, .secs_since_valid_hangup = 10};
    info.retry_and_idle_policy = &retry;
    lws_set_log_level(LLL_ERR, NULL);
    context = lws_create_context(&info);
    if (!context) { fputs("Cannot start server (address or port unavailable)\n", stderr); return 1; }
    signal(SIGINT, stop); signal(SIGTERM, stop);
    lws_sul_schedule(context, 0, &timer, tick, 100 * LWS_US_PER_MS);
    printf("Life Duel: http://%s:%u\n%s\n", !strcmp(bind, "0.0.0.0") ? "localhost" : bind,
           port, !strcmp(bind, "0.0.0.0") ? "LAN: share this machine's IP address and port." : "Listening on loopback only; use --bind 0.0.0.0 for LAN play.");
    fflush(stdout);
    while (!stopping && lws_service(context, 0) >= 0) {}
    lws_sul_cancel(&timer);
    lws_context_destroy(context);
    for (unsigned i = 0; i < ROOMS; ++i) if (rooms[i].journal) fclose(rooms[i].journal);
    return 0;
}
