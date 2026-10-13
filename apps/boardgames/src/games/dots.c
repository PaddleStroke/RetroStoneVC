/*
 * Dots and boxes in RetroStone Board: the dots on one plank, the lines in the players' colours, the boxes they claim;
 * a finger shows the nearest line while it is down and draws it when lifted (the rules and the notation: dots_rules.c,
 * dots.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dots.h"
#include "shell.h"

#define FRAME 0.035f                            /* the board's frame, a part of its side (a dot in each cell) */
#define LINE_W 0.13f                            /* a line's width, in cells; at most LINE_B of the board */
#define LINE_B 0.018f
#define DOT 0.25f                               /* a dot's size, in cells; at most DOT_B of the board */
#define DOT_B 0.036f
#define COUNTER 0.36f                           /* the counter in a claimed box, in cells */
#define REACH 0.62f                             /* how far from a line a finger still finds it, in cells */
#define ANIM_MS 200

static dgame G;
static int n = 5;                               /* boxes on a side */
static int sq, fr, made = -1, made_n = -1;
static float bx, by;                            /* the plank's top left corner, inside the frame */
static int lw, lh;                              /* an across line's picture, pixels (an up line's is lh x lw) */
static int sw, sh;                              /* an across line's shadow picture */
static SDL_Texture *t_board, *t_line[2][2], *t_shadow[2], *t_dot, *t_box[2], *t_counter[2];
static struct { int on, line; } hold;           /* the finger is down: the line it shows */
static struct { int on, line; Uint32 t0; } anim;
static const uint8_t TINT[2][4] = {{252, 246, 230, 165}, {48, 36, 30, 125}};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int variant)
{
    n = variant == 0 ? 3 : variant == 2 ? 7 : 5;
    dots_new(&G, n);
    anim.on = hold.on = 0;
}

static int status(void)
{
    switch (dots_status(&G)) {
    case DS_PLAY: return GS_PLAY;
    case DS_WIN0: return GS_WIN0;
    case DS_WIN1: return GS_WIN1;
    default: return GS_DRAW;
    }
}

static int turn(void) { return dots_pos(&G)->side; }

static void play(int line, int animate)
{
    int claimed = dots_play(&G, line);
    if (claimed < 0) return;
    anim.on = animate;
    anim.line = line;
    anim.t0 = ui_ms();
    ui_sound(SND_PLACE);
    if (claimed) ui_sound(SND_CHECK);           /* a box: a chime, and the same player again */
}

/* the bot's side (bots.h): the game, read-only; a line in the notation ("h1,1"), drawn as a tap does */
const dgame *dots_live(void) { return &G; }

int dots_show(const char *move)
{
    const dpos *p = dots_pos(&G);
    int len, line = dots_parse(p->n, move, &len);
    if (line < 0 || move[len] || p->line[line] >= 0 || status() != GS_PLAY) return 0;
    hold.on = 0;
    play(line, 1);
    return 1;
}

static int undo(void)
{
    if (!dots_undo(&G)) return 0;
    anim.on = hold.on = 0;
    return 1;
}

