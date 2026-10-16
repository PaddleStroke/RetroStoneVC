/*
 * Tile Connect's rules: the line (straight, one turn, two turns, round the outside, never three, blocked cells), the
 * falls (down, left, to the middle, held by blocks), the search against a full search on small boards, boards made
 * from many seeds of every size and fall (each cleared by its own plan; fast enough on 12 x 8), every designed level
 * (read, cleared by the plan it carries), the hint and the shuffle from positions played at random (the shuffle always
 * leaves a board that clears), undo, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/tileconnect.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a board from rows: a letter a face ('a' face 0...), '.' empty, '#' blocked */
static tcn_board board(int fall, const char *rows)
{
    tcn_board b;
    memset(&b, 0, sizeof b);
    memset(b.face, TCN_EMPTY, sizeof b.face);
    memset(b.id, TCN_EMPTY, sizeof b.id);
    b.fall = fall;
    int x = 0, y = 0, w = 0;
    for (const char *p = rows; *p; p++) {
        if (*p == '|') { w = x; x = 0; y++; continue; }
        int c = x + y * TCN_MAXW;               /* laid out at the widest first, packed below */
        b.face[c] = *p == '.' ? TCN_EMPTY : *p == '#' ? TCN_BLOCK : (uint8_t)(*p - 'a');
        x++;
    }
    if (!w) w = x;
    b.w = w;
    b.h = y + 1;
    uint8_t f[TCN_CELLS];
    memset(f, TCN_EMPTY, sizeof f);
    for (int r = 0; r < b.h; r++)
        for (int c = 0; c < w; c++) f[c + r * w] = b.face[c + r * TCN_MAXW];
    memcpy(b.face, f, sizeof f);
    for (int c = 0; c < w * b.h; c++) b.id[c] = b.face[c] < TCN_FACES ? (uint8_t)c : TCN_EMPTY;
    return b;
}

static int at(const tcn_board *b, int x, int y) { return x + y * b->w; }

static int tiles(const tcn_board *b)
{
    int n = 0;
    for (int c = 0; c < b->w * b->h; c++) n += b->face[c] < TCN_FACES;
    return n;
}

/* the full search: every pair tried, nothing kept */
static int brute(const tcn_board *b)
{
    if (!tiles(b)) return 1;
    int n = b->w * b->h;
    for (int a = 0; a < n; a++)
        for (int c = a + 1; c < n; c++)
            if (tcn_can_take(b, a, c)) {
                tcn_board t = *b;
                t.face[a] = t.face[c] = TCN_EMPTY;
                t.id[a] = t.id[c] = TCN_EMPTY;
                tcn_fall(&t, NULL);
                if (brute(&t)) return 1;
            }
    return 0;
}

/* the plan the game carries, played: each step's two tiles taken */
static int plan_clears(tcn_game g)
{
    for (int k = g.plan; g.tiles; k++) {
        int p[2], m = 0;
        for (int c = 0; c < g.b.w * g.b.h && m < 2; c++)
            if (g.b.face[c] < TCN_FACES && g.step[g.b.id[c]] == k) p[m++] = c;
        if (m < 2 || !tcn_take(&g, p[0], p[1], NULL)) return 0;
    }
    return tcn_won(&g);
}

static tcn_game G, H;

