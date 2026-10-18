/*
 * Tilt Maze: the rules (tiltmaze.h): the marbles rolling, the holes, gates, one-way tiles and lids, the search for the
 * fewest tilts, the maker of puzzles, undo, the notation. No SDL: tested by tests/tiltmaze_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/tiltmaze.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

/* ---- maps as text ------------------------------------------------------------------------------------------------ */
int tlm_parse(tlm_map *m, const char *text)
{
    memset(m, 0, sizeof *m);
    int x = 0, y = 0, w = -1, holes[TLM_COLOURS] = {0}, nm = 0;
    uint8_t mc[TLM_CELLS], ms[TLM_CELLS];
    uint8_t t[TLM_CELLS] = {0}, k[TLM_CELLS] = {0}, wl[TLM_CELLS] = {0};
    for (const char *p = text;; p += 2) {
        if (*p == '/' || !*p) {
            if (w < 0) w = x;
            else if (x != w) return 0;
            y++;
            x = 0;
            if (!*p) break;
            p--;                                /* the row's end is one character */
            continue;
        }
        if (!p[1] || x >= TLM_MAXW || y >= TLM_MAXH || p[1] < '0' || p[1] > '3') return 0;
        int c = x + y * TLM_MAXW;
        const char *q;
        wl[c] = (uint8_t)(p[1] - '0');
        if (*p == '.') {
        } else if (*p == '#') t[c] = TLM_BLOCK;
        else if ((q = strchr("RGBY", *p))) {
            if (nm >= TLM_MAXM) return 0;
            mc[nm] = (uint8_t)(q - "RGBY");
            ms[nm++] = (uint8_t)c;
        } else if ((q = strchr("rgby", *p))) { t[c] = TLM_HOLE; k[c] = (uint8_t)(q - "rgby"); holes[k[c]]++; }
        else if ((q = strchr("1234", *p))) { t[c] = TLM_LID; k[c] = (uint8_t)(q - "1234"); holes[k[c]]++; }
        else if ((q = strchr("klmn", *p))) { t[c] = TLM_GATE; k[c] = (uint8_t)(q - "klmn"); }
        else if ((q = strchr("^>v<", *p))) { t[c] = TLM_ONEWAY; k[c] = (uint8_t)(q - "^>v<"); }
        else return 0;
        x++;
    }
    if (w < 1 || y < 1 || !nm) return 0;
    m->w = w;
    m->h = y;
    for (int yy = 0; yy < y; yy++)
        for (int xx = 0; xx < w; xx++) {
            m->t[xx + yy * w] = t[xx + yy * TLM_MAXW];
            m->k[xx + yy * w] = k[xx + yy * TLM_MAXW];
            m->wall[xx + yy * w] = wl[xx + yy * TLM_MAXW];
        }
    int count[TLM_COLOURS] = {0};
    for (int col = 0; col < TLM_COLOURS; col++)  /* the marbles by colour, each colour's by place */
        for (int i = 0; i < nm; i++)
            if (mc[i] == col) {
                m->col[m->nm] = (uint8_t)col;
                m->start[m->nm++] = (uint8_t)(ms[i] % TLM_MAXW + ms[i] / TLM_MAXW * w);
                count[col]++;
            }
    for (int col = 0; col < TLM_COLOURS; col++)
        if (count[col] != holes[col]) return 0;
    return 1;
}

void tlm_text(const tlm_map *m, char *out)
{
    char *o = out;
    for (int y = 0; y < m->h; y++) {
        if (y) *o++ = '/';
        for (int x = 0; x < m->w; x++) {
            int c = x + y * m->w, kk = m->k[c];
            char ch = '.';
            switch (m->t[c]) {
            case TLM_BLOCK: ch = '#'; break;
            case TLM_HOLE: ch = "rgby"[kk]; break;
            case TLM_LID: ch = "1234"[kk]; break;
            case TLM_GATE: ch = "klmn"[kk]; break;
            case TLM_ONEWAY: ch = "^>v<"[kk]; break;
            default: break;
            }
            for (int i = 0; i < m->nm; i++)
                if (m->start[i] == c) ch = "RGBY"[m->col[i]];
            *o++ = ch;
            *o++ = (char)('0' + m->wall[c]);
        }
    }
    *o = 0;
}

