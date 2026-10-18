/*
 * Beaver Rush: video. The trunk (BG2, redrawn on each gnaw; the drop is a vertical scroll), the beavers, the
 * effects (the tumbling log, chips, splashes, logs floating to the dam), the woodpecker, the family, the sky and
 * the weather, the HUD and the screens (the house UI kit), and the versus split screen (SDK viewports: 2 players
 * side by side, 3-4 players in full-height columns, house_ui.c hu_split HU_SPLIT_COLUMNS).
 * Nothing here changes the game.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
 *
 * Layers (docs/art-direction.md): BG1 the UI (the kit), BG2 the trunk, BG3 the dam, banks and near bank
 * (scene.c), BG4 the far layer; OBJ 0 the beaver (each viewport its player's colours: scene.c), 1 player 2 (the
 * title's icon), 2 the wood, 3 the kit, 4 fx, 5 gold, 6 the bird, 7 the season's particles.
 */
#include "br.h"
#include "draw.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include "house_tall.h"             /* a phone held upright: the title and the panels in the columns shown */
#include <stdio.h>
#include <string.h>

/* the title logo: BEAVER in the beaver accent, RUSH in gold (house_style.py ACCENTS, HOUSE gold) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(236, 184, 124), RS_RGB8(190, 124, 68), RS_RGB8(128, 76, 40)},
    {RS_RGB8(255, 238, 140), RS_RGB8(252, 204, 64), RS_RGB8(214, 142, 28)}};
static const int medal_scores[4] = {MEDAL_T1, MEDAL_T2, MEDAL_T3, MEDAL_T4};
static ht_title_out title_out;            /* where the narrow title put its glyph and slots (drawn every title frame) */

/* the timer bar: BG1 tiles at BAR_TILE (caps, then 0..8 px filled, normal and low), palette 0 entries 11..14 */
#define BAR_PAL_LIGHT 11
#define BAR_PAL_MID   12
#define BAR_PAL_LOW   13
#define BAR_PAL_LOWD  14
#define BAR_ROW       4

/* ---- per player cosmetic state, effects ------------------------------------------------------------------------ */
typedef struct pview {
    int drawn_logs, drawn_sig;      /* the trunk on BG2 was drawn for this */
    int drop_t;                      /* frames since the last gnaw (the drop) */
    int32_t bird_y;                  /* the woodpecker, Q8 screen y */
    int bird_side, bird_peck, bird_on;
    int bar_px;                      /* the timer bar shown */
} pview;
static pview PV[MAX_PLAYERS];

typedef struct fx_obj {
    uint8_t kind, frame, pal, prio, flip, view;
    int16_t t, life, slot;
    int32_t x, y, vx, vy;            /* Q8, view coordinates */
    int32_t x0, y0, x1, y1;          /* floaters: from, to (Q8) */
} fx_obj;
enum { FX_NONE, FX_LOG, FX_CHIP, FX_BRANCH, FX_SPLASH, FX_FLOAT, FX_RIPPLE, FX_ZZZ, FX_SPARK };
#define FX_MAX 72
static fx_obj fx[FX_MAX];
static rs_rng fx_rng;
static int shown_state = -1, shown_best = -1, shown_players = -1, shown_over_t = -1;
static int banner_t = 1000, banner_stage, banner_on;   /* the scene name shown after a milestone */
static int family_cheer;                        /* frames left of the family's cheer */
static int views_on;
static int laid_w;                              /* the width the maps and the bars were laid out for */
static int view_oam[MAX_PLAYERS][2];            /* the last frame's OAM range of each player's view (the bot) */

/* in 4 columns (78 px) the beaver stands this much closer to the trunk, its teeth in the bark, so that more of it
 * shows */
#define NARROW_NUDGE 6

/* ---- helpers ------------------------------------------------------------------------------------------------------ */
static void spr(int id, int x, int y, int prio, int pal, int flags)
{
    const br_sprite_def *d = &br_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= rs_screen_top() - (int)d->h ||
        y >= rs_screen_bottom())
        return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

/* widescreen (DESIGN.md "Widescreen"): a screen wider than 320 centres the 320-px stage (rs_screen_origin in
 * draw_init) and shows more of the scenery on both sides; the UI layer and the trunks' layer are 128 tiles wide
 * (the versus views are wider: the trees 256 px apart, the timer bars where the views are) */
static int wide(void) { return rs_screen_width() > RS_SCREEN_W; }
static int map_w(void) { return wide() ? 128 : 64; }
static int fdiv8(int v) { return v >= 0 ? v / 8 : -((7 - v) / 8); }      /* floor(v / 8) */
/* a tall screen (portrait, DESIGN.md "Tall screen"): BG2 holds the 16 segments kept (TALL_ROWS rows) */
static int tall(void) { return rs_screen_height() > RS_SCREEN_H; }
static int bg2_rows(void) { return tall() ? TALL_ROWS : 32; }

/* the split screen: a full-height view per player (2: halves; 3-4: columns); the stage's 240 lines (a tall screen:
 * draw_frame's two more views, the sky above them and the earth below) */
static int split(int players, rs_viewport *v)
{
    int n = hu_split(players, HU_SPLIT_COLUMNS, v);
    for (int i = 0; i < n; i++) v[i].y = 0, v[i].h = RS_SCREEN_H;
    return n;
}

static int view_w(const world *w)
{
    rs_viewport v[RS_VIEW_MAX];
    if (w->players == 1) return RS_SCREEN_W;
    split(w->players, v);
    return v[0].w;
}

static int view_x0(const world *w, int p)
{
    rs_viewport v[RS_VIEW_MAX];
    if (w->players == 1) return 0;
    split(w->players, v);
    return v[p & 3].x;
}

/* the trunk's centre in a view (1 player: the screen's) */
static int tree_cx(const world *w) { return w->players == 1 ? 160 : (view_w(w) + 1) / 2; }

int draw_view_x(const world *w, int p)
{
    if (w->players == 1) return w->bv[0].side == SIDE_L ? 130 : 190;
    return view_x0(w, p) + tree_cx(w);
}

void draw_view_oam(int p, int *first, int *count)
{
    *first = view_oam[p & 3][0];
    *count = view_oam[p & 3][1];
}

/* the panorama x shown at a view's left edge: the middle of the valley, around the trunk */
static int view_bg_x(const world *w) { return w->players == 1 ? PANO_X + scene_lean() / 2 : PANO_X + 160 - tree_cx(w); }

