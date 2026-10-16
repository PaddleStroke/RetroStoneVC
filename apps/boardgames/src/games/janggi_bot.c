/*
 * Janggi's bot (bots.h): alpha-beta (ai.h) over the rules' moves (its own generator: the same moves as
 * janggi_rules.c's, the check found from the general outwards, much faster for a search) and the pass (a move like the
 * others when not in check: two passes in a row repeat the position, a draw the search sees at once), a transposition
 * table, the captures first (most valuable victim, least valuable attacker), killers and history, the pass last, a
 * quiescence on the captures (every evasion when in check). The evaluation: material (the usual points: chariot 13,
 * cannon 7, horse 5, elephant and guard 3, soldier 2), the soldiers' advance towards the palace, the chariots' open
 * lines and the enemy palace, the horses to the centre, the cannons short of screens once the board empties, the
 * general in the middle of its palace. Runs on the worker thread: it copies the position (and the keys of the
 * positions before it) at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("e3f3", "pass"), played by janggi_show().
 */
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/chessbots.h"
#include "games/janggi.h"

#define PASS (1 << 20)

typedef struct {
    jpos p;
    uint64_t key;
    uint8_t end, ply;
    cb_hist h;
} jbpos;

static const int VAL[8] = {0, 0, 150, 150, 250, 650, 350, 100};  /* general guard elephant horse chariot cannon soldier */

static uint64_t key_of(const jpos *p) { return cb_board_key(p->sq, JN_PTS, 0x7a6ull + p->side); }
static int enc(jmove m) { return m.from | m.to << 7 | m.cap << 14; }
static jmove dec(int x)
{
    if (x == PASS) return (jmove){JN_PASS, JN_PASS, 0};
    jmove m = {(uint8_t)(x & 127), (uint8_t)(x >> 7 & 127), (uint8_t)(x >> 14 & 15)};
    return m;
}

/* ---- the moves: the rules' exactly (tests/ai_janggi_test.c compares them), the check found from the general --------
 * outwards (the rules' test generates every enemy move for each candidate: too slow for a search) */
static const int8_t DIRS[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};

static int on(int x, int y) { return x >= 0 && x < JN_W && y >= 0 && y < JN_H; }
static int in_palace(int x, int y) { return x >= 3 && x <= 5 && (y <= 2 || y >= 7); }
static int own_palace(int player, int x, int y) { return in_palace(x, y) && (player ? y >= 7 : y <= 2); }
static int fwd(int player) { return player ? -1 : 1; }
static int at(const jpos *p, int x, int y) { return on(x, y) ? p->sq[JN_PT(x, y)] : -1; }

/* a step along a palace's diagonal: both points in one palace, one of them its middle */
static int link(int x0, int y0, int x1, int y1)
{
    if (!in_palace(x0, y0) || !in_palace(x1, y1) || (y0 <= 2) != (y1 <= 2)) return 0;
    return (x0 == 4 && (y0 == 1 || y0 == 8)) || (x1 == 4 && (y1 == 1 || y1 == 8));
}

static int general(const jpos *p, int player)
{
    for (int y = player ? 7 : 0; y <= (player ? 9 : 2); y++)   /* it never leaves its palace */
        for (int x = 3; x <= 5; x++)
            if (p->sq[JN_PT(x, y)] == JN_PC(player, JN_GENERAL)) return JN_PT(x, y);
    return -1;
}

static int facing(const jpos *p)
{
    int a = general(p, 0), b = general(p, 1);
    if (a < 0 || b < 0 || JN_X(a) != JN_X(b)) return 0;
    for (int s = a + JN_W; s < b; s += JN_W)
        if (p->sq[s]) return 0;
    return 1;
}

