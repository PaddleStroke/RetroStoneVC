/*
 * Water Sort's levels (and Ball Sort's: tools/ballsort_gen.c is this file with SRT_ONE set): src/games/
 * watersort_levels.h. Our own levels: no published set is used. Six chapters, the colours growing: 3 and 4, 5 and 6, 7,
 * 8 and 9, 10, 11 and 12, each with two empty tubes. A candidate: four layers of each colour shuffled into the tubes
 * (the rules' srt_fill: no tube complete, few layers on their own colour); its shortest way found by the rules' A*
 * (srt_solve) with plenty of room; one whose search could not finish is left out (its par would not be sure), and
 * so is one already kept (the tubes' order forgotten). The difficulty: the shortest way's length, more when the search
 * had to look at many positions (a tangled puzzle, more dead ends). Each chapter keeps 25 candidates spread evenly over
 * its difficulties, from the easiest. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/wsgen apps/boardgames/tools/watersort_gen.c -lm && \
 *       /tmp/wsgen > apps/boardgames/src/games/watersort_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define SRT_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SRT_ONE
#define SRT_ONE 0
#define NAME "Water Sort"
#define FILE_ID "watersort"
#define PRE "ws"
#define UP "WS"
#define WHAT "pours"
#include "games/watersort_rules.c"
#endif

#define LEVELS 150
#define CHAPTERS 6
#define PER (LEVELS / CHAPTERS)
#define POOL 160
#define TRIES 3000
#define CAP 2500000                             /* the positions a candidate's search may make */

static const int COLOURS[CHAPTERS][2] = {{3, 4}, {5, 6}, {7, 7}, {8, 9}, {10, 10}, {11, 12}};

typedef struct {
    int colours, par, looked;
    double diff;
    uint64_t key;
    char text[SRT_TUBES * 5 + 1];
} cand;

static cand pool[POOL];
static int npool;

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

static int known(uint64_t key, const cand *list, int n)
{
    for (int i = 0; i < n; i++)
        if (list[i].key == key) return 1;
    return 0;
}

int main(void)
{
    static cand out[LEVELS];
    static srt_move way[SRT_PATH];
    int nout = 0;
    for (int ch = 0; ch < CHAPTERS; ch++) {
        uint32_t s = pr_seed(5000u + 100u * SRT_ONE + (uint32_t)ch);
        npool = 0;
        for (int tries = 0; tries < TRIES && npool < POOL; tries++) {
            int colours = COLOURS[ch][pr_below(&s, 2)], tubes = colours + 2;
            srt_pos p;
            srt_fill(colours, &s, &p);
            uint16_t codes[SRT_TUBES];
            for (int t = 0; t < tubes; t++) codes[t] = srt_code(&p, t);
            cand c;
            memset(&c, 0, sizeof c);
            c.key = srt_key(codes, tubes);
            if (known(c.key, pool, npool) || known(c.key, out, nout)) continue;
            int full;
            c.par = srt_solve(&p, tubes, SRT_ONE, CAP, 4, way, &c.looked, &full);
            if (c.par <= 0) continue;           /* no way, or not sure to be the shortest */
            c.colours = colours;
            c.diff = c.par + 1.5 * log2(1.0 + c.looked);
            srt_text(&p, tubes, c.text);
            pool[npool++] = c;
        }
        qsort(pool, (size_t)npool, sizeof *pool, by_diff);
        fprintf(stderr, "chapter %d: %d candidates, %d .. %d " WHAT "\n", ch + 1, npool, pool[0].par,
                pool[npool - 1].par);
        for (int i = 0, last = -1; i < PER; i++) {
            double q = ch == 0 && i == 0 ? 0 : 0.03 + 0.94 * i / (PER - 1);
            int at = (int)(q * (npool - 1) + 0.5);
            if (at <= last) at = last + 1;
            if (at >= npool) at = npool - 1;
            last = at;
            out[nout++] = pool[at];
        }
    }
    /* the chapters in order of colours, then of difficulty within each */
    printf("/*\n"
           " * " NAME "'s levels: our own, made by tools/" FILE_ID "_gen.c (colours shuffled into the tubes by chance, the\n"
           " * shortest way found by the rules' search, six chapters of 25 from the easiest). Each: the tubes from the\n"
           " * left ('|' between them), each one's colours from the bottom (1..9 a b c); in the comment the colours, the\n"
           " * shortest way and the positions the search made; the shortest ways again in " PRE "_level_par (the tests\n"
           " * check them).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const " PRE "_levels[" UP "_LEVELS] = {\n");
    for (int i = 0; i < nout; i++)
        printf("    \"%s\", /* %d: %d colours, %d " WHAT ", %d looked at */\n", out[i].text, i + 1, out[i].colours,
               out[i].par, out[i].looked);
    printf("};\n\nconst uint8_t " PRE "_level_par[" UP "_LEVELS] = {");
    for (int i = 0; i < nout; i++) printf("%s%d,", i % 25 ? " " : "\n    ", out[i].par);
    printf("\n};\n");
    return nout == LEVELS ? 0 : 1;
}
