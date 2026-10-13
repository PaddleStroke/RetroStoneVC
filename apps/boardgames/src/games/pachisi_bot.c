/*
 * Pachisi's bot (bots.h), two players (two armies each) or four (partners opposite). The throw is a move: "roll",
 * played by pachisi_show() with the game's own cowries, as a tap throws them; after a throw the bot chooses the
 * piece, or declines the throw ('-') when every move would do more harm than good.
 *   easy    a quick player's sense: captures (another throw), a castle reached, a piece in reach of an enemy moved
 *           on, the furthest piece home; a choice now and then that is merely good;
 *   medium  the position after each choice weighed: every piece's way, its danger (the chance, by the cowries' odds,
 *           that an enemy throwing before our next turn lands on it: from the track behind it, or leaving the
 *           charkoni with a 10 or a 25, and after a grace), the castles and the arms' middle columns where nothing
 *           can catch it, the pieces still to come out (a grace each), captures by what they cost the enemy;
 *   hard    the same weighing, a few turns ahead: expectiminimax (ai_expecti) over the throws (the cowries' odds, the
 *           graces and captures that throw again), the enemy answering with its best.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: "roll", '-' (declined), or the piece's letter ("a".."h", four players "a".."d").
 */
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/pachisi.h"

#define DECLINE (2 * PA_PIECES)                 /* the choice not to move */

static const int ODDS[7] = {1, 6, 15, 20, 15, 6, 1};   /* the mouths up 0..6, in 64: the throws 25, 10, 2..6 */

/* ---- the position: pa_throw, pa_move and pa_decline on a ppos alone (no history) ------------------------------ */
static void next(ppos *p, int again)
{
    p->phase = PA_THROW;
    if (!again) p->turn = (uint8_t)((p->turn + 1) % p->players);
}

void pcb_throw(ppos *p, int mouths)
{
    p->roll = (uint8_t)pa_value(mouths);
    p->throws++;
    p->phase = PA_MOVE;
    if (pa_movable(p)) return;
    next(p, pa_grace(p->roll));
}

void pcb_move(ppos *p, int piece)
{
    int to = pa_dest(p, piece), me = p->turn, caught = 0;
    if (to < 0) return;
    int a = pa_army(p, piece), sq = pa_square(a, to);
    p->at[a][piece % PA_PIECES] = (uint8_t)to;
    p->out |= (uint8_t)(1 << a);
    if (!pa_castle(sq))
        for (int e = !(me & 1); e < PA_ARMIES; e += 2)
            for (int i = 0; i < PA_PIECES; i++)
                if (sq >= 0 && pa_square(e, p->at[e][i]) == sq) {
                    p->at[e][i] = 0;
                    caught++;
                }
    if (pa_home(p, me & 1) == 2 * PA_PIECES) {
        p->winner = (int8_t)me;
        p->phase = PA_OVER;
    } else {
        next(p, pa_grace(p->roll) || caught);
    }
}

void pcb_decline(ppos *p) { next(p, pa_grace(p->roll)); }

static void choose(ppos *p, int c)
{
    if (c == DECLINE) pcb_decline(p);
    else pcb_move(p, c);
}

/* ---- what a piece is worth, and its danger ----------------------------------------------------------------------- */
#define OUT 14                                  /* out of the charkoni (in squares of the way) */
#define HOME_BONUS 10

static int worth(int place)
{
    if (place == 0) return 0;
    if (place == PA_HOME) return OUT + PA_HOME + HOME_BONUS;
    return OUT + place;
}

/* the throws (in 4096ths) that take an enemy piece d squares along the track: one throw (25, 10, 2..6 by the
 * cowries' odds), or a grace (6, 10, 25) and then a throw */
static const int16_t ODDS_D[PA_LAP] = {
    [2] = 960, [3] = 1280, [4] = 960, [5] = 384, [6] = 64, [8] = 15, [9] = 20, [10] = 399, [11] = 6, [12] = 91,
    [13] = 120, [14] = 90, [15] = 36, [16] = 12, [20] = 36, [25] = 64, [27] = 15, [28] = 20, [29] = 15, [30] = 6,
    [31] = 2, [35] = 12, [50] = 1,
};

static int odds_of(int d) { return d > 0 && d < PA_LAP ? ODDS_D[d] : 0; }

