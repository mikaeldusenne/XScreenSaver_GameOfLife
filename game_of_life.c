/*
 * XScreenSaver Game of Life
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation. No representations are made about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 */

#define _POSIX_C_SOURCE 200809L
#include "life.h"
#include "figures.h"
#include "scene.h"
#include "journal.h"
#include "gpu.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifndef LIFE_HAVE_XSS
#define LIFE_HAVE_XSS 1
#endif
#if LIFE_HAVE_XSS
#include <X11/extensions/scrnsaver.h>
#endif
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef LIFE_VERSION
#define LIFE_VERSION "dev"
#endif

enum { BATCH = 1024, HISTORY = 64 };
struct options {
    const char *display;
    const char *history_path;
    unsigned long replay_line;
    Window window;
    int size, random_size, fps, trail, seconds, idle;
    unsigned seed;
    enum mode mode;
    const struct figure *figure;
    bool root, fullscreen, wallpaper, doctor, english, replay, no_history, gpu, square_cells, fancy;
};

struct app {
    Display *display;
    Window window, root;
    GC gc;
    XFontStruct *font;
    Atom wm_delete;
    Colormap colormap;
    unsigned long colors[LIFE_COLORS];
    int allocated_colors;
    XRectangle rectangles[LIFE_COLORS][BATCH];
    int counts[LIFE_COLORS];
    struct life life;
    struct gpu *gpu;
    float rgb[LIFE_COLORS][3];
    struct options options;
    int width, height, size, base_size, left, top;
    unsigned fading;
    struct scene timing;
    enum mode scene;
    const struct figure *figure;
    struct entry history[HISTORY];
    struct entry replay;
    char *journal_path;
    bool journaling;
    unsigned history_count, history_index;
    double idle_check, idle_after;
    bool own_window, visible, redraw, presenting, gpu_pending;
};

static volatile sig_atomic_t stopping;
static Window drawing_window;
static bool window_destroyed;

static void
stop(int signal_number)
{
    (void)signal_number;
    stopping = 1;
}

static void
fail(const char *message)
{
    fprintf(stderr, "game_of_life: %s\n", message);
    exit(EXIT_FAILURE);
}

static int
x_error(Display *display, XErrorEvent *event)
{
    /* The host can destroy its window while our drawing requests are in flight. */
    if (event->resourceid == drawing_window &&
        (event->error_code == BadWindow || event->error_code == BadDrawable)) {
        window_destroyed = true;
        stopping = 1;
        return 0;
    }
    char message[256];
    XGetErrorText(display, event->error_code, message, sizeof(message));
    fail(message);
    return 0;
}

static unsigned long
number(const char *text, unsigned long low, unsigned long high)
{
    char *end;
    errno = 0;
    unsigned long value = strtoul(text, &end, 0);
    if (errno || !*text || *text == '-' || *end || value < low || value > high)
        fail("invalid numeric argument (see -help)");
    return value;
}

static bool
french_locale(void)
{
    const char *locale = getenv("LC_ALL");
    if (!locale || !*locale) locale = getenv("LC_MESSAGES");
    if (!locale || !*locale) locale = getenv("LANG");
    return locale && !strncmp(locale, "fr", 2);
}

