/*
 * Happy Families (le jeu des 7 familles) in RetroStone Board: 2 to 6 players passing the device round, with an
 * original deck of animal families (families_deck.c; the rules: families_rules.c, families.h). Each hand is secret
 * (VIEW_HIDDEN): the player to move sees their hand at the bottom, their families laid down above it, the others'
 * hands as small fans of backs along the top (their families under them), the stock and the last question in the
 * middle. To ask: tap the player to ask (the next one is chosen at first), tap a card of your hand (its family's six
 * members come up), tap the member you want. The answer is the app's: the card flies over from that player, or
 * "pioche", a card from the stock. A family complete flies down as one badge.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/cardtable.h"
#include "games/families.h"

#define SLIDE_MS 380

static fgame G;
static int dealing, target = -1, chosen = -1;   /* the player to ask, the family whose members show (-1) */
static ct_moves MV;
static Uint32 flash_t;                          /* a card given (or the lucky draw): its gold light until then */
static int flash_card = -1;
enum { H_HAND = 1, H_SEAT, H_LAID };

static struct {
    box b, hb;
    int me, k;
    float sy, sw, sfan, es;                     /* seats: fans' centre y, card width, fan width; badges' size */
    float sx, cy, wp;                           /* the stock */
    float ax, aw;                               /* the members asked for: first centre x, card width */
    float lx, ly, ls;                           /* the viewer's families: first badge, y, size */
} T;

static float seat_x(int q) { return ct_seat_x(T.b.x, T.b.w, T.k, ct_seat(q, T.me, G.pos.players)); }

static void layout(box b)
{
    const fpos *p = &G.pos;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= p->players) T.me = 0;
    T.k = p->players - 1;
    int wide = b.w > b.h * 1.05f;
    float hs = b.h * (wide ? 0.25f : 0.21f), hm = b.h * (wide ? 0.3f : 0.27f), hl = b.h * 0.08f, u = ui.unit;
    T.sw = floorf(fminf(fminf(hs * 0.5f / 1.4f, b.w / T.k * 0.3f), 0.8f * u));
    T.sfan = fminf(b.w / T.k * 0.8f, T.sw * 3.0f);
    T.sy = b.y + hs * 0.33f;
    T.es = floorf(fminf(hs * 0.2f, b.w / T.k / 7.6f));
    T.wp = floorf(fminf(hm * 0.7f / 1.4f, b.w * 0.17f));
    T.sx = b.x + 0.1f * u + T.wp * 0.7f;
    T.cy = b.y + hs + hm / 2;
    float ax0 = T.sx + T.wp * 0.8f, aw = b.x + b.w - ax0;
    T.aw = floorf(fminf(aw / 6.4f, hm * 0.8f / 1.4f));
    T.ax = ax0 + (aw - 6 * 1.06f * T.aw) / 2 + 0.53f * T.aw;
    T.ls = floorf(fminf(hl * 0.86f, b.w / 8.5f));
    T.lx = b.x + 0.1f * u + T.ls / 2;
    T.ly = b.y + hs + hm + hl / 2;
    T.hb = (box){b.x, b.y + hs + hm + hl, b.w, b.h - hs - hm - hl};
}

static float hand_spots(int n, cards_spot *s)
{
    return ct_hand(T.hb, n, 1.4f, fminf(T.b.w * 0.25f, 2.2f * ui.unit), s);
}

/* a family badge's place: the viewer's above their hand, the others' under their fan */
static void laid_at(int q, int i, float *x, float *y, float *s)
{
    if (q == T.me) {
        *s = T.ls;
        *x = T.lx + i * T.ls * 1.08f;
        *y = T.ly;
        return;
    }
    *s = T.es;
    *x = seat_x(q) + (i - (G.pos.nlaid[q] - 1) / 2.0f) * T.es * 1.05f;
    *y = T.sy + 0.7f * T.sw * 1.4f + 0.55f * T.sw + 0.6f * T.es;
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
static int busy(void) { return ct_busy(&MV); }

static int spot_of(const uint8_t *hand, int n, int card, cards_spot *s, float *w)
{
    *w = hand_spots(n, s);
    for (int i = 0; i < n; i++)
        if (hand[i] == card) return i;
    return -1;
}

static void deal_slides(void)
{
    const fpos *p = &G.pos;
    int k = 0;
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) {
            cards_spot s[FAM_CARDS];
            float w = hand_spots(p->n[q], s);
            for (int i = 0; i < p->n[q]; i++, k++)
                ct_slide(&MV, DECK_FRENCH, p->hand[q][i], T.sx, T.cy, T.wp, 0, 0, s[i].x, s[i].y, w, s[i].deg, 1, 300,
                         k * 50, H_HAND, q, p->hand[q][i], k % 3 ? -1 : SND_MOVE);
        } else {
            for (int i = 0; i < p->n[q]; i++, k++) {
                float x, y, a;
                ct_backs_end(DECK_FRENCH, seat_x(q), T.sy, T.sfan, T.sw, i + 1, &x, &y, &a);
                ct_slide(&MV, DECK_FRENCH, CARD_NONE, T.sx, T.cy, T.wp, 0, 0, x, y, T.sw, a, 0, 260, k * 50, H_SEAT,
                         q, -1, k % 3 ? -1 : SND_MOVE);
            }
        }
    }
}

