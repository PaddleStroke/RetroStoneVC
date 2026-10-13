/*
 * The climbing games' bot (Big Two, Tiến lên: games/shed.h), shared: information-set Monte Carlo tree search (ai_mcts)
 * over the deal, each iteration dealing the cards the seat has not seen (the pack minus its hand and every card
 * played) at random into the others' hands (the cards never dealt, with fewer players, out of play); heuristic
 * playouts to the deal's end. No SDL; static inline: a game's bot file includes it once with its rules.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The search's moves: a pass (0), a play of up to five cards by its cards (6 bits each: the power + 1), a longer one
 * (Tiến lên's sequences and double sequences) by its kind, length, first rank and top card, its other cards the lowest
 * of their ranks. The plays listed: every single; pairs and threes (the lowest cards with each top card); four alike;
 * Big Two's five-card hands (straights with each top card, flushes, full houses, four of a kind with the lowest
 * kicker, straight flushes); Tiến lên's sequences and double sequences (each length; following: each top card).
 * The playouts: lead the play that sheds the most low cards; follow with the cheapest play that beats, keeping the 2s
 * and the bombs while the hand is long, unless someone is about to go out. The reward: going out first (Big Two: the
 * others' penalty counts a little; Tiến lên: the places).
 */
#ifndef SHED_BOT_H
#define SHED_BOT_H

#include <stdio.h>

#include "ai.h"
#include "games/cardbot.h"
#include "games/shed.h"
#include "games/tricks.h"

#define SB_LONG (1 << 30)                       /* a long play: | kind << 24 | n << 16 | top power << 8 | rank */

typedef struct {
    shed_pos p;
    const shed_rules *R;
    uint64_t gone;                              /* the cards played this deal (shed_bit) */
    uint8_t me, places;                         /* places: the reward by places (Tiến lên), else by the first out */
} sb_st;

/* the cards of the hand by rank (0..12), low to high power */
typedef struct { uint8_t c[13][4], n[13]; } sb_ranks;

static inline void sb_by_rank(const shed_rules *R, const uint8_t *h, int n, sb_ranks *k)
{
    memset(k->n, 0, sizeof k->n);
    for (int i = 0; i < n; i++) {             /* the hand is sorted by power: each rank's cards in order */
        int c = h[i];
        if (c >= CDB_HIDDEN) continue;
        int r = shed_rank_index(c);
        if (k->n[r] < 4) k->c[r][k->n[r]++] = (uint8_t)c;
    }
    (void)R;
}

static inline int sb_code(const shed_rules *R, const uint8_t *cards, int n)   /* up to 5 cards, low to high */
{
    int m = 0;
    for (int i = 0; i < n; i++) m |= (R->power(cards[i]) + 1) << (6 * i);
    return m;
}

/* the move as the rules' action (a mask; 0: none) */
static inline uint64_t sb_real(const shed_rules *R, const shed_pos *p, int m)
{
    if (!m) return SHED_PASS;
    uint64_t a = 0;
    if (!(m & SB_LONG)) {
        const uint8_t *h = p->hand[p->turn];
        for (int i = 0; i < 5; i++) {
            int x = m >> (6 * i) & 63, found = 0;
            if (!x) break;
            for (int j = 0; j < p->n[p->turn] && !found; j++)   /* the card of that power, in the hand */
                if (h[j] < CDB_HIDDEN && R->power(h[j]) == x - 1) a |= shed_bit(h[j]), found = 1;
            if (!found) return 0;
        }
        return a;
    }
    int kind2 = m >> 24 & 63, n = m >> 16 & 255, topp = m >> 8 & 255, r0 = m & 255, len = kind2 ? n / 2 : n;
    sb_ranks k;
    sb_by_rank(R, p->hand[p->turn], p->n[p->turn], &k);
    for (int i = 0; i < len; i++) {
        int r = r0 + i, top = i == len - 1, used = 0;
        if (r > 12) return 0;
        if (top) {
            for (int j = 0; j < k.n[r]; j++)
                if (R->power(k.c[r][j]) == topp) a |= shed_bit(k.c[r][j]), used = 1;
            if (!used) return 0;
        }
        for (int j = 0; j < k.n[r] && used < (kind2 ? 2 : 1); j++)
            if (!(a & shed_bit(k.c[r][j]))) a |= shed_bit(k.c[r][j]), used++;
        if (used < (kind2 ? 2 : 1)) return 0;
    }
    return a;
}

