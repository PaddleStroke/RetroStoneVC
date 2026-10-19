/*
 * Jigsaw in RetroStone Board, for one player: a picture (the 1898 tarot's trumps and courts, public-domain paintings)
 * cut into pieces with tabs and blanks, each puzzle its own cut. The pieces wait in a tray along the table's bottom
 * (its right on a wide screen), scrolled with a finger; dragged out onto the table they lie where they are dropped,
 * lock into their place when they come near it, and join a neighbour lying near it the right way round: joined pieces
 * move together. A tap picks a piece up (in gold light) and a tap on the table puts it there; in a turned puzzle a
 * second tap turns it a quarter. The tools: the lamp (a piece flies to its place), the eye (the picture's ghost on
 * the board) and the frame (only the border's pieces in the tray). Solved: a wave of gold over the picture, then the
 * shell's stars card (the hints and the time).
 * The levels: chapters of a picture set each, the pieces growing (jigsaw_levels.h, tools/jigsaw_gen.c); the die tab:
 * any picture at the count chosen, upright or turned.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/jigsaw.h"
#include "games/jigsaw_art.h"
#define JIG_LEVEL_COUNT_ONLY
#include "games/jigsaw_levels.h"
#include "games/puzzles.h"
#include "shell.h"

#define STBI_ONLY_JPEG                          /* the declarations only: cards.c holds the decoder */
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "stb_image.h"

enum { T_HINT, T_GHOST, T_EDGES, TOOLS };

#define GLIDE_MS 140                            /* a piece settling where it snapped */
#define FLY_MS 520                              /* a hint's piece flying to its place */
#define GLOW_MS 650                             /* a placed group's gold light */
#define WAVE_STEP 40
#define WAVE_LEN 520
#define SLOP 0.012f                             /* a finger moved this far (of the screen's short side): a drag */

static jig_game G;
static const uint8_t GOLD[3] = {255, 204, 96};

/* ---- the pictures: JPEG bytes, decoded, cropped to the grid ---------------------------------------------------- */
typedef struct { uint8_t *rgb; int w, h; } image;

static int pic_bytes(int pic, const unsigned char **jpg, int *size)
{
    const jig_picture *P = jig_pic(pic);
    if (P->tarot >= 0 && P->tarot < TAROT_ART_N) {
        *jpg = cards_tarot_art[P->tarot].jpg;
        *size = cards_tarot_art[P->tarot].size;
        return 1;
    }
    for (int i = 0; P->file && i < jigsaw_art_n; i++)
        if (!strcmp(jigsaw_art[i].name, P->file)) {
            *jpg = jigsaw_art[i].jpg;
            *size = jigsaw_art[i].size;
            return 1;
        }
    return 0;
}

static image pic_load(int pic)
{
    image im = {0};
    const unsigned char *jpg;
    int size, n;
    if (pic_bytes(pic, &jpg, &size)) im.rgb = stbi_load_from_memory(jpg, size, &im.w, &im.h, &n, 3);
    if (!im.rgb) {                              /* missing: a plain warm grey, never a crash */
        im.w = im.h = 4;
        im.rgb = malloc(48);
        if (im.rgb) memset(im.rgb, 150, 48);
    }
    return im;
}

static void pic_free(image *im)
{
    free(im->rgb);
    im->rgb = NULL;
}

/* the part of the picture a grid of cols x rows square cells shows: the middle, cut to that shape */
static void crop_for(const image *im, float aspect, float *x0, float *y0, float *cw, float *ch)
{
    if ((float)im->w / im->h > aspect) {
        *ch = (float)im->h;
        *cw = im->h * aspect;
    } else {
        *cw = (float)im->w;
        *ch = im->w / aspect;
    }
    *x0 = (im->w - *cw) / 2;
    *y0 = (im->h - *ch) / 2;
}

/* the crop (x0, y0, cw x ch of the picture) into W x H RGBA: averaged when it shrinks, bilinear when it grows */
static void sample(const image *im, float x0, float y0, float cw, float ch, uint8_t *o, int W, int H)
{
    float sx = cw / W, sy = ch / H;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float acc[3] = {0, 0, 0};
            uint8_t *q = o + 4 * ((size_t)y * W + x);
            if (sx > 1.5f || sy > 1.5f) {       /* the source pixels under it */
                int a0 = (int)(x0 + x * sx), a1 = (int)(x0 + (x + 1) * sx), b0 = (int)(y0 + y * sy),
                    b1 = (int)(y0 + (y + 1) * sy), n = 0;
                if (a1 <= a0) a1 = a0 + 1;
                if (b1 <= b0) b1 = b0 + 1;
                for (int b = b0; b < b1; b++)
                    for (int a = a0; a < a1; a++) {
                        int aa = a < 0 ? 0 : a >= im->w ? im->w - 1 : a, bb = b < 0 ? 0 : b >= im->h ? im->h - 1 : b;
                        const uint8_t *p = im->rgb + 3 * ((size_t)bb * im->w + aa);
                        for (int c = 0; c < 3; c++) acc[c] += p[c];
                        n++;
                    }
                for (int c = 0; c < 3; c++) q[c] = (uint8_t)(acc[c] / n + 0.5f);
            } else {
                float fx = x0 + (x + 0.5f) * sx - 0.5f, fy = y0 + (y + 0.5f) * sy - 0.5f;
                int ax = (int)floorf(fx), ay = (int)floorf(fy);
                float tx = fx - ax, ty = fy - ay;
                for (int k = 0; k < 4; k++) {
                    int px = ax + (k & 1), py = ay + (k >> 1);
                    px = px < 0 ? 0 : px >= im->w ? im->w - 1 : px;
                    py = py < 0 ? 0 : py >= im->h ? im->h - 1 : py;
                    float wgt = ((k & 1) ? tx : 1 - tx) * ((k >> 1) ? ty : 1 - ty);
                    const uint8_t *p = im->rgb + 3 * ((size_t)py * im->w + px);
                    for (int c = 0; c < 3; c++) acc[c] += wgt * p[c];
                }
                for (int c = 0; c < 3; c++) q[c] = (uint8_t)clampf(acc[c] + 0.5f, 0, 255);
            }
            q[3] = 255;
        }
}

/* ---- a piece: its outline filled (smoothed), the picture in it, its edge bevelled -------------------------------- */
/* the polygon (n points, pixels) as coverage 0..1 in cov (W x H, cleared first): four rows a pixel, exact across */
static void raster(const float *xy, int n, int W, int H, float *cov)
{
    memset(cov, 0, sizeof *cov * (size_t)W * H);
    float xs[64];
    for (int y = 0; y < H; y++)
        for (int s = 0; s < 4; s++) {
            float ys = y + (s + 0.5f) / 4;
            int m = 0;
            for (int i = 0; i < n && m < 64; i++) {
                float x0 = xy[2 * i], y0 = xy[2 * i + 1], x1 = xy[2 * ((i + 1) % n)], y1 = xy[2 * ((i + 1) % n) + 1];
                if ((y0 <= ys && ys < y1) || (y1 <= ys && ys < y0)) xs[m++] = x0 + (ys - y0) * (x1 - x0) / (y1 - y0);
            }
            for (int i = 1; i < m; i++)
                for (int j = i; j > 0 && xs[j - 1] > xs[j]; j--) {
                    float t = xs[j];
                    xs[j] = xs[j - 1];
                    xs[j - 1] = t;
                }
            float *row = cov + (size_t)y * W;
            for (int i = 0; i + 1 < m; i += 2) {
                float a = fmaxf(0, xs[i]), b = fminf((float)W, xs[i + 1]);
                for (int px = (int)a; px < W && px < b; px++)
                    row[px] += 0.25f * (fminf(px + 1.0f, b) - fmaxf((float)px, a));
            }
        }
}

