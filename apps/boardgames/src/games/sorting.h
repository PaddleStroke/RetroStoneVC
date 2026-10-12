/*
 * The sorting puzzles' rules (Water Sort, Ball Sort): one engine, no SDL. Tubes hold up to four layers of colour (a
 * layer of liquid, or a ball). A move takes from the top of one tube to another whose top is the same colour, or that
 * is empty: Water Sort pours the top layer and every layer of its colour under it, as far as the target holds them;
 * Ball Sort moves one ball (`one`). Solved: each colour gathered in one tube (full of it), the others empty.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="ballsort" builds
 * ballsort.c and ballsort_*.c), so the engine lives in a header both games' rules include.
 *
 * The puzzles: n colours (4, 7, 10 or 12), four layers of each shuffled at random into n tubes, two tubes empty; each
 * one is solved by the search below before it is given (so every puzzle can be solved), and of a few drawn the one
 * whose shortest way is the longest is kept (the puzzles that need the most moves).
 * The helper: once a puzzle, an extra empty tube may be set on the rack (an action, which undo takes back).
 * The par: the shortest way's length when the search finished (always for the levels, made offline by
 * tools/watersort_gen.c and tools/ballsort_gen.c with room to search); for the largest random Ball Sort puzzles, when
 * the shortest way takes too long to find, the nearly shortest one found. A tray as text (the levels): the tubes from
 * the left, '|' between them, each one's colours from the bottom (1..9 a b c), "1213|2321|3132||".
 * The search: a weighted A* over the positions, the tubes' order forgotten (a position and the same tubes in another
 * order are one), with a lower bound of the moves left: every run of colour lying on another colour must leave its tube
 * once (Ball Sort: every ball of it), and of the tubes whose bottom is the same colour all but one must be emptied.
 */
#ifndef SORTING_H
#define SORTING_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

#define SRT_CAP 4
#define SRT_COLOURS 12
#define SRT_TUBES 15                            /* 12 colours, 2 empty tubes, the extra one */
#define SRT_HIST 4096
#define SRT_PATH 512
#define SRT_VARIANTS 4
#define SRT_EXTRA 255                           /* a history entry: the extra tube set on the rack */

static const int SRT_VARIANT_COLOURS[SRT_VARIANTS] = {4, 7, 10, 12};

typedef struct { uint8_t h[SRT_TUBES], c[SRT_TUBES][SRT_CAP]; } srt_pos;   /* c[t][0]: the bottom; colours 1.. */
typedef struct { uint8_t from, to, n; } srt_move;

typedef struct {
    int colours, base, tubes, one;              /* base: the tubes given; tubes: with the extra one when it is set */
    srt_pos p, start;
    srt_move hist[SRT_HIST];
    int nh, moves;                              /* moves: the pours (the extra tube is not one) */
    int par;                                    /* the shortest way from the start (see above; 0: not known) */
} srt_game;

/* ---- a position ---------------------------------------------------------------------------------------------------- */
static inline int srt_top(const srt_pos *p, int t) { return p->h[t] ? p->c[t][p->h[t] - 1] : 0; }

static inline int srt_run(const srt_pos *p, int t)  /* the layers of the top colour on top */
{
    int k = 0, c = srt_top(p, t);
    for (int i = p->h[t] - 1; i >= 0 && p->c[t][i] == c; i--) k++;
    return k;
}

static inline int srt_complete(const srt_pos *p, int t)   /* full of one colour */
{
    return p->h[t] == SRT_CAP && srt_run(p, t) == SRT_CAP;
}

/* how many layers a move from `from` to `to` takes (0: not allowed) */
static inline int srt_amount(const srt_pos *p, int tubes, int one, int from, int to)
{
    if (from < 0 || to < 0 || from >= tubes || to >= tubes || from == to || !p->h[from] || p->h[to] >= SRT_CAP)
        return 0;
    if (p->h[to] && srt_top(p, to) != srt_top(p, from)) return 0;
    int k = one ? 1 : srt_run(p, from), room = SRT_CAP - p->h[to];
    return k < room ? k : room;
}

static inline void srt_shift(srt_pos *p, int from, int to, int n)
{
    for (int i = 0; i < n; i++) {
        p->c[to][p->h[to]++] = p->c[from][--p->h[from]];
        p->c[from][p->h[from]] = 0;
    }
}

