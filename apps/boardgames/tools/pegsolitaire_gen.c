/*
 * Peg Solitaire's levels, made by chance and chosen: src/games/pegsolitaire_levels.h. Our own positions: no published
 * problem is used. Three kinds:
 *   - pictures: symmetric positions grown as one clump from the board's middle (or gaps opened in a full board),
 *     their goal a hole the symmetry keeps, solved by the rules' search (see picture() below); preferred;
 *   - positions played backwards from one marble in a goal hole (the rules' ps_random: a goal drawn, then jumps undone
 *     at random), so each has a known way to one marble in its goal; graded by the marbles and by how often a player
 *     jumping at random gets there (300 games: the rarer, the harder);
 *   - the classic full boards (every hole but one full), solved by the rules' search: for each board its classic start
 *     first (the rules' ps_new), then the empty hole nearest its middle, the goal the same hole first, then the others
 *     from the middle out, those only whose class (the parities of the marbles on the thirds of the diagonals, which
 *     no jump changes) is the start's.
 * Mirror and rotation duplicates are dropped. Six chapters by the marbles at the start (3..6, 7..10, 11..14, 15..19,
 * 20..26, 27 and more), the English board and the small triangle first, the others from the second and third
 * chapters; 25 each, chosen evenly over the chapter's grades, easiest first, a picture taken when one is near that
 * grade; the full boards end their chapters.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/psgen apps/boardgames/tools/pegsolitaire_gen.c \
 *       apps/boardgames/src/games/pegsolitaire_rules.c -lm \
 *       && /tmp/psgen > apps/boardgames/src/games/pegsolitaire_levels.h
 * Deterministic: fixed seeds. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pegsolitaire.h"
#include "games/puzrand.h"

#define CANDS 1200                              /* a chapter's */
#define PER (PS_LEVELS / PS_CHAPTERS)
#define PLAYS 300
#define TRIED (1 << 20)
#define FULL_BUDGET 30000000L
#define PIC_BUDGET 500000L                      /* a picture's search: the hard ones are dropped, not waited for */

typedef struct {
    int board, goal, pegs, succ, pic, used;
    uint64_t mask;
    float grade;
    char sol[PS_MAXH * 5 + 1];
} level;

static const char *const NAME[PS_BOARDS] = {"PS_ENGLISH", "PS_FRENCH", "PS_TRIANGLE", "PS_DIAMOND", "PS_WIEGLEB",
                                            "PS_TRIANGLE6"};
static const char *const WORD[PS_BOARDS] = {"English", "French", "triangle", "diamond", "Wiegleb's", "big triangle"};
static const int LO[PS_CHAPTERS] = {3, 7, 11, 15, 20, 27}, HI[PS_CHAPTERS] = {6, 10, 14, 19, 26, 44};
static const unsigned BOARDS[PS_CHAPTERS] = {
    1u << PS_ENGLISH | 1u << PS_TRIANGLE,
    1u << PS_ENGLISH | 1u << PS_FRENCH | 1u << PS_TRIANGLE | 1u << PS_TRIANGLE6,
    0x3f, 0x3f, 0x3f, 0x3f,
};

static uint32_t rng;

/* ---- the boards' symmetries -------------------------------------------------------------------------------------- */
/* the cell c turned by symmetry t of the board (0..7 the square's; the triangles 0..5); -1: off the board */
static int turn_cell(const psgame *g, int t, int c)
{
    int x = c % PS_SIDE, y = c / PS_SIDE, X, Y;
    if (g->tri) {
        int r = g->rows - 1, v[3] = {x, y - x, r - y}, p[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1},
                                                                  {2, 1, 0}};
        if (t >= 6) return -1;
        int a = v[p[t][0]], b = v[p[t][1]];
        X = a;
        Y = a + b;
    } else {
        int cx = (g->cols - 1) / 2, cy = (g->rows - 1) / 2, dx = x - cx, dy = y - cy;
        if (t & 1) dx = -dx;
        if (t & 2) dy = -dy;
        if (t & 4) { int k = dx; dx = dy; dy = k; }
        X = cx + dx;
        Y = cy + dy;
    }
    if (X < 0 || Y < 0 || X >= PS_SIDE || Y >= PS_SIDE) return -1;
    int d = Y * PS_SIDE + X;
    return g->hole[d] ? d : -1;
}

