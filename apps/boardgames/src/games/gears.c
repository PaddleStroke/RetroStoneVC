/*
 * Gears in RetroStone Board, for one player: a plank of the house wood in its frame, brass pegs set in it, the motor's
 * gear turned by a crank with a walnut knob, the target gears with a red lacquer cap and their arrows, the output shafts
 * in a brass collar with their speed (the turns they make for one of the motor's) on a walnut badge (the rules:
 * gears_rules.c). The gears are drawn by code: involute teeth, small ones of turned steel, middle ones of brass with
 * five holes, big ones of wood with six spokes; their light and shadow stay put while they turn. The whole train turns
 * all the time at the speeds of its sizes, ticking softly; a jammed train stands and judders, red.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The tools: the lamp (a hint: one step of the nearest solution, played, in gold light), then the tray's sizes (the one
 * chosen on the gold disc). Tap an empty peg: a gear of the size chosen goes there (one that would hit another gear
 * shows red and does not go). Tap a gear put: it goes back to the tray. Drag a gear from the tray to a peg, or a gear
 * put to another peg (or off the board). Undo takes back one step, all the way to the start. The bar: the gears put,
 * and at its right end the par (the fewest) by its flag. Solved: gold light runs along the train.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/gears.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define FRAME 0.45f                             /* the board's frame, of a unit */
#define MARGIN 0.4f                             /* wood round the pegs' room, of a unit (seen only) */
#define SPEED 1.1                               /* the pitch circles' speed, units a second */
#define DROP_MS 300
#define LIFT_MS 220
#define DENY_MS 650
#define HINT_MS 1100
#define WIN_MS 1700
#define JUDDER_MS 900                           /* a jammed train strains: a burst each */
#define PAD 0.32f                               /* round a gear's teeth in its picture, of a unit */
#define TEETH 12                                /* a gear of size s: 12 s teeth */
#define T_HINT 0
#define T_SIZE 1

enum { MAT_STEEL, MAT_BRASS, MAT_WOOD };
static const uint8_t MAT[3][3] = {{176, 182, 190}, {214, 168, 76}, {150, 88, 50}};
static const uint8_t IVORY[3] = {240, 230, 208}, GOLD[3] = {250, 206, 96}, RED[3] = {196, 52, 40},
                     WALNUT[3] = {104, 68, 44}, BRASS[3] = {216, 172, 86}, CREAM[3] = {246, 238, 220};

static gea_game G;
static gea_eval_t E;
static int serial, sel = 1, tool_down = -1, ntray, tray[GEA_SIZES];
static double rest[GEA_PEGS];                   /* the angle of a gear that does not turn */
static Uint32 jam_t0, ease_t0;
static double shown_th[GEA_PEGS], ease_off[GEA_PEGS];   /* the angles shown last; a jump eased away */
static uint8_t shown_g[GEA_PEGS];
#define EASE_MS 380
static int was_jam, last_tick = -1;
static struct { Uint32 t0; int on; } drop[GEA_PEGS];
static struct { int on, size; float x, y; Uint32 t0; } lift;   /* a gear going back to the tray */
static struct { int on, peg, size; Uint32 t0; } deny;
static struct { int on, peg; Uint32 t0; } hintfx;
static struct { int on; Uint32 t0; } win;
static struct { int on, from, size, moved, tool, over, fit; float x0, y0, x, y; } drag;

static box last_box;
static int turned, made_u = -1, made_serial = -1, made_light = -1, made_small = -1;
static float U, ox, oy, SW, SH;                 /* a unit (pixels), the plank's top left and size on the screen */
static SDL_Texture *t_board, *t_gear[GEA_SIZES + 1], *t_light[GEA_SIZES + 1], *t_shadow[GEA_SIZES + 1], *t_cap,
                   *t_redcap, *t_knob, *t_arm[GEA_SIZES + 1], *t_arrow[GEA_SIZES + 1][2], *t_badge, *t_icon, *t_par;
static int gear_px[GEA_SIZES + 1];

static int mat_of(int s) { return s == 1 ? MAT_STEEL : s == 2 ? MAT_BRASS : MAT_WOOD; }

/* ---- pictures ------------------------------------------------------------------------------------------------------
 * A gear of size s, U pixels a unit, centred on (cx, cy) of a picture w x h, turned by rot (radians): its teeth's
 * involute flanks, a rim, a hub; steel turned in rings, brass with five holes, wood with six spokes. Its shading is
 * round (the same at any turn: the light is laid over it apart, see paint_light); lit 1 adds the light (a still
 * picture). */
static float inv_a(float a) { return tanf(a) - a; }

static void paint_gear(uint8_t *o, int w, int h, float cx, float cy, int s, float u, float rot, int lit)
{
    int N = TEETH * s, mat = mat_of(s);
    float R = s * u, md = 2.0f / TEETH * u, Rt = R + 0.8f * md, Rr = R - md, Rb = R * cosf(20 * PUZ_PI / 180);
    float psi_p = PUZ_PI / (2 * N) * 0.94f, inv_p = inv_a(20 * PUZ_PI / 180);
    float hub = (0.30f + 0.08f * (s - 1)) * u, axle = 0.1f * u, rim_in = Rr - 0.17f * u;
    float bev = fmaxf(1.2f, 0.05f * u);
    int x0 = (int)fmaxf(0, floorf(cx - Rt - 2)), x1 = (int)fminf((float)w, ceilf(cx + Rt + 2));
    int y0 = (int)fmaxf(0, floorf(cy - Rt - 2)), y1 = (int)fminf((float)h, ceilf(cy + Rt + 2));
    float cr = cosf(-rot), sr = sinf(-rot);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, rho = hypotf(dx, dy);
            if (rho > Rt + 1.5f) continue;
            float gx = dx * cr - dy * sr, gy = dx * sr + dy * cr;   /* in the gear's own frame */
            float th = atan2f(gy, gx), per = 2 * PUZ_PI / N;
            float dth = th - per * floorf(th / per + 0.5f);          /* from the nearest tooth's middle */
            float psi = psi_p + inv_p - (rho > Rb ? inv_a(acosf(fminf(1, Rb / rho))) : 0);
            psi = fmaxf(psi, 0.16f / N);
            float d_tooth = fminf(Rt - rho, (psi - fabsf(dth)) * rho), d = fmaxf(d_tooth, Rr - rho);
            float cov = clampf(d + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float k = 1, cut = 0;
            /* the edge's bevel, the rim's, the hub's */
            if (d < bev) k *= 0.8f + 0.2f * clampf(d / bev, 0, 1);
            if (rho < hub) {
                k *= 1.06f;
                if (hub - rho < bev) k *= 0.82f;
            } else if (rho < rim_in) {
                k *= 0.84f;
                if (rho - hub < bev) k *= 0.8f;
                if (rim_in - rho < bev) k *= 0.88f;
            } else if (fabsf(rho - rim_in) < bev) k *= 0.9f;
            if (rho < axle) k *= 0.3f;
            /* holes, spokes */
            if (mat == MAT_BRASS) {
                float rc = (hub + rim_in) / 2, rh = 0.36f * (rim_in - hub);
                for (int i = 0; i < 5; i++) {
                    float a = 2 * PUZ_PI * i / 5 + PUZ_PI / 10, hx = rc * cosf(a) - gx, hy = rc * sinf(a) - gy;
                    float e = hypotf(hx, hy) - rh;
                    cut = fmaxf(cut, clampf(0.5f - e, 0, 1));
                    if (e > 0 && e < bev) k *= 0.8f + 0.2f * e / bev;
                }
            } else if (mat == MAT_WOOD) {
                float in0 = hub + 0.2f * u, sw = 0.16f * u;
                float sp = 2 * PUZ_PI / 6, ds = th - sp * floorf(th / sp + 0.5f);
                float e = fminf(fminf(rho - in0, rim_in - rho), fabsf(ds) * rho - sw);
                cut = clampf(e + 0.5f, 0, 1);
                if (e < 0 && e > -bev) k *= 0.82f + 0.18f * -e / bev;
            }
            cov *= 1 - cut;
            if (cov <= 0) continue;
            /* the material */
            if (mat == MAT_WOOD) {
                float g = sinf(gy / u * 9 + 2.4f * sinf(gx / u * 1.1f) + 1.3f * sinf(gy / u * 2.3f));
                k *= 1 + 0.07f * g + 0.05f * art_noise((int)(gx * 0.7f) + 999, (int)(gy * 3));
            } else {
                k *= 1 + 0.045f * art_noise((int)(rho * 2.2f), 17) + 0.02f * sinf(rho / u * 60);
            }
            if (lit && rho > 0.5f) {
                float nd = -(dx + dy) / (1.4142f * rho), z = clampf(rho / R, 0, 1);
                k *= 1 + 0.22f * nd * z;
            }
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(MAT[mat][i] * k, 0, 255);
            pzk_over(o + 4 * ((size_t)y * w + x), c, cov);
        }
}

