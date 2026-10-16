/*
 * Roll the Block: the rules (rollblock.h): maps as text, rolls, the special tiles, undo, the breadth-first search for
 * the fewest moves (the par, the hint), the puzzles' maker, the levels. No SDL: tested by tests/rollblock_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/rollblock.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

/* ---- maps as text ------------------------------------------------------------------------------------------------ */
int rlb_parse(rlb_map *m, const char *s)
{
    memset(m, 0, sizeof *m);
    m->start = m->hole = m->split = m->ring[0] = m->ring[1] = -1;
    int x = 0, y = 0, rings = 0;
    for (;; s++) {
        if (*s == '|' || !*s) {
            if (x) {
                if (!m->w) m->w = x;
                if (x != m->w) return 0;
                y++;
            }
            x = 0;
            if (!*s) break;
            continue;
        }
        if (x >= RLB_MAXW || y >= RLB_MAXH) return 0;
        int c = x + y * RLB_MAXW, t;            /* (cells kept RLB_MAXW wide while reading: packed at the end) */
        char k = *s;
        if (k == '.') t = RLB_VOID;
        else if (k == 'o') t = RLB_TILE;
        else if (k == 'S') { t = RLB_TILE; if (m->start >= 0) return 0; m->start = c; }
        else if (k == 'H') { t = RLB_HOLE; if (m->hole >= 0) return 0; m->hole = c; }
        else if (k == 'f') t = RLB_FRAGILE;
        else if (k >= 'a' && k <= 'd') { t = RLB_SOFT; m->grp[c] = (uint8_t)(k - 'a'); }
        else if (k >= 'A' && k <= 'D') { t = RLB_HEAVY; m->grp[c] = (uint8_t)(k - 'A'); }
        else if (k >= '1' && k <= '4') { t = RLB_BRIDGE; m->grp[c] = (uint8_t)(k - '1'); }
        else if (k >= '5' && k <= '8') {
            t = RLB_BRIDGE;
            m->grp[c] = (uint8_t)(k - '5');
            m->open0 |= (uint8_t)(1 << (k - '5'));
        }
        else if (k == 'x') { t = RLB_SPLIT; if (m->split >= 0) return 0; m->split = c; }
        else if (k == 'y') { t = RLB_RING; if (rings >= 2) return 0; m->ring[rings++] = c; }
        else return 0;
        m->t[c] = (uint8_t)t;
        x++;
    }
    m->h = y;
    if (!m->w || !m->h || m->start < 0 || m->hole < 0 || (m->split >= 0) != (rings == 2)) return 0;
    /* packed w wide */
    uint8_t t[RLB_CELLS], g[RLB_CELLS];
    memcpy(t, m->t, sizeof t);
    memcpy(g, m->grp, sizeof g);
    memset(m->t, 0, sizeof m->t);
    memset(m->grp, 0, sizeof m->grp);
    for (int yy = 0; yy < m->h; yy++)
        for (int xx = 0; xx < m->w; xx++) {
            m->t[xx + yy * m->w] = t[xx + yy * RLB_MAXW];
            m->grp[xx + yy * m->w] = g[xx + yy * RLB_MAXW];
        }
#define PACK(c) ((c) < 0 ? -1 : (c) % RLB_MAXW + (c) / RLB_MAXW * m->w)
    m->start = PACK(m->start);
    m->hole = PACK(m->hole);
    m->split = PACK(m->split);
    m->ring[0] = PACK(m->ring[0]);
    m->ring[1] = PACK(m->ring[1]);
#undef PACK
    return 1;
}

void rlb_text(const rlb_map *m, char *out)
{
    int k = 0;
    for (int y = 0; y < m->h; y++) {
        if (y) out[k++] = '|';
        for (int x = 0; x < m->w; x++) {
            int c = x + y * m->w, g = m->grp[c];
            char ch = '.';
            switch (m->t[c]) {
            case RLB_TILE: ch = c == m->start ? 'S' : 'o'; break;
            case RLB_FRAGILE: ch = 'f'; break;
            case RLB_SOFT: ch = (char)('a' + g); break;
            case RLB_HEAVY: ch = (char)('A' + g); break;
            case RLB_BRIDGE: ch = (char)((m->open0 >> g & 1 ? '5' : '1') + g); break;
            case RLB_SPLIT: ch = 'x'; break;
            case RLB_RING: ch = 'y'; break;
            case RLB_HOLE: ch = 'H'; break;
            }
            out[k++] = ch;
        }
    }
    out[k] = 0;
}