/* a question: the card given or drawn flies into the hand; a family complete flies down to its badge; a card drawn
 * by a player whose turn came with an empty hand */
static void animate(const fpos *b)
{
    const fpos *p = &G.pos;
    int pl = b->turn, given = p->answer == FAM_GIVEN, in = given ? p->ask_card : p->drawn, delay = 0;
    if (pl != T.me) return;
    float x0 = given ? seat_x(p->ask_to) : T.sx, y0 = given ? T.sy : T.cy, w0 = given ? T.sw : T.wp, a0 = 0;
    if (given) ct_backs_end(DECK_FRENCH, x0, T.sy, T.sfan, T.sw, b->n[p->ask_to], &x0, &y0, &a0);
    int laid = p->nlaid[pl] > b->nlaid[pl] ? p->laid[pl][p->nlaid[pl] - 1] : -1;
    if (in != FAM_NONE) {
        cards_spot s[FAM_CARDS];
        float w;
        if (laid >= 0 && FAM_FAMILY(in) == laid) {   /* it completes a family: in the hand, then all six down */
            uint8_t v[FAM_CARDS];
            int n = 0, put = 0;
            for (int i = 0; i < b->n[pl]; i++) {
                if (!put && b->hand[pl][i] > in) v[n++] = (uint8_t)in, put = 1;
                v[n++] = b->hand[pl][i];
            }
            if (!put) v[n++] = (uint8_t)in;
            int j = spot_of(v, n, in, s, &w), first;
            first = ct_slide(&MV, DECK_FRENCH, in, x0, y0, w0, a0, 0, s[j].x, s[j].y, w, s[j].deg, 1, SLIDE_MS, 0, 0,
                             -1, -1, SND_PLACE);
            float lx, ly, ls;
            laid_at(pl, p->nlaid[pl] - 1, &lx, &ly, &ls);
            for (int m = 0, k = 0; m < FAM_MEMBERS; m++) {
                int c = FAM_CARD(laid, m), i = spot_of(v, n, c, s, &w);
                if (i < 0) continue;
                int mv = ct_slide(&MV, DECK_FRENCH, c, s[i].x, s[i].y, w, s[i].deg, 1, lx, ly, ls * 0.7f, 0, 1,
                                  SLIDE_MS, SLIDE_MS + 350 + 60 * k, H_LAID, pl, laid, k ? -1 : SND_CAPTURE);
                if (c == in) ct_after(&MV, mv, first);
                k++;
            }
            delay = 2 * SLIDE_MS + 750;
        } else {
            int j = spot_of(p->hand[pl], p->n[pl], in, s, &w);
            if (j >= 0)
                ct_slide(&MV, DECK_FRENCH, in, x0, y0, w0, a0, 0, s[j].x, s[j].y, w, s[j].deg, 1, SLIDE_MS, 0,
                         H_HAND, pl, in, SND_PLACE);
            delay = SLIDE_MS;
        }
        if (p->answer != FAM_MISSED) flash_card = in, flash_t = ui_ms() + (Uint32)delay + 900;
    }
    for (int q = 0; q < p->players; q++) {      /* an empty hand's draw */
        if (q == pl || p->n[q] <= b->n[q] - (given && q == p->ask_to)) continue;
        float x, y, a;
        ct_backs_end(DECK_FRENCH, seat_x(q), T.sy, T.sfan, T.sw, p->n[q], &x, &y, &a);
        ct_slide(&MV, DECK_FRENCH, CARD_NONE, T.sx, T.cy, T.wp, 0, 0, x, y, T.sw, a, 0, SLIDE_MS, delay, H_SEAT, q, -1,
                 SND_MOVE);
    }
    if (p->refills > b->refills && p->turn == pl && p->n[pl] == 1) {   /* the asker's own, after laying the last */
        cards_spot s[FAM_CARDS];
        float w = hand_spots(1, s);
        ct_slide(&MV, DECK_FRENCH, p->hand[pl][0], T.sx, T.cy, T.wp, 0, 0, s[0].x, s[0].y, w, s[0].deg, 1, SLIDE_MS,
                 delay, H_HAND, pl, p->hand[pl][0], SND_MOVE);
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    fam_new(&G, ui.players, (uint32_t)ui_random(1 << 30));
    ct_calm(&MV);
    dealing = 1;
    target = chosen = -1;
}

static unsigned winners(void) { return fam_winners(&G.pos); }

static int status(void)
{
    unsigned w = winners();
    if (!w) return GS_PLAY;
    if (w & (w - 1)) return GS_DRAW;
    return gs_win(__builtin_ctz(w));
}

static int turn(void) { return G.pos.turn; }

/* the player asked: the one chosen, else the next who may be asked */
static int asked(void)
{
    const fpos *p = &G.pos;
    if (fam_askable(p, target)) return target;
    for (int k = 1; k < p->players; k++)
        if (fam_askable(p, (p->turn + k) % p->players)) return (p->turn + k) % p->players;
    return -1;
}

static void ask(int card)
{
    fpos before = G.pos;
    int q = asked();
    if (q < 0 || !fam_play(&G, FAM_ASK(q, card))) { ui_sound(SND_DENY); return; }
    ct_calm(&MV);
    animate(&before);
    chosen = -1;
    if (G.pos.answer == FAM_MISSED) ui_sound(SND_DROP);
    if (G.pos.phase == FP_ASK) ui_handoff(G.pos.turn);
    if (G.pos.turn != before.turn) target = -1;
}

static int undo(void) { return 0; }             /* every answer shows a card: none taken back */

static int script(const char *s)
{
    int ok = fam_script(&G, s);
    ct_calm(&MV);
    dealing = 0;
    target = chosen = -1;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_tok[MAX_PLAYERS], *t_badge[FAM_FAMILIES], *t_badge_s[FAM_FAMILIES];
static int tok_made, badge_made, badge_s_made;

static void badges(SDL_Texture **t, int *made, int size)
{
    if (size < 4 || *made == size) return;
    *made = size;
    for (int f = 0; f < FAM_FAMILIES; f++) {
        ui_free(&t[f]);
        fam_emblem(f, size, ui_buf(size, size));
        t[f] = ui_tex(size, size, ui_buf(size, size));
    }
}

static void draw(void)
{
    const fpos *p = &G.pos;
    if (dealing && ui.viewer >= 0) { dealing = 0; ct_calm(&MV); deal_slides(); }   /* the cover lifted: it flies */
    int mine = p->turn == T.me && !ui_handing() && p->phase == FP_ASK, ready = mine && !ct_moving(&MV);
    int to = asked();
    ct_tokens(t_tok, &tok_made, (int)(0.62f * T.aw));
    badges(t_badge, &badge_made, (int)T.ls);
    badges(t_badge_s, &badge_s_made, (int)T.es);
    /* the stock */
    cards_pile(DECK_FRENCH, T.sx, T.cy, T.wp, p->nstock, CARD_NONE, 0);
    /* the others: their hands (the one asked in gold light), their families */
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) continue;
        float x = seat_x(q), ch = T.sw * 1.4f;
        int n = p->n[q] - ct_hides(&MV, H_SEAT, q, -1);
        if (ready && q == to) ui_mark_gold(x, T.sy, 2.4f * T.sw);
        ct_backs(DECK_FRENCH, x, T.sy, T.sfan, T.sw, n, 0);
        if (!n) cards_place(DECK_FRENCH, x, T.sy, T.sw);
        ct_label(t_tok[q], x, T.sy + 0.5f * ch + 0.32f * T.sw, 0.46f * T.sw, n);
        for (int i = 0; i < p->nlaid[q]; i++) {
            float bx, by, bs;
            laid_at(q, i, &bx, &by, &bs);
            ui_put_c(t_badge_s[p->laid[q][i]], bx, by, bs, bs);
        }
    }
    for (int i = 0; i < p->nlaid[T.me]; i++) {  /* the viewer's families */
        if (ct_hides(&MV, H_LAID, T.me, p->laid[T.me][i])) continue;
        float bx, by, bs;
        laid_at(T.me, i, &bx, &by, &bs);
        ui_put_c(t_badge[p->laid[T.me][i]], bx, by, bs, bs);
    }
    /* the middle: the members of the family chosen, or the last question (who asked whom, for what) */
    float y = T.cy;
    if (chosen >= 0 && ready) {
        for (int m = 0; m < FAM_MEMBERS; m++) {
            int c = FAM_CARD(chosen, m), held = fam_holds(p, T.me, c);
            fam_draw(c, T.ax + m * 1.06f * T.aw, y, T.aw, 0, held ? CD_DIM : CD_GLOW);
        }
    } else if (p->ask_from != FAM_NONE) {
        float cx = T.ax + 2.65f * 1.06f * T.aw, d = 0.62f * T.aw, gap = 1.25f * T.aw;
        ui_put_c(t_tok[p->ask_from], cx - gap, y, d, d);
        ui_put_c(t_tok[p->ask_to], cx + gap, y, d, d);
        fam_draw(p->ask_card, cx, y, T.aw, 0, p->answer == FAM_MISSED ? CD_DIM : CD_GLOW);
        if (p->answer != FAM_GIVEN)             /* pioche: a back over the stock's way */
            cards_draw(DECK_FRENCH, CARD_NONE, cx + 0.45f * T.aw, y + 0.5f * T.aw, 0.5f * T.aw, 12,
                       CD_DOWN | (p->answer == FAM_LUCKY ? CD_GLOW : 0));
    }
    /* the hand: the family chosen raised (under the hand-off cover: face down) */
    cards_spot s[FAM_CARDS];
    int n = p->n[T.me];
    float w = hand_spots(n, s);
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        if (ct_hides(&MV, H_HAND, T.me, c)) continue;
        if (ui.viewer < 0) { fam_draw(CARD_NONE, s[i].x, s[i].y, w, s[i].deg, CD_DOWN); continue; }
        int how = ready && chosen < 0 ? CD_GLOW : ready && FAM_FAMILY(c) == chosen ? CD_RAISED : 0;
        if (c == flash_card && ui_ms() < flash_t) { ui_mark_gold(s[i].x, s[i].y, 1.7f * w); ui_animate(); }
        fam_draw(c, s[i].x, s[i].y, w, s[i].deg, how);
    }
    ct_draw(&MV, fam_slide_draw);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    const fpos *p = &G.pos;
    if (p->phase != FP_ASK || ct_moving(&MV)) return;
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    for (int q = 0; q < p->players; q++) {      /* a player to ask */
        if (q == T.me || fabsf(x - seat_x(q)) > T.b.w / T.k / 2 || fabsf(y - T.sy) > T.sw * 1.4f) continue;
        int was = target;
        target = q;
        if (asked() != q) { target = was; ui_sound(SND_DENY); }
        else ui_sound(SND_TAP);
        return;
    }
    if (chosen >= 0)                            /* a member of the family chosen */
        for (int m = 0; m < FAM_MEMBERS; m++)
            if (cards_in(T.ax + m * 1.06f * T.aw, T.cy, T.aw, 1.4f, x, y)) {
                int c = FAM_CARD(chosen, m);
                if (fam_holds(p, T.me, c)) ui_sound(SND_DENY);
                else ask(c);
                return;
            }
    cards_spot s[FAM_CARDS];
    float w = hand_spots(p->n[T.me], s);
    int i = cards_hit(s, p->n[T.me], w, 1.4f, x, y);
    if (i >= 0) {
        int f = FAM_FAMILY(p->hand[T.me][i]);
        chosen = chosen == f ? -1 : f;
        ui_sound(SND_TAP);
        return;
    }
    if (chosen >= 0) { chosen = -1; return; }
    ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the families laid down */
{
    float h = fminf(0.46f * ui.unit, b.w / 1.6f);
    ui_number(b.x + 0.37f * h, b.y + b.h / 2, h, G.pos.nlaid[player], 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: three cards of three families in a cascade */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[3] = {FAM_CARD(FAM_FROGS, FAM_GRANDFATHER), FAM_CARD(FAM_RABBITS, FAM_MOTHER),
                                 FAM_CARD(FAM_BEARS, FAM_DAUGHTER)};
    float w = size * 0.54f;
    int cw = (int)w, ch = (int)(w * 1.4f);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    memset(rgba, 0, (size_t)size * size * 4);
    for (int i = 0; i < 3; i++) {
        fam_paint(cards[i], cw, ch, b);
        art_blend(rgba, size, size, b, cw, ch, (int)(size * (0.03f + 0.2f * i)), (int)(size * (0.21f - 0.09f * i)));
    }
    free(b);
}

static const game families_game = {
    .id = "families", .order = 301, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .view = VIEW_HIDDEN,
};
REGISTER_GAME(families_game)
