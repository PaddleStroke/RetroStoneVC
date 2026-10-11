/*
 * Cribbage: the rules (cribbage.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/cribbage.h"

int crib_value(int card)
{
    int r = CARD_RANK(card);
    return r > 10 ? 10 : r;
}

int crib_away(const cpos *p) { return p->ndeal - 4; }

static void sort_cards(uint8_t *c, int n)       /* by rank, then suit */
{
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            int a = CARD_RANK(c[j - 1]) * 8 + CARD_SUIT(c[j - 1]), b = CARD_RANK(c[j]) * 8 + CARD_SUIT(c[j]);
            if (a <= b) break;
            uint8_t t = c[j];
            c[j] = c[j - 1];
            c[j - 1] = t;
        }
}

static int bits(unsigned m)
{
    int n = 0;
    for (; m; m &= m - 1) n++;
    return n;
}

/* ---- the show ---------------------------------------------------------------------------------------------------- */
static void item(crib_ev *items, int *n, int kind, int pts, unsigned mask)
{
    if (*n >= CRIB_MAX_EV) return;
    items[*n] = (crib_ev){0, (uint8_t)kind, (uint8_t)pts, (uint8_t)mask, CS_SHOW, 0, 0};
    (*n)++;
}

/* the ranks of the cards in mask make a run (all different, one after the other) */
static int is_run(const uint8_t *c, unsigned mask)
{
    int lo = 99, hi = 0, seen = 0;
    for (int i = 0; i < 5; i++) {
        if (!(mask >> i & 1)) continue;
        int r = CARD_RANK(c[i]);
        if (seen >> r & 1) return 0;
        seen |= 1 << r;
        lo = r < lo ? r : lo;
        hi = r > hi ? r : hi;
    }
    return hi - lo + 1 == bits(mask);
}

int crib_show(const uint8_t *four, int starter, int is_crib, crib_ev *items, int *n)
{
    uint8_t c[5] = {four[0], four[1], four[2], four[3], (uint8_t)starter};
    int total = 0;
    *n = 0;
    for (unsigned m = 1; m < 32; m++) {         /* fifteens: every combination */
        int s = 0;
        for (int i = 0; i < 5; i++) s += (m >> i & 1) ? crib_value(c[i]) : 0;
        if (s == 15) item(items, n, CE_FIFTEEN, 2, m), total += 2;
    }
    for (int i = 0; i < 5; i++)                 /* pairs: each one */
        for (int j = i + 1; j < 5; j++)
            if (CARD_RANK(c[i]) == CARD_RANK(c[j])) item(items, n, CE_PAIR, 2, 1u << i | 1u << j), total += 2;
    for (int len = 5; len >= 3; len--) {        /* runs: the longest, every one of them (double runs) */
        int found = 0;
        for (unsigned m = 1; m < 32; m++)
            if (bits(m) == len && is_run(c, m)) item(items, n, CE_RUN, len, m), total += len, found = 1;
        if (found) break;
    }
    int suit = CARD_SUIT(c[0]), four_suit = 1;
    for (int i = 1; i < 4; i++) four_suit &= CARD_SUIT(c[i]) == suit;
    if (four_suit && CARD_SUIT(c[4]) == suit) item(items, n, CE_FLUSH, 5, 31), total += 5;
    else if (four_suit && !is_crib) item(items, n, CE_FLUSH, 4, 15), total += 4;
    for (int i = 0; i < 4; i++)                 /* his nobs: the jack of the starter's suit */
        if (CARD_RANK(c[i]) == RANK_JACK && CARD_SUIT(c[i]) == CARD_SUIT(c[4]))
            item(items, n, CE_NOBS, 1, 1u << i | 16u), total += 1;
    return total;
}

int crib_show_of(const cpos *p, int k, const uint8_t **cards)
{
    if (k >= p->players) {
        *cards = p->crib;
        return p->dealer;
    }
    int pl = (p->dealer + 1 + k) % p->players;
    *cards = p->kept[pl];
    return pl;
}

/* ---- scoring ----------------------------------------------------------------------------------------------------- */
/* what was scored, for the pictures (it belongs to the next leap of a peg) */
static void ev_add(cpos *p, int pl, int kind, int pts, int mask, int src, int count)
{
    if (p->nev < CRIB_MAX_EV)
        p->ev[p->nev++] = (crib_ev){(uint8_t)pl, (uint8_t)kind, (uint8_t)pts, (uint8_t)mask, (uint8_t)src,
                                    (uint8_t)count, p->nleap};
}

