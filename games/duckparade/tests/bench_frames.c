/*
 * Duck Parade: the cost of every frame of a replayed run, robust to the host's noise (make duckparade-bench).
 * The run is replayed from an input script (the bot's recorded presses: tools/bench.sh); after each frame the
 * picture is rendered twice more and the cheapest of the three renders is kept, so a frame's cost is its own and
 * not the host scheduler's. Prints the average, the median, the 99th and 99.9th percentiles and the maximum.
 *
 *   bench_frames --input SCRIPT --frames N [--opt key=value]...
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 */
#include "dp.h"
#include "rs_host.h"
#include "rs_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const rs_game *rs_game_main(void);

typedef struct { int frame, port; uint16_t buttons; } event_t;
static event_t ev[8192];
static int nev;

static void add(int f, int port, uint16_t b)
{
    if (nev >= 8192) return;
    int i = nev++;
    while (i > 0 && ev[i - 1].frame > f) { ev[i] = ev[i - 1]; i--; }
    ev[i].frame = f, ev[i].port = port, ev[i].buttons = b;
}

static void load(const char *path)
{
    size_t n;
    char *s = rsd_load_file(path, &n);
    if (!s) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    for (char *line = s, *next; line && *line; line = next) {      /* not strtok: rsd_buttons uses it */
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        int f;
        char p[8], tap[8], b[32];
        if (sscanf(line, "%d %7s %7s %31s", &f, p, tap, b) == 4 && !strcmp(tap, "tap")) {
            add(f, p[1] - '1', rsd_buttons(b));
            add(f + 2, p[1] - '1', 0);
        }
    }
}

static int cmp(const void *a, const void *b) { uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b; return x < y ? -1 : x > y; }
static void quiet(const char *l) { (void)l; }

int main(int argc, char **argv)
{
    int frames = 12000;
    const char *input = NULL;
    rs_host_set_log(quiet);
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--input")) input = argv[i + 1];
        else if (!strcmp(argv[i], "--frames")) frames = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--opt")) {
            char kv[128];
            snprintf(kv, sizeof kv, "%s", argv[i + 1]);
            char *eq = strchr(kv, '=');
            if (eq) { *eq = 0; rs_host_set_option(kv, eq + 1); }
        }
    }
    if (input) load(input);
    rs_host_init(rs_game_main());
    uint32_t *cost = calloc((size_t)frames, sizeof *cost);
    uint16_t pads[RS_PAD_MAX] = {0};
    int e = 0, worst = 60, sprites = 0;
    double sum = 0;
    for (int f = 0; f < frames; f++) {
        while (e < nev && ev[e].frame <= f) { pads[ev[e].port] = ev[e].buttons; e++; }
        for (int p = 0; p < RS_PAD_MAX; p++) rs_host_set_pad(p, pads[p], 1);
        rs_host_frame();
        const rs_host_stats *st = rs_host_stats_last();
        uint32_t r = st->render_us;
        for (int k = 0; k < 2; k++) {
            uint64_t t0 = rs_host_time_us();
            rs_host_render();
            uint32_t d = (uint32_t)(rs_host_time_us() - t0);
            if (d < r) r = d;
        }
        cost[f] = st->update_us + r + st->audio_us;
        if (st->sprites > sprites) sprites = st->sprites;
        if (f >= 60) { sum += cost[f]; if (cost[f] > cost[worst]) worst = f; }
    }
    int n = frames - 60;
    uint32_t w = cost[worst];
    qsort(cost + 60, (size_t)n, sizeof *cost, cmp);
    const world *wd = dp_test_world(NULL);
    printf("frames %d: avg %.0f us, median %u, p99 %u, p99.9 %u, max %u us (frame %d); %d sprites at most; "
           "score %d, lanes %d\n", n, sum / n, cost[60 + n / 2], cost[60 + n * 99 / 100], cost[60 + n * 999 / 1000], w,
           worst, sprites, world_score(wd, 0), wd->d[0].max_col - START_COL);
    rs_host_shutdown();
    return 0;
}