static inline int srt_solved_pos(const srt_pos *p, int tubes)
{
    for (int t = 0; t < tubes; t++)
        if (p->h[t] && !srt_complete(p, t)) return 0;
    return 1;
}

/* ---- the search ------------------------------------------------------------------------------------------------- */
static inline uint16_t srt_code(const srt_pos *p, int t)
{
    return (uint16_t)(p->c[t][0] | p->c[t][1] << 4 | p->c[t][2] << 8 | p->c[t][3] << 12);
}

static inline void srt_decode(uint16_t code, srt_pos *p, int t)
{
    p->h[t] = 0;
    for (int i = 0; i < SRT_CAP; i++) {
        p->c[t][i] = (uint8_t)(code >> (4 * i) & 15);
        if (p->c[t][i]) p->h[t] = (uint8_t)(i + 1);
    }
}

/* the position's key, the tubes' order forgotten */
static inline uint64_t srt_key(const uint16_t *codes, int tubes)
{
    uint16_t s[SRT_TUBES];
    for (int i = 0; i < tubes; i++) {
        int k = i;
        for (; k > 0 && s[k - 1] > codes[i]; k--) s[k] = s[k - 1];
        s[k] = codes[i];
    }
    uint64_t h = 0x9e3779b97f4a7c15ull;
    for (int i = 0; i < tubes; i++) {
        h ^= s[i] + 0x632be59bd9b4e019ull + (h << 6) + (h >> 2);
        h *= 0xbf58476d1ce4e5b9ull;
        h ^= h >> 31;
    }
    return h ? h : 1;
}

/* the lower bound of the moves left */
static inline int srt_bound(const srt_pos *p, int tubes, int one)
{
    int b = 0, bottoms[SRT_COLOURS + 1] = {0}, small[SRT_COLOURS + 1] = {0}, big[SRT_COLOURS + 1] = {0};
    for (int t = 0; t < tubes; t++) {
        if (!p->h[t]) continue;
        int c = p->c[t][0], run = 1;
        while (run < p->h[t] && p->c[t][run] == c) run++;
        for (int i = run; i < p->h[t]; i++)     /* the layers over the bottom run: each run (or each ball) leaves */
            b += one || p->c[t][i] != p->c[t][i - 1];
        bottoms[c]++;
        small[c] += run;                        /* Ball Sort: all the bottom runs of a colour but the largest move */
        if (run > big[c]) big[c] = run;
    }
    for (int c = 1; c <= SRT_COLOURS; c++)
        if (bottoms[c] > 1) b += one ? small[c] - big[c] : bottoms[c] - 1;
    return b;
}

typedef struct { uint16_t t[SRT_TUBES]; int32_t parent; int16_t g; uint8_t from, to, dead; } srt_node;
typedef struct { int32_t f, node; } srt_open;

static inline void srt_heap_push(srt_open *h, int *n, srt_open v)
{
    int i = (*n)++;
    while (i > 0) {
        int up = (i - 1) / 2;
        if (h[up].f <= v.f) break;
        h[i] = h[up];
        i = up;
    }
    h[i] = v;
}

static inline srt_open srt_heap_pop(srt_open *h, int *n)
{
    srt_open top = h[0], v = h[--*n];
    int i = 0;
    for (;;) {
        int k = 2 * i + 1;
        if (k >= *n) break;
        if (k + 1 < *n && h[k + 1].f < h[k].f) k++;
        if (h[k].f >= v.f) break;
        h[i] = h[k];
        i = k;
    }
    if (*n) h[i] = v;
    return top;
}

/* the moves worth trying from a position (the search's; the hint's): none out of a complete tube, a tube of one colour
 * not poured into an empty one, only the first empty tube as a target */
static inline int srt_useful(const srt_pos *p, int tubes, int one, int from, int to, int first_empty)
{
    if (!p->h[to] && to != first_empty) return 0;
    int n = srt_amount(p, tubes, one, from, to);
    if (!n) return 0;
    if (!p->h[to] && n == p->h[from] && srt_run(p, from) == p->h[from]) return 0;
    return n;
}

/* a way to solve p (tubes, one: Ball Sort), making no more than `cap` positions, the bound counted `w` quarters (4:
 * A*, the shortest way; more: faster, longer; 0: breadth first): the way's moves in path (SRT_PATH at most) and their
 * number; -1: none (no way, or none within cap: *full says which); looked: the positions made */
