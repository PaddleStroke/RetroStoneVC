/*
 * RetroStone Board's bots: the registry (bots.c, no SDL) and the worker thread that runs one (ai_worker.c, SDL).
 * Kept out of the game struct, so the shell and the games stay apart: a game with a bot has a file of its own,
 * games/<id>_bot.c, that registers it with REGISTER_BOT(<id>, &bot). Built with the game (GAMES="<id>" too: the
 * Makefile takes src/games/<id>_*.c), linked as an object, so its constructor always runs.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * ---- A game's bot -----------------------------------------------------------------------------------------------
 *   static int think(int seat, int level, char *move, int cap);
 *       Runs on the WORKER THREAD. Writes the move for `seat` at `level` (AI_EASY..AI_HARD) in the game's own
 *       script notation (the same as --moves / game.script: "d3", "4", "e2e4"), NUL-terminated in cap bytes, and
 *       returns 1; 0 if cancelled (poll ai_cancelled(); the search does it itself) or if seat has nothing to do.
 *       Never stalls: with any move legal it returns one, at once if the budget is gone.
 *   static int actor(void);                 optional (MAIN THREAD): the seat that must act now (a bid, a discard, a
 *       simultaneous call), -1 none; NULL: game.turn().
 *   int play(const char *move);             optional (MAIN THREAD): plays the move with the game's usual animation and
 *       sounds, as if the human had tapped it; 0: refused. NULL: game.script() (instant).
 *
 * ---- What think() may touch (thread safety) --------------------------------------------------------------------
 * The main thread keeps drawing while think() runs; it never changes the game's rules state then (the shell sends no
 * input for a bot's seat, and cancels and joins the worker before undo, a new game, leaving or quitting). So think():
 *   - copies the rules state AT ENTRY into a local (rgame g = *reversi_live();), then reads only that copy;
 *   - calls only pure rules functions (<id>_rules.c: no SDL, no static state, nothing written but their arguments)
 *     and the AI kit (ai.h);
 *   - touches NO UI state: no SDL, no textures, no ui_* call, no ui_random() (take the seed from ai_session_seed()),
 *     no sound, no static variable of the game's UI file (animations, selections, the hand-off);
 *   - writes no static or global variable at all (it runs while the main thread runs); its own tables are locals or
 *     malloc'ed and freed before it returns;
 *   - sees only what `seat` may see: in a game of hidden hands it copies its own hand and the public table, and lets
 *     ai_mcts's determinise() deal the rest at random. No peeking at another hand, the stock or the deck's order.
 * The UI file exposes the live rules state read-only to its bot file (games/<id>.h: `const rgame *reversi_live(void)`)
 * and the animated play (`int reversi_show(const char *move)`); a headless test defines both itself.
 *
 * ---- The shell's pass (the integration plan) ---------------------------------------------------------------------
 *   - The picker: a human / bot choice per seat when bot_find(game.id) is not NULL, and a level (AI_EASY..AI_HARD),
 *     kept in the save store with the players' count.
 *   - Each frame in a game: if no bot is thinking, the game isn't busy(), no hand-off waits, status() is GS_PLAY and
 *     the actor (b->actor ? b->actor() : game.turn()) is a bot seat: ai_set_seed(ui_random-made seed);
 *     bot_begin(b, seat, level). Then bot_done(move, cap) each frame; when it returns 1 and at least the minimum
 *     pause has gone (bot_ms() >= about 600 ms, so humans can follow), play it on the main thread: b->play ?
 *     b->play(move) : game.script(move). A move refused (should never happen) or an empty move: log it, and pass
 *     the turn to the human seats (never loop).
 *   - Undo, a new game, home (leaving the game) and quitting: bot_halt() first (cancel + join), then the action (the
 *     rules page does not stop it: the move waits until the page closes). Undo in a game against bots goes back to
 *     the last human move (undo while the actor is a bot, until it is a human's).
 *   - Input for a bot's seat is ignored (press/motion/release not sent while the actor is a bot).
 *   - Hand-off covers only between HUMAN seats: ui_handoff(next) to a bot seat shows no cover and keeps the view on
 *     the last human (one human: always that human's view). Face to face: human seats only.
 */
#ifndef BOTS_H
#define BOTS_H

typedef struct bot {
    int (*think)(int seat, int level, char *move, int cap);
    int (*actor)(void);
    int (*play)(const char *move);
} bot;

#define BOT_MOVE_CAP 64                         /* a move's notation, at most (the worker's buffer) */
#define BOT_MAX 128

void bot_register(const char *id, const bot *b);
#define REGISTER_BOT(id, b) \
    __attribute__((constructor)) static void register_bot_##id(void) { bot_register(#id, b); }

const bot *bot_find(const char *id);            /* the game's bot, or NULL */
int bot_count(void);
const char *bot_id(int i);                      /* 0..bot_count()-1 */

/* ---- the worker (ai_worker.c, SDL threads; main thread only) ------------------------------------------------- */
int  bot_begin(const bot *b, int seat, int level);  /* clears the cancel flag, starts think(); 0: already thinking */
int  bot_done(char *move, int cap);             /* 1: finished (joined), the move in move ("" cancelled or none) */
int  bot_thinking(void);                        /* 1: started, not yet collected by bot_done() */
unsigned bot_ms(void);                          /* since the last bot_begin() (the pause after it is done) */
void bot_halt(void);                            /* ai_cancel() and join (undo, new game, leaving, quitting) */

#endif
