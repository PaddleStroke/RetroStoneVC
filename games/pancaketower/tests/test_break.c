/*
 * Pancake Tower: the breakthroughs, frame by frame. The picture agrees with the world while the camera scrolls
 * through the kitchen ceiling and the roof (and shakes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 *
 * The whole game runs (runtime, draw, the screen-reading bot), run after run. After every frame, for each view:
 *   - the near scenery (BG3 rendered alone) is the art, pixel for pixel, on every line of the view: no stray or
 *     misaligned tile anywhere (a streamed row at the wrong height, a carved tile left behind), except in a hole;
 *   - in the rows of a hole the tower broke (the ceiling 152..192, the roof 288..336): inside the hole's extent the
 *     hole's colour, around it only the broken rim (at most JAG px wider), the art beyond;
 *   - the hole is exactly the tower's width: the union of the layers in those rows (the one that broke through and
 *     the ones that followed), and the tower (BG2 rendered alone) has no pixel outside it in those rows; each pancake
 *     row baked there spans exactly its layer (no sway inside a hole);
 *   - the layers are drawn where they belong, whatever the shake.
 *
 *   test_break [--frames N] [--opt key=value]...
 */
#include "pt.h"
#include "rs_host.h"
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const rs_game *rs_game_main(void);

#define SW RS_SCREEN_W
#define SH RS_SCREEN_H
#define JAG 4                       /* the rim: at most 3 px of ragged edge and its 1-px outline */

static uint16_t buf0[SW * SH], buf2[SW * SH], buf3[SW * SH];
static int fails, frames_checked, hole_frames, shake_frames, rows_checked, layers_checked, breaks_seen[2];

static void quiet_log(const char *line) { (void)line; }

static uint16_t to565(rs_color c)
{
    unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    return (uint16_t)((r << 11) | (((g << 1) | (g >> 4)) << 5) | b);
}

static void render(int bgmask, uint16_t *out)
{
    static uint8_t flags[RS_OAM_MAX];
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        flags[i] = s->flags;
        s->flags |= RS_SPR_HIDE;
    }
    for (int l = 0; l < 4; l++) rs_bg_enable(l, (bgmask >> l) & 1);
    rs_host_render();
    memcpy(out, rs_host_framebuffer(), sizeof buf0);
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    for (int i = 0; i < RS_OAM_MAX; i++) rs_oam(i)->flags = flags[i];
}

static void fail(int frame, const char *fmt, int a, int b, int c, int d)
{
    fails++;
    if (fails <= 16) {
        printf("  FAIL frame %d: ", frame);
        printf(fmt, a, b, c, d);
        printf("\n");
    }
}

/* the art's pixel at panorama x X, world y wy: its colour (RGB565), or -1 = transparent */
static int scenery(int X, int wy)
{
    int k = wy >= 0 ? wy / 8 : (wy - 7) / 8, y = 7 - (wy - k * 8);
    int seg = (k * 8 - SEG_BASE_Y) >> 8, row = 31 - ((k + 8) & 31);
    if (seg < 0) return -1;
    if (seg > NEAR_SEGS - 1) seg = NEAR_SEGS - 1;
    uint16_t e = pt_near_map[(seg * NEAR_H + row) * NEAR_W + X / 8];
    const uint8_t *t = pt_near_tiles + RS_MAP_TILE(e) * RS_TILE_BYTES;
    int x = X & 7, sx = (e & RS_MAP_HFLIP) ? 7 - x : x, sy = (e & RS_MAP_VFLIP) ? 7 - y : y;
    uint8_t b = t[sy * 4 + sx / 2];
    int idx = (sx & 1) ? b & 15 : b >> 4;
    return idx ? to565(rs_pal_get(RS_PAL_BG(RS_MAP_PAL(e)) + idx)) : -1;
}

static const int HY0[2] = {CEILING_Y, ROOF_Y}, HY1[2] = {CEILING_TOP, ROOF_TOP};

