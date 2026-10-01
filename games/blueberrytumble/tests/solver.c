/*
 * Blueberry Tumble: the input-search solver (tests only).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 *
 * The input model is a player's: on foot (normal, snowberry) the only action is a TAP (A held for TAP_FRAMES
 * frames, then released); gliding, A is held or released freely (holds are the point there).
 * sv_alive(): a depth-first search over these choices on every frame, the default first (no tap / keep the glide
 * input), on the game's own physics. A memo remembers, per (frame, state), the furthest frame proven reachable and
 * the nearest goal proven unreachable, so each state is expanded once per goal.
 * sv_careful(): the careful player. Take the default while the course can still be completed; when it cannot,
 * the frames on which acting still works form runs (the windows): take the latest comfortable one (>= COMFORT
 * frames; else the widest) and act in its middle. The windows, presses, skills and reaction times of that path are
 * what the validator reports and what the difficulty score is made of.
 */
#include "solver.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMO_BITS 22
#define MEMO_N (1u << MEMO_BITS)
#define TAP_FRAMES 6            /* a tap: A held this long (100 ms) */
#define COMFORT 6               /* a window this wide is comfortable: the careful player takes the latest such */
#define SCAN_BACK 60            /* how far back (frames) the careful player looks for another way */

typedef struct sst { berry b; int tap; } sst;   /* tap: frames of the current tap still to hold */

typedef struct key {
    int32_t f, h, vy, orb, pad;
    uint16_t bits, tap;
} key;
typedef struct slot {
    key k;
    int32_t reach, dead_goal;
    uint32_t gen;
} slot;

static slot *memo;
static uint32_t gen = 1, used;

void sv_init(void)
{
    if (!memo) memo = calloc(MEMO_N, sizeof *memo);
    if (!memo) { fprintf(stderr, "solver: out of memory\n"); exit(2); }
}

void sv_reset(void)
{
    gen++;
    used = 0;
}

static void make_key(key *k, const sst *s, int32_t f)
{
    const berry *b = &s->b;
    memset(k, 0, sizeof *k);
    k->f = f;
    k->h = b->h;
    k->vy = b->vy;
    k->orb = b->orb_used;
    k->pad = b->pad_used;
    k->bits = (uint16_t)(b->mode | b->grounded << 2 | b->held << 3 | b->need_release << 4 | b->on_ice << 5 |
                         b->coin_got << 6);
    k->tap = (uint16_t)s->tap;
}

