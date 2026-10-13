/*
 * Klondike: the rules' test (the deal; the moves' legality, the foundations, the stock and its unlimited turning
 * over, the taps, the end going up by itself, undo; whole random games from seeds; a deal solved by a small search
 * and played again from its script). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/klondike.h"
#include "games/klondike_levels.h"
#include "levels_check.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static solrec G, H;
static solpos *P = &G.pos;

#define S(s, r) CARD(SUIT_##s, r)

/* every card once: 52 cards, nothing twice */
static int whole(const solpos *p)
{
    int seen[256] = {0}, n = 0;
    for (int i = 0; i < p->npiles; i++)
        for (int k = 0; k < p->n[i]; k++) seen[p->c[i][k]]++, n++;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (seen[CARD(s, r)] != 1) return 0;
    return n == 52;
}

/* an empty table to set cards on (the 52 elsewhere don't matter for these checks) */
static void clear(void)
{
    kl_new(&G, KL_DRAW1, 1);
    for (int i = 0; i < KL_PILES; i++) P->n[i] = P->down[i] = 0;
    P->n[KL_STOCK] = P->down[KL_STOCK] = 1;     /* a card in the stock: no finishing by itself */
}

static void put(int pile, int down, const int *cards, int n)
{
    for (int k = 0; k < n; k++) P->c[pile][k] = (uint8_t)cards[k];
    P->n[pile] = (uint8_t)n;
    P->down[pile] = (uint8_t)down;
}

static int legal(int a, int b, int n) { return kl_legal(P, sol_mv(SM_MOVE, a, b, n)); }

