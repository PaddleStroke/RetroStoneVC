/*
 * Puffin Plunge: the rules alone (src/world.c, no SDK). Every chunk of levels/chunks.txt is well formed and passable; the
 * endless well, made far down (400 chunks, 4800 m: every zone four times, the gates, the rest stops, the walrus's caves),
 * is passable from every column of every chunk and across their joins; a CONTINUE makes the same well; the bot dives
 * past the gentle first metres on every seed and deep on most; the rules are deterministic.
 *   test_world <levels/chunks.txt> [seeds] [-v]
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static chunk defs[MAX_CHUNK_DEFS];
static int ndefs, fails, verbose;
static world W, W2;

static void ok(int c, const char *what)
{
    if (c) printf("  ok   %s\n", what);
    else { printf("  FAIL %s\n", what); fails++; }
}

/* the bot dives (shops: its choices) until it is down or `frames` pass; returns the depth reached */
static int dive(world *w, uint32_t seed, int frames, int *hp_at, int at_m, uint32_t *hash)
{
    uint32_t bs = 0;
    world_new_run(w, seed);
    world_begin(w, 0, defs, ndefs);
    if (hp_at) *hp_at = -1;
    for (int f = 0; f < frames; f++) {
        if (w->room || w->cleared) {
            int k;
            while ((k = bot_shop(w)) >= 0) world_buy(w, k);
            if (w->room) world_leave_room(w);
            else world_leave_rest(w);
            continue;
        }
        world_step(w, bot_input(w, &bs), defs, ndefs);
        if (hash) *hash = (*hash ^ world_hash(w)) * 16777619u;
        if (hp_at && *hp_at < 0 && w->depth_m >= at_m) *hp_at = w->hp;
        if (w->h.dead) break;
    }
    return w->depth_m;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: test_world chunks.txt [seeds] [-v]\n"); return 2; }
    int seeds = argc > 2 && argv[2][0] != '-' ? atoi(argv[2]) : 30;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "-v")) verbose = 1;
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    static char text[1 << 16];
    size_t n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[n] = 0;
    char err[200];
    ndefs = chunks_parse(text, defs, MAX_CHUNK_DEFS, err, sizeof err);
    if (ndefs < 0) { printf("  FAIL chunks: %s\n", err); return 1; }
    int bad = 0;
    for (int i = 0; i < ndefs; i++)
        if (!chunk_check(&defs[i], err, sizeof err)) { printf("  FAIL %s\n", err); bad++; }
    char s[160];
    snprintf(s, sizeof s, "%d chunks, each passable from its top", ndefs);
    ok(!bad, s);
    for (int z = 0; z < ZONES; z++) {
        int k = 0;
        for (int i = 0; i < ndefs; i++) k += (defs[i].zones >> z) & 1;
        snprintf(s, sizeof s, "zone %d has %d chunks (8 at least)", z + 1, k);
        ok(k >= 8, s);
    }

    /* the endless well, far down, on a few seeds: every chunk and every join passable */
    int reach_bad = 0, kinds[5] = {0};
    for (int sd = 1; sd <= 3 && reach_bad < 5; sd++) {
        world_new_run(&W, (uint32_t)sd * 977u);
        world_begin(&W, 0, defs, ndefs);
        for (int k = 0; k < 400 && reach_bad < 5; k++) {
            int row0 = TOP_ROWS + CHUNK_H * k;
            W.h.y = row0 * CELL * SUB;               /* (the puffin follows: the ring keeps the rows round it) */
            world_gen_to(&W, row0 + CHUNK_H + 1, defs, ndefs);
            if (sd == 1) kinds[world_chunk_kind(k)]++;
            if (!world_reach(&W, row0, CHUNK_H, 0, err, sizeof err) ||
                (k > 0 && !world_reach(&W, row0 - CHUNK_H / 2, CHUNK_H, 0, err, sizeof err))) {
                printf("  FAIL seed %d: %s (kind %d)\n", sd, err, world_chunk_kind(k));
                reach_bad++;
            }
        }
    }
    snprintf(s, sizeof s, "the well down to 4800 m (3 seeds): every chunk and every join passable (%d gates, %d rest "
             "stops, %d walrus caves)", kinds[CK_GATE], kinds[CK_REST], kinds[CK_ARENA2]);
    ok(!reach_bad && kinds[CK_GATE] >= 15 && kinds[CK_REST] >= 20 && kinds[CK_ARENA2] >= 4, s);

    /* a CONTINUE makes the same well: chunk 40 from the top and from a dive begun at chunk 38 */
    world_new_run(&W, 4242);
    world_begin(&W, 0, defs, ndefs);
    W.h.y = (TOP_ROWS + CHUNK_H * 38) * CELL * SUB;
    world_gen_to(&W, TOP_ROWS + CHUNK_H * 41, defs, ndefs);
    world_new_run(&W2, 4242);
    world_begin(&W2, 38, defs, ndefs);
    world_gen_to(&W2, TOP_ROWS + CHUNK_H * 41, defs, ndefs);
    int same = 1;
    for (int r = TOP_ROWS + CHUNK_H * 40; r < TOP_ROWS + CHUNK_H * 41; r++)
        for (int c = 0; c < COLS; c++) if (world_cell(&W, c, r) != world_cell(&W2, c, r)) same = 0;
    ok(same, "a CONTINUE makes the same well (chunk 40 from the top and from chunk 38)");

    /* the bot */
    int past = 0, clean = 0, deep = 0, sum = 0, minm = 1 << 30, maxm = 0;
    for (int sd = 1; sd <= seeds; sd++) {
        int hp150;
        int m = dive(&W, (uint32_t)sd * 7919u, 60 * 60 * 4, &hp150, 150, NULL);
        sum += m;
        if (m < minm) minm = m;
        if (m > maxm) maxm = m;
        if (m >= 150) past++;
        if (hp150 >= START_HP - 1) clean++;
        if (m >= 600) deep++;
        if (verbose)
            printf("    seed %d: %d m (hp %d at 150 m, %d at the end; gems %d, weapon %d, passives %d, walruses %d; a walrus: "
                   "hp %d, st %d)\n", sd, m, hp150, W.hp, W.gems, W.weapon, W.passives, W.walruses,
                   W.boss >= 0 ? W.en[W.boss].hp : -1, W.boss >= 0 ? W.en[W.boss].st : -1);
    }
    snprintf(s, sizeof s, "the bot dives past 150 m on %d of %d seeds (all needed)", past, seeds);
    ok(past == seeds, s);
    snprintf(s, sizeof s, "... losing at most a heart in the first 150 m on %d of them (80%% needed)", clean);
    ok(clean * 10 >= seeds * 8, s);
    printf("  info the bot dives %d m on average (%d .. %d), 600 m or more on %d of %d seeds\n", sum / seeds, minm, maxm,
           deep, seeds);

    /* determinism */
    uint32_t h1 = 2166136261u, h2 = 2166136261u;
    dive(&W, 4242, 4000, NULL, 0, &h1);
    dive(&W, 4242, 4000, NULL, 0, &h2);
    ok(h1 == h2, "determinism: the same dive twice, the same hash");
    printf(fails ? "test_world: FAILED\n" : "test_world: all passed\n");
    return fails ? 1 : 0;
}
