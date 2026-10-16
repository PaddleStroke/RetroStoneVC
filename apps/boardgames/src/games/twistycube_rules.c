/*
 * Twisty Cube's rules (twistycube.h): the stickers, the layer turns, the solved cube, scrambles, the game with its
 * history, the hint, the notation. No SDL: the tests and the level maker (tools/twistycube_gen.c) build it alone.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/twistycube.h"

#ifndef TCB_GEN
#include "games/twistycube_levels.h"
#endif

/* ---- the stickers' places ------------------------------------------------------------------------------------- */
void tcb_place(int n, int s, int p[3], int nrm[3])
{
    int f = s / (n * n), rc = s % (n * n), row = rc / n, col = rc % n, a = f / 2, sg = f % 2 ? -1 : 1;
    int u = (a + 1) % 3, v = (a + 2) % 3;
    p[a] = sg * (n - 1);
    p[u] = 2 * col - (n - 1);
    p[v] = 2 * row - (n - 1);
    nrm[0] = nrm[1] = nrm[2] = 0;
    nrm[a] = sg;
}

int tcb_sticker_at(int n, const int p[3], const int nrm[3])
{
    int a = nrm[0] ? 0 : nrm[1] ? 1 : 2, sg = nrm[a] > 0 ? 1 : -1, u = (a + 1) % 3, v = (a + 2) % 3;
    if (p[a] != sg * (n - 1)) return -1;
    int col = p[u] + n - 1, row = p[v] + n - 1;
    if (col < 0 || row < 0 || col > 2 * (n - 1) || row > 2 * (n - 1) || (col & 1) || (row & 1)) return -1;
    return (2 * a + (sg < 0)) * n * n + row / 2 * n + col / 2;
}

int tcb_layer_of(int n, int s, int axis)
{
    int p[3], nrm[3];
    tcb_place(n, s, p, nrm);
    return (p[axis] + n - 1) / 2;
}

static void rot(int a, int q, int p[3])         /* q quarter turns, counter-clockwise seen from +a */
{
    int u = (a + 1) % 3, v = (a + 2) % 3;
    for (int i = 0; i < (q & 3); i++) {
        int t = p[u];
        p[u] = -p[v];
        p[v] = t;
    }
}

/* ---- the turns -------------------------------------------------------------------------------------------------- */
static void make_perm(int n, tcb_move m, uint8_t *to)   /* sticker s goes to to[s] */
{
    int a = tcb_axis(m), l = tcb_layer(m), q = tcb_back(m) ? 3 : 1;
    for (int s = 0; s < 6 * n * n; s++) {
        int p[3], nrm[3];
        tcb_place(n, s, p, nrm);
        if ((p[a] + n - 1) / 2 != l) { to[s] = (uint8_t)s; continue; }
        rot(a, q, p);
        rot(a, q, nrm);
        to[s] = (uint8_t)tcb_sticker_at(n, p, nrm);
    }
}

/* the turns' tables, made once (the same bytes whichever thread makes them first) */
static uint8_t perm[TCB_MAXN + 1][3 * 8 + 8][TCB_STICKERS];
static int perm_made[TCB_MAXN + 1];          /* set (release) once its table is whole */

static const uint8_t *perm_of(int n, tcb_move m)
{
    if (!__atomic_load_n(&perm_made[n], __ATOMIC_ACQUIRE)) {
        uint8_t t[3 * 8 + 8][TCB_STICKERS];
        uint8_t tmp[TCB_STICKERS];
        for (int a = 0; a < 3; a++)
            for (int l = 0; l < n; l++)
                for (int b = 0; b < 2; b++) {
                    tcb_move mv = tcb_mv(a, l, b);
                    make_perm(n, mv, tmp);
                    memcpy(t[mv], tmp, sizeof tmp);
                }
        memcpy(perm[n], t, sizeof t);
        __atomic_store_n(&perm_made[n], 1, __ATOMIC_RELEASE);
    }
    return perm[n][m];
}

