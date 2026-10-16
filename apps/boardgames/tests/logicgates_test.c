/*
 * Logic Gates' rules: the gates' truth tables, the values running through a circuit, the route switches and the cut
 * wires, locked switches, undo and restart, the hint, the notation; every one of the 150 levels read, drawn apart,
 * solvable, its par the stored one (the solver's, and the hint followed from the start, and "=" played), never
 * solved from the start nor by any single move, every gate needed; random play then solved; no two levels the
 * same circuit; the generator the same for the same seed, its random puzzles as good as the levels and quick, made
 * of the parts the setup screen's checklist allows only.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/logicgates.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static uint32_t rng = 12345;
static int rnd(int n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

static lg_game G;

static int load(const char *src)
{
    static lg_level L;
    char err[128];
    if (!lg_parse(src, &L, err, sizeof err)) {
        printf("parse: %s: %s\n", err, src);
        return 0;
    }
    lg_load(&G, &L, -1, 0);
    return 1;
}

static int node_at(int col, int row)
{
    for (int i = 0; i < G.L.nn; i++)
        if (G.L.n[i].col == col && G.L.n[i].row == row) return i;
    return -1;
}

static void truth_tables(void)
{
    static const struct { int kind; uint8_t t2[4], t3[8]; } T[] = {
        {LG_AND, {0, 0, 0, 1}, {0, 0, 0, 0, 0, 0, 0, 1}},
        {LG_OR, {0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1}},
        {LG_XOR, {0, 1, 1, 0}, {0, 1, 1, 0, 1, 0, 0, 1}},
        {LG_NAND, {1, 1, 1, 0}, {1, 1, 1, 1, 1, 1, 1, 0}},
        {LG_NOR, {1, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0}},
    };
    for (size_t k = 0; k < sizeof T / sizeof T[0]; k++) {
        for (int v = 0; v < 4; v++) {
            uint8_t in[2] = {(uint8_t)(v >> 1 & 1), (uint8_t)(v & 1)};
            CHECK(lg_gate(T[k].kind, in, 2) == T[k].t2[v]);
        }
        for (int v = 0; v < 8; v++) {
            uint8_t in[3] = {(uint8_t)(v >> 2 & 1), (uint8_t)(v >> 1 & 1), (uint8_t)(v & 1)};
            CHECK(lg_gate(T[k].kind, in, 3) == T[k].t3[v]);
        }
    }
    uint8_t zero = 0, one = 1;
    CHECK(lg_gate(LG_NOT, &zero, 1) == 1);
    CHECK(lg_gate(LG_NOT, &one, 1) == 0);
}

/* a circuit read from its text: each switch setting gives the values by hand */
static void propagation(void)
{
    /* lamp = (a AND b) OR NOT c; the alarm = a XOR c */
    CHECK(load("s@a1 s@c1 s@e1 AND@b2(a1,c1) NOT@e2(e1) OR@c3(b2,e2) XOR@d3(a1,e2) L@c4(c3) A@d4(d3)"));
    int a = node_at(0, 0), b = node_at(2, 0), c = node_at(4, 0), lamp = node_at(2, 3), alarm = node_at(3, 3);
    CHECK(a >= 0 && b >= 0 && c >= 0 && lamp >= 0 && alarm >= 0);
    CHECK(G.L.nsw == 3 && G.L.nrt == 0 && G.L.ncut == 0 && G.L.nvar == 3);
    for (int v = 0; v < 8; v++) {
        int va = v & 1, vb = v >> 1 & 1, vc = v >> 2 & 1;
        G.val[a] = (uint8_t)va;
        G.val[b] = (uint8_t)vb;
        G.val[c] = (uint8_t)vc;
        int solved = lg_eval(&G.L, G.val, G.cut, G.out, G.wsrc, G.wend);
        int l = (va && vb) || !vc, al = va ^ !vc;
        CHECK(G.out[lamp][0] == l);
        CHECK(G.out[alarm][0] == al);
        CHECK(solved == (l && !al) && lg_solved(&G) == solved);
    }
    lg_restart(&G);                             /* the same by moves: the values follow each flip */
    CHECK(lg_play(&G, (lg_move){LG_MSWITCH, 2}));
    CHECK(G.out[node_at(4, 1)][0] == 0 && G.out[lamp][0] == 0 && G.out[alarm][0] == 0);
    CHECK(lg_play(&G, (lg_move){LG_MSWITCH, 0}));
    CHECK(G.out[lamp][0] == 0 && G.out[alarm][0] == 1);
    CHECK(lg_play(&G, (lg_move){LG_MSWITCH, 1}));
    CHECK(G.out[lamp][0] == 1 && G.out[alarm][0] == 1 && !lg_solved(&G));
    /* the depths: a switch 0, a gate one more than its deepest input */
    CHECK(G.L.depth[a] == 0 && G.L.depth[node_at(4, 1)] == 1 && G.L.depth[node_at(2, 2)] == 2 && G.L.depth[lamp] == 3);
}