/* the light that stays put over a turning gear: its rim and hub brighter towards the upper left, darker towards the
 * lower right (white or black, faded) */
static void paint_light(uint8_t *o, int D, int s, float u)
{
    float c = D / 2.0f, R = s * u, Rr = R - 2.0f / TEETH * u, rim_in = Rr - 0.17f * u, hub = (0.30f + 0.08f * (s - 1)) * u;
    float k = mat_of(s) == MAT_WOOD ? 0.6f : 1;
    memset(o, 0, (size_t)D * D * 4);
    for (int y = 0; y < D; y++)
        for (int x = 0; x < D; x++) {
            float dx = x + 0.5f - c, dy = y + 0.5f - c, rho = hypotf(dx, dy);
            float band = clampf(fminf(rho - rim_in, Rr - rho) + 0.5f, 0, 1), hb = clampf(hub - rho + 0.5f, 0, 1);
            if (band <= 0 && hb <= 0) continue;
            float nd = rho > 0.5f ? -(dx + dy) / (1.4142f * rho) : 0;
            float a = band * (nd > 0 ? 0.42f * powf(nd, 1.4f) : 0.3f * -nd) + hb * (nd > 0 ? 0.5f * nd : 0.25f * -nd) *
                      clampf(rho / hub, 0, 1);
            uint8_t *p = o + 4 * ((size_t)y * D + x);
            p[0] = p[1] = p[2] = nd > 0 ? 255 : 0;
            p[3] = (uint8_t)clampf(255 * a * k, 0, 255);
        }
}

/* a gear's shadow, D x D: its own outline (teeth, holes, spokes) blurred, black */
static void paint_shadow(uint8_t *o, int D, int s, float u)
{
    float *a = malloc(sizeof *a * D * D), *t = malloc(sizeof *t * D * D);
    memset(o, 0, (size_t)D * D * 4);
    if (!a || !t) { free(a); free(t); return; }
    paint_gear(o, D, D, D / 2.0f, D / 2.0f, s, u, 0, 0);
    for (int i = 0; i < D * D; i++) a[i] = o[4 * i + 3] / 255.0f;
    int r = (int)fmaxf(1, 0.06f * u);
    for (int pass = 0; pass < 2; pass++) {      /* a box blur across, then down, twice */
        for (int y = 0; y < D; y++)
            for (int x = 0; x < D; x++) {
                float sum = 0;
                for (int k = -r; k <= r; k++) sum += x + k >= 0 && x + k < D ? a[y * D + x + k] : 0;
                t[y * D + x] = sum / (2 * r + 1);
            }
        for (int y = 0; y < D; y++)
            for (int x = 0; x < D; x++) {
                float sum = 0;
                for (int k = -r; k <= r; k++) sum += y + k >= 0 && y + k < D ? t[(y + k) * D + x] : 0;
                a[y * D + x] = sum / (2 * r + 1);
            }
    }
    for (int i = 0; i < D * D; i++) {
        o[4 * i] = o[4 * i + 1] = o[4 * i + 2] = 0;
        o[4 * i + 3] = (uint8_t)(255 * 0.42f * a[i]);
    }
    free(a);
    free(t);
}

/* two arcs round a centre, each ending in an arrow head, going clockwise (ccw: the other way), white with a dark
 * edge (tinted when drawn), D x D; their radius ra, their width wa */
static void paint_arrow(uint8_t *o, int D, float ra, float wa, int ccw)
{
    static const uint8_t white[3] = {255, 255, 255}, dark[3] = {40, 26, 16};
    float c = D / 2.0f, e = fmaxf(1.2f, 0.22f * wa);
    memset(o, 0, (size_t)D * D * 4);
    for (int pass = 0; pass < 2; pass++) {
        const uint8_t *col = pass ? white : dark;
        float grow = pass ? 0 : e;
        for (int k = 0; k < 2; k++) {
            float a0 = (k ? 30.0f : 210.0f), a1 = a0 + 110;   /* degrees, y down: clockwise */
            /* pzk_arc counts counterclockwise from the right, y up: turn ours into its */
            pzk_arc(o, D, D, c, c, ra - wa / 2 - grow, ra + wa / 2 + grow, -a1, -a0, col, 1);
            float a = a1 * PUZ_PI / 180, hx = c + ra * cosf(a), hy = c + ra * sinf(a);
            float tx = -sinf(a), ty = cosf(a), nx = cosf(a), ny = sinf(a), L = 1.9f * wa + grow * 1.6f,
                  W = 1.25f * wa + grow;
            const float tri[] = {hx + tx * L, hy + ty * L, hx + nx * W - tx * 0.15f * L, hy + ny * W - ty * 0.15f * L,
                                 hx - nx * W - tx * 0.15f * L, hy - ny * W - ty * 0.15f * L};
            for (int y = 0; y < D; y++)
                for (int x = 0; x < D; x++)
                    pzk_over(o + 4 * ((size_t)y * D + x), col, pzt_cover(tri, 3, (float)x, (float)y));
        }
    }
    if (ccw)                                    /* the mirror image turns the other way */
        for (int y = 0; y < D; y++)
            for (int x = 0; x < D / 2; x++) {
                uint32_t *a = (uint32_t *)(o + 4 * ((size_t)y * D + x)), *b = (uint32_t *)(o + 4 * ((size_t)y * D + D - 1 - x));
                uint32_t t = *a;
                *a = *b;
                *b = t;
            }
}

