/*
 * Canasta: the rules (canasta.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/canasta.h"

/* ---- the cards --------------------------------------------------------------------------------------------------- */
static int joker(int c) { return CARD_SUIT(c) == SUIT_JOKER; }
int ca_wild(int c) { return joker(c) || CARD_RANK(c) == 2; }
int ca_red3(int c) { return !joker(c) && CARD_RANK(c) == 3 && (CARD_SUIT(c) == SUIT_HEARTS || CARD_SUIT(c) == SUIT_DIAMONDS); }
int ca_black3(int c) { return !joker(c) && CARD_RANK(c) == 3 && !ca_red3(c); }

int ca_value(int c)
{
    if (joker(c)) return 50;
    int r = CARD_RANK(c);
    return r == 2 || r == RANK_ACE ? 20 : r >= 8 ? 10 : r >= 4 ? 5 : r == 3 && ca_black3(c) ? 5 : 0;
}

int ca_side(const capos *p, int pl) { return p->players == 2 ? pl : pl & 1; }

/* a card out of a hand (jokers are all alike): the card taken (CARD_NONE: not there) */
static int remove_card(uint8_t *h, uint8_t *n, int c)
{
    for (int i = 0; i < *n; i++)
        if (h[i] == c || (joker(h[i]) && joker(c))) {
            int got = h[i];
            memmove(h + i, h + i + 1, (size_t)(*n - i - 1));
            (*n)--;
            return got;
        }
    return CARD_NONE;
}

/* ---- the melds --------------------------------------------------------------------------------------------------- */
int ca_meld_rank(const uint8_t *c, int n, int rank)
{
    int r = rank;
    for (int i = 0; i < n; i++) {
        if (ca_wild(c[i])) continue;
        if (ca_red3(c[i])) return -1;
        int cr = CARD_RANK(c[i]);
        if (!r) r = cr;
        else if (r != cr) return -1;
    }
    return r == 1 || (r >= 3 && r <= 13) ? r : -1;
}

/* n cards of rank r together: a meld */
static int valid(const uint8_t *c, int n, int r)
{
    int nat = 0, wild = 0;
    for (int i = 0; i < n; i++) {
        if (ca_wild(c[i])) wild++;
        else if (CARD_RANK(c[i]) == r && !ca_red3(c[i])) nat++;
        else return 0;
    }
    if (r == 3) return !wild && nat >= 3;
    return nat >= 2 && wild <= 3 && n >= 3 && n <= CA_MELD;
}

int ca_meld_points(const capos *p, int s)
{
    int v = 0;
    for (int r = 0; r < CA_RANKS; r++)
        for (int i = 0; i < p->mn[s][r]; i++) v += ca_value(p->meld[s][r][i]);
    return v;
}

static int melds_any(const capos *p, int s)
{
    for (int r = 0; r < CA_RANKS; r++)
        if (p->mn[s][r]) return 1;
    return 0;
}

static int wilds_in(const capos *p, int s, int r)
{
    int w = 0;
    for (int i = 0; i < p->mn[s][r]; i++) w += ca_wild(p->meld[s][r][i]);
    return w;
}

int ca_canastas(const capos *p, int s)
{
    int k = 0;
    for (int r = 0; r < CA_RANKS; r++) k += p->mn[s][r] >= 7;
    return k;
}

int ca_can_out(const capos *p, int s) { return ca_canastas(p, s) >= (p->players == 2 ? 2 : 1); }

int ca_need(const capos *p, int s)
{
    if (p->opened[s]) return 0;
    int v = p->score[s];
    return v < 0 ? 15 : v < 1500 ? 50 : v < 3000 ? 90 : 120;
}

/* the initial meld: made, nothing laid yet, or laid and worth enough */
static int min_ok(const capos *p, int s)
{
    int v = ca_meld_points(p, s);
    return p->opened[s] || !v || v >= ca_need(p, s);
}

/* the player to move would go out concealed now */
static int concealed(const capos *p) { return p->now && !p->melded[p->turn] && !p->took && !p->touched; }

int ca_frozen_for(const capos *p, int s) { return p->frozen || !p->opened[s]; }

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static void lay_red3(capos *p, int pl, int c) { p->red3[ca_side(p, pl)][p->nred3[ca_side(p, pl)]++ & 3] = (uint8_t)c; }