int tcb_valid(int n, tcb_move m) { return n >= 2 && n <= TCB_MAXN && tcb_axis(m) < 3 && tcb_layer(m) < n; }

void tcb_init(tcb_cube *c, int n)
{
    memset(c, 0, sizeof *c);
    c->n = n;
    for (int s = 0; s < 6 * n * n; s++) c->c[s] = (uint8_t)(s / (n * n));
}

void tcb_turn(tcb_cube *c, tcb_move m)
{
    if (!tcb_valid(c->n, m)) return;
    const uint8_t *to = perm_of(c->n, m);
    uint8_t o[TCB_STICKERS];
    int k = 6 * c->n * c->n;
    for (int s = 0; s < k; s++) o[to[s]] = c->c[s];
    memcpy(c->c, o, (size_t)k);
}

int tcb_solved(const tcb_cube *c)
{
    int nn = c->n * c->n;
    for (int f = 0; f < 6; f++)
        for (int i = 1; i < nn; i++)
            if (c->c[f * nn + i] != c->c[f * nn]) return 0;
    return 1;
}

/* ---- scrambles -------------------------------------------------------------------------------------------------- */
static int qcost(int t) { t &= 3; return t == 3 ? 1 : t; }

int tcb_axis_cost(int n, const int *turns)
{
    int best = 1 << 30;
    for (int k = 0; k < 4; k++) {
        int c = 0;
        for (int l = 0; l < n; l++) c += qcost(turns[l] - k);
        if (c < best) best = c;
    }
    return best;
}

int tcb_full_depth(int n) { return n <= 2 ? 12 : n == 3 ? 26 : 40; }

int tcb_scramble(uint32_t *rng, int n, int depth, tcb_move *out)
{
    int run_axis = -1, run_len = 0, last_layer = -1, last_back = 0, turns[TCB_MAXN] = {0};
    if (depth > TCB_MAXSCR) depth = TCB_MAXSCR;
    for (int i = 0; i < depth;) {
        int a = pr_below(rng, 3), l = pr_below(rng, n), b = pr_below(rng, 2);
        if (a == run_axis) {
            int half = l == last_layer && b == last_back && (turns[l] & 3) != 2;
            if (l < last_layer || (l == last_layer && !half)) continue;
            int t[TCB_MAXN];
            memcpy(t, turns, sizeof t);
            t[l] += b ? -1 : 1;
            if (tcb_axis_cost(n, t) != run_len + 1) continue;
            memcpy(turns, t, sizeof t);
            run_len++;
        } else {
            memset(turns, 0, sizeof turns);
            turns[l] = b ? -1 : 1;
            run_axis = a;
            run_len = 1;
        }
        last_layer = l;
        last_back = b;
        out[i++] = tcb_mv(a, l, b);
    }
    return depth;
}

/* ---- the game --------------------------------------------------------------------------------------------------- */
void tcb_new(tcb_game *g, int n, const tcb_move *scr, int nscr, int par, int full)
{
    memset(g, 0, sizeof *g);
    g->n = n;
    g->par = par;
    g->full = full;
    tcb_init(&g->cube, n);
    if (nscr > TCB_MAXSCR) nscr = TCB_MAXSCR;
    for (int i = 0; i < nscr; i++)
        if (tcb_valid(n, scr[i])) {
            g->scr[g->nscr++] = scr[i];
            tcb_turn(&g->cube, scr[i]);
        }
}

void tcb_random(tcb_game *g, int variant, uint32_t seed)
{
    int n = variant < 0 ? 3 : variant > 2 ? 4 : variant + 2;
    uint32_t r = pr_seed(seed);
    tcb_move m[TCB_MAXSCR];
    int k = tcb_scramble(&r, n, tcb_full_depth(n), m);
    tcb_new(g, n, m, k, 0, 1);
}

#ifndef TCB_GEN
int tcb_level_info(int level, int *n, int *par, int *full)
{
    if (level < 0 || level >= TCB_LEVELS) return 0;
    const uint8_t *d = TCB_LEVEL_DATA + TCB_LEVEL_AT[level];
    if (n) *n = d[0];
    if (par) *par = d[1];
    if (full) *full = d[1] == 0;
    return 1;
}

