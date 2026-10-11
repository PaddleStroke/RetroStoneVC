/*
 * Old Maid (le pouilleux) in RetroStone Board: 2 to 6 players passing the device round (the rules: oldmaid_rules.c,
 * oldmaid.h). Each hand is secret (VIEW_HIDDEN): the player to move sees their hand at the bottom (sorted, face up),
 * the others' hands as small fans of backs along the top, and in the middle, big and face down, the hand of the
 * player they draw from. Tap one of its cards: it flies into the hand, and a pair it makes goes down onto the pile at
 * the left. The device then passes on. At the end, the jack of spades shows where it stayed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/cardtable.h"
#include "games/oldmaid.h"

#define SLIDE_MS 420
#define DEAL_MS 220

static ogame G;
static int dealing;
static ct_moves MV;
enum { H_HAND = 1, H_SEAT, H_PILE };

static struct {
    box b, ob, hb;                              /* the table; the offered hand's box; the viewer's hand's box */
    int me, k;
    float sy, sw, sfan;                         /* the others' seats */
    float px, py, pw;                           /* the pile of pairs */
} T;

static float seat_x(int q) { return ct_seat_x(T.b.x, T.b.w, T.k, ct_seat(q, T.me, G.pos.players)); }

static void layout(box b)
{
    const opos *p = &G.pos;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= p->players) T.me = 0;
    T.k = p->players - 1;
    int wide = b.w > b.h * 1.05f;
    float hs = b.h * (wide ? 0.21f : 0.17f), hm = b.h * (wide ? 0.37f : 0.36f), u = ui.unit;
    T.sw = floorf(fminf(fminf(hs * 0.62f / 1.4f, b.w / T.k * 0.3f), 0.85f * u));
    T.sfan = fminf(b.w / T.k * 0.86f, T.sw * 3.2f);
    T.sy = b.y + hs * 0.42f;
    T.pw = floorf(fminf(b.w * 0.13f, hm * 0.55f / 1.4f));
    T.px = b.x + 0.15f * u + T.pw * 0.6f;
    T.py = b.y + hs + hm / 2;
    float ox = T.px + T.pw * 0.75f;
    T.ob = (box){ox, b.y + hs, b.x + b.w - ox, hm};
    T.hb = (box){b.x, b.y + hs + hm, b.w, b.h - hs - hm};
}

/* the viewer's hand shown sorted (rank, then suit): order[i] the index in the hand of the i-th card shown */
static int shown(const opos *p, int pl, uint8_t *order)
{
    int n = p->n[pl];
    for (int i = 0; i < n; i++) order[i] = (uint8_t)i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            int a = p->hand[pl][order[j - 1]], c = p->hand[pl][order[j]];
            int ka = CARD_RANK(a) * 4 + CARD_SUIT(a), kc = CARD_RANK(c) * 4 + CARD_SUIT(c);
            if (ka <= kc) break;
            uint8_t t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    return n;
}

static float hand_spots(int n, cards_spot *s)
{
    return ct_hand(T.hb, n, 1.4f, fminf(T.b.w * 0.24f, 2.2f * ui.unit), s);
}

static float offer_spots(int n, cards_spot *s)
{
    return ct_hand(T.ob, n, 1.4f, fminf(T.ob.w * 0.3f, 1.8f * ui.unit), s);
}