static void begin_turn(capos *p)
{
    p->phase = CP_DRAW;
    p->took = p->touched = p->now = p->ndrew = 0;
    memcpy(p->mn0, p->mn, sizeof p->mn);
}

static void deal(capos *p)
{
    int n = cards_pack(PACK_54, p->stock);
    n += cards_pack(PACK_54, p->stock + n);
    cards_shuffle(p->stock, n, mix(p->seed, 1000u + p->hands));
    p->nstock = (uint8_t)n;
    memset(p->n, 0, sizeof p->n);
    memset(p->mn, 0, sizeof p->mn);
    memset(p->nred3, 0, sizeof p->nred3);
    memset(p->opened, 0, sizeof p->opened);
    memset(p->melded, 0, sizeof p->melded);
    p->npile = 0;
    p->frozen = 0;
    p->freezer = CA_NONE;
    p->out = CA_NONE;
    p->concealed = 0;
    int k = p->players == 2 ? 15 : 11, first = (p->dealer + 1) % p->players;
    for (int i = 0; i < k; i++)
        for (int j = 0; j < p->players; j++) {
            int pl = (first + j) % p->players;
            p->hand[pl][p->n[pl]++] = p->stock[--p->nstock];
        }
    for (;;) {                                  /* the upcard: a wild card or a red 3 is covered, the pile frozen */
        int c = p->stock[--p->nstock];
        p->pile[p->npile++] = (uint8_t)c;
        if (!ca_wild(c) && !ca_red3(c)) break;
        p->frozen = 1;
        if (p->freezer == CA_NONE) p->freezer = (uint8_t)c;
    }
    for (int j = 0; j < p->players; j++) {      /* the red 3s dealt: laid down, replaced */
        int pl = (first + j) % p->players;
        for (int i = 0; i < p->n[pl];) {
            if (!ca_red3(p->hand[pl][i])) { i++; continue; }
            lay_red3(p, pl, p->hand[pl][i]);
            p->hand[pl][i] = p->stock[--p->nstock];
        }
    }
    p->turn = (uint8_t)first;
    begin_turn(p);
}

void ca_new(cagame *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    capos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players == 2 ? 2 : 4);
    p->dealer = (uint8_t)(p->players - 1);
    deal(p);
}