/* cards of the hand (sorted low to high) make a play that may go now: the card the first play must hold, the
 * combination, what it must beat (shed_legal without the mask's round trip) */
static inline int sb_ok(const shed_rules *R, const shed_pos *p, const uint8_t *cards, int n)
{
    if (p->must != SHED_NONE) {
        int has = 0;
        for (int i = 0; i < n; i++) has |= cards[i] == p->must;
        if (!has) return 0;
    }
    shed_combo c;
    memset(&c, 0, sizeof c);
    if (!R->classify(cards, n, &c)) return 0;
    return !p->top.n || R->beats(&c, &p->top);
}

static inline int sb_add(const shed_rules *R, const shed_pos *p, int *out, int k, const uint8_t *cards, int n)
{
    if (k >= AI_MAX_MOVES - 1 || n > 5) return k;
    uint8_t s[5];
    memcpy(s, cards, (size_t)n);
    shed_sort(R, s, n);
    if (!sb_ok(R, p, s, n)) return k;
    int m = sb_code(R, s, n);
    for (int i = n == 5 ? 0 : k; i < k; i++)   /* (only the five-card hands may come twice) */
        if (out[i] == m) return k;
    out[k] = m;
    return k + 1;
}

static inline int sb_add_long(const shed_rules *R, const shed_pos *p, int *out, int k, int dseq, int n, int top, int r0)
{
    if (k >= AI_MAX_MOVES - 1) return k;
    int m = SB_LONG | dseq << 24 | n << 16 | R->power(top) << 8 | r0;
    uint64_t a = sb_real(R, p, m);
    if (!a || !shed_legal(R, p, a)) return k;
    out[k] = m;
    return k + 1;
}

