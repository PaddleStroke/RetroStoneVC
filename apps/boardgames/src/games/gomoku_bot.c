/*
 * Gomoku's bot (bots.h), every variant: freestyle and standard (15 x 15, five in a row; standard: exactly five),
 * renju (ebony's forbidden points never played: the rules' gk_forbidden() decides, after a quick test of the lines),
 * six in a row (19 x 19, two stones a turn, six or more win).
 * The bot keeps its own form of the position: every window of five (six in a row: six) points along a line, with the
 * stones of each colour in it, updated stone by stone (make / unmake). A window holding one colour only is that
 * player's chance: the evaluation adds them up by their stones (a window one stone short of a row: a threat).
 * Threats first (threat-space heuristics): a row to complete is played at once; the other's row to complete is
 * blocked (only those points are searched); then the points near the stones, ranked by what they make and what they
 * block, the best few searched by alpha-beta (ai.h), the moves that make a four searched past the horizon (a
 * quiescence of fours). Before the search, hard (and medium, shallower) looks for a win by continuous fours (a four,
 * the forced block, a four... until two points complete a row at once).
 * Easy: two plies (one in three moves only one: it overlooks an open three), a few points, noise. Medium: four plies,
 * the short four-search, a little noise. Hard: as deep as its budget allows. Six in a row: each stone is a ply.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("h8"), one stone at a time; played by gomoku_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/gomoku.h"

#define WMAX (4 * GK_MAXPTS)                    /* windows: a direction and a first point */
#define RANKED 1024                             /* a move: its point + RANKED * its rank in the list (its order) */

static const int DX[4] = {1, 0, 1, 1}, DY[4] = {0, 1, 1, -1};
/* a window's worth by its stones (one colour only in it): five in a row, six in a row */
static const int W5[7] = {0, 2, 14, 90, 600, 0, 0}, W6[7] = {0, 1, 8, 40, 300, 700, 0};

typedef struct {
    uint8_t pt[GK_MAXPTS];                      /* 0 empty, 1 ebony, 2 ivory */
    uint8_t cnt[WMAX][2];                       /* each window's stones of each colour */
    int total[2];                               /* the windows' worth, each colour's */
    int pure[2][7];                             /* the windows holding k stones of that colour only (k >= 1) */
    int variant, n, len, nmoves, winner;        /* len: a row's length (5, 6); winner -1, 0, 1, 2 a draw */
    uint64_t key;                               /* the stones' hash, kept stone by stone */
} gkb;

static uint64_t stone_key(int p, int c) { return ai_mix((uint64_t)p * 2 + (uint64_t)c + 1, 0x51ed270b); }

static int side_of(const gkb *s, int k) { return (s->variant == GK_SIX ? (k + 1) / 2 : k) & 1; }
static int to_move(const gkb *s) { return side_of(s, s->nmoves); }
static int left(const gkb *s) { return s->variant == GK_SIX && s->nmoves && (s->nmoves & 1) ? 2 : 1; }
static const int *worth(const gkb *s) { return s->len == 6 ? W6 : W5; }

/* the run of colour c's stones through p along direction d, p counted as c's */
static int run(const gkb *s, int p, int c, int d)
{
    int n = s->n, x0 = p % n, y0 = p / n, k = 1, who = c + 1;
    for (int sg = -1; sg <= 1; sg += 2)
        for (int x = x0 + sg * DX[d], y = y0 + sg * DY[d];
             x >= 0 && y >= 0 && x < n && y < n && s->pt[x + y * n] == who;
             x += sg * DX[d], y += sg * DY[d])
            k++;
    return k;
}

static int wins_len(const gkb *s, int c, int len)
{
    if (s->variant == GK_RENJU) return c == 0 ? len == 5 : len >= 5;
    return s->variant == GK_STANDARD ? len == 5 : s->variant == GK_SIX ? len >= 6 : len >= 5;
}

/* a stone of c's on the empty point p wins */
static int wins_at(const gkb *s, int p, int c)
{
    for (int d = 0; d < 4; d++)
        if (wins_len(s, c, run(s, p, c, d))) return 1;
    return 0;
}

