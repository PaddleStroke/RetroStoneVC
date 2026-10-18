/*
 * Colour Mixer for one player: the rules (colourmix.h). No SDL: the tests and the levels' tool
 * (tools/colourmix_gen.c) use them.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/colourmix.h"
#include "games/puzrand.h"

/* ---- the paints ------------------------------------------------------------------------------------------------- */
/* what a drop brings: red, yellow, blue, white, black */
static const float MIX[CMX_PAINTS][CMX_BASE] = {
    {1, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1},
    {0.5f, 0.5f, 0, 0, 0},                      /* orange */
    {0, 0.5f, 0.5f, 0, 0},                      /* green */
    {0.5f, 0, 0.5f, 0, 0},                      /* violet */
    {0.4f, 0.3f, 0.15f, 0, 0.15f},              /* brown */
    {0.35f, 0, 0, 0.65f, 0},                    /* pink */
};
static const char LETTER[CMX_PAINTS + 1] = "rybwkogvnp";

/* the painters' cube: the colour of each mixture of the primaries at full strength (corner r + 2 y + 4 b) */
static const float CUBE[8][3] = {
    {1, 1, 1},             {0.89f, 0.13f, 0.11f}, {0.99f, 0.85f, 0.10f}, {0.98f, 0.52f, 0.06f},
    {0.10f, 0.32f, 0.78f}, {0.47f, 0.15f, 0.55f}, {0.13f, 0.60f, 0.26f}, {0.26f, 0.17f, 0.11f},
};
static const float BLACK[3] = {0.11f, 0.105f, 0.10f};

void cmx_paint_mix(int p, float a[CMX_BASE])
{
    for (int i = 0; i < CMX_BASE; i++) a[i] = p >= 0 && p < CMX_PAINTS ? MIX[p][i] : 0;
}

int cmx_colour(const float a[CMX_BASE], float rgb[3])
{
    float C = a[0] + a[1] + a[2], W = a[3], K = a[4], T = C + W + K;
    if (T <= 1e-6f) { rgb[0] = rgb[1] = rgb[2] = 1; return 0; }
    float hue[3] = {1, 1, 1}, mix[3];
    if (C > 1e-6f) {
        float m = fmaxf(a[0], fmaxf(a[1], a[2])), r = a[0] / m, y = a[1] / m, b = a[2] / m;
        for (int k = 0; k < 3; k++) hue[k] = 0;
        for (int c = 0; c < 8; c++) {
            float w = (c & 1 ? r : 1 - r) * (c & 2 ? y : 1 - y) * (c & 4 ? b : 1 - b);
            for (int k = 0; k < 3; k++) hue[k] += w * CUBE[c][k];
        }
    }
    for (int k = 0; k < 3; k++) mix[k] = C + W > 1e-6f ? (C * hue[k] + W) / (C + W) : 1;
    float f = K > 0 ? powf((C + W) / T, 1.4f) : 1;
    for (int k = 0; k < 3; k++) rgb[k] = BLACK[k] + (mix[k] - BLACK[k]) * f;
    return 1;
}

void cmx_paint_rgb(int p, float rgb[3])
{
    float a[CMX_BASE];
    cmx_paint_mix(p, a);
    cmx_colour(a, rgb);
}

static float lin(float c) { return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f); }
static float lab_f(float t) { return t > 0.008856f ? cbrtf(t) : 7.787f * t + 16.0f / 116; }

static void to_lab(const float c[3], float *L, float *A, float *B)
{
    float r = lin(c[0]), g = lin(c[1]), b = lin(c[2]);
    float X = (0.4124f * r + 0.3576f * g + 0.1805f * b) / 0.95047f, Y = 0.2126f * r + 0.7152f * g + 0.0722f * b,
          Z = (0.0193f * r + 0.1192f * g + 0.9505f * b) / 1.08883f;
    float fx = lab_f(X), fy = lab_f(Y), fz = lab_f(Z);
    *L = 116 * fy - 16;
    *A = 500 * (fx - fy);
    *B = 200 * (fy - fz);
}

