/*
 * Xiangqi's bot (bots.h): alpha-beta (ai.h) over the rules' moves (xiangqi_rules.c: the flying general too), a
 * transposition table, the captures first (most valuable victim, least valuable attacker), killers and history, a
 * quiescence on the captures (every evasion when in check). The evaluation: material (a soldier worth twice as much
 * over the river), the soldiers' advance towards the palace, the horses to the centre and forwards, the chariots'
 * open lines, the cannons on the middle file while the board is full (fewer screens later: the horse gains on it),
 * the general at home. A player with no move loses (mate or stalemate); a repetition is a draw (the rules').
 * Runs on the worker thread: it copies the position (and the keys of the positions before it) at entry and touches
 * nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("h2e2"), played by xiangqi_show().
 */
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/chessbots.h"
#include "games/xiangqi.h"

typedef struct {
    xpos p;
    uint64_t key;
    uint8_t end, ply;
    cb_hist h;
} xbpos;

static const int VAL[8] = {0, 0, 200, 200, 400, 900, 450, 100};   /* general advisor elephant horse chariot cannon
                                                                    * soldier */

static uint64_t key_of(const xpos *p) { return cb_board_key(p->sq, XQ_PTS, 0x8a1ull + p->side); }
static int enc(xmove m) { return m.from | m.to << 7 | m.cap << 14; }
static xmove dec(int x)
{
    xmove m = {(uint8_t)(x & 127), (uint8_t)(x >> 7 & 127), (uint8_t)(x >> 14 & 15)};
    return m;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static void a_make(void *st, int mv)
{
    xbpos *s = st;
    s->ply++;
    if (mv == CB_END) {
        s->end = 1;
        s->key ^= 0xe4d;
        return;
    }
    cb_hist_push(&s->h, s->key);
    xq_make(&s->p, dec(mv));
    if (!s->p.quiet) cb_hist_clear(&s->h);
    s->key = key_of(&s->p);
}

static int a_side(const void *st) { return ((const xbpos *)st)->p.side; }

static int a_over(const void *st, int *score)
{
    const xbpos *s = st;
    if (s->end) { *score = -ai_win(0); return 1; }   /* no move: mated or stalemated, lost */
    if (s->ply && (s->p.quiet >= XQ_QUIET_PLIES || cb_hist_seen(&s->h, s->key, s->p.quiet, 2))) {
        *score = 0;
        return 1;
    }
    return 0;
}

static int a_moves(const void *st, int *out)
{
    xmove l[XQ_MAX_MOVES];
    int n = xq_legal(&((const xbpos *)st)->p, l);
    if (!n) { out[0] = CB_END; return 1; }
    for (int i = 0; i < n; i++) out[i] = enc(l[i]);
    return n;
}

static int a_noisy(const void *st, int *out)
{
    const xpos *p = &((const xbpos *)st)->p;
    xmove l[XQ_MAX_MOVES];
    int n = xq_legal(p, l), k = 0;
    if (!n) { out[0] = CB_END; return 1; }
    int all = xq_in_check(p, p->side);
    for (int i = 0; i < n; i++)
        if (all || l[i].cap) out[k++] = enc(l[i]);
    return k;
}

static uint64_t a_hash(const void *st) { return ((const xbpos *)st)->key; }

static int a_order(const void *st, int mv)
{
    if (mv == CB_END) return 0;
    const xbpos *s = st;
    xmove m = dec(mv);
    if (!m.cap) return 0;
    int mover = XQ_TYPE(s->p.sq[m.from]), victim = XQ_TYPE(m.cap);
    return cb_mvvlva(victim == XQ_GENERAL ? 3000 : VAL[victim], mover == XQ_GENERAL ? 1000 : VAL[mover]);
}

/* ---- the evaluation ---------------------------------------------------------------------------------------------- */
/* the open points a chariot sees along its file and rank */
static int reach(const xpos *p, int s)
{
    static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int n = 0;
    for (int d = 0; d < 4; d++)
        for (int x = XQ_X(s) + D[d][0], y = XQ_Y(s) + D[d][1]; x >= 0 && x < XQ_W && y >= 0 && y < XQ_H;
             x += D[d][0], y += D[d][1]) {
            n++;
            if (p->sq[XQ_PT(x, y)]) break;
        }
    return n;
}

static int a_eval(const void *st)
{
    const xbpos *s = st;
    const xpos *p = &s->p;
    int us = p->side, v[2] = {0, 0}, men = 0;
    for (int q = 0; q < XQ_PTS; q++) men += p->sq[q] != 0;
    int full = men >= 22, late = men <= 12;
    for (int q = 0; q < XQ_PTS; q++) {
        int pc = p->sq[q];
        if (!pc) continue;
        int c = XQ_OWNER(pc), t = XQ_TYPE(pc), x = XQ_X(q), rr = c ? 9 - XQ_Y(q) : XQ_Y(q), mid = 4 - cb_abs(x - 4);
        v[c] += VAL[t];
        switch (t) {
        case XQ_SOLDIER:
            if (rr >= 5) {                      /* over the river: twice the worth, more near the palace */
                v[c] += 90 + (rr >= 6 && rr <= 8 ? 20 + (x >= 3 && x <= 5 ? 25 : 0) : 0) - (rr == 9 ? 60 : 0);
                if (late) v[c] += 30;
            }
            break;
        case XQ_HORSE:
            v[c] += 6 * mid + (rr >= 3 && rr <= 7 ? 15 : 0) - (rr == 0 ? 15 : 0) + (late ? 40 : 0);
            break;
        case XQ_CHARIOT: v[c] += 4 * reach(p, q); break;
        case XQ_CANNON:
            v[c] += (full && x == 4 ? 25 : 0) - (late ? 60 : 0);
            break;
        case XQ_GENERAL: v[c] += x == 4 && rr == 0 ? 10 : rr ? -10 : 0; break;
        }
    }
    return v[us] - v[!us] + 10;
}

static const ai_game ADAPTER = {
    .size = sizeof(xbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};
static const ai_game ADAPTER_EASY = {           /* easy: no quiescence (a longer exchange escapes it) */
    .size = sizeof(xbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

static const cb_levels LEVELS = {
    .nodes = {[AI_EASY] = 8000, [AI_MEDIUM] = 50000, [AI_HARD] = 220000},    /* hard: about 0.3 s on a PC */
    .depth = {[AI_EASY] = 2, [AI_MEDIUM] = 4, [AI_HARD] = 0},
    .noise = {[AI_EASY] = 100, [AI_MEDIUM] = 25, [AI_HARD] = 0},
    .tt_bits = 18,
};

void xqb_root(const xgame *g, void *root)
{
    xbpos *s = root;
    memset(s, 0, sizeof *s);
    s->p = *xq_pos(g);
    s->key = key_of(&s->p);
    int back = cb_min(s->p.quiet, CB_HIST), from = g->ply - back < 0 ? 0 : g->ply - back;
    for (int i = from; i < g->ply; i++) cb_hist_push(&s->h, key_of(&g->pos[i]));
}

int xqb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats)
{
    const xbpos *s = root;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, s->key));
    ai_opts o = cb_opts(&LEVELS, level, &rng);
    int mv = ai_ab(level <= AI_EASY ? &ADAPTER_EASY : &ADAPTER, s, &o, stats);
    if (mv < 0 || mv == CB_END) return 0;
    xmove m = dec(mv);
    out[0] = (char)('a' + XQ_X(m.from));
    out[1] = (char)('0' + XQ_Y(m.from));
    out[2] = (char)('a' + XQ_X(m.to));
    out[3] = (char)('0' + XQ_Y(m.to));
    out[4] = 0;
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    xbpos *root = malloc(sizeof *root);         /* the copy, at entry: nothing else is read */
    if (!root) return 0;
    xqb_root(xiangqi_live(), root);
    xmove l[XQ_MAX_MOVES];
    char m[8];
    int ok = root->p.side == seat && cap >= 5 && xq_legal(&root->p, l) > 0 &&
             xqb_choose(root, level, ai_session_seed(), m, NULL) && !ai_cancelled();
    free(root);
    if (ok) memcpy(move, m, 5);
    return ok;
}

static const bot xiangqi_bot = {.think = think, .play = xiangqi_show};
REGISTER_BOT(xiangqi, &xiangqi_bot)
