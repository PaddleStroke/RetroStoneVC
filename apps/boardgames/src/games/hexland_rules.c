/*
 * Hex Landscape: the rules (hexland.h): the map of hexes, placements and their points, regions and quests, the stack
 * and its bonuses, the hint and the greedy player, tiles and levels as text, the landscapes' maker. No SDL: tested by
 * tests/hexland_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/hexland.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const int DQ[6] = {1, 0, -1, -1, 0, 1}, DR[6] = {0, 1, 1, 0, -1, -1};
static const char LETTER[HXL_TERRAINS] = {'.', 'm', 'f', 'w', 'v', 'r', 't'};
static int terrain_of(char ch);
static void put(char *o, int *n, int cap, const char *s);

/* ---- the map ----------------------------------------------------------------------------------------------------- */
int hxl_cell(int q, int r)
{
    q += HXL_N / 2;
    r += HXL_N / 2;
    return q < 0 || r < 0 || q >= HXL_N || r >= HXL_N ? -1 : q + r * HXL_N;
}

void hxl_qr(int c, int *q, int *r)
{
    *q = c % HXL_N - HXL_N / 2;
    *r = c / HXL_N - HXL_N / 2;
}

int hxl_nb(int c, int d)
{
    int q, r;
    hxl_qr(c, &q, &r);
    return hxl_cell(q + DQ[d], r + DR[d]);
}

int hxl_reach(int c)
{
    int q, r;
    hxl_qr(c, &q, &r);
    return (abs(q) + abs(r) + abs(q + r)) / 2;
}

uint32_t hxl_turn(uint32_t t, int k)
{
    k = ((k % 6) + 6) % 6;
    uint32_t o = 0;
    for (int d = 0; d < 6; d++) o |= (uint32_t)hxl_edge(t, d) << (3 * ((d + k) % 6));
    return o;
}

int hxl_has(uint32_t t, int terrain)
{
    int n = 0;
    for (int d = 0; d < 6; d++) n += hxl_edge(t, d) == terrain;
    return n;
}

int hxl_frontier(const hxl_state *s, int c)
{
    if (c < 0 || s->cell[c] || hxl_reach(c) > HXL_REACH) return 0;
    for (int d = 0; d < 6; d++) {
        int n = hxl_nb(c, d);
        if (n >= 0 && s->cell[n]) return 1;
    }
    return 0;
}

int hxl_can(const hxl_state *s, int c, uint32_t tile)
{
    if (!hxl_frontier(s, c)) return 0;
    for (int d = 0; d < 6; d++) {
        int n = hxl_nb(c, d);
        if (n < 0 || !s->cell[n]) continue;
        int a = hxl_edge(tile, d), b = hxl_edge(s->cell[n], hxl_opp(d));
        if ((hxl_strict(a) || hxl_strict(b)) && a != b) return 0;
    }
    return 1;
}

int hxl_gain(const hxl_state *s, int c, uint32_t tile, int *matches, int *perfect)
{
    int m = 0, nbs = 0;
    for (int d = 0; d < 6; d++) {
        int n = hxl_nb(c, d);
        if (n < 0 || !s->cell[n]) continue;
        nbs++;
        m += hxl_edge(tile, d) == hxl_edge(s->cell[n], hxl_opp(d));
    }
    int p = nbs == 6 && m == 6;
    if (matches) *matches = m;
    if (perfect) *perfect = p;
    return m * HXL_MATCH + p * HXL_PERFECT;
}

/* the region of t through c: its size, open; mark (may be NULL): its cells set to 1 */
static int region_fill(const hxl_state *s, int c, int t, int *open, uint8_t *mark)
{
    uint8_t seen[HXL_CELLS / 8 + 1];
    int16_t queue[HXL_CELLS];
    int head = 0, tail = 0, op = 0;
    if (open) *open = 0;
    if (c < 0 || !s->cell[c] || !hxl_has(s->cell[c], t)) return 0;
    memset(seen, 0, sizeof seen);
    seen[c >> 3] |= (uint8_t)(1 << (c & 7));
    queue[tail++] = (int16_t)c;
    while (head < tail) {
        int x = queue[head++];
        if (mark) mark[x] = 1;
        for (int d = 0; d < 6; d++) {
            if (hxl_edge(s->cell[x], d) != t) continue;
            int n = hxl_nb(x, d);
            if (n < 0 || !s->cell[n]) {
                if (n >= 0 && hxl_reach(n) <= HXL_REACH) op = 1;
                continue;
            }
            if (hxl_edge(s->cell[n], hxl_opp(d)) != t || (seen[n >> 3] >> (n & 7) & 1)) continue;
            seen[n >> 3] |= (uint8_t)(1 << (n & 7));
            queue[tail++] = (int16_t)n;
        }
    }
    if (open) *open = op;
    return tail;
}

int hxl_region(const hxl_state *s, int c, int t, int *open) { return region_fill(s, c, t, open, NULL); }

