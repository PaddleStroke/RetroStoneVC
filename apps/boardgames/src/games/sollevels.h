/*
 * The patience games' designed deals (Klondike, FreeCell, Spider, Pyramid, TriPeaks, Golf, Yukon, Canfield): the
 * levels, and the search that proves each one can be won. No SDL: the rules, their tests and the tools that make the
 * levels (tools/<id>_gen.c) use it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A level is a variant and the pack in the order it is dealt (never a seed: a later change to the shuffle cannot
 * change the levels), a letter a card: A..M the spades ace to king, N..Z the hearts, a..m the diamonds, n..z the clubs
 * (Spider's two packs: each card twice). The rules deal it as they deal a shuffled pack (<id>_deal_cards).
 *
 * The search (sol_solve): depth first over the game's own moves (its rules' play(), the moves it makes by itself
 * included), the moves worth trying in the game's order (its cands()), each position seen once (a table of keys), up
 * to a number of positions; or, for a game that says how promising a position looks (Spider), the most promising
 * position first. It knows every card, face down or not: it proves a deal can be won, not that a player can see how.
 */
#ifndef SOLLEVELS_H
#define SOLLEVELS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/solitaire.h"

typedef struct {
    uint8_t variant;
    const char *cards;                          /* the pack in the order dealt, a letter a card */
    const char *win;                            /* the search's win (sol_win_write), or NULL: the tests search again */
} sollevel;

/* a win as letters (Spider's levels keep theirs: its search takes long): each move of cards three letters, 'A' + its
 * pile, 'A' + the pile it goes to, 'A' + how many; '*' the stock's move. out: 3 n + 1 characters at most. */
static inline void sol_win_write(const solmove *path, int n, char *out)
{
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (SM_KIND(path[i].kind) != SM_MOVE) { out[k++] = '*'; continue; }
        out[k++] = (char)('A' + path[i].from);
        out[k++] = (char)('A' + path[i].to);
        out[k++] = (char)('A' + path[i].n);
    }
    out[k] = 0;
}

/* the win played from the deal r: 1 when every move was legal and the game is won */
static inline int sol_win_play(solrec *r, const solrules *R, const char *s)
{
    while (*s) {
        solmove m;
        if (*s == '*') m = R->stock(&r->pos), s++;
        else if (s[1] && s[2]) m = sol_mv(SM_MOVE, s[0] - 'A', s[1] - 'A', s[2] - 'A'), s += 3;
        else return 0;
        if (m.kind == SM_NONE || !R->play(r, m)) return 0;
    }
    return R->won(&r->pos);
}

static inline char sol_card_letter(int card)
{
    static const char a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int s = CARD_SUIT(card), r = CARD_RANK(card);
    return s < 4 && r >= 1 && r <= 13 ? a[s * 13 + r - 1] : '?';
}

static inline int sol_letter_card(char ch)      /* CARD_NONE: not a card's letter */
{
    int i = ch >= 'A' && ch <= 'Z' ? ch - 'A' : ch >= 'a' && ch <= 'z' ? ch - 'a' + 26 : -1;
    return i < 0 ? CARD_NONE : CARD(i / 13, i % 13 + 1);
}

/* a level's cards into out (at most max): how many; -1: a letter that is not a card */
static inline int sol_cards_read(const char *s, uint8_t *out, int max)
{
    int n = 0;
    for (; *s && n < max; s++) {
        int c = sol_letter_card(*s);
        if (c == CARD_NONE) return -1;
        out[n++] = (uint8_t)c;
    }
    return n;
}

static inline void sol_cards_write(const uint8_t *cards, int n, char *out)
{
    for (int i = 0; i < n; i++) out[i] = sol_card_letter(cards[i]);
    out[n] = 0;
}

/* ---- the search -------------------------------------------------------------------------------------------------- */
#define SOLVE_BRANCH 192                        /* the most moves a position offers the search */

typedef struct {
    const solrules *R;
    int (*cands)(const solpos *p, solmove *list);   /* the moves worth trying, best first (SOLVE_BRANCH at most) */
    int (*done)(const solpos *p);               /* won, or as good as (NULL: R->won) */
    uint64_t (*key)(const solpos *p);           /* a position's key, never 0 (NULL: sol_key: every card, the
                                                 * redeals) */
    int (*eval)(const solpos *p);               /* how promising a position looks: set, the search takes the best
                                                 * first (sol_solve_best) instead of going depth first */
} solsearch;

