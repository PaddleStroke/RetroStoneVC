/*
 * Bubble Pop's rules (bubbles.h): the shots in whole numbers, a landing's pops and falls, the moves worth trying, the
 * beam search that solves a box, the obvious player, the maker of symmetric boxes, the levels and the random puzzles.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/bubbles.h"
#include "games/puzrand.h"

static const char COLCH[] = ".rgbypw";

/* ---- the grid --------------------------------------------------------------------------------------------------- */
int bub_row_len(const bub_level *L, int r) { return r & 1 ? L->w - 1 : L->w; }

void bub_cell_xy(const bub_level *L, int drop, int r, int c, int32_t *x, int32_t *y)
{
    (void)L;
    *x = c * BUB_D + (r & 1 ? BUB_D : BUB_R);
    *y = BUB_R + (r + drop) * BUB_ROWH;
}

void bub_launcher(const bub_level *L, int32_t *x, int32_t *y)
{
    *x = L->w * BUB_D / 2;
    *y = BUB_R + (L->rows - 1) * BUB_ROWH + 1900;
}

int32_t bub_field_h(const bub_level *L)
{
    int32_t x, y;
    bub_launcher(L, &x, &y);
    return y + 820;
}

/* the cells beside (r, c): their flat indexes (r * BUB_MAXW + c) */
static int nbrs(const bub_level *L, int r, int c, int *out)
{
    static const int EV[6][2] = {{0, -1}, {0, 1}, {-1, -1}, {-1, 0}, {1, -1}, {1, 0}};
    static const int OD[6][2] = {{0, -1}, {0, 1}, {-1, 0}, {-1, 1}, {1, 0}, {1, 1}};
    const int (*d)[2] = r & 1 ? OD : EV;
    int n = 0;
    for (int k = 0; k < 6; k++) {
        int rr = r + d[k][0], cc = c + d[k][1];
        if (rr < 0 || rr >= BUB_ROWS || cc < 0 || cc >= bub_row_len(L, rr)) continue;
        out[n++] = rr * BUB_MAXW + cc;
    }
    return n;
}

#define CELL(s, i) (((uint8_t *)(s)->c)[(i)])
#define LC(L, i) (((uint8_t *)(L)->c)[(i)])

int bub_count(const bub_state *s, int what)
{
    int n = 0;
    for (int i = 0; i < BUB_CELLS; i++)
        if (what < 0 ? CELL(s, i) != 0 : CELL(s, i) == what) n++;
    return n;
}

/* ---- text ------------------------------------------------------------------------------------------------------- */
static int code_of(char ch)
{
    const char *p;
    if (ch == '.') return 0;
    if (ch == 's') return BUB_STONE;
    if (ch == '*') return BUB_GEM;
    if (ch == '?') return BUB_WILD;
    if (ch == '!') return BUB_BOMB;
    if (ch == '@') return BUB_RAINBOW;
    if ((p = strchr(COLCH + 1, ch))) return (int)(p - COLCH);
    if (ch >= 'A' && ch <= 'Z') {
        int k = code_of((char)(ch - 'A' + 'a'));
        if (k >= 1 && k <= BUB_COLOURS) return k | BUB_LOCK;
    }
    return -1;
}

static char char_of(int code)
{
    if (code == 0) return '.';
    if (code == BUB_STONE) return 's';
    if (code == BUB_GEM) return '*';
    if (code == BUB_WILD) return '?';
    if (code == BUB_BOMB) return '!';
    if (code == BUB_RAINBOW) return '@';
    if (code & BUB_LOCK) return (char)(COLCH[code & 15] - 'a' + 'A');
    return code >= 1 && code <= BUB_COLOURS ? COLCH[code] : '.';
}

static int hexv(char ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1; }

int bub_parse(bub_level *L, const char *t)
{
    memset(L, 0, sizeof *L);
    if (!t || strlen(t) < 5) return 0;
    L->w = t[0] - '0';
    L->rows = hexv(t[1]);
    L->every = hexv(t[2]);
    if (L->w < 5 || L->w > BUB_MAXW || L->rows < 6 || L->rows > BUB_ROWS - 3 || L->every < 0 || t[3] != ':') return 0;
    const char *p = t + 4;
    int r = 0, c = 0;
    for (; *p && *p != ':'; p++) {
        if (*p == '|') { r++; c = 0; continue; }
        int k = code_of(*p);
        if (k < 0 || k == BUB_BOMB || k == BUB_RAINBOW || r >= L->rows || c >= bub_row_len(L, r)) return 0;
        L->c[r][c++] = (uint8_t)k;
        if (k == BUB_GEM) L->gems++;
    }
    if (*p++ != ':') return 0;
    for (; *p && *p != ':'; p++) {
        int k = code_of(*p);
        if (k <= 0 || k == BUB_STONE || k == BUB_GEM || k == BUB_WILD || (k & BUB_LOCK) || L->nseq >= BUB_SEQ) return 0;
        L->seq[L->nseq++] = (uint8_t)k;
    }
    if (!L->nseq) return 0;
    if (*p == ':') {
        p++;
        while (*p && L->nsol < BUB_SEQ) {
            int sw = 0;
            if (*p == '~') { sw = 1; p++; }
            char *e;
            long a = strtol(p, &e, 10);
            if (e == p || a < -BUB_AMAX || a > BUB_AMAX) return 0;
            L->sol[L->nsol] = (int16_t)a;
            L->solswap[L->nsol++] = (uint8_t)sw;
            p = e;
            if (*p == ',') p++;
            else if (*p) return 0;
        }
    }
    return 1;
}