const hxl_item *hxl_current(const hxl_state *s) { return s->top < s->ns ? &s->stack[s->top] : NULL; }
int hxl_left(const hxl_state *s) { return s->ns - s->top; }
int hxl_over(const hxl_state *s)
{
    return s->top >= s->ns || (s->nobj > 0 && s->met == (1u << s->nobj) - 1);
}

int hxl_fits(const hxl_state *s, uint32_t tile)
{
    for (int c = 0; c < HXL_CELLS; c++) {
        if (!hxl_frontier(s, c)) continue;
        for (int k = 0; k < 6; k++)
            if (hxl_can(s, c, hxl_turn(tile, k))) return 1;
    }
    return 0;
}

static int bonus(hxl_state *s, int n)          /* n tiles from the reserve onto the stack's end */
{
    int got = 0;
    while (n-- > 0 && s->rtop < s->nr && s->ns < HXL_STACK) {
        s->stack[s->ns++] = s->res[s->rtop++];
        got++;
    }
    return got;
}

static void quests_check(hxl_state *s, hxl_result *res)
{
    for (int i = 0; i < s->nq; i++) {
        hxl_quest *q = &s->q[i];
        if (q->state != HXL_OPEN) continue;
        int open, size = hxl_region(s, q->cell, q->terr, &open);
        q->size = (uint8_t)(size > 255 ? 255 : size);
        int st = HXL_OPEN;
        if (q->kind == HXL_EXACT) {
            if (size > q->n) st = HXL_FAILED;
            else if (!open) st = size == q->n ? HXL_DONE : HXL_FAILED;
        } else {
            if (size >= q->n) st = HXL_DONE;
            else if (!open) st = HXL_FAILED;
        }
        q->state = (uint8_t)st;
        if (st == HXL_DONE) {
            s->score += HXL_QUEST;
            s->done++;
            int b = bonus(s, HXL_QUEST_TILES);
            if (res) {
                res->points += HXL_QUEST;
                res->bonus += b;
                if (res->ndone < 8) res->done[res->ndone++] = i;
            }
        } else if (st == HXL_FAILED) {
            s->failed++;
            if (res && res->nfail < 8) res->fail[res->nfail++] = i;
        }
    }
}

/* forests the tile at c closed: its own, and its neighbours' that met it with forest it did not give back */
static void forests_closed(const hxl_state *s, int c, hxl_result *res)
{
    uint8_t mark[HXL_CELLS];
    memset(mark, 0, sizeof mark);
    int from[7], n = 0;
    from[n++] = c;
    for (int d = 0; d < 6; d++) {
        int x = hxl_nb(c, d);
        if (x >= 0 && s->cell[x] && hxl_edge(s->cell[x], hxl_opp(d)) == HXL_FOREST) from[n++] = x;
    }
    for (int i = 0; i < n; i++) {
        if (mark[from[i]] || !hxl_has(s->cell[from[i]], HXL_FOREST)) continue;
        int open, size = region_fill(s, from[i], HXL_FOREST, &open, mark);
        if (!open && size >= 3 && res->nforest < 8) res->forest[res->nforest++] = from[i];
    }
}

/* ---- objectives ---------------------------------------------------------------------------------------------- */
/* the regions of terrain t: the largest (its size, a cell), the largest of at most n tiles (its size, a cell), and
 * whether one is closed at exactly n */
static void regions_of(const hxl_state *s, int t, int n, int *big, int *bigc, int *upto, int *uptoc, int *exact)
{
    uint8_t mark[HXL_CELLS];
    memset(mark, 0, sizeof mark);
    *big = *upto = *exact = 0;
    *bigc = *uptoc = -1;
    for (int c = 0; c < HXL_CELLS; c++) {
        if (!s->cell[c] || mark[c] || !hxl_has(s->cell[c], t)) continue;
        int open, size = region_fill(s, c, t, &open, mark);
        if (size > *big) { *big = size; *bigc = c; }
        if (size <= n && size > *upto) { *upto = size; *uptoc = c; }
        if (size == n && !open) *exact = 1;
    }
}

int hxl_largest(const hxl_state *s, int t, int *cell)
{
    int big, bigc, upto, uptoc, exact;
    regions_of(s, t, 0, &big, &bigc, &upto, &uptoc, &exact);
    if (cell) *cell = bigc;
    return big;
}

/* objective i now: how far (not clamped), met */
static int obj_now(const hxl_state *s, int i, int *cell, int *met)
{
    const hxl_obj *o = &s->obj[i];
    int big, bigc, upto, uptoc, exact, v = 0;
    if (cell) *cell = -1;
    switch (o->kind) {
    case HXL_OB_REGION:
        regions_of(s, o->terr, o->n, &big, &bigc, &upto, &uptoc, &exact);
        v = big;
        if (cell) *cell = bigc;
        *met = v >= o->n;
        break;
    case HXL_OB_EXACT:
        regions_of(s, o->terr, o->n, &big, &bigc, &upto, &uptoc, &exact);
        v = upto;
        if (cell) *cell = uptoc;
        *met = exact;
        break;
    case HXL_OB_PERFECT: v = s->perfects; *met = v >= o->n; break;
    case HXL_OB_QUESTS: v = s->done; *met = v >= o->n; break;
    default: v = s->score; *met = v >= o->n; break;
    }
    return v;
}

