/*
 * Poker Run in RetroStone Board: a single-player poker roguelite (the rules: pokerrun_rules.c, pokerrun.h). Portrait
 * first: the charms along the top, the blind's target, score, hands and discards under them, the play area in the
 * middle (the cards played, the chips and the mult), the hand fanned at the bottom and its buttons (sort by rank, play,
 * discard, sort by suit). A hand played scores step by step: each card's chips pop, the mult ticks up, the charms
 * wiggle as they act, then the total fills the target's bar. Between blinds: the coins counted, the shop (charms,
 * tomes, enhancements, packs), the next blind and its twist. Tools: fast scoring, and the book (the hand types'
 * levels, the run's records, the charms met). The charms' and bosses' words: help/<lang>/pokerrun_words.txt.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/pokerrun.h"
#include "shell.h"
#include "text.h"

#define WORDS "pokerrun_words"
/* the lines of the words file */
enum { W_CNAME = 0, W_CFX = 40, W_BNAME = 80, W_BFX = 90, W_HAND = 100, W_ENAME = 109, W_EFX = 116, W_PNAME = 123,
       W_PFX = 126, W_TOME = 129, W_TOMEFX = 130, W_SMALL = 131, W_BIG = 132, W_BOSSB = 133, W_ENDLESS = 134,
       W_FINISH = 135 };

static pkr_game G;
static unsigned enh_mask;                       /* the cards chosen for an enhancement (bit i: the card offered i) */
static int fast;                                /* fast scoring (a setting, kept) */
static uint64_t seen_kept;                      /* the charms met in all runs (kept) */
static int best_ante, wins;                     /* kept */
static uint8_t chosen[PKR_DECK_MAX];            /* the hand's cards chosen (by deck index) */

/* the open panel: a charm, a shop item, a pack's choice, the boss, the book */
enum { INF_NONE, INF_CHARM, INF_SHOP, INF_PACK, INF_BOSS, INF_BOOK, INF_DECK };
static int info, info_i, book_pick = -1;
static struct { int on, from, moved; float x0, y0, x, y; } CD;   /* a charm under the finger */
static int charm_slot_at(float x);

static const uint8_t WHITE[3] = {252, 248, 238}, GOLD[3] = {250, 214, 90}, BLUE[3] = {58, 112, 196},
                     RED[3] = {196, 56, 48}, DIMC[3] = {140, 140, 130}, GREEN[3] = {70, 160, 96};

/* ---- the theme's colours --------------------------------------------------------------------------------------- */
static const uint8_t *ink(void) { return ct_ink(); }
static const uint8_t *panel_ink(void) { return ct_light() ? CT_INK : CT_CREAM; }

/* a panel of our own (the light theme: white; never the shell's, so our white digits stay white) */
static void panel(float x, float y, float w, float h, int alpha)
{
    uint8_t c[4] = {28, 34, 32, (uint8_t)alpha};
    if (ct_light()) c[0] = 252, c[1] = 250, c[2] = 244;
    float r = fminf(0.3f * ui.unit, 0.25f * fminf(w, h));
    int iw = (int)(w + 0.5f), ih = (int)(h + 0.5f);
    if (ct_light()) {                           /* a hairline rim */
        const uint8_t e[4] = {200, 190, 170, (uint8_t)alpha};
        ui_put(ct_round(iw + 2, ih + 2, r + 1, e), x - 1, y - 1, (float)iw + 2, (float)ih + 2);
    }
    ui_put(ct_round(iw, ih, r, c), x, y, (float)iw, (float)ih);
}

static void round_box(float x, float y, float w, float h, const uint8_t rgb[3], Uint8 a)
{
    const uint8_t c[4] = {rgb[0], rgb[1], rgb[2], 255};
    SDL_Texture *t = ct_round((int)(w + 0.5f), (int)(h + 0.5f), 0.22f * fminf(w, h), c);
    SDL_SetTextureAlphaMod(t, a);
    ui_put(t, x, y, w, h);
    SDL_SetTextureAlphaMod(t, 255);
}

/* ---- painting: shapes on a unit square ------------------------------------------------------------------------- */
static void dye(uint8_t *o, int s, art_fig *f, const uint8_t rgb[3], float light)
{
    art_fig_paint(o, s, f, rgb, NULL, 0, light);
}

static void ell(uint8_t *o, int s, float cx, float cy, float rx, float ry, const uint8_t rgb[3])
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, cx, cy, rx, ry);
    dye(o, s, f, rgb, 0.35f);
}

static void ell_flat(uint8_t *o, int s, float cx, float cy, float rx, float ry, const uint8_t rgb[3])
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, cx, cy, rx, ry);
    dye(o, s, f, rgb, 0);
}

static void band(uint8_t *o, int s, float x0, float y0, float x1, float y1, float w, const uint8_t rgb[3])
{
    art_fig *f = art_fig_new();
    art_band(f, ART_PART, x0, y0, x1, y1, w);
    dye(o, s, f, rgb, 0.2f);
}

static void poly(uint8_t *o, int s, const float *xy, int n, const uint8_t rgb[3])
{
    art_fig *f = art_fig_new();
    art_poly(f, ART_PART, xy, n);
    dye(o, s, f, rgb, 0.3f);
}

static void ring(uint8_t *o, int s, float cx, float cy, float r0, float r1, const uint8_t rgb[3])
{
    art_fig *f = art_fig_new();
    art_arc(f, ART_PART, cx, cy, r0, r1, 0, 360);
    dye(o, s, f, rgb, 0.3f);
}

static void arc(uint8_t *o, int s, float cx, float cy, float r0, float r1, float d0, float d1, const uint8_t rgb[3])
{
    art_fig *f = art_fig_new();
    art_arc(f, ART_PART, cx, cy, r0, r1, d0, d1);
    dye(o, s, f, rgb, 0.3f);
}

static void rect(uint8_t *o, int s, float x0, float y0, float x1, float y1, const uint8_t rgb[3])
{
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, x0, y0, x1, y1);
    dye(o, s, f, rgb, 0.25f);
}

static void star(uint8_t *o, int s, float cx, float cy, float r, const uint8_t rgb[3])
{
    art_fig *f = art_fig_new();
    art_star(f, ART_PART, cx, cy, r);
    dye(o, s, f, rgb, 0.3f);
}

/* a suit's pip centred on (cx, cy), r its half size */
static void pip(uint8_t *o, int s, int suit, float cx, float cy, float r, const uint8_t rgb[3])
{
    switch (suit) {
    case SUIT_HEARTS: {
        ell(o, s, cx - 0.48f * r, cy - 0.3f * r, 0.52f * r, 0.5f * r, rgb);
        ell(o, s, cx + 0.48f * r, cy - 0.3f * r, 0.52f * r, 0.5f * r, rgb);
        const float t[] = {cx - 0.98f * r, cy - 0.18f * r, cx + 0.98f * r, cy - 0.18f * r, cx, cy + 0.95f * r};
        poly(o, s, t, 3, rgb);
        break;
    }
    case SUIT_DIAMONDS: {
        const float d[] = {cx, cy - r, cx + 0.72f * r, cy, cx, cy + r, cx - 0.72f * r, cy};
        poly(o, s, d, 4, rgb);
        break;
    }
    case SUIT_SPADES: {
        ell(o, s, cx - 0.46f * r, cy + 0.18f * r, 0.5f * r, 0.46f * r, rgb);
        ell(o, s, cx + 0.46f * r, cy + 0.18f * r, 0.5f * r, 0.46f * r, rgb);
        const float t[] = {cx - 0.94f * r, cy + 0.08f * r, cx + 0.94f * r, cy + 0.08f * r, cx, cy - 0.95f * r};
        poly(o, s, t, 3, rgb);
        const float st[] = {cx - 0.1f * r, cy + 0.3f * r, cx + 0.1f * r, cy + 0.3f * r, cx + 0.34f * r, cy + r,
                            cx - 0.34f * r, cy + r};
        poly(o, s, st, 4, rgb);
        break;
    }
    default: {
        ell(o, s, cx, cy - 0.42f * r, 0.42f * r, 0.42f * r, rgb);
        ell(o, s, cx - 0.48f * r, cy + 0.16f * r, 0.42f * r, 0.42f * r, rgb);
        ell(o, s, cx + 0.48f * r, cy + 0.16f * r, 0.42f * r, 0.42f * r, rgb);
        const float st[] = {cx - 0.1f * r, cy, cx + 0.1f * r, cy, cx + 0.34f * r, cy + r, cx - 0.34f * r, cy + r};
        poly(o, s, st, 4, rgb);
        break;
    }
    }
}

static void clear_pic(uint8_t *o, int s) { memset(o, 0, (size_t)s * s * 4); }

static const uint8_t IVORY[3] = {244, 236, 216}, EBONY[3] = {38, 34, 32}, WALNUT[3] = {98, 64, 40},
                     AMBER[3] = {214, 150, 40}, BRASS[3] = {196, 160, 70}, STEEL[3] = {150, 158, 168},
                     LEAF[3] = {58, 140, 70}, SKY[3] = {120, 180, 220}, PINK[3] = {222, 150, 140},
                     STONE[3] = {128, 124, 118}, CREAMY[3] = {236, 226, 200}, LACQUER[3] = {166, 36, 34},
                     JADEC[3] = {40, 140, 100}, LAPIS[3] = {44, 84, 168}, PURPLE[3] = {112, 52, 140},
                     RUBY[3] = {200, 30, 50}, TOPAZ[3] = {230, 160, 40}, DARKW[3] = {64, 42, 28};

/* a charm's emblem on the medallion's ivory face (0.5, 0.5; about 0.3 across) */
static void emblem(uint8_t *o, int s, int id)
{
    switch (id) {
    case CH_RUBY:
        pip(o, s, SUIT_HEARTS, 0.5f, 0.5f, 0.24f, RUBY);
        ell_flat(o, s, 0.42f, 0.42f, 0.04f, 0.03f, WHITE);
        break;
    case CH_JET: pip(o, s, SUIT_SPADES, 0.5f, 0.5f, 0.24f, EBONY); break;
    case CH_TOPAZ:
        pip(o, s, SUIT_DIAMONDS, 0.5f, 0.5f, 0.25f, TOPAZ);
        ell_flat(o, s, 0.46f, 0.42f, 0.03f, 0.05f, WHITE);
        break;
    case CH_JADE: pip(o, s, SUIT_CLUBS, 0.5f, 0.5f, 0.24f, JADEC); break;
    case CH_PEBBLE:
        ell(o, s, 0.38f, 0.58f, 0.12f, 0.09f, STONE), ell(o, s, 0.6f, 0.6f, 0.1f, 0.08f, DIMC);
        ell(o, s, 0.5f, 0.42f, 0.11f, 0.085f, CREAMY);
        break;
    case CH_FEATHER: {
        const float f[] = {0.36f, 0.72f, 0.42f, 0.5f, 0.56f, 0.32f, 0.68f, 0.26f, 0.64f, 0.42f, 0.5f, 0.6f};
        poly(o, s, f, 6, CREAMY);
        band(o, s, 0.33f, 0.76f, 0.62f, 0.32f, 0.025f, WALNUT);
        break;
    }
    case CH_LENS:
        ring(o, s, 0.45f, 0.45f, 0.12f, 0.17f, BRASS), ell_flat(o, s, 0.45f, 0.45f, 0.12f, 0.12f, SKY);
        band(o, s, 0.57f, 0.57f, 0.72f, 0.72f, 0.07f, WALNUT);
        break;
    case CH_COMPASS:
        ring(o, s, 0.5f, 0.5f, 0.2f, 0.25f, BRASS);
        { const float n[] = {0.5f, 0.28f, 0.56f, 0.5f, 0.44f, 0.5f}; poly(o, s, n, 3, RUBY); }
        { const float n[] = {0.5f, 0.72f, 0.56f, 0.5f, 0.44f, 0.5f}; poly(o, s, n, 3, EBONY); }
        break;
    case CH_SEAL: {
        art_style st = art_player[4];
        ct_paint_piece(o, s, 6, &st, 0.5f, 0.0f);   /* a king */
        break;
    }
    case CH_BELL: case CH_GONG: {
        if (id == CH_GONG) {
            band(o, s, 0.28f, 0.74f, 0.32f, 0.28f, 0.04f, WALNUT);
            band(o, s, 0.72f, 0.74f, 0.68f, 0.28f, 0.04f, WALNUT);
            band(o, s, 0.27f, 0.29f, 0.73f, 0.29f, 0.04f, WALNUT);
            ell(o, s, 0.5f, 0.52f, 0.17f, 0.17f, BRASS), ring(o, s, 0.5f, 0.52f, 0.07f, 0.09f, AMBER);
            break;
        }
        const float b[] = {0.34f, 0.66f, 0.38f, 0.46f, 0.43f, 0.33f, 0.5f, 0.3f, 0.57f, 0.33f, 0.62f, 0.46f,
                           0.66f, 0.66f};
        poly(o, s, b, 7, AMBER);
        ell(o, s, 0.5f, 0.7f, 0.05f, 0.05f, WALNUT), ell(o, s, 0.5f, 0.27f, 0.03f, 0.03f, WALNUT);
        break;
    }
    case CH_MIRROR:
        band(o, s, 0.5f, 0.6f, 0.5f, 0.78f, 0.06f, WALNUT);
        ell(o, s, 0.5f, 0.44f, 0.16f, 0.2f, BRASS), ell_flat(o, s, 0.5f, 0.44f, 0.12f, 0.16f, SKY);
        band(o, s, 0.44f, 0.38f, 0.5f, 0.32f, 0.025f, WHITE);
        break;
    case CH_KNOT:
        ring(o, s, 0.42f, 0.44f, 0.08f, 0.115f, AMBER), ring(o, s, 0.58f, 0.44f, 0.08f, 0.115f, BRASS);
        ring(o, s, 0.5f, 0.58f, 0.08f, 0.115f, WALNUT);
        break;
    case CH_LADDER:
        band(o, s, 0.4f, 0.26f, 0.4f, 0.76f, 0.04f, WALNUT), band(o, s, 0.6f, 0.26f, 0.6f, 0.76f, 0.04f, WALNUT);
        for (int i = 0; i < 4; i++) band(o, s, 0.4f, 0.34f + 0.12f * i, 0.6f, 0.34f + 0.12f * i, 0.03f, AMBER);
        break;
    case CH_SHELL:
        for (int i = 0; i < 5; i++) {
            float a = (ART_PI * (0.15f + 0.175f * i));
            band(o, s, 0.5f, 0.66f, 0.5f - 0.24f * cosf(a), 0.66f - 0.3f * sinf(a), 0.07f, PINK);
        }
        arc(o, s, 0.5f, 0.66f, 0.26f, 0.3f, 190, 350, PINK);
        ell(o, s, 0.5f, 0.68f, 0.07f, 0.04f, PINK);
        break;
    case CH_PINES:
        for (int i = 0; i < 3; i++) {
            float cx = 0.34f + 0.16f * i, top = i == 1 ? 0.26f : 0.36f;
            const float t[] = {cx, top, cx + 0.1f, 0.68f, cx - 0.1f, 0.68f};
            poly(o, s, t, 3, LEAF);
            band(o, s, cx, 0.68f, cx, 0.74f, 0.025f, WALNUT);
        }
        break;
    case CH_CANDLE:
        rect(o, s, 0.43f, 0.44f, 0.57f, 0.74f, CREAMY);
        ell_flat(o, s, 0.5f, 0.35f, 0.05f, 0.085f, AMBER), ell_flat(o, s, 0.5f, 0.37f, 0.025f, 0.045f, WHITE);
        break;
    case CH_PALM: {
        ell(o, s, 0.5f, 0.6f, 0.15f, 0.14f, PINK);
        for (int i = 0; i < 4; i++) {
            float x = 0.39f + 0.075f * i;
            band(o, s, x, 0.55f, x, 0.3f + (i == 1 || i == 2 ? 0 : 0.05f), 0.055f, PINK);
        }
        band(o, s, 0.36f, 0.62f, 0.27f, 0.5f, 0.055f, PINK);
        break;
    }
    case CH_WORRY:
        ell(o, s, 0.5f, 0.5f, 0.2f, 0.15f, STONE), ell_flat(o, s, 0.52f, 0.49f, 0.08f, 0.05f, DIMC);
        break;
    case CH_ANCHOR:
        band(o, s, 0.5f, 0.3f, 0.5f, 0.72f, 0.05f, BRASS), band(o, s, 0.38f, 0.38f, 0.62f, 0.38f, 0.045f, BRASS);
        ring(o, s, 0.5f, 0.27f, 0.035f, 0.065f, BRASS);
        arc(o, s, 0.5f, 0.56f, 0.15f, 0.2f, 20, 160, BRASS);
        break;
    case CH_LAST: {
        ell(o, s, 0.5f, 0.5f, 0.2f, 0.2f, AMBER);
        ell_flat(o, s, 0.58f, 0.44f, 0.17f, 0.17f, IVORY);
        break;
    }
    case CH_WATER: {
        ell(o, s, 0.5f, 0.58f, 0.15f, 0.15f, SKY);
        const float t[] = {0.5f, 0.26f, 0.63f, 0.52f, 0.37f, 0.52f};
        poly(o, s, t, 3, SKY);
        ell_flat(o, s, 0.45f, 0.58f, 0.03f, 0.05f, WHITE);
        break;
    }
    case CH_SAND: {
        band(o, s, 0.34f, 0.27f, 0.66f, 0.27f, 0.04f, WALNUT), band(o, s, 0.34f, 0.73f, 0.66f, 0.73f, 0.04f, WALNUT);
        const float t1[] = {0.38f, 0.3f, 0.62f, 0.3f, 0.5f, 0.5f}, t2[] = {0.5f, 0.5f, 0.62f, 0.7f, 0.38f, 0.7f};
        poly(o, s, t1, 3, SKY), poly(o, s, t2, 3, SKY);
        const float sand[] = {0.5f, 0.56f, 0.6f, 0.7f, 0.4f, 0.7f};
        poly(o, s, sand, 3, TOPAZ);
        break;
    }
    case CH_MOTH:
        ell(o, s, 0.38f, 0.44f, 0.12f, 0.1f, STONE), ell(o, s, 0.62f, 0.44f, 0.12f, 0.1f, STONE);
        ell(o, s, 0.4f, 0.58f, 0.09f, 0.08f, DIMC), ell(o, s, 0.6f, 0.58f, 0.09f, 0.08f, DIMC);
        ell(o, s, 0.5f, 0.5f, 0.03f, 0.13f, WALNUT);
        break;
    case CH_PURSE:
        ell(o, s, 0.5f, 0.58f, 0.18f, 0.15f, WALNUT);
        band(o, s, 0.4f, 0.42f, 0.6f, 0.42f, 0.03f, AMBER);
        ell(o, s, 0.5f, 0.36f, 0.08f, 0.06f, WALNUT);
        ell(o, s, 0.5f, 0.6f, 0.05f, 0.05f, AMBER);
        break;
    case CH_BREATH:
        ell(o, s, 0.4f, 0.54f, 0.11f, 0.09f, SKY), ell(o, s, 0.55f, 0.47f, 0.13f, 0.11f, SKY);
        ell(o, s, 0.62f, 0.57f, 0.1f, 0.08f, SKY);
        break;
    case CH_SNOW:
        for (int i = 0; i < 3; i++) {
            float a = ART_PI * i / 3;
            band(o, s, 0.5f - 0.22f * cosf(a), 0.5f - 0.22f * sinf(a), 0.5f + 0.22f * cosf(a), 0.5f + 0.22f * sinf(a),
                 0.035f, SKY);
        }
        ell(o, s, 0.5f, 0.5f, 0.06f, 0.06f, SKY);
        break;
    case CH_SCRAP: case CH_LEDGER:
        rect(o, s, 0.33f, 0.3f, 0.67f, 0.72f, id == CH_SCRAP ? LACQUER : LAPIS);
        rect(o, s, 0.37f, 0.33f, 0.63f, 0.69f, CREAMY);
        for (int i = 0; i < 4; i++) band(o, s, 0.41f, 0.41f + 0.07f * i, 0.59f, 0.41f + 0.07f * i, 0.015f, DIMC);
        break;
    case CH_IVY:
        band(o, s, 0.36f, 0.74f, 0.62f, 0.26f, 0.03f, LEAF);
        for (int i = 0; i < 3; i++) {
            float t = 0.25f + 0.25f * i, x = 0.36f + 0.26f * t, y = 0.74f - 0.48f * t, side = i % 2 ? 1.0f : -1.0f;
            ell(o, s, x + 0.08f * side, y, 0.07f, 0.05f, LEAF);
        }
        break;
    case CH_TALLY:
        for (int i = 0; i < 4; i++) band(o, s, 0.36f + 0.09f * i, 0.32f, 0.36f + 0.09f * i, 0.68f, 0.035f, WALNUT);
        band(o, s, 0.3f, 0.62f, 0.7f, 0.38f, 0.035f, AMBER);
        break;
    case CH_POCKET: {
        const float p[] = {0.32f, 0.32f, 0.68f, 0.32f, 0.68f, 0.6f, 0.5f, 0.72f, 0.32f, 0.6f};
        poly(o, s, p, 5, LAPIS);
        band(o, s, 0.32f, 0.4f, 0.68f, 0.4f, 0.03f, CREAMY);
        break;
    }
    case CH_GLOVE:
        rect(o, s, 0.36f, 0.4f, 0.62f, 0.72f, WALNUT);
        ell(o, s, 0.49f, 0.4f, 0.13f, 0.12f, WALNUT);
        band(o, s, 0.62f, 0.56f, 0.71f, 0.44f, 0.08f, WALNUT);
        band(o, s, 0.36f, 0.68f, 0.62f, 0.68f, 0.04f, AMBER);
        break;
    case CH_SHEARS:
        band(o, s, 0.36f, 0.3f, 0.6f, 0.62f, 0.04f, STEEL), band(o, s, 0.64f, 0.3f, 0.4f, 0.62f, 0.04f, STEEL);
        ring(o, s, 0.36f, 0.68f, 0.04f, 0.075f, LACQUER), ring(o, s, 0.64f, 0.68f, 0.04f, 0.075f, LACQUER);
        break;
    case CH_DOOR:
        rect(o, s, 0.36f, 0.26f, 0.64f, 0.74f, WALNUT), rect(o, s, 0.4f, 0.3f, 0.6f, 0.74f, AMBER);
        ell(o, s, 0.56f, 0.52f, 0.025f, 0.025f, WALNUT);
        break;
    case CH_PIG:
        ell(o, s, 0.5f, 0.54f, 0.2f, 0.15f, PINK);
        { const float e[] = {0.38f, 0.42f, 0.42f, 0.3f, 0.47f, 0.41f}; poly(o, s, e, 3, PINK); }
        ell(o, s, 0.69f, 0.53f, 0.05f, 0.05f, RUBY), band(o, s, 0.44f, 0.4f, 0.56f, 0.4f, 0.025f, WALNUT);
        break;
    case CH_HAGGLE:
        ring(o, s, 0.5f, 0.56f, 0.12f, 0.17f, BRASS);
        { const float g[] = {0.5f, 0.28f, 0.58f, 0.36f, 0.5f, 0.44f, 0.42f, 0.36f}; poly(o, s, g, 4, RUBY); }
        break;
    case CH_CLOVER:
        ell(o, s, 0.5f, 0.38f, 0.09f, 0.09f, LEAF), ell(o, s, 0.38f, 0.5f, 0.09f, 0.09f, LEAF);
        ell(o, s, 0.62f, 0.5f, 0.09f, 0.09f, LEAF), ell(o, s, 0.5f, 0.62f, 0.09f, 0.09f, LEAF);
        band(o, s, 0.52f, 0.6f, 0.62f, 0.76f, 0.03f, LEAF);
        break;
    case CH_RAG:
        rect(o, s, 0.32f, 0.36f, 0.68f, 0.64f, SKY);
        for (int i = 0; i < 5; i++) band(o, s, 0.35f + 0.075f * i, 0.64f, 0.35f + 0.075f * i, 0.72f, 0.025f, SKY);
        band(o, s, 0.36f, 0.42f, 0.64f, 0.42f, 0.02f, WHITE);
        break;
    case CH_HAMMER:
        band(o, s, 0.5f, 0.36f, 0.5f, 0.76f, 0.05f, WALNUT);
        rect(o, s, 0.34f, 0.26f, 0.66f, 0.38f, STEEL);
        break;
    }
}