static inline int srt_solve(const srt_pos *start, int tubes, int one, int cap, int w, srt_move *path, int *looked,
                            int *full)
{
    if (full) *full = 0;
    if (looked) *looked = 0;
    if (srt_solved_pos(start, tubes)) return 0;
    int hsize = 1;
    while (hsize < 2 * cap) hsize <<= 1;
    size_t mask = (size_t)hsize - 1;
    srt_node *nodes = malloc((size_t)cap * sizeof *nodes);
    uint64_t *keys = calloc((size_t)hsize, sizeof *keys);
    int32_t *best = malloc((size_t)hsize * sizeof *best);   /* each position's node with the fewest moves to it */
    srt_open *heap = malloc((size_t)cap * sizeof *heap);
    int result = -1, nn = 0, nheap = 0;
    if (!nodes || !keys || !best || !heap) goto done;
    memset(&nodes[0], 0, sizeof nodes[0]);
    for (int t = 0; t < tubes; t++) nodes[0].t[t] = srt_code(start, t);
    nodes[0].parent = -1;
    uint64_t k0 = srt_key(nodes[0].t, tubes);
    keys[k0 & mask] = k0;
    best[k0 & mask] = 0;
    nn = 1;
    srt_heap_push(heap, &nheap, (srt_open){w * srt_bound(start, tubes, one), 0});
    while (nheap) {
        int at = srt_heap_pop(heap, &nheap).node;
        if (nodes[at].dead) continue;           /* reached since by a shorter way */
        nodes[at].dead = 1;                     /* expanded once */
        srt_pos p;
        for (int t = 0; t < tubes; t++) srt_decode(nodes[at].t[t], &p, t);
        if (srt_solved_pos(&p, tubes)) {        /* the way, back from here */
            int len = nodes[at].g;
            if (len > SRT_PATH) goto done;
            for (int x = at, i = len - 1; x > 0; x = nodes[x].parent, i--) {
                srt_pos b;
                for (int t = 0; t < tubes; t++) srt_decode(nodes[nodes[x].parent].t[t], &b, t);
                path[i] = (srt_move){nodes[x].from, nodes[x].to,
                                     (uint8_t)srt_amount(&b, tubes, one, nodes[x].from, nodes[x].to)};
            }
            result = len;
            goto done;
        }
        int first_empty = -1;
        for (int t = 0; t < tubes && first_empty < 0; t++)
            if (!p.h[t]) first_empty = t;
        for (int from = 0; from < tubes; from++) {
            if (!p.h[from] || srt_complete(&p, from)) continue;
            for (int to = 0; to < tubes; to++) {
                int n = srt_useful(&p, tubes, one, from, to, first_empty);
                if (!n) continue;
                srt_pos q = p;
                srt_shift(&q, from, to, n);
                uint16_t codes[SRT_TUBES];
                memcpy(codes, nodes[at].t, sizeof codes);
                codes[from] = srt_code(&q, from);
                codes[to] = srt_code(&q, to);
                uint64_t k = srt_key(codes, tubes);
                size_t i = k & mask;
                while (keys[i] && keys[i] != k) i = (i + 1) & mask;
                int g = nodes[at].g + 1;
                if (keys[i] && nodes[best[i]].g <= g) continue;   /* already reached as soon */
                if (nn >= cap) { if (full) *full = 1; goto done; }
                if (keys[i]) nodes[best[i]].dead = 1;
                keys[i] = k;
                best[i] = nn;
                srt_node *c = &nodes[nn];
                memcpy(c->t, codes, sizeof codes);
                c->parent = at;
                c->g = (int16_t)g;
                c->from = (uint8_t)from;
                c->to = (uint8_t)to;
                c->dead = 0;
                srt_heap_push(heap, &nheap, (srt_open){4 * g + w * srt_bound(&q, tubes, one), nn});
                nn++;
            }
        }
    }
done:
    if (looked) *looked = nn;
    free(nodes);
    free(keys);
    free(best);
    free(heap);
    return result;
}

/* ---- a game -------------------------------------------------------------------------------------------------------- */
static inline void srt_load(srt_game *g, int colours, int tubes, int one, const srt_pos *p)
{
    memset(g, 0, sizeof *g);
    g->colours = colours;
    g->base = g->tubes = tubes;
    g->one = one;
    g->p = g->start = *p;
}

/* the shuffle: four layers of each colour into `colours` tubes, two empty after them; none of them already complete,
 * and few layers on one of their own colour */
