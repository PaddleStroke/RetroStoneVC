/*
 * Bubble Pop in RetroStone Board, for one player: a deep wooden box in the house frame, glossy marbles hanging from its
 * brass ceiling, a brass launcher at the bottom (the rules: bubbles_rules.c). Drag on the box to aim (a dotted line
 * shows the path, off a wall or two, and a ghost where the marble will land) and let go to shoot; the arrow keys step
 * the aim from one landing place to the next, up (or Return) shoots, down swaps. Tap the next marble (beside the
 * launcher) to swap it in, the launcher to shoot along the line shown. The marble flies, bounces off the walls, snaps
 * into its place with a little wobble of its neighbours; three of a colour burst into shards, one after another from
 * the shot; what no longer hangs from the ceiling drops, bounces on the floor and rolls out. Colours carry a pale mark
 * each (a ring, a triangle, a diamond, a star, a cross, two bars) for colour-blind eyes. Stones are rough grey, gems
 * sit in clear glass, locked marbles wear a brass cage, a bomb has a lit fuse, a rainbow marble swirls all six colours;
 * a ceiling coming down is a plank that lowers a row with a thud.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Everything moves on the shell's clock (ui_ms): a shot changes the rules' state at once, and its flight, pops and falls
 * are drawn from the time it was fired, so a game played again from its record looks the same; a new shot waits until
 * the last one has landed (ui_ms arithmetic). The tools: restart, the lamp (the hint: the aim of a way to win, in gold;
 * from a lost position it first takes shots back). Undo takes back one shot, all the way to the start. The bar: the
 * shots made, and the par by the flag; beside the launcher, the shots left. Stars: the shots against the par. A try
 * lost (out of shots, a marble past the line) shows what went wrong, then the box veils under a big restart button
 * whose ring of lights goes out until the level starts again by itself; a tap anywhere restarts at once, and undo
 * still takes back the losing shot (from the try before a restart too).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bubbles.h"
#include "games/bubbles_levels.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define FRAME 0.46f                             /* the frame round the box, marbles */
#define SPEED 0.05f                             /* a shot's flight, marbles a ms */
#define POP_MS 48                               /* between two rings of pops */
#define WOBBLE_MS 300
#define LOAD_MS 170                             /* the next marble rolling into the launcher */
#define SWAP_MS 200
#define AIM_MS 110                              /* the keys' aim easing to its next place */
#define DROP_MS 360                             /* the ceiling coming down */
#define WIN_MS 1500
#define LOST_MS 2200                            /* a lost try shown before the level starts again */
#define GRAV 0.00005f                           /* falling, steps a ms squared (scaled by BUB_D) */
#define NFX 360

enum { T_RESTART, T_HINT, T_COUNT };
enum { FX_FALL, FX_SHARD, FX_RING, FX_SPARK, FX_CAGE };

static const uint8_t COL[BUB_COLOURS + 1][3] = {
    {0, 0, 0}, {212, 44, 50}, {38, 160, 84}, {48, 104, 220}, {244, 186, 36}, {146, 72, 196}, {238, 234, 224},
};
static const uint8_t STONE[3] = {150, 144, 134}, GLASS[3] = {196, 228, 238}, GEMC[3] = {70, 214, 220},
                     BRASS[3] = {214, 168, 82}, BOMBC[3] = {44, 44, 52}, GOLD[3] = {246, 204, 96};

static bub_game G;
static int serial, aim, hint_on, tool_down = -1, finger, finger_moved, press_on;
static float fx0, fy0;                          /* where the finger went down */
static bub_move hint_m;

/* the last shot on its way: its path and what it did, the state before it */
static struct {
    int on;
    Uint32 t0, t_land, t_fall, t_drop, t_end;
    bub_shot sh;
    bub_state before;
    float len[BUB_PATH], total;
    int maxring, won;
} A;
static struct { int on; Uint32 t0; } swp, load, win;
static struct { int on, from, to; Uint32 t0; } aimk;
static Uint32 lose_t0, deny_t0;
static bub_game pre;                            /* the try before the last restart (undo goes back into it) */
static int have_pre;

typedef struct {
    uint8_t kind, code;
    float x, y, vx, vy, spin;                   /* steps, steps a ms */
    Uint32 t0;
    int dur;
} fx_t;
static fx_t fx[NFX];
static int nfx;

static box last_box;
static float ox, oy, k;                         /* the box's inside on the screen (its top left), pixels a step */
static int bw, bh, fr;                          /* the board's picture, its frame */
static int made_k = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_board, *t_plate, *t_marble[16], *t_cage, *t_ghost, *t_par, *t_barrel;
static int marble_px;

