/*
 * Klotski (Huarong Dao) in RetroStone Board, for one player: a walnut tray of 4 x 5 cells, an opening cut in the middle
 * of its foot, and its wooden blocks: the great one in red lacquer with a gold inlay, the long ones in a darker wood,
 * the small ones pale; the tray in all the room the shell gives, the hint one of its tools (the rules: klotski_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag a block: it follows the finger cell by cell, round corners too. Or tap it: a block with only one place to go
 * goes there; else it is picked (dots on the cells it can reach) and a tap on one of those cells sends it there, by
 * the shortest way. A tap on an empty cell that only one block can reach moves that block. The hint plays the first
 * move of a shortest way, in gold light. Undo takes back one move (a block slid again at once is the same move), all
 * the way to the start. The bar: the moves, and at its right end the start's par (the fewest moves) by the great
 * block, lit in gold when it is matched. Solved: the great block slides out through the opening in gold light.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/klotski.h"
#include "games/puzzles.h"
#include "games/pztools.h"
#include "shell.h"

#define FRAME 0.03f
#define STEP_MS 85                              /* a block sliding one cell */
#define HINT_MS 1300
#define OUT_MS 900                              /* the great block leaving */
#define SHAKE_MS 260

enum { B_HINT, BTNS };

static const uint8_t LACQUER[3] = {178, 52, 38}, DARK_WOOD[3] = {168, 110, 66}, PALE_WOOD[3] = {238, 212, 160};
static const int8_t PW[4] = {2, 1, 2, 1}, PH[4] = {2, 2, 1, 1};

static klt_game G;
static int sel = -1;
static struct { int on, piece, n; int8_t path[2 * KLT_PATH]; float x0, y0; Uint32 t0; } anim;   /* x0, y0: cells */
static struct { int piece; Uint32 t0; } hint = {-1, 0}, shake = {-1, 0};
static struct { int on; Uint32 t0; } win;
static struct { int on, piece, moved; float x, y, x0, y0, gx, gy; } ptr;

static box last_box;
static float bx, by;
static int q, fr, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_piece[4], *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a block w x h (pixels) of a kind: its wood, tinted, bevelled; the great one's gold inlay, a groove on the long
 * ones */
static void paint_block(int kind, int w, int h, uint8_t *o)
{
    pzt_plank(o, w, h, kind * 3 + 1);
    const uint8_t *tone = kind == KLT_BIG ? LACQUER : kind == KLT_SMALL ? PALE_WOOD : DARK_WOOD;
    for (int i = 0; i < w * h; i++) pzt_tint(o + 4 * i, tone, kind == KLT_BIG ? 1.05f : 1);
    int m = w < h ? w : h;
    if (kind == KLT_BIG) {                      /* a gold ring inlaid, a smaller one inside */
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                float d = hypotf(x + 0.5f - w / 2.0f, y + 0.5f - h / 2.0f) / m;
                float a = clampf(1 - fabsf(d - 0.3f) * m / (0.025f * m + 0.8f), 0, 1) +
                          clampf(1 - fabsf(d - 0.2f) * m / (0.012f * m + 0.6f), 0, 1) * 0.8f;
                uint8_t *p = o + 4 * (y * w + x);
                float lit = 1 + 0.25f * clampf(-(x - w / 2.0f + y - h / 2.0f) / (0.4f * m), -1, 1);
                for (int k = 0; k < 3; k++)
                    p[k] = (uint8_t)clampf(p[k] + (PZT_BRASS[k] * lit - p[k]) * clampf(a, 0, 1), 0, 255);
            }
    } else if (kind != KLT_SMALL) {             /* the long ones: a groove across the middle */
        float th = fmaxf(1, 0.03f * m);
        if (h > w) puz_ink_rect(o, w, h, 0.22f * w, h / 2.0f - th, 0.78f * w, h / 2.0f + th, PUZ_INK, 0.35f);
        else puz_ink_rect(o, w, h, w / 2.0f - th, 0.22f * h, w / 2.0f + th, 0.78f * h, PUZ_INK, 0.35f);
    }
    puz_bevel(o, w, h, 0.12f * m, fmaxf(2, 0.09f * m), 0.42f);
}

