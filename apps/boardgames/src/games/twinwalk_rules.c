/*
 * Twin Walkers: the rules (twinwalk.h): the moves of both walkers, the search for the fewest moves, the maker of
 * puzzles, undo, the notation. No SDL: tested by tests/twinwalk_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/twinwalk.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

int twk_mirror_way(int dir) { return (4 - dir) & 3; }

/* ---- maps as text ------------------------------------------------------------------------------------------------ */
int twk_parse(twk_map *m, const char *text)
{
    memset(m, 0, sizeof *m);
    m->start[0] = m->start[1] = m->exit[0] = m->exit[1] = -1;
    int x = 0, y = 0, w = -1;
    for (const char *p = text;; p++) {
        if (*p == '|' || !*p) {
            if (w < 0) w = x;
            else if (x != w) return 0;
            y++;
            x = 0;
            if (!*p) break;
            continue;
        }
        if (x >= TWK_MAXW || y >= TWK_MAXH) return 0;
        int c = x + y * TWK_MAXW, t = TWK_FLOOR, g = 0;   /* laid out TWK_MAXW wide first, packed below */
        switch (*p) {
        case '.': break;
        case '#': t = TWK_WALL; break;
        case 'A': m->start[0] = c; break;
        case 'B': m->start[1] = c; break;
        case 'a': m->exit[0] = c; break;
        case 'b': m->exit[1] = c; break;
        case '%': t = TWK_CRUMBLE; break;
        case 'p': case 'q': t = TWK_PLATE; g = *p - 'p'; break;
        case 'P': case 'Q': t = TWK_DOOR; g = *p - 'P'; break;
        case '~': t = TWK_ICE; break;
        default: return 0;
        }
        m->t[c] = (uint8_t)t;
        m->grp[c] = (uint8_t)g;
        x++;
    }
    if (w < 1 || y < 1) return 0;
    m->w = w;
    m->h = y;
    uint8_t t[TWK_CELLS], g[TWK_CELLS];
    for (int yy = 0; yy < m->h; yy++)
        for (int xx = 0; xx < w; xx++) {
            t[xx + yy * w] = m->t[xx + yy * TWK_MAXW];
            g[xx + yy * w] = m->grp[xx + yy * TWK_MAXW];
        }
    memset(m->t, 0, sizeof m->t);
    memset(m->grp, 0, sizeof m->grp);
    memcpy(m->t, t, (size_t)w * m->h);
    memcpy(m->grp, g, (size_t)w * m->h);
    for (int i = 0; i < 2; i++) {
        if (m->start[i] < 0 || m->exit[i] < 0) return 0;
        m->start[i] = m->start[i] % TWK_MAXW + m->start[i] / TWK_MAXW * w;
        m->exit[i] = m->exit[i] % TWK_MAXW + m->exit[i] / TWK_MAXW * w;
    }
    if (m->start[0] == m->start[1] || m->exit[0] == m->exit[1]) return 0;
    for (int c = 0; c < w * m->h; c++)
        if (m->t[c] == TWK_CRUMBLE) {
            if (m->ncr >= TWK_MAXCR) return 0;
            m->grp[c] = (uint8_t)m->ncr++;
        }
    return 1;
}

