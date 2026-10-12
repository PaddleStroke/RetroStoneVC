/*
 * Logic Gates' rules: the gates' truth tables, the values running through a circuit, the route switches and the cut
 * wires, locked switches, undo and restart, the hint, the notation; every one of the 150 levels read, drawn apart,
 * solvable, its par the stored one (the solver's, and the hint followed from the start, and "=" played), never
 * solved from the start nor by any single move; random play then solved; the generator the same for the same seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void levels(void)
{
    int counts[LG_CHAPTERS][3] = {{0}};
    for (int l = 0; l < LG_LEVELS; l++) {
        static lg_level L;
        char err[128];
        if (!lg_parse(lg_levels[l], &L, err, sizeof err)) {
            printf("level %d: %s\n", l + 1, err);
            fails++;
            continue;
        }
        lg_load(&G, &L, l, lg_pars[l]);
        CHECK(!lg_solved(&G));
        lg_answer a;
        lg_solve(&L, G.val, G.cut, &a);
        if (a.par != lg_pars[l]) printf("level %d: par %d, stored %d\n", l + 1, a.par, lg_pars[l]);
        CHECK(a.par == lg_pars[l] && a.par >= 1);
        /* not every single move solves it (unless there is only one to make) */
        int single = 0;
        for (int v = 0; v < L.nvar; v++) {
            lg_game t = G;
            lg_move m = {L.var_kind[v], L.var_idx[v]};
            if (lg_play(&t, m) && lg_solved(&t)) single++;
        }
        CHECK(L.nvar == 1 || single < L.nvar);
        /* the hint followed from the start: solved at par */
        lg_game h = G;
        lg_move m;
        int steps = 0;
        while (!lg_solved(&h) && steps < 40 && lg_hint(&h, &m) && lg_play(&h, m)) steps++;
        CHECK(lg_solved(&h) && steps == lg_pars[l]);
        /* the notation: open it, play the best solution */
        lg_game s;
        CHECK(lg_open(&s, l) && lg_script(&s, "=") && lg_solved(&s) && s.nh == lg_pars[l] && lg_stars(&s) == 3);
        /* random moves, then solved from there (undoing the cuts that block it) */
        for (int k = 0; k < 6; k++) {
            lg_game r = G;
            for (int j = 0; j < 8 && !lg_solved(&r); j++) {
                lg_move rm = {L.var_kind[rnd(L.nvar)], 0};
                rm.idx = (uint8_t)rnd(rm.kind == LG_MSWITCH ? L.nsw : rm.kind == LG_MROUTE ? L.nrt : L.ncut);
                lg_play(&r, rm);
            }
            while (!lg_solved(&r) && !lg_hint(&r, &m)) CHECK(lg_undo(&r));
            if (!lg_solved(&r)) CHECK(lg_script(&r, "=") && lg_solved(&r));
        }
        int c = l / LG_PER_CHAPTER;
        counts[c][0] += L.nrt > 0;
        counts[c][1] += L.ncut > 0;
        for (int i = 0; i < L.nn; i++) counts[c][2] += L.n[i].nin == 3;
        CHECK(L.cols <= LG_COLS && L.rows <= LG_ROWS);
    }
    /* the chapters' ideas: no route switch, fuse nor three inputs before their chapter; route switches in all of the
     * third, fuses in all of the fourth */
    for (int c = 0; c < 2; c++) CHECK(!counts[c][0] && !counts[c][1] && !counts[c][2]);
    CHECK(counts[2][0] == LG_PER_CHAPTER && !counts[2][1] && !counts[2][2]);
    CHECK(counts[3][1] == LG_PER_CHAPTER && !counts[3][2]);
    CHECK(counts[4][2] > 0 && counts[5][0] > 0 && counts[5][1] > 0);
    /* the hand-made levels: their pars */
    static const int first[] = {1, 2, 1, 2, 1, 1};
    for (int i = 0; i < 6; i++) CHECK(lg_pars[i] == first[i]);
}

static void generator(void)
{
    lg_params p = {.cols = 6, .rows = 7, .sw = 5, .kinds = 0x7e, .routes = 1, .fuses = 2, .locked = 1, .lamps = 2,
                   .alarm = 1, .fan = 30, .three = 20, .gates = 4};
    int made = 0, differ = 0;
    char prev[2048] = "";
    for (uint32_t seed = 1; seed <= 300; seed++) {
        char a[2048], b[2048];
        int ka = lg_generate(seed, &p, a, sizeof a), kb = lg_generate(seed, &p, b, sizeof b);
        CHECK(ka == kb);
        if (!ka) continue;
        CHECK(!strcmp(a, b));
        made++;
        differ += strcmp(a, prev) != 0;
        strcpy(prev, a);
        static lg_level L;
        char err[128];
        if (lg_parse(a, &L, err, sizeof err)) {   /* read back: most draw apart; the parts all used */
            int lamps = 0;
            for (int i = 0; i < L.nn; i++) lamps += L.n[i].kind == LG_LAMP;
            CHECK(lamps >= 1 && L.rows <= p.rows && L.cols <= p.cols);
        }
    }
    CHECK(made > 100 && differ == made);
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
    generator();
    if (fails) {
        printf("logicgates: %d failures\n", fails);
        return 1;
    }
    printf("logicgates: ok\n");
    return 0;
}