void bub_text(const bub_level *L, char *o)
{
    static const char HX[] = "0123456789abcdef";
    int n = 0, last = 0;
    for (int r = 0; r < L->rows; r++)
        for (int c = 0; c < bub_row_len(L, r); c++)
            if (L->c[r][c]) last = r;
    o[n++] = (char)('0' + L->w);
    o[n++] = HX[L->rows & 15];
    o[n++] = HX[L->every & 15];
    o[n++] = ':';
    for (int r = 0; r <= last; r++) {
        if (r) o[n++] = '|';
        for (int c = 0; c < bub_row_len(L, r); c++) o[n++] = char_of(L->c[r][c]);
    }
    o[n++] = ':';
    for (int i = 0; i < L->nseq; i++) o[n++] = char_of(L->seq[i]);
    o[n++] = ':';
    for (int i = 0; i < L->nsol && n < BUB_TEXT - 12; i++)
        n += sprintf(o + n, "%s%s%d", i ? "," : "", L->solswap[i] ? "~" : "", L->sol[i]);
    o[n] = 0;
}

/* ---- a state ---------------------------------------------------------------------------------------------------- */
void bub_begin(const bub_level *L, bub_state *s)
{
    memset(s, 0, sizeof *s);
    memcpy(s->c, L->c, sizeof s->c);
    s->cur = L->nseq > 0 ? L->seq[0] : 0;
    s->nxt = L->nseq > 1 ? L->seq[1] : 0;
    s->qi = 2;
    s->end = BUB_PLAYING;
}

int bub_swap(bub_state *s)
{
    if (s->end != BUB_PLAYING || !s->nxt || !s->cur) return 0;
    uint8_t t = s->cur;
    s->cur = s->nxt;
    s->nxt = t;
    return 1;
}

/* ---- a shot's path ---------------------------------------------------------------------------------------------- */
static int64_t isqrt64(int64_t v)
{
    if (v <= 0) return 0;
    int64_t s = (int64_t)sqrt((double)v);
    while (s > 0 && s * s > v) s--;
    while ((s + 1) * (s + 1) <= v) s++;
    return s;
}

/* the marbles a shot can touch: those with an empty place beside them (or the box's open bottom) */
static int exposed(const bub_level *L, const bub_state *s, int *ex)
{
    int n = 0, nb[6];
    for (int r = 0; r < BUB_ROWS; r++)
        for (int c = 0; c < bub_row_len(L, r); c++) {
            if (!s->c[r][c]) continue;
            int k = nbrs(L, r, c, nb), open = r == BUB_ROWS - 1;
            for (int j = 0; j < k && !open; j++)
                if (!CELL(s, nb[j])) open = 1;
            if (open) ex[n++] = r * BUB_MAXW + c;
        }
    return n;
}

/* the path at aim: its points in out (may be NULL); the cell it lands in (-1 none: it went past BUB_PATH points) and
 * how many times it bounced; *hit the marble it touched (-1 the ceiling) */
static int trace(const bub_level *L, const bub_state *s, int aim, const int *ex, int nex, bub_shot *out, int *bounces,
                 int *hit, int64_t *hx, int64_t *hy)
{
    int32_t lx, ly;
    bub_launcher(L, &lx, &ly);
    int64_t px = lx, py = ly, vx = aim, vy = -BUB_AY;
    const int64_t xmin = BUB_R, xmax = (int64_t)L->w * BUB_D - BUB_R, ymin = BUB_R + (int64_t)s->drop * BUB_ROWH;
    const int64_t H2 = (int64_t)BUB_HIT * BUB_HIT;
    int n = 0;
    if (out) { out->x[0] = lx; out->y[0] = ly; }
    n = 1;
    *bounces = 0;
    for (int seg = 0; seg < BUB_PATH - 2; seg++) {
        int64_t a = vx * vx + vy * vy, bn = 0, bd = 1;
        int best = -1;
        for (int i = 0; i < nex; i++) {
            int32_t cx, cy;
            bub_cell_xy(L, s->drop, ex[i] / BUB_MAXW, ex[i] % BUB_MAXW, &cx, &cy);
            int64_t dx = cx - px, dy = cy - py, b = vx * dx + vy * dy;
            if (b <= 0) continue;
            int64_t c = dx * dx + dy * dy - H2, num;
            if (c <= 0) num = 0;
            else {
                int64_t disc = b * b - a * c;
                if (disc < 0) continue;
                num = b - isqrt64(disc);
                if (num < 0) num = 0;
            }
            if (best < 0 || num * bd < bn * a) { best = ex[i]; bn = num; bd = a; }
        }
        /* the ceiling and the walls */
        int64_t cn = py - ymin, cd = -vy, wn = 0, wd = 0;
        if (vx > 0) { wn = xmax - px; wd = vx; }
        else if (vx < 0) { wn = px - xmin; wd = -vx; }
        if (cn < 0) cn = 0;
        if (wn < 0) wn = 0;
        int event;                              /* 0 a marble, 1 the ceiling, 2 a wall */
        if (best >= 0 && bn * cd <= cn * bd && (!wd || bn * wd <= wn * bd)) event = 0;
        else if (!wd || cn * wd <= wn * cd) event = 1;
        else event = 2;
        if (event == 0) { px += vx * bn / bd; py += vy * bn / bd; }
        else if (event == 1) { px += vx * cn / cd; py = ymin; }
        else { py += vy * wn / wd; px = vx > 0 ? xmax : xmin; }
        if (out) { out->x[n] = (int32_t)px; out->y[n] = (int32_t)py; }
        n++;
        if (event == 2) {
            vx = -vx;
            (*bounces)++;
            continue;
        }
        if (out) out->n = n;
        *hit = event == 0 ? best : -1;
        *hx = px;
        *hy = py;
        /* the place it snaps into: the empty one nearest the point it stopped at, beside what it touched */
        int cand[BUB_CELLS], nc = 0;
        if (event == 0) nc = nbrs(L, best / BUB_MAXW, best % BUB_MAXW, cand);
        else
            for (int c = 0; c < bub_row_len(L, 0); c++) cand[nc++] = c;
        int pick = -1;
        int64_t pd = 0;
        for (int pass = 0; pass < 2 && pick < 0; pass++) {
            if (pass) {                         /* nothing free beside it: the nearest free place anywhere */
                nc = 0;
                for (int r = 0; r < BUB_ROWS; r++)
                    for (int c = 0; c < bub_row_len(L, r); c++) cand[nc++] = r * BUB_MAXW + c;
            }
            for (int i = 0; i < nc; i++) {
                if (CELL(s, cand[i])) continue;
                int32_t cx, cy;
                bub_cell_xy(L, s->drop, cand[i] / BUB_MAXW, cand[i] % BUB_MAXW, &cx, &cy);
                int64_t d = (cx - px) * (cx - px) + (cy - py) * (cy - py);
                if (pick < 0 || d < pd) { pick = cand[i]; pd = d; }
            }
        }
        return pick;
    }
    if (out) out->n = n;
    *hit = -1;
    return -1;
}

