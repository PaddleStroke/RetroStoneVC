/*
 * Accordion: the rules (accordion.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/accordion.h"

void acd_new(solrec *r, uint32_t seed)
{
    uint8_t deck[2 * 52];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    acd_deal_cards(r, 0, deck);
    r->seed = seed;
}

void acd_deal_cards(solrec *r, int variant, const uint8_t *deck)
{
    (void)variant;
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    p->npiles = ACD_PILES;
    for (int i = 0; i < ACD_PILES; i++) {
        p->kind[i] = SP_TAB;
        p->c[i][0] = deck[i];
        p->n[i] = 1;
    }
    sol_begin(r);
}

int acd_piles(const solpos *p)
{
    int k = 0;
    while (k < ACD_PILES && p->n[k]) k++;
    return k;
}

static int match(int a, int b) { return CARD_SUIT(a) == CARD_SUIT(b) || CARD_RANK(a) == CARD_RANK(b); }

int acd_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to;
    if (SM_KIND(m.kind) != SM_MOVE || a >= ACD_PILES || b >= a || (a - b != 1 && a - b != 3) || !p->n[a]) return 0;
    if (m.n != 1 && m.n != p->n[a]) return 0;
    return p->n[b] && match(sol_top(p, a), sol_top(p, b));
}

int acd_play(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES - ACD_PILES || !acd_legal(&r->pos, m)) return 0;
    m.n = r->pos.n[m.from];                     /* the whole pile */
    sol_push(r, m);
    for (int j = m.from + 1; j < ACD_PILES && r->pos.n[j]; j++)   /* the row closes up */
        sol_push(r, sol_mv(SM_MOVE | SM_AUTO, j, j - 1, r->pos.n[j]));
    return 1;
}

int acd_stuck(const solpos *p)
{
    int n = acd_piles(p);
    for (int i = 1; i < n; i++)
        if (match(sol_top(p, i), sol_top(p, i - 1)) || (i >= 3 && match(sol_top(p, i), sol_top(p, i - 3)))) return 0;
    return 1;
}

int acd_won(const solpos *p) { return acd_piles(p) == 1; }

int acd_stars(int piles) { return piles <= 1 ? 3 : piles <= 3 ? 2 : piles <= 6 ? 1 : 0; }

/* ---- the search: the row as its top cards only (what lies under a top card never matters again) ---------------- */
typedef struct {
    uint64_t *seen;
    uint32_t mask;
    long used, budget;
    int best, len;                              /* the fewest piles reached, and the way there */
    uint8_t way[ACD_PILES], bway[ACD_PILES];    /* each move: the pile's place * 2 + (onto the third: 1) */
} acd_ctx;

static uint64_t key_of(const uint8_t *t, int n)
{
    uint64_t h = 1469598103934665603ull ^ (uint64_t)n;
    for (int i = 0; i < n; i++) h = (h ^ t[i]) * 1099511628211ull;
    return h | 1;
}

/* the moves of a row, those worth trying first first: onto the third pile before onto the neighbour, the far right
 * of the row first (it leaves the most to come) */
static int row_moves(const uint8_t *t, int n, uint8_t *out)
{
    int k = 0;
    for (int i = n - 1; i >= 3; i--)
        if (match(t[i], t[i - 3])) out[k++] = (uint8_t)(i * 2 + 1);
    for (int i = n - 1; i >= 1; i--)
        if (match(t[i], t[i - 1])) out[k++] = (uint8_t)(i * 2);
    return k;
}

static int dfs(acd_ctx *c, const uint8_t *t, int n, int depth)
{
    if (n < c->best) {
        c->best = n;
        c->len = depth;
        memcpy(c->bway, c->way, (size_t)depth);
        if (n == 1) return 1;
    }
    if (c->used >= c->budget) return 0;
    uint64_t k = key_of(t, n);
    for (uint32_t i = (uint32_t)k & c->mask;; i = (i + 1) & c->mask) {
        if (c->seen[i] == k) return 0;
        if (!c->seen[i]) { c->seen[i] = k; break; }
    }
    c->used++;
    uint8_t mv[2 * ACD_PILES], u[ACD_PILES];
    int m = row_moves(t, n, mv);
    for (int j = 0; j < m; j++) {
        int i = mv[j] / 2, to = mv[j] & 1 ? i - 3 : i - 1;
        memcpy(u, t, (size_t)n);
        u[to] = t[i];
        memmove(u + i, u + i + 1, (size_t)(n - 1 - i));
        c->way[depth] = mv[j];
        if (dfs(c, u, n - 1, depth + 1)) return 1;
        if (c->used >= c->budget) return 0;
    }
    return 0;
}

