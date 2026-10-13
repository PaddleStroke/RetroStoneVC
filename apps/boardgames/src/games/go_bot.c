/*
 * Go's bot (bots.h), every variant: 9 x 9, 13 x 13, 19 x 19 (area scoring, komi 7.5) and capture go (9 x 9,
 * 13 x 13: the first capture wins). Monte Carlo tree search with RAVE (games/rave.h) over the bot's own fast board
 * (groups kept as rings of stones with pseudo-liberties: a group is in atari when all its pseudo-liberties are one
 * point), light playouts with a little knowledge:
 *   - the playouts: capture the group the last move left in atari; save a group of one's own the last move put in
 *     atari (when the escape gives it two liberties); otherwise a point at random, never one's own eye, rarely a
 *     self-atari (never one of two stones or more); two passes end them, counted by area (stones and eyes, komi);
 *   - the tree's priors: captures and escapes first, self-ataris last, the first line poor and the third and fourth
 *     good in the opening, the points next to the last move a little better; one's own eyes are never searched.
 * Passing: the tree offers the pass only after the other's pass (or with nothing else to play); two passes end the
 * game there, counted as the stones stand (so the bot takes the dead stones out of its own area first, free under
 * area scoring). It passes when the other has passed and that count is already its win, when the search says so,
 * or late in the game when nothing is left to play for: every empty point settled (whose, in nine playouts out of
 * ten), no dead stone of the other's in its area, and the count as the stones stand agreeing with the playouts.
 * The counting (both passed, the bot to mark): a group standing in the other's area (most of the empty points round
 * it touch the other colour) is dead; the bot marks them so (its own too, honestly), one group a move, then is done.
 * The positional superko is the rules' (go_try): the root's moves are checked against the whole game; inside the
 * search, only the simple ko.
 * Strength (honestly): hard plays 9 x 9 at about a beginner-club level (it beats a beginner and a casual player);
 * on 13 x 13 it is a weak club player's opponent; on 19 x 19 a few thousand playouts a move are not much: it knows
 * captures, eyes and life and death in simple shapes, and plays reasonable shape locally, but its global sense is
 * thin (a beginner who has played a few dozen games will beat it). Medium is weaker, easy plays a few hundred
 * playouts (it makes the beginner's mistakes: it misses ataris and lets groups die).
 * Runs on the worker thread: it copies the game at entry (on the heap: the game keeps every position) and touches
 * nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("d4", "pass"); while counting, a point marks its group dead or alive and a pass
 * says the counting is done (go_script). Played by go_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/go.h"
#include "games/rave.h"

#define W_MAX (GO_MAX + 2)                      /* the board with a frame of edge points */
#define P_MAX (W_MAX * W_MAX)
#define PASS P_MAX                              /* the pass, as a move of the search */

enum { EMPTY, BLACK, WHITE, EDGE };             /* BLACK: ebony, player 0; WHITE: ivory, player 1 */

typedef struct {
    uint8_t col[P_MAX];
    int16_t head[P_MAX], next[P_MAX];           /* each stone's group (its head stone), the ring of its stones */
    int16_t lc[P_MAX], size[P_MAX];             /* a group (at its head): pseudo-liberties, stones */
    int32_t ls[P_MAX], ls2[P_MAX];              /* the pseudo-liberties' sum and sum of squares */
    int16_t empt[P_MAX], epos[P_MAX];           /* the empty points, and each one's place in the list */
    int nempty, n, w;                           /* n: the board's side, w = n + 2 (a row of the frame) */
    int side, ko, passes, last, moves, capture, winner, stones;
    int16_t atari[8];                           /* the stones of groups lately put in atari (a ring; may be stale) */
    int natari;
    uint8_t root;                               /* 1 until the first move: the root's banned points apply */
    uint8_t banned[P_MAX];                      /* the root: illegal by the rules (superko) */
    float *own;                                 /* the playouts' owners (the root's search), NULL */
} gst;

static int opp(int c) { return 3 - c; }
static int colour(int player) { return player + 1; }