int hxl_obj_progress(const hxl_state *s, int i, int *cell)
{
    if (i < 0 || i >= s->nobj) return 0;
    int met, v = obj_now(s, i, cell, &met);
    if (s->met >> i & 1) return s->obj[i].n;
    return v > s->obj[i].n ? s->obj[i].n : v;
}

/* the objectives the tile at c may have met (c < 0: all looked at; the start): a region only grows or closes
 * through the tile just laid, so only its own region and its neighbours' are looked at */
static void objs_check(hxl_state *s, int c, hxl_result *res)
{
    for (int i = 0; i < s->nobj; i++) {
        if (s->met >> i & 1) continue;
        const hxl_obj *o = &s->obj[i];
        int met = 0;
        if (c < 0 || (o->kind != HXL_OB_REGION && o->kind != HXL_OB_EXACT)) obj_now(s, i, NULL, &met);
        else {
            int from[7], n = 0;
            if (hxl_has(s->cell[c], o->terr)) from[n++] = c;
            for (int d = 0; d < 6; d++) {
                int x = hxl_nb(c, d);
                if (x >= 0 && s->cell[x] && hxl_edge(s->cell[x], hxl_opp(d)) == o->terr) from[n++] = x;
            }
            for (int k = 0; k < n && !met; k++) {
                int open, size = hxl_region(s, from[k], o->terr, &open);
                met = o->kind == HXL_OB_REGION ? size >= o->n : size == o->n && !open;
            }
        }
        if (met) {
            s->met |= 1u << i;
            if (res) res->obj_met |= 1u << i;
        }
    }
}

#define OBJ_W 12                                /* the players' worth of a tile towards an objective */
#define OBJ_MET 120                             /* and of an objective met */
static int blind;                               /* (the levels' tool: a player who does not look at the objectives) */
#ifdef HXL_GEN
/* the objectives' worth of a state: met ones, and how far the others have come */
static int obj_value(const hxl_state *s)
{
    int v = 0;
    for (int i = 0; i < s->nobj; i++) {
        const hxl_obj *o = &s->obj[i];
        if (s->met >> i & 1) { v += OBJ_MET + OBJ_W * (o->kind == HXL_OB_SCORE ? 0 : o->n); continue; }
        if (o->kind == HXL_OB_SCORE) continue;
        int met, p = obj_now(s, i, NULL, &met);
        v += OBJ_W * (p < o->n ? p : o->n) * (o->kind == HXL_OB_PERFECT || o->kind == HXL_OB_QUESTS ? 4 : 1);
    }
    return v;
}
#endif

int hxl_lost(const hxl_state *s) { return s->nobj > 0 && hxl_over(s) && !(s->met & 1); }

int hxl_level_stars(const hxl_state *s, int goal)
{
    if (!s->nobj) return hxl_stars(s->score, goal);
    if (!(s->met & 1)) return 0;
    int m = 0;
    for (int i = 0; i < s->nobj; i++) m += s->met >> i & 1;
    return m == s->nobj ? 3 : 2 * m >= s->nobj ? 2 : 1;
}

int hxl_objs_parse(const char *s, hxl_obj *out, int cap)
{
    int n = 0;
    while (s && *s) {
        if (*s == ' ') { s++; continue; }
        if (n >= cap) return -1;
        hxl_obj o = {HXL_OB_REGION, 0, 0};
        int eq = *s == '=';
        if (eq) { o.kind = HXL_OB_EXACT; s++; }
        if (*s == 'p') o.kind = HXL_OB_PERFECT;
        else if (*s == 'q') o.kind = HXL_OB_QUESTS;
        else if (*s == 's') o.kind = HXL_OB_SCORE;
        else if (!(o.terr = (uint8_t)terrain_of(*s))) return -1;
        if (eq && o.kind != HXL_OB_EXACT) return -1;
        s++;
        int v = 0, d = 0;
        while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0', d++;
        if (!d || v < 1 || v > 60000) return -1;
        o.n = (uint16_t)v;
        out[n++] = o;
    }
    return n;
}

void hxl_objs_text(const hxl_obj *o, int n, char *out, int cap)
{
    int k = 0;
    out[0] = 0;
    for (int i = 0; i < n; i++) {
        char t[16];
        int l = 0;
        if (i) t[l++] = ' ';
        if (o[i].kind == HXL_OB_EXACT) t[l++] = '=';
        t[l++] = o[i].kind == HXL_OB_PERFECT ? 'p' : o[i].kind == HXL_OB_QUESTS ? 'q' : o[i].kind == HXL_OB_SCORE ? 's'
                                                                                                         : LETTER[o[i].terr];
        l += snprintf(t + l, sizeof t - (size_t)l, "%d", o[i].n);
        put(out, &k, cap, t);
    }
}

