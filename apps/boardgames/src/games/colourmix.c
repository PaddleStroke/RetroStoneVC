/*
 * Colour Mixer in RetroStone Board, for one player: a painter's table of the house wood, paint tubes in a row along
 * its top (aluminium, a band of their colour, a cap of it), a white porcelain pot, the colour to match on a card in a
 * brass frame beside it, and a closeness meter under them (the rules: colourmix_rules.c). A tap on a tube squeezes it:
 * a drop falls into the pot, splashes, and swirls in until the pot shows the new mixture; the meter's gold fills
 * as the pot nears the card's colour.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A tube that holds few drops shows how many are left on a walnut badge; each tube shows the drops it has given on an
 * ivory one. The lamp: a hint (the drop to add, or the one to take back, played, the tube in gold light). Undo takes
 * back the last drop. The bar: the drops, and at its right end the par by its flag. Solved: gold light round the pot
 * and the card.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/colourmix.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define FALL_MS 340                             /* a drop falling into the pot */
#define SPLASH_MS 420
#define SWIRL_MS 800                            /* the new colour swirling in */
#define BACK_MS 320                             /* a drop taken back: the colour going back */
#define SQUEEZE_MS 240
#define WIN_MS 1500
#define DENY_MS 380
#define FLY 8
#define T_HINT 0
/* in units of the table */
#define TW 1.2f                                 /* a tube's width, height */
#define TH 3.4f
#define POT_R 3.05f
#define CARD 3.0f

static const uint8_t GOLD[3] = {250, 206, 96}, CREAM[3] = {246, 238, 220}, WALNUT[3] = {96, 62, 40},
                     BRASS[3] = {216, 172, 86}, INK[3] = {40, 28, 20};
static const float PORCELAIN[3] = {0.96f, 0.95f, 0.92f};

static cmx_game G;
static int serial, tool_down = -1, tube_down = -1;
static struct { int on, tube; Uint32 t0; float x0, y0, x1, y1; uint8_t rgb[3]; } fly[FLY];
static struct { float from[3], to[3]; Uint32 t0; int ms, swirl; } pot;
static struct { float from, to; Uint32 t0; } meter;
static struct { Uint32 t0; } squeeze[CMX_TUBES];
static struct { int on, tube; Uint32 t0; } deny, hintfx;
static struct { int on; Uint32 t0; } win;

static box last_box;
static float u, X0, Y0;                         /* a unit, the table's top left (pixels) */
static int made_u = -1, made_serial = -1, made_small = -1, made_light = -1;
static SDL_Texture *t_table, *t_tube[CMX_TUBES], *t_pot, *t_surface, *t_gloss, *t_swirl, *t_card, *t_frame, *t_drop,
                   *t_ring, *t_icon, *t_par;

/* ---- the layout's places: upright, the tubes in a row over the pot and the card, the meter under them; on a wide
 * screen, the tubes in rows of four at the right ------------------------------------------------------------------- */
static int wide;
static float TBW, TBH;                          /* the table, units */
static float tub_x[CMX_TUBES], tub_y[CMX_TUBES], slot;   /* each tube's top left (units from the table's) */
static float pot_ux, pot_uy, card_ux, card_uy, met_ux, met_uy, met_uw;

static void places(void)
{
    int n = G.m.n ? G.m.n : 1;
    if (!wide) {
        TBW = 10;
        TBH = 12;
        slot = fminf(10.0f / n, 1.9f);
        for (int t = 0; t < n; t++) {
            tub_x[t] = 5 + (t + 0.5f - n / 2.0f) * slot - TW / 2;
            tub_y[t] = 0.3f;
        }
        pot_ux = 3.35f;
        pot_uy = 0.3f + TH + 0.5f + POT_R;
        card_ux = 8.25f;
        card_uy = pot_uy;
        met_ux = 0.5f;
        met_uy = pot_uy + POT_R + 0.4f;
        met_uw = 9;
    } else {
        TBW = 18;
        TBH = 9;
        int cols = n < 4 ? n : 4, rows = (n + cols - 1) / cols;
        slot = fminf(6.6f / cols, 1.9f);
        float y0 = (TBH - rows * (TH + 0.4f) + 0.4f) / 2;
        for (int t = 0; t < n; t++) {
            int r = t / cols, c = t % cols, inrow = r == rows - 1 ? n - r * cols : cols;
            tub_x[t] = 14.4f + (c + 0.5f - inrow / 2.0f) * slot - TW / 2;
            tub_y[t] = y0 + r * (TH + 0.4f);
        }
        pot_ux = 7.15f;                         /* the card at the left, the pot next to the tubes */
        pot_uy = 3.95f;
        card_ux = 2.0f;
        card_uy = pot_uy;
        met_ux = 0.5f;
        met_uy = pot_uy + POT_R + 0.5f;
        met_uw = 9.6f;
    }
}

