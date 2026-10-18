/*
 * Hex Stacks' rules (hexstack.h): the board, a placement and its chain, the clears and the locks, the deals (a
 * level's sequence, endless chance), undo, the goals, the rules' own player (the hint, the levels' tool), the
 * notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/hexstack.h"
#include "games/puzrand.h"

/* the endless boards: small, medium, large */
const char *const hx_boards[HX_VARIANTS] = {
    "..ooo..|.oooo..|.ooooo.|.oooo..|..ooo..",
    "..ooo..|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|..ooo..",
    ".......|.oooo..|.ooooo.|oooooo.|ooooooo|oooooo.|.ooooo.|.oooo..",
};

/* ---- the board ----------------------------------------------------------------------------------------------- */
int hx_dir(int c, int d)
{
    int x = c % HX_COLS, y = c / HX_COLS, odd = y & 1;
    static const int DX[2][6] = {{1, 0, -1, -1, -1, 0}, {1, 1, 0, -1, 0, 1}}, DY[6] = {0, 1, 1, 0, -1, -1};
    x += DX[odd][d];
    y += DY[d];
    if (x < 0 || y < 0 || x >= HX_COLS || y >= HX_ROWS) return -1;
    return x + y * HX_COLS;
}

int hx_neighbours(const hx_pos *p, int c, int out[6])
{
    int n = 0;
    for (int d = 0; d < 6; d++) {
        int k = hx_dir(c, d);
        if (k >= 0 && p->kind[k] != HX_VOID) out[n++] = k;
    }
    return n;
}

int hx_top(const hx_pos *p, int c, int *band)
{
    int n = p->n[c];
    if (!n) { if (band) *band = 0; return -1; }
    int col = p->t[c][n - 1], b = 1;
    while (b < n && p->t[c][n - 1 - b] == col) b++;
    if (band) *band = b;
    return col;
}

int hx_open_empty(const hx_pos *p)
{
    int k = 0;
    for (int c = 0; c < HX_CELLS; c++) k += p->kind[c] == HX_OPEN && !p->n[c];
    return k;
}

int hx_full(const hx_pos *p) { return hx_open_empty(p) == 0; }

int hx_tray_empty(const hx_pos *p)
{
    for (int i = 0; i < HX_TRAY; i++)
        if (p->tray[i].n) return 0;
    return 1;
}

/* ---- stacks as text ------------------------------------------------------------------------------------------ */
static int parse_one(const char **sp, hx_tray_stack *o)
{
    const char *s = *sp;
    if (!(*s >= 'A' && *s <= 'X')) return 0;
    o->n = 0;
    int first = 1;
    while ((first && *s >= 'A' && *s <= 'X') || (!first && *s >= 'a' && *s <= 'x')) {
        int v = *s - (first ? 'A' : 'a'), col = v / 4, cnt = v % 4 + 1;
        if (col >= HX_COLOURS || o->n + cnt > HX_STACK) return 0;
        for (int i = 0; i < cnt; i++) o->t[o->n++] = (uint8_t)col;
        first = 0;
        s++;
    }
    *sp = s;
    return 1;
}

int hx_parse_stacks(const char *s, hx_tray_stack *out, int cap)
{
    int n = 0;
    while (*s) {
        if (*s == ' ') { s++; continue; }
        if (n >= cap || !parse_one(&s, &out[n])) return 0;
        n++;
    }
    return n;
}

void hx_stack_text(const hx_tray_stack *st, char *out)
{
    int k = 0;
    for (int i = 0; i < st->n;) {
        int col = st->t[i], cnt = 0;
        while (i < st->n && st->t[i] == col && cnt < 4) i++, cnt++;
        out[k] = (char)((k ? 'a' : 'A') + col * 4 + cnt - 1);
        k++;
    }
    out[k] = 0;
}

/* ---- the deals ----------------------------------------------------------------------------------------------- */
void hx_chance_stack(uint32_t *r, int colours, int press, const int *bias, int nb, hx_tray_stack *o)
{
    int one = 45 - 5 * press, three = 15 + 5 * press, roll = pr_below(r, 100);
    int bands = roll < one ? 1 : roll < 100 - three ? 2 : 3, prev = -1;
    if (colours <= 3 && bands == 3) bands = 2;
    int top = nb && pr_below(r, 3 + press) == 0 ? bias[pr_below(r, nb)] : -1;
    o->n = 0;
    for (int b = 0; b < bands; b++) {
        int col = b == bands - 1 && top >= 0 && top != prev ? top : pr_below(r, colours);
        if (col == prev) col = (col + 1 + pr_below(r, colours - 1)) % colours;
        int cnt = 1 + pr_below(r, 3);
        if (pr_below(r, 4) == 0 && cnt < 3) cnt++;
        for (int i = 0; i < cnt && o->n < HX_STACK; i++) o->t[o->n++] = (uint8_t)col;
        prev = col;
    }
}

