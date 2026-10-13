/*
 * Janggi's bot (bots.h): alpha-beta (ai.h) over the rules' moves (janggi_rules.c) and the pass (a move like the others
 * when not in check: two passes in a row repeat the position, a draw the search sees at once), a transposition table,
 * the captures first (most valuable victim, least valuable attacker), killers and history, the pass last, a
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

static uint64_t key_of(const jpos *p) { return cb_board_key(p->sq, JG_PTS, 0x7a6ull + p->side); }
static int enc(jmove m) { return m.from | m.to << 7 | m.cap << 14; }
static jmove dec(int x)
{
    if (x == PASS) return (jmove){JG_PASS, JG_PASS, 0};
    jmove m = {(uint8_t)(x & 127), (uint8_t)(x >> 7 & 127), (uint8_t)(x >> 14 & 15)};
    return m;
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
    jmove l[JG_MAX_MOVES];
    int n = jn_legal(p, l);
    for (int i = 0; i < n; i++) out[i] = enc(l[i]);
    if (jn_can_pass(p)) out[n++] = PASS;
    if (!n) { out[0] = CB_END; return 1; }
    return n;
}

static int a_noisy(const void *st, int *out)
{
    const jpos *p = &((const jbpos *)st)->p;
    if (jn_in_check(p, p->side)) {
        int n = a_moves(st, out);
        return n;
    }
    jmove l[JG_MAX_MOVES];
    int n = jn_legal(p, l), k = 0;
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
    int mover = JG_TYPE(s->p.sq[m.from]), victim = JG_TYPE(m.cap);
    return cb_mvvlva(victim == JG_GENERAL ? 3000 : VAL[victim], mover == JG_GENERAL ? 1000 : VAL[mover]);
}

/* ---- the evaluation ---------------------------------------------------------------------------------------------- */
static int reach(const jpos *p, int s)
{
    static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int n = 0;
    for (int d = 0; d < 4; d++)
        for (int x = JG_X(s) + D[d][0], y = JG_Y(s) + D[d][1]; x >= 0 && x < JG_W && y >= 0 && y < JG_H;
             x += D[d][0], y += D[d][1]) {
            n++;
            if (p->sq[JG_PT(x, y)]) break;
        }
    return n;
}

static int a_eval(const void *st)
{
    const jbpos *s = st;
    const jpos *p = &s->p;
    int us = p->side, v[2] = {0, 0}, men = 0;
    for (int q = 0; q < JG_PTS; q++) men += p->sq[q] != 0;
    for (int q = 0; q < JG_PTS; q++) {
        int pc = p->sq[q];
        if (!pc) continue;
        int c = JG_OWNER(pc), t = JG_TYPE(pc), x = JG_X(q), rr = c ? 9 - JG_Y(q) : JG_Y(q), mid = 4 - cb_abs(x - 4);
        int palace = x >= 3 && x <= 5 && rr >= 7;   /* in the enemy palace */
        v[c] += VAL[t];
        switch (t) {
        case JG_SOLDIER: v[c] += (rr > 3 ? 8 * (rr - 3) : 0) + (palace ? 25 : 0) - (rr == 9 ? 30 : 0); break;
        case JG_HORSE: v[c] += 5 * mid + (rr >= 2 && rr <= 6 ? 10 : 0); break;
        case JG_ELEPHANT: v[c] += 3 * mid; break;
        case JG_CHARIOT: v[c] += 3 * reach(p, q) + (palace ? 25 : 0); break;
        case JG_CANNON: v[c] += men < 14 ? -40 : palace ? 15 : 0; break;
        case JG_GENERAL: v[c] += x == 4 && rr == 1 ? 15 : 0; break;
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
    .nodes = {[AI_EASY] = 8000, [AI_MEDIUM] = 50000, [AI_HARD] = 110000},    /* hard: about 0.3 s on a PC */
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
    out[0] = (char)('a' + JG_X(m.from));
    out[1] = (char)('0' + JG_Y(m.from));
    out[2] = (char)('a' + JG_X(m.to));
    out[3] = (char)('0' + JG_Y(m.to));
    out[4] = 0;
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    jbpos *root = malloc(sizeof *root);         /* the copy, at entry: nothing else is read */
    if (!root) return 0;
    jnb_root(janggi_live(), root);
    jmove l[JG_MAX_MOVES];
    char m[8];
    int ok = root->p.side == seat && cap >= 5 && (jn_legal(&root->p, l) > 0 || jn_can_pass(&root->p)) &&
             jnb_choose(root, level, ai_session_seed(), m, NULL) && !ai_cancelled();
    free(root);
    if (ok) memcpy(move, m, 5);
    return ok;
}

static const bot janggi_bot = {.think = think, .play = janggi_show};
REGISTER_BOT(janggi, &janggi_bot)
