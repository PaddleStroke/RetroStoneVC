/*
 * The Game of the Goose in RetroStone Board, for two to six (the rules: goose_rules.c, goose.h). The board is a plank
 * of the house wood with a spiral of 62 squares inlaid in pale wood, running in from the start (the inlaid corner at
 * the bottom left, where the pawns wait) to the goose's garden in the middle (63). The squares are drawn: a goose on
 * each goose, the bridge, the inn, the well, the maze, the prison, death's skull and crossbones, the dice of the
 * opening throws on 26 and 53; marquetry stars in the other corners. Numbers only, no words.
 * Nothing is secret (VIEW_OPEN): the view follows the player to throw, their bar at the bottom. The two dice wait
 * beside the board in their colour, in a gold light: a tap anywhere throws them. The pawn then hops square by square,
 * flies on from a goose, slides along the bridge or back through the maze, flies back to the start from death; a pawn
 * it lands on flies to where it came from. Each bar: the square the pawn is on, and what holds it (the inn and how many
 * turns, the well, the prison).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Coordinates: fractions of the board's side, from its top left. The spiral: its centre line at radius
 * R_OUT - PITCH t / 2 pi after turning t radians (anticlockwise on the screen) from the bottom left; the squares share
 * its length equally.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/chess.h"
#include "games/dicecup.h"
#include "games/goose.h"
#include "shell.h"

#define FR 0.035f                               /* the walnut frame */
#define PITCH 0.084f                            /* one turn of the spiral to the next: a square's width */
#define R_OUT 0.418f                            /* the outer turn's centre line */
#define R_END 0.132f                            /* the inner end's centre line */
#define RG (R_END - PITCH / 2)                  /* the garden */
#define PHI0 (0.75f * RACE_PI)                  /* where the spiral starts: towards the bottom left corner */
#define CELLS 62
#define SPN 4096                                /* the spiral's length table */
#define PAWN_SZ 0.82f                           /* a pawn's size, of a square */

#define HOP_MS 180
#define PAUSE_MS 170                            /* on a goose, the bridge... before it acts */
#define GLIDE_MS 95                             /* a square's length, gliding */
#define FLY_MS 720
#define SWAP_MS 650
#define REST_MS 220

static goo_game G;
static int S, made = -1, made_icon = -1;
static float bx, by, dcx, dcy, dsize;           /* the board; the dice (centre, a die's size) */
static SDL_Texture *t_board, *t_pawn[GOO_PLAYERS], *t_icon[3];   /* the bars' icons: the inn, the well, the prison */
static struct { int on, hops; Uint32 t0; goo_move m; int total, sounded; } anim;

/* ---- the spiral -------------------------------------------------------------------------------------------------- */
static float sp_T, sp_L, sp_s[SPN + 1];

static float sp_r(float t) { return R_OUT - PITCH * t / (2 * RACE_PI); }

static void sp_init(void)
{
    if (sp_T > 0) return;
    sp_T = 2 * RACE_PI * (R_OUT - R_END) / PITCH;
    float k = PITCH / (2 * RACE_PI), dt = sp_T / SPN;
    sp_s[0] = 0;
    for (int i = 1; i <= SPN; i++) {
        float r = sp_r((i - 0.5f) * dt);
        sp_s[i] = sp_s[i - 1] + sqrtf(r * r + k * k) * dt;
    }
    sp_L = sp_s[SPN];
}

static float sp_s_of(float t)
{
    float f = clampf(t / sp_T, 0, 1) * SPN;
    int i = (int)f;
    if (i >= SPN) return sp_L;
    return sp_s[i] + (sp_s[i + 1] - sp_s[i]) * (f - i);
}

static float sp_t_of(float s)
{
    int lo = 0, hi = SPN;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (sp_s[mid] < s) lo = mid;
        else hi = mid;
    }
    float f = (s - sp_s[lo]) / fmaxf(1e-9f, sp_s[hi] - sp_s[lo]);
    return (lo + clampf(f, 0, 1)) * sp_T / SPN;
}

/* a point of the spiral: t along, off across (outwards) */
static void sp_at(float t, float off, float *x, float *y)
{
    float r = sp_r(t) + off, a = PHI0 - t;
    *x = 0.5f + r * cosf(a);
    *y = 0.5f + r * sinf(a);
}

static float cell_t(int sq) { return sp_t_of((sq - 0.5f) * sp_L / CELLS); }

/* the band under a point: 1, t along it and the offset across (outwards) */
static int sp_find(float x, float y, float *t, float *off)
{
    float dx = x - 0.5f, dy = y - 0.5f, rho = hypotf(dx, dy), base = PHI0 - atan2f(dy, dx);
    base = fmodf(base, 2 * RACE_PI);
    if (base < 0) base += 2 * RACE_PI;
    for (int k = 0; k < 6; k++) {
        float tt = base + 2 * RACE_PI * k;
        if (tt > sp_T) break;
        float o = rho - sp_r(tt);
        if (fabsf(o) <= PITCH / 2) {
            *t = tt;
            *off = o;
            return 1;
        }
    }
    return 0;
}

/* each player's place at the start, in the corner: two or three players big, four to six smaller */
static void yard(int player, float *x, float *y, float *size)
{
    static const float BIG[3][2] = {{0.088f, 0.915f}, {0.205f, 0.925f}, {0.082f, 0.795f}};
    static const float SMALL[6][2] = {{0.072f, 0.935f}, {0.152f, 0.938f}, {0.232f, 0.942f},
                                      {0.070f, 0.855f}, {0.150f, 0.858f}, {0.068f, 0.775f}};
    int p = player < 0 ? 0 : player % GOO_PLAYERS;
    if (G.players <= 3) *x = BIG[p % 3][0], *y = BIG[p % 3][1], *size = PAWN_SZ;
    else *x = SMALL[p][0], *y = SMALL[p][1], *size = 0.72f;
}

