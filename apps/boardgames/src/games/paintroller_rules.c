/*
 * Paint Roller: the rules (paintroller.h): maps as text, pushes, the paint, the special tiles, undo, the
 * breadth-first search for the fewest pushes (the par, the hint), the puzzles' maker, the levels. No SDL: tested by
 * tests/paintroller_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/paintroller.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/symorbit.h"

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

/* ---- maps as text ------------------------------------------------------------------------------------------------ */
static void number_bits(pnr_map *m)             /* each coat's bit; the portals' partners */
{
    m->nbits = 0;
    int first[3] = {-1, -1, -1};
    for (int c = 0; c < PNR_CELLS; c++) m->bit[c][0] = m->bit[c][1] = m->other[c] = -1;
    for (int c = 0; c < m->w * m->h; c++) {
        int t = m->t[c];
        if (t == PNR_TILE || t == PNR_ARROW || t == PNR_DOUBLE) m->bit[c][0] = (int16_t)m->nbits++;
        if (t == PNR_DOUBLE) m->bit[c][1] = (int16_t)m->nbits++;
        if (t == PNR_PORTAL) {
            int p = m->k[c];
            if (first[p] < 0) first[p] = c;
            else { m->other[c] = (int16_t)first[p]; m->other[first[p]] = (int16_t)c; }
        }
    }
}

int pnr_parse(pnr_map *m, const char *s)
{
    memset(m, 0, sizeof *m);
    m->start = -1;
    int x = 0, y = 0, portals[3] = {0, 0, 0};
    uint8_t t[PNR_CELLS] = {0}, k[PNR_CELLS] = {0};
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
        if (x >= PNR_MAXW || y >= PNR_MAXH) return 0;
        int c = x + y * PNR_MAXW;
        const char *a = strchr("^>v<", *s);
        if (*s == '#') t[c] = PNR_WALL;
        else if (*s == '.') t[c] = PNR_TILE;
        else if (*s == 'S') { t[c] = PNR_TILE; if (m->start >= 0) return 0; m->start = c; }
        else if (*s == '2') t[c] = PNR_DOUBLE;
        else if (*s >= 'A' && *s <= 'C') { t[c] = PNR_PORTAL; k[c] = (uint8_t)(*s - 'A'); portals[*s - 'A']++; }
        else if (a) { t[c] = PNR_ARROW; k[c] = (uint8_t)(a - "^>v<"); }
        else return 0;
        x++;
    }
    m->h = y;
    if (!m->w || !m->h || m->start < 0) return 0;
    for (int p = 0; p < 3; p++)
        if (portals[p] != 0 && portals[p] != 2) return 0;
    for (int yy = 0; yy < m->h; yy++)
        for (int xx = 0; xx < m->w; xx++) {
            m->t[xx + yy * m->w] = t[xx + yy * PNR_MAXW];
            m->k[xx + yy * m->w] = k[xx + yy * PNR_MAXW];
        }
    m->start = m->start % PNR_MAXW + m->start / PNR_MAXW * m->w;
    number_bits(m);
    return m->nbits <= 64 * PNR_WORDS;
}

void pnr_text(const pnr_map *m, char *out)
{
    int n = 0;
    for (int y = 0; y < m->h; y++) {
        if (y) out[n++] = '|';
        for (int x = 0; x < m->w; x++) {
            int c = x + y * m->w;
            char ch = '#';
            switch (m->t[c]) {
            case PNR_TILE: ch = c == m->start ? 'S' : '.'; break;
            case PNR_DOUBLE: ch = '2'; break;
            case PNR_PORTAL: ch = (char)('A' + m->k[c]); break;
            case PNR_ARROW: ch = "^>v<"[m->k[c] & 3]; break;
            }
            out[n++] = ch;
        }
    }
    out[n] = 0;
}

/* ---- pushes ------------------------------------------------------------------------------------------------------ */
static int has_bit(const pnr_state *s, int b) { return b >= 0 && (s->paint[b >> 6] >> (b & 63) & 1); }
static void set_bit(pnr_state *s, int b) { s->paint[b >> 6] |= 1ull << (b & 63); }

static void coat(const pnr_map *m, pnr_state *s, int c)   /* one more coat on c, if it takes it */
{
    if (m->bit[c][0] < 0) return;
    if (!has_bit(s, m->bit[c][0])) set_bit(s, m->bit[c][0]);
    else if (m->bit[c][1] >= 0) set_bit(s, m->bit[c][1]);
}

