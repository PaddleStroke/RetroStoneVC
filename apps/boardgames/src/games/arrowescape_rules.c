/*
 * Clear the Arrows for one player: the rules (arrowescape.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/arrowescape.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const char HEAD[4] = {'U', 'R', 'D', 'L'}, BODY[4] = {'u', 'r', 'd', 'l'};

static int inside(const arx_map *m, int x, int y) { return x >= 0 && y >= 0 && x < m->w && y < m->h; }

int arx_cells(const arx_map *m, int i, int dir, int *cells)
{
    const arx_piece *p = &m->p[i];
    for (int k = 0; k < p->len; k++) cells[k] = (p->x - k * ARX_DX[dir]) + (p->y - k * ARX_DY[dir]) * m->w;
    return p->len;
}

/* ---- boards as text ---------------------------------------------------------------------------------------------- */
int arx_parse(arx_map *m, const char *text)
{
    char g[ARX_CELLS];
    int w = 0, h = 0, x = 0;
    memset(m, 0, sizeof *m);
    for (const char *s = text;; s++) {
        if (*s == '|' || !*s) {
            if (!h) w = x;
            if (x != w || !w) return 0;
            h++;
            x = 0;
            if (!*s) break;
            continue;
        }
        if (x >= ARX_MAXW || h >= ARX_MAXH) return 0;
        g[x + h * ARX_MAXW] = *s;
        x++;
    }
    m->w = w;
    m->h = h;
    uint8_t claimed[ARX_CELLS] = {0};
    for (int y = 0; y < h; y++)
        for (int xx = 0; xx < w; xx++) {
            char c = g[xx + y * ARX_MAXW];
            int d = -1, turn = 0;
            for (int k = 0; k < 4; k++)
                if (c == HEAD[k]) d = k;
            if (c >= '0' && c <= '3') { d = c - '0'; turn = 1; }
            if (d < 0) continue;
            arx_piece *p = &m->p[m->n];
            p->x = (int8_t)xx;
            p->y = (int8_t)y;
            p->dir = (int8_t)d;
            p->turn = (int8_t)turn;
            p->len = 1;
            claimed[xx + y * w] = 1;
            while (!turn && p->len < ARX_MAXLEN) {
                int bx = xx - p->len * ARX_DX[d], by = y - p->len * ARX_DY[d];
                if (!inside(m, bx, by) || g[bx + by * ARX_MAXW] != BODY[d] || claimed[bx + by * w]) break;
                claimed[bx + by * w] = 1;
                p->len++;
            }
            m->n++;
        }
    for (int y = 0; y < h; y++)
        for (int xx = 0; xx < w; xx++) {
            char c = g[xx + y * ARX_MAXW];
            if (c == '.') continue;
            if (!claimed[xx + y * w]) return 0;   /* a bar's square with no head, or a character not known */
            m->shape[xx + y * w] = 1;
        }
    return m->n > 0;
}

void arx_text(const arx_map *m, char *out)
{
    char g[ARX_CELLS];
    memset(g, '.', sizeof g);
    for (int i = 0; i < m->n; i++) {
        int cells[ARX_MAXLEN], n = arx_cells(m, i, m->p[i].dir, cells);
        for (int k = 0; k < n; k++) g[cells[k]] = BODY[(int)m->p[i].dir];
        g[cells[0]] = m->p[i].turn ? (char)('0' + m->p[i].dir) : HEAD[(int)m->p[i].dir];
    }
    int o = 0;
    for (int y = 0; y < m->h; y++) {
        if (y) out[o++] = '|';
        for (int x = 0; x < m->w; x++) out[o++] = g[x + y * m->w];
    }
    out[o] = 0;
}

/* ---- play -------------------------------------------------------------------------------------------------------- */
void arx_begin(arx_game *g)
{
    memset(g->at, ARX_NONE, sizeof g->at);
    for (int i = 0; i < g->m.n; i++) {
        g->dir[i] = (uint8_t)g->m.p[i].dir;
        g->gone[i] = 0;
        int cells[ARX_MAXLEN], n = arx_cells(&g->m, i, g->dir[i], cells);
        for (int k = 0; k < n; k++) g->at[cells[k]] = (uint8_t)i;
    }
    g->nh = g->moves = g->mistakes = g->hints = 0;
    g->left = g->m.n;
}

