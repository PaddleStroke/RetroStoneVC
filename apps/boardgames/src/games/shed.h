/*
 * The climbing (shedding) games' rules, shared by Big Two and Tiến lên: no SDL, static inline. A deal of the 52 cards,
 * hands sorted by the game's order of the cards, the first play holding the lowest card dealt, tricks where each in
 * turn beats the last play with a play of its kind or passes, the players going out, the history replayed for undo,
 * the notation. A game brings its order of the cards and of the suits, its combinations, what beats what, whether a
 * pass leaves the trick for good, and when the deal ends (shed_rules).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Cards are cards.h's bytes (DECK_FRENCH). A play is a set of cards, a mask (bit shed_bit(card)): its cards in the
 * game's order from low to high (shed_cards). Notation (shed_script): a play's cards in a row ("3d", "7s7h", "3d4c5h6s7d";
 * ranks 2..9, T, J, Q, K, A; suits s h d c), p a pass, "." a plain action, "#" plain actions to the end of the deal.
 */
#ifndef SHED_H
#define SHED_H

#include <stdint.h>
#include <string.h>

#include "cards.h"
#include "games/tricks.h"

#define SHED_MAXP 4
#define SHED_HAND 18                            /* Big Two for three: 17 each, and the extra card */
#define SHED_MAX_ACTS 1024
#define SHED_NONE 255
#define SHED_PASS (1ull << 62)

enum { SP_PLAY, SP_OVER };

/* a combination: its kind (the game's own numbering, 0: none), its size, its strength within its kind (key) */
typedef struct { uint8_t kind, n; uint32_t key; } shed_combo;

typedef struct {
    int (*power)(int card);                     /* the card's place in the game's order, 0 (lowest) .. 51 */
    /* the combination n cards make (sorted low to high); 0: none */
    int (*classify)(const uint8_t *cards, int n, shed_combo *c);
    int (*beats)(const shed_combo *a, const shed_combo *top);   /* a may be played on top */
    int lockout;                                /* a player who passes is out of the trick until the next lead */
    int first_out_ends;                         /* the deal ends as soon as a player is out */
    int deal_all;                               /* all 52 dealt round (the odd card to the lowest card's holder);
                                                 * else 13 each, the rest unused */
} shed_rules;

typedef struct {
    uint8_t players, phase, turn;
    uint8_t hand[SHED_MAXP][SHED_HAND], n[SHED_MAXP], dealt[SHED_MAXP];
    uint8_t out[SHED_MAXP], nout;               /* the players out of cards, in order (then the rest, at the end) */
    uint8_t must;                               /* the card the first play must hold (SHED_NONE: free) */
    shed_combo top;                             /* the play to beat (n 0: a lead) */
    uint8_t top_player, passed, passes;         /* bit q: q passed since the last play (lockout: out of the trick) */
    uint8_t pile[52], npile, plays[52], who[52], nplays;   /* the trick: its cards in order, each play's size, player */
    uint8_t tricks, played;                     /* tricks over; cards played in them (gone face down) */
    uint32_t seed;
} shed_pos;

typedef struct {
    shed_pos pos;
    uint32_t seed;
    uint8_t players;
    uint64_t act[SHED_MAX_ACTS];
    int n;
} shed_game;

static inline uint64_t shed_bit(int card) { return 1ull << (CARD_SUIT(card) * 13 + CARD_RANK(card) - 1); }
static inline int shed_rank_index(int card) { return (CARD_RANK(card) + 10) % 13; }   /* 3 0 .. king 10, ace 11, 2 12 */

static inline void shed_sort(const shed_rules *R, uint8_t *c, int n)
{
    for (int i = 1; i < n; i++) {
        uint8_t x = c[i];
        int j = i;
        while (j > 0 && R->power(c[j - 1]) > R->power(x)) { c[j] = c[j - 1]; j--; }
        c[j] = x;
    }
}

/* a mask's cards, low to high; how many */
static inline int shed_cards(const shed_rules *R, uint64_t m, uint8_t *out)
{
    int n = 0;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (m & shed_bit(CARD(s, r))) out[n++] = (uint8_t)CARD(s, r);
    shed_sort(R, out, n);
    return n;
}

static inline int shed_find(const shed_pos *p, int pl, int card)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] == card) return i;
    return -1;
}