int hxl_place(hxl_state *s, int c, int k, hxl_result *res)
{
    hxl_result dummy;
    if (!res) res = &dummy;
    memset(res, 0, sizeof *res);
    const hxl_item *it = hxl_current(s);
    if (!it || hxl_over(s) || c < 0 || c >= HXL_CELLS) return 0;
    uint32_t tile = hxl_turn(it->tile, k);
    if (!hxl_can(s, c, tile)) return 0;
    int m, p, pts = hxl_gain(s, c, tile, &m, &p);
    hxl_item item = *it;
    s->cell[c] = tile;
    s->top++;
    s->score += pts;
    s->placed++;
    s->matches += m;
    s->last = c;
    res->points = pts;
    res->matches = m;
    res->perfect = p;
    if (p) {
        s->perfects++;
        res->bonus += bonus(s, HXL_PERFECT_TILES);
    }
    if (item.qn && s->nq < HXL_QUESTS && hxl_has(tile, item.qterr)) {
        hxl_quest *q = &s->q[s->nq++];
        q->cell = (int16_t)c;
        q->terr = item.qterr;
        q->kind = item.qkind;
        q->n = item.qn;
        q->state = HXL_OPEN;
        q->size = 1;
    }
    quests_check(s, res);
    forests_closed(s, c, res);
    objs_check(s, c, res);
    while (!hxl_over(s) && !hxl_fits(s, hxl_current(s)->tile)) {
        s->top++;
        s->skipped++;
        res->skipped++;
    }
    return 1;
}

int hxl_moves(const hxl_state *s, int *out)
{
    const hxl_item *it = hxl_current(s);
    int n = 0;
    if (!it) return 0;
    uint32_t turned[6];
    for (int k = 0; k < 6; k++) turned[k] = hxl_turn(it->tile, k);
    for (int c = 0; c < HXL_CELLS; c++) {
        if (!hxl_frontier(s, c)) continue;
        for (int k = 0; k < 6; k++) {
            int dup = 0;                        /* a tile that looks the same turned: one placement */
            for (int j = 0; j < k && !dup; j++) dup = turned[j] == turned[k];
            if (!dup && hxl_can(s, c, turned[k])) out[n++] = c * 6 + k;
        }
    }
    return n;
}

/* ---- the hint and the greedy player ------------------------------------------------------------------------------ */
static int open_quests(const hxl_state *s)
{
    for (int i = 0; i < s->nq; i++)
        if (s->q[i].state == HXL_OPEN) return 1;
    return 0;
}

/* a placement's worth now: its points, a quest failed costs what it would have given; neighbours as a tie-break
 * (tiles kept together leave holes to fill perfectly); scratch: a state to try it in */
static int worth(const hxl_state *s, int c, int k, hxl_state *scratch, int *nb)
{
    const hxl_item *it = hxl_current(s);
    uint32_t tile = hxl_turn(it->tile, k);
    int n = 0;
    for (int d = 0; d < 6; d++) {
        int x = hxl_nb(c, d);
        n += x >= 0 && s->cell[x];
    }
    *nb = n;
    int goals = !blind && s->nobj > 0 && s->met != (1u << s->nobj) - 1;
    if (!it->qn && !open_quests(s) && !goals) return hxl_gain(s, c, tile, NULL, NULL);
    memcpy(scratch, s, sizeof *s);
    hxl_result r;
    hxl_place(scratch, c, k, &r);
    int v = r.points - r.nfail * (HXL_QUEST + 20) + r.bonus * 20;
    for (int i = 0; goals && i < s->nobj; i++) {   /* the objectives: met now, or the region it grows */
        const hxl_obj *o = &s->obj[i];
        if (s->met >> i & 1) continue;
        if (scratch->met >> i & 1) { v += OBJ_MET; continue; }
        if ((o->kind == HXL_OB_REGION || o->kind == HXL_OB_EXACT) && hxl_has(tile, o->terr)) {
            int sz = hxl_region(scratch, c, o->terr, NULL);
            v += o->kind == HXL_OB_REGION ? OBJ_W * (sz < o->n ? sz : o->n) : sz <= o->n ? OBJ_W * sz : -OBJ_W * 4;
        }
        if (o->kind == HXL_OB_PERFECT) v += 4 * OBJ_W * r.perfect;
        if (o->kind == HXL_OB_QUESTS) v += 4 * OBJ_W * r.ndone;
    }
    return v;
}

/* the best gain of the next tile after the current one is laid as move m (its points only) */
static int next_best(const hxl_state *s, int m, hxl_state *scratch)
{
    if (s->top + 1 >= s->ns) return 0;
    memcpy(scratch, s, sizeof *s);
    hxl_result r;
    if (!hxl_place(scratch, m / 6, m % 6, &r) || !hxl_current(scratch)) return 0;
    uint32_t t = hxl_current(scratch)->tile;
    int best = 0;
    for (int c = 0; c < HXL_CELLS; c++) {
        if (!hxl_frontier(scratch, c)) continue;
        for (int k = 0; k < 6; k++) {
            uint32_t tt = hxl_turn(t, k);
            if (!hxl_can(scratch, c, tt)) continue;
            int g = hxl_gain(scratch, c, tt, NULL, NULL);
            if (g > best) best = g;
        }
    }
    return best;
}

