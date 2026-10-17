/*
 * Draw and Guess in RetroStone Board: a drawing game for 2 to 8 people round one device (the rules: drawguess_rules.c,
 * drawguess.h; the people and the hand-off: party.h). The one drawing sees the word alone, behind the shell's cover,
 * then draws it with a finger on a sheet of paper while the others call out: six inks, an eraser, the sheet cleared
 * (the tools), undo a stroke (the shell's undo). A tap on a chip: that player guessed it. The words are the game's
 * own (help/<lang>/drawguess_words.txt).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/drawguess.h"
#include "games/guessart.h"
#include "games/party.h"
#include "shell.h"
#include "text.h"

#define INKS 6
enum { T_ERASER = INKS, T_CLEAR, TOOLS };
#define MAXPTS 30000
#define MAXSTROKES 1500
#define INK_CLEAR (-1)

static dg_state P;
static pt_seat S;
static pt_timer clock_;
static const uint8_t PAPER[3] = {250, 247, 238}, INKC[3] = {60, 40, 26};
static const uint8_t INK[INKS][3] = {{34, 30, 30}, {204, 52, 44}, {40, 96, 196}, {46, 150, 72}, {240, 186, 36},
                                     {130, 84, 46}};

/* ---- the sheet: the strokes (points as parts of its width and height), painted into a picture kept on the screen -- */
static struct { float x, y; } pt[MAXPTS];
typedef struct { int first, n, ink; } stroke;
static stroke st[MAXSTROKES];
static int npt, nst, open_stroke, ink;          /* open_stroke: a finger drawing the last stroke */

static box Tb, C;                               /* the table; the sheet on the screen */
static int cw, ch;                              /* its picture */
static uint8_t *cbuf;
static SDL_Texture *ctex;
static SDL_Rect dirty;
static int ddirty;

static float brush(int k) { return fmaxf(1.5f, (k == T_ERASER ? 0.032f : 0.0105f) * fminf(cw, ch)); }

static void mark_dirty(int x0, int y0, int x1, int y1)
{
    x0 = x0 < 0 ? 0 : x0;
    y0 = y0 < 0 ? 0 : y0;
    x1 = x1 > cw ? cw : x1;
    y1 = y1 > ch ? ch : y1;
    if (x1 <= x0 || y1 <= y0) return;
    if (!ddirty) dirty = (SDL_Rect){x0, y0, x1 - x0, y1 - y0};
    else {
        int ax = dirty.x < x0 ? dirty.x : x0, ay = dirty.y < y0 ? dirty.y : y0;
        int bx = dirty.x + dirty.w > x1 ? dirty.x + dirty.w : x1, by = dirty.y + dirty.h > y1 ? dirty.y + dirty.h : y1;
        dirty = (SDL_Rect){ax, ay, bx - ax, by - ay};
    }
    ddirty = 1;
}

/* a round dab of ink r pixels across its radius at (x, y), its edge smoothed */
static void dab(float x, float y, float r, const uint8_t *c)
{
    int x0 = (int)floorf(x - r - 1), x1 = (int)ceilf(x + r + 1), y0 = (int)floorf(y - r - 1), y1 = (int)ceilf(y + r + 1);
    for (int j = y0 < 0 ? 0 : y0; j < y1 && j < ch; j++)
        for (int i = x0 < 0 ? 0 : x0; i < x1 && i < cw; i++) {
            float d = hypotf(i + 0.5f - x, j + 0.5f - y), a = clampf(r - d + 0.5f, 0, 1);
            if (a <= 0) continue;
            uint8_t *p = cbuf + 4 * ((size_t)j * cw + i);
            for (int k = 0; k < 3; k++) p[k] = (uint8_t)(p[k] + (c[k] - p[k]) * a + 0.5f);
            p[3] = 255;
        }
    mark_dirty(x0, y0, x1, y1);
}

