/*
 * RetroStone Board's card table (the card kit's second part, on top of cards.h and the shell): what the card games
 * draw round their cards, written once. Small pictures made at the size they are drawn and kept (the players' chips,
 * suit signs, discs, panels, the shell's icons tinted, round buttons, badges).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef CARDS_TABLE_H
#define CARDS_TABLE_H

#include <stdint.h>

#include "cards.h"
#include "shell.h"

/* ---- small pictures, kept ----------------------------------------------------------------------------------------
 * Each made once at a size and kept in a cache the kit shares: take it again each frame, do not keep it. It comes
 * back untinted and opaque (ct_icon: tinted r, g, b); tint or fade it after. */
SDL_Texture *ct_token(int player, int size);    /* a player's chip (cards_token), size x size */
SDL_Texture *ct_suit(int deck, int suit, int size);   /* a suit's sign (cards_suit_icon) */
/* a disc of one colour fading from alpha a0 at its middle (art_disc; a0 in hundredths: 85 = 0.85) */
SDL_Texture *ct_disc(const uint8_t rgb[3], int a0, int size);
SDL_Texture *ct_round(int w, int h, float radius, const uint8_t rgba[4]);   /* a rounded rectangle of one colour */
SDL_Texture *ct_icon(int which, int size, Uint8 r, Uint8 g, Uint8 b);       /* the shell's icon (art.h ICON_*) */

#endif