int bub_trace(const bub_level *L, const bub_state *s, int aim, bub_shot *out)
{
    int ex[BUB_CELLS], b, h;
    int64_t hx, hy;
    int n = exposed(L, s, ex);
    if (aim < -BUB_AMAX) aim = -BUB_AMAX;
    if (aim > BUB_AMAX) aim = BUB_AMAX;
    int cell = trace(L, s, aim, ex, n, out, &b, &h, &hx, &hy);
    if (out) {
        out->cell = cell;
        out->bounces = b;
        if (cell >= 0) {                        /* the last point: the place it snaps into */
            int32_t x, y;
            bub_cell_xy(L, s->drop, cell / BUB_MAXW, cell % BUB_MAXW, &x, &y);
            out->x[out->n] = x;
            out->y[out->n++] = y;
        }
    }
    return cell;
}

/* ---- a landing ---------------------------------------------------------------------------------------------------- */
static int matches(int code, int colour)
{
    if (code == BUB_WILD) return 1;
    if (!code || code == BUB_STONE || code == BUB_GEM) return 0;
    return (code & 15) == colour;
}

/* the group of `colour` through the cell start (it counts whatever is there): its cells marked, how many */
static int flood(const bub_level *L, const bub_state *s, int start, int colour, uint8_t *mark)
{
    int q[BUB_CELLS], h = 0, t = 0, nb[6];
    uint8_t seen[BUB_CELLS] = {0};
    q[t++] = start;
    seen[start] = 1;
    while (h < t) {
        int i = q[h++], k = nbrs(L, i / BUB_MAXW, i % BUB_MAXW, nb);
        for (int j = 0; j < k; j++) {
            int n = nb[j];
            if (seen[n] || !matches(CELL(s, n), colour)) continue;
            seen[n] = 1;
            q[t++] = n;
        }
    }
    if (mark)
        for (int i = 0; i < t; i++) mark[q[i]] = 1;
    return t;
}

static void record_fall(bub_shot *o, int i, int code)
{
    if (!o) return;
    o->fall[o->nfall] = (uint8_t)i;
    o->fallc[o->nfall++] = (uint8_t)code;
}

