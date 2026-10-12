/*
 * Merge: the rules (merge.h): the pushes and the merges, the new tiles from the game's own seeded numbers, undo, the
 * end, the hint (a short look ahead), the notation. No SDL: tested by tests/merge_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/merge.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

void mrg_variant(int variant, int *n, int *target)
{
    static const int V[MRG_VARIANTS][2] = {{4, 11}, {5, 11}, {3, 8}};
    if (variant < 0 || variant >= MRG_VARIANTS) variant = 0;
    *n = V[variant][0];
    *target = V[variant][1];
}

/* the places of line `k` for a push `dir`, from the side the tiles go to */
static void line_of(int n, int dir, int k, int *idx)
{
    for (int i = 0; i < n; i++) {
        int x, y;
        if (dir == MRG_LEFT) x = i, y = k;
        else if (dir == MRG_RIGHT) x = n - 1 - i, y = k;
        else if (dir == MRG_UP) x = k, y = i;
        else x = k, y = n - 1 - i;
        idx[i] = x + y * n;
    }
}

int mrg_slide(const uint8_t *in, int n, int dir, uint8_t *out, int *gain, mrg_push *how)
{
    uint8_t res[MRG_CELLS] = {0}, merged[MRG_CELLS] = {0};
    int moved = 0, points = 0;
    if (how) {
        memset(how, 0, sizeof *how);
        for (int i = 0; i < MRG_CELLS; i++) how->to[i] = -1;
        how->spawn = -1;
    }
    for (int k = 0; k < n; k++) {
        int idx[MRG_MAX], last = -1, put = 0;
        line_of(n, dir, k, idx);
        for (int i = 0; i < n; i++) {
            int v = in[idx[i]];
            if (!v) continue;
            int dst;
            if (last >= 0 && res[idx[last]] == v && !merged[idx[last]] && v < MRG_TOP) {
                dst = idx[last];
                res[dst] = (uint8_t)(v + 1);
                merged[dst] = 1;
                points += 1 << (v + 1);
                moved = 1;
            } else {
                dst = idx[put];
                res[dst] = (uint8_t)v;
                last = put++;
                if (dst != idx[i]) moved = 1;
            }
            if (how) how->to[idx[i]] = (int8_t)dst;
        }
    }
    if (how) {
        memcpy(how->merged, merged, sizeof merged);
        how->gain = points;
    }
    if (out) memcpy(out, res, (size_t)n * n);
    if (gain) *gain = points;
    return moved;
}

static void spawn(int n, uint8_t *c, uint32_t *rng, int *at)
{
    int empty[MRG_CELLS], k = 0;
    for (int i = 0; i < n * n; i++)
        if (!c[i]) empty[k++] = i;
    *at = -1;
    if (!k) return;
    int p = empty[pr_below(rng, k)];
    c[p] = pr_below(rng, 10) < 9 ? 1 : 2;
    *at = p;
}

void mrg_new(mrg_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= MRG_VARIANTS) variant = 0;
    memset(g, 0, sizeof *g);
    g->variant = variant;
    mrg_variant(variant, &g->n, &g->target);
    g->p.rng = pr_seed(seed);
    int at;
    spawn(g->n, g->p.c, &g->p.rng, &at);
    spawn(g->n, g->p.c, &g->p.rng, &at);
}

int mrg_can(const mrg_game *g, int dir)
{
    if (dir < 0 || dir > 3) return 0;
    return mrg_slide(g->p.c, g->n, dir, NULL, NULL, NULL);
}

int mrg_any(const mrg_game *g)
{
    for (int d = 0; d < 4; d++)
        if (mrg_can(g, d)) return 1;
    return 0;
}

int mrg_move(mrg_game *g, int dir, mrg_push *how)
{
    mrg_push local;
    if (!how) how = &local;
    if (dir < 0 || dir > 3) return 0;
    uint8_t out[MRG_CELLS] = {0};
    int gain;
    if (!mrg_slide(g->p.c, g->n, dir, out, &gain, how)) return 0;
    if (g->nh >= MRG_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + MRG_HIST / 2, sizeof g->hist[0] * (MRG_HIST / 2));
        g->nh -= MRG_HIST / 2;
    }
    g->hist[g->nh++] = g->p;
    memcpy(g->p.c, out, sizeof out);
    g->p.score += (uint32_t)gain;
    spawn(g->n, g->p.c, &g->p.rng, &how->spawn);
    g->moves++;
    return 1;
}