/* the plays of the seat to move (see the head of the file) */
static inline int sb_moves_of(const shed_rules *R, const shed_pos *p, int five, int seqs, int *out)
{
    if (p->phase != SP_PLAY) return 0;
    int k = 0, pl = p->turn, follow = p->top.n, want = p->top.n;
    sb_ranks g;
    sb_by_rank(R, p->hand[pl], p->n[pl], &g);
    if (follow) out[k++] = 0;
    uint8_t c[13];
    if (!follow || want == 1)
        for (int r = 0; r < 13; r++)
            for (int j = 0; j < g.n[r]; j++) c[0] = g.c[r][j], k = sb_add(R, p, out, k, c, 1);
    for (int r = 0; r < 13; r++) {
        int n = g.n[r];
        if ((!follow || want == 2) && n >= 2)
            for (int j = 1; j < n; j++) c[0] = g.c[r][0], c[1] = g.c[r][j], k = sb_add(R, p, out, k, c, 2);
        if ((!follow || want == 3) && n >= 3)
            for (int j = 2; j < n; j++)
                c[0] = g.c[r][0], c[1] = g.c[r][1], c[2] = g.c[r][j], k = sb_add(R, p, out, k, c, 3);
        if (n == 4 && (!follow || want == 4 || seqs))   /* four alike (a bomb in Tiến lên) */
            k = sb_add(R, p, out, k, g.c[r], 4);
    }
    if (five && (!follow || want == 5)) {
        for (int r = 0; r + 4 < 12; r++) {      /* straights: no 2 */
            int ok = 1;
            for (int i = 0; i < 5; i++) ok &= g.n[r + i] > 0;
            if (!ok) continue;
            for (int t = 0; t < g.n[r + 4]; t++) {
                for (int i = 0; i < 4; i++) c[i] = g.c[r + i][0];
                c[4] = g.c[r + 4][t];
                k = sb_add(R, p, out, k, c, 5);
            }
            for (int s = 0; s < 4; s++) {       /* straight flushes */
                int fl = 1;
                for (int i = 0; i < 5 && fl; i++) {
                    fl = 0;
                    for (int j = 0; j < g.n[r + i]; j++)
                        if (CARD_SUIT(g.c[r + i][j]) == s) c[i] = g.c[r + i][j], fl = 1;
                }
                if (fl) k = sb_add(R, p, out, k, c, 5);
            }
        }
        for (int s = 0; s < 4; s++) {           /* flushes: the lowest five, then the fifth raised */
            uint8_t f[18];
            int nf = 0;
            for (int i = 0; i < p->n[pl]; i++)
                if (p->hand[pl][i] < CDB_HIDDEN && CARD_SUIT(p->hand[pl][i]) == s) f[nf++] = p->hand[pl][i];
            if (nf < 5) continue;
            for (int t = 4; t < nf; t++) {
                memcpy(c, f, 4);
                c[4] = f[t];
                k = sb_add(R, p, out, k, c, 5);
            }
        }
        for (int t = 0; t < 13; t++) {          /* full houses, four of a kind and a kicker */
            if (g.n[t] >= 3)
                for (int q = 0; q < 13; q++) {
                    if (q == t || g.n[q] < 2) continue;
                    c[0] = g.c[t][0], c[1] = g.c[t][1], c[2] = g.c[t][2], c[3] = g.c[q][0], c[4] = g.c[q][1];
                    k = sb_add(R, p, out, k, c, 5);
                }
            if (g.n[t] == 4)
                for (int q = 0; q < 13; q++) {
                    if (q == t || !g.n[q]) continue;
                    memcpy(c, g.c[t], 4);
                    c[4] = g.c[q][0];
                    k = sb_add(R, p, out, k, c, 5);
                    break;
                }
        }
    }
    if (seqs) {
        for (int r = 0; r < 12; r++)            /* sequences of 3 and more (no 2) */
            for (int len = 3; r + len <= 12; len++) {
                if (!g.n[r + len - 1]) break;
                int ok = 1;
                for (int i = 0; i < len; i++) ok &= g.n[r + i] > 0;
                if (!ok) break;
                if (follow && (p->top.n != len)) continue;
                int tops = follow ? g.n[r + len - 1] : 1;
                for (int t = 0; t < tops; t++) {
                    uint8_t top = g.c[r + len - 1][follow ? t : 0];
                    if (len <= 5) {
                        for (int i = 0; i < len - 1; i++) c[i] = g.c[r + i][0];
                        c[len - 1] = top;
                        k = sb_add(R, p, out, k, c, len);
                    } else k = sb_add_long(R, p, out, k, 0, len, top, r);
                }
            }
        for (int r = 0; r < 12; r++)            /* double sequences of 3 pairs and more */
            for (int len = 3; r + len <= 12; len++) {
                int ok = 1;
                for (int i = 0; i < len; i++) ok &= g.n[r + i] >= 2;
                if (!ok) break;
                int n = 2 * len;
                int top_rank = r + len - 1, tops = follow ? g.n[top_rank] - 1 : 1;
                for (int t = 0; t < tops; t++)
                    k = sb_add_long(R, p, out, k, 1, n, g.c[top_rank][follow ? t + 1 : 1], r);
            }
    }
    return k;
}

/* ---- the playouts' heuristic ------------------------------------------------------------------------------------ */
static inline int sb_count(int m)
{
    if (!m) return 0;
    if (m & SB_LONG) return m >> 16 & 255;
    int n = 0;
    while (n < 5 && (m >> (6 * n) & 63)) n++;
    return n;
}

static inline int sb_top_power(int m)          /* the highest card's power + 1 */
{
    if (m & SB_LONG) return (m >> 8 & 255) + 1;
    return m >> (6 * (sb_count(m) - 1)) & 63;
}

