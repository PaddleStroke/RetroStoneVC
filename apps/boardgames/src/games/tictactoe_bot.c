/*
 * Tic-tac-toe's bot (bots.h), both variants, on the bot's own small form of the position (each player's marks in each
 * small board as 9 bits, the boards claimed, the full ones).
 *   Classic: alpha-beta (ai.h) to the end of the game: hard never loses; medium looks three moves ahead with a little
 *   noise (a fork can catch it); easy looks two ahead with a lot of noise, and one move in four only one ahead (it
 *   then misses a block, as a beginner does).
 *   Ultimate: Monte Carlo tree search (ai_mcts): random playouts, except that a playout takes a small board it can
 *   win at once and blocks one the other player would win (the big board's line first); easy a few hundred
 *   playouts, medium a few thousand, hard as many as the budget allows.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("5", Ultimate "5.1"), played by tictactoe_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/tictactoe.h"

/* the eight lines of a 3 x 3 board as 9-bit masks */
static const uint16_t LINE[8] = {0007, 0070, 0700, 0111, 0222, 0444, 0421, 0124};

typedef struct {
    uint16_t m[2][9];                           /* each player's marks in each small board (classic: board 0) */
    uint16_t big[2], full;                      /* the boards claimed by each player, the full ones nobody won */
    int8_t target;                              /* the board the player to move is sent to, -1 any */
    int8_t winner;                              /* 0, 1; 2 a draw; -1 playing */
    uint8_t side, ultimate, n;
} tts;

static int won(unsigned m)
{
    for (int i = 0; i < 8; i++)
        if ((m & LINE[i]) == LINE[i]) return 1;
    return 0;
}

/* a line the other player cannot block any more (no mark of theirs, no full board in it): one left? */
static int line_left(const tts *s)
{
    for (int i = 0; i < 8; i++) {
        unsigned l = LINE[i];
        if (l & s->full) continue;
        if (!(l & s->big[1]) || !(l & s->big[0])) return 1;
    }
    return 0;
}