static void routes(void)
{
    CHECK(load("S@b1 y@b2(b1) A@a3(b2<) L@c3(b2>)"));
    int r = node_at(1, 1);
    CHECK(G.L.nrt == 1 && G.val[r] == 0);
    CHECK(G.out[r][0] == 1 && G.out[r][1] == 0);    /* left: the input goes left, the right carries 0 */
    CHECK(!lg_solved(&G));
    CHECK(lg_play(&G, (lg_move){LG_MROUTE, 0}));
    CHECK(G.out[r][0] == 0 && G.out[r][1] == 1);
    CHECK(lg_solved(&G));
    CHECK(!lg_play(&G, (lg_move){LG_MSWITCH, 0}));   /* solved: no more moves */
    CHECK(lg_undo(&G) && G.val[r] == 0 && !lg_solved(&G));
    CHECK(lg_play(&G, (lg_move){LG_MSWITCH, 0}));    /* the input off: both ways carry 0 */
    CHECK(G.out[r][0] == 0 && G.out[r][1] == 0);
    /* the notation's numbering: reading order from the bottom left */
    CHECK(load("s@a1 s@c1 y@a2(a1) Y@c2(c1) OR@b3(a2>,c2<) A@a4(a2<) L@b4(b3) A@c4(c2>)"));
    CHECK(G.L.rt[0] == node_at(0, 1) && G.L.rt[1] == node_at(2, 1));
    CHECK(G.val[G.L.rt[1]] == 1);
}

static void cuts(void)
{
    CHECK(load("%1 K@a1 S@c1 NOR@b2(a1x,c1x) L@b3(b2)"));
    G.par = 2;
    CHECK(G.L.ncut == 2 && G.L.cut_limit == 1 && G.L.nvar == 3);   /* the locked switch is no variable */
    int nor = node_at(1, 1), w0 = G.L.cut[0];
    CHECK(G.wsrc[w0] == 1 && G.wend[w0] == 1);
    CHECK(!lg_play(&G, (lg_move){LG_MSWITCH, 0}));   /* locked */
    CHECK(lg_play(&G, (lg_move){LG_MCUT, 0}));
    CHECK(G.cut[w0] && G.wsrc[w0] == 1 && G.wend[w0] == 0);   /* carries its signal up to the fuse, 0 beyond */
    CHECK(!lg_play(&G, (lg_move){LG_MCUT, 0}));      /* cut already */
    CHECK(!lg_play(&G, (lg_move){LG_MCUT, 1}));      /* no cut left */
    CHECK(lg_cuts(&G) == 1 && G.out[nor][0] == 0);
    CHECK(lg_play(&G, (lg_move){LG_MSWITCH, 1}));
    CHECK(lg_solved(&G) && G.nh == 2 && lg_stars(&G) == 3);
    CHECK(lg_undo(&G) && lg_undo(&G) && !G.cut[w0] && G.wend[w0] == 1);   /* only undo mends a cut */
    CHECK(!lg_undo(&G));
    /* the solver keeps cuts made: from a wrong cut there is no way but back */
    CHECK(lg_play(&G, (lg_move){LG_MCUT, 1}));
    lg_answer an;
    lg_solve(&G.L, G.val, G.cut, &an);
    CHECK(an.par < 0);
    lg_move m;
    CHECK(!lg_hint(&G, &m));
    lg_restart(&G);
    CHECK(G.nh == 0 && !lg_cuts(&G));
    lg_solve(&G.L, G.val, G.cut, &an);
    CHECK(an.par == 2 && an.nsol == 1);
}

static void stars(void)
{
    CHECK(load("s@a1 s@c1 AND@b2(a1,c1) L@b3(b2)"));
    G.par = 2;
    CHECK(lg_script(&G, "s1 s1 s1 s2"));
    CHECK(lg_solved(&G) && G.nh == 4 && lg_stars(&G) == 2);
    lg_restart(&G);
    CHECK(lg_script(&G, "s1 s1 s1 s1 s1 s2"));
    CHECK(lg_stars(&G) == 1);
    lg_restart(&G);
    CHECK(lg_script(&G, "s1 s1 u u s1 s2") && lg_stars(&G) == 3);   /* undo takes the moves back */
    lg_restart(&G);
    CHECK(lg_script(&G, "h s2") && lg_solved(&G) && G.hinted && lg_stars(&G) == 2);   /* a hint: two at most */
    lg_restart(&G);                             /* a long game: the history drops its oldest moves, never a new one */
    for (int k = 0; k < 2 * LG_HIST + 2; k++) CHECK(lg_play(&G, (lg_move){LG_MSWITCH, 0}));
    CHECK(G.nh == 2 * LG_HIST + 2);
    int back = 0;
    while (lg_undo(&G)) back++;
    CHECK(back == LG_HIST && G.val[G.L.sw[0]] == G.L.n[G.L.sw[0]].init);
    CHECK(lg_script(&G, "s1 s2") && lg_solved(&G));
    CHECK(!lg_script(&G, "s7"));
    CHECK(!lg_script(&G, "q1"));
    CHECK(!lg_script(&G, "L1"));                /* the levels open by lg_open (the shell's level screen) */
    CHECK(!lg_open(&G, LG_LEVELS) && !lg_open(&G, -1) && lg_open(&G, LG_LEVELS - 1) && G.level == LG_LEVELS - 1);
}

