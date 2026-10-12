/*
 * Logic Gates: the rules (logicgates.h): the levels read from their text, the drawing's tracks, the values, the
 * solver (every setting tried), the moves and undo, the hint, the notation, and the generator that helped make the
 * levels (logicgates_levels.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/logicgates.h"
#include "games/logicgates_levels.h"

static const char *const KIND_NAME[LG_KINDS] = {"s", "NOT", "AND", "OR", "XOR", "NAND", "NOR", "y", "L", "A"};

int lg_gate(int kind, const uint8_t *in, int nin)
{
    int ones = 0;
    for (int i = 0; i < nin; i++) ones += in[i] != 0;
    switch (kind) {
    case LG_NOT: return ones == 0;
    case LG_AND: return ones == nin;
    case LG_OR: return ones > 0;
    case LG_XOR: return ones & 1;
    case LG_NAND: return ones != nin;
    case LG_NOR: return ones == 0;
    default: return 0;
    }
}

static int fail(char *err, int size, const char *fmt, ...)
{
    if (err && size > 0) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(err, (size_t)size, fmt, ap);
        va_end(ap);
    }
    return 0;
}

/* ---- the level's text -------------------------------------------------------------------------------------------- */
typedef struct {
    int kind, locked, init, col, row, nref;
    int rcol[3], rrow[3], rport[3], rcut[3];
} lg_tok;

static int read_cell(const char **s, int *col, int *row)
{
    const char *p = *s;
    if (*p < 'a' || *p >= 'a' + LG_COLS || p[1] < '1' || p[1] >= '1' + LG_ROWS) return 0;
    *col = *p - 'a';
    *row = p[1] - '1';
    *s = p + 2;
    return 1;
}

static int read_kind(const char *s, int n, lg_tok *t)
{
    static const struct { const char *name; int kind, locked, init; } K[] = {
        {"s", LG_SWITCH, 0, 0}, {"S", LG_SWITCH, 0, 1}, {"k", LG_SWITCH, 1, 0}, {"K", LG_SWITCH, 1, 1},
        {"NOT", LG_NOT, 0, 0}, {"AND", LG_AND, 0, 0}, {"OR", LG_OR, 0, 0}, {"XOR", LG_XOR, 0, 0},
        {"NAND", LG_NAND, 0, 0}, {"NOR", LG_NOR, 0, 0}, {"y", LG_ROUTE, 0, 0}, {"Y", LG_ROUTE, 0, 1},
        {"L", LG_LAMP, 0, 0}, {"A", LG_ALARM, 0, 0}};
    for (size_t i = 0; i < sizeof K / sizeof K[0]; i++)
        if ((int)strlen(K[i].name) == n && !strncmp(K[i].name, s, (size_t)n)) {
            t->kind = K[i].kind;
            t->locked = K[i].locked;
            t->init = K[i].init;
            return 1;
        }
    return 0;
}

static int ins_of(int kind, int n)
{
    switch (kind) {
    case LG_SWITCH: return n == 0;
    case LG_NOT: case LG_ROUTE: case LG_LAMP: case LG_ALARM: return n == 1;
    default: return n == 2 || n == 3;
    }
}

static int nports(int kind) { return kind == LG_ROUTE ? 2 : kind == LG_LAMP || kind == LG_ALARM ? 0 : 1; }