static const uint8_t *kind_colour(int kind)
{
    switch (kind) {
    case CK_CHIPS: return LAPIS;
    case CK_MULT: return LACQUER;
    case CK_XMULT: return PURPLE;
    case CK_COINS: return AMBER;
    default: return JADEC;
    }
}

/* a charm: a medallion, its rim in the colour of what it does, its rarity's gold dots, its emblem */
static void paint_charm(int id, int s, uint8_t *o)
{
    clear_pic(o, s);
    if (id < 0 || id >= CH_N) return;
    const pkr_charm_info *in = &pkr_charms[id];
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.47f, 0.47f);
    art_fig_paint(o, s, f, kind_colour(in->kind), EBONY, 0.012f, 0.55f);
    for (int i = 0; i <= in->rarity; i++) {
        float x = 0.5f + (i - in->rarity / 2.0f) * 0.1f;
        ell(o, s, x, 0.905f, 0.027f, 0.027f, GOLD);
    }
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.48f, 0.355f, 0.355f);
    art_fig_paint(o, s, f, IVORY, DARKW, 0.01f, 0.25f);
    emblem(o, s, id);
}

/* a boss: a lacquer medallion in a dark rim, its sign in gold (kind: boss * 4 + the suit for the fading suit) */
static void paint_boss(int kind, int s, uint8_t *o)
{
    clear_pic(o, s);
    int id = kind / 4, suit = kind % 4;
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.47f, 0.47f);
    art_fig_paint(o, s, f, EBONY, EBONY, 0.0f, 0.4f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.4f, 0.4f);
    art_fig_paint(o, s, f, LACQUER, DARKW, 0.012f, 0.45f);
    const uint8_t *g = GOLD;
    switch (id) {
    case BO_FADE:
        pip(o, s, suit, 0.5f, 0.5f, 0.2f, CREAMY);
        band(o, s, 0.3f, 0.7f, 0.7f, 0.3f, 0.045f, g);
        break;
    case BO_ROPE:
        for (int i = 0; i < 3; i++) ring(o, s, 0.38f + 0.12f * i, 0.5f, 0.06f, 0.095f, g);
        break;
    case BO_FIST:
        rect(o, s, 0.34f, 0.36f, 0.66f, 0.66f, g), rect(o, s, 0.38f, 0.4f, 0.62f, 0.44f, LACQUER);
        band(o, s, 0.34f, 0.52f, 0.66f, 0.52f, 0.02f, LACQUER);
        band(o, s, 0.66f, 0.6f, 0.72f, 0.46f, 0.07f, g);
        break;
    case BO_WELL: {
        ring(o, s, 0.5f, 0.58f, 0.1f, 0.14f, g);
        const float t[] = {0.5f, 0.26f, 0.62f, 0.5f, 0.38f, 0.5f};
        art_fig *d = art_fig_new();
        art_poly(d, ART_PART, t, 3);
        dye(o, s, d, g, 0.3f);
        band(o, s, 0.3f, 0.72f, 0.7f, 0.3f, 0.045f, CREAMY);
        break;
    }
    case BO_WALL:
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 3; c++) {
                float x = 0.29f + 0.14f * c + (r % 2 ? 0.07f : 0), y = 0.3f + 0.1f * r;
                rect(o, s, x, y, x + 0.12f, y + 0.08f, g);
            }
        break;
    case BO_FOG:
        ell(o, s, 0.38f, 0.56f, 0.12f, 0.1f, CREAMY), ell(o, s, 0.55f, 0.46f, 0.15f, 0.13f, CREAMY);
        ell(o, s, 0.64f, 0.58f, 0.11f, 0.09f, CREAMY);
        break;
    case BO_TIDE:
        for (int i = 0; i < 3; i++) arc(o, s, 0.38f + 0.12f * i, 0.6f, 0.07f, 0.11f, 180, 360, g);
        band(o, s, 0.28f, 0.66f, 0.72f, 0.66f, 0.03f, g);
        break;
    case BO_GATE:
        rect(o, s, 0.32f, 0.4f, 0.4f, 0.72f, g), rect(o, s, 0.6f, 0.4f, 0.68f, 0.72f, g);
        arc(o, s, 0.5f, 0.42f, 0.1f, 0.18f, 180, 360, g);
        break;
    case BO_TOLL:
        ell(o, s, 0.5f, 0.5f, 0.17f, 0.17f, g), ring(o, s, 0.5f, 0.5f, 0.1f, 0.12f, AMBER);
        band(o, s, 0.42f, 0.5f, 0.58f, 0.5f, 0.04f, LACQUER);
        break;
    case BO_RUST:
        for (int i = 0; i < 6; i++) {
            float a = ART_PI * i / 3;
            band(o, s, 0.5f, 0.5f, 0.5f + 0.2f * cosf(a), 0.5f + 0.2f * sinf(a), 0.08f, TOPAZ);
        }
        ell(o, s, 0.5f, 0.5f, 0.14f, 0.14f, TOPAZ), ell_flat(o, s, 0.5f, 0.5f, 0.05f, 0.05f, LACQUER);
        break;
    }
}

/* the small pictures */
enum { K_COIN, K_TARGET, K_FAST, K_BOOK, K_SUITS, K_SORTR, K_CYCLE, K_PACK0, K_PACK1, K_PACK2, K_TOME0 = 16,
       K_BLIND0 = 32, K_BLIND1, K_HANDS, K_TICK, K_SLASH };
static const uint8_t TOME_COLOUR[HT_N][3] = {{120, 110, 100}, {44, 84, 168}, {40, 120, 150}, {40, 140, 100},
                                             {150, 120, 40}, {170, 70, 40}, {166, 36, 34}, {112, 52, 140},
                                             {200, 160, 50}};
static void paint_misc(int kind, int s, uint8_t *o)
{
    clear_pic(o, s);
    if (kind >= K_TOME0 && kind < K_TOME0 + HT_N) {      /* a tome: a book in its hand type's colour */
        const uint8_t *c = TOME_COLOUR[kind - K_TOME0];
        rect(o, s, 0.24f, 0.16f, 0.78f, 0.84f, c);
        rect(o, s, 0.3f, 0.2f, 0.74f, 0.8f, c);
        rect(o, s, 0.72f, 0.2f, 0.78f, 0.8f, CREAMY);
        band(o, s, 0.27f, 0.16f, 0.27f, 0.84f, 0.04f, DARKW);
        star(o, s, 0.5f, 0.42f, 0.13f, GOLD);
        int n = kind - K_TOME0 + 1;             /* the hand type's rank: as many gold dots, in rows of five */
        for (int i = 0; i < n; i++) {
            int row = i / 5, in_row = n - row * 5 < 5 ? n - row * 5 : 5;
            ell(o, s, 0.51f + 0.065f * (i % 5 - (in_row - 1) / 2.0f), 0.64f + 0.065f * row, 0.022f, 0.022f, GOLD);
        }
        return;
    }
    switch (kind) {
    case K_COIN:
        ell(o, s, 0.5f, 0.5f, 0.46f, 0.46f, AMBER);
        ell(o, s, 0.5f, 0.5f, 0.38f, 0.38f, GOLD);
        ring(o, s, 0.5f, 0.5f, 0.24f, 0.29f, AMBER);
        break;
    case K_TARGET:
        ell(o, s, 0.5f, 0.5f, 0.46f, 0.46f, LACQUER), ell_flat(o, s, 0.5f, 0.5f, 0.33f, 0.33f, IVORY);
        ell_flat(o, s, 0.5f, 0.5f, 0.21f, 0.21f, LACQUER), ell_flat(o, s, 0.5f, 0.5f, 0.09f, 0.09f, IVORY);
        break;
    case K_FAST: {                              /* white: two chevrons */
        for (int i = 0; i < 2; i++) {
            float x = 0.24f + 0.26f * i;
            band(o, s, x, 0.24f, x + 0.22f, 0.5f, 0.11f, WHITE), band(o, s, x + 0.22f, 0.5f, x, 0.76f, 0.11f, WHITE);
        }
        break;
    }
    case K_BOOK: {                              /* white: an open book */
        const float l[] = {0.12f, 0.26f, 0.48f, 0.32f, 0.48f, 0.8f, 0.12f, 0.72f};
        const float r[] = {0.52f, 0.32f, 0.88f, 0.26f, 0.88f, 0.72f, 0.52f, 0.8f};
        poly(o, s, l, 4, WHITE), poly(o, s, r, 4, WHITE);
        break;
    }
    case K_SUITS:                               /* white: the four suits */
        pip(o, s, SUIT_SPADES, 0.32f, 0.32f, 0.15f, WHITE), pip(o, s, SUIT_HEARTS, 0.68f, 0.32f, 0.15f, WHITE);
        pip(o, s, SUIT_CLUBS, 0.32f, 0.68f, 0.15f, WHITE), pip(o, s, SUIT_DIAMONDS, 0.68f, 0.68f, 0.15f, WHITE);
        break;
    case K_SORTR: {                             /* white: three cards rising */
        for (int i = 0; i < 3; i++) {
            float x = 0.16f + 0.24f * i, h = 0.3f + 0.15f * i;
            rect(o, s, x, 0.86f - h, x + 0.2f, 0.86f, WHITE);
        }
        break;
    }
    case K_CYCLE: ct_pic_paint(CT_CYCLE, s, o); break;
    case K_PACK0: case K_PACK1: case K_PACK2: {
        int p = kind - K_PACK0;
        if (p == PK_CHARM) {                    /* a velvet pouch, a gold string */
            ell(o, s, 0.5f, 0.62f, 0.32f, 0.28f, PURPLE);
            const float n[] = {0.36f, 0.4f, 0.64f, 0.4f, 0.58f, 0.24f, 0.42f, 0.24f};
            poly(o, s, n, 4, PURPLE);
            band(o, s, 0.34f, 0.4f, 0.66f, 0.4f, 0.04f, GOLD);
            ell(o, s, 0.5f, 0.64f, 0.11f, 0.11f, GOLD), ell_flat(o, s, 0.5f, 0.64f, 0.06f, 0.06f, AMBER);
        } else if (p == PK_TOME) {              /* three books */
            rect(o, s, 0.18f, 0.6f, 0.82f, 0.78f, LAPIS), rect(o, s, 0.22f, 0.42f, 0.78f, 0.6f, LACQUER);
            rect(o, s, 0.2f, 0.24f, 0.8f, 0.42f, JADEC);
            for (int i = 0; i < 3; i++) band(o, s, 0.74f, 0.27f + 0.18f * i, 0.74f, 0.39f + 0.18f * i, 0.03f, CREAMY);
        } else {                                /* fanned backs tied with a ribbon */
            for (int i = 0; i < 3; i++) {
                float x = 0.22f + 0.12f * i;
                rect(o, s, x, 0.2f + 0.04f * i, x + 0.34f, 0.76f + 0.04f * i, WALNUT);
                rect(o, s, x + 0.03f, 0.23f + 0.04f * i, x + 0.31f, 0.73f + 0.04f * i, JADEC);
            }
            band(o, s, 0.18f, 0.56f, 0.82f, 0.56f, 0.06f, RUBY);
        }
        break;
    }
    case K_BLIND0: case K_BLIND1: {             /* a gaming chip: ivory or amber, its edge notched */
        const uint8_t *c = kind == K_BLIND0 ? IVORY : AMBER, *e = kind == K_BLIND0 ? LAPIS : LACQUER;
        ell(o, s, 0.5f, 0.5f, 0.47f, 0.47f, e);
        for (int i = 0; i < 8; i++) {
            float a = ART_PI * i / 4;
            band(o, s, 0.5f + 0.34f * cosf(a), 0.5f + 0.34f * sinf(a), 0.5f + 0.46f * cosf(a),
                 0.5f + 0.46f * sinf(a), 0.09f, c);
        }
        ell(o, s, 0.5f, 0.5f, 0.3f, 0.3f, c), ring(o, s, 0.5f, 0.5f, 0.27f, 0.29f, e);
        break;
    }
    case K_HANDS:                               /* white: three cards fanned */
        for (int i = 0; i < 3; i++) {
            float x = 0.2f + 0.16f * i;
            rect(o, s, x, 0.24f + (i == 1 ? -0.04f : 0), x + 0.3f, 0.76f + (i == 1 ? -0.04f : 0), WHITE);
        }
        break;
    case K_TICK: ct_pic_paint(CT_TICK, s, o); break;
    case K_SLASH: band(o, s, 0.3f, 0.82f, 0.7f, 0.18f, 0.1f, WHITE); break;
    }
}

