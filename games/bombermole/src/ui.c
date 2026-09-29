/*
 * Bomber Mole: objectives on screen, kept light: a short level banner, the per-depth counts on the HUD,
 * the pause screen with the map, the "molehill open" banner with the exit arrow, and the golden glow on
 * blocks that hide a grub.
 * All rights reserved, 8BCraft.
 *
 * Drawn on BG1 with the font and a few tiles made here:
 *   BG1 tiles 100..309: the pause map (3 depths x 10x7 tiles, 4x4 pixels per cell)
 *   BG1 tiles 320..335: the objective arrow (4 metatiles: up, right, down, left)
 * The glow is a metatile on the explosion layer (BG3, tiles 900..903 of the playfield base) drawn with
 * BG palette 0 entry 15, whose colour cycles (SNES-style colour cycling: the pulse costs nothing).
 * BG palette 0: 1 white, 2 dark, 3 box, 6-14 map and arrow colours, 15 the glow.
 */
#include "bm.h"
#include <stdio.h>
#include <string.h>

#define T_MAP 100
#define T_ARROW 320
#define T_GLOW 900              /* relative to the playfield layers' base */
enum { C_GOLD = 6, C_FLOOR, C_WALL, C_DIRT, C_WATER, C_HOLE, C_UP, C_EXIT, C_FOE, C_GLOW };

extern int hud_pal_below;
static int banner_t, level_t;
static char level_line1[48], level_line2[48];

static int center(const char *s) { return (40 - (int)strlen(s)) / 2; }

/* ---- setup ------------------------------------------------------------------------------ */
void ui_init_level(void)
{
    static const uint32_t cols[] = {0xffd040, 0xc8b890, 0x404050, 0x8a5a30, 0x4080e0, 0x080808, 0xff9020,
                                    0xe04040, 0xff3070};
    for (int i = 0; i < 9; i++) rs_pal_set(RS_PAL_BG(0) + C_GOLD + i, RS_HEX(cols[i]));
    /* arrow metatiles: a yellow arrow with a dark outline, 16x16, pointing down (then rotated) */
    uint8_t down[16][16];
    memset(down, 0, sizeof down);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            int in_shaft = y < 8 && x >= 5 && x <= 10;
            int in_head = y >= 7 && y <= 14 && x >= y - 7 && x <= 22 - y;
            if (in_shaft || in_head) down[y][x] = C_GOLD;
        }
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            if (!down[y][x])
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int yy = y + dy, xx = x + dx;
                        if (yy >= 0 && yy < 16 && xx >= 0 && xx < 16 && down[yy][xx] == C_GOLD) down[y][x] = 2;
                    }
    for (int dir = 0; dir < 4; dir++) {
        uint8_t px[16][16];
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                int sx = x, sy = y;
                switch (dir) {
                case DIR_UP: sy = 15 - y; break;
                case DIR_RIGHT: sx = y; sy = 15 - x; break;
                case DIR_LEFT: sx = y; sy = x; break;
                default: break;
                }
                px[y][x] = down[sy][sx];
            }
        for (int q = 0; q < 4; q++) {
            uint8_t t[64];
            for (int i = 0; i < 64; i++) t[i] = px[(q >> 1) * 8 + i / 8][(q & 1) * 8 + i % 8];
            rs_tiles_load8(T_ARROW + dir * 4 + q, t, 1);
        }
    }
    /* the glow: the block's corners and a few sparkle pixels, in the cycling colour; transparent
     * elsewhere, so the block's own art shows through (not a frame like a power-up's) */
    uint8_t g[16][16];
    memset(g, 0, sizeof g);
    for (int i = 1; i < 5; i++) {
        g[1][i] = g[i][1] = g[1][15 - i] = g[i][14] = C_GLOW;
        g[14][i] = g[15 - i][1] = g[14][15 - i] = g[15 - i][14] = C_GLOW;
    }
    g[6][7] = g[7][6] = g[7][8] = g[8][7] = C_GLOW;           /* a small twinkle in the middle */
    for (int q = 0; q < 4; q++) {
        uint8_t t[64];
        for (int i = 0; i < 64; i++) t[i] = g[(q >> 1) * 8 + i / 8][(q & 1) * 8 + i % 8];
        rs_tiles_load8(1024 + T_GLOW + q, t, 1);
    }
    banner_t = 0;
    hud_pal_below = 0;
}