static void from_rules(const tt_game *g, tts *s)
{
    memset(s, 0, sizeof *s);
    s->ultimate = (uint8_t)g->ultimate;
    s->side = (uint8_t)tt_turn(g);
    s->n = (uint8_t)g->n;
    for (int c = 0; c < tt_cells(g); c++)
        if (g->cell[c] >= 0) s->m[g->cell[c]][c / 9] |= (uint16_t)(1u << (c % 9));
    for (int b = 0; b < 9; b++) {
        if (g->board[b] == 0 || g->board[b] == 1) s->big[g->board[b]] |= (uint16_t)(1u << b);
        if (g->board[b] == TT_FULL) s->full |= (uint16_t)(1u << b);
    }
    s->target = (int8_t)tt_target(g);
    int st = tt_status(g);
    s->winner = (int8_t)(st == TT_PLAY ? -1 : st == TT_WIN0 ? 0 : st == TT_WIN1 ? 1 : 2);
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int open_board(const tts *s, int b) { return !((s->big[0] | s->big[1] | s->full) >> b & 1); }

static int a_moves(const void *st, int *out)
{
    const tts *s = st;
    int n = 0;
    if (s->winner >= 0) return 0;
    if (!s->ultimate) {
        unsigned used = s->m[0][0] | s->m[1][0];
        for (int q = 0; q < 9; q++)
            if (!(used >> q & 1)) out[n++] = q;
        return n;
    }
    for (int b = 0; b < 9; b++) {
        if ((s->target >= 0 && b != s->target) || !open_board(s, b)) continue;
        unsigned used = s->m[0][b] | s->m[1][b];
        for (int q = 0; q < 9; q++)
            if (!(used >> q & 1)) out[n++] = b * 9 + q;
    }
    return n;
}

static void a_make(void *st, int c)
{
    tts *s = st;
    int p = s->side, b = c / 9, q = c % 9;
    s->m[p][b] |= (uint16_t)(1u << q);
    s->n++;
    s->side ^= 1;
    if (!s->ultimate) {
        if (won(s->m[p][0])) s->winner = (int8_t)p;
        else if (s->n == 9) s->winner = 2;
        return;
    }
    if (won(s->m[p][b])) {
        s->big[p] |= (uint16_t)(1u << b);
        if (won(s->big[p])) { s->winner = (int8_t)p; return; }
    } else if ((s->m[0][b] | s->m[1][b]) == 0777) {
        s->full |= (uint16_t)(1u << b);
    }
    s->target = (int8_t)(open_board(s, q) ? q : -1);
    if (!line_left(s)) s->winner = 2;
}

static int a_side(const void *st) { return ((const tts *)st)->side; }

static int a_over(const void *st, int *score)
{
    const tts *s = st;
    if (s->winner < 0) return 0;
    *score = s->winner == 2 ? 0 : s->winner == s->side ? ai_win(0) : -ai_win(0);
    return 1;
}

static int a_result(const void *st, float *rw)
{
    const tts *s = st;
    if (s->winner < 0) return 0;
    rw[0] = s->winner == 2 ? 0.5f : s->winner == 0;
    rw[1] = s->winner == 2 ? 0.5f : s->winner == 1;
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const tts *s = st;
    return ai_mix(s->m[0][0] | (uint64_t)s->m[1][0] << 9, s->side);
}

/* classic: the lines still open to each player, weighted by the marks in them (the side to move's view) */
static int a_eval(const void *st)
{
    const tts *s = st;
    int me = s->side, v = 0;
    for (int i = 0; i < 8; i++) {
        unsigned a = s->m[me][0] & LINE[i], o = s->m[!me][0] & LINE[i];
        if (a && !o) v += __builtin_popcount(a) == 2 ? 30 : 10;
        if (o && !a) v -= __builtin_popcount(o) == 2 ? 30 : 10;
    }
    return v;
}

/* a small board's square that wins it for p (marks m, the other's o), -1 */
static int winning_square(unsigned m, unsigned o)
{
    for (int i = 0; i < 8; i++) {
        unsigned l = LINE[i], have = m & l;
        if (!(o & l) && __builtin_popcount(have) == 2) return __builtin_ctz(l & ~have);
    }
    return -1;
}

/* Ultimate's playouts: a board won at once when its line on the big board is open, a block of the other's, else at
 * random */
static int a_playout(const void *st, ai_rng *r)
{
    const tts *s = st;
    int mv[81], n = a_moves(s, mv), me = s->side;
    if (!n) return AI_NONE;
    int block = -1;
    for (int b = 0; b < 9; b++) {
        if ((s->target >= 0 && b != s->target) || !open_board(s, b)) continue;
        int w = winning_square(s->m[me][b], s->m[!me][b]);
        if (w >= 0) return b * 9 + w;
        if (block < 0 && (w = winning_square(s->m[!me][b], s->m[me][b])) >= 0) block = b * 9 + w;
    }
    if (block >= 0 && ai_rand_n(r, 2)) return block;
    return mv[ai_rand_n(r, n)];
}

static const ai_game CLASSIC = {
    .size = sizeof(tts), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .unit = 10,
};

static const ai_game ULTIMATE = {
    .size = sizeof(tts), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .result = a_result,
    .playout = a_playout, .players = 2,
};

/* the cell for the position (its side to move), at a level; -1: none */
int ttb_choose(const tt_game *g, int level, uint64_t seed, ai_stats *stats)
{
    tts s;
    from_rules(g, &s);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(&s) ^ (uint64_t)s.n << 40 ^ (uint64_t)s.big[0] << 48));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng};
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    if (!s.ultimate) {
        o.tt_bits = 12;
        switch (level) {
        case AI_EASY:   o.budget.depth = ai_rand_n(&rng, 4) ? 2 : 1; o.budget.noise = 300; break;
        case AI_MEDIUM: o.budget.depth = 3; o.budget.noise = 150; break;
        default:        o.budget.depth = 9; o.budget.noise = 0; break;
        }
        return ai_ab(&CLASSIC, &s, &o, stats);
    }
    switch (level) {
    case AI_EASY:   o.budget.nodes = 150; break;
    case AI_MEDIUM: o.budget.nodes = 3000; break;
    default:        o.budget.nodes = 60000; break;     /* about 0.3 s on a PC */
    }
    return ai_mcts(&ULTIMATE, &s, &o, stats);
}

static int think(int seat, int level, char *move, int cap)
{
    tt_game g = *tictactoe_live();              /* the copy, at entry: nothing else is read */
    if (tt_status(&g) != TT_PLAY || tt_turn(&g) != seat || cap < 4) return 0;
    int c = ttb_choose(&g, level, ai_session_seed(), NULL);
    if (c < 0 || ai_cancelled()) return 0;
    if (g.ultimate) snprintf(move, (size_t)cap, "%d.%d", c / 9 + 1, c % 9 + 1);
    else snprintf(move, (size_t)cap, "%d", c + 1);
    return 1;
}

static const bot tictactoe_bot = {.think = think, .play = tictactoe_show};
REGISTER_BOT(tictactoe, &tictactoe_bot)
