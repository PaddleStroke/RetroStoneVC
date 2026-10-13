/*
 * The Royal Game of Ur's bot (bots.h): expectiminimax (ai_expecti) over the rules' position, the four dice a chance
 * node (0..4 marked tips up: 1, 4, 6, 4, 1 ways in 16). The evaluation weighs each piece's way along its path, the
 * central rosette held (safe, and it bars the other player's way), the pieces borne off, and the shared row's danger:
 * what the player throwing next can send back, by the chance of the number that reaches it.
 * The throw itself is a move: "roll", played by ur_show() with the game's own dice, as a tap throws them.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: a letter, the place of the piece moved ('a' a waiting piece, 'b'..'o' 1..14).
 */
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/ur.h"

static const int WAYS[5] = {1, 4, 6, 4, 1};    /* the throws 0..4, in 16 */

/* ---- the position: ur_throw and ur_move on a upos alone (no history) ------------------------------------------- */
void urb_throw(upos *p, int value)
{
    p->roll = (uint8_t)value;
    p->throws++;
    p->phase = UR_MOVE;
    if (ur_movable(p)) return;
    p->turn ^= 1;                               /* a 0, or nothing can move: lost */
    p->phase = UR_THROW;
}

void urb_move(upos *p, int from)
{
    int to = ur_dest(p, from), me = p->turn;
    if (to < 0) return;
    p->n[me][from]--;
    p->n[me][to]++;
    if (ur_shared(to) && p->n[!me][to]) {
        p->n[!me][to] = 0;
        p->n[!me][0]++;
    }
    if (p->n[me][UR_OFF] == UR_PIECES) {
        p->winner = (int8_t)me;
        p->phase = UR_OVER;
        return;
    }
    p->phase = UR_THROW;
    if (!ur_rosette(to)) p->turn ^= 1;
}

/* ---- the evaluation ---------------------------------------------------------------------------------------------- */
/* a piece's worth on a place: its way along the path, more once it is past the shared row's danger */
static int worth(int place)
{
    if (place == UR_OFF) return 190;
    if (place == 0) return 0;
    return 12 * place + (place > UR_SHARED_HI ? 12 : 0);
}

/* what `att` (to throw next) can expect to send back of `def`'s pieces: for each throw, the best piece it reaches */
static int threat(const upos *p, int att, int def)
{
    int sum = 0;
    for (int r = 1; r <= 4; r++) {
        int best = 0;
        for (int t = UR_SHARED_LO; t <= UR_SHARED_HI; t++) {
            if (!p->n[def][t] || t == UR_CENTRE) continue;
            int f = t - r;
            if (f >= 1 && p->n[att][f] && worth(t) + 20 > best) best = worth(t) + 20;
        }
        sum += WAYS[r] * best;
    }
    return sum / 16;
}

static int side_value(const upos *p, int s)
{
    int v = 0;
    for (int i = 0; i <= UR_OFF; i++) v += p->n[s][i] * worth(i);
    if (p->n[s][UR_CENTRE]) v += 30;            /* the central rosette: safe, and it bars the way */
    if (p->n[s][4]) v += 6;                     /* its own rosettes: another throw was had, a piece ready */
    if (p->n[s][14]) v += 4;
    return v;
}