/* the points pegged: the back peg leaps past the front one (121 at most); 0: the game is won, nothing more counts */
static int peg(cpos *p, int pl, int pts)
{
    if (p->phase == CP_OVER) return 0;
    if (pts <= 0) return 1;
    int s = p->score[pl] + pts;
    s = s > CRIB_GOAL ? CRIB_GOAL : s;
    if (p->nleap < CRIB_MAX_LEAPS)
        p->leap[p->nleap++] = (crib_leap){(uint8_t)pl, p->score[pl], (int16_t)s};
    p->back[pl] = p->score[pl];
    p->score[pl] = (int16_t)s;
    if (s < CRIB_GOAL) return 1;
    p->phase = CP_OVER;
    p->turn = (uint8_t)pl;
    return 0;
}

/* the play: what the card just laid scores (the count, the pairs, the runs among the last cards), pegged at once */
static void play_points(cpos *p, int pl)
{
    int n = p->nseq, sum = 0;
    if (p->count == 15) ev_add(p, pl, CE_FIFTEEN, 2, n, CS_PLAY, 0), sum += 2;
    if (p->count == 31) ev_add(p, pl, CE_THIRTYONE, 2, n, CS_PLAY, 0), sum += 2;
    int k = 1;
    while (k < n && CARD_RANK(p->seq[n - 1 - k]) == CARD_RANK(p->seq[n - 1])) k++;
    if (k >= 2) ev_add(p, pl, CE_PAIR, k * (k - 1), k, CS_PLAY, 0), sum += k * (k - 1);
    for (int len = n; len >= 3; len--) {        /* the longest run among the last cards */
        int seen = 0, lo = 99, hi = 0, ok = 1;
        for (int i = n - len; i < n && ok; i++) {
            int r = CARD_RANK(p->seq[i]);
            ok = !(seen >> r & 1);
            seen |= 1 << r;
            lo = r < lo ? r : lo;
            hi = r > hi ? r : hi;
        }
        if (ok && hi - lo + 1 == len) {
            ev_add(p, pl, CE_RUN, len, len, CS_PLAY, 0), sum += len;
            break;
        }
    }
    peg(p, pl, sum);
}

static int left_in_hand(const cpos *p, int pl) { return 4 - bits(p->used[pl]); }

int crib_fits(const cpos *p, int pl, int slot)
{
    return slot >= 0 && slot < 4 && !(p->used[pl] >> slot & 1) && p->count + crib_value(p->kept[pl][slot]) <= 31;
}

int crib_can_play(const cpos *p, int pl)
{
    for (int s = 0; s < 4; s++)
        if (crib_fits(p, pl, s)) return 1;
    return 0;
}

static void score_show(cpos *p);

/* after a card: who lays the next one; a go, a new count, the show */
static void advance(cpos *p)
{
    int n = p->players, last = p->last;
    if (p->count < 31)
        for (int i = 1; i <= n; i++) {
            int q = (last + i) % n;
            if (crib_can_play(p, q)) { p->turn = (uint8_t)q; return; }
        }
    int any = 0;                                /* nobody can lay a card: the count ends */
    for (int q = 0; q < n; q++) any |= left_in_hand(p, q) > 0;
    if (p->count < 31) {
        ev_add(p, last, any ? CE_GO : CE_LAST, 1, 0, CS_PLAY, 0);
        if (!peg(p, last, 1)) return;
    }
    p->old = (uint8_t)(p->old + p->nseq);
    p->nseq = 0;
    p->count = 0;
    for (int i = 1; i <= n; i++) {
        int q = (last + i) % n;
        if (left_in_hand(p, q)) { p->turn = (uint8_t)q; return; }
    }
    p->phase = CP_SHOW;                         /* every card laid: the show, the first hand counted at once */
    p->shown = 0;
    score_show(p);
}

