/*
 * Shogi's bot (bots.h), on the 9 x 9 board and mini shogi's 5 x 5: alpha-beta (ai.h) over its own move generator (the
 * rules' moves exactly: the promotions, the drops and their limits, the pawn drop that may not mate; checked against
 * shogi_rules.c's on random games by tests/ai_shogi_test.c), which finds the attacks from the king's square outwards
 * (the rules' generator tries every piece: too slow for a search, and it keeps a static buffer, unsafe on the worker
 * thread). A transposition table, the captures first (most valuable victim, least valuable attacker), the promotions,
 * the drops next to the enemy king, killers and history, a quiescence on the captures and the promotions (every
 * evasion when in check). The evaluation: material (a piece in hand worth a little more than on the board), the
 * golds and silvers round the own king, the enemy's pieces near it and its hand against its open squares, the
 * pawns' advance. A player with no legal move loses; a repetition in the search is a draw (mini shogi: sente's loss,
 * the rules'). Runs on the worker thread: it copies the position (and the keys of the positions before it) at entry
 * and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation (USI: "7g7f", "8h2b+", "P*5e"), played by shogi_show().
 */
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/chessbots.h"
#include "games/shogi.h"

typedef struct {
    spos p;
    uint64_t key;
    uint8_t end, ply;
    cb_hist h;
} sbpos;

/* ---- the pieces' moves, as sente sees them (forward: the row falls) ---------------------------------------------- */
static const int8_t D8[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};   /* steps */
static const int8_t SL[8][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};   /* slides */
typedef struct { uint8_t step, knight, slide; } finfo;
#define GOLD_STEPS 0x5f                         /* forward three, the sides, straight back */
static const finfo FI[32] = {
    [SG_PAWN] = {0x02, 0, 0},
    [SG_LANCE] = {0, 0, 0x01},
    [SG_KNIGHT] = {0, 1, 0},
    [SG_SILVER] = {0xa7, 0, 0},                 /* forward three, the back diagonals */
    [SG_GOLD] = {GOLD_STEPS, 0, 0},
    [SG_BISHOP] = {0, 0, 0xf0},
    [SG_ROOK] = {0, 0, 0x0f},
    [SG_KING] = {0xff, 0, 0},
    [SG_PAWN | SG_PROMOTED] = {GOLD_STEPS, 0, 0},
    [SG_LANCE | SG_PROMOTED] = {GOLD_STEPS, 0, 0},
    [SG_KNIGHT | SG_PROMOTED] = {GOLD_STEPS, 0, 0},
    [SG_SILVER | SG_PROMOTED] = {GOLD_STEPS, 0, 0},
    [SG_BISHOP | SG_PROMOTED] = {0x5a, 0, 0xf0},   /* the horse: the bishop and the four straight steps */
    [SG_ROOK | SG_PROMOTED] = {0xa5, 0, 0x0f},     /* the dragon: the rook and the four diagonal steps */
};

static int inside(int n, int x, int y) { return x >= 0 && y >= 0 && x < n && y < n; }
static int ahead(int n, int player, int row) { return player ? n - 1 - row : row; }
static int zone(int n, int player, int row) { return ahead(n, player, row) < (n == 9 ? 3 : 1); }
static int dead(int n, int kind, int player, int row)
{
    int a = ahead(n, player, row);
    return ((kind == SG_PAWN || kind == SG_LANCE) && a == 0) || (kind == SG_KNIGHT && a <= 1);
}

/* 1: `by` attacks the square t (looked for from t outwards) */
static int attacked(const spos *p, int t, int by)
{
    int n = p->n, tx = SG_COL(t), ty = SG_ROW(t), turn = by ? -1 : 1;
    for (int d = 0; d < 8; d++) {               /* a step: the attacker one square back along its own step */
        int ax = tx - D8[d][0] * turn, ay = ty - D8[d][1] * turn;
        if (!inside(n, ax, ay)) continue;
        int pc = p->sq[SG_SQ(ax, ay)];
        if (pc && SG_OWNER(pc) == by && (FI[SG_FACE(pc)].step >> d & 1)) return 1;
    }
    for (int sx = -1; sx <= 1; sx += 2) {       /* a knight: two forward, one aside */
        int ax = tx - sx * turn, ay = ty + 2 * turn;
        if (inside(n, ax, ay) && p->sq[SG_SQ(ax, ay)] == SG_PC(by, SG_KNIGHT)) return 1;
    }
    for (int d = 0; d < 8; d++) {               /* a slide: the first piece back along the line */
        int dx = SL[d][0] * turn, dy = SL[d][1] * turn;
        for (int ax = tx - dx, ay = ty - dy; inside(n, ax, ay); ax -= dx, ay -= dy) {
            int pc = p->sq[SG_SQ(ax, ay)];
            if (!pc) continue;
            if (SG_OWNER(pc) == by && (FI[SG_FACE(pc)].slide >> d & 1)) return 1;
            break;
        }
    }
    return 0;
}