/* the windows through p (their indices, at most 4 x 6) */
static int windows_at(const gkb *s, int p, int *out)
{
    int n = s->n, x = p % n, y = p / n, L = s->len, k = 0;
    for (int d = 0; d < 4; d++)
        for (int i = 0; i < L; i++) {
            int sx = x - i * DX[d], sy = y - i * DY[d], ex = sx + (L - 1) * DX[d], ey = sy + (L - 1) * DY[d];
            if (sx < 0 || sy < 0 || sx >= n || sy >= n || ex < 0 || ey < 0 || ex >= n || ey >= n) continue;
            out[k++] = d * GK_MAXPTS + sx + sy * n;
        }
    return k;
}

static void put(gkb *s, int p, int c)
{
    const int *W = worth(s);
    int win[24], nw = windows_at(s, p, win);
    s->pt[p] = (uint8_t)(c + 1);
    s->key ^= stone_key(p, c);
    for (int i = 0; i < nw; i++) {
        int w = win[i], a = s->cnt[w][c], b = s->cnt[w][!c];
        if (!b) {
            if (a) { s->total[c] -= W[a]; s->pure[c][a]--; }
            s->total[c] += W[a + 1];
            s->pure[c][a + 1]++;
        } else if (!a) {
            s->total[!c] -= W[b];
            s->pure[!c][b]--;
        }
        s->cnt[w][c]++;
    }
}

static void lift(gkb *s, int p, int c)
{
    const int *W = worth(s);
    int win[24], nw = windows_at(s, p, win);
    s->pt[p] = 0;
    s->key ^= stone_key(p, c);
    for (int i = 0; i < nw; i++) {
        int w = win[i], a = --s->cnt[w][c], b = s->cnt[w][!c];
        if (!b) {
            s->total[c] -= W[a + 1];
            s->pure[c][a + 1]--;
            if (a) { s->total[c] += W[a]; s->pure[c][a]++; }
        } else if (!a) {
            s->total[!c] += W[b];
            s->pure[!c][b]++;
        }
    }
}

static void from_rules(const gkgame *g, gkb *s)
{
    memset(s, 0, sizeof *s);
    s->variant = g->variant;
    s->n = g->n;
    s->len = g->variant == GK_SIX ? 6 : 5;
    s->winner = -1;
    for (int k = 0; k < g->nmoves; k++) put(s, g->move[k], side_of(s, k));
    s->nmoves = g->nmoves;
    int st = gk_status(g);
    s->winner = st == GK_PLAY ? -1 : st == GK_WIN0 ? 0 : st == GK_WIN1 ? 1 : 2;
}

/* renju: may ebony play the empty point p? A quick test first (a double three, a double four or an overline needs
 * stones of ebony's in two lines through p, or a run of six), then the rules' own test */
static int forbidden(const gkb *s, int p)
{
    if (s->variant != GK_RENJU || to_move(s) != 0) return 0;
    int lines = 0, win[24], nw = windows_at(s, p, win), most[4] = {0, 0, 0, 0};
    for (int i = 0; i < nw; i++) {              /* each line: the most ebony stones in a window free of ivory */
        int w = win[i], d = w / GK_MAXPTS;
        if (!s->cnt[w][1] && s->cnt[w][0] > most[d]) most[d] = s->cnt[w][0];
    }
    for (int d = 0; d < 4; d++) {
        if (run(s, p, 0, d) >= 6) { lines = 2; break; }
        lines += most[d] >= 3 ? 2 : most[d] >= 2;    /* a line can hold two fours */
    }
    if (lines < 2) return 0;
    gkgame g;                                   /* the rules' test reads the stones only */
    memset(&g, 0, sizeof g);
    g.variant = s->variant;
    g.n = s->n;
    g.nmoves = s->nmoves;
    memcpy(g.pt, s->pt, sizeof s->pt);
    return gk_forbidden(&g, p);
}

