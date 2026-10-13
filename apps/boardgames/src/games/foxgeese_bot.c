/*
 * Fox and geese's bot (bots.h), for either side: alpha-beta (ai.h) over the rules' position, with quiescence over the
 * fox's jumps (a chain of jumps is a jump a move, the fox still to move). One evaluation, from the geese's side (the
 * side to move's view is its sign), with what each side plays for:
 *   the fox: geese taken (five win), geese it can jump now (two at once cannot both be covered), its room to run,
 *   and above all a way through the geese's line (behind it, the geese can never come back to trap it);
 *   the geese: the fox penned in (the points it can still reach, stepping through empty points, fewer and fewer until
 *   it cannot move at all) and kept in front of their line, the line moving up the board (a goose with a goose or the
 *   edge behind it cannot be jumped).
 * The turn: the fox's whole chain, searched jump by jump ("d4xd2xb2").
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("c3-c4" a step, "d4xd2" a jump, "d4xd2xb2" a chain), played by foxgeese_show().
 */
#include <stdio.h>

#include "bots.h"
#include "games/capture_bots.h"
#include "games/foxgeese.h"

static int encode(fgmove m) { return m.from | m.to << 6 | (m.over + 1) << 12; }
static fgmove decode(int m) { return (fgmove){(int8_t)(m & 63), (int8_t)(m >> 6 & 63), (int8_t)((m >> 12) - 1)}; }

typedef struct {
    fgpos p;                                    /* first: the adapter reads the position through st */
    cpb_rep rep;                                /* the positions before it, since the last capture or goose forwards */
} fgs;

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    fgmove l[FG_MAX_MOVES];
    int n = fg_legal(st, l);
    for (int i = 0; i < n; i++) out[i] = encode(l[i]);
    return n;
}

static uint64_t a_hash(const void *st)
{
    const fgpos *p = st;
    return cpb_hash(p->at, sizeof p->at, p->side | p->chain << 1);
}

static void a_make(void *st, int m)
{
    fgs *s = st;
    cpb_rep_push(&s->rep, a_hash(st));
    fg_make(&s->p, decode(m));
    if (!s->p.quiet) cpb_rep_clear(&s->rep);
}

