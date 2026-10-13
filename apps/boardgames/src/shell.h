/*
 * RetroStone Board: one app, many board games for two players on one device. The shell (shell.c) does everything the
 * games share: the menu, the variant picker, the players' bars (their token, the turn, what each game adds), the
 * buttons (home, undo, flip, face to face, new game), the end-of-game card, the sounds, the screenshots. A game
 * (games/<id>.c) only brings its rules and its board, and registers itself with REGISTER_GAME.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 and 1. Player 0 plays first and sits at the bottom (white, ivory), unless the board is flipped. A game
 * may take more players (up to 6: ludo, halma...): the picker then asks how many, the game reads ui.players. No text
 * anywhere (nothing to translate): pictures, and digits for scores.
 */
#ifndef SHELL_H
#define SHELL_H

#include <SDL.h>
#include <stdint.h>

#include "art.h"

typedef struct { float x, y, w, h; } box;

#define MAX_PLAYERS 6
enum { FINGER_DOWN, FINGER_MOVE, FINGER_UP };    /* game.finger()'s phases */
enum { GS_PLAY, GS_WIN0, GS_WIN1, GS_DRAW, GS_WIN2, GS_WIN3, GS_WIN4, GS_WIN5 };
static inline int gs_win(int player) { return player < 2 ? GS_WIN0 + player : GS_WIN2 + player - 2; }
static inline int gs_winner(int status)         /* the winner, or -1 (playing, a draw) */
{
    return status == GS_WIN0 || status == GS_WIN1 ? status - GS_WIN0 : status >= GS_WIN2 ? status - GS_WIN2 + 2 : -1;
}

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
    void (*bar)(int player, box b);             /* what the player's bar shows next to the token (pieces taken,
                                                 * a score) */
    int  (*can_pass)(void);                     /* 1: the player to move may pass (the pass button shows in the bar) */
    void (*pass)(void);
    int  (*script)(const char *moves);          /* plays moves in the game's notation (screenshots, tests); 0: bad */
    int  (*busy)(void);                         /* 1 while the last move still animates: the end of the game waits
                                                 * for it (then a pause showing the final position, then the card) */
    /* a press in a player's bar (shogi's pieces in hand): x, y in the frame bar() drew in (the shell turns them for
     * a bar turned round face to face); 1: taken. The motion and the release then go to press()'s usual path. */
    int  (*bar_press)(int player, float x, float y);
    /* more than two players: the counts it can be played with (bit n: n players; 0: two only), and where each player
     * sits (their yard, their corner of the star: a point of the board, 0..1 from its top left, not flipped). Their
     * bars go on that side of the board (top or bottom), in that order from the left. */
    unsigned players;
    void (*home)(int player, float *fx, float *fy);
    /* card and tile games (VIEW_*; 0, VIEW_BOARD: a board everyone sees, all the above as it is): the screen shows one
     * player's view at a time, ui.viewer's: their hand at the bottom of the table, their bar under it, the others'
     * bars at the top (in turn order from the left), no flip nor face to face. layout() gets a rectangle, the table:
     * all the room between the bars. The game turns the view with ui_handoff(). VIEW_OPEN: nothing is secret (golf),
     * the view turns at once; VIEW_HIDDEN: each hand is secret, a cover hides the screen at every hand-off (and at a
     * new game, and when the game is opened again from the menu) until the next player taps its start button. */
    int view;
    /* the players who share the win, read once the game is over (bit n: player n): partners (pachisi, belote), a tie
     * for the best score (GS_DRAW: only the tied players stand). The end card stands them all in gold light together
     * and lays the others down, dimmed; their bars all glow. NULL: gs_winner(status) alone (a draw: everyone). */
    unsigned (*winners)(void);
    int players_usual;                          /* the count the picker offers first (tarot: 4); 0: two if it can,
                                                 * else the fewest */
    /* several fingers at once (bataille corse's slaps): each finger touching the board, with its own id; then press(),
     * motion() and release() get only the mouse (a computer). NULL: one touch at a time, as press() 's. */
    void (*finger)(int id, int phase, float x, float y);
    /* designed levels (Logic Gates, Box Pusher): the shell's level screen comes instead of the picker. Chapter tabs,
     * then the chapter's levels as numbered tiles: solved in gold with their stars, open in wood, locked dim (the
     * first `levels_open` unsolved levels are open, 3 by default). A tile calls start(level) (0..levels-1): a new
     * game of that level. Solved: status() GS_WIN0; the shell asks level_stars() and keeps the best in the save
     * store (shell/levels/<id>; ui_level_best() reads it), and its end card offers the next level. The new game
     * button goes back to the level screen. */
    int levels;                                 /* how many (0: none, the picker as usual) */
    /* optional, a game of levels that also makes puzzles at random: the level screen gets one more tab after the
     * chapters (a die): the variant picker (its variants, thumb(), two axes), and a card picked there starts
     * start_random(variant) instead of start(level); ui.level is then -1. Its end card's next button: a new random
     * puzzle of that variant (replay: the same one). Stars are kept per level only. */
    void (*start_random)(int variant);
    int chapters;                               /* the tabs: the levels split evenly (0, 1: one chapter) */
    int (*chapter_of)(int level);               /* optional: a level's chapter (0.., never going down): uneven ones */
    void (*chapter_icon)(int chapter, int size, uint8_t *rgba);   /* optional: a tab's picture (else its number) */
    void (*level_thumb)(int level, int size, uint8_t *rgba);      /* optional: a tile's picture (else its number) */
    /* the puzzle just solved (status() GS_WIN0): its stars 0..3 (NULL: no stars, solved only). A game of levels: the
     * shell keeps the best. Any one-player game may have it, levels or not: its end card then shows the stars
     * (popping in), the moves beside par(), a replay button (the same puzzle again) and a next one (a new puzzle of
     * the same variant; the next level). ui_par_stars() rates moves against par. */
    int  (*level_stars)(void);
    int levels_open;                            /* the unsolved levels open at once (0: 3) */
    /* optional: a key pressed in the game (not its repeats while held), one the shell does not use itself (Esc, Back,
     * M, H, F1, U, Backspace, F, T, N are the shell's): the arrows... Never while the rules page or the hand-off
     * cover is up, nor on another screen. */
    void (*key)(SDL_Keycode k);
    /* optional, one player: the score the end card shows in big digits beside the token (a final score, the moves a
     * puzzle took); read once the game is over */
    int  (*score)(void);
    /* optional, one player: the fewest moves the puzzle just solved can take (0: not known); the end card shows the
     * moves (score()) beside it */
    int  (*par)(void);
    /* optional, the room on the screen. fill 1: layout() gets the whole rectangle between the bars and the buttons
     * (portrait: the bars' width, all the height between them; landscape: all the width left of the side column), not
     * the 9-unit square: a wide or tall board, a grid fitted with ui_grid_fit (VIEW_* games always get it). tools: how
     * many round buttons of its own the game shows beside its board (0..UI_MAX_TOOLS: a flag mode, a hint, a
     * magnifier, digits to enter): the shell lays them out where they cost the board least (ui_tool*), and their
     * presses come to press() as any other. */
    int fill;
    int tools;
    /* optional, variants on two axes (the logic grids: a size, then a level): axes {sizes, levels}, variant =
     * size * axes[1] + level (the first axes[0] * axes[1] variants). The picker then shows the sizes (in rows; the one
     * chosen in gold light, a tap chooses another) above a row of the levels (a tap starts that size at that level),
     * instead of a card each. axis_pic[0](size, px, rgba), axis_pic[1](level, px, rgba): their pictures (NULL: a size
     * is thumb() of that size at the level chosen last; a level is as many ebony stones, one easy .. three hard). */
    int axes[2];
    void (*axis_pic[2])(int i, int size, uint8_t *rgba);
    /* optional, a VIEW_* game whose hands are secret in some games only (open-hands Durak): 1 the hands are hidden
     * (the cover at each hand-off, as VIEW_HIDDEN), 0 open (the view just turns, as VIEW_OPEN); asked instead of
     * .view == VIEW_HIDDEN at each hand-off. Set .view to either VIEW_OPEN or VIEW_HIDDEN as well. */
    int  (*hidden)(void);
} game;

