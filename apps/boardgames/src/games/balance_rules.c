/*
 * Balance Scales: the rules (balance.h): the scales as equations, their rank, the reasoning that grades a puzzle and
 * gives the hints, the maker, the player's guesses and undo. No SDL: tested by tests/balance_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/balance.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

/* ---- the scales as equations: sum coef[s] * w[s] = rhs ------------------------------------------------------------ */
static void equation(const bal_scale *s, int *coef, int *rhs)
{
    memset(coef, 0, sizeof(int) * BAL_SHAPES);
    for (int side = 0; side < 2; side++)
        for (int i = 0; i < s->n[side]; i++) coef[s->s[side][i]] += side ? -1 : 1;
    *rhs = s->w[1] - s->w[0];
}

int bal_tilt(const bal_scale *s, const int16_t *w)
{
    int load[2] = {s->w[0], s->w[1]};
    for (int side = 0; side < 2; side++)
        for (int i = 0; i < s->n[side]; i++) {
            if (w[s->s[side][i]] <= 0) return 2;
            load[side] += w[s->s[side][i]];
        }
    return load[0] > load[1] ? -1 : load[0] < load[1] ? 1 : 0;
}

int bal_check(const bal_puzzle *p)
{
    for (int i = 0; i < p->m; i++)
        if (bal_tilt(&p->sc[i], p->wt) != 0) return 0;
    for (int s = 0; s < p->k; s++)
        if (p->wt[s] < 1) return 0;
    return 1;
}

int bal_rank(const bal_puzzle *p)
{
    double a[BAL_SCALES][BAL_SHAPES];
    for (int i = 0; i < p->m; i++) {
        int c[BAL_SHAPES], r;
        equation(&p->sc[i], c, &r);
        for (int s = 0; s < p->k; s++) a[i][s] = c[s];
    }
    int rank = 0;
    for (int col = 0; col < p->k && rank < p->m; col++) {
        int piv = -1;
        for (int i = rank; i < p->m; i++)
            if (fabs(a[i][col]) > 1e-9 && (piv < 0 || fabs(a[i][col]) > fabs(a[piv][col]))) piv = i;
        if (piv < 0) continue;
        for (int s = 0; s < p->k; s++) {
            double t = a[rank][s];
            a[rank][s] = a[piv][s];
            a[piv][s] = t;
        }
        for (int i = 0; i < p->m; i++)
            if (i != rank && fabs(a[i][col]) > 1e-9) {
                double f = a[i][col] / a[rank][col];
                for (int s = 0; s < p->k; s++) a[i][s] -= f * a[rank][s];
            }
        rank++;
    }
    return rank;
}

/* one step of reasoning from the weights known (known[s] 0: not yet): the shape found (its weight in *w), its kind
 * in *pair (0 plain, 1 pair); -1 none */
static int step(const bal_puzzle *p, const int16_t *known, int *w, int *pair)
{
    int c[BAL_SCALES][BAL_SHAPES], r[BAL_SCALES];
    for (int i = 0; i < p->m; i++) {
        equation(&p->sc[i], c[i], &r[i]);
        for (int s = 0; s < p->k; s++)
            if (known[s]) r[i] -= c[i][s] * known[s], c[i][s] = 0;
    }
    for (int round = 0; round < 2; round++)
        for (int a = 0; a < p->m; a++)
            for (int b = round ? 0 : a; b < (round ? p->m : a + 1); b++)
                for (int sign = -1; sign <= (round ? 1 : -1); sign += 2) {
                    if (round && b <= a) continue;
                    int cc[BAL_SHAPES], rr = r[a] + (round ? sign * r[b] : 0), one = -1, n = 0;
                    for (int s = 0; s < p->k; s++) {
                        cc[s] = c[a][s] + (round ? sign * c[b][s] : 0);
                        if (cc[s]) one = s, n++;
                    }
                    if (n != 1 || rr % cc[one] || rr / cc[one] < 1) continue;
                    *w = rr / cc[one];
                    *pair = round;
                    return one;
                }
    return -1;
}

