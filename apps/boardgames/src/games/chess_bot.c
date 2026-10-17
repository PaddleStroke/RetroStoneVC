/*
 * Chess's bot (bots.h), one core for all nine variants: alpha-beta (ai.h) over the rules' own moves (chess.c,
 * perft-tested: every variant's rules come with them), a transposition table, the captures first (most valuable
 * victim, least valuable attacker; atomic: what the blast takes), killers and history, and a quiescence on the
 * captures (and the evasions when in check; the checks too in three-check, where they are few and decide).
 * The evaluation: material and piece-square terms (formulas: the centre, the pawns' advance, the king's shelter in
 * the middle game and its walk to the centre in the end), the pawns (passed, doubled, isolated), rooks on open files,
 * the bishop pair, the lone king driven to the edge; and each variant's own terms:
 *   crazyhouse    the pieces in hand (worth a little more than on the board), the king's open squares against them;
 *   three-check   the checks given (the second is worth a lot), the king's open squares;
 *   king of the hill  each king's distance to the hill;
 *   losing chess  fewer pieces is better (the captures are forced: the rules give only them);
 *   atomic        the own pieces round the own king (a capture there blows it up), more so when attacked;
 *   racing kings  each king's rank;
 *   horde         the pawns' advance (white has no king).
 * Runs on the worker thread: it copies the position (and the keys of the positions before it) at entry and touches
 * nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: "e2e4", "e7e8q", castling the king onto its rook ("e1h1": Chess960's way, which
 * chess_ui.c's script() takes in every variant), drops "N@f3"; played by chess_show().
 */
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/chess.h"
#include "games/chessbots.h"

typedef struct {
    cpos p;
    uint64_t key;
    int8_t k[2];                                /* the kings' squares (-1: none: the horde, atomic once blown up) */
    uint8_t end, ply;                           /* end: CB_END played (no legal move); ply: from the root */
    cb_hist h;
} cbpos;

static const int VAL[7] = {0, 100, 320, 330, 500, 900, 0};
static const int VAL_ZH[7] = {0, 100, 260, 280, 380, 700, 0};   /* crazyhouse: a piece is never gone for good */

/* ---- the position's key, the moves' encoding --------------------------------------------------------------------- */
static uint64_t key_of(const cpos *p)
{
    uint64_t h = cb_board_key(p->sq, 64, 0xc4e55ull + p->side);
    h ^= cb_mix(0x1000000ull + (uint64_t)(p->castle | (p->ep + 1) << 4 | p->checks[0] << 12 | p->checks[1] << 14 |
                                          p->side << 16));
    if (p->rules == CR_CRAZYHOUSE) {
        for (int c = 0; c < 2; c++)
            for (int t = PAWN; t <= QUEEN; t++)
                if (p->hand[c][t]) h ^= cb_mix(0x2000000ull + (uint64_t)((c * 8 + t) * 64 + p->hand[c][t]));
        h ^= cb_mix(p->promoted ^ 0x3000000ull);
    }
    return h;
}

static int enc(cmove m) { return m.from | m.to << 6 | m.promo << 12 | m.flags << 15 | m.drop << 20; }
static cmove dec(int x)
{
    cmove m = {(uint8_t)(x & 63), (uint8_t)(x >> 6 & 63), (uint8_t)(x >> 12 & 7), (uint8_t)(x >> 15 & 31),
               (uint8_t)(x >> 20 & 7)};
    return m;
}

