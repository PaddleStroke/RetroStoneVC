/*
 * Train Switches: the rules (trains.h): the track, the switches and toggles, crossings and tunnels, a run one by one
 * or all together (crashes), the search for the fewest flips, the maker of networks, undo, the notation. No SDL:
 * tested by tests/trains_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/trains.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
#define WILD 255                                /* a train's colour while the maker lays the network: any station */

static int bits(unsigned v)
{
    int n = 0;
    for (; v; v >>= 1) n += v & 1;
    return n;
}

static int low_side(unsigned v)
{
    for (int d = 0; d < 4; d++)
        if (v >> d & 1) return d;
    return -1;
}

int trs_neighbour(const trs_map *m, int c, int side)
{
    int x = c % m->w + DX[side & 3], y = c / m->w + DY[side & 3];
    return x < 0 || y < 0 || x >= m->w || y >= m->h ? -1 : x + y * m->w;
}

int trs_switch_at(const trs_map *m, int cell)
{
    for (int i = 0; i < m->ns; i++)
        if (m->sw[i] == cell) return i;
    return -1;
}

int trs_branch(const trs_map *m, int c, int setting)
{
    int n = 0, stem = m->k[c];
    for (int i = 1; i < 4; i++) {
        int d = (stem + i) & 3;
        if (m->open[c] >> d & 1) {
            if (n == setting) return d;
            n++;
        }
    }
    return -1;
}

int trs_exit(const trs_map *m, int c, int in, uint16_t set)
{
    switch (m->kind[c]) {
    case TRS_TRACK: return low_side(m->open[c] & ~(1u << in));
    case TRS_CROSS: return (in + 2) & 3;
    case TRS_SWITCH:
        if (in != m->k[c]) return m->k[c];
        return trs_branch(m, c, set >> trs_switch_at(m, c) & 1);
    default: return -1;
    }
}

static int twin(const trs_map *m, int c)        /* a tunnel mouth's twin */
{
    for (int i = 0; i < m->w * m->h; i++)
        if (i != c && m->kind[i] == TRS_TUNNEL && m->k[i] == m->k[c]) return i;
    return -1;
}

/* ---- maps as text ------------------------------------------------------------------------------------------------ */
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }

int trs_parse(trs_map *m, const char *text)
{
    memset(m, 0, sizeof *m);
    const char *bar = strchr(text, '|'), *sl = strchr(text, '/');
    if (!bar) return 0;
    int first = (int)((sl && sl < bar ? sl : bar) - text);
    if (first % 3 || first < 3 || first / 3 > TRS_MAXW) return 0;
    int w = first / 3, x = 0, y = 0;
    m->w = w;
    const char *p = text;
    for (;;) {
        if (*p == '/' || *p == '|') {
            if (x != w) return 0;
            y++;
            x = 0;
            if (*p++ == '|') break;
            continue;
        }
        if (!p[0] || !p[1] || !p[2] || x >= w || y >= TRS_MAXH) return 0;
        int c = x + y * w, o = hexv(p[1]);
        if (o < 0) return 0;
        m->open[c] = (uint8_t)o;
        char e = p[2];
        switch (p[0]) {
        case '.': m->kind[c] = TRS_EMPTY; break;
        case 't': m->kind[c] = TRS_TRACK; break;
        case 's': case 'S': m->kind[c] = TRS_SWITCH; m->toggle[c] = p[0] == 'S'; m->k[c] = (uint8_t)(e - '0'); break;
        case 'x': m->kind[c] = TRS_CROSS; break;
        case 'p': m->kind[c] = TRS_STATION; m->k[c] = (uint8_t)(e - '0'); break;
        case 'e': m->kind[c] = TRS_PORTAL; break;
        case 'u': m->kind[c] = TRS_TUNNEL; m->k[c] = (uint8_t)(e - '0'); break;
        default: return 0;
        }
        if (m->k[c] > 9) return 0;
        x++;
        p += 3;
    }
    m->h = y;
    if (m->h < 1) return 0;
    for (int c = 0; c < w * m->h; c++) {        /* each kind its sides; each side met by its neighbour's */
        int n = bits(m->open[c]), kd = m->kind[c];
        static const int NEED[] = {0, 2, 3, 4, 1, 1, 1};
        if (n != NEED[kd]) return 0;
        if (kd == TRS_SWITCH && !(m->open[c] >> m->k[c] & 1)) return 0;
        if (kd == TRS_STATION && m->k[c] >= TRS_COLOURS) return 0;
        if (kd == TRS_TUNNEL && (m->k[c] > 3 || twin(m, c) < 0)) return 0;
        for (int d = 0; d < 4; d++)
            if (m->open[c] >> d & 1) {
                int b = trs_neighbour(m, c, d);
                if (b < 0 || !(m->open[b] >> ((d + 2) & 3) & 1)) return 0;
            }
        if (kd == TRS_SWITCH) {
            if (m->ns >= TRS_MAXS) return 0;
            m->sw[m->ns++] = (uint8_t)c;
        }
        if (kd == TRS_PORTAL) {
            if (m->ne >= TRS_MAXE) return 0;
            m->portal[m->ne++] = (uint8_t)c;
        }
    }
    for (int c = 0; c < w * m->h; c++)          /* a pair of mouths, no more */
        if (m->kind[c] == TRS_TUNNEL)
            for (int i = 0, n = 0; i < w * m->h; i++)
                if (m->kind[i] == TRS_TUNNEL && m->k[i] == m->k[c] && ++n > 2) return 0;
    if (*p == 'T') m->together = 1;
    else if (*p != 'S') return 0;
    if (*++p != '|') return 0;
    p++;
    while (*p && *p != '|') {
        if (*p == ',') { p++; continue; }
        if (m->nt >= TRS_MAXT || p[0] < '0' || p[0] >= '0' + TRS_COLOURS || p[1] < '0' || p[1] >= '0' + m->ne)
            return 0;
        m->tcol[m->nt] = (uint8_t)(p[0] - '0');
        m->tfrom[m->nt++] = (uint8_t)(p[1] - '0');
        p += 2;
    }
    if (*p++ != '|' || !m->nt) return 0;
    for (int i = 0; i < m->ns; i++, p++) {
        if (*p != '0' && *p != '1') return 0;
        if (*p == '1') m->set0 |= (uint16_t)(1u << i);
    }
    if (*p) return 0;
    if (m->together)                            /* together: a train a portal */
        for (int i = 0; i < m->nt; i++)
            for (int j = 0; j < i; j++)
                if (m->tfrom[i] == m->tfrom[j]) return 0;
    return 1;
}