/* a box blur of radius r (both ways) */
static void blur(const float *in, float *out, float *tmp, int W, int H, int r)
{
    for (int y = 0; y < H; y++) {
        float s = 0;
        const float *row = in + (size_t)y * W;
        for (int x = -r; x <= r; x++) s += row[x < 0 ? 0 : x >= W ? W - 1 : x] * (x >= 0 && x < W);
        for (int x = 0; x < W; x++) {
            tmp[(size_t)y * W + x] = s / (2 * r + 1);
            int add = x + r + 1, sub = x - r;
            if (add < W) s += row[add];
            if (sub >= 0) s -= row[sub];
        }
    }
    for (int x = 0; x < W; x++) {
        float s = 0;
        for (int y = 0; y <= r && y < H; y++) s += tmp[(size_t)y * W + x];
        for (int y = 0; y < H; y++) {
            out[(size_t)y * W + x] = s / (2 * r + 1);
            int add = y + r + 1, sub = y - r;
            if (add < H) s += tmp[(size_t)add * W + x];
            if (sub >= 0) s -= tmp[(size_t)sub * W + x];
        }
    }
}

static int piece_px(float c) { return (int)ceilf(c * (1 + 2 * JIG_MARGIN)) | 1; }

/* piece k of g, its cells c pixels, from the picture board (BW x BH RGBA, the whole grid): P x P RGBA (P:
 * piece_px), its cell's centre at the middle; mask (optional): its coverage as bytes */
static void piece_paint(const jig_game *g, int k, const uint8_t *board, int BW, int BH, float c, uint8_t *o,
                        uint8_t *mask)
{
    int P = piece_px(c), i = k % g->cols, j = k / g->cols;
    float off = (P - c) / 2, xy[2 * JIG_OUTLINE];
    int n = jig_outline(g, k, xy);
    for (int q = 0; q < n; q++) {
        xy[2 * q] = (xy[2 * q] - i) * c + off;
        xy[2 * q + 1] = (xy[2 * q + 1] - j) * c + off;
    }
    float *cov = malloc(sizeof *cov * (size_t)P * P * 3);
    if (!cov) {
        memset(o, 0, (size_t)P * P * 4);
        return;
    }
    float *bl = cov + (size_t)P * P, *tmp = bl + (size_t)P * P;
    raster(xy, n, P, P, cov);
    int r = (int)fmaxf(1, roundf(0.045f * c));
    blur(cov, bl, tmp, P, P, r);
    int ox = (int)lroundf(i * c - off), oy = (int)lroundf(j * c - off);
    for (int y = 0; y < P; y++)
        for (int x = 0; x < P; x++) {
            size_t at = (size_t)y * P + x;
            uint8_t *q = o + 4 * at;
            float a = clampf(cov[at], 0, 1);
            if (mask) mask[at] = (uint8_t)(255 * a);
            if (a <= 0) {
                memset(q, 0, 4);
                continue;
            }
            int bx = ox + x, by = oy + y;
            bx = bx < 0 ? 0 : bx >= BW ? BW - 1 : bx;
            by = by < 0 ? 0 : by >= BH ? BH - 1 : by;
            const uint8_t *p = board + 4 * ((size_t)by * BW + bx);
            int x0 = x - r < 0 ? 0 : x - r, y0 = y - r < 0 ? 0 : y - r, x1 = x + r >= P ? P - 1 : x + r,
                y1 = y + r >= P ? P - 1 : y + r;
            float shade = bl[(size_t)y0 * P + x0] - bl[(size_t)y1 * P + x1];   /* lit from the upper left */
            float b = bl[at], edge = clampf((0.85f - b) / 0.55f, 0, 1);
            float k2 = (1 + 0.7f * shade) * (1 - 0.2f * edge);
            for (int ch = 0; ch < 3; ch++) q[ch] = (uint8_t)clampf(p[ch] * k2 + 26 * shade, 0, 255);
            q[3] = (uint8_t)(255 * a);
        }
    free(cov);
}

/* ---- the screen -------------------------------------------------------------------------------------------------- */
static box room, tray, area;                    /* the whole room; the tray; where loose pieces may lie */
static float cell, bx, by;                      /* a cell's side; the picture's top left on the screen */
static float fr;                                /* the board's frame */
static int tray_down;                           /* 1: the tray along the bottom (scrolled across), 0: at the right */
static int lines;                               /* the tray's rows (or columns) of pieces */
static float tcell, pitch, scroll;              /* a tray piece's cell, the slots' pitch, the tray scrolled */
static SDL_Texture *t_piece[JIG_MAX], *t_board, *t_well, *t_tray, *t_bar;
static uint8_t *mask[JIG_MAX];
static int mask_p;                              /* the masks' side */
static struct { int pic, n; uint32_t seed; float cell; } made = {-1, 0, 0, 0};
static int ghost, edges_only, sel = -1;
static image img = {0};
static int img_pic = -1;
static Uint32 elapsed, last_act;                /* the time played (ms): the gaps between actions, each at most 20 s */

static struct { float x, y, s; Uint32 t0; int ms; } glide[JIG_MAX];   /* a piece moving to its state (from x, y) */
static Uint32 glow_t0[JIG_MAX];
static struct { int on; Uint32 t0; } win;

static struct {
    int on, kind, k;                            /* kind: D_TRAY (pending), D_SCROLL, D_PIECE */
    float x0, y0, x, y, gx, gy;                 /* down at; now at; the grab (the piece's centre less the finger) */
    int moved, from_tray;
} D;
enum { D_TRAY, D_SCROLL, D_PIECE };

static float S(void) { return fminf((float)ui.w, (float)ui.h); }
static void to_screen(float x, float y, float *sx, float *sy) { *sx = bx + x * cell; *sy = by + y * cell; }
static void to_table(float sx, float sy, float *x, float *y) { *x = (sx - bx) / cell; *y = (sy - by) / cell; }

static int tray_list(int16_t *out)              /* the tray's pieces shown (the border's alone: the filter) */
{
    int n = 0;
    for (int i = 0; i < G.s.ntray; i++)
        if (!edges_only || jig_is_edge(&G, G.s.tray[i])) out[n++] = G.s.tray[i];
    return n;
}

static float tray_len(void) { return tray_down ? tray.w : tray.h; }

static void slot_at(int q, float *cx, float *cy)   /* slot q's centre on the screen */
{
    int p = q / lines, l = q % lines;
    float along = (p + 0.5f) * pitch - scroll, across = (l + 0.5f) * ((tray_down ? tray.h : tray.w) / lines);
    if (tray_down) {
        *cx = tray.x + along;
        *cy = tray.y + across;
    } else {
        *cx = tray.x + across;
        *cy = tray.y + along;
    }
}