/* the crank's arm for a motor of size s: a brass bar from the hub out to 0.62 of its pitch circle, D x D */
static float arm_len(int s) { return 0.62f * s * U; }

static void paint_arm(uint8_t *o, int D, int s, float u)
{
    float c = D / 2.0f, L = 0.62f * s * u, wd = 0.13f * u;
    memset(o, 0, (size_t)D * D * 4);
    static const uint8_t dark[3] = {60, 40, 20};
    pzk_stroke(o, D, D, c, c, c + L, c, wd + fmaxf(1, 0.03f * u), dark, 0.8f);
    pzk_stroke(o, D, D, c, c, c + L, c, wd, BRASS, 1);
    pzk_ball(o, D, D, c, c, 0.2f * u, BRASS, 1, 0.6f);
}

static void make_tex(SDL_Texture **t, int D, void (*paint)(uint8_t *, int, int, float), int s)
{
    ui_free(t);
    uint8_t *b = ui_buf(D, D);
    memset(b, 0, (size_t)D * D * 4);
    paint(b, D, s, U);
    *t = ui_tex(D, D, b);
}

static void paint_gear_tex(uint8_t *b, int D, int s, float u) { paint_gear(b, D, D, D / 2.0f, D / 2.0f, s, u, 0, 0); }

static void paint_ball_tex(uint8_t *b, int D, const uint8_t rgb[3], float light)
{
    memset(b, 0, (size_t)D * D * 4);
    pzk_ball(b, D, D, D / 2.0f, D / 2.0f, D / 2.0f - 1, rgb, 1, light);
}

/* the shafts' walnut badge: a disc with a thin brass rim, D x D */
static void paint_badge(uint8_t *b, int D)
{
    memset(b, 0, (size_t)D * D * 4);
    pzk_ball(b, D, D, D / 2.0f, D / 2.0f, D / 2.0f - 1, BRASS, 1, 0.4f);
    pzk_ball(b, D, D, D / 2.0f, D / 2.0f, D / 2.0f - 1 - fmaxf(1.5f, 0.08f * D), WALNUT, 1, 0.3f);
}

/* the plank and its pegs: brass pins in holes, a shaft's in a brass collar */
static void paint_board(uint8_t *o, int W, int H, int f)
{
    art_plank(W - 2 * f, H - 2 * f, f, o);
    for (int i = 0; i < G.m.n; i++) {
        const gea_peg *p = &G.m.p[i];
        float px = f + (MARGIN + (turned ? G.m.h - p->y : p->x)) * U, py = f + (MARGIN + (turned ? p->x : p->y)) * U;
        float r = 0.2f * U;
        for (int y = (int)(py - r - 2); y <= (int)(py + r + 2); y++)
            for (int x = (int)(px - r - 2); x <= (int)(px + r + 2); x++) {
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                float k = art_recess(x + 0.5f - px, y + 0.5f - py, r, 0.06f * U, 0.42f, 0.24f, 0.2f);
                uint8_t *q = o + 4 * ((size_t)y * W + x);
                for (int c = 0; c < 3; c++) q[c] = (uint8_t)clampf(q[c] * k, 0, 255);
            }
        if (p->kind == GEA_SHAFT) {
            static const uint8_t dark[3] = {70, 48, 20};
            pzk_arc(o, W, H, px, py, 0.19f * U, 0.33f * U, 0, 360, dark, 0.5f);
            pzk_arc(o, W, H, px - 0.01f * U, py - 0.015f * U, 0.2f * U, 0.31f * U, 0, 360, BRASS, 1);
        }
        pzk_ball(o, W, H, px + 0.03f * U, py + 0.04f * U, 0.11f * U, WALNUT, 0.5f, 0);   /* the pin's shadow */
        pzk_ball(o, W, H, px, py, 0.1f * U, BRASS, 1, 1);
    }
}

/* ---- the layout --------------------------------------------------------------------------------------------------- */
static void to_screen(float lx, float ly, float *sx, float *sy)
{
    *sx = ox + (MARGIN + (turned ? G.m.h - ly : lx)) * U;
    *sy = oy + (MARGIN + (turned ? lx : ly)) * U;
}

static void peg_xy(int i, float *sx, float *sy) { to_screen(G.m.p[i].x, G.m.p[i].y, sx, sy); }

static void tray_of(void)
{
    ntray = 0;
    for (int s = 1; s <= GEA_SIZES; s++)
        if (G.m.sizes >> s & 1) tray[ntray++] = s;
    int ok = 0;
    for (int i = 0; i < ntray; i++) ok |= tray[i] == sel;
    if (!ok) sel = tray[0];
}