/* ---- painting -------------------------------------------------------------------------------------------------- */
static float smooth01(float e0, float e1, float x) { float t = clampf((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t); }

/* a colour's pale mark (an alpha mask d x d): 1 a ring, 2 a triangle, 3 a diamond, 4 a star, 5 a cross, 6 two bars */
static void mark_mask(int col, int d, uint8_t *m)
{
    memset(m, 0, (size_t)d * d * 4);
    static const uint8_t W[3] = {255, 255, 255};
    art_fig *f = art_fig_new();
    switch (col) {
    case 1: art_arc(f, ART_PART, 0.5f, 0.5f, 0.085f, 0.155f, 0, 360); break;
    case 2: { const float t[] = {0.5f, 0.33f, 0.665f, 0.62f, 0.335f, 0.62f}; art_poly(f, ART_PART, t, 3); break; }
    case 3: { const float t[] = {0.5f, 0.33f, 0.66f, 0.5f, 0.5f, 0.67f, 0.34f, 0.5f}; art_poly(f, ART_PART, t, 4); break; }
    case 4: art_star(f, ART_PART, 0.5f, 0.52f, 0.18f); break;
    case 5: art_rect(f, ART_PART, 0.455f, 0.345f, 0.545f, 0.655f); art_rect(f, ART_PART, 0.345f, 0.455f, 0.655f, 0.545f); break;
    default: art_rect(f, ART_PART, 0.35f, 0.39f, 0.65f, 0.46f); art_rect(f, ART_PART, 0.35f, 0.54f, 0.65f, 0.61f); break;
    }
    art_fig_paint(m, d, f, W, NULL, 0, 0);
}

/* a lit marble d across with its soft shadow (lower right), in the picture o (d x d): kind 1..6 a colour, BUB_STONE,
 * BUB_GEM (a gem in clear glass), BUB_WILD (all six colours swirled), BUB_BOMB */
static void paint_marble(int kind, int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    float R = 0.445f * d, mx = 0.47f * d, my = 0.465f * d;
    float L[3] = {-0.45f, -0.62f, 0.64f}, HV[3] = {-0.45f, -0.62f, 1.64f};
    float ln = sqrtf(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]), hn = sqrtf(HV[0] * HV[0] + HV[1] * HV[1] + HV[2] * HV[2]);
    for (int i = 0; i < 3; i++) L[i] /= ln, HV[i] /= hn;
    uint8_t *mk = NULL;
    int md = (int)(2 * R);
    if (kind >= 1 && kind <= BUB_COLOURS && md > 6) {
        mk = malloc((size_t)md * md * 4);
        if (mk) mark_mask(kind, md, mk);
    }
    float gloss = kind == BUB_STONE ? 0.22f : kind == BUB_BOMB ? 0.8f : 1;
    for (int py = 0; py < d; py++)
        for (int px = 0; px < d; px++) {
            float col[3] = {0, 0, 0}, cov = 0, al = 0;
            for (int sy = 0; sy < 3; sy++)
                for (int sx = 0; sx < 3; sx++) {
                    float x = (px + (sx + 0.5f) / 3 - mx) / R, y = (py + (sy + 0.5f) / 3 - my) / R, r2 = x * x + y * y;
                    if (r2 >= 1) continue;
                    float z = sqrtf(1 - r2), nl = x * L[0] + y * L[1] + z * L[2], nh = x * HV[0] + y * HV[1] + z * HV[2];
                    float base[3], a = 1;
                    if (kind >= 1 && kind <= BUB_COLOURS) for (int j = 0; j < 3; j++) base[j] = COL[kind][j];
                    else if (kind == BUB_STONE) {
                        float n = art_noise((int)(px * 1.7f + sx), (int)(py * 1.7f + sy)) + 0.6f * art_noise(px / 3 + 40, py / 3);
                        for (int j = 0; j < 3; j++) base[j] = STONE[j] * (1 + 0.22f * n);
                    } else if (kind == BUB_BOMB) for (int j = 0; j < 3; j++) base[j] = BOMBC[j];
                    else if (kind == BUB_WILD) {
                        float ang = atan2f(y, x) + 2.2f * sqrtf(r2), h = (ang / (2 * PUZ_PI) + 1.0f) * 6;
                        int i0 = (int)floorf(h) % 6, i1 = (i0 + 1) % 6;
                        float f = h - floorf(h);
                        f = smooth01(0.25f, 0.75f, f);
                        for (int j = 0; j < 3; j++) base[j] = COL[1 + i0][j] * (1 - f) + COL[1 + i1][j] * f;
                    } else {                    /* a gem: clear glass, the stone inside */
                        for (int j = 0; j < 3; j++) base[j] = GLASS[j];
                        a = 0.3f + 0.6f * powf(sqrtf(r2), 3);
                        float gx = x / 0.74f, gy = (y - 0.03f) / 0.74f, ang = atan2f(gy, gx), rr = hypotf(gx, gy);
                        float oct = cosf(PUZ_PI / 8) / cosf(fmodf(ang + 2 * PUZ_PI + PUZ_PI / 8, PUZ_PI / 4) - PUZ_PI / 8);
                        if (rr < oct) {         /* its facets: the table in the middle, eight around, lit by turns */
                            int facet = (int)floorf((ang + PUZ_PI) / (PUZ_PI / 4)) & 7;
                            float sh = rr < 0.5f * oct ? 1.18f : facet % 2 ? 0.86f : 1.06f;
                            sh += 0.25f * clampf(-(gx + gy) * 0.6f, -0.4f, 0.6f);
                            for (int j = 0; j < 3; j++) base[j] = clampf(GEMC[j] * sh + (rr < 0.5f * oct ? 30 : 0), 0, 255);
                            a = 1;
                        }
                    }
                    float glow = gloss * 0.42f * clampf(x * 0.5f + y * 0.7f, 0, 1);   /* light through it, low right */
                    float kk = 0.36f + 0.72f * fmaxf(0, nl) + (kind == BUB_GEM ? 0.2f : glow);
                    float sp = gloss * (0.95f * powf(fmaxf(0, nh), 60) + 0.2f * powf(fmaxf(0, nh), 5));
                    float rim = clampf((sqrtf(r2) - 1) * R / 1.4f + 1, 0, 1) * 0.45f;
                    float mka = 0;
                    if (mk) {                   /* the mark, pale, shaded with the sphere */
                        int mxp = (int)((x + 1) * 0.5f * md), myp = (int)((y + 1) * 0.5f * md);
                        if (mxp >= 0 && myp >= 0 && mxp < md && myp < md) mka = mk[4 * (myp * md + mxp) + 3] / 255.0f;
                    }
                    for (int j = 0; j < 3; j++) {
                        float v = clampf(base[j] * kk, 0, 255);
                        if (mka > 0) {
                            float pale = kind == 4 || kind == 6 ? base[j] * 0.5f : 255;
                            v += (clampf(pale * (0.62f + 0.45f * fmaxf(0, nl)), 0, 255) - v) * mka * 0.62f;
                        }
                        v += (255 - v) * sp;
                        col[j] += v * (1 - rim) + base[j] * 0.35f * rim;
                    }
                    al += fminf(1, a + sp);
                    cov += 1;
                }
            uint8_t *p = o + 4 * ((size_t)py * d + px);
            float ssx = (px + 0.5f - mx - 0.07f * d) / R, ssy = (py + 0.5f - my - 0.09f * d) / R;
            float sa = 0.42f * clampf((1.06f - sqrtf(ssx * ssx + ssy * ssy)) / 0.25f, 0, 1), a = al / 9;
            float out = a + sa * (1 - a);
            for (int j = 0; j < 3; j++) p[j] = (uint8_t)(out > 0 && cov > 0 ? clampf(col[j] / cov * (a / out), 0, 255) : 0);
            p[3] = (uint8_t)(255 * out + 0.5f);
        }
    free(mk);
    if (kind == BUB_BOMB) {                     /* a brass cap and its fuse */
        float cx = 0.66f * d, cy = 0.27f * d;
        pzk_ball(o, d, d, cx, cy, 0.1f * d, BRASS, 1, 0.8f);
        pzk_stroke(o, d, d, cx + 0.03f * d, cy - 0.04f * d, 0.8f * d, 0.1f * d, 0.03f * d, (const uint8_t[]){90, 70, 50}, 1);
    }
}

/* the brass cage of a locked marble: a band round its middle, one over its top, rivets where they cross */
static void paint_cage(int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    float R = 0.445f * d, mx = 0.47f * d, my = 0.465f * d, wd = 0.075f;
    const float L[3] = {-0.42f, -0.6f, 0.68f};
    for (int py = 0; py < d; py++)
        for (int px = 0; px < d; px++) {
            float acc = 0, lit = 0;
            for (int s = 0; s < 4; s++) {
                float x = (px + 0.25f + 0.5f * (s & 1) - mx) / R, y = (py + 0.25f + 0.5f * (s >> 1) - my) / R;
                float r2 = x * x + y * y;
                if (r2 >= 1.02f) continue;
                float z = sqrtf(fmaxf(0, 1 - r2));
                /* the bands: the sphere's points near the plane y = 0.1 z (the middle) and x = 0.15 z (over the top) */
                float b1 = fabsf(y - 0.12f * z), b2 = fabsf(x + 0.18f * z);
                if (b1 < wd || b2 < wd) {
                    acc += 1;
                    lit += 0.55f + 0.6f * fmaxf(0, x * L[0] + y * L[1] + z * L[2]);
                }
            }
            if (acc <= 0) continue;
            uint8_t c[3];
            float l = lit / acc;
            for (int j = 0; j < 3; j++) c[j] = (uint8_t)clampf(BRASS[j] * l + (l > 1.05f ? 70 : 0), 0, 255);
            pzk_over(o + 4 * ((size_t)py * d + px), c, acc / 4);
        }
    for (int i = 0; i < 2; i++) {               /* rivets where the bands meet the rim */
        float x = i ? 0.94f : -0.94f;
        pzk_ball(o, d, d, mx + x * R * 0.98f, my + 0.12f * R * 0.3f, 0.07f * d, GOLD, 1, 0.9f);
    }
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    paint_marble(1, size, rgba);
}

/* the box: the frame, the inside (the house wood, deep and shaded), the line not to pass (a brass inlay), the
 * launcher's well; w x h steps of inside at kk pixels a step, the frame f pixels */