static int endless_colours(const hx_pos *p) { int c = 4 + p->cleared / 30; return c > HX_COLOURS ? HX_COLOURS : c; }

static void deal(const hx_game *g, hx_pos *p)
{
    if (!hx_tray_empty(p)) return;
    if (g->goal == HX_ENDLESS) {
        int bias[HX_COLOURS], nb = 0, seen = 0;
        for (int c = 0; c < HX_CELLS; c++) {
            int t = hx_top(p, c, NULL);
            if (t >= 0 && !(seen >> t & 1)) seen |= 1 << t, bias[nb++] = t;
        }
        int cols = endless_colours(p);
        for (int i = 0; i < nb; i++)
            if (bias[i] >= cols) bias[i--] = bias[--nb];
        int press = p->cleared / 50 > 9 ? 9 : p->cleared / 50;
        for (int i = 0; i < HX_TRAY; i++) hx_chance_stack(&p->rng, cols, press, bias, nb, &p->tray[i]);
        return;
    }
    for (int i = 0; i < HX_TRAY && g->nseq > 0; i++) p->tray[i] = g->seq[p->next++ % g->nseq];
}

/* ---- the chain ----------------------------------------------------------------------------------------------- */
static int below(const hx_pos *p, int c)
{
    int b;
    hx_top(p, c, &b);
    return p->n[c] - b;
}

static void push(int *list, int *n, int c)
{
    for (int i = 0; i < *n; i++)
        if (list[i] == c) return;
    list[(*n)++] = c;
}

static int move_band(hx_pos *p, int from, int to, hx_event *ev, int *ne, int cap)
{
    int b, col = hx_top(p, from, &b);
    if (p->n[to] + b > HX_TALL) b = HX_TALL - p->n[to];
    if (b <= 0) return 0;
    for (int i = 0; i < b; i++) p->t[to][p->n[to]++] = (uint8_t)col;
    p->n[from] = (uint8_t)(p->n[from] - b);
    if (ev && *ne < cap) ev[*ne] = (hx_event){HX_EV_MOVE, (uint8_t)from, (uint8_t)to, (uint8_t)col, (uint8_t)b};
    (*ne)++;
    return 1;
}

static void unlocks(hx_pos *p, hx_event *ev, int *ne, int cap)
{
    for (int c = 0; c < HX_CELLS; c++)
        if (p->kind[c] == HX_LOCK && p->cleared >= p->need[c] * 10) {
            p->kind[c] = HX_OPEN;
            if (ev && *ne < cap) ev[*ne] = (hx_event){HX_EV_UNLOCK, (uint8_t)c, 0, 0, 0};
            (*ne)++;
        }
}

int hx_resolve(hx_pos *p, int cell, hx_event *ev, int cap)
{
    int list[HX_CELLS * 8], nl = 0, head = 0, ne = 0, steps = 0;
    list[nl++] = cell;
    while (head < nl && steps++ < HX_EVENTS) {
        int x = list[head++];
        /* (taken off the list: it may go on again) */
        memmove(list, list + head, (size_t)(nl - head) * sizeof *list);
        nl -= head;
        head = 0;
        int bx, col = hx_top(p, x, &bx);
        if (col < 0) continue;
        int nb[6], k = hx_neighbours(p, x, nb), m[6], nm = 0;
        for (int i = 0; i < k; i++)
            if (p->n[nb[i]] && hx_top(p, nb[i], NULL) == col) m[nm++] = nb[i];
        if (!nm) {
            if (bx >= HX_CLEAR) {
                p->n[x] = (uint8_t)(p->n[x] - bx);
                p->cleared = (uint16_t)(p->cleared + bx);
                p->by_colour[col] = (uint16_t)(p->by_colour[col] + bx);
                if (ev && ne < cap) ev[ne] = (hx_event){HX_EV_CLEAR, (uint8_t)x, 0, (uint8_t)col, (uint8_t)bx};
                ne++;
                unlocks(p, ev, &ne, cap);
                push(list, &nl, x);
            }
            continue;
        }
        int least = 1 << 20, best = -1, bb = 0;
        for (int i = 0; i < nm; i++) {
            int bl = below(p, m[i]), band;
            hx_top(p, m[i], &band);
            if (bl < least || (bl == least && band > bb)) least = bl, best = m[i], bb = band;
        }
        if (below(p, x) <= least) {             /* x gathers them all */
            int moved = 0;
            for (int i = 0; i < nm; i++) moved |= move_band(p, m[i], x, ev, &ne, cap);
            if (!moved) continue;
            push(list, &nl, x);
            for (int i = 0; i < nm; i++) push(list, &nl, m[i]);
        } else {                                /* x gives its band to the purest */
            if (!move_band(p, x, best, ev, &ne, cap)) continue;
            push(list, &nl, best);
            push(list, &nl, x);
        }
    }
    return ne;
}