static void add_lib(gst *s, int g, int p) { s->lc[g]++; s->ls[g] += p; s->ls2[g] += p * p; }
static void del_lib(gst *s, int g, int p) { s->lc[g]--; s->ls[g] -= p; s->ls2[g] -= p * p; }
static int in_atari(const gst *s, int g)
{
    return s->lc[g] > 0 && (int64_t)s->lc[g] * s->ls2[g] == (int64_t)s->ls[g] * s->ls[g];
}
static int atari_lib(const gst *s, int g) { return s->ls[g] / s->lc[g]; }

static void del_empty(gst *s, int p)
{
    int i = s->epos[p], q = s->empt[--s->nempty];
    s->empt[i] = (int16_t)q;
    s->epos[q] = (int16_t)i;
}

static void add_empty(gst *s, int p)
{
    s->epos[p] = (int16_t)s->nempty;
    s->empt[s->nempty++] = (int16_t)p;
}

static void merge(gst *s, int a, int b)         /* heads a and b: one group */
{
    if (s->size[a] < s->size[b]) { int t = a; a = b; b = t; }
    int q = b;
    do { s->head[q] = (int16_t)a; q = s->next[q]; } while (q != b);
    int t = s->next[a];
    s->next[a] = s->next[b];
    s->next[b] = (int16_t)t;
    s->lc[a] += s->lc[b];
    s->ls[a] += s->ls[b];
    s->ls2[a] += s->ls2[b];
    s->size[a] += s->size[b];
}

/* a stone of colour c on p, joined to its friends, the neighbours' liberties counted; no capture */
static void put(gst *s, int p, int c)
{
    const int d[4] = {1, -1, s->w, -s->w};
    s->col[p] = (uint8_t)c;
    del_empty(s, p);
    s->head[p] = s->next[p] = (int16_t)p;
    s->size[p] = 1;
    s->lc[p] = 0;
    s->ls[p] = s->ls2[p] = 0;
    s->stones++;
    for (int k = 0; k < 4; k++) {
        int q = p + d[k];
        if (s->col[q] == EMPTY) add_lib(s, p, q);
        else if (s->col[q] != EDGE) del_lib(s, s->head[q], p);
    }
    for (int k = 0; k < 4; k++) {
        int q = p + d[k];
        if (s->col[q] == c && s->head[q] != s->head[p]) merge(s, s->head[q], s->head[p]);
    }
}

static int take(gst *s, int g)                  /* the group off the board: its stones */
{
    const int d[4] = {1, -1, s->w, -s->w};
    int st[GO_PTS], k = 0, q = g;
    do { st[k++] = q; q = s->next[q]; } while (q != g);
    for (int i = 0; i < k; i++) {
        s->col[st[i]] = EMPTY;
        add_empty(s, st[i]);
    }
    for (int i = 0; i < k; i++)
        for (int j = 0; j < 4; j++) {
            int r = st[i] + d[j];
            if (s->col[r] == BLACK || s->col[r] == WHITE) add_lib(s, s->head[r], st[i]);
        }
    s->stones -= k;
    return k;
}

/* colour c may play p: empty, not the ko, not a suicide */
static int legal(const gst *s, int p, int c)
{
    const int d[4] = {1, -1, s->w, -s->w};
    if (s->col[p] != EMPTY || p == s->ko) return 0;
    for (int k = 0; k < 4; k++)
        if (s->col[p + d[k]] == EMPTY) return 1;
    for (int k = 0; k < 4; k++) {
        int q = p + d[k], g = s->head[q];
        if (s->col[q] == c && !in_atari(s, g)) return 1;
        if (s->col[q] == opp(c) && in_atari(s, g)) return 1;
    }
    return 0;
}

