/*
 * Leady Squid: the obstacle caps sit flush on their columns, on the real picture.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
 *
 * The whole game runs (runtime, draw, sound) with a scripted player: many runs,
 * each started after a random wait on the title or get-ready screen (so each
 * run's course comes from a different seed), a simple autopilot that swims
 * through a random number of obstacles and then gives up, and a retry at a
 * random moment. After every frame, every obstacle on screen is checked:
 *   - OAM: its top cap sprite ends exactly at the gap top, its bottom cap starts
 *     exactly at the gap bottom (the caps are sprites at the gap's pixel height);
 *   - pixels: the picture is rendered again with BG2 (the column bodies) off,
 *     sprites and text hidden, and compared with BG2 on: that difference is
 *     the column body alone. There must be none of it inside the gap, and the
 *     body must be there right past each cap (above the top cap, below the
 *     bottom cap), within the obstacle's 24 columns and not beside them.
 * Every gap value of the first theme (--opt skip=N) and every scroll phase must be covered.
 *
 *   test_caps [--runs N] [--seed S] [--opt key=value]...   (--opt skip=10 for coral, ...)
 */
#include "ls.h"
#include "rs_host.h"
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const rs_game *rs_game_main(void);

#define SW RS_SCREEN_W
#define SH RS_SCREEN_H

static uint16_t with_bg2[SW * SH], without_bg2[SW * SH];
static uint8_t body[SH][SW];                     /* 1 = a BG2 pixel shows here */
static int fails, fail_courses, checked_obstacles, checked_frames;
static uint32_t last_fail_seed;
static uint8_t gap_seen[THEMES][PLAY_H], phase_seen[8];
static uint32_t rng_state = 12345;

static uint32_t rnd(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void quiet_log(const char *line) { (void)line; }

static void render_without_sprites(int bg2, uint16_t *out)
{
    static uint8_t flags[RS_OAM_MAX];
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        flags[i] = s->flags;
        s->flags |= RS_SPR_HIDE;
    }
    rs_bg_enable(RS_BG1, 0);
    rs_bg_enable(RS_BG2, bg2);
    rs_host_render();
    memcpy(out, rs_host_framebuffer(), sizeof with_bg2);
    rs_bg_enable(RS_BG1, 1);
    rs_bg_enable(RS_BG2, 1);
    for (int i = 0; i < RS_OAM_MAX; i++) rs_oam(i)->flags = flags[i];
}

static int cap_in_oam(int sprite, int x, int y)
{
    const ls_sprite_def *d = &ls_spr[sprite];
    for (int i = 0; i < RS_OAM_MAX; i++) {
        const rs_sprite *s = rs_oam(i);
        if (s->used && !(s->flags & RS_SPR_HIDE) && s->tile == d->tile && s->x == x && s->y == y) return 1;
    }
    return 0;
}

/* first and last rows of the body in the column at screen x, around the gap (for the report) */
static void body_rows(int x, int gap_top, int *top_end, int *bot_start)
{
    int y = gap_top + GAP / 2;
    *top_end = *bot_start = -1;
    for (int k = y; k >= 0; k--)
        if (body[k][x]) { *top_end = k; break; }
    for (int k = y; k < PLAY_H; k++)
        if (body[k][x]) { *bot_start = k; break; }
}

static void fail(int frame, uint32_t seed, const obstacle *o, int ox, const char *what)
{
    static int printed;
    static uint32_t printed_seed;
    fails++;
    if (seed != last_fail_seed) fail_courses++, last_fail_seed = seed;
    /* one report per course, with the column's centre on screen */
    int te = -1, bs = -1, cx = ox + OBST_W / 2;
    if (printed >= 10 || seed == printed_seed || cx < 0 || cx >= SW) return;
    printed++;
    printed_seed = seed;
    body_rows(cx, o->gap_top, &te, &bs);
    printf("  FAIL frame %d seed %08x obstacle %d (theme %d, gap %d..%d, screen x %d): %s; the column body ends at "
           "row %d above the gap and starts at row %d below it\n",
           frame, (unsigned)seed, o->index, o->theme, o->gap_top, o->gap_top + GAP - 1, ox, what, te, bs);
}

static void check_frame(const world *w, int frame)
{
    int sx = world_scroll_px(w);
    render_without_sprites(1, with_bg2);
    render_without_sprites(0, without_bg2);
    for (int i = 0; i < SW * SH; i++) body[i / SW][i % SW] = with_bg2[i] != without_bg2[i];
    checked_frames++;
    phase_seen[sx & 7] = 1;
    for (int i = 0; i < w->nob; i++) {
        const obstacle *o = &w->ob[i];
        int ox = o->x - sx, g = o->gap_top;
        int x0 = ox < 0 ? 0 : ox, x1 = ox + OBST_W > SW ? SW : ox + OBST_W;
        if (x1 <= x0) continue;
        checked_obstacles++;
        gap_seen[o->theme][g] = 1;
        /* the caps (sprites) at the gap's exact pixel height */
        const ls_sprite_def *cap = &ls_spr[SPR_CAP_KELP + o->theme * 2];
        if (ox > -(int)cap->w && ox < SW) {
            if (!cap_in_oam(SPR_CAP_KELP + o->theme * 2, ox, g - CAP_H))
                fail(frame, w->seed, o, ox, "no top cap sprite ending at the gap top");
            if (!cap_in_oam(SPR_CAP_KELP + o->theme * 2 + 1, ox, g + GAP))
                fail(frame, w->seed, o, ox, "no bottom cap sprite starting at the gap bottom");
        }
        /* no column body inside the gap (nor beside the column) */
        int n = 0;
        for (int y = g; y < g + GAP; y++)
            for (int x = x0 - 8; x < x1 + 8; x++)
                if (x >= 0 && x < SW) n += body[y][x];
        if (n) fail(frame, w->seed, o, ox, "column body pixels inside the gap (stalk out of place, cap looks detached)");
        /* the body right past each cap, within the obstacle's columns only */
        if (x1 - x0 < 8) continue;
        const int rows[2] = {g - CAP_H - 1, g + GAP + CAP_H};
        for (int r = 0; r < 2; r++) {
            int in = 0, out = 0;
            for (int x = x0 - 8; x < x1 + 8; x++) {
                if (x < 0 || x >= SW) continue;
                if (x >= x0 && x < x1) in += body[rows[r]][x];
                else out += body[rows[r]][x];
            }
            if (in < 2) fail(frame, w->seed, o, ox, r ? "no column body just below the bottom cap (gap under it)"
                                                      : "no column body just above the top cap (gap over it)");
            if (out) fail(frame, w->seed, o, ox, "column body beside the obstacle (horizontal offset)");
        }
    }
}

