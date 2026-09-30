/*
 * Blueberry Tumble: the bot (a test hook, --opt bot=1). It plays from the SCREEN only, like a player: it reads the
 * PPU's playfield map and its scroll and shear registers (what the game wrote: draw_view), and the OAM (its berry,
 * the mushrooms, the dew drops, the gust leaves, the pine cones). From what is on screen it builds its own little
 * course, keeps a model of its own berry (it knows when it pressed, like a player feels a jump) checked against the
 * sprite, and looks a little ahead (PLAN_F frames) for inputs that survive: it taps (TAP frames) when waiting would
 * not. It never reads the game's world or the director.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 */
#include "bt.h"
#include "assets.h"
#include <string.h>

#define PLAN_F 44                   /* frames looked ahead */
#define TAP 6                       /* a tap: A held this long */
#define SLACK 3                     /* it acts this many frames before the last moment */
#define VIEW_COLS 16                /* cells read ahead (the screen shows 14 in front of the berry) */
#define GROUND_TROW 44              /* the playfield map's ground row (draw.c) */
#define GROUND_PY (GROUND_TROW * 8)

typedef struct bot_t {
    berry model;                    /* its own berry as it believes it is */
    int have_model, tap_left, out, prev_hofs, speed_px16;
    int cone_n, cone_sx[8];         /* the cones on the last frame (screen x) */
} bot_t;
static bot_t B[MAX_PLAYERS];
static bt_course L;                 /* the course it sees (scratch, rebuilt every frame) */

void bot_reset(void) { memset(B, 0, sizeof B); }

void bot_state(void) { rs_state_var("bot.B", B, sizeof B); }

static int in_range(int tile, int spr_first, int spr_n)
{
    int t0 = gm_spr[spr_first].tile, t1 = gm_spr[spr_first + spr_n - 1].tile + gm_spr[spr_first + spr_n - 1].w / 8 *
                                           (gm_spr[spr_first + spr_n - 1].h / 8);
    return tile >= t0 && tile < t1;
}

/* the kind of the playfield cell whose top-left map entry is at (tx, ty) */
static int cell_kind(int tx, int ty)
{
    int best = -1;
    for (int q = 0; q < 4; q++) {
        int k = draw_tile_kind(rs_bg_get(RS_BG2, (tx + (q & 1)) & 63, (ty + (q >> 1)) & 63));
        if (k >= 0) { best = k; break; }
    }
    return best;
}

/* the berry's feet from its sprite: (screen x of the centre, height above the ground in px), the mode */
static int find_berry(int p, int hofs, int vofs, int c, int *sx, int *h, int *mode)
{
    (void)hofs;
    int found = 0;
    for (int i = 0; i < RS_OAM_MAX; i++) {
        const rs_sprite *s = rs_oam(i);
        if (!s || !s->used) continue;
        int berry_pal = p ? 1 : 0, snow_pal = p ? 6 : 5;
        int off = -1, m = M_NORMAL;
        if (s->pal == berry_pal && s->w == 16 && (in_range(s->tile, SPR_BERRY, 16) || in_range(s->tile, SPR_BERRY_SQUASH, 4) ||
                                                  in_range(s->tile, SPR_BERRY_STRETCH, 4))) off = 15;
        else if (s->pal == snow_pal && s->w == 24 && in_range(s->tile, SPR_SNOWBERRY, 4)) { off = 23; m = M_SNOW; }
        if (off < 0) continue;
        int x = s->x + s->w / 2;
        int ys = s->y + off;
        /* on a leaf? (the leaf sprite right under it) */
        for (int j = 0; j < RS_OAM_MAX; j++) {
            const rs_sprite *l = rs_oam(j);
            if (l && l->used && l->w == 32 && l->pal == 4 && in_range(l->tile, SPR_LEAF, 3) && l->x == x - 16) {
                m = M_GLIDE;
                ys = s->y + 15 - 2;
            }
        }
        *sx = x;
        *h = GROUND_PY - vofs - ((c * (x - PIVOT_X)) >> 8) - ys;
        *mode = m;
        found = 1;
        break;
    }
    return found;
}

/* ---- the plan: a small depth-first search over taps (on foot) or holds (gliding) on the course it sees --------- */
typedef struct pnode { berry b; int f, tap, choice; } pnode;
static pnode stack[PLAN_F + 2];
#define PMEMO 4096
static uint32_t memo_key[PMEMO];
static uint32_t memo_gen[PMEMO], gen = 1;