static int eye(const gst *s, int p, int c)      /* an eye of colour c's (the diagonals: at most one enemy, none at
                                                 * the edge) */
{
    const int d[4] = {1, -1, s->w, -s->w}, x[4] = {s->w + 1, s->w - 1, -s->w + 1, -s->w - 1};
    int bad = 0, edge = 0;
    for (int k = 0; k < 4; k++)
        if (s->col[p + d[k]] != c && s->col[p + d[k]] != EDGE) return 0;
    for (int k = 0; k < 4; k++) {
        int v = s->col[p + x[k]];
        bad += v == opp(c);
        edge += v == EDGE;
    }
    return edge ? bad == 0 : bad < 2;
}

/* the liberties the group of a stone of c's on p would have, up to 2 (2 also when it captures) */
static int libs_after(const gst *s, int p, int c)
{
    const int d[4] = {1, -1, s->w, -s->w};
    int libs[2], n = 0;
#define LIB(x) \
    do { int x_ = (x); if (x_ != p && (n == 0 || libs[0] != x_)) { if (n == 1) return 2; libs[n++] = x_; } } while (0)
    for (int k = 0; k < 4; k++)
        if (s->col[p + d[k]] == EMPTY) LIB(p + d[k]);
    for (int k = 0; k < 4; k++) {
        int q = p + d[k];
        if (s->col[q] == opp(c) && in_atari(s, s->head[q])) return 2;
    }
    for (int k = 0; k < 4; k++) {
        int q = p + d[k];
        if (s->col[q] != c) continue;
        int g = s->head[q], r = g;
        do {
            for (int j = 0; j < 4; j++)
                if (s->col[r + d[j]] == EMPTY) LIB(r + d[j]);
            r = s->next[r];
        } while (r != g);
    }
#undef LIB
    return n;
}

/* the playouts' quick test: plainly not a self-atari (two empty neighbours, or a friend with plenty of pseudo-
 * liberties: an estimate, as playouts may) */
static int roomy(const gst *s, int p, int c)
{
    const int d[4] = {1, -1, s->w, -s->w};
    int e = 0;
    for (int k = 0; k < 4; k++) {
        int q = p + d[k];
        if (s->col[q] == EMPTY && ++e >= 2) return 1;
        if (s->col[q] == c && s->lc[s->head[q]] >= 4 && !in_atari(s, s->head[q])) return 1;
    }
    return 0;
}

static int has_friend(const gst *s, int p, int c)
{
    return s->col[p + 1] == c || s->col[p - 1] == c || s->col[p + s->w] == c || s->col[p - s->w] == c;
}

/* area counting as the stones stand (the stones, and the empty regions that touch one colour only): black's points -
 * white's - komi > 0: black (player 0) wins */
static int count_winner(const gst *s, float *own)
{
    const int d[4] = {1, -1, s->w, -s->w};
    float score = -GO_KOMI;
    uint8_t seen[P_MAX];
    int16_t stack[P_MAX], region[P_MAX];
    memset(seen, 0, sizeof seen);
    for (int y = 1; y <= s->n; y++)
        for (int x = 1; x <= s->n; x++) {
            int p = y * s->w + x, who = s->col[p];
            if (who != EMPTY) {
                score += who == BLACK ? 1 : -1;
                if (own) own[p] += who == BLACK ? 1.0f : -1.0f;
                continue;
            }
            if (seen[p]) continue;
            int top = 0, k = 0, touch = 0;
            stack[top++] = (int16_t)p;
            seen[p] = 1;
            while (top) {
                int q = stack[--top];
                region[k++] = (int16_t)q;
                for (int j = 0; j < 4; j++) {
                    int r = q + d[j], c = s->col[r];
                    if (c == BLACK || c == WHITE) touch |= c;
                    else if (c == EMPTY && !seen[r]) { seen[r] = 1; stack[top++] = (int16_t)r; }
                }
            }
            float v = touch == BLACK ? 1.0f : touch == WHITE ? -1.0f : 0;
            score += v * (float)k;
            if (own)
                for (int i = 0; i < k; i++) own[region[i]] += v;
        }
    return score > 0 ? 0 : 1;
}

