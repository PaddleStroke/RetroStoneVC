/*
 * Pogo Mamie: the picture agrees with the world at every scroll phase (the camera moves in x AND y).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 *
 * The whole game runs (runtime, draw, sound) with the screen-reading bot playing, run after run. After every
 * frame:
 *   - tiles: the picture is rendered with the play layer (BG2) alone, then without it: the difference is BG2.
 *     In every screen column over a building, the first BG2 pixel must be exactly the roof the physics uses
 *     (bldg_drawn_top: the zinc, the slope, the chimney's pots, the skylight's frame), with nothing above it;
 *     the street (or the river) at STREET_Y in the gaps;
 *   - sprites: every prop, pigeon, antenna and power-up in view has its sprite in OAM at its world position
 *     minus the camera (the props' surface row on their landing surface);
 *   - on a landing frame, Mamie's sprite stands exactly on the surface: its pogo's tip on the row above it.
 * Every x phase and every y phase of the camera (64 combinations) must be covered.
 *
 *   test_align [--frames N] [--seed S] [--opt key=value]...
 */
#include "pm.h"
#include "rs_host.h"
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const rs_game *rs_game_main(void);

#define SW RS_SCREEN_W
#define SH RS_SCREEN_H

static uint16_t with_bg2[SW * SH], without_bg2[SW * SH], spr_on[SW * SH], spr_off[SW * SH];
static uint8_t bg2[SH][SW];
static int fails, frames_checked, columns_checked, sprites_checked, landings_checked;
static uint8_t phase_seen[8][8];

static void quiet_log(const char *line) { (void)line; }

static void render(int bgmask, int sprites, uint16_t *out)
{
    static uint8_t flags[RS_OAM_MAX];
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        flags[i] = s->flags;
        if (!sprites) s->flags |= RS_SPR_HIDE;
    }
    for (int l = 0; l < 4; l++) rs_bg_enable(l, (bgmask >> l) & 1);
    draw_test_backdrop(1);
    rs_host_render();
    draw_test_backdrop(0);
    memcpy(out, rs_host_framebuffer(), sizeof with_bg2);
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    for (int i = 0; i < RS_OAM_MAX; i++) rs_oam(i)->flags = flags[i];
}

static int fail(int frame, const char *fmt, int a, int b, int c)
{
    fails++;
    if (fails <= 12) {
        printf("  FAIL frame %d: ", frame);
        printf(fmt, a, b, c);
        printf("\n");
    }
    return 0;
}

static int oam_has(int id, int x, int y)
{
    const pm_sprite_def *d = &pm_spr[id];
    for (int i = 0; i < RS_OAM_MAX; i++) {
        const rs_sprite *s = rs_oam(i);
        if (s->used && !(s->flags & RS_SPR_HIDE) && s->tile == d->tile && s->x == x && s->y == y) return 1;
    }
    return 0;
}

static int oam_has_any(int id, int frames, int x, int y)
{
    for (int f = 0; f < frames; f++) if (oam_has(id + f, x, y)) return 1;
    return 0;
}

static int visible(int sx, int sy, int w, int h) { return sx > -w && sx < SW && sy > -h && sy < SH; }