/* ---- the tilts --------------------------------------------------------------------------------------------------- */
int tlm_wall(const tlm_map *m, int c, int dir)
{
    int x = c % m->w, y = c / m->w;
    switch (dir) {
    case TLM_UP: return y == 0 || (m->wall[c - m->w] & TLM_WALL_D);
    case TLM_RIGHT: return x == m->w - 1 || (m->wall[c] & TLM_WALL_R);
    case TLM_DOWN: return y == m->h - 1 || (m->wall[c] & TLM_WALL_D);
    default: return x == 0 || (m->wall[c - 1] & TLM_WALL_R);
    }
}

/* the holes taken (by any marble dropped in: full[c] 1) */
static void fill_of(const tlm_map *m, const tlm_state *s, uint8_t *full)
{
    memset(full, 0, (size_t)m->w * m->h);
    for (int i = 0; i < m->nm; i++)
        if (s->in >> i & 1) full[s->at[i]] = 1;
}

/* a lid lifted: every open hole of its colour taken by a marble of that colour */
static int lid_up(const tlm_map *m, const tlm_state *s, int c)
{
    for (int h = 0; h < m->w * m->h; h++) {
        if (m->t[h] != TLM_HOLE || m->k[h] != m->k[c]) continue;
        int ok = 0;
        for (int i = 0; i < m->nm; i++)
            if ((s->in >> i & 1) && !(s->stuck >> i & 1) && s->at[i] == h) ok = 1;
        if (!ok) return 0;
    }
    return 1;
}

int tlm_lid_open(const tlm_map *m, const tlm_state *s, int c) { return m->t[c] == TLM_LID && lid_up(m, s, c); }

int tlm_dead(const tlm_state *s) { return s->stuck != 0; }

static void drop(const tlm_map *m, tlm_state *s, int i, uint8_t *full)
{
    int c = s->at[i];
    s->in |= (uint8_t)(1u << i);
    full[c] = 1;
    if (m->k[c] != m->col[i]) s->stuck |= (uint8_t)(1u << i);
}

int tlm_try(const tlm_map *m, const tlm_state *s, int dir, tlm_state *out, tlm_trace *tr)
{
    if (tlm_dead(s)) return 0;
    tlm_state cur = *s;
    uint8_t occ[TLM_CELLS], full[TLM_CELLS], lifted[TLM_CELLS];   /* lifted: the lids up as the tilt begins (one
                                                                * lifting meanwhile waits for the marbles to rest) */
    int order[TLM_MAXM], n = 0, moved = 0, step = DX[dir] + DY[dir] * m->w;
    memset(occ, 0, (size_t)m->w * m->h);
    fill_of(m, &cur, full);
    for (int c = 0; c < m->w * m->h; c++) lifted[c] = (uint8_t)(m->t[c] == TLM_LID && lid_up(m, &cur, c));
    if (tr) memset(tr, 0, sizeof *tr);
    for (int i = 0; i < m->nm; i++) {
        if (tr) tr->from[i] = cur.at[i];
        if (cur.in >> i & 1) continue;
        occ[cur.at[i]] = (uint8_t)(i + 1);
        order[n++] = i;
    }
    for (int a = 0; a < n; a++)                 /* the front ones first: the farthest the way it tilts */
        for (int b = a + 1; b < n; b++) {
            int ca = cur.at[order[a]], cb = cur.at[order[b]];
            int ka = DX[dir] * (ca % m->w) + DY[dir] * (ca / m->w), kb = DX[dir] * (cb % m->w) + DY[dir] * (cb / m->w);
            if (kb > ka) { int t = order[a]; order[a] = order[b]; order[b] = t; }
        }
    for (int a = 0; a < n; a++) {
        int i = order[a], c = cur.at[i], len = 0, dropped = 0;
        while (!tlm_wall(m, c, dir)) {
            int nx = c + step, t = m->t[nx];
            if (t == TLM_BLOCK || occ[nx]) break;
            if (t == TLM_GATE && m->k[nx] != m->col[i]) break;
            if (t == TLM_ONEWAY && m->k[nx] != dir) break;
            occ[c] = 0;
            c = nx;
            len++;
            occ[c] = (uint8_t)(i + 1);
            if (!full[c] && (t == TLM_HOLE || (t == TLM_LID && lifted[c]))) {
                if (m->k[c] == m->col[i]) occ[c] = 0;   /* in another colour's hole it sticks out: in the way */
                cur.at[i] = (uint8_t)c;
                drop(m, &cur, i, full);
                dropped = 1;
                break;
            }
        }
        cur.at[i] = (uint8_t)c;
        if (tr) { tr->len[i] = len; tr->drop[i] = dropped; }
        if (len) moved = 1;
    }
    for (int i = 0; i < m->nm; i++) {           /* a lid lifted under a marble at rest: it drops in */
        int c = cur.at[i];
        if ((cur.in >> i & 1) || m->t[c] != TLM_LID || full[c] || !lid_up(m, &cur, c)) continue;
        drop(m, &cur, i, full);
        if (tr) tr->drop[i] = 2;
    }
    if (!moved) return 0;
    if (out) *out = cur;
    return 1;
}

