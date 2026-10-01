/*
 * Duck Parade: the UI on the real picture. The whole game runs (runtime, draw, the house kit) and each screen is
 * checked on the layers, OAM and the rendered frame, then saved as a PNG:
 *   --mode title   the title (the logo on BG1, the prompt and its A glyph, the copyright), get ready, the first
 *                  hop starting the run, the pause (dimmed, PAUSED) and the resume;
 *   --mode run     the bot plays (--opt bot=1 ...): after every frame the HUD's digits show the score and the
 *                  duckling count is right; every 16 frames the sky band hides the world (the picture's band is
 *                  the same with the world's sprites hidden); a banking shows the family photo; the game-over
 *                  banner, panel, score, best, the egg medal and the retry line;
 *   --mode coop    Mother and Father (bot=2 players=2): two scores in the HUD, the family panel at the end.
 * The house kit's layout (games/common/src/house_ui.h): BG1 tiles 0..95 the font, 192..287 the 2x glyphs,
 * 288..296 the panel frame; the kit sprites from KIT_OBJ_TILE: the digits first (4 tiles each).
 *
 *   test_ui --mode M [--out DIR] [--frames N] [--opt key=value]...
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 */
#include "dp.h"
#include "rs_host.h"
#include "rs_desktop.h"
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const rs_game *rs_game_main(void);

static int fails, checks;
static const char *out_dir = "build/duckparade-ui";
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; if (fails < 30) { printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } } while (0)
#define OK(...) do { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } while (0)

static void quiet_log(const char *line) { if (strstr(line, "FAIL") || strstr(line, "strict")) printf("%s\n", line); }

static void frame(uint16_t p1, uint16_t p2)
{
    rs_host_set_pad(0, p1, 1);
    rs_host_set_pad(1, p2, 1);
    rs_host_frame();
}

static void frames(int n) { for (int i = 0; i < n; i++) frame(0, 0); }
static void tap(uint16_t b) { frame(b, 0); frame(b, 0); frame(0, 0); }

/* a press on pad 1..3 (pads 3 and 4: players 3 and 4 join there) */
static void pad_tap(int pad, uint16_t b)
{
    if (pad == 1) { frame(0, b); frame(0, 0); return; }
    rs_host_set_pad(pad, b, 1);
    frame(0, 0);
    rs_host_set_pad(pad, 0, 1);
    frame(0, 0);
}

static void shot(const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.png", out_dir, name);
    rsd_write_png(path, rs_host_framebuffer(), 1);
}

/* BG1 text: the kit's small font is tiles 0..95 (ASCII 32..127) on palette 0 */
static int bg1_text_at(int x, int y, const char *s, int base)
{
    for (int i = 0; s[i]; i++) {
        uint16_t e = rs_bg_get(RS_BG1, x + i, y);
        if (s[i] == ' ') continue;
        if (RS_MAP_TILE(e) != (unsigned)(base + s[i] - 32)) return 0;
    }
    return 1;
}

#define bg1_text_is(x, y, s) bg1_text_at(x, y, s, 0)
#define bg1_box_is(x, y, s) bg1_text_at(x, y, s, 96)     /* the font on the panel colour */

static int bg1_count(int y0, int y1, int tile_lo, int tile_hi)
{
    int n = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = 0; x < 40; x++) {
            int t = RS_MAP_TILE(rs_bg_get(RS_BG1, x, y));
            if (t >= tile_lo && t <= tile_hi) n++;
        }
    return n;
}

static int bg1_pal_count(int y0, int y1, int pal)
{
    int n = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = 0; x < 40; x++) {
            uint16_t e = rs_bg_get(RS_BG1, x, y);
            if (RS_MAP_TILE(e) && RS_MAP_PAL(e) == pal) n++;
        }
    return n;
}

