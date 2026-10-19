/*
 * Tilt Maze in RetroStone Board, for one player: a tray of green felt in the house's frame, walled into a maze by
 * walnut rails (and walnut blocks), the holes dark wells ringed with their colour in a brass rim, the marbles glass
 * balls of red lacquer, ivory, lapis and amber with a swirl inside that turns as they roll. Gates are panes of coloured
 * glass, one-way tiles brass arrows set in the felt, lids brass discs with a jewel of their colour (the rules:
 * tiltmaze_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Swipe on the tray, tap one of the four arrows (the shell's tools), tap the tray (the way from its middle to the
 * finger), or use the arrow keys: the tray tips gently that way, the marbles roll, speeding up, and knock against what
 * stops them; it settles back level. A marble dropped into a hole of another colour glows red: undo. The lamp: the
 * hint (the next tilt of a shortest way, its arrow in gold light and the squares the marbles would reach ringed; from a
 * dead end, the hint first takes back the tilts that led there). The bar: the tilts, and at its right end the par by a
 * flag. Solved: the holes flare gold; at par the light runs out over the tray.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/swipepad.h"
#include "games/tiltmaze.h"
#include "shell.h"

#define FRAME 0.035f
#define TIP_MS 110                              /* the tray tipping */
#define SETTLE_MS 200                           /* and back level */
#define ROLL0_MS 60                             /* a roll: this, and so much a square */
#define ROLL_MS 52
#define KNOCK_MS 140
#define SINK_MS 220
#define LID_MS 320
#define FLARE_MS 900
#define WAVE_STEP 50
#define WAVE_LEN 520
#define TIP 0.035f                              /* how far the tray tips: its low side this much smaller */
#define BALL 0.78f                              /* a marble, of a square */

static const int COL[TLM_COLOURS] = {2, 0, 5, 4};   /* red lacquer, ivory, lapis, amber */
static const uint8_t BRASS[3] = {214, 170, 84}, WALNUT[3] = {96, 60, 36}, WALNUT_LIT[3] = {156, 108, 68};
static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

static tlm_game G;
static struct { int on, dir; tlm_state from; tlm_trace tr; Uint32 t0; } mv;
static struct { int on; Uint32 t0; } win;
static struct { int on, dir; Uint32 t0; } nudge;    /* a tilt that moves nothing: the tray tips a little, back */
static Uint32 lid_t0[TLM_CELLS];
static float spin[TLM_MAXM];                    /* each marble's swirl, degrees */
static int hint_way = -1, down = -1, serial;
static swp_swipe sw;

static box last_box;
static float ox, oy;                            /* the felt's top left */
static int fr, cell, bw, bh, made_cell = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_board, *t_ball[TLM_COLOURS], *t_shine, *t_lid[TLM_COLOURS], *t_par;

