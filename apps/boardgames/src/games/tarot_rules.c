/*
 * French Tarot: the rules (tarot.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/tarot.h"

/* ---- the cards --------------------------------------------------------------------------------------------------- */
static int is_trump(int c) { return CARD_SUIT(c) == SUIT_TRUMP && c != TAROT_EXCUSE; }

int tarot_oudler(int c) { return c == TAROT_EXCUSE || c == TAROT_PETIT || c == TAROT_21; }

int tarot_value2(int c)
{
    if (tarot_oudler(c)) return 9;
    if (CARD_SUIT(c) == SUIT_TRUMP) return 1;
    switch (CARD_RANK(c)) {
    case TAROT_KING: return 9;
    case TAROT_QUEEN: return 7;
    case TAROT_KNIGHT: return 5;
    case TAROT_JACK: return 3;
    default: return 1;
    }
}

int tarot_key(int c)
{
    static const int order[6] = {0, 1, 3, 2, 4, 5};    /* spades, hearts, clubs, diamonds: the colours alternate */
    return c == TAROT_EXCUSE ? 4 * 32 + 31 : order[CARD_SUIT(c)] * 32 + CARD_RANK(c);
}

int tarot_need(int oudlers)
{
    static const int need[4] = {56, 51, 41, 36};
    return need[oudlers < 0 ? 0 : oudlers > 3 ? 3 : oudlers];
}

int tarot_mult(int contract)
{
    static const int m[5] = {0, 1, 2, 4, 6};
    return contract >= TB_PETITE && contract <= TB_CONTRE ? m[contract] : 0;
}

int tarot_poignee_size(int players, int level)
{
    static const int size[3][3] = {{13, 15, 18}, {10, 13, 15}, {8, 10, 13}};
    if (level < 1 || level > 3 || players < 3 || players > 5) return 99;
    return size[players - 3][level - 1];
}

int tarot_poignee_bonus(int level) { return level >= 1 && level <= 3 ? 10 + 10 * level : 0; }

/* ---- hands ------------------------------------------------------------------------------------------------------- */
static int find(const uint8_t *cards, int n, int c)
{
    for (int i = 0; i < n; i++)
        if (cards[i] == c) return i;
    return -1;
}

static void take_out(uint8_t *cards, uint8_t *n, int i)
{
    memmove(cards + i, cards + i + 1, (size_t)(*n - i - 1));
    (*n)--;
}

static void put_in(uint8_t *cards, uint8_t *n, int c)  /* in the hand's order */
{
    int i = *n;
    while (i > 0 && tarot_key(cards[i - 1]) > tarot_key(c)) { cards[i] = cards[i - 1]; i--; }
    cards[i] = (uint8_t)c;
    (*n)++;
}

int tarot_trumps(const tpos *p, int pl)
{
    int n = 0;
    for (int i = 0; i < p->nhand[pl]; i++) n += CARD_SUIT(p->hand[pl][i]) == SUIT_TRUMP;
    return n;
}

int tarot_highest(const tpos *p)
{
    int h = TB_PASS;
    for (int q = 0; q < p->players; q++)
        if (p->bid[q] != TB_NONE && p->bid[q] > h) h = p->bid[q];
    return h;
}

int tarot_call_rank(const tpos *p)
{
    for (int r = TAROT_KING; r > TAROT_JACK; r--) {
        int all = 1;
        for (int s = 0; s < 4; s++) all &= find(p->hand[p->taker], p->nhand[p->taker], CARD(s, r)) >= 0;
        if (!all) return r;
    }
    return TAROT_JACK;
}

