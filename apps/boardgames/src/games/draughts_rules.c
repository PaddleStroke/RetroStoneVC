/*
 * Draughts rules (draughts.h). Captures first: every capture sequence of every piece, found square by square; plain
 * moves only when no piece can capture. The pieces taken stay on the board until the move ends (they block, and none
 * is jumped twice), except in Turkish draughts, where each goes at once (and a piece may not turn straight back).
 * International, Brazilian and Turkish keep the sequences that take the most pieces; English and Russian keep them all.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "draughts.h"

#include <string.h>

/* the directions: diagonal (0-3), then orthogonal (4-7: Turkish draughts) */
static const int DX[8] = {1, -1, 1, -1, 1, -1, 0, 0}, DY[8] = {1, 1, -1, -1, 0, 0, 1, -1};

/* most: the maximum capture rule (2: Frisian's, by value); fly: flying kings; back: men take backwards too; stop: a
 * man crowned in a capture stops there; promote: it is crowned there and takes on as a king; orth: straight lines,
 * every square; at_once: the pieces taken go as they are jumped; quiet: the plies without a capture or a man moved
 * that make a draw (0: none); frisian: captures along the rows and files too (over every second square), its kings'
 * three moves, its draws; columns: the pieces taken go under the taker (bashni) */
static const struct rules {
    uint8_t n, most, fly, back, stop, promote, orth, at_once, quiet, frisian, columns;
} R[DR_VARIANTS] = {
    /* n most fly back stop promote orth at_once quiet frisian columns */
    {10, 1, 1, 1, 0, 0, 0, 0, 50, 0, 0},        /* international: crowned only ending on the far row */
    {8, 0, 0, 0, 1, 0, 0, 0, 80, 0, 0},         /* English: men forwards only, kings one square at a time */
    {8, 0, 1, 1, 0, 1, 0, 0, 30, 0, 0},         /* Russian: any capture; a man reaching the far row is a king at once */
    {8, 1, 1, 1, 0, 0, 0, 0, 40, 0, 0},         /* Brazilian: the international rules on 8 x 8 */
    {8, 1, 1, 0, 0, 0, 1, 1, 50, 0, 0},         /* Turkish: men forwards and sideways */
    {10, 2, 1, 1, 0, 0, 0, 0, 0, 1, 0},         /* Frisian: by value, along the rows and files too */
    {12, 1, 1, 1, 0, 0, 0, 0, 50, 0, 0},        /* Canadian: the international rules on 12 x 12 */
    {8, 0, 1, 1, 0, 1, 0, 0, 30, 0, 1},         /* bashni: the Russian rules, columns */
};

int dr_size(int variant) { return R[variant].n; }
int dr_all(int variant) { return R[variant].orth; }
int dr_columns(int variant) { return R[variant].columns; }
int dr_pieces(int variant) { return R[variant].n == 12 ? 30 : R[variant].n == 10 ? 20 : R[variant].orth ? 16 : 12; }

static int on(int n, int x, int y) { return x >= 0 && y >= 0 && x < n && y < n; }

