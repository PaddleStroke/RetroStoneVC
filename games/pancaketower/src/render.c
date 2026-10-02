/*
 * Pancake Tower: the pancakes and toppings, drawn at run time (they can be cut to any width, so they are not
 * pre-drawn sheets): pixel functions in the house style (the palettes of tools/make_art.py: TOWER_PAL,
 * TOPPING_PAL), the rows of the tower on BG2 and the pancake sprites (the slider, the top of the tower, the
 * pieces that fall). Integer maths only.
 *
 * BG2 rows: the middle of a row is made of shared 8x8 tiles (a few variants, drawn once at start-up); its two
 * end tiles are drawn for that row (pixel-exact ends). The interior pattern depends on the absolute map column,
 * so a row's end tiles and the shared tiles always join seamlessly.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 */
#include "pt.h"
#include "assets.h"
#include "render.h"
#include <string.h>

/* BG2 tiles, relative to VR_BG2 */
enum {
    BT_BLANK = 0,
    BT_PANCAKE = 1,             /* 4 variants */
    BT_SYRUP = 5,               /* 4 variants */
    BT_BUTTER = 9,              /* the centre tile with a butter streak */
    BT_BUTTER_SYRUP = 10,
    BT_TOPPING = 11,            /* 5 kinds x 2 variants */
    BT_STATIC_END = 21,
    BT_DYN = 32                 /* per player: RING_ROWS rows x 4 end tiles (2 at each end: the ends' shading is 5 px wide) */
};

static uint32_t hash2(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}

static int variant(int n, int tc) { return (int)(hash2((uint32_t)n, (uint32_t)(tc & 1023)) & 3); }

/* Narrow views share identical end tiles. References keep an edge alive until every map row using it changes. */
#define END_SLOTS (MAX_PLAYERS * RING_ROWS * 4)
static uint8_t end_pixels[END_SLOTS][64];
static uint16_t end_refs[END_SLOTS];
static int16_t row_ends[MAX_PLAYERS][RING_ROWS][4];
static int narrow;

void render_reset(int players)
{
    narrow = players > 2;
    memset(end_refs, 0, sizeof end_refs);
    memset(row_ends, -1, sizeof row_ends);
}

static void release_ends(int p, int row)
{
    for (int k = 0; k < 4; k++) {
        int slot = row_ends[p][row][k];
        if (slot >= 0) end_refs[slot]--;
        row_ends[p][row][k] = -1;
    }
}

static int share_end(int p, int row, int k, const uint8_t *pixels)
{
    int free_slot = -1;
    for (int s = 0; s < END_SLOTS; s++) {
        if (!end_refs[s]) { if (free_slot < 0) free_slot = s; continue; }
        if (!memcmp(end_pixels[s], pixels, 64)) {
            end_refs[s]++;
            row_ends[p][row][k] = (int16_t)s;
            return BT_DYN + s;
        }
    }
    /* There are at most END_SLOTS references; this row released its old ones before requesting replacements. */
    if (free_slot < 0) return BT_BLANK;
    memcpy(end_pixels[free_slot], pixels, 64);
    end_refs[free_slot] = 1;
    row_ends[p][row][k] = (int16_t)free_slot;
    rs_tiles_load8(VR_BG2 + BT_DYN + free_slot, pixels, 1);
    return BT_DYN + free_slot;
}

void render_state(void)
{
    rs_state_var("render.end_pixels", end_pixels, sizeof end_pixels);
    rs_state_var("render.end_refs", end_refs, sizeof end_refs);
    rs_state_var("render.row_ends", row_ends, sizeof row_ends);
    rs_state_var("render.narrow", &narrow, sizeof narrow);
}

/* ---- the pancake ------------------------------------------------------------------------------------------ */
#define END_ZONE 5                  /* px at each end drawn per row (the rounded end, its shading, the syrup) */
static const uint8_t INSET[2][8] = {{2, 1, 0, 0, 0, 0, 1, 2}, {3, 1, 0, 0, 0, 0, 0, 2}};