/* the marble `m` lands in cell L0: pops, cages, falls, the ceiling, the end */
static void land(const bub_level *L, bub_state *s, int at, int m, bub_shot *o)
{
    uint8_t mark[BUB_CELLS] = {0};
    int nb[6], k = nbrs(L, at / BUB_MAXW, at % BUB_MAXW, nb), any = 0, freed = 0;
    if (m == BUB_BOMB) {
        mark[at] = 1;
        for (int j = 0; j < k; j++)
            if (CELL(s, nb[j])) mark[nb[j]] = 1;
        CELL(s, at) = BUB_STONE;                /* blown up with the rest (locks and stones too) */
        any = 2;
    } else if (m == BUB_RAINBOW) {
        CELL(s, at) = BUB_WILD;
        for (int col = 1; col <= BUB_COLOURS; col++) {
            int near = 0;
            for (int j = 0; j < k; j++)
                if ((CELL(s, nb[j]) & 15) == col) near = 1;
            if (!near) continue;
            uint8_t mk[BUB_CELLS] = {0};
            if (flood(L, s, at, col, mk) >= 3) {
                any = 1;
                for (int i = 0; i < BUB_CELLS; i++) mark[i] |= mk[i];
            }
        }
    } else {
        CELL(s, at) = (uint8_t)m;
        uint8_t mk[BUB_CELLS] = {0};
        if (flood(L, s, at, m, mk) >= 3) {
            any = 1;
            memcpy(mark, mk, sizeof mark);
        }
    }
    if (any) {
        /* the rings: how far each one lies from the landing, through the cells popped */
        uint8_t ring[BUB_CELLS];
        memset(ring, 255, sizeof ring);
        int q[BUB_CELLS], h = 0, t = 0;
        q[t++] = at;
        ring[at] = 0;
        while (h < t) {
            int i = q[h++], kk = nbrs(L, i / BUB_MAXW, i % BUB_MAXW, nb);
            for (int j = 0; j < kk; j++)
                if (mark[nb[j]] && ring[nb[j]] == 255) {
                    ring[nb[j]] = (uint8_t)(ring[i] + 1);
                    q[t++] = nb[j];
                }
        }
        for (int i = 0; i < BUB_CELLS; i++) {
            if (!mark[i]) continue;
            int code = CELL(s, i);
            if ((code & BUB_LOCK) && any == 1) { /* a cage broken: the marble stays */
                CELL(s, i) = (uint8_t)(code & 15);
                if (o) o->unlock[o->nunlock++] = (uint8_t)i;
                continue;
            }
            if (code == BUB_GEM) freed++;
            if (o) {
                o->pop[o->npop] = (uint8_t)i;
                o->popc[o->npop] = (uint8_t)(i == at && any == 2 ? BUB_BOMB : code);
                o->popring[o->npop++] = ring[i] == 255 ? 1 : ring[i];
            }
            CELL(s, i) = 0;
        }
    }
    /* what no longer hangs from the ceiling falls */
    uint8_t held[BUB_CELLS] = {0};
    int q[BUB_CELLS], h = 0, t = 0;
    for (int c = 0; c < bub_row_len(L, 0); c++)
        if (s->c[0][c]) { held[c] = 1; q[t++] = c; }
    while (h < t) {
        int i = q[h++], kk = nbrs(L, i / BUB_MAXW, i % BUB_MAXW, nb);
        for (int j = 0; j < kk; j++)
            if (CELL(s, nb[j]) && !held[nb[j]]) { held[nb[j]] = 1; q[t++] = nb[j]; }
    }
    for (int i = 0; i < BUB_CELLS; i++)
        if (CELL(s, i) && !held[i]) {
            if (CELL(s, i) == BUB_GEM) freed++;
            record_fall(o, i, CELL(s, i));
            CELL(s, i) = 0;
        }
    if (o) o->gems_freed = freed;
    s->shots++;
    int won = L->gems ? bub_count(s, BUB_GEM) == 0 : bub_count(s, -1) == 0;
    if (won) { s->end = BUB_WON; return; }
    if (L->every && s->shots % L->every == 0) {
        s->drop++;
        if (o) o->dropped = 1;
    }
    for (int r = 0; r < BUB_ROWS; r++)
        for (int c = 0; c < bub_row_len(L, r); c++)
            if (s->c[r][c] && r + s->drop >= L->rows) { s->end = BUB_LOW; return; }
    if (!s->cur) s->end = BUB_OVER;
}

int bub_shoot(const bub_level *L, bub_state *s, int aim, int swap, bub_shot *o)
{
    if (s->end != BUB_PLAYING || !s->cur || (swap && !s->nxt)) return 0;
    if (o) memset(o, 0, sizeof *o);
    int cell = bub_trace(L, s, aim, o);
    if (cell < 0) return 0;
    int m;
    if (swap) { m = s->nxt; }
    else { m = s->cur; s->cur = s->nxt; }
    s->nxt = s->qi < L->nseq ? L->seq[s->qi] : 0;
    if (s->qi < L->nseq) s->qi++;
    if (o) { o->marble = (uint8_t)m; o->cell = cell; }
    land(L, s, cell, m, o);
    return 1;
}

/* ---- the moves worth trying ------------------------------------------------------------------------------------- */
#define SAMPLES 321                             /* every half degree from 170 down to 10 */

static int sample_aim(int i)
{
    double th = (170.0 - 0.5 * i) * 3.14159265358979 / 180;
    double a = BUB_AY * cos(th) / sin(th);
    long v = lround(a);
    return (int)(v < -BUB_AMAX ? -BUB_AMAX : v > BUB_AMAX ? BUB_AMAX : v);
}

int bub_moves(const bub_level *L, const bub_state *s, bub_move *out, int max)
{
    if (s->end != BUB_PLAYING || !s->cur) return 0;
    int ex[BUB_CELLS], nex = exposed(L, s, ex), cell[SAMPLES], aim[SAMPLES];
    for (int i = 0; i < SAMPLES; i++) {
        int b, h;
        int64_t hx, hy;
        aim[i] = sample_aim(i);
        cell[i] = trace(L, s, aim[i], ex, nex, NULL, &b, &h, &hx, &hy);
        if (b > 2) cell[i] = -1;
    }
    /* runs of samples landing in one place: the longest run of each place, at its middle */
    int best_len[BUB_CELLS] = {0}, best_mid[BUB_CELLS];
    for (int i = 0; i < SAMPLES;) {
        int j = i;
        while (j < SAMPLES && cell[j] == cell[i]) j++;
        int len = j - i;
        if (cell[i] >= 0 && len >= 2 && len > best_len[cell[i]]) {
            best_len[cell[i]] = len;
            best_mid[cell[i]] = aim[(i + j - 1) / 2];
        }
        i = j;
    }
    bub_move m[BUB_CELLS];
    int n = 0;
    for (int c = 0; c < BUB_CELLS; c++)
        if (best_len[c]) { m[n].aim = (int16_t)best_mid[c]; m[n].cell = (uint8_t)c; m[n++].swap = 0; }
    for (int i = 1; i < n; i++)                 /* by aim, from the left */
        for (int j = i; j > 0 && m[j].aim < m[j - 1].aim; j--) {
            bub_move t = m[j];
            m[j] = m[j - 1];
            m[j - 1] = t;
        }
    int k = 0, two = s->nxt && s->nxt != s->cur;
    for (int i = 0; i < n && k < max; i++) {
        out[k++] = m[i];
        if (two && k < max) { out[k] = m[i]; out[k++].swap = 1; }
    }
    return k;
}

