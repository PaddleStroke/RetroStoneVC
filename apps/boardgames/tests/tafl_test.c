/*
 * The Hnefatafl, Tablut and Brandubh rules on a few positions: the start, rook moves and the restricted squares,
 * captures against pieces, corners and the empty throne, the safe move between two enemies, the king's capture and
 * escape, no move, repetition, undo; whole random games of each.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/tafl.h"

#define H TF_HNEFATAFL
#define T TF_TABLUT
#define B TF_BRANDUBH

static int fails;
static tgame G;

static void ok(const char *name, int good)
{
    printf("%s %s\n", good ? "ok  " : "FAIL", name);
    fails += !good;
}

static int sq(const char *s)
{
    int used;
    return tf_square(G.variant, s, &used);
}

/* an empty board, `side` to move, then the pieces: "Ka5 Ac3 Dd4" (A an attacker, D a defender, K the king) */
static void setup(int variant, int side, const char *pieces)
{
    tf_new(&G, variant);
    memset(G.pos[0].sq, 0, sizeof G.pos[0].sq);
    G.pos[0].side = (uint8_t)side;
    for (const char *s = pieces; *s;) {
        while (*s == ' ') s++;
        if (!*s) break;
        int used, at = tf_square(variant, s + 1, &used);
        G.pos[0].sq[at] = s[0] == 'A' ? TF_ATT : s[0] == 'D' ? TF_DEF : TF_KING;
        s += 1 + used;
    }
}

static int can(const char *from, const char *to)
{
    tmove m[TF_MAX_MOVES];
    int n = tf_legal(tf_pos(&G), G.variant, m), f = sq(from), t = sq(to);
    for (int i = 0; i < n; i++)
        if (m[i].from == f && m[i].to == t) return 1;
    return 0;
}

static int at(const char *s) { return tf_pos(&G)->sq[sq(s)]; }

