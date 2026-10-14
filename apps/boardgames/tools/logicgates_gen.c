/*
 * Logic Gates' levels, made by chance and chosen: src/games/logicgates_levels.h. Our own levels: no published set is
 * used. The levels that bring a new gate or part are drawn by hand (one gentle idea each); the others come from the
 * rules' generator (lg_make, logicgates.h), band by band, in the order of the plan below:
 *   - each band asks for circuits of its size: switches, the gates it may use, route switches, cuttable wires, lamps,
 *     alarm lamps, its par;
 *   - lg_make keeps a circuit when it is a puzzle: few settings solve it (counting the route switches and the cuts),
 *     no gate's output the same in every setting, every gate matters, no switch that never matters (one at most in
 *     the last chapter), a start at the par asked for with not every lamp lit, a clear drawing with few crossings;
 *   - its par is exact (every setting of the switches, route switches and cuts tried), its difficulty rated
 *     (lg_rate: the par, the parts that matter, the gates, the depth, XORs, branchings, route switches, cables, alarms,
 *     lamps; fewer for many solutions);
 *   - no repeats: none the same circuit as one kept (whatever the switches' order, a mirror image, the inputs' order,
 *     the same shape with the same truth table and start), none the same circuit with another start, at most two
 *     with the same truth table in a chapter (four in the first);
 *   - from each band's candidates, the levels are taken evenly over their difficulty (the narrower drawing when two
 *     are alike), then laid out from the easiest.
 * The curve: one gate (and one part) a chapter, and every chapter grows from a few switches to 8.
 *   1 (40 levels: the free edition's): AND and OR, alarm lamps as soon as the puzzles need them (with AND and OR
 *     alone, every switch on lights everything); NOT from its middle;
 *   2: XOR (locked switches);   3: NAND (route switches);   4: NOR (cuttable wires);   5: gates of three inputs,
 *   harder mixes, two lamps;   6: everything, the biggest circuits.
 * Every level's drawing is searched here at length (lg_route_best: the parts' rows and the rows' orders) and stored
 * after its circuit (lg_layout_text): the game reads it and searches nothing.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/lggen apps/boardgames/tools/logicgates_gen.c \
 *       apps/boardgames/src/games/logicgates_rules.c -lm && /tmp/lggen > apps/boardgames/src/games/logicgates_levels.h
 * Deterministic: fixed seeds, the same levels every run. A summary of the chapters goes to stderr.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/logicgates.h"

#define K(k) (1u << (k))
#define C1 (K(LG_AND) | K(LG_OR) | K(LG_NOT))     /* the gates known in chapter 1 (NOT from its middle) */
#define C2 (C1 | K(LG_XOR))                         /* 2 */
#define C3 (C2 | K(LG_NAND))                        /* 3 */
#define ALL (C3 | K(LG_NOR))                        /* 4 on */
#define AO (K(LG_AND) | K(LG_OR))

/* a step of the plan: a level drawn by hand (text), or a band of `count` levels made by the generator */
typedef struct { int chapter; const char *text; int count; lg_spec sp; } step;

