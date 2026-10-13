/*
 * Skat in RetroStone Board: three players passing the device round, each hand secret (VIEW_HIDDEN; the rules:
 * skat_rules.c, skat.h). The table (the kit's cards_table.h): the viewer's hand fanned at the bottom, the two others'
 * seats round the top, the trick with each card placed towards its player, the trick pile (tap it: the last trick). The
 * skat lies face down at the top of the table while the players bid. Bidding (the kit's calls): the one who says taps a
 * value on a pad of the bid values (arrows: the higher ones), then the tick, or the pass button; the one who hears sees
 * the value said and taps the tick (hold) or pass. Each seat shows its last word (the value in a dark disc; a tick:
 * held; the pass icon). The declarer then taps the skat to take it up, or names a hand game at once: a row of the games
 * (the four suits, grand: the jack's figure, null: a card crossed out), a row of announcements (90: schneider, 120:
 * schwarz, the open cards: ouvert) and the tick. With the skat taken up, he taps two cards to lay them away (tapped in
 * the skat: back to the hand). An ouvert hand lies face up at the declarer's seat for all to see. After each deal a
 * table: the game, its bid, the matadors, the levels times the base, the value, the skat and the card points, each
 * player's score and Seeger-Fabian points and total, and the list of deals (a chip of each declarer: gold, won; dimmed,
 * lost; an empty ring, thrown in).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/chess.h"
#include "games/skat.h"
#include "games/tricks.h"
#include "shell.h"

#define PAD 15                                  /* the bid values shown at once */
enum { PIC_NULL = 8, PIC_OUVERT };               /* the game's own pictures (grand: the kit's, the jack's figure) */
enum { TAG_SKAT = 10 };

static skgame G;
static ct_table T;
static ct_moves A;
static ct_touch F;
static int floor_n, show_last, pick = -1, page, gtype = -1, gflags;
static unsigned seen;
static int16_t BIDS[80];
static int nbids;

/* ---- the game's pictures ----------------------------------------------------------------------------------------- */
static void paint(int kind, int s, uint8_t *rgba)
{
    float asp = cards_aspect(DECK_FRENCH);
    memset(rgba, 0, (size_t)s * s * 4);
    int n = kind == PIC_OUVERT ? 3 : 1, cw = (int)(s * (n == 3 ? 0.42f : 0.44f)), ch = (int)(cw * asp);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    if (kind == PIC_NULL) {                     /* on an ivory disc, like the other games */
        ct_paint_ivory(rgba, s);
        cards_disc_rim(rgba, s);
    }
    static const int faces[3] = {CARD(SUIT_CLUBS, RANK_JACK), CARD(SUIT_HEARTS, RANK_ACE), CARD(SUIT_SPADES, 10)};
    for (int i = 0; i < n; i++) {               /* null: a card's back; ouvert: three cards face up */
        cards_paint(DECK_FRENCH, n == 1 ? CARD_NONE : faces[i], cw, ch, b);
        int x = n == 1 ? (s - cw) / 2 : (int)(0.06f * s + i * 0.25f * s), y = (s - ch) / 2 + (n == 3 ? (i - 1) * s / 40 : 0);
        art_blend(rgba, s, s, b, cw, ch, x, y);
    }
    free(b);
    if (kind != PIC_NULL) return;
    art_fig *f = art_fig_new();                 /* crossed out: a red cross */
    const float a[] = {0.22f, 0.27f, 0.27f, 0.22f, 0.78f, 0.73f, 0.73f, 0.78f};
    const float c[] = {0.73f, 0.22f, 0.78f, 0.27f, 0.27f, 0.78f, 0.22f, 0.73f};
    art_poly(f, ART_PART, a, 4);
    art_poly(f, ART_PART, c, 4);
    art_style st = {{196, 40, 34}, {70, 10, 8}, {70, 10, 8}, {0, 0, 0}, 0.25f, 0.025f};
    ct_paint_fig(rgba, s, f, &st);
}

static int game_kind(int type) { return type < 4 ? type : type == SG_GRAND ? CT_CALL_FIG | BISHOP : PIC_NULL; }

/* a game's picture (a suit's sign, grand, null, ouvert) d wide, faded (alpha) */
static SDL_Texture *game_pic(int kind, float d)
{
    return kind == PIC_NULL || kind == PIC_OUVERT ? ct_picture(paint, kind, (int)(d + 0.5f)) : ct_call_tex(kind, d);
}

