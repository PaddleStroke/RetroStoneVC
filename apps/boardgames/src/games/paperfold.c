/*
 * Paper Fold in RetroStone Board, for one player: a sheet of coloured paper on a board of the house wood ruled in
 * squares, the picture to make on a small board of its own beside it (the rules: paperfold_rules.c). A fold lifts the
 * flap off the board and turns it over its crease in three dimensions (it rises, foreshortens, its face darkening as
 * it stands on edge, its back showing as it comes down, its shadow on the paper below), then it lies on the other side.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag the paper: the crease is the ruled line half way between the finger's start and where it is, the side it
 * started on folds, and the flap follows the finger over; let go past the upright and it folds, before it falls back.
 * Or tap a crease (it lights in gold), then the side to fold. A fold that is not allowed: the crease shivers. The
 * lamp: the hint (the next fold of a shortest way: its crease lit, its flap pulsing; from a dead end it first takes
 * back the folds that led there). Undo takes back one fold, all the way to the start. The bar: the folds, and at its
 * right end the par by its flag. Solved: gold light runs over the paper and round the picture's board.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/paperfold.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define FOLD_MS 420                             /* a whole fold, flat to flat */
#define BACK_MS 220                             /* a flap falling back */
#define SHIVER_MS 260
#define WIN_MS 1400
#define FRAME 0.32f                             /* the board's walnut frame, of a square */
#define HINT_TOOL 0

static const uint8_t COLOUR[PFO_COLOURS][3] = {
    {238, 228, 204}, {186, 58, 44}, {50, 140, 104}, {52, 88, 166}, {232, 168, 54}, {58, 50, 46},
};

static pfo_game G;
static int serial, sel_line = -1, hint_fold = -1, tool_down = -1;
static struct {                                 /* a fold on its way: from angle a0 to a1 (radians) */
    int on, fold, commit;
    float a0, a1;
    Uint32 t0, ms;
    pfo_state from;
} anim;
static struct { int on, moved, fold; float x0, y0, x, y, angle; } drag;
static struct { int on, line; Uint32 t0; } shiver;
static struct { int on; Uint32 t0; } win;

static box last_box;
static float bx0, by0, q, cx0, cy0, cq;         /* the board: its first square's top left, a square; the card's */
static int made_q = -1, made_serial = -1, made_small = -1, made_cq = -1;
static SDL_Texture *t_board, *t_face[PFO_COLOURS], *t_card, *t_par;

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* a square of paper, s x s: its colour, its fibres, a faint tooth */
static void paint_face(int colour, int s, uint8_t *o)
{
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            float k = 1 + 0.05f * art_noise(x * 3, y) + 0.03f * art_noise(x + 977, y * 5);
            uint8_t *p = o + 4 * ((size_t)y * s + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(COLOUR[colour][i] * k, 0, 255);
            p[3] = 255;
        }
}

/* the board: the plank w x h squares of s pixels in its frame f, the squares ruled faintly */
static void paint_board(int w, int h, int s, int f, uint8_t *o)
{
    int W = w * s + 2 * f, H = h * s + 2 * f;
    art_plank(w * s, h * s, f, o);
    float t = fmaxf(1, 0.03f * s);
    for (int i = 1; i < w; i++)
        art_tint_rect(o, W, H, f + i * s - t / 2, (float)f, f + i * s + t / 2, (float)(H - f), PUZ_INK, 0.22f);
    for (int i = 1; i < h; i++)
        art_tint_rect(o, W, H, (float)f, f + i * s - t / 2, (float)(W - f), f + i * s + t / 2, PUZ_INK, 0.22f);
}

/* a sheet of paper lying on such a board (its squares' colours: cell, PFO_NONE none; up: show the tops), with a soft
 * shadow; into o (W x H) at the board's squares */
static void paint_paper(const uint8_t *col, int w, int h, int s, int f, int W, int H, uint8_t *o)
{
    static const uint8_t black[3] = {0, 0, 0};
    float off = fmaxf(1, 0.07f * s);
    for (int c = 0; c < w * h; c++)
        if (col[c] != PFO_NONE) {
            float x = f + c % w * s, y = f + c / w * s;
            art_tint_rect(o, W, H, x + off, y + off, x + s + off, y + s + off, black, 0.28f);
        }
    for (int c = 0; c < w * h; c++) {
        if (col[c] == PFO_NONE) continue;
        int x0 = f + c % w * s, y0 = f + c / w * s;
        for (int y = 0; y < s; y++)
            for (int x = 0; x < s; x++) {
                int px = x0 + x, py = y0 + y;
                if (px >= W || py >= H) continue;
                float k = 1 + 0.05f * art_noise(px * 3, py) + 0.03f * art_noise(px + 977, py * 5);
                uint8_t *p = o + 4 * ((size_t)py * W + px);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(COLOUR[col[c]][i] * k, 0, 255);
            }
    }
}