/* ---- rolls ------------------------------------------------------------------------------------------------------- */
static rlb_state start_of(const rlb_map *m)
{
    rlb_state s = {(int8_t)(m->start % m->w), (int8_t)(m->start / m->w), RLB_STAND, 0, 0, 0, m->open0};
    return s;
}

void rlb_begin(rlb_game *g)
{
    g->s = start_of(&g->m);
    g->nh = g->moves = 0;
}

int rlb_cells(const rlb_state *s, int w, int *c)
{
    c[0] = s->x + s->y * w;
    if (s->o == RLB_STAND) return 1;
    c[1] = s->o == RLB_LIE_X ? c[0] + 1 : s->o == RLB_LIE_Y ? c[0] + w : s->x2 + s->y2 * w;
    return 2;
}

static int floor_at(const rlb_map *m, int open, int x, int y)
{
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    int c = x + y * m->w, t = m->t[c];
    return t != RLB_VOID && !(t == RLB_BRIDGE && !(open >> m->grp[c] & 1));
}

static int all_floor(const rlb_map *m, const rlb_state *s)   /* (by its cells' places: none off the map) */
{
    int x2 = s->o == RLB_LIE_X ? s->x + 1 : s->o == RLB_TWO ? s->x2 : s->x;
    int y2 = s->o == RLB_LIE_Y ? s->y + 1 : s->o == RLB_TWO ? s->y2 : s->y;
    return floor_at(m, s->open, s->x, s->y) && floor_at(m, s->open, x2, y2);
}

int rlb_try(const rlb_map *m, const rlb_state *s, int dir, rlb_state *out, int *events)
{
    rlb_state n = *s;
    int ev = 0, dx = DX[dir], dy = DY[dir];
    if (s->o == RLB_TWO) {
        int ax = s->act ? s->x2 : s->x, ay = s->act ? s->y2 : s->y;
        int bx = s->act ? s->x : s->x2, by = s->act ? s->y : s->y2;
        ax += dx;
        ay += dy;
        if (!floor_at(m, s->open, ax, ay) || (ax == bx && ay == by)) return 0;
        int c = ax + ay * m->w;
        if (m->t[c] == RLB_SOFT) { n.open ^= (uint8_t)(1 << m->grp[c]); ev |= RLB_EV_SWITCH; }
        if (s->act) n.x2 = (int8_t)ax, n.y2 = (int8_t)ay;
        else n.x = (int8_t)ax, n.y = (int8_t)ay;
        if (!floor_at(m, n.open, ax, ay) || !floor_at(m, n.open, bx, by)) return 0;
        if (abs(ax - bx) + abs(ay - by) == 1) { /* side by side: the block again, lying on both */
            n.o = ay == by ? RLB_LIE_X : RLB_LIE_Y;
            n.x = (int8_t)(ax < bx ? ax : bx);
            n.y = (int8_t)(ay < by ? ay : by);
            n.x2 = n.y2 = n.act = 0;
            ev |= RLB_EV_JOIN;
        }
        *out = n;
        if (events) *events = ev;
        return 1;
    }
    if (s->o == RLB_STAND) {
        n.o = dx ? RLB_LIE_X : RLB_LIE_Y;
        n.x = (int8_t)(s->x + (dx > 0 ? 1 : dx < 0 ? -2 : 0));
        n.y = (int8_t)(s->y + (dy > 0 ? 1 : dy < 0 ? -2 : 0));
    } else if (s->o == RLB_LIE_X) {
        if (dx) { n.o = RLB_STAND; n.x = (int8_t)(s->x + (dx > 0 ? 2 : -1)); }
        else n.y = (int8_t)(s->y + dy);
    } else {
        if (dy) { n.o = RLB_STAND; n.y = (int8_t)(s->y + (dy > 0 ? 2 : -1)); }
        else n.x = (int8_t)(s->x + dx);
    }
    if (!all_floor(m, &n)) return 0;
    int c[2], k = rlb_cells(&n, m->w, c);
    if (n.o == RLB_STAND && m->t[c[0]] == RLB_FRAGILE) return 0;
    for (int i = 0; i < k; i++) {
        int t = m->t[c[i]];
        if (t == RLB_SOFT || (t == RLB_HEAVY && n.o == RLB_STAND)) {
            n.open ^= (uint8_t)(1 << m->grp[c[i]]);
            ev |= RLB_EV_SWITCH;
        }
    }
    if (!all_floor(m, &n)) return 0;            /* a bridge shut under the block */
    if (n.o == RLB_STAND && c[0] == m->hole) ev |= RLB_EV_WIN;
    else if (n.o == RLB_STAND && c[0] == m->split) {   /* split in two cubes, on the two rings */
        n.o = RLB_TWO;
        n.x = (int8_t)(m->ring[0] % m->w);
        n.y = (int8_t)(m->ring[0] / m->w);
        n.x2 = (int8_t)(m->ring[1] % m->w);
        n.y2 = (int8_t)(m->ring[1] / m->w);
        n.act = 0;
        ev |= RLB_EV_SPLIT;
        if (!all_floor(m, &n)) return 0;
    }
    *out = n;
    if (events) *events = ev;
    return 1;
}

