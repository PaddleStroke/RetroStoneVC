/*
 * Pool's bot (bots.h): it tries shots and keeps the best. The shots: each ball it may strike towards each pocket
 * (the ghost ball's aim, three powers; with the cue ball in hand, the cue ball set behind it), the break, and some
 * at random; each is run by the rules' own physics (cuetable.h) and the result weighed: the game won or lost, a foul,
 * the balls left of each group, the turn kept, and how easy the next shot is for whoever plays it. Harder levels try
 * more shots (hard: its best few again with a little noise, the safest kept) and aim truer: the chosen shot is played
 * with the level's noise, the human-like miss. Runs on the worker thread: a copy of the position, nothing else (no
 * SDL). MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/pool.h"

typedef struct { int ang, pow, placed; float x, y, prior; } pshot;   /* prior: how promising it looks */

static const float AIM[6][2] = {{-0.4f, -0.4f}, {50, -1.0f}, {100.4f, -0.4f}, {-0.4f, 50.4f}, {50, 51.0f},
                                {100.4f, 50.4f}};

/* how easy the next shot is for the player to move, 0..1: the best ball to a pocket on clear lines, nearly straight */
static float quality(const pool_pos *p)
{
    if (p->status != POOL_PLAY) return 0;
    const cue_world *w = &p->w;
    uint32_t m = pool_targets(p);
    const cue_ball *c = &w->b[0];
    float best = 0;
    for (int i = 1; i < w->n; i++) {
        if (!(m >> i & 1)) continue;
        const cue_ball *b = &w->b[i];
        for (int k = 0; k < 6; k++) {
            float ux = AIM[k][0] - b->x, uy = AIM[k][1] - b->y, d2 = hypotf(ux, uy);
            ux /= d2;
            uy /= d2;
            if ((k == 1 || k == 4) && fabsf(uy) < 0.55f) continue;   /* a side pocket from the side only */
            if (!cue_clear(w, b->x, b->y, AIM[k][0], AIM[k][1], POOL_R * 0.95f, 0, i)) continue;
            float gx = b->x - ux * 2 * POOL_R, gy = b->y - uy * 2 * POOL_R, q;
            if (p->in_hand) q = 0.95f - d2 / 260;
            else {
                float vx = gx - c->x, vy = gy - c->y, d1 = hypotf(vx, vy), cs = d1 > 0 ? (vx * ux + vy * uy) / d1 : 1;
                if (cs < 0.3f || !cue_clear(w, c->x, c->y, gx, gy, POOL_R * 0.95f, 0, i)) continue;
                q = cs * cs * (1 - (d1 + d2) / 240);
            }
            if (q > best) best = q;
        }
    }
    return best;
}

/* the result for `me` (higher better) */
static float weigh(const pool_pos *q, int me)
{
    if (q->status != POOL_PLAY) return (q->status == POOL_WIN0) == (me == 0) ? 1e6f : -1e6f;
    float v = 0;
    if (q->variant == POOL_EIGHT && q->group[me] != POOL_OPEN)
        v += 110.0f * (pool_left(q, q->group[!me]) - pool_left(q, q->group[me]));
    if (q->turn == me) v += 420 + 300 * quality(q);
    else v -= 300 * quality(q) + (q->in_hand ? 140 : 0);
    return v;
}

static float gauss(ai_rng *r)
{
    float u = (float)ai_rand_f(r) + 1e-6f, v = (float)ai_rand_f(r);
    return sqrtf(-2 * logf(u)) * cosf(2 * CUE_PI * v);
}

static int add(pshot *s, int n, int cap, float dx, float dy, float power, int placed, float x, float y, float prior)
{
    if (n >= cap) return n;
    s[n].prior = prior;
    s[n].ang = cue_angle(dx, dy);
    s[n].pow = (int)lroundf(fminf(1, fmaxf(0.04f, power)) * 1000);
    s[n].placed = placed;
    s[n].x = roundf(x * 100) / 100;
    s[n].y = roundf(y * 100) / 100;
    return n + 1;
}

#define CAP 400