/* the position's key: the smallest (marbles, goal) over the board's symmetries */
static uint64_t key_of(const psgame *g)
{
    uint64_t best = ~0ull;
    for (int t = 0; t < 8; t++) {
        psgame h;
        ps_board(&h, g->board);
        int ok = 1, goal = -1;
        for (int c = 0; c < PS_CELLS && ok; c++) {
            if (!g->hole[c]) continue;
            int d = turn_cell(g, t, c);
            if (d < 0) { ok = 0; break; }
            h.peg[d] = g->start[c];
            if (c == g->goal) goal = d;
        }
        if (!ok) continue;
        uint64_t k = ps_mask(&h) * 0x9e3779b97f4a7c15ull ^ (uint64_t)(goal + 1) * 0xc2b2ae3d27d4eb4full ^
                     (uint64_t)(g->board + 1);
        if (k < best) best = k;
    }
    return best;
}

static int tried(uint64_t k)
{
    static uint64_t set[TRIED];
    k |= 1;
    for (uint64_t i = (k >> 20) & (TRIED - 1);; i = (i + 1) & (TRIED - 1)) {
        if (set[i] == k) return 1;
        if (!set[i]) { set[i] = k; return 0; }
    }
}

/* ---- grading ----------------------------------------------------------------------------------------------------- */
static int by_chance(const psgame *g0)
{
    int won = 0;
    for (int i = 0; i < PLAYS; i++) {
        psgame g = *g0;
        psmove m[PS_MAX_MOVES];
        int k;
        while ((k = ps_moves(&g, m)) > 0) {
            psmove x = m[pr_below(&rng, k)];
            ps_play(&g, x.from, x.to);
        }
        won += ps_gold(&g);
    }
    return won;
}

static void sol_text(const psgame *g, const psmove *sol, int n, char *o)
{
    o[0] = 0;
    for (int i = 0; i < n; i++) {
        char a[3], b[3];
        ps_name(g, sol[i].from, a);
        ps_name(g, sol[i].to, b);
        sprintf(o + strlen(o), "%s%s%s", i ? " " : "", a, b);
    }
}

static void finish(level *L, const psgame *g)
{
    L->board = g->board;
    L->goal = g->goal;
    L->pegs = g->pegs;
    L->mask = ps_mask(g);
    L->succ = by_chance(g);
    L->grade = L->pegs + 3 * log2f((PLAYS + 1.0f) / (L->succ + 1.0f));
    sol_text(g, g->sol, g->nsol, L->sol);
}

/* the position's class: the parities of the marbles on each third of the board's diagonals (x + y and x - y, mod 3:
 * a jump takes one marble off each third and puts one on, or the reverse, so the parities' differences never
 * change; the triangles: x + y only, its slanted jumps keep x - y) */
static int klass(const psgame *g, const uint8_t *peg)
{
    int a[3] = {0}, b[3] = {0};
    for (int c = 0; c < PS_CELLS; c++)
        if (peg[c]) {
            int x = c % PS_SIDE, y = c / PS_SIDE;
            a[(x + y) % 3] ^= 1;
            b[((x - y) % 3 + 3) % 3] ^= 1;
        }
    return (a[0] ^ a[1]) | (a[1] ^ a[2]) << 1 | (g->tri ? 0 : (b[0] ^ b[1]) << 2 | (b[1] ^ b[2]) << 3);
}