static struct options
options(int argc, char **argv)
{
    struct options opt = {.fps = 15, .trail = 18,
                          .english = !french_locale(),
                          .seed = (unsigned)time(NULL) ^ (unsigned)getpid()};
    bool mode_chosen = false;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') ++arg;
        if (!strcmp(arg, "-help") || !strcmp(arg, "-h")) {
            puts("Game of Life " LIFE_VERSION " — cheap & beautiful\n\n"
                 "Try: game_of_life --alternate\n"
                 "     game_of_life --figure pulsar\n"
                 "     game_of_life --wallpaper --idle 120\n\n"
                 "Usage: game_of_life [options]\n"
                 "  --random           Random cells (default)\n"
                 "  --figure NAME      One centered instance of a known figure\n"
                 "  --random-figures    Scatter separated, randomly oriented figures\n"
                 "  --alternate        Cycle random -> figure -> random figures\n"
                 "  --mode MODE        random, random-figures or alternate\n"
                 "  --list-figures      List the built-in library and exit\n"
                 "  --find TEXT         Filter figures by name or family and exit\n"
                 "  --doctor            Check this display and report supported features\n"
                 "  --version           Print version and build features\n"
                 "  --gpu               Compute and draw with OpenGL 4.3 (optional)\n"
                 "  --cpu               Use the original CPU/Xlib engine (default)\n"
                 "  --fancy             Ember colors and subtle rising flames; selects GPU\n"
                 "  --classic           Original colors, no flames (default)\n"
                 "  --square-cells      Sharp GPU cells (default: rounded from 5 pixels)\n"
                 "  --lang en|fr        Caption language (default: system locale)\n"
                 "  --wallpaper        Animate behind applications (default: alternate)\n"
                 "  --idle N           With --wallpaper, show above apps after N idle seconds\n"
                 "                     0 = disabled (default), 1..86400; any key returns\n"
                 "  -root              Draw in XScreenSaver's window (or root)\n"
                 "  -window-id ID      Draw in an existing X11 window\n"
                 "  -fullscreen        Fullscreen preview; any key exits\n"
                 "  -display DISPLAY   X server (default: $DISPLAY)\n"
                 "  -size N            Cell size in pixels, 0 = automatic (default)\n"
                 "  --random-size N    Override cell size for random cells and figure mixes\n"
                 "                     0 = follow --size (default); single figures keep their zoom\n"
                 "  -fps N             Frames per second, 1..60 (default: 15)\n"
                 "  -trail N           Trail length, 0..31 (default: 18)\n"
                 "  -seconds N         Fixed duration, 10..3600; 0 = adaptive (default)\n"
                 "  -seed N            Reproducible initial world, 0..4294967295\n"
                 "  --history FILE     Append initial recipes (default: wallpaper only, XDG state)\n"
                 "  --no-history       Disable history writes\n"
                 "  --replay N         Replay history line N; 0 = last complete line\n"
                 "                     Uses --history FILE or the default journal; never appends\n"
                 "Left click: next scene; right click: previous scene (replayed).\n"
                 "Wheel: zoom at pointer; middle click: reset view. The world keeps evolving.\n"
                 "Preview: Escape/Q closes; Space gently renews the world.");
            exit(EXIT_SUCCESS);
        }
        if (!strcmp(arg, "-version")) {
            printf("Game of Life %s (X11; idle detection: %s; GPU build: %s; %zu figures)\n",
                   LIFE_VERSION, LIFE_HAVE_XSS ? "yes" : "no", LIFE_HAVE_GPU ? "yes" : "no", figure_count);
            exit(EXIT_SUCCESS);
        }
        if (!strcmp(arg, "-find")) {
            if (++i == argc) fail("--find needs a name or family, e.g. --find gun");
            for (size_t j = 0; j < figure_count; ++j)
                if (strstr(figures[j].name, argv[i]) || strstr(figures[j].kind, argv[i]))
                    printf("%-32s %-12s %3d x %d\n", figures[j].name, figures[j].kind,
                           figures[j].width, figures[j].height);
            exit(EXIT_SUCCESS);
        }
        if (!strcmp(arg, "-list-figures")) {
            for (size_t j = 0; j < figure_count; ++j)
                printf("%-32s %-12s %3d x %d\n", figures[j].name, figures[j].kind,
                       figures[j].width, figures[j].height);
            exit(EXIT_SUCCESS);
        }
        bool scene_option = !strcmp(arg, "-random") || !strcmp(arg, "-figure") ||
            !strcmp(arg, "-random-figures") || !strcmp(arg, "-alternate") ||
            !strcmp(arg, "-mode") || !strcmp(arg, "-replay");
        if (scene_option) {
            if (mode_chosen) fail("choose a single mode (see --help)");
            mode_chosen = true;
        }
        if (!strcmp(arg, "-doctor")) {
            opt.doctor = true;
        } else if (!strcmp(arg, "-gpu")) {
            opt.gpu = true;
        } else if (!strcmp(arg, "-cpu")) {
            opt.gpu = false;
            opt.fancy = false;
        } else if (!strcmp(arg, "-fancy")) {
            opt.fancy = opt.gpu = true;
        } else if (!strcmp(arg, "-classic")) {
            opt.fancy = false;
        } else if (!strcmp(arg, "-square-cells")) {
            opt.square_cells = true;
        } else if (!strcmp(arg, "-no-history")) {
            opt.no_history = true;
        } else if (!strcmp(arg, "-root")) {
            opt.root = true;
        } else if (!strcmp(arg, "-fullscreen")) {
            opt.fullscreen = true;
        } else if (!strcmp(arg, "-wallpaper")) {
            opt.wallpaper = true;
        } else if (!strcmp(arg, "-random")) {
            opt.mode = MODE_RANDOM;
        } else if (!strcmp(arg, "-random-figures")) {
            opt.mode = MODE_RANDOM_FIGURES;
        } else if (!strcmp(arg, "-alternate")) {
            opt.mode = MODE_ALTERNATE;
        } else {
            if (i + 1 == argc)
                fail("missing option value (see -help)");
            const char *value = argv[++i];
            if (!strcmp(arg, "-display")) opt.display = value;
            else if (!strcmp(arg, "-history")) {
                if (!*value) fail("history path cannot be empty");
                opt.history_path = value;
            }
            else if (!strcmp(arg, "-replay")) {
                opt.replay = true;
                opt.replay_line = number(value, 0, ULONG_MAX);
            }
            else if (!strcmp(arg, "-window-id")) opt.window = number(value, 1, ULONG_MAX);
            else if (!strcmp(arg, "-size")) opt.size = (int)number(value, 0, 128);
            else if (!strcmp(arg, "-random-size")) opt.random_size = (int)number(value, 0, 128);
            else if (!strcmp(arg, "-fps")) opt.fps = (int)number(value, 1, 60);
            else if (!strcmp(arg, "-trail")) opt.trail = (int)number(value, 0, 31);
            else if (!strcmp(arg, "-idle")) opt.idle = (int)number(value, 0, 86400);
            else if (!strcmp(arg, "-lang")) {
                if (strcmp(value, "en") && strcmp(value, "fr")) fail("--lang must be en or fr");
                opt.english = !strcmp(value, "en");
            }
            else if (!strcmp(arg, "-seconds")) {
                opt.seconds = (int)number(value, 0, 3600);
                if (opt.seconds && opt.seconds < 10) fail("seconds must be 0 or 10..3600");
            }
            else if (!strcmp(arg, "-seed")) opt.seed = (unsigned)number(value, 0, UINT32_MAX);
            else if (!strcmp(arg, "-figure")) {
                opt.mode = MODE_FIGURE;
                opt.figure = figure_find(value);
                if (!opt.figure) {
                    fprintf(stderr, "game_of_life: unknown figure '%s'; use --list-figures\n", value);
                    exit(EXIT_FAILURE);
                }
            } else if (!strcmp(arg, "-mode")) {
                if (!strcmp(value, "random")) opt.mode = MODE_RANDOM;
                else if (!strcmp(value, "random-figures")) opt.mode = MODE_RANDOM_FIGURES;
                else if (!strcmp(value, "alternate")) opt.mode = MODE_ALTERNATE;
                else fail("unknown mode; use random, random-figures or alternate");
            }
            else fail("unknown option (see -help)");
        }
    }
    if ((opt.fullscreen && (opt.root || opt.window || opt.wallpaper)) ||
        (opt.wallpaper && (opt.root || opt.window)))
        fail("choose one target: wallpaper, fullscreen preview, or an existing window");
    if (opt.wallpaper && !mode_chosen) opt.mode = MODE_ALTERNATE;
    if (opt.idle && !opt.wallpaper) fail("--idle requires --wallpaper");
    if (opt.idle && !LIFE_HAVE_XSS)
        fail("idle detection was disabled at build time; install libXss and rebuild, or use XScreenSaver");
    return opt;
}