float cmx_de(const float a[3], const float b[3])
{
    float l1, a1, b1, l2, a2, b2;
    to_lab(a, &l1, &a1, &b1);
    to_lab(b, &l2, &a2, &b2);
    return sqrtf((l1 - l2) * (l1 - l2) + (a1 - a2) * (a1 - a2) + (b1 - b2) * (b1 - b2));
}

int cmx_pot(const cmx_map *m, const uint8_t *add, float rgb[3])
{
    float a[CMX_BASE] = {0};
    for (int t = 0; t < m->n; t++) {
        int d = m->start[t] + (add ? add[t] : 0);
        for (int i = 0; i < CMX_BASE && d; i++) a[i] += d * MIX[m->paint[t]][i];
    }
    return cmx_colour(a, rgb);
}

float cmx_dist(const cmx_map *m, const uint8_t *add)
{
    float c[3];
    if (!cmx_pot(m, add, c)) return 1000;
    return cmx_de(c, m->target);
}

int cmx_solved(const cmx_map *m, const cmx_state *s) { return cmx_dist(m, s->add) <= CMX_TOL; }

int cmx_left(const cmx_map *m, const cmx_state *s, int t)
{
    if (m->limit[t] == CMX_ANY) return CMX_ANY;
    return m->limit[t] > s->add[t] ? m->limit[t] - s->add[t] : 0;
}

int cmx_drop(cmx_game *g, int t)
{
    if (t < 0 || t >= g->m.n || !cmx_left(&g->m, &g->s, t) || g->s.n >= CMX_HIST || g->s.add[t] >= 99) return 0;
    g->s.add[t]++;
    g->s.order[g->s.n++] = (uint8_t)t;
    return 1;
}

int cmx_undo(cmx_game *g)
{
    if (!g->s.n) return 0;
    g->s.add[g->s.order[--g->s.n]]--;
    return 1;
}

int cmx_take(cmx_game *g, int t)
{
    for (int i = g->s.n - 1; i >= 0; i--)
        if (g->s.order[i] == t) {
            memmove(g->s.order + i, g->s.order + i + 1, (size_t)(g->s.n - i - 1));
            g->s.n--;
            g->s.add[t]--;
            return 1;
        }
    return 0;
}

int cmx_count(const cmx_state *s) { return s->n; }

/* ---- the search: every set of drops, the fewest first ------------------------------------------------------------ */
typedef struct {
    const cmx_map *m;
    float base[CMX_BASE], per[CMX_TUBES][CMX_BASE], L, A, B, tol;
    uint8_t room[CMX_TUBES], cur[CMX_TUBES], found[CMX_TUBES];
    int hit, count, drops, skip_exact;
} cmx_search;

static float search_de(cmx_search *S)
{
    float a[CMX_BASE], c[3], l, x, y;
    for (int i = 0; i < CMX_BASE; i++) {
        a[i] = S->base[i];
        for (int t = 0; t < S->m->n; t++) a[i] += S->cur[t] * S->per[t][i];
    }
    if (!cmx_colour(a, c)) return 1000;
    to_lab(c, &l, &x, &y);
    return sqrtf((l - S->L) * (l - S->L) + (x - S->A) * (x - S->A) + (y - S->B) * (y - S->B));
}

/* the drops left to share out (left) among tubes t.. ; the first set within tol found (or each counted) */
static void search_rec(cmx_search *S, int t, int left)
{
    if (S->hit && !S->drops) return;
    if (t == S->m->n - 1 || left == 0) {
        if (left > S->room[t]) return;
        S->cur[t] = (uint8_t)left;
        float de = search_de(S);
        if (de <= S->tol) {
            if (S->drops) {
                if (!S->skip_exact || de > 0.05f) S->count++;
            } else if (!S->hit) {
                S->hit = 1;
                memcpy(S->found, S->cur, sizeof S->found);
            }
        }
        S->cur[t] = 0;
        return;
    }
    for (int k = left < S->room[t] ? left : S->room[t]; k >= 0; k--) {
        S->cur[t] = (uint8_t)k;
        search_rec(S, t + 1, left - k);
        if (S->hit && !S->drops) break;
    }
    S->cur[t] = 0;
}

