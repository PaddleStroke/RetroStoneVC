/*
 * Draughts' bot (bots.h), every rule set (international, English, Russian, Brazilian, Turkish, Frisian, Canadian,
 * bashni): alpha-beta (ai.h) over the rules' position and their own move generator (draughts_rules.c: a capture
 * sequence is one move there, so a turn is always one move), captures first and quiescence over them (capturing is
 * compulsory: a position with a capture to make is never judged before it is made). The evaluation: the men and the
 * kings (a flying king worth three men, an English king less than two), the men's march to the far row, the back
 * row kept while the other still has men to crown, the centre, and in bashni each column with what it holds (the
 * player's own pieces beneath its top rise when it is taken; the other's are prisoners, out of play). Ahead, it
 * trades down (the same lead counts more with fewer pieces left); a position seen before in the game is a draw to it,
 * so it makes progress when ahead.
 * Runs on the worker thread: it copies what it needs at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation (draughts.h): "32-28", "26x37x48" (every landing square: never ambiguous);
 * Russian, Brazilian, Turkish and bashni "c3-d4", "c3:e5:g3"; played by draughts_show().
 */
#include <stdio.h>

#include "bots.h"
#include "games/capture_bots.h"
#include "games/draughts.h"

/* the plies without a capture or a man moved that draw the game (as draughts_rules.c's table; Frisian: its own) */
static const uint8_t QUIET[DR_VARIANTS] = {50, 80, 30, 40, 50, 0, 50, 30};

typedef struct {
    dpos p;                                     /* first: the position */
    uint8_t variant, n;
    cpb_rep rep;                                /* the positions before it, since the last capture or man moved */
} dbs;

/* a move as an int: from, to, which of the moves between them (taking different pieces), pieces taken, crowned */
static int code(const dmove *m, int k)
{
    return m->from | m->path[m->n - 1] << 8 | k << 16 | m->ncap << 20 | m->crown << 26;
}

/* the legal moves, the same capture told once (two routes taking the same pieces to the same square), with their
 * codes; their count */
static int listing(const dbs *s, dmove *l, int *codes)
{
    int n = dr_legal(&s->p, s->variant, l), k = 0;
    for (int i = 0; i < n; i++) {
        int dup = 0, nth = 0, to = l[i].path[l[i].n - 1];
        for (int j = 0; j < k; j++)
            if (l[j].from == l[i].from && l[j].path[l[j].n - 1] == to) {
                dup |= dr_same(&l[j], &l[i]) && l[j].crown == l[i].crown;
                nth++;                          /* another capture between the same squares */
            }
        if (dup || nth > 15) continue;          /* (sixteen: never in a real game) */
        l[k] = l[i];
        codes[k] = code(&l[k], nth);
        k++;
    }
    return k;
}

/* 1: a piece of the side to move has a step to an empty square next to it (so it has a move) */
static int has_step(const dbs *s)
{
    static const int DX[8] = {1, -1, 1, -1, 1, -1, 0, 0}, DY[8] = {1, 1, -1, -1, 0, 0, 1, -1};
    int n = s->n, me = s->p.side, fwd = me ? -1 : 1, d0 = dr_all(s->variant) ? 4 : 0;
    for (int q = 0; q < n * n; q++) {
        int pc = s->p.sq[q];
        if (!pc || DR_OWNER(pc) != me) continue;
        for (int d = d0; d < d0 + 4; d++) {
            if (!DR_IS_KING(pc) && DY[d] == -fwd) continue;
            int x = q % n + DX[d], y = q / n + DY[d];
            if (x >= 0 && y >= 0 && x < n && y < n && !s->p.sq[y * n + x]) return 1;
        }
    }
    return 0;
}