int bal_reason(const bal_puzzle *p, int *plain, int *pair)
{
    int16_t known[BAL_SHAPES] = {0};
    int np = 0, nq = 0;
    for (int got = 0; got < p->k; got++) {
        int w, kind, s = step(p, known, &w, &kind);
        if (s < 0 || w != p->wt[s]) return 0;
        known[s] = (int16_t)w;
        if (kind) nq++;
        else np++;
    }
    if (plain) *plain = np;
    if (pair) *pair = nq;
    return 1;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
static int pan_weight(const bal_scale *s, int side, const int16_t *w)
{
    int t = 0;
    for (int i = 0; i < s->n[side]; i++) t += w[s->s[side][i]];
    return t;
}

static int same(const bal_scale *a, const bal_scale *b)
{
    int ca[BAL_SHAPES], cb[BAL_SHAPES], ra, rb;
    equation(a, ca, &ra);
    equation(b, cb, &rb);
    int pos = ra == rb, neg = ra == -rb;
    for (int s = 0; s < BAL_SHAPES; s++) pos &= ca[s] == cb[s], neg &= ca[s] == -cb[s];
    return pos || neg;
}

static void deal_scale(const bal_make *m, const int16_t *wt, uint32_t *s, bal_scale *sc)
{
    memset(sc, 0, sizeof *sc);
    int on_left = 0;
    sc->n[0] = (uint8_t)(1 + pr_below(s, m->pan));
    for (int i = 0; i < sc->n[0]; i++) {
        sc->s[0][i] = (uint8_t)pr_below(s, m->k);
        on_left |= 1 << sc->s[0][i];
    }
    if (m->both && pr_below(s, 100) < 65) {     /* shapes on the right too, none of the left's kinds */
        int free[BAL_SHAPES], nf = 0;
        for (int k = 0; k < m->k; k++)
            if (!(on_left >> k & 1)) free[nf++] = k;
        int n = nf ? 1 + pr_below(s, m->pan - 1 > 0 ? m->pan - 1 : 1) : 0;
        for (int i = 0; i < n; i++) sc->s[1][i] = (uint8_t)free[pr_below(s, nf)];
        sc->n[1] = (uint8_t)n;
    }
    for (int side = 0; side < 2; side++)        /* each pan in order of its shapes */
        for (int i = 1; i < sc->n[side]; i++)
            for (int j = i; j > 0 && sc->s[side][j - 1] > sc->s[side][j]; j--) {
                uint8_t t = sc->s[side][j];
                sc->s[side][j] = sc->s[side][j - 1];
                sc->s[side][j - 1] = t;
            }
    int diff = pan_weight(sc, 0, wt) - pan_weight(sc, 1, wt);
    if (diff > 0) sc->w[1] = (int16_t)diff;
    else if (diff < 0) sc->w[0] = (int16_t)-diff;
}

void bal_make_puzzle(const bal_make *m, uint32_t seed, bal_puzzle *p)
{
    uint32_t s = pr_seed(seed);
    int k = m->k < 1 ? 1 : m->k > BAL_SHAPES ? BAL_SHAPES : m->k, ns = m->m < k ? k : m->m > BAL_SCALES ? BAL_SCALES : m->m;
    bal_make mk = *m;
    mk.k = k;
    if (mk.pan < 1) mk.pan = 1;
    if (mk.pan > BAL_PAN) mk.pan = BAL_PAN;
    for (int attempt = 0; attempt < 20000; attempt++) {
        memset(p, 0, sizeof *p);
        p->k = k;
        p->m = ns;
        for (int i = 0; i < k; i++) {           /* the weights: different ones while there are enough */
            int w, again;
            do {
                w = 1 + pr_below(&s, m->wmax);
                again = 0;
                for (int j = 0; j < i; j++) again |= p->wt[j] == w;
            } while (again && k <= m->wmax);
            p->wt[i] = (int16_t)w;
        }
        int ok = 1, seen = 0;
        for (int i = 0; i < ns && ok; i++) {
            deal_scale(&mk, p->wt, &s, &p->sc[i]);
            const bal_scale *sc = &p->sc[i];
            ok = sc->w[0] < 100 && sc->w[1] < 100 && (sc->n[1] || sc->w[1]);
            for (int j = 0; j < i && ok; j++) ok = !same(sc, &p->sc[j]);
            for (int side = 0; side < 2; side++)
                for (int t = 0; t < sc->n[side]; t++) seen |= 1 << sc->s[side][t];
        }
        if (!ok || seen != (1 << k) - 1 || bal_rank(p) != k) continue;
        int plain, pair;
        if (!bal_reason(p, &plain, &pair)) continue;
        if (m->pair ? pair < 1 : pair > 0) continue;
        return;
    }
    memset(p, 0, sizeof *p);                    /* never found: one shape on one scale */
    p->k = p->m = 1;
    p->wt[0] = 2;
    p->sc[0].n[0] = 1;
    p->sc[0].w[1] = 2;
}

bal_make bal_variant_make(int variant)
{
    static const bal_make V[BAL_VARIANTS] = {{2, 2, 9, 0, 0, 3}, {3, 3, 12, 1, 0, 4}, {4, 4, 15, 1, 1, 4}};
    return V[variant < 0 || variant >= BAL_VARIANTS ? 0 : variant];
}

/* ---- as text ----------------------------------------------------------------------------------------------------- */
int bal_encode(const bal_puzzle *p, char *out, int size)
{
    int len = 0;
#define PUT(...)                                                                                                    \
    do {                                                                                                            \
        int k_ = snprintf(out + len, (size_t)(size - len), __VA_ARGS__);                                            \
        if (k_ < 0 || len + k_ >= size) return 0;                                                                   \
        len += k_;                                                                                                  \
    } while (0)
    for (int s = 0; s < p->k; s++) PUT("%s%d", s ? "," : "", p->wt[s]);
    for (int i = 0; i < p->m; i++) {
        PUT("%c", i ? '|' : ';');
        for (int side = 0; side < 2; side++) {
            if (side) PUT("/");
            int first = 1;
            for (int t = 0; t < p->sc[i].n[side]; t++, first = 0) PUT("%s%c", first ? "" : ",", 'a' + p->sc[i].s[side][t]);
            if (p->sc[i].w[side]) PUT("%s%d", first ? "" : ",", p->sc[i].w[side]);
        }
    }
#undef PUT
    return len;
}

int bal_decode(const char *s, bal_puzzle *p)
{
    memset(p, 0, sizeof *p);
    char *e;
    while (p->k < BAL_SHAPES) {
        long w = strtol(s, &e, 10);
        if (e == s || w < 1 || w > 99) return 0;
        p->wt[p->k++] = (int16_t)w;
        s = e;
        if (*s != ',') break;
        s++;
    }
    if (*s != ';') return 0;
    while (*s == ';' || *s == '|') {
        if (p->m >= BAL_SCALES) return 0;
        bal_scale *sc = &p->sc[p->m++];
        s++;
        for (int side = 0; side < 2; side++) {
            if (side && *s++ != '/') return 0;
            while (*s && *s != '/' && *s != '|') {
                if (*s >= 'a' && *s < 'a' + p->k) {
                    if (sc->n[side] >= BAL_PAN) return 0;
                    sc->s[side][sc->n[side]++] = (uint8_t)(*s++ - 'a');
                } else {
                    long w = strtol(s, &e, 10);
                    if (e == s || w < 1 || w > 99) return 0;
                    sc->w[side] = (int16_t)w;
                    s = e;
                }
                if (*s == ',') s++;
            }
        }
    }
    return *s == 0 && p->m > 0 && bal_check(p);
}

/* ---- the player's guesses ---------------------------------------------------------------------------------------- */
void bal_load(bal_game *g, const bal_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
}

void bal_new(bal_game *g, int variant, uint32_t seed)
{
    bal_puzzle p;
    bal_make m = bal_variant_make(variant);
    bal_make_puzzle(&m, seed, &p);
    bal_load(g, &p);
}

int bal_most(const bal_puzzle *p)
{
    int w = 1;
    for (int s = 0; s < p->k; s++) w = p->wt[s] > w ? p->wt[s] : w;
    return w;
}

int bal_solved(const bal_game *g)
{
    if (!g->p.k) return 0;
    for (int s = 0; s < g->p.k; s++)
        if (g->guess[s] != g->p.wt[s]) return 0;
    return 1;
}

void bal_judge(bal_game *g)                  /* every shape guessed and some wrong: a mistake (once) */
{
    int full = 1;
    for (int s = 0; s < g->p.k; s++) full &= g->guess[s] > 0;
    if (full && !bal_solved(g) && !g->full_wrong) g->mistakes++, g->full_wrong = 1;
    if (!full || bal_solved(g)) g->full_wrong = 0;
}

int bal_set(bal_game *g, int shape, int w)
{
    if (shape < 0 || shape >= g->p.k || w < 0 || w > 99 || g->guess[shape] == w) return 0;
    if (g->nh == BAL_HIST) {
        memmove(g->hist[0], g->hist[1], sizeof g->hist[0] * (BAL_HIST - 1));
        g->nh--;
    }
    memcpy(g->hist[g->nh++], g->guess, sizeof g->guess);
    g->moves++;
    g->guess[shape] = (int16_t)w;
    g->full_wrong = 0;
    return 1;
}

int bal_undo(bal_game *g)
{
    if (!g->nh) return 0;
    memcpy(g->guess, g->hist[--g->nh], sizeof g->guess);
    g->moves--;
    g->full_wrong = 0;
    return 1;
}

int bal_hint(bal_game *g)
{
    if (bal_solved(g)) return -1;
    int s = -1;
    for (int t = 0; t < g->p.k && s < 0; t++)   /* a wrong guess first */
        if (g->guess[t] && g->guess[t] != g->p.wt[t]) s = t;
    if (s < 0) {
        int16_t known[BAL_SHAPES] = {0};
        for (int t = 0; t < g->p.k; t++)
            if (g->guess[t] == g->p.wt[t]) known[t] = g->guess[t];
        int w, kind;
        s = step(&g->p, known, &w, &kind);
    }
    for (int t = 0; t < g->p.k && s < 0; t++)
        if (g->guess[t] != g->p.wt[t]) s = t;
    if (s < 0 || !bal_set(g, s, g->p.wt[s])) return -1;
    g->hints++;
    bal_judge(g);
    return s;
}

int bal_script(bal_game *g, const char *s)
{
    while (*s) {
        char c = *s;
        if (c == ' ' || c == ',') { s++; continue; }
        if (c == 'h') {
            if (bal_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (c == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int t = 0; t < g->p.k && k > 0; t++)
                if (g->guess[t] != g->p.wt[t]) bal_set(g, t, g->p.wt[t]), bal_judge(g), k--;
            continue;
        }
        if (c < 'a' || c >= 'a' + g->p.k) return 0;
        s++;
        int w = 0;
        if (*s == '=') {
            for (s++; *s >= '0' && *s <= '9'; s++) w = w * 10 + *s - '0';
            if (w < 1) return 0;
        }
        bal_set(g, c - 'a', w);
        bal_judge(g);
    }
    return 1;
}