int lg_parse(const char *src, lg_level *L, char *err, int errsize)
{
    static lg_tok T[LG_NODES];
    int nt = 0, limit = -1;
    memset(L, 0, sizeof *L);
    for (const char *s = src; *s;) {
        if (*s == ' ') { s++; continue; }
        if (*s == '%') {
            limit = (int)strtol(s + 1, (char **)&s, 10);
            continue;
        }
        if (nt >= LG_NODES) return fail(err, errsize, "too many parts");
        lg_tok *t = &T[nt];
        memset(t, 0, sizeof *t);
        const char *at = strchr(s, '@');
        if (!at || !read_kind(s, (int)(at - s), t)) return fail(err, errsize, "a bad part at '%.12s'", s);
        s = at + 1;
        if (!read_cell(&s, &t->col, &t->row)) return fail(err, errsize, "a bad cell at '%.12s'", at);
        if (*s == '(') {
            s++;
            for (;;) {
                if (t->nref >= 3) return fail(err, errsize, "too many inputs at '%.12s'", at);
                int k = t->nref++;
                if (!read_cell(&s, &t->rcol[k], &t->rrow[k])) return fail(err, errsize, "a bad input at '%.12s'", s);
                if (*s == '<' || *s == '>') t->rport[k] = *s++ == '<' ? 1 : 2;
                if (*s == 'x') { t->rcut[k] = 1; s++; }
                if (*s == ',') { s++; continue; }
                if (*s == ')') { s++; break; }
                return fail(err, errsize, "a bad input list at '%.12s'", s);
            }
        }
        if (!ins_of(t->kind, t->nref)) return fail(err, errsize, "%s@%c%d: wrong inputs", KIND_NAME[t->kind],
                                                    'a' + t->col, t->row + 1);
        for (int i = 0; i < nt; i++)
            if (T[i].col == t->col && T[i].row == t->row) return fail(err, errsize, "two parts on one cell");
        nt++;
    }
    /* in reading order from the bottom left */
    for (int i = 1; i < nt; i++) {
        lg_tok t = T[i];
        int j = i;
        while (j > 0 && (T[j - 1].row > t.row || (T[j - 1].row == t.row && T[j - 1].col > t.col))) {
            T[j] = T[j - 1];
            j--;
        }
        T[j] = t;
    }
    int at[LG_COLS][LG_ROWS];
    memset(at, -1, sizeof at);
    for (int i = 0; i < nt; i++) {
        lg_node *n = &L->n[i];
        n->kind = (uint8_t)T[i].kind;
        n->col = (uint8_t)T[i].col;
        n->row = (uint8_t)T[i].row;
        n->nin = (uint8_t)T[i].nref;
        n->locked = (uint8_t)T[i].locked;
        n->init = (uint8_t)T[i].init;
        at[n->col][n->row] = i;
        if (n->col + 1 > L->cols) L->cols = n->col + 1;
        if (n->row + 1 > L->rows) L->rows = n->row + 1;
    }
    L->nn = nt;
    int used[LG_NODES][3] = {{0}}, lamps = 0;
    for (int i = 0; i < nt; i++) {
        lg_node *n = &L->n[i];
        if (n->kind == LG_SWITCH && n->row != 0) return fail(err, errsize, "a switch off the bottom row");
        lamps += n->kind == LG_LAMP;
        for (int k = 0; k < n->nin; k++) {
            int f = at[T[i].rcol[k]][T[i].rrow[k]], port = T[i].rport[k];
            if (f < 0) return fail(err, errsize, "an input from an empty cell %c%d", 'a' + T[i].rcol[k],
                                   T[i].rrow[k] + 1);
            int fk = L->n[f].kind;
            if (fk == LG_LAMP || fk == LG_ALARM) return fail(err, errsize, "a lamp's light used");
            if ((fk == LG_ROUTE) != (port != 0)) return fail(err, errsize, "a route switch's way (< >) is wrong");
            if (L->n[f].row >= n->row) return fail(err, errsize, "a wire not going up");
            if (L->nw >= LG_WIRES) return fail(err, errsize, "too many wires");
            lg_wire *w = &L->w[L->nw];
            w->from = (uint8_t)f;
            w->port = (uint8_t)port;
            w->to = (uint8_t)i;
            w->slot = (uint8_t)k;
            w->cuttable = (uint8_t)T[i].rcut[k];
            w->net = f * 3 + port;
            n->in[k] = (int8_t)L->nw++;
            used[f][port ? port - 1 : 0]++;
        }
    }
    if (!lamps) return fail(err, errsize, "no lamp");
    for (int i = 0; i < nt; i++)
        for (int p = 0; p < nports(L->n[i].kind); p++)
            if (!used[i][p]) return fail(err, errsize, "an output going nowhere at %c%d", 'a' + L->n[i].col,
                                         L->n[i].row + 1);
    /* the notation's numbers, the variables */
    for (int i = 0; i < nt; i++) {
        if (L->n[i].kind == LG_SWITCH) L->sw[L->nsw++] = i;
        if (L->n[i].kind == LG_ROUTE) L->rt[L->nrt++] = i;
    }
    for (int w = 0; w < L->nw; w++)
        if (L->w[w].cuttable) L->cut[L->ncut++] = w;
    L->cut_limit = limit < 0 || limit > L->ncut ? L->ncut : limit;
    for (int i = 0; i < L->nsw; i++)
        if (!L->n[L->sw[i]].locked) {
            if (L->nvar >= LG_VARS) return fail(err, errsize, "too many parts to change");
            L->var_kind[L->nvar] = LG_MSWITCH;
            L->var_idx[L->nvar++] = (uint8_t)i;
        }
    for (int i = 0; i < L->nrt + L->ncut; i++) {
        if (L->nvar >= LG_VARS) return fail(err, errsize, "too many parts to change");
        L->var_kind[L->nvar] = i < L->nrt ? LG_MROUTE : LG_MCUT;
        L->var_idx[L->nvar++] = (uint8_t)(i < L->nrt ? i : i - L->nrt);
    }
    for (int i = 0; i < nt; i++) {
        int d = 0;
        for (int k = 0; k < L->n[i].nin; k++) {
            int f = L->w[L->n[i].in[k]].from;
            if (L->depth[f] + 1 > d) d = L->depth[f] + 1;
        }
        L->depth[i] = d;
        if (d > L->maxdepth) L->maxdepth = d;
    }
    if (!lg_route(L)) return fail(err, errsize, "the wires cannot be drawn apart");
    return 1;
}

/* ---- the drawing: tracks, crossings, fuses ----------------------------------------------------------------------- */
static float in_off(int nin, int slot)          /* an input's place under a part, columns from its middle */
{
    return nin == 1 ? 0 : nin == 2 ? (slot ? 0.18f : -0.18f) : (slot - 1) * 0.24f;
}
static float out_off(int port) { return port == 1 ? -0.18f : port == 2 ? 0.18f : 0; }