/* where a player's pawn stands on a square (its foot): the start (its own place), the spiral, the garden */
static void square_at(int sq, int player, float *x, float *y, float *size)
{
    *size = PAWN_SZ;
    if (sq <= 0) yard(player, x, y, size);
    else if (sq >= GOO_LAST) *x = 0.5f, *y = 0.53f, *size = 1.1f;
    else sp_at(cell_t(sq), 0, x, y);
}

/* ---- the pictures: figures in a unit square -------------------------------------------------------------------- */
typedef struct { art_fig *f; art_style st; } layer;
enum { FIG_GOOSE, FIG_BRIDGE, FIG_INN, FIG_WELL, FIG_MAZE, FIG_PRISON, FIG_DEATH, FIGS };
#define LAYERS 4

static art_style flat(const uint8_t rgb[3], const uint8_t ink[3], const uint8_t mark[3], float light)
{
    art_style st = art_flat(rgb, ink, 0.028f, light);
    for (int c = 0; c < 3; c++) {
        st.inner[c] = (rgb[c] + ink[c]) / 2.0f;
        st.mark[c] = mark[c];
    }
    return st;
}

static void poly(art_fig *f, int role, int n, const float *xy) { art_poly(f, role, xy, n); }

/* a figure's layers, back to front; how many */
static int figure(int kind, layer *L)
{
    static const uint8_t white[3] = {250, 248, 240}, grey_ink[3] = {74, 64, 54}, eye[3] = {28, 24, 20};
    static const uint8_t orange[3] = {238, 148, 42}, orange_ink[3] = {120, 62, 18};
    static const uint8_t wood[3] = {178, 120, 70}, wood_ink[3] = {78, 48, 26};
    static const uint8_t stone[3] = {168, 162, 150}, stone_ink[3] = {72, 68, 62}, dark[3] = {36, 32, 30};
    static const uint8_t roof[3] = {176, 64, 44}, roof_ink[3] = {84, 28, 18};
    static const uint8_t water[3] = {86, 140, 192}, water_ink[3] = {40, 70, 110}, foam[3] = {214, 234, 246};
    static const uint8_t wall[3] = {242, 228, 192}, lamp[3] = {255, 214, 96}, lamp_ink[3] = {130, 90, 30};
    static const uint8_t hedge[3] = {84, 136, 62}, hedge_ink[3] = {34, 66, 26}, sand[3] = {232, 212, 160};
    static const uint8_t iron[3] = {98, 100, 108}, iron_ink[3] = {30, 30, 34};
    static const uint8_t bone[3] = {238, 230, 208}, bone_ink[3] = {84, 72, 56};
    int n = 0;
    for (int i = 0; i < LAYERS; i++) L[i].f = NULL;
    switch (kind) {
    case FIG_GOOSE: {                           /* a white goose walking left: body, tail, neck, head, wing; beak and
                                                 * feet orange */
        art_fig *f = art_fig_new();
        L[n].f = f, L[n++].st = flat(orange, orange_ink, eye, 0.3f);
        poly(f, ART_PART, 4, (const float[]){0.47f, 0.76f, 0.52f, 0.76f, 0.50f, 0.92f, 0.45f, 0.92f});
        poly(f, ART_PART, 4, (const float[]){0.60f, 0.76f, 0.65f, 0.76f, 0.66f, 0.92f, 0.61f, 0.92f});
        poly(f, ART_PART, 3, (const float[]){0.40f, 0.95f, 0.52f, 0.90f, 0.53f, 0.95f});
        poly(f, ART_PART, 3, (const float[]){0.55f, 0.95f, 0.67f, 0.90f, 0.68f, 0.95f});
        f = art_fig_new();
        L[n].f = f, L[n++].st = flat(white, grey_ink, eye, 0.35f);
        poly(f, ART_PART, 4, (const float[]){0.74f, 0.52f, 0.95f, 0.38f, 0.90f, 0.58f, 0.78f, 0.70f});
        art_ellipse(f, ART_PART, 0.56f, 0.62f, 0.30f, 0.18f);
        poly(f, ART_PART, 6, (const float[]){0.27f, 0.62f, 0.22f, 0.40f, 0.21f, 0.24f, 0.33f, 0.24f, 0.36f, 0.42f,
                                             0.43f, 0.60f});
        art_ellipse(f, ART_PART, 0.27f, 0.22f, 0.085f, 0.08f);
        poly(f, ART_PART, 5, (const float[]){0.48f, 0.56f, 0.62f, 0.50f, 0.80f, 0.54f, 0.72f, 0.66f, 0.54f, 0.66f});
        art_ellipse(f, ART_MARK, 0.255f, 0.20f, 0.02f, 0.02f);
        f = art_fig_new();
        L[n].f = f, L[n++].st = flat(orange, orange_ink, eye, 0.3f);
        poly(f, ART_PART, 4, (const float[]){0.20f, 0.20f, 0.07f, 0.24f, 0.08f, 0.27f, 0.20f, 0.27f});
        break;
    }
    case FIG_BRIDGE: {                          /* a wooden arch over the water, its rail and posts */
        art_fig *f = art_fig_new();
        L[n].f = f, L[n++].st = flat(water, water_ink, foam, 0.15f);
        art_rect(f, ART_PART, 0.04f, 0.70f, 0.96f, 0.94f);
        for (int k = 0; k < 3; k++) {
            float x = 0.12f + 0.28f * k, y = 0.80f + 0.06f * (k & 1);
            art_rect(f, ART_MARK, x, y, x + 0.16f, y + 0.025f);
        }
        f = art_fig_new();
        L[n].f = f, L[n++].st = flat(wood, wood_ink, wood_ink, 0.35f);
        art_arc(f, ART_PART, 0.5f, 0.86f, 0.26f, 0.38f, 0, 180);
        art_rect(f, ART_PART, 0.03f, 0.44f, 0.97f, 0.52f);
        art_rect(f, ART_PART, 0.03f, 0.48f, 0.15f, 0.86f);
        art_rect(f, ART_PART, 0.85f, 0.48f, 0.97f, 0.86f);
        for (int k = 0; k < 6; k++) {
            float x = 0.06f + 0.172f * k;
            art_rect(f, ART_PART, x, 0.30f, x + 0.05f, 0.45f);
        }
        art_rect(f, ART_PART, 0.03f, 0.27f, 0.97f, 0.32f);
        break;
    }
    case FIG_INN: {                             /* a house: walls, a door, lit windows, a roof, its chimney, a lamp */
        art_fig *f = art_fig_new();
        L[n].f = f, L[n++].st = flat(roof, roof_ink, roof_ink, 0.3f);
        art_rect(f, ART_PART, 0.64f, 0.14f, 0.74f, 0.36f);
        f = art_fig_new();
        L[n].f = f, L[n++].st = flat(wall, wood_ink, wood_ink, 0.25f);
        art_rect(f, ART_PART, 0.18f, 0.44f, 0.82f, 0.90f);
        art_rect(f, ART_MARK, 0.42f, 0.62f, 0.58f, 0.90f);
        f = art_fig_new();
        L[n].f = f, L[n++].st = flat(roof, roof_ink, roof_ink, 0.3f);
        poly(f, ART_PART, 3, (const float[]){0.08f, 0.48f, 0.50f, 0.12f, 0.92f, 0.48f});
        f = art_fig_new();
        L[n].f = f, L[n++].st = flat(lamp, lamp_ink, lamp_ink, 0.2f);
        art_rect(f, ART_PART, 0.24f, 0.56f, 0.36f, 0.70f);
        art_rect(f, ART_PART, 0.64f, 0.56f, 0.76f, 0.70f);
        art_ellipse(f, ART_PART, 0.50f, 0.33f, 0.07f, 0.07f);
        break;
    }
    case FIG_WELL: {                            /* stones round the dark water, two posts, a roof, a bucket */
        art_fig *f = art_fig_new();
        L[n].f = f, L[n++].st = flat(wood, wood_ink, wood_ink, 0.35f);
        art_rect(f, ART_PART, 0.22f, 0.20f, 0.29f, 0.62f);
        art_rect(f, ART_PART, 0.71f, 0.20f, 0.78f, 0.62f);
        art_rect(f, ART_PART, 0.22f, 0.30f, 0.78f, 0.34f);
        art_rect(f, ART_MARK, 0.493f, 0.34f, 0.507f, 0.44f);
        poly(f, ART_PART, 4, (const float[]){0.43f, 0.43f, 0.57f, 0.43f, 0.55f, 0.53f, 0.45f, 0.53f});
        f = art_fig_new();
        L[n].f = f, L[n++].st = flat(stone, stone_ink, dark, 0.35f);
        art_rect(f, ART_PART, 0.18f, 0.58f, 0.82f, 0.90f);
        art_ellipse(f, ART_PART, 0.50f, 0.90f, 0.32f, 0.05f);
        art_ellipse(f, ART_PART, 0.50f, 0.58f, 0.32f, 0.09f);
        art_ellipse(f, ART_MARK, 0.50f, 0.58f, 0.25f, 0.055f);
        for (int k = 0; k < 4; k++) art_rect(f, ART_MARK, 0.20f + 0.16f * k, 0.70f, 0.215f + 0.16f * k, 0.80f);
        f = art_fig_new();
        L[n].f = f, L[n++].st = flat(roof, roof_ink, roof_ink, 0.3f);
        poly(f, ART_PART, 3, (const float[]){0.12f, 0.26f, 0.50f, 0.06f, 0.88f, 0.26f});
        break;
    }
    case FIG_MAZE: {                            /* a square of hedge, sand paths winding in it */
        art_fig *f = art_fig_new();
        L[n].f = f, L[n++].st = flat(hedge, hedge_ink, sand, 0.25f);
        art_rect(f, ART_PART, 0.10f, 0.10f, 0.90f, 0.90f);
        static const float paths[][4] = {
            {0.10f, 0.44f, 0.26f, 0.52f}, {0.20f, 0.20f, 0.26f, 0.80f}, {0.20f, 0.20f, 0.66f, 0.26f},
            {0.74f, 0.20f, 0.80f, 0.80f}, {0.32f, 0.74f, 0.80f, 0.80f}, {0.32f, 0.34f, 0.38f, 0.74f},
            {0.32f, 0.32f, 0.68f, 0.38f}, {0.62f, 0.32f, 0.68f, 0.68f}, {0.44f, 0.62f, 0.68f, 0.68f},
            {0.44f, 0.44f, 0.50f, 0.62f}, {0.44f, 0.44f, 0.56f, 0.50f}, {0.66f, 0.20f, 0.80f, 0.26f},
        };
        for (int k = 0; k < 12; k++) art_rect(f, ART_MARK, paths[k][0], paths[k][1], paths[k][2], paths[k][3]);
        break;
    }
    case FIG_PRISON: {                          /* a barred window in a stone wall */
        art_fig *f = art_fig_new();
        L[n].f = f, L[n++].st = flat(stone, stone_ink, dark, 0.3f);
        art_rect(f, ART_PART, 0.12f, 0.12f, 0.88f, 0.88f);
        art_rect(f, ART_MARK, 0.25f, 0.24f, 0.75f, 0.80f);
        f = art_fig_new();
        L[n].f = f, L[n++].st = flat(iron, iron_ink, iron_ink, 0.45f);
        for (int k = 0; k < 4; k++) art_rect(f, ART_PART, 0.30f + 0.12f * k, 0.22f, 0.35f + 0.12f * k, 0.82f);
        art_rect(f, ART_PART, 0.23f, 0.47f, 0.77f, 0.53f);
        break;
    }
    case FIG_DEATH: {                           /* a skull over crossed bones */
        art_fig *f = art_fig_new();
        L[n].f = f, L[n++].st = flat(bone, bone_ink, dark, 0.3f);
        poly(f, ART_PART, 4, (const float[]){0.16f, 0.78f, 0.20f, 0.72f, 0.84f, 0.92f, 0.80f, 0.98f});
        poly(f, ART_PART, 4, (const float[]){0.80f, 0.72f, 0.84f, 0.78f, 0.20f, 0.98f, 0.16f, 0.92f});
        art_ellipse(f, ART_PART, 0.13f, 0.73f, 0.05f, 0.05f);
        art_ellipse(f, ART_PART, 0.87f, 0.73f, 0.05f, 0.05f);
        art_ellipse(f, ART_PART, 0.13f, 0.97f, 0.05f, 0.05f);
        art_ellipse(f, ART_PART, 0.87f, 0.97f, 0.05f, 0.05f);
        f = art_fig_new();
        L[n].f = f, L[n++].st = flat(bone, bone_ink, dark, 0.35f);
        poly(f, ART_PART, 6, (const float[]){0.34f, 0.56f, 0.66f, 0.56f, 0.64f, 0.80f, 0.58f, 0.84f, 0.42f, 0.84f,
                                             0.36f, 0.80f});
        art_ellipse(f, ART_PART, 0.50f, 0.40f, 0.29f, 0.27f);
        art_ellipse(f, ART_MARK, 0.39f, 0.44f, 0.08f, 0.09f);
        art_ellipse(f, ART_MARK, 0.61f, 0.44f, 0.08f, 0.09f);
        poly(f, ART_MARK, 3, (const float[]){0.50f, 0.54f, 0.46f, 0.62f, 0.54f, 0.62f});
        art_rect(f, ART_MARK, 0.38f, 0.70f, 0.62f, 0.72f);
        for (int k = 0; k < 3; k++) art_rect(f, ART_MARK, 0.43f + 0.07f * k, 0.70f, 0.445f + 0.07f * k, 0.80f);
        break;
    }
    }
    return n;
}