static pnr_state start_of(const pnr_map *m)
{
    pnr_state s;
    memset(&s, 0, sizeof s);
    s.pos = m->start;
    coat(m, &s, m->start);
    return s;
}

void pnr_begin(pnr_game *g)
{
    g->s = start_of(&g->m);
    g->nh = g->moves = 0;
}

int pnr_coats(const pnr_map *m, const pnr_state *s, int c) { return has_bit(s, m->bit[c][0]) + has_bit(s, m->bit[c][1]); }

int pnr_need(const pnr_map *m, int c) { return (m->bit[c][0] >= 0) + (m->bit[c][1] >= 0); }

int pnr_try(const pnr_map *m, const pnr_state *s, int dir, pnr_state *out, int *path, int *np)
{
    int cur = s->pos, n = 0, steps = 0, limit = 4 * m->w * m->h;
    if (m->t[cur] == PNR_ARROW && m->k[cur] != dir) return 0;   /* off an arrow: along it only */
    pnr_state ns = *s;
    if (path) path[n] = cur;
    n++;
    for (;;) {
        int x = cur % m->w + DX[dir], y = cur / m->w + DY[dir];
        if (x < 0 || y < 0 || x >= m->w || y >= m->h) break;
        int c = x + y * m->w, t = m->t[c];
        if (t == PNR_WALL || (t == PNR_ARROW && m->k[c] != dir)) break;
        if (++steps > limit) return 0;          /* round and round through portals: never */
        if (t == PNR_PORTAL) {
            if (path && n + 2 <= PNR_PATH) { path[n] = c; path[n + 1] = m->other[c]; }
            n += 2;
            cur = m->other[c];
            continue;
        }
        coat(m, &ns, c);
        if (path && n < PNR_PATH) path[n] = c;
        n++;
        cur = c;
    }
    if (cur == s->pos && steps == 0) return 0;
    if (n > PNR_PATH) n = PNR_PATH;
    ns.pos = cur;
    if (out) *out = ns;
    if (np) *np = n;
    return 1;
}

int pnr_solved(const pnr_map *m, const pnr_state *s)
{
    for (int w = 0; w < PNR_WORDS; w++) {
        int lo = w * 64, k = m->nbits - lo;
        uint64_t full = k >= 64 ? ~0ull : k <= 0 ? 0 : (1ull << k) - 1;
        if ((s->paint[w] & full) != full) return 0;
    }
    return 1;
}

int pnr_move(pnr_game *g, int dir)
{
    pnr_state n;
    if (dir < 0 || dir > 3 || pnr_solved(&g->m, &g->s) || !pnr_try(&g->m, &g->s, dir, &n, NULL, NULL)) return 0;
    if (g->nh >= PNR_HIST) {
        memmove(g->hist, g->hist + PNR_HIST / 2, sizeof g->hist / 2);
        g->nh -= PNR_HIST / 2;
    }
    g->hist[g->nh++] = g->s;
    g->s = n;
    g->moves++;
    return 1;
}

int pnr_undo(pnr_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    g->moves--;
    return 1;
}

/* ---- the search: states kept in a table, found again by a hash ------------------------------------------------- */
static uint32_t hash_of(const pnr_state *s)
{
    uint64_t h = (uint64_t)s->pos * 0x9e3779b97f4a7c15ull;
    for (int w = 0; w < PNR_WORDS; w++) {
        h ^= s->paint[w] + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h *= 0xff51afd7ed558ccdull;
    }
    return (uint32_t)(h ^ (h >> 31));
}

static int same(const pnr_state *a, const pnr_state *b)
{
    return a->pos == b->pos && !memcmp(a->paint, b->paint, sizeof a->paint);
}

