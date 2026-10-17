/*
 * The RetroStone app's Arcade tab (EDITION=retrostone: its .conf's ARCADE=<the Arcade's .conf>): every RetroStone VC
 * game of the Arcade's catalogue, played inside this app. Each has a card in the menu, in its own section (the
 * shell's SEC_ARCADE, a joystick on its tab): a game of the shell's list (game_register) that stands for it, its
 * picture a moment of its play, its name its text "arcade_<id>" (tools/gen_arcade.py: $(B)/arcade/gen). A tap on it
 * runs the game through the RetroStone VC runtime in the app's window (arcade_play.c: apps/launcher's runtime, its
 * modules and its SDL frontend's picture, touch buttons, pads, keys and sound) until its back button or Back.
 * The other editions build none of it: no card, so no tab (arcade.c's stand-ins).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef ARCADE_H
#define ARCADE_H

#include <SDL.h>
#include <stdint.h>

/* ---- the runtime's side (arcade_play.c; stand-ins without ARCADE) ------------------------------------------------ */
/* plays the Arcade game `id` ("leadysquid") in the renderer's window until it is left (its back button, Back, Esc),
 * the device held either way: full screen in landscape (Android: no system bars), the bars kept upright (rsb_sdl.c's
 * picture across the width, the touch buttons below it). Its save state and save RAM (its best score) are kept as the Arcade app keeps them
 * (beside the app's save file: arcade-<id>.state, .srm); the session gives XP (stats.h: STATS_ARCADE). 1: the app
 * was closed meanwhile (an SDL_QUIT is queued again for the shell). */
int  arcade_play(const char *id, SDL_Renderer *r);
/* the picture mode (shell.c --shot out.bmp --game arcade_<id>): the game started afresh, run --at ms (3000) of play
 * headless, the pad's buttons pressed as --pad "START@60 A@120" says (each for 6 frames from that frame), the frame
 * drawn into the renderer (its touch buttons, pause and back buttons) and saved as out; 0: done */
int  arcade_shot(const char *id, SDL_Renderer *r, SDL_Surface *surf, const char *out, int argc, char **argv);

#ifndef ARCADE_RUNTIME
#include "shell.h"

/* ---- the menu's side (arcade.c, the generated arcade_data.c) ----------------------------------------------------- */
typedef struct {
    const char *id;                             /* the Arcade's id: "leadysquid" */
    const game *card;                           /* the shell's game for it ("arcade_leadysquid") */
    const uint8_t *pix, *pal;                   /* its picture: 96 x 96 colour indices, ncol RGB colours */
    int ncol;
} arcade_card;
extern const arcade_card arcade_cards[];        /* in the Arcade's order (by name) */
extern const int arcade_ncards;                 /* 0: not the RetroStone app */
#define ARCADE_SIDE 96

int  arcade_of(const game *g);                  /* the card standing for an Arcade game (its arcade_cards index), else -1 */
void arcade_thumb(int a, int size, uint8_t *rgba);   /* its picture, size x size (any thread: the menu's helper's) */
void arcade_icon(int size, uint8_t *rgba);      /* the Arcade tab's picture: a joystick */

/* the shell's game for an Arcade card: a picture only (its tap plays the Arcade game, shell.c); the rest stands still */
void arcade_token(int player, int size, uint8_t *rgba);
void arcade_start(int variant);
void arcade_layout(box board);
void arcade_draw(void);
void arcade_xy(float x, float y);
int  arcade_zero(void);
#define ARCADE_CARD(id_, order_, thumb_)                                                                            \
    {.id = id_, .order = order_, .variants = 1, .thumb = thumb_, .token = arcade_token, .start = arcade_start,      \
     .layout = arcade_layout, .draw = arcade_draw, .press = arcade_xy, .motion = arcade_xy, .release = arcade_xy,    \
     .undo = arcade_zero, .turn = arcade_zero, .status = arcade_zero, .players = 1u << 1}
#endif

#endif
