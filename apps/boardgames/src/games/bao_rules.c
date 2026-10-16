/*
 * Bao la kujifunza: the sowing, the captures from the kichwa, the relay sowing, the moves allowed, the end (bao.h).
 * No SDL; tests/bao_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/bao.h"

void bao_start(bao_pos *p)
{
    memset(p, 0, sizeof *p);
    memset(p->pit, 2, sizeof p->pit);
    p->winner = -1;
}

void bao_new(bao_game *g)
{
    memset(g, 0, sizeof *g);
    bao_start(&g->pos[0]);
}

int bao_seeds(const bao_pos *p, int player)
{
    int n = 0;
    for (int i = 0; i < BAO_RING; i++) n += p->pit[player][i];
    return n;
}

int bao_inner(const bao_pos *p, int player)
{
    int n = 0;
    for (int i = 0; i < 8; i++) n += p->pit[player][i];
    return n;
}

void bao_cell(int player, int pit, int *col, int *row)
{
    int inner = pit < 8, k = inner ? pit : pit - 8;   /* k: from the player's left (inner), from their right (outer) */
    if (!player) { *col = inner ? k : 7 - k; *row = inner ? 1 : 0; }
    else { *col = inner ? 7 - k : k; *row = inner ? 2 : 3; }
}

int bao_at(int col, int row, int *player)
{
    if (col < 0 || col > 7 || row < 0 || row > 3) return -1;
    *player = row >= 2;
    if (row == 1) return col;
    if (row == 0) return 8 + 7 - col;
    if (row == 2) return 7 - col;
    return 8 + col;
}

/* the pit the last seed of n sown from `from` (emptied) falls in, and whether it held seeds before it */
static int last_pit(const bao_pos *p, int me, int from, int n, int dir, int *held)
{
    int last = (from + dir * n) & 15;
    int laps = (n - 1) / BAO_RING;              /* full rounds before the last seed: each puts one seed there */
    int before = (last == from ? 0 : p->pit[me][last]) + laps;
    *held = before > 0;
    return last;
}

int bao_capturing(const bao_pos *p, int move)
{
    int me = p->turn, from = bao_mv_pit(move), n = p->pit[me][from], held;
    if (n < 2) return 0;
    int last = last_pit(p, me, from, n, bao_mv_cw(move) ? 1 : -1, &held);
    return held && last < 8 && p->pit[!me][7 - last] > 0;
}

/* the moves allowed by the rules alone (a capture must be taken; a takasa from the inner row when it can) */
int bao_moves(const bao_pos *p, int *out)
{
    if (p->winner >= 0) return 0;
    int me = p->turn, n = 0, inner2 = 0;
    for (int m = 0; m < 2 * BAO_RING; m++)
        if (bao_capturing(p, m)) out[n++] = m;
    if (n) return n;
    for (int i = 0; i < 8; i++) inner2 |= p->pit[me][i] >= 2;
    for (int i = 0; i < (inner2 ? 8 : BAO_RING); i++)
        if (p->pit[me][i] >= 2) {
            out[n++] = bao_mv(i, 1);
            out[n++] = bao_mv(i, 0);
        }
    return n;
}

int bao_legal(const bao_pos *p, int move)
{
    int mv[2 * BAO_RING], n = bao_moves(p, mv);
    for (int i = 0; i < n; i++)
        if (mv[i] == move) return 1;
    return 0;
}

static void note(bao_trace *t, int kind, int player, int pit, int n)
{
    if (t && t->n < BAO_TRACE) t->s[t->n++] = (bao_step){(uint8_t)kind, (uint8_t)player, (uint8_t)pit, (uint8_t)n};
}

void bao_apply(bao_pos *p, int move, bao_trace *t)
{
    int me = p->turn, op = !me, pos = bao_mv_pit(move), dir = bao_mv_cw(move) ? 1 : -1;
    int mtaji = bao_capturing(p, move), hand = p->pit[me][pos], sown = 0;
    if (t) t->n = t->captures = 0;
    note(t, BAO_PICK, me, pos, hand);
    p->pit[me][pos] = 0;
    while (hand > 0 && sown < BAO_MAX_SOWN) {
        while (hand > 0 && sown < BAO_MAX_SOWN) {   /* sow the hand */
            pos = (pos + dir) & 15;
            p->pit[me][pos]++;
            hand--;
            sown++;
            note(t, BAO_DROP, me, pos, 1);
        }
        if (hand || p->pit[me][pos] == 1) break;    /* stopped, or the last seed in an empty pit: the end */
        if (mtaji && pos < 8 && p->pit[op][7 - pos]) {   /* a capture: sown from a kichwa */
            hand = p->pit[op][7 - pos];
            p->pit[op][7 - pos] = 0;
            note(t, BAO_TAKE, op, 7 - pos, hand);
            if (t) t->captures++;
            if (pos <= 1) dir = 1;
            else if (pos >= 6) dir = -1;
            pos = dir > 0 ? 15 : 8;             /* the first seed goes into the kichwa itself (0 or 7) */
        } else {                                /* relay: take up the pit and sow on */
            hand = p->pit[me][pos];
            p->pit[me][pos] = 0;
            note(t, BAO_PICK, me, pos, hand);
        }
    }
    if (hand) p->pit[me][pos] += (uint8_t)hand; /* stopped for ever's sake: the hand's seeds stay there */
    p->moves++;
    p->turn = (uint8_t)op;
    int mv[2 * BAO_RING];
    if (!bao_inner(p, op)) p->winner = (int8_t)me;
    else if (!bao_inner(p, me)) p->winner = (int8_t)op;
    else if (!bao_moves(p, mv)) p->winner = (int8_t)me;   /* the other player cannot move */
    else if (p->moves >= BAO_MAX_MOVES) {
        int a = bao_seeds(p, 0), b = bao_seeds(p, 1);
        p->winner = (int8_t)(a > b ? 0 : b > a ? 1 : 2);
    }
}

const bao_pos *bao_now(const bao_game *g) { return &g->pos[g->n]; }

int bao_play(bao_game *g, int move, bao_trace *t)
{
    if (g->n >= BAO_MAX_MOVES || !bao_legal(bao_now(g), move)) return 0;
    g->pos[g->n + 1] = g->pos[g->n];
    g->move[g->n] = (uint8_t)move;
    bao_apply(&g->pos[++g->n], move, t);
    return 1;
}

int bao_undo(bao_game *g)
{
    if (!g->n) return 0;
    g->n--;
    return 1;
}

int bao_parse(const bao_pos *p, const char *s, int *move)
{
    if (s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '4' || (s[2] != '+' && s[2] != '-')) return 0;
    int player, pit = bao_at(s[0] - 'a', s[1] - '1', &player);
    if (player != p->turn) return 0;
    *move = bao_mv(pit, s[2] == '+');
    return 3;
}

void bao_name(const bao_pos *p, int move, char *out)
{
    int col, row;
    bao_cell(p->turn, bao_mv_pit(move), &col, &row);
    out[0] = (char)('a' + col);
    out[1] = (char)('1' + row);
    out[2] = bao_mv_cw(move) ? '+' : '-';
    out[3] = 0;
}

int bao_script(bao_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int m, len = bao_parse(bao_now(g), s, &m);
        if (!len || !bao_play(g, m, NULL)) return 0;
        s += len;
    }
    return 1;
}