/* a figure painted over a picture (pw x ph), w pixels wide, its top left at (x, y), with a soft drop shadow */
static void paint_figure(uint8_t *o, int pw, int ph, int kind, int x, int y, int w)
{
    layer L[LAYERS];
    int n = figure(kind, L);
    if (w < 4) {
        for (int i = 0; i < n; i++) art_fig_free(L[i].f);
        return;
    }
    uint8_t *b = calloc((size_t)w * w, 4), *one = malloc((size_t)w * w * 4);
    if (!b || !one) {
        free(b), free(one);
        for (int i = 0; i < n; i++) art_fig_free(L[i].f);
        return;
    }
    for (int i = 0; i < n; i++) {
        L[i].st.line = fmaxf(L[i].st.line, 1.1f / w);
        art_render(L[i].f, &L[i].st, w, w, (float)w, 0, 0, one);
        art_blend(b, w, w, one, w, w, 0, 0);
        art_fig_free(L[i].f);
    }
    for (int i = 0; i < w * w; i++)             /* the shadow: its shape, dark, a third */
        one[4 * i] = one[4 * i + 1] = one[4 * i + 2] = 20, one[4 * i + 3] = b[4 * i + 3] / 3;
    int d = w / 28 + 1;
    art_blend(o, pw, ph, one, w, w, x + d, y + d);
    art_blend(o, pw, ph, b, w, w, x, y);
    free(b);
    free(one);
}