int rlb_move(rlb_game *g, int dir)
{
    rlb_state n;
    int ev;
    if (dir < 0 || dir > 3 || rlb_solved(&g->s, &g->m) || !rlb_try(&g->m, &g->s, dir, &n, &ev)) return 0;
    if (g->nh >= RLB_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + RLB_HIST / 2, sizeof g->hist / 2);
        g->nh -= RLB_HIST / 2;
    }
    g->hist[g->nh++] = g->s;
    g->s = n;
    g->moves++;
    return ev + 16;
}

int rlb_choose(rlb_game *g)
{
    if (g->s.o != RLB_TWO) return 0;
    g->s.act = (int8_t)!g->s.act;
    return 1;
}

int rlb_undo(rlb_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    g->moves--;
    return 1;
}

int rlb_solved(const rlb_state *s, const rlb_map *m) { return s->o == RLB_STAND && s->x + s->y * m->w == m->hole; }

/* ---- the search -------------------------------------------------------------------------------------------------- */
/* a state's number: the block's cell and way, or the two cubes' cells (either order alike), and the bridges open */
static int per_open(const rlb_map *m) { return m->w * m->h * 3 + (m->split >= 0 ? m->w * m->h * m->w * m->h : 0); }

static int groups_of(const rlb_map *m)          /* the colours used: 1 + the highest */
{
    int n = 0;
    for (int c = 0; c < m->w * m->h; c++)
        if ((m->t[c] == RLB_SOFT || m->t[c] == RLB_HEAVY || m->t[c] == RLB_BRIDGE) && m->grp[c] + 1 > n)
            n = m->grp[c] + 1;
    return n;
}

static int id_of(const rlb_map *m, const rlb_state *s)
{
    int wh = m->w * m->h, base;
    if (s->o != RLB_TWO) base = (s->x + s->y * m->w) * 3 + s->o;
    else {
        int a = s->x + s->y * m->w, b = s->x2 + s->y2 * m->w;
        base = wh * 3 + (a < b ? a * wh + b : b * wh + a);
    }
    return (s->open & ((1 << groups_of(m)) - 1)) * per_open(m) + base;
}

int rlb_solve(const rlb_map *m, const rlb_state *s, int *dir, int *cube, int *states)
{
    if (states) *states = 0;
    if (rlb_solved(s, m)) return 0;
    int G = groups_of(m), N = per_open(m) << G, found = -1, best = -1;
    uint8_t *seen = calloc((size_t)N, 1);
    rlb_state *q = malloc((size_t)N * sizeof *q);
    int32_t *parent = malloc((size_t)N * sizeof *parent);
    uint8_t *how = malloc((size_t)N);           /* the move that led there: its way + 4 * its cube */
    int16_t *depth = malloc((size_t)N * sizeof *depth);
    if (!seen || !q || !parent || !how || !depth) goto done;
    int head = 0, tail = 0;
    q[tail] = *s;
    parent[tail] = -1;
    depth[tail] = 0;
    seen[id_of(m, s)] = 1;
    tail++;
    while (head < tail && found < 0) {
        int at = head++;
        rlb_state cur = q[at];
        int cubes = cur.o == RLB_TWO ? 2 : 1;
        for (int k = 0; k < cubes && found < 0; k++) {
            cur.act = (int8_t)k;
            for (int d = 0; d < 4; d++) {
                rlb_state n;
                int ev;
                if (!rlb_try(m, &cur, d, &n, &ev)) continue;
                int id = id_of(m, &n);
                if (seen[id]) continue;
                seen[id] = 1;
                q[tail] = n;
                parent[tail] = at;
                how[tail] = (uint8_t)(d + 4 * k);
                depth[tail] = (int16_t)(depth[at] + 1);
                if (ev & RLB_EV_WIN) { found = tail; tail++; break; }
                tail++;
            }
        }
    }
    if (states) *states = tail;
    if (found < 0) goto done;
    best = depth[found];
    while (parent[found] > 0) found = parent[found];
    if (dir) *dir = how[found] & 3;
    if (cube) *cube = how[found] >> 2;
done:
    free(seen);
    free(q);
    free(parent);
    free(how);
    free(depth);
    return best;
}

