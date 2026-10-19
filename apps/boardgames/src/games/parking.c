/*
 * Traffic Jam in RetroStone Board, for one player: a wooden parking lot of 6 x 6 bays in its walnut frame, seen from
 * above, the bays ruled in pale paint, a gap cut in the frame at the right end of the third row; toy cars and trucks
 * in the house's lacquers (the red car to free, jade, amber, lapis, ivory, ebony and a few more), each with its
 * windows, lamps and wheels; the trucks carry a wooden crate. The lot in all the room the shell gives, the hint one of
 * its tools (the rules: parking_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag a vehicle along its row or column: it follows the finger and parks in the nearest bay. Or tap it: a vehicle
 * that can go to only one place goes there; else it is picked (dots on the bays it can reach) and a tap on one of
 * them drives it there. A tap on an empty bay that only one vehicle can reach brings that vehicle. The hint plays the
 * first move of a shortest way, in gold light. Undo takes back one move, all the way to the start. The bar: the moves,
 * and at its right end the par (the fewest moves) by a small red car, lit in gold when it is matched. Solved: the red
 * car drives out through the gap in gold light.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/parking.h"
#include "games/puzzles.h"
#include "games/pztools.h"
#include "shell.h"

#define FRAME 0.035f
#define BASE_MS 90                              /* a drive: this, and so much a bay */
#define BAY_MS 55
#define HINT_MS 1300
#define OUT_MS 950                              /* the red car leaving */
#define SHAKE_MS 260

enum { B_HINT, BTNS };

/* the lacquers: the red car's, then the others' in turn */
static const uint8_t RED[3] = {182, 46, 34};
static const uint8_t PAINT[9][3] = {
    {62, 128, 98}, {222, 168, 66}, {52, 84, 150}, {238, 228, 206}, {64, 58, 56},
    {118, 60, 112}, {44, 124, 134}, {136, 132, 58}, {150, 196, 214},
};
static const uint8_t GLASS[3] = {34, 44, 54}, TYRE[3] = {28, 24, 22}, LAMP[3] = {255, 244, 206},
                     TAIL[3] = {150, 30, 24};

static pkj_game G;
static int sel = -1, serial;
static struct { int on, car; float from; Uint32 t0, ms; } anim;   /* from: its place before, along its line */
static struct { int car; Uint32 t0; } hint = {-1, 0}, shake = {-1, 0};
static struct { int on; Uint32 t0; } win;
static struct { int on, car, moved; float x0, y0, x, y; float at; } ptr;   /* at: the place it is pulled to */

static box last_box;
static float bx, by;
static int q, fr, made_q = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_lot, *t_car[PKJ_CARS], *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the signed distance from (px, py) to a rounded rectangle (negative inside) */
static float rr(float px, float py, float x0, float y0, float x1, float y1, float r)
{
    float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hx = (x1 - x0) / 2 - r, hy = (y1 - y0) / 2 - r;
    float dx = fabsf(px - cx) - hx, dy = fabsf(py - cy) - hy;
    return hypotf(fmaxf(dx, 0), fmaxf(dy, 0)) + fminf(fmaxf(dx, dy), 0) - r;
}

/* a colour laid over one pixel, covering it by a (0..1) */
static void over_px(uint8_t *p, const uint8_t rgb[3], float a)
{
    if (a <= 0) return;
    if (a > 1) a = 1;
    float da = p[3] / 255.0f, oa = a + da * (1 - a);
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)((rgb[i] * a + p[i] * da * (1 - a)) / oa + 0.5f);
    p[3] = (uint8_t)(oa * 255 + 0.5f);
}

static void over(uint8_t *p, const uint8_t rgb[3], float k, float a)
{
    uint8_t c[3];
    for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(rgb[i] * k, 0, 255);
    over_px(p, c, a);
}

static float smin(float a, float b, float k)    /* a smooth union of two distances, k wide */
{
    float e = fmaxf(k - fabsf(a - b), 0) / k;
    return fminf(a, b) - e * e * k / 4;
}

