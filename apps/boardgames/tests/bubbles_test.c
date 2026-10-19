/*
 * Bubble Pop's rules: the geometry (a straight shot, a bank shot, mirrored aims on a mirrored box), the landing (three
 * pop, two stay, what hangs on nothing falls, stones and gems never pop, a cage breaks first, a bomb, a rainbow), the
 * ceiling coming down and the line, the shots running out; levels as text both ways; every level won by its own
 * solution in its par, no wider search finding fewer than the par, the obvious player losing every level past each
 * chapter's opening ones; undo, the hint, the notation; the random puzzles (solved, the same from the same seed, the
 * time they take).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/bubbles.h"
#include "games/bubbles_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static bub_level lv(const char *t)
{
    bub_level L;
    int ok = bub_parse(&L, t);
    CHECK(ok);
    return L;
}

/* the aim from the launcher at the centre of cell (r, c) */
static int aim_to(const bub_level *L, int r, int c)
{
    int32_t lx, ly, x, y;
    bub_launcher(L, &lx, &ly);
    bub_cell_xy(L, 0, r, c, &x, &y);
    return (int)((long long)BUB_AY * (x - lx) / (ly - y));
}

int main(void)
{
    static bub_game g;
    bub_shot o;
    bub_state s;
    char text[BUB_TEXT];

    /* ---- the landing -------------------------------------------------------------------------------------------- */
    {   /* two reds in the top row: a red under them pops all three, the blue hanging under them falls */
        bub_level L = lv("8c0:...rr...|...b...:rr");
        bub_begin(&L, &s);
        CHECK(bub_shoot(&L, &s, aim_to(&L, 1, 4), 0, &o));
        CHECK(o.npop == 3 && o.nfall == 1 && o.fallc[0] == 3);
        CHECK(s.end == BUB_WON && bub_count(&s, -1) == 0);
    }
    {   /* a red beside one red: two stay, and the shots run out */
        bub_level L = lv("8c0:r.......:r");
        bub_begin(&L, &s);
        CHECK(bub_shoot(&L, &s, aim_to(&L, 0, 1), 0, &o));
        CHECK(o.npop == 0 && bub_count(&s, -1) == 2 && s.end == BUB_OVER);
        CHECK(!bub_shoot(&L, &s, 0, 0, &o));
    }
    {   /* a stone among reds stays (and falls with them if they held it); a gem freed wins */
        bub_level L = lv("8c0:rr......|*......|s.......:rb");
        bub_begin(&L, &s);
        CHECK(L.gems == 1);
        CHECK(bub_shoot(&L, &s, aim_to(&L, 0, 2), 0, &o));
        CHECK(o.npop == 3 && o.nfall == 2 && o.gems_freed == 1 && s.end == BUB_WON);
    }
    {   /* a locked red: the first pop breaks its cage, the second pops it */
        bub_level L = lv("8c0:Rr......:rrr");
        bub_begin(&L, &s);
        CHECK(bub_shoot(&L, &s, aim_to(&L, 0, 2), 0, &o));
        CHECK(o.nunlock == 1 && o.npop == 2 && s.c[0][0] == 1);
        CHECK(bub_shoot(&L, &s, aim_to(&L, 0, 1), 0, &o) && bub_shoot(&L, &s, aim_to(&L, 1, 0), 0, &o));
        CHECK(s.end == BUB_WON);
    }
    {   /* a bomb breaks what is round it, stones and cages too */
        bub_level L = lv("8c0:..sRs...:!r");
        bub_begin(&L, &s);
        CHECK(bub_shoot(&L, &s, aim_to(&L, 1, 3), 0, &o));
        CHECK(o.npop >= 3 && s.c[0][3] == 0);
    }
    {   /* a rainbow joins two of a colour; with no group it stays and joins later */
        bub_level L = lv("8c0:bb......:@gg");
        bub_begin(&L, &s);
        CHECK(bub_shoot(&L, &s, aim_to(&L, 0, 2), 0, &o) && o.npop == 3 && s.end == BUB_WON);
        bub_level M = lv("8c0:g.......:@ggg");
        bub_begin(&M, &s);
        CHECK(bub_shoot(&M, &s, aim_to(&M, 0, 7), 0, &o) && o.npop == 0 && s.c[0][7] == BUB_WILD);
        CHECK(bub_shoot(&M, &s, aim_to(&M, 0, 6), 0, &o) && o.npop == 0);
        CHECK(bub_shoot(&M, &s, aim_to(&M, 0, 5), 0, &o) && o.npop == 3);
    }
    {   /* the ceiling comes down after every two shots; a marble past the line ends the try */
        bub_level L = lv("882:rbrbrbrb|bgbgbgb|rbrbrbrb|bgbgbgb|rbrbrbrb|bgbgbgb:yyyyyy");
        bub_begin(&L, &s);
        CHECK(bub_shoot(&L, &s, 0, 0, &o) && !o.dropped && s.drop == 0);
        CHECK(bub_shoot(&L, &s, 0, 0, &o) && o.dropped && s.drop == 1);
        for (int i = 0; i < 4 && s.end == BUB_PLAYING; i++) bub_shoot(&L, &s, 0, 0, &o);
        CHECK(s.end == BUB_LOW);
    }
    {   /* swapping: the next marble shot, the one in the launcher kept */
        bub_level L = lv("8c0:rr......:brg");
        bub_begin(&L, &s);
        CHECK(s.cur == 3 && s.nxt == 1);
        CHECK(bub_shoot(&L, &s, aim_to(&L, 0, 2), 1, &o) && o.marble == 1 && s.cur == 3 && s.nxt == 2);
    }

    /* ---- the geometry ------------------------------------------------------------------------------------------- */
    {
        bub_level L = lv("8c0:rrrrrrrr|.......:rr");
        bub_begin(&L, &s);
        CHECK(bub_trace(&L, &s, 0, &o) >= 0 && o.bounces == 0 && o.n == 3);   /* straight up: no wall */
        int far = BUB_AMAX;
        CHECK(bub_trace(&L, &s, far, &o) >= 0 && o.bounces >= 2);
        for (int a = -BUB_AMAX; a <= BUB_AMAX; a += 97) {   /* a mirrored box: mirrored aims, mirrored places */
            int c1 = bub_trace(&L, &s, a, NULL), c2 = bub_trace(&L, &s, -a, NULL);
            CHECK(c1 >= 0 && c2 >= 0 && c1 / BUB_MAXW == c2 / BUB_MAXW &&
                  c1 % BUB_MAXW == bub_row_len(&L, c1 / BUB_MAXW) - 1 - c2 % BUB_MAXW);
        }
        bub_move m[BUB_MOVES];
        int n = bub_moves(&L, &s, m, BUB_MOVES);
        CHECK(n > 0);
        for (int i = 0; i < n; i++) {
            CHECK(bub_trace(&L, &s, m[i].aim, NULL) == m[i].cell);
            if (i) CHECK(m[i].aim >= m[i - 1].aim);
        }
    }

    /* ---- the levels -------------------------------------------------------------------------------------------- */
    {
        static const int LENS[BUB_CHAPTERS] = BUB_CH_LENS;
        int total = 0, easy = 0, first[BUB_CHAPTERS], at = 0;
        for (int c = 0; c < BUB_CHAPTERS; c++) { first[c] = at; at += LENS[c]; total += LENS[c]; }
        CHECK(total == BUB_LEVELS);
        clock_t c0 = clock();
        for (int i = 0; i < BUB_LEVELS; i++) {
            bub_level L;
            CHECK(bub_parse(&L, bub_levels[i]));
            bub_text(&L, text);
            if (strcmp(text, bub_levels[i])) { printf("level %d: text %s\n", i + 1, text); fails++; }
            CHECK(L.nsol >= 1 && L.nsol < L.nseq + 1);
            int won = bub_check_solution(&L);
            if (won != L.nsol) { printf("level %d: its solution %d, par %d\n", i + 1, won, L.nsol); fails++; }
            bub_begin(&L, &s);
            int found = bub_solve(&L, &s, 40, NULL);
            if (found >= 0 && found < L.nsol) { printf("level %d: %d found, par %d\n", i + 1, found, L.nsol); fails++; }
            int ch = bub_chapter_of(i), greedy = bub_greedy(&L, &s);
            if (greedy >= 0) {
                easy++;
                int opening = i - first[ch] < (ch ? 2 : 3);
                if (!opening) { printf("level %d: the obvious player wins it\n", i + 1); fails++; }
            }
            for (int j = 0; j < i; j++) CHECK(strcmp(bub_levels[i], bub_levels[j]));
        }
        printf("levels: %d, the obvious player wins %d (the openings), %.1f s\n", BUB_LEVELS, easy,
               (double)(clock() - c0) / CLOCKS_PER_SEC);
        CHECK(bub_chapter_of(0) == 0 && bub_chapter_of(BUB_LEVELS - 1) == BUB_CHAPTERS - 1);
    }

    /* ---- a game: undo, the hint, the notation -------------------------------------------------------------------- */
    for (int lvl = 0; lvl < BUB_LEVELS; lvl += 7) {
        CHECK(bub_open(&g, lvl));
        bub_state s0 = g.s;
        bub_move m;
        CHECK(bub_hint(&g, &m));
        CHECK(bub_play(&g, m.aim, m.swap, NULL));
        CHECK(bub_undo(&g) && !memcmp(&g.s, &s0, sizeof s0) && !bub_undo(&g));
        /* a shot astray, then the hints to the end */
        bub_move all[BUB_MOVES];
        int n = bub_moves(&g.L, &g.s, all, BUB_MOVES);
        CHECK(n > 0 && bub_play(&g, all[n / 2].aim, 0, NULL));
        CHECK(bub_script(&g, "="));
        CHECK(g.s.end == BUB_WON);
        CHECK(bub_open(&g, lvl) && bub_script(&g, "w w h ="));
        CHECK(g.s.end == BUB_WON && g.hints == 1);
    }
    CHECK(!bub_open(&g, BUB_LEVELS) && !bub_open(&g, -1));

    /* ---- the random puzzles ------------------------------------------------------------------------------------- */
    for (int v = 0; v < BUB_VARIANTS; v++) {
        double worst = 0;
        int greedy_wins = 0;
        for (uint32_t seed = 1; seed <= 12; seed++) {
            clock_t c0 = clock();
            bub_random(&g, v, seed * 7919u);
            double t = (double)(clock() - c0) / CLOCKS_PER_SEC;
            if (t > worst) worst = t;
            CHECK(g.L.nsol >= 3 && bub_check_solution(&g.L) == g.L.nsol);
            bub_begin(&g.L, &s);
            greedy_wins += bub_greedy(&g.L, &s) >= 0;
            char a[BUB_TEXT];
            bub_text(&g.L, a);
            bub_random(&g, v, seed * 7919u);
            bub_text(&g.L, text);
            CHECK(!strcmp(a, text));
        }
        printf("random %d: the slowest %.3f s, the obvious player wins %d of 12\n", v, worst, greedy_wins);
        CHECK(worst < 1.5);
        CHECK(greedy_wins <= 3);
    }

    if (fails) { printf("bubbles: %d failed\n", fails); return 1; }
    printf("bubbles: all passed\n");
    return 0;
}
