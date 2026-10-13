/*
 * Makruk's bot (bots.h): alpha-beta (ai.h) over the rules' moves (makruk_rules.c), a transposition table, the
 * captures first (most valuable victim, least valuable attacker), killers and history, a quiescence on the captures
 * and the promotions (every evasion when in check). The evaluation: material, the pieces' squares (the Ma and the
 * Khon to the centre, the Bia's advance to its promotion, the Khun behind its men, then to the centre in the end),
 * the lone Khun driven to a corner (the counting runs: the stronger side hurries, and its edge melts as the count
 * nears its limit, a draw). Draws by the rules: stalemate, the two Khun alone, the count's limit, a repetition.
 * Runs on the worker thread: it copies the position (and the keys of the positions before it) at entry and touches
 * nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("e3e4"), played by makruk_show().
 */
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/chessbots.h"
#include "games/makruk.h"

typedef struct {
    mkpos p;
    uint64_t rkey, key;                         /* rkey: the board and the side (repetitions); key: and the count */
    int8_t k[2];
    uint8_t end, ply, men;                      /* men: the pieces on the board */
    cb_hist h;
} mbpos;

static const int VAL[7] = {0, 100, 320, 260, 500, 190, 0};   /* Bia Ma Khon Rua Met */

static void derive(mbpos *s)
{
    const mkpos *p = &s->p;
    s->rkey = cb_board_key(p->sq, 64, 0x3a4c0ull + p->side);
    s->key = s->rkey ^ cb_mix(0x77000000ull + (uint64_t)(p->count_kind | p->counter << 2 | p->count << 3 | p->limit << 11));
    s->k[0] = s->k[1] = -1;
    s->men = 0;
    for (int q = 0; q < 64; q++)
        if (p->sq[q]) {
            s->men++;
            if (MK_KIND(p->sq[q]) == MK_KHUN) s->k[MK_COLOR(p->sq[q])] = (int8_t)q;
        }
}