/* 1: a move of `by` reaches the point t */
static int attacked(const jpos *p, int t, int by)
{
    int tx = JN_X(t), ty = JN_Y(t);
    for (int d = 0; d < 8; d++) {               /* the lines: a chariot first, a cannon past one screen */
        int dx = DIRS[d][0], dy = DIRS[d][1], x = tx, y = ty, screen = 0;
        for (;;) {
            int nx = x + dx, ny = y + dy;
            if (!on(nx, ny) || (d >= 4 && !link(x, y, nx, ny))) break;
            x = nx;
            y = ny;
            int pc = p->sq[JN_PT(x, y)];
            if (!pc) continue;
            if (!screen) {
                if (pc == JN_PC(by, JN_CHARIOT)) return 1;
                if (JN_TYPE(pc) == JN_CANNON) break;   /* a cannon never screens a cannon */
                screen = 1;
            } else {
                if (pc == JN_PC(by, JN_CANNON)) return 1;
                break;
            }
        }
        /* the general and the guards: one step inside their own palace */
        int ax = tx - dx, ay = ty - dy, pc = at(p, ax, ay);
        if (pc > 0 && JN_OWNER(pc) == by && (JN_TYPE(pc) == JN_GENERAL || JN_TYPE(pc) == JN_GUARD) &&
            own_palace(by, tx, ty) && (d < 4 || link(ax, ay, tx, ty)))
            return 1;
    }
    for (int d = 0; d < 4; d++) {               /* the horse and the elephant: back along their path, every point free */
        int dx = DIRS[d][0], dy = DIRS[d][1];
        for (int s = -1; s <= 1; s += 2) {
            int ex = dx + s * dy, ey = dy + s * dx;
            int hx = tx - dx - ex, hy = ty - dy - ey;
            if (at(p, hx, hy) == JN_PC(by, JN_HORSE) && at(p, hx + dx, hy + dy) == 0) return 1;
            int ax = tx - dx - 2 * ex, ay = ty - dy - 2 * ey;
            if (at(p, ax, ay) == JN_PC(by, JN_ELEPHANT) && at(p, ax + dx, ay + dy) == 0 &&
                at(p, ax + dx + ex, ay + dy + ey) == 0)
                return 1;
        }
    }
    int f = fwd(by), sol = JN_PC(by, JN_SOLDIER);   /* a soldier: from behind, beside, or a palace diagonal behind */
    if (at(p, tx, ty - f) == sol || at(p, tx - 1, ty) == sol || at(p, tx + 1, ty) == sol) return 1;
    for (int s = -1; s <= 1; s += 2)
        if (at(p, tx - s, ty - f) == sol && link(tx - s, ty - f, tx, ty)) return 1;
    return 0;
}

static int in_check(const jpos *p, int player)
{
    int g = general(p, player);
    return g >= 0 && attacked(p, g, !player);
}

static void add(const jpos *p, int me, jmove *out, int *n, int from, int x, int y)
{
    if (!on(x, y)) return;
    int to = JN_PT(x, y), pc = p->sq[to];
    if (pc && JN_OWNER(pc) == me) return;
    out[(*n)++] = (jmove){(uint8_t)from, (uint8_t)to, (uint8_t)pc};
}

static int pseudo(const jpos *p, int me, jmove *out)
{
    int n = 0;
    for (int from = 0; from < JN_PTS; from++) {
        int pc = p->sq[from];
        if (!pc || JN_OWNER(pc) != me) continue;
        int x = JN_X(from), y = JN_Y(from), t = JN_TYPE(pc);
        switch (t) {
        case JN_GENERAL:
        case JN_GUARD:
            for (int d = 0; d < 8; d++) {
                int tx = x + DIRS[d][0], ty = y + DIRS[d][1];
                if (own_palace(me, tx, ty) && (d < 4 || link(x, y, tx, ty))) add(p, me, out, &n, from, tx, ty);
            }
            break;
        case JN_HORSE:
        case JN_ELEPHANT:
            for (int d = 0; d < 4; d++) {
                int dx = DIRS[d][0], dy = DIRS[d][1];
                if (at(p, x + dx, y + dy) != 0) continue;
                for (int s = -1; s <= 1; s += 2) {
                    int ex = dx + s * dy, ey = dy + s * dx;
                    if (t == JN_HORSE) add(p, me, out, &n, from, x + dx + ex, y + dy + ey);
                    else if (at(p, x + dx + ex, y + dy + ey) == 0) add(p, me, out, &n, from, x + dx + 2 * ex, y + dy + 2 * ey);
                }
            }
            break;
        case JN_CHARIOT:
        case JN_CANNON:
            for (int d = 0; d < 8; d++) {
                int dx = DIRS[d][0], dy = DIRS[d][1], cx = x, cy = y, screen = 0;
                for (;;) {
                    int nx = cx + dx, ny = cy + dy;
                    if (!on(nx, ny) || (d >= 4 && !link(cx, cy, nx, ny))) break;
                    cx = nx;
                    cy = ny;
                    int q = p->sq[JN_PT(cx, cy)];
                    if (t == JN_CHARIOT) {
                        add(p, me, out, &n, from, cx, cy);
                        if (q) break;
                    } else if (!screen) {
                        if (q && JN_TYPE(q) == JN_CANNON) break;
                        screen = q != 0;
                    } else if (!q) add(p, me, out, &n, from, cx, cy);
                    else {
                        if (JN_TYPE(q) != JN_CANNON) add(p, me, out, &n, from, cx, cy);
                        break;
                    }
                }
            }
            break;
        case JN_SOLDIER:
            add(p, me, out, &n, from, x, y + fwd(me));
            add(p, me, out, &n, from, x - 1, y);
            add(p, me, out, &n, from, x + 1, y);
            for (int s = -1; s <= 1; s += 2)
                if (link(x, y, x + s, y + fwd(me))) add(p, me, out, &n, from, x + s, y + fwd(me));
            break;
        }
    }
    return n;
}

