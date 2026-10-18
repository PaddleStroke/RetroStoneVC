/*
 * Defuse the Bomb's rules: every level read and drawn, safe at the start and not won, its par the stored one (the
 * solver's), won by "=" and by following the hint, a trap and an order that matters in each from the fourth on, most
 * controls reaching two lamps; the values (a cut, a flip-flop turning over, a capacitor's charge and pulse, the board
 * timer), the ticks, undo and restart, a blown bomb that takes no undo; random play solved, the same for the same
 * seed, and quick.
 * The circuit's rules are Logic Gates' (the build links a game's own rules only: they are taken in here).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/defuse.h"
#include "games/logicgates_rules.c"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                                \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static df_game G;

static int follow_hints(df_game *g, int cap)
{
    df_move m;
    int n = 0;
    for (int k = 0; k < cap && !g->over && df_hint(g, &m); k++) {
        g->hinted = 1;
        if (m.kind == DF_MWAIT) df_pass(g);
        else { df_play(g, m); n++; }
    }
    return n;
}

static void levels(void)
{
    int seen[DF_CHAPTERS][4] = {{0}};
    for (int i = 0; i < DF_LEVELS; i++) {
        char err[96];
        static df_level D;
        if (!df_parse(df_levels[i], &D, err, sizeof err)) { fprintf(stderr, "level %d: %s\n", i + 1, err); fails++; continue; }
        CHECK(D.L.stored);
        CHECK(df_open(&G, i));
        CHECK(!G.v.det && !G.v.win);
        int par = df_solve(&G.D, &G.s, NULL, NULL);
        if (par != df_pars[i] || par <= 0) { fprintf(stderr, "level %d: par %d, stored %d\n", i + 1, par, df_pars[i]); fails++; }
        df_rating r;
        CHECK(df_rate(&G.D, &G.s, &r) == 1);
        if (i >= 3) {
            CHECK(r.lose_first + r.dead_first > 0);   /* a trap */
            CHECK(r.orders >= 2 && r.ways < r.orders);   /* an order that matters */
            CHECK(!r.greedy_ok);
            CHECK(r.shared >= 60);
        }
        CHECK(r.lose_first + r.dead_first < r.moves_first);
        int n = follow_hints(&G, 200);
        CHECK(G.over == DF_WON && n == par && df_stars(&G) == 2);
        CHECK(df_open(&G, i) && df_script(&G, "=") && G.over == DF_WON && G.nh == par && df_stars(&G) == 3);
        int c = df_chapter(i);
        if (G.D.ncut) seen[c][0] = 1;
        if (G.D.nff) seen[c][1] = 1;
        if (G.D.ncap) seen[c][2] = 1;
        if (G.D.timer >= 0) seen[c][3] = 1;
    }
    CHECK(!seen[0][0] && !seen[0][1] && !seen[0][2] && !seen[0][3]);   /* chapter 1: switches and gates only */
    CHECK(seen[1][0]);                                                  /* 2: cables */
    CHECK(seen[2][1]);                                                  /* 3: flip-flops */
    CHECK(seen[3][2]);                                                  /* 4: capacitors */
    CHECK(seen[4][3]);                                                  /* 5: the board timer */
}