static int king_of(const spos *p, int player)
{
    for (int s = 0; s < 81; s++)
        if (p->sq[s] == SG_PC(player, SG_KING)) return s;
    return -1;
}

static int in_check(const spos *p, int player)
{
    int k = king_of(p, player);
    return k >= 0 && attacked(p, k, !player);
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
typedef struct { smove *out; int n; } mlist;

static void add(mlist *l, int from, int to, int drop, int promo)
{
    l->out[l->n++] = (smove){(uint8_t)from, (uint8_t)to, (uint8_t)drop, (uint8_t)promo};
}

static void add_move(mlist *l, const spos *p, int from, int to)
{
    int pc = p->sq[from], me = SG_OWNER(pc), kind = SG_KIND(pc), n = p->n;
    int may = !(pc & SG_PROMOTED) && kind != SG_GOLD && kind != SG_KING;
    if (may && (zone(n, me, SG_ROW(from)) || zone(n, me, SG_ROW(to)))) {
        add(l, from, to, 0, 1);
        if (dead(n, kind, me, SG_ROW(to))) return;
    }
    add(l, from, to, 0, 0);
}

static void pseudo(const spos *p, mlist *l)
{
    int me = p->side, turn = me ? -1 : 1, n = p->n;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int s = SG_SQ(x, y), pc = p->sq[s];
            if (!pc || SG_OWNER(pc) != me) continue;
            finfo f = FI[SG_FACE(pc)];
            for (int d = 0; d < 8; d++) {
                if (!(f.step >> d & 1)) continue;
                int tx = x + D8[d][0] * turn, ty = y + D8[d][1] * turn;
                if (!inside(n, tx, ty)) continue;
                int t = p->sq[SG_SQ(tx, ty)];
                if (!t || SG_OWNER(t) != me) add_move(l, p, s, SG_SQ(tx, ty));
            }
            if (f.knight)
                for (int sx = -1; sx <= 1; sx += 2) {
                    int tx = x + sx * turn, ty = y - 2 * turn;
                    if (!inside(n, tx, ty)) continue;
                    int t = p->sq[SG_SQ(tx, ty)];
                    if (!t || SG_OWNER(t) != me) add_move(l, p, s, SG_SQ(tx, ty));
                }
            for (int d = 0; d < 8; d++) {
                if (!(f.slide >> d & 1)) continue;
                int dx = SL[d][0] * turn, dy = SL[d][1] * turn;
                for (int tx = x + dx, ty = y + dy; inside(n, tx, ty); tx += dx, ty += dy) {
                    int t = p->sq[SG_SQ(tx, ty)];
                    if (t && SG_OWNER(t) == me) break;
                    add_move(l, p, s, SG_SQ(tx, ty));
                    if (t) break;
                }
            }
        }
    for (int k = SG_PAWN; k <= SG_ROOK; k++) {
        if (!p->hand[me][k]) continue;
        int pawn_on[9] = {0};                   /* no second unpromoted pawn of one's own on a file */
        if (k == SG_PAWN)
            for (int s = 0; s < 81; s++)
                if (p->sq[s] == SG_PC(me, SG_PAWN)) pawn_on[SG_COL(s)] = 1;
        for (int y = 0; y < n; y++) {
            if (dead(n, k, me, y)) continue;
            for (int x = 0; x < n; x++)
                if (!p->sq[SG_SQ(x, y)] && !pawn_on[x]) add(l, 0, SG_SQ(x, y), k, 0);
        }
    }
}

/* the side to move has a legal move (the pawn drop rule aside, as the rules') */
static int any_legal(const spos *p)
{
    smove buf[SG_MAX_MOVES * 2];
    mlist l = {buf, 0};
    pseudo(p, &l);
    for (int i = 0; i < l.n; i++) {
        spos q = *p;
        sg_make(&q, buf[i]);
        if (!in_check(&q, p->side)) return 1;
    }
    return 0;
}