/* "h1,1 v2,1": the screenshots' and the tests' lines (dots.h) */
static int script(const char *s)
{
    int ok = dots_script(&G, s);
    anim.on = hold.on = 0;
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a line's width and a dot's size on a board of nn x nn boxes, in cells: thinner on the big cells of the small board */
static float line_k(int nn) { return fminf(LINE_W, LINE_B * (nn + 1) / (1 - 2 * FRAME)); }
static float dot_k(int nn) { return fminf(DOT, DOT_B * (nn + 1) / (1 - 2 * FRAME)); }

/* the cell and the frame (which takes what is left) for a board of nn x nn boxes in a square of side px */
static void fit(int nn, float side, int *s, int *f)
{
    *s = (int)(side * (1 - 2 * FRAME) / (nn + 1));
    *f = (int)((side - (nn + 1) * *s) / 2);
}

/* a rounded bar (a line), w x h in the unit square's units, from (x, y) */
static void bar_shape(art_fig *f, float x, float y, float w, float h)
{
    float r = fminf(w, h) / 2, pt[2 * 24];
    int k = 0;
    for (int i = 0; i < 24; i++) {              /* two half circles, joined */
        float a = (i < 12 ? -90 + i * 180 / 11.0f : 90 + (i - 12) * 180 / 11.0f) * 3.14159265f / 180;
        int end = i < 12;                       /* the right (or lower) end first */
        float cx = w >= h ? (end ? x + w - r : x + r) : x + r, cy = w >= h ? y + r : (end ? y + h - r : y + r);
        float ax = cosf(a), ay = sinf(a);
        if (w < h) { float t = ax; ax = -ay; ay = t; }   /* turned a quarter: the ends below and above */
        pt[k++] = cx + r * ax;
        pt[k++] = cy + r * ay;
    }
    art_poly(f, ART_PART, pt, 24);
}

/* a player's line between two dots s pixels apart, across or up: its picture and size */
static void pic_line(int nn, int c, int across, int s, uint8_t *out, int *w, int *h)
{
    float len = (float)s, k = line_k(nn), wid = k * s;
    int a = (int)ceilf(len) + 4, b = (int)ceilf(wid) + 4;   /* 2 pixels round it: the outline */
    *w = across ? a : b;
    *h = across ? b : a;
    art_style st = art_player[c];
    st.light = 0.12f;
    st.line = fmaxf(0.012f, 1.1f / s);
    art_fig *f = art_fig_new();
    bar_shape(f, 0, 0, across ? 1 : k, across ? k : 1);
    art_render(f, &st, *w, *h, (float)s, 2, 2, out);
    art_fig_free(f);
}

/* the soft shadow of a line: the bar in black, blurred */
static void pic_shadow(int nn, int across, int s, uint8_t *out, int *w, int *h)
{
    float k = line_k(nn), wid = k * s;
    int pad = (int)ceilf(0.5f * wid) + 1, a = s + 2 * pad, b = (int)ceilf(wid) + 2 * pad, r = (int)(0.3f * wid) + 1;
    *w = across ? a : b;
    *h = across ? b : a;
    static const art_style black = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 0, 0};
    art_fig *f = art_fig_new();
    bar_shape(f, 0, 0, across ? 1 : k, across ? k : 1);
    art_render(f, &black, *w, *h, (float)s, (float)pad, (float)pad, out);
    art_fig_free(f);
    int W = *w, H = *h;
    uint8_t *tmp = malloc((size_t)W * H);
    if (!tmp) return;
    for (int pass = 0; pass < 2; pass++) {      /* a box blur, across then down */
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int sum = 0;
                for (int d = -r; d <= r; d++) {
                    int xx = pass ? x : x + d, yy = pass ? y + d : y;
                    if (xx >= 0 && yy >= 0 && xx < W && yy < H) sum += out[4 * (yy * W + xx) + 3];
                }
                tmp[y * W + x] = (uint8_t)(sum / (2 * r + 1));
            }
        for (int i = 0; i < W * H; i++) out[4 * i + 3] = tmp[i];
    }
    free(tmp);
}

static void pic_dot(int nn, int s, uint8_t *out, int *d)
{
    static const art_style ink = {{70, 48, 34}, {34, 22, 16}, {34, 22, 16}, {0, 0, 0}, 0.55f, 0};
    art_style st = ink;
    *d = (int)ceilf(dot_k(nn) * s) | 1;
    st.line = fmaxf(0.03f, 1.0f / *d);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.48f - st.line, 0.48f - st.line);   /* its outline inside */
    art_render(f, &st, *d, *d, (float)*d, 0, 0, out);
    art_fig_free(f);
}

static int box_px(int nn, int s) { return (int)(s - line_k(nn) * s - 0.12f * s); }

static void pic_box(int nn, int c, int s, uint8_t *out)
{
    art_roundrect(box_px(nn, s), box_px(nn, s), 0.1f * s, TINT[c], out);
}

static void tex(SDL_Texture **t, int w, int h)
{
    ui_free(t);
    *t = ui_tex(w, h, ui_buf(w, h));
}