static void layout(box b)
{
    last_box = b;
    float W = G.m.w + 2 * (FRAME + MARGIN), H = G.m.h + 2 * (FRAME + MARGIN);
    float k0 = fminf(b.w / W, b.h / H), k1 = fminf(b.w / H, b.h / W);
    turned = k1 > 1.04f * k0;
    U = floorf(fminf(turned ? k1 : k0, fminf(b.w, b.h) / 7.2f));   /* a small board not blown up past a phone gear */
    SW = ((turned ? G.m.h : G.m.w) + 2 * MARGIN) * U;
    SH = ((turned ? G.m.w : G.m.h) + 2 * MARGIN) * U;
    ox = floorf(b.x + (b.w - SW) / 2);
    oy = floorf(b.y + (b.h - SH) / 2);
    int u = (int)U, light = art_frame_is_light();
    if (made_u != u || made_serial != serial || made_light != light) {
        int f = (int)(FRAME * U), BW = (int)SW + 2 * f, BH = (int)SH + 2 * f;
        uint8_t *o = malloc((size_t)BW * BH * 4);
        ui_free(&t_board);
        if (o) {
            paint_board(o, BW, BH, f);
            t_board = ui_tex(BW, BH, o);
        }
        free(o);
    }
    if (made_u != u) {
        for (int s = 1; s <= GEA_SIZES; s++) {
            int D = (int)ceilf(2 * (s + PAD) * U) + 2;
            gear_px[s] = D;
            make_tex(&t_gear[s], D, paint_gear_tex, s);
            make_tex(&t_light[s], D, paint_light, s);
            make_tex(&t_shadow[s], D, paint_shadow, s);
            make_tex(&t_arm[s], D, paint_arm, s);
            for (int w = 0; w < 2; w++) {
                ui_free(&t_arrow[s][w]);
                paint_arrow(ui_buf(D, D), D, (s == 1 ? 0.6f : 0.55f * s + 0.05f) * U, (0.1f + 0.02f * s) * U, w);
                t_arrow[s][w] = ui_tex(D, D, ui_buf(D, D));
            }
        }
        int c = (int)fmaxf(4, 0.24f * U);
        ui_free(&t_cap);
        paint_ball_tex(ui_buf(c, c), c, BRASS, 1);
        t_cap = ui_tex(c, c, ui_buf(c, c));
        int rc = (int)fmaxf(4, 0.42f * U);
        ui_free(&t_redcap);
        paint_ball_tex(ui_buf(rc, rc), rc, RED, 0.8f);
        t_redcap = ui_tex(rc, rc, ui_buf(rc, rc));
        int kn = (int)fmaxf(4, 0.4f * U);
        ui_free(&t_knob);
        paint_ball_tex(ui_buf(kn, kn), kn, WALNUT, 1.1f);
        t_knob = ui_tex(kn, kn, ui_buf(kn, kn));
        int bd = (int)fmaxf(6, 0.66f * U);
        ui_free(&t_badge);
        paint_badge(ui_buf(bd, bd), bd);
        t_badge = ui_tex(bd, bd, ui_buf(bd, bd));
    }
    made_u = u;
    made_serial = serial;
    made_light = light;
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_par);                        /* the par's flag, as the other puzzles show it */
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
        ui_free(&t_icon);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        paint_gear(ui_buf(small, small), small, small, small / 2.0f, small / 2.0f, 2, small / 4.8f, 0.1f, 1);
        t_icon = ui_tex(small, small, ui_buf(small, small));
    }
    ui_tools_shown(T_SIZE + ntray);
}

/* ---- the position ------------------------------------------------------------------------------------------------- */
static void angles(double t, double *th);

/* what the board does now; a gear that keeps its place but now meshes another way (a target driven at last, a loop
 * closed) turns from where it was shown to its new phase over a moment instead of jumping */
static void evaluate(void)
{
    gea_eval(&G.m, &G.s, &E);
    Uint32 now = ui_ms();
    if (E.jam && !was_jam) jam_t0 = now;
    was_jam = E.jam;
    double th[GEA_PEGS];
    angles(now / 1000.0, th);
    for (int i = 0; i < G.m.n; i++) {
        double d = shown_g[i] && shown_g[i] == G.s.g[i] ? fmod(shown_th[i] - th[i], 2 * PUZ_PI) : 0;
        if (d > PUZ_PI) d -= 2 * PUZ_PI;
        if (d < -PUZ_PI) d += 2 * PUZ_PI;
        ease_off[i] = d;
    }
    ease_t0 = now;
}

static void calm(void)
{
    memset(drop, 0, sizeof drop);
    lift.on = deny.on = hintfx.on = win.on = drag.on = 0;
    tool_down = -1;
}

static void begin(void)
{
    serial++;
    calm();
    was_jam = 0;
    memset(shown_g, 0, sizeof shown_g);
    for (int i = 0; i < GEA_PEGS; i++) rest[i] = 0.37 * i;
    sel = 0;                                    /* (its first size: not the last puzzle's, which a kept game played
                                                 * again would not know) */
    tray_of();
    evaluate();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!gea_level(&G, level)) gea_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    gea_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int solved(void) { return E.solved; }
static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return gea_count(&G.m, &G.s); }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(score(), par()); }

static int busy(void)
{
    Uint32 now = ui_ms();
    for (int i = 0; i < G.m.n; i++)
        if (drop[i].on && now - drop[i].t0 < DROP_MS) return 1;
    return (lift.on && now - lift.t0 < LIFT_MS) || (win.on && now - win.t0 < WIN_MS);
}

static void after(void)                         /* the board changed: what it does now, the win */
{
    int jam = was_jam;
    evaluate();
    if (E.jam && !jam) ui_sound(SND_DENY);
    if (E.solved && !win.on) {
        win.on = 1;
        win.t0 = ui_ms();
        ui_sound_later(SND_END, 120);
    }
    if (!E.solved) win.on = 0;
    ui_animate();
}

static void dropped(int peg)
{
    drop[peg].on = 1;
    drop[peg].t0 = ui_ms();
    rest[peg] = 0.37 * peg;
    ui_sound(SND_PLACE);
}

static void lifted(int peg, int size, float x, float y)
{
    lift.on = 1;
    lift.size = size;
    lift.x = x;
    lift.y = y;
    lift.t0 = ui_ms();
    drop[peg].on = 0;
    ui_sound_soft(SND_TAP, 0.6f);
}

static void denied(int peg, int size)
{
    deny.on = 1;
    deny.peg = peg;
    deny.size = size;
    deny.t0 = ui_ms();
    ui_sound(SND_DENY);
    ui_animate();
}

static void put(int peg, int size)
{
    float x, y;
    peg_xy(peg, &x, &y);
    int had = G.s.g[peg];
    if (!gea_put(&G, peg, size)) return;
    if (size) dropped(peg);
    else lifted(peg, had, x, y);
    after();
}

static void hint(void)
{
    int peg, size;
    if (solved() || !gea_hint(&G.m, &G.s, G.par, &peg, &size)) { ui_sound(SND_DENY); return; }
    put(peg, size);
    hintfx.on = 1;
    hintfx.peg = peg;
    hintfx.t0 = ui_ms();
}

static int undo(void)
{
    calm();
    int r = gea_undo(&G);
    evaluate();
    return r;
}

static int script(const char *moves)
{
    calm();
    int ok = gea_script(&G, moves);
    evaluate();
    if (E.solved) { win.on = 1; win.t0 = ui_ms(); }
    return ok;
}

/* ---- the train's motion ------------------------------------------------------------------------------------------ */
/* each gear's angle now (radians, the board's own frame): the motor's turning with the clock, each gear driven
 * meshing with the one driving it (a tooth of one in a gap of the other on the line between them); a jammed train
 * stood still where it jammed */
static void angles(double t, double *th)
{
    int mo = 0, done[GEA_PEGS] = {0};
    for (int i = 0; i < G.m.n; i++) {
        th[i] = rest[i];
        if (G.m.p[i].kind == GEA_MOTOR) mo = i;
    }
    double tt = E.jam ? jam_t0 / 1000.0 : t;
    th[mo] = SPEED / G.m.p[mo].size * tt;
    done[mo] = 1;
    for (int pass = 0; pass < G.m.n; pass++)
        for (int v = 0; v < G.m.n; v++) {
            int u = E.from[v];
            if (done[v] || !E.train[v] || u < 0 || !done[u]) continue;
            double phi = atan2((double)G.m.p[v].y - G.m.p[u].y, (double)G.m.p[v].x - G.m.p[u].x);
            double nu = (double)TEETH * G.s.g[u], nv = (double)TEETH * G.s.g[v];
            th[v] = phi + PUZ_PI - PUZ_PI / nv + (phi - th[u]) * nu / nv;
            done[v] = 1;
        }
}