int hxl_hint(const hxl_state *s, int *c, int *k)
{
    static int moves[HXL_MOVES];
    int n = hxl_moves(s, moves);
    if (!n) return 0;
    hxl_state *scratch = malloc(sizeof *scratch);
    if (!scratch) return 0;
    enum { TOP = 10 };
    int top[TOP] = {moves[0]}, val[TOP] = {0}, nt = 0;
    for (int i = 0; i < n; i++) {
        int nb, v = 4 * worth(s, moves[i] / 6, moves[i] % 6, scratch, &nb) + nb, j;
        if (nt < TOP) j = nt++;
        else if (v <= val[TOP - 1]) continue;
        else j = TOP - 1;
        while (j > 0 && val[j - 1] < v) { top[j] = top[j - 1]; val[j] = val[j - 1]; j--; }
        top[j] = moves[i];
        val[j] = v;
    }
    int best = top[0], bv = -1000000000;
    for (int i = 0; i < nt; i++) {
        int v = val[i] + 2 * next_best(s, top[i], scratch);
        if (v > bv) { bv = v; best = top[i]; }
    }
    free(scratch);
    *c = best / 6;
    *k = best % 6;
    return 1;
}

int hxl_greedy(const hxl_state *from)
{
    hxl_state *s = malloc(sizeof *s), *scratch = malloc(sizeof *scratch);
    int *moves = malloc(sizeof(int) * HXL_MOVES), score = from->score;
    if (s && scratch && moves) {
        memcpy(s, from, sizeof *s);
        while (!hxl_over(s)) {
            int n = hxl_moves(s, moves), best = -1, bv = -1000000000;
            for (int i = 0; i < n; i++) {
                int nb, v = 8 * worth(s, moves[i] / 6, moves[i] % 6, scratch, &nb) + nb;
                if (v > bv) { bv = v; best = moves[i]; }
            }
            if (best < 0 || !hxl_place(s, best / 6, best % 6, NULL)) break;
        }
        score = s->score;
    }
    free(s);
    free(scratch);
    free(moves);
    return score;
}

int hxl_goal2(int goal) { return goal * 4 / 5; }

int hxl_stars(int score, int goal)
{
    if (goal <= 0 || score >= goal) return 3;
    return score >= hxl_goal2(goal) ? 2 : 1;
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
int hxl_play(hxl_game *g, int c, int k, hxl_result *res)
{
    static hxl_state was;
    was = g->s;
    if (!hxl_place(&g->s, c, k, res)) return 0;
    g->prev = was;
    g->can_undo = 1;
    return 1;
}

int hxl_undo(hxl_game *g)
{
    if (!g->can_undo) return 0;
    g->s = g->prev;
    g->can_undo = 0;
    return 1;
}

int hxl_script(hxl_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            s++;
            do {
                int c, k;
                if (!hxl_hint(&g->s, &c, &k) || !hxl_play(g, c, k, NULL)) break;
            } while (all && !hxl_over(&g->s));
            continue;
        }
        char *e;
        long q = strtol(s, &e, 10);
        if (e == s || *e != ',') return 0;
        s = e + 1;
        long r = strtol(s, &e, 10);
        if (e == s || *e != ',') return 0;
        s = e + 1;
        long k = strtol(s, &e, 10);
        if (e == s) return 0;
        s = e;
        if (!hxl_play(g, hxl_cell((int)q, (int)r), (int)k, NULL)) return 0;
    }
    return 1;
}

/* ---- text -------------------------------------------------------------------------------------------------------- */
static int terrain_of(char ch)
{
    for (int t = 1; t < HXL_TERRAINS; t++)
        if (LETTER[t] == ch) return t;
    return 0;
}

uint32_t hxl_tile_parse(const char *s)
{
    uint32_t t = 0;
    for (int d = 0; d < 6; d++) {
        int v = terrain_of(s[d]);
        if (!v) return 0;
        t |= (uint32_t)v << (3 * d);
    }
    return t;
}

void hxl_tile_text(uint32_t t, char *out)
{
    for (int d = 0; d < 6; d++) out[d] = LETTER[hxl_edge(t, d)];
    out[6] = 0;
}

/* one item at s: its length (0 bad) */
static int item_parse(const char *s, hxl_item *it)
{
    memset(it, 0, sizeof *it);
    it->tile = hxl_tile_parse(s);
    if (!it->tile) return 0;
    int n = 6;
    if (s[n] == '=' || s[n] == '+') {
        it->qkind = s[n] == '=' ? HXL_EXACT : HXL_ATLEAST;
        it->qterr = (uint8_t)terrain_of(s[n + 1]);
        int v = 0, j = n + 2;
        while (s[j] >= '0' && s[j] <= '9') v = v * 10 + s[j++] - '0';
        if (!it->qterr || v < 1 || v > 255 || j == n + 2) return 0;
        it->qn = (uint8_t)v;
        n = j;
    }
    return n;
}