static float slot_w(void) { return slot; }
static void tube_box(int t, float *x, float *y, float *w, float *h)
{
    *w = TW * u;
    *h = TH * u;
    *x = X0 + tub_x[t] * u;
    *y = Y0 + tub_y[t] * u;
}
static void pot_xy(float *x, float *y) { *x = X0 + pot_ux * u; *y = Y0 + pot_uy * u; }
static void card_xy(float *x, float *y) { *x = X0 + card_ux * u; *y = Y0 + card_uy * u; }
static void meter_box(float *x, float *y, float *w, float *h)
{
    *x = X0 + met_ux * u;
    *y = Y0 + met_uy * u;
    *w = met_uw * u;
    *h = 0.7f * u;
}
/* ---- pictures ---------------------------------------------------------------------------------------------------- */
static void to_bytes(const float c[3], uint8_t o[3])
{
    for (int i = 0; i < 3; i++) o[i] = (uint8_t)clampf(255 * c[i] + 0.5f, 0, 255);
}

/* a paint tube standing, its nozzle down, w x h: a crimped end, an aluminium body round to the light, a band of its
 * colour, a shoulder, a cap of its colour */
static void paint_tube(uint8_t *o, int w, int h, const uint8_t col[3])
{
    memset(o, 0, (size_t)w * h * 4);
    static const uint8_t alu[3] = {196, 200, 206};
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float fy = (y + 0.5f) / h, fx = (x + 0.5f) / w, half;
            if (fy < 0.13f) half = 0.5f;                                /* the crimp */
            else if (fy < 0.80f) half = 0.5f - 0.07f * (fy - 0.13f) / 0.67f;   /* the body, a little narrower down */
            else if (fy < 0.87f) half = 0.43f - 0.25f * (fy - 0.80f) / 0.07f;  /* the shoulder */
            else half = 0.2f;                                            /* the cap */
            float d = (half - fabsf(fx - 0.5f)) * w, cov = clampf(d + 0.5f, 0, 1);
            if (fy > 0.985f) cov *= clampf((1 - fy) * h * 1.2f, 0, 1);
            if (fy < 0.01f) cov *= clampf(fy * h * 1.5f, 0, 1);
            if (cov <= 0) continue;
            float across = (fx - 0.5f) / half;                         /* -1 .. 1 round the tube */
            float lit = 0.78f + 0.32f * cosf(1.3f * (across + 0.35f)) - 0.12f * across * across;
            uint8_t c[3];
            const uint8_t *base = alu;
            float k = lit;
            if (fy < 0.13f) k *= 0.9f + 0.1f * (((int)(fy * h / fmaxf(1, 0.025f * h))) & 1);   /* the crimp's ridges */
            if (fy > 0.25f && fy < 0.70f) {                            /* the band of colour */
                base = col;
                float e = fminf(fy - 0.25f, 0.70f - fy) * h;
                if (e < 1.5f) k *= 0.85f;
            } else if (fy >= 0.87f) {                                  /* the cap */
                base = col;
                k *= 0.8f;
                if (fabsf(fy - 0.87f) * h < 1.2f) k *= 0.7f;
            }
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(base[i] * k, 0, 255);
            pzk_over(o + 4 * ((size_t)y * w + x), c, cov);
        }
}

/* the porcelain pot from above, D x D: a glazed rim with a blue line, the inner wall shaded (lit from the top left:
 * its far wall, the lower right, catches the light), a white bottom */
