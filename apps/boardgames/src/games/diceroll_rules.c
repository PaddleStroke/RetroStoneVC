/*
 * Dice Roll: the rules (diceroll.h): the die's 24 ways, maps as text, rolls, the special tiles, undo, the
 * breadth-first search for the fewest rolls (the par, the hint, how many shortest ways), the puzzles' maker, the
 * levels. No SDL: tested by tests/diceroll_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/diceroll.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/symorbit.h"

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

/* ---- the die ----------------------------------------------------------------------------------------------------- */
static int8_t ROT[24][9];                       /* map = ROT * model */
static int8_t TOP[24], SOUTH[24], EAST[24], NEXT[24][4];

/* the face of a model direction (x, y, z: one of them +-1) */
static int face_of(int x, int y, int z)
{
    if (z) return z > 0 ? 1 : 6;
    if (y) return y > 0 ? 2 : 5;
    return x > 0 ? 3 : 4;
}

static void mul(const int8_t *a, const int8_t *b, int8_t *out)   /* out = a b (3 x 3) */
{
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) out[3 * r + c] = (int8_t)(a[3 * r] * b[c] + a[3 * r + 1] * b[3 + c] + a[3 * r + 2] * b[6 + c]);
}

/* the 24 ways, from the die as the model lies (1 up, 2 south, 3 east), by rolls; made once before main() */
__attribute__((constructor)) static void make_ways(void)
{
    /* each roll as a turn of the map: up (-y), right (+x), down (+y), left (-x) */
    static const int8_t Q[4][9] = {
        {1, 0, 0, 0, 0, -1, 0, 1, 0}, {0, 0, 1, 0, 1, 0, -1, 0, 0},
        {1, 0, 0, 0, 0, 1, 0, -1, 0}, {0, 0, -1, 0, 1, 0, 1, 0, 0},
    };
    static const int8_t I[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    int n = 1;
    memcpy(ROT[0], I, 9);
    for (int at = 0; at < n; at++)
        for (int d = 0; d < 4; d++) {
            int8_t r[9];
            mul(Q[d], ROT[at], r);
            int k = 0;
            while (k < n && memcmp(ROT[k], r, 9)) k++;
            if (k == n && n < 24) memcpy(ROT[n++], r, 9);
            NEXT[at][d] = (int8_t)k;
        }
    for (int o = 0; o < 24; o++) {              /* a map direction's model one: the row of ROT (ROT^T e) */
        const int8_t *R = ROT[o];
        TOP[o] = (int8_t)face_of(R[6], R[7], R[8]);
        SOUTH[o] = (int8_t)face_of(R[3], R[4], R[5]);
        EAST[o] = (int8_t)face_of(R[0], R[1], R[2]);
    }
}

const int8_t *dcr_rot(int o) { return ROT[o < 0 || o > 23 ? 0 : o]; }
int dcr_top(int o) { return TOP[o]; }
int dcr_south(int o) { return SOUTH[o]; }
int dcr_east(int o) { return EAST[o]; }
int dcr_roll(int o, int dir) { return NEXT[o][dir & 3]; }

int dcr_way_of(int top, int south)
{
    for (int o = 0; o < 24; o++)
        if (TOP[o] == top && SOUTH[o] == south) return o;
    return -1;
}

/* ---- maps as text ------------------------------------------------------------------------------------------------ */
int dcr_parse(dcr_map *m, const char *s)
{
    memset(m, 0, sizeof *m);
    m->start = m->goal = -1;
    if (!s[0] || !s[1] || !s[2] || s[3] != ':') return 0;
    m->face = s[0] - '0';
    m->o0 = dcr_way_of(s[1] - '0', s[2] - '0');
    if (m->face < 1 || m->face > 6 || m->o0 < 0) return 0;
    s += 4;
    int x = 0, y = 0;
    uint8_t t[DCR_CELLS] = {0}, k[DCR_CELLS] = {0};
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
        if (x >= DCR_MAXW || y >= DCR_MAXH) return 0;
        int c = x + y * DCR_MAXW;
        const char *a = strchr("^>v<", *s);
        if (*s == '.') t[c] = DCR_VOID;
        else if (*s == 'o') t[c] = DCR_TILE;
        else if (*s == 'S') { t[c] = DCR_TILE; if (m->start >= 0) return 0; m->start = c; }
        else if (*s == 'G') { t[c] = DCR_GOAL; if (m->goal >= 0) return 0; m->goal = c; }
        else if (*s >= '1' && *s <= '6') { t[c] = DCR_NUM; k[c] = (uint8_t)(*s - '0'); }
        else if (*s == 'i') t[c] = DCR_ODD;
        else if (*s == 'e') t[c] = DCR_EVEN;
        else if (a) { t[c] = DCR_ARROW; k[c] = (uint8_t)(a - "^>v<"); }
        else return 0;
        x++;
    }
    m->h = y;
    if (!m->w || !m->h || m->start < 0 || m->goal < 0) return 0;
    for (int yy = 0; yy < m->h; yy++)          /* packed w wide */
        for (int xx = 0; xx < m->w; xx++) {
            m->t[xx + yy * m->w] = t[xx + yy * DCR_MAXW];
            m->k[xx + yy * m->w] = k[xx + yy * DCR_MAXW];
        }
    m->start = m->start % DCR_MAXW + m->start / DCR_MAXW * m->w;
    m->goal = m->goal % DCR_MAXW + m->goal / DCR_MAXW * m->w;
    return 1;
}