int tcb_level(tcb_game *g, int level)
{
    if (level < 0 || level >= TCB_LEVELS) return 0;
    const uint8_t *d = TCB_LEVEL_DATA + TCB_LEVEL_AT[level];
    tcb_new(g, d[0], d + 3, d[2], d[1], d[1] == 0);
    return 1;
}
#endif

int tcb_play(tcb_game *g, tcb_move m)
{
    if (!tcb_valid(g->n, m)) return 0;
    if (g->nh == TCB_HIST) {                    /* the oldest move forgotten (undo stops there) */
        memmove(g->hist, g->hist + 1, (TCB_HIST - 1) * sizeof g->hist[0]);
        g->nh--;
    }
    g->hist[g->nh++] = m;
    tcb_turn(&g->cube, m);
    return 1;
}

int tcb_undo(tcb_game *g)
{
    if (g->nh <= 0) return 0;
    tcb_turn(&g->cube, tcb_inverse(g->hist[--g->nh]));
    return 1;
}

int tcb_moves(const tcb_game *g) { return g->nh; }

int tcb_reduce(int n, const tcb_move *in, int k, tcb_move *out)
{
    int no = 0;
    for (int i = 0; i < k; i++) {
        tcb_move m = in[i];
        int a = tcb_axis(m), from = no, t[TCB_MAXN] = {0};
        while (from > 0 && tcb_axis(out[from - 1]) == a) from--;
        for (int j = from; j < no; j++) t[tcb_layer(out[j])] += tcb_back(out[j]) ? -1 : 1;
        t[tcb_layer(m)] += tcb_back(m) ? -1 : 1;
        no = from;
        for (int l = 0; l < n; l++) {
            int q = t[l] & 3;
            if (q == 1 || q == 2) out[no++] = tcb_mv(a, l, 0);
            if (q == 2) out[no++] = tcb_mv(a, l, 0);
            if (q == 3) out[no++] = tcb_mv(a, l, 1);
        }
    }
    return no;
}

int tcb_hint(const tcb_game *g, tcb_move *m)
{
    if (g->full || tcb_solved(&g->cube)) return 0;
    static tcb_move all[TCB_MAXSCR + TCB_HIST], red[TCB_MAXSCR + TCB_HIST];   /* the main thread's only */
    int k = 0;
    for (int i = 0; i < g->nscr; i++) all[k++] = g->scr[i];
    for (int i = 0; i < g->nh; i++) all[k++] = g->hist[i];
    int r = tcb_reduce(g->n, all, k, red);
    if (r <= 0) return 0;
    *m = tcb_inverse(red[r - 1]);
    return 1;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int tcb_parse_move(int n, const char *s, tcb_move *m)
{
    if (s[0] < 'x' || s[0] > 'z' || s[1] < '0' || s[1] > '3' || (s[2] != '+' && s[2] != '-')) return 0;
    *m = tcb_mv(s[0] - 'x', s[1] - '0', s[2] == '-');
    return tcb_valid(n, *m);
}

void tcb_move_text(tcb_move m, char out[4])
{
    out[0] = (char)('x' + tcb_axis(m));
    out[1] = (char)('0' + tcb_layer(m));
    out[2] = tcb_back(m) ? '-' : '+';
    out[3] = 0;
}

int tcb_script(tcb_game *g, const char *moves)
{
    char w[16];
    int k = 0;
    for (const char *p = moves;; p++) {
        if (*p && *p != ' ' && *p != ',') {
            if (k < 15) w[k++] = *p;
            continue;
        }
        if (k) {
            w[k] = 0;
            tcb_move m;
            if (!strcmp(w, "h")) {
                if (!tcb_hint(g, &m)) return 0;
                g->hints++;
                tcb_play(g, m);
            } else if (!tcb_parse_move(g->n, w, &m) || !tcb_play(g, m)) return 0;
            k = 0;
        }
        if (!*p) break;
    }
    return 1;
}