/* the legal moves; their count */
int sgb_legal(const spos *p, smove *out)
{
    smove buf[SG_MAX_MOVES * 2];
    mlist l = {buf, 0};
    pseudo(p, &l);
    int me = p->side, k = 0, ek = king_of(p, !me), front = ek >= 0 ? ek + (me ? -9 : 9) : -1;
    for (int i = 0; i < l.n; i++) {
        spos q = *p;
        sg_make(&q, buf[i]);
        if (in_check(&q, me)) continue;
        /* a pawn dropped in front of the king (a check) that leaves it no move: not allowed */
        if (buf[i].drop == SG_PAWN && buf[i].to == front && !any_legal(&q)) continue;
        out[k++] = buf[i];
    }
    return k;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static uint64_t key_of(const spos *p)
{
    uint64_t h = cb_board_key(p->sq, 81, 0x5609ull + p->side);
    for (int c = 0; c < 2; c++)
        for (int k = SG_PAWN; k <= SG_ROOK; k++)
            if (p->hand[c][k]) h ^= cb_mix(0x4400000ull + (uint64_t)((c * 8 + k) * 32 + p->hand[c][k]));
    return h;
}

static int enc(smove m) { return m.from | m.to << 7 | m.drop << 14 | m.promo << 17; }
static smove dec(int x)
{
    smove m = {(uint8_t)(x & 127), (uint8_t)(x >> 7 & 127), (uint8_t)(x >> 14 & 7), (uint8_t)(x >> 17 & 1)};
    return m;
}

static void a_make(void *st, int mv)
{
    sbpos *s = st;
    s->ply++;
    if (mv == CB_END) {
        s->end = 1;
        s->key ^= 0xe4d;
        return;
    }
    cb_hist_push(&s->h, s->key);
    sg_make(&s->p, dec(mv));
    s->key = key_of(&s->p);
}

static int a_side(const void *st) { return ((const sbpos *)st)->p.side; }

static int a_over(const void *st, int *score)
{
    const sbpos *s = st;
    if (s->end) { *score = -ai_win(0); return 1; }   /* no legal move: lost */
    if (s->ply && cb_hist_seen(&s->h, s->key, CB_HIST, 2)) {
        *score = s->p.n == 5 ? (s->p.side ? ai_win(0) : -ai_win(0)) : 0;   /* mini shogi: sente loses */
        return 1;
    }
    return 0;
}

/* at most AI_MAX_MOVES: past it (a huge hand), the drops far from the kings are left out */
static int to_ints(const spos *p, const smove *l, int n, int *out)
{
    int k = 0;
    if (n <= AI_MAX_MOVES) {
        for (int i = 0; i < n; i++) out[k++] = enc(l[i]);
        return k;
    }
    for (int i = 0; i < n && k < AI_MAX_MOVES; i++)
        if (!l[i].drop) out[k++] = enc(l[i]);
    int ek = king_of(p, !p->side);
    for (int far = 0; far <= 8 && k < AI_MAX_MOVES; far++)
        for (int i = 0; i < n && k < AI_MAX_MOVES; i++)
            if (l[i].drop && ek >= 0 &&
                cb_cheb(SG_COL(l[i].to), SG_ROW(l[i].to), SG_COL(ek), SG_ROW(ek)) == far)
                out[k++] = enc(l[i]);
    return k;
}

static int a_moves(const void *st, int *out)
{
    const spos *p = &((const sbpos *)st)->p;
    smove l[SG_MAX_MOVES];
    int n = sgb_legal(p, l);
    if (!n) { out[0] = CB_END; return 1; }
    return to_ints(p, l, n, out);
}

static int a_noisy(const void *st, int *out)
{
    const spos *p = &((const sbpos *)st)->p;
    smove l[SG_MAX_MOVES];
    int n = sgb_legal(p, l), k = 0;
    if (!n) { out[0] = CB_END; return 1; }
    if (in_check(p, p->side)) return to_ints(p, l, n, out);
    for (int i = 0; i < n; i++)
        if (!l[i].drop && (p->sq[l[i].to] || (l[i].promo && SG_KIND(p->sq[l[i].from]) >= SG_SILVER))) out[k++] = enc(l[i]);
    return k;
}

static uint64_t a_hash(const void *st) { return ((const sbpos *)st)->key; }

/* the pieces' worth by face (kind | promoted); in hand by kind */
static const int VAL[32] = {
    [SG_PAWN] = 100, [SG_LANCE] = 300, [SG_KNIGHT] = 350, [SG_SILVER] = 480, [SG_GOLD] = 550, [SG_BISHOP] = 650,
    [SG_ROOK] = 800, [SG_KING] = 0, [SG_PAWN | SG_PROMOTED] = 530, [SG_LANCE | SG_PROMOTED] = 520,
    [SG_KNIGHT | SG_PROMOTED] = 520, [SG_SILVER | SG_PROMOTED] = 540, [SG_BISHOP | SG_PROMOTED] = 900,
    [SG_ROOK | SG_PROMOTED] = 1050,
};

static int a_order(const void *st, int mv)
{
    if (mv == CB_END) return 0;
    const spos *p = &((const sbpos *)st)->p;
    smove m = dec(mv);
    if (m.drop) {                               /* next to the enemy king first */
        int ek = king_of(p, !p->side);
        int d = ek >= 0 ? cb_cheb(SG_COL(m.to), SG_ROW(m.to), SG_COL(ek), SG_ROW(ek)) : 9;
        return (d <= 1 ? 40 : d == 2 ? 20 : 0) - m.drop;
    }
    int v = 0, mover = SG_FACE(p->sq[m.from]), victim = p->sq[m.to];
    if (victim) v += cb_mvvlva(SG_KIND(victim) == SG_KING ? 3000 : VAL[SG_FACE(victim)], mover == SG_KING ? 1200 : VAL[mover]);
    if (m.promo) v += VAL[mover | SG_PROMOTED] - VAL[mover];
    return v;
}

/* ---- the evaluation ---------------------------------------------------------------------------------------------- */
static int a_eval(const void *st)
{
    const spos *p = &((const sbpos *)st)->p;
    int us = p->side, n = p->n, v[2] = {0, 0}, k[2] = {king_of(p, 0), king_of(p, 1)}, hand[2] = {0, 0};
    for (int c = 0; c < 2; c++)
        for (int t = SG_PAWN; t <= SG_ROOK; t++) hand[c] += p->hand[c][t] * VAL[t] * 11 / 10;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int pc = p->sq[SG_SQ(x, y)];
            if (!pc) continue;
            int c = SG_OWNER(pc), face = SG_FACE(pc), rr = n - 1 - ahead(n, c, y);   /* rows from the own side */
            v[c] += VAL[face];
            if (face == SG_PAWN) v[c] += 3 * rr;
            if (face == SG_KING) {
                v[c] += rr <= 1 ? 20 : -15 * rr;
                continue;
            }
            for (int o = 0; o < 2; o++) {       /* near a king: a guard of its own, a threat of the other's */
                if (k[o] < 0) continue;
                int d = cb_cheb(x, y, SG_COL(k[o]), SG_ROW(k[o]));
                if (d > 2) continue;
                if (o == c) v[c] += (face == SG_GOLD || face == SG_SILVER || (face & SG_PROMOTED)) ? (d == 1 ? 28 : 12) : 4;
                else v[c] += d == 1 ? 45 : 22;
            }
        }
    for (int c = 0; c < 2; c++) {
        v[c] += hand[c];
        if (k[c] < 0) continue;
        int open = 0, kx = SG_COL(k[c]), ky = SG_ROW(k[c]);
        for (int d = 0; d < 8; d++) {
            int x = kx + D8[d][0], y = ky + D8[d][1];
            open += inside(n, x, y) && !p->sq[SG_SQ(x, y)];
        }
        v[c] -= open * (6 + hand[!c] / 70);    /* the open squares round the king against the other's hand */
    }
    return v[us] - v[!us] + 10;
}