/* the shown place of a card in a player's hand (the viewer's) */
static int place_of(const opos *p, int pl, int card, float *x, float *y, float *w, float *deg)
{
    uint8_t order[OM_CARDS];
    cards_spot s[OM_CARDS];
    int n = shown(p, pl, order);
    *w = hand_spots(n, s);
    for (int i = 0; i < n; i++)
        if (p->hand[pl][order[i]] == card) {
            *x = s[i].x, *y = s[i].y, *deg = s[i].deg;
            return 1;
        }
    return 0;
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
static int busy(void) { return ct_busy(&MV); }

static void deal_slides(void)
{
    const opos *p = &G.pos;
    float cx = T.ob.x + T.ob.w / 2, cy = T.ob.y + T.ob.h / 2, w = offer_spots(1, (cards_spot[1]){{0, 0, 0}});
    int k = 0;
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) {
            uint8_t order[OM_CARDS];
            cards_spot s[OM_CARDS];
            int n = shown(p, q, order);
            float hw = hand_spots(n, s);
            for (int i = 0; i < n; i++, k++)
                ct_slide(&MV, DECK_FRENCH, p->hand[q][order[i]], cx, cy, w, 0, 0, s[i].x, s[i].y, hw, s[i].deg, 1,
                         DEAL_MS + 80, k * 30, H_HAND, q, p->hand[q][order[i]], k % 4 ? -1 : SND_MOVE);
        } else {
            for (int i = 0; i < p->n[q] && i < 14; i++, k++) {
                float x, y, a;
                ct_backs_end(DECK_FRENCH, seat_x(q), T.sy, T.sfan, T.sw, i + 1, &x, &y, &a);
                ct_slide(&MV, DECK_FRENCH, CARD_NONE, cx, cy, w, 0, 0, x, y, T.sw, a, 0, DEAL_MS, k * 30, H_SEAT, q,
                         -1, k % 4 ? -1 : SND_MOVE);
            }
        }
    }
}

/* a draw: the card from the offered hand to the drawer's; a pair goes down */
static void animate(const opos *b, int index)
{
    const opos *p = &G.pos;
    int pl = b->turn, o = om_offerer(b), card = b->hand[o][index];
    if (pl != T.me) return;
    cards_spot s[OM_CARDS];
    float w = offer_spots(b->n[o], s), x, y, hw, deg;
    float x0 = s[index].x, y0 = s[index].y, a0 = s[index].deg;
    if (p->npile == b->npile) {                 /* no pair: into the hand, turning over */
        if (place_of(p, pl, card, &x, &y, &hw, &deg))
            ct_slide(&MV, DECK_FRENCH, card, x0, y0, w, a0, 0, x, y, hw, deg, 1, SLIDE_MS, 0, H_HAND, pl, card,
                     SND_PLACE);
        return;
    }
    int partner = p->pile[p->npile - 2];        /* a pair: next to its partner, then both onto the pile */
    if (!place_of(b, pl, partner, &x, &y, &hw, &deg)) return;
    int first = ct_slide(&MV, DECK_FRENCH, card, x0, y0, w, a0, 0, x + 0.3f * hw, y - 0.25f * hw, hw, deg, 1, SLIDE_MS,
                         0, 0, -1, -1, SND_PLACE);
    ct_slide(&MV, DECK_FRENCH, partner, x, y, hw, deg, 1, T.px, T.py, T.pw, -8, 1, SLIDE_MS, SLIDE_MS + 300, H_PILE,
             -1, -1, -1);
    int then = ct_slide(&MV, DECK_FRENCH, card, x + 0.3f * hw, y - 0.25f * hw, hw, deg, 1, T.px, T.py, T.pw, 0, 1,
                        SLIDE_MS, SLIDE_MS + 380, H_PILE, -1, -1, SND_CAPTURE);
    ct_after(&MV, then, first);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    om_new(&G, ui.players, (uint32_t)ui_random(1 << 30));
    ct_calm(&MV);
    dealing = 1;
}

static int status(void)
{
    unsigned w = om_winners(&G.pos);
    for (int q = 0; q < G.pos.players; q++)
        if (w >> q & 1) return gs_win(q);
    return G.pos.phase == OP_OVER ? GS_DRAW : GS_PLAY;
}

static unsigned winners(void) { return om_winners(&G.pos); }
static int turn(void) { return G.pos.turn; }

static void draw_card(int index)
{
    opos before = G.pos;
    if (!om_play(&G, index)) { ui_sound(SND_DENY); return; }
    ct_calm(&MV);
    animate(&before, index);
    if (G.pos.phase == OP_DRAW) ui_handoff(G.pos.turn);
}

static int undo(void) { return 0; }             /* every draw shows a hidden card: none taken back */