static float cover(float d) { return clampf(0.5f - d, 0, 1); }

/* a vehicle's outline: its body (a truck: the cab, and the bed under the crate), swelling over its wheels */
typedef struct { float h, x0, x1, y0, y1, cab, wl, wx[3]; int nw, truck; } shape;

static float body_d(const shape *s, float px, float py)
{
    float h = s->h, d = s->truck ? fminf(rr(px, py, s->cab, s->y0, s->x1, s->y1, 0.26f * h),
                                         rr(px, py, s->x0, s->y0, s->cab + 0.1f * h, s->y1, 0.1f * h))
                                 : rr(px, py, s->x0, s->y0, s->x1, s->y1, 0.34f * h);
    for (int k = 0; k < s->nw; k++)
        d = smin(d, rr(px, py, s->wx[k] - s->wl / 2 - 0.07f * h, 0.085f * h, s->wx[k] + s->wl / 2 + 0.07f * h,
                       h - 0.085f * h, 0.15f * h), 0.1f * h);
    return d;
}

/* a vehicle seen from above, its front to the right: w x h pixels (len bays long), its lacquer. A toy: the body lit
 * as if round, a gloss along its upper side, swelling over the wheels that peek out at its sides; the glass all round
 * the roof (the windscreen, the side windows, the rear window), the roof lit on its own; the lamps. A truck (len 3):
 * the cab at the front, a wooden crate on its bed behind it. The red car: a pair of ivory stripes along it. A soft
 * shadow under it all. */