static fx_obj *fx_new(int kind, int view)
{
    for (int i = 0; i < FX_MAX; i++)
        if (fx[i].kind == FX_NONE) {
            memset(&fx[i], 0, sizeof fx[i]);
            fx[i].kind = (uint8_t)kind;
            fx[i].view = (uint8_t)view;
            fx[i].prio = 2;
            return &fx[i];
        }
    return NULL;
}

static int rnd(int n) { return rs_rng_range(&fx_rng, n); }

/* ---- the timer bar tiles --------------------------------------------------------------------------------------- */
static void bar_tiles(void)
{
    rs_pal_set(RS_PAL_BG(PAL_UI) + BAR_PAL_LIGHT, RS_RGB8(255, 226, 110));
    rs_pal_set(RS_PAL_BG(PAL_UI) + BAR_PAL_MID, RS_RGB8(226, 150, 40));
    rs_pal_set(RS_PAL_BG(PAL_UI) + BAR_PAL_LOW, RS_RGB8(255, 120, 100));
    rs_pal_set(RS_PAL_BG(PAL_UI) + BAR_PAL_LOWD, RS_RGB8(196, 40, 44));
    uint8_t t[64];
    for (int cap = 0; cap < 2; cap++) {                 /* rounded caps: the outline and the rim */
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                int e = cap ? 7 - x : x;                /* distance to the outer edge */
                int v = 0;
                if (e >= 5) v = (y == 0 || y == 7) ? HU_OUTLINE : HU_DARK;
                else if (e >= 3) v = (y <= 0 || y >= 7) ? 0 : (y == 1 || y == 6) ? HU_OUTLINE : HU_DARK;
                else if (e == 2) v = (y >= 2 && y <= 5) ? HU_OUTLINE : 0;
                t[y * 8 + x] = (uint8_t)v;
            }
        rs_tiles_load8(VR_BG1 + BAR_TILE + cap, t, 1);
    }
    for (int low = 0; low < 2; low++)
        for (int k = 0; k <= 8; k++) {
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    int v = (y == 0 || y == 7) ? HU_OUTLINE : HU_DARK;
                    if (y >= 1 && y <= 6 && x < k)
                        v = y <= 2 ? (low ? BAR_PAL_LOW : BAR_PAL_LIGHT) : (low ? BAR_PAL_LOWD : BAR_PAL_MID);
                    t[y * 8 + x] = (uint8_t)v;
                }
            rs_tiles_load8(VR_BG1 + BAR_TILE + 2 + low * 9 + k, t, 1);
        }
}

static void bar_draw(int col, int inner, int px, int low)
{
    int mw = rs_bg_map_w(RS_BG1) - 1;               /* widescreen: a view's columns may be left of 0 */
    rs_bg_put(RS_BG1, col & mw, BAR_ROW, RS_MAP(BAR_TILE, PAL_UI, 1, 0, 0));
    for (int i = 0; i < inner; i++) {
        int k = clampi(px - i * 8, 0, 8);
        rs_bg_put(RS_BG1, (col + 1 + i) & mw, BAR_ROW, RS_MAP(BAR_TILE + 2 + low * 9 + k, PAL_UI, 1, 0, 0));
    }
    rs_bg_put(RS_BG1, (col + 1 + inner) & mw, BAR_ROW, RS_MAP(BAR_TILE + 1, PAL_UI, 1, 0, 0));
}

/* player p's bar: its left cap's column and its inner tiles, centred in its view (on the 8-px grid of BG1) */
static void bar_geom(const world *w, int p, int *col, int *inner)
{
    if (w->players == 1 || (w->players == 2 && !wide())) {
        *col = w->players == 1 ? 13 : p ? 25 : 5;
        *inner = w->players == 1 ? 12 : 8;
        return;
    }
    rs_viewport v[RS_VIEW_MAX];
    split(w->players, v);
    int x0 = v[p].x, x1 = v[p].x + v[p].w, m = 6;      /* the caps draw from +2 (left) and up to +5 (right) */
    int c0 = fdiv8(x0 + m - 2 + 7), c1 = fdiv8(x1 - m - 6);
    *inner = clampi(c1 - c0 - 1, 1, 12);
    *col = c0 + (c1 - c0 - 1 - *inner) / 2;            /* centred (a widescreen view is wider than the bar) */
}

/* ---- set-up ------------------------------------------------------------------------------------------------------ */
void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());       /* widescreen: the 320-px stage centred, the scenery wider */
    /* a tall screen (portrait): the stage low, TALL_BELOW lines of earth under it, the trunk up to the top (draw.h) */
    rs_screen_origin_v(clampi(rs_screen_height() - RS_SCREEN_H - TALL_BELOW, 0, TALL_ABOVE));
    hu_config hc = hu_defaults();   /* BG1 at VRAM 0, the kit at 0 on palette 0, the logo at 320 on palette 5 */
    hc.obj_tile = 0;                /* the kit sprites first (HU_OBJ_TILES), ours from OBJ_FIRST */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = OPAL_KIT;
    hc.box_glyphs = " SCOREBESTNWMDALPYIG1234!:-RKH";   /* the panels' letters only (VRAM) */
    /* the glyphs beyond ASCII (translated text): the kit's default pool (640..767) holds the timer bar here (BAR_TILE
     * 700..719: its tiles were overwritten by the PAUSED text in ja, el, ru...); the free tiles after it instead */
    hc.ext_tile = 768;
    hu_init(&hc);
    bar_tiles();
    rs_bg_setup(RS_BG1, map_w(), 32, VR_BG1);
    rs_bg_setup(RS_BG2, map_w(), bg2_rows(), VR_BG2);   /* four trees, a quarter of the map apart (tree_cols) */
    rs_tiles_load(VR_BG2, br_bg2_tiles, br_bg2_tile_count);
    scene_init();
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ + OBJ_FIRST, br_obj_tiles, br_obj_tile_count);
    rs_rng_seed(&fx_rng, 0xbea7e2u);
    laid_w = rs_screen_width();
    memset(PV, 0, sizeof PV);
    for (int p = 0; p < MAX_PLAYERS; p++) PV[p].drawn_logs = -1;
}