int hxl_items_parse(const char *s, hxl_item *out, int cap)
{
    int n = 0;
    while (*s) {
        if (*s == ' ') { s++; continue; }
        if (n >= cap) return -1;
        int l = item_parse(s, &out[n]);
        if (!l || (s[l] && s[l] != ' ')) return -1;
        n++;
        s += l;
    }
    return n;
}

static void begin(hxl_game *g)
{
    hxl_state *s = &g->s;
    while (hxl_current(s) && !hxl_fits(s, hxl_current(s)->tile)) {
        s->top++;
        s->skipped++;
    }
    g->prev = g->s;
    g->can_undo = 0;
}

int hxl_load(hxl_game *g, const hxl_level_t *lv)
{
    memset(g, 0, sizeof *g);
    hxl_state *s = &g->s;
    s->last = -1;
    const char *p = lv->start;
    while (*p) {
        if (*p == ' ') { p++; continue; }
        int q = p[0] - 'A' - 12, r = p[1] - 'A' - 12, c = hxl_cell(q, r);
        uint32_t t = c >= 0 && p[1] ? hxl_tile_parse(p + 2) : 0;
        if (!t || s->cell[c]) return 0;
        s->cell[c] = t;
        g->start_tiles++;
        p += 8;
        if (*p && *p != ' ') return 0;
    }
    if (!g->start_tiles) return 0;
    s->ns = hxl_items_parse(lv->stack, s->stack, HXL_STACK);
    s->nr = hxl_items_parse(lv->reserve, s->res, HXL_RES);
    if (s->ns < 1 || s->nr < 0) return 0;
    s->nobj = hxl_objs_parse(lv->goals, s->obj, HXL_OBJS);
    if (s->nobj < 0) return 0;
    objs_check(s, -1, NULL);
    g->goal = lv->goal;
    begin(g);
    return 1;
}

#ifndef HXL_GEN
int hxl_level(hxl_game *g, int level)
{
    if (level < 0 || level >= HXL_LEVELS) return 0;
    return hxl_load(g, &hxl_levels[level]);
}
#endif

static void put(char *o, int *n, int cap, const char *s)
{
    while (*s && *n < cap - 1) o[(*n)++] = *s++;
    o[*n] = 0;
}

static void items_text(const hxl_item *it, int n, char *o, int cap)
{
    int k = 0;
    o[0] = 0;
    for (int i = 0; i < n; i++) {
        char t[24];
        hxl_tile_text(it[i].tile, t);
        if (it[i].qn) {
            int l = 6;
            t[l++] = it[i].qkind == HXL_EXACT ? '=' : '+';
            t[l++] = LETTER[it[i].qterr];
            if (it[i].qn >= 100) t[l++] = (char)('0' + it[i].qn / 100);
            if (it[i].qn >= 10) t[l++] = (char)('0' + it[i].qn / 10 % 10);
            t[l++] = (char)('0' + it[i].qn % 10);
            t[l] = 0;
        }
        if (i) put(o, &k, cap, " ");
        put(o, &k, cap, t);
    }
}

void hxl_text(const hxl_game *g, char *start, char *stack, char *reserve, int cap)
{
    int k = 0;
    start[0] = 0;
    for (int c = 0; c < HXL_CELLS; c++) {
        if (!g->s.cell[c]) continue;
        int q, r;
        hxl_qr(c, &q, &r);
        char t[16];
        t[0] = (char)('A' + 12 + q);
        t[1] = (char)('A' + 12 + r);
        hxl_tile_text(g->s.cell[c], t + 2);
        if (k) put(start, &k, cap, " ");
        put(start, &k, cap, t);
    }
    items_text(g->s.stack + g->s.top, g->s.ns - g->s.top, stack, cap);
    items_text(g->s.res + g->s.rtop, g->s.nr - g->s.rtop, reserve, cap);
}

/* ---- the landscapes' maker --------------------------------------------------------------------------------------- */
typedef struct {
    int n, cell[128];                           /* the disc's cells */
    uint8_t in[HXL_CELLS], base[HXL_CELLS], path[HXL_CELLS];
    uint8_t edge[HXL_CELLS][6];                 /* rivers' and rails' edges */
    uint32_t tile[HXL_CELLS];
} land;

static uint32_t mix(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u + (a << 6) + (a >> 2));
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return h;
}

