/*
 * Pairs (Concentration) in RetroStone Board: 1 to 4 players passing the device round (the rules: pairs_rules.c,
 * pairs.h). Nothing is secret (VIEW_OPEN): the view just turns to the player to move. The cards lie face down in a
 * grid (portrait; in landscape the same grid turned on its side, so every card keeps its neighbours). Their backs are
 * the card kit's house back; their faces the house's own pictures: the family deck's animals (families_deck.h) and
 * chess pieces in the players' colours (art.h), each on an ivory card. Tap a card: it turns over. A pair glows gold
 * and stays up, dimmed, with the finder's chip in its corner; two cards that differ turn back by themselves after a
 * moment (or at a tap), and the device passes. The bars: the pairs found (one player: the moves).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/chess.h"
#include "games/families_deck.h"
#include "games/pairs.h"
#include "shell.h"

#define FLIP_MS 300
#define SHOW_MS 1100                            /* two cards that differ: up this long, then back */
#define GOLD_MS 700                             /* a pair: its gold light */

static prgame G;
static int variant;

/* ---- the grid ---------------------------------------------------------------------------------------------------- */
static struct {
    box b;
    int land, cols, rows;                       /* as shown */
    float w, x0, y0;                            /* a card's width; the grid's top left */
} T;

static float aspect(void) { return cards_aspect(DECK_FRENCH); }

static void layout(box b)
{
    const prpos *p = &G.pos;
    T.b = b;
    T.land = b.w > b.h * 1.05f;
    T.cols = T.land ? p->rows : p->cols;
    T.rows = T.land ? p->cols : p->rows;
    float gap = 0.12f;
    T.w = floorf(fminf(b.w * 0.96f / (T.cols + (T.cols - 1) * gap),
                       b.h * 0.96f / (T.rows * aspect() + (T.rows - 1) * gap)));
    float gw = T.w * (T.cols + (T.cols - 1) * gap), gh = T.w * (T.rows * aspect() + (T.rows - 1) * gap);
    T.x0 = floorf(b.x + (b.w - gw) / 2);
    T.y0 = floorf(b.y + (b.h - gh) / 2);
}

/* a card's centre: its place in the portrait grid, turned on its side in landscape */
static void spot(int i, float *x, float *y)
{
    const prpos *p = &G.pos;
    int r = i / p->cols, c = i % p->cols, col = T.land ? r : c, row = T.land ? c : r;
    *x = T.x0 + col * 1.12f * T.w + T.w / 2;
    *y = T.y0 + row * (aspect() + 0.12f) * T.w + T.w * aspect() / 2;
}

/* ---- the faces --------------------------------------------------------------------------------------------------- */
static const uint8_t RIM[4] = {176, 164, 140, 255}, IVORY[4] = {250, 246, 236, 255};
static const uint8_t WALNUT[3] = {104, 70, 46}, LINEN[3] = {226, 210, 178};

/* src (s x s) over dst, centred on (cx, cy) */
static void blend_c(uint8_t *dst, int w, int h, const uint8_t *src, int s, float cx, float cy)
{
    art_blend(dst, w, h, src, s, s, (int)floorf(cx - s / 2.0f + 0.5f), (int)floorf(cy - s / 2.0f + 0.5f));
}

/* an ivory card in its rim, the picture big in the middle: an animal's badge, or a chess piece on a disc (walnut
 * under the light ones, linen under the dark ones) */
static void paint_face(int pic, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    art_roundrect(w, h, 0.075f * w, RIM, o);
    int b = w / 60 > 1 ? w / 60 : 1, d = (int)(0.84f * w);
    uint8_t *t = malloc((size_t)w * h * 4);
    if (!t) return;
    art_roundrect(w - 2 * b, h - 2 * b, 0.075f * w - b, IVORY, t);
    art_blend(o, w, h, t, w - 2 * b, h - 2 * b, b, b);
    if (pic < PR_ANIMALS) {
        fam_emblem(pic, d, t);
        blend_c(o, w, h, t, d, w / 2.0f, h / 2.0f);
    } else {
        int type, col, s = (int)(0.8f * d);
        pr_piece(pic, &type, &col);
        art_disc(d, col == 0 || col == 4 ? WALNUT : LINEN, 1, 0, t);
        blend_c(o, w, h, t, d, w / 2.0f, h / 2.0f);
        art_piece_style(type, &art_player[col], s, t);
        blend_c(o, w, h, t, s, w / 2.0f, h / 2.0f - 0.03f * d);
    }
    free(t);
}

static SDL_Texture *t_disc;
static int tok_size, disc_size;

/* the cards: a deck of the game's own for the kit (its faces: a picture's number; the backs: the house's) */
static int deck(void)
{
    static const cards_deck_def def = {1.4f, paint_face, NULL, NULL};
    static int d = -1;
    if (d < 0) d = cards_deck_add(&def);
    return d;
}