/* the shots worth trying */
static int candidates(const pool_pos *p, pshot *s, ai_rng *r)
{
    int n = 0;
    const cue_world *w = &p->w;
    const cue_ball *c = &w->b[0];
    if (p->brk) {                               /* the break: from behind the head string at the rack's apex */
        for (int k = 0; k < 12; k++) {
            float y = POOL_H / 2 + (k % 6 - 2.5f) * 3.2f, x = 12 + (k / 6) * 8;
            float ax = POOL_FOOT_X - x, ay = POOL_H / 2 + (ai_rand_f(r) - 0.5f) * 0.8f - y;
            float l = hypotf(ax, ay);
            n = add(s, n, CAP, ax / l, ay / l, 0.9f + 0.1f * (float)ai_rand_f(r), 1, x, y, 1);
        }
        return n;
    }
    uint32_t m = pool_targets(p);
    for (int i = 1; i < w->n; i++) {
        if (!(m >> i & 1)) continue;
        const cue_ball *b = &w->b[i];
        for (int k = 0; k < 6; k++) {
            float ux = AIM[k][0] - b->x, uy = AIM[k][1] - b->y, d2 = hypotf(ux, uy);
            ux /= d2;
            uy /= d2;
            if ((k == 1 || k == 4) && fabsf(uy) < 0.4f) continue;
            float gx = b->x - ux * 2 * POOL_R, gy = b->y - uy * 2 * POOL_R;
            if (p->in_hand) {                   /* the cue ball set straight behind it */
                for (int t = 0; t < 2; t++) {
                    float dd = t ? 16 : 6, x = gx - ux * dd, y = gy - uy * dd;
                    if (!pool_place_ok(p, roundf(x * 100) / 100, roundf(y * 100) / 100)) continue;
                    if (!cue_clear(w, x, y, gx, gy, POOL_R * 0.9f, 0, i)) continue;
                    float pw = 0.16f + (dd + d2 * 1.7f) / 150;
                    for (int q = 0; q < 3; q++) n = add(s, n, CAP, ux, uy, pw * (0.8f + 0.3f * q), 1, x, y, 1.95f - d2 / 260 - dd / 200);
                }
            } else {
                float vx = gx - c->x, vy = gy - c->y, d1 = hypotf(vx, vy);
                if (d1 < 1e-3f || (vx * ux + vy * uy) / d1 < 0.15f) continue;
                float pw = 0.16f + (d1 + d2 * 1.7f) / 150, cs = (vx * ux + vy * uy) / d1;
                for (int q = 0; q < 3; q++)
                    n = add(s, n, CAP, vx / d1, vy / d1, pw * (0.8f + 0.3f * q), 0, 0, 0, 1 + cs * cs * (1 - (d1 + d2) / 240));
            }
        }
        if (!p->in_hand) {                      /* a full hit, softly and harder: a safety, a cushion first */
            float vx = b->x - c->x, vy = b->y - c->y, d1 = hypotf(vx, vy);
            n = add(s, n, CAP, vx / d1, vy / d1, 0.25f, 0, 0, 0, 0.5f);
            n = add(s, n, CAP, vx / d1, vy / d1, 0.6f, 0, 0, 0, 0.5f);
        }
    }
    for (int k = 0; k < 40; k++) {              /* and some at random */
        float a = 2 * CUE_PI * (float)ai_rand_f(r), x = c->x, y = c->y;
        n = add(s, n, CAP, cosf(a), sinf(a), 0.15f + 0.8f * (float)ai_rand_f(r), p->in_hand, x, y, 0);
    }
    return n;
}

static float run(const pool_pos *p, const pshot *s, int me)
{
    pool_pos q = *p;
    if (!pool_shot(&q, s->ang, s->pow, s->placed, s->x, s->y, NULL)) return -1e7f;
    return weigh(&q, me);
}

int pool_choose(const pool_pos *p, int level, uint64_t seed, char *move, int cap)
{
    if (p->status != POOL_PLAY) return 0;
    level = level < AI_EASY ? AI_EASY : level > AI_HARD ? AI_HARD : level;
    ai_rng r;
    uint64_t h = seed;
    for (int i = 0; i < p->w.n; i++) h = ai_mix(h, (uint64_t)(p->w.b[i].x * 1000) * 131 + (uint64_t)(p->w.b[i].y * 1000));
    ai_seed(&r, h);
    static const int TRIES[AI_LEVELS] = {14, 60, 400};
    static const float NOISE_DEG[AI_LEVELS] = {2.2f, 0.7f, 0.15f}, NOISE_POW[AI_LEVELS] = {0.14f, 0.06f, 0.02f};
    double t0 = ai_now_ms(), limit = ai_level_budget(level).ms * 0.55;
    pshot s[CAP];
    int n = candidates(p, s, &r);
    for (int i = n - 1; i > 0; i--) {           /* a random order (easy sees a few of them) */
        int j = ai_rand_n(&r, i + 1);
        pshot t = s[i];
        s[i] = s[j];
        s[j] = t;
    }
    if (level > AI_EASY)                        /* the others try the most promising first */
        for (int i = 1; i < n; i++) {
            pshot t = s[i];
            int j = i;
            while (j > 0 && s[j - 1].prior < t.prior) { s[j] = s[j - 1]; j--; }
            s[j] = t;
        }
    if (n > TRIES[level]) n = TRIES[level];
    if (n <= 0) return 0;
    int me = p->turn;
    enum { KEEP = 6 };
    int top[KEEP] = {0};
    float topv[KEEP];
    for (int k = 0; k < KEEP; k++) topv[k] = -1e9f;
    for (int i = 0; i < n; i++) {
        if (ai_cancelled()) return 0;
        if (i > 4 && ai_now_ms() - t0 > limit) break;
        float v = run(p, &s[i], me);
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
                pshot q = s[top[k]];
                q.ang = ((q.ang + (int)lroundf(gauss(&r) * NOISE_DEG[level] * 100)) % 36000 + 36000) % 36000;
                q.pow = (int)lroundf(q.pow * (1 + gauss(&r) * NOISE_POW[level]));
                q.pow = q.pow < 1 ? 1 : q.pow > 1000 ? 1000 : q.pow;
                sum += run(p, &q, me);
                m++;
            }
            if (sum / m > best) { best = sum / m; pick = top[k]; }
        }
    }
    pshot c = s[pick];                          /* played with the level's hand */
    c.ang = ((c.ang + (int)lroundf(gauss(&r) * NOISE_DEG[level] * 100)) % 36000 + 36000) % 36000;
    c.pow = (int)lroundf(c.pow * (1 + gauss(&r) * NOISE_POW[level]));
    c.pow = c.pow < 1 ? 1 : c.pow > 1000 ? 1000 : c.pow;
    if (c.placed) snprintf(move, (size_t)cap, "%d:%d@%d,%d", c.ang, c.pow, (int)lroundf(c.x * 100), (int)lroundf(c.y * 100));
    else snprintf(move, (size_t)cap, "%d:%d", c.ang, c.pow);
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    pool_pos p = *pool_now(pool_live());        /* the copy, at entry: nothing else is read */
    if (p.status != POOL_PLAY || p.turn != seat) return 0;
    if (!pool_choose(&p, level, ai_session_seed(), move, cap) || ai_cancelled()) return 0;
    return 1;
}

static const bot pool_bot = {.think = think, .play = pool_show};
REGISTER_BOT(pool, &pool_bot)