/* the chance (in 4096ths) that the enemy side (armies e, e + 2), one turn, lands on track square sq */
static int reach(const ppos *p, int e, int sq)
{
    int best = 0, chark = 0;
    char seen[PA_LAP] = {0};
    for (int a = e; a < PA_ARMIES; a += 2)
        for (int i = 0; i < PA_PIECES; i++) {
            int at = p->at[a][i];
            if (at == 0) {                      /* leaving the charkoni: 10 or 25 land on the track */
                chark |= 1 << a;
                continue;
            }
            int s = pa_square(a, at);
            if (s < 0 || s >= PA_LAP) continue;
            int d = (sq - s + PA_LAP) % PA_LAP;
            if (d == 0 || at + d > PA_UP || seen[d]) continue;
            seen[d] = 1;
            best += odds_of(d);
        }
    for (int a = e; a < PA_ARMIES; a += 2)
        if (chark >> a & 1)
            for (int v = 10; v <= 25; v += 15)
                if (pa_square(a, v) == sq) best += 64 * ODDS[v == 25 ? 0 : 1];
    return best > 4096 ? 4096 : best;
}

/* a piece's danger (in 4096ths): on the track, off a castle */
static int danger(const ppos *p, int army, int at)
{
    int sq = pa_square(army, at);
    if (sq < 0 || sq >= PA_LAP || pa_castle(sq)) return 0;
    int e = !(army & 1), r = reach(p, e, sq);
    if (p->players == 4) {                      /* four: the two enemies throw before our next turn */
        int r2 = r;                             /* both their armies are in r already; a second throw, roughly */
        r = 4096 - (4096 - r) * (4096 - r2 / 2) / 4096;
    }
    return r;
}

/* a side's pieces (armies s, s + 2), in tenths of a square, their danger weighed */
static int side_value(const ppos *p, int s, int careful)
{
    int v = 0;
    for (int a = s; a < PA_ARMIES; a += 2) {
        int waiting = 0;
        for (int i = 0; i < PA_PIECES; i++) {
            int at = p->at[a][i], w = worth(at);
            v += 10 * w;
            if (careful && at) v -= 10 * danger(p, a, at) * (w + 6) / 4096;
            waiting += at == 0;
        }
        if (waiting && (p->out >> a & 1)) v -= 15 * waiting;   /* each still needs a grace to come out */
    }
    return v;
}

/* the position for player me's side */
static int judge(const ppos *p, int me, int careful)
{
    if (p->winner >= 0) return (p->winner & 1) == (me & 1) ? 1000000 : -1000000;
    return side_value(p, me & 1, careful) - side_value(p, !(me & 1), careful);
}

/* ---- the choices ------------------------------------------------------------------------------------------------- */
/* the mover's choices, one for each army and place (two pieces together move alike), and declining */
static int choices(const ppos *p, int *out)
{
    int n = 0, m = pa_movable(p), key[2 * PA_PIECES + 1];
    for (int i = 0; i < 2 * PA_PIECES; i++) {
        if (!(m >> i & 1)) continue;
        int a = pa_army(p, i), k = a * 128 + p->at[a][i % PA_PIECES], dup = 0;
        for (int j = 0; j < n; j++) dup |= key[j] == k;
        if (dup) continue;
        key[n] = k;
        out[n++] = i;
    }
    if (p->phase == PA_MOVE) out[n++] = DECLINE;
    return n;
}

/* a choice by rule of thumb: what it catches, home, a castle or the arm reached, a piece in danger moved on, the
 * danger it walks into; declining only when it has nothing better */
static int quick_score(const ppos *p, int c)
{
    if (c == DECLINE) return -40;
    int me = p->turn, a = pa_army(p, c), at = p->at[a][c % PA_PIECES], to = pa_dest(p, c), s = 0;
    int caught = pa_takes(p, c), e = !(me & 1);
    if (caught) {
        int sq = pa_square(a, to);
        for (int x = e; x < PA_ARMIES; x += 2)
            for (int i = 0; i < PA_PIECES; i++)
                if (pa_square(x, p->at[x][i]) == sq) s += 30 + worth(p->at[x][i]);
    }
    if (to == PA_HOME) s += 25;
    if (at == 0) s += 12;
    int sq0 = pa_square(a, at), sq1 = pa_square(a, to);
    int was = at == 0 || sq0 >= PA_LAP || pa_castle(sq0), will = sq1 < 0 || sq1 >= PA_LAP || pa_castle(sq1);
    if (!was) s += reach(p, e, sq0) * (worth(at) + 6) / 3000;
    if (!will) s -= reach(p, e, sq1) * (worth(to) + 6) / 3000;
    else if (!was) s += 6;
    return 4 * s + to / 10;
}

static int quick_move(const ppos *p, ai_rng *r, int noise)
{
    int c[2 * PA_PIECES + 1], n = choices(p, c), best = c[0], bs = 0;
    for (int i = 0; i < n; i++) {
        int s = quick_score(p, c[i]) + (noise ? ai_rand_n(r, noise) : 0);
        if (i == 0 || s > bs) best = c[i], bs = s;
    }
    return best;
}