static void search_init(cmx_search *S, const cmx_map *m, const uint8_t *add, float tol)
{
    memset(S, 0, sizeof *S);
    S->m = m;
    S->tol = tol;
    to_lab(m->target, &S->L, &S->A, &S->B);
    for (int t = 0; t < m->n; t++) {
        int d = m->start[t] + (add ? add[t] : 0);
        for (int i = 0; i < CMX_BASE; i++) {
            S->base[i] += d * MIX[m->paint[t]][i];
            S->per[t][i] = MIX[m->paint[t]][i];
        }
        int used = add ? add[t] : 0;
        S->room[t] = (uint8_t)(m->limit[t] == CMX_ANY ? 99 : m->limit[t] > used ? m->limit[t] - used : 0);
    }
}

int cmx_solve(const cmx_map *m, const uint8_t *add, int most, uint8_t *more)
{
    cmx_search S;
    search_init(&S, m, add, CMX_TOL);
    if (m->n < 1) return -1;
    for (int k = 0; k <= most; k++) {
        search_rec(&S, 0, k);
        if (S.hit) {
            if (more) memcpy(more, S.found, CMX_TUBES);
            return k;
        }
    }
    return -1;
}

int cmx_rivals(const cmx_map *m, int drops, float de)
{
    cmx_search S;
    search_init(&S, m, NULL, de);
    S.drops = 1;
    S.skip_exact = 1;
    for (int k = 1; k <= drops; k++) search_rec(&S, 0, k);
    return S.count;
}

int cmx_hint(const cmx_map *m, const cmx_state *s, int *tube, int *take)
{
    if (cmx_solved(m, s)) return 0;
    uint8_t more[CMX_TUBES];
    int r = cmx_solve(m, s->add, 10, more);
    if (r > 0) {                                /* a drop of the tube the rest needs most */
        int best = -1;
        for (int t = 0; t < m->n; t++)
            if (more[t] && (best < 0 || more[t] > more[best])) best = t;
        *tube = best;
        *take = 0;
        return 1;
    }
    /* too far gone: the drop to take back that leaves the shortest way */
    int best = -1, bestr = 1 << 20;
    for (int t = 0; t < m->n; t++) {
        if (!s->add[t]) continue;
        uint8_t a[CMX_TUBES];
        memcpy(a, s->add, sizeof a);
        a[t]--;
        int k = cmx_solve(m, a, 8, NULL);
        if (k >= 0 && k < bestr) { bestr = k; best = t; }
    }
    if (best < 0 && s->n) best = s->order[s->n - 1];
    if (best < 0) return 0;
    *tube = best;
    *take = 1;
    return 1;
}

/* ---- boards as text ---------------------------------------------------------------------------------------------- */
int cmx_parse(cmx_map *m, const char *t)
{
    memset(m, 0, sizeof *m);
    const char *f[4];
    int len[4], k = 0;
    for (const char *s = t; k < 4; k++) {
        f[k] = s;
        const char *e = strchr(s, '/');
        len[k] = e ? (int)(e - s) : (int)strlen(s);
        if (!e && k < 3) return 0;
        s = e ? e + 1 : s + len[k];
    }
    m->n = len[0];
    if (m->n < 1 || m->n > CMX_TUBES) return 0;
    for (int i = 1; i < 4; i++)
        if (len[i] != m->n) return 0;
    for (int i = 0; i < m->n; i++) {
        const char *p = strchr(LETTER, f[0][i]);
        if (!p || !f[0][i]) return 0;
        m->paint[i] = (uint8_t)(p - LETTER);
        m->limit[i] = f[1][i] == '-' ? CMX_ANY : (uint8_t)(f[1][i] - '0');
        m->start[i] = (uint8_t)(f[2][i] - '0');
        m->recipe[i] = (uint8_t)(f[3][i] - '0');
        if ((f[1][i] != '-' && m->limit[i] > 9) || m->start[i] > 9 || m->recipe[i] > 9) return 0;
    }
    cmx_pot(m, m->recipe, m->target);
    return 1;
}