/* ---- drawing ------------------------------------------------------------------------------------------------------ */
static void draw_gear(int s, float x, float y, double th, float scale, Uint8 alpha, const uint8_t *tint)
{
    float D = gear_px[s] * scale;
    double deg = fmod(th * 180 / PUZ_PI + (turned ? 90 : 0), 360);
    SDL_SetTextureAlphaMod(t_gear[s], alpha);
    SDL_SetTextureColorMod(t_gear[s], tint ? tint[0] : 255, tint ? tint[1] : 255, tint ? tint[2] : 255);
    ui_put_rot(t_gear[s], x, y, D, D, deg);
    SDL_SetTextureColorMod(t_gear[s], 255, 255, 255);
    SDL_SetTextureAlphaMod(t_gear[s], 255);
    SDL_SetTextureAlphaMod(t_light[s], alpha);
    ui_put_c(t_light[s], x, y, D, D);
    SDL_SetTextureAlphaMod(t_light[s], 255);
}

static void draw_shadow(int s, float x, float y, float lift, float a, double th)
{
    float D = gear_px[s] * (1 + 0.04f * lift);
    SDL_SetTextureAlphaMod(t_shadow[s], (Uint8)clampf(255 * a, 0, 255));
    ui_put_rot(t_shadow[s], x + (0.07f + 0.12f * lift) * U, y + (0.1f + 0.16f * lift) * U, D, D,
               fmod(th * 180 / PUZ_PI + (turned ? 90 : 0), 360));
    SDL_SetTextureAlphaMod(t_shadow[s], 255);
}