/* a move of the player to move: a point (legal) or PASS */
static void play(gst *s, int m)
{
    const int d[4] = {1, -1, s->w, -s->w};
    int c = colour(s->side);
    s->root = 0;
    s->moves++;
    if (m == PASS) {
        s->ko = 0;
        s->last = PASS;
        s->side ^= 1;
        if (++s->passes >= 2 && !s->capture) s->winner = count_winner(s, NULL);
        return;
    }
    put(s, m, c);
    int took = 0, single = -1;
    for (int k = 0; k < 4; k++) {
        int q = m + d[k];
        if (s->col[q] == opp(c) && s->lc[s->head[q]] == 0) {
            int t = take(s, s->head[q]);
            if (t == 1) single = q;
            took += t;
        }
    }
    int g = s->head[m];
    s->ko = took == 1 && s->size[g] == 1 && s->lc[g] == 1 ? single : 0;
    for (int k = 0; k < 4; k++) {               /* the groups now in atari, remembered for the playouts */
        int q = m + d[k];
        if (s->col[q] == opp(c) && in_atari(s, s->head[q])) s->atari[s->natari++ & 7] = (int16_t)q;
    }
    if (in_atari(s, g)) s->atari[s->natari++ & 7] = (int16_t)m;
    s->passes = 0;
    s->last = m;
    if (s->capture && took) s->winner = s->side;
    s->side ^= 1;
}

static void from_rules(const gogame *g, gst *s)
{
    memset(s, 0, sizeof *s);
    s->n = g->n;
    s->w = g->n + 2;
    for (int p = 0; p < P_MAX; p++) s->col[p] = EDGE;
    for (int y = 1; y <= s->n; y++)
        for (int x = 1; x <= s->n; x++) {
            int p = y * s->w + x;
            s->col[p] = EMPTY;
            add_empty(s, p);
        }
    const gopos *now = go_now(g);
    for (int i = 0; i < s->n * s->n; i++)
        if (now->pt[i] != GO_EMPTY) put(s, (i / s->n + 1) * s->w + i % s->n + 1, now->pt[i]);
    s->side = now->side;
    s->capture = g->capture;
    s->winner = -1;
    int last = go_last(g);
    s->last = last == GO_PASS ? PASS : last >= 0 ? (last / s->n + 1) * s->w + last % s->n + 1 : -1;
    s->passes = last == GO_PASS;
    s->root = 1;
    for (int p = 0; p < P_MAX && s->natari < 8; p++)   /* the groups in atari now, for the playouts */
        if ((s->col[p] == BLACK || s->col[p] == WHITE) && s->head[p] == p && in_atari(s, p))
            s->atari[s->natari++] = (int16_t)p;
    for (int i = 0; i < s->n * s->n; i++) {     /* the rules' own legality: ko and superko over the whole game */
        int p = (i / s->n + 1) * s->w + i % s->n + 1;
        gopos out;
        s->banned[p] = now->pt[i] == GO_EMPTY && go_try(g, i, &out) < 0;
    }
}

static int rules_point(const gst *s, int p) { return (p / s->w - 1) * s->n + p % s->w - 1; }

/* ---- the playouts ------------------------------------------------------------------------------------------------ */
static int capture_go_move(const gst *s, ai_rng *r)
{
    int c = colour(s->side), best = -1;
    for (int i = 0; i < s->nempty; i++) {       /* a capture wins at once */
        int p = s->empt[i];
        for (int k = 0, d[4] = {1, -1, s->w, -s->w}; k < 4; k++) {
            int q = p + d[k];
            if (s->col[q] == opp(c) && in_atari(s, s->head[q]) && legal(s, p, c)) return p;
        }
    }
    for (int i = 0; i < s->nempty && best < 0; i++) {   /* a group of one's own in atari: out of it */
        int p = s->empt[i];
        for (int k = 0, d[4] = {1, -1, s->w, -s->w}; k < 4; k++) {
            int q = p + d[k];
            if (s->col[q] == c && in_atari(s, s->head[q]) && legal(s, p, c) && libs_after(s, p, c) >= 2) best = p;
        }
    }
    if (best >= 0) return best;
    int n = s->nempty, start = n ? ai_rand_n(r, n) : 0, fallback = -1;
    for (int i = 0; i < n; i++) {
        int p = s->empt[(start + i) % n];
        if (!legal(s, p, c)) continue;
        if (libs_after(s, p, c) >= 2) return p;
        if (fallback < 0) fallback = p;
    }
    return fallback;                            /* only self-ataris (or nothing: -1, a loss) */
}

