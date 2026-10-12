/*
 * The xiangqi pieces, for the games played with them (xiangqi, banqi): an ivory wooden disc, an engraved ring and the
 * piece's character in its side's ink (red or black). Written once, here, as static inline functions (palace.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef XQPIECE_H
#define XQPIECE_H

#include "games/palace.h"
#include "games/xiangqi.h"

/* the characters, XQ_GENERAL..XQ_SOLDIER: red's set, black's set (help/glyphs.txt keeps them in the fonts) */
static const int XQ_CHARS[2][8] = {
    {0, 0x5E25, 0x4ED5, 0x76F8, 0x508C, 0x4FE5, 0x70AE, 0x5175},   /* 帥仕相傌俥炮兵 */
    {0, 0x5C07, 0x58EB, 0x8C61, 0x99AC, 0x8ECA, 0x7832, 0x5352},   /* 將士象馬車砲卒 */
};
static const uint8_t XQ_INK[2][3] = {PALACE_RED_INK, {32, 28, 26}};   /* red ink, ebony ink */

/* a piece (XQ_GENERAL..XQ_SOLDIER) of a side (0 red, 1 black), size x size */
static inline void xq_piece_paint(int type, int player, int size, uint8_t *rgba)
{
    palace_piece(0, 1, XQ_CHARS[player][type], XQ_INK[player], size, rgba);
}

#endif
