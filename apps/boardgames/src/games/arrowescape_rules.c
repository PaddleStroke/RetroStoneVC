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

int arx_cells(const arx_map *m, int i, int *cells)
{
    const arx_piece *p = &m->p[i];
    int x = p->x, y = p->y;
    for (int k = 0; k < p->len; k++) {
        cells[k] = x + y * m->w;
        if (k + 1 < p->len) {
            x += ARX_DX[(int)p->step[k]];
            y += ARX_DY[(int)p->step[k]];
        }
    }
    return p->len;
}

int arx_bends(const arx_map *m, int i)
{
    const arx_piece *p = &m->p[i];
    int n = 0, last = (p->dir + 2) & 3;         /* (a neck at the side of the head: a bend) */
    for (int k = 0; k + 1 < p->len; k++) {
        n += p->step[k] != last;
        last = p->step[k];
    }
    return n;
}

/* 1: square (x, y) lies on the way out of an arrow with its head at (hx, hy) pointing d */
static int on_way(int hx, int hy, int d, int x, int y)
{
    int dx = x - hx, dy = y - hy;
    return dx * ARX_DY[d] == dy * ARX_DX[d] && dx * ARX_DX[d] + dy * ARX_DY[d] > 0;
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
            int d = -1;
            for (int k = 0; k < 4; k++)
                if (g[xx + y * ARX_MAXW] == HEAD[k]) d = k;
            if (d < 0) continue;
            arx_piece *p = &m->p[m->n++];
            p->x = (int8_t)xx;
            p->y = (int8_t)y;
            p->dir = (int8_t)d;
            p->len = 1;
            claimed[xx + y * w] = 1;
            for (int cx = xx, cy = y; p->len < ARX_MAXLEN;) {   /* the square pointing at this one: the next one */
                int found = -1;
                for (int k = 0; k < 4 && found < 0; k++) {
                    int nx = cx + ARX_DX[k], ny = cy + ARX_DY[k];
                    if (inside(m, nx, ny) && !claimed[nx + ny * w] && g[nx + ny * ARX_MAXW] == BODY[(k + 2) & 3])
                        found = k;
                }
                if (found < 0) break;
                cx += ARX_DX[found];
                cy += ARX_DY[found];
                if (on_way(xx, y, d, cx, cy)) return 0;   /* its body on its own way out */
                claimed[cx + cy * w] = 1;
                p->step[p->len - 1] = (int8_t)found;
                p->len++;
            }
        }
    for (int y = 0; y < h; y++)
        for (int xx = 0; xx < w; xx++) {
            if (g[xx + y * ARX_MAXW] == '.') continue;
            if (!claimed[xx + y * w]) return 0;   /* a body square with no head, or a character not known */
            m->shape[xx + y * w] = 1;
        }
    return m->n > 0;
}

void arx_text(const arx_map *m, char *out)
{
    char g[ARX_CELLS];
    memset(g, '.', sizeof g);
    for (int i = 0; i < m->n; i++) {
        int cells[ARX_MAXLEN], n = arx_cells(m, i, cells);
        g[cells[0]] = HEAD[(int)m->p[i].dir];
        for (int k = 1; k < n; k++) g[cells[k]] = BODY[(m->p[i].step[k - 1] + 2) & 3];
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
        g->gone[i] = 0;
        int cells[ARX_MAXLEN], n = arx_cells(&g->m, i, cells);
        for (int k = 0; k < n; k++) g->at[cells[k]] = (uint8_t)i;
    }
    g->nh = g->moves = g->mistakes = g->hints = 0;
    g->left = g->m.n;
}

/* the first arrow from (x, y) the way d, not counting (x, y): -1 none; *gap the empty squares before it */
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
    return ray(&g->m, g->at, g->m.p[i].x, g->m.p[i].y, g->m.p[i].dir, gap);
}

int arx_free(const arx_game *g, int i) { return !g->gone[i] && arx_blocker(g, i, NULL) < 0; }

int arx_tap(arx_game *g, int i, int *blocker)
{
    if (i < 0 || i >= g->m.n || g->gone[i]) return ARX_REFUSED;
    int b = arx_blocker(g, i, NULL);
    if (blocker) *blocker = b;
    g->moves++;
    if (b >= 0) {
        g->mistakes++;
        return ARX_BUMP;
    }
    if (g->nh == ARX_HIST) {                    /* (never: an arrow leaves once) */
        memmove(g->hist, g->hist + 1, ARX_HIST - 1);
        g->nh--;
    }
    g->hist[g->nh++] = (uint8_t)i;
    int cells[ARX_MAXLEN], n = arx_cells(&g->m, i, cells);
    for (int k = 0; k < n; k++) g->at[cells[k]] = ARX_NONE;
    g->gone[i] = 1;
    g->left--;
    return ARX_GONE;
}

