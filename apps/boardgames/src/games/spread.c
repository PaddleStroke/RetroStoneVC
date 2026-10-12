/*
 * Spread in RetroStone Board: a 7 x 7 plank ruled in squares, walnut blocks on the blocked cells, the house's discs.
 * Tap (or drag) one of your discs, then a cell: a big dot is a clone (a new disc slides out of yours), a small dot a
 * jump (the disc leaps there). The enemy discs around it then turn over one after the other. When a player cannot
 * move, the empty cells show faintly in the other player's colour: theirs at the count. The rules: spread_rules.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The script's notation: a move is its two cells, "a1b2" (a clone) or "a1c3" (a jump), columns a..g from player 0's
 * left, rows 1..7 from their side.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/grid.h"
#include "games/lines.h"
#include "games/spread.h"
#include "shell.h"

#define MOVE_MS 210                             /* the disc going out (a clone) or leaping (a jump) */
#define STEP_MS 70                              /* from one turning disc to the next */
#define FLIP_MS 300                             /* one disc turning over */
#define PI_F 3.14159265f

static spdgame G;
static spdmove legal[SPD_MAX_MOVES];
static int nlegal, sel = -1, board;
static lines_geo L = {.cells = SPD_N};
static int made = -1, made_board = -1, made_small = -1;
static SDL_Texture *t_disc[2], *t_small[2], *t_board;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, n, order[8], jump; spdmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = spd_status(&G) == SPD_PLAY ? spd_legal(spd_pos(&G), legal) : 0;
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    board = variant;
    spd_new(&G, variant);
    anim.on = 0;
    refresh();
}

static int status(void)
{
    static const int map[] = {[SPD_PLAY] = GS_PLAY, [SPD_WIN0] = GS_WIN0, [SPD_WIN1] = GS_WIN1, [SPD_DRAW] = GS_DRAW};
    return map[spd_status(&G)];
}

static int turn(void) { return spd_pos(&G)->side; }

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static int anim_ms(void) { return MOVE_MS + (anim.n ? (anim.n - 1) * STEP_MS + FLIP_MS : 0); }