static void paint_vehicle(uint8_t *o, int w, int h, int len, const uint8_t col[3])
{
    static const uint8_t WHITE[3] = {255, 252, 244}, STRIPE[3] = {246, 238, 220}, BED[3] = {58, 52, 50},
                         SHADE[3] = {0, 0, 0};
    memset(o, 0, (size_t)w * h * 4);
    int red = col == RED;
    shape s = {.h = (float)h, .x0 = 0.05f * h, .x1 = w - 0.07f * h, .y0 = 0.115f * h, .y1 = h - 0.115f * h,
               .wl = 0.34f * h, .truck = len == 3};
    float H = (float)h, L = s.x1 - s.x0;
    s.cab = s.truck ? s.x1 - 0.95f * H : s.x0;
    if (s.truck) {
        s.nw = 3;
        s.wx[0] = s.x0 + 0.32f * H;
        s.wx[1] = s.x0 + 0.72f * H;
        s.wx[2] = s.x1 - 0.32f * H;
    } else {
        s.nw = 2;
        s.wx[0] = s.x0 + 0.42f * H;
        s.wx[1] = s.x1 - 0.44f * H;
    }
    /* the glass round the roof, and the roof */
    float g0 = s.truck ? s.cab + 0.1f * H : s.x0 + 0.2f * L, g1 = s.truck ? s.x1 - 0.28f * H : s.x0 + 0.72f * L;
    float r0 = s.truck ? s.cab + 0.12f * H : s.x0 + 0.33f * L, r1 = s.truck ? s.x1 - 0.45f * H : s.x0 + 0.6f * L;
    uint8_t *crate = NULL;
    if (s.truck && (crate = malloc((size_t)w * h * 4))) {
        pzt_plank(crate, w, h, 5);
        static const uint8_t wood[3] = {214, 170, 112};
        for (int i = 0; i < w * h; i++) pzt_tint(crate + 4 * i, wood, 1);
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            uint8_t *p = o + 4 * (y * w + x);
            /* the shadow it casts on the lot, close under it */
            float ds = body_d(&s, px - 0.025f * H, py - 0.04f * H);
            if (ds < 0.06f * H) over_px(p, SHADE, 0.32f * clampf(1 - ds / (0.06f * H), 0, 1));
            /* the wheels: a tread lit across its middle */
            for (int k = 0; k < s.nw; k++) {
                float d = rr(px, py, s.wx[k] - s.wl / 2, 0.025f * H, s.wx[k] + s.wl / 2, H - 0.025f * H, 0.1f * H);
                if (d < 0.5f) {
                    float t = (px - s.wx[k]) / (s.wl / 2);
                    over(p, TYRE, 1 + 1.3f * expf(-t * t * 3) * (0.6f + 0.4f * (1 - py / H)), cover(d));
                }
            }
            /* the body */
            float d = body_d(&s, px, py);
            if (d < 0.5f) {
                const uint8_t *c = s.truck && px < s.cab + 0.02f * H &&
                                           rr(px, py, s.cab, s.y0, s.x1, s.y1, 0.26f * H) > 0.5f
                                       ? BED : col;
                float z = sqrtf(clampf(-d / (0.24f * H), 0, 1));
                float k = (0.56f + 0.5f * z) * (1.08f - 0.18f * py / H);
                over(p, c, k, cover(d));
                if (c == col) {
                    over_px(p, WHITE, 0.42f * z * z * expf(-powf((py - 0.29f * H) / (0.07f * H), 2)) * cover(d));
                    if (red) over(p, STRIPE, 0.9f + 0.2f * z, cover(fabsf(fabsf(py - 0.5f * H) - 0.075f * H) -
                                                                   0.032f * H) * cover(d));
                }
                if (d > -1.5f) over(p, PZT_DARK, 1, 0.5f * cover(d) * clampf(d + 1.5f, 0, 1));
            }
            if (crate) {                        /* the crate on the bed: planks along it, a lid's lit rim */
                float dc = rr(px, py, s.x0 + 0.02f * H, s.y0 + 0.01f * H, s.cab - 0.05f * H, s.y1 - 0.01f * H,
                              0.06f * H);
                if (dc < 0.5f) {
                    const uint8_t *cw = crate + 4 * (y * w + x);
                    float seam = fmodf(py - s.y0, (s.y1 - s.y0) / 3);
                    float k = 1.02f - 0.16f * (py / H) + (seam < 1.1f && dc < -2 ? -0.32f : 0);
                    if (dc > -0.07f * H) k *= 0.82f + 0.3f * (1 - py / H);
                    if (dc > -1.6f) k *= 0.55f;
                    uint8_t cc[3] = {cw[0], cw[1], cw[2]};
                    over(p, cc, k, cover(dc));
                }
            }
            /* the glass: dark, lighter towards the sky, a streak across the windscreen */
            float dg = rr(px, py, g0, 0.2f * H, g1, H - 0.2f * H, (s.truck ? 0.14f : 0.17f) * H);
            if (dg < 0.5f) {
                float streak = expf(-powf((px + 0.4f * py - (g1 - 0.06f * H + 0.2f * H)) / (0.045f * H), 2));
                over(p, GLASS, 1.0f + 0.7f * (1 - py / H) + 1.6f * streak * (px > r1), cover(dg));
                if (dg > -1.2f) over(p, PZT_DARK, 1, 0.4f * cover(dg));
            }
            /* the roof: the lacquer again, a dome with its own gloss */
            float dr = rr(px, py, r0, 0.27f * H, r1, H - 0.27f * H, 0.11f * H);
            if (dr < 0.5f) {
                float z = sqrtf(clampf(-dr / (0.12f * H), 0, 1));
                over(p, col, (0.72f + 0.42f * z) * (1.06f - 0.14f * py / H), cover(dr));
                float sx = (px - (r0 + 0.35f * (r1 - r0))) / (0.25f * (r1 - r0) + 1), sy = (py - 0.38f * H) / (0.09f * H);
                over_px(p, WHITE, 0.4f * z * expf(-sx * sx - sy * sy) * cover(dr));
                if (red) over(p, STRIPE, 1.0f, cover(fabsf(fabsf(py - 0.5f * H) - 0.075f * H) - 0.032f * H) * cover(dr));
            }
            /* the lamps: two at the front with a little glow, two red ones at the back */
            for (int k = 0; k < 2; k++) {
                float ly = (k ? 0.7f : 0.3f) * H;
                float dl = rr(px, py, s.x1 - 0.13f * H, ly - 0.06f * H, s.x1 - 0.045f * H, ly + 0.06f * H, 0.05f * H);
                if (dl < 0.5f) over(p, LAMP, 1, cover(dl));
                else if (dl < 0.08f * H) over_px(p, LAMP, 0.25f * (1 - dl / (0.08f * H)));
                float dt = rr(px, py, s.x0 + 0.04f * H, ly - 0.065f * H, s.x0 + 0.095f * H, ly + 0.065f * H, 0.03f * H);
                if (dt < 0.5f) over(p, TAIL, 1.2f, 0.95f * cover(dt));
            }
        }
    free(crate);
}