/* ---- the search ------------------------------------------------------------------------------------------------- */
/* how far a state seems from the end (lower: nearer): what is left, gems weighing most, groups of a colour helping,
 * marbles whose colour will not come again (they can only fall), a box near the line */
static int score(const bub_level *L, const bub_state *s)
{
    int n = 0, gems = 0, stones = 0, pairs = 0, dead = 0, low = 0, nb[6];
    unsigned left = 0;
    for (int i = s->qi; i < L->nseq; i++) left |= 1u << (L->seq[i] & 15);
    left |= 1u << (s->cur & 15) | 1u << (s->nxt & 15);
    int rainbow = (left >> BUB_RAINBOW & 1) || (left >> BUB_BOMB & 1);
    for (int r = 0; r < BUB_ROWS; r++)
        for (int c = 0; c < bub_row_len(L, r); c++) {
            int v = s->c[r][c];
            if (!v) continue;
            n++;
            if (r + s->drop > low) low = r + s->drop;
            if (v == BUB_GEM) { gems++; continue; }
            if (v == BUB_STONE) { stones++; continue; }
            int col = v & 15;
            if (v != BUB_WILD && !(left >> col & 1) && !rainbow) dead++;
            int k = nbrs(L, r, c, nb);
            for (int j = 0; j < k; j++)
                if (nb[j] > r * BUB_MAXW + c && (CELL(s, nb[j]) & 15) == col && CELL(s, nb[j]) != BUB_STONE &&
                    CELL(s, nb[j]) != BUB_GEM)
                    pairs++;
        }
    int v = L->gems ? 400 * gems + 3 * n + 3 * stones - 2 * pairs + 2 * dead
                    : 10 * n + 6 * stones - 3 * pairs + 6 * dead;
    if (low >= L->rows - 2) v += 30 * (low - L->rows + 3);
    return v;
}