/* ---- the end of a hand ------------------------------------------------------------------------------------------- */
static void hand_end(capos *p, int out)
{
    p->out = (uint8_t)out;
    p->concealed = (uint8_t)(out != CA_NONE && concealed(p));
    int top = -(1 << 20);
    for (int s = 0; s < 2; s++) {
        ca_score *r = &p->res[s];
        memset(r, 0, sizeof *r);
        r->melds = (int16_t)ca_meld_points(p, s);
        for (int k = 0; k < CA_RANKS; k++) {
            if (p->mn[s][k] < 7) continue;
            if (wilds_in(p, s, k)) r->mixed++;
            else r->natural++;
        }
        r->bonus = (int16_t)(500 * r->natural + 300 * r->mixed);
        r->red3 = (int16_t)(p->nred3[s] == 4 ? 800 : 100 * p->nred3[s]);
        if (!melds_any(p, s)) r->red3 = (int16_t)-r->red3;
        if (out != CA_NONE && ca_side(p, out) == s) r->out = p->concealed ? 200 : 100;
        for (int pl = 0; pl < p->players; pl++)
            if (ca_side(p, pl) == s)
                for (int i = 0; i < p->n[pl]; i++) r->minus = (int16_t)(r->minus + ca_value(p->hand[pl][i]));
        r->total = (int16_t)(r->melds + r->bonus + r->red3 + r->out - r->minus);
        p->score[s] = (int16_t)(p->score[s] + r->total);
        if (p->score[s] > top) top = p->score[s];
    }
    p->phase = top >= CA_GOAL ? CP_OVER : CP_HAND;
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
/* one card from the stock into the hand (red 3s laid down and replaced): 1 drawn, 0 the stock empty, -1 the hand
 * ended (the last card a red 3) */
static int draw_one(capos *p, int pl)
{
    for (;;) {
        if (!p->nstock) return 0;
        int c = p->stock[--p->nstock];
        if (ca_red3(c)) {
            lay_red3(p, pl, c);
            if (!p->nstock) { hand_end(p, CA_NONE); return -1; }
            continue;
        }
        p->hand[pl][p->n[pl]++] = (uint8_t)c;
        if (p->ndrew < 2) p->drew[p->ndrew++] = (uint8_t)c;
        return 1;
    }
}

static void end_turn(capos *p)
{
    int pl = p->turn, s = ca_side(p, pl);
    if (!p->n[pl]) { hand_end(p, pl); return; }   /* out (concealed or not: before this turn counts as melded) */
    if (melds_any(p, s)) p->opened[s] = 1;
    if (p->now) p->melded[pl] = 1;
    p->turn = (uint8_t)((pl + 1) % p->players);
    begin_turn(p);
}

/* puts cards (already out of the hand) on the side's meld of rank r */
static void put(capos *p, int s, int r, const uint8_t *c, int n)
{
    if (p->mn0[s][r]) p->touched = 1;
    for (int i = 0; i < n && p->mn[s][r] < CA_MELD; i++) p->meld[s][r][p->mn[s][r]++] = c[i];
    p->now = 1;
}

/* the action, without checks (the cards taken from the hand must be there); 0: cards missing */
static int apply(capos *p, const ca_act *a)
{
    int pl = p->turn, s = ca_side(p, pl);
    uint8_t got[CA_ACT];
    int n = a->n < CA_ACT ? a->n : CA_ACT;
    switch (a->kind) {
    case CA_STOCK: {
        if (!p->nstock) { hand_end(p, CA_NONE); return 1; }
        int k = p->players == 2 ? 2 : 1;
        for (int i = 0; i < k; i++)
            if (draw_one(p, pl) <= 0) break;
        if (p->phase == CP_DRAW) p->phase = CP_MELD;
        return 1;
    }
    case CA_TAKE: {
        int top = p->pile[p->npile - 1], r = CARD_RANK(top);
        for (int i = 0; i < n; i++)
            if ((got[i] = (uint8_t)remove_card(p->hand[pl], &p->n[pl], a->card[i])) == CARD_NONE) return 0;
        uint8_t first = (uint8_t)top;
        put(p, s, r, &first, 1);
        put(p, s, r, got, n);
        for (int i = 0; i < p->npile - 1; i++) {
            int c = p->pile[i];
            if (ca_red3(c)) lay_red3(p, pl, c);
            else p->hand[pl][p->n[pl]++] = (uint8_t)c;
        }
        p->npile = 0;
        p->frozen = 0;
        p->freezer = CA_NONE;
        p->took = 1;
        p->phase = CP_MELD;
        if (!p->n[pl]) hand_end(p, pl);
        return 1;
    }
    case CA_MELD_IT: {
        int r = ca_meld_rank(a->card, n, a->rank);
        if (r < 0) return 0;
        for (int i = 0; i < n; i++)
            if ((got[i] = (uint8_t)remove_card(p->hand[pl], &p->n[pl], a->card[i])) == CARD_NONE) return 0;
        put(p, s, r, got, n);
        if (!p->n[pl]) hand_end(p, pl);
        return 1;
    }
    case CA_DISCARD: {
        int c = remove_card(p->hand[pl], &p->n[pl], a->card[0]);
        if (c == CARD_NONE) return 0;
        p->pile[p->npile++] = (uint8_t)c;
        if (ca_wild(c)) {
            if (!p->frozen) p->freezer = (uint8_t)c;
            p->frozen = 1;
        }
        end_turn(p);
        return 1;
    }
    case CA_NEXT:
        p->hands++;
        p->dealer = (uint8_t)((p->dealer + 1) % p->players);
        deal(p);
        return 1;
    }
    return 0;
}

/* after a meld or a take (p before it, q after): the hand left may be this small only when going out, and so may the
 * initial meld's count be short only going out concealed (the count asked: p's, before the hand is scored) */
static int after_ok(const capos *p, const capos *q, int pl)
{
    int s = ca_side(p, pl), v = ca_meld_points(q, s), ended = q->phase == CP_HAND || q->phase == CP_OVER;
    if (!ended && q->n[pl] >= 2) return 1;
    int mok = p->opened[s] || !v || v >= ca_need(p, s);
    return ca_can_out(q, s) && (mok || (ended ? q->concealed : concealed(q)));
}

int ca_legal(const capos *p, const ca_act *a)
{
    int pl = p->turn, s = ca_side(p, pl);
    if (a->n > CA_ACT) return 0;
    if (a->kind == CA_NEXT) return p->phase == CP_HAND;
    if (p->phase != CP_DRAW && p->phase != CP_MELD) return 0;
    capos q = *p;
    switch (a->kind) {
    case CA_STOCK:
        if (p->phase != CP_DRAW) return 0;
        return p->nstock || (!ca_forced(p) && min_ok(p, s));
    case CA_TAKE: {
        if (p->phase != CP_DRAW || !p->npile) return 0;
        int top = p->pile[p->npile - 1], r = CARD_RANK(top), nat = 0;
        if (ca_wild(top) || ca_black3(top) || ca_red3(top)) return 0;
        for (int i = 0; i < a->n; i++) {
            if (ca_wild(a->card[i])) continue;
            if (ca_red3(a->card[i]) || CARD_RANK(a->card[i]) != r) return 0;
            nat++;
        }
        if (ca_frozen_for(p, s) && nat < 2) return 0;   /* frozen: a natural pair from the hand */
        if (!a->n && !p->mn[s][r]) return 0;
        uint8_t all[CA_MELD + CA_ACT + 1];
        int k = 0;
        for (int i = 0; i < p->mn[s][r]; i++) all[k++] = p->meld[s][r][i];
        all[k++] = (uint8_t)top;
        for (int i = 0; i < a->n; i++) all[k++] = a->card[i];
        if (!valid(all, k, r)) return 0;
        if (!apply(&q, a)) return 0;
        if (!p->opened[s] && ca_meld_points(&q, s) < ca_need(p, s)) return 0;
        return after_ok(p, &q, pl);
    }
    case CA_MELD_IT: {
        if (p->phase == CP_DRAW && p->opened[s]) return 0;     /* before drawing: only the initial meld */
        if (!a->n) return 0;
        int r = ca_meld_rank(a->card, a->n, a->rank);
        if (r < 0) return 0;
        if (r == 3 && p->phase == CP_DRAW) return 0;
        uint8_t all[CA_MELD + CA_ACT];
        int k = 0;
        for (int i = 0; i < p->mn[s][r]; i++) all[k++] = p->meld[s][r][i];
        for (int i = 0; i < a->n; i++) all[k++] = a->card[i];
        if (!valid(all, k, r)) return 0;
        if (!apply(&q, a)) return 0;
        if (p->phase == CP_DRAW) return q.n[pl] >= 1 && q.phase == CP_DRAW;
        if (r == 3 && q.n[pl] > 1) return 0;   /* black 3s: only going out */
        return after_ok(p, &q, pl);
    }
    case CA_DISCARD:
        if (p->phase != CP_MELD || a->n != 1) return 0;
        if (!apply(&q, a)) return 0;
        if (q.phase == CP_HAND || q.phase == CP_OVER) return ca_can_out(p, s) && (min_ok(p, s) || q.concealed);
        return min_ok(p, s);
    }
    return 0;
}

int ca_forced(const capos *p)
{
    if (p->phase != CP_DRAW || p->nstock || !p->npile) return 0;
    ca_act a = {CA_TAKE, 0, 0, {0}};
    capos q = *p;
    q.nstock = 1;                               /* the take's own checks, not this one again */
    return ca_legal(&q, &a);
}

int ca_play(cagame *g, const ca_act *a)
{
    if (g->n >= CA_MAX_ACTS || !ca_legal(&g->pos, a)) return 0;
    if (!apply(&g->pos, a)) return 0;
    g->act[g->n++] = *a;
    return 1;
}

int ca_undo(cagame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static ca_act acts[CA_MAX_ACTS];
    memcpy(acts, g->act, sizeof(ca_act) * (size_t)n);
    ca_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) ca_play(g, &acts[i]);
    return 1;
}