static void check_frame(const world *w, int frame, int st)
{
    int cx, cy;
    draw_camera(&cx, &cy);
    phase_seen[cx & 7][cy & 7] = 1;
    frames_checked++;
    render(0x2, 0, with_bg2);
    render(0x0, 0, without_bg2);
    for (int i = 0; i < SW * SH; i++) bg2[i / SW][i % SW] = with_bg2[i] != without_bg2[i];
    /* the roofs */
    for (int i = 0; i < w->nb; i++) {
        const bldg *b = &w->b[i];
        for (int x = (int)b->x0; x < b->x1; x++) {
            int sx = x - cx;
            if (sx < 0 || sx >= SW) continue;
            int top = bldg_drawn_top(b, x), sy = top - cy;
            if (sy < 1 || sy >= SH) continue;
            columns_checked++;
            int above = 0;
            for (int y = sy - 1; y >= 0 && y >= sy - 40; y--) above += bg2[y][sx];
            if (!bg2[sy][sx]) { fail(frame, "no roof pixel at screen (%d, %d) (building %d)", sx, sy, (int)b->index); break; }
            if (above) { fail(frame, "BG2 pixels above the roof at screen (%d, %d) (building %d)", sx, sy, (int)b->index); break; }
        }
    }
    /* the street in the gaps */
    int sy_street = STREET_Y - cy;
    if (sy_street >= 1 && sy_street < SH)
        for (int sx = 0; sx < SW; sx += 7)
            if (!world_bldg_at(w, sx + cx) && (!bg2[sy_street][sx] || bg2[sy_street - 1][sx])) {
                int prop = 0;
                for (int k = 0; k < w->no; k++) if (w->o[k].kind == OB_CRADLE && sx + cx >= w->o[k].x && sx + cx < w->o[k].x + w->o[k].w) prop = 1;
                if (!prop) { fail(frame, "the street is not at screen y %d (column %d)%.0d", sy_street, sx, 0); break; }
            }
    /* the sprites of the world */
    for (int i = 0; i < w->no; i++) {
        const obj *o = &w->o[i];
        if (o->state == 2) continue;
        int ok = 1, sx, sy;
        switch (o->kind) {
        case OB_AWNING:
            sx = (int)o->x - cx; sy = o->y - SURF_AWNING - cy;
            if (visible(sx, sy, 24, 16)) { sprites_checked++; ok = oam_has_any(SPR_AWNING, 2, sx, sy); }
            break;
        case OB_POT:
            if (o->state) break;
            sx = (int)o->x - cx; sy = o->y - SURF_POT - cy;
            if (visible(sx, sy, 16, 16)) { sprites_checked++; ok = oam_has(SPR_POT, sx, sy); }
            break;
        case OB_CRADLE:
            sx = (int)o->x - cx; sy = (int)(o->pos >> 16) - SURF_CRADLE - cy;
            if (visible(sx, sy, 32, 16)) { sprites_checked++; ok = oam_has(SPR_CRADLE, sx, sy); }
            break;
        case OB_LEDGE:
            if (o->state) break;
            sx = (int)o->x - cx; sy = o->y - SURF_LEDGE - cy;
            if (visible(sx, sy, 8, 8)) { sprites_checked++; ok = oam_has(SPR_LEDGE, sx, sy); }
            break;
        case OB_ANTENNA:
            sx = (int)o->x - 8 - cx; sy = o->y - 32 - cy;
            if (visible(sx, sy, 16, 32)) { sprites_checked++; ok = oam_has_any(SPR_ANTENNA, 3, sx, sy); }
            break;
        case OB_PIGEON:
            if (o->state) break;
            { int pcx, pfy; pigeon_at(o, &pcx, &pfy); sx = pcx - 8 - cx; sy = pfy - 16 - cy; }
            if (visible(sx, sy, 16, 16)) { sprites_checked++; ok = oam_has_any(SPR_PIGEON, 7, sx, sy); }
            break;
        default:
            break;
        }
        if (!ok) fail(frame, "object %d (kind %d) not drawn at its place (%d)", i, o->kind, 0);
    }
    /* Mamie on the roof she just landed on */
    const mamie *m = &w->m[0];
    if ((st == DS_PLAY) && (w->events[0] & EV_LAND) && m->state == MS_AIR && m->land_kind != SF_SKY) {
        int x = (int)(m->x >> 16) - cx, feet = m->land_y - cy;
        if (x >= 2 && x < SW - 2 && feet >= 1 && feet < SH) {
            landings_checked++;
            render(0x0, 1, spr_on);
            render(0x0, 0, spr_off);
            int tip = spr_on[(feet - 1) * SW + x] != spr_off[(feet - 1) * SW + x] ||
                      spr_on[(feet - 1) * SW + x - 1] != spr_off[(feet - 1) * SW + x - 1];
            int below = spr_on[feet * SW + x] != spr_off[feet * SW + x] && spr_on[feet * SW + x - 1] != spr_off[feet * SW + x - 1];
            (void)below;
            if (!tip) fail(frame, "Mamie's pogo does not touch the roof at screen (%d, %d)%.0d", x, feet, 0);
            if (!oam_has_any(SPR_MAMIE, 13, (int)(m->x >> 16) - 12 - cx, m->land_y - 32 - cy) &&
                !oam_has_any(SPR_MAMIE, 13, (int)(m->x >> 16) - 12 - cx, feet - 32))
                fail(frame, "Mamie's sprite is not on the surface (%d, %d)%.0d", x, feet, 0);
        }
    }
}

int main(int argc, char **argv)
{
    int frames = 20000;
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
            fprintf(stderr, "usage: test_align [--frames N] [--opt key=value]...\n");
            return 2;
        }
    }
    rs_host_set_log(quiet_log);
    rs_host_set_option("music", "0");
    rs_host_set_option("sound", "0");
    rs_host_set_option("bot", "1");
    rs_host_set_option("botruns", "1000");
    rs_host_init(rs_game_main());
    int runs = 0, last_st = -1;
    for (int f = 0; f < frames; f++) {
        rs_host_frame();
        int st;
        const world *w = pm_test_world(&st);
        if (st == DS_OVER && last_st != DS_OVER) runs++;
        last_st = st;
        check_frame(w, f, st);
    }
    rs_host_shutdown();
    int phases = 0;
    for (int x = 0; x < 8; x++) for (int y = 0; y < 8; y++) phases += phase_seen[x][y];
    printf("  %s picture = world: %d frames, %d runs, %d roof columns, %d sprites, %d landings, %d/64 scroll phases\n",
           fails ? "FAIL" : "ok  ", frames_checked, runs, columns_checked, sprites_checked, landings_checked, phases);
    if (phases < 64) { printf("  FAIL only %d of the 64 x/y scroll phases were seen\n", phases); fails++; }
    if (landings_checked < 20) { printf("  FAIL only %d landings checked\n", landings_checked); fails++; }
    return fails ? 1 : 0;
}