/* the interior of a pancake (away from its ends): row y, column px = ax & 7 of a tile of variant v */
static int pancake_inside(int y, int px, int v, int flags, int sprite, int centre)
{
    static const uint8_t body[8] = {TC_HI, TC_LIGHT, TC_LIGHT, TC_MID, TC_MID, TC_MID, TC_CRUST, TC_OUT};
    int c = body[y];
    if (sprite && y == 0) return TC_OUT;
    if (sprite && y == 1) c = TC_HI;
    switch (v) {                                  /* browned spots and bubbles */
    case 1: if ((y == 4 && (px == 3 || px == 4)) || (y == 5 && px == 4)) c = TC_CRUST; break;
    case 2: if (y == 2 && px == 2) c = TC_MID; if (y == 5 && px == 6) c = TC_CRUST; break;
    case 3: if (y == 3 && px == 6) c = TC_LIGHT; if (y == 5 && (px == 1 || px == 2)) c = TC_CRUST; break;
    default: break;
    }
    if (flags & LF_SYRUP) {
        int top = sprite ? 1 : 0;
        if (y == top) c = TC_SL;
        else if (y == top + 1) c = TC_SM;
        else if ((v & 1) && px == 5 && y <= top + 3) c = y == top + 3 ? TC_SD : TC_SM;   /* a drip */
    }
    if (centre && (flags & LF_BUTTER)) {
        int top = sprite ? 1 : 0;
        if (y == top && px >= 1 && px <= 6) c = TC_BUTTER;
        if (y == top + 1 && px >= 2 && px <= 5) c = TC_BUTTER2;
    }
    return c;
}

/* a pancake [x0, x0 + w) at absolute map x ax, row y (0 = top). cut: bit 0 the left end is a straight cut, bit 1
 * the right end (a piece that was cut off shows its crumb). centre_tc: the tile column of the butter streak. */
int pancake_px(int ax, int y, int x0, int w, int n, int flags, int sprite, int cut, int centre_tc)
{
    int lx = ax - x0, rx = x0 + w - 1 - ax;
    if (lx < 0 || rx < 0 || y < 0 || y > 7) return 0;
    uint32_t hn = hash2((uint32_t)n, 77u);
    int il = (cut & 1) ? 0 : INSET[hn & 1][y], ir = (cut & 2) ? 0 : INSET[(hn >> 1) & 1][y];
    if (w < 6) il = ir = 0;
    if (lx < il || rx < ir) return 0;
    if (y == 7) return TC_OUT;
    if (lx == il) return (cut & 1) ? (y == 0 ? TC_OUT : TC_CRUMB) : TC_OUT;
    if (rx == ir) return (cut & 2) ? (y == 0 ? TC_OUT : TC_CRUMB) : TC_OUT;
    int c = pancake_inside(y, ax & 7, variant(n, ax >> 3), flags, sprite, (ax >> 3) == centre_tc);
    if (sprite && y == 0) return TC_OUT;
    /* the ends: lighter on the left (the light), darker on the right; syrup runs down them */
    if (lx - il <= 1 && !(cut & 1)) {
        if (flags & LF_SYRUP) c = y <= 5 ? TC_SM : c;
        else c = c == TC_MID ? TC_LIGHT : c == TC_LIGHT ? TC_HI : c;
    } else if (rx - ir <= 1 && !(cut & 2)) {
        if (flags & LF_SYRUP) c = y <= 5 ? TC_SD : c;
        else c = c == TC_MID || c == TC_LIGHT ? TC_CRUST : c;
    }
    return c;
}

/* ---- the toppings ------------------------------------------------------------------------------------------ */
static int ell(int px, int y, int cx2, int cy2, int rx2, int ry2)
{
    /* (px + 0.5 - cx)^2 / rx^2 + ... in half-pixel units: returns 0..256+ (256 = the edge) */
    int dx = 2 * px + 1 - cx2, dy = 2 * y + 1 - cy2;
    return (dx * dx * 256) / (rx2 * rx2) + (dy * dy * 256) / (ry2 * ry2);
}