/* a picture turned to a vehicle's way: way 0 its front right, 1 down, 2 left, 3 up (w x h facing right) into out */
static void turn_pic(const uint8_t *in, int w, int h, int way, uint8_t *out)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int ox, oy, ow = way & 1 ? h : w;
            switch (way) {
            case 0: ox = x; oy = y; break;
            case 1: ox = h - 1 - y; oy = x; break;
            case 2: ox = w - 1 - x; oy = h - 1 - y; break;
            default: ox = y; oy = w - 1 - x; break;
            }
            memcpy(out + 4 * ((size_t)oy * ow + ox), in + 4 * ((size_t)y * w + x), 4);
        }
}

static const uint8_t *paint_of(int i) { return i == 0 ? RED : PAINT[(i - 1) % 9]; }

/* which way vehicle i faces: the red car to the gap, the others as their place says */
static int way_of(const pkj_car *c, int i)
{
    if (i == 0) return 0;
    int h = (c->x * 7 + c->y * 13 + i * 5) % 2;
    return c->vert ? (h ? 1 : 3) : (h ? 0 : 2);
}

/* vehicle c (as it stands at the start: its way) painted pw x ph into a new picture */
static uint8_t *vehicle_pic(const pkj_car *c, int i, int pw, int ph)
{
    int w = c->vert ? ph : pw, h = c->vert ? pw : ph;
    uint8_t *a = malloc((size_t)w * h * 4), *b = malloc((size_t)w * h * 4);
    if (!a || !b) { free(a); free(b); return NULL; }
    paint_vehicle(a, w, h, c->len, paint_of(i));
    turn_pic(a, w, h, way_of(c, i), b);
    free(a);
    return b;
}

/* the lot: the plank in its walnut frame (6 x 6 bays of cq pixels, frame f), a shade darker inside, the bays ruled in
 * pale paint, the gap in the frame at the right end of the red car's row, an arrow painted before it */
static void paint_lot(uint8_t *o, int cq, int f)
{
    int W = PKJ_N * cq + 2 * f;
    art_board_n(PKJ_N, PKJ_N, cq, f, 0, o);
    static const uint8_t paint[3] = {242, 232, 206};
    for (int y = f; y < W - f; y++)
        for (int x = f; x < W - f; x++) {
            uint8_t *p = o + 4 * (y * W + x);
            float e = fminf((float)(x - f), (float)(y - f)), k = 0.8f - 0.12f * clampf(1 - e / (0.12f * cq), 0, 1);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
            float lx = (float)(x - f), ly = (float)(y - f), rx = fmodf(lx, (float)cq), ry = fmodf(ly, (float)cq);
            float t = fmaxf(1, 0.03f * cq);
            int line = ((rx < t / 2 || rx > cq - t / 2) && lx > t && lx < PKJ_N * cq - t) ||
                       ((ry < t / 2 || ry > cq - t / 2) && ly > t && ly < PKJ_N * cq - t);
            /* dashed: the stripes broken near each bay's corners */
            int corner = (rx < 0.16f * cq || rx > 0.84f * cq) && (ry < 0.16f * cq || ry > 0.84f * cq);
            if (line && !corner) over_px(p, paint, 0.38f);
        }
    /* the arrow on the last bay of the red car's row, pointing at the gap */
    float ax = f + (PKJ_N - 0.5f) * cq, ay = f + (PKJ_ROW + 0.5f) * cq, s = 0.2f * cq;
    for (int y = (int)(ay - 2 * s); y < ay + 2 * s; y++)
        for (int x = (int)(ax - 2 * s); x < ax + 2 * s; x++) {
            float dx = x + 0.5f - ax, dy = fabsf(y + 0.5f - ay);
            float d = fabsf(dx + dy - 0.2f * s) - 0.22f * s;           /* a chevron */
            if (dy > 1.2f * s) d = fmaxf(d, dy - 1.2f * s);
            if (d < 0.5f) over_px(o + 4 * (y * W + x), paint, 0.42f * clampf(0.5f - d, 0, 1));
        }
    for (int y = f + PKJ_ROW * cq; y < f + (PKJ_ROW + 1) * cq; y++)   /* the gap */
        for (int x = W - f; x < W; x++) {
            uint8_t *p = o + 4 * (y * W + x);
            float e = fminf((float)(y - f - PKJ_ROW * cq), (float)(f + (PKJ_ROW + 1) * cq - 1 - y)), a = clampf(e / 2, 0, 1);
            p[3] = (uint8_t)(p[3] * (1 - a));
        }
}