static void draw_arrow(int s, int dir, float x, float y, const uint8_t rgb[3], Uint8 a)
{
    SDL_Texture *t = t_arrow[s][dir < 0];
    float D = (float)gear_px[s];
    SDL_SetTextureColorMod(t, rgb[0], rgb[1], rgb[2]);
    SDL_SetTextureAlphaMod(t, a);
    ui_put_c(t, x, y, D, D);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a shaft's speed on its badge: the turns it makes for one of the motor's, a whole number or a fraction */
static void draw_badge(int peg, float x, float y)
{
    int mo = 0;
    for (int i = 0; i < G.m.n; i++)
        if (G.m.p[i].kind == GEA_MOTOR) mo = i;
    int a = G.m.p[mo].size, b = G.m.p[peg].size, g = a;
    for (int r = b; r;) { int t = g % r; g = r; r = t; }
    a /= g;
    b /= g;
    float d = 0.66f * U;
    ui_put_c(t_badge, x, y, d, d);
    if (b == 1) ui_number_ex(x, y, 0.36f * U, a, CREAM, 255, UI_NUM_TIGHT);
    else {
        ui_number_ex(x, y - 0.13f * U, 0.22f * U, a, CREAM, 255, UI_NUM_TIGHT);
        ui_number_ex(x, y + 0.14f * U, 0.22f * U, b, CREAM, 255, UI_NUM_TIGHT);
        ui_fill(x - 0.13f * U, y - 0.012f * U, 0.26f * U, fmaxf(1, 0.035f * U), CREAM[0], CREAM[1], CREAM[2], 255);
    }
}

static float drop_scale(int i, Uint32 now, float *lift)
{
    *lift = 0;
    if (!drop[i].on) return 1;
    float f = (now - drop[i].t0) / (float)DROP_MS;
    if (f >= 1) { drop[i].on = 0; return 1; }
    ui_animate();
    /* falls from a little above, a small bounce as it settles */
    float e = 1 - f, b = sinf(f * PUZ_PI * 2.2f) * e * e * 0.05f;
    *lift = e * e;
    return 1 + 0.16f * e * e + b;
}

static void draw(void)
{
    if (made_u < 0 || made_serial != serial || made_light != art_frame_is_light()) layout(last_box);
    Uint32 now = ui_ms();
    double t = now / 1000.0, th[GEA_PEGS];
    angles(t, th);
    float ek = (float)(int)(now - ease_t0) / EASE_MS;
    for (int i = 0; i < G.m.n; i++) {
        if (ek < 1) th[i] += ease_off[i] * (1 - puz_ease(ek));
        shown_th[i] = th[i];
        shown_g[i] = G.s.g[i];
    }
    if (ek < 1) ui_animate();
    int f = (int)(FRAME * U);
    ui_put(t_board, ox - f, oy - f, SW + 2 * f, SH + 2 * f);
    /* the judder of a jammed train: bursts of a quick shake */
    float jud = 0;
    if (E.jam) {
        int k = (int)((now - jam_t0) % JUDDER_MS);
        if (k < 320) jud = 0.035f * sinf(k * 0.11f) * (1 - k / 320.0f);
        ui_animate();
    }
    int dragged = drag.on && drag.moved && drag.from >= 0 ? drag.from : -1;
    /* the shadows, then the gears, then what stays put over them */
    for (int i = 0; i < G.m.n; i++) {
        if (!G.s.g[i] || i == dragged) continue;
        float x, y, lift;
        peg_xy(i, &x, &y);
        drop_scale(i, now, &lift);
        draw_shadow(G.s.g[i], x, y, lift, 1, th[i]);
    }
    for (int i = 0; i < G.m.n; i++) {
        if (!G.s.g[i] || i == dragged) continue;
        float x, y, lift, sc = drop_scale(i, now, &lift);
        peg_xy(i, &x, &y);
        double a = th[i] + (E.jam && E.train[i] ? jud * (i % 2 ? 1 : -1) : 0);
        draw_gear(G.s.g[i], x, y, a, sc, 255, NULL);
    }
    for (int i = 0; i < G.m.n; i++) {
        const gea_peg *p = &G.m.p[i];
        float x, y;
        peg_xy(i, &x, &y);
        if (E.jam && E.train[i] && G.s.g[i] && (now - jam_t0) % JUDDER_MS < 420)
            ui_mark_glow(x, y, 2.2f * G.s.g[i] * U * 0.8f);
        if (p->kind == GEA_MOTOR) {             /* the crank: its arm turning, its knob */
            double a = th[i] + (turned ? PUZ_PI / 2 : 0) + (E.jam ? jud * 2 : 0);
            ui_put_rot(t_arm[p->size], x, y, (float)gear_px[p->size], (float)gear_px[p->size], a * 180 / PUZ_PI);
            float kx = x + arm_len(p->size) * (float)cos(a), ky = y + arm_len(p->size) * (float)sin(a), kd = 0.4f * U;
            ui_shadow_flat(kx + 0.04f * U, ky + 0.06f * U, kd, kd, 0.5f);
            ui_put_c(t_knob, kx, ky, kd, kd);
            continue;
        }
        if (p->kind == GEA_TARGET || p->kind == GEA_SHAFT) {
            const uint8_t *col = E.ok[i] ? GOLD : E.dir[i] && G.s.g[i] ? RED : IVORY;
            int s = p->kind == GEA_TARGET ? p->size : G.s.g[i] ? G.s.g[i] : 1;
            if (E.ok[i]) ui_mark_gold_a(x, y, 1.3f * (s + 0.4f) * U, 0.35f + 0.15f * sinf(now / 260.0f));
            if (i != dragged) draw_arrow(s, p->dir, x, y, col, 235);
            if (p->kind == GEA_TARGET) ui_put_c(t_redcap, x, y, 0.42f * U, 0.42f * U);
            else draw_badge(i, x, y);
            if (E.ok[i]) ui_animate();
            continue;
        }
        if (G.s.g[i] && i != dragged) ui_put_c(t_cap, x, y, 0.24f * U, 0.24f * U);
    }
    /* a gear going back to the tray */
    if (lift.on) {
        float k = (now - lift.t0) / (float)LIFT_MS;
        if (k >= 1) lift.on = 0;
        else {
            draw_shadow(lift.size, lift.x, lift.y, 1 + k, 1 - k, 0);
            draw_gear(lift.size, lift.x - 0.1f * U * k, lift.y - 0.25f * U * k, 0, 1 + 0.12f * k, (Uint8)(255 * (1 - k)),
                      NULL);
            ui_animate();
        }
    }
    /* a gear refused: its red ghost */
    if (deny.on) {
        float k = (now - deny.t0) / (float)DENY_MS;
        if (k >= 1) deny.on = 0;
        else {
            float x, y;
            static const uint8_t red[3] = {255, 90, 70};
            peg_xy(deny.peg, &x, &y);
            draw_gear(deny.size, x + 0.05f * U * sinf(k * 40) * (1 - k), y, 0, 1, (Uint8)(170 * (1 - k * k)), red);
            ui_animate();
        }
    }
    /* the hint's gold light */
    if (hintfx.on) {
        float k = (now - hintfx.t0) / (float)HINT_MS;
        if (k >= 1) hintfx.on = 0;
        else {
            float x, y;
            peg_xy(hintfx.peg, &x, &y);
            ui_mark_gold_a(x, y, 2.6f * (G.s.g[hintfx.peg] ? G.s.g[hintfx.peg] : 1) * U, puz_pulse(k, 0, 1));
            ui_animate();
        }
    }
    /* the win: gold light along the train, from the motor out */
    if (win.on) {
        float wt = (float)(int)(now - win.t0);
        if (wt < WIN_MS) {
            for (int i = 0; i < G.m.n; i++) {
                if (!E.train[i]) continue;
                int depth = 0;
                for (int v = i; E.from[v] >= 0 && depth < GEA_PEGS; v = E.from[v]) depth++;
                float a = puz_pulse(wt, 110.0f * depth, 560), x, y;
                peg_xy(i, &x, &y);
                if (a > 0) ui_mark_gold_a(x, y, 2.4f * G.s.g[i] * U, a);
            }
            ui_animate();
        }
    }
    /* a gear dragged: where it would go (a ghost; red where it would hit another gear), and the gear in the hand */
    if (drag.on && drag.moved) {
        if (drag.over >= 0) {
            float x, y;
            static const uint8_t red[3] = {255, 80, 64};
            peg_xy(drag.over, &x, &y);
            draw_gear(drag.size, x, y, 0, 1, drag.fit ? 110 : 150, drag.fit ? NULL : red);
        }
        draw_shadow(drag.size, drag.x, drag.y, 1.4f, 0.8f, 0);
        draw_gear(drag.size, drag.x, drag.y, 0, 1.08f, 235, NULL);
    }
    /* the ticking: a soft tick for each tooth of the motor's, while gears turn */
    if (!E.jam && E.put > E.idle && !ui_replaying()) {
        int mo = 0;
        for (int i = 0; i < G.m.n; i++)
            if (G.m.p[i].kind == GEA_MOTOR) mo = i;
        int tick = (int)floor(th[mo] * TEETH * G.m.p[mo].size / (2 * PUZ_PI));
        if (last_tick >= 0 && tick != last_tick) ui_sound_soft(SND_TAP, 0.05f);
        last_tick = tick;
    }
    ui_animate();                               /* the train turns all the time */
    /* the tools */
    ui_tool(T_HINT, ICON_HINT, (solved() ? UI_DIM : 0) | (tool_down == T_HINT ? UI_ON : 0));
}

/* ---- the tray's tools ---------------------------------------------------------------------------------------------- */
static void paint_tool(int s, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    float k = s == 1 ? 0.5f : s == 2 ? 0.7f : 0.92f;          /* its size shown by its size on the disc */
    float u = k * size / (2 * (s + 0.2f));
    paint_gear(o, size, size, size / 2.0f, size / 2.0f, s, u, 0.2f, 1);
    pzk_ball(o, size, size, size / 2.0f, size / 2.0f, fmaxf(1.5f, 0.12f * u), BRASS, 1, 1);
}

static void draw_tools(void)
{
    for (int k = 0; k < ntray; k++)
        ui_tool_paint(T_SIZE + k, paint_tool, tray[k], 0.86f,
                      UI_IVORY | (tray[k] == sel ? UI_ON : 0) | (solved() ? UI_DIM : 0));
}

static void draw_all(void)
{
    draw();
    draw_tools();
}

/* ---- the pointer -------------------------------------------------------------------------------------------------- */
static void to_lattice(float x, float y, float *lx, float *ly)
{
    float a = (x - ox) / U - MARGIN, b = (y - oy) / U - MARGIN;
    *lx = turned ? b : a;
    *ly = turned ? G.m.h - a : b;
}

/* the peg under (x, y): a gear whose disc is there, else the nearest peg within reach (d units); -1 none */
static int peg_at(float x, float y, float reach, int skip)
{
    float lx, ly, best = 1e9f;
    int pick = -1;
    to_lattice(x, y, &lx, &ly);
    for (int i = 0; i < G.m.n; i++) {
        if (i == skip) continue;
        float d = hypotf(lx - G.m.p[i].x, ly - G.m.p[i].y);
        float r = G.s.g[i] ? fmaxf(reach, (float)G.s.g[i]) : reach;
        if (d < r && d / r < best) { best = d / r; pick = i; }
    }
    return pick;
}

static void drag_follow(float x, float y)
{
    drag.x = x;
    drag.y = y;
    drag.over = peg_at(x, y, 0.9f, -1);
    if (drag.over >= 0 && (gea_fixed(&G.m, drag.over) || (G.s.g[drag.over] && drag.over != drag.from)))
        drag.over = -1;
    drag.fit = 0;
    if (drag.over >= 0) {
        gea_state t = G.s;
        if (drag.from >= 0) t.g[drag.from] = 0;
        drag.fit = gea_fits(&G.m, &t, drag.over, drag.size) == GEA_OK;
    }
}

static void press(float x, float y)
{
    drag.on = 0;
    tool_down = ui_tool_hit(x, y);
    if (tool_down >= 0) {
        if (tool_down >= T_SIZE && tool_down < T_SIZE + ntray && !solved()) {
            drag.on = 1;                        /* a gear may be dragged off the tray */
            drag.tool = 1;
            drag.from = -1;
            drag.size = tray[tool_down - T_SIZE];
            drag.moved = 0;
            drag.x0 = drag.x = x;
            drag.y0 = drag.y = y;
            drag.over = -1;
        }
        ui_animate();
        return;
    }
    if (solved()) return;
    int p = peg_at(x, y, 0.8f, -1);
    if (p < 0) return;
    drag.on = 1;
    drag.tool = 0;
    drag.from = G.s.g[p] && !gea_fixed(&G.m, p) ? p : -1;
    drag.size = drag.from >= 0 ? G.s.g[p] : sel;
    drag.over = p;
    drag.moved = 0;
    drag.x0 = drag.x = x;
    drag.y0 = drag.y = y;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (!drag.moved && hypotf(x - drag.x0, y - drag.y0) > 0.3f * U) {
        if (!drag.tool && drag.from < 0) { drag.on = 0; return; }   /* from an empty peg: nothing to drag */
        drag.moved = 1;
        if (drag.tool) tool_down = -1;
    }
    if (drag.moved) {
        drag_follow(x, y);
        ui_animate();
    }
}

static void tap(int p)
{
    if (gea_fixed(&G.m, p)) { ui_sound(SND_DENY); return; }
    if (G.s.g[p]) { put(p, 0); return; }        /* a gear put: back to the tray */
    int why = gea_fits(&G.m, &G.s, p, sel);
    if (why == GEA_OK) put(p, sel);
    else denied(p, sel);
}

static void release(float x, float y)
{
    if (tool_down >= 0 && !(drag.on && drag.moved)) {
        int k = tool_down;
        tool_down = -1;
        drag.on = 0;
        ui_animate();
        if (ui_tool_hit(x, y) != k) return;
        if (k == T_HINT) hint();
        else if (k >= T_SIZE && k < T_SIZE + ntray && !solved()) {
            sel = tray[k - T_SIZE];
            ui_sound(SND_TAP);
        }
        return;
    }
    tool_down = -1;
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) {
        if (drag.over >= 0) tap(drag.over);
        return;
    }
    drag_follow(x, y);
    if (drag.tool) {                            /* off the tray */
        sel = drag.size;
        if (drag.over < 0) { ui_animate(); return; }
        if (drag.fit) put(drag.over, drag.size);
        else denied(drag.over, drag.size);
        return;
    }
    if (drag.over == drag.from) { ui_animate(); return; }
    if (drag.over < 0) {                        /* off the board: back to the tray */
        int had = G.s.g[drag.from];
        if (gea_put(&G, drag.from, 0)) {
            lifted(drag.from, had, x, y);
            after();
        }
        return;
    }
    if (drag.fit && gea_shift(&G, drag.from, drag.over)) {
        dropped(drag.over);
        after();
    } else denied(drag.over, drag.size);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, t_icon, score(), t_par, par(), solved() && score() <= par());
}

