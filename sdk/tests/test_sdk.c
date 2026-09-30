/*
 * RetroStone VC SDK unit tests: renderer golden images (layers, sprites,
 * priorities, flips, raster, affine/windows/math, text), input edges, save
 * RAM, RNG and audio.
 *   test_sdk [--update] [--golden DIR] [--out DIR]
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "rs_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

static int failures, checks, update, update_new;
static const char *golden_dir = "sdk/tests/golden", *out_dir = "build";

#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/* never called: the tests drive the runtime directly */
const rs_game *rs_game_main(void) { static const rs_game g = {"SDK tests", "sdktest", "1", 0, 0, 0, 0, 0, 0, 0, 0}; return &g; }

static uint16_t px(int x, int y) { return rs_host_framebuffer()[y * RS_SCREEN_W + x]; }
static uint16_t c565(rs_color c)
{
    unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    return (uint16_t)((r << 11) | (((g << 1) | (g >> 4)) << 5) | b);
}

static void golden(const char *name)
{
    char gp[512], ap[512];
    snprintf(gp, sizeof gp, "%s/%s.png", golden_dir, name);
    snprintf(ap, sizeof ap, "%s/%s_actual.png", out_dir, name);
    const uint16_t *fb = rs_host_framebuffer();
    if (update) {
        rsd_write_png(gp, fb, 1);
        printf("  updated %s\n", gp);
        return;
    }
    int w, h, n;
    unsigned char *img = stbi_load(gp, &w, &h, &n, 3);
    if (!img && update_new) {                    /* --new: write the goldens that do not exist yet */
        rsd_write_png(gp, fb, 1);
        printf("  created %s\n", gp);
        return;
    }
    checks++;
    if (!img) { failures++; printf("  FAIL golden %s missing (run with --update)\n", gp); rsd_write_png(ap, fb, 1); return; }
    int diff = 0;
    if (w != RS_SCREEN_W || h != RS_SCREEN_H) diff = -1;
    else
        for (int i = 0; i < w * h; i++) {
            uint16_t c = fb[i];
            unsigned r = c >> 11, g = (c >> 5) & 63, b = c & 31;
            unsigned char *p = img + i * 3;
            if (p[0] != ((r << 3) | (r >> 2)) || p[1] != ((g << 2) | (g >> 4)) || p[2] != ((b << 3) | (b >> 2)))
                diff++;
        }
    stbi_image_free(img);
    if (diff) {
        failures++;
        rsd_write_png(ap, fb, 1);
        printf("  FAIL golden %s: %d pixels differ (actual in %s)\n", name, diff, ap);
    }
}

/* ---- test fixtures --------------------------------------------------------- */
static const rs_color PAL[16] = {
    0, RS_HEX(0xff0000), RS_HEX(0x00ff00), RS_HEX(0x0000ff), RS_HEX(0xffff00), RS_HEX(0xff00ff),
    RS_HEX(0x00ffff), RS_HEX(0xffffff), RS_HEX(0x808080), RS_HEX(0x804000), RS_HEX(0x008040),
    RS_HEX(0x400080), RS_HEX(0xff8000), RS_HEX(0x80ff00), RS_HEX(0x0080ff), RS_HEX(0x202020)};

static void make_tiles(void)
{
    uint8_t t[64];
    /* tiles 1..15: solid colour n */
    for (int c = 1; c < 16; c++) { memset(t, c, 64); rs_tiles_load8(c, t, 1); }
    /* tile 16: "F" shape, asymmetric in both axes (colour 7 on colour 8) */
    static const char *F[8] = {"77777778", "78888888", "78888888", "77777888",
                               "78888888", "78888888", "78888888", "88888888"};
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) t[y * 8 + x] = (uint8_t)(F[y][x] - '0');
    rs_tiles_load8(16, t, 1);
    /* tile 17: checker 1/0 (transparent holes) */
    for (int i = 0; i < 64; i++) t[i] = (uint8_t)((((i & 7) ^ (i >> 3)) & 1) ? 1 : 0);
    rs_tiles_load8(17, t, 1);
    /* tile 18: diagonal stripe of colour 4 on transparent */
    for (int i = 0; i < 64; i++) t[i] = (uint8_t)(((i & 7) == (i >> 3)) ? 4 : 0);
    rs_tiles_load8(18, t, 1);
    /* tiles 32..47: a 16x16 sprite as 4 tiles, then a 32x32 as 16 tiles:
     * a numbered quadrant pattern so flips are visible */
    for (int q = 0; q < 20; q++) {
        for (int i = 0; i < 64; i++) {
            int x = i & 7, y = i >> 3;
            t[i] = (uint8_t)((x == 0 || y == 0) ? 15 : (x + y < 4 ? 0 : 1 + (q % 14)));
        }
        rs_tiles_load8(32 + q, t, 1);
    }
    for (int p = 0; p < 8; p++) {
        rs_pal_load(RS_PAL_BG(p), PAL, 16);
        rs_pal_load(RS_PAL_OBJ(p), PAL, 16);
    }
    /* distinguish palettes: colour 1 of palette p is a different red/green */
    for (int p = 1; p < 8; p++) {
        rs_pal_set(RS_PAL_BG(p) + 1, RS_RGB(31 - p * 3, p * 4, p * 2));
        rs_pal_set(RS_PAL_OBJ(p) + 1, RS_RGB(p * 4, 31 - p * 3, p * 2));
    }
}

