/*
 * Bataille corse (Egyptian Ratscrew) in RetroStone Board: 2 to 4 players round one device lying between them (the
 * rules: slap_rules.c, slap.h). Nobody holds a hand and nothing is secret: a board everyone sees (the default view,
 * no hand-off), each player seated by home() so that the shell puts their bar on their side. The board is cut into one
 * zone a player, as large as it can be: halves for two, the top half in two for three, quadrants for four (round the
 * table in turn order: bottom left, top left, top right, bottom right); face to face, the top zones are turned round
 * for the players across. Each zone shows its player's pile at their edge, their token and how many cards they have,
 * and the tribute they owe (a dot a try left); the centre pile is in the middle for all to see.
 * Tap your pile to turn a card when it is your turn (it glows); tap anywhere else in your zone to slap. Real time:
 * anyone may slap at any moment, even while cards fly, with as many fingers as there are players (finger()); the
 * first slap the device saw wins. A right slap: a gold light in the zone and the pile flies to its pile; a wrong
 * one: a red glow and a card goes under the centre. A tribute unpaid: the pile goes to the face card's player after
 * a moment (or at once if they tap their zone), so that a last double can still be slapped.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/slap.h"
#include "pages.h"
#include "shell.h"

#define PLAY_MS 220
#define TAKE_MS 340
#define FLASH_MS 650
#define CLAIM_WAIT 1300                         /* an unpaid tribute: the pile waits that long for a slap */

static slgame G;
static ct_moves MV = {.rest = 300};
static int variant;
static Uint32 claim_at;                         /* the last action: the wait for a slap starts there, or as the cards
                                                 * land (on the clock, not at a frame: a kept game played again draws
                                                 * few) */
static struct { int q, good; Uint32 t0; } flash;
enum { TAG_CENTRE = 1, TAG_PILE, TAG_UNDER };

/* ---- the zones --------------------------------------------------------------------------------------------------- */
static struct {
    box b;
    float cx, cy, wc, wp, asp;                  /* the centre; the centre's cards' width, the piles' */
    box zone[SLAP_MAXP];
    int n;                                      /* the players the zones were made for */
    int top[SLAP_MAXP];                         /* the zone's player sits at the top of the board */
    int tok_made;                               /* the tokens' size */
} T;

/* where each player sits (a point of the board): two face to face; three, one below and two across; four, one at
 * each corner, round the table in turn order (bottom left, top left, top right, bottom right). The shell puts each
 * player's bar on that side. */
static void home(int player, float *fx, float *fy)
{
    static const float x4[4] = {0.25f, 0.25f, 0.75f, 0.75f}, y4[4] = {1, 0, 0, 1}, x3[3] = {0.5f, 0.25f, 0.75f};
    int n = G.pos.players ? G.pos.players : 2, q = player & 3;
    *fx = n == 4 ? x4[q] : n == 3 ? x3[q % 3] : 0.5f;
    *fy = n == 4 ? y4[q] : q ? 0 : 1;
}

static void layout(box b)
{
    int n = G.pos.players ? G.pos.players : 2;
    T.b = b;
    T.n = n;
    T.cx = b.x + b.w / 2;
    T.cy = b.y + b.h / 2;
    T.asp = cards_aspect(DECK_FRENCH);
    /* each player's zone on their side of the board (flipped with it), that side's players from the left */
    float hh = b.h / 2, fx[SLAP_MAXP], zw = b.w;
    for (int q = 0; q < n; q++) {
        float hx, hy;
        home(q, &hx, &hy);
        fx[q] = ui.flipped ? 1 - hx : hx;
        T.top[q] = ui.flipped ? hy > 0.5f : hy < 0.5f;
    }
    for (int q = 0; q < n; q++) {
        int k = 0, left = 0;
        for (int r = 0; r < n; r++)
            if (T.top[r] == T.top[q]) k++, left += fx[r] < fx[q];
        float w = b.w / k;
        T.zone[q] = (box){b.x + left * w, T.top[q] ? b.y : b.y + hh, w, hh};
        if (w < zw) zw = w;
    }
    T.wc = floorf(fminf(b.w * 0.2f, b.h * 0.15f));
    T.wp = floorf(fminf(fminf(zw * 0.32f, hh * 0.36f / T.asp), 0.9f * T.wc));
    int ts = (int)fminf(1.0f * ui.unit, 0.62f * T.wp);
    if (ts >= 4) T.tok_made = ts;
}

/* a point of player q's zone in its own frame (x across, 0..w; d from its edge, where its player sits), on the
 * screen; face to face (ui_upside) the zone is turned round for the player across */
static void zone_pt(int q, float x, float d, float *sx, float *sy)
{
    box z = T.zone[q];
    *sx = ui_upside(q) ? z.x + z.w - x : z.x + x;
    *sy = T.top[q] ? z.y + d : z.y + z.h - d;
}

static float zone_deg(int q) { return ui_upside(q) ? 180 : 0; }

static void pile_at(int q, float *x, float *y)
{
    zone_pt(q, T.zone[q].w / 2, 0.62f * T.wp * T.asp + 0.12f * ui.unit, x, y);
}

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
    claim_at = ui_ms();
}

static int busy(void) { return ct_busy(&MV); }

/* ---- the bot (slap_bot.c): the game read-only; its action played as a touch does; its reaction time is real here.
 * The humans may touch while it thinks (slaps are real time): its move carries the actions it saw ("s1/57"), and a
 * move made for a game that has moved on since is let go, as a hand too late (the bot looks again). The count is in
 * the move, so the record judges it the same way when the game is played again (a move without one: in time). */
const slgame *slap_live(void) { return &G; }