static void layout(box b)
{
    int cells = n + 1;
    fit(n, b.w, &sq, &fr);
    int px = cells * sq + 2 * fr;
    bx = floorf(b.x + (b.w - px) / 2) + fr;
    by = floorf(b.y + (b.h - px) / 2) + fr;
    if (made == sq && made_n == n) return;
    made = sq;
    made_n = n;
    int w, h, s = sq;
    art_board_n(cells, cells, s, fr, 0, ui_buf(px, px));
    tex(&t_board, px, px);
    for (int c = 0; c < 2; c++) {
        for (int o = 0; o < 2; o++) {
            pic_line(n, c, !o, s, ui_buf(s + 8, s + 8), &w, &h);
            tex(&t_line[c][o], w, h);
            if (!o) lw = w, lh = h;
        }
        pic_box(n, c, s, ui_buf(box_px(n, s), box_px(n, s)));
        tex(&t_box[c], box_px(n, s), box_px(n, s));
        int k = (int)(COUNTER * s);
        art_counter(COUNTER_DISC, c, k, ui_buf(k, k));
        tex(&t_counter[c], k, k);
    }
    for (int o = 0; o < 2; o++) {
        pic_shadow(n, !o, s, ui_buf(2 * s, 2 * s), &w, &h);
        tex(&t_shadow[o], w, h);
        if (!o) sw = w, sh = h;
    }
    pic_dot(n, s, ui_buf(s, s), &w);
    tex(&t_dot, w, w);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void dot_xy(int x, int y, float *cx, float *cy)
{
    if (ui.flipped) { x = n - x; y = n - y; }
    *cx = bx + (x + 0.5f) * sq;
    *cy = by + (n - y + 0.5f) * sq;
}

static int line_xy(int line, float *x0, float *y0, float *x1, float *y1)   /* its ends; 1: across */
{
    int a, b, c, d;
    dots_ends(n, line, &a, &b, &c, &d);
    dot_xy(a, b, x0, y0);
    dot_xy(c, d, x1, y1);
    return b == d;
}

static void box_xy(int box, float *cx, float *cy)
{
    float x0, y0, x1, y1;
    dot_xy(box % n, box / n, &x0, &y0);
    dot_xy(box % n + 1, box / n + 1, &x1, &y1);
    *cx = (x0 + x1) / 2;
    *cy = (y0 + y1) / 2;
}

/* the undrawn line nearest to (x, y), within reach; -1 */
static int nearest(float x, float y)
{
    const dpos *p = dots_pos(&G);
    int best = -1;
    float bd = REACH * sq;
    for (int l = 0; l < dots_lines(n); l++) {
        if (p->line[l] >= 0) continue;
        float x0, y0, x1, y1;
        line_xy(l, &x0, &y0, &x1, &y1);
        float ex = x1 - x0, ey = y1 - y0, t = ((x - x0) * ex + (y - y0) * ey) / (ex * ex + ey * ey);
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        float d = hypotf(x - x0 - t * ex, y - y0 - t * ey);
        if (d < bd) { bd = d; best = l; }
    }
    return best;
}

/* the band along a line (the last one, the one the finger shows) */
static void band(int line, void (*mark)(float, float, float, float))
{
    float x0, y0, x1, y1, w = 0.4f * sq;
    int across = line_xy(line, &x0, &y0, &x1, &y1);
    float mx = (x0 + x1) / 2, my = (y0 + y1) / 2;
    if (across) mark(mx - sq / 2.0f, my - w / 2, (float)sq, w);
    else mark(mx - w / 2, my - sq / 2.0f, w, (float)sq);
}

/* a line of player c; k: how much wider it shows (set down), lift: its shadow's distance, alpha 0..1 */
static void line_at(int c, int line, float k, float lift, float alpha)
{
    float x0, y0, x1, y1;
    int across = line_xy(line, &x0, &y0, &x1, &y1);
    float mx = (x0 + x1) / 2, my = (y0 + y1) / 2, o = lift * sq;
    SDL_Texture *s = t_shadow[!across], *t = t_line[c][!across];
    SDL_SetTextureAlphaMod(s, (Uint8)(110 * alpha));
    if (across) ui_put_c(s, mx + 0.6f * o, my + o, (float)sw, sh * k);
    else ui_put_c(s, mx + 0.6f * o, my + o, sh * k, (float)sw);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    if (across) ui_put_c(t, mx, my, (float)lw, lh * k);
    else ui_put_c(t, mx, my, lh * k, (float)lw);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw(void)
{
    const dpos *p = dots_pos(&G);
    int px = (n + 1) * sq + 2 * fr, last = G.ply ? G.move[G.ply - 1] : -1, boxes = n * n;
    ui_put(t_board, bx - fr, by - fr, (float)px, (float)px);
    float t = 1, x, y;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)ANIM_MS;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    if (last >= 0) band(last, ui_mark_last);
    for (int b = 0; b < boxes; b++) {           /* the boxes the last line closed: a gold light */
        int s[4];
        dots_sides(n, b, s);
        if (p->box[b] >= 0 && (s[0] == last || s[1] == last || s[2] == last || s[3] == last)) {
            box_xy(b, &x, &y);
            ui_mark_gold(x, y, 1.5f * sq);
        }
    }
    for (int b = 0; b < boxes; b++) {
        if (p->box[b] < 0) continue;
        int s[4], fresh = 0;
        dots_sides(n, b, s);
        for (int i = 0; i < 4; i++) fresh |= anim.on && s[i] == anim.line;
        float a = fresh ? t : 1, d = COUNTER * sq * (fresh ? 1.3f - 0.3f * t : 1);
        int c = p->box[b];
        box_xy(b, &x, &y);
        SDL_SetTextureAlphaMod(t_box[c], (Uint8)(255 * a));
        ui_put_c(t_box[c], x, y, (float)box_px(n, sq), (float)box_px(n, sq));
        ui_shadow_flat(x, y, d, d, a);
        ui_put_c(t_counter[c], x, y, d, d);
    }
    for (int l = 0; l < dots_lines(n); l++) {
        if (p->line[l] < 0) continue;
        if (anim.on && l == anim.line) line_at(p->line[l], l, 1.5f - 0.5f * t, 0.12f - 0.06f * t, 1);
        else line_at(p->line[l], l, 1, 0.06f, 1);
    }
    if (hold.on && hold.line >= 0 && status() == GS_PLAY) {   /* the line the finger shows */
        band(hold.line, ui_mark_select);
        line_at(p->side, hold.line, 1, 0.1f, 0.6f);
    }
    for (int dy = 0; dy <= n; dy++)
        for (int dx = 0; dx <= n; dx++) {
            float d = dot_k(n) * sq;
            dot_xy(dx, dy, &x, &y);
            ui_put_c(t_dot, x, y, d, d);
        }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    hold.on = 1;
    hold.line = nearest(x, y);
    if (hold.line < 0) ui_sound(SND_DENY);
}

static void motion(float x, float y)
{
    if (!hold.on) return;
    hold.line = nearest(x, y);
    ui_animate();
}

static void release(float x, float y)
{
    if (!hold.on) return;
    hold.on = 0;
    int line = nearest(x, y);                   /* slid off the board: nothing */
    if (line >= 0 && status() == GS_PLAY) play(line, 1);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the boxes the player owns */
static void bar(int player, box b)
{
    float u = ui.unit;
    ui_number(b.x + 0.45f * u, b.y + b.h / 2, 0.46f * u, dots_pos(&G)->boxes[player], 1);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* a picture of a game going on, for the cards: nn x nn boxes, the lines ivory and ebony drew ("h1,1 v2,1"), the
 * boxes each one claimed (their four sides drawn too) */
static const struct { int n; const char *line[2]; const char *box[2]; } CARD[3] = {
    {3, {"h1,1 v3,1 h3,3 v4,2", "h2,2 v1,2 h1,1"}, {"h1,3", "h2,3"}},
    {5, {"h1,1 v4,1 h3,3 v6,3 h4,5 v2,2", "h2,2 v1,4 h5,1 v5,2 h3,4"}, {"h1,5 h3,2", "h2,5 h4,3"}},
    {7, {"h1,1 v4,1 h3,3 v8,3 h4,5 v2,2 h6,7 v6,5", "h2,2 v1,4 h5,1 v5,2 h3,4 v7,6 h6,1"},
        {"h1,7 h3,2 h5,5", "h2,7 h4,3 h6,4"}},
};

static void card_pos(int v, dpos *p)
{
    int nn = CARD[v].n, len, s[4];
    dots_start(p, nn);
    for (int c = 0; c < 2; c++) {
        for (const char *q = CARD[v].box[c]; *q; q += *q == ' ') {   /* a box, named by its lower left dot */
            int l = dots_parse(nn, q, &len), x0, y0, x1, y1;
            if (l < 0) break;
            q += len;
            dots_ends(nn, l, &x0, &y0, &x1, &y1);
            int b = y0 * nn + x0;
            p->box[b] = (int8_t)c;
            dots_sides(nn, b, s);
            for (int i = 0; i < 4; i++)
                if (p->line[s[i]] < 0) p->line[s[i]] = (int8_t)(i == 3 ? c : (c ^ (i & 1)));
        }
        for (const char *q = CARD[v].line[c]; *q; q += *q == ' ') {
            int l = dots_parse(nn, q, &len);
            if (l < 0) break;
            q += len;
            if (p->line[l] < 0) p->line[l] = (int8_t)c;
        }
    }
}

/* the menu's card and the picker's: the board of that size with a game going on (the menu: the small one) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static dpos P;
    int v = variant < 0 ? 0 : variant > 2 ? 2 : variant;
    card_pos(v, &P);
    int nn = P.n, cells = nn + 1, s, f;
    fit(nn, (float)size, &s, &f);
    int w = cells * s + 2 * f, o = (size - w) / 2, pw, ph;
    art_board_n(cells, cells, s, f, 0, ui_buf(w, w));
    art_blend(rgba, size, size, ui_buf(w, w), w, w, o, o);
    float ox = o + f + 0.5f * s, oy = o + f + 0.5f * s;   /* the top left dot */
    for (int b = 0; b < nn * nn; b++) {
        if (P.box[b] < 0) continue;
        int k = box_px(nn, s), d = (int)(COUNTER * s);
        float cx = ox + (b % nn + 0.5f) * s, cy = oy + (nn - b / nn - 0.5f) * s;
        pic_box(nn, P.box[b], s, ui_buf(k, k));
        art_blend(rgba, size, size, ui_buf(k, k), k, k, (int)lroundf(cx - k / 2.0f), (int)lroundf(cy - k / 2.0f));
        art_counter(COUNTER_DISC, P.box[b], d, ui_buf(d, d));
        art_blend(rgba, size, size, ui_buf(d, d), d, d, (int)lroundf(cx - d / 2.0f), (int)lroundf(cy - d / 2.0f));
    }
    uint8_t *one = ui_buf(2 * s + 4, 2 * s + 4);             /* each picture in turn, painted over the card */
    for (int l = 0; l < dots_lines(nn); l++) {
        if (P.line[l] < 0) continue;
        int x0, y0, x1, y1;
        dots_ends(nn, l, &x0, &y0, &x1, &y1);
        float mx = ox + (x0 + x1) * 0.5f * s, my = oy + (nn - (y0 + y1) * 0.5f) * s;
        pic_shadow(nn, y0 == y1, s, one, &pw, &ph);
        for (int i = 0; i < pw * ph; i++) one[4 * i + 3] = (uint8_t)(one[4 * i + 3] * 0.4f);
        art_blend(rgba, size, size, one, pw, ph, (int)lroundf(mx - pw / 2.0f + 0.04f * s),
                  (int)lroundf(my - ph / 2.0f + 0.06f * s));
        pic_line(nn, P.line[l], y0 == y1, s, one, &pw, &ph);
        art_blend(rgba, size, size, one, pw, ph, (int)lroundf(mx - pw / 2.0f), (int)lroundf(my - ph / 2.0f));
    }
    int d;
    pic_dot(nn, s, one, &d);
    for (int dy = 0; dy <= nn; dy++)
        for (int dx = 0; dx <= nn; dx++)
            art_blend(rgba, size, size, one, d, d, (int)lroundf(ox + dx * s - d / 2.0f),
                      (int)lroundf(oy + dy * s - d / 2.0f));
}

static int busy(void) { return anim.on; }     /* the end waits for the last line to settle */

static const game dots_game = {
    .id = "dots", .order = 80, .variants = 3,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(dots_game)