/* the empty points of colour c's windows holding at least k of its stones (and none of the other's), each once */
static int window_points(const gkb *s, int c, int k, int *out, uint8_t *seen)
{
    int n = 0, N = s->n, L = s->len;
    for (int d = 0; d < 4; d++)
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) {
                int w = d * GK_MAXPTS + x + y * N;
                if (s->cnt[w][c] < k || s->cnt[w][!c]) continue;
                int ex = x + (L - 1) * DX[d], ey = y + (L - 1) * DY[d];
                if (ex < 0 || ey < 0 || ex >= N || ey >= N) continue;
                for (int i = 0; i < L; i++) {
                    int q = x + i * DX[d] + (y + i * DY[d]) * N;
                    if (!s->pt[q] && !seen[q]) { seen[q] = 1; out[n++] = q; }
                }
            }
    for (int i = 0; i < n; i++) seen[out[i]] = 0;
    return n;
}

/* colour c's threats: a window one turn from a row (five in a row: four stones; six: four or five) */
static int threatened(const gkb *s, int c)
{
    return s->len == 5 ? s->pure[c][4] > 0 : s->pure[c][4] + s->pure[c][5] > 0;
}

/* the points where the side to move wins with this stone (or, six in a row, a stone of a pair that wins) */
static int win_points(const gkb *s, int *out, uint8_t *seen)
{
    int c = to_move(s), k = s->len - left(s);
    if (s->pure[c][k] + (k + 1 <= s->len ? s->pure[c][k + 1] : 0) == 0) return 0;
    int pts[GK_MAXPTS], m = window_points(s, c, k, pts, seen), n = 0;
    for (int i = 0; i < m; i++) {
        int p = pts[i];
        if (left(s) == 1 ? wins_at(s, p, c) : 1) {
            if (forbidden(s, p)) continue;
            out[n++] = p;
        }
    }
    return n;
}

/* a candidate's promise for colour c: what it adds to c's windows and takes from the other's */
static int promise(const gkb *s, int p, int c)
{
    const int *W = worth(s);
    int v = 0, win[24], nw = windows_at(s, p, win);
    for (int i = 0; i < nw; i++) {
        int w = win[i], a = s->cnt[w][c], b = s->cnt[w][!c];
        if (!b) v += W[a + 1] - W[a];
        else if (!a) v += W[b] * 7 / 8;
    }
    return v;
}

/* the points searched in a position: the best few */
static int beam_of(const gkb *s) { return s->n >= 19 ? 8 : 10; }