static void paint_pot(uint8_t *o, int D)
{
    float c = D / 2.0f, R = D / 2.0f - 2;
    static const uint8_t blue[3] = {60, 92, 160};
    memset(o, 0, (size_t)D * D * 4);
    for (int y = 0; y < D; y++)
        for (int x = 0; x < D; x++) {
            float dx = x + 0.5f - c, dy = y + 0.5f - c, rho = hypotf(dx, dy) / R;
            float cov = clampf((1 - rho) * R + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float nd = rho > 0.01f ? -(dx + dy) / (1.4142f * rho * R) : 0, k;
            uint8_t col[3] = {246, 244, 238};
            if (rho > 0.86f) k = 1 + 0.08f * nd - 0.25f * (rho - 0.93f) * (rho - 0.93f) * 40;   /* the rim, round */
            else if (rho > 0.6f) k = 0.86f - 0.16f * nd + 0.06f * (0.86f - rho);               /* the inner wall */
            else k = 0.92f;
            if (fabsf(rho - 0.95f) < 0.012f) memcpy(col, blue, 3);
            uint8_t cc[3];
            for (int i = 0; i < 3; i++) cc[i] = (uint8_t)clampf(col[i] * k, 0, 255);
            pzk_over(o + 4 * ((size_t)y * D + x), cc, cov);
        }
}

/* the paint's surface: a white disc to be tinted (its edge a little darker where it meets the wall), D x D */
static void paint_surface(uint8_t *o, int D)
{
    float c = D / 2.0f, R = D / 2.0f - 1;
    for (int y = 0; y < D; y++)
        for (int x = 0; x < D; x++) {
            float dx = x + 0.5f - c, dy = y + 0.5f - c, rho = hypotf(dx, dy);
            uint8_t *p = o + 4 * ((size_t)y * D + x);
            float k = 1 - 0.18f * powf(clampf(rho / R, 0, 1), 6) + 0.02f * art_noise(x, y);
            p[0] = p[1] = p[2] = (uint8_t)clampf(255 * k, 0, 255);
            p[3] = (uint8_t)(255 * clampf(R - rho + 0.5f, 0, 1));
        }
}

/* the light on the paint: a soft white gleam towards the top left, a shadow along the wall there, D x D */
static void paint_gloss(uint8_t *o, int D)
{
    float c = D / 2.0f, R = D / 2.0f - 1;
    for (int y = 0; y < D; y++)
        for (int x = 0; x < D; x++) {
            float dx = (x + 0.5f - c) / R, dy = (y + 0.5f - c) / R, rho = hypotf(dx, dy);
            uint8_t *p = o + 4 * ((size_t)y * D + x);
            float in = clampf((1 - rho) * R + 0.5f, 0, 1);
            float g = expf(-((dx + 0.38f) * (dx + 0.38f) * 5 + (dy + 0.42f) * (dy + 0.42f) * 9)) * 0.55f;
            float sh = clampf((rho - 0.82f) / 0.18f, 0, 1) * clampf(-(dx + dy) / 1.4f, 0, 1) * 0.35f;
            if (g >= sh) { p[0] = p[1] = p[2] = 255; p[3] = (uint8_t)(255 * g * in); }
            else { p[0] = p[1] = p[2] = 0; p[3] = (uint8_t)(255 * sh * in); }
        }
}

/* the swirl: three arms winding in (white, faded at their edges), D x D */
static void paint_swirl(uint8_t *o, int D)
{
    float c = D / 2.0f, R = D / 2.0f - 1;
    for (int y = 0; y < D; y++)
        for (int x = 0; x < D; x++) {
            float dx = x + 0.5f - c, dy = y + 0.5f - c, rho = hypotf(dx, dy) / R, th = atan2f(dy, dx);
            float arm = 0.5f + 0.5f * cosf(3 * (th + 4.2f * rho));
            float a = clampf((arm - 0.35f) / 0.4f, 0, 1) * clampf((1 - rho) * R + 0.5f, 0, 1) * (0.4f + 0.6f * rho);
            uint8_t *p = o + 4 * ((size_t)y * D + x);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(255 * a);
        }
}

/* the card: paper brushed with paint (white, tinted when drawn), S x S */
static void paint_card(uint8_t *o, int S)
{
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            uint8_t *p = o + 4 * ((size_t)y * S + x);
            float k = 0.97f + 0.035f * sinf(y * 0.9f + 2.5f * art_noise(x / 6, y / 2)) + 0.02f * art_noise(x, y * 3);
            p[0] = p[1] = p[2] = (uint8_t)clampf(255 * k, 0, 255);
            p[3] = 255;
        }
}

/* the card's brass frame, F x F, its opening `in` pixels from the edge */
static void paint_frame(uint8_t *o, int F, float in)
{
    memset(o, 0, (size_t)F * F * 4);
    for (int y = 0; y < F; y++)
        for (int x = 0; x < F; x++) {
            float px = x + 0.5f, py = y + 0.5f, d = fminf(fminf(px, F - px), fminf(py, F - py));
            if (d > in + 1) continue;
            float cov = clampf(in - d + 0.5f, 0, 1), t = d / in;
            float k = 0.85f + 0.3f * sinf(PUZ_PI * t) + (px + py < F ? 0.08f : -0.08f) * (t < 0.5f ? 1 : -1);
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(BRASS[i] * k, 0, 255);
            pzk_over(o + 4 * ((size_t)y * F + x), c, cov);
        }
}

