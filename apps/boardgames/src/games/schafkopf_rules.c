/*
 * Schafkopf: the rules (schafkopf.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/schafkopf.h"
#include "games/trick4.h"
#include "games/tricks.h"

/* ---- the cards: 7..10, Unter (jack) 11, Ober (queen) 12, king 13, Sau (ace) 1 ----------------------------------- */
static const int8_t HIGH[4] = {2, 1, 0, 3};     /* the Obers' and Unters' order: clubs 3, spades 2, hearts 1, diamonds 0
                                                 * (indexed by suit: spades, hearts, diamonds, clubs) */

static int rank_of(int card)
{
    int r = CARD_RANK(card) & 15;
    return r > 13 ? 0 : r;
}

int sfk_points(int card)
{
    static const int8_t P[14] = {0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 10, 2, 3, 4};
    return P[rank_of(card)];
}

/* the trump suit of a game (-1: none, the Wenz) */
static int trump_suit(int kind, int suit) { return kind == SFG_WENZ ? -1 : kind == SFG_SOLO ? suit : SUIT_HEARTS; }

int sfk_trump(int card, int kind, int suit)
{
    int r = rank_of(card);
    if (r == RANK_JACK) return 1;
    if (kind == SFG_WENZ) return 0;
    return r == RANK_QUEEN || CARD_SUIT(card) == trump_suit(kind, suit);
}

int sfk_power(int card, int kind, int suit)
{
    static const int8_t PLAIN[14] = {0, 8, 0, 0, 0, 0, 0, 1, 2, 3, 7, 4, 5, 6};   /* A 10 K (O) (U) 9 8 7 */
    int r = rank_of(card), s = CARD_SUIT(card) & 3;
    if (r == RANK_JACK) return 20 + HIGH[s];
    if (r == RANK_QUEEN && kind != SFG_WENZ) return 30 + HIGH[s];
    return (sfk_trump(card, kind, suit) ? 10 : 0) + PLAIN[r];
}

static int eff_suit(int card, int kind, int suit) { return sfk_trump(card, kind, suit) ? SUIT_TRUMP : CARD_SUIT(card); }

int sfk_winner(const uint8_t *cards, int n, int kind, int suit)
{
    int su[TK4_P], pw[TK4_P];
    for (int i = 0; i < n && i < TK4_P; i++) {
        su[i] = eff_suit(cards[i], kind, suit);
        pw[i] = sfk_power(cards[i], kind, suit);
    }
    return tk4_best(su, pw, n);
}

/* the trumps from the top: the Obers, the Unters, the trump suit's A 10 K 9 8 7; how many */
static int trump_run(int kind, int suit, uint8_t *out)
{
    static const int order[4] = {SUIT_CLUBS, SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS};
    static const int ranks[6] = {RANK_ACE, 10, RANK_KING, 9, 8, 7};
    int n = 0;
    if (kind != SFG_WENZ)
        for (int i = 0; i < 4; i++) out[n++] = CARD(order[i], RANK_QUEEN);
    for (int i = 0; i < 4; i++) out[n++] = CARD(order[i], RANK_JACK);
    if (kind != SFG_WENZ)
        for (int i = 0; i < 6; i++) out[n++] = CARD(trump_suit(kind, suit), ranks[i]);
    return n;
}

int sfk_runners(const uint8_t (*hands)[SFK_HAND], unsigned side, int kind, int suit)
{
    uint8_t run[14];
    int n = trump_run(kind, suit, run), k = 0, with = -1;
    for (; k < n; k++) {
        int holder = -1;
        for (int q = 0; q < TK4_P && holder < 0; q++)
            if (trick_find(hands[q], SFK_HAND, run[k]) >= 0) holder = q;
        int ours = holder >= 0 && (side >> holder & 1);
        if (with < 0) with = ours;
        if (ours != with) break;
    }
    return with ? k : -k;
}