static void car_pic(int size, uint8_t *rgba)    /* the red car fitted in a square (the token) */
{
    int w = (int)(0.92f * size), h = (int)(0.5f * size);
    uint8_t *b = malloc((size_t)w * h * 4);
    if (!b) return;
    paint_vehicle(b, w, h, 2, RED);
    art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    car_pic(size, rgba);
}

static int gap_of(int cq) { return (int)fmaxf(1, 0.06f * cq); }

static void layout(box b)
{
    last_box = b;
    float S = fminf(b.w, b.h);
    fr = (int)(S * FRAME);
    q = (int)fminf((b.w - 2 * fr) / PKJ_N, (b.h - 2 * fr) / PKJ_N);
    int W = PKJ_N * q + 2 * fr;
    bx = floorf(b.x + (b.w - W) / 2);
    by = floorf(b.y + (b.h - W) / 2);
    if (made_q != q) {
        ui_free(&t_lot);
        paint_lot(ui_buf(W, W), q, fr);
        t_lot = ui_tex(W, W, ui_buf(W, W));
    }
    if (made_q != q || made_serial != serial) {
        made_q = q;
        made_serial = serial;
        int g = gap_of(q);
        for (int i = 0; i < PKJ_CARS; i++) {
            ui_free(&t_car[i]);
            if (i >= G.nc) continue;
            const pkj_car *c = &G.c[i];
            int pw = (c->vert ? 1 : c->len) * q - 2 * g, ph = (c->vert ? c->len : 1) * q - 2 * g;
            uint8_t *pic = vehicle_pic(c, i, pw, ph);
            if (pic) t_car[i] = ui_tex(pw, ph, pic);
            free(pic);
        }
    }
    ui_board_used(bx, by, (float)W, (float)W);
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));   /* the par's flag, as in the other level puzzles */
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    sel = -1;
    anim.on = win.on = 0;
    hint.car = shake.car = -1;
    memset(&ptr, 0, sizeof ptr);
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a designed level */
{
    if (!pkj_level(&G, level)) pkj_level(&G, 0);
    begin();
}

static void start_random(int variant)           /* a lot made at random: easy, medium or hard */
{
    if (!pkj_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u)) pkj_level(&G, 0);
    begin();
}

static int status(void) { return pkj_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int par(void) { return G.par; }
static int score(void) { return G.moves; }
static int level_stars(void) { return ui_par_stars(G.moves, G.par); }

static Uint32 anim_end(void) { return anim.on ? anim.t0 + anim.ms : 0; }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (anim.on && now < anim_end()) || (win.on && now < win.t0 + OUT_MS + 350);
}

static void check_win(Uint32 at)
{
    if (!pkj_solved(&G) || win.on) return;
    win.on = 1;
    win.t0 = at;
    sel = -1;
    ui_sound_later(SND_END, (int)(at - ui_ms()));
}