static void put_pic(int kind, float x, float y, float d, Uint8 alpha)
{
    SDL_Texture *t = game_pic(kind, d);
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_c(t, x, y, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b) { ct_layout(&T, b, SKAT_P, ui_bottom(), SKAT_HAND); }

static void reset_ui(void)
{
    pick = -1;
    page = 0;
    gtype = -1;
    gflags = 0;
    show_last = 0;
    ct_touch_reset(&F);
}

static int hand(uint8_t *h, int *how, int live)
{
    const skpos *p = &G.pos;
    int n = p->n[T.me], mine = live && p->turn == T.me;
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        h[i] = (uint8_t)c;
        how[i] = 0;
        if (mine && p->phase == SK_PLAY) how[i] = skat_can_play(p, c) ? CD_GLOW : CD_DIM;
        if (mine && p->phase == SK_DISCARD && p->nskat < 2) how[i] = CD_GLOW;
        if (ct_is_raised(&F, i)) how[i] = CD_RAISED;
    }
    return n;
}

/* ---- the controls in the middle (the viewer's turn) -------------------------------------------------------------- */
typedef struct { float x, y; } pt;
static struct {
    float d, sw;                                /* a button's diameter; the skat's cards' width */
    pt skat[2];
    pt pad[PAD];
    int first, npad;                            /* the pad's first value (an index in BIDS), how many */
    pt pass, ok, prev, next, held;              /* held: the value to hold (the hearer) */
    pt type[SG_TYPES], flag[3];
} K;

static const int TYPE_ORDER[SG_TYPES] = {SUIT_CLUBS, SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS, SG_GRAND, SG_NULL};
static const int FLAG_OF[3] = {SF_SCHNEIDER, SF_SCHWARZ, SF_OUVERT};

static void controls(void)
{
    const skpos *p = &G.pos;
    float u = T.u, top = T.b.y + 0.15f * u, bot = fminf(T.hand_top, T.sy[T.me] - 0.8f * T.tok) - 0.1f * u;
    float zw = T.b.w * 0.94f;
    int deciding = p->phase == SK_SKAT || p->phase == SK_DISCARD;
    K.sw = floorf(fminf(deciding ? T.wt : 0.62f * T.wt, (bot - top) * (deciding ? 0.26f : 0.2f) / T.asp));
    float sh = K.sw * T.asp;
    for (int i = 0; i < 2; i++) K.skat[i].x = T.cx + (i - 0.5f) * 1.12f * K.sw, K.skat[i].y = top + sh / 2;
    float y0 = top + sh + 0.25f * u, room = bot - y0;   /* below the skat */
    if (p->phase == SK_BID) {
        int fi = 0;
        while (fi < nbids && BIDS[fi] <= p->bid) fi++;
        K.first = fi + page * PAD;
        K.npad = nbids - K.first < PAD ? nbids - K.first : PAD;
        if (skat_saying(p)) {
            K.d = floorf(fminf(fminf(u, zw / 5.7f), room / (3 * 1.12f + 1.3f)));
            float h = 3 * 1.12f * K.d + 1.3f * K.d, ya = y0 + (room - h) / 2 + 0.56f * K.d;
            for (int i = 0; i < PAD; i++) K.pad[i].x = T.cx + (i % 5 - 2) * 1.12f * K.d, K.pad[i].y = ya + (i / 5) * 1.12f * K.d;
            float yc = ya + 3 * 1.12f * K.d + 0.12f * K.d;
            K.prev = (pt){T.cx - 1.95f * K.d, yc};
            K.pass = (pt){T.cx - 0.65f * K.d, yc};
            K.ok = (pt){T.cx + 0.65f * K.d, yc};
            K.next = (pt){T.cx + 1.95f * K.d, yc};
        } else {
            K.d = floorf(fminf(u, room / 2.9f));
            float ya = y0 + (room - 2.75f * K.d) / 2 + 0.7f * K.d;
            K.held = (pt){T.cx, ya};
            K.pass = (pt){T.cx - 0.7f * K.d, ya + 1.55f * K.d};
            K.ok = (pt){T.cx + 0.7f * K.d, ya + 1.55f * K.d};
        }
        return;
    }
    /* the game: a row of the six games, a row of the announcements and the tick */
    K.d = floorf(fminf(fminf(u, zw / 6.8f), room / 2.6f));
    float ya = y0 + (room - 2.45f * K.d) / 2 + 0.55f * K.d;
    for (int i = 0; i < SG_TYPES; i++) K.type[TYPE_ORDER[i]] = (pt){T.cx + (i - 2.5f) * 1.12f * K.d, ya};
    for (int i = 0; i < 3; i++) K.flag[i] = (pt){T.cx + (i - 1.5f) * 1.3f * K.d, ya + 1.3f * K.d};
    K.ok = (pt){T.cx + 1.5f * 1.3f * K.d, ya + 1.3f * K.d};
}