static uint64_t hash_state(const bub_state *s)
{
    uint64_t h = 0xcbf29ce484222325ull;
    const uint8_t *p = (const uint8_t *)s;
    for (size_t i = 0; i < sizeof *s; i++) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

typedef struct { bub_state s; int parent, score; bub_move m; uint64_t h; } node;

static int by_hash(const void *a, const void *b)
{
    const node *x = a, *y = b;
    if (x->h != y->h) return x->h < y->h ? -1 : 1;
    return x->score - y->score;
}

static int by_score(const void *a, const void *b)
{
    const node *x = a, *y = b;
    if (x->score != y->score) return x->score - y->score;
    return x->h < y->h ? -1 : x->h > y->h;
}

int bub_solve(const bub_level *L, const bub_state *s0, int beam, bub_move *path)
{
    if (s0->end == BUB_WON) return 0;
    if (s0->end != BUB_PLAYING) return -1;
    if (beam < 1) beam = 1;
    int depth_max = L->nseq - s0->shots;
    node *layer = malloc(sizeof(node) * (size_t)beam), *cand = malloc(sizeof(node) * (size_t)beam * BUB_MOVES);
    int16_t *par = malloc(sizeof(int16_t) * (size_t)beam * (BUB_SEQ + 1));
    bub_move *mv = malloc(sizeof(bub_move) * (size_t)beam * (BUB_SEQ + 1));
    int found = -1;
    if (!layer || !cand || !par || !mv) goto done;
    layer[0].s = *s0;
    int nl = 1;
    for (int d = 0; d < depth_max && d < BUB_SEQ; d++) {
        int nc = 0;
        for (int i = 0; i < nl; i++) {
            bub_move m[BUB_MOVES];
            int n = bub_moves(L, &layer[i].s, m, BUB_MOVES);
            for (int j = 0; j < n; j++) {
                node *c = &cand[nc];
                c->s = layer[i].s;
                if (!bub_shoot(L, &c->s, m[j].aim, m[j].swap, NULL)) continue;
                c->parent = i;
                c->m = m[j];
                if (c->s.end == BUB_WON) {
                    found = d + 1;
                    if (path) {
                        path[d] = m[j];
                        int p = i;
                        for (int e = d - 1; e >= 0; e--) {
                            path[e] = mv[e * beam + p];
                            p = par[e * beam + p];
                        }
                    }
                    goto done;
                }
                if (c->s.end != BUB_PLAYING) continue;
                c->score = score(L, &c->s);
                c->h = hash_state(&c->s);
                nc++;
            }
        }
        if (!nc) break;
        qsort(cand, (size_t)nc, sizeof *cand, by_hash);   /* one of each state, the best way to it */
        int u = 0;
        for (int i = 0; i < nc; i++)
            if (!u || cand[i].h != cand[u - 1].h) cand[u++] = cand[i];
        qsort(cand, (size_t)u, sizeof *cand, by_score);
        nl = u < beam ? u : beam;
        for (int i = 0; i < nl; i++) {
            par[d * beam + i] = (int16_t)cand[i].parent;
            mv[d * beam + i] = cand[i].m;
            layer[i] = cand[i];
        }
    }
done:
    free(layer);
    free(cand);
    free(par);
    free(mv);
    return found;
}

int bub_greedy(const bub_level *L, const bub_state *s0)
{
    bub_state s = *s0;
    while (s.end == BUB_PLAYING) {
        bub_move m[BUB_MOVES];
        int n = bub_moves(L, &s, m, BUB_MOVES), bi = -1, bv = 0, bs = 0;
        for (int j = 0; j < n; j++) {
            bub_state t = s;
            bub_shot o;
            if (!bub_shoot(L, &t, m[j].aim, m[j].swap, &o)) continue;
            int v = t.end == BUB_WON ? 1000000 : t.end != BUB_PLAYING ? -1000000
                                                : (o.npop + o.nfall) * 10 + o.gems_freed * 1000;
            int sc = score(L, &t);
            if (bi < 0 || v > bv || (v == bv && sc < bs)) { bi = j; bv = v; bs = sc; }
        }
        if (bi < 0) return -1;
        bub_shoot(L, &s, m[bi].aim, m[bi].swap, NULL);
    }
    return s.end == BUB_WON ? s.shots : -1;
}

int bub_check_solution(const bub_level *L)
{
    bub_state s;
    bub_begin(L, &s);
    for (int i = 0; i < L->nsol; i++)
        if (!bub_shoot(L, &s, L->sol[i], L->solswap[i], NULL)) return -1;
    return s.end == BUB_WON ? s.shots : -1;
}

/* ---- a game ----------------------------------------------------------------------------------------------------- */
void bub_new(bub_game *g, const bub_level *L)
{
    memset(g, 0, sizeof *g);
    g->L = *L;
    bub_begin(&g->L, &g->s);
}

int bub_play(bub_game *g, int aim, int swap, bub_shot *out)
{
    bub_state before = g->s;
    if (!bub_shoot(&g->L, &g->s, aim, swap, out)) return 0;
    if (g->nh < BUB_SEQ + 1) g->hist[g->nh++] = before;
    return 1;
}

int bub_undo(bub_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    return 1;
}

static int same_place(const bub_state *a, const bub_state *b)
{
    return !memcmp(a->c, b->c, sizeof a->c) && a->qi == b->qi && a->shots == b->shots && a->drop == b->drop &&
           ((a->cur == b->cur && a->nxt == b->nxt) || (a->cur == b->nxt && a->nxt == b->cur));
}

int bub_hint(bub_game *g, bub_move *m)
{
    if (g->s.end == BUB_WON) return 0;
    for (;;) {
        if (g->s.end == BUB_PLAYING) {
            /* the kept solution, while the shots so far follow it */
            bub_state t;
            bub_begin(&g->L, &t);
            int k = 0;
            while (k < g->L.nsol && t.shots < g->s.shots) {
                if (!bub_shoot(&g->L, &t, g->L.sol[k], g->L.solswap[k], NULL)) break;
                k++;
            }
            if (k < g->L.nsol && t.shots == g->s.shots && same_place(&t, &g->s)) {
                m->aim = g->L.sol[k];
                m->swap = g->L.solswap[k];
                if (t.cur != g->s.cur) m->swap = !m->swap;   /* swapped already */
                m->cell = (uint8_t)bub_trace(&g->L, &g->s, m->aim, NULL);
                return 1;
            }
            bub_move path[BUB_SEQ];
            if (bub_solve(&g->L, &g->s, 12, path) > 0) {
                *m = path[0];
                return 1;
            }
        }
        if (!bub_undo(g)) return 0;
    }
}

int bub_script(bub_game *g, const char *p)
{
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == 'w') { if (!bub_swap(&g->s)) return 0; p++; continue; }
        if (*p == 'h') {
            bub_move m;
            if (!bub_hint(g, &m) || !bub_play(g, m.aim, m.swap, NULL)) return 0;
            g->hints++;
            p++;
            continue;
        }
        if (*p == '=') {
            for (int guard = 0; g->s.end == BUB_PLAYING && guard < BUB_SEQ; guard++) {
                bub_move m;
                if (!bub_hint(g, &m) || !bub_play(g, m.aim, m.swap, NULL)) return 0;
            }
            p++;
            continue;
        }
        int sw = 0;
        if (*p == '~') { sw = 1; p++; }
        char *e;
        long a = strtol(p, &e, 10);
        if (e == p || !bub_play(g, (int)a, sw, NULL)) return 0;
        p = e;
    }
    return 1;
}

/* ---- the maker -------------------------------------------------------------------------------------------------- */
static int mirror_c(const bub_level *L, int r, int c) { return bub_row_len(L, r) - 1 - c; }

