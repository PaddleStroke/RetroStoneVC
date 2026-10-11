/*
 * Xiangqi (Chinese chess) in RetroStone Board: the board with its river and palaces, the wooden discs and their
 * characters, tap or drag (the rules: xiangqi_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The script's notation (the screenshots, the tests): a move is its two points, files a..i from red's left, ranks
 * 0..9 from red's side: "h2e2 h9g7 h0g2 i9h9" (red's cannon to the middle, black's horse out, ...).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/grid.h"
#include "games/xiangqi.h"
#include "shell.h"
#include "text.h"

#define FRAME 0.34f                             /* the board's frame, in points' squares */
#define ANIM_MS 150

static xgame G;
static xmove legal[XQ_MAX_MOVES];
static int nlegal, state = GS_PLAY;
static int sq, fr, made = -1, made_small = -1;
static float bx, by;                            /* the plank's top left corner (inside the frame) */
static SDL_Texture *t_piece[2][8], *t_small[2][8], *t_board;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; xmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = xq_legal(xq_pos(&G), legal);
    switch (xq_status(&G)) {
    case XQ_PLAY: state = GS_PLAY; break;
    case XQ_MATE:
    case XQ_STALEMATE: state = xq_pos(&G)->side ? GS_WIN0 : GS_WIN1; break;   /* no move: that player lost */
    default: state = GS_DRAW;
    }
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    xq_new(&G);
    anim.on = 0;
    refresh();
}

static int status(void) { return state; }
static int turn(void) { return xq_pos(&G)->side; }

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static void play(xmove m, int animate)
{
    xq_play(&G, m);
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.cap ? SND_CAPTURE : SND_MOVE);
    if (state == GS_PLAY && xq_in_check(xq_pos(&G), xq_pos(&G)->side)) ui_sound(SND_CHECK);
}