int arx_undo(arx_game *g)
{
    if (!g->nh) return 0;
    int i = g->hist[--g->nh], cells[ARX_MAXLEN], n = arx_cells(&g->m, i, cells);
    for (int k = 0; k < n; k++) g->at[cells[k]] = (uint8_t)i;
    g->gone[i] = 0;
    g->left++;
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
        for (int j = 0; j < g->m.n; j++)        /* the arrows it stands in front of */
            if (j != i && !g->gone[j] && arx_blocker(g, j, NULL) == i) s++;
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
            int i = list[k], cells[ARX_MAXLEN], c = arx_cells(&t.m, i, cells);
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

/* ---- the maker --------------------------------------------------------------------------------------------------- */
typedef struct {
    arx_map *m;
    uint8_t at[ARX_CELLS];                      /* the arrows laid so far */
    uint8_t up[ARX_MAXP];                       /* the longest chain of arrows each one stands in front of */
    uint8_t bv[ARX_CELLS];                      /* the longest chain an arrow there would stand in front of */
} maker;

static int empty_cell(const maker *k, int x, int y)
{
    return inside(k->m, x, y) && k->m->shape[x + y * k->m->w] && k->at[x + y * k->m->w] == ARX_NONE;
}

/* the longest chain behind the arrows whose ways out pass over these squares */
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
                if (p->x == x && p->y == y && p->dir == ((e + 2) & 3) && k->up[t] > best) best = k->up[t];
            }
    }
    return best;
}

static int mirror_dir(int d, int sym) { return sym == 1 ? (d + 2) & 3 : d == ARX_LEFT || d == ARX_RIGHT ? d ^ 2 : d; }

static arx_piece image(const arx_map *m, const arx_piece *p, int sym)
{
    arx_piece q = *p;
    q.x = (int8_t)(m->w - 1 - p->x);
    if (sym == 1) q.y = (int8_t)(m->h - 1 - p->y);
    q.dir = (int8_t)mirror_dir(p->dir, sym);
    for (int k = 0; k + 1 < p->len; k++) q.step[k] = (int8_t)mirror_dir(p->step[k], sym);
    return q;
}

/* an arrow's squares if they all lie on the board (0: not) */
static int piece_cells(const arx_map *m, const arx_piece *p, int *cells)
{
    int x = p->x, y = p->y;
    for (int k = 0; k < p->len; k++) {
        if (!inside(m, x, y)) return 0;
        cells[k] = x + y * m->w;
        if (k + 1 < p->len) {
            x += ARX_DX[(int)p->step[k]];
            y += ARX_DY[(int)p->step[k]];
        }
    }
    return p->len;
}

/* a body grown from a head through the empty squares (never on its own way out): a search trying the likeliest
 * squares first (bending, running along what is laid, over the ways out of the arrows laid: they must wait), going
 * back from dead ends, until the length wanted or its budget is spent; the longest found */
typedef struct {
    maker *k;
    const arx_spec *sp;
    uint32_t *r;
    arx_piece cur, best;
    int want, budget, turn;                     /* (each arrow its own taste for bends: S, L, U, spirals) */
    uint8_t mine[ARX_CELLS];
} growth;

static void grow_from(growth *g, int x, int y, int last, int sense, int run)
{
    maker *k = g->k;
    if (g->cur.len > g->best.len) g->best = g->cur;
    if (g->cur.len >= g->want || --g->budget <= 0) return;
    int ord[4], sc[4], n = 0;
    for (int d = 0; d < 4; d++) {
        int nx = x + ARX_DX[d], ny = y + ARX_DY[d];
        if (!empty_cell(k, nx, ny) || g->mine[nx + ny * k->m->w] || on_way(g->cur.x, g->cur.y, g->cur.dir, nx, ny))
            continue;
        int s = pr_below(g->r, 100);
        if (d != last) {
            s += run >= 2 ? g->turn : g->turn / 3;   /* (a bend after a straight run: S, U, L more than zigzags) */
            int sn = ((d - last) & 3) == 1 ? 1 : -1;   /* the way it turns: the same again winds a spiral */
            if (sense && sn == sense) s += g->turn / 2;
        }
        int walls = 0;
        for (int e = 0; e < 4; e++) {
            int ex = nx + ARX_DX[e], ey = ny + ARX_DY[e];
            walls += !empty_cell(k, ex, ey) || g->mine[ex + ey * k->m->w];
        }
        s += g->sp->hug * walls / 3 + k->bv[nx + ny * k->m->w] * g->sp->deep / 6;
        int i = n++;
        for (; i > 0 && sc[i - 1] < s; i--) { sc[i] = sc[i - 1]; ord[i] = ord[i - 1]; }
        sc[i] = s;
        ord[i] = d;
    }
    for (int i = 0; i < n; i++) {
        int d = ord[i], nx = x + ARX_DX[d], ny = y + ARX_DY[d];
        g->cur.step[g->cur.len - 1] = (int8_t)d;
        g->cur.len++;
        g->mine[nx + ny * k->m->w] = 1;
        grow_from(g, nx, ny, d, d != last && g->cur.len > 2 ? (((d - last) & 3) == 1 ? 1 : -1) : sense,
                  d != last ? 1 : run + 1);
        g->mine[nx + ny * k->m->w] = 0;
        g->cur.len--;
        if (g->best.len >= g->want || g->budget <= 0) return;
    }
}