/* what the panel lets the declarer choose now */
static int hand_now(void) { return G.pos.phase == SK_SKAT; }

static int flag_enabled(int i)
{
    if (gtype == SG_NULL) return FLAG_OF[i] == SF_OUVERT;
    return hand_now();
}

static int type_enabled(int t)                  /* null: one of its values must reach the bid */
{
    return t != SG_NULL || skat_base(SG_NULL, SF_OUVERT, hand_now()) >= G.pos.bid;
}

static int game_ready(void) { return gtype >= 0 && skat_game_ok(&G.pos, gtype, gflags); }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    if (!nbids) nbids = skat_bids(BIDS);
    skat_new(&G, v, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    const skpos *p = &G.pos;
    if (p->phase != SK_OVER) return GS_PLAY;
    int m = skat_winners(p);
    for (int q = 0; q < SKAT_P; q++)
        if (m == 1 << q) return gs_win(q);
    return GS_DRAW;
}

static unsigned winners(void) { return (unsigned)skat_winners(&G.pos); }

static int turn(void) { return G.pos.turn; }

/* where the viewer's card c lies in a hand of n (h) as fanned */
static int spot_of(const uint8_t *h, int n, int c, cards_spot *s)   /* the card's index in the hand (-1: none) */
{
    cards_spot all[CT_HAND];
    ct_fan(&T, n, all);
    int i = ct_index(h, n, c);
    if (i >= 0) *s = all[i];
    return i;
}

static void animate(const skpos *b, uint32_t a, const uint8_t *h, int n)
{
    const skpos *p = &G.pos;
    int k = SKA_ARG(a), at;
    cards_spot s;
    controls();
    switch (SKA_KIND(a)) {
    case SA_SAY:
    case SA_HOLD: ui_sound(SND_PLACE); break;
    case SA_PASS: ui_sound(SND_TAP); break;
    case SA_PICKUP:                             /* the skat's two cards into the hand, turning over */
        for (int i = 0; i < 2; i++)
            if ((at = spot_of(p->hand[T.me], p->n[T.me], b->skat[i], &s)) >= 0)
                ct_slide(&A, DECK_FRENCH, b->skat[i], K.skat[i].x, K.skat[i].y, K.sw, 0, 0, s.x, s.y, T.wh, s.deg, 1,
                         CT_SLIDE_MS + 60, 90 * i, CT_HAND_TAG, at, -1, i == 0 ? SND_MOVE : -1);
        break;
    case SA_PUT:                                /* a card laid away: from the hand to its place in the skat */
        if (spot_of(h, n, k, &s) >= 0)
            ct_slide(&A, DECK_FRENCH, k, s.x, s.y, T.wh, s.deg, 1, K.skat[b->nskat].x, K.skat[b->nskat].y, K.sw, 0, 0,
                     CT_SLIDE_MS, 0, TAG_SKAT, b->nskat, -1, SND_PLACE);
        break;
    case SA_TAKE: {                             /* back to the hand */
        int i = b->skat[0] == k ? 0 : 1;
        if ((at = spot_of(p->hand[T.me], p->n[T.me], k, &s)) >= 0)
            ct_slide(&A, DECK_FRENCH, k, K.skat[i].x, K.skat[i].y, K.sw, 0, 0, s.x, s.y, T.wh, s.deg, 1, CT_SLIDE_MS,
                     0, CT_HAND_TAG, at, -1, SND_PLACE);
        break;
    }
    case SA_GAME: ui_sound(SND_PLACE); break;
    case SA_PLAY:
        ct_play(&A, &T, &F, h, n, k);
        if (p->tricks != b->tricks || p->phase != b->phase)
            ct_gather(&A, &T, p->last, NULL, SKAT_P, p->last_leader, p->last_winner, CT_SLIDE_MS + CT_LOOK_MS,
                      CT_SLIDE_MS);
        break;
    }
}