/* a drop of paint, falling (white, tinted): a ball drawn out upward a little, D x D */
static void paint_drop(uint8_t *o, int D)
{
    memset(o, 0, (size_t)D * D * 4);
    float c = D / 2.0f;
    for (int y = 0; y < D; y++)
        for (int x = 0; x < D; x++) {
            float dx = (x + 0.5f - c) / (0.36f * D), dy = (y + 0.5f - c - 0.12f * D) / (0.36f * D);
            if (dy < 0) dx /= 1 + 0.9f * dy * dy * 0 + (-dy) * 0.55f;   /* narrower up top */
            float r = hypotf(dx, dy * (dy < 0 ? 0.62f : 1)), cov = clampf((1 - r) * 0.36f * D + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float k = 0.86f + 0.3f * clampf(-(dx + dy) * 0.6f + 0.2f, -0.5f, 1);
            uint8_t g = (uint8_t)clampf(255 * k, 0, 255), *p = o + 4 * ((size_t)y * D + x);
            p[0] = p[1] = p[2] = g;
            p[3] = (uint8_t)(255 * cov);
        }
    pzk_ball(o, D, D, 0.4f * D, 0.55f * D, 0.07f * D, CREAM, 0.8f, 0);
}

static void tex(SDL_Texture **t, int w, int h, void (*paint)(uint8_t *, int))
{
    ui_free(t);
    uint8_t *b = ui_buf(w, h);
    memset(b, 0, (size_t)w * h * 4);
    paint(b, w);
    *t = ui_tex(w, h, b);
}

/* ---- the layout ---------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    last_box = b;
    int was = wide;
    wide = b.w > 1.35f * b.h;
    if (was != wide) made_u = -1;
    places();
    u = floorf(fminf(b.w / TBW, b.h / TBH));
    X0 = floorf(b.x + (b.w - TBW * u) / 2);
    Y0 = floorf(b.y + (b.h - TBH * u) / 2);
    int ui_u = (int)u, light = art_frame_is_light();
    if (made_u != ui_u || made_light != light) {
        int f = (int)(0.3f * u), W = (int)(TBW * u), H = (int)(TBH * u);
        uint8_t *o = malloc((size_t)(W + 2 * f) * (H + 2 * f) * 4);
        ui_free(&t_table);
        if (o) {
            art_plank(W, H, f, o);
            t_table = ui_tex(W + 2 * f, H + 2 * f, o);
        }
        free(o);
    }
    if (made_u != ui_u) {
        int D = (int)(2 * POT_R * u);
        tex(&t_pot, D, D, paint_pot);
        int S = (int)(2 * 0.84f * POT_R * u);
        tex(&t_surface, S, S, paint_surface);
        tex(&t_gloss, S, S, paint_gloss);
        tex(&t_swirl, S, S, paint_swirl);
        int C = (int)(CARD * u);
        tex(&t_card, C, C, paint_card);
        ui_free(&t_frame);
        paint_frame(ui_buf(C, C), C, 0.16f * u);
        t_frame = ui_tex(C, C, ui_buf(C, C));
        int dd = (int)fmaxf(6, 0.55f * u);
        tex(&t_drop, dd, dd, paint_drop);
        int rr = (int)fmaxf(8, 2 * u);
        ui_free(&t_ring);
        art_ring(rr, 0.82f, CREAM, 1, ui_buf(rr, rr));
        t_ring = ui_tex(rr, rr, ui_buf(rr, rr));
    }
    if (made_u != ui_u || made_serial != serial) {
        int w = (int)(TW * u), h = (int)(TH * u);
        for (int t = 0; t < CMX_TUBES; t++) {
            ui_free(&t_tube[t]);
            if (t >= G.m.n) continue;
            float c[3];
            uint8_t col[3];
            cmx_paint_rgb(G.m.paint[t], c);
            to_bytes(c, col);
            paint_tube(ui_buf(w, h), w, h, col);
            t_tube[t] = ui_tex(w, h, ui_buf(w, h));
        }
    }
    made_u = ui_u;
    made_serial = serial;
    made_light = light;
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_par);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
        ui_free(&t_icon);                       /* a red drop */
        uint8_t *b = ui_buf(small, small);
        paint_drop(b, small);
        static const uint8_t red[3] = {214, 50, 40};
        for (int i = 0; i < small * small; i++)
            for (int k = 0; k < 3; k++) b[4 * i + k] = (uint8_t)(b[4 * i + k] * red[k] / 255);
        t_icon = ui_tex(small, small, b);
    }
}

/* ---- the position --------------------------------------------------------------------------------------------------- */
static void pot_now(float c[3])                 /* the colour in the pot (porcelain when empty) */
{
    if (!cmx_pot(&G.m, G.s.add, c)) memcpy(c, PORCELAIN, sizeof PORCELAIN);
}

static float closeness(float de) { return 1 / (1 + powf(de / 14, 1.4f)); }

static float meter_now(Uint32 now)
{
    float k = puz_ease((float)(int)(now - meter.t0) / 450);
    return meter.from + (meter.to - meter.from) * k;
}

static void shown(Uint32 now, float c[3])      /* the pot's colour as shown now (mid-swirl: blended) */
{
    float k = puz_ease((float)(int)(now - pot.t0) / pot.ms);
    for (int i = 0; i < 3; i++) c[i] = pot.from[i] + (pot.to[i] - pot.from[i]) * k;
}

static void calm(void)
{
    memset(fly, 0, sizeof fly);
    memset(squeeze, 0, sizeof squeeze);
    deny.on = hintfx.on = win.on = 0;
    tool_down = tube_down = -1;
    pot_now(pot.to);
    memcpy(pot.from, pot.to, sizeof pot.from);
    pot.t0 = 0;
    pot.ms = 1;
    meter.from = meter.to = closeness(cmx_dist(&G.m, G.s.add));
    meter.t0 = 0;
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!cmx_level(&G, level)) cmx_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    cmx_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int solved(void) { return cmx_solved(&G.m, &G.s); }
static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return cmx_count(&G.s); }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(score(), par()); }

static int busy(void)
{
    Uint32 now = ui_ms();
    for (int i = 0; i < FLY; i++)
        if (fly[i].on && now - fly[i].t0 < FALL_MS + SPLASH_MS) return 1;
    return (int)(now - pot.t0) < pot.ms || (win.on && now - win.t0 < WIN_MS);
}