static inline int sb_policy(const sb_st *st, const int *mv, int n, ai_rng *r, int eps)
{
    const shed_pos *p = &st->p;
    if (n == 1) return mv[0];
    if (eps && ai_rand_n(r, 100) < eps) return mv[ai_rand_n(r, n)];
    int pl = p->turn, danger = 0, cards = p->n[pl];
    for (int q = 0; q < p->players; q++)
        if (q != pl && shed_active(p, q) && p->n[q] <= 2) danger = 1;
    int best = mv[0];
    long long bs = -(1LL << 40);
    for (int i = 0; i < n; i++) {
        int m = mv[i], cnt = sb_count(m), top = sb_top_power(m);
        long long sc;
        if (!m) sc = danger ? -5000 : cards > 6 ? -300 : -900;   /* a pass */
        else if (!p->top.n) sc = cnt * 120 - top * 14 + (cnt == cards ? 100000 : 0);
        else {
            sc = 1000 - top * 16;
            if (top > 44 && cards > 5 && !danger) sc -= 900;   /* the 2s (and the aces) kept */
            if (cnt == cards) sc += 100000;
            if (danger) sc += top * 20;
        }
        sc += ai_rand_n(r, 60);
        if (sc > bs) bs = sc, best = m;
    }
    return best;
}

/* ---- the adapter --------------------------------------------------------------------------------------------- */
static inline int sb_five(const shed_rules *R) { return R->deal_all; }   /* Big Two's five-card hands */
static inline int sb_seqs(const shed_rules *R) { return R->lockout; }    /* Tiến lên's sequences and bombs */

static inline int sb_a_moves(const void *s, int *out)
{
    const sb_st *st = s;
    return sb_moves_of(st->R, &st->p, sb_five(st->R), sb_seqs(st->R), out);
}

static inline void sb_a_make(void *s, int m)
{
    sb_st *st = s;
    uint64_t a = sb_real(st->R, &st->p, m);
    if (a && shed_legal(st->R, &st->p, a)) shed_apply(st->R, &st->p, a);
}

static inline int sb_a_side(const void *s) { return ((const sb_st *)s)->p.turn; }

static inline int sb_a_result(const void *s, float *rw)
{
    const sb_st *st = s;
    const shed_pos *p = &st->p;
    if (p->phase != SP_OVER) return 0;
    for (int q = 0; q < p->players; q++) {
        int place = shed_place(p, q);
        if (st->places) rw[q] = place == 0 ? 1 : 0.35f * (float)(p->players - 1 - place) / (float)(p->players - 1);
        else rw[q] = place == 0 ? 1 : 0.25f * (1.0f - (float)(p->n[q] > 13 ? 13 : p->n[q]) / 13.0f);
    }
    return 1;
}

/* the playouts: quick plays of one rank (the whole list only to beat a longer play, or to go out) */
static inline int sb_a_playout(const void *s, ai_rng *r)
{
    const sb_st *st = s;
    const shed_pos *p = &st->p;
    const shed_rules *R = st->R;
    if (p->phase != SP_PLAY) return AI_NONE;
    int pl = p->turn, n = p->n[pl], danger = 0;
    for (int q = 0; q < p->players; q++)
        if (q != pl && shed_active(p, q) && p->n[q] <= 2) danger = 1;
    sb_ranks g;
    sb_by_rank(R, p->hand[pl], n, &g);
    uint8_t c[5];
    if (n <= 5 && sb_ok(R, p, p->hand[pl], n)) return sb_code(R, p->hand[pl], n);   /* out at once */
    if (!p->top.n) {                           /* lead: the lowest rank, alone or with its fellows */
        int r0 = shed_rank_index(p->must != SHED_NONE ? p->must : p->hand[pl][0]), k = g.n[r0];
        if (k > 3) k = 3;
        int take = k > 1 && ai_rand_n(r, 100) < 70 ? k : 1;
        for (int i = 0; i < take; i++) c[i] = g.c[r0][i];
        if (p->must != SHED_NONE && take == 1) c[0] = p->must;
        if (sb_ok(R, p, c, take)) return sb_code(R, c, take);
        int mv[AI_MAX_MOVES], m = sb_a_moves(s, mv);
        return m ? sb_policy(st, mv, m, r, 10) : AI_NONE;
    }
    int want = p->top.n;
    if (want <= 4) {                           /* the cheapest of the rank kind that beats */
        for (int rk = 0; rk < 13; rk++) {
            if (g.n[rk] < want) continue;
            for (int t = want - 1; t < g.n[rk]; t++) {
                for (int i = 0; i < want - 1; i++) c[i] = g.c[rk][i];
                c[want - 1] = g.c[rk][t];
                if (!sb_ok(R, p, c, want)) continue;
                if (rk >= 11 && n > 5 && !danger && ai_rand_n(r, 100) < 60) return 0;   /* the aces and 2s kept */
                return sb_code(R, c, want);
            }
        }
        if (!R->lockout || want < 3) return 0;
    }
    if (ai_rand_n(r, 100) < 50 && !danger) return 0;
    int mv[AI_MAX_MOVES], m = sb_a_moves(s, mv);
    return m ? sb_policy(st, mv, m, r, 6) : 0;
}

