/*
 * Logic Gates' levels, made by chance and chosen: src/games/logicgates_levels.h. Our own levels: no published set is
 * used. The first levels of the chapters that bring a new part are drawn by hand (one idea each); the others come
 * from the rules' generator (lg_make, logicgates.h), band by band:
 *   - each band asks for circuits of its size: switches, gates, route switches, fuses, lamps, its par;
 *   - lg_make keeps a circuit when it is a puzzle: few settings solve it (counting the route switches and the cuts),
 *     no gate's output the same in every setting, every gate matters, no switch that never matters (one at most in
 *     the last chapter), a start at the par asked for with not every lamp lit, a clear drawing with few crossings;
 *   - its par is exact (every setting of the switches, route switches and cuts tried), its difficulty rated
 *     (lg_rate: the par, the parts that matter, the gates, the depth, XORs, branchings, route switches, fuses, alarms,
 *     lamps; fewer for many solutions);
 *   - no repeats: none the same circuit as one kept (whatever the switches' order, a mirror image, the inputs' order,
 *     the same shape with the same truth table and start), none the same circuit with another start, at most two
 *     with the same truth table in a chapter (four in the first: three switches have few);
 *   - from each band's candidates, the levels are taken evenly over their difficulty, then laid out from the easiest.
 * The switches grow along the chapters: 2 to 3 in the first, up to 5 in the second, 6 in the third, 8 from the
 * fourth.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/lggen apps/boardgames/tools/logicgates_gen.c \
 *       apps/boardgames/src/games/logicgates_rules.c -lm && /tmp/lggen > apps/boardgames/src/games/logicgates_levels.h
 * Deterministic: fixed seeds, the same levels every run. A summary of the chapters goes to stderr.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/logicgates.h"

#define K(k) (1u << (k))
#define BASIC (K(LG_NOT) | K(LG_AND) | K(LG_OR) | K(LG_XOR))
#define ALL (BASIC | K(LG_NAND) | K(LG_NOR))

typedef struct { int chapter; const char *text; } hand;

/* the hand-made levels, at the start of their chapter: one idea each */
static const hand HAND[] = {
    {0, "s@a1 s@c1 AND@b2(a1,c1) L@b3(b2)"},                                   /* AND: both on */
    {0, "S@a1 s@c1 NOT@a2(a1) AND@b3(a2,c1) L@b4(b3)"},                         /* NOT */
    {0, "s@a1 s@b1 s@c1 OR@a2(a1,b1) AND@b3(a2,c1) L@b4(b3)"},                  /* OR: either */
    {0, "S@a1 S@b1 s@c1 XOR@a2(a1,b1) AND@b3(a2,c1) L@b4(b3)"},                 /* XOR: one, not both */
    {1, "S@a1 S@c1 s@e1 NAND@b2(a1,c1) AND@c3(b2,e1) L@c4(c3)"},               /* NAND */
    {1, "S@a1 S@c1 NOR@b2(a1,c1) L@b3(b2)"},                                   /* NOR */
    {1, "s@a1 S@c1 S@e1 OR@b2(a1,c1) AND@d2(c1,e1) L@b3(b2) A@d3(d2)"},         /* an alarm */
    {1, "K@a1 S@c1 XOR@b2(a1,c1) L@b3(b2)"},                                   /* a locked switch */
    {2, "S@b1 y@b2(b1) A@a3(b2<) L@c3(b2>)"},                                  /* a route switch */
    {2, "S@a1 S@c1 Y@c2(c1) AND@b3(a1,c2<) L@b4(b3) A@d3(c2>)"},
    {3, "K@a1 NOT@a2(a1x) L@a3(a2)"},                                          /* a fuse */
    {3, "%1 K@a1 S@c1 NOR@b2(a1x,c1x) L@b3(b2)"},                              /* one cut for two fuses */
    {4, "s@a1 s@b1 s@c1 AND@b2(a1,b1,c1) L@b3(b2)"},                           /* three inputs */
    {4, "s@a1 s@b1 s@c1 s@d1 XOR@b2(a1,b1,c1) AND@c3(b2,d1) L@c4(c3)"},
};
#define NHAND (int)(sizeof HAND / sizeof HAND[0])

typedef struct { int chapter, count; lg_spec sp; } band;