/* a hand as shown: trumps first (strongest first), then the other suits, their colours alternating */
void sfk_sort(uint8_t *h, int n, int kind, int suit)
{
    static const int cycle[4] = {SUIT_CLUBS, SUIT_HEARTS, SUIT_SPADES, SUIT_DIAMONDS};
    int ts = trump_suit(kind, suit), place[4];
    if (ts < 0)
        for (int i = 0; i < 4; i++) place[cycle[i]] = i + 1;
    else {                                      /* the colours alternate: the other colour, the same, the other */
        int red = ts == SUIT_HEARTS || ts == SUIT_DIAMONDS, same = -1, other[2], no = 0;
        place[ts] = 0;
        for (int i = 0; i < 4; i++) {
            int s = cycle[i], r = s == SUIT_HEARTS || s == SUIT_DIAMONDS;
            if (s == ts) continue;
            if (r == red) same = s;
            else other[no++] = s;
        }
        place[other[0]] = 1;
        place[same] = 2;
        place[other[1]] = 3;
    }
    int16_t key[256];
    for (int c = 0; c < 256; c++) {
        if (c >= 128) { key[c] = 999; continue; }
        key[c] = (int16_t)((sfk_trump(c, kind, suit) ? 0 : place[CARD_SUIT(c)]) * 64 + 63 - sfk_power(c, kind, suit));
    }
    tk4_sort(h, n, key);
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static int forehand(const sfpos *p) { return tk4_next(p->dealer); }

static void deal(sfpos *p)
{
    static const int ranks[] = {RANK_ACE, 7, 8, 9, 10, RANK_JACK, RANK_QUEEN, RANK_KING, 0};
    static const int packets[] = {4, 4, 0};
    uint8_t pack[32];
    tk4_pack(pack, ranks, 1, trick_mix(p->seed, 5000u + p->deal));
    p->dealer = (uint8_t)((3 + p->deal) % TK4_P);
    tk4_deal(&p->t, pack, packets, forehand(p));
    for (int q = 0; q < TK4_P; q++) {
        sfk_sort(p->t.hand[q], p->t.n[q], SFG_RUF, 0);
        memcpy(p->dealt[q], p->t.hand[q], SFK_HAND);
    }
    memset(p->word, SFW_NONE, sizeof p->word);
    memset(p->want, 0, sizeof p->want);
    p->spoke = 0;
    p->declarer = p->partner = SFK_NONE;
    p->kind = SFG_NONE;
    p->suit = 0;
    p->searched = p->revealed = 0;
    p->turn = (uint8_t)forehand(p);
    p->phase = SFP_BID;
}

void sfk_new(sfgame *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    g->variant = g->pos.variant = (uint8_t)(variant == SFK_RAMSCH ? SFK_RAMSCH : SFK_PLAIN);
    deal(&g->pos);
}

/* ---- legality ---------------------------------------------------------------------------------------------------- */
static int plain_of(const uint8_t *h, int n, int suit)   /* the cards of a suit that are not trumps in a Rufspiel */
{
    int k = 0;
    for (int i = 0; i < n; i++) k += CARD_SUIT(h[i]) == suit && !sfk_trump(h[i], SFG_RUF, 0);
    return k;
}

int sfk_game_ok(const sfpos *p, int game)
{
    int kind = SFG_KIND(game), s = SFG_SUIT(game), pl = p->turn;
    if (p->phase != SFP_BID || (game & ~0x3f)) return 0;
    if (kind == SFG_WENZ) return s == 0;
    if (kind == SFG_SOLO) return 1;
    if (kind != SFG_RUF || s == SUIT_HEARTS) return 0;
    return !tk4_holds(&p->t, pl, CARD(s, RANK_ACE)) && plain_of(p->t.hand[pl], p->t.n[pl], s) > 0;
}

int sfk_can_play(const sfpos *p, int card)
{
    if (p->phase != SFP_PLAY) return 0;
    const tk4_table *t = &p->t;
    int pl = tk4_turn(t), n = t->n[pl], K = p->kind, S = p->suit;
    const uint8_t *h = t->hand[pl];
    if (!tk4_holds(t, pl, card)) return 0;
    int ace = CARD(S, RANK_ACE), holds_ace = K == SFG_RUF && tk4_holds(t, pl, ace);
    int called = !sfk_trump(card, K, S) && CARD_SUIT(card) == S;   /* a card of the called suit */
    if (!t->ntrick) {                           /* leading the called suit: the ace, unless running away */
        if (holds_ace && !p->searched && called && card != ace) return plain_of(h, n, S) >= 4;
        return 1;
    }
    int led = eff_suit(t->trick[0], K, S), follow = 0;
    for (int i = 0; i < n; i++) follow |= eff_suit(h[i], K, S) == led;
    if (follow) {
        if (eff_suit(card, K, S) != led) return 0;
        if (holds_ace && led == S && p->searched == t->tricks + 1) return card == ace;   /* the called suit sought */
        return 1;
    }
    if (holds_ace && !p->searched && card == ace && n > 1) return 0;   /* the called ace never thrown away */
    return 1;
}

int sfk_legal(const sfpos *p, int act)
{
    int a = SFA_ARG(act);
    switch (SFA_KIND(act)) {
    case SFA_PASS: return p->phase == SFP_BID && !a;
    case SFA_GAME: return sfk_game_ok(p, a);
    case SFA_PLAY: return sfk_can_play(p, a);
    case SFA_NEXT: return p->phase == SFP_DEAL && !a;
    default: return 0;
    }
}

int sfk_side(const sfpos *p)
{
    if (p->declarer == SFK_NONE) return 0;
    return 1 << p->declarer | (p->partner != SFK_NONE ? 1 << p->partner : 0);
}

/* ---- the deal's end ---------------------------------------------------------------------------------------------- */
/* as with 61 and 60 to win: the declaring side is free of schneider with 31, the others already with 30 */
int sfk_schneider(int side, int won) { return won ? side >= 91 : side <= 30; }

static void score_ramsch(const sfpos *p, sfdeal *r)
{
    int most = -1, tricks = -1;
    for (int q = 0; q < TK4_P; q++) {
        if (p->t.won[q] == SFK_HAND) r->march = (uint8_t)(q + 1);
        if (r->eyes[q] > most) most = r->eyes[q];
    }
    if (r->march) {                             /* Durchmarsch: 50 from each */
        for (int q = 0; q < TK4_P; q++) r->score[q] = (int16_t)(q == r->march - 1 ? 150 : -50);
        r->value = 50;
        return;
    }
    for (int q = 0; q < TK4_P; q++)
        if (r->eyes[q] == most && p->t.won[q] > tricks) tricks = p->t.won[q];
    for (int q = 0; q < TK4_P; q++)
        if (r->eyes[q] == most && p->t.won[q] == tricks) r->losers |= (uint8_t)(1 << q);
    int nl = 0;
    for (int q = 0; q < TK4_P; q++) nl += r->losers >> q & 1;
    r->value = 10;
    for (int q = 0; q < TK4_P; q++) r->score[q] = (int16_t)(r->losers >> q & 1 ? -10 * (TK4_P - nl) : 10 * nl);
}

static void deal_end(sfpos *p)
{
    sfdeal *r = &p->list[p->ndeals < SFK_DEALS ? p->ndeals : SFK_DEALS - 1];
    memset(r, 0, sizeof *r);
    r->declarer = p->declarer;
    r->kind = p->kind;
    r->suit = p->suit;
    r->partner = p->partner;
    for (int q = 0; q < TK4_P; q++) r->eyes[q] = p->t.eyes[q];
    if (p->kind == SFG_RAMSCH) score_ramsch(p, r);
    else if (p->kind != SFG_NONE) {
        unsigned side = (unsigned)sfk_side(p);
        int tricks = 0;
        for (int q = 0; q < TK4_P; q++)
            if (side >> q & 1) r->side = (int16_t)(r->side + r->eyes[q]), tricks += p->t.won[q];
        r->won = r->side >= 61;
        int lose_tricks = r->won ? SFK_HAND - tricks : tricks;
        r->schneider = (uint8_t)sfk_schneider(r->side, r->won);
        r->schwarz = lose_tricks == 0;
        int run = abs(sfk_runners((const uint8_t(*)[SFK_HAND])p->dealt, side, p->kind, p->suit));
        r->runners = (uint8_t)(run >= (p->kind == SFG_WENZ ? 2 : 3) ? run : 0);
        r->value = (int16_t)((p->kind == SFG_RUF ? 10 : 50) + 10 * r->schneider + 10 * r->schwarz + 10 * r->runners);
        int v = r->won ? r->value : -r->value, solo = p->kind != SFG_RUF;
        for (int q = 0; q < TK4_P; q++)
            r->score[q] = (int16_t)(side >> q & 1 ? (solo ? 3 * v : v) : -v);
    }
    for (int q = 0; q < TK4_P; q++) p->total[q] = (int16_t)(p->total[q] + r->score[q]);
    if (p->ndeals < SFK_DEALS) p->ndeals++;
    p->phase = p->ndeals >= SFK_DEALS ? SFP_OVER : SFP_DEAL;
    p->turn = (uint8_t)tk4_next(tk4_next(p->dealer));   /* the next deal's first to speak */
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static void begin_play(sfpos *p, int declarer, int game)
{
    p->declarer = (uint8_t)declarer;
    p->kind = (uint8_t)SFG_KIND(game);
    p->suit = (uint8_t)(p->kind == SFG_WENZ || p->kind == SFG_RAMSCH ? 0 : SFG_SUIT(game));
    if (p->kind == SFG_RUF)
        for (int q = 0; q < TK4_P; q++)
            if (tk4_holds(&p->t, q, CARD(p->suit, RANK_ACE))) p->partner = (uint8_t)q;
    for (int q = 0; q < TK4_P; q++) sfk_sort(p->t.hand[q], p->t.n[q], p->kind, p->suit);
    p->phase = SFP_PLAY;
    p->turn = (uint8_t)tk4_turn(&p->t);
}

static void end_bidding(sfpos *p)
{
    int best = -1, rank = 0;
    for (int i = 0; i < TK4_P; i++) {           /* the highest game; the first to speak among equals */
        int q = (forehand(p) + i) % TK4_P, k = SFG_KIND(p->want[q]);
        if (p->word[q] == SFW_PLAY && k > rank) rank = k, best = q;
    }
    if (best >= 0) { begin_play(p, best, p->want[best]); return; }
    if (p->variant == SFK_RAMSCH) {
        begin_play(p, SFK_NONE, SFG_RAMSCH);
        p->declarer = SFK_NONE;
        return;
    }
    deal_end(p);                                /* thrown in */
}

static void apply(sfpos *p, int act)
{
    int pl = p->turn, a = SFA_ARG(act);
    switch (SFA_KIND(act)) {
    case SFA_PASS:
    case SFA_GAME:
        p->word[pl] = SFA_KIND(act) == SFA_PASS ? SFW_PASS : SFW_PLAY;
        p->want[pl] = SFA_KIND(act) == SFA_PASS ? 0 : (uint8_t)a;
        p->turn = (uint8_t)tk4_next(pl);
        if (++p->spoke == TK4_P) end_bidding(p);
        break;
    case SFA_PLAY: {
        tk4_table *t = &p->t;
        if (!t->ntrick && p->kind == SFG_RUF && !p->searched && !sfk_trump(a, p->kind, p->suit) &&
            CARD_SUIT(a) == p->suit)
            p->searched = (uint8_t)(t->tricks + 1);   /* the called suit led: sought (or run away from) */
        if (p->kind == SFG_RUF && a == CARD(p->suit, RANK_ACE)) p->revealed = 1;
        tk4_lay(t, a);
        if (t->ntrick == TK4_P) {
            int w = (t->leader + sfk_winner(t->trick, TK4_P, p->kind, p->suit)) % TK4_P, eyes = 0;
            for (int i = 0; i < TK4_P; i++) eyes += sfk_points(t->trick[i]);
            tk4_take(t, w, eyes);
            if (t->tricks == SFK_HAND) { deal_end(p); break; }
        }
        p->turn = (uint8_t)tk4_turn(t);
        break;
    }
    case SFA_NEXT:
        p->deal++;
        deal(p);
        break;
    }
}

int sfk_play(sfgame *g, int act)
{
    if (g->n >= SFK_MAX_ACTS || !sfk_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int sfk_undo(sfgame *g)
{
    if (!g->n) return 0;
    static uint16_t acts[SFK_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    sfk_new(g, g->variant, g->seed);
    for (int i = 0; i < n; i++) sfk_play(g, acts[i]);
    return 1;
}

int sfk_winners(const sfpos *p)
{
    if (p->phase != SFP_OVER) return 0;
    int best = p->total[0], m = 0;
    for (int q = 1; q < TK4_P; q++) best = p->total[q] > best ? p->total[q] : best;
    for (int q = 0; q < TK4_P; q++) m |= (p->total[q] == best) << q;
    return m;
}

/* ---- plain actions ----------------------------------------------------------------------------------------------- */
static int count_trumps(const uint8_t *h, int n, int kind, int suit)
{
    int k = 0;
    for (int i = 0; i < n; i++) k += sfk_trump(h[i], kind, suit);
    return k;
}

int sfk_auto(const sfpos *p)
{
    const tk4_table *t = &p->t;
    int pl = p->turn, n = t->n[pl];
    const uint8_t *h = t->hand[pl];
    switch (p->phase) {
    case SFP_BID: {
        for (int s = 0; s < 4; s++)
            if (count_trumps(h, n, SFG_SOLO, s) >= 6) return SFA_GAME | SFG(SFG_SOLO, s);
        int aces = 0;
        for (int i = 0; i < n; i++) aces += rank_of(h[i]) == RANK_ACE;
        if (count_trumps(h, n, SFG_WENZ, 0) >= 3 && aces >= 2) return SFA_GAME | SFG(SFG_WENZ, 0);
        if (count_trumps(h, n, SFG_RUF, 0) >= 4)
            for (int s = 0; s < 4; s++)
                if (sfk_game_ok(p, SFG(SFG_RUF, s))) return SFA_GAME | SFG(SFG_RUF, s);
        return SFA_PASS;
    }
    case SFP_PLAY:
        for (int i = 0; i < n; i++)
            if (sfk_can_play(p, h[i])) return SFA_PLAY | h[i];
        return 0;
    case SFP_DEAL: return SFA_NEXT;
    default: return 0;
    }
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int sfk_script(sfgame *g, const char *s)
{
    const sfpos *p = &g->pos;
    for (;;) {
        tk4_space(&s);
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (p->phase != SFP_OVER && (all || p->phase != SFP_DEAL))
                if (!sfk_play(g, sfk_auto(p))) return 0;
            continue;
        }
        char c = *s++;
        if (c == '.') act = sfk_auto(p);
        else if (c == 'p') act = SFA_PASS;
        else if (c == 'N') act = SFA_NEXT;
        else if (c == 'w') act = SFA_GAME | SFG(SFG_WENZ, 0);
        else if (c == 'r' || c == 's') {
            int su = tk4_suit_letter(&s);
            if (su < 0) return 0;
            act = SFA_GAME | SFG(c == 'r' ? SFG_RUF : SFG_SOLO, su);
        } else {
            s--;
            int card = trick_parse(&s);
            if (card == CARD_NONE) return 0;
            act = SFA_PLAY | card;
        }
        if (!sfk_play(g, act)) return 0;
    }
    return 1;
}
