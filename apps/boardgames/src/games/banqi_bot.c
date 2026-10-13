/*
 * Banqi's bot (bots.h): expectiminimax (ai.h's alpha-beta with chance nodes) over what the player to move may know:
 * the pieces face up, and the pieces taken (face up when taken). The pieces face down are never read: the bot's copy
 * of the board has them blank, and the pieces still face down (the full set less those face up and those taken) make
 * a pool. Turning a piece over is two steps: the choice of the square (a decision), then what shows (a chance node:
 * each kind of piece in the pool, weighed by how many are left). A transposition table, the captures first (most
 * valuable victim, least valuable attacker), the turns last, killers and history, a quiescence on the captures. The
 * evaluation: the pieces each colour still has (face up, and its share of the pool: the same worth face down), the
 * pieces face up that the other side can take next. The game's first turn (nobody has a colour yet) turns a piece
 * over at random.
 * Runs on the worker thread: it copies the public position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("c2" turns c2 over, "c2c3" moves or takes), played by banqi_show().
 */
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/banqi.h"
#include "games/chessbots.h"

#define SHOW (1 << 12)                          /* a chance outcome: the piece that shows (SHOW | colour << 3 | type) */

typedef struct {
    bqpos p;                                    /* the face-down squares blank (BQ_HIDDEN alone) */
    uint8_t pool[2][8];                         /* the pieces face down, by colour and type */
    int8_t pending;                             /* a square chosen to turn over, its face not drawn yet; -1 */
    uint8_t end, ply;
    uint64_t key;
} bbpos;

/* general advisor elephant chariot horse cannon soldier */
static const int VAL[8] = {0, 650, 400, 250, 160, 110, 330, 80};
static const uint8_t SET[8] = {0, 1, 2, 2, 2, 2, 2, 5};

static uint64_t key_of(const bbpos *s)
{
    uint64_t h = cb_board_key(s->p.sq, BQ_N, 0xb4c1ull + s->p.side) ^ cb_mix(0x900000ull + (uint64_t)(s->pending + 1));
    for (int c = 0; c < 2; c++)
        for (int t = 1; t < 8; t++) h ^= cb_mix(0x910000ull + (uint64_t)((c * 8 + t) * 16 + s->pool[c][t]));
    return h;
}

static int enc(bqmove m) { return m.kind << 10 | m.to << 5 | m.from; }

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static void a_make(void *st, int mv)
{
    bbpos *s = st;
    bqpos *p = &s->p;
    s->ply++;
    if (mv == CB_END) {
        s->end = 1;
        s->key ^= 0xe4d;
        return;
    }
    if (mv & SHOW) {                            /* the face drawn from the pool */
        int pc = mv & 15;
        p->sq[s->pending] = (uint8_t)pc;
        s->pool[BQ_COLOUR(pc)][BQ_TYPE(pc)]--;
        s->pending = -1;
        if (!p->decided) {
            p->decided = 1;
            p->side = p->first = (uint8_t)BQ_COLOUR(pc);
        }
        p->quiet = 0;
        p->side ^= 1;
    } else if ((mv >> 10) == BQ_FLIP) {
        s->pending = (int8_t)(mv & 31);
    } else {
        bqmove m = {(uint8_t)(mv >> 10), (uint8_t)(mv & 31), (uint8_t)(mv >> 5 & 31), 0};
        bq_make(p, m);
    }
    s->key = key_of(s);
}

static int a_side(const void *st) { return ((const bbpos *)st)->p.side; }

static int a_over(const void *st, int *score)
{
    const bbpos *s = st;
    if (s->pending >= 0) return 0;
    if (s->end || s->p.taken[s->p.side] >= 16) { *score = -ai_win(0); return 1; }
    if (s->p.quiet >= BQ_QUIET) { *score = 0; return 1; }
    return 0;
}

static int a_chance(const void *st, int *out, int *w)
{
    const bbpos *s = st;
    if (s->pending < 0) return 0;
    int n = 0;
    for (int c = 0; c < 2; c++)
        for (int t = 1; t < 8; t++)
            if (s->pool[c][t]) {
                out[n] = SHOW | BQ_PC(c, t);
                w[n++] = s->pool[c][t];
            }
    return n;
}