/* an enhancement drawn over a card w wide (the picture s = 1.45 w square, the card in its middle): its frame and its
 * badge at the card's foot */
static void paint_overlay(int enh, int s, uint8_t *o)
{
    clear_pic(o, s);
    float w = s / 1.45f, h = w * 1.4f, x0 = (s - w) / 2, y0 = (s - h) / 2, t = fmaxf(2, 0.05f * w);
    static const uint8_t FR[EN_N][3] = {{0, 0, 0}, {226, 178, 50}, {120, 200, 240}, {140, 150, 162},
                                        {60, 170, 80}, {60, 110, 210}, {214, 60, 40}, {200, 120, 220}};
    const uint8_t *c = FR[enh];
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            float dx = x + 0.5f - x0, dy = y + 0.5f - y0;
            if (dx < 0 || dy < 0 || dx > w || dy > h) continue;
            float edge = fminf(fminf(dx, w - dx), fminf(dy, h - dy));
            /* the corners rounded as the kit's (0.075 w) */
            float rr = 0.075f * w, cx = fminf(fmaxf(dx, rr), w - rr), cy = fminf(fmaxf(dy, rr), h - rr);
            float d = sqrtf((dx - cx) * (dx - cx) + (dy - cy) * (dy - cy));
            if (d > rr) continue;
            if (d > rr - t && (dx < rr || dx > w - rr) && (dy < rr || dy > h - rr)) edge = 0;
            uint8_t *p = o + 4 * (y * s + x);
            float a = 0;
            uint8_t col[3] = {c[0], c[1], c[2]};
            if (edge < t) a = 1;
            else if (enh == EN_CRYSTAL) {       /* a sheen across */
                float k = (dx + dy * 0.6f) / w;
                a = 0.16f + 0.18f * (fmodf(k * 3, 1) < 0.25f);
                col[0] = 230, col[1] = 246, col[2] = 255;
            } else if (enh == EN_GILDED) a = 0.14f;
            else if (enh == EN_IRON) a = 0.18f, col[0] = col[1] = col[2] = 120;
            if (enh == EN_PRISM && edge < t) {  /* the rainbow round the rim */
                float k = (dx / w + dy / h) * 3;
                float r = 0.5f + 0.5f * sinf(k * 2.1f), g = 0.5f + 0.5f * sinf(k * 2.1f + 2.1f),
                      b = 0.5f + 0.5f * sinf(k * 2.1f + 4.2f);
                col[0] = (uint8_t)(255 * r), col[1] = (uint8_t)(255 * g), col[2] = (uint8_t)(255 * b);
            }
            p[0] = col[0], p[1] = col[1], p[2] = col[2], p[3] = (uint8_t)(255 * a);
        }
    /* the badge: a disc at the foot, its sign */
    float bx = 0.5f, by = (y0 + h - 0.02f * w) / s, br = 0.13f * w / s;
    ell(o, s, bx, by, br, br, EBONY);
    ell(o, s, bx, by, br * 0.8f, br * 0.8f, c);
    switch (enh) {
    case EN_GILDED: ring(o, s, bx, by, br * 0.35f, br * 0.5f, AMBER); break;
    case EN_CRYSTAL: {
        const float d[] = {bx, by - br * 0.5f, bx + br * 0.4f, by, bx, by + br * 0.5f, bx - br * 0.4f, by};
        poly(o, s, d, 4, WHITE);
        break;
    }
    case EN_IRON: rect(o, s, bx - br * 0.35f, by - br * 0.35f, bx + br * 0.35f, by + br * 0.35f, EBONY); break;
    case EN_CLOVER:
        for (int i = 0; i < 3; i++) {
            float a = ART_PI * (0.5f + 2 * i / 3.0f);
            ell(o, s, bx + br * 0.28f * cosf(a), by - br * 0.28f * sinf(a), br * 0.24f, br * 0.24f, WHITE);
        }
        break;
    case EN_ETCHED: ring(o, s, bx, by, br * 0.25f, br * 0.45f, WHITE); break;
    case EN_EMBER: ell(o, s, bx, by + br * 0.08f, br * 0.25f, br * 0.4f, GOLD); break;
    case EN_PRISM: star(o, s, bx, by, br * 0.5f, WHITE); break;
    }
}

/* ---- words ------------------------------------------------------------------------------------------------------- */
static int word(int i, char *buf, int size)
{
    if (!text_word(WORDS, i, buf, size)) { buf[0] = 0; return 0; }
    return 1;
}

/* a block of text (a name, bold and larger, then its words wrapped to w) as a picture, kept */
#define TEXTS 24
static struct { char s[512]; int w, px, light; SDL_Texture *t; int th; Uint32 used; } TX[TEXTS];

static SDL_Texture *text_block(const char *s, int w, float px, int *th)
{
    int ipx = (int)(px + 0.5f), light = ct_light();
    for (int i = 0; i < TEXTS; i++)
        if (TX[i].t && TX[i].w == w && TX[i].px == ipx && TX[i].light == light && !strcmp(TX[i].s, s)) {
            TX[i].used = ui_real_ms();
            *th = TX[i].th;
            return TX[i].t;
        }
    int k = 0;
    for (int i = 1; i < TEXTS; i++)
        if (!TX[i].t || (TX[k].t && TX[i].used < TX[k].used)) k = i;
    ui_free(&TX[k].t);
    int h = text_page(s, w, (float)ipx, panel_ink(), NULL, 0, 0);
    if (h <= 0) h = 1;
    uint8_t *b = calloc((size_t)w * h, 4);
    if (!b) return NULL;
    text_page(s, w, (float)ipx, panel_ink(), b, 0, h);
    TX[k].t = ui_tex(w, h, b);
    free(b);
    snprintf(TX[k].s, sizeof TX[k].s, "%s", s);
    TX[k].w = w, TX[k].px = ipx, TX[k].light = light, TX[k].th = h, TX[k].used = ui_real_ms();
    *th = h;
    return TX[k].t;
}

static void texts_free(void)
{
    for (int i = 0; i < TEXTS; i++) ui_free(&TX[i].t);
}

/* a line of the words drawn centred at (cx, cy), px high, no wider than maxw; its width */
static float word_at(int i, float cx, float cy, float px, float maxw, const uint8_t rgb[3])
{
    int w, h;
    SDL_Texture *t = ui_word(WORDS, i, px, maxw, rgb, &w, &h);
    if (!t) return 0;
    ui_put_c(t, cx, cy, (float)w, (float)h);
    return (float)w;
}

/* ---- numbers ---------------------------------------------------------------------------------------------------- */
static int clampi(int64_t v) { return v > 2000000000 ? 2000000000 : v < -2000000000 ? -2000000000 : (int)v; }

static float num_w(int64_t v, float h) { return ui_number_w(clampi(v < 0 ? -v : v), h); }

static void num(float cx, float cy, float h, int64_t v, const uint8_t rgb[3], float a)
{
    ui_number_ex(cx, cy, h, clampi(v < 0 ? -v : v), rgb, (Uint8)(255 * clampf(a, 0, 1)), 0);
}

/* a sign before a number: 1 plus, 2 times, 3 minus; h the digits' height */
static float sign_w(int sign, float h) { return sign ? 0.62f * h : 0; }

static void sign_at(int sign, float cx, float cy, float h, const uint8_t rgb[3], float a)
{
    Uint8 al = (Uint8)(255 * clampf(a, 0, 1));
    float t = fmaxf(2, floorf(0.14f * h + 0.5f)), s = floorf(0.48f * h + 0.5f);
    if (sign == 1 || sign == 3) ui_fill(floorf(cx - s / 2), floorf(cy - t / 2), s, t, rgb[0], rgb[1], rgb[2], al);
    if (sign == 1) ui_fill(floorf(cx - t / 2), floorf(cy - s / 2), t, s, rgb[0], rgb[1], rgb[2], al);
    if (sign == 2) ct_pic(CT_CROSS, cx, cy, 0.62f * h, 0, rgb[0], rgb[1], rgb[2], al);
}

/* a value: a sign and a number (hundredths: a times-mult, 1.25), centred */
static float value_w(int sign, int64_t v, int hundredths, float h)
{
    float w = sign_w(sign, h) + (sign ? 0.08f * h : 0);
    if (!hundredths) return w + num_w(v, h);
    int64_t ip = v / 100, fp = v % 100;
    w += num_w(ip, h);
    if (fp) w += 0.3f * h + num_w(fp % 10 ? fp : fp / 10, h);
    return w;
}

static void value_at(int sign, int64_t v, int hundredths, float cx, float cy, float h, const uint8_t rgb[3], float a)
{
    float w = value_w(sign, v, hundredths, h), x = cx - w / 2;
    if (sign) sign_at(sign, x + sign_w(sign, h) / 2, cy, h, rgb, a), x += sign_w(sign, h) + 0.08f * h;
    if (!hundredths) { num(x + num_w(v, h) / 2, cy, h, v, rgb, a); return; }
    int64_t ip = v / 100, fp = v % 100;
    num(x + num_w(ip, h) / 2, cy, h, ip, rgb, a);
    x += num_w(ip, h);
    if (fp) {
        float d = fmaxf(2, 0.14f * h);
        ui_fill(floorf(x + 0.15f * h - d / 2), floorf(cy + 0.5f * h - d), d, d, rgb[0], rgb[1], rgb[2],
                (Uint8)(255 * clampf(a, 0, 1)));
        x += 0.3f * h;
        int64_t f = fp % 10 ? fp : fp / 10;
        if (fp < 10) {                          /* 1.05: the zero first */
            num(x + num_w(0, h) / 2, cy, h, 0, rgb, a);
            x += num_w(0, h);
            f = fp;
        }
        num(x + num_w(f, h) / 2, cy, h, f, rgb, a);
    }
}

/* ---- the layout ------------------------------------------------------------------------------------------------- */
static struct {
    box b;
    int land;
    float u;
    box ch, hud, play, hand, btn, scr;          /* the charms; the blind's numbers; the play area; the hand; its
                                                 * buttons; the room of the other screens (all but the charms) */
    float chd, chx[PKR_CHARMS], chy;            /* the charms: their size, centres */
    float hw, hcx, hcy, hwidth, arc;            /* the hand: card width, the fan */
    float pw, py, pcx;                          /* the cards played: width, the row's centre */
    float pilex, piley, pilew;                  /* the draw pile */
    float bd, bplay[2], bdisc[2], bsr[2], bss[2];   /* the buttons */
    float bossx, bossy, bossd;                  /* the boss's medallion in the blind's numbers */
} L;

static void layout(box b)
{
    L.b = b;
    L.land = b.w > b.h * 1.05f;
    float u = L.u = L.land ? fminf(b.w / 11.5f, b.h / 8.6f) : fminf(b.w / 9, b.h / 11.2f);
    if (!L.land) {
        float h = b.h, y = b.y;
        float hch = fminf(1.55f * u, 0.15f * h), hhud = fminf(1.6f * u, 0.15f * h), hbtn = fminf(1.55f * u, 0.15f * h);
        float rest = h - hch - hhud - hbtn, hplay = rest * 0.5f, hhand = rest - hplay;
        L.ch = (box){b.x, y, b.w, hch};
        L.hud = (box){b.x, y + hch, b.w, hhud};
        L.play = (box){b.x, y + hch + hhud, b.w, hplay};
        L.hand = (box){b.x, L.play.y + hplay, b.w, hhand};
        L.btn = (box){b.x, L.hand.y + hhand, b.w, hbtn};
        L.hw = floorf(fminf(b.w * 0.96f / (1 + 7 * 0.74f), hhand * 0.66f / 1.4f));
        L.hwidth = fminf(b.w * 0.97f, L.hw * (1 + 7 * 0.74f) * 1.02f);
        L.hcx = b.x + b.w / 2;
        L.hcy = L.hand.y + hhand * 0.56f;
        L.arc = 9;
        L.bd = fminf(1.25f * u, hbtn * 0.82f);
        float by = L.btn.y + hbtn * 0.5f, cx = b.x + b.w / 2;
        L.bplay[0] = cx - 0.75f * L.bd, L.bplay[1] = by;
        L.bdisc[0] = cx + 0.75f * L.bd, L.bdisc[1] = by;
        L.bsr[0] = cx - 2.35f * L.bd, L.bsr[1] = by;
        L.bss[0] = cx + 2.35f * L.bd, L.bss[1] = by;
    } else {
        float h = b.h, y = b.y;
        float htop = fminf(1.6f * u, 0.19f * h), rest = h - htop, hplay = rest * 0.44f, hhand = rest - hplay;
        float side = 1.6f * u;
        L.ch = (box){b.x, y, b.w * 0.5f, htop};
        L.hud = (box){b.x + b.w * 0.5f, y, b.w * 0.5f, htop};
        L.play = (box){b.x, y + htop, b.w, hplay};
        L.hand = (box){b.x + side, y + htop + hplay, b.w - 2 * side, hhand};
        L.btn = (box){b.x, L.hand.y, b.w, hhand};
        L.hw = floorf(fminf(L.hand.w * 0.96f / (1 + 7 * 0.74f), hhand * 0.7f / 1.4f));
        L.hwidth = fminf(L.hand.w * 0.97f, L.hw * (1 + 7 * 0.74f) * 1.02f);
        L.hcx = L.hand.x + L.hand.w / 2;
        L.hcy = L.hand.y + hhand * 0.55f;
        L.arc = 8;
        L.bd = fminf(1.2f * u, hhand * 0.3f);
        float lx = b.x + side / 2, rx = b.x + b.w - side / 2, my = L.hand.y + hhand * 0.5f;
        L.bplay[0] = rx, L.bplay[1] = my - 0.62f * L.bd;
        L.bdisc[0] = rx, L.bdisc[1] = my + 0.62f * L.bd;
        L.bsr[0] = lx, L.bsr[1] = my - 0.62f * L.bd;
        L.bss[0] = lx, L.bss[1] = my + 0.62f * L.bd;
    }
    L.scr = (box){b.x, L.ch.y + L.ch.h, b.w, b.y + b.h - (L.ch.y + L.ch.h)};
    if (L.land) L.scr = (box){b.x, b.y + L.ch.h, b.w, b.h - L.ch.h};
    /* the charms */
    L.chd = fminf(L.ch.h * 0.8f, L.ch.w / (PKR_CHARMS + 0.6f));
    L.chy = L.ch.y + L.ch.h * 0.5f;
    for (int i = 0; i < PKR_CHARMS; i++) L.chx[i] = L.ch.x + L.ch.w * (i + 0.5f) / PKR_CHARMS;
    /* the play area: the cards played, the pile */
    L.pilew = floorf(fminf(L.hw * 0.72f, L.play.h * 0.36f / 1.4f));
    L.pilex = L.play.x + L.play.w - 0.75f * L.pilew - 0.15f * u;
    float room = L.pilex - 0.7f * L.pilew - L.play.x - 0.2f * u;   /* left of the pile: five cards */
    L.pw = floorf(fminf(fminf(L.hw * 1.0f, L.play.h * 0.5f / 1.4f), room / 5.5f));
    L.py = L.play.y + L.play.h * 0.4f;
    L.pcx = L.play.x + 0.1f * u + room / 2;
    L.piley = L.py;
    L.bossd = L.hud.h * 0.62f;
    L.bossx = L.hud.x + L.hud.h * 0.5f + 0.95f * L.hud.h * 0.62f;
    L.bossy = L.hud.y + L.hud.h * 0.5f;
    texts_free();
}

/* ---- the cards' motion ------------------------------------------------------------------------------------------ */
typedef struct { float x, y, deg, w; } vpos;
static struct { vpos from, to; Uint32 t0; int dur, on; } V[PKR_DECK_MAX];

static float speedk(void) { return fast ? 0.45f : 1.0f; }

static vpos vnow(int i)
{
    float t = V[i].dur > 0 ? clampf((float)(int)(ui_ms() - V[i].t0) / V[i].dur, 0, 1) : 1;
    if ((int)(ui_ms() - V[i].t0) < 0) t = 0;
    float e = ui_ease(t);
    vpos a = V[i].from, b = V[i].to, r;
    r.x = a.x + (b.x - a.x) * e, r.y = a.y + (b.y - a.y) * e, r.deg = a.deg + (b.deg - a.deg) * e;
    r.w = a.w + (b.w - a.w) * e;
    return r;
}

static int vmoving(int i) { return V[i].on && (int)(ui_ms() - V[i].t0) < V[i].dur; }