void dr_start(dpos *p, int variant)
{
    int n = dr_size(variant), rows = n == 12 ? 5 : n == 10 ? 4 : 3;
    memset(p, 0, sizeof *p);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            if (dr_all(variant)) {              /* Turkish: the second and third rows, every square */
                if (y == 1 || y == 2) p->sq[y * n + x] = DR_MAN0;
                else if (y == n - 2 || y == n - 3) p->sq[y * n + x] = DR_MAN1;
            } else if (!((x + y) & 1)) {
                if (y < rows) p->sq[y * n + x] = DR_MAN0;
                else if (y >= n - rows) p->sq[y * n + x] = DR_MAN1;
            }
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
typedef struct {
    const struct rules *r;
    int n, me, man, king, far, fwd, d0, nd, best, count;
    uint8_t b[DR_MAX_SQ];                       /* the board, the moving piece lifted from its square */
    uint8_t taken[DR_MAX_SQ];                   /* the pieces jumped so far (still on the board) */
    dmove cur;
    dmove *out;
} gen;

/* Frisian's value of the pieces a capture takes (higher: better): two for each piece, a king a little less (k kings
 * between 2 k - 1 and 2 k men), then, of equal value, a king's capture before a man's */
static int value(const gen *g, int depth)
{
    int kings = 0;
    for (int i = 0; i < depth; i++) kings += DR_IS_KING(g->b[g->cur.cap[i]]);
    int men = depth - kings;
    return (((2 * kings + men) * 64 + (63 - kings)) * 2) + !g->man;
}

static void record(gen *g, int depth)
{
    if (g->r->most) {                           /* only the sequences that take the most */
        int v = g->r->most == 2 ? value(g, depth) : depth;
        if (v < g->best) return;
        if (v > g->best) g->best = v, g->count = 0;
    }
    if (g->count >= DR_MAX_MOVES) return;
    dmove *m = &g->out[g->count++];
    *m = g->cur;
    m->n = m->ncap = (uint8_t)depth;
    m->crown = g->man && (g->king || m->path[depth - 1] / g->n == g->far);   /* a man a king by now: Russian */
}

/* a step along direction d: Frisian's rows and files go from a dark square to the next one, two squares on */
static int step(const gen *g, int d) { return g->r->frisian && d >= 4 ? 2 : 1; }

/* the piece the moving one can take from `at` in direction d (its last jump: in direction `last`, -1 at first);
 * -1: none */
static int target(const gen *g, int at, int d, int last)
{
    int n = g->n, dx = DX[d] * step(g, d), dy = DY[d] * step(g, d);
    if (!g->king && !g->r->back && DY[d] == -g->fwd) return -1;   /* men taking forwards (and sideways) only */
    if (g->r->at_once && last >= 0 && DX[d] == -DX[last] && DY[d] == -DY[last]) return -1;   /* no turning back */
    int x = at % n + dx, y = at / n + dy;
    if (g->king && g->r->fly) while (on(n, x, y) && !g->b[y * n + x]) x += dx, y += dy;
    if (!on(n, x, y) || !on(n, x + dx, y + dy) || g->b[(y + dy) * n + x + dx]) return -1;
    int v = y * n + x, pc = g->b[v];
    return pc && DR_OWNER(pc) != g->me && !g->taken[v] ? v : -1;
}

static int can_take(const gen *g, int at, int last)
{
    for (int d = g->d0; d < g->d0 + g->nd; d++)
        if (target(g, at, d, last) >= 0) return 1;
    return 0;
}

/* the captures on from square `at`, `depth` pieces taken so far, the last jump in direction `last`; a sequence ends
 * where no more can be taken */
static void jumps(gen *g, int at, int depth, int last)
{
    int n = g->n, flying = g->king && g->r->fly, any = 0;
    if (depth >= DR_MAX_STEPS) { record(g, depth); return; }
    for (int d = g->d0; d < g->d0 + g->nd; d++) {
        int v = target(g, at, d, last), dx = DX[d] * step(g, d), dy = DY[d] * step(g, d);
        if (v < 0) continue;
        any = 1;
        uint8_t pc = g->b[v];
        if (g->r->at_once) g->b[v] = DR_EMPTY;
        else g->taken[v] = 1;
        g->cur.cap[depth] = (uint8_t)v;
        /* a flying king lands on any empty square beyond, but on one where it can take on if there is one */
        int must = 0;
        for (int x = v % n + dx, y = v / n + dy; flying && !must && on(n, x, y) && !g->b[y * n + x];
             x += dx, y += dy)
            must = can_take(g, y * n + x, d);
        for (int x = v % n + dx, y = v / n + dy; on(n, x, y) && !g->b[y * n + x]; x += dx, y += dy) {
            if (!must || can_take(g, y * n + x, d)) {
                g->cur.path[depth] = (uint8_t)(y * n + x);
                if (!g->king && y == g->far && g->r->stop) record(g, depth + 1);   /* English: crowned, it stops */
                else if (!g->king && y == g->far && g->r->promote) {   /* Russian: a king at once, taking on */
                    g->king = 1;
                    jumps(g, y * n + x, depth + 1, d);
                    g->king = 0;
                } else jumps(g, y * n + x, depth + 1, d);
            }
            if (!flying) break;                 /* only a flying king may land further on */
        }
        if (g->r->at_once) g->b[v] = pc;
        else g->taken[v] = 0;
    }
    if (!any && depth) record(g, depth);
}

int dr_legal(const dpos *p, int variant, dmove *out)
{
    gen g;
    g.r = &R[variant];
    g.n = dr_size(variant);
    g.me = p->side;
    g.far = p->side ? 0 : g.n - 1;
    g.fwd = p->side ? -1 : 1;
    g.d0 = g.r->orth ? 4 : 0;
    g.nd = g.r->frisian ? 8 : 4;
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
        g.man = !g.king;
        g.cur.from = (uint8_t)s;
        jumps(&g, s, 0, -1);
    }
    if (g.count) return g.count;
    int tired = -1;                             /* Frisian: the king that made three simple moves in a row */
    if (g.r->frisian && p->kmoves[p->side] >= 3)
        for (int s = 0; s < n * n && tired < 0; s++)
            if (p->sq[s] == DR_MAN0 + p->side) tired = p->kat[p->side];   /* only while the player has men */
    for (int s = 0; s < n * n; s++) {           /* no capture: the plain moves */
        int pc = p->sq[s];
        if (!pc || DR_OWNER(pc) != p->side || s == tired) continue;
        for (int d = g.d0; d < g.d0 + 4; d++) {
            if (!DR_IS_KING(pc) && DY[d] == -g.fwd) continue;   /* men never go back */
            for (int x = s % n + DX[d], y = s / n + DY[d]; on(n, x, y) && !p->sq[y * n + x];
                 x += DX[d], y += DY[d]) {
                dmove *m = &out[g.count++];
                memset(m, 0, sizeof *m);
                m->from = (uint8_t)s;
                m->n = 1;
                m->path[0] = (uint8_t)(y * n + x);
                m->crown = !DR_IS_KING(pc) && y == g.far;
                if (!DR_IS_KING(pc) || !g.r->fly) break;   /* only a flying king goes further */
            }
        }
    }
    return g.count;
}

