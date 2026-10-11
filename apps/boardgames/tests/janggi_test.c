/*
 * Janggi's rules test (src/games/janggi_rules.c, no SDL): each piece's moves (the palaces' diagonals for the general,
 * the guards, the chariot, the cannon and the soldiers; the horse's leg and the elephant's two blocking points; the
 * cannon's screen, never a cannon), the generals facing, check and its answers, the pass (never in check), mate, the
 * position with no move but a pass, the repetition draw (four passes too), undo.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/janggi.h"

static int fails, checks;
static jpos P;
static jmove M[JG_MAX_MOVES];
static int N;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* a position (player `side` to move) and its legal moves in M[0..N) */
static void pos(int side, const char *pieces0, const char *pieces1)
{
    if (!jg_setup(&P, side, pieces0, pieces1)) printf("bad setup %s / %s\n", pieces0, pieces1);
    N = jg_legal(&P, M);
}

static int has(const char *move) { return jg_parse(M, N, move) >= 0; }

/* the moves of the piece on `from`, written in s: exactly these ("b0a2 b0c2") */
static int only(const char *from, const char *moves)
{
    int f = jg_point(from), count = 0, want = 0;
    for (int i = 0; i < N; i++) count += M[i].from == f;
    for (const char *s = moves; *s;) {
        if (*s == ' ') { s++; continue; }
        if (!has(s)) return 0;
        want++;
        s += 4;
    }
    return count == want;
}

static jgame G;

/* plays the moves ("pass" a pass) on a fresh game; 0: one of them is not legal */
static int game(const char *moves)
{
    jg_new(&G);
    for (const char *s = moves; *s;) {
        if (*s == ' ') { s++; continue; }
        if (!strncmp(s, "pass", 4)) {
            if (!jg_can_pass(jg_pos(&G))) return 0;
            jg_play(&G, (jmove){JG_PASS, JG_PASS, 0});
            s += 4;
            continue;
        }
        jmove m[JG_MAX_MOVES];
        int k = jg_parse(m, jg_legal(jg_pos(&G), m), s);
        if (k < 0) return 0;
        jg_play(&G, m[k]);
        s += 4;
    }
    return 1;
}