/* the side to move's view (a throw to make, or a piece to move: the roll known) */
static int eval_pos(const upos *p)
{
    int me = p->turn, op = !me;
    int v = side_value(p, me) - side_value(p, op);
    if (p->phase == UR_MOVE && !ur_movable(p)) {   /* a lost throw: the other player throws next */
        v += threat(p, me, op) / 2 - threat(p, op, me);
    } else if (p->phase == UR_MOVE) {           /* the throw is known: the best capture now */
        int best = 0;
        for (int f = 0; f < UR_OFF; f++)
            if (ur_takes(p, f)) {
                int w = worth(ur_dest(p, f)) + 20;
                best = w > best ? w : best;
            }
        v += best - threat(p, op, me) / 2;
    } else {
        v += threat(p, me, op) - threat(p, op, me) / 2;   /* the next throw is mine; theirs comes after */
    }
    return v;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
/* In the search a throw that cannot be used is a move of its own, PASS: the rules pass the turn by themselves, but
 * a pass inside the chance node would cost no depth, and a run of lost throws (0, 0, 0...) would never end. */
#define PASS UR_OFF

static int a_moves(const void *st, int *out)
{
    const upos *p = st;
    if (p->phase != UR_MOVE) return 0;
    int n = 0, m = ur_movable(p);
    for (int f = 0; f < UR_OFF; f++)
        if (m >> f & 1) out[n++] = f;
    if (!n) out[n++] = PASS;
    return n;
}

static int a_chance(const void *st, int *out, int *w)
{
    const upos *p = st;
    if (p->phase != UR_THROW) return 0;
    for (int v = 0; v <= 4; v++) out[v] = v, w[v] = WAYS[v];
    return 5;
}

static void a_make(void *st, int m)
{
    upos *p = st;
    if (p->phase == UR_THROW) {               /* the throw: the move to choose (or PASS) comes next */
        p->roll = (uint8_t)m;
        p->phase = UR_MOVE;
    } else if (m == PASS) {
        p->turn ^= 1;
        p->phase = UR_THROW;
    } else {
        urb_move(p, m);
    }
}

static int a_side(const void *st) { return ((const upos *)st)->turn; }

static int a_over(const void *st, int *score)
{
    const upos *p = st;
    if (p->winner < 0) return 0;
    int margin = UR_PIECES - p->n[!p->winner][UR_OFF];
    *score = p->winner == p->turn ? ai_win(margin) : -ai_win(margin);
    return 1;
}

static int a_eval(const void *st) { return eval_pos(st); }

static uint64_t a_hash(const void *st)
{
    const upos *p = st;
    uint64_t a = 0, b = 0;
    for (int i = 0; i <= UR_OFF; i++) {
        a = a * 8 + p->n[0][i];
        b = b * 8 + p->n[1][i];
    }
    return ai_mix(ai_mix(a, b), (uint64_t)(p->turn | p->phase << 1 | p->roll << 3));
}

static int a_order(const void *st, int m)       /* captures, the central rosette, then the furthest piece */
{
    const upos *p = st;
    int to = ur_dest(p, m);
    return (ur_takes(p, m) ? 1000 : 0) + (to == UR_CENTRE ? 500 : ur_rosette(to) ? 300 : 0) + m;
}

static const ai_game ADAPTER = {
    .size = sizeof(upos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .chance = a_chance, .unit = 12,    /* unit: a square of the way */
};

/* the piece to move (its place) for the position p (a throw made), at a level; -1: none */
int urb_choose(const upos *p, int level, uint64_t seed, ai_stats *stats)
{
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 16};
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 1; o.budget.nodes = 2000; o.budget.noise = 500; break;
    case AI_MEDIUM: o.budget.depth = 3; o.budget.nodes = 20000; o.budget.noise = 60; break;
    default:        o.budget.depth = 0; o.budget.nodes = 600000; break;    /* about 0.05 s on a PC */
    }
    if (stats) memset(stats, 0, sizeof *stats);
    int mv[AI_MAX_MOVES];
    if (a_moves(p, mv) == 1) return mv[0];      /* nothing to choose */
    return ai_expecti(&ADAPTER, p, &o, stats);
}

static int think(int seat, int level, char *move, int cap)
{
    upos p = ur_live()->pos;                    /* the copy, at entry: nothing else is read */
    if (p.phase == UR_OVER || p.turn != seat || cap < 5) return 0;
    if (p.phase == UR_THROW) {                  /* the dice are the game's: the bot only throws them */
        strcpy(move, "roll");
        return 1;
    }
    int f = urb_choose(&p, level, ai_session_seed(), NULL);
    if (f < 0 || ai_cancelled()) return 0;
    move[0] = (char)('a' + f);
    move[1] = 0;
    return 1;
}

static const bot ur_bot = {.think = think, .play = ur_show};
REGISTER_BOT(ur, &ur_bot)