void dcr_text(const dcr_map *m, char *out)
{
    int n = 0;
    out[n++] = (char)('0' + m->face);
    out[n++] = (char)('0' + TOP[m->o0]);
    out[n++] = (char)('0' + SOUTH[m->o0]);
    out[n++] = ':';
    for (int y = 0; y < m->h; y++) {
        if (y) out[n++] = '|';
        for (int x = 0; x < m->w; x++) {
            int c = x + y * m->w;
            char ch = '.';
            switch (m->t[c]) {
            case DCR_TILE: ch = c == m->start ? 'S' : 'o'; break;
            case DCR_GOAL: ch = 'G'; break;
            case DCR_NUM: ch = (char)('0' + m->k[c]); break;
            case DCR_ODD: ch = 'i'; break;
            case DCR_EVEN: ch = 'e'; break;
            case DCR_ARROW: ch = "^>v<"[m->k[c] & 3]; break;
            }
            out[n++] = ch;
        }
    }
    out[n] = 0;
}

/* ---- rolls ------------------------------------------------------------------------------------------------------- */
static dcr_state start_of(const dcr_map *m)
{
    dcr_state s = {(int8_t)(m->start % m->w), (int8_t)(m->start / m->w), (int8_t)m->o0};
    return s;
}

void dcr_begin(dcr_game *g)
{
    g->s = start_of(&g->m);
    g->nh = g->moves = 0;
}

int dcr_try(const dcr_map *m, const dcr_state *s, int dir, dcr_state *out)
{
    int c = s->x + s->y * m->w;
    if (m->t[c] == DCR_ARROW && m->k[c] != dir) return 0;   /* off an arrow: along it only */
    int x = s->x + DX[dir], y = s->y + DY[dir];
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    int n = x + y * m->w, o = NEXT[s->o][dir], top = TOP[o];
    switch (m->t[n]) {
    case DCR_VOID: return 0;
    case DCR_NUM: if (top != m->k[n]) return 0; break;
    case DCR_ODD: if (!(top & 1)) return 0; break;
    case DCR_EVEN: if (top & 1) return 0; break;
    case DCR_ARROW: if (m->k[n] != dir) return 0; break;
    }
    out->x = (int8_t)x;
    out->y = (int8_t)y;
    out->o = (int8_t)o;
    return 1;
}

int dcr_solved(const dcr_map *m, const dcr_state *s)
{
    return s->x + s->y * m->w == m->goal && TOP[s->o] == m->face;
}

int dcr_move(dcr_game *g, int dir)
{
    dcr_state n;
    if (dir < 0 || dir > 3 || dcr_solved(&g->m, &g->s) || !dcr_try(&g->m, &g->s, dir, &n)) return 0;
    if (g->nh >= DCR_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + DCR_HIST / 2, sizeof g->hist / 2);
        g->nh -= DCR_HIST / 2;
    }
    g->hist[g->nh++] = g->s;
    g->s = n;
    g->moves++;
    return 1;
}

int dcr_undo(dcr_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    g->moves--;
    return 1;
}

/* ---- the search -------------------------------------------------------------------------------------------------- */
#define NSTATE (DCR_CELLS * 24)