void twk_text(const twk_map *m, char *out)
{
    char *o = out;
    for (int y = 0; y < m->h; y++) {
        if (y) *o++ = '|';
        for (int x = 0; x < m->w; x++) {
            int c = x + y * m->w;
            char ch = ".#%pP~"[m->t[c]];
            if (m->t[c] == TWK_PLATE) ch = (char)('p' + m->grp[c]);
            if (m->t[c] == TWK_DOOR) ch = (char)('P' + m->grp[c]);
            if (c == m->start[0]) ch = 'A';
            else if (c == m->start[1]) ch = 'B';
            else if (c == m->exit[0]) ch = 'a';
            else if (c == m->exit[1]) ch = 'b';
            *o++ = ch;
        }
    }
    *o = 0;
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
int twk_door_open(const twk_map *m, const twk_state *s, int cell)
{
    if (s->p[0] == cell || s->p[1] == cell) return 1;
    for (int i = 0; i < 2; i++)
        if (m->t[s->p[i]] == TWK_PLATE && m->grp[s->p[i]] == m->grp[cell]) return 1;
    return 0;
}

int twk_passable(const twk_map *m, const twk_state *s, int cell)
{
    switch (m->t[cell]) {
    case TWK_WALL: return 0;
    case TWK_CRUMBLE: return !(s->gone >> m->grp[cell] & 1);
    case TWK_DOOR: return twk_door_open(m, s, cell);
    default: return 1;
    }
}

static int step(const twk_map *m, int from, int dir)
{
    int x = from % m->w + DX[dir], y = from / m->w + DY[dir];
    return x < 0 || y < 0 || x >= m->w || y >= m->h ? -1 : x + y * m->w;
}

int twk_try(const twk_map *m, const twk_state *s, int dir, twk_state *out, twk_trace *tr)
{
    twk_state cur = *s;
    int dirs[2] = {dir, twk_mirror_way(dir)}, act[2] = {1, 1}, n[2] = {0, 0};
    if (tr) memset(tr, 0, sizeof *tr);
    for (int tick = 0; tick < TWK_LONGEST && (act[0] || act[1]); tick++) {
        int tg[2] = {-1, -1};
        for (int i = 0; i < 2; i++)
            if (act[i]) {
                tg[i] = step(m, cur.p[i], dirs[i]);
                if (tg[i] < 0 || !twk_passable(m, &cur, tg[i])) act[i] = 0;
            }
        if (act[0] && act[1] && (tg[0] == tg[1] || (tg[0] == cur.p[1] && tg[1] == cur.p[0]))) act[0] = act[1] = 0;
        for (int k = 0; k < 2; k++)             /* into the other one: it must be leaving (and not off a crumbling
                                                 * tile, which falls as it goes) */
            for (int i = 0; i < 2; i++) {
                int j = 1 - i;
                if (act[i] && tg[i] == cur.p[j] && (!act[j] || m->t[cur.p[j]] == TWK_CRUMBLE)) act[i] = 0;
            }
        if (!act[0] && !act[1]) break;
        twk_state next = cur;
        for (int i = 0; i < 2; i++)
            if (act[i]) {
                if (m->t[cur.p[i]] == TWK_CRUMBLE) next.gone |= (uint16_t)(1u << m->grp[cur.p[i]]);
                next.p[i] = (uint8_t)tg[i];
                if (tr) tr->at[i][n[i]] = (uint8_t)tg[i];
                n[i]++;
            }
        cur = next;
        for (int i = 0; i < 2; i++)
            if (act[i] && m->t[cur.p[i]] != TWK_ICE) act[i] = 0;
    }
    if (tr)
        for (int i = 0; i < 2; i++) {
            tr->n[i] = n[i];
            tr->bump[i] = !n[i];
        }
    if (!n[0] && !n[1]) return 0;
    if (out) *out = cur;
    return 1;
}

int twk_solved(const twk_map *m, const twk_state *s) { return s->p[0] == m->exit[0] && s->p[1] == m->exit[1]; }

/* ---- the search: states by their key (the places, 7 bits each, the tiles fallen above), a hash of them in the order
 * found (breadth first), each with the first move that led there -------------------------------------------------- */
#define MAX_STATES 600000

typedef struct {
    int32_t *slot;                              /* an index into key[], -1 empty */
    uint32_t *key;
    uint8_t *first;
    int cap, n, room;
} bfs;

static uint32_t key_of(const twk_state *s) { return s->p[0] | (uint32_t)s->p[1] << 7 | (uint32_t)s->gone << 14; }
static twk_state state_of(uint32_t k)
{
    twk_state s = {{(uint8_t)(k & 127), (uint8_t)(k >> 7 & 127)}, (uint16_t)(k >> 14)};
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

static unsigned hash(uint32_t k, int cap) { return (k * 2654435761u ^ k >> 15) & (unsigned)(cap - 1); }

/* the state's index, added when new (*added 1); -1 out of room */
static int bfs_add(bfs *b, uint32_t k, int first, int *added)
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
        uint32_t *nk = realloc(b->key, sizeof *nk * (size_t)room);
        if (!nk) return -1;
        b->key = nk;
        uint8_t *nf = realloc(b->first, (size_t)room);
        if (!nf) return -1;
        b->first = nf;
        b->room = room;
    }
    if (2 * (b->n + 1) > b->cap) {              /* grow the table, everything put in again */
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

/* breadth first from s: to the exits (far NULL), or everywhere: far[a * cells + b] the fewest moves to the walkers at
 * a and b (any tiles fallen; -1 never), seen[c] 1 for a tile a walker ever stood on */
static int search(const twk_map *m, const twk_state *s0, int *dir, int *states, int16_t *far, uint8_t *seen)
{
    int cells = m->w * m->h, res = -1, added;
    if (!far && twk_solved(m, s0)) {
        if (states) *states = 1;
        return 0;
    }
    bfs b;
    if (!bfs_init(&b)) { bfs_free(&b); return -2; }
    bfs_add(&b, key_of(s0), 0, &added);
    if (far) {
        for (int i = 0; i < cells * cells; i++) far[i] = -1;
        far[s0->p[0] * cells + s0->p[1]] = 0;
    }
    if (seen) {
        memset(seen, 0, (size_t)cells);
        seen[s0->p[0]] = seen[s0->p[1]] = 1;
    }
    int head = 0, depth = 0;
    while (head < b.n && res == -1) {
        int end = b.n;
        for (; head < end && res == -1; head++) {
            twk_state s = state_of(b.key[head]), t;
            for (int d = 0; d < 4; d++) {
                if (!twk_try(m, &s, d, &t, NULL)) continue;
                int i = bfs_add(&b, key_of(&t), head ? b.first[head] : d, &added);
                if (i < 0) { res = -2; break; }
                if (!added) continue;
                if (far) {
                    int16_t *f = &far[t.p[0] * cells + t.p[1]];
                    if (*f < 0) *f = (int16_t)(depth + 1);
                    if (seen) seen[t.p[0]] = seen[t.p[1]] = 1;
                } else if (twk_solved(m, &t)) {
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
    return far && res == -1 ? depth - 1 : res;
}

int twk_solve(const twk_map *m, const twk_state *s, int *dir, int *states)
{
    return search(m, s, dir, states, NULL, NULL);
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
void twk_begin(twk_game *g)
{
    g->s.p[0] = (uint8_t)g->m.start[0];
    g->s.p[1] = (uint8_t)g->m.start[1];
    g->s.gone = 0;
    g->nh = g->moves = 0;
}

int twk_move(twk_game *g, int dir)
{
    twk_state t;
    if (dir < 0 || dir > 3 || twk_solved(&g->m, &g->s) || !twk_try(&g->m, &g->s, dir, &t, NULL)) return 0;
    if (g->nh >= TWK_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + TWK_HIST / 2, sizeof g->hist[0] * (TWK_HIST / 2));
        g->nh -= TWK_HIST / 2;
    }
    g->hist[g->nh++] = g->s;
    g->s = t;
    g->moves++;
    return 1;
}

int twk_undo(twk_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    g->moves--;
    return 1;
}

int twk_hint(const twk_game *g)
{
    int d = -1;
    if (twk_solved(&g->m, &g->s)) return -1;
    return twk_solve(&g->m, &g->s, &d, NULL) > 0 ? d : -1;
}

int twk_script(twk_game *g, const char *s)
{
    while (*s) {
        char c = *s++;
        if (c == ' ' || c == ',') continue;
        if (c == '=') {
            int k = 0, any = 0;
            for (; *s >= '0' && *s <= '9'; s++, any = 1) k = k * 10 + *s - '0';
            if (!any) k = 1 << 20;
            for (int i = 0; i < k && !twk_solved(&g->m, &g->s); i++) {
                int d = twk_hint(g);
                if (d < 0 || !twk_move(g, d)) return 0;
            }
            continue;
        }
        const char *dirs = "urdl", *p = strchr(dirs, c | 32);
        if (!p || !c || !twk_move(g, (int)(p - dirs))) return 0;
    }
    return 1;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
/* the tiles a tile goes with under the symmetry (itself first): how many */
static int orbit(int w, int h, int sym, int c, int *o)
{
    int x = c % w, y = c / w, n = 0;
    o[n++] = c;
    int d = sym == 1 ? (w - 1 - x) + y * w : sym == 2 ? (w - 1 - x) + (h - 1 - y) * w : sym == 3 ? x + (h - 1 - y) * w : c;
    if (d != c) o[n++] = d;
    return n;
}

static int is_plain(const twk_map *m, int c) { return m->t[c] == TWK_FLOOR; }

/* the best pair of exits from the starts: the fewest moves to it as many as can be up to hi (the walkers' exits on
 * plain floor, off their starts, a tile apart at least); its moves, or -1. far: the search's table */
static int best_pair(const twk_map *m, const int16_t *far, int hi, uint32_t *rs, int *ea, int *eb)
{
    int cells = m->w * m->h, best = -1, nbest = 0;
    for (int a = 0; a < cells; a++) {
        if (!is_plain(m, a) || a == m->start[0] || a == m->start[1]) continue;
        for (int b = 0; b < cells; b++) {
            int d = far[a * cells + b];
            if (d < 0 || d > hi || b == a || !is_plain(m, b) || b == m->start[0] || b == m->start[1]) continue;
            if (d > best) { best = d; nbest = 0; }
            if (d == best) {                    /* drawn among the equals alike */
                nbest++;
                if (!rs || pr_below(rs, nbest) == 0) { *ea = a; *eb = b; }
            }
        }
    }
    return best;
}

static int eval(twk_map *m, int hi, uint32_t *rs, int16_t *far, uint8_t *seen, int *states)
{
    twk_state s = {{(uint8_t)m->start[0], (uint8_t)m->start[1]}, 0};
    int r = search(m, &s, NULL, states, far, seen);
    if (r < 0) return -1;
    int ea = -1, eb = -1, d = best_pair(m, far, hi, rs, &ea, &eb);
    if (d < 0) return -1;
    m->exit[0] = ea;
    m->exit[1] = eb;
    return d;
}

static int par_of(const twk_map *m)
{
    twk_state s = {{(uint8_t)m->start[0], (uint8_t)m->start[1]}, 0};
    return twk_solve(m, &s, NULL, NULL);
}

/* the special tiles of kind t matter: made plain floor, the fewest moves change */
static int matters(const twk_map *m, int kind, int par)
{
    twk_map w = *m;
    int any = 0;
    for (int c = 0; c < w.w * w.h; c++)
        if (w.t[c] == kind || (kind == TWK_DOOR && w.t[c] == TWK_PLATE)) {
            w.t[c] = TWK_FLOOR;
            any = 1;
        }
    if (!any) return 1;
    w.ncr = kind == TWK_CRUMBLE ? 0 : w.ncr;
    if (par_of(&w) == par) return 0;
    if (kind != TWK_DOOR) return 1;
    w = *m;                                     /* the doors: opening them matters too (shut for good, the fewest
                                                 * moves change) */
    for (int c = 0; c < w.w * w.h; c++)
        if (w.t[c] == TWK_DOOR) w.t[c] = TWK_WALL;
    return par_of(&w) != par;
}

/* a plain floor tile drawn at random, not one of the starts; -1 none */
static int free_tile(const twk_map *m, uint32_t *rs)
{
    int cells = m->w * m->h;
    for (int k = 0; k < 4 * cells; k++) {
        int c = pr_below(rs, cells);
        if (is_plain(m, c) && c != m->start[0] && c != m->start[1]) return c;
    }
    return -1;
}

static int walls_of(const twk_map *m)
{
    int n = 0;
    for (int c = 0; c < m->w * m->h; c++) n += m->t[c] == TWK_WALL;
    return n;
}

static int trivial_starts(const twk_map *m, int sym)   /* the mirror walker would only ever mirror the first */
{
    int o[2];
    return sym == 1 && orbit(m->w, m->h, 1, m->start[0], o) == 2 && o[1] == m->start[1];
}

static void number_crumbles(twk_map *m)
{
    m->ncr = 0;
    for (int c = 0; c < m->w * m->h; c++)
        if (m->t[c] == TWK_CRUMBLE) m->grp[c] = (uint8_t)m->ncr++;
}

/* special tiles of kind (grp) laid on n random plain tiles, in pairs under the symmetry when they fit */
static void lay(twk_map *m, int sym, int kind, int grp, int n, uint32_t *rs)
{
    for (int k = 0; k < n;) {
        int c = free_tile(m, rs), o[2];
        if (c < 0) return;
        int no = orbit(m->w, m->h, sym, c, o);
        if (no == 2 && k + 2 <= n && is_plain(m, o[1]) && o[1] != m->start[0] && o[1] != m->start[1]) {
            for (int i = 0; i < 2; i++) { m->t[o[i]] = (uint8_t)kind; m->grp[o[i]] = (uint8_t)grp; }
            k += 2;
        } else {
            m->t[c] = (uint8_t)kind;
            m->grp[c] = (uint8_t)grp;
            k++;
        }
    }
}

int twk_make(twk_map *out, const twk_spec *sp, uint32_t seed)
{
    uint32_t rs = pr_seed(seed);
    int w = sp->w, h = sp->h, cells = w * h, found = 0, best_score = -1, loose_score = -1;
    int16_t *far = malloc(sizeof *far * (size_t)cells * cells);
    uint8_t seen[TWK_CELLS];
    twk_map best, loose;
    memset(&best, 0, sizeof best);
    memset(&loose, 0, sizeof loose);
    if (!far) return 0;
    for (int attempt = 0; attempt < (sp->tries ? sp->tries : 60) && !found; attempt++) {
        int sym = sp->sym >= 0 ? sp->sym : 1 + pr_below(&rs, 3);
        twk_map m;
        memset(&m, 0, sizeof m);
        m.w = w;
        m.h = h;
        for (int c = 0; c < cells; c++) {       /* the walls, an orbit at a time */
            int o[2], no = orbit(w, h, sym, c, o);
            if (o[no - 1] < c) continue;
            if (pr_below(&rs, 100) < sp->walls)
                for (int i = 0; i < no; i++) m.t[o[i]] = TWK_WALL;
        }
        m.start[0] = m.start[1] = -1;
        m.start[0] = free_tile(&m, &rs);
        m.start[1] = free_tile(&m, &rs);
        if (m.start[0] < 0 || m.start[1] < 0 || m.start[0] == m.start[1] || trivial_starts(&m, sym)) continue;
        lay(&m, sym, TWK_CRUMBLE, 0, sp->crumble, &rs);
        lay(&m, sym, TWK_ICE, 0, sp->ice, &rs);
        for (int g = 0; g < sp->plates; g++) {
            lay(&m, 0, TWK_PLATE, g, 1, &rs);
            lay(&m, sym, TWK_DOOR, g, 1 + pr_below(&rs, 2), &rs);
        }
        number_crumbles(&m);
        int states, score = eval(&m, sp->hi, &rs, far, seen, &states);
        for (int it = 0; it < sp->iters; it++) {
            twk_map t = m;
            int r = pr_below(&rs, 100), c = pr_below(&rs, cells);
            if (r < 70) {                       /* a wall orbit put or taken away */
                int o[2], no = orbit(w, h, sym, c, o), ok = 1;
                for (int i = 0; i < no; i++)
                    if (o[i] == t.start[0] || o[i] == t.start[1] || (t.t[o[i]] != TWK_WALL && t.t[o[i]] != TWK_FLOOR))
                        ok = 0;
                if (!ok) continue;
                int to = t.t[c] == TWK_WALL ? TWK_FLOOR : TWK_WALL;
                if (to == TWK_WALL && 100 * (walls_of(&t) + no) > 45 * cells) continue;   /* airy: not a wall mass */
                for (int i = 0; i < no; i++) t.t[o[i]] = (uint8_t)to;
            } else if (r < 85) {                /* a special tile moved */
                if (t.t[c] == TWK_FLOOR || t.t[c] == TWK_WALL) continue;
                int d = free_tile(&t, &rs);
                if (d < 0) continue;
                t.t[d] = t.t[c];
                t.grp[d] = t.grp[c];
                t.t[c] = TWK_FLOOR;
                t.grp[c] = 0;
            } else {                            /* a start moved */
                int i = pr_below(&rs, 2), d = free_tile(&t, &rs);
                if (d < 0) continue;
                t.start[i] = d;
                if (trivial_starts(&t, sym)) continue;
            }
            number_crumbles(&t);
            int st, sc = eval(&t, sp->hi, &rs, far, seen, &st);
            if (sc > score || (sc == score && sc >= 0 && st <= states)) {
                m = t;
                score = sc;
                states = st;
            }
        }
        if (score < 0) continue;
        score = eval(&m, sp->hi, &rs, far, seen, &states);   /* the exits of the map kept */
        int par = par_of(&m);
        if (par != score || par <= 0) continue;
        if (!matters(&m, TWK_CRUMBLE, par) || !matters(&m, TWK_DOOR, par) || !matters(&m, TWK_ICE, par)) {
            if (score > loose_score) {          /* kept in case nothing better comes */
                loose = m;
                loose_score = score;
            }
            continue;
        }
        if (score > best_score) {
            best = m;
            best_score = score;
        }
        if (score >= sp->lo) found = 1;
    }
    free(far);
    if (best_score < 0 && loose_score > 0) best = loose, best_score = loose_score;
    if (best_score < 0) {                       /* nothing kept at all: an open floor, each walker in a corner
                                                 * of its own */
        memset(&best, 0, sizeof best);
        best.w = w;
        best.h = h;
        best.start[0] = 0;
        best.start[1] = cells - 1;
        best.exit[0] = w - 1;
        best.exit[1] = cells - w;
    }
    *out = best;
    return found;
}

void twk_variant(int variant, twk_spec *sp)
{
    static const twk_spec V[TWK_VARIANTS] = {
        {6, 6, -1, 24, 0, 0, 0, 7, 10, 120, 0},
        {6, 7, -1, 22, 0, 1, 3, 9, 13, 60, 12},
        {7, 7, -1, 20, 1, 1, 4, 11, 16, 60, 12},
    };
    *sp = V[variant < 0 || variant >= TWK_VARIANTS ? 0 : variant];
}

void twk_random(twk_game *g, int variant, uint32_t seed)
{
    twk_spec sp;
    twk_variant(variant, &sp);
    memset(g, 0, sizeof *g);
    twk_make(&g->m, &sp, seed);
    g->par = par_of(&g->m);
    twk_begin(g);
}

#ifndef TWK_GEN                                 /* not in the levels' tool (tools/twinwalk_gen.c): it makes them */
#include "games/twinwalk_levels.h"

int twk_level(twk_game *g, int level)
{
    if (level < 0 || level >= TWK_LEVELS) return 0;
    memset(g, 0, sizeof *g);
    if (!twk_parse(&g->m, twk_levels[level])) return 0;
    g->par = twk_level_par[level];
    twk_begin(g);
    return 1;
}
#endif