static void colour_of(int k, uint8_t rgb[3])
{
    const float *f = art_player[COL[k & 3]].fill;
    for (int i = 0; i < 3; i++) rgb[i] = (uint8_t)f[i];
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a hole: a dark well in the felt, a ring of its colour, a thin brass rim. Small (the menu's card, the level tiles):
 * the ring kept two pixels wide, the well tinted with its colour, so the colour still shows */
static void paint_hole(uint8_t *o, int W, int H, float cx, float cy, int sq, int k)
{
    float rt = fmaxf(0.075f * sq, fminf(2.2f, 0.2f * sq)), rim = fmaxf(0.03f * sq, 0.8f);
    float r = 0.44f * sq - rt - rim, b = 0.1f * sq, tint = clampf((48.0f - sq) / 48.0f, 0, 0.55f);
    uint8_t col[3];
    colour_of(k, col);
    for (int y = (int)(cy - r - 1); y <= cy + r + 1; y++)
        for (int x = (int)(cx - r - 1); x <= cx + r + 1; x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = hypotf(dx, dy);
            if (d > r + 0.5f) continue;
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float f = art_recess(dx, dy, r, b, 0.12f, 0.3f, 0.2f), a = clampf(r + 0.5f - d, 0, 1);
            for (int i = 0; i < 3; i++) {
                float well = p[i] * f * 0.6f;
                well += (col[i] * 0.8f - well) * tint;
                p[i] = (uint8_t)clampf(p[i] + (well - p[i]) * a, 0, 255);
            }
        }
    pzk_arc(o, W, H, cx, cy, r, r + rt, 0, 360, col, 1);
    pzk_arc(o, W, H, cx, cy, r + rt, r + rt + rim, 0, 360, BRASS, 1);
    pzk_arc(o, W, H, cx, cy, r - 0.02f * sq, r, 0, 360, PUZ_WHITE, 0.12f);
}

/* a gate: a pane of coloured glass in a brass frame */
static void paint_gate(uint8_t *o, int W, int H, float x0, float y0, int sq, int k)
{
    uint8_t col[3];
    colour_of(k, col);
    static const uint8_t smoke[3] = {34, 28, 24};
    float in = 0.08f * sq, s = sq - 2 * in;
    art_roundrect_put(o, W, H, (int)(x0 + in), (int)(y0 + in), (int)s, (int)s, 0.12f * sq, smoke, 0.4f);
    art_roundrect_put(o, W, H, (int)(x0 + in), (int)(y0 + in), (int)s, (int)s, 0.12f * sq, col, 0.3f);
    for (int i = 0; i < 3; i++) {               /* its bars, of its colour, inked */
        float x = x0 + in + s * (0.25f + 0.25f * i), ya = y0 + in + 0.06f * sq, yb = y0 + sq - in - 0.06f * sq;
        pzk_stroke(o, W, H, x, ya, x, yb, 0.06f * sq, smoke, 0.8f);
        pzk_stroke(o, W, H, x, ya, x, yb, 0.042f * sq, col, 1);
    }
    pzk_stroke(o, W, H, x0 + in + 0.1f * s, y0 + in + 0.35f * s, x0 + in + 0.35f * s, y0 + in + 0.1f * s,
               0.03f * sq, PUZ_WHITE, 0.4f);   /* a glint */
    float e = 0.03f * sq;
    pzk_stroke(o, W, H, x0 + in, y0 + in, x0 + sq - in, y0 + in, e, BRASS, 1);
    pzk_stroke(o, W, H, x0 + in, y0 + sq - in, x0 + sq - in, y0 + sq - in, e, BRASS, 1);
    pzk_stroke(o, W, H, x0 + in, y0 + in, x0 + in, y0 + sq - in, e, BRASS, 1);
    pzk_stroke(o, W, H, x0 + sq - in, y0 + in, x0 + sq - in, y0 + sq - in, e, BRASS, 1);
}

/* a one-way tile: a brass arrow set in the felt, pointing its way */
static void paint_arrow(uint8_t *o, int W, int H, float cx, float cy, int sq, int dir)
{
    static const uint8_t dark[3] = {40, 30, 20};
    float dx = DX[dir], dy = DY[dir], px = -dy, py = dx, l = 0.3f * sq, hw = 0.2f * sq;
    for (int pass = 0; pass < 2; pass++) {      /* its shadow, then the brass */
        float sx = pass ? 0 : 0.03f * sq, sy = pass ? 0 : 0.04f * sq;
        const uint8_t *c = pass ? BRASS : dark;
        float a = pass ? 1 : 0.35f;
        float tx = cx + dx * l + sx, ty = cy + dy * l + sy;   /* the tip */
        pzk_stroke(o, W, H, cx - dx * l + sx, cy - dy * l + sy, tx - dx * 0.1f * sq, ty - dy * 0.1f * sq,
                   0.055f * sq, c, a);
        pzk_stroke(o, W, H, tx, ty, tx - dx * hw + px * hw, ty - dy * hw + py * hw, 0.055f * sq, c, a);
        pzk_stroke(o, W, H, tx, ty, tx - dx * hw - px * hw, ty - dy * hw - py * hw, 0.055f * sq, c, a);
    }
}

/* a walnut rail from (ax, ay) to (bx, by): its shadow on the felt, the rail, its lit top edge */
static void rail(uint8_t *o, int W, int H, float ax, float ay, float bx, float by, int sq)
{
    static const uint8_t black[3] = {0, 0, 0};
    float r = 0.075f * sq, s = 0.05f * sq;
    pzk_stroke(o, W, H, ax + s, ay + 1.2f * s, bx + s, by + 1.2f * s, r * 1.2f, black, 0.3f);
    pzk_stroke(o, W, H, ax, ay, bx, by, r, WALNUT, 1);
    pzk_stroke(o, W, H, ax - 0.3f * r, ay - 0.3f * r, bx - 0.3f * r, by - 0.3f * r, 0.35f * r, WALNUT_LIT, 0.8f);
}

/* the tray: the frame, the felt, the blocks, the holes (lids apart), the gates and arrows, the rails;
 * (w sq + 2 f) x (h sq + 2 f) */
static void paint_board(const tlm_map *m, int sq, int f, uint8_t *o)
{
    int W = m->w * sq + 2 * f, H = m->h * sq + 2 * f;
    art_board_n(m->w, m->h, sq, f, 0, o);
    art_felt(o, W, H, f, f, W - f, H - f);
    for (int c = 1; c < m->w; c++)              /* the squares, faintly */
        art_tint_rect(o, W, H, f + c * sq - 0.5f, f, f + c * sq + 0.5f, H - f, PUZ_WHITE, 0.05f);
    for (int c = 1; c < m->h; c++)
        art_tint_rect(o, W, H, f, f + c * sq - 0.5f, W - f, f + c * sq + 0.5f, PUZ_WHITE, 0.05f);
    for (int c = 0; c < m->w * m->h; c++) {
        float x0 = f + c % m->w * sq, y0 = f + c / m->w * sq, cx = x0 + sq / 2.0f, cy = y0 + sq / 2.0f;
        switch (m->t[c]) {
        case TLM_HOLE: case TLM_LID: paint_hole(o, W, H, cx, cy, sq, m->k[c]); break;
        case TLM_GATE: paint_gate(o, W, H, x0, y0, sq, m->k[c]); break;
        case TLM_ONEWAY: paint_arrow(o, W, H, cx, cy, sq, m->k[c]); break;
        case TLM_BLOCK: {
            static const uint8_t black[3] = {0, 0, 0};
            float in = 0.06f * sq;
            art_roundrect_put(o, W, H, (int)(x0 + in + 0.05f * sq), (int)(y0 + in + 0.06f * sq), (int)(sq - 2 * in),
                              (int)(sq - 2 * in), 0.14f * sq, black, 0.3f);
            art_roundrect_put(o, W, H, (int)(x0 + in), (int)(y0 + in), (int)(sq - 2 * in), (int)(sq - 2 * in),
                              0.14f * sq, WALNUT, 1);
            art_roundrect_put(o, W, H, (int)(x0 + 2 * in), (int)(y0 + 2 * in), (int)(sq - 4 * in), (int)(sq - 4 * in),
                              0.1f * sq, WALNUT_LIT, 0.35f);
            break;
        }
        default: break;
        }
    }
    for (int c = 0; c < m->w * m->h; c++) {     /* the rails between squares */
        int x = c % m->w, y = c / m->w;
        float x0 = f + x * sq, y0 = f + y * sq;
        if (x < m->w - 1 && (m->wall[c] & TLM_WALL_R)) rail(o, W, H, x0 + sq, y0, x0 + sq, y0 + sq, sq);
        if (y < m->h - 1 && (m->wall[c] & TLM_WALL_D)) rail(o, W, H, x0, y0 + sq, x0 + sq, y0 + sq, sq);
    }
}

/* a marble's glass, size x size: its colour deeper towards the rim, a pale swirl through it (turned as it rolls) */
static void paint_ball(int k, int size, uint8_t *o)
{
    uint8_t col[3];
    colour_of(k, col);
    float r = size / 2.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = (x + 0.5f - r) / r, dy = (y + 0.5f - r) / r, d = hypotf(dx, dy);
            if (d > 1 + 1 / r) continue;
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            float th = atan2f(dy, dx), band = fabsf(sinf(2 * th + 3.2f * d));
            float sw_ = d < 0.92f && band < 0.22f ? (1 - band / 0.22f) * (1 - d) : 0;
            float k1 = 1.12f - 0.45f * d * d;
            for (int i = 0; i < 3; i++) {
                float v = col[i] * k1;
                p[i] = (uint8_t)clampf(v + (250 - v) * 0.55f * sw_, 0, 255);
            }
            p[3] = (uint8_t)(255 * clampf((1 - d) * r + 0.5f, 0, 1));
        }
}

/* the light on it, not turning: a bright spot at its top left, a soft one low right, its rim shaded */
static void paint_shine(int k, int size, uint8_t *o)
{
    (void)k;
    float r = size / 2.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = (x + 0.5f - r) / r, dy = (y + 0.5f - r) / r, d = hypotf(dx, dy);
            if (d > 1) continue;
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            float hl = clampf(1 - hypotf(dx + 0.36f, dy + 0.4f) / 0.3f, 0, 1);
            float low = 0.35f * clampf(1 - hypotf(dx - 0.42f, dy - 0.45f) / 0.25f, 0, 1);
            float rim = clampf((d - 0.7f) / 0.3f, 0, 1) * (0.55f + 0.45f * clampf((dx + dy) * 0.7f, -1, 1));
            float a = clampf(rim * 0.45f, 0, 1), wa = clampf(hl * hl * 0.95f + low, 0, 1);
            if (wa > a) { p[0] = p[1] = p[2] = 255; p[3] = (uint8_t)(255 * wa * clampf((1 - d) * r, 0, 1)); }
            else { p[0] = p[1] = p[2] = 0; p[3] = (uint8_t)(255 * a * clampf((1 - d) * r, 0, 1)); }
        }
}