static float scroll_max(int nvis)
{
    float len = ((nvis + lines - 1) / lines) * pitch;
    return fmaxf(0, len - tray_len());
}

static void scroll_clamp(void)
{
    int16_t v[JIG_MAX];
    scroll = clampf(scroll, 0, scroll_max(tray_list(v)));
}

/* the slot nearest the finger in the tray (the place a piece goes back to): its index in the tray's whole list */
static int tray_insert_at(float x, float y)
{
    int16_t v[JIG_MAX];
    int n = tray_list(v), best = n;
    float bd = 1e9f;
    for (int q = 0; q < n; q++) {
        float cx, cy;
        slot_at(q, &cx, &cy);
        float d = hypotf(cx - x, cy - y);
        if (d < bd) bd = d, best = q;
    }
    if (best >= n) return G.s.ntray;
    for (int i = 0; i < G.s.ntray; i++)
        if (G.s.tray[i] == v[best]) return i;
    return G.s.ntray;
}

static int in_box(box b, float x, float y) { return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }

/* the group of k kept on the table: its centres inside the area (k's target moved) */
static void clamp_group(int k, float *tx, float *ty)
{
    float lo_x = 1e9f, hi_x = -1e9f, lo_y = 1e9f, hi_y = -1e9f;
    const jig_piece *p = &G.s.p[k];
    for (int q = 0; q < G.n; q++) {
        const jig_piece *o = &G.s.p[q];
        if (q != k && (p->where != JIG_TABLE || o->group != p->group || o->where != JIG_TABLE)) continue;
        float dx = q == k ? 0 : o->x - p->x, dy = q == k ? 0 : o->y - p->y;
        lo_x = fminf(lo_x, dx), hi_x = fmaxf(hi_x, dx), lo_y = fminf(lo_y, dy), hi_y = fmaxf(hi_y, dy);
    }
    float ax0 = (area.x - bx) / cell + 0.5f, ax1 = (area.x + area.w - bx) / cell - 0.5f,
          ay0 = (area.y - by) / cell + 0.5f, ay1 = (area.y + area.h - by) / cell - 0.5f;
    if (*tx + hi_x > ax1) *tx = ax1 - hi_x;
    if (*tx + lo_x < ax0) *tx = ax0 - lo_x;
    if (*ty + hi_y > ay1) *ty = ay1 - hi_y;
    if (*ty + lo_y < ay0) *ty = ay0 - lo_y;
}

static void clamp_all(void)                     /* after a new layout or an undo: every loose group on the table */
{
    for (int k = 0; k < G.n; k++) {
        jig_piece *p = &G.s.p[k];
        if (p->where != JIG_TABLE || p->group != k) continue;   /* each group once, by its first piece */
        float x = p->x, y = p->y;
        clamp_group(k, &x, &y);
        float dx = x - p->x, dy = y - p->y;
        if (dx == 0 && dy == 0) continue;
        for (int q = 0; q < G.n; q++)
            if (G.s.p[q].where == JIG_TABLE && G.s.p[q].group == k) G.s.p[q].x += dx, G.s.p[q].y += dy;
    }
}

static void free_pieces(void)
{
    for (int k = 0; k < JIG_MAX; k++) {
        ui_free(&t_piece[k]);
        free(mask[k]);
        mask[k] = NULL;
    }
    ui_free(&t_board);
}

/* the pieces' pictures at this size (made again for another puzzle or another cell) */
static void make_pieces(void)
{
    if (made.pic == G.pic && made.n == G.n && made.seed == G.seed && made.cell == cell && t_board) return;
    free_pieces();
    made.pic = G.pic;
    made.n = G.n;
    made.seed = G.seed;
    made.cell = cell;
    if (img_pic != G.pic) {
        pic_free(&img);
        img = pic_load(G.pic);
        img_pic = G.pic;
    }
    int BW = (int)(G.cols * cell), BH = (int)(G.rows * cell), P = piece_px(cell);
    uint8_t *board = malloc((size_t)BW * BH * 4);
    if (!board) return;
    float x0, y0, cw, ch;
    crop_for(&img, (float)G.cols / G.rows, &x0, &y0, &cw, &ch);
    sample(&img, x0, y0, cw, ch, board, BW, BH);
    t_board = ui_tex(BW, BH, board);
    mask_p = P;
    uint8_t *o = malloc((size_t)P * P * 4);
    for (int k = 0; o && k < G.n; k++) {
        mask[k] = malloc((size_t)P * P);
        piece_paint(&G, k, board, BW, BH, cell, o, mask[k]);
        t_piece[k] = ui_tex(P, P, o);
    }
    free(o);
    free(board);
}

/* the board: a walnut frame, its floor darker wood, sunk: a soft shadow falling in from the frame's top and left, a
 * lit lip along its bottom and right */
static void make_well(void)
{
    int w = (int)(G.cols * cell), h = (int)(G.rows * cell), f = (int)fr;
    ui_free(&t_well);
    uint8_t *b = ui_buf(w + 2 * f, h + 2 * f);
    art_plank(w, h, f, b);
    float s = fmaxf(4, 0.9f * fr);
    for (int y = f; y < f + h; y++)
        for (int x = f; x < f + w; x++) {
            uint8_t *p = b + 4 * ((size_t)y * (w + 2 * f) + x);
            float t = fmaxf(0, 1 - (y - f) / s), l = fmaxf(0, 1 - (x - f) / (0.7f * s));
            float k = 0.5f * (1 - 0.32f * t * t - 0.22f * l * l);
            if (f + h - y <= 2 || f + w - x <= 2) k += 0.05f;
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] * k);
        }
    t_well = ui_tex(w + 2 * f, h + 2 * f, b);
    int tw = (int)tray.w, th = (int)tray.h, tf = (int)fmaxf(2, 0.4f * fr);
    ui_free(&t_tray);
    if (tw > 2 * tf && th > 2 * tf) {
        b = ui_buf(tw, th);
        art_plank(tw - 2 * tf, th - 2 * tf, tf, b);
        for (int i = 0; i < tw * th; i++)
            for (int c = 0; c < 3; c++) b[4 * i + c] = (uint8_t)(b[4 * i + c] * 0.62f);
        t_tray = ui_tex(tw, th, b);
    }
    int d = (int)fmaxf(6, 0.6f * ui.unit);
    ui_free(&t_bar);
    b = ui_buf(d, d);
    art_icon(ICON_PUZZLE, d, b);
    for (int i = 0; i < d * d; i++) b[4 * i] = PUZ_CREAM[0], b[4 * i + 1] = PUZ_CREAM[1], b[4 * i + 2] = PUZ_CREAM[2];
    t_bar = ui_tex(d, d, b);
}

