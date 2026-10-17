/*
 * Kitty Kibble: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1); BG2 the maze: the walls (each room's furniture, 47 shapes chosen by a
 *   cell's 8 neighbours), the kibble, the dock's gate and floor (rebuilt from the world every frame); BG3 the room
 *   around the maze (the floor, the back wall with its windows, the furniture seen from above); BG2 also has the
 *   pet doors where the tunnels leave the maze.
 *   Sprites: the kittens (OBJ 0; players 2-4 OBJ 1, 4, 5: the fur and the collar recoloured), the props: yarn, the
 *   fish treat, points, dust, a caught vacuum's eyes (OBJ 2), the kit's digits, glyphs and medals (OBJ 3), the robot
 *   vacuums (OBJ 6; their flash at the end of yarn time OBJ 7). Window 1 hides the sprites outside the maze's
 *   columns on its lines (the actors go out through the pet doors), set per line by the raster callback.
 * Widescreen (docs/spec.md "Widescreen"): the 320-px stage is centred (rs_screen_origin); the maze stays the same
 * and a wider screen shows more of the room around it (BG3 is a stage panorama from x -128 to 447).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word (light, mid, dark): KITTY in ginger, KIBBLE in biscuit brown */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 214, 150), RS_RGB8(246, 160, 72), RS_RGB8(206, 104, 42)},
    {RS_RGB8(250, 214, 150), RS_RGB8(214, 146, 72), RS_RGB8(160, 96, 46)}};
#define MAZE_COL0   6           /* the maze's first map column (MAZE_X = 6 * 8 + 4: the layer scrolls by 4) */
#define MAZE_ROW0   3           /* its first map row (MAZE_Y = 3 * 8 + 4) */
#define TITLE_KIT_Y 92
#define BOTTOM_CHIPS 222        /* 3-4 players: the bottom chips' digits, under the maze */

static int shown_key = -1, shown_best = -1, shown_room = -1;
static int win_on;              /* the sprite window (in play) */

/* the raster callback: window 1 = the maze's columns on its lines (sprites outside it are hidden: the tunnels), the
 * whole screen elsewhere (the score chips) */
static void raster(int line, void *user)
{
    (void)user;
    if (win_on && line >= MAZE_Y && line < MAZE_Y + MAZE_H * CELL) rs_window(0, MAZE_X, MAZE_X + MAZE_W * CELL);
    else rs_window(0, rs_screen_left(), rs_screen_right());
}

/* ---- the maps -------------------------------------------------------------------------------------------------------
 * BG3 is a "stage panorama": PANO_W tiles from stage x PANO_X0 (-128), so every width up to 560 shows it once: map
 * column c holds stage tile c (0..), the last columns the stage's left margin (negative x), as the BG wraps. Maps of
 * 64 tiles up to 512 columns, 128 beyond (the map size is saved in states: draw_state_loaded sets them up again). */
static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }

static void put_pano(int layer, const uint16_t *pano, int mw)
{
    int lim = mw == 64 ? 52 : 56;           /* stage tiles 0..51 (55) on the right, the rest is the left margin */
    for (int c = 0; c < mw; c++) {
        int st = c < lim ? c : c - mw, i = st - PANO_X0 / 8;
        for (int y = 0; y < PANO_H; y++)
            rs_bg_put(layer, c, y, i >= 0 && i < PANO_W ? pano[y * PANO_W + i] : 0);
    }
}

/* a room's art: its walls' tiles and colours (BG2), the room around the maze (BG3) */
static void load_room(int maze)
{
    rs_tiles_load(VR_BG2 + WALL_TILE0, gm_wall_tiles + (size_t)maze * WALL_SHAPES * RS_TILE_BYTES, WALL_SHAPES);
    rs_pal_load(RS_PAL_BG(PAL_WALL), gm_wall_pals + maze * 16, 16);
    rs_tiles_load(VR_BG3, gm_room_tiles[maze], gm_room_tile_count[maze]);
    rs_pal_load(RS_PAL_BG(PAL_ROOM), gm_room_pals + maze * 64, 64);
    put_pano(RS_BG3, gm_room_map[maze], map_w());
    shown_room = maze;
}

static void maps_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, mw, 32, VR_BG2);
    rs_bg_setup(RS_BG3, mw, 32, VR_BG3);
    if (shown_room >= 0) put_pano(RS_BG3, gm_room_map[shown_room], mw);
}