/* the first tile from (x, y) the way d, not counting (x, y): -1 none; *gap the empty squares before it */
static int ray(const arx_map *m, const uint8_t *at, int x, int y, int d, int *gap)
{
    int n = 0;
    for (x += ARX_DX[d], y += ARX_DY[d]; inside(m, x, y); x += ARX_DX[d], y += ARX_DY[d], n++)
        if (at[x + y * m->w] != ARX_NONE) {
            if (gap) *gap = n;
            return at[x + y * m->w];
        }
    if (gap) *gap = n;
    return -1;
}

int arx_blocker(const arx_game *g, int i, int *gap)
{
    return ray(&g->m, g->at, g->m.p[i].x, g->m.p[i].y, g->dir[i], gap);
}

int arx_free(const arx_game *g, int i)
{
    if (g->gone[i]) return 0;
    if (!g->m.p[i].turn) return arx_blocker(g, i, NULL) < 0;
    for (int d = 0; d < 4; d++)
        if (ray(&g->m, g->at, g->m.p[i].x, g->m.p[i].y, d, NULL) < 0) return 1;
    return 0;
}

static void push(arx_game *g, int i, int kind)
{
    if (g->nh == ARX_HIST) {                    /* (a turnstile turned for ever: the oldest steps forgotten) */
        memmove(g->hist, g->hist + 1, (ARX_HIST - 1) * sizeof g->hist[0]);
        g->nh--;
    }
    g->hist[g->nh++] = (arx_step){(uint8_t)i, (uint8_t)kind, g->dir[i]};
}

int arx_tap(arx_game *g, int i, int *blocker)
{
    if (i < 0 || i >= g->m.n || g->gone[i]) return ARX_REFUSED;
    int b = arx_blocker(g, i, NULL);
    if (blocker) *blocker = b;
    g->moves++;
    if (b < 0) {
        int cells[ARX_MAXLEN], n = arx_cells(&g->m, i, g->dir[i], cells);
        push(g, i, ARX_GONE);
        for (int k = 0; k < n; k++) g->at[cells[k]] = ARX_NONE;
        g->gone[i] = 1;
        g->left--;
        return ARX_GONE;
    }
    if (g->m.p[i].turn) {
        push(g, i, ARX_TURNED);
        g->dir[i] = (uint8_t)((g->dir[i] + 1) & 3);
        return ARX_TURNED;
    }
    g->mistakes++;
    return ARX_BUMP;
}

int arx_undo(arx_game *g)
{
    if (!g->nh) return 0;
    arx_step s = g->hist[--g->nh];
    g->dir[s.piece] = s.dir;
    if (s.kind == ARX_GONE) {
        int cells[ARX_MAXLEN], n = arx_cells(&g->m, s.piece, s.dir, cells);
        for (int k = 0; k < n; k++) g->at[cells[k]] = s.piece;
        g->gone[s.piece] = 0;
        g->left++;
    }
    if (g->moves > 0) g->moves--;
    return 1;
}

int arx_solved(const arx_game *g) { return g->left == 0; }

int arx_hint(const arx_game *g)
{
    int best = -1, score = -1;
    for (int i = 0; i < g->m.n; i++) {
        if (!arx_free(g, i)) continue;
        int s = 0;
        for (int j = 0; j < g->m.n; j++)        /* the tiles it stands in front of */
            if (j != i && !g->gone[j] && arx_blocker(g, j, NULL) == i) s += 2;
        if (!g->m.p[i].turn || arx_blocker(g, i, NULL) < 0) s += 100;   /* a tap takes it off */
        if (s > score) { score = s; best = i; }
    }
    return best;
}

