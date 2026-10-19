/*
 * Goat Gorge: the room solver. A breadth-first search over the kid's inputs, run through the game's own rules
 * (src/world.c): each step holds one of 11 inputs for a few frames (nothing, Left, Right, each with or without Jump,
 * and a dash Right, Left, Up, Up-Right or Up-Left: only the inputs the phone's touch buttons have, so every room is
 * playable by touch too). States already seen are dropped, and each layer keeps a few states per 8 x 8 px place
 * (with the dash, the berry, the ground), so the search spreads over the whole room. It proves every room can be
 * finished (and its berry taken) and records the inputs: levels/solutions.txt, replayed by tests/test_rooms.c and
 * played by the game's bot (--opt bot=1).
 *
 *   solver <levels/chN.txt> <chapter> [--room N] [--step K] [--keep Q] [--frames F]     -> solution lines on stdout
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NACT 11
static const uint8_t ACT[NACT] = {
    0, IN_LEFT, IN_RIGHT, IN_JUMP, IN_LEFT | IN_JUMP, IN_RIGHT | IN_JUMP,
    IN_DASH | IN_RIGHT, IN_DASH | IN_LEFT, IN_DASH | IN_UP, IN_DASH | IN_UP | IN_RIGHT, IN_DASH | IN_UP | IN_LEFT};

#define BUCKETS 4194301u                /* (a prime: the places hashed into it) */
typedef struct node { int32_t parent; uint8_t act; } node;
static node *nodes;
static int nnodes, maxnodes = 8 << 20;
static uint64_t *seen;
static const uint64_t SEEN_N = 1u << 24;

static uint64_t mix(uint64_t h, uint64_t v) { h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); return h; }

static int lcm_period(const room *r)
{
    int p = 1;
    for (int i = 0; i < r->nplats; i++) {
        int q = 2 * (r->plat[i].dist + PLAT_PAUSE), a = p, b = q;
        while (b) { int t = a % b; a = b; b = t; }
        p = p / a * q;
        if (p > 100000) p = 100000;
    }
    if (r->wind_off > 0) {
        int q = r->wind_on + r->wind_off, a = p, b = q;
        while (b) { int t = a % b; a = b; b = t; }
        p = p / a * q;
    }
    return p;
}

static uint64_t key_of(const world *w, int period)
{
    const hero *h = &w->h;
    uint64_t k = 1469598103934665603ull;
    k = mix(k, (uint64_t)(h->x + 1000) | (uint64_t)(h->y + 1000) << 16 | (uint64_t)((h->rx + 256) >> 6) << 32 |
                   (uint64_t)((h->ry + 256) >> 6) << 40);
    k = mix(k, (uint64_t)((h->vx + 8192) >> 5) | (uint64_t)((h->vy + 8192) >> 5) << 16 |
                   (uint64_t)(h->grounded | h->dash_avail << 1 | (h->coyote_t > 0) << 2 | h->can_cut << 3 |
                              h->auto_var << 4 | ((h->held & IN_JUMP) != 0) << 5 | ((h->held & IN_DASH) != 0) << 6 |
                              (h->dash_cool > 0) << 7) << 32 |
                   (uint64_t)(h->dash_t | h->freeze_t << 4) << 40 | (uint64_t)(h->var_t | h->wall_lock << 4) << 48 |
                   (uint64_t)h->buffer_t << 56);
    for (int i = 0; i < MAX_CRUMBLE; i += 4)
        k = mix(k, (uint64_t)(uint16_t)w->crumble[i] | (uint64_t)(uint16_t)w->crumble[i + 1] << 16 |
                       (uint64_t)(uint16_t)w->crumble[i + 2] << 32 | (uint64_t)(uint16_t)w->crumble[i + 3] << 48);
    for (int i = 0; i < MAX_GEMS; i++) k = mix(k, (uint64_t)w->gem_t[i]);
    k = mix(k, (uint64_t)w->berry_got | (uint64_t)(period > 1 ? w->t % period : 0) << 8);
    return k ? k : 1;
}

static int seen_add(uint64_t k)
{
    uint64_t i = k & (SEEN_N - 1);
    while (seen[i]) {
        if (seen[i] == k) return 0;
        i = (i + 1) & (SEEN_N - 1);
    }
    seen[i] = k;
    return 1;
}