static uint64_t key(const dpos *p, int n, int columns)
{
    uint64_t h = cpb_hash(p->sq, (size_t)n * n, p->side);
    if (columns)
        for (int q = 0; q < DR_COLUMNS; q++)
            if (p->nunder[q]) h = ai_mix(h, p->under[q] * 64 + p->nunder[q] + ((uint64_t)q << 58));
    return h;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    dmove l[DR_MAX_MOVES];
    return listing(st, l, out);
}

static uint64_t a_hash(const void *st)
{
    const dbs *s = st;
    return key(&s->p, s->n, dr_columns(s->variant));
}

static void a_make(void *st, int m)
{
    dbs *s = st;
    dmove l[DR_MAX_MOVES];
    int codes[DR_MAX_MOVES], n = listing(s, l, codes);
    for (int i = 0; i < n; i++)
        if (codes[i] == m) {
            cpb_rep_push(&s->rep, a_hash(st));
            dr_make(&s->p, s->variant, &l[i]);
            if (!s->p.quiet) cpb_rep_clear(&s->rep);
            return;
        }
}

static int a_side(const void *st) { return ((const dbs *)st)->p.side; }

static int a_over(const void *st, int *score)
{
    const dbs *s = st;
    const dpos *p = &s->p;
    int q = QUIET[s->variant];
    if ((q && p->quiet >= q) || cpb_rep_seen(&s->rep, a_hash(st))) { *score = 0; return 1; }
    if (s->variant == DR_FRISIAN && p->quiet >= 4) {   /* kings only: one each after two moves each; 2 to 1 after 7 */
        int kings[2] = {0, 0}, men = 0;
        for (int i = 0; i < s->n * s->n; i++)
            if (p->sq[i]) {
                if (DR_IS_KING(p->sq[i])) kings[DR_OWNER(p->sq[i])]++;
                else men++;
            }
        if (!men && kings[0] && kings[1] && (kings[0] + kings[1] == 2 || (kings[0] + kings[1] == 3 && p->quiet >= 14)))
            { *score = 0; return 1; }
    }
    if (has_step(s)) return 0;
    dmove l[DR_MAX_MOVES];
    if (dr_legal(p, s->variant, l)) return 0;
    *score = -ai_win(0);                        /* no move (or no piece): lost */
    return 1;
}

/* one player's pieces, from their side; 100 a man */
static int army(const dbs *s, int pl, int king, int foe_men, int *material)
{
    const dpos *p = &s->p;
    int n = s->n, v = 0, columns = dr_columns(s->variant);
    *material = 0;
    for (int q = 0; q < n * n; q++) {
        int pc = p->sq[q];
        if (!pc || DR_OWNER(pc) != pl) continue;
        int x = q % n, r = pl ? n - 1 - q / n : q / n;
        if (DR_IS_KING(pc)) *material += king;
        else {
            *material += 100;
            v += 3 * r + (r == n - 2 ? 15 : 0);  /* the march: one row from a crown */
            if (!r && foe_men) v += 8;          /* the back row: no crown for the other there */
            if (x >= n / 2 - 2 && x <= (n - 1) / 2 + 2) v += 2;
        }
        if (columns && q < DR_COLUMNS)          /* bashni: what the column holds */
            for (int i = 0; i < p->nunder[q]; i++) {
                int u = (int)(p->under[q] >> (2 * i) & 3), mine = (u & 1) == pl, k = u >> 1;
                v += mine ? (k ? 120 : 50) : (k ? 70 : 35);
            }
    }
    return v;
}

/* the side to move's view */
static int a_eval(const void *st)
{
    const dbs *s = st;
    int me = s->p.side, men[2] = {0, 0};
    for (int q = 0; q < s->n * s->n; q++)
        if (s->p.sq[q] && !DR_IS_KING(s->p.sq[q])) men[DR_OWNER(s->p.sq[q])]++;
    int king = s->variant == DR_ENGLISH ? 160 : s->variant == DR_TURKISH ? 250 : 300, mm, mo;
    int v = army(s, me, king, men[!me], &mm) - army(s, !me, king, men[me], &mo);
    int total = mm + mo, start = 200 * dr_pieces(s->variant);
    v += (mm - mo) + (mm - mo) * (start - total) / (total + 200);   /* the lead, more as the pieces go */
    return v;
}