/* a river or a rail wandering in from the disc's edge */
static void wander(land *L, uint32_t *rng, int radius, int kind)
{
    int border[128], nb = 0;
    for (int i = 0; i < L->n; i++)
        if (hxl_reach(L->cell[i]) == radius && !L->path[L->cell[i]]) border[nb++] = L->cell[i];
    if (!nb) return;
    int c = border[pr_below(rng, nb)], dir = -1;
    for (int d = 0, best = 99; d < 6; d++) {    /* in towards the middle */
        int x = hxl_nb(c, d);
        if (x >= 0 && L->in[x] && hxl_reach(x) < best) { best = hxl_reach(x); dir = d; }
    }
    if (dir < 0) return;
    int out = hxl_opp(dir);                     /* it comes in from outside */
    if (hxl_nb(c, out) >= 0 && !L->in[hxl_nb(c, out)]) L->edge[c][out] = (uint8_t)kind;
    L->path[c] = (uint8_t)kind;
    int len = radius + 1 + pr_below(rng, radius + 2);
    for (int step = 0; step < len; step++) {
        int choice[3] = {dir, (dir + 1) % 6, (dir + 5) % 6}, w[3] = {3, 1, 1}, ok[3], tot = 0;
        for (int i = 0; i < 3; i++) {
            int x = hxl_nb(c, choice[i]);
            ok[i] = x >= 0 && L->in[x] && !L->path[x];
            tot += ok[i] * w[i];
        }
        if (!tot) break;
        int pick = pr_below(rng, tot), i = 0;
        while (!ok[i] || pick >= w[i]) { if (ok[i]) pick -= w[i]; i++; }
        int x = hxl_nb(c, choice[i]);
        L->edge[c][choice[i]] = (uint8_t)kind;
        L->edge[x][hxl_opp(choice[i])] = (uint8_t)kind;
        L->path[x] = (uint8_t)kind;
        dir = choice[i];
        c = x;
        if (hxl_reach(c) == radius && step >= radius - 1) {   /* out again at the far edge */
            int y = hxl_nb(c, dir);
            if (y >= 0 && !L->in[y] && pr_below(rng, 3)) L->edge[c][dir] = (uint8_t)kind;
            break;
        }
    }
}

static void paint_land(land *L, const hxl_spec *sp, int radius, uint32_t seed)
{
    uint32_t rng = pr_seed(seed);
    memset(L, 0, sizeof *L);
    for (int c = 0; c < HXL_CELLS; c++)
        if (hxl_reach(c) <= radius && L->n < 128) {
            L->in[c] = 1;
            L->cell[L->n++] = c;
        }
    int soft[4], ns = 0;
    static const int W[HXL_TERRAINS] = {0, 3, 3, 2, 2, 0, 0};
    for (int t = HXL_MEADOW; t <= HXL_VILLAGE; t++)
        if (sp->terrains >> t & 1) soft[ns++] = t;
    if (!ns) soft[ns++] = HXL_MEADOW;
    int k = L->n / 5 + 2, seedc[40], seedt[40];
    if (k > 40) k = 40;
    for (int i = 0; i < k; i++) {
        seedc[i] = L->cell[pr_below(&rng, L->n)];
        if (i < ns) seedt[i] = soft[i];
        else {
            int tot = 0;
            for (int j = 0; j < ns; j++) tot += W[soft[j]];
            int p = pr_below(&rng, tot), j = 0;
            while (p >= W[soft[j]]) p -= W[soft[j++]];
            seedt[i] = soft[j];
        }
    }
    uint32_t salt = pr_next(&rng);
    for (int i = 0; i < L->n; i++) {
        int c = L->cell[i], best = 0;
        float bd = 1e9f;
        for (int j = 0; j < k; j++) {
            int q0, r0, q1, r1;
            hxl_qr(c, &q0, &r0);
            hxl_qr(seedc[j], &q1, &r1);
            int dq = q0 - q1, dr = r0 - r1;
            float d = (float)(abs(dq) + abs(dr) + abs(dq + dr)) / 2 + (mix(salt + (uint32_t)c, (uint32_t)j) % 1000) / 1100.0f;
            if (d < bd) { bd = d; best = j; }
        }
        L->base[c] = (uint8_t)seedt[best];
    }
    for (int i = 0; i < sp->rivers; i++) wander(L, &rng, radius, HXL_RIVER);
    for (int i = 0; i < sp->rails; i++) wander(L, &rng, radius, HXL_RAIL);
    for (int i = 0; i < L->n; i++) {
        int c = L->cell[i];
        uint32_t t = 0;
        for (int d = 0; d < 6; d++) {
            int x = hxl_nb(c, d), v;
            if (L->edge[c][d]) v = L->edge[c][d];
            else if (x < 0 || !L->in[x] || L->base[x] == L->base[c]) v = L->base[c];
            else {
                int lo = c < x ? c : x, hi = c < x ? x : c;
                v = mix(salt ^ (uint32_t)lo, (uint32_t)hi) & 1 ? L->base[lo] : L->base[hi];
            }
            t |= (uint32_t)v << (3 * d);
        }
        L->tile[c] = t;
    }
}