/* the pot to change to its colour now: after `delay` ms, swirling in (or fading back) */
static void retint(int delay, int swirl)
{
    Uint32 now = ui_ms();
    shown(now, pot.from);
    pot_now(pot.to);
    pot.t0 = now + (Uint32)delay;
    pot.ms = swirl ? SWIRL_MS : BACK_MS;
    pot.swirl = swirl;
    meter.from = meter_now(now);
    meter.to = closeness(cmx_dist(&G.m, G.s.add));
    meter.t0 = now + (Uint32)delay;
    if (solved() && !win.on) {
        win.on = 1;
        win.t0 = now + (Uint32)delay + (swirl ? SWIRL_MS / 2 : 0);
        ui_sound_later(SND_END, delay + (swirl ? SWIRL_MS / 2 : 0));
    }
    if (!solved()) win.on = 0;
    ui_animate();
}

static void add(int t)
{
    if (!cmx_drop(&G, t)) {
        deny.on = 1;
        deny.tube = t;
        deny.t0 = ui_ms();
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    Uint32 now = ui_ms();
    int k = 0;
    for (int i = 0; i < FLY; i++)
        if (!fly[i].on || now - fly[i].t0 >= FALL_MS + SPLASH_MS) { k = i; break; }
    float x, y, w, h, px, py, c[3];
    tube_box(t, &x, &y, &w, &h);
    pot_xy(&px, &py);
    fly[k].on = 1;
    fly[k].tube = t;
    fly[k].t0 = now;
    fly[k].x0 = x + w / 2;
    fly[k].y0 = y + h;
    fly[k].x1 = px + (art_hash(G.s.n, 3) - 0.5f) * 1.2f * u;
    fly[k].y1 = py + (art_hash(G.s.n, 7) - 0.5f) * 1.0f * u;
    cmx_paint_rgb(G.m.paint[t], c);
    to_bytes(c, fly[k].rgb);
    squeeze[t].t0 = now;
    ui_sound_soft(SND_TAP, 0.5f);
    ui_sound_later(SND_DROP, FALL_MS);
    retint(FALL_MS, 1);
}

static void hint(void)
{
    int t, take;
    if (solved() || !cmx_hint(&G.m, &G.s, &t, &take)) { ui_sound(SND_DENY); return; }
    hintfx.on = 1;
    hintfx.tube = t;
    hintfx.t0 = ui_ms();
    if (!take) { add(t); return; }
    cmx_take(&G, t);
    ui_sound(SND_TAP);
    retint(0, 0);
}

static int undo(void)
{
    int r = cmx_undo(&G);
    if (r) {
        memset(fly, 0, sizeof fly);
        retint(0, 0);
    }
    return r;
}

/* the moves played as taps would play them (a drop falls, the pot swirls): the last one's still on its way after */
static int script(const char *moves)
{
    for (const char *s = moves; s && *s; s++) {
        if (*s == ' ') continue;
        if (*s == '-') { if (!undo()) return 0; continue; }
        if (*s == 'h' || *s == '=') {
            for (int k = 0; k < (*s == '=' ? 40 : 1) && !solved(); k++) hint();
            continue;
        }
        int t = *s - 'a', n = G.s.n;
        if (t < 0 || t >= G.m.n) return 0;
        add(t);
        if (G.s.n == n) return 0;
    }
    return 1;
}

/* ---- drawing ------------------------------------------------------------------------------------------------------- */
static void put_tint(SDL_Texture *t, float cx, float cy, float w, float h, const float c[3], Uint8 a)
{
    uint8_t b[3];
    to_bytes(c, b);
    SDL_SetTextureColorMod(t, b[0], b[1], b[2]);
    SDL_SetTextureAlphaMod(t, a);
    ui_put_c(t, cx, cy, w, h);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

static void badge(float cx, float cy, float d, int n, int dark)
{
    pzk_dot(cx + 0.04f * d, cy + 0.06f * d, d, 1, (pzk_rgba){0, 0, 0, 70}, 0);
    if (dark) pzk_dot(cx, cy, d, 1, (pzk_rgba){WALNUT[0], WALNUT[1], WALNUT[2], 255}, 0);
    else pzk_dot(cx, cy, d, 1, (pzk_rgba){240, 230, 208, 255}, 0);
    ui_number_ex(cx, cy, 0.62f * d, n, dark ? CREAM : INK, 255, UI_NUM_TIGHT);
}

static void draw_tubes(Uint32 now)
{
    for (int t = 0; t < G.m.n; t++) {
        float x, y, w, h;
        tube_box(t, &x, &y, &w, &h);
        float k = (float)(int)(now - squeeze[t].t0) / SQUEEZE_MS, sx = 1, sy = 1, dx = 0;
        if (squeeze[t].t0 && k >= 0 && k < 1) {
            float e = sinf(PUZ_PI * k);
            sx = 1 + 0.07f * e;
            sy = 1 - 0.04f * e;
            ui_animate();
        }
        if (deny.on && deny.tube == t) {
            float f = (now - deny.t0) / (float)DENY_MS;
            if (f >= 1) deny.on = 0;
            else { dx = 0.06f * u * sinf(f * 30) * (1 - f); ui_animate(); }
        }
        int left = cmx_left(&G.m, &G.s, t);
        if (hintfx.on && hintfx.tube == t) {
            float f = (now - hintfx.t0) / 1000.0f;
            if (f >= 1) hintfx.on = 0;
            else { ui_mark_gold_a(x + w / 2, y + h / 2, 2.2f * w, puz_pulse(f, 0, 1)); ui_animate(); }
        }
        ui_shadow_flat(x + w / 2 + 0.1f * u + dx, y + h / 2 + 0.12f * u, w * 0.95f, h * 0.97f, 0.35f);
        SDL_SetTextureAlphaMod(t_tube[t], left ? 255 : 110);
        ui_put_c(t_tube[t], x + w / 2 + dx, y + h * (1 - sy / 2), w * sx, h * sy);
        SDL_SetTextureAlphaMod(t_tube[t], 255);
        if (G.m.limit[t] != CMX_ANY) badge(x + w / 2 + dx, y + 0.07f * h, 0.62f * w, left, 1);
        if (G.s.add[t]) badge(x + w * 0.82f + dx, y + 0.62f * h, 0.5f * w, G.s.add[t], 0);
    }
}

static void draw_pot(Uint32 now)
{
    float px, py;
    pot_xy(&px, &py);
    float D = 2 * POT_R * u, S = 2 * 0.84f * POT_R * u;
    if (win.on) {
        float f = (float)(int)(now - win.t0);
        if (f >= 0) ui_mark_gold_a(px, py, 1.25f * D, f < WIN_MS ? 0.4f + 0.6f * puz_pulse(f, 0, WIN_MS) : 0.4f);
        ui_animate();
    }
    ui_shadow_flat(px + 0.15f * u, py + 0.22f * u, D, D, 0.45f);
    ui_put_c(t_pot, px, py, D, D);
    /* the paint: the colour it had, the new one swirling in (or fading in, going back) */
    float k = (float)(int)(now - pot.t0) / pot.ms;
    if (k < 1) ui_animate();
    if (k <= 0) put_tint(t_surface, px, py, S, S, pot.from, 255);
    else if (k >= 1) put_tint(t_surface, px, py, S, S, pot.to, 255);
    else {
        put_tint(t_surface, px, py, S, S, pot.from, 255);
        put_tint(t_surface, px, py, S, S, pot.to, (Uint8)(255 * puz_ease(clampf(k * 1.25f - 0.25f, 0, 1))));
        if (pot.swirl) {
            uint8_t b[3];
            to_bytes(pot.to, b);
            SDL_SetTextureColorMod(t_swirl, b[0], b[1], b[2]);
            SDL_SetTextureAlphaMod(t_swirl, (Uint8)(255 * sinf(PUZ_PI * fminf(1, k * 1.15f))));
            float sc = 0.4f + 0.6f * puz_ease(k);
            ui_put_rot(t_swirl, px, py, S * sc, S * sc, 220 * k + 30);
            SDL_SetTextureColorMod(t_swirl, 255, 255, 255);
            SDL_SetTextureAlphaMod(t_swirl, 255);
        }
    }
    ui_put_c(t_gloss, px, py, S, S);
}

static void draw_card(Uint32 now)
{
    float cx, cy, C = CARD * u;
    card_xy(&cx, &cy);
    if (win.on && (int)(now - win.t0) >= 0) ui_mark_gold_a(cx, cy, 1.5f * C, 0.4f + 0.6f * puz_pulse((float)(int)(now - win.t0), 0, WIN_MS));
    ui_shadow_flat(cx + 0.12f * u, cy + 0.18f * u, C, C, 0.4f);
    put_tint(t_card, cx, cy, C - 0.3f * u, C - 0.3f * u, G.m.target, 255);
    ui_put_c(t_frame, cx, cy, C, C);
}

static void draw_meter(Uint32 now)
{
    float x, y, w, h;
    meter_box(&x, &y, &w, &h);
    float f = meter_now(now), r = h / 2, tol = closeness(CMX_TOL);
    if ((int)(now - meter.t0) < 450) ui_animate();
    pzk_line(x + r, y + r + 0.05f * u, x + w - r, y + r + 0.05f * u, h, 1.5f, (pzk_rgba){0, 0, 0, 60}, 0);
    pzk_line(x + r, y + r, x + w - r, y + r, h, 1, (pzk_rgba){52, 34, 22, 255}, 0);
    pzk_dot(x + r, y + r, h, 1, (pzk_rgba){52, 34, 22, 255}, 0);
    pzk_dot(x + w - r, y + r, h, 1, (pzk_rgba){52, 34, 22, 255}, 0);
    float in = 0.72f * h, fx = x + r + (w - 2 * r) * clampf(f, 0, 1);
    int full = f >= tol - 0.002f;
    pzk_rgba fill = full ? (pzk_rgba){GOLD[0], GOLD[1], GOLD[2], 255}
                         : (pzk_rgba){(Uint8)(150 + 80 * f), (Uint8)(120 + 70 * f), (Uint8)(70 + 10 * f), 255};
    pzk_dot(x + r, y + r, in, 1, fill, 0);
    if (fx > x + r + 0.5f) {
        pzk_line(x + r, y + r, fx, y + r, in, 1, fill, 0);
        pzk_dot(fx, y + r, in, 1, fill, 0);
    }
    float tx = x + r + (w - 2 * r) * tol;      /* the mark past which it is a match */
    pzk_line(tx, y - 0.1f * u, tx, y + h + 0.1f * u, fmaxf(2, 0.07f * u), 1, (pzk_rgba){GOLD[0], GOLD[1], GOLD[2], 255}, 0);
    if (full) ui_mark_gold_a(fx, y + r, 1.4f * h, 0.6f);
}

static void draw_drops(Uint32 now)
{
    for (int i = 0; i < FLY; i++) {
        if (!fly[i].on) continue;
        float t = (float)(int)(now - fly[i].t0);
        if (t >= FALL_MS + SPLASH_MS) { fly[i].on = 0; continue; }
        ui_animate();
        float c[3] = {fly[i].rgb[0] / 255.0f, fly[i].rgb[1] / 255.0f, fly[i].rgb[2] / 255.0f};
        if (t < FALL_MS) {                      /* falling: faster as it goes */
            float f = t / FALL_MS, e = f * f;
            float x = fly[i].x0 + (fly[i].x1 - fly[i].x0) * f, y = fly[i].y0 + (fly[i].y1 - fly[i].y0) * e;
            float d = 0.42f * u * (0.6f + 0.4f * fminf(1, f * 4));
            put_tint(t_drop, x, y, d, d * (1 + 0.25f * e), c, 255);
        } else {                                /* the splash: a ring spreading, droplets thrown up */
            float f = (t - FALL_MS) / SPLASH_MS;
            float d = (0.4f + 1.6f * puz_ease(f)) * u;
            put_tint(t_ring, fly[i].x1, fly[i].y1, d, d * 0.92f, c, (Uint8)(200 * (1 - f)));
            for (int k = 0; k < 6; k++) {
                float a = k * PUZ_PI / 3 + art_hash(i, k), rr = (0.25f + 0.75f * f) * 0.9f * u;
                float hop = 0.5f * u * sinf(PUZ_PI * f);
                put_tint(t_drop, fly[i].x1 + rr * cosf(a), fly[i].y1 + 0.6f * rr * sinf(a) - hop, 0.14f * u,
                         0.14f * u, c, (Uint8)(255 * (1 - f)));
            }
        }
    }
}

static void draw(void)
{
    if (made_u < 0 || made_serial != serial || made_light != art_frame_is_light()) layout(last_box);
    Uint32 now = ui_ms();
    int f = (int)(0.3f * u);
    ui_put(t_table, X0 - f, Y0 - f, TBW * u + 2 * f, TBH * u + 2 * f);
    draw_card(now);
    draw_pot(now);
    draw_meter(now);
    draw_tubes(now);
    draw_drops(now);
    ui_tool(T_HINT, ICON_HINT, (solved() ? UI_DIM : 0) | (tool_down == T_HINT ? UI_ON : 0));
}

/* ---- the pointer ---------------------------------------------------------------------------------------------------- */
static int tube_at(float px, float py)
{
    for (int t = 0; t < G.m.n; t++) {
        float x, y, w, h, sw = slot_w() * u;
        tube_box(t, &x, &y, &w, &h);
        if (px >= x + w / 2 - sw / 2 && px < x + w / 2 + sw / 2 && py >= y - 0.3f * u && py < y + h + 0.4f * u) return t;
    }
    return -1;
}

static void press(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    tube_down = tool_down >= 0 || solved() ? -1 : tube_at(x, y);
    ui_animate();
}

static void motion(float x, float y) { (void)x; (void)y; }

static void release(float x, float y)
{
    if (tool_down >= 0) {
        int k = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) == k && k == T_HINT) hint();
        return;
    }
    int t = tube_down;
    tube_down = -1;
    if (t >= 0 && tube_at(x, y) == t && !solved()) add(t);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, t_icon, score(), t_par, par(), solved() && score() <= par());
}