static const band BANDS[] = {
    /* 1: NOT, AND, OR, XOR; 2 to 3 switches */
    {0, 5, {.sw_lo = 2, .sw_hi = 3, .gates_lo = 2, .gates_hi = 3, .kinds = BASIC, .lamps = 1,
            .fan = 10, .nots = 30, .depth_lo = 2, .depth_hi = 2, .par_lo = 1, .par_hi = 2, .sols = 2}},
    {0, 9, {.sw_lo = 3, .sw_hi = 3, .gates_lo = 2, .gates_hi = 4, .kinds = BASIC, .lamps = 1, .fan = 15,
            .nots = 25, .depth_lo = 2, .depth_hi = 3, .par_lo = 2, .par_hi = 3, .sols = 2}},
    {0, 7, {.sw_lo = 3, .sw_hi = 3, .gates_lo = 3, .gates_hi = 5, .kinds = BASIC, .lamps = 1, .fan = 30,
            .nots = 25, .depth_lo = 3, .depth_hi = 4, .par_lo = 2, .par_hi = 3, .sols = 2, .cross = 1}},
    /* 2: NAND, NOR, alarms, locked switches; up to 5 switches */
    {1, 7, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 3, .gates_hi = 4, .kinds = ALL, .need = K(LG_NAND), .lamps = 1,
            .alarms = 1, .fan = 15, .nots = 15, .depth_lo = 2, .depth_hi = 3, .par_lo = 2, .par_hi = 3, .sols = 2,
            .cross = 1}},
    {1, 7, {.sw_lo = 4, .sw_hi = 4, .gates_lo = 4, .gates_hi = 6, .kinds = ALL, .need = K(LG_NOR), .lamps = 1,
            .alarms = 1, .locked = 1, .fan = 20, .nots = 15, .depth_lo = 3, .depth_hi = 3, .par_lo = 2,
            .par_hi = 4, .sols = 2, .cross = 1}},
    {1, 7, {.sw_lo = 5, .sw_hi = 5, .gates_lo = 5, .gates_hi = 7, .kinds = ALL, .lamps = 1, .alarms = 1,
            .locked = 1, .fan = 20, .nots = 15, .depth_lo = 3, .depth_hi = 4, .par_lo = 3, .par_hi = 4, .sols = 3,
            .cross = 2}},
    /* 3: route switches; up to 6 switches */
    {2, 7, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 3, .gates_hi = 5, .kinds = ALL, .routes_lo = 1, .routes_hi = 1,
            .lamps = 1, .alarms = 1, .fan = 15, .nots = 15, .depth_lo = 2, .depth_hi = 3, .par_lo = 2, .par_hi = 3,
            .sols = 2, .need_route = 1, .cross = 1}},
    {2, 8, {.sw_lo = 5, .sw_hi = 5, .gates_lo = 4, .gates_hi = 6, .kinds = ALL, .routes_lo = 1, .routes_hi = 1,
            .lamps = 1, .alarms = 1, .locked = 1, .fan = 20, .nots = 15, .depth_lo = 3, .depth_hi = 4,
            .par_lo = 3, .par_hi = 4, .sols = 3, .need_route = 1, .cross = 2}},
    {2, 8, {.sw_lo = 6, .sw_hi = 6, .gates_lo = 5, .gates_hi = 8, .kinds = ALL, .routes_lo = 1, .routes_hi = 2,
            .lamps = 1, .alarms = 1, .locked = 1, .fan = 20, .nots = 15, .depth_lo = 3, .depth_hi = 4,
            .par_lo = 3, .par_hi = 5, .sols = 3, .need_route = 1, .cross = 2}},
    /* 4: fuses; up to 8 switches */
    {3, 7, {.sw_lo = 4, .sw_hi = 5, .gates_lo = 4, .gates_hi = 6, .kinds = ALL, .fuses_lo = 1, .fuses_hi = 2,
            .lamps = 1, .alarms = 1, .fan = 20, .nots = 15, .depth_lo = 2, .depth_hi = 4, .par_lo = 2, .par_hi = 4,
            .sols = 2, .need_cut = 1, .cross = 1}},
    {3, 8, {.sw_lo = 6, .sw_hi = 7, .gates_lo = 6, .gates_hi = 8, .kinds = ALL, .routes_hi = 1, .fuses_lo = 1,
            .fuses_hi = 3, .lamps = 1, .alarms = 1, .locked = 1, .fan = 20, .nots = 15, .depth_lo = 3,
            .depth_hi = 5, .par_lo = 3, .par_hi = 5, .sols = 3, .need_cut = 1, .cross = 2}},
    {3, 8, {.sw_lo = 8, .sw_hi = 8, .gates_lo = 7, .gates_hi = 10, .kinds = ALL, .routes_hi = 1, .fuses_lo = 1,
            .fuses_hi = 3, .lamps = 1, .alarms = 1, .locked = 1, .fan = 20, .nots = 15, .depth_lo = 3,
            .depth_hi = 5, .par_lo = 4, .par_hi = 6, .sols = 4, .need_cut = 1, .cross = 3}},
    /* 5: three inputs, branchings, two lamps */
    {4, 8, {.sw_lo = 6, .sw_hi = 6, .gates_lo = 5, .gates_hi = 8, .kinds = ALL, .routes_hi = 1, .fuses_hi = 1,
            .lamps = 2, .alarms = 1, .three = 40, .fan = 30, .nots = 10, .depth_lo = 3, .depth_hi = 4, .par_lo = 3,
            .par_hi = 5, .sols = 3, .cross = 2}},
    {4, 8, {.sw_lo = 7, .sw_hi = 7, .gates_lo = 7, .gates_hi = 10, .kinds = ALL, .routes_hi = 1, .fuses_hi = 2,
            .lamps = 2, .alarms = 1, .locked = 1, .three = 35, .fan = 30, .nots = 10, .depth_lo = 3,
            .depth_hi = 5, .par_lo = 4, .par_hi = 6, .sols = 4, .cross = 3}},
    {4, 7, {.sw_lo = 8, .sw_hi = 8, .gates_lo = 8, .gates_hi = 12, .kinds = ALL, .routes_hi = 1, .fuses_hi = 2,
            .lamps = 2, .alarms = 1, .locked = 1, .three = 30, .fan = 30, .nots = 10, .depth_lo = 4,
            .depth_hi = 5, .par_lo = 5, .par_hi = 7, .sols = 4, .cross = 3}},
    /* 6: everything, 7 to 8 switches */
    {5, 8, {.sw_lo = 7, .sw_hi = 8, .gates_lo = 8, .gates_hi = 11, .kinds = ALL, .routes_lo = 1, .routes_hi = 2,
            .fuses_lo = 1, .fuses_hi = 2, .lamps = 2, .alarms = 1, .locked = 1, .three = 15, .fan = 25,
            .nots = 10, .depth_lo = 3, .depth_hi = 5, .par_lo = 4, .par_hi = 6, .sols = 4, .herrings = 1,
            .cross = 3}},
    {5, 9, {.sw_lo = 8, .sw_hi = 8, .gates_lo = 9, .gates_hi = 12, .kinds = ALL, .routes_lo = 1, .routes_hi = 2,
            .fuses_lo = 1, .fuses_hi = 3, .lamps = 2, .alarms = 1, .locked = 1, .three = 15, .fan = 25,
            .nots = 10, .depth_lo = 4, .depth_hi = 6, .par_lo = 5, .par_hi = 7, .sols = 4, .herrings = 1,
            .cross = 4}},
    {5, 8, {.sw_lo = 8, .sw_hi = 8, .gates_lo = 10, .gates_hi = 14, .kinds = ALL, .routes_lo = 2, .routes_hi = 2,
            .fuses_lo = 2, .fuses_hi = 3, .lamps = 2, .alarms = 1, .locked = 1, .three = 15, .fan = 25,
            .nots = 10, .depth_lo = 4, .depth_hi = 6, .par_lo = 6, .par_hi = 8, .sols = 4, .herrings = 1,
            .cross = 4}},
};
#define NBANDS (int)(sizeof BANDS / sizeof BANDS[0])

