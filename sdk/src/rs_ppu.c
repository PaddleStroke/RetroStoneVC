/*
 * RetroStone VC SDK: the scanline renderer ("PPU").
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Per scanline:
 *   1. the raster callback runs (HDMA-like register changes);
 *   2. each enabled BG layer is decoded into a byte line: 0 = transparent,
 *      else bit 7 = tile priority, bits 4-6 = palette, bits 0-3 = colour;
 *   3. sprites are rasterised into a sprite line (lowest OAM index wins);
 *   4. planes are painted back to front in the SNES Mode 0 order:
 *        BG4L BG3L S0 BG4H BG3H S1 BG2L BG1L S2 BG2H BG1H S3
 *      writing RGB565 directly into the output row;
 *   5. clip-to-black windows and master brightness.
 * With viewports (split screen), steps 1-5 run for each viewport crossing the line, on its columns only,
 * with its own scroll, layers and sprites; the columns no viewport covers show the divider colour.
 * No floating point anywhere in this file.
 */
#include "rs_internal.h"
#include <string.h>

#define W RS_SCREEN_W
#define H RS_SCREEN_H

typedef struct bg_state {
    uint16_t map[128 * 128];
    int mw, mh;                 /* tiles, power of two */
    int base;
    int enabled;
    int sx, sy;
    const int16_t *ldx, *ldy;
    int win;
    int affine;
    rs_affine aff;
} bg_state;

static uint8_t   g_tiles[RS_TILE_MAX][64] __attribute__((aligned(8)));
static int       g_tiles_used;          /* tiles written (VRAM accounting) */
static uint8_t   g_tile_written[RS_TILE_MAX];
static bg_state  g_bg[RS_BG_COUNT];
static rs_color  g_cgram[256];
static uint16_t  g_cg565[256];
static rs_sprite g_oam[RS_OAM_MAX];
static int       g_obj_base;
static int       g_obj_win;
static int       g_clip_win;
static int       g_win_l[2], g_win_r[2];
static int       g_math_mode, g_math_layers;
static uint16_t  g_math_fixed;
static int       g_bright = 15;
static rs_raster_fn g_raster;
static void     *g_raster_user;
static int       g_last_sprites, g_last_max_line;
static int       g_nviews, g_cur_view;

/* line buffers, indexed by the screen column: 16 bytes of slack before the visible area (fine scroll) */
static uint8_t  g_line[RS_BG_COUNT][W + 32] __attribute__((aligned(8)));
#define LINE(l) (g_line[l] + 16)
static uint8_t  g_sline[W];
static uint8_t  g_sprio[W];
static uint8_t  g_win[W];

RS_INLINE uint16_t to565(rs_color c)
{
    unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    return (uint16_t)((r << 11) | (((g << 1) | (g >> 4)) << 5) | b);
}

void ppu_reset(void)
{
    memset(g_tiles, 0, sizeof g_tiles);
    memset(g_bg, 0, sizeof g_bg);
    for (int i = 0; i < RS_BG_COUNT; i++) {
        g_bg[i].mw = 32;
        g_bg[i].mh = 32;
    }
    memset(g_cgram, 0, sizeof g_cgram);
    memset(g_cg565, 0, sizeof g_cg565);
    memset(g_oam, 0, sizeof g_oam);
    g_tiles_used = 0;
    memset(g_tile_written, 0, sizeof g_tile_written);
    g_obj_base = 0;
    g_obj_win = g_clip_win = 0;
    g_win_l[0] = g_win_l[1] = g_win_r[0] = g_win_r[1] = 0;
    g_math_mode = g_math_layers = 0;
    g_math_fixed = 0;
    g_bright = 15;
    g_raster = NULL;
    g_raster_user = NULL;
    g_nviews = 0;
    g_cur_view = -1;
}

/* ---- palettes ---------------------------------------------------------- */
void rs_pal_set(int i, rs_color c)
{
    if ((unsigned)i >= 256) return;
    g_cgram[i] = c & 0x7fff;
    g_cg565[i] = to565(c);
}
void rs_pal_load(int first, const rs_color *c, int n)
{
    for (int i = 0; i < n; i++) rs_pal_set(first + i, c[i]);
}
rs_color rs_pal_get(int i) { return (unsigned)i < 256 ? g_cgram[i] : 0; }
void rs_backdrop(rs_color c) { rs_pal_set(0, c); }