void draw_init(void)
{
    static const uint8_t blank[RS_TILE_BYTES];
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more of the room on both sides */
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;         /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hc.logo_pal = PAL_LOGO;             /* BG 7: the room around the maze takes BG 3-6 */
    hu_init(&hc);
    rs_tiles_load(VR_BG2, blank, 1);
    rs_tiles_load(VR_BG2 + ITEM_TILE, gm_item_tiles, ITEM_TILES);
    rs_pal_load(RS_PAL_BG(PAL_ITEMS), gm_item_pal, 16);
    maps_setup();
    load_room(0);
    for (int l = 0; l < 3; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* the kitten (ginger), P2 (silver), the props */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);     /* P3 (white), P4 (chocolate), the vacuums, their flash */
    rs_backdrop(RS_RGB8(40, 30, 40));
    rs_raster(raster, NULL);
}

/* ---- BG2: the maze, from the world ----------------------------------------------------------------------------------- */
static int is_wall(const world *w, int x, int y)
{
    if (x < 0 || x >= MAZE_W || y < 0 || y >= MAZE_H) return 0;      /* outside: open (the maze's outer edge) */
    return w->cell[y][x] == C_WALL;
}

static void maze_put(const world *w, int show)
{
    static const int nb[8][3] = {{NB_N, 0, -1}, {NB_NE, 1, -1}, {NB_E, 1, 0}, {NB_SE, 1, 1},
                                 {NB_S, 0, 1}, {NB_SW, -1, 1}, {NB_W, -1, 0}, {NB_NW, -1, -1}};
    for (int y = 0; y < MAZE_H; y++)
        for (int x = 0; x < MAZE_W; x++) {
            uint16_t e = 0;
            if (show) {
                switch (w->cell[y][x]) {
                case C_WALL: {
                    int m = 0;
                    for (int k = 0; k < 8; k++)
                        if (is_wall(w, x + nb[k][1], y + nb[k][2])) m |= nb[k][0];
                    e = (uint16_t)((WALL_TILE0 + gm_wall_lut[m]) | (PAL_WALL << 10));
                    break;
                }
                case C_KIBBLE: e = (uint16_t)((TILE_KIBBLE + ((x + y) & 1)) | (PAL_ITEMS << 10)); break;
                case C_GATE: e = (uint16_t)(TILE_GATE | (PAL_ITEMS << 10)); break;
                case C_DOCK: e = (uint16_t)(TILE_DOCK | (PAL_ITEMS << 10)); break;
                default: break;
                }
            }
            rs_bg_put(RS_BG2, MAZE_COL0 + x, MAZE_ROW0 + y, e);
        }
    /* the pet doors where the tunnels leave the maze (8x24: the row above, the row, the row below) */
    for (int y = 0; y < MAZE_H; y++) {
        rs_bg_put(RS_BG2, MAZE_COL0 - 1, MAZE_ROW0 + y, 0);
        rs_bg_put(RS_BG2, MAZE_COL0 + MAZE_W, MAZE_ROW0 + y, 0);
    }
    for (int y = 1; show && y < MAZE_H - 1; y++)
        if (w->cell[y][0] == C_TUNNEL)
            for (int t = 0; t < 3; t++) {
                uint16_t e = (uint16_t)((TILE_DOOR + t) | (PAL_ITEMS << 10));
                rs_bg_put(RS_BG2, MAZE_COL0 - 1, MAZE_ROW0 + y - 1 + t, e);
                rs_bg_put(RS_BG2, MAZE_COL0 + MAZE_W, MAZE_ROW0 + y - 1 + t, (uint16_t)(e | RS_MAP_HFLIP));
            }
}

/* the room's walls flash when it is clean */
static void wall_palette(const world *w)
{
    int flash = w->phase == PH_CLEAR && w->phase_t < CLEAR_FRAMES - 24 && (w->phase_t / 10) % 2;
    for (int i = 1; i < 8; i++) {
        rs_color c = gm_wall_pals[w->maze * 16 + i];
        rs_pal_set(RS_PAL_BG(PAL_WALL) + i, flash ? hu_lerp_color(c, RS_RGB8(255, 255, 255), 170) : c);
    }
}

/* ---- sprites -------------------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    x += hu_shake_x();                  /* a catch's shake moves the actors with the maze and the room (BG2, BG3) */
    y += hu_shake_y();
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