static int topping_inside(int kind, int y, int px, int v)
{
    int d;
    switch (kind) {
    case LK_STRAWBERRY:
        d = ell(px, y, v ? 8 : 7, 8, 7, 8);
        if (d > 256) return 0;
        if (d > 170) return PC_OUT;
        if (ell(px, y, v ? 8 : 7, 7, 3, 4) <= 256) return PC_CORE;
        if ((px == 2 && y == 3) || (px == 5 && y == 5) || (px == 4 && y == 2)) return PC_SD;
        return (px + y < 6) ? PC_SL : y < 5 ? PC_SM : PC_SD;
    case LK_BLUEBERRY: {
        int cy = v ? 9 : 10;
        if ((d = ell(px, y, 4, cy, 5, 5)) <= 256) return d > 150 ? PC_OUT : (px + y < 4 + cy / 2 - 2) ? PC_BL : y < cy / 2 + 1 ? PC_BM : PC_BD;
        if ((d = ell(px, y, 12, 17 - cy, 5, 5)) <= 256) return d > 150 ? PC_OUT : px > 5 && y < (17 - cy) / 2 ? PC_BL : PC_BM;
        if ((d = ell(px, y, 12, 17 - cy + 8, 5, 5)) <= 256) return d > 150 ? PC_OUT : PC_BD;
        return y >= 6 ? PC_BD : 0;
    }
    case LK_BANANA:
        d = ell(px, y, 8, 8, 7, 7);
        if (d > 256) return y == 7 ? PC_BAN2 : 0;
        if (d > 170) return PC_OUT;
        if (d > 60 && d < 110 && ((px + y + v) & 1)) return PC_SEED;
        return y < 4 ? PC_BAN : PC_BAN2;
    case LK_CHOCOLATE: {
        if (y == 7) return PC_CD;
        if (y == 6) return PC_CL;
        int cx = v ? 5 : 2, half = (y - 1) / 2;
        if (y >= 1 && px >= cx - half && px <= cx + half) return y == 1 ? PC_OUT : px < cx ? PC_CL : PC_CD;
        return 0;
    }
    default: {                                    /* whipped cream: a wavy top */
        static const uint8_t wave[8] = {1, 0, 0, 1, 2, 2, 1, 1};
        int top = wave[(px + v * 4) & 7];
        if (y < top) return 0;
        if (y == top) return PC_CREAM2;
        if (y >= 6 || (y == 4 && ((px + v) & 3) == 1)) return PC_CREAM2;
        return PC_CREAM;
    }
    }
}

int topping_px(int kind, int ax, int y, int x0, int w)
{
    int lx = ax - x0, rx = x0 + w - 1 - ax;
    if (lx < 0 || rx < 0 || y < 0 || y > 7) return 0;
    int c = topping_inside(kind, y, ax & 7, (int)(hash2((uint32_t)kind, (uint32_t)(ax >> 3)) & 1));
    if (c && (lx == 0 || rx == 0)) c = PC_OUT;
    return c;
}

/* one layer's pixel (palette index; *pal = the BG/OBJ palette kind: 0 tower, 1 topping) */
static int layer_px(const layer *l, int x0, int ax, int y, int sprite, int cut, int *pal)
{
    if (l->kind == LK_PANCAKE) {
        *pal = 0;
        return pancake_px(ax, y, x0, l->w, l->n, l->flags, sprite, cut, (x0 + l->w / 2) >> 3);
    }
    *pal = 1;
    return topping_px(l->kind, ax, y, x0, l->w);
}

