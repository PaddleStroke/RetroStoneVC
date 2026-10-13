/*
 * The AI kit (src/ai.c) and the bots' registry (src/bots.c) on toy games: tic-tac-toe (alpha-beta: perfect play,
 * with and without the table, copy-make and make/unmake the same; UCT never loses to a random player), Nim (the
 * winning take), a gamble against a sure thing (expectiminimax: chance nodes), a hidden card (determinisation: the
 * search must not peek), three players (UCT's rewards each), the budgets, the cancel, the random generator,
 * REGISTER_BOT.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* ---- tic-tac-toe ------------------------------------------------------------------------------------------------- */
typedef struct { int8_t cell[9]; int n; } ttt;

static int ttt_line(const ttt *t)               /* the player with three in a row, or -1 */
{
    static const int L[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
    for (int i = 0; i < 8; i++)
        if (t->cell[L[i][0]] >= 0 && t->cell[L[i][0]] == t->cell[L[i][1]] && t->cell[L[i][1]] == t->cell[L[i][2]])
            return t->cell[L[i][0]];
    return -1;
}
static int ttt_moves(const void *st, int *out)
{
    const ttt *t = st;
    int n = 0;
    if (ttt_line(t) >= 0) return 0;
    for (int i = 0; i < 9; i++)
        if (t->cell[i] < 0) out[n++] = i;
    return n;
}
static void ttt_make(void *st, int m) { ttt *t = st; t->cell[m] = (int8_t)(t->n++ & 1); }
static void ttt_unmake(void *st, int m) { ttt *t = st; t->cell[m] = -1; t->n--; }
static int ttt_side(const void *st) { return ((const ttt *)st)->n & 1; }
static int ttt_over(const void *st, int *score)
{
    const ttt *t = st;
    int w = ttt_line(t);
    if (w >= 0) { *score = w == (t->n & 1) ? ai_win(0) : -ai_win(0); return 1; }
    if (t->n == 9) { *score = 0; return 1; }
    return 0;
}
static int ttt_eval(const void *st) { (void)st; return 0; }
static uint64_t ttt_hash(const void *st)
{
    const ttt *t = st;
    uint64_t h = 0;
    for (int i = 0; i < 9; i++) h = h * 3 + (uint64_t)(t->cell[i] + 1);
    return ai_mix(h, 0);
}
static void ttt_new(ttt *t) { memset(t->cell, -1, sizeof t->cell); t->n = 0; }

static const ai_game TTT = {.size = sizeof(ttt), .moves = ttt_moves, .make = ttt_make, .side = ttt_side,
                           .over = ttt_over, .eval = ttt_eval, .hash = ttt_hash};

/* the score of a position with full search (the plain ones) */
static int ttt_value(const ai_game *g, ttt *t, int tt_bits)
{
    ai_opts o = {.budget = {.depth = 9}, .tt_bits = tt_bits};
    ai_stats s;
    ai_ab(g, t, &o, &s);
    return s.score;
}

/* plays a game: player 0 by search f0 (or random if NULL), player 1 by f1; the winner, -1 a draw */
typedef int (*chooser)(const ttt *t, ai_rng *r);
static int ttt_ab(const ttt *t, ai_rng *r)
{
    ai_opts o = {.budget = {.depth = 9}, .rng = r, .tt_bits = 12};
    return ai_ab(&TTT, t, &o, NULL);
}
static int ttt_uct(const ttt *t, ai_rng *r)
{
    ai_opts o = {.budget = {.nodes = 3000}, .rng = r};
    return ai_mcts(&TTT, t, &o, NULL);
}
static int ttt_rand(const ttt *t, ai_rng *r) { return ai_random_move(&TTT, t, r); }
static int ttt_play(chooser f0, chooser f1, ai_rng *r)
{
    ttt t;
    ttt_new(&t);
    int s;
    while (!ttt_over(&t, &s)) ttt_make(&t, (t.n & 1 ? f1 : f0)(&t, r));
    return ttt_line(&t);
}

/* ---- Nim: take 1 to 3 of n, who takes the last wins ------------------------------------------------------------- */
typedef struct { int left, side; } nim;
static int nim_moves(const void *st, int *out)
{
    const nim *p = st;
    int n = 0;
    for (int k = 1; k <= 3 && k <= p->left; k++) out[n++] = k;
    return n;
}
static void nim_make(void *st, int k) { nim *p = st; p->left -= k; p->side ^= 1; }
static int nim_side(const void *st) { return ((const nim *)st)->side; }
static int nim_over(const void *st, int *score)
{
    if (((const nim *)st)->left) return 0;
    *score = -ai_win(0);                        /* the player who took the last one won */
    return 1;
}
static int nim_eval(const void *st) { (void)st; return 0; }
static const ai_game NIM = {.size = sizeof(nim), .moves = nim_moves, .make = nim_make, .side = nim_side,
                           .over = nim_over, .eval = nim_eval};

/* ---- a gamble: take `sure` points, or throw a die (faces 1..6, weights w) ------------------------------------- */
typedef struct { int stage, points, sure; int w6; } gamble;
static int gb_moves(const void *st, int *out)
{
    const gamble *g = st;
    if (g->stage) return 0;
    out[0] = 0;                                 /* sure */
    out[1] = 1;                                 /* throw */
    return 2;
}
static int gb_chance(const void *st, int *out, int *w)
{
    const gamble *g = st;
    if (g->stage != 1) return 0;
    for (int i = 0; i < 6; i++) { out[i] = 10 + i + 1; w[i] = i == 5 ? g->w6 : 1; }
    return 6;
}
static void gb_make(void *st, int m)
{
    gamble *g = st;
    if (m == 0) { g->points = g->sure; g->stage = 2; }
    else if (m == 1) g->stage = 1;
    else { g->points = m - 10; g->stage = 2; }
}
static int gb_side(const void *st) { (void)st; return 0; }
static int gb_over(const void *st, int *score)
{
    const gamble *g = st;
    if (g->stage != 2) return 0;
    *score = g->points * 100;
    return 1;
}
static int gb_eval(const void *st) { (void)st; return 0; }
static int gb_result(const void *st, float *rw)
{
    const gamble *g = st;
    if (g->stage != 2) return 0;
    rw[0] = g->points / 6.0f;
    rw[1] = 1 - rw[0];
    return 1;
}
static const ai_game GAMBLE = {.size = sizeof(gamble), .moves = gb_moves, .make = gb_make, .side = gb_side,
                              .over = gb_over, .eval = gb_eval, .chance = gb_chance};

/* ---- a hidden card: the other player holds 0, 1 or 2; guess it ------------------------------------------------- */
/* the seat knows it isn't 2 and, from the play so far, that it is 1 four times out of five: its determinise() deals
 * that. The real card (in the state the search gets) is 0: a search that peeked would guess 0. */
typedef struct { int card, guessed, guess; } hidden;
static int hd_moves(const void *st, int *out)
{
    if (((const hidden *)st)->guessed) return 0;
    for (int i = 0; i < 3; i++) out[i] = i;
    return 3;
}
static void hd_make(void *st, int m) { hidden *h = st; h->guess = m; h->guessed = 1; }
static int hd_side(const void *st) { (void)st; return 0; }
static int hd_result(const void *st, float *rw)
{
    const hidden *h = st;
    if (!h->guessed) return 0;
    rw[0] = h->guess == h->card;
    rw[1] = 1 - rw[0];
    return 1;
}
static int determinised;
static void hd_det(void *st, int seat, ai_rng *r)
{
    (void)seat;
    determinised++;
    ((hidden *)st)->card = ai_rand_n(r, 5) ? 1 : 0;
}
static const ai_game HIDDEN = {.size = sizeof(hidden), .moves = hd_moves, .make = hd_make, .side = hd_side,
                              .result = hd_result, .determinise = hd_det};

/* ---- three players: each in turn takes the prize (and wins) or passes; after 6 passes nobody wins ------------- */
typedef struct { int turn, passes, winner; } three;
static int th_moves(const void *st, int *out)
{
    const three *t = st;
    if (t->winner >= 0 || t->passes >= 6) return 0;
    out[0] = 0;                                 /* pass */
    out[1] = 1;                                 /* take */
    return 2;
}
static void th_make(void *st, int m)
{
    three *t = st;
    if (m) t->winner = t->turn;
    else t->passes++;
    t->turn = (t->turn + 1) % 3;
}
static int th_side(const void *st) { return ((const three *)st)->turn; }
static int th_result(const void *st, float *rw)
{
    const three *t = st;
    if (t->winner < 0 && t->passes < 6) return 0;
    for (int p = 0; p < 3; p++) rw[p] = t->winner < 0 ? 1.0f / 3 : p == t->winner;
    return 1;
}
static const ai_game THREE = {.size = sizeof(three), .moves = th_moves, .make = th_make, .side = th_side,
                             .result = th_result, .players = 3};

/* ---- a game that never ends (for the budgets): 30 moves a turn, any depth -------------------------------------- */
typedef struct { uint64_t h; int ply; } endless;
static int en_moves(const void *st, int *out) { (void)st; for (int i = 0; i < 30; i++) out[i] = i; return 30; }
static void en_make(void *st, int m) { endless *e = st; e->h = ai_mix(e->h, (uint64_t)m); e->ply++; }
static int en_side(const void *st) { return ((const endless *)st)->ply & 1; }
static int en_over(const void *st, int *score) { (void)st; (void)score; return 0; }
static int en_eval(const void *st) { return (int)(((const endless *)st)->h % 200) - 100; }
static uint64_t en_hash(const void *st) { return ((const endless *)st)->h; }
static int en_result(const void *st, float *rw) { (void)st; (void)rw; return 0; }
static const ai_game ENDLESS = {.size = sizeof(endless), .moves = en_moves, .make = en_make, .side = en_side,
                               .over = en_over, .eval = en_eval, .hash = en_hash, .result = en_result,
                               .cutoff = 20};

/* ---- the registry ------------------------------------------------------------------------------------------------ */
static int toy_think(int seat, int level, char *move, int cap)
{
    snprintf(move, (size_t)cap, "%d-%d", seat, level);
    return 1;
}
static const bot toy = {.think = toy_think};
REGISTER_BOT(toy, &toy)

int main(void)
{
    char name[128];
    ai_rng r;

    /* the random generator: in range, repeatable, roughly even */
    {
        ai_rng a, b;
        ai_seed(&a, 42);
        ai_seed(&b, 42);
        int same = 1, in = 1, hist[6] = {0};
        for (int i = 0; i < 60000; i++) {
            int x = ai_rand_n(&a, 6);
            same &= x == ai_rand_n(&b, 6);
            in &= x >= 0 && x < 6;
            if (x >= 0 && x < 6) hist[x]++;
            double f = ai_rand_f(&a);
            ai_rand_f(&b);
            in &= f >= 0 && f < 1;
        }
        int even = 1;
        for (int i = 0; i < 6; i++) even &= hist[i] > 9500 && hist[i] < 10500;
        check("random: the same seed repeats, in range, even", same && in && even);
    }

    /* tic-tac-toe: the start is a draw; a won position found; table or not, copy-make or unmake: the same */
    {
        ttt t;
        ttt_new(&t);
        ai_game un = TTT;
        un.unmake = ttt_unmake;
        check("tic-tac-toe: a draw from the start (no table)", ttt_value(&TTT, &t, 0) == 0);
        check("tic-tac-toe: a draw from the start (table)", ttt_value(&TTT, &t, 14) == 0);
        check("tic-tac-toe: a draw from the start (make/unmake)", ttt_value(&un, &t, 0) == 0);
        int same = 1;
        ai_seed(&r, 3);
        for (int k = 0; k < 300 && same; k++) {  /* random positions: the three agree */
            ttt p;
            ttt_new(&p);
            int s, plies = ai_rand_n(&r, 6);
            for (int i = 0; i < plies && !ttt_over(&p, &s); i++) ttt_make(&p, ai_random_move(&TTT, &p, &r));
            if (ttt_over(&p, &s)) continue;
            int a = ttt_value(&TTT, &p, 0), b = ttt_value(&TTT, &p, 10), c = ttt_value(&un, &p, 0);
            same = a == b && b == c;
        }
        check("tic-tac-toe: 300 positions, the same score three ways", same);
        /* x at 0 and 1, o at 3 and 4, x to move: 2 wins at once (and the score says a win at ply 1) */
        ttt_new(&t);
        ttt_make(&t, 0); ttt_make(&t, 3); ttt_make(&t, 1); ttt_make(&t, 4);
        ai_opts o = {.budget = {.depth = 9}};
        ai_stats s;
        int m = ai_ab(&TTT, &t, &o, &s);
        snprintf(name, sizeof name, "tic-tac-toe: the win at once (%d, score %d)", m, s.score);
        check(name, m == 2 && s.score == ai_win(0) - 1);
        int lost = 0, draws = 0;
        ai_seed(&r, 11);
        for (int k = 0; k < 100; k++) {
            int w0 = ttt_play(ttt_ab, ttt_rand, &r), w1 = ttt_play(ttt_rand, ttt_ab, &r);
            lost += (w0 == 1) + (w1 == 0);
        }
        for (int k = 0; k < 20; k++) draws += ttt_play(ttt_ab, ttt_ab, &r) < 0;
        check("tic-tac-toe: alpha-beta never loses to random (200 games)", !lost);
        check("tic-tac-toe: alpha-beta against itself: always a draw", draws == 20);
        lost = 0;
        for (int k = 0; k < 100; k++) {
            int w0 = ttt_play(ttt_uct, ttt_rand, &r), w1 = ttt_play(ttt_rand, ttt_uct, &r);
            lost += (w0 == 1) + (w1 == 0);
        }
        snprintf(name, sizeof name, "tic-tac-toe: UCT (3000 playouts) loses at most 2 of 200 to random (%d)", lost);
        check(name, lost <= 2);
    }

    /* Nim: from 10, take 2 (leave a multiple of 4); from 8 every take loses */
    {
        nim p = {10, 0};
        ai_opts o = {.budget = {.depth = 20}};
        ai_stats s;
        check("nim 10: alpha-beta takes 2", ai_ab(&NIM, &p, &o, &s) == 2 && s.score > AI_WIN / 2);
        p.left = 8;
        ai_ab(&NIM, &p, &o, &s);
        check("nim 8: alpha-beta knows it is lost", s.score < -AI_WIN / 2);
        p.left = 7;
        ai_opts u = {.budget = {.nodes = 5000}};
        check("nim 7: UCT takes 3", ai_mcts(&NIM, &p, &u, NULL) == 3);
    }

    /* chance nodes: a die averages 3.5; with a loaded six (weight 5 of 10) 4.5 */
    {
        gamble g = {0, 0, 3, 1};
        ai_opts o = {.budget = {.depth = 4}};
        ai_stats s;
        check("expectiminimax: a die (3.5) over a sure 3", ai_expecti(&GAMBLE, &g, &o, &s) == 1 && s.score == 350);
        g.sure = 4;
        check("expectiminimax: a sure 4 over a die (3.5)", ai_expecti(&GAMBLE, &g, &o, &s) == 0 && s.score == 400);
        g.w6 = 5;
        check("expectiminimax: a loaded die (4.5) over a sure 4", ai_expecti(&GAMBLE, &g, &o, &s) == 1
                                                                   && s.score == 450);
        g.stage = 1;
        check("expectiminimax: a throw to make is not a decision", ai_expecti(&GAMBLE, &g, &o, NULL) == AI_NONE);
        ai_game ug = GAMBLE;                    /* UCT: the rewards points / 6 */
        ug.result = gb_result;
        gamble h = {0, 0, 3, 1};
        ai_opts u = {.budget = {.nodes = 4000}};
        check("UCT through chance nodes: a die (3.5) over a sure 3", ai_mcts(&ug, &h, &u, NULL) == 1);
        h.sure = 4;
        check("UCT through chance nodes: a sure 4 over a die (3.5)", ai_mcts(&ug, &h, &u, NULL) == 0);
    }

    /* hidden information: the search guesses from what the seat knows, not from the real card */
    {
        hidden h = {0, 0, 0};
        ai_seed(&r, 5);
        ai_opts o = {.budget = {.nodes = 4000}, .rng = &r};
        determinised = 0;
        int m = ai_mcts(&HIDDEN, &h, &o, NULL);
        snprintf(name, sizeof name, "determinisation: guesses 1 from its knowledge, not the real 0 (%d, dealt %d)", m,
                 determinised);
        check(name, m == 1 && determinised >= 3000);
    }

    /* three players: player 0 takes the prize at once */
    {
        three t = {0, 0, -1};
        ai_opts o = {.budget = {.nodes = 3000}};
        check("three players: UCT takes the prize", ai_mcts(&THREE, &t, &o, NULL) == 1);
        t.turn = 2;
        check("three players: UCT (player 2) takes it too", ai_mcts(&THREE, &t, &o, NULL) == 1);
    }

    /* budgets and cancel on a game without an end */
    {
        endless e = {1, 0};
        ai_stats s;
        ai_opts o = {.budget = {.nodes = 20000}, .tt_bits = 12};
        int m = ai_ab(&ENDLESS, &e, &o, &s);
        snprintf(name, sizeof name, "alpha-beta: the node cap (%ld nodes, depth %d)", s.nodes, s.depth);
        check(name, m >= 0 && m < 30 && s.nodes <= 20000 && s.stopped && s.depth >= 2);
        o = (ai_opts){.budget = {.ms = 60}, .tt_bits = 16, .aspiration = 30};
        m = ai_ab(&ENDLESS, &e, &o, &s);
        snprintf(name, sizeof name, "alpha-beta: the time cap 60 ms (%.1f ms, depth %d)", s.ms, s.depth);
        check(name, m >= 0 && s.ms < 80 && s.stopped);
        o = (ai_opts){.budget = {.ms = 60}};
        m = ai_mcts(&ENDLESS, &e, &o, &s);
        snprintf(name, sizeof name, "UCT: the time cap 60 ms (%.1f ms, %ld playouts)", s.ms, s.nodes);
        check(name, m >= 0 && s.ms < 80 && s.stopped);
        ai_cancel();
        check("cancel: seen", ai_cancelled());
        o = (ai_opts){.budget = {.ms = 5000}};
        double t0 = ai_now_ms();
        m = ai_ab(&ENDLESS, &e, &o, &s);
        int m2 = ai_mcts(&ENDLESS, &e, &o, NULL);
        snprintf(name, sizeof name, "cancel: both searches stop at once with a legal move (%.1f ms)", ai_now_ms() - t0);
        check(name, m >= 0 && m < 30 && m2 >= 0 && m2 < 30 && ai_now_ms() - t0 < 50);
        ai_cancel_clear();
        check("cancel: cleared", !ai_cancelled());
    }

    /* the levels: harder is deeper and quieter */
    {
        ai_budget e = ai_level_budget(AI_EASY), m = ai_level_budget(AI_MEDIUM), h = ai_level_budget(AI_HARD);
        check("levels: easy < medium < hard (nodes), noise falls, hard about 1 s at most",
              e.nodes < m.nodes && m.nodes < h.nodes && e.noise > m.noise && h.noise == 0 && h.ms <= 1000);
        check("levels: out of range clamps",
              ai_level_budget(-3).nodes == e.nodes && ai_level_budget(9).nodes == h.nodes);
    }

    /* the registry: REGISTER_BOT's constructor ran */
    {
        const bot *b = bot_find("toy");
        char m[BOT_MOVE_CAP] = "";
        check("REGISTER_BOT: found by id, think() called", b && b->think(1, AI_HARD, m, sizeof m) && !strcmp(m, "1-2"));
        int found = 0;
        for (int i = 0; i < bot_count(); i++) found += !strcmp(bot_id(i), "toy");
        check("the registry lists it once; an unknown id: NULL", found == 1 && !bot_find("nothing"));
    }
    printf(fails ? "ai kit: %d FAILED\n" : "ai kit: all passed\n", fails);
    return fails != 0;
}