static int script(const char *s)
{
    int ok = om_script(&G, s);
    ct_calm(&MV);
    dealing = 0;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_tok[MAX_PLAYERS];
static int tok_made;

static void draw(void)
{
    const opos *p = &G.pos;
    if (dealing) { dealing = 0; ct_calm(&MV); deal_slides(); }
    int over = p->phase == OP_OVER, o = om_offerer(p), mine = p->turn == T.me && !ui_handing();
    int ready = mine && !over && !ct_moving(&MV);
    ct_tokens(t_tok, &tok_made, (int)fmaxf(8, 0.5f * T.sw));
    /* the pairs */
    int np = p->npile - ct_hides(&MV, H_PILE, -1, -1);
    cards_pile(DECK_FRENCH, T.px, T.py, T.pw, np, np > 0 ? p->pile[np - 1] : CARD_NONE, 0);
    /* the others' hands: the offered one big in the middle, the rest at their seats (the jack face up at the end) */
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) continue;
        float x = seat_x(q), ch = T.sw * 1.4f;
        int n = p->n[q] - ct_hides(&MV, H_SEAT, q, -1);
        if (q == o) {
            ui_mark_gold(x, T.sy, 2.2f * T.sw);
            cards_place(DECK_FRENCH, x, T.sy, T.sw);
        } else if (over && n == 1) {
            ui_mark_glow(x, T.sy, 2.6f * T.sw);
            cards_draw(DECK_FRENCH, p->hand[q][0], x, T.sy, T.sw * 1.3f, 0, 0);
        } else {
            ct_backs(DECK_FRENCH, x, T.sy, T.sfan, T.sw, n, 0);
            if (!n) cards_place(DECK_FRENCH, x, T.sy, T.sw);
        }
        ct_label(t_tok[q], x, T.sy + 0.5f * ch + 0.32f * T.sw + 0.12f * ch, 0.48f * T.sw, p->n[q]);
    }
    if (o >= 0) {
        cards_spot s[OM_CARDS];
        float w = offer_spots(p->n[o], s);
        for (int i = 0; i < p->n[o]; i++)
            cards_draw(DECK_FRENCH, CARD_NONE, s[i].x, s[i].y, w, s[i].deg, CD_DOWN | (ready ? CD_GLOW : 0));
    }
    /* the hand, sorted */
    uint8_t order[OM_CARDS];
    cards_spot s[OM_CARDS];
    int n = shown(p, T.me, order);
    float w = hand_spots(n, s);
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][order[i]];
        if (ct_hides(&MV, H_HAND, T.me, c)) continue;
        if (over && c == OM_ODD) ui_mark_glow(s[i].x, s[i].y, 1.6f * w);
        cards_draw(DECK_FRENCH, c, s[i].x, s[i].y, w, s[i].deg, 0);
    }
    ct_draw(&MV, NULL);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    const opos *p = &G.pos;
    if (p->phase != OP_DRAW || ct_moving(&MV)) return;
    int o = om_offerer(p);
    if (p->turn != T.me || o < 0) { ui_sound(SND_DENY); return; }
    cards_spot s[OM_CARDS];
    float w = offer_spots(p->n[o], s);
    int i = cards_hit(s, p->n[o], w, 1.4f, x, y);
    if (i >= 0) draw_card(i);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the cards in hand */
{
    int v = G.pos.n[player];
    float h = fminf(0.46f * ui.unit, b.w / 1.6f);
    ui_number(b.x + (v >= 10 ? 0.74f : 0.37f) * h, b.y + b.h / 2, h, v, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a pair of red sevens and, between them, the jack of spades */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[3] = {CARD(SUIT_HEARTS, 7), OM_ODD, CARD(SUIT_DIAMONDS, 7)};
    float w = size * 0.5f;
    int cw = (int)w, ch = (int)(w * 1.4f);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    memset(rgba, 0, (size_t)size * size * 4);
    for (int k = 0; k < 3; k++) {
        int i = k == 2 ? 1 : k == 1 ? 2 : 0;    /* the jack last: in front */
        cards_paint(DECK_FRENCH, cards[i], cw, ch, b);
        int x = (int)(size * (0.02f + 0.23f * i)), y = (int)(size * (i == 1 ? 0.06f : 0.22f));
        art_blend(rgba, size, size, b, cw, ch, x, y);
    }
    free(b);
}

static const game oldmaid_game = {
    .id = "oldmaid", .order = 299, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .view = VIEW_HIDDEN,
};
REGISTER_GAME(oldmaid_game)