int tarot_attack(const tpos *p, int pl)
{
    return p->taker != TAROT_NONE && (pl == p->taker || pl == p->partner);
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static int petit_sec(const tpos *p, int pl)
{
    return tarot_trumps(p, pl) == 1 && find(p->hand[pl], p->nhand[pl], TAROT_PETIT) >= 0;
}

int tarot_cut(uint8_t *pack, uint32_t seed)
{
    uint32_t s = mix(seed, 4242u);
    int k = 4 + (int)(cards_rand(&s) % 71u);   /* more than 3 cards lifted, more than 3 left */
    uint8_t t[78];
    memcpy(t, pack + k, (size_t)(78 - k));     /* the lower part... */
    memcpy(t + 78 - k, pack, (size_t)k);       /* ...on the lifted one */
    memcpy(pack, t, 78);
    return k;
}

/* the deal: from this pack, top first, once cut (NULL: a fresh pack, shuffled: the warm-up) */
static void deal(tpos *p, const uint8_t *from)
{
    uint8_t pack[CARDS_MAX];
    int n = p->players, packets = (78 - p->dogsize) / 3;
    p->cutter = (uint8_t)((p->dealer + n - 1) % n);   /* the dealer's left */
    if (from) {
        memcpy(pack, from, 78);
        p->cut = (uint8_t)tarot_cut(pack, mix(p->seed, 100u + p->dealno));
    } else {
        cards_pack(PACK_TAROT, pack);
        cards_shuffle(pack, 78, mix(p->seed, 100u + p->dealno));
        p->cut = 0;
    }
    uint8_t after[32] = {0};                    /* a dog's card after these packets: never the first nor the last */
    uint32_t s = mix(p->seed, 7777u + p->dealno);
    for (int k = 0; k < p->dogsize;) {
        int i = (int)(cards_rand(&s) % (uint32_t)(packets - 1));
        if (!after[i]) after[i] = 1, k++;
    }
    memset(p->nhand, 0, sizeof p->nhand);
    int c = 0, q = 0, nd = 0;
    for (int i = 0; i < packets; i++) {
        int pl = (p->dealer + 1 + i) % n;
        for (int k = 0; k < 3; k++, c++) {
            p->deal_to[q] = (uint8_t)pl;
            p->deal_card[q++] = pack[c];
            put_in(p->hand[pl], &p->nhand[pl], pack[c]);
        }
        if (after[i]) {
            p->deal_to[q] = TAROT_DOG;
            p->deal_card[q++] = pack[c];
            p->dog[nd++] = pack[c++];
        }
    }
    for (int k = 0; k < n; k++) {
        p->bid[k] = TB_NONE;
        p->poignee[k] = 0;
        p->nshown[k] = 0;
        p->npile[k] = 0;
        p->won[k] = 0;
    }
    p->spoke = 0;
    p->taker = p->partner = p->called = TAROT_NONE;
    p->contract = TB_PASS;
    p->revealed = 0;
    p->necart = 0;
    p->aside_attack = 0;
    p->ntrick = p->tricks = 0;
    p->xowner = p->xto = p->pab_by = TAROT_NONE;
    p->xdebt = 0;
    p->nheap[0] = p->nheap[1] = 0;
    p->sec = TAROT_NONE;
    p->turn = p->leader = (uint8_t)((p->dealer + 1) % n);
    p->phase = TP_BID;
    for (int k = 0; k < n; k++) {
        int pl = (p->dealer + 1 + k) % n;
        if (petit_sec(p, pl)) { p->sec = (uint8_t)pl; p->phase = TP_VOID; break; }
    }
}

static void begin(tgame *g, int players, uint32_t seed, const uint8_t *pack)
{
    uint8_t from[78];
    if (pack) memcpy(from, pack, 78);           /* it may be g->start (undo) */
    memset(g, 0, sizeof *g);
    tpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players < 3 ? 3 : players > TAROT_MAXP ? TAROT_MAXP : players);
    p->deals = p->players;
    p->dogsize = p->players == 5 ? 3 : 6;
    p->handsize = (uint8_t)((78 - p->dogsize) / p->players);
    p->dealer = (uint8_t)(p->players - 1);      /* player 0 bids and leads first */
    if (pack) memcpy(g->start, from, 78);
    deal(p, pack ? g->start : NULL);
}

void tarot_start(tgame *g, int players, uint32_t seed, const uint8_t *pack)
{
    uint8_t ok[78];                             /* a pack of the 78 cards, once each, or a fresh one */
    int seen[256] = {0}, good = pack != NULL;
    cards_pack(PACK_TAROT, ok);
    for (int i = 0; good && i < 78; i++) good = ++seen[pack[i]] == 1;
    for (int i = 0; good && i < 78; i++) good = seen[ok[i]] == 1;
    begin(g, players, seed, good ? pack : ok);
}

void tarot_new(tgame *g, int players, uint32_t seed)
{
    uint8_t pack[78];
    tarot_warmup(pack, players, seed);
    tarot_start(g, players, seed, pack);
}