/* ---- tiles ------------------------------------------------------------- */
void rs_tiles_load(int first, const uint8_t *p, int count)
{
    for (int t = 0; t < count; t++, p += RS_TILE_BYTES) {
        int ti = first + t;
        if ((unsigned)ti >= RS_TILE_MAX) return;
        uint8_t *d = g_tiles[ti];
        for (int i = 0; i < 32; i++) {
            d[i * 2] = p[i] >> 4;
            d[i * 2 + 1] = p[i] & 15;
        }
        if (!g_tile_written[ti]) { g_tile_written[ti] = 1; g_tiles_used++; }
    }
}
void rs_tiles_load8(int first, const uint8_t *p, int count)
{
    for (int t = 0; t < count; t++, p += 64) {
        int ti = first + t;
        if ((unsigned)ti >= RS_TILE_MAX) return;
        for (int i = 0; i < 64; i++) g_tiles[ti][i] = p[i] & 15;
        if (!g_tile_written[ti]) { g_tile_written[ti] = 1; g_tiles_used++; }
    }
}
void rs_tile_pixel(int tile, int x, int y, int v)
{
    if ((unsigned)tile >= RS_TILE_MAX || (unsigned)x > 7 || (unsigned)y > 7) return;
    g_tiles[tile][y * 8 + x] = (uint8_t)(v & 15);
    if (!g_tile_written[tile]) { g_tile_written[tile] = 1; g_tiles_used++; }
}
int rs_tiles_used(void) { return g_tiles_used; }

/* ---- backgrounds ----------------------------------------------------------- */
static int map_dim(int v)
{
    if (v <= 32) return 32;
    if (v <= 64) return 64;
    if (v > 128) rs_warn(RS_WARN_MAP_SIZE, "map dimension %d clamped to 128", v);
    return 128;
}
void rs_bg_setup(int l, int mw, int mh, int base)
{
    if ((unsigned)l >= RS_BG_COUNT) return;
    bg_state *b = &g_bg[l];
    b->mw = map_dim(mw);
    b->mh = map_dim(mh);
    b->base = base;
    memset(b->map, 0, sizeof b->map);
}
void rs_bg_enable(int l, int on) { if ((unsigned)l < RS_BG_COUNT) g_bg[l].enabled = !!on; }
uint16_t *rs_bg_map(int l) { return (unsigned)l < RS_BG_COUNT ? g_bg[l].map : NULL; }
int rs_bg_map_w(int l) { return (unsigned)l < RS_BG_COUNT ? g_bg[l].mw : 0; }
int rs_bg_map_h(int l) { return (unsigned)l < RS_BG_COUNT ? g_bg[l].mh : 0; }
void rs_bg_put(int l, int x, int y, uint16_t e)
{
    if ((unsigned)l >= RS_BG_COUNT) return;
    bg_state *b = &g_bg[l];
    if ((unsigned)x >= (unsigned)b->mw || (unsigned)y >= (unsigned)b->mh) return;
    b->map[y * b->mw + x] = e;
}
uint16_t rs_bg_get(int l, int x, int y)
{
    if ((unsigned)l >= RS_BG_COUNT) return 0;
    bg_state *b = &g_bg[l];
    if ((unsigned)x >= (unsigned)b->mw || (unsigned)y >= (unsigned)b->mh) return 0;
    return b->map[y * b->mw + x];
}
void rs_bg_fill(int l, uint16_t e)
{
    if ((unsigned)l >= RS_BG_COUNT) return;
    for (int i = 0; i < 128 * 128; i++) g_bg[l].map[i] = e;
}
void rs_bg_meta(int l, int mx, int my, const uint16_t m[4])
{
    rs_bg_put(l, mx * 2, my * 2, m[0]);
    rs_bg_put(l, mx * 2 + 1, my * 2, m[1]);
    rs_bg_put(l, mx * 2, my * 2 + 1, m[2]);
    rs_bg_put(l, mx * 2 + 1, my * 2 + 1, m[3]);
}
void rs_bg_scroll(int l, int x, int y)
{
    if ((unsigned)l >= RS_BG_COUNT) return;
    g_bg[l].sx = x;
    g_bg[l].sy = y;
}
void rs_bg_line_scroll(int l, const int16_t *dx, const int16_t *dy)
{
    if ((unsigned)l >= RS_BG_COUNT) return;
    g_bg[l].ldx = dx;
    g_bg[l].ldy = dy;
}
void rs_bg_affine(int l, const rs_affine *m)
{
    if ((unsigned)l >= RS_BG_COUNT) return;
    if (!m) { g_bg[l].affine = 0; return; }
    for (int i = 0; i < RS_BG_COUNT; i++) g_bg[i].affine = 0; /* one at a time */
    g_bg[l].affine = 1;
    g_bg[l].aff = *m;
}
void rs_raster(rs_raster_fn fn, void *user) { g_raster = fn; g_raster_user = user; }

/* ---- windows / math ------------------------------------------------------ */
void rs_window(int w, int l, int r)
{
    if ((unsigned)w > 1) return;
    g_win_l[w] = l;
    g_win_r[w] = r;
}
void rs_bg_window(int l, int mask) { if ((unsigned)l < RS_BG_COUNT) g_bg[l].win = mask; }
void rs_obj_window(int mask) { g_obj_win = mask; }
void rs_clip_black(int mask) { g_clip_win = mask; }
static int g_fog_win, g_fog_layers;
static uint16_t g_fog565;
void rs_fog(int mask, rs_color colour, int layers)
{
    g_fog_win = mask;
    g_fog_layers = mask ? layers : 0;
    g_fog565 = to565(colour);
}
static uint16_t fog565(uint16_t c)
{
    unsigned r = ((c >> 11) + 3u * (g_fog565 >> 11)) / 4, g = (((c >> 5) & 63) + 3u * ((g_fog565 >> 5) & 63)) / 4,
             b = ((c & 31) + 3u * (g_fog565 & 31)) / 4;
    return (uint16_t)((r << 11) | (g << 5) | b);
}
void rs_math(int mode, int layers, rs_color fixed)
{
    g_math_mode = mode;
    g_math_layers = mode ? layers : 0;
    g_math_fixed = to565(fixed);
}
void rs_brightness(int b) { g_bright = b < 0 ? 0 : b > 15 ? 15 : b; }