static bool
limited_desktop(Display *display)
{
    int opcode, event, error;
    /* Xwayland cannot manage native Wayland windows or observe all their input. */
    if (XQueryExtension(display, "XWAYLAND", &opcode, &event, &error)) return true;
    const char *session = getenv("XDG_SESSION_TYPE");
    if (session && !strcmp(session, "wayland")) return true;
    if (getenv("WSL_INTEROP") || getenv("WSL_DISTRO_NAME")) return true;
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
}

static int
doctor(const struct options *opt)
{
    printf("Game of Life %s\nLibrary: %zu figures\n", LIFE_VERSION, figure_count);
    Display *display = XOpenDisplay(opt->display);
    if (!display) {
        puts("Display: unavailable\nOpen a terminal in your graphical session.\n"
             "On macOS, install/start XQuartz and sign in again.\n"
             "On Windows, use WSL 2 with WSLg. Do not set DISPLAY to :0 blindly.");
        return EXIT_FAILURE;
    }
    bool desktop = !limited_desktop(display);
    bool idle = false;
#if LIFE_HAVE_XSS
    int event, error;
    idle = XScreenSaverQueryExtension(display, &event, &error);
#endif
    XFontStruct *font = XLoadQueryFont(display, "6x10");
    if (!font) font = XLoadQueryFont(display, "fixed");
    printf("Display: %s\nPreview: %s\nWallpaper: %s\nIdle screensaver: %s\n",
           DisplayString(display), font ? "ready" : "missing X11 core fonts",
           desktop ? "X11 session; desktop-dependent" : "unavailable in this session; use preview",
           desktop && idle ? "ready" : "unavailable; use preview or an external screensaver");
    bool fonts_available = font != NULL;
    printf("Engine: CPU by default; GPU support %s\n", LIFE_HAVE_GPU ? "built (probe with --doctor --gpu)" : "not built");
    bool gpu_available = true;
    if (opt->gpu) {
        Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 16, 16, 0, 0, 0);
        struct gpu *gpu = gpu_open(display, window);
        gpu_available = gpu != NULL;
        printf("OpenGL: %s\n", gpu ? gpu_renderer(gpu) : gpu_error());
        gpu_close(gpu);
        XDestroyWindow(display, window);
    }
    if (font) XFreeFont(display, font);
    XCloseDisplay(display);
    return fonts_available && gpu_available ? EXIT_SUCCESS : EXIT_FAILURE;
}