static int solve(const room *r, int want_berry, int step, int keep, int max_frames, uint8_t *out, int *nout)
{
    int period = lcm_period(r);
    int cap = 200000;
    world *cur = malloc(sizeof(world) * (size_t)cap), *nxt = malloc(sizeof(world) * (size_t)cap);
    int *cur_id = malloc(sizeof(int) * (size_t)cap), *nxt_id = malloc(sizeof(int) * (size_t)cap);
    static uint8_t bucket[BUCKETS];
    memset(seen, 0, sizeof(uint64_t) * SEEN_N);
    nnodes = 0;
    int ncur = 1;
    world_spawn(&cur[0], r, 0);
    nodes[nnodes++] = (node){-1, 0};
    cur_id[0] = 0;
    seen_add(key_of(&cur[0], period));
    int goal = -1, goal_frames = 0;
    for (int layer = 0; layer * step < max_frames && ncur && goal < 0; layer++) {
        memset(bucket, 0, sizeof bucket);
        int nn = 0;
        for (int s = 0; s < ncur && goal < 0; s++) {
            for (int a = 0; a < NACT && goal < 0; a++) {
                world w = cur[s];
                int dead = 0, done = 0, f;
                for (f = 0; f < step; f++) {
                    world_step(&w, r, ACT[a]);
                    if (w.h.dead_t) { dead = 1; break; }
                    if (w.exited) { done = 1; f++; break; }
                }
                if (dead) continue;
                if (done && (!want_berry || w.berry_got)) {
                    if (nnodes >= maxnodes) break;
                    nodes[nnodes] = (node){cur_id[s], (uint8_t)a};
                    goal = nnodes++;
                    goal_frames = layer * step + f;
                    break;
                }
                if (done) continue;
                uint64_t k = key_of(&w, period);
                if (!seen_add(k)) continue;
                const hero *h = &w.h;
                int bx = (h->x + 16) / 8, by = (h->y + 16) / 8;
                if (bx < 0) bx = 0;
                if (bx > 47) bx = 47;
                if (by < 0) by = 0;
                if (by > 35) by = 35;
                int ph = period > 1 ? (int)((w.t % period) * 8 / period) : 0;
                int vxc = (h->vx + 1280) / 320, vyc = h->vy < -256 ? 0 : h->vy > 256 ? 2 : 1;
                if (vxc < 0) vxc = 0;
                if (vxc > 7) vxc = 7;
                uint32_t b = (uint32_t)(((((by * 48 + bx) * 8 + (h->dash_avail | w.berry_got << 1 | h->grounded << 2)) * 8 + ph) * 8 + vxc) * 3 + vyc);
                b = (b * 2654435761u) % BUCKETS;
                if (bucket[b] >= keep) continue;
                bucket[b]++;
                if (nn >= cap || nnodes >= maxnodes) continue;
                nodes[nnodes] = (node){cur_id[s], (uint8_t)a};
                nxt[nn] = w;
                nxt_id[nn] = nnodes++;
                nn++;
            }
        }
        world *t = cur; cur = nxt; nxt = t;
        int *ti = cur_id; cur_id = nxt_id; nxt_id = ti;
        ncur = nn;
    }
    free(cur); free(nxt); free(cur_id); free(nxt_id);
    if (goal < 0) return -1;
    /* the actions, from the goal back to the start, then the frames */
    int acts[8192], na = 0;
    for (int i = goal; nodes[i].parent >= 0 && na < 8192; i = nodes[i].parent) acts[na++] = nodes[i].act;
    int n = 0;
    for (int i = na - 1; i >= 0; i--)
        for (int f = 0; f < step && n < goal_frames; f++) out[n++] = ACT[acts[i]];
    *nout = n;
    return goal_frames;
}

/* replays the frames from the start: 1 when the kid leaves the room (with the berry if asked) without a death */
static int verify(const room *r, const uint8_t *in, int n, int want_berry)
{
    world w;
    world_spawn(&w, r, 0);
    for (int i = 0; i < n; i++) {
        world_step(&w, r, in[i]);
        if (w.h.dead_t) return 0;
        if (w.exited) return !want_berry || w.berry_got;
    }
    return 0;
}