/* a face-down square next to a piece face up (what turns there may take or be taken at once) */
static int near_up(const bqpos *p, int s)
{
    static const int8_t X[4] = {1, -1, 0, 0}, Y[4] = {0, 0, 1, -1};
    for (int d = 0; d < 4; d++) {
        int x = BQ_X(s) + X[d], y = BQ_Y(s) + Y[d];
        if (x >= 0 && y >= 0 && x < BQ_W && y < BQ_H && p->sq[BQ_SQ(x, y)] && !(p->sq[BQ_SQ(x, y)] & BQ_HIDDEN))
            return 1;
    }
    return 0;
}

/* the moves; below the root, the turns over only next to the pieces face up (a few others alike stand for the rest:
 * each turn is a chance node of up to 14 faces, the search's cost) */
static int a_moves(const void *st, int *out)
{
    const bbpos *s = st;
    bqmove l[BQ_MAX_MOVES];
    int n = bq_legal(&s->p, l), k = 0, far = 0;
    if (!n) { out[0] = CB_END; return 1; }
    for (int i = 0; i < n; i++) {
        if (l[i].kind == BQ_FLIP && s->ply && !near_up(&s->p, l[i].from) && far++ >= 2) continue;
        out[k++] = enc(l[i]);
    }
    return k;
}

static int a_noisy(const void *st, int *out)
{
    const bbpos *s = st;
    if (s->pending >= 0) return 0;
    bqmove l[BQ_MAX_MOVES];
    int n = bq_legal(&s->p, l), k = 0;
    for (int i = 0; i < n; i++)
        if (l[i].kind == BQ_TAKE) out[k++] = enc(l[i]);
    return k;
}

static uint64_t a_hash(const void *st) { return ((const bbpos *)st)->key; }

static int a_order(const void *st, int mv)
{
    if (mv == CB_END || (mv & SHOW)) return 0;
    const bbpos *s = st;
    int kind = mv >> 10, from = mv & 31, to = mv >> 5 & 31;
    if (kind == BQ_FLIP) return -20;
    if (kind == BQ_TAKE) return cb_mvvlva(VAL[BQ_TYPE(s->p.sq[to])], VAL[BQ_TYPE(s->p.sq[from])]);
    return 0;
}

/* ---- the evaluation ---------------------------------------------------------------------------------------------- */
static const int8_t DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};

/* the piece on s (face up) can be taken by the other colour's next move */
static int hanging(const bqpos *p, int s)
{
    int pc = p->sq[s], c = BQ_COLOUR(pc), t = BQ_TYPE(pc);
    for (int d = 0; d < 4; d++) {
        int x = BQ_X(s) + DX[d], y = BQ_Y(s) + DY[d], screens = 0;
        for (int k = 1; x >= 0 && y >= 0 && x < BQ_W && y < BQ_H; k++, x += DX[d], y += DY[d]) {
            int q = p->sq[BQ_SQ(x, y)];
            if (!q) continue;
            if (!(q & BQ_HIDDEN) && BQ_COLOUR(q) != c) {
                if (k == 1 && screens == 0 && bq_beats(BQ_TYPE(q), t)) return 1;
                if (screens == 1 && BQ_TYPE(q) == BQ_CANNON) return 1;
            }
            if (++screens > 1) break;
        }
    }
    return 0;
}

static int a_eval(const void *st)
{
    const bbpos *s = st;
    const bqpos *p = &s->p;
    int us = p->side, v[2] = {0, 0}, soldiers[2] = {0, 0};
    for (int c = 0; c < 2; c++)
        for (int t = 1; t < 8; t++) v[c] += s->pool[c][t] * VAL[t];
    for (int q = 0; q < BQ_N; q++) {
        int pc = p->sq[q];
        if (pc && !(pc & BQ_HIDDEN) && BQ_TYPE(pc) == BQ_SOLDIER) soldiers[BQ_COLOUR(pc)]++;
    }
    for (int q = 0; q < BQ_N; q++) {
        int pc = p->sq[q];
        if (!pc || (pc & BQ_HIDDEN)) continue;
        int c = BQ_COLOUR(pc), t = BQ_TYPE(pc), worth = VAL[t];
        if (t == BQ_GENERAL) worth -= 40 * (soldiers[!c] + s->pool[!c][BQ_SOLDIER]) / 2;   /* the soldiers' prey */
        v[c] += worth + 6;                     /* face up: it can act */
        if (hanging(p, q)) v[c] -= c == us ? worth / 6 : worth * 3 / 4;   /* mine can still flee */
    }
    return v[us] - v[!us] + 5;
}

