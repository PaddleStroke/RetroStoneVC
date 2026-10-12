/*
 * Bataille corse (Egyptian Ratscrew) in RetroStone Board: 2 to 4 players round one device lying between them (the
 * rules: slap_rules.c, slap.h). Nobody holds a hand: the piles are face down for everyone (VIEW_OPEN, and the view
 * never turns: player 0 sits at the bottom). The table is cut into one zone a player, as large as it can be: halves
 * for two (the top one turned round for the player facing), the top half in two for three, quadrants for four (round
 * the table in turn order: bottom left, top left, top right, bottom right). Each zone shows its player's pile at their
 * edge, their token and how many cards they have, and the tribute they owe (a dot a try left); the centre pile is in
 * the middle for all to see.
 * Tap your pile to turn a card when it is your turn (it glows); tap anywhere else in your zone to slap. Real time:
 * anyone may slap at any moment, even while cards fly; the first slap that reaches the app wins (the shell sends one
 * pointer: one finger at a time). A right slap: a gold light in the zone and the pile flies to its pile; a wrong
 * one: a red glow and a card goes under the centre. A tribute unpaid: the pile goes to the face card's player after
 * a moment (or at once if they tap their zone), so that a last double can still be slapped.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/cardtable.h"
#include "games/slap.h"
#include "shell.h"

#define PLAY_MS 220
#define TAKE_MS 340
#define FLASH_MS 650
#define CLAIM_WAIT 1300                         /* an unpaid tribute: the pile waits that long for a slap */

static slgame G;
static ct_moves MV;
static int variant;
static Uint32 claim_at;                         /* when the pile became the claimer's (0: not yet seen) */
static struct { int q, good; Uint32 t0; } flash;
enum { TAG_CENTRE = 1, TAG_PILE, TAG_UNDER };

/* ---- the zones --------------------------------------------------------------------------------------------------- */
static struct {
    box b;
    float cx, cy, wc, wp, asp;                  /* the centre; the centre's cards' width, the piles' */
    box zone[SLAP_MAXP];
    int n;                                      /* the players the zones were made for */
    int top[SLAP_MAXP];                         /* the zone's player sits across (it is drawn turned round) */
    SDL_Texture *tok[MAX_PLAYERS];
    int tok_made;
} T;

static void layout(box b)
{
    int n = G.pos.players ? G.pos.players : 2;
    T.b = b;
    T.n = n;
    T.cx = b.x + b.w / 2;
    T.cy = b.y + b.h / 2;
    T.asp = cards_aspect(DECK_FRENCH);
    float hw = b.w / 2, hh = b.h / 2;
    for (int q = 0; q < SLAP_MAXP; q++) T.top[q] = 0;
    if (n == 2) {
        T.zone[0] = (box){b.x, b.y + hh, b.w, hh};
        T.zone[1] = (box){b.x, b.y, b.w, hh};
        T.top[1] = 1;
    } else if (n == 3) {
        T.zone[0] = (box){b.x, b.y + hh, b.w, hh};
        T.zone[1] = (box){b.x, b.y, hw, hh};
        T.zone[2] = (box){b.x + hw, b.y, hw, hh};
        T.top[1] = T.top[2] = 1;
    } else {
        T.zone[0] = (box){b.x, b.y + hh, hw, hh};
        T.zone[1] = (box){b.x, b.y, hw, hh};
        T.zone[2] = (box){b.x + hw, b.y, hw, hh};
        T.zone[3] = (box){b.x + hw, b.y + hh, hw, hh};
        T.top[1] = T.top[2] = 1;
    }
    T.wc = floorf(fminf(b.w * 0.2f, b.h * 0.15f));
    float zw = n == 4 ? hw : n == 3 ? hw : b.w;
    T.wp = floorf(fminf(fminf(zw * 0.32f, hh * 0.36f / T.asp), 0.9f * T.wc));
    ct_tokens(T.tok, &T.tok_made, (int)fminf(1.0f * ui.unit, 0.62f * T.wp));
}

/* a point of player q's zone in its own frame (x across, 0..w; d from its outer edge, where its player sits),
 * on the screen */