static int playout_move(const gst *s, ai_rng *r)
{
    int c = colour(s->side), last = s->last;
    const int d[4] = {1, -1, s->w, -s->w};
    if (last >= 0 && last != PASS && s->col[last] == opp(c)) {
        int g = s->head[last];
        if (in_atari(s, g)) {                   /* the last move's group in atari: take it */
            int l = atari_lib(s, g);
            if (legal(s, l, c)) return l;
        }
        for (int k = 0; k < 4; k++) {           /* a group of ours next to it in atari: out (two liberties) */
            int q = last + d[k];
            if (s->col[q] != c || !in_atari(s, s->head[q])) continue;
            int l = atari_lib(s, s->head[q]);
            if (legal(s, l, c) && libs_after(s, l, c) >= 2) return l;
        }
    }
    int escape = -1;
    for (int i = 0; i < 8 && i < s->natari; i++) {   /* the groups lately put in atari: take them, or get out */
        int q = s->atari[i];
        if (s->col[q] != BLACK && s->col[q] != WHITE) continue;
        int g = s->head[q];
        if (!in_atari(s, g)) continue;
        int l = atari_lib(s, g);
        if (s->col[q] == opp(c) && legal(s, l, c)) return l;
        if (s->col[q] == c && escape < 0 && legal(s, l, c) && libs_after(s, l, c) >= 2) escape = l;
    }
    if (escape >= 0) return escape;
    int n = s->nempty, start = n ? ai_rand_n(r, n) : 0;
    for (int i = 0, k = start; i < n; i++, k = k + 1 < n ? k + 1 : 0) {
        int p = s->empt[k];
        if (eye(s, p, c) || !legal(s, p, c)) continue;
        if (!roomy(s, p, c) && libs_after(s, p, c) < 2 && (has_friend(s, p, c) || ai_rand_n(r, 4))) continue;
        return p;                               /* (a self-atari: rarely, and never of two stones or more) */
    }
    return PASS;
}

static int r_playout(void *st, ai_rng *r, int *mv, uint8_t *pl, int cap, int *len)
{
    gst *s = st;
    int limit = s->moves + 3 * s->n * s->n;
    *len = 0;
    while (s->winner < 0) {
        if (s->capture) {
            int m = capture_go_move(s, r);
            if (m < 0) return !s->side;          /* nowhere to play: a loss */
            if (*len < cap) { mv[*len] = m; pl[*len] = (uint8_t)s->side; (*len)++; }
            play(s, m);
            if (s->moves > limit) return ai_rand_n(r, 2);
            continue;
        }
        if (s->moves > limit) { s->winner = count_winner(s, NULL); break; }
        int m = playout_move(s, r);
        if (m != PASS && *len < cap) { mv[*len] = m; pl[*len] = (uint8_t)s->side; (*len)++; }
        play(s, m);
    }
    if (s->own) count_winner(s, s->own);
    return s->winner;
}