static void fresh(void)
{
    rs_host_reset();
    make_tiles();
    rs_backdrop(RS_HEX(0x102030));
}

/* ---- renderer tests --------------------------------------------------------- */
static void test_layers(void)
{
    printf("layers\n");
    fresh();
    /* BG4: solid colour 2 everywhere, scrolled; BG3: checker; BG2: stripes; BG1: text-like tiles */
    for (int l = 0; l < 4; l++) { rs_bg_setup(l, 64, 32, 0); rs_bg_enable(l, 1); }
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 64; x++) {
            rs_bg_put(RS_BG4, x, y, RS_MAP(((x + y) & 1) ? 2 : 3, 0, 0, 0, 0));
            if ((x / 4 + y / 4) & 1) rs_bg_put(RS_BG3, x, y, RS_MAP(17, (x / 8) & 7, 0, 0, 0));
            if (y % 5 == 0) rs_bg_put(RS_BG2, x, y, RS_MAP(18, 0, 0, 0, 0));
            if (x % 9 == 0 && y % 3 == 0) rs_bg_put(RS_BG1, x, y, RS_MAP(16, 0, 0, 0, 0));
        }
    rs_bg_scroll(RS_BG4, 3, 5);
    rs_bg_scroll(RS_BG3, -13, 7);
    rs_bg_scroll(RS_BG2, 250, 0);   /* wraps around the 512-px map */
    rs_bg_scroll(RS_BG1, 0, -2);
    rs_host_render();
    golden("layers");
    /* BG4 scrolled by (3,5): screen (0,0) = map pixel (3,5) = tile (0,0) colour 3 (blue) */
    CHECK(px(0, 0) != 0, "layers: pixel (0,0) is black");
    /* map entry (0,0) BG1 is tile 16 at y scroll -2: screen (1,2) = F pixel (1,0) = colour 7 */
    CHECK(px(1, 2) == c565(PAL[7]), "layers: BG1 F glyph pixel at (1,2) = %04x", px(1, 2));
    rs_bg_enable(RS_BG1, 0); rs_bg_enable(RS_BG2, 0); rs_bg_enable(RS_BG3, 0);
    rs_host_render();
    CHECK(px(0, 0) == c565(PAL[3]), "layers: BG4 scroll origin colour = %04x", px(0, 0));
    CHECK(px(5, 0) == c565(PAL[2]), "layers: BG4 next tile colour = %04x", px(5, 0));
}

static void test_flips(void)
{
    printf("flips\n");
    fresh();
    rs_bg_setup(RS_BG1, 32, 32, 0);
    rs_bg_enable(RS_BG1, 1);
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < 40; x++)
            rs_bg_put(RS_BG1, x, y, RS_MAP(16, 0, 0, x & 1, y & 1));
    rs_host_render();
    golden("flips");
    CHECK(px(0, 0) == c565(PAL[7]) && px(7, 0) == c565(PAL[8]), "flips: plain tile");
    CHECK(px(15, 0) == c565(PAL[7]) && px(8, 0) == c565(PAL[8]), "flips: h-flip tile");
    CHECK(px(0, 15) == c565(PAL[7]) && px(0, 8) == c565(PAL[8]), "flips: v-flip tile");
    CHECK(px(15, 15) == c565(PAL[7]), "flips: hv-flip tile");
}

static void test_sprites(void)
{
    printf("sprites\n");
    fresh();
    rs_obj_base(32);
    /* each size drawn 4 times: plain, H, V, HV (2x2 grid) */
    static const int spr[][4] = {   /* w, h, x, y */
        {8, 8, 4, 4}, {16, 16, 28, 4}, {32, 32, 68, 4}, {16, 8, 140, 4}, {8, 16, 180, 4},
        {32, 16, 204, 4}, {16, 24, 4, 100}, {64, 64, 60, 100}};
    for (int i = 0; i < 8; i++) {
        int w = spr[i][0], h = spr[i][1];
        for (int f = 0; f < 4; f++)
            rs_spr(spr[i][2] + (f & 1) * (w + 2), spr[i][3] + (f >> 1) * (h + 2), 0, w, h, i, 0, f);
    }
    /* overlapping: lower OAM index in front, whatever the priority */
    rs_spr(240, 180, 0, 16, 16, 1, 0, 0);
    rs_spr(248, 188, 0, 16, 16, 2, 3, 0);
    /* partly off-screen */
    rs_spr(-8, 220, 0, 16, 16, 3, 0, 0);
    rs_spr(312, -8, 0, 16, 16, 4, 0, 0);
    rs_host_render();
    golden("sprites");
    /* (253,193) is colour 4 in the first sprite and colour 1 in the second */
    CHECK(px(253, 193) == c565(PAL[4]), "sprites: OAM order (%04x)", px(253, 193));
    /* plain 16x16 at (28,4): pixel (0,0) is the tile edge colour 15; H-flipped copy at (46,4)
     * has its edge column on the right: pixel (46+15, 4+1) = 15 */
    CHECK(px(28, 5) == c565(PAL[15]) && px(61, 5) == c565(PAL[15]), "sprites: h-flip edge");
    CHECK(px(28, 22 + 15) == c565(PAL[15]), "sprites: v-flip edge (%04x)", px(28, 37));
    CHECK(px(0, 221) != c565(RS_HEX(0x102030)), "sprites: left clip draws the visible part");
}