/* ---- OAM --------------------------------------------------------------- */
void rs_obj_base(int base) { g_obj_base = base; }
void rs_oam_clear(void) { memset(g_oam, 0, sizeof g_oam); }
rs_sprite *rs_oam(int i) { return (unsigned)i < RS_OAM_MAX ? &g_oam[i] : NULL; }
int rs_spr(int x, int y, int tile, int w, int h, int pal, int prio, int flags)
{
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = &g_oam[i];
        if (s->used) continue;
        s->x = (int16_t)x; s->y = (int16_t)y; s->tile = (uint16_t)tile;
        s->w = (uint8_t)w; s->h = (uint8_t)h;
        s->pal = (uint8_t)(pal & 7); s->prio = (uint8_t)(prio & 3);
        s->flags = (uint8_t)flags; s->used = 1;
        if (i >= RS_OAM_GUIDE)
            rs_warn(RS_WARN_OAM_COUNT, "more than %d sprites in OAM", RS_OAM_GUIDE);
        return i;
    }
    return -1;
}

/* ---- viewports (split screen) ----------------------------------------------- */
static rs_viewport g_views[RS_VIEW_MAX];
static uint16_t  g_div565;

void rs_viewports(int n, const rs_viewport *v, rs_color divider)
{
    if (n < 0 || !v) n = 0;
    if (n > RS_VIEW_MAX) n = RS_VIEW_MAX;
    if (n) memcpy(g_views, v, (size_t)n * sizeof *v);
    g_nviews = n;
    g_div565 = to565(divider);
}
int rs_viewport_count(void) { return g_nviews; }
int rs_viewport_current(void) { return g_cur_view; }
int rs_oam_next(void)
{
    for (int i = 0; i < RS_OAM_MAX; i++)
        if (!g_oam[i].used) return i;
    return RS_OAM_MAX;
}

static void view_rect(rs_viewport *v, int x, int y, int w, int h)
{
    memset(v, 0, sizeof *v);
    v->x = (int16_t)x; v->y = (int16_t)y; v->w = (int16_t)w; v->h = (int16_t)h;
    v->layers = 0x0f;
    v->objs = 1;
}

int rs_viewport_layout(int players, int flags, rs_viewport *out)
{
    const int D = RS_VIEW_DIVIDER, hw = (W - D) / 2, hh = (H - D) / 2, x2 = W - hw, y2 = H - hh;
    switch (players) {
    case 2:
        if (flags & RS_LAYOUT_HSPLIT) { view_rect(&out[0], 0, 0, W, hh); view_rect(&out[1], 0, y2, W, hh); }
        else { view_rect(&out[0], 0, 0, hw, H); view_rect(&out[1], x2, 0, hw, H); }
        return 2;
    case 3:
        if (!(flags & RS_LAYOUT_MAP)) {
            view_rect(&out[0], 0, 0, W, hh);
            view_rect(&out[1], 0, y2, hw, hh);
            view_rect(&out[2], x2, y2, hw, hh);
            return 3;
        }
        /* 4 quadrants, the 4th for a map */
        /* fall through */
    case 4:
        view_rect(&out[0], 0, 0, hw, hh);
        view_rect(&out[1], x2, 0, hw, hh);
        view_rect(&out[2], 0, y2, hw, hh);
        view_rect(&out[3], x2, y2, hw, hh);
        return 4;
    default:
        view_rect(&out[0], 0, 0, W, H);
        return 1;
    }
}

/* ---- line decoding ------------------------------------------------------ */
/* SWAR: expand 4 pixels (values 0..15) with attribute byte `hi` where non-zero. */
RS_INLINE uint32_t attr4(uint32_t p, uint32_t hi4)
{
    uint32_t nz = ((p + 0x0f0f0f0fu) & 0x10101010u) >> 4; /* 1 per non-zero byte */
    return p | (hi4 & (nz * 0xffu));
}
RS_INLINE uint32_t bswap32(uint32_t v)
{
#if defined(__GNUC__)
    return __builtin_bswap32(v);
#else
    return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
#endif
}

/* Decodes the layer pixels (lx, ly), (lx + 1, ly)... for the screen columns [x0, x0 + n) into `line`, indexed by
 * the screen column (line[x0] = layer pixel lx). Returns bit0 = has low-priority tiles, bit1 = has high ones. */
