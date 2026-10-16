/*
 * Bao's bot (bots.h): negamax alpha-beta (ai_ab) over the rules' position, copy-make (a move's relay sowing is too
 * long to undo by hand). The evaluation counts the seeds each player holds (the game is won by taking them all, or
 * the inner row), the seeds in the inner row (the row under attack, and the one that must not empty), the captures
 * the side to move has (a capture must be taken: the more, the better) and how many moves each side keeps.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: "d2+", a pit and a way (bao.h).
 */
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/bao.h"

static int a_moves(const void *st, int *out) { return bao_moves(st, out); }
static void a_make(void *st, int m) { bao_apply(st, m, NULL); }
static int a_side(const void *st) { return ((const bao_pos *)st)->turn; }

static int a_over(const void *st, int *score)
{
    const bao_pos *p = st;
    if (p->winner < 0) return 0;
    if (p->winner == 2) { *score = 0; return 1; }
    int margin = bao_seeds(p, p->winner) / 4;
    *score = p->winner == p->turn ? ai_win(margin) : -ai_win(margin);
    return 1;
}

static int captures(const bao_pos *p)
{
    int n = 0;
    for (int m = 0; m < 2 * BAO_RING; m++) n += bao_capturing(p, m);
    return n;
}

/* the side to move's view: 100 a seed */
static int a_eval(const void *st)
{
    const bao_pos *p = st;
    int me = p->turn, op = !me;
    bao_pos q = *p;
    q.turn = (uint8_t)op;
    int v = 100 * (bao_seeds(p, me) - bao_seeds(p, op)) + 12 * (bao_inner(p, me) - bao_inner(p, op));
    v += 25 * captures(p) - 15 * captures(&q);
    int empty_me = 0, empty_op = 0;             /* empty inner pits: room the other side's sowing cannot hit */
    for (int i = 0; i < 8; i++) empty_me += !p->pit[me][i], empty_op += !p->pit[op][i];
    return v + 6 * (empty_me - empty_op);
}

static uint64_t a_hash(const void *st)
{
    const bao_pos *p = st;
    uint64_t h = p->turn;
    for (int k = 0; k < 2; k++) {
        uint64_t w[2];
        memcpy(w, p->pit[k], sizeof w);
        h = ai_mix(ai_mix(h, w[0]), w[1]);
    }
    return h;
}

static int a_order(const void *st, int m)       /* captures first, the biggest catch first */
{
    const bao_pos *p = st;
    int me = p->turn, from = bao_mv_pit(m), n = p->pit[me][from];
    if (!bao_capturing(p, m)) return 0;
    int last = (from + (bao_mv_cw(m) ? n : -n)) & 15;
    return 100 + 10 * p->pit[!me][7 - last];
}

static const ai_game ADAPTER = {
    .size = sizeof(bao_pos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* the move for the position p, at a level; -1: none */
int bao_choose(const bao_pos *p, int level, uint64_t seed, ai_stats *stats)
{
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 16};
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 1; o.budget.nodes = 2000; o.budget.noise = 400; break;
    case AI_MEDIUM: o.budget.depth = 3; o.budget.nodes = 20000; o.budget.noise = 60; break;
    default:        o.budget.depth = 0; o.budget.nodes = 120000; break;   /* about 0.05 s on a PC */
    }
    if (stats) memset(stats, 0, sizeof *stats);
    int mv[2 * BAO_RING], n = bao_moves(p, mv);
    if (n <= 1) return n ? mv[0] : -1;
    int m = ai_ab(&ADAPTER, p, &o, stats);
    return m < 0 ? mv[0] : m;
}

static int think(int seat, int level, char *move, int cap)
{
    const bao_game *g = bao_live();
    bao_pos p = g->pos[g->n];                   /* the copy, at entry: nothing else is read */
    if (p.winner >= 0 || p.turn != seat || cap < 4) return 0;
    int m = bao_choose(&p, level, ai_session_seed(), NULL);
    if (m < 0 || ai_cancelled()) return 0;
    bao_name(&p, m, move);
    return 1;
}

static const bot bao_bot = {.think = think, .play = bao_show};
REGISTER_BOT(bao, &bao_bot)
