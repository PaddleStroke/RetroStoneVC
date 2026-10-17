/*
 * The Spy in RetroStone Board: a talk game for 3 to 10 people passing the device round (the rules: spy_rules.c,
 * spy.h; the people and the hand-off: party.h). Each one in turn sees their card alone, behind the shell's cover:
 * the place, or the spy. Then the device goes in the middle of the table: the clock, the list of every place (the
 * spy's help, everyone's reference); the accusation (a chip tapped), the spy's guess (a place tapped); the round's
 * points. The places' names are the game's words (help/<lang>/spy_places.txt), the spy's card says what it is
 * (spy_words.txt).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/guessart.h"
#include "games/party.h"
#include "games/spy.h"
#include "shell.h"
#include "text.h"

#define SPY_CARD SPY_PLACES                     /* the spy's card in the game's deck; 0..59 the places */

static spy_state P;
static pt_seat S;
static int pick = -1;                           /* the chip or the place chosen, not confirmed yet */
static pt_timer clock_;                         /* the talk's clock */

static const uint8_t CREAM[3] = {246, 238, 220}, INKC[3] = {60, 40, 26};

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the spy: a hat pulled down, dark glasses, a raised collar (a unit square) */
static void paint_spy_in(uint8_t *rgba, int w, int h, float s, float ox, float oy)
{
    static const float coat[3] = {86, 78, 70}, skin[3] = {232, 192, 152}, hat[3] = {52, 46, 44},
                       band[3] = {150, 52, 40}, glass[3] = {30, 30, 34};
    ga_pen p;
    ga_begin(&p, rgba, w, h, s, ox, oy);
    float lw = fmaxf(0.8f, s * 0.012f);
    const float body[] = {0.2f, 0.74f, 0.8f, 0.74f, 0.92f, 1.0f, 0.08f, 1.0f};
    ga_poly(&p, ART_PART, body, 4);
    ga_paint(&p, coat, NULL, 0.25f, lw);
    ga_ell(&p, ART_PART, 0.5f, 0.52f, 0.17f, 0.2f);
    ga_paint(&p, skin, NULL, 0.2f, lw);
    const float c1[] = {0.5f, 0.86f, 0.24f, 0.74f, 0.3f, 0.6f, 0.42f, 0.72f};
    const float c2[] = {0.5f, 0.86f, 0.76f, 0.74f, 0.7f, 0.6f, 0.58f, 0.72f};
    ga_poly(&p, ART_PART, c1, 4);
    ga_poly(&p, ART_PART, c2, 4);
    ga_paint(&p, coat, NULL, 0.35f, lw);
    ga_ell(&p, ART_PART, 0.43f, 0.52f, 0.062f, 0.046f);   /* the glasses */
    ga_ell(&p, ART_PART, 0.57f, 0.52f, 0.062f, 0.046f);
    ga_rect(&p, ART_PART, 0.47f, 0.505f, 0.53f, 0.52f);
    ga_paint(&p, glass, NULL, 0.4f, 0);
    ga_ell(&p, ART_PART, 0.5f, 0.38f, 0.33f, 0.07f);      /* the hat: its brim, its crown, its band */
    ga_ell(&p, ART_PART, 0.5f, 0.24f, 0.16f, 0.07f);
    ga_rect(&p, ART_PART, 0.34f, 0.24f, 0.66f, 0.37f);
    ga_paint(&p, hat, NULL, 0.3f, lw);
    ga_rect(&p, ART_PART, 0.34f, 0.31f, 0.66f, 0.355f);
    ga_paint(&p, band, NULL, 0.2f, 0);
    ga_end(&p);
}

static void paint_spy(int kind, int size, uint8_t *rgba)
{
    (void)kind;
    memset(rgba, 0, (size_t)size * size * 4);
    paint_spy_in(rgba, size, size, size, 0, 0);
}

