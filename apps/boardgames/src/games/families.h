/*
 * Happy Families (le jeu des 7 familles) for 2 to 6 players on one device: the rules. No SDL; the deal comes from a
 * seed (cards.h's shuffle), so a game is its seed and its questions.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The family deck (families_deck.h): 7 families of 6 (grandfather, grandmother, father, mother, son, daughter). Deal:
 * 7 cards each for 2 or 3 players, 6 for 4, 5 for 5 or 6; the rest is the stock. On your turn, ask any other player
 * holding cards (when nobody else holds any: anyone, the stock answers) for one card: a member of a family you
 * already hold a card of, a card you do not hold. They have it: they give it to you and you ask again. They do not:
 * "pioche", you draw from the stock; if it is the card you asked for, you ask again, else the turn passes to the next
 * player (the stock empty: it passes at once). A family complete in a hand is laid down at once. A player whose turn
 * comes with no cards draws one (the stock empty: their turn is passed). The game ends when the 7 families are down: the most families win (a tie: they share the win).
 * Notation (script; the screenshots and the tests): a<p><f><m> ask player p for family f's member m (three digits);
 * x one plain question; # plain questions to the end.
 */
#ifndef FAMILIES_H
#define FAMILIES_H

#include <stdint.h>

#include "games/families_deck.h"

#define FAM_MAXP 6
#define FAM_CARDS 42
#define FAM_MAX_ACTS 2048
#define FAM_NONE 255

enum { FP_ASK, FP_OVER };
enum { FAM_MISSED, FAM_GIVEN, FAM_LUCKY };      /* the last question's answer: pioche, given, the card drawn */

typedef struct {
    uint8_t players, phase, turn;
    uint8_t hand[FAM_MAXP][FAM_CARDS], n[FAM_MAXP];   /* sorted: by family, then member */
    uint8_t stock[FAM_CARDS];                   /* its top last */
    int16_t nstock;
    uint8_t owner[FAM_FAMILIES];                /* who laid each family down (FAM_NONE: not yet) */
    uint8_t laid[FAM_MAXP][FAM_FAMILIES], nlaid[FAM_MAXP];   /* each player's families, in the order laid */
    uint8_t ask_from, ask_to, ask_card, answer; /* the last question (ask_from FAM_NONE: none yet) */
    uint8_t drawn;                              /* the card drawn after it (FAM_NONE: none) */
    uint8_t refills;                            /* cards drawn by players whose turn came with an empty hand */
    uint32_t seed;
} fpos;

typedef struct {
    fpos pos;
    uint32_t seed;
    uint8_t players;
    uint16_t act[FAM_MAX_ACTS];
    int n;
} fam_game;

#define FAM_ASK(player, card) ((player) << 8 | (card))
#define FAM_ASK_PLAYER(a) ((a) >> 8)
#define FAM_ASK_CARD(a) ((a) & 0xff)

void fam_new(fam_game *g, int players, uint32_t seed);
int  fam_holds(const fpos *p, int player, int card);
int  fam_has_family(const fpos *p, int player, int family);
int  fam_askable(const fpos *p, int player);    /* the player to move may ask them (they hold cards, or nobody
                                                 * else does) */
int  fam_legal(const fpos *p, int ask);
int  fam_play(fam_game *g, int ask);            /* 0: not allowed */
int  fam_undo(fam_game *g);                     /* back one question (a replay from the deal) */
unsigned fam_winners(const fpos *p);            /* FP_OVER: the most families (bit n: player n) */
int  fam_auto(const fpos *p);                   /* a plain question (it knows the hands: for the tests) */
int  fam_script(fam_game *g, const char *moves);   /* 0: a question is bad */

/* the bot (families_bot.c, bots.h), and what the screen (families.c) gives it; a headless test defines both */
const fam_game *families_live(void);            /* the game on the screen, read-only: the bot copies it at entry */
int families_show(const char *move);            /* main thread: "a<p><f><m>" asked as taps do; 0: not legal */
int famb_ask(const fam_game *g, int level, uint64_t seed);   /* the question (FAM_ASK) for the player to move; -1 */

#endif