/* ---- the trunk ----------------------------------------------------------------------------------------------------- */
static void tree_draw(int p, const tree *t)
{
    /* (a tall screen: every segment kept, up to the screen's top; the map's rows wrap, TALL_ROWS of them) */
    int c0 = p * tree_cols(), mw = rs_bg_map_w(RS_BG2) - 1, mh = rs_bg_map_h(RS_BG2) - 1;
    for (int r = 0; r <= mh; r++)
        for (int c = LBRANCH_COL; c < LBRANCH_COL + 16; c++) rs_bg_put(RS_BG2, (c0 + c) & mw, r, 0);
    for (int k = 0; k <= (tall() ? TRUNK_N - 1 : SEG_SHOWN); k++) {
        const seg *s = &t->s[k];
        int row = fdiv8(TRUNK_MAP_Y0 - SEG_H * (k + 1));
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 6; c++) {
                uint16_t e = br_seg_map[s->look * 18 + r * 6 + c];
                if (s->gold) e = (uint16_t)((e & ~0x1c00) | (PAL_GOLD << 10));
                rs_bg_put(RS_BG2, (c0 + TRUNK_COL + c) & mw, (row + r) & mh, e);
            }
            if (s->branch) {
                int b = (s->branch == SIDE_L ? 0 : 1) + (s->stolen ? 2 : 0);
                int col = s->branch == SIDE_L ? LBRANCH_COL : RBRANCH_COL;
                for (int c = 0; c < 5; c++)
                    rs_bg_put(RS_BG2, (c0 + col + c) & mw, (row + r) & mh, br_branch_map[b * 15 + r * 5 + c]);
            }
        }
    }
}

static int drop_px(int t)
{
    if (t >= DROP_FRAMES) return 0;
    int r = DROP_FRAMES - t;
    return SEG_H * r * r / (DROP_FRAMES * DROP_FRAMES);
}

void draw_new_run(const world *w)
{
    memset(fx, 0, sizeof fx);
    rs_bg_fill(RS_BG2, 0);                              /* no tree of a player who left */
    for (int p = 0; p < MAX_PLAYERS; p++) {
        PV[p].drawn_logs = -1;
        PV[p].drop_t = DROP_FRAMES;
        PV[p].bird_on = 0;
        PV[p].bar_px = -1;
    }
    dam_reset(world_dam_logs(w));
    scene_restart(world_stage(w));
    banner_t = 1000;
    family_cheer = 0;
    shown_state = -1;
}

/* ---- events -> effects ---------------------------------------------------------------------------------------------- */
static void spawn_log(const world *w, int p, const beaver *b)
{
    int cx = tree_cx(w), dir = b->side == SIDE_L ? 1 : -1;
    int gold = (b->last_gnawed >> 2) & 1;
    fx_obj *f = fx_new(FX_LOG, p);
    if (f) {
        f->x = (cx - 24) << 8;
        f->y = (GROUND_Y - SEG_H - 12) << 8;
        f->vx = dir * (1500 + rnd(220));
        f->vy = -(300 + rnd(140));
        f->pal = gold ? OPAL_GOLD : OPAL_WOOD;
        f->flip = dir < 0;
        f->slot = (int16_t)(world_dam_logs(w) - 1);
    }
    for (int i = 0; i < (w->players == 1 ? 5 : w->players == 2 ? 3 : 1); i++) {   /* keep four simultaneous bites within the scanline budget */
        fx_obj *c = fx_new(FX_CHIP, p);
        if (!c) break;
        c->x = (cx - dir * 26 + rnd(6) - 3) << 8;
        c->y = (GROUND_Y - 14 + rnd(6)) << 8;
        c->vx = -dir * (60 + rnd(260)) + (rnd(2) ? 40 : -40);
        c->vy = -(280 + rnd(420));
        c->frame = (uint8_t)rnd(3);
        c->pal = gold ? OPAL_GOLD : OPAL_WOOD;
        c->life = (int16_t)(26 + rnd(14));
    }
    int bs = b->last_gnawed & 3;
    if (bs) {                                            /* its branch snaps off and spins away on its side */
        fx_obj *br = fx_new(FX_BRANCH, p);
        if (br) {
            int d = bs == SIDE_L ? -1 : 1;
            br->x = (cx + d * 44 - 16) << 8;
            br->y = (GROUND_Y - SEG_H - 20) << 8;
            br->vx = d * (300 + rnd(160));
            br->vy = -(500 + rnd(160));
            br->flip = d > 0;
        }
    }
    if (gold)
        for (int i = 0; i < (w->players >= 3 ? 2 : 6); i++) {
            fx_obj *s = fx_new(FX_SPARK, p);
            if (!s) break;
            s->x = (cx - 24 + rnd(48)) << 8;
            s->y = (GROUND_Y - 30 + rnd(24)) << 8;
            s->vy = -(40 + rnd(80));
            s->life = (int16_t)(24 + rnd(20));
        }
}

void draw_events(const world *w)
{
    for (int p = 0; p < w->players; p++) {
        int ev = w->events[p];
        const beaver *b = &w->bv[p];
        if (ev & EV_GNAW) {
            PV[p].drop_t = 0;
            spawn_log(w, p, b);
        }
        if (ev & EV_MILESTONE) {
            family_cheer = 150;
            banner_t = 0;
            banner_stage = world_stage(w);
        }
        if (ev & (EV_STOLEN | EV_GNAW)) PV[p].drawn_logs = -1;          /* redraw the trunk */
    }
}

