/*
 * Word Tiles, the crossword tile game, for 2 to 4 players passing the device round: the rules. No SDL; the bag is
 * shuffled from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Our own game of letter tiles on a 15 x 15 board: our own layout of premium squares (wt_premium), and our own
 * letters for each language, their counts and values worked out from how often each letter appears in running text
 * (wordtiles_rules.c: 100 letters and 2 blanks). There is NO dictionary: the game never checks words, only where the
 * tiles go (one line, no gap, joined to the tiles already played, the first play through the centre star), and it
 * scores every word the play makes, with the premiums under the new tiles. The players keep each other honest: the
 * next player may challenge a play before making their own; the play is then taken back (its tiles back to their
 * owner's rack, still hidden, its score removed), its player has lost their turn and the challenger plays on.
 *
 * A turn: play tiles, exchange tiles (the bag holding at least 7), or pass. Seven tiles in one play: 50 more points.
 * The end: a player plays their last tile when the bag is empty (the next player may still challenge that play, or
 * accept it), or six scoreless turns in a row (passes, exchanges, plays taken back). Each player then loses the value
 * of the tiles left on their rack; the player who went out gains them all.
 *
 * Tiles: 0 none, 1..n a letter of the set (wset.l[tile - 1]), WT_BLANK a blank on a rack, WT_BLANK | letter a blank
 * on the board standing for that letter. Cells: row * 15 + column from the top left (112 the centre).
 * Notation (script; the screenshots and the tests): h8>WORD plays from h8 (columns a..o, rows 1..15 from the top)
 * across (">") or down ("v"), the letters of the word in order, those on the board already included; "?" before a
 * letter: a blank standing for it. xABC exchanges these tiles ("x?" a blank), "-" passes, "c" challenges the last
 * play, "a" accepts the last play (the end), "#" plays a random legal action (the tests).
 */
#ifndef WORDTILES_H
#define WORDTILES_H

#include <stdint.h>

#define WT_N 15                                 /* the board's side */
#define WT_CELLS (WT_N * WT_N)
#define WT_CENTRE (7 * WT_N + 7)
#define WT_RACK 7
#define WT_MAXP 4
#define WT_LETTERS 100                          /* letter tiles in a set, and two blanks */
#define WT_BLANKS 2
#define WT_TILES (WT_LETTERS + WT_BLANKS)
#define WT_BLANK 64
#define WT_LETTER(t) ((t) & 63)
#define WT_BINGO 50                             /* all seven tiles in one play */
#define WT_SCORELESS 6                          /* scoreless turns in a row: the end */
#define WT_MAX_ACTS 1024
#define WT_SET_MAX 48                           /* letters in a set, at most */

/* the premium squares: double and triple letter, double and triple word, the centre star (a double word) */
enum { WQ_NONE, WQ_DL, WQ_TL, WQ_DW, WQ_TW, WQ_STAR };
enum { WP_PLAY, WP_FINAL, WP_OVER };            /* WP_FINAL: the last tile is played, the next player may challenge */
enum { WA_PLAY = 1, WA_EXCHANGE, WA_CHALLENGE, WA_ACCEPT };   /* WA_EXCHANGE of no tile: a pass */
/* why a play is not allowed (wt_eval) */
enum { WE_OK, WE_TURN = -1, WE_TILES = -2, WE_CELL = -3, WE_LINE = -4, WE_GAP = -5, WE_CENTRE = -6, WE_ALONE = -7,
       WE_ONE = -8 };

typedef struct { uint32_t cp; uint8_t count, value; } wletter;
typedef struct { const char *lang; int n; wletter l[WT_SET_MAX]; } wset;

/* an action: a play (n tiles from rack slots to cells; a blank's letter), an exchange (n tiles from rack slots), a
 * challenge or an acceptance */
typedef struct {
    uint8_t kind, n;
    uint8_t slot[WT_RACK], cell[WT_RACK], letter[WT_RACK];
} wact;

typedef struct {
    uint8_t set, players, turn, phase;
    uint8_t board[WT_CELLS];
    uint8_t rack[WT_MAXP][WT_RACK];             /* 0: an empty slot */
    uint8_t bag[WT_TILES];                      /* the next tile drawn is the last */
    int16_t nbag;
    int16_t score[WT_MAXP], left[WT_MAXP];      /* left: the end's change (the tiles left on the racks) */
    uint8_t scoreless, ender;                   /* scoreless turns in a row; who went out (255: nobody) */
    struct {                                    /* the last action: who, what; a play's tiles (for the challenge) */
        uint8_t kind, player, n, scoreless;
        int16_t score;
        uint8_t cell[WT_RACK], slot[WT_RACK], tile[WT_RACK], drawn[WT_RACK];
    } last;
    uint16_t mixes;                             /* the bag's shuffles so far */
    uint32_t seed;
} wpos;

typedef struct {
    wpos pos;
    uint32_t seed;
    uint8_t set, players;
    wact act[WT_MAX_ACTS];
    int n;
} wgame;

/* a play's score: each word it makes (from cell `at`, across or down, `len` letters) and the total */
#define WT_WORDS 8
typedef struct {
    int score, words, bingo;
    struct { int at, down, len, score; } w[WT_WORDS];
} wscore;

int  wt_sets(void);
const wset *wt_set(int i);
int  wt_set_for(const char *lang);              /* the set of a language ("fr", "pt_BR"); English for the others */
int  wt_premium(int cell);                      /* WQ_* */
int  wt_value(const wpos *p, int tile);         /* a tile's points (blanks 0) */
uint32_t wt_char(const wpos *p, int tile);      /* the letter a tile shows (0: a blank on a rack) */
int  wt_rack_value(const wpos *p, int player);  /* the points of the tiles on a rack */
int  wt_rack_count(const wpos *p, int player);

void wt_new(wgame *g, int set, int players, uint32_t seed);
int  wt_eval(const wpos *p, const wact *a, wscore *s);   /* a play: its score (s filled), or WE_* below zero */
int  wt_legal(const wpos *p, const wact *a);
int  wt_play(wgame *g, const wact *a);          /* 0: not allowed */
int  wt_undo(wgame *g);                         /* back one action (a replay); 0: none */
int  wt_can_challenge(const wpos *p);
int  wt_can_exchange(const wpos *p);            /* the bag holds a rack's worth */
int  wt_winner(const wpos *p);                  /* WP_OVER: the best score (-1: a tie, or not over) */
unsigned wt_winners(const wpos *p);             /* WP_OVER: the players with the best score (bit n) */
void wt_arrange(wpos *p, int player, const uint8_t perm[WT_RACK]);   /* the rack's order (new slot i: old perm[i]) */
int  wt_auto(const wpos *p, uint32_t *rng, wact *a);   /* a random legal action (the tests, the "#" notation) */
int  wt_script(wgame *g, const char *moves);    /* 0: an action is bad */

#endif