static int undo(void)
{
    if (!xq_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "h2e2 h9g7": the screenshots' and the tests' moves */
static int script(const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int k = xq_parse(legal, nlegal, s);
        if (k < 0) return 0;
        play(legal[k], 0);
        s += 4;
    }
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the characters, XQ_GENERAL..XQ_SOLDIER: red's set, black's set (help/glyphs.txt keeps them in the fonts) */
static const int CHARS[2][8] = {
    {0, 0x5E25, 0x4ED5, 0x76F8, 0x508C, 0x4FE5, 0x70AE, 0x5175},   /* 帥仕相傌俥炮兵 */
    {0, 0x5C07, 0x58EB, 0x8C61, 0x99AC, 0x8ECA, 0x7832, 0x5352},   /* 將士象馬車砲卒 */
};
static const uint8_t INK[2][3] = {{176, 30, 26}, {32, 28, 26}};   /* red ink, ebony ink */

/* a piece: an ivory wooden disc, an engraved ring and the character in its player's ink, size x size */
static void piece_pic(int type, int player, int size, uint8_t *rgba)
{
    art_style st = art_player[0];
    st.line = fmaxf(0.022f, 1.3f / size);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.45f, 0.45f);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
    uint8_t *layer = calloc((size_t)size * size, 4);
    if (!layer) return;
    int d = (int)(0.78f * size);
    d -= (size - d) & 1;                        /* centred to the pixel */
    art_ring(d, 0.91f, INK[player], 0.9f, layer);
    art_blend(rgba, size, size, layer, d, d, (size - d) / 2, (size - d) / 2);
    memset(layer, 0, (size_t)size * size * 4);
    text_glyph(CHARS[player][type], 0.5f * size, 1, INK[player], layer, size, size);
    art_blend(rgba, size, size, layer, size, size, 0, 0);
    free(layer);
}

/* the diagonal lines (the palaces'): a segment's coverage, half-width hw, into the map (stride st) */
static void cover_seg(float *m, int st, float x0, float y0, float x1, float y1, float hw)
{
    float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    for (int y = (int)(fminf(y0, y1) - hw - 1); y <= (int)(fmaxf(y0, y1) + hw + 1); y++)
        for (int x = (int)(fminf(x0, x1) - hw - 1); x <= (int)(fmaxf(x0, x1) + hw + 1); x++) {
            if (x < 0 || y < 0 || x >= st || y >= st) continue;
            float px = x + 0.5f - x0, py = y + 0.5f - y0, t = fminf(fmaxf((px * dx + py * dy) / len2, 0), 1);
            float c = fminf(fmaxf(hw - hypotf(px - t * dx, py - t * dy) + 0.5f, 0), 1);
            if (c > m[y * st + x]) m[y * st + x] = c;
        }
}

/* the board: the plank in its frame and its lines, the part of the board from point (x0, y0) (its bottom left), nx x
 * ny points of s pixels, frame f. Lines going on past the part run to the frame (the menu's card). */
static void paint(uint8_t *o, int x0, int y0, int nx, int ny, int s, int f)
{
    static const uint8_t LINE[3] = {34, 24, 16};
    int w = nx * s + 2 * f, h = ny * s + 2 * f, st = w > h ? w : h;
    art_board_n(nx, ny, s, f, 0, o);
    float *m = calloc((size_t)st * st, sizeof *m);   /* square: grid_cover_rect's map */
    if (!m) return;
    float lw = 0.9f + 0.02f * s, edge = 1.8f * lw, mark = 0.09f * s, arm = 0.22f * s;
    float left = (float)f, right = (float)(f + nx * s), top = (float)f, bottom = (float)(f + ny * s);
#define PX(x) (f + ((x) - x0) * s + s / 2.0f)
#define PY(y) (f + (y0 + ny - 1 - (y)) * s + s / 2.0f)
    /* an upright rectangle, cut to the plank */
#define RECT(a, b, c, d) grid_cover_rect(m, st, fmaxf(a, left), fmaxf(b, top), fminf(c, right), fminf(d, bottom))
    for (int y = 0; y < XQ_H; y++) {            /* the ranks */
        float t = y == 0 || y == XQ_H - 1 ? edge : lw;
        RECT(PX(0) - edge / 2, PY(y) - t / 2, PX(XQ_W - 1) + edge / 2, PY(y) + t / 2);
    }
    for (int x = 0; x < XQ_W; x++) {            /* the files, the inner ones stopping at the river */
        float t = x == 0 || x == XQ_W - 1 ? edge : lw, e = edge / 2;
        if (t == edge) RECT(PX(x) - t / 2, PY(XQ_H - 1) - e, PX(x) + t / 2, PY(0) + e);
        else {
            RECT(PX(x) - t / 2, PY(XQ_H - 1) - e, PX(x) + t / 2, PY(5));
            RECT(PX(x) - t / 2, PY(4), PX(x) + t / 2, PY(0) + e);
        }
    }
    for (int p = 0; p < 2; p++) {               /* the palaces' diagonals */
        int ya = p ? 7 : 0, yb = ya + 2;
        cover_seg(m, st, PX(3), PY(ya), PX(5), PY(yb), lw / 2);
        cover_seg(m, st, PX(5), PY(ya), PX(3), PY(yb), lw / 2);
    }
    static const int marks[][2] = {{1, 2}, {7, 2}, {1, 7}, {7, 7}, {0, 3}, {2, 3}, {4, 3}, {6, 3}, {8, 3},
                                   {0, 6}, {2, 6}, {4, 6}, {6, 6}, {8, 6}};
    for (unsigned i = 0; i < sizeof marks / sizeof marks[0]; i++)   /* the cannons' and soldiers' points */
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2) {
                int x = marks[i][0], y = marks[i][1];
                if (x + sx < 0 || x + sx >= XQ_W) continue;   /* not past the board's edge */
                float cx = PX(x) + sx * mark, cy = PY(y) + sy * mark, ex = cx + sx * arm, ey = cy + sy * arm;
                RECT(fminf(cx, ex) - lw / 2, cy - lw / 2, fmaxf(cx, ex) + lw / 2, cy + lw / 2);
                RECT(cx - lw / 2, fminf(cy, ey) - lw / 2, cx + lw / 2, fmaxf(cy, ey) + lw / 2);
            }
#undef RECT
#undef PX
#undef PY
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float a = 0.9f * m[y * st + x];
            uint8_t *q = o + 4 * (y * w + x);
            for (int j = 0; j < 3; j++) q[j] = (uint8_t)(q[j] * (1 - a) + LINE[j] * a + 0.5f);
        }
    free(m);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    sq = (int)floorf(fminf(b.w / (XQ_W + 2 * FRAME), b.h / (XQ_H + 2 * FRAME)));
    fr = (int)(sq * FRAME);
    int w = XQ_W * sq + 2 * fr, h = XQ_H * sq + 2 * fr;
    bx = floorf(b.x + (b.w - w) / 2) + fr;
    by = floorf(b.y + (b.h - h) / 2) + fr;
    if (made != sq) {
        made = sq;
        for (int c = 0; c < 2; c++)
            for (int t = XQ_GENERAL; t <= XQ_SOLDIER; t++) {
                ui_free(&t_piece[c][t]);
                piece_pic(t, c, sq, ui_buf(sq, sq));
                t_piece[c][t] = ui_tex(sq, sq, ui_buf(sq, sq));
            }
        ui_free(&t_board);
        paint(ui_buf(w, h), 0, 0, XQ_W, XQ_H, sq, fr);
        t_board = ui_tex(w, h, ui_buf(w, h));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++)
            for (int t = XQ_GENERAL; t <= XQ_SOLDIER; t++) {
                ui_free(&t_small[c][t]);
                piece_pic(t, c, small, ui_buf(small, small));
                t_small[c][t] = ui_tex(small, small, ui_buf(small, small));
            }
    }
}

