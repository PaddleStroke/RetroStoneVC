/*
 * Beaver Rush: video. The trunk (BG2, redrawn on each gnaw; the drop is a vertical scroll), the beavers, the
 * effects (the tumbling log, chips, splashes, logs floating to the dam), the woodpecker, the family, the sky and
 * the weather, the HUD and the screens (the house UI kit), and the versus split screen (SDK viewports).
 * Nothing here changes the game.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
 *
 * Layers (docs/art-direction.md): BG1 the UI (the kit), BG2 the trunk, BG3 the dam, banks and near bank
 * (scene.c), BG4 the far layer; OBJ 0 the beaver, 1 player 2, 2 the wood, 3 the kit, 4 fx, 5 gold, 6 the bird,
 * 7 the season's particles.
 */
#include "br.h"
#include "draw.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>

/* the title logo: BEAVER in the beaver accent, RUSH in gold (house_style.py ACCENTS, HOUSE gold) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(236, 184, 124), RS_RGB8(190, 124, 68), RS_RGB8(128, 76, 40)},
    {RS_RGB8(255, 238, 140), RS_RGB8(252, 204, 64), RS_RGB8(214, 142, 28)}};
static const int medal_scores[4] = {MEDAL_T1, MEDAL_T2, MEDAL_T3, MEDAL_T4};

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

/* ---- helpers ------------------------------------------------------------------------------------------------------ */
static void spr(int id, int x, int y, int prio, int pal, int flags)
{
    const br_sprite_def *d = &br_spr[id];
    if (x <= -(int)d->w || x >= RS_SCREEN_W || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

static void oam_pad(int upto)
{
    while (rs_oam_next() < upto) rs_spr(0, -64, 0, 8, 8, 0, 0, RS_SPR_HIDE);
}

static int tree_cx(const world *w) { return w->players == 2 ? 80 : 160; }

/* the panorama x shown at a view's left edge */
static int view_bg_x(const world *w) { return w->players == 2 ? PANO_X + VIEW_BG_X : PANO_X + scene_lean() / 2; }

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
    rs_bg_put(RS_BG1, col, BAR_ROW, RS_MAP(BAR_TILE, PAL_UI, 1, 0, 0));
    for (int i = 0; i < inner; i++) {
        int k = clampi(px - i * 8, 0, 8);
        rs_bg_put(RS_BG1, col + 1 + i, BAR_ROW, RS_MAP(BAR_TILE + 2 + low * 9 + k, PAL_UI, 1, 0, 0));
    }
    rs_bg_put(RS_BG1, col + 1 + inner, BAR_ROW, RS_MAP(BAR_TILE + 1, PAL_UI, 1, 0, 0));
}

/* ---- set-up ------------------------------------------------------------------------------------------------------ */
void draw_init(void)
{
    hu_config hc = hu_defaults();   /* BG1 at VRAM 0, the kit at 0 on palette 0, the logo at 320 on palette 5 */
    hc.obj_tile = 0;                /* the kit sprites first (HU_OBJ_TILES), ours from OBJ_FIRST */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = OPAL_KIT;
    hc.box_glyphs = " SCOREBESTNWMDALPYIG12!:-RKH";   /* the panel's letters only (VRAM) */
    hu_init(&hc);
    bar_tiles();
    rs_bg_setup(RS_BG1, 64, 32, VR_BG1);
    rs_bg_setup(RS_BG2, 32, 32, VR_BG2);
    rs_tiles_load(VR_BG2, br_bg2_tiles, br_bg2_tile_count);
    scene_init();
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ + OBJ_FIRST, br_obj_tiles, br_obj_tile_count);
    rs_rng_seed(&fx_rng, 0xbea7e2u);
    memset(PV, 0, sizeof PV);
    for (int p = 0; p < MAX_PLAYERS; p++) PV[p].drawn_logs = -1;
}

