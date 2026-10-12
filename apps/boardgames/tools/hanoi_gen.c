/*
 * The Tower of Hanoi's levels: src/games/hanoi_levels.h. Our own levels. A chapter a number of discs (3 to 7: 12, 18,
 * 20, 25 and 25 levels). Every position of the discs is tried with every goal peg (3^n x 3 of them), its fewest moves
 * found by the rules (hn_left); one already solved, or the mirror of one kept (the pegs read from the right), is left
 * out. The difficulty: the fewest moves, then how many discs must move (more discs to move: more to plan). Each
 * chapter keeps candidates spread evenly over its difficulties, the easiest first, and ends with the classic tower
 * (every disc on the left peg, the goal the right one: 2^n - 1 moves). Deterministic: no chance at all.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/hngen apps/boardgames/tools/hanoi_gen.c && \
 *       /tmp/hngen > apps/boardgames/src/games/hanoi_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define HN_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/hanoi_rules.c"

typedef struct {
    char text[HN_MAX + 3];
    int par, moving;
    double diff;
} cand;

static cand pool[6561];
static int npool;

/* the level's text: the goal peg, ':', each disc's peg from the largest */
static void text_of(int n, const int8_t *peg, int goal, char *s)
{
    s[0] = (char)('a' + goal);
    s[1] = ':';
    for (int i = 0; i < n; i++) s[2 + i] = (char)('a' + peg[n - 1 - i]);
    s[2 + n] = 0;
}

static int by_diff(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    double d = x->diff - y->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(x->text, y->text);
}

int main(void)
{
    static cand out[HN_LEVELS];
    int nout = 0;
    for (int ch = 0; ch < HN_CHAPTERS; ch++) {
        int n = 3 + ch, states = 1, want = hn_chapter_start(ch + 1) - hn_chapter_start(ch);
        for (int i = 0; i < n; i++) states *= 3;
        npool = 0;
        char classic[HN_MAX + 3];
        int8_t left[HN_MAX] = {0};
        text_of(n, left, 2, classic);
        for (int code = 0; code < states; code++)
            for (int goal = 0; goal < 3; goal++) {
                int8_t peg[HN_MAX], mirror[HN_MAX];
                for (int d = 0, c = code; d < n; d++, c /= 3) peg[d] = (int8_t)(c % 3), mirror[d] = (int8_t)(2 - peg[d]);
                hn_game g;
                hn_set(&g, n, peg, goal);
                if (!g.par) continue;
                cand k;
                text_of(n, peg, goal, k.text);
                char m[HN_MAX + 3];
                text_of(n, mirror, 2 - goal, m);
                if (strcmp(m, k.text) < 0 && strcmp(m, classic) != 0) continue;   /* its mirror stands for both */
                if (!strcmp(k.text, classic) || (strcmp(m, classic) == 0)) continue;   /* the classic comes last */
                k.par = g.par;
                k.moving = 0;
                for (int d = 0; d < n; d++) k.moving += peg[d] != goal;
                k.diff = g.par + 0.3 * k.moving;
                pool[npool++] = k;
            }
        qsort(pool, (size_t)npool, sizeof *pool, by_diff);
        fprintf(stderr, "chapter %d (%d discs): %d candidates, %d .. %d moves\n", ch + 1, n, npool, pool[0].par,
                pool[npool - 1].par);
        /* the first level of all: two moves at least; then evenly over the rest */
        int lo = 0;
        if (ch == 0)
            while (lo < npool - 1 && pool[lo].par < 2) lo++;
        else
            lo = npool / 20;
        int hi = npool - 1 - npool / 30;
        for (int i = 0, last = -1; i < want - 1; i++) {
            int at = lo + (int)((double)(hi - lo) * i / (want - 2) + 0.5);
            if (at <= last) at = last + 1;
            last = at;
            out[nout++] = pool[at];
        }
        hn_game g;
        hn_set(&g, n, left, 2);
        strcpy(out[nout].text, classic);
        out[nout].par = g.par;
        out[nout].moving = n;
        nout++;
    }
    printf("/*\n"
           " * The Tower of Hanoi's levels: our own, made by tools/hanoi_gen.c (every position and goal peg tried, the\n"
           " * fewest moves by the rules, spread evenly from the easiest; each chapter ends with the classic tower). Each:\n"
           " * the goal peg, ':', each disc's peg from the largest; then (in the comment) its fewest moves and the discs\n"
           " * not yet on the goal peg; the fewest moves again in hn_level_par (the tests check them).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const hn_levels[HN_LEVELS] = {\n");
    for (int i = 0; i < nout; i++)
        printf("    \"%s\", /* %d: %d moves, %d discs to move */\n", out[i].text, i + 1, out[i].par, out[i].moving);
    printf("};\n\nconst uint8_t hn_level_par[HN_LEVELS] = {");
    for (int i = 0; i < nout; i++) printf("%s%d,", i % 20 ? " " : "\n    ", out[i].par);
    printf("\n};\n");
    return nout == HN_LEVELS ? 0 : 1;
}