/* card i goes to `to` (from the pile when it had no place yet), after delay ms */
static vpos vgo(int i, vpos to, int ms, int delay)
{
    if (!V[i].on) {
        V[i].on = 1;
        V[i].from = (vpos){L.pilex, L.piley, 0, L.pilew};
        V[i].to = to;
        V[i].t0 = ui_ms() + (Uint32)delay;
        V[i].dur = ms;
        return vnow(i);
    }
    if (fabsf(V[i].to.x - to.x) > 0.5f || fabsf(V[i].to.y - to.y) > 0.5f || fabsf(V[i].to.deg - to.deg) > 0.2f ||
        fabsf(V[i].to.w - to.w) > 0.5f) {
        V[i].from = vnow(i);
        V[i].to = to;
        V[i].t0 = ui_ms() + (Uint32)delay;
        V[i].dur = ms;
    }
    return vnow(i);
}

static void vsnap(void) { memset(V, 0, sizeof V); }

/* cards flying away (discarded, played): drawn until they are gone */
#define LEAVING 16
static struct { pkr_card c; vpos from, to; Uint32 t0; int dur, on; } LV[LEAVING];

static void leave(pkr_card c, vpos from, vpos to, int ms, int delay)
{
    for (int i = 0; i < LEAVING; i++)
        if (!LV[i].on || (int)(ui_ms() - LV[i].t0) > LV[i].dur) {
            LV[i].on = 1, LV[i].c = c, LV[i].from = from, LV[i].to = to;
            LV[i].t0 = ui_ms() + (Uint32)delay, LV[i].dur = ms;
            return;
        }
}

/* ---- the scoring's animation ---------------------------------------------------------------------------------- */
static struct {
    int on;
    Uint32 t0;
    pkr_state b;                                /* the state before the play */
    pkr_log log;
    int st[PKR_STEPS + 1], t_base, t_total, t_fill, t_off, t_end, sounded, won;
} A;

static int an_t(void) { return A.on ? (int)(ui_ms() - A.t0) : 1 << 30; }
static int scoring(void) { return A.on && an_t() < A.t_end; }

static void score_anim(const pkr_state *before)
{
    A.on = 1;
    A.t0 = ui_ms();
    A.b = *before;
    A.log = G.s.log;
    A.won = G.s.phase == PH_CASH;
    float k = speedk();
    int t = (int)(320 * k) + 40 * A.log.n;     /* the cards fly to the middle */
    A.t_base = t;
    t += (int)(420 * k);
    for (int i = 0; i < A.log.nsteps; i++) {
        A.st[i] = i == 0 ? A.t_base : t;
        if (i == 0) continue;
        const pkr_step *s = &A.log.step[i];
        t += (int)((s->kind == SS_AGAIN ? 260 : s->kind == SS_DEBUFF ? 380 : 400) * k);
    }
    A.st[A.log.nsteps] = t;
    A.t_total = t + (int)(120 * k);
    A.t_fill = A.t_total + (int)(700 * k);
    A.t_off = A.t_fill + (int)(520 * k);
    A.t_end = A.t_off + (int)(200 * k);
    A.sounded = 0;
}

/* the step showing at time t (-1: none yet) */
static int step_at(int t)
{
    int k = -1;
    for (int i = 0; i < A.log.nsteps; i++)
        if (t >= A.st[i]) k = i;
    return k;
}

static void anim_sounds(void)
{
    if (!scoring()) return;
    int t = an_t();
    while (A.sounded < A.log.nsteps && t >= A.st[A.sounded]) {
        const pkr_step *s = &A.log.step[A.sounded];
        float v = fminf(1, 0.45f + 0.04f * A.sounded);
        if (s->kind == SS_BASE) ui_sound(SND_MOVE);
        else if (s->kind == SS_DEBUFF) ui_sound_soft(SND_DENY, 0.5f);
        else if (s->kind == SS_BREAK) ui_sound(SND_CAPTURE);
        else if (s->what == SV_CHIPS) ui_sound_soft(SND_PLACE, v);
        else if (s->what == SV_MULT) ui_sound_soft(SND_TAP, v);
        else if (s->what == SV_XMULT) ui_sound_soft(SND_CHECK, v);
        else if (s->what == SV_COINS) ui_sound_soft(SND_DROP, v);
        A.sounded++;
    }
    if (A.sounded == A.log.nsteps && t >= A.t_total) {
        ui_sound(SND_CAPTURE);
        A.sounded++;
    }
    if (A.sounded == A.log.nsteps + 1 && t >= A.t_fill) {
        ui_sound(A.won ? SND_END : SND_DROP);
        A.sounded++;
    }
}

/* ---- the run's records ------------------------------------------------------------------------------------------ */
static void read_records(void)
{
    uint8_t b[8] = {0};
    best_ante = ui_save_get("pokerrun/best", b, 1) == 1 ? b[0] : 0;
    wins = ui_save_get("pokerrun/wins", b, 2) == 2 ? b[0] | b[1] << 8 : 0;
    seen_kept = 0;
    if (ui_save_get("pokerrun/seen", b, 8) == 8)
        for (int i = 0; i < 8; i++) seen_kept |= (uint64_t)b[i] << (8 * i);
}

static void keep_records(int won_now)
{
    uint8_t b[8];
    if (G.s.seen & ~seen_kept) {
        seen_kept |= G.s.seen;
        for (int i = 0; i < 8; i++) b[i] = (uint8_t)(seen_kept >> (8 * i));
        ui_save_set("pokerrun/seen", b, 8);
    }
    int reached = G.s.ante;
    if (reached > best_ante) {
        best_ante = reached > 255 ? 255 : reached;
        b[0] = (uint8_t)best_ante;
        ui_save_set("pokerrun/best", b, 1);
    }
    if (won_now) {
        wins++;
        b[0] = (uint8_t)wins, b[1] = (uint8_t)(wins >> 8);
        ui_save_set("pokerrun/wins", b, 2);
    }
}

/* ---- the actions ------------------------------------------------------------------------------------------------ */
static void clear_chosen(void) { memset(chosen, 0, sizeof chosen); }

static unsigned chosen_mask(const pkr_state *s)
{
    unsigned m = 0;
    for (int i = 0; i < s->nhand; i++)
        if (chosen[s->hand[i]]) m |= 1u << i;
    return m;
}

static void hand_spots(const pkr_state *s, int n, const uint8_t *hand, unsigned raised, cards_spot *sp);

static void act(int a)
{
    static pkr_state before;
    before = G.s;
    if (!pkr_play(&G, a)) { ui_sound(SND_DENY); return; }
    int k = PKR_KIND(a);
    info = INF_NONE;
    switch (k) {
    case A_BEGIN:
        vsnap();
        memset(LV, 0, sizeof LV);
        clear_chosen();
        ui_sound(SND_MOVE);
        break;
    case A_PLAY:
        clear_chosen();
        score_anim(&before);
        break;
    case A_DISCARD: {
        cards_spot sp[PKR_HAND_MAX];
        hand_spots(&before, before.nhand, before.hand, (unsigned)PKR_ARG(a), sp);
        int j = 0;
        for (int i = 0; i < before.nhand; i++)
            if (PKR_ARG(a) >> i & 1) {
                int di = before.hand[i];
                vpos from = V[di].on ? vnow(di) : (vpos){sp[i].x, sp[i].y, sp[i].deg, L.hw};
                vpos to = {L.b.x + L.b.w + L.hw, L.hand.y - 0.2f * L.hw * (j + 1), 40, L.hw * 0.9f};
                leave(before.deck[di], from, to, (int)(420 * speedk()), 40 * j);
                V[di].on = 0;
                chosen[di] = 0;
                j++;
            }
        ui_sound(SND_MOVE);
        break;
    }
    case A_SORT: ui_sound(SND_TAP); break;
    case A_CASH: ui_sound(SND_DROP); break;
    case A_BUY: case A_PICK: ui_sound(SND_PLACE); break;
    case A_SELL: ui_sound(SND_DROP); break;
    case A_REROLL: ui_sound(SND_DICE); break;
    case A_ENH: ui_sound(SND_PLACE); break;
    default: ui_sound(SND_TAP); break;
    }
    keep_records(!before.won && G.s.won);
}

/* ---- the position ----------------------------------------------------------------------------------------------- */
static void start(int v)
{
    (void)v;
    uint8_t b = 0;
    if (ui_replaying()) fast = ui_save_get("pokerrun/fast0", &b, 1) == 1 ? b : 0;
    else {
        fast = ui_save_get("pokerrun/fast", &b, 1) == 1 ? b : 0;
        b = (uint8_t)fast;
        ui_save_set("pokerrun/fast0", &b, 1);
    }
    read_records();
    pkr_new(&G, (uint32_t)ui_random(1 << 30));
    memset(&A, 0, sizeof A);
    vsnap();
    memset(LV, 0, sizeof LV);
    clear_chosen();
    enh_mask = 0;
    info = INF_NONE;
    keep_records(0);
}

static int status(void)
{
    if (!pkr_over(&G.s)) return GS_PLAY;
    return G.s.won ? GS_WIN0 : GS_WIN1;
}

static int turn(void) { return 0; }
static int score(void) { return G.s.ante; }

static int busy(void)
{
    if (scoring()) return 1;
    for (int i = 0; i < PKR_DECK_MAX; i++)
        if (vmoving(i)) return 1;
    for (int i = 0; i < LEAVING; i++)
        if (LV[i].on && (int)(ui_ms() - LV[i].t0) < LV[i].dur) return 1;
    return 0;
}

/* moves: played at once; a "!" at the end: the last one animated as a tap plays it */
static int script(const char *s)
{
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", s);
    int n = (int)strlen(buf), animate = n > 0 && buf[n - 1] == '!';
    if (animate) buf[--n] = 0;
    char *last = animate ? strrchr(buf, ' ') : NULL;
    int ok = 1;
    if (animate) {
        char *tail = last ? last + 1 : buf;
        if (last) *last = 0, ok = pkr_script(&G, buf);
        pkr_game t = G;
        if (ok && pkr_script(&t, tail) && t.n == G.n + 1) {
            vsnap();
            act(t.act[G.n]);
        } else ok = 0;
    } else {
        ok = pkr_script(&G, buf);
        vsnap();
        memset(&A, 0, sizeof A);
    }
    clear_chosen();
    enh_mask = 0;
    keep_records(0);
    return ok;
}

/* ---- drawing: cards --------------------------------------------------------------------------------------------- */
static void card_at(pkr_card c, float cx, float cy, float w, float deg, int how, float alpha)
{
    cards_draw_alpha(DECK_FRENCH, c.card, cx, cy, w, deg, how, alpha);
    if (c.enh && !(how & CD_DOWN) && c.enh < EN_N) {
        float s = 1.45f * w;
        SDL_Texture *t = ct_picture(paint_overlay, c.enh, (int)(s + 0.5f));
        if (!t) return;
        if (alpha < 1) SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(alpha, 0, 1)));
        if (how & CD_DIM) SDL_SetTextureColorMod(t, 150, 150, 150);
        ui_put_rot(t, cx, cy, s, s, deg);
        SDL_SetTextureAlphaMod(t, 255);
        SDL_SetTextureColorMod(t, 255, 255, 255);
    }
}

static void hand_spots(const pkr_state *s, int n, const uint8_t *hand, unsigned raised, cards_spot *sp)
{
    (void)s, (void)hand;
    cards_fan(L.hcx, L.hcy, L.hwidth, L.hw, 1.4f, n, n > 1 ? L.arc : 0, sp);
    for (int i = 0; i < n; i++)
        if (raised >> i & 1) {
            float a = sp[i].deg * ART_PI / 180, k = 0.3f * L.hw;
            sp[i].x += sinf(a) * k, sp[i].y -= cosf(a) * k;
        }
}

/* the cards played, in the middle: card k of n */
static void play_spot(int k, int n, float *x, float *y)
{
    float step = L.pw * 1.08f;
    *x = L.pcx + (k - (n - 1) / 2.0f) * step;
    *y = L.py;
}

static int debuffed(const pkr_state *s, int di)
{
    return s->blind == 2 && s->boss == BO_FADE && CARD_SUIT(s->deck[di].card) == s->boss_suit &&
           s->deck[di].enh != EN_PRISM;
}

/* the hand: its cards (the chosen raised), moving to their places */
static void draw_hand(const pkr_state *s, const uint8_t *hand, int n, unsigned raised, int dim_all)
{
    cards_spot sp[PKR_HAND_MAX];
    hand_spots(s, n, hand, raised, sp);
    int ms = (int)(260 * speedk());
    for (int i = 0; i < n; i++) {
        int di = hand[i];
        vpos p = vgo(di, (vpos){sp[i].x, sp[i].y, sp[i].deg, L.hw}, ms, 50 * i);
        int how = (raised >> i & 1 ? CD_GLOW : 0) | (s->hid[di] ? CD_DOWN : 0) | (dim_all ? CD_DIM : 0);
        if (debuffed(s, di) && !(how & CD_DOWN)) how |= CD_DIM;
        if ((int)(ui_ms() - V[di].t0) < 0 && V[di].on) continue;   /* not dealt yet */
        card_at(s->deck[di], p.x, p.y, p.w, p.deg, how, 1);
    }
}

static void draw_leaving(void)
{
    for (int i = 0; i < LEAVING; i++) {
        if (!LV[i].on) continue;
        int t = (int)(ui_ms() - LV[i].t0);
        if (t >= LV[i].dur) { LV[i].on = 0; continue; }
        float f = t < 0 ? 0 : ui_ease((float)t / LV[i].dur);
        vpos a = LV[i].from, b = LV[i].to;
        card_at(LV[i].c, a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.w + (b.w - a.w) * f,
                a.deg + (b.deg - a.deg) * f, CD_LIFT, 1);
        ui_animate();
    }
}

/* ---- drawing: the charms ---------------------------------------------------------------------------------------- */
static float wiggle(int j, float *scale)
{
    *scale = 1;
    if (!scoring()) return 0;
    int t = an_t(), best = -1;
    for (int i = 0; i < A.log.nsteps; i++) {
        const pkr_step *s = &A.log.step[i];
        int who = s->kind == SS_CHARM ? s->idx : s->kind == SS_CARD && s->src >= 2 ? s->src - 2 : -1;
        if (who == j && t >= A.st[i] && t < A.st[i] + 420) best = i;
    }
    if (best < 0) return 0;
    float f = (float)(t - A.st[best]) / 420;
    *scale = 1 + 0.18f * sinf(ART_PI * fminf(1, f * 2.2f));
    return 16 * sinf(f * ART_PI * 4) * (1 - f);
}

static void draw_charm_slot(int i, const pkr_charm *c, int lit)
{
    float x = L.chx[i], y = L.chy, d = L.chd;
    if (!c) {
        SDL_Texture *t = ct_disc(ct_light() ? (const uint8_t[3]){150, 130, 100} : (const uint8_t[3]){0, 0, 0},
                                 ct_light() ? 25 : 30, (int)d);
        ui_put_c(t, x, y, d * 0.9f, d * 0.9f);
        return;
    }
    float sc, deg = wiggle(i, &sc);
    if (CD.on && CD.moved && CD.from == i) {    /* under the finger: lifted, bigger */
        x = CD.x, y = CD.y, sc = 1.15f;
        ui_shadow_flat(x + 0.08f * d, y + 0.12f * d, d * sc, d * sc, 0.5f);
    } else {
        if (lit) ui_mark_gold(x, y, d * 1.35f);
        ui_shadow_flat(x, y, d * sc, d * sc, 0.6f);
    }
    ui_put_rot(ct_picture(paint_charm, c->id, (int)d), x, y, d * sc, d * sc, deg);
    int v = c->id == CH_SNOW || c->id == CH_TALLY || c->id == CH_SCRAP ? c->val : 0;
    if (v > 0) ct_badge(x + 0.36f * d, y + 0.36f * d, 0.42f * d, v);
    if (c->id == CH_IVY && c->val > 100) {      /* the ivy's times-mult now */
        float h = 0.26f * d, w = value_w(2, c->val, 1, h) + 0.4f * h;
        round_box(x + 0.36f * d - w / 2, y + 0.36f * d - 0.6f * h, w, 1.2f * h, (const uint8_t[3]){28, 30, 28}, 230);
        value_at(2, c->val, 1, x + 0.36f * d, y + 0.36f * d, h, WHITE, 1);
    }
}

static void draw_charms(const pkr_state *s)
{
    int drag = CD.on && CD.moved && CD.from < s->ncharms;
    if (drag) ui_mark_gold_a(L.chx[charm_slot_at(CD.x)], L.chy, L.chd * 1.35f, 0.7f);   /* where it would go */
    for (int i = 0; i < PKR_CHARMS; i++)
        if (!(drag && i == CD.from))
            draw_charm_slot(i, i < s->ncharms ? &s->charm[i] : NULL, info == INF_CHARM && info_i == i);
    if (drag) draw_charm_slot(CD.from, &s->charm[CD.from], 0);
}