int ca_winner(const capos *p)
{
    if (p->phase != CP_OVER) return -1;
    return p->score[0] > p->score[1] ? 0 : p->score[1] > p->score[0] ? 1 : -1;
}

unsigned ca_winners(const capos *p)
{
    if (p->phase != CP_OVER) return 0;
    int w = ca_winner(p);
    unsigned m = 0;
    for (int pl = 0; pl < p->players; pl++)
        if (w < 0 || ca_side(p, pl) == w) m |= 1u << pl;
    return m;
}

/* ---- plain play (the screenshots, the tests) --------------------------------------------------------------------- */
static int try(const capos *p, ca_act *a, int kind, int rank, const uint8_t *c, int n)
{
    a->kind = (uint8_t)kind;
    a->rank = (uint8_t)rank;
    a->n = (uint8_t)n;
    if (n) memcpy(a->card, c, (size_t)n);
    return ca_legal(p, a);
}

/* the naturals of each rank in the hand, and the wild cards */
typedef struct { uint8_t nat[CA_RANKS][CA_ACT]; int nn[CA_RANKS]; uint8_t wild[CA_ACT]; int nw; } sorted;

static void sort_hand(const capos *p, int pl, sorted *h)
{
    memset(h, 0, sizeof *h);
    for (int i = 0; i < p->n[pl]; i++) {
        int c = p->hand[pl][i];
        if (ca_wild(c)) { if (h->nw < CA_ACT) h->wild[h->nw++] = (uint8_t)c; }
        else if (h->nn[CARD_RANK(c)] < CA_ACT) h->nat[CARD_RANK(c)][h->nn[CARD_RANK(c)]++] = (uint8_t)c;
    }
}

