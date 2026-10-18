/*
 * Corner Blocks' bot (bots.h), two to four players: greedy, by a block's size and the corners it leaves. A move is
 * worth its squares, the corner squares its player has after it (where the next blocks may start), less the corners it
 * takes from the others, and, early on, how far it reaches towards the middle of the board.
 * Easy: mostly the biggest blocks, laid anywhere they fit (a lot of chance). Medium: the value, a little chance.
 * Hard: the value, then its best few moves weighed against the next player's best answer to each.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("t3c12"), played by cornerblocks_show().
 */
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/cornerblocks.h"

#define MAX_MOVES 6000
#define HARD_TOP 6

/* the value of the move for the player to move, from their side */
static int value(const cblpos *p, int move)
{
    cblpos q = *p;
    int me = p->turn, b = cbl_block(move), a = cbl_anchor(move), n = p->n, reach = 0;
    cbl_shape s;
    cbl_shape_of(b, cbl_orient(move), &s);
    float c = (n - 1) / 2.0f;
    for (int i = 0; i < s.n; i++) {
        int x = a % CBL_MAXN + s.x[i], y = a / CBL_MAXN + s.y[i];
        q.cell[y * CBL_MAXN + x] = (uint8_t)(me + 1);
        float d = (x > c ? x - c : c - x) + (y > c ? y - c : c - y);
        reach += (int)(2 * (n - d));
    }
    q.left[me] &= ~(1u << b);
    int v = 40 * s.n + 6 * cbl_corners(&q, me, NULL);
    for (int o = 0; o < p->seats; o++)
        if (o != me && !p->out[o] && p->left[o]) v -= (p->seats == 2 ? 5 : 3) * cbl_corners(&q, o, NULL);
    if (p->laid[me] < (n == 14 ? 16 : 26)) v += reach / s.n;   /* the opening: towards the middle */
    return v;
}

int cblb_choose(const cblpos *p, int level, uint64_t seed)
{
    if (p->over) return -1;
    int *mv = malloc(sizeof(int) * MAX_MOVES * 2);
    if (!mv) return -1;
    int *val = mv + MAX_MOVES, k = cbl_moves(p, mv, MAX_MOVES);
    if (!k) { free(mv); return -1; }
    uint64_t key = (uint64_t)p->turn * 977 + p->laid[0] * 31 + p->laid[1];
    for (int s = 0; s < p->seats; s++) key = ai_mix(key, p->left[s]);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, key));
    for (int i = 0; i < k; i++) {
        if ((i & 63) == 0 && ai_cancelled()) { free(mv); return -1; }
        int b = cbl_block(mv[i]);
        if (level <= AI_EASY) val[i] = 40 * cbl_size(b) + ai_rand_n(&r, 90);
        else val[i] = value(p, mv[i]) + (level == AI_MEDIUM ? ai_rand_n(&r, 30) : ai_rand_n(&r, 3));
    }
    int best = 0;
    for (int i = 1; i < k; i++)
        if (val[i] > val[best]) best = i;
    int *reply = level >= AI_HARD && k > 1 ? malloc(sizeof(int) * MAX_MOVES) : NULL;
    if (reply) {                                /* the best few, against the next player's best answer */
        int top[HARD_TOP], nt = 0, pick = -1, pick_v = 0;
        for (; nt < HARD_TOP && nt < k; nt++) {
            int t = -1;
            for (int i = 0; i < k; i++) {
                int taken = 0;
                for (int j = 0; j < nt; j++) taken |= top[j] == i;
                if (!taken && (t < 0 || val[i] > val[t])) t = i;
            }
            top[nt] = t;
        }
        for (int t = 0; t < nt; t++) {
            cblpos q = *p;
            cbl_apply(&q, mv[top[t]]);
            int answer = 0;
            if (!q.over && q.turn != p->turn) {
                int kr = cbl_moves(&q, reply, MAX_MOVES);
                for (int i = 0; i < kr; i++) {
                    if ((i & 31) == 0 && ai_cancelled()) { free(reply); free(mv); return -1; }
                    int v = value(&q, reply[i]);
                    if (v > answer) answer = v;
                }
            }
            int v = val[top[t]] - answer * (p->seats == 2 ? 3 : 2) / 5;
            if (pick < 0 || v > pick_v) pick = top[t], pick_v = v;
        }
        free(reply);
        if (pick >= 0) best = pick;
    }
    int m = mv[best];
    free(mv);
    return m;
}

static int think(int seat, int level, char *move, int cap)
{
    cblpos p = *cbl_pos(cornerblocks_live());    /* the copy, at entry: nothing else is read */
    if (p.over || p.turn != seat || cap < 8) return 0;
    int m = cblb_choose(&p, level, ai_session_seed());
    if (m < 0 || ai_cancelled()) return 0;
    cbl_name(&p, m, move);
    return 1;
}

static const bot cornerblocks_bot = {.think = think, .play = cornerblocks_show};
REGISTER_BOT(cornerblocks, &cornerblocks_bot)