static void undo_all(void)
{
    for (int t = 0; t < 40; t++) {
        int level = rnd(LG_LEVELS);
        CHECK(lg_open(&G, level));
        uint8_t v0[LG_NODES], o0[LG_NODES][2];
        memcpy(v0, G.val, sizeof v0);
        memcpy(o0, G.out, sizeof o0);
        int made = 0;
        for (int k = 0; k < 30 && !lg_solved(&G); k++) {
            lg_move m = {(uint8_t)rnd(3), 0};
            int n = m.kind == LG_MSWITCH ? G.L.nsw : m.kind == LG_MROUTE ? G.L.nrt : G.L.ncut;
            if (!n) continue;
            m.idx = (uint8_t)rnd(n);
            made += lg_play(&G, m);
        }
        CHECK(G.nh == made);
        while (lg_undo(&G)) {}
        CHECK(!memcmp(v0, G.val, sizeof v0) && !memcmp(o0, G.out, sizeof o0) && !lg_cuts(&G));
    }
}

/* a level as a puzzle: read, drawn, not solved at the start, its par the solver's (and the hint's, and "="'s) */
static void puzzle(const lg_level *L, int level, int par)
{
    lg_load(&G, L, level, par);
    CHECK(!lg_solved(&G));
    lg_answer a;
    lg_solve(L, G.val, G.cut, &a);
    if (a.par != par) printf("level %d: par %d, stored %d\n", level + 1, a.par, par);
    CHECK(a.par == par && a.par >= 1);
    /* not every single move solves it (unless there is only one to make) */
    int single = 0;
    for (int v = 0; v < L->nvar; v++) {
        lg_game t = G;
        lg_move m = {L->var_kind[v], L->var_idx[v]};
        if (lg_play(&t, m) && lg_solved(&t)) single++;
    }
    if (L->nvar > 1 && single >= L->nvar) printf("level %d: every move solves it\n", level + 1);
    CHECK(L->nvar == 1 || single < L->nvar);
    /* the hint followed from the start: solved at par */
    lg_game h = G;
    lg_move m;
    int steps = 0;
    while (!lg_solved(&h) && steps < 40 && lg_hint(&h, &m) && lg_play(&h, m)) steps++;
    CHECK(lg_solved(&h) && steps == par);
    lg_game s = G;
    CHECK(lg_script(&s, "=") && lg_solved(&s) && s.nh == par && lg_stars(&s) == 3);
    /* random moves, then solved from there (undoing the cuts that block it) */
    for (int k = 0; k < 6; k++) {
        lg_game r = G;
        for (int j = 0; j < 8 && !lg_solved(&r); j++) {
            lg_move rm = {L->var_kind[rnd(L->nvar)], 0};
            rm.idx = (uint8_t)rnd(rm.kind == LG_MSWITCH ? L->nsw : rm.kind == LG_MROUTE ? L->nrt : L->ncut);
            lg_play(&r, rm);
        }
        while (!lg_solved(&r) && !lg_hint(&r, &m)) CHECK(lg_undo(&r));
        if (!lg_solved(&r)) CHECK(lg_script(&r, "=") && lg_solved(&r));
    }
}

static lg_rating rate[LG_LEVELS];