int main(void)
{
    /* ---- the line ---- */
    {
        tcn_board b = board(TCN_STILL, "a...|cdea|fghi");
        tcn_pt p[4];
        int n;
        CHECK(tcn_connect(&b, at(&b, 1, 1), at(&b, 2, 1), p, &n) && n == 2);   /* side by side */
        CHECK(tcn_connect(&b, at(&b, 0, 0), at(&b, 3, 1), p, &n) && n == 3);   /* along the top, then down */
        CHECK(p[1].x == 3 && p[1].y == 0);
        CHECK(tcn_can_take(&b, at(&b, 0, 0), at(&b, 3, 1)));
        CHECK(!tcn_can_take(&b, at(&b, 1, 1), at(&b, 2, 1)));                 /* not the same face */
        CHECK(tcn_connect(&b, at(&b, 0, 2), at(&b, 3, 2), p, &n) && n == 4);   /* round the bottom outside */
        CHECK(p[1].y == 3 && p[2].y == 3);
        CHECK(tcn_connect(&b, at(&b, 1, 1), at(&b, 2, 0), p, &n) && n == 3);   /* up into the gap, along */
        tcn_board s = board(TCN_STILL, "a.b|cde|fga");
        CHECK(tcn_connect(&s, at(&s, 1, 1), at(&s, 1, 2), NULL, NULL));
        CHECK(!tcn_connect(&s, at(&s, 0, 0), at(&s, 2, 2), NULL, NULL));      /* the corners: three turns */
        /* three turns needed: a tile walled in on three sides and its twin far behind a wall */
        tcn_board t = board(TCN_STILL, "bbbbb|b.b.b|bab.b|bbbab|bbbbb");
        CHECK(!tcn_connect(&t, at(&t, 1, 2), at(&t, 3, 3), NULL, NULL));
        tcn_board u = board(TCN_STILL, "bbbbb|b...b|bab.b|bbbab|bbbbb");
        CHECK(tcn_connect(&u, at(&u, 1, 2), at(&u, 3, 3), p, &n) && n == 4);   /* up, across, down */
        tcn_board v = board(TCN_STILL, "bbbbb|b.#.b|bab.b|bbbab|bbbbb");
        CHECK(!tcn_connect(&v, at(&v, 1, 2), at(&v, 3, 3), NULL, NULL));       /* a block in the way */
        tcn_board e = board(TCN_STILL, "a..a");
        CHECK(tcn_connect(&e, 0, 3, p, &n) && n == 2);                         /* straight along the row */
    }
    /* ---- the falls ---- */
    {
        tcn_board b = board(TCN_DOWN, "ab|.c|d.|#e|.f");
        uint8_t from[TCN_CELLS];
        tcn_fall(&b, from);
        CHECK(b.face[at(&b, 0, 0)] == TCN_EMPTY && b.face[at(&b, 0, 1)] == 0 && b.face[at(&b, 0, 2)] == 3);
        CHECK(b.face[at(&b, 0, 3)] == TCN_BLOCK && b.face[at(&b, 0, 4)] == TCN_EMPTY);   /* held by the block */
        CHECK(b.face[at(&b, 1, 0)] == TCN_EMPTY && b.face[at(&b, 1, 4)] == 5 && b.face[at(&b, 1, 1)] == 1);
        CHECK(from[at(&b, 0, 1)] == at(&b, 0, 0) && from[at(&b, 1, 1)] == at(&b, 1, 0));
        CHECK(b.id[at(&b, 0, 1)] == at(&b, 0, 0));
        tcn_board l = board(TCN_LEFT, ".a.b#.c");
        tcn_fall(&l, NULL);
        CHECK(l.face[0] == 0 && l.face[1] == 1 && l.face[2] == TCN_EMPTY && l.face[4] == TCN_BLOCK && l.face[5] == 2);
        tcn_board m = board(TCN_CENTRE, "a..b|.ab.|ab.c");
        tcn_fall(&m, NULL);
        CHECK(m.face[1] == 0 && m.face[2] == 1 && m.face[0] == TCN_EMPTY && m.face[3] == TCN_EMPTY);
        CHECK(m.face[4 + 1] == 0 && m.face[4 + 2] == 1);
        CHECK(m.face[8 + 0] == 0 && m.face[8 + 1] == 1 && m.face[8 + 2] == 2 && m.face[8 + 3] == TCN_EMPTY);
    }
    /* ---- the search against the full search: small random boards (often not clearable), every fall ---- */
    {
        uint32_t s = pr_seed(99);
        int agree = 1, yes = 0, no = 0;
        for (int t = 0; t < 600; t++) {
            int w = 3 + pr_below(&s, 2), h = 3, fall = t % TCN_FALLS;
            char rows[32];
            int k = 0, cells = w * h, faces[12], n = 0, used[4] = {0};
            int holes = cells % 2 ? 1 : 2 * pr_below(&s, 2);
            for (int i = 0; i < (cells - holes) / 2; i++) {   /* four faces, two pairs of each at most */
                int f = pr_below(&s, 4);
                while (used[f] == 2) f = (f + 1) % 4;
                used[f]++;
                faces[n] = faces[n + 1] = f;
                n += 2;
            }
            int order[12];
            for (int i = 0; i < cells; i++) order[i] = i;
            pr_shuffle(&s, order, cells);
            char cell[12];
            for (int i = 0; i < cells; i++) cell[order[i]] = i < n ? (char)('a' + faces[i]) : (i == n && pr_below(&s, 2) ? '#' : '.');
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) rows[k++] = cell[x + y * w];
                rows[k++] = y < h - 1 ? '|' : 0;
            }
            tcn_board b = board(fall, rows);
            tcn_fall(&b, NULL);
            int r = tcn_solve(&b, 1000000, NULL, NULL, NULL), f = brute(&b);
            if (r != f) { agree = 0; printf("  disagree on %s (fall %d): search %d, full %d\n", rows, fall, r, f); }
            yes += f;
            no += !f;
        }
        CHECK(agree && yes > 50 && no > 10);
        printf("ok   search = full search on 600 small boards (%d clear, %d do not)\n", yes, no);
    }
    /* ---- boards made: every size, every fall, blocks ---- */
    {
        int good = 1;
        double most = 0;
        for (int seed = 1; seed <= 300; seed++) {
            int v = seed % TCN_VARIANTS, w, h;
            tcn_size(v, &w, &h);
            uint8_t blk[TCN_CELLS] = {0};
            if (seed % 3 == 0) blk[w + 1] = blk[w * h - w - 2] = 1;
            clock_t t0 = clock();
            int ok = tcn_make(&G, w, h, seed % TCN_FALLS, blk, (w * h / 2) / 2, (uint32_t)seed);
            double dt = (double)(clock() - t0) / CLOCKS_PER_SEC;
            if (dt > most) most = dt;
            if (!ok || !plan_clears(G) || G.tiles != w * h - (seed % 3 == 0 ? 2 : 0)) good = 0;
            int cnt[TCN_FACES] = {0};
            for (int c = 0; c < w * h; c++)
                if (G.b.face[c] < TCN_FACES) cnt[G.b.face[c]]++;
            for (int f = 0; f < TCN_FACES; f++) good &= cnt[f] % 2 == 0 && cnt[f] <= 6;
        }
        CHECK(good && most < 0.1);
        clock_t t0 = clock();
        for (int seed = 0; seed < 50; seed++) tcn_new(&G, 3, (uint32_t)seed);
        double each = (double)(clock() - t0) / CLOCKS_PER_SEC / 50;
        CHECK(each < 0.05);
        printf("ok   300 boards made and cleared by their plans (%.1f ms at most; 12 x 8: %.1f ms each)\n", 1000 * most,
               1000 * each);
    }
    /* ---- every designed level ---- */
    {
        int good = 0, last = 0, still = 0, falls = 0, blocks = 0;
        for (int l = 0; l < TCN_LEVELS; l++) {
            int ok = tcn_level(&G, l) && plan_clears(G);
            const tcn_lvl *lv = &tcn_levels[l];
            for (int k = 0; k < l && ok; k++)
                if (!strcmp(tcn_levels[k].faces, lv->faces)) ok = 0;
            int cnt[TCN_FACES] = {0};
            for (int c = 0; c < G.b.w * G.b.h; c++)
                if (G.b.face[c] < TCN_FACES) cnt[G.b.face[c]]++;
                else if (G.b.face[c] == TCN_BLOCK) blocks++;
            for (int f = 0; f < TCN_FACES; f++) ok &= cnt[f] % 2 == 0 && cnt[f] <= 6;
            int size = lv->w * lv->h;
            if (l % (TCN_LEVELS / TCN_CHAPTERS) == 0) { ok &= size >= last || l >= 2 * TCN_LEVELS / TCN_CHAPTERS; last = size; }
            if (!ok) printf("FAIL level %d\n", l + 1);
            good += ok;
            still += lv->fall == TCN_STILL;
            falls += lv->fall != TCN_STILL;
        }
        CHECK(good == TCN_LEVELS && still >= 40 && falls >= 75 && blocks > 0);
        CHECK(!tcn_level(&G, TCN_LEVELS) && !tcn_level(&G, -1));
        CHECK(!tcn_load(&G, 2, 1, 0, "ab", "00") && !tcn_load(&G, 2, 1, 0, "aa", "01") && tcn_load(&G, 2, 1, 0, "aa", "00"));
        printf("ok   %d levels read and cleared by their plans (%d still, %d falling)\n", good, still, falls);
    }
    /* ---- the hint and the shuffle from positions played at random; undo; the notation ---- */
    {
        int hint_ok = 1, shuffle_ok = 1, undo_ok = 1, stuck = 0, shuffles = 0;
        double slow = 0;
        uint32_t s = pr_seed(7);
        for (int t = 0; t < 120; t++) {
            if (t % 2) tcn_level(&G, (t * 37) % TCN_LEVELS);
            else tcn_new(&G, t / 2 % TCN_VARIANTS, (uint32_t)t);
            for (int moves = 0; G.tiles && moves < 80; moves++) {
                if (moves % 9 == 4) {           /* a hint: a pair that can be taken, or the shuffle when stuck */
                    int a, b;
                    clock_t t0 = clock();
                    int k = tcn_hint(&G, &a, &b);
                    double dt = (double)(clock() - t0) / CLOCKS_PER_SEC;
                    if (dt > slow) slow = dt;
                    if (k == TCN_HINT_PAIR) hint_ok &= tcn_can_take(&G.b, a, b);
                    else hint_ok &= k == TCN_HINT_SHUFFLE;
                }
                int pa[512][2], n = 0;
                for (int a = 0; a < G.b.w * G.b.h; a++)
                    for (int b = a + 1; b < G.b.w * G.b.h && n < 512; b++)
                        if (tcn_can_take(&G.b, a, b)) pa[n][0] = a, pa[n++][1] = b;
                CHECK(n == tcn_count_pairs(&G.b));
                if (!n) {                       /* stuck: the shuffle, and its board clears by its plan */
                    stuck++;
                    if (!tcn_shuffle(&G)) { shuffle_ok = 0; break; }
                    shuffles++;
                    shuffle_ok &= plan_clears(G) && G.plan == 0;
                    continue;
                }
                H = G;
                int k = pr_below(&s, n);
                tcn_take(&G, pa[k][0], pa[k][1], NULL);
                if (moves % 7 == 3) {           /* undo: the same board back */
                    tcn_undo(&G);
                    undo_ok &= !memcmp(&G.b, &H.b, sizeof G.b) && G.tiles == H.tiles && G.plan == H.plan;
                    tcn_take(&G, pa[k][0], pa[k][1], NULL);
                }
            }
        }
        CHECK(hint_ok); CHECK(shuffle_ok); CHECK(undo_ok); CHECK(stuck > 5 && slow < 1.0);
        printf("ok   hints and shuffles in 120 games played at random (%d times stuck; slowest hint %.0f ms)\n", stuck,
               1000 * slow);
        tcn_level(&G, 0);
        CHECK(tcn_script(&G, "=3") && G.tiles == G.pairs * 2 - 6 && G.plan == 3);
        CHECK(tcn_script(&G, "h") && G.hints == 1 && G.plan == 4);
        CHECK(tcn_script(&G, "s") && G.shuffles == 1 && G.plan == 0);
        CHECK(tcn_undo(&G) && G.plan == 4 && G.shuffles == 1);
        CHECK(tcn_script(&G, "=*") && tcn_won(&G));
        CHECK(!tcn_script(&G, "zz-a1") && !tcn_script(&G, "a1"));
        CHECK(tcn_cell("c2", 6, 4, NULL) == 8 && tcn_cell("g1", 6, 4, NULL) < 0 && tcn_cell("a5", 6, 4, NULL) < 0);
        tcn_level(&G, 0);
        int a, b;
        CHECK(tcn_find_pair(&G.b, &a, &b));
        char mv[32];
        snprintf(mv, sizeof mv, "%c%d-%c%d", 'a' + a % G.b.w, a / G.b.w + 1, 'a' + b % G.b.w, b / G.b.w + 1);
        CHECK(tcn_script(&G, mv) && G.tiles == 2 * G.pairs - 2);
        tcn_new(&G, 0, 5);
        CHECK(tcn_script(&G, "=*") && tcn_won(&G));
    }
    if (fails) { printf("tileconnect: %d failures\n", fails); return 1; }
    printf("tileconnect: all passed\n");
    return 0;
}
