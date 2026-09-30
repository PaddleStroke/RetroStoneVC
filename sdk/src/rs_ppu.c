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

/* line buffers: 8 bytes of slack before the visible area for fine scroll */
static uint8_t  g_line[RS_BG_COUNT][W + 16] __attribute__((aligned(8)));
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

/* Returns bit0 = has low-priority tiles, bit1 = has high-priority tiles.
 * *out points to the pixel for screen x = 0. */
static int decode_bg(const bg_state *b, int y, uint8_t *buf, const uint8_t **out)
{
    int sx = b->sx + (b->ldx ? b->ldx[y] : 0);
    int yy = (b->sy + (b->ldy ? b->ldy[y] : 0) + y) & (b->mh * 8 - 1);
    const uint16_t *mrow = b->map + (yy >> 3) * b->mw;
    int fy = yy & 7;
    int wmask = b->mw - 1;
    int tx = (sx >> 3) & wmask;
    int flags = 0;
    uint8_t *d = buf;
    for (int i = 0; i < W / 8 + 1; i++) {
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
    *out = buf + (sx & 7);
    return flags;
}

static int decode_affine(const bg_state *b, int y, uint8_t *buf, const uint8_t **out)
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
    *out = buf;
    return flags;
}

/* Returns [x0, x1) span touched by sprites on this line, count in *n. */
static int decode_sprites(int y, int *x0, int *x1)
{
    int n = 0, lo = W, hi = 0;
    for (int i = 0; i < RS_OAM_MAX; i++) {
        const rs_sprite *s = &g_oam[i];
        if (!s->used || (s->flags & RS_SPR_HIDE)) continue;
        int r = y - s->y;
        if ((unsigned)r >= s->h) continue;
        n++;
        if (s->flags & RS_SPR_VFLIP) r = s->h - 1 - r;
        int sw = s->w, tw = sw >> 3;
        int xs = s->x < 0 ? -s->x : 0;
        int xe = s->x + sw > W ? W - s->x : sw;
        if (xs >= xe) continue;
        if (s->x + xs < lo) lo = s->x + xs;
        if (s->x + xe > hi) hi = s->x + xe;
        uint8_t col = (uint8_t)(128 + (s->pal & 7) * 16);
        uint8_t pr = s->prio & 3;
        int tbase = g_obj_base + s->tile + (r >> 3) * tw;
        int fy8 = (r & 7) * 8;
        uint8_t *dl = g_sline + s->x, *dp = g_sprio + s->x;
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

static void paint_bg(uint16_t *o, const bg_state *b, int layer, const uint8_t *src, int hi)
{
    int want = hi ? 0x80 : 0;
    int math = (g_math_layers >> layer) & 1, fog = (g_fog_layers >> layer) & 1;
    if (b->win || math || fog) {
        paint_slow(o, src, NULL, want, 0, W, b->win, math, 0, fog);
        return;
    }
    const uint16_t *cg = g_cg565;
    if (hi) {
        for (int x = 0; x < W; x++) {
            uint8_t v = src[x];
            if (v & 0x80) o[x] = cg[v & 0x7f];
        }
    } else {
        for (int x = 0; x < W; x++) {
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
        if (g_raster) g_raster(y, g_raster_user);

        const uint8_t *src[RS_BG_COUNT] = {0};
        int has[RS_BG_COUNT] = {0};
        for (int l = 0; l < RS_BG_COUNT; l++) {
            const bg_state *b = &g_bg[l];
            if (!b->enabled) continue;
            has[l] = b->affine ? decode_affine(b, y, g_line[l], &src[l])
                               : decode_bg(b, y, g_line[l], &src[l]);
        }

        int sx0 = 0, sx1 = 0;
        memset(g_sline, 0, sizeof g_sline);
        int n = decode_sprites(y, &sx0, &sx1);
        if (n > max_line) max_line = n;
        if (n > 32) rs_warn(RS_WARN_SPRITES_PER_LINE, "%d sprites on line %d (guideline 32)", n, y);

        int anywin = g_obj_win | g_clip_win | g_fog_win;
        for (int l = 0; l < RS_BG_COUNT; l++) anywin |= g_bg[l].win;
        if (anywin) {
            int l0 = g_win_l[0], r0 = g_win_r[0], l1 = g_win_l[1], r1 = g_win_r[1];
            for (int x = 0; x < W; x++)
                g_win[x] = (uint8_t)((x >= l0 && x < r0) | ((x >= l1 && x < r1) << 1));
        }

        /* backdrop */
        uint16_t back = g_cg565[0];
        if ((g_math_layers & RS_MATH_BACK) && (g_math_mode & RS_MATH_FIXED))
            back = blend565(back, g_math_fixed, g_math_mode);
        {
            uint32_t bb = back | ((uint32_t)back << 16);
            uint32_t *o32 = (uint32_t *)o;
            for (int x = 0; x < W / 2; x++) o32[x] = bb;
        }
        if (g_fog_layers & RS_MATH_BACK) {
            uint16_t fb = fog565(back);
            for (int x = 0; x < W; x++)
                if (win_hides(g_fog_win, g_win[x])) o[x] = fb;
        }

        /* Mode 0 order, back to front */
#define BG(l, hi) if (has[l] & ((hi) ? 2 : 1)) paint_bg(o, &g_bg[l], l, src[l], hi)
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
            for (int x = 0; x < W; x++)
                if (hide[g_win[x]]) o[x] = 0;
        }
        if (g_bright < 15) brightness_line(o);
    }
    g_last_sprites = total;
    g_last_max_line = max_line;
}

int ppu_last_sprites(void) { return g_last_sprites; }
int ppu_last_max_line(void) { return g_last_max_line; }