static const step PLAN[] = {
    /* 1 (40 levels, the free edition's): AND and OR up to 8 switches, alarm lamps; then NOT */
    {0, "s@a1 s@c1 AND@b2(a1,c1) L@b3(b2)", 0, {0}},                                   /* AND: both on */
    {0, "s@a1 s@b1 s@c1 OR@a2(a1,b1) AND@b3(a2,c1) L@b4(b3)", 0, {0}},                  /* OR: either */
    {0, NULL, 2, {.sw_lo = 3, .sw_hi = 3, .gates_lo = 2, .gates_hi = 3, .kinds = AO, .lamps = 1, .fan = 10,
                  .depth_lo = 2, .depth_hi = 2, .par_lo = 2, .par_hi = 3, .sols = 2}},
    {0, "s@a1 S@c1 S@e1 OR@b2(a1,c1) AND@d2(c1,e1) L@b3(b2) A@d3(d2)", 0, {0}},         /* an alarm: keep it dark */
    {0, NULL, 4, {.sw_lo = 4, .sw_hi = 5, .gates_lo = 3, .gates_hi = 5, .kinds = AO, .lamps = 1, .alarms = 1,
                  .alarm_need = 1, .fan = 20, .depth_lo = 2, .depth_hi = 3, .par_lo = 2, .par_hi = 3, .sols = 2,
                  .cross = 1}},
    {0, NULL, 5, {.sw_lo = 6, .sw_hi = 6, .gates_lo = 4, .gates_hi = 7, .kinds = AO, .lamps = 2, .alarms = 1,
                  .alarm_need = 1, .fan = 20, .depth_lo = 2, .depth_hi = 4, .par_lo = 2, .par_hi = 4, .sols = 2,
                  .cross = 1, .wide = 5.0f}},
    {0, NULL, 6, {.sw_lo = 7, .sw_hi = 8, .gates_lo = 5, .gates_hi = 9, .kinds = AO, .lamps = 2, .alarms = 1,
                  .alarm_need = 1, .fan = 20, .depth_lo = 3, .depth_hi = 4, .par_lo = 3, .par_hi = 5, .sols = 3,
                  .cross = 1, .wide = 5.5f}},
    {0, "S@a1 s@c1 NOT@a2(a1) AND@b3(a2,c1) L@b4(b3)", 0, {0}},                         /* NOT */
    {0, NULL, 5, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 3, .gates_hi = 4, .kinds = C1, .need = K(LG_NOT),
                  .lamps = 1, .alarms = 1, .fan = 15, .nots = 30, .depth_lo = 2, .depth_hi = 3, .par_lo = 2,
                  .par_hi = 3, .sols = 2, .cross = 1}},
    {0, NULL, 6, {.sw_lo = 5, .sw_hi = 6, .gates_lo = 4, .gates_hi = 7, .kinds = C1, .need = K(LG_NOT),
                  .lamps = 2, .alarms = 1, .fan = 20, .nots = 25, .depth_lo = 3, .depth_hi = 4, .par_lo = 3,
                  .par_hi = 4, .sols = 2, .cross = 1, .wide = 5.0f}},
    {0, NULL, 8, {.sw_lo = 7, .sw_hi = 8, .gates_lo = 6, .gates_hi = 9, .kinds = C1, .need = K(LG_NOT),
                  .lamps = 2, .alarms = 1, .fan = 20, .nots = 20, .depth_lo = 3, .depth_hi = 5, .par_lo = 3,
                  .par_hi = 5, .sols = 3, .cross = 2, .wide = 5.5f}},
    /* 2: XOR, locked switches */
    {1, "S@a1 S@b1 s@c1 XOR@a2(a1,b1) AND@b3(a2,c1) L@b4(b3)", 0, {0}},                 /* XOR: one, not both */
    {1, "K@a1 S@b1 s@c1 XOR@a2(a1,b1) AND@b3(a2,c1) L@b4(b3)", 0, {0}},                 /* a locked switch */
    {1, NULL, 7, {.sw_lo = 3, .sw_hi = 5, .gates_lo = 3, .gates_hi = 5, .kinds = C2, .need = K(LG_XOR),
                  .lamps = 1, .alarms = 1, .locked = 1, .fan = 15, .nots = 15, .depth_lo = 2, .depth_hi = 3,
                  .par_lo = 2, .par_hi = 3, .sols = 2, .cross = 1}},
    {1, NULL, 8, {.sw_lo = 5, .sw_hi = 6, .gates_lo = 4, .gates_hi = 7, .kinds = C2, .need = K(LG_XOR),
                  .lamps = 2, .alarms = 1, .locked = 1, .fan = 20, .nots = 15, .depth_lo = 3, .depth_hi = 4,
                  .par_lo = 3, .par_hi = 4, .sols = 2, .cross = 1, .wide = 5.0f}},
    {1, NULL, 8, {.sw_lo = 7, .sw_hi = 8, .gates_lo = 6, .gates_hi = 10, .kinds = C2, .need = K(LG_XOR),
                  .lamps = 2, .alarms = 1, .locked = 1, .fan = 20, .nots = 15, .depth_lo = 3, .depth_hi = 5,
                  .par_lo = 3, .par_hi = 5, .sols = 3, .cross = 2, .wide = 5.5f}},
    /* 3: NAND, route switches */
    {2, "S@a1 S@c1 s@e1 NAND@b2(a1,c1) AND@c3(b2,e1) L@c4(c3)", 0, {0}},               /* NAND */
    {2, NULL, 6, {.sw_lo = 3, .sw_hi = 5, .gates_lo = 3, .gates_hi = 5, .kinds = C3, .need = K(LG_NAND),
                  .lamps = 1, .alarms = 1, .locked = 1, .fan = 15, .nots = 15, .depth_lo = 2, .depth_hi = 3,
                  .par_lo = 2, .par_hi = 3, .sols = 2, .cross = 1}},
    {2, "S@b1 y@b2(b1) A@a3(b2<) L@c3(b2>)", 0, {0}},                                  /* a route switch */
    {2, "S@a1 S@c1 Y@c2(c1) AND@b3(a1,c2<) L@b4(b3) A@d3(c2>)", 0, {0}},
    {2, NULL, 6, {.sw_lo = 4, .sw_hi = 6, .gates_lo = 3, .gates_hi = 6, .kinds = C3, .routes_lo = 1,
                  .routes_hi = 1, .lamps = 1, .alarms = 1, .fan = 15, .nots = 15, .depth_lo = 2, .depth_hi = 4,
                  .par_lo = 2, .par_hi = 4, .sols = 2, .need_route = 1, .cross = 1, .wide = 5.0f}},
    {2, NULL, 5, {.sw_lo = 6, .sw_hi = 7, .gates_lo = 5, .gates_hi = 8, .kinds = C3, .need = K(LG_NAND),
                  .routes_hi = 1, .lamps = 2, .alarms = 1, .locked = 1, .fan = 20, .nots = 15, .depth_lo = 3,
                  .depth_hi = 4, .par_lo = 3, .par_hi = 5, .sols = 3, .cross = 2, .wide = 5.5f}},
    {2, NULL, 5, {.sw_lo = 8, .sw_hi = 8, .gates_lo = 7, .gates_hi = 10, .kinds = C3, .need = K(LG_NAND),
                  .routes_hi = 1, .lamps = 2, .alarms = 1, .locked = 1, .fan = 20, .nots = 15, .depth_lo = 3,
                  .depth_hi = 5, .par_lo = 4, .par_hi = 6, .sols = 3, .cross = 2, .wide = 5.5f}},
    /* 4: NOR, cuttable wires */
    {3, "S@a1 S@c1 NOR@b2(a1,c1) L@b3(b2)", 0, {0}},                                   /* NOR */
    {3, NULL, 6, {.sw_lo = 3, .sw_hi = 5, .gates_lo = 3, .gates_hi = 5, .kinds = ALL, .need = K(LG_NOR),
                  .lamps = 1, .alarms = 1, .locked = 1, .fan = 15, .nots = 15, .depth_lo = 2, .depth_hi = 3,
                  .par_lo = 2, .par_hi = 3, .sols = 2, .cross = 1}},
    {3, "K@a1 NOT@a2(a1x) L@a3(a2)", 0, {0}},                                          /* a cable to cut */
    {3, "%1 K@a1 S@c1 NOR@b2(a1x,c1x) L@b3(b2)", 0, {0}},                              /* one cut for two */
    {3, NULL, 6, {.sw_lo = 4, .sw_hi = 6, .gates_lo = 4, .gates_hi = 7, .kinds = ALL, .fuses_lo = 1, .fuses_hi = 2,
                  .lamps = 1, .alarms = 1, .locked = 1, .fan = 20, .nots = 10, .depth_lo = 2, .depth_hi = 4,
                  .par_lo = 2, .par_hi = 4, .sols = 2, .need_cut = 1, .cross = 1, .wide = 5.0f}},
    {3, NULL, 5, {.sw_lo = 6, .sw_hi = 7, .gates_lo = 5, .gates_hi = 8, .kinds = ALL, .need = K(LG_NOR),
                  .routes_hi = 1, .fuses_hi = 2, .lamps = 2, .alarms = 1, .locked = 1, .fan = 20, .nots = 10,
                  .depth_lo = 3, .depth_hi = 5, .par_lo = 3, .par_hi = 5, .sols = 3, .cross = 2, .wide = 5.5f}},
    {3, NULL, 5, {.sw_lo = 8, .sw_hi = 8, .gates_lo = 7, .gates_hi = 10, .kinds = ALL, .need = K(LG_NOR),
                  .routes_hi = 1, .fuses_hi = 2, .lamps = 2, .alarms = 1, .locked = 1, .fan = 20, .nots = 10,
                  .depth_lo = 3, .depth_hi = 5, .par_lo = 4, .par_hi = 6, .sols = 3, .cross = 2, .wide = 5.5f}},
    /* 5: three inputs, harder mixes, branchings, two lamps */
    {4, "s@a1 s@b1 s@c1 AND@b2(a1,b1,c1) L@b3(b2)", 0, {0}},                           /* three inputs */
    {4, "s@a1 s@b1 s@c1 s@d1 XOR@b2(a1,b1,c1) AND@c3(b2,d1) L@c4(c3)", 0, {0}},
    {4, NULL, 7, {.sw_lo = 5, .sw_hi = 6, .gates_lo = 4, .gates_hi = 7, .kinds = ALL, .fuses_hi = 1, .lamps = 1,
                  .alarms = 1, .locked = 1, .three = 40, .fan = 25, .nots = 10, .depth_lo = 2, .depth_hi = 4,
                  .par_lo = 3, .par_hi = 4, .sols = 2, .cross = 1, .wide = 5.0f}},
    {4, NULL, 8, {.sw_lo = 6, .sw_hi = 7, .gates_lo = 6, .gates_hi = 9, .kinds = ALL, .routes_hi = 1, .fuses_hi = 2,
                  .lamps = 2, .alarms = 1, .locked = 1, .three = 35, .fan = 30, .nots = 10, .depth_lo = 3,
                  .depth_hi = 5, .par_lo = 3, .par_hi = 5, .sols = 3, .cross = 2, .wide = 5.5f}},
    {4, NULL, 8, {.sw_lo = 8, .sw_hi = 8, .gates_lo = 8, .gates_hi = 12, .kinds = ALL, .routes_hi = 1,
                  .fuses_hi = 2, .lamps = 2, .alarms = 1, .locked = 1, .three = 30, .fan = 30, .nots = 10,
                  .depth_lo = 4, .depth_hi = 5, .par_lo = 5, .par_hi = 7, .sols = 4, .cross = 3, .wide = 6.0f}},
    /* 6: everything, 7 and 8 switches */
    {5, NULL, 8, {.sw_lo = 7, .sw_hi = 8, .gates_lo = 8, .gates_hi = 11, .kinds = ALL, .routes_lo = 1,
                  .routes_hi = 2, .fuses_lo = 1, .fuses_hi = 2, .lamps = 2, .alarms = 1, .locked = 1, .three = 15,
                  .fan = 25, .nots = 10, .depth_lo = 3, .depth_hi = 5, .par_lo = 4, .par_hi = 6, .sols = 4,
                  .herrings = 1, .cross = 3, .wide = 6.0f}},
    {5, NULL, 9, {.sw_lo = 8, .sw_hi = 8, .gates_lo = 9, .gates_hi = 12, .kinds = ALL, .routes_lo = 1,
                  .routes_hi = 2, .fuses_lo = 1, .fuses_hi = 3, .lamps = 2, .alarms = 1, .locked = 1, .three = 15,
                  .fan = 25, .nots = 10, .depth_lo = 4, .depth_hi = 6, .par_lo = 5, .par_hi = 7, .sols = 4,
                  .herrings = 1, .cross = 4, .wide = 6.0f}},
    {5, NULL, 8, {.sw_lo = 8, .sw_hi = 8, .gates_lo = 10, .gates_hi = 14, .kinds = ALL, .routes_lo = 2,
                  .routes_hi = 2, .fuses_lo = 2, .fuses_hi = 3, .lamps = 2, .alarms = 1, .locked = 1, .three = 15,
                  .fan = 25, .nots = 10, .depth_lo = 4, .depth_hi = 6, .par_lo = 6, .par_hi = 8, .sols = 4,
                  .herrings = 1, .cross = 4, .wide = 6.5f}},
};
#define NPLAN (int)(sizeof PLAN / sizeof PLAN[0])