static void paint_board(const bub_level *L, float kk, int W, int H, int f, int full, uint8_t *o)
{
    art_plank(W - 2 * f, H - 2 * f, f, o);
    float fw = L->w * BUB_D * kk, fh = full ? bub_field_h(L) * kk : (float)(H - 2 * f);
    for (int y = f; y < H - f; y++)
        for (int x = f; x < W - f; x++) {
            float ix = x - f + 0.5f, iy = y - f + 0.5f;
            float edge = fminf(fminf(ix, fw - ix), iy) / (0.5f * BUB_D * kk);
            float depth = 0.56f + 0.16f * (iy / fh) + 0.0f;
            float sh = 1 - 0.38f * expf(-edge * 1.6f) - 0.25f * expf(-iy / (0.7f * BUB_D * kk));
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * depth * sh, 0, 255);
        }
    /* the brass ceiling rail */
    float rail = 0.12f * BUB_D * kk;
    for (int y = f; y < f + (int)ceilf(rail); y++)
        for (int x = f; x < W - f; x++) {
            float t = (y - f + 0.5f) / rail;
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(BRASS[i] * (1.15f - 0.5f * t), 0, 255);
            pzk_over(o + 4 * ((size_t)y * W + x), c, clampf(rail - (y - f), 0, 1));
        }
    if (!full) return;                          /* a tile's box: the top only */
    /* the line: a thin brass inlay across the box */
    int32_t lx, ly;
    bub_launcher(L, &lx, &ly);
    float yl = f + (BUB_R + (L->rows - 1) * BUB_ROWH + BUB_R + 90) * kk, t = fmaxf(1, 0.035f * BUB_D * kk);
    for (int i = 0; i < 40; i++) {
        float x0 = f + (i + 0.18f) * fw / 40, x1 = f + (i + 0.82f) * fw / 40;
        puz_ink_rect(o, W, H, x0, yl - t / 2, x1, yl + t / 2, BRASS, 0.55f);
    }
    /* the launcher's well: a dark recess round where it stands */
    float cx = f + lx * kk, cy = f + ly * kk, r = 0.78f * BUB_D * kk;
    for (int y = (int)(cy - r - 2); y < (int)(cy + r + 2) && y < H - f; y++)
        for (int x = (int)(cx - r - 2); x < (int)(cx + r + 2); x++) {
            if (x < f || y < f || x >= W - f) continue;
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float m = art_recess(x + 0.5f - cx, y + 0.5f - cy, r, 0.14f * r, 0.42f, 0.26f, 0.22f);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * m, 0, 255);
        }
    /* the next marble's cup, at the launcher's left; the shots' plate at its right */
    float nx = cx - 1.75f * BUB_D * kk, nr = 0.5f * BUB_D * kk;
    for (int y = (int)(cy - nr - 2); y < (int)(cy + nr + 2) && y < H - f; y++)
        for (int x = (int)(nx - nr - 2); x < (int)(nx + nr + 2); x++) {
            if (x < f || y < f || x >= W - f) continue;
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float m = art_recess(x + 0.5f - nx, y + 0.5f - cy, nr, 0.16f * nr, 0.45f, 0.26f, 0.22f);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * m, 0, 255);
        }
    float px = cx + 1.75f * BUB_D * kk, pw = 1.0f * BUB_D * kk, ph = 0.72f * BUB_D * kk;
    art_roundrect_put(o, W, H, (int)(px - pw / 2 + 0.06f * pw), (int)(cy - ph / 2 + 0.1f * ph), (int)pw, (int)ph,
                      0.2f * ph, (const uint8_t[]){20, 12, 8}, 0.35f);
    art_roundrect_put(o, W, H, (int)(px - pw / 2), (int)(cy - ph / 2), (int)pw, (int)ph, 0.2f * ph, BRASS, 1);
    art_roundrect_put(o, W, H, (int)(px - pw / 2 + 0.08f * ph), (int)(cy - ph / 2 + 0.08f * ph),
                      (int)(pw - 0.16f * ph), (int)(ph - 0.16f * ph), 0.14f * ph, (const uint8_t[]){236, 196, 112}, 1);
}

/* the ceiling's plank coming down: w steps wide, rows tall, its brass edge at the bottom */
static void paint_plate(const bub_level *L, float kk, int W, int H, uint8_t *o)
{
    (void)L;
    art_plank(W, H, 0, o);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float g = 0.82f + 0.04f * sinf(y * 0.9f / (0.05f * BUB_D * kk + 1));
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * g, 0, 255);
        }
    float rail = 0.14f * BUB_D * kk;
    for (int y = H - (int)ceilf(rail); y < H; y++)
        for (int x = 0; x < W; x++) {
            if (y < 0) continue;
            float t = (y - (H - rail)) / rail;
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(BRASS[i] * (1.2f - 0.55f * t), 0, 255);
            pzk_over(o + 4 * ((size_t)y * W + x), c, 1);
        }
}