/* a classic full board: every hole but one full, solved; 0 when the search finds none in time */
static int full_board(int board, level *L)
{
    psgame g;
    ps_board(&g, board);
    int cells[PS_MAXH], n = 0;
    float mx = 0, my = 0;
    for (int c = 0; c < PS_CELLS; c++)
        if (g.hole[c]) {
            cells[n++] = c;
            mx += c % PS_SIDE;
            my += c / PS_SIDE;
        }
    mx /= n;
    my /= n;
    for (int i = 1; i < n; i++)                 /* the holes from the middle out */
        for (int j = i; j > 0; j--) {
            float da = hypotf(cells[j] % PS_SIDE - mx, cells[j] / PS_SIDE - my);
            float db = hypotf(cells[j - 1] % PS_SIDE - mx, cells[j - 1] / PS_SIDE - my);
            if (da >= db) break;
            int k = cells[j];
            cells[j] = cells[j - 1];
            cells[j - 1] = k;
        }
    psgame c;                                   /* the classic start first (the rules' ps_new) */
    ps_new(&c, board);
    psmove path[PS_MAXH];
    int r = ps_solve(&c, c.goal, path, 4 * FULL_BUDGET);
    fprintf(stderr, "  %s: the classic start: %s\n", WORD[board], r == 1 ? "solved" : r ? "too long" : "no way");
    if (r == 1) {
        memcpy(c.sol, path, sizeof path);
        c.nsol = c.pegs - 1;
        finish(L, &c);
        return 1;
    }
    int tries = 0;
    for (int i = 0; i < n && tries < 4; i++) {
        int v = cells[i];
        for (int k = -1; k < n && tries < 4; k++) {
            int t = k < 0 ? v : cells[k];
            if (k >= 0 && t == v) continue;
            ps_board(&g, board);
            memcpy(g.peg, g.hole, sizeof g.peg);
            g.peg[v] = 0;
            uint8_t one[PS_CELLS] = {0};
            one[t] = 1;
            if (klass(&g, g.peg) != klass(&g, one)) continue;   /* the class cannot change */
            g.pegs = g.holes - 1;
            g.goal = t;
            memcpy(g.start, g.peg, sizeof g.start);
            tries++;
            r = ps_solve(&g, t, path, FULL_BUDGET);
            fprintf(stderr, "  %s: empty %d, goal %d: %s\n", WORD[board], v, t, r == 1 ? "solved" : r ? "too long" :
                    "no way");
            if (r != 1) continue;
            memcpy(g.sol, path, sizeof path);
            g.nsol = g.pegs - 1;
            finish(L, &g);
            return 1;
        }
    }
    return 0;
}

/* ---- the pictures: symmetric positions ------------------------------------------------------------------------- */
/* A position drawn as a picture: its marbles symmetric (a mirror, both mirrors, a half turn or all eight turns of
 * the crosses; one of the triangles' three mirrors), grown as one clump from the board's middle (or, for the full
 * chapters, the board filled and symmetric gaps opened), its goal a hole the symmetry keeps in place, nearest the
 * middle; kept when the rules' search finds the way to one marble in that goal. */
static const int GROUPS[7][3] = {{1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}, {5, 0, 0}, {1, 2, 0}, {1, 2, 4}};
static const int TRI_GROUPS[4] = {1, 2, 5, 3};  /* the triangles' three mirrors, and their third of a turn */

static int orbit(const psgame *g, const int *gen, int c, int *out)
{
    int n = 0;
    out[n++] = c;
    for (int i = 0; i < n; i++)
        for (int k = 0; k < 3 && gen[k]; k++) {
            int d = turn_cell(g, gen[k], out[i]);
            if (d < 0) return 0;
            int dup = 0;
            for (int j = 0; j < n; j++) dup |= out[j] == d;
            if (!dup) out[n++] = d;
        }
    return n;
}

