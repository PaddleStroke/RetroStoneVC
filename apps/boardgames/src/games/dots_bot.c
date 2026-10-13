/*
 * Dots and boxes' bot (bots.h), every board (3 x 3, 5 x 5, 7 x 7 boxes). The game's own theory more than a search:
 *   - A line is safe when it gives no box its third side. While safe lines are left, a box offered is taken.
 *   - With no safe line left the board is cut into chains (boxes in a row, open at both ends) and loops; whoever must
 *     draw opens one, the other takes it and keeps control by double-dealing (taking all but the last two boxes of a
 *     chain, all but four of a loop, and handing those over so the opener must open the next one). The bot values
 *     such endgames exactly over the chains and loops (a small memoised search over their lengths: for each one to
 *     open, the other takes all and moves, or takes all but two (four) and gives the move back) and plays the line
 *     that gives the least away: a chain of two is opened in its middle (no double-deal possible), a long chain at an
 *     end. A shape with a junction is valued as a chain of its boxes (an estimate).
 *   - When boxes can be taken, it takes them all or keeps control (double-deals) as the values say.
 *   - Before that, hard searches the last fourteen safe lines (alpha-beta, ai.h; the fight over who must open the
 *     first chain, the long-chain rule), their ends valued exactly as above, the long-chain rule's parity (the first
 *     player wants an even number of long chains on these boards) breaking the ties the search cannot see through.
 *   - The last lines (hard: 16, medium: 8) are searched one by one, every line (sacrifices too), to the end.
 * Easy takes every box it is offered (one time in seven it misses one), draws safe lines at random (one in ten: any
 * line) and, at the end, opens the chain that gives the fewest boxes at once; it never double-deals. Medium draws
 * its safe lines at random, then plays the endgame by the values, keeping control three times in four. Hard does it
 * all, always.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("h1,1", "v2,1"), one line at a time; played by dots_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/dots.h"

/* ---- the board's structure --------------------------------------------------------------------------------------- */
static int sides_drawn(const dpos *p, int b)
{
    int s[4], k = 0;
    dots_sides(p->n, b, s);
    for (int i = 0; i < 4; i++) k += p->line[s[i]] >= 0;
    return k;
}

/* the boxes on either side of a line (one at the edge) */
static int boxes_of(int n, int line, int *b)
{
    int x, y, x1, y1, k = 0;
    dots_ends(n, line, &x, &y, &x1, &y1);
    if (y1 == y) {
        if (y > 0) b[k++] = (y - 1) * n + x;
        if (y < n) b[k++] = y * n + x;
    } else {
        if (x > 0) b[k++] = y * n + x - 1;
        if (x < n) b[k++] = y * n + x;
    }
    return k;
}

static int safe(const dpos *p, int line)        /* undrawn, and no box gets its third side */
{
    int b[2], k = boxes_of(p->n, line, b);
    if (p->line[line] >= 0) return 0;
    for (int i = 0; i < k; i++)
        if (sides_drawn(p, b[i]) == 2) return 0;
    return 1;
}

static int safe_lines(const dpos *p, int *out)
{
    int n = 0;
    for (int l = 0; l < dots_lines(p->n); l++)
        if (safe(p, l)) out[n++] = l;
    return n;
}

static int undrawn_side(const dpos *p, int b, int not)   /* a box's undrawn side, other than `not` */
{
    int s[4];
    dots_sides(p->n, b, s);
    for (int i = 0; i < 4; i++)
        if (p->line[s[i]] < 0 && s[i] != not) return s[i];
    return -1;
}

static int other_box(int n, int line, int b)    /* the box across the line from b, -1: the edge */
{
    int bb[2], k = boxes_of(n, line, bb);
    for (int i = 0; i < k; i++)
        if (bb[i] != b) return bb[i];
    return -1;
}

static int a_box_to_take(const dpos *p)         /* a box with three sides, -1 */
{
    for (int b = 0; b < p->n * p->n; b++)
        if (p->box[b] < 0 && sides_drawn(p, b) == 3) return b;
    return -1;
}

/* ---- the chains and loops' values -------------------------------------------------------------------------------
 * A position with nothing to take and no safe line: its components (boxes joined by undrawn lines), each a chain
 * (code: its length) or a loop (64 + its length); a shape with a junction counts as a chain of its boxes. */