void trs_text(const trs_map *m, char *out)
{
    static const char HEX[] = "0123456789abcdef";
    char *o = out;
    for (int y = 0; y < m->h; y++) {
        if (y) *o++ = '/';
        for (int x = 0; x < m->w; x++) {
            int c = x + y * m->w;
            char k = '.', e = '.';
            switch (m->kind[c]) {
            case TRS_TRACK: k = 't'; break;
            case TRS_SWITCH: k = m->toggle[c] ? 'S' : 's'; e = (char)('0' + m->k[c]); break;
            case TRS_CROSS: k = 'x'; break;
            case TRS_STATION: k = 'p'; e = (char)('0' + m->k[c]); break;
            case TRS_PORTAL: k = 'e'; break;
            case TRS_TUNNEL: k = 'u'; e = (char)('0' + m->k[c]); break;
            default: break;
            }
            *o++ = k;
            *o++ = HEX[m->open[c] & 15];
            *o++ = e;
        }
    }
    *o++ = '|';
    *o++ = m->together ? 'T' : 'S';
    *o++ = '|';
    for (int i = 0; i < m->nt; i++) {
        if (i) *o++ = ',';
        *o++ = (char)('0' + m->tcol[i]);
        *o++ = (char)('0' + m->tfrom[i]);
    }
    *o++ = '|';
    for (int i = 0; i < m->ns; i++) *o++ = (char)('0' + (m->set0 >> i & 1));
    *o = 0;
}

/* ---- a run ------------------------------------------------------------------------------------------------------- */
/* a train one square on from (cell, out): 0 on its way, 1 stopped at a station it may stop at, -TRS_* it failed; a
 * toggle it went onto flips (*flipped: its number, else -1) */
static int advance(const trs_map *m, uint16_t *set, int col, int *cell, int *in, int *out, int *flipped)
{
    int c = *cell, o = *out, n, ni;
    *flipped = -1;
    if (o == 5) { n = twin(m, c); ni = 5; }
    else {
        n = trs_neighbour(m, c, o);
        ni = (o + 2) & 3;
        if (n < 0 || !(m->open[n] >> ni & 1)) { *out = 4; return -TRS_DEAD; }
    }
    *cell = n;
    *in = ni;
    switch (m->kind[n]) {
    case TRS_STATION: *out = 4; return col == WILD || m->k[n] == col ? 1 : -TRS_WRONG;
    case TRS_PORTAL: *out = 4; return -TRS_DEAD;
    case TRS_TUNNEL: *out = ni == 5 ? low_side(m->open[n]) : 5; return 0;
    default: {
        int e = trs_exit(m, n, ni, *set);
        if (e < 0) { *out = 4; return -TRS_DEAD; }
        *out = e;
        if (m->kind[n] == TRS_SWITCH && m->toggle[n]) {
            *flipped = trs_switch_at(m, n);
            *set ^= (uint16_t)(1u << *flipped);
        }
        return 0;
    }
    }
}

static void note(trs_run *r, int i, int j, int cell, int in, int out)
{
    if (!r || j >= TRS_PATH) return;
    r->cell[i][j] = (uint8_t)cell;
    r->in[i][j] = (uint8_t)in;
    r->out[i][j] = (uint8_t)out;
    r->len[i] = j + 1;
}

static void note_flip(trs_run *r, int tick, int sw)
{
    if (!r || sw < 0 || r->nflip >= (int)(sizeof r->flip_s)) return;
    r->flip_t[r->nflip] = (short)tick;
    r->flip_s[r->nflip++] = (uint8_t)sw;
}

static int failed(trs_run *r, int res, int tick, int cell, int t1, int t2)
{
    if (r) {
        r->result = res;
        r->ticks = tick;
        r->bad = cell;
        r->bad_t = t1;
        r->bad_t2 = t2;
    }
    return res;
}

