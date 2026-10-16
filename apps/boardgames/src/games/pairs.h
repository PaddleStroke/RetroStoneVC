/*
 * Pairs (Concentration), for 1 to 4 players on one device: the rules. No SDL; the cards are laid out from a seed
 * (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The cards lie face down in a grid, each picture twice: 12 cards (6 pictures), 24 (12) or 36 (18). The pictures are
 * the house's own: the seven animals of the family deck (families_deck.h) and chess pieces in the players' colours
 * (art.h); a game takes all its pictures from the animals first (6 of the 7 for 12 cards), then pieces, picked by the
 * seed. A turn: turn two cards up. A pair: the player keeps it and plays again. Not a pair: both are turned face
 * down again (the action "turn back"), and the next player plays. One player: a solo puzzle, every pair to find, the
 * moves (pairs of cards turned) counted. More players: once every pair is found, the most pairs wins (a tie: shared).
 * Notation (script; the screenshots and the tests): t<card> turn a card up (0.. in reading order, the grid in
 * portrait), h turn the two back, a a plain action (a player who remembers every card seen), "#" plain actions to the
 * end.
 */
#ifndef PAIRS_H
#define PAIRS_H

#include <stdint.h>

#define PR_MAXP 4
#define PR_MAX 36
#define PR_ANIMALS 7                            /* pictures 0..6: the families' animals */
#define PR_PIECES 17                            /* 7..23: chess pieces (pr_piece) */
#define PR_POOL (PR_ANIMALS + PR_PIECES)
#define PR_MAX_ACTS 4096                        /* kept for undo (a replay): a 36-card game at random needs 2000 */
#define PR_NONE 255

enum { PR_SMALL, PR_MEDIUM, PR_LARGE, PR_VARIANTS };   /* 12, 24, 36 cards */
enum { PP_PLAY, PP_SHOW, PP_OVER };             /* PP_SHOW: two cards up that are not a pair */
enum { PA_TURN = 1 << 6, PA_BACK = 2 << 6 };    /* an action: its kind and a card */
#define PA_KIND(a) ((a) & 0xc0)
#define PA_CARD(a) ((a) & 0x3f)

typedef struct {
    uint8_t variant, players, n, cols, rows;    /* the grid in portrait: cols x rows */
    uint8_t turn, phase;
    uint8_t pic[PR_MAX];                        /* each card's picture (0..PR_POOL-1) */
    uint8_t found[PR_MAX];                      /* who found it (PR_NONE: still in play) */
    uint8_t seen[PR_MAX];                       /* turned up at least once */
    int8_t up[2];                               /* the cards up this turn (-1) */
    uint8_t pairs[PR_MAXP];
    uint16_t moves;                             /* pairs of cards turned */
    uint32_t seed;
} prpos;

typedef struct {
    prpos pos;
    uint32_t seed;
    uint8_t variant, players;
    uint8_t act[PR_MAX_ACTS];
    int n;
} prgame;

int  pr_cards(int variant);                     /* 12, 24, 36 */
/* a chess picture: its piece (chess.h PAWN..KING) and colour (art_player: 0..5); picture 7.. */
void pr_piece(int picture, int *type, int *colour);
void pr_new(prgame *g, int variant, int players, uint32_t seed);
int  pr_legal(const prpos *p, int act);
int  pr_play(prgame *g, int act);               /* 0: not allowed */
int  pr_undo(prgame *g);                        /* back one action (a replay); 0: none */
int  pr_left(const prpos *p);                   /* the cards still in play */
unsigned pr_winners(const prpos *p);            /* PP_OVER: the players with the most pairs */
int  pr_auto(const prpos *p);                   /* a plain action */
int  pr_script(prgame *g, const char *moves);   /* 0: an action is bad */

/* the bot (pairs_bot.c, bots.h), and what the screen (pairs.c) gives it; a headless test defines both */
const prgame *pairs_live(void);                 /* the game on the screen, read-only: the bot copies it at entry */
int pairs_show(const char *move);               /* main thread: "t5" turns a card up as a tap does; "h" turns the two
                                                 * back; 0: not legal */
void prb_view(prgame *g);                       /* the pictures of the cards never turned up wiped */
int  prb_choose(const prgame *view, int level, uint64_t seed);   /* its action (PA_*); -1: none */

#endif