/* the picture to make on its own board, size x size (or the sheet: its tops) */
static void paint_map(int w, int h, const uint8_t *col, int size, uint8_t *rgba, int *pw, int *ph)
{
    int s = (int)(size / (fmaxf((float)w, (float)h) + 2 * FRAME)), f = (int)(FRAME * s);
    *pw = *ph = 0;
    if (s < 2) return;
    int W = w * s + 2 * f, H = h * s + 2 * f;
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    paint_board(w, h, s, f, o);
    paint_paper(col, w, h, s, f, W, H, o);
    art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    *pw = W;
    *ph = H;
    free(o);
}

/* a sheet with its corner folded over (the bars' token, the menu): ivory, the corner's back red */
static void paint_token(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    float a = 0.12f * size, b = 0.88f * size, cut = 0.42f * size;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            float in = clampf(fminf(fminf(px - a, b - px), fminf(py - a, b - py)) + 0.5f, 0, 1);
            float corner = (b - px) + (py - a) - cut;  /* < 0: the corner folded away */
            if (in > 0 && corner > -0.5f) pzk_over(p, COLOUR[0], in * clampf(corner + 0.5f, 0, 1));
        }
    /* the flap: the corner's mirror over the fold line, its back showing */
    const float flap[] = {b - cut, a, b - cut, a + cut, b, a + cut};
    const float shade[] = {b - cut + 0.04f * size, a + 0.05f * size, b - cut + 0.04f * size, a + cut + 0.05f * size,
                           b + 0.02f * size, a + cut + 0.05f * size};
    static const uint8_t dark[3] = {20, 12, 8};
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            pzk_over(p, dark, 0.35f * pzt_cover(shade, 3, (float)x, (float)y));
        }
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            float cov = pzt_cover(flap, 3, (float)x, (float)y);
            if (cov <= 0) continue;
            uint8_t c[3];
            float k = 0.9f + 0.2f * (x - (b - cut)) / cut;
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(COLOUR[1][i] * k, 0, 255);
            pzk_over(p, c, cov);
        }
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    paint_token(size, rgba);
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    last_box = b;
    int w = G.m.w, h = G.m.h;
    /* the board as big as it goes with a card of at least 0.4 of its squares beside it (upright: above it), the
     * card then as big as the room left lets it, up to 0.6 of the board's squares; both centred together */
    float W = w + 2 * FRAME, H = h + 2 * FRAME, gap = 0.3f * ui.unit;
    int up = b.h >= b.w;
    float along = up ? b.h : b.w, across = up ? b.w : b.h, n_along = up ? H : W, n_across = up ? W : H;
    q = floorf(fminf(across / n_across, (along - gap) / (1.4f * n_along)));
    cq = floorf(fminf(fminf(0.6f * q, (along - gap - n_along * q) / n_along), across / n_across));
    float total = n_along * (q + cq) + gap, s0 = (up ? b.y : b.x) + (along - total) / 2;
    if (up) {
        cx0 = floorf(b.x + (b.w - w * cq) / 2);
        cy0 = floorf(s0 + FRAME * cq);
        bx0 = floorf(b.x + (b.w - w * q) / 2);
        by0 = floorf(s0 + H * cq + gap + FRAME * q);
    } else {
        cx0 = floorf(s0 + FRAME * cq);
        cy0 = floorf(b.y + (b.h - h * cq) / 2);
        bx0 = floorf(s0 + W * cq + gap + FRAME * q);
        by0 = floorf(b.y + (b.h - h * q) / 2);
    }
    int s = (int)q, f = (int)(FRAME * q);
    if (made_q != s || made_serial != serial) {
        made_q = s;
        int W = w * s + 2 * f, H = h * s + 2 * f;
        uint8_t *o = malloc((size_t)W * H * 4);
        ui_free(&t_board);
        if (o) {
            paint_board(w, h, s, f, o);
            t_board = ui_tex(W, H, o);
        }
        free(o);
        for (int i = 0; i < PFO_COLOURS; i++) {
            ui_free(&t_face[i]);
            paint_face(i, s, ui_buf(s, s));
            t_face[i] = ui_tex(s, s, ui_buf(s, s));
        }
    }
    int cs = (int)cq;
    if (made_cq != cs || made_serial != serial) {
        made_cq = cs;
        int cf = (int)(FRAME * cs), W = w * cs + 2 * cf, H = h * cs + 2 * cf;
        uint8_t *o = malloc((size_t)W * H * 4);
        ui_free(&t_card);
        if (o && cs > 1) {
            paint_board(w, h, cs, cf, o);
            paint_paper(G.m.target, w, h, cs, cf, W, H, o);
            t_card = ui_tex(W, H, o);
        }
        free(o);
    }
    made_serial = serial;
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_par);                        /* the par's flag, as the other puzzles show it */
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    anim.on = drag.on = shiver.on = win.on = 0;
    sel_line = hint_fold = tool_down = -1;
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!pfo_level(&G, level)) pfo_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    pfo_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int solved(void) { return pfo_solved(&G.m, &G.s); }
static int status(void) { return solved() && !anim.on ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(G.moves, par()); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return anim.on || (win.on && now - win.t0 < WIN_MS);
}

