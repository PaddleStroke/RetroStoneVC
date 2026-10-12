/*
 * Truco in RetroStone Board: 2 players, or 4 in two partnerships, passing the device round, each hand secret
 * (VIEW_HIDDEN; the rules: truco_rules.c, truco.h). The kit's Spanish pack (DECK_SPANISH) on the card table (the
 * kit's cards_table.h): the viewer's three cards at the bottom, the seats round the middle, each player's
 * cards played in a row in front of his chip, round after round (a round's losers dimmed). The calls are buttons at
 * the lower corners of the table: the envido's (a pair of cards and its worth: envido 2, real envido 3; the falta a
 * gold bolt on red) at the left; the truco (its worth on red: truco 2, retruco 3, vale cuatro 4) and the mazo (a card's
 * back) at the right; answering a call: no quiero (a cross), quiero (a tick), and the raises. Each seat shows its last
 * word, its flor (a flower), the best envido once played. The hand's worth on red by the side that may raise it. After
 * each hand, each side's points: envido, flor, the hand, the score.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards_table.h"
#include "games/envite.h"
#include "games/spanish.h"
#include "games/truco.h"
#include "shell.h"
#include "text.h"

static tgame G;
static ct_table T;
static ct_moves A = {.rest = 250};
static ct_touch F;
static int floor_n, variant;
static unsigned seen;

enum { TB_ENVIDO, TB_REAL, TB_FALTA, TB_TRUCO, TB_MAZO, TB_QUIERO, TB_NOQUIERO, TB_N };
static struct {
    vt_btn b[TB_N];
    int nb;
    float wp;                                   /* the played cards' width */
} K;

static void reset_ui(void) { ct_touch_reset(&F); }

static void layout(box b)
{
    T.deck = DECK_SPANISH;
    ct_layout(&T, b, G.pos.players ? G.pos.players : 2, ui_bottom(), 5);
}

/* where a player's card of round k lies: in a row in front of his chip */
static void place(int pl, int k, float *x, float *y, float *deg)
{
    float a = T.ang[pl] * 3.14159265f / 180, s = sinf(a), c = cosf(a);
    float step = fabsf(s) * (G.pos.players == 4 ? 0.62f : 0.74f) * K.wp + fabsf(c) * 0.46f * K.wp * T.asp;
    *x = T.tx[pl] + (k - 1) * step * s;
    *y = T.ty[pl] - (k - 1) * step * c;
    *deg = T.tdeg[pl];
}

/* the buttons for this moment (the viewer to act): the envido's at the lower left, the others at the lower right */
static void geometry(void)
{
    const tpos *p = &G.pos;
    float u = T.u, players4 = p->players == 4;
    K.wp = floorf(T.wt * (players4 ? 0.74f : 0.92f));
    K.nb = 0;
    int me = T.me, a = truco_actor(p);
    if (p->phase != TP_PLAY || a != me || ui_handing() || ui.viewer < 0) return;
    float d = floorf(fminf(0.92f * u, T.b.w / 7.4f)), y = T.hand_top - 0.7f * d, step = 1.12f * d;
    float xl = T.b.x + 0.65f * d, xr = T.b.x + T.b.w - 0.65f * d;
    int left[3], nl = 0, right[4], nr = 0;
    if (truco_legal(p, TA_ENVIDO)) left[nl++] = TB_ENVIDO;
    if (truco_legal(p, TA_REAL)) left[nl++] = TB_REAL;
    if (truco_legal(p, TA_FALTA)) left[nl++] = TB_FALTA;
    if (truco_legal(p, TA_QUIERO)) right[nr++] = TB_QUIERO;
    if (truco_legal(p, TA_NOQUIERO)) right[nr++] = TB_NOQUIERO;
    if (truco_legal(p, TA_TRUCO)) right[nr++] = TB_TRUCO;
    if (truco_legal(p, TA_MAZO)) right[nr++] = TB_MAZO;
    /* in columns up the sides from the bottom, below the side seats (four players); a second column inwards when
     * one would reach them */
    float top = p->players == 4 ? T.sy[(me + 1) % 4] + T.tok + 0.5f * d : T.b.y + 0.6f * d;
    int per = (int)fmaxf(1, floorf((y - top) / step) + 1);
    for (int i = 0; i < nl; i++)
        K.b[K.nb++] = (vt_btn){xl + (i / per) * step, y - (i % per) * step, d, left[i], 1};
    for (int i = 0; i < nr; i++)
        K.b[K.nb++] = (vt_btn){xr - (i / per) * step, y - (i % per) * step, d, right[i], 1};
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    variant = v;
    truco_new(&G, ui.players, v & 1 ? 30 : 15, v >= 2, (uint32_t)ui_random(1 << 30));
    T.deck = DECK_SPANISH;
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    int w = truco_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static unsigned winners(void)
{
    int w = truco_winner(&G.pos);
    if (w < 0) return 0;
    return G.pos.players == 4 ? 1u << w | 1u << (w + 2) : 1u << w;
}

static int turn(void) { return truco_actor(&G.pos); }

static int busy(void) { return ct_busy(&A); }

/* the viewer's hand as shown: the strongest first */
static int hand(uint8_t *h)
{
    const tpos *p = &G.pos;
    int n = p->n[T.me];
    memcpy(h, p->hand[T.me], (size_t)n);
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && truco_power(h[j]) > truco_power(h[j - 1]); j--) {
            uint8_t t = h[j];
            h[j] = h[j - 1];
            h[j - 1] = t;
        }
    return n;
}