static int picture(int board, int want, level *L)
{
    psgame g;
    ps_board(&g, board);
    int gen[3] = {0, 0, 0};
    if (g.tri) gen[0] = TRI_GROUPS[pr_below(&rng, 4)];
    else memcpy(gen, GROUPS[pr_below(&rng, 7)], sizeof gen);
    int cells[PS_MAXH], n = 0, orb[8];
    float mx = 0, my = 0;
    for (int c = 0; c < PS_CELLS; c++)
        if (g.hole[c]) {
            if (!orbit(&g, gen, c, orb)) return 0;       /* the board does not have this symmetry */
            cells[n++] = c;
            mx += c % PS_SIDE;
            my += c / PS_SIDE;
        }
    mx /= n;
    my /= n;
    int dense = want > g.holes / 2;
    uint8_t on[PS_CELLS];
    for (int c = 0; c < PS_CELLS; c++) on[c] = dense ? g.hole[c] : 0;
    int count = dense ? g.holes : 0, target = dense ? g.holes - want : want, grown = 0;
    /* a clump (of marbles, or of gaps) grown from near the middle: each step an orbit next to it */
    while (grown < target) {
        int pick[PS_MAXH], np = 0;
        for (int i = 0; i < n; i++) {
            int c = cells[i], x = c % PS_SIDE, y = c / PS_SIDE, near = 0;
            if (on[c] != dense) continue;
            if (!grown) near = hypotf(x - mx, y - my) < 1.6f;
            else
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int X = x + dx, Y = y + dy;
                        if ((dx || dy) && X >= 0 && Y >= 0 && X < PS_SIDE && Y < PS_SIDE && g.hole[Y * PS_SIDE + X] &&
                            on[Y * PS_SIDE + X] != dense)
                            near = 1;
                    }
            if (near) pick[np++] = c;
        }
        if (!np) return 0;
        int k = orbit(&g, gen, pick[pr_below(&rng, np)], orb);
        if (grown + k > target + 1) {           /* too big: another try with the smallest orbit at hand */
            int best = -1, bk = 99;
            for (int i = 0; i < np; i++) {
                int kk = orbit(&g, gen, pick[i], orb);
                if (kk < bk) { bk = kk; best = pick[i]; }
            }
            if (grown + bk > target + 1) break;
            k = orbit(&g, gen, best, orb);
        }
        for (int j = 0; j < k; j++) on[orb[j]] = !dense;
        grown += k;
        count += dense ? -k : k;
    }
    if (count < LO[0] || count > HI[PS_CHAPTERS - 1] || count > g.holes - 2) return 0;
    /* the goal: a hole the symmetry keeps (an orbit of one), nearest the middle; then the solving */
    int goals[PS_MAXH], ng = 0;
    for (int i = 0; i < n; i++)
        if (orbit(&g, gen, cells[i], orb) == 1) goals[ng++] = cells[i];
    if (!ng) return 0;
    for (int i = 1; i < ng; i++)
        for (int j = i; j > 0; j--) {
            float da = hypotf(goals[j] % PS_SIDE - mx, goals[j] / PS_SIDE - my);
            float db = hypotf(goals[j - 1] % PS_SIDE - mx, goals[j - 1] / PS_SIDE - my);
            if (da >= db) break;
            int t = goals[j];
            goals[j] = goals[j - 1];
            goals[j - 1] = t;
        }
    memcpy(g.peg, on, sizeof g.peg);
    memcpy(g.start, on, sizeof g.start);
    g.pegs = count;
    for (int i = 0, tries = 0; i < ng && tries < 2; i++) {
        uint8_t one[PS_CELLS] = {0};
        one[goals[i]] = 1;
        if (klass(&g, g.peg) != klass(&g, one)) continue;
        tries++;
        g.goal = goals[i];
        psmove path[PS_MAXH];
        if (ps_solve(&g, g.goal, path, PIC_BUDGET) != 1) continue;
        memcpy(g.sol, path, sizeof path);
        g.nsol = g.pegs - 1;
        finish(L, &g);
        L->pic = 1;
        return 1;
    }
    return 0;
}

static int cmp(const void *a, const void *b)
{
    float d = ((const level *)a)->grade - ((const level *)b)->grade;
    return d < 0 ? -1 : d > 0;
}

/* the solution printed in pieces of about 100 characters */
static void print_sol(const char *s)
{
    int len = (int)strlen(s);
    for (int at = 0; at < len;) {
        int k = len - at > 100 ? 100 : len - at;
        while (at + k < len && s[at + k] != ' ') k++;
        if (at + k < len) k++;                  /* the space ends the piece */
        printf("%s\"%.*s\"", at ? "\n     " : "", k, s + at);
        at += k;
    }
}