/* the legal moves (not the pass); their count */
int jnb_legal(const jpos *p, jmove *out)
{
    jmove all[JN_MAX_MOVES];
    int n = pseudo(p, p->side, all), k = 0, me = p->side, g = general(p, me);
    for (int i = 0; i < n; i++) {
        jpos q = *p;
        jn_make(&q, all[i]);
        int gq = all[i].from == g ? all[i].to : g;
        if ((gq < 0 || !attacked(&q, gq, !me)) && !facing(&q)) out[k++] = all[i];
    }
    return k;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static void a_make(void *st, int mv)
{
    jbpos *s = st;
    s->ply++;
    if (mv == CB_END) {
        s->end = 1;
        s->key ^= 0xe4d;
        return;
    }
    cb_hist_push(&s->h, s->key);
    jn_make(&s->p, dec(mv));
    if (!s->p.quiet) cb_hist_clear(&s->h);
    s->key = key_of(&s->p);
}

static int a_side(const void *st) { return ((const jbpos *)st)->p.side; }

static int a_over(const void *st, int *score)
{
    const jbpos *s = st;
    if (s->end) { *score = -ai_win(0); return 1; }   /* in check, no move: mated */
    if (s->ply && cb_hist_seen(&s->h, s->key, s->p.quiet, 2)) { *score = 0; return 1; }
    return 0;
}

static int a_moves(const void *st, int *out)
{
    const jpos *p = &((const jbpos *)st)->p;
    jmove l[JN_MAX_MOVES];
    int n = jnb_legal(p, l);
    for (int i = 0; i < n; i++) out[i] = enc(l[i]);
    if (!in_check(p, p->side)) out[n++] = PASS;   /* the pass, never in check */
    if (!n) { out[0] = CB_END; return 1; }
    return n;
}

static int a_noisy(const void *st, int *out)
{
    const jpos *p = &((const jbpos *)st)->p;
    if (in_check(p, p->side)) return a_moves(st, out);
    jmove l[JN_MAX_MOVES];
    int n = jnb_legal(p, l), k = 0;
    for (int i = 0; i < n; i++)
        if (l[i].cap) out[k++] = enc(l[i]);
    return k;
}

static uint64_t a_hash(const void *st) { return ((const jbpos *)st)->key; }

static int a_order(const void *st, int mv)
{
    if (mv == CB_END) return 0;
    if (mv == PASS) return -50;
    const jbpos *s = st;
    jmove m = dec(mv);
    if (!m.cap) return 0;
    int mover = JN_TYPE(s->p.sq[m.from]), victim = JN_TYPE(m.cap);
    return cb_mvvlva(victim == JN_GENERAL ? 3000 : VAL[victim], mover == JN_GENERAL ? 1000 : VAL[mover]);
}

/* ---- the evaluation ---------------------------------------------------------------------------------------------- */
static int reach(const jpos *p, int s)
{
    static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int n = 0;
    for (int d = 0; d < 4; d++)
        for (int x = JN_X(s) + D[d][0], y = JN_Y(s) + D[d][1]; x >= 0 && x < JN_W && y >= 0 && y < JN_H;
             x += D[d][0], y += D[d][1]) {
            n++;
            if (p->sq[JN_PT(x, y)]) break;
        }
    return n;
}

static int a_eval(const void *st)
{
    const jbpos *s = st;
    const jpos *p = &s->p;
    int us = p->side, v[2] = {0, 0}, men = 0;
    for (int q = 0; q < JN_PTS; q++) men += p->sq[q] != 0;
    for (int q = 0; q < JN_PTS; q++) {
        int pc = p->sq[q];
        if (!pc) continue;
        int c = JN_OWNER(pc), t = JN_TYPE(pc), x = JN_X(q), rr = c ? 9 - JN_Y(q) : JN_Y(q), mid = 4 - cb_abs(x - 4);
        int palace = x >= 3 && x <= 5 && rr >= 7;   /* in the enemy palace */
        v[c] += VAL[t];
        switch (t) {
        case JN_SOLDIER: v[c] += (rr > 3 ? 8 * (rr - 3) : 0) + (palace ? 25 : 0) - (rr == 9 ? 30 : 0); break;
        case JN_HORSE: v[c] += 5 * mid + (rr >= 2 && rr <= 6 ? 10 : 0); break;
        case JN_ELEPHANT: v[c] += 3 * mid; break;
        case JN_CHARIOT: v[c] += 3 * reach(p, q) + (palace ? 25 : 0); break;
        case JN_CANNON: v[c] += men < 14 ? -40 : palace ? 15 : 0; break;
        case JN_GENERAL: v[c] += x == 4 && rr == 1 ? 15 : 0; break;
        }
    }
    return v[us] - v[!us] + 10;
}

static const ai_game ADAPTER = {
    .size = sizeof(jbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};
static const ai_game ADAPTER_EASY = {           /* easy: no quiescence (a longer exchange escapes it) */
    .size = sizeof(jbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

static const cb_levels LEVELS = {
    .nodes = {[AI_EASY] = 8000, [AI_MEDIUM] = 30000, [AI_HARD] = 250000},    /* hard: about 0.3 s on a PC */
    .depth = {[AI_EASY] = 2, [AI_MEDIUM] = 4, [AI_HARD] = 0},
    .noise = {[AI_EASY] = 100, [AI_MEDIUM] = 25, [AI_HARD] = 0},
    .tt_bits = 18,
};

void jnb_root(const jgame *g, void *root)
{
    jbpos *s = root;
    memset(s, 0, sizeof *s);
    s->p = *jn_pos(g);
    s->key = key_of(&s->p);
    int back = cb_min(s->p.quiet, CB_HIST), from = g->ply - back < 0 ? 0 : g->ply - back;
    for (int i = from; i < g->ply; i++) cb_hist_push(&s->h, key_of(&g->pos[i]));
}

int jnb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats)
{
    const jbpos *s = root;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, s->key));
    ai_opts o = cb_opts(&LEVELS, level, &rng);
    int mv = ai_ab(level <= AI_EASY ? &ADAPTER_EASY : &ADAPTER, s, &o, stats);
    if (mv < 0 || mv == CB_END) return 0;
    if (mv == PASS) {
        strcpy(out, "pass");
        return 1;
    }
    jmove m = dec(mv);
    out[0] = (char)('a' + JN_X(m.from));
    out[1] = (char)('0' + JN_Y(m.from));
    out[2] = (char)('a' + JN_X(m.to));
    out[3] = (char)('0' + JN_Y(m.to));
    out[4] = 0;
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    jbpos *root = malloc(sizeof *root);         /* the copy, at entry: nothing else is read */
    if (!root) return 0;
    jnb_root(janggi_live(), root);
    jmove l[JN_MAX_MOVES];
    char m[8];
    int ok = root->p.side == seat && cap >= 5 && (jnb_legal(&root->p, l) > 0 || !in_check(&root->p, seat)) &&
             jnb_choose(root, level, ai_session_seed(), m, NULL) && !ai_cancelled();
    free(root);
    if (ok) memcpy(move, m, 5);
    return ok;
}

static const bot janggi_bot = {.think = think, .play = janggi_show};
REGISTER_BOT(janggi, &janggi_bot)
