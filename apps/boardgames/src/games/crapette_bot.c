/*
 * Crapette's bot (bots.h): the race patience for two, played by a short search over the moves of its turn and a
 * heuristic order. Everything is on the table but the talons (face down for both players): the bot never looks at
 * them; a card it turns is a card it holds. Runs on the worker thread: it copies the game at entry and reads only
 * that copy. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   Its own turn: the moves that break no priority (a card to a foundation first, the crapette before the talon:
 *   the rules' own check), searched a few moves deep (house to house, loading the other's crapette and waste, the
 *   crapette, the talon card); the line that ends best is begun, one move a think(). A move between houses only
 *   begins a line that gets rid of a card (never rearranging for its own sake), and a house card is never loaded on
 *   the other's crapette when it could go straight back to a house (an endless swap). The worth of a position: the
 *   cards each player still has to get rid of, those of the crapette counting most; for hard, also the empty houses
 *   (they count most of the rest), what the other player could do next (their crapette's card free to go, cards of
 *   ours they could load on our crapette). With nothing worth doing it turns its talon's top card, and when that
 *   card has nowhere good to go it puts it on its waste (the turn ends).
 *   The other player's fault: the bot calls "Crapette!" at once (actor() names it while a fault is marked: the shell
 *   runs the bot for that seat even though the turn is the other's). The bot never breaks a priority itself, so a
 *   human never has a fault of the bot's to call.
 * Levels: easy looks one move ahead with some noise; medium two; hard four, with the other player's chances weighed
 * (a tenth of a second at worst on a PC; most moves are quick).
 * Its moves are the game's notation ("c-f3", "4-C", "h-2", "t", "d", "p", "x"), played by crapette_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/crapette.h"

#define CB_TURN_MOVES 80                        /* a turn's moves before the bot stops rearranging */

typedef struct {
    int me, level, depth;
    long nodes, budget;
    ai_rng *r;
} search;

static int red(int c) { return CARD_SUIT(c) == SUIT_HEARTS || CARD_SUIT(c) == SUIT_DIAMONDS; }

/* the cards a player still has to get rid of, weighted: the crapette's most */
static int load(const cpos *p, int q)
{
    return 10 * p->ncrap[q] + 2 * (p->ntalon[q] + p->nwaste[q]) + (q == p->turn && p->held != CARD_NONE ? 2 : 0);
}

/* the worth of a position for me (the player to move), at the end of my line */
static int eval(const cpos *p, int me, int level)
{
    int op = 1 - me;
    if (p->phase == CP_OVER) return p->winner == me ? 100000 : p->winner == op ? -100000 : 0;
    int v = 16 * (load(p, op) - load(p, me));   /* (a move costs 1: the shorter line among equals) */
    if (level < AI_HARD) return v;
    for (int k = 0; k < CR_COLS; k++) v += p->ncol[k] ? 0 : 4;   /* room to move */
    /* the other player's next turn: their crapette's card free to go; our crapette loaded with house cards */
    int oc = p->ncrap[op] ? p->crap[op][p->ncrap[op] - 1] : CARD_NONE;
    int mc = p->ncrap[me] ? p->crap[me][p->ncrap[me] - 1] : CARD_NONE;
    if (oc != CARD_NONE) {
        int free = 0;
        for (int f = 0; f < CR_FOUND && !free; f++) {
            int t = p->found[f];
            free = t == CARD_NONE ? CARD_RANK(oc) == RANK_ACE : CARD_SUIT(t) == CARD_SUIT(oc) &&
                                                               CARD_RANK(t) + 1 == CARD_RANK(oc);
        }
        for (int k = 0; k < CR_COLS && !free; k++) {
            if (!p->ncol[k]) { free = 1; break; }
            int t = p->col[k][p->ncol[k] - 1];
            free = CARD_RANK(t) == CARD_RANK(oc) + 1 && red(t) != red(oc);
        }
        v -= free ? 32 : 0;
    }
    if (mc != CARD_NONE)
        for (int k = 0; k < CR_COLS; k++) {
            if (!p->ncol[k]) continue;
            int t = p->col[k][p->ncol[k] - 1];
            if (CARD_SUIT(t) == CARD_SUIT(mc) && abs(CARD_RANK(t) - CARD_RANK(mc)) == 1) v -= 16;
        }
    return v;
}

/* a house card loaded on the other's crapette could go straight back to a house (theirs to play: an endless swap) */
static int back_home(const cpos *p, int s)
{
    int c = p->col[s][p->ncol[s] - 1];
    for (int k = 0; k < CR_COLS; k++) {
        int n = p->ncol[k] - (k == s);
        if (!n) return 1;
        int t = p->col[k][n - 1];
        if (CARD_RANK(t) == CARD_RANK(c) + 1 && red(t) != red(c)) return 1;
    }
    return 0;
}

/* the moves of the turn that break no priority (no talon turned, no discard), the likeliest first */
static int good_moves(const cpos *p, int *out)
{
    int all[600], n = crap_moves(p, all), k = 0;
    int score[600];
    for (int i = 0; i < n; i++) {
        int a = all[i];
        if (CA_KIND(a) != CA_MOVE || crap_fault_of(p, a) != CF_NONE) continue;
        int s = CA_SRC(a), d = CA_DST(a);
        /* a lone card from a house to an empty one changes nothing */
        if (s < CL_FOUND && d < CL_FOUND && p->ncol[s] == 1 && !p->ncol[d]) continue;
        if (s < CL_FOUND && d == CL_OCRAP && back_home(p, s)) continue;
        int w = d >= CL_FOUND && d < CL_CRAP ? 50 : d >= CL_OCRAP ? 40 : 0;
        w += s == CL_CRAP ? 30 : s == CL_HELD ? 20 : 0;
        score[k] = w;
        out[k++] = a;
    }
    for (int i = 1; i < k; i++)
        for (int j = i; j > 0 && score[j] > score[j - 1]; j--) {
            int t = score[j];
            score[j] = score[j - 1], score[j - 1] = t;
            t = out[j];
            out[j] = out[j - 1], out[j - 1] = t;
        }
    return k;
}

