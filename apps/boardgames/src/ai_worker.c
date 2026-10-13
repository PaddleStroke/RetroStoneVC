/*
 * RetroStone Board's bot worker (bots.h): one bot's think() on an SDL thread while the main thread draws. The only
 * SDL in the AI: the searches (ai.c) and the registry (bots.c) have none. Main thread only, one bot at a time.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <SDL.h>
#include <string.h>

#include "ai.h"
#include "bots.h"

static struct {
    SDL_Thread *thread;
    SDL_atomic_t finished;                      /* the worker sets it last: move and ok are written then */
    const bot *b;
    int seat, level, ok;
    char move[BOT_MOVE_CAP];
    Uint32 t0;
} W;

static int run(void *arg)
{
    (void)arg;
    char m[BOT_MOVE_CAP] = "";
    int ok = W.b->think(W.seat, W.level, m, (int)sizeof m);
    m[sizeof m - 1] = 0;
    memcpy(W.move, m, sizeof m);
    W.ok = ok && !ai_cancelled() && m[0];
    SDL_AtomicSet(&W.finished, 1);              /* a full barrier: the main thread then sees move and ok */
    return 0;
}

int bot_begin(const bot *b, int seat, int level)
{
    if (W.thread || !b || !b->think) return 0;
    ai_cancel_clear();
    W.b = b;
    W.seat = seat;
    W.level = level;
    W.ok = 0;
    W.move[0] = 0;
    W.t0 = SDL_GetTicks();
    SDL_AtomicSet(&W.finished, 0);
    W.thread = SDL_CreateThread(run, "bot", NULL);
    return W.thread != NULL;
}

int bot_thinking(void) { return W.thread != NULL; }
unsigned bot_ms(void) { return SDL_GetTicks() - W.t0; }   /* since the last bot_begin(), done or not */

int bot_done(char *move, int cap)
{
    if (!W.thread || !SDL_AtomicGet(&W.finished)) return 0;
    SDL_WaitThread(W.thread, NULL);
    W.thread = NULL;
    if (cap > 0) {
        SDL_strlcpy(move, W.ok ? W.move : "", (size_t)cap);
    }
    return 1;
}

void bot_halt(void)
{
    if (!W.thread) return;
    ai_cancel();
    SDL_WaitThread(W.thread, NULL);
    W.thread = NULL;
    ai_cancel_clear();
}