static int enc(mkmove m) { return m.from | m.to << 6 | m.promo << 12 | m.taken << 13; }
static mkmove dec(int x)
{
    mkmove m = {(uint8_t)(x & 63), (uint8_t)(x >> 6 & 63), (uint8_t)(x >> 12 & 1), (uint8_t)(x >> 13 & 31)};
    return m;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static void a_make(void *st, int mv)
{
    mbpos *s = st;
    s->ply++;
    if (mv == CB_END) {
        s->end = 1;
        s->key ^= 0xe4d;
        return;
    }
    cb_hist_push(&s->h, s->rkey);
    mk_make(&s->p, dec(mv));
    if (!s->p.quiet) cb_hist_clear(&s->h);
    derive(s);
}

static int a_side(const void *st) { return ((const mbpos *)st)->p.side; }

static int a_over(const void *st, int *score)
{
    const mbpos *s = st;
    const mkpos *p = &s->p;
    if (s->end) {
        *score = mk_in_check(p, p->side) ? -ai_win(0) : 0;
        return 1;
    }
    if (s->men <= 2 || (p->count_kind != MK_COUNT_NONE && p->side == p->counter && p->count >= p->limit) ||
        (s->ply && cb_hist_seen(&s->h, s->rkey, p->quiet, 2))) {
        *score = 0;
        return 1;
    }
    return 0;
}

static int a_moves(const void *st, int *out)
{
    mkmove l[MK_MAX_MOVES];
    int n = mk_legal(&((const mbpos *)st)->p, l);
    if (!n) { out[0] = CB_END; return 1; }
    for (int i = 0; i < n; i++) out[i] = enc(l[i]);
    return n;
}

static int a_noisy(const void *st, int *out)
{
    const mbpos *s = st;
    mkmove l[MK_MAX_MOVES];
    int n = mk_legal(&s->p, l), k = 0;
    if (!n) { out[0] = CB_END; return 1; }
    int all = s->k[s->p.side] >= 0 && mk_in_check(&s->p, s->p.side);
    for (int i = 0; i < n; i++)
        if (all || l[i].taken || l[i].promo) out[k++] = enc(l[i]);
    return k;
}

static uint64_t a_hash(const void *st) { return ((const mbpos *)st)->key; }

static int a_order(const void *st, int mv)
{
    if (mv == CB_END) return 0;
    const mbpos *s = st;
    mkmove m = dec(mv);
    int v = 0, mover = MK_KIND(s->p.sq[m.from]);
    if (m.taken) v += cb_mvvlva(VAL[MK_KIND(m.taken)], mover == MK_KHUN ? 600 : VAL[mover]);
    if (m.promo) v += 150;
    return v;
}

/* ---- the evaluation ---------------------------------------------------------------------------------------------- */
static int centre(int s) { return 7 - (cb_abs(2 * MK_FILE(s) - 7) + cb_abs(2 * MK_RANK(s) - 7)) / 2; }

static const int BIA[8] = {0, 0, 0, 8, 22, 0, 0, 0};   /* rows from the own side: the third, the fourth, the fifth */

static int a_eval(const void *st)
{
    const mbpos *s = st;
    const mkpos *p = &s->p;
    int us = p->side, mat[2] = {0, 0}, pos[2] = {0, 0}, bia[2] = {0, 0}, minor[2] = {0, 0}, end;
    for (int q = 0; q < 64; q++) {
        int pc = p->sq[q];
        if (!pc) continue;
        int c = MK_COLOR(pc), t = MK_KIND(pc);
        mat[c] += VAL[t];
        if (t == MK_BIA) bia[c]++;
        else if (t != MK_KHUN) minor[c] += VAL[t];
    }
    end = minor[0] + minor[1] <= 1400;          /* few pieces: the Khun comes out */
    for (int q = 0; q < 64; q++) {
        int pc = p->sq[q];
        if (!pc) continue;
        int c = MK_COLOR(pc), t = MK_KIND(pc), rr = c == MK_WHITE ? MK_RANK(q) : 7 - MK_RANK(q), cen = centre(q);
        switch (t) {
        case MK_BIA: pos[c] += BIA[rr] + (MK_FILE(q) >= 2 && MK_FILE(q) <= 5 ? 4 : 0); break;
        case MK_MA: pos[c] += 5 * cen - 12; break;
        case MK_KHON: pos[c] += 3 * cen - 6; break;
        case MK_MET: pos[c] += 3 * cen; break;
        case MK_RUA: pos[c] += rr == 5 || rr == 6 ? 12 : 0; break;
        case MK_KHUN: pos[c] += end ? 6 * cen - 18 : rr == 0 ? 10 : -20 * rr; break;
        }
    }
    int lead = mat[us] - mat[!us], win = lead > 0 ? us : !us, score;
    /* the lone Khun (or nearly) to a corner, the other Khun close: the mate before the count ends */
    if (cb_abs(lead) >= 150 && !bia[!win] && minor[!win] <= 190 && s->k[0] >= 0 && s->k[1] >= 0) {
        int lk = s->k[!win], wk = s->k[win], lf = MK_FILE(lk), lr = MK_RANK(lk);
        int corner = cb_min(lf, 7 - lf) + cb_min(lr, 7 - lr);
        pos[win] += 14 * (7 - corner) + 5 * (14 - cb_abs(MK_FILE(lk) - MK_FILE(wk)) - cb_abs(MK_RANK(lk) - MK_RANK(wk)));
    }
    score = mat[us] - mat[!us] + pos[us] - pos[!us] + 8;
    int left = mk_moves_left(p);
    if (left >= 0 && left < 24) score = score * (left + 1) / 25;   /* the count nearly out: a draw */
    return score;
}

static const ai_game ADAPTER = {
    .size = sizeof(mbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};
static const ai_game ADAPTER_EASY = {           /* easy: no quiescence (a longer exchange escapes it) */
    .size = sizeof(mbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

static const cb_levels LEVELS = {
    .nodes = {[AI_EASY] = 8000, [AI_MEDIUM] = 50000, [AI_HARD] = 120000},    /* hard: about 0.3 s on a PC */
    .depth = {[AI_EASY] = 2, [AI_MEDIUM] = 4, [AI_HARD] = 0},
    .noise = {[AI_EASY] = 100, [AI_MEDIUM] = 25, [AI_HARD] = 0},
    .tt_bits = 18,
};

void mkb_root(const mkgame *g, void *root)
{
    mbpos *s = root;
    memset(s, 0, sizeof *s);
    s->p = *mk_pos(g);
    derive(s);
    int back = cb_min(s->p.quiet, CB_HIST), from = g->ply - back < 0 ? 0 : g->ply - back;
    for (int i = from; i < g->ply; i++) cb_hist_push(&s->h, cb_board_key(g->pos[i].sq, 64, 0x3a4c0ull + g->pos[i].side));
}

int mkb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats)
{
    const mbpos *s = root;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, s->key));
    ai_opts o = cb_opts(&LEVELS, level, &rng);
    int mv = ai_ab(level <= AI_EASY ? &ADAPTER_EASY : &ADAPTER, s, &o, stats);
    if (mv < 0 || mv == CB_END) return 0;
    mkmove m = dec(mv);
    out[0] = (char)('a' + MK_FILE(m.from));
    out[1] = (char)('1' + MK_RANK(m.from));
    out[2] = (char)('a' + MK_FILE(m.to));
    out[3] = (char)('1' + MK_RANK(m.to));
    out[4] = 0;
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    mbpos *root = malloc(sizeof *root);         /* the copy, at entry: nothing else is read */
    if (!root) return 0;
    mkb_root(makruk_live(), root);
    mkmove l[MK_MAX_MOVES];
    char m[8];
    int ok = root->p.side == seat && cap >= 5 && mk_legal(&root->p, l) > 0 &&
             mkb_choose(root, level, ai_session_seed(), m, NULL) && !ai_cancelled();
    free(root);
    if (ok) memcpy(move, m, 5);
    return ok;
}

static const bot makruk_bot = {.think = think, .play = makruk_show};
REGISTER_BOT(makruk, &makruk_bot)