/* a lid: a brass disc, a jewel of its colour, a ring stamped round it */
static void paint_lid(int k, int size, uint8_t *o)
{
    static const uint8_t dark[3] = {70, 50, 26};
    uint8_t col[3];
    colour_of(k, col);
    float c = size / 2.0f;
    pzk_ball(o, size, size, c, c, 0.48f * size, BRASS, 1, 0.9f);
    pzk_arc(o, size, size, c, c, 0.3f * size, 0.335f * size, 0, 360, dark, 0.45f);
    pzk_ball(o, size, size, c, c, 0.17f * size, col, 1, 1);
}

static void token(int player, int size, uint8_t *rgba)   /* three marbles */
{
    (void)player;
    int d = (int)(0.5f * size);
    uint8_t *b = calloc((size_t)d * d, 4);
    if (!b) return;
    static const float P[3][2] = {{0.3f, 0.62f}, {0.7f, 0.62f}, {0.5f, 0.3f}};
    for (int i = 0; i < 3; i++) {
        memset(b, 0, (size_t)d * d * 4);
        paint_ball(i == 2 ? 3 : i == 1 ? 2 : 0, d, b);
        int x = (int)(P[i][0] * size - d / 2.0f), y = (int)(P[i][1] * size - d / 2.0f);
        art_blend(rgba, size, size, b, d, d, x, y);
        memset(b, 0, (size_t)d * d * 4);       /* the light over it (painted alone: it covers the ball) */
        paint_shine(0, d, b);
        art_blend(rgba, size, size, b, d, d, x, y);
    }
    free(b);
}

