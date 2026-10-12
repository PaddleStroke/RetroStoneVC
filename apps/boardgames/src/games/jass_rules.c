/*
 * Jass (the Schieber): the rules (jass.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/jass.h"
#include "games/trick4.h"
#include "games/tricks.h"

/* ---- the cards: ranks 6..10, jack 11, queen 12, king 13, ace 1 --------------------------------------------------- */
static int rank_of(int card)
{
    int r = CARD_RANK(card) & 15;
    return r > 13 ? 0 : r;
}

int jas_points(int card, int mode)
{
    static const int8_t TRUMP[14] = {0, 11, 0, 0, 0, 0, 0, 0, 0, 14, 10, 20, 3, 4};
    static const int8_t PLAIN[14] = {0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 10, 2, 3, 4};
    static const int8_t OBEN[14] = {0, 11, 0, 0, 0, 0, 0, 0, 8, 0, 10, 2, 3, 4};
    static const int8_t UNDE[14] = {0, 0, 0, 0, 0, 0, 11, 0, 8, 0, 10, 2, 3, 4};
    int r = rank_of(card);
    if (mode == JM_OBEN) return OBEN[r];
    if (mode == JM_UNDE) return UNDE[r];
    return CARD_SUIT(card) == mode ? TRUMP[r] : PLAIN[r];
}

int jas_power(int card, int mode)
{
    static const int8_t TRUMP[14] = {0, 7, 0, 0, 0, 0, 1, 2, 3, 8, 4, 9, 5, 6};   /* J 9 A K Q 10 8 7 6 */
    static const int8_t PLAIN[14] = {0, 9, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};   /* A K Q J 10 9 8 7 6 */
    int r = rank_of(card);
    if (mode == JM_UNDE) return r ? 10 - PLAIN[r] : 0;   /* 6 7 8 9 10 J Q K A */
    return mode < 4 && CARD_SUIT(card) == mode ? TRUMP[r] : PLAIN[r];
}

static int trump(int card, int mode) { return mode < 4 && CARD_SUIT(card) == mode; }

int jas_winner(const uint8_t *cards, int n, int mode)
{
    int suit[TK4_P], power[TK4_P];
    for (int i = 0; i < n && i < TK4_P; i++) {
        suit[i] = trump(cards[i], mode) ? SUIT_TRUMP : CARD_SUIT(cards[i]);
        power[i] = jas_power(cards[i], mode);
    }
    return tk4_best(suit, power, n);
}

int jas_mult(int variant, int mode)
{
    if (variant != JASS_2500) return 1;
    return mode >= JM_OBEN ? 3 : mode == SUIT_SPADES || mode == SUIT_CLUBS ? 2 : 1;
}

int jas_target(int variant) { return variant == JASS_2500 ? 2500 : 1000; }

/* ---- the weis ---------------------------------------------------------------------------------------------------- */
static int seq_index(int rank) { return rank == RANK_ACE ? 8 : rank - 6; }   /* 6 7 8 9 10 J Q K A: 0..8 */
static int seq_rank(int index) { return index == 8 ? RANK_ACE : index + 6; }

int jas_meld_points(jsmeld m)
{
    static const int16_t seq[10] = {0, 0, 0, 20, 50, 100, 150, 200, 250, 300};
    if (m.len & 16) {
        int r = CARD_RANK(m.top);
        return r == RANK_JACK ? 200 : r == 9 ? 150 : 100;
    }
    return m.len <= 9 ? seq[m.len] : 0;
}

static int32_t meld_key(jsmeld m, int mode)
{
    int four = (m.len & 16) != 0;
    return jas_meld_points(m) * 64 + four * 32 + seq_index(CARD_RANK(m.top)) * 2 +
           (!four && CARD_SUIT(m.top) == mode);
}

int jas_melds(const uint8_t *h, int n, int mode, jsmeld *out, int *count, int32_t *key)
{
    int k = 0, pts = 0;
    int32_t best = -1;
    jsmeld m[8];
    for (int s = 0; s < 4; s++) {               /* sequences */
        unsigned bits = 0;
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(h[i]) == s && rank_of(h[i]) && (rank_of(h[i]) == RANK_ACE || rank_of(h[i]) >= 6))
                bits |= 1u << seq_index(rank_of(h[i]));
        for (int b = 0, run = 0; b <= 9; b++) {
            if (b < 9 && bits >> b & 1) { run++; continue; }
            if (run >= 3 && k < 8) m[k++] = (jsmeld){(uint8_t)run, CARD(s, seq_rank(b - 1))};
            run = 0;
        }
    }
    static const int fours[6] = {RANK_JACK, 9, RANK_ACE, RANK_KING, RANK_QUEEN, 10};
    for (int f = 0; f < 6; f++) {               /* four of a kind */
        int got = 0;
        for (int i = 0; i < n; i++) got += rank_of(h[i]) == fours[f];
        if (got == 4 && k < 8) m[k++] = (jsmeld){4 | 16, CARD(0, fours[f])};
    }
    for (int i = 0; i < k; i++) {
        pts += jas_meld_points(m[i]);
        if (meld_key(m[i], mode) > best) best = meld_key(m[i], mode);
        if (out && i < JAS_MELDS) out[i] = m[i];
    }
    if (count) *count = k < JAS_MELDS ? k : JAS_MELDS;
    if (key) *key = best;
    return pts;
}

