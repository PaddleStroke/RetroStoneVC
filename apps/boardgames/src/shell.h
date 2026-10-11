/*
 * RetroStone Board: one app, many board games for two players on one device. The shell (shell.c) does everything the
 * games share: the menu, the variant picker, the players' bars (their token, the turn, what each game adds), the
 * buttons (home, undo, flip, face to face, new game), the end-of-game card, the sounds, the screenshots. A game
 * (games/<id>.c) only brings its rules and its board, and registers itself with REGISTER_GAME.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 and 1. Player 0 plays first and sits at the bottom (white, ivory), unless the board is flipped. No text
 * anywhere (nothing to translate): pictures, and digits for scores.
 */
#ifndef SHELL_H
#define SHELL_H

#include <SDL.h>
#include <stdint.h>

#include "art.h"

typedef struct { float x, y, w, h; } box;

enum { GS_PLAY, GS_WIN0, GS_WIN1, GS_DRAW };

typedef struct game {
    const char *id;                             /* "chess" */
    int order;                                  /* the place in the menu */
    int variants;                               /* 1: none to choose; n: a picker before a new game (go's sizes) */
    void (*thumb)(int variant, int size, uint8_t *rgba);   /* the menu's card (variant -1) and the picker's */
    void (*token)(int player, int size, uint8_t *rgba);    /* a player's mark: the bars, the end-of-game card */
    void (*start)(int variant);                 /* a new game */
    void (*layout)(box board);                  /* where the board goes (a square, pixels): make its pictures */
    void (*draw)(void);                         /* the board and the pieces (the shell drew the background) */
    void (*press)(float x, float y);            /* the pointer (mouse or a finger), pixels */
    void (*motion)(float x, float y);
    void (*release)(float x, float y);
    int  (*undo)(void);                         /* 0: nothing to undo */
    int  (*turn)(void);                         /* the player to move */
    int  (*status)(void);                       /* GS_* */
    /* optional (NULL) */
    void (*bar)(int player, box b);             /* what the player's bar shows next to the token (pieces taken, a score) */
    int  (*can_pass)(void);                     /* 1: the player to move may pass (the pass button shows in the bar) */
    void (*pass)(void);
    int  (*script)(const char *moves);          /* plays moves in the game's notation (screenshots, tests); 0: bad */
    int  (*busy)(void);                         /* 1 while the last move still animates: the end of the game waits
                                                 * for it (then a pause showing the final position, then the card) */
} game;

void game_register(const game *g);
#define REGISTER_GAME(g) \
    __attribute__((constructor)) static void register_##g(void) { game_register(&g); }

/* ---- what the shell gives the games ------------------------------------------------------------------------------- */
extern struct ui {
    SDL_Renderer *r;
    int w, h;                                   /* the screen, pixels */
    int flipped;                                /* player 1 at the bottom */
    int f2f;                                    /* face to face: the top player's pieces and bar turned to face them */
    float unit;                                 /* a ninth of the board's side: the size of the marks, the bars */
} ui;

int  ui_bottom(void);                           /* the player at the bottom */
int  ui_upside(int player);                     /* 1: draw this player's pieces turned round (face to face, top) */
SDL_Texture *ui_tex(int w, int h, const uint8_t *rgba);   /* a picture as a texture (alpha blending) */
void ui_free(SDL_Texture **t);
uint8_t *ui_buf(int w, int h);                  /* scratch for one picture (reused) */
void ui_put(SDL_Texture *t, float x, float y, float w, float h);
void ui_put_c(SDL_Texture *t, float cx, float cy, float w, float h);
void ui_put_rot(SDL_Texture *t, float cx, float cy, float w, float h, double degrees);
void ui_fill(float x, float y, float w, float h, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void ui_shadow(float x, float y, float size);   /* the soft shadow under a piece drawn at (x, y), size x size */
void ui_number(float cx, float cy, float h, int value, int light);   /* digits centred; light: cream, else dark */
void ui_animate(void);                          /* draw the next frame too (an animation runs) */
Uint32 ui_ms(void);

/* the shared marks, made for the board's size by the shell (a square of side `size`) */
void ui_mark_last(float x, float y, float w, float h);   /* the last move's squares (a warm tint) */
void ui_mark_select(float x, float y, float w, float h); /* the selected piece */
void ui_mark_dot(float cx, float cy, float d);            /* a possible move: a soft dot */
void ui_mark_ring(float cx, float cy, float d);           /* a possible capture: a ring */
void ui_mark_glow(float cx, float cy, float d);           /* danger (check): a red glow */

enum { SND_MOVE, SND_CAPTURE, SND_PLACE, SND_DROP, SND_CHECK, SND_END, SND_TAP, SND_DENY, SNDS };
void ui_sound(int s);

/* sound.c */
void snd_init(void);
void snd_play(int s);
extern int snd_muted;

#endif