static void layout(box b)
{
    room = b;
    if (!G.n) return;
    float s = S(), m = floorf(0.02f * s);
    fr = floorf(fmaxf(3, 0.016f * s));
    float best = 0;
    for (int down = 1; down >= 0; down--) {     /* the tray below or beside: the bigger cells */
        float tt = clampf(0.24f * (down ? b.h : b.w), 0.17f * s, 0.34f * s);
        float aw = b.w - 2 * m - 2 * fr - (down ? 0 : tt + m), ah = b.h - 2 * m - 2 * fr - (down ? tt + m : 0);
        float c = floorf(fminf(aw / G.cols, ah / G.rows));
        if (c > best * 1.02f) {
            best = c;
            tray_down = down;
        }
    }
    cell = fmaxf(4, best);
    float tt = clampf(0.24f * (tray_down ? b.h : b.w), 0.17f * s, 0.34f * s);
    float BW = G.cols * cell, BH = G.rows * cell;
    if (tray_down) {
        tray = (box){b.x + m, b.y + b.h - m - tt, b.w - 2 * m, tt};
        area = (box){b.x, b.y, b.w, tray.y - b.y};
    } else {
        tray = (box){b.x + b.w - m - tt, b.y + m, tt, b.h - 2 * m};
        area = (box){b.x, b.y, tray.x - b.x, b.h};
    }
    bx = floorf(area.x + (area.w - BW) / 2);
    by = floorf(area.y + (area.h - BH) / 2);
    ui_grid_fit((box){bx - fr, by - fr, BW + 2 * fr, BH + 2 * fr}, 1, 1, 0);   /* the board for the stars card */
    float across = tray_down ? tray.h : tray.w;
    tcell = fminf(cell, across / 1.62f);
    lines = (int)fmaxf(1, floorf(across / (1.62f * tcell)));
    tcell = fminf(cell, across / (1.62f * lines));
    pitch = 1.62f * tcell;
    make_pieces();
    make_well();
    clamp_all();
    scroll_clamp();
}

static void relayout(void)
{
    if (room.w > 0) layout(room);
}

/* ---- drawing ---------------------------------------------------------------------------------------------------- */
static void put_piece(int k, float sx, float sy, float sc, int rot, int lifted, Uint8 alpha)
{
    SDL_Texture *t = t_piece[k];
    if (!t) return;
    float P = mask_p * sc;
    if (lifted >= 0) {                          /* its shadow: the piece in black, faded, towards the lower right */
        float d = (lifted ? 0.12f : 0.045f) * cell * sc;
        SDL_SetTextureColorMod(t, 0, 0, 0);
        SDL_SetTextureAlphaMod(t, (Uint8)((lifted ? 60 : 80) * alpha / 255));
        ui_put_rot(t, sx + 0.6f * d, sy + d, P, P, 90.0 * rot);
        SDL_SetTextureAlphaMod(t, (Uint8)((lifted ? 35 : 40) * alpha / 255));
        ui_put_rot(t, sx + 1.1f * d, sy + 1.6f * d, P * 1.01f, P * 1.01f, 90.0 * rot);
        SDL_SetTextureColorMod(t, 255, 255, 255);
    }
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_rot(t, sx, sy, P, P, 90.0 * rot);
    SDL_SetTextureAlphaMod(t, 255);
}

static int dragging(int k)                      /* piece k goes with the finger (the dragged piece, its group) */
{
    if (!D.on || D.kind != D_PIECE || !D.moved || D.k < 0) return 0;
    const jig_piece *p = &G.s.p[k], *d = &G.s.p[D.k];
    return k == D.k || (d->where == JIG_TABLE && p->where == JIG_TABLE && p->group == d->group);
}

/* where piece k shows (its glide on its way), and how big (a piece flying out of the tray grows) */
static void shown_at(int k, Uint32 now, float *sx, float *sy, float *sc)
{
    const jig_piece *p = &G.s.p[k];
    float x, y;
    to_screen(p->x, p->y, &x, &y);
    *sc = 1;
    if (glide[k].ms && now - glide[k].t0 < (Uint32)glide[k].ms) {
        float f = ui_ease((float)(now - glide[k].t0) / glide[k].ms);
        x = glide[k].x + (x - glide[k].x) * f;
        y = glide[k].y + (y - glide[k].y) * f;
        *sc = glide[k].s + (1 - glide[k].s) * f;
        ui_animate();
    } else {
        glide[k].ms = 0;
    }
    if (dragging(k)) {                          /* with the finger: the dragged piece's centre at the grab */
        const jig_piece *d = &G.s.p[D.k];
        if (d->where == JIG_TRAY) {
            x = D.x + D.gx;
            y = D.y + D.gy;
        } else {
            float kx, ky;
            to_screen(d->x, d->y, &kx, &ky);
            x += D.x + D.gx - kx;
            y += D.y + D.gy - ky;
        }
        *sc = 1;
    }
    *sx = x;
    *sy = y;
}

static void paint_frame(int kind, int size, uint8_t *o);

static void draw_tray(void)
{
    if (t_tray) ui_put(t_tray, tray.x, tray.y, tray.w, tray.h);
    int16_t v[JIG_MAX];
    int n = tray_list(v);
    SDL_Rect clip = {(int)tray.x + 2, (int)tray.y + 2, (int)tray.w - 4, (int)tray.h - 4};
    SDL_RenderSetClipRect(ui.r, &clip);
    float sc = tcell / cell;
    for (int q = 0; q < n; q++) {
        int k = v[q];
        if (dragging(k) || glide[k].ms) continue;
        float cx, cy;
        slot_at(q, &cx, &cy);
        if (cx < tray.x - pitch || cy < tray.y - pitch || cx > tray.x + tray.w + pitch || cy > tray.y + tray.h + pitch)
            continue;
        if (k == sel) ui_mark_gold_a(cx, cy, 1.7f * tcell, 0.9f);
        put_piece(k, cx, cy, sc, G.s.p[k].rot, 0, 255);
    }
    SDL_RenderSetClipRect(ui.r, NULL);
    if (!n && !jig_solved(&G) && edges_only && G.s.ntray)   /* the filter hides them all: the frame's light */
        ui_mark_gold_a(tray.x + tray.w / 2, tray.y + tray.h / 2, 0.8f * fminf(tray.w, tray.h), 0.35f);
    float smax = scroll_max(n);                 /* how far along: a thin bar */
    if (smax > 0) {
        float len = tray_len(), vis = len / (len + smax), pos = scroll / smax, t = fmaxf(2, 0.006f * S());
        if (tray_down)
            ui_fill(tray.x + pos * (1 - vis) * len, tray.y + tray.h - 2 * t, vis * len, t, 246, 238, 220, 110);
        else
            ui_fill(tray.x + tray.w - 2 * t, tray.y + pos * (1 - vis) * len, t, vis * len, 246, 238, 220, 110);
    }
}