/* the tray: 4 x 5 cells of the plank in walnut, the opening cut in the middle of its foot */
static void paint_tray(uint8_t *o, int cq, int f)
{
    int w = 4 * cq + 2 * f, h = 5 * cq + 2 * f;
    art_board_n(4, 5, cq, f, 0, o);
    for (int y = 0; y < h; y++)                 /* the floor: a shade darker, so the blocks stand out */
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            if (x < f || y < f || x >= w - f || y >= h - f) continue;
            float e = fminf((float)(x - f), (float)(y - f)), k = 0.78f - 0.12f * clampf(1 - e / (0.12f * cq), 0, 1);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
    for (int y = 5 * cq + f; y < h; y++)        /* the opening */
        for (int x = f + cq; x < f + 3 * cq; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            float a = clampf(fminf((float)(x - f - cq), (float)(f + 3 * cq - 1 - x)) / 2.0f, 0, 1);
            p[3] = (uint8_t)(p[3] * (1 - a));
        }
}

static void block_pic(int kind, int size, uint8_t *rgba)   /* a block fitted in a square (the token, the bar) */
{
    int w = (int)(size * PW[kind] / 2.0f * 0.9f), h = (int)(size * PH[kind] / 2.0f * 0.9f);
    uint8_t *b = calloc((size_t)w * h, 4);
    if (!b) return;
    paint_block(kind, w, h, b);
    art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    block_pic(KLT_BIG, size, rgba);
}