static inline int shed_is_out(const shed_pos *p, int pl)
{
    for (int i = 0; i < p->nout; i++)
        if (p->out[i] == pl) return 1;
    return 0;
}

static inline int shed_place(const shed_pos *p, int pl)   /* 0 first out; -1: still in */
{
    for (int i = 0; i < p->nout; i++)
        if (p->out[i] == pl) return i;
    return -1;
}

static inline int shed_active(const shed_pos *p, int pl) { return p->n[pl] > 0 && !shed_is_out(p, pl); }

/* still in this trick: holding cards, and (lockout) not passed since the lead */
static inline int shed_in_trick(const shed_rules *R, const shed_pos *p, int pl)
{
    return shed_active(p, pl) && !(R->lockout && p->passed >> pl & 1);
}

static inline int shed_count_active(const shed_pos *p)
{
    int k = 0;
    for (int q = 0; q < p->players; q++) k += shed_active(p, q);
    return k;
}

static inline void shed_new(shed_game *g, const shed_rules *R, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    shed_pos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > SHED_MAXP ? SHED_MAXP : players);
    uint8_t pack[CARDS_MAX];
    int n = cards_pack(PACK_52, pack), each = R->deal_all ? n / p->players : 13, k = 0;
    cards_shuffle(pack, n, seed);
    for (int r = 0; r < each; r++)
        for (int q = 0; q < p->players; q++) p->hand[q][p->n[q]++] = pack[k++];
    int low = 99, holder = 0;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++)
            if (R->power(p->hand[q][i]) < low) low = R->power(p->hand[q][i]), holder = q, p->must = p->hand[q][i];
    if (R->deal_all)                            /* the odd cards: to the lowest card's holder */
        while (k < n) {
            if (R->power(pack[k]) < low) low = R->power(pack[k]), p->must = pack[k];   /* lower still: it leads */
            p->hand[holder][p->n[holder]++] = pack[k++];
        }
    for (int q = 0; q < p->players; q++) {
        shed_sort(R, p->hand[q], p->n[q]);
        p->dealt[q] = p->n[q];
    }
    p->turn = (uint8_t)holder;
    p->top_player = SHED_NONE;
    p->phase = SP_PLAY;
}

/* the combination a mask makes (0: none) */
static inline int shed_combo_of(const shed_rules *R, uint64_t m, shed_combo *c)
{
    uint8_t cards[52];
    int n = shed_cards(R, m, cards);
    memset(c, 0, sizeof *c);
    return n > 0 && n <= 13 && R->classify(cards, n, c);
}

static inline int shed_legal(const shed_rules *R, const shed_pos *p, uint64_t a)
{
    if (p->phase != SP_PLAY) return 0;
    if (a == SHED_PASS) return p->top.n > 0;
    if (!a || a >> 52) return 0;
    uint8_t cards[52];
    int n = shed_cards(R, a, cards);
    for (int i = 0; i < n; i++)
        if (shed_find(p, p->turn, cards[i]) < 0) return 0;
    if (p->must != SHED_NONE && !(a & shed_bit(p->must))) return 0;
    shed_combo c;
    if (!shed_combo_of(R, a, &c)) return 0;
    return !p->top.n || R->beats(&c, &p->top);
}

/* the next player in turn after pl still in the trick; -1: none */
static inline int shed_next_in(const shed_rules *R, const shed_pos *p, int pl)
{
    for (int i = 1; i < p->players; i++) {
        int q = (pl + i) % p->players;
        if (shed_in_trick(R, p, q)) return q;
    }
    return -1;
}

static inline int shed_next_active(const shed_pos *p, int pl)
{
    for (int i = 1; i <= p->players; i++) {
        int q = (pl + i) % p->players;
        if (shed_active(p, q)) return q;
    }
    return pl;
}

/* the trick is over: its last player leads (out of cards: the next one still holding some) */
static inline void shed_trick_end(shed_pos *p)
{
    int lead = p->top_player;
    p->played = (uint8_t)(p->played + p->npile);
    p->npile = p->nplays = 0;
    memset(&p->top, 0, sizeof p->top);
    p->top_player = SHED_NONE;
    p->passed = p->passes = 0;
    p->tricks++;
    p->turn = (uint8_t)(shed_active(p, lead) ? lead : shed_next_active(p, lead));
}

static inline void shed_deal_end(shed_pos *p)
{
    for (int q = 0; q < p->players; q++)        /* the rest, in turn from the last one out */
        if (!shed_is_out(p, q)) p->out[p->nout++] = (uint8_t)q;
    p->phase = SP_OVER;
}