static inline uint64_t sol_key_of(const solpos *p, int redeals)
{
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < p->npiles; i++) {
        h = (h ^ (p->n[i] * 128u + p->down[i])) * 1099511628211ull;
        for (int k = 0; k < p->n[i]; k++) h = (h ^ p->c[i][k]) * 1099511628211ull;
    }
    if (redeals) h = (h ^ p->redeals) * 1099511628211ull;
    return h | 1;
}

static inline uint64_t sol_key(const solpos *p) { return sol_key_of(p, 1); }

/* the moves m (n of them) into list, the highest score v first (the order kept among equals), those scored 0 or less
 * left out: how many (SOLVE_BRANCH at most) */
static inline int sol_best_first(solmove *m, int *v, int n, solmove *list)
{
    int k = 0;
    for (; k < n && k < SOLVE_BRANCH; k++) {
        int best = k;
        for (int i = k + 1; i < n; i++)
            if (v[i] > v[best]) best = i;
        if (v[best] <= 0) break;
        solmove t = m[best];
        int tv = v[best];
        memmove(m + k + 1, m + k, (size_t)(best - k) * sizeof *m);
        memmove(v + k + 1, v + k, (size_t)(best - k) * sizeof *v);
        m[k] = t, v[k] = tv;
        list[k] = t;
    }
    return k;
}

/* ---- the best-first search (solsearch.eval set): the most promising position first ------------------------------ */
/* a position in a few bytes (each pile: its count, its face-down count, its cards; then the redeals), and back into a
 * position of the same game (p: one of it already, its kinds kept) */
static inline int sol_pack(const solpos *p, uint8_t *o)
{
    int k = 0;
    for (int i = 0; i < p->npiles; i++) {
        o[k++] = p->n[i];
        o[k++] = p->down[i];
        memcpy(o + k, p->c[i], p->n[i]);
        k += p->n[i];
    }
    o[k++] = p->redeals;
    return k;
}

static inline void sol_unpack(solpos *p, const uint8_t *o)
{
    int k = 0;
    for (int i = 0; i < p->npiles; i++) {
        p->n[i] = o[k++];
        p->down[i] = o[k++];
        memcpy(p->c[i], o + k, p->n[i]);
        k += p->n[i];
    }
    p->redeals = o[k];
}

typedef struct { uint32_t at, parent; int len; solmove m; } sol_node;
typedef struct { int v; uint32_t node; } sol_heap;

static inline void sol_heap_push(sol_heap *h, int *n, sol_heap x)
{
    int i = (*n)++;
    while (i > 0 && h[(i - 1) / 2].v < x.v) h[i] = h[(i - 1) / 2], i = (i - 1) / 2;
    h[i] = x;
}

static inline sol_heap sol_heap_pop(sol_heap *h, int *n)
{
    sol_heap top = h[0], x = h[--*n];
    int i = 0;
    for (;;) {
        int c = 2 * i + 1;
        if (c >= *n) break;
        if (c + 1 < *n && h[c + 1].v > h[c].v) c++;
        if (h[c].v <= x.v) break;
        h[i] = h[c], i = c;
    }
    if (*n) h[i] = x;
    return top;
}