/* a point's centre on the screen */
static void pt_xy(int p, float *x, float *y)
{
    int c = XQ_X(p), r = XQ_H - 1 - XQ_Y(p);
    if (ui.flipped) { c = XQ_W - 1 - c; r = XQ_H - 1 - r; }
    *x = bx + c * sq + sq / 2.0f;
    *y = by + r * sq + sq / 2.0f;
}

/* the point under (x, y), or -1 off the board */
static int xy_pt(float x, float y)
{
    int c = (int)floorf((x - bx) / sq), r = (int)floorf((y - by) / sq);
    if (c < 0 || c >= XQ_W || r < 0 || r >= XQ_H) return -1;
    if (ui.flipped) { c = XQ_W - 1 - c; r = XQ_H - 1 - r; }
    return XQ_PT(c, XQ_H - 1 - r);
}

/* a piece centred on (x, y), turned round for the top player face to face: its character faces them */
static void piece_at(int pc, float x, float y, float size)
{
    SDL_Texture *t = t_piece[XQ_OWNER(pc)][XQ_TYPE(pc)];
    if (ui_upside(XQ_OWNER(pc))) ui_put_rot(t, x, y, size, size, 180);
    else ui_put_c(t, x, y, size, size);
}

static void draw(void)
{
    const xpos *p = xq_pos(&G);
    float x, y, h = sq / 2.0f;
    ui_put(t_board, bx - fr, by - fr, (float)(XQ_W * sq + 2 * fr), (float)(XQ_H * sq + 2 * fr));
    if (G.ply) {
        xmove m = G.move[G.ply - 1];
        pt_xy(m.from, &x, &y);
        ui_mark_last(x - h, y - h, sq, sq);
        pt_xy(m.to, &x, &y);
        ui_mark_last(x - h, y - h, sq, sq);
    }
    if (sel >= 0) {
        pt_xy(sel, &x, &y);
        ui_mark_select(x - h, y - h, sq, sq);
    }
    if (state != GS_DRAW && xq_in_check(p, p->side)) {
        pt_xy(xq_general(p, p->side), &x, &y);
        ui_mark_glow(x, y, sq);
    }
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)ANIM_MS;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    for (int pass = 0; pass < 2; pass++)        /* the shadows first: a shadow falls on the wood only */
        for (int s = 0; s < XQ_PTS; s++) {
            if (!p->sq[s] || (drag.on && drag.moved && s == drag.from) || (anim.on && s == anim.m.to)) continue;
            pt_xy(s, &x, &y);
            if (pass) piece_at(p->sq[s], x, y, (float)sq);
            else ui_shadow_flat(x, y, (float)sq, (float)sq, 1);
        }
    if (sel >= 0) {                             /* where the selected piece can go */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            pt_xy(legal[i].to, &x, &y);
            if (legal[i].cap) ui_mark_ring(x, y, sq);
            else ui_mark_dot(x, y, 0.32f * sq);
        }
    }
    if (anim.on) {
        float x0, y0, x1, y1;
        pt_xy(anim.m.from, &x0, &y0);
        pt_xy(anim.m.to, &x1, &y1);
        x = x0 + (x1 - x0) * t;
        y = y0 + (y1 - y0) * t;
        ui_shadow_flat(x, y, (float)sq, (float)sq, 1);
        piece_at(p->sq[anim.m.to], x, y, (float)sq);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? 0.25f : -0.25f;
        x = drag.x;
        y = drag.y + s * off;
        ui_shadow_flat(x, y, s, s, 1);
        piece_at(p->sq[drag.from], x, y, s);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (state != GS_PLAY) return;
    int s = xy_pt(x, y);
    if (s < 0) { sel = -1; return; }
    if (sel >= 0 && target(sel, s) >= 0) {
        play(legal[target(sel, s)], 1);
        return;
    }
    const xpos *p = xq_pos(&G);
    if (p->sq[s] && XQ_OWNER(p->sq[s]) == p->side) {
        sel = s;
        drag.on = 1;
        drag.from = s;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    if (sel >= 0) ui_sound(SND_DENY);
    sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int s = xy_pt(x, y), k = s < 0 ? -1 : target(drag.from, s);
    if (k >= 0) play(legal[k], 0);
    else if (s != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pieces a player took from the other: small discs, the strongest first */
static void bar(int player, box b)
{
    static const int start_n[8] = {0, 1, 2, 2, 2, 2, 2, 5},
                     order[6] = {XQ_CHARIOT, XQ_CANNON, XQ_HORSE, XQ_ELEPHANT, XQ_ADVISOR, XQ_SOLDIER};
    const xpos *p = xq_pos(&G);
    int c = !player, gone[8] = {0}, total = 0, groups = 0;
    for (int t = 0; t < 8; t++) gone[t] = start_n[t];
    for (int s = 0; s < XQ_PTS; s++)
        if (p->sq[s] && XQ_OWNER(p->sq[s]) == c) gone[XQ_TYPE(p->sq[s])]--;
    for (int k = 0; k < 6; k++) {
        if (gone[order[k]] <= 0) continue;
        total += gone[order[k]];
        groups++;
    }
    if (!total) return;
    float sz = 0.6f * ui.unit, gap = 0.12f * ui.unit, x = b.x + sz / 2, cy = b.y + b.h / 2;
    float step = total > 1 ? fminf(0.86f * sz, (b.w - sz - (groups - 1) * gap) / (total - 1)) : 0;
    for (int k = 0; k < 6; k++) {
        int t = order[k];
        if (gone[t] <= 0) continue;
        for (int i = 0; i < gone[t]; i++, x += step) ui_put_c(t_small[c][t], x, cy, sz, sz);
        x += gap;
    }
}

static void token(int player, int size, uint8_t *rgba) { piece_pic(XQ_GENERAL, player, size, rgba); }

/* the menu's card: the red palace and the points around it, a few pieces */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int n = 5, s = (int)(size / (n + 2 * FRAME)), f = (int)(s * FRAME), w = n * s + 2 * f, o = (size - w) / 2;
    uint8_t *board = ui_buf(w, w);
    paint(board, 2, 0, n, n, s, f);
    art_blend(rgba, size, size, board, w, w, o, o);
    static const int place[][3] = {{2, 4, XQ_GENERAL}, {1, 4, XQ_ADVISOR}, {4, 4, XQ_ELEPHANT}, {2, 2, XQ_CANNON},
                                   {0, 1, XQ_SOLDIER}, {3, 0, 8 + XQ_CHARIOT}, {1, 0, 8 + XQ_HORSE}};
    uint8_t *one = calloc((size_t)s * s, 4);
    if (!one) return;
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        piece_pic(place[i][2] & 7, place[i][2] >> 3, s, one);
        art_blend(rgba, size, size, one, s, s, o + f + place[i][0] * s, o + f + place[i][1] * s);
    }
    free(one);
}

static int busy(void) { return anim.on; }     /* the end waits for the last piece to arrive */

static const game xiangqi_game = {
    .id = "xiangqi", .order = 160, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(xiangqi_game)