/* the show's next count: its items, then the total pegged at once */
static void score_show(cpos *p)
{
    const uint8_t *cards;
    int k = p->shown, pl = crib_show_of(p, k, &cards), n;
    crib_ev items[CRIB_MAX_EV];
    int total = crib_show(cards, p->starter, k >= p->players, items, &n);
    p->turn = (uint8_t)pl;
    for (int i = 0; i < n; i++) ev_add(p, pl, items[i].kind, items[i].pts, items[i].mask, CS_SHOW, k);
    if (!peg(p, pl, total)) return;
    p->shown++;
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static void deal(cpos *p)
{
    int n = cards_pack(PACK_52, p->stock);
    cards_shuffle(p->stock, n, mix(p->seed, 1000u + p->hands));
    p->nstock = (uint8_t)n;
    p->ncrib = 0;
    for (int k = 0; k < p->ndeal; k++)
        for (int i = 1; i <= p->players; i++) {
            int q = (p->dealer + i) % p->players;
            p->hand[q][k] = p->stock[--p->nstock];
        }
    for (int q = 0; q < p->players; q++) {
        p->nhand[q] = p->ndeal;
        sort_cards(p->hand[q], p->ndeal);
        p->used[q] = 0;
        memset(p->kept[q], CARD_NONE, 4);
    }
    if (p->players == 3) p->crib[p->ncrib++] = p->stock[--p->nstock];
    p->starter = CARD_NONE;
    p->nseq = p->count = p->old = p->shown = 0;
    p->last = CRIB_NONE;
    p->turn = (uint8_t)((p->dealer + 1) % p->players);
    p->phase = CP_DISCARD;
    p->hands++;
}

void crib_new(cgame *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    cpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players == 3 ? 3 : 2);
    p->ndeal = p->players == 3 ? 5 : 6;
    p->dealer = (uint8_t)(p->players - 1);
    deal(p);
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
int crib_legal(const cpos *p, int act)
{
    switch (p->phase) {
    case CP_DISCARD: return act > 0 && act < 64 && bits((unsigned)act) == crib_away(p) && act >> p->nhand[p->turn] == 0;
    case CP_CUT: return act == CA_CUT;
    case CP_PLAY: return act >= CA_PLAY && act < CA_PLAY + 4 && crib_fits(p, p->turn, act - CA_PLAY);
    case CP_SHOW: return act == CA_NEXT;
    default: return 0;
    }
}

static void apply(cpos *p, int act)
{
    int pl = p->turn;
    p->nev = p->nleap = 0;
    switch (p->phase) {
    case CP_DISCARD: {
        int k = 0;
        for (int s = 0; s < p->nhand[pl]; s++) {
            if (act >> s & 1) p->crib[p->ncrib++] = p->hand[pl][s];
            else p->kept[pl][k++] = p->hand[pl][s];
        }
        p->nhand[pl] = 0;
        if (pl == p->dealer) {
            p->phase = CP_CUT;
            p->turn = (uint8_t)((p->dealer + 1) % p->players);
        } else p->turn = (uint8_t)((pl + 1) % p->players);
        break;
    }
    case CP_CUT:
        p->starter = p->stock[--p->nstock];
        p->phase = CP_PLAY;
        if (CARD_RANK(p->starter) == RANK_JACK) {
            ev_add(p, p->dealer, CE_HEELS, 2, 0, CS_CUT, 0);
            if (!peg(p, p->dealer, 2)) break;
        }
        p->turn = (uint8_t)pl;
        break;
    case CP_PLAY: {
        int s = act - CA_PLAY, card = p->kept[pl][s];
        p->used[pl] |= (uint8_t)(1u << s);
        p->seq[p->nseq] = (uint8_t)card;
        p->by[p->nseq++] = (uint8_t)pl;
        p->count = (uint8_t)(p->count + crib_value(card));
        p->last = (uint8_t)pl;
        play_points(p, pl);
        if (p->phase == CP_PLAY) advance(p);
        break;
    }
    case CP_SHOW:
        if (p->shown <= p->players) score_show(p);
        else {
            p->dealer = (uint8_t)((p->dealer + 1) % p->players);
            deal(p);
        }
        break;
    }
}

int crib_play(cgame *g, int act)
{
    if (g->n >= CRIB_MAX_ACTS || !crib_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint8_t)act;
    return 1;
}

int crib_undo(cgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    uint8_t acts[CRIB_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n);
    crib_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) crib_play(g, acts[i]);
    return 1;
}

int crib_winner(const cpos *p)
{
    if (p->phase != CP_OVER) return -1;
    for (int q = 0; q < p->players; q++)
        if (p->score[q] >= CRIB_GOAL) return q;
    return -1;
}

int crib_auto(const cpos *p)
{
    switch (p->phase) {
    case CP_DISCARD: return crib_away(p) == 2 ? 3 : 1;   /* the first cards */
    case CP_CUT: return CA_CUT;
    case CP_PLAY:
        for (int s = 0; s < 4; s++)
            if (crib_fits(p, p->turn, s)) return CA_PLAY + s;
        return 0;
    case CP_SHOW: return CA_NEXT;
    default: return 0;
    }
}

int crib_script(cgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        if (c == '#') {                         /* plain moves to the show (in the show: to the next deal) */
            int show = g->pos.phase == CP_SHOW;
            while (g->pos.phase != CP_OVER && (show ? g->pos.phase == CP_SHOW : g->pos.phase != CP_SHOW))
                if (!crib_play(g, crib_auto(&g->pos))) return 0;
            continue;
        }
        int act = 0, slots = 0, any = 0;
        while (*s >= '0' && *s <= '9') slots |= 1 << (*s++ - '0'), any = 1;
        switch (c) {
        case 'd': act = slots; break;
        case 'p':
            for (int i = 0; i < 4; i++)
                if (slots == 1 << i) act = CA_PLAY + i;
            break;
        case 'c': act = CA_CUT; break;
        case 'n': act = CA_NEXT; break;
        default: return 0;
        }
        if ((c == 'd' || c == 'p') && !any) return 0;
        if (!crib_play(g, act)) return 0;
    }
    return 1;
}