static void act(int a)
{
    tpos before = G.pos;
    uint8_t h[3];
    int n = hand(h), who = truco_actor(&before);
    if (!truco_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    const tpos *p = &G.pos;
    int k = TA_KIND(a);
    if (k == TA_PLAY) {                         /* the card from the hand (or the finger) to its place */
        cards_spot s[3];
        ct_fan(&T, n, s);
        int i = 0;
        while (i < n && h[i] != TA_ARG(a)) i++;
        float x0 = i < n ? s[i].x : T.hx, y0 = i < n ? s[i].y - 0.14f * T.wh : T.hy, d0 = i < n ? s[i].deg : 0, x, y, deg;
        if (F.on) x0 = F.x, y0 = F.y, d0 = 0;
        geometry();
        place(who, before.round, &x, &y, &deg);
        ct_rest(&A, ct_slide(&A, T.deck, TA_ARG(a), x0, y0, T.wh, d0, 1, x, y, K.wp, deg, 1, CT_SLIDE_MS, 0, CT_TRICK,
                             who, before.round, SND_PLACE),
                p->round != before.round || p->phase != TP_PLAY ? 900 : 250);
    } else if (k == TA_NEXT)
        seen = 0;
    else
        ui_sound(k == TA_MAZO || k == TA_NOQUIERO ? SND_TAP : SND_PLACE);
    if (p->phase != TP_PLAY && before.phase == TP_PLAY) ct_until(&A, ui_ms() + 600);
    reset_ui();
    if (truco_actor(p) != who || p->phase != TP_PLAY) floor_n = G.n;
    if (p->phase == TP_PLAY) ui_handoff(truco_actor(p));
}

static int undo(void)
{
    if (F.raised != CARD_NONE) { reset_ui(); return 1; }
    if (G.n <= floor_n || !truco_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(truco_actor(&G.pos));
    return 1;
}

static int script(const char *s)
{
    int ok = truco_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(truco_actor(&G.pos));
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void side_chips(int s, float cx, float cy, float d)
{
    const int ts = (int)(d + 0.5f);
    if (G.pos.players == 2) { ui_put_c(ct_token(s, ts), cx, cy, d, d); return; }
    ui_put_c(ct_token(s + 2, ts), cx + 0.22f * d, cy, d, d);
    ui_put_c(ct_token(s, ts), cx - 0.22f * d, cy, d, d);
}

/* a truco's worth on red lacquer */
static void truco_sign(float cx, float cy, float d, int worth)
{
    vt_red(cx, cy, d);
    ui_number(cx, cy, 0.46f * d, worth, 1);
}

/* an envido's call: a pair of cards and its worth (falta: the bolt) */
static void envido_sign(int kind, float cx, float cy, float d, int button)
{
    if (kind == TE_FALTA) { vt_bolt(cx, cy, d); return; }
    if (button) ct_button(cx, cy, d, kind == TE_REAL, -1, 0);
    else ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    vt_icon(VT_PAIR, cx, cy - 0.17f * d, 0.5f * d, 246, 238, 220, 255);
    ui_number(cx, cy + 0.2f * d, 0.3f * d, kind == TE_REAL ? 3 : 2, 1);
}

/* the mazo: a card's back on a button */
static void mazo_sign(float cx, float cy, float d, int button)
{
    if (button) ct_button(cx, cy, d, 0, -1, 0);
    else ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    cards_draw(T.deck, CARD_NONE, cx, cy, 0.4f * d, -10, CD_DOWN | CD_FLAT);
}

static void draw_words(void)
{
    const tpos *p = &G.pos;
    float d = 0.95f * T.tok;
    for (int q = 0; q < p->players; q++) {
        float x, y, x1, y1;
        ct_seat_spot(&T, q, 0, &x, &y);
        ct_seat_spot(&T, q, 1, &x1, &y1);
        int w = p->word[q];
        switch (w) {
        case TW_TRUCO: truco_sign(x, y, d, p->wordn[q]); break;
        case TW_ENVIDO: envido_sign(TE_ENVIDO, x, y, d, 0); break;
        case TW_REAL: envido_sign(TE_REAL, x, y, d, 0); break;
        case TW_FALTA: envido_sign(TE_FALTA, x, y, d, 0); break;
        case TW_QUIERO: vt_quiero(x, y, d); break;
        case TW_NOQUIERO: vt_noquiero(x, y, d); break;
        case TW_MAZO: mazo_sign(x, y, d, 0); break;
        default: break;
        }
        int k = w != TW_NONE && w != TW_FLOR;
        float fx = k ? x1 : x, fy = k ? y1 : y;
        if (p->flor_mask >> q & 1) {           /* a flor: a gold flower */
            ui_mark_gold(fx, fy, 1.5f * d);
            ct_pic(CT_DISC_DARK, fx, fy, d, 0, 255, 255, 255, 255);
            vt_icon(VT_FLOWER, fx, fy, 0.7f * d, 246, 206, 90, 255);
            k = 1, fx = x1, fy = y1;
        }
        if (p->env_winner == q) {               /* the best envido */
            float ex, ey;
            ct_seat_spot(&T, q, k ? 2 : 0, &ex, &ey);
            if (!k) ex = x, ey = y;
            ui_mark_gold(ex, ey, 1.5f * d);
            ct_badge(ex, ey, d, p->env_value[q]);
        }
    }
}

static void draw_played(void)
{
    const tpos *p = &G.pos;
    for (int k = 0; k <= p->round && k < 3; k++)
        for (int q = 0; q < p->players; q++) {
            int c = p->played[k][q];
            if (c == CARD_NONE || ct_hides(&A, CT_TRICK, q, k)) continue;
            float x, y, deg;
            place(q, k, &x, &y, &deg);
            int r = p->result[k], how = 0;
            if (r <= 1 && (q & 1) != r) how = CD_DIM;   /* the round's losers dimmed */
            cards_draw(T.deck, c, x, y, K.wp, deg, how);
        }
}

/* the hand's worth once a truco is accepted: on red, by the side that may raise it */
static void draw_worth(void)
{
    const tpos *p = &G.pos;
    if (p->level < 2 || p->phase != TP_PLAY) return;
    float d = 0.95f * T.tok, x = T.b.x + 0.75f * T.u, y = T.b.y + 0.75f * T.u;
    truco_sign(x, y, 1.2f * d, p->level);
    side_chips(p->owner, x + 1.25f * d, y, 0.75f * d);
}

static void draw_pad(int live)
{
    for (int i = 0; i < K.nb; i++) {
        const vt_btn *b = &K.b[i];
        switch (b->id) {
        case TB_ENVIDO: envido_sign(TE_ENVIDO, b->x, b->y, b->d, 1); break;
        case TB_REAL: envido_sign(TE_REAL, b->x, b->y, b->d, 1); break;
        case TB_FALTA: {
            vt_bolt(b->x, b->y, b->d);
            int f = truco_falta(&G.pos, T.me & 1);
            ct_badge(b->x + 0.36f * b->d, b->y + 0.36f * b->d, 0.5f * b->d, f);
            break;
        }
        case TB_TRUCO:
            if (live) ui_mark_gold(b->x, b->y, 1.7f * b->d);
            truco_sign(b->x, b->y, b->d, G.pos.call ? G.pos.call + 1 : G.pos.level + 1);
            break;
        case TB_MAZO: mazo_sign(b->x, b->y, b->d, 1); break;
        case TB_QUIERO:
            ct_button(b->x, b->y, b->d, 0, -1, 0);
            ct_pic(CT_TICK, b->x, b->y, 0.66f * b->d, 0, 160, 236, 170, 255);
            break;
        case TB_NOQUIERO:
            ct_button(b->x, b->y, b->d, 0, -1, 0);
            vt_icon(VT_CROSS, b->x, b->y, 0.7f * b->d, 240, 120, 100, 255);
            break;
        }
    }
}

/* the hand's end: a row a side: its chips, the envido's points (its best envido), the flor's, the hand's (its worth),
 * the score */
static void draw_summary(void)
{
    const tpos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, 3, &x0, &pw, &y, &rh);
    float d = fminf(0.6f * rh, 0.72f * u);
    float cols[4] = {x0 + 0.33f * pw, x0 + 0.53f * pw, x0 + 0.73f * pw, x0 + pw - 0.7f * u};
    /* the heads: envido, flor, the hand */
    envido_sign(TE_ENVIDO, cols[0], y, d, 0);
    if (p->flor) vt_badge(VT_FLOWER, cols[1], y, d);
    truco_sign(cols[2], y, d, p->hand_pts ? p->hand_pts : p->level);
    y += rh;
    ct_rule(x0 + 0.3f * u, y - rh / 2, pw - 0.6f * u);
    for (int s = 0; s < 2; s++, y += rh) {
        float x = x0 + 0.6f * u + (p->players == 4 ? 0.2f * d : 0);
        if (p->score[s] > p->score[!s]) ui_mark_gold(x, y, 1.9f * d);
        side_chips(s, x, y, d);
        if (p->gained[s][0]) {
            ct_plus(cols[0], y, 0.36f * rh, p->gained[s][0]);
            if (p->env_winner != TR_NONE && (p->env_winner & 1) == s)
                ct_badge(cols[0] - 0.95f * d, y, 0.62f * d, p->env_value[p->env_winner]);
        }
        if (p->gained[s][1]) ct_plus(cols[1], y, 0.36f * rh, p->gained[s][1]);
        if (p->gained[s][2]) ct_plus(cols[2], y, 0.36f * rh, p->gained[s][2]);
        if (p->mazo && ((p->mazo - 1) & 1) == s) mazo_sign(cols[2] - 0.95f * d, y, 0.62f * d, 0);
        ui_number(cols[3] - ct_number_w(p->score[s], 0.46f * rh, 0) / 2 + 0.3f * u, y, 0.46f * rh, p->score[s], 1);
        if (s == 0) ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const tpos *p = &G.pos;
    int hide = ui.viewer < 0, live = !ui_handing() && !ct_busy(&A);
    if (T.deck != DECK_SPANISH || T.players != p->players || T.me != (ui_bottom() < 0 ? T.me : ui_bottom()))
        layout(T.b);
    uint8_t h[3];
    int n = hand(h);
    if (!(seen >> T.me & 1) && !ui_handing() && p->phase == TP_PLAY && p->round == 0 &&
        p->played[0][T.me] == CARD_NONE) {
        seen |= 1u << T.me;
        ct_deal(&A, &T, h, n);
    }
    geometry();
    ct_draw_seats(&T, p->phase == TP_PLAY ? truco_actor(p) : -1);
    draw_words();
    draw_worth();
    draw_played();
    int mine = p->phase == TP_PLAY && truco_actor(p) == T.me && !p->call && p->env != TE_CALLING;
    if (F.raised != CARD_NONE && live && mine) {
        float x, y, deg;
        place(T.me, p->round, &x, &y, &deg);
        ui_mark_gold(x, y, 1.6f * K.wp);
        cards_place(T.deck, x, y, K.wp);
    }
    int how[3];
    for (int i = 0; i < n; i++) how[i] = ct_is_raised(&F, i) ? CD_RAISED : live && mine ? CD_GLOW : 0;
    if (hide) ct_hand_backs(&T, n);
    else ct_hand_draw(&T, &A, h, n, how, NULL, ct_dragged(&F));
    ct_draw(&A);
    if (!hide) {
        ct_touch_draw(&F, &T);
        draw_pad(live);
    }
    if (p->phase != TP_PLAY && !ct_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    const tpos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (ct_busy(&A)) return;
    if (p->phase == TP_HAND) { act(TA_NEXT); return; }
    if (p->phase != TP_PLAY) return;
    if (truco_actor(p) != T.me) { ui_sound(SND_DENY); return; }
    geometry();
    switch (vt_hit(K.b, K.nb, x, y)) {
    case TB_ENVIDO: act(TA_ENVIDO); return;
    case TB_REAL: act(TA_REAL); return;
    case TB_FALTA: act(TA_FALTA); return;
    case TB_TRUCO: act(TA_TRUCO); return;
    case TB_MAZO: act(TA_MAZO); return;
    case TB_QUIERO: act(TA_QUIERO); return;
    case TB_NOQUIERO: act(TA_NOQUIERO); return;
    default: break;
    }
    int can = !p->call && p->env != TE_CALLING;
    if (F.raised != CARD_NONE && can) {
        float px, py, deg;
        place(T.me, p->round, &px, &py, &deg);
        if (cards_in(px, py, 1.2f * K.wp, T.asp, x, y)) { act(TA_PLAY | F.raised); return; }
    }
    uint8_t h[3];
    int how[3], n = hand(h);
    for (int i = 0; i < n; i++) how[i] = ct_is_raised(&F, i) ? CD_RAISED : 0;
    if (can) ct_touch_hand(&F, &T, h, n, how, NULL, x, y);
    else if (F.raised != CARD_NONE) ct_lower(&F);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y) { ct_touch_move(&F, x, y); }

static void release(float x, float y)
{
    int c, r = ct_touch_up(&F, &T, x, y, &c);
    if (r == CT_T_SEND) act(TA_PLAY | c);
    else if (r == CT_T_TAP && ct_tap_play(&F, c, 1)) act(TA_PLAY | c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    float h = 0.46f * ui.unit;
    int s = G.pos.score[player & 1];
    ui_number(b.x + ct_number_w(s, h, 0) / 2, b.y + b.h / 2, h, s, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* digits painted into a picture (the picker's cards: the game's length), centred, hgt tall */
static void paint_digits(uint8_t *rgba, int size, int v, float cx, float cy, float hgt)
{
    static const uint8_t cream[3] = {246, 238, 220};
    char d[2] = {(char)('0' + v / 10), (char)('0' + v % 10)};
    int bw = (int)ceilf(0.9f * hgt) + 2, bh = (int)ceilf(1.3f * hgt) + 2;
    uint8_t *b = malloc((size_t)bw * bh * 4);
    if (!b) return;
    for (int i = 0; i < 2; i++) {
        memset(b, 0, (size_t)bw * bh * 4);
        if (text_glyph(d[i], 1.2f * hgt, 1, cream, b, bw, bh))
            art_blend(rgba, size, size, b, bw, bh, (int)(cx + (i - 0.5f) * 0.56f * hgt - bw / 2.0f + 0.5f),
                      (int)(cy - bh / 2.0f + 0.5f));
    }
    free(b);
}

/* the menu's card: the two anchos and the 7 of espadas; the picker's: the game's length on red, a flower for the flor */
static void thumb(int v, int size, uint8_t *rgba)
{
    int deck = DECK_SPANISH;
    float w = 0.34f * size;
    memset(rgba, 0, (size_t)size * size * 4);
    float lift = v < 0 ? 0 : 0.07f * size;
    cards_paint_turned(rgba, size, deck, CARD(ES_ESPADAS, 7), 0.3f * size, 0.5f * size - lift, w, -14);
    cards_paint_turned(rgba, size, deck, CARD(ES_BASTOS, 1), 0.5f * size, 0.46f * size - lift, w, 0);
    cards_paint_turned(rgba, size, deck, CARD(ES_ESPADAS, 1), 0.7f * size, 0.5f * size - lift, w, 14);
    if (v < 0) return;
    int ds = (int)(0.3f * size);
    uint8_t *b = calloc((size_t)ds * ds, 4);
    if (!b) return;
    art_disc(ds, VT_RED, 1, 0, b);
    float bx = (v >= 2 ? 0.36f : 0.5f) * size, by = 0.8f * size;
    art_blend(rgba, size, size, b, ds, ds, (int)(bx - ds / 2.0f), (int)(by - ds / 2.0f));
    paint_digits(rgba, size, v & 1 ? 30 : 15, bx, by, 0.13f * size);
    if (v >= 2) {
        memset(b, 0, (size_t)ds * ds * 4);
        vt_paint(VT_FLOWER, ds, b);
        for (int i = 0; i < ds * ds; i++) b[4 * i] = 246, b[4 * i + 1] = 206, b[4 * i + 2] = 90;
        art_blend(rgba, size, size, b, ds, ds, (int)(0.64f * size - ds / 2.0f), (int)(by - ds / 2.0f));
    }
    free(b);
}

static const game truco_game = {
    .id = "truco", .order = 326, .variants = 4,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(truco_game)