static void act(uint32_t a)
{
    skpos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 0);
    if (!skat_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, a, h, n);
    int keep_game = SKA_KIND(a) == SA_PUT || SKA_KIND(a) == SA_TAKE || SKA_KIND(a) == SA_PICKUP, gt = gtype, gf = gflags;
    reset_ui();
    if (keep_game) gtype = gt, gflags = gf;     /* the game chosen stays while the skat is made */
    if (SKA_KIND(a) == SA_PICKUP) gflags &= gtype == SG_NULL ? SF_OUVERT : 0;   /* announcements: hand only */
    const skpos *p = &G.pos;
    if (SKA_KIND(a) == SA_NEXT) seen = 0;
    if (p->turn != before.turn || p->phase == SK_DEAL || p->phase == SK_OVER) floor_n = G.n;
    if (SKA_KIND(a) == SA_PICKUP) floor_n = G.n;   /* the skat seen: no going back to a hand game */
    if (p->phase < SK_DEAL) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick >= 0 || page || gtype >= 0 || gflags || F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }
    if (G.n <= floor_n || !skat_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    if (!nbids) nbids = skat_bids(BIDS);
    int ok = skat_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

/* the bot's side (games/skat_bot.c): the game on the screen, read-only; one action of the notation, played as a tap */
const skgame *skat_live(void) { return &G; }

int skat_show(const char *move)
{
    static skgame t;
    t = G;
    if (!skat_script(&t, move) || t.n != G.n + 1) return 0;
    act(t.act[G.n]);
    return 1;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_flag(int i, float x, float y, float d, int on, int enabled)
{
    if (on) ui_mark_gold(x, y, 1.8f * d);
    ct_button(x, y, d, on, -1, 0);
    Uint8 a = enabled ? 255 : 90;
    if (FLAG_OF[i] == SF_OUVERT) { put_pic(PIC_OUVERT, x, y, 0.82f * d, a); return; }
    ui_number(x, y, (i == 0 ? 0.42f : 0.34f) * d, i == 0 ? 90 : 120, 1);
    if (!enabled) ct_pic(CT_DISC_OFF, x, y, d, 0, 255, 255, 255, 150);
}

static void draw_bid(int live)
{
    const skpos *p = &G.pos;
    if (skat_saying(p)) {
        for (int i = 0; i < K.npad; i++) {
            int v = BIDS[K.first + i];
            ct_button(K.pad[i].x, K.pad[i].y, K.d, v == pick, -1, 0);
            ui_number(K.pad[i].x, K.pad[i].y, (v >= 100 ? 0.36f : 0.44f) * K.d, v, 1);
        }
        if (page > 0) ct_button(K.prev.x, K.prev.y, 0.8f * K.d, 0, CT_ARROW, 180);
        if (K.first + PAD < nbids) ct_button(K.next.x, K.next.y, 0.8f * K.d, 0, CT_ARROW, 0);
        ct_pass(K.pass.x, K.pass.y, K.d, 0);
        if (pick >= 0 && live) ui_mark_gold(K.ok.x, K.ok.y, 1.9f * K.d);
        ct_button(K.ok.x, K.ok.y, K.d, pick >= 0, CT_TICK, 0);
        return;
    }
    ct_badge(K.held.x, K.held.y, 1.4f * K.d, p->bid);
    ct_pass(K.pass.x, K.pass.y, K.d, 0);
    ct_button(K.ok.x, K.ok.y, K.d, 1, CT_TICK, 0);
}

static void draw_panel(int live)
{
    const skpos *p = &G.pos;
    for (int t = 0; t < SG_TYPES; t++) ct_choice(game_pic(game_kind(t), K.d), K.type[t].x, K.type[t].y, K.d,
                                                 t == gtype, type_enabled(t));
    for (int i = 0; i < 3; i++) draw_flag(i, K.flag[i].x, K.flag[i].y, K.d, (gflags & FLAG_OF[i]) != 0,
                                          flag_enabled(i));
    if (game_ready() && live) ui_mark_gold(K.ok.x, K.ok.y, 1.9f * K.d);
    ct_button(K.ok.x, K.ok.y, K.d, game_ready(), CT_TICK, 0);
    (void)p;
}

/* the skat at the top of the table: face down while bidding (glowing for the declarer: take it), the cards laid
 * away while the declarer makes his skat */
static void draw_skat(int live, int hide)
{
    const skpos *p = &G.pos;
    int mine = p->turn == T.me && !hide;
    if (p->phase == SK_BID || p->phase == SK_SKAT) {
        for (int i = 0; i < 2; i++)
            cards_draw(DECK_FRENCH, CARD_NONE, K.skat[i].x, K.skat[i].y, K.sw, (i - 0.5f) * 4,
                       CD_DOWN | (p->phase == SK_SKAT && mine && live ? CD_GLOW : 0));
        return;
    }
    if (p->phase != SK_DISCARD) return;
    for (int i = 0; i < 2; i++) {
        if (i >= p->nskat || ct_hides(&A, TAG_SKAT, i, -1)) { cards_place(DECK_FRENCH, K.skat[i].x, K.skat[i].y,
                                                                          K.sw); continue; }
        cards_draw(DECK_FRENCH, mine ? p->skat[i] : CARD_NONE, K.skat[i].x, K.skat[i].y, K.sw, 0, mine ? 0 : CD_DOWN);
    }
}

/* a seat's word in the bidding, or the declarer's game */
static void draw_words(void)
{
    const skpos *p = &G.pos;
    float d = 0.95f * T.tok;
    for (int q = 0; q < SKAT_P; q++) {
        float x, y, x1, y1;
        ct_seat_spot(&T, q, 0, &x, &y);
        ct_seat_spot(&T, q, 1, &x1, &y1);
        if (p->phase == SK_BID) {
            if (p->word[q] == SW_PASSED) ct_pass_badge(x, y, d);
            else if (p->word[q] != SW_NONE) {
                ct_badge(x, y, d, p->said[q]);
                if (p->word[q] == SW_HELD) ct_pic(CT_TICK, x1, y1, 0.8f * d, 0, CT_GOLD[0], CT_GOLD[1], CT_GOLD[2], 255);
            }
            continue;
        }
        if (q != p->declarer || p->phase == SK_DEAL || p->phase == SK_OVER) continue;
        ct_badge(x, y, d, p->bid);
        if (p->phase != SK_PLAY) continue;
        put_pic(game_kind(p->type), x1, y1, d, 255);
        float x2 = x1 + (x1 - x), sw = 0.32f * d;
        if (p->hand_game)                       /* hand: the skat untouched, two backs */
            for (int i = 0; i < 2; i++)
                cards_draw(DECK_FRENCH, CARD_NONE, x2 + (i - 0.5f) * 0.5f * sw, y1, sw, (i - 0.5f) * 10,
                           CD_DOWN | CD_FLAT);
        if (p->type != SG_NULL && (p->flags & SF_SCHNEIDER))
            ui_number(x2 + (x1 - x) * (p->hand_game ? 1 : 0), y1, 0.4f * d, p->flags & SF_SCHWARZ ? 120 : 90, 1);
    }
}

/* ouvert: the declarer's hand face up at his seat, for all to see */
static void draw_open(int hide)
{
    const skpos *p = &G.pos;
    int d = p->declarer;
    if (p->phase != SK_PLAY || !(p->flags & SF_OUVERT) || (d == T.me && !hide)) return;
    if (d == T.me) {                            /* under the cover: the viewer's own open hand, still face up */
        uint8_t h[CT_HAND];
        int how[CT_HAND];
        ct_hand_draw(&T, &A, h, hand(h, how, 0), how, NULL, -1);
        return;
    }
    int n = p->n[d];
    float y1 = T.sy[d] - 0.75f * T.tok, y0 = T.b.y + 0.08f * T.u;
    float w = floorf(fminf(0.55f * T.wt, (y1 - y0) / T.asp)), step = 0.52f * w;
    float span = w + step * (n - 1), cx = clampf(T.sx[d], T.b.x + span / 2 + 0.1f * T.u,
                                                 T.b.x + T.b.w - span / 2 - 0.1f * T.u);
    if (w < 8) return;
    for (int i = 0; i < n; i++)
        cards_draw(DECK_FRENCH, p->hand[d][i], cx - span / 2 + w / 2 + i * step, (y0 + y1) / 2, w, 0, 0);
}

/* ---- the deal's table -------------------------------------------------------------------------------------------- */
static void times(float x, float y, float h)    /* a times sign: two strokes crossing */
{
    ct_pic(CT_SLASH, x, y, h, 0, 246, 238, 220, 255);
    ct_pic(CT_SLASH, x, y, h, -60, 246, 238, 220, 255);
}

static void draw_summary(void)
{
    const skpos *p = &G.pos;
    const skdeal *r = &p->list[p->ndeals - 1];
    int N = skat_deals(p->variant), lrows = (N + 11) / 12, thrown = r->declarer == SKAT_NONE;
    int rows = (thrown ? 1 : 3) + SKAT_P + lrows;
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, rows, &x0, &pw, &y, &rh);
    float d = floorf(fminf(0.68f * rh, 0.8f * u)), L = x0 + 0.35f * u + d / 2, R = x0 + pw - 0.35f * u - d / 2;
    float nh = 0.42f * rh;
    const int ts = (int)d;
    if (thrown) {                               /* everyone passed */
        for (int q = 0; q < SKAT_P; q++) {
            float x = x0 + pw * (q + 0.5f) / SKAT_P - 0.45f * d;
            ui_put_c(ct_token(q, ts), x, y, d, d);
            ct_pass_badge(x + 0.95f * d, y, 0.8f * d);
        }
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
        y += rh;
    } else {
        int dc = r->declarer;
        /* the game: the declarer, the game, hand, the announcements; the bid */
        ui_put_c(ct_token(dc, ts), L, y, d, d);
        float x = L + 1.05f * d;
        put_pic(game_kind(r->type), x, y, 0.9f * d, 255);
        x += 0.95f * d;
        if (r->hand) {
            for (int i = 0; i < 2; i++)
                cards_draw(DECK_FRENCH, CARD_NONE, x + (i - 0.5f) * 0.28f * d, y, 0.5f * d, (i - 0.5f) * 10, CD_DOWN);
            x += 0.9f * d;
        }
        if (r->type != SG_NULL && (r->flags & SF_SCHNEIDER)) {
            x += 0.2f * d;
            ui_number(x, y, 0.5f * nh, 90, 1);
            x += 0.75f * d;
        }
        if (r->type != SG_NULL && (r->flags & SF_SCHWARZ)) {
            ui_number(x + 0.15f * d, y, 0.5f * nh, 120, 1);
            x += 1.0f * d;
        }
        if (r->flags & SF_OUVERT) put_pic(PIC_OUVERT, x + 0.2f * d, y, 0.8f * d, 255);
        ct_badge(R, y, 0.9f * d, r->bid);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
        y += rh;
        /* the value: with or without (the club jack, dimmed and struck through: without) and how many; the levels
         * times the base; the value (raised to the bid when overbid: a red glow) */
        x = L;
        if (r->type != SG_NULL) {
            float cw = 0.5f * d;
            cards_draw(DECK_FRENCH, CARD(SUIT_CLUBS, RANK_JACK), x, y, cw, 0, r->matadors < 0 ? CD_DIM : 0);
            if (r->matadors < 0) ct_pic(CT_SLASH, x, y, 0.9f * cw * T.asp, 0, 230, 70, 60, 255);
            ui_number(x + 0.85f * d, y, nh, abs(r->matadors), 1);
            float xl = x0 + pw * 0.42f;
            ui_number(xl, y, nh, r->levels, 1);
            times(xl + 0.6f * nh + ct_number_w(r->levels, nh, 0) / 2, y, 0.8f * nh);
            float xb = xl + 1.2f * nh + ct_number_w(r->levels, nh, 0) / 2 + ct_number_w(r->base, nh, 0) / 2;
            ui_number(xb, y, nh, r->base, 1);
            ct_pic(CT_ARROW, xb + ct_number_w(r->base, nh, 0) / 2 + 0.55f * nh, y, 0.8f * nh, 0, 246, 238, 220, 200);
        } else
            put_pic(PIC_NULL, x, y, 0.8f * d, 255);
        float vh = 0.5f * rh;
        if (r->overbid) ui_mark_glow(R, y, 1.4f * d);
        ui_number(R, y, vh, r->value, 1);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
        y += rh;
        /* the skat, the declarer's card points (null: his tricks) and the outcome: a gold tick, or lost (a red glow) */
        float cw = 0.46f * d;
        for (int i = 0; i < 2; i++) cards_draw(DECK_FRENCH, p->skat[i], L + (i - 0.5f) * 0.9f * cw + 0.2f * d, y, cw,
                                               (i - 0.5f) * 8, 0);
        float px = x0 + pw * 0.42f;
        if (!r->won) ui_mark_glow(px, y, 1.3f * d);
        ui_number(px, y, nh, r->type == SG_NULL ? r->tricks : r->points, 1);
        if (r->won) ct_pic(CT_TICK, R, y, 0.9f * d, 0, CT_GOLD[0], CT_GOLD[1], CT_GOLD[2], 255);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
        y += rh;
    }
    /* each player: the deal's score, the Seeger-Fabian points, the total (the best in gold light) */
    int best = -100000;
    for (int q = 0; q < SKAT_P; q++) best = skat_total(p, q) > best ? skat_total(p, q) : best;
    for (int q = 0; q < SKAT_P; q++, y += rh) {
        int sc = 0, sf = 0;
        if (!thrown) {
            if (q == r->declarer) sc = r->score, sf = r->won ? 50 : -50;
            else sf = r->won ? 0 : 40;
        }
        if (skat_total(p, q) == best) ui_mark_gold(L, y, 1.8f * d);
        ui_put_c(ct_token(q, ts), L, y, d, d);
        if (sc) ui_number_signed(x0 + pw * 0.36f, y, nh, sc, 1, 1);
        if (sf) ui_number_signed(x0 + pw * 0.58f, y, 0.8f * nh, sf, 1, 1);
        float th = 0.5f * rh, tw = ct_number_w(skat_total(p, q), th, 0);
        ui_number_signed(R + d / 2 - tw / 2, y, th, skat_total(p, q), 1, 0);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
    /* the list: a chip a deal, the declarer's (gold light: won; dimmed: lost; an empty ring: thrown in), then the
     * deals to come */
    uint8_t who[SKAT_DEALS_MAX], made[SKAT_DEALS_MAX];
    for (int i = 0; i < p->ndeals; i++) {
        who[i] = p->list[i].declarer == SKAT_NONE ? 0 : (uint8_t)(1u << p->list[i].declarer);
        made[i] = (uint8_t)p->list[i].won;
    }
    ct_deal_list(x0, pw, y, rh, N, 12, p->ndeals, who, made);
}

static void draw(void)
{
    const skpos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&A), ph = p->phase, hide = ui.viewer < 0;   /* hide: the hand-off cover */
    if (!(seen >> T.me & 1) && !ui_handing()) {   /* the viewer's first look at this deal */
        seen |= 1u << T.me;
        if (ph == SK_BID) {
            uint8_t h[CT_HAND];
            int how[CT_HAND];
            ct_deal(&A, &T, h, hand(h, how, 0));
        }
    }
    int dealing = ct_hides(&A, CT_HAND_TAG, -1, -1);   /* the deal still flying in: no bid under the deck */
    controls();
    ct_draw_seats(&T, ph < SK_DEAL ? p->turn : -1);
    draw_words();
    ct_draw_pile(&T, p->tricks);
    if (ph <= SK_DISCARD) draw_skat(live, hide);
    int mine = p->turn == T.me && !hide;
    if (mine && ph == SK_BID && !dealing) draw_bid(live);
    if (mine && (ph == SK_SKAT || ph == SK_DISCARD)) draw_panel(live);
    draw_open(hide);
    ct_draw_trick(&T, &A, p->trick, NULL, p->ntrick, p->leader);
    if (F.raised != CARD_NONE && live && ph == SK_PLAY) ct_draw_target(&T);
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, live), summary = (ph == SK_DEAL || ph == SK_OVER) && !ct_busy(&A) && p->ndeals;
    if (summary) n = 0;                         /* a null lost early: the cards left, under the table */
    if (hide) {
        if (!(ph == SK_PLAY && (p->flags & SF_OUVERT) && p->declarer == T.me)) ct_hand_backs(&T, n);
    } else
        ct_hand_draw(&T, &A, h, n, how, NULL, ct_dragged(&F));
    ct_draw(&A);
    if (!hide) ct_touch_draw(&F, &T);
    if (show_last && p->last_winner != SKAT_NONE) ct_draw_last(&T, p->last, NULL, SKAT_P, p->last_leader,
                                                               p->last_winner);
    if (summary) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    const skpos *p = &G.pos;
    if (p->phase == SK_DISCARD) { act(SA_PUT | (uint32_t)card); return; }
    if (skat_can_play(p, card)) act(SA_PLAY | (uint32_t)card);
    else { ct_lower(&F); ui_sound(SND_DENY); }
}