/* the room at 8 px per character with the kid's path over it (every 4th frame: '.', a dash 'D', the berry 'b') */
static void trace(const room *r, const uint8_t *in, int n)
{
    static const char CH[C_COUNT] = {' ', '#', '-', '=', '^', 'v', '<', '>'};
    char g[30][41];
    for (int y = 0; y < 30; y++) {
        for (int x = 0; x < 40; x++) g[y][x] = CH[r->cell[y / 2][x / 2]];
        g[y][40] = 0;
    }
    for (int i = 0; i < r->nsprings; i++) g[r->spring_y[i] / 8 + 1][r->spring_x[i] / 8] = 'o';
    for (int i = 0; i < r->ngems; i++) g[r->gem_y[i] / 8][r->gem_x[i] / 8] = '*';
    if (r->has_berry) g[r->berry_y / 8][r->berry_x / 8] = 'B';
    world w;
    world_spawn(&w, r, 0);
    for (int i = 0; i < n; i++) {
        world_step(&w, r, in[i]);
        int cx = (w.h.x + HB_W / 2) / 8, cy = (w.h.y + HB_H / 2) / 8;
        if (i % 4 == 0 && cx >= 0 && cx < 40 && cy >= 0 && cy < 30)
            g[cy][cx] = w.h.dash_t || w.h.freeze_t ? 'D' : w.berry_got ? 'b' : '.';
    }
    for (int y = 0; y < 30; y++) fprintf(stderr, "    |%s|\n", g[y]);
}

static void encode(const uint8_t *in, int n, char *out)
{
    int k = 0;
    for (int i = 0; i < n;) {
        int j = i;
        while (j < n && in[j] == in[i]) j++;
        k += sprintf(out + k, j - i > 1 ? "%c%d" : "%c", sol_abc[in[i] & 63], j - i);
        i = j;
    }
    out[k] = 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: solver <levels/chN.txt> <chapter> [--room N] [--step K] [--keep Q] [--frames F]\n"); return 2; }
    int only = 0, step = 4, keep = 1, frames = 1800, show = 0;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--trace")) show = 1;
        else if (!strcmp(argv[i], "--room") && i + 1 < argc) only = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--step") && i + 1 < argc) step = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--keep") && i + 1 < argc) keep = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
    }
    int chapter = atoi(argv[2]);
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    static char text[1 << 20];
    size_t len = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[len] = 0;
    static room rooms[MAX_ROOMS];
    char err[200];
    int n = chapter_parse(text, rooms, MAX_ROOMS, err, sizeof err);
    if (n < 0) { fprintf(stderr, "%s: %s\n", argv[1], err); return 1; }
    nodes = malloc(sizeof(node) * (size_t)maxnodes);
    seen = malloc(sizeof(uint64_t) * SEEN_N);
    static uint8_t in[20000];
    static char rle[80000];
    int fail = 0;
    for (int i = 0; i < n; i++) {
        if (only && i + 1 != only) continue;
        for (int berry = 0; berry <= rooms[i].has_berry; berry++) {
            int k, got = -1, len2 = 0;
            /* the coarse steps first (fast), finer ones when the room needs a sharper timing */
            int steps[3] = {step, step > 2 ? step / 2 : 2, 1};
            for (k = 0; k < 3 && got < 0; k++) {
                got = solve(&rooms[i], berry, steps[k], keep, frames, in, &len2);
                if (got >= 0 && !verify(&rooms[i], in, len2, berry)) {
                    fprintf(stderr, "room %d-%d: the solution does not replay\n", chapter, i + 1);
                    got = -1;
                }
            }
            if (got < 0) {
                fprintf(stderr, "room %d-%d%s: NO SOLUTION found\n", chapter, i + 1, berry ? " (berry)" : "");
                fail = 1;
                continue;
            }
            encode(in, len2, rle);
            printf("%d %d %s %d %s\n", chapter, i + 1, berry ? "berry" : "exit", len2, rle);
            fflush(stdout);
            fprintf(stderr, "room %d-%d%s: %d frames (%.1f s), %d search nodes\n", chapter, i + 1, berry ? " (berry)" : "",
                    len2, len2 / 60.0, nnodes);
            if (show) trace(&rooms[i], in, len2);
        }
    }
    return fail;
}
