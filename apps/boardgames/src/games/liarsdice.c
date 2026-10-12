/*
 * Liar's Dice in RetroStone Board: the bluffing game of dice under cups, for 2 to 6 players passing the device round
 * (the rules: liarsdice_rules.c, liarsdice.h). VIEW_HIDDEN: each cup is secret, a cover hides the screen whenever
 * the device passes; the reveal after a call is for everyone (no cover).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The table, from the top: the others' cups (leather, a band in the player's colour, how many dice each has left),
 * the bid to beat (the bidder's chip, how many, a die of the face) and the round's earlier bids smaller under it, the
 * player's own dice (their cup lifted beside them; when no cover came before, as after a reveal everyone watched, the
 * cup stays down until tapped), then the bid picker: a grid of numbers (how many dice) and the five faces 2 to 6 (the
 * ones are wild, nobody bids them), the choice always a legal raise (picking one adjusts the other), and two
 * buttons: bid (a check) and liar (a cup lifted off its dice). Landscape: the picker on the right.
 * The call: every cup lifts, a row of dice for each player; the dice that count (the face, and the ones) light up one
 * by one while a gold count climbs beside the bid; then the verdict: the count in gold (the bid held: the caller
 * loses a die) or in red (a lie: the bidder loses one), and the loser's die fades away. Tap to throw the next round.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/liarsdice.h"
#include "games/race.h"
#include "shell.h"

#define BID_MS 320                              /* a bid landing in the middle */
#define REST_MS 350                             /* before the device passes */
#define LIFT_MS 520                             /* the cups lifting at the call */
#define COUNT_AT 700                            /* the counting starts */
#define VERDICT_MS 1100                         /* the verdict shown, the lost die fading */

static lgame G;
static const lpos *P = &G.pos;
static int sel_count, sel_face, peek, seen_viewer = -2, rolled[LD_MAXP];
static Uint32 bid_t0, rev_t0, roll_t0, quiet;

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static struct {
    box b;
    int me, land;
    float u;
    box others, bid, mine, nums, faces, knobs;  /* the table's parts */
    int cols;                                   /* the numbers' grid */
    float chip, die;                            /* a number's chip (its height), a face die */
} T;

enum { K_BID, K_LIAR, K_NEXT, KNOBS };

static void layout(box b)
{
    float u = ui.unit;
    T.b = b;
    T.u = u;
    T.me = ui_bottom();
    T.land = b.w > b.h * 1.05f;
    if (!T.land) {
        float h = b.h, y = b.y, k = h / (10.6f * u) < 1 ? h / (10.6f * u) : 1;
        float ho = 1.9f * u * k, hb = 2.2f * u * k, hm = 1.6f * u * k, hn = 2.5f * u * k, hf = 1.15f * u * k;
        float gap = (h - ho - hb - hm - hn - hf - 1.3f * u * k) / 6;
        T.others = (box){b.x, y, b.w, ho};
        T.bid = (box){b.x, y += ho + gap, b.w, hb};
        T.mine = (box){b.x, y += hb + gap, b.w, hm};
        T.nums = (box){b.x, y += hm + gap, b.w, hn};
        T.faces = (box){b.x, y += hn + gap, b.w, hf};
        T.knobs = (box){b.x, y += hf + gap, b.w, b.y + b.h - y};
        T.cols = 10;
    } else {
        float lw = floorf(b.w * 0.55f), rx = b.x + lw + 0.2f * u, rw = b.x + b.w - rx;
        float ho = b.h * 0.3f, hb = b.h * 0.34f;
        T.others = (box){b.x, b.y, lw, ho};
        T.bid = (box){b.x, b.y + ho, lw, hb};
        T.mine = (box){b.x, b.y + ho + hb, lw, b.h - ho - hb};
        T.nums = (box){rx, b.y, rw, b.h * 0.52f};
        T.faces = (box){rx, b.y + b.h * 0.54f, rw, b.h * 0.2f};
        T.knobs = (box){rx, b.y + b.h * 0.76f, rw, b.h * 0.24f};
        T.cols = 6;
    }
    T.die = floorf(fminf(0.95f * u, fminf(T.faces.w / 6.2f, T.faces.h * 0.8f)));
}