typedef struct { char text[1024]; lg_rating r; int chapter; float width; } level;

static level kept[LG_LEVELS];
static int nkept;

/* the same as a level kept, or near it (the same circuit with another start), or a third with its truth table in
 * the chapter (four in the first) */
static int repeat(const lg_rating *r, int chapter)
{
    int same_func = 0;
    for (int i = 0; i < nkept; i++) {
        const lg_rating *k = &kept[i].r;
        if (k->exact == r->exact || (k->shape == r->shape && k->func == r->func && k->start == r->start)) return 1;
        if (k->structure == r->structure || (k->shape == r->shape && k->func == r->func)) return 1;
        same_func += kept[i].chapter == chapter && k->func == r->func;
    }
    return same_func >= (chapter ? 2 : 4);   /* three switches have few truth tables */
}

static void keep(const char *text, const lg_rating *r, int chapter, float width)
{
    level *k = &kept[nkept++];
    snprintf(k->text, sizeof k->text, "%s", text);
    k->r = *r;
    k->chapter = chapter;
    k->width = width;
}

/* by difficulty; alike (within half a point): the narrower drawing first */
static int by_score(const void *a, const void *b)
{
    const level *x = a, *y = b;
    if (x->r.score < y->r.score - 0.5f) return -1;
    if (x->r.score > y->r.score + 0.5f) return 1;
    return x->width < y->width ? -1 : x->width > y->width;
}