/* the number drawn in the kit's big digits on sprite row y (x in [x0, x1)), -1 = none */
static int digits_at(int y, int x0, int x1)
{
    int xs[8], ds[8], n = 0;
    for (int i = 0; i < RS_OAM_MAX && n < 8; i++) {
        const rs_sprite *s = rs_oam(i);
        if (!s->used || (s->flags & RS_SPR_HIDE) || s->y != y || s->x < x0 || s->x >= x1 || s->w != 16) continue;
        int rel = s->tile - KIT_OBJ_TILE;
        if (rel < 0 || rel >= 40 || rel % 4) continue;
        xs[n] = s->x;
        ds[n++] = rel / 4;
    }
    if (!n) return -1;
    for (int a = 0; a < n; a++)
        for (int b = a + 1; b < n; b++)
            if (xs[b] < xs[a]) { int t = xs[a]; xs[a] = xs[b]; xs[b] = t; t = ds[a]; ds[a] = ds[b]; ds[b] = t; }
    int v = 0;
    for (int a = 0; a < n; a++) v = v * 10 + ds[a];
    return v;
}


static int sprite_range(int id, int nframes, int prio)
{
    int n = 0, t0 = dp_spr[id].tile, t1 = dp_spr[id + nframes - 1].tile;
    for (int i = 0; i < RS_OAM_MAX; i++) {
        const rs_sprite *s = rs_oam(i);
        if (s->used && !(s->flags & RS_SPR_HIDE) && s->tile >= t0 && s->tile <= t1 && (prio < 0 || s->prio == prio)) n++;
    }
    return n;
}

static long luma(void)
{
    const uint16_t *fb = rs_host_framebuffer();
    long s = 0;
    for (int i = 0; i < RS_SCREEN_W * RS_SCREEN_H; i++) s += (fb[i] >> 11) + ((fb[i] >> 5) & 63) / 2 + (fb[i] & 31);
    return s;
}

/* the sky band hides the world: the band's pixels are the same with the world's sprites (priority < 3) hidden */
static int band_hides_world(void)
{
    static uint16_t a[RS_SCREEN_W * FIELD_Y];
    static uint8_t flags[RS_OAM_MAX];
    rs_host_render();
    memcpy(a, rs_host_framebuffer(), sizeof a);
    int world_in_band = 0;
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        flags[i] = s->flags;
        if (s->used && s->prio < 3) { if (s->y < FIELD_Y && s->y + s->h > 0) world_in_band++; s->flags |= RS_SPR_HIDE; }
    }
    rs_host_render();
    int same = !memcmp(a, rs_host_framebuffer(), sizeof a);
    for (int i = 0; i < RS_OAM_MAX; i++) rs_oam(i)->flags = flags[i];
    rs_host_render();
    return same ? world_in_band : -1;
}