/* ---- the tree ---------------------------------------------------------------------------------------------------- */
static int r_expand(const void *st, int *mv, float *prior, float *weight)
{
    const gst *s = st;
    const int d[4] = {1, -1, s->w, -s->w};
    int c = colour(s->side), k = 0, n = s->n, opening = s->stones < n * n / 5;
    if (s->winner >= 0) return 0;
    for (int i = 0; i < s->nempty; i++) {
        int p = s->empt[i];
        if ((s->root && s->banned[p]) || !legal(s, p, c)) continue;
        if (!s->capture && eye(s, p, c)) continue;
        float pr = 0.5f, wt = 0;
        int capture = 0, escape = 0;
        for (int j = 0; j < 4; j++) {
            int q = p + d[j];
            if (s->col[q] == opp(c) && in_atari(s, s->head[q])) capture += s->size[s->head[q]];
            if (s->col[q] == c && in_atari(s, s->head[q])) escape = 1;
        }
        int libs = libs_after(s, p, c);
        int x = p % s->w - 1, y = p / s->w - 1, line = 1 + (x < y ? x : y);
        int far = n - 1 - x < n - 1 - y ? n - x : n - y;
        if (far < line) line = far;
        if (capture) { pr = s->capture ? 1.0f : 0.85f; wt = 30 + 5 * capture; }
        else if (escape && libs >= 2) { pr = 0.75f; wt = 25; }
        else if (libs < 2) { pr = s->capture ? 0.02f : 0.15f; wt = 20; }
        else if (opening && line == 1) { pr = 0.3f; wt = 12; }
        else if (opening && (line == 3 || line == 4)) { pr = 0.6f; wt = 6; }
        else if (s->last >= 0 && s->last != PASS) {
            int lx = s->last % s->w - 1, ly = s->last / s->w - 1, dx = abs(lx - x), dy = abs(ly - y);
            if (dx <= 1 && dy <= 1) { pr = 0.58f; wt = 5; }
        }
        mv[k] = p;
        prior[k] = pr;
        weight[k] = wt;
        k++;
    }
    if (!s->capture && (s->passes || !k)) {     /* the pass: only after the other's, or with nothing else */
        mv[k] = PASS;
        prior[k] = 0.5f;
        weight[k] = 0;
        k++;
    }
    return k;
}

static void r_make(void *st, int m) { play(st, m); }
static int r_side(const void *st) { return ((const gst *)st)->side; }
static int r_over(const void *st, int *winner)
{
    const gst *s = st;
    if (s->winner < 0) return 0;
    *winner = s->winner;
    return 1;
}

static const rave_game RG = {
    .size = sizeof(gst), .moves_max = P_MAX + 1, .expand = r_expand, .make = r_make, .side = r_side,
    .over = r_over, .playout = r_playout, .rave_k = 1500, .explore = 0, .expand_at = 2,
};

/* ---- the choice -------------------------------------------------------------------------------------------------- */
static uint64_t board_key(const gogame *g)
{
    const gopos *p = go_now(g);
    uint64_t k = (uint64_t)g->n;
    for (int i = 0; i < g->n * g->n; i++) k = ai_mix(k, p->pt[i]);
    return k;
}

/* the counting's next step: a group to mark (its point) or GO_PASS (done). A group is dead when the area it stands
 * in (the points joined to it that are empty or its colour's) is the other's: more of that area's empty points touch
 * the other colour than its own (a stone or two left in the other's territory); otherwise alive. The marks differing
 * from that are changed one group a move. */
static int counting_move(const gogame *g)
{
    const gopos *now = go_now(g);
    int n = g->n, res = GO_PASS;
    uint8_t seen[GO_PTS];
    int stack[GO_PTS], area[GO_PTS];
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < n * n && res == GO_PASS; i++) {
        if (now->pt[i] == GO_EMPTY || seen[i]) continue;
        int c = now->pt[i], o = 3 - c, top = 0, k = 0, mine = 0, theirs = 0;
        uint8_t in[GO_PTS];
        memset(in, 0, sizeof in);
        stack[top++] = i;
        in[i] = 1;
        while (top) {                           /* the area: empty points and c's stones, joined */
            int q = stack[--top], x = q % n, y = q / n;
            area[k++] = q;
            int nb[4] = {x > 0 ? q - 1 : -1, x < n - 1 ? q + 1 : -1, y > 0 ? q - n : -1, y < n - 1 ? q + n : -1};
            int tc = 0, to = 0;
            for (int j = 0; j < 4; j++) {
                if (nb[j] < 0) continue;
                int v = now->pt[nb[j]];
                tc |= v == c;
                to |= v == o;
                if (v != o && !in[nb[j]]) { in[nb[j]] = 1; stack[top++] = nb[j]; }
            }
            if (now->pt[q] == GO_EMPTY) { mine += tc; theirs += to; }
        }
        int dead = theirs > mine;
        for (int j = 0; j < k && res == GO_PASS; j++) {
            int q = area[j];
            if (now->pt[q] != c) continue;
            seen[q] = 1;
            if (dead != (g->dead[q] != 0)) res = q;
        }
    }
    return res;
}