/* ---- the hand as shown ------------------------------------------------------------------------------------------- */
void jas_sort(uint8_t *h, int n, int mode)
{
    static const int cycle[4] = {SUIT_SPADES, SUIT_HEARTS, SUIT_CLUBS, SUIT_DIAMONDS};
    int place[4], k = 0, m = mode == JAS_NONE ? JM_OBEN : mode;
    if (m >= 4)
        for (int i = 0; i < 4; i++) place[cycle[i]] = i;
    else {
        int red = m == SUIT_HEARTS || m == SUIT_DIAMONDS, same = -1, other[2], no = 0;
        place[m] = k++;
        for (int i = 0; i < 4; i++) {
            int s = cycle[i], r = s == SUIT_HEARTS || s == SUIT_DIAMONDS;
            if (s == m) continue;
            if (r == red) same = s;
            else other[no++] = s;
        }
        place[other[0]] = k++;
        place[same] = k++;
        place[other[1]] = k++;
    }
    int16_t key[256];
    for (int c = 0; c < 256; c++) key[c] = (int16_t)(c < 128 ? place[CARD_SUIT(c) & 3] * 16 + 15 - jas_power(c, m) : 999);
    tk4_sort(h, n, key);
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static int voorhand(const jspos *p) { return tk4_next(p->dealer); }

static void deal(jspos *p)
{
    static const int ranks[] = {RANK_ACE, 6, 7, 8, 9, 10, RANK_JACK, RANK_QUEEN, RANK_KING, 0};
    static const int packets[] = {3, 3, 3, 0};
    uint8_t pack[36];
    tk4_pack(pack, ranks, 1, trick_mix(p->seed, 4000u + p->deal));
    p->dealer = (uint8_t)((3 + p->deal) % TK4_P);
    tk4_deal(&p->t, pack, packets, voorhand(p));
    for (int q = 0; q < TK4_P; q++) jas_sort(p->t.hand[q], p->t.n[q], JAS_NONE);
    p->chooser = p->mode = p->weis_team = p->stock_pl = JAS_NONE;
    p->pushed = p->stock_done = 0;
    memset(p->declared, 0, sizeof p->declared);
    memset(p->wpts, 0, sizeof p->wpts);
    memset(p->wkey, 0, sizeof p->wkey);
    memset(p->nmeld, 0, sizeof p->nmeld);
    p->weis[0] = p->weis[1] = p->stock[0] = p->stock[1] = 0;
    p->turn = (uint8_t)voorhand(p);
    p->phase = JP_CHOOSE;
}

void jas_new(jsgame *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    g->variant = g->pos.variant = (uint8_t)(variant == JASS_2500 ? JASS_2500 : JASS_1000);
    g->pos.first = JAS_NONE;
    deal(&g->pos);
}

/* ---- legality ---------------------------------------------------------------------------------------------------- */
int jas_can_play(const jspos *p, int card)
{
    if (p->phase != JP_PLAY) return 0;
    const tk4_table *t = &p->t;
    int pl = tk4_turn(t), m = p->mode, n = t->n[pl], s = CARD_SUIT(card);
    const uint8_t *h = t->hand[pl];
    if (!tk4_holds(t, pl, card)) return 0;
    if (!t->ntrick) return 1;
    int led = CARD_SUIT(t->trick[0]), has_led = trick_has_suit(h, n, led);
    if (m >= 4) return !has_led || s == led;
    if (led == m) {                             /* trumps led: a trump, but the Puur need not be played */
        int others = 0;
        for (int i = 0; i < n; i++) others += CARD_SUIT(h[i]) == m && rank_of(h[i]) != RANK_JACK;
        return !others || s == m;
    }
    if (s == m) {                               /* a trump on another suit: never under one in the trick */
        int best = 0, all = 1;
        for (int i = 0; i < t->ntrick; i++)
            if (trump(t->trick[i], m) && jas_power(t->trick[i], m) > best) best = jas_power(t->trick[i], m);
        for (int i = 0; i < n; i++) all &= CARD_SUIT(h[i]) == m;
        return !best || jas_power(card, m) > best || all;
    }
    return !has_led || s == led;
}

int jas_can_weis(const jspos *p)
{
    int pl = p->turn;
    if (p->phase != JP_PLAY || p->t.tricks || p->declared[pl] || p->t.n[pl] != JAS_HAND) return 0;
    return jas_melds(p->t.hand[pl], p->t.n[pl], p->mode, NULL, NULL, NULL) > 0;
}

int jas_legal(const jspos *p, int act)
{
    int a = JA_ARG(act);
    switch (JA_KIND(act)) {
    case JA_MODE: return p->phase == JP_CHOOSE && a < JM_MODES;
    case JA_PUSH: return p->phase == JP_CHOOSE && !p->pushed && !a;
    case JA_WEIS: return jas_can_weis(p) && !a;
    case JA_PLAY: return jas_can_play(p, a);
    case JA_NEXT: return p->phase == JP_DEAL && !a;
    default: return 0;
    }
}

/* ---- scoring ----------------------------------------------------------------------------------------------------- */
static void add(jspos *p, int team, int points)
{
    p->total[team] = (int16_t)(p->total[team] + points);
    if (p->first == JAS_NONE && p->total[team] >= jas_target(p->variant)) p->first = (uint8_t)team;
}

/* the first trick taken: the best weis's team scores all its weis */
static void weis_end(jspos *p)
{
    int32_t best = -1;
    int team = JAS_NONE;
    for (int i = 0; i < TK4_P; i++) {           /* in play order: the first of equal ones stays */
        int pl = (p->t.last_leader + i) % TK4_P;
        if (p->declared[pl] && p->wpts[pl] && p->wkey[pl] > best) best = p->wkey[pl], team = tk4_team(pl);
    }
    if (team == JAS_NONE) return;
    p->weis_team = (uint8_t)team;
    for (int q = 0; q < TK4_P; q++)
        if (tk4_team(q) == team && p->declared[q]) p->weis[team] = (int16_t)(p->weis[team] + p->wpts[q]);
    add(p, team, p->weis[team] * jas_mult(p->variant, p->mode));
}

static void deal_end(jspos *p)
{
    jsdeal *r = &p->list[p->ndeals < JAS_DEALS ? p->ndeals : JAS_DEALS - 1];
    int mult = jas_mult(p->variant, p->mode), before = p->first;
    memset(r, 0, sizeof *r);
    r->chooser = p->chooser;
    r->pushed = p->pushed;
    r->mode = p->mode;
    r->mult = (uint8_t)mult;
    for (int q = 0; q < TK4_P; q++) r->eyes[tk4_team(q)] = (int16_t)(r->eyes[tk4_team(q)] + p->t.eyes[q]);
    for (int k = 0; k < 2; k++) {
        if (p->t.won[k] + p->t.won[k + 2] == JAS_HAND) r->match = (uint8_t)(k + 1);
        r->weis[k] = p->weis[k];
        r->stock[k] = p->stock[k];
    }
    int trick[2];
    for (int k = 0; k < 2; k++) {
        trick[k] = (r->eyes[k] + (r->match == k + 1 ? 100 : 0)) * mult;
        r->score[k] = (int16_t)(trick[k] + (r->weis[k] + r->stock[k]) * mult);
        p->total[k] = (int16_t)(p->total[k] + trick[k]);
    }
    if (before == JAS_NONE) {                   /* over the target with the tricks: the higher total */
        int target = jas_target(p->variant), a = p->total[0] >= target, b = p->total[1] >= target;
        if (a && b)
            p->first = (uint8_t)(p->total[0] != p->total[1] ? p->total[1] > p->total[0]
                                                            : tk4_team(p->t.last_winner));
        else if (a || b)
            p->first = (uint8_t)(b ? 1 : 0);
    }
    if (p->ndeals < JAS_DEALS) p->ndeals++;
    p->phase = p->first != JAS_NONE || p->ndeals >= JAS_DEALS ? JP_OVER : JP_DEAL;
    if (p->phase == JP_OVER && p->first == JAS_NONE) p->first = (uint8_t)(p->total[1] > p->total[0]);
    p->turn = (uint8_t)tk4_next(tk4_next(p->dealer));   /* the next deal's chooser */
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static void apply(jspos *p, int act)
{
    int pl = p->turn, a = JA_ARG(act);
    switch (JA_KIND(act)) {
    case JA_PUSH:
        p->pushed = 1;
        p->turn = (uint8_t)tk4_partner(pl);
        break;
    case JA_MODE:
        p->mode = (uint8_t)a;
        p->chooser = (uint8_t)pl;
        for (int q = 0; q < TK4_P; q++) {
            jas_sort(p->t.hand[q], p->t.n[q], a);
            if (a < 4 && tk4_holds(&p->t, q, CARD(a, RANK_KING)) && tk4_holds(&p->t, q, CARD(a, RANK_QUEEN)))
                p->stock_pl = (uint8_t)q;
        }
        p->phase = JP_PLAY;
        p->turn = (uint8_t)tk4_turn(&p->t);
        break;
    case JA_WEIS: {
        int k;
        p->declared[pl] = 1;
        p->wpts[pl] = (int16_t)jas_melds(p->t.hand[pl], p->t.n[pl], p->mode, p->meld[pl], &k, &p->wkey[pl]);
        p->nmeld[pl] = (uint8_t)k;
        break;
    }
    case JA_PLAY: {
        tk4_table *t = &p->t;
        tk4_lay(t, a);
        int m = p->mode;
        if (pl == p->stock_pl && !p->stock_done && m < 4 && CARD_SUIT(a) == m &&
            (CARD_RANK(a) == RANK_KING || CARD_RANK(a) == RANK_QUEEN) && !tk4_holds(t, pl, CARD(m, RANK_KING)) &&
            !tk4_holds(t, pl, CARD(m, RANK_QUEEN))) {   /* stöck: the second of the two played */
            p->stock_done = 1;
            p->stock[tk4_team(pl)] = 20;
            add(p, tk4_team(pl), 20 * jas_mult(p->variant, m));
        }
        if (t->ntrick == TK4_P) {
            int w = (t->leader + jas_winner(t->trick, TK4_P, m)) % TK4_P, eyes = 0;
            for (int i = 0; i < TK4_P; i++) eyes += jas_points(t->trick[i], m);
            if (t->tricks == JAS_HAND - 1) eyes += 5;   /* the last trick */
            tk4_take(t, w, eyes);
            if (t->tricks == 1) weis_end(p);
            if (t->tricks == JAS_HAND) { deal_end(p); break; }
        }
        p->turn = (uint8_t)tk4_turn(t);
        break;
    }
    case JA_NEXT:
        p->deal++;
        deal(p);
        break;
    }
}

int jas_play(jsgame *g, int act)
{
    if (g->n >= JAS_MAX_ACTS || !jas_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int jas_undo(jsgame *g)
{
    if (!g->n) return 0;
    static uint16_t acts[JAS_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    jas_new(g, g->variant, g->seed);
    for (int i = 0; i < n; i++) jas_play(g, acts[i]);
    return 1;
}

int jas_winners(const jspos *p)
{
    if (p->phase != JP_OVER || p->first == JAS_NONE) return 0;
    return 1 << p->first | 1 << (p->first + 2);
}

/* ---- plain actions ----------------------------------------------------------------------------------------------- */
static int hope(const uint8_t *h, int n, int mode)   /* how good a game looks with this hand */
{
    int v = 0;
    for (int i = 0; i < n; i++) {
        int pw = jas_power(h[i], mode);
        if (mode < 4) v += CARD_SUIT(h[i]) == mode ? 6 + jas_points(h[i], mode) : pw == 9 ? 8 : 0;
        else v += pw == 9 ? 14 : pw == 8 ? 6 : 0;
    }
    return v;
}

int jas_auto(const jspos *p)
{
    const tk4_table *t = &p->t;
    int pl = p->turn;
    switch (p->phase) {
    case JP_CHOOSE: {
        int best = 0;
        for (int m = 1; m < JM_MODES; m++)
            if (hope(t->hand[pl], t->n[pl], m) > hope(t->hand[pl], t->n[pl], best)) best = m;
        return hope(t->hand[pl], t->n[pl], best) < 45 && !p->pushed ? JA_PUSH : JA_MODE | best;
    }
    case JP_PLAY:
        if (jas_can_weis(p)) return JA_WEIS;
        for (int i = 0; i < t->n[pl]; i++)
            if (jas_can_play(p, t->hand[pl][i])) return JA_PLAY | t->hand[pl][i];
        return 0;
    case JP_DEAL: return JA_NEXT;
    default: return 0;
    }
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int jas_script(jsgame *g, const char *s)
{
    const jspos *p = &g->pos;
    for (;;) {
        tk4_space(&s);
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (p->phase != JP_OVER && (all || p->phase != JP_DEAL))
                if (!jas_play(g, jas_auto(p))) return 0;
            continue;
        }
        char c = *s++;
        if (c == '.') act = jas_auto(p);
        else if (c == 'p') act = JA_PUSH;
        else if (c == 'w') act = JA_WEIS;
        else if (c == 'o') act = JA_MODE | JM_OBEN;
        else if (c == 'u') act = JA_MODE | JM_UNDE;
        else if (c == 'N') act = JA_NEXT;
        else if (c == 't') {
            int su = tk4_suit_letter(&s);
            if (su < 0) return 0;
            act = JA_MODE | su;
        } else {
            s--;
            int card = trick_parse(&s);
            if (card == CARD_NONE) return 0;
            act = JA_PLAY | card;
        }
        if (!jas_play(g, act)) return 0;
    }
    return 1;
}