static void draw(void)
{
    if (!t_board || !t_well) layout(room);
    if (!t_board) return;
    Uint32 now = ui_ms();
    float BW = G.cols * cell, BH = G.rows * cell;
    ui_put(t_well, bx - fr, by - fr, BW + 2 * fr, BH + 2 * fr);
    int solved = jig_solved(&G);
    if (ghost && !solved) {
        SDL_SetTextureAlphaMod(t_board, 70);
        ui_put(t_board, bx, by, BW, BH);
        SDL_SetTextureAlphaMod(t_board, 255);
    }
    float fade = 0;                             /* the seamless picture coming in after the wave */
    if (win.on) {
        Uint32 len = (Uint32)((G.cols + G.rows) * WAVE_STEP + WAVE_LEN);
        fade = clampf((float)(now - win.t0) / len, 0, 1);
        if (now - win.t0 < len + 200) ui_animate();
    } else if (solved) {
        fade = 1;
    }
    for (int k = 0; k < G.n; k++) {             /* the pieces in their places */
        if (G.s.p[k].where != JIG_PLACED || glide[k].ms) continue;
        float sx, sy, sc;
        shown_at(k, now, &sx, &sy, &sc);
        if (glide[k].ms) continue;
        put_piece(k, sx, sy, 1, 0, -1, 255);
    }
    if (fade > 0) {
        SDL_SetTextureAlphaMod(t_board, (Uint8)(255 * fade));
        ui_put(t_board, bx, by, BW, BH);
        SDL_SetTextureAlphaMod(t_board, 255);
    }
    for (int k = 0; k < G.n; k++) {             /* the gold light of a group just placed */
        if (!glow_t0[k]) continue;
        float f = puz_pulse((float)(now - glow_t0[k]), 0, GLOW_MS);
        if (now - glow_t0[k] >= GLOW_MS) {
            glow_t0[k] = 0;
            continue;
        }
        float sx, sy;
        to_screen(G.s.p[k].x, G.s.p[k].y, &sx, &sy);
        ui_mark_gold_a(sx, sy, 1.5f * cell, 0.7f * f);
        ui_animate();
    }
    if (win.on)                                 /* the wave of gold, corner to corner */
        for (int k = 0; k < G.n; k++) {
            int i = k % G.cols, j = k / G.cols;
            float f = puz_pulse((float)(now - win.t0), (float)((i + j) * WAVE_STEP), WAVE_LEN);
            if (f > 0) ui_mark_gold_a(bx + (i + 0.5f) * cell, by + (j + 0.5f) * cell, 1.6f * cell, f);
        }
    draw_tray();
    for (int z = 0; z < G.s.nz; z++) {          /* the loose pieces, bottom to top, the dragged ones last */
        int k = G.s.z[z];
        if (dragging(k)) continue;
        float sx, sy, sc;
        shown_at(k, now, &sx, &sy, &sc);
        if (k == sel || (sel >= 0 && G.s.p[sel].where == JIG_TABLE && G.s.p[k].group == G.s.p[sel].group))
            ui_mark_gold_a(sx, sy, 1.6f * cell, 0.85f);
        put_piece(k, sx, sy, sc, G.s.p[k].rot, 0, 255);
    }
    for (int k = 0; k < G.n; k++) {             /* pieces on their way (a hint's, a snap's) */
        if (!glide[k].ms || dragging(k)) continue;
        float sx, sy, sc;
        shown_at(k, now, &sx, &sy, &sc);
        if (glide[k].ms) put_piece(k, sx, sy, sc, G.s.p[k].rot, 1, 255);
    }
    for (int k = 0; k < G.n; k++) {
        if (!dragging(k)) continue;
        float sx, sy, sc;
        shown_at(k, now, &sx, &sy, &sc);
        put_piece(k, sx, sy, 1, G.s.p[k].rot, 1, 255);
    }
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
    ui_tool(T_GHOST, ICON_EYE, solved ? UI_DIM : ghost ? UI_ON : 0);
    ui_tool_paint(T_EDGES, paint_frame, 0, 0.66f, UI_TINT | (solved ? UI_DIM : edges_only ? UI_ON : 0));
}

/* the frame tool: a square frame of four bars, two knobs inwards (white: tinted) */
static void paint_frame(int kind, int size, uint8_t *o)
{
    (void)kind;
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    float a = 0.14f, b = 0.86f, t = 0.15f;
    art_rect(f, ART_PART, a, a, b, a + t);
    art_rect(f, ART_PART, a, b - t, b, b);
    art_rect(f, ART_PART, a, a, a + t, b);
    art_rect(f, ART_PART, b - t, a, b, b);
    art_ellipse(f, ART_PART, 0.5f, a + t + 0.06f, 0.075f, 0.075f);   /* a knob on the top bar, inwards */
    art_ellipse(f, ART_PART, b - t - 0.06f, 0.5f, 0.075f, 0.075f);
    art_fig_paint(o, size, f, PUZ_WHITE, PUZ_WHITE, 0, 0);
}

/* ---- the finger -------------------------------------------------------------------------------------------------- */
static int tray_hit(float x, float y)           /* the tray piece under the finger, or -1 */
{
    if (!in_box(tray, x, y)) return -1;
    int16_t v[JIG_MAX];
    int n = tray_list(v), best = -1;
    float bd = 0.75f * pitch;
    for (int q = 0; q < n; q++) {
        float cx, cy;
        slot_at(q, &cx, &cy);
        float d = fmaxf(fabsf(cx - x), fabsf(cy - y));
        if (d < bd) bd = d, best = v[q];
    }
    return best;
}

static int table_hit(float x, float y)          /* the loose piece on top under the finger, or -1 */
{
    for (int z = G.s.nz - 1; z >= 0; z--) {
        int k = G.s.z[z];
        const jig_piece *p = &G.s.p[k];
        float sx, sy;
        to_screen(p->x, p->y, &sx, &sy);
        float dx = x - sx, dy = y - sy;
        for (int r = 0; r < (p->rot & 3); r++) {   /* back to the piece upright */
            float t = dx;
            dx = dy;
            dy = -t;
        }
        if (fabsf(dx) < 0.55f * cell && fabsf(dy) < 0.55f * cell) return k;
        int mx = (int)(mask_p / 2.0f + dx), my = (int)(mask_p / 2.0f + dy);
        if (mask[k] && mx >= 0 && my >= 0 && mx < mask_p && my < mask_p && mask[k][my * mask_p + mx] > 90) return k;
    }
    return -1;
}

static void act(void)                           /* the time: the gap since the last action (at most 20 s) */
{
    Uint32 now = ui_ms();
    if (!jig_solved(&G)) elapsed += now - last_act > 20000 ? 20000 : now - last_act;
    last_act = now;
}

/* the screen places of the pieces before a move (to glide from them after it) */
static float was_x[JIG_MAX], was_y[JIG_MAX], was_s[JIG_MAX];
static int8_t was_where[JIG_MAX];

static void remember(void)
{
    Uint32 now = ui_ms();
    for (int k = 0; k < G.n; k++) {
        shown_at(k, now, &was_x[k], &was_y[k], &was_s[k]);
        was_where[k] = G.s.p[k].where;
        if (G.s.p[k].where == JIG_TRAY) {       /* a tray piece: its slot */
            int16_t v[JIG_MAX];
            int n = tray_list(v);
            was_s[k] = tcell / cell;
            for (int q = 0; q < n; q++)
                if (v[q] == k) slot_at(q, &was_x[k], &was_y[k]);
        }
    }
}