/* the candidates (sorted, best first), at most `beam` */
static int candidates(const gkb *s, int *out, int beam)
{
    int c = to_move(s), N = s->n;
    uint8_t near[GK_MAXPTS] = {0}, seen[GK_MAXPTS] = {0};
    int pts[GK_MAXPTS], m = 0;
    if (s->winner >= 0) return 0;
    /* forced: a win now; the other's row to stop */
    m = win_points(s, pts, seen);
    if (m) { out[0] = pts[0]; return 1; }
    if (threatened(s, !c)) {                    /* five in a row: the points that complete the other's row */
        m = window_points(s, !c, 4, pts, seen);
        int n = 0;
        for (int i = 0; i < m; i++)
            if ((s->len == 6 || wins_at(s, pts[i], !c)) && !forbidden(s, pts[i])) pts[n++] = pts[i];
        if (n) { m = n; goto rank; }
    }
    m = 0;
    if (!s->nmoves) { out[0] = N / 2 + N / 2 * N; return 1; }
    if (s->variant == GK_RENJU && s->nmoves < 3) {      /* the opening: near the middle */
        int ctr = N / 2;
        for (int y = ctr - s->nmoves; y <= ctr + s->nmoves; y++)
            for (int x = ctr - s->nmoves; x <= ctr + s->nmoves; x++)
                if (!s->pt[x + y * N]) pts[m++] = x + y * N;
        goto rank;
    }
    for (int p = 0; p < N * N; p++) {
        if (!s->pt[p]) continue;
        int x0 = p % N, y0 = p / N;
        for (int y = y0 - 2; y <= y0 + 2; y++)
            for (int x = x0 - 2; x <= x0 + 2; x++)
                if (x >= 0 && y >= 0 && x < N && y < N) near[x + y * N] = 1;
    }
    for (int p = 0; p < N * N; p++)
        if (near[p] && !s->pt[p]) pts[m++] = p;
rank:;
    int sc[GK_MAXPTS], n = 0;
    for (int i = 0; i < m; i++) {
        int p = pts[i], v = promise(s, p, c), j;
        if (n < beam) j = n++;                  /* kept: the best `beam`, sorted */
        else if (v <= sc[beam - 1]) continue;
        else j = beam - 1;
        while (j > 0 && sc[j - 1] < v) { sc[j] = sc[j - 1]; out[j] = out[j - 1]; j--; }
        sc[j] = v;
        out[j] = p;
    }
    /* renju: ebony's forbidden points out (checked on the kept ones only) */
    int k = 0;
    for (int i = 0; i < n; i++)
        if (!forbidden(s, out[i])) out[k++] = out[i];
    return k;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves_beam(const void *st, int *out, int beam)
{
    const gkb *s = st;
    int n = candidates(s, out, beam);
    for (int i = 0; i < n; i++) out[i] += RANKED * i;
    return n;
}
static int a_moves(const void *st, int *out) { return a_moves_beam(st, out, beam_of(st)); }

static void a_make(void *st, int m)
{
    gkb *s = st;
    int p = m % RANKED, c = to_move(s);
    put(s, p, c);
    s->nmoves++;
    if (wins_at(s, p, c)) s->winner = c;
    else if (s->nmoves >= s->n * s->n) s->winner = 2;
}

static void a_unmake(void *st, int m)
{
    gkb *s = st;
    s->nmoves--;
    lift(s, m % RANKED, to_move(s));
    s->winner = -1;
}

static int a_side(const void *st) { return to_move(st); }

static int a_over(const void *st, int *score)
{
    const gkb *s = st;
    if (s->winner < 0) return 0;
    *score = s->winner == 2 ? 0 : s->winner == to_move(s) ? ai_win(0) : -ai_win(0);
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const gkb *s = st;
    return s->key ^ (uint64_t)s->nmoves << 54;
}

/* the side to move's view; about 100 an open three */
static int a_eval(const void *st)
{
    const gkb *s = st;
    int c = to_move(s);
    if (threatened(s, c) && (s->len == 6 ? (left(s) == 2 || s->pure[c][5]) : 1)) {
        uint8_t seen[GK_MAXPTS] = {0};
        int pts[GK_MAXPTS];
        if (win_points(s, pts, seen)) return 30000;   /* a row to complete now */
    }
    if (s->len == 5 && s->pure[!c][4] > 1) {    /* two of the other's fours: two points to stop? */
        uint8_t seen[GK_MAXPTS] = {0};
        int pts[GK_MAXPTS];
        if (window_points(s, !c, 4, pts, seen) > 1) return -30000;
    }
    return s->total[c] * 6 / 5 - s->total[!c];
}

static int a_order(const void *st, int m)
{
    (void)st;
    return -(m / RANKED);
}

/* past the horizon: a win, the block of a four, the moves that make a four (five in a row only) */
static int a_noisy(const void *st, int *out)
{
    const gkb *s = st;
    int c = to_move(s), pts[GK_MAXPTS], n = 0;
    uint8_t seen[GK_MAXPTS] = {0};
    if (s->winner >= 0) return 0;
    if ((n = win_points(s, pts, seen))) { out[0] = pts[0]; return 1; }
    if (threatened(s, !c)) return candidates(s, out, 4);
    if (s->len != 5 || !s->pure[c][3]) return 0;
    int m = window_points(s, c, 3, pts, seen);
    for (int i = 0; i < m && n < 12; i++)
        if (!forbidden(s, pts[i])) out[n++] = pts[i];
    return n;
}

static const ai_game ADAPTER = {
    .size = sizeof(gkb), .moves = a_moves, .make = a_make, .unmake = a_unmake, .side = a_side, .over = a_over,
    .eval = a_eval, .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

/* ---- the win by continuous fours (five in a row) ----------------------------------------------------------------- */
typedef struct { long nodes, cap; double deadline; int stop; } vcfx;

/* a four of the side to move's, the forced block, again...: 1 and its first move in *first when it wins */
static int vcf(gkb *s, int depth, vcfx *x, int *first)
{
    int c = to_move(s), pts[GK_MAXPTS], m;
    uint8_t seen[GK_MAXPTS] = {0};
    if (x->stop || depth <= 0) return 0;
    if (++x->nodes > x->cap || ((x->nodes & 127) == 0 && (ai_cancelled() || ai_now_ms() > x->deadline))) {
        x->stop = 1;
        return 0;
    }
    if (!s->pure[c][3]) return 0;
    m = window_points(s, c, 3, pts, seen);
    for (int i = 0; i < m; i++) {
        int p = pts[i];
        if (forbidden(s, p)) continue;
        a_make(s, p);
        int ok = 0;
        if (s->winner == c) ok = 1;
        else if (s->winner < 0 && s->pure[c][4]) {
            int blocks[GK_MAXPTS], nb = 0;
            uint8_t seen2[GK_MAXPTS] = {0};
            int w = window_points(s, c, 4, blocks, seen2);
            for (int j = 0; j < w; j++) nb += wins_at(s, blocks[j], c);   /* the points that really make a row */
            if (nb >= 2) ok = 1;
            else if (nb == 1) {
                int b = -1;
                for (int j = 0; j < w && b < 0; j++)
                    if (wins_at(s, blocks[j], c)) b = blocks[j];
                a_make(s, b);                   /* the other's forced block */
                if (s->winner < 0 && !threatened(s, c ^ 1)) {
                    int f;
                    ok = vcf(s, depth - 1, x, &f);
                }
                a_unmake(s, b);
            }
        }
        a_unmake(s, p);
        if (ok) { *first = p; return 1; }
    }
    return 0;
}

/* the stone for the position (its side to move), at a level; -1: none */
int gkb_choose(const gkgame *g, int level, uint64_t seed, ai_stats *stats)
{
    double t0 = ai_now_ms();
    gkb *s = malloc(sizeof *s);
    if (!s) return -1;
    from_rules(g, s);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(s)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    o.budget.ms = o.budget.ms * 85 / 100;       /* a margin under the level's time (a slow or busy machine) */
    int vcf_depth = 0;
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:
        o.budget.depth = ai_rand_n(&rng, 3) ? 2 : 1;
        o.budget.nodes = 3000;
        o.budget.noise = 60;
        break;
    case AI_MEDIUM: o.budget.depth = 4; o.budget.nodes = 30000; o.budget.noise = 15; vcf_depth = 4; break;
    default:        o.budget.nodes = s->len == 6 ? 100000 : 150000; vcf_depth = 12; break;   /* about 0.2 s on a PC */
    }
    int best = -1, c = to_move(s);
    if (stats) memset(stats, 0, sizeof *stats);
    if (vcf_depth && s->len == 5 && s->winner < 0) {   /* a win by fours, unless the other has one to complete */
        uint8_t seen[GK_MAXPTS] = {0};
        int pts[GK_MAXPTS];
        vcfx x = {0, level >= AI_HARD ? 40000 : 3000, ai_now_ms() + (level >= AI_HARD ? 200 : 60), 0};
        if (!win_points(s, pts, seen) && !threatened(s, !c)) {
            int f;
            if (vcf(s, vcf_depth, &x, &f)) best = f;
        }
    }
    if (best < 0) {
        int spent = (int)(ai_now_ms() - t0);   /* the four-search's time comes off the budget */
        if (o.budget.ms > 0) o.budget.ms = o.budget.ms - spent > 50 ? o.budget.ms - spent : 50;
        int m = ai_ab(&ADAPTER, s, &o, stats);
        best = m < 0 ? -1 : m % RANKED;
    }
    free(s);
    return best;
}

static int think(int seat, int level, char *move, int cap)
{
    gkgame g = *gomoku_live();                  /* the copy, at entry: nothing else is read */
    if (gk_status(&g) != GK_PLAY || gk_side(&g) != seat || cap < 4) return 0;
    int p = gkb_choose(&g, level, ai_session_seed(), NULL);
    if (p < 0 || ai_cancelled()) return 0;
    snprintf(move, (size_t)cap, "%c%d", 'a' + p % g.n, p / g.n + 1);
    return 1;
}

static const bot gomoku_bot = {.think = think, .play = gomoku_show};
REGISTER_BOT(gomoku, &gomoku_bot)