static void levels(void)
{
    int counts[LG_CHAPTERS][3] = {{0}}, sw_lo[LG_CHAPTERS], sw_hi[LG_CHAPTERS] = {0}, first[LG_KINDS],
        alarms[LG_CHAPTERS] = {0};
    memset(first, -1, sizeof first);
    for (int c = 0; c < LG_CHAPTERS; c++) sw_lo[c] = 99;
    for (int l = 0; l < LG_LEVELS; l++) {
        static lg_level L;
        char err[128];
        if (!lg_parse(lg_levels[l], &L, err, sizeof err)) {
            printf("level %d: %s\n", l + 1, err);
            fails++;
            continue;
        }
        puzzle(&L, l, lg_pars[l]);
        lg_game s;
        CHECK(lg_open(&s, l) && s.level == l && s.par == lg_pars[l]);
        /* a puzzle: no gate's output the same in every setting, every gate matters, the lamps not all lit at the
         * start, few solutions, at most one switch that never matters */
        lg_rating *r = &rate[l];
        CHECK(lg_rate(&L, r));
        if (r->consts || r->idle || r->herrings > 1) printf("level %d: %d constant gates, %d idle, %d herrings\n",
                                                            l + 1, r->consts, r->idle, r->herrings);
        CHECK(!r->consts && !r->idle && r->herrings <= 1 && r->nsol <= 4 && r->par == lg_pars[l]);
        lg_load(&G, &L, l, lg_pars[l]);
        int lit = 1, alarm = 0;   /* the lamps all lit at the start: an alarm with them */
        for (int i = 0; i < L.nn; i++) {
            lit &= L.n[i].kind != LG_LAMP || G.out[i][0];
            alarm |= L.n[i].kind == LG_ALARM && G.out[i][0];
        }
        CHECK(!lit || alarm);
        /* written back as text: the same level */
        char text[1024];
        static lg_level B;
        lg_rating rb;
        CHECK(lg_text(&L, text, sizeof text) && lg_parse(text, &B, NULL, 0) && lg_rate(&B, &rb));
        CHECK(rb.exact == r->exact);
        int c = lg_chapter(l);
        counts[c][0] += L.nrt > 0;
        counts[c][1] += L.ncut > 0;
        for (int i = 0; i < L.nn; i++) {
            counts[c][2] += L.n[i].nin == 3;
            int k = L.n[i].kind;
            if (k >= LG_NOT && k <= LG_NOR && first[k] < 0) first[k] = l;
            alarms[c] += k == LG_ALARM;
        }
        if (L.nsw < sw_lo[c]) sw_lo[c] = L.nsw;
        if (L.nsw > sw_hi[c]) sw_hi[c] = L.nsw;
        CHECK(L.cols <= LG_COLS && L.rows <= LG_ROWS);
        if (!L.stored) printf("level %d: not laid out from its stored drawing\n", l + 1);
        CHECK(L.stored);                        /* its drawing read, not searched */
        for (int i = 0; i < L.nn; i++) {        /* each chapter's gates */
            int k = L.n[i].kind;
            if (c == 0) CHECK(k != LG_XOR && k != LG_NAND && k != LG_NOR);
            if (c == 1) CHECK(k != LG_NAND && k != LG_NOR);
            if (c == 2) CHECK(k != LG_NOR);
            if (c < 4) CHECK(L.n[i].nin < 3);
        }
    }
    /* the curve: AND and OR first, NOT in the middle of chapter 1, XOR in chapter 2, NAND in 3, NOR in 4, three
     * inputs in 5; alarm lamps in chapter 1, route switches from chapter 3, cuttable wires from chapter 4 */
    CHECK(first[LG_AND] == 0 && first[LG_OR] == 1);
    CHECK(first[LG_NOT] >= 15 && first[LG_NOT] <= 25);
    CHECK(first[LG_XOR] == lg_chapter_start(1) && first[LG_NAND] == lg_chapter_start(2));
    CHECK(first[LG_NOR] == lg_chapter_start(3));
    CHECK(lg_chapter(LG_FIRST_CHAPTER - 1) == 0 && lg_chapter(LG_FIRST_CHAPTER) == 1 && lg_chapter(LG_LEVELS - 1) == 5);
    for (int k = LG_NOT; k <= LG_NOR; k++) {    /* each new gate's first level: a gentle one, a few switches */
        static lg_level F;
        CHECK(first[k] >= 0 && lg_parse(lg_levels[first[k]], &F, NULL, 0) && F.nsw <= 4 && lg_pars[first[k]] <= 3);
        printf("logicgates: %s first in level %d\n", (const char *[]){"", "NOT", "AND", "OR", "XOR", "NAND", "NOR"}[k],
               first[k] + 1);
    }
    CHECK(alarms[0] > 0);
    for (int c = 0; c < 2; c++) CHECK(!counts[c][0]);   /* route switches from chapter 3 */
    for (int c = 0; c < 3; c++) CHECK(!counts[c][1]);   /* cuttable wires from 4 */
    for (int c = 0; c < 4; c++) CHECK(!counts[c][2]);   /* three inputs from 5 */
    CHECK(counts[2][0] > 0 && counts[3][1] > 0 && counts[4][2] > 0 && counts[5][0] > 0 && counts[5][1] > 0);
    /* every chapter grows to 8 switches */
    for (int c = 0; c < LG_CHAPTERS; c++) {
        printf("logicgates: chapter %d, %d to %d switches\n", c + 1, sw_lo[c], sw_hi[c]);
        CHECK(sw_hi[c] == 8);
    }
    CHECK(sw_lo[0] == 2 && sw_lo[5] >= 7);
    /* the hand-made first levels: AND, OR, NOT */
    CHECK(lg_pars[0] == 2 && lg_pars[1] == 2 && lg_pars[first[LG_NOT]] == 2);
}
/* no two levels the same circuit: whatever the switches' order, a mirror image, the inputs' order of a gate, a route
 * switch's sides; nor the same shape with the same truth table and start. Near the same (one circuit, another
 * start): a few at most, each one deliberate. */