static void press_bid(float x, float y)
{
    const skpos *p = &G.pos;
    if (ct_on(K.pass.x, K.pass.y, K.d, x, y)) { act(SA_PASS); return; }
    if (!skat_saying(p)) {
        if (ct_on(K.ok.x, K.ok.y, K.d, x, y) || ct_on(K.held.x, K.held.y, 1.4f * K.d, x, y)) act(SA_HOLD);
        else ui_sound(SND_DENY);
        return;
    }
    for (int i = 0; i < K.npad; i++)
        if (ct_on(K.pad[i].x, K.pad[i].y, K.d, x, y)) {
            pick = BIDS[K.first + i];
            ui_sound(SND_TAP);
            return;
        }
    if (page > 0 && ct_on(K.prev.x, K.prev.y, 0.8f * K.d, x, y)) { page--; ui_sound(SND_TAP); return; }
    if (K.first + PAD < nbids && ct_on(K.next.x, K.next.y, 0.8f * K.d, x, y)) { page++; ui_sound(SND_TAP); return; }
    if (ct_on(K.ok.x, K.ok.y, K.d, x, y) && pick > p->bid) { act(SA_SAY | (uint32_t)pick); return; }
    ui_sound(SND_DENY);
}

static void toggle(int i)
{
    int f = FLAG_OF[i];
    if (!flag_enabled(i)) { ui_sound(SND_DENY); return; }
    if (gflags & f) {                           /* off: and what needs it */
        gflags &= ~f;
        if (f == SF_SCHNEIDER) gflags &= ~(SF_SCHWARZ | SF_OUVERT);
        if (f == SF_SCHWARZ) gflags &= ~SF_OUVERT;
    } else {                                    /* on: and what it needs */
        gflags |= f;
        if (gtype != SG_NULL && f == SF_OUVERT) gflags |= SF_SCHWARZ | SF_SCHNEIDER;
        if (f == SF_SCHWARZ) gflags |= SF_SCHNEIDER;
    }
    ui_sound(SND_TAP);
}

