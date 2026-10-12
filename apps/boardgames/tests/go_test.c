/*
 * Go's rules test (src/games/go_rules.c, no SDL): captures, suicide, ko, the passes, the counting, undo.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/go.h"

static gogame G;
static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int at(const char *name)
{
    int len;
    return go_point(G.n, name, &len);
}

static int stone(const char *name) { return go_now(&G)->pt[at(name)]; }

static void fresh(int n, const char *moves)
{
    go_new(&G, n);
    if (!go_script(&G, moves)) printf("bad script: %s\n", moves);
}

int main(void)
{
    /* notation: letters skip the i, numbers from the bottom */
    go_new(&G, 19);
    CHECK(at("a1") == 0 && at("t19") == 360 && at("q16") == 15 + 15 * 19 && at("j1") == 8);
    CHECK(at("i5") == GO_NONE && at("u1") == GO_NONE && at("a20") == GO_NONE && at("pass") == GO_PASS);

    /* one stone captured in the middle, then two together */
    fresh(9, "d5 e5 f5 a1 e4 a2 e6");
    CHECK(stone("e5") == GO_EMPTY && go_now(&G)->caps[0] == 1 && go_now(&G)->side == 1);
    fresh(9, "d5 e5 d6 e6 f5 a1 f6 a2 e4 a3 e7");
    CHECK(stone("e5") == GO_EMPTY && stone("e6") == GO_EMPTY && go_now(&G)->caps[0] == 2);

    /* at the edge, in the corner */
    fresh(9, "a4 a5 a6 j9 b5");
    CHECK(stone("a5") == GO_EMPTY && go_now(&G)->caps[0] == 1);
    fresh(9, "a2 a1 b1");
    CHECK(stone("a1") == GO_EMPTY && go_now(&G)->caps[0] == 1);

    /* suicide refused (the corner, then a group's last liberty); a suicide-looking move that captures is legal */
    fresh(9, "a2 j9 b1");
    CHECK(go_play(&G, at("a1")) < 0 && G.nmoves == 3);
    fresh(9, "a2 a1 b2 j9 c1 j8");                /* ivory's a1 lives on b1: ivory there would take its own last */
    CHECK(go_play(&G, at("j7")) == 0 && go_play(&G, at("b1")) < 0);
    fresh(9, "a3 a2 b2 b1 c1 j9");
    CHECK(go_play(&G, at("a1")) == 2 && stone("a2") == GO_EMPTY && stone("b1") == GO_EMPTY && stone("a1") == GO_EBONY);

    /* ko: the stone just captured may not be taken back at once; after a move elsewhere on each side it may */
    fresh(9, "d5 f4 e4 f6 e6 g5 a9 e5 f5");
    CHECK(stone("e5") == GO_EMPTY && go_now(&G)->caps[0] == 1);
    CHECK(go_play(&G, at("e5")) < 0);
    CHECK(go_script(&G, "j1 j9") && go_play(&G, at("e5")) == 1 && stone("f5") == GO_EMPTY);

    /* two passes start the counting; no stone then; undo goes back before the last pass, then move by move */
    fresh(9, "d5 e5 f5 a1 e4 a2 e6 pass");
    CHECK(G.phase == GO_PLAY && go_pass(&G) && G.phase == GO_COUNT && go_play(&G, at("c3")) < 0);
    CHECK(go_undo(&G) && G.phase == GO_PLAY && G.nmoves == 8 && go_last(&G) == GO_PASS);
    CHECK(go_undo(&G) && go_undo(&G) && stone("e5") == GO_IVORY && go_now(&G)->caps[0] == 0 && go_now(&G)->side == 0);
    while (go_undo(&G)) {}
    CHECK(G.nmoves == 0 && go_last(&G) == GO_NONE && go_now(&G)->side == 0);

    /* area scoring: ebony walls off five columns, ivory four and a stone inside ebony's (dead, then alive) */
    fresh(9, "e1 f1 e2 f2 e3 f3 e4 f4 e5 f5 e6 f6 e7 f7 e8 f8 e9 f9 pass b5 pass pass");
    int8_t owner[GO_PTS];
    float sc[2];
    CHECK(G.phase == GO_COUNT);
    go_score(&G, owner, sc);                      /* b5 alive: a..d nobody's */
    CHECK(sc[0] == 9 && sc[1] == 36 + 1 + GO_KOMI && owner[at("a1")] == -1 && owner[at("j1")] == 1);
    CHECK(go_toggle(&G, at("b5")) && G.dead[at("b5")] && !go_toggle(&G, at("c3")));
    go_score(&G, owner, sc);                      /* b5 dead: a..e ebony's */
    CHECK(sc[0] == 45 && sc[1] == 36 + GO_KOMI && owner[at("b5")] == 0 && owner[at("a1")] == 0);
    CHECK(go_winner(&G) == -1 && go_pass(&G) && G.phase == GO_OVER && go_winner(&G) == 0);
    CHECK(go_undo(&G) && G.phase == GO_COUNT && G.dead[at("b5")]);
    CHECK(go_toggle(&G, at("e5")) && G.dead[at("e1")] && G.dead[at("e9")]);   /* the whole group */

    /* the komi decides an empty board */
    fresh(13, "pass pass pass");
    go_score(&G, owner, sc);
    CHECK(G.phase == GO_OVER && sc[0] == 0 && sc[1] == GO_KOMI && go_winner(&G) == 1);

    /* capture go: the first capture wins at once (either player), no pass, undo takes the winning move back */
    go_new_capture(&G, 9);
    CHECK(G.n == 9 && G.capture && go_script(&G, "d5 e5 f5 a1 e4 a2") && G.phase == GO_PLAY && go_winner(&G) == -1);
    CHECK(go_play(&G, at("e6")) == 1 && G.phase == GO_OVER && go_winner(&G) == 0);
    CHECK(go_play(&G, at("j9")) < 0 && !go_pass(&G));
    CHECK(go_undo(&G) && G.phase == GO_PLAY && stone("e5") == GO_IVORY && stone("e6") == GO_EMPTY);
    go_new_capture(&G, 13);
    CHECK(G.n == 13 && go_script(&G, "a2 a1 j9 b1") && G.phase == GO_PLAY);
    CHECK(go_play(&G, at("b2")) == 0 && go_script(&G, "c1") && G.phase == GO_PLAY);   /* b1, a1: two liberties */
    CHECK(!go_script(&G, "pass") && go_script(&G, "j8 c2") == 1 && G.phase == GO_PLAY);
    go_new_capture(&G, 9);
    CHECK(go_script(&G, "e5 d5 j9 c5 j8 d4 j7 d6 j6") && G.phase == GO_PLAY);
    CHECK(go_play(&G, at("e4")) == 0 && go_script(&G, "j5 e6 j4 f5") && G.phase == GO_OVER && go_winner(&G) == 1);

    /* nowhere to play: every empty point would take the stone's own last liberty, nothing captured */
    go_new_capture(&G, 9);
    gopos *full = &G.pos[0];
    for (int p = 0; p < 81; p++) full->pt[p] = GO_EBONY;
    full->pt[at("a1")] = full->pt[at("j9")] = GO_EMPTY;
    full->side = 1;
    CHECK(!go_can_play(&G));
    full->side = 0;
    full->pt[at("e5")] = GO_EMPTY;
    full->pt[at("j9")] = GO_IVORY;
    CHECK(go_can_play(&G));

    /* whole random capture games (a seed each): each ends at its first capture, the one who made it the winner */
    int bad = 0, captured = 0;
    unsigned seed = 2026;
    for (int k = 0; k < 60; k++) {
        go_new_capture(&G, k & 1 ? 13 : 9);
        while (G.phase == GO_PLAY && G.nmoves < GO_HIST) {
            int empty[GO_PTS], ne = 0;
            for (int p = 0; p < G.n * G.n; p++)
                if (go_now(&G)->pt[p] == GO_EMPTY) empty[ne++] = p;
            int took = -1;
            while (took < 0 && ne) {
                seed = seed * 1103515245u + 12345u;
                int i = (int)((seed >> 8) % (unsigned)ne);
                took = go_play(&G, empty[i]);
                empty[i] = empty[--ne];
            }
            if (took < 0) { bad++; break; }
            if (took > 0 && G.phase != GO_OVER) bad++;
        }
        const gopos *p = go_now(&G);
        int caps = p->caps[0] + p->caps[1];
        captured += caps > 0;
        if (G.phase != GO_OVER || (caps && p->caps[go_winner(&G)] != caps) || (!caps && go_can_play(&G))) bad++;
    }
    CHECK(!bad && captured > 50);
    printf("go: 60 random capture games, %d ended by a capture, %d bad\n", captured, bad);

    printf("go: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