static uint32_t key_of(const pnode *n)
{
    uint32_t h = 2166136261u;
    const int32_t v[7] = {n->f, n->b.h, n->b.vy, n->b.mode | n->b.grounded << 2 | n->b.held << 3 | n->b.need_release << 4,
                          n->tap, n->b.orb_used, n->b.pad_used};
    for (int i = 0; i < 7; i++) h = (h ^ (uint32_t)v[i]) * 16777619u;
    return h | 1;
}
static int dead_seen(uint32_t k)
{
    uint32_t i = k & (PMEMO - 1);
    return memo_gen[i] == gen && memo_key[i] == k;
}
static void dead_mark(uint32_t k)
{
    uint32_t i = k & (PMEMO - 1);
    memo_gen[i] = gen;
    memo_key[i] = k;
}

static int32_t speed_q16;
static int64_t x0q;

static void apply(pnode *n, int choice)
{
    int held;
    if (n->b.mode == M_GLIDE) { held = choice ? !n->b.held : n->b.held; n->tap = 0; }
    else if (n->tap > 0) { held = 1; n->tap--; }
    else if (choice) { held = 1; n->tap = TAP - 1; }
    else held = 0;
    int64_t x = x0q + (int64_t)speed_q16 * (n->f + 1);
    berry_step(&n->b, &L, x, x, held, NULL);
    n->f++;
}

/* does the berry survive the plan when it does `first_choice` now, then waits `wait` more frames, then plays free? */
static int survives(const berry *b0, int wait, int first_choice)
{
    gen++;
    int budget = 3000;
    pnode root = {*b0, 0, 0, 0};
    apply(&root, first_choice);
    for (int i = 0; i < wait && !root.b.dead; i++) apply(&root, 0);
    if (root.b.dead) return 0;
    int sp = 0;
    stack[sp] = root;
    stack[sp].choice = 0;
    sp++;
    while (sp > 0) {
        pnode *t = &stack[sp - 1];
        if (t->f >= PLAN_F) return 1;
        if (--budget < 0) return 1;         /* it looked long enough: probably fine */
        int nch = t->b.mode == M_GLIDE ? 2 : t->tap > 0 ? 1 : 2;
        if (t->choice >= nch) { dead_mark(key_of(t)); sp--; continue; }
        pnode n = *t;
        n.choice = 0;
        apply(&n, t->choice++);
        if (n.b.dead || dead_seen(key_of(&n))) continue;
        stack[sp++] = n;
    }
    return 0;
}