/* ---- a game -------------------------------------------------------------------------------------------------- */
static int parse_board(hx_pos *p, const char *board, const hx_tray_stack *pre, int npre)
{
    memset(p, 0, sizeof *p);
    int x = 0, y = 0, used = 0, open = 0;
    for (const char *s = board; *s; s++) {
        if (*s == '|') { y++; x = 0; continue; }
        if (x >= HX_COLS || y >= HX_ROWS) return 0;
        int c = x + y * HX_COLS;
        switch (*s) {
        case '.': case ' ': break;
        case 'o': p->kind[c] = HX_OPEN; open++; break;
        case '#': p->kind[c] = HX_BLOCK; break;
        case 'S':
            if (used >= npre) return 0;
            p->kind[c] = HX_OPEN;
            p->n[c] = pre[used].n;
            memcpy(p->t[c], pre[used].t, pre[used].n);
            used++;
            open++;
            break;
        default:
            if (*s < '1' || *s > '9') return 0;
            p->kind[c] = HX_LOCK;
            p->need[c] = (uint8_t)(*s - '0');
            break;
        }
        x++;
    }
    return open > 0 && used == npre;
}

int hx_load(hx_game *g, const char *board, const char *pre, const char *seq, int goal, int colour, int target,
            int colours)
{
    hx_tray_stack ps[HX_CELLS];
    int npre = pre && *pre ? hx_parse_stacks(pre, ps, HX_CELLS) : 0;
    if (pre && *pre && !npre) return 0;
    memset(g, 0, sizeof *g);
    g->nseq = seq ? hx_parse_stacks(seq, g->seq, HX_SEQ) : 0;
    if (goal != HX_ENDLESS && g->nseq < HX_TRAY) return 0;
    if (!parse_board(&g->p, board, ps, npre)) return 0;
    g->goal = goal;
    g->colour = colour;
    g->target = target;
    g->colours = colours;
    g->last_cell = -1;
    deal(g, &g->p);
    return 1;
}

#ifndef HX_GEN                                  /* not in the levels' tool (tools/hexstack_gen.c): it makes them */
#include "games/hexstack_levels.h"

int hx_level(hx_game *g, int level)
{
    if (level < 0 || level >= HX_LEVELS) return 0;
    const hx_level_def *d = &hx_levels[level];
    if (!hx_load(g, d->board, d->pre, d->seq, d->goal, d->colour, d->target, d->colours)) return 0;
    g->par = d->par;
    g->variant = -1;
    return 1;
}
#endif

void hx_endless(hx_game *g, int variant, uint32_t seed)
{
    int v = variant < 0 || variant >= HX_VARIANTS ? 0 : variant;
    memset(g, 0, sizeof *g);
    parse_board(&g->p, hx_boards[v], NULL, 0);
    g->goal = HX_ENDLESS;
    g->variant = v;
    g->last_cell = -1;
    g->p.rng = pr_seed(seed);
    deal(g, &g->p);
}

int hx_goal_left(const hx_game *g)
{
    int done = g->goal == HX_COLOUR ? g->p.by_colour[g->colour] : g->p.cleared;
    return g->goal == HX_ENDLESS ? 0 : done >= g->target ? 0 : g->target - done;
}

int hx_solved(const hx_game *g) { return g->goal != HX_ENDLESS && hx_goal_left(g) == 0; }
int hx_over(const hx_game *g) { return hx_solved(g) || hx_full(&g->p); }
int hx_score(const hx_game *g) { return g->p.cleared; }

int hx_stars(const hx_game *g)
{
    static const int TWO[HX_VARIANTS] = {150, 250, 400}, THREE[HX_VARIANTS] = {350, 600, 1000};
    int v = g->variant < 0 || g->variant >= HX_VARIANTS ? 0 : g->variant, s = g->p.cleared;
    return s >= THREE[v] ? 3 : s >= TWO[v] ? 2 : 1;
}

static int place_on(const hx_game *g, hx_pos *p, int slot, int cell, hx_event *ev, int cap, int *ne)
{
    if (slot < 0 || slot >= HX_TRAY || cell < 0 || cell >= HX_CELLS || !p->tray[slot].n) return 0;
    if (p->kind[cell] != HX_OPEN || p->n[cell]) return 0;
    p->n[cell] = p->tray[slot].n;
    memcpy(p->t[cell], p->tray[slot].t, p->tray[slot].n);
    p->tray[slot].n = 0;
    p->placed++;
    int n = hx_resolve(p, cell, ev, cap);
    if (ne) *ne = n;
    deal(g, p);
    return 1;
}