int main(void)
{
    /* the start */
    tf_new(&G, H);
    tmove m[TF_MAX_MOVES];
    ok("hnefatafl: 24 attackers, 12 defenders, a king", tf_count(H, TF_ATT) == 24 && tf_count(H, TF_DEF) == 12 &&
       tf_count(H, TF_KING) == 1 && at("f6") == TF_KING && at("a4") == TF_ATT && at("d6") == TF_DEF);
    ok("tablut: 16 attackers, 8 defenders, a king", tf_count(T, TF_ATT) == 16 && tf_count(T, TF_DEF) == 8 &&
       tf_count(T, TF_KING) == 1);
    ok("the attackers move first", tf_pos(&G)->side == 0 && can("a4", "d4") && !can("f6", "f7"));
    ok("116 opening moves", tf_legal(tf_pos(&G), H, m) == 116);

    /* rook moves, the restricted squares */
    setup(H, 0, "Ab1 Ab5 Kh8");
    ok("a rook move along a row or a column", can("b1", "j1") && can("b1", "b4") && !can("b1", "c2"));
    ok("blocked by a piece", !can("b1", "b5") && !can("b1", "b6"));
    ok("not onto a corner", !can("b1", "a1") && !can("b1", "k1"));
    setup(H, 0, "Ac6 Kh8");
    ok("over the empty throne, not onto it", can("c6", "g6") && can("c6", "k6") && !can("c6", "f6"));
    setup(H, 1, "Kf8 Dc3");
    ok("the king back onto the throne", can("f8", "f6") && can("f8", "f1"));
    setup(H, 1, "Kc1");
    ok("the king onto a corner", can("c1", "a1") && can("c1", "k1"));

    /* captures */
    setup(H, 0, "Ac5 Ae2 Dd5 Kh9");
    tf_script(&G, "e2-e5");
    ok("a sandwich takes", at("d5") == TF_EMPTY && at("e5") == TF_ATT && G.move[0].ncap == 1);
    setup(H, 0, "Ac3 Db1 Kh9");
    tf_script(&G, "c3-c1");
    ok("against a corner", at("b1") == TF_EMPTY);
    setup(H, 1, "Aa2 Dd3 Kh9");
    tf_script(&G, "d3-a3");
    ok("the defenders against a corner too", at("a2") == TF_EMPTY);
    setup(H, 0, "Ae7 De5 Ae2 Kh9");
    tf_script(&G, "e2-e3");
    ok("not across a gap", at("e5") == TF_DEF);
    setup(H, 0, "De6 Ad9 Kh9");
    tf_script(&G, "d9-d6");
    ok("a defender against the empty throne", at("e6") == TF_EMPTY);
    setup(H, 0, "Kf6 De6 Ad9 Ah1");
    tf_script(&G, "d9-d6");
    ok("not against the throne with the king on it", at("e6") == TF_DEF);
    setup(H, 1, "Ae6 Dd9 Kh9");
    tf_script(&G, "d9-d6");
    ok("an attacker against the empty throne", at("e6") == TF_EMPTY);
    setup(H, 1, "Kf6 Ae6 Dd9");
    tf_script(&G, "d9-d6");
    ok("an attacker against the king on the throne", at("e6") == TF_EMPTY);
    setup(H, 1, "Kc5 Ad5 De1 Aj9");
    tf_script(&G, "e1-e5");
    ok("the king as the anvil", at("d5") == TF_EMPTY);
    setup(H, 1, "Kc1 Ad5 De5 Aj9");
    tf_script(&G, "c1-c5");
    ok("the king as the hammer", at("d5") == TF_EMPTY);
    setup(H, 1, "Ac5 Ae5 Dd1 Kh9");
    tf_script(&G, "d1-d5");
    ok("moving between two enemies is safe", at("d5") == TF_DEF);
    setup(H, 0, "Ad5 De5 Df5 Ag2 Kj2");
    tf_script(&G, "g2-g5");
    ok("a row of two is not taken", at("e5") == TF_DEF && at("f5") == TF_DEF);
    setup(H, 0, "Dd4 Df4 De5 Ac4 Ag4 Ae6 Ae1 Kj9");
    tf_script(&G, "e1-e4");
    ok("one move, three captures", !at("d4") && !at("f4") && !at("e5") && G.move[0].ncap == 3);

    /* the king */
    setup(H, 0, "Ke5 Ad5 Af5 Ae4 Ae9");
    tf_script(&G, "e9-e6");
    ok("the king taken on four sides", at("e5") == TF_EMPTY && tf_status(&G) == TF_WON0);
    setup(H, 0, "Ke6 Ae5 Ae7 Ad3");
    tf_script(&G, "d3-d6");
    ok("three sides and the empty throne", at("e6") == TF_EMPTY && tf_status(&G) == TF_WON0);
    setup(H, 0, "Kf6 Ae6 Ag6 Af5 Ad7");
    tf_script(&G, "d7-f7");
    ok("on the throne: four sides", at("f6") == TF_EMPTY);
    setup(H, 0, "Ke5 Ad5 Ae4 Ae9");
    tf_script(&G, "e9-e6");
    ok("not with three sides", at("e5") == TF_KING && tf_status(&G) == TF_PLAY);
    setup(H, 0, "Ke1 Ad1 Ae2 Af3");
    tf_script(&G, "f3-f1");
    ok("never on the edge", at("e1") == TF_KING);
    setup(T, 0, "Kc3 Ab3 Ad5");
    tf_script(&G, "d5-d3");
    ok("tablut, away from the throne: two sides", at("c3") == TF_EMPTY && tf_status(&G) == TF_WON0);
    setup(T, 0, "Ke4 Ad4 Af9");
    tf_script(&G, "f9-f4");
    ok("tablut, next to the throne: not two sides", at("e4") == TF_KING);

    /* the escape */
    setup(H, 1, "Kc1 Ae8");
    ok("the king in a corner wins (hnefatafl)", tf_script(&G, "c1-a1") && tf_status(&G) == TF_WON1);
    setup(H, 1, "Kc4 Ae8");
    ok("the edge is not enough (hnefatafl)", tf_script(&G, "c4-a4") && tf_status(&G) == TF_PLAY);
    setup(T, 1, "Kc4 Ae8");
    ok("the edge wins (tablut)", tf_script(&G, "c4-a4") && tf_status(&G) == TF_WON1);
    setup(T, 0, "Ab1 Kd5");
    ok("tablut's corners are plain squares", can("b1", "a1"));

    /* no move, repetition */
    setup(H, 1, "Kd1 Ac1 Ae1 Ad2 Ak5");
    ok("the defenders cannot move: they lose", tf_status(&G) == TF_WON0);
    setup(H, 0, "Ab1 Dc1 Db2 Kj9");
    ok("the attackers cannot move: they lose", tf_status(&G) == TF_WON1);
    setup(H, 0, "Ac3 Kh8 Dj9");
    tf_script(&G, "c3-c4 j9-j8 c4-c3 j8-j9 c3-c4 j9-j8 c4-c3");
    ok("not yet a repetition", tf_status(&G) == TF_PLAY);
    tf_script(&G, "j8-j9");
    ok("the third repetition is a draw", tf_status(&G) == TF_DRAW && !tf_script(&G, "c3-c4"));

    /* Brandubh, 7 x 7 */
    tf_new(&G, B);
    ok("brandubh: 8 attackers, 4 defenders, a king", tf_count(B, TF_ATT) == 8 && tf_count(B, TF_DEF) == 4 &&
       tf_count(B, TF_KING) == 1 && at("d4") == TF_KING && at("d1") == TF_ATT && at("a4") == TF_ATT &&
       at("c4") == TF_DEF && at("d5") == TF_DEF && tf_size(B) == 7);
    ok("brandubh: 40 opening moves, the attackers first", tf_legal(tf_pos(&G), B, m) == 40 && can("d2", "a2") &&
       !can("d1", "a1"));
    setup(B, 0, "Kb2 Aa2 Ac5");
    tf_script(&G, "c5-c2");
    ok("brandubh: the king taken by two, away from the throne", at("b2") == TF_EMPTY && tf_status(&G) == TF_WON0);
    setup(B, 0, "Kb1 Ac7 Ag6");
    tf_script(&G, "c7-c1");
    ok("brandubh: the king taken against a corner", at("b1") == TF_EMPTY && tf_status(&G) == TF_WON0);
    setup(B, 0, "Kc4 Ab7 Ag1");
    tf_script(&G, "b7-b4");
    ok("brandubh: next to the throne, two are not enough", at("c4") == TF_KING && tf_status(&G) == TF_PLAY);
    setup(B, 0, "Kc4 Ab4 Ac5 Ae3");
    tf_script(&G, "e3-c3");
    ok("brandubh: next to the throne, three and the throne", at("c4") == TF_EMPTY && tf_status(&G) == TF_WON0);
    setup(B, 0, "Kd4 Ac4 Ae4 Ad5 Ag3");
    tf_script(&G, "g3-d3");
    ok("brandubh: on the throne, four sides", at("d4") == TF_EMPTY);
    setup(B, 1, "Kc1 Ae7");
    ok("brandubh: the king in a corner wins", tf_script(&G, "c1-a1") && tf_status(&G) == TF_WON1);
    setup(B, 1, "Kc3 Ae7");
    ok("brandubh: the edge is not enough", tf_script(&G, "c3-a3") && tf_status(&G) == TF_PLAY);
    setup(B, 0, "Ab1 Ad7 Kg5");
    ok("brandubh: not onto a corner", !can("b1", "a1") && can("d7", "b7") && !can("d7", "a7"));
    setup(B, 0, "Ac4 Kg5");
    ok("brandubh: over the empty throne, not onto it", can("c4", "e4") && !can("c4", "d4"));
    setup(B, 1, "Dc3 Ab1 Kg5");
    tf_script(&G, "c3-c1");
    ok("brandubh: a capture against a corner", at("b1") == TF_EMPTY);

    /* whole random games of each board, a seed each: no piece ever comes back, the game ends, undo to the start */
    for (int v = 0; v < 3; v++) {
        unsigned seed = 777u + (unsigned)v;
        int bad = 0, ends[4] = {0};
        for (int k = 0; k < 60; k++) {
            tf_new(&G, v);
            tpos first = *tf_pos(&G);
            int att = tf_count(v, TF_ATT), def = tf_count(v, TF_DEF);
            while (tf_status(&G) == TF_PLAY) {
                int nm = tf_legal(tf_pos(&G), v, m);
                seed = seed * 1103515245u + 12345u;
                tmove mv = m[(seed >> 8) % (unsigned)nm];
                if (!tf_play(&G, &mv)) { bad++; break; }
                int a = 0, d = 0;
                for (int s = 0; s < tf_size(v) * tf_size(v); s++)
                    a += tf_pos(&G)->sq[s] == TF_ATT, d += tf_pos(&G)->sq[s] == TF_DEF;
                bad += a > att || d > def || a + d + (tf_king(tf_pos(&G), v) >= 0) + mv.ncap != att + def + 1;
                att = a, def = d;
            }
            ends[tf_status(&G)]++;
            while (tf_undo(&G)) {}
            bad += memcmp(&first, tf_pos(&G), sizeof first) != 0;
        }
        printf("%s 60 random games, board %d: %d won by the attackers, %d by the defenders, %d drawn\n",
               bad ? "FAIL" : "ok  ", v, ends[TF_WON0], ends[TF_WON1], ends[TF_DRAW]);
        fails += bad != 0;
    }

    /* a game, undo */
    tf_new(&G, H);
    tpos start = *tf_pos(&G);
    ok("a game", tf_script(&G, "d1-d3 f4-e4 a4-c4 e4-e2 d3-e3"));
    ok("a capture in it", G.move[4].ncap == 1 && !at("e2"));
    ok("illegal moves refused", !tf_script(&G, "a6-f6") && !tf_script(&G, "a5-a5") && !tf_script(&G, "f6-f9"));
    while (tf_undo(&G)) {}
    ok("undo all the way", G.ply == 0 && !memcmp(&start, tf_pos(&G), sizeof start));

    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