/* bashni: the pieces taken leave the top of their columns and go under the capturing one, in the order taken */
static void make_columns(dpos *p, const dmove *m)
{
    int from = m->from, to = m->path[m->n - 1], h = p->nunder[from];
    uint64_t col = p->under[from];
    p->under[from] = 0;
    p->nunder[from] = 0;
    for (int i = 0; i < m->ncap; i++) {
        int c = m->cap[i], taken = p->sq[c];
        if (p->nunder[c]) {                     /* the piece beneath commands the column now */
            p->sq[c] = (uint8_t)((p->under[c] & 3) + 1);
            p->under[c] >>= 2;
            p->nunder[c]--;
        } else p->sq[c] = DR_EMPTY;
        if (h < 31) col |= (uint64_t)(taken - 1) << (2 * h++);
    }
    p->under[to] = col;
    p->nunder[to] = (uint8_t)h;
}

void dr_make(dpos *p, int variant, const dmove *m)
{
    int pc = p->sq[m->from], to = m->path[m->n - 1];
    p->sq[m->from] = DR_EMPTY;
    if (R[variant].columns && dr_size(variant) * dr_size(variant) <= DR_COLUMNS) make_columns(p, m);
    else for (int i = 0; i < m->ncap; i++) p->sq[m->cap[i]] = DR_EMPTY;
    p->sq[to] = (uint8_t)(m->crown ? pc + 2 : pc);
    p->quiet = m->ncap || !DR_IS_KING(pc) ? 0 : p->quiet + 1;
    if (R[variant].frisian) {                   /* a king's simple moves in a row (the same king) */
        int s = p->side;
        if (!m->ncap && DR_IS_KING(pc)) {
            p->kmoves[s] = p->kat[s] == m->from && p->kmoves[s] ? p->kmoves[s] + 1 : 1;
            p->kat[s] = (uint8_t)to;
        } else p->kmoves[s] = 0;
    }
    p->side ^= 1;
}

static int columns_same(const dpos *a, const dpos *b)
{
    return !memcmp(a->nunder, b->nunder, sizeof a->nunder) && !memcmp(a->under, b->under, sizeof a->under);
}

int dr_prisoners(const dpos *p, int player)
{
    int k = 0;
    for (int s = 0; s < DR_COLUMNS; s++) {
        if (!p->sq[s] || DR_OWNER(p->sq[s]) != player) continue;
        for (int i = 0; i < p->nunder[s]; i++) k += (int)(p->under[s] >> (2 * i) & 1) != player;
    }
    return k;
}

int dr_height(const dpos *p, int s) { return !p->sq[s] ? 0 : 1 + (s < DR_COLUMNS ? p->nunder[s] : 0); }

uint64_t dr_column(const dpos *p, int s)
{
    if (!p->sq[s]) return 0;
    return (uint64_t)(p->sq[s] - 1) | (s < DR_COLUMNS ? p->under[s] << 2 : 0);
}

int dr_stack(dpos *p, int s, const char *u)
{
    if (s < 0 || s >= DR_COLUMNS || !p->sq[s]) return 0;
    for (; *u; u++) {
        int c = *u == 'w' ? 0 : *u == 'b' ? 1 : *u == 'W' ? 2 : *u == 'B' ? 3 : -1;
        if (c < 0 || p->nunder[s] >= 31) return 0;
        p->under[s] |= (uint64_t)c << (2 * p->nunder[s]++);
    }
    return 1;
}