/* the metatile drawn on BG3 over a block hiding a grub (map entries, relative to the layer base) */
const uint16_t *ui_glow_meta(void)
{
    static const uint16_t m[4] = {RS_MAP(T_GLOW, 0, 1, 0, 0), RS_MAP(T_GLOW + 1, 0, 1, 0, 0),
                                  RS_MAP(T_GLOW + 2, 0, 1, 0, 0), RS_MAP(T_GLOW + 3, 0, 1, 0, 0)};
    return m;
}

/* the pulse: about 1 s, between a dim and a warm gold (one CGRAM entry per frame) */
void ui_glow_pulse(uint32_t t)
{
    int k = (int)(t % 64);
    k = k < 32 ? k : 63 - k;                                   /* 0..31..0 */
    rs_pal_set(RS_PAL_BG(0) + C_GLOW, RS_RGB(14 + k * 17 / 31, 9 + k * 16 / 31, 1 + k * 6 / 31));
}

static void arrow_at(int tx, int ty, int dir)
{
    for (int q = 0; q < 4; q++)
        rs_bg_put(RS_BG1, tx + (q & 1), ty + (q >> 1), RS_MAP(T_ARROW + dir * 4 + q, 0, 1, 0, 0));
}

/* ---- grubs per depth -------------------------------------------------------------------- */
int ui_grubs(int d)
{
    int n = 0;
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) n += W.g[d][y][x].item == IT_GRUB;
    for (int i = 0; i < W.na; i++)
        if (W.a[i].alive && W.a[i].kind == AK_BOSS && W.a[i].depth == d) n++;   /* the boss holds one */
    return n;
}

/* ---- level start: a short banner, not blocking --------------------------------------------- */
void ui_level_banner(const level_def *L)
{
    snprintf(level_line1, sizeof level_line1, "%s", L->name);
    snprintf(level_line2, sizeof level_line2, "COLLECT %d GRUB%s TO OPEN THE MOLEHILL", W.grubs_left,
             W.grubs_left == 1 ? "" : "S");
    level_t = 150;                                             /* 2.5 s, the last 0.3 s slide away */
}

/* ---- pause screen: the objective, the map ---------------------------------------------------- */
static int cell_colour(int d, int x, int y)
{
    const cell *c = &W.g[d][y][x];
    switch (c->t) {
    case TR_STONE: case TR_WINDMILL: return C_WALL;
    case TR_DIRT: case TR_ROCK: case TR_ROOTS: case TR_FROZEN: case TR_LEAVES: case TR_CRATE: return C_DIRT;
    case TR_WATER: case TR_PUDDLE: return C_WATER;
    case TR_HOLE_DOWN: return C_HOLE;
    case TR_HOLE_UP: case TR_LADDER: return C_UP;
    case TR_EXIT: return C_EXIT;
    default: return C_FLOOR;
    }
}

static void build_map(void)
{
    static uint8_t px[NDEPTH][GH * 4][GW * 4];
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                int col = cell_colour(d, x, y);
                for (int j = 0; j < 4; j++)
                    for (int i = 0; i < 4; i++) px[d][y * 4 + j][x * 4 + i] = (uint8_t)col;
                int dot = 0;
                if (W.g[d][y][x].item == IT_GRUB) dot = C_GOLD;          /* hidden ones too */
                if (W.g[d][y][x].t == TR_EXIT && W.exit_open && (W.t / 10) % 2) dot = C_GOLD;
                if (dot)
                    for (int j = 1; j < 3; j++)
                        for (int i = 1; i < 3; i++) px[d][y * 4 + j][x * 4 + i] = (uint8_t)dot;
            }
    for (int i = 0; i < W.na; i++) {
        const actor *a = &W.a[i];
        if (!a->alive || a->kind == AK_DOG) continue;
        int col = a->kind == AK_MOLE ? ((W.t / 8) % 2 ? 2 : 1) : C_FOE;
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                if ((j == 0 || j == 3) + (k == 0 || k == 3) < 2) px[a->depth][a->cy * 4 + j][a->cx * 4 + k] = (uint8_t)col;
    }
    uint8_t t[64];
    for (int d = 0; d < NDEPTH; d++)
        for (int ty = 0; ty < 7; ty++)
            for (int tx = 0; tx < 10; tx++) {
                for (int i = 0; i < 64; i++) t[i] = px[d][ty * 8 + i / 8][tx * 8 + i % 8];
                rs_tiles_load8(T_MAP + d * 70 + ty * 10 + tx, t, 1);
            }
}

