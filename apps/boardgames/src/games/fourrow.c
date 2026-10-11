/*
 * Four in a row in RetroStone Board: the upright board pierced by its holes, the discs falling into them, tap a column
 * or drag a disc along the top (the rules: fourrow_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The discs are drawn behind the board, so they show through its holes, and a falling disc slides behind the wood.
 */
#include <math.h>
#include <stdlib.h>

#include "games/fourrow.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in holes */
#define ROOM 1.08f                              /* the room above the board for the disc about to drop, in holes */
#define HOLE 0.40f                              /* a hole's radius, in holes */
#define DISC 0.96f                              /* a disc's picture, in holes: its face a little wider than the hole */
#define GRAVITY 95.0f                           /* holes a second, a second */
#define BOUNCE 0.22f                            /* the speed a disc keeps when it bounces */

static fr_game G;
static int sq, fr, made = -1, made_small = -1;
static float bx, by, top;                       /* the top left hole's corner; the top of the room above the board */
static SDL_Texture *t_board, *t_disc[2], *t_small[2], *t_win, *t_last;
static struct { int on, col, down; } aim;       /* the column chosen: a finger down, or the mouse over the board */
static struct { int on, col, row, landed; Uint32 t0; } fall;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int variant)
{
    (void)variant;
    fr_new(&G);
    fall.on = 0;
    aim.on = aim.down = 0;
}

static int status(void)
{
    switch (fr_status(&G)) {
    case FR_WIN0: return GS_WIN0;
    case FR_WIN1: return GS_WIN1;
    case FR_DRAW: return GS_DRAW;
    default: return GS_PLAY;
    }
}

static int turn(void) { return fr_turn(&G); }

/* a disc dropped in a column: it falls (the sound when it lands), or a full column says no */
static void drop(int col)
{
    int row = fr_play(&G, col);
    if (row < 0) { ui_sound(SND_DENY); return; }
    fall.on = 1;
    fall.col = col;
    fall.row = row;
    fall.landed = 0;
    fall.t0 = ui_ms();
    ui_animate();
}

static int undo(void)
{
    if (!fr_undo(&G)) return 0;
    fall.on = 0;
    return 1;
}

/* "4 4 3 5": the columns played, 1 to 7 from the left (the screenshots' and the tests' moves) */
static int script(const char *s)
{
    for (char *end; *s; s = end) {
        long c = strtol(s, &end, 10);
        if (end == s) break;
        if (fr_play(&G, (int)c - 1) < 0) return 0;
        ui_sound(SND_DROP);
    }
    fall.on = 0;
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the house plank in its walnut frame, pierced by nx x ny round holes: the edge of each bevelled (lit at the bottom
 * right), a soft shadow inside it (deeper at the top left), else clear: what is behind the board shows through */
static void make_board(int nx, int ny, int s, int f, uint8_t *o)
{
    art_board_n(nx, ny, s, f, 0, o);
    int w = nx * s + 2 * f;
    float R = HOLE * s, bevel = 0.07f * s, off = 0.18f * R;
    for (int y = f; y < f + ny * s; y++)
        for (int x = f; x < f + nx * s; x++) {
            float dx = x + 0.5f - (f + ((x - f) / s + 0.5f) * s), dy = y + 0.5f - (f + ((y - f) / s + 0.5f) * s);
            float d = sqrtf(dx * dx + dy * dy);
            if (d > R + bevel) continue;
            uint8_t *p = o + 4 * (y * w + x);
            float wood = clampf(d - R + 0.5f, 0, 1);
            float k = 1 + 0.3f * (dx + dy) / (d * 1.4142f + 1e-3f) * (1 - (d - R) / bevel);
            float ex = dx - off, ey = dy - off, e = clampf((sqrtf(ex * ex + ey * ey) - 0.6f * R) / (0.55f * R), 0, 1);
            float shade = 0.6f * e * e * (3 - 2 * e), a = wood + (1 - wood) * shade;
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k * wood / (a > 0 ? a : 1), 0, 255);
            p[3] = (uint8_t)(255 * a);
        }
}

/* a gold mark with a dark edge (it shows on both colours): a ring (r_in: its hole, 0..1), or a dot (0) */
static SDL_Texture *gold_mark(int size, float r_in)
{
    static const uint8_t shade[3] = {40, 26, 10}, gold[3] = {255, 204, 72};
    int o = (size - (int)(size * 0.86f) + 1) / 2, s = size - 2 * o;
    uint8_t *a = calloc((size_t)size * size, 4), *b = calloc((size_t)s * s, 4);
    if (!a || !b) { free(a); free(b); return NULL; }
    art_ring(size, fmaxf(0, r_in * 0.86f - 0.08f), shade, 0.55f, a);
    art_ring(s, r_in, gold, 1, b);
    art_blend(a, size, size, b, s, s, o, o);
    SDL_Texture *t = ui_tex(size, size, a);
    free(a);
    free(b);
    return t;
}