/* ---- BG2: the rows of the tower ------------------------------------------------------------------------------ */
void render_init(void)
{
    uint8_t t[64];
    memset(t, 0, sizeof t);
    rs_tiles_load8(VR_BG2 + BT_BLANK, t, 1);
    /* the shared middle tiles: a very wide layer, sampled at a tile column of each variant */
    for (int k = 0; k < 10; k++) {              /* 4 plain variants, 4 with syrup, the butter tile (+ syrup) */
        int v = k < 8 ? k & 3 : 0;
        int flags = k >= 4 && k < 8 ? LF_SYRUP : k >= 8 ? LF_BUTTER | (k == 9 ? LF_SYRUP : 0) : 0;
        uint8_t tt[64];
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                tt[y * 8 + x] = (uint8_t)pancake_inside(y, x, v, flags, 0, k >= 8);
        rs_tiles_load8(VR_BG2 + BT_PANCAKE + k, tt, 1);
    }
    for (int kind = 0; kind < TOPPINGS; kind++)
        for (int v = 0; v < 2; v++) {
            uint8_t tt[64];
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) tt[y * 8 + x] = (uint8_t)topping_inside(LK_STRAWBERRY + kind, y, x, v);
            rs_tiles_load8(VR_BG2 + BT_TOPPING + kind * 2 + v, tt, 1);
        }
}

static uint16_t middle_entry(const layer *l, int tc, int centre_tc)
{
    if (l->kind == LK_PANCAKE) {
        int t;
        if (tc == centre_tc && (l->flags & LF_BUTTER)) t = (l->flags & LF_SYRUP) ? BT_BUTTER_SYRUP : BT_BUTTER;
        else t = ((l->flags & LF_SYRUP) ? BT_SYRUP : BT_PANCAKE) + variant(l->n, tc);
        return RS_MAP(t, PAL_TOWER, 0, 0, 0);
    }
    int v = (int)(hash2((uint32_t)l->kind, (uint32_t)tc) & 1);
    return RS_MAP(BT_TOPPING + (l->kind - LK_STRAWBERRY) * 2 + v, PAL_TOPPING, 0, 0, 0);
}

void render_row(int player, int ring_row, const layer *l, int centre_px, int col0, int ncols)
{
    if (narrow) release_ends(player, ring_row);
    int x0 = centre_px + l->x, x1 = x0 + l->w - 1, tx0 = x0 >> 3, tx1 = x1 >> 3, centre_tc = (x0 + l->w / 2) >> 3;
    int lz = (x0 + END_ZONE - 1) >> 3, rz = (x1 - END_ZONE + 1) >> 3;       /* the end zones' last / first tile */
    int pal = l->kind == LK_PANCAKE ? PAL_TOWER : PAL_TOPPING;
    for (int c = col0; c < col0 + ncols; c++) {
        uint16_t e = RS_MAP(BT_BLANK, 0, 0, 0, 0);
        if (c >= tx0 && c <= tx1) {
            if (c <= lz || c >= rz) {
                int k = c <= lz ? c - tx0 : 2 + (tx1 - c);
                int slot = BT_DYN + (player * RING_ROWS + ring_row) * 4 + k;
                uint8_t t[64];
                int p;
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++) {
                        int v = layer_px(l, x0, c * 8 + x, y, 0, 0, &p);
                        t[y * 8 + x] = (uint8_t)v;
                    }
                if (narrow) slot = share_end(player, ring_row, k, t);
                else rs_tiles_load8(VR_BG2 + slot, t, 1);
                e = RS_MAP(slot, pal, 0, 0, 0);
            } else {
                e = middle_entry(l, c, centre_tc);
            }
        }
        rs_bg_put(RS_BG2, c & (rs_bg_map_w(RS_BG2) - 1), ring_row, e);
    }
}

void render_clear_row(int ring_row, int col0, int ncols)
{
    if (narrow) release_ends(col0 / 16, ring_row);
    for (int c = col0; c < col0 + ncols; c++) rs_bg_put(RS_BG2, c & (rs_bg_map_w(RS_BG2) - 1), ring_row, 0);
}

/* ---- sprites: a layer in a 128x8 box (16 tiles: two 64x8 sprites) ------------------------------------------------- */
static uint8_t sbuf[32 * 128];