int dr_same(const dmove *a, const dmove *b)
{
    if (a->from != b->from || a->path[a->n - 1] != b->path[b->n - 1] || a->ncap != b->ncap) return 0;
    for (int i = 0; i < a->ncap; i++)
        if (!memchr(b->cap, a->cap[i], b->ncap)) return 0;
    return 1;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static int numbered(int variant)
{
    return variant == DR_INTERNATIONAL || variant == DR_ENGLISH || variant == DR_FRISIAN || variant == DR_CANADIAN;
}

int dr_square(int variant, int number)
{
    int n = dr_size(variant), half = n / 2;
    if (!numbered(variant) || number < 1 || number > n * half) return -1;
    int y = n - 1 - (number - 1) / half, x = 2 * ((number - 1) % half) + (y & 1);
    if (variant == DR_ENGLISH) x = n - 1 - x, y = n - 1 - y;   /* English: player 0 (ebony) sits across from ivory */
    return y * n + x;
}

int dr_number(int variant, int square)
{
    int n = dr_size(variant), x = square % n, y = square / n;
    if (!numbered(variant)) return -1;
    if (variant == DR_ENGLISH) x = n - 1 - x, y = n - 1 - y;
    return (n - 1 - y) * (n / 2) + x / 2 + 1;
}

int dr_read(int variant, const char *s, int *used)
{
    int n = dr_size(variant), x = 0, k = 0, v = 0;
    if (!numbered(variant)) {                   /* "c3": the file's letter, then the rank */
        x = s[0] - 'a';
        if (x < 0 || x >= n) return -1;
        k = 1;
    }
    int k0 = k;
    for (; s[k] >= '0' && s[k] <= '9'; k++)
        if (v < 1000) v = v * 10 + (s[k] - '0');
    *used = k;
    if (k == k0) return -1;
    if (numbered(variant)) return dr_square(variant, v);
    if (v < 1 || v > n || (!dr_all(variant) && ((x + v - 1) & 1))) return -1;   /* off the board, or a light square */
    return (v - 1) * n + x;
}

int dr_parse(const dmove *legal, int nlegal, int variant, const char *s, int *used)
{
    const char *s0 = s;
    int sq[DR_MAX_STEPS + 1], k = 0;
    while (*s == ' ') s++;
    for (;;) {
        int u, q = k > DR_MAX_STEPS ? -1 : dr_read(variant, s, &u);
        if (q < 0) return -1;
        sq[k++] = q;
        s += u;
        if (*s != '-' && *s != 'x' && *s != ':') break;
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
            int king = *s == 'K', used, q = dr_read(variant, s + king, &used);
            if (q < 0) return 0;
            p->sq[q] = (uint8_t)(DR_MAN0 + pl + 2 * king);
            s += king + used;
        }
    return 1;
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
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
    /* the same position (the same side to move) for the third time; none comes back past a capture or a man moved
     * forwards (a Turkish man's step sideways can be undone) */
    int n = dr_size(g->variant), seen = 1;
    for (int k = 1; k <= g->ply; k++) {
        const dmove *m = &g->move[g->ply - k];
        if (m->ncap || (!DR_IS_KING(g->pos[g->ply - k].sq[m->from]) && m->path[0] / n != m->from / n)) break;
        if (!(k & 1) && !memcmp(g->pos[g->ply - k].sq, p->sq, (size_t)n * n) &&
            (!R[g->variant].columns || columns_same(&g->pos[g->ply - k], p)) && ++seen == 3)
            return DR_REPETITION;
    }
    /* no capture and no man moved for 25 moves each (international, Turkish, Canadian), 40 (English), 15 (Russian,
     * bashni), 20 (Brazilian) */
    if (R[g->variant].quiet && p->quiet >= R[g->variant].quiet) return DR_QUIET;
    if (R[g->variant].frisian) {                /* kings only: two against one, seven moves each; one each, two */
        int kings[2] = {0, 0}, men = 0;
        for (int s = 0; s < n * n; s++)
            if (p->sq[s]) {
                if (DR_IS_KING(p->sq[s])) kings[DR_OWNER(p->sq[s])]++;
                else men++;
            }
        if (!men && kings[0] + kings[1] == 3 && kings[0] && kings[1] && p->quiet >= 14) return DR_QUIET;
        if (!men && kings[0] == 1 && kings[1] == 1 && p->quiet >= 4) return DR_QUIET;
    }
    return DR_PLAY;
}