typedef struct { float a, b, c; int vert, wire, net; } lg_seg;   /* vertical: x = c, y a..b; horizontal: y = c */

#define EPS 1e-4f

static int segments(const lg_level *L, lg_seg *S)
{
    int n = 0;
    for (int w = 0; w < L->nw; w++) {
        const lg_wire *W = &L->w[w];
        for (int i = 0; i + 1 < W->npt; i++) {
            float x0 = W->px[i], y0 = W->py[i], x1 = W->px[i + 1], y1 = W->py[i + 1];
            if (fabsf(x0 - x1) < EPS && fabsf(y0 - y1) < EPS) continue;
            lg_seg *s = &S[n++];
            s->vert = fabsf(x0 - x1) < EPS;
            s->a = s->vert ? fminf(y0, y1) : fminf(x0, x1);
            s->b = s->vert ? fmaxf(y0, y1) : fmaxf(x0, x1);
            s->c = s->vert ? x0 : y0;
            s->wire = w;
            s->net = W->net;
        }
    }
    return n;
}

/* the drawing is clear: no two signals touch but by crossing squarely, no wire runs through a part; crossings */
static int check(const lg_level *L, int *crossings)
{
    static lg_seg S[LG_WIRES * 3];
    int n = segments(L, S), cross = 0;
    for (int i = 0; i < n; i++) {
        const lg_seg *s = &S[i];
        if (s->vert)
            for (int k = 0; k < L->nn; k++) {
                const lg_node *N = &L->n[k];
                if (k == L->w[s->wire].from || k == L->w[s->wire].to) continue;
                float nx = N->col + 0.5f;
                if (fabsf(s->c - nx) < LG_HALF + 0.03f && s->a < N->row + LG_NH + 0.02f && s->b > N->row - 0.02f)
                    return 0;
            }
        for (int j = i + 1; j < n; j++) {
            const lg_seg *t = &S[j];
            if (s->net == t->net) continue;
            if (s->vert == t->vert) {
                float gap = s->vert ? 0.07f : 0.05f;
                if (fabsf(s->c - t->c) < gap && s->a < t->b + 0.04f && t->a < s->b + 0.04f) return 0;
                continue;
            }
            const lg_seg *h = s->vert ? t : s, *v = s->vert ? s : t;
            if (v->c < h->a - 0.04f || v->c > h->b + 0.04f || h->c < v->a - 0.04f || h->c > v->b + 0.04f) continue;
            if (v->c < h->a + 0.06f || v->c > h->b - 0.06f || h->c < v->a + 0.06f || h->c > v->b - 0.06f) return 0;
            cross++;
        }
    }
    *crossings = cross;
    return 1;
}

/* a cuttable wire's fuse: the longest stretch of it that is its own (no other wire of its signal along it) and clear of
 * crossings; 0 when there is none long enough */
#define FUSE_ROOM 0.32f
static int place_fuse(lg_level *L, int w)
{
    static lg_seg S[LG_WIRES * 3];
    int n = segments(L, S);
    lg_wire *W = &L->w[w];
    float best = 0, along = 0;
    for (int i = 0; i + 1 < W->npt; i++) {
        float x0 = W->px[i], y0 = W->py[i], x1 = W->px[i + 1], y1 = W->py[i + 1];
        int vert = fabsf(x0 - x1) < EPS;
        float a = vert ? fminf(y0, y1) : fminf(x0, x1), b = vert ? fmaxf(y0, y1) : fmaxf(x0, x1),
              c = vert ? x0 : y0, len = b - a;
        if (len < EPS) continue;
        /* the blocked spans along it */
        float lo[LG_WIRES * 3 + 2], hi[LG_WIRES * 3 + 2];
        int nb = 0;
        lo[nb] = a - 1;
        hi[nb++] = a + 0.07f;
        lo[nb] = b - 0.07f;
        hi[nb++] = b + 1;
        for (int k = 0; k < n; k++) {
            const lg_seg *t = &S[k];
            if (t->wire == w) continue;
            if (t->vert == vert) {
                if (fabsf(t->c - c) < 0.05f && t->a < b && t->b > a) { lo[nb] = t->a - 0.05f; hi[nb++] = t->b + 0.05f; }
            } else if (t->c > a - 0.1f && t->c < b + 0.1f && c > t->a - 0.05f && c < t->b + 0.05f) {
                lo[nb] = t->c - 0.1f;
                hi[nb++] = t->c + 0.1f;
            }
        }
        /* the free spans: sweep the blocked ones in order */
        for (int p = 0; p < nb; p++)
            for (int q = p + 1; q < nb; q++)
                if (lo[q] < lo[p]) {
                    float tl = lo[p], th = hi[p];
                    lo[p] = lo[q];
                    hi[p] = hi[q];
                    lo[q] = tl;
                    hi[q] = th;
                }
        float reach = a - 1;
        for (int p = 0; p <= nb; p++) {
            float start = reach, end = p < nb ? lo[p] : b + 1;
            if (end > start && end - start > best) {
                float m = (start + end) / 2;
                best = end - start;
                W->fvert = vert;
                W->fx = vert ? c : m;
                W->fy = vert ? m : c;
                /* how far along the wire */
                float d = vert ? fabsf(m - y0) : fabsf(m - x0);
                W->fs = along + d;
            }
            if (p < nb && hi[p] > reach) reach = hi[p];
        }
        along += len;
    }
    return best >= FUSE_ROOM;
}

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