int arx_rounds(const arx_map *m, int *free_at_start)
{
    static arx_game t;                          /* (not reentrant: the tool and the tests are single threads) */
    t.m = *m;
    arx_begin(&t);
    int rounds = 0, list[ARX_MAXP];
    if (free_at_start) *free_at_start = 0;
    while (t.left) {
        int n = 0;
        for (int i = 0; i < t.m.n; i++)
            if (arx_free(&t, i)) list[n++] = i;
        if (!n) return -1;
        if (!rounds && free_at_start) *free_at_start = n;
        for (int k = 0; k < n; k++) {
            int i = list[k], cells[ARX_MAXLEN], c = arx_cells(&t.m, i, t.dir[i], cells);
            for (int j = 0; j < c; j++) t.at[cells[j]] = ARX_NONE;
            t.gone[i] = 1;
            t.left--;
        }
        rounds++;
    }
    return rounds;
}

/* ---- shapes: every one the same mirrored across; all but the heart the same turned half round too ---------------- */
void arx_shape(int shape, int w, int h, uint8_t *mask)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float u = (2.0f * x + 1 - w) / w, v = (2.0f * y + 1 - h) / h, au = u < 0 ? -u : u, av = v < 0 ? -v : v;
            int in = 1;
            switch (shape) {
            case ARX_DIAMOND: in = au + av <= 1.08f; break;
            case ARX_HEART: {
                float X = 1.22f * u, Y = 0.22f - 1.28f * v, a = X * X + Y * Y - 1;
                in = a * a * a - X * X * Y * Y * Y <= 0;
                break;
            }
            case ARX_ROUND: in = u * u + v * v <= 1.1f; break;
            case ARX_PLUS: in = au <= 0.42f || av <= 0.42f; break;
            case ARX_OCTAGON: in = au + av <= 1.45f; break;
            case ARX_BLOB: in = au * au * au + av * av * av <= 1.05f && !(av > 0.55f && au < 0.25f); break;
            default: break;
            }
            mask[x + y * w] = (uint8_t)in;
        }
}

/* ---- the maker ------------------------------------------------------------------------------------------------- */
typedef struct {
    arx_map *m;
    uint8_t at[ARX_CELLS];                      /* the tiles laid so far */
    uint8_t up[ARX_MAXP];                       /* the longest chain of tiles each one stands in front of */
} maker;

static int empty_cell(const maker *k, int x, int y)
{
    return inside(k->m, x, y) && k->m->shape[x + y * k->m->w] && k->at[x + y * k->m->w] == ARX_NONE;
}

static int ways_out(const maker *k, int c)      /* bit d: the way d from square c is clear of the tiles laid */
{
    int o = 0;
    for (int d = 0; d < 4; d++)
        if (ray(k->m, k->at, c % k->m->w, c / k->m->w, d, NULL) < 0) o |= 1 << d;
    return o;
}

static int popcount4(int v) { return (v & 1) + (v >> 1 & 1) + (v >> 2 & 1) + (v >> 3 & 1); }

/* the tile's image under the symmetry (sym 0 across, 1 half turn) */
static arx_piece image(const arx_map *m, arx_piece p, int sym)
{
    arx_piece q = p;
    q.x = (int8_t)(m->w - 1 - p.x);
    if (sym == 1) {
        q.y = (int8_t)(m->h - 1 - p.y);
        q.dir = (int8_t)((p.dir + 2) & 3);
    } else if (p.dir == ARX_LEFT || p.dir == ARX_RIGHT) q.dir = (int8_t)(p.dir ^ 2);
    return q;
}

static int piece_cells(const arx_map *m, const arx_piece *p, int *cells)
{
    for (int k = 0; k < p->len; k++) {
        int x = p->x - k * ARX_DX[(int)p->dir], y = p->y - k * ARX_DY[(int)p->dir];
        if (!inside(m, x, y)) return 0;
        cells[k] = x + y * m->w;
    }
    return p->len;
}

static int same_piece(const arx_piece *a, const arx_piece *b)
{
    return a->x == b->x && a->y == b->y && a->dir == b->dir && a->len == b->len;
}