int pnr_solve(const pnr_map *m, const pnr_state *s, int cap, int *dir, int *states)
{
    if (states) *states = 0;
    if (pnr_solved(m, s)) return 0;
    int size = 1;
    while (size < 2 * cap) size <<= 1;
    pnr_state *q = malloc((size_t)cap * sizeof *q);
    int32_t *tab = malloc((size_t)size * sizeof *tab);
    int16_t *depth = malloc((size_t)cap * sizeof *depth);
    int8_t *first = malloc((size_t)cap);
    int best = -1, tail = 0, head = 0;
    if (!q || !tab || !depth || !first) goto done;
    memset(tab, -1, (size_t)size * sizeof *tab);
    q[0] = *s;
    depth[0] = 0;
    first[0] = -1;
    tab[hash_of(s) & (size - 1)] = 0;
    tail = 1;
    while (head < tail && best < 0) {
        int at = head++;
        for (int d = 0; d < 4; d++) {
            pnr_state n;
            if (!pnr_try(m, &q[at], d, &n, NULL, NULL)) continue;
            uint32_t h = hash_of(&n) & (size - 1);
            while (tab[h] >= 0 && !same(&q[tab[h]], &n)) h = (h + 1) & (size - 1);
            if (tab[h] >= 0) continue;
            if (tail >= cap) { tail = cap; head = tail; break; }   /* too many: given up */
            tab[h] = tail;
            q[tail] = n;
            depth[tail] = (int16_t)(depth[at] + 1);
            first[tail] = (int8_t)(at == 0 ? d : first[at]);
            if (pnr_solved(m, &n)) {
                best = depth[tail];
                if (dir) *dir = first[tail];
                tail++;
                break;
            }
            tail++;
        }
    }
done:
    if (states) *states = tail;
    free(q);
    free(tab);
    free(depth);
    free(first);
    return best;
}

int pnr_script(pnr_game *g, const char *s)
{
    for (; *s; s++) {
        if (*s == ' ' || *s == ',') continue;
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            do {
                int d;
                if (pnr_solve(&g->m, &g->s, 400000, &d, NULL) <= 0) break;
                pnr_move(g, d);
            } while (all && !pnr_solved(&g->m, &g->s));
            continue;
        }
        const char *k = strchr("urdl", *s);
        if (!k || !pnr_move(g, (int)(k - "urdl"))) return 0;
    }
    return 1;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
/* a push that goes round and round through the portals, from some tile some way: such a floor is never made */
static int loops(const pnr_map *m)
{
    pnr_state s;
    memset(&s, 0, sizeof s);
    for (int c = 0; c < m->w * m->h; c++) {
        if (m->t[c] == PNR_WALL) continue;
        s.pos = c;
        for (int d = 0; d < 4; d++) {
            int x = c % m->w + DX[d], y = c / m->w + DY[d];
            if (x < 0 || y < 0 || x >= m->w || y >= m->h || m->t[x + y * m->w] == PNR_WALL) continue;
            int n = x + y * m->w;
            if (m->t[c] == PNR_ARROW || (m->t[n] == PNR_ARROW && m->k[n] != d)) continue;
            if (!pnr_try(m, &s, d, NULL, NULL, NULL)) return 1;   /* it could go, yet refused: round and round */
        }
    }
    return 0;
}

static int par_of(const pnr_map *m, int cap)
{
    if (loops(m)) return -1;
    pnr_state s = start_of(m);
    return pnr_solve(m, &s, cap, NULL, NULL);
}

static void crop(pnr_map *m)                    /* the walls round the edges taken away */
{
    int x0 = m->w, x1 = -1, y0 = m->h, y1 = -1;
    for (int c = 0; c < m->w * m->h; c++)
        if (m->t[c] != PNR_WALL) {
            int x = c % m->w, y = c / m->w;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    if (x1 < 0) return;
    pnr_map n = *m;
    n.w = x1 - x0 + 1;
    n.h = y1 - y0 + 1;
    memset(n.t, 0, sizeof n.t);
    memset(n.k, 0, sizeof n.k);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            n.t[x - x0 + (y - y0) * n.w] = m->t[x + y * m->w];
            n.k[x - x0 + (y - y0) * n.w] = m->k[x + y * m->w];
        }
    n.start = m->start % m->w - x0 + (m->start / m->w - y0) * n.w;
    *m = n;
    number_bits(m);
}

/* the floor carved by a ball rolling at random, each roll's cells and their images made tiles, the wall that stops it
 * kept a wall for good (so every roll made stays as it was: the floor can be painted whole by them, mostly) */