static void test_priority(void)
{
    printf("priority\n");
    fresh();
    /* each BG layer: left half low priority, right half high priority,
     * horizontal bands so every layer is visible somewhere */
    for (int l = 0; l < 4; l++) { rs_bg_setup(l, 64, 32, 0); rs_bg_enable(l, 1); }
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < 40; x++)
            for (int l = 0; l < 4; l++)
                if (y >= l * 6 && y < 30 - l * 2)
                    rs_bg_put(l, x, y, RS_MAP(1 + l * 2, 0, x >= 20, 0, 0)); /* colours 1,3,5,7 */
    rs_obj_base(0);
    /* sprites of priority 0..3 (solid colours 9..12) in the band where all layers exist */
    for (int p = 0; p < 4; p++)
        for (int i = 0; i < 20; i++)
            rs_spr(i * 16, 144 + p * 12, 9 + p, 8, 8, 0, p, 0);
    rs_host_render();
    golden("priority");
    /* rows: BG1 0..29, BG2 6..27, BG3 12..25, BG4 18..23; left half low priority, right half high */
    CHECK(px(8, 190) == c565(PAL[1]), "priority: BG1L over BG2L/BG3L/BG4L (%04x)", px(8, 190));
    CHECK(px(200, 190) == c565(PAL[1]), "priority: BG1H on top (%04x)", px(200, 190));
    CHECK(px(1, 145) == c565(PAL[1]), "priority: S0 below BG1L (%04x)", px(1, 145));
    CHECK(px(161, 181) == c565(PAL[12]), "priority: S3 above BG1H (%04x)", px(161, 181));
    CHECK(px(1, 169) == c565(PAL[11]), "priority: S2 above BG1L (%04x)", px(1, 169));
    CHECK(px(161, 169) == c565(PAL[1]), "priority: S2 below BG1H (%04x)", px(161, 169));
    rs_bg_enable(RS_BG1, 0);
    rs_bg_enable(RS_BG2, 0);
    rs_host_render();
    CHECK(px(200, 190) == c565(PAL[5]), "priority: BG3H over BG4H (%04x)", px(200, 190));
    CHECK(px(161, 157) == c565(PAL[10]), "priority: S1 above BG3H (%04x)", px(161, 157));
    CHECK(px(161, 145) == c565(PAL[5]), "priority: BG3H above S0 (%04x)", px(161, 145));
    CHECK(px(1, 145) == c565(PAL[9]), "priority: S0 above BG3L (%04x)", px(1, 145));
    rs_bg_enable(RS_BG2, 1);
    rs_host_render();
    CHECK(px(1, 157) == c565(PAL[3]), "priority: S1 below BG2L (%04x)", px(1, 157));
}

static int16_t wave[240];
static void raster_cb(int line, void *u)
{
    (void)u;
    rs_window(0, 160 - line / 2, 160 + line / 2);
    rs_pal_set(0, RS_RGB(line / 8, 0, 31 - line / 8));
}

static void test_raster(void)
{
    printf("raster\n");
    fresh();
    rs_bg_setup(RS_BG2, 64, 32, 0);
    rs_bg_enable(RS_BG2, 1);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 64; x++)
            if ((x + y) % 3 == 0) rs_bg_put(RS_BG2, x, y, RS_MAP(16, 0, 0, 0, 0));
    for (int i = 0; i < 240; i++) wave[i] = (int16_t)((i % 32 < 16 ? i % 16 : 16 - i % 16) - 8);
    rs_bg_line_scroll(RS_BG2, wave, NULL);
    rs_raster(raster_cb, NULL);
    rs_bg_window(RS_BG2, RS_WIN1_OUT);   /* BG2 only inside the triangle */
    rs_host_render();
    golden("raster");
    CHECK(px(2, 100) == c565(RS_RGB(100 / 8, 0, 31 - 100 / 8)), "raster: per-line backdrop");
}

