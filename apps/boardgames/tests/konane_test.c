/*
 * Konane: the rules' test (the boards, the opening removals, single and multiple jumps, no turning, stopping, the end
 * with no move, undo, the notation). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/konane.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static kngame G;
static knmove L[KN_MAX_MOVES];

static const knpos *pos(void) { return kn_pos(&G); }
static int nlegal(void) { return kn_legal(pos(), L); }
static int sq(const char *s) { return kn_square(pos()->n, s, NULL); }
static int at(const char *s) { return pos()->at[sq(s)]; }

static int has(const char *from, const char *to)
{
    int n = nlegal();
    for (int i = 0; i < n; i++)
        if (L[i].from == sq(from) && L[i].to == sq(to)) return 1;
    return 0;
}

/* a position after the opening on an 8 x 8 board: player 0's stones ("a1 c3"), player 1's, who moves */
static void setup(const char *zero, const char *one, int side)
{
    kn_new(&G, 8);
    knpos *p = &G.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->opening = 2;
    p->side = (uint8_t)side;
    const char *list[2] = {zero, one};
    for (int c = 0; c < 2; c++)
        for (const char *t = list[c]; *t;) {
            int len, s = kn_square(8, t, &len);
            if (s < 0) { t++; continue; }
            p->at[s] = (int8_t)c;
            t += len;
        }
}

/* the boards: full, the colours alternating; player 0 on the squares whose x + y is odd */
static void test_board(void)
{
    for (int v = 0; v < KN_VARIANTS; v++) {
        int n = kn_size(v), ok = 1, count[2] = {0};
        kn_new(&G, n);
        for (int s = 0; s < n * n; s++) {
            ok &= pos()->at[s] == kn_owner(n, s) && pos()->at[s] == (((s % n + s / n) & 1) ? 0 : 1);
            count[pos()->at[s]]++;
        }
        CHECK(ok && count[0] == n * n / 2 && count[1] == n * n / 2);
        CHECK(pos()->side == 0 && pos()->opening == 0 && kn_winner(&G) < 0);
    }
    CHECK(kn_size(0) == 6 && kn_size(1) == 8 && kn_size(2) == 10);
    int len;
    CHECK(kn_square(8, "a1", &len) == 0 && len == 2 && kn_square(8, "h8", NULL) == 63);
    CHECK(kn_square(10, "j10", &len) == 99 && len == 3 && kn_square(8, "a9", NULL) < 0);
    CHECK(kn_square(8, "i1", NULL) < 0 && kn_square(6, "f6", NULL) == 35 && kn_square(6, "a7", NULL) < 0);
}

/* the opening: player 0 lifts a stone of theirs from the middle or a corner, player 1 one next to the hole */
static void test_opening(void)
{
    kn_new(&G, 8);
    CHECK(nlegal() == 4 && has("e4", "e4") && has("d5", "d5") && has("h1", "h1") && has("a8", "a8"));
    CHECK(!kn_script(&G, "d4") && !kn_script(&G, "a1") && !kn_script(&G, "c3"));   /* not theirs, not allowed */
    CHECK(!kn_script(&G, "b2-d2"));             /* no jumps before the removals */
    CHECK(kn_script(&G, "e4") && at("e4") < 0 && pos()->side == 1 && pos()->opening == 1);
    CHECK(nlegal() == 4 && has("e5", "e5") && has("e3", "e3") && has("d4", "d4") && has("f4", "f4"));
    CHECK(!kn_script(&G, "e6") && !kn_script(&G, "d5"));   /* not next to the hole; not theirs */
    CHECK(kn_script(&G, "e5") && at("e5") < 0 && pos()->side == 0 && pos()->opening == 2);
    /* now the jumps: into player 0's hole e4, from e2, c4 and g4 (e6's way is empty) */
    CHECK(nlegal() == 3 && has("e2", "e4") && has("c4", "e4") && has("g4", "e4"));
    kn_new(&G, 8);                              /* a corner: two neighbours */
    CHECK(kn_script(&G, "h1") && nlegal() == 2 && has("g1", "g1") && has("h2", "h2"));
    for (int v = 0; v < KN_VARIANTS; v += 2) {  /* 6 x 6 and 10 x 10: four choices too */
        kn_new(&G, kn_size(v));
        CHECK(nlegal() == 4);
    }
    kn_new(&G, 10);
    CHECK(has("f5", "f5") && has("e6", "e6") && has("j1", "j1") && has("a10", "a10"));
}

