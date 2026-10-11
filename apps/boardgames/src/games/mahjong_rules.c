/*
 * Mahjong Solitaire: the rules (mahjong.h). No SDL: tested by tests/mahjong_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/mahjong.h"

#include <stdlib.h>
#include <string.h>

#define TRIES 400                               /* deals tried by chance before giving up */

/* ---- the layouts ------------------------------------------------------------------------------------------------- */
static int add(mjpos *p, int n, int x, int y, int z)
{
    p[n] = (mjpos){(int8_t)x, (int8_t)y, (int8_t)z};
    return n + 1;
}

int mj_layout(int variant, mjpos *p)
{
    int n = 0;
    if (variant == 1) {                         /* the Pyramid */
        for (int z = 0; z < 3; z++)             /* 8 x 6, 6 x 4, 4 x 2 */
            for (int r = z; r < 6 - z; r++)
                for (int c = z; c < 8 - z; c++) n = add(p, n, 2 * c, 2 * r, z);
        return n;
    }
    /* the Turtle: its shell (12 wide, 8 rows), its head and tail half a row down, and four layers up to the top */
    static const int8_t ROWS[8][2] = {{1, 12}, {3, 10}, {2, 11}, {1, 12}, {1, 12}, {2, 11}, {3, 10}, {1, 12}};
    for (int r = 0; r < 8; r++)
        for (int c = ROWS[r][0]; c <= ROWS[r][1]; c++) n = add(p, n, 2 * c, 2 * r, 0);
    n = add(p, n, 0, 7, 0);
    n = add(p, n, 26, 7, 0);
    n = add(p, n, 28, 7, 0);
    for (int z = 1; z <= 3; z++)                /* 6 x 6, 4 x 4, 2 x 2 */
        for (int r = z; r <= 7 - z; r++)
            for (int c = 3 + z; c <= 10 - z; c++) n = add(p, n, 2 * c, 2 * r, z);
    return add(p, n, 13, 7, 4);
}

int mj_match(int a, int b)
{
    if (a >= MJ_FACES || b >= MJ_FACES) return 0;
    if (a >= MJ_SEASONS) return b >= MJ_SEASONS;
    if (a >= MJ_FLOWERS) return b >= MJ_FLOWERS && b < MJ_SEASONS;
    return a == b;
}

static void neighbours(mjgame *g)
{
    memset(g->above, -1, sizeof g->above);
    memset(g->left, -1, sizeof g->left);
    memset(g->right, -1, sizeof g->right);
    for (int i = 0; i < g->n; i++) {
        int na = 0, nl = 0, nr = 0;
        mjpos a = g->pos[i];
        for (int j = 0; j < g->n; j++) {
            mjpos b = g->pos[j];
            if (abs(b.y - a.y) >= 2) continue;
            if (b.z == a.z + 1 && abs(b.x - a.x) < 2 && na < 4) g->above[i][na++] = (int16_t)j;
            if (b.z == a.z && b.x == a.x - 2 && nl < 2) g->left[i][nl++] = (int16_t)j;
            if (b.z == a.z && b.x == a.x + 2 && nr < 2) g->right[i][nr++] = (int16_t)j;
        }
    }
}

/* ---- free places ------------------------------------------------------------------------------------------------- */
static int open_side(const int16_t *side, const uint8_t *on)
{
    for (int k = 0; k < 2; k++)
        if (side[k] >= 0 && on[side[k]]) return 0;
    return 1;
}

/* free in the set `on` (1: a tile in that place) */
static int free_in(const mjgame *g, const uint8_t *on, int i)
{
    if (!on[i]) return 0;
    for (int k = 0; k < 4; k++)
        if (g->above[i][k] >= 0 && on[g->above[i][k]]) return 0;
    return open_side(g->left[i], on) || open_side(g->right[i], on);
}

static void occupied(const mjgame *g, uint8_t *on)
{
    for (int i = 0; i < g->n; i++) on[i] = g->face[i] != MJ_NONE;
}

int mj_free(const mjgame *g, int i)
{
    if (i < 0 || i >= g->n) return 0;
    uint8_t on[MJ_MAX];
    occupied(g, on);
    return free_in(g, on, i);
}

/* an order taking away the places in `on` two free ones at a time (out: 2k, 2k + 1 each step), by chance: 1, or 0
 * when it stuck (fewer than two free places, tiles left) */
static int take_order(const mjgame *g, const uint8_t *start, int (*rnd)(int), uint8_t *out)
{
    uint8_t on[MJ_MAX];
    memcpy(on, start, sizeof on);
    int left = 0, k = 0;
    for (int i = 0; i < g->n; i++) left += on[i];
    while (left > 0) {
        int fr[MJ_MAX], nf = 0;
        for (int i = 0; i < g->n; i++)
            if (free_in(g, on, i)) fr[nf++] = i;
        if (nf < 2) return 0;
        int a = rnd(nf), b = rnd(nf - 1);
        if (b >= a) b++;
        out[k++] = (uint8_t)fr[a];
        out[k++] = (uint8_t)fr[b];
        on[fr[a]] = on[fr[b]] = 0;
        left -= 2;
    }
    return 1;
}