static void segment(int k, float ax, float ay, float bx, float by)
{
    float r = brush(k), x0 = ax * cw, y0 = ay * ch, x1 = bx * cw, y1 = by * ch;
    const uint8_t *c = k == T_ERASER ? PAPER : INK[k];
    int steps = (int)ceilf(hypotf(x1 - x0, y1 - y0) / fmaxf(0.8f, r * 0.3f));
    for (int i = 0; i <= steps; i++) {
        float t = steps ? (float)i / steps : 0;
        dab(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, r, c);
    }
}

static void stroke_paint(int s)
{
    if (st[s].ink == INK_CLEAR) return;
    int f = st[s].first;
    for (int i = 0; i < st[s].n; i++) {
        int a = f + (i ? i - 1 : 0), b = f + i;
        segment(st[s].ink, pt[a].x, pt[a].y, pt[b].x, pt[b].y);
    }
}

/* the sheet painted again from its strokes (since the last time it was cleared) */
static void sheet_paint(void)
{
    if (!cbuf) return;
    for (int i = 0; i < cw * ch; i++) memcpy(cbuf + 4 * i, (const uint8_t[4]){PAPER[0], PAPER[1], PAPER[2], 255}, 4);
    int from = 0;
    for (int s = 0; s < nst; s++)
        if (st[s].ink == INK_CLEAR) from = s + 1;
    for (int s = from; s < nst; s++) stroke_paint(s);
    mark_dirty(0, 0, cw, ch);
}

static void sheet_clear_all(void)
{
    npt = nst = open_stroke = 0;
    sheet_paint();
}

static void sheet_make(void)
{
    int w = (int)C.w, h = (int)C.h;
    if (w < 8 || h < 8) return;
    if (ctex && w == cw && h == ch) return;
    ui_free(&ctex);
    free(cbuf);
    cw = w;
    ch = h;
    cbuf = malloc((size_t)cw * ch * 4);
    ctex = SDL_CreateTexture(ui.r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, cw, ch);
    sheet_paint();
}

static int on_sheet(float x, float y) { return x >= C.x && y >= C.y && x < C.x + C.w && y < C.y + C.h; }

static void stroke_start(float x, float y)
{
    if (nst >= MAXSTROKES || npt >= MAXPTS) { ui_sound(SND_DENY); return; }
    st[nst] = (stroke){npt, 1, ink};
    pt[npt].x = clampf((x - C.x) / C.w, 0, 1);
    pt[npt].y = clampf((y - C.y) / C.h, 0, 1);
    npt++;
    stroke_paint(nst);
    nst++;
    open_stroke = 1;
    ui_animate();
}

static void stroke_to(float x, float y)
{
    if (!open_stroke || npt >= MAXPTS) return;
    float fx = clampf((x - C.x) / C.w, 0, 1), fy = clampf((y - C.y) / C.h, 0, 1);
    if (fabsf(fx - pt[npt - 1].x) * cw < 1.5f && fabsf(fy - pt[npt - 1].y) * ch < 1.5f) return;
    pt[npt].x = fx;
    pt[npt].y = fy;
    npt++;
    st[nst - 1].n++;
    segment(st[nst - 1].ink, pt[npt - 2].x, pt[npt - 2].y, fx, fy);
    ui_animate();
}