void ui_pause_screen(int cursor)
{
    static const char *const names[NDEPTH] = {"SURFACE", "BELOW", "DEEP"};
    text_box(1, 2, 38, 20);
    textf_at(center(W.def->name), 3, "%s", W.def->name);
    /* the level's hint, as a subtitle: word-wrapped on two lines of 36 characters */
    const char *p = W.def->hint;
    for (int row = 5; row < 7 && *p; row++) {
        char h[40];
        int n = (int)strlen(p);
        if (n > 36) {
            n = 36;
            while (n > 0 && p[n] != ' ') n--;
            if (n == 0) n = 36;
        }
        snprintf(h, sizeof h, "%.*s", n, p);
        textf_at(center(h), row, "%s", h);
        p += n;
        while (*p == ' ') p++;
    }
    const char *goal = W.exit_open ? "ALL GRUBS FOUND: GO TO THE MOLEHILL!" : "COLLECT THE GRUBS, THEN THE MOLEHILL";
    text_at(center(goal), 8, goal);
    build_map();
    for (int d = 0; d < NDEPTH; d++) {
        int x0 = 3 + d * 12;
        int n = ui_grubs(d);
        textf_at(x0, 10, "%-7s", names[d]);
        if (n) textf_at(x0 + 8, 10, "%d", n);
        else text_at(x0 + 8, 10, "OK");
        for (int ty = 0; ty < 7; ty++)
            for (int tx = 0; tx < 10; tx++)
                rs_bg_put(RS_BG1, x0 + tx, 11 + ty, RS_MAP(T_MAP + d * 70 + ty * 10 + tx, 0, 1, 0, 0));
    }
    text_at(center("GOLD GRUB  BLACK HOLE  ORANGE WAY UP"), 18, "GOLD GRUB  BLACK HOLE  ORANGE WAY UP");
    static const char *const items[] = {"RESUME", "RESTART", "QUIT"};
    for (int i = 0; i < 3; i++) textf_at(8 + i * 10, 20, "%c%s", i == cursor ? '>' : ' ', items[i]);
    rs_text_setup(RS_BG1, 0, 0, 1);
}

void ui_screen_done(void) { hud_pal_below = 0; }

/* ---- during play: level banner, "molehill open" banner, exit arrow ------------------------------ */
void ui_banner_exit_open(void) { banner_t = 90; }          /* 1.5 s */

void ui_play_overlays(int view_depth)
{
    text_clear_all();
    if (level_t) {
        level_t--;
        int y = 3 - (level_t < 18 ? (18 - level_t) / 6 : 0);  /* slides up out of the playfield */
        if (y >= 2) {
            int w = (int)strlen(level_line2) + 4;
            if ((int)strlen(level_line1) + 4 > w) w = (int)strlen(level_line1) + 4;
            text_box((40 - w) / 2, y, w, 4);
            text_at(center(level_line1), y + 1, level_line1);
            text_at(center(level_line2), y + 2, level_line2);
            rs_text_setup(RS_BG1, 0, 0, 1);
        }
    }
    if (banner_t) {
        banner_t--;
        text_box(12, 3, 16, 3);
        text_at(center("MOLEHILL OPEN!"), 4, "MOLEHILL OPEN!");
        rs_text_setup(RS_BG1, 0, 0, 1);
    }
    if (!W.exit_open) return;
    int ex = -1, ey = -1;
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (W.g[0][y][x].t == TR_EXIT) { ex = x; ey = y; }
    if (ex < 0) return;
    int bob = (W.t / 16) % 2;
    if (view_depth == 0) {
        if (ey > 0) arrow_at(ex * 2, 2 + (ey - 1) * 2 - bob, DIR_DOWN);
        else arrow_at(ex * 2, 2 + (ey + 1) * 2 + bob, DIR_UP);
    } else if (!banner_t) {
        arrow_at(ex * 2, 2 + bob, DIR_UP);
        const char *s = "EXIT: SURFACE";
        int tx = ex * 2 + 3;
        if (tx + (int)strlen(s) > 39) tx = ex * 2 - 1 - (int)strlen(s);
        text_box(tx - 1, 2, (int)strlen(s) + 2, 3);
        text_at(tx, 3, s);
        rs_text_setup(RS_BG1, 0, 0, 1);
    }
}