/* ---- the shell's pictures ------------------------------------------------------------------------------------------ */
typedef struct { float x, y; int s, kind, dir; } scene_gear;

/* gears (s 0: a bare peg; a shaft's in its collar) size x size: on a plank (lattice w x h) or alone, fitted to their
 * extent; each meshing with an earlier one when they mesh (the angles worked out as on the board) */
static void paint_scene(uint8_t *rgba, int size, int w, int h, const scene_gear *g, int n, int plank)
{
    memset(rgba, 0, (size_t)size * size * 4);
    float u, ex, ey;
    if (plank) {
        u = size / (fmaxf((float)w, (float)h) + 2 * FRAME);
        int f = (int)(FRAME * u), W = (int)(w * u), H = (int)(h * u), x0 = (size - W) / 2 - f, y0 = (size - H) / 2 - f;
        if (u < 2) return;
        uint8_t *b = malloc((size_t)(W + 2 * f) * (H + 2 * f) * 4);
        if (b) {
            art_plank(W, H, f, b);
            art_blend(rgba, size, size, b, W + 2 * f, H + 2 * f, x0, y0);
            free(b);
        }
        ex = (float)(x0 + f);
        ey = (float)(y0 + f);
    } else {
        float a0 = 1e9f, a1 = -1e9f, b0 = 1e9f, b1 = -1e9f;
        for (int i = 0; i < n; i++) {
            float r = g[i].s ? g[i].s + 0.2f : 0.4f;
            a0 = fminf(a0, g[i].x - r);
            a1 = fmaxf(a1, g[i].x + r);
            b0 = fminf(b0, g[i].y - r);
            b1 = fmaxf(b1, g[i].y + r);
        }
        u = 0.96f * size / fmaxf(a1 - a0, b1 - b0);
        if (u < 2) return;
        ex = (size - (a1 - a0) * u) / 2 - a0 * u;
        ey = (size - (b1 - b0) * u) / 2 - b0 * u;
    }
    float th[GEA_PEGS];
    for (int i = 0; i < n && i < GEA_PEGS; i++) {
        th[i] = 0.3f;
        for (int j = 0; j < i && g[i].s; j++) { /* meshing with an earlier one */
            float dx = g[i].x - g[j].x, dy = g[i].y - g[j].y;
            if (!g[j].s || fabsf(hypotf(dx, dy) - (g[i].s + g[j].s)) > 0.01f) continue;
            float phi = atan2f(dy, dx), nu = (float)TEETH * g[j].s, nv = (float)TEETH * g[i].s;
            th[i] = phi + PUZ_PI - PUZ_PI / nv + (phi - th[j]) * nu / nv;
            break;
        }
    }
    for (int i = 0; i < n; i++) {               /* the bare pegs, their shadows */
        float cx = ex + g[i].x * u, cy = ey + g[i].y * u;
        if (!g[i].s) {
            if (g[i].kind == GEA_SHAFT) pzk_arc(rgba, size, size, cx, cy, 0.2f * u, 0.34f * u, 0, 360, BRASS, 1);
            pzk_ball(rgba, size, size, cx, cy, fmaxf(1.2f, 0.13f * u), BRASS, 1, 1);
            continue;
        }
        static const uint8_t black[3] = {0, 0, 0};
        pzk_ball(rgba, size, size, cx + 0.08f * u, cy + 0.12f * u, (g[i].s + 0.02f) * u, black, 0.3f, 0);
    }
    for (int i = 0; i < n; i++) {
        float cx = ex + g[i].x * u, cy = ey + g[i].y * u;
        if (!g[i].s) continue;
        paint_gear(rgba, size, size, cx, cy, g[i].s, u, th[i], 1);
        if (g[i].kind == GEA_TARGET) pzk_ball(rgba, size, size, cx, cy, 0.21f * u, RED, 1, 0.8f);
        else pzk_ball(rgba, size, size, cx, cy, 0.12f * u, BRASS, 1, 1);
        if (g[i].kind == GEA_MOTOR) {
            float L = 0.62f * g[i].s * u, a = -0.6f;
            pzk_stroke(rgba, size, size, cx, cy, cx + L * cosf(a), cy + L * sinf(a), 0.07f * u, BRASS, 1);
            pzk_ball(rgba, size, size, cx + L * cosf(a), cy + L * sinf(a), 0.2f * u, WALNUT, 1, 1.1f);
        }
        if (g[i].kind == GEA_TARGET && g[i].dir) {
            int D = (int)(2 * (g[i].s + PAD) * u) + 2;
            uint8_t *a = malloc((size_t)D * D * 4);
            if (a) {
                paint_arrow(a, D, (g[i].s == 1 ? 0.6f : 0.55f * g[i].s + 0.05f) * u, (0.1f + 0.02f * g[i].s) * u,
                            g[i].dir < 0);
                for (int k = 0; k < D * D; k++)
                    if (a[4 * k] > 128) { a[4 * k] = GOLD[0]; a[4 * k + 1] = GOLD[1]; a[4 * k + 2] = GOLD[2]; }
                art_blend(rgba, size, size, a, D, D, (int)lroundf(cx - D / 2.0f), (int)lroundf(cy - D / 2.0f));
                free(a);
            }
        }
    }
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    paint_gear(rgba, size, size, size / 2.0f, size / 2.0f, 2, size / 4.9f, 0.1f, 1);
    pzk_ball(rgba, size, size, size / 2.0f, size / 2.0f, 0.1f * size, BRASS, 1, 1);
}