static void carve(pnr_map *m, int sym, int rolls, uint32_t *r)
{
    uint8_t stop[PNR_CELLS] = {0};
    int o[4], st[4], wh = m->w * m->h;
    for (int c = 0; c < wh; c++) m->t[c] = PNR_WALL;
    int n = sym_orbit(sym, m->w, m->h, m->start, o, st);
    for (int i = 0; i < n; i++) m->t[o[i]] = PNR_TILE;
    int pos = m->start;
    for (int k = 0; k < rolls; k++) {
        int d = pr_below(r, 4), run = 0, x = pos % m->w, y = pos / m->w;
        while (1) {                             /* how far it may go: to the edge or a wall kept */
            int nx = x + DX[d] * (run + 1), ny = y + DY[d] * (run + 1);
            if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h || stop[nx + ny * m->w]) break;
            run++;
        }
        if (!run) continue;
        int len = 1 + pr_below(r, run), end = pos + (DX[d] + DY[d] * m->w) * len;
        int bx = end % m->w + DX[d], by = end / m->w + DY[d], beyond = -1;
        if (bx >= 0 && by >= 0 && bx < m->w && by < m->h) beyond = bx + by * m->w;
        if (beyond >= 0 && m->t[beyond] != PNR_WALL) continue;   /* it would roll on */
        uint8_t keep[PNR_CELLS];
        memcpy(keep, m->t, sizeof keep);
        int ok = 1;
        for (int i = 1; i <= len && ok; i++) {
            int c = pos + (DX[d] + DY[d] * m->w) * i, nn = sym_orbit(sym, m->w, m->h, c, o, st);
            for (int j = 0; j < nn; j++) {
                if (stop[o[j]] || o[j] == beyond) ok = 0;
                m->t[o[j]] = PNR_TILE;
            }
        }
        if (!ok) { memcpy(m->t, keep, sizeof keep); continue; }
        if (beyond >= 0) stop[beyond] = 1;
        pos = end;
    }
}

static int is_special(int t) { return t == PNR_DOUBLE || t == PNR_PORTAL || t == PNR_ARROW; }

/* a set of tiles made special: doubles, arrows (turned with the symmetry); a pair of portals (a tile and its image
 * by the half turn or the mirror, apart) */
static int lay(pnr_map *m, int kind, int sym, int c, uint32_t *r)
{
    int o[4], st[4], n = sym_orbit(sym, m->w, m->h, c, o, st), wh = m->w * m->h;
    for (int i = 0; i < n; i++)
        if (m->t[o[i]] != PNR_TILE || o[i] == m->start) return 0;
    if (kind == PNR_PORTAL) {
        int e = sym_image(sym, m->w, m->h, c, sym_half(sym)), used[3] = {0, 0, 0}, p = 0;
        if (e == c || m->t[e] != PNR_TILE || e == m->start) return 0;
        if (abs(e % m->w - c % m->w) + abs(e / m->w - c / m->w) < 3) return 0;
        for (int i = 0; i < wh; i++)
            if (m->t[i] == PNR_PORTAL) used[m->k[i]] = 1;
        while (p < 3 && used[p]) p++;
        if (p == 3) return 0;
        m->t[c] = m->t[e] = PNR_PORTAL;
        m->k[c] = m->k[e] = (uint8_t)p;
        return 1;
    }
    int dir = pr_below(r, 4);
    for (int i = 0; i < n; i++) {
        m->t[o[i]] = (uint8_t)kind;
        m->k[o[i]] = (uint8_t)(kind == PNR_ARROW ? (st[i] < 0 ? dir : sym_way(sym, dir, st[i])) : 0);
    }
    return 1;
}