#define LOOP 64
#define MEMO_BITS 14

typedef struct { uint64_t key; int value; } memo_e;
typedef struct { memo_e *e; } memo;

static int components(const dpos *p, int *code)
{
    int n = p->n, nc = 0, seen[DOTS_MAX_BOXES] = {0}, stack[DOTS_MAX_BOXES];
    for (int b0 = 0; b0 < n * n; b0++) {
        if (p->box[b0] >= 0 || seen[b0]) continue;
        int top = 0, boxes = 0, ground = 0, junction = 0;
        stack[top++] = b0;
        seen[b0] = 1;
        while (top) {
            int b = stack[--top], s[4], deg = 0;
            boxes++;
            dots_sides(n, b, s);
            for (int i = 0; i < 4; i++) {
                if (p->line[s[i]] >= 0) continue;
                deg++;
                int o = other_box(n, s[i], b);
                if (o < 0) { ground++; continue; }
                if (!seen[o]) { seen[o] = 1; stack[top++] = o; }
            }
            junction |= deg != 2;
        }
        code[nc++] = !junction && !ground ? LOOP + boxes : boxes;
    }
    return nc;
}

static uint64_t multiset_key(const int *c, int n)   /* sorted codes, hashed */
{
    uint64_t k = 0x9e3779b97f4a7c15ull;
    for (int i = 0; i < n; i++) k = ai_mix(k, (uint64_t)c[i]);
    return k | 1;
}

/* the value (my boxes - theirs, of those left) for the player who must open one of the components (sorted codes) */
static int value(memo *m, const int *c, int n)
{
    if (!n) return 0;
    uint64_t key = multiset_key(c, n);
    memo_e *e = NULL;
    if (m && m->e) {                            /* a few probes; a full table just stops remembering */
        uint64_t i = key & ((1u << MEMO_BITS) - 1);
        for (int probe = 0; probe < 32; probe++, i = (i + 1) & ((1u << MEMO_BITS) - 1)) {
            if (m->e[i].key == key) return m->e[i].value;
            if (!m->e[i].key) { e = &m->e[i]; break; }
        }
    }
    int best = -1000, rest[DOTS_MAX_BOXES];
    for (int i = 0; i < n; i++) {
        if (i && c[i] == c[i - 1]) continue;    /* the same kind: the same value */
        int k = 0;
        for (int j = 0; j < n; j++)
            if (j != i) rest[k++] = c[j];
        int v = value(m, rest, k), L = c[i] >= LOOP ? c[i] - LOOP : c[i], them;
        if (c[i] >= LOOP) them = L >= 4 ? (L + v > L - 8 - v ? L + v : L - 8 - v) : L + v;
        else them = L >= 3 ? (L + v > L - 4 - v ? L + v : L - 4 - v) : L + v;
        if (-them > best) best = -them;
    }
    if (e) { e->key = key; e->value = best; }
    return best;
}

static void sort_codes(int *c, int n)
{
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && c[j] < c[j - 1]; j--) { int t = c[j]; c[j] = c[j - 1]; c[j - 1] = t; }
}

/* the value for the side to move of a position with nothing to take: no line left 0; safe lines left: unknown (0,
 * *known = 0); else the chains' */
static int quiet_value(memo *m, const dpos *p, int *known)
{
    int l[DOTS_MAX_LINES];
    *known = 1;
    if (p->drawn == dots_lines(p->n)) return 0;
    if (safe_lines(p, l)) { *known = 0; return 0; }
    int c[DOTS_MAX_BOXES], n = components(p, c);
    sort_codes(c, n);
    return value(m, c, n);
}

/* ---- taking boxes -------------------------------------------------------------------------------------------------
 * The boxes the side to move can take, followed from each box with three sides along the boxes with two: a string
 * ending at the edge or a junction (it can keep the last two: a double-deal), or at another box with three sides (a
 * loop opened: it can keep the last four; two boxes sharing their last line: nothing to keep). */
typedef struct {
    int k;                                      /* the boxes taken by taking them all */
    dpos rest;                                  /* the position after (the same player to move) */
    int keep;                                   /* the boxes a double-deal hands over: 2, 4, 0 none */
    int string;                                 /* the start box of the string kept for it */
} taking;