static void shuffle_pairs(uint8_t (*pairs)[2], int n, int (*rnd)(int))
{
    for (int i = n - 1; i > 0; i--) {
        int j = rnd(i + 1);
        uint8_t t0 = pairs[i][0], t1 = pairs[i][1];
        pairs[i][0] = pairs[j][0];
        pairs[i][1] = pairs[j][1];
        pairs[j][0] = t0;
        pairs[j][1] = t1;
    }
}

/* the faces in the places `on`, the pairs in `pairs` (as many as there are places / 2), dealt backwards: 1, or 0 */
static int deal_into(mjgame *g, const uint8_t *on, uint8_t (*pairs)[2], int npairs, int (*rnd)(int))
{
    uint8_t order[MJ_MAX];
    for (int t = 0; t < TRIES; t++) {
        if (!take_order(g, on, rnd, order)) continue;
        shuffle_pairs(pairs, npairs, rnd);
        for (int k = 0; k < npairs; k++) {
            int swap = rnd(2);
            g->face[order[2 * k]] = pairs[k][swap];
            g->face[order[2 * k + 1]] = pairs[k][!swap];
        }
        memcpy(g->sol, order, (size_t)2 * npairs);
        g->nsol = 2 * npairs;
        return 1;
    }
    return 0;
}

/* a cheap source of chance of its own (the dead end test must not use the game's) */
static uint32_t own_rng;
static int own(int n)
{
    own_rng = own_rng * 1664525u + 1013904223u;
    return n > 0 ? (int)((own_rng >> 8) % (uint32_t)n) : 0;
}

static void refresh(mjgame *g)                  /* after a move: is it a dead end? */
{
    g->dead = 0;
    if (!g->tiles || mj_pairs(g)) return;
    uint8_t on[MJ_MAX], order[MJ_MAX];
    occupied(g, on);
    own_rng = 12345u + (uint32_t)g->tiles;
    for (int t = 0; t < TRIES; t++)
        if (take_order(g, on, own, order)) return;
    g->dead = 1;
}

int mj_deal(mjgame *g, int variant, int (*rnd)(int))
{
    memset(g, 0, sizeof *g);
    g->variant = variant < 0 || variant >= MJ_VARIANTS ? 0 : variant;
    g->n = mj_layout(g->variant, g->pos);
    neighbours(g);
    uint8_t pairs[MJ_MAX / 2][2], fl[4], se[4];
    int np = 0;
    for (int f = 0; f < MJ_FLOWERS; f++)        /* each face four times: two pairs */
        for (int k = 0; k < 2; k++) pairs[np][0] = pairs[np][1] = (uint8_t)f, np++;
    for (int i = 0; i < 4; i++) fl[i] = (uint8_t)(MJ_FLOWERS + i), se[i] = (uint8_t)(MJ_SEASONS + i);
    for (int i = 3; i > 0; i--) {               /* the flowers, the seasons: any two make a pair */
        int j = rnd(i + 1);
        uint8_t t = fl[i]; fl[i] = fl[j]; fl[j] = t;
        j = rnd(i + 1);
        t = se[i]; se[i] = se[j]; se[j] = t;
    }
    for (int k = 0; k < 2; k++) {
        pairs[np][0] = fl[2 * k], pairs[np][1] = fl[2 * k + 1], np++;
        pairs[np][0] = se[2 * k], pairs[np][1] = se[2 * k + 1], np++;
    }
    shuffle_pairs(pairs, np, rnd);              /* a smaller layout: the first pairs of the set, by chance */
    uint8_t on[MJ_MAX];
    memset(on, 1, sizeof on);
    memset(g->face, MJ_NONE, sizeof g->face);
    g->tiles = g->n;
    if (!deal_into(g, on, pairs, g->n / 2, rnd)) return 0;
    refresh(g);
    return 1;
}

int mj_can_take(const mjgame *g, int a, int b)
{
    if (a < 0 || b < 0 || a >= g->n || b >= g->n || a == b) return 0;
    return mj_free(g, a) && mj_free(g, b) && mj_match(g->face[a], g->face[b]);
}

static void push(mjgame *g)
{
    if (g->nh == MJ_HIST) {                     /* full: the oldest move can no longer be undone */
        memmove(g->hist[0], g->hist[1], (size_t)(MJ_HIST - 1) * MJ_MAX);
        g->nh--;
    }
    memcpy(g->hist[g->nh++], g->face, MJ_MAX);
}

int mj_take(mjgame *g, int a, int b)
{
    if (!mj_can_take(g, a, b)) return 0;
    push(g);
    g->face[a] = g->face[b] = MJ_NONE;
    g->tiles -= 2;
    refresh(g);
    return 1;
}

