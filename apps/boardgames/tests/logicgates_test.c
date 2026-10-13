/*
 * Logic Gates' rules: the gates' truth tables, the values running through a circuit, the route switches and the cut
 * wires, locked switches, undo and restart, the hint, the notation; every one of the 150 levels read, drawn apart,
 * solvable, its par the stored one (the solver's, and the hint followed from the start, and "=" played), never
 * solved from the start nor by any single move, every gate needed; random play then solved; no two levels the
 * same circuit; the generator the same for the same seed, its random puzzles as good as the levels and quick.
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
    int counts[LG_CHAPTERS][3] = {{0}}, sw_lo[LG_CHAPTERS], sw_hi[LG_CHAPTERS] = {0};
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
        int c = l / LG_PER_CHAPTER;
        counts[c][0] += L.nrt > 0;
        counts[c][1] += L.ncut > 0;
        for (int i = 0; i < L.nn; i++) counts[c][2] += L.n[i].nin == 3;
        if (L.nsw < sw_lo[c]) sw_lo[c] = L.nsw;
        if (L.nsw > sw_hi[c]) sw_hi[c] = L.nsw;
        CHECK(L.cols <= LG_COLS && L.rows <= LG_ROWS);
    }
    /* the chapters' ideas: no route switch, fuse nor three inputs before their chapter; route switches in all of the
     * third, fuses in all of the fourth */
    for (int c = 0; c < 2; c++) CHECK(!counts[c][0] && !counts[c][1] && !counts[c][2]);
    CHECK(counts[2][0] == LG_PER_CHAPTER && !counts[2][1] && !counts[2][2]);
    CHECK(counts[3][1] == LG_PER_CHAPTER && !counts[3][2]);
    CHECK(counts[4][2] > 0 && counts[5][0] > 0 && counts[5][1] > 0);
    /* the switches grow: 2 to 3, up to 5, up to 6, then up to 8 */
    static const int most[LG_CHAPTERS] = {3, 5, 6, 8, 8, 8};
    for (int c = 0; c < LG_CHAPTERS; c++) {
        printf("logicgates: chapter %d, %d to %d switches\n", c + 1, sw_lo[c], sw_hi[c]);
        CHECK(sw_hi[c] == most[c]);
    }
    CHECK(sw_lo[0] == 2 && sw_lo[5] >= 7);
    /* the hand-made first levels: AND, NOT, OR, XOR */
    for (int i = 0; i < 4; i++) CHECK(lg_pars[i] == 2);
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

/* a level's drawing: rows by depth, the switches evenly spaced at the bottom, the lamps on top; no two parts
 * overlapping, no wire through a part; each wire rising from its source, square bends, rising into its pin; few
 * crossings, each one hopped */
static int drawn(const lg_level *L, int level)
{
    char err[128];
    CHECK(lg_check(L, err, sizeof err));
    int top = L->layers - 1, nsw = 0;
    float sx[LG_NODES];
    for (int i = 0; i < L->nn; i++) {
        const lg_node *N = &L->n[i];
        int lamp = N->kind == LG_LAMP || N->kind == LG_ALARM;
        CHECK((N->kind == LG_SWITCH) == (N->layer == 0) && lamp == (N->layer == top));
        CHECK(N->x >= 0.5f - 1e-3f && N->x <= L->width - 0.5f + 1e-3f && N->y + LG_NH <= L->height + 1e-3f);
        for (int s = 0; s < N->nin; s++) CHECK(L->n[L->w[N->in[s]].from].layer < N->layer);
        if (N->kind == LG_SWITCH) sx[nsw++] = N->x;
        for (int j = i + 1; j < L->nn; j++) {   /* no two parts overlap */
            const lg_node *M = &L->n[j];
            int apart = fabsf(N->x - M->x) >= 2 * LG_HALF + 0.1f || N->y + LG_NH <= M->y || M->y + LG_NH <= N->y;
            if (!apart) printf("level %d: parts %d and %d overlap\n", level + 1, i, j);
            CHECK(apart);
        }
    }
    for (int i = 1; i < nsw; i++)               /* the switches a whole number of columns apart */
        for (int j = 0; j < i; j++) {
            float d = fabsf(sx[i] - sx[j]);
            CHECK(d > 0.5f && fabsf(d - floorf(d + 0.5f)) < 1e-3f);
        }
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
                int through = fminf(x0, x1) < N->x + LG_HALF && fmaxf(x0, x1) > N->x - LG_HALF &&
                              fminf(y0, y1) < N->y + LG_NH && fmaxf(y0, y1) > N->y;
                if (through) printf("level %d: wire %d through part %d\n", level + 1, w, k);
                CHECK(!through);
            }
        }
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

/* the generator: the same level for the same seed; every random puzzle of the die tab a puzzle as the levels are,
 * made fast */
static void generator(void)
{
    for (int v = 0; v < LG_RANDOM; v++) {
        lg_spec sp;
        lg_random_spec(v, &sp);
        double worst = 0, all = 0;
        int made = 0;
        uint64_t prev = 0;
        for (uint32_t seed = 1; seed <= 60; seed++) {
            char a[1024], b[1024];
            lg_rating ra, rb;
            int ka = lg_make(seed, &sp, 400, a, sizeof a, &ra), kb = lg_make(seed, &sp, 400, b, sizeof b, &rb);
            CHECK(ka == kb && (!ka || !strcmp(a, b)));
            clock_t t0 = clock();
            static lg_game g;
            CHECK(lg_random(&g, v, seed));
            double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
            all += ms;
            if (ms > worst) worst = ms;
            CHECK(g.level == -1);
            lg_rating r;
            CHECK(lg_rate(&g.L, &r));
            CHECK(r.sw >= sp.sw_lo && r.sw <= sp.sw_hi && r.nsol <= sp.sols);
            CHECK(r.par >= sp.par_lo && r.par <= sp.par_hi);
            CHECK(!r.consts && !r.idle && r.herrings <= sp.herrings && r.crossings <= sp.cross);
            puzzle(&g.L, -1, g.par);
            drawn(&g.L, -1);
            made += r.exact != prev;
            prev = r.exact;
        }
        printf("logicgates: random %d: %.1f ms on average, %.1f at most\n", v, all / 60, worst);
        CHECK(made >= 50 && all / 60 < 40);
    }
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
    generator();
    if (fails) {
        printf("logicgates: %d failures\n", fails);
        return 1;
    }
    printf("logicgates: ok\n");
    return 0;
}
