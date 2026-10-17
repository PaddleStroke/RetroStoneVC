/*
 * RetroStone Board: a card or tile game's table of its rounds (the scores), drawn over the game's table between two
 * rounds, and inside the shell's end card once the game is over (shell.h: ui_over, ui_end_room). Shared by the games
 * that draw their own table (Barbu, Belote, Bridge, Canasta, Dominoes...); those built on ct_summary need none of it.
 * Static inline, no state kept.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef ENDTABLE_H
#define ENDTABLE_H

#include <math.h>

#include "cards_table.h"
#include "shell.h"

/* The table pw x ph: where its top left goes (x0, y0), in the coordinates to draw it in; then draw it as always, and
 * call endt_done(). Between rounds: in the middle of b, the game's table under a veil (its alpha) and an opaque panel.
 * The game over: in the end card's room (the shell veils the table and draws the card); a room smaller than asked
 * takes the table scaled down whole (the renderer's scale, until endt_done). Once a frame, the last thing drawn. */
static inline void endt_begin(box b, float pw, float ph, Uint8 veil, float *x0, float *y0)
{
    if (!ui_over()) {
        *x0 = b.x + (b.w - pw) / 2;
        *y0 = b.y + (b.h - ph) / 2;
        ct_veil(b, veil);
        ct_panel(*x0, *y0, pw, ph, 255);
        return;
    }
    box r = ui_end_room(pw, ph);
    float k = fminf(1, fminf(r.w / pw, r.h / ph));
    *x0 = r.x + (r.w - pw * k) / 2;
    *y0 = r.y + (r.h - ph * k) / 2;
    if (k < 1) {
        SDL_RenderSetScale(ui.r, k, k);
        *x0 /= k;
        *y0 /= k;
    }
}

static inline void endt_done(void) { SDL_RenderSetScale(ui.r, 1, 1); }

#endif