static float ease_out(float t) { t = clampf(t, 0, 1); return 1 - (1 - t) * (1 - t); }

/* a fold set going from angle a0 (a flap a finger left) to flat over (commit) or back flat */
static void fold_go(int fold, float a0, int commit)
{
    anim.on = 1;
    anim.fold = fold;
    anim.commit = commit;
    anim.a0 = a0;
    anim.a1 = commit ? PUZ_PI : 0;
    anim.from = G.s;
    anim.t0 = ui_ms();
    float part = fabsf(anim.a1 - a0) / PUZ_PI;
    anim.ms = (Uint32)((commit ? FOLD_MS : BACK_MS) * fmaxf(0.35f, part));
    if (commit) {
        ui_sound(SND_MOVE);
        ui_sound_later(SND_PLACE, (int)anim.ms);
    }
    sel_line = hint_fold = -1;
    ui_animate();
}

static void deny(int line)
{
    shiver.on = 1;
    shiver.line = line;
    shiver.t0 = ui_ms();
    ui_sound(SND_DENY);
    ui_animate();
}

/* the fold at its end: kept, the win seen */
static void fold_land(void)
{
    anim.on = 0;
    if (!anim.commit) return;
    G.s = anim.from;
    if (pfo_move(&G, anim.fold) && solved()) {
        win.on = 1;
        win.t0 = ui_ms();
        ui_sound(SND_END);
    }
}