int slap_realtime(void) { return 1; }

int slap_show(const char *move)
{
    const char *sl = strchr(move, '/');
    int len = sl ? (int)(sl - move) : (int)strlen(move);
    int a = len == 1 && move[0] == 'p' ? SLA_PLAY : len == 1 && move[0] == 'c' ? SLA_CLAIM : -1;
    if (len == 2 && move[0] == 's' && move[1] >= '0' && move[1] <= '9') a = SLA_SLAP | (move[1] - '0');
    if (a < 0) return 0;
    if ((sl && atoi(sl + 1) != G.n) || (SLA_KIND(a) == SLA_SLAP && !slap_right(&G.pos)))
        return 1;                               /* too late: nothing */
    if (!slap_legal(&G.pos, a)) return 0;
    act(a);
    return 1;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    variant = v == SLAP_SANDWICH ? SLAP_SANDWICH : SLAP_DOUBLE;
    slap_new(&G, variant, ui.players, (uint32_t)ui_random(1 << 30));
    ct_calm(&MV);
    flash.t0 = 0;
    claim_at = ui_ms();
}

static int status(void)
{
    const slpos *p = &G.pos;
    return p->phase == SLP_OVER ? gs_win(p->winner) : GS_PLAY;
}

static int turn(void) { return G.pos.turn; }

static int undo(void)
{
    if (!slap_undo(&G)) return 0;
    ct_calm(&MV);
    flash.t0 = 0;
    claim_at = ui_ms();
    return 1;
}

static int script(const char *s)
{
    int ok = slap_script(&G, s);
    ct_calm(&MV);
    flash.t0 = 0;
    claim_at = ui_ms();
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
    /* the zone, faintly (the light table: the ebony fainter, not a grey band; the ivory a warm beige) */
    if (!TH.dark && q == 0) ui_fill(z.x, z.y, z.w, z.h, 170, 140, 96, 22);
    else ui_fill(z.x, z.y, z.w, z.h, TINT[q][0], TINT[q][1], TINT[q][2], q == 1 ? (TH.dark ? 40 : 14) : 22);
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
    if (T.tok_made) {
        SDL_Texture *tk = ct_token(q, T.tok_made);
        SDL_SetTextureAlphaMod(tk, out ? 90 : 255);
        ui_put_rot(tk, tx, ty, ts, ts, deg);
    }
    ui_number_rot(nx, ny, 0.55f * ts, p->n[q], TH.dark, deg);   /* (cream on the dark table, dark on the light) */
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
    const Uint8 *lc = TH.dark ? (const Uint8[3]){246, 238, 220} : (const Uint8[3]){84, 56, 36};   /* the light: ink */
    ui_fill(T.b.x, T.cy - t / 2, T.b.w, t, lc[0], lc[1], lc[2], 50);
    for (int q = 0; q < p->players; q++)
        if (T.zone[q].x > T.b.x + 1) ui_fill(T.zone[q].x - t / 2, T.zone[q].y, t, T.zone[q].h, lc[0], lc[1], lc[2], 50);
    /* the centre pile: the cards face down under it, then the last ones face up, askew */
    int nc = p->nc - ct_hides(&MV, TAG_CENTRE, -1, -1), from = nc > 7 ? nc - 7 : 0;
    if (!nc && !ct_hides(&MV, TAG_PILE, -1, -1)) cards_place(DECK_FRENCH, T.cx, T.cy, T.wc);
    for (int i = from; i < nc; i++) {
        if (ct_hides(&MV, TAG_PILE, -1, -1)) break;   /* being gathered: it flies */
        float x, y, deg;
        centre_at(p, i, &x, &y, &deg);
        cards_draw(DECK_FRENCH, p->centre[i], x, y, T.wc, deg, i < p->buried ? CD_DOWN : i > from ? CD_FLAT : 0);
    }
    ct_draw(&MV);
    /* an unpaid tribute: the pile waits a moment for a slap, then goes to the face card's player */
    if (p->phase == SLP_CLAIM) {
        ui_animate();
        Uint32 from = ct_quiet(&MV);
        if ((Sint32)(claim_at - from) > 0) from = claim_at;
        if (!ct_busy(&MV) && now - from > CLAIM_WAIT) act(SLA_CLAIM);
    }
}

/* ---- touches: your pile turns a card (your turn), anywhere else in your zone slaps -------------------------------
 * Each finger on the board comes on its own (finger(): its id), in the order the device saw them, so the first finger
 * down on a double wins it; one coming after the pile is taken finds nothing to slap and costs nothing. The mouse (a
 * computer) comes through press(). */
static void touch(float x, float y)
{
    const slpos *p = &G.pos;
    if (p->phase == SLP_OVER) return;
    int q = zone_of(x, y);
    float px, py;
    if (q >= 0) pile_at(q, &px, &py);
    int a = q < 0 ? 0 : slap_touch(p, q, cards_in(px, py, T.wp * 1.15f, T.asp, x, y));
    if (a) act(a);
    else ui_sound(SND_DENY);
}

static void finger(int id, int phase, float x, float y)
{
    (void)id;                                   /* every finger slaps alike: only the order counts */
    if (phase == FINGER_DOWN) touch(x, y);
}

static void press(float x, float y) { touch(x, y); }
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
                         sand[3] = {CARD(SUIT_CLUBS, RANK_KING), CARD(SUIT_DIAMONDS, 4),
                                    CARD(SUIT_HEARTS, RANK_KING)};
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
    .bar = bar, .script = script, .busy = busy, .home = home, .finger = finger,
    .players = 1 << 2 | 1 << 3 | 1 << 4, .players_usual = 2,
};
REGISTER_GAME(slap_game)