static double
now(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
        fail("cannot read monotonic clock");
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static void
palette(struct app *app)
{
    /* Ink -> blue trails; mint births -> jade -> warm, settled colonies. */
    const int ink[3] = {3, 7, 14}, blue[3] = {27, 88, 117};
    const int mint[3] = {188, 248, 221}, jade[3] = {48, 173, 161};
    const int gold[3] = {190, 148, 89};
    for (int i = 0; i < LIFE_COLORS; ++i) {
        const int *from, *to;
        double t;
        if (i < LIFE_ALIVE) {
            from = ink; to = blue;
            t = app->options.trail ? (double)i / app->options.trail : 0;
            if (t > 1) t = 1;
            t *= t;
        } else if (i < LIFE_ALIVE + 8) {
            from = mint; to = jade;
            t = (double)(i - LIFE_ALIVE) / 8;
        } else {
            from = jade; to = gold;
            t = (double)(i - LIFE_ALIVE - 8) / (LIFE_COLORS - LIFE_ALIVE - 9);
        }
        XColor color = {.flags = DoRed | DoGreen | DoBlue};
        color.red = (unsigned short)((from[0] + (to[0] - from[0]) * t) * 257);
        color.green = (unsigned short)((from[1] + (to[1] - from[1]) * t) * 257);
        color.blue = (unsigned short)((from[2] + (to[2] - from[2]) * t) * 257);
        if (!XAllocColor(app->display, app->colormap, &color))
            fail("cannot allocate palette in this window's colormap");
        app->colors[app->allocated_colors++] = color.pixel;
        app->rgb[i][0] = color.red / 65535.0f;
        app->rgb[i][1] = color.green / 65535.0f;
        app->rgb[i][2] = color.blue / 65535.0f;
    }
}

static void
remember_scene(struct app *app)
{
    enum mode mode = app->options.mode;
    const struct figure *figure = app->options.figure;
    if (mode == MODE_ALTERNATE) {
        mode = app->history_count ? (app->scene + 1) % MODE_ALTERNATE : MODE_RANDOM;
        if (mode == MODE_FIGURE)
            figure = &figures[life_random(&app->life) % figure_count];
    }
    if (app->history_count == HISTORY) {
        memmove(app->history, app->history + 1, (HISTORY - 1) * sizeof(*app->history));
        --app->history_count;
    }
    app->history_index = app->history_count++;
    app->history[app->history_index] = app->options.replay ? app->replay :
        (struct entry){.mode = mode, .figure = figure, .random = app->life.random};
}

static int
grid_height(const struct app *app)
{
    int footer = app->font->ascent + app->font->descent + 16;
    return app->width >= 40 && app->height > footer + 16
        ? app->height - footer : app->height;
}

static void
reset_view(struct app *app)
{
    app->size = app->base_size;
    app->left = (app->width - app->life.columns * app->size) / 2;
    app->top = (grid_height(app) - app->life.rows * app->size) / 2;
    app->redraw = true;
}

static int
view_origin(int origin, int extent, int available)
{
    if (extent <= available) return (available - extent) / 2;
    if (origin > 0) return 0;
    if (origin < available - extent) return available - extent;
    return origin;
}

static void
zoom(struct app *app, int direction, int x, int y)
{
    static const int sizes[] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128};
    int size = app->size, height = grid_height(app);
    if (x < 0 || x >= app->width || y < 0 || y >= height) return;
    if (direction > 0) {
        for (size_t i = 0; i < sizeof(sizes) / sizeof(*sizes); ++i)
            if (sizes[i] > size) { size = sizes[i]; break; }
    } else {
        for (size_t i = sizeof(sizes) / sizeof(*sizes); i-- > 0;)
            if (sizes[i] < size) { size = sizes[i]; break; }
    }
    if (size == app->size) return;
    /* Change only the camera. Keep the cell under the pointer in place;
     * constrain the edges so zooming out never loses the world off-screen. */
    int left = x - (int)((int64_t)(x - app->left) * size / app->size);
    int top = y - (int)((int64_t)(y - app->top) * size / app->size);
    app->left = view_origin(left, app->life.columns * size, app->width);
    app->top = view_origin(top, app->life.rows * size, height);
    app->size = size;
    app->redraw = true;
}