/* a board's fixed gears and its pegs, fitted (a level's tile) */
static void map_scene(const gea_map *m, int size, uint8_t *rgba)
{
    scene_gear g[GEA_PEGS];
    int n = 0;
    for (int i = 0; i < m->n; i++)
        g[n++] = (scene_gear){m->p[i].x, m->p[i].y, gea_fixed(m, i) ? m->p[i].size : 0, m->p[i].kind, m->p[i].dir};
    paint_scene(rgba, size, m->w, m->h, g, n, 0);
}

/* the menu's card: a motor's brass gear, a steel one and a wooden one meshing; the picker's: boards of each size */
static void thumb(int variant, int size, uint8_t *rgba)
{
    if (variant < 0) {
        const scene_gear g[] = {{3.2f, 2.9f, 2, GEA_MOTOR, 1}, {6.2f, 2.9f, 1, GEA_PEG, 0},
                                {6.2f, 6.9f, 3, GEA_TARGET, 1}};
        paint_scene(rgba, size, 10, 10, g, 3, 0);
        return;
    }
    static const scene_gear A[] = {{3, 3, 1, GEA_MOTOR, 1}, {5, 3, 1, GEA_PEG, 0}, {5, 5, 1, GEA_TARGET, -1}};
    static const scene_gear B[] = {{3, 3, 2, GEA_MOTOR, 1}, {6, 3, 1, GEA_PEG, 0}, {6, 5, 1, GEA_PEG, 0},
                                   {6, 8, 2, GEA_TARGET, 1}};
    static const scene_gear C[] = {{3, 3, 2, GEA_MOTOR, 1}, {6, 3, 1, GEA_PEG, 0}, {10, 3, 3, GEA_PEG, 0},
                                   {10, 7, 1, GEA_TARGET, -1}};
    if (variant == 0) paint_scene(rgba, size, 8, 8, A, 3, 1);
    else if (variant == 1) paint_scene(rgba, size, 9, 11, B, 4, 1);
    else paint_scene(rgba, size, 13, 11, C, 4, 1);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    gea_game *t = malloc(sizeof *t);
    if (t && gea_level(t, level)) map_scene(&t->m, size, rgba);
    free(t);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const scene_gear C0[] = {{3, 3, 1, GEA_TARGET, 1}};
    static const scene_gear C1[] = {{2.5f, 3, 1, GEA_PEG, 0}, {5.5f, 3, 2, GEA_PEG, 0}};
    static const scene_gear C2[] = {{2.5f, 2.6f, 2, GEA_PEG, 0}, {5.5f, 2.6f, 1, GEA_PEG, 0}, {5.5f, 6.6f, 3, GEA_PEG, 0}};
    static const scene_gear C3[] = {{4, 4, 2, GEA_MOTOR, 1}, {7, 4, 1, GEA_TARGET, -1}, {4, 7, 1, GEA_TARGET, -1}};
    static const scene_gear C4[] = {{3, 4, 2, GEA_MOTOR, 1}, {6, 4, 1, GEA_PEG, 0}};
    static const scene_gear C5[] = {{2, 2, 1, GEA_PEG, 0}, {4, 2, 1, GEA_PEG, 0}, {6, 2, 1, GEA_PEG, 0},
                                    {2, 4, 1, GEA_PEG, 0}, {4, 4, 1, GEA_PEG, 0}, {6, 4, 1, GEA_PEG, 0},
                                    {2, 6, 1, GEA_PEG, 0}, {4, 6, 1, GEA_PEG, 0}, {6, 6, 1, GEA_PEG, 0}};
    switch (chapter) {
    case 0: paint_scene(rgba, size, 6, 6, C0, 1, 0); break;
    case 1: paint_scene(rgba, size, 8, 6, C1, 2, 0); break;
    case 2: paint_scene(rgba, size, 10, 10, C2, 3, 0); break;
    case 3: paint_scene(rgba, size, 8, 8, C3, 3, 0); break;
    case 4:
        paint_scene(rgba, size, 8, 8, C4, 2, 0);
        {                                       /* the speed: a walnut badge with a 2 */
            float u = size / (8 + 2 * FRAME), cx = (size - 8 * u) / 2 + 6 * u, cy = (size - 8 * u) / 2 + 4 * u;
            pzk_ball(rgba, size, size, cx, cy, 0.42f * u, BRASS, 1, 0.4f);
            pzk_ball(rgba, size, size, cx, cy, 0.36f * u, WALNUT, 1, 0.3f);
            puz_digits(rgba, size, size, 2, cx, cy, 0.5f * u, CREAM, 1, 1);
        }
        break;
    default: paint_scene(rgba, size, 8, 8, C5, 9, 0); break;
    }
}

static const game gears_game = {
    .id = "gears", .order = 496, .variants = GEA_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw_all,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = GEA_LEVELS, .chapter_of = gea_chapter_of, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1,
    .tools = T_SIZE + GEA_SIZES,
};
REGISTER_GAME(gears_game)