static void repeats(void)
{
    /* the keys: the four old levels that drew alike (21, 22, 23, 25: x AND y AND NOT z AND NOT w, every switch
     * wrong at the start) are one */
    static const char *const alike[] = {
        "S@a1 s@c1 S@d1 s@e1 NOT@a2(a1) AND@c2(c1,e1) NOT@d2(d1) AND@b3(a2,c2) AND@c4(b3,d2) L@c5(c4)",
        "s@a1 s@b1 S@c1 S@d1 AND@b2(a1,b1) NOT@c2(c1) NOT@d2(d1) AND@c3(b2,c2) AND@d4(c3,d2) L@d5(d4)",
        "s@a1 S@b1 s@c1 S@d1 NOT@b2(b1) AND@c2(a1,c1) NOT@d2(d1) AND@d3(c2,d2) AND@c4(b2,d3) L@c5(c4)",
        "s@a1 S@b1 s@c1 S@d1 NOT@b2(b1) AND@c2(a1,c1) NOT@d2(d1) AND@c3(b2,c2) AND@d4(c3,d2) L@d5(d4)",
    };
    static lg_level L;
    lg_rating r0, r;
    CHECK(lg_parse(alike[0], &L, NULL, 0) && lg_rate(&L, &r0));
    for (int i = 1; i < 4; i++) CHECK(lg_parse(alike[i], &L, NULL, 0) && lg_rate(&L, &r) && r.exact == r0.exact);
    /* another start: near; a route switch's sides swapped with its way: the same; another gate: neither */
    CHECK(lg_parse("s@a1 s@c1 S@d1 s@e1 NOT@a2(a1) AND@c2(c1,e1) NOT@d2(d1) AND@b3(a2,c2) AND@c4(b3,d2) L@c5(c4)", &L,
                   NULL, 0) && lg_rate(&L, &r));
    CHECK(r.exact != r0.exact && r.structure == r0.structure);
    lg_rating ra, rb;
    CHECK(lg_parse("S@a1 S@c1 Y@c2(c1) AND@b3(a1,c2<) L@b4(b3) A@d3(c2>)", &L, NULL, 0) && lg_rate(&L, &ra));
    CHECK(lg_parse("S@a1 S@c1 y@c2(c1) AND@b3(a1,c2>) L@b4(b3) A@d3(c2<)", &L, NULL, 0) && lg_rate(&L, &rb));
    CHECK(ra.exact == rb.exact);
    CHECK(lg_parse("S@a1 S@c1 Y@c2(c1) OR@b3(a1,c2<) L@b4(b3) A@d3(c2>)", &L, NULL, 0) && lg_rate(&L, &rb));
    CHECK(rb.exact != ra.exact && rb.structure != ra.structure);
    /* the levels */
    int near = 0;
    for (int a = 0; a < LG_LEVELS; a++)
        for (int b = a + 1; b < LG_LEVELS; b++) {
            const lg_rating *x = &rate[a], *y = &rate[b];
            int same = x->exact == y->exact || (x->shape == y->shape && x->func == y->func && x->start == y->start);
            if (same) printf("levels %d and %d: the same circuit\n", a + 1, b + 1);
            CHECK(!same);
            if (!same && (x->structure == y->structure || (x->shape == y->shape && x->func == y->func))) {
                printf("logicgates: levels %d and %d: one circuit, another start\n", a + 1, b + 1);
                near++;
            }
        }
    CHECK(near <= 3);
}

/* the points where wires of two different signals cross (a cut wire's is its own signal) */
static int crossings(const lg_level *L)
{
    float px[256], py[256];
    int np = 0;
    for (int a = 0; a < L->nw; a++)
        for (int b = 0; b < L->nw; b++) {
            const lg_wire *A = &L->w[a], *B = &L->w[b];
            if (a == b || (A->net == B->net && !A->cuttable && !B->cuttable)) continue;
            for (int i = 0; i + 1 < A->npt; i++)
                for (int j = 0; j + 1 < B->npt; j++) {
                    float ax = A->px[i], ay0 = A->py[i], ay1 = A->py[i + 1];
                    float by = B->py[j], bx0 = B->px[j], bx1 = B->px[j + 1];
                    if (fabsf(ax - A->px[i + 1]) > 1e-4f || fabsf(by - B->py[j + 1]) > 1e-4f) continue;
                    if (ax <= fminf(bx0, bx1) + 1e-3f || ax >= fmaxf(bx0, bx1) - 1e-3f) continue;
                    if (by <= fminf(ay0, ay1) + 1e-3f || by >= fmaxf(ay0, ay1) - 1e-3f) continue;
                    int dup = 0;
                    for (int k = 0; k < np; k++) dup |= fabsf(px[k] - ax) < 1e-3f && fabsf(py[k] - by) < 1e-3f;
                    if (!dup && np < 256) { px[np] = ax; py[np++] = by; }
                }
        }
    return np;
}

