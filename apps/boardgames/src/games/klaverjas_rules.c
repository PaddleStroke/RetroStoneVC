/*
 * Klaverjas: the rules (klaverjas.h), Rotterdams. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/klaverjas.h"
#include "games/trick4.h"
#include "games/tricks.h"

/* ---- the cards: ranks 7..10, jack 11, queen 12, king 13, ace 1 --------------------------------------------------- */
int kvj_points(int card, int trump)
{
    static const int8_t TRUMP[14] = {0, 11, 0, 0, 0, 0, 0, 0, 0, 14, 10, 20, 3, 4};
    static const int8_t PLAIN[14] = {0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 10, 2, 3, 4};
    int r = CARD_RANK(card) & 15;
    return r > 13 ? 0 : CARD_SUIT(card) == trump ? TRUMP[r] : PLAIN[r];
}

int kvj_power(int card, int trump)
{
    static const int8_t TRUMP[14] = {0, 6, 0, 0, 0, 0, 0, 1, 2, 7, 5, 8, 3, 4};   /* J 9 A 10 K Q 8 7 */
    static const int8_t PLAIN[14] = {0, 8, 0, 0, 0, 0, 0, 1, 2, 3, 7, 4, 5, 6};   /* A 10 K Q J 9 8 7 */
    int r = CARD_RANK(card) & 15;
    return r > 13 ? 0 : CARD_SUIT(card) == trump ? TRUMP[r] : PLAIN[r];
}

int kvj_winner(const uint8_t *cards, int n, int trump)
{
    int suit[TK4_P], power[TK4_P];
    for (int i = 0; i < n && i < TK4_P; i++) {
        suit[i] = CARD_SUIT(cards[i]) == trump ? SUIT_TRUMP : CARD_SUIT(cards[i]);
        power[i] = kvj_power(cards[i], trump);
    }
    return tk4_best(suit, power, n);
}

int kvj_roem(const uint8_t *cards, int n, int trump)
{
    if (n == 4 && CARD_RANK(cards[0]) == CARD_RANK(cards[1]) && CARD_RANK(cards[0]) == CARD_RANK(cards[2]) &&
        CARD_RANK(cards[0]) == CARD_RANK(cards[3])) {   /* four of a kind (no sequence then) */
        int r = CARD_RANK(cards[0]);
        return r == RANK_JACK ? 200 : r == RANK_ACE || r >= 10 ? 100 : 0;
    }
    static const int8_t place[14] = {0, 7, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6};   /* 7 8 9 10 J Q K A */
    int roem = 0, king = 0, queen = 0;
    for (int s = 0; s < 4; s++) {
        unsigned bits = 0;
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(cards[i]) == s) bits |= 1u << place[CARD_RANK(cards[i]) & 15];
        int run = 0, best = 0;
        for (int b = 0; b < 8; b++) {
            run = bits >> b & 1 ? run + 1 : 0;
            if (run > best) best = run;
        }
        roem += best >= 4 ? 50 : best == 3 ? 20 : 0;
    }
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(cards[i]) == trump) {
            king |= CARD_RANK(cards[i]) == RANK_KING;
            queen |= CARD_RANK(cards[i]) == RANK_QUEEN;
        }
    return roem + (king && queen ? 20 : 0);
}

