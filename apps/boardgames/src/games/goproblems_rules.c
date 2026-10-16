/*
 * Go Problems' rules (goproblems.h): the corner's own small board and its solver, the problems written out and read
 * back, and a problem played with go's rules (go_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/goproblems.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GPZ_NO_LEVELS
#include "games/goproblems_levels.h"
#endif

/* ---- the solver's board ------------------------------------------------------------------------------------------ */
static uint64_t Z[GPZ_CELLS][3], Z_SIDE, Z_PASS;
static int z_made;

static uint64_t mix(uint64_t *s)
{
    uint64_t z = (*s += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static void zobrist(void)
{
    if (z_made) return;
    uint64_t s = 0x60b0a7c3u;
    for (int c = 0; c < GPZ_CELLS; c++)
        for (int k = 0; k < 3; k++) Z[c][k] = mix(&s);
    Z_SIDE = mix(&s);
    Z_PASS = mix(&s);
    z_made = 1;
}

static const int DIR[4] = {1, -1, GPZ_S, -GPZ_S};

static int inside(int c) { return c >= 0 && c < GPZ_CELLS; }

/* the block on c into st (its stones, n), its liberties' count (empty or outside cells, each once); free: it touches
 * a cell outside the problem */
static uint32_t seen[GPZ_CELLS], stamp;        /* a cell seen by the fill whose stamp it holds */

static int block(const gpz_board *b, int c, uint8_t *st, int *n, int *free)
{
    if (!++stamp) { memset(seen, 0, sizeof seen); stamp = 1; }
    int col = b->c[c], k = 0, libs = 0, fr = 0;
    st[k++] = (uint8_t)c;
    seen[c] = stamp;
    for (int i = 0; i < k; i++)
        for (int d = 0; d < 4; d++) {
            int q = st[i] + DIR[d];
            if (!inside(q) || seen[q] == stamp) continue;
            int v = b->c[q];
            if (v == col) { seen[q] = stamp; st[k++] = (uint8_t)q; }
            else if (v == GPZ_EMPTY || v == GPZ_FREE) { seen[q] = stamp; libs++; fr |= v == GPZ_FREE; }
        }
    *n = k;
    if (free) *free = fr;
    return libs;
}

int gpz_play(gpz_board *b, int cell, int colour)
{
    zobrist();
    if (!inside(cell) || b->c[cell] != GPZ_EMPTY) return 0;
    uint8_t st[GPZ_CELLS];
    int n, took = 0, foe = 3 - colour;
    b->c[cell] = (uint8_t)colour;
    b->hash ^= Z[cell][colour];
    for (int d = 0; d < 4; d++) {
        int q = cell + DIR[d];
        if (!inside(q) || b->c[q] != foe || block(b, q, st, &n, NULL)) continue;
        for (int i = 0; i < n; i++) {
            b->c[st[i]] = GPZ_EMPTY;
            b->hash ^= Z[st[i]][foe];
        }
        took += n;
    }
    if (!block(b, cell, st, &n, NULL)) return 0;   /* suicide (b is then spoilt: play on a copy) */
    return 1 + took;
}

/* Benson's test: the blocks of the colour that can never be taken, however the other plays (the outside cells count
 * as empty: a region holding one is never an eye) */
int gpz_benson(const gpz_board *b, int cell)
{
    int col = b->c[cell];
    if (col != GPZ_BLACK && col != GPZ_WHITE) return 0;
    static int16_t bid[GPZ_CELLS], rid[GPZ_CELLS];
    uint8_t st[GPZ_CELLS];
    int nb = 0, nr = 0;
    memset(bid, -1, sizeof bid);
    memset(rid, -1, sizeof rid);
    for (int c = 0; c < GPZ_CELLS; c++) {       /* the blocks */
        if (b->c[c] != col || bid[c] >= 0) continue;
        int n;
        block(b, c, st, &n, NULL);
        if (nb >= 64) return 0;
        for (int i = 0; i < n; i++) bid[st[i]] = (int16_t)nb;
        nb++;
    }
    enum { MAXR = 96 };
    uint64_t touch[MAXR], healthy[MAXR];       /* each region's blocks: round it; next to all its empty cells */
    for (int c = 0; c < GPZ_CELLS; c++) {       /* the regions: what is not the colour, joined */
        int v = b->c[c];
        if (v == col || v == GPZ_EDGE || rid[c] >= 0) continue;
        if (nr >= MAXR) return 0;
        int k = 0, stack[GPZ_CELLS];
        uint64_t t = 0, h = ~0ull;
        stack[k++] = c;
        rid[c] = (int16_t)nr;
        while (k) {
            int p = stack[--k], w = b->c[p];
            uint64_t next = 0;
            for (int d = 0; d < 4; d++) {
                int q = p + DIR[d];
                if (!inside(q)) continue;
                int u = b->c[q];
                if (u == col) next |= 1ull << bid[q];
                else if (u != GPZ_EDGE && rid[q] < 0) { rid[q] = (int16_t)nr; stack[k++] = q; }
            }
            t |= next;
            if (w == GPZ_EMPTY || w == GPZ_FREE) h &= next;
        }
        touch[nr] = t;
        healthy[nr] = h & t;
        nr++;
    }
    uint64_t alive = nb >= 64 ? ~0ull : (1ull << nb) - 1;
    for (int changed = 1; changed;) {
        changed = 0;
        for (int i = 0; i < nb; i++) {
            if (!(alive >> i & 1)) continue;
            int eyes = 0;
            for (int r = 0; r < nr && eyes < 2; r++)
                if ((healthy[r] >> i & 1) && !(touch[r] & ~alive)) eyes++;
            if (eyes < 2) { alive &= ~(1ull << i); changed = 1; }
        }
    }
    return (int)(alive >> bid[cell] & 1);
}

int gpz_status(const gpz_problem *pz, const gpz_board *b)
{
    if (b->c[pz->key] != pz->def) return GPZ_AWIN;
    uint8_t st[GPZ_CELLS];
    int n, free;
    int libs = block(b, pz->key, st, &n, &free);
    if (free || (libs >= 2 && gpz_benson(b, pz->key))) return GPZ_DWIN;   /* two eyes: two liberties at least */
    return 0;
}

/* ---- the search ------------------------------------------------------------------------------------------------ */
#define TT_BITS 17
typedef struct { uint64_t key; uint8_t val, depth, move, used; } tt_entry;
static tt_entry tt[1u << TT_BITS];
static uint64_t tt_owner;                       /* the problem the table's positions belong to */

void gpz_new_search(void)
{
    memset(tt, 0, sizeof tt);
    tt_owner = 0;
}

typedef struct {
    const gpz_problem *pz;
    uint64_t path[256];
    int npath;
    long nodes, limit;
} ctx;

#define NODE_LIMIT 3000000L                     /* a search's most positions (then: not known) */



static int seen_before(const ctx *s, uint64_t h)
{
    for (int i = 0; i < s->npath; i++)
        if (s->path[i] == h) return 1;
    return 0;
}

/* the moves to try, the best first: the table's, then the key group's liberties and their neighbours, the cells
 * next to stones in atari, the rest; a pass last */
static int order(const gpz_problem *pz, const gpz_board *b, int first, int *mv)
{
    uint8_t st[GPZ_CELLS], near[GPZ_CELLS] = {0}, libs[GPZ_CELLS] = {0};
    int n, score[GPZ_CELLS];
    block(b, pz->key, st, &n, NULL);            /* the key group's liberties */
    for (int j = 0; j < n; j++)
        for (int d = 0; d < 4; d++) {
            int q = st[j] + DIR[d];
            if (inside(q) && b->c[q] == GPZ_EMPTY) near[q] = 2;
        }
    for (int c = 0; c < GPZ_CELLS; c++) {       /* each stone: its block's liberties */
        int v = b->c[c];
        if ((v != GPZ_BLACK && v != GPZ_WHITE) || libs[c]) continue;
        int l = block(b, c, st, &n, NULL);
        for (int i = 0; i < n; i++) libs[st[i]] = (uint8_t)(l > 3 ? 3 : l);
    }
    int k = 0;
    for (int c = 0; c < GPZ_CELLS && k < GPZ_MAXMOVES - 1; c++) {
        if (b->c[c] != GPZ_EMPTY) continue;
        int sc = near[c] * 8;
        for (int d = 0; d < 4; d++) {
            int q = c + DIR[d];
            if (!inside(q)) continue;
            int v = b->c[q];
            if (v == GPZ_EMPTY) sc += 1;
            else if (v == GPZ_BLACK || v == GPZ_WHITE) {
                if (libs[q] == 1) sc += 12;
                else if (libs[q] == 2) sc += 3;
            }
        }
        if (c == first) sc = 1000;
        score[c] = sc;
        int j = k++;
        while (j > 0 && score[mv[j - 1]] < sc) { mv[j] = mv[j - 1]; j--; }
        mv[j] = c;
    }
    mv[k++] = GPZ_PASS;
    return k;
}

static void store(tt_entry *e, uint64_t key, int val, int depth, int move)
{
    if (e->key != key && e->used && e->val != GPZ_UNK && val == GPZ_UNK) return;   /* keep a proof */
    e->key = key;
    e->val = (uint8_t)val;
    e->depth = (uint8_t)depth;
    e->move = (uint8_t)move;
    e->used = 1;
}

static int search(ctx *s, const gpz_board *b, int side, int depth, int passes, int *best)
{
    s->nodes++;
    uint64_t key = b->hash ^ (side == GPZ_WHITE ? Z_SIDE : 0) ^ (passes ? Z_PASS : 0);
    tt_entry *e = &tt[key & ((1u << TT_BITS) - 1)];
    int first = -1;
    if (e->used && e->key == key) {
        if (e->val != GPZ_UNK) { if (best) *best = e->move; return e->val; }
        if (e->depth >= depth) { if (best) *best = e->move; return GPZ_UNK; }
        first = e->move;
    }
    if (depth <= 0 || s->nodes > s->limit) return GPZ_UNK;
    const gpz_problem *pz = s->pz;
    int att = side == pz->att, win = att ? GPZ_AWIN : GPZ_DWIN, lose = att ? GPZ_DWIN : GPZ_AWIN;
    int mv[GPZ_MAXMOVES], n = order(pz, b, first, mv), unk = 0, bm = -1;
    for (int i = 0; i < n; i++) {
        int m = mv[i], v;
        if (m == GPZ_PASS) v = passes ? GPZ_DWIN : search(s, b, 3 - side, depth - 1, 1, NULL);
        else {
            gpz_board nb = *b;
            if (!gpz_play(&nb, m, side) || seen_before(s, nb.hash)) continue;
            v = gpz_status(pz, &nb);
            if (!v) {
                s->path[s->npath++] = nb.hash;
                v = search(s, &nb, 3 - side, depth - 1, 0, NULL);
                s->npath--;
            }
        }
        if (v == win) {
            store(e, key, win, depth, m);
            if (best) *best = m;
            return win;
        }
        if (v == GPZ_UNK && !unk) { unk = 1; bm = m; }
        if (bm < 0) bm = m;
    }
    if (bm < 0) bm = GPZ_PASS;
    int r = unk ? GPZ_UNK : lose;
    if (s->nodes <= s->limit || r != GPZ_UNK) store(e, key, r, depth, bm);
    if (best) *best = bm;
    return r;
}

static uint64_t owner_of(const gpz_problem *pz)
{
    return pz->start.hash ^ (uint64_t)pz->key * 0x9e3779b97f4a7c15ull ^ (uint64_t)(pz->kind + 1) << 56 ^
           (uint64_t)(pz->w * 16 + pz->h) << 48;
}

static void own(const gpz_problem *pz)
{
    zobrist();
    uint64_t o = owner_of(pz);
    if (o != tt_owner) {
        memset(tt, 0, sizeof tt);
        tt_owner = o;
    }
}

static void set_path(ctx *s, const uint64_t *path, int npath)
{
    if (npath > 200) { path += npath - 200; npath = 200; }
    memcpy(s->path, path, sizeof *path * (size_t)npath);
    s->npath = npath;
}

int gpz_prove(const gpz_problem *pz, const gpz_board *b, int side, int passes, const uint64_t *path, int npath,
              int maxdepth, int *depth, int *move)
{
    own(pz);
    ctx s = {.pz = pz, .limit = NODE_LIMIT};
    set_path(&s, path, npath);
    int v = gpz_status(pz, b);
    if (v) { if (depth) *depth = 0; if (move) *move = GPZ_PASS; return v; }
    for (int d = 1; d <= maxdepth; d++) {
        int m = GPZ_PASS;
        v = search(&s, b, side, d, passes, &m);
        if (v != GPZ_UNK || s.nodes > s.limit) {
            if (depth) *depth = d;
            if (move) *move = m;
            return v;
        }
    }
    if (depth) *depth = maxdepth;
    return GPZ_UNK;
}

int gpz_analyse(const gpz_problem *pz, const gpz_board *b, int side, int passes, const uint64_t *path, int npath,
                int maxdepth, gpz_eval *out, long *nodes)
{
    own(pz);
    ctx s = {.pz = pz, .limit = NODE_LIMIT};
    set_path(&s, path, npath);
    int mv[GPZ_MAXMOVES], n = order(pz, b, -1, mv), k = 0;
    for (int i = 0; i < n; i++) {
        int m = mv[i], v = 0, d = 0;
        gpz_board nb = *b;
        if (m == GPZ_PASS) {
            if (passes) { v = GPZ_DWIN; d = 1; }
        } else {
            if (!gpz_play(&nb, m, side) || seen_before(&s, nb.hash)) continue;
            v = gpz_status(pz, &nb);
            d = 1;
        }
        if (!v) {
            if (m != GPZ_PASS) s.path[s.npath++] = nb.hash;
            for (d = 1; d < maxdepth; d++) {
                v = search(&s, &nb, 3 - side, d, m == GPZ_PASS, NULL);
                if (v != GPZ_UNK || s.nodes > s.limit) break;
            }
            d++;
            if (m != GPZ_PASS) s.npath--;
        }
        out[k++] = (gpz_eval){m, v, d};
    }
    if (nodes) *nodes = s.nodes;
    return k;
}

/* ---- the problems ------------------------------------------------------------------------------------------------ */
int gpz_pt(int cell) { return GPZ_X(cell) + GPZ_Y(cell) * 19; }

int gpz_cell(int pt)
{
    if (pt < 0 || pt >= 19 * 19) return 0;
    int x = pt % 19, y = pt / 19;
    return x < GPZ_MAXW && y < GPZ_MAXW ? GPZ_CELL(x, y) : 0;
}

static void board_frame(gpz_board *b, int w, int h)
{
    for (int c = 0; c < GPZ_CELLS; c++) {
        int x = GPZ_X(c), y = GPZ_Y(c);
        b->c[c] = x < 0 || y < 0 ? GPZ_EDGE : x >= w || y >= h ? GPZ_FREE : GPZ_EMPTY;
    }
    b->hash = 0;
}

static const char *read_xy(const char *s, int *cell)
{
    int x = 0, y = 0;
    if (*s < '0' || *s > '9') return NULL;
    while (*s >= '0' && *s <= '9') x = x * 10 + *s++ - '0';
    if (*s++ != ',') return NULL;
    if (*s < '0' || *s > '9') return NULL;
    while (*s >= '0' && *s <= '9') y = y * 10 + *s++ - '0';
    *cell = GPZ_CELL(x, y);
    return x < GPZ_MAXW && y < GPZ_MAXW ? s : NULL;
}

int gpz_parse(gpz_problem *pz, const char *s)
{
    zobrist();
    memset(pz, 0, sizeof *pz);
    if (*s != 'K' && *s != 'L') return 0;
    pz->kind = *s == 'K' ? GPZ_KILL : GPZ_LIVE;
    pz->att = pz->kind == GPZ_KILL ? GPZ_BLACK : GPZ_WHITE;
    pz->def = 3 - pz->att;
    char *e;
    pz->w = (int)strtol(s + 1, &e, 10);
    pz->h = (int)strtol(e, &e, 10);
    pz->depth = (int)strtol(e, &e, 10);
    if (pz->w < 2 || pz->h < 2 || pz->w > GPZ_MAXW || pz->h > GPZ_MAXW || pz->depth < 1 || *e != ' ') return 0;
    s = read_xy(e + 1, &pz->key);
    if (!s || *s != ' ') return 0;
    s = read_xy(s + 1, &pz->first);
    if (!s || *s != ' ') return 0;
    s++;
    board_frame(&pz->start, pz->w, pz->h);
    for (int y = 0; y < pz->h; y++) {
        for (int x = 0; x < pz->w; x++, s++) {
            int c = GPZ_CELL(x, y), v;
            switch (*s) {
            case '.': v = GPZ_EMPTY; break;
            case ',': v = GPZ_FREE; break;
            case 'X': v = GPZ_BLACK; break;
            case 'O': v = GPZ_WHITE; break;
            default: return 0;
            }
            pz->start.c[c] = (uint8_t)v;
            if (v == GPZ_BLACK || v == GPZ_WHITE) pz->start.hash ^= Z[c][v];
        }
        if (y < pz->h - 1 && *s++ != '/') return 0;
    }
    while (*s == ' ' && pz->nline < GPZ_LINE) {   /* the main line */
        int c = GPZ_PASS;
        s++;
        if (*s == 'p') s++;
        else if (!(s = read_xy(s, &c))) return 0;
        pz->line[pz->nline++] = (uint8_t)c;
    }
    if (*s) return 0;
    uint8_t st[GPZ_CELLS];
    int n;
    for (int c = 0; c < GPZ_CELLS; c++) {       /* every group has a liberty */
        int v = pz->start.c[c];
        if ((v == GPZ_BLACK || v == GPZ_WHITE) && !block(&pz->start, c, st, &n, NULL)) return 0;
    }
    if (pz->start.c[pz->key] != pz->def) return 0;
    if (pz->first != GPZ_PASS && pz->start.c[pz->first] != GPZ_EMPTY) return 0;
    return 1;
}

void gpz_write(const gpz_problem *pz, char *out, int size)
{
    int k = snprintf(out, (size_t)size, "%c %d %d %d %d,%d %d,%d ", pz->kind == GPZ_KILL ? 'K' : 'L', pz->w, pz->h,
                     pz->depth, GPZ_X(pz->key), GPZ_Y(pz->key), GPZ_X(pz->first), GPZ_Y(pz->first));
    for (int y = 0; y < pz->h && k < size - 2; y++) {
        for (int x = 0; x < pz->w && k < size - 2; x++) out[k++] = ".XO?,"[pz->start.c[GPZ_CELL(x, y)]];
        if (y < pz->h - 1 && k < size - 2) out[k++] = '/';
    }
    for (int i = 0; i < pz->nline && k < size - 8; i++)
        k += pz->line[i] == GPZ_PASS ? snprintf(out + k, (size_t)(size - k), " p")
                                     : snprintf(out + k, (size_t)(size - k), " %d,%d", GPZ_X(pz->line[i]),
                                                GPZ_Y(pz->line[i]));
    out[k < size ? k : size - 1] = 0;
}

#ifndef GPZ_NO_LEVELS
int gpz_level_load(gpz_problem *pz, int level)
{
    if (level < 0 || level >= GPZ_LEVELS) return 0;
    return gpz_parse(pz, gpz_level_text[level]);
}

int gpz_level_chapter(int level) { return level < 0 ? 0 : level >= GPZ_LEVELS ? GPZ_CHAPTERS - 1 : level / 25; }
#else
int gpz_level_load(gpz_problem *pz, int level) { (void)pz; (void)level; return 0; }
int gpz_level_chapter(int level) { (void)level; return 0; }
#endif

/* ---- a problem played -------------------------------------------------------------------------------------------- */
/* the solver's board for the stones now (the outside cells of the problem kept) */
static void local(const gpz_game *g, int k, gpz_board *b)
{
    zobrist();
    *b = g->pz.start;
    b->hash = 0;
    const gopos *p = &g->g.pos[k];
    for (int y = 0; y < g->pz.h; y++)
        for (int x = 0; x < g->pz.w; x++) {
            int c = GPZ_CELL(x, y);
            if (b->c[c] == GPZ_FREE) continue;
            int v = p->pt[x + 19 * y];
            b->c[c] = (uint8_t)(v == GO_EBONY ? GPZ_BLACK : v == GO_IVORY ? GPZ_WHITE : GPZ_EMPTY);
            if (b->c[c] != GPZ_EMPTY) b->hash ^= Z[c][b->c[c]];
        }
}

static int path_of(const gpz_game *g, uint64_t *path)
{
    int n = 0;
    for (int k = 0; k <= g->g.nmoves && n < 256; k++) {
        if (k && g->g.move[k - 1] == GO_PASS) continue;
        gpz_board b;
        local(g, k, &b);
        path[n++] = b.hash;
    }
    return n;
}

static int passes_now(const gpz_game *g) { return go_last(&g->g) == GO_PASS; }

static int side_now(const gpz_game *g) { return go_now(&g->g)->side == 0 ? GPZ_BLACK : GPZ_WHITE; }

void gpz_begin(gpz_game *g, const gpz_problem *pz, int level)
{
    gogame *G = &g->g;
    memset(g, 0, sizeof *g);
    g->pz = *pz;
    g->level = level;
    g->hint_ply = -1;
    go_new(G, 19);
    int bl[GPZ_CELLS], wh[GPZ_CELLS], nb = 0, nw = 0;   /* the stones set down in turn (passes between): never a
                                                         * capture, every group of the problem having a liberty */
    for (int c = 0; c < GPZ_CELLS; c++) {
        if (pz->start.c[c] == GPZ_BLACK) bl[nb++] = gpz_pt(c);
        if (pz->start.c[c] == GPZ_WHITE) wh[nw++] = gpz_pt(c);
    }
    for (int i = 0; i < nb || i < nw; i++) {
        if (i < nb) go_play(G, bl[i]); else go_pass(G);
        if (i < nw) go_play(G, wh[i]); else go_pass(G);
    }
    G->pos[0] = *go_now(G);
    G->pos[0].side = 0;
    G->pos[0].caps[0] = G->pos[0].caps[1] = 0;
    G->nmoves = 0;
    G->phase = GO_PLAY;
    gpz_new_search();
}

int gpz_level(gpz_game *g, int level)
{
    gpz_problem pz;
    if (!gpz_level_load(&pz, level)) return 0;
    gpz_begin(g, &pz, level);
    return 1;
}

int gpz_to_move(const gpz_game *g) { return !g->solved && !g->failed && side_now(g) == GPZ_BLACK; }

/* a move (a cell or a pass) on the game; 0: not allowed */
static int put(gpz_game *g, int cell)
{
    if (cell == GPZ_PASS) {
        if (g->g.phase != GO_PLAY || g->g.nmoves >= GO_HIST) return 0;
        go_pass(&g->g);
        g->g.phase = GO_PLAY;                   /* two passes: the end (gpz's), never go's counting */
        return 1;
    }
    if (g->pz.start.c[cell] == GPZ_FREE || GPZ_X(cell) < 0 || GPZ_Y(cell) < 0 || GPZ_X(cell) >= g->pz.w ||
        GPZ_Y(cell) >= g->pz.h)
        return 0;
    return go_play(&g->g, gpz_pt(cell)) >= 0;
}

/* the end after the last move: the stones, or two passes in a row */
static int ended(const gpz_game *g)
{
    gpz_board b;
    local(g, g->g.nmoves, &b);
    int v = gpz_status(&g->pz, &b);
    if (v) return v;
    int n = g->g.nmoves;
    return n >= 2 && g->g.move[n - 1] == GO_PASS && g->g.move[n - 2] == GO_PASS ? GPZ_DWIN : 0;
}

static int player_wins(const gpz_game *g, int v) { return v == (g->pz.kind == GPZ_KILL ? GPZ_AWIN : GPZ_DWIN); }

static int budget(const gpz_game *g)
{
    int left = g->pz.depth + GPZ_SLACK - g->g.nmoves;
    return left < 1 ? 1 : left;
}

/* the device's reply (White to move): its best. right: 1 if every reply loses (the player's move was right) */
static int reply(gpz_game *g, int *right)
{
    gpz_board b;
    uint64_t path[256];
    gpz_eval ev[GPZ_MAXMOVES];
    local(g, g->g.nmoves, &b);
    int np = path_of(g, path), n = gpz_analyse(&g->pz, &b, GPZ_WHITE, passes_now(g), path, np, budget(g), ev, NULL);
    int mine = g->pz.kind == GPZ_KILL ? GPZ_DWIN : GPZ_AWIN;   /* the device's win */
    int best = -1;
    for (int i = 0; i < n; i++)                 /* a refutation: the quickest win */
        if (ev[i].val == mine && (best < 0 || ev[i].depth < ev[best].depth)) best = i;
    if (best >= 0) { *right = 0; return ev[best].move; }
    for (int i = 0; i < n; i++)                 /* not known: as good as a refutation */
        if (ev[i].val == GPZ_UNK) { *right = 0; return ev[i].move; }
    *right = 1;
    for (int i = 0; i < n; i++)                 /* the longest resistance (a pass only when nothing else is) */
        if (best < 0 || ev[i].depth > ev[best].depth ||
            (ev[i].depth == ev[best].depth && ev[best].move == GPZ_PASS))
            best = i;
    return best >= 0 ? ev[best].move : GPZ_PASS;
}

/* 1: the moves so far are the main line's */
static int on_line(const gpz_game *g)
{
    if (g->g.nmoves > g->pz.nline) return 0;
    for (int i = 0; i < g->g.nmoves; i++) {
        int m = g->g.move[i];
        if (g->pz.line[i] != (m == GO_PASS ? GPZ_PASS : gpz_cell(m))) return 0;
    }
    return 1;
}

int gpz_try(gpz_game *g, int cell)
{
    if (!gpz_to_move(g)) return GPZ_ILLEGAL;
    int follow = on_line(g) && g->g.nmoves < g->pz.nline && g->pz.line[g->g.nmoves] == cell;
    if (!put(g, cell)) return GPZ_ILLEGAL;
    int v = ended(g);
    if (v) {
        if (player_wins(g, v)) { g->solved = 1; return GPZ_SOLVED; }
        g->failed = 1;                          /* two passes in a kill: the group stays */
        g->mistakes++;
        return GPZ_WRONG;
    }
    int right = 1, r;
    if (follow && g->g.nmoves < g->pz.nline) r = g->pz.line[g->g.nmoves];   /* the main line: its answer */
    else {
        r = reply(g, &right);
        if (g->g.nmoves == 1 && cell != g->pz.first) right = 0;   /* the first move: proven, only the key */
    }
    if (!put(g, r)) { r = GPZ_PASS; put(g, r); }
    if (!right) {
        g->failed = 1;
        g->mistakes++;
        return GPZ_WRONG;
    }
    v = ended(g);
    if ((v && player_wins(g, v)) || (r == GPZ_PASS && g->pz.kind == GPZ_LIVE)) {
        g->solved = 1;                          /* the attacker passes: the black group lives */
        return GPZ_SOLVED;
    }
    return GPZ_RIGHT;
}

int gpz_hint(gpz_game *g)
{
    if (!gpz_to_move(g)) return -1;
    int move;
    if (on_line(g) && g->g.nmoves < g->pz.nline) move = g->pz.line[g->g.nmoves];   /* the main line's */
    else {                                      /* off it: the quickest win */
        gpz_board b;
        uint64_t path[256];
        gpz_eval ev[GPZ_MAXMOVES];
        local(g, g->g.nmoves, &b);
        int np = path_of(g, path), n = gpz_analyse(&g->pz, &b, GPZ_BLACK, passes_now(g), path, np, budget(g), ev,
                                                   NULL);
        int want = g->pz.kind == GPZ_KILL ? GPZ_AWIN : GPZ_DWIN, best = -1;
        for (int i = 0; i < n; i++)
            if (ev[i].val == want && (best < 0 || ev[i].depth < ev[best].depth)) best = i;
        if (best < 0) return -1;
        move = ev[best].move;
    }
    if (g->hint_ply != g->g.nmoves) { g->hints++; g->hint_ply = g->g.nmoves; }
    return g->hint = move;
}

int gpz_undo(gpz_game *g)
{
    if (!g->g.nmoves) return 0;
    go_undo(&g->g);
    while (g->g.nmoves && side_now(g) != GPZ_BLACK) go_undo(&g->g);
    g->g.phase = GO_PLAY;
    g->solved = g->failed = 0;
    g->hint_ply = -1;
    return 1;
}

int gpz_stars(const gpz_game *g)
{
    int s = g->hints + g->mistakes;
    return !s ? 3 : s <= 2 ? 2 : 1;            /* the shell's ui_logic_stars, for the tests */
}

int gpz_cell_at(const gpz_game *g, int cell)
{
    int x = GPZ_X(cell), y = GPZ_Y(cell);
    if (x < 0 || y < 0 || x >= g->pz.w || y >= g->pz.h) return GPZ_EDGE;
    if (g->pz.start.c[cell] == GPZ_FREE) return GPZ_FREE;
    int v = go_now(&g->g)->pt[x + 19 * y];
    return v == GO_EBONY ? GPZ_BLACK : v == GO_IVORY ? GPZ_WHITE : GPZ_EMPTY;
}

int gpz_key_group(const gpz_game *g, uint8_t mark[GPZ_CELLS])
{
    gpz_board b;
    local(g, g->g.nmoves, &b);
    memset(mark, 0, GPZ_CELLS);
    if (b.c[g->pz.key] != g->pz.def) return 0;
    uint8_t st[GPZ_CELLS];
    int n;
    block(&b, g->pz.key, st, &n, NULL);
    for (int i = 0; i < n; i++) mark[st[i]] = 1;
    return n;
}

int gpz_script(gpz_game *g, const char *s)
{
    if (*s == '#') {
        int n = atoi(s + 1);
        if (!gpz_level(g, n)) return 0;
        while (*s && *s != ' ') s++;
    }
    for (;;) {
        while (*s == ' ') s++;
        if (!*s) return 1;
        int len, p = go_point(19, s, &len);
        if (p == GO_NONE || (s[len] && s[len] != ' ')) return 0;
        int r = gpz_try(g, p == GO_PASS ? GPZ_PASS : gpz_cell(p));
        if (r == GPZ_ILLEGAL) return 0;
        if (r == GPZ_WRONG) gpz_undo(g);
        s += len;
    }
}
