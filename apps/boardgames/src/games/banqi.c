/*
 * Banqi (Chinese dark chess) in RetroStone Board: half a xiangqi board (4 x 8 squares, its palace's cross on one
 * edge), the 32 pieces face down; a tap turns one over (it spins round to its face), or tap or drag your piece to move
 * or capture (the rules: banqi_rules.c, banqi.h). Nothing is secret (VIEW_OPEN): the view just turns to the player to
 * move. The board stands up in portrait and lies down in landscape, whichever gives the bigger squares.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The players are seats: seat 0 turns the first piece over and plays its colour, seat 1 the other (bq_seat(): a black
 * piece first, and seat 0 plays black while red, seat 1, moves next). turn(), the end, the bars and the tokens are by
 * seat (their bars show the general of the seat's colour; before the first flip, when nobody has a colour yet, a piece
 * face down).
 * The script's notation: "c2" turns c2 over, "c2c3" moves (files a..h along the long side, ranks 1..4); "@7" first:
 * the deal of seed 7 (the screenshots).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/banqi.h"
#include "games/captures_bar.h"
#include "games/grid.h"
#include "games/xqpiece.h"
#include "shell.h"

#define FRAME 0.30f                             /* the board's frame, in squares */
#define MOVE_MS 160
#define FLIP_MS 360
#define PI_F 3.14159265f

static bqgame G;
static bqmove legal[BQ_MAX_MOVES];
static int nlegal, sel = -1;
static struct { int land, cols, rows, s, f; float x, y; } T;   /* as shown; (x, y): the squares' top left */
static int made = -1, made_land = -1, made_small = -1;
static SDL_Texture *t_face[2][8], *t_small[2][8], *t_back, *t_board;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; bqmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    static int decided = -1;
    nlegal = bq_status(&G) == BQ_PLAY ? bq_legal(bq_pos(&G), legal) : 0;
    sel = -1;
    drag.on = 0;
    if (decided != bq_pos(&G)->decided) {      /* the colours given (or taken back): the bars' tokens follow */
        decided = bq_pos(&G)->decided;
        ui_tokens_changed();
    }
}

static void start(int variant)
{
    (void)variant;
    bq_new(&G, (uint32_t)ui_random(1 << 30));
    anim.on = 0;
    refresh();
}

static int status(void)                         /* the rules' winner is a colour: its seat won */
{
    int s = bq_status(&G);
    if (s != BQ_WIN0 && s != BQ_WIN1) return s == BQ_DRAW ? GS_DRAW : GS_PLAY;
    return bq_seat(bq_pos(&G), s == BQ_WIN1) ? GS_WIN1 : GS_WIN0;
}

static int turn(void) { return bq_seat(bq_pos(&G), bq_pos(&G)->side); }   /* the seat whose colour moves */

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].kind != BQ_FLIP && legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static void play(bqmove m, int animate)
{
    if (!bq_play(&G, m)) return;
    anim.on = animate;
    anim.m = G.move[G.ply - 1];
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.kind == BQ_TAKE ? SND_CAPTURE : m.kind == BQ_FLIP ? SND_PLACE : SND_MOVE);
    ui_handoff(turn());                         /* the view turns once the piece has landed */
}