/* the launcher's barrel: a brass arrow pointing up, centred */
static void paint_barrel(int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    art_fig *f = art_fig_new();
    const float shaft[] = {0.44f, 0.5f, 0.56f, 0.5f, 0.56f, 0.2f, 0.44f, 0.2f};
    art_poly(f, ART_PART, shaft, 4);
    art_head(f, ART_PART, 0.5f, 0.06f, -90, 0.2f);
    art_fig_paint(o, d, f, BRASS, (const uint8_t[]){110, 80, 36}, 0.012f, 0.6f);
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void make_marbles(int d)
{
    for (int i = 1; i <= BUB_COLOURS; i++) {
        ui_free(&t_marble[i]);
        paint_marble(i, d, ui_buf(d, d));
        t_marble[i] = ui_tex(d, d, ui_buf(d, d));
    }
    int extra[4] = {BUB_STONE, BUB_GEM, BUB_WILD, BUB_BOMB};
    for (int i = 0; i < 4; i++) {
        ui_free(&t_marble[extra[i]]);
        paint_marble(extra[i], d, ui_buf(d, d));
        t_marble[extra[i]] = ui_tex(d, d, ui_buf(d, d));
    }
    ui_free(&t_cage);
    paint_cage(d, ui_buf(d, d));
    t_cage = ui_tex(d, d, ui_buf(d, d));
    ui_free(&t_ghost);                          /* the landing's ghost: a soft ring */
    uint8_t *g = ui_buf(d, d);
    memset(g, 0, (size_t)d * d * 4);
    pzk_arc(g, d, d, 0.47f * d, 0.465f * d, 0.36f * d, 0.445f * d, 0, 360, PUZ_WHITE, 0.9f);
    t_ghost = ui_tex(d, d, g);
    ui_free(&t_barrel);
    int bd = (int)(1.6f * d);
    paint_barrel(bd, ui_buf(bd, bd));
    t_barrel = ui_tex(bd, bd, ui_buf(bd, bd));
}

static void layout(box b)
{
    last_box = b;
    const bub_level *L = &G.L;
    float FW = L->w * BUB_D, FH = (float)bub_field_h(L), F = FRAME * BUB_D;
    k = fminf(b.w / (FW + 2 * F), b.h / (FH + 2 * F));
    fr = (int)ceilf(F * k);
    bw = (int)ceilf(FW * k) + 2 * fr;
    bh = (int)ceilf(FH * k) + 2 * fr;
    ox = floorf(b.x + (b.w - bw) / 2) + fr;
    oy = floorf(b.y + (b.h - bh) / 2) + fr;
    ui_board_used(ox - fr, oy - fr, (float)bw, (float)bh);
    int kq = (int)(k * 100000);
    if (made_k != kq || made_serial != serial) {
        uint8_t *o = malloc((size_t)bw * bh * 4);
        ui_free(&t_board);
        if (o) {
            paint_board(L, k, bw, bh, fr, 1, o);
            t_board = ui_tex(bw, bh, o);
        }
        free(o);
        int pw = (int)ceilf(FW * k), ph = (int)ceilf((BUB_ROWS * BUB_ROWH + BUB_R) * k);
        o = malloc((size_t)pw * ph * 4);
        ui_free(&t_plate);
        if (o && L->every) {
            paint_plate(L, k, pw, ph, o);
            t_plate = ui_tex(pw, ph, o);
        }
        free(o);
        int d = (int)ceilf(BUB_D * k * 1.06f);
        if (made_k != kq || marble_px != d) make_marbles(d);
        marble_px = d;
        made_k = kq;
        made_serial = serial;
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

static float SX(float x) { return ox + x * k; }
static float SY(float y) { return oy + y * k; }

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    memset(&A, 0, sizeof A);
    swp.on = load.on = win.on = aimk.on = 0;
    nfx = 0;
    hint_on = 0;
    tool_down = -1;
    press_on = 0;
    lose_t0 = deny_t0 = 0;
}

static void begin(void)
{
    serial++;
    calm();
    aim = 0;
    have_pre = 0;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!bub_open(&G, level)) bub_open(&G, 0);
    begin();
}

static void start_random(int variant)
{
    bub_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int won(void) { return G.s.end == BUB_WON; }
static int lost(void) { return G.s.end == BUB_OVER || G.s.end == BUB_LOW; }
static int status(void) { return won() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.s.shots; }
static int par(void) { return G.L.nsol; }
static int level_stars(void) { return ui_par_stars(G.s.shots, par()); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (A.on && (int)(now - A.t_end) < 0) || (win.on && (int)(now - win.t0) < WIN_MS) ||
           (swp.on && (int)(now - swp.t0) < SWAP_MS);
}

/* a shot may be fired: the last one has landed and its pops have gone */
static int ready(void)
{
    Uint32 now = ui_ms();
    if (A.on && (int)(now - A.t_fall) < 0) return 0;
    if (swp.on && (int)(now - swp.t0) < SWAP_MS) return 0;
    return 1;
}

static void add_fx(int kind, int code, float x, float y, float vx, float vy, Uint32 t0, int dur, float spin)
{
    fx_t *e = nfx < NFX ? &fx[nfx++] : &fx[(t0 + (Uint32)nfx) % NFX];
    e->kind = (uint8_t)kind;
    e->code = (uint8_t)code;
    e->x = x;
    e->y = y;
    e->vx = vx;
    e->vy = vy;
    e->t0 = t0;
    e->dur = dur;
    e->spin = spin;
}

/* the effects of a shot just made, from the state before it */
static void stage(const bub_shot *o, const bub_state *before, Uint32 t0)
{
    memset(&A, 0, sizeof A);
    A.on = 1;
    A.sh = *o;
    A.before = *before;
    A.t0 = t0;
    for (int i = 1; i < o->n; i++) {
        A.len[i] = hypotf((float)(o->x[i] - o->x[i - 1]), (float)(o->y[i] - o->y[i - 1]));
        A.total += A.len[i];
    }
    int fly = (int)fmaxf(110, A.total / (SPEED * BUB_D));
    A.t_land = t0 + (Uint32)fly;
    for (int i = 0; i < o->npop; i++) if (o->popring[i] > A.maxring) A.maxring = o->popring[i];
    Uint32 pops_end = A.t_land + (o->npop ? 70 + (Uint32)(A.maxring + 1) * POP_MS : 40);
    A.t_fall = pops_end;
    A.t_drop = A.t_fall + (o->nfall ? 160 : 0);
    A.t_end = A.t_land + WOBBLE_MS;
    if (o->npop) A.t_end = pops_end + 450;
    if (o->nfall) A.t_end = A.t_fall + 1100;
    if (o->dropped && A.t_drop + DROP_MS > A.t_end) A.t_end = A.t_drop + DROP_MS;
    /* the pops: shards and a ring each, at their ring's time */
    for (int i = 0; i < o->npop; i++) {
        int c = o->pop[i];
        int32_t x, y;
        bub_cell_xy(&G.L, before->drop, c / BUB_MAXW, c % BUB_MAXW, &x, &y);
        Uint32 tp = A.t_land + 60 + (Uint32)o->popring[i] * POP_MS;
        add_fx(FX_RING, o->popc[i], (float)x, (float)y, 0, 0, tp, 320, 0);
        for (int s = 0; s < 7; s++) {
            float a = (s + art_hash(c, s)) * 2 * PUZ_PI / 7, v = (0.55f + 0.5f * art_hash(s, c + 7)) * BUB_D / 300.0f;
            add_fx(FX_SHARD, o->popc[i], x + cosf(a) * 0.25f * BUB_D, y + sinf(a) * 0.25f * BUB_D, cosf(a) * v,
                   sinf(a) * v - 0.35f * BUB_D / 300.0f, tp, 520, 0);
        }
        if (i < 6) ui_sound_later(i % 2 ? SND_TAP : SND_CAPTURE, (int)(tp - t0));
    }
    for (int i = 0; i < o->nunlock; i++) {
        int c = o->unlock[i];
        int32_t x, y;
        bub_cell_xy(&G.L, before->drop, c / BUB_MAXW, c % BUB_MAXW, &x, &y);
        for (int s = 0; s < 2; s++)
            add_fx(FX_CAGE, 0, (float)x, (float)y, (s ? 1 : -1) * 0.5f * BUB_D / 300.0f, -0.8f * BUB_D / 300.0f,
                   A.t_land + 60, 700, s ? 0.6f : -0.6f);
    }
    /* the falls: each one from where it hung, a little hop out, then down, a bounce on the floor, out */
    for (int i = 0; i < o->nfall; i++) {
        int c = o->fall[i];
        int32_t x, y;
        bub_cell_xy(&G.L, before->drop, c / BUB_MAXW, c % BUB_MAXW, &x, &y);
        float hx = art_hash(c, 31) - 0.5f, hy = art_hash(c, 57);
        add_fx(FX_FALL, o->fallc[i], (float)x, (float)y, hx * 0.9f * BUB_D / 300.0f, -(0.15f + 0.35f * hy) * BUB_D / 300.0f,
               A.t_fall + (Uint32)(c % 7) * 12, 1400, hx * 0.8f);
    }
    if (o->nfall) {
        for (int i = 0; i < 4 && i < o->nfall; i++) ui_sound_later(SND_DROP, (int)(A.t_fall - t0) + 330 + 70 * i);
    }
    ui_sound_later(SND_PLACE, fly);
    for (int i = 1; i < o->n - 2 && i < 3; i++) {   /* the walls' knocks */
        float at = 0;
        for (int j = 1; j <= i; j++) at += A.len[j];
        ui_sound_later(SND_TAP, (int)(at / A.total * fly));
    }
    if (o->dropped) ui_sound_later(SND_DROP, (int)(A.t_drop - t0) + DROP_MS - 80);
}

static void after_shot(Uint32 t0)
{
    hint_on = 0;
    load.on = 1;
    load.t0 = t0;
    if (won()) {
        win.on = 1;
        win.t0 = A.t_end > t0 + 200 ? A.t_end - 200 : t0 + 200;
        /* the rest of the box drops as well: a last cascade */
        for (int r = 0; r < BUB_ROWS; r++)
            for (int c = 0; c < bub_row_len(&G.L, r); c++) {
                int v = G.s.c[r][c];
                if (!v) continue;
                int32_t x, y;
                bub_cell_xy(&G.L, G.s.drop, r, c, &x, &y);
                int ci = r * BUB_MAXW + c;
                float hx = art_hash(ci, 31) - 0.5f;
                add_fx(FX_FALL, v, (float)x, (float)y, hx * 0.9f * BUB_D / 300.0f, -0.25f * BUB_D / 300.0f,
                       A.t_fall + 120 + (Uint32)(ci % 9) * 25, 1400, hx);
            }
    } else if (lost()) lose_t0 = A.t_end;
}

static void fire(int a, int swap)
{
    if (G.s.end != BUB_PLAYING || !ready()) { ui_sound(SND_DENY); return; }
    bub_state before = G.s;
    bub_shot o;
    if (!bub_play(&G, a, swap, &o)) { ui_sound(SND_DENY); return; }
    Uint32 now = ui_ms();
    stage(&o, &before, now);
    after_shot(now);
    ui_sound(SND_MOVE);
    ui_animate();
}

static void do_swap(void)
{
    if (!ready() || !bub_swap(&G.s)) { ui_sound(SND_DENY); return; }
    swp.on = 1;
    swp.t0 = ui_ms();
    if (hint_on) hint_m.swap = !hint_m.swap;
    ui_sound(SND_TAP);
    ui_animate();
}

/* a new try from the start; the try before it is kept, so undo can take back its last shot */
static void restart(void)
{
    if (!G.nh) { ui_sound(SND_DENY); return; }
    static bub_game fresh;
    pre = G;
    have_pre = 1;
    bub_new(&fresh, &G.L);
    fresh.hints = G.hints;
    G = fresh;
    calm();
    ui_sound(SND_TAP);
    ui_animate();
}

/* a try lost (out of shots, a marble past the line): shown a moment, then the level starts again by itself */
static void lost_tick(void)
{
    if (lost() && lose_t0 && (int)(ui_ms() - lose_t0) >= LOST_MS) restart();
}

static int undo(void)
{
    int ok;
    lost_tick();
    if (!G.nh && have_pre) {                    /* just restarted: back to the try before, its last shot taken back */
        G = pre;
        have_pre = 0;
        ok = 1;
        bub_undo(&G);
    } else ok = bub_undo(&G);
    calm();
    return ok;
}

static void hint(void)
{
    lost_tick();
    if (won()) { ui_sound(SND_DENY); return; }
    bub_move m;
    int had = G.nh;
    if (!bub_hint(&G, &m)) { ui_sound(SND_DENY); return; }
    if (G.nh != had) calm();                    /* it took shots back */
    if (m.swap) { bub_swap(&G.s); swp.on = 1; swp.t0 = ui_ms(); m.swap = 0; }
    hint_m = m;
    hint_on = 1;
    aimk.on = 1;
    aimk.from = aim;
    aimk.to = m.aim;
    aimk.t0 = ui_ms();
    aim = m.aim;
    G.hints++;
    ui_sound(SND_TAP);
    ui_animate();
}

static int script(const char *moves)
{
    calm();
    int ok = bub_script(&G, moves);
    if (won()) { win.on = 1; win.t0 = ui_ms() - WIN_MS; }
    if (lost()) lose_t0 = ui_ms();
    return ok;
}

/* ---- the aim ---------------------------------------------------------------------------------------------------- */
static float aim_deg(int a) { return atan2f((float)BUB_AY, (float)a) * 180 / PUZ_PI; }   /* 90 straight up */
static int aim_of_deg(float d)
{
    float r = d * PUZ_PI / 180;
    int a = (int)lroundf(BUB_AY * cosf(r) / sinf(r));
    return a < -BUB_AMAX ? -BUB_AMAX : a > BUB_AMAX ? BUB_AMAX : a;
}

/* the aim shown now (the keys ease it from one place to the next) */
static int aim_shown(Uint32 now)
{
    if (!aimk.on) return aim;
    float t = (now - aimk.t0) / (float)AIM_MS;
    if (t >= 1) { aimk.on = 0; return aim; }
    ui_animate();
    float d0 = aim_deg(aimk.from), d1 = aim_deg(aimk.to);
    return aim_of_deg(d0 + (d1 - d0) * puz_ease(t));
}

/* the aim towards a point of the screen: 0 when it is not above the launcher */
static int aim_at(float x, float y, int *a)
{
    int32_t lx, ly;
    bub_launcher(&G.L, &lx, &ly);
    float dx = (x - SX((float)lx)) / k, dy = (y - SY((float)ly)) / k;
    if (dy > -0.35f * BUB_D) return 0;
    if (hypotf(dx, dy) < 0.9f * BUB_D) return 0;
    float v = BUB_AY * dx / -dy;
    *a = (int)lroundf(clampf(v, -BUB_AMAX, BUB_AMAX));
    return 1;
}

/* ---- drawing ------------------------------------------------------------------------------------------------------ */
static void put_marble(int code, float x, float y, float scale, float alpha, float deg)
{
    int base = code & 15;
    SDL_Texture *t = code == BUB_RAINBOW ? t_marble[BUB_WILD] : base >= 1 && base <= BUB_COLOURS ? t_marble[base]
                   : t_marble[code < 16 ? code : 0];
    if (!t) return;
    float d = marble_px * scale;
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(alpha, 0, 1)));
    if (deg != 0) ui_put_rot(t, SX(x), SY(y), d, d, deg);
    else ui_put_c(t, SX(x), SY(y), d, d);
    SDL_SetTextureAlphaMod(t, 255);
    if (code & BUB_LOCK && t_cage) {
        SDL_SetTextureAlphaMod(t_cage, (Uint8)(255 * clampf(alpha, 0, 1)));
        ui_put_c(t_cage, SX(x), SY(y), d, d);
        SDL_SetTextureAlphaMod(t_cage, 255);
    }
}