int acd_solve(const solpos *p, long budget, solmove *path, int *len, long *nodes)
{
    acd_ctx c;
    uint8_t t[ACD_PILES];
    int n = acd_piles(p);
    for (int i = 0; i < n; i++) t[i] = (uint8_t)sol_top(p, i);
    uint32_t size = 1024;
    while (size < 2 * (uint64_t)budget && size < (1u << 24)) size *= 2;
    c.seen = calloc(size, sizeof *c.seen);
    c.mask = size - 1;
    c.used = 0;
    c.budget = c.seen ? budget : 0;
    c.best = n + 1;
    c.len = 0;
    dfs(&c, t, n, 0);
    free(c.seen);
    for (int i = 0; i < c.len; i++) {
        int from = c.bway[i] / 2;
        path[i] = sol_mv(SM_MOVE, from, c.bway[i] & 1 ? from - 3 : from - 1, 1);
    }
    *len = c.len;
    if (nodes) *nodes = c.used;
    return c.best > n ? n : c.best;
}

/* the hint: the first move of the best way a short search sees (a few thousand rows) */
solmove acd_hint(const solpos *p)
{
    solmove path[ACD_PILES];
    int len;
    acd_solve(p, 20000, path, &len, NULL);
    return len ? path[0] : sol_mv(SM_NONE, 0, 0, 0);
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p)
{
    (void)p;
    return none();
}

static int pickable(const solpos *p, int pile, int i) { return p->n[pile] && i == p->n[pile] - 1; }

/* a tap on a pile: onto the third pile to its left if it can, else onto its neighbour */
static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    (void)found_only;
    if (!pickable(p, pile, i)) return none();
    for (int d = 3; d >= 1; d -= 2) {
        solmove m = sol_mv(SM_MOVE, pile, pile - d, p->n[pile]);
        if (pile >= d && acd_legal(p, m)) return m;
    }
    return none();
}

static void deal(solrec *r, uint32_t n) { acd_new(r, n); }

const solrules acd_rules = {acd_legal, acd_play, stock, acd_hint, tap, pickable, acd_won, deal};

/* the levels' check (sollevels.h): the moves of a row, the search's order */
static int cands(const solpos *p, solmove *list)
{
    uint8_t t[ACD_PILES], mv[2 * ACD_PILES];
    int n = acd_piles(p);
    for (int i = 0; i < n; i++) t[i] = (uint8_t)sol_top(p, i);
    int m = row_moves(t, n, mv);
    for (int j = 0; j < m && j < SOLVE_BRANCH; j++) {
        int i = mv[j] / 2;
        list[j] = sol_mv(SM_MOVE, i, mv[j] & 1 ? i - 3 : i - 1, p->n[i]);
    }
    return m < SOLVE_BRANCH ? m : SOLVE_BRANCH;
}

const solsearch acd_search = {&acd_rules, cands, NULL, NULL, NULL};

int acd_script(solrec *r, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        if (*s == '#') {
            uint32_t n = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) n = n * 10 + (uint32_t)(*s - '0');
            acd_new(r, n);
            continue;
        }
        solmove m = none();
        if (*s == '?') m = acd_hint(&r->pos), s++;
        else {
            int k = 0;
            while (*s >= '0' && *s <= '9') k = k * 10 + (*s++ - '0');
            int d = *s == 'a' ? 1 : *s == 'c' ? 3 : 0;
            if (!d || k < 1) return 0;
            s++;
            m = sol_mv(SM_MOVE, k - 1, k - 1 - d, 1);
            if (k - 1 - d < 0) return 0;
        }
        if (m.kind == SM_NONE || !acd_play(r, m)) return 0;
    }
    return 1;
}