static int decode_bg(const bg_state *b, int lx, int ly, int x0, int n, uint8_t *line)
{
    int yy = ly & (b->mh * 8 - 1);
    const uint16_t *mrow = b->map + (yy >> 3) * b->mw;
    int fy = yy & 7;
    int wmask = b->mw - 1;
    int tx = (lx >> 3) & wmask;
    int flags = 0;
    uint8_t *d = line + x0 - (lx & 7);
    for (int i = (n + (lx & 7) + 7) >> 3; i > 0; i--) {
        uint16_t e = mrow[tx];
        tx = (tx + 1) & wmask;
        const uint8_t *t = g_tiles[(b->base + (e & 1023)) & (RS_TILE_MAX - 1)] +
                           ((e & RS_MAP_VFLIP) ? 7 - fy : fy) * 8;
        uint32_t hi = (uint32_t)(((e >> 10) & 7) << 4) | ((e & RS_MAP_PRIO) ? 0x80u : 0u);
        uint32_t hi4 = hi * 0x01010101u;
        uint32_t w0, w1;
        memcpy(&w0, t, 4);
        memcpy(&w1, t + 4, 4);
        if (e & RS_MAP_HFLIP) {
            uint32_t tmp = bswap32(w1);
            w1 = bswap32(w0);
            w0 = tmp;
        }
        w0 = attr4(w0, hi4);
        w1 = attr4(w1, hi4);
        memcpy(d, &w0, 4);
        memcpy(d + 4, &w1, 4);
        d += 8;
        flags |= (e & RS_MAP_PRIO) ? 2 : 1;
    }
    return flags;
}

static int decode_affine(const bg_state *b, int y, uint8_t *buf)
{
    const rs_affine *m = &b->aff;
    int hofs = b->sx + (b->ldx ? b->ldx[y] : 0);
    int vofs = b->sy + (b->ldy ? b->ldy[y] : 0);
    int32_t dx = hofs - m->cx, dy = y + vofs - m->cy;
    int32_t X = m->a * dx + m->b * dy + (m->cx << 8);
    int32_t Y = m->c * dx + m->d * dy + (m->cy << 8);
    int pw = b->mw * 8, ph = b->mh * 8;
    int flags = 0;
    for (int x = 0; x < W; x++, X += m->a, Y += m->c) {
        int px = X >> 8, py = Y >> 8;
        if (m->wrap) {
            px &= pw - 1;
            py &= ph - 1;
        } else if ((unsigned)px >= (unsigned)pw || (unsigned)py >= (unsigned)ph) {
            buf[x] = 0;
            continue;
        }
        uint16_t e = b->map[(py >> 3) * b->mw + (px >> 3)];
        int fx = px & 7, fy = py & 7;
        if (e & RS_MAP_HFLIP) fx = 7 - fx;
        if (e & RS_MAP_VFLIP) fy = 7 - fy;
        uint8_t p = g_tiles[(b->base + (e & 1023)) & (RS_TILE_MAX - 1)][fy * 8 + fx];
        uint8_t hi = (uint8_t)((((e >> 10) & 7) << 4) | ((e & RS_MAP_PRIO) ? 0x80 : 0));
        buf[x] = p ? (uint8_t)(p | hi) : 0;
        flags |= (e & RS_MAP_PRIO) ? 2 : 1;
    }
    return flags;
}

/* Rasterises the sprites [first, end) of OAM, moved by (ox, oy) and clipped to the columns [cx0, cx1). Returns
 * the number of sprites on the line and the span [x0, x1) they touch. */
static int decode_sprites(int y, int first, int end, int ox, int oy, int cx0, int cx1, int *x0, int *x1)
{
    int n = 0, lo = cx1, hi = cx0;
    for (int i = first; i < end; i++) {
        const rs_sprite *s = &g_oam[i];
        if (!s->used || (s->flags & RS_SPR_HIDE)) continue;
        int sx = s->x + ox, r = y - (s->y + oy);
        if ((unsigned)r >= s->h) continue;
        n++;
        if (s->flags & RS_SPR_VFLIP) r = s->h - 1 - r;
        int sw = s->w, tw = sw >> 3;
        int xs = sx < cx0 ? cx0 - sx : 0;
        int xe = sx + sw > cx1 ? cx1 - sx : sw;
        if (xs >= xe) continue;
        if (sx + xs < lo) lo = sx + xs;
        if (sx + xe > hi) hi = sx + xe;
        uint8_t col = (uint8_t)(128 + (s->pal & 7) * 16);
        uint8_t pr = s->prio & 3;
        int tbase = g_obj_base + s->tile + (r >> 3) * tw;
        int fy8 = (r & 7) * 8;
        uint8_t *dl = g_sline + sx, *dp = g_sprio + sx;
        if (s->flags & RS_SPR_HFLIP) {
            for (int px = xs; px < xe; px++) {
                int c = sw - 1 - px;
                uint8_t p = g_tiles[(tbase + (c >> 3)) & (RS_TILE_MAX - 1)][fy8 + (c & 7)];
                if (p && !dl[px]) { dl[px] = col | p; dp[px] = pr; }
            }
        } else {
            for (int px = xs; px < xe; px++) {
                uint8_t p = g_tiles[(tbase + (px >> 3)) & (RS_TILE_MAX - 1)][fy8 + (px & 7)];
                if (p && !dl[px]) { dl[px] = col | p; dp[px] = pr; }
            }
        }
    }
    *x0 = lo;
    *x1 = hi;
    return n;
}