static void layout(box b)
{
    last_box = b;
    box r = b;                                  /* the whole room (game.fill); the hint is the shell's tool */
    float S = fminf(r.w, r.h);
    fr = (int)(S * FRAME);
    q = (int)fminf((r.w - 2 * fr) / 4, (r.h - 2 * fr - 0.3f * ui.unit) / 5);
    bx = floorf(r.x + (r.w - 4 * q - 2 * fr) / 2);
    by = floorf(r.y + (r.h - 5 * q - 2 * fr) / 2 - 0.15f * ui.unit);
    if (made_key != q) {
        made_key = q;
        int w = 4 * q + 2 * fr, h = 5 * q + 2 * fr;
        ui_free(&t_board);
        paint_tray(ui_buf(w, h), q, fr);
        t_board = ui_tex(w, h, ui_buf(w, h));
        int gap = (int)fmaxf(1, 0.035f * q);
        for (int k = 0; k < 4; k++) {
            int pw = PW[k] * q - 2 * gap, ph = PH[k] * q - 2 * gap;
            ui_free(&t_piece[k]);
            paint_block(k, pw, ph, ui_buf(pw, ph));
            t_piece[k] = ui_tex(pw, ph, ui_buf(pw, ph));
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        block_pic(KLT_BIG, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    sel = -1;
    anim.on = win.on = 0;
    hint.piece = shake.piece = -1;
    memset(&ptr, 0, sizeof ptr);
}

static void start(int level)                    /* a designed level */
{
    if (!klt_level(&G, level)) klt_level(&G, 0);
    calm();
}

static void start_random(int variant)           /* a tray made at random: easy, medium or hard */
{
    if (!klt_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u)) klt_level(&G, 0);
    calm();
}

static int status(void) { return klt_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int par(void) { return G.par; }
static int level_stars(void) { return ui_par_stars(G.moves, G.par); }

static Uint32 anim_end(void) { return anim.on ? anim.t0 + (Uint32)(anim.n * STEP_MS) : 0; }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (anim.on && now < anim_end()) || (win.on && now < win.t0 + OUT_MS + 400);
}

static void check_win(Uint32 at)
{
    if (!klt_solved(&G) || win.on) return;
    win.on = 1;
    win.t0 = at;
    sel = -1;
    ui_sound_later(SND_END, (int)(at - ui_ms()));
}

/* block i to (x, y) in one move, sliding along its way */
static void go(int i, int x, int y)
{
    int8_t path[2 * KLT_PATH];
    int n = klt_path(&G, i, x, y, path);
    if (!n) { ui_sound(SND_DENY); return; }
    anim.on = 1;
    anim.piece = i;
    anim.n = n;
    memcpy(anim.path, path, sizeof path);
    anim.x0 = G.p[i].x;
    anim.y0 = G.p[i].y;
    anim.t0 = ui_ms();
    klt_move(&G, i, x, y);
    sel = -1;
    ui_sound(SND_MOVE);
    ui_sound_later(SND_PLACE, n * STEP_MS - 20);
    check_win(anim_end());
    ui_animate();
}

static int undo(void)
{
    if (!G.nh) return 0;
    int8_t before[KLT_PIECES][2];
    for (int i = 0; i < G.np; i++) before[i][0] = G.p[i].x, before[i][1] = G.p[i].y;
    int i = klt_undo(&G);
    calm();
    if (i >= 0) {                               /* it glides straight back */
        anim.on = 1;
        anim.piece = i;
        anim.n = 1;
        anim.path[0] = G.p[i].x;
        anim.path[1] = G.p[i].y;
        anim.x0 = before[i][0];
        anim.y0 = before[i][1];
        anim.t0 = ui_ms();
    }
    ui_animate();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = klt_script(&G, moves);
    check_win(ui_ms());
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static float cell_x(float x) { return bx + fr + x * q; }
static float cell_y(float y) { return by + fr + y * q; }

static int cell_at(float x, float y, int *cx, int *cy)
{
    float ix = (x - bx - fr) / q, iy = (y - by - fr) / q;
    if (ix < 0 || iy < 0 || ix >= 4 || iy >= 5) return 0;
    *cx = (int)ix;
    *cy = (int)iy;
    return 1;
}

/* where block i is drawn (in cells): sliding along its way, pushed by the finger, leaving at the win */
static void piece_place(int i, Uint32 now, float *x, float *y)
{
    *x = G.p[i].x;
    *y = G.p[i].y;
    if (anim.on && anim.piece == i && now < anim_end()) {
        float t = (now - anim.t0) / (float)STEP_MS;
        int k = (int)t;
        float f = puz_ease(t - k), px = k ? anim.path[2 * (k - 1)] : anim.x0, py = k ? anim.path[2 * k - 1] : anim.y0;
        *x = px + (anim.path[2 * k] - px) * f;
        *y = py + (anim.path[2 * k + 1] - py) * f;
    }
    if (ptr.on && ptr.moved && ptr.piece == i) {   /* the finger's pull, towards a free cell only */
        float dx = (ptr.x - ptr.gx) / q, dy = (ptr.y - ptr.gy) / q;
        if (fabsf(dx) >= fabsf(dy))
            *x += clampf(dx, klt_can(&G, i, -1, 0) ? -0.45f : -0.06f, klt_can(&G, i, 1, 0) ? 0.45f : 0.06f);
        else *y += clampf(dy, klt_can(&G, i, 0, -1) ? -0.45f : -0.06f, klt_can(&G, i, 0, 1) ? 0.45f : 0.06f);
    }
    if (shake.piece == i && now - shake.t0 < SHAKE_MS) {
        float t = (now - shake.t0) / (float)SHAKE_MS;
        *x += sinf(t * 6 * PUZ_PI) * 0.05f * (1 - t);
    }
    if (win.on && G.p[i].type == KLT_BIG && now > win.t0)
        *y += 1.8f * puz_ease(clampf((now - win.t0) / (float)OUT_MS, 0, 1));
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    int w = 4 * q + 2 * fr, h = 5 * q + 2 * fr, gap = (int)fmaxf(1, 0.035f * q);
    ui_put(t_board, bx, by, (float)w, (float)h);
    if (sel >= 0) {                             /* the picked block's reach: dots on the cells it can come to */
        int8_t reach[20], dot[20] = {0};
        klt_reach(&G, sel, reach);
        int t = G.p[sel].type;
        for (int c = 0; c < 20; c++) {
            if (reach[c] <= 0) continue;
            for (int dy = 0; dy < PH[t]; dy++)
                for (int dx = 0; dx < PW[t]; dx++) dot[c + dx + dy * 4] = 1;
        }
        for (int c = 0; c < 20; c++)
            if (dot[c] && klt_at(&G, c % 4, c / 4) < 0)
                ui_mark_dot(cell_x(c % 4 + 0.5f), cell_y(c / 4 + 0.5f), 0.32f * q);
    }
    for (int pass = 0; pass < 2; pass++)        /* the moving block last, over the others */
        for (int i = 0; i < G.np; i++) {
            int moving = (anim.on && anim.piece == i && now < anim_end()) || (ptr.on && ptr.moved && ptr.piece == i) ||
                         (win.on && G.p[i].type == KLT_BIG);
            if (moving != pass) continue;
            float x, y;
            piece_place(i, now, &x, &y);
            int t = G.p[i].type;
            float pw = (float)(PW[t] * q - 2 * gap), ph = (float)(PH[t] * q - 2 * gap);
            float px = cell_x(x) + gap, py = cell_y(y) + gap, fade = 1;
            if (win.on && t == KLT_BIG && now > win.t0) {   /* leaving: the gold light round it, fading out */
                float f = clampf((now - win.t0) / (float)OUT_MS, 0, 1);
                fade = 1 - clampf((f - 0.5f) / 0.45f, 0, 1);
                if (fade > 0)                   /* gone with the block: no light left below the tray */
                    ui_mark_gold_a(px + pw / 2, py + ph / 2, 2.4f * q * sinf(PUZ_PI * fminf(1, f * 1.2f)) + 0.4f * q,
                                   fminf(1, 2 * fade));
                SDL_SetTextureAlphaMod(t_piece[t], (Uint8)(255 * fade));
            }
            if (hint.piece == i) {
                float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
                if (p > 0) ui_mark_gold(px + pw / 2, py + ph / 2, (1.2f + PW[t] * 0.6f) * q * p);
            }
            int in_tray = !(win.on && t == KLT_BIG);   /* a shadow in the tray stays on its floor (not out the exit) */
            SDL_Rect floor = {(int)bx + fr, (int)by + fr, 4 * q, 5 * q};
            if (in_tray) SDL_RenderSetClipRect(ui.r, &floor);
            ui_shadow_flat(px + pw / 2 + 0.03f * q, py + ph / 2 + 0.05f * q, pw, ph, 0.55f * fade);
            if (in_tray) SDL_RenderSetClipRect(ui.r, NULL);
            ui_put(t_piece[t], px, py, pw, ph);
            SDL_SetTextureAlphaMod(t_piece[t], 255);
            if (i == sel) ui_mark_select(px, py, pw, ph);
        }
    if (anim.on && now < anim_end()) ui_animate();
    if (hint.piece >= 0) {
        if (now - hint.t0 < HINT_MS) ui_animate();
        else hint.piece = -1;
    }
    if (shake.piece >= 0 && now - shake.t0 < SHAKE_MS) ui_animate();
    if (win.on && now < win.t0 + OUT_MS + 400) ui_animate();
    ui_tool(B_HINT, ICON_HINT, klt_solved(&G) ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void use_hint(void)
{
    int i, x, y;
    if (klt_solve(&G, &i, &x, &y) <= 0) { ui_sound(SND_DENY); return; }
    go(i, x, y);
    hint.piece = i;
    hint.t0 = ui_ms();
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (klt_solved(&G)) return;
    if (ui_tool_hit(x, y) == B_HINT) { use_hint(); return; }
    if (anim.on && ui_ms() < anim_end()) anim.on = 0;   /* a slide still going: done at once */
    int cx, cy;
    ptr.on = 1;
    ptr.piece = cell_at(x, y, &cx, &cy) ? klt_at(&G, cx, cy) : -1;
    ptr.x = ptr.x0 = ptr.gx = x;
    ptr.y = ptr.y0 = ptr.gy = y;
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    ptr.x = x;
    ptr.y = y;
    int i = ptr.piece;
    if (i < 0) return;
    if (!ptr.moved && hypotf(x - ptr.x0, y - ptr.y0) > 0.18f * q) {
        ptr.moved = 1;
        sel = -1;
    }
    if (!ptr.moved) return;
    for (int k = 0; k < 8; k++) {               /* past half a cell towards a free one: it steps there */
        float dx = x - ptr.gx, dy = y - ptr.gy;
        int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
        if (fabsf(dx) >= fabsf(dy) && fabsf(dx) > 0.5f * q && klt_can(&G, i, sx, 0)) {
            klt_step(&G, i, sx, 0);
            ptr.gx += sx * q;
        } else if (fabsf(dy) > 0.5f * q && klt_can(&G, i, 0, sy)) {
            klt_step(&G, i, 0, sy);
            ptr.gy += sy * q;
        } else if (fabsf(dx) > 0.5f * q && klt_can(&G, i, sx, 0)) {
            klt_step(&G, i, sx, 0);
            ptr.gx += sx * q;
        } else break;
        ui_sound_soft(SND_PLACE, 0.5f);
    }
    ui_animate();
}

static void tap(int cx, int cy)
{
    int i = klt_at(&G, cx, cy);
    if (i >= 0) {
        int8_t reach[20];
        klt_reach(&G, i, reach);
        int n = 0, only = -1;
        for (int c = 0; c < 20; c++)
            if (reach[c] > 0) { n++; only = c; }
        if (!n) {
            shake.piece = i;
            shake.t0 = ui_ms();
            sel = -1;
            ui_sound(SND_DENY);
        } else if (n == 1) {
            go(i, only % 4, only / 4);
        } else {
            sel = sel == i ? -1 : i;
            ui_sound(SND_TAP);
        }
        ui_animate();
        return;
    }
    /* an empty cell: the picked block (or the only block that can) comes over it, the shortest way */
    int best = -1, bc = -1, bd = 99, many = 0;
    for (int k = 0; k < G.np; k++) {
        if (sel >= 0 && k != sel) continue;
        int8_t reach[20];
        klt_reach(&G, k, reach);
        int t = G.p[k].type, kd = 99, kc = -1;
        for (int c = 0; c < 20; c++) {
            int x0 = c % 4, y0 = c / 4;
            if (reach[c] <= 0 || cx < x0 || cy < y0 || cx >= x0 + PW[t] || cy >= y0 + PH[t]) continue;
            if (reach[c] < kd) { kd = reach[c]; kc = c; }
        }
        if (kc < 0) continue;
        if (best >= 0) many = 1;
        if (kd < bd) { bd = kd; best = k; bc = kc; }
    }
    if (best < 0 || (many && sel < 0)) {
        sel = -1;
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    go(best, bc % 4, bc / 4);
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    if (ptr.moved) {
        if (G.last >= 0) ui_sound(SND_PLACE);
        check_win(ui_ms());
        return;
    }
    int cx, cy;
    if (cell_at(x, y, &cx, &cy)) tap(cx, cy);
    else if (sel >= 0) { sel = -1; ui_animate(); }
}

/* ---- the bar: the moves; the par at the right -------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two(b, NULL, G.moves, t_small, G.par, klt_solved(&G) && G.moves <= G.par);
}

/* ---- the pictures of the shell's screens: a tray, its blocks where they are ------------------------------------- */
static void paint_trayful(const klt_game *T, int size, uint8_t *rgba)
{
    int f = (int)(size * 0.03f), cq = (int)((size * 0.94f - 2 * f) / 5), w = 4 * cq + 2 * f, h = 5 * cq + 2 * f;
    uint8_t *b = calloc((size_t)w * h, 4);
    if (!b) return;
    paint_tray(b, cq, f);
    int gap = (int)fmaxf(1, 0.04f * cq);
    for (int i = 0; i < T->np; i++) {
        int t = T->p[i].type, pw = PW[t] * cq - 2 * gap, ph = PH[t] * cq - 2 * gap;
        uint8_t *pc = calloc((size_t)pw * ph, 4);
        if (!pc) continue;
        paint_block(t, pw, ph, pc);
        art_blend(b, w, h, pc, pw, ph, f + T->p[i].x * cq + gap, f + T->p[i].y * cq + gap);
        free(pc);
    }
    art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
}

/* the trays the pictures show, each made once: the menu's and the random variants' (a fixed seed), the levels' */
static klt_game *pic_tray(int which)            /* which: -1 the menu, -2.. a variant, 0.. a level */
{
    static klt_game T;
    static int made = -1000;
    if (made == which) return &T;
    int ok = which >= 0 ? klt_level(&T, which) : which == -1 ? klt_level(&T, 0) : klt_random(&T, -2 - which, 3);
    made = ok ? which : -1000;
    return ok ? &T : NULL;
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    klt_game *T = pic_tray(variant < 0 ? -1 : -2 - variant);
    if (T) paint_trayful(T, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    klt_game *T = pic_tray(level);
    if (T) paint_trayful(T, size, rgba);
}

/* a chapter's tab: the great block, and as many small blocks under it as the chapter's number */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    klt_game T;
    memset(&T, 0, sizeof T);
    T.p[T.np++] = (klt_piece){KLT_BIG, 1, 0};
    for (int k = 0; k <= chapter && k < 6; k++)
        T.p[T.np++] = (klt_piece){KLT_SMALL, (int8_t)(k % 4), (int8_t)(2 + k / 4)};
    paint_trayful(&T, size, rgba);
}

static int score(void) { return G.moves; }   /* the end card's number */

static const game klotski_game = {
    .id = "klotski", .order = 441, .variants = KLT_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
    .levels = KLT_LEVELS, .chapters = KLT_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .par = par, .fill = 1, .tools = BTNS,
};
REGISTER_GAME(klotski_game)