/* a level's drawing: rows going up, the lamps on top, a switch under the gates it feeds; no two parts
 * overlapping, no wire through a part, the cables on their wires; each wire rising from its source, square bends,
 * rising into its pin; few crossings, each one hopped */
static int drawn(const lg_level *L, int level)
{
    char err[128];
    CHECK(lg_check(L, err, sizeof err));
    int top = L->layers - 1, low = 0;
    for (int i = 0; i < L->nn; i++) {
        const lg_node *N = &L->n[i];
        int lamp = N->kind == LG_LAMP || N->kind == LG_ALARM;
        float hw = lg_half(N->kind);
        CHECK(lamp == (N->layer == top));       /* the lamps on top; the switches anywhere under their gates */
        low |= N->layer == 0;
        CHECK(N->x >= hw + 0.1f - 1e-3f && N->x <= L->width - hw - 0.1f + 1e-3f && N->y + LG_NH <= L->height + 1e-3f);
        for (int s = 0; s < N->nin; s++) CHECK(L->n[L->w[N->in[s]].from].layer < N->layer);
        for (int j = i + 1; j < L->nn; j++) {   /* no two parts overlap */
            const lg_node *M = &L->n[j];
            int apart = fabsf(N->x - M->x) >= hw + lg_half(M->kind) + 0.1f || N->y + LG_NH <= M->y ||
                        M->y + LG_NH <= N->y;
            if (!apart) printf("level %d: parts %d and %d overlap\n", level + 1, i, j);
            CHECK(apart);
        }
    }
    CHECK(low);
    for (int w = 0; w < L->nw; w++) {
        const lg_wire *W = &L->w[w];
        const lg_node *F = &L->n[W->from], *D = &L->n[W->to];
        int n = W->npt;
        CHECK(n >= 2 && n <= LG_PTS);
        CHECK(fabsf(W->py[0] - (F->y + LG_NH)) < 1e-3f && fabsf(W->px[0] - F->x) < 0.2f);
        CHECK(fabsf(W->py[n - 1] - D->y) < 1e-3f && fabsf(W->px[n - 1] - D->x) <= LG_HALF);
        CHECK(fabsf(W->px[1] - W->px[0]) < 1e-4f && W->py[1] > W->py[0]);   /* up from its source */
        CHECK(fabsf(W->px[n - 1] - W->px[n - 2]) < 1e-4f && W->py[n - 1] > W->py[n - 2]);   /* up into its pin */
        for (int i = 0; i + 1 < n; i++) {
            float x0 = W->px[i], y0 = W->py[i], x1 = W->px[i + 1], y1 = W->py[i + 1];
            CHECK(fabsf(x0 - x1) < 1e-4f || fabsf(y0 - y1) < 1e-4f);   /* square */
            CHECK(y1 >= y0 - 1e-4f);                                     /* never down */
            for (int k = 0; k < L->nn; k++) {                            /* never through a part */
                const lg_node *N = &L->n[k];
                if (k == W->from || k == W->to) continue;
                int through = fminf(x0, x1) < N->x + lg_half(N->kind) && fmaxf(x0, x1) > N->x - lg_half(N->kind) &&
                              fminf(y0, y1) < N->y + LG_NH && fmaxf(y0, y1) > N->y;
                if (through) printf("level %d: wire %d through part %d\n", level + 1, w, k);
                CHECK(!through);
            }
        }
    }
    for (int w = 0; w < L->nw; w++) {           /* a cable: two terminals on its wire, apart, bowing a little */
        const lg_wire *W = &L->w[w];
        if (!W->cuttable) continue;
        CHECK(W->ta > 0.05f && W->tb < W->len - 0.05f && W->tb - W->ta >= 0.3f - 1e-3f);
        CHECK(fabsf(W->bow) > 0.01f && fabsf(W->bow) <= 0.17f + 1e-3f);
        float dx = W->tbx - W->tax, dy = W->tby - W->tay;
        CHECK(fabsf(fabsf(dx) + fabsf(dy) - (W->tb - W->ta)) < 1e-3f && (W->fvert ? fabsf(dx) : fabsf(dy)) < 1e-3f);
    }
    int c = crossings(L);
    CHECK(c == L->crossings && L->nhop == c);
    return c;
}