static const ai_game ADAPTER = {
    .size = sizeof(bbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .chance = a_chance, .unit = 100,
};
static const ai_game ADAPTER_EASY = {           /* easy: no quiescence */
    .size = sizeof(bbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .chance = a_chance, .unit = 100,
};

static const cb_levels LEVELS = {
    .nodes = {[AI_EASY] = 20000, [AI_MEDIUM] = 100000, [AI_HARD] = 450000},   /* hard: 0.1 to 0.4 s on a PC */
    .depth = {[AI_EASY] = 2, [AI_MEDIUM] = 3, [AI_HARD] = 0},
    .noise = {[AI_EASY] = 100, [AI_MEDIUM] = 25, [AI_HARD] = 0},
    .tt_bits = 18,
};

/* the root as the player to move sees it: the face-down squares blank, the pool from the pieces face up and taken */
void bqb_root(const bqgame *g, void *root)
{
    bbpos *s = root;
    memset(s, 0, sizeof *s);
    s->p = *bq_pos(g);
    s->pending = -1;
    for (int c = 0; c < 2; c++) memcpy(s->pool[c], SET, sizeof SET);
    for (int q = 0; q < BQ_N; q++) {
        if (s->p.sq[q] & BQ_HIDDEN) s->p.sq[q] = BQ_HIDDEN;   /* never read what is face down */
        else if (s->p.sq[q]) s->pool[BQ_COLOUR(s->p.sq[q])][BQ_TYPE(s->p.sq[q])]--;
    }
    for (int i = 0; i < g->ply; i++)            /* the pieces taken were face up when taken */
        if (g->move[i].kind == BQ_TAKE) s->pool[BQ_COLOUR(g->move[i].pc)][BQ_TYPE(g->move[i].pc)]--;
    s->key = key_of(s);
}

int bqb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats)
{
    const bbpos *s = root;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, s->key));
    int mv;
    if (!s->p.decided) {                        /* the first turn: any piece, at random */
        int hidden[BQ_N], n = 0;
        for (int q = 0; q < BQ_N; q++)
            if (s->p.sq[q] & BQ_HIDDEN) hidden[n++] = q;
        if (!n) return 0;
        mv = enc((bqmove){BQ_FLIP, (uint8_t)hidden[ai_rand_n(&rng, n)], 0, 0});
        if (stats) memset(stats, 0, sizeof *stats);
    } else {
        ai_opts o = cb_opts(&LEVELS, level, &rng);
        mv = ai_ab(level <= AI_EASY ? &ADAPTER_EASY : &ADAPTER, s, &o, stats);
    }
    if (mv < 0 || mv == CB_END) return 0;
    int kind = mv >> 10, from = mv & 31, to = mv >> 5 & 31;
    out[0] = (char)('a' + BQ_X(from));
    out[1] = (char)('1' + BQ_Y(from));
    out[2] = kind == BQ_FLIP ? 0 : (char)('a' + BQ_X(to));
    out[3] = (char)('1' + BQ_Y(to));
    out[4] = 0;
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    bbpos *root = malloc(sizeof *root);         /* the public copy, at entry: nothing else is read */
    if (!root) return 0;
    bqb_root(banqi_live(), root);
    bqmove l[BQ_MAX_MOVES];
    char m[8];
    /* the seat whose colour moves (seat 0 plays the colour it turned over first) */
    int ok = bq_seat(&root->p, root->p.side) == seat && cap >= 5 && root->p.taken[root->p.side] < 16 &&
             root->p.quiet < BQ_QUIET && bq_legal(&root->p, l) > 0 &&
             bqb_choose(root, level, ai_session_seed(), m, NULL) && !ai_cancelled();
    free(root);
    if (ok) memcpy(move, m, 5);
    return ok;
}

static const bot banqi_bot = {.think = think, .play = banqi_show};
REGISTER_BOT(banqi, &banqi_bot)