/* every cell some reachable state touches (bit c of a byte array), for the maker's clean-up */
static void touched(const rlb_map *m, uint8_t *mark)
{
    int G = groups_of(m), N = per_open(m) << G;
    uint8_t *seen = calloc((size_t)N, 1);
    rlb_state *q = malloc((size_t)N * sizeof *q);
    memset(mark, 0, RLB_CELLS);
    if (!seen || !q) { free(seen); free(q); return; }
    int head = 0, tail = 0;
    q[tail++] = start_of(m);
    seen[id_of(m, &q[0])] = 1;
    while (head < tail) {
        rlb_state cur = q[head++];
        int c[2], n = rlb_cells(&cur, m->w, c);
        for (int i = 0; i < n; i++) mark[c[i]] = 1;
        if (rlb_solved(&cur, m)) continue;
        for (int k = 0; k < (cur.o == RLB_TWO ? 2 : 1); k++) {
            cur.act = (int8_t)k;
            for (int d = 0; d < 4; d++) {
                rlb_state nx;
                if (!rlb_try(m, &cur, d, &nx, NULL)) continue;
                int id = id_of(m, &nx);
                if (seen[id]) continue;
                seen[id] = 1;
                q[tail++] = nx;
            }
        }
    }
    free(seen);
    free(q);
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
static int par_of(const rlb_map *m)
{
    rlb_state s = start_of(m);
    return rlb_solve(m, &s, NULL, NULL, NULL);
}

static int special(const rlb_map *m, int c)
{
    int t = m->t[c];
    return c == m->start || t == RLB_HOLE || t == RLB_SOFT || t == RLB_HEAVY || t == RLB_BRIDGE || t == RLB_SPLIT ||
           t == RLB_RING;
}

/* the map cut down to the cells used (its void rows and columns at the edges taken away) */
static void crop(rlb_map *m)
{
    int x0 = m->w, x1 = -1, y0 = m->h, y1 = -1;
    for (int c = 0; c < m->w * m->h; c++)
        if (m->t[c] != RLB_VOID) {
            int x = c % m->w, y = c / m->w;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    if (x1 < 0) return;
    rlb_map n = *m;
    n.w = x1 - x0 + 1;
    n.h = y1 - y0 + 1;
    memset(n.t, 0, sizeof n.t);
    memset(n.grp, 0, sizeof n.grp);
#define MOVE(c) ((c) < 0 ? -1 : (c) % m->w - x0 + ((c) / m->w - y0) * n.w)
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            n.t[x - x0 + (y - y0) * n.w] = m->t[x + y * m->w];
            n.grp[x - x0 + (y - y0) * n.w] = m->grp[x + y * m->w];
        }
    n.start = MOVE(m->start);
    n.hole = MOVE(m->hole);
    n.split = MOVE(m->split);
    n.ring[0] = MOVE(m->ring[0]);
    n.ring[1] = MOVE(m->ring[1]);
#undef MOVE
    *m = n;
}

/* a plain tile of the zone (the columns or rows lo..hi), drawn by chance; -1 none */
static int pick_tile(const rlb_map *m, int across, int lo, int hi, uint32_t *r)
{
    int got = -1, n = 0;
    for (int c = 0; c < m->w * m->h; c++) {
        int k = across ? c / m->w : c % m->w;
        if (k < lo || k > hi || m->t[c] != RLB_TILE || c == m->start) continue;
        if (pr_below(r, ++n) == 0) got = c;
    }
    return got;
}

void rlb_make(rlb_map *m, const rlb_spec *sp, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    memset(m, 0, sizeof *m);
    m->w = sp->w;
    m->h = sp->h;
    m->split = m->ring[0] = m->ring[1] = -1;
    for (int c = 0; c < m->w * m->h; c++) m->t[c] = RLB_TILE;
    /* the zones: gaps of void across the map (a column each, or a row each), one a bridge or the split crosses */
    int gaps = sp->bridges + (sp->split ? 1 : 0), across = pr_below(&r, 2);   /* across: the gaps are rows */
    int len = across ? m->h : m->w, side = across ? m->w : m->h, cut[4], zlo[5], zhi[5];
    for (int k = 0; k < gaps; k++) cut[k] = (k + 1) * (len + 1) / (gaps + 1) - 1;
    for (int z = 0; z <= gaps; z++) {
        zlo[z] = z ? cut[z - 1] + 1 : 0;
        zhi[z] = z < gaps ? cut[z] - 1 : len - 1;
    }
    for (int k = 0; k < gaps; k++)
        for (int i = 0; i < side; i++) m->t[across ? i + cut[k] * m->w : cut[k] + i * m->w] = RLB_VOID;
    m->start = pick_tile(m, across, zlo[0], zlo[0] + (zhi[0] - zlo[0]) / 2, &r);
    if (m->start < 0) m->start = 0;
    int z = gaps, hole = pick_tile(m, across, zhi[z] - (zhi[z] - zlo[z]) / 2, zhi[z], &r);
    if (hole < 0) hole = m->w * m->h - 1;
    m->t[hole] = RLB_HOLE;
    m->hole = hole;
    int split_gap = sp->split ? pr_below(&r, gaps) : -1;
    for (int k = 0, g = 0; k < gaps; k++) {
        if (k == split_gap) {                   /* the split tile before the gap, its rings after it, apart */
            int s = pick_tile(m, across, zlo[k], zhi[k], &r), a = pick_tile(m, across, zlo[k + 1], zhi[k + 1], &r);
            if (s < 0 || a < 0) continue;
            m->t[s] = RLB_SPLIT;
            m->split = s;
            m->t[a] = RLB_RING;
            m->ring[0] = a;
            int b = -1;
            for (int t = 0; t < 30 && b < 0; t++) {
                b = pick_tile(m, across, zlo[k + 1], zhi[k + 1], &r);
                if (b >= 0 && abs(b % m->w - a % m->w) + abs(b / m->w - a / m->w) < 2) b = -1;
            }
            if (b < 0) { m->t[s] = m->t[a] = RLB_TILE; m->split = m->ring[0] = -1; continue; }
            m->t[b] = RLB_RING;
            m->ring[1] = b;
            continue;
        }
        /* a bridge in the gap, one or two cells long, its button somewhere before the gap */
        int at = pr_below(&r, side), two = pr_below(&r, 2) && at + 1 < side;
        for (int i = 0; i <= two; i++) {
            int c = across ? at + i + cut[k] * m->w : cut[k] + (at + i) * m->w;
            m->t[c] = RLB_BRIDGE;
            m->grp[c] = (uint8_t)g;
        }
        int b = pick_tile(m, across, 0, cut[k] - 1, &r);
        if (b >= 0) {
            m->t[b] = (uint8_t)(pr_below(&r, 3) ? RLB_SOFT : RLB_HEAVY);
            m->grp[b] = (uint8_t)g;
        }
        g++;
    }
    int par = par_of(m);
    for (int it = 0; it < sp->iters; it++) {
        rlb_map keep = *m;
        int kind = pr_below(&r, 100), c = pr_below(&r, m->w * m->h), t = m->t[c], need = 0;
        int k = across ? c / m->w : c % m->w, in_gap = 0;
        for (int i = 0; i < gaps; i++) in_gap |= k == cut[i];
        if (kind < 55) {                        /* a tile taken away */
            if ((t != RLB_TILE && t != RLB_FRAGILE) || c == m->start) continue;
            m->t[c] = RLB_VOID;
        } else if (kind < 68) {                 /* a tile put back beside the others */
            if (t != RLB_VOID || in_gap) continue;
            int near = 0;
            for (int d = 0; d < 4; d++) near |= floor_at(m, 0xff, c % m->w + DX[d], c / m->w + DY[d]);
            if (!near) continue;
            m->t[c] = RLB_TILE;
            need = 1;
        } else if (kind < 84) {                 /* a tile cracked, or made whole again */
            if (!sp->fragile || c == m->start || (t != RLB_TILE && t != RLB_FRAGILE)) continue;
            m->t[c] = (uint8_t)(t == RLB_TILE ? RLB_FRAGILE : RLB_TILE);
            need = t == RLB_FRAGILE;
        } else {                                /* the start, the hole or a button moved to another tile of its zone */
            if (t != RLB_TILE || c == m->start || in_gap) continue;
            int z0 = 0;
            for (int i = 0; i < gaps; i++) z0 += k > cut[i];
            int which = pr_below(&r, 3), from = -1;
            if (which == 0) from = m->start;
            else if (which == 1) from = m->hole;
            else {
                int n = 0;
                for (int e = 0; e < m->w * m->h; e++)
                    if ((m->t[e] == RLB_SOFT || m->t[e] == RLB_HEAVY) && pr_below(&r, ++n) == 0) from = e;
            }
            if (from < 0) continue;
            int fk = across ? from / m->w : from % m->w, fz = 0;
            for (int i = 0; i < gaps; i++) fz += fk > cut[i];
            if (which < 2 && fz != z0) continue;    /* the start and the hole keep to their zones */
            if (which == 0) m->start = c;
            else {
                m->t[c] = m->t[from];
                m->grp[c] = m->grp[from];
                m->t[from] = RLB_TILE;
                m->grp[from] = 0;
                if (which == 1) m->hole = c;
            }
        }
        int p = par_of(m);
        if (p < 0 || p < par + need) *m = keep;
        else par = p;
    }
    for (int c = 0; c < m->w * m->h; c++)      /* the cracks that do not matter made whole again */
        if (m->t[c] == RLB_FRAGILE) {
            m->t[c] = RLB_TILE;
            if (par_of(m) != par) m->t[c] = RLB_FRAGILE;
        }
    uint8_t mark[RLB_CELLS];                    /* the tiles never touched taken away */
    touched(m, mark);
    for (int c = 0; c < m->w * m->h; c++)
        if (!mark[c] && !special(m, c)) m->t[c] = RLB_VOID;
    crop(m);
    if (m->ring[0] > m->ring[1]) {              /* the rings in reading order */
        int t = m->ring[0];
        m->ring[0] = m->ring[1];
        m->ring[1] = t;
    }
}

int rlb_script(rlb_game *g, const char *s)
{
    for (; *s; s++) {
        const char *k = strchr("urdl", *s);
        if (*s == ' ' || *s == ',') continue;
        if (*s == 'x') { if (!rlb_choose(g)) return 0; continue; }
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            do {
                int d, cube;
                if (rlb_solve(&g->m, &g->s, &d, &cube, NULL) <= 0) break;
                if (g->s.o == RLB_TWO) g->s.act = (int8_t)cube;
                rlb_move(g, d);
            } while (all && !rlb_solved(&g->s, &g->m));
            continue;
        }
        if (!k || !*s || !rlb_move(g, (int)(k - "urdl"))) return 0;
    }
    return 1;
}