static void make_tex(SDL_Texture **t, int size, void (*paint)(int, int, uint8_t *), int k)
{
    ui_free(t);
    memset(ui_buf(size, size), 0, (size_t)size * size * 4);
    paint(k, size, ui_buf(size, size));
    *t = ui_tex(size, size, ui_buf(size, size));
}

static void layout(box b)
{
    last_box = b;
    fr = (int)(fminf(b.w, b.h) * FRAME);
    cell = (int)fminf((b.w - 2 * fr) / G.m.w, (b.h - 2 * fr) / G.m.h);
    bw = G.m.w * cell + 2 * fr;
    bh = G.m.h * cell + 2 * fr;
    ox = floorf(b.x + (b.w - bw) / 2) + fr;
    oy = floorf(b.y + (b.h - bh) / 2) + fr;
    if (made_cell != cell || made_serial != serial) {
        int fresh = made_cell != cell || !t_shine;
        made_cell = cell;
        made_serial = serial;
        ui_free(&t_board);
        uint8_t *o = malloc((size_t)bw * bh * 4);
        if (o) {
            paint_board(&G.m, cell, fr, o);
            t_board = ui_tex(bw, bh, o);
            free(o);
        }
        if (fresh) {
            int d = (int)(BALL * cell), l = (int)(0.86f * cell);
            for (int k = 0; k < TLM_COLOURS; k++) {
                make_tex(&t_ball[k], d, paint_ball, k);
                make_tex(&t_lid[k], l, paint_lid, k);
            }
            make_tex(&t_shine, d, paint_shine, 0);
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_par);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int solved(void) { return tlm_solved(&G.m, &G.s); }
static int perfect(void) { return solved() && G.moves == G.par; }

static int roll_ms(int len) { return len ? ROLL0_MS + ROLL_MS * len : 0; }
static int moving_ms(void)                      /* the rolls, the knocks and the drops of the tilt shown */
{
    int most = 0;
    for (int i = 0; i < G.m.nm; i++) {
        int t = roll_ms(mv.tr.len[i]) + (mv.tr.drop[i] ? SINK_MS : mv.tr.len[i] ? KNOCK_MS : 0);
        if (mv.tr.drop[i] == 2) t = roll_ms(mv.tr.len[i]) + 80 + SINK_MS;
        if (t > most) most = t;
    }
    return TIP_MS / 2 + most;
}

static void calm(void)
{
    mv.on = win.on = nudge.on = 0;
    hint_way = down = -1;
    sw.on = 0;
    memset(lid_t0, 0, sizeof lid_t0);
}

static void begin(void)
{
    serial++;
    calm();
    memset(spin, 0, sizeof spin);
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!tlm_level(&G, level)) tlm_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    tlm_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(G.moves, par()); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (mv.on && now - mv.t0 < (Uint32)moving_ms()) || (win.on && now - win.t0 < FLARE_MS);
}

static void push(int way)
{
    if (solved() || way < 0) return;
    tlm_state from = G.s;
    tlm_trace tr;
    if (!tlm_try(&G.m, &G.s, way, NULL, &tr)) {   /* nothing moves: the tray tips a little and back */
        nudge.on = 1;
        nudge.dir = way;
        nudge.t0 = ui_ms();
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    for (int c = 0; c < G.m.w * G.m.h; c++)     /* the lids shut before */
        lid_t0[c] = G.m.t[c] == TLM_LID && tlm_lid_open(&G.m, &G.s, c) ? 1 : 0;
    tlm_move(&G, way);
    hint_way = -1;
    nudge.on = 0;
    mv.on = 1;
    mv.dir = way;
    mv.from = from;
    mv.tr = tr;
    mv.t0 = ui_ms();
    int last = 0;
    for (int i = 0; i < G.m.nm; i++) {
        int t = TIP_MS / 2 + roll_ms(tr.len[i]);
        if (tr.drop[i]) ui_sound_later(G.s.stuck >> i & 1 ? SND_DENY : SND_DROP, t + (tr.drop[i] == 2 ? 80 : 0));
        else if (tr.len[i]) ui_sound_later(SND_PLACE, t);
        if (t > last) last = t;
    }
    for (int c = 0; c < G.m.w * G.m.h; c++) {   /* the lids lifting now: when the tilt is over */
        int was = lid_t0[c] == 1;
        lid_t0[c] = G.m.t[c] == TLM_LID && !was && tlm_lid_open(&G.m, &G.s, c) ? mv.t0 + (Uint32)last : 0;
    }
    ui_sound(SND_MOVE);
    if (solved()) {
        win.on = 1;
        win.t0 = mv.t0 + (Uint32)moving_ms();
        ui_sound_later(SND_END, moving_ms() + 60);
    }
    ui_animate();
}

static void key(SDL_Keycode k)
{
    int w = swp_key_way(k);
    if (w >= 0) push(w);
}

static void hint(void)
{
    if (solved()) { ui_sound(SND_DENY); return; }
    int d = tlm_hint(&G);
    while (d < 0 && tlm_undo(&G)) d = tlm_hint(&G);
    mv.on = 0;
    if (d < 0) { ui_sound(SND_DENY); return; }
    hint_way = d;
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    int r = tlm_undo(&G);
    calm();
    return r;
}

static int script(const char *moves)
{
    int ok = tlm_script(&G, moves);
    calm();
    if (solved()) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- the tray tipped: a point of the level tray (pixels) where it shows now ----------------------------------- */
static float tip_amt;                           /* 0 level .. 1 tipped the way tip_dir */
static int tip_dir;

static void tipped(float x, float y, float *sx, float *sy)
{
    float cx = ox + G.m.w * cell / 2.0f, cy = oy + G.m.h * cell / 2.0f, hw = bw / 2.0f, hh = bh / 2.0f;
    float k = TIP * tip_amt;
    if (DX[tip_dir]) {                          /* tipped right: the right side lower, smaller */
        float u = (x - cx) / hw * DX[tip_dir];  /* -1 the high side .. 1 the low side */
        *sx = x - DX[tip_dir] * k * 0.5f * hw * (u * u);
        *sy = cy + (y - cy) * (1 - k * u);
    } else {
        float u = (y - cy) / hh * DY[tip_dir];
        *sy = y - DY[tip_dir] * k * 0.5f * hh * (u * u);
        *sx = cx + (x - cx) * (1 - k * u);
    }
}

static void draw_board(void)
{
    if (tip_amt <= 0.001f) { ui_put(t_board, ox - fr, oy - fr, (float)bw, (float)bh); return; }
    enum { N = 12 };
    SDL_Vertex v[(N + 1) * 2];
    int idx[N * 6];
    float x0 = ox - fr, y0 = oy - fr;
    for (int i = 0; i <= N; i++)
        for (int j = 0; j < 2; j++) {
            float a = (float)i / N, b = (float)j, u, w;   /* strips across the way it tips */
            if (DX[tip_dir]) { u = a; w = b; } else { u = b; w = a; }
            float px = x0 + u * bw, py = y0 + w * bh, sx, sy;
            tipped(px, py, &sx, &sy);
            float low = DX[tip_dir] ? (u - 0.5f) * DX[tip_dir] : (w - 0.5f) * DY[tip_dir];
            Uint8 s = (Uint8)clampf(255 * (1 - 0.1f * tip_amt * (low + 0.5f)), 0, 255);
            v[i * 2 + j] = (SDL_Vertex){{sx, sy}, {s, s, s, 255}, {u, w}};
        }
    for (int i = 0; i < N; i++) {
        int a = i * 2, *q = idx + i * 6;
        q[0] = a; q[1] = a + 1; q[2] = a + 3; q[3] = a; q[4] = a + 3; q[5] = a + 2;
    }
    SDL_RenderGeometry(ui.r, t_board, v, (N + 1) * 2, idx, N * 6);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)
{
    *x = ox + (c % G.m.w + 0.5f) * cell;
    *y = oy + (c / G.m.w + 0.5f) * cell;
}

/* marble i now: where (level tray), how sunk (0 rolling .. 1 in its hole), how far it rolled (squares) */
static void marble_at(int i, Uint32 now, float *x, float *y, float *sink, float *dist)
{
    int in = G.s.in >> i & 1;
    cell_xy(G.s.at[i], x, y);
    *sink = in ? 1 : 0;
    *dist = 0;
    if (!mv.on) return;
    float t = (float)(int)(now - mv.t0) - TIP_MS / 2.0f;
    int len = mv.tr.len[i];
    if (t < 0) { cell_xy(mv.tr.from[i], x, y); *sink = mv.from.in >> i & 1; return; }
    float T = (float)roll_ms(len);
    if (len && t < T) {                         /* rolling, faster and faster */
        float q = t / T, f = 0.25f * q + 0.75f * q * q;   /* a little speed at once, then more */
        float fx, fy, tx, ty;
        cell_xy(mv.tr.from[i], &fx, &fy);
        cell_xy(G.s.at[i], &tx, &ty);
        *x = fx + (tx - fx) * f;
        *y = fy + (ty - fy) * f;
        *dist = len * f;
        *sink = 0;
        return;
    }
    *dist = (float)len;
    float after = t - T;
    if (mv.tr.drop[i]) {                        /* dropping in (a lid lifting first: a moment later) */
        float wait = mv.tr.drop[i] == 2 ? 80 : 0;
        *sink = clampf((after - wait) / SINK_MS, 0, 1);
    } else if (len && after < KNOCK_MS) {       /* knocking against what stopped it: back a little */
        float k = 0.07f * sinf(PUZ_PI * after / KNOCK_MS) * cell;
        *x -= DX[mv.dir] * k;
        *y -= DY[mv.dir] * k;
    }
}

static void draw(void)
{
    if (made_cell < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    tip_amt = 0;                                /* the tray: tipping, held, settling back */
    if (mv.on) {
        float t = (float)(int)(now - mv.t0), hold = (float)moving_ms();
        tip_dir = mv.dir;
        tip_amt = t < TIP_MS ? puz_ease(t / TIP_MS) : t < hold ? 1 : 1 - puz_ease((t - hold) / SETTLE_MS);
        if (t >= hold + SETTLE_MS) { mv.on = 0; tip_amt = 0; }
        else ui_animate();
    } else if (nudge.on) {
        float t = (float)(int)(now - nudge.t0);
        tip_dir = nudge.dir;
        tip_amt = 0.45f * sinf(PUZ_PI * clampf(t / 260, 0, 1));
        if (t >= 260) nudge.on = 0;
        else ui_animate();
    }
    draw_board();
    float d = BALL * cell;
    for (int c = 0; c < G.m.w * G.m.h; c++) {   /* the lids: shut, or lifting away */
        if (G.m.t[c] != TLM_LID) continue;
        int open = tlm_lid_open(&G.m, &G.s, c);
        float a = 1, s = 1;
        if (open) {
            if (!lid_t0[c] || now >= lid_t0[c] + LID_MS) continue;
            float q = now < lid_t0[c] ? 0 : (now - lid_t0[c]) / (float)LID_MS;
            a = 1 - q;
            s = 1 + 0.3f * q;
            ui_animate();
        }
        float cx, cy, sx, sy;
        cell_xy(c, &cx, &cy);
        tipped(cx, cy, &sx, &sy);
        float l = 0.86f * cell * s;
        ui_shadow_flat(sx + 0.04f * cell, sy + 0.05f * cell, l, l, 0.4f * a);
        SDL_SetTextureAlphaMod(t_lid[G.m.k[c]], (Uint8)(255 * a));
        ui_put_c(t_lid[G.m.k[c]], sx, sy - (s - 1) * 0.3f * cell, l, l);
        SDL_SetTextureAlphaMod(t_lid[G.m.k[c]], 255);
    }
    if (hint_way >= 0 && !mv.on) {              /* the hint: where the marbles would come to rest */
        tlm_state t;
        tlm_trace tr;
        if (tlm_try(&G.m, &G.s, hint_way, &t, &tr))
            for (int i = 0; i < G.m.nm; i++)
                if (tr.len[i]) {
                    float hx, hy;
                    cell_xy(t.at[i], &hx, &hy);
                    ui_mark_gold(hx, hy, 1.1f * cell);
                }
    }
    float wt = win.on ? (float)(int)(now - win.t0) : -1e9f;
    for (int pass = 0; pass < 2; pass++)        /* the marbles in holes first, then those rolling */
        for (int i = 0; i < G.m.nm; i++) {
            float x, y, sink, dist, sx, sy;
            marble_at(i, now, &x, &y, &sink, &dist);
            if ((sink >= 1) != (pass == 0)) continue;
            tipped(x, y, &sx, &sy);
            int k = G.m.col[i] & 3;
            float r = d * (1 - 0.22f * sink);
            Uint8 dim = (Uint8)(255 * (1 - 0.3f * sink));
            if (sink < 1) ui_shadow_flat(sx + 0.06f * cell, sy + 0.08f * cell, r, r, 0.55f * (1 - sink));
            if (dist > 0 || mv.on) {           /* the swirl turned by the way rolled */
                float sgn = DX[mv.dir] - DY[mv.dir];
                spin[i] = (float)((G.moves * 97 + i * 53) % 360) + sgn * dist * 360.0f / (PUZ_PI * BALL);
            }
            SDL_SetTextureColorMod(t_ball[k], dim, dim, dim);
            ui_put_rot(t_ball[k], sx, sy, r, r, spin[i]);
            SDL_SetTextureColorMod(t_ball[k], 255, 255, 255);
            SDL_SetTextureAlphaMod(t_shine, (Uint8)(255 * (1 - 0.5f * sink)));
            ui_put_c(t_shine, sx, sy, r, r);
            SDL_SetTextureAlphaMod(t_shine, 255);
            if ((G.s.stuck >> i & 1) && sink >= 1) {   /* in another colour's hole: a red glow */
                ui_mark_glow(sx, sy, (1.25f + 0.1f * sinf(now / 200.0f)) * cell);
                ui_animate();
            }
        }
    if (win.on && wt >= 0) {
        for (int c = 0; c < G.m.w * G.m.h; c++) {
            if (G.m.t[c] != TLM_HOLE && G.m.t[c] != TLM_LID) continue;
            float cx, cy, q = puz_pulse(wt, 0, FLARE_MS);
            cell_xy(c, &cx, &cy);
            if (q > 0) ui_mark_gold(cx, cy, (1.2f + 1.4f * q) * cell * q);
        }
        if (perfect())
            for (int c = 0; c < G.m.w * G.m.h; c++) {
                float best = 1e9f, cx, cy;
                for (int h = 0; h < G.m.w * G.m.h; h++)
                    if (G.m.t[h] == TLM_HOLE || G.m.t[h] == TLM_LID)
                        best = fminf(best, hypotf((float)(c % G.m.w - h % G.m.w), (float)(c / G.m.w - h / G.m.w)));
                float k = puz_pulse(wt, FLARE_MS * 0.3f + best * WAVE_STEP, WAVE_LEN);
                if (k <= 0) continue;
                cell_xy(c, &cx, &cy);
                ui_mark_gold(cx, cy, 1.2f * cell * k);
            }
        if (wt < FLARE_MS + (G.m.w + G.m.h) * WAVE_STEP + WAVE_LEN) ui_animate();
    }
    unsigned can = 0;
    for (int w = 0; w < 4; w++)
        if (!solved() && tlm_try(&G.m, &G.s, w, NULL, NULL)) can |= 1u << w;
    swp_tools_draw(can, mv.on ? -1 : hint_way, !solved(), down);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void tap_at(float x, float y)            /* the way from the tray's middle to the finger */
{
    float cx = ox + G.m.w * cell / 2.0f, cy = oy + G.m.h * cell / 2.0f, dx = (x - cx) / bw, dy = (y - cy) / bh;
    if (hypotf(dx, dy) < 0.08f) { ui_sound(SND_DENY); return; }
    push(swp_way(dx, dy));
}

static void press(float x, float y)
{
    sw.on = 0;
    down = swp_tool_hit(x, y);
    if (down >= 0) { ui_animate(); return; }
    if (!solved() && x >= ox - fr && y >= oy - fr && x < ox - fr + bw && y < oy - fr + bh) swp_begin(&sw, x, y);
}

static void motion(float x, float y)
{
    int w = swp_follow(&sw, x, y, 0.6f * ui.unit);
    if (w >= 0) push(w);
}

static void release(float x, float y)
{
    if (down >= 0) {
        int k = down;
        down = -1;
        ui_animate();
        if (swp_tool_hit(x, y) != k) return;
        if (k == SWP_HINT) hint();
        else if (solved()) ui_sound(SND_DENY);
        else push(k);
        return;
    }
    int tap, w = swp_end(&sw, x, y, 0.3f * ui.unit, &tap);
    if (w >= 0) push(w);
    else if (tap && !solved()) tap_at(sw.x0, sw.y0);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.moves, t_par, par(), solved() && G.moves <= par());
}

/* ---- the shell's pictures: a tray, its marbles at their starts ----------------------------------------------- */
static void paint_tray(const tlm_map *m, int size, uint8_t *rgba)
{
    int big = m->w > m->h ? m->w : m->h, f = (int)(size * FRAME), sq = (size - 2 * f) / big;
    if (sq < 3) return;
    int W = m->w * sq + 2 * f, H = m->h * sq + 2 * f, d = (int)(BALL * sq), l = (int)(0.86f * sq);
    uint8_t *o = malloc((size_t)W * H * 4), *b = calloc((size_t)d * d, 4), *lid = calloc((size_t)l * l, 4);
    if (o && b && lid) {
        paint_board(m, sq, f, o);
        for (int c = 0; c < m->w * m->h; c++)
            if (m->t[c] == TLM_LID) {
                memset(lid, 0, (size_t)l * l * 4);
                paint_lid(m->k[c], l, lid);
                art_blend(o, W, H, lid, l, l, f + c % m->w * sq + (sq - l) / 2, f + c / m->w * sq + (sq - l) / 2);
            }
        for (int i = 0; i < m->nm; i++) {
            memset(b, 0, (size_t)d * d * 4);
            paint_ball(m->col[i], d, b);
            float cx = f + (m->start[i] % m->w + 0.5f) * sq, cy = f + (m->start[i] / m->w + 0.5f) * sq;
            art_flat_put(o, W, H, b, d, cx, cy);
            memset(b, 0, (size_t)d * d * 4);   /* the light over it (painted alone: it covers the ball) */
            paint_shine(0, d, b);
            art_blend(o, W, H, b, d, d, (int)(cx - d / 2.0f + 0.5f), (int)(cy - d / 2.0f + 0.5f));
        }
        art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    }
    free(o);
    free(b);
    free(lid);
}

/* the menu's card and the random tab's: a level of that kind (levels are read, never made here: the cards are
 * painted on a thread of their own) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const int LV[TLM_VARIANTS] = {14, 40, 128};
    tlm_game T;
    if (tlm_level(&T, variant < 0 ? 46 : LV[variant % TLM_VARIANTS])) paint_tray(&T.m, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    tlm_game T;
    if (tlm_level(&T, level)) paint_tray(&T.m, size, rgba);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const ICON[TLM_CHAPTERS] = {
        "R0.0.0/.0.0.0/.0.0r0", "R0.0B0/.0.0.0/b0.0r0", "R0k0.0/.0.0.0/.0.0r0", "R0>0.0/.0.0.0/.0.0r0",
        "R0.0R0/.0.0.0/r0.010", "R0>0B0/k0.0.0/b0.010",
    };
    tlm_map m;
    if (chapter >= 0 && chapter < TLM_CHAPTERS && tlm_parse(&m, ICON[chapter])) paint_tray(&m, size, rgba);
}

static const game tiltmaze_game = {
    .id = "tiltmaze", .order = 455, .variants = TLM_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = TLM_LEVELS, .chapters = TLM_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1,
    .tools = SWP_KNOBS,
};
REGISTER_GAME(tiltmaze_game)