/* ---- the shell's pictures ------------------------------------------------------------------------------------------ */
static void blob(uint8_t *o, int size, float cx, float cy, float r, const float c[3])
{
    uint8_t b[3];
    static const uint8_t black[3] = {0, 0, 0};
    to_bytes(c, b);
    pzk_ball(o, size, size, cx + 0.06f * r, cy + 0.1f * r, r, black, 0.3f, 0);
    pzk_ball(o, size, size, cx, cy, r, b, 1, 0.55f);
}

static void pot_pic(uint8_t *o, int size, float cx, float cy, float r, const float c[3])
{
    int D = (int)(2 * r);
    uint8_t *p = malloc((size_t)D * D * 4);
    if (!p) return;
    paint_pot(p, D);
    static const uint8_t black[3] = {0, 0, 0};
    pzk_ball(o, size, size, cx + 0.05f * r, cy + 0.08f * r, r, black, 0.35f, 0);
    art_blend(o, size, size, p, D, D, (int)(cx - r), (int)(cy - r));
    free(p);
    uint8_t b[3];
    to_bytes(c, b);
    pzk_ball(o, size, size, cx, cy, 0.84f * r, b, 1, 0.25f);
}

static void tube_pic(uint8_t *o, int size, float cx, float cy, float h, int paint)
{
    int w = (int)(h * TW / TH), H = (int)h;
    uint8_t *p = malloc((size_t)w * H * 4);
    if (!p || w < 2) { free(p); return; }
    float c[3];
    uint8_t b[3];
    cmx_paint_rgb(paint, c);
    to_bytes(c, b);
    paint_tube(p, w, H, b);
    art_blend(o, size, size, p, w, H, (int)(cx - w / 2.0f), (int)(cy - H / 2.0f));
    free(p);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    float c[3];
    memset(rgba, 0, (size_t)size * size * 4);
    cmx_paint_rgb(CMX_ORANGE, c);
    pot_pic(rgba, size, size / 2.0f, size / 2.0f, 0.46f * size, c);
}