/* ---- per update: cosmetic motion ---------------------------------------------------------------------------------- */
static void fx_step(const world *w)
{
    for (int i = 0; i < FX_MAX; i++) {
        fx_obj *f = &fx[i];
        if (f->kind == FX_NONE) continue;
        f->t++;
        switch (f->kind) {
        case FX_LOG: {
            f->x += f->vx;
            f->vy += 60;
            f->y += f->vy;
            f->frame = (uint8_t)((f->t / 3) % 6);
            int y = f->y >> 8;
            if (y > NEAR_Y - 8 && f->vy > 0) f->prio = 0;               /* behind the near bank, into the river */
            if (y > NEAR_Y + 10) {
                int x = (f->x >> 8) + 24;
                fx_obj *s = fx_new(FX_SPLASH, f->view);
                if (s) { s->x = (x - 16) << 8; s->y = (NEAR_Y - 10) << 8; s->prio = 0; s->life = 20; }
                sfx_pan(SFX_SPLASH, view_x0(w, f->view) + x, 0);
                fx_obj *fl = fx_new(FX_FLOAT, f->view);
                int sx, sy;
                dam_slot(f->slot, &sx, &sy);
                if (fl) {
                    fl->x0 = fl->x = (x - 8) << 8;
                    fl->y0 = fl->y = (NEAR_Y - 6) << 8;
                    fl->x1 = (DAM_COL * 8 + sx - view_bg_x(w) - 4) << 8;
                    fl->y1 = (DAM_Y + sy - 3) << 8;
                    fl->life = 84;
                    fl->slot = f->slot;
                    fl->pal = f->pal;
                    fl->prio = 0;
                } else {
                    dam_add(f->slot);
                }
                f->kind = FX_NONE;
            }
            break;
        }
        case FX_CHIP:
        case FX_BRANCH:
            f->x += f->vx;
            f->vy += f->kind == FX_CHIP ? 50 : 40;
            f->y += f->vy;
            if (f->kind == FX_BRANCH) {
                f->frame = (uint8_t)((f->t / 4) % 4);
                if ((f->y >> 8) > NEAR_Y) f->prio = 0;
                if ((f->y >> 8) > RS_SCREEN_H) f->kind = FX_NONE;
            } else if (f->t >= f->life) {
                f->kind = FX_NONE;
            }
            break;
        case FX_FLOAT: {
            int k = f->t * 256 / f->life;
            int e = k * (512 - k) / 256;                 /* ease-out */
            f->x = f->x0 + (int32_t)((int64_t)(f->x1 - f->x0) * e / 256);
            f->y = f->y0 + (int32_t)((int64_t)(f->y1 - f->y0) * e / 256);
            f->frame = k > 140;
            if (f->t >= f->life) {
                dam_add(f->slot);
                fx_obj *r = fx_new(FX_RIPPLE, f->view);
                if (r) { r->x = f->x - (4 << 8); r->y = f->y; r->prio = 0; r->life = 18; }
                f->kind = FX_NONE;
            }
            break;
        }
        case FX_SPLASH:
        case FX_RIPPLE:
            if (f->t >= f->life) f->kind = FX_NONE;
            break;
        case FX_ZZZ:
            f->y -= 40;
            f->x += ((f->t / 12) & 1) ? 30 : -30;
            if (f->t >= f->life) f->kind = FX_NONE;
            break;
        case FX_SPARK:
            f->y += f->vy;
            if (f->t >= f->life) f->kind = FX_NONE;
            break;
        }
    }
}

/* the woodpecker: beside the next branch on the beaver's side (the next switch), pecking at it */
static int zigzag_shown(const tree *t)
{
    for (int i = 1; i + 2 <= SEG_SHOWN - 1; i++)
        if (t->s[i].branch && !t->s[i + 1].branch && t->s[i + 2].branch && t->s[i + 2].branch != t->s[i].branch)
            return 1;
    return 0;
}

static void bird_step(const world *w, int p)
{
    pview *v = &PV[p];
    const beaver *b = &w->bv[p];
    v->bird_on = world_stage(w) >= WOODPECKER_STAGE && b->state == BV_PLAY;
    if (!v->bird_on) return;
    int k = -1;
    for (int i = 1; i < SEG_SHOWN && k < 0; i++)
        if (b->tr.s[i].branch == b->side) k = i;
    int side = k >= 0 ? b->side : other_side(b->side), ty;
    if (k < 0) k = SEG_SHOWN - 2;
    ty = (GROUND_Y - SEG_H * (k + 1) + 8) << 8;
    if (side != v->bird_side) { v->bird_side = side; v->bird_y = ty; }
    v->bird_y += (ty - v->bird_y) / 4;
    int period = zigzag_shown(&b->tr) ? 8 : 16;
    v->bird_peck = (w->t % period) < 3;
    if (p == 0 && w->players == 1 && (w->t % period) == 0 && b->tr.s[k].branch == b->side) sfx_pan(SFX_TOK, 160, 0);
}

void draw_update(const world *w, int state)
{
    for (int p = 0; p < w->players; p++) {
        if (PV[p].drop_t < 100) PV[p].drop_t++;
        bird_step(w, p);
        const beaver *b = &w->bv[p];
        if (b->state == BV_SLEEP && (b->t % 40) == 5) {
            fx_obj *z = fx_new(FX_ZZZ, p);
            if (z) {
                z->x = (tree_cx(w) + (b->side == SIDE_L ? -40 : 44)) << 8;
                z->y = (GROUND_Y - 30) << 8;
                z->life = 60;
            }
        }
    }
    fx_step(w);
    if (family_cheer) family_cheer--;
    if (banner_t < 1000) banner_t++;
    hu_shake_step();
    (void)state;
}

/* ---- sprites -------------------------------------------------------------------------------------------------------- */
static int beaver_frame(const beaver *b, int t)
{
    enum { F_IDLE, F_IDLE2, F_WINDUP, F_BITE, F_RECOVER, F_HOP, F_BONK, F_SLEEP, F_CHEER };
    switch (b->state) {
    case BV_READY: return (t % 150) < 10 ? F_IDLE2 : F_IDLE;
    case BV_BONK: return F_BONK;
    case BV_SLEEP: return F_SLEEP;
    case BV_WIN: return F_CHEER + (t / 10) % 2;
    default: {
        int g = b->gnaw_t;
        if (g == 0) return F_WINDUP;
        if (g <= 2) return F_BITE;
        if (g <= 4) return F_RECOVER;
        return (t % 150) < 10 ? F_IDLE2 : F_IDLE;
    }
    }
}

static void draw_beaver(const world *w, int p, int ox, int oy)
{
    const beaver *b = &w->bv[p];
    int cx = tree_cx(w), right = b->side == SIDE_R;
    int f = beaver_frame(b, w->t + p * 37), nudge = w->players >= 4 ? NARROW_NUDGE : 0;
    int x = right ? cx + 24 - nudge : cx - 56 + nudge, y = GROUND_Y - 31;
    /* OBJ 0: in a split screen the raster gives each view its player's colours (scene.c) */
    spr(SPR_BEAVER + f, x + ox, y + oy, 2, OPAL_BEAVER, right ? RS_SPR_HFLIP : 0);
    if (b->state == BV_BONK)                            /* stars circling the bump */
        for (int i = 0; i < (w->players >= 3 ? 1 : 3); i++) {
            int a = (w->t * 2 + i * 21) % 64, dx = a < 32 ? a - 16 : 48 - a, dy = (a < 16 || a >= 48) ? -3 : 3;
            spr(SPR_DIZZY + (w->t / 8 + i) % 2, x + 14 + dx / 2 + ox, y + 1 + dy + oy, 2, -1, 0);
        }
}

static void draw_bird(const world *w, int p, int ox, int oy)
{
    const pview *v = &PV[p];
    if (!v->bird_on) return;
    int cx = tree_cx(w), left = v->bird_side == SIDE_L;
    int x = left ? cx - 24 - 13 : cx + 24 - 3;
    spr(SPR_WOODPECKER + v->bird_peck, x + ox, (v->bird_y >> 8) - drop_px(v->drop_t) + oy, 2, -1, left ? 0 : RS_SPR_HFLIP);
}

