/*
 * The four-handed trick games of Central Europe and the Low Countries (Doppelkopf, Schafkopf, Jass, Klaverjas): the
 * rules they share. No SDL; static inline, each game takes what it uses (its tests link only its own rules). The
 * drawing side is games/trick4_ui.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Four players round the table, play going from player to player + 1; partners (when they are fixed) sit across:
 * players 0 and 2 against 1 and 3. The cards are cards.h's bytes (DECK_FRENCH, ace 1 .. king 13), a double pack
 * holding each card twice (two equal bytes). A trick: the cards in the order played from its leader; the strongest
 * wins it, the first of two equal cards beating the second.
 * Notation: tricks.h's (a rank 2..9, T, J, Q, K, A and a suit s, h, d, c: "Qc", "Th").
 */
#ifndef TRICK4_H
#define TRICK4_H

#include <stdint.h>
#include <string.h>

#include "cards.h"
#include "games/tricks.h"

#define TK4_P 4
#define TK4_HAND 12
#define TK4_NONE 255

/* the cards in play: the hands, the trick, the last trick, what each player has taken */
typedef struct {
    uint8_t hand[TK4_P][TK4_HAND], n[TK4_P];
    uint8_t leader, trick[TK4_P], ntrick, tricks;   /* trick[i]: played by (leader + i) % 4 */
    uint8_t last[TK4_P], last_leader, last_winner;  /* the trick before (last_winner TK4_NONE: none yet) */
    uint8_t won[TK4_P];                             /* tricks taken by each player */
    int16_t eyes[TK4_P];                            /* card points taken by each player */
} tk4_table;

static inline int tk4_next(int pl) { return (pl + 1) % TK4_P; }
static inline int tk4_partner(int pl) { return (pl + 2) % TK4_P; }
static inline int tk4_team(int pl) { return pl & 1; }

/* a deal: the pack (top first) given out in packets (sizes in packets[], 0 ends the list; each packet to every player
 * in turn from `first`), the hands left unsorted; nothing played yet, `first` leads */
static inline void tk4_deal(tk4_table *t, const uint8_t *pack, const int *packets, int first)
{
    memset(t, 0, sizeof *t);
    int k = 0;
    for (int r = 0; packets[r]; r++)
        for (int q = 0; q < TK4_P; q++) {
            int pl = (first + q) % TK4_P;
            for (int c = 0; c < packets[r] && t->n[pl] < TK4_HAND; c++) t->hand[pl][t->n[pl]++] = pack[k++];
        }
    t->leader = (uint8_t)first;
    t->last_leader = t->last_winner = TK4_NONE;
    memset(t->last, CARD_NONE, sizeof t->last);
}

static inline int tk4_turn(const tk4_table *t) { return (t->leader + t->ntrick) % TK4_P; }

static inline int tk4_holds(const tk4_table *t, int pl, int card) { return trick_find(t->hand[pl], t->n[pl], card) >= 0; }

/* how many cards of a hand pass a test (the game's: a suit as followed, a trump...) */
static inline int tk4_count(const uint8_t *h, int n, int (*test)(int card, int arg), int arg)
{
    int k = 0;
    for (int i = 0; i < n; i++) k += test(h[i], arg) != 0;
    return k;
}

/* the card that wins: its index in the order played. suit[i]: the suit card i counts in (SUIT_TRUMP for a trump),
 * power[i] its strength there; a trump beats any other suit, a card of another suit than the led one never wins, and
 * of two equal cards the first stays. */
static inline int tk4_best(const int *suit, const int *power, int n)
{
    int best = 0;
    for (int i = 1; i < n; i++)
        if (suit[i] == suit[best] ? power[i] > power[best] : suit[i] == SUIT_TRUMP) best = i;
    return best;
}

/* the player to move lays a card on the trick (taken out of the hand); 0: not in it */
static inline int tk4_lay(tk4_table *t, int card)
{
    int pl = tk4_turn(t);
    if (t->ntrick >= TK4_P || !trick_remove(t->hand[pl], &t->n[pl], card)) return 0;
    t->trick[t->ntrick++] = (uint8_t)card;
    return 1;
}

/* the full trick taken by `winner` (a player) with its card points: the last trick kept, the winner leads */
static inline void tk4_take(tk4_table *t, int winner, int eyes)
{
    memcpy(t->last, t->trick, TK4_P);
    t->last_leader = t->leader;
    t->last_winner = (uint8_t)winner;
    t->won[winner]++;
    t->eyes[winner] = (int16_t)(t->eyes[winner] + eyes);
    t->ntrick = 0;
    t->tricks++;
    t->leader = (uint8_t)winner;
}

/* a hand sorted by a key for each card (key[card], low first) */
static inline void tk4_sort(uint8_t *h, int n, const int16_t *key)
{
    for (int i = 1; i < n; i++) {
        uint8_t c = h[i];
        int j = i;
        while (j > 0 && key[h[j - 1]] > key[c]) { h[j] = h[j - 1]; j--; }
        h[j] = c;
    }
}

/* the pack of a game: every card of the ranks listed (0 ends them) in each suit, `copies` times, shuffled from the
 * seed; its size */
static inline int tk4_pack(uint8_t *out, const int *ranks, int copies, uint32_t seed)
{
    int n = 0;
    for (int k = 0; k < copies; k++)
        for (int s = 0; s < 4; s++)
            for (int r = 0; ranks[r]; r++) out[n++] = CARD(s, ranks[r]);
    cards_shuffle(out, n, seed);
    return n;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static inline void tk4_space(const char **s)
{
    while (**s == ' ') (*s)++;
}

/* a suit letter (s h d c) at *s, moved past; -1: none */
static inline int tk4_suit_letter(const char **s)
{
    static const char suits[] = "shdc";
    for (int i = 0; i < 4; i++)
        if (**s == suits[i]) { (*s)++; return i; }
    return -1;
}

#endif