static void test_affine_math(void)
{
    printf("affine + colour math\n");
    fresh();
    rs_bg_setup(RS_BG2, 32, 32, 0);
    rs_bg_enable(RS_BG2, 1);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) rs_bg_put(RS_BG2, x, y, RS_MAP(((x ^ y) & 1) ? 16 : 1 + (x % 7), 0, 0, 0, 0));
    /* rotate ~30 degrees, scale 1.25: a = cos/s, b = sin/s ... in 8.8 */
    rs_affine m = {179, -103, 103, 179, 128, 128, 1};
    rs_bg_affine(RS_BG2, &m);
    rs_bg_scroll(RS_BG2, -32, 8);
    rs_bg_setup(RS_BG1, 32, 32, 0);
    rs_bg_enable(RS_BG1, 1);
    for (int y = 10; y < 20; y++)
        for (int x = 5; x < 35; x++) rs_bg_put(RS_BG1, x, y, RS_MAP(3, 0, 0, 0, 0));
    rs_math(RS_MATH_ADD | RS_MATH_HALF, RS_MATH_BG1, 0);
    rs_window(1, 200, 280);
    rs_clip_black(RS_WIN2);
    rs_host_render();
    golden("affine_math");
    CHECK(px(240, 10) == 0, "window 2 clips to black");
    {
        /* the blended band differs from the opaque band everywhere */
        uint16_t row[RS_SCREEN_W];
        memcpy(row, rs_host_framebuffer() + 120 * RS_SCREEN_W, sizeof row);
        rs_math(RS_MATH_OFF, 0, 0);
        rs_host_render();
        int d = 0;
        for (int x = 40; x < 200; x++) d += row[x] != px(x, 120) && px(x, 120) == c565(PAL[3]);
        CHECK(d > 120, "colour math: %d/160 band pixels blended", d); /* blue over blue stays blue */
    }
    /* fixed-colour subtract on everything + brightness */
    rs_math(RS_MATH_SUB | RS_MATH_FIXED, RS_MATH_BG2 | RS_MATH_BACK, RS_RGB(8, 8, 8));
    rs_clip_black(0);
    rs_brightness(8);
    rs_host_render();
    golden("affine_fade");
}

static void test_text(void)
{
    printf("text\n");
    fresh();
    rs_bg_setup(RS_BG1, 64, 32, 0);
    rs_bg_enable(RS_BG1, 1);
    int n = rs_text_load(256, 1);
    CHECK(n == 96 + 384, "text: tiles uploaded = %d", n);
    rs_pal_set(RS_PAL_BG(0) + 1, RS_HEX(0xffffff));
    rs_pal_set(RS_PAL_BG(0) + 2, RS_HEX(0x000000));
    rs_text_setup(RS_BG1, 256, 0, 1);
    rs_text(1, 1, "RETROSTONE VC 0123456789 !?.,:;+-*/");
    rs_text(1, 3, "the quick brown fox jumps over the lazy dog");
    rs_text_big(1, 6, "BOMBER MOLE");
    rs_textf(1, 10, "%d HEARTS \x7f", 3);
    rs_host_render();
    golden("text");
}

/* ---- non-video tests ------------------------------------------------------ */
static int upd_calls;
static uint16_t seen_pressed[8], seen_released[8], seen_held[8];
static void t_update(void)
{
    if (upd_calls < 8) {
        seen_pressed[upd_calls] = rs_pad_pressed(0);
        seen_released[upd_calls] = rs_pad_released(0);
        seen_held[upd_calls] = rs_pad(0);
    }
    upd_calls++;
}
static void test_input(void)
{
    printf("input\n");
    static const rs_game g = {"input", "input", "1", 0, t_update, 0, 0, 0, 0, 0, 0};
    rs_host_init(&g);
    uint16_t seq[5] = {0, RS_BTN_B, RS_BTN_B | RS_BTN_UP, RS_BTN_UP, 0};
    for (int i = 0; i < 5; i++) { rs_host_set_pad(0, seq[i], 1); rs_host_frame(); }
    CHECK(seen_pressed[1] == RS_BTN_B, "input: B pressed on frame 1");
    CHECK(seen_pressed[2] == RS_BTN_UP && seen_held[2] == (RS_BTN_B | RS_BTN_UP), "input: UP pressed, B held");
    CHECK(seen_released[3] == RS_BTN_B, "input: B released on frame 3");
    CHECK(seen_released[4] == RS_BTN_UP && seen_held[4] == 0, "input: UP released on frame 4");
    CHECK(rsd_buttons("B+START+LEFT") == (RS_BTN_B | RS_BTN_START | RS_BTN_LEFT), "input: button names");
    rs_host_set_pad(3, RS_BTN_R, 1);
    CHECK(rs_pad(3) == RS_BTN_R && rs_pad_connected(3), "input: port 4");
    CHECK(rs_pad_device(2) == RS_DEVICE_PAD, "input: a pad by default");
    rs_host_set_pad_device(0, RS_DEVICE_KEYBOARD);
    rs_host_set_pad_device(1, RS_DEVICE_KEYBOARD2);
    rs_host_set_pad_device(2, 7);                        /* unknown: ignored */
    CHECK(rs_pad_device(0) == RS_DEVICE_KEYBOARD && rs_pad_device(1) == RS_DEVICE_KEYBOARD2 &&
          rs_pad_device(2) == RS_DEVICE_PAD && rs_pad_device(9) == RS_DEVICE_PAD, "input: the device of each port");
    rs_host_set_pad_device(0, RS_DEVICE_PAD);
    rs_host_set_pad_device(1, RS_DEVICE_PAD);
    rs_host_shutdown();
}