/* the wires' points for given tracks (track[w]: in the gap under the wire's destination; -1: straight up) */
static void points(lg_level *L, const int *track)
{
    for (int w = 0; w < L->nw; w++) {
        lg_wire *W = &L->w[w];
        const lg_node *F = &L->n[W->from], *D = &L->n[W->to];
        float xs = F->col + 0.5f + out_off(W->port), ys = F->row + LG_NH;
        float xd = D->col + 0.5f + in_off(D->nin, W->slot), yd = D->row;
        if (track[w] < 0) {
            W->npt = 2;
            W->px[0] = xs; W->py[0] = ys;
            W->px[1] = xd; W->py[1] = yd;
        } else {
            int k = D->row - 1;
            float ty = k + LG_NH + (1 - LG_NH) * (track[w] + 1) / (L->tracks[k] + 1);
            W->npt = 4;
            W->px[0] = xs; W->py[0] = ys;
            W->px[1] = xs; W->py[1] = ty;
            W->px[2] = xd; W->py[2] = ty;
            W->px[3] = xd; W->py[3] = yd;
        }
        W->len = 0;
        for (int i = 0; i + 1 < W->npt; i++) W->len += fabsf(W->px[i + 1] - W->px[i]) + fabsf(W->py[i + 1] - W->py[i]);
    }
}

int lg_route(lg_level *L)
{
    /* the groups: in each gap, the wires of one signal going sideways share a track (a cuttable wire has its own) */
    int group[LG_WIRES], ng = 0, gk[LG_WIRES];
    float ga[LG_WIRES], gb[LG_WIRES];
    for (int w = 0; w < L->nw; w++) {
        lg_wire *W = &L->w[w];
        const lg_node *F = &L->n[W->from], *D = &L->n[W->to];
        float xs = F->col + 0.5f + out_off(W->port), xd = D->col + 0.5f + in_off(D->nin, W->slot);
        group[w] = -1;
        if (fabsf(xs - xd) < EPS) continue;
        int k = D->row - 1, g = -1;
        if (!W->cuttable)
            for (int v = 0; v < w; v++)
                if (group[v] >= 0 && !L->w[v].cuttable && L->w[v].net == W->net && gk[group[v]] == k) g = group[v];
        if (g < 0) {
            g = ng++;
            gk[g] = k;
            ga[g] = gb[g] = xs;
        }
        group[w] = g;
        ga[g] = fminf(ga[g], fminf(xs, xd));
        gb[g] = fmaxf(gb[g], fmaxf(xs, xd));
    }
    int best = -1, best_cross = 1 << 30, gtrack[LG_WIRES], best_track[LG_WIRES], best_tracks[LG_ROWS];
    for (int attempt = 0; attempt < 64; attempt++) {
        int order[LG_WIRES];
        for (int g = 0; g < ng; g++) order[g] = g;
        for (int i = 1; i < ng; i++) {          /* attempt 0: from the left; then shuffled */
            int g = order[i], j = i;
            uint32_t kg = attempt ? mix((uint32_t)(attempt * 977 + g)) : (uint32_t)(ga[g] * 1000);
            while (j > 0) {
                int h = order[j - 1];
                uint32_t kh = attempt ? mix((uint32_t)(attempt * 977 + h)) : (uint32_t)(ga[h] * 1000);
                if (kh <= kg) break;
                order[j] = h;
                j--;
            }
            order[j] = g;
        }
        int ok = 1;
        memset(L->tracks, 0, sizeof L->tracks);
        for (int i = 0; i < ng; i++) {
            int g = order[i], t = 0;
            for (;; t++) {
                int clash = 0;
                for (int j = 0; j < i && !clash; j++) {
                    int h = order[j];
                    clash = gk[h] == gk[g] && gtrack[h] == t && ga[h] < gb[g] + 0.12f && ga[g] < gb[h] + 0.12f;
                }
                if (!clash) break;
            }
            gtrack[g] = t;
            if (t + 1 > L->tracks[gk[g]]) L->tracks[gk[g]] = t + 1;
            if (t >= LG_TRACKS) ok = 0;
        }
        if (!ok) continue;
        int track[LG_WIRES];
        for (int w = 0; w < L->nw; w++) track[w] = group[w] < 0 ? -1 : gtrack[group[w]];
        points(L, track);
        int cross;
        if (!check(L, &cross)) continue;
        int fuses = 1;
        for (int w = 0; w < L->nw && fuses; w++)
            if (L->w[w].cuttable) fuses = place_fuse(L, w);
        if (!fuses) continue;
        if (cross < best_cross) {
            best_cross = cross;
            best = attempt;
            memcpy(best_track, track, sizeof track);
            memcpy(best_tracks, L->tracks, sizeof best_tracks);
        }
        if (!cross) break;
    }
    if (best < 0) return 0;
    memcpy(L->tracks, best_tracks, sizeof best_tracks);
    points(L, best_track);
    for (int w = 0; w < L->nw; w++)
        if (L->w[w].cuttable) place_fuse(L, w);
    L->crossings = best_cross;
    return 1;
}