int mj_pairs(const mjgame *g)
{
    uint8_t on[MJ_MAX];
    int fr[MJ_MAX], nf = 0, n = 0;
    occupied(g, on);
    for (int i = 0; i < g->n; i++)
        if (free_in(g, on, i)) fr[nf++] = i;
    for (int i = 0; i < nf; i++)
        for (int j = i + 1; j < nf; j++) n += mj_match(g->face[fr[i]], g->face[fr[j]]);
    return n;
}

int mj_hint(const mjgame *g, int *a, int *b)
{
    uint8_t on[MJ_MAX];
    int fr[MJ_MAX], nf = 0;
    occupied(g, on);
    for (int i = 0; i < g->n; i++)
        if (free_in(g, on, i)) fr[nf++] = i;
    for (int i = 0; i < nf; i++)
        for (int j = i + 1; j < nf; j++)
            if (mj_match(g->face[fr[i]], g->face[fr[j]])) {
                *a = fr[i];
                *b = fr[j];
                return 1;
            }
    return 0;
}

int mj_shuffle(mjgame *g, int (*rnd)(int))
{
    if (!g->tiles || g->dead) return 0;
    uint8_t on[MJ_MAX], pairs[MJ_MAX / 2][2], spare[3] = {MJ_NONE, MJ_NONE, MJ_NONE};
    int np = 0, count[MJ_FACES] = {0};
    occupied(g, on);
    for (int i = 0; i < g->n; i++)
        if (on[i]) count[g->face[i]]++;
    for (int f = 0; f < MJ_FACES; f++)          /* the tiles left in pairs: alike, or flowers, or seasons */
        for (int k = 0; k < count[f]; k++) {
            int kind = f >= MJ_SEASONS ? 2 : f >= MJ_FLOWERS ? 1 : 0;
            if (!kind) {
                if (k & 1) pairs[np][0] = pairs[np][1] = (uint8_t)f, np++;
            } else if (spare[kind] == MJ_NONE) {
                spare[kind] = (uint8_t)f;
            } else {
                pairs[np][0] = spare[kind], pairs[np][1] = (uint8_t)f, np++;
                spare[kind] = MJ_NONE;
            }
        }
    if (2 * np != g->tiles) return 0;
    uint8_t keep[MJ_MAX], keep_sol[MJ_MAX];
    int keep_n = g->nsol;
    memcpy(keep, g->face, sizeof keep);
    memcpy(keep_sol, g->sol, sizeof keep_sol);
    if (!deal_into(g, on, pairs, np, rnd)) {
        memcpy(g->face, keep, sizeof keep);
        memcpy(g->sol, keep_sol, sizeof keep_sol);
        g->nsol = keep_n;
        return 0;
    }
    uint8_t now[MJ_MAX];                        /* the history keeps the faces before the shuffle */
    memcpy(now, g->face, sizeof now);
    memcpy(g->face, keep, sizeof keep);
    push(g);
    memcpy(g->face, now, sizeof now);
    refresh(g);
    return 1;
}

int mj_undo(mjgame *g)
{
    if (!g->nh) return 0;
    memcpy(g->face, g->hist[--g->nh], MJ_MAX);
    g->tiles = 0;
    for (int i = 0; i < g->n; i++) g->tiles += g->face[i] != MJ_NONE;
    refresh(g);
    return 1;
}

int mj_status(const mjgame *g)
{
    if (!g->tiles) return MJ_WON;
    if (mj_pairs(g)) return MJ_PLAY;
    return g->dead ? MJ_LOST : MJ_STUCK;
}

int mj_script(mjgame *g, const char *s, int (*rnd)(int))
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        char *end;
        if (*s == 's') {
            if (!mj_shuffle(g, rnd)) return 0;
            s++;
        } else if (*s == '=' || *s == '+') {
            int plus = *s == '+', all = plus && s[1] == '*';   /* "+*": while there is a pair */
            long n = strtol(s + 1, &end, 10);
            if (all) n = MJ_MAX, end = (char *)s + 2;
            else if (end == s + 1) n = 1;
            for (long k = 0; k < n; k++) {
                int a = -1, b = -1;
                if (plus) {
                    if (!mj_hint(g, &a, &b)) {
                        if (all) break;
                        return 0;
                    }
                } else {
                    for (int i = 0; i + 1 < g->nsol && a < 0; i += 2)
                        if (g->face[g->sol[i]] != MJ_NONE && g->face[g->sol[i + 1]] != MJ_NONE)
                            a = g->sol[i], b = g->sol[i + 1];
                }
                if (!mj_take(g, a, b)) return 0;
            }
            s = end;
        } else {
            long a = strtol(s, &end, 10);
            if (end == s || *end != '-') return 0;
            s = end + 1;
            long b = strtol(s, &end, 10);
            if (end == s || !mj_take(g, (int)a, (int)b)) return 0;
            s = end;
        }
    }
    return 1;
}