static void drawing(void)
{
    int total = 0, worst = 0, worst_level = 0;
    for (int l = 0; l < LG_LEVELS; l++) {
        static lg_level L;
        if (!lg_parse(lg_levels[l], &L, NULL, 0)) continue;   /* levels() says why */
        int c = drawn(&L, l);
        CHECK(c <= 4);
        total += c;
        if (c > worst) { worst = c; worst_level = l + 1; }
    }
    printf("logicgates: %d crossings in all, at most %d (level %d)\n", total, worst, worst_level);
}

/* the setup screen's checklist: what makes a puzzle, the parts a circuit has */
static void checklist(void)
{
    enum { A = 1u << LG_C_AND, O = 1u << LG_C_OR, N = 1u << LG_C_NOT, X = 1u << LG_C_XOR, NA = 1u << LG_C_NAND,
           NO = 1u << LG_C_NOR };
    CHECK(lg_comp_ok(A) && lg_comp_ok(O | N) && lg_comp_ok(N | NA) && lg_comp_ok(N | NO) && lg_comp_ok(LG_C_ALL));
    CHECK(!lg_comp_ok(0) && !lg_comp_ok(N) && !lg_comp_ok(O) && !lg_comp_ok(X | N) && !lg_comp_ok(NA | NO | O | X));
    CHECK(!lg_comp_ok(0x7c0u));                 /* the special parts alone */
    CHECK(lg_comp_fix(0) == A && lg_comp_fix(N | (1u << LG_C_CABLE)) == (A | N | (1u << LG_C_CABLE)));
    CHECK(lg_comp_fix(O | N) == (O | N) && lg_comp_fix(0xffffffffu) == LG_C_ALL);
    static lg_level L;
    CHECK(lg_parse("s@a1 K@b1 S@c1 y@b2(b1) XOR@a3(a1,b2<) NOR@c3(b2>x,c1) L@a4(a3) L@b4(c3) A@c4(c3)", &L, NULL, 0));
    CHECK(lg_comp_of(&L) == (X | NO | 1u << LG_C_ALARM | 1u << LG_C_LAMPS | 1u << LG_C_ROUTE | 1u << LG_C_LOCK |
                             1u << LG_C_CABLE));
    /* the last resort's circuits (written in lg_random): puzzles of par 2 */
    static const char *const last[] = {
        "s@a1 S@b1 s@c1 AND@a2(a1,b1) AND@b3(a2,c1) L@b4(b3)", "S@a1 S@b1 NOR@a2(a1,b1) L@a3(a2)",
        "s@a1 s@b1 NAND@a2(a1,b1) NOT@a3(a2) L@a4(a3)", "S@a1 S@b1 OR@a2(a1,b1) NOT@a3(a2) L@a4(a3)"};
    for (int i = 0; i < 4; i++) {
        CHECK(lg_parse(last[i], &L, NULL, 0));
        puzzle(&L, -1, 2);
    }
}

/* the generator: the same level for the same seed; every random puzzle of the die tab a puzzle as the levels are,
 * made fast: with every part chosen (the default), as strict as the levels; with checklists at random, only the parts
 * chosen, as many of them as the size has room for */