static const int ORDER[11] = {13, 12, 11, 10, 9, 8, 1, 7, 6, 5, 4};   /* the ranks tried, the dearest first */

/* a meld of the hand worth melding now: on the side's melds, new melds of three naturals or more; the initial meld
 * only when the whole plan reaches its count */
static int auto_meld(const capos *p, ca_act *a)
{
    int pl = p->turn, s = ca_side(p, pl);
    sorted h;
    sort_hand(p, pl, &h);
    if (!p->opened[s]) {
        int v = ca_meld_points(p, s), left = p->n[pl], w = 0, need = ca_need(p, s);
        for (int k = 0; k < 11; k++) {          /* the plan, in the order the melds are then laid */
            int r = ORDER[k];
            if (h.nn[r] >= 3 && left - h.nn[r] >= 2) {
                v += h.nn[r] * ca_value(h.nat[r][0]);
                left -= h.nn[r];
            } else if (h.nn[r] == 2 && w < h.nw && left - 3 >= 2 && !p->mn[s][r]) {
                v += 2 * ca_value(h.nat[r][0]) + ca_value(h.wild[w++]);
                left -= 3;
            }
        }
        if (v < need) return 0;
        for (int k = 0; k < 11; k++) {
            int r = ORDER[k];
            if (h.nn[r] >= 3 && try(p, a, CA_MELD_IT, 0, h.nat[r], h.nn[r])) return 1;
            if (h.nn[r] == 2 && h.nw && !p->mn[s][r]) {
                uint8_t c[3] = {h.nat[r][0], h.nat[r][1], h.wild[0]};
                if (try(p, a, CA_MELD_IT, 0, c, 3)) return 1;
            }
        }
        return 0;
    }
    for (int k = 0; k < 11; k++) {              /* on the side's melds, then new ones */
        int r = ORDER[k];
        if (h.nn[r] && p->mn[s][r] && try(p, a, CA_MELD_IT, r, h.nat[r], h.nn[r])) return 1;
    }
    for (int k = 0; k < 11; k++) {
        int r = ORDER[k];
        if (h.nn[r] >= 3 && try(p, a, CA_MELD_IT, 0, h.nat[r], h.nn[r])) return 1;
    }
    for (int k = 0; k < 11 && h.nw; k++) {      /* a wild card towards a canasta */
        int r = ORDER[k];
        if (p->mn[s][r] >= 4 && p->mn[s][r] < 7 && try(p, a, CA_MELD_IT, r, h.wild, 1)) return 1;
    }
    if (ca_can_out(p, s) && h.nn[3] >= 3 && try(p, a, CA_MELD_IT, 3, h.nat[3], h.nn[3])) return 1;
    return 0;
}