/* a vehicle's slide drawn from `from` (along its line) to where it is now */
static void slide(int i, int from)
{
    int d = abs(pkj_pos(&G.c[i]) - from);
    anim.on = 1;
    anim.car = i;
    anim.from = from;
    anim.t0 = ui_ms();
    anim.ms = (Uint32)(BASE_MS + BAY_MS * d);
    ui_animate();
}

/* vehicle i driven to pos along its line, in one move */
static void go(int i, int pos)
{
    int from = pkj_pos(&G.c[i]);
    if (!pkj_drive(&G, i, pos)) { ui_sound(SND_DENY); return; }
    slide(i, from);
    sel = -1;
    ui_sound(SND_MOVE);
    ui_sound_later(SND_PLACE, (int)anim.ms - 20);
    check_win(anim_end());
}

static int undo(void)
{
    if (!G.nh) return 0;
    int i = G.hist[G.nh - 1].car, from = pkj_pos(&G.c[i]);
    pkj_undo(&G);
    calm();
    slide(i, from);
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = pkj_script(&G, moves);
    check_win(ui_ms());
    return ok;
}

/* ---- the lot on the screen --------------------------------------------------------------------------------------- */
static float bay_x(float x) { return bx + fr + x * q; }
static float bay_y(float y) { return by + fr + y * q; }

static int bay_at(float x, float y, int *cx, int *cy)
{
    float ix = (x - bx - fr) / q, iy = (y - by - fr) / q;
    if (ix < 0 || iy < 0 || ix >= PKJ_N || iy >= PKJ_N) return 0;
    *cx = (int)ix;
    *cy = (int)iy;
    return 1;
}

/* where vehicle i is drawn along its line (in bays): sliding, pulled by the finger, leaving at the win */
static float car_place(int i, Uint32 now)
{
    float p = (float)pkj_pos(&G.c[i]);
    if (anim.on && anim.car == i && now < anim_end())
        p = anim.from + (p - anim.from) * puz_ease((now - anim.t0) / (float)anim.ms);
    if (ptr.on && ptr.moved && ptr.car == i) p = ptr.at;
    if (shake.car == i && now - shake.t0 < SHAKE_MS) {
        float t = (now - shake.t0) / (float)SHAKE_MS;
        p += sinf(t * 6 * PUZ_PI) * 0.05f * (1 - t);
    }
    if (win.on && i == 0 && now > win.t0) p += 2.6f * puz_ease(clampf((now - win.t0) / (float)OUT_MS, 0, 1));
    return p;
}