/* ---- the trunk ----------------------------------------------------------------------------------------------------- */
static void tree_draw(int p, const tree *t)
{
    int r0 = 0, c0 = p * TREE_COLS;
    for (int r = 0; r < 32; r++)
        for (int c = LBRANCH_COL; c < LBRANCH_COL + TREE_COLS; c++) rs_bg_put(RS_BG2, (c0 + c) & 31, r0 + r, 0);
    for (int k = 0; k <= SEG_SHOWN; k++) {
        const seg *s = &t->s[k];
        int row = r0 + (TRUNK_MAP_Y0 - SEG_H * (k + 1)) / 8;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 6; c++) {
                uint16_t e = br_seg_map[s->look * 18 + r * 6 + c];
                if (s->gold) e = (uint16_t)((e & ~0x1c00) | (PAL_GOLD << 10));
                rs_bg_put(RS_BG2, (c0 + TRUNK_COL + c) & 31, row + r, e);
            }
            if (s->branch) {
                int b = (s->branch == SIDE_L ? 0 : 1) + (s->stolen ? 2 : 0);
                int col = s->branch == SIDE_L ? LBRANCH_COL : RBRANCH_COL;
                for (int c = 0; c < 5; c++) rs_bg_put(RS_BG2, (c0 + col + c) & 31, row + r, br_branch_map[b * 15 + r * 5 + c]);
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
    for (int i = 0; i < (w->players == 2 ? 3 : 5); i++) {  /* chips from the bite */
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
        for (int i = 0; i < 6; i++) {
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
                if (f->view == 0 || w->players == 2) sfx_pan(SFX_SPLASH, w->players == 2 ? (f->view ? 160 : 0) + x : x, 0);
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
    int f = beaver_frame(b, w->t + p * 37);
    int x = right ? cx + 24 : cx - 56, y = GROUND_Y - 31;
    spr(SPR_BEAVER + f, x + ox, y + oy, 2, p == 1 ? OPAL_BEAVER2 : OPAL_BEAVER, right ? RS_SPR_HFLIP : 0);
    if (b->state == BV_BONK)                            /* stars circling the bump */
        for (int i = 0; i < 3; i++) {
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

static void draw_fx(int view, int ox, int oy)
{
    for (int i = 0; i < FX_MAX; i++) {
        const fx_obj *f = &fx[i];
        if (f->kind == FX_NONE || f->view != view) continue;
        int x = (f->x >> 8) + ox, y = (f->y >> 8) + oy;
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
    }
}

/* the sky: the sun or the moon and stars (behind the mountains), and the season's weather */
static void draw_sky(const world *w, int view_w, int t)
{
    int tod = scene_tod(), season = scene_season(), half = w->players == 2;
    static const int16_t stars[12][2] = {{12, 8}, {40, 30}, {70, 14}, {96, 40}, {122, 10}, {150, 26}, {182, 12},
                                        {206, 36}, {236, 18}, {262, 6}, {288, 30}, {310, 12}};
    if (tod == 3) {
        spr(SPR_MOON, (half ? 24 : 54), 18, 0, -1, 0);
        for (int i = 0; i < 12; i++) {
            int x = half ? stars[i][0] / 2 : stars[i][0];
            if (!half || i % 2 == 0) spr(SPR_STAR + ((t / 20 + i) % 5 == 0), x, stars[i][1] + 2 * (i % 3), 0, -1, 0);
        }
    } else {
        static const int sun[3][2] = {{36, 64}, {244, 12}, {268, 60}};
        int sx = sun[tod][0], sy = sun[tod][1];
        spr(SPR_SUN, half ? sx / 2 : sx, sy, 0, -1, 0);
    }
    /* weather: stateless paths from the frame number */
    int n = 0, kind = -1;
    if (season == 1) { n = 7; kind = 0; }                 /* autumn leaves */
    if (season == 2) { n = 14; kind = 1; }                /* snow */
    if (season == 3) { n = 7; kind = 0; }                 /* blossoms (the leaf sprite in spring's pink) */
    if (season == 0 && tod == 3) { n = 6; kind = 2; }      /* fireflies */
    if (half) n = (n + 1) / 2;
    for (int i = 0; i < n; i++) {
        int speed = 1 + i % 3;
        if (kind == 2) {
            int x = (i * 53 + (t / 3) % 40) % view_w, y = 150 + (i * 29) % 40 + ((t / 7 + i * 5) % 12 < 6 ? 0 : 2);
            if ((t / 9 + i) % 7) spr(SPR_FIREFLY + (t / 12 + i) % 2, x, y, 2, -1, 0);
            continue;
        }
        int y = (i * 71 + t * speed / (kind ? 2 : 3)) % 260 - 10;
        int sway = ((t / 4 + i * 11) % 32), dx = sway < 16 ? sway : 32 - sway;
        int x = (i * 97 + t / 5 + dx) % (view_w + 8) - 4;
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
    static const char *const tod[4] = {"DAWN", "DAY", "SUNSET", "NIGHT"};
    static const char *const sea[4] = {"SUMMER", "AUTUMN", "WINTER", "SPRING"};
    snprintf(s, n, "%s %s", sea[(stage / 4) % 4], tod[stage % 4]);
    return s;
}

static void versus_panel(const world *w, int new_best)
{
    (void)new_best;
    hu_panel(10, 9, 20, 12);
    hu_box_text(12, 11, "PLAYER 1");
    hu_box_text(12, 14, "PLAYER 2");
    const char *win = w->winner == 0 ? "P1 WINS!" : w->winner == 1 ? "P2 WINS!" : "DRAW!";
    hu_box_text(20 - (int)strlen(win) / 2, 17, win);
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best)
{
    char s[48];
    int redo = state != shown_state || best != shown_best || w->players != shown_players;
    if (redo) {
        hu_clear();
        banner_on = 0;
        if (state == DS_TITLE) {
            hu_logo("BEAVER RUSH", title_ramps, 2, 2, 0);
            snprintf(s, sizeof s, "BEST %d", best);
            if (best > 0) hu_text(hu_center(s, 0), 24, s);
            hu_copyright(28);
        } else {
            hu_logo_hide();
        }
        if (state == DS_READY) hu_get_ready(6);
        if (state == DS_OVER) {
            hu_banner(4, "GAME OVER");
            if (w->players == 1) hu_gameover_panel(1, new_best, w->bv[0].score, 0);
            else versus_panel(w, new_best);
        }
        shown_state = state;
        shown_best = best;
        shown_players = w->players;
        for (int p = 0; p < MAX_PLAYERS; p++) PV[p].bar_px = -1;
    }
    if (state == DS_TITLE || state == DS_READY) {
        hu_prompt(21, "PRESS A TO GNAW", st_t);
        if (state == DS_READY || w->players == 2) hu_join_line(26, w->players, "VERSUS!");
    }
    if (state == DS_OVER) hu_retry_line(st_t, RETRY_LOCK, "A: GNAW AGAIN");
    /* the timer bars */
    if (state == DS_PLAY || state == DS_END || state == DS_READY) {
        for (int p = 0; p < w->players; p++) {
            const beaver *b = &w->bv[p];
            int inner = w->players == 1 ? 12 : 8, col = w->players == 1 ? 13 : (p == 0 ? 5 : 25);
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

/* ---- per frame ------------------------------------------------------------------------------------------------------- */
static void setup_views(const world *w, int state, int slide)
{
    if (w->players == 1) {
        if (views_on) { rs_viewports(0, NULL, 0); views_on = 0; }
        return;
    }
    rs_viewport v[4];
    int n = rs_viewport_layout(2, 0, v);
    for (int p = 0; p < 2; p++) {
        rs_viewport *q = &v[p];
        q->sx[RS_BG1] = q->x;
        q->sy[RS_BG1] = 0;
        q->sx[RS_BG2] = (int16_t)((TRUNK_MAP_X + p * TREE_COLS * 8 - (tree_cx(w) - TRUNK_W / 2)) & 255);
        q->sy[RS_BG2] = (int16_t)(TRUNK_SCROLL_Y + drop_px(PV[p].drop_t) - hu_shake_y());
        q->sx[RS_BG2] -= (int16_t)hu_shake_x();
        q->sx[RS_BG3] = q->sx[RS_BG4] = PANO_X + VIEW_BG_X;
        q->sy[RS_BG3] = q->sy[RS_BG4] = 0;
        q->oam_first = (uint16_t)(p * VIEW_OAM);
        q->oam_count = VIEW_OAM;
    }
    if (state == DS_OVER) {                     /* the banner and the panel over both halves */
        rs_viewport *b = &v[n++], *pn = &v[n++];
        memset(b, 0, sizeof *b);
        memset(pn, 0, sizeof *pn);
        b->x = pn->x = 80;
        b->w = pn->w = 160;
        b->y = (int16_t)(32 + slide); b->h = 32;
        pn->y = (int16_t)(72 + slide); pn->h = 96;
        b->layers = pn->layers = 1;
        b->sx[RS_BG1] = pn->sx[RS_BG1] = 80;
        b->sy[RS_BG1] = 32;
        pn->sy[RS_BG1] = 72;
        pn->objs = 1;
        pn->oam_first = 2 * VIEW_OAM;
        pn->oam_count = 16;
    }
    rs_viewports(n, v, RS_RGB8(12, 20, 44));
    views_on = 1;
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    int t = w->t;
    const beaver *b0 = &w->bv[0];
    int stage = state == DS_TITLE ? 0 : world_stage(w);
    scene_frame(stage, world_dam_logs(w), state == DS_TITLE ? 0 : b0->side, w->players, (int)rs_frame_count());
    for (int p = 0; p < w->players; p++)
        if (PV[p].drawn_logs != w->bv[p].logs) {
            tree_draw(p, &w->bv[p].tr);
            PV[p].drawn_logs = w->bv[p].logs;
        }
    int sx = hu_shake_x(), sy = hu_shake_y();
    if (w->players == 1) {
        rs_bg_scroll(RS_BG2, (TRUNK_MAP_X - (160 - TRUNK_W / 2) - sx) & 255,
                     (TRUNK_SCROLL_Y + drop_px(PV[0].drop_t) - sy) & 255);
        scene_bg_scroll(PANO_X);
    }
    screen_text(w, state, st_t, best, new_best);
    int slide = state == DS_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);
    setup_views(w, state, slide);

    rs_oam_clear();
    int vw = w->players == 2 ? 159 : RS_SCREEN_W;
    for (int p = 0; p < w->players; p++) {
        if (p == 1) oam_pad(VIEW_OAM);
        int ox = sx, oy = sy;
        /* front to back: the UI, the effects, the beaver, the bird, the sky */
        if (state == DS_PLAY || state == DS_END) hu_number(w->bv[p].score, tree_cx(w), 10, 3);
        if (p == 0 && (state == DS_TITLE || state == DS_READY) && w->players == 1)
            hu_glyph(HU_BTN_A, hu_center("PRESS A TO GNAW", 0) * 8 - 12, 21 * 8 - 4, (st_t / 30) % 2, 3);
        if (p == 0 && state == DS_OVER && w->players == 1)
            hu_gameover_sprites(1, w->bv[0].score, 0, best, hu_medal_of(w->bv[0].score, medal_scores), st_t, slide);
        draw_fx(p, ox, oy);
        draw_beaver(w, p, ox, oy);
        draw_bird(w, p, ox, oy);
        if (w->players == 1) draw_family(w, t);
        draw_sky(w, vw, t);
    }
    if (w->players == 2 && state == DS_OVER) {        /* the panel's numbers, in the panel's view */
        oam_pad(2 * VIEW_OAM);
        hu_number(w->bv[0].score, 25 * 8 - 80, 11 * 8 - 4 - 72, 3);
        hu_number(w->bv[1].score, 25 * 8 - 80, 14 * 8 - 4 - 72, 3);
    }
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(PV); S(fx); S(fx_rng); S(shown_state); S(shown_best); S(shown_players); S(shown_over_t); S(banner_t);
    S(banner_stage); S(banner_on); S(family_cheer); S(views_on);
    scene_state();
}
#undef S