int main(void)
{
    /* the start: the generals in their palaces' middles, Cho's 31 first moves (the cannons have no screen yet) */
    jg_start(&P);
    N = jg_legal(&P, M);
    CHECK(jg_general(&P, 0) == jg_point("e1") && jg_general(&P, 1) == jg_point("e8"));
    CHECK(P.sq[jg_point("b0")] == JG_PC(0, JG_HORSE) && P.sq[jg_point("c9")] == JG_PC(1, JG_ELEPHANT));
    CHECK(N == 31 && jg_can_pass(&P));
    CHECK(only("b2", "") && only("c0", "") && only("e1", "e1d1 e1f1 e1e0 e1e2 e1d2 e1f2"));

    /* the general and the guards: one point along the palace's lines, its diagonals only through the middle */
    pos(0, "Kd0 Ae1", "Kf9");
    CHECK(only("d0", "d0d1 d0e0") && only("e1", "e1d1 e1f1 e1e0 e1e2 e1d2 e1f2 e1f0"));
    pos(0, "Kf0 Ad1", "Kd9");
    CHECK(only("d1", "d1d0 d1d2 d1e1"));        /* no diagonal from the side of the palace */

    /* the horse: one point straight, then one diagonally outward; blocked by a piece on its leg */
    pos(0, "Ke1 He4 Pe5", "Kf8");
    CHECK(only("e4", "e4g5 e4g3 e4c5 e4c3 e4d2 e4f2"));

    /* the elephant: one point straight, then two diagonally; blocked on either point it passes */
    pos(0, "Ke1 Ee4", "Kf8");
    CHECK(only("e4", "e4h6 e4h2 e4b6 e4b2 e4g7 e4c7 e4g1 e4c1"));
    pos(0, "Ke1 Ee4", "Kf8 Pg5");               /* the second point (g5) on the way to h6 */
    CHECK(!has("e4h6") && has("e4h2") && has("e4g7") && N > 0);
    pos(0, "Ke1 Ee4 Pf4", "Kf8");               /* the first point (f4): both h6 and h2 */
    CHECK(!has("e4h6") && !has("e4h2") && has("e4g7") && has("e4g1"));

    /* the chariot: along the lines, and along a palace's diagonal through its middle */
    pos(0, "Ke0 Rd2", "Kf8");
    CHECK(has("d2e1") && has("d2f0") && has("d2d9") && has("d2a2") && !has("d2c1") && !has("d2c3"));
    pos(0, "Ke0 Rd2 Pe1", "Kf8");
    CHECK(!has("d2e1") && !has("d2f0"));
    pos(0, "Kd0 Rd8", "Kf9");                   /* d8: the side of Han's palace, no diagonal */
    CHECK(!has("d8e9") && !has("d8e7") && has("d8e8"));
    pos(1, "Kd0 Rf9", "Ke8");                   /* check along the diagonal */
    CHECK(jg_in_check(&P, 1));

    /* the cannon: moves and takes over exactly one piece, never a cannon, never takes a cannon */
    pos(0, "Ke1 Ca0 Pa2", "Kf8 Ra5");
    CHECK(only("a0", "a0a3 a0a4 a0a5"));        /* not a1: no move without a screen */
    pos(0, "Ke1 Ca0 Ca2", "Kf8 Ra5");
    CHECK(only("a0", "") && has("a2a6") && has("a2a9") && !has("a2a5"));
    pos(0, "Ke1 Ca0 Pa2", "Kf8 Ca5");
    CHECK(only("a0", "a0a3 a0a4"));             /* the cannon on a5 cannot be taken */
    pos(0, "Ke0 Cd0 Pe1", "Kf8 Rf2");           /* along the diagonal, over the middle */
    CHECK(has("d0f2"));
    pos(0, "Ke0 Cd0", "Kf8 Rf2");
    CHECK(!has("d0f2") && !has("d0e1"));
    pos(0, "Kd1 Cd2 Pe1", "Kf8");
    CHECK(has("d2f0"));
    pos(1, "Kd0 Cf9 Pe9", "Ke8");               /* no check: the screen must be between */
    CHECK(!jg_in_check(&P, 1));
    pos(1, "Kd0 Ce5 Pe6", "Ke8");               /* check over a soldier */
    CHECK(jg_in_check(&P, 1));
    pos(1, "Kd0 Ce5 Ce6", "Ke8");               /* no check over a cannon */
    CHECK(!jg_in_check(&P, 1));

    /* the soldiers: forward or sideways, never back; forward along the diagonals of the other palace */
    pos(0, "Ke0 Pc4", "Kf8");
    CHECK(only("c4", "c4c5 c4b4 c4d4"));
    pos(1, "Ke0", "Kf8 Pc4");
    CHECK(only("c4", "c4c3 c4b4 c4d4"));
    pos(0, "Kd0 Pd7", "Kf9");
    CHECK(only("d7", "d7d8 d7c7 d7e7 d7e8"));
    pos(0, "Kd0 Pe8", "Kf9");
    CHECK(only("e8", "e8e9 e8d8 e8f8 e8d9 e8f9"));
    pos(0, "Kd0 Pd8", "Kf9");
    CHECK(only("d8", "d8d9 d8c8 d8e8"));
    pos(1, "Kd0", "Kf9 Pf2");                   /* Han's soldier in Cho's palace, towards the middle */
    CHECK(only("f2", "f2f1 f2e2 f2g2 f2e1"));

    /* the generals may not be left facing each other on an open file */
    pos(0, "Ke1 Re4", "Ke8");
    CHECK(has("e4e5") && has("e4e7") && !has("e4d4") && has("e1d1"));
    pos(0, "Kd1", "Ke8");
    CHECK(!has("d1e1") && has("d1d0"));

    /* check: every legal move answers it, and no pass */
    pos(0, "Ke1 Rh0", "Kf8 Re5");
    CHECK(jg_in_check(&P, 0) && !jg_can_pass(&P) && N > 0);
    {
        int all = 1;
        for (int i = 0; i < N; i++) {
            jpos q = P;
            jg_make(&q, M[i]);
            all &= !jg_in_check(&q, 0);
        }
        CHECK(all && has("e1d1") && has("e1d2") && !has("e1f1") && !has("h0h1") && !has("e1e2"));   /* f1: facing */
    }

    /* mate: the side to move lost */
    pos(1, "Kd0 Ra9 Rb8", "Ke9");
    CHECK(N == 0 && jg_in_check(&P, 1) && !jg_can_pass(&P));
    jg_new(&G);
    G.pos[0] = P;
    CHECK(jg_status(&G) == JG_MATE);
    /* no move but not in check: the player passes, the game goes on */
    pos(1, "Kd0 Rb8 Rh8 Pd8 Pf8", "Ke9");
    CHECK(N == 0 && !jg_in_check(&P, 1) && jg_can_pass(&P));
    G.pos[0] = P;
    CHECK(jg_status(&G) == JG_PLAY);
    jg_play(&G, (jmove){JG_PASS, JG_PASS, 0});
    CHECK(jg_pos(&G)->side == 0 && !memcmp(jg_pos(&G)->sq, P.sq, sizeof P.sq));

    /* a few moves and a pass, then undo all the way */
    CHECK(game("e3f3 c6d6 pass i6i5"));
    CHECK(jg_status(&G) == JG_PLAY && G.move[2].from == JG_PASS && jg_pos(&G)->side == 0);
    {
        jpos start;
        jg_start(&start);
        int n = 0;
        while (jg_undo(&G)) n++;
        CHECK(n == 4 && !memcmp(jg_pos(&G), &start, sizeof start) && !jg_undo(&G));
    }
    CHECK(!game("e3e4 e6e5 e4e5 pass e5e6 e8e7"));   /* the soldier takes, Han passes; e7: guarded by it */
    CHECK(G.ply == 5 && G.move[2].cap == JG_PC(1, JG_SOLDIER));

    /* the same position a third time: a draw; four passes in a row bring it back too */
    CHECK(game("a0a1 a9a8 a1a0 a8a9 a0a1 a9a8 a1a0"));
    CHECK(jg_status(&G) == JG_PLAY);
    CHECK(game("a0a1 a9a8 a1a0 a8a9 a0a1 a9a8 a1a0 a8a9"));
    CHECK(jg_status(&G) == JG_REPETITION);
    jg_undo(&G);
    CHECK(jg_status(&G) == JG_PLAY);
    CHECK(game("pass pass pass") && jg_status(&G) == JG_PLAY);
    CHECK(game("pass pass pass pass") && jg_status(&G) == JG_REPETITION);

    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails != 0;
}