static void test_sram(void)
{
    printf("save RAM\n");
    uint8_t *s = rs_sram();
    CHECK(s == rs_host_sram(), "sram: same buffer");
    rs_host_sram_dirty(1);
    for (int i = 0; i < RS_SRAM_SIZE; i++) s[i] = (uint8_t)(i * 7 + 3);
    CHECK(!rs_host_sram_dirty(0), "sram: not dirty before commit");
    rs_sram_commit();
    CHECK(rs_host_sram_dirty(1) && !rs_host_sram_dirty(0), "sram: commit sets dirty, clear resets");
    char p[512];
    snprintf(p, sizeof p, "%s/test.srm", out_dir);
    CHECK(rsd_sram_save(p) == 0, "sram: save");
    memset(s, 0, RS_SRAM_SIZE);
    CHECK(rsd_sram_load(p) == 0, "sram: load");
    int bad = 0;
    for (int i = 0; i < RS_SRAM_SIZE; i++) bad += s[i] != (uint8_t)(i * 7 + 3);
    CHECK(bad == 0, "sram: round trip (%d bad bytes)", bad);
    size_t n = 0;
    void *d = rsd_load_file(p, &n);
    CHECK(d && n == RS_SRAM_SIZE, "sram: file size %u", (unsigned)n);
    free(d);
    /* the runtime reset must not clear the save RAM (frontends load it before init) */
    rs_host_reset();
    CHECK(rs_sram()[100] == (uint8_t)(100 * 7 + 3), "sram: survives reset");
}

static void test_rng(void)
{
    printf("rng\n");
    rs_rng a, b;
    rs_rng_seed(&a, 1234);
    rs_rng_seed(&b, 1234);
    int same = 1, inrange = 1;
    for (int i = 0; i < 1000; i++) {
        if (rs_rng_next(&a) != rs_rng_next(&b)) same = 0;
        int r = rs_rng_range(&a, 7);
        rs_rng_range(&b, 7);
        if (r < 0 || r >= 7) inrange = 0;
    }
    CHECK(same, "rng: deterministic");
    CHECK(inrange, "rng: range");
    rs_rng_seed(&a, 1);
    CHECK(rs_rng_next(&a) == 270369u, "rng: xorshift32 reference value (%u)", a.s);
}

static void test_audio(void)
{
    printf("audio\n");
    rs_host_reset();
    int16_t sq[64];
    for (int i = 0; i < 64; i++) sq[i] = i < 32 ? 8000 : -8000;
    CHECK(rs_sample_pcm16(0, sq, 64, 32000, 0) == 0, "audio: load looping sample");
    rs_adsr env = {5, 20, 64, 30};
    rs_voice_play(0, 0, RS_PITCH_1, 127, 0, &env);   /* hard left */
    static const rs_game g = {"audio", "audio", "1", 0, 0, 0, 0, 0, 0, 0, 0};
    (void)g;
    rs_host_frame();
    int n;
    const int16_t *a = rs_host_audio(&n);
    CHECK(n == 533 || n == 534, "audio: frames per video frame = %d", n);
    long el = 0, er = 0;
    for (int i = 0; i < n; i++) { el += labs(a[i * 2]); er += labs(a[i * 2 + 1]); }
    CHECK(el > 100000 && er < el / 50, "audio: panned left (L %ld, R %ld)", el, er);
    rs_voice_release(0);
    for (int i = 0; i < 4; i++) rs_host_frame();
    CHECK(!rs_voice_active(0), "audio: release ends the voice");
    int v = rs_sfx(0, 100, 64, RS_PITCH_1 * 2);
    CHECK(v >= 0 && v < RS_VOICES, "audio: sfx voice %d", v);
    /* ADPCM: a slow ramp decodes to a monotonic-ish signal */
    uint8_t ad[32];
    memset(ad, 0x44, sizeof ad);   /* +4 steps */
    CHECK(rs_sample_adpcm(1, ad, 64, 16000, -1) == 0, "audio: adpcm load");
    rs_echo(100, 60, 50);
    rs_voice_echo(v, 1);
    rs_host_frame();
    CHECK(rs_music_play("not a module", 12, 1) != 0, "audio: bad module rejected");
    rs_host_reset();
}