static void zone_pt(int q, float x, float d, float *sx, float *sy)
{
    box z = T.zone[q];
    if (T.top[q]) *sx = z.x + z.w - x, *sy = z.y + d;
    else *sx = z.x + x, *sy = z.y + z.h - d;
}

static float zone_deg(int q) { return T.top[q] ? 180 : 0; }

static void pile_at(int q, float *x, float *y) { zone_pt(q, T.zone[q].w / 2, 0.62f * T.wp * T.asp + 0.12f * ui.unit, x, y); }

static int zone_of(float x, float y)
{
    for (int q = 0; q < G.pos.players; q++) {
        box z = T.zone[q];
        if (x >= z.x && x < z.x + z.w && y >= z.y && y < z.y + z.h) return q;
    }
    return -1;
}

/* the centre's card i: a little askew, a little off (from the card and its place: it stays put) */
static void centre_at(const slpos *p, int i, float *x, float *y, float *deg)
{
    unsigned h = (unsigned)(p->centre[i] * 2654435761u) ^ (unsigned)(i * 40503u);
    h ^= h >> 13;
    *deg = (float)((int)(h % 31) - 15);
    *x = T.cx + ((int)(h >> 5 & 15) - 7.5f) * 0.012f * T.wc;
    *y = T.cy + ((int)(h >> 9 & 15) - 7.5f) * 0.012f * T.wc;
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
static void animate(const slpos *b)
{
    const slpos *p = &G.pos;
    float x0, y0, x1, y1, deg;
    switch (p->event) {
    case SLE_PLAY: {
        int q = p->who, i = p->nc - 1;
        if (!ct_busy(&MV)) ct_calm(&MV);
        pile_at(q, &x0, &y0);
        centre_at(p, i, &x1, &y1, &deg);
        ct_slide(&MV, DECK_FRENCH, p->centre[i], x0, y0, T.wp, zone_deg(q), 0, x1, y1, T.wc, deg, 1, PLAY_MS, 0,
                 TAG_CENTRE, i, 0, SND_PLACE);
        break;
    }
    case SLE_TAKE: {                            /* every centre card flies to the taker's pile */
        int q = p->who;
        ct_calm(&MV);
        pile_at(q, &x1, &y1);
        for (int i = b->nc - 1, k = 0; i >= 0; i--, k++) {
            centre_at(b, i, &x0, &y0, &deg);
            int up = i >= b->buried;
            ct_slide(&MV, DECK_FRENCH, b->centre[i], x0, y0, T.wc, deg, up, x1, y1, T.wp, zone_deg(q), 0, TAKE_MS,
                     k * 22, TAG_PILE, q, 0, k == 0 ? (p->how == 1 ? SND_CAPTURE : SND_MOVE) : -1);
        }
        if (p->how == 1) flash.q = q, flash.good = 1, flash.t0 = ui_ms();
        break;
    }
    case SLE_PENALTY: {
        int q = p->who;
        if (!ct_busy(&MV)) ct_calm(&MV);
        pile_at(q, &x0, &y0);
        ct_slide(&MV, DECK_FRENCH, p->centre[0], x0, y0, T.wp, zone_deg(q), 0, T.cx, T.cy + 0.25f * T.wc * T.asp,
                 T.wc, 0, 0, PLAY_MS + 80, 0, TAG_UNDER, 0, 0, -1);
        flash.q = q, flash.good = 0, flash.t0 = ui_ms();
        ui_sound(SND_DENY);
        break;
    }
    case SLE_OUT:
        flash.q = p->who, flash.good = 0, flash.t0 = ui_ms();
        ui_sound(SND_DENY);
        break;
    }
}

static void act(int a)
{
    slpos before = G.pos;
    if (!slap_play(&G, a)) { ui_sound(SND_DENY); return; }
    animate(&before);
    claim_at = 0;
}

static int busy(void) { return ct_busy(&MV); }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    variant = v == SLAP_SANDWICH ? SLAP_SANDWICH : SLAP_DOUBLE;
    slap_new(&G, variant, ui.players, (uint32_t)ui_random(1 << 30));
    ct_calm(&MV);
    flash.t0 = 0;
    claim_at = 0;
}

