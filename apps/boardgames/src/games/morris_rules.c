/*
 * The men's morris games: the rules (no SDL; tested by tests/morris_test.c). The boards, the notation and the rules:
 * morris.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/morris.h"

#define NINE_XY {0, 3, 6, 1, 3, 5, 2, 3, 4, 0, 1, 2, 4, 5, 6, 2, 3, 4, 1, 3, 5, 0, 3, 6}, \
                {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6}
#define NINE_LINES \
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {9, 10, 11}, {12, 13, 14}, {15, 16, 17}, {18, 19, 20}, {21, 22, 23},   /* rows */ \
    {0, 9, 21}, {3, 10, 18}, {6, 11, 15}, {1, 4, 7}, {16, 19, 22}, {8, 12, 17}, {5, 13, 20}, {2, 14, 23} /* columns */

static const mboard BOARDS[MORRIS_VARIANTS] = {
    [MORRIS_NINE] = {7, 24, 16, 9, 1, NINE_XY, {NINE_LINES}},
    [MORRIS_TWELVE] = {7, 24, 20, 12, 1, NINE_XY,
                       {NINE_LINES, {0, 3, 6}, {2, 5, 8}, {21, 18, 15}, {23, 20, 17}}},   /* the corners' diagonals */
    [MORRIS_SIX] = {5, 16, 12, 6, 0,
                    {0, 2, 4, 1, 2, 3, 0, 1, 3, 4, 1, 2, 3, 0, 2, 4}, {0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4},
                    {{0, 1, 2}, {3, 4, 5}, {10, 11, 12}, {13, 14, 15}, {0, 6, 13}, {3, 7, 10}, {5, 8, 12}, {2, 9, 15},
                     {1, 4, -1}, {6, 7, -1}, {8, 9, -1}, {11, 14, -1}}},   /* the squares' sides, then between them */
};

const mboard *morris_board(int variant)
{
    return &BOARDS[variant >= 0 && variant < MORRIS_VARIANTS ? variant : MORRIS_NINE];
}

static const mboard *board_of(const mpos *p) { return morris_board(p->variant); }

/* two points next to each other on a line */
static int adjacent(const mboard *b, int a, int c)
{
    for (int i = 0; i < b->lines; i++) {
        const int8_t *l = b->line[i];
        for (int j = 0; j < 2 && l[j + 1] >= 0; j++)
            if ((l[j] == a && l[j + 1] == c) || (l[j] == c && l[j + 1] == a)) return 1;
    }
    return 0;
}

void morris_start(mpos *p, int variant)
{
    memset(p, 0, sizeof *p);
    memset(p->at, -1, sizeof p->at);
    p->variant = (uint8_t)(morris_board(variant) - BOARDS);
    p->hand[0] = p->hand[1] = (uint8_t)board_of(p)->pieces;
}

int morris_in_mill(const mpos *p, int point)
{
    const mboard *b = board_of(p);
    int c = p->at[point];
    if (c < 0) return 0;
    for (int i = 0; i < b->lines; i++) {
        const int8_t *m = b->line[i];
        if (m[2] >= 0 && (m[0] == point || m[1] == point || m[2] == point) && p->at[m[0]] == c &&
            p->at[m[1]] == c && p->at[m[2]] == c)
            return 1;
    }
    return 0;
}

int morris_flying(const mpos *p, int player) { return board_of(p)->fly && !p->hand[player] && p->on[player] == 3; }

int morris_legal(const mpos *p, mmove *out)
{
    const mboard *b = board_of(p);
    int n = 0, c = p->side, np = b->points;
    if (p->take) {                              /* the opponent's pieces outside mills, or any if none is */
        int all = 1;
        for (int i = 0; i < np; i++)
            if (p->at[i] == !c && !morris_in_mill(p, i)) all = 0;
        for (int i = 0; i < np; i++)
            if (p->at[i] == !c && (all || !morris_in_mill(p, i))) out[n++] = (mmove){M_TAKE, (int8_t)i, -1};
        return n;
    }
    if (p->hand[c]) {
        for (int i = 0; i < np; i++)
            if (p->at[i] < 0) out[n++] = (mmove){M_PLACE, -1, (int8_t)i};
        return n;
    }
    int fly = morris_flying(p, c);
    for (int a = 0; a < np; a++) {
        if (p->at[a] != c) continue;
        for (int d = 0; d < np; d++)
            if (p->at[d] < 0 && (fly || adjacent(b, a, d))) out[n++] = (mmove){M_MOVE, (int8_t)a, (int8_t)d};
    }
    return n;
}