/* an actor at maze pixel (x, y) (its centre): a 16x16 sprite, and its other half across a tunnel's wrap */
static void actor(int id, int32_t qx, int32_t qy, int pal, int flags, int dy)
{
    int x = (int)(qx >> 16), y = (int)(qy >> 16), sx = MAZE_X + x - 8, sy = MAZE_Y + y - 8 + dy;
    spr(id, sx, sy, 2, pal, flags);
    if (x < 8) spr(id, sx + MAZE_W * CELL, sy, 2, pal, flags);
    if (x > MAZE_W * CELL - 8) spr(id, sx - MAZE_W * CELL, sy, 2, pal, flags);
}

/* player p's sprite palette: the house rule (P1 OBJ 0, P2 OBJ 1, P3 OBJ 4, P4 OBJ 5: house_ui.h hu_player_pal) */
static int player_pal(int p) { return p ? hu_player_pal(p) : 0; }

static void small_number(int n, int cx, int y)
{
    char s[8];
    int len = snprintf(s, sizeof s, "%d", n);
    for (int i = 0; i < len; i++) spr(SPR_DIGIT + (s[i] - '0'), cx - len * 3 + i * 6 - 1, y, 2, -1, 0);
}

static void draw_kitten(const world *w, int p)
{
    const kitten *k = &w->k[p];
    if (k->state == KS_OUT) return;
    if (k->state == KS_CAUGHT) {                      /* dizzy: it spins, stars round its head, then it is gone */
        if (k->t > CAUGHT_FRAMES - 16 && (k->t / 2) % 2) return;
        actor(SPR_KIT_DIZZY + (w->t / 6) % 2, k->x, k->y, player_pal(p), 0, 0);
        int x = MAZE_X + (int)(k->x >> 16), y = MAZE_Y + (int)(k->y >> 16) - 10;
        static const int ox[8] = {0, 5, 7, 5, 0, -5, -7, -5}, oy[8] = {-3, -2, 0, 2, 3, 2, 0, -2};
        for (int i = 0; i < 3; i++) {
            int a = (w->t / 3 + i * 3) & 7;
            spr(SPR_STAR + (w->t / 8 + i) % 2, x - 4 + ox[a], y - 4 + oy[a], 2, -1, 0);
        }
        return;
    }
    int walk = k->moving ? (int)(((k->x + k->y) >> 16) / 4) % 2 : 0, munch = k->munch_t < 6;
    int id, fl = 0;
    switch (k->face) {
    case DIR_LEFT: fl = RS_SPR_HFLIP; /* fall through */
    case DIR_RIGHT: id = SPR_KIT_SIDE + (munch ? 2 : walk); break;
    case DIR_UP: id = SPR_KIT_UP + walk; break;
    default: id = SPR_KIT_DOWN + (munch ? 2 : walk); break;
    }
    actor(id, k->x, k->y, player_pal(p), fl, 0);
}

static void draw_vacuum(const world *w, int i)
{
    const vacuum *v = &w->v[i];
    if (v->state == VS_HOME || v->state == VS_ENTER) {       /* caught: its eyes zip home */
        actor(SPR_EYES + (v->dir < 0 ? 0 : v->dir), v->x, v->y, -1, 0, 0);
        return;
    }
    int f = (w->t / 6 + i) % 2;
    if (v->scared_t) {
        int pal = 6;
        if (v->scared_t < YARN_FLASH && (v->scared_t / 8) % 2) pal = 7;
        actor(SPR_SCARED + (w->t / 8) % 2, v->x, v->y, pal, 0, 0);
        return;
    }
    static const int base[VACUUMS] = {SPR_DASH, SPR_SNEAK, SPR_PATROL, SPR_WOBBLE};
    int id = base[i], fl = 0, bob = 0;
    if (v->state == VS_DOCK) { id += 2 + (w->t / 16) % 2; bob = (w->t / 20 + i) % 2; }  /* charging, facing us */
    else if (v->dir == DIR_RIGHT || v->dir == DIR_LEFT) { id += f; fl = v->dir == DIR_LEFT ? RS_SPR_HFLIP : 0; }
    else if (v->dir == DIR_UP) id += 4 + f;
    else id += 2 + f;
    actor(id, v->x, v->y, -1, fl, bob);
}