static int a_side(const void *st) { return ((const fgpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const fgpos *p = st;
    int geese = p->side == FG_GOOSE;
    if (p->geese < FG_FOX_WINS) { *score = geese ? -ai_win(0) : ai_win(0); return 1; }
    if (p->chain) return 0;
    if (p->quiet >= FG_QUIET || cpb_rep_seen(&((const fgs *)st)->rep, a_hash(st))) { *score = 0; return 1; }
    fgmove l[FG_MAX_MOVES];
    if (fg_legal(p, l)) return 0;
    *score = -ai_win(0);                        /* no move: the fox trapped, or the geese stuck */
    return 1;
}

/* the geese's view; 100 about a goose */
static int geese_view(const fgpos *p)
{
    int v = 160 * p->geese, lowest = 7, sum = 0, above = 0, fy = fg_y[p->fox];
    /* the geese: up the board (slowly: the line first), backed (a goose with a goose or the edge behind it cannot be
     * jumped from the front), none past the fox */
    for (int i = 0; i < FG_POINTS; i++) {
        if (p->at[i] != FG_GOOSE) continue;
        int y = fg_y[i], back = fg_next[i][4];
        sum += y;
        above += y > fy;
        if (y < lowest) lowest = y;
        if (back < 0 || p->at[back] == FG_GOOSE) v += 6;
    }
    v += 2 * sum - 45 * above;                  /* a goose past the fox is out of the trap for good */
    /* the fox's room: the points it reaches stepping through empty points; those below the lowest goose are out of
     * the geese's reach for good (they never step back): a fox through the line cannot be trapped */
    uint64_t seen = 1ull << p->fox;
    int stack[FG_POINTS], top = 0, room = 0, behind = fg_y[p->fox] < lowest;
    stack[top++] = p->fox;
    while (top) {
        int a = stack[--top];
        for (int d = 0; d < 8; d++) {
            int b = fg_next[a][d];
            if (b < 0 || p->at[b] >= 0 || (seen >> b & 1)) continue;
            seen |= 1ull << b;
            room++;
            behind += fg_y[b] < lowest;
            stack[top++] = b;
        }
    }
    v -= 9 * room + (behind ? 300 + 20 * behind : 0);
    /* its jumps now */
    uint64_t victims = 0;
    for (int d = 0; d < 8; d++) {
        int over = fg_next[p->fox][d], to = over >= 0 ? fg_next[over][d] : -1;
        if (to >= 0 && p->at[over] == FG_GOOSE && p->at[to] < 0) victims |= 1ull << over;
    }
    int prey = __builtin_popcountll(victims);
    if (prey) {
        if (p->side == FG_FOX) v -= 40 + 20 * (prey - 1);     /* quiescence takes one: a little more */
        else v -= prey > 1 ? 170 + 60 * (prey - 2) : 30;      /* the geese cover one */
    }
    return v;
}

static int a_eval(const void *st)
{
    const fgpos *p = st;
    int v = geese_view(p);
    return p->side == FG_GOOSE ? v : -v;
}

static int a_order(const void *st, int m)       /* jumps first; geese: forwards first */
{
    const fgpos *p = st;
    fgmove x = decode(m);
    if (x.over >= 0) return 100;
    return p->side == FG_GOOSE ? fg_y[x.to] - fg_y[x.from] : 0;
}

static int a_noisy(const void *st, int *out)
{
    const fgpos *p = st;
    if (p->side != FG_FOX) return 0;
    fgmove l[FG_MAX_MOVES];
    int n = fg_legal(p, l), k = 0;
    for (int i = 0; i < n; i++)
        if (l[i].over >= 0) out[k++] = encode(l[i]);
    return k;
}

static const ai_game ADAPTER = {
    .size = sizeof(fgs), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

static const cpb_levels LEVELS = {
    .easy_depth = 2, .easy_nodes = 3000, .easy_noise = 100,
    .medium_depth = 4, .medium_nodes = 40000, .medium_noise = 25,
    .hard_nodes = 500000, .tt_bits = 18, .chains = 1,
};

/* the whole turn of the side to move in the game g (a fox's chain: several jumps), at a level: its moves (encoded) in
 * out; their count, 0: none. g is read at once (its position and the ones before it that may come again). */
int fgb_turn(const fggame *g, int level, uint64_t seed, int *out, int max, ai_stats *stats)
{
    fgs s = {.p = *fg_pos(g)};
    for (int i = g->ply - 1; i >= 0 && s.rep.n < CPB_REP && g->pos[i + 1].quiet; i--)
        cpb_rep_push(&s.rep, a_hash(&g->pos[i]));
    s.rep.ply = 0;                              /* the root: never a repetition itself */
    int score;
    if (a_over(&s, &score)) return 0;
    return cpb_turn(&ADAPTER, &s, &LEVELS, level, seed, out, max, stats);
}

/* a turn in the notation: "c3-c4", "d4xd2xb2" */
void fgb_write(const int *moves, int n, char *out, int cap)
{
    int k = 0;
    for (int i = 0; i < n && k + 6 < cap; i++) {
        fgmove x = decode(moves[i]);
        if (!i) k += snprintf(out + k, (size_t)(cap - k), "%c%c", 'a' + fg_x[x.from], '1' + fg_y[x.from]);
        k += snprintf(out + k, (size_t)(cap - k), "%c%c%c", x.over >= 0 ? 'x' : '-',
                      'a' + fg_x[x.to], '1' + fg_y[x.to]);
    }
}

static int think(int seat, int level, char *move, int cap)
{
    const fggame *g = foxgeese_live();          /* read at entry by fgb_turn(): nothing else is */
    int mv[CPB_TURN];
    if (fg_pos(g)->side != seat || cap < 6) return 0;
    int n = fgb_turn(g, level, ai_session_seed(), mv, CPB_TURN < (cap - 2) / 3 ? CPB_TURN : (cap - 2) / 3, NULL);
    if (!n || ai_cancelled()) return 0;
    fgb_write(mv, n, move, cap);
    return 1;
}

static const bot foxgeese_bot = {.think = think, .play = foxgeese_show};
REGISTER_BOT(foxgeese, &foxgeese_bot)