void morris_make(mpos *p, mmove m)
{
    int c = p->side;
    if (m.type == M_TAKE) {
        p->at[m.from] = -1;
        p->on[!c]--;
        p->take = 0;
        p->side = !c;
        return;
    }
    if (m.type == M_PLACE) {
        p->hand[c]--;
        p->on[c]++;
    } else {
        p->at[m.from] = -1;
    }
    p->at[m.to] = (int8_t)c;
    if (morris_in_mill(p, m.to)) {              /* a new mill: it goes through the point just taken */
        p->quiet = 0;
        if (p->on[!c]) { p->take = 1; return; }
    } else if (m.type == M_MOVE) {
        p->quiet++;
    }
    p->side = !c;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void morris_new(mgame *g, int variant)
{
    g->ply = 0;
    morris_start(&g->pos[0], variant);
}

int morris_play(mgame *g, mmove m)
{
    if (g->ply >= MORRIS_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    morris_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int morris_undo(mgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

static int same(const mpos *a, const mpos *b)
{
    return a->side == b->side && a->take == b->take && !memcmp(a->at, b->at, sizeof a->at) &&
           !memcmp(a->hand, b->hand, sizeof a->hand);
}

mstatus morris_status(const mgame *g)
{
    const mpos *p = morris_pos(g);
    int c = p->side;
    if (p->take) return MS_PLAY;
    if (p->hand[c] + p->on[c] < 3) return MS_FEW;
    if (p->on[0] + p->on[1] == board_of(p)->points) return MS_FULL;   /* full while placing: nowhere to go */
    mmove m[MORRIS_MAX_MOVES];
    if (!morris_legal(p, m)) return MS_BLOCKED;
    if (p->quiet >= MORRIS_QUIET) return MS_QUIET;
    int seen = 0;                               /* back to the last placing or removal: nothing before can repeat */
    for (int i = g->ply; i >= 0; i--) {
        const mpos *q = &g->pos[i];
        if (memcmp(q->on, p->on, sizeof p->on) || memcmp(q->hand, p->hand, sizeof p->hand)) break;
        seen += same(q, p);
    }
    return seen >= 3 ? MS_REPETITION : MS_PLAY;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int morris_point(int variant, const char *s)
{
    const mboard *b = morris_board(variant);
    if (s[0] < 'a' || s[0] >= 'a' + b->cells || s[1] < '1' || s[1] >= '1' + b->cells) return -1;
    for (int i = 0; i < b->points; i++)
        if (b->x[i] == s[0] - 'a' && b->y[i] == s[1] - '1') return i;
    return -1;
}

static int point_at(int variant, const char **s)
{
    int i = morris_point(variant, *s);
    if (i >= 0) *s += 2;
    return i;
}

static int play_one(mgame *g, int type, int from, int to)
{
    mmove l[MORRIS_MAX_MOVES];
    int n = morris_legal(morris_pos(g), l);
    for (int i = 0; i < n; i++)
        if (l[i].type == type && l[i].from == from && l[i].to == to) return morris_play(g, l[i]);
    return 0;
}

int morris_script(mgame *g, const char *s)
{
    int v = morris_pos(g)->variant;
    while (*s) {
        if (*s == ' ') { s++; continue; }
        if (*s == 'x') {
            s++;
            int pt = point_at(v, &s);
            if (pt < 0 || !play_one(g, M_TAKE, pt, -1)) return 0;
            continue;
        }
        int a = point_at(v, &s), b = -1;
        if (a < 0) return 0;
        if (*s == '-' || (*s >= 'a' && *s <= 'g')) {
            if (*s == '-') s++;
            if ((b = point_at(v, &s)) < 0) return 0;
        }
        if (b >= 0 ? !play_one(g, M_MOVE, a, b) : !play_one(g, M_PLACE, -1, a)) return 0;
    }
    return 1;
}