static void layout(box b)
{
    sq = (int)floorf(fminf(b.w / (FR_COLS + 2 * FRAME), b.h / (FR_ROWS + 2 * FRAME + ROOM)));
    fr = (int)(sq * FRAME);
    float w = (float)FR_COLS * sq + 2 * fr, h = (float)FR_ROWS * sq + 2 * fr + ROOM * sq;
    top = floorf(b.y + (b.h - h) / 2);
    bx = floorf(b.x + (b.w - w) / 2) + fr;
    by = top + floorf(ROOM * sq) + fr;
    if (made != sq) {
        made = sq;
        int bw = FR_COLS * sq + 2 * fr, bh = FR_ROWS * sq + 2 * fr, d = (int)(DISC * sq);
        ui_free(&t_board);
        make_board(FR_COLS, FR_ROWS, sq, fr, ui_buf(bw, bh));
        t_board = ui_tex(bw, bh, ui_buf(bw, bh));
        for (int p = 0; p < 2; p++) {
            ui_free(&t_disc[p]);
            art_counter(COUNTER_DISC, p, d, ui_buf(d, d));
            t_disc[p] = ui_tex(d, d, ui_buf(d, d));
        }
        ui_free(&t_win);
        t_win = gold_mark((int)(0.76f * sq), 0.8f);
        ui_free(&t_last);
        t_last = gold_mark((int)(0.2f * sq), 0);
    }
    int small = (int)(0.5f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int p = 0; p < 2; p++) {
            ui_free(&t_small[p]);
            art_counter(COUNTER_DISC, p, small, ui_buf(small, small));
            t_small[p] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void hole_xy(int col, int row, float *x, float *y)    /* a hole's centre (row 0 at the bottom) */
{
    *x = bx + (col + 0.5f) * sq;
    *y = by + (FR_ROWS - 0.5f - row) * sq;
}

static float aim_y(void) { return by - fr - 0.54f * sq; }   /* the disc held above the board */

/* the falling disc's centre: it drops from above the board and bounces twice on what stops it; 0 once it rests */
static int fall_y(float *y)
{
    float x, y1, y0 = aim_y(), g = GRAVITY * sq / 1e6f;    /* pixels a millisecond, a millisecond */
    hole_xy(fall.col, fall.row, &x, &y1);
    float t = (float)(ui_ms() - fall.t0), tf = sqrtf(2 * (y1 - y0) / g), v = g * tf;
    if (t < tf) { *y = y0 + 0.5f * g * t * t; return 1; }
    if (!fall.landed) { fall.landed = 1; ui_sound(SND_DROP); }
    t -= tf;
    for (int i = 0; i < 2; i++) {
        v *= BOUNCE;
        float tb = 2 * v / g;
        if (t < tb) { *y = y1 - (v * t - 0.5f * g * t * t); return 1; }
        t -= tb;
    }
    *y = y1;
    return 0;
}

static void draw(void)
{
    float d = DISC * sq, x, y, fy = 0;
    if (fall.on && !fall_y(&fy)) fall.on = 0;
    if (fall.on) ui_animate();
    ui_fill(bx, by, (float)FR_COLS * sq, (float)FR_ROWS * sq, 0, 0, 0, 36);    /* the darkness behind the holes */
    for (int r = 0; r < FR_ROWS; r++)
        for (int c = 0; c < FR_COLS; c++) {
            if (G.hole[r][c] < 0 || (fall.on && c == fall.col && r == fall.row)) continue;
            hole_xy(c, r, &x, &y);
            ui_put_c(t_disc[G.hole[r][c]], x, y, d, d);
        }
    int playing = status() == GS_PLAY, p = turn(), full = aim.on && G.height[aim.col] >= FR_ROWS;
    if (playing && aim.on && !full) {           /* where the disc held up would land: a faint disc */
        hole_xy(aim.col, G.height[aim.col], &x, &y);
        SDL_SetTextureAlphaMod(t_disc[p], 64);
        ui_put_c(t_disc[p], x, y, d, d);
        SDL_SetTextureAlphaMod(t_disc[p], 255);
    }
    if (fall.on) {
        hole_xy(fall.col, fall.row, &x, &y);
        ui_put_c(t_disc[G.hole[fall.row][fall.col]], x, fy, d, d);
    }
    ui_put(t_board, bx - fr, by - fr, (float)FR_COLS * sq + 2 * fr, (float)FR_ROWS * sq + 2 * fr);
    if (!fall.on && G.line) {                   /* the winning four in gold rings */
        for (int r = 0; r < FR_ROWS; r++)
            for (int c = 0; c < FR_COLS; c++) {
                if (!(G.line & FR_BIT(c, r))) continue;
                hole_xy(c, r, &x, &y);
                ui_put_c(t_win, x, y, 0.76f * sq, 0.76f * sq);
            }
    } else if (!fall.on && G.n) {               /* the last disc: a small gold dot */
        int c = G.move[G.n - 1];
        hole_xy(c, G.height[c] - 1, &x, &y);
        ui_put_c(t_last, x, y, 0.2f * sq, 0.2f * sq);
    }
    if (playing && aim.on) {                    /* the disc held above the column (dim when the column is full) */
        hole_xy(aim.col, 0, &x, &y);
        if (full) SDL_SetTextureAlphaMod(t_disc[p], 110);
        ui_put_c(t_disc[p], x, aim_y(), d, d);
        SDL_SetTextureAlphaMod(t_disc[p], 255);
    }
}

/* ---- the pointer: a tap in a column, or a drag along the top, drops a disc there --------------------------------- */
static int over_board(float x) { return x >= bx - fr && x < bx + FR_COLS * sq + fr; }

static int col_of(float x)
{
    int c = (int)floorf((x - bx) / sq);
    return c < 0 ? 0 : c >= FR_COLS ? FR_COLS - 1 : c;
}

static void press(float x, float y)
{
    if (status() != GS_PLAY || !over_board(x) || y < top || y >= by + FR_ROWS * sq + fr) return;
    aim.on = aim.down = 1;
    aim.col = col_of(x);
    ui_animate();
}

static void motion(float x, float y)
{
    int on = aim.down || (status() == GS_PLAY && over_board(x) && y >= top && y < by + FR_ROWS * sq + fr);
    int col = col_of(x);
    if (on == aim.on && (!on || col == aim.col)) return;
    aim.on = on;
    aim.col = col;
    ui_animate();
}

static void release(float x, float y)
{
    (void)y;
    if (!aim.down) return;
    aim.on = aim.down = 0;                      /* a lifted finger holds nothing up (the mouse: again on moving) */
    if (over_board(x)) drop(col_of(x));         /* dragged off the side: nothing */
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the discs a player still has: a row of them, then their number */
static void bar(int player, box b)
{
    int n = fr_left(&G, player);
    float u = ui.unit, sz = 0.5f * u, h = 0.46f * u, cy = b.y + b.h / 2, x = b.x;
    float nw = (n >= 10 ? 0.74f : 0) * h + 0.62f * h, step = n > 1 ? (b.w - sz - nw - 0.3f * u) / (n - 1) : 0;
    step = clampf(step, 0, 0.14f * u);
    for (int i = 0; i < n; i++, x += step) ui_put(t_small[player], x, cy - sz / 2, sz, sz);
    if (n) x += sz - step;
    ui_number(x + 0.22f * u + nw / 2, cy, h, n, 1);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's card: a corner of the board, a game under way, an ivory disc about to drop */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    enum { NX = 5, NY = 4 };
    static const char *rows[NY] = {"..0..", ".01..", "1010.", "01011"};   /* the top row first */
    int s = (int)(size / (NY + 2 * FRAME + ROOM)), f = (int)(s * FRAME), d = (int)(DISC * s);
    int w = NX * s + 2 * f, h = NY * s + 2 * f, ox = (size - w) / 2, oy = (size - h + (int)(ROOM * s)) / 2;
    uint8_t *one = calloc((size_t)NX * s * NY * s, 4);
    if (!one) return;
    for (int i = 0; i < NX * s * NY * s; i++) one[4 * i + 3] = 36;   /* the darkness behind the holes */
    art_blend(rgba, size, size, one, NX * s, NY * s, ox + f, oy + f);
    for (int r = 0; r < NY; r++)
        for (int c = 0; c < NX; c++) {
            if (rows[r][c] == '.') continue;
            art_counter(COUNTER_DISC, rows[r][c] - '0', d, one);
            art_blend(rgba, size, size, one, d, d, ox + f + c * s + (s - d) / 2, oy + f + r * s + (s - d) / 2);
        }
    make_board(NX, NY, s, f, ui_buf(w, h));
    art_blend(rgba, size, size, ui_buf(w, h), w, h, ox, oy);
    art_counter(COUNTER_DISC, 0, d, one);
    art_blend(rgba, size, size, one, d, d, ox + f + 3 * s + (s - d) / 2, oy - (int)(0.54f * s) - d / 2);
    free(one);
}

static int busy(void) { return fall.on; }     /* the end waits for the last disc to land */

static const game fourrow_game = {
    .id = "fourrow", .order = 40, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(fourrow_game)