/* the string from box a (three sides): its length and its end (0 the edge or a junction, 1 another such box) */
static int follow(const dpos *p, int a, int *meet, int *last, int *before)
{
    int len = 1, cur = a, from_line = -1;
    *meet = 0;
    *before = -1;
    for (;;) {
        int l = undrawn_side(p, cur, from_line);
        *last = cur;
        if (l < 0) return len;
        int o = other_box(p->n, l, cur);
        if (o < 0) return len;                  /* the edge */
        int sd = sides_drawn(p, o);
        if (sd == 3) { *meet = 1; *before = cur; *last = o; return len + 1; }
        if (sd != 2) return len;                /* a junction */
        *before = cur;
        cur = o;
        from_line = l;
        len++;
    }
}

static void take_all(const dpos *p, taking *t)
{
    t->rest = *p;
    t->k = 0;
    t->keep = 0;
    t->string = -1;
    int b;
    while ((b = a_box_to_take(&t->rest)) >= 0) {
        int c = dots_make(&t->rest, undrawn_side(&t->rest, b, -1));
        t->k += c > 0 ? c : 0;
    }
    for (int a = 0; a < p->n * p->n; a++) {     /* a string to double-deal: a chain's first (cheaper), a loop's */
        if (p->box[a] >= 0 || sides_drawn(p, a) != 3) continue;
        int meet, last, before, len = follow(p, a, &meet, &last, &before);
        if (!meet && len >= 2 && t->keep != 2) { t->keep = 2; t->string = a; }
        if (meet && len >= 4 && !t->keep) { t->keep = 4; t->string = a; }
    }
}

/* the value for the side to move (boxes left: its - theirs) when it can take boxes; *dd: 1 if a double-deal is best */
static int take_value(memo *m, const dpos *p, int *dd, int allow_dd)
{
    taking t;
    take_all(p, &t);
    int known, v = quiet_value(m, &t.rest, &known);
    int all = t.k + v;
    *dd = 0;
    if (allow_dd && known && t.keep && t.rest.drawn < dots_lines(p->n)) {
        int deal = t.k - 2 * t.keep - v;
        if (deal > all) { *dd = 1; return deal; }
    }
    return all;
}

/* the value for the side to move, whatever the position (safe lines left: 0, unknown) */
static int any_value(memo *m, const dpos *p, int allow_dd)
{
    int dd, known;
    if (a_box_to_take(p) >= 0) return take_value(m, p, &dd, allow_dd);
    return quiet_value(m, p, &known);
}

/* the line that takes or double-deals, by the plan (dd: keep control) */
static int taking_line(const dpos *p, int dd)
{
    taking t;
    take_all(p, &t);
    int a = -1;
    for (int b = 0; b < p->n * p->n && a < 0; b++)   /* first the boxes outside the string kept */
        if (p->box[b] < 0 && sides_drawn(p, b) == 3) {
            if (dd && t.keep) {
                int meet, last, before;
                follow(p, t.string, &meet, &last, &before);
                if (b == t.string || b == last) continue;
            }
            a = b;
        }
    if (a >= 0) return undrawn_side(p, a, -1);
    if (!dd || !t.keep) {
        int b = a_box_to_take(p);
        return undrawn_side(p, b, -1);
    }
    int meet, last, before, len = follow(p, t.string, &meet, &last, &before);
    if (len > t.keep) return undrawn_side(p, t.string, -1);   /* take one more from its start */
    if (!meet) {                                /* two left: the far side of the second, not the line between */
        int between = undrawn_side(p, t.string, -1);
        return undrawn_side(p, last, between);
    }
    /* four of a loop left: a b c d, a and d with three sides: the line between b and c */
    int l1 = undrawn_side(p, t.string, -1), b = other_box(p->n, l1, t.string);
    int l2 = undrawn_side(p, b, l1);
    return l2;
}

/* ---- the search of the safe lines (hard, medium) ----------------------------------------------------------------- */
typedef struct {
    dpos p;
    memo *m;                                    /* the think's own table (not shared: one per think) */
} dst;