static void
seed(struct app *app)
{
    struct entry *entry = &app->history[app->history_index];
    app->scene = entry->mode;
    app->figure = entry->figure;
    /* Store seeds, not grids: previous/next replays the same initial world. */
    app->life.random = entry->random;
    int width = app->width, height = grid_height(app);
    const struct figure *figure = app->scene == MODE_FIGURE ? app->figure : NULL;
    int size = app->scene == MODE_FIGURE
        ? scene_cell_size(width, height, app->options.size, figure)
        : scene_random_cell_size(width, height,
            app->options.random_size ? app->options.random_size : app->options.size);
    int columns = width / size, rows = height / size;
    if (!columns) columns = 1;
    if (!rows) rows = 1;
    if (app->options.replay) {
        columns = entry->columns;
        rows = entry->rows;
        size = width / columns;
        if (height / rows < size) size = height / rows;
        if (size < 1) size = 1;
        if (size > 128) size = 128;
    }
    if ((columns != app->life.columns || rows != app->life.rows) &&
        !life_resize(&app->life, columns, rows))
        fail("cannot allocate grid; try a larger -size");
    app->base_size = size;
    reset_view(app);
    if (app->scene == MODE_RANDOM) {
        life_seed(&app->life);
        /* Random soup opens on an established world, figures at generation zero. */
        for (int i = 0; i < 32; ++i)
            life_step(&app->life, (unsigned)app->options.trail, false);
    } else if (app->scene == MODE_FIGURE) {
        life_clear(&app->life);
        figure_stamp(&app->life, app->figure, (columns - app->figure->width) / 2,
                     (rows - app->figure->height) / 2, 0);
    } else {
        figures_scatter(&app->life);
    }
    bool changed = entry->columns != columns || entry->rows != rows;
    if (app->options.replay) {
        if (journal_checksum(&app->life) != entry->checksum)
            fail("history checksum mismatch: this generator/library cannot reproduce that world");
    } else if (changed && app->journaling) {
        entry->columns = columns;
        entry->rows = rows;
        entry->trail = (unsigned)app->options.trail;
        entry->checksum = journal_checksum(&app->life);
        if (!journal_append(app->journal_path, entry)) {
            fprintf(stderr, "game_of_life: history disabled (%s): %s\n",
                    app->journal_path, strerror(errno));
            app->journaling = false;
        }
    }
    if (app->own_window) {
        char title[96];
        const char *name = app->scene == MODE_FIGURE ? app->figure->name :
            (app->scene == MODE_RANDOM ? "random" : "random figures");
        snprintf(title, sizeof(title), "Game of Life - %s", name);
        XStoreName(app->display, app->window, title);
    }
    if (!scene_reset(&app->timing, &app->life, figure,
                     (unsigned)app->options.fps, (unsigned)app->options.seconds))
        fail("cannot allocate scene history");
    if (app->gpu && !gpu_seed(app->gpu, &app->life, app->timing.visited))
        fail(gpu_error());
    app->fading = 0;
    app->gpu_pending = false;
    app->redraw = true;
}

static void
navigate(struct app *app, int direction)
{
    if (direction < 0) {
        if (!app->history_index) return;
        --app->history_index;
    } else if (app->history_index + 1 < app->history_count) {
        ++app->history_index;
    } else {
        remember_scene(app);
    }
    seed(app);
}

static void
resize(struct app *app, int width, int height)
{
    if (width == app->width && height == app->height) return;
    if (width < 1 || height < 1 || width > SHRT_MAX || height > SHRT_MAX)
        fail("unsupported window dimensions");
    app->width = width;
    app->height = height;
    seed(app);
}

static void
flush_color(struct app *app, int color)
{
    if (!app->counts[color]) return;
    XSetForeground(app->display, app->gc, app->colors[color]);
    XFillRectangles(app->display, app->window, app->gc,
                    app->rectangles[color], app->counts[color]);
    app->counts[color] = 0;
}

static void
caption(struct app *app, Drawable drawable, GC gc, int height, unsigned long color)
{
    /* The footer lies outside the grid, so its static label needs drawing
     * only after an expose, a resize or a scene change. Core fonts are Latin-1. */
    char label[160];
    if (app->scene == MODE_FIGURE)
        snprintf(label, sizeof(label), app->options.english ? "Figure: %s" : "Mode : une figure - %s",
                 app->figure->name);
    else
        snprintf(label, sizeof(label), "%s", app->options.english
                 ? (app->scene == MODE_RANDOM ? "Mode: random" : "Mode: random figures")
                 : (app->scene == MODE_RANDOM ? "Mode : al\351atoire" : "Mode : figures al\351atoires"));
    if (app->options.fancy)
        strncat(label, " | fancy", sizeof(label) - strlen(label) - 1);
    if (app->size != app->base_size) {
        size_t used = strlen(label);
        snprintf(label + used, sizeof(label) - used, " | zoom %.2gx",
                 (double)app->size / app->base_size);
    }
    int length = (int)strlen(label);
    while (length && XTextWidth(app->font, label, length) > app->width - 16) --length;
    XSetForeground(app->display, gc, color);
    XDrawString(app->display, drawable, gc,
                app->width - 8 - XTextWidth(app->font, label, length),
                height - 8 - app->font->descent, label, length);
    if (app->own_window) {
        const char *hint = app->options.english
            ? (app->presenting ? "Left = next ; right = previous ; wheel = zoom ; middle = reset ; key = desktop"
                               : "Left = next ; right = previous ; wheel = zoom ; middle = reset")
            : (app->presenting ? "Gauche = suivant ; droit = pr\351c\351dent ; molette = zoom ; milieu = cadrage ; touche = bureau"
                               : "Gauche = suivant ; droit = pr\351c\351dent ; molette = zoom ; milieu = cadrage");
        int hint_length = (int)strlen(hint);
        if (XTextWidth(app->font, hint, hint_length) +
            XTextWidth(app->font, label, length) + 40 <= app->width)
            XDrawString(app->display, drawable, gc, 8,
                        height - 8 - app->font->descent, hint, hint_length);
    }
}