static void values(void)
{
    static df_level D;
    /* POWER = battery OR s1; DISARM = s2; DETONATE = s1 AND s2: s2 alone wins, s1 then s2 blows */
    CHECK(df_parse("Pa3 ~ s@a1 s@b1 K@c1 OR@a2(c1,a1) AND@b2(a1,b1) L@a3(a2) L@b3(b1) A@c3(b2)", &D, NULL, 0));
    df_load(&G, &D, -1, 1);
    CHECK(G.v.powered && !G.v.disarm && !G.v.det);
    CHECK(df_script(&G, "s2") && G.over == DF_WON && df_stars(&G) == 3);
    df_restart(&G);
    CHECK(df_play(&G, (df_move){DF_MFLIP, 0}) == DF_SAFE && df_play(&G, (df_move){DF_MFLIP, 1}) == DF_LOST);
    CHECK(!df_undo(&G));                        /* blown: no undo */
    df_restart(&G);
    CHECK(df_script(&G, "s1 u") && G.nh == 0 && !G.over);
    /* a cable on the battery: cut, POWER rests on s1 */
    CHECK(df_parse("Pa3 ~ s@a1 s@b1 K@c1 OR@a2(c1x,a1) AND@b2(a1,b1) L@a3(a2) L@b3(b1) A@c3(b2)", &D, NULL, 0));
    df_load(&G, &D, -1, 1);
    CHECK(df_script(&G, "c1") && G.over == DF_WON && !G.v.powered);
    df_restart(&G);
    CHECK(df_script(&G, "s1 c1") && !G.over && G.v.powered);
    CHECK(df_play(&G, (df_move){DF_MCUT, 0}) < 0);   /* cut already */
    /* a flip-flop on s1 into DISARM: s1 up turns it to 1 (won); at 1, s1 up turns it back to 0 */
    CHECK(df_parse("Pa3 Fb2:0 ~ s@a1 s@b1 K@c1 OR@a2(c1,b1) NOT@b2(a1) L@a3(a2) L@b3(b2) A@c3(b1)", &D, NULL, 0));
    CHECK(D.nff == 1 && D.role[D.ff[0]] == DF_FLIP);
    df_load(&G, &D, -1, 1);
    CHECK(df_play(&G, (df_move){DF_MFLIP, 0}) == DF_WON);
    df_state st = G.start;
    st.ff = 1;
    df_values v;
    CHECK(df_move_do(&G.D, &st, (df_move){DF_MFLIP, 0}, NULL) == DF_SAFE && st.ff == 0);
    CHECK(df_move_do(&G.D, &st, (df_move){DF_MFLIP, 0}, NULL) == DF_SAFE && st.ff == 0);   /* falling: nothing */
    CHECK(df_move_do(&G.D, &st, (df_move){DF_MFLIP, 0}, NULL) == DF_WON && st.ff == 1);
    df_eval(&G.D, &st, NULL, 0, &v);
    CHECK(v.disarm);
    /* a capacitor of 3 fed by s1 into DETONATE: left charging, it blows on the third tick; s1 off empties it */
    CHECK(df_parse("Pa4 Cb3:3 ~ s@a1 K@b1 s@c1 AND@a2(a1,b1) NOT@b3(a2) OR@c2(b1,c1) L@a4(c2) L@b4(c1) A@c4(b3)", &D,
                   NULL, 0));
    df_load(&G, &D, -1, 1);
    CHECK(G.D.ncap == 1);
    CHECK(df_play(&G, (df_move){DF_MFLIP, 0}) == DF_SAFE);
    CHECK(df_pass(&G) == DF_SAFE && G.s.ch[0] == 1 && df_pass(&G) == DF_SAFE && df_pass(&G) == DF_LOST);
    df_restart(&G);
    CHECK(df_play(&G, (df_move){DF_MFLIP, 0}) == DF_SAFE && df_pass(&G) == DF_SAFE && df_pass(&G) == DF_SAFE);
    CHECK(df_play(&G, (df_move){DF_MFLIP, 0}) == DF_SAFE && df_pass(&G) == DF_SAFE && G.s.ch[0] == 0);
    df_move m;
    CHECK(df_hint(&G, &m) && m.kind == DF_MFLIP && m.idx == 1);
    /* the board timer of 2 into DISARM: waiting wins, the hint says wait */
    CHECK(df_parse("Pa3 Zb1:2 ~ s@a1 k@b1 K@c1 OR@a2(c1,a1) L@a3(a2) L@b3(b1) A@c3(a1)", &D, NULL, 0));
    df_load(&G, &D, -1, 0);
    CHECK(df_hint(&G, &m) && m.kind == DF_MWAIT);
    CHECK(df_pass(&G) == DF_SAFE && df_pass(&G) == DF_WON);
}

static void random_play(void)
{
    for (int z = 0; z < DF_RANDOM; z++) {
        double worst = 0, sum = 0;
        int n = 12, made = 0;
        for (int s = 1; s <= n; s++) {
            clock_t t = clock();
            int ok = df_random(&G, z, (uint32_t)s * 7777u);
            double ms = (clock() - t) * 1000.0 / CLOCKS_PER_SEC;
            sum += ms;
            if (ms > worst) worst = ms;
            if (!ok) continue;
            made++;
            CHECK(!G.v.det && !G.v.win && G.par > 0);
            CHECK(df_solve(&G.D, &G.s, NULL, NULL) == G.par);
            CHECK(df_script(&G, "=") && G.over == DF_WON);
            static df_game H;
            CHECK(df_random(&H, z, (uint32_t)s * 7777u) && H.start.sw == G.start.sw && H.D.L.nn == G.D.L.nn);
        }
        CHECK(made == n);
        fprintf(stderr, "defuse: random %d: %.1f ms on average, %.1f at most\n", z, sum / n, worst);
        CHECK(worst < 2500);
    }
}

int main(void)
{
    values();
    levels();
    random_play();
    if (fails) {
        fprintf(stderr, "defuse: %d failures\n", fails);
        return 1;
    }
    fprintf(stderr, "defuse: ok\n");
    return 0;
}