/* the best end of a line from p, `left` moves more at most (stopping is always a choice) */
static int dfs(search *S, const cpos *p, int left, cgame *scratch)
{
    int best = eval(p, S->me, S->level);
    if (!left || p->phase != CP_PLAY || ++S->nodes > S->budget) return best;
    int mv[600], n = good_moves(p, mv);
    for (int i = 0; i < n; i++) {
        scratch->pos = *p;
        scratch->n = 0;
        if (!crap_play(scratch, mv[i])) continue;
        cpos q = scratch->pos;
        int v = dfs(S, &q, left - 1, scratch) - 1;   /* (the shorter line among equals) */
        if (v > best) best = v;
    }
    return best;
}

/* the moves made in this turn so far */
static int turn_moves(const cgame *g)
{
    int k = 0;
    for (int i = g->n - 1; i >= 0; i--) {
        int a = g->act[i];
        if (CA_KIND(a) == CA_DISCARD || CA_KIND(a) == CA_PASS || CA_KIND(a) == CA_CALL) break;
        k++;
    }
    return k;
}

/* the action for seat at a level (0: nothing to do) */
int crapb_choose(const cgame *g, int seat, int level, uint64_t seed)
{
    const cpos *p = &g->pos;
    if (p->phase != CP_PLAY) return 0;
    if (p->fault != CF_NONE && seat != p->turn) return CA_CALL;
    if (seat != p->turn) return 0;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 23 + (uint64_t)seat));
    cgame *sc = malloc(sizeof *sc);
    if (!sc) return crap_auto(p);
    sc->seed = g->seed;
    int mv[600], n = good_moves(p, mv);
    search S = {.me = seat, .level = level, .depth = level == AI_EASY ? 1 : level == AI_MEDIUM ? 2 : 4,
                .budget = level == AI_HARD ? 60000 : 6000, .r = &r};
    int now = eval(p, seat, level), best = -(1 << 30), bm = 0, found = 0, bestc = -(1 << 30), bc = 0;
    int guard = turn_moves(g) > CB_TURN_MOVES;  /* (rearranging for ever: no more moves between houses) */
    for (int i = 0; i < n; i++) {
        int a = mv[i], s = CA_SRC(a), d = CA_DST(a), house = s < CL_FOUND && d < CL_FOUND;
        if (house && guard) continue;
        sc->pos = *p;
        sc->n = 0;
        if (!crap_play(sc, a)) continue;
        cpos q = sc->pos;
        int v = dfs(&S, &q, S.depth - 1, sc) - 1, to_found = d >= CL_FOUND && d < CL_CRAP;
        if (s == CL_CRAP && v > bestc) bestc = v, bc = a;
        /* a move between houses only on a line that gets rid of a card (no rearranging for its own sake) */
        if (!to_found && v <= now + (house ? 16 : 0)) continue;
        if (level == AI_EASY) v += ai_rand_n(&r, 24);
        if (to_found && !found) found = 1, best = v, bm = a;   /* a foundation move first, always */
        else if ((!found || to_found) && v > best) best = v, bm = a;
        if (ai_cancelled()) break;
    }
    free(sc);
    if (bm) return bm;
    if (p->held != CARD_NONE) return CA_DISCARD;
    if (crap_legal(p, CA_TURN) && crap_fault_of(p, CA_TURN) == CF_NONE) return CA_TURN;
    if (crap_legal(p, CA_PASS)) return CA_PASS;
    return bc ? bc : crap_auto(p);              /* the crapette must move before the talon: its best place */
}

static void place_name(int pl, char *o)
{
    if (pl < CL_FOUND) o[0] = (char)('0' + pl), o[1] = 0;
    else if (pl < CL_CRAP) o[0] = 'f', o[1] = (char)('0' + pl - CL_FOUND), o[2] = 0;
    else o[0] = pl == CL_CRAP ? 'c' : pl == CL_HELD ? 'h' : pl == CL_OCRAP ? 'C' : 'W', o[1] = 0;
}

/* an action in the notation */
void crapb_name(int a, char *m, int cap)
{
    char s[4], d[4];
    switch (CA_KIND(a)) {
    case CA_MOVE:
        place_name(CA_SRC(a), s);
        place_name(CA_DST(a), d);
        snprintf(m, (size_t)cap, "%s-%s", s, d);
        break;
    case CA_TURN: snprintf(m, (size_t)cap, "t"); break;
    case CA_DISCARD: snprintf(m, (size_t)cap, "d"); break;
    case CA_PASS: snprintf(m, (size_t)cap, "p"); break;
    default: snprintf(m, (size_t)cap, "x"); break;
    }
}

static int think(int seat, int level, char *move, int cap)
{
    cgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *crapette_live();                      /* the copy, at entry: nothing else is read */
    int a = crapb_choose(g, seat, level, ai_session_seed());
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    crapb_name(a, move, cap);
    return 1;
}

/* the player to move; while a fault is marked, the other player (who may call it) */
static int actor(void)
{
    const cpos *p = &crapette_live()->pos;
    if (p->phase != CP_PLAY) return -1;
    return p->fault != CF_NONE ? 1 - p->turn : p->turn;
}

static const bot crapette_bot = {.think = think, .actor = actor, .play = crapette_show};
REGISTER_BOT(crapette, &crapette_bot)
