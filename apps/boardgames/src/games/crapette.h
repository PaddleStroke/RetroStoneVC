/*
 * Crapette (Russian Bank), the race patience for two, each with their own pack of 52: the rules. No SDL; the deal
 * comes from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The classic French rules, as played here:
 * - Each player deals their own shuffled pack: 13 cards in a pile, the crapette, its top card face up; 4 cards face up
 *   in a column of houses on their side of the middle (the 8 houses are shared); the 35 others face down, the talon.
 *   Each has a waste pile too, empty at first. The 8 foundations in the middle are shared.
 * - The higher top card of the crapettes starts (ace low, king high); equal: the higher house cards, from the first;
 *   all equal: player 0.
 * - The available cards: your crapette's top card (the next one is turned at once), the top (outer) card of each
 *   house, and the card you turned from your talon. Never your own waste, never the other's piles.
 * - Foundations: an ace starts one, then up in suit to the king. Houses: down in alternating colours; an empty house
 *   takes any card. Loading: you may put a card on the other's crapette or waste (not an empty one) if it is of the
 *   same suit and one rank above or below its top card.
 * - The priorities: (1) a card that can go to a foundation must go there before anything else, and the crapette's
 *   top card first of all; (2) the crapette's top card must be played, if it can go anywhere, before you turn a card
 *   from your talon. Moves between houses are free otherwise; the talon card may be played or not.
 * - The talon: turn its top card (once your hands are free: nothing turned already); play it, then turn another; or
 *   put it on your waste: that ends your turn. A talon used up: the waste is turned over to make a new one (when you
 *   need a card). With no talon nor waste left you pass.
 * - "Crapette!": the app does not forbid a move that breaks a priority (it marks it); until your turn ends, the other
 *   player may call it. The game then goes back to the position before that first faulty move, your turned talon
 *   card (if any) goes onto your waste, and the turn passes to the caller.
 * - The first to get rid of their crapette, talon and waste wins. Blocked (no card goes to a foundation, nor leaves
 *   a crapette or a talon, for long enough that each player has been through their whole talon twice): the fewer
 *   cards left in crapette, talon and waste wins; equal: a draw.
 * Places (relative to the player to move): houses 0..7 (player 0's 0..3, player 1's 4..7, the first nearest its
 * owner), foundations 8..15, the crapette 16, the talon card turned 17, the other's crapette 18, the other's waste 19.
 * Notation (script): <from>-<to> moves a card, the places: 0..7 the houses, f a foundation (the one that fits; f0..f7
 * one of them), c the crapette, h the talon card turned, C the other's crapette, W the other's waste; t turns the
 * talon's top card, d puts the talon card on your waste (the turn ends), p passes, x calls crapette (the other
 * player); "." one plain action, "#" plain actions to the end of the game.
 */
#ifndef CRAPETTE_H
#define CRAPETTE_H

#include <stdint.h>

#include "cards.h"

#define CR_COLS 8
#define CR_FOUND 8
#define CR_PILE 104
#define CR_COLMAX 16
#define CR_MAX_ACTS 16384
#define CR_NONE 255
#define CR_DRAW 2

enum { CL_COL = 0, CL_FOUND = 8, CL_CRAP = 16, CL_HELD = 17, CL_OCRAP = 18, CL_OWASTE = 19, CL_N = 20 };
enum { CP_PLAY, CP_OVER };
/* an action: its kind, and for a move its place from and to */
enum { CA_MOVE = 1 << 10, CA_TURN = 2 << 10, CA_DISCARD = 3 << 10, CA_PASS = 4 << 10, CA_CALL = 5 << 10 };
#define CA_KIND(a) ((a) & 0x1c00)
#define CA_SRC(a) ((a) >> 5 & 31)
#define CA_DST(a) ((a) & 31)
#define CA_MOVEOF(s, d) (CA_MOVE | (s) << 5 | (d))
/* the priority broken: the crapette to a foundation first; a card to a foundation; the crapette before the talon */
enum { CF_NONE, CF_CRAP_FOUND, CF_FOUND, CF_CRAP_FIRST };

typedef struct {
    uint8_t phase, turn, first, winner;         /* winner: 0, 1, CR_DRAW (CR_NONE: playing) */
    uint8_t held;                               /* the talon card turned (CARD_NONE: none) */
    uint8_t crap[2][CR_PILE], ncrap[2];         /* each pile bottom first, its top last */
    uint8_t talon[2][CR_PILE], ntalon[2];
    uint8_t waste[2][CR_PILE], nwaste[2];
    uint8_t col[CR_COLS][CR_COLMAX], ncol[CR_COLS];
    uint8_t found[CR_FOUND];                    /* each foundation's top card (CARD_NONE: empty) */
    uint8_t fault, fault_card;                  /* this turn's first priority broken (CF_*), the card it passed over */
    uint8_t moved, blocked;                     /* a real move this turn; the game ended blocked */
    uint16_t idle;                              /* turns in a row without a real move */
    uint32_t seed;
} cpos;

typedef struct {
    cpos pos, saved;                            /* saved: the position before this turn's first fault */
    uint32_t seed;
    uint16_t act[CR_MAX_ACTS];
    int n;
} cgame;

void crap_new(cgame *g, uint32_t seed);
int  crap_legal(const cpos *p, int act);
int  crap_moves(const cpos *p, int *acts);      /* every legal action (at most 512); how many */
int  crap_play(cgame *g, int act);              /* 0: not allowed */
int  crap_undo(cgame *g);                       /* back one action (a replay from the deal); 0: none */
int  crap_card_at(const cpos *p, int place);    /* the available card at a place (the player to move's view); a
                                                 * pile's top; CARD_NONE: none */
int  crap_fits(const cpos *p, int card, int place);   /* the card may go there (building, loading) */
int  crap_fault_of(const cpos *p, int act);     /* the priority this action would break (CF_*) */
int  crap_found_for(const cpos *p, int card);   /* the foundation it goes on (CL_FOUND + i), the first that fits */
int  crap_left(const cpos *p, int player);      /* cards still in their crapette, talon, waste (and turned) */
int  crap_auto(const cpos *p);                  /* a plain action (the "." of the notation) */
int  crap_script(cgame *g, const char *moves);  /* 0: an action is bad */

#endif