static void hint(void)
{
    if (solved() || anim.on) { ui_sound(SND_DENY); return; }
    int f, n = pfo_solve(&G.m, &G.s, &f, NULL);
    while (n < 0 && pfo_undo(&G)) n = pfo_solve(&G.m, &G.s, &f, NULL);
    if (n <= 0) { ui_sound(SND_DENY); return; }
    hint_fold = f;
    sel_line = f >> 1;
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    calm();
    return pfo_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = pfo_script(&G, moves);
    if (solved()) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* a point of the board (squares from its top left) z squares above it, on the screen: lifted things grow a little and
 * rise up the screen, as seen from the front and above */
static void proj(float ax, float ay, float z, float *sx, float *sy)
{
    float k = 1 + 0.07f * z, mx = G.m.w / 2.0f, my = G.m.h / 2.0f;
    *sx = bx0 + q * (mx + (ax - mx) * k + 0.05f * z);
    *sy = by0 + q * (my + (ay - my) * k - 0.16f * z);
}

static void quad(SDL_Texture *t, const float *xy, Uint8 shade, Uint8 alpha)
{
    SDL_Vertex v[4];
    const float uv[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    for (int i = 0; i < 4; i++)
        v[i] = (SDL_Vertex){{xy[2 * i], xy[2 * i + 1]}, {shade, shade, shade, alpha}, {uv[2 * i], uv[2 * i + 1]}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(ui.r, t, v, 4, idx, 6);
}

/* the four corners of square c of a flap turned by angle a over its fold's crease (shadow 1: where its shadow
 * falls on the board, the light coming from the top left) */
static void flap_corners(int fold, int c, float a, int shadow, float *xy)
{
    int axis = PFO_AXIS(fold), k = PFO_LINE(fold), side = PFO_SIDE(fold), x = c % G.m.w, y = c / G.m.w;
    int p = axis ? y : x, l = axis ? x : y;
    float dn = side ? (float)(p - k) : (float)(k - p - 1), sg = side ? 1.0f : -1.0f;   /* its near edge, its way */
    for (int i = 0; i < 4; i++) {               /* round the square: near, far, far, near */
        float dd = dn + (i == 1 || i == 2), ll = (float)l + (i >= 2), along = k + sg * dd * cosf(a), z = dd * sinf(a);
        float ax = axis ? ll : along, ay = axis ? along : ll;
        if (shadow) proj(ax + 0.32f * z, ay + 0.42f * z, 0, &xy[2 * i], &xy[2 * i + 1]);
        else proj(ax, ay, z, &xy[2 * i], &xy[2 * i + 1]);
    }
}

static void draw_line(int line, float a, int gold)   /* a crease across the board (a = alpha 0..1) */
{
    int axis = line >> 3, k = line & 7;
    float t = fmaxf(2, 0.06f * q), sx = 0;
    if (shiver.on && shiver.line == line) {
        float f = (ui_ms() - shiver.t0) / (float)SHIVER_MS;
        if (f >= 1) shiver.on = 0;
        else { sx = 0.06f * q * sinf(PUZ_PI * 4 * f) * (1 - f); ui_animate(); }
    }
    pzk_rgba c = gold ? (pzk_rgba){240, 200, 100, (Uint8)(230 * a)} : (pzk_rgba){180, 60, 40, (Uint8)(200 * a)};
    float ext = 0.2f * q;
    if (axis == 0) pzk_line(bx0 + k * q + sx, by0 - ext, bx0 + k * q + sx, by0 + G.m.h * q + ext, t, 1.5f, c, 0);
    else pzk_line(bx0 - ext, by0 + k * q + sx, bx0 + G.m.w * q + ext, by0 + k * q + sx, t, 1.5f, c, 0);
    if (gold) {
        pzk_rgba glow = {240, 190, 80, (Uint8)(90 * a)};
        if (axis == 0) pzk_line(bx0 + k * q, by0 - ext, bx0 + k * q, by0 + G.m.h * q + ext, 3 * t, 3 * t, glow, 1);
        else pzk_line(bx0 - ext, by0 + k * q, bx0 + G.m.w * q + ext, by0 + k * q, 3 * t, 3 * t, glow, 1);
    }
}

static int line_can(int line)                   /* a fold along it is possible (either side) */
{
    return pfo_try(&G.m, &G.s, line * 2, NULL) || pfo_try(&G.m, &G.s, line * 2 + 1, NULL);
}

/* the paper of state s, the flap of `fold` (-1 none) turned by angle a */
static void draw_paper(const pfo_state *s, int fold, float a)
{
    int wh = G.m.w * G.m.h;
    uint64_t flap = 0;
    if (fold >= 0) pfo_flap(&G.m, s, fold, &flap);
    float off = 0.06f * q;
    for (int c = 0; c < wh; c++) {              /* the shadows of the paper lying flat */
        if (s->cell[c] == PFO_NONE || (flap >> c & 1)) continue;
        float o = off * (1 + 0.3f * fminf(3, s->layers[c] - 1));
        ui_fill(bx0 + c % G.m.w * q + o, by0 + c / G.m.w * q + o, q, q, 0, 0, 0, 60);
    }
    for (int c = 0; c < wh; c++) {
        if (s->cell[c] == PFO_NONE || (flap >> c & 1)) continue;
        SDL_SetTextureColorMod(t_face[pfo_up(s->cell[c])], 255, 255, 255);
        ui_put(t_face[pfo_up(s->cell[c])], bx0 + c % G.m.w * q, by0 + c / G.m.w * q, q, q);
    }
    /* the creases on the paper: faint lines where both sides have paper */
    pzk_rgba ink = {60, 40, 28, 46};
    float t = fmaxf(1, 0.025f * q);
    for (int y = 0; y < G.m.h; y++)
        for (int x = 0; x < G.m.w; x++) {
            int c = x + y * G.m.w;
            float x0 = bx0 + x * q, y0 = by0 + y * q;
            if (s->cell[c] == PFO_NONE || (flap >> c & 1)) continue;
            if (x + 1 < G.m.w && s->cell[c + 1] != PFO_NONE && !(flap >> (c + 1) & 1))
                pzk_line(x0 + q, y0 + 0.08f * q, x0 + q, y0 + 0.92f * q, t, 0, ink, 0);
            if (y + 1 < G.m.h && s->cell[c + G.m.w] != PFO_NONE && !(flap >> (c + G.m.w) & 1))
                pzk_line(x0 + 0.08f * q, y0 + q, x0 + 0.92f * q, y0 + q, t, 0, ink, 0);
        }
    if (!flap) return;
    float sa = sinf(a), ca = cosf(a);
    for (int c = 0; c < wh; c++)               /* the flap's shadow */
        if (flap >> c & 1) {
            float xy[8];
            flap_corners(fold, c, a, 1, xy);
            quad(NULL, xy, 0, (Uint8)(46 * sa + 12));
        }
    for (int c = 0; c < wh; c++)
        if (flap >> c & 1) {
            float xy[8];
            flap_corners(fold, c, a, 0, xy);
            int face = ca >= 0 ? pfo_up(s->cell[c]) : pfo_down(s->cell[c]);
            float lit = 0.62f + 0.38f * fabsf(ca) + (ca < 0 ? -0.04f : 0.04f) * sa;
            SDL_SetTextureColorMod(t_face[face], 255, 255, 255);
            quad(t_face[face], xy, (Uint8)clampf(255 * lit, 0, 255), 255);
        }
}

static void draw(void)
{
    if (made_q < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    {                                           /* the board, the card */
        int f = (int)(FRAME * q), cf = (int)(FRAME * cq);
        ui_put(t_board, bx0 - f, by0 - f, G.m.w * q + 2 * f, G.m.h * q + 2 * f);
        if (t_card) {
            float W = G.m.w * cq + 2 * cf, H = G.m.h * cq + 2 * cf;
            if (win.on) ui_mark_gold_a(cx0 + G.m.w * cq / 2, cy0 + G.m.h * cq / 2, 1.5f * fmaxf(W, H),
                                       0.5f + 0.5f * puz_pulse((float)(now - win.t0), 0, WIN_MS));
            ui_put(t_card, cx0 - cf, cy0 - cf, W, H);
        }
    }
    if (anim.on) {
        float f = (now - anim.t0) / (float)anim.ms;
        if (f >= 1) fold_land();
        else {
            float a = anim.a0 + (anim.a1 - anim.a0) * (anim.commit ? puz_ease(f) : ease_out(f));
            draw_paper(&anim.from, anim.fold, a);
            ui_animate();
        }
    }
    if (!anim.on) {
        if (drag.on && drag.fold >= 0) draw_paper(&G.s, drag.fold, drag.angle);
        else draw_paper(&G.s, -1, 0);
        if (sel_line >= 0 && !drag.on) {
            draw_line(sel_line, 1, 1);
            for (int side = 0; side < 2; side++) {   /* the sides that can fold, lit; the hint's pulsing */
                int fold = sel_line * 2 + side;
                if (!pfo_try(&G.m, &G.s, fold, NULL)) continue;
                uint64_t m;
                pfo_flap(&G.m, &G.s, fold, &m);
                float pulse = 0.25f + 0.35f * (0.5f + 0.5f * sinf(now / 160.0f));
                float k = fold == hint_fold ? pulse : hint_fold < 0 ? 0.2f : 0;
                if (k > 0)
                    for (int c = 0; c < G.m.w * G.m.h; c++)
                        if (m >> c & 1)
                            ui_mark_gold_a(bx0 + (c % G.m.w + 0.5f) * q, by0 + (c / G.m.w + 0.5f) * q, 1.2f * q, k);
            }
            if (hint_fold >= 0) ui_animate();
        } else if (shiver.on) draw_line(shiver.line, 1, 0);
        if (drag.on && drag.fold >= 0) draw_line(drag.fold >> 1, 0.8f, 1);
    }
    if (win.on) {                               /* the gold wave over the paper */
        float wt = (float)(int)(now - win.t0);
        if (wt >= 0 && wt < WIN_MS) {
            for (int c = 0; c < G.m.w * G.m.h; c++) {
                if (G.s.cell[c] == PFO_NONE) continue;
                float d = (float)(c % G.m.w + c / G.m.w), a = puz_pulse(wt, 90 * d, 520);
                if (a > 0) ui_mark_gold_a(bx0 + (c % G.m.w + 0.5f) * q, by0 + (c / G.m.w + 0.5f) * q, 1.4f * q, a);
            }
            ui_animate();
        }
    }
    ui_tool(HINT_TOOL, ICON_HINT, (solved() ? UI_DIM : 0) | (tool_down == HINT_TOOL ? UI_ON : 0));
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void to_board(float x, float y, float *bx, float *by) { *bx = (x - bx0) / q; *by = (y - by0) / q; }

/* the crease line nearest to a point of the board (squares), within slack squares: its number (axis * 8 + k), -1 */
static int line_near(float bx, float by, float slack)
{
    int best = -1;
    float bd = slack;
    for (int k = 1; k < G.m.w; k++)
        if (by > -0.3f && by < G.m.h + 0.3f && fabsf(bx - k) < bd) { bd = fabsf(bx - k); best = k; }
    for (int k = 1; k < G.m.h; k++)
        if (bx > -0.3f && bx < G.m.w + 0.3f && fabsf(by - k) < bd) { bd = fabsf(by - k); best = 8 + k; }
    return best;
}

/* the drag: its crease half way between where it began and the finger, the side it began on, the flap's angle */
static void drag_follow(void)
{
    float ax, ay, bxx, byy;
    to_board(drag.x0, drag.y0, &ax, &ay);
    to_board(drag.x, drag.y, &bxx, &byy);
    float dx = bxx - ax, dy = byy - ay;
    int axis = fabsf(dx) >= fabsf(dy) ? 0 : 1, n = axis ? G.m.h : G.m.w;
    float p0 = axis ? ay : ax, p1 = axis ? byy : bxx;
    int k = (int)lroundf((p0 + p1) / 2);
    drag.fold = -1;
    if (k < 1 || k >= n) return;
    int side = p0 < k ? 0 : 1, fold = PFO_FOLD(axis, k, side);
    if (!pfo_try(&G.m, &G.s, fold, NULL)) return;
    float r = fabsf(p0 - k), s = side ? p1 - k : k - p1;   /* how far the finger's point is from the crease */
    drag.fold = fold;
    drag.angle = r > 0.05f ? acosf(clampf(s / r, -1, 1)) : 0;
}

static void press(float x, float y)
{
    drag.on = 0;
    tool_down = ui_tool_hit(x, y);
    if (tool_down >= 0) { ui_animate(); return; }
    if (anim.on || solved()) return;
    float bx, by;
    to_board(x, y, &bx, &by);
    if (bx < -0.4f || by < -0.4f || bx > G.m.w + 0.4f || by > G.m.h + 0.4f) return;
    drag.on = 1;
    drag.moved = 0;
    drag.fold = -1;
    drag.x0 = drag.x = x;
    drag.y0 = drag.y = y;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    if (!drag.moved && hypotf(x - drag.x0, y - drag.y0) > 0.25f * q) drag.moved = 1;
    if (drag.moved) {
        drag_follow();
        ui_animate();
    }
}

static void tap(float x, float y)
{
    float bx, by;
    to_board(x, y, &bx, &by);
    int line = line_near(bx, by, 0.24f);
    if (line >= 0 && line != sel_line) {
        if (!line_can(line)) { deny(line); return; }
        sel_line = line;
        hint_fold = -1;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (line >= 0 && line == sel_line) { sel_line = hint_fold = -1; ui_animate(); return; }
    if (sel_line < 0) {
        if (bx >= 0 && by >= 0 && bx < G.m.w && by < G.m.h) ui_sound(SND_DENY);
        return;
    }
    int axis = sel_line >> 3, k = sel_line & 7, side = (axis ? by : bx) < k ? 0 : 1, fold = sel_line * 2 + side;
    if (!pfo_try(&G.m, &G.s, fold, NULL)) { deny(sel_line); return; }
    fold_go(fold, 0, 1);
}

static void release(float x, float y)
{
    if (tool_down >= 0) {
        int k = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) == k && k == HINT_TOOL) hint();
        return;
    }
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    drag.on = 0;
    if (!drag.moved) { tap(x, y); return; }
    drag_follow();
    if (drag.fold < 0) { ui_sound(SND_DENY); ui_animate(); return; }
    if (drag.angle > PUZ_PI / 2) fold_go(drag.fold, drag.angle, 1);
    else fold_go(drag.fold, drag.angle, 0);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.moves, t_par, par(), solved() && G.moves <= par());
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void tops(const pfo_state *s, int n, uint8_t *col)
{
    for (int c = 0; c < n; c++) col[c] = s->cell[c] == PFO_NONE ? PFO_NONE : (uint8_t)pfo_up(s->cell[c]);
}

static void paint_level(const pfo_map *m, int size, uint8_t *rgba)
{
    uint8_t col[PFO_CELLS];
    int W, H;
    tops(&m->start, m->w * m->h, col);
    paint_map(m->w, m->h, col, size, rgba, &W, &H);
}

/* the menu's card: a striped sheet on its board, its right edge folded over, the back showing */
static void thumb(int variant, int size, uint8_t *rgba)
{
    pfo_game *t = malloc(sizeof *t);
    if (!t) return;
    if (variant >= 0) {
        pfo_random(t, variant, 5u + (uint32_t)variant);
        paint_level(&t->m, size, rgba);
        free(t);
        return;
    }
    free(t);
    uint8_t col[16];
    for (int c = 0; c < 16; c++) col[c] = (uint8_t)(c % 4 == 1 ? 2 : 0);
    col[3] = col[7] = col[11] = col[15] = PFO_NONE;
    int W, H;
    paint_map(4, 4, col, size, rgba, &W, &H);
    if (!W) return;
    float s = (float)((int)(size / (4 + 2 * FRAME)));
    float x0 = (size - W) / 2 + (int)(FRAME * s), y0 = (size - H) / 2 + (int)(FRAME * s);
    /* the flap: the last column lifted past the upright, leaning over the third, its back (red) showing */
    float k = x0 + 3 * s, lean = 0.55f * s, rise = 0.18f * s;
    const float flap[] = {k, y0, k - lean, y0 - rise, k - lean, y0 + 4 * s - rise, k, y0 + 4 * s};
    const float shade[] = {k, y0 + 0.05f * s, k - lean + 0.25f * s, y0 + 0.1f * s,
                           k - lean + 0.25f * s, y0 + 4.12f * s, k, y0 + 4 * s};
    static const uint8_t dark[3] = {20, 12, 8};
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            uint8_t *p = rgba + 4 * ((size_t)y * size + x);
            pzk_over(p, dark, 0.3f * pzt_cover(shade, 4, (float)x, (float)y));
        }
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            uint8_t *p = rgba + 4 * ((size_t)y * size + x);
            float cov = pzt_cover(flap, 4, (float)x, (float)y);
            if (cov <= 0) continue;
            uint8_t c[3];
            float lit = 0.8f + 0.25f * (k - x) / lean;
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(COLOUR[1][i] * lit, 0, 255);
            pzk_over(p, c, cov);
        }
}

/* a level's tile: the picture to make */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    pfo_game *t = malloc(sizeof *t);
    int W, H;
    if (t && pfo_level(t, level)) paint_map(t->m.w, t->m.h, t->m.target, size, rgba, &W, &H);
    free(t);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const ICON[PFO_CHAPTERS] = {
        "abab|abab|abab", "acab|acab|acab", "ab..ab|ababab|ababab", "dcdbdc|dbdcdb|dcdbdc",
        "bd..bd|bdadbd|bd..bd", "cfcbcf..|cbcfcbcf|cfcbcfcb|..cfcbcf",
    };
    pfo_map m;
    char pic[PFO_CELLS + PFO_MAXH + 1];
    if (chapter < 0 || chapter >= PFO_CHAPTERS) return;
    int n = 0;
    for (const char *s = ICON[chapter]; *s; s++) {   /* a picture of the same shape (unused) */
        if (*s == '|') { pic[n++] = '|'; continue; }
        pic[n++] = '.';
        s++;
    }
    pic[n] = 0;
    if (pfo_parse(&m, ICON[chapter], pic)) paint_level(&m, size, rgba);
}

static const game paperfold_game = {
    .id = "paperfold", .order = 458, .variants = PFO_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = PFO_LEVELS, .chapters = PFO_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = 1,
};
REGISTER_GAME(paperfold_game)