static const ai_game ADAPTER = {
    .size = sizeof(sbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};
static const ai_game ADAPTER_EASY = {           /* easy: no quiescence (a longer exchange escapes it) */
    .size = sizeof(sbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

static const cb_levels LEVELS = {
    .nodes = {[AI_EASY] = 8000, [AI_MEDIUM] = 50000, [AI_HARD] = 170000},    /* hard: about 0.3 s on a PC */
    .depth = {[AI_EASY] = 2, [AI_MEDIUM] = 4, [AI_HARD] = 0},
    .noise = {[AI_EASY] = 100, [AI_MEDIUM] = 25, [AI_HARD] = 0},
    .tt_bits = 18,
};

void sgb_root(const sgame *g, void *root)
{
    sbpos *s = root;
    memset(s, 0, sizeof *s);
    s->p = *sg_pos(g);
    s->key = key_of(&s->p);
    for (int i = g->ply - CB_HIST < 0 ? 0 : g->ply - CB_HIST; i < g->ply; i++) cb_hist_push(&s->h, key_of(&g->pos[i]));
}

int sgb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats)
{
    const sbpos *s = root;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, s->key));
    ai_opts o = cb_opts(&LEVELS, level, &rng);
    if (level >= AI_HARD && s->p.n == 5) o.budget.nodes = o.budget.nodes * 3 / 2;   /* mini shogi: cheaper nodes */
    int mv = ai_ab(level <= AI_EASY ? &ADAPTER_EASY : &ADAPTER, s, &o, stats);
    if (mv < 0 || mv == CB_END) return 0;
    sg_usi(s->p.n, dec(mv), out);
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    sbpos *root = malloc(sizeof *root);         /* the copy, at entry: nothing else is read */
    if (!root) return 0;
    sgb_root(shogi_live(), root);
    smove l[SG_MAX_MOVES];
    char m[8];
    int ok = root->p.side == seat && cap >= 6 && sgb_legal(&root->p, l) > 0 &&
             sgb_choose(root, level, ai_session_seed(), m, NULL) && !ai_cancelled();
    free(root);
    if (ok) memcpy(move, m, strlen(m) + 1);
    return ok;
}

static const bot shogi_bot = {.think = think, .play = shogi_show};
REGISTER_BOT(shogi, &shogi_bot)