void cmx_text(const cmx_map *m, char *out)
{
    int k = 0;
    for (int i = 0; i < m->n; i++) out[k++] = LETTER[m->paint[i]];
    out[k++] = '/';
    for (int i = 0; i < m->n; i++) out[k++] = m->limit[i] == CMX_ANY ? '-' : (char)('0' + m->limit[i]);
    out[k++] = '/';
    for (int i = 0; i < m->n; i++) out[k++] = (char)('0' + m->start[i]);
    out[k++] = '/';
    for (int i = 0; i < m->n; i++) out[k++] = (char)('0' + m->recipe[i]);
    out[k] = 0;
}

void cmx_begin(cmx_game *g) { memset(&g->s, 0, sizeof g->s); }

/* ---- making boards ------------------------------------------------------------------------------------------------
 * The tubes offered, a recipe drawn among them (its kinds and drops in the spec's ranges, its counts sharing no
 * factor when the pot starts empty), maybe drops in the pot at first, maybe tubes holding just what the recipe takes;
 * kept when no fewer drops reach the colour, no other recipe of as few drops comes near it, and the target is not
 * the start's colour nor a tube's. */
static int gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }

void cmx_make(cmx_map *m, const cmx_spec *sp, uint32_t seed, int *par)
{
    uint32_t r = pr_seed(seed);
    *par = -1;
    for (int attempt = 0; attempt < 300; attempt++) {
        cmx_map t;
        memset(&t, 0, sizeof t);
        for (int p = 0; p < CMX_PAINTS && t.n < CMX_TUBES; p++)
            if (sp->tubes >> p & 1) { t.paint[t.n] = (uint8_t)p; t.limit[t.n] = CMX_ANY; t.n++; }
        if (t.n < 1) return;
        int kinds = sp->kinds_lo + pr_below(&r, sp->kinds_hi - sp->kinds_lo + 1);
        if (kinds > t.n) kinds = t.n;
        int idx[CMX_TUBES];
        for (int i = 0; i < t.n; i++) idx[i] = i;
        pr_shuffle(&r, idx, t.n);
        int total = sp->lo + pr_below(&r, sp->hi - sp->lo + 1);
        if (total < kinds) total = kinds;
        for (int i = 0; i < kinds; i++) t.recipe[idx[i]] = 1;
        for (int k = kinds; k < total; k++) t.recipe[idx[pr_below(&r, kinds)]]++;
        int g = 0, ok = 1;
        for (int i = 0; i < t.n; i++) {
            g = gcd(g, t.recipe[i]);
            if (t.recipe[i] > 9) ok = 0;
        }
        if (!ok) continue;
        if (sp->need & 1 << CMX_WHITE) {
            int w = 0;
            for (int i = 0; i < t.n; i++) w |= t.recipe[i] && (t.paint[i] == CMX_WHITE || t.paint[i] == CMX_BLACK);
            if (!w) continue;
        }
        if (sp->need & 1 << CMX_ORANGE) {
            int w = 0;
            for (int i = 0; i < t.n; i++) w |= t.recipe[i] && t.paint[i] >= CMX_BASE;
            if (!w) continue;
        }
        if (sp->start) {                        /* paint in the pot already: one or two of the primaries, white */
            int n = 1 + pr_below(&r, sp->start), kinds2 = 1 + pr_below(&r, 2), pick[2];
            int base[CMX_TUBES], nb = 0;
            for (int i = 0; i < t.n; i++)
                if (t.paint[i] <= CMX_WHITE) base[nb++] = i;
            if (!nb) continue;
            for (int k = 0; k < kinds2; k++) pick[k] = base[pr_below(&r, nb)];
            for (int k = 0; k < n; k++) t.start[pick[k % kinds2]]++;
        } else if (g != 1) continue;
        if (sp->limits) {                       /* tubes holding just what the recipe takes */
            int used[CMX_TUBES], nu = 0;
            for (int i = 0; i < t.n; i++)
                if (t.recipe[i]) used[nu++] = i;
            pr_shuffle(&r, used, nu);
            for (int k = 0; k < sp->limits && k < nu; k++) t.limit[used[k]] = t.recipe[used[k]];
        }
        cmx_pot(&t, t.recipe, t.target);
        float sc[3];
        if (cmx_pot(&t, NULL, sc) && cmx_de(sc, t.target) < 12) continue;   /* the start's colour already */
        int plain = 0;
        for (int i = 0; i < t.n; i++) {
            float c[3];
            cmx_paint_rgb(t.paint[i], c);
            plain |= cmx_de(c, t.target) < 8;
        }
        if (plain) continue;
        int p = cmx_solve(&t, NULL, total, NULL);
        if (p != total) continue;
        if (cmx_rivals(&t, total, 2.2f * CMX_TOL)) continue;
        *m = t;
        *par = p;
        return;
    }
}