static int a_order(const void *st, int m)       /* the most pieces taken, a crown */
{
    (void)st;
    return (m >> 20 & 63) * 10 + (m >> 26 & 1) * 5;
}

static int a_noisy(const void *st, int *out)
{
    dmove l[DR_MAX_MOVES];
    int n = listing(st, l, out);
    return n && l[0].ncap ? n : 0;              /* capturing is compulsory: all the moves, or none */
}

static const ai_game ADAPTER = {
    .size = sizeof(dbs), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

static const cpb_levels LEVELS = {
    .easy_depth = 2, .easy_nodes = 3000, .easy_noise = 100,
    .medium_depth = 4, .medium_nodes = 30000, .medium_noise = 25,
    .hard_nodes = 200000, .tt_bits = 18,
};

/* the move in the game g (its side to move) at a level, in out; 0: none. g is read at once (its position and the
 * ones before it that may come again). */
int dgb_choose(const dgame *g, int level, uint64_t seed, dmove *out, ai_stats *stats)
{
    dbs s = {.p = *dr_pos(g), .variant = (uint8_t)g->variant, .n = (uint8_t)dr_size(g->variant)};
    int columns = dr_columns(g->variant), m, score;
    for (int i = g->ply - 1; i >= 0 && s.rep.n < CPB_REP && g->pos[i + 1].quiet; i--)
        cpb_rep_push(&s.rep, key(&g->pos[i], s.n, columns));
    s.rep.ply = 0;                              /* the root: never a repetition itself */
    if (a_over(&s, &score) || !cpb_turn(&ADAPTER, &s, &LEVELS, level, seed, &m, 1, stats)) return 0;
    dbs r = {.p = *dr_pos(g), .variant = s.variant, .n = s.n};
    dmove l[DR_MAX_MOVES];
    int codes[DR_MAX_MOVES], n = listing(&r, l, codes);
    for (int i = 0; i < n; i++)
        if (codes[i] == m) { *out = l[i]; return 1; }
    return 0;
}

/* a square in the variant's notation: "32", "c3" */
static int square(int variant, int q, char *out, int cap)
{
    int n = dr_size(variant), num = dr_number(variant, q);
    return num > 0 ? snprintf(out, (size_t)cap, "%d", num) : snprintf(out, (size_t)cap, "%c%d", 'a' + q % n, q / n + 1);
}

/* a move in the notation: every landing square ("26x37x48", "c3:e5:g3"); from and end only if that is too long */
void dgb_write(int variant, const dmove *m, char *out, int cap)
{
    char sep = !m->ncap ? '-' : dr_number(variant, m->from) > 0 ? 'x' : ':';
    int k = square(variant, m->from, out, cap);
    for (int i = 0; i < m->n && k < cap; i++) {
        if (k + 5 >= cap) {                     /* no room for the route: from and end (the first such capture) */
            k = square(variant, m->from, out, cap);
            out[k++] = sep;
            square(variant, m->path[m->n - 1], out + k, cap - k);
            return;
        }
        out[k++] = sep;
        k += square(variant, m->path[i], out + k, cap - k);
    }
}

static int think(int seat, int level, char *move, int cap)
{
    const dgame *g = draughts_live();           /* read at entry by dgb_choose() and here: nothing else is */
    int variant = g->variant;
    dmove m;
    if (dr_pos(g)->side != seat || cap < 8) return 0;
    if (!dgb_choose(g, level, ai_session_seed(), &m, NULL) || ai_cancelled()) return 0;
    dgb_write(variant, &m, move, cap);
    return 1;
}

static const bot draughts_bot = {.think = think, .play = draughts_show};
REGISTER_BOT(draughts, &draughts_bot)