static void draw_world(const world *w)
{
    /* front to back: points and sparkles, the kittens (P1 in front), the vacuums, the yarn and the fish */
    for (int i = 0; i < FX_MAX; i++) {
        const effect *e = &w->fxs[i];
        if (!e->on) continue;
        int x = MAZE_X + e->x, y = MAZE_Y + e->y;
        if (e->kind == FX_POINTS) small_number(e->value, x, y - 12 - e->t / 5);
        else if (e->kind == FX_SPARK && e->t < 20)
            for (int k = 0; k < 4; k++)
                spr(SPR_SPARKLE + (e->t / 5 + k) % 2, x - 4 + (k & 1 ? 1 : -1) * (3 + e->t / 2),
                    y - 4 + (k & 2 ? 1 : -1) * (3 + e->t / 2), 2, -1, 0);
        else if (e->kind == FX_DUST) spr(SPR_DUST + (e->t >= 12), x - 8, y - 8 - e->t / 4, 2, -1, 0);
    }
    for (int p = 0; p < w->players; p++) draw_kitten(w, p);
    for (int i = 0; i < VACUUMS; i++) draw_vacuum(w, i);
    for (int y = 0; y < MAZE_H; y++)
        for (int x = 0; x < MAZE_W; x++)
            if (w->cell[y][x] == C_YARN)
                spr(SPR_YARN + (w->t / 16 + x) % 2, MAZE_X + x * CELL + 4 - 8, MAZE_Y + y * CELL + 4 - 8, 2, -1, 0);
    if (w->fish_on && (w->fish_t > 120 || (w->fish_t / 6) % 2))
        spr(SPR_FISH + (w->t / 12) % 2, MAZE_X + w->fish_x * CELL + 4 - 8,
            MAZE_Y + w->fish_y * CELL + 4 - 8 + hu_bob(w->t, 48, 1), 2, -1, 0);
}

/* the title's player slots: each joined player's icon, in its colours (hu_title_sprites calls it) */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(SPR_KIT_ICON, cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 2, player_pal(p), 0);
}

/* 2-4 players: ranked by score */
static void standing(const world *w, hu_standing *s)
{
    int v[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) v[p] = w->k[p].score;
    hu_rank(s, w->players, v, v);
}

/* ---- the BG screens -------------------------------------------------------------------------------------------- */
static const char *room_name(int maze)
{
    switch (maze) {
    case 0: return TR(ROOM_KITCHEN);
    case 1: return TR(ROOM_LIVING);
    case 2: return TR(ROOM_BEDROOM);
    case 3: return TR(ROOM_GARDEN);
    default: return TR(ROOM_ATTIC);
    }
}

/* P2 CAUGHT! on a little panel in the band above the maze (the back wall: readable over any room's windows and
 * pictures, between the 2 players' scores and the 3-4 players' corner chips); a text with accents or CJK glyphs
 * takes one more row (its marks need the panel's fill above them) */
static void caught_panel(const char *s)
{
    int tall = hu_tall(s), w = hu_width(s, 0) + 4;
    if (w > 38) w = 38;
    hu_panel((40 - w) / 2, 0, w, 3 + tall);
    hu_box_text(hu_center(s, 0), 1 + tall, s);
}

/* the room's name (and number) or ROOM CLEAR! on a little panel over the dock: gold 2x letters, small ones if long */
static void room_panel(const char *small, const char *big)
{
    int bigw = hu_width(big, 1), fits = bigw <= 34, w = fits ? bigw : hu_width(big, 0);
    if (small && hu_width(small, 0) > w) w = hu_width(small, 0);
    w += 4;
    if (w > 38) w = 38;
    int top = small ? 10 : 12, row = small ? 14 : 14;
    hu_panel((40 - w) / 2, top, w, small ? 8 : 5);
    if (small) hu_box_text(hu_center(small, 0), 12, small);
    if (fits) hu_big(hu_center(big, 1), row, big, HU_BIG_BANNER);
    else hu_box_text(hu_center(big, 0), row, big);
}