/* a place: a pin on a folded map */
static void paint_pin_in(uint8_t *rgba, int w, int h, float s, float ox, float oy)
{
    static const float map[3] = {222, 210, 170}, fold[3] = {196, 182, 140}, pin[3] = {184, 56, 44},
                       hole[3] = {248, 240, 222}, road[3] = {120, 156, 110};
    ga_pen p;
    ga_begin(&p, rgba, w, h, s, ox, oy);
    float lw = fmaxf(0.8f, s * 0.012f);
    const float m1[] = {0.06f, 0.72f, 0.34f, 0.64f, 0.34f, 0.96f, 0.06f, 1.0f};
    const float m2[] = {0.34f, 0.64f, 0.66f, 0.72f, 0.66f, 1.0f, 0.34f, 0.96f};
    const float m3[] = {0.66f, 0.72f, 0.94f, 0.64f, 0.94f, 0.96f, 0.66f, 1.0f};
    ga_poly(&p, ART_PART, m1, 4);
    ga_poly(&p, ART_PART, m3, 4);
    ga_paint(&p, map, NULL, 0.2f, lw);
    ga_poly(&p, ART_PART, m2, 4);
    ga_paint(&p, fold, NULL, 0.2f, lw);
    const float r1[] = {0.1f, 0.9f, 0.5f, 0.78f, 0.9f, 0.84f, 0.9f, 0.87f, 0.5f, 0.81f, 0.1f, 0.93f};
    ga_poly(&p, ART_PART, r1, 6);
    ga_paint(&p, road, NULL, 0, 0);
    const float tip[] = {0.36f, 0.4f, 0.64f, 0.4f, 0.5f, 0.82f};
    ga_poly(&p, ART_PART, tip, 3);
    ga_ell(&p, ART_PART, 0.5f, 0.32f, 0.19f, 0.19f);
    ga_paint(&p, pin, NULL, 0.35f, lw);
    ga_ell(&p, ART_PART, 0.5f, 0.32f, 0.075f, 0.075f);
    ga_paint(&p, hole, NULL, 0.1f, 0);
    ga_end(&p);
}

/* the game's cards: a place (its pin and its name) or the spy (and the words saying so) */
static void paint_card(int card, int w, int h, uint8_t *rgba, int words)
{
    pt_card(w, h, rgba);
    char s[96] = "";
    if (card == SPY_CARD) {
        paint_spy_in(rgba, w, h, w * 0.78f, w * 0.11f, h * 0.06f);
        if (!words || !text_word("spy_words", 0, s, sizeof s)) return;
        pt_text(rgba, w, h, s, w * 0.095f, 1, INKC, w / 2.0f, h * 0.84f, w * 0.86f);
    } else {
        paint_pin_in(rgba, w, h, w * 0.56f, w * 0.22f, h * 0.1f);
        if (!words || !text_word("spy_places", card, s, sizeof s)) return;
        pt_text(rgba, w, h, s, w * 0.12f, 1, INKC, w / 2.0f, h * 0.72f, w * 0.86f);
    }
}

static void paint_face(int card, int w, int h, uint8_t *rgba) { paint_card(card, w, h, rgba, 1); }

static int deck(void)
{
    static const cards_deck_def def = {1.4f, paint_face, NULL, NULL};
    static int d = -1;
    if (d < 0) d = cards_deck_add(&def);
    return d;
}

static int card_of(int person) { return person == P.spy ? SPY_CARD : P.place; }

/* ---- the screen -------------------------------------------------------------------------------------------------- */
static box Tb;
static pt_split L;                              /* the public screens */
static pt_look K;                               /* the card seen alone */
static float cd, cx[SPY_MAXP], cy[SPY_MAXP];    /* the chips to accuse */

/* the places, all in a grid (alphabetical in the language), painted once */
static struct {
    int cols, rows, order[SPY_PLACES], key;
    float x, y, cw, rh;
    char lang[16];
    SDL_Texture *t;
} G;

static int by_name(const void *a, const void *b)
{
    char s[96], t[96], fs[192], ft[192];
    if (!text_word("spy_places", *(const int *)a, s, sizeof s)) s[0] = 0;
    if (!text_word("spy_places", *(const int *)b, t, sizeof t)) t[0] = 0;
    text_fold(s, fs, sizeof fs);
    text_fold(t, ft, sizeof ft);
    return strcmp(fs, ft);
}

