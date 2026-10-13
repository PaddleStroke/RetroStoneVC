/*
 * Lasca's bot (bots.h): alpha-beta (ai.h) over the rules' position, captures first and quiescence over them (a chain
 * of captures is a jump a move, the same player still to move: the turn is searched jump by jump and written whole,
 * "c3xe5xc7"). The evaluation, column by column: the commander (an officer worth more than a soldier), the player's
 * own pieces beneath it (they rise when it is taken: a tall column of one's own is hard to win), the prisoners it
 * holds (the other player's pieces out of play, freed if the column is taken), the soldiers' march to the far row,
 * and the mobility.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("c3-d4" a step, "c3xe5" a capture, "c3xe5xc7" a chain), played by lasca_show().
 */
#include <stdio.h>

#include "bots.h"
#include "games/capture_bots.h"
#include "games/lasca.h"

static int encode(lsmove m) { return m.from | m.to << 6 | (m.over >= 0) << 12; }
static lsmove decode(int m)
{
    int from = m & 63, to = m >> 6 & 63;
    return (lsmove){(int8_t)from, (int8_t)to, (int8_t)(m >> 12 ? (from + to) / 2 : -1)};
}

typedef struct {
    lspos p;                                    /* first: the adapter reads the position through st */
    cpb_rep rep;                                /* the positions before it, since the last capture or soldier's step */
} lss;

static uint64_t a_hash(const void *st);

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    lsmove l[LS_MAX_MOVES];
    int n = ls_legal(st, l);
    for (int i = 0; i < n; i++) out[i] = encode(l[i]);
    return n;
}

static void a_make(void *st, int m)
{
    lss *s = st;
    cpb_rep_push(&s->rep, a_hash(st));
    ls_make(&s->p, decode(m));
    if (!s->p.quiet) cpb_rep_clear(&s->rep);
}
static int a_side(const void *st) { return ((const lspos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const lspos *p = st;
    if (p->chain >= 0) return 0;
    if (p->quiet >= LS_QUIET || cpb_rep_seen(&((const lss *)st)->rep, a_hash(st))) { *score = 0; return 1; }
    lsmove l[LS_MAX_MOVES];
    if (ls_legal(p, l)) return 0;
    *score = -ai_win(0);                        /* no move: lost */
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const lspos *p = st;
    uint64_t h = (uint64_t)p->side | (uint64_t)(p->chain + 1) << 1;
    for (int s = 0; s < LS_SQUARES; s++)
        if (p->col[s].h) h = ai_mix(h, p->col[s].pc * 64 + p->col[s].h + ((uint64_t)s << 58));
    return ai_mix(h, p->jumped);
}

/* one player's columns, from their side; 100 a soldier in command */
static int side_view(const lspos *p, int pl)
{
    int v = 0;
    for (int s = 0; s < LS_SQUARES; s++) {
        lscol c = p->col[s];
        if (!c.h || LS_OWNER(ls_top(c)) != pl) continue;
        int top = ls_top(c), rank = pl ? LS_SIDE - 1 - ls_y(s) : ls_y(s);
        v += LS_OFFICER(top) ? 260 : 100 + 6 * rank;
        for (int i = 1; i < c.h; i++) {
            int pc = ls_piece(c, i);
            if (LS_OWNER(pc) == pl) v += LS_OFFICER(pc) ? 120 : 55;   /* reserves: they rise when it is taken */
            else v += LS_OFFICER(pc) ? 70 : 40;                         /* prisoners: out of the other's play */
        }
        int x = ls_x(s);
        if (x > 0 && x < LS_SIDE - 1) v += 4;   /* off the edge: room both ways */
    }
    return v;
}

/* the side to move's view */
static int a_eval(const void *st)
{
    const lspos *p = st;
    return side_view(p, p->side) - side_view(p, !p->side);
}

static int a_order(const void *st, int m)       /* captures first (of an officer, of a tall column first) */
{
    const lspos *p = st;
    lsmove x = decode(m);
    if (x.over < 0) return 0;
    lscol c = p->col[x.over];
    return 100 + 40 * LS_OFFICER(ls_top(c)) + c.h;
}

static int a_noisy(const void *st, int *out)
{
    const lspos *p = st;
    lsmove l[LS_MAX_MOVES];
    int n = ls_legal(p, l);
    if (!n || l[0].over < 0) return 0;          /* captures are compulsory: all of them, or none */
    for (int i = 0; i < n; i++) out[i] = encode(l[i]);
    return n;
}

static const ai_game ADAPTER = {
    .size = sizeof(lss), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

static const cpb_levels LEVELS = {
    .easy_depth = 2, .easy_nodes = 3000, .easy_noise = 100,
    .medium_depth = 4, .medium_nodes = 40000, .medium_noise = 25,
    .hard_nodes = 500000, .tt_bits = 18, .chains = 1,
};

/* the whole turn of the side to move in the game g (a chain: several captures), at a level: its moves (encoded)
 * in out; their count, 0: none. g is read at once (its position and the ones before it that may come again). */
int lsb_turn(const lsgame *g, int level, uint64_t seed, int *out, int max, ai_stats *stats)
{
    lss s = {.p = *ls_pos(g)};
    for (int i = g->ply - 1; i >= 0 && s.rep.n < CPB_REP && g->pos[i + 1].quiet; i--)
        cpb_rep_push(&s.rep, a_hash(&g->pos[i]));
    s.rep.ply = 0;                              /* the root: never a repetition itself */
    int score;
    if (a_over(&s, &score)) return 0;
    return cpb_turn(&ADAPTER, &s, &LEVELS, level, seed, out, max, stats);
}

/* a turn in the notation: "c3-d4", "c3xe5xc7" */
void lsb_write(const int *moves, int n, char *out, int cap)
{
    int k = 0;
    for (int i = 0; i < n && k + 6 < cap; i++) {
        lsmove x = decode(moves[i]);
        if (!i) k += snprintf(out + k, (size_t)(cap - k), "%c%c", 'a' + ls_x(x.from), '1' + ls_y(x.from));
        k += snprintf(out + k, (size_t)(cap - k), "%c%c%c", x.over >= 0 ? 'x' : '-',
                      'a' + ls_x(x.to), '1' + ls_y(x.to));
    }
}

static int think(int seat, int level, char *move, int cap)
{
    const lsgame *g = lasca_live();  /* read at entry by lsb_turn(): nothing else is */
    int mv[CPB_TURN];
    if (ls_pos(g)->side != seat || cap < 6) return 0;
    int n = lsb_turn(g, level, ai_session_seed(), mv, CPB_TURN < (cap - 2) / 3 ? CPB_TURN : (cap - 2) / 3, NULL);
    if (!n || ai_cancelled()) return 0;
    lsb_write(mv, n, move, cap);
    return 1;
}

static const bot lasca_bot = {.think = think, .play = lasca_show};
REGISTER_BOT(lasca, &lasca_bot)