static int by_score_only(const void *a, const void *b)
{
    float x = ((const level *)a)->r.score, y = ((const level *)b)->r.score;
    return x < y ? -1 : x > y;
}

#define POOL 400

int main(void)
{
    static level pool[POOL];
    static lg_level L;
    int per_chapter[LG_CHAPTERS] = {0};
    for (int b = 0; b < NPLAN; b++) {
        const step *P = &PLAN[b];
        int c = P->chapter;
        if (P->text) {                          /* drawn by hand */
            lg_rating r;
            char err[128];
            if (!lg_parse(P->text, &L, err, sizeof err) || !lg_rate(&L, &r)) {
                fprintf(stderr, "step %d (by hand): %s\n", b + 1, err);
                return 1;
            }
            if (repeat(&r, c)) fprintf(stderr, "step %d (by hand) repeats a level\n", b + 1);
            keep(P->text, &r, c, L.width);
            per_chapter[c]++;
            continue;
        }
        /* the candidates: unlike any kept and any other candidate */
        int np = 0, want = 12 * P->count;
        for (uint32_t seed = 1000u * (uint32_t)(b + 1); seed < 1000u * (uint32_t)(b + 1) + 6000 && np < want; seed++) {
            level *p = &pool[np];
            if (!lg_make(seed, &P->sp, 300, p->text, sizeof p->text, &p->r) || repeat(&p->r, c)) continue;
            int dup = 0;
            for (int i = 0; i < np && !dup; i++) {
                const lg_rating *k = &pool[i].r;
                dup = k->structure == p->r.structure || (k->shape == p->r.shape && k->func == p->r.func);
            }
            if (dup || !lg_parse(p->text, &L, NULL, 0)) continue;
            p->width = L.width;
            p->chapter = 0;
            np++;
        }
        if (np < P->count) {
            fprintf(stderr, "step %d: %d candidates for %d levels\n", b + 1, np, P->count);
            return 1;
        }
        qsort(pool, (size_t)np, sizeof pool[0], by_score);
        /* evenly over their difficulty (the easiest and hardest tenth left out), each unlike those kept */
        int lo = np / 10, hi = np - 1 - np / 10, start = nkept;
        for (int i = 0; i < P->count; i++) {
            int at = P->count > 1 ? lo + (hi - lo) * i / (P->count - 1) : (lo + hi) / 2;
            for (int d = 0; d < np; d++) {
                int j = at + (d & 1 ? -(d + 1) / 2 : d / 2);
                if (j < 0 || j >= np || pool[j].chapter < 0 || repeat(&pool[j].r, c)) continue;
                keep(pool[j].text, &pool[j].r, c, pool[j].width);
                pool[j].chapter = -1;
                break;
            }
        }
        if (nkept - start != P->count) {
            fprintf(stderr, "step %d: %d of %d levels\n", b + 1, nkept - start, P->count);
            return 1;
        }
        qsort(kept + start, (size_t)P->count, sizeof kept[0], by_score_only);
        per_chapter[c] += P->count;
        fprintf(stderr, "step %d: %d candidates\n", b + 1, np);
    }
    for (int c = 0; c < LG_CHAPTERS; c++)
        if (per_chapter[c] != lg_chapter_size(c)) {
            fprintf(stderr, "chapter %d: %d levels\n", c + 1, per_chapter[c]);
            return 1;
        }
    if (nkept != LG_LEVELS) return 1;
    /* each level's drawing searched at length, stored after its circuit */
    for (int i = 0; i < nkept; i++) {
        char lay[LG_LAYOUT];
        if (!lg_parse(kept[i].text, &L, NULL, 0) || !lg_route_best(&L, 800) || !lg_layout_text(&L, lay, sizeof lay) ||
            strlen(kept[i].text) + strlen(lay) >= sizeof kept[i].text) {
            fprintf(stderr, "level %d: no drawing\n", i + 1);
            return 1;
        }
        strcat(kept[i].text, lay);
        static lg_level S;
        if (!lg_parse(kept[i].text, &S, NULL, 0) || !S.stored || fabsf(S.width - L.width) > 1e-3f) {
            fprintf(stderr, "level %d: its stored drawing does not read back\n", i + 1);
            return 1;
        }
        kept[i].width = L.width;
        kept[i].r.crossings = L.crossings;
    }
    printf("/*\n");
    printf(" * Logic Gates: the 150 levels (the format: logicgates.h), 25 a chapter, and their pars (the fewest\n");
    printf(" * moves; the tests check them). Made by tools/logicgates_gen.c (do not edit by hand): the levels\n");
    printf(" * bringing a new gate or part drawn by hand, the others by the generator (lg_make), chosen evenly\n");
    printf(" * over their difficulty and laid out from the easiest; no two the same circuit. After each circuit,\n");
    printf(" * its drawing (\" |\": each part's row, each row's order), searched by the tool.\n");
    static const char *const GATES[LG_CHAPTERS] = {
        "AND, OR, then NOT", "+ XOR", "+ NAND", "+ NOR", "+ three inputs", "everything"};
    for (int c = 0; c < LG_CHAPTERS; c++) {
        int s0 = 99, s1 = 0, g0 = 99, g1 = 0, p0 = 99, p1 = 0;
        float w1 = 0;
        for (int i = lg_chapter_start(c); i < lg_chapter_start(c) + lg_chapter_size(c); i++) {
            const lg_rating *r = &kept[i].r;
            if (r->sw < s0) s0 = r->sw;
            if (r->sw > s1) s1 = r->sw;
            if (r->gates < g0) g0 = r->gates;
            if (r->gates > g1) g1 = r->gates;
            if (r->par < p0) p0 = r->par;
            if (r->par > p1) p1 = r->par;
            if (kept[i].width > w1) w1 = kept[i].width;
        }
        printf(" * Chapter %d (%s): %d to %d switches, %d to %d gates, par %d to %d.\n", c + 1, GATES[c], s0, s1, g0,
               g1, p0, p1);
        fprintf(stderr, "chapter %d (%s): %d-%d switches, %d-%d gates, par %d-%d, width %.2f at most\n", c + 1,
                GATES[c], s0, s1, g0, g1, p0, p1, w1);
    }
    printf(" * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\nconst char *const lg_levels[LG_LEVELS] = {\n");
    for (int i = 0; i < nkept; i++) {
        if (i == lg_chapter_start(lg_chapter(i))) printf("    /* chapter %d */\n", lg_chapter(i) + 1);
        printf("    \"%s\",\n", kept[i].text);
        const lg_rating *r = &kept[i].r;
        fprintf(stderr, "%3d sw %d var %2d gates %2d depth %d rt %d fu %d lamps %d+%d par %d sols %d herr %d cross %d "
                "width %.2f score %5.1f\n", i + 1, r->sw, r->vars, r->gates, r->depth, r->routes, r->fuses, r->lamps,
                r->alarms, r->par, r->nsol, r->herrings, r->crossings, kept[i].width, r->score);
    }
    printf("};\n\nconst uint8_t lg_pars[LG_LEVELS] = {\n");
    for (int c = 0; c < LG_CHAPTERS; c++) {
        printf("   ");
        for (int i = 0; i < lg_chapter_size(c); i++) {
            if (i && i % 25 == 0) printf("\n   ");
            printf(" %d,", kept[lg_chapter_start(c) + i].r.par);
        }
        printf("\n");
    }
    printf("};\n");
    return 0;
}