void pnr_make(pnr_map *m, const pnr_spec *sp, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    int best = -1;
    pnr_map got;
    memset(&got, 0, sizeof got);
    for (int tries = 0; tries < 12 && best < 0; tries++) {   /* a floor that can be painted whole */
        memset(m, 0, sizeof *m);
        m->w = sp->w;
        m->h = sp->h;
        int sym = pr_below(&r, 4);
        m->start = pr_below(&r, m->w * m->h);
        carve(m, sym, sp->rolls, &r);
        number_bits(m);
        int par = par_of(m, sp->cap);
        if (par < 0) continue;
        int lays[3] = {sp->doubles, sp->portals, sp->arrows}, kinds[3] = {PNR_DOUBLE, PNR_PORTAL, PNR_ARROW};
        for (int j = 0; j < 3; j++)
            for (int k = 0, laid = 0; k < 4 * lays[j] && laid < lays[j]; k++) {   /* (a few tries each) */
                pnr_map keep = *m;
                if (!lay(m, kinds[j], sym, pr_below(&r, m->w * m->h), &r)) continue;
                number_bits(m);
                int p = par_of(m, sp->cap);
                if (p < par) *m = keep;
                else { par = p; laid++; }
            }
        for (int it = 0; it < sp->iters; it++) {
            pnr_map keep = *m;
            int kind = pr_below(&r, 100), c = pr_below(&r, m->w * m->h), o[4], st[4];
            int n = sym_orbit(sym, m->w, m->h, c, o, st), plain = 1, special = 1;
            for (int i = 0; i < n; i++) {
                plain &= m->t[o[i]] == PNR_TILE && o[i] != m->start;
                special &= is_special(m->t[o[i]]) && m->t[o[i]] != PNR_PORTAL;
            }
            if (kind < 35) {                    /* a set of tiles made walls */
                if (!plain) continue;
                for (int i = 0; i < n; i++) m->t[o[i]] = PNR_WALL;
            } else if (kind < 50) {             /* a set of walls made tiles */
                int walls = 1;
                for (int i = 0; i < n; i++) walls &= m->t[o[i]] == PNR_WALL;
                if (!walls) continue;
                for (int i = 0; i < n; i++) m->t[o[i]] = PNR_TILE;
            } else if (kind < 75) {             /* a special set laid */
                int w3[3] = {sp->doubles ? 3 : 0, sp->portals ? 1 : 0, sp->arrows ? 2 : 0}, tot = w3[0] + w3[1] + w3[2];
                if (!tot) continue;
                int q = pr_below(&r, tot), kd = q < w3[0] ? PNR_DOUBLE : q < w3[0] + w3[1] ? PNR_PORTAL : PNR_ARROW;
                if (!lay(m, kd, sym, c, &r)) continue;
            } else if (kind < 85) {             /* a special set (not portals) made plain */
                if (!special) continue;
                for (int i = 0; i < n; i++) { m->t[o[i]] = PNR_TILE; m->k[o[i]] = 0; }
            } else {                            /* the start moved */
                if (!plain || m->t[c] != PNR_TILE) continue;
                m->start = c;
            }
            number_bits(m);
            int p = par_of(m, sp->cap);
            if (p < 0 || p < par) *m = keep;
            else par = p;
        }
        for (int c = 0; c < m->w * m->h; c++) {   /* the special tiles that do not matter made plain */
            if (!is_special(m->t[c])) continue;
            pnr_map keep = *m;
            if (m->t[c] == PNR_PORTAL) {
                int e = m->other[c];
                m->t[c] = m->t[e] = PNR_TILE;
            } else {
                int o[4], st[4], n = sym_orbit(sym, m->w, m->h, c, o, st);
                for (int i = 0; i < n; i++)
                    if (m->t[o[i]] == m->t[c] && o[i] != c) m->t[o[i]] = PNR_TILE;
                m->t[c] = PNR_TILE;
            }
            number_bits(m);
            if (par_of(m, sp->cap) != par) *m = keep;
        }
        for (int c = 0; c < m->w * m->h; c++)
            if (m->t[c] != PNR_PORTAL && m->t[c] != PNR_ARROW) m->k[c] = 0;
        crop(m);
        best = par;
        got = *m;
    }
    if (best < 0) pnr_parse(m, "S..|#.#|...");  /* (never seen) */
    else *m = got;
}

/* ---- the variants and the levels --------------------------------------------------------------------------------- */
void pnr_variant(int v, pnr_spec *sp)
{
    static const pnr_spec V[PNR_VARIANTS] = {
        {5, 6, 10, 0, 0, 0, 15, 20000}, {7, 8, 18, 2, 0, 0, 20, 30000}, {8, 9, 24, 1, 1, 1, 25, 30000},
    };
    *sp = V[v < 0 || v >= PNR_VARIANTS ? 0 : v];
}

#ifndef PNR_GEN                                 /* not in the levels' tool (tools/paintroller_gen.c): it makes them */
#include "games/paintroller_levels.h"

int pnr_level(pnr_game *g, int level)
{
    if (level < 0 || level >= PNR_LEVELS || !pnr_parse(&g->m, pnr_levels[level])) return 0;
    pnr_begin(g);
    g->par = pnr_level_par[level];
    return 1;
}

/* a puzzle at random: of a few made to the variant, the one with the most pushes */
void pnr_random(pnr_game *g, int variant, uint32_t seed)
{
    pnr_spec sp;
    pnr_variant(variant, &sp);
    uint32_t r = pr_seed(seed);
    int best = -1;
    for (int k = 0; k < 6 && (k < 2 || best < 4); k++) {
        pnr_map m;
        pnr_make(&m, &sp, pr_next(&r));
        int p = par_of(&m, sp.cap);
        if (p > best) { best = p; g->m = m; }
    }
    if (best < 0) { pnr_level(g, 0); return; }
    pnr_begin(g);
    g->par = best;
}
#endif