/* the scripted player: a simple autopilot through the next gap, until it gives up */
static int autopilot(const world *w, int p, int give_up_at)
{
    const squid *s = &w->sq[p];
    if (s->state != SQ_SWIM || s->score >= give_up_at) return 0;
    const obstacle *o = world_next_obstacle(w, p);
    int target = o ? o->gap_top + GAP - 15 : PLAY_H / 2;
    return (int)(s->y >> 16) > target && s->vy >= 0;
}

int main(int argc, char **argv)
{
    int runs = 60;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--runs") && i + 1 < argc) runs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) rng_state = ((uint32_t)strtoul(argv[++i], NULL, 0) + 1) * 2654435761u;
        else if (!strcmp(argv[i], "--opt") && i + 1 < argc) {
            char kv[128];
            snprintf(kv, sizeof kv, "%s", argv[++i]);
            char *eq = strchr(kv, '=');
            if (!eq) { fprintf(stderr, "--opt needs key=value\n"); return 2; }
            *eq = 0;
            rs_host_set_option(kv, eq + 1);
        } else {
            fprintf(stderr, "usage: test_caps [--runs N] [--seed S] [--opt key=value]...\n");
            return 2;
        }
    }
    rs_host_set_log(quiet_log);
    rs_host_set_option("music", "0");
    rs_host_set_option("sound", "0");
    rs_host_init(rs_game_main());
    int done = 0, wait = (int)(rnd() % 64), give_up[MAX_PLAYERS] = {0, 0}, frame = 0, first_theme = -1;
    int last_sx = -1, last_st = -1;
    uint32_t last_seed = 0;
    while (done < runs && frame < runs * 6000) {
        int st;
        const world *w = ls_test_world(&st);
        uint16_t pad[MAX_PLAYERS] = {0, 0};
        if (st == DS_TITLE || st == DS_READY) {
            if (--wait <= 0) {
                pad[0] = RS_BTN_A;
                for (int p = 0; p < MAX_PLAYERS; p++) give_up[p] = w->first_index + 1 + (int)(rnd() % (THEME_BAND - 1));
                wait = 1 << 30;
            }
        } else if (st == DS_PLAY || st == DS_DEAD) {
            for (int p = 0; p < w->players; p++)
                if (autopilot(w, p, give_up[p])) pad[p] = RS_BTN_A;
        } else if (st == DS_OVER) {
            if (wait == 1 << 30) wait = RETRY_LOCK + 1 + (int)(rnd() % 24);
            if (--wait <= 0) {
                pad[0] = RS_BTN_A;
                done++;
                wait = 2 + (int)(rnd() % 64);
            }
        }
        for (int p = 0; p < MAX_PLAYERS; p++) rs_host_set_pad(p, pad[p], 1);
        rs_host_frame();
        frame++;
        w = ls_test_world(&st);
        if (first_theme < 0) first_theme = w->ob[0].theme;
        /* the course moves (or changes) only with the scroll, a new run or a new state: every frame
         * of the first 96 px of a run, then every third pixel of scroll (all 8 phases over a run) */
        int sx = world_scroll_px(w);
        if (st != last_st || w->seed != last_seed || (sx != last_sx && (sx < 96 || sx % 3 == 0)))
            check_frame(w, frame);
        last_sx = sx;
        last_st = st;
        last_seed = w->seed;
    }
    rs_host_shutdown();
    static const char *names[THEMES] = {"kelp", "coral", "masts", "chains"};
    char cover[160] = "";
    for (int t = 0; t < THEMES; t++) {
        int n = 0;
        for (int g = 0; g < PLAY_H; g++) n += gap_seen[t][g];
        if (n) snprintf(cover + strlen(cover), sizeof cover - strlen(cover), " %s %d/%d", names[t], n, GAP_TOP_RANGE);
    }
    int phases = 0;
    for (int k = 0; k < 8; k++) phases += phase_seen[k];
    printf("  %s caps flush on their columns (%s first): %d runs, %d frames checked, %d obstacle views, gap values:%s, "
           "%d/8 scroll phases\n",
           fails ? "FAIL" : "ok  ", names[first_theme < 0 ? 0 : first_theme], done, checked_frames, checked_obstacles,
           cover, phases);
    if (fails) printf("  %d failed checks, on %d of the %d courses (runs) played\n", fails, fail_courses, done);
    if (first_theme >= 0) {
        int n = 0;
        for (int g = 0; g < PLAY_H; g++) n += gap_seen[first_theme][g];
        if (n < GAP_TOP_RANGE) { printf("  FAIL not every gap value of the first theme was seen\n"); fails++; }
    }
    if (done < runs) { printf("  FAIL only %d runs of %d\n", done, runs); fails++; }
    return fails ? 1 : 0;
}