/* the menu's card: tubes of red and yellow over a pot of orange, a drop falling; the picker's: a pot and its tubes */
static void thumb(int variant, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    float c[3], s = (float)size;
    if (variant < 0) {
        cmx_paint_rgb(CMX_ORANGE, c);
        pot_pic(rgba, size, 0.5f * s, 0.64f * s, 0.34f * s, c);
        tube_pic(rgba, size, 0.24f * s, 0.24f * s, 0.42f * s, CMX_RED);
        tube_pic(rgba, size, 0.5f * s, 0.2f * s, 0.42f * s, CMX_YELLOW);
        tube_pic(rgba, size, 0.76f * s, 0.24f * s, 0.42f * s, CMX_BLUE);
        cmx_paint_rgb(CMX_YELLOW, c);
        blob(rgba, size, 0.5f * s, 0.47f * s, 0.035f * s, c);
        return;
    }
    static const int P[CMX_VARIANTS][7] = {{CMX_RED, CMX_YELLOW, CMX_BLUE, -1}, {CMX_RED, CMX_YELLOW, CMX_BLUE, CMX_WHITE, -1},
                                           {CMX_RED, CMX_YELLOW, CMX_BLUE, CMX_WHITE, CMX_BLACK, CMX_GREEN, -1}};
    int n = 0;
    while (n < 7 && P[variant][n] >= 0) n++;
    cmx_paint_rgb(variant == 0 ? CMX_ORANGE : variant == 1 ? CMX_VIOLET : CMX_BROWN, c);
    pot_pic(rgba, size, 0.5f * s, 0.66f * s, 0.3f * s, c);
    for (int i = 0; i < n; i++)
        tube_pic(rgba, size, (0.5f + (i - (n - 1) / 2.0f) * 0.86f / n) * s, 0.2f * s, 0.34f * s, P[variant][i]);
}