static int undo(void)
{
    if (!bq_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    ui_handoff(turn());
    return 1;
}

static int script(const char *s)
{
    int ok = bq_script(&G, s);
    anim.on = 0;
    refresh();
    ui_handoff(turn());
    return ok;
}

/* ---- the bot (banqi_bot.c) --------------------------------------------------------------------------------------- */
const bqgame *banqi_live(void) { return &G; }

/* one move ("c2" turns c2 over, "c2c3" moves), played as if tapped: it turns over or slides */
int banqi_show(const char *move)
{
    int a = move ? bq_square(move) : -1, b = a >= 0 && move[2] ? bq_square(move + 2) : -1, k = -1;
    if (a < 0 || (move[2] && (b < 0 || move[4])) || status() != GS_PLAY) return 0;
    for (int i = 0; i < nlegal && k < 0; i++)
        if (b < 0 ? legal[i].kind == BQ_FLIP && legal[i].from == a : legal[i].kind != BQ_FLIP && legal[i].from == a &&
                                                                      legal[i].to == b) k = i;
    if (k < 0) return 0;
    play(legal[k], 1);
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static const int XQ_OF[8] = {0, XQ_GENERAL, XQ_ADVISOR, XQ_ELEPHANT, XQ_CHARIOT, XQ_HORSE, XQ_CANNON, XQ_SOLDIER};

static void face_pic(int pc, int size, uint8_t *rgba) { xq_piece_paint(XQ_OF[BQ_TYPE(pc)], BQ_COLOUR(pc), size, rgba); }

/* a piece's back: the same wooden disc in a warmer boxwood, two engraved rings and no character */
static void back_pic(int size, uint8_t *rgba)
{
    static const uint8_t ink[3] = {118, 82, 48};
    art_style st = art_player[0];
    const float wood[3] = {226, 200, 156};
    memcpy(st.fill, wood, sizeof wood);
    st.line = fmaxf(0.022f, 1.3f / size);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.45f, 0.45f);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
    uint8_t *layer = calloc((size_t)size * size, 4);
    if (!layer) return;
    for (int k = 0; k < 2; k++) {
        int d = (int)((k ? 0.36f : 0.78f) * size);
        d -= (size - d) & 1;
        memset(layer, 0, (size_t)size * size * 4);
        art_ring(d, k ? 0.80f : 0.91f, ink, 0.75f, layer);
        art_blend(rgba, size, size, layer, d, d, (size - d) / 2, (size - d) / 2);
    }
    free(layer);
}

/* a model corner (ex 0..8 along the files, ey 0..4 along the ranks) in the picture of the board as shown */
static void corner_px(int land, int s, int f, float ex, float ey, float *px, float *py)
{
    *px = land ? f + ex * s : f + ey * s;
    *py = land ? f + (BQ_H - ey) * s : f + (BQ_W - ex) * s;
}

/* the plank (cols x rows squares of s pixels, frame f), the lines between the squares and the palace's cross on the
 * first rank (files d, e); open: a corner of the board, its lines running on to the frame (the menu's card) */
static void paint_board(uint8_t *o, int land, int cols, int rows, int s, int f, int open)
{
    static const uint8_t INK[3] = {40, 28, 18};
    int w = cols * s + 2 * f, h = rows * s + 2 * f, st = w > h ? w : h;
    art_board_n(cols, rows, s, f, 0, o);
    float *cov = calloc((size_t)st * st, sizeof *cov);
    if (!cov) return;
    float lw = 0.8f + 0.018f * s;
    grid_lines(cov, st, (float)f, (float)f, cols, rows, s, lw, 1.8f * lw, 0.9f * lw, open);
    if (!open) {
        float ax, ay, bx, by;
        corner_px(land, s, f, 3, 0, &ax, &ay);
        corner_px(land, s, f, 5, 2, &bx, &by);
        palace_seg(cov, st, ax, ay, bx, by, lw / 2);
        corner_px(land, s, f, 5, 0, &ax, &ay);
        corner_px(land, s, f, 3, 2, &bx, &by);
        palace_seg(cov, st, ax, ay, bx, by, lw / 2);
    }
    grid_ink(o, w, h, cov, st, INK, 0.85f);
    free(cov);
}

static SDL_Texture *pic_tex(int pc, int size)   /* pc 0: the back */
{
    if (pc) face_pic(pc, size, ui_buf(size, size));
    else back_pic(size, ui_buf(size, size));
    return ui_tex(size, size, ui_buf(size, size));
}

static void layout(box b)
{
    float kp = fminf(b.w / (BQ_H + 2 * FRAME), b.h / (BQ_W + 2 * FRAME)),
          kl = fminf(b.w / (BQ_W + 2 * FRAME), b.h / (BQ_H + 2 * FRAME));
    T.land = kl > kp;
    T.cols = T.land ? BQ_W : BQ_H;
    T.rows = T.land ? BQ_H : BQ_W;
    T.s = (int)floorf(fmaxf(kp, kl));
    T.f = (int)(T.s * FRAME);
    int w = T.cols * T.s + 2 * T.f, h = T.rows * T.s + 2 * T.f;
    T.x = floorf(b.x + (b.w - w) / 2) + T.f;
    T.y = floorf(b.y + (b.h - h) / 2) + T.f;
    if (made != T.s) {
        made = T.s;
        for (int c = 0; c < 2; c++)
            for (int t = BQ_GENERAL; t <= BQ_SOLDIER; t++) {
                ui_free(&t_face[c][t]);
                t_face[c][t] = pic_tex(BQ_PC(c, t), T.s);
            }
        ui_free(&t_back);
        t_back = pic_tex(0, T.s);
        made_land = -1;
    }
    if (made_land != T.land) {
        made_land = T.land;
        ui_free(&t_board);
        paint_board(ui_buf(w, h), T.land, T.cols, T.rows, T.s, T.f, 0);
        t_board = ui_tex(w, h, ui_buf(w, h));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++)
            for (int t = BQ_GENERAL; t <= BQ_SOLDIER; t++) {
                ui_free(&t_small[c][t]);
                t_small[c][t] = pic_tex(BQ_PC(c, t), small);
            }
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void sq_xy(int s, float *x, float *y)    /* a square's centre */
{
    int col = T.land ? BQ_X(s) : BQ_Y(s), row = T.land ? BQ_H - 1 - BQ_Y(s) : BQ_W - 1 - BQ_X(s);
    *x = T.x + (col + 0.5f) * T.s;
    *y = T.y + (row + 0.5f) * T.s;
}

static int xy_sq(float x, float y)
{
    int col = (int)floorf((x - T.x) / T.s), row = (int)floorf((y - T.y) / T.s);
    if (col < 0 || row < 0 || col >= T.cols || row >= T.rows) return -1;
    return T.land ? BQ_SQ(col, BQ_H - 1 - row) : BQ_SQ(BQ_W - 1 - row, col);
}

static void mark_sq(int s, void (*mark)(float, float, float, float))
{
    float x, y, h = T.s / 2.0f;
    sq_xy(s, &x, &y);
    mark(x - h, y - h, (float)T.s, (float)T.s);
}

/* a piece (pc; hidden: its back) centred on (x, y): d across, squeezed to w of its width (turning over), alpha */
static void piece_at(uint8_t pc, float x, float y, float d, float w, float alpha)
{
    SDL_Texture *t = pc & BQ_HIDDEN ? t_back : t_face[BQ_COLOUR(pc)][BQ_TYPE(pc)];
    float lift = d / T.s - 1;
    ui_shadow_flat(x + 0.5f * lift * T.s, y + lift * T.s, d * fmaxf(w, 0.12f), d, alpha);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(alpha, 0, 1)));
    ui_put_c(t, x, y, d * fmaxf(w, 0.04f), d);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw(void)
{
    const bqpos *p = bq_pos(&G);
    float x, y, s = (float)T.s;
    ui_put(t_board, T.x - T.f, T.y - T.f, T.cols * s + 2 * T.f, T.rows * s + 2 * T.f);
    if (G.ply) {
        bqmove m = G.move[G.ply - 1];
        mark_sq(m.from, ui_mark_last);
        if (m.to != m.from) mark_sq(m.to, ui_mark_last);
    }
    if (sel >= 0) mark_sq(sel, ui_mark_select);
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(anim.m.kind == BQ_FLIP ? FLIP_MS : MOVE_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    for (int q = 0; q < BQ_N; q++) {
        if (!p->sq[q] || (drag.on && drag.moved && q == drag.from) || (anim.on && q == anim.m.to)) continue;
        sq_xy(q, &x, &y);
        piece_at(p->sq[q], x, y, s, 1, 1);
    }
    if (sel >= 0)                               /* where the selected piece can go */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].kind == BQ_FLIP || legal[i].from != sel) continue;
            sq_xy(legal[i].to, &x, &y);
            if (legal[i].kind == BQ_TAKE) ui_mark_ring(x, y, s);
            else ui_mark_dot(x, y, 0.32f * s);
        }
    if (anim.on) {
        bqmove m = anim.m;
        if (m.kind == BQ_FLIP) {                /* lifted, turned on its edge, laid down on its face */
            sq_xy(m.from, &x, &y);
            piece_at(t < 0.5f ? (uint8_t)(m.pc | BQ_HIDDEN) : m.pc, x, y, s * (1 + 0.16f * sinf(t * PI_F)),
                     fabsf(cosf(t * PI_F)), 1);
        } else {
            float e = 1 - (1 - t) * (1 - t) * (1 - t), x0, y0;
            sq_xy(m.to, &x, &y);
            if (m.kind == BQ_TAKE) piece_at(m.pc, x, y, s, 1, 1 - e);   /* the captured piece fades */
            sq_xy(m.from, &x0, &y0);
            piece_at(p->sq[m.to], x0 + (x - x0) * e, y0 + (y - y0) * e, s, 1, 1);
        }
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, above the finger */
        float d = 1.18f * s;
        piece_at(p->sq[drag.from], drag.x, drag.y - 0.25f * d, d, 1, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY || anim.on) return;
    const bqpos *p = bq_pos(&G);
    int s = xy_sq(x, y), i;
    if (s < 0) { sel = -1; return; }
    if (sel >= 0 && (i = target(sel, s)) >= 0) {
        play(legal[i], 1);
        return;
    }
    if (p->sq[s] & BQ_HIDDEN) {                 /* a face-down piece: turned over */
        play((bqmove){BQ_FLIP, (uint8_t)s, (uint8_t)s, 0}, 1);
        return;
    }
    if (p->sq[s] && p->decided && BQ_COLOUR(p->sq[s]) == p->side) {
        sel = s;
        drag.on = 1;
        drag.from = s;
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
    if (hypotf(x - drag.x, y - drag.y) > T.s * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int s = xy_sq(x, y), i = s >= 0 && s != drag.from ? target(drag.from, s) : -1;
    if (i >= 0) play(legal[i], 0);
    else if (s != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pieces the player (a seat) captured, the strongest first; the turns left before the draw once they are few */
static void bar(int player, box b)
{
    static const int start_n[8] = {0, 1, 2, 2, 2, 2, 2, 5};
    const bqpos *p = bq_pos(&G);
    int c = !bq_colour(p, player), gone[8];     /* the other colour's pieces gone */
    memcpy(gone, start_n, sizeof gone);
    for (int q = 0; q < BQ_N; q++)
        if (p->sq[q] && BQ_COLOUR(p->sq[q]) == c) gone[BQ_TYPE(p->sq[q])]--;
    float end = b.x + b.w;
    int left = BQ_QUIET - p->quiet;
    if (left <= 20 && turn() == player && bq_status(&G) == BQ_PLAY) {
        box r = ui_bar_countdown(b, left);
        end = r.x + r.w;
    }
    /* their glyphs readable: two rows of smaller pieces rather than one row of pieces hidden under each other */
    cbar_draw(b, end, &t_small[c][BQ_GENERAL], &gone[BQ_GENERAL], BQ_SOLDIER - BQ_GENERAL + 1, 0.86f, 0.12f, 0.6f);
}

/* a player's (a seat's) general, in its colour; before the first flip a piece face down (no colour yet) */
static void token(int player, int size, uint8_t *rgba)
{
    if (bq_pos(&G)->decided) xq_piece_paint(XQ_GENERAL, bq_colour(bq_pos(&G), player), size, rgba);
    else back_pic(size, rgba);
}

/* the menu's card: a corner of the board, its pieces mostly face down, one turning over */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (4 + 2 * FRAME)), f = (int)(s * FRAME), n = 4 * s + 2 * f, o = (size - n) / 2;
    uint8_t *board = ui_buf(n, n);
    paint_board(board, 1, 4, 4, s, f, 1);
    art_blend(rgba, size, size, board, n, n, o, o);
    /* column, row from the top left, the piece (0: face down) */
    static const int place[][3] = {{0, 0, 0}, {1, 0, 0}, {2, 0, BQ_PC(1, BQ_CANNON)}, {3, 0, 0},
                                   {0, 1, BQ_PC(0, BQ_CHARIOT)}, {2, 1, 0}, {3, 1, 0}, {0, 2, 0},
                                   {1, 2, BQ_PC(0, BQ_GENERAL)}, {2, 2, 0}, {3, 2, BQ_PC(1, BQ_HORSE)}, {1, 3, 0},
                                   {2, 3, 0}, {3, 3, 0}};
    uint8_t *one = malloc((size_t)s * s * 4);
    if (!one) return;
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        memset(one, 0, (size_t)s * s * 4);
        if (place[i][2]) face_pic(place[i][2], s, one);
        else back_pic(s, one);
        art_flat_put(rgba, size, size, one, s, o + f + (place[i][0] + 0.5f) * s, o + f + (place[i][1] + 0.5f) * s);
    }
    free(one);
}

static int busy(void) { return anim.on; }     /* the view and the end wait for the piece to land */

static const game banqi_game = {
    .id = "banqi", .order = 166, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .view = VIEW_OPEN,
};
REGISTER_GAME(banqi_game)