static int long_chains(const dpos *p)           /* the chains of three boxes or more among the boxes with two sides */
{
    int n = p->n, seen[DOTS_MAX_BOXES] = {0}, stack[DOTS_MAX_BOXES], count = 0;
    for (int b0 = 0; b0 < n * n; b0++) {
        if (p->box[b0] >= 0 || seen[b0] || sides_drawn(p, b0) != 2) continue;
        int top = 0, k = 0;
        stack[top++] = b0;
        seen[b0] = 1;
        while (top) {
            int b = stack[--top], s[4];
            k++;
            dots_sides(n, b, s);
            for (int i = 0; i < 4; i++) {
                if (p->line[s[i]] >= 0) continue;
                int o = other_box(n, s[i], b);
                if (o >= 0 && !seen[o] && p->box[o] < 0 && sides_drawn(p, o) == 2) { seen[o] = 1; stack[top++] = o; }
            }
        }
        count += k >= 3;
    }
    return count;
}

static int s_moves(const void *st, int *out)
{
    const dst *s = st;
    int n = safe_lines(&s->p, out);
    return n;
}

static void s_make(void *st, int line) { dots_make(&((dst *)st)->p, line); }
static int s_side(const void *st) { return ((const dst *)st)->p.side; }

static int s_over(const void *st, int *score)
{
    const dst *s = st;
    if (s->p.drawn < dots_lines(s->p.n)) return 0;
    *score = 100 * (s->p.boxes[s->p.side] - s->p.boxes[!s->p.side]);   /* the margin, as the evaluation counts */
    return 1;
}

/* the end searched line by line (few lines left): every line, those that take a box first, then the safe ones */
static int x_moves(const void *st, int *out)
{
    const dst *s = st;
    int n = 0;
    for (int l = 0; l < dots_lines(s->p.n); l++)
        if (s->p.line[l] < 0) out[n++] = l;
    return n;
}

static int x_order(const void *st, int l)
{
    const dst *s = st;
    int b[2], k = boxes_of(s->p.n, l, b), v = 1;
    for (int i = 0; i < k; i++) {
        int d = sides_drawn(&s->p, b[i]);
        if (d == 3) return 3;
        if (d == 2) v = -1;
    }
    return v;
}

/* the side to move's view at the horizon: the boxes won, and what is left valued as well as known */
static int x_eval(const void *st)
{
    const dst *s = st;
    const dpos *p = &s->p;
    int me = p->side, v = 100 * (p->boxes[me] - p->boxes[!me]), known = 1, dd, end;
    if (a_box_to_take(p) >= 0) end = take_value(s->m, p, &dd, 1);
    else end = quiet_value(s->m, p, &known);
    return v + (known ? 100 * end : 0);
}

/* the side to move's view, 100 a box: the boxes won, and the end's value (exact once no safe line is left) */
static int s_eval(const void *st)
{
    const dst *s = st;
    const dpos *p = &s->p;
    int me = p->side, v = 100 * (p->boxes[me] - p->boxes[!me]), known = 1, dd, end;
    if (a_box_to_take(p) >= 0) end = take_value(s->m, p, &dd, 1);
    else end = quiet_value(s->m, p, &known);
    if (known) return v + 100 * end;
    int odd = long_chains(p) & 1;               /* the long-chain rule: player 0 wants them even */
    return v + ((odd == (me == 1)) ? 40 : -40);
}

static uint64_t s_hash(const void *st)
{
    const dst *s = st;
    uint64_t a = 0, b = 0;
    for (int l = 0; l < dots_lines(s->p.n); l++) {
        if (s->p.line[l] < 0) continue;
        if (l < 64) a |= 1ull << l;
        else b |= 1ull << (l - 64);
    }
    return ai_mix(a, b ^ (uint64_t)s->p.side << 63 ^ (uint64_t)s->p.boxes[0] << 50);
}

static const ai_game SAFE = {
    .size = sizeof(dst), .moves = s_moves, .make = s_make, .side = s_side, .over = s_over, .eval = s_eval,
    .hash = s_hash, .unit = 100,
};

static const ai_game EXACT = {
    .size = sizeof(dst), .moves = x_moves, .make = s_make, .side = s_side, .over = s_over, .eval = x_eval,
    .hash = s_hash, .order = x_order, .unit = 100,
};

/* ---- the choice -------------------------------------------------------------------------------------------------- */
/* no box to take, no safe line: the line whose opening gives the other the least: by the value with best play, then
 * (equal values: a player who might not double-deal) by the boxes taken at once */