/* is cell (r, c) in shape `sh` of h rows (u: its centre across, in marbles from the box's middle, 0..w/2; v: down) */
static int in_shape(int sh, int r, int h, float u, float w2, float k)
{
    float dx = u / w2, v = h > 1 ? (float)r / (h - 1) : 0, vm = (r + 0.5f) / h;
    switch (sh) {
    case 0: return 1;                                                         /* a block */
    case 1: return dx <= 1.05f - 0.8f * v;                                    /* a wedge */
    case 2: return dx <= 0.3f + 0.75f * sinf(3.14159f * vm);                  /* a lens */
    case 3: return dx <= 0.22f + 1.25f * fabsf(vm - 0.5f) || r == 0;         /* an hourglass */
    case 4: return v < 0.4f || dx > 0.5f;                                     /* an arch on two legs */
    case 5: {                                                                 /* two chandeliers */
        float d = fabsf(dx - k), wd = 0.1f + 0.36f * v * v;
        return d <= wd || r == 0;
    }
    case 6: {                                                                 /* a ring */
        float e = sqrtf(dx * dx + (vm - 0.5f) * (vm - 0.5f) * 4);
        return (e <= 1.08f && e >= 0.5f) || r == 0;
    }
    case 7: return v <= 0.45f + 0.55f * fabsf(cosf(dx * 3.14159f * k));       /* teeth */
    case 8: return !(r % 3 == 2 && ((int)(u + 0.5f)) % 2 == 1);               /* a block with windows */
    case 9: return v <= 1.0f - 0.75f * fabsf(sinf(3.14159f * dx));            /* a W */
    case 10: return dx >= 0.4f * v || r < 2;                                  /* a V cut up the middle */
    default: {                                                                /* three drops */
        float d = fminf(dx, fabsf(dx - 0.75f)), wd = 0.08f + 0.3f * v;
        return d <= wd || r == 0;
    }
    }
}

#define SHAPES 12

/* the class (0..k-1) of a cell by pattern `pt` */
static int pattern(int pt, int r, int h, float u, int k, int band, uint32_t salt)
{
    int q = (int)(u + 0.5f);
    switch (pt) {
    case 0: return (r / band) % k;                                            /* rows */
    case 1: return (q / band) % k;                                            /* columns from the middle */
    case 2: {                                                                 /* rings */
        float d = sqrtf(u * u + (r - h * 0.5f) * (r - h * 0.5f) * 0.75f);
        return (int)(d / band) % k;
    }
    case 3: return ((r + q) / band) % k;                                      /* chevrons */
    case 4: return ((r / 2) + (q / 2)) % k;                                   /* blocks */
    default: return (int)((salt >> ((r * 3 + q) % 29)) % (uint32_t)k);       /* scattered */
    }
}

#define PATTERNS 6

int bub_make(bub_level *L, const bub_spec *sp, uint32_t seed)
{
    uint32_t rs = pr_seed(seed);
    memset(L, 0, sizeof *L);
    L->w = sp->w;
    L->rows = sp->rows;
    L->every = sp->every;
    int h = sp->hmin + pr_below(&rs, sp->hmax - sp->hmin + 1);
    if (h > L->rows - 2) h = L->rows - 2;
    int sh = pr_below(&rs, SHAPES), pt = pr_below(&rs, PATTERNS), k = sp->colours;
    L->shape = sh;
    L->pattern = pt;
    int band = 1 + pr_below(&rs, 2);
    float kk = 0.45f + 0.25f * (pr_below(&rs, 100) / 100.0f);
    uint32_t salt = pr_next(&rs);
    float w2 = L->w / 2.0f;
    int pal[BUB_COLOURS] = {1, 2, 3, 4, 5, 6};
    pr_shuffle(&rs, pal, BUB_COLOURS);
    /* the shape and its colours, mirrored */
    for (int r = 0; r < h; r++)
        for (int c = 0; c < bub_row_len(L, r); c++) {
            if (c > mirror_c(L, r, c)) continue;
            int32_t x, y;
            bub_cell_xy(L, 0, r, c, &x, &y);
            float u = fabsf(x / (float)BUB_D - w2);
            if (!in_shape(sh, r, h, u, w2, kk)) continue;
            int cl = pattern(pt, r, h, u, k, band, salt);
            if (pt == 5) cl = pr_below(&rs, k);
            L->c[r][c] = L->c[r][mirror_c(L, r, c)] = (uint8_t)pal[cl];
        }
    if (pt == 5)                                /* scattered: grown into small groups */
        for (int pass = 0; pass < 2; pass++)
            for (int r = 0; r < h; r++)
                for (int c = 0; c < bub_row_len(L, r); c++) {
                    if (!L->c[r][c] || c > mirror_c(L, r, c) || pr_below(&rs, 100) < 45) continue;
                    int nb[6], n = nbrs(L, r, c, nb), j = nb[pr_below(&rs, n)];
                    int v = LC(L, j);
                    if (v >= 1 && v <= BUB_COLOURS) L->c[r][c] = L->c[r][mirror_c(L, r, c)] = (uint8_t)v;
                }
    /* only what hangs from the ceiling */
    bub_state t;
    memcpy(t.c, L->c, sizeof t.c);
    uint8_t held[BUB_CELLS] = {0};
    int q[BUB_CELLS], qh = 0, qt = 0, nb[6];
    for (int c = 0; c < bub_row_len(L, 0); c++)
        if (LC(L, c)) { held[c] = 1; q[qt++] = c; }
    while (qh < qt) {
        int i = q[qh++], n = nbrs(L, i / BUB_MAXW, i % BUB_MAXW, nb);
        for (int j = 0; j < n; j++)
            if (CELL(&t, nb[j]) && !held[nb[j]]) { held[nb[j]] = 1; q[qt++] = nb[j]; }
    }
    int cells = 0;
    for (int i = 0; i < BUB_CELLS; i++) {
        if (!held[i]) LC(L, i) = 0;
        if (LC(L, i)) cells++;
    }
    if (cells < 8) return 0;
    /* the specials, in mirrored pairs (or on the middle line) */
    int want[3] = {sp->gems, sp->stones, sp->locks};
    for (int kind = 0; kind < 3; kind++)
        for (int n = 0, tries = 0; n < want[kind] && tries < 200; tries++) {
            int r = 1 + pr_below(&rs, h - 1 > 0 ? h - 1 : 1), c = pr_below(&rs, bub_row_len(L, r));
            if (kind == 0 && r < h / 2 && tries < 120) continue;       /* gems deep in the box */
            int v = L->c[r][c];
            if (v < 1 || v > BUB_COLOURS) continue;
            int mc = mirror_c(L, r, c);
            int nv = kind == 0 ? BUB_GEM : kind == 1 ? BUB_STONE : v | BUB_LOCK;
            L->c[r][c] = L->c[r][mc] = (uint8_t)nv;
            n += mc == c ? 1 : 2;
        }
    for (int i = 0; i < BUB_CELLS; i++)
        if (LC(L, i) == BUB_GEM) L->gems++;
    if (sp->gems && !L->gems) return 0;
    /* the sequence: the box's colours, each as often as it is there or so, never four alike in a row */
    int count[BUB_COLOURS + 1] = {0}, total = 0;
    for (int i = 0; i < BUB_CELLS; i++) {
        int v = LC(L, i) & 15;
        if (LC(L, i) != BUB_STONE && LC(L, i) != BUB_GEM && v >= 1 && v <= BUB_COLOURS) { count[v]++; total++; }
    }
    if (!total) return 0;
    for (int i = 0; i < BUB_SEQ; i++) {
        int pick = 0;
        for (int tries = 0; tries < 8; tries++) {
            int x = pr_below(&rs, total + 2 * BUB_COLOURS), acc = 0;
            pick = 0;
            for (int c = 1; c <= BUB_COLOURS && !pick; c++) {
                if (!count[c]) continue;
                acc += count[c] + 2;
                if (x < acc) pick = c;
            }
            if (!pick) for (int c = 1; c <= BUB_COLOURS; c++) if (count[c]) { pick = c; break; }
            if (i < 3 || L->seq[i - 1] != pick || L->seq[i - 2] != pick || L->seq[i - 3] != pick) break;
        }
        L->seq[i] = (uint8_t)pick;
    }
    for (int b = 0; b < sp->bombs; b++) L->seq[1 + pr_below(&rs, 7) + 6 * b] = BUB_BOMB;
    for (int b = 0; b < sp->rainbows; b++) L->seq[2 + pr_below(&rs, 7) + 6 * b] = BUB_RAINBOW;
    L->nseq = BUB_SEQ;
    return 1;
}