int dcr_solve(const dcr_map *m, const dcr_state *s, int *dir, int *states, int *ways)
{
    if (states) *states = 0;
    if (ways) *ways = 1;
    if (dcr_solved(m, s)) return 0;
    int16_t dist[NSTATE];
    int16_t q[NSTATE], first[NSTATE];          /* first: the way of the first roll that led there */
    uint16_t count[NSTATE];
    memset(dist, -1, sizeof dist);
    int head = 0, tail = 0, best = -1, nways = 0, firstdir = -1;
    int id0 = (s->x + s->y * m->w) * 24 + s->o;
    q[tail++] = (int16_t)id0;
    dist[id0] = 0;
    count[id0] = 1;
    first[id0] = -1;
    while (head < tail) {
        int id = q[head++], d = dist[id];
        if (best >= 0 && d >= best) break;
        dcr_state cur = {(int8_t)(id / 24 % m->w), (int8_t)(id / 24 / m->w), (int8_t)(id % 24)};
        for (int k = 0; k < 4; k++) {
            dcr_state n;
            if (!dcr_try(m, &cur, k, &n)) continue;
            int nid = (n.x + n.y * m->w) * 24 + n.o;
            if (dist[nid] < 0) {
                dist[nid] = (int16_t)(d + 1);
                count[nid] = 0;
                first[nid] = (int16_t)(d == 0 ? k : first[id]);
                if (dcr_solved(m, &n)) {
                    if (best < 0) { best = d + 1; firstdir = first[nid]; }
                } else q[tail++] = (int16_t)nid;
            }
            if (dist[nid] == d + 1) {
                int c = count[nid] + count[id];
                count[nid] = (uint16_t)(c > 1000 ? 1000 : c);
            }
        }
    }
    if (best > 0)
        for (int o = 0; o < 24; o++) {
            int id = m->goal * 24 + o;
            if (TOP[o] == m->face && dist[id] == best) nways += count[id];
        }
    if (states) *states = tail;
    if (ways) *ways = nways > 1000 ? 1000 : nways;
    if (dir) *dir = firstdir;
    return best;
}

int dcr_script(dcr_game *g, const char *s)
{
    for (; *s; s++) {
        if (*s == ' ' || *s == ',') continue;
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            do {
                int d;
                if (dcr_solve(&g->m, &g->s, &d, NULL, NULL) <= 0) break;
                dcr_move(g, d);
            } while (all && !dcr_solved(&g->m, &g->s));
            continue;
        }
        const char *k = strchr("urdl", *s);
        if (!k || !dcr_move(g, (int)(k - "urdl"))) return 0;
    }
    return 1;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
static int par_of(const dcr_map *m)
{
    dcr_state s = start_of(m);
    return dcr_solve(m, &s, NULL, NULL, NULL);
}

/* every cell some reachable state stands on */
static void touched(const dcr_map *m, uint8_t *mark)
{
    uint8_t seen[NSTATE];
    int16_t q[NSTATE];
    memset(seen, 0, sizeof seen);
    memset(mark, 0, DCR_CELLS);
    dcr_state s = start_of(m);
    int head = 0, tail = 0, id = m->start * 24 + s.o;
    q[tail++] = (int16_t)id;
    seen[id] = 1;
    while (head < tail) {
        id = q[head++];
        dcr_state cur = {(int8_t)(id / 24 % m->w), (int8_t)(id / 24 / m->w), (int8_t)(id % 24)};
        mark[id / 24] = 1;
        if (dcr_solved(m, &cur)) continue;
        for (int k = 0; k < 4; k++) {
            dcr_state n;
            if (!dcr_try(m, &cur, k, &n)) continue;
            int nid = (n.x + n.y * m->w) * 24 + n.o;
            if (!seen[nid]) { seen[nid] = 1; q[tail++] = (int16_t)nid; }
        }
    }
}

