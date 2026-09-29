/*
 * Bomber Mole: objectives on screen (intro card, pause screen with the map and the HUD legend,
 * "molehill open" banner and exit arrow, tutorial prompts).
 * All rights reserved, 8BCraft.
 *
 * Everything is drawn on BG1 with the font, the HUD metatiles and a few tiles made here:
 *   BG1 tiles 100..309: the pause map (3 depths x 10x7 tiles, 4x4 pixels per cell)
 *   BG1 tiles 320..335: the objective arrow (4 metatiles: up, right, down, left)
 * BG palette 0: 1 white, 2 dark, 3 box, 6-14 map and arrow colours.
 */
#include "bm.h"
#include <stdio.h>
#include <string.h>

#define T_MAP 100
#define T_ARROW 320
enum { C_GOLD = 6, C_FLOOR, C_WALL, C_DIRT, C_WATER, C_HOLE, C_UP, C_EXIT, C_FOE };

extern int hud_pal_below;
static char prompt_text[48];
static int prompt_t, banner_t;
static const int DEPTH_ICON[NDEPTH] = {HUD_DEPTH_SURFACE, HUD_DEPTH_UNDER1, HUD_DEPTH_UNDER2};

static int center(const char *s) { return (40 - (int)strlen(s)) / 2; }

/* ---- setup ------------------------------------------------------------------------------ */
void ui_init_level(void)
{
    static const uint32_t cols[] = {0xffd040, 0xc8b890, 0x404050, 0x8a5a30, 0x4080e0, 0x080808, 0xff9020,
                                    0xe04040, 0xff3070};
    for (int i = 0; i < 9; i++) rs_pal_set(RS_PAL_BG(0) + C_GOLD + i, RS_HEX(cols[i]));
    /* arrow metatiles: a yellow triangle with a dark outline, 16x16, pointing down (then rotated) */
    uint8_t down[16][16];
    memset(down, 0, sizeof down);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            int in_shaft = y < 8 && x >= 5 && x <= 10;
            int in_head = y >= 7 && y <= 14 && x >= y - 7 && x <= 22 - y;
            if (in_shaft || in_head) down[y][x] = C_GOLD;
        }
    for (int y = 0; y < 16; y++)         /* outline */
        for (int x = 0; x < 16; x++)
            if (!down[y][x])
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int yy = y + dy, xx = x + dx;
                        if (yy >= 0 && yy < 16 && xx >= 0 && xx < 16 && down[yy][xx] == C_GOLD) down[y][x] = 2;
                    }
    for (int dir = 0; dir < 4; dir++) {     /* DIR_UP, RIGHT, DOWN, LEFT */
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
    prompt_t = banner_t = 0;
    hud_pal_below = 0;
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

static void depth_counts(int x, int y)
{
    /* [icon] N HERE   [icon] N BELOW   [icon] N DEEP  (icons are 16x16: even tile coordinates) */
    static const char *const names[NDEPTH] = {"HERE", "BELOW", "DEEP"};
    for (int d = 0; d < NDEPTH; d++) {
        int tx = x + d * 11;
        rs_bg_meta(RS_BG1, tx / 2, y / 2, bm_hud_meta[DEPTH_ICON[d]]);
        int n = ui_grubs(d);
        if (n) textf_at(tx + 2, y, "%d", n);
        else textf_at(tx + 2, y, "OK");
        text_at(tx + 2, y + 1, names[d]);
    }
}

/* word-wrap s into lines of at most w characters; returns the number of lines */
static int wrap(const char *s, int w, char out[][40], int maxl)
{
    int n = 0;
    while (*s && n < maxl) {
        while (*s == ' ') s++;
        int len = (int)strlen(s), cut = len;
        if (len > w) {
            cut = w;
            while (cut > 0 && s[cut] != ' ') cut--;
            if (cut == 0) cut = w;
        }
        snprintf(out[n++], 40, "%.*s", cut, s);
        s += cut;
    }
    return n;
}

/* ---- intro card --------------------------------------------------------------------------- */
void ui_intro_card(const level_def *L, int arc)
{
    char lines[3][40];
    int nl = L->hint[0] ? wrap(L->hint, 30, lines, 3) : 0;
    hud_pal_below = 1;
    text_box(3, 6, 34, 20);
    textf_at(center(L->name), 7, "%s", L->name);
    textf_at(center("SPRING 1-1"), 8, "%s %d-%d", season_name(L->season), arc + 1, L->num);
    char goal[40];
    snprintf(goal, sizeof goal, "COLLECT %d GOLDEN GRUB%s:", W.grubs_left, W.grubs_left == 1 ? "" : "S");
    text_at(center(goal), 10, goal);
    depth_counts(6, 12);
    text_at(center("THEN ENTER THE MOLEHILL"), 15, "THEN ENTER THE MOLEHILL");
    for (int i = 0; i < nl; i++) textf_at(center(lines[i]), 17 + i, "%s", lines[i]);
    text_at(center("START: THE MAP AND THE HUD"), 22, "START: THE MAP AND THE HUD");
    if ((W.t / 20) % 2 == 0) text_at(center("PRESS A"), 24, "PRESS A");
    rs_text_setup(RS_BG1, 0, 0, 1);
}

/* ---- pause screen: objective, map, HUD legend --------------------------------------------- */
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
    actor *m = world_player(0);
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                int col = cell_colour(d, x, y);
                for (int j = 0; j < 4; j++)
                    for (int i = 0; i < 4; i++) px[d][y * 4 + j][x * 4 + i] = (uint8_t)col;
                int dot = 0;
                if (W.g[d][y][x].item == IT_GRUB) dot = C_GOLD;
                if (W.g[d][y][x].t == TR_EXIT && W.exit_open && (W.t / 10) % 2) dot = C_GOLD;
                if (dot)
                    for (int j = 1; j < 3; j++)
                        for (int i = 1; i < 3; i++) px[d][y * 4 + j][x * 4 + i] = (uint8_t)dot;
            }
    for (int i = 0; i < W.na; i++) {
        const actor *a = &W.a[i];
        if (!a->alive || a->kind == AK_DOG) continue;
        int col = a->kind == AK_MOLE ? 1 : C_FOE;
        if (a->kind == AK_MOLE && (W.t / 8) % 2) col = 2;
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                if ((j == 0 || j == 3) + (k == 0 || k == 3) < 2) px[a->depth][a->cy * 4 + j][a->cx * 4 + k] = (uint8_t)col;
    }
    (void)m;
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
    hud_pal_below = 1;
    text_box(1, 2, 38, 27);
    textf_at(center(W.def->name), 3, "%s", W.def->name);
    if (W.exit_open) {
        text_at(center("ALL GRUBS FOUND: GO TO THE MOLEHILL!"), 5, "ALL GRUBS FOUND: GO TO THE MOLEHILL!");
    } else {
        char s[40];
        snprintf(s, sizeof s, "COLLECT THE %d GOLDEN GRUB%s LEFT,", W.grubs_left, W.grubs_left == 1 ? "" : "S");
        text_at(center(s), 5, s);
        text_at(center("THEN ENTER THE MOLEHILL"), 6, "THEN ENTER THE MOLEHILL");
    }
    build_map();
    for (int d = 0; d < NDEPTH; d++) {
        int x0 = 3 + d * 12;
        int n = ui_grubs(d);
        textf_at(x0, 8, "%-7s %s", names[d], n ? "" : "OK");
        if (n) textf_at(x0 + 8, 8, "%d", n);
        for (int ty = 0; ty < 7; ty++)
            for (int tx = 0; tx < 10; tx++)
                rs_bg_put(RS_BG1, x0 + tx, 9 + ty, RS_MAP(T_MAP + d * 70 + ty * 10 + tx, 0, 1, 0, 0));
    }
    text_at(2, 16, "GOLD GRUB  BLACK HOLE  ORANGE WAY UP");
    /* the HUD, explained: [icon] text, two columns */
    static const struct { int icon; const char *text; } legend[] = {
        {HUD_HEART, "HITS LEFT"}, {HUD_BOMB, "BOMBS AT ONCE"},
        {HUD_FIRE, "BLAST RANGE"}, {HUD_SPEED, "SPEED"},
        {HUD_GRUB, "GRUBS LEFT"}, {HUD_DEPTH_UNDER1, "GRUBS ON A DEPTH"},
        {HUD_CURSOR, "YOU ARE HERE"}, {HUD_CHECK, "DEPTH CLEARED"},
        {HUD_DANGER, "DANGER THERE"},
    };
    for (unsigned i = 0; i < sizeof legend / sizeof legend[0]; i++) {
        int mx = (i % 2) ? 10 : 1, my = 9 + (int)(i / 2);
        rs_bg_meta(RS_BG1, mx, my, bm_hud_meta[legend[i].icon]);
        text_at(mx * 2 + 2, my * 2 + 1, legend[i].text);
    }
    static const char *const items[] = {"RESUME", "RESTART", "QUIT"};
    for (int i = 0; i < 3; i++) textf_at(8 + i * 10, 28, "%c%s", i == cursor ? '>' : ' ', items[i]);
    rs_text_setup(RS_BG1, 0, 0, 1);
}

