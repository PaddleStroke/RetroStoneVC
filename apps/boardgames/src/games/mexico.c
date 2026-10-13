/*
 * Mexico (Mäxchen) in RetroStone Board: the bluffing game of two dice under a cup, for 2 to 6 players passing the
 * device round (the rules and the notation: mexico_rules.c, mexico.h). VIEW_HIDDEN: what lies under the cup is
 * secret, a cover hides the screen whenever the device passes; the cup lifted after a call is for everyone (no cover).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The table, from the top: the value to beat (who announced it, and the value as two dice, in a gold light; 21,
 * Mexico, in a brighter one), the round's earlier values smaller under it; the cup (turned walnut, two brass rings, a
 * band in the colour of the player holding it); the pad of the 21 values, lowest first (the plain values, then the
 * doubles, then 21 last), each as its two dice, those below the value to beat faded; the buttons: lift the cup (a
 * call), throw (believe it and throw), announce (a check). After a throw the cup tips aside and the two dice tumble
 * out, for the player alone (a tap on the cup puts it back over them); their value glows on the pad. Landscape: the
 * pad and the buttons on the right.
 * The call: the cup lifts off the dice; the value announced above them; then the verdict: the dice in a gold light
 * (they make the value: the caller loses a life) or in a red glow (a bluff: the announcer loses one), the loser's
 * chip and the life they lose, fading. Tap to start the next round.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/dicecup.h"
#include "games/mexico.h"
#include "shell.h"

#define SAY_MS 320                              /* a value announced landing on the table */
#define REST_MS 350                             /* before the device passes */
#define LIFT_MS 600                             /* the cup lifting at a call */
#define VERDICT_AT 1100                         /* the verdict shown */
#define VERDICT_MS 1200                         /* the life lost fading */
#define COLS 7

static mxgame G;
static const mxpos *P = &G.pos;
static int sel = -1, peek;
static Uint32 say_t0, roll_t0, rev_t0, quiet;

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static struct {
    box b, claim, cup, pad, knobs;
    int me, land;
    float u, cw, ch;                            /* a pad cell */
} T;

enum { K_CALL, K_THROW, K_SAY, KNOBS };

static void layout(box b)
{
    float u = ui.unit;
    T.b = b;
    T.u = u;
    T.land = b.w > b.h * 1.05f;
    if (!T.land) {
        float k = b.h / (10.4f * u) < 1 ? b.h / (10.4f * u) : 1, y = b.y;
        float hc = 2.5f * u * k, hu = 2.6f * u * k, hp = 3.3f * u * k, hk = 1.3f * u * k;
        float gap = (b.h - hc - hu - hp - hk) / 4;
        T.claim = (box){b.x, y, b.w, hc};
        T.cup = (box){b.x, y += hc + gap, b.w, hu};
        T.pad = (box){b.x, y += hu + gap, b.w, hp};
        T.knobs = (box){b.x, y += hp + gap, b.w, b.y + b.h - y};
    } else {
        float lw = floorf(b.w * 0.45f), rx = b.x + lw + 0.2f * u, rw = b.x + b.w - rx;
        T.claim = (box){b.x, b.y, lw, b.h * 0.45f};
        T.cup = (box){b.x, b.y + b.h * 0.45f, lw, b.h * 0.55f};
        T.pad = (box){rx, b.y, rw, b.h * 0.7f};
        T.knobs = (box){rx, b.y + b.h * 0.72f, rw, b.h * 0.28f};
    }
    T.cw = fminf(T.pad.w / COLS, 1.45f * u);
    T.ch = fminf(T.pad.h / 3, 1.0f * u);
}

/* the pad: rank r's cell (the plain values in two rows, the doubles and 21 in the third) */
static void cell(int r, float *cx, float *cy)
{
    int row = r / COLS, col = r % COLS;
    *cx = T.pad.x + T.pad.w / 2 + (col - (COLS - 1) / 2.0f) * T.cw;
    *cy = T.pad.y + T.pad.h / 2 + (row - 1) * T.ch;
}

static void knob_xy(int k, float *x, float *y, float *d)
{
    *d = fminf(1.1f * T.u, T.knobs.h * 0.85f);
    *y = T.knobs.y + T.knobs.h / 2;
    *x = T.knobs.x + T.knobs.w / 2 + (k - 1) * 1.6f * T.u;
}

