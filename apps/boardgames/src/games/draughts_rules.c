/*
 * Draughts rules (draughts.h). Captures first: every capture sequence of every piece, found square by square with the
 * pieces taken left on the board until the move ends (they block, and none is jumped twice); plain moves only when no
 * piece can capture. International keeps the sequences that take the most pieces; English keeps them all.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "draughts.h"

#include <stdlib.h>
#include <string.h>

static const int DX[4] = {1, -1, 1, -1}, DY[4] = {1, 1, -1, -1};

int dr_size(int variant) { return variant == DR_ENGLISH ? 8 : 10; }

static int on(int n, int x, int y) { return x >= 0 && y >= 0 && x < n && y < n; }

void dr_start(dpos *p, int variant)
{
    int n = dr_size(variant), rows = variant == DR_ENGLISH ? 3 : 4;
    memset(p, 0, sizeof *p);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            if (!((x + y) & 1)) {
                if (y < rows) p->sq[y * n + x] = DR_MAN0;
                else if (y >= n - rows) p->sq[y * n + x] = DR_MAN1;
            }
}

/* ---- the moves ------------------------------------------------------------------------------------------------------- */
typedef struct {
    int n, english, me, king, far, best, count;
    uint8_t b[DR_MAX_SQ];                       /* the board, the moving piece lifted from its square */
    uint8_t taken[DR_MAX_SQ];                   /* the pieces jumped so far (still on the board) */
    dmove cur;
    dmove *out;
} gen;

static void record(gen *g, int depth)
{
    if (!g->english) {                          /* international: only the sequences that take the most */
        if (depth < g->best) return;
        if (depth > g->best) g->best = depth, g->count = 0;
    }
    if (g->count >= DR_MAX_MOVES) return;
    dmove *m = &g->out[g->count++];
    *m = g->cur;
    m->n = m->ncap = (uint8_t)depth;
    m->crown = !g->king && m->path[depth - 1] / g->n == g->far;
}

/* the captures on from square `at`, `depth` pieces taken so far; a sequence ends where no more can be taken */
static void jumps(gen *g, int at, int depth)
{
    int n = g->n, flying = g->king && !g->english, any = 0;
    for (int d = 0; d < 4; d++) {
        if (g->english && !g->king && DY[d] != (g->me ? -1 : 1)) continue;   /* English men take forwards only */
        int x = at % n + DX[d], y = at / n + DY[d];
        if (flying) while (on(n, x, y) && !g->b[y * n + x]) x += DX[d], y += DY[d];
        if (!on(n, x, y)) continue;
        int v = y * n + x, pc = g->b[v];
        if (!pc || DR_OWNER(pc) == g->me || g->taken[v]) continue;
        for (x += DX[d], y += DY[d]; on(n, x, y) && !g->b[y * n + x]; x += DX[d], y += DY[d]) {
            any = 1;
            g->taken[v] = 1;
            g->cur.path[depth] = (uint8_t)(y * n + x);
            g->cur.cap[depth] = (uint8_t)v;
            if (g->english && !g->king && y == g->far) record(g, depth + 1);   /* crowned: the move ends */
            else jumps(g, y * n + x, depth + 1);
            g->taken[v] = 0;
            if (!flying) break;                 /* only a flying king may land further on */
        }
    }
    if (!any && depth) record(g, depth);
}

int dr_legal(const dpos *p, int variant, dmove *out)
{
    gen g;
    g.n = dr_size(variant);
    g.english = variant == DR_ENGLISH;
    g.me = p->side;
    g.far = p->side ? 0 : g.n - 1;
    g.best = 1;
    g.count = 0;
    g.out = out;
    memset(g.taken, 0, sizeof g.taken);
    int n = g.n;
    for (int s = 0; s < n * n; s++) {
        if (!p->sq[s] || DR_OWNER(p->sq[s]) != p->side) continue;
        memcpy(g.b, p->sq, sizeof g.b);
        g.b[s] = DR_EMPTY;
        g.king = DR_IS_KING(p->sq[s]);
        g.cur.from = (uint8_t)s;
        jumps(&g, s, 0);
    }
    if (g.count) return g.count;
    for (int s = 0; s < n * n; s++) {           /* no capture: the plain moves */
        int pc = p->sq[s];
        if (!pc || DR_OWNER(pc) != p->side) continue;
        for (int d = 0; d < 4; d++) {
            if (!DR_IS_KING(pc) && DY[d] != (p->side ? -1 : 1)) continue;
            for (int x = s % n + DX[d], y = s / n + DY[d]; on(n, x, y) && !p->sq[y * n + x];
                 x += DX[d], y += DY[d]) {
                dmove *m = &out[g.count++];
                memset(m, 0, sizeof *m);
                m->from = (uint8_t)s;
                m->n = 1;
                m->path[0] = (uint8_t)(y * n + x);
                m->crown = !DR_IS_KING(pc) && y == g.far;
                if (!DR_IS_KING(pc) || g.english) break;   /* only an international king flies */
            }
        }
    }
    return g.count;
}