/* ---- save states ---------------------------------------------------------------------- */
static struct { int counter; int16_t lines[RS_SCREEN_H]; int *cursor; } st_obj;
static const int st_table[4] = {10, 20, 30, 40};
static const int *st_pick;                          /* points into a reference (not saved) */
static void st_raster(int line, void *u) { (void)u; if (line == 100) rs_pal_set(1, RS_RGB(31, 0, 0)); }
static void st_register(void)
{
    rs_state_var("st_obj", &st_obj, sizeof st_obj);
    rs_state_ptr("st_obj.cursor", &st_obj.cursor);
    rs_state_ptr("st_pick", &st_pick);
    rs_state_ref("st_table", st_table, sizeof st_table);
    rs_state_raster("st_raster", st_raster);
}
static void st_update(void)
{
    st_obj.counter++;
    st_obj.cursor = &st_obj.counter;
    st_pick = &st_table[st_obj.counter & 3];
    for (int i = 0; i < RS_SCREEN_H; i++) st_obj.lines[i] = (int16_t)((i + st_obj.counter) & 7);
    rs_bg_scroll(RS_BG1, st_obj.counter, rs_rand_range(8));
    rs_oam(0)->x = (int16_t)(st_obj.counter * 3);
}
static void st_init(void)
{
    rs_pal_set(0, RS_RGB(0, 0, 8));
    for (int t = 1; t < 4; t++)
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) rs_tile_pixel(t, x, y, (x + y + t) & 15);
    for (int i = 1; i < 16; i++) rs_pal_set(i, RS_RGB(i * 2, 31 - i * 2, i));
    rs_bg_setup(RS_BG1, 64, 32, 0);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 64; x++) rs_bg_put(RS_BG1, x, y, RS_MAP(1 + (x + y) % 3, 0, 0, x & 1, 0));
    rs_bg_enable(RS_BG1, 1);
    rs_bg_line_scroll(RS_BG1, st_obj.lines, NULL);
    rs_raster(st_raster, NULL);
    rs_spr(40, 40, 2, 16, 16, 0, 2, 0);
    static int16_t sq[64];
    for (int i = 0; i < 64; i++) sq[i] = i < 32 ? 8000 : -8000;
    rs_sample_pcm16(0, sq, 64, 32000, 0);
    rs_voice_play(0, 0, RS_PITCH_1, 100, 64, NULL);
    rs_echo(50, 40, 40);
}