static void glide_after(int ms, int r)          /* each piece moved: from where it showed to its new place */
{
    Uint32 now = ui_ms();
    for (int k = 0; k < G.n; k++) {
        const jig_piece *p = &G.s.p[k];
        if (p->where == JIG_TRAY) continue;
        float sx, sy;
        to_screen(p->x, p->y, &sx, &sy);
        if (fabsf(sx - was_x[k]) < 0.5f && fabsf(sy - was_y[k]) < 0.5f && was_where[k] == p->where) continue;
        glide[k].x = was_x[k];
        glide[k].y = was_y[k];
        glide[k].s = was_s[k];
        glide[k].t0 = now;
        glide[k].ms = ms;
        if (p->where == JIG_PLACED && was_where[k] != JIG_PLACED) glow_t0[k] = now + ms;
    }
    if (r & JIG_DONE) {
        win.on = 1;
        win.t0 = now + ms + 120;
        ghost = 0;
    }
    ui_animate();
}

static void sound_of(int r)
{
    ui_sound(r & JIG_PLACE ? SND_PLACE : r & JIG_SNAP ? SND_CAPTURE : SND_MOVE);
}

/* piece k (its group) dropped with its centre at (sx, sy) on the screen */
static void drop_at(int k, float sx, float sy, int ms)
{
    float x, y;
    to_table(sx, sy, &x, &y);
    if (G.s.p[k].where == JIG_TRAY) {
        float lo = (area.x - bx) / cell + 0.5f, hi = (area.x + area.w - bx) / cell - 0.5f;
        x = clampf(x, lo, hi);
        lo = (area.y - by) / cell + 0.5f;
        hi = (area.y + area.h - by) / cell - 0.5f;
        y = clampf(y, lo, hi);
    } else {
        clamp_group(k, &x, &y);
    }
    int r = jig_drop(&G, k, x, y);
    sound_of(r);
    glide_after(ms, r);
}

static void use_hint(void)
{
    remember();
    int k = jig_hint(&G);
    if (k < 0) {
        ui_sound(SND_DENY);
        return;
    }
    sel = -1;
    ui_sound(SND_PLACE);
    glide_after(FLY_MS, jig_solved(&G) ? JIG_DONE : 0);
    scroll_clamp();
}

static void press(float x, float y)
{
    memset(&D, 0, sizeof D);
    int t = ui_tool_hit(x, y);
    if (t >= 0) {
        if (jig_solved(&G)) { ui_sound(SND_DENY); return; }
        act();
        if (t == T_HINT) use_hint();
        else if (t == T_GHOST) ghost = !ghost, ui_sound(SND_TAP);
        else {
            edges_only = !edges_only;
            scroll = 0;
            ui_sound(SND_TAP);
        }
        ui_animate();
        return;
    }
    if (jig_solved(&G) || win.on) return;
    act();
    D.on = 1;
    D.x0 = D.x = x;
    D.y0 = D.y = y;
    if (in_box(tray, x, y)) {
        D.kind = D_TRAY;
        D.k = tray_hit(x, y);
        D.from_tray = 1;
        return;
    }
    D.k = table_hit(x, y);
    D.kind = D_PIECE;
    if (D.k >= 0) {
        float sx, sy;
        to_screen(G.s.p[D.k].x, G.s.p[D.k].y, &sx, &sy);
        D.gx = sx - x;
        D.gy = sy - y;
    }
}

static void motion(float x, float y)
{
    if (!D.on) return;
    float dx = x - D.x, dy = y - D.y, slop = SLOP * S();
    D.x = x;
    D.y = y;
    float tx = x - D.x0, ty = y - D.y0;
    if (D.kind == D_TRAY) {
        float along = tray_down ? tx : ty, across = tray_down ? ty : tx;
        if (fabsf(along) > slop && fabsf(along) > 1.2f * fabsf(across)) {
            D.kind = D_SCROLL;
        } else if (D.k >= 0 && fabsf(across) > slop) {   /* out of the tray: the piece under the finger, a bit above */
            D.kind = D_PIECE;
            D.moved = 1;
            D.gx = 0;
            D.gy = tray_down ? -0.45f * cell : 0;
            if (!tray_down) D.gx = -0.45f * cell;
            sel = -1;
        }
    }
    if (D.kind == D_SCROLL) {
        scroll -= tray_down ? dx : dy;
        scroll_clamp();
        D.moved = 1;
    } else if (D.kind == D_PIECE && D.k >= 0) {
        if (!D.moved && hypotf(tx, ty) > slop) D.moved = 1, sel = -1;
    }
    ui_animate();
}

static void tap(float x, float y)               /* a tap: pick a piece up, turn it, put the one picked up there */
{
    int k = in_box(tray, x, y) ? tray_hit(x, y) : table_hit(x, y);
    if (k >= 0) {
        if (k == sel && G.rotate) {
            remember();
            jig_rotate(&G, k);
            ui_sound(SND_MOVE);
            glide_after(GLIDE_MS, jig_solved(&G) ? JIG_DONE : 0);
        } else if (k == sel) {
            sel = -1;
        } else {
            sel = k;
            ui_sound(SND_TAP);
        }
        return;
    }
    if (sel < 0) return;
    int s = sel;
    sel = -1;
    remember();
    if (in_box(tray, x, y)) {                   /* back into the tray: a lone piece */
        if (G.s.p[s].where == JIG_TABLE && jig_to_tray(&G, s, tray_insert_at(x, y))) ui_sound(SND_MOVE);
        else if (G.s.p[s].where != JIG_TRAY) ui_sound(SND_DENY);
        scroll_clamp();
        return;
    }
    drop_at(s, x, y, 2 * GLIDE_MS);
}

static void release(float x, float y)
{
    if (!D.on) return;
    D.x = x;
    D.y = y;
    int k = D.k;
    if (D.kind == D_PIECE && D.moved && k >= 0) {
        float sx = x + D.gx, sy = y + D.gy;
        remember();
        D.on = 0;
        if (in_box(tray, x, y)) {               /* let go over the tray: back in it (a lone piece), else beside it */
            if (G.s.p[k].where == JIG_TRAY) {
                D.on = 0;
                ui_animate();
                return;
            }
            if (jig_to_tray(&G, k, tray_insert_at(x, y))) {
                ui_sound(SND_MOVE);
                scroll_clamp();
                return;
            }
        }
        drop_at(k, sx, sy, GLIDE_MS);
        scroll_clamp();
    } else if (!D.moved) {
        D.on = 0;
        tap(x, y);
    }
    D.on = 0;
    ui_animate();
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    memset(glide, 0, sizeof glide);
    memset(glow_t0, 0, sizeof glow_t0);
    memset(&D, 0, sizeof D);
    win.on = 0;
    sel = -1;
}

static void begin(void)
{
    calm();
    ghost = edges_only = 0;
    scroll = 0;
    elapsed = 0;
    last_act = ui_ms();
    relayout();
}

static void start(int level)
{
    jig_load(&G, level);
    begin();
}