typedef struct { char text[1024]; lg_rating r; int chapter; } level;

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

static void keep(const char *text, const lg_rating *r, int chapter)
{
    level *k = &kept[nkept++];
    snprintf(k->text, sizeof k->text, "%s", text);
    k->r = *r;
    k->chapter = chapter;
}

static int by_score(const void *a, const void *b)
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
    for (int c = 0; c < LG_CHAPTERS; c++) {
        for (int h = 0; h < NHAND; h++) {
            if (HAND[h].chapter != c) continue;
            lg_rating r;
            char err[128];
            if (!lg_parse(HAND[h].text, &L, err, sizeof err) || !lg_rate(&L, &r)) {
                fprintf(stderr, "hand level %d: %s\n", h + 1, err);
                return 1;
            }
            if (repeat(&r, c)) fprintf(stderr, "hand level %d repeats one\n", h + 1);
            keep(HAND[h].text, &r, c);
            per_chapter[c]++;
        }
        for (int b = 0; b < NBANDS; b++) {
            const band *B = &BANDS[b];
            if (B->chapter != c) continue;
            /* the candidates: unlike any kept and any other candidate */
            int np = 0, want = 12 * B->count;
            for (uint32_t seed = 1000u * (uint32_t)(b + 1); seed < 1000u * (uint32_t)(b + 1) + 4000 && np < want;
                 seed++) {
                level *p = &pool[np];
                if (!lg_make(seed, &B->sp, 300, p->text, sizeof p->text, &p->r) || repeat(&p->r, c)) continue;
                int dup = 0;
                for (int i = 0; i < np && !dup; i++) {
                    const lg_rating *k = &pool[i].r;
                    dup = k->structure == p->r.structure || (k->shape == p->r.shape && k->func == p->r.func);
                }
                if (!dup) np++;
            }
            if (np < B->count) {
                fprintf(stderr, "band %d: %d candidates for %d levels\n", b + 1, np, B->count);
                return 1;
            }
            qsort(pool, (size_t)np, sizeof pool[0], by_score);
            /* evenly over their difficulty (the easiest and hardest tenth left out), each unlike those kept */
            int lo = np / 10, hi = np - 1 - np / 10, start = nkept;
            for (int i = 0; i < B->count; i++) {
                int at = B->count > 1 ? lo + (hi - lo) * i / (B->count - 1) : (lo + hi) / 2;
                for (int d = 0; d < np; d++) {
                    int j = at + (d & 1 ? -(d + 1) / 2 : d / 2);
                    if (j < 0 || j >= np || pool[j].chapter < 0 || repeat(&pool[j].r, c)) continue;
                    keep(pool[j].text, &pool[j].r, c);
                    pool[j].chapter = -1;
                    break;
                }
            }
            for (int i = 0; i < np; i++) pool[i].chapter = 0;
            if (nkept - start != B->count) {
                fprintf(stderr, "band %d: %d of %d levels\n", b + 1, nkept - start, B->count);
                return 1;
            }
            qsort(kept + start, (size_t)B->count, sizeof kept[0], by_score);
            per_chapter[c] += B->count;
        }
        if (per_chapter[c] != LG_PER_CHAPTER) {
            fprintf(stderr, "chapter %d: %d levels\n", c + 1, per_chapter[c]);
            return 1;
        }
    }
    printf("/*\n");
    printf(" * Logic Gates: the 150 levels (the format: logicgates.h), 25 a chapter, and their pars (the fewest\n");
    printf(" * moves; the tests check them). Made by tools/logicgates_gen.c (do not edit by hand): the first\n");
    printf(" * levels of a chapter that brings a new part drawn by hand, the others by the generator (lg_make),\n");
    printf(" * chosen evenly over their difficulty and laid out from the easiest; no two the same circuit.\n");
    for (int c = 0; c < LG_CHAPTERS; c++) {
        int s0 = 99, s1 = 0, g0 = 99, g1 = 0, p0 = 99, p1 = 0;
        for (int i = c * LG_PER_CHAPTER; i < (c + 1) * LG_PER_CHAPTER; i++) {
            const lg_rating *r = &kept[i].r;
            if (r->sw < s0) s0 = r->sw;
            if (r->sw > s1) s1 = r->sw;
            if (r->gates < g0) g0 = r->gates;
            if (r->gates > g1) g1 = r->gates;
            if (r->par < p0) p0 = r->par;
            if (r->par > p1) p1 = r->par;
        }
        printf(" * Chapter %d: %d to %d switches, %d to %d gates, par %d to %d.\n", c + 1, s0, s1, g0, g1, p0, p1);
        fprintf(stderr, "chapter %d: %d-%d switches, %d-%d gates, par %d-%d\n", c + 1, s0, s1, g0, g1, p0, p1);
    }
    printf(" * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\nconst char *const lg_levels[LG_LEVELS] = {\n");
    for (int i = 0; i < nkept; i++) {
        if (i % LG_PER_CHAPTER == 0) printf("    /* chapter %d */\n", i / LG_PER_CHAPTER + 1);
        printf("    \"%s\",\n", kept[i].text);
        const lg_rating *r = &kept[i].r;
        fprintf(stderr, "%3d sw %d var %2d gates %2d depth %d rt %d fu %d lamps %d+%d par %d sols %d herr %d cross %d "
                "score %5.1f\n", i + 1, r->sw, r->vars, r->gates, r->depth, r->routes, r->fuses, r->lamps,
                r->alarms, r->par, r->nsol, r->herrings, r->crossings, r->score);
    }
    printf("};\n\nconst uint8_t lg_pars[LG_LEVELS] = {\n");
    for (int c = 0; c < LG_CHAPTERS; c++) {
        printf("   ");
        for (int i = 0; i < LG_PER_CHAPTER; i++) printf(" %d,", kept[c * LG_PER_CHAPTER + i].r.par);
        printf("\n");
    }
    printf("};\n");
    return 0;
}