/* ---- the variants and the levels --------------------------------------------------------------------------------- */
void rlb_variant(int v, rlb_spec *sp)
{
    static const rlb_spec V[RLB_VARIANTS] = {{6, 5, 0, 0, 0, 90}, {7, 6, 1, 1, 0, 200}, {9, 7, 1, 1, 1, 420}};
    *sp = V[v < 0 || v >= RLB_VARIANTS ? 0 : v];
}

#ifndef RLB_GEN                                 /* not in the levels' tool (tools/rollblock_gen.c): it makes them */
#include "games/rollblock_levels.h"

int rlb_level(rlb_game *g, int level)
{
    if (level < 0 || level >= RLB_LEVELS || !rlb_parse(&g->m, rlb_levels[level])) return 0;
    rlb_begin(g);
    g->par = rlb_level_par[level];
    return 1;
}

/* a puzzle at random: the best of a few made to the variant (the most moves), never fewer than 4 */
void rlb_random(rlb_game *g, int variant, uint32_t seed)
{
    rlb_spec sp;
    rlb_variant(variant, &sp);
    uint32_t r = pr_seed(seed);
    int best = -1;
    for (int k = 0; k < 40 && (k < 3 || best < 4); k++) {
        rlb_map m;
        rlb_make(&m, &sp, pr_next(&r));
        int p = par_of(&m);
        if (p > best) { best = p; g->m = m; }
    }
    if (best < 0) { rlb_level(g, 0); return; }   /* (never seen) */
    rlb_begin(g);
    g->par = best;
}
#endif