static inline void shed_apply(const shed_rules *R, shed_pos *p, uint64_t a)
{
    int pl = p->turn;
    if (a == SHED_PASS) {
        p->passed |= (uint8_t)(1u << pl);
        p->passes++;
        int next = shed_next_in(R, p, pl);
        if (R->lockout) {                       /* over once nobody but the last player is left in it */
            if (next < 0 || next == p->top_player) shed_trick_end(p);
            else p->turn = (uint8_t)next;
        } else {
            int need = shed_count_active(p) - shed_active(p, p->top_player);
            if (p->passes >= need) shed_trick_end(p);
            else p->turn = (uint8_t)next;
        }
        return;
    }
    uint8_t cards[52];
    int n = shed_cards(R, a, cards);
    shed_combo_of(R, a, &p->top);
    for (int i = 0; i < n; i++) {
        int k = shed_find(p, pl, cards[i]);
        memmove(&p->hand[pl][k], &p->hand[pl][k + 1], (size_t)(p->n[pl] - k - 1));
        p->n[pl]--;
        p->pile[p->npile++] = cards[i];
    }
    p->who[p->nplays] = (uint8_t)pl;
    p->plays[p->nplays++] = (uint8_t)n;
    p->top_player = (uint8_t)pl;
    p->must = SHED_NONE;
    p->passes = 0;
    if (!R->lockout) p->passed = 0;
    if (!p->n[pl]) {
        p->out[p->nout++] = (uint8_t)pl;
        if (R->first_out_ends || shed_count_active(p) <= 1) { shed_deal_end(p); return; }
    }
    int next = shed_next_in(R, p, pl);
    if (next < 0) shed_trick_end(p);            /* nobody left to beat it */
    else p->turn = (uint8_t)next;
}

static inline int shed_play(shed_game *g, const shed_rules *R, uint64_t a)
{
    if (g->n >= SHED_MAX_ACTS || !shed_legal(R, &g->pos, a)) return 0;
    shed_apply(R, &g->pos, a);
    g->act[g->n++] = a;
    return 1;
}

static inline int shed_undo(shed_game *g, const shed_rules *R)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint64_t acts[SHED_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    shed_new(g, R, g->players, g->seed);
    for (int i = 0; i < n; i++) shed_play(g, R, acts[i]);
    return 1;
}

/* the first legal play of k cards of the hand, in the hand's order (the lowest first); 0: none */
static inline uint64_t shed_search(const shed_rules *R, const shed_pos *p, int k)
{
    int pl = p->turn, n = p->n[pl], idx[13];
    if (k < 1 || k > 13 || k > n) return 0;
    for (int i = 0; i < k; i++) idx[i] = i;
    for (;;) {
        uint64_t m = 0;
        for (int i = 0; i < k; i++) m |= shed_bit(p->hand[pl][idx[i]]);
        if (shed_legal(R, p, m)) return m;
        int i = k - 1;
        while (i >= 0 && idx[i] == n - k + i) i--;
        if (i < 0) return 0;
        idx[i]++;
        for (int j = i + 1; j < k; j++) idx[j] = idx[j - 1] + 1;
    }
}

/* a plain action: lead the lowest card (the one the first play must hold), else the lowest play of the same size
 * that beats the last one, else pass */
static inline uint64_t shed_auto(const shed_rules *R, const shed_pos *p)
{
    if (p->phase != SP_PLAY) return 0;
    if (!p->top.n) return shed_bit(p->must != SHED_NONE ? p->must : p->hand[p->turn][0]);
    uint64_t m = shed_search(R, p, p->top.n);
    return m ? m : SHED_PASS;
}

static inline int shed_script(shed_game *g, const shed_rules *R, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        uint64_t a = 0;
        if (*s == '#') {
            s++;
            while (g->pos.phase == SP_PLAY)
                if (!shed_play(g, R, shed_auto(R, &g->pos))) return 0;
            continue;
        }
        if (*s == '.') s++, a = shed_auto(R, &g->pos);
        else if (*s == 'p') s++, a = SHED_PASS;
        else
            for (int c; (c = trick_parse(&s)) != CARD_NONE;) a |= shed_bit(c);
        if (!a || !shed_play(g, R, a)) return 0;
    }
    return 1;
}

#endif