static uint64_t fb_hash(void)
{
    uint64_t h = 0xcbf29ce484222325ull;
    const uint8_t *p = (const uint8_t *)rs_host_framebuffer();
    for (int i = 0; i < RS_SCREEN_W * RS_SCREEN_H * 2; i++) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

static void test_states(void)
{
    printf("save states\n");
    static const rs_game none = {"none", "none", "1", 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(rs_host_state_size(&none) == 0, "states: a game without the state callback has none");
    static const rs_game g = {"states", "statetest", "1", st_init, st_update, 0, 0, 0, st_register, 0, 3};
    size_t size = rs_host_state_size(&g);
    CHECK(size > 250000 && rs_host_state_size(&g) == size, "states: a fixed size (%u bytes)", (unsigned)size);
    rs_host_init(&g);
    for (int i = 0; i < 5; i++) rs_host_frame();
    uint8_t *a = calloc(1, size), *b = calloc(1, size);
    size_t used = rs_host_state_save(a, size);
    CHECK(used > 250000 && used <= size && !memcmp(a, "RSVC", 4), "states: saved (%u bytes used)", (unsigned)used);
    CHECK(rs_host_state_save(b, size - 1) == 0, "states: a buffer too small is refused");
    rs_host_frame();
    uint64_t h1 = fb_hash();
    int c1 = st_obj.counter;
    for (int i = 0; i < 7; i++) rs_host_frame();     /* everything moves on */
    rs_pal_set(3, RS_RGB(1, 2, 3));
    rs_tile_pixel(2, 0, 0, 9);
    rs_bg_line_scroll(RS_BG1, NULL, NULL);
    rs_raster(NULL, NULL);
    CHECK(rs_host_state_load(a, size) == 0, "states: loaded");
    CHECK(st_obj.cursor == &st_obj.counter && st_pick == &st_table[st_obj.counter & 3],
          "states: pointers restored (into a saved object and into a reference)");
    CHECK(rs_host_state_save(b, size) == used && !memcmp(a, b, size), "states: saving again gives the same bytes");
    rs_host_frame();
    CHECK(fb_hash() == h1 && st_obj.counter == c1, "states: the next frame is the same (line scroll, raster, OAM)");
    CHECK(rs_frame_count() == 6, "states: frame counter restored (%u)", rs_frame_count());
    /* registering outside the callback does nothing */
    static int stray;
    rs_state_var("stray", &stray, sizeof stray);
    CHECK(rs_host_state_size(&g) == size, "states: registration outside rs_game.state is ignored");
    /* pointers a state cannot express: saving fails */
    static int16_t unregistered[RS_SCREEN_H];
    rs_bg_line_scroll(RS_BG1, unregistered, NULL);
    CHECK(rs_host_state_save(b, size) == 0, "states: a line-scroll table outside the saved objects: not saved");
    rs_bg_line_scroll(RS_BG1, st_obj.lines + 10, NULL);   /* 240 entries do not fit after it */
    CHECK(rs_host_state_save(b, size) == 0, "states: a line-scroll table overflowing its object: not saved");
    rs_bg_line_scroll(RS_BG1, st_obj.lines, NULL);
    int local;
    st_obj.cursor = &local;
    CHECK(rs_host_state_save(b, size) == 0, "states: a pointer to the stack: not saved");
    st_obj.cursor = &st_obj.counter;
    CHECK(rs_host_state_save(b, size) == used, "states: fixed, saved again");
    /* the game, version and size checks */
    a[8] ^= 1;
    CHECK(rs_host_state_load(a, size) != 0, "states: another game id is refused");
    a[8] ^= 1;
    a[44] ^= 1;
    CHECK(rs_host_state_load(a, size) != 0, "states: another state_version is refused");
    a[44] ^= 1;
    CHECK(rs_host_state_load(a, used - 1) != 0 && rs_host_state_load(a, used) == 0, "states: truncated refused, whole loaded");
    free(a);
    free(b);
    rs_host_shutdown();
    rs_host_reset();
}

/* ---- viewports (split screen) ------------------------------------------------------ */
static int raster_views[RS_VIEW_MAX + 1];
static void view_raster(int line, void *u)
{
    (void)u;
    int v = rs_viewport_current();
    if (line == 150) raster_views[v + 1]++;
}

/* a world of numbered tiles (64x64 map), 3 sprites per viewport */
static void viewport_scene(int layout, int flags)
{
    fresh();
    rs_bg_setup(RS_BG4, 64, 64, 0);
    rs_bg_setup(RS_BG3, 64, 64, 0);
    rs_bg_setup(RS_BG1, 64, 32, 0);
    rs_bg_enable(RS_BG4, 1);
    rs_bg_enable(RS_BG3, 1);
    rs_bg_enable(RS_BG1, 1);
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            rs_bg_put(RS_BG4, x, y, RS_MAP(1 + (x / 4 + y / 4) % 14, 0, 0, 0, 0));
            if ((x + 2 * y) % 7 == 0) rs_bg_put(RS_BG3, x, y, RS_MAP(16, (x / 8) & 7, 0, x & 1, 0));
        }
    for (int x = 0; x < 20; x++) rs_bg_put(RS_BG1, x, 0, RS_MAP(18, 0, 1, 0, 0));   /* a HUD strip */
    rs_viewport v[RS_VIEW_MAX];
    int n = rs_viewport_layout(layout, flags, v);
    rs_oam_clear();
    for (int i = 0; i < n; i++) {
        v[i].oam_first = (uint16_t)rs_oam_next();
        for (int k = 0; k < 3; k++)                 /* relative to the viewport; the last one straddles its right edge */
            rs_spr(k == 2 ? v[i].w - 8 : 8 + k * 24 + i * 4, 12 + k * 20, 32, 16, 16, (i + k) & 7, 2, 0);
        v[i].oam_count = (uint16_t)(rs_oam_next() - v[i].oam_first);
        for (int l = 0; l < 4; l++) { v[i].sx[l] = (int16_t)(i * 77); v[i].sy[l] = (int16_t)(i * 45 - 8); }
        v[i].sx[RS_BG1] = 0;
        v[i].sy[RS_BG1] = 0;
    }
    rs_viewports(n, v, RS_HEX(0x40ff40));
}

static void test_viewports(void)
{
    printf("viewports\n");
    static const struct { int players, flags; const char *name; } L[] = {
        {1, 0, "viewports_1"}, {2, 0, "viewports_2"}, {2, RS_LAYOUT_HSPLIT, "viewports_2h"},
        {3, 0, "viewports_3"}, {3, RS_LAYOUT_MAP, "viewports_3map"}, {4, 0, "viewports_4"}};
    for (unsigned k = 0; k < sizeof L / sizeof L[0]; k++) {
        viewport_scene(L[k].players, L[k].flags);
        rs_host_render();
        golden(L[k].name);
    }
    /* 2 players: the divider between the halves, each half shows its own scroll */
    viewport_scene(2, 0);
    rs_host_render();
    CHECK(px(159, 100) == c565(RS_HEX(0x40ff40)) && px(160, 100) == c565(RS_HEX(0x40ff40)),
          "viewports: the divider at x=159..160 (%04x %04x)", px(159, 100), px(160, 100));
    rs_viewport v[4];
    rs_viewport_layout(2, 0, v);
    CHECK(v[0].w == 159 && v[1].x == 161 && v[1].w == 159 && v[0].h == 240, "viewports: layout 2 = two 159x240 halves");
    CHECK(rs_viewport_layout(4, 0, v) == 4 && v[3].x == 161 && v[3].y == 121 && v[3].h == 119, "viewports: layout 4 = quadrants");
    CHECK(rs_viewport_layout(3, RS_LAYOUT_MAP, v) == 4, "viewports: layout 3 with a map = 4 rectangles");
    /* the same world pixel in the left half at scroll (0,-8) and the right half at scroll (77,37) */
    viewport_scene(2, 0);
    rs_host_render();
    uint16_t a = px(77, 53), b = px(161, 8);      /* both show the world pixel (77, 45) */
    CHECK(a == b, "viewports: each viewport has its own scroll (%04x vs %04x)", a, b);
    /* sprites are clipped to their viewport: the one on the left half's right edge stops at x=158 */
    int spr_edge = 0;
    for (int y = 52; y < 68; y++) spr_edge += px(159, y) == c565(RS_HEX(0x40ff40)) && px(160, y) == c565(RS_HEX(0x40ff40));
    CHECK(spr_edge == 16, "viewports: a sprite straddling the edge is clipped at the divider (%d)", spr_edge);
    /* the raster callback runs per viewport */
    memset(raster_views, 0, sizeof raster_views);
    viewport_scene(4, 0);
    rs_raster(view_raster, NULL);
    rs_host_render();
    CHECK(raster_views[0] == 0 && raster_views[3] == 1 && raster_views[4] == 1,
          "viewports: raster callback per viewport (line 150: views 2 and 3)");
    /* an overlay viewport drawn over the others (a pause box) */
    viewport_scene(4, 0);
    rs_viewport o[5];
    int n = rs_viewport_layout(4, 0, o);
    memset(&o[n], 0, sizeof o[n]);
    o[n].x = 100; o[n].y = 90; o[n].w = 120; o[n].h = 60; o[n].layers = 1 << RS_BG1;
    rs_viewports(n + 1, o, RS_HEX(0x40ff40));
    rs_host_render();
    CHECK(px(160, 120) != c565(RS_HEX(0x40ff40)), "viewports: an overlay covers the divider");
    golden("viewports_overlay");
    rs_viewports(0, NULL, 0);
    rs_host_render();
    CHECK(rs_viewport_count() == 0, "viewports: off again");
}

/* PPU stress: 4 full layers (two with per-line scroll, one with priority
 * tiles), 128 sprites of 32x32 (max 32 on a line is exceeded on purpose in
 * places), colour math on one layer. Prints the average render time. */
static void bench_ppu(int frames)
{
    static int16_t wave2[240];
    fresh();
    for (int l = 0; l < 4; l++) {
        rs_bg_setup(l, 64, 64, 0);
        rs_bg_enable(l, 1);
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 64; x++)
                rs_bg_put(l, x, y, RS_MAP(l == 3 ? 1 + (x + y) % 8 : 16 + ((x * 7 + y * 3 + l) % 3), l,
                                          (x + y + l) & 1, x & 1, y & 1));
    }
    for (int i = 0; i < 240; i++) wave2[i] = (int16_t)((i * 5) % 16 - 8);
    rs_bg_line_scroll(RS_BG2, wave2, NULL);
    rs_bg_line_scroll(RS_BG3, NULL, wave2);
    rs_math(RS_MATH_ADD | RS_MATH_HALF, RS_MATH_BG2, 0);
    rs_obj_base(32);
    uint64_t total = 0;
    for (int f = 0; f < frames; f++) {
        rs_oam_clear();
        for (int i = 0; i < 128; i++)
            rs_spr((i * 37 + f) % 336 - 16, (i * 53 + f / 2) % 256 - 16, 0, 32, 32, i & 7, i & 3, i & 3);
        for (int l = 0; l < 4; l++) rs_bg_scroll(l, f * (l + 1), f / (l + 1));
        uint64_t t0 = rs_host_time_us();
        rs_host_render();
        total += rs_host_time_us() - t0;
    }
    printf("PPU stress (4 layers 64x64, 2 with line scroll, colour math, 128 sprites 32x32): "
           "%.1f us per frame over %d frames\n", (double)total / frames, frames);
    /* the same scene in 4 viewports (quadrants), 32 of the sprites each */
    rs_viewport v[4];
    int n = rs_viewport_layout(4, 0, v);
    uint64_t t4 = 0;
    for (int f = 0; f < frames; f++) {
        rs_oam_clear();
        for (int i = 0; i < n; i++) {
            v[i].oam_first = (uint16_t)rs_oam_next();
            for (int k = 0; k < 32; k++)
                rs_spr((k * 37 + f) % 176 - 16, (k * 53 + f / 2) % 136 - 16, 0, 32, 32, k & 7, k & 3, k & 3);
            v[i].oam_count = 32;
            for (int l = 0; l < 4; l++) { v[i].sx[l] = (int16_t)(f * (l + 1) + i * 80); v[i].sy[l] = (int16_t)(f / (l + 1) + i * 60); }
        }
        rs_viewports(n, v, 0);
        uint64_t t0 = rs_host_time_us();
        rs_host_render();
        t4 += rs_host_time_us() - t0;
    }
    rs_viewports(0, NULL, 0);
    printf("PPU stress in 4 viewports (quadrants, 32 sprites each): %.1f us per frame (%+.1f%%)\n",
           (double)t4 / frames, 100.0 * ((double)t4 - (double)total) / (double)total);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--bench")) {
            rs_host_set_log(NULL);
            bench_ppu(600);
            return 0;
        }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--update")) update = 1;
        else if (!strcmp(argv[i], "--new")) update_new = 1;
        else if (!strcmp(argv[i], "--golden") && i + 1 < argc) golden_dir = argv[++i];
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) out_dir = argv[++i];
    }
    rs_host_set_log(NULL);
    test_layers();
    test_flips();
    test_sprites();
    test_priority();
    test_raster();
    test_affine_math();
    test_text();
    test_viewports();
    test_input();
    test_sram();
    test_rng();
    test_audio();
    test_states();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