static void cup_at(float *cx, float *cy, float *s)
{
    *s = floorf(fminf(T.cup.h * 0.92f, 2.2f * T.u));
    *cx = T.cup.x + T.cup.w / 2;
    *cy = T.cup.y + T.cup.h / 2;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static SDL_Texture *t_cup[MX_MAXP], *t_tok[MX_MAXP], *t_chip, *t_chip_on;
static int made_cup = -1, made_tok = -1, made_chip = -1;

/* the call's button: a small cup lifted off two dice */
static void paint_knob(int kind, int s, uint8_t *o)
{
    (void)kind;
    int c = (int)(0.6f * s);
    uint8_t *b = malloc((size_t)c * c * 4);
    if (!b) return;
    art_cup(ART_CUP_WALNUT, 0, c, b);
    art_blend_rot(o, s, s, b, c, c, 0.56f * s, 0.34f * s, 16);
    free(b);
    art_die_blend(o, s, s, 0.36f * s, 0.72f * s, (int)(0.24f * s), 0, 2, -8);
    art_die_blend(o, s, s, 0.62f * s, 0.74f * s, (int)(0.24f * s), 0, 1, 10);
}

static void textures(void)
{
    float cx, cy, s;
    cup_at(&cx, &cy, &s);
    int cs = (int)s;
    if (made_cup != cs && cs > 4) {
        made_cup = cs;
        for (int q = 0; q < MX_MAXP; q++) {
            ui_free(&t_cup[q]);
            art_cup(ART_CUP_WALNUT, q, cs, ui_buf(cs, cs));
            t_cup[q] = ui_tex(cs, cs, ui_buf(cs, cs));
        }
    }
    int ts = (int)(0.8f * T.u);
    if (made_tok != ts) {
        made_tok = ts;
        for (int q = 0; q < MX_MAXP; q++) {
            ui_free(&t_tok[q]);
            cards_token(q, ts, ui_buf(ts, ts));
            t_tok[q] = ui_tex(ts, ts, ui_buf(ts, ts));
        }
    }
    int w = (int)(0.92f * T.cw), h = (int)(0.84f * T.ch), key = w * 1000 + h;
    if (made_chip != key && w > 4 && h > 4) {   /* the pad's cells: plain, chosen */
        made_chip = key;
        const uint8_t off[4] = {40, 56, 50, 255}, on[4] = {150, 118, 46, 255};
        ui_free(&t_chip);
        ui_free(&t_chip_on);
        art_roundrect(w, h, 0.25f * h, off, ui_buf(w, h));
        t_chip = ui_tex(w, h, ui_buf(w, h));
        art_roundrect(w, h, 0.25f * h, on, ui_buf(w, h));
        t_chip_on = ui_tex(w, h, ui_buf(w, h));
    }
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static Uint32 reveal_ms(void) { return VERDICT_AT + VERDICT_MS; }
static int busy(void) { return quiet && ui_ms() < quiet; }
static int status(void) { return P->phase == MX_OVER ? gs_win(P->winner) : GS_PLAY; }
static int turn(void) { return P->turn; }
static unsigned winners(void) { return P->phase == MX_OVER ? 1u << P->winner : 0; }
static int my_turn(void) { return T.me == P->turn && !ui_handing() && ui.viewer >= 0; }

static int actual(void) { return P->dice[0] ? mx_rank(P->dice[0], P->dice[1]) : -1; }

static void choose_default(void)               /* the value thrown, or the lowest allowed */
{
    int a = actual();
    sel = P->phase != MX_SAY ? -1 : a >= P->claim ? a : P->claim;
}

static void start(int variant)
{
    mx_new(&G, ui.players, variant == 1 ? 6 : 3);
    sel = -1;
    peek = 0;
    quiet = 0;
}

static void act(int a)
{
    int kind = MX_KIND(a);
    if (!mx_play(&G, a)) { ui_sound(SND_DENY); return; }
    switch (kind) {
    case MXA_THROW:
        roll_t0 = ui_ms();
        peek = 1;
        ui_sound(SND_DICE);
        break;
    case MXA_SAY:
        say_t0 = ui_ms();
        quiet = say_t0 + SAY_MS + REST_MS;
        peek = 0;
        ui_sound(SND_PLACE);
        ui_handoff(P->turn);
        break;
    case MXA_CALL:
        rev_t0 = ui_ms();
        quiet = rev_t0 + reveal_ms();
        ui_sound(SND_MOVE);
        ui_sound_later(P->held ? SND_PLACE : SND_CAPTURE, VERDICT_AT);
        break;
    case MXA_NEXT:
        quiet = 0;
        peek = 0;
        ui_handoff(P->turn);
        break;
    }
    choose_default();
}

static int undo(void) { return 0; }             /* a throw is never taken back; announcing passes the device */

static int script(const char *s)
{
    int ok = mx_script(&G, s);
    choose_default();
    say_t0 = rev_t0 = ui_ms();
    roll_t0 = ui_ms() - UI_DICE_MS;
    peek = P->phase == MX_SAY;
    quiet = P->phase == MX_REVEAL || P->phase == MX_OVER ? rev_t0 + reveal_ms() : 0;
    ui_handoff(P->turn);
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void put_alpha(SDL_Texture *t, float cx, float cy, float w, float h, float a)
{
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(a, 0, 1)));
    ui_put_c(t, cx, cy, w, h);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a value: two dice (high first) of size d centred on (cx, cy), in a player's colour */
static void value_at(int rank, float cx, float cy, float d, int player, float alpha)
{
    int hi, lo;
    mx_dice_of(rank, &hi, &lo);
    ui_die(cx - 0.56f * d, cy, d, player, hi, 0, 0, alpha, 1);
    ui_die(cx + 0.56f * d, cy, d, player, lo, 0, 0, alpha, 1);
}

/* a value announced: the player's chip, the value's two dice */
static void claim_row(float cx, float cy, float d, int player, int rank, float alpha)
{
    float tw = 0.85f * d;
    put_alpha(t_tok[player], cx - 1.25f * d, cy, tw, tw, alpha);
    value_at(rank, cx + 0.5f * d, cy, d, player, alpha);
}

static void draw_claim(void)
{
    box b = T.claim;
    float d = fminf(0.85f * T.u, b.h * 0.36f), cy = b.y + b.h * 0.36f, cx = b.x + b.w / 2;
    if (P->claim < 0) {                         /* nothing yet: the round's first player, in a gold light */
        ui_mark_gold(cx, cy, 2.4f * d);
        ui_put_c(t_tok[P->turn], cx, cy, 0.95f * d, 0.95f * d);
        return;
    }
    float f = clampf((ui_ms() - say_t0) / (float)SAY_MS, 0, 1), k = 1 + 0.25f * (1 - ui_ease(f));
    if (f < 1) ui_animate();
    ui_mark_gold(cx + 0.5f * d, cy, (P->claim == MX_MEXICO ? 3.6f : 2.8f) * d);
    if (P->claim == MX_MEXICO) ui_mark_gold(cx + 0.5f * d, cy, 2.2f * d);
    claim_row(cx, cy, d * k, P->claimer, P->claim, 1);
    int n = P->nclaims - 1, shown = n < 4 ? n : 4;   /* the round's earlier values, the latest first */
    float sd = 0.42f * d, rw = 3.4f * sd, y = b.y + b.h * 0.82f;
    for (int i = 0; i < shown; i++) {
        int j = n - 1 - i;
        float x = cx + (i - (shown - 1) / 2.0f) * rw * 1.12f;
        claim_row(x, y, sd, P->claims[j].player, P->claims[j].rank, 0.8f - 0.1f * i);
    }
}

/* the cup: down (the dice under it secret), or tipped aside off the viewer's own throw */
static void draw_cup(void)
{
    float cx, cy, s;
    cup_at(&cx, &cy, &s);
    int holder = P->turn, mine = my_turn() && P->phase == MX_SAY;
    if (mine && peek) {
        float d = fminf(0.42f * s, 1.05f * T.u), t = (float)(ui_ms() - roll_t0);
        float x0 = cx - 0.62f * s;
        for (int i = 0; i < 2; i++)
            ui_die_thrown(x0 + (i ? 0.62f : -0.62f) * d, cy + 0.12f * s, d, holder, P->dice[i],
                          (art_hash(P->round * 7 + holder, i + G.n) - 0.5f) * 24, i, (int)t, 1);
        if (t < UI_DICE_MS) ui_animate();
        ui_shadow_flat(cx + 0.45f * s, cy + 0.42f * s, 0.62f * s, 0.16f * s, 0.5f);
        ui_put_rot(t_cup[holder], cx + 0.45f * s, cy, 0.86f * s, 0.86f * s, 24);
        return;
    }
    ui_shadow_flat(cx, cy + 0.4f * s, 0.7f * s, 0.18f * s, 0.65f);
    ui_put_c(t_cup[holder], cx, cy, s, s);
    if (mine) ui_mark_gold_a(cx, cy, 1.5f * s, 0.5f);   /* a tap lifts it again */
}

static void draw_pad(void)
{
    int on = my_turn() && P->phase == MX_SAY, me = T.me >= 0 ? T.me : 0, a = on ? actual() : -1;
    for (int r = 0; r < MX_VALUES; r++) {
        float cx, cy;
        cell(r, &cx, &cy);
        int ok = on && r >= P->claim, chosen = on && r == sel;
        if (r == a && on && !chosen) ui_mark_gold_a(cx, cy, 1.5f * T.cw, 0.55f);
        put_alpha(chosen ? t_chip_on : t_chip, cx, cy, 0.92f * T.cw, 0.84f * T.ch, ok || chosen ? 1 : 0.4f);
        float d = fminf(0.36f * T.cw, 0.56f * T.ch);
        value_at(r, cx, cy, d, me, ok || chosen ? 1 : on ? 0.3f : 0.55f);
        if (r == MX_MEXICO && (ok || !on)) ui_mark_gold_a(cx, cy, 1.2f * T.cw, 0.35f);
    }
}

static void draw_knobs(void)
{
    int mine = my_turn(), throwing = mine && P->phase == MX_THROW;
    float x, y, d;
    knob_xy(K_CALL, &x, &y, &d);
    ui_button_paint(x, y, d, paint_knob, K_CALL, 1, throwing && P->claim >= 0 ? UI_LIT : UI_DIM);
    knob_xy(K_THROW, &x, &y, &d);
    ui_button_paint(x, y, d, cup_button_pic, CUP_THROW, 1, throwing ? UI_LIT : UI_DIM);
    knob_xy(K_SAY, &x, &y, &d);
    ui_button(x, y, d, ICON_TICK, mine && P->phase == MX_SAY && sel >= P->claim && sel >= 0 ? UI_LIT : UI_DIM);
}

/* the call: the value announced, the cup lifting off the dice, the verdict, the life lost */
static void draw_reveal(void)
{
    box b = T.b;
    int t = (int)(ui_ms() - rev_t0), real = actual();
    if ((Uint32)t < reveal_ms()) ui_animate();
    float d = fminf(1.0f * T.u, b.w / 6), top = b.y + 0.18f * b.h, mid = b.y + 0.45f * b.h;
    claim_row(b.x + b.w / 2, top, 0.8f * d, P->claimer, P->claim, 1);
    if (t >= VERDICT_AT) {
        if (P->held) ui_mark_gold(b.x + b.w / 2, mid, 3.4f * d);
        else ui_mark_glow(b.x + b.w / 2, mid, 3.4f * d);
    }
    value_at(real, b.x + b.w / 2, mid, 1.2f * d, P->claimer, 1);
    float f = clampf(t / (float)LIFT_MS, 0, 1), cs = 2.4f * d;   /* the cup lifting off */
    if (f < 1) put_alpha(t_cup[P->claimer], b.x + b.w / 2, mid - ui_ease(f) * 1.6f * d, cs, cs, 1 - f * f);
    if (t < VERDICT_AT) return;
    /* the loser: their chip, their lives in a row, the one lost fading in a red glow */
    float ly = b.y + 0.7f * b.h, cd = 0.62f * T.u, n = P->lives[P->loser] + 1.0f;
    float x0 = b.x + b.w / 2 - (n * 0.8f * cd) / 2 + 0.6f * T.u;
    float fl = clampf((t - VERDICT_AT) / (float)VERDICT_MS, 0, 1);
    ui_mark_glow(x0 - 1.1f * T.u, ly, 1.6f * T.u);
    ui_put_c(t_tok[P->loser], x0 - 1.1f * T.u, ly, 0.85f * T.u, 0.85f * T.u);
    SDL_Texture *chip = cup_chip((int)cd);
    for (int i = 0; i < (int)n; i++) {
        float x = x0 + (i + 0.5f) * 0.8f * cd, a = 1;
        if (i == (int)n - 1) {
            ui_mark_glow(x, ly, 1.8f * cd * (1 - 0.4f * fl));
            a = 1 - fl;
        }
        if (chip) put_alpha(chip, x, ly, cd, cd, a);
    }
    if (P->phase == MX_REVEAL && t >= (int)reveal_ms()) {   /* the next round */
        float kx, ky, kd;
        knob_xy(K_THROW, &kx, &ky, &kd);
        ui_button(b.x + b.w / 2, b.y + b.h - 0.75f * kd, kd, ICON_ARROW, UI_LIT);
    }
}

static void draw(void)
{
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= P->players) T.me = P->turn;
    textures();
    if (P->phase == MX_REVEAL || P->phase == MX_OVER) { draw_reveal(); return; }
    draw_claim();
    draw_cup();
    draw_pad();
    draw_knobs();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    T.me = ui_bottom();                         /* the view may have turned since the last frame */
    if (T.me < 0 || T.me >= P->players) T.me = P->turn;
    if (P->phase == MX_OVER) return;
    if (P->phase == MX_REVEAL) {
        if (!busy()) act(MX_ACT(MXA_NEXT, 0));
        return;
    }
    if (!my_turn() || busy()) { ui_sound(SND_DENY); return; }
    float kx, ky, kd, cx, cy, s;
    knob_xy(K_CALL, &kx, &ky, &kd);
    if (ui_button_hit(kx, ky, kd, x, y)) {
        if (P->phase == MX_THROW && P->claim >= 0) act(MX_ACT(MXA_CALL, 0));
        else ui_sound(SND_DENY);
        return;
    }
    cup_at(&cx, &cy, &s);
    knob_xy(K_THROW, &kx, &ky, &kd);
    int on_cup = fabsf(x - cx) < 0.6f * s && fabsf(y - cy) < 0.5f * s;
    if (ui_button_hit(kx, ky, kd, x, y) || (on_cup && P->phase == MX_THROW)) {
        if (P->phase == MX_THROW) act(MX_ACT(MXA_THROW, MX_DICE(1 + ui_random(6), 1 + ui_random(6))));
        else ui_sound(SND_DENY);
        return;
    }
    if (on_cup || (x >= T.cup.x && y >= T.cup.y && x < T.cup.x + T.cup.w && y < T.cup.y + T.cup.h)) {
        peek = !peek;                           /* the cup back over the dice, or lifted again */
        ui_sound(SND_TAP);
        return;
    }
    knob_xy(K_SAY, &kx, &ky, &kd);
    if (ui_button_hit(kx, ky, kd, x, y)) {
        if (P->phase == MX_SAY && sel >= 0 && sel >= P->claim) act(MX_ACT(MXA_SAY, sel));
        else ui_sound(SND_DENY);
        return;
    }
    for (int r = 0; r < MX_VALUES; r++) {
        float px, py;
        cell(r, &px, &py);
        if (fabsf(x - px) <= 0.5f * T.cw && fabsf(y - py) <= 0.5f * T.ch) {
            if (P->phase == MX_SAY && r >= P->claim) { sel = r; ui_sound(SND_TAP); }
            else ui_sound(SND_DENY);
            return;
        }
    }
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the lives left (the loser's not yet lost before the verdict) */
{
    int n = P->lives[player];
    if ((P->phase == MX_REVEAL || P->phase == MX_OVER) && player == P->loser && ui_ms() - rev_t0 < VERDICT_AT) n++;
    ui_bar_number(b, cup_chip((int)(0.6f * ui.unit)), n);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the cup tipped off 2 and 1, Mexico, in a gold light; the picker's: the cup down and the lives,
 * three or six bone chips */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const uint8_t gold[3] = {255, 196, 64};
    memset(rgba, 0, (size_t)size * size * 4);
    int cs = (int)(0.62f * size);
    uint8_t *b = malloc((size_t)cs * cs * 4);
    if (!b) return;
    if (variant < 0) {
        int g = (int)(0.7f * size);
        uint8_t *l = malloc((size_t)g * g * 4);
        if (l) {
            art_disc(g, gold, 0.6f, 1.5f, l);
            art_blend(rgba, size, size, l, g, g, (int)(0.34f * size) - g / 2, (int)(0.7f * size) - g / 2);
            free(l);
        }
        art_die_blend(rgba, size, size, 0.2f * size, 0.72f * size, (int)(0.22f * size), 0, 2, -12);
        art_die_blend(rgba, size, size, 0.46f * size, 0.74f * size, (int)(0.22f * size), 0, 1, 9);
        art_cup(ART_CUP_WALNUT, 2, cs, b);
        art_blend_rot(rgba, size, size, b, cs, cs, 0.68f * size, 0.38f * size, 26);
    } else {
        art_cup(ART_CUP_WALNUT, variant ? 3 : 2, cs, b);
        art_blend(rgba, size, size, b, cs, cs, (int)(0.19f * size), (int)(0.02f * size));
        int n = variant ? 6 : 3, c = (int)(0.2f * size);
        uint8_t *ch = malloc((size_t)c * c * 4);
        if (ch) {
            cup_chip_pic(c, ch);
            for (int i = 0; i < n; i++) {
                int row = i / 3, col = i % 3, x = (int)((0.5f + (col - 1) * 0.25f) * size) - c / 2;
                art_blend(rgba, size, size, ch, c, c, x, (int)((0.68f + row * 0.17f) * size) - c / 2);
            }
            free(ch);
        }
    }
    free(b);
}

static const game mexico_game = {
    .id = "mexico", .order = 158, .variants = 2,  /* three lives, six lives */
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .view = VIEW_HIDDEN,
};
REGISTER_GAME(mexico_game)
