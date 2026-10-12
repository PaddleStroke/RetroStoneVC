/*
 * Same Game's levels: src/games/samegame_levels.h. Our own levels: no published set is used. Six chapters, the board
 * growing: 6 x 6 in 3 colours, 8 x 8 in 3, 10 x 10 in 3, 12 x 10 in 4, 15 x 10 in 4, 20 x 12 in 5. A candidate: a
 * board made backwards by the rules (sm_make: every board can be cleared), its way to clear kept for the hint; its
 * best score sought by the rules (sm_reference: a beam search with a wide beam, a greedy game, the way kept), and a
 * greedy game's alone (a beam of one). The goal: nine tenths of the best (sm_goal_of). The difficulty: how far the
 * greedy play falls short of the best (a board where taking the obvious group first costs the most). Each chapter keeps
 * 25 candidates spread evenly over its difficulties, from the easiest. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/smgen apps/boardgames/tools/samegame_gen.c && \
 *       /tmp/smgen > apps/boardgames/src/games/samegame_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define SM_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/samegame_rules.c"

#define PER (SM_LEVELS / SM_CHAPTERS)
#define POOL 50

static const struct { int w, h, colours, beam; } CH[SM_CHAPTERS] = {
    {6, 6, 3, 256}, {8, 8, 3, 256}, {10, 10, 3, 160}, {12, 10, 4, 128}, {15, 10, 4, 96}, {20, 12, 5, 64},
};

typedef struct {
    int best, greedy, goal;
    double diff;
    char text[SM_W * SM_H * 3 + 4];
} cand;

static cand pool[POOL];

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

int main(void)
{
    static cand out[SM_LEVELS];
    static sm_game g;
    int nout = 0;
    for (int ch = 0; ch < SM_CHAPTERS; ch++) {
        for (int i = 0; i < POOL; i++) {
            sm_make(&g, CH[ch].w, CH[ch].h, CH[ch].colours, 6000u + 1000u * (uint32_t)ch + (uint32_t)i);
            cand *c = &pool[i];
            c->best = sm_reference(&g, CH[ch].beam);
            c->greedy = sm_best(&g.start, g.w, g.h, 1);
            c->goal = sm_goal_of(c->best);
            c->diff = c->best > 0 ? 1.0 - (double)c->greedy / c->best : 0;
            sm_text(&g, c->text);
        }
        qsort(pool, POOL, sizeof *pool, by_diff);
        fprintf(stderr, "chapter %d (%d x %d, %d colours): best %d .. %d\n", ch + 1, CH[ch].w, CH[ch].h, CH[ch].colours,
                pool[0].best, pool[POOL - 1].best);
        for (int i = 0, last = -1; i < PER; i++) {
            int at = (int)((POOL - 1) * (double)i / (PER - 1) + 0.5);
            if (at <= last) at = last + 1;
            last = at;
            out[nout++] = pool[at];
        }
    }
    printf("/*\n"
           " * Same Game's levels: our own, made by tools/samegame_gen.c (boards made backwards so that each can be cleared,\n"
           " * the best score sought by the rules' beam search; six chapters of 25). Each: the rows from the top (each tile's\n"
           " * colour), then after a space the way to clear it (each removal a place: its column and its row from the\n"
           " * bottom, as letters); in the comment the best score found and a greedy player's; the goals (nine tenths of the\n"
           " * best) in sm_level_goal.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const sm_levels[SM_LEVELS] = {\n");
    for (int i = 0; i < nout; i++)
        printf("    \"%s\", /* %d: best %d, greedy %d */\n", out[i].text, i + 1, out[i].best, out[i].greedy);
    printf("};\n\nconst uint16_t sm_level_goal[SM_LEVELS] = {");
    for (int i = 0; i < nout; i++) printf("%s%d,", i % 15 ? " " : "\n    ", out[i].goal);
    printf("\n};\n");
    return nout == SM_LEVELS ? 0 : 1;
}