/* the longest chain behind the tiles a new one at these squares stands in front of */
static int chain_behind(const maker *k, const int *cells, int n)
{
    int best = 0;
    for (int i = 0; i < n; i++) {
        int c = cells[i];
        for (int e = 0; e < 4; e++)
            for (int x = c % k->m->w + ARX_DX[e], y = c / k->m->w + ARX_DY[e]; inside(k->m, x, y);
                 x += ARX_DX[e], y += ARX_DY[e]) {
                int t = k->at[x + y * k->m->w];
                if (t == ARX_NONE) continue;
                const arx_piece *p = &k->m->p[t];
                if (p->x == x && p->y == y && !p->turn && p->dir == ((e + 2) & 3) && k->up[t] > best) best = k->up[t];
            }
    }
    return best;
}

/* lay tiles a (and b, its image, when nb) if they fit: their squares empty, each one's way clear of what is laid and
 * of the other, and no empty square of the shape left with no way out after them; 1: laid */
static int try_lay(maker *k, arx_piece *a, arx_piece *b, int nb)
{
    arx_map *m = k->m;
    int ca[ARX_MAXLEN], cb[ARX_MAXLEN], na = piece_cells(m, a, ca), nbc = nb ? piece_cells(m, b, cb) : 0;
    if (!na || (nb && !nbc)) return 0;
    for (int i = 0; i < na; i++)
        if (!empty_cell(k, ca[i] % m->w, ca[i] / m->w)) return 0;
    for (int i = 0; i < nbc; i++) {
        if (!empty_cell(k, cb[i] % m->w, cb[i] / m->w)) return 0;
        for (int j = 0; j < na; j++)
            if (cb[i] == ca[j]) return 0;
    }
    if (m->n + 1 + nb > ARX_MAXP) return 0;
    int ia = m->n, ib = m->n + 1;
    for (int i = 0; i < na; i++) k->at[ca[i]] = (uint8_t)ia;
    for (int i = 0; i < nbc; i++) k->at[cb[i]] = (uint8_t)ib;
    int ok = 1;
    for (int t = 0; t < 1 + nb && ok; t++) {     /* each one's way out clear (a turnstile: some way) */
        arx_piece *p = t ? b : a;
        if (p->turn) {
            int any = 0;
            for (int d = 0; d < 4; d++) any |= ray(m, k->at, p->x, p->y, d, NULL) < 0;
            ok = any;
        } else ok = ray(m, k->at, p->x, p->y, p->dir, NULL) < 0;
    }
    for (int t = 0; t < 1 + nb && ok; t++) {     /* the empty squares in their rows and columns keep a way out */
        const int *cc = t ? cb : ca;
        int nn = t ? nbc : na;
        for (int i = 0; i < nn && ok; i++)
            for (int e = 0; e < 4 && ok; e++)
                for (int x = cc[i] % m->w + ARX_DX[e], y = cc[i] / m->w + ARX_DY[e]; inside(m, x, y) && ok;
                     x += ARX_DX[e], y += ARX_DY[e])
                    if (empty_cell(k, x, y) && !ways_out(k, x + y * m->w)) ok = 0;
    }
    if (!ok) {
        for (int i = 0; i < na; i++) k->at[ca[i]] = ARX_NONE;
        for (int i = 0; i < nbc; i++) k->at[cb[i]] = ARX_NONE;
        return 0;
    }
    /* the chains: what each stands in front of (the other of the pair: not counted, they go together) */
    for (int i = 0; i < na; i++) k->at[ca[i]] = ARX_NONE;
    for (int i = 0; i < nbc; i++) k->at[cb[i]] = ARX_NONE;
    int ua = 1 + chain_behind(k, ca, na), ub = nb ? 1 + chain_behind(k, cb, nbc) : 0;
    for (int i = 0; i < na; i++) k->at[ca[i]] = (uint8_t)ia;
    for (int i = 0; i < nbc; i++) k->at[cb[i]] = (uint8_t)ib;
    m->p[ia] = *a;
    k->up[ia] = (uint8_t)(ua > 250 ? 250 : ua);
    if (nb) {
        m->p[ib] = *b;
        k->up[ib] = (uint8_t)(ub > 250 ? 250 : ub);
    }
    m->n += 1 + nb;
    return 1;
}