/* ---- the play ---------------------------------------------------------------------------------------------------- */
/* the suit to follow: the first card that is not the Excuse (-1: none yet) */
static int led_suit(const uint8_t *cards, int n)
{
    for (int i = 0; i < n; i++)
        if (cards[i] != TAROT_EXCUSE) return CARD_SUIT(cards[i]);
    return -1;
}

static int top_trump(const uint8_t *cards, int n)
{
    int t = 0;
    for (int i = 0; i < n; i++)
        if (is_trump(cards[i]) && CARD_RANK(cards[i]) > t) t = CARD_RANK(cards[i]);
    return t;
}

int tarot_trick_winner(const tpos *p, const uint8_t *cards, int n, int leader)
{
    int suit = led_suit(cards, n), best = -1, bv = -1;
    for (int i = 0; i < n; i++) {
        int c = cards[i], v;
        if (c == TAROT_EXCUSE) continue;
        if (is_trump(c)) v = 100 + CARD_RANK(c);
        else if (CARD_SUIT(c) == suit) v = CARD_RANK(c);
        else continue;
        if (v > bv) bv = v, best = i;
    }
    return best < 0 ? leader : (leader + best) % p->players;
}

static int can_play(const tpos *p, int c)
{
    int pl = p->turn;
    const uint8_t *h = p->hand[pl];
    int nh = p->nhand[pl];
    if (find(h, nh, c) < 0) return 0;
    if (c == TAROT_EXCUSE) return 1;
    int suit = led_suit(p->trick, p->ntrick);
    if (suit < 0) return 1;
    int has_suit = 0, has_trump = 0, has_over = 0, top = top_trump(p->trick, p->ntrick);
    for (int i = 0; i < nh; i++) {
        if (h[i] == TAROT_EXCUSE) continue;
        if (suit != SUIT_TRUMP && CARD_SUIT(h[i]) == suit) has_suit = 1;
        if (is_trump(h[i])) {
            has_trump = 1;
            if (CARD_RANK(h[i]) > top) has_over = 1;
        }
    }
    if (has_suit) return CARD_SUIT(c) == suit;
    if (has_trump) return is_trump(c) && (!has_over || CARD_RANK(c) > top);
    return 1;
}

/* the trumps a discard may hold: none while the player has enough plain cards (no king) to discard */
static int plain(int c) { return CARD_SUIT(c) != SUIT_TRUMP && CARD_RANK(c) != TAROT_KING; }

static int discard_ok(const tpos *p, int c)
{
    int pl = p->taker;
    if (find(p->hand[pl], p->nhand[pl], c) < 0 || p->necart >= p->dogsize) return 0;
    if (tarot_oudler(c) || (CARD_SUIT(c) != SUIT_TRUMP && CARD_RANK(c) == TAROT_KING)) return 0;
    if (!is_trump(c)) return 1;
    int np = 0, nt = 0;
    for (int i = 0; i < p->nhand[pl]; i++) np += plain(p->hand[pl][i]);
    for (int i = 0; i < p->necart; i++) np += plain(p->ecart[i]), nt += is_trump(p->ecart[i]);
    return np < p->dogsize && nt < p->dogsize - np;
}

int tarot_legal(const tpos *p, int act)
{
    int k = TA_KIND(act), a = TA_ARG(act);
    switch (p->phase) {
    case TP_BID: return k == TA_BID && a <= TB_CONTRE && (a == TB_PASS || a > tarot_highest(p));
    case TP_CALL:
        return k == TA_CALL && CARD_SUIT(a) < 4 && CARD_RANK(a) == tarot_call_rank(p);
    case TP_DOG: return k == TA_TAKE;
    case TP_DISCARD:
        if (k == TA_DISCARD) return discard_ok(p, a);
        if (k == TA_RETURN) return find(p->ecart, p->necart, a) >= 0;
        return k == TA_DONE && p->necart == p->dogsize;
    case TP_PLAY:
        if (k == TA_POIGNEE)
            return p->tricks == 0 && !p->poignee[p->turn] && a >= 1 && a <= 3 &&
                   tarot_trumps(p, p->turn) >= tarot_poignee_size(p->players, a);
        return k == TA_PLAY && can_play(p, a);
    case TP_SCORE:
    case TP_VOID: return k == TA_NEXT;
    default: return 0;
    }
}