static void kings(cbpos *s)
{
    s->k[0] = s->k[1] = -1;
    for (int q = 0; q < 64; q++)
        if (TYPE(s->p.sq[q]) == KING && s->p.rules != CR_ANTI) s->k[COLOR(s->p.sq[q])] = (int8_t)q;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static void a_make(void *st, int mv)
{
    cbpos *s = st;
    s->ply++;
    if (mv == CB_END) {
        s->end = 1;
        s->key ^= 0xe4d;
        return;
    }
    cb_hist_push(&s->h, s->key);
    chess_make(&s->p, dec(mv));
    if (!s->p.halfmove) cb_hist_clear(&s->h);   /* a capture or a pawn's move: nothing before repeats */
    s->key = key_of(&s->p);
    kings(s);
}

static int a_side(const void *st) { return ((const cbpos *)st)->p.side; }

static int a_over(const void *st, int *score)
{
    const cbpos *s = st;
    const cpos *p = &s->p;
    int us = p->side;
    if (s->end) {                               /* no legal move: mate, stalemate; losing chess: a win */
        *score = p->rules == CR_ANTI ? ai_win(0) : chess_in_check(p, us) ? -ai_win(0) : 0;
        return 1;
    }
    int o = p->rules == CR_CHESS ? 0 : chess_over(p);
    switch (o) {
    case 0: break;
    case ST_EXPLODED: *score = s->k[us] < 0 ? -ai_win(0) : ai_win(0); return 1;
    case ST_RACE: *score = (s->k[BLACK] >= 0 && RANK_OF(s->k[BLACK]) == 7) == (us == BLACK) ? ai_win(0) : -ai_win(0);
        return 1;
    case ST_RACE_DRAW: *score = 0; return 1;
    case ST_HORDE: *score = us == WHITE ? -ai_win(0) : ai_win(0); return 1;
    default: *score = -ai_win(0); return 1;     /* the third check, the other king on the hill: the mover won */
    }
    if (s->ply && (p->halfmove >= 100 || cb_hist_seen(&s->h, s->key, p->halfmove, 2))) {
        *score = 0;
        return 1;
    }
    return 0;
}

static int a_moves(const void *st, int *out)
{
    const cbpos *s = st;
    cmove l[MAX_MOVES];
    int n = chess_legal(&s->p, l);
    if (!n) { out[0] = CB_END; return 1; }
    for (int i = 0; i < n; i++) out[i] = enc(l[i]);
    return n;
}

/* quiescence's moves: the captures and the queen's promotions; every evasion when in check; the variants' own
 * sudden ends too (a king onto the hill or the eighth rank, three-check's checks) */
static int a_noisy(const void *st, int *out)
{
    const cbpos *s = st;
    const cpos *p = &s->p;
    cmove l[MAX_MOVES];
    int n = chess_legal(p, l), k = 0, us = p->side;
    if (!n) { out[0] = CB_END; return 1; }
    int all = s->k[us] >= 0 && chess_in_check(p, us);
    for (int i = 0; i < n; i++) {
        cmove m = l[i];
        int keep = all || (m.flags & MV_CAPTURE) || m.promo == QUEEN;
        if (!keep && TYPE(p->sq[m.from]) == KING && !(m.flags & (MV_CASTLE | MV_DROP)))
            keep = (p->rules == CR_HILL && CHESS_HILL(m.to)) || (p->rules == CR_RACING && RANK_OF(m.to) == 7);
        if (!keep && p->rules == CR_THREECHECK && s->k[!us] >= 0) {
            cpos q = *p;
            chess_make(&q, m);
            keep = chess_in_check(&q, !us);
        }
        if (keep) out[k++] = enc(m);
    }
    return k;
}

static uint64_t a_hash(const void *st) { return ((const cbpos *)st)->key; }

/* atomic: what a capture on `to` by the piece on `from` blows up, theirs less ours (a king: the game) */
static int blast(const cpos *p, int from, int to, int victim)
{
    int us = p->side, v = VAL[victim] - VAL[TYPE(p->sq[from])];
    for (int df = -1; df <= 1; df++)
        for (int dr = -1; dr <= 1; dr++) {
            int f = FILE_OF(to) + df, r = RANK_OF(to) + dr;
            if ((!df && !dr) || f < 0 || f > 7 || r < 0 || r > 7) continue;
            int q = p->sq[SQ(f, r)], t = TYPE(q);
            if (!q || t == PAWN || SQ(f, r) == from) continue;
            int worth = t == KING ? 20000 : VAL[t];
            v += COLOR(q) == us ? -worth : worth;
        }
    return v;
}

static int a_order(const void *st, int mv)
{
    if (mv == CB_END) return 0;
    const cbpos *s = st;
    const cpos *p = &s->p;
    cmove m = dec(mv);
    int us = p->side, v = 0;
    if (m.flags & MV_DROP) {                    /* crazyhouse: a drop next to their king first */
        int ek = s->k[!us];
        if (ek >= 0) {
            int d = cb_cheb(FILE_OF(m.to), RANK_OF(m.to), FILE_OF(ek), RANK_OF(ek));
            v += d <= 1 ? 40 : d == 2 ? 20 : 0;
        }
        return v - m.drop;
    }
    int mover = TYPE(p->sq[m.from]);
    if (m.flags & MV_CAPTURE) {
        int victim = m.flags & MV_EP ? PAWN : TYPE(p->sq[m.to]);
        if (p->rules == CR_ATOMIC) v += 1000 + blast(p, m.from, m.to, victim) / 4;
        else if (p->rules == CR_ANTI) v += 1000 + VAL[victim] / 8 + (mover == KING ? 50 : VAL[mover] / 8);
        else v += cb_mvvlva(victim == KING ? 2000 : VAL[victim], mover == KING ? 1000 : VAL[mover]);
    }
    if (m.promo) v += m.promo == QUEEN ? 900 : p->rules == CR_ANTI ? 100 : -50;
    if (m.flags & MV_CASTLE) v += 30;
    if (mover == KING && !(m.flags & MV_CASTLE)) {
        if (p->rules == CR_RACING) v += 60 * (RANK_OF(m.to) - RANK_OF(m.from));
        if (p->rules == CR_HILL) {
            int a = cb_min(cb_cheb(FILE_OF(m.from), RANK_OF(m.from), 3, 3), cb_cheb(FILE_OF(m.from), RANK_OF(m.from), 4, 4));
            int b = cb_min(cb_cheb(FILE_OF(m.to), RANK_OF(m.to), 3, 3), cb_cheb(FILE_OF(m.to), RANK_OF(m.to), 4, 4));
            v += 60 * (a - b);
        }
    }
    return v;
}

/* ---- the evaluation ---------------------------------------------------------------------------------------------- */
/* the centre: 0 in a corner .. 6 on d4 e4 d5 e5 */
static int centre(int s) { return 7 - (cb_abs(2 * FILE_OF(s) - 7) + cb_abs(2 * RANK_OF(s) - 7)) / 2; }

/* the hill's distance: 0 on it .. 3 */
static int hill_d(int s)
{
    int f = FILE_OF(s), r = RANK_OF(s);
    return cb_max(f < 3 ? 3 - f : f > 4 ? f - 4 : 0, r < 3 ? 3 - r : r > 4 ? r - 4 : 0);
}

/* the squares round the king (and the one in front of it) that are empty: the ways in */
static int open_round(const cpos *p, int k, int c)
{
    int n = 0;
    for (int df = -1; df <= 1; df++)
        for (int dr = -1; dr <= 1; dr++) {
            int f = FILE_OF(k) + df, r = RANK_OF(k) + dr;
            if ((!df && !dr) || f < 0 || f > 7 || r < 0 || r > 7) continue;
            int q = p->sq[SQ(f, r)];
            n += !q || COLOR(q) != c;
        }
    return n;
}

static const int P_MG[8] = {0, 0, 2, 6, 14, 24, 40, 0};
static const int P_EG[8] = {0, 0, 6, 12, 22, 38, 64, 0};
static const int PASS_MG[8] = {0, 4, 6, 12, 22, 38, 62, 0};
static const int PASS_EG[8] = {0, 10, 14, 26, 46, 76, 116, 0};
static const int K_FILE[8] = {20, 30, 10, -5, -5, 10, 30, 20};

static int eval_anti(const cpos *p)
{
    int n[2] = {0, 0}, kings[2] = {0, 0};
    for (int s = 0; s < 64; s++)
        if (p->sq[s]) {
            n[COLOR(p->sq[s])]++;
            kings[COLOR(p->sq[s])] += TYPE(p->sq[s]) == KING;
        }
    int us = p->side;
    /* fewer pieces is better; a piece that can never be taken by force (none to take it) is a burden late */
    return 100 * (n[!us] - n[us]) + 10 * (kings[us] - kings[!us]);
}

static int a_eval(const void *st)
{
    const cbpos *s = st;
    const cpos *p = &s->p;
    int rules = p->rules, us = p->side;
    if (rules == CR_ANTI) return eval_anti(p);
    const int *V = rules == CR_CRAZYHOUSE ? VAL_ZH : VAL;
    int mat[2] = {0, 0}, npm[2] = {0, 0}, mg[2] = {0, 0}, eg[2] = {0, 0}, bish[2] = {0, 0}, pawns[2] = {0, 0};
    int add[2] = {0, 0};                        /* not by phase */
    int8_t hi[2][8], lo[2][8];
    memset(hi, -1, sizeof hi);
    memset(lo, 8, sizeof lo);
    for (int q = 0; q < 64; q++) {
        int pc = p->sq[q];
        if (!pc) continue;
        int c = COLOR(pc), t = TYPE(pc), r = RANK_OF(q), f = FILE_OF(q);
        if (t == PAWN) {
            pawns[c]++;
            if (r > hi[c][f]) hi[c][f] = (int8_t)r;
            if (r < lo[c][f]) lo[c][f] = (int8_t)r;
        } else if (t != KING) npm[c] += V[t];
        if (t == BISHOP) bish[c]++;
        mat[c] += V[t];
    }
    if (rules == CR_CRAZYHOUSE)
        for (int c = 0; c < 2; c++)
            for (int t = PAWN; t <= QUEEN; t++) mat[c] += p->hand[c][t] * V[t] * 6 / 5;
    int all = npm[0] + npm[1], phase = all >= 6400 ? 256 : all * 256 / 6400;
    if (rules == CR_CRAZYHOUSE || rules == CR_THREECHECK) phase = 256;   /* the king is never safe there */

    for (int q = 0; q < 64; q++) {
        int pc = p->sq[q];
        if (!pc) continue;
        int c = COLOR(pc), t = TYPE(pc), f = FILE_OF(q), r = RANK_OF(q), rr = c == WHITE ? r : 7 - r, cen = centre(q);
        switch (t) {
        case PAWN: {
            int pm = P_MG[rr], pe = P_EG[rr];
            if (rules == CR_HORDE && c == WHITE) pm = pe;
            mg[c] += pm + ((f == 3 || f == 4) && (rr == 3 || rr == 4) ? 10 : 0);
            eg[c] += pe;
            int passed = 1;
            for (int ff = f - 1; ff <= f + 1 && passed; ff++)
                if (ff >= 0 && ff < 8) passed = c == WHITE ? hi[BLACK][ff] <= r : lo[WHITE][ff] >= r || lo[WHITE][ff] == 8;
            if (passed) {
                int ahead = q + (c == WHITE ? 8 : -8), free = ahead >= 0 && ahead < 64 && !p->sq[ahead];
                mg[c] += PASS_MG[rr];
                eg[c] += PASS_EG[rr] + (free ? PASS_EG[rr] / 4 : 0);
            }
            if ((c == WHITE ? lo[c][f] : hi[c][f]) != r) { mg[c] -= 8; eg[c] -= 12; }   /* doubled (the back one) */
            if ((f == 0 || hi[c][f - 1] < 0) && (f == 7 || hi[c][f + 1] < 0)) { mg[c] -= 8; eg[c] -= 10; }
            break;
        }
        case KNIGHT:
            mg[c] += 4 * cen - 10 - (rr == 0 ? 12 : 0);
            eg[c] += 4 * cen - 10;
            break;
        case BISHOP:
            mg[c] += 2 * cen - (rr == 0 ? 12 : 0);
            eg[c] += 2 * cen;
            break;
        case ROOK:
            if (rr == 6) { mg[c] += 20; eg[c] += 15; }
            if (hi[c][f] < 0) {
                int open = hi[!c][f] < 0;
                mg[c] += open ? 18 : 9;
                eg[c] += open ? 8 : 4;
            }
            break;
        case QUEEN:
            mg[c] += cen;
            eg[c] += 2 * cen;
            break;
        case KING:
            if (rules == CR_RACING) break;
            if (rr == 0) mg[c] += K_FILE[f];
            else mg[c] -= 30 * cb_min(rr, 3);
            for (int ff = f - 1; ff <= f + 1; ff++) {   /* the shelter: own pawns just in front */
                if (ff < 0 || ff > 7) continue;
                int r1 = r + (c == WHITE ? 1 : -1), r2 = r + (c == WHITE ? 2 : -2);
                if (r1 >= 0 && r1 < 8 && p->sq[SQ(ff, r1)] == PC(c, PAWN)) mg[c] += 12;
                else if (r2 >= 0 && r2 < 8 && p->sq[SQ(ff, r2)] == PC(c, PAWN)) mg[c] += 6;
                else if (hi[c][ff] < 0) mg[c] -= 10;
            }
            eg[c] += 6 * cen - 18;
            break;
        }
    }
    for (int c = 0; c < 2; c++) {
        if (bish[c] >= 2) { mg[c] += 25; eg[c] += 40; }
        int k = s->k[c];
        switch (rules) {
        case CR_CRAZYHOUSE:
            if (k >= 0) {
                int hand = 0;
                for (int t = PAWN; t <= QUEEN; t++) hand += p->hand[!c][t] * V[t];
                add[c] -= open_round(p, k, c) * (8 + hand / 40);
            }
            break;
        case CR_THREECHECK: {
            static const int GIVEN[4] = {0, 140, 460, 0};
            add[c] += GIVEN[p->checks[c] & 3];
            if (k >= 0) add[c] -= open_round(p, k, c) * (10 + 25 * p->checks[!c]);
            break;
        }
        case CR_HILL:
            if (k >= 0) {
                static const int NEAR[4] = {0, 240, 100, 30};
                add[c] += NEAR[hill_d(k)] * (512 - phase) / 256;
            }
            break;
        case CR_RACING:
            if (k >= 0) add[c] += 40 * RANK_OF(k) + 9 * RANK_OF(k) * RANK_OF(k);
            break;
        case CR_ATOMIC:
            if (k >= 0)
                for (int df = -1; df <= 1; df++)
                    for (int dr = -1; dr <= 1; dr++) {
                        int f = FILE_OF(k) + df, r = RANK_OF(k) + dr;
                        if ((!df && !dr) || f < 0 || f > 7 || r < 0 || r > 7) continue;
                        int q = p->sq[SQ(f, r)];
                        if (!q || COLOR(q) != c || TYPE(q) == KING) continue;
                        add[c] -= chess_attacked(p, SQ(f, r), !c) ? 70 : 12;
                    }
            break;
        }
    }
    /* the end: a lone king (or nearly) driven to the edge, the other king coming */
    int lead = mat[us] - mat[!us], win = lead > 0 ? us : !us;
    if (cb_abs(lead) >= 300 && !pawns[!win] && npm[!win] <= 330 && s->k[0] >= 0 && s->k[1] >= 0 && rules != CR_RACING) {
        int lk = s->k[!win], wk = s->k[win];
        add[win] += 12 * (6 - centre(lk)) + 6 * (14 - cb_abs(FILE_OF(lk) - FILE_OF(wk)) - cb_abs(RANK_OF(lk) - RANK_OF(wk)));
    }
    int v[2];
    for (int c = 0; c < 2; c++) v[c] = mat[c] + add[c] + (mg[c] * phase + eg[c] * (256 - phase)) / 256;
    int score = v[us] - v[!us] + 10;
    /* no pawns, a minor piece's edge or less: little chance (chess's and atomic's ends) */
    if ((rules == CR_CHESS || rules == CR_ATOMIC) && !pawns[win] && npm[win] - npm[!win] <= 330 && npm[win] <= 660)
        score /= 8;
    return score;
}

static const ai_game ADAPTER = {
    .size = sizeof(cbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};
/* easy: no quiescence, so a longer exchange than its two plies escapes it (a human's mistake) */
static const ai_game ADAPTER_EASY = {
    .size = sizeof(cbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* ---- the choice -------------------------------------------------------------------------------------------------- */
static const cb_levels LEVELS = {
    .nodes = {[AI_EASY] = 8000, [AI_MEDIUM] = 50000, [AI_HARD] = 400000},    /* hard: about 0.3 s on a PC */
    .depth = {[AI_EASY] = 2, [AI_MEDIUM] = 4, [AI_HARD] = 0},
    .noise = {[AI_EASY] = 100, [AI_MEDIUM] = 25, [AI_HARD] = 0},
    .tt_bits = 18,
};

/* the root: the position, and the keys of the positions since the last capture or pawn move */
void chb_root(const cgame *g, void *root)
{
    cbpos *s = root;
    memset(s, 0, sizeof *s);
    s->p = *game_pos(g);
    s->key = key_of(&s->p);
    kings(s);
    int back = cb_min(s->p.halfmove, CB_HIST), from = g->ply - back < 0 ? 0 : g->ply - back;
    for (int i = from; i < g->ply; i++) cb_hist_push(&s->h, key_of(&g->pos[i]));
}

/* the move for the root's side to move, at a level (its notation in out, 8 bytes); 0: none */
int chb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats)
{
    const cbpos *s = root;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, s->key));
    ai_opts o = cb_opts(&LEVELS, level, &rng);
    /* hard: the variants' nodes cost more or less (drops, blasts, the race's checks): about 0.3 s each on a PC */
    static const int PCT[8] = {[CR_CHESS] = 80, [CR_CRAZYHOUSE] = 58, [CR_THREECHECK] = 63, [CR_HILL] = 82,
                               [CR_ANTI] = 150, [CR_ATOMIC] = 55, [CR_RACING] = 46, [CR_HORDE] = 48};
    if (level >= AI_HARD) o.budget.nodes = o.budget.nodes * PCT[s->p.rules & 7] / 100;
    int mv = ai_ab(level <= AI_EASY ? &ADAPTER_EASY : &ADAPTER, s, &o, stats);
    if (mv < 0 || mv == CB_END) return 0;
    return chess_move_text(dec(mv), out, 8);
}

static int think(int seat, int level, char *move, int cap)
{
    cbpos *root = malloc(sizeof *root);         /* the copy, at entry: nothing else is read */
    if (!root) return 0;
    chb_root(chess_live(), root);
    cmove l[MAX_MOVES];
    char m[8];
    int ok = root->p.side == seat && cap >= 6 && (root->p.rules == CR_CHESS || !chess_over(&root->p)) &&
             chess_legal(&root->p, l) > 0 && chb_choose(root, level, ai_session_seed(), m, NULL) && !ai_cancelled();
    free(root);
    if (ok) memcpy(move, m, strlen(m) + 1);
    return ok;
}

static const bot chess_bot = {.think = think, .play = chess_show};
REGISTER_BOT(chess, &chess_bot)