/* the board cut down to its shape's rows and columns */
static void trim(arx_map *m)
{
    int x0 = m->w, y0 = m->h, x1 = -1, y1 = -1;
    for (int c = 0; c < m->w * m->h; c++)
        if (m->shape[c]) {
            int x = c % m->w, y = c / m->w;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    if (x1 < 0) return;
    uint8_t s[ARX_CELLS] = {0};
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) s[x + y * w] = m->shape[x + x0 + (y + y0) * m->w];
    memcpy(m->shape, s, sizeof s);
    for (int i = 0; i < m->n; i++) {
        m->p[i].x = (int8_t)(m->p[i].x - x0);
        m->p[i].y = (int8_t)(m->p[i].y - y0);
    }
    m->w = w;
    m->h = h;
}

int arx_make(arx_map *m, const arx_spec *sp, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    static maker k;
    memset(m, 0, sizeof *m);
    m->w = sp->w < 2 ? 2 : sp->w > ARX_MAXW ? ARX_MAXW : sp->w;
    m->h = sp->h < 2 ? 2 : sp->h > ARX_MAXH ? ARX_MAXH : sp->h;
    arx_shape(sp->shape, m->w, m->h, m->shape);
    int sym = sp->sym;
    for (int c = 0; c < m->w * m->h && sym < 2; c++) {   /* the shape must be the same under the symmetry (a heart
                                                            * turned half round is not: mirrored then) */
        int x = c % m->w, y = c / m->w, o = sym == 1 ? (m->w - 1 - x) + (m->h - 1 - y) * m->w : (m->w - 1 - x) + y * m->w;
        if (m->shape[c] != m->shape[o]) { sym = sym == 1 ? 0 : 2; c = -1; }
    }
    k.m = m;
    memset(k.at, ARX_NONE, sizeof k.at);
    for (;;) {
        int pick = -1, fewest = 9, ties = 0, any = 0;
        for (int c = 0; c < m->w * m->h; c++) {
            if (!empty_cell(&k, c % m->w, c / m->w)) continue;
            any = 1;
            int o = popcount4(ways_out(&k, c));
            if (!o) return 0;
            if (o < fewest) { fewest = o; pick = c; ties = 1; }
            else if (o == fewest && pr_below(&r, ++ties) == 0) pick = c;
        }
        if (!any) break;
        int x = pick % m->w, y = pick / m->w, outs = ways_out(&k, pick);
        /* the ways to try: the clear ones, the one standing in front of the longest chain first (by `deep`) */
        int order[4], no = 0, score[4];
        for (int d = 0; d < 4; d++)
            if (outs >> d & 1) {
                int cell = pick;
                score[no] = chain_behind(&k, &cell, 1) * 16 + pr_below(&r, 16);
                order[no++] = d;
            }
        if (pr_below(&r, 100) < sp->deep) {
            for (int i = 0; i < no; i++)        /* the best first */
                for (int j = i + 1; j < no; j++)
                    if (score[j] > score[i]) {
                        int t = score[i]; score[i] = score[j]; score[j] = t;
                        t = order[i]; order[i] = order[j]; order[j] = t;
                    }
        } else {
            for (int i = no - 1; i > 0; i--) {
                int j = pr_below(&r, i + 1), t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
        int laid = 0;
        for (int oi = 0; oi < no && !laid; oi++) {
            int d = order[oi];
            int want = 1;
            if (sp->maxlen > 1 && pr_below(&r, 100) < sp->bars) want = 2 + pr_below(&r, sp->maxlen - 1);
            int turn = want == 1 && pr_below(&r, 100) < sp->turns;
            for (int len = want; len >= 1 && !laid; len--) {
                arx_piece a = {(int8_t)x, (int8_t)y, (int8_t)d, (int8_t)len, (int8_t)(len == 1 && turn)};
                if (a.turn) {                   /* a turnstile starts pointing a way that is blocked, if any */
                    int blocked[4], nbk = 0;
                    for (int e = 0; e < 4; e++)
                        if (!(outs >> e & 1)) blocked[nbk++] = e;
                    if (nbk) a.dir = (int8_t)blocked[pr_below(&r, nbk)];
                }
                arx_piece b = a;
                int nb = 0;
                if (sym < 2) {
                    b = image(m, a, sym);
                    int ca[ARX_MAXLEN], cb[ARX_MAXLEN], na = piece_cells(m, &a, ca), n2 = piece_cells(m, &b, cb);
                    int clash = 0;
                    for (int i = 0; i < na; i++)
                        for (int j = 0; j < n2; j++) clash += ca[i] == cb[j];
                    /* its own image (or, turned half round, on the same squares: the middle): laid alone */
                    if (na && n2 && (same_piece(&a, &b) || (clash == na && sym == 1))) nb = 0;
                    else if (clash) continue;
                    else nb = 1;
                }
                laid = try_lay(&k, &a, &b, nb);
            }
        }
        if (!laid) return 0;
    }
    trim(m);
    return m->n > 0;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int arx_script(arx_game *g, const char *moves)
{
    const char *s = moves;
    while (*s) {
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        if (*s == 'h') {
            int i = arx_hint(g);
            if (i < 0) return 0;
            g->hints++;
            arx_tap(g, i, NULL);
            s++;
            continue;
        }
        if (*s == '=') {
            for (int guard = 0; !arx_solved(g) && guard < 4 * ARX_MAXP; guard++) {
                int i = arx_hint(g);
                if (i < 0) return 0;
                arx_tap(g, i, NULL);
            }
            s++;
            continue;
        }
        if (!islower((unsigned char)*s) || !isdigit((unsigned char)s[1])) return 0;
        int x = *s - 'a', y = 0;
        s++;
        while (isdigit((unsigned char)*s)) y = y * 10 + (*s++ - '0');
        y--;
        if (!inside(&g->m, x, y) || g->at[x + y * g->m.w] == ARX_NONE) return 0;
        arx_tap(g, g->at[x + y * g->m.w], NULL);
    }
    return 1;
}

/* ---- the variants, the levels ------------------------------------------------------------------------------------ */
void arx_variant(int v, arx_spec *sp)
{
    static const arx_spec V[ARX_VARIANTS] = {
        {6, 6, ARX_RECT, 0, 2, 25, 0, 60},
        {8, 8, ARX_OCTAGON, 0, 3, 35, 8, 75},
        {10, 11, ARX_ROUND, 0, 4, 40, 6, 90},
    };
    *sp = V[v < 0 || v >= ARX_VARIANTS ? 0 : v];
}

#ifndef ARX_GEN                                 /* not in the levels' tool (tools/arrowescape_gen.c): it makes them */
#include "games/arrowescape_levels.h"

int arx_level(arx_game *g, int level)
{
    if (level < 0 || level >= ARX_LEVELS || !arx_parse(&g->m, arx_levels[level])) return 0;
    arx_begin(g);
    return 1;
}

void arx_random(arx_game *g, int variant, uint32_t seed)
{
    static const uint8_t LO[ARX_VARIANTS] = {3, 5, 6};
    static const int SHAPES[ARX_VARIANTS][3] = {
        {ARX_RECT, ARX_DIAMOND, ARX_PLUS}, {ARX_OCTAGON, ARX_ROUND, ARX_HEART}, {ARX_ROUND, ARX_HEART, ARX_BLOB}};
    arx_spec sp;
    arx_variant(variant, &sp);
    int v = variant < 0 || variant >= ARX_VARIANTS ? 0 : variant;
    uint32_t r = pr_seed(seed);
    for (int tries = 0; tries < 200; tries++) {
        sp.shape = SHAPES[v][pr_below(&r, 3)];
        sp.sym = pr_below(&r, 3) == 0 ? 1 : 0;
        int f0, n;
        if (!arx_make(&g->m, &sp, pr_next(&r))) continue;
        n = arx_rounds(&g->m, &f0);
        if (n >= LO[v] && f0 * 2 <= g->m.n) { arx_begin(g); return; }
    }
    arx_level(g, 0);
}
#endif