static int in_list(const uint8_t *l, int n, int c)
{
    for (int i = 0; i < n; i++)
        if (l[i] == c) return i;
    return -1;
}

/* the ceiling's rows down now (eased as it comes down) */
static float drop_now(Uint32 now)
{
    if (A.on && A.sh.dropped) {
        float t = (float)(int)(now - A.t_drop) / DROP_MS;
        if (t < 0) return A.before.drop;
        if (t < 1) {
            ui_animate();
            float e = t < 0.7f ? puz_ease(t / 0.7f) * 1.06f : 1.06f - 0.06f * puz_ease((t - 0.7f) / 0.3f);
            return A.before.drop + e;
        }
    }
    return G.s.drop;
}

static void cell_at(int r, int c, float drop, float *x, float *y)
{
    int32_t xi, yi;
    bub_cell_xy(&G.L, 0, r, c, &xi, &yi);
    *x = (float)xi;
    *y = (float)yi + drop * BUB_ROWH;
}

/* the marbles hanging in the box now */
static void draw_grid(Uint32 now)
{
    float drop = drop_now(now);
    float wob_t = A.on ? (float)(int)(now - A.t_land) : 1e9f;
    int land = A.on ? A.sh.cell : -1;
    int32_t lxi = 0, lyi = 0;
    if (land >= 0) bub_cell_xy(&G.L, A.before.drop, land / BUB_MAXW, land % BUB_MAXW, &lxi, &lyi);
    for (int r = 0; r < BUB_ROWS; r++)
        for (int c = 0; c < bub_row_len(&G.L, r); c++) {
            int i = r * BUB_MAXW + c, v = G.s.c[r][c];
            float d = drop;
            if (A.on) {
                int p = in_list(A.sh.pop, A.sh.npop, i), f = in_list(A.sh.fall, A.sh.nfall, i);
                if (p >= 0) {                   /* still there until its pop */
                    Uint32 tp = A.t_land + 60 + (Uint32)A.sh.popring[p] * POP_MS;
                    if (i == land && (int)(now - A.t_land) < 0) continue;
                    float g = (float)(int)(now - tp);
                    if (g >= 0) {
                        if (g < 90) {
                            float x, y;
                            cell_at(r, c, A.before.drop, &x, &y);
                            put_marble(A.sh.popc[p] == BUB_BOMB ? BUB_BOMB : A.sh.popc[p], x, y, 1 + 0.35f * g / 90, 1 - g / 90, 0);
                        }
                        continue;
                    }
                    v = A.sh.popc[p];
                    d = A.before.drop;
                } else if (f >= 0) {            /* still hanging until the falls begin */
                    if ((int)(now - A.t_fall) >= 0) continue;
                    v = A.sh.fallc[f];
                    d = A.before.drop;
                } else if (i == land && (int)(now - A.t_land) < 0) continue;
                if (in_list(A.sh.unlock, A.sh.nunlock, i) >= 0 && (int)(now - (A.t_land + 60)) < 0) v |= BUB_LOCK;
            }
            if (!v) continue;
            if (win.on && (int)(now - (A.t_fall + 120)) >= 0) continue;   /* gone in the last cascade */
            float x, y;
            cell_at(r, c, d, &x, &y);
            if (wob_t >= 0 && wob_t < WOBBLE_MS && land >= 0 && i != land) {   /* the neighbours give a little */
                float dx = x - lxi, dy = y - (lyi + (d - A.before.drop) * BUB_ROWH), dist = hypotf(dx, dy);
                if (dist > 0 && dist < 2.2f * BUB_D) {
                    float amp = 0.07f * BUB_D * (dist < 1.2f * BUB_D ? 1 : 0.45f);
                    float s = sinf(wob_t / WOBBLE_MS * 3 * PUZ_PI) * expf(-wob_t / 110.0f) * amp;
                    x += dx / dist * s;
                    y += dy / dist * s;
                    ui_animate();
                }
            }
            float sc = 1;
            if (i == land && wob_t >= 0 && wob_t < WOBBLE_MS) {   /* the shot's own squash as it snaps */
                sc = 1 + 0.08f * sinf(wob_t / WOBBLE_MS * 2 * PUZ_PI) * expf(-wob_t / 120.0f);
                ui_animate();
            }
            put_marble(v, x, y, sc, 1, 0);
        }
}

/* the shot on its way: along its path, then easing into its place */
static void draw_flight(Uint32 now)
{
    if (!A.on || (int)(now - A.t_land) >= 0) return;
    float t = (float)(int)(now - A.t0) / (float)(A.t_land - A.t0), dist = clampf(t, 0, 1) * A.total;
    int i = 1;
    while (i < A.sh.n - 1 && dist > A.len[i]) { dist -= A.len[i]; i++; }
    float f = A.len[i] > 0 ? clampf(dist / A.len[i], 0, 1) : 1;
    float x = A.sh.x[i - 1] + (A.sh.x[i] - A.sh.x[i - 1]) * f, y = A.sh.y[i - 1] + (A.sh.y[i] - A.sh.y[i - 1]) * f;
    for (int tr = 1; tr <= 3; tr++) {           /* a faint trail */
        float bx = x - (A.sh.x[i] - A.sh.x[i - 1]) / (A.len[i] + 1) * tr * 0.32f * BUB_D,
              by = y - (A.sh.y[i] - A.sh.y[i - 1]) / (A.len[i] + 1) * tr * 0.32f * BUB_D;
        put_marble(A.sh.marble, bx, by, 1 - 0.12f * tr, 0.22f - 0.06f * tr, 0);
    }
    put_marble(A.sh.marble, x, y, 1, 1, 0);
    ui_animate();
}