/* the card to throw: a black 3, else the loneliest natural (the cheapest; not one the other side melds), else any */
static int auto_discard(const capos *p, ca_act *a)
{
    int pl = p->turn, o = 1 - ca_side(p, pl), best = -1, bestk = 1 << 30;
    sorted h;
    sort_hand(p, pl, &h);
    for (int i = 0; i < p->n[pl]; i++) {
        int c = p->hand[pl][i], k;
        if (ca_black3(c)) k = 0;
        else if (ca_wild(c)) k = 100000 + ca_value(c);
        else k = 1000 * h.nn[CARD_RANK(c)] + ca_value(c) + (p->mn[o][CARD_RANK(c)] ? 5000 : 0);
        uint8_t one = (uint8_t)c;
        if (k < bestk && try(p, a, CA_DISCARD, 0, &one, 1)) bestk = k, best = c;
    }
    if (best < 0) return 0;
    uint8_t one = (uint8_t)best;
    return try(p, a, CA_DISCARD, 0, &one, 1);
}

int ca_auto(const capos *p, ca_act *a)
{
    int pl = p->turn, s = ca_side(p, pl);
    memset(a, 0, sizeof *a);
    switch (p->phase) {
    case CP_HAND: return try(p, a, CA_NEXT, 0, NULL, 0);
    case CP_DRAW:
        if (p->npile) {                         /* the pile: on a meld, or with a natural pair (and more) */
            int top = p->pile[p->npile - 1], r = CARD_RANK(top);
            sorted h;
            sort_hand(p, pl, &h);
            if (!ca_wild(top) && !ca_red3(top)) {
                if (try(p, a, CA_TAKE, 0, NULL, 0)) return 1;
                if (h.nn[r] >= 2 && try(p, a, CA_TAKE, 0, h.nat[r], h.nn[r])) return 1;
                if (h.nn[r] >= 2 && try(p, a, CA_TAKE, 0, h.nat[r], 2)) return 1;
                if (h.nn[r] == 1 && h.nw) {
                    uint8_t c[2] = {h.nat[r][0], h.wild[0]};
                    if (try(p, a, CA_TAKE, 0, c, 2)) return 1;
                }
            }
        }
        (void)s;
        return try(p, a, CA_STOCK, 0, NULL, 0);
    case CP_MELD:
        if (auto_meld(p, a)) return 1;
        return auto_discard(p, a);
    }
    return 0;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static int rank_of(char c)
{
    static const char R[] = "A23456789TJQK";
    const char *f = c ? strchr(R, c) : NULL;
    return f ? (int)(f - R) + 1 : 0;
}

int ca_card_name(const char *s, int *card)
{
    if (s[0] == 'X') { *card = CARD(SUIT_JOKER, 0); return 1; }
    int r = rank_of(s[0]);
    const char *f = s[1] ? strchr("shdc", s[1]) : NULL;
    if (!r || !f) return 0;
    *card = CARD((int)(f - "shdc"), r);
    return 2;
}

/* cards joined by dots: "Kh.Kd.X"; the letters read (-1: bad) */
static int card_list(const char *s, ca_act *a)
{
    int k = 0, c;
    a->n = 0;
    while (s[k] && s[k] != ' ') {
        int m = ca_card_name(s + k, &c);
        if (!m || a->n >= CA_ACT) return -1;
        a->card[a->n++] = (uint8_t)c;
        k += m;
        if (s[k] == '.') k++;
    }
    return k;
}

int ca_script(cagame *g, const char *s)
{
    ca_act a;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        memset(&a, 0, sizeof a);
        if (c == 'a' || c == '#' || c == '*') {
            int guard = 0;
            do {
                const capos *p = &g->pos;
                if (c == '#' && (p->phase == CP_HAND || p->phase == CP_OVER)) break;
                if (c == '*' && p->phase == CP_OVER) break;
                if (!ca_auto(p, &a) || !ca_play(g, &a)) return 0;
            } while (c != 'a' && ++guard < 200000);
            continue;
        }
        switch (c) {
        case 's': a.kind = CA_STOCK; break;
        case 'n': a.kind = CA_NEXT; break;
        case 't':
        case 'm':
        case 'd': {
            a.kind = c == 't' ? CA_TAKE : c == 'm' ? CA_MELD_IT : CA_DISCARD;
            if (c == 'm' && s[0] && s[1] == ':') {
                a.rank = (uint8_t)rank_of(s[0]);
                if (!a.rank) return 0;
                s += 2;
            }
            int k = card_list(s, &a);
            if (k < 0) return 0;
            s += k;
            break;
        }
        default: return 0;
        }
        if (!ca_play(g, &a)) return 0;
    }
    return 1;
}
