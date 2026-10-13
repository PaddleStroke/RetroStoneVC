/*
 * RetroStone Board's AI kit: the searches every game's bot is built on, without SDL (the tests link it alone).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   ai_ab      negamax alpha-beta: iterative deepening, principal variation search, an optional aspiration window,
 *              move ordering (the table's move, the game's order(), two killers a ply, history), an optional
 *              transposition table (the game's hash), an optional quiescence (the game's noisy moves), chance nodes
 *              (expectiminimax: dice), a time / node / depth budget, the cancel flag, root noise for the easy levels.
 *   ai_expecti the same search for games with dice: the game gives chance() and the search averages the throws.
 *   ai_mcts    UCT (Monte Carlo tree search) for any number of players: random or heuristic playouts (playout()),
 *              playouts cut short by an estimate, and hidden information (determinise(): information-set MCTS, one
 *              observer: each iteration deals the unseen cards anew, consistently with what the seat to move knows).
 *   ai_rng     a small random generator (splitmix64): each search its own, no shared state.
 *   ai_level_budget   easy / medium / hard: milliseconds, nodes, depth and noise.
 *   ai_cancel  any thread: the search running stops at its next poll (every 1024 nodes) and returns its best so far.
 *
 * ---- One adapter, two searches ---------------------------------------------------------------------------------
 * A game fills one ai_game: its state is a plain struct (a copy of its rules' position, or the bot's own faster form
 * of it), copied with memcpy, so it holds no pointers to the game's live data. Moves are ints >= 0 in the game's own
 * encoding (a square, a column, from * 64 + to...). Each search uses the fields it needs:
 *   ai_ab:   size, moves, make, side, over, eval (+ unmake, hash, order, noisy, chance, unit)
 *   ai_mcts: size, moves, make, side, players, result or over (+ chance, playout, estimate, cutoff, determinise)
 *
 * Turns: the side to move comes from side(); negamax flips the score's sign only when the side changes, so a pass
 * (reversi), a capture that moves again (draughts) or several moves a turn need nothing special: they are moves
 * like the others (a pass the game must offer is a move; a position with no move and not over() is scored by eval()).
 * More than two players: ai_mcts (each player's reward); or ai_ab with the game's own "paranoid" side() (0 the bot,
 * 1 everyone else).
 *
 * ---- Threads ------------------------------------------------------------------------------------------------------
 * Nothing here is shared between calls except the cancel flag (an atomic, one for the app: one bot thinks at a time)
 * and the session seed (set by the main thread before it starts the worker): each search allocates its own tables
 * (a few MB with a transposition table) and frees them. A bot's think() runs on a worker thread while the main
 * thread draws: see bots.h for exactly what think() may touch.
 *
 * ---- The shell's pass (integration plan, see bots.h) ------------------------------------------------------------
 * The player picker's human / bot seats and level, bot_begin() when the actor is a bot and the game isn't busy(),
 * bot_done() each frame (then a minimum pause, then the move played on the main thread), bot_halt() on undo, a new
 * game, leaving and quitting.
 */
#ifndef AI_H
#define AI_H

#include <stddef.h>
#include <stdint.h>

#define AI_MAX_MOVES 512                        /* moves in one position, at most (go 19 x 19: 362) */
#define AI_MAX_PLY   96                         /* the deepest line, quiescence included */
#define AI_MAX_SEATS 8
#define AI_NONE      (-1)                       /* no move */

/* ---- scores (ai_ab): the side to move's view ----------------------------------------------------------------------
 * eval() is on the game's own scale (ai_game.unit: one unit of advantage, 100 by default), well under AI_WIN / 2 in
 * size; a game over is ai_win(margin) (won by margin, 0..AI_WIN / 4: reversi's discs) or -ai_win(margin), 0 a draw.
 * The search prefers the quicker win and the slower loss (it takes the ply off). */
#define AI_INF  (1 << 28)
#define AI_WIN  (1 << 26)
static inline int ai_win(int margin) { return AI_WIN + margin; }
static inline int ai_is_win(int score) { return score > AI_WIN / 2 || score < -AI_WIN / 2; }   /* won or lost */

/* ---- the random generator ------------------------------------------------------------------------------------- */
typedef struct { uint64_t s; } ai_rng;
void     ai_seed(ai_rng *r, uint64_t seed);
uint32_t ai_rand(ai_rng *r);
int      ai_rand_n(ai_rng *r, int n);           /* 0..n-1 (n >= 1) */
double   ai_rand_f(ai_rng *r);                  /* 0 <= x < 1 */
uint64_t ai_mix(uint64_t a, uint64_t b);        /* a hash of two words: seeds, a position's key */
/* the session's seed: the shell sets it (from ui_random(), on the main thread) before each think; a bot mixes it with
 * its position (ai_mix) so the same game repeats in the tests and the pictures, and varies in play */
void     ai_set_seed(uint64_t seed);
uint64_t ai_session_seed(void);

/* ---- cancel, clock ----------------------------------------------------------------------------------------------- */
void   ai_cancel(void);                         /* any thread: stop the search running (undo, new game, leaving) */
void   ai_cancel_clear(void);                   /* before a new think (bot_begin() does it) */
int    ai_cancelled(void);                      /* cheap (a relaxed atomic load): think() may poll it too */
double ai_now_ms(void);                         /* a monotonic clock, milliseconds */