int mrg_undo(mrg_game *g)
{
    if (!g->nh) return 0;
    g->p = g->hist[--g->nh];
    g->moves--;
    return 1;
}

static int top_of(const uint8_t *c, int n)
{
    int t = 0;
    for (int i = 0; i < n * n; i++)
        if (c[i] > t) t = c[i];
    return t;
}

int mrg_top(const mrg_game *g) { return top_of(g->p.c, g->n); }
int mrg_reached(const mrg_game *g) { return mrg_top(g) >= g->target; }
int mrg_over(const mrg_game *g) { return !mrg_any(g); }

int mrg_just_reached(const mrg_game *g)
{
    return mrg_reached(g) && g->nh > 0 && top_of(g->hist[g->nh - 1].c, g->n) < g->target;
}

/* ---- the hint: a push, then each new tile the chance may bring, then the best push after it -------------------- */
/* how good a grid looks: room, rows and columns in order (rising or falling), neighbours close, a large top tile */
static float judge(const uint8_t *c, int n)
{
    float empty = 0, mono = 0, smooth = 0, top = 0;
    for (int i = 0; i < n * n; i++) {
        if (!c[i]) empty++;
        if (c[i] > top) top = c[i];
    }
    for (int k = 0; k < n; k++)
        for (int axis = 0; axis < 2; axis++) {
            float up = 0, down = 0;
            for (int i = 0; i + 1 < n; i++) {
                int a = axis ? c[k + i * n] : c[i + k * n], b = axis ? c[k + (i + 1) * n] : c[i + 1 + k * n];
                if (a > b) down += a - b;
                else up += b - a;
                if (a && b) smooth -= (float)abs(a - b);
            }
            mono -= up < down ? up : down;
        }
    /* the top tile in a corner */
    int corner = c[0] == top || c[n - 1] == top || c[n * (n - 1)] == top || c[n * n - 1] == top;
    return 2.7f * empty + 1.0f * mono + 0.1f * smooth + 1.5f * corner * top;
}

static float best_after(const uint8_t *c, int n)
{
    float best = -1e9f;
    int any = 0;
    for (int d = 0; d < 4; d++) {
        uint8_t out[MRG_CELLS];
        if (!mrg_slide(c, n, d, out, NULL, NULL)) continue;
        any = 1;
        float v = judge(out, n);
        if (v > best) best = v;
    }
    return any ? best : -1000;
}

int mrg_hint(const mrg_game *g)
{
    int n = g->n, pick = -1;
    float best = -1e30f;
    for (int d = 0; d < 4; d++) {
        uint8_t out[MRG_CELLS];
        int gain;
        if (!mrg_slide(g->p.c, n, d, out, &gain, NULL)) continue;
        float sum = 0, weight = 0;
        for (int i = 0; i < n * n; i++) {
            if (out[i]) continue;
            for (int v = 1; v <= 2; v++) {
                float pv = v == 1 ? 0.9f : 0.1f;
                out[i] = (uint8_t)v;
                sum += pv * best_after(out, n);
                weight += pv;
                out[i] = 0;
            }
        }
        float score = (weight > 0 ? sum / weight : judge(out, n)) + 0.002f * gain;
        if (pick < 0 || score > best) { best = score; pick = d; }
    }
    return pick;
}

int mrg_script(mrg_game *g, const char *s)
{
    while (*s) {
        char c = *s++;
        if (c == ' ' || c == ',') continue;
        if (c == '=') {
            int k = 0;
            for (; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int i = 0; i < k; i++) {
                int d = mrg_hint(g);
                if (d < 0) break;
                mrg_move(g, d, NULL);
            }
            continue;
        }
        if (c == '#') {                         /* the grid set: n * n powers */
            uint8_t cells[MRG_CELLS] = {0};
            for (int i = 0; i < g->n * g->n; i++) {
                while (*s == ' ' || *s == ',') s++;
                if (*s < '0' || *s > '9') return 0;
                int v = 0;
                for (; *s >= '0' && *s <= '9'; s++) v = v * 10 + *s - '0';
                if (v > MRG_TOP) return 0;
                cells[i] = (uint8_t)v;
            }
            memcpy(g->p.c, cells, sizeof cells);
            g->p.score = 0;
            g->nh = g->moves = 0;
            continue;
        }
        const char *dirs = "urdl", *p = strchr(dirs, c | 32);
        if (!p || !c || !mrg_move(g, (int)(p - dirs), NULL)) return 0;
    }
    return 1;
}