static void draw_fx(Uint32 now)
{
    float floor_y = bub_field_h(&G.L) - BUB_R, wall = G.L.w * BUB_D - BUB_R;
    for (int i = 0; i < nfx; i++) {
        fx_t *e = &fx[i];
        float t = (float)(int)(now - e->t0);
        if (t < 0 || t > e->dur) continue;
        ui_animate();
        float g = GRAV * BUB_D, x = e->x + e->vx * t, y = e->y + e->vy * t + 0.5f * g * t * t;
        if (e->kind == FX_FALL) {
            /* the floor: one bounce, then out through it, fading */
            float a = 0.5f * g, b = e->vy, c = e->y - floor_y, disc = b * b - 4 * a * c, alpha = 1;
            if (disc > 0) {
                float t1 = (-b + sqrtf(disc)) / (2 * a);
                if (t > t1) {
                    float v1 = -(b + g * t1) * 0.42f, u = t - t1;
                    y = floor_y + v1 * u + 0.5f * g * u * u;
                    alpha = clampf(1 - (u - 160) / 300, 0, 1);
                }
            }
            float span = 2 * (wall - BUB_R), q = fmodf(x - BUB_R, span);   /* off the side walls */
            if (q < 0) q += span;
            x = BUB_R + (q > span / 2 ? span - q : q);
            if (alpha <= 0) continue;
            put_marble(e->code, x, y, 1, alpha, e->spin * t * 0.4f);
            if (e->code == BUB_GEM && alpha < 1) ui_mark_gold_a(SX(x), SY(y), 2.2f * BUB_D * k, 0.6f * alpha);
        } else if (e->kind == FX_SHARD) {
            float f = t / e->dur;
            const uint8_t *c = (e->code & 15) >= 1 && (e->code & 15) <= BUB_COLOURS ? COL[e->code & 15]
                             : e->code == BUB_GEM ? GEMC : e->code == BUB_BOMB ? (const uint8_t[]){255, 170, 60} : STONE;
            float sz = (0.22f - 0.12f * f) * BUB_D * k;
            pzk_dot(SX(x), SY(y), sz, 0.3f * sz, (pzk_rgba){c[0], c[1], c[2], (Uint8)(235 * (1 - f))}, 0);
            pzk_dot(SX(x) - 0.2f * sz, SY(y) - 0.2f * sz, 0.4f * sz, 0.3f * sz, (pzk_rgba){255, 255, 255, (Uint8)(150 * (1 - f))}, 0);
        } else if (e->kind == FX_RING) {
            float f = t / e->dur;
            pzk_dot(SX(e->x), SY(e->y), (0.9f + 1.0f * f) * BUB_D * k, 0.3f * BUB_D * k,
                    (pzk_rgba){255, 244, 214, (Uint8)(70 * (1 - f) * (1 - f))}, 1);
            if (e->code == BUB_BOMB && f < 0.6f)
                pzk_dot(SX(e->x), SY(e->y), (1.5f + 2.5f * f) * BUB_D * k, 0.8f * BUB_D * k,
                        (pzk_rgba){255, 150, 50, (Uint8)(200 * (1 - f / 0.6f))}, 1);
        } else if (e->kind == FX_CAGE) {
            float f = t / e->dur;
            pzk_line(SX(x) - 0.25f * BUB_D * k * cosf(e->spin * t * 0.02f), SY(y) - 0.25f * BUB_D * k * sinf(e->spin * t * 0.02f),
                     SX(x) + 0.25f * BUB_D * k * cosf(e->spin * t * 0.02f), SY(y) + 0.25f * BUB_D * k * sinf(e->spin * t * 0.02f),
                     0.08f * BUB_D * k, 1, (pzk_rgba){BRASS[0], BRASS[1], BRASS[2], (Uint8)(255 * (1 - f))}, 0);
        }
    }
}

/* the dotted line of the aim (gold: the hint's), off two walls at most, and the ghost where it lands */
static void draw_guide(Uint32 now)
{
    if (G.s.end != BUB_PLAYING || (A.on && (int)(now - A.t_land) < 0)) return;
    int a = aim_shown(now);
    bub_shot o;
    int cell = bub_trace(&G.L, &G.s, a, &o);
    int gold = hint_on && a == hint_m.aim;
    float step = 0.5f * BUB_D, phase = fmodf(now / 9.0f, step);
    int walls = 0;
    float along = 0;
    int last = cell >= 0 ? o.n - 2 : o.n - 1;   /* the point it stops at (before the snap) */
    for (int i = 1; i <= last && walls < 3; i++) {
        float x0 = (float)o.x[i - 1], y0 = (float)o.y[i - 1], x1 = (float)o.x[i], y1 = (float)o.y[i];
        float l = hypotf(x1 - x0, y1 - y0);
        for (float s = (i == 1 ? 0.95f * BUB_D : 0) + fmodf(step - fmodf(along, step) + phase, step); s < l; s += step) {
            float f = s / l, x = x0 + (x1 - x0) * f, y = y0 + (y1 - y0) * f, fade = clampf(1 - (along + s) / (26.0f * BUB_D), 0.35f, 1);
            if (gold) {
                pzk_dot(SX(x), SY(y), 0.34f * BUB_D * k, 0.2f * BUB_D * k, (pzk_rgba){255, 210, 100, (Uint8)(120 * fade)}, 1);
                pzk_dot(SX(x), SY(y), 0.17f * BUB_D * k, 1, (pzk_rgba){255, 236, 170, (Uint8)(255 * fade)}, 0);
            } else
                pzk_dot(SX(x), SY(y), 0.15f * BUB_D * k, 1, (pzk_rgba){250, 240, 220, (Uint8)(200 * fade)}, 0);
        }
        along += l;
        if (i < last) walls++;
    }
    if (cell >= 0 && o.bounces <= 2) {
        float x, y;
        cell_at(cell / BUB_MAXW, cell % BUB_MAXW, G.s.drop, &x, &y);
        SDL_SetTextureColorMod(t_ghost, gold ? 255 : 250, gold ? 214 : 240, gold ? 110 : 220);
        SDL_SetTextureAlphaMod(t_ghost, (Uint8)(gold ? 230 : 170));
        ui_put_c(t_ghost, SX(x), SY(y), (float)marble_px, (float)marble_px);
        SDL_SetTextureColorMod(t_ghost, 255, 255, 255);
        SDL_SetTextureAlphaMod(t_ghost, 255);
        if (gold) ui_mark_gold_a(SX(x), SY(y), 1.4f * BUB_D * k, 0.45f + 0.2f * sinf(now / 160.0f));
    }
    ui_animate();
}