static void
draw(struct app *app)
{
    if (app->gpu) {
        int footer = app->height - grid_height(app);
        if (app->redraw) {
            int height = footer ? footer : 1;
            Pixmap bitmap = XCreatePixmap(app->display, app->window, (unsigned)app->width, (unsigned)height, 1);
            GC gc = XCreateGC(app->display, bitmap, 0, NULL);
            XSetForeground(app->display, gc, 0);
            XFillRectangle(app->display, bitmap, gc, 0, 0, (unsigned)app->width, (unsigned)height);
            XSetFont(app->display, gc, app->font->fid);
            if (footer) caption(app, bitmap, gc, height, 1);
            bool ok = gpu_caption(app->gpu, bitmap, app->width, height);
            XFreeGC(app->display, gc);
            XFreePixmap(app->display, bitmap);
            if (!ok) fail(gpu_error());
        }
        gpu_draw(app->gpu, app->width, app->height, app->height - footer,
                 app->size, app->left, app->top, !app->options.square_cells, app->options.fancy);
        app->redraw = false;
        XFlush(app->display);
        return;
    }
    if (app->redraw) {
        XSetForeground(app->display, app->gc, app->colors[0]);
        XFillRectangle(app->display, app->window, app->gc, 0, 0,
                       (unsigned)app->width, (unsigned)app->height);
    }
    int size = app->size, gap = size >= 5 ? 1 : 0;
    const struct life *life = &app->life;
    int height = grid_height(app);
    bool clipped = app->left < 0 || app->top < 0 ||
        app->left + life->columns * size > app->width || app->top + life->rows * size > height;
    if (clipped) {
        XRectangle view = {0, 0, (unsigned short)app->width, (unsigned short)height};
        XSetClipRectangles(app->display, app->gc, 0, 0, &view, 1, Unsorted);
    }
    /* Cull before converting to X11's 16-bit coordinates: a zoomed world
     * can be much wider than the window or a signed short. */
    int first_x = app->left < 0 ? -app->left / size : 0;
    int first_y = app->top < 0 ? -app->top / size : 0;
    int end_x = (app->width - app->left + size - 1) / size;
    int end_y = (height - app->top + size - 1) / size;
    if (end_x > life->columns) end_x = life->columns;
    if (end_y > life->rows) end_y = life->rows;
    for (int y = first_y; y < end_y; ++y) {
        int left = app->redraw ? first_x : life->dirty_left[y];
        int right = app->redraw ? end_x : life->dirty_right[y];
        if (left < first_x) left = first_x;
        if (right > end_x) right = end_x;
        for (int x = left; x < right; ++x) {
            int i = (y + 1) * life->stride + x + 1;
            if (!app->redraw && !(x & 7) && x + 8 <= right &&
                !memcmp(life->cells + i, life->previous + i, 8)) {
                x += 7;
                continue;
            }
            int color = life->cells[i];
            if (app->redraw ? !color : color == life->previous[i]) continue;
            int run = 1;
            /* Fine grids have no gutters: merge horizontal runs of changes. */
            while (!gap && x + run < right && life->cells[i + run] == color &&
                   (app->redraw || life->previous[i + run] != color)) ++run;
            XRectangle rectangle = {
                .x = (short)(app->left + x * size),
                .y = (short)(app->top + y * size),
                .width = (unsigned short)(run * size - gap),
                .height = (unsigned short)(size - gap)
            };
            app->rectangles[color][app->counts[color]++] = rectangle;
            if (app->counts[color] == BATCH) flush_color(app, color);
            x += run - 1;
        }
    }
    for (int i = 0; i < LIFE_COLORS; ++i) flush_color(app, i);
    if (clipped) XSetClipMask(app->display, app->gc, None);
    if (app->redraw && app->width >= 40 &&
        app->height > app->font->ascent + app->font->descent + 32)
        caption(app, app->window, app->gc, app->height, app->colors[LIFE_ALIVE + 13]);
    app->redraw = false;
    XFlush(app->display);
}

static void
advance(struct app *app)
{
    if (app->gpu) {
        /* Observe the last displayed generation before computing the next.
         * This preserves the fade boundary without waiting immediately after
         * dispatch; the regular frame-rate sleep gives the GPU time to finish. */
        if (app->gpu_pending) {
            struct gpu_stats stats;
            if (!gpu_observe(app->gpu, &stats)) fail(gpu_error());
            app->life.population = stats.population;
            app->life.changes = stats.changes;
            app->life.hash = stats.hash;
            if (scene_observe(&app->timing, &app->life, (int)stats.fresh) && !app->fading)
                app->fading = (unsigned)app->options.trail + 2;
        }
        bool sample = !app->fading && (app->timing.generation + 1) % app->timing.fps == 0;
        if (!gpu_step(app->gpu, (unsigned)app->options.trail, app->fading != 0, sample))
            fail(gpu_error());
        app->gpu_pending = !app->fading;
    } else {
        life_step(&app->life, (unsigned)app->options.trail, app->fading != 0);
    }
    if (app->fading) {
        if (--app->fading == 0) navigate(app, 1);
        return;
    }
    if (!app->gpu && scene_finished(&app->timing, &app->life))
        app->fading = (unsigned)app->options.trail + 2;
}