int tlm_solved(const tlm_map *m, const tlm_state *s)
{
    return !s->stuck && s->in == (uint8_t)((1u << m->nm) - 1);
}

/* ---- the search: states by a key (each marble a byte: its square, 128 more once in a hole; each colour's marbles in
 * order: marbles of one colour alike), breadth first, each with the first tilt that led there ---------------------- */
#define MAX_STATES 500000

typedef struct {
    int32_t *slot;
    uint64_t *key;
    uint8_t *first;
    int cap, n, room;
} bfs;

static uint64_t key_of(const tlm_map *m, const tlm_state *s)
{
    uint8_t code[TLM_MAXM];
    for (int i = 0; i < m->nm; i++) code[i] = (uint8_t)(s->at[i] | (s->in >> i & 1) << 7);
    for (int i = 0; i < m->nm; i++)             /* sorted within each colour */
        for (int j = i + 1; j < m->nm && m->col[j] == m->col[i]; j++)
            if (code[j] < code[i]) { uint8_t t = code[i]; code[i] = code[j]; code[j] = t; }
    uint64_t k = 0;
    for (int i = 0; i < m->nm; i++) k = k << 8 | code[i];
    return k;
}

static tlm_state state_of(const tlm_map *m, uint64_t k)
{
    tlm_state s;
    memset(&s, 0, sizeof s);
    for (int i = m->nm - 1; i >= 0; i--, k >>= 8) {
        s.at[i] = (uint8_t)(k & 127);
        if (k & 128) s.in |= (uint8_t)(1u << i);
    }
    return s;
}

static int bfs_init(bfs *b)
{
    b->cap = 1 << 12;
    b->room = 1 << 11;
    b->n = 0;
    b->slot = malloc(sizeof *b->slot * (size_t)b->cap);
    b->key = malloc(sizeof *b->key * (size_t)b->room);
    b->first = malloc((size_t)b->room);
    if (!b->slot || !b->key || !b->first) return 0;
    memset(b->slot, -1, sizeof *b->slot * (size_t)b->cap);
    return 1;
}

static void bfs_free(bfs *b)
{
    free(b->slot);
    free(b->key);
    free(b->first);
}

static unsigned hash(uint64_t k, int cap)
{
    k ^= k >> 29;
    k *= 0xbf58476d1ce4e5b9ull;
    k ^= k >> 32;
    return (unsigned)k & (unsigned)(cap - 1);
}

static int bfs_add(bfs *b, uint64_t k, int first, int *added)
{
    *added = 0;
    unsigned h = hash(k, b->cap);
    while (b->slot[h] >= 0) {
        if (b->key[b->slot[h]] == k) return b->slot[h];
        h = (h + 1) & (unsigned)(b->cap - 1);
    }
    if (b->n >= MAX_STATES) return -1;
    if (b->n >= b->room) {
        int room = b->room * 2;
        uint64_t *nk = realloc(b->key, sizeof *nk * (size_t)room);
        if (!nk) return -1;
        b->key = nk;
        uint8_t *nf = realloc(b->first, (size_t)room);
        if (!nf) return -1;
        b->first = nf;
        b->room = room;
    }
    if (2 * (b->n + 1) > b->cap) {
        int cap = b->cap * 2;
        int32_t *ns = malloc(sizeof *ns * (size_t)cap);
        if (!ns) return -1;
        memset(ns, -1, sizeof *ns * (size_t)cap);
        for (int i = 0; i < b->n; i++) {
            unsigned g = hash(b->key[i], cap);
            while (ns[g] >= 0) g = (g + 1) & (unsigned)(cap - 1);
            ns[g] = i;
        }
        free(b->slot);
        b->slot = ns;
        b->cap = cap;
        h = hash(k, cap);
        while (b->slot[h] >= 0) h = (h + 1) & (unsigned)(cap - 1);
    }
    b->key[b->n] = k;
    b->first[b->n] = (uint8_t)first;
    b->slot[h] = b->n;
    *added = 1;
    return b->n++;
}