/* an eight-pointed marquetry star, two woods, centred on (cx, cy) pixels, radius r */
static void star(uint8_t *o, int n, float cx, float cy, float r)
{
    static const uint8_t light[3] = {236, 196, 136}, dark_[3] = {120, 74, 40}, ink[3] = {70, 44, 24};
    int w = (int)(2 * r) + 2;
    for (int pass = 0; pass < 2; pass++) {
        art_fig *f = art_fig_new();
        for (int k = 0; k < 8; k++) {           /* a kite each: the centre, a waist, the tip, the other waist */
            if ((k & 1) != pass) continue;
            float a = k * RACE_PI / 4, a1 = a + RACE_PI / 8, a2 = a - RACE_PI / 8, ri = 0.2f;
            float tip = pass ? 0.34f : 0.48f;
            float p[8] = {0.5f, 0.5f, 0.5f + ri * cosf(a2), 0.5f + ri * sinf(a2), 0.5f + tip * cosf(a),
                          0.5f + tip * sinf(a), 0.5f + ri * cosf(a1), 0.5f + ri * sinf(a1)};
            art_poly(f, ART_PART, p, 4);
        }
        art_style st = art_flat(pass ? dark_ : light, ink, 0.012f, 0.25f);
        art_fig_blend(o, n, n, f, &st, (int)(cx - w / 2.0f), (int)(cy - w / 2.0f), w, 0.85f);
        art_fig_free(f);
    }
}