/* the numbers' grid: 1..all the dice on the table */
static void num_cell(int n, float *cx, float *cy, float *w, float *h)
{
    int total = ld_total(P) > 0 ? ld_total(P) : 1, cols = total < T.cols ? total : T.cols;
    int rows = (total + T.cols - 1) / T.cols;
    float cw = fminf(T.nums.w / cols, 1.25f * T.u), ch = fminf(T.nums.h / (rows > 2 ? rows : 2), 0.95f * T.u);
    int i = n - 1, r = i / T.cols, c = i % T.cols;
    *w = cw * 0.86f;
    *h = ch * 0.8f;
    *cx = T.nums.x + T.nums.w / 2 + (c - (cols - 1) / 2.0f) * cw;
    *cy = T.nums.y + T.nums.h / 2 + (r - (rows - 1) / 2.0f) * ch;
}

static void face_at(int f, float *cx, float *cy)
{
    *cx = T.faces.x + T.faces.w / 2 + (f - 4) * T.die * 1.22f;
    *cy = T.faces.y + T.faces.h / 2;
}

static void knob_xy(int k, float *x, float *y, float *d)
{
    *d = fminf(1.05f * T.u, T.knobs.h * 0.8f);
    *y = T.knobs.y + T.knobs.h / 2;
    if (k == K_NEXT) *x = T.knobs.x + T.knobs.w / 2;
    else *x = T.knobs.x + T.knobs.w / 2 + (k == K_BID ? 1 : -1) * 1.0f * T.u;
}

/* the others in turn order after the viewer: their places in the top row */
static int others_order(int *who)
{
    int k = 0;
    for (int i = 1; i < P->players; i++) who[k++] = (T.me + i) % P->players;
    return k;
}