static int start_cells(int pattern, int *out)
{
    static const int8_t P[6][8][2] = {
        {{0, 0}},
        {{-1, 0}, {0, 0}, {1, 0}},
        {{0, 0}, {1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {0, -1}, {1, -1}},
        {{1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {0, -1}, {1, -1}},
        {{0, 0}, {1, 0}, {0, 1}},
        {{-2, 0}, {-1, 0}, {0, 0}, {1, 0}, {2, 0}},
    };
    static const int N[6] = {1, 3, 7, 6, 3, 5};
    if (pattern < 0 || pattern > 5) pattern = 0;
    for (int i = 0; i < N[pattern]; i++) out[i] = hxl_cell(P[pattern][i][0], P[pattern][i][1]);
    return N[pattern];
}

void hxl_make(hxl_game *g, const hxl_spec *sp, uint32_t seed)
{
    land *L = malloc(sizeof *L), *S = malloc(sizeof *S);
    hxl_state *full = malloc(sizeof *full);
    memset(g, 0, sizeof *g);
    g->s.last = -1;
    if (!L || !S || !full) { free(L); free(S); free(full); return; }
    uint32_t rng = pr_seed(seed ^ 0x51ed27u);
    paint_land(L, sp, sp->radius, pr_next(&rng));
    hxl_state *s = &g->s;
    int st[8], nst = start_cells(sp->start, st);
    for (int i = 0; i < nst; i++)
        if (st[i] >= 0 && L->in[st[i]]) { s->cell[st[i]] = L->tile[st[i]]; g->start_tiles++; }
    memset(full, 0, sizeof *full);
    for (int i = 0; i < L->n; i++) full->cell[L->cell[i]] = L->tile[L->cell[i]];
    /* the stack: the other cells, shuffled, turned */
    int order[128], n = 0;
    for (int i = 0; i < L->n; i++)
        if (!s->cell[L->cell[i]]) order[n++] = L->cell[i];
    pr_shuffle(&rng, order, n);
    paint_land(S, sp, sp->radius, pr_next(&rng));
    for (int i = 0; i < n && s->ns < HXL_STACK; i++) {
        hxl_item *it = &s->stack[s->ns++];
        uint32_t t = L->tile[order[i]];
        if (i > 0 && (int)pr_below(&rng, 100) < sp->strays) t = S->tile[S->cell[pr_below(&rng, S->n)]];
        it->tile = hxl_turn(t, pr_below(&rng, 6));
    }
    /* quests: on tiles of the landscape (not strays), their regions as the landscape has them */
    int tries = 0, made = 0;
    while (made < sp->quests && tries++ < 400 && n > 2) {
        int i = 1 + pr_below(&rng, n - 1);
        hxl_item *it = &s->stack[i];
        if (it->qn) continue;
        int c = order[i], found = 0;
        for (int k = 0; k < 6 && !found; k++)
            found = hxl_turn(L->tile[c], k) == it->tile;
        if (!found) continue;                   /* a stray */
        int t = hxl_edge(L->tile[c], pr_below(&rng, 6));
        if (t == HXL_RAIL || (t == HXL_RIVER && !sp->river_quests) || t == HXL_MEADOW) continue;
        int open, size = hxl_region(full, c, t, &open);
        if (size < 3 || size > 30) continue;
        int exact = t != HXL_RIVER && (int)pr_below(&rng, 100) < sp->exact;
        it->qterr = (uint8_t)t;
        it->qkind = (uint8_t)(exact ? HXL_EXACT : HXL_ATLEAST);
        it->qn = (uint8_t)(exact ? size : size - (int)pr_below(&rng, size / 4 + 1));
        if (it->qn < 3) it->qn = 3;
        made++;
    }
    /* the reserve: a landscape of its own */
    paint_land(S, sp, sp->radius < 4 ? 4 : sp->radius, pr_next(&rng));
    int ro[128];
    for (int i = 0; i < S->n; i++) ro[i] = S->cell[i];
    pr_shuffle(&rng, ro, S->n);
    for (int i = 0; i < sp->reserve && i < S->n && s->nr < HXL_RES; i++) {
        s->res[s->nr].tile = hxl_turn(S->tile[ro[i]], pr_below(&rng, 6));
        s->nr++;
    }
    free(L);
    free(S);
    free(full);
    begin(g);
}

void hxl_variant(int variant, hxl_spec *sp)
{
    static const hxl_spec V[HXL_VARIANTS] = {
        {3, 0x1e, 1, 0, 0, 10, 3, 40, 40, 1},
        {4, 0x1e, 1, 1, 0, 10, 5, 40, 60, 1},
        {5, 0x1e, 2, 1, 0, 10, 8, 40, 80, 1},
    };
    *sp = V[variant < 0 || variant >= HXL_VARIANTS ? 0 : variant];
}

void hxl_random(hxl_game *g, int variant, uint32_t seed)
{
    hxl_spec sp;
    hxl_variant(variant, &sp);
    hxl_make(g, &sp, seed);
    int goal = hxl_greedy(&g->s) * 95 / 100 / 10 * 10;
    g->goal = goal < 10 ? 10 : goal;
}

#ifndef HXL_GEN
#include "games/hexland_levels.h"
#endif