static void check_view(const match *m, int p, int frame)
{
    pt_view_info v;
    draw_test_view(p, &v);
    const tower *tw = &m->tw[p];
    uint16_t hole565 = to565(RS_RGB8(64, 42, 36)), rim565 = to565(RS_RGB8(112, 66, 36));
    int any_hole = 0;
    /* the holes are the tower's width: the union of its layers in the hole's rows */
    for (int h = 0; h < 2; h++) {
        int i0 = HY0[h] / ROW_H, i1 = HY1[h] / ROW_H, lo = 1 << 20, hi = -(1 << 20), known = 1;
        for (int i = i0; i < i1 && i < tw->nlayers; i++) {
            if (i < tw->nlayers - RING) { known = 0; break; }
            const layer *l = tower_layer(tw, i);
            if (l->x < lo) lo = l->x;
            if (l->x + l->w > hi) hi = l->x + l->w;
        }
        int broken = tw->nlayers > i0;
        if (broken != v.hole_on[h]) fail(frame, "player %d hole %d open %d, the tower has %d layers", p, h, v.hole_on[h], tw->nlayers);
        if (broken && known && (v.hx0[h] != lo || v.hx1[h] != hi))
            fail(frame, "hole %d is [%d, %d) but the tower there is [%d, ...)", h, v.hx0[h], v.hx1[h], lo);
        if (broken) breaks_seen[h] = 1;
    }
    for (int l = 0; l < v.h; l++) {
        int sy = v.y + l, wy = v.cam + v.h - 1 - (l - v.shy);
        if (sy < 0 || sy >= SH) continue;
        int hole = -1;
        for (int h = 0; h < 2; h++)
            if (v.hole_on[h] && wy >= HY0[h] && wy < HY1[h]) hole = h;
        int ax0 = 0, ax1 = 0, bad3 = 0, bad2 = 0;
        if (hole >= 0) ax0 = 160 + v.hx0[hole], ax1 = 160 + v.hx1[hole], any_hole = 1, rows_checked++;
        int left2 = 1 << 20, right2 = -(1 << 20);
        for (int i = 0; i < v.w; i++) {
            int sx = v.x + i, X = 160 - v.sx0 + i - v.shx;
            if (sx < 0 || sx >= SW) continue;
            int o = sy * SW + sx;
            if (buf2[o] != buf0[o]) { if (X < left2) left2 = X; if (X > right2) right2 = X; }
            if (X < v.pcol0 * 8 || X >= (v.pcol0 + v.ncols) * 8) continue;
            int art = scenery(X, wy);
            uint16_t want = art < 0 ? buf0[o] : (uint16_t)art;
            if (hole >= 0 && X >= ax0 && X < ax1) {
                if (buf3[o] != hole565) bad3 = X + 1;
            } else if (hole >= 0 && X >= ax0 - JAG && X < ax1 + JAG) {
                if (buf3[o] != want && buf3[o] != hole565 && buf3[o] != rim565) bad3 = X + 1;
            } else if (buf3[o] != want) {
                bad3 = X + 1;
            }
            if (hole >= 0 && buf2[o] != buf0[o] && (X < ax0 || X >= ax1)) bad2 = X + 1;
        }
        if (bad3) fail(frame, "player %d, world y %d (line %d): the scenery is wrong at panorama x %d", p, wy, sy, bad3 - 1);
        if (bad2) fail(frame, "player %d, world y %d: the tower sticks out of the hole at panorama x %d%.0d", p, wy, bad2 - 1, 0);
        /* a pancake baked in a hole's rows spans exactly its layer on its middle row */
        int i = wy / ROW_H;
        if (hole >= 0 && wy % ROW_H == 3 && i < tw->nlayers - 1 && i >= tw->nlayers - RING) {
            const layer *ly = tower_layer(tw, i);
            if (ly->kind == LK_PANCAKE) {
                layers_checked++;
                if (left2 != 160 + ly->x || right2 != 160 + ly->x + ly->w - 1)
                    fail(frame, "layer %d drawn over [%d, %d], its extent starts at %d", i, left2 - 160, right2 - 160, ly->x);
            }
        }
    }
    if (any_hole) {
        hole_frames++;
        if (v.shx || v.shy) shake_frames++;
    }
}

int main(int argc, char **argv)
{
    int frames = 6000;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--opt") && i + 1 < argc) {
            char kv[128];
            snprintf(kv, sizeof kv, "%s", argv[++i]);
            char *eq = strchr(kv, '=');
            if (!eq) { fprintf(stderr, "--opt needs key=value\n"); return 2; }
            *eq = 0;
            rs_host_set_option(kv, eq + 1);
        } else {
            fprintf(stderr, "usage: test_break [--frames N] [--opt key=value]...\n");
            return 2;
        }
    }
    rs_host_set_log(quiet_log);
    rs_host_set_option("music", "0");
    rs_host_set_option("sound", "0");
    rs_host_set_option("botruns", "1000");
    rs_host_init(rs_game_main());
    int runs = 0, last = -1;
    for (int f = 0; f < frames; f++) {
        rs_host_frame();
        int st;
        const match *m = pt_test_match(&st);
        if (st == 3 && last != 3) runs++;
        last = st;
        if ((st == DS_TITLE || st == DS_OVER) && m->players > 1) continue; /* menu overlays cover the play views */
        render(0x0, buf0);
        render(0x2, buf2);
        render(0x4, buf3);
        frames_checked++;
        for (int p = 0; p < m->players; p++) check_view(m, p, f);
    }
    rs_host_shutdown();
    printf("  %s %d frames, %d runs: %d with a hole in view (%d shaking), %d hole rows, %d layers in a hole; "
           "broke the ceiling %s, the roof %s\n", fails ? "FAIL" : "ok  ", frames_checked, runs, hole_frames, shake_frames,
           rows_checked, layers_checked, breaks_seen[0] ? "yes" : "NO", breaks_seen[1] ? "yes" : "NO");
    if (!breaks_seen[0] || !breaks_seen[1]) { printf("  FAIL the run did not break both\n"); fails++; }
    if (hole_frames < 100 || shake_frames < 10) { printf("  FAIL too few frames with a hole in view\n"); fails++; }
    return fails ? 1 : 0;
}