/* ---- composition ----------------------------------------------------------- */
static uint16_t blend565(uint16_t a, uint16_t b, int mode)
{
    int r = a >> 11, g = (a >> 5) & 63, bl = a & 31;
    int r2 = b >> 11, g2 = (b >> 5) & 63, b2 = b & 31;
    if (mode & RS_MATH_SUB) { r -= r2; g -= g2; bl -= b2; }
    else { r += r2; g += g2; bl += b2; }
    if (mode & RS_MATH_HALF) { r >>= 1; g >>= 1; bl >>= 1; }
    if (r < 0) r = 0; else if (r > 31) r = 31;
    if (g < 0) g = 0; else if (g > 63) g = 63;
    if (bl < 0) bl = 0; else if (bl > 31) bl = 31;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

RS_INLINE int win_hides(int mask, int w)
{
    return ((mask & RS_WIN1) && (w & 1)) || ((mask & RS_WIN1_OUT) && !(w & 1)) ||
           ((mask & RS_WIN2) && (w & 2)) || ((mask & RS_WIN2_OUT) && !(w & 2));
}

/* generic (windowed and/or colour math) plane painter */
static void paint_slow(uint16_t *o, const uint8_t *src, const uint8_t *prio, int want,
                       int x0, int x1, int win, int math, int objmath, int fog)
{
    uint8_t hide[4], fogged[4];
    for (int w = 0; w < 4; w++) { hide[w] = (uint8_t)win_hides(win, w); fogged[w] = (uint8_t)(fog && win_hides(g_fog_win, w)); }
    for (int x = x0; x < x1; x++) {
        uint8_t b = src[x];
        if (!b) continue;
        if (prio) { if (prio[x] != want) continue; }
        else if ((b & 0x80) != want) continue;
        if (win && hide[g_win[x]]) continue;
        uint16_t c = g_cg565[prio ? b : (b & 0x7f)];
        if (math && (!objmath || b >= 192))
            c = blend565(c, (g_math_mode & RS_MATH_FIXED) ? g_math_fixed : o[x], g_math_mode);
        if (fog && fogged[g_win[x]]) c = fog565(c);
        o[x] = c;
    }
}

static void paint_bg(uint16_t *o, const bg_state *b, int layer, const uint8_t *src, int hi, int x0, int x1)
{
    int want = hi ? 0x80 : 0;
    int math = (g_math_layers >> layer) & 1, fog = (g_fog_layers >> layer) & 1;
    if (b->win || math || fog) {
        paint_slow(o, src, NULL, want, x0, x1, b->win, math, 0, fog);
        return;
    }
    const uint16_t *cg = g_cg565;
    if (hi) {
        for (int x = x0; x < x1; x++) {
            uint8_t v = src[x];
            if (v & 0x80) o[x] = cg[v & 0x7f];
        }
    } else {
        for (int x = x0; x < x1; x++) {
            uint8_t v = src[x];
            if (v && !(v & 0x80)) o[x] = cg[v];
        }
    }
}

static void paint_obj(uint16_t *o, int p, int x0, int x1)
{
    int math = (g_math_layers & RS_MATH_OBJ) != 0;
    if (g_obj_win || math) {
        paint_slow(o, g_sline, g_sprio, p, x0, x1, g_obj_win, math, 1, 0);
        return;
    }
    for (int x = x0; x < x1; x++) {
        uint8_t v = g_sline[x];
        if (v && g_sprio[x] == p) o[x] = g_cg565[v];
    }
}

static void brightness_line(uint16_t *o)
{
    int b = g_bright;
    for (int x = 0; x < W; x++) {
        uint16_t c = o[x];
        unsigned r = ((c >> 11) * b) / 15, g = (((c >> 5) & 63) * b) / 15, bl = ((c & 31) * b) / 15;
        o[x] = (uint16_t)((r << 11) | (g << 5) | bl);
    }
}

/* One line of the screen columns [x0, x1): the whole screen (v = NULL: global scroll, every sprite) or one
 * viewport (its layers, scroll and sprites). Returns the sprites on the line. */
static int render_span(uint16_t *o, int y, int x0, int x1, const rs_viewport *v)
{
    const uint8_t *src[RS_BG_COUNT] = {0};
    int has[RS_BG_COUNT] = {0};
    for (int l = 0; l < RS_BG_COUNT; l++) {
        const bg_state *b = &g_bg[l];
        if (!b->enabled || (v && !((v->layers >> l) & 1))) continue;
        uint8_t *line = LINE(l);
        if (b->affine) {
            has[l] = decode_affine(b, y, line);
        } else {
            int dx = b->ldx ? b->ldx[y] : 0, dy = b->ldy ? b->ldy[y] : 0;
            int lx = v ? v->sx[l] + (x0 - v->x) + dx : b->sx + dx + x0;
            int ly = v ? v->sy[l] + (y - v->y) + dy : b->sy + y + dy;
            has[l] = decode_bg(b, lx, ly, x0, x1 - x0, line);
        }
        src[l] = line;
    }

    int sx0 = 0, sx1 = 0, n = 0;
    if (!v || v->objs) {
        memset(g_sline + x0, 0, (size_t)(x1 - x0));
        int first = v ? v->oam_first : 0, end = v ? v->oam_first + v->oam_count : RS_OAM_MAX;
        if (end > RS_OAM_MAX) end = RS_OAM_MAX;
        n = decode_sprites(y, first, end, v ? v->x : 0, v ? v->y : 0, x0, x1, &sx0, &sx1);
    }

    int anywin = g_obj_win | g_clip_win | g_fog_win;
    for (int l = 0; l < RS_BG_COUNT; l++) anywin |= g_bg[l].win;
    if (anywin) {
        int l0 = g_win_l[0], r0 = g_win_r[0], l1 = g_win_l[1], r1 = g_win_r[1];
        for (int x = x0; x < x1; x++)
            g_win[x] = (uint8_t)((x >= l0 && x < r0) | ((x >= l1 && x < r1) << 1));
    }

    /* backdrop */
    uint16_t back = g_cg565[0];
    if ((g_math_layers & RS_MATH_BACK) && (g_math_mode & RS_MATH_FIXED))
        back = blend565(back, g_math_fixed, g_math_mode);
    for (int x = x0; x < x1; x++) o[x] = back;
    if (g_fog_layers & RS_MATH_BACK) {
        uint16_t fb = fog565(back);
        for (int x = x0; x < x1; x++)
            if (win_hides(g_fog_win, g_win[x])) o[x] = fb;
    }

    /* Mode 0 order, back to front */
#define BG(l, hi) if (has[l] & ((hi) ? 2 : 1)) paint_bg(o, &g_bg[l], l, src[l], hi, x0, x1)
#define OBJ(p) if (n) paint_obj(o, p, sx0, sx1)
    BG(3, 0); BG(2, 0); OBJ(0);
    BG(3, 1); BG(2, 1); OBJ(1);
    BG(1, 0); BG(0, 0); OBJ(2);
    BG(1, 1); BG(0, 1); OBJ(3);
#undef BG
#undef OBJ

    if (g_clip_win) {
        uint8_t hide[4];
        for (int w = 0; w < 4; w++) hide[w] = (uint8_t)win_hides(g_clip_win, w);
        for (int x = x0; x < x1; x++)
            if (hide[g_win[x]]) o[x] = 0;
    }
    return n;
}

void ppu_render(uint16_t *fb)
{
    int max_line = 0, total = 0;
    for (int i = 0; i < RS_OAM_MAX; i++)
        if (g_oam[i].used && !(g_oam[i].flags & RS_SPR_HIDE)) {
            total++;
            if (g_oam[i].w > 64 || g_oam[i].h > 64 || (g_oam[i].w & 7) || (g_oam[i].h & 7))
                rs_warn(RS_WARN_SPRITE_SIZE, "sprite %d is %dx%d (guideline: 8..64, multiples of 8)",
                        i, g_oam[i].w, g_oam[i].h);
        }
    if (total > RS_OAM_GUIDE)
        rs_warn(RS_WARN_OAM_COUNT, "%d sprites visible (guideline %d)", total, RS_OAM_GUIDE);
    {
        long vram = (long)g_tiles_used * 32;
        for (int l = 0; l < RS_BG_COUNT; l++)
            if (g_bg[l].enabled) vram += (long)g_bg[l].mw * g_bg[l].mh * 2;
        if (vram > 65536)
            rs_warn(RS_WARN_VRAM, "VRAM use %ld bytes (tiles + maps; guideline 65536)", vram);
    }

    for (int y = 0; y < H; y++) {
        uint16_t *o = fb + y * W;
        int n = 0;
        if (!g_nviews) {
            g_cur_view = -1;
            if (g_raster) g_raster(y, g_raster_user);
            n = render_span(o, y, 0, W, NULL);
        } else {
            /* the divider shows where no viewport is; later viewports are drawn over earlier ones */
            uint32_t dd = g_div565 | ((uint32_t)g_div565 << 16), *o32 = (uint32_t *)o;
            for (int x = 0; x < W / 2; x++) o32[x] = dd;
            for (int i = 0; i < g_nviews; i++) {
                const rs_viewport *v = &g_views[i];
                if (y < v->y || y >= v->y + v->h) continue;
                int x0 = v->x < 0 ? 0 : v->x, x1 = v->x + v->w > W ? W : v->x + v->w;
                if (x0 >= x1) continue;
                g_cur_view = i;
                if (g_raster) g_raster(y, g_raster_user);
                n += render_span(o, y, x0, x1, v);
            }
            g_cur_view = -1;
        }
        if (n > max_line) max_line = n;
        if (n > 32) rs_warn(RS_WARN_SPRITES_PER_LINE, "%d sprites on line %d (guideline 32)", n, y);
        if (g_bright < 15) brightness_line(o);
    }
    g_last_sprites = total;
    g_last_max_line = max_line;
}

int ppu_last_sprites(void) { return g_last_sprites; }
int ppu_last_max_line(void) { return g_last_max_line; }

/* ---- save states: VRAM (packed 4bpp), maps, CGRAM, OAM, registers, viewports ------------------------------ */
void ppu_state_save(rs_wr *w)
{
    uint8_t packed[32], bits[RS_TILE_MAX / 8];
    for (int t = 0; t < RS_TILE_MAX; t++) {
        for (int i = 0; i < 32; i++) packed[i] = (uint8_t)(g_tiles[t][i * 2] << 4 | (g_tiles[t][i * 2 + 1] & 15));
        wr_bytes(w, packed, 32);
    }
    memset(bits, 0, sizeof bits);
    for (int t = 0; t < RS_TILE_MAX; t++)
        if (g_tile_written[t]) bits[t >> 3] |= (uint8_t)(1 << (t & 7));
    wr_bytes(w, bits, sizeof bits);
    wr_u32(w, (uint32_t)g_tiles_used);
    for (int i = 0; i < 256; i++) wr_u16(w, g_cgram[i]);
    for (int i = 0; i < RS_OAM_MAX; i++) {
        const rs_sprite *s = &g_oam[i];
        wr_u16(w, (uint16_t)s->x); wr_u16(w, (uint16_t)s->y); wr_u16(w, s->tile);
        wr_u8(w, s->w); wr_u8(w, s->h); wr_u8(w, s->pal); wr_u8(w, s->prio); wr_u8(w, s->flags); /* +used below */
    }
    for (int i = 0; i < RS_OAM_MAX; i += 8) {        /* the used flags as bits */
        uint8_t u = 0;
        for (int k = 0; k < 8; k++) u |= (uint8_t)((g_oam[i + k].used ? 1 : 0) << k);
        wr_u8(w, u);
    }
    wr_i32(w, g_obj_base); wr_i32(w, g_obj_win); wr_i32(w, g_clip_win);
    wr_i32(w, g_win_l[0]); wr_i32(w, g_win_l[1]); wr_i32(w, g_win_r[0]); wr_i32(w, g_win_r[1]);
    wr_i32(w, g_math_mode); wr_i32(w, g_math_layers); wr_u16(w, g_math_fixed);
    wr_i32(w, g_bright); wr_i32(w, g_fog_win); wr_i32(w, g_fog_layers); wr_u16(w, g_fog565);
    state_raster_put(w, g_raster);
    state_ptr_put(w, g_raster_user, 0, "the raster callback's user pointer");
    for (int l = 0; l < RS_BG_COUNT; l++) {
        const bg_state *b = &g_bg[l];
        wr_i32(w, b->mw); wr_i32(w, b->mh); wr_i32(w, b->base); wr_i32(w, b->enabled);
        wr_i32(w, b->sx); wr_i32(w, b->sy);
        state_ptr_put(w, b->ldx, H * sizeof(int16_t), "a line-scroll table (rs_bg_line_scroll)");
        state_ptr_put(w, b->ldy, H * sizeof(int16_t), "a line-scroll table (rs_bg_line_scroll)");
        wr_i32(w, b->win); wr_i32(w, b->affine);
        wr_i32(w, b->aff.a); wr_i32(w, b->aff.b); wr_i32(w, b->aff.c); wr_i32(w, b->aff.d);
        wr_i32(w, b->aff.cx); wr_i32(w, b->aff.cy); wr_i32(w, b->aff.wrap);
        for (int i = 0; i < 128 * 128; i++) wr_u16(w, b->map[i]);
    }
    wr_i32(w, g_nviews);
    wr_u16(w, g_div565);
    for (int i = 0; i < RS_VIEW_MAX; i++) {
        const rs_viewport *v = &g_views[i];
        wr_u16(w, (uint16_t)v->x); wr_u16(w, (uint16_t)v->y); wr_u16(w, (uint16_t)v->w); wr_u16(w, (uint16_t)v->h);
        for (int l = 0; l < 4; l++) { wr_u16(w, (uint16_t)v->sx[l]); wr_u16(w, (uint16_t)v->sy[l]); }
        wr_u8(w, v->layers); wr_u8(w, v->objs); wr_u16(w, v->oam_first); wr_u16(w, v->oam_count);
    }
}

int ppu_state_load(rs_rd *r, int apply)
{
    uint8_t packed[32], bits[RS_TILE_MAX / 8];
    for (int t = 0; t < RS_TILE_MAX; t++) {
        rd_bytes(r, packed, 32);
        if (apply)
            for (int i = 0; i < 32; i++) { g_tiles[t][i * 2] = packed[i] >> 4; g_tiles[t][i * 2 + 1] = packed[i] & 15; }
    }
    rd_bytes(r, bits, sizeof bits);
    int used = rd_i32(r);
    if (used < 0 || used > RS_TILE_MAX) return -1;
    if (apply) {
        for (int t = 0; t < RS_TILE_MAX; t++) g_tile_written[t] = (bits[t >> 3] >> (t & 7)) & 1;
        g_tiles_used = used;
    }
    for (int i = 0; i < 256; i++) {
        rs_color c = rd_u16(r);
        if (apply) rs_pal_set(i, c);                /* CGRAM and its RGB565 copy */
    }
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite s;
        s.x = (int16_t)rd_u16(r); s.y = (int16_t)rd_u16(r); s.tile = rd_u16(r);
        s.w = rd_u8(r); s.h = rd_u8(r); s.pal = rd_u8(r); s.prio = rd_u8(r); s.flags = rd_u8(r);
        s.used = g_oam[i].used;
        if (apply) g_oam[i] = s;
    }
    for (int i = 0; i < RS_OAM_MAX; i += 8) {
        uint8_t u = rd_u8(r);
        if (apply)
            for (int k = 0; k < 8; k++) g_oam[i + k].used = (u >> k) & 1;
    }
    int regs[11];
    uint16_t fixed, fog;
    for (int i = 0; i < 9; i++) regs[i] = rd_i32(r);
    fixed = rd_u16(r);
    int bright = rd_i32(r), fog_win = rd_i32(r), fog_layers = rd_i32(r);
    fog = rd_u16(r);
    rs_raster_fn fn;
    const void *user;
    if (state_raster_get(r, &fn) || state_ptr_get(r, &user, 0)) return -1;
    if (r->err || bright < 0 || bright > 15) return -1;
    if (apply) {
        g_obj_base = regs[0]; g_obj_win = regs[1]; g_clip_win = regs[2];
        g_win_l[0] = regs[3]; g_win_l[1] = regs[4]; g_win_r[0] = regs[5]; g_win_r[1] = regs[6];
        g_math_mode = regs[7]; g_math_layers = regs[8]; g_math_fixed = fixed;
        g_bright = bright; g_fog_win = fog_win; g_fog_layers = fog_layers; g_fog565 = fog;
        g_raster = fn;
        g_raster_user = (void *)(uintptr_t)user;
    }
    for (int l = 0; l < RS_BG_COUNT; l++) {
        bg_state *b = &g_bg[l];
        int v[6];
        for (int i = 0; i < 6; i++) v[i] = rd_i32(r);
        const void *ldx, *ldy;
        if (state_ptr_get(r, &ldx, H * sizeof(int16_t)) || state_ptr_get(r, &ldy, H * sizeof(int16_t))) return -1;
        if ((uintptr_t)ldx % sizeof(int16_t) || (uintptr_t)ldy % sizeof(int16_t)) return -1;
        int win = rd_i32(r), affine = rd_i32(r);
        rs_affine aff;
        aff.a = rd_i32(r); aff.b = rd_i32(r); aff.c = rd_i32(r); aff.d = rd_i32(r);
        aff.cx = rd_i32(r); aff.cy = rd_i32(r); aff.wrap = rd_i32(r);
        if (r->err || (v[0] != 32 && v[0] != 64 && v[0] != 128) || (v[1] != 32 && v[1] != 64 && v[1] != 128)) return -1;
        if (apply) {
            b->mw = v[0]; b->mh = v[1]; b->base = v[2]; b->enabled = v[3] ? 1 : 0; b->sx = v[4]; b->sy = v[5];
            b->ldx = ldx; b->ldy = ldy;
            b->win = win; b->affine = affine ? 1 : 0; b->aff = aff;
            for (int i = 0; i < 128 * 128; i++) b->map[i] = rd_u16(r);
        } else {
            rd_bytes(r, NULL, 128 * 128 * 2);
        }
    }
    int nviews = rd_i32(r);
    uint16_t div = rd_u16(r);
    if (r->err || nviews < 0 || nviews > RS_VIEW_MAX) return -1;
    for (int i = 0; i < RS_VIEW_MAX; i++) {
        rs_viewport v;
        v.x = (int16_t)rd_u16(r); v.y = (int16_t)rd_u16(r); v.w = (int16_t)rd_u16(r); v.h = (int16_t)rd_u16(r);
        for (int l = 0; l < 4; l++) { v.sx[l] = (int16_t)rd_u16(r); v.sy[l] = (int16_t)rd_u16(r); }
        v.layers = rd_u8(r); v.objs = rd_u8(r); v.oam_first = rd_u16(r); v.oam_count = rd_u16(r);
        if (apply) g_views[i] = v;
    }
    if (r->err) return -1;
    if (apply) {
        g_nviews = nviews;
        g_div565 = div;
        g_cur_view = -1;
    }
    return 0;
}