/* a level's tile: the colour to make, a dab of paint */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    cmx_game *t = malloc(sizeof *t);
    memset(rgba, 0, (size_t)size * size * 4);
    if (t && cmx_level(t, level)) blob(rgba, size, 0.5f * size, 0.52f * size, 0.3f * size, t->m.target);
    free(t);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const int P[CMX_CHAPTERS][3] = {{CMX_RED, CMX_YELLOW, -1}, {CMX_RED, CMX_YELLOW, CMX_BLUE},
                                           {CMX_RED, CMX_WHITE, CMX_BLACK}, {-1, -1, -1},
                                           {CMX_ORANGE, CMX_GREEN, CMX_VIOLET}, {CMX_BLUE, -1, -1}};
    float c[3], s = (float)size;
    memset(rgba, 0, (size_t)size * size * 4);
    if (chapter == 3) {                         /* a pot holding paint already */
        cmx_paint_rgb(CMX_GREEN, c);
        pot_pic(rgba, size, 0.5f * s, 0.5f * s, 0.42f * s, c);
        return;
    }
    if (chapter == 5) {                         /* a tube with few drops: its badge */
        tube_pic(rgba, size, 0.5f * s, 0.5f * s, 0.9f * s, CMX_BLUE);
        pzk_ball(rgba, size, size, 0.5f * s, 0.16f * s, 0.15f * s, WALNUT, 1, 0.3f);
        puz_digits(rgba, size, size, 2, 0.5f * s, 0.16f * s, 0.2f * s, CREAM, 1, 1);
        return;
    }
    int n = P[chapter][2] < 0 ? 2 : 3;
    for (int i = 0; i < n; i++) {
        cmx_paint_rgb(P[chapter][i], c);
        float a = -PUZ_PI / 2 + 2 * PUZ_PI * i / n;
        blob(rgba, size, (0.5f + 0.22f * cosf(a)) * s, (0.52f + 0.22f * sinf(a)) * s, 0.2f * s, c);
    }
}

static const game colourmix_game = {
    .id = "colourmix", .order = 497, .variants = CMX_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = CMX_LEVELS, .chapters = CMX_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = 1,
};
REGISTER_GAME(colourmix_game)