/* the garden (63): a lawn with flowers in a gold ring, a big goose */
static void garden(uint8_t *o, int n)
{
    float cx = 0.5f * n, cy = 0.5f * n, R = RG * n, rim = fmaxf(2, 0.012f * n);
    for (int y = (int)(cy - R) - 1; y <= (int)(cy + R) + 1; y++)
        for (int x = (int)(cx - R) - 1; x <= (int)(cx + R) + 1; x++) {
            if (x < 0 || y < 0 || x >= n || y >= n) continue;
            float d = hypotf(x + 0.5f - cx, y + 0.5f - cy), in = clampf(R - d + 0.5f, 0, 1);
            if (in <= 0) continue;
            uint8_t *p = o + 4 * ((size_t)y * n + x);
            float c[3];
            if (d > R - rim) {                  /* the gold ring, lit from the upper left */
                float lit = 0.85f + 0.3f * ((cx - x) + (cy - y)) / (2 * R);
                c[0] = 214 * lit, c[1] = 170 * lit, c[2] = 78 * lit;
            } else {
                float k = (1 - 0.35f * powf(d / R, 3)) *
                          (1 + 0.08f * art_noise(x, y) + 0.05f * art_noise(x / 3, y / 3));
                c[0] = 82 * k, c[1] = 142 * k, c[2] = 66 * k;
            }
            for (int ch = 0; ch < 3; ch++) p[ch] = (uint8_t)clampf(p[ch] + (c[ch] - p[ch]) * in, 0, 255);
        }
    static const uint8_t petals[4][3] = {{232, 84, 76}, {250, 226, 96}, {246, 244, 240}, {190, 130, 226}};
    static const uint8_t heart[3] = {250, 200, 60}, ink[3] = {80, 60, 40};
    for (int k = 0; k < 8; k++) {               /* the flowers, round the goose (the top left free: its number) */
        float a = RACE_PI * (-0.18f + 1.3f * k / 7), r = (0.72f + 0.1f * (k & 1)) * (R - rim);
        int w = (int)(0.24f * R) + 2;
        art_fig *f = art_fig_new();
        for (int j = 0; j < 5; j++) {
            float b = j * 2 * RACE_PI / 5;
            art_ellipse(f, ART_PART, 0.5f + 0.24f * cosf(b), 0.5f + 0.24f * sinf(b), 0.2f, 0.2f);
        }
        art_style st = art_flat(petals[k % 4], ink, 0.04f, 0.2f);
        art_fig_blend(o, n, n, f, &st, (int)(cx + r * cosf(a) - w / 2.0f), (int)(cy + r * sinf(a) - w / 2.0f), w, 1);
        art_fig_free(f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.14f, 0.14f);
        st = art_flat(heart, ink, 0.04f, 0.3f);
        art_fig_blend(o, n, n, f, &st, (int)(cx + r * cosf(a) - w / 2.0f), (int)(cy + r * sinf(a) - w / 2.0f), w, 1);
        art_fig_free(f);
    }
    int w = (int)(1.15f * R);
    paint_figure(o, n, n, FIG_GOOSE, (int)(cx - w / 2.0f), (int)(cy - w * 0.62f), w);
}

