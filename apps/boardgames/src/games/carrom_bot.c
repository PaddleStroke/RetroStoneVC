/*
 * Carrom's bot (bots.h): it tries shots and keeps the best. The shots: each coin of its colour (and the queen) towards
 * each hole, the striker set where the shot is straight (or near it, a cut), three powers; the opening break; some at
 * random. Each is run by the rules' own physics (cuetable.h) and weighed: the game won or lost, the coins pocketed on
 * each side, the queen, the turn kept and how easy the next shot is for whoever plays it. Harder levels try more shots
 * (hard: its best few again with a little noise) and aim truer: the chosen shot is played with the level's noise.
 * Runs on the worker thread: a copy of the position, nothing else (no SDL).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/carrom.h"

typedef struct { int pos, ang, pow; float prior; } cshot;   /* prior: how promising it looks */

static void hole(int k, float *x, float *y)
{
    *x = k & 1 ? CRM_SIZE - CRM_HOLE_AT : CRM_HOLE_AT;
    *y = k & 2 ? CRM_SIZE - CRM_HOLE_AT : CRM_HOLE_AT;
}

static int wanted(const crm_pos *p, int team, int i)
{
    if (p->w.b[i].down >= 0) return 0;
    return crm_colour(i) == team || (i == CRM_QUEEN && p->queen == CRM_Q_BOARD);
}

/* each coin worth playing, to each hole: the straight shot's place on the shooter's baseline (local units), the ghost */
typedef void (*each_fn)(const crm_pos *p, int i, int k, float gx, float gy, float ux, float uy, void *ctx);
static void each_shot(const crm_pos *p, int seat, each_fn fn, void *ctx)
{
    int team = crm_team(p, seat);
    for (int i = 1; i < CRM_N; i++) {
        if (!wanted(p, team, i)) continue;
        const cue_ball *b = &p->w.b[i];
        for (int k = 0; k < 4; k++) {
            float hx, hy;
            hole(k, &hx, &hy);
            float ux = hx - b->x, uy = hy - b->y, d = hypotf(ux, uy);
            if (d < 1e-3f) continue;
            ux /= d;
            uy /= d;
            float R = CRM_COIN_R + CRM_STRIKER_R;
            fn(p, i, k, b->x - ux * R, b->y - uy * R, ux, uy, ctx);
        }
    }
}

/* how easy the next shot is for the seat to move (0..1) */
typedef struct { const crm_pos *p; float best; } qctx;
static void q_one(const crm_pos *p, int i, int k, float gx, float gy, float ux, float uy, void *ctx)
{
    qctx *q = ctx;
    float hx, hy;
    hole(k, &hx, &hy);
    const cue_world *w = &p->w;
    if (!cue_clear(w, w->b[i].x, w->b[i].y, hx, hy, CRM_COIN_R * 0.9f, 0, i)) return;
    float best = 0;
    for (int pos = -1000; pos <= 1000; pos += 250) {
        float sx, sy;
        crm_spot(p, p->turn, pos, &sx, &sy);
        float vx = gx - sx, vy = gy - sy, d1 = hypotf(vx, vy);
        if (d1 < 1e-3f) continue;
        float cs = (vx * ux + vy * uy) / d1;
        if (cs < 0.3f || !crm_spot_free(p, p->turn, pos) || !cue_clear(w, sx, sy, gx, gy, CRM_STRIKER_R * 0.9f, 0, i))
            continue;
        float v = cs * cs * (1 - (d1 + hypotf(hx - w->b[i].x, hy - w->b[i].y)) / 260);
        if (v > best) best = v;
    }
    if (best > q->best) q->best = best;
}

static float quality(const crm_pos *p)
{
    if (p->status != CRM_PLAY) return 0;
    qctx q = {p, 0};
    each_shot(p, p->turn, q_one, &q);
    return q.best;
}

static float weigh(const crm_pos *q, int team)
{
    if (q->status != CRM_PLAY) return (q->status == CRM_WIN0) == (team == 0) ? 1e6f : -1e6f;
    float v = 100.0f * (crm_pocketed(q, team) - crm_pocketed(q, !team));
    if (q->queen == CRM_Q_COVERED) v += q->queen_team == team ? 220 : -220;
    else if (q->queen == CRM_Q_PENDING) v += q->queen_team == team ? 70 : -70;
    v -= 60.0f * (q->debt[team] - q->debt[!team]);
    if (crm_team(q, q->turn) == team) v += 120 + 220 * quality(q);
    else v -= 220 * quality(q);
    return v;
}