static void draw(void)
{
    if (made_q < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    int W = PKJ_N * q + 2 * fr, gap = gap_of(q);
    ui_put(t_lot, bx, by, (float)W, (float)W);
    if (sel >= 0) {                             /* the picked vehicle's reach: dots on the bays it can come to */
        int lo, hi;
        pkj_range(&G, sel, &lo, &hi);
        const pkj_car *c = &G.c[sel];
        for (int p = lo; p <= hi + c->len - 1; p++) {
            int x = c->vert ? c->x : p, y = c->vert ? p : c->y;
            if (pkj_at(&G, x, y) < 0) ui_mark_dot(bay_x(x + 0.5f), bay_y(y + 0.5f), 0.3f * q);
        }
    }
    for (int pass = 0; pass < 2; pass++)        /* the moving vehicle last, over the others */
        for (int i = 0; i < G.nc; i++) {
            int moving = (anim.on && anim.car == i && now < anim_end()) || (ptr.on && ptr.moved && ptr.car == i) ||
                         (win.on && i == 0);
            if (moving != pass || !t_car[i]) continue;
            const pkj_car *c = &G.c[i];
            float p = car_place(i, now), x = c->vert ? c->x : p, y = c->vert ? p : c->y;
            float pw = (float)((c->vert ? 1 : c->len) * q - 2 * gap);
            float ph = (float)((c->vert ? c->len : 1) * q - 2 * gap);
            float px = bay_x(x) + gap, py = bay_y(y) + gap, fade = 1;
            if (win.on && i == 0 && now > win.t0) {   /* leaving: the gold light round it, fading out */
                float f = clampf((now - win.t0) / (float)OUT_MS, 0, 1);
                fade = 1 - clampf((f - 0.55f) / 0.4f, 0, 1);
                if (fade > 0)
                    ui_mark_gold_a(px + pw / 2, py + ph / 2, 2.6f * q * sinf(PUZ_PI * fminf(1, f * 1.2f)) + 0.5f * q,
                                   fminf(1, 2 * fade));
                SDL_SetTextureAlphaMod(t_car[i], (Uint8)(255 * fade));
            }
            if (hint.car == i) {
                float k = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
                if (k > 0) ui_mark_gold(px + pw / 2, py + ph / 2, (1.0f + c->len * 0.55f) * q * k);
            }
            ui_shadow_flat(px + pw / 2 + 0.04f * q, py + ph / 2 + 0.06f * q, pw * 0.98f, ph * 0.98f, 0.5f * fade);
            ui_put(t_car[i], px, py, pw, ph);
            SDL_SetTextureAlphaMod(t_car[i], 255);
            if (i == sel) ui_mark_select(px, py, pw, ph);
        }
    if (anim.on && now < anim_end()) ui_animate();
    if (hint.car >= 0) {
        if (now - hint.t0 < HINT_MS) ui_animate();
        else hint.car = -1;
    }
    if (shake.car >= 0 && now - shake.t0 < SHAKE_MS) ui_animate();
    if (win.on && now < win.t0 + OUT_MS + 350) ui_animate();
    ui_tool(B_HINT, ICON_HINT, pkj_solved(&G) ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void use_hint(void)
{
    int i, p;
    if (pkj_solve(&G, &i, &p) <= 0) { ui_sound(SND_DENY); return; }
    G.last = -1;                                /* the hint's move is a move of its own */
    go(i, p);
    hint.car = i;
    hint.t0 = ui_ms();
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (pkj_solved(&G)) return;
    if (ui_tool_hit(x, y) == B_HINT) { use_hint(); return; }
    if (anim.on && ui_ms() < anim_end()) anim.on = 0;   /* a slide still going: done at once */
    int cx, cy;
    ptr.on = 1;
    ptr.car = bay_at(x, y, &cx, &cy) ? pkj_at(&G, cx, cy) : -1;
    ptr.x = ptr.x0 = x;
    ptr.y = ptr.y0 = y;
    if (ptr.car >= 0) ptr.at = (float)pkj_pos(&G.c[ptr.car]);
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    ptr.x = x;
    ptr.y = y;
    int i = ptr.car;
    if (i < 0) return;
    const pkj_car *c = &G.c[i];
    float d = (c->vert ? y - ptr.y0 : x - ptr.x0) / q;
    if (!ptr.moved && fabsf(d) > 0.18f) {
        ptr.moved = 1;
        sel = -1;
    }
    if (!ptr.moved) return;
    int lo, hi;
    pkj_range(&G, i, &lo, &hi);
    float at = clampf(pkj_pos(c) + d, (float)lo, (float)hi);
    if ((int)lroundf(at) != (int)lroundf(ptr.at)) ui_sound_soft(SND_PLACE, 0.35f);
    ptr.at = at;
    ui_animate();
}

/* the vehicle that comes over bay (x, y) the shortest way: the picked one, else the only one that can; -1 none */
static int bring(int x, int y, int *pos)
{
    int best = -1, many = 0, bd = 99;
    for (int i = 0; i < G.nc; i++) {
        if (sel >= 0 && i != sel) continue;
        const pkj_car *c = &G.c[i];
        if (c->vert ? c->x != x : c->y != y) continue;
        int lo, hi, along = c->vert ? y : x, p0 = pkj_pos(c), kp = -1, kd = 99;
        pkj_range(&G, i, &lo, &hi);
        for (int p = lo; p <= hi; p++)
            if (along >= p && along < p + c->len && abs(p - p0) < kd && p != p0) { kd = abs(p - p0); kp = p; }
        if (kp < 0) continue;
        if (best >= 0) many = 1;
        if (kd < bd) { bd = kd; best = i; *pos = kp; }
    }
    return many && sel < 0 ? -1 : best;
}

static void tap(int cx, int cy)
{
    int i = pkj_at(&G, cx, cy);
    if (i >= 0) {
        int lo, hi;
        pkj_range(&G, i, &lo, &hi);
        if (lo == hi) {
            shake.car = i;
            shake.t0 = ui_ms();
            sel = -1;
            ui_sound(SND_DENY);
        } else if (hi - lo == 1) {
            go(i, pkj_pos(&G.c[i]) == lo ? hi : lo);
        } else {
            sel = sel == i ? -1 : i;
            ui_sound(SND_TAP);
        }
        ui_animate();
        return;
    }
    int pos, k = bring(cx, cy, &pos);
    if (k < 0) {
        sel = -1;
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    go(k, pos);
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    if (ptr.moved) {
        int i = ptr.car, to = (int)lroundf(ptr.at), from = pkj_pos(&G.c[i]);
        if (to != from && pkj_drive(&G, i, to)) ui_sound(SND_PLACE);
        float off = fabsf(ptr.at - pkj_pos(&G.c[i]));
        if (off > 0.01f) {                     /* it settles into its bay from where the finger left it */
            anim.on = 1;
            anim.car = i;
            anim.from = ptr.at;
            anim.t0 = ui_ms();
            anim.ms = (Uint32)(60 + 140 * off);
        }
        check_win(anim.on ? anim_end() : ui_ms());
        return;
    }
    int cx, cy;
    if (bay_at(x, y, &cx, &cy)) tap(cx, cy);
    else if (sel >= 0) { sel = -1; ui_animate(); }
}

/* ---- the bar: the moves; the par at the right -------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.moves, t_small, G.par, pkj_solved(&G) && G.moves <= G.par);
}

/* ---- the pictures of the shell's screens: a lot, its vehicles where they are ----------------------------------- */
static void paint_lotful(const pkj_game *T, int size, uint8_t *rgba)
{
    int f = (int)(size * FRAME), cq = (int)((size * 0.96f - 2 * f) / PKJ_N), W = PKJ_N * cq + 2 * f;
    uint8_t *b = malloc((size_t)W * W * 4);
    if (!b) return;
    paint_lot(b, cq, f);
    int g = gap_of(cq);
    for (int i = 0; i < T->nc; i++) {
        const pkj_car *c = &T->c[i];
        int pw = (c->vert ? 1 : c->len) * cq - 2 * g, ph = (c->vert ? c->len : 1) * cq - 2 * g;
        uint8_t *pic = vehicle_pic(c, i, pw, ph);
        if (!pic) continue;
        art_blend(b, W, W, pic, pw, ph, f + c->x * cq + g, f + c->y * cq + g);
        free(pic);
    }
    art_blend(rgba, size, size, b, W, W, (size - W) / 2, (size - W) / 2);
    free(b);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    pkj_game T;
    int ok = variant < 0 ? pkj_level(&T, 0) : pkj_random(&T, variant, 5u + (uint32_t)variant);
    if (ok) paint_lotful(&T, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    pkj_game T;
    if (pkj_level(&T, level)) paint_lotful(&T, size, rgba);
}

/* a chapter's tab: the red car in its row, and as many trucks standing in the lot as the chapter's number */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    pkj_game T;
    memset(&T, 0, sizeof T);
    T.c[T.nc++] = (pkj_car){0, PKJ_ROW, 2, 0};
    for (int k = 0; k <= chapter && k < 6; k++) T.c[T.nc++] = (pkj_car){(int8_t)(k < 3 ? 2 + k : k - 1), k < 3 ? 0 : 3,
                                                                       3, 1};
    paint_lotful(&T, size, rgba);
}

static const game parking_game = {
    .id = "parking", .order = 445, .variants = PKJ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
    .levels = PKJ_LEVELS, .chapters = PKJ_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .par = par, .fill = 1, .tools = BTNS,
};
REGISTER_GAME(parking_game)
