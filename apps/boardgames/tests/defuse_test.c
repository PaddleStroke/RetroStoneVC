/*
 * Defuse the Bomb's rules: every one of the levels read and drawn, safe at the start and not won, its par the stored
 * one (the solver's), won by "=" and by following the hint from the start, at least one first move blowing (and not
 * all of them), every chapter's idea in it; the values (a cut, a latch, a press, the power), undo and restart, a
 * blown bomb that takes no undo; random play solved, the same for the same seed, and quick.
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

static void levels(void)
{
    int per_ch[DF_CHAPTERS][4] = {{0}};         /* NOT, cables, power socket, XOR... per chapter */
    for (int i = 0; i < DF_LEVELS; i++) {
        char err[96];
        static df_level D;
        int ok = df_parse(df_levels[i], &D, err, sizeof err);
        if (!ok) { fprintf(stderr, "level %d: %s\n", i + 1, err); fails++; continue; }
        CHECK(D.L.stored);
        CHECK(df_open(&G, i));
        CHECK(!G.v.det && !G.v.win);
        int par = df_solve(&G.D, G.state, NULL);
        if (par != df_pars[i] || par <= 0) { fprintf(stderr, "level %d: par %d, stored %d\n", i + 1, par, df_pars[i]); fails++; }
        df_move ms[LG_NODES + LG_WIRES];
        int nm = df_moves(&G.D, G.state, ms), lose = 0;
        for (int k = 0; k < nm; k++) {
            uint32_t ns;
            lose += df_step(&G.D, G.state, ms[k], &ns, NULL) == DF_LOST;
        }
        CHECK(lose > 0 && lose < nm);
        /* the hint followed from the start: par moves, won, 2 stars at most */
        int n = 0;
        df_move m;
        while (!G.over && n < 40 && df_hint(&G, &m)) {
            G.hinted = 1;
            df_play(&G, m);
            n++;
        }
        CHECK(G.over == DF_WON && n == par);
        CHECK(df_stars(&G) == 2);
        CHECK(df_open(&G, i) && df_script(&G, "=") && G.over == DF_WON && G.nh == par && df_stars(&G) == 3);
        int c = df_chapter(i);
        for (int k = 0; k < G.D.L.nn; k++) {
            if (G.D.L.n[k].kind == LG_NOT && G.D.role[k] == DF_GATE) per_ch[c][0] = 1;
            if (G.D.role[k] == DF_PLUG) per_ch[c][2] = 1;
            if (G.D.role[k] == DF_BUTTON) per_ch[c][3] = 1;
        }
        if (G.D.ncut) per_ch[c][1] = 1;
        CHECK(G.D.nled <= (c < 3 ? 2 : c < 5 ? 3 : 4));
    }
    CHECK(!per_ch[0][1] && !per_ch[0][2] && !per_ch[0][3]);   /* chapter 1: switches and gates only */
    CHECK(per_ch[1][1] && !per_ch[1][2]);                      /* 2: cables */
    CHECK(per_ch[2][2]);                                       /* 3: the power socket */
    CHECK(per_ch[4][3]);                                       /* 5: buttons */
    CHECK(df_chapter(DF_FIRST_CHAPTER - 1) == 0 && df_chapter(DF_FIRST_CHAPTER) == 1 &&
          df_chapter(DF_LEVELS - 1) == DF_CHAPTERS - 1);
}