#define CAP 600
typedef struct { cshot *s; int n; int seat; } cctx;

static void add(cctx *c, int pos, float dx, float dy, float power, float prior)
{
    if (c->n >= CAP) return;
    c->s[c->n].prior = prior;
    c->s[c->n].pos = pos;
    c->s[c->n].ang = cue_angle(dx, dy);
    c->s[c->n].pow = (int)lroundf(fminf(1, fmaxf(0.04f, power)) * 1000);
    c->n++;
}

static void c_one(const crm_pos *p, int i, int k, float gx, float gy, float ux, float uy, void *ctx)
{
    cctx *c = ctx;
    float hx, hy;
    hole(k, &hx, &hy);
    float d2 = hypotf(hx - p->w.b[i].x, hy - p->w.b[i].y);
    /* the place where the shot is straight: the line from the hole through the coin, on the baseline */
    float lgx, lgy, lux, luy, side = crm_side(p, c->seat);
    int back = (4 - (int)side) & 3;
    crm_turn_pt(back, gx, gy, &lgx, &lgy);
    float ex, ey;
    crm_turn_pt(back, CRM_SIZE / 2 + ux, CRM_SIZE / 2 + uy, &ex, &ey);
    lux = ex - CRM_SIZE / 2;
    luy = ey - CRM_SIZE / 2;
    int poss[6], np = 0;
    if (fabsf(luy) > 0.05f) {
        float t = (lgy - CRM_BASE) / luy;
        if (t > 0) {
            float x = lgx - t * lux;
            poss[np++] = (int)lroundf(fminf(1, fmaxf(-1, (x - CRM_SIZE / 2) / CRM_BASE_HALF)) * 1000);
        }
    }
    static const int AT[5] = {-900, -450, 0, 450, 900};
    for (int a = 0; a < 5; a++) poss[np++] = AT[a];
    for (int a = 0; a < np; a++) {
        int pos = crm_free_pos(p, c->seat, poss[a]);
        float sx, sy;
        crm_spot(p, c->seat, pos, &sx, &sy);
        float vx = gx - sx, vy = gy - sy, d1 = hypotf(vx, vy);
        if (d1 < 1e-3f || (vx * ux + vy * uy) / d1 < 0.2f) continue;
        if (!cue_clear(&p->w, sx, sy, gx, gy, CRM_STRIKER_R * 0.85f, 0, i)) continue;
        float pw = 0.18f + (d1 * 0.55f + d2 * 1.6f) / 120;
        float cs = (vx * ux + vy * uy) / d1;
        for (int q = 0; q < 3; q++) add(c, pos, vx / d1, vy / d1, pw * (0.8f + 0.3f * q), 1 + cs * cs * (1 - (d1 + d2) / 260));
    }
}

static float gauss(ai_rng *r)
{
    float u = (float)ai_rand_f(r) + 1e-6f, v = (float)ai_rand_f(r);
    return sqrtf(-2 * logf(u)) * cosf(2 * CUE_PI * v);
}

static float run(const crm_pos *p, const cshot *s, int team)
{
    crm_pos q = *p;
    cue_events e;
    if (!crm_shot(&q, s->pos, s->ang, s->pow, &e)) return -1e7f;
    return weigh(&q, team) - (e.first < 0 ? 150 : 0);   /* a shot that touches nothing: never a way to wait */
}

