/*
 * Mus in RetroStone Board: four players in two partnerships passing the device round, each hand secret (VIEW_HIDDEN;
 * the rules: mus_rules.c, mus.h). The Spanish pack (spanish.h, the kit's cards_spanish.c) on the card table (the kit's
 * cards_table.h): the viewer's four cards at the bottom, the seats round the middle (the partner across), each with its
 * last word: mus (two arrows, one each way) or the cut (struck through), the cards it took, a pass, a bet, a raise (a
 * plus), the ordago (a gold bolt on red), quiero (a tick), no quiero (a cross); in Pares and Juego whether it has them.
 * Along the top of the middle the four lances (grande an arrow up, chica an arrow down, pares a pair of cards, juego
 * 31, punto 30), the one being bet in gold light, each played one with what came of it. In the middle the stock and the
 * cards laid away while changing cards, then the bet on the table. Above the viewer's chip, his choices: mus or cut;
 * the cards to lay away (tap them) and the tick; pass, the bet (its amount on the minus and plus), the ordago; facing a
 * bet: no quiero, quiero, the raise, the ordago. After each hand the count: every hand shown, each lance with its
 * winners and points, the scores.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards_table.h"
#include "games/envite.h"
#include "games/mus.h"
#include "games/spanish.h"
#include "shell.h"

static mgame G;
static ct_table T;
static ct_moves A = {.rest = 250};
static int floor_n, amount, sel;                /* the bet's amount on the stepper; the cards chosen to lay away */
static unsigned seen;

enum { B_MUS, B_CUT, B_OK, B_PASS, B_MINUS, B_BET, B_PLUS, B_ORDAGO, B_QUIERO, B_NOQUIERO, B_N };

static struct {
    float strip_y, cell, step;                  /* the lances' row: its centre, a cell's size and the step */
    float mid_y;                                /* the middle: the piles, the bet */
    float pw;                                   /* the piles' cards' width */
    vt_btn b[B_N];
    int nb;
} K;

static void reset_ui(void)
{
    sel = 0;
    amount = 2;
}

static int facing(void) { return G.pos.bet > 0 || G.pos.ordago; }

static int min_amount(void) { return facing() ? 1 : 2; }

/* where everything goes in the middle, and the viewer's buttons for this moment */
static void geometry(void)
{
    const mpos *p = &G.pos;
    float u = T.u, tok = T.tok;
    int me = T.me, left = (me + 1) % 4, right = (me + 3) % 4, across = (me + 2) % 4;
    float x0 = T.sx[left] + 0.9f * tok, x1 = T.sx[right] - 0.9f * tok, w = x1 - x0;
    float top = T.sy[across] + 0.9f * tok, bot = T.sy[me] - 0.85f * tok;
    K.cell = floorf(fminf(0.85f * u, w / 5.2f));
    K.step = fminf(1.35f * K.cell, w / 4.2f);
    K.strip_y = top + 0.55f * K.cell;
    float d = floorf(fminf(0.95f * u, w / 5.6f)), rows = 1;
    int mine = p->turn == me && !ui_handing() && ui.viewer >= 0;
    K.nb = 0;
    if (mine && p->phase == MP_MUS) {
        K.b[K.nb++] = (vt_btn){0, 0, 0, B_MUS, 1};
        K.b[K.nb++] = (vt_btn){0, 0, 0, B_CUT, 1};
        vt_row(K.b, 2, T.cx, bot - 0.65f * d, d, 1.6f);
    } else if (mine && p->phase == MP_DISCARD) {
        K.b[K.nb++] = (vt_btn){0, 0, 0, B_OK, sel != 0};
        vt_row(K.b, 1, T.cx, bot - 0.65f * d, d, 1);
    } else if (mine && p->phase == MP_BET) {
        if (!facing()) {
            vt_btn r[5] = {{0, 0, 0, B_PASS, 1}, {0, 0, 0, B_MINUS, 1}, {0, 0, 0, B_BET, 1}, {0, 0, 0, B_PLUS, 1},
                           {0, 0, 0, B_ORDAGO, 1}};
            vt_row(r, 5, T.cx, bot - 0.65f * d, d, 1.18f);
            memcpy(K.b, r, sizeof r);
            K.nb = 5;
        } else {
            vt_btn r1[2] = {{0, 0, 0, B_NOQUIERO, 1}, {0, 0, 0, B_QUIERO, 1}};
            vt_row(r1, 2, T.cx, bot - 0.65f * d, d, 1.6f);
            memcpy(K.b, r1, sizeof r1);
            K.nb = 2;
            if (!p->ordago) {
                vt_btn r2[4] = {{0, 0, 0, B_MINUS, 1}, {0, 0, 0, B_BET, 1}, {0, 0, 0, B_PLUS, 1},
                                {0, 0, 0, B_ORDAGO, 1}};
                vt_row(r2, 4, T.cx, bot - 1.95f * d, d, 1.18f);
                memcpy(K.b + 2, r2, sizeof r2);
                K.nb = 6;
                rows = 2;
            }
        }
    }
    for (int i = 0; i < K.nb; i++)              /* the stepper's buttons smaller */
        if (K.b[i].id == B_MINUS || K.b[i].id == B_PLUS) K.b[i].d = floorf(0.72f * d);
    float pad_top = K.nb ? bot - (rows == 2 ? 2.6f : 1.3f) * d : bot;
    K.mid_y = (K.strip_y + 1.2f * K.cell + pad_top) / 2;
    K.pw = floorf(fminf(0.62f * T.wt, (pad_top - K.strip_y - 1.3f * K.cell) / T.asp * 0.8f));
    if (amount < min_amount()) amount = 2;
}