enum { VIEW_BOARD, VIEW_OPEN, VIEW_HIDDEN };

void game_register(const game *g);
#define REGISTER_GAME(g) \
    __attribute__((constructor)) static void register_##g(void) { game_register(&g); }

/* ---- what the shell gives the games ------------------------------------------------------------------------------ */
extern struct ui {
    SDL_Renderer *r;
    int w, h;                                   /* the screen, pixels */
    int flipped;                                /* player 1 at the bottom */
    int f2f;                                    /* face to face: the top player's pieces and bar turned to face them */
    int players;                                /* how many play this game (2 unless the game takes more) */
    float unit;                                 /* a ninth of the board's side: the size of the marks, the bars */
    int viewer;                                 /* VIEW_* games: whose view is shown (their hand at the bottom); -1
                                                 * while the hand-off cover is up (draw no hand) */
    int level;                                  /* a game of levels: the level in play (0..levels-1); -1 a random game
                                                 * (start_random), or a game without levels */
} ui;

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
int  ui_bottom(void);                          /* the player at the bottom */
int  ui_upside(int player);                     /* 1: draw this player's pieces turned round (face to face, top) */
SDL_Texture *ui_tex(int w, int h, const uint8_t *rgba);   /* a picture as a texture (alpha blending) */
void ui_free(SDL_Texture **t);
uint8_t *ui_buf(int w, int h);                  /* scratch for one picture (reused) */
void ui_put(SDL_Texture *t, float x, float y, float w, float h);
void ui_put_c(SDL_Texture *t, float cx, float cy, float w, float h);
void ui_put_rot(SDL_Texture *t, float cx, float cy, float w, float h, double degrees);
void ui_fill(float x, float y, float w, float h, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
/* the soft shadow under a standing piece drawn at (x, y), size x size */
void ui_shadow(float x, float y, float size);
/* the shadow of a flat piece (disc, stone, man) drawn w x h centred on (cx, cy), towards the lower right; alpha 0..1 */
void ui_shadow_flat(float cx, float cy, float w, float h, float alpha);
/* a flat piece d x d centred on (cx, cy) with its shadow, faded (alpha 0..1), turned round when upside (ui_upside) */
void ui_flat_at(SDL_Texture *t, float cx, float cy, float d, float alpha, int upside);
void ui_number(float cx, float cy, float h, int value, int light);   /* digits centred; light: cream, else dark */
/* the same turned round its centre (degrees: 180 for the top player face to face) */
void ui_number_rot(float cx, float cy, float h, int value, int light, double degrees);
/* a number that may be below zero (a minus before it), or with a plus when plus is set and it is above zero (points
 * added): the scores of card games */
void ui_number_signed(float cx, float cy, float h, int value, int light, int plus);
/* any colour, faded (alpha 0..255); how: UI_NUM_REGULAR the regular digits (else bold), UI_NUM_TIGHT set closer (a
 * number in a small cell) */
enum { UI_NUM_REGULAR = 1, UI_NUM_TIGHT = 2 };
void ui_number_ex(float cx, float cy, float h, int value, const uint8_t rgb[3], Uint8 alpha, int how);
float ui_number_w(int value, float h);          /* how wide ui_number draws it */
/* in a player's bar (bar()'s box): a small piece (0.6 unit) and how many of it, in cream (pieces taken); 0: nothing */
void ui_bar_count(box b, SDL_Texture *piece, int n);
/* the same showing 0 too (a move counter), the piece optional (NULL); returns the x after the number */
float ui_bar_number(box b, SDL_Texture *piece, int n);
/* the turns left before a draw (a quiet stretch counted, the count of an endgame), at the bar's right end: a small
 * hourglass and the number, in soft cream (the rules say when to show it: the player to move, the last twenty or
 * so); returns the bar's room left of it */
box  ui_bar_countdown(box b, int left);
/* a bar holding counts and round buttons, fitted (a puzzle's moves, what is left; its tools at the right end): the
 * counts from the left (a small picture, or NULL, and the number in cream; the moves first) and room for nbtn buttons
 * at the right end. A narrow bar (landscape's) first drops the first count's picture, then sets everything smaller.
 * Draws the counts and returns the buttons' size; ui_bar_spot gives button i's centre, from the right end. */
typedef struct { SDL_Texture *pic; int value; } ui_count;
float ui_bar_fit(box b, const ui_count *counts, int nc, int nbtn);
void ui_bar_spot(box b, int i, float d, float *cx, float *cy);
/* where a player's bar is on the screen (cards flying to it, a glow); upside: drawn turned round (face to face) */
box  ui_bar_box(int player, int *upside);
/* the game's token() now paints other pictures (banqi: the colours decided by the first piece turned over): the bars',
 * the end card's and the hand-off cover's tokens are made again */
void ui_tokens_changed(void);
void ui_animate(void);                          /* draw the next frame too (an animation runs) */
Uint32 ui_ms(void);                             /* the clock for animations: use it, not SDL_GetTicks (the picture
                                                 * mode has a clock of its own: still, moved on past each gesture's
                                                 * animations, or to --at ms after the last one) */

/* the shared marks, made for the board's size by the shell (a square of side `size`) */
void ui_mark_last(float x, float y, float w, float h);   /* the last move's squares (a warm tint) */
void ui_mark_select(float x, float y, float w, float h); /* the selected piece */
/* the same two round (cx, cy the centre, d the diameter): round holes, points */
void ui_mark_last_round(float cx, float cy, float d);
void ui_mark_select_round(float cx, float cy, float d);
void ui_mark_dot(float cx, float cy, float d);            /* a possible move: a dark dot in a thin cream rim */
void ui_mark_ring(float cx, float cy, float d);           /* a possible capture: a ring */
void ui_mark_glow(float cx, float cy, float d);           /* danger (check): a red glow */
void ui_mark_gold(float cx, float cy, float d);           /* a success (a mill, a winning line): a gold light */
void ui_mark_gold_a(float cx, float cy, float d, float a); /* the same fainter (a 0..1: a light coming, pulsing) */

/* ---- a game's own round buttons (on its table, in its bar): the shell's disc, a picture on it -------------------
 * how: UI_LIT a gold light round it (it can be pressed now), UI_DIM faded (not now), UI_ON the gold disc (a mode that
 * is on, the action ready), UI_IVORY an ivory medallion in the disc (a picture in its own colours), UI_TINT the
 * picture (white) tinted cream like the shell's icons, UI_UPSIDE the picture turned round (the top player's). The
 * pictures are made at the size shown and kept. */
enum { UI_LIT = 1, UI_DIM = 2, UI_ON = 4, UI_IVORY = 8, UI_TINT = 16, UI_UPSIDE = 32 };
typedef void (*ui_painter)(int kind, int size, uint8_t *rgba);   /* paints kind's picture on a clear size x size */
/* d across, centred on (cx, cy): one of the shell's icons (ICON_*, cream, 0.62 of d) */
void ui_button(float cx, float cy, float d, int icon, int how);
/* the same with the game's picture: paint(kind, (int)(k * d), ...), shown k * d wide (k 1: the whole disc; paint
 * NULL: the disc alone) */
void ui_button_paint(float cx, float cy, float d, ui_painter paint, int kind, float k, int how);
int  ui_button_hit(float cx, float cy, float d, float x, float y);   /* (x, y) on it, a finger's slack round it */
/* a button's picture painted into a picture (a thumbnail's): its disc d across centred on (cx, cy), the game's
 * picture on it (paint, k, how as above; how: UI_ON, UI_IVORY, UI_TINT) */
void ui_button_pic(uint8_t *rgba, int w, int h, float cx, float cy, int d, ui_painter paint, int kind, float k,
                   int how);

/* ---- the game's tools (game.tools): its round buttons, laid out by the shell ---------------------------------------
 * Portrait: up to three at the right end of the bottom bar (when one bar is there: bar() gets the room left), more in
 * a row (two past seven) between the board and that bar. Landscape: in the side column, between the top bar and the
 * shell's buttons. Tool i keeps its place while the game shows it; the game draws them (ui_tool, ui_tool_paint: as
 * ui_button, ui_button_paint) and finds them under a finger (ui_tool_hit) in press(). */
#define UI_MAX_TOOLS 12
void ui_tools_shown(int n);                     /* how many show now: the first n (a magnifier only on big fields);
                                                 * laid out again at once; each layout() starts with all */
int  ui_tool_at(int i, float *cx, float *cy, float *d);   /* tool i's centre and size; 0: not shown */
void ui_tool(int i, int icon, int how);         /* tool i with one of the shell's icons (ICON_*, how: UI_*) */
void ui_tool_paint(int i, ui_painter paint, int kind, float k, int how);   /* with the game's picture */
int  ui_tool_hit(float x, float y);             /* the tool under the finger, else -1 */

/* ---- a grid of square cells in a box (game.fill's room): cols x rows cells, `frame` cells of room on each side, as
 * big as they go; turned a quarter (turned 1: the game's columns run down the screen, its rows across) when that
 * makes the cells bigger (a 30 x 16 field on a phone held upright). Centred in the box. */
typedef struct {
    float x, y, cell;                           /* the cells' top left on the screen; a cell's side (whole pixels) */
    int cols, rows;                             /* as on the screen (turned: the game's rows and columns swapped) */
    int turned;
} ui_grid;
ui_grid ui_grid_fit(box b, int cols, int rows, float frame);
void ui_grid_cell(const ui_grid *g, int col, int row, float *x, float *y);   /* the game's cell: its top left */
int  ui_grid_hit(const ui_grid *g, float x, float y, int *col, int *row);    /* the game's cell there: 1, else 0 */

/* ---- the loupe, for boards whose cells are small for a finger (go 19 x 19, gomoku, hex 11 x 11, word tiles): while
 * a finger is down on the board, a square window beside it shows the cells under it magnified (the ghost stone, the
 * target square in it), so the finger can slide to the right point before it lets go. It sits above the finger
 * (below it for a player whose hand comes from the top, or when there is no room above), inside the room given, in a
 * walnut frame. The game draws what it shows itself, between ui_loupe_begin() and ui_loupe_end(): each point of the
 * board at ui_loupe_map()'s place, its sizes times l.z. */
#define UI_LOUPE_SMALL 0.06f                    /* cells under this part of the screen's short side: it shows */
#define UI_LOUPE_Z 2.3f                         /* its magnification */
typedef struct {
    float fx, fy;                               /* the point of the board it magnifies (the finger's) */
    float cx, cy, side, z, cell;                /* its window: the centre, the side; the magnification; a cell */
} ui_loupe;
int  ui_loupe_small(float cell);                /* 1: cells this wide (pixels) are small for a finger */
/* the loupe for a finger at (fx, fy) on cells `cell` wide, `cells` of them across, inside the box room: above the
 * finger, or below it (below 1: the player's hand comes from the top: face to face); smaller (down to 0.6 of its
 * size) where the room on that side is short, on the other side when even that does not fit there */
ui_loupe ui_loupe_at(float fx, float fy, float cell, float cells, int below, box room);
void ui_loupe_map(const ui_loupe *l, float x, float y, float *lx, float *ly);   /* a board's point as it shows it */
void ui_loupe_begin(const ui_loupe *l);         /* its shadow, frame and table; then drawing is clipped to it */
void ui_loupe_end(void);                        /* the clip lifted */
/* a finger put down near the ghost stone on small cells (closer than `slack`) is taken as put down on it: the offset
 * from the finger to the ghost, kept while the finger slides (else 0, 0) */
void ui_loupe_snap(float x, float y, float gx, float gy, float slack, float *dx, float *dy);

/* ---- trays by the players' sides, for two-player boards that fill the room (game.fill): in the room the board
 * leaves (above and below it when the room is taller than the board, else beside it on the right: the top player's
 * half above the bottom player's), what each player holds (pieces still to place) or has taken, near them. Slots: n
 * of them, in rows of cols, the first nearest the player and on their left (the top player's turned round face to
 * face). No room for slots of side `least`: no trays (ui_trays_on 0), the game shows them otherwise (in its bars). */
typedef struct {
    box b[2];                                   /* player p's tray (w 0: no trays) */
    int n, cols, rows;                          /* its slots: n, in rows of cols, the first row nearest the player */
    float pitch;                                /* a slot's side, pixels */
} ui_trays;
/* the board (bw x bh pixels) in the room r, with a tray by each player's side for n things in slots of side up to
 * `most` (none below `least`): the board's top left in *x, *y (whole pixels) */
void ui_trays_place(ui_trays *t, box r, float bw, float bh, int n, float most, float least, float *x, float *y);
int  ui_trays_on(const ui_trays *t);
/* slot i's centre in player p's tray: the first row along the side nearest them, from their left */
void ui_trays_slot(const ui_trays *t, int p, int i, float *cx, float *cy);
int  ui_trays_hit(const ui_trays *t, int p, float x, float y);   /* 1: (x, y) in player p's tray */

/* a game's own words in the game (a bid, a call; no other words outside the rules page): line i of its words
 * (text_word: help/<lang>/<id>_words.txt, key "<id>_words") as a picture, bold, px pixels, no wider than maxw (made
 * smaller to fit), in rgb. The shell keeps it; *w x *h its size (the ink centred in it, 1.55 px high). NULL: none. */
SDL_Texture *ui_word(const char *key, int i, float px, float maxw, const uint8_t rgb[3], int *w, int *h);

enum { SND_MOVE, SND_CAPTURE, SND_PLACE, SND_DROP, SND_CHECK, SND_END, SND_TAP, SND_DENY, SND_DICE, SNDS };
void ui_sound(int s);
void ui_sound_soft(int s, float volume);        /* quieter (0..1): many small sounds in a row (mancala's seeds) */
void ui_sound_later(int s, int ms);             /* in ms: when a sliding piece lands */

/* chance: the shell's dice. ui_random gives 0..n-1 (seeded at start; --seed in the picture mode, so tests and
 * screenshots repeat); the rules take the values as input. ui_dice_throw shows dice tumbling to their values (with
 * their sound), ui_dice_draw draws them in a row where the game wants (size: one die; up to UI_MAX_DICE), the used
 * ones dimmed (bit i: die i). A game that lays its dice out itself (kept, set aside, in a cup...) draws them one by
 * one: ui_die, ui_die_thrown. */
#define UI_MAX_DICE 8
#define UI_DICE_MS 650                          /* a throw's tumble */
int  ui_random(int n);
void ui_dice_throw(int n, const int *values, int player);
void ui_dice_show(int n, const int *values, int player);  /* dice set to these values at once: no tumble, no sound */
/* the same with a colour each (players[i]: die i's; backgammon's opening throw, a die each) */
void ui_dice_throw_each(int n, const int *values, const int *players);
void ui_dice_show_each(int n, const int *values, const int *players);
void ui_dice_draw(float cx, float cy, float size, unsigned used);
/* one die at rest, any player's, any value (a die waiting to be thrown, a past throw): alpha 0..1 */
void ui_die_at(float cx, float cy, float size, int player, int value, float alpha);
int  ui_dice_busy(void);                        /* 1 while they tumble */
/* one die anywhere: turned by deg, lifted (pixels: its shadow stays on the table), faded (alpha 0..1), darkened
 * (shade 0..1: 1 as it is; a die that cannot be used) */
void ui_die(float cx, float cy, float size, int player, int value, float deg, float lift, float alpha, float shade);
/* a die thrown t ms ago: tumbling (its faces flickering, turning, bouncing; i: which die, each spins its own way),
 * then at rest on value, tilted by deg; shade as ui_die's. 1 while it still tumbles. */
int  ui_die_thrown(float cx, float cy, float size, int player, int value, float deg, int i, int t, float shade);
/* the tumble's motion t ms after a throw (0..UI_DICE_MS), for a lot of the game's own (cowries, pyramid dice): how far
 * it has turned (degrees, added to its tilt) and how high it bounces (a part of its size) */
void ui_die_tumble(int t, int i, float *turn, float *lift);

/* the hand-off (VIEW_* games): the device passes to `player`. Once the last move has landed (busy() 0), the view
 * turns to them (ui.viewer, the bars follow); VIEW_HIDDEN first covers the screen (the table greyed under it, drawn
 * with ui.viewer -1: draw() must then show every hand hidden; their token in gold light, a "pass the device" picture,
 * a start button) until they tap start. The same
 * player again: nothing. A new game covers for turn() by itself; a game opened again from the menu shows the cover
 * first. Undo never reveals a hand: a VIEW_HIDDEN game's undo() stops at its last hand-off (its own turn only), and
 * the shell takes no undo while a hand-off waits. */
void ui_handoff(int player);
int  ui_handing(void);                          /* 1 while a hand-off waits or the cover is up (the game's input waits:
                                                 * the shell sends none) */

/* what the app keeps between two runs, on the device only (save.c: one small file, written whole then renamed, a
 * checksum; a damaged file is ignored): small values under a key, namespaced by the game ("logicgates/best"; the
 * shell's own: "shell/..."). Keys: 1..63 printable ASCII characters, no spaces; values up to 4096 bytes, kept as
 * bytes (write numbers wider than a byte in a fixed order: little-endian). ui_save_get copies at most size bytes
 * and returns how many (0: none kept; buf NULL: the value's length); ui_save_set keeps a copy (size 0: removed),
 * written to the file soon after. The picture mode keeps them in memory only, empty at first (--save DIR: that
 * folder's file), so its pictures never depend on a past run. */
int  ui_save_get(const char *key, void *buf, int size);
void ui_save_set(const char *key, const void *data, int size);
/* the open game's best for one of its levels (game.levels): its stars 0..3, or -1 not solved yet */
int  ui_level_best(int level);
/* the shell's star, d across centred on (cx, cy): gold (lit), or a hollow in the wood (not won) */
void ui_star(float cx, float cy, float d, int lit);
/* the stars for a puzzle solved in `moves` when it can be done in `par`: 3 at par (or par not known, 0), 2 up to half as
 * many again, else 1 */
int  ui_par_stars(int moves, int par);
/* the stars for a logic puzzle solved (the rule for all of them): 3 with no hint and no mistake, 2 with two of them
 * at most, else 1 */
int  ui_logic_stars(int hints, int mistakes);

/* sound.c */
void snd_init(void);
void snd_play(int s);
void snd_play_vol(int s, float volume);
void snd_play_at(int s, float volume, int delay_ms);
extern int snd_muted;

/* save.c (the shell's side of ui_save_*): open the store in a folder (NULL: the device's, SDL_GetPrefPath; the
 * picture mode opens none: memory only), write it out when something changed */
void save_open(const char *dir);
void save_flush(void);
const char *save_path(void);                    /* the file (NULL: memory only) */

#endif