/* jumps: over an enemy stone next to it onto the empty square beyond; on in a straight line, never turning */
static void test_jumps(void)
{
    setup("a1", "a2 a4 a6 b3", 0);
    CHECK(nlegal() == 3 && has("a1", "a3") && has("a1", "a5") && has("a1", "a7"));
    CHECK(!has("a1", "c3"));                    /* a3 then right over b3: a turn, not allowed */
    CHECK(kn_script(&G, "a1-a5") && at("a2") < 0 && at("a4") < 0 && at("a5") == 0 && at("a6") == 1);
    CHECK(pos()->took[0] == 2 && pos()->side == 1);
    setup("a1", "a2 a4 a6 b3", 0);              /* stopping is allowed: one jump, then the other player's turn */
    CHECK(kn_script(&G, "a1-a3") && pos()->side == 1 && at("a4") == 1 && pos()->took[0] == 1);
    setup("a1", "a2 a4 a6 b3", 0);              /* written landing by landing */
    CHECK(kn_script(&G, "a1-a3-a5-a7") && pos()->took[0] == 3 && at("a7") == 0);
    setup("a1", "a2 a4 a6 b3", 0);
    CHECK(!kn_script(&G, "a1-a3-c3") && !kn_script(&G, "a1-a5-a3") && !kn_script(&G, "a1xa3xa3"));
    setup("a1 a3", "a2", 0);                    /* the square beyond taken: no jump */
    CHECK(nlegal() == 0);
    setup("a1 a2", "h8", 0);                    /* nor over one's own stone, nor diagonally */
    CHECK(nlegal() == 0);
    setup("c3", "d4 b2", 0);
    CHECK(nlegal() == 0);
    setup("d4", "d5 e4 d3 c4", 0);              /* all four ways */
    CHECK(nlegal() == 4 && has("d4", "d6") && has("d4", "f4") && has("d4", "d2") && has("d4", "b4"));
    setup("a1", "a2 a3", 0);                    /* two stones in a row cannot be jumped */
    CHECK(nlegal() == 0);
}

/* the end: the player to move with no jump loses; no draws (a whole game ends with a winner) */
static void test_end(void)
{
    setup("a1", "h8", 0);
    CHECK(kn_winner(&G) == 1);
    setup("a1", "a2", 1);
    CHECK(kn_winner(&G) == 0);
    setup("a1", "a2", 0);
    CHECK(kn_winner(&G) < 0 && kn_script(&G, "a1-a3") && kn_winner(&G) == 0);
    for (int v = 0; v < KN_VARIANTS; v++)
        for (int seed = 0; seed < 5; seed++) {  /* whole games: every jump takes a stone; the end has a winner */
            kn_new(&G, kn_size(v));
            int ok = 1;
            while (kn_winner(&G) < 0) {
                int n = nlegal();
                if (!kn_play(&G, L[(G.ply * 7 + seed * 3) % n])) { ok = 0; break; }
            }
            ok &= pos()->took[0] + pos()->took[1] >= G.ply - 2 && G.ply >= 2;
            ok &= kn_winner(&G) == !pos()->side;
            CHECK(ok);
        }
}

/* undo: back one move at a time, all the way to the start */
static void test_undo(void)
{
    kn_new(&G, 8);
    knpos start = *pos();
    CHECK(!kn_undo(&G));
    CHECK(kn_script(&G, "e4 e5 c4-e4"));
    CHECK(G.ply == 3 && pos()->took[0] == 1 && at("d4") < 0);
    int n = 0;
    while (kn_undo(&G)) n++;
    CHECK(n == 3 && !memcmp(pos(), &start, sizeof start));
    CHECK(!kn_script(&G, "z9") && !kn_script(&G, "e4-") && !kn_script(&G, "e4e5"));
}

int main(void)
{
    test_board();
    test_opening();
    test_jumps();
    test_end();
    test_undo();
    printf("konane: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