int hx_place(hx_game *g, int slot, int cell)
{
    if (hx_over(g)) return 0;
    hx_pos keep = g->p;
    int ne = 0;
    if (!place_on(g, &g->p, slot, cell, g->ev, HX_EVENTS, &ne)) return 0;
    g->prev = keep;
    g->has_prev = 1;
    g->nev = ne < HX_EVENTS ? ne : HX_EVENTS;
    g->last_cell = cell;
    g->last_slot = slot;
    return 1;
}

int hx_undo(hx_game *g)
{
    if (!g->has_prev) return 0;
    g->p = g->prev;
    g->has_prev = 0;
    g->nev = 0;
    g->last_cell = -1;
    return 1;
}

/* ---- the rules' player --------------------------------------------------------------------------------------- */
/* how good a position looks: the goal's tiles cleared most, then room, then tops that match their neighbours, pure
 * tall stacks, few bands */
static long eval(const hx_game *g, const hx_pos *p)
{
    long v = 0;
    int goal = g->goal == HX_COLOUR ? p->by_colour[g->colour] : p->cleared;
    if (g->goal != HX_ENDLESS && goal >= g->target) return 1000000L - p->placed * 1000L;
    v += 300L * goal + (g->goal == HX_COLOUR ? 60L * p->cleared : 0);
    int empty = hx_open_empty(p);
    if (!empty) return -1000000L + v;
    v += 26L * empty + (empty < 4 ? -120L * (4 - empty) : 0);
    for (int c = 0; c < HX_CELLS; c++) {
        if (p->kind[c] == HX_LOCK) v -= 4;
        if (!p->n[c]) continue;
        int band, col = hx_top(p, c, &band), bands = 1;
        for (int i = 1; i < p->n[c]; i++) bands += p->t[c][i] != p->t[c][i - 1];
        v -= 9L * (bands - 1) + (p->n[c] > 10 ? 4L * (p->n[c] - 10) : 0);
        v += (long)band * band;
        int nb[6], k = hx_neighbours(p, c, nb);
        for (int i = 0; i < k; i++) {
            if (!p->n[nb[i]]) continue;
            int t = hx_top(p, nb[i], NULL);
            v += t == col ? 6 : -3;
        }
    }
    return v;
}

int hx_best(const hx_game *g, int noise, uint32_t *rng, int *slot, int *cell)
{
    long best = 0;
    int got = 0;
    if (hx_over(g)) return 0;
    for (int s = 0; s < HX_TRAY; s++) {
        if (!g->p.tray[s].n) continue;
        int same = 0;                           /* the same stack twice: tried once */
        for (int k = 0; k < s; k++)
            if (g->p.tray[k].n == g->p.tray[s].n && !memcmp(g->p.tray[k].t, g->p.tray[s].t, g->p.tray[s].n)) same = 1;
        if (same) continue;
        for (int c = 0; c < HX_CELLS; c++) {
            if (g->p.kind[c] != HX_OPEN || g->p.n[c]) continue;
            hx_pos t = g->p;
            place_on(g, &t, s, c, NULL, 0, NULL);
            long v = eval(g, &t);
            if (noise > 0 && rng) v += pr_below(rng, noise);
            if (!got || v > best) best = v, *slot = s, *cell = c, got = 1;
        }
    }
    return got;
}

int hx_hint(const hx_game *g, int *slot, int *cell) { return hx_best(g, 0, NULL, slot, cell); }

int hx_play_out(hx_game *g, int noise, uint32_t seed, int most, int *won, int *room)
{
    uint32_t r = pr_seed(seed);
    int k = 0, least = 99;
    while (k < most && !hx_over(g)) {
        int s, c;
        if (!hx_best(g, noise, &r, &s, &c) || !hx_place(g, s, c)) break;
        k++;
        int e = hx_open_empty(&g->p);
        if (!hx_solved(g) && e < least) least = e;
    }
    if (won) *won = hx_solved(g);
    if (room) *room = least;
    return k;
}

/* ---- the notation -------------------------------------------------------------------------------------------- */
int hx_script(hx_game *g, const char *moves)
{
    const char *s = moves;
    while (*s) {
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        if (*s == '=') {
            s++;
            int k = 0, all = !(*s >= '0' && *s <= '9');
            while (*s >= '0' && *s <= '9') k = k * 10 + (*s++ - '0');
            for (int i = 0; (all ? i < 100000 : i < k) && !hx_over(g); i++) {
                int sl, c;
                if (!hx_hint(g, &sl, &c) || !hx_place(g, sl, c)) return 0;
            }
            continue;
        }
        if (s[0] < 'a' || s[0] > 'c' || s[1] < 'a' || s[1] >= 'a' + HX_COLS || s[2] < '1' || s[2] > '9') return 0;
        int cell = (s[1] - 'a') + (s[2] - '1') * HX_COLS;
        if (!hx_place(g, s[0] - 'a', cell)) return 0;
        s += 3;
    }
    return 1;
}