int trs_simulate(const trs_map *m, uint16_t set, trs_run *r)
{
    int cell[TRS_MAXT], in[TRS_MAXT], out[TRS_MAXT], fl;
    if (r) {
        r->nflip = 0;
        r->bad = r->bad_t = r->bad_t2 = -1;
        for (int i = 0; i < TRS_MAXT; i++) r->start[i] = r->len[i] = 0;
    }
    for (int i = 0; i < m->nt; i++) {
        cell[i] = m->portal[m->tfrom[i]];
        in[i] = 4;
        out[i] = low_side(m->open[cell[i]]);
    }
    if (!m->together) {
        int t = 0;
        for (int i = 0; i < m->nt; i++) {
            if (r) r->start[i] = t;
            note(r, i, 0, cell[i], in[i], out[i]);
            for (int j = 1;; j++) {
                if (j >= TRS_PATH) return failed(r, TRS_LOOP, t + j - 1, cell[i], i, -1);
                int res = advance(m, &set, m->tcol[i], &cell[i], &in[i], &out[i], &fl);
                note(r, i, j, cell[i], in[i], out[i]);
                note_flip(r, t + j, fl);
                if (res < 0) return failed(r, -res, t + j, cell[i], i, -1);
                if (res) { t += j + 1; break; }
            }
        }
        if (r) { r->result = TRS_OK; r->ticks = t - 1; }
        return TRS_OK;
    }
    int on[TRS_MAXT];
    for (int i = 0; i < m->nt; i++) {
        on[i] = 1;
        note(r, i, 0, cell[i], in[i], out[i]);
    }
    for (int tick = 1;; tick++) {
        int prev[TRS_MAXT], moved[TRS_MAXT], any = 0;
        for (int i = 0; i < m->nt; i++) {
            prev[i] = cell[i];
            moved[i] = on[i];
            any |= on[i];
        }
        if (!any) {
            if (r) { r->result = TRS_OK; r->ticks = tick - 1; }
            return TRS_OK;
        }
        if (tick >= TRS_PATH) {
            int i = 0;
            while (!on[i]) i++;
            return failed(r, TRS_LOOP, tick - 1, cell[i], i, -1);
        }
        for (int i = 0; i < m->nt; i++) {
            if (!on[i]) continue;
            int res = advance(m, &set, m->tcol[i], &cell[i], &in[i], &out[i], &fl);
            note(r, i, tick, cell[i], in[i], out[i]);
            note_flip(r, tick, fl);
            if (res < 0) return failed(r, -res, tick, cell[i], i, -1);
            if (res) on[i] = 0;
        }
        for (int i = 0; i < m->nt; i++)        /* a crash: one square, or passing each other */
            for (int j = i + 1; j < m->nt; j++) {
                if (!moved[i] || !moved[j]) continue;
                if (cell[i] == cell[j] && m->kind[cell[i]] != TRS_STATION)
                    return failed(r, TRS_CRASH, tick, cell[i], i, j);
                if (cell[i] == prev[j] && cell[j] == prev[i])
                    return failed(r, TRS_CRASH, tick, cell[i], i, j);
            }
    }
}