static void upload_box(int tile, int w, int h)
{
    /* the box -> (w / 64) sprites of 64 x h, each row-major */
    for (int s = 0; s < w / 64; s++)
        for (int ty = 0; ty < h / 8; ty++)
            for (int tx = 0; tx < 8; tx++) {
                uint8_t t[64];
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++) t[y * 8 + x] = sbuf[(ty * 8 + y) * w + s * 64 + tx * 8 + x];
                rs_tiles_load8(VR_OBJ + tile + s * (h / 8) * 8 + ty * 8 + tx, t, 1);
            }
}

int render_layer_sprite(int tile, const layer *l, int box_x0, int x0, int squash, int box_w, int *pal)
{
    memset(sbuf, 0, 128 * 8);
    int p = 0;
    for (int y = 0; y < 8; y++) {
        int sy = y, ww = l->w, xx0 = x0;
        if (squash) {                             /* the landing: 1 px flatter, 1 px wider on each side */
            if (y == 0) continue;
            sy = y <= 3 ? y - 1 : y;
            ww += 2;
            xx0 -= 1;
        }
        layer ll = *l;
        ll.w = (int16_t)ww;
        for (int x = 0; x < box_w; x++) {
            int ax = box_x0 + x;
            int v = layer_px(&ll, xx0, ax, sy, 1, 0, &p);
            if (v) sbuf[y * box_w + x] = (uint8_t)v;
        }
    }
    upload_box(tile, box_w, 8);
    *pal = p;
    return 0;
}

/* ---- the pieces that fall: rotated into a 128x16 box (32 tiles: two 64x16 sprites) --------------------------------- */
/* sin of 0..90 degrees in 16 steps of 5.625 degrees, Q14 */
static const int16_t SIN16[17] = {0, 1606, 3196, 4756, 6270, 7723, 9102, 10394, 11585, 12665, 13623, 14449, 15137,
                                  15679, 16069, 16305, 16384};
static int isin64(int a) {                        /* a: 64 steps per turn */
    a &= 63;
    if (a < 16) return SIN16[a];
    if (a < 32) return SIN16[32 - a];
    if (a < 48) return -SIN16[a - 32];
    return -SIN16[64 - a];
}
static int icos64(int a) { return isin64(a + 16); }

int render_piece_fits(int w, int a)
{
    int s = isin64(a), c = icos64(a);
    if (s < 0) s = -s;
    if (c < 0) c = -c;
    int half_h = (w * s / 2 + 4 * c) >> 14, half_w = (w * c / 2 + 4 * s) >> 14;
    return half_h <= 7 && half_w <= 63;
}

int render_piece(int tile, const cut_piece *pc, int a, int box_w)
{
    int used = 0;                                 /* bit 0: the left 64 columns hold pixels, bit 1: the right ones */
    memset(sbuf, 0, 128 * 16);
    layer l = {0, (int16_t)pc->w, (uint8_t)pc->kind, 0, pc->n};
    /* the piece's inner end was cut: a piece that hung on the right was cut on its left */
    int cut = pc->kind == LK_PANCAKE && !pc->whole ? (pc->side > 0 ? 1 : 2) : 0;
    int s = isin64(a), c = icos64(a), p;
    for (int by = 0; by < 16; by++)
        for (int bx = 0; bx < box_w; bx++) {
            /* box centre (64, 8); rotate the box pixel back into the piece (Q14) */
            int dx = 2 * bx + 1 - box_w, dy = 2 * by + 1 - 16;            /* half pixels */
            int u = (dx * c + dy * s) >> 15, v = (-dx * s + dy * c) >> 15;   /* pixels */
            int lx = u + pc->w / 2, ly = v + 4;
            if (lx < 0 || lx >= pc->w || ly < 0 || ly > 7) continue;
            int col = layer_px(&l, 0, lx, ly, 1, cut, &p);
            if (col) { sbuf[by * box_w + bx] = (uint8_t)col; used |= bx < 64 ? 1 : 2; }
        }
    upload_box(tile, box_w, 16);
    return used;
}
