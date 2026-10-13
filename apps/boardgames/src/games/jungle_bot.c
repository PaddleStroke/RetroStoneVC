/*
 * Jungle's bot (bots.h): alpha-beta (ai.h) over the rules' moves (jungle_rules.c), a transposition table, the den and
 * the captures first (most valuable victim, least valuable attacker), killers and history, a quiescence on the
 * captures and the steps into the den. The evaluation: the animals' worth (the rat more than its rank: it takes the
 * elephant and swims), each animal's road to the enemy den (worth more the nearer, a lot next to it), the same for the
 * enemy against one's own den. The end by the rules: the den entered, every animal taken, a player stuck; a draw by
 * repetition or the quiet moves. Runs on the worker thread: it copies the position (and the keys of the positions
 * before it) at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("a3a4", a jump "a1a5"), played by jungle_show().
 */
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/chessbots.h"
#include "games/jungle.h"

typedef struct {
    jgpos p;
    uint64_t key;
    uint8_t end, ply;
    cb_hist h;
} jgbpos;

/* rat cat dog wolf leopard tiger lion elephant */
static const int VAL[9] = {0, 350, 200, 250, 300, 450, 750, 850, 900};

static uint64_t key_of(const jgpos *p) { return cb_board_key(p->sq, JG_SQUARES, 0x1a6e1ull + p->side); }
static int enc(jgmove m) { return m.from | m.to << 6 | m.taken << 12; }
static jgmove dec(int x)
{
    jgmove m = {(int8_t)(x & 63), (int8_t)(x >> 6 & 63), (uint8_t)(x >> 12 & 63)};
    return m;
}

static int den_of(int player) { return JG_SQ(3, player ? 8 : 0); }

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static void a_make(void *st, int mv)
{
    jgbpos *s = st;
    s->ply++;
    if (mv == CB_END) {
        s->end = 1;
        s->key ^= 0xe4d;
        return;
    }
    cb_hist_push(&s->h, s->key);
    jg_make(&s->p, dec(mv));
    if (!s->p.quiet) cb_hist_clear(&s->h);
    s->key = key_of(&s->p);
}

static int a_side(const void *st) { return ((const jgbpos *)st)->p.side; }

static int a_over(const void *st, int *score)
{
    const jgbpos *s = st;
    const jgpos *p = &s->p;
    if (s->end || p->sq[den_of(p->side)] || !p->left[p->side]) {   /* stuck, the den entered, none left: lost */
        *score = -ai_win(0);
        return 1;
    }
    if (s->ply && (p->quiet >= JG_QUIET || cb_hist_seen(&s->h, s->key, p->quiet, 2))) {
        *score = 0;
        return 1;
    }
    return 0;
}

static int a_moves(const void *st, int *out)
{
    jgmove l[JG_MAX_MOVES];
    int n = jg_legal(&((const jgbpos *)st)->p, l);
    if (!n) { out[0] = CB_END; return 1; }
    for (int i = 0; i < n; i++) out[i] = enc(l[i]);
    return n;
}

static int a_noisy(const void *st, int *out)
{
    const jgpos *p = &((const jgbpos *)st)->p;
    jgmove l[JG_MAX_MOVES];
    int n = jg_legal(p, l), k = 0, den = den_of(!p->side);
    for (int i = 0; i < n; i++)
        if (l[i].taken || l[i].to == den) out[k++] = enc(l[i]);
    return k;
}

static uint64_t a_hash(const void *st) { return ((const jgbpos *)st)->key; }

/* the steps from s to the player's enemy den */
static int road(int s, int player)
{
    return cb_abs(JG_FILE(s) - 3) + cb_abs(JG_ROW(s) - (player ? 0 : 8));
}

static int a_order(const void *st, int mv)
{
    if (mv == CB_END) return 0;
    const jgbpos *s = st;
    jgmove m = dec(mv);
    int us = s->p.side, v = 0;
    if (m.to == den_of(!us)) return 50000;
    if (m.taken) v += cb_mvvlva(VAL[JG_RANK(m.taken)], VAL[JG_RANK(s->p.sq[m.from])]);
    return v + 4 * (road(m.from, us) - road(m.to, us));
}

/* ---- the evaluation ---------------------------------------------------------------------------------------------- */
static int a_eval(const void *st)
{
    const jgbpos *s = st;
    const jgpos *p = &s->p;
    int us = p->side, v[2] = {0, 0};
    for (int q = 0; q < JG_SQUARES; q++) {
        int pc = p->sq[q];
        if (!pc) continue;
        int c = JG_OWNER(pc), r = JG_RANK(pc), d = road(q, c);
        v[c] += VAL[r];
        /* the road: the strong animals press on, the weak ones a little (they guard) */
        int w = r >= JG_LEOPARD || r == JG_RAT ? 6 : 3;
        v[c] += w * (11 - d) + (d <= 2 ? 40 : 0) + (d == 1 ? 120 : 0);
        if (jg_trap(q) == !c) v[c] += 20;       /* in the enemy's trap, next to its den */
    }
    return v[us] - v[!us] + 10;
}

static const ai_game ADAPTER = {
    .size = sizeof(jgbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};
static const ai_game ADAPTER_EASY = {           /* easy: no quiescence (a longer exchange escapes it) */
    .size = sizeof(jgbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

static const cb_levels LEVELS = {
    .nodes = {[AI_EASY] = 8000, [AI_MEDIUM] = 50000, [AI_HARD] = 1100000},   /* hard: about 0.3 s on a PC */
    .depth = {[AI_EASY] = 2, [AI_MEDIUM] = 5, [AI_HARD] = 0},
    .noise = {[AI_EASY] = 100, [AI_MEDIUM] = 25, [AI_HARD] = 0},
    .tt_bits = 18,
};

void jgb_root(const jggame *g, void *root)
{
    jgbpos *s = root;
    memset(s, 0, sizeof *s);
    s->p = *jg_pos(g);
    s->key = key_of(&s->p);
    int back = cb_min(s->p.quiet, CB_HIST), from = g->ply - back < 0 ? 0 : g->ply - back;
    for (int i = from; i < g->ply; i++) cb_hist_push(&s->h, key_of(&g->pos[i]));
}

int jgb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats)
{
    const jgbpos *s = root;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, s->key));
    ai_opts o = cb_opts(&LEVELS, level, &rng);
    int mv = ai_ab(level <= AI_EASY ? &ADAPTER_EASY : &ADAPTER, s, &o, stats);
    if (mv < 0 || mv == CB_END) return 0;
    jgmove m = dec(mv);
    out[0] = (char)('a' + JG_FILE(m.from));
    out[1] = (char)('1' + JG_ROW(m.from));
    out[2] = (char)('a' + JG_FILE(m.to));
    out[3] = (char)('1' + JG_ROW(m.to));
    out[4] = 0;
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    jgbpos *root = malloc(sizeof *root);        /* the copy, at entry: nothing else is read */
    if (!root) return 0;
    jgb_root(jungle_live(), root);
    jgmove l[JG_MAX_MOVES];
    char m[8];
    int ok = root->p.side == seat && cap >= 5 && jg_legal(&root->p, l) > 0 &&
             jgb_choose(root, level, ai_session_seed(), m, NULL) && !ai_cancelled();
    free(root);
    if (ok) memcpy(move, m, 5);
    return ok;
}

static const bot jungle_bot = {.think = think, .play = jungle_show};
REGISTER_BOT(jungle, &jungle_bot)