int main(void)
{
    ps_solver_size(24);
    rng = pr_seed(20261012);
    static level cand[CANDS], full[PS_BOARDS];
    int nfull = 0;
    fprintf(stderr, "the full boards:\n");
    for (int b = 0; b < PS_BOARDS; b++)
        if (full_board(b, &full[nfull])) nfull++;
    static const char *const head[] = {
        "/*",
        " * Peg Solitaire's levels: our own positions, made by tools/pegsolitaire_gen.c (symmetric pictures solved by",
        " * search, positions played backwards from one marble in the goal hole, the classic full boards solved by",
        " * search), six chapters of 25 by the marbles",
        " * at the start, each from the easiest. Each: its board, its goal hole (a cell of the 9 x 9 grid), its marbles",
        " * (bit i: the board's hole i, in cell order) and a way to one marble in the goal; then (in the comment) the",
        " * marbles, how many of 300 games played at random end in the goal, its grade.",
        " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).",
        " */",
        "const ps_level_t ps_levels[PS_LEVELS] = {",
    };
    for (size_t i = 0; i < sizeof head / sizeof head[0]; i++) printf("%s\n", head[i]);
    int number = 0;
    for (int ch = 0; ch < PS_CHAPTERS; ch++) {
        int nc = 0, npic = 0;
        /* the pictures first (fewer tries for the big chapters: their searches are long) */
        for (long tries = 0, most = ch < 4 ? 20000 : 3000; nc < CANDS / 2 && tries < most; tries++) {
            int board;
            do board = pr_below(&rng, PS_BOARDS);
            while (!(BOARDS[ch] >> board & 1));
            int want = LO[ch] + pr_below(&rng, HI[ch] - LO[ch] + 1);
            level L;
            memset(&L, 0, sizeof L);
            if (!picture(board, want, &L) || L.pegs < LO[ch] || L.pegs > HI[ch]) continue;
            psgame h;
            ps_set(&h, L.board, L.goal, L.mask, NULL);
            if (tried(key_of(&h))) continue;
            cand[nc++] = L;
        }
        npic = nc;
        for (long tries = 0; nc < CANDS && tries < 300000; tries++) {
            int board;
            do board = pr_below(&rng, PS_BOARDS);
            while (!(BOARDS[ch] >> board & 1));
            psgame g;
            ps_board(&g, board);
            int want = LO[ch] + pr_below(&rng, HI[ch] - LO[ch] + 1);
            if (want > g.holes - 2) continue;
            ps_random(&g, board, pr_next(&rng), want - 1);
            if (g.pegs != want) continue;
            if (tried(key_of(&g))) continue;
            memset(&cand[nc], 0, sizeof cand[nc]);
            finish(&cand[nc++], &g);
        }
        qsort(cand, (size_t)nc, sizeof(level), cmp);
        int nf = 0;
        level *ends[PS_BOARDS];
        for (int i = 0; i < nfull; i++)
            if (full[i].pegs >= LO[ch] && full[i].pegs <= HI[ch]) ends[nf++] = &full[i];
        fprintf(stderr, "chapter %d: %d candidates (%d pictures), grades %.1f..%.1f, %d full boards\n", ch + 1, nc,
                npic, nc ? cand[0].grade : 0, nc ? cand[nc - 1].grade : 0, nf);
        int take = PER - nf;
        if (nc < take) {
            fprintf(stderr, "chapter %d: too few candidates\n", ch + 1);
            return 1;
        }
        for (int i = 0; i < PER; i++) {
            const level *L = ends[i < take ? 0 : i - take];
            if (i < take) {                     /* evenly over the grades, a picture near there first */
                int at = take > 1 ? (int)((long)(nc - 1) * i / (take - 1)) : 0, pick = -1;
                for (int d = 0; pick < 0 && d <= 12; d++)
                    for (int sgn = -1; sgn <= 1 && pick < 0; sgn += 2) {
                        int j = at + sgn * d;
                        if (j >= 0 && j < nc && !cand[j].used && cand[j].pic) pick = j;
                    }
                for (int d = 0; pick < 0 && d < nc; d++)
                    for (int sgn = -1; sgn <= 1 && pick < 0; sgn += 2) {
                        int j = at + sgn * d;
                        if (j >= 0 && j < nc && !cand[j].used) pick = j;
                    }
                cand[pick].used = 1;
                L = &cand[pick];
            }
            printf("    {%s, %d, 0x%llxull,\n     ", NAME[L->board], L->goal, (unsigned long long)L->mask);
            print_sol(L->sol);
            printf("}, /* %d: %s, %d marbles, %d of %d at random, %.1f%s */\n", ++number, WORD[L->board], L->pegs,
                   L->succ, PLAYS, L->grade, i >= take ? ", the full board" : L->pic ? ", a picture" : "");
        }
    }
    printf("};\n");
    return 0;
}