static int screen_key(const world *w, int state, int st_t)
{
    int screen = state == ST_DEAD ? ST_PLAY : state;
    int key = screen * 100000;
    if (screen == ST_PLAY) {
        if (w->phase == PH_INTRO || w->phase == PH_READY) key += 1000 + w->room;
        if (w->phase == PH_CLEAR) key += 2000 + w->room;
        if (w->msg) key += 10000 + w->msg * 10 + w->msg_player;
    }
    if (state == ST_OVER && st_t >= RETRY_LOCK) key += 1;
    return key;
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    int key = screen_key(w, state, st_t);
    if (key != shown_key || best != shown_best) {
        char s[64];
        hu_clear();
        if (state == ST_TITLE) hu_logo("KITTY KIBBLE", title_ramps, 2, 2, 0);
        if ((state == ST_PLAY || state == ST_DEAD) && w->players >= 3) hu_score_tags(w->players, BOTTOM_CHIPS);
        if (state == ST_PLAY || state == ST_DEAD) {
            if (w->phase == PH_INTRO || w->phase == PH_READY) {
                rs_strf(s, sizeof s, TR(ROOM_N), w->room);
                room_panel(s, room_name(w->maze));
            } else if (w->phase == PH_CLEAR) {
                room_panel(NULL, TR(CLEAR));
            }
            if (w->msg == MSG_CAUGHT) {
                char tag[16];
                rs_strf(tag, sizeof tag, HTR(PLAYER_TAG), w->msg_player + 1);
                rs_strf(s, sizeof s, TR(CAUGHT), tag);
                caught_panel(s);
            }
        }
        if (state == ST_OVER) {
            if (w->players == 1) {
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->k[0].score, 0);
                rs_strf(s, sizeof s, TR(ROOM_N), w->room);
                hu_gameover_note(s);
            } else {
                hu_results_panel(rs, NULL);            /* P2 WINS! and the ranking */
            }
        }
        shown_key = key;
        shown_best = best;
    }
    if (state == ST_TITLE) hu_title_draw(st_t, best);
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

/* the title: the kitten bobbing under the logo with a ball of yarn, the four vacuums gliding by below */
static void title_sprites(int t)
{
    int b = hu_bob(t, 64, READY_BOB);
    spr(SPR_KIT_DOWN + (t / 32) % 2, 160 - 16, TITLE_KIT_Y + b, 2, 0, 0);
    spr(SPR_YARN + (t / 16) % 2, 160 + 2, TITLE_KIT_Y + 2, 2, -1, 0);
    int span = rs_screen_right() - rs_screen_left() + 120, x0 = rs_screen_left() - 60;
    static const int base[VACUUMS] = {SPR_DASH, SPR_SNEAK, SPR_PATROL, SPR_WOBBLE};
    int lap = (t / 2) % (2 * span), dir = lap < span ? 1 : -1, pos = lap < span ? lap : 2 * span - lap;
    for (int i = 0; i < VACUUMS; i++) {
        int x = x0 + pos - dir * i * 22;
        spr(base[i] + (t / 6 + i) % 2, x, TITLE_KIT_Y + 26, 2, -1, dir < 0 ? RS_SPR_HFLIP : 0);
    }
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    static const int medals[4] = MEDAL_SCORES;
    int sx = hu_shake_x(), sy = hu_shake_y();
    hu_standing rs;
    standing(w, &rs);
    if (w->maze != shown_room) load_room(w->maze);
    int pm = map_w() * 8 - 1;
    rs_bg_scroll(RS_BG3, (-sx) & pm, (-sy) & 255);
    rs_bg_scroll(RS_BG2, (-(MAZE_X - MAZE_COL0 * 8) - sx) & pm, (-(MAZE_Y - MAZE_ROW0 * 8) - sy) & 255);
    maze_put(w, state != ST_TITLE);
    wall_palette(w);

    screen_text(w, state, st_t, best, new_best, &rs);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;     /* the panel slides up, ease-out */
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);

    win_on = state != ST_TITLE;
    rs_obj_window(win_on ? RS_WIN1_OUT : 0);
    rs_oam_clear();
    /* front to back: the UI, the play */
    if (state == ST_PLAY || state == ST_DEAD) {
        int v[MAX_PLAYERS];
        for (int p = 0; p < w->players; p++) v[p] = w->k[p].score;
        hu_score_chips(w->players, v, BOTTOM_CHIPS);   /* 1: centred; 2: x 80 / 240; 3-4: the corners */
    }
    if (state == ST_OVER) {
        if (w->players == 1) {
            hu_gameover_sprites(1, w->k[0].score, 0, best, hu_medal_of(w->k[0].score, medals), st_t, slide);
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_KIT_ICON, cx - 8, cy - 8 + slide, 3, player_pal(rs.order[i]), 0);
            }
        }
    }
    if (state == ST_TITLE) {
        hu_title_sprites(st_t, slot_icon, NULL);   /* the prompt's glyph, the joined players popping in */
        title_sprites(st_t);
    } else if (state != ST_OVER) {
        draw_world(w);
    }
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings maps of another size: set them up for this width, redraw the UI */
void draw_state_loaded(void)
{
    if (rs_bg_map_w(RS_BG2) == map_w()) return;
    maps_setup();
    shown_key = -1;
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(shown_key); S(shown_best); S(shown_room); S(win_on);
    RS_STATE_RASTER(raster);
}
#undef S