static int press_panel(float x, float y)
{
    const skpos *p = &G.pos;
    for (int t = 0; t < SG_TYPES; t++)
        if (ct_on(K.type[t].x, K.type[t].y, K.d, x, y)) {
            if (!type_enabled(t)) { ui_sound(SND_DENY); return 1; }
            gtype = t;
            if (t == SG_NULL) gflags &= SF_OUVERT;
            else if (!hand_now()) gflags = 0;
            else if (gflags & SF_OUVERT) gflags |= SF_SCHWARZ | SF_SCHNEIDER;
            ui_sound(SND_TAP);
            return 1;
        }
    for (int i = 0; i < 3; i++)
        if (ct_on(K.flag[i].x, K.flag[i].y, K.d, x, y)) { toggle(i); return 1; }
    if (ct_on(K.ok.x, K.ok.y, K.d, x, y)) {
        if (game_ready()) act(SKA_GAME(gtype, gflags));
        else ui_sound(SND_DENY);
        return 1;
    }
    for (int i = 0; i < 2; i++)                 /* the skat: take it up; a card laid away: back to the hand */
        if (cards_in(K.skat[i].x, K.skat[i].y, K.sw, T.asp, x, y)) {
            if (p->phase == SK_SKAT) act(SA_PICKUP);
            else if (i < p->nskat) act(SA_TAKE | p->skat[i]);
            else ui_sound(SND_DENY);
            return 1;
        }
    return 0;
}

