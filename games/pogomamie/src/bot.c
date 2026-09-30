/*
 * Pogo Mamie: the test bot (--opt bot=1). It plays from the SCREEN STATE only: the sprites in OAM (its own
 * Mamie, the pigeons, the antennas, the props) and the play layer as the PPU shows it (the BG2 map and its scroll
 * registers, read through the tile shapes: the first opaque row of each tile column, as a player reads the
 * roofs). It never reads the world, the generator or anything off-screen. It knows the feel of the game (the
 * physics functions and tuning.h) and its own inputs, so it follows its own speed like a player does (its
 * guess is re-synchronised whenever the pixel shown disagrees).
 *
 * Each frame it tries a few dozen ways to finish the current arc (hold Right for k frames, then let go or
 * brake), keeps those that land safely (a roof, a chimney, a prop; not the street, not a skylight, no pigeon or
 * antenna on the way), prefers the one that lands furthest, away from the edges, and holds A for a big bounce
 * when a normal one could not take it over the gap or the wall ahead.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 */
#include "pm.h"
#include "assets.h"
#include <string.h>

#define W_ RS_SCREEN_W
#define NONE_Y 30000
#define SEEN_MAX 24

enum { K_GAP = 100, K_HAZARD = 101, K_FALL = 102 };

typedef struct { int x0, x1, y, kind; } seen_prop;      /* landable props (screen x, screen y of the surface) */
typedef struct { int x0, y0, x1, y1; } seen_box;         /* hazards: pigeons' bodies, antennas */

static int16_t land_y[W_], wall_y[W_];
static uint8_t col_kind[W_];                             /* SF_ROOF, SF_BUMP, SF_SKY or K_GAP */
static seen_prop props[SEEN_MAX];
static seen_box boxes[SEEN_MAX];
static int nprops, nboxes;
static int acc_x[MAX_PLAYERS], acc_y[MAX_PLAYERS], last_sx[MAX_PLAYERS], last_sy[MAX_PLAYERS];
static int base_x, base_y;                               /* the planning frame's screen origin in bot coordinates */
static mamie model[MAX_PLAYERS];
static int model_ok[MAX_PLAYERS], prev_ox[MAX_PLAYERS], prev_oy[MAX_PLAYERS], plan_a[MAX_PLAYERS], seen_once[MAX_PLAYERS];
static int sim_hazard;
static int best_x[MAX_PLAYERS], stall[MAX_PLAYERS];         /* no progress for a while: take more risks */

void bot_reset(void)
{
    for (int p = 0; p < MAX_PLAYERS; p++) model_ok[p] = seen_once[p] = plan_a[p] = stall[p] = 0, best_x[p] = -100000;
}

static int in_spr(int tile, int id, int frames)
{
    int t0 = pm_spr[id].tile, n = pm_spr[id].w / 8 * (pm_spr[id].h / 8);
    return tile >= t0 && tile < t0 + n * frames;
}

/* ---- the screen ---------------------------------------------------------------------------------------------------- */
static void read_bg2(void)
{
    int sx = draw_scroll_x(), sy = draw_scroll_y();
    for (int c = 0; c < W_; c++) {
        int mx = ((sx + c) >> 3) & 63, px = (sx + c) & 7;
        land_y[c] = wall_y[c] = NONE_Y;
        col_kind[c] = K_GAP;
        for (int r = 0; r <= RS_SCREEN_H / 8; r++) {
            uint16_t e = rs_bg_get(RS_BG2, mx, ((sy >> 3) + r) & 31);
            int t = RS_MAP_TILE(e);
            if (t >= T_COUNT) continue;
            int fl = pm_tile_flags[t];
            if (!(fl & TF_SOLID) || (fl & TF_OPEN)) continue;
            int top = pm_tile_top[t * 8 + ((e & RS_MAP_HFLIP) ? 7 - px : px)];
            if (top >= 8) continue;
            int y = r * 8 + top - (sy & 7);
            if (fl & TF_GROUND) {
                if (land_y[c] == NONE_Y) { land_y[c] = (int16_t)y; col_kind[c] = K_GAP; }
                if (wall_y[c] == NONE_Y) wall_y[c] = (int16_t)y;
                break;
            }
            if (land_y[c] == NONE_Y) {
                land_y[c] = (int16_t)y;
                col_kind[c] = (fl & TF_GLASS) ? SF_SKY : (fl & TF_BUMP) ? SF_BUMP : SF_ROOF;
            }
            if (!(fl & TF_BUMP)) { wall_y[c] = (int16_t)y; break; }
        }
    }
}