/* the launcher: its barrel along the aim, the marble in it, the next one in its cup, the shots left on the plate */
static void draw_launcher(Uint32 now)
{
    int32_t lx, ly;
    bub_launcher(&G.L, &lx, &ly);
    float cx = (float)lx, cy = (float)ly, nx = cx - 1.75f * BUB_D;
    int a = aim_shown(now);
    float deg = 90 - aim_deg(a);
    float bd = 1.6f * marble_px;
    SDL_SetTextureColorMod(t_barrel, 0, 0, 0);
    SDL_SetTextureAlphaMod(t_barrel, 80);
    ui_put_rot(t_barrel, SX(cx) + 0.05f * BUB_D * k, SY(cy) + 0.07f * BUB_D * k, bd, bd, deg);
    SDL_SetTextureColorMod(t_barrel, 255, 255, 255);
    SDL_SetTextureAlphaMod(t_barrel, G.s.end == BUB_PLAYING ? 255 : 120);
    ui_put_rot(t_barrel, SX(cx), SY(cy), bd, bd, deg);
    SDL_SetTextureAlphaMod(t_barrel, 255);
    /* the marbles: the current one rolls in from the cup after a shot; a swap trades their places along an arc */
    float lt = load.on ? clampf((float)(int)(now - load.t0) / LOAD_MS, 0, 1) : 1;
    float st = swp.on ? clampf((float)(int)(now - swp.t0) / SWAP_MS, 0, 1) : 1;
    if (lt < 1 || st < 1) ui_animate();
    if (lt >= 1) load.on = 0;
    if (st >= 1) swp.on = 0;
    if (G.s.cur) {
        float e = puz_ease(lt < 1 ? lt : st), x = cx, y = cy, sc = 1;
        if (lt < 1) { x = nx + (cx - nx) * e; y = cy - 0.5f * BUB_D * sinf(PUZ_PI * e); sc = 0.78f + 0.22f * e; }
        else if (st < 1) { x = nx + (cx - nx) * e; y = cy + 0.55f * BUB_D * sinf(PUZ_PI * e); sc = 0.78f + 0.22f * e; }
        put_marble(G.s.cur, x, y, sc, 1, 0);
        if (G.s.cur == BUB_BOMB) {              /* its fuse burns */
            float fl = 0.6f + 0.4f * sinf(now / 60.0f);
            pzk_dot(SX(x + 0.33f * BUB_D * sc), SY(y - 0.38f * BUB_D * sc), 0.3f * BUB_D * k * fl, 0.15f * BUB_D * k,
                    (pzk_rgba){255, 190, 80, 230}, 1);
            ui_animate();
        }
    }
    if (G.s.nxt) {
        float x = nx, y = cy, sc = 0.78f;
        if (st < 1) { float e = puz_ease(st); x = cx + (nx - cx) * e; y = cy - 0.55f * BUB_D * sinf(PUZ_PI * e); sc = 1 - 0.22f * e; }
        else if (lt < 1) sc = 0.78f * puz_ease(lt);
        put_marble(G.s.nxt, x, y, sc, 1, 0);
    }
    /* the shots left */
    int left = G.L.nseq - G.s.shots;
    float px = cx + 1.75f * BUB_D;
    const uint8_t ink[3] = {70, 44, 20}, red[3] = {176, 30, 24};
    ui_number_ex(SX(px), SY(cy), 0.5f * BUB_D * k, left, lost() && G.s.end == BUB_OVER ? red : ink, 255, UI_NUM_TIGHT);
    if (lost()) {
        float g = 0.6f + 0.4f * sinf((now - lose_t0) / 200.0f);
        if (G.s.end == BUB_OVER) ui_mark_glow(SX(px), SY(cy), 1.6f * BUB_D * k * g);
        ui_animate();
    }
    if (deny_t0 && (int)(now - deny_t0) < 300) ui_animate();
}

static void draw(void)
{
    if (made_k < 0 || made_serial != serial) layout(last_box);
    lost_tick();
    Uint32 now = ui_ms();
    ui_put(t_board, ox - fr, oy - fr, (float)bw, (float)bh);
    float drop = drop_now(now);
    if (t_plate && drop > 0) {                  /* the ceiling's plank, lowered */
        int pw, ph;
        SDL_QueryTexture(t_plate, NULL, NULL, &pw, &ph);
        float hh = drop * BUB_ROWH * k;
        SDL_Rect src = {0, (int)fmaxf(0, ph - hh), pw, (int)fminf((float)ph, hh + 1)};
        SDL_FRect dst = {ox, oy, (float)pw, (float)src.h};
        SDL_RenderCopyF(ui.r, t_plate, &src, &dst);
        ui_fill(ox, oy + src.h, (float)pw, fmaxf(2, 0.12f * BUB_D * k), 0, 0, 0, 60);
    }
    draw_guide(now);
    draw_grid(now);
    if (lost() && G.s.end == BUB_LOW) {         /* what passed the line */
        float g = 0.6f + 0.4f * sinf((now - lose_t0) / 200.0f);
        for (int r = 0; r < BUB_ROWS; r++)
            for (int c = 0; c < bub_row_len(&G.L, r); c++)
                if (G.s.c[r][c] && r + G.s.drop >= G.L.rows) {
                    float x, y;
                    cell_at(r, c, G.s.drop, &x, &y);
                    ui_mark_glow(SX(x), SY(y), 1.5f * BUB_D * k * g);
                }
        ui_animate();
    }
    draw_flight(now);
    draw_launcher(now);
    draw_fx(now);
    if (win.on) {                               /* gold light swelling over the box */
        float t = (float)(int)(now - win.t0);
        if (t >= 0 && t < WIN_MS + 400) {
            float q = puz_pulse(t, 0, WIN_MS);
            int32_t lx, ly;
            bub_launcher(&G.L, &lx, &ly);
            ui_mark_gold_a(SX(lx), SY(G.L.rows * BUB_ROWH * 0.45f), (2 + 6 * q) * BUB_D * k, 0.55f * q);
            ui_mark_gold_a(SX(lx), SY(ly), (1.5f + 2 * q) * BUB_D * k, 0.8f * q);
        }
        if (t < WIN_MS + 400) ui_animate();
    }
    if (lost() && lose_t0 && (int)(now - lose_t0) >= 0) {   /* lost: the box veiled, a big restart in it, its ring
                                                 * closing round it until the level starts again by itself */
        float t = clampf((float)(int)(now - lose_t0) / LOST_MS, 0, 1), cx = SX(G.L.w * BUB_D / 2.0f);
        float cy = SY(G.L.rows * BUB_ROWH * 0.55f), d = fmaxf(2.0f * BUB_D * k, 1.6f * ui.unit), a = clampf(t * 6, 0, 1);
        ui_fill(ox, oy, G.L.w * BUB_D * k, (float)bub_field_h(&G.L) * k, 0, 0, 0, (Uint8)(90 * a));
        ui_mark_gold_a(cx, cy, d * (1.9f + 0.15f * sinf(now / 180.0f)), 0.7f * a);
        for (int i = 0; i < 24; i++) {          /* the countdown: dots round the button, going out one by one */
            float ang = -PUZ_PI / 2 + i * 2 * PUZ_PI / 24, on = (float)i / 24 >= t;
            pzk_dot(cx + cosf(ang) * 0.78f * d, cy + sinf(ang) * 0.78f * d, 0.1f * d, 1,
                    (pzk_rgba){255, 226, 140, (Uint8)(a * (on ? 255 : 60))}, 0);
        }
        ui_button(cx, cy, d, ICON_REPLAY, UI_LIT);
        ui_animate();
    }
    if (busy()) ui_animate();
    int glow_restart = lost() || (deny_t0 && (int)(now - deny_t0) < 900);
    ui_tool(T_RESTART, ICON_REPLAY, (glow_restart ? UI_LIT : G.nh ? 0 : UI_DIM) | (tool_down == T_RESTART ? UI_ON : 0));
    ui_tool(T_HINT, ICON_HINT, (won() ? UI_DIM : hint_on ? UI_LIT : 0) | (tool_down == T_HINT ? UI_ON : 0));
    if (glow_restart) ui_animate();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int on_next(float x, float y)
{
    int32_t lx, ly;
    bub_launcher(&G.L, &lx, &ly);
    float d = fmaxf(0.75f * BUB_D * k, 0.55f * ui.unit);
    return hypotf(x - SX(lx - 1.75f * BUB_D), y - SY((float)ly)) < d;
}

static int on_launcher(float x, float y)
{
    int32_t lx, ly;
    bub_launcher(&G.L, &lx, &ly);
    float d = fmaxf(0.85f * BUB_D * k, 0.6f * ui.unit);
    return hypotf(x - SX((float)lx), y - SY((float)ly)) < d;
}

enum { P_NONE, P_AIM, P_NEXT, P_LAUNCH };

static void press(float x, float y)
{
    lost_tick();
    tool_down = ui_tool_hit(x, y);
    press_on = P_NONE;
    if (tool_down >= 0) { ui_animate(); return; }
    if (G.s.end != BUB_PLAYING) {
        if (lost()) restart();                  /* lost: a tap anywhere starts again */
        return;
    }
    fx0 = x;
    fy0 = y;
    finger_moved = 0;
    if (on_next(x, y)) { press_on = P_NEXT; return; }
    if (on_launcher(x, y)) { press_on = P_LAUNCH; return; }
    int a;
    if (aim_at(x, y, &a)) {
        press_on = P_AIM;
        aim = a;
        aimk.on = 0;
        ui_animate();
    }
}

static void motion(float x, float y)
{
    if (tool_down >= 0 || press_on == P_NONE) return;
    if (hypotf(x - fx0, y - fy0) > 0.25f * ui.unit) finger_moved = 1;
    if (press_on == P_NEXT && !finger_moved) return;
    int a;
    if (aim_at(x, y, &a)) {
        press_on = P_AIM;
        if (a != aim) { aim = a; aimk.on = 0; ui_animate(); }
        finger = 1;
    } else if (press_on == P_AIM) finger = 0;   /* below the launcher: let go there, nothing shot */
}

static void release(float x, float y)
{
    if (tool_down >= 0) {
        int t = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) != t) return;
        if (t == T_RESTART) restart();
        else hint();
        return;
    }
    int p = press_on;
    press_on = P_NONE;
    if (p == P_NEXT && !finger_moved) { do_swap(); return; }
    if (p == P_LAUNCH && !finger_moved) { fire(aim, 0); return; }
    int a;
    if (p == P_AIM && aim_at(x, y, &a)) {
        aim = a;
        if (hint_on && abs(aim - hint_m.aim) < 12 + abs(hint_m.aim) / 40) aim = hint_m.aim;   /* close to the hint */
        fire(aim, 0);
    }
}