static void grid_make(void)
{
    box m = L.mid;
    int key = (int)m.w * 8192 + (int)m.h;
    if (pt_lang_changed(G.lang, sizeof G.lang)) G.key = -1;
    if (G.t && G.key == key) return;
    G.key = key;
    for (int i = 0; i < SPY_PLACES; i++) G.order[i] = i;
    qsort(G.order, SPY_PLACES, sizeof G.order[0], by_name);
    float pad = 0.15f * ui.unit;
    int cols = 2;                               /* the columns that let the names be biggest */
    for (int c = 2, best = 0; c <= 6; c++) {
        int rows = (SPY_PLACES + c - 1) / c;
        float px = fminf((m.h - 2 * pad) / rows * 0.5f, (m.w - 2 * pad) / c * 0.115f);
        if (px > best) { best = (int)px; cols = c; }
    }
    G.cols = cols;
    G.rows = (SPY_PLACES + cols - 1) / cols;
    G.cw = floorf((m.w - 2 * pad) / cols);
    G.rh = floorf((m.h - 2 * pad) / G.rows);
    G.x = floorf(m.x + (m.w - G.cw * cols) / 2);
    G.y = floorf(m.y + (m.h - G.rh * G.rows) / 2);
    int W = (int)(G.cw * cols), H = (int)(G.rh * G.rows);
    uint8_t *o = W > 0 && H > 0 ? calloc((size_t)W * H, 4) : NULL;
    if (!o) return;
    float px = fminf(G.rh * 0.5f, G.cw * 0.13f);
    for (int k = 0; k < SPY_PLACES; k++) {
        char s[96];
        if (!text_word("spy_places", G.order[k], s, sizeof s)) continue;
        int c = k / G.rows, r = k % G.rows;    /* down the columns: alphabetical reads down */
        pt_text(o, W, H, s, px, 0, CREAM, (c + 0.5f) * G.cw, (r + 0.5f) * G.rh, G.cw * 0.92f);
    }
    ui_free(&G.t);
    G.t = ui_tex(W, H, o);
    free(o);
}

static void grid_cell(int place, float *x, float *y)
{
    for (int k = 0; k < SPY_PLACES; k++)
        if (G.order[k] == place) {
            *x = G.x + (k / G.rows) * G.cw;
            *y = G.y + (k % G.rows) * G.rh;
            return;
        }
    *x = *y = -1e4f;
}

static int grid_hit(float x, float y)
{
    if (x < G.x || y < G.y) return -1;
    int c = (int)((x - G.x) / G.cw), r = (int)((y - G.y) / G.rh), k = c * G.rows + r;
    return c < G.cols && r < G.rows && k < SPY_PLACES ? G.order[k] : -1;
}

static void layout(box b)
{
    Tb = b;
    pt_split_layout(&L, b);
    pt_look_layout(&K, b, cards_aspect(deck()));
    cd = pt_grid(L.mid, P.n, 1.7f * ui.unit, cx, cy);
    G.key = -1;
}