static int medium_move(const ppos *p, ai_rng *r, int noise)
{
    int c[2 * PA_PIECES + 1], n = choices(p, c), best = c[0], bs = 0, me = p->turn;
    for (int i = 0; i < n; i++) {
        ppos q = *p;
        choose(&q, c[i]);
        int s = judge(&q, me, 1) + (noise ? ai_rand_n(r, noise) : 0);
        if (i == 0 || s > bs) best = c[i], bs = s;
    }
    return best;
}

/* ---- hard: expectiminimax (ai_expecti), the cowries a chance node -------------------------------------------- */
/* In the search a throw does not pass the turn by itself: the choices follow it, declining among them (a throw no
 * piece can use: declining alone), so a run of lost throws costs depth like any turn. The sides: player & 1. */
static int a_moves(const void *st, int *out)
{
    const ppos *p = st;
    return p->phase == PA_MOVE ? choices(p, out) : 0;
}

static int a_chance(const void *st, int *out, int *w)
{
    const ppos *p = st;
    if (p->phase != PA_THROW) return 0;
    for (int m = 0; m <= 6; m++) out[m] = m, w[m] = ODDS[m];
    return 7;
}

static void a_make(void *st, int m)
{
    ppos *p = st;
    if (p->phase == PA_THROW) {
        p->roll = (uint8_t)pa_value(m);
        p->throws++;
        p->phase = PA_MOVE;
    } else {
        choose(p, m);
    }
}

static int a_side(const void *st) { return ((const ppos *)st)->turn & 1; }

static int a_over(const void *st, int *score)
{
    const ppos *p = st;
    if (p->winner < 0) return 0;
    int lost = 2 * PA_PIECES - pa_home(p, !(p->winner & 1));
    *score = (p->winner & 1) == (p->turn & 1) ? ai_win(lost) : -ai_win(lost);
    return 1;
}

static int a_eval(const void *st)
{
    const ppos *p = st;
    return judge(p, p->turn, 1);
}

static uint64_t a_hash(const void *st)
{
    const ppos *p = st;
    uint64_t a, b;
    memcpy(&a, p->at[0], 8);
    memcpy(&b, p->at[2], 8);
    return ai_mix(ai_mix(a, b), (uint64_t)(p->turn | p->phase << 2 | p->roll << 4 | (uint64_t)p->out << 12));
}

static int a_order(const void *st, int m)       /* captures first, then the quick player's sense */
{
    const ppos *p = st;
    return m == DECLINE ? -100 : 20 * pa_takes(p, m) + quick_score(p, m) / 8;
}

static const ai_game ADAPTER = {
    .size = sizeof(ppos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .chance = a_chance, .unit = 10,   /* unit: a square of the way */
};

/* the choice (a piece 0..7, or DECLINE) for the position p (a throw made), at a level; -1: none */
int pcb_choose(const ppos *p, int level, uint64_t seed, ai_stats *stats)
{
    if (stats) memset(stats, 0, sizeof *stats);
    if (p->phase != PA_MOVE) return -1;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, a_hash(p)));
    double t0 = ai_now_ms();
    int c;
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   c = ai_rand_n(&r, 4) ? quick_move(p, &r, 60) : medium_move(p, &r, 3000); break;
    case AI_MEDIUM: c = medium_move(p, &r, 30); break;
    default: {
        int mv[2 * PA_PIECES + 1];
        if (choices(p, mv) == 1) return mv[0];
        ai_opts o = {.budget = ai_level_budget(AI_HARD), .rng = &r, .tt_bits = 16};
        o.budget.depth = 0;
        o.budget.nodes = 150000;
        c = ai_expecti(&ADAPTER, p, &o, stats);
        return c;
    }
    }
    if (stats) stats->ms = ai_now_ms() - t0;
    return c;
}


static int think(int seat, int level, char *move, int cap)
{
    ppos p = pachisi_live()->pos;               /* the copy, at entry: nothing else is read */
    if (p.phase == PA_OVER || p.turn != seat || cap < 5) return 0;
    if (p.phase == PA_THROW) {                  /* the cowries are the game's: the bot only throws them */
        strcpy(move, "roll");
        return 1;
    }
    int c = pcb_choose(&p, level, ai_session_seed(), NULL);
    if (c < 0 || ai_cancelled()) return 0;
    move[0] = c == DECLINE ? '-' : (char)('a' + c);
    move[1] = 0;
    return 1;
}

static const bot pachisi_bot = {.think = think, .play = pachisi_show};
REGISTER_BOT(pachisi, &pachisi_bot)
