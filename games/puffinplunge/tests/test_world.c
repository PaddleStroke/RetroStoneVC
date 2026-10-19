/*
 * Puffin Plunge: the rules alone (src/world.c, no SDK). Every chunk of levels/chunks.txt is well formed and passable;
 * every level of a run assembles; the bot dives through levels 1 and 2 on many seeds (no death allowed on most of
 * them: a gentle first zone) and gets deep on more; the rules are deterministic (the same inputs, the same hash).
 *   test_world <levels/chunks.txt> [seeds] [-v]
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static chunk defs[MAX_CHUNK_DEFS];
static int ndefs, fails, verbose;
static world W;

static void ok(int c, const char *what)
{
    if (c) printf("  ok   %s\n", what);
    else { printf("  FAIL %s\n", what); fails++; }
}

/* plays level `level` with the bot (shops: the bot's choices); returns 1 cleared, 0 dead, -1 timeout */
static int play_level(world *w, int level, int max_frames, uint32_t *hash)
{
    uint32_t bs = 0;
    world_start_level(w, level, defs, ndefs);
    for (int f = 0; f < max_frames; f++) {
        if (w->room) {
            int k = bot_shop(w);
            if (k >= 0) world_buy(w, k);
            world_leave_room(w);
            continue;
        }
        world_step(w, bot_input(w, &bs));
        if (hash) *hash = (*hash ^ world_hash(w)) * 16777619u;
        if (w->h.dead) return 0;
        if (w->won) return 1;
        if (w->cleared) {
            int k;
            while ((k = bot_shop(w)) >= 0) world_buy(w, k);
            if (verbose) printf("    level %d cleared in %d frames: hp %d/%d gems %d kills %d combo %d\n", level + 1, f, w->hp, w->hpmax, w->gems, w->kills, w->best_combo);
            return 1;
        }
    }
    if (verbose) {
        int r0 = w->h.y / SUB / CELL;
        printf("    level %d: timeout at row %d of %d (x %d, ground %d, vy %d)\n", level + 1, r0, w->rows, w->h.x / SUB,
               w->h.ground, w->h.vy);
        for (int r = r0 - 3; r < r0 + 4; r++) {
            if (r < 0 || r >= w->rows) continue;
            printf("      ");
            for (int c = 0; c < COLS; c++) putchar(r == r0 && c == w->h.x / SUB / CELL ? '@' : ".#cCDSF"[w->cell[r][c]]);
            putchar('\n');
        }
    }
    return -1;
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
    char s[96];
    snprintf(s, sizeof s, "%d chunks, each passable from its top", ndefs);
    ok(!bad, s);
    for (int z = 0; z < ZONES; z++) {
        int k = 0;
        for (int i = 0; i < ndefs; i++) k += (defs[i].zones >> z) & 1;
        snprintf(s, sizeof s, "zone %d has %d chunks (8 at least)", z + 1, k);
        ok(k >= 8, s);
    }
    /* every level assembles: within the rows, a floor at the bottom, critters in it */
    int lv_ok = 1;
    for (int l = 0; l < LEVELS; l++) {
        world_new_run(&W, 7);
        world_start_level(&W, l, defs, ndefs);
        if (W.rows > MAX_ROWS || W.nchunks != world_level_rows(l) || W.cell[W.rows - 1][5] != C_FLOOR || W.n_en < 3) lv_ok = 0;
        if (verbose) printf("    level %d: %d chunks, %d rows, %d critters, %d doors\n", l + 1, W.nchunks, W.rows, W.n_en, W.ndoors);
    }
    ok(lv_ok, "the 12 levels assemble (their chunks, a floor, critters)");
    /* the bot: levels 1 and 2 */
    int clean = 0, through = 0, deep = 0, deepest_sum = 0;
    for (int sd = 1; sd <= seeds; sd++) {
        world_new_run(&W, (uint32_t)sd * 7919u);
        int hp0 = W.hp, r1 = play_level(&W, 0, 60 * 120, NULL), r2 = r1 == 1 ? play_level(&W, 1, 60 * 120, NULL) : 0;
        if (r1 == 1 && r2 == 1) through++;
        if (r1 == 1 && W.hp >= hp0 - 1 && r2 == 1) clean++;
        int l = r2 == 1 ? 2 : 0;
        while (r2 == 1 && l < LEVELS && play_level(&W, l, 60 * 150, NULL) == 1) l++;
        if (l >= 6) deep++;
        deepest_sum += l;
        if (verbose) printf("  seed %d: levels %d %d, reached level %d (hp %d/%d, gems %d, weapon %d, passives %d)\n", sd, r1, r2, l + 1, W.hp, W.hpmax, W.gems, W.weapon, W.passives);
    }
    snprintf(s, sizeof s, "the bot dives through levels 1-2 on %d of %d seeds (all needed)", through, seeds);
    ok(through == seeds, s);
    snprintf(s, sizeof s, "... losing at most a heart on %d of them (80%% needed)", clean);
    ok(clean * 10 >= seeds * 8, s);
    printf("  info the bot reached level %.1f on average, level 7+ on %d of %d seeds\n", (double)deepest_sum / seeds + 1, deep, seeds);
    /* determinism */
    uint32_t h1 = 2166136261u, h2 = 2166136261u;
    world_new_run(&W, 4242);
    play_level(&W, 0, 3000, &h1);
    world_new_run(&W, 4242);
    play_level(&W, 0, 3000, &h2);
    ok(h1 == h2, "determinism: the same run twice, the same hash");
    printf(fails ? "test_world: FAILED\n" : "test_world: all passed\n");
    return fails ? 1 : 0;
}