static void start_play(tpos *p)
{
    p->phase = TP_PLAY;
    p->turn = p->leader = (uint8_t)((p->dealer + 1) % p->players);
    p->ntrick = p->tricks = 0;
    int s = p->aside_attack != 0;               /* the aside, at the bottom of its side's pile */
    for (int i = 0; i < p->necart; i++) p->heap[s][p->nheap[s]++] = p->ecart[i];
}

/* after the bidding (and the call): the dog turned up, or put aside unseen */
static void to_dog(tpos *p)
{
    p->turn = p->taker;
    if (p->contract <= TB_GARDE) { p->phase = TP_DOG; return; }
    memcpy(p->ecart, p->dog, p->dogsize);
    p->necart = p->dogsize;
    p->aside_attack = p->contract == TB_SANS;
    start_play(p);
}

static int oud_bit(int c) { return c == TAROT_PETIT ? 1 : c == TAROT_21 ? 2 : c == TAROT_EXCUSE ? 4 : 0; }

static void score(tpos *p)
{
    tresult *r = &p->res;
    memset(r, 0, sizeof *r);
    int half = 0, aw = 0;
    for (int q = 0; q < p->players; q++) {
        if (!tarot_attack(p, q)) continue;
        aw += p->won[q];
        for (int i = 0; i < p->npile[q]; i++) {
            int c = p->pile[q][i];
            half += tarot_value2(c);
            r->oud |= (uint8_t)oud_bit(c);
        }
    }
    if (p->aside_attack)
        for (int i = 0; i < p->necart; i++) {
            half += tarot_value2(p->ecart[i]);
            r->oud |= (uint8_t)oud_bit(p->ecart[i]);
        }
    if (p->xowner != TAROT_NONE) half += tarot_attack(p, p->xowner) ? -1 : 1;   /* the Excuse's half point */
    r->half = (int16_t)half;
    r->oudlers = (uint8_t)((r->oud & 1) + (r->oud >> 1 & 1) + (r->oud >> 2 & 1));
    r->need = (uint8_t)tarot_need(r->oudlers);
    r->mult = (uint8_t)tarot_mult(p->contract);
    int d2 = half - 2 * r->need, sign;
    r->made = d2 >= 0;
    sign = r->made ? 1 : -1;
    r->diff = (int16_t)((abs(d2) + 1) / 2);
    if (p->pab_by != TAROT_NONE) r->pab = tarot_attack(p, p->pab_by) ? 10 : -10;
    int pg = 0;
    for (int q = 0; q < p->players; q++) pg += tarot_poignee_bonus(p->poignee[q]);
    r->poignee = (int16_t)(sign * pg);
    r->chelem = aw == p->handsize ? 200 : aw == 0 ? -200 : 0;
    int unit = sign * (25 + r->diff) * r->mult + r->pab * r->mult + r->poignee + r->chelem;
    r->unit = (int16_t)unit;
    int alone = p->partner == p->taker || p->partner == TAROT_NONE;
    for (int q = 0; q < p->players; q++) {
        int s = -unit;
        if (q == p->taker) s = alone ? unit * (p->players - 1) : 2 * unit;
        else if (q == p->partner) s = unit;
        p->score[q] = (int16_t)s;
        p->total[q] = (int16_t)(p->total[q] + s);
    }
    p->played++;
    p->phase = p->played >= p->deals ? TP_OVER : TP_SCORE;
}

/* the sides' piles as the cards lie on the table (tarot_gather) */
static void to_heap(tpos *p, int pl, int c)
{
    int s = tarot_attack(p, pl);
    p->heap[s][p->nheap[s]++] = (uint8_t)c;
}

/* the Excuse's small card: from the top of its owner's side's pile onto the other side's, once there is one */
static void settle(tpos *p)
{
    if (!p->xdebt) return;
    int s = tarot_attack(p, p->xowner);
    for (int i = p->nheap[s] - 1; i >= 0; i--) {
        int c = p->heap[s][i];
        if (tarot_value2(c) != 1) continue;
        take_out(p->heap[s], &p->nheap[s], i);
        p->heap[!s][p->nheap[!s]++] = (uint8_t)c;
        p->xdebt = 0;
        return;
    }
}