static void start_random(int variant)
{
    jig_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int undo(void)
{
    if (win.on || !jig_undo(&G)) return 0;
    calm();
    clamp_all();
    scroll_clamp();
    return 1;
}

/* the game's own notation (jig_script), and: e the border's filter on, g the ghost on, s<k> piece k picked up */
static int script(const char *moves)
{
    calm();
    char buf[1024], rest[1024];
    snprintf(buf, sizeof buf, "%s", moves);
    rest[0] = 0;
    for (char *tok = strtok(buf, " "); tok; tok = strtok(NULL, " ")) {
        if (!strcmp(tok, "e")) edges_only = 1;
        else if (!strcmp(tok, "g")) ghost = 1;
        else if (tok[0] == 's' && tok[1]) sel = atoi(tok + 1);
        else {
            size_t l = strlen(rest);
            snprintf(rest + l, sizeof rest - l, "%s%s", l ? " " : "", tok);
        }
    }
    if (rest[0] && !jig_script(&G, rest)) return 0;
    if (sel >= G.n || (sel >= 0 && G.s.p[sel].where == JIG_PLACED)) sel = -1;
    clamp_all();
    scroll_clamp();
    if (jig_solved(&G)) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return 1;
}

static int status(void) { return jig_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void)
{
    Uint32 now = ui_ms();
    if (win.on && now - win.t0 < (Uint32)((G.cols + G.rows) * WAVE_STEP + WAVE_LEN)) return 1;
    for (int k = 0; k < G.n; k++)
        if (glide[k].ms && now - glide[k].t0 < (Uint32)glide[k].ms) return 1;
    return 0;
}
static int level_stars(void) { return ui_logic_stars(G.hints, jig_slow(&G, (int)(elapsed / 1000))); }

static void bar(int player, box b)              /* the pieces still to place */
{
    (void)player;
    if (t_bar && ui_ink_dark()) SDL_SetTextureColorMod(t_bar, 48, 43, 35);   /* cream: inked on a light bar */
    ui_bar_number(b, t_bar, G.n - G.s.placed);
    if (t_bar) SDL_SetTextureColorMod(t_bar, 255, 255, 255);
}

/* ---- the pictures for the shell ---------------------------------------------------------------------------------- */
/* the picture pic, its middle cut square, into a size x size region of o (w wide) at (x, y) */
static void paint_square(int pic, uint8_t *o, int w, int h, int x, int y, int size, float aspect)
{
    image im = pic_load(pic);
    float x0, y0, cw, ch;
    crop_for(&im, aspect, &x0, &y0, &cw, &ch);
    int sw = size, sh = (int)(size / aspect);
    if (sh > size) sh = size, sw = (int)(size * aspect);
    uint8_t *b = malloc((size_t)sw * sh * 4);
    if (b) {
        sample(&im, x0, y0, cw, ch, b, sw, sh);
        art_blend(o, w, h, b, sw, sh, x + (size - sw) / 2, y + (size - sh) / 2);
        free(b);
    }
    pic_free(&im);
}

/* a line from (x0, y0) to (x1, y1), wd wide, rgb at alpha a, over o (w x h) */
static void line(uint8_t *o, int w, int h, float x0, float y0, float x1, float y1, float wd, const uint8_t rgb[3],
                 float a)
{
    float len = hypotf(x1 - x0, y1 - y0);
    int n = (int)(len * 2) + 1;
    for (int s = 0; s <= n; s++) {
        float cx = x0 + (x1 - x0) * s / n, cy = y0 + (y1 - y0) * s / n, r = wd / 2;
        for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
            for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
                if (x < 0 || y < 0 || x >= w || y >= h) continue;
                float d = hypotf(x + 0.5f - cx, y + 0.5f - cy), k = a * clampf(r + 0.5f - d, 0, 1);
                uint8_t *p = o + 4 * ((size_t)y * w + x);
                if (k <= 0) continue;
                float cur = p[3] / 255.0f;
                for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] + (rgb[c] - p[c]) * k * 0.5f);
                p[3] = (uint8_t)(255 * fmaxf(cur, k));
            }
    }
}

/* the cut of g drawn over a picture: the grid's inner edges as dark lines, in the region (x, y, cw x ch) */
static void paint_cut(const jig_game *g, uint8_t *o, int w, int h, float x, float y, float c)
{
    static const uint8_t ink[3] = {30, 22, 16};
    float wd = fmaxf(1, c / 22), uv[2 * JIG_EDGE_PTS + 2];
    for (int pass = 0; pass < 2; pass++)
        for (int j = 0; j < (pass ? g->rows : g->rows - 1); j++)
            for (int i = 0; i < (pass ? g->cols - 1 : g->cols); i++) {
                const jig_knob *k = pass ? &g->vk[j * (g->cols - 1) + i] : &g->hk[j * g->cols + i];
                jig_edge(k, uv);
                uv[2 * JIG_EDGE_PTS] = 1;
                uv[2 * JIG_EDGE_PTS + 1] = 0;
                for (int q = 0; q < JIG_EDGE_PTS; q++) {
                    float u0 = uv[2 * q], v0 = k->sign * uv[2 * q + 1], u1 = uv[2 * q + 2],
                          v1 = k->sign * uv[2 * q + 3];
                    float ax, ay, bx2, by2;
                    if (pass) ax = i + 1 + v0, ay = j + u0, bx2 = i + 1 + v1, by2 = j + u1;
                    else ax = i + u0, ay = j + 1 + v0, bx2 = i + u1, by2 = j + 1 + v1;
                    line(o, w, h, x + ax * c, y + ay * c, x + bx2 * c, y + by2 * c, wd, ink, 0.85f);
                }
            }
}

/* a picture cut in about jig_counts[count] pieces, square, in o (size x size); a turn's arrow in the corner */
static void paint_cut_picture(int pic, int count, int turned, uint32_t seed, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    jig_game *g = malloc(sizeof *g);
    if (!g) return;
    jig_new(g, pic, count, 0, seed);
    float aspect = (float)g->cols / g->rows;
    int m = (int)(0.04f * size), in = size - 2 * m;
    int sw = in, sh = (int)(in / aspect);
    if (sh > in) sh = in, sw = (int)(in * aspect);
    int x = (size - sw) / 2, y = (size - sh) / 2, f = (int)fmaxf(2, 0.03f * size);
    uint8_t *frm = malloc((size_t)(sw + 2 * f) * (sh + 2 * f) * 4);
    if (frm) {                                  /* a thin walnut frame round it */
        art_plank(sw, sh, f, frm);
        art_blend(o, size, size, frm, sw + 2 * f, sh + 2 * f, x - f, y - f);
        free(frm);
    }
    paint_square(pic, o, size, size, x, y, sw > sh ? sw : sh, aspect);
    paint_cut(g, o, size, size, (float)x, (float)y, (float)sw / g->cols);
    if (turned) {                               /* a gold arrow going round, on a dark disc */
        int d = (int)(0.4f * size);
        uint8_t *b = calloc((size_t)d * d, 4);
        if (b) {
            art_disc(d, (const uint8_t[3]){28, 22, 18}, 0.85f, 0, b);
            art_fig *fg = art_fig_new();
            art_arc(fg, ART_PART, 0.5f, 0.5f, 0.24f, 0.33f, 200, 470);
            art_head(fg, ART_PART, 0.5f + 0.285f * cosf(470 * ART_PI / 180), 0.5f + 0.285f * sinf(470 * ART_PI / 180),
                     560, 0.2f);
            art_fig_paint(b, d, fg, GOLD, (const uint8_t[3]){150, 96, 30}, 0.02f, 0.3f);
            art_blend(o, size, size, b, d, d, size - d - m / 2, size - d - m / 2);
            free(b);
        }
    }
    free(g);
}