static uint32_t hash_key(const key *k)
{
    uint32_t h = 2166136261u;
    const uint8_t *p = (const uint8_t *)k;
    for (size_t i = 0; i < sizeof *k; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static slot *lookup(const key *k, int create)
{
    uint32_t i = hash_key(k) & (MEMO_N - 1);
    for (;;) {
        slot *s = &memo[i];
        if (s->gen != gen) {
            if (!create) return NULL;
            if (used > MEMO_N / 4 * 3) { sv_reset(); return lookup(k, create); }
            s->gen = gen;
            s->k = *k;
            s->reach = -1;
            s->dead_goal = INT32_MAX;
            used++;
            return s;
        }
        if (!memcmp(&s->k, k, sizeof *k)) return s;
        i = (i + 1) & (MEMO_N - 1);
    }
}

static uint64_t nodes;

/* ---- the input model ------------------------------------------------------------------------------------------------- */
static int n_choices(const sst *s) { return s->b.mode == M_GLIDE ? 2 : s->tap > 0 ? 1 : 2; }
static int acts(const sst *s) { return s->b.mode != M_GLIDE && s->tap == 0; }     /* choice 1 = a new tap */

/* choice 0: the default (no tap, or the tap goes on; gliding: keep the input); 1: tap, or change the glide input */
static void apply(const sv_query *q, sst *s, int32_t f, int choice)
{
    int held;
    if (s->b.mode == M_GLIDE) {
        held = choice ? !s->b.held : s->b.held;
        s->tap = 0;
    } else if (s->tap > 0) {
        held = 1;
        s->tap--;
    } else if (choice) {
        held = 1;
        s->tap = TAP_FRAMES - 1;
    } else {
        held = 0;
    }
    int64_t x = course_x(q->c, f + 1);
    berry_step(&s->b, q->c, x, x, held, NULL);
    nodes++;
}

static void fill(const sv_query *q, int32_t f)
{
    if (!q->fill_to) return;
    int32_t col = (int32_t)((course_x(q->c, f + q->horizon + 60) >> 16) / CELL) + 8;
    if (col > q->fill_to) col = q->fill_to;
    director_fill(q->c, col);
}

typedef struct frame_t { sst s; int32_t f; int choice; } frame_t;
#define STACK_N 8192
static frame_t stack[STACK_N];

static void mark_reach(int sp, int32_t goal)
{
    key k;
    for (int i = 0; i < sp; i++) {
        make_key(&k, &stack[i].s, stack[i].f);
        slot *z = lookup(&k, 1);
        if (z->reach < goal) z->reach = goal;
    }
}

static int alive(const sv_query *q, const sst *s0, int32_t f0, int32_t goal)
{
    if (s0->b.dead) return 0;
    if (goal > q->end_f) goal = q->end_f;
    int final = goal >= q->end_f;
    int sp = 0;
    stack[0].s = *s0;
    stack[0].f = f0;
    stack[0].choice = 0;
    sp = 1;
    while (sp > 0) {
        frame_t *t = &stack[sp - 1];
        key k;
        make_key(&k, &t->s, t->f);
        int coin_ok = !final || !q->need_coin || t->s.b.coin_got;
        if (t->choice == 0) {
            if (t->f >= goal && coin_ok) { mark_reach(sp, goal); return 1; }
            slot *s = lookup(&k, 0);
            if (s && s->reach >= goal && coin_ok) { mark_reach(sp - 1, goal); return 1; }
            if ((s && s->dead_goal <= goal) || t->f >= goal) {
                if (!s) s = lookup(&k, 1);
                if (s->dead_goal > goal) s->dead_goal = goal;
                sp--;
                continue;
            }
        }
        if (t->choice >= n_choices(&t->s)) {
            slot *s = lookup(&k, 1);
            if (s->dead_goal > goal) s->dead_goal = goal;
            sp--;
            continue;
        }
        int ch = t->choice++;
        if (sp >= STACK_N) continue;
        frame_t *n = &stack[sp];
        n->s = t->s;
        n->f = t->f + 1;
        n->choice = 0;
        apply(q, &n->s, t->f, ch);
        if (n->s.b.dead) continue;
        sp++;
    }
    return 0;
}

int sv_alive(const sv_query *q, const berry *b, int32_t f, int32_t goal)
{
    sst s = {*b, 0};
    return alive(q, &s, f, goal);
}

/* ---- the careful player ------------------------------------------------------------------------------------------- */
static int32_t goal_of(const sv_query *q, int32_t f)
{
    if (!q->horizon) return q->end_f;
    int32_t g = f + q->horizon;
    return g > q->end_f ? q->end_f : g;
}

static int next_object(const bt_course *c, int64_t x, int32_t *obj_x, int *is_cone, int *cone_i)
{
    int32_t col0 = (int32_t)((x >> 16) / CELL) + 1;
    int32_t best = INT32_MAX;
    *is_cone = 0;
    for (int32_t col = col0; col < col0 + 24; col++) {
        const bt_col *k = course_col(c, col);
        int any = k->ground != G_GROUND || (k->flags & (F_LEAF | F_LEAF_END));
        for (int r = 0; r < ROWS && !any; r++) any = k->cell[r] != K_EMPTY && k->cell[r] != K_COIN;
        if (any) { best = col * CELL; break; }
    }
    for (int32_t i = c->ncone - 1; i >= 0 && i >= c->ncone - CONE_RING; i--) {
        int32_t kx = (int32_t)(cone_x(&c->cone[i % CONE_RING], x) >> 16);
        if (kx > (int32_t)(x >> 16) && kx < best) { best = kx; *is_cone = 1; *cone_i = i; }
    }
    *obj_x = best;
    return best != INT32_MAX;
}

/* frames between the object the press is for coming into view and the press */
static int reaction(const sv_query *q, int32_t m, int64_t x)
{
    int32_t ox;
    int cone, ci = 0;
    if (!next_object(q->c, x, &ox, &cone, &ci)) return 999;
    int32_t v = m;
    while (v > m - 400) {
        int64_t xv = course_x(q->c, v - 1);
        int64_t dist = cone ? (cone_x(&q->c->cone[ci % CONE_RING], xv) >> 16) - (xv >> 16) : ox - (xv >> 16);
        if (dist > VIEW_AHEAD) break;
        v--;
    }
    return m - v;
}

static sst path[1 << 16];
static int32_t path_base;
static int budget;              /* backtracking steps left for this query */
#define BUDGET 400
#define ALTERNATIVES 4          /* other frames tried in a window, around its middle, when a later window is too tight */

/* the careful walk from state s at frame f: take the default while it can still finish; at each decision the
 * preferred run of workable frames is the window; act in its middle (strict: every window >= MIN_WINDOW, and when a
 * later one is too tight, try other frames of this window). Returns 1 when the end is reached. The decisions are
 * pushed to r->tog. */
static int walk(const sv_query *q, sv_result *r, sst s, int32_t f, int32_t seg, int32_t force_at, int strict)
{
    while (f < q->end_f) {
        if (f - path_base >= (int32_t)(sizeof path / sizeof path[0])) return 0;
        fill(q, f);
        path[f - path_base] = s;
        int32_t goal = goal_of(q, f + 1);
        sst d = s;
        apply(q, &d, f, 0);
        int def_ok = !d.b.dead && alive(q, &d, f + 1, goal);
        int forced = f == force_at && acts(&s);
        if (def_ok && !forced) {
            s = d;
            f++;
            continue;
        }
        if (n_choices(&s) < 2 && !forced) { r->fail_frame = f; return 0; }   /* inside a tap: stuck */
        /* the windows: runs of frames g on which acting works */
        int32_t hi = -1, lo = -1, run_hi = -1, best_w = 0;
        int32_t g_min = f - SCAN_BACK > seg ? f - SCAN_BACK : seg;
        for (int32_t g = f; g >= g_min - 1; g--) {
            int ok = 0;
            if (g >= g_min && n_choices(&path[g - path_base]) == 2 && (!forced || g == f)) {
                sst t = path[g - path_base];
                apply(q, &t, g, 1);
                ok = !t.b.dead && alive(q, &t, g + 1, goal);
            }
            if (ok) {
                if (run_hi < 0) run_hi = g;
            } else if (run_hi >= 0) {
                int32_t w = run_hi - g;                 /* the run [g + 1, run_hi] */
                if (hi < 0 || (best_w < COMFORT && w > best_w)) { hi = run_hi; lo = g + 1; best_w = w; }
                run_hi = -1;
                if (best_w >= COMFORT) break;
            }
        }
        if (hi < 0) { r->fail_frame = f; return 0; }
        int w = forced ? 999 : (int)(hi - lo + 1);
        if (strict && w < MIN_WINDOW) { r->fail_frame = f; return 0; }
        int32_t mid = forced ? f : (lo + hi) / 2;
        /* strict: the middle first, then the other frames of the window outwards (a player may act anywhere in it) */
        int half = (int)(hi - lo) / 2 + 1;
        int tries = strict && !forced ? 1 + 2 * (half > ALTERNATIVES ? half : ALTERNATIVES) : 1;
        for (int k = 0; k < tries; k++) {
            int32_t m = mid + (k & 1 ? -(k + 1) / 2 : k / 2);
            if (m < lo || m > hi) continue;
            if (k > 0 && --budget < 0) return 0;
            int nt = r->ntog;
            if (r->ntog < SV_MAX_TOGGLES) {
                sv_toggle *tg = &r->tog[r->ntog++];
                const sst *pm = &path[m - path_base];
                tg->frame = m;
                tg->press = pm->b.mode != M_GLIDE || !pm->b.held;
                tg->win_lo = (int)lo;
                tg->win_hi = forced ? (int)lo + 998 : (int)hi;   /* a forced action does not count as a window */
            }
            sst t = path[m - path_base];
            apply(q, &t, m, 1);
            if (walk(q, r, t, m + 1, m + 1, force_at, strict)) return 1;
            r->ntog = nt;
            if (!strict) return 0;
        }
        return 0;
    }
    r->frames = f - q->start_f;
    return !s.b.dead && (!q->need_coin || s.b.coin_got);
}

/* replay the decisions: the skills, presses, reaction times, windows and the tail */
static void replay(const sv_query *q, sv_result *r)
{
    sst s = {q->start, 0};
    int evs = 0, k = 0;
    r->presses = 0;
    r->min_window = 999;
    r->react = 999;
    r->land_after = -1;
    for (int32_t f = q->start_f; f < q->end_f; f++) {
        int act = k < r->ntog && r->tog[k].frame == f;
        if (act) {
            const sv_toggle *tg = &r->tog[k++];
            int w = tg->win_hi - tg->win_lo + 1;
            if (w < r->min_window) { r->min_window = w; r->min_window_frame = f; }
            if (tg->press) {
                r->presses++;
                int re = reaction(q, f, s.b.x);
                if (re < r->react) r->react = re;
            }
        }
        sst prev = s;
        apply(q, &s, f, act);
        berry e = prev.b;
        int64_t x = course_x(q->c, f + 1);
        evs |= berry_step(&e, q->c, x, x, s.b.held, NULL);
        if (s.b.on_ice) evs |= EV_ICE;
        for (int32_t i = q->c->ncone - 1; i >= 0 && i >= q->c->ncone - CONE_RING; i--) {
            int64_t kx = cone_x(&q->c->cone[i % CONE_RING], x);
            if (kx > x - 16 * Q16_ONE && kx < x + 16 * Q16_ONE) r->skills |= SK_CONE;
        }
        if (q->boundary_x >= 0 && r->land_after < 0 && (s.b.x >> 16) >= q->boundary_x && s.b.grounded &&
            s.b.h == 0 && s.b.mode == M_NORMAL) {
            int32_t fb = f + 1;
            while (fb > q->start_f && (course_x(q->c, fb - 1) >> 16) >= q->boundary_x) fb--;
            r->land_after = f + 1 - fb;
        }
        if (s.b.dead) break;
    }
    if (evs & EV_JUMP) r->skills |= SK_JUMP;
    if (evs & EV_ORB) r->skills |= SK_ORB;
    if (evs & EV_PAD) r->skills |= SK_PAD;
    if (evs & EV_GLIDE) r->skills |= SK_GLIDE;
    if (evs & EV_GROW) r->skills |= SK_SNOW;
    if (evs & EV_ICE) r->skills |= SK_ICE;
}

static void careful(const sv_query *q, sv_result *r, int32_t force_at)
{
    memset(r, 0, sizeof *r);
    sst s = {q->start, 0};
    path_base = q->start_f;
    fill(q, q->start_f);
    if (!alive(q, &s, q->start_f, goal_of(q, q->start_f))) {
        r->fail_frame = q->start_f;
        r->min_window = 0;
        return;
    }
    /* a player who hits every window (>= MIN_WINDOW frames); if there is none, the plain careful path tells how
     * tight it gets */
    budget = BUDGET;
    r->ok = walk(q, r, s, q->start_f, q->start_f, force_at, 1);
    if (!r->ok) {
        int32_t ff = r->fail_frame;
        memset(r, 0, sizeof *r);
        budget = BUDGET;
        r->ok = walk(q, r, s, q->start_f, q->start_f, force_at, 0);
        if (!r->ok) r->fail_frame = ff;
    }
    replay(q, r);
}

void sv_careful(const sv_query *q, sv_result *r)
{
    nodes = 0;
    if (!q->late_last_press) {
        careful(q, r, -1);
    } else {
        /* the last press at the late edge of its window (the tail's worst case) */
        static sv_result first;
        careful(q, &first, -1);
        int32_t late = -1;
        for (int i = first.ntog - 1; i >= 0; i--)
            if (first.tog[i].press) { late = first.tog[i].win_hi; break; }
        if (late < 0 || !first.ok) *r = first;
        else careful(q, r, late);
    }
    r->nodes = nodes;
}
