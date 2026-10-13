/*
 * Mines' rules: the mines laid (their count, never on the first cell nor round it, the first uncovering opening an
 * area), the no-guessing fields over hundreds of seeds and first cells (the reasoner clears them; how many tries and
 * how long the largest takes), the reasoner's soundness (hints played from many positions of classic fields never
 * uncover a mine and never flag a safe cell), uncovering, flood, chords, flags, blasts and undo by replay, and whole
 * games from a seed; the stars' counts (hints, mistakes); every level (no guess from its first area, its opening).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/mines.h"
#include "games/mines_levels.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int bad_level(int i, const char *why)
{
    printf("mines level %d: %s\n", i + 1, why);
    return 0;
}

static int count(const uint8_t *a, int n, int v)
{
    int k = 0;
    for (int i = 0; i < n; i++) k += a[i] == v;
    return k;
}

int main(void)
{
    /* the layings: no guessing and classic, every size, many seeds and first cells */
    for (int v = 0; v < MNS_VARIANTS; v++) {
        int w, h, m, bad = 0, tries = 0, most = 0, n = v % MNS_SIZES == 3 ? 150 : 300;
        mns_size(v, &w, &h, &m);
        double worst = 0, all = 0;
        uint32_t r = pr_seed(1000 + (uint32_t)v);
        for (int i = 0; i < n; i++) {
            uint8_t mine[MNS_CELLS];
            int first = pr_below(&r, w * h);
            clock_t t0 = clock();
            int k = mns_lay(w, h, m, v < MNS_SIZES, (uint32_t)i + 1, first, mine);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            all += ms;
            if (ms > worst) worst = ms;
            tries += k;
            if (k > most) most = k;
            if (count(mine, w * h, 1) != m) bad++;
            for (int c = 0; c < w * h; c++) {
                int dx = c % w - first % w, dy = c / w - first / w;
                if (mine[c] && dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) bad++;
            }
            if (v < MNS_SIZES && !mns_logical(w, h, m, mine, first)) bad++;
        }
        printf("mines %dx%d/%d %s: %.1f tries a field (most %d), %.2f ms a field, worst %.1f ms\n", w, h, m,
               v < MNS_SIZES ? "no guessing" : "classic", tries / (double)n, most, all / n, worst);
        CHECK(bad == 0);
        CHECK(worst < 600);
    }

    /* a field the reasoner cannot clear: two cells, one mine, nothing to tell them apart (a 50/50 corner) */
    {
        uint8_t mine[MNS_CELLS] = {0};
        /* 4 x 3: mines at (3,0); first at (0,1): the number column x=2 sees (3,0),(3,1),(3,2)... */
        int w = 4, h = 2;
        mine[3] = 1;                            /* top right; the cell below it is the other candidate */
        CHECK(!mns_logical(w, h, 1, mine, 0));
        mine[3] = 0;
        mine[7] = 1;
        CHECK(!mns_logical(w, h, 1, mine, 0));
        CHECK(mns_logical(w, h, 0, (uint8_t[MNS_CELLS]){0}, 0));
    }

    /* the reasoner is sound: hints from classic fields, after random safe uncoverings, never touch a mine */
    int sound = 1, played = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        mns_game g;
        mns_new(&g, MNS_SIZES + (int)(seed % MNS_SIZES), seed);
        uint32_t r = pr_seed(seed);
        CHECK(mns_dig(&g, pr_below(&r, g.w * g.h)));
        while (!mns_solved(&g)) {
            int kind, c = mns_hint(&g, &kind);
            if (c < 0 || kind == MNS_UNDO) { sound = 0; break; }
            if (kind == MNS_DIG && g.mine[c]) sound = 0;
            if (kind == MNS_MARK && !g.mine[c]) sound = 0;
            if (mns_play_hint(&g) < 0) { sound = 0; break; }
            played++;
        }
        if (g.blast >= 0) sound = 0;
    }
    CHECK(sound);
    printf("mines: %d hints played on classic fields\n", played);

    /* uncovering, flags, chords, blasts, undo */
    mns_game g;
    mns_new(&g, 0, 7);
    CHECK(g.w == 9 && g.h == 9 && g.mines == 10 && g.logic && !g.placed);
    CHECK(mns_flag(&g, 0) && g.flags == 1 && g.st[0] == MNS_FLAG);
    CHECK(!mns_dig(&g, 0));                    /* a flag protects its cell */
    CHECK(mns_flag(&g, 0) && g.flags == 0);
    CHECK(mns_dig(&g, 40) && g.placed && g.first == 40);
    CHECK(g.adj[40] == 0 && g.opened >= 9 && g.blast < 0);
    for (int c = 0; c < 81; c++) CHECK(!(g.st[c] == MNS_OPEN && g.mine[c]));
    uint8_t layout[MNS_CELLS];
    memcpy(layout, g.mine, sizeof layout);
    int opened = g.opened;
    CHECK(!mns_dig(&g, 40));                   /* an empty cell again: nothing */
    int mine_cell = -1, safe_cell = -1;
    for (int c = 0; c < 81; c++) {
        if (g.st[c] != MNS_HIDDEN) continue;
        if (g.mine[c] && mine_cell < 0) mine_cell = c;
        if (!g.mine[c] && safe_cell < 0) safe_cell = c;
    }
    CHECK(mns_dig(&g, mine_cell) && g.blast == mine_cell && !mns_solved(&g));
    CHECK(!mns_dig(&g, safe_cell) && !mns_flag(&g, safe_cell));   /* a blast: everything waits for undo */
    int kind;
    CHECK(mns_hint(&g, &kind) == mine_cell && kind == MNS_UNDO);
    CHECK(mns_undo(&g) && g.blast < 0 && g.opened == opened && !memcmp(layout, g.mine, sizeof layout));
    /* a chord: a number whose mines are all flagged */
    int chord = -1;
    for (int c = 0; c < 81 && chord < 0; c++) {
        if (g.st[c] != MNS_OPEN || !g.adj[c]) continue;
        int x = c % 9, y = c / 9, hid = 0;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && ny >= 0 && nx < 9 && ny < 9 && g.st[nx + ny * 9] == MNS_HIDDEN && !g.mine[nx + ny * 9])
                    hid++;
            }
        if (hid) chord = c;
    }
    CHECK(chord >= 0);
    CHECK(!mns_dig(&g, chord));                /* its flags not set: no chord */
    {
        int x = chord % 9, y = chord / 9;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, ny = y + dy, c = nx + ny * 9;
                if (nx >= 0 && ny >= 0 && nx < 9 && ny < 9 && g.mine[c] && g.st[c] == MNS_HIDDEN) mns_flag(&g, c);
            }
        CHECK(mns_flags_around(&g, chord) == g.adj[chord]);
        int before = g.opened;
        CHECK(mns_dig(&g, chord) && g.opened > before && g.blast < 0);
    }
    while (mns_undo(&g)) {}
    CHECK(!g.placed && g.opened == 0 && g.flags == 0 && g.moves == 0 && g.na == 0);
    /* the same first cell lays the same mines again */
    CHECK(mns_dig(&g, 40) && !memcmp(layout, g.mine, sizeof layout));
    CHECK(mns_script(&g, "=") && mns_solved(&g));
    CHECK(!mns_script(&g, "d0.1") && !mns_script(&g, "d10.1") && !mns_script(&g, "x"));

    /* whole games from a seed: random tries (a blast taken back by undo, or by the hint), then hints to the end;
     * undo all the way back gives the covered field */
    int whole = 1;
    for (uint32_t seed = 1; seed <= 200; seed++) {
        mns_new(&g, (int)(seed % MNS_VARIANTS), seed * 31);
        uint32_t r = pr_seed(seed);
        int steps = pr_below(&r, 30);
        for (int i = 0; i < steps; i++) {
            int c = pr_below(&r, g.w * g.h);
            if (pr_below(&r, 4) == 0) mns_flag(&g, c);
            else mns_dig(&g, c);
            if (g.blast >= 0) {
                if (pr_below(&r, 2)) mns_undo(&g);
                else mns_play_hint(&g);
            }
        }
        for (int k = 0; k < 2000 && !mns_solved(&g); k++)
            if (mns_play_hint(&g) < 0) break;
        if (!mns_solved(&g)) whole = 0;
        while (mns_undo(&g)) {}
        if (g.placed || g.opened || g.flags || count(g.st, g.w * g.h, MNS_HIDDEN) != g.w * g.h) whole = 0;
    }
    CHECK(whole);

    /* the script: "@" leaves one cell; it is the last */
    mns_new(&g, 1, 5);
    CHECK(mns_script(&g, "d4.6 @") && !mns_solved(&g) && g.opened == g.w * g.h - g.mines - 1);
    CHECK(mns_script(&g, "h") && mns_solved(&g));

    /* the stars' counts: a mine uncovered is a mistake undo does not take back; the hint taking a mine back is not
     * counted again; a wrong flag the hint pulls up is a hint and a mistake */
    mns_new(&g, 0, 7);
    CHECK(mns_dig(&g, 40) && g.hints == 0 && g.mistakes == 0);
    mine_cell = safe_cell = -1;
    for (int c = 0; c < 81; c++) {
        if (g.st[c] != MNS_HIDDEN) continue;
        if (g.mine[c] && mine_cell < 0) mine_cell = c;
        if (!g.mine[c] && safe_cell < 0) safe_cell = c;
    }
    CHECK(mns_dig(&g, mine_cell) && g.mistakes == 1);
    CHECK(mns_undo(&g) && g.mistakes == 1 && g.hints == 0);
    CHECK(mns_dig(&g, mine_cell) && g.mistakes == 2);
    CHECK(mns_play_hint(&g) == mine_cell && g.blast < 0 && g.hints == 0 && g.mistakes == 2);
    CHECK(mns_flag(&g, safe_cell) && mns_play_hint(&g) == safe_cell && g.st[safe_cell] == MNS_HIDDEN);
    CHECK(g.hints == 1 && g.mistakes == 3);
    CHECK(mns_play_hint(&g) >= 0 && g.hints == 2 && g.mistakes == 3);
    CHECK(mns_script(&g, "=") && mns_solved(&g) && g.mistakes == 3);
    {
        mns_game lv;                            /* a level cleared alone: no hint, no mistake */
        CHECK(mns_load(&lv, &mns_levels[0]) && mns_script(&lv, "#") && mns_solved(&lv) && !lv.hints && !lv.mistakes);
    }

    /* every level: it reads back the same, its mines away from its first cell, the reasoner clears it from that first
     * area alone; it opens with that area, undo never covers it, and the hints clear it without a mistake. The sizes
     * never shrink, the first chapter needs one number at a time, no two levels are the same field */
    {
        int tiers[MNS_TIERS] = {0}, last = 0, ok = 1, solo = 1;
        for (int i = 0; i < MNS_LEVELS; i++) {
            const mns_level *L = &mns_levels[i];
            uint8_t mine[MNS_CELLS];
            int n = mns_decode(L, mine), N = L->w * L->h, rounds[MNS_TIERS];
            if (n < 1 || n >= N - 9) {
                ok = bad_level(i, "its mines");
                continue;
            }
            char bits[MNS_CELLS / 6 + 8];
            mns_encode(L->w, L->h, mine, bits);
            if (strcmp(bits, L->bits)) ok = bad_level(i, "read back");
            for (int c = 0; c < N; c++) {
                int dx = c % L->w - L->first % L->w, dy = c / L->w - L->first / L->w;
                if (mine[c] && dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) ok = bad_level(i, "a mine by the start");
            }
            if (!mns_effort(L->w, L->h, n, mine, L->first, rounds)) ok = bad_level(i, "a guess");
            for (int t = 0; t < MNS_TIERS; t++) tiers[t] += rounds[t];
            if (i < MNS_LEVELS / MNS_CHAPTERS && (rounds[MNS_PAIR] || rounds[MNS_COUNT])) solo = 0;
            if (N < last) ok = bad_level(i, "smaller");
            last = N;
            for (int j = 0; j < i; j++)
                if (mns_levels[j].w == L->w && mns_levels[j].h == L->h && mns_levels[j].first == L->first &&
                    !strcmp(mns_levels[j].bits, L->bits))
                    ok = bad_level(i, "twice");
            mns_game lv;
            if (!mns_load(&lv, L) || lv.mines != n || lv.w != L->w || lv.h != L->h || !lv.placed) {
                ok = bad_level(i, "loaded");
                continue;
            }
            int opened = lv.opened;
            if (opened < 4 || lv.st[L->first] != MNS_OPEN || lv.adj[L->first]) ok = bad_level(i, "its opening");
            if (mns_undo(&lv) || lv.opened != opened) ok = bad_level(i, "undone");
            if (!mns_script(&lv, "=") || !mns_solved(&lv) || lv.mistakes) ok = bad_level(i, "the hints");
            while (mns_undo(&lv)) {}
            if (lv.opened != opened || !lv.placed || memcmp(lv.mine, mine, MNS_CELLS)) ok = bad_level(i, "undone all");
        }
        CHECK(ok);
        CHECK(solo);
        printf("mines levels: %d, the reasoner's rounds: %d one number, %d two numbers, %d the mines left\n",
               MNS_LEVELS, tiers[MNS_SINGLE], tiers[MNS_PAIR], tiers[MNS_COUNT]);
    }

    if (fails) { printf("mines: %d failures\n", fails); return 1; }
    printf("mines: all passed\n");
    return 0;
}