static void sheet_draw(float alpha)
{
    sheet_make();
    if (!ctex) return;
    if (ddirty) {
        SDL_UpdateTexture(ctex, &dirty, cbuf + 4 * ((size_t)dirty.y * cw + dirty.x), cw * 4);
        ddirty = 0;
    }
    float m = fmaxf(3, 0.12f * ui.unit);
    ui_fill(C.x - m + 4, C.y - m + 5, C.w + 2 * m, C.h + 2 * m, 0, 0, 0, 70);
    ui_fill(C.x - m, C.y - m, C.w + 2 * m, C.h + 2 * m, 112, 76, 46, 255);   /* a walnut frame */
    SDL_SetTextureAlphaMod(ctex, (Uint8)(255 * alpha));
    ui_put(ctex, C.x, C.y, C.w, C.h);
    SDL_SetTextureAlphaMod(ctex, 255);
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void paint_swatch(int k, int size, uint8_t *rgba)   /* an ink: a blob of it */
{
    memset(rgba, 0, (size_t)size * size * 4);
    art_disc(size, INK[k], 1, 0, rgba);
}

/* the word on its card (kept: the word, the size, the language) */
static SDL_Texture *word_card(int w, int h)
{
    static SDL_Texture *t;
    static int key[3] = {-1};
    static char lang[16];
    int k[3] = {P.word, w, h};
    if (pt_lang_changed(lang, sizeof lang)) key[0] = -1;
    if (t && !memcmp(k, key, sizeof k)) return t;
    memcpy(key, k, sizeof k);
    uint8_t *o = calloc((size_t)w * h, 4);
    if (!o) return NULL;
    pt_card(w, h, o);
    char s[96];
    if (text_word("drawguess_words", P.word, s, sizeof s))
        pt_text(o, w, h, s, h * 0.34f, 1, INKC, w / 2.0f, h / 2.0f, w * 0.86f);
    ui_free(&t);
    t = ui_tex(w, h, o);
    free(o);
    return t;
}

/* ---- the screen -------------------------------------------------------------------------------------------------- */
static struct {
    box top, side;                              /* the clock's strip; the chips' room */
    int wide;
    float bd, ax, ay;                           /* the round button (start, next) */
    float cd, cx[DG_MAXP], cy[DG_MAXP];         /* the chips of the players guessing (by player; the drawer none) */
} L;

static void chips_layout(void)
{
    int k = 0, who[DG_MAXP];
    for (int i = 0; i < P.n; i++)
        if (i != P.drawer) who[k++] = i;
    float x[DG_MAXP], y[DG_MAXP];
    L.cd = pt_grid(L.side, k, 1.3f * ui.unit, x, y);
    for (int i = 0; i < P.n; i++) L.cx[i] = L.cy[i] = -1e4f;
    for (int i = 0; i < k; i++) {
        L.cx[who[i]] = x[i];
        L.cy[who[i]] = y[i];
    }
}

static void layout(box b)
{
    float u = ui.unit, m = 0.25f * u;
    Tb = b;
    L.wide = b.w > b.h * 1.15f;
    L.bd = floorf(1.15f * u);
    if (L.wide) {
        float col = fminf(3.4f * u, b.w * 0.3f);
        L.top = (box){b.x, b.y, col, 1.4f * u};
        L.side = (box){b.x, b.y + 1.4f * u, col, b.h - 3.0f * u};
        L.ax = b.x + col / 2;
        L.ay = b.y + b.h - 0.9f * u;
        C = (box){b.x + col + m, b.y + m, b.w - col - 2 * m, b.h - 2 * m};
    } else {
        float th = 1.35f * u;                   /* one strip at the top: the drawer, the others, the clock */
        L.top = (box){b.x, b.y, b.w, th};
        L.side = (box){b.x + 2.3f * u, b.y, b.w - 3.6f * u, th - 0.2f * u};
        C = (box){b.x + m, b.y + th, b.w - 2 * m, b.h - th - m};
        L.ax = C.x + C.w - 0.9f * u;
        L.ay = C.y + C.h - 0.9f * u;
    }
    C.x = floorf(C.x);
    C.y = floorf(C.y);
    C.w = floorf(C.w);
    C.h = floorf(C.h);
    chips_layout();
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
static void sync(void)
{
    if (P.phase != DG_OVER && P.drawer != S.who[S.slot]) pt_pass(&S, P.drawer);
    if (P.phase == DG_OVER) ui_tokens_changed();
    if (P.phase == DG_LOOK) sheet_clear_all();
    if (P.phase != DG_DRAW) { pt_timer_reset(&clock_); open_stroke = 0; }
    chips_layout();
}

static void start(int v)
{
    dg_new(&P, DG_MINP + (v < 0 ? 0 : v), (uint32_t)ui_random(1 << 30));
    pt_seat_reset(&S, P.drawer);
    pt_timer_reset(&clock_);
    ink = 0;
    sheet_clear_all();
    if (Tb.w > 0) layout(Tb);
}

static int script(const char *s)
{
    int ok = dg_script(&P, s);
    sync();
    return ok;
}

/* undo: the last stroke (or the clearing of the sheet), while drawing */
static int undo(void)
{
    if (P.phase != DG_DRAW || !nst) return 0;
    nst--;
    npt = st[nst].first;
    open_stroke = 0;
    sheet_paint();
    return 1;
}

static int turn(void) { return S.slot; }
static int status(void) { return P.phase == DG_OVER ? GS_WIN0 : GS_PLAY; }
static int busy(void) { return 0; }

static int left_ms(void) { return DG_SECONDS * 1000 - (clock_.on ? clock_.ms : 0); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_chips(int lit)
{
    for (int i = 0; i < P.n; i++) {
        if (i == P.drawer) continue;
        if (i == P.guesser) ui_mark_gold(L.cx[i], L.cy[i], L.cd * 1.55f);
        pt_chip_at(i, L.cx[i], L.cy[i], L.cd, lit || i == P.guesser ? 1 : 0.55f);
        if (P.gain[i]) pt_plus(L.cx[i] + L.cd * 0.62f, L.cy[i] - L.cd * 0.42f, L.cd * 0.34f, P.gain[i]);
    }
}

static void draw_top(void)
{
    box t = L.top;
    float u = ui.unit, d = fminf(0.85f * u, t.h * 0.7f), y = t.y + t.h / 2;
    float x = t.x + 0.6f * u;
    if (!L.wide) y -= 0.1f * u;
    ui_put_c(pt_icon(ICON_PENCIL, (int)(d * 0.8f)), x + d * 0.9f, y, d * 0.8f, d * 0.8f);
    pt_chip_at(P.drawer, x, y, d, 1);           /* the one drawing */
    if (P.gain[P.drawer]) pt_plus(x + d * 0.62f, y - d * 0.42f, d * 0.4f, P.gain[P.drawer]);
    int ms = left_ms();
    if (P.phase == DG_DRAW) {
        float w = t.w - 0.4f * u;
        pt_timebar(t.x + 0.2f * u, t.y + t.h - 0.2f * u, w, fmaxf(3, 0.1f * u), ms / (DG_SECONDS * 1000.0f));
        ui_number_ex(t.x + t.w - 0.7f * u, y, 0.62f * u, (ms + 999) / 1000,
                     ms < 10000 ? (const uint8_t[3]){222, 96, 70} : pt_ink(), 255, 0);
    }
}

static void draw_tools(void)
{
    ui_tools_shown(P.phase == DG_DRAW ? TOOLS : 0);
    if (P.phase != DG_DRAW) return;
    for (int k = 0; k < INKS; k++) ui_tool_paint(k, paint_swatch, k, 0.6f, ink == k ? UI_ON : 0);
    ui_tool(T_ERASER, ICON_ERASER, ink == T_ERASER ? UI_ON : 0);
    ui_tool(T_CLEAR, ICON_CLOSE, nst ? 0 : UI_DIM);
}

static void draw_scores(box r)                  /* the end: each chip, its total */
{
    float x[DG_MAXP], y[DG_MAXP];
    float d = pt_grid(r, P.n, 1.45f * ui.unit, x, y);
    unsigned won = dg_winners(&P);
    for (int i = 0; i < P.n; i++) {
        float c = y[i] - 0.2f * d;
        if (won >> i & 1) ui_mark_gold(x[i], c, d * 1.5f);
        pt_chip_at(i, x[i], c, d * 0.8f, 1);
        ui_number(x[i], c + d * 0.62f, d * 0.36f, P.score[i], 1);
    }
}

static void draw(void)
{
    draw_tools();
    if (P.phase == DG_OVER) {
        sheet_draw(0.5f);
        float u = ui.unit;
        if (ui_over()) {
            box r = ui_end_room(fminf(Tb.w, 7.5f * u), fminf(Tb.h * 0.6f, (P.n > 4 ? 4.6f : 2.6f) * u));
            if (r.w > 0) draw_scores(r);
        } else draw_scores(C);
        return;
    }
    int mine = ui.viewer >= 0;
    if (P.phase == DG_DRAW) {
        pt_timer_tick(&clock_);
        if (left_ms() <= 0) {                   /* time's up: nobody found it */
            dg_timeout(&P);
            ui_sound(SND_CHECK);
            sync();
        }
    }
    sheet_draw(P.phase == DG_LOOK ? 0.6f : 1);
    draw_top();
    draw_chips(P.phase == DG_DRAW);
    float u = ui.unit;
    if (P.phase == DG_LOOK && mine) {           /* the word, seen alone */
        int w = (int)fminf(C.w * 0.86f, 7 * u), h = (int)(w * 0.45f);
        SDL_Texture *t = word_card(w, h);
        float x = C.x + C.w / 2, y = C.y + C.h * 0.42f;
        ui_shadow_flat(x, y, (float)w, (float)h, 0.8f);
        if (t) ui_put_c(t, x, y, (float)w, (float)h);
        ui_button(x, y + h / 2.0f + 1.0f * u, L.bd, ICON_PLAY, UI_LIT);
    } else if (P.phase == DG_SHOW) {            /* the word, for everyone; next */
        int w = (int)fminf(C.w * 0.7f, 5.5f * u), h = (int)(w * 0.4f);
        SDL_Texture *t = word_card(w, h);
        float x = C.x + C.w / 2, y = C.y + h * 0.5f + 0.3f * u;
        ui_shadow_flat(x, y, (float)w, (float)h, 0.8f);
        if (t) ui_put_c(t, x, y, (float)w, (float)h);
        ui_button(L.ax, L.ay, L.bd, ICON_ARROW, UI_LIT);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    switch (P.phase) {
    case DG_LOOK: {
        float u = ui.unit, w = fminf(C.w * 0.86f, 7 * u), h = floorf(w * 0.45f);
        if (!ui_button_hit(C.x + C.w / 2, C.y + C.h * 0.42f + h / 2 + 1.0f * u, L.bd, x, y)) return;
        dg_begin(&P);
        ui_sound(SND_TAP);
        sync();
        return;
    }
    case DG_DRAW: {
        int t = ui_tool_hit(x, y);
        if (t >= 0) {
            if (t == T_CLEAR) {
                if (!nst || nst >= MAXSTROKES) { ui_sound(SND_DENY); return; }
                st[nst++] = (stroke){npt, 0, INK_CLEAR};
                sheet_paint();
            } else ink = t;
            ui_sound(SND_TAP);
            return;
        }
        if (on_sheet(x, y)) { stroke_start(x, y); return; }
        for (int i = 0; i < P.n; i++)
            if (i != P.drawer && hypotf(x - L.cx[i], y - L.cy[i]) < L.cd * 0.6f) {
                dg_guessed(&P, i);
                ui_sound(SND_CAPTURE);
                sync();
                return;
            }
        return;
    }
    case DG_SHOW:
        if (!ui_button_hit(L.ax, L.ay, L.bd, x, y)) return;
        dg_next(&P);
        ui_sound(SND_TAP);
        sync();
        return;
    }
}

static void motion(float x, float y)
{
    if (P.phase == DG_DRAW) stroke_to(x, y);
}

static void release(float x, float y)
{
    if (P.phase == DG_DRAW && open_stroke) stroke_to(x, y);
    open_stroke = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the holder's bar: nothing more; the other's: everyone's points */
static void bar(int player, box b)
{
    if (player == S.slot) return;
    int s = (int)(0.6f * ui.unit);
    ui_count c[DG_MAXP];
    for (int i = 0; i < P.n; i++) c[i] = (ui_count){pt_chip_tex(i, s), P.score[i]};
    ui_bar_fit(b, c, P.n, 0);
}

static void token(int player, int size, uint8_t *rgba)
{
    if (P.phase == DG_OVER) {                   /* the end card: the winner (a tie: everyone) */
        if (player) return;
        unsigned w = dg_winners(&P);
        if (w && !(w & (w - 1))) pt_chip(__builtin_ctz(w), size, rgba);
        else pt_group(size, rgba);
        return;
    }
    int who = S.who[player & 1];
    if (who >= 0) pt_chip(who, size, rgba);
}

/* the menu's card: a drawing on a sheet (a house under the sun), a pencil; the picker's: the count of people */
static void thumb(int v, int size, uint8_t *rgba)
{
    if (v >= 0) { pt_count_thumb(DG_MINP + v, size, rgba); return; }
    memset(rgba, 0, (size_t)size * size * 4);
    static const float paper[3] = {250, 247, 238}, ink0[3] = {34, 30, 30}, red[3] = {204, 52, 44},
                       sun[3] = {240, 186, 36}, green[3] = {46, 150, 72}, wood[3] = {226, 170, 70},
                       lead[3] = {60, 56, 56}, pink[3] = {230, 140, 150};
    ga_pen p;
    ga_begin(&p, rgba, size, size, size, 0, 0);
    float lw = fmaxf(0.8f, size * 0.008f);
    ga_rect(&p, ART_PART, 0.1f, 0.14f, 0.9f, 0.86f);
    ga_paint(&p, paper, NULL, 0, lw * 1.5f);
    art_band(p.f, ART_PART, 0.24f, 0.56f, 0.24f, 0.78f, 0.022f);   /* the house, drawn with a felt pen */
    art_band(p.f, ART_PART, 0.56f, 0.56f, 0.56f, 0.78f, 0.022f);
    art_band(p.f, ART_PART, 0.22f, 0.78f, 0.58f, 0.78f, 0.022f);
    art_band(p.f, ART_PART, 0.4f, 0.66f, 0.4f, 0.78f, 0.022f);
    ga_grow(&p, 0.2f, 0.54f, 0.6f, 0.8f);
    ga_paint(&p, ink0, NULL, 0, 0);
    art_band(p.f, ART_PART, 0.2f, 0.58f, 0.4f, 0.4f, 0.024f);
    art_band(p.f, ART_PART, 0.4f, 0.4f, 0.6f, 0.58f, 0.024f);
    ga_grow(&p, 0.18f, 0.38f, 0.62f, 0.6f);
    ga_paint(&p, red, NULL, 0, 0);
    ga_ell(&p, ART_PART, 0.72f, 0.3f, 0.07f, 0.07f);
    ga_paint(&p, sun, NULL, 0, 0);
    art_band(p.f, ART_PART, 0.14f, 0.8f, 0.86f, 0.8f, 0.016f);
    ga_grow(&p, 0.14f, 0.78f, 0.86f, 0.82f);
    ga_paint(&p, green, NULL, 0, 0);
    art_band(p.f, ART_PART, 0.62f, 0.86f, 0.9f, 0.5f, 0.07f);      /* the pencil */
    ga_grow(&p, 0.58f, 0.46f, 0.94f, 0.9f);
    ga_paint(&p, wood, NULL, 0.3f, lw);
    art_band(p.f, ART_PART, 0.9f, 0.5f, 0.94f, 0.45f, 0.07f);
    ga_grow(&p, 0.86f, 0.4f, 0.98f, 0.54f);
    ga_paint(&p, pink, NULL, 0.2f, lw);
    const float tip[] = {0.59f, 0.84f, 0.65f, 0.885f, 0.56f, 0.92f};
    ga_poly(&p, ART_PART, tip, 3);
    ga_paint(&p, lead, NULL, 0, lw);
    ga_end(&p);
}

static const game drawguess_game = {
    .id = "drawguess", .order = 394, .variants = DG_VARIANTS, .variant_first = 2,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .view = VIEW_HIDDEN, .tools = TOOLS,
};
REGISTER_GAME(drawguess_game)