static void test_deal(void)
{
    kl_new(&G, KL_DRAW1, 42);
    kl_new(&H, KL_DRAW1, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    for (int c = 0; c < 7; c++) CHECK(P->n[c] == c + 1 && P->down[c] == c);
    CHECK(P->n[KL_STOCK] == 24 && P->down[KL_STOCK] == 24 && P->n[KL_WASTE] == 0 && whole(P));
    CHECK(P->draw == 1 && G.n == 0 && G.moves == 0);
    kl_new(&H, KL_DRAW3, 43);
    CHECK(H.pos.draw == 3 && memcmp(H.pos.c, G.pos.c, sizeof G.pos.c) != 0);
}

static void test_moves(void)
{
    clear();
    const int a[] = {S(SPADES, 7)}, b[] = {S(HEARTS, 6)}, c[] = {S(DIAMONDS, 7)}, k[] = {S(CLUBS, 13)};
    put(0, 0, a, 1);
    put(1, 0, b, 1);
    put(2, 0, c, 1);
    put(3, 0, k, 1);
    CHECK(legal(1, 0, 1));                      /* a red 6 on a black 7 */
    CHECK(!legal(1, 2, 1));                     /* not on a red 7 */
    CHECK(!legal(0, 2, 1) && !legal(2, 0, 1));  /* not the same rank */
    CHECK(legal(3, 4, 1) && !legal(0, 4, 1));   /* only a king to an empty column */
    CHECK(!legal(1, 0, 2) && !legal(1, 1, 1));  /* more cards than there are; to itself */
    /* a run moves whole or in part; a face-down card left on top turns up */
    clear();
    const int col[] = {S(HEARTS, 2), S(DIAMONDS, 10), S(CLUBS, 9), S(HEARTS, 8), S(SPADES, 7)},
              t1[] = {S(HEARTS, 10)}, t2[] = {S(SPADES, 10)}, t3[] = {S(SPADES, 9)};
    put(0, 1, col, 5);
    put(1, 0, t1, 1);
    put(2, 0, t2, 1);
    put(3, 0, t3, 1);
    CHECK(legal(0, 1, 3) && !legal(0, 1, 4) && !legal(0, 2, 3));
    CHECK(legal(0, 3, 2) && !legal(0, 3, 1));
    CHECK(kl_play(&G, sol_mv(SM_MOVE, 0, 1, 3)));
    CHECK(P->n[0] == 2 && P->down[0] == 1 && P->n[1] == 4 && sol_top(P, 1) == S(SPADES, 7));
    CHECK(kl_play(&G, sol_mv(SM_MOVE, 0, 1, 1)) == 0);     /* the 10 does not go on the 7 */
    const int t4[] = {S(CLUBS, 11)};
    put(4, 0, t4, 1);
    CHECK(kl_play(&G, sol_mv(SM_MOVE, 0, 4, 1)));
    CHECK(P->n[0] == 1 && P->down[0] == 0);     /* the 2 of hearts turned up */
    CHECK(!legal(0, 0, 1) && !legal(KL_STOCK, 0, 1));
    /* a run whose cards do not follow cannot move */
    const int bad[] = {S(SPADES, 9), S(HEARTS, 7)}, t6[] = {S(HEARTS, 10)};
    put(5, 0, bad, 2);
    put(6, 0, t6, 1);
    CHECK(!legal(5, 6, 2) && !legal(5, 6, 1));
}

static void test_foundations(void)
{
    clear();
    const int a[] = {S(HEARTS, 1)}, b[] = {S(HEARTS, 2)}, c[] = {S(SPADES, 2)}, d[] = {S(HEARTS, 3)};
    put(0, 0, a, 1);
    put(1, 0, b, 1);
    put(2, 0, c, 1);
    put(3, 0, d, 1);
    CHECK(!legal(1, KL_FOUND, 1) && legal(0, KL_FOUND, 1) && legal(0, KL_FOUND + 2, 1));   /* an ace, any empty one */
    CHECK(kl_play(&G, sol_mv(SM_MOVE, 0, KL_FOUND + 2, 1)));
    CHECK(!legal(2, KL_FOUND + 2, 1) && !legal(3, KL_FOUND + 2, 1) && legal(1, KL_FOUND + 2, 1));
    CHECK(!legal(1, KL_FOUND, 1));              /* the two of hearts only where the hearts are */
    CHECK(kl_play(&G, sol_mv(SM_MOVE, 1, KL_FOUND + 2, 1)) && sol_top(P, KL_FOUND + 2) == S(HEARTS, 2));
    /* back down to the tableau: on a black 3; never from one foundation to another */
    const int e[] = {S(CLUBS, 3)};
    put(4, 0, e, 1);
    CHECK(legal(KL_FOUND + 2, 4, 1) && !legal(KL_FOUND + 2, KL_FOUND, 1) && !legal(KL_FOUND + 2, 4, 2));
    /* a tap: the foundation first, else a column; a quick second tap: the foundation or nothing */
    solmove m = kl_rules.tap(P, 3, 0, 0);
    CHECK(m.kind == SM_MOVE && m.to == KL_FOUND + 2);
    m = kl_rules.tap(P, 2, 0, 1);
    CHECK(m.kind == SM_NONE);
    const int f[] = {S(DIAMONDS, 3)};
    put(5, 0, f, 1);
    m = kl_rules.tap(P, 2, 0, 0);
    CHECK(m.kind == SM_MOVE && m.to == 3);      /* the black 2 on a red 3: the next column round */
}

/* the stock: one card or three at a time; empty, it takes the waste back, again and again */
static void test_stock(void)
{
    kl_new(&G, KL_DRAW1, 5);
    uint8_t stock[24];
    memcpy(stock, P->c[KL_STOCK], 24);
    CHECK(!kl_legal(P, sol_mv(SM_REDEAL, KL_WASTE, KL_STOCK, 0)));
    for (int i = 0; i < 24; i++) CHECK(kl_script(&G, "s"));
    CHECK(P->n[KL_STOCK] == 0 && P->n[KL_WASTE] == 24 && sol_top(P, KL_WASTE) == stock[0]);
    CHECK(!kl_legal(P, sol_mv(SM_DRAW, KL_STOCK, KL_WASTE, 1)));
    for (int round = 0; round < 10; round++) {  /* unlimited */
        CHECK(kl_script(&G, "s"));              /* the waste turned over */
        CHECK(P->n[KL_STOCK] == 24 && P->n[KL_WASTE] == 0 && !memcmp(P->c[KL_STOCK], stock, 24));
        CHECK(P->down[KL_STOCK] == 24);
        for (int i = 0; i < 24; i++) kl_script(&G, "s");
    }
    CHECK(G.moves == 24 + 10 * 25);
    kl_new(&G, KL_DRAW3, 5);
    memcpy(stock, P->c[KL_STOCK], 24);
    CHECK(kl_script(&G, "s") && P->n[KL_WASTE] == 3 && sol_top(P, KL_WASTE) == stock[21]);
    CHECK(P->c[KL_WASTE][0] == stock[23]);      /* drawn one at a time: the third one on top */
    for (int i = 0; i < 7; i++) kl_script(&G, "s");
    CHECK(P->n[KL_STOCK] == 0 && kl_script(&G, "s") && P->n[KL_STOCK] == 24 && !memcmp(P->c[KL_STOCK], stock, 24));
    CHECK(!kl_legal(P, sol_mv(SM_DRAW, KL_STOCK, KL_WASTE, 1)));   /* draw three: three */
    /* the waste's top card plays */
    while (P->n[KL_WASTE] < 24) kl_script(&G, "s");
    CHECK(!kl_legal(P, sol_mv(SM_MOVE, KL_WASTE, 0, 2)));
}

/* the stock and the waste empty, all face up: the rest goes up by itself; undo takes it back with the move */
static void test_finish(void)
{
    clear();
    P->n[KL_STOCK] = P->down[KL_STOCK] = 0;
    int n = 0;
    for (int r = 13; r >= 1; r--)               /* four columns: K..A alternating colours */
        for (int c = 0; c < 4; c++) {
            int suit = (r + c) % 2 ? (c < 2 ? SUIT_HEARTS : SUIT_DIAMONDS) : (c < 2 ? SUIT_SPADES : SUIT_CLUBS);
            P->c[c][P->n[c]++] = (uint8_t)CARD(suit, r);
            n++;
        }
    /* column 0's ace on the waste instead; column 1's king face down */
    P->c[KL_WASTE][0] = P->c[0][12];
    P->n[0] = 12;
    P->n[KL_WASTE] = 1;
    P->down[1] = 1;
    G.start = G.pos;
    G.n = G.moves = 0;
    CHECK(whole(P) && n == 52);
    CHECK(!kl_won(P));
    solmove m = kl_hint(P);
    CHECK(m.kind == SM_MOVE && P->kind[m.to] == SP_FOUND);
    CHECK(kl_script(&G, "wh"));
    CHECK(!kl_won(P) && G.n == 1 && sol_on_found(P) == 1);     /* a face-down card is left: no finishing yet */
    P->down[1] = 0;                             /* (turned up by hand for the check) */
    G.start = G.pos;
    G.n = G.moves = 0;
    CHECK(kl_script(&G, "2h"));
    CHECK(kl_won(P) && G.moves == 1 && G.n == 52 - 1);
    for (int i = 1; i < G.n; i++) CHECK(G.mv[i].kind == (SM_MOVE | SM_AUTO));
    CHECK(sol_undo(&G) && G.n == 0 && !memcmp(&G.pos, &G.start, sizeof G.pos));
}

static uint32_t rng = 1;
static int rnd(int n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

/* every legal move from a position (the stock's too) */
static int all_moves(const solpos *p, solmove *out)
{
    int k = 0;
    solmove s = kl_rules.stock(p);
    if (s.kind != SM_NONE) out[k++] = s;
    for (int a = 0; a < KL_PILES; a++)
        for (int b = 0; b < KL_PILES; b++)
            for (int n = 1; n <= sol_up(p, a); n++)
                if (kl_legal(p, sol_mv(SM_MOVE, a, b, n))) out[k++] = sol_mv(SM_MOVE, a, b, n);
    return k;
}

static int sane(const solpos *p)
{
    if (!whole(p)) return 0;
    for (int c = 0; c < 7; c++) {
        if (p->down[c] > p->n[c] || (p->n[c] && p->down[c] == p->n[c])) return 0;
        if (p->n[c] && !sol_run(p, c, p->down[c], 0)) return 0;
    }
    for (int f = KL_FOUND; f < KL_STOCK; f++)
        for (int k = 0; k < p->n[f]; k++)
            if (CARD_RANK(p->c[f][k]) != k + 1 || CARD_SUIT(p->c[f][k]) != CARD_SUIT(p->c[f][0])) return 0;
    return p->down[KL_STOCK] == p->n[KL_STOCK] && p->down[KL_WASTE] == 0;
}

static void test_random(void)
{
    solmove list[512];
    int ok = 1, ends = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        rng = seed;
        kl_new(&G, seed % 2 ? KL_DRAW1 : KL_DRAW3, seed);
        solpos start = G.pos;
        for (int step = 0; step < 400; step++) {
            solmove h = kl_hint(P);
            if (h.kind != SM_NONE && !kl_legal(P, h)) ok = 0;
            int n = all_moves(P, list);
            if (!n) { ends++; break; }
            if (!kl_play(&G, rnd(3) == 0 && h.kind != SM_NONE ? h : list[rnd(n)])) ok = 0;
            if (!sane(P)) { ok = 0; printf("seed %u step %d: not sane\n", seed, step); break; }
        }
        while (sol_undo(&G)) {}
        if (memcmp(&G.pos, &start, sizeof start) || G.n || G.moves) ok = 0;
    }
    CHECK(ok);
    printf("ok   300 random games (%d with no move left)\n", ends);
}

/* ---- a deal solved: a depth-first search (draw one), then the moves played again from the script ---------------- */
#define TABLE (1 << 21)
#define DEPTH 2000
static uint64_t *seen;
static solmove path[DEPTH];
static solpos *step;                            /* the positions down the search, one a depth (off the stack) */
static int nodes;

static uint64_t key(const solpos *p)
{
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < KL_PILES; i++) {
        h = (h ^ (p->n[i] * 64u + p->down[i])) * 1099511628211ull;
        for (int k = 0; k < p->n[i]; k++) h = (h ^ p->c[i][k]) * 1099511628211ull;
    }
    return h | 1;
}