static int status(void)
{
    const slpos *p = &G.pos;
    return p->phase == SLP_OVER ? gs_win(p->winner) : GS_PLAY;
}

static int turn(void) { return G.pos.turn; }

/* the view never turns: player 0 at the bottom, everyone round the device */
static int undo(void)
{
    if (!slap_undo(&G)) return 0;
    ct_calm(&MV);
    flash.t0 = 0;
    claim_at = 0;
    return 1;
}

static int script(const char *s)
{
    int ok = slap_script(&G, s);
    ct_calm(&MV);
    flash.t0 = 0;
    claim_at = 0;
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static const Uint8 TINT[SLAP_MAXP][3] = {{240, 230, 210}, {20, 18, 16}, {190, 52, 40}, {40, 150, 112}};

static void draw_zone(int q, Uint32 now)
{
    const slpos *p = &G.pos;
    box z = T.zone[q];
    float u = ui.unit, deg = zone_deg(q), x, y;
    int out = p->out >> q & 1;
    ui_fill(z.x, z.y, z.w, z.h, TINT[q][0], TINT[q][1], TINT[q][2], q == 1 ? 40 : 22);   /* the zone, faintly */
    if (flash.t0 && now - flash.t0 < FLASH_MS && flash.q == q) {   /* a slap: gold (right), red (wrong) */
        ui_animate();
        zone_pt(q, z.w / 2, z.h * 0.55f, &x, &y);
        float d = fminf(z.w, z.h) * 1.1f;
        if (flash.good) ui_mark_gold(x, y, d);
        else ui_mark_glow(x, y, d);
    }
    /* the pile, glowing when it is this player's turn to turn a card (or to gather the pile) */
    int n = p->n[q] - ct_hides(&MV, TAG_PILE, q, -1), mine = p->turn == q && p->phase != SLP_OVER;
    pile_at(q, &x, &y);
    if (n > 0) cards_pile(DECK_FRENCH, x, y, T.wp, n, CARD_NONE, mine && p->phase == SLP_PLAY ? CD_GLOW : 0);
    else cards_place(DECK_FRENCH, x, y, T.wp);
    /* the token (in gold light: to move, or to gather), and the cards held */
    float ts = (float)T.tok_made, side = 0.5f * T.wp + 0.75f * ts;
    float tx, ty, nx, ny;
    zone_pt(q, z.w / 2 - side, 0.62f * T.wp * T.asp + 0.12f * u, &tx, &ty);
    zone_pt(q, z.w / 2 + side + 0.1f * ts, 0.62f * T.wp * T.asp + 0.12f * u, &nx, &ny);
    if (mine) ui_mark_gold(tx, ty, 2.2f * ts);
    if (T.tok[q]) {
        SDL_SetTextureAlphaMod(T.tok[q], out ? 90 : 255);
        ui_put_rot(T.tok[q], tx, ty, ts, ts, deg);
    }
    ui_number_rot(nx, ny, 0.55f * ts, p->n[q], 1, deg);
    /* the tribute owed: a dot a try left, above the pile */
    if (p->owed && p->phase == SLP_PLAY && p->turn == q)
        for (int i = 0; i < p->owed; i++) {
            float dx, dy;
            zone_pt(q, z.w / 2 + (i - (p->owed - 1) / 2.0f) * 0.45f * u, 1.24f * T.wp * T.asp + 0.4f * u, &dx, &dy);
            ui_mark_dot(dx, dy, 0.36f * u);
        }
}

static void draw(void)
{
    const slpos *p = &G.pos;
    Uint32 now = ui_ms();
    if (T.n != p->players) layout(T.b);
    for (int q = 0; q < p->players; q++) draw_zone(q, now);
    /* the lines between the zones */
    float t = fmaxf(1, 0.03f * ui.unit);
    if (p->players >= 2) ui_fill(T.b.x, T.cy - t / 2, T.b.w, t, 246, 238, 220, 50);
    if (p->players >= 3) ui_fill(T.cx - t / 2, T.b.y, t, T.b.h / 2, 246, 238, 220, 50);
    if (p->players == 4) ui_fill(T.cx - t / 2, T.cy, t, T.b.h / 2, 246, 238, 220, 50);
    /* the centre pile: the cards face down under it, then the last ones face up, askew */
    int nc = p->nc - ct_hides(&MV, TAG_CENTRE, -1, -1), from = nc > 7 ? nc - 7 : 0;
    if (!nc && !ct_hides(&MV, TAG_PILE, -1, -1)) cards_place(DECK_FRENCH, T.cx, T.cy, T.wc);
    for (int i = from; i < nc; i++) {
        if (ct_hides(&MV, TAG_PILE, -1, -1)) break;   /* being gathered: it flies */
        float x, y, deg;
        centre_at(p, i, &x, &y, &deg);
        cards_draw(DECK_FRENCH, p->centre[i], x, y, T.wc, deg, i < p->buried ? CD_DOWN : i > from ? CD_FLAT : 0);
    }
    ct_draw(&MV, NULL);
    /* an unpaid tribute: the pile waits a moment for a slap, then goes to the face card's player */
    if (p->phase == SLP_CLAIM) {
        ui_animate();
        if (ct_busy(&MV)) claim_at = 0;
        else if (!claim_at) claim_at = now;
        else if (now - claim_at > CLAIM_WAIT) act(SLA_CLAIM);
    }
}

/* ---- the pointer: your pile turns a card (your turn), anywhere else in your zone slaps -------------------------- */
static void press(float x, float y)
{
    const slpos *p = &G.pos;
    if (p->phase == SLP_OVER) return;
    int q = zone_of(x, y);
    if (q < 0 || (p->out >> q & 1)) { ui_sound(SND_DENY); return; }
    float px, py;
    pile_at(q, &px, &py);
    int on_pile = cards_in(px, py, T.wp * 1.15f, T.asp, x, y);
    if (p->turn == q && p->phase == SLP_CLAIM) { act(SLA_CLAIM); return; }
    if (p->turn == q && p->phase == SLP_PLAY && on_pile) { act(SLA_PLAY); return; }
    if (!p->nc) { ui_sound(SND_DENY); return; }  /* nothing to slap (too late: someone took it) */
    act(SLA_SLAP | q);
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the cards held: a back and the count */
{
    float u = ui.unit, cw = 0.36f * u, cy = b.y + b.h / 2, h = fminf(0.42f * u, 0.45f * b.h);
    cards_draw(DECK_FRENCH, CARD_NONE, b.x + cw / 2, cy, cw, 0, CD_DOWN | CD_FLAT);
    int v = G.pos.n[player], digits = v >= 10 ? 2 : 1;
    ui_number(b.x + cw + 0.12f * u + (0.74f * (digits - 1) + 0.62f) * h / 2, cy, h, v, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's and the picker's cards: a double of nines on the centre pile; the sandwich: a king, a four, a king */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const uint8_t dbl[3] = {CARD(SUIT_CLUBS, 4), CARD(SUIT_SPADES, 9), CARD(SUIT_HEARTS, 9)},
                         sand[3] = {CARD(SUIT_CLUBS, RANK_KING), CARD(SUIT_DIAMONDS, 4), CARD(SUIT_HEARTS, RANK_KING)};
    const uint8_t *c = v == SLAP_SANDWICH ? sand : dbl;
    float asp = cards_aspect(DECK_FRENCH), w = fminf(size * 0.5f, size * 0.8f / asp);
    int cw = (int)w, ch = (int)(w * asp);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    memset(rgba, 0, (size_t)size * size * 4);
    static const float ox[3] = {0.06f, 0.26f, 0.44f}, oy[3] = {0.02f, 0.12f, 0.06f};
    for (int i = 0; i < 3; i++) {
        cards_paint(DECK_FRENCH, c[i], cw, ch, b);
        art_blend(rgba, size, size, b, cw, ch, (int)(ox[i] * size), (int)(oy[i] * size + (size - ch) * 0.4f));
    }
    free(b);
}

static const game slap_game = {
    .id = "slap", .order = 306, .variants = SLAP_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
    .players = 1 << 2 | 1 << 3 | 1 << 4, .players_usual = 2, .view = VIEW_OPEN,
};
REGISTER_GAME(slap_game)