int trs_solve(const trs_map *m, uint16_t set, uint16_t *to, int *count)
{
    int best = -1, n = 0;
    uint16_t bto = 0;
    for (unsigned c = 0; c < 1u << m->ns; c++)
        if (trs_simulate(m, (uint16_t)c, NULL) == TRS_OK) {
            n++;
            int d = bits(c ^ set);
            if (best < 0 || d < best) { best = d; bto = (uint16_t)c; }
        }
    if (to) *to = bto;
    if (count) *count = n;
    return best;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void trs_begin(trs_game *g)
{
    g->set = g->m.set0;
    g->nh = g->moves = g->runs = g->fails = g->hints = g->solved = 0;
}

int trs_flip(trs_game *g, int sw)
{
    if (g->solved || sw < 0 || sw >= g->m.ns || g->nh >= TRS_HIST) return 0;
    g->hist[g->nh++] = g->set;
    g->set ^= (uint16_t)(1u << sw);
    g->moves = g->nh;
    return 1;
}

int trs_undo(trs_game *g)
{
    if (g->solved) { g->solved = 0; return 1; }
    if (!g->nh) return 0;
    g->set = g->hist[--g->nh];
    g->moves = g->nh;
    return 1;
}

int trs_go(trs_game *g, trs_run *r)
{
    if (g->solved) return TRS_OK;
    int res = trs_simulate(&g->m, g->set, r);
    g->runs++;
    if (res == TRS_OK) g->solved = 1;
    else g->fails++;
    return res;
}

int trs_hint(const trs_game *g)
{
    uint16_t to;
    if (g->solved || trs_solve(&g->m, g->set, &to, NULL) <= 0) return -1;
    return __builtin_ctz((unsigned)(to ^ g->set));
}

int trs_script(trs_game *g, const char *s)
{
    while (*s) {
        char c = *s++;
        if (c == ' ' || c == ',') continue;
        if (c == '=') {
            int k = 0, any = 0;
            for (; *s >= '0' && *s <= '9'; s++, any = 1) k = k * 10 + *s - '0';
            for (int i = 0; !any || i < k; i++) {
                int sw = trs_hint(g);
                if (sw < 0) break;
                if (!trs_flip(g, sw)) return 0;
            }
            if (!any && trs_go(g, NULL) != TRS_OK) return 0;
            continue;
        }
        if (c == 'g') { trs_go(g, NULL); continue; }
        if (c < 'a' || c >= 'a' + g->m.w || *s < '1' || *s > '9') return 0;
        int y = 0;
        for (; *s >= '0' && *s <= '9'; s++) y = y * 10 + *s - '0';
        if (y < 1 || y > g->m.h) return 0;
        if (!trs_flip(g, trs_switch_at(&g->m, (c - 'a') + (y - 1) * g->m.w))) return 0;
    }
    return 1;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- *
 * A network laid on the grid: the portals and stations on the border (the track meets them from inside), the first
 * line from a portal to a station; each other station a branch off the track (a switch where it leaves), each other
 * portal a line joining the track (a switch where it meets), loops between two places of the track (two switches),
 * a tunnel now and then (a line going in at one mouth, out at the other). Each line the cheapest way over squares
 * weighted at random, turns costing more (straighter lines), over a straight track at right angles where crossings
 * are wanted. Then a setting drawn at random sends the trains (their colours not yet known) to stations, and each
 * train takes the colour of the station it reached: the puzzle can be solved. The first setting: one at the par
&
typedef struct {
    float wt[TRS_CELLS];
    uint8_t reserved[TRS_CELLS];                /* a portal's or a station's square inside, kept for its own line */
    int crossings, cross_cost;
    int sym;                                    /* each line laid with its mirror image */
    uint32_t rs;
} lay_ctx;

enum { TG_CELL, TG_TRACK };

static int inward(const trs_map *m, int c)      /* a border square's side towards the inside (-1: a corner, inside) */
{
    int x = c % m->w, y = c / m->w, n = 0, d = -1;
    if (y == 0) { d = TRS_DOWN; n++; }
    if (y == m->h - 1) { d = TRS_UP; n++; }
    if (x == 0) { d = TRS_RIGHT; n++; }
    if (x == m->w - 1) { d = TRS_LEFT; n++; }
    return n == 1 ? d : -1;
}

static int plain_track(const trs_map *m, int c) { return m->kind[c] == TRS_TRACK && bits(m->open[c]) == 2; }

static int mir(const trs_map *m, int c) { return m->w - 1 - c % m->w + c / m->w * m->w; }
static int mir_side(int d) { return d == TRS_LEFT ? TRS_RIGHT : d == TRS_RIGHT ? TRS_LEFT : d; }

/* a square a line may be laid over: empty, not kept for another line (allow: the one it may use); with a mirror,
 * its image too */
static int free_sq(const trs_map *m, const lay_ctx *L, int n, int allow)
{
    for (int k = 0; k < (L->sym ? 2 : 1); k++) {
        int q = k ? mir(m, n) : n;
        if (m->kind[q] != TRS_EMPTY) return 0;
        if (L->reserved[q] && q != allow && !(L->sym && allow >= 0 && q == mir(m, allow))) return 0;
    }
    return 1;
}

/* the cheapest line from one of the starts (square sc[i], stepping the way sd[i]) to the target: square `tcell`
 * entered the way `tdir` (-1 any), or (TG_TRACK) a plain track entered by a free side, not the start's square; the
 * line laid; 0 none */
static int lay(trs_map *m, lay_ctx *L, const int *sc, const int *sd, const float *s0, int ns, int tk, int tcell,
               int tdir, int allow)
{
    enum { V = TRS_CELLS * 4 };
    static float dist[V];
    static short prev[V];
    static uint8_t done[V], cross[V];
    int W = m->w * m->h;
    for (int v = 0; v < V; v++) { dist[v] = 1e9f; prev[v] = -1; done[v] = 0; cross[v] = 0; }
    float best = 1e9f;
    int bv = -1, bn = -1, bsrc = -1;
    /* a state: square c reached stepping the way d (c an empty square laid as track, or a track crossed); prev: the
     * state before, -2 - i from start i */
#define FREE(n) free_sq(m, L, n, allow)
#define IS_TARGET(n, d, src) ((tk == TG_CELL && (n) == tcell && (tdir < 0 || (d) == tdir)) ||                      \
                              (tk == TG_TRACK && plain_track(m, n) && !(m->open[n] >> (((d) + 2) & 3) & 1) &&       \
                               (n) != (src) && (!L->sym || mir(m, n) != (n))))
    for (int i = 0; i < ns; i++) {
        int n = trs_neighbour(m, sc[i], sd[i]);
        if (n < 0) continue;
        if (IS_TARGET(n, sd[i], sc[i])) {
            if (s0[i] + 1 < best) { best = s0[i] + 1; bv = -2 - i; bn = n; bsrc = i; }
            continue;
        }
        if (!FREE(n)) continue;
        int v = n * 4 + sd[i];
        float cost = s0[i] + 1 + L->wt[n];
        if (cost < dist[v]) { dist[v] = cost; prev[v] = (short)(-2 - i); }
    }
    for (;;) {
        int u = -1;
        for (int v = 0; v < W * 4; v++)
            if (!done[v] && dist[v] < 1e8f && (u < 0 || dist[v] < dist[u])) u = v;
        if (u < 0 || dist[u] >= best) break;
        done[u] = 1;
        int c = u / 4, d = u % 4;
        int src = u;                            /* the start this state came from */
        while (prev[src] >= 0) src = prev[src];
        int scell = sc[-2 - prev[src]];
        for (int t = -1; t <= 1; t++) {
            if (cross[u] && t) continue;
            int nd = (d + t + 4) & 3, n = trs_neighbour(m, c, nd);
            if (n < 0) continue;
            float cost = dist[u] + 1 + (t ? 1.1f : 0);
            if (IS_TARGET(n, nd, scell)) {
                if (cost < best) { best = cost; bv = u; bn = n; bsrc = -1; }
                continue;
            }
            int v = n * 4 + nd, cr = 0;
            if (FREE(n)) cost += L->wt[n];
            else if (L->crossings && plain_track(m, n) && m->open[n] == (nd & 1 ? 5 : 10)) {
                cost += L->wt[n] + L->cross_cost;
                cr = 1;
            } else continue;
            if (cost < dist[v]) { dist[v] = cost; prev[v] = (short)u; cross[v] = (uint8_t)cr; }
        }
    }
#undef FREE
#undef IS_TARGET
    if (bn < 0) return 0;
    int path[V], np = 0, v = bv;                /* the states back to the start */
    while (v >= 0) { path[np++] = v; v = prev[v]; }
    int start = bsrc >= 0 ? bsrc : -2 - v;
    int seen[TRS_CELLS] = {0};
    for (int i = 0; i < np; i++) {
        if (seen[path[i] / 4]) return 0;        /* through a square twice: not laid */
        seen[path[i] / 4] = 1;
    }
    if (seen[bn] || seen[sc[start]]) return 0;
    int a = sc[start], d = sd[start], sa[V + 1], sb[V + 1], sdir[V + 1], scr[V + 1], nst = 0;
    for (int i = np - 1; i >= -1; i--) {         /* each step: a to b, the way d */
        int b = i >= 0 ? path[i] / 4 : bn, bd = i >= 0 ? path[i] % 4 : -1;
        if (i < 0) {                            /* the last step into the target: from a */
            int dd = -1;
            for (int k = 0; k < 4; k++)
                if (trs_neighbour(m, a, k) == b) dd = k;
            d = dd;
        } else d = bd;
        sa[nst] = a;
        sb[nst] = b;
        sdir[nst] = d;
        scr[nst++] = i >= 0 ? (m->kind[b] == TRS_TRACK ? 2 : 1) : 0;   /* 1 a new track, 2 a crossing, 0 the end */
        a = b;
    }
    for (int k = 0; k < (L->sym ? 2 : 1); k++)  /* the line, then its mirror image */
        for (int i = 0; i < nst; i++) {
            int A = k ? mir(m, sa[i]) : sa[i], B = k ? mir(m, sb[i]) : sb[i], D = k ? mir_side(sdir[i]) : sdir[i];
            m->open[A] |= (uint8_t)(1u << D);
            m->open[B] |= (uint8_t)(1u << ((D + 2) & 3));
            if (k && B == sb[i]) continue;       /* a square on the middle: itself */
            if (scr[i] == 2) m->kind[B] = TRS_CROSS;
            else if (scr[i] == 1) m->kind[B] = TRS_TRACK;
        }
    return 1;
}

/* the starts off the track: each plain track's free side onto a free square (a branch), cost a little at random */
static int branch_starts(const trs_map *m, lay_ctx *L, int *sc, int *sd, float *s0)
{
    int n = 0;
    for (int c = 0; c < m->w * m->h; c++) {
        if (!plain_track(m, c)) continue;
        for (int d = 0; d < 4; d++) {
            int b = trs_neighbour(m, c, d);
            if ((m->open[c] >> d & 1) || b < 0 || m->kind[b] != TRS_EMPTY || L->reserved[b]) continue;
            sc[n] = c;
            sd[n] = d;
            s0[n++] = (float)pr_below(&L->rs, 40) / 10;
        }
    }
    return n;
}

static int far_cell(const trs_map *m, lay_ctx *L, int from)   /* an empty square inside, away from `from` */
{
    for (int k = 0; k < 60; k++) {
        int c = pr_below(&L->rs, m->w * m->h), x = c % m->w, y = c / m->w;
        if (x < 1 || y < 1 || x >= m->w - 1 || y >= m->h - 1 || m->kind[c] != TRS_EMPTY || L->reserved[c]) continue;
        if (from >= 0 && abs(x - from % m->w) + abs(y - from / m->w) < 3) continue;
        int free = 0;
        for (int d = 0; d < 4; d++) {
            int b = trs_neighbour(m, c, d);
            free += b >= 0 && m->kind[b] == TRS_EMPTY && !L->reserved[b];
        }
        if (free >= 2) return c;
    }
    return -1;
}

/* a line from the starts to the target (as lay()), through a tunnel: in at a mouth, out at its twin, pair `pair` */
static int lay_tunnel(trs_map *m, lay_ctx *L, const int *sc, const int *sd, const float *s0, int ns, int tk,
                      int tcell, int tdir, int allow, int pair)
{
    int a = far_cell(m, L, -1);
    if (a < 0) return 0;
    L->reserved[a] = 1;
    int b = far_cell(m, L, a);
    L->reserved[a] = 0;
    if (b < 0) return 0;
    L->reserved[b] = 1;
    m->kind[a] = TRS_TUNNEL;                    /* a target the line enters from any side */
    m->k[a] = (uint8_t)pair;
    int ok = lay(m, L, sc, sd, s0, ns, TG_CELL, a, -1, allow);
    L->reserved[b] = 0;
    if (!ok) return 0;
    m->kind[b] = TRS_TUNNEL;
    m->k[b] = (uint8_t)pair;
    int bs[4], bd[4], n = 0;
    float b0[4] = {0};
    for (int d = 0; d < 4; d++) {
        int nb = trs_neighbour(m, b, d);
        if (nb >= 0 && m->kind[nb] == TRS_EMPTY && !L->reserved[nb]) { bs[n] = b; bd[n++] = d; }
    }
    for (int i = 0; i < n; i++) b0[i] = (float)pr_below(&L->rs, 30) / 10;
    return n && lay(m, L, bs, bd, b0, n, tk, tcell, tdir, allow);
}

/* the squares' distances along the track from the portals (through the tunnels too): a switch's natural stem faces
 * the nearer side */
static void track_dist(const trs_map *m, int *dist)
{
    int q[TRS_CELLS], n = 0, W = m->w * m->h;
    for (int c = 0; c < W; c++) dist[c] = -1;
    for (int c = 0; c < W; c++)
        if (m->kind[c] == TRS_PORTAL) { dist[c] = 0; q[n++] = c; }
    for (int h = 0; h < n; h++) {
        int c = q[h];
        for (int d = 0; d < 4; d++) {
            int b = trs_neighbour(m, c, d);
            if ((m->open[c] >> d & 1) && b >= 0 && dist[b] < 0) { dist[b] = dist[c] + 1; q[n++] = b; }
        }
        if (m->kind[c] == TRS_TUNNEL) {
            int t = twin(m, c);
            if (t >= 0 && dist[t] < 0) { dist[t] = dist[c] + 1; q[n++] = t; }
        }
    }
}

/* a laid network made a puzzle's: the switches and their stems (facing the portals, at random; mirrored), the
 * toggles, the trains' portals; 0: not a network */
static int finish(trs_map *m, const trs_spec *sp, lay_ctx *L)
{
    int W = m->w * m->h, dist[TRS_CELLS];
    track_dist(m, dist);
    m->ns = m->ne = 0;
    m->together = sp->together;
    for (int c = 0; c < W; c++) {
        int n = bits(m->open[c]);
        if (m->kind[c] == TRS_TRACK && n == 3) {
            m->kind[c] = TRS_SWITCH;
            int x = c % m->w, d = -1;
            if (L->sym && 2 * x + 1 > m->w) d = mir_side(m->k[mir(m, c)]);   /* the mirror of its twin's */
            else if (L->sym && 2 * x + 1 == m->w) {   /* on the middle: its stem up or down */
                for (int k = 0; k < 4; k += 2)
                    if (m->open[c] >> k & 1) d = k;
            } else if (pr_below(&L->rs, 100) < sp->natural) {
                int near = 0, far = 0, dn = -1, df = -1;   /* the odd one out: one side nearer, or one farther */
                for (int k = 0; k < 4; k++) {
                    int b = trs_neighbour(m, c, k);
                    if (!(m->open[c] >> k & 1) || b < 0) continue;
                    if (dist[b] >= 0 && dist[b] < dist[c]) { near++; dn = k; }
                    else { far++; df = k; }
                }
                d = near == 1 ? dn : far == 1 ? df : -1;
            }
            if (d < 0 || !(m->open[c] >> d & 1))
                do d = pr_below(&L->rs, 4); while (!(m->open[c] >> d & 1));
            m->k[c] = (uint8_t)d;
            if (m->ns >= TRS_MAXS) return 0;
            m->sw[m->ns++] = (uint8_t)c;
        } else if (m->kind[c] == TRS_TRACK && n != 2) return 0;
        if (m->kind[c] == TRS_PORTAL) {
            if (m->ne >= TRS_MAXE) return 0;
            m->portal[m->ne++] = (uint8_t)c;
        }
        if (m->kind[c] == TRS_TUNNEL && n != 1) return 0;
        if (m->kind[c] == TRS_CROSS && m->open[c] != 15) return 0;
        if ((m->kind[c] == TRS_PORTAL || m->kind[c] == TRS_STATION) && n != 1) return 0;
        if (m->kind[c] == TRS_EMPTY && n) return 0;
    }
    for (int c = 0; c < W; c++)                 /* every side met */
        for (int d = 0; d < 4; d++)
            if (m->open[c] >> d & 1) {
                int b = trs_neighbour(m, c, d);
                if (b < 0 || !(m->open[b] >> ((d + 2) & 3) & 1)) return 0;
            }
    if (m->ns < sp->switches || m->ns < 1 || !m->ne) return 0;
    int crosses = 0;
    for (int c = 0; c < W; c++) crosses += m->kind[c] == TRS_CROSS;
    if (crosses < sp->crossings) return 0;
    int ord[TRS_MAXS], nt = 0;                  /* the toggles (a mirrored pair: both) */
    for (int i = 0; i < m->ns; i++)
        if (!L->sym || 2 * (m->sw[i] % m->w) + 1 <= m->w) ord[nt++] = i;
    pr_shuffle(&L->rs, ord, nt);
    for (int t = 0; t < sp->toggles && t < nt; t++) {
        int c = m->sw[ord[t]];
        m->toggle[c] = 1;
        if (L->sym) m->toggle[mir(m, c)] = 1;
    }
    /* the trains: a portal each at least, together a portal each exactly */
    m->nt = sp->together || !sp->toggles ? m->ne : sp->trains;   /* no toggles: a portal's trains would all go alike */
    if (m->nt > TRS_MAXT || m->nt < m->ne) return 0;
    int from[TRS_MAXT];
    for (int i = 0; i < m->nt; i++) from[i] = i < m->ne ? i : pr_below(&L->rs, m->ne);
    pr_shuffle(&L->rs, from, m->nt);
    for (int i = 0; i < m->nt; i++) { m->tfrom[i] = (uint8_t)from[i]; m->tcol[i] = WILD; }
    return 1;
}

/* a place on the border for a portal or a station (half: on the left half, mirrored later; mid: on the middle) */
static int border_place(trs_map *m, lay_ctx *L, int is_port, int half, int mid, const int *placed)
{
    int W = m->w * m->h;
    for (int tries = 0; tries < 200; tries++) {
        int e = pr_below(&L->rs, 10), q, wx = half ? m->w / 2 - 1 : m->w - 2;
        if (wx < 1) wx = 1;
        if (mid) q = m->w / 2 + (is_port ? 0 : (m->h - 1) * m->w);
        else if (is_port)
            q = e < 6 ? 1 + pr_below(&L->rs, wx) : (e < 8 || half ? 0 : m->w - 1) + pr_below(&L->rs, m->h / 2) * m->w;
        else q = e < 6 ? 1 + pr_below(&L->rs, wx) + (m->h - 1) * m->w
                       : (e < 8 || half ? 0 : m->w - 1) + (m->h / 2 + pr_below(&L->rs, (m->h + 1) / 2)) * m->w;
        if (half && !mid && 2 * (q % m->w) + 1 >= m->w) continue;
        if (inward(m, q) < 0 || placed[q]) continue;
        int in = trs_neighbour(m, q, inward(m, q)), near = 0;
        for (int o = 0; o < W; o++)
            if (placed[o] && abs(o % m->w - q % m->w) + abs(o / m->w - q / m->w) < 2) near = 1;
        if (half && !mid && abs(mir(m, q) % m->w - q % m->w) < 2) near = 1;   /* its mirror too close */
        if (near || L->reserved[in]) continue;
        return q;
    }
    return -1;
}

/* the network, its portals and stations, the trains' portals; 0: it could not be laid */
static int network(trs_map *m, const trs_spec *sp, lay_ctx *L)
{
    memset(m, 0, sizeof *m);
    m->w = sp->w;
    m->h = sp->h;
    m->together = sp->together;
    L->sym = sp->sym && (m->w & 1) && !sp->tunnels;
    int W = m->w * m->h, port[TRS_MAXE], stat[TRS_COLOURS], np = sp->portals, nst = sp->stations, prim[16];
    for (int c = 0; c < W; c++) L->wt[c] = (float)pr_below(&L->rs, 100) / 60;
    if (L->sym)
        for (int c = 0; c < W; c++)
            if (2 * (c % m->w) + 1 < m->w) L->wt[mir(m, c)] = L->wt[c];
    memset(L->reserved, 0, sizeof L->reserved);
    /* the portals mostly along the top and the upper sides, the stations along the bottom and the lower sides;
     * mirrored: in pairs, an odd one on the middle */
    int placed[TRS_CELLS] = {0};
    for (int pass = 0, k = 0; pass < 2; pass++) {
        int n = pass ? nst : np, *out = pass ? stat : port;
        for (int i = 0; i < n; i++) {
            int mid = L->sym && (n & 1) && i == 0, c = border_place(m, L, !pass, L->sym, mid, placed);
            if (c < 0) return 0;
            int cs[2] = {c, mir(m, c)};
            for (int j = 0; j < (L->sym && !mid ? 2 : 1); j++) {
                if (i + j >= n || placed[cs[j]]) return 0;
                placed[cs[j]] = 1;
                L->reserved[trs_neighbour(m, cs[j], inward(m, cs[j]))] = 1;
                m->kind[cs[j]] = pass ? TRS_STATION : TRS_PORTAL;
                out[i + j] = cs[j];
                prim[k + i + j] = j == 0;
            }
            if (L->sym && !mid) i++;
        }
        k += n;
    }
    int sc[TRS_CELLS * 4], sd[TRS_CELLS * 4];
    float s0[TRS_CELLS * 4] = {0};
    int tunnels = sp->tunnels, pair = 0;
    /* the first line */
    {
        int p = port[0], s = stat[0], in = trs_neighbour(m, s, inward(m, s)), pin;
        sc[0] = p;
        sd[0] = inward(m, p);
        pin = trs_neighbour(m, p, sd[0]);
        L->reserved[pin] = 0;
        if (L->sym) L->reserved[mir(m, pin)] = 0;
        L->reserved[in] = 0;
        if (!lay(m, L, sc, sd, s0, 1, TG_CELL, s, (inward(m, s) + 2) & 3, in)) return 0;
        if (L->sym) L->reserved[mir(m, in)] = 0;
    }
    /* the other stations, the other portals, the loops: in a shuffled order, a tunnel on some (mirrored: the
     * first of each pair; its twin comes with it) */
    int jobs[16], nj = 0;
    for (int i = 1; i < nst; i++)
        if (prim[np + i] && !(L->sym && stat[i] == mir(m, stat[0]))) jobs[nj++] = i;
    for (int i = 1; i < np; i++)
        if (prim[i] && !(L->sym && port[i] == mir(m, port[0]))) jobs[nj++] = 8 + i;
    for (int i = 0; i < sp->loops; i++) jobs[nj++] = 15;
    pr_shuffle(&L->rs, jobs, nj);
    for (int j = 0; j < nj; j++) {
        int job = jobs[j], tun = tunnels > 0 && pr_below(&L->rs, nj - j) < tunnels, ok;
        if (job < 8) {                          /* a branch to a station */
            int s = stat[job], in = trs_neighbour(m, s, inward(m, s));
            int n = branch_starts(m, L, sc, sd, s0);
            L->reserved[in] = 0;
            ok = tun ? lay_tunnel(m, L, sc, sd, s0, n, TG_CELL, s, (inward(m, s) + 2) & 3, in, pair)
                     : lay(m, L, sc, sd, s0, n, TG_CELL, s, (inward(m, s) + 2) & 3, in);
            if (L->sym) L->reserved[mir(m, in)] = 0;
        } else if (job < 15) {                  /* a portal's line joining the track */
            int p = port[job - 8];
            sc[0] = p;
            sd[0] = inward(m, p);
            int in = trs_neighbour(m, p, sd[0]);
            L->reserved[in] = 0;
            if (L->sym) L->reserved[mir(m, in)] = 0;
            ok = tun ? lay_tunnel(m, L, sc, sd, s0, 1, TG_TRACK, -1, -1, -1, pair)
                     : lay(m, L, sc, sd, s0, 1, TG_TRACK, -1, -1, -1);
        } else {                                /* a loop: from a branch back onto the track */
            int n = branch_starts(m, L, sc, sd, s0);
            if (!n) return 0;
            int i = pr_below(&L->rs, n);
            ok = lay(m, L, sc + i, sd + i, s0 + i, 1, TG_TRACK, -1, -1, -1);
            tun = 0;
        }
        if (!ok) return 0;
        if (tun) { tunnels--; pair++; }
    }
    if (pair < sp->tunnels) return 0;
    return finish(m, sp, L);
}

/* every setting that works: bit c of ok[] (2^ns bits); how the others fail (fails[TRS_*]) */
static int settings(const trs_map *m, uint8_t *ok, int *fails)
{
    int n = 0;
    memset(ok, 0, (1u << m->ns) / 8 + 1);
    for (int i = 0; i < 5; i++) fails[i] = 0;
    for (unsigned c = 0; c < 1u << m->ns; c++) {
        int r = trs_simulate(m, (uint16_t)c, NULL);
        fails[r]++;
        if (r == TRS_OK) { ok[c >> 3] |= (uint8_t)(1u << (c & 7)); n++; }
    }
    return n;
}

static int par_from(const trs_map *m, const uint8_t *ok, unsigned set)
{
    int best = -1;
    for (unsigned c = 0; c < 1u << m->ns; c++)
        if (ok[c >> 3] >> (c & 7) & 1) {
            int d = bits(c ^ set);
            if (best < 0 || d < best) best = d;
        }
    return best;
}

/* a quality a candidate network is judged by (the levels' tool reads them too) */
typedef struct { int par, sols, crashes, visited, distinct; } trs_quality;

/* the trains coloured by one setting that works, and the first setting at the par sought: 1 done; q its measures */
static int colour_and_set(trs_map *m, const trs_spec *sp, lay_ctx *L, trs_quality *q)
{
    static trs_run r;
    int col[TRS_COLOURS] = {0, 1, 2, 3, 4};
    pr_shuffle(&L->rs, col, TRS_COLOURS);
    int ns_ = 0;
    for (int c = 0; c < m->w * m->h; c++)
        if (m->kind[c] == TRS_STATION) m->k[c] = (uint8_t)col[ns_++ % TRS_COLOURS];
    uint16_t star = 0;
    int found = 0;
    for (int k = 0; k < 40 && !found; k++) {
        star = (uint16_t)(pr_next(&L->rs) & ((1u << m->ns) - 1));
        if (trs_simulate(m, star, &r) != TRS_OK) continue;
        unsigned seen = 0;                      /* where the trains went: each switch run over, the stations met */
        uint8_t used[TRS_CELLS] = {0};
        int dest[TRS_MAXT];
        for (int i = 0; i < m->nt; i++) {
            for (int j = 0; j < r.len[i]; j++) used[r.cell[i][j]] = 1;
            dest[i] = r.cell[i][r.len[i] - 1];
            seen |= 1u << m->k[dest[i]];
        }
        int visited = 0;
        for (int s = 0; s < m->ns; s++) visited += used[m->sw[s]];
        if (visited < m->ns - m->ns / 3) continue;   /* switches no train goes over: a few at most */
        int want = m->nt < ns_ ? m->nt : ns_;
        if (!sp->toggles && want > m->ne) want = m->ne;
        if (want > 3) want = 3;
        if (bits(seen) < want) continue;
        for (int i = 0; i < m->nt; i++) m->tcol[i] = m->k[dest[i]];
        q->visited = visited;
        q->distinct = bits(seen);
        found = 1;
    }
    if (!found) return 0;
    static uint8_t ok[(1u << TRS_MAXS) / 8 + 1];
    int fails[5];
    q->sols = settings(m, ok, fails);
    q->crashes = fails[TRS_CRASH];
    if (sp->together && fails[TRS_CRASH] * 8 < (1 << m->ns)) return 0;   /* together: crashes must threaten */
    int best = -1, bd = 1 << 20, mid = (sp->lo + sp->hi + 1) / 2;
    for (int k = 0; k < 48; k++) {
        unsigned s = pr_next(&L->rs) & ((1u << m->ns) - 1);
        int p = par_from(m, ok, s), d = abs(p - mid) + (p < sp->lo || p > sp->hi ? 10 : 0);
        if (p > 0 && d < bd) { bd = d; best = (int)s; q->par = p; }
    }
    if (best < 0) return 0;
    m->set0 = (uint16_t)best;
    return 1;
}

static int make_one(trs_map *m, const trs_spec *sp, uint32_t *rs, trs_quality *q)
{
    lay_ctx L;
    memset(&L, 0, sizeof L);
    L.rs = *rs;
    L.crossings = sp->crossings;
    L.cross_cost = sp->crossings ? -1 : 0;
    int ok = network(m, sp, &L) && colour_and_set(m, sp, &L, q);
    *rs = L.rs;
    return ok;
}

int trs_make(trs_map *m, const trs_spec *sp, uint32_t seed)
{
    uint32_t rs = pr_seed(seed);
    trs_map best;
    memset(&best, 0, sizeof best);
    int bd = -1, tries = sp->tries ? sp->tries : 400;
    for (int t = 0; t < tries; t++) {
        trs_quality q;
        if (!make_one(m, sp, &rs, &q)) continue;
        int d = q.par < sp->lo ? sp->lo - q.par : q.par > sp->hi ? q.par - sp->hi : 0;
        if (d == 0) return 1;
        if (bd < 0 || d < bd) { bd = d; best = *m; }
    }
    if (bd >= 0) { *m = best; return 0; }
    /* nothing laid at all: the smallest network there is */
    trs_parse(m, ".0.e4..0./.0.s70tc./.0.p11p10|S|10|0");
    return 0;
}

void trs_variant(int variant, trs_spec *sp)
{
    static const trs_spec V[TRS_VARIANTS] = {
        /* w h portals stations trains switches loops toggles crossings tunnels together lo hi tries sym natural */
        {5, 7, 1, 3, 2, 2, 0, 0, 0, 0, 0, 1, 2, 300, 1, 80},
        {7, 7, 2, 3, 3, 3, 0, 1, 0, 0, 0, 2, 3, 400, 1, 60},
        {6, 8, 2, 4, 4, 4, 1, 1, 1, 0, 0, 3, 6, 800, 0, 50},
    };
    *sp = V[variant < 0 || variant >= TRS_VARIANTS ? 0 : variant];
}

void trs_random(trs_game *g, int variant, uint32_t seed)
{
    trs_spec sp;
    trs_variant(variant, &sp);
    memset(g, 0, sizeof *g);
    trs_make(&g->m, &sp, seed);
    g->par = trs_solve(&g->m, g->m.set0, NULL, NULL);
    trs_begin(g);
}

/* the chapters' pictures: a switch; two portals joining; a toggle; a crossing; two trains together; a tunnel */
const char *const trs_icons[TRS_CHAPTERS] = {
    ".0.e4..0./.0.s70tc./.0.p11p10|S|10|0",
    "e4..0.e4./t3.se2t9./.0.p12.0.|S|20,21|0",
    ".0.e4..0./.0.S70tc./.0.p11p10|S|00,10|0",
    ".0.e4..0./e2.xf.p81/.0.p10.0.|S|00,11|",
    "e4..0.e4./t5..0.t5./p11.0.p10|T|10,01|",
    "e4..0..0./u10.0.u40/.0..0.p13|S|30|",
};

#ifndef TRS_GEN                                 /* not in the levels' tool (tools/trains_gen.c): it makes them */
#include "games/trains_levels.h"

int trs_level(trs_game *g, int level)
{
    if (level < 0 || level >= TRS_LEVELS) return 0;
    memset(g, 0, sizeof *g);
    if (!trs_parse(&g->m, trs_levels[level])) return 0;
    g->par = trs_level_par[level];
    trs_begin(g);
    return 1;
}
#endif