static int visit(const solpos *p)               /* 0: seen before */
{
    uint64_t k = key(p);
    for (uint32_t i = (uint32_t)k & (TABLE - 1);; i = (i + 1) & (TABLE - 1)) {
        if (seen[i] == k) return 0;
        if (!seen[i]) { seen[i] = k; return 1; }
    }
}

static int done(const solpos *p)                /* the rest would go up by itself */
{
    if (p->n[KL_STOCK] || p->n[KL_WASTE]) return 0;
    for (int c = 0; c < 7; c++)
        if (p->down[c]) return 0;
    return 1;
}

/* the moves worth trying from p, into list (256 at most), in the order tried */
static int moves_from(const solpos *p, solmove *list)
{
    int n = 0;
    for (int a = 0; a < KL_PILES; a++) {        /* up to the foundations */
        int t = p->kind[a] == SP_TAB || a == KL_WASTE ? sol_top(p, a) : CARD_NONE;
        if (t != CARD_NONE && sol_found_for(p, t) >= 0) list[n++] = sol_mv(SM_MOVE, a, sol_found_for(p, t), 1);
    }
    for (int a = 0; a < 7; a++)                 /* runs that uncover a card (or free one for a foundation) */
        for (int i = p->down[a]; i < p->n[a]; i++) {
            int useful = i == p->down[a] ? p->down[a] > 0 : sol_found_for(p, p->c[a][i - 1]) >= 0;
            for (int b = 0; b < 7 && useful; b++)
                if (b != a && kl_legal(p, sol_mv(SM_MOVE, a, b, p->n[a] - i)))
                    list[n++] = sol_mv(SM_MOVE, a, b, p->n[a] - i);
        }
    for (int b = 0; b < 7; b++)                 /* the waste's card down */
        if (kl_legal(p, sol_mv(SM_MOVE, KL_WASTE, b, 1))) list[n++] = sol_mv(SM_MOVE, KL_WASTE, b, 1);
    solmove s = kl_rules.stock(p);
    if (s.kind != SM_NONE) list[n++] = s;
    for (int a = KL_FOUND; a < KL_STOCK; a++)   /* a card back down from a foundation */
        for (int b = 0; b < 7; b++)
            if (kl_legal(p, sol_mv(SM_MOVE, a, b, 1))) list[n++] = sol_mv(SM_MOVE, a, b, 1);
    return n;
}