int bub_settle(bub_level *L, const bub_spec *sp)
{
    bub_state s;
    bub_move path[BUB_SEQ];
    L->nseq = BUB_SEQ;
    bub_begin(L, &s);
    int n = bub_solve(L, &s, sp->beam, path);
    if (n <= 0) return 0;
    int budget = n + (sp->slack > 0 ? sp->slack : 1);
    if (budget > BUB_SEQ) return 0;
    L->nseq = budget;
    L->nsol = n;
    for (int i = 0; i < n; i++) {
        L->sol[i] = path[i].aim;
        L->solswap[i] = path[i].swap;
    }
    return bub_check_solution(L) == n;
}

/* ---- the levels and the random puzzles ---------------------------------------------------------------------------- */
void bub_variant(int v, bub_spec *sp)
{
    static const bub_spec V[BUB_VARIANTS] = {
        /* w rows every colours hmin hmax gems stones locks bombs rainbows slack beam */
        {7, 11, 0, 3, 3, 5, 0, 0, 0, 0, 0, 2, 16},
        {8, 12, 0, 4, 4, 6, 2, 2, 0, 1, 0, 2, 16},
        {8, 12, 7, 5, 4, 7, 0, 2, 2, 1, 1, 1, 16},
    };
    *sp = V[v < 0 || v >= BUB_VARIANTS ? 0 : v];
}

#ifndef BUB_GEN                                 /* not in the levels' tool (tools/bubbles_gen.c): it makes them */
#define BUB_LEVEL_DATA
#include "games/bubbles_levels.h"

const int bub_level_count = BUB_LEVELS;

int bub_chapter_of(int level)
{
    static const int LENS[BUB_CHAPTERS] = BUB_CH_LENS;
    for (int c = 0, at = 0; c < BUB_CHAPTERS; c++) {
        at += LENS[c];
        if (level < at) return c;
    }
    return BUB_CHAPTERS - 1;
}

int bub_open(bub_game *g, int level)
{
    bub_level L;
    if (level < 0 || level >= BUB_LEVELS || !bub_parse(&L, bub_levels[level])) return 0;
    bub_new(g, &L);
    return 1;
}

/* a puzzle at random: boxes of the variant made from the seed until one is solved and the obvious player cannot win it
 * (a few tries; else the last one solved) */
void bub_random(bub_game *g, int variant, uint32_t seed)
{
    bub_spec sp;
    bub_variant(variant, &sp);
    uint32_t r = pr_seed(seed);
    bub_level L, keep;
    int have = 0;
    for (int k = 0; k < 40; k++) {
        if (!bub_make(&L, &sp, pr_next(&r)) || !bub_settle(&L, &sp)) continue;
        if (L.nsol < 3 + variant) continue;
        keep = L;
        have = 1;
        bub_state s;
        bub_begin(&L, &s);
        if (bub_greedy(&L, &s) < 0) break;
    }
    if (!have) { bub_open(g, 0); return; }
    bub_new(g, &keep);
}
#endif