int crm_choose(const crm_pos *p, int level, uint64_t seed, char *move, int cap)
{
    if (p->status != CRM_PLAY) return 0;
    level = level < AI_EASY ? AI_EASY : level > AI_HARD ? AI_HARD : level;
    ai_rng r;
    uint64_t h = seed;
    for (int i = 0; i < p->w.n; i++) h = ai_mix(h, (uint64_t)(p->w.b[i].x * 1000) * 131 + (uint64_t)(p->w.b[i].y * 1000));
    ai_seed(&r, h);
    static const int TRIES[AI_LEVELS] = {14, 60, 500};
    static const float NOISE_DEG[AI_LEVELS] = {2.4f, 0.8f, 0.18f}, NOISE_POW[AI_LEVELS] = {0.14f, 0.06f, 0.02f};
    double t0 = ai_now_ms(), limit = ai_level_budget(level).ms * 0.55;
    cshot s[CAP];
    cctx c = {s, 0, p->turn};
    int side = crm_side(p, p->turn);
    each_shot(p, p->turn, c_one, &c);
    for (int k = 0; k < 6; k++) {               /* at the middle (the break), and a few at random */
        int pos = crm_free_pos(p, p->turn, (k - 2) * 300);
        float sx, sy, ax, ay;
        crm_spot(p, p->turn, pos, &sx, &sy);
        crm_turn_pt(side, CRM_SIZE / 2 + (k - 2.5f) * 1.5f, CRM_SIZE / 2, &ax, &ay);
        float l = hypotf(ax - sx, ay - sy);
        add(&c, pos, (ax - sx) / l, (ay - sy) / l, 0.85f + 0.03f * k, 0.8f);
    }
    for (int k = 0; k < 40; k++) {
        float a = 2 * CUE_PI * (float)ai_rand_f(&r);
        add(&c, crm_free_pos(p, p->turn, ai_rand_n(&r, 2001) - 1000), cosf(a), sinf(a), 0.2f + 0.8f * (float)ai_rand_f(&r), 0);
    }
    int n = c.n;
    for (int i = n - 1; i > 0; i--) {           /* a random order (easy sees a few of them) */
        int j = ai_rand_n(&r, i + 1);
        cshot t = s[i];
        s[i] = s[j];
        s[j] = t;
    }
    if (level > AI_EASY)                        /* the others try the most promising first */
        for (int i = 1; i < n; i++) {
            cshot t = s[i];
            int j = i;
            while (j > 0 && s[j - 1].prior < t.prior) { s[j] = s[j - 1]; j--; }
            s[j] = t;
        }
    if (n > TRIES[level]) n = TRIES[level];
    if (n <= 0) return 0;
    int team = crm_team(p, p->turn);
    enum { KEEP = 6 };
    int top[KEEP] = {0};
    float topv[KEEP];
    for (int k = 0; k < KEEP; k++) topv[k] = -1e9f;
    for (int i = 0; i < n; i++) {
        if (ai_cancelled()) return 0;
        if (i > 4 && ai_now_ms() - t0 > limit) break;
        float v = run(p, &s[i], team);
        for (int k = 0; k < KEEP; k++)
            if (v > topv[k]) {
                for (int j = KEEP - 1; j > k; j--) { topv[j] = topv[j - 1]; top[j] = top[j - 1]; }
                topv[k] = v;
                top[k] = i;
                break;
            }
    }
    int pick = top[0];
    if (level > AI_EASY) {                      /* the best few again with its own hand's noise: the shot that holds */
        float best = -1e9f;
        int tries = level == AI_HARD ? 5 : 3;
        for (int k = 0; k < KEEP && topv[k] > -1e8f; k++) {
            float sum = topv[k];
            int m = 1;
            for (int t = 0; t < tries; t++) {
                if (ai_cancelled()) return 0;
                if (ai_now_ms() - t0 > limit * 1.6) break;
                cshot q = s[top[k]];
                q.ang = ((q.ang + (int)lroundf(gauss(&r) * NOISE_DEG[level] * 100)) % 36000 + 36000) % 36000;
                q.pow = (int)lroundf(q.pow * (1 + gauss(&r) * NOISE_POW[level]));
                q.pow = q.pow < 1 ? 1 : q.pow > 1000 ? 1000 : q.pow;
                sum += run(p, &q, team);
                m++;
            }
            if (sum / m > best) { best = sum / m; pick = top[k]; }
        }
    }
    cshot x = s[pick];
    x.ang = ((x.ang + (int)lroundf(gauss(&r) * NOISE_DEG[level] * 100)) % 36000 + 36000) % 36000;
    x.pow = (int)lroundf(x.pow * (1 + gauss(&r) * NOISE_POW[level]));
    x.pow = x.pow < 1 ? 1 : x.pow > 1000 ? 1000 : x.pow;
    snprintf(move, (size_t)cap, "%d:%d:%d", x.pos, x.ang, x.pow);
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    crm_pos p = *crm_now(carrom_live());        /* the copy, at entry: nothing else is read */
    if (p.status != CRM_PLAY || p.turn != seat) return 0;
    if (!crm_choose(&p, level, ai_session_seed(), move, cap) || ai_cancelled()) return 0;
    return 1;
}

static const bot carrom_bot = {.think = think, .play = carrom_show};
REGISTER_BOT(carrom, &carrom_bot)