static inline void sb_a_determinise(void *s, int seat, ai_rng *r)
{
    (void)seat;
    sb_st *st = s;
    shed_pos *p = &st->p;
    uint8_t pool[52];
    int np = 0;
    uint64_t seen = st->gone;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++)
            if (p->hand[q][i] < CDB_HIDDEN) seen |= shed_bit(p->hand[q][i]);
    for (int s = 0; s < 4; s++)
        for (int rk = 1; rk <= 13; rk++)
            if (!(seen & shed_bit(CARD(s, rk)))) pool[np++] = (uint8_t)CARD(s, rk);
    cdb_shuffle(pool, np, r);
    for (int q = 0; q < p->players; q++) {
        int hidden = 0;
        for (int i = 0; i < p->n[q]; i++) hidden += p->hand[q][i] == CDB_HIDDEN;
        if (!hidden) continue;
        cdb_fill(p->hand[q], p->n[q], pool, &np);
        shed_sort(st->R, p->hand[q], p->n[q]);
    }
}

/* seat me's view: its hand, the others' hidden, every card played this deal gone */
static inline void sb_view(const shed_rules *R, const shed_game *g, int me, int places, sb_st *v)
{
    memset(v, 0, sizeof *v);
    v->p = g->pos;
    v->R = R;
    v->me = (uint8_t)me;
    v->places = (uint8_t)places;
    for (int i = 0; i < g->n; i++)
        if (g->act[i] != SHED_PASS) v->gone |= g->act[i];
    for (int q = 0; q < v->p.players; q++)
        if (q != me) memset(v->p.hand[q], CDB_HIDDEN, v->p.n[q]);
    v->p.seed = 0;
}

/* the action for seat at a level (0: nothing to do) */
static inline uint64_t sb_choose(const shed_rules *R, const shed_game *g, int seat, int level, int places, long hard,
                                 uint64_t seed, ai_stats *stats)
{
    const shed_pos *live = &g->pos;
    if (stats) memset(stats, 0, sizeof *stats);
    if (live->phase != SP_PLAY || live->turn != seat) return 0;
    sb_st *v = malloc(sizeof *v);
    if (!v) return shed_auto(R, live);
    sb_view(R, g, seat, places, v);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 131 + live->n[seat]));
    int mv[AI_MAX_MOVES], n = sb_a_moves(v, mv), m = n ? mv[0] : -1;
    if (n > 1) {
        if (cdb_slip(level, &r)) m = sb_policy(v, mv, n, &r, CDB_SLIP_EPS);
        else {
            ai_game G = {
                .size = sizeof(sb_st), .moves = sb_a_moves, .make = sb_a_make, .side = sb_a_side,
                .players = live->players, .result = sb_a_result, .playout = sb_a_playout,
                .determinise = sb_a_determinise,
            };
            int x = cdb_search(&G, v, level, hard, &r, stats);
            if (x != AI_NONE) m = x;
        }
    }
    uint64_t a = m < 0 ? 0 : sb_real(R, live, m);
    free(v);
    return a && shed_legal(R, live, a) ? a : shed_auto(R, live);
}

/* the action in the notation: its cards ("3d4c5h"), p a pass */
static inline int sb_write(const shed_rules *R, uint64_t a, char *move, int cap)
{
    if (a == SHED_PASS) { snprintf(move, (size_t)cap, "p"); return 1; }
    uint8_t c[52];
    int n = shed_cards(R, a, c), k = 0;
    if (2 * n + 1 > cap) return 0;
    for (int i = 0; i < n; i++) trick_name(c[i], move + k), k += 2;
    move[k] = 0;
    return 1;
}

#endif