static int read_oam(int player, int *sx, int *feet, int *frame)
{
    int found = 0;
    nprops = nboxes = 0;
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        if (!s || !s->used || (s->flags & RS_SPR_HIDE)) continue;
        int t = s->tile;
        if (in_spr(t, SPR_MAMIE, 13) && s->pal == player && !found) {
            *sx = s->x + 12;
            *feet = s->y + 32;
            *frame = (t - pm_spr[SPR_MAMIE].tile) / 12;
            found = 1;
        } else if (in_spr(t, SPR_PIGEON, 3) && nboxes < SEEN_MAX) {
            int cx = s->x + 8, f = s->y + 16;
            boxes[nboxes++] = (seen_box){cx - PIGEON_W / 2, f - PIGEON_H + 2, cx + PIGEON_W / 2, f};
            if (nprops < SEEN_MAX) props[nprops++] = (seen_prop){cx - PIGEON_W / 2, cx + PIGEON_W / 2, f - PIGEON_H, SF_PIGEON};
        } else if (in_spr(t, SPR_ANTENNA, 3) && nboxes < SEEN_MAX) {
            int ax = s->x + 8, b = s->y + 32;
            boxes[nboxes++] = (seen_box){ax - ANTENNA_HALF, b - ANTENNA_H, ax + ANTENNA_HALF + 1, b};
        } else if (nprops < SEEN_MAX) {
            if (in_spr(t, SPR_AWNING, 2)) props[nprops++] = (seen_prop){s->x, s->x + 24, s->y + SURF_AWNING, SF_AWNING};
            else if (in_spr(t, SPR_POT, 1)) props[nprops++] = (seen_prop){s->x, s->x + 16, s->y + SURF_POT, SF_POT};
            else if (in_spr(t, SPR_CRADLE, 1)) props[nprops++] = (seen_prop){s->x, s->x + 32, s->y + SURF_CRADLE, SF_CRADLE};
            else if (in_spr(t, SPR_LEDGE, 3)) props[nprops++] = (seen_prop){s->x, s->x + 8, s->y + SURF_LEDGE, SF_LEDGE};
            else if (in_spr(t, SPR_BAGUETTE, 3)) props[nprops++] = (seen_prop){s->x, s->x + 8, s->y + SURF_BAGUETTE, SF_BAGUETTE};
        }
    }
    return found;
}

/* ---- the terrain as the bot sees it (bot coordinates = screen + base) -------------------------------------------- */
static int col_of(int x) { return x - base_x; }

static int bot_land(const void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, hit *h)
{
    (void)ctx; (void)x0;
    int fx = (int)(x1 >> 16), best = NONE_Y, kind = 0;
    int c = col_of(fx);
    /* the roof under the foot (or the nearest roof column under its width) */
    int cs = -1;
    if (c >= 0 && c < W_ && col_kind[c] != K_GAP) cs = c;
    for (int d = 1; d <= FOOT_HALF && cs < 0; d++) {
        if (c - d >= 0 && c - d < W_ && col_kind[c - d] != K_GAP) cs = c - d;
        else if (c + d >= 0 && c + d < W_ && col_kind[c + d] != K_GAP) cs = c + d;
    }
    int cc = c < 0 ? 0 : c >= W_ ? W_ - 1 : c;
    if (c >= W_) cs = -1;                                         /* beyond the screen: unknown, a gap */
    int sy = cs >= 0 ? land_y[cs] + base_y : c >= W_ ? NONE_Y : land_y[cc] + base_y;
    if (sy < NONE_Y / 2 && y1 >= (int32_t)sy * Q16_ONE && y0 <= (int32_t)(sy + STEP_UP) * Q16_ONE) {
        best = sy;
        kind = cs >= 0 ? col_kind[cs] : K_GAP;
    }
    for (int i = 0; i < nprops; i++) {
        const seen_prop *p = &props[i];
        if (fx + FOOT_HALF < p->x0 + base_x || fx - FOOT_HALF >= p->x1 + base_x) continue;
        int py = p->y + base_y;
        if (y1 >= (int32_t)py * Q16_ONE && y0 <= (int32_t)(py + 1) * Q16_ONE && py < best) { best = py; kind = p->kind; }
    }
    if (best == NONE_Y) return 0;
    h->kind = kind;
    h->idx = -1;
    h->sub = -1;
    h->y = best;
    h->slope = 0;
    if (kind == SF_ROOF && cs > 0 && cs < W_ - 1) {
        int d = land_y[cs + 1] - land_y[cs - 1];              /* the slope from the picture */
        if (col_kind[cs + 1] != K_GAP && col_kind[cs - 1] != K_GAP && iabs(d) <= 4 && d)
            h->slope = (d > 0 ? 1 : -1) * (iabs(d) >= 2 ? 2 : 1);
    }
    return 1;
}