/* ---- drawing: the blind's numbers ------------------------------------------------------------------------------- */
static void blind_disc(int blind, int boss, int suit, float x, float y, float d, float alpha)
{
    SDL_Texture *t = blind == 2 ? ct_picture(paint_boss, boss * 4 + suit, (int)d)
                                : ct_picture(paint_misc, K_BLIND0 + blind, (int)d);
    if (alpha < 1) SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    ui_put_c(t, x, y, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a count in a coloured disc, with its picture above it */
static void counter(float x, float y, float d, int v, const uint8_t rgb[3], int pic)
{
    round_box(x - d / 2, y - d / 2, d, d, rgb, 255);
    num(x, y + 0.12f * d, 0.44f * d, v, WHITE, 1);
    SDL_Texture *t = pic == K_HANDS ? ct_icon(ICON_PLAY, (int)(0.3f * d), 255, 255, 255)
                                    : ct_picture(paint_misc, pic, (int)(0.3f * d));
    ui_put_c(t, x, y - 0.27f * d, 0.26f * d, 0.26f * d);
}

static void draw_hud(const pkr_state *s, int64_t score, int hands, int discards)
{
    box h = L.hud;
    float d = h.h * 0.7f, x = h.x + h.h * 0.5f, cy = h.y + h.h * 0.5f;
    blind_disc(s->blind, s->boss, s->boss_suit, x, cy, d, 1);
    if (info == INF_BOSS) ui_mark_gold(x, cy, d * 1.3f);
    /* the target and the bar */
    float cd = fminf(h.h * 0.62f, 1.1f * L.u), right = h.x + h.w - 0.1f * L.u, cx2 = right - cd * 0.55f,
          cx1 = cx2 - cd * 1.15f;
    float bx = x + d * 0.62f, bw = cx1 - cd * 0.7f - bx, nh = fminf(0.36f * h.h, 0.5f * L.u);
    nh = fminf(nh, bw / (1.3f + num_w(s->target, 1)));   /* a long target: smaller */
    float ty = h.y + h.h * 0.3f;
    SDL_Texture *tg = ct_picture(paint_misc, K_TARGET, (int)nh);
    if (score >= s->target) ui_mark_gold(bx + nh * 0.55f, ty, nh * 2.2f);   /* reached */
    ui_put_c(tg, bx + nh * 0.55f, ty, nh, nh);
    num(bx + nh * 1.3f + num_w(s->target, nh) / 2, ty, nh, s->target, ink(), 1);
    float by = h.y + h.h * 0.68f, bh = fmaxf(0.26f * h.h, 0.3f * L.u);
    const uint8_t back[3] = {0, 0, 0};
    round_box(bx, by - bh / 2, bw, bh, ct_light() ? (const uint8_t[3]){210, 200, 180} : back, ct_light() ? 255 : 110);
    double f = s->target > 0 ? (double)score / (double)s->target : 0;
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    if (f > 0) round_box(bx, by - bh / 2, fmaxf((float)(bw * f), bh), bh, GOLD, 255);
    num(bx + bw / 2, by, bh * 0.7f, score, ct_light() || f > 0.5 ? CT_INK : CT_CREAM, 1);
    counter(cx1, cy, cd, hands, BLUE, K_HANDS);
    counter(cx2, cy, cd, discards, RED, K_CYCLE);
}

/* the chips and the mult (box: their row), the hand's name and level over them */
static void draw_score_boxes(int type, int level, int64_t chips, int64_t mult, float pop_c, float pop_m, float alpha)
{
    box p = L.play;
    float bh = fminf(0.2f * p.h, 0.85f * L.u), bw = fminf(p.w * 0.3f, 2.6f * L.u), cy = p.y + p.h - bh * 0.62f;
    float cx = L.pcx, gap = 0.55f * bh;
    float x1 = cx - gap / 2 - bw / 2, x2 = cx + gap / 2 + bw / 2;
    Uint8 a = (Uint8)(255 * alpha);
    float sc = 1 + 0.15f * pop_c, sm = 1 + 0.15f * pop_m;
    round_box(x1 - bw * sc / 2, cy - bh * sc / 2, bw * sc, bh * sc, BLUE, a);
    round_box(x2 - bw * sm / 2, cy - bh * sm / 2, bw * sm, bh * sm, RED, a);
    float nh = bh * 0.56f;
    num(x1, cy, nh * sc * fminf(1, bw * 0.86f / fmaxf(1, num_w(chips, nh))), chips, WHITE, alpha);
    num(x2, cy, nh * sm * fminf(1, bw * 0.86f / fmaxf(1, num_w(mult, nh))), mult, WHITE, alpha);
    sign_at(2, cx, cy, bh * 0.6f, ink(), alpha);
    if (type >= 0) {                            /* the name, and the level's star */
        float ty = cy - bh * 0.95f, px = fminf(0.42f * bh + 10, 0.42f * L.u);
        float w = word_at(W_HAND + type, cx - 0.4f * px, ty, px, p.w * 0.5f, ink());
        if (level > 1) {
            float sx = cx - 0.4f * px + w / 2 + 0.7f * px;
            ui_star(sx, ty, 0.9f * px, 1);
            num(sx + 0.55f * px + num_w(level, 0.7f * px) / 2, ty, 0.7f * px, level, ink(), alpha);
        }
    }
}

/* the draw pile: its backs and the cards left */
static void draw_pile(int n)
{
    cards_pile(DECK_FRENCH, L.pilex, L.piley, L.pilew, n, CARD_NONE, 0);
    num(L.pilex, L.piley + L.pilew * 0.95f, 0.34f * L.pilew, n, ink(), 0.9f);
}

/* the value of a step, risen from its place */
static void popup(const pkr_step *s, float x, float y, int t)
{
    if (t < 0 || t > 800) return;
    float f = t / 800.0f, a = f < 0.7f ? 1 : 1 - (f - 0.7f) / 0.3f, h = 0.48f * L.u;
    y -= 0.5f * L.u * ui_ease(fminf(1, f * 1.6f));
    const uint8_t *c = s->what == SV_CHIPS ? BLUE : s->what == SV_COINS ? GOLD : RED;
    int sign = s->what == SV_XMULT ? 2 : s->value < 0 ? 3 : 1;
    int64_t v = s->value < 0 ? -s->value : s->value;
    float w = value_w(sign, v, s->what == SV_XMULT, h) + 0.5f * h;
    round_box(x - w / 2, y - 0.62f * h, w, 1.24f * h, c, (Uint8)(235 * a));
    value_at(sign, v, s->what == SV_XMULT, x, y, h * 0.8f, WHITE, a);
    if (s->what == SV_COINS) {
        SDL_Texture *t = ct_picture(paint_misc, K_COIN, (int)h);
        SDL_SetTextureAlphaMod(t, (Uint8)(255 * a));
        ui_put_c(t, x - w / 2 - 0.1f * h, y, 0.8f * h, 0.8f * h);
        SDL_SetTextureAlphaMod(t, 255);
    }
}

/* the play under way: the before-state's hand less the cards played, the cards played scoring one by one */
static void draw_scoring(void)
{
    const pkr_state *b = &A.b;
    const pkr_log *lg = &A.log;
    int t = an_t(), cur = step_at(t);
    float k = speedk();
    /* the rest of the hand */
    uint8_t rest[PKR_HAND_MAX];
    int nr = 0;
    for (int i = 0; i < b->nhand; i++) {
        int played = 0;
        for (int j = 0; j < lg->n; j++) played |= lg->played[j] == b->hand[i];
        if (!played) rest[nr++] = b->hand[i];
    }
    if (t < A.t_off) draw_hand(b, rest, nr, 0, 0);
    else if (G.s.phase == PH_PLAY || G.s.phase == PH_LOST)   /* the new hand */
        draw_hand(&G.s, G.s.hand, G.s.nhand, 0, G.s.phase == PH_LOST);
    /* the cards played */
    for (int j = 0; j < lg->n; j++) {
        int di = lg->played[j];
        float x, y;
        play_spot(j, lg->n, &x, &y);
        vpos p;
        if (t < A.t_off) p = vgo(di, (vpos){x, y, 0, L.pw}, (int)(300 * k), 40 * j);
        else {
            if (V[di].to.x < L.b.x + L.b.w) leave(b->deck[di], vnow(di), (vpos){L.b.x + L.b.w + L.pw, y - 0.3f * L.pw,
                                                                                   30, L.pw}, (int)(380 * k), 30 * j);
            V[di].to.x = L.b.x + 2 * L.b.w;
            continue;
        }
        int how = 0;
        if (t >= A.t_base && !(lg->scoring >> j & 1)) how |= CD_DIM;
        /* a hop while it scores */
        float hop = 0;
        for (int i = 0; i < lg->nsteps; i++) {
            const pkr_step *s = &lg->step[i];
            if ((s->kind == SS_CARD || s->kind == SS_AGAIN || s->kind == SS_DEBUFF) && s->idx == j &&
                t >= A.st[i] && t < A.st[i] + 260)
                hop = fmaxf(hop, sinf(ART_PI * (t - A.st[i]) / 260.0f));
        }
        if (lg->broke >> j & 1 && t >= A.st[lg->nsteps] - 1) {   /* a crystal broken: it fades */
            float f = clampf((t - A.t_total) / 400.0f, 0, 1);
            card_at(b->deck[di], p.x, p.y - hop * 0.18f * L.pw, p.w, p.deg, how, 1 - f);
            continue;
        }
        card_at(b->deck[di], p.x, p.y - hop * 0.18f * L.pw, p.w * (1 + 0.06f * hop), p.deg,
                how | (hop > 0 ? CD_GLOW : 0), 1);
    }
    /* the chips and the mult */
    if (t >= A.t_base && t < A.t_total + 250) {
        const pkr_step *s = &lg->step[cur < 0 ? 0 : cur];
        float pc = 0, pm = 0;
        if (cur >= 0 && t - A.st[cur] < 200) {
            float f = sinf(ART_PI * (t - A.st[cur]) / 200.0f);
            const pkr_step *p0 = cur > 0 ? &lg->step[cur - 1] : NULL;
            if (!p0 || p0->chips != s->chips) pc = f;
            if (!p0 || p0->mult != s->mult) pm = f;
        }
        float fade = t < A.t_total ? 1 : 1 - (t - A.t_total) / 250.0f;
        draw_score_boxes(lg->type, lg->level, s->chips, s->mult, pc, pm, fade);
    }
    /* the values risen */
    for (int i = 1; i < lg->nsteps; i++) {
        const pkr_step *s = &lg->step[i];
        int dt = t - A.st[i];
        if (dt < 0 || dt > 800 || s->what == SV_NONE) continue;
        float x = L.pcx, y = L.py - 0.75f * L.pw * 1.4f;
        if (s->kind == SS_CARD) play_spot(s->idx, lg->n, &x, &y), y -= 0.62f * L.pw * 1.4f;
        else if (s->kind == SS_CHARM) x = L.chx[s->idx], y = L.chy + 0.75f * L.chd;
        else if (s->kind == SS_HELD) {
            cards_spot sp[PKR_HAND_MAX];
            cards_fan(L.hcx, L.hcy, L.hwidth, L.hw, 1.4f, nr, nr > 1 ? L.arc : 0, sp);
            if (s->idx < nr) x = sp[s->idx].x, y = sp[s->idx].y - 0.8f * L.hw;
        } else if (s->kind == SS_COIN) x = L.hud.x + L.hud.w * 0.5f, y = L.hud.y + L.hud.h;
        if (s->kind == SS_CARD && s->src >= 2 && s->src - 2 < PKR_CHARMS) {   /* a charm's: from the charm */
            popup(s, x, y, dt);
            continue;
        }
        popup(s, x, y, dt);
    }
    for (int i = 1; i < lg->nsteps; i++) {      /* the broken and the faded: a red stroke */
        const pkr_step *s = &lg->step[i];
        int dt = t - A.st[i];
        if ((s->kind == SS_DEBUFF || s->kind == SS_BREAK) && dt >= 0 && dt < 600) {
            float x, y;
            play_spot(s->idx, lg->n, &x, &y);
            ct_strike(x, y, 0.8f * L.pw);
        }
    }
    /* the total, then into the bar */
    if (t >= A.t_total && t < A.t_off) {
        float f = clampf((float)(t - A.t_total) / (A.t_fill - A.t_total), 0, 1), g = ui_ease(fminf(1, f * 1.8f));
        float h = (0.7f + 0.5f * g) * L.u, cx = L.pcx, cy = L.play.y + L.play.h - 0.6f * L.u;
        float a = 1;
        if (t >= A.t_fill) {                    /* it flies up to the bar */
            float e = ui_ease(clampf((float)(t - A.t_fill) / (A.t_off - A.t_fill), 0, 1));
            cy += (L.hud.y + L.hud.h * 0.68f - cy) * e;
            h *= 1 - 0.5f * e;
            a = 1 - e;
        }
        float w = num_w(lg->total, h) + 0.9f * h;
        ui_mark_gold_a(cx, cy, w * 1.1f, 0.7f * a);
        round_box(cx - w / 2, cy - 0.65f * h, w, 1.3f * h, ct_light() ? WHITE : (const uint8_t[3]){24, 28, 26},
                  (Uint8)(235 * a));
        num(cx, cy, h, lg->total, ct_light() ? CT_INK : GOLD, a);
    }
    ui_animate();
}

/* ---- drawing: the screens --------------------------------------------------------------------------------------- */
static void button_pic(float cx, float cy, float d, int kind, int how)
{
    ui_button_paint(cx, cy, d, paint_misc, kind, 0.62f, how | UI_TINT);
}

static void coins_at(float x, float y, float h, int v, int sign)
{
    SDL_Texture *t = ct_picture(paint_misc, K_COIN, (int)h);
    ui_put_c(t, x + h / 2, y, h, h);
    float vw = value_w(sign, v < 0 ? -v : v, 0, 0.8f * h);
    value_at(sign, v < 0 ? -v : v, 0, x + 1.15f * h + vw / 2, y, 0.8f * h, panel_ink(), 1);
}

static float coins_w(int v, int sign, float h) { return 1.15f * h + value_w(sign, v < 0 ? -v : v, 0, 0.8f * h); }

/* the ante's progress: eight pips, the antes passed in gold, this one lit */
static void ante_pips(const pkr_state *s, float cx, float cy, float d)
{
    if (s->ante > PKR_WIN_ANTE) {
        ui_star(cx - 0.6f * d, cy, 1.2f * d, 1);
        num(cx + 0.5f * d + num_w(s->ante, d) / 2, cy, d, s->ante, panel_ink(), 1);
        return;
    }
    for (int i = 0; i < PKR_WIN_ANTE; i++) {
        float x = cx + (i - 3.5f) * d * 1.25f;
        int a = i + 1;
        if (a < s->ante) ui_star(x, cy, d, 1);
        else if (a == s->ante) ui_mark_gold(x, cy, 1.8f * d), ui_star(x, cy, d * 1.15f, 1);
        else ui_star(x, cy, d, 0);
    }
}

/* an information panel in box p: a picture, then the name and the words; a button (pic, how) at its right end */
static void info_text(box p, int name_line, int fx_line, const char *extra)
{
    char nm[160], fx[300], s[480];
    word(name_line, nm, sizeof nm);
    if (fx_line >= 0) word(fx_line, fx, sizeof fx);
    else fx[0] = 0;
    snprintf(s, sizeof s, "%s\n%s%s", nm, fx, extra ? extra : "");
    int th, w = (int)p.w;
    float px = fminf(0.34f * L.u, 22.0f * L.u / 80) + 4;
    SDL_Texture *t = text_block(s, w, px, &th);
    if (t) ui_put(t, p.x, p.y + fmaxf(0, (p.h - th) / 2), (float)w, (float)th);
}

/* what an item is (its picture: a charm, a tome, an enhancement on a card, a pack, a card), drawn d across */
static void item_pic(const pkr_item *it, int pack_kind, float x, float y, float d)
{
    if (pack_kind == PK_CARD) {
        card_at(it->c, x, y, d * 0.62f, 0, 0, 1);
        return;
    }
    switch (it->kind) {
    case IT_CHARM:
        ui_shadow_flat(x, y, d * 0.9f, d * 0.9f, 0.6f);
        ui_put_c(ct_picture(paint_charm, it->id, (int)(0.9f * d)), x, y, 0.9f * d, 0.9f * d);
        break;
    case IT_TOME: ui_put_c(ct_picture(paint_misc, K_TOME0 + it->id, (int)d), x, y, d, d); break;
    case IT_ENH: {
        pkr_card c = {CARD(SUIT_SPADES, RANK_ACE), it->id};
        card_at(c, x, y, d * 0.62f, 0, 0, 1);
        break;
    }
    case IT_PACK: ui_put_c(ct_picture(paint_misc, K_PACK0 + it->id, (int)d), x, y, d, d); break;
    }
}

/* the words of an item: its name and its effect lines */
static void item_lines(const pkr_item *it, int pack_kind, int *name, int *fx)
{
    *name = *fx = -1;
    if (pack_kind == PK_CARD) {
        if (it->c.enh) *name = W_ENAME + it->c.enh - 1, *fx = W_EFX + it->c.enh - 1;
        return;
    }
    switch (it->kind) {
    case IT_CHARM: *name = W_CNAME + it->id, *fx = W_CFX + it->id; break;
    case IT_TOME: *name = W_HAND + it->id, *fx = W_TOMEFX; break;
    case IT_ENH: *name = W_ENAME + it->id - 1, *fx = W_EFX + it->id - 1; break;
    case IT_PACK: *name = W_PNAME + it->id, *fx = W_PFX + it->id; break;
    }
}

/* a tome's numbers: the hand's chips and mult now, and what a level adds */
static void tome_numbers(int type, float x, float y, float h)
{
    const pkr_state *s = &G.s;
    float bw = 2.0f * h, bh = 1.25f * h;
    round_box(x, y - bh / 2, bw, bh, BLUE, 255);
    num(x + bw / 2, y, 0.7f * h, pkr_hand_chips(s, type), WHITE, 1);
    sign_at(2, x + bw + 0.45f * h, y, 0.7f * h, panel_ink(), 1);
    round_box(x + bw + 0.9f * h, y - bh / 2, bw, bh, RED, 255);
    num(x + 1.5f * bw + 0.9f * h, y, 0.7f * h, pkr_hand_mult(s, type), WHITE, 1);
    float x2 = x + 2 * bw + 1.3f * h;
    value_at(1, pkr_hand_base[type][2], 0, x2 + value_w(1, pkr_hand_base[type][2], 0, 0.7f * h) / 2, y - 0.4f * h,
             0.6f * h, BLUE, 1);
    value_at(1, pkr_hand_base[type][3], 0, x2 + value_w(1, pkr_hand_base[type][3], 0, 0.7f * h) / 2, y + 0.45f * h,
             0.6f * h, RED, 1);
}

/* the panel for the open item, in box p; its button (1 take, 2 sell: the coins it gives, 3 buy: the price) at the
 * right end: info_btn_box */
static box info_btn_box(box p)
{
    float pad = 0.25f * L.u, bd = fminf(1.1f * L.u, p.h * 0.6f);
    float bx = p.x + p.w - pad - bd * 0.7f, by = p.y + p.h / 2 - 0.12f * bd;
    return (box){bx - bd / 2, by - bd / 2, bd, bd};
}

static void draw_info(box p, const pkr_item *it, int pack_kind, int charm_slot, int btn_kind, int btn_value,
                      int btn_ok)
{
    panel(p.x, p.y, p.w, p.h, 235);
    float pad = 0.25f * L.u, pd = fminf(p.h - 2 * pad, 1.5f * L.u), bd = fminf(1.1f * L.u, p.h * 0.6f);
    float tx = p.x + pad * 2 + pd, tw = p.w - (tx - p.x) - pad - (btn_kind ? bd * 1.6f : 0);
    int name = -1, fx = -1;
    char extra[64] = "";
    if (charm_slot >= 0) {
        const pkr_charm *c = &G.s.charm[charm_slot];
        pkr_item tmp = {IT_CHARM, c->id, c->price, 0, {0, 0}};
        item_pic(&tmp, -1, p.x + pad + pd / 2, p.y + p.h / 2, pd);
        name = W_CNAME + c->id, fx = W_CFX + c->id;
    } else if (it) {
        item_pic(it, pack_kind, p.x + pad + pd / 2, p.y + p.h / 2, pd);
        item_lines(it, pack_kind, &name, &fx);
    }
    if (it && it->kind == IT_TOME && pack_kind != PK_CARD) {
        box tb = {tx, p.y + pad * 0.6f, tw, p.h * 0.5f};
        info_text(tb, name, fx, NULL);
        tome_numbers(it->id, tx, p.y + p.h * 0.7f, fminf(0.42f * L.u, p.h * 0.2f));
    } else if (name >= 0) info_text((box){tx, p.y + pad * 0.5f, tw, p.h - pad}, name, fx, extra);
    else if (pack_kind == PK_CARD && it) {     /* a plain card: its chips */
        value_at(1, pkr_rank_chips(CARD_RANK(it->c.card)), 0, tx + 0.8f * L.u, p.y + p.h / 2, 0.5f * L.u, BLUE, 1);
    }
    if (btn_kind) {
        box bb = info_btn_box(p);
        float bx = bb.x + bb.w / 2, by = bb.y + bb.h / 2;
        if (btn_kind == 1) ui_button(bx, by, bd, ICON_TICK, btn_ok ? UI_ON | UI_LIT : UI_DIM);
        else ui_button_paint(bx, by, bd, paint_misc, K_COIN, 0.6f, btn_ok ? UI_LIT | UI_IVORY : UI_DIM);
        if (btn_value) {
            float h = 0.34f * bd;
            int sign = btn_kind == 2 ? 1 : 0;
            value_at(sign, btn_value, 0, bx, by + 0.72f * bd, h, btn_kind == 2 ? GREEN : panel_ink(), 1);
        }
    }
}

/* the blind ahead: the three blinds, this one lit; its target and reward; the boss's twist; play */
static void draw_blind_screen(void)
{
    const pkr_state *s = &G.s;
    box r = L.scr;
    float u = L.u, pw = L.land ? r.w * 0.9f : fminf(r.w * 0.92f, 8.2f * u), ph = fminf(r.h * 0.94f, 9.6f * u);
    float px = r.x + (r.w - pw) / 2, py = r.y + (r.h - ph) / 2;
    float lw = pw, bx = px, bw = pw, by0;        /* landscape: the blinds on the left, the boss and play on the right */
    if (L.land) ph = r.h * 0.94f, py = r.y + r.h * 0.03f, lw = pw * 0.56f, bx = px + lw, bw = pw - lw;
    panel(px, py, pw, ph, 220);
    float y = py + 0.55f * u, lc = px + lw / 2;
    ante_pips(s, lc, y, fminf(0.42f * u, lw / 11));
    y += 1.35f * u;
    float d = fminf(1.5f * u, ph * 0.18f);
    for (int b = 0; b < 3; b++) {
        float x = lc + (b - 1) * lw * 0.3f;
        float a = b == s->blind ? 1 : 0.45f;
        if (b == s->blind) ui_mark_gold(x, y, d * 1.6f);
        blind_disc(b, s->boss, s->boss_suit, x, y, b == s->blind ? d * 1.1f : d * 0.85f, a);
        if (b < s->blind) ui_put_c(ct_picture(paint_misc, K_TICK, (int)(0.6f * d)), x, y, 0.6f * d, 0.6f * d);
        word_at(b == 0 ? W_SMALL : b == 1 ? W_BIG : W_BOSSB, x, y + 0.85f * d, 0.3f * u, lw * 0.3f,
                b == s->blind ? panel_ink() : DIMC);
    }
    y += 1.4f * d;
    /* the target */
    float nh = 0.85f * u;
    SDL_Texture *tg = ct_picture(paint_misc, K_TARGET, (int)nh);
    float tw = nh * 1.3f + num_w(s->target, nh);
    ui_put_c(tg, lc - tw / 2 + nh / 2, y, nh, nh);
    num(lc - tw / 2 + nh * 1.3f + num_w(s->target, nh) / 2, y, nh, s->target, panel_ink(), 1);
    y += 1.0f * u;
    float cw = coins_w(3 + s->blind, 1, 0.55f * u);
    coins_at(lc - cw / 2, y, 0.55f * u, 3 + s->blind, 1);
    y += 0.75f * u;
    by0 = L.land ? py + 0.4f * u : y;
    float bd = fminf(1.4f * u, ph * 0.16f), by = py + ph - bd * 0.7f;
    /* the boss's twist (the one ahead, faded, before its blind) */
    float sd = fminf(1.1f * u, (by - bd * 0.6f - by0) * 0.6f);
    box tb = {bx + 0.5f * u + sd, by0, bw - 0.9f * u - sd, by - bd * 0.6f - by0};
    if (s->blind < 2) ct_rule(bx + 0.3f * u, by0 - 0.1f * u, bw - 0.6f * u);
    blind_disc(2, s->boss, s->boss_suit, bx + 0.3f * u + sd / 2, tb.y + tb.h / 2, sd, s->blind == 2 ? 1 : 0.7f);
    info_text(tb, W_BNAME + s->boss, W_BFX + s->boss, NULL);
    ui_button(bx + bw / 2, by, bd, ICON_PLAY, UI_ON | UI_LIT);
}

static void blind_play_spot(float *x, float *y, float *d)
{
    box r = L.scr;
    float u = L.u, pw = L.land ? r.w * 0.9f : fminf(r.w * 0.92f, 8.2f * u), ph = fminf(r.h * 0.94f, 9.6f * u);
    float py = r.y + (r.h - ph) / 2;
    float px = r.x + (r.w - pw) / 2, bx = px, bw = pw;
    if (L.land) ph = r.h * 0.94f, py = r.y + r.h * 0.03f, bx = px + pw * 0.56f, bw = pw * 0.44f;
    *d = fminf(1.4f * u, ph * 0.16f);
    *x = bx + bw / 2;
    *y = py + ph - *d * 0.7f;
}

/* the coins counted: a row for each, then the total, and go on */
static void draw_cash_screen(void)
{
    const pkr_state *s = &G.s;
    box r = L.scr;
    float u = L.u, pw = fminf(r.w * 0.86f, 7.2f * u), rh = fminf(0.95f * u, r.h * 0.11f);
    int rows = 2 + (s->cash_interest > 0) + (s->cash_gilded > 0) + (s->cash_charms > 0);
    float ph = rh * (rows + 2.6f) + 1.6f * u, px = r.x + (r.w - pw) / 2, py = r.y + (r.h - ph) / 2;
    panel(px, py, pw, ph, 220);
    int t = A.on ? an_t() - A.t_end : 1 << 20, k = 0, total = 0;
    float y = py + 0.5f * u + rh / 2, h = 0.62f * rh, lx = px + 0.8f * u, rx = px + pw - 0.6f * u;
    struct { int pic, v, extra; } row[5] = {{K_BLIND0, s->cash_blind, 0}, {K_HANDS, s->cash_hands, s->hands_left},
                                            {K_COIN, s->cash_interest, 0}, {K_PACK0, s->cash_gilded, 0},
                                            {K_PACK1, s->cash_charms, 0}};
    for (int i = 0; i < 5; i++) {
        if (i >= 2 && row[i].v == 0) continue;
        float a = clampf((t - 180 * k) / 200.0f, 0, 1);
        k++;
        if (a <= 0) { y += rh; continue; }
        if (i == 0) blind_disc(s->blind, s->boss, s->boss_suit, lx, y, rh * 0.8f, a);
        else if (i == 1) {
            round_box(lx - 0.4f * rh, y - 0.4f * rh, 0.8f * rh, 0.8f * rh, BLUE, (Uint8)(255 * a));
            SDL_Texture *tt = ct_icon(ICON_PLAY, (int)(0.5f * rh), 255, 255, 255);
            ui_put_c(tt, lx, y, 0.5f * rh, 0.5f * rh);
        } else if (i == 2) {                    /* interest: a coin and five */
            ui_put_c(ct_picture(paint_misc, K_COIN, (int)(0.7f * rh)), lx, y, 0.7f * rh, 0.7f * rh);
            ui_put_c(ct_picture(paint_misc, K_SLASH, (int)(0.5f * rh)), lx + 0.7f * rh, y, 0.5f * rh, 0.5f * rh);
            num(lx + 1.25f * rh, y, 0.5f * rh, 5, panel_ink(), a);
        } else if (i == 3) {
            pkr_card c = {CARD(SUIT_DIAMONDS, RANK_KING), EN_GILDED};
            card_at(c, lx, y, 0.55f * rh, 0, 0, a);
        } else {
            const pkr_charm *pig = NULL;
            for (int j = 0; j < s->ncharms; j++)
                if (s->charm[j].id == CH_PIG) pig = &s->charm[j];
            if (pig) ui_put_c(ct_picture(paint_charm, CH_PIG, (int)(0.8f * rh)), lx, y, 0.8f * rh, 0.8f * rh);
        }
        float cw = coins_w(row[i].v, 1, h);
        coins_at(rx - cw, y, h, row[i].v, 1);
        total += row[i].v;
        if (k < rows) ct_rule(px + 0.3f * u, y + rh / 2, pw - 0.6f * u);
        y += rh;
    }
    float a = clampf((t - 180 * rows) / 250.0f, 0, 1);
    y += 0.3f * rh;
    if (a > 0) {
        float hh = 0.95f * rh, cw = coins_w(total, 1, hh);
        coins_at(px + pw / 2 - cw / 2, y + 0.4f * rh, hh, total, 1);
    }
    float bd = fminf(1.25f * u, 1.3f * rh);
    ui_button(px + pw / 2, py + ph - bd * 0.7f, bd, ICON_ARROW, a >= 1 ? UI_ON | UI_LIT : UI_ON);
    if (t < 180 * rows + 300) ui_animate();
}

static void cash_btn_spot(float *x, float *y, float *d)
{
    const pkr_state *s = &G.s;
    box r = L.scr;
    float u = L.u, rh = fminf(0.95f * u, r.h * 0.11f);
    int rows = 2 + (s->cash_interest > 0) + (s->cash_gilded > 0) + (s->cash_charms > 0);
    float ph = rh * (rows + 2.6f) + 1.6f * u, py = r.y + (r.h - ph) / 2;
    *d = fminf(1.25f * u, 1.3f * rh);
    *x = r.x + r.w / 2;
    *y = py + ph - *d * 0.7f;
}

/* ---- the shop --------------------------------------------------------------------------------------------------- */
static struct { float x[PKR_SHOP], y[PKR_SHOP], d; float coins_y, rr[2], nx[2], bd; box info; } SH;

static void shop_layout(void)
{
    box r = L.scr;
    float u = L.u;
    if (!L.land) {
        float top = r.y + 0.15f * u, hh = fminf(1.5f * u, r.h * 0.15f);
        SH.coins_y = top + hh / 2;
        SH.bd = fminf(1.15f * u, hh * 0.85f);
        SH.rr[0] = r.x + r.w * 0.62f, SH.rr[1] = SH.coins_y;
        SH.nx[0] = r.x + r.w - 0.9f * u, SH.nx[1] = SH.coins_y;
        float iy = top + hh + 0.2f * u, ih = r.h * 0.27f;
        SH.d = fminf(ih * 0.62f, r.w * 0.22f);
        for (int i = 0; i < PKR_SHOP_ITEMS; i++) SH.x[i] = r.x + r.w * (i + 0.5f) / 3, SH.y[i] = iy + ih * 0.42f;
        float py = iy + ih, phh = r.h * 0.22f;
        for (int i = PKR_SHOP_ITEMS; i < PKR_SHOP; i++)
            SH.x[i] = r.x + r.w * (i - PKR_SHOP_ITEMS + 0.5f) / 2.0f * 0.8f + r.w * 0.1f, SH.y[i] = py + phh * 0.42f;
        float iy2 = py + phh + 0.1f * u;
        SH.info = (box){r.x + 0.2f * u, iy2, r.w - 0.4f * u, r.y + r.h - iy2 - 0.2f * u};
    } else {
        float lw = r.w * 0.58f, hh = fminf(1.4f * u, r.h * 0.2f), top = r.y + 0.1f * u;
        SH.coins_y = top + hh / 2;
        SH.bd = fminf(1.1f * u, hh * 0.85f);
        SH.rr[0] = r.x + r.w - 2.6f * u, SH.rr[1] = SH.coins_y;
        SH.nx[0] = r.x + r.w - 0.9f * u, SH.nx[1] = SH.coins_y;
        float ih = (r.h - hh - 0.3f * u) / 2;
        SH.d = fminf(ih * 0.62f, lw * 0.22f);
        for (int i = 0; i < PKR_SHOP_ITEMS; i++) SH.x[i] = r.x + lw * (i + 0.5f) / 3, SH.y[i] = top + hh + ih * 0.42f;
        for (int i = PKR_SHOP_ITEMS; i < PKR_SHOP; i++)
            SH.x[i] = r.x + lw * (i - PKR_SHOP_ITEMS + 0.5f) / 2, SH.y[i] = top + hh + ih * 1.42f;
        SH.info = (box){r.x + lw + 0.2f * u, top + hh + 0.2f * u, r.w - lw - 0.4f * u, r.h - hh - 0.6f * u};
    }
}

static void price_at(float x, float y, float h, int price, int ok)
{
    float cw = coins_w(price, 0, h);
    SDL_Texture *t = ct_picture(paint_misc, K_COIN, (int)h);
    if (!ok) SDL_SetTextureAlphaMod(t, 120);
    ui_put_c(t, x - cw / 2 + h / 2, y, h, h);
    SDL_SetTextureAlphaMod(t, 255);
    value_at(0, price, 0, x - cw / 2 + 1.15f * h + num_w(price, 0.8f * h) / 2, y, 0.8f * h, ok ? ink() : RED, 1);
}

static void draw_shop(void)
{
    const pkr_state *s = &G.s;
    shop_layout();
    box r = L.scr;
    float u = L.u, ch = 0.75f * u;
    coins_at(r.x + 0.4f * u, SH.coins_y, ch, s->coins, 0);
    /* reroll: its price under it; next */
    int rc = pkr_reroll_cost(s), rok = s->coins >= rc;
    button_pic(SH.rr[0], SH.rr[1] - 0.1f * u, SH.bd, K_CYCLE, rok ? UI_LIT : UI_DIM);
    price_at(SH.rr[0], SH.rr[1] + 0.55f * SH.bd, 0.36f * u, rc, rok);
    ui_button(SH.nx[0], SH.nx[1], SH.bd, ICON_ARROW, UI_ON | UI_LIT);
    /* the items */
    for (int i = 0; i < PKR_SHOP; i++) {
        const pkr_item *it = &s->shop[i];
        float x = SH.x[i], y = SH.y[i], d = SH.d;
        int lit = info == INF_SHOP && info_i == i;
        if (it->kind == IT_NONE || it->sold) {
            SDL_Texture *t = ct_disc(ct_light() ? (const uint8_t[3]){150, 130, 100} : (const uint8_t[3]){0, 0, 0}, 25,
                                     (int)d);
            ui_put_c(t, x, y, d * 0.8f, d * 0.8f);
            continue;
        }
        if (lit) ui_mark_gold(x, y, d * 1.45f);
        item_pic(it, -1, x, y, d);
        int pr = pkr_price(s, it);
        price_at(x, y + d * 0.68f, 0.38f * u, pr, s->coins >= pr);
    }
    /* the open item, or an equipped charm (sell) */
    if (info == INF_SHOP) {
        const pkr_item *it = &s->shop[info_i];
        int ok = pkr_legal(s, PKR_ACT(A_BUY, info_i));
        draw_info(SH.info, it, -1, -1, 3, pkr_price(s, it), ok);
    } else if (info == INF_CHARM && info_i < s->ncharms) {
        draw_info(SH.info, NULL, -1, info_i, 2, pkr_sell_value(&s->charm[info_i]), 1);
    }
}

/* a pack opened: its choices in a row, the one chosen in gold light, its words, take or skip */
static struct { float x[PKR_PACK_MAX], y, d; box info; float skip[2], bd; } PK;

static void pack_layout(void)
{
    const pkr_state *s = &G.s;
    box r = L.scr;
    float u = L.u;
    int n = s->npack;
    PK.bd = fminf(1.1f * u, r.h * 0.12f);
    if (!L.land) {
        PK.d = fminf(r.w * 0.85f / n, r.h * 0.26f);
        PK.y = r.y + r.h * 0.26f;
        for (int i = 0; i < n; i++) PK.x[i] = r.x + r.w * (i + 0.5f) / n;
        PK.info = (box){r.x + 0.2f * u, r.y + r.h * 0.5f, r.w - 0.4f * u, r.h * 0.3f};
        PK.skip[0] = r.x + r.w / 2, PK.skip[1] = r.y + r.h - PK.bd * 0.8f;
    } else {
        float lw = r.w * 0.58f;
        PK.d = fminf(lw * 0.9f / n, r.h * 0.4f);
        PK.y = r.y + r.h * 0.4f;
        for (int i = 0; i < n; i++) PK.x[i] = r.x + lw * (i + 0.5f) / n;
        PK.info = (box){r.x + lw + 0.2f * u, r.y + 0.2f * u, r.w - lw - 0.4f * u, r.h * 0.55f};
        PK.skip[0] = r.x + lw + (r.w - lw) / 2, PK.skip[1] = r.y + r.h - PK.bd * 0.9f;
    }
}

static void draw_pack(void)
{
    const pkr_state *s = &G.s;
    pack_layout();
    box r = L.scr;
    float u = L.u;
    ui_put_c(ct_picture(paint_misc, K_PACK0 + s->pack_kind, (int)(1.3f * u)), r.x + 0.9f * u, r.y + 0.8f * u, 1.3f * u,
             1.3f * u);
    word_at(W_PNAME + s->pack_kind, r.x + r.w / 2, r.y + 0.8f * u, 0.45f * u, r.w * 0.6f, ink());
    for (int i = 0; i < s->npack; i++) {
        const pkr_item *it = &s->pack[i];
        if (info == INF_PACK && info_i == i) ui_mark_gold(PK.x[i], PK.y, PK.d * 1.4f);
        item_pic(it, s->pack_kind, PK.x[i], PK.y, PK.d);
    }
    if (info == INF_PACK && info_i < s->npack)
        draw_info(PK.info, &s->pack[info_i], s->pack_kind, -1, 1, 0, pkr_legal(s, PKR_ACT(A_PICK, info_i)));
    else if (info == INF_CHARM && info_i < s->ncharms)
        draw_info(PK.info, NULL, -1, info_i, 2, pkr_sell_value(&s->charm[info_i]), 1);

    ui_button(PK.skip[0], PK.skip[1], PK.bd, ICON_CLOSE, 0);
}

/* an enhancement bought: the cards offered, choose up to two */
static struct { cards_spot sp[PKR_PICK]; float w; box info; float ok[2], bd; } EN;

static void enh_layout(void)
{
    const pkr_state *s = &G.s;
    box r = L.scr;
    float u = L.u;
    int n = s->npick, per = (n + 1) / 2;
    EN.bd = fminf(1.2f * u, r.h * 0.12f);
    if (!L.land) {
        EN.info = (box){r.x + 0.2f * u, r.y + 0.15f * u, r.w - 0.4f * u, fminf(2.2f * u, r.h * 0.22f)};
        float top = EN.info.y + EN.info.h + 0.3f * u, avail = r.y + r.h - EN.bd * 1.6f - top;
        EN.w = floorf(fminf(r.w * 0.9f / per / 1.08f, avail / 2 / 1.4f / 1.1f));
        for (int i = 0; i < n; i++) {
            int row = i / per, col = i % per;
            EN.sp[i].x = r.x + r.w / 2 + (col - (per - 1) / 2.0f) * EN.w * 1.12f;
            EN.sp[i].y = top + EN.w * 0.7f + row * EN.w * 1.4f * 1.12f + 0.1f * u;
            EN.sp[i].deg = 0;
        }
        EN.ok[0] = r.x + r.w / 2, EN.ok[1] = r.y + r.h - EN.bd * 0.75f;
    } else {
        float lw = r.w * 0.62f;
        EN.info = (box){r.x + lw + 0.2f * u, r.y + 0.2f * u, r.w - lw - 0.4f * u, r.h * 0.5f};
        EN.w = floorf(fminf(lw * 0.9f / per / 1.08f, r.h * 0.9f / 2 / 1.4f / 1.1f));
        for (int i = 0; i < n; i++) {
            int row = i / per, col = i % per;
            EN.sp[i].x = r.x + lw / 2 + (col - (per - 1) / 2.0f) * EN.w * 1.12f;
            EN.sp[i].y = r.y + r.h / 2 + (row - 0.5f) * EN.w * 1.4f * 1.12f;
            EN.sp[i].deg = 0;
        }
        EN.ok[0] = r.x + lw + (r.w - lw) / 2, EN.ok[1] = r.y + r.h - EN.bd * 0.9f;
    }
}


static void draw_enh(void)
{
    const pkr_state *s = &G.s;
    enh_layout();
    pkr_item it = {IT_ENH, s->pick_enh, 0, 0, {0, 0}};
    draw_info(EN.info, &it, -1, -1, 0, 0, 0);
    int n = 0;
    for (int i = 0; i < s->npick; i++) n += enh_mask >> i & 1;
    for (int i = 0; i < s->npick; i++) {
        pkr_card c = s->deck[s->pick[i]];
        int on = enh_mask >> i & 1;
        if (on) c.enh = s->pick_enh;            /* shown as it will be */
        card_at(c, EN.sp[i].x, EN.sp[i].y - (on ? 0.15f * EN.w : 0), EN.w, 0,
                on ? CD_GLOW : (n < s->pick_max ? 0 : CD_DIM), 1);
    }
    ui_button(EN.ok[0], EN.ok[1], EN.bd, ICON_TICK, UI_ON | UI_LIT);
}

/* the win: the stars of all the antes, then go on (endless) or end the run */
static void won_spots(float *x1, float *x2, float *y, float *d)
{
    box r = L.scr;
    *d = fminf(1.4f * L.u, r.h * 0.15f);
    *y = r.y + r.h * 0.66f;
    *x1 = r.x + r.w * 0.32f, *x2 = r.x + r.w * 0.68f;
}

static void draw_won(void)
{
    const pkr_state *s = &G.s;
    box r = L.scr;
    float u = L.u, x1, x2, y, d;
    won_spots(&x1, &x2, &y, &d);
    float cy = r.y + r.h * 0.25f, sd = fminf(2.4f * u, r.h * 0.25f);
    float pulse = 0.75f + 0.25f * sinf(ui_ms() / 300.0f);
    ui_mark_gold_a(r.x + r.w / 2, cy, sd * 2.2f, pulse);
    ui_star(r.x + r.w / 2, cy, sd, 1);
    ante_pips(s, r.x + r.w / 2, cy + sd * 0.85f, 0.42f * u);
    ui_button(x1, y, d, ICON_ARROW, UI_LIT);
    ui_button(x2, y, d, ICON_TICK, UI_ON | UI_LIT);
    word_at(W_ENDLESS, x1, y + 0.85f * d, 0.34f * u, r.w * 0.34f, ink());
    word_at(W_FINISH, x2, y + 0.85f * d, 0.34f * u, r.w * 0.34f, ink());
    ui_animate();
}

/* ---- the book: the hand types' levels, the records, the charms met ------------------------------------------- */
static struct { box p; float gx, gy, gd; int cols; box info; } BK;

static void book_layout(void)
{
    box b = L.b;
    float u = L.u;
    BK.p = (box){b.x + 0.15f * u, b.y + 0.15f * u, b.w - 0.3f * u, b.h - 0.3f * u};
    BK.cols = L.land ? 10 : 8;
    BK.gd = fminf(BK.p.w * (L.land ? 0.5f : 0.92f) / BK.cols, 0.95f * u);
    if (!L.land) {
        BK.gx = BK.p.x + (BK.p.w - BK.gd * BK.cols) / 2;
        BK.gy = BK.p.y + BK.p.h * 0.56f;
        BK.info = (box){BK.p.x + 0.3f * u, BK.gy + BK.gd * (CH_N / BK.cols) + 0.15f * u, BK.p.w - 0.6f * u,
                        BK.p.y + BK.p.h - (BK.gy + BK.gd * (CH_N / BK.cols)) - 0.3f * u};
    } else {
        BK.gx = BK.p.x + BK.p.w * 0.48f;
        BK.gy = BK.p.y + 1.3f * u;
        BK.info = (box){BK.gx, BK.gy + BK.gd * (CH_N / BK.cols) + 0.2f * u, BK.gd * BK.cols,
                        BK.p.y + BK.p.h - (BK.gy + BK.gd * (CH_N / BK.cols)) - 0.4f * u};
    }
}

static void draw_book(void)
{
    const pkr_state *s = &G.s;
    book_layout();
    box p = BK.p;
    float u = L.u;
    ct_veil(L.b, 140);
    panel(p.x, p.y, p.w, p.h, 255);
    /* the records: the best ante (a star), the wins (a cup), the charms met */
    float ry = p.y + 0.6f * u, h = 0.5f * u, rx = p.x + 0.5f * u;
    ui_star(rx, ry, 0.7f * u, 1);
    num(rx + 0.55f * u + num_w(best_ante, h) / 2, ry, h, best_ante, panel_ink(), 1);
    rx += 2.0f * u;
    ui_put_c(ct_icon(ICON_CUP, (int)(0.6f * u), GOLD[0], GOLD[1], GOLD[2]), rx, ry, 0.6f * u, 0.6f * u);
    num(rx + 0.5f * u + num_w(wins, h) / 2, ry, h, wins, panel_ink(), 1);
    rx += 2.0f * u;
    int met = 0;
    for (int i = 0; i < CH_N; i++) met += (int)((seen_kept | s->seen) >> i & 1);
    ui_put_c(ct_picture(paint_charm, CH_WORRY, (int)(0.6f * u)), rx, ry, 0.6f * u, 0.6f * u);
    float x = rx + 0.45f * u;
    num(x + num_w(met, h) / 2, ry, h, met, panel_ink(), 1);
    x += num_w(met, h) + 0.1f * u;
    SDL_Texture *sl = ct_picture(paint_misc, K_SLASH, (int)h);
    SDL_SetTextureColorMod(sl, panel_ink()[0], panel_ink()[1], panel_ink()[2]);
    ui_put_c(sl, x + 0.2f * u, ry, h, h);
    SDL_SetTextureColorMod(sl, 255, 255, 255);
    num(x + 0.45f * u + num_w(CH_N, h) / 2, ry, h, CH_N, panel_ink(), 1);
    /* the hand types */
    float ty = ry + 0.75f * u, lw = L.land ? p.w * 0.45f : p.w, rh = fminf(0.62f * u, (BK.gy - ty - 0.3f * u) / HT_N);
    if (L.land) rh = fminf(0.62f * u, (p.y + p.h - ty - 0.2f * u) / HT_N);
    for (int t = HT_N - 1, k = 0; t >= 0; t--, k++) {
        float y = ty + (k + 0.5f) * rh, nh = 0.48f * rh;
        int w, hh;
        SDL_Texture *tx = ui_word(WORDS, W_HAND + t, 0.5f * rh, lw * 0.42f, panel_ink(), &w, &hh);
        if (tx) ui_put(tx, p.x + 0.4f * u, y - hh / 2.0f, (float)w, (float)hh);
        float lx = p.x + lw * 0.5f;
        num(lx, y, nh, s->level[t], s->level[t] > 1 ? GOLD : DIMC, 1);
        float bx = p.x + lw * 0.58f, bw = lw * 0.15f;
        round_box(bx, y - 0.4f * rh, bw, 0.8f * rh, BLUE, 255);
        num(bx + bw / 2, y, nh, pkr_hand_chips(s, t), WHITE, 1);
        sign_at(2, bx + bw + 0.2f * rh, y, 0.5f * rh, panel_ink(), 1);
        round_box(bx + bw + 0.4f * rh, y - 0.4f * rh, bw, 0.8f * rh, RED, 255);
        num(bx + 1.5f * bw + 0.4f * rh, y, nh, pkr_hand_mult(s, t), WHITE, 1);
        num(p.x + lw - 0.5f * u, y, 0.4f * rh, s->played_n[t], DIMC, 1);
    }
    /* the charms met: in colour; the others, dark */
    for (int i = 0; i < CH_N; i++) {
        float cx = BK.gx + (i % BK.cols + 0.5f) * BK.gd, cy = BK.gy + (i / BK.cols + 0.5f) * BK.gd;
        if ((seen_kept | s->seen) >> i & 1) {
            if (book_pick == i) ui_mark_gold(cx, cy, BK.gd * 1.3f);
            ui_put_c(ct_picture(paint_charm, i, (int)(0.86f * BK.gd)), cx, cy, 0.86f * BK.gd, 0.86f * BK.gd);
        } else {
            SDL_Texture *t = ct_picture(paint_charm, i, (int)(0.86f * BK.gd));
            SDL_SetTextureColorMod(t, 40, 40, 40);
            SDL_SetTextureAlphaMod(t, 110);
            ui_put_c(t, cx, cy, 0.86f * BK.gd, 0.86f * BK.gd);
            SDL_SetTextureColorMod(t, 255, 255, 255);
            SDL_SetTextureAlphaMod(t, 255);
        }
    }
    if (book_pick >= 0) info_text(BK.info, W_CNAME + book_pick, W_CFX + book_pick, NULL);
}

/* the deck: a row a suit, by rank; the cards still to draw bright, the others dimmed */
static void draw_deck(void)
{
    const pkr_state *s = &G.s;
    float u = L.u;
    box p = {L.b.x + 0.15f * u, L.hud.y, L.b.w - 0.3f * u, L.btn.y + L.btn.h - L.hud.y};
    if (L.land) p = (box){L.b.x + 0.15f * u, L.b.y + 0.15f * u, L.b.w - 0.3f * u, L.b.h - 0.3f * u};
    ct_veil(L.b, 120);
    panel(p.x, p.y, p.w, p.h, 250);
    uint8_t in_draw[PKR_DECK_MAX] = {0};
    for (int i = 0; i < s->ndraw; i++) in_draw[s->draw[i]] = 1;
    float rh = (p.h - 0.4f * u) / 4;
    for (int su = 0; su < 4; su++) {
        uint8_t row[PKR_DECK_MAX];
        int n = 0;
        for (int r = 14; r >= 2; r--)
            for (int i = 0; i < s->ndeck; i++) {
                int c = s->deck[i].card;
                if (c != CARD_NONE && CARD_SUIT(c) == su && (CARD_RANK(c) == RANK_ACE ? 14 : CARD_RANK(c)) == r)
                    row[n++] = (uint8_t)i;
            }
        if (!n) continue;
        float w = fminf(rh * 0.86f / 1.4f, (p.w - 0.4f * u) / (0.62f * (n - 1) + 1)), step = fminf(w * 0.62f,
                  n > 1 ? (p.w - 0.4f * u - w) / (n - 1) : 0);
        float x0 = p.x + 0.2f * u + w / 2, y = p.y + 0.2f * u + (su + 0.5f) * rh;
        for (int k = 0; k < n; k++)
            card_at(s->deck[row[k]], x0 + k * step, y, w, 0, in_draw[row[k]] ? 0 : CD_DIM, in_draw[row[k]] ? 1 : 0.6f);
    }
}

/* ---- draw ------------------------------------------------------------------------------------------------------- */
static void draw_end(void);

static void draw(void)
{
    const pkr_state *s = &G.s;
    anim_sounds();
    ui_tool_paint(0, paint_misc, K_FAST, 0.62f, UI_TINT | (fast ? UI_ON : 0));
    ui_tool_paint(1, paint_misc, K_BOOK, 0.62f, UI_TINT | (info == INF_BOOK ? UI_ON : 0));
    if (scoring()) {
        draw_charms(s);
        int t = an_t();
        int64_t sc = A.b.score;
        if (t >= A.t_fill) {
            float e = ui_ease(clampf((float)(t - A.t_fill) / (A.t_off - A.t_fill), 0, 1));
            sc = A.b.score + (int64_t)((double)A.log.total * e);
        }
        draw_hud(&A.b, sc, A.b.hands_left - 1, A.b.discards_left);
        draw_pile(t < A.t_off ? A.b.ndraw : s->phase == PH_PLAY ? s->ndraw : A.b.ndraw);
        draw_scoring();
        draw_leaving();
        return;
    }
    {
        switch (s->phase) {
        case PH_PLAY:
        case PH_LOST: {
            draw_charms(s);
            draw_hud(s, s->score, s->hands_left, s->discards_left);
            draw_pile(s->ndraw);
            unsigned m = chosen_mask(s);
            int type = -1;
            for (int i = 0; i < s->nhand; i++)
                if (m >> i & 1 && s->hid[s->hand[i]]) type = -2;
            if (type == -1 && m) type = pkr_preview_type(s, m);
            if (type >= 0) draw_score_boxes(type, s->level[type], pkr_hand_chips(s, type), pkr_hand_mult(s, type),
                                            0, 0, 1);
            else if (m) draw_score_boxes(-1, 0, 0, 0, 0, 0, 0.5f);
            draw_hand(s, s->hand, s->nhand, m, s->phase == PH_LOST);
            draw_leaving();
            if (s->phase == PH_PLAY) {
                int c = 0;
                for (int i = 0; i < s->nhand; i++) c += m >> i & 1;
                int okp = m && pkr_legal(s, PKR_ACT(A_PLAY, m)), okd = m && pkr_legal(s, PKR_ACT(A_DISCARD, m));
                ui_button(L.bplay[0], L.bplay[1], L.bd * 1.1f, ICON_PLAY, okp ? UI_ON | UI_LIT : UI_DIM);
                button_pic(L.bdisc[0], L.bdisc[1], L.bd, K_CYCLE, okd ? UI_LIT : UI_DIM);
                button_pic(L.bsr[0], L.bsr[1], L.bd * 0.82f, K_SORTR, s->sort == 0 ? UI_ON : 0);
                button_pic(L.bss[0], L.bss[1], L.bd * 0.82f, K_SUITS, s->sort == 1 ? UI_ON : 0);
                (void)c;
            }
            if (info == INF_CHARM && info_i < s->ncharms) {
                box p = {L.b.x + 0.2f * L.u, L.hud.y, L.b.w - 0.4f * L.u, fminf(2.4f * L.u, L.hud.h + L.play.h * 0.5f)};
                if (L.land) p = (box){L.b.x + 0.2f * L.u, L.play.y, L.b.w * 0.6f, fminf(2.4f * L.u, L.play.h)};
                draw_info(p, NULL, -1, info_i, 0, 0, 0);
            } else if (info == INF_DECK) {
                draw_deck();
            } else if (info == INF_BOSS) {
                box p = {L.b.x + 0.2f * L.u, L.play.y, L.b.w - 0.4f * L.u, fminf(2.4f * L.u, L.play.h)};
                panel(p.x, p.y, p.w, p.h, 235);
                float pd = fminf(p.h - 0.5f * L.u, 1.5f * L.u);
                ui_put_c(ct_picture(paint_boss, s->boss * 4 + s->boss_suit, (int)pd), p.x + 0.25f * L.u + pd / 2,
                         p.y + p.h / 2, pd, pd);
                info_text((box){p.x + 0.5f * L.u + pd, p.y + 0.15f * L.u, p.w - pd - 0.75f * L.u, p.h - 0.3f * L.u},
                          W_BNAME + s->boss, W_BFX + s->boss, NULL);
            }
            break;
        }
        case PH_BLIND: draw_charms(s), draw_blind_screen(); break;
        case PH_CASH: draw_charms(s), draw_cash_screen(); break;
        case PH_SHOP: draw_charms(s), draw_shop(); break;
        case PH_PACK: draw_charms(s), draw_pack(); break;
        case PH_ENH: draw_charms(s), draw_enh(); break;
        case PH_WON: draw_charms(s), draw_won(); break;
        case PH_DONE: draw_charms(s), draw_won(); break;
        }
    }
    int other = s->phase == PH_BLIND || s->phase == PH_CASH || s->phase == PH_ENH || s->phase == PH_WON;
    if (info == INF_CHARM && other && info_i < s->ncharms) {   /* a charm's words, under the charms */
        box p = {L.b.x + 0.2f * L.u, L.ch.y + L.ch.h, L.b.w - 0.4f * L.u, fminf(2.4f * L.u, L.scr.h * 0.3f)};
        draw_info(p, NULL, -1, info_i, 0, 0, 0);
    }
    if (info == INF_BOOK) draw_book();
    draw_end();
}

/* the end card's room: the ante reached, the best hand, the record, the wins */
static void draw_end(void)
{
    if (!ui_over()) return;
    float u = ui.unit, rh = 0.8f * u;
    box r = ui_end_room(5.6f * u, 3 * rh);
    if (r.w <= 0) return;
    rh = r.h / 3;
    const pkr_state *s = &G.s;
    float h = 0.5f * rh;
    for (int i = 0; i < 3; i++) {
        float y = r.y + (i + 0.5f) * rh, lx = r.x + 0.6f * u;
        int64_t v = i == 0 ? s->best_hand : i == 1 ? best_ante : wins;
        if (i == 0) {
            pkr_card c = {CARD(SUIT_HEARTS, RANK_ACE), 0};
            card_at(c, lx, y, 0.42f * rh, 0, 0, 1);
        } else if (i == 1) ui_star(lx, y, 0.7f * rh, 1);
        else ui_put_c(ct_icon(ICON_CUP, (int)(0.7f * rh), GOLD[0], GOLD[1], GOLD[2]), lx, y, 0.7f * rh, 0.7f * rh);
        ui_number(r.x + r.w - 0.4f * u - num_w(v, h) / 2, y, h, clampi(v), 1);
        if (i < 2) ct_rule(r.x + 0.2f * u, y + rh / 2, r.w - 0.4f * u);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------ */
static int charm_hit(float x, float y)
{
    for (int i = 0; i < G.s.ncharms; i++)
        if (ct_on(L.chx[i], L.chy, L.chd, x, y)) return i;
    return -1;
}

static int in_box(box b, float x, float y) { return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }

static void press_play(float x, float y)
{
    const pkr_state *s = &G.s;
    if (info == INF_CHARM || info == INF_BOSS || info == INF_DECK) { info = INF_NONE; return; }
    if (cards_in(L.pilex, L.piley, L.pilew * 1.2f, 1.4f, x, y)) { info = INF_DECK; ui_sound(SND_TAP); return; }
    int c = charm_hit(x, y);
    if (c >= 0) { info = INF_CHARM, info_i = c; ui_sound(SND_TAP); return; }
    if (s->blind == 2 && ct_on(L.hud.x + L.hud.h * 0.5f, L.hud.y + L.hud.h * 0.5f, L.hud.h * 0.7f, x, y)) {
        info = INF_BOSS;
        ui_sound(SND_TAP);
        return;
    }
    if (s->phase != PH_PLAY) return;
    unsigned m = chosen_mask(s);
    if (ui_button_hit(L.bplay[0], L.bplay[1], L.bd * 1.1f, x, y)) {
        if (m && pkr_legal(s, PKR_ACT(A_PLAY, m))) act(PKR_ACT(A_PLAY, m));
        else ui_sound(SND_DENY);
        return;
    }
    if (ui_button_hit(L.bdisc[0], L.bdisc[1], L.bd, x, y)) {
        if (m && pkr_legal(s, PKR_ACT(A_DISCARD, m))) act(PKR_ACT(A_DISCARD, m));
        else ui_sound(SND_DENY);
        return;
    }
    if (ui_button_hit(L.bsr[0], L.bsr[1], L.bd * 0.82f, x, y)) { act(PKR_ACT(A_SORT, 0)); return; }
    if (ui_button_hit(L.bss[0], L.bss[1], L.bd * 0.82f, x, y)) { act(PKR_ACT(A_SORT, 1)); return; }
    cards_spot sp[PKR_HAND_MAX];
    hand_spots(s, s->nhand, s->hand, m, sp);
    int i = cards_hit(sp, s->nhand, L.hw, 1.4f, x, y);
    if (i >= 0) {
        int di = s->hand[i], n = 0;
        for (int k = 0; k < s->nhand; k++) n += m >> k & 1;
        if (!chosen[di] && n >= PKR_PLAY_MAX) { ui_sound(SND_DENY); return; }
        chosen[di] ^= 1;
        ui_sound_soft(SND_TAP, 0.6f);
        return;
    }
    if (m) clear_chosen();                      /* a tap on the table: none chosen */
}

static void press_shop(float x, float y)
{
    const pkr_state *s = &G.s;
    shop_layout();
    if (((info == INF_SHOP && info_i < PKR_SHOP) || (info == INF_CHARM && info_i < s->ncharms)) &&
        in_box(info_btn_box(SH.info), x, y)) {
        if (info == INF_SHOP) {
            if (pkr_legal(s, PKR_ACT(A_BUY, info_i))) act(PKR_ACT(A_BUY, info_i));
            else ui_sound(SND_DENY);
        } else if (info == INF_CHARM) act(PKR_ACT(A_SELL, info_i));
        return;
    }
    int c = charm_hit(x, y);
    if (c >= 0) { info = INF_CHARM, info_i = c; ui_sound(SND_TAP); return; }
    if (ui_button_hit(SH.nx[0], SH.nx[1], SH.bd, x, y)) { act(PKR_ACT(A_LEAVE, 0)); return; }
    if (ui_button_hit(SH.rr[0], SH.rr[1] - 0.1f * L.u, SH.bd, x, y)) {
        if (pkr_legal(s, PKR_ACT(A_REROLL, 0))) act(PKR_ACT(A_REROLL, 0));
        else ui_sound(SND_DENY);
        return;
    }
    for (int i = 0; i < PKR_SHOP; i++)
        if (s->shop[i].kind != IT_NONE && !s->shop[i].sold && ct_on(SH.x[i], SH.y[i], SH.d * 1.1f, x, y)) {
            if (info == INF_SHOP && info_i == i && pkr_legal(s, PKR_ACT(A_BUY, i))) act(PKR_ACT(A_BUY, i));
            else info = INF_SHOP, info_i = i, ui_sound(SND_TAP);
            return;
        }
    info = INF_NONE;
}

static void press_pack(float x, float y)
{
    const pkr_state *s = &G.s;
    pack_layout();
    if (((info == INF_PACK && info_i < s->npack) || (info == INF_CHARM && info_i < s->ncharms)) &&
        in_box(info_btn_box(PK.info), x, y)) {
        if (info == INF_PACK) {
            if (pkr_legal(s, PKR_ACT(A_PICK, info_i))) act(PKR_ACT(A_PICK, info_i));
            else ui_sound(SND_DENY);
        } else if (info == INF_CHARM) act(PKR_ACT(A_SELL, info_i));
        return;
    }
    int c = charm_hit(x, y);
    if (c >= 0) { info = INF_CHARM, info_i = c; ui_sound(SND_TAP); return; }
    if (ui_button_hit(PK.skip[0], PK.skip[1], PK.bd, x, y)) { act(PKR_ACT(A_SKIP, 0)); return; }
    for (int i = 0; i < s->npack; i++)
        if (ct_on(PK.x[i], PK.y, PK.d, x, y)) {
            if (info == INF_PACK && info_i == i && pkr_legal(s, PKR_ACT(A_PICK, i))) act(PKR_ACT(A_PICK, i));
            else info = INF_PACK, info_i = i, ui_sound(SND_TAP);
            return;
        }
    info = INF_NONE;
}

static void press_enh(float x, float y)
{
    const pkr_state *s = &G.s;
    enh_layout();
    if (ui_button_hit(EN.ok[0], EN.ok[1], EN.bd, x, y)) {
        act(PKR_ACT(A_ENH, enh_mask));
        enh_mask = 0;
        return;
    }
    for (int i = s->npick - 1; i >= 0; i--)
        if (cards_in(EN.sp[i].x, EN.sp[i].y, EN.w, 1.4f, x, y)) {
            int n = 0;
            for (int k = 0; k < s->npick; k++) n += enh_mask >> k & 1;
            if (!(enh_mask >> i & 1) && n >= s->pick_max) { ui_sound(SND_DENY); return; }
            enh_mask ^= 1u << i;
            ui_sound_soft(SND_TAP, 0.6f);
            return;
        }
}

static void press(float x, float y)
{
    const pkr_state *s = &G.s;
    if (scoring()) {                            /* a tap: the scoring to its end */
        A.t0 = ui_ms() - (Uint32)A.t_end;
        return;
    }
    if (ui_tool_hit(x, y) == 0) {
        fast ^= 1;
        uint8_t b = (uint8_t)fast;
        ui_save_set("pokerrun/fast", &b, 1);
        ui_sound(SND_TAP);
        return;
    }
    if (ui_tool_hit(x, y) == 1) {
        info = info == INF_BOOK ? INF_NONE : INF_BOOK;
        book_pick = -1;
        ui_sound(SND_TAP);
        return;
    }
    if (info == INF_BOOK) {
        book_layout();
        for (int i = 0; i < CH_N; i++) {
            float cx = BK.gx + (i % BK.cols + 0.5f) * BK.gd, cy = BK.gy + (i / BK.cols + 0.5f) * BK.gd;
            if (ct_on(cx, cy, BK.gd, x, y) && (seen_kept | s->seen) >> i & 1) { book_pick = i; return; }
        }
        info = INF_NONE;
        return;
    }
    int c = charm_hit(x, y);                    /* a charm: tapped (its words) or dragged along the row (release) */
    if (c >= 0 && !pkr_over(s)) {
        CD.on = 1, CD.from = c, CD.moved = 0, CD.x0 = CD.x = x, CD.y0 = CD.y = y;
        return;
    }
    if (info == INF_CHARM && s->phase != PH_SHOP && s->phase != PH_PACK) { info = INF_NONE; return; }
    switch (s->phase) {
    case PH_PLAY: press_play(x, y); break;
    case PH_BLIND: {
        float bx, by, bd;
        blind_play_spot(&bx, &by, &bd);
        if (ui_button_hit(bx, by, bd, x, y)) act(PKR_ACT(A_BEGIN, 0));
        break;
    }
    case PH_CASH: {
        float bx, by, bd;
        cash_btn_spot(&bx, &by, &bd);
        if (ui_button_hit(bx, by, bd, x, y)) act(PKR_ACT(A_CASH, 0));
        break;
    }
    case PH_SHOP: press_shop(x, y); break;
    case PH_PACK: press_pack(x, y); break;
    case PH_ENH: press_enh(x, y); break;
    case PH_WON: {
        float x1, x2, wy, d;
        won_spots(&x1, &x2, &wy, &d);
        if (ui_button_hit(x1, wy, d, x, y)) act(PKR_ACT(A_ENDLESS, 0));
        else if (ui_button_hit(x2, wy, d, x, y)) act(PKR_ACT(A_FINISH, 0));
        break;
    }
    case PH_LOST: press_play(x, y); break;
    }
}

static void motion(float x, float y)
{
    if (!CD.on) return;
    CD.x = x, CD.y = y;
    if (fabsf(x - CD.x0) + fabsf(y - CD.y0) > 0.25f * L.chd) CD.moved = 1;
}

/* the charm's place under the finger along the row */
static int charm_slot_at(float x)
{
    int best = 0;
    for (int i = 1; i < G.s.ncharms; i++)
        if (fabsf(x - L.chx[i]) < fabsf(x - L.chx[best])) best = i;
    return best;
}

static void release(float x, float y)
{
    if (!CD.on) return;
    CD.on = 0;
    if (!CD.moved) {                            /* a tap: its words (again: away) */
        info = info == INF_CHARM && info_i == CD.from ? INF_NONE : INF_CHARM, info_i = CD.from;
        ui_sound(SND_TAP);
        return;
    }
    (void)y;
    int to = charm_slot_at(x);
    if (to != CD.from) {
        if (info == INF_CHARM && info_i == CD.from) info_i = to;
        int keep = info;
        act(PKR_ACT(A_MOVE, CD.from * 8 + to));
        info = keep;
    }
}

/* keys: 1..9 choose a card, Return play, D discard, S sort */
static void key(SDL_Keycode k)
{
    const pkr_state *s = &G.s;
    if (scoring() || s->phase != PH_PLAY) {
        if (k == SDLK_RETURN && s->phase == PH_BLIND) act(PKR_ACT(A_BEGIN, 0));
        else if (k == SDLK_RETURN && s->phase == PH_CASH) act(PKR_ACT(A_CASH, 0));
        return;
    }
    unsigned m = chosen_mask(s);
    if (k >= SDLK_1 && k <= SDLK_9 && (int)(k - SDLK_1) < s->nhand) {
        int di = s->hand[k - SDLK_1], n = 0;
        for (int i = 0; i < s->nhand; i++) n += m >> i & 1;
        if (!chosen[di] && n >= PKR_PLAY_MAX) { ui_sound(SND_DENY); return; }
        chosen[di] ^= 1;
    } else if (k == SDLK_RETURN) {
        if (m && pkr_legal(s, PKR_ACT(A_PLAY, m))) act(PKR_ACT(A_PLAY, m));
    } else if (k == SDLK_d) {
        if (m && pkr_legal(s, PKR_ACT(A_DISCARD, m))) act(PKR_ACT(A_DISCARD, m));
    } else if (k == SDLK_s) act(PKR_ACT(A_SORT, !s->sort));
}

/* ---- the shell's pictures --------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    const pkr_state *s = scoring() ? &A.b : &G.s;
    int coins = s->coins;
    if (scoring()) {                            /* the coins as they come in */
        int t = an_t();
        for (int i = 0; i < A.log.nsteps; i++)
            if (t >= A.st[i] && A.log.step[i].what == SV_COINS) coins += A.log.step[i].value;
    }
    int ts = (int)(0.5f * ui.unit);
    ui_count c[2] = {{ct_picture(paint_misc, K_COIN, ts), coins}, {NULL, s->ante}};
    float d = ui_bar_fit(b, c, 1, 0);
    (void)d;
    float x = b.x + b.w * 0.55f, cy = b.y + b.h / 2, h = 0.4f * ui.unit;
    if (b.w > 4.5f * ui.unit) {                 /* the ante: a star and its number */
        ui_star(x, cy, 0.5f * ui.unit, 1);
        ui_number(x + 0.4f * ui.unit + ui_number_w(s->ante, h) / 2, cy, h, s->ante, 1);
    }
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    int w = (int)(size * 0.5f);
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_SPADES, RANK_ACE), size * 0.4f, size * 0.42f, w);
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_HEARTS, RANK_ACE), size * 0.6f, size * 0.58f, w);
}