static void grow(maker *k, arx_piece *a, int want, const arx_spec *sp, uint32_t *r)
{
    static growth g;
    memset(g.mine, 0, sizeof g.mine);
    g.k = k;
    g.sp = sp;
    g.r = r;
    g.want = want;
    g.budget = 300;
    g.turn = sp->turn * (40 + pr_below(r, 80)) / 100;
    a->len = 1;
    g.cur = g.best = *a;
    g.mine[a->x + a->y * k->m->w] = 1;
    grow_from(&g, a->x, a->y, (a->dir + 2) & 3, 0, 2);
    *a = g.best;
}


/* lay arrows a (and b, its image, when nb) if they fit: their squares empty and apart, each one's way out clear of
 * what is laid and of the other; 1: laid */
static int try_lay(maker *k, const arx_piece *a, const arx_piece *b, int nb)
{
    arx_map *m = k->m;
    int ca[ARX_MAXLEN], cb[ARX_MAXLEN], na = piece_cells(m, a, ca), nbc = nb ? piece_cells(m, b, cb) : 0;
    if (!na || (nb && !nbc) || m->n + 1 + nb > ARX_MAXP) return 0;
    for (int i = 0; i < na; i++)
        if (!empty_cell(k, ca[i] % m->w, ca[i] / m->w)) return 0;
    for (int i = 0; i < nbc; i++) {
        if (!empty_cell(k, cb[i] % m->w, cb[i] / m->w)) return 0;
        for (int j = 0; j < na; j++)
            if (cb[i] == ca[j]) return 0;
        if (on_way(a->x, a->y, a->dir, cb[i] % m->w, cb[i] / m->w)) return 0;
    }
    for (int i = 0; i < na && nb; i++)
        if (on_way(b->x, b->y, b->dir, ca[i] % m->w, ca[i] / m->w)) return 0;
    if (ray(m, k->at, a->x, a->y, a->dir, NULL) >= 0) return 0;
    if (nb && ray(m, k->at, b->x, b->y, b->dir, NULL) >= 0) return 0;
    int ua = 1 + chain_behind(k, ca, na), ub = nb ? 1 + chain_behind(k, cb, nbc) : 0;
    int ia = m->n;
    for (int i = 0; i < na; i++) k->at[ca[i]] = (uint8_t)ia;
    m->p[ia] = *a;
    k->up[ia] = (uint8_t)(ua > 250 ? 250 : ua);
    if (nb) {
        for (int i = 0; i < nbc; i++) k->at[cb[i]] = (uint8_t)(ia + 1);
        m->p[ia + 1] = *b;
        k->up[ia + 1] = (uint8_t)(ub > 250 ? 250 : ub);
    }
    m->n += 1 + nb;
    return 1;
}