static int bot_solid(const void *ctx, int x, int feet)
{
    (void)ctx;
    int c = col_of(x);
    if (c < 0) return 1;                                           /* the left edge of the screen */
    if (c >= W_ || col_kind[c] == K_GAP) return 0;
    return wall_y[c] + base_y < feet - STEP_UP;
}

static const terrain bot_terrain = {bot_land, bot_solid, 0, 0};

static int hazard_hit(const mamie *m)
{
    int x = (int)(m->x >> 16) - base_x, y = (int)(m->y >> 16) - base_y;
    for (int i = 0; i < nboxes; i++) {
        const seen_box *b = &boxes[i];
        if (x - HALF_W < b->x1 && b->x0 < x + HALF_W + 1 && y - BODY_H < b->y1 && b->y0 < y) return 1;
    }
    return 0;
}

/* ---- simulation --------------------------------------------------------------------------------------------------- */
typedef struct { int x, kind, t, vx, y; } landing;

/* hold Right for k frames, then `after` (0: let go, -1: brake to a stop) until the landing */
static landing simulate(mamie m, int k, int after, int limit)
{
    landing r = {0, K_FALL, 0, 0, 0};
    sim_hazard = 0;
    for (int t = 0; t < limit; t++) {
        int dir = t < k ? 1 : after < 0 ? (m.vx > Q16(0.1) ? -1 : 0) : after;
        hit h;
        int e = mamie_air_step(&m, &bot_terrain, dir, &h);
        if (hazard_hit(&m) && !(e & EV_LAND && h.kind == SF_PIGEON)) { r.kind = K_HAZARD; r.t = t; return r; }
        if (e & EV_LAND) {
            r.x = (int)(m.x >> 16);
            r.y = (int)h.y;
            r.kind = h.kind;
            r.t = t;
            r.vx = m.vx;
            return r;
        }
        if ((m.y >> 16) > base_y + RS_SCREEN_H + 24) { r.t = t; return r; }
    }
    return r;
}

static int safe_kind(int k) { return k != K_GAP && k != K_HAZARD && k != K_FALL && k != SF_SKY; }

/* the first place ahead of x (bot coords) where the surface ends: a gap, or a wall rising above STEP_UP */
static int edge_ahead(int x)
{
    int c = col_of(x);
    if (c < 0 || c >= W_ || col_kind[c] == K_GAP) return x;
    int y = wall_y[c];
    for (int k = c + 1; k < W_; k++)
        if (col_kind[k] == K_GAP || wall_y[k] < y - STEP_UP) return k + base_x;
    return W_ + base_x;
}

static int edge_penalty(int x)
{
    int c = col_of(x), pen = 0;
    for (int d = -14; d <= 14; d++) {
        int k = c + d;
        if (k < 0 || k >= W_ || col_kind[k] == K_GAP) { int p = (15 - iabs(d)) * 8; pen = p > pen ? p : pen; }
    }
    return pen;
}

/* from a landing at (x, y) with speed vx: can a bounce of this kind (0 normal, 1 big) take her past `beyond`? */
static int hop_passes(int x, int y, int32_t vx, int big, int beyond)
{
    static const int ks[] = {0, 8, 16, 24, 32, 44, 200};
    mamie m;
    memset(&m, 0, sizeof m);
    m.state = MS_AIR;
    m.x = (int32_t)x * Q16_ONE;
    m.y = (int32_t)y * Q16_ONE;
    m.vx = vx;
    m.vy = -bounce_speed(big ? BN_BIG : BN_NORMAL);
    for (unsigned i = 0; i < sizeof ks / sizeof ks[0]; i++) {
        landing r = simulate(m, ks[i], -1, 200);
        if (safe_kind(r.kind) && r.x > beyond) return 1;
    }
    return 0;
}