/* the trick is complete: who wins it, where its cards go */
static void end_trick(tpos *p)
{
    int n = p->players, last = p->tricks == p->handsize - 1, win = tarot_trick_winner(p, p->trick, n, p->leader);
    int xi = find(p->trick, n, TAROT_EXCUSE), owner = xi < 0 ? -1 : (p->leader + xi) % n;
    if (owner >= 0 && last) {                   /* the last trick: the Excuse wins it after a clean sweep, else it
                                                 * goes to the trick's winner */
        int camp = tarot_attack(p, owner), all = 0;
        for (int q = 0; q < n; q++)
            if (tarot_attack(p, q) == camp) all += p->won[q];
        if (p->tricks > 0 && all == p->tricks) win = owner;
        owner = -1;
    }
    for (int i = 0; i < n; i++) {
        int c = p->trick[i];
        if (i == xi && owner >= 0) {            /* the Excuse stays with its owner, a half point owed */
            p->pile[owner][p->npile[owner]++] = (uint8_t)c;
            to_heap(p, owner, c);
            if (tarot_attack(p, owner) != tarot_attack(p, win))
                p->xowner = (uint8_t)owner, p->xto = (uint8_t)win, p->xdebt = 1;
            continue;
        }
        p->pile[win][p->npile[win]++] = (uint8_t)c;
        to_heap(p, win, c);
        if (c == TAROT_PETIT && last) p->pab_by = (uint8_t)win;
    }
    settle(p);
    p->won[win]++;
    memcpy(p->last, p->trick, (size_t)n);
    p->lastlead = p->leader;
    p->lastwin = (uint8_t)win;
    p->tricks++;
    p->ntrick = 0;
    p->leader = p->turn = (uint8_t)win;
}

static void apply(tpos *p, int act)
{
    int k = TA_KIND(act), a = TA_ARG(act), pl = p->turn, n = p->players;
    switch (k) {
    case TA_BID:
        p->bid[pl] = (uint8_t)a;
        p->turn = (uint8_t)((pl + 1) % n);
        if (++p->spoke < n) break;
        p->contract = (uint8_t)tarot_highest(p);
        p->turn = (uint8_t)pl;
        if (p->contract == TB_PASS) { p->phase = TP_VOID; break; }   /* everyone passed: the deal is cancelled */
        for (int q = 0; q < n; q++)
            if (p->bid[q] == p->contract) p->taker = (uint8_t)q;
        p->partner = p->taker;
        p->turn = p->taker;
        if (n == 5) p->phase = TP_CALL;
        else to_dog(p);
        break;
    case TA_CALL:
        p->called = (uint8_t)a;
        for (int q = 0; q < n; q++)
            if (q != p->taker && find(p->hand[q], p->nhand[q], a) >= 0) p->partner = (uint8_t)q;
        to_dog(p);
        break;
    case TA_TAKE:
        for (int i = 0; i < p->dogsize; i++) put_in(p->hand[pl], &p->nhand[pl], p->dog[i]);
        p->necart = 0;
        p->aside_attack = 1;
        p->phase = TP_DISCARD;
        break;
    case TA_DISCARD:
        take_out(p->hand[pl], &p->nhand[pl], find(p->hand[pl], p->nhand[pl], a));
        p->ecart[p->necart++] = (uint8_t)a;
        break;
    case TA_RETURN:
        take_out(p->ecart, &p->necart, find(p->ecart, p->necart, a));
        put_in(p->hand[pl], &p->nhand[pl], a);
        break;
    case TA_DONE:
        start_play(p);
        break;
    case TA_POIGNEE: {
        int need = tarot_poignee_size(n, a), m = 0;
        p->poignee[pl] = (uint8_t)a;
        for (int i = 0; i < p->nhand[pl] && m < need; i++)   /* the lowest trumps... */
            if (is_trump(p->hand[pl][i])) p->shown[pl][m++] = p->hand[pl][i];
        if (m < need) p->shown[pl][m++] = TAROT_EXCUSE;      /* ...the Excuse only when it completes them */
        p->nshown[pl] = (uint8_t)m;
        break;
    }
    case TA_PLAY:
        take_out(p->hand[pl], &p->nhand[pl], find(p->hand[pl], p->nhand[pl], a));
        p->trick[p->ntrick++] = (uint8_t)a;
        if (a == p->called) p->revealed = 1;
        p->turn = (uint8_t)((pl + 1) % n);
        if (p->ntrick < n) break;
        end_trick(p);
        if (p->tricks == p->handsize) {
            score(p);
            p->turn = (uint8_t)pl;              /* the count shows to whoever played last: no hand-off */
        }
        break;
    case TA_NEXT: {
        uint8_t pack[78];
        tarot_gather(p, pack);                  /* no shuffle: gathered, cut, dealt */
        p->dealer = (uint8_t)((p->dealer + 1) % n);
        p->dealno++;
        deal(p, pack);
        break;
    }
    }
}