/* the board: the plank, the spiral inlaid, the start's corner, the stars, the garden, the figures and dice */
static void board_pic(int n, uint8_t *o, int detail)
{
    sp_init();
    int fr = (int)(FR * n);
    art_board_n(1, 1, n - 2 * fr, fr, 0, o);
    float seam = fmaxf(1.4f, 0.0032f * n), Lc = sp_L / CELLS;
    float corner_x = FR, corner_y = 1 - FR, yard_r = 0.30f;
    for (int y = fr; y < n - fr; y++)
        for (int x = fr; x < n - fr; x++) {
            float fx = (x + 0.5f) / n, fy = (y + 0.5f) / n, t, off, tone[3] = {0.80f, 0.76f, 0.72f}, d = 1e9f;
            float rho = hypotf(fx - 0.5f, fy - 0.5f);
            if (rho < RG) continue;             /* the garden: painted after */
            if (sp_find(fx, fy, &t, &off)) {
                float s = sp_s_of(t);
                int c = (int)(s / Lc) + 1;
                c = c < 1 ? 1 : c > CELLS ? CELLS : c;
                float along = fminf(s - (c - 1) * Lc, c * Lc - s) * rho / sp_r(t);
                d = fminf(along, PITCH / 2 - fabsf(off)) * n;
                int kind = goo_kind(c);
                static const float A[3] = {1.13f, 1.09f, 1.02f}, B[3] = {1.04f, 0.99f, 0.92f};
                static const float GOOSE_T[3] = {1.20f, 1.18f, 1.12f}, HAZ[3] = {1.08f, 0.95f, 0.88f};
                const float *k = kind == GK_GOOSE ? GOOSE_T
                                 : kind == GK_PLAIN || kind == GK_DICE ? (c & 1 ? A : B) : HAZ;
                memcpy(tone, k, sizeof tone);
            } else {
                float dy = hypotf(fx - corner_x, fy - corner_y);
                if (dy < yard_r && rho > R_OUT + PITCH / 2) {   /* the start's corner, inlaid */
                    tone[0] = 1.06f, tone[1] = 1.00f, tone[2] = 0.92f;
                    d = fminf(yard_r - dy, rho - R_OUT - PITCH / 2) * n;
                }
            }
            float k = d < seam ? 0.55f + 0.45f * clampf(d / seam, 0, 1) : 1;
            uint8_t *p = o + 4 * ((size_t)y * n + x);
            for (int ch = 0; ch < 3; ch++) p[ch] = (uint8_t)clampf(p[ch] * tone[ch] * k, 0, 255);
        }
    garden(o, n);
    star(o, n, 0.115f * n, 0.115f * n, 0.075f * n);
    star(o, n, 0.885f * n, 0.115f * n, 0.075f * n);
    star(o, n, 0.885f * n, 0.885f * n, 0.075f * n);
    if (!detail) return;
    for (int c = 1; c < GOO_LAST; c++) {         /* the squares' pictures, a little inward of the number */
        int kind = goo_kind(c), fig = kind == GK_GOOSE ? FIG_GOOSE : kind == GK_BRIDGE ? FIG_BRIDGE
                                     : kind == GK_INN ? FIG_INN : kind == GK_WELL ? FIG_WELL
                                     : kind == GK_MAZE ? FIG_MAZE : kind == GK_PRISON ? FIG_PRISON
                                     : kind == GK_DEATH ? FIG_DEATH : -1;
        float x, y;
        sp_at(cell_t(c), -0.08f * PITCH, &x, &y);
        if (fig >= 0) {
            int w = (int)(0.66f * PITCH * n);
            paint_figure(o, n, n, fig, (int)(x * n - w / 2.0f), (int)(y * n - w / 2.0f), w);
        } else if (kind == GK_DICE) {           /* 26: 6 and 3; 53: 5 and 4 */
            int w = (int)(0.3f * PITCH * n);
            art_die_blend(o, n, n, x * n - 0.6f * w, y * n, w, 0, c == 26 ? 6 : 5, -12);
            art_die_blend(o, n, n, x * n + 0.6f * w, y * n + 0.1f * w, w, 0, c == 26 ? 3 : 4, 9);
        }
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int variant)
{
    (void)variant;
    goo_new(&G, ui.players);
    anim.on = 0;
}

static int status(void)
{
    int s = goo_status(&G);
    return s == GOO_PLAY ? GS_PLAY : gs_win(s - GOO_WIN0);
}

static int turn(void) { return goo_turn(&G); }

/* the moving pawn's timeline: when each step starts (ms after the dice settle), the pause before it, its length */
static int step_pause(const goo_move *m, int i)
{
    int n = m->d1 + m->d2;
    if (i == 0) return 0;
    if (m->step[i].how != GOO_HOP) return PAUSE_MS;
    return i % n == 0 ? PAUSE_MS : 0;           /* a goose's flight again */
}

static int step_ms(const goo_move *m, int i)
{
    int from = i ? m->step[i - 1].sq : m->from, to = m->step[i].sq;
    switch (m->step[i].how) {
    case GOO_GLIDE: return 260 + GLIDE_MS * abs(to - from);
    case GOO_FLY: return FLY_MS;
    default: return HOP_MS;
    }
}

static int move_ms(const goo_move *m)
{
    int t = 0;
    for (int i = 0; i < m->n; i++) t += step_pause(m, i) + step_ms(m, i);
    if (m->swap >= 0) t += PAUSE_MS + SWAP_MS;
    return t;
}

static int busy(void)
{
    if (ui_dice_busy()) return 1;
    if (!anim.on) return 0;
    return (int)(ui_ms() - anim.t0) < anim.total + REST_MS;
}

static void throw_dice(void)
{
    int d[2];
    if (!goo_next(&G, &d[0], &d[1])) d[0] = 1 + ui_random(6), d[1] = 1 + ui_random(6);
    int p = goo_turn(&G);
    ui_dice_throw(2, d, p);
    goo_play(&G, d[0], d[1]);
    anim.m = G.last;
    anim.on = 1;
    anim.t0 = ui_ms() + UI_DICE_MS;         /* its way starts once the dice lie still */
    anim.total = move_ms(&anim.m);
    anim.hops = anim.sounded = 0;
    ui_handoff(goo_turn(&G));                    /* once the pawn has landed */
}

static int undo(void)
{
    if (!goo_undo(&G)) return 0;
    anim.on = 0;
    if (G.n) {                                  /* the dice show the throw before */
        int d[2] = {G.dice[G.n - 1][0], G.dice[G.n - 1][1]};
        ui_dice_show(2, d, G.who[G.n - 1]);
    }
    ui_handoff(goo_turn(&G));
    return 1;
}

static int script(const char *s)
{
    int ok = goo_script(&G, s);
    anim.on = 0;
    if (G.n) {
        int d[2] = {G.dice[G.n - 1][0], G.dice[G.n - 1][1]};
        ui_dice_show(2, d, G.who[G.n - 1]);
    }
    ui_handoff(goo_turn(&G));
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    float u = ui.unit;
    if (b.h >= b.w * 1.12f) {                   /* portrait: the board, the dice under it */
        S = (int)fminf(b.w, b.h - 1.7f * u);
        bx = floorf(b.x + (b.w - S) / 2);
        by = floorf(b.y);
        dcx = b.x + b.w / 2;
        dcy = (by + S + b.y + b.h) / 2;
    } else {                                    /* landscape: the dice on its right */
        S = (int)fminf(b.h, b.w - 2.3f * u);
        bx = floorf(b.x);
        by = floorf(b.y + (b.h - S) / 2);
        dcx = (bx + S + b.x + b.w) / 2;
        dcy = b.y + b.h / 2;
    }
    dsize = fminf(0.95f * u, 0.4f * (b.x + b.w - (bx + S)) + (b.h >= b.w * 1.12f ? 9 * u : 0));
    if (made != S) {
        made = S;
        board_pic(S, ui_buf(S, S), 1);
        ui_free(&t_board);
        t_board = ui_tex(S, S, ui_buf(S, S));
        int p = (int)(PAWN_SZ * 1.3f * PITCH * S);
        for (int q = 0; q < GOO_PLAYERS; q++) {
            ui_free(&t_pawn[q]);
            art_piece(PAWN, q, p, ui_buf(p, p));
            t_pawn[q] = ui_tex(p, p, ui_buf(p, p));
        }
    }
    int ic = (int)(0.62f * u);
    if (made_icon != ic) {
        made_icon = ic;
        static const int figs[3] = {FIG_INN, FIG_WELL, FIG_PRISON};
        for (int k = 0; k < 3; k++) {
            uint8_t *o = ui_buf(ic, ic);
            memset(o, 0, (size_t)ic * ic * 4);
            paint_figure(o, ic, ic, figs[k], 0, 0, ic);
            ui_free(&t_icon[k]);
            t_icon[k] = ui_tex(ic, ic, o);
        }
    }
}

/* a pawn standing at (x, y) (its foot, fractions), size: of a square */
static void pawn(int p, float x, float y, float size)
{
    float w = size * 1.3f * PITCH * S, left = bx + x * S - w / 2, top = by + y * S - 0.62f * w;
    ui_shadow(left, top, w);
    ui_put(t_pawn[p], left, top, w, w);
}

/* where the moving pawn is now (its foot, fractions) and how high it hops; 0: its way is over (it stands on m->to) */
static int moving(float *x, float *y, float *lift, int *at)
{
    const goo_move *m = &anim.m;
    float s0;
    *lift = 0;
    *at = m->from;
    square_at(m->from, m->player, x, y, &s0);
    int t = (int)(ui_ms() - anim.t0), from = m->from;
    if (t < 0) return 1;                        /* the dice still tumble */
    for (int i = 0; i < m->n; i++) {
        int pause = step_pause(m, i), len = step_ms(m, i), to = m->step[i].sq, how = m->step[i].how;
        if (t < pause) {                        /* resting on a goose, the bridge... before it acts */
            square_at(from, m->player, x, y, &s0);
            *at = from;
            return 1;
        }
        t -= pause;
        if (t < len) {
            float f = t / (float)len, x0, y0, x1, y1, e = race_ease(f);
            square_at(from, m->player, &x0, &y0, &s0);
            square_at(to, m->player, &x1, &y1, &s0);
            if (anim.sounded < i + 1) {         /* each step's sound once */
                anim.sounded = i + 1;
                if (how == GOO_FLY) ui_sound(to == 0 ? SND_CAPTURE : SND_CHECK);
                else if (how == GOO_GLIDE) ui_sound(to < from ? SND_CAPTURE : SND_CHECK);
                else {
                    if (step_pause(m, i)) ui_sound(SND_CHECK);   /* a goose: off again */
                    ui_sound_later(SND_MOVE, len > 40 ? len - 40 : 0);
                }
            }
            if (how == GOO_GLIDE) {              /* along the spiral */
                float t0 = cell_t(from), t1 = cell_t(to);
                sp_at(t0 + (t1 - t0) * e, 0, x, y);
                *lift = 0.15f * PITCH * sinf(RACE_PI * f);
            } else {
                *x = x0 + (x1 - x0) * e;
                *y = y0 + (y1 - y0) * e;
                float far = hypotf(x1 - x0, y1 - y0);
                *lift = (how == GOO_FLY ? 0.3f * far + 0.05f : 0.32f * PITCH) * sinf(RACE_PI * f);
            }
            *at = f > 0.5f ? to : from;
            return 1;
        }
        t -= len;
        from = to;
    }
    if (anim.sounded <= m->n) {                 /* the end of its way: held by the inn, the well, the prison */
        anim.sounded = m->n + 1;
        int k = goo_kind(m->to);
        if (k == GK_WELL || k == GK_PRISON) ui_sound(SND_DROP);
        else if (k == GK_INN) ui_sound(SND_PLACE);
    }
    square_at(m->to, m->player, x, y, &s0);
    *at = m->to;
    return 0;
}

/* the pawn the mover landed on: still there until the mover arrives, then flying to where the mover came from;
 * 0: it stands where it goes */
static int swapping(float *x, float *y, float *lift)
{
    const goo_move *m = &anim.m;
    float s0, x0, y0, x1, y1;
    *lift = 0;
    square_at(m->to, m->swap, &x0, &y0, &s0);
    square_at(m->from, m->swap, &x1, &y1, &s0);
    int t = (int)(ui_ms() - anim.t0) - (anim.total - SWAP_MS);
    if (t < 0) { *x = x0, *y = y0; return 1; }
    *x = x1, *y = y1;
    if (t >= SWAP_MS) return 0;
    if (anim.hops != -1) { anim.hops = -1; ui_sound(SND_CAPTURE); }
    float f = t / (float)SWAP_MS, e = race_ease(f);
    *x = x0 + (x1 - x0) * e;
    *y = y0 + (y1 - y0) * e;
    *lift = (0.3f * hypotf(x1 - x0, y1 - y0) + 0.04f) * sinf(RACE_PI * f);
    return 1;
}

static void draw(void)
{
    ui_put(t_board, bx, by, (float)S, (float)S);
    for (int c = 1; c <= GOO_LAST; c++) {        /* the numbers: big on a plain square, small beside a picture */
        float x, y, h;
        int kind = goo_kind(c);
        if (c == GOO_LAST) {
            ui_number(bx + 0.5f * S, by + (0.5f - 0.66f * RG) * S, 0.3f * PITCH * S, c, 1);
            continue;
        }
        if (kind == GK_PLAIN) sp_at(cell_t(c), 0, &x, &y), h = 0.30f * PITCH;
        else sp_at(cell_t(c), 0.35f * PITCH, &x, &y), h = 0.23f * PITCH;
        ui_number(bx + x * S, by + y * S, h * S, c, 0);
    }
    /* each pawn's foot and size; the moving one and the one it sends back where they are now */
    float px[GOO_PLAYERS], py[GOO_PLAYERS], ps[GOO_PLAYERS], lift = 0;
    int order[GOO_PLAYERS], np = G.players, at, still = 1;
    for (int q = 0; q < np; q++) square_at(G.pos[q], q, &px[q], &py[q], &ps[q]);
    if (anim.on) {
        const goo_move *m = &anim.m;
        int a = moving(&px[m->player], &py[m->player], &lift, &at);
        py[m->player] -= lift;
        if (a) ps[m->player] = PAWN_SZ;
        int b = 0;
        if (m->swap >= 0) {
            b = swapping(&px[m->swap], &py[m->swap], &lift);
            py[m->swap] -= lift;
        }
        still = !a && !b && !busy();
        if (still) anim.on = 0;
        else ui_animate();
    }
    if (still && G.n) {                         /* the last move: where it left, where it ended */
        float x, y, s;
        if (G.last.from) {
            square_at(G.last.from, G.last.player, &x, &y, &s);
            ui_mark_last_round(bx + x * S, by + y * S, 0.95f * PITCH * S);
        }
        if (G.last.to) {
            square_at(G.last.to, G.last.player, &x, &y, &s);
            ui_mark_last_round(bx + x * S, by + y * S, 0.95f * PITCH * S);
        }
    }
    for (int q = 0; q < np; q++) order[q] = q;
    for (int i = 1; i < np; i++)                /* back to front: the lower on the screen last */
        for (int j = i; j > 0 && py[order[j - 1]] > py[order[j]]; j--) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    int waiting = status() == GS_PLAY && still && !ui_dice_busy();
    if (waiting) {                              /* the pawn of the player to throw */
        int p = goo_turn(&G);
        ui_mark_gold(bx + px[p] * S, by + (py[p] - 0.25f * PITCH) * S, 1.6f * PITCH * S);
    }
    for (int i = 0; i < np; i++) pawn(order[i], px[order[i]], py[order[i]], ps[order[i]]);
    /* the dice: thrown, or waiting in a gold light for the player to throw (their last throw, in their colour) */
    if (waiting) {
        int p = goo_turn(&G), d1 = 6, d2 = 3;
        goo_last_throw(&G, p, &d1, &d2);
        float gap = 0.65f * dsize;
        ui_mark_gold(dcx, dcy, 3.2f * dsize);
        ui_die_at(dcx - gap, dcy, dsize, p, d1, 1);
        ui_die_at(dcx + gap, dcy, dsize, p, d2, 1);
    } else {
        ui_dice_draw(dcx, dcy, dsize, 0);
    }
}

/* ---- the pointer: a tap anywhere throws ------------------------------------------------------------------------- */
static void press(float x, float y)
{
    (void)x, (void)y;
    if (status() != GS_PLAY || busy()) return;
    throw_dice();
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the square the pawn is on, and what holds it: the inn (and the turns still to miss), the well, the prison */
static void bar(int player, box b)
{
    float u = ui.unit, cy = b.y + b.h / 2, h = 0.46f * u;
    int at = G.pos[player];
    if (anim.on && player == anim.m.player) {
        float x, y, l;
        moving(&x, &y, &l, &at);
    }
    if (anim.on && player == anim.m.swap && busy()) at = anim.m.to;
    int w = G.wait[player], kind = goo_kind(G.pos[player]), icon = -1;
    if (!(anim.on && (player == anim.m.player || player == anim.m.swap))) {
        if (w > 0) icon = 0;
        else if (w == GOO_HELD) icon = kind == GK_WELL ? 1 : 2;
    }
    float digits = at >= 10 ? 2 : 1, nw = (0.74f * (digits - 1) + 0.62f) * h, ic = 0.62f * u;
    int narrow = b.w < nw + ic + 0.3f * u;
    float x = b.x + 0.05f * u;
    if (at && !(narrow && icon >= 0)) {
        ui_number(x + nw / 2, cy, h, at, 1);
        x += nw + 0.18f * u;
    }
    if (icon >= 0 && t_icon[icon]) {
        ui_put_c(t_icon[icon], x + ic / 2, cy, ic, ic);
        if (icon == 0 && b.x + b.w - (x + ic) > 0.3f * u) ui_number(x + ic + 0.2f * u, cy, 0.36f * u, w, 1);
    }
}

static void token(int player, int size, uint8_t *rgba) { art_piece(PAWN, player, size, rgba); }

/* the menu's card: the board itself, two pawns on their way */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    board_pic(size, rgba, 1);
    int p = (int)(PAWN_SZ * 1.3f * PITCH * size * 1.2f);
    uint8_t *b = malloc((size_t)p * p * 4);
    if (!b) return;
    static const int at[2] = {14 - 1, 40};
    for (int k = 0; k < 2; k++) {
        float x, y;
        sp_at(cell_t(at[k]), 0, &x, &y);
        art_piece(PAWN, k ? 2 : 0, p, b);
        art_blend(rgba, size, size, b, p, p, (int)(x * size - p / 2.0f), (int)(y * size - 0.62f * p));
    }
    free(b);
}

static const game goose_game = {
    .id = "goose", .order = 157, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
    .players = 1u << 2 | 1u << 3 | 1u << 4 | 1u << 5 | 1u << 6, .view = VIEW_OPEN,
};
REGISTER_GAME(goose_game)
