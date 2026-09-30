/*
 * RetroStone VC SDK: text through a bitmap-font tileset.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "rs_internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern const uint8_t rs_font_5x7[96][8];

static int g_layer, g_first, g_pal, g_prio, g_big_first = -1, g_loaded_big;

void text_reset(void)
{
    g_layer = RS_BG1;
    g_first = 0;
    g_pal = 0;
    g_prio = 1;
    g_big_first = -1;
    g_loaded_big = 0;
}

/* ink = 1, shadow = 2 (one pixel right and down) */
static void glyph_pixels(const uint8_t *g, int scale, uint8_t out[16][16])
{
    memset(out, 0, 16 * 16);
    int n = 8 * scale;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (g[y] & (0x80 >> x))
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++) {
                        int px = x * scale + sx, py = y * scale + sy;
                        out[py][px] = 1;
                    }
    /* shadow */
    for (int y = n - 1; y >= 0; y--)
        for (int x = n - 1; x >= 0; x--)
            if (out[y][x] == 1) {
                int sx = x + 1, sy = y + 1;
                if (sx < n && sy < n && out[sy][sx] == 0) out[sy][sx] = 2;
            }
}

int rs_text_load(int first, int big)
{
    uint8_t px[16][16], tile[64];
    for (int c = 0; c < 96; c++) {
        glyph_pixels(rs_font_5x7[c], 1, px);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) tile[y * 8 + x] = px[y][x];
        rs_tiles_load8(first + c, tile, 1);
    }
    if (!big) return 96;
    g_big_first = first + 96;
    g_loaded_big = 1;
    for (int c = 0; c < 96; c++) {
        glyph_pixels(rs_font_5x7[c], 2, px);
        for (int q = 0; q < 4; q++) {
            int ox = (q & 1) * 8, oy = (q >> 1) * 8;
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) tile[y * 8 + x] = px[oy + y][ox + x];
            rs_tiles_load8(g_big_first + c * 4 + q, tile, 1);
        }
    }
    return 96 + 384;
}

void rs_text_setup(int layer, int tile_first, int pal, int prio)
{
    g_layer = layer;
    g_first = tile_first;
    g_pal = pal;
    g_prio = prio;
}
void rs_text_pal(int pal) { g_pal = pal; }

void rs_text(int x, int y, const char *s)
{
    for (; *s; s++, x++) {
        int c = (unsigned char)*s;
        if (c < 32 || c > 127) c = '?';
        rs_bg_put(g_layer, x, y, RS_MAP(g_first + c - 32, g_pal, g_prio, 0, 0));
    }
}

void rs_textf(int x, int y, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    rs_text(x, y, buf);
}

void rs_text_big(int x, int y, const char *s)
{
    if (!g_loaded_big) { rs_text(x, y, s); return; }
    /* big glyph tiles are relative to the text tile_first like the small ones */
    int base = g_first + 96;
    for (; *s; s++, x += 2) {
        int c = (unsigned char)*s;
        if (c < 32 || c > 127) c = '?';
        int t = base + (c - 32) * 4;
        rs_bg_put(g_layer, x, y, RS_MAP(t, g_pal, g_prio, 0, 0));
        rs_bg_put(g_layer, x + 1, y, RS_MAP(t + 1, g_pal, g_prio, 0, 0));
        rs_bg_put(g_layer, x, y + 1, RS_MAP(t + 2, g_pal, g_prio, 0, 0));
        rs_bg_put(g_layer, x + 1, y + 1, RS_MAP(t + 3, g_pal, g_prio, 0, 0));
    }
}

void rs_text_clear(int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) rs_bg_put(g_layer, x + i, y + j, RS_MAP(g_first, g_pal, 0, 0, 0));
}

/* ---- save states ---------------------------------------------------------------------------------------------- */
void text_state_save(rs_wr *w)
{
    wr_i32(w, g_layer); wr_i32(w, g_first); wr_i32(w, g_pal); wr_i32(w, g_prio);
    wr_i32(w, g_big_first); wr_i32(w, g_loaded_big);
}

int text_state_load(rs_rd *r, int apply)
{
    int v[6];
    for (int i = 0; i < 6; i++) v[i] = rd_i32(r);
    if (r->err) return -1;
    if (apply) {
        g_layer = v[0]; g_first = v[1]; g_pal = v[2]; g_prio = v[3]; g_big_first = v[4]; g_loaded_big = v[5] ? 1 : 0;
    }
    return 0;
}