/* depth first, iterative (the stacks on the heap: deep searches need no big stack): step[d] the position at depth
 * d, list + 256 d its moves, next[d] the one to try next; the moves found in path; their count (-1: none) */
static solmove *list;
static int count[DEPTH], next[DEPTH];

static int open_at(int d)                       /* the moves from step[d]; 0: cut (too deep, too long, seen) */
{
    if (d >= DEPTH - 10 || ++nodes > 400000 || !visit(&step[d])) return 0;
    count[d] = moves_from(&step[d], list + 256 * d);
    next[d] = 0;
    return 1;
}

static int search(const solpos *start)
{
    step[0] = *start;
    if (done(&step[0])) return 0;
    if (!open_at(0)) return -1;
    for (int d = 0; d >= 0;) {
        if (next[d] >= count[d]) { d--; continue; }
        solmove m = list[256 * d + next[d]++];
        step[d + 1] = step[d];
        sol_apply(&step[d + 1], m);
        path[d] = m;
        if (done(&step[d + 1])) return d + 1;
        if (open_at(d + 1)) d++;
    }
    return -1;
}

static void test_solved(void)
{
    seen = calloc(TABLE, sizeof *seen);
    step = malloc((DEPTH + 1) * sizeof *step);
    list = malloc((size_t)DEPTH * 256 * sizeof *list);
    if (!seen || !step || !list) return;
    int solved = 0;
    static char script[DEPTH * 8];
    for (uint32_t seed = 1; seed <= 12 && solved < 3; seed++) {
        kl_new(&G, KL_DRAW1, seed);
        memset(seen, 0, TABLE * sizeof *seen);
        nodes = 0;
        int n = search(&G.pos);
        if (n < 0) continue;
        solpos q = G.pos;                       /* the moves in the notation */
        script[0] = 0;
        for (int k = 0; k < n; k++) {
            char m[8];
            sol_write(&q, path[k], m);
            sol_apply(&q, path[k]);
            strcat(script, k ? " " : "");
            strcat(script, m);
        }
        kl_new(&H, KL_DRAW1, seed);
        CHECK(kl_script(&H, script));
        CHECK(kl_won(&H.pos) && H.moves == n && H.n == n + (52 - sol_on_found(&q)));
        if (!solved) printf("ok   seed %u solved in %d moves: %s\n", seed, n, script);
        solved++;
        while (sol_undo(&H)) {}
        CHECK(!memcmp(&H.pos, &H.start, sizeof H.pos));
    }
    CHECK(solved >= 2);
    free(seen);
    free(step);
    free(list);
}

/* ---- the levels: a deal from its cards is the shuffled deal; every level won by the search ---------------------- */
static void test_levels(void)
{
    uint8_t deck[CARDS_MAX];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, 77);
    kl_new(&G, KL_DRAW3, 77);
    kl_deal_cards(&H, KL_DRAW3, deck);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(lc_levels("klondike", kl_levels, KL_LEVELS, 52, kl_deal_cards, &kl_search, 300000) == KL_LEVELS);
}

int main(void)
{
    test_deal();
    test_moves();
    test_foundations();
    test_stock();
    test_finish();
    test_random();
    test_solved();
    test_levels();
    printf("klondike: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