/* square c taken by the tail of arrow i (next to it) when no arrow laid after i has its way out over c */
static int extend(maker *k, int i, int c)
{
    arx_map *m = k->m;
    arx_piece *p = &m->p[i];
    int cells[ARX_MAXLEN], n = arx_cells(m, i, cells), x = c % m->w, y = c / m->w;
    if (n >= ARX_MAXLEN || on_way(p->x, p->y, p->dir, x, y)) return 0;
    int tx = cells[n - 1] % m->w, ty = cells[n - 1] / m->w, d = -1;
    for (int e = 0; e < 4; e++)
        if (tx + ARX_DX[e] == x && ty + ARX_DY[e] == y) d = e;
    if (d < 0) return 0;
    for (int j = i + 1; j < m->n; j++)
        if (on_way(m->p[j].x, m->p[j].y, m->p[j].dir, x, y)) return 0;
    p->step[n - 1] = (int8_t)d;
    p->len++;
    k->at[c] = (uint8_t)i;
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

/* the squares left: taken by the tails next to them (in symmetric pairs when it can) */
static void fill_tails(maker *k, int sym)
{
    arx_map *m = k->m;
    for (int pass = 0; pass < 6; pass++)
        for (int c = 0; c < m->w * m->h; c++) {
            if (!empty_cell(k, c % m->w, c / m->w)) continue;
            for (int e = 0; e < 4; e++) {
                int nx = c % m->w + ARX_DX[e], ny = c / m->w + ARX_DY[e];
                if (!inside(m, nx, ny) || k->at[nx + ny * m->w] == ARX_NONE) continue;
                int i = k->at[nx + ny * m->w], cs[ARX_MAXLEN], n = arx_cells(m, i, cs);
                if (cs[n - 1] != nx + ny * m->w || !extend(k, i, c)) continue;
                if (sym < 2) {                  /* its image the same way */
                    int ox = m->w - 1 - c % m->w, oy = sym == 1 ? m->h - 1 - c / m->w : c / m->w, oc = ox + oy * m->w;
                    int tx = m->w - 1 - nx, ty = sym == 1 ? m->h - 1 - ny : ny;
                    if (oc != c && empty_cell(k, ox, oy) && k->at[tx + ty * m->w] != ARX_NONE) {
                        int j = k->at[tx + ty * m->w], cj[ARX_MAXLEN], nj = arx_cells(m, j, cj);
                        if (cj[nj - 1] == tx + ty * m->w) extend(k, j, oc);
                    }
                }
                break;
            }
        }
}

int arx_make(arx_map *m, const arx_spec *sp, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    static maker k;
    memset(m, 0, sizeof *m);
    m->w = sp->w < 2 ? 2 : sp->w > ARX_MAXW ? ARX_MAXW : sp->w;
    m->h = sp->h < 2 ? 2 : sp->h > ARX_MAXH ? ARX_MAXH : sp->h;
    arx_shape(sp->shape, m->w, m->h, m->shape);
    int sym = sp->sym, cells = 0;
    for (int c = 0; c < m->w * m->h && sym < 2; c++) {   /* the shape must be the same under the symmetry (a heart
                                                            * turned half round is not: mirrored then) */
        int x = c % m->w, y = c / m->w, o = sym == 1 ? (m->w - 1 - x) + (m->h - 1 - y) * m->w : (m->w - 1 - x) + y * m->w;
        if (m->shape[c] != m->shape[o]) { sym = sym == 1 ? 0 : 2; c = -1; }
    }
    for (int c = 0; c < m->w * m->h; c++) cells += m->shape[c];
    k.m = m;
    memset(k.at, ARX_NONE, sizeof k.at);
    int lo = sp->lo < 1 ? 1 : sp->lo, hi = sp->hi < lo ? lo : sp->hi > ARX_MAXLEN ? ARX_MAXLEN : sp->hi;
    /* arrows as long as wanted first; when no head can grow one, shorter ones (single squares last) */
    int len_now = lo;
    static uint8_t banned[ARX_CELLS * 4];
    memset(banned, 0, sizeof banned);
    for (;;) {
        /* the longest chain behind each square: the arrows whose way out passes over it */
        memset(k.bv, 0, sizeof k.bv);
        for (int j = 0; j < m->n; j++)
            for (int x = m->p[j].x + ARX_DX[(int)m->p[j].dir], y = m->p[j].y + ARX_DY[(int)m->p[j].dir];
                 inside(m, x, y); x += ARX_DX[(int)m->p[j].dir], y += ARX_DY[(int)m->p[j].dir])
                if (k.up[j] > k.bv[x + y * m->w]) k.bv[x + y * m->w] = k.up[j];
        /* the heads that can go: an empty square, a way whose squares are clear of what is laid; a long way across
         * the shape first (the arrows laid later, over it, make it wait), next to what is laid */
        int best = -1, bd = 0, bs = -1;
        for (int c = 0; c < m->w * m->h; c++) {
            if (!empty_cell(&k, c % m->w, c / m->w)) continue;
            for (int d = 0; d < 4; d++) {
                if (banned[c * 4 + d] || ray(m, k.at, c % m->w, c / m->w, d, NULL) >= 0) continue;
                int span = 0;
                for (int x = c % m->w + ARX_DX[d], y = c / m->w + ARX_DY[d]; inside(m, x, y); x += ARX_DX[d], y += ARX_DY[d])
                    span += m->shape[x + y * m->w];
                int s = pr_below(&r, 100) + span * sp->deep / 8 + 6 * k.bv[c] * sp->deep / 100;
                int walls = 0;
                for (int e = 0; e < 4; e++) walls += !empty_cell(&k, c % m->w + ARX_DX[e], c / m->w + ARX_DY[e]);
                s += 12 * walls;
                if (s > bs) { bs = s; best = c; bd = d; }
            }
        }
        if (best < 0) {
            if (len_now <= 1) break;
            len_now--;                          /* no head grows one that long: shorter ones */
            if (len_now == 1) fill_tails(&k, sym);   /* (tails before single squares) */
            memset(banned, 0, sizeof banned);
            continue;
        }
        arx_piece a = {(int8_t)(best % m->w), (int8_t)(best / m->w), (int8_t)bd, 1, {0}};
        int u1 = pr_below(&r, hi - lo + 1), u2 = pr_below(&r, hi - lo + 1), want = lo + (u1 > u2 ? u1 : u2), laid = 0;
        if (want < len_now) want = len_now;
        grow(&k, &a, want, sp, &r);
        for (; a.len >= len_now && !laid; a.len--) {
            if (sym >= 2) { laid = try_lay(&k, &a, &a, 0); continue; }
            arx_piece b = image(m, &a, sym);
            int ca[ARX_MAXLEN], cb[ARX_MAXLEN], na = piece_cells(m, &a, ca), n2 = piece_cells(m, &b, cb), same = 0;
            for (int i = 0; i < na; i++)
                for (int j = 0; j < n2; j++) same += ca[i] == cb[j];
            /* its own image (the same squares, the same way: on the middle column), laid alone */
            if (na && n2 && same == na && na == n2 && b.dir == a.dir && b.x == a.x && b.y == a.y)
                laid = try_lay(&k, &a, &a, 0);
            else if (!same) laid = try_lay(&k, &a, &b, 1);
        }
        if (laid) memset(banned, 0, sizeof banned);
        else banned[best * 4 + bd] = 1;
    }
    fill_tails(&k, sym);
    int holes = 0;
    for (int c = 0; c < m->w * m->h; c++) holes += empty_cell(&k, c % m->w, c / m->w);
    if (holes * 30 > cells) return 0;
    for (int c = 0; c < m->w * m->h && sym < 2; c++) {   /* the holes symmetric too (the shape stays so) */
        int x = c % m->w, y = c / m->w, o = sym == 1 ? (m->w - 1 - x) + (m->h - 1 - y) * m->w : (m->w - 1 - x) + y * m->w;
        if (empty_cell(&k, x, y) != empty_cell(&k, o % m->w, o / m->w)) return 0;
    }
    for (int c = 0; c < m->w * m->h; c++)       /* a hole stays out of the shape: no bed shows there */
        if (empty_cell(&k, c % m->w, c / m->w)) m->shape[c] = 0;
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
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            s++;
            for (int guard = 0; guard < ARX_MAXP && !arx_solved(g); guard++) {
                int i = arx_hint(g);
                if (i < 0) return 0;
                if (!all) g->hints++;
                arx_tap(g, i, NULL);
                if (!all) break;
            }
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
        {6, 6, ARX_RECT, 0, 2, 5, 55, 40, 60},
        {8, 9, ARX_OCTAGON, 0, 3, 8, 60, 50, 75},
        {10, 11, ARX_ROUND, 0, 3, 12, 65, 60, 90},
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
        {ARX_RECT, ARX_DIAMOND, ARX_OCTAGON}, {ARX_OCTAGON, ARX_ROUND, ARX_HEART}, {ARX_ROUND, ARX_HEART, ARX_BLOB}};
    arx_spec sp;
    arx_variant(variant, &sp);
    int v = variant < 0 || variant >= ARX_VARIANTS ? 0 : variant;
    uint32_t r = pr_seed(seed);
    for (int tries = 0; tries < 200; tries++) {
        sp.shape = SHAPES[v][pr_below(&r, 3)];
        sp.sym = pr_below(&r, 4) == 0 ? 1 : 0;
        int f0;
        if (!arx_make(&g->m, &sp, pr_next(&r))) continue;
        int n = arx_rounds(&g->m, &f0);
        if (n >= LO[v] && f0 * 2 <= g->m.n) { arx_begin(g); return; }
    }
    arx_level(g, 0);
}
#endif