static void draw_fx(int view, int ox, int oy, int players)
{
    /* Four columns share one scanline budget. Keep room for each beaver, bird and weather. */
    uint8_t lines[RS_SCREEN_H] = {0};
    for (int i = 0; i < FX_MAX; i++) {
        const fx_obj *f = &fx[i];
        if (f->kind == FX_NONE || f->view != view) continue;
        int x = (f->x >> 8) + ox, y = (f->y >> 8) + oy;
        int first = rs_oam_next();
        switch (f->kind) {
        case FX_LOG: {
            static const uint8_t fr[6] = {0, 1, 2, 3, 2, 1};
            int k = f->flip ? (6 - f->frame) % 6 : f->frame;
            const br_sprite_def *d = &br_spr[SPR_LOG + fr[k]];
            spr(SPR_LOG + fr[k], x + 12 - d->w / 2, y + 12 - d->h / 2, f->prio, f->pal, k > 3 ? RS_SPR_HFLIP : 0);
            break;
        }
        case FX_CHIP: spr(SPR_CHIP + f->frame, x - 4, y - 4, 2, f->pal, 0); break;
        case FX_BRANCH: {
            int k = f->frame, fl = k >= 2 ? RS_SPR_HFLIP | RS_SPR_VFLIP : 0;
            spr(SPR_BRANCH_PIECE + (k & 1), x, y, f->prio, -1, fl ^ (f->flip ? RS_SPR_HFLIP : 0));
            break;
        }
        case FX_SPLASH: spr(SPR_SPLASH + clampi(f->t * 4 / (f->life + 1), 0, 3), x, y, f->prio, -1, 0); break;
        case FX_RIPPLE: spr(SPR_RIPPLE + clampi(f->t * 3 / (f->life + 1), 0, 2), x, y, 0, -1, 0); break;
        case FX_FLOAT: spr(SPR_FLOATER + f->frame, x - (f->frame ? 4 : 8), y - 4, 0, f->pal, 0); break;
        case FX_ZZZ: spr(SPR_ZZZ + (f->t / 15) % 2, x, y, 2, -1, 0); break;
        case FX_SPARK: hu_sparkle(x, y, (f->t / 6) % 2, 2); break;
        }
        if (players >= 3) {
            for (int j = first; j < rs_oam_next(); j++) {
                rs_sprite *s = rs_oam(j);
                int y0 = clampi(s->y, 0, RS_SCREEN_H), y1 = clampi(s->y + s->h, 0, RS_SCREEN_H);
                int room = 1;
                for (int row = y0; row < y1; row++) if (lines[row] >= 4) room = 0;
                if (!room) s->flags |= RS_SPR_HIDE;
                else for (int row = y0; row < y1; row++) lines[row]++;
            }
        }
    }
}

/* the sky: the sun or the moon and stars (behind the mountains), and the season's weather */
static void draw_sky(const world *w, int view_w, int t)
{
    int tod = scene_tod(), season = scene_season(), k = w->players;     /* a view is about 1/k of the screen */
    static const int16_t stars[12][2] = {{12, 8}, {40, 30}, {70, 14}, {96, 40}, {122, 10}, {150, 26}, {182, 12},
                                        {206, 36}, {236, 18}, {262, 6}, {288, 30}, {310, 12}};
    /* widescreen: the sky spans the screen (1 player: stage x0 .. x0 + span) or the wider views (x scaled to them) */
    int wd = wide(), x0 = wd && k == 1 ? rs_screen_left() : 0, span = wd && k == 1 ? rs_screen_width() : view_w;
    /* a tall screen: the weather falls from the screen's top, over the columns shown (the stage y of the top, its
     * height: 0 and 240 on a 240-line screen) */
    int top = rs_screen_top(), sh = rs_screen_bottom() - top;
    if (k == 1 && tall()) {
        x0 = rs_screen_visible_x0() - rs_screen_origin_x();
        span = rs_screen_visible_x1() - rs_screen_visible_x0();
    }
#define SKY_X(x) (wd && k > 1 ? (x) * view_w / RS_SCREEN_W : (x) / k)
    if (tod == 3) {
        spr(SPR_MOON, k == 2 ? SKY_X(48) : k == 1 && tall() ? clampi(54, x0 + 4, 160) : SKY_X(54), 18, 0, -1, 0);
        for (int i = 0; i < 12; i++)
            if (i % k == 0) spr(SPR_STAR + ((t / 20 + i) % 5 == 0), SKY_X(stars[i][0]), stars[i][1] + 2 * (i % 3), 0, -1, 0);
        for (int band = 1; k == 1 && -band * 64 > top - 48; band++)    /* a tall screen: more stars above the stage */
            for (int i = band % 2; i < 12; i += 2)
                spr(SPR_STAR + ((t / 20 + i + band) % 5 == 0), stars[(i + band * 5) % 12][0],
                    stars[i][1] - band * 64, 0, -1, 0);
        if (wd && k == 1)                       /* the margins: the stars mirrored (spr clips them to the screen) */
            for (int i = 0; i < 12; i++) {
                int y = stars[(i + 5) % 12][1] + 2 * (i % 3), f = (t / 20 + i + 3) % 5 == 0;
                spr(SPR_STAR + f, -stars[i][0] - 8, y, 0, -1, 0);
                spr(SPR_STAR + f, 2 * RS_SCREEN_W - stars[i][0], y, 0, -1, 0);
            }
    } else {
        static const int sun[3][2] = {{36, 64}, {244, 12}, {268, 60}};
        int sx = sun[tod][0], sy = sun[tod][1];
        if (k == 1 && tall()) sx = clampi(sx, x0 + 4, x0 + span - 36);    /* (a zoomed screen: in the columns shown) */
        spr(SPR_SUN, SKY_X(sx), sy, 0, -1, 0);
    }
#undef SKY_X
    /* weather: stateless paths from the frame number */
    int n = 0, kind = -1;
    if (season == 1) { n = 7; kind = 0; }                 /* autumn leaves */
    if (season == 2) { n = 14; kind = 1; }                /* snow */
    if (season == 3) { n = 7; kind = 0; }                 /* blossoms (the leaf sprite in spring's pink) */
    if (season == 0 && tod == 3) { n = 6; kind = 2; }      /* fireflies */
    if (wd) n = n * (k == 1 ? span : view_w * k) / RS_SCREEN_W;           /* as many per column on a wider screen */
    n = (n + k - 1) / k;
    /* Leave scanline room for four gnawing beavers, birds and log effects. */
    if (k >= 3 && n > 2) n = 2;
    for (int i = 0; i < n; i++) {
        int speed = 1 + i % 3;
        if (kind == 2) {
            int x = x0 + (i * 53 + (t / 3) % 40) % span, y = 150 + (i * 29) % 40 + ((t / 7 + i * 5) % 12 < 6 ? 0 : 2);
            if ((t / 9 + i) % 7) spr(SPR_FIREFLY + (t / 12 + i) % 2, x, y, 2, -1, 0);
            continue;
        }
        int y = top + (i * 71 + t * speed / (kind ? 2 : 3)) % (sh + 20) - 10;
        int sway = ((t / 4 + i * 11) % 32), dx = sway < 16 ? sway : 32 - sway;
        int x = x0 + (i * 97 + t / 5 + dx) % (span + 8) - 4;
        if (kind == 1) spr(SPR_SNOW + i % 2, x, y, i % 3 ? 2 : 0, -1, 0);
        else spr(SPR_LEAF + (t / 16 + i) % 2, x, y, 2, -1, (t / 32 + i) % 2 ? RS_SPR_HFLIP : 0);
    }
}