/* ---- the values -------------------------------------------------------------------------------------------------- */
int lg_eval(const lg_level *L, const uint8_t *val, const uint8_t *cut, uint8_t out[][2], uint8_t *wsrc,
            uint8_t *wend)
{
    int ok = 1;
    for (int i = 0; i < L->nn; i++) {
        const lg_node *n = &L->n[i];
        uint8_t in[3] = {0, 0, 0};
        for (int s = 0; s < n->nin; s++) {
            int w = n->in[s];
            const lg_wire *W = &L->w[w];
            uint8_t v = out[W->from][W->port == 2];
            if (wsrc) wsrc[w] = v;
            if (cut[w]) v = 0;
            if (wend) wend[w] = v;
            in[s] = v;
        }
        out[i][1] = 0;
        switch (n->kind) {
        case LG_SWITCH: out[i][0] = val[i] != 0; break;
        case LG_ROUTE:
            out[i][0] = !val[i] && in[0];
            out[i][1] = val[i] && in[0];
            break;
        case LG_LAMP: out[i][0] = in[0]; ok &= in[0]; break;
        case LG_ALARM: out[i][0] = in[0]; ok &= !in[0]; break;
        default: out[i][0] = (uint8_t)lg_gate(n->kind, in, n->nin);
        }
    }
    return ok;
}

uint32_t lg_mask(const lg_level *L, const uint8_t *val, const uint8_t *cut)
{
    uint32_t m = 0;
    for (int v = 0; v < L->nvar; v++) {
        int i = L->var_idx[v], on = L->var_kind[v] == LG_MSWITCH ? val[L->sw[i]]
                                    : L->var_kind[v] == LG_MROUTE ? val[L->rt[i]] : cut[L->cut[i]];
        if (on) m |= 1u << v;
    }
    return m;
}

static void apply(const lg_level *L, uint32_t m, uint8_t *val, uint8_t *cut)
{
    for (int v = 0; v < L->nvar; v++) {
        int i = L->var_idx[v], on = m >> v & 1;
        if (L->var_kind[v] == LG_MSWITCH) val[L->sw[i]] = (uint8_t)on;
        else if (L->var_kind[v] == LG_MROUTE) val[L->rt[i]] = (uint8_t)on;
        else cut[L->cut[i]] = (uint8_t)on;
    }
}

static int bits(uint32_t m)
{
    int n = 0;
    for (; m; m &= m - 1) n++;
    return n;
}