static void crop(dcr_map *m)
{
    int x0 = m->w, x1 = -1, y0 = m->h, y1 = -1;
    for (int c = 0; c < m->w * m->h; c++)
        if (m->t[c] != DCR_VOID) {
            int x = c % m->w, y = c / m->w;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    if (x1 < 0) return;
    dcr_map n = *m;
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
    n.goal = m->goal % m->w - x0 + (m->goal / m->w - y0) * n.w;
    *m = n;
}

static int plain(const dcr_map *m, int c) { return m->t[c] == DCR_TILE && c != m->start; }

/* a special tile's kind for the spec, drawn by chance (DCR_NUM, DCR_ODD, DCR_ARROW...); -1: none allowed */
static int special_kind(const dcr_spec *sp, uint32_t *r)
{
    int w[3] = {sp->nums ? 3 : 0, sp->parity ? 2 : 0, sp->arrows ? 2 : 0}, tot = w[0] + w[1] + w[2];
    if (!tot) return -1;
    int k = pr_below(r, tot);
    return k < w[0] ? DCR_NUM : k < w[0] + w[1] ? DCR_ODD : DCR_ARROW;
}

/* a set of cells (an orbit) made one kind of special tile: number tiles all the same number, odd or even all alike,
 * arrows turned with the symmetry */
static void lay(dcr_map *m, const int *o, const int *st, int n, int kind, int sym, uint32_t *r)
{
    int num = 1 + pr_below(r, 6), dir = pr_below(r, 4), even = pr_below(r, 2);
    for (int i = 0; i < n; i++) {
        int c = o[i];
        if (kind == DCR_NUM) { m->t[c] = DCR_NUM; m->k[c] = (uint8_t)num; }
        else if (kind == DCR_ODD) { m->t[c] = (uint8_t)(even ? DCR_EVEN : DCR_ODD); m->k[c] = 0; }
        else { m->t[c] = DCR_ARROW; m->k[c] = (uint8_t)(st[i] < 0 ? dir : sym_way(sym, dir, st[i])); }
    }
}

static int is_special(int t) { return t == DCR_NUM || t == DCR_ODD || t == DCR_EVEN || t == DCR_ARROW; }

void dcr_make(dcr_map *m, const dcr_spec *sp, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    memset(m, 0, sizeof *m);
    m->w = sp->w;
    m->h = sp->h;
    int wh = m->w * m->h, sym = pr_below(&r, m->w == m->h ? 5 : 4), o[4], st[4];
    for (int c = 0; c < wh; c++) m->t[c] = DCR_TILE;
    /* the start and the goal: apart, the goal often the start's image */
    int best = -1, half = sym_half(sym);
    for (int t = 0; t < 40; t++) {
        int a = pr_below(&r, wh), b = t < 20 ? sym_image(sym, m->w, m->h, a, half) : pr_below(&r, wh);
        int d = abs(a % m->w - b % m->w) + abs(a / m->w - b / m->w);
        if (a != b && d > best) { best = d; m->start = a; m->goal = b; }
        if (best >= (m->w + m->h) / 2 + 1) break;
    }
    m->t[m->goal] = DCR_GOAL;
    m->face = 1 + pr_below(&r, 6);
    m->o0 = pr_below(&r, 24);
    for (int k = 0; k < sp->holes; k++) {      /* holes in symmetric sets */
        int c = pr_below(&r, wh), n = sym_orbit(sym, m->w, m->h, c, o, st), ok = 1;
        for (int i = 0; i < n; i++) ok &= plain(m, o[i]);
        if (!ok) continue;
        for (int i = 0; i < n; i++) m->t[o[i]] = DCR_VOID;
        if (par_of(m) < 0)
            for (int i = 0; i < n; i++) m->t[o[i]] = DCR_TILE;
    }
    int lays[3] = {sp->nums, sp->parity, sp->arrows}, kinds[3] = {DCR_NUM, DCR_ODD, DCR_ARROW};
    for (int j = 0; j < 3; j++)
        for (int k = 0; k < lays[j]; k++) {
            int c = pr_below(&r, wh), n = sym_orbit(sym, m->w, m->h, c, o, st), ok = 1;
            for (int i = 0; i < n; i++) ok &= plain(m, o[i]);
            if (!ok) continue;
            dcr_map keep = *m;
            lay(m, o, st, n, kinds[j], sym, &r);
            if (par_of(m) < 0) *m = keep;
        }
    int par = par_of(m);
    for (int it = 0; it < sp->iters; it++) {
        dcr_map keep = *m;
        int kind = pr_below(&r, 100), c = pr_below(&r, wh), need = 0, n = sym_orbit(sym, m->w, m->h, c, o, st);
        int all_plain = 1, all_void = 1, all_special = 1, any_end = 0;
        for (int i = 0; i < n; i++) {
            all_plain &= plain(m, o[i]);
            all_void &= m->t[o[i]] == DCR_VOID;
            all_special &= is_special(m->t[o[i]]);
            any_end |= o[i] == m->start || o[i] == m->goal;
        }
        if (kind < 35) {                        /* a set of tiles taken away */
            if (!all_plain) continue;
            for (int i = 0; i < n; i++) m->t[o[i]] = DCR_VOID;
        } else if (kind < 45) {                 /* a set put back beside the others: only if it makes it longer */
            if (!all_void) continue;
            for (int i = 0; i < n; i++) m->t[o[i]] = DCR_TILE;
            need = 1;
        } else if (kind < 65) {                 /* a set made special */
            int sk = special_kind(sp, &r);
            if (!all_plain || sk < 0) continue;
            lay(m, o, st, n, sk, sym, &r);
        } else if (kind < 75) {                 /* a special set made plain again: only if it makes it longer */
            if (!all_special) continue;
            for (int i = 0; i < n; i++) { m->t[o[i]] = DCR_TILE; m->k[o[i]] = 0; }
            need = 1;
        } else if (kind < 85) {                 /* a special set changed: another number, odd for even, turned */
            if (!all_special) continue;
            int num = 1 + pr_below(&r, 6), turn = 1 + pr_below(&r, 3);
            for (int i = 0; i < n; i++) {
                int t = m->t[o[i]];
                if (t == DCR_NUM) m->k[o[i]] = (uint8_t)num;
                else if (t == DCR_ODD || t == DCR_EVEN) m->t[o[i]] = (uint8_t)(t == DCR_ODD ? DCR_EVEN : DCR_ODD);
                else m->k[o[i]] = (uint8_t)((m->k[o[i]] + turn) & 3);
            }
        } else if (kind < 93) {                 /* the goal's number, or the die's way at the start */
            if (pr_below(&r, 2)) m->face = 1 + pr_below(&r, 6);
            else m->o0 = pr_below(&r, 24);
        } else {                                /* the start moved to another plain tile */
            if (!all_plain || any_end) continue;
            m->start = c;
        }
        int p = par_of(m);
        if (p < 0 || p < par + need) *m = keep;
        else par = p;
    }
    for (int c = 0; c < wh; c++) {             /* the special sets that do not matter made plain */
        if (!is_special(m->t[c])) continue;
        int n = sym_orbit(sym, m->w, m->h, c, o, st);
        dcr_map keep = *m;
        for (int i = 0; i < n; i++)
            if (is_special(m->t[o[i]])) { m->t[o[i]] = DCR_TILE; m->k[o[i]] = 0; }
        if (par_of(m) != par) *m = keep;
    }
    uint8_t mark[DCR_CELLS];                    /* the sets never touched taken away */
    touched(m, mark);
    for (int c = 0; c < wh; c++) {
        if (m->t[c] == DCR_VOID) continue;
        int n = sym_orbit(sym, m->w, m->h, c, o, st), used = 0;
        for (int i = 0; i < n; i++) used |= mark[o[i]] || o[i] == m->start || o[i] == m->goal;
        if (!used)
            for (int i = 0; i < n; i++) m->t[o[i]] = DCR_VOID;
    }
    for (int c = 0; c < wh; c++)
        if (m->t[c] != DCR_NUM && m->t[c] != DCR_ARROW) m->k[c] = 0;
    crop(m);
}

/* ---- the variants and the levels --------------------------------------------------------------------------------- */
void dcr_variant(int v, dcr_spec *sp)
{
    static const dcr_spec V[DCR_VARIANTS] = {
        {5, 5, 3, 2, 0, 0, 40}, {6, 6, 4, 2, 2, 0, 120}, {7, 7, 5, 2, 2, 2, 260},
    };
    *sp = V[v < 0 || v >= DCR_VARIANTS ? 0 : v];
}

#ifndef DCR_GEN                                 /* not in the levels' tool (tools/diceroll_gen.c): it makes them */
#include "games/diceroll_levels.h"

int dcr_level(dcr_game *g, int level)
{
    if (level < 0 || level >= DCR_LEVELS || !dcr_parse(&g->m, dcr_levels[level])) return 0;
    dcr_begin(g);
    g->par = dcr_level_par[level];
    return 1;
}

/* a puzzle at random: of a few made to the variant, the one whose rolls come nearest the variant's aim (easy 10,
 * medium 16, hard 24), never fewer than 5 */
void dcr_random(dcr_game *g, int variant, uint32_t seed)
{
    static const int AIM[DCR_VARIANTS] = {10, 16, 24};
    dcr_spec sp;
    dcr_variant(variant, &sp);
    uint32_t r = pr_seed(seed);
    int best = -1, aim = AIM[variant < 0 || variant >= DCR_VARIANTS ? 0 : variant];
    for (int k = 0; k < 30 && (k < 6 || best < 5); k++) {
        dcr_map m;
        dcr_make(&m, &sp, pr_next(&r));
        int p = par_of(&m);
        if (p >= 5 && (best < 5 || abs(p - aim) < abs(best - aim))) { best = p; g->m = m; }
    }
    if (best < 0) { dcr_level(g, 0); return; }
    dcr_begin(g);
    g->par = best;
}
#endif