static void layout(box b)
{
    T.deck = es_deck();
    ct_layout(&T, b, MUS_P, ui_bottom(), 6);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    mus_new(&G, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    int w = mus_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static unsigned winners(void)
{
    int w = mus_winner(&G.pos);
    return w < 0 ? 0 : 1u << w | 1u << (w + 2);
}

static int turn(void) { return G.pos.turn; }

static int busy(void) { return ct_busy(&A); }

/* the piles in the middle while changing cards: the stock (left), the cards laid away (right) */
static void piles_at(float *sx, float *lx)
{
    *sx = T.cx - 0.7f * K.pw;
    *lx = T.cx + 0.7f * K.pw;
}

static void act(int a)
{
    mpos before = G.pos;
    if (!mus_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    const mpos *p = &G.pos;
    int k = MA_KIND(a);
    if (k == MA_DISCARD) {                      /* the cards laid away fly to their pile, new ones come in */
        cards_spot s[4], s1[4];
        ct_fan(&T, 4, s);
        ct_fan(&T, 4, s1);
        float sx, lx;
        geometry();
        piles_at(&sx, &lx);
        int j = 0;
        for (int i = 0; i < 4; i++)
            if (MA_ARG(a) >> i & 1)
                ct_slide(&A, T.deck, before.hand[before.turn][i], s[i].x, s[i].y - 0.14f * T.wh, T.wh, s[i].deg, 1,
                         lx, K.mid_y, K.pw, 0, 0, CT_SLIDE_MS, 40 * j++, CT_NONE, 0, -1, SND_MOVE);
        j = 0;
        for (int i = 0; i < 4; i++) {
            int c = p->hand[before.turn][i], old = 0;
            for (int q = 0; q < 4; q++) old |= before.hand[before.turn][q] == c && !(MA_ARG(a) >> q & 1);
            if (!old)
                ct_slide(&A, T.deck, c, sx, K.mid_y, K.pw, 0, 0, s1[i].x, s1[i].y, T.wh, s1[i].deg, 1, CT_DEAL_MS + 60,
                         CT_SLIDE_MS + 60 + 90 * j++, CT_HAND_TAG, i, -1, SND_MOVE);
        }
    } else if (k == MA_MUS || k == MA_PASS)
        ui_sound(SND_TAP);
    else if (k == MA_NEXT)
        seen = 0;
    else
        ui_sound(SND_PLACE);
    if (p->phase == MP_OVER || (p->phase == MP_HAND && before.phase != MP_HAND)) ct_until(&A, ui_ms() + 500);
    reset_ui();
    if (p->turn != before.turn || p->phase >= MP_HAND) floor_n = G.n;
    if (p->phase < MP_HAND) ui_handoff(p->turn);
}

static int undo(void)
{
    if (sel || amount != 2) { reset_ui(); return 1; }
    if (G.n <= floor_n || !mus_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = mus_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* a side's two chips, overlapping, d wide each, centred on (cx, cy) */
static void side_chips(int s, float cx, float cy, float d)
{
    const int ts = (int)(d + 0.5f);
    ui_put_c(ct_token(s + 2, ts), cx + 0.22f * d, cy, d, d);
    ui_put_c(ct_token(s, ts), cx - 0.22f * d, cy, d, d);
}

/* a lance's sign on a dark disc: grande, chica, pares, juego (31) or punto (30) */
static void lance_sign(int l, int punto, float cx, float cy, float d, Uint8 a)
{
    ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, a);
    if (l == ML_JUEGO) {
        ui_number(cx, cy, 0.4f * d, punto ? 30 : 31, 1);
        return;
    }
    vt_icon(l == ML_GRANDE ? VT_UP : l == ML_CHICA ? VT_DOWN : VT_PAIR, cx, cy, 0.7f * d, 246, 238, 220, a);
}

/* a lance's outcome, d wide: en paso, the bet accepted, refused (its points, the side that took them), one side only,
 * nobody, the ordago */
static void outcome(const mpos *p, int l, float cx, float cy, float d)
{
    switch (p->ls[l]) {
    case LS_PASO: ct_pass_badge(cx, cy, d); break;
    case LS_SEEN:
        ct_badge(cx, cy, d, p->lamount[l]);
        ct_pic(CT_TICK, cx + 0.42f * d, cy + 0.36f * d, 0.5f * d, 0, 140, 220, 150, 255);
        break;
    case LS_DEJE:
        side_chips(p->lside[l], cx - 0.32f * d, cy, 0.62f * d);
        ui_number(cx + 0.42f * d, cy, 0.46f * d, p->lamount[l], 1);
        break;
    case LS_ONE: side_chips(p->lside[l], cx, cy, 0.7f * d); break;
    case LS_NONE: ct_pic(CT_SLASH, cx, cy, 0.7f * d, 0, 246, 238, 220, 160); break;
    case LS_ORDAGO: vt_bolt(cx, cy, d); break;
    }
}

static void draw_strip(void)
{
    const mpos *p = &G.pos;
    for (int l = 0; l < ML_N; l++) {
        float x = T.cx + (l - 1.5f) * K.step, y = K.strip_y;
        int now = p->phase == MP_BET && p->lance == l, done = p->ls[l] != LS_WAIT;
        if (now) ui_mark_gold(x, y, 1.9f * K.cell);
        lance_sign(l, l == ML_JUEGO && (done || now || p->phase >= MP_HAND) && p->punto, x, y, K.cell,
                   now || done || p->phase == MP_BET ? 255 : 150);
        if (done) outcome(p, l, x, y + 0.95f * K.cell, 0.66f * K.cell);
    }
}

/* each seat's word, and in Pares and Juego whether it has them */
static void draw_words(void)
{
    const mpos *p = &G.pos;
    float d = 0.95f * T.tok;
    for (int q = 0; q < MUS_P; q++) {
        float x, y, x1, y1;
        ct_seat_spot(&T, q, 0, &x, &y);
        ct_seat_spot(&T, q, 1, &x1, &y1);
        if (p->phase >= MP_HAND) continue;
        switch (p->word[q]) {
        case MW_MUS: vt_badge(VT_CYCLE, x, y, d); break;
        case MW_CUT:
            vt_badge(VT_CYCLE, x, y, d);
            ct_pic(CT_SLASH, x, y, 0.95f * d, 0, 236, 110, 90, 255);
            break;
        case MW_PASS: ct_pass_badge(x, y, d); break;
        case MW_BET: ct_badge(x, y, d, p->wordn[q]); break;
        case MW_RAISE: vt_raise(x, y, d, p->wordn[q]); break;
        case MW_ORDAGO: vt_bolt(x, y, d); break;
        case MW_QUIERO: vt_quiero(x, y, d); break;
        case MW_NOQUIERO: vt_noquiero(x, y, d); break;
        default:
            if ((p->phase == MP_MUS || p->phase == MP_DISCARD) && p->took[q]) {
                ct_badge(x, y, d, p->took[q]);  /* the cards taken in the last change */
                vt_icon(VT_CARD, x + 0.4f * d, y - 0.36f * d, 0.55f * d, 246, 238, 220, 255);
            }
        }
        if (p->phase == MP_BET && p->lance >= ML_PARES && !(p->lance == ML_JUEGO && p->punto)) {
            int has = mus_has(p, p->lance, q);
            float dx = p->word[q] == MW_NONE ? x : x1, dy = p->word[q] == MW_NONE ? y : y1;
            lance_sign(p->lance, 0, dx, dy, 0.85f * d, has ? 255 : 110);
            if (!has) ct_pic(CT_SLASH, dx, dy, 0.85f * d, 0, 236, 110, 90, 255);
        }
    }
}

/* the middle: the piles while changing cards; the bet on the table */
static void draw_middle(void)
{
    const mpos *p = &G.pos;
    if (p->phase == MP_MUS || p->phase == MP_DISCARD) {
        float sx, lx;
        piles_at(&sx, &lx);
        int moving = 0;
        for (int i = 0; i < A.n; i++)          /* the cards on their way out still show in the stock */
            if (cards_slide_busy(&A.m[i].s) && A.m[i].tag == CT_HAND_TAG &&
                ui_ms() < A.m[i].s.t0 + (Uint32)A.m[i].s.delay)
                moving++;
        cards_pile(T.deck, sx, K.mid_y, K.pw, p->nstock + moving, CARD_NONE, 0);
        if (p->nlaid) cards_pile(T.deck, lx, K.mid_y, K.pw, p->nlaid, CARD_NONE, 0);
        return;
    }
    if (p->phase != MP_BET || !facing()) return;
    float d = 0.8f * K.cell, x = T.cx + (p->lance - 1.5f) * K.step, y = K.strip_y + 1.0f * K.cell;
    ui_mark_gold(x, y, 1.7f * d);                /* the bet on the table, under its lance */
    if (p->ordago) vt_bolt(x, y, d);
    else ct_badge(x, y, d, p->bet);
}

static void draw_pad(int live)
{
    const mpos *p = &G.pos;
    for (int i = 0; i < K.nb; i++) {
        const vt_btn *b = &K.b[i];
        switch (b->id) {
        case B_MUS:
            ct_button(b->x, b->y, b->d, 0, -1, 0);
            vt_icon(VT_CYCLE, b->x, b->y, 0.7f * b->d, 255, 255, 255, 255);
            break;
        case B_CUT:
            ct_button(b->x, b->y, b->d, 0, -1, 0);
            vt_icon(VT_CYCLE, b->x, b->y, 0.7f * b->d, 255, 255, 255, 140);
            ct_pic(CT_SLASH, b->x, b->y, 0.85f * b->d, 0, 236, 110, 90, 255);
            break;
        case B_OK:
            if (b->on && live) ui_mark_gold(b->x, b->y, 1.9f * b->d);
            ct_button(b->x, b->y, b->d, b->on, CT_TICK, 0);
            break;
        case B_PASS: ct_pass(b->x, b->y, b->d, 0); break;
        case B_MINUS: vt_step(b->x, b->y, b->d, 0, amount > min_amount()); break;
        case B_PLUS: vt_step(b->x, b->y, b->d, 1, p->bet + amount < MUS_MAX_BET); break;
        case B_BET: vt_bet_button(b->x, b->y, b->d, amount, facing(), live); break;
        case B_ORDAGO: vt_bolt(b->x, b->y, b->d); break;
        case B_QUIERO:
            ct_button(b->x, b->y, b->d, 0, -1, 0);
            ct_pic(CT_TICK, b->x, b->y, 0.66f * b->d, 0, 160, 236, 170, 255);
            break;
        case B_NOQUIERO:
            ct_button(b->x, b->y, b->d, 0, -1, 0);
            vt_icon(VT_CROSS, b->x, b->y, 0.7f * b->d, 240, 120, 100, 255);
            break;
        }
    }
}

/* the count: every hand (from the mano), each lance's outcome, winners and points, the scores */
static void draw_summary(void)
{
    const mpos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, 9, &x0, &pw, &y, &rh);
    float d = fminf(0.62f * rh, 0.7f * u), cw = floorf(fminf(0.8f * rh / T.asp, 0.9f * u));
    const int ts = (int)d;
    for (int k = 0; k < MUS_P; k++, y += rh) {  /* the hands */
        int q = (p->mano + k) % MUS_P, key, pares = mus_pares(p->hand[q], &key), pts = mus_points(p->hand[q]);
        float x = x0 + 0.6f * u;
        ui_put_c(ct_token(q, ts), x, y, d, d);
        for (int i = 0; i < 4; i++) cards_draw(T.deck, p->hand[q][i], x + 0.75f * d + cw * (0.5f + 1.06f * i), y, cw,
                                               0, CD_FLAT);
        float xr = x0 + pw - 0.6f * u;
        if (pares) {
            vt_badge(VT_PAIR, xr - 1.25f * d, y, 0.8f * d);
            ui_number(xr - 1.25f * d + 0.48f * d, y + 0.3f * d, 0.3f * d, pares, 1);
        }
        if (pts >= 31) ui_mark_gold(xr, y, 1.4f * d);
        ui_number(xr, y, 0.42f * rh, pts, 1);
    }
    ct_rule(x0 + 0.3f * u, y - rh / 2, pw - 0.6f * u);
    for (int l = 0; l < ML_N; l++, y += rh) {   /* the lances */
        float x = x0 + 0.6f * u;
        lance_sign(l, p->punto, x, y, d, 255);
        if (p->ls[l] != LS_WAIT && p->ls[l] != LS_ONE) outcome(p, l, x + 1.35f * d, y, 0.8f * d);
        int s = p->lwin[l];
        if (s != MUS_NONE) {
            side_chips(s, x0 + pw * 0.62f, y, 0.8f * d);
            if (p->lpts[l]) ct_plus(x0 + pw - 0.6f * u - 0.5f * d, y, 0.4f * rh, p->lpts[l]);
        }
    }
    ct_rule(x0 + 0.3f * u, y - rh / 2, pw - 0.6f * u);
    for (int s = 0; s < 2; s++) {               /* the scores: the leader in gold light */
        float x = x0 + pw * (s ? 0.62f : 0.18f);
        int lead = p->score[s] > p->score[!s];
        if (lead) ui_mark_gold(x, y, 1.9f * d);
        side_chips(s, x, y, 0.85f * d);
        ui_number(x + 1.25f * d + ct_number_w(p->score[s], 0.46f * rh, 0) / 2, y, 0.46f * rh, p->score[s], 1);
    }
}

static void draw(void)
{
    const mpos *p = &G.pos;
    int hide = ui.viewer < 0, live = !ui_handing() && !ct_busy(&A);
    if (T.deck != es_deck()) layout(T.b);
    if (!(seen >> T.me & 1) && !ui_handing() && p->phase == MP_MUS && !p->rounds) {
        seen |= 1u << T.me;                     /* the deal, as each player first sees it */
        ct_deal(&A, &T, p->hand[T.me], 4);
    }
    geometry();
    ct_draw_seats(&T, p->phase < MP_HAND ? p->turn : -1);
    draw_words();
    draw_strip();
    draw_middle();
    int how[4];
    for (int i = 0; i < 4; i++) how[i] = sel >> i & 1 ? CD_RAISED : p->phase == MP_DISCARD && p->turn == T.me && live
                                                                      ? CD_GLOW : 0;
    if (hide) ct_hand_backs(&T, 4);
    else ct_hand_draw(&T, &A, p->hand[T.me], 4, how, NULL, CARD_NONE);
    ct_draw(&A);
    if (!hide && p->phase < MP_HAND) draw_pad(live);
    if (p->phase >= MP_HAND && !ct_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    const mpos *p = &G.pos;
    if (ct_busy(&A)) return;
    if (p->phase == MP_HAND) { act(MA_NEXT); return; }
    if (p->phase == MP_OVER) return;
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    geometry();
    if (p->phase == MP_DISCARD) {
        int i = ct_hand_hit(&T, 4, (int[4]){sel & 1 ? CD_RAISED : 0, sel & 2 ? CD_RAISED : 0, sel & 4 ? CD_RAISED : 0,
                                            sel & 8 ? CD_RAISED : 0}, NULL, x, y);
        if (i >= 0) {
            sel ^= 1 << i;
            ui_sound(SND_TAP);
            return;
        }
    }
    switch (vt_hit(K.b, K.nb, x, y)) {
    case B_MUS: act(MA_MUS); break;
    case B_CUT: act(MA_CUT); break;
    case B_OK:
        if (sel) act(MA_DISCARD | sel);
        else ui_sound(SND_DENY);
        break;
    case B_PASS: act(MA_PASS); break;
    case B_MINUS:
        if (amount > min_amount()) amount--, ui_sound(SND_TAP);
        else ui_sound(SND_DENY);
        break;
    case B_PLUS:
        if (p->bet + amount < MUS_MAX_BET) amount++, ui_sound(SND_TAP);
        else ui_sound(SND_DENY);
        break;
    case B_BET: act(MA_BET | amount); break;
    case B_ORDAGO: act(MA_ORDAGO); break;
    case B_QUIERO: act(MA_QUIERO); break;
    case B_NOQUIERO: act(MA_NOQUIERO); break;
    default: ui_sound(SND_DENY);
    }
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the side's score */
{
    float h = 0.46f * ui.unit;
    int s = G.pos.score[player & 1];
    ui_number(b.x + ct_number_w(s, h, 0) / 2, b.y + b.h / 2, h, s, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the eight kings: a rey and a 3 (a rey too), fanned, with a coin's as under them */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    int deck = es_deck();
    float w = 0.36f * size;
    memset(rgba, 0, (size_t)size * size * 4);
    cards_paint_turned(rgba, size, deck, CARD(ES_COPAS, 3), 0.3f * size, 0.52f * size, w, -14);
    cards_paint_turned(rgba, size, deck, CARD(ES_OROS, ES_REY), 0.5f * size, 0.47f * size, w, 0);
    cards_paint_turned(rgba, size, deck, CARD(ES_BASTOS, ES_REY), 0.7f * size, 0.52f * size, w, 14);
}

static const game mus_game = {
    .id = "mus", .order = 325, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(mus_game)
