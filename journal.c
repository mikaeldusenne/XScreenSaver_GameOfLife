#define _POSIX_C_SOURCE 200809L
#include "journal.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

char *
journal_path(void)
{
    const char *base = getenv("XDG_STATE_HOME"), *suffix = "/game-of-life/history.tsv";
    if (!base || *base != '/') {
        base = getenv("HOME");
        suffix = "/.local/state/game-of-life/history.tsv";
    }
    if (!base || *base != '/') return NULL;
    size_t length = strlen(base) + strlen(suffix) + 1;
    char *path = malloc(length);
    if (path) snprintf(path, length, "%s%s", base, suffix);
    return path;
}

uint64_t
journal_checksum(const struct life *life)
{
    uint64_t hash = LIFE_EMPTY_HASH;
    /* Include colors (ages and trails), exclude the scratch halo. */
    for (int y = 1; y <= life->rows; ++y)
        for (int x = 1; x <= life->columns; ++x)
            hash = (hash ^ life->cells[y * life->stride + x]) * UINT64_C(1099511628211);
    return hash;
}

static bool
parents(const char *path)
{
    char *copy = strdup(path);
    if (!copy) return false;
    for (char *p = copy + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        int result = mkdir(copy, 0700);
        *p = '/';
        if (result != 0 && errno != EEXIST) { free(copy); return false; }
    }
    free(copy);
    return true;
}

bool
journal_append(const char *path, const struct entry *entry)
{
    if (!path || !*path) { errno = EINVAL; return false; }
    char line[512];
    const char *mode = entry->mode == MODE_RANDOM ? "random" :
        entry->mode == MODE_FIGURE ? "figure" : "mix";
    int length = snprintf(line, sizeof(line),
        "life1\t%jd\t%s\t%d\t%d\t%08" PRIx32 "\t%u\t%s\t%016" PRIx64 "\n",
        (intmax_t)time(NULL), mode, entry->columns, entry->rows, entry->random,
        entry->trail, entry->mode == MODE_FIGURE ? entry->figure->name : "-", entry->checksum);
    if (length < 0 || (size_t)length >= sizeof(line)) { errno = EOVERFLOW; return false; }
    if (!parents(path)) return false;
    /* One append per scene, no descriptor or per-frame disk work. O_NONBLOCK
     * avoids getting stuck if the destination was accidentally a FIFO. */
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_NONBLOCK | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    struct stat info;
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(fd);
        errno = EINVAL;
        return false;
    }
    ssize_t written;
    do { written = write(fd, line, (size_t)length); } while (written < 0 && errno == EINTR);
    int error = written < 0 ? errno : EIO;
    int closed = close(fd);
    if (written != length) { errno = error; return false; }
    return closed == 0;
}

static bool
integer(const char *text, int base, uintmax_t limit, uintmax_t *value)
{
    char *end;
    if (!*text || *text == '-' || *text == '+') return false;
    errno = 0;
    *value = strtoumax(text, &end, base);
    return !errno && !*end && *value <= limit;
}

static const char *
decode(char *line, struct entry *entry)
{
    char *fields[9], *save;
    unsigned count = 0;
    for (char *s = strtok_r(line, "\t\r\n ", &save); s; s = strtok_r(NULL, "\t\r\n ", &save)) {
        if (count == 9) return "too many history fields";
        fields[count++] = s;
    }
    if (count != 9) return "incomplete history record";
    if (strcmp(fields[0], "life1")) return "unsupported history version";
    uintmax_t date, columns, rows, random, trail, checksum;
    if (!integer(fields[1], 10, INT64_MAX, &date) ||
        !integer(fields[3], 10, 32767, &columns) || !columns ||
        !integer(fields[4], 10, 32767, &rows) || !rows ||
        (columns + 2) * (rows + 2) > 4 * 1024 * 1024 ||
        !integer(fields[5], 16, UINT32_MAX, &random) ||
        !integer(fields[6], 10, 31, &trail) ||
        !integer(fields[8], 16, UINT64_MAX, &checksum)) return "invalid history values";
    enum mode mode;
    const struct figure *figure = NULL;
    if (!strcmp(fields[2], "random")) mode = MODE_RANDOM;
    else if (!strcmp(fields[2], "mix")) mode = MODE_RANDOM_FIGURES;
    else if (!strcmp(fields[2], "figure")) {
        mode = MODE_FIGURE;
        figure = figure_find(fields[7]);
        if (!figure) return "history figure is missing from this library";
    } else return "unknown history mode";
    if (mode != MODE_FIGURE && strcmp(fields[7], "-")) return "unexpected history figure";
    *entry = (struct entry){.mode = mode, .figure = figure, .random = (uint32_t)random,
        .columns = (int)columns, .rows = (int)rows, .trail = (unsigned)trail, .checksum = (uint64_t)checksum};
    return NULL;
}

const char *
journal_read(const char *path, unsigned long wanted, struct entry *entry)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return "cannot open history file";
    struct stat info;
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(fd);
        return "history must be a regular file";
    }
    FILE *file = fdopen(fd, "r");
    if (!file) { close(fd); return "cannot read history file"; }
    char line[512], selected[512] = "";
    unsigned long number = 0;
    const char *error = NULL;
    while (fgets(line, sizeof(line), file)) {
        ++number;
        if (!strchr(line, '\n')) {
            /* An interrupted final append is ignored for --replay 0. */
            if (feof(file) && !wanted) break;
            error = "overlong or incomplete history line";
            break;
        }
        if (!wanted || number == wanted) strcpy(selected, line);
        if (wanted && number == wanted) break;
    }
    if (ferror(file)) error = "cannot read history file";
    fclose(file);
    if (error) return error;
    if (!*selected) return "history line not found";
    return decode(selected, entry);
}