/* sol_solve's way when the game gives an eval: positions taken best first (the highest eval), each seen once */
static inline int sol_solve_best(const solsearch *S, const solrec *r, long budget, solmove *path, int max, long *nodes)
{
    const int TB = 1 << 22;
    int (*done)(const solpos *) = S->done ? S->done : S->R->won;
    long cap = budget * 8 + 64;                 /* the positions kept: each one expanded has a few dozen children */
    int psize = 2 * r->pos.npiles + 1;
    for (int i = 0; i < r->pos.npiles; i++) psize += r->pos.n[i];
    uint64_t *seen = calloc((size_t)TB, sizeof *seen);
    sol_node *nd = malloc((size_t)cap * sizeof *nd);
    uint8_t *arena = malloc((size_t)cap * (size_t)psize);
    sol_heap *heap = malloc((size_t)cap * sizeof *heap);
    solmove *list = malloc(SOLVE_BRANCH * sizeof *list);
    solrec *w = malloc(sizeof *w);
    long used = 0, nn = 0;
    int nh = 0, found = -1;
    if (!seen || !nd || !arena || !heap || !list || !w) goto out;
    *w = *r;
    if (done(&w->pos)) { found = 0; goto out; }
    sol_pack(&w->pos, arena);
    nd[0] = (sol_node){0, 0, r->n, sol_mv(SM_NONE, 0, 0, 0)};
    nn = 1;
    sol_heap_push(heap, &nh, (sol_heap){S->eval(&w->pos), 0});
    while (nh > 0 && used < budget) {
        sol_heap top = sol_heap_pop(heap, &nh);
        used++;
        const uint8_t *at = arena + (size_t)nd[top.node].at;
        sol_unpack(&w->pos, at);
        int count = S->cands(&w->pos, list);
        for (int j = 0; j < count; j++) {
            sol_unpack(&w->pos, at);
            w->n = nd[top.node].len;
            if (!S->R->play(w, list[j])) continue;
            uint64_t k = S->key ? S->key(&w->pos) : sol_key(&w->pos);
            int fresh = 0;
            for (uint32_t i = (uint32_t)k & (uint32_t)(TB - 1);; i = (i + 1) & (uint32_t)(TB - 1)) {
                if (seen[i] == k) break;
                if (!seen[i]) { seen[i] = k; fresh = 1; break; }
            }
            if (!fresh) continue;
            if (done(&w->pos)) {                /* the moves back to the start */
                int len = 1;
                for (uint32_t q = top.node; q; q = nd[q].parent) len++;
                if (len > max) goto out;
                path[len - 1] = list[j];
                int i = len - 2;
                for (uint32_t q = top.node; q; q = nd[q].parent) path[i--] = nd[q].m;
                found = len;
                goto out;
            }
            if (nn >= cap) goto out;
            nd[nn] = (sol_node){(uint32_t)((size_t)nn * psize), top.node, w->n, list[j]};
            sol_pack(&w->pos, arena + (size_t)nn * psize);
            sol_heap_push(heap, &nh, (sol_heap){S->eval(&w->pos), (uint32_t)nn});
            nn++;
        }
    }
out:
    if (nodes) *nodes = used;
    free(seen);
    free(nd);
    free(arena);
    free(heap);
    free(list);
    free(w);
    return found;
}

/* the player's moves that win from the game r (path, at most max), in the order played; how many (0: won already),
 * -1: none found within `budget` positions. *nodes: the positions it looked at. */
static inline int sol_solve(const solsearch *S, const solrec *r, long budget, solmove *path, int max, long *nodes)
{
    if (S->eval) return sol_solve_best(S, r, budget, path, max, nodes);
    const int TB = 1 << 22;
    int (*done)(const solpos *) = S->done ? S->done : S->R->won;
    uint64_t *seen = calloc((size_t)TB, sizeof *seen);
    solpos *step = malloc((size_t)(max + 1) * sizeof *step);
    solmove *list = malloc((size_t)(max + 1) * SOLVE_BRANCH * sizeof *list);
    int *count = malloc((size_t)(max + 1) * sizeof *count), *next = malloc((size_t)(max + 1) * sizeof *next);
    int *len = malloc((size_t)(max + 1) * sizeof *len);
    solrec *w = malloc(sizeof *w);
    long used = 0;
    int found = -1;
    if (!seen || !step || !list || !count || !next || !len || !w) goto out;
    *w = *r;
    if (done(&w->pos)) { found = 0; goto out; }
    for (int d = 0;;) {
        {                                       /* open the position at depth d */
            uint64_t k = S->key ? S->key(&w->pos) : sol_key(&w->pos);
            int fresh = 0;
            for (uint32_t i = (uint32_t)k & (uint32_t)(TB - 1);; i = (i + 1) & (uint32_t)(TB - 1)) {
                if (seen[i] == k) break;
                if (!seen[i]) { seen[i] = k; fresh = 1; break; }
            }
            if (fresh && d < max && ++used <= budget) {
                step[d] = w->pos;
                len[d] = w->n;
                count[d] = S->cands(&w->pos, list + (size_t)d * SOLVE_BRANCH);
                next[d] = 0;
            } else {
                if (used > budget) goto out;
                d--;                            /* back to the parent: it tries its next move */
            }
        }
        for (;;) {                              /* the next move at depth d, else back up */
            if (d < 0) goto out;
            if (next[d] >= count[d]) { d--; continue; }
            solmove m = list[(size_t)d * SOLVE_BRANCH + next[d]++];
            w->pos = step[d];
            w->n = len[d];
            if (!S->R->play(w, m)) continue;
            path[d] = m;
            if (done(&w->pos)) { found = d + 1; goto out; }
            d++;
            break;
        }
    }
out:
    if (nodes) *nodes = used;
    free(seen);
    free(step);
    free(list);
    free(count);
    free(next);
    free(len);
    free(w);
    return found;
}

#endif