static int talk_left(void)                      /* seconds */
{
    int all = spy_minutes(P.n) * 60;
    return all - (clock_.on ? clock_.ms / 1000 : 0);
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
static int holder(void) { return P.phase == SPY_LOOK ? P.looker : PT_ALL; }

static void sync(void)
{
    int h = holder();
    if (P.phase != SPY_OVER && h != S.who[S.slot]) pt_pass(&S, h);
    if (P.phase != SPY_TALK) pt_timer_reset(&clock_);
    if (P.phase == SPY_OVER) ui_tokens_changed();
    pick = -1;
}

static void start(int v)
{
    spy_new(&P, SPY_MINP + (v < 0 ? 0 : v), (uint32_t)ui_random(1 << 30));
    pt_seat_reset(&S, holder());
    pick = -1;
    pt_timer_reset(&clock_);
    K.key = -1;
    if (Tb.w > 0) layout(Tb);
}

static int script(const char *s)
{
    int ok = spy_script(&P, s);
    sync();
    K.key = -1;
    return ok;
}

static int undo(void) { return 0; }
static int turn(void) { return S.slot; }
static int status(void) { return P.phase == SPY_OVER ? GS_WIN0 : GS_PLAY; }
static int busy(void) { return pt_look_busy(&K); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void spy_badge(float x, float y, float d)
{
    ui_put_c(ct_disc((const uint8_t[3]){30, 30, 34}, 100, (int)d), x, y, d, d);
    ui_put_c(ct_picture(paint_spy, 0, (int)(d * 0.84f)), x, y - d * 0.04f, d * 0.84f, d * 0.84f);
}

static void draw_grid(int lit)
{
    float u = ui.unit;
    grid_make();
    ct_panel(L.mid.x + 0.05f * u, L.mid.y + 0.05f * u, L.mid.w - 0.1f * u, L.mid.h - 0.1f * u, lit ? 220 : 170);
    if (pick >= 0 && lit) {
        float x, y;
        grid_cell(pick, &x, &y);
        ui_fill(x + 2, y + 1, G.cw - 4, G.rh - 2, 234, 186, 66, 230);
    }
    if (G.t) ui_put(G.t, G.x, G.y, G.cw * G.cols, G.rh * G.rows);
}

static void draw_clock(void)
{
    box t = L.top;
    float u = ui.unit, h = fminf(1.2f * u, t.h * 0.42f), y = t.y + t.h * 0.42f;
    int left = talk_left();
    pt_clock(t.x + t.w / 2, y, h, left, left <= 30 ? (const uint8_t[3]){222, 96, 70} : pt_ink());
    float bw = fminf(t.w * 0.8f, 6 * u);
    pt_timebar(t.x + (t.w - bw) / 2, y + 0.85f * h, bw, fmaxf(3, 0.1f * u), left / (spy_minutes(P.n) * 60.0f));
    float d = fminf(1.1f * u, t.h * 0.42f), x = L.wide ? t.x + t.w / 2 : t.x + 0.9f * u, fy = L.wide ? y + 1.6f * h : y;
    ui_mark_gold(x, fy, d * 1.5f);              /* the first to ask */
    pt_chip_at(P.first, x, fy, d, 1);
}

/* the points: each chip, its total under it, the round's points beside it */
static void draw_scores(box r, int with_gain)
{
    float x[SPY_MAXP], y[SPY_MAXP];
    float d = pt_grid(r, P.n, 1.45f * ui.unit, x, y);
    unsigned won = P.phase == SPY_OVER ? spy_winners(&P) : 0;
    for (int i = 0; i < P.n; i++) {
        float c = y[i] - 0.22f * d;
        if (won >> i & 1) ui_mark_gold(x[i], c, d * 1.5f);
        pt_chip_at(i, x[i], c, d * 0.8f, 1);
        if (i == P.spy && P.phase != SPY_OVER) spy_badge(x[i] + d * 0.38f, c - d * 0.3f, d * 0.42f);
        ui_number(x[i], c + d * 0.62f, d * 0.36f, P.score[i], 1);
        if (with_gain && P.gain[i]) ct_plus(x[i] + d * 0.62f, c + d * 0.62f, d * 0.26f, P.gain[i]);
    }
}

/* the round's result: the place's card, the spy, the one accused wrongly or the spy's guess; the points */
static void draw_result(void)
{
    box t = L.top;
    float u = ui.unit, a = cards_aspect(deck());
    float cw = floorf(fminf(t.h * 0.9f / a, 1.6f * u)), d = fminf(1.1f * u, t.h * 0.5f);
    float y = L.wide ? t.y + t.h * 0.3f : t.y + t.h / 2;
    float x0 = L.wide ? t.x + t.w * 0.25f : t.x + t.w / 2 - 2.6f * u;
    cards_draw(deck(), P.place, x0, y, cw, 0, 0);
    float sx = x0 + cw / 2 + 1.0f * u;
    pt_chip_at(P.spy, sx, y, d, 1);
    spy_badge(sx + d * 0.42f, y - d * 0.4f, d * 0.5f);
    float gx = sx + 1.6f * u;
    if (L.wide) {
        gx = x0;
        y += cw * a / 2 + 0.9f * u;
    }
    if (P.accused >= 0 && P.accused != P.spy) {
        pt_chip_at(P.accused, gx, y, d, 1);
        ct_badge_no(gx + d * 0.42f, y - d * 0.4f, d * 0.5f);
    } else if (P.guess >= 0) {
        float gw = floorf(cw * 0.8f);
        cards_draw(deck(), P.guess, gx + gw * 0.2f, y, gw, 0, 0);
        if (P.guess == P.place) ct_badge_yes(gx + gw * 0.7f, y - gw * 0.5f, d * 0.6f);
        else ct_badge_no(gx + gw * 0.7f, y - gw * 0.5f, d * 0.6f);
    }
    ct_panel(L.mid.x + 0.05f * u, L.mid.y + 0.05f * u, L.mid.w - 0.1f * u, L.mid.h - 0.1f * u, 200);
    draw_scores(L.mid, 1);
}

static void draw(void)
{
    if (P.phase == SPY_OVER) {
        if (ui_over()) {
            float u = ui.unit;
            box r = ui_end_room(fminf(Tb.w, 7.5f * u), fminf(Tb.h * 0.6f, (P.n > 5 ? 4.6f : 2.6f) * u));
            if (r.w > 0) draw_scores(r, 0);
        } else draw_scores(L.mid, 0);
        return;
    }
    float mx = L.bx[1];
    switch (P.phase) {
    case SPY_LOOK:
        pt_look_draw(&K, deck(), card_of(P.looker), P.round * 16 + P.looker,
                     ui.viewer >= 0 && S.who[ui.viewer] == P.looker);
        break;
    case SPY_TALK:
        if (ui.viewer >= 0) {              /* (not under the cover) */
            pt_timer_tick(&clock_);
            if (talk_left() <= 0) {             /* time's up: the accusation */
                spy_call(&P);
                ui_sound(SND_CHECK);
                sync();
                return;
            }
        }
        draw_clock();
        draw_grid(0);
        ui_button(L.bx[0], L.by, L.bd, ICON_PERSON, UI_LIT);
        ui_button(L.bx[2], L.by, L.bd, ICON_SEARCH, 0);
        break;
    case SPY_VOTE: {
        box t = L.top;
        float d = fminf(1.3f * ui.unit, t.h * 0.6f);
        ui_put_c(pt_icon(ICON_PERSON, (int)d), t.x + t.w / 2, t.y + t.h / 2, d, d);
        for (int i = 0; i < P.n; i++) {
            if (i == pick) ui_mark_gold(cx[i], cy[i], cd * 1.55f);
            pt_chip_at(i, cx[i], cy[i], cd, 1);
        }
        ui_button(mx, L.by, L.bd, ICON_TICK, pick >= 0 ? UI_LIT : UI_DIM);
        break;
    }
    case SPY_GUESS: {
        box t = L.top;
        float d = fminf(1.3f * ui.unit, t.h * 0.6f), x = t.x + t.w / 2, y = t.y + t.h / 2;
        pt_chip_at(P.spy, x - 0.75f * d, y, d, 1);
        ui_put_c(ct_picture(paint_spy, 0, (int)d), x + 0.75f * d, y, d, d);
        draw_grid(1);
        ui_button(mx, L.by, L.bd, ICON_TICK, pick >= 0 ? UI_LIT : UI_DIM);
        break;
    }
    case SPY_SCORE:
        draw_result();
        ui_button(mx, L.by, L.bd, ICON_ARROW, UI_LIT);
        break;
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    float mx = L.bx[1];
    switch (P.phase) {
    case SPY_LOOK:
        if (!pt_look_hit(&K, x, y)) return;
        if (busy()) { ui_sound(SND_DENY); return; }
        spy_looked(&P);
        ui_sound(SND_TAP);
        sync();
        return;
    case SPY_TALK:
        if (ui_button_hit(L.bx[0], L.by, L.bd, x, y)) { spy_call(&P); ui_sound(SND_TAP); sync(); }
        else if (ui_button_hit(L.bx[2], L.by, L.bd, x, y)) { spy_stop(&P); ui_sound(SND_TAP); sync(); }
        return;
    case SPY_VOTE: {
        int c = pt_grid_hit(P.n, cx, cy, cd, x, y);
        if (c >= 0) { pick = c; ui_sound(SND_TAP); return; }
        if (!ui_button_hit(mx, L.by, L.bd, x, y)) return;
        if (pick < 0) { ui_sound(SND_DENY); return; }
        spy_accuse(&P, pick);
        ui_sound(P.phase == SPY_GUESS ? SND_CAPTURE : SND_MOVE);
        sync();
        return;
    }
    case SPY_GUESS: {
        int g = grid_hit(x, y);
        if (g >= 0) { pick = g; ui_sound(SND_TAP); return; }
        if (!ui_button_hit(mx, L.by, L.bd, x, y)) return;
        if (pick < 0) { ui_sound(SND_DENY); return; }
        spy_name(&P, pick);
        ui_sound(P.guess == P.place ? SND_CAPTURE : SND_MOVE);
        sync();
        return;
    }
    case SPY_SCORE:
        if (!ui_button_hit(mx, L.by, L.bd, x, y)) return;
        spy_next(&P);
        ui_sound(SND_TAP);
        sync();
        return;
    }
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the holder's bar: nothing more; the other's: the rounds (those played gold, this one in gold light) */
static void bar(int player, box b)
{
    if (player == S.slot) return;
    float d = fminf(0.36f * ui.unit, b.h * 0.4f), step = d * 1.6f, x = b.x + d;
    static const uint8_t gold[3] = {234, 186, 66}, dim[3] = {200, 190, 170};
    for (int i = 0; i < SPY_ROUNDS; i++) {
        int done = i < P.round || (i == P.round && P.phase >= SPY_SCORE);
        SDL_Texture *t = ct_disc(done || i == P.round ? gold : dim, done ? 100 : i == P.round ? 60 : 45, (int)d);
        ui_put_c(t, x + i * step, b.y + b.h / 2, d, d);
    }
}

static void token(int player, int size, uint8_t *rgba)
{
    if (P.phase == SPY_OVER) {                  /* the end card: the winner (a tie: everyone) */
        if (player) return;
        unsigned w = spy_winners(&P);
        if (w && !(w & (w - 1))) pt_chip(__builtin_ctz(w), size, rgba);
        else pt_group(size, rgba);
        return;
    }
    int who = S.who[player & 1];
    if (who >= 0) pt_chip(who, size, rgba);
    else if (who == PT_ALL) pt_group(size, rgba);
}

/* the menu's card: the spy's card over two places' (no words); the picker's: the count of people */
static void thumb(int v, int size, uint8_t *rgba)
{
    if (v >= 0) { pt_count_thumb(SPY_MINP + v, size, rgba); return; }
    memset(rgba, 0, (size_t)size * size * 4);
    int w = (int)(size * 0.4f), h = (int)(w * 1.4f);
    uint8_t *c = malloc((size_t)w * h * 4);
    if (!c) return;
    paint_card(0, w, h, c, 0);
    art_blend_rot(rgba, size, size, c, w, h, size * 0.31f, size * 0.47f, -14);
    art_blend_rot(rgba, size, size, c, w, h, size * 0.69f, size * 0.47f, 14);
    paint_card(SPY_CARD, w, h, c, 0);
    art_blend_rot(rgba, size, size, c, w, h, size * 0.5f, size * 0.53f, 0);
    free(c);
}

static const game spy_game = {
    .id = "spy", .order = 390, .variants = SPY_VARIANTS, .variant_first = 2,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .view = VIEW_HIDDEN,
};
REGISTER_GAME(spy_game)