static inline void srt_fill(int colours, uint32_t *s, srt_pos *p)
{
    int bag[SRT_COLOURS * SRT_CAP];
    for (;;) {
        memset(p, 0, sizeof *p);
        for (int i = 0; i < colours * SRT_CAP; i++) bag[i] = 1 + i / SRT_CAP;
        pr_shuffle(s, bag, colours * SRT_CAP);
        int same = 0, done = 0;
        for (int t = 0; t < colours; t++) {
            for (int i = 0; i < SRT_CAP; i++) p->c[t][i] = (uint8_t)bag[t * SRT_CAP + i];
            p->h[t] = SRT_CAP;
            for (int i = 1; i < SRT_CAP; i++) same += p->c[t][i] == p->c[t][i - 1];
            done += srt_complete(p, t);
        }
        if (!done && same <= colours / 3) return;
    }
}

/* a new puzzle of `colours` from a seed: of SRT_DRAWS shuffles that can be solved, the one whose shortest way is the
 * longest (a tie: the one whose search made the most positions, the more tangled; the largest Ball Sort puzzles,
 * when the shortest way takes too long to find, by a nearly shortest one); the position, and its way (in sol, its
 * length returned: the tests play it) */
#define SRT_DRAWS 6
static inline int srt_generate(srt_game *g, int colours, int one, uint32_t seed, srt_move *sol)
{
    uint32_t s = pr_seed(seed);
    int best = -1, best_looked = 0;
    srt_pos p, keep;
    static srt_move way[SRT_PATH];
    for (int tries = 0, found = 0; found < SRT_DRAWS && tries < 50; tries++) {
        srt_fill(colours, &s, &p);
        int looked, full, len = srt_solve(&p, colours + 2, one, 20000, 4, way, &looked, &full);
        if (len < 0 && full) {                  /* the shortest way not found in time: a nearly shortest one */
            len = srt_solve(&p, colours + 2, one, 100000, 6, way, NULL, NULL);
            looked = 0;
        }
        if (len < 0) continue;
        found++;
        if (len > best || (len == best && looked > best_looked)) {
            best = len;
            best_looked = looked;
            keep = p;
            if (sol) memcpy(sol, way, (size_t)len * sizeof *sol);
        }
    }
    if (best < 0) {                             /* never seen: the colours in order, every tube complete */
        memset(&keep, 0, sizeof keep);
        for (int t = 0; t < colours; t++) {
            keep.h[t] = SRT_CAP;
            for (int i = 0; i < SRT_CAP; i++) keep.c[t][i] = (uint8_t)(t + 1);
        }
        best = 0;
    }
    srt_load(g, colours, colours + 2, one, &keep);
    g->par = best;
    return best;
}

static inline int srt_can(const srt_game *g, int from, int to) { return srt_amount(&g->p, g->tubes, g->one, from, to); }

static inline void srt_remember(srt_game *g, srt_move m)
{
    if (g->nh >= SRT_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + SRT_HIST / 2, (SRT_HIST / 2) * sizeof *g->hist);
        g->nh -= SRT_HIST / 2;
    }
    g->hist[g->nh++] = m;
}

static inline int srt_move_do(srt_game *g, int from, int to)   /* the layers moved; 0: not allowed */
{
    int n = srt_can(g, from, to);
    if (!n) return 0;
    srt_shift(&g->p, from, to, n);
    srt_remember(g, (srt_move){(uint8_t)from, (uint8_t)to, (uint8_t)n});
    g->moves++;
    return n;
}

static inline int srt_extra_ok(const srt_game *g) { return g->tubes == g->base && g->base < SRT_TUBES; }

static inline int srt_extra(srt_game *g)      /* the extra empty tube on the rack; 0: already there */
{
    if (!srt_extra_ok(g)) return 0;
    g->p.h[g->tubes] = 0;
    memset(g->p.c[g->tubes], 0, SRT_CAP);
    g->tubes++;
    srt_remember(g, (srt_move){SRT_EXTRA, 0, 0});
    return 1;
}

/* the last action taken back: 1 a pour, 2 the extra tube taken away; 0: nothing to undo */
static inline int srt_undo(srt_game *g)
{
    if (!g->nh) return 0;
    srt_move m = g->hist[--g->nh];
    if (m.from == SRT_EXTRA) {
        g->tubes--;
        return 2;
    }
    srt_shift(&g->p, m.to, m.from, m.n);
    g->moves--;
    return 1;
}