/* the move for the game (its player to move, or the counting), at a level: a point, GO_PASS; GO_NONE: none */
int gob_choose(const gogame *g, int level, uint64_t seed, ai_stats *stats)
{
    if (stats) memset(stats, 0, sizeof *stats);
    if (g->phase == GO_OVER) return GO_NONE;
    if (g->phase == GO_COUNT) return counting_move(g);
    if (!g->capture && (g->nmoves >= GO_HIST - 2 || !go_can_play(g))) return GO_PASS;
    gst *s = malloc(sizeof *s);
    float *own = calloc(P_MAX, sizeof(float));
    if (!s || !own) { free(s); free(own); return g->capture ? GO_NONE : GO_PASS; }
    from_rules(g, s);
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    int n = g->n, res;
    /* the other has passed and the count as the stones stand is already ours: done */
    if (!g->capture && s->passes && count_winner(s, NULL) == s->side) { free(s); free(own); return GO_PASS; }
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, board_key(g) ^ (uint64_t)g->nmoves << 48));
    /* playouts a move: hard about 0.3 s on a PC (a 9 x 9 playout is about 40 us, a 19 x 19 one 250 us) */
    static const long ITERS[3][AI_LEVELS] = {{250, 2500, 8000}, {200, 1000, 2500}, {150, 500, 1200}};
    long iters = ITERS[n == 9 ? 0 : n == 13 ? 1 : 2][level] * (g->capture ? 2 : 3) / 3;   /* capture go: slower */
    s->own = own;
    float rate;
    int m = rave_search(&RG, s, iters, ai_level_budget(level).ms * 85 / 100, &rng, &rate, NULL, stats);
    long played = stats ? stats->nodes : iters;
    if (m < 0) res = g->capture ? GO_NONE : GO_PASS;
    else if (m == PASS) res = GO_PASS;
    else {
        res = rules_point(s, m);
        /* late in the game, every empty point settled (whose, in nine playouts out of ten), no stone of the
         * other's left dead in our area, and the count as the stones stand saying what the playouts say: playing on
         * changes nothing, pass */
        int open = 0, me = colour(s->side);
        for (int p = 0; p < P_MAX && played > 0; p++) {
            float o = own[p] / (float)played, mine = me == BLACK ? o : -o;
            if (s->col[p] == EMPTY) open += o < 0.9f && o > -0.9f;
            else if (s->col[p] == opp(me)) open += mine > 0.5f;
        }
        if (!g->capture && played > 0 && s->stones > n * n / 2 && !open &&
            (count_winner(s, NULL) == s->side) == (rate >= 0.5f))
            res = GO_PASS;
    }
    free(s);
    free(own);
    return res;
}

static int think(int seat, int level, char *move, int cap)
{
    gogame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *go_live();                            /* the copy, at entry: nothing else is read */
    int ok = 0;
    if (g->phase != GO_OVER && go_now(g)->side == seat && cap >= 5) {
        int m = gob_choose(g, level, ai_session_seed(), NULL);
        if (m != GO_NONE && !ai_cancelled()) {
            static const char COLS[] = "abcdefghjklmnopqrst";
            if (m == GO_PASS) snprintf(move, (size_t)cap, "pass");
            else snprintf(move, (size_t)cap, "%c%d", COLS[m % g->n], m / g->n + 1);
            ok = 1;
        }
    }
    free(g);
    return ok;
}

static const bot go_bot = {.think = think, .play = go_show};
REGISTER_BOT(go, &go_bot)