/* a card face down, or up (how: the kit's CD_*) */
static void card(int pic, float cx, float cy, float w, int up, int how)
{
    cards_draw(deck(), up ? pic : CARD_NONE, cx, cy, w, 0, how | (up ? 0 : CD_DOWN));
}

/* ---- the flips --------------------------------------------------------------------------------------------------- */
static struct { int on, up; Uint32 t0; } fl[PR_MAX];
static Uint32 quiet, back_at, gold_until;
static int gold[2] = {-1, -1};

static void calm(void)
{
    memset(fl, 0, sizeof fl);
    quiet = back_at = gold_until = 0;
    gold[0] = gold[1] = -1;
}

static void flip(int i, int up, int delay)
{
    fl[i].on = 1;
    fl[i].up = up;
    fl[i].t0 = ui_ms() + (Uint32)delay;
    Uint32 end = fl[i].t0 + FLIP_MS;
    if (end > quiet) quiet = end;
    ui_sound_later(SND_PLACE, delay + FLIP_MS / 2);
}

/* a flip's progress: -1 none (or over), else 0..1 towards the side it turns to */
static float flipping(int i)
{
    if (!fl[i].on) return -1;
    Uint32 now = ui_ms();
    if (now >= fl[i].t0 + FLIP_MS) { fl[i].on = 0; return -1; }
    ui_animate();
    float t = now < fl[i].t0 ? 0 : (float)(now - fl[i].t0) / FLIP_MS;
    return t * t * (3 - 2 * t);
}

static int busy(void)
{
    for (int i = 0; i < PR_MAX; i++)
        if (fl[i].on && ui_ms() < fl[i].t0 + FLIP_MS) return 1;
    return ui_ms() < quiet;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    variant = v >= 0 && v < PR_VARIANTS ? v : PR_MEDIUM;
    pr_new(&G, variant, ui.players, (uint32_t)ui_random(1 << 30));
    calm();
}

static int turn(void) { return G.pos.turn; }

static int status(void)
{
    const prpos *p = &G.pos;
    if (p->phase != PP_OVER) return GS_PLAY;
    if (p->players == 1) return GS_WIN0;
    unsigned w = pr_winners(p);
    int n = 0, q = 0;
    for (int k = 0; k < p->players; k++)
        if (w >> k & 1) n++, q = k;
    return n > 1 ? GS_DRAW : gs_win(q);
}

static unsigned winners(void) { return G.pos.players > 1 ? pr_winners(&G.pos) : 0; }

static void act(int a)
{
    prpos b = G.pos;
    if (!pr_play(&G, a)) { ui_sound(SND_DENY); return; }
    const prpos *p = &G.pos;
    if (PA_KIND(a) == PA_BACK) {                /* both turn back; the device passes */
        flip(b.up[0], 0, 0);
        flip(b.up[1], 0, 60);
        back_at = 0;
        ui_handoff(p->turn);
        return;
    }
    int c = PA_CARD(a);
    flip(c, 1, 0);
    if (b.up[0] >= 0 && p->found[c] != PR_NONE) {   /* a pair: its gold light */
        gold[0] = b.up[0], gold[1] = c;
        gold_until = ui_ms() + FLIP_MS + GOLD_MS;
        quiet = gold_until;
        ui_sound_later(SND_CAPTURE, FLIP_MS);
    } else if (p->phase == PP_SHOW) back_at = ui_ms() + FLIP_MS + SHOW_MS;
}