/* the family on the far bank, at the left end of the dam (1 player) */
static void draw_family(const world *w, int t)
{
    static const int fam_x[3] = {4, 16, 28};
    int ln = scene_lean() / 2;
    for (int i = 0; i < 3; i++) {
        int x = fam_x[i], y = 116 + (x + 20) / 2 - 15;
        int f = 0;
        if (family_cheer) {
            f = 2 + ((t / 8 + i) % 2);
            if (((t / 6 + i * 2) % 8) < 4) y -= 3;
        } else if ((t / 40 + i * 3) % 7 == 0) {
            f = 1;
        }
        spr(SPR_FAMILY + f, x - ln, y, 1, -1, 0);
    }
    if (family_cheer > 110) {                                   /* the splash on the completed section */
        int k = world_dam_logs(w) - 1, sx, sy;
        dam_slot(k < 0 ? 0 : k, &sx, &sy);
        int sec = ((k < 0 ? 0 : k) / STAGE_LOGS) % DAM_SECTIONS;
        spr(SPR_SPLASH + (150 - family_cheer) / 10, DAM_COL * 8 + sec * 40 + 4 - view_bg_x(w), DAM_Y - 12, 1, -1, 0);
    }
}

/* ---- the screens (the house kit) ---------------------------------------------------------------------------------- */
static const char *scene_name(int stage, char *s, size_t n)
{
    static const int tod[4] = {GS_TOD_DAWN, GS_TOD_DAY, GS_TOD_SUNSET, GS_TOD_NIGHT};
    static const int sea[4] = {GS_SEASON_SUMMER, GS_SEASON_AUTUMN, GS_SEASON_WINTER, GS_SEASON_SPRING};
    rs_strf(s, n, TR(SCENE), rs_tr(&gs_strings, sea[(stage / 4) % 4]), rs_tr(&gs_strings, tod[stage % 4]));
    return s;
}

/* the results of 2-4 players: the last beaver standing first, then the later out (world_rank_keys); scores shown */
static void standing(const world *w, hu_standing *s)
{
    int key[MAX_PLAYERS], score[MAX_PLAYERS];
    world_rank_keys(w, key);
    for (int p = 0; p < MAX_PLAYERS; p++) score[p] = w->bv[p].score;
    hu_rank(s, w->players, key, score);
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    char s[48];
    int redo = state != shown_state || best != shown_best || w->players != shown_players;
    if (redo) {
        hu_clear();
        banner_on = 0;
        if (state == DS_TITLE && ht_narrow()) ht_logo("BEAVER RUSH", title_ramps, 2, 2, 0);    /* BEAVER over RUSH */
        else if (state == DS_TITLE) hu_logo("BEAVER RUSH", title_ramps, 2, 2, 0);
        else hu_logo_hide();
        if (state == DS_OVER) {
            if (w->players == 1) {
                if (ht_narrow()) {                     /* a phone held upright: the panel in the columns shown */
                    ht_banner(4, HTR(GAME_OVER));
                    ht_gameover_panel(new_best, NULL, HTR(BACK_MENU));
                } else {
                    hu_banner(4, HTR(GAME_OVER));
                    hu_gameover_panel(1, new_best, w->bv[0].score, 0);
                    if (!hu_medal_of(w->bv[0].score, medal_scores)) hu_box_text(22, 17, "-");
                }
            } else {
                hu_results_panel(rs, NULL);            /* "P3 WINS!" (or DRAW!) and the ranking */
            }
        }
        shown_state = state;
        shown_best = best;
        shown_players = w->players;
        for (int p = 0; p < MAX_PLAYERS; p++) PV[p].bar_px = -1;
    }
    /* the title, every frame: PRESS <-/-> TO GNAW, the player slots, BEST, the join line, the credits */
    if (state == DS_TITLE && ht_narrow()) {
        ht_title tc = {TR(VERB), MAX_PLAYERS, 15, 0};
        ht_title_draw(&tc, st_t, best, &title_out);
    }
    else if (state == DS_TITLE) hu_title_draw(st_t, best);
    if (state == DS_OVER) {
        if (w->players == 1 && ht_narrow()) ht_retry_line_at(ht_gameover_retry_row(NULL), st_t, RETRY_LOCK, HTR(BACK_MENU));
        else if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_MENU));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_MENU));
    }
    /* the timer bars, one per view */
    if (state == DS_PLAY || state == DS_END) {
        for (int p = 0; p < w->players; p++) {
            const beaver *b = &w->bv[p];
            int col, inner;
            bar_geom(w, p, &col, &inner);
            int px = (int)((int64_t)b->bar * inner * 8 / BAR_FULL);
            int low = b->bar < BAR_FULL / 4 && (b->state != BV_PLAY || (w->t / 8) % 2);
            int key = px * 2 + low;
            if (key != PV[p].bar_px) {
                bar_draw(col, inner, px, low);
                PV[p].bar_px = key;
            }
        }
    }
    /* the scene's name after a milestone (1 player), for 1.5 s */
    int want = w->players == 1 && state == DS_PLAY && banner_t < 90;
    if (want != banner_on) {
        if (want) {
            scene_name(banner_stage, s, sizeof s);
            hu_big(hu_center(s, 1), 7, s, HU_BIG_FREE);
        }
        else hu_clear_rows(7, 2);
        banner_on = want;
    }
}

/* the title's player slots: each joined player's beaver head pops in, in its colours (hu_title_sprites calls it) */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(SPR_ICON, cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 3, scene_player_pal(p), 0);
}