static void values(void)
{
    static df_level D;
    /* a: switch 1 into LED A; b: switch 2 into DETONATE */
    CHECK(df_parse("~ s@a1 s@b1 L@a2(a1) A@b2(b1)", &D, NULL, 0));
    df_load(&G, &D, 0, -1, 1);
    CHECK(df_script(&G, "s1") && G.over == DF_WON && df_stars(&G) == 3);
    df_restart(&G);
    CHECK(df_play(&G, (df_move){DF_MFLIP, 1}) == DF_LOST && G.over == DF_LOST);
    CHECK(!df_undo(&G));                        /* blown: no undo, a restart */
    CHECK(df_play(&G, (df_move){DF_MFLIP, 0}) < 0);
    df_restart(&G);
    CHECK(!G.over && G.nh == 0);
    /* the order: A = s1 AND s2, DETONATE = s1 AND NOT s2: s2 first */
    CHECK(df_parse("~ s@a1 s@b1 NOT@b2(b1) AND@a3(a1,b1) AND@b3(a1,b2) L@a4(a3) A@b4(b3)", &D, NULL, 0));
    df_load(&G, &D, 0, -1, 2);
    CHECK(df_play(&G, (df_move){DF_MFLIP, 0}) == DF_LOST);
    df_restart(&G);
    df_move m;
    CHECK(df_hint(&G, &m) && m.kind == DF_MFLIP && m.idx == 1);
    CHECK(df_script(&G, "s2") && !G.over && df_script(&G, "u") && G.nh == 0 && df_script(&G, "s2 s1") &&
          G.over == DF_WON);
    /* a cable: cut for good; the power socket fed by the battery through it: cut, the bomb is dead */
    CHECK(df_parse("Pc2 ~ s@a1 K@b1 L@a2(a1) A@b2(a1) L@c2(b1x)", &D, NULL, 0));
    CHECK(D.plug >= 0 && D.battery >= 0 && D.ncut == 1 && D.nled == 1);
    df_load(&G, &D, 0, -1, 1);
    CHECK(G.v.powered);
    CHECK(df_script(&G, "c1") && G.over == DF_WON && !G.v.powered && !G.v.lit);
    df_restart(&G);
    CHECK(df_play(&G, (df_move){DF_MFLIP, 0}) == DF_LOST);   /* A and DETONATE on one switch */
    /* a button into a latch: pressed once, A lit for good; DETONATE on the button itself with switch 1 on */
    CHECK(df_parse("Ba1 Mb2 ~ s@a1 s@b1 NOT@b2(a1) AND@a2(a1,b1) L@a3(b2) A@b3(a2)", &D, NULL, 0));
    CHECK(D.role[D.sw[0]] == DF_BUTTON && D.nlatch == 1);
    df_load(&G, &D, 0, -1, 1);
    CHECK(df_script(&G, "b1") && G.over == DF_WON);
    df_restart(&G);
    CHECK(df_script(&G, "s2") && df_play(&G, (df_move){DF_MPRESS, 0}) == DF_LOST);
    CHECK(df_play(&G, (df_move){DF_MFLIP, 0}) < 0);
}

static void random_play(void)
{
    for (int z = 0; z < DF_RANDOM; z++) {
        double worst = 0, sum = 0;
        int n = 30;
        for (int s = 1; s <= n; s++) {
            clock_t t = clock();
            int ok = df_random(&G, z, (uint32_t)s * 7777u);
            double ms = (clock() - t) * 1000.0 / CLOCKS_PER_SEC;
            sum += ms;
            if (ms > worst) worst = ms;
            CHECK(ok);
            if (!ok) continue;
            CHECK(!G.v.det && !G.v.win && G.par > 0);
            CHECK(G.D.nled >= 1 && G.D.nled <= 4);
            CHECK(df_solve(&G.D, G.state, NULL) == G.par);
            CHECK(df_script(&G, "=") && G.over == DF_WON);
            static df_game H;
            CHECK(df_random(&H, z, (uint32_t)s * 7777u) && H.start == G.start && H.D.L.nn == G.D.L.nn);
        }
        fprintf(stderr, "defuse: random %d: %.1f ms on average, %.1f at most\n", z, sum / n, worst);
        CHECK(worst < 900);
    }
}

int main(void)
{
    levels();
    values();
    random_play();
    if (fails) {
        fprintf(stderr, "defuse: %d failures\n", fails);
        return 1;
    }
    fprintf(stderr, "defuse: ok\n");
    return 0;
}