static int menu_pic(void)                       /* the Great Wave when it is there, else the World */
{
    for (int i = 0; i < jig_pictures(); i++)
        if (jig_pic(i)->file && !strcmp(jig_pic(i)->file, "hokusai-great-wave")) {
            const unsigned char *j;
            int s;
            if (pic_bytes(i, &j, &s)) return i;
        }
    return 21;
}

/* the menu's card: a picture in its frame, 3 x 3 pieces, the last one lifted out beside its place */
static void thumb_menu(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    jig_game *g = malloc(sizeof *g);
    if (!g) return;
    int pic = menu_pic();
    jig_new(g, pic, 0, 0, 11);
    g->cols = g->rows = 3;                      /* a 3 x 3 cut of the square middle */
    g->n = 9;
    float c = 0.22f * size, x0 = 0.07f * size, y0 = 0.19f * size;
    int BW = (int)(3 * c), f = (int)fmaxf(2, 0.035f * size);
    image im = pic_load(pic);
    float cx, cy, cw, ch;
    crop_for(&im, 1, &cx, &cy, &cw, &ch);
    uint8_t *board = malloc((size_t)BW * BW * 4), *frm = malloc((size_t)(BW + 2 * f) * (BW + 2 * f) * 4);
    int P = piece_px(c);
    uint8_t *pc = malloc((size_t)P * P * 4);
    if (board && frm && pc) {
        sample(&im, cx, cy, cw, ch, board, BW, BW);
        art_plank(BW, BW, f, frm);
        for (int y = f; y < f + BW; y++)
            for (int x = f; x < f + BW; x++)
                for (int k = 0; k < 3; k++) frm[4 * ((size_t)y * (BW + 2 * f) + x) + k] /= 2;
        art_blend(o, size, size, frm, BW + 2 * f, BW + 2 * f, (int)x0 - f, (int)y0 - f);
        for (int k = 0; k < 9; k++) {
            piece_paint(g, k, board, BW, BW, c, pc, NULL);
            int i = k % 3, j = k / 3;
            float px = x0 + (i + 0.5f) * c, py = y0 + (j + 0.5f) * c;
            if (k == 5) {                       /* lifted, its shadow under it */
                px += 0.13f * size;
                py -= 0.09f * size;
                uint8_t *sh = malloc((size_t)P * P * 4);
                if (sh) {
                    for (int q = 0; q < P * P; q++) sh[4 * q] = sh[4 * q + 1] = sh[4 * q + 2] = 0,
                                                    sh[4 * q + 3] = (uint8_t)(pc[4 * q + 3] * 0.35f);
                    art_blend_rot(o, size, size, sh, P, P, px + 0.04f * size, py + 0.06f * size, 12);
                    free(sh);
                }
                art_blend_rot(o, size, size, pc, P, P, px, py, 12);
            } else {
                art_blend(o, size, size, pc, P, P, (int)lroundf(px - P / 2.0f), (int)lroundf(py - P / 2.0f));
            }
        }
    }
    free(board);
    free(frm);
    free(pc);
    pic_free(&im);
    free(g);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    if (variant < 0) {
        thumb_menu(size, rgba);
        return;
    }
    paint_cut_picture(menu_pic(), variant / 2, variant % 2, 5, size, rgba);
}

static void axis_size(int i, int size, uint8_t *rgba) { paint_cut_picture(menu_pic(), i, 0, 5, size, rgba); }

/* upright, or turned: one piece of a picture, the second one turned with the arrow */
static void axis_turn(int i, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    jig_game *g = malloc(sizeof *g);
    if (!g) return;
    int pic = menu_pic();
    jig_new(g, pic, 0, 0, 3);
    g->cols = g->rows = 3;
    g->n = 9;
    float c = 0.5f * size;
    int BW = (int)(3 * c), P = piece_px(c);
    image im = pic_load(pic);
    float cx, cy, cw, ch;
    crop_for(&im, 1, &cx, &cy, &cw, &ch);
    uint8_t *board = malloc((size_t)BW * BW * 4), *pc = malloc((size_t)P * P * 4);
    if (board && pc) {
        sample(&im, cx, cy, cw, ch, board, BW, BW);
        piece_paint(g, 7, board, BW, BW, c, pc, NULL);   /* the bottom middle: the wave */
        art_blend_rot(o, size, size, pc, P, P, 0.5f * size, 0.5f * size, i ? 90 : 0);
    }
    free(board);
    free(pc);
    pic_free(&im);
    free(g);
    if (i) {
        int d = (int)(0.42f * size);
        uint8_t *b = calloc((size_t)d * d, 4);
        if (b) {
            art_disc(d, (const uint8_t[3]){28, 22, 18}, 0.85f, 0, b);
            art_fig *fg = art_fig_new();
            art_arc(fg, ART_PART, 0.5f, 0.5f, 0.24f, 0.33f, 200, 470);
            art_head(fg, ART_PART, 0.5f + 0.285f * cosf(470 * ART_PI / 180), 0.5f + 0.285f * sinf(470 * ART_PI / 180),
                     560, 0.2f);
            art_fig_paint(b, d, fg, GOLD, (const uint8_t[3]){150, 96, 30}, 0.02f, 0.3f);
            art_blend(o, size, size, b, d, d, size - d, size - d);
            free(b);
        }
    }
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    paint_cut_picture(jig_level_pic(level), jig_level_count(level), jig_level_rotate(level),
                      7919u * (uint32_t)level + 17u, size, rgba);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)   /* the chapter's first picture, cut */
{
    int l = jig_chapter_first(chapter);
    paint_cut_picture(jig_level_pic(l), 0, jig_level_rotate(l), 7919u * (uint32_t)l + 17u, size, rgba);
}

static void token(int player, int size, uint8_t *rgba)   /* an ivory disc, a jigsaw piece on it */
{
    (void)player;
    art_counter(COUNTER_DISC, 0, size, rgba);
    int q = (int)(0.56f * size);
    uint8_t *ic = calloc((size_t)q * q, 4);
    if (!ic) return;
    art_icon(ICON_PUZZLE, q, ic);
    for (int i = 0; i < q * q; i++) ic[4 * i] = 92, ic[4 * i + 1] = 60, ic[4 * i + 2] = 40;
    art_blend(rgba, size, size, ic, q, q, (size - q) / 2, (size - q) / 2);
    free(ic);
}

static int chapter_of(int level) { return jig_chapter_of(level); }

static const game jigsaw_game = {
    .id = "jigsaw", .order = 452, .variants = JIG_VARIANTS, .axes = {JIG_COUNTS, 2},
    .axis_pic = {axis_size, axis_turn},
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = TOOLS,
    .levels = JIG_LEVEL_COUNT, .chapter_of = chapter_of, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars,
};
REGISTER_GAME(jigsaw_game)