static void mode_title(void)
{
    int st;
    frames(100);
    dp_test_world(&st);
    CHECK(st == DS_TITLE, "the title shows (st %d)", st);
    int logo = bg1_pal_count(2, 9, PAL_LOGO);
    CHECK(logo > 80, "the house logo on BG1 rows 2-9 (%d tiles on the logo palette)", logo);
    int prompt = 0, glyph = 0;
    for (int i = 0; i < 60; i++) {
        frame(0, 0);
        prompt |= bg1_text_is(10, 21, "PRESS ANY ARROW TO HOP");
        for (int k = 0; k < RS_OAM_MAX; k++) {
            const rs_sprite *s = rs_oam(k);
            if (s->used && s->pal == 3 && s->y == 21 * 8 - 4 && s->tile >= KIT_OBJ_TILE && s->x < 10 * 8) glyph = 1;
        }
    }
    CHECK(prompt, "PRESS ANY ARROW TO HOP on row 21 (it blinks)");
    CHECK(glyph, "the D-pad glyph beside it");
    CHECK(bg1_text_is(4, 28, "(C) 2026 8BCRAFT - RETROSTONE VC"), "the copyright line on row 28");
    CHECK(bg1_text_is(4, 18, "P1"), "P1's slot on row 18");
    CHECK(sprite_range(SPR_LING_RIGHT, 2, 2) >= 3 && sprite_range(SPR_DUCK_RIGHT, 4, 2) >= 1, "Mother and her ducklings march under the logo");
    shot("title");
    /* parents 2, 3 and 4 join (A on their pads): their heads pop in on their slots, in their palettes */
    static const char *names[3] = {"title-2-players", "title-3-players", "title-4-players"};
    for (int p = 1; p < 4; p++) {
        pad_tap(p, RS_BTN_A);
        frames(30);
        int heads = 0;
        for (int k = 0; k < RS_OAM_MAX; k++) {
            const rs_sprite *s = rs_oam(k);
            if (s->used && s->tile >= dp_spr[SPR_DUCK_HEAD].tile && s->tile <= dp_spr[SPR_DUCK_HEAD + 1].tile && s->pal == (p == 1 ? 1 : p + 2)) heads++;
        }
        CHECK(heads == 1, "P%d joins: its head on its slot (palette %d)", p + 1, p == 1 ? 1 : p + 2);
        shot(names[p - 1]);
    }
    if (!fails) OK("title: logo, prompt and glyph, copyright, the family, four parents join");
    /* P2-P4 leave (B): back to Mother alone */
    for (int p = 3; p >= 1; p--) { pad_tap(p, RS_BTN_B); frames(3); }
    tap(RS_BTN_A);
    frames(20);
    const world *w = dp_test_world(&st);
    CHECK(st == DS_PLAY && w->players == 1 && w->started && w->d[0].max_col == START_COL + 1,
          "A starts the run and is the first hop (st %d, players %d, col %d)", st, w->players, w->d[0].max_col);
    CHECK(bg1_pal_count(2, 9, PAL_LOGO) == 0, "the logo is gone");
    CHECK(digits_at(0, 100, 220) == world_score(w, 0), "the HUD shows the score (%d)", world_score(w, 0));
    long bright = luma();
    tap(RS_BTN_START);
    frames(4);
    long dim = luma();
    int cam = world_cam_px(w);
    frames(40);
    CHECK(dim < bright * 3 / 4, "Start pauses: the picture dims (%ld -> %ld)", bright, dim);
    CHECK(bg1_count(13, 14, 192, 287) >= 10, "PAUSED in 2x glyphs on row 13");
    CHECK(world_cam_px(w) == cam, "the world is frozen");
    shot("pause");
    tap(RS_BTN_START);
    frames(40);
    CHECK(world_cam_px(w) > cam && luma() > dim, "Start again resumes");
    CHECK(bg1_count(13, 14, 192, 287) == 0, "PAUSED is gone");
    if (!fails) OK("get ready, the first hop, the HUD, pause and resume");
}

static void mode_run(int max_frames)
{
    int st, last_st = -1, banks = 0, photo_ok = 0, band_checks = 0, band_bad = 0, hud_bad = 0, hud_checks = 0, line_bad = 0;
    int over_checked = 0;
    for (int f = 0; f < max_frames; f++) {
        frame(0, 0);
        const world *w = dp_test_world(&st);
        if (st == DS_PLAY) {
            hud_checks++;
            int shown = digits_at(0, 100, 220);
            if (shown != world_score(w, 0)) { if (hud_bad++ < 3) printf("  frame %d: the HUD shows %d, the score is %d\n", f, shown, world_score(w, 0)); }
            char s[8];
            snprintf(s, sizeof s, "%d", world_line_total(w));
            if (!bg1_text_is(3, 1, s)) line_bad++;
            if ((f & 15) == 0) {
                band_checks++;
                int r = band_hides_world();
                if (r < 0) band_bad++;
            }
            if (w->events[0] & EV_BANK) {
                banks++;
                frames(6);                                     /* the family pops in, one after the other */
                int panel = bg1_count(3, 8, 288, 296);
                int fam = sprite_range(SPR_LING_DOWN, 2, 3) + sprite_range(SPR_DUCK_DOWN, 4, 3);
                if (panel >= 30 && fam >= 2) photo_ok++;
                if (banks == 1) {
                    frames(10);
                    shot("banking");
                }
            }
        }
        if (st == DS_OVER && last_st != DS_OVER) {
            frames(45);
            w = dp_test_world(&st);
            int s0 = world_score(w, 0);
            CHECK(bg1_count(4, 7, 288, 296) >= 40, "the GAME OVER banner (a 20x4 panel at row 4)");
            CHECK(bg1_count(9, 20, 288, 296) >= 200, "the score panel (20x12 at row 9)");
            CHECK(bg1_count(5, 6, 192, 287) >= 16, "GAME OVER in gold 2x glyphs");
            CHECK(digits_at(11 * 8 - 4, 150, 260) == s0, "SCORE shows %d", s0);
            CHECK(digits_at(14 * 8 - 4, 150, 260) >= s0, "BEST shows the best");
            int medal = medal_of(s0), egg = sprite_range(SPR_EGG, 4, 3);
            CHECK(medal ? egg == 1 : egg == 0, "the egg medal (tier %d, %d egg sprites)", medal, egg);
            CHECK(bg1_box_is(14, 19, "A: HOP AGAIN") || bg1_box_is(14, 19, "            "), "the retry line on row 19");
            shot("gameover");
            over_checked = 1;
            printf("  the run: score %d, lanes %d, banked %d; %d frames checked\n", s0, w->d[0].max_col - START_COL, w->d[0].banked, hud_checks);
            break;
        }
        last_st = st;
    }
    CHECK(hud_checks > 300 && hud_bad == 0, "the HUD showed the score on every frame of the run (%d frames, %d wrong)", hud_checks, hud_bad);
    CHECK(line_bad == 0, "the duckling count on the HUD (%d wrong)", line_bad);
    CHECK(band_checks > 20 && band_bad == 0, "the sky band hides the world's sprites (%d checks, %d failed)", band_checks, band_bad);
    CHECK(banks == 0 || photo_ok == banks, "the family photo after each banking (%d of %d)", photo_ok, banks);
    CHECK(over_checked, "the run ended on the game-over panel");
    printf("  %d banking(s) seen\n", banks);
}