static void
open_window(struct app *app)
{
    app->display = XOpenDisplay(app->options.display);
    if (!app->display) fail("cannot open X display; run game_of_life --doctor for help");
    Display *display = app->display;
    int screen = DefaultScreen(display);
    Window root = app->root = RootWindow(display, screen);
    if (app->options.wallpaper && limited_desktop(display))
        fail("wallpaper/idle modes need an X11 desktop; use --alternate for a preview in this session");
#if LIFE_HAVE_XSS
    if (app->options.idle) {
        int event, error;
        if (!XScreenSaverQueryExtension(display, &event, &error))
            fail("--idle needs the X11 screen saver extension");
    }
#endif
    const char *provided = getenv("XSCREENSAVER_WINDOW");
    app->window = app->options.window;
    if (!app->options.wallpaper && !app->window && provided && *provided)
        app->window = number(provided, 1, ULONG_MAX);
    if (!app->window && app->options.root) app->window = root;
    app->own_window = !app->window;
    if (app->own_window) {
        bool screen_sized = app->options.fullscreen || app->options.wallpaper;
        int width = screen_sized ? DisplayWidth(display, screen) : 960;
        int height = screen_sized ? DisplayHeight(display, screen) : 600;
        app->window = XCreateSimpleWindow(display, root, 0, 0,
            (unsigned)width, (unsigned)height, 0, 0, 0);
        XStoreName(display, app->window, "Game of Life");
        XClassHint hint = {.res_name = "game_of_life", .res_class = "GameOfLife"};
        XSetClassHint(display, app->window, &hint);
        unsigned long pid = (unsigned long)getpid();
        XChangeProperty(display, app->window, XInternAtom(display, "_NET_WM_PID", False),
                        XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
        app->wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, app->window, &app->wm_delete, 1);
        if (app->options.wallpaper) {
            XSetWindowAttributes attributes = {.override_redirect = True};
            XChangeWindowAttributes(display, app->window, CWOverrideRedirect, &attributes);
            XWMHints hints = {.flags = InputHint, .input = False};
            XSetWMHints(display, app->window, &hints);
            Atom type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
            Atom desktop = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
            XChangeProperty(display, app->window, type, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)&desktop, 1);
            unsigned long desktops = 0xffffffffUL;
            XChangeProperty(display, app->window, XInternAtom(display, "_NET_WM_DESKTOP", False),
                            XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&desktops, 1);
            XSelectInput(display, root, StructureNotifyMask);
        }
        if (app->options.fullscreen) {
            Atom state = XInternAtom(display, "_NET_WM_STATE", False);
            Atom fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
            XChangeProperty(display, app->window, state, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)&fullscreen, 1);
        }
    }
    long mask = ExposureMask | StructureNotifyMask | VisibilityChangeMask;
    if (app->own_window) {
        mask |= ButtonPressMask;
        if (!app->options.wallpaper || app->options.idle) mask |= KeyPressMask;
    }
    XSelectInput(display, app->window, mask);
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(display, app->window, &attributes))
        fail("cannot inspect target window");
    app->visible = attributes.map_state == IsViewable;
    app->colormap = attributes.colormap;
    palette(app);
    if (app->options.gpu) {
        app->gpu = gpu_open(display, app->window);
        if (!app->gpu) fail(gpu_error());
        gpu_palette(app->gpu, &app->rgb[0][0]);
    }
    app->gc = XCreateGC(display, app->window, 0, NULL);
    XSetGraphicsExposures(display, app->gc, False);
    app->font = XLoadQueryFont(display, "6x10");
    if (!app->font) app->font = XLoadQueryFont(display, "fixed");
    if (!app->font) fail("cannot load the X11 fixed font");
    XSetFont(display, app->gc, app->font->fid);
    app->life.random = app->options.seed;
    remember_scene(app);
    resize(app, attributes.width, attributes.height);
    if (app->own_window) {
        if (app->options.wallpaper) XLowerWindow(display, app->window);
        XMapWindow(display, app->window);
    }
}

static void
return_to_desktop(struct app *app)
{
    XUngrabKeyboard(app->display, CurrentTime);
    XUngrabPointer(app->display, CurrentTime);
    XLowerWindow(app->display, app->window);
    app->presenting = false;
    app->redraw = true;
    app->idle_after = now() + app->options.idle;
    XFlush(app->display);
}