/* the results panel's sprites (2-4 players): the scores, an acorn per medal place (the house tiers, like the solo
 * panel's), a sparkle by the winner and each player's icon; drawn at screen coordinates, then moved into the panel's
 * viewport (x0, y0) */
static void results_sprites(const hu_standing *rs, int st_t, int x0, int y0)
{
    int first = rs_oam_next();
    for (int i = 0; i < rs->n; i++) {
        int y = hu_results_row(rs, i) * 8, m = hu_results_medal(rs, i), cx, cy;
        hu_number(rs->value[rs->order[i]], 24 * 8, y - 4, 3);
        if (m) {
            spr(SPR_ACORN + m - 1, 27 * 8, y - 8, 3, OPAL_KIT, 0);
            if (rs->rank[i] == 0 && (st_t / 20) % 3 == 0) hu_sparkle(27 * 8 + 18, y - 8, (st_t / 10) % 2, 3);
        }
        hu_results_icon_pos(rs, i, &cx, &cy);
        spr(SPR_ICON, cx - 8, cy - 8, 3, OPAL_BEAVER, 0);
    }
    for (int i = first; i < rs_oam_next(); i++) {
        rs_sprite *s = rs_oam(i);
        s->x = (int16_t)(s->x - x0);
        s->y = (int16_t)(s->y - y0);
    }
}

static void view_rect(rs_viewport *v, int x, int y, int w, int h, int sx, int sy)
{
    memset(v, 0, sizeof *v);
    v->x = (int16_t)x; v->y = (int16_t)y; v->w = (int16_t)w; v->h = (int16_t)h;
    v->layers = 1 << RS_BG1;
    v->sx[RS_BG1] = (int16_t)sx;
    v->sy[RS_BG1] = (int16_t)sy;
}

/* ---- per frame ------------------------------------------------------------------------------------------------------- */
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    int t = w->t, multi = w->players >= 2;
    const beaver *b0 = &w->bv[0];
    hu_standing rs;
    standing(w, &rs);
    scene_title_pals(state == DS_TITLE);
    int stage = state == DS_TITLE ? 0 : world_stage(w);
    scene_frame(stage, world_dam_logs(w), state == DS_TITLE ? 0 : b0->side, w->players, (int)rs_frame_count());
    for (int p = 0; p < w->players; p++)
        if (PV[p].drawn_logs != w->bv[p].logs) {
            tree_draw(p, &w->bv[p].tr);
            PV[p].drawn_logs = w->bv[p].logs;
        }
    int sx = hu_shake_x(), sy = hu_shake_y(), cx = tree_cx(w), vw = view_w(w);
    if (!multi) {
        int tx = (TRUNK_MAP_X - (160 - TRUNK_W / 2) - sx) & (rs_bg_map_w(RS_BG2) * 8 - 1);
        int ty = (TRUNK_SCROLL_Y + drop_px(PV[0].drop_t) - sy) & (rs_bg_map_h(RS_BG2) * 8 - 1);
        rs_bg_scroll(RS_BG2, tx, ty);
        scene_trunk(160 - TRUNK_W / 2 + sx, tx, ty);    /* a tall screen: the trunk above the stage */
        scene_bg_scroll(PANO_X);
    }
    /* a tall screen (DESIGN.md "Tall screen"): one view throughout, the display's height filled (the sides cropped):
     * the title, play, the panels (laid out in the columns shown: house_tall.h); only a split screen (2-4 beavers, each
     * its trunk across the stage) and its ranking over the views show every column; the HUD (the score, the bar) at
     * the screen's top */
    rs_screen_fill(!multi || state == DS_TITLE);
    screen_text(w, state, st_t, best, new_best, &rs);
    int slide = state == DS_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    ht_pause(paused, 13);
    int hud = tall() && !multi && (state == DS_PLAY || state == DS_END), hy = hud ? rs_screen_top() : 0;
    rs_bg_pin(RS_BG1, 0, hud ? HUD_LINES : 0, 0);
    scene_hud(hud ? HUD_LINES : 0);
    int pin = !ht_narrow() ? HT_PIN_OFF : state == DS_TITLE ? HT_PIN_TITLE : state == DS_OVER && !multi ? HT_PIN_PANEL : HT_PIN_OFF;
    if (pin) ht_pin(pin);                       /* the title and the panels at the same place on every game's screen */
    scene_ui_pin(pin, ht_pin_dy(pin));

    rs_viewport v[RS_VIEW_MAX];
    int nv = multi ? split(w->players, v) : 0;
    rs_oam_clear();
    /* front to back: the UI, the effects, the beaver, the bird, the sky */
    if (state == DS_TITLE && ht_narrow()) {
        ht_title tc = {TR(VERB), MAX_PLAYERS, 15, 0};
        ht_title_sprites(&tc, &title_out, st_t, slot_icon, NULL);
    }
    else if (state == DS_TITLE) hu_title_sprites(st_t, slot_icon, NULL);   /* the D-pad glyph, the players popping in */
    if (!multi && state == DS_OVER) {
        /* the house panel's values; the medal is an acorn in the house tiers (bronze, silver, gold, pearl) */
        int m = hu_medal_of(w->bv[0].score, medal_scores);
        if (ht_narrow()) {                         /* (the panel in the columns shown: each value under its label) */
            int cx, y;
            ht_gameover_pos(0, &cx, &y);
            hu_number(w->bv[0].score, cx, y + slide, 3);
            ht_gameover_pos(1, &cx, &y);
            hu_number(best, cx, y + slide, 3);
            ht_gameover_pos(2, &cx, &y);
            if (m) spr(SPR_ACORN + m - 1, cx - 12 + hu_ui_x(), y + slide, 3, OPAL_KIT, 0);
            if (m && (st_t / 20) % 3 == 0) hu_sparkle(cx + 14, y + slide, (st_t / 10) % 2, 3);
            if (!m) hu_box_text((cx - 4) / 8, y / 8 + 1, "-");
        } else {
        hu_number(w->bv[0].score, 25 * 8, 11 * 8 - 4 + slide, 3);
        hu_number(best, 25 * 8, 14 * 8 - 4 + slide, 3);
        if (m) {
            spr(SPR_ACORN + m - 1, 22 * 8 - 4, 16 * 8 - 4 + slide, 3, OPAL_KIT, 0);
            if ((st_t / 20) % 3 == 0) hu_sparkle(22 * 8 + 12, 16 * 8 - 4 + slide, (st_t / 10) % 2, 3);
        }
        }
    }
    for (int p = 0; p < w->players; p++) {
        if (multi) hu_view_oam_begin(&v[p]);
        int ox = sx, oy = sy;
        if (state == DS_PLAY || state == DS_END) {
            hu_number(w->bv[p].score, cx, 10 + hy, 3);
            if (multi) {                                /* whose view: the player's head by the score */
                char d[12];
                int len = snprintf(d, sizeof d, "%d", w->bv[p].score);
                spr(SPR_ICON, cx - len * 6 - 19, 10, 3, OPAL_BEAVER, 0);
            }
        }
        draw_fx(p, ox, oy, w->players);
        draw_beaver(w, p, ox, oy);
        draw_bird(w, p, ox, oy);
        if (!multi) draw_family(w, t);
        draw_sky(w, vw, t);
        if (multi) hu_view_oam_end(&v[p]);
        view_oam[p][0] = multi ? v[p].oam_first : 0;
        view_oam[p][1] = multi ? v[p].oam_count : RS_OAM_MAX;
    }
    for (int p = w->players; p < MAX_PLAYERS; p++) view_oam[p][0] = view_oam[p][1] = 0;
    if (!multi) {
        if (views_on) { rs_viewports(0, NULL, 0); views_on = 0; }
        return;
    }
    scene_trunk(-1, 0, 0);                      /* (a tall split screen: sky only above the stage) */
    /* the split screen: each view its tree, the middle of the valley, its sprites, its beaver's colours */
    for (int p = 0; p < nv; p++) {
        rs_viewport *q = &v[p];
        q->sx[RS_BG1] = q->x;
        q->sy[RS_BG1] = 0;
        q->sx[RS_BG2] = (int16_t)((TRUNK_MAP_X + p * tree_cols() * 8 - (cx - TRUNK_W / 2) - sx) &
                                  (rs_bg_map_w(RS_BG2) * 8 - 1));
        q->sy[RS_BG2] = (int16_t)(TRUNK_SCROLL_Y + drop_px(PV[p].drop_t) - sy);
        q->sx[RS_BG3] = q->sx[RS_BG4] = (int16_t)view_bg_x(w);
        q->sy[RS_BG3] = q->sy[RS_BG4] = 0;
        if (state == DS_OVER) q->layers &= (uint8_t)~(1 << RS_BG1);    /* the panel slides in its own views */
        scene_view_fur(p, p);
    }
    int n = nv;
    if (tall() && rs_screen_top() < 0) {      /* a tall screen: the sky above the views, the earth below (raster_out) */
        view_rect(&v[n], rs_screen_left(), rs_screen_top(), rs_screen_width(), -rs_screen_top(), 0, 0);
        view_rect(&v[n + 1], rs_screen_left(), RS_SCREEN_H, rs_screen_width(), rs_screen_bottom() - RS_SCREEN_H, 0, 0);
        v[n].layers = v[n + 1].layers = 0;
        scene_view_fur(n, -1);
        scene_view_fur(n + 1, -1);
        n += 2;
    }
    if (state == DS_OVER) {                     /* the banner and the ranking over the views, sliding up */
        rs_viewport *bn = &v[n], *pn = &v[n + 1];
        int rows = 3 * rs.n + 4;
        view_rect(bn, 80, 24 + slide, 160, 32, 80, 24);
        view_rect(pn, 64, 64 + slide, 192, rows * 8, 64, 64);
        pn->objs = 1;
        hu_view_oam_begin(pn);
        results_sprites(&rs, st_t, 64, 64);
        hu_view_oam_end(pn);
        scene_view_fur(n, -1);
        scene_view_fur(n + 1, FUR_BY_LINE);
        scene_line_fur(0, RS_SCREEN_H, -1);
        for (int i = 0; i < rs.n; i++) {
            int icx, icy;
            hu_results_icon_pos(&rs, i, &icx, &icy);
            scene_line_fur(icy - 8 + slide, icy + 8 + slide, rs.order[i]);
        }
        n += 2;
    }
    rs_viewports(n, v, RS_RGB8(12, 20, 44));
    views_on = 1;
}