static void generator(void)
{
    static const char *const NAME[LG_COMPONENTS] = {"AND", "OR", "NOT", "XOR", "NAND", "NOR", "alarm", "two lamps",
                                                    "route", "lock", "cable"};
    for (int v = 0; v < LG_RANDOM; v++) {
        lg_spec sp;
        lg_random_spec(v, LG_C_ALL, LG_C_ALL, &sp);
        double worst = 0, all = 0;
        int made = 0, has[LG_COMPONENTS] = {0}, strict = 0, loose = 0;
        uint64_t prev = 0;
        for (uint32_t seed = 1; seed <= 60; seed++) {
            char a[1024], b[1024];
            lg_rating ra, rb;
            int ka = lg_make(seed, &sp, 200, a, sizeof a, &ra), kb = lg_make(seed, &sp, 200, b, sizeof b, &rb);
            CHECK(ka == kb && (!ka || !strcmp(a, b)));
            clock_t t0 = clock();
            static lg_game g, h;
            CHECK(lg_random(&g, v, LG_C_ALL, seed));
            double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
            all += ms;
            if (ms > worst) worst = ms;
            CHECK(lg_random(&h, v, LG_C_ALL, seed) && !memcmp(&h.L.n, &g.L.n, sizeof g.L.n));   /* the same */
            CHECK(g.level == -1);
            lg_rating r;
            lg_spec base;
            lg_random_spec(v, LG_C_ALL, 0, &base);
            CHECK(lg_rate(&g.L, &r));
            CHECK(r.sw >= base.sw_lo && r.sw <= base.sw_hi && r.par >= 1 && r.par <= base.par_hi);
            loose += r.nsol > 4;
            CHECK(!r.consts && !r.idle && r.herrings <= base.herrings + 1 && r.crossings <= base.cross + 1);
            strict += r.nsol <= base.sols && r.par >= base.par_lo && r.herrings <= base.herrings &&
                      r.crossings <= base.cross;
            puzzle(&g.L, -1, g.par);
            drawn(&g.L, -1);
            made += r.exact != prev;
            prev = r.exact;
            unsigned c = lg_comp_of(&g.L);
            for (int k = 0; k < LG_COMPONENTS; k++) has[k] += c >> k & 1;
        }
        printf("logicgates: random %d, every part: %.1f ms on average, %.1f at most; %d of 60 as strict as the levels, %d with more than 4 solutions;"
               " parts in:", v, all / 60, worst, strict, loose);
        for (int k = 0; k < LG_COMPONENTS; k++) printf(" %s %d", NAME[k], has[k]);
        printf("\n");
        CHECK(made >= 50 && all / 60 < 6 && worst < 25 && strict >= 30 && loose <= 2);
        for (int k = 0; k < LG_COMPONENTS; k++) CHECK(has[k] >= (v ? 18 : 3));   /* each part often (small: now
                                                                                   * and then: three fit at once) */
    }
    /* checklists at random (some with nothing to build from: made good), by size: only the parts chosen; each part
     * chosen in most circuits when the size has room for it; a puzzle, drawn, fast */
    double worst = 0, all = 0;
    int runs = 0, loose = 0, chosen = 0, in = 0;
    for (int t = 0; t < 240; t++) {
        unsigned comp = (unsigned)rnd(1 << LG_COMPONENTS), good = lg_comp_fix(comp);
        int v = t % LG_RANDOM;
        static lg_game g;
        clock_t t0 = clock();
        CHECK(lg_random(&g, v, comp, 1000u + (uint32_t)t));
        double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
        all += ms;
        runs++;
        if (ms > worst) worst = ms;
        unsigned c = lg_comp_of(&g.L);
        if (c & ~good) printf("logicgates: checklist %#x made %#x\n", good, c);
        CHECK(!(c & ~good));                     /* only the parts chosen */
        lg_rating r;
        CHECK(lg_rate(&g.L, &r));
        CHECK(!r.consts && !r.idle && r.par >= 1 && r.nsol <= 32);
        loose += r.nsol > 4;
        puzzle(&g.L, -1, g.par);
        drawn(&g.L, -1);
        if (v == 2 && __builtin_popcount(good) <= 6) {   /* big, a few parts: they all fit */
            chosen += __builtin_popcount(good);
            in += __builtin_popcount(c & good);
        }
    }
    printf("logicgates: random checklists: %.1f ms on average, %.1f at most; %d of %d with more than 4 solutions; "
           "big circuits with a few parts chosen have %d of %d\n", all / runs, worst, loose, runs, in, chosen);
    CHECK(all / runs < 6 && worst < 25 && loose <= runs / 6 && in >= chosen * 3 / 4);
}

/* opening a level: its circuit read, its stored drawing laid out, no search: well under a millisecond here (a phone
 * is some 6 times slower: a few milliseconds); the bounded search of the die tab's circuits on the same levels */
static void timing(void)
{
    static lg_game g;
    static lg_level L;
    double worst = 0, all = 0, fworst = 0, fall = 0;
    int wl = 0;
    for (int rep = 0; rep < 3; rep++)
        for (int l = 0; l < LG_LEVELS; l++) {
            clock_t t0 = clock();
            CHECK(lg_open(&g, l));
            double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
            all += ms;
            if (ms > worst) { worst = ms; wl = l + 1; }
        }
    for (int l = 0; l < LG_LEVELS; l++) {
        char text[1024];
        CHECK(lg_parse(lg_levels[l], &L, NULL, 0) && lg_text(&L, text, sizeof text));
        clock_t t0 = clock();
        CHECK(lg_parse(text, &L, NULL, 0) && !L.stored);   /* the circuit alone: the bounded search */
        double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
        fall += ms;
        if (ms > fworst) fworst = ms;
    }
    printf("logicgates: a level opens in %.3f ms on average, %.3f at most (level %d); searched: %.2f, %.2f at most\n",
           all / (3 * LG_LEVELS), worst, wl, fall / LG_LEVELS, fworst);
    CHECK(worst < 4 && fworst < 15);
}

int main(void)
{
    truth_tables();
    propagation();
    routes();
    cuts();
    stars();
    undo_all();
    levels();
    repeats();
    drawing();
    timing();
    checklist();
    generator();
    if (fails) {
        printf("logicgates: %d failures\n", fails);
        return 1;
    }
    printf("logicgates: ok\n");
    return 0;
}