/* the menu's card: a royal fan, a charm and a chip */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    static const int fan[5] = {CARD(SUIT_HEARTS, 10), CARD(SUIT_HEARTS, RANK_JACK), CARD(SUIT_HEARTS, RANK_QUEEN),
                               CARD(SUIT_HEARTS, RANK_KING), CARD(SUIT_HEARTS, RANK_ACE)};
    for (int i = 0; i < 5; i++)
        cards_paint_turned(rgba, size, DECK_FRENCH, fan[i], size * (0.24f + 0.13f * i),
                           size * (0.6f + 0.012f * (i - 2) * (i - 2)), size * 0.3f, (float)((i - 2) * 9));
    int d = (int)(size * 0.36f);
    uint8_t *c = malloc((size_t)d * d * 4);
    if (!c) return;
    paint_charm(CH_RUBY, d, c);
    art_blend(rgba, size, size, c, d, d, (int)(size * 0.03f), (int)(size * 0.04f));
    paint_charm(CH_IVY, d, c);
    art_blend(rgba, size, size, c, d, d, (int)(size * 0.34f), (int)(size * 0.02f));
    paint_misc(K_COIN, d, c);
    art_blend(rgba, size, size, c, d, d, (int)(size * 0.64f), (int)(size * 0.05f));
    free(c);
}

static const game pokerrun_game = {
    .id = "pokerrun", .order = 335, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = NULL, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 2, .score = score, .key = key, .fill = 1,
};
REGISTER_GAME(pokerrun_game)