int tarot_play(tgame *g, int act)
{
    if (g->n >= TAROT_MAX_ACTS || !tarot_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int tarot_undo(tgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint16_t acts[TAROT_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    begin(g, g->players, g->seed, g->start);
    for (int i = 0; i < n; i++) tarot_play(g, acts[i]);
    return 1;
}

/* ---- the pack between deals -------------------------------------------------------------------------------------- */
static void push(uint8_t *b, int *n, const uint8_t *c, int k)
{
    for (int i = 0; i < k && *n < 78; i++) b[(*n)++] = c[i];
}

void tarot_gather(const tpos *p, uint8_t *pack)
{
    uint8_t b[78];                              /* bottom first */
    int n = 0, ph = p->phase, played = ph == TP_PLAY || ph == TP_SCORE || ph == TP_OVER;
    if (played) {
        push(b, &n, p->heap[1], p->nheap[1]);   /* the taker's side's pile, the defence's on it */
        push(b, &n, p->heap[0], p->nheap[0]);
    }
    if (ph == TP_PLAY) push(b, &n, p->trick, p->ntrick);
    for (int k = 0; k < p->players; k++) {      /* the hands as held, the dealer's next first */
        int pl = (p->dealer + 1 + k) % p->players;
        push(b, &n, p->hand[pl], p->nhand[pl]);
    }
    if (ph == TP_DISCARD) push(b, &n, p->ecart, p->necart);
    else if (!played) push(b, &n, p->dog, p->dogsize);
    int seen[256] = {0}, good = n == 78;
    for (int i = 0; good && i < 78; i++) good = ++seen[b[i]] == 1;
    if (!good) {                                /* never: a fresh pack rather than a wrong one */
        cards_pack(PACK_TAROT, pack);
        return;
    }
    for (int i = 0; i < 78; i++) pack[i] = b[77 - i];
}

/* the session's first pack: a whole deal from a shuffled pack, played by legal random choices, never seen */
int tarot_warmup(uint8_t *pack, int players, uint32_t seed)
{
    static tgame w;
    for (uint32_t k = 0;; k++) {                /* a deal that is not cancelled (no Petit sec) */
        begin(&w, players, mix(seed, 9000u + k), NULL);
        if (w.pos.phase == TP_BID) break;
    }
    tpos *p = &w.pos;
    uint32_t s = mix(seed, 31337u);
    for (int steps = 0; p->phase != TP_SCORE && p->phase != TP_OVER && steps < 1000; steps++) {
        int pl = p->turn, act = 0, cand[TAROT_HAND], nc = 0;
        switch (p->phase) {
        case TP_BID: act = TACT(TA_BID, p->spoke == 0 ? TB_PETITE : TB_PASS); break;
        case TP_CALL: act = TACT(TA_CALL, CARD(cards_rand(&s) % 4u, tarot_call_rank(p))); break;
        case TP_DOG: act = TACT(TA_TAKE, 0); break;
        case TP_DISCARD:
            if (p->necart == p->dogsize) { act = TACT(TA_DONE, 0); break; }
            for (int i = 0; i < p->nhand[pl]; i++)
                if (discard_ok(p, p->hand[pl][i])) cand[nc++] = TACT(TA_DISCARD, p->hand[pl][i]);
            break;
        case TP_PLAY:
            for (int i = 0; i < p->nhand[pl]; i++)
                if (can_play(p, p->hand[pl][i])) cand[nc++] = TACT(TA_PLAY, p->hand[pl][i]);
            break;
        }
        if (nc) act = cand[cards_rand(&s) % (uint32_t)nc];
        if (!tarot_play(&w, act)) break;
    }
    tarot_gather(p, pack);
    return p->phase == TP_SCORE || p->phase == TP_OVER;
}

int tarot_winner(const tpos *p)
{
    if (p->phase != TP_OVER) return -1;
    int best = 0, tie = 0;
    for (int q = 1; q < p->players; q++) {
        if (p->total[q] > p->total[best]) best = q, tie = 0;
        else if (p->total[q] == p->total[best]) tie = 1;
    }
    return tie ? -1 : best;
}

/* ---- plain actions, the notation --------------------------------------------------------------------------------- */
int tarot_auto(const tpos *p)
{
    int pl = p->turn;
    switch (p->phase) {
    case TP_BID: return TACT(TA_BID, tarot_highest(p) == TB_PASS ? TB_PETITE : TB_PASS);
    case TP_CALL:
        for (int s = 0; s < 4; s++)             /* a king they do not hold */
            if (find(p->hand[pl], p->nhand[pl], CARD(s, tarot_call_rank(p))) < 0)
                return TACT(TA_CALL, CARD(s, tarot_call_rank(p)));
        return TACT(TA_CALL, CARD(0, tarot_call_rank(p)));
    case TP_DOG: return TACT(TA_TAKE, 0);
    case TP_DISCARD:
        if (p->necart == p->dogsize) return TACT(TA_DONE, 0);
        for (int i = 0; i < p->nhand[pl]; i++)
            if (discard_ok(p, p->hand[pl][i])) return TACT(TA_DISCARD, p->hand[pl][i]);
        return 0;
    case TP_PLAY:
        for (int i = 0; i < p->nhand[pl]; i++)
            if (can_play(p, p->hand[pl][i])) return TACT(TA_PLAY, p->hand[pl][i]);
        return 0;
    case TP_SCORE:
    case TP_VOID: return TACT(TA_NEXT, 0);
    default: return 0;
    }
}

static int number(const char **s)
{
    int v = -1;
    while (**s >= '0' && **s <= '9') v = (v < 0 ? 0 : v * 10) + (*(*s)++ - '0');
    return v;
}

int tarot_parse_card(const char **s)
{
    char c = *(*s)++;
    if (c == 'e') return TAROT_EXCUSE;
    int r = number(s);
    if (c == 'a') return r >= 1 && r <= 21 ? CARD(SUIT_TRUMP, r) : CARD_NONE;
    const char *suits = "shdc", *f = c ? strchr(suits, c) : NULL;
    if (!f || r < 1 || r > 14) return CARD_NONE;
    static const int suit[4] = {SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS, SUIT_CLUBS};
    return CARD(suit[f - suits], r);
}

int tarot_script(tgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act = 0, c, v;
        switch (*s) {
        case '#':                               /* plain actions to the end of the deal */
            s++;
            while (g->pos.phase != TP_SCORE && g->pos.phase != TP_OVER)
                if (!tarot_play(g, tarot_auto(&g->pos))) return 0;
            continue;
        case '.':
            s++;
            v = number(&s);
            for (int i = 0; i < (v < 0 ? 1 : v); i++)
                if (!tarot_play(g, tarot_auto(&g->pos))) return 0;
            continue;
        case 'p': s++; act = TACT(TA_BID, TB_PASS); break;
        case 'b': s++; v = number(&s); act = v >= 1 && v <= 4 ? TACT(TA_BID, v) : 0; break;
        case 'g': s++; v = number(&s); act = v >= 1 && v <= 3 ? TACT(TA_POIGNEE, v) : 0; break;
        case 't': s++; act = TACT(TA_TAKE, 0); break;
        case 'f': s++; act = TACT(TA_DONE, 0); break;
        case 'n': s++; act = TACT(TA_NEXT, 0); break;
        case 'k':
        case 'x':
        case 'r': {
            int kind = *s == 'k' ? TA_CALL : *s == 'x' ? TA_DISCARD : TA_RETURN;
            s++;
            c = tarot_parse_card(&s);
            act = c == CARD_NONE ? 0 : TACT(kind, c);
            break;
        }
        default:
            c = tarot_parse_card(&s);
            act = c == CARD_NONE ? 0 : TACT(TA_PLAY, c);
        }
        if (!act || !tarot_play(g, act)) return 0;
    }
    return 1;
}