/* ---- levels ------------------------------------------------------------------------------------------------------
 * The budget: whichever comes first stops the search (0: no limit). nodes is the deterministic limit (the same move
 * on every machine and in the tests); ms is the safety limit for a slow phone. A game calibrates nodes to its own
 * node rate (its test prints ms a move) so that hard takes about 0.3 s on a PC (about 1 s on a phone).
 *   easy:   a shallow search and a lot of noise: it sees a capture or a threat, misses deeper ideas, sometimes
 *           prefers a worse move (human-like mistakes); it beats a random player most of the time.
 *   medium: a few plies, a little noise: a good game for a casual player.
 *   hard:   as deep as the budget allows, no noise. */
enum { AI_EASY, AI_MEDIUM, AI_HARD, AI_LEVELS };
typedef struct {
    int  ms;                                    /* wall clock, milliseconds */
    long nodes;                                 /* nodes (ai_ab) or iterations (ai_mcts) */
    int  depth;                                 /* plies (ai_ab), 0: AI_MAX_PLY */
    int  noise;                                 /* random noise on the root's moves, in hundredths of ai_game.unit */
} ai_budget;
ai_budget ai_level_budget(int level);           /* AI_EASY .. AI_HARD (clamped) */

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
typedef struct ai_game {
    size_t size;                                /* the state's bytes */
    int  (*moves)(const void *st, int *out);    /* the side to move's moves (<= AI_MAX_MOVES); 0: none */
    void (*make)(void *st, int move);           /* plays a move (or a chance outcome) */
    int  (*side)(const void *st);               /* the player to move, 0..players-1 */
    /* the game over: 1, and *score from the side to move's view (ai_win(m), -ai_win(m), 0); NULL for ai_mcts if
     * result() is given */
    int  (*over)(const void *st, int *score);
    int  (*eval)(const void *st);               /* ai_ab: the side to move's view */
    /* optional */
    void (*unmake)(void *st, int move);         /* ai_ab: undoes make(); NULL: the search copies the state instead */
    uint64_t (*hash)(const void *st);           /* ai_ab: the transposition table's key (the side to move in it) */
    /* ai_ab: a move's promise, higher first (wins, captures, the centre), -100000..100000; then the killers and the
     * history order the moves it calls equal */
    int  (*order)(const void *st, int move);
    int  (*noisy)(const void *st, int *out);    /* ai_ab: the moves quiescence resolves past the horizon (captures) */
    /* a chance node (dice to throw, a card to turn): its outcomes (moves for make()) and their weights; 0: a
     * decision. ai_ab averages them (expectiminimax; a throw costs no depth); ai_mcts draws one by its weight. */
    int  (*chance)(const void *st, int *outcome, int *weight);
    int  unit;                                  /* ai_ab: eval's "one unit" (a pawn): the noise's scale; 0: 100 */
    int  players;                               /* ai_mcts: 2..AI_MAX_SEATS (0: 2) */
    /* ai_mcts: the game over: 1, and each player's reward 0..1 (a win 1, a loss 0, a draw 1 / winners); NULL: over()
     * with two players (its score's sign) */
    int  (*result)(const void *st, float *reward);
    int  (*playout)(const void *st, ai_rng *r); /* ai_mcts: a playout's move (a quick heuristic); NULL: uniform */
    /* ai_mcts: a playout stopped after `cutoff` moves (0: played to the end) is scored by estimate() (rewards 0..1;
     * NULL: a draw) */
    int  (*estimate)(const void *st, float *reward);
    int  cutoff;
    /* ai_mcts, hidden information: deals what `seat` cannot see (the others' hands, the stock) at random, consistently
     * with what that seat knows (cards played, suits shown void); called on a copy of the root each iteration. The
     * state given to the search must already be that seat's view: never a copy that the bot read the hidden cards
     * from. NULL: perfect information. */
    void (*determinise)(void *st, int seat, ai_rng *r);
} ai_game;

/* ---- the searches ------------------------------------------------------------------------------------------------ */
typedef struct {
    ai_budget budget;
    ai_rng *rng;                                /* the noise, ties, playouts (NULL: a fixed seed) */
    int tt_bits;                                /* ai_ab: a transposition table of 2^bits entries (24 bytes each); 0:
                                                 * none (also without hash()) */
    int aspiration;                             /* ai_ab: the half window round the last score (eval units); 0: off */
    float uct_c;                                /* ai_mcts: exploration (0: 0.7) */
} ai_opts;

typedef struct ai_stats {
    long nodes;                                 /* nodes searched (ai_mcts: iterations) */
    int depth;                                  /* ai_ab: the last depth searched completely */
    int score;                                  /* ai_ab: its score (side to move); ai_mcts: the move's win rate % */
    double ms;
    int stopped;                                /* 1: the budget or the cancel cut the search */
} ai_stats;

/* the move to play (AI_NONE: none, or the root is a chance node). Always a legal move when there is one, even
 * cancelled at once (the first by order). stats may be NULL. */
int ai_ab(const ai_game *g, const void *root, const ai_opts *o, ai_stats *stats);
static inline int ai_expecti(const ai_game *g, const void *root, const ai_opts *o, ai_stats *stats)
{
    return ai_ab(g, root, o, stats);            /* g->chance set: the throws are averaged */
}
int ai_mcts(const ai_game *g, const void *root, const ai_opts *o, ai_stats *stats);

/* a move picked at random among the legal ones (a random player in the tests; AI_NONE: none) */
int ai_random_move(const ai_game *g, const void *st, ai_rng *r);

#endif