int bot_decide(int p)
{
    bot_t *me = &B[p];
    int hofs, vofs, c;
    draw_view(&hofs, &vofs, &c);
    int sx, h, mode;
    if (!find_berry(p, hofs, vofs, c, &sx, &h, &mode)) { me->have_model = 0; return 0; }
    /* its speed: how fast the ground slides by (px per frame, 1/16 px), snapped to the game's tempo tiers */
    int dh = (hofs - me->prev_hofs) & 511;
    me->prev_hofs = hofs;
    if (dh > 0 && dh < 8) me->speed_px16 = me->speed_px16 ? (me->speed_px16 * 7 + dh * 16) / 8 : dh * 16;
    int best_t = 0, best_d = 1 << 30;
    for (int t = 0; t < NTIERS; t++) {
        int d = (tier_speed(t) >> 12) - me->speed_px16;
        if (d < 0) d = -d;
        if (d < best_d) { best_d = d; best_t = t; }
    }
    speed_q16 = tier_speed(best_t);

    /* the course it sees: local column 0 is the cell under its centre */
    int mapx = hofs + sx;                   /* the berry's x on the map (px, wrapping at 512) */
    int col0 = (mapx >> 4) & 31, frac = mapx & 15;
    course_reset(&L, best_t);
    course_place_flat(&L, -START_COL + VIEW_COLS + 4, PAT_BRIDGE);
    L.end_col = VIEW_COLS + 2;
    for (int j = -2; j < VIEW_COLS; j++) {
        bt_col *k = course_col_w(&L, j);
        memset(k, 0, sizeof *k);
        int tx = ((col0 + j) * 2) & 63;
        int g = cell_kind(tx, GROUND_TROW);
        k->ground = g >= 100 ? (uint8_t)(g - 100) : G_GROUND;
        for (int r = 0; r < ROWS; r++) {
            int kk = cell_kind(tx, GROUND_TROW - 2 * (r + 1));
            if (kk == K_BLOCK || kk == K_THORN || kk == K_HANG || kk == K_PEBBLE) k->cell[r] = (uint8_t)kk;
        }
    }
    /* the sprites: mushrooms, dew drops, gusts, cones (screen x -> local cell) */
    int cones_sx[8] = {0}, nc = 0;
    for (int i = 0; i < RS_OAM_MAX; i++) {
        const rs_sprite *s = rs_oam(i);
        if (!s || !s->used) continue;
        int cx = s->x + s->w / 2;
        int j = ((hofs + cx) >> 4) - (mapx >> 4);
        if (j < -2 || j >= VIEW_COLS) continue;
        int ybot = s->y + s->h;
        int hh = GROUND_PY - vofs - ((c * (cx - PIVOT_X)) >> 8) - ybot;
        int r = hh / CELL;
        bt_col *k = course_col_w(&L, j);
        if (s->pal == 2 && in_range(s->tile, SPR_MUSHROOM, 2) && r >= 0 && r < ROWS) k->cell[r] = K_PAD;
        else if (s->pal == 2 && in_range(s->tile, SPR_DEW, 2)) {
            int rr = (hh + 8) / CELL;
            if (rr >= 0 && rr < ROWS) k->cell[rr] = K_ORB;
        } else if (s->pal == 4 && s->w == 8 && in_range(s->tile, SPR_GUST, 2)) {
            k->flags |= mode == M_GLIDE ? F_LEAF_END : F_LEAF;
        } else if (s->pal == 4 && s->w == 16 && in_range(s->tile, SPR_CONE, 4) && nc < 8) {
            cones_sx[nc++] = cx;
        }
    }
    /* cones: matched with the last frame's to get their speed (a cone rolls at k of the run speed toward it) */
    for (int i = 0; i < nc; i++) {
        int best = -1, bd = 1 << 20;
        for (int q = 0; q < me->cone_n; q++) {
            int d = me->cone_sx[q] - cones_sx[i];
            if (d >= 0 && d < bd) { bd = d; best = q; }
        }
        int k256 = CONE_SLOW;
        if (best >= 0 && me->speed_px16) {
            int moved16 = (me->cone_sx[best] - cones_sx[i]) * 16;      /* screen px per frame, 1/16 */
            k256 = (moved16 - me->speed_px16) * 256 / me->speed_px16;
            k256 = k256 < CONE_SLOW - 20 ? CONE_SLOW : k256 > CONE_FAST + 40 ? CONE_FAST : (k256 < (CONE_SLOW + CONE_FAST) / 2 ? CONE_SLOW : CONE_FAST);
        }
        /* the cone's local x now (px from the local origin, the berry is at frac) */
        int64_t cxq = (int64_t)(frac + (cones_sx[i] - sx)) * Q16_ONE;
        /* cone_x(r) = m + k (m - r) with r = the berry's x = frac now: m = (cx + k r) / (1 + k) */
        int64_t rq = (int64_t)frac * Q16_ONE;
        int64_t m = (cxq * 256 + (int64_t)k256 * rq) / (256 + k256);
        bt_cone *k = &L.cone[L.ncone % CONE_RING];
        k->meet = (int32_t)(m >> 16);
        k->k256 = (uint8_t)k256;
        k->gone = 0;
        L.ncone++;
    }
    me->cone_n = nc;
    memcpy(me->cone_sx, cones_sx, sizeof cones_sx);

    /* its berry: the model, checked against the sprite */
    x0q = (int64_t)frac * Q16_ONE;
    berry b;
    if (me->have_model && me->model.mode == mode && (me->model.h >> 16) - h <= 1 && (me->model.h >> 16) - h >= -1) {
        b = me->model;
    } else {
        berry_reset(&b, x0q);
        b.h = h * Q16_ONE;
        b.mode = (uint8_t)mode;
        b.grounded = h == 0 || (me->have_model && me->model.grounded);
        b.vy = me->have_model ? me->model.vy : 0;
        b.held = (uint8_t)me->out;
    }
    b.x = x0q;
    b.dead = 0;
    b.orb_used = b.pad_used = -1;           /* ids are of the course it sees now */
    if (b.h < 0 && b.grounded) b.grounded = 0;

    /* decide like a player with a little slack: act now if waiting a few more frames would be too late (and acting now
     * works); at the last moment, act if anything works */
    int act = 0;
    if (me->tap_left > 0 && mode != M_GLIDE) act = 0;
    else if (!survives(&b, SLACK, 0)) act = survives(&b, 0, 1) || !survives(&b, 0, 0);

    int held;
    if (mode == M_GLIDE) {
        held = act ? !me->out : me->out;
        me->tap_left = 0;
    } else if (me->tap_left > 0) {
        held = 1;
        me->tap_left--;
    } else if (act) {
        held = 1;
        me->tap_left = TAP - 1;
    } else {
        held = 0;
    }
    /* the model steps with the chosen input */
    pnode n = {b, 0, 0, 0};
    n.b.held = b.held;
    int64_t x1 = x0q + speed_q16;
    berry_step(&n.b, &L, x1, x1, held, NULL);
    n.b.h = n.b.h;
    me->model = n.b;
    me->have_model = 1;
    me->out = held;
    return held;
}