/* the suits' places in a hand as shown: trumps first, then the others, the colours alternating */
void kvj_sort(uint8_t *h, int n, int trump)
{
    static const int cycle[4] = {SUIT_SPADES, SUIT_HEARTS, SUIT_CLUBS, SUIT_DIAMONDS};
    int place[4], k = 0;
    if (trump == KVJ_NONE || trump > 3)
        for (int i = 0; i < 4; i++) place[cycle[i]] = i;
    else {
        int red = trump == SUIT_HEARTS || trump == SUIT_DIAMONDS, same = -1, other[2], no = 0;
        place[trump] = k++;
        for (int i = 0; i < 4; i++) {
            int s = cycle[i], r = s == SUIT_HEARTS || s == SUIT_DIAMONDS;
            if (s == trump) continue;
            if (r == red) same = s;
            else other[no++] = s;
        }
        place[other[0]] = k++;
        place[same] = k++;
        place[other[1]] = k++;
    }
    int16_t key[256];
    for (int c = 0; c < 256; c++) key[c] = (int16_t)(c < 128 ? place[CARD_SUIT(c) & 3] * 16 + 15 - kvj_power(c, trump) : 999);
    tk4_sort(h, n, key);
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static int voorhand(const kvpos *p) { return tk4_next(p->dealer); }

static void deal(kvpos *p)
{
    static const int ranks[] = {RANK_ACE, 7, 8, 9, 10, RANK_JACK, RANK_QUEEN, RANK_KING, 0};
    static const int packets[] = {3, 2, 3, 0};
    uint8_t pack[32];
    tk4_pack(pack, ranks, 1, trick_mix(p->seed, 3000u + p->deal));
    p->dealer = (uint8_t)((3 + p->deal) % TK4_P);
    tk4_deal(&p->t, pack, packets, voorhand(p));
    for (int q = 0; q < TK4_P; q++) kvj_sort(p->t.hand[q], p->t.n[q], KVJ_NONE);
    memset(p->word, KW_NONE, sizeof p->word);
    p->passes = 0;
    p->taker = p->trump = KVJ_NONE;
    p->roem[0] = p->roem[1] = 0;
    p->trick_roem = 0;
    p->turn = (uint8_t)voorhand(p);
    p->phase = KP_BID;
}

void kvj_new(kvgame *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    deal(&g->pos);
}

/* ---- legality ---------------------------------------------------------------------------------------------------- */
int kvj_must_name(const kvpos *p) { return p->phase == KP_BID && p->passes >= TK4_P; }

int kvj_can_play(const kvpos *p, int card)
{
    if (p->phase != KP_PLAY) return 0;
    const tk4_table *t = &p->t;
    int pl = tk4_turn(t), T = p->trump, n = t->n[pl], s = CARD_SUIT(card);
    const uint8_t *h = t->hand[pl];
    if (!tk4_holds(t, pl, card)) return 0;
    if (!t->ntrick) return 1;
    int led = CARD_SUIT(t->trick[0]), best = 0, over = 0, other = 0;
    for (int i = 0; i < t->ntrick; i++)
        if (CARD_SUIT(t->trick[i]) == T && kvj_power(t->trick[i], T) > best) best = kvj_power(t->trick[i], T);
    for (int i = 0; i < n; i++) {
        if (CARD_SUIT(h[i]) == T && kvj_power(h[i], T) > best) over = 1;
        if (CARD_SUIT(h[i]) != T) other = 1;
    }
    int has_trump = trick_has_suit(h, n, T);
    if (led == T) {                             /* trumps led: a trump, higher if you can */
        if (!has_trump) return 1;
        return s == T && (!over || kvj_power(card, T) > best);
    }
    if (trick_has_suit(h, n, led)) return s == led;   /* follow suit */
    if (!has_trump) return 1;
    if (!best) return s == T;                   /* void: trump */
    if (over) return s == T && kvj_power(card, T) > best;   /* over the trick's trump */
    return other ? s != T : 1;                  /* no undertrumping, unless only trumps are left */
}

int kvj_legal(const kvpos *p, int act)
{
    int a = KA_ARG(act);
    switch (KA_KIND(act)) {
    case KA_PASS: return p->phase == KP_BID && !kvj_must_name(p) && !a;
    case KA_TRUMP: return p->phase == KP_BID && a < 4;
    case KA_PLAY: return kvj_can_play(p, a);
    case KA_NEXT: return p->phase == KP_DEAL && !a;
    default: return 0;
    }
}

/* ---- the deal's end ---------------------------------------------------------------------------------------------- */
static void deal_end(kvpos *p)
{
    kvdeal *r = &p->list[p->ndeals < KVJ_DEALS ? p->ndeals : KVJ_DEALS - 1];
    memset(r, 0, sizeof *r);
    r->taker = p->taker;
    r->trump = p->trump;
    for (int q = 0; q < TK4_P; q++) r->eyes[tk4_team(q)] = (int16_t)(r->eyes[tk4_team(q)] + p->t.eyes[q]);
    for (int k = 0; k < 2; k++)
        if (p->t.won[k] + p->t.won[k + 2] == KVJ_HAND) {   /* pit */
            r->pit = (uint8_t)(k + 1);
            p->roem[k] = (int16_t)(p->roem[k] + 100);
        }
    int P = tk4_team(p->taker), O = 1 - P;
    r->roem[0] = p->roem[0];
    r->roem[1] = p->roem[1];
    int all = KVJ_TOTAL + r->roem[0] + r->roem[1], mine = r->eyes[P] + r->roem[P];
    r->made = 2 * mine > all;
    if (r->made) {
        r->score[P] = (int16_t)mine;
        r->score[O] = (int16_t)(r->eyes[O] + r->roem[O]);
    } else {                                    /* nat: everything to the others */
        r->score[P] = 0;
        r->score[O] = (int16_t)all;
    }
    for (int k = 0; k < 2; k++) p->total[k] = (int16_t)(p->total[k] + r->score[k]);
    if (p->ndeals < KVJ_DEALS) p->ndeals++;
    p->phase = p->ndeals >= KVJ_DEALS ? KP_OVER : KP_DEAL;
    p->turn = (uint8_t)tk4_next(tk4_next(p->dealer));   /* the next deal's voorhand */
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static void apply(kvpos *p, int act)
{
    int pl = p->turn, a = KA_ARG(act);
    switch (KA_KIND(act)) {
    case KA_PASS:
        p->word[pl] = KW_PASS;
        p->passes++;
        p->turn = (uint8_t)(p->passes >= TK4_P ? voorhand(p) : tk4_next(pl));
        break;
    case KA_TRUMP:
        p->word[pl] = KW_TRUMP;
        p->taker = (uint8_t)pl;
        p->trump = (uint8_t)a;
        for (int q = 0; q < TK4_P; q++) kvj_sort(p->t.hand[q], p->t.n[q], a);
        p->phase = KP_PLAY;
        p->turn = (uint8_t)tk4_turn(&p->t);
        break;
    case KA_PLAY: {
        tk4_table *t = &p->t;
        tk4_lay(t, a);
        if (t->ntrick == TK4_P) {
            int w = (t->leader + kvj_winner(t->trick, TK4_P, p->trump)) % TK4_P, eyes = 0;
            for (int i = 0; i < TK4_P; i++) eyes += kvj_points(t->trick[i], p->trump);
            if (t->tricks == KVJ_HAND - 1) eyes += 10;   /* the last trick */
            p->trick_roem = (int16_t)kvj_roem(t->trick, TK4_P, p->trump);
            p->roem[tk4_team(w)] = (int16_t)(p->roem[tk4_team(w)] + p->trick_roem);
            tk4_take(t, w, eyes);
            if (t->tricks == KVJ_HAND) { deal_end(p); break; }
        }
        p->turn = (uint8_t)tk4_turn(t);
        break;
    }
    case KA_NEXT:
        p->deal++;
        deal(p);
        break;
    }
}

int kvj_play(kvgame *g, int act)
{
    if (g->n >= KVJ_MAX_ACTS || !kvj_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int kvj_undo(kvgame *g)
{
    if (!g->n) return 0;
    static uint16_t acts[KVJ_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    kvj_new(g, g->seed);
    for (int i = 0; i < n; i++) kvj_play(g, acts[i]);
    return 1;
}

int kvj_winners(const kvpos *p)
{
    if (p->phase != KP_OVER) return 0;
    if (p->total[0] == p->total[1]) return 15;
    int w = p->total[1] > p->total[0];
    return 1 << w | 1 << (w + 2);
}

/* ---- plain actions ----------------------------------------------------------------------------------------------- */
static int strength(const uint8_t *h, int n, int suit)   /* the trumps' points in hand, if this suit were trumps */
{
    int v = 0;
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) == suit) v += kvj_points(h[i], suit) + 3;
    return v;
}

int kvj_auto(const kvpos *p)
{
    const tk4_table *t = &p->t;
    int pl = p->turn;
    switch (p->phase) {
    case KP_BID: {
        int best = 0;
        for (int s = 1; s < 4; s++)
            if (strength(t->hand[pl], t->n[pl], s) > strength(t->hand[pl], t->n[pl], best)) best = s;
        return strength(t->hand[pl], t->n[pl], best) >= 40 || kvj_must_name(p) ? KA_TRUMP | best : KA_PASS;
    }
    case KP_PLAY:
        for (int i = 0; i < t->n[pl]; i++)
            if (kvj_can_play(p, t->hand[pl][i])) return KA_PLAY | t->hand[pl][i];
        return 0;
    case KP_DEAL: return KA_NEXT;
    default: return 0;
    }
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int kvj_script(kvgame *g, const char *s)
{
    const kvpos *p = &g->pos;
    for (;;) {
        tk4_space(&s);
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (p->phase != KP_OVER && (all || p->phase != KP_DEAL))
                if (!kvj_play(g, kvj_auto(p))) return 0;
            continue;
        }
        char c = *s++;
        if (c == '.') act = kvj_auto(p);
        else if (c == 'p') act = KA_PASS;
        else if (c == 'N') act = KA_NEXT;
        else if (c == 't') {
            int su = tk4_suit_letter(&s);
            if (su < 0) return 0;
            act = KA_TRUMP | su;
        } else {
            s--;
            int card = trick_parse(&s);
            if (card == CARD_NONE) return 0;
            act = KA_PLAY | card;
        }
        if (!kvj_play(g, act)) return 0;
    }
    return 1;
}