static int opening_line(memo *m, const dpos *p, int allow_dd, int greedy_only)
{
    int best = -1, bv = 1 << 30, bk = 1 << 30;
    for (int l = 0; l < dots_lines(p->n); l++) {
        if (p->line[l] >= 0) continue;
        dpos q = *p;
        dots_make(&q, l);
        taking t;
        take_all(&q, &t);
        int v = greedy_only ? 0 : any_value(m, &q, allow_dd);   /* the other's value after it */
        if (v < bv || (v == bv && t.k < bk)) { bv = v; bk = t.k; best = l; }
    }
    return best;
}

int dtb_choose(const dpos *p, int level, uint64_t seed)
{
    int nl = dots_lines(p->n), safe_l[DOTS_MAX_LINES];
    if (p->drawn >= nl) return -1;
    uint64_t key = 0;
    for (int l = 0; l < nl; l++) key = key * 3 + (uint64_t)(p->line[l] + 1);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, key ^ (uint64_t)p->drawn << 56));
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    memo m = {calloc((size_t)1 << MEMO_BITS, sizeof(memo_e))};
    int ns = safe_lines(p, safe_l), line = -1, take = a_box_to_take(p) >= 0;
    if (level == AI_EASY) {
        if (take && !(ns && ai_rand_n(&rng, 7) == 0)) line = taking_line(p, 0);
        else if (ns) line = ai_rand_n(&rng, 10) ? safe_l[ai_rand_n(&rng, ns)] : -1;
        else line = opening_line(&m, p, 0, 1);
        if (line < 0) {                         /* a slip: any line */
            int k = ai_rand_n(&rng, nl - p->drawn);
            for (line = 0; line < nl; line++)
                if (p->line[line] < 0 && k-- == 0) break;
        }
    } else if (nl - p->drawn <= (level == AI_HARD ? 16 : 8)) {   /* few lines left: every line searched */
        dst s = {*p, &m};
        ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
        o.budget.ms = o.budget.ms * 85 / 100;       /* a margin under the level's time (a slow or busy machine) */
        o.budget.noise = 0;
        o.budget.depth = 0;
        o.budget.nodes = level == AI_HARD ? 200000 : 40000;
        line = ai_ab(&EXACT, &s, &o, NULL);
    } else if (take) {
        int dd = 0;
        take_value(&m, p, &dd, 1);
        if (level == AI_MEDIUM && !ai_rand_n(&rng, 4)) dd = 0;   /* medium keeps control three times in four */
        line = taking_line(p, dd);
    } else if (!ns) {
        line = opening_line(&m, p, 1, 0);
    } else if (level == AI_MEDIUM || ns > 14) {   /* many safe lines (medium: always): any of them */
        line = safe_l[ai_rand_n(&rng, ns)];
    } else {
        dst s = {*p, &m};
        ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 16};
        o.budget.ms = o.budget.ms * 85 / 100;       /* a margin under the level's time (a slow or busy machine) */
        o.budget.nodes = 150000;                /* hard: about 0.3 s on a PC at most */
        line = ai_ab(&SAFE, &s, &o, NULL);
        if (line < 0) line = safe_l[ai_rand_n(&rng, ns)];
    }
    free(m.e);
    return line;
}

int dtb_endgame(const dpos *p)
{
    memo m = {calloc((size_t)1 << MEMO_BITS, sizeof(memo_e))};
    int v = any_value(&m, p, 1);
    free(m.e);
    return v;
}

static int think(int seat, int level, char *move, int cap)
{
    dpos p = *dots_pos(dots_live());            /* the copy, at entry (the position only): nothing else is read */
    if (p.side != seat || p.drawn >= dots_lines(p.n) || cap < 8) return 0;
    int l = dtb_choose(&p, level, ai_session_seed());
    if (l < 0 || ai_cancelled()) return 0;
    int n = p.n, across = l < n * (n + 1), k = across ? l : l - n * (n + 1), w = across ? n : n + 1;
    snprintf(move, (size_t)cap, "%c%d,%d", across ? 'h' : 'v', k % w + 1, k / w + 1);
    return 1;
}

static const bot dots_bot = {.think = think, .play = dots_show};
REGISTER_BOT(dots, &dots_bot)
