/*
 * Falling Dominoes' rules: dominoes fall along their axis and stay when hit from the side, the trigger must fall last,
 * delay dominoes fall two steps late, stoppers stop the chain, splitters, turns, long dominoes over a gap (or stopped
 * by a piece standing in it); moves, turns in place, the trigger that never moves, undo; boards as text both ways;
 * every level: its par the search's (none in fewer moves), its stored way at par and its push checked by a push
 * written here (its own steps), no push working before a move, the chapters' pieces, no two levels alike; the puzzles
 * made at random (their par, the same from the same seed, the time they take); the hints lead to the end at par.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/dominochain.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* ---- the test's own push: knocks (square, way, reaching over, when) in a list sorted by time ------------------- */
typedef struct { int sq, way, over, at, due; } knock;

static int own_next(const dmc_state *s, int c, int d)
{
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int x = c % s->w + dx[d], y = c / s->w + dy[d];
    return x < 0 || y < 0 || x >= s->w || y >= s->h ? -1 : x + y * s->w;
}

static int own_push(const dmc_state *s, int c0, int d0)
{
    int when[DMC_CELLS];
    for (int c = 0; c < DMC_CELLS; c++) when[c] = -1;
    knock k[1024];
    int n = 0;
    k[n++] = (knock){c0, d0, 0, 0, 1};          /* due: a fall to make (a delay's, the push) rather than a hit */
    for (int i = 0; i < n && n < 1000; i++) {
        /* the earliest knock left, the falls due first at that time, then in the order they were made */
        int best = i;
        for (int j = i + 1; j < n; j++)
            if (k[j].at < k[best].at || (k[j].at == k[best].at && k[j].due > k[best].due)) best = j;
        knock h = k[best];
        memmove(k + i + 1, k + i, sizeof(knock) * (size_t)(best - i));
        k[i] = h;
        int c = h.sq, d = h.way, t = c >= 0 ? s->t[c] : DMC_GAP, o = c >= 0 ? s->o[c] : 0, out[2], no = 0;
        if (c < 0) continue;
        if (!h.due) {
            if (t < DMC_DOMINO || when[c] >= 0) {   /* nothing standing: a long domino reaches on */
                if (h.over) {
                    int b = own_next(s, c, d);
                    if (b >= 0) { k[i] = (knock){b, d, 0, h.at, 0}; i--; }
                }
                continue;
            }
            if (t == DMC_DELAY && d % 2 == o) {
                when[c] = h.at + 2;
                k[n++] = (knock){c, d, 0, h.at + 2, 1};
                continue;
            }
            if ((t == DMC_DOMINO || t == DMC_TRIGGER || t == DMC_LONG) && d % 2 == o) out[no++] = d;
            if (t == DMC_TURN) {
                int from = (d + 2) % 4;
                if (from == o) out[no++] = (o + 1) % 4;
                if (from == (o + 1) % 4) out[no++] = o;
            }
            if (t == DMC_SPLIT) { out[no++] = (d + 1) % 4; out[no++] = (d + 3) % 4; }
            if (!no) continue;
        } else {
            if (t == DMC_SPLIT) { out[no++] = (d + 1) % 4; out[no++] = (d + 3) % 4; }
            else out[no++] = d;
        }
        when[c] = h.at;
        for (int j = 0; j < no; j++) {
            int b = own_next(s, c, out[j]);
            if (b >= 0) k[n++] = (knock){b, out[j], t == DMC_LONG, h.at + 1, 0};
        }
    }
    int last = -1, trig = -1;
    for (int c = 0; c < s->w * s->h; c++) {
        int t = s->t[c];
        if (t < DMC_DOMINO || t == DMC_STOPPER) continue;
        if (when[c] < 0) return 0;
        if (t == DMC_TRIGGER) trig = when[c];
        else if (when[c] > last) last = when[c];
    }
    return trig > last;
}

static int count(const char *s, const char *any)
{
    int n = 0;
    for (; *s; s++) n += strchr(any, *s) != NULL;
    return n;
}