static void
check_idle(struct app *app, double time)
{
#if LIFE_HAVE_XSS
    if (!app->options.idle || app->presenting || time < app->idle_check) return;
    app->idle_check = time + 1;
    if (time < app->idle_after) return;
    XScreenSaverInfo info = {0};
    if (!XScreenSaverQueryInfo(app->display, app->root, &info))
        fail("cannot query X11 idle time");
    if (info.idle < (unsigned long)app->options.idle * 1000 || info.state == ScreenSaverOn)
        return;
    /* Never steal a grab from a locker or another application. Both grabs must
     * succeed before raising the wallpaper; neither changes the input focus. */
    if (XGrabKeyboard(app->display, app->window, False, GrabModeAsync,
                      GrabModeAsync, CurrentTime) != GrabSuccess) return;
    if (XGrabPointer(app->display, app->window, False, ButtonPressMask | ButtonReleaseMask,
                     GrabModeAsync, GrabModeAsync, None, None, CurrentTime) != GrabSuccess) {
        XUngrabKeyboard(app->display, CurrentTime);
        XFlush(app->display);
        return;
    }
    app->presenting = app->visible = app->redraw = true;
    XRaiseWindow(app->display, app->window);
#else
    (void)app;
    (void)time;
#endif
}

static void
events(struct app *app)
{
    while (!stopping && XPending(app->display)) {
        XEvent event;
        XNextEvent(app->display, &event);
        switch (event.type) {
        case Expose: app->redraw = true; break;
        case ConfigureNotify:
            if (app->options.wallpaper && event.xconfigure.window == app->root)
                XMoveResizeWindow(app->display, app->window, 0, 0,
                                  (unsigned)event.xconfigure.width, (unsigned)event.xconfigure.height);
            else if (event.xconfigure.window == app->window)
                resize(app, event.xconfigure.width, event.xconfigure.height);
            break;
        case MapNotify: app->visible = app->redraw = true; break;
        case UnmapNotify: app->visible = false; break;
        case VisibilityNotify:
            app->visible = event.xvisibility.state != VisibilityFullyObscured;
            break;
        case DestroyNotify:
            if (event.xdestroywindow.window == app->window) {
                stopping = 1;
                window_destroyed = true;
            }
            break;
        case ClientMessage:
            if ((Atom)event.xclient.data.l[0] == app->wm_delete) stopping = 1;
            break;
        case ButtonPress:
            if (event.xbutton.button == Button1) navigate(app, 1);
            else if (event.xbutton.button == Button2) reset_view(app);
            else if (event.xbutton.button == Button3) navigate(app, -1);
            else if (event.xbutton.button == Button4 || event.xbutton.button == Button5)
                zoom(app, event.xbutton.button == Button4 ? 1 : -1,
                     event.xbutton.x, event.xbutton.y);
            break;
        case KeyPress: {
            if (app->presenting) {
                return_to_desktop(app);
                break;
            }
            if (app->options.wallpaper) break;
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (app->options.fullscreen || key == XK_Escape || key == XK_q)
                stopping = 1;
            else if (key == XK_space && !app->fading)
                app->fading = (unsigned)app->options.trail + 2;
            break;
        }
        }
    }
}

int
main(int argc, char **argv)
{
    struct app app = {.options = options(argc, argv)};
    if (app.options.doctor) return doctor(&app.options);
    app.journaling = !app.options.no_history && !app.options.replay &&
        (app.options.wallpaper || app.options.history_path);
    if (app.journaling || app.options.replay) {
        app.journal_path = app.options.history_path ? strdup(app.options.history_path) : journal_path();
        if (!app.journal_path) {
            if (app.options.replay) fail("cannot locate history; use --history FILE");
            fprintf(stderr, "game_of_life: history disabled; cannot locate state directory\n");
            app.journaling = false;
        }
    }
    if (app.options.replay) {
        const char *error = journal_read(app.journal_path, app.options.replay_line, &app.replay);
        if (error) fail(error);
        app.options.mode = app.replay.mode;
        app.options.figure = app.replay.figure;
        app.options.trail = (int)app.replay.trail;
    }
    struct sigaction action = {.sa_handler = stop};
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    open_window(&app);
    drawing_window = app.window;
    XSetErrorHandler(x_error);
    double next = now(), interval = 1.0 / app.options.fps;
    struct pollfd fd = {.fd = ConnectionNumber(app.display), .events = POLLIN};
    while (!stopping) {
        events(&app);
        if (stopping) break;
        double time = now();
        check_idle(&app, time);
        if (app.visible && (app.redraw || time >= next)) {
            if (time >= next && !app.redraw) advance(&app);
            draw(&app);
            /* Never catch up missed frames in a CPU-consuming burst. */
            next = now() + interval;
        }
        int timeout = app.visible ? (int)((next - now()) * 1000) + 1 : 1000;
        if (timeout < 0) timeout = 0;
        int ready = poll(&fd, 1, timeout);
        if (ready < 0 && errno != EINTR) fail("poll failed");
        if (ready > 0 && (fd.revents & (POLLERR | POLLHUP | POLLNVAL))) break;
    }
    gpu_close(app.gpu);
    scene_free(&app.timing);
    life_free(&app.life);
    free(app.journal_path);
    XFreeFont(app.display, app.font);
    XFreeGC(app.display, app.gc);
    if (!window_destroyed) {
        XFreeColors(app.display, app.colormap, app.colors, app.allocated_colors, 0);
        if (app.own_window) XDestroyWindow(app.display, app.window);
    }
    XCloseDisplay(app.display);
    return EXIT_SUCCESS;
}