static void press(float x, float y)
{
    const skpos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (ct_busy(&A)) return;                    /* the cards still moving */
    if (p->phase == SK_DEAL) { act(SA_NEXT); return; }
    if (p->phase == SK_OVER) return;
    if (ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    controls();
    if (p->phase == SK_BID) { press_bid(x, y); return; }
    if ((p->phase == SK_SKAT || p->phase == SK_DISCARD) && press_panel(x, y)) return;
    if (p->phase == SK_SKAT) { ui_sound(SND_DENY); return; }
    if (p->phase == SK_PLAY && F.raised != CARD_NONE && ct_on_target(&T, x, y)) { send(F.raised); return; }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 1);
    ct_touch_hand(&F, &T, h, n, how, NULL, x, y);   /* a tap or a drag: the release says */
}

static void motion(float x, float y) { ct_touch_move(&F, x, y); }

static void release(float x, float y)
{
    int c, r = ct_touch_up(&F, &T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP) {
        if (G.pos.phase == SK_DISCARD) {        /* a tap lays the card away */
            if (G.pos.nskat < 2) send(c);
            else ui_sound(SND_DENY);
        } else if (ct_tap_play(&F, c, skat_can_play(&G.pos, c)))
            send(c);
    }
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, T.deck, G.pos.n[player], skat_total(&G.pos, player)); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the four jacks, the club jack on top; the picker's: with one pile of deals (12) or three (36) */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const int cards[4] = {CARD(SUIT_DIAMONDS, RANK_JACK), CARD(SUIT_HEARTS, RANK_JACK),
                                 CARD(SUIT_SPADES, RANK_JACK), CARD(SUIT_CLUBS, RANK_JACK)};
    ct_thumb(DECK_FRENCH, cards, 4, size, rgba);
    if (v < 0) return;
    int cw = (int)(size * 0.2f), ch = (int)(cw * cards_aspect(DECK_FRENCH));
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    cards_paint(DECK_FRENCH, CARD_NONE, cw, ch, b);
    int piles = v == SKAT_LONG ? 3 : 1;
    for (int k = 0; k < piles; k++)
        for (int j = 0; j < 4; j++)
            art_blend(rgba, size, size, b, cw, ch, (int)(size * 0.5f + (k - piles / 2.0f) * 1.15f * cw) - j,
                      size - ch - (int)(size * 0.02f) - 2 * j);
    free(b);
}

static const game skat_game = {
    .id = "skat", .order = 311, .variants = SKAT_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 3, .view = VIEW_HIDDEN,
};
REGISTER_GAME(skat_game)