static inline int srt_solved(const srt_game *g) { return srt_solved_pos(&g->p, g->tubes); }

static inline int srt_stuck(const srt_game *g)  /* no move at all */
{
    for (int a = 0; a < g->tubes; a++)
        for (int b = 0; b < g->tubes; b++)
            if (srt_can(g, a, b)) return 0;
    return 1;
}

/* a way from the position: the shortest one; failing that (too many positions to look at), any. -1: none */
static inline int srt_way(const srt_pos *p, int tubes, int one, srt_move *way)
{
    for (int w = 4; w <= 16; w *= 2) {
        int full, len = srt_solve(p, tubes, one, 30000, w, way, NULL, &full);
        if (len >= 0 || !full) return len;      /* found, or the whole search done: no way */
    }
    return -1;
}

/* the hint: the first move of a way to solve the puzzle from here (-1: none found: the extra tube would help if it can
 * still be set, *extra 1; else a move must be undone) */
static inline int srt_hint(const srt_game *g, int *from, int *to, int *extra)
{
    static srt_move way[SRT_PATH];
    *extra = 0;
    if (srt_solved(g)) return -1;
    int len = srt_way(&g->p, g->tubes, g->one, way);
    if (len > 0) {
        *from = way[0].from;
        *to = way[0].to;
        return len;
    }
    if (srt_extra_ok(g)) {
        srt_pos q = g->p;
        q.h[g->tubes] = 0;
        memset(q.c[g->tubes], 0, SRT_CAP);
        if (srt_way(&q, g->tubes + 1, g->one, way) >= 0) *extra = 1;
    }
    return -1;
}

/* ---- a tray as text (the levels) -------------------------------------------------------------------------------- */
#define SRT_DIGITS "123456789abc"

/* 0: not a tray (a colour not four times, colours missing from 1 up, too many tubes, a tube over full) */
static inline int srt_parse(srt_game *g, const char *s, int one)
{
    srt_pos p;
    memset(&p, 0, sizeof p);
    int t = 0, count[SRT_COLOURS + 1] = {0}, colours = 0;
    for (;; s++) {
        if (*s == '|' || !*s) {
            if (++t >= SRT_TUBES) return 0;     /* the extra tube's place stays free */
            if (!*s) break;
            continue;
        }
        const char *c = strchr(SRT_DIGITS, *s);
        if (!c || p.h[t] >= SRT_CAP) return 0;
        int col = (int)(c - SRT_DIGITS) + 1;
        p.c[t][p.h[t]++] = (uint8_t)col;
        count[col]++;
    }
    while (colours < SRT_COLOURS && count[colours + 1]) colours++;
    for (int c = 1; c <= SRT_COLOURS; c++)
        if (count[c] != (c <= colours ? SRT_CAP : 0)) return 0;
    if (!colours) return 0;
    srt_load(g, colours, t, one, &p);
    return 1;
}

static inline void srt_text(const srt_pos *p, int tubes, char *out)   /* tubes * 5 bytes at most */
{
    int k = 0;
    for (int t = 0; t < tubes; t++) {
        if (t) out[k++] = '|';
        for (int i = 0; i < p->h[t]; i++) out[k++] = SRT_DIGITS[p->c[t][i] - 1];
    }
    out[k] = 0;
}

/* the notation: a move is its two tubes, a b c... from the left, "ac bd"; "+" sets the extra tube; "=k" plays the next
 * k moves of a way the search finds from here ("=": all of it) */
static inline int srt_script(srt_game *g, const char *s)
{
    static srt_move way[SRT_PATH];
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == '+') {
            if (!srt_extra(g)) return 0;
            s++;
            continue;
        }
        if (*s == '=') {                       /* "=": the whole way */
            int k = 0, all = s[1] < '0' || s[1] > '9';
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            int len = srt_way(&g->p, g->tubes, g->one, way);
            if (all) k = len;
            if (len < k) return 0;
            for (int i = 0; i < k; i++)
                if (!srt_move_do(g, way[i].from, way[i].to)) return 0;
            continue;
        }
        if (s[0] < 'a' || s[0] >= 'a' + g->tubes || s[1] < 'a' || s[1] >= 'a' + g->tubes) return 0;
        if (!srt_move_do(g, s[0] - 'a', s[1] - 'a')) return 0;
        s += 2;
    }
    return 1;
}

#endif