static void play(spdmove m, int animate)
{
    uint64_t t = spd_turns(spd_pos(&G), m);
    if (!spd_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.jump = spd_dist(m.from, m.to) == 2;
    anim.t0 = ui_ms();
    anim.n = 0;
    static const int8_t ring[8][2] = {{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}};
    int x = m.to % SPD_N, y = m.to / SPD_N;     /* round the cell reached, clockwise */
    for (int k = 0; k < 8; k++) {
        int tx = x + ring[k][0], ty = y + ring[k][1];
        if (tx >= 0 && ty >= 0 && tx < SPD_N && ty < SPD_N && (t >> SPD_AT(tx, ty) & 1))
            anim.order[anim.n++] = SPD_AT(tx, ty);
    }
    refresh();
    ui_sound(anim.jump ? SND_MOVE : SND_PLACE);
    if (anim.n && animate) ui_sound_later(SND_CAPTURE, MOVE_MS);
    else if (anim.n) ui_sound(SND_CAPTURE);
}

static int undo(void)
{
    if (!spd_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

static int script(const char *s)
{
    int ok = spd_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a blocked cell: a walnut block set in the plank, lit along its top and left edges (the square of side s at x, y) */
static void paint_block(uint8_t *o, int w, int h, float x0, float y0, float s)
{
    float in = 0.06f * s, bev = 0.09f * s;
    for (int y = (int)(y0 + in); y < (int)(y0 + s - in); y++)
        for (int x = (int)(x0 + in); x < (int)(x0 + s - in); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float dx = x + 0.5f - x0 - in, dy = y + 0.5f - y0 - in, side = s - 2 * in;
            float k = 0.40f;
            if (dx < bev || dy < bev) k = dx < dy ? (dx < bev ? 0.58f : 0.40f) : (dy < bev ? 0.62f : 0.40f);
            if (side - dx < bev || side - dy < bev) k = fminf(k, 0.27f);
            uint8_t *p = o + 4 * (y * w + x);
            p[0] = (uint8_t)(p[0] * k * 0.92f);
            p[1] = (uint8_t)(p[1] * k * 0.84f);
            p[2] = (uint8_t)(p[2] * k * 0.80f);
        }
}

/* the plank: n x n squares of s pixels in a frame of f, the lines, the blocked cells of the board b; corner: the top
 * left corner of a board, its lines running on to the frame (the menu's card) */
static void paint_board(uint8_t *o, int n, int s, int f, int b, int corner)
{
    static const uint8_t INK[3] = {44, 30, 20};
    int w = n * s + 2 * f;
    art_board_n(n, n, s, f, 0, o);
    float *cov = calloc((size_t)w * w, sizeof *cov);
    if (!cov) return;
    float lw = 0.8f + 0.018f * s;
    grid_lines(cov, w, (float)f, (float)f, n, n, s, lw, 1.8f * lw, 0.9f * lw, corner);
    grid_ink(o, w, w, cov, w, INK, 0.8f);
    free(cov);
    if (corner) return;
    spdgame *g = malloc(sizeof *g);
    if (!g) return;
    spd_new(g, b);
    for (int c = 0; c < SPD_CELLS; c++)
        if (g->pos[0].c[c] == SPD_BLOCK)
            paint_block(o, w, w, (float)(f + c % n * s), (float)(f + (n - 1 - c / n) * s), (float)s);
    free(g);
}

static SDL_Texture *disc_tex(int player, int size)
{
    art_counter(COUNTER_DISC, player, size, ui_buf(size, size));
    return ui_tex(size, size, ui_buf(size, size));
}

static void layout(box b)
{
    int changed = lines_fit(&L, b);
    if (changed || made != L.sq) {
        made = L.sq;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_disc[c]);
            t_disc[c] = disc_tex(c, L.sq);
        }
        made_board = -1;
    }
    if (made_board != board) {
        made_board = board;
        int w = (int)lines_side(&L);
        ui_free(&t_board);
        paint_board(ui_buf(w, w), SPD_N, L.sq, L.fr, board, 0);
        t_board = ui_tex(w, w, ui_buf(w, w));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_small[c]);
            t_small[c] = disc_tex(c, small);
        }
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y) { lines_xy(&L, c % SPD_N, c / SPD_N, x, y); }

static int xy_cell(float x, float y)
{
    int gx, gy;
    return lines_at(&L, x, y, &gx, &gy) ? SPD_AT(gx, gy) : -1;
}

/* a disc of the player centred on (x, y): w its width (1 flat, 0 on its edge), k its size (lifted: bigger), alpha */
static void disc_at(int player, float x, float y, float w, float k, float alpha)
{
    float d = L.sq * k, lift = (k - 1) * L.sq;
    ui_shadow_flat(x + 0.5f * lift, y + lift, d * fmaxf(w, 0.12f), d, alpha);
    SDL_SetTextureAlphaMod(t_disc[player], (Uint8)(255 * clampf(alpha, 0, 1)));
    ui_put_c(t_disc[player], x, y, d * fmaxf(w, 0.06f), d);
    SDL_SetTextureAlphaMod(t_disc[player], 255);
}

static void draw(void)
{
    const spdpos *p = spd_pos(&G);
    float x, y, s = (float)L.sq;
    ui_put(t_board, L.x - L.fr, L.y - L.fr, lines_side(&L), lines_side(&L));
    if (G.ply) {
        spdmove m = G.move[G.ply - 1];
        if (spd_dist(m.from, m.to) == 2) lines_mark_last(&L, m.from % SPD_N, m.from / SPD_N);
        lines_mark_last(&L, m.to % SPD_N, m.to / SPD_N);
    }
    if (sel >= 0) lines_mark_select(&L, sel % SPD_N, sel / SPD_N);
    int el = (int)(ui_ms() - anim.t0);
    if (anim.on && el >= anim_ms()) anim.on = 0;
    if (anim.on) ui_animate();
    int mover = !p->side;                       /* who played the last move */
    for (int c = 0; c < SPD_CELLS; c++) {
        int who = p->c[c] == SPD_P0 ? 0 : p->c[c] == SPD_P1 ? 1 : -1;
        if (who < 0 || (drag.on && drag.moved && c == drag.from) || (anim.on && c == anim.m.to)) continue;
        float w = 1, k = 1;
        for (int i = 0; anim.on && i < anim.n; i++) {
            if (anim.order[i] != c) continue;   /* squeezed to a line and back, the colour changing on its edge */
            float t = (el - MOVE_MS - i * STEP_MS) / (float)FLIP_MS;
            if (t < 0.5f) who = !mover;
            if (t > 0 && t < 1) w = fabsf(cosf(t * PI_F)), k = 1 + 0.16f * sinf(t * PI_F);
        }
        cell_xy(c, &x, &y);
        disc_at(who, x, y, w, k, 1);
    }
    if (!anim.on && spd_status(&G) != SPD_PLAY && spd_filler(&G) >= 0)   /* the empty cells: the filler's */
        for (int c = 0; c < SPD_CELLS; c++)
            if (p->c[c] == SPD_EMPTY) {
                cell_xy(c, &x, &y);
                SDL_SetTextureAlphaMod(t_disc[spd_filler(&G)], 110);
                ui_put_c(t_disc[spd_filler(&G)], x, y, 0.8f * s, 0.8f * s);
                SDL_SetTextureAlphaMod(t_disc[spd_filler(&G)], 255);
            }
    if (sel >= 0)                               /* a big dot: a clone; a small one: a jump */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            cell_xy(legal[i].to, &x, &y);
            ui_mark_dot(x, y, (spd_dist(sel, legal[i].to) == 1 ? 0.32f : 0.18f) * s);
        }
    if (anim.on) {
        float t = fminf(1, el / (float)MOVE_MS), e = 1 - (1 - t) * (1 - t), x0, y0;
        cell_xy(anim.m.from, &x0, &y0);
        cell_xy(anim.m.to, &x, &y);
        float k = anim.jump ? 1 + 0.28f * sinf(t * PI_F) : 0.7f + 0.3f * e;   /* a leap; a clone grows */
        disc_at(mover, x0 + (x - x0) * e, y0 + (y - y0) * e, 1, k, anim.jump ? 1 : 0.5f + 0.5f * e);
    }
    if (drag.on && drag.moved) {                /* the dragged disc, a little bigger, above the finger */
        float k = 1.18f;
        disc_at(p->side, drag.x, drag.y - 0.3f * k * s, 1, k, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY || anim.on) return;
    const spdpos *p = spd_pos(&G);
    int c = xy_cell(x, y), i;
    if (c < 0) { sel = -1; return; }
    if (sel >= 0 && (i = target(sel, c)) >= 0) {
        play(legal[i], 1);
        return;
    }
    if (p->c[c] == SPD_P0 + p->side) {
        int any = 0;
        for (i = 0; i < nlegal && !any; i++) any = legal[i].from == c;
        if (!any) { ui_sound(SND_DENY); sel = -1; return; }   /* hemmed in */
        sel = c;
        drag.on = 1;
        drag.from = c;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    ui_sound(SND_DENY);
    sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > L.sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the disc stays selected */
    int c = xy_cell(x, y), i = c >= 0 && c != drag.from ? target(drag.from, c) : -1;
    if (i >= 0) play(legal[i], 0);
    else if (c != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the player's discs (at the end, the empty cells they take too); the moves left before the count once few */
static void bar(int player, box b)
{
    ui_bar_number(b, t_small[player], spd_score(&G, player));
    const spdpos *p = spd_pos(&G);
    int left = SPD_IDLE - p->idle;
    if (left <= 20 && p->side == player && spd_status(&G) == SPD_PLAY) {
        float dh = 0.4f * ui.unit, w = ui_number_w(left, dh);
        static const uint8_t dim[3] = {200, 190, 170};
        ui_number_ex(b.x + b.w - w / 2, b.y + b.h / 2, dh, left, dim, 200, UI_NUM_REGULAR);
    }
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* a disc picture squeezed across to w of its width (a disc turning over), s x s */
static void squeeze(const uint8_t *src, int s, float w, uint8_t *out)
{
    memset(out, 0, (size_t)s * s * 4);
    for (int x = 0; x < s; x++) {
        int sx = (int)floorf(s / 2.0f + (x + 0.5f - s / 2.0f) / w);
        if (sx < 0 || sx >= s) continue;
        for (int y = 0; y < s; y++) memcpy(out + 4 * (y * s + x), src + 4 * (y * s + sx), 4);
    }
}

/* the menu's card: a corner of the board, a disc cloned, its neighbours turning; the picker's: each whole board at
 * the start */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int n = variant < 0 ? 4 : SPD_N;
    float k = variant < 0 ? 0.06f : 0.04f;
    int f = (int)(size * k), s = (size - 2 * f) / n, w = n * s + 2 * f, o = (size - w) / 2;
    uint8_t *pic = ui_buf(w, w);
    paint_board(pic, n, s, f, variant < 0 ? 0 : variant, variant < 0);
    art_blend(rgba, size, size, pic, w, w, o, o);
    uint8_t *one = malloc((size_t)s * s * 4), *flat = malloc((size_t)s * s * 4);
    if (one && flat) {
        if (variant >= 0) {
            static const int corner[4][3] = {{0, 6, 0}, {6, 0, 0}, {0, 0, 1}, {6, 6, 1}};   /* col, row: top left */
            for (int i = 0; i < 4; i++) {
                art_counter(COUNTER_DISC, corner[i][2], s, one);
                art_flat_put(rgba, size, size, one, s, o + f + (corner[i][0] + 0.5f) * s,
                             o + f + (corner[i][1] + 0.5f) * s);
            }
        } else {
            static const int place[][4] = {{0, 0, 1, 0}, {1, 0, 1, 0}, {0, 1, 1, 0}, {1, 1, 0, 0}, {2, 1, 0, 1},
                                           {2, 2, 0, 0}, {1, 2, 0, 1}, {3, 2, 1, 0}, {0, 3, 0, 0}, {2, 0, 1, 0}};
            for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
                float wd = place[i][3] ? 0.45f : 1;
                int x = o + f + place[i][0] * s, y = o + f + place[i][1] * s;
                art_flat_shadow(s, one);
                squeeze(one, s, wd, flat);
                art_blend(rgba, size, size, flat, s, s, x + s / 20, y + s * 8 / 100);
                art_counter(COUNTER_DISC, place[i][2], s, one);
                squeeze(one, s, wd, flat);
                art_blend(rgba, size, size, flat, s, s, x, place[i][3] ? y - s / 12 : y);
            }
        }
    }
    free(one);
    free(flat);
}

static int busy(void) { return anim.on; }     /* the end waits for the last disc to turn */

static const game spread_game = {
    .id = "spread", .order = 260, .variants = SPD_BOARDS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(spread_game)