int main(void)
{
    static dmc_game g, t;
    dmc_state s;
    dmc_trace tr;
    char text[DMC_TEXT];

    /* a line: pushed right, all fall, the trigger last */
    CHECK(dmc_parse(&s, "hhhX") && s.w == 4 && s.h == 1);
    CHECK(dmc_run(&s, DMC_PUSH(0, 1), &tr) && tr.n == 4 && tr.trigger == 3);
    CHECK(!dmc_run(&s, DMC_PUSH(3, 3), NULL));                  /* the trigger pushed: it falls first */
    CHECK(!dmc_can_push(&s, DMC_PUSH(0, 0)));                    /* across its axis: no push */
    CHECK(dmc_solving_push(&s) == DMC_PUSH(0, 1));
    /* a domino across the push stays; turned, it falls */
    CHECK(dmc_parse(&g.m.start, "hvhX"));
    dmc_begin(&g);
    CHECK(dmc_solving_push(&g.s) < 0 && !dmc_run(&g.s, DMC_PUSH(0, 1), &tr) && tr.nwob == 1 && tr.wob[0].c == 1);
    CHECK(!dmc_move(&g, DMC_MOVE(3, 3)) && !dmc_move(&g, DMC_MOVE(3, 2)));   /* the trigger never moves */
    CHECK(dmc_move(&g, DMC_MOVE(1, 1)) && g.moves == 1 && dmc_solving_push(&g.s) == DMC_PUSH(0, 1));
    CHECK(dmc_undo(&g) && g.moves == 0 && g.s.o[1] == 0);
    CHECK(dmc_script(&g, "rba paal") == 1 && !g.done);           /* pushed the wrong way: nothing solved */
    CHECK(dmc_script(&g, "paar") && g.done && g.moves == 1);
    dmc_text(&g.s, text);
    CHECK(!strcmp(text, "hhhX"));
    /* a piece moved to free wood; not onto a gap nor a piece */
    CHECK(dmc_parse(&g.m.start, "h.hX|.##.") && (dmc_begin(&g), 1));
    CHECK(!dmc_move(&g, DMC_MOVE(0, 5)) && !dmc_move(&g, DMC_MOVE(0, 2)) && dmc_move(&g, DMC_MOVE(0, 1)));
    CHECK(g.s.t[0] == DMC_WOOD && g.s.t[1] == DMC_DOMINO && g.s.o[1] == 1);
    /* a stopper stops the chain and never falls; it is not needed to fall */
    CHECK(dmc_parse(&s, "hhSX") && !dmc_run(&s, DMC_PUSH(0, 1), &tr) && !tr.fallen[2] && !tr.fallen[3]);
    CHECK(dmc_parse(&s, "hhX.|..S."));
    CHECK(dmc_run(&s, DMC_PUSH(0, 1), NULL));
    /* a delay domino falls two steps late: here it makes the trigger last */
    CHECK(dmc_parse(&s, "###v|hhh+|###v|###Y") && dmc_run(&s, DMC_PUSH(4, 1), NULL));   /* a splitter: up and down */
    CHECK(dmc_parse(&s, "###v|###v|hhh+|###v|###Y") && !dmc_run(&s, DMC_PUSH(8, 1), NULL));   /* a tie: not last */
    CHECK(dmc_parse(&s, "###v|###v|hhh+|###e|###Y") && dmc_run(&s, DMC_PUSH(8, 1), &tr) && tr.trigger == 7);
    /* a long domino reaches over a gap; a domino standing in the gap's place is hit instead */
    CHECK(dmc_parse(&s, "hl#hX") && dmc_run(&s, DMC_PUSH(0, 1), NULL));
    CHECK(dmc_parse(&s, "hl.hX") && dmc_run(&s, DMC_PUSH(0, 1), NULL));
    CHECK(dmc_parse(&s, "hlvhX") && !dmc_run(&s, DMC_PUSH(0, 1), NULL));
    /* a turn: right then down; the wrong corner stays */
    CHECK(dmc_parse(&s, "h3|#v|#Y") && dmc_run(&s, DMC_PUSH(0, 1), NULL));
    CHECK(dmc_parse(&s, "h4|#v|#Y") && !dmc_run(&s, DMC_PUSH(0, 1), NULL));
    CHECK(!dmc_parse(&s, "hhh") && !dmc_parse(&s, "hXX") && !dmc_parse(&s, "hX|h") && !dmc_parse(&s, "hzX"));
    /* the own push agrees with the rules on these */
    {
        static const char *const B[] = {"hhhX", "###v|###v|hhh+|###e|###Y", "hl#hX", "h3|#v|#Y", "###v|###v|hhh+|###v|###Y",
                                        "hhSX", "hlvhX"};
        for (int i = 0; i < 7; i++) {
            CHECK(dmc_parse(&s, B[i]));
            int c = 0;
            while (s.t[c] != DMC_DOMINO || !s.o[c]) c++;   /* the first domino across, pushed right */
            CHECK(own_push(&s, c, 1) == dmc_run(&s, DMC_PUSH(c, 1), NULL));
        }
    }

    /* every level: read back, its par the search's, its way at par solves (the own push too), no push before a move */
    int per = DMC_LEVELS / DMC_CHAPTERS;
    double slow = 0;
    for (int lv = 0; lv < DMC_LEVELS; lv++) {
        const dmc_level_t *L = &dmc_levels[lv];
        CHECK(dmc_level(&g, lv));
        dmc_text(&g.s, text);
        CHECK(!strcmp(text, L->board));
        CHECK(g.par == L->par && g.par >= 1 && g.m.nsol == g.par && g.m.push >= 0);
        CHECK(dmc_solving_push(&g.s) < 0);
        if (lv >= 3) CHECK(!dmc_obvious(&g.s, g.par));   /* from level 4 on: the obvious play fails */
        clock_t t0 = clock();
        dmc_info in;
        int ok = dmc_search(&g.s, g.par, 2000000L, &in);
        double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        if (ms > slow) slow = ms;
        CHECK(ok && in.par == g.par);
        if (!ok || in.par != g.par) printf("dominochain: level %d, the search %d, par %d\n", lv + 1, in.par, g.par);
        t = g;
        for (int k = 0; k < g.m.nsol; k++) CHECK(dmc_move(&t, g.m.sol[k]));
        CHECK(dmc_run(&t.s, g.m.push, NULL) && own_push(&t.s, g.m.push / 4, g.m.push % 4));
        t = g;
        CHECK(dmc_script(&t, "=") && t.done && t.moves == g.par);
        int ch = lv / per;
        if (ch == 0) CHECK(count(L->board, "S") > 0);
        if (ch == 1) CHECK(count(L->board, "+") > 0);
        if (ch == 2) CHECK(count(L->board, "de") > 0);
        if (ch == 3) CHECK(count(L->board, "li") > 0);
        if (lv && lv % per) CHECK(L->par + 1 >= dmc_levels[lv - 1].par);   /* a smooth curve */
        for (int k = 0; k < lv; k++) CHECK(strcmp(dmc_levels[k].board, L->board) != 0);
    }
    CHECK(!dmc_level(&g, DMC_LEVELS) && !dmc_level(&g, -1));
    printf("dominochain: every level at its par, the slowest search %.0f ms\n", slow);

    /* the puzzles made at random: solvable in their par, the same from the same seed, the time it takes */
    for (int v = 0; v < DMC_VARIANTS; v++) {
        double worst = 0;
        int lo = 99, hi = 0, plain = 0;
        for (uint32_t seed = 1; seed <= 12; seed++) {
            clock_t t0 = clock();
            dmc_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            if (g.par < lo) lo = g.par;
            if (g.par > hi) hi = g.par;
            CHECK(g.par >= 1 && dmc_solving_push(&g.s) < 0);
            plain += dmc_obvious(&g.s, g.par);
            t = g;
            CHECK(dmc_script(&t, "=") && t.done && t.moves == g.par);
            dmc_random(&t, v, seed);
            CHECK(!memcmp(&t.m, &g.m, sizeof g.m));
        }
        CHECK(worst < 1500 && plain <= 2);
        printf("dominochain: random variant %d, pars %d .. %d, %d of 12 obvious, the slowest %.0f ms\n", v, lo, hi,
               plain, worst);
    }

    /* hints from a wrong first move: they take it back or work round it, and reach the end */
    int ok = 1;
    for (int lv = 0; lv < DMC_LEVELS; lv += 7) {
        dmc_level(&g, lv);
        for (int c = 0; c < g.s.w * g.s.h && !g.moves; c++)
            if (dmc_movable(&g.s, c) && g.s.t[c] != DMC_TURN) dmc_move(&g, DMC_MOVE(c, c));
        dmc_script(&g, "=");
        if (!g.done) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("dominochain: %d failures\n", fails); return 1; }
    printf("dominochain: all passed\n");
    return 0;
}