void bot_decide(int p, int *dir, int *a)
{
    *dir = 0;
    *a = plan_a[p];
    int sx = 0, feet = 0, frame = 0;
    read_bg2();
    int scx = draw_scroll_x(), scy = draw_scroll_y();
    if (seen_once[p]) {
        acc_x[p] += ((scx - last_sx[p] + 256 + 512) & 511) - 256;
        acc_y[p] += ((scy - last_sy[p] + 128 + 256) & 255) - 128;
    }
    last_sx[p] = scx;
    last_sy[p] = scy;
    base_x = acc_x[p];
    base_y = acc_y[p];
    if (!read_oam(p, &sx, &feet, &frame)) { model_ok[p] = 0; return; }
    int ox = sx + base_x, oy = feet + base_y;
    int controllable = frame <= MF_STUMBLE;
    if (!controllable) { model_ok[p] = 0; seen_once[p] = 1; prev_ox[p] = ox; prev_oy[p] = oy; return; }
    mamie *m = &model[p];
    /* follow its own motion: keep the guess while it matches the picture, else re-synchronise */
    if (!model_ok[p] || iabs((int)(m->x >> 16) - ox) > 1 || iabs((int)(m->y >> 16) - oy) > 1) {
        memset(m, 0, sizeof *m);
        m->state = MS_AIR;
        m->x = (int32_t)ox * Q16_ONE + Q16_ONE / 2;
        m->y = (int32_t)oy * Q16_ONE;
        m->vx = seen_once[p] ? (int32_t)(ox - prev_ox[p]) * Q16_ONE : VX_CRUISE;
        m->vy = seen_once[p] ? (int32_t)(oy - prev_oy[p]) * Q16_ONE + GRAVITY / 2 : 0;
        if (frame == MF_SQUASH1 || frame == MF_SQUASH2) m->vy = -bounce_speed(plan_a[p] ? BN_BIG : BN_NORMAL);
        model_ok[p] = 1;
    } else {
        if ((int)(m->x >> 16) != ox) m->x = (int32_t)ox * Q16_ONE + (m->x & 0xffff);
        if ((int)(m->y >> 16) != oy) m->y = (int32_t)oy * Q16_ONE + (m->y & 0xffff);
    }
    seen_once[p] = 1;
    prev_ox[p] = ox;
    prev_oy[p] = oy;
    if (ox > best_x[p] + 16) { best_x[p] = ox; stall[p] = 0; }
    else stall[p]++;
    int stuck = stall[p] > 300;
    /* power-ups it can see on itself: the umbrella over its head */
    m->umbrella_t = 0;
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        if (s->used && in_spr(s->tile, SPR_UMBRELLA, 2) && iabs(s->x + 12 - sx) < 4) m->umbrella_t = 100;
    }
    /* try the ways to finish this arc */
    int best = -1000000, best_k = 0, best_after = 0;
    landing best_r = {0, K_FALL, 0, 0, 0};
    for (int after = 0; after >= -1; after--)
        for (int k = 0; k <= 72; k += 4) {
            if (after == 0 && k > 0 && k < 72 && (k % 8)) continue;
            landing r = simulate(*m, k, after, 240);
            if (!safe_kind(r.kind)) continue;
            int sc = r.x * 16 - edge_penalty(r.x) * 16;
            if (!stuck && (r.kind == SF_BUMP || r.kind == SF_PIGEON)) sc += 80;
            if (sc > best) { best = sc; best_k = k; best_after = after; best_r = r; }
        }
    if (best == -1000000) {                                   /* nothing safe: brake and hope for a prop */
        landing r = simulate(*m, 0, -1, 240);
        best_k = 0;
        best_after = -1;
        best_r = r;
        if (!safe_kind(r.kind)) { best_k = 72; best_after = 0; }
    }
    *dir = best_k > 0 ? 1 : best_after < 0 ? (m->vx > Q16(0.1) ? -1 : 0) : 0;
    /* the next bounce: big only when a normal one could not pass the edge (or the wall) ahead of the landing */
    if (safe_kind(best_r.kind) && best_r.t < 14) {
        int edge = edge_ahead(best_r.x);
        int need = edge - best_r.x < 40 && edge < W_ + base_x;
        plan_a[p] = (need && !hop_passes(best_r.x, best_r.y, best_r.vx, 0, edge) && hop_passes(best_r.x, best_r.y, best_r.vx, 1, edge)) ||
                    (stuck && (stall[p] / 150) % 2);
    } else if (best_r.t >= 20) {
        plan_a[p] = 0;
    }
    *a = plan_a[p];
    /* the guess of the next frame */
    hit h;
    int e = mamie_air_step(m, &bot_terrain, *dir, &h);
    if (e & EV_LAND) mamie_bounce(m, &h, *a);
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("bot." #v, &(v), sizeof(v))
void bot_state(void)
{
    S(land_y); S(wall_y); S(col_kind); S(props); S(boxes); S(nprops); S(nboxes); S(acc_x); S(acc_y); S(last_sx);
    S(last_sy); S(base_x); S(base_y); S(model); S(model_ok); S(prev_ox); S(prev_oy); S(plan_a); S(seen_once);
    S(sim_hazard); S(best_x); S(stall);
}
#undef S

void bot_state_loaded(void) {}