/* ---- save states (main.c) ---- */
/* After a state was loaded (rs_game.state_loaded): the width is not in a state, and one saved at another width brings
 * the maps of that width. Set up those of this one: the scenery (scene.c), the UI layer (its 320 columns move
 * over), the trunks (redrawn a quarter of the map apart, before the bot reads them). In versus the timer bars are
 * drawn again on the next frame (where the views of this width want them; the same place at the same width).
 * A state of another height (the phone turned: house_tall.h ht_shape_reset): the same, the UI drawn again for this
 * screen, and the far layer's and the trunk's windows as scene_init sets them (raster_out hid the layers). */
void draw_relayout(const world *w, int state)
{
    int shape = ht_shape_reset();
    if (shape) {
        rs_bg_window(RS_BG4, RS_WIN1);
        rs_bg_window(RS_BG2, RS_WIN2);
        shown_state = -1;                       /* screen_text clears the UI layer and draws it again */
    }
    /* a state of this size: nothing to do (it saves the same bytes; a tall screen's BG2 is taller: bg2_rows) */
    if (!shape && laid_w == rs_screen_width() && rs_bg_map_h(RS_BG2) == bg2_rows()) return;
    laid_w = rs_screen_width();
    scene_relayout();
    if (rs_bg_map_w(RS_BG1) != map_w()) {
        uint16_t ui[32][40];
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 40; x++) ui[y][x] = rs_bg_get(RS_BG1, x, y);
        rs_bg_setup(RS_BG1, map_w(), 32, VR_BG1);  /* entry 0: the kit's blank (hu_clear) */
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 40; x++) rs_bg_put(RS_BG1, x, y, ui[y][x]);
    }
    if (rs_bg_map_w(RS_BG2) != map_w() || rs_bg_map_h(RS_BG2) != bg2_rows()) {
        rs_bg_setup(RS_BG2, map_w(), bg2_rows(), VR_BG2);
        for (int p = 0; p < w->players; p++) {
            tree_draw(p, &w->bv[p].tr);
            PV[p].drawn_logs = w->bv[p].logs;
        }
    }
    if (w->players >= 2 && (state == DS_PLAY || state == DS_END)) {
        for (int x = 0; x < rs_bg_map_w(RS_BG1); x++) rs_bg_put(RS_BG1, x, BAR_ROW, 0);
        for (int p = 0; p < MAX_PLAYERS; p++) PV[p].bar_px = -1;
    }
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(PV); S(fx); S(fx_rng); S(shown_state); S(shown_best); S(shown_players); S(shown_over_t); S(banner_t);
    S(banner_stage); S(banner_on); S(family_cheer); S(views_on); S(view_oam); S(laid_w);
    scene_state();
}
#undef S