void lg_solve(const lg_level *L, const uint8_t *val, const uint8_t *cut, lg_answer *a)
{
    uint8_t v[LG_NODES], c[LG_WIRES], out[LG_NODES][2];
    memcpy(v, val, sizeof v);
    memcpy(c, cut, sizeof c);
    uint32_t base = lg_mask(L, val, cut), cuts = 0;
    for (int i = 0; i < L->nvar; i++)
        if (L->var_kind[i] == LG_MCUT) cuts |= 1u << i;
    uint32_t must = base & cuts;
    a->par = -1;
    a->nsol = a->nconf = 0;
    a->best = 0;
    for (uint32_t m = 0; m < 1u << L->nvar; m++) {
        if ((m & must) != must || bits(m & cuts) > L->cut_limit) continue;
        apply(L, m, v, c);
        a->nconf++;
        if (!lg_eval(L, v, c, out, NULL, NULL)) continue;
        a->nsol++;
        int d = bits(m ^ base);
        if (a->par < 0 || d < a->par) { a->par = d; a->best = m; }
    }
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void settle(lg_game *g) { lg_eval(&g->L, g->val, g->cut, g->out, g->wsrc, g->wend); }

void lg_restart(lg_game *g)
{
    for (int i = 0; i < g->L.nn; i++) g->val[i] = g->L.n[i].init;
    memset(g->cut, 0, sizeof g->cut);
    g->nh = 0;
    g->hinted = 0;
    settle(g);
}

void lg_load(lg_game *g, const lg_level *L, int level, int par)
{
    if (&g->L != L) g->L = *L;
    g->level = level;
    g->par = par;
    lg_restart(g);
}

int lg_open(lg_game *g, int level)
{
    if (level < 0 || level >= LG_LEVELS) return 0;
    static lg_level L;
    if (!lg_parse(lg_levels[level], &L, NULL, 0)) return 0;
    lg_load(g, &L, level, lg_pars[level]);
    return 1;
}

int lg_solved(const lg_game *g)
{
    for (int i = 0; i < g->L.nn; i++) {
        int k = g->L.n[i].kind;
        if ((k == LG_LAMP && !g->out[i][0]) || (k == LG_ALARM && g->out[i][0])) return 0;
    }
    return 1;
}

int lg_cuts(const lg_game *g)
{
    int n = 0;
    for (int i = 0; i < g->L.ncut; i++) n += g->cut[g->L.cut[i]];
    return n;
}

static int flip(lg_game *g, lg_move m, int undoing)
{
    const lg_level *L = &g->L;
    switch (m.kind) {
    case LG_MSWITCH:
        if (m.idx >= L->nsw || L->n[L->sw[m.idx]].locked) return 0;
        g->val[L->sw[m.idx]] ^= 1;
        return 1;
    case LG_MROUTE:
        if (m.idx >= L->nrt) return 0;
        g->val[L->rt[m.idx]] ^= 1;
        return 1;
    case LG_MCUT:
        if (m.idx >= L->ncut) return 0;
        if (undoing) { g->cut[L->cut[m.idx]] = 0; return 1; }
        if (g->cut[L->cut[m.idx]] || lg_cuts(g) >= L->cut_limit) return 0;
        g->cut[L->cut[m.idx]] = 1;
        return 1;
    }
    return 0;
}

int lg_play(lg_game *g, lg_move m)
{
    if (lg_solved(g) || g->nh >= LG_HIST || !flip(g, m, 0)) return 0;
    g->hist[g->nh++] = m;
    settle(g);
    return 1;
}

int lg_undo(lg_game *g)
{
    if (!g->nh) return 0;
    flip(g, g->hist[--g->nh], 1);
    settle(g);
    return 1;
}

int lg_stars(const lg_game *g)
{
    int s = g->nh <= g->par ? 3 : g->nh <= g->par + 2 ? 2 : 1;
    return g->hinted && s > 2 ? 2 : s;
}

int lg_hint(const lg_game *g, lg_move *m)
{
    lg_answer a;
    lg_solve(&g->L, g->val, g->cut, &a);
    if (a.par <= 0) return 0;
    uint32_t diff = a.best ^ lg_mask(&g->L, g->val, g->cut);
    for (int v = 0; v < g->L.nvar; v++)
        if (diff >> v & 1) {
            m->kind = g->L.var_kind[v];
            m->idx = g->L.var_idx[v];
            return 1;
        }
    return 0;
}

int lg_script(lg_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        char c = *s++;
        if (c == 'u') { if (!lg_undo(g)) return 0; continue; }
        if (c == 'x') { lg_restart(g); continue; }
        if (c == 'h' || c == '=') {
            lg_move m;
            int once = c == 'h';
            if (once) g->hinted = 1;
            do {
                if (!lg_hint(g, &m) || !lg_play(g, m)) return 0;
            } while (!once && !lg_solved(g));
            continue;
        }
        char *end;
        long n = strtol(s, &end, 10);
        if (end == s || n < 1) return 0;
        s = end;
        if (c == 'L') { if (!lg_open(g, (int)n - 1)) return 0; continue; }
        lg_move m = {c == 's' ? LG_MSWITCH : c == 'r' ? LG_MROUTE : LG_MCUT, (uint8_t)(n - 1)};
        if ((c != 's' && c != 'r' && c != 'c') || !lg_play(g, m)) return 0;
    }
    return 1;
}

/* ---- the generator ----------------------------------------------------------------------------------------------- */
typedef struct {
    int kind, col, row, nin, alive, init, locked;
    int src[3], port[3], cut[3];
    int used[2];
} gnode;

static uint32_t next(uint32_t *s)
{
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}
static int rn(uint32_t *s, int n) { return n > 0 ? (int)(next(s) % (uint32_t)n) : 0; }

/* the trunk of (node, port) can rise to the gap under row r: nothing on its column in between */
static int clear_up(gnode *N, int occ[LG_COLS][LG_ROWS], int f, int r)
{
    for (int rr = N[f].row + 1; rr < r; rr++)
        if (occ[N[f].col][rr] >= 0) return 0;
    return 1;
}

static float gx(const gnode *N, int f, int port) { return N[f].col + 0.5f + out_off(port); }