/* undo: one card turned; a pair turned back is taken back with the card that missed */
static int undo(void)
{
    if (!G.n) return 0;
    if (PA_KIND(G.act[G.n - 1]) == PA_BACK) pr_undo(&G);
    pr_undo(&G);
    calm();
    if (G.pos.phase == PP_SHOW) back_at = ui_ms() + SHOW_MS;
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = pr_script(&G, s);
    calm();
    if (G.pos.phase == PP_SHOW) back_at = ui_ms() + SHOW_MS;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void textures(void)
{
    tok_size = (int)fmaxf(4, 0.3f * T.w);
    int ds = (int)(2.2f * ui.unit);
    if (ds != disc_size) {
        disc_size = ds;
        const uint8_t dark[3] = {24, 30, 28};
        ui_free(&t_disc);
        art_disc(ds, dark, 0.86f, 0, ui_buf(ds, ds));
        t_disc = ui_tex(ds, ds, ui_buf(ds, ds));
    }
}

static void draw(void)
{
    const prpos *p = &G.pos;
    Uint32 now = ui_ms();
    textures();
    if (p->phase == PP_SHOW && back_at && now >= back_at && !busy()) act(PA_BACK);   /* they turn back by themselves */
    if (p->phase == PP_SHOW && back_at) ui_animate();
    int glowing = now < gold_until;
    if (glowing) ui_animate();
    for (int i = 0; i < p->n; i++) {
        float x, y, t = flipping(i);
        spot(i, &x, &y);
        int found = p->found[i] != PR_NONE, up = found || i == p->up[0] || i == p->up[1];
        if (t >= 0) {
            cards_draw_flip(deck(), p->pic[i], x, y, T.w, 0, fl[i].up ? t : 1 - t, 0);
            continue;
        }
        if (glowing && (i == gold[0] || i == gold[1])) {
            ui_mark_gold(x, y, 1.9f * T.w);
            card(p->pic[i], x, y, T.w, 1, CD_RAISED);
            continue;
        }
        if (found) {                            /* found: dimmed, flat, the finder's chip in its corner */
            card(p->pic[i], x, y, T.w * 0.94f, 1, CD_FLAT | CD_DIM);
            if (p->players > 1) {
                float d = (float)tok_size;
                ui_put_c(ct_token(p->found[i], tok_size), x + T.w * 0.47f - d * 0.6f, y + T.w * aspect() * 0.47f - d * 0.6f,
                         d, d);
            }
            continue;
        }
        card(p->pic[i], x, y, T.w, up, 0);
    }
    if (p->phase == PP_OVER && p->players == 1 && !busy()) {   /* solved: the moves, big */
        float cx = T.b.x + T.b.w / 2, cy = T.b.y + T.b.h / 2, d = 2.2f * ui.unit;
        ui_mark_gold(cx, cy, 1.6f * d);
        ui_put_c(t_disc, cx, cy, d, d);
        ui_number(cx, cy, 0.4f * d, p->moves, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int card_at(float x, float y)
{
    for (int i = 0; i < G.pos.n; i++) {
        float cx, cy;
        spot(i, &cx, &cy);
        if (cards_in(cx, cy, T.w * 1.08f, aspect(), x, y)) return i;
    }
    return -1;
}

static void press(float x, float y)
{
    const prpos *p = &G.pos;
    if (ui_handing() || p->phase == PP_OVER) return;
    if (p->phase == PP_SHOW) {                  /* a tap turns them back at once */
        if (!busy()) act(PA_BACK);
        return;
    }
    int i = card_at(x, y);
    if (i < 0 || !pr_legal(p, PA_TURN | i)) { ui_sound(SND_DENY); return; }
    act(PA_TURN | i);
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pairs found: two small cards and the count (one player: the moves) */
static void bar(int player, box b)
{
    const prpos *p = &G.pos;
    float u = ui.unit, cy = b.y + b.h / 2, w = 0.26f * u;
    int v = p->players == 1 ? p->moves : p->pairs[player], digits = v >= 100 ? 3 : v >= 10 ? 2 : 1;
    cards_draw(DECK_FRENCH, CARD_NONE, b.x + w * 0.5f, cy, w, -8, CD_DOWN | CD_FLAT);
    cards_draw(DECK_FRENCH, CARD_NONE, b.x + w * 0.95f, cy, w, 8, CD_DOWN | CD_FLAT);
    float h = fminf(0.46f * u, b.h * 0.6f), nw = (0.74f * (digits - 1) + 0.62f) * h;
    ui_number(b.x + 1.5f * w + 0.2f * u + nw / 2, cy, h, v, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: six cards, a pair of bears turned up; the picker's: the variant's grid of backs, a pair up */
static void thumb(int v, int size, uint8_t *rgba)
{
    int cols = v < 0 ? 3 : v == PR_SMALL ? 3 : v == PR_MEDIUM ? 4 : 6, rows = v < 0 ? 2 : v == PR_SMALL ? 4 : 6;
    float gap = 0.12f, w = fminf(size * 0.94f / (cols + (cols - 1) * gap), size * 0.94f / (rows * 1.4f + (rows - 1) * gap));
    int cw = (int)w, ch = (int)(w * 1.4f);
    float x0 = (size - w * (cols + (cols - 1) * gap)) / 2, y0 = (size - w * (rows * 1.4f + (rows - 1) * gap)) / 2;
    uint8_t *back = malloc((size_t)cw * ch * 4), *bear = malloc((size_t)cw * ch * 4);
    memset(rgba, 0, (size_t)size * size * 4);
    if (!back || !bear) { free(back); free(bear); return; }
    cards_paint(DECK_FRENCH, CARD_NONE, cw, ch, back);
    paint_face(0, cw, ch, bear);
    int a = v < 0 ? 0 : 1, b = v < 0 ? 4 : cols + 2;
    for (int i = 0; i < cols * rows; i++)
        art_blend(rgba, size, size, i == a || i == b ? bear : back, cw, ch, (int)(x0 + (i % cols) * (1 + gap) * w),
                  (int)(y0 + (i / cols) * (1.4f + gap) * w));
    free(back);
    free(bear);
}

static const game pairs_game = {
    .id = "pairs", .order = 380, .variants = PR_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4, .view = VIEW_OPEN,
};
REGISTER_GAME(pairs_game)