/* ---- the notation ----------------------------------------------------------------------------------------------- */
int cmx_script(cmx_game *g, const char *moves)
{
    for (const char *s = moves; s && *s; s++) {
        if (*s == ' ') continue;
        if (*s == '-') { if (!cmx_undo(g)) return 0; continue; }
        if (*s == 'h' || *s == '=') {
            for (int k = 0; k < (*s == '=' ? 40 : 1) && !cmx_solved(&g->m, &g->s); k++) {
                int t, take;
                if (!cmx_hint(&g->m, &g->s, &t, &take)) break;
                if (take) cmx_take(g, t);
                else cmx_drop(g, t);
            }
            continue;
        }
        if (!cmx_drop(g, *s - 'a')) return 0;
    }
    return 1;
}

/* ---- levels and random boards ------------------------------------------------------------------------------------ */
void cmx_variant(int v, cmx_spec *sp)
{
    static const cmx_spec V[CMX_VARIANTS] = {
        /* tubes, lo, hi, kinds, start, limits, need */
        {1 << CMX_RED | 1 << CMX_YELLOW | 1 << CMX_BLUE | 1 << CMX_WHITE, 2, 4, 2, 2, 0, 0, 0},
        {1 << CMX_RED | 1 << CMX_YELLOW | 1 << CMX_BLUE | 1 << CMX_WHITE | 1 << CMX_BLACK, 3, 6, 2, 3, 0, 0, 0},
        {1 << CMX_RED | 1 << CMX_YELLOW | 1 << CMX_BLUE | 1 << CMX_WHITE | 1 << CMX_BLACK | 1 << CMX_ORANGE |
             1 << CMX_GREEN, 5, 9, 3, 4, 0, 0, 0},
    };
    *sp = V[v < 0 || v >= CMX_VARIANTS ? 0 : v];
}

#ifndef CMX_GEN                                 /* not in the levels' tool (tools/colourmix_gen.c): it makes them */
#include "games/colourmix_levels.h"

int cmx_level(cmx_game *g, int level)
{
    if (level < 0 || level >= CMX_LEVELS || !cmx_parse(&g->m, cmx_levels[level].text)) return 0;
    cmx_begin(g);
    g->par = cmx_levels[level].par;
    return 1;
}

void cmx_random(cmx_game *g, int variant, uint32_t seed)
{
    cmx_spec sp;
    cmx_variant(variant, &sp);
    uint32_t r = pr_seed(seed);
    for (int k = 0; k < 30; k++) {
        int par;
        if (k && k % 10 == 0 && sp.hi > sp.lo) sp.hi--;
        cmx_make(&g->m, &sp, pr_next(&r), &par);
        if (par >= 1) {
            cmx_begin(g);
            g->par = par;
            return;
        }
    }
    cmx_level(g, 0);
}
#endif
