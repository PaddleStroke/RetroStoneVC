/*
 * Mystery Face, the face-guessing game for two on one device: the rules. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 24 faces (our own drawings, mysteryface.c), each with a hair colour, a hair style, an eye colour, a skin tone and
 * some of: glasses, a hat, a beard, a moustache, earrings. Each player secretly picks a face (player 0 first, then
 * player 1; player 0 then asks first). A turn: ask ONE yes / no question about the other player's face (the device
 * answers it), and put down (or stand up again) any of your faces; then end the turn. Or, instead of a question, guess:
 * name a face. Right, you win; wrong, you lose at once. Undo takes back the faces put down in your own turn, never a
 * question nor a guess.
 * Notation (script; the screenshots and the tests), words separated by spaces: p<face> pick (0..23), e end the turn
 * (a pick confirmed), q<question> ask (0..15, MQ_*), f<face> put a face down or stand it up again, g<face> guess.
 * Faces count in reading order, from the top left.
 */
#ifndef MYSTERYFACE_H
#define MYSTERYFACE_H

#include <stdint.h>

#define MF_FACES 24
#define MF_MAX_ACTS 4096

enum { HAIR_BLACK, HAIR_BROWN, HAIR_BLOND, HAIR_RED, HAIR_WHITE, HAIRS };
enum { STYLE_LONG, STYLE_SHORT, STYLE_CURLY, STYLE_BALD, STYLES };
enum { EYE_BROWN, EYE_BLUE, EYE_GREEN, EYES };
enum { MF_GLASSES = 1, MF_HAT = 2, MF_BEARD = 4, MF_MOUSTACHE = 8, MF_EARRINGS = 16 };
#define SKINS 5

typedef struct { uint8_t hair, style, eyes, skin, has; } mf_face;
extern const mf_face MF_FACE[MF_FACES];

/* the questions: the picture buttons, in their order on the screen */
enum { MQ_HAT, MQ_GLASSES, MQ_BEARD, MQ_MOUSTACHE, MQ_EARRINGS, MQ_BALD, MQ_LONG, MQ_CURLY,
       MQ_BLACK, MQ_BROWN, MQ_BLOND, MQ_RED, MQ_WHITE, MQ_EYE_BROWN, MQ_EYE_BLUE, MQ_EYE_GREEN, MF_QUESTIONS };
int mf_has(int face, int question);             /* the answer: 1 yes, 0 no */

enum { MF_PICK, MF_PLAY, MF_OVER };
/* an action: its kind (high bits) and a face or a question (low bits) */
enum { MA_PICK = 1 << 5, MA_ASK = 2 << 5, MA_FLIP = 3 << 5, MA_GUESS = 4 << 5, MA_END = 5 << 5 };
#define MA_KIND(a) ((a) & 0xe0)
#define MA_ARG(a) ((a) & 0x1f)

typedef struct {
    uint8_t phase, turn;
    int8_t secret[2];                           /* each player's face (-1: not picked yet) */
    uint32_t down[2];                           /* bit f: face f put down on that player's board */
    int8_t asked, answer;                       /* this turn's question (-1: none yet) and its answer */
    int8_t winner, guess;                       /* MF_OVER: the winner; the face guessed */
    uint16_t questions[2];                      /* questions asked so far */
} mfpos;

typedef struct {
    mfpos pos;
    uint8_t act[MF_MAX_ACTS];
    int n;
} mfgame;

void mf_new(mfgame *g);
int  mf_legal(const mfpos *p, int act);
int  mf_play(mfgame *g, int act);               /* 0: not allowed */
/* undo: the last face put down (or stood up) in this turn, or the face picked; never a question nor a guess; 0: none */
int  mf_undo(mfgame *g);
int  mf_up(const mfpos *p, int player);         /* the faces still standing on the player's board */
int  mf_script(mfgame *g, const char *moves);   /* 0: an action is bad */

#endif