static void other_at(int i, int k, float *cx, float *cy, float *s)
{
    float cw = T.others.w / k;
    *s = floorf(fminf(1.3f * T.u, fminf(cw * 0.72f, T.others.h * 0.62f)));
    *cx = T.others.x + (i + 0.5f) * cw;
    *cy = T.others.y + T.others.h * 0.42f;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static const uint8_t LEATHER[3] = {122, 74, 44}, LEATHER_INK[3] = {58, 34, 20};

/* a dice cup upside down, seen from the side: brown leather, a band of the player's colour near its mouth */
static void cup_paint(int player, int s, uint8_t *o)
{
    memset(o, 0, (size_t)s * s * 4);
    art_fig *f = art_fig_new();
    static const float body[] = {0.3f, 0.12f, 0.7f, 0.12f, 0.86f, 0.88f, 0.14f, 0.88f};
    art_poly(f, ART_PART, body, 4);
    art_ellipse(f, ART_PART, 0.5f, 0.88f, 0.36f, 0.06f);
    art_style st = race_style(LEATHER, LEATHER_INK, 0.025f, 0.45f);
    art_render(f, &st, s, s, (float)s, 0, 0, o);
    art_fig_free(f);
    f = art_fig_new();                          /* the top, a little lighter */
    art_ellipse(f, ART_PART, 0.5f, 0.12f, 0.2f, 0.045f);
    const uint8_t top[3] = {150, 96, 60};
    art_fig_paint(o, s, f, top, LEATHER_INK, 0.015f, 0.1f);
    f = art_fig_new();                          /* the band */
    static const float band[] = {0.188f, 0.68f, 0.812f, 0.68f, 0.835f, 0.79f, 0.165f, 0.79f};
    art_poly(f, ART_PART, band, 4);
    art_style bs = art_player[player < 0 ? 0 : player % ART_PLAYERS];
    bs.line = 0.012f;
    uint8_t *b = calloc((size_t)s * s, 4);
    if (b) {
        art_render(f, &bs, s, s, (float)s, 0, 0, b);
        art_blend(o, s, s, b, s, s, 0, 0);
        free(b);
    }
    art_fig_free(f);
    f = art_fig_new();                          /* stitches */
    for (int i = 0; i < 7; i++) {
        float y = 0.2f + i * 0.065f;
        art_rect(f, ART_PART, 0.5f - 0.006f, y, 0.5f + 0.006f, y + 0.03f);
    }
    const uint8_t stitch[3] = {214, 182, 140};
    art_fig_paint(o, s, f, stitch, NULL, 0, 0.1f);
}

static SDL_Texture *t_cup[LD_MAXP], *t_tok[LD_MAXP], *t_chip, *t_chip_on, *t_small[LD_MAXP];
static int made_cup = -1, made_tok = -1, made_chip = -1, made_small = -1;

/* a die at (cx, cy), turned, lifted (its shadow stays down), faded */
static void die_at(int player, int value, float cx, float cy, float size, float deg, float lift, float alpha)
{
    ui_die(cx, cy, size, player, value, deg, lift, alpha, 1);
}

/* the buttons' pictures (the bid is the shell's tick): next, an arrow going round; liar, a cup lifted off two dice,
 * an arrow up beside it */
static void paint_knob(int kind, int s, uint8_t *o)
{
    static const uint8_t cream[3] = {246, 238, 220}, red[3] = {196, 64, 48}, dark[3] = {52, 34, 22};
    art_fig *f = art_fig_new();
    if (kind == K_NEXT) {
        art_arc(f, ART_PART, 0.5f, 0.52f, 0.17f, 0.25f, 200, 480);
        art_head(f, ART_PART, 0.29f, 0.45f, 250, 0.2f);
        art_fig_paint(o, s, f, cream, NULL, 0, 0.1f);
        return;
    }
    static const float body[] = {0.36f, 0.16f, 0.6f, 0.16f, 0.68f, 0.48f, 0.28f, 0.48f};
    art_poly(f, ART_PART, body, 4);
    art_fig_paint(o, s, f, red, dark, 0.02f, 0.1f);
    f = art_fig_new();
    art_rect(f, ART_PART, 0.27f, 0.58f, 0.45f, 0.76f);
    art_rect(f, ART_PART, 0.51f, 0.6f, 0.69f, 0.78f);
    art_fig_paint(o, s, f, cream, dark, 0.015f, 0.1f);
    f = art_fig_new();
    art_band(f, ART_PART, 0.79f, 0.52f, 0.79f, 0.28f, 0.05f);
    art_head(f, ART_PART, 0.79f, 0.25f, -90, 0.13f);
    art_fig_paint(o, s, f, cream, NULL, 0, 0.1f);
}

static void textures(void)
{
    int who[LD_MAXP];
    int n = others_order(who);
    float x, y, s;
    other_at(0, n > 0 ? n : 1, &x, &y, &s);
    int cs = (int)fmaxf(s, 1.3f * T.u);
    if (made_cup != cs) {
        made_cup = cs;
        for (int q = 0; q < LD_MAXP; q++) {
            ui_free(&t_cup[q]);
            cup_paint(q, cs, ui_buf(cs, cs));
            t_cup[q] = ui_tex(cs, cs, ui_buf(cs, cs));
        }
    }
    int ts = (int)(0.62f * T.u);
    if (made_tok != ts) {
        made_tok = ts;
        for (int q = 0; q < LD_MAXP; q++) {
            ui_free(&t_tok[q]);
            cards_token(q, ts, ui_buf(ts, ts));
            t_tok[q] = ui_tex(ts, ts, ui_buf(ts, ts));
        }
    }
    float cx, cy, w, h;
    num_cell(1, &cx, &cy, &w, &h);
    int key = (int)w * 1000 + (int)h;
    if (made_chip != key) {                     /* the numbers' chips: plain, chosen */
        made_chip = key;
        const uint8_t off[4] = {40, 56, 50, 255}, on[4] = {150, 118, 46, 255};
        ui_free(&t_chip);
        ui_free(&t_chip_on);
        art_roundrect((int)w, (int)h, 0.3f * h, off, ui_buf((int)w, (int)h));
        t_chip = ui_tex((int)w, (int)h, ui_buf((int)w, (int)h));
        art_roundrect((int)w, (int)h, 0.3f * h, on, ui_buf((int)w, (int)h));
        t_chip_on = ui_tex((int)w, (int)h, ui_buf((int)w, (int)h));
    }
}

static void smalls(void)                        /* the bars' die */
{
    int ss = (int)(0.6f * ui.unit);
    if (made_small == ss || ss < 4) return;
    made_small = ss;
    for (int q = 0; q < LD_MAXP; q++) {
        ui_free(&t_small[q]);
        art_die(5, q, ss, ui_buf(ss, ss));
        t_small[q] = ui_tex(ss, ss, ui_buf(ss, ss));
    }
}

/* ---- the choice: always a legal raise ---------------------------------------------------------------------------- */
static int count_ok(int n)                      /* some face makes n dice a raise */
{
    for (int f = 2; f <= 6; f++)
        if (ld_raises(P, n, f)) return 1;
    return 0;
}

static void choose_default(void)
{
    if (!P->count) sel_count = 1, sel_face = sel_face >= 2 && sel_face <= 6 ? sel_face : 2;
    else if (P->face < 6) sel_count = P->count, sel_face = P->face + 1;
    else sel_count = P->count + 1, sel_face = 2;
    if (!ld_raises(P, sel_count, sel_face)) sel_count = 0;
}

static void pick_count(int n)
{
    if (!count_ok(n)) { ui_sound(SND_DENY); return; }
    sel_count = n;
    if (!ld_raises(P, n, sel_face))
        for (int f = 2; f <= 6; f++)
            if (ld_raises(P, n, f)) { sel_face = f; break; }
    ui_sound(SND_TAP);
}

static void pick_face(int f)
{
    int n = sel_count > 0 ? sel_count : 1;
    while (n <= ld_total(P) && !ld_raises(P, n, f)) n++;
    if (n > ld_total(P)) { ui_sound(SND_DENY); return; }
    if (!ld_raises(P, sel_count, f)) sel_count = n;
    sel_face = f;
    ui_sound(SND_TAP);
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static int busy(void)
{
    Uint32 t = ui_ms();
    if (quiet && t < quiet) return 1;
    return 0;
}

static Uint32 reveal_ms(void)                   /* how long the reveal runs */
{
    int step = P->found ? (int)fminf(170, 2400.0f / P->found) : 0;
    return COUNT_AT + (Uint32)(P->found * step) + 300 + VERDICT_MS;
}

static void start(int variant)
{
    (void)variant;
    ld_new(&G, ui.players, (uint32_t)ui_random(1 << 30));
    sel_count = 0, sel_face = 0;
    choose_default();
    peek = 1;                                   /* a new game covers first */
    seen_viewer = -2;
    memset(rolled, 0, sizeof rolled);
    quiet = 0;
}

static int status(void) { return P->phase == LP_OVER ? gs_win(P->winner) : GS_PLAY; }
static int turn(void) { return P->turn; }

static void act(int a)
{
    int kind = LD_KIND(a);
    if (!ld_play(&G, a)) { ui_sound(SND_DENY); return; }
    switch (kind) {
    case LA_BID:
        bid_t0 = ui_ms();
        quiet = bid_t0 + BID_MS + REST_MS;
        ui_sound(SND_PLACE);
        ui_handoff(P->turn);
        break;
    case LA_LIAR:
        rev_t0 = ui_ms();
        quiet = rev_t0 + reveal_ms();
        ui_sound(SND_MOVE);
        ui_sound_later(SND_CAPTURE, (int)(reveal_ms() - VERDICT_MS));
        break;
    case LA_NEXT:
        quiet = 0;
        if (P->turn == T.me) peek = 0;          /* everyone watched: the cup stays down until tapped */
        ui_handoff(P->turn);
        break;
    }
    choose_default();
}

static int undo(void) { return 0; }             /* each turn is one action, the device passes after it */

static int script(const char *s)
{
    int ok = ld_script(&G, s);
    choose_default();
    rev_t0 = bid_t0 = ui_ms();
    quiet = 0;
    peek = 1;
    ui_handoff(P->turn);                        /* the reveal: the caller has the device */
    return ok;
}

static unsigned winners(void) { return P->phase == LP_OVER ? 1u << P->winner : 0; }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_others(void)
{
    int who[LD_MAXP], k = others_order(who);
    for (int i = 0; i < k; i++) {
        int q = who[i];
        float cx, cy, s;
        other_at(i, k, &cx, &cy, &s);
        float a = P->ndice[q] ? 1 : 0.28f;
        if (q == P->turn && P->phase == LP_BID) ui_mark_gold(cx, cy, 1.5f * s);
        ui_shadow_flat(cx, cy + 0.4f * s, 0.75f * s, 0.2f * s, 0.6f * a);
        SDL_SetTextureAlphaMod(t_cup[q], (Uint8)(255 * a));
        ui_put_c(t_cup[q], cx, cy, s, s);
        SDL_SetTextureAlphaMod(t_cup[q], 255);
    }
}

/* a bid: the bidder's chip, how many, a die of the face; size h (the digits' height) */
static void draw_bid_row(float cx, float cy, float h, int player, int count, int face, float alpha)
{
    int digits = count >= 10 ? 2 : 1;
    float tw = 0.9f * h, nw = (0.74f * (digits - 1) + 0.62f) * h, dw = 1.15f * h, gap = 0.3f * h;
    float x = cx - (tw + nw + dw + 2 * gap) / 2;
    SDL_SetTextureAlphaMod(t_tok[player], (Uint8)(255 * alpha));
    ui_put_c(t_tok[player], x + tw / 2, cy, tw, tw);
    SDL_SetTextureAlphaMod(t_tok[player], 255);
    x += tw + gap;
    ui_number(x + nw / 2, cy, h, count, 1);
    x += nw + gap;
    die_at(player, face, x + dw / 2, cy, dw, 0, 0, alpha);
}

static void draw_bid(void)
{
    box b = T.bid;
    float h = fminf(0.85f * T.u, b.h * 0.42f), cy = b.y + b.h * 0.36f;
    if (!P->count) {                            /* no bid yet: the opener's chip in a gold light */
        ui_mark_gold(b.x + b.w / 2, cy, 2.2f * h);
        ui_put_c(t_tok[P->turn], b.x + b.w / 2, cy, 0.9f * h, 0.9f * h);
        return;
    }
    float f = clampf((ui_ms() - bid_t0) / (float)BID_MS, 0, 1), k = 1 + 0.25f * (1 - race_ease(f));
    if (f < 1) ui_animate();
    ui_mark_gold(b.x + b.w / 2, cy, 2.6f * h);
    draw_bid_row(b.x + b.w / 2, cy, h * k, P->bidder, P->count, P->face, 1);
    /* the round's earlier bids, smaller, the latest first */
    int n = P->nbids - 1, shown = n < 4 ? n : 4;
    float sh = 0.4f * h, rw = 3.4f * sh, y = b.y + b.h * 0.82f;
    for (int i = 0; i < shown; i++) {
        int j = n - 1 - i;
        float x = b.x + b.w / 2 + (i - (shown - 1) / 2.0f) * rw * 1.15f;
        draw_bid_row(x, y, sh, P->bids[j].player, P->bids[j].count, P->bids[j].face, 0.75f - 0.1f * i);
    }
}

/* the viewer's dice: in a row, their cup lifted beside them (or down over them) */
static void draw_mine(void)
{
    int me = T.me;
    if (me < 0 || me >= P->players) return;
    box b = T.mine;
    int n = P->ndice[me];
    float s = floorf(fminf(0.95f * T.u, fminf(b.h * 0.7f, b.w / 7.5f))), gap = 0.28f * s;
    float cx = b.x + b.w / 2 - 0.5f * s, cy = b.y + b.h / 2, x0 = cx - (n * s + (n - 1) * gap) / 2 + s / 2;
    float cs = 1.35f * s;
    if (!n) return;
    if (!peek || ui.viewer < 0) {               /* the cup down over the dice: tap it (the hand-off cover: down) */
        ui_shadow_flat(cx, cy + 0.35f * cs, 1.2f * cs, 0.3f * cs, 0.7f);
        ui_put_c(t_cup[me], cx, cy, cs * 1.2f, cs * 1.2f);
        if (ui.viewer >= 0) ui_mark_gold(cx, cy, 2.2f * cs);
        return;
    }
    if (rolled[me] != P->round + 1) { rolled[me] = P->round + 1; roll_t0 = ui_ms(); ui_sound(SND_DICE); }
    int t = (int)(ui_ms() - roll_t0);
    for (int i = 0; i < n; i++) {
        float x = x0 + i * (s + gap), tilt = (race_hash(P->round * 7 + me, i) - 0.5f) * 20;
        ui_die_thrown(x, cy, s, me, P->dice[me][i], tilt, i, t, 1);
    }
    float ux = x0 + n * (s + gap) + 0.25f * cs, uy = cy - 0.15f * cs;   /* the cup, lifted, tipped beside them */
    ui_shadow_flat(ux, cy + 0.4f * cs, 0.9f * cs, 0.22f * cs, 0.5f);
    ui_put_rot(t_cup[me], ux, uy, cs, cs, 18);
}

static void draw_picker(void)
{
    int total = ld_total(P), my = P->turn == T.me && P->phase == LP_BID && !ui_handing();
    for (int n = 1; n <= total; n++) {
        float cx, cy, w, h;
        num_cell(n, &cx, &cy, &w, &h);
        int on = n == sel_count, ok = count_ok(n) && my;
        SDL_Texture *t = on ? t_chip_on : t_chip;
        SDL_SetTextureAlphaMod(t, ok || on ? 255 : 90);
        ui_put_c(t, cx, cy, w, h);
        SDL_SetTextureAlphaMod(t, 255);
        ui_number(cx, cy, fminf(0.56f * h, w / (n >= 10 ? 1.6f : 0.9f)), n, ok || on);
    }
    for (int f = 2; f <= 6; f++) {
        float cx, cy;
        face_at(f, &cx, &cy);
        if (f == sel_face && my) ui_mark_gold(cx, cy, 1.7f * T.die);
        die_at(T.me >= 0 ? T.me : 0, f, cx, cy, T.die * (f == sel_face ? 1.06f : 0.92f), 0, 0, my ? 1 : 0.5f);
    }
    float x, y, d;
    knob_xy(K_LIAR, &x, &y, &d);
    ui_button_paint(x, y, d, paint_knob, K_LIAR, 1, my && P->count ? 0 : UI_DIM);
    knob_xy(K_BID, &x, &y, &d);
    int ok = my && sel_count && ld_raises(P, sel_count, sel_face);
    ui_button(x, y, d, ICON_TICK, ok ? UI_LIT : UI_DIM);
}

/* the call: a row a player (their chip, their dice), the dice that count lighting up, the verdict */
static void draw_reveal(void)
{
    box b = T.b;
    int t = (int)(ui_ms() - rev_t0), step = P->found ? (int)fminf(170, 2400.0f / P->found) : 0;
    int verdict = COUNT_AT + P->found * step + 300;
    if ((Uint32)t < reveal_ms()) ui_animate();
    /* the bid on top, the count beside it */
    float h = fminf(0.8f * T.u, b.h * 0.07f), top = b.y + 0.9f * h;
    draw_bid_row(b.x + b.w * 0.4f, top, h, P->bidder, P->count, P->face, 1);
    int lit = t < COUNT_AT ? 0 : step ? (t - COUNT_AT) / step + 1 : 0;
    lit = lit > P->found ? P->found : lit;
    float nx = b.x + b.w * 0.78f;
    if (t >= verdict) {
        if (P->held) ui_mark_gold(nx, top, 3.2f * h);
        else ui_mark_glow(nx, top, 3.2f * h);
    }
    ui_number(nx, top, 1.3f * h, lit, 1);
    /* the players' rows */
    int n = P->players, row = 0;
    float y0 = top + 1.2f * h, rh = fminf(1.25f * T.u, (b.y + b.h - y0 - 1.6f * T.u) / n);
    float s = fminf(0.8f * rh, b.w / 8.5f);
    y0 += (b.y + b.h - 1.6f * T.u - y0 - n * rh) * 0.4f;     /* the rows in the middle of the room left */
    int k = 0;                                  /* the counting goes row by row, die by die */
    for (int i = 0; i < n; i++, row++) {
        int q = (T.me + i) % n, nd = P->ndice[q] + (q == P->loser), last = q == P->loser ? P->ndice[q] : -1;
        float cy = y0 + (row + 0.5f) * rh, x = b.x + 0.7f * T.u;
        if (!nd) {
            SDL_SetTextureAlphaMod(t_tok[q], 70);
            ui_put_c(t_tok[q], x, cy, 0.6f * s, 0.6f * s);
            SDL_SetTextureAlphaMod(t_tok[q], 255);
            continue;
        }
        ui_put_c(t_tok[q], x, cy, 0.7f * s, 0.7f * s);
        float dx = x + 0.75f * T.u;
        for (int d = 0; d < nd; d++) {
            int v = P->dice[q][d], counts = v == P->face || v == 1;
            float px = dx + d * 1.18f * s + s / 2, a = 1;
            if (counts) {
                if (k < lit) ui_mark_gold(px, cy, 1.6f * s);
                k++;
            } else if (t >= COUNT_AT) a = 0.45f;
            if (d == last && t >= verdict) {    /* the die lost: a red glow, it fades away */
                float f = clampf((t - verdict) / (float)(VERDICT_MS * 0.8f), 0, 1);
                ui_mark_glow(px, cy, 1.8f * s * (1 - 0.4f * f));
                a *= 1 - f;
            }
            die_at(q, v, px, cy, s, (race_hash(P->round * 13 + q, d) - 0.5f) * 16, 0, a);
        }
        /* the cup lifting off */
        float f = clampf((t - 60 * i) / (float)LIFT_MS, 0, 1);
        if (f < 1) {
            float cs = 1.25f * s, cx = dx + (nd * 1.18f * s) / 2;
            SDL_SetTextureAlphaMod(t_cup[q], (Uint8)(255 * (1 - f)));
            ui_put_c(t_cup[q], cx, cy - race_ease(f) * 1.2f * rh, cs * 1.4f, cs * 1.4f);
            SDL_SetTextureAlphaMod(t_cup[q], 255);
        }
    }
    if (P->phase == LP_REVEAL && t >= (int)reveal_ms()) {   /* next round: a button */
        float x, y, d;
        knob_xy(K_NEXT, &x, &y, &d);
        y = b.y + b.h - 0.7f * d;
        ui_button_paint(x, y, d, paint_knob, K_NEXT, 1, UI_LIT);
    }
}

static void draw(void)
{
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= P->players) T.me = P->turn;
    if (seen_viewer != ui.viewer) {             /* a hand-off has passed (a cover came): the cup lifts */
        if (seen_viewer != -2) peek = 1;
        seen_viewer = ui.viewer;
    }
    textures();
    if (P->phase != LP_BID) { draw_reveal(); return; }
    draw_others();
    draw_bid();
    draw_mine();
    draw_picker();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (P->phase == LP_OVER) return;
    if (P->phase == LP_REVEAL) {
        if (busy()) return;
        act(LD_ACT(LA_NEXT, 0, 0));
        return;
    }
    if (P->turn != T.me || ui_handing()) { ui_sound(SND_DENY); return; }
    box m = T.mine;
    if (x >= m.x && y >= m.y && x < m.x + m.w && y < m.y + m.h) {   /* the cup: lift it, or put it down */
        peek = !peek;
        ui_sound(SND_TAP);
        return;
    }
    float kx, ky, d;
    knob_xy(K_BID, &kx, &ky, &d);
    if (ui_button_hit(kx, ky, d, x, y)) {
        if (sel_count && ld_raises(P, sel_count, sel_face)) act(LD_ACT(LA_BID, sel_count, sel_face));
        else ui_sound(SND_DENY);
        return;
    }
    knob_xy(K_LIAR, &kx, &ky, &d);
    if (ui_button_hit(kx, ky, d, x, y)) {
        if (P->count) act(LD_ACT(LA_LIAR, 0, 0));
        else ui_sound(SND_DENY);
        return;
    }
    for (int n = 1; n <= ld_total(P); n++) {
        float cx, cy, w, h;
        num_cell(n, &cx, &cy, &w, &h);
        if (fabsf(x - cx) <= w * 0.58f && fabsf(y - cy) <= h * 0.62f) { pick_count(n); return; }
    }
    for (int f = 2; f <= 6; f++) {
        float cx, cy;
        face_at(f, &cx, &cy);
        if (fabsf(x - cx) <= 0.6f * T.die && fabsf(y - cy) <= 0.65f * T.die) { pick_face(f); return; }
    }
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the dice left */
{
    smalls();
    int n = G.pos.ndice[player];
    if (P->phase != LP_BID && player == P->loser && ui_ms() - rev_t0 < reveal_ms() - VERDICT_MS)
        n++;                                    /* the reveal: not lost yet */
    ui_bar_count(b, t_small[player % LD_MAXP], n);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a cup lifted off three dice, another cup down */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    int cs = (int)(0.56f * size), ds = (int)(0.2f * size);
    uint8_t *b = malloc((size_t)cs * cs * 4), *d = malloc((size_t)ds * ds * 4), *sh = malloc((size_t)ds * ds * 4);
    if (b && d && sh) {
        static const int faces[3] = {4, 1, 4};
        art_flat_shadow(ds, sh);
        for (int i = 0; i < 3; i++) {
            int x = (int)(0.1f * size + i * 0.24f * size), y = (int)(0.7f * size - (i == 1 ? 0.06f : 0) * size);
            art_blend(rgba, size, size, sh, ds, ds, x + ds / 12, y + ds / 8);
            art_die(faces[i], 0, ds, d);
            art_blend(rgba, size, size, d, ds, ds, x, y);
        }
        cup_paint(1, cs, b);
        art_blend(rgba, size, size, b, cs, cs, (int)(0.44f * size), (int)(0.02f * size));
        cup_paint(2, cs, b);
        art_blend(rgba, size, size, b, cs, cs, (int)(0.0f * size), (int)(0.04f * size));
    }
    free(b);
    free(d);
    free(sh);
}

static const game liarsdice_game = {
    .id = "liarsdice", .order = 156, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .view = VIEW_HIDDEN,
};
REGISTER_GAME(liarsdice_game)