int tlm_solve(const tlm_map *m, const tlm_state *s0, int *dir, int *states)
{
    if (states) *states = 1;
    if (tlm_solved(m, s0)) return 0;
    if (tlm_dead(s0)) return -1;
    bfs b;
    if (!bfs_init(&b)) { bfs_free(&b); return -2; }
    int added, res = -1, head = 0, depth = 0;
    bfs_add(&b, key_of(m, s0), 0, &added);
    while (head < b.n && res == -1) {
        int end = b.n;
        for (; head < end && res == -1; head++) {
            tlm_state s = head ? state_of(m, b.key[head]) : *s0, t;
            for (int d = 0; d < 4; d++) {
                if (!tlm_try(m, &s, d, &t, NULL) || tlm_dead(&t)) continue;
                int i = bfs_add(&b, key_of(m, &t), head ? b.first[head] : d, &added);
                if (i < 0) { res = -2; break; }
                if (!added) continue;
                if (tlm_solved(m, &t)) {
                    if (dir) *dir = b.first[i];
                    res = depth + 1;
                    break;
                }
            }
        }
        depth++;
    }
    if (states) *states = b.n;
    bfs_free(&b);
    return res;
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
void tlm_begin(tlm_game *g)
{
    memset(&g->s, 0, sizeof g->s);
    for (int i = 0; i < g->m.nm; i++) g->s.at[i] = g->m.start[i];
    g->nh = g->moves = 0;
}

int tlm_move(tlm_game *g, int dir)
{
    tlm_state t;
    if (dir < 0 || dir > 3 || tlm_solved(&g->m, &g->s) || !tlm_try(&g->m, &g->s, dir, &t, NULL)) return 0;
    if (g->nh >= TLM_HIST) {
        memmove(g->hist, g->hist + TLM_HIST / 2, sizeof g->hist[0] * (TLM_HIST / 2));
        g->nh -= TLM_HIST / 2;
    }
    g->hist[g->nh++] = g->s;
    g->s = t;
    g->moves++;
    return 1;
}

int tlm_undo(tlm_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    g->moves--;
    return 1;
}

int tlm_hint(const tlm_game *g)
{
    int d = -1;
    if (tlm_solved(&g->m, &g->s)) return -1;
    return tlm_solve(&g->m, &g->s, &d, NULL) > 0 ? d : -1;
}

int tlm_script(tlm_game *g, const char *s)
{
    while (*s) {
        char c = *s++;
        if (c == ' ' || c == ',') continue;
        if (c == '=') {
            int k = 0, any = 0;
            for (; *s >= '0' && *s <= '9'; s++, any = 1) k = k * 10 + *s - '0';
            if (!any) k = 1 << 20;
            for (int i = 0; i < k && !tlm_solved(&g->m, &g->s); i++) {
                int d = tlm_hint(g);
                if (d < 0 || !tlm_move(g, d)) return 0;
            }
            continue;
        }
        const char *dirs = "urdl", *p = strchr(dirs, c | 32);
        if (!p || !c || !tlm_move(g, (int)(p - dirs))) return 0;
    }
    return 1;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
static int image(int w, int h, int sym, int c)  /* a square's image under the symmetry */
{
    int x = c % w, y = c / w;
    return sym == 1 ? (w - 1 - x) + y * w : sym == 2 ? (w - 1 - x) + (h - 1 - y) * w : sym == 3 ? x + (h - 1 - y) * w : c;
}

static void set_wall(tlm_map *m, int a, int b, int on)   /* the wall between two squares side by side */
{
    if (b < a) { int t = a; a = b; b = t; }
    int bit = b == a + 1 ? TLM_WALL_R : TLM_WALL_D;
    if (on) m->wall[a] |= (uint8_t)bit;
    else m->wall[a] &= (uint8_t)~bit;
}

static int has_wall(const tlm_map *m, int a, int b)
{
    if (b < a) { int t = a; a = b; b = t; }
    return m->wall[a] & (b == a + 1 ? TLM_WALL_R : TLM_WALL_D);
}

/* the wall between a and its neighbour the way dir, and its image, set to on */
static void wall_pair(tlm_map *m, int sym, int a, int dir, int on)
{
    int b = a + DX[dir] + DY[dir] * m->w;
    set_wall(m, a, b, on);
    set_wall(m, image(m->w, m->h, sym, a), image(m->w, m->h, sym, b), on);
}

static int plain(const tlm_map *m, int c)       /* bare floor, no marble on it */
{
    if (m->t[c] != TLM_FLOOR) return 0;
    for (int i = 0; i < m->nm; i++)
        if (m->start[i] == c) return 0;
    return 1;
}

static int blocks_of(const tlm_map *m)
{
    int n = 0;
    for (int c = 0; c < m->w * m->h; c++) n += m->t[c] == TLM_BLOCK;
    return n;
}

static int free_square(const tlm_map *m, uint32_t *rs)
{
    int cells = m->w * m->h;
    for (int k = 0; k < 4 * cells; k++) {
        int c = pr_below(rs, cells);
        if (plain(m, c)) return c;
    }
    return -1;
}

static int par_of(const tlm_map *m, int *states)
{
    tlm_game g;
    g.m = *m;
    tlm_begin(&g);
    return tlm_solve(m, &g.s, NULL, states);
}

/* the special tiles of a kind matter: plain (a lid: an open hole), the fewest tilts change */
static int matters(const tlm_map *m, int kind, int par)
{
    tlm_map w = *m;
    int any = 0;
    for (int c = 0; c < w.w * w.h; c++)
        if (w.t[c] == kind) {
            w.t[c] = kind == TLM_LID ? TLM_HOLE : TLM_FLOOR;
            any = 1;
        }
    return !any || par_of(&w, NULL) != par;
}

static void sort_marbles(tlm_map *m)            /* by colour, as tlm_parse leaves them */
{
    for (int i = 0; i < m->nm; i++)
        for (int j = i + 1; j < m->nm; j++)
            if (m->col[j] < m->col[i] || (m->col[j] == m->col[i] && m->start[j] < m->start[i])) {
                uint8_t c = m->col[i], s = m->start[i];
                m->col[i] = m->col[j];
                m->start[i] = m->start[j];
                m->col[j] = c;
                m->start[j] = s;
            }
}

int tlm_make(tlm_map *out, const tlm_spec *sp, uint32_t seed)
{
    uint32_t rs = pr_seed(seed);
    int w = sp->w, h = sp->h, cells = w * h, best_par = -1, found = 0, cols = 0;
    tlm_map best;
    memset(&best, 0, sizeof best);
    for (int k = 0; k < TLM_COLOURS; k++)
        if (sp->marbles[k]) cols++;
    for (int attempt = 0; attempt < (sp->tries ? sp->tries : 40) && !found; attempt++) {
        int sym = sp->sym >= 0 ? sp->sym : 1 + pr_below(&rs, 3);
        tlm_map m;
        memset(&m, 0, sizeof m);
        m.w = w;
        m.h = h;
        for (int c = 0; c < cells; c++)         /* the walls, a pair at a time */
            for (int d = TLM_RIGHT; d <= TLM_DOWN; d++)
                if (!tlm_wall(&m, c, d) && pr_below(&rs, 100) < sp->walls) wall_pair(&m, sym, c, d, 1);
        for (int k = 0; k < sp->blocks;) {
            int c = pr_below(&rs, cells), o = image(w, h, sym, c);
            if (m.t[c] != TLM_FLOOR || m.t[o] != TLM_FLOOR) continue;
            m.t[c] = m.t[o] = TLM_BLOCK;
            k += c == o ? 1 : 2;
        }
        int lids = sp->lids;
        for (int col = 0; col < TLM_COLOURS; col++)   /* the holes; a lid on a colour's second hole */
            for (int k = 0; k < sp->marbles[col]; k++) {
                int c = free_square(&m, &rs);
                if (c < 0) continue;
                m.t[c] = k && lids > 0 ? TLM_LID : TLM_HOLE;
                if (m.t[c] == TLM_LID) lids--;
                m.k[c] = (uint8_t)col;
            }
        for (int k = 0; k < sp->gates; k++) {
            int c = free_square(&m, &rs), col;
            do col = pr_below(&rs, TLM_COLOURS);
            while (!sp->marbles[col]);
            if (c >= 0) { m.t[c] = TLM_GATE; m.k[c] = (uint8_t)col; }
        }
        for (int k = 0; k < sp->oneway; k++) {
            int c = free_square(&m, &rs);
            if (c >= 0) { m.t[c] = TLM_ONEWAY; m.k[c] = (uint8_t)pr_below(&rs, 4); }
        }
        m.nm = 0;
        for (int col = 0; col < TLM_COLOURS; col++)
            for (int k = 0; k < sp->marbles[col]; k++) {
                int c = free_square(&m, &rs);
                if (c < 0) continue;
                m.col[m.nm] = (uint8_t)col;
                m.start[m.nm++] = (uint8_t)c;
            }
        sort_marbles(&m);
        int states = 0, par = par_of(&m, &states);
        if (par > sp->hi) par = -1;
        for (int it = 0; it < sp->iters; it++) {
            tlm_map t = m;
            int r = pr_below(&rs, 100), c = pr_below(&rs, cells);
            if (r < 50) {                       /* a wall put or taken away (with its image) */
                int d = TLM_RIGHT + pr_below(&rs, 2);
                if ((d == TLM_RIGHT && c % w == w - 1) || (d == TLM_DOWN && c / w == h - 1)) continue;
                int b = c + DX[d] + DY[d] * w;
                wall_pair(&t, sym, c, d, !has_wall(&t, c, b));
            } else if (r < 58) {                /* a block */
                int o = image(w, h, sym, c);
                if (t.t[c] == TLM_BLOCK && t.t[o] == TLM_BLOCK) t.t[c] = t.t[o] = TLM_FLOOR;
                else if (plain(&t, c) && plain(&t, o) && blocks_of(&t) < 2 * sp->blocks + 1)
                    t.t[c] = t.t[o] = TLM_BLOCK;   /* a few at most */
                else continue;
            } else if (r < 80) {                /* a marble moved */
                int i = pr_below(&rs, t.nm), d = free_square(&t, &rs);
                if (d < 0) continue;
                t.start[i] = (uint8_t)d;
                sort_marbles(&t);
            } else {                            /* a hole or a special tile moved */
                if (t.t[c] == TLM_FLOOR || t.t[c] == TLM_BLOCK) continue;
                int d = free_square(&t, &rs);
                if (d < 0) continue;
                t.t[d] = t.t[c];
                t.k[d] = t.k[c];
                t.t[c] = TLM_FLOOR;
                t.k[c] = 0;
            }
            int st = 0, p = par_of(&t, &st);
            if (p > sp->hi) p = -1;
            if (p > par || (p == par && (p < 0 || st >= states))) {
                m = t;
                par = p;
                states = st;
            }
        }
        if (par <= 0) continue;
        if (!matters(&m, TLM_GATE, par) || !matters(&m, TLM_ONEWAY, par) || !matters(&m, TLM_LID, par)) continue;
        if (par > best_par) {
            best = m;
            best_par = par;
        }
        if (par >= sp->lo) found = 1;
    }
    if (best_par <= 0) {                        /* nothing at all: one marble, its hole across the tray */
        tlm_parse(&best, "R0.0/.0.0/.0r0");
    }
    *out = best;
    (void)cols;
    return found;
}

void tlm_variant(int variant, tlm_spec *sp)
{
    static const tlm_spec V[TLM_VARIANTS] = {
        {5, 6, -1, 18, 1, {1, 1, 0, 0}, 0, 0, 0, 6, 9, 60, 0},
        {6, 7, -1, 18, 2, {2, 1, 0, 0}, 1, 0, 0, 6, 11, 60, 0},
        {6, 7, -1, 18, 2, {2, 1, 0, 0}, 1, 2, 1, 6, 12, 100, 0},
    };
    *sp = V[variant < 0 || variant >= TLM_VARIANTS ? 0 : variant];
}

void tlm_random(tlm_game *g, int variant, uint32_t seed)
{
    tlm_spec sp;
    tlm_variant(variant, &sp);
    memset(g, 0, sizeof *g);
    tlm_make(&g->m, &sp, seed);
    g->par = par_of(&g->m, NULL);
    tlm_begin(g);
}

#ifndef TLM_GEN                                 /* not in the levels' tool (tools/tiltmaze_gen.c): it makes them */
#include "games/tiltmaze_levels.h"

int tlm_level(tlm_game *g, int level)
{
    if (level < 0 || level >= TLM_LEVELS) return 0;
    memset(g, 0, sizeof *g);
    if (!tlm_parse(&g->m, tlm_levels[level])) return 0;
    g->par = tlm_level_par[level];
    tlm_begin(g);
    return 1;
}
#endif