/* the keys: left and right step the aim to the next landing place that way, up shoots, down swaps */
static void key(SDL_Keycode kc)
{
    lost_tick();
    if (kc == SDLK_r) { restart(); return; }
    if (G.s.end != BUB_PLAYING) return;
    if (kc == SDLK_UP || kc == SDLK_RETURN || kc == SDLK_SPACE || kc == SDLK_KP_ENTER) { fire(aim, 0); return; }
    if (kc == SDLK_DOWN) { do_swap(); return; }
    if (kc != SDLK_LEFT && kc != SDLK_RIGHT) return;
    bub_move m[BUB_MOVES];
    int n = bub_moves(&G.L, &G.s, m, BUB_MOVES), best = -1;
    int here = bub_trace(&G.L, &G.s, aim, NULL);
    for (int i = 0; i < n; i++) {
        if (m[i].swap || m[i].cell == here) continue;
        if (kc == SDLK_LEFT ? m[i].aim < aim : m[i].aim > aim) {
            if (best < 0 || (kc == SDLK_LEFT ? m[i].aim > m[best].aim : m[i].aim < m[best].aim)) best = i;
        }
    }
    if (best < 0) { ui_sound(SND_DENY); return; }
    aimk.on = 1;
    aimk.from = aim_shown(ui_ms());
    aimk.to = m[best].aim;
    aimk.t0 = ui_ms();
    aim = m[best].aim;
    ui_sound_soft(SND_TAP, 0.4f);
    ui_animate();
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.s.shots, t_par, par(), won() && G.s.shots <= par());
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a box drawn small (pixels only: the menu's thread too): the frame, the inside, the marbles; full: the whole box
 * with its launcher and the first marble in it, else its top (a level's tile: the marbles big enough to read) */
static void paint_box(const bub_level *L, int size, uint8_t *rgba, int full)
{
    float FW = L->w * BUB_D, FH = (float)bub_field_h(L), F = FRAME * BUB_D;
    if (!full) {
        int last = 0;
        for (int r = 0; r < BUB_ROWS; r++)
            for (int c = 0; c < bub_row_len(L, r); c++)
                if (L->c[r][c]) last = r;
        FH = fmaxf(0.86f * FW, (last + 1) * BUB_ROWH + 0.9f * BUB_D);
    }
    float kk = size * 0.97f / fmaxf(FW + 2 * F, FH + 2 * F);
    int f = (int)ceilf(F * kk), W = (int)ceilf(FW * kk) + 2 * f, H = (int)ceilf(FH * kk) + 2 * f;
    uint8_t *o = W > 8 && H > 8 ? malloc((size_t)W * H * 4) : NULL;
    if (!o) return;
    paint_board(L, kk, W, H, f, full, o);
    int d = (int)ceilf(BUB_D * kk * 1.06f);
    uint8_t *m = d > 2 ? malloc((size_t)d * d * 4) : NULL;
    for (int r = 0; r < BUB_ROWS; r++)
        for (int c = 0; c < bub_row_len(L, r); c++) {
            int v = L->c[r][c];
            if (!v || !m) continue;
            int32_t x, y;
            bub_cell_xy(L, 0, r, c, &x, &y);
            paint_marble(v & 15, d, m);
            if (v & BUB_LOCK) {
                uint8_t *cg = malloc((size_t)d * d * 4);
                if (cg) { paint_cage(d, cg); art_blend(m, d, d, cg, d, d, 0, 0); free(cg); }
            }
            art_blend(o, W, H, m, d, d, (int)lroundf(f + x * kk - d / 2.0f), (int)lroundf(f + y * kk - d / 2.0f));
        }
    if (m && L->nseq && full) {                 /* the first marble in the launcher */
        int32_t lx, ly;
        bub_launcher(L, &lx, &ly);
        paint_marble(L->seq[0] == BUB_RAINBOW ? BUB_WILD : L->seq[0], d, m);
        art_blend(o, W, H, m, d, d, (int)lroundf(f + lx * kk - d / 2.0f), (int)lroundf(f + ly * kk - d / 2.0f));
    }
    free(m);
    art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    free(o);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    bub_level L;
    if (level >= 0 && level < BUB_LEVELS && bub_parse(&L, bub_levels[level])) paint_box(&L, size, rgba, 0);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    if (variant < 0) {                          /* the menu: a heart of marbles */
        bub_level L;
        if (bub_parse(&L, bub_levels[BUB_LEVELS > 7 ? 7 : 0])) paint_box(&L, size, rgba, 0);
        return;
    }
    bub_spec sp;
    bub_variant(variant, &sp);
    bub_level L;
    for (uint32_t s = 11; s < 60; s++)          /* a box of the variant (not solved: a picture only) */
        if (bub_make(&L, &sp, s + 100u * (uint32_t)variant)) {
            int n = 0;
            for (int r = 0; r < BUB_ROWS; r++)
                for (int c = 0; c < BUB_MAXW; c++) n += L.c[r][c] != 0;
            if (n >= 14 + 4 * variant) break;
        }
    paint_box(&L, size, rgba, 0);
}

/* a chapter's tab: three marbles of a colour; a gem; a stone by two; a bomb by a rainbow; a locked marble; the
 * ceiling's plank over a marble */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int d = (int)(0.56f * size), big = (int)(0.86f * size);
    uint8_t *m = big > 2 ? malloc((size_t)big * big * 4) : NULL;
    if (!m) return;
#define PUT(kind, dd, fx, fy) do { paint_marble(kind, dd, m); art_blend(rgba, size, size, m, dd, dd, (int)((fx) * size - (dd) / 2.0f), (int)((fy) * size - (dd) / 2.0f)); } while (0)
    switch (chapter) {
    case 0: PUT(1, d, 0.3f, 0.34f); PUT(1, d, 0.72f, 0.34f); PUT(1, d, 0.51f, 0.71f); break;
    case 1: PUT(BUB_GEM, big, 0.5f, 0.52f); break;
    case 2: PUT(BUB_STONE, d, 0.3f, 0.34f); PUT(2, d, 0.72f, 0.34f); PUT(2, d, 0.51f, 0.71f); break;
    case 3: PUT(BUB_WILD, d, 0.68f, 0.62f); PUT(BUB_BOMB, d, 0.34f, 0.4f); break;
    case 4:
        PUT(4, big, 0.5f, 0.52f);
        paint_cage(big, m);
        art_blend(rgba, size, size, m, big, big, (int)(0.5f * size - big / 2.0f), (int)(0.52f * size - big / 2.0f));
        break;
    default: {
        int pw = (int)(0.86f * size), ph = (int)(0.3f * size);
        uint8_t *p = malloc((size_t)pw * ph * 4);
        if (p) {
            bub_level L = {.w = 8, .rows = 12};
            paint_plate(&L, ph / (0.5f * BUB_D), pw, ph, p);
            art_blend(rgba, size, size, p, pw, ph, (size - pw) / 2, (int)(0.08f * size));
            free(p);
        }
        PUT(6, d, 0.5f, 0.66f);
        break;
    }
    }
#undef PUT
    free(m);
}

static const game bubbles_game = {
    .id = "bubbles", .order = 463, .variants = BUB_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = BUB_LEVELS, .chapters = BUB_CHAPTERS, .chapter_of = bub_chapter_of, .start_random = start_random,
    .chapter_icon = chapter_icon, .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par,
    .fill = 1, .tools = T_COUNT,
};
REGISTER_GAME(bubbles_game)