void ui_screen_done(void) { hud_pal_below = 0; }

/* ---- during play: banner, tutorial prompt, exit arrow ------------------------------------------ */
void ui_banner_exit_open(void) { banner_t = 200; }

void ui_prompt(const char *text)
{
    snprintf(prompt_text, sizeof prompt_text, "%s", text);
    prompt_t = 210;
}

int ui_prompt_busy(void) { return prompt_t > 0; }

void ui_play_overlays(int view_depth)
{
    text_clear_all();
    if (banner_t) {
        banner_t--;
        if ((banner_t / 10) % 6) {
            text_box(9, 3, 22, 3);
            text_at(center("THE MOLEHILL IS OPEN!"), 4, "THE MOLEHILL IS OPEN!");
            rs_text_setup(RS_BG1, 0, 0, 1);
        }
    }
    if (prompt_t) {
        prompt_t--;
        int w = (int)strlen(prompt_text) + 4;
        text_box((40 - w) / 2, 25, w, 3);
        text_at(center(prompt_text), 26, prompt_text);
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
        /* on the surface: an arrow next to the molehill, pointing at it */
        if (ey > 0) arrow_at(ex * 2, 2 + (ey - 1) * 2 - bob, DIR_DOWN);
        else arrow_at(ex * 2, 2 + (ey + 1) * 2 + bob, DIR_UP);
    } else {
        /* below: an arrow at the top edge and the exit's depth */
        arrow_at(ex * 2, 2 + bob, DIR_UP);
        const char *s = "EXIT: SURFACE (GO UP)";
        int tx = ex * 2 + 3;
        if (tx + (int)strlen(s) > 39) tx = ex * 2 - 1 - (int)strlen(s);
        text_box(tx - 1, 2, (int)strlen(s) + 2, 3);
        text_at(tx, 3, s);
        rs_text_setup(RS_BG1, 0, 0, 1);
    }
}