static void mode_coop(int max_frames)
{
    int st, seen = 0, bad = 0;
    for (int f = 0; f < max_frames; f++) {
        frame(0, 0);
        const world *w = dp_test_world(&st);
        if (st == DS_PLAY && w->players >= 2 && w->d[0].state == DK_ALIVE && w->d[1].state == DK_ALIVE) {
            seen++;
            if (digits_at(0, 100, 220) != world_total(w)) bad++;
            if (seen == 400) shot(w->players == 4 ? "play-4-players" : "coop");
        }
        if (st == DS_OVER) {
            frames(45);
            int fam = 8 + 3 * w->players + 4;           /* the family panel under the ranking */
            CHECK(bg1_box_is(10, 10, "1ST"), "the ranking: 1ST on row 10");
            CHECK(bg1_box_is(10, fam + 1, "FAMILY"), "the family panel (row %d)", fam + 1);
            CHECK(digits_at((fam + 1) * 8 - 4, 150, 260) == world_total(w), "FAMILY shows the family score (%d)", world_total(w));
            int heads = sprite_range(SPR_DUCK_HEAD, 2, 3);
            CHECK(heads == w->players, "each parent's head by its place (%d)", heads);
            shot(w->players == 4 ? "results-4-players" : "coop-gameover");
            break;
        }
    }
    CHECK(seen > 200 && bad == 0, "the family score in the HUD (%d frames, %d wrong)", seen, bad);
    CHECK(sprite_range(SPR_DUCK_RIGHT, 4, -1) >= 0, "Father Duck drawn");
}

int main(int argc, char **argv)
{
    const char *mode = "title";
    int max_frames = 30000;
    rs_host_set_log(quiet_log);
    rs_host_set_option("music", "0");
    rs_host_set_option("sound", "0");
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--mode")) mode = argv[i + 1];
        else if (!strcmp(argv[i], "--out")) out_dir = argv[i + 1];
        else if (!strcmp(argv[i], "--frames")) max_frames = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--opt")) {
            char kv[128];
            snprintf(kv, sizeof kv, "%s", argv[i + 1]);
            char *eq = strchr(kv, '=');
            if (eq) { *eq = 0; rs_host_set_option(kv, eq + 1); }
        }
    }
    rs_host_init(rs_game_main());
    if (!strcmp(mode, "title")) mode_title();
    else if (!strcmp(mode, "run")) mode_run(max_frames);
    else mode_coop(max_frames);
    rs_host_shutdown();
    printf("%s %s: %d checks, %d failed\n", fails ? "FAILED" : "all passed", mode, checks, fails);
    return fails != 0;
}