void dr_make(dpos *p, int variant, const dmove *m)
{
    (void)variant;
    int pc = p->sq[m->from];
    p->sq[m->from] = DR_EMPTY;
    for (int i = 0; i < m->ncap; i++) p->sq[m->cap[i]] = DR_EMPTY;
    p->sq[m->path[m->n - 1]] = (uint8_t)(m->crown ? pc + 2 : pc);
    p->quiet = m->ncap || !DR_IS_KING(pc) ? 0 : p->quiet + 1;
    p->side ^= 1;
}

int dr_same(const dmove *a, const dmove *b)
{
    if (a->from != b->from || a->path[a->n - 1] != b->path[b->n - 1] || a->ncap != b->ncap) return 0;
    for (int i = 0; i < a->ncap; i++)
        if (!memchr(b->cap, a->cap[i], b->ncap)) return 0;
    return 1;
}

/* ---- the notation ------------------------------------------------------------------------------------------------------ */
int dr_square(int variant, int number)
{
    int n = dr_size(variant), half = n / 2;
    if (number < 1 || number > n * half) return -1;
    int y = n - 1 - (number - 1) / half, x = 2 * ((number - 1) % half) + (y & 1);
    if (variant == DR_ENGLISH) x = n - 1 - x, y = n - 1 - y;   /* English: player 0 (ebony) sits across from ivory */
    return y * n + x;
}

int dr_number(int variant, int square)
{
    int n = dr_size(variant), x = square % n, y = square / n;
    if (variant == DR_ENGLISH) x = n - 1 - x, y = n - 1 - y;
    return (n - 1 - y) * (n / 2) + x / 2 + 1;
}

int dr_parse(const dmove *legal, int nlegal, int variant, const char *s, int *used)
{
    const char *s0 = s;
    int sq[DR_MAX_STEPS + 1], k = 0;
    while (*s == ' ') s++;
    for (;;) {
        if (*s < '0' || *s > '9' || k > DR_MAX_STEPS) return -1;
        int v = 0;
        while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
        if ((sq[k++] = dr_square(variant, v)) < 0) return -1;
        if (*s != '-' && *s != 'x') break;
        s++;
    }
    if (k < 2) return -1;
    *used = (int)(s - s0);
    for (int i = 0; i < nlegal; i++) {
        const dmove *m = &legal[i];
        if (m->from != sq[0] || m->path[m->n - 1] != sq[k - 1] || (k > 2 && m->n != k - 1)) continue;
        int j = 1;
        while (j < k - 1 && m->path[j - 1] == sq[j]) j++;
        if (j == k - 1) return i;
    }
    return -1;
}

int dr_setup(dpos *p, int variant, int side, const char *pieces0, const char *pieces1)
{
    const char *list[2] = {pieces0, pieces1};
    memset(p, 0, sizeof *p);
    p->side = (uint8_t)side;
    for (int pl = 0; pl < 2; pl++)
        for (const char *s = list[pl]; s && *s;) {
            if (*s == ' ' || *s == ',') { s++; continue; }
            int king = *s == 'K';
            char *end;
            int q = dr_square(variant, (int)strtol(s + king, &end, 10));
            if (end == s + king || q < 0) return 0;
            p->sq[q] = (uint8_t)(DR_MAN0 + pl + 2 * king);
            s = end;
        }
    return 1;
}

/* ---- the game ---------------------------------------------------------------------------------------------------------- */
void dr_new(dgame *g, int variant)
{
    g->variant = variant;
    g->ply = 0;
    dr_start(&g->pos[0], variant);
}

int dr_play(dgame *g, const dmove *m)
{
    if (g->ply >= DR_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    dr_make(&g->pos[g->ply + 1], g->variant, m);
    g->move[g->ply++] = *m;
    return 1;
}

int dr_undo(dgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

dstatus dr_status(const dgame *g)
{
    static dmove buf[DR_MAX_MOVES];
    const dpos *p = dr_pos(g);
    if (!dr_legal(p, g->variant, buf)) return p->side ? DR_WON0 : DR_WON1;
    /* the same position (the same side to move) for the third time; none can come back past a capture or a man move */
    int n = dr_size(g->variant), seen = 1;
    for (int k = 2; k <= p->quiet && k <= g->ply; k += 2)
        if (!memcmp(g->pos[g->ply - k].sq, p->sq, (size_t)n * n) && ++seen == 3) return DR_REPETITION;
    /* international: 25 moves each with only kings moved, no capture; English: 40 moves each, no capture, no man moved */
    if (p->quiet >= (g->variant == DR_ENGLISH ? 80 : 50)) return DR_QUIET;
    return DR_PLAY;
}