int lg_generate(uint32_t seed, const lg_params *p, char *out, int size)
{
    uint32_t s = mix(seed * 2654435761u + 0x9e3779b9u) | 1u;
    for (int i = 0; i < 4; i++) next(&s);
    gnode N[LG_NODES];
    int nn = 0, occ[LG_COLS][LG_ROWS], cols = p->cols, rows = p->rows;
    if (cols > LG_COLS || rows > LG_ROWS || rows < 3 || p->sw < 1) return 0;
    memset(occ, -1, sizeof occ);
    memset(N, 0, sizeof N);
    /* the switches, spread over the bottom row */
    int colsw[LG_COLS], nsw = p->sw < cols ? p->sw : cols;
    for (int c = 0; c < cols; c++) colsw[c] = c < nsw;
    for (int c = cols - 1; c > 0; c--) {
        int k = rn(&s, c + 1), t = colsw[c];
        colsw[c] = colsw[k];
        colsw[k] = t;
    }
    for (int c = 0; c < cols; c++)
        if (colsw[c]) {
            N[nn] = (gnode){.kind = LG_SWITCH, .col = c, .row = 0, .alive = 1, .init = rn(&s, 2)};
            occ[c][0] = nn++;
        }
    int routes = p->routes, need = p->lamps + p->alarm, kinds[LG_KINDS], nk = 0;
    for (int k = LG_NOT; k <= LG_NOR; k++)
        if (p->kinds >> k & 1) kinds[nk++] = k;
    if (!nk) return 0;
    for (int r = 1; r < rows - 1; r++) {
        int left = rows - 1 - r, placed = 0;
        for (int tries = 0; tries < 40 && placed < p->gates && nn < LG_NODES - 3; tries++) {
            int unused = 0;
            for (int i = 0; i < nn; i++)
                if (N[i].alive && N[i].kind != LG_LAMP)
                    for (int q = 0; q < (N[i].kind == LG_ROUTE ? 2 : 1); q++) unused += !N[i].used[q];
            int target = need + (unused - need) * (left - 1) / left;
            if (unused <= target && placed) break;
            int kind, nin;
            if (routes > 0 && rn(&s, 100) < 45) kind = LG_ROUTE, nin = 1;
            else {
                kind = kinds[rn(&s, nk)];
                if (kind == LG_NOT && rn(&s, 2)) kind = kinds[rn(&s, nk)];   /* fewer NOTs */
                nin = kind == LG_NOT ? 1 : rn(&s, 100) < p->three ? 3 : 2;
            }
            /* the first input: an unused output, the lowest first */
            int cf[LG_NODES * 2], cp[LG_NODES * 2], nc = 0, low = 99;
            for (int i = 0; i < nn; i++)
                for (int q = 0; q < (N[i].kind == LG_ROUTE ? 2 : 1); q++)
                    if (N[i].alive && N[i].row < r && !N[i].used[q] && clear_up(N, occ, i, r)) {
                        if (N[i].row < low) { low = N[i].row; nc = 0; }
                        if (N[i].row == low) { cf[nc] = i; cp[nc++] = q; }
                    }
            if (!nc) break;
            int pick = rn(&s, nc), src[3] = {cf[pick]}, port[3] = {N[cf[pick]].kind == LG_ROUTE ? cp[pick] + 1 : 0};
            if (kind == LG_NOT && N[src[0]].kind == LG_NOT) continue;   /* NOT NOT: nothing */
            float x0 = gx(N, src[0], port[0]);
            int ok = 1;
            for (int k = 1; k < nin && ok; k++) {   /* the others: near it; unused, or (fan) any */
                int fan = rn(&s, 100) < p->fan;
                nc = 0;
                for (int i = 0; i < nn; i++)
                    for (int q = 0; q < (N[i].kind == LG_ROUTE ? 2 : 1); q++) {
                        int pq = N[i].kind == LG_ROUTE ? q + 1 : 0, dup = 0;
                        for (int j = 0; j < k; j++) dup |= src[j] == i;   /* two inputs from one part: dull */
                        if (!N[i].alive || N[i].kind == LG_LAMP || N[i].row >= r || dup) continue;
                        if (fabsf(gx(N, i, pq) - x0) > 2.3f || !clear_up(N, occ, i, r)) continue;
                        if (N[i].used[q] && !fan) continue;
                        cf[nc] = i;
                        cp[nc++] = pq;
                    }
                if (!nc) { ok = 0; break; }
                pick = rn(&s, nc);
                src[k] = cf[pick];
                port[k] = cp[pick];
            }
            if (!ok) continue;
            /* sort the inputs from the left, so the wires need not cross */
            for (int a = 0; a < nin; a++)
                for (int b = a + 1; b < nin; b++)
                    if (gx(N, src[b], port[b]) < gx(N, src[a], port[a])) {
                        int t = src[a]; src[a] = src[b]; src[b] = t;
                        t = port[a]; port[a] = port[b]; port[b] = t;
                    }
            float mx = 0;
            for (int k = 0; k < nin; k++) mx += gx(N, src[k], port[k]);
            int c0 = (int)floorf(mx / nin), col = -1;
            for (int d = 0; d < 3 && col < 0; d++) {
                int a = c0 - d, b = c0 + d, first = rn(&s, 2) ? a : b, second = first == a ? b : a;
                if (first >= 0 && first < cols && occ[first][r] < 0) col = first;
                else if (second >= 0 && second < cols && occ[second][r] < 0) col = second;
            }
            if (col < 0) continue;
            gnode *G = &N[nn];
            *G = (gnode){.kind = kind, .col = col, .row = r, .nin = nin, .alive = 1, .init = rn(&s, 2)};
            for (int k = 0; k < nin; k++) {
                G->src[k] = src[k];
                G->port[k] = port[k];
                N[src[k]].used[port[k] ? port[k] - 1 : 0]++;
            }
            occ[col][r] = nn++;
            placed++;
            if (kind == LG_ROUTE) routes--;
        }
    }
    /* the lamps: on the top row, each fed by an unused output (the highest first) */
    int fed[LG_NODES * 2] = {0};                /* a signal lights one lamp at most */
    for (int l = 0; l < need; l++) {
        int bf = -1, bq = 0, br = -1;
        for (int pass = 0; pass < 2 && bf < 0; pass++)
            for (int i = 0; i < nn; i++)
                for (int q = 0; q < (N[i].kind == LG_ROUTE ? 2 : 1); q++)
                    if (N[i].alive && N[i].kind != LG_LAMP && N[i].kind != LG_ALARM && N[i].kind != LG_SWITCH &&
                        !fed[2 * i + q] && (pass || !N[i].used[q]) && clear_up(N, occ, i, rows - 1) &&
                        (N[i].row > br || (N[i].row == br && rn(&s, 2)))) {
                        bf = i;
                        bq = q;
                        br = N[i].row;
                    }
        if (bf < 0) return 0;
        fed[2 * bf + bq] = 1;
        int c0 = N[bf].col, col = -1;
        for (int d = 0; d < 3 && col < 0; d++)
            for (int sg = -1; sg <= 1 && col < 0; sg += 2)
                if (c0 + sg * d >= 0 && c0 + sg * d < cols && occ[c0 + sg * d][rows - 1] < 0) col = c0 + sg * d;
        if (col < 0) return 0;
        int port = N[bf].kind == LG_ROUTE ? bq + 1 : 0;
        N[nn] = (gnode){.kind = l < p->lamps ? LG_LAMP : LG_ALARM, .col = col, .row = rows - 1, .nin = 1,
                        .alive = 1, .src = {bf}, .port = {port}};
        N[bf].used[bq]++;
        occ[col][rows - 1] = nn++;
    }
    /* what feeds nothing goes */
    for (int changed = 1; changed;) {
        changed = 0;
        for (int i = 0; i < nn; i++) {
            gnode *G = &N[i];
            if (!G->alive || G->kind == LG_LAMP || G->kind == LG_ALARM) continue;
            int np = G->kind == LG_ROUTE ? 2 : 1, idle = 0;
            for (int q = 0; q < np; q++) idle += !G->used[q];
            if (!idle) continue;
            if (G->kind == LG_ROUTE && idle == 1) return 0;
            G->alive = 0;
            occ[G->col][G->row] = -1;
            for (int k = 0; k < G->nin; k++) N[G->src[k]].used[G->port[k] ? G->port[k] - 1 : 0]--;
            changed = 1;
        }
    }
    int alive_sw = 0, gates = 0;
    for (int i = 0; i < nn; i++) {
        alive_sw += N[i].alive && N[i].kind == LG_SWITCH;
        gates += N[i].alive && N[i].kind >= LG_NOT && N[i].kind <= LG_ROUTE;
    }
    if (alive_sw < 2 || !gates) return 0;
    /* empty columns and rows closed up */
    for (int c = cols - 1; c >= 0; c--) {
        int any = 0;
        for (int i = 0; i < nn; i++) any |= N[i].alive && N[i].col == c;
        if (!any)
            for (int i = 0; i < nn; i++)
                if (N[i].col > c) N[i].col--;
    }
    for (int r = rows - 2; r >= 1; r--) {
        int any = 0;
        for (int i = 0; i < nn; i++) any |= N[i].alive && N[i].row == r;
        if (!any)
            for (int i = 0; i < nn; i++)
                if (N[i].row > r) N[i].row--;
    }
    /* locked switches, fuses */
    int lock = p->locked < alive_sw - 2 ? p->locked : alive_sw - 2;
    for (int t = 0; t < 20 && lock > 0; t++) {
        int i = rn(&s, nn);
        if (N[i].alive && N[i].kind == LG_SWITCH && !N[i].locked) { N[i].locked = 1; lock--; }
    }
    int fuses = p->fuses;
    for (int t = 0; t < 60 && fuses > 0; t++) {
        int i = rn(&s, nn);
        if (!N[i].alive || !N[i].nin || N[i].kind == LG_LAMP) continue;   /* a lamp's own wire: no use cutting */
        int k = rn(&s, N[i].nin);
        if (!N[i].cut[k]) { N[i].cut[k] = 1; fuses--; }
    }
    /* the text */
    int len = 0;
    if (p->fuses >= 2 && rn(&s, 3) == 0)          /* fewer cuts than fuses, now and then */
        len += snprintf(out + len, (size_t)(size - len), "%%%d", 1 + rn(&s, p->fuses - 1));
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            for (int i = 0; i < nn; i++) {
                gnode *G = &N[i];
                if (!G->alive || G->col != c || G->row != r) continue;
                const char *name = G->kind == LG_SWITCH ? (G->locked ? (G->init ? "K" : "k") : G->init ? "S" : "s")
                                 : G->kind == LG_ROUTE ? (G->init ? "Y" : "y") : KIND_NAME[G->kind];
                len += snprintf(out + len, (size_t)(size > len ? size - len : 0), "%s%s@%c%d", len ? " " : "", name,
                                'a' + c, r + 1);
                for (int k = 0; k < G->nin; k++) {
                    gnode *F = &N[G->src[k]];
                    len += snprintf(out + len, (size_t)(size > len ? size - len : 0), "%c%c%d%s%s", k ? ',' : '(',
                                    'a' + F->col, F->row + 1, G->port[k] == 1 ? "<" : G->port[k] == 2 ? ">" : "",
                                    G->cut[k] ? "x" : "");
                }
                if (G->nin) len += snprintf(out + len, (size_t)(size > len ? size - len : 0), ")");
            }
    return len < size;
}
