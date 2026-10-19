/*
 * Falling Notes in RetroStone Board, for one player: Play by Ear's piano (games/pianokit.h: the walnut case, the ivory
 * and ebony keys, the piano's voice) under a dark lacquered board with a lane above each key. A song's notes fall down
 * the lanes, each a bar as long as its note (amber over a white key, red lacquer over a black one); the player presses
 * the key (or its lane, near the gold line) as the bar's foot reaches the gold line above the keys, and holds a long
 * bar down to its end. A press in time plays the note: perfect (a gold star rising), good (a ring); a note left to
 * pass goes grey and stays silent (a red glow at the line), a press for no note sounds dull, the key flashing red. The
 * combo shows in gold digits at the top, the song's progress in a thin gold line. The end: a wave of gold over the
 * keys, the song's title (Play by Ear's titles), the shell's stars card (score: the accuracy, 0..100).
 * Levels: 60 songs in six chapters (pianofall_levels.h, tools/pianofall_gen.py: public-domain tunes only); the die tab: a
 * melody made at random (easy, medium, hard). Tools: practice (the song slower, no stars: it ends without a card) and
 * play again from the start. Each finger its own key (chords).
 * The timing: the song's clock is the shell's (ui_ms(): the same in every frame and press, the picture mode's still
 * one included), paused while the game is not drawn (another screen, the rules) and turned into the song's time (0.6
 * of it in practice). A press is judged at that time less the calibration: the players' mean lateness (a phone's
 * touch and screen delays), learned a little after each song (pfl_calibrate) and kept in the save store.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/pianofall.h"
#include "games/pianokit.h"
#include "games/puzzles.h"
#include "shell.h"
#include "text.h"

enum { T_PRACTICE, T_RESTART, TOOLS };

#define LEAD_MS 2000                            /* a note falls from the board's top to the line this long (real ms) */
#define OPEN_MS 700                             /* the board empty this long before the first note shows */
#define PRACTICE 0.6f                           /* practice: the song's speed */
#define END_MS 900                              /* the song over: this long after its last note */
#define PAUSE_MS 250                            /* frames further apart than this: the game was away (paused) */
#define FX_MS 480                               /* a judgement's light */
#define FLASH_MS 380
#define WRONG_MS 480
#define PRESS_IN 40
#define PRESS_OUT 140
#define WAVE_STEP 45
#define WAVE_LEN 520
#define SAVE_KEY "pianofall/offset"

static pfl_game G;
static int practice, offset;                    /* practice mode; the calibration (real ms, + presses late) */

/* ---- the clock --------------------------------------------------------------------------------------------------- */
/* anchor: the clock (ui_ms) at the song's time base; last: the clock at the last frame drawn, real: the real time then
 * (ui_real_ms: a gap is real time away, even in the picture mode, whose clock jumps) */
static struct { Uint32 anchor, last, real; float base; int done; } clk;

static float speed(void) { return practice ? PRACTICE : 1.0f; }
static float song_at(Uint32 now) { return clk.base + (float)(int)(now - clk.anchor) * speed(); }
static int judged_at(Uint32 now) { return (int)floorf(song_at(now) - offset * speed()); }
/* away: a long real-time gap since the last frame drawn; or the clock leaping far on (a kept game played again: its
 * clock runs fast, lands 10 minutes on, then jumps back to the real clock; the picture mode's long jumps) */
#define LEAP_MS 300000
static int away(Uint32 now) { return clk.last && (ui_real_ms() - clk.real > PAUSE_MS || now - clk.last > LEAP_MS); }
/* the clock between frames: the game away, the time of the last one drawn (draw() then moves the song past it) */
static Uint32 clock_now(void)
{
    Uint32 now = ui_ms();
    return away(now) ? clk.last : now;
}

/* ---- what shows: the judgements' lights, the keys played, the fingers ---------------------------------------------- */
#define FX 32
enum { FX_PERFECT, FX_GOOD, FX_MISS, FX_BROKEN };
static struct { int pitch, kind; Uint32 t0; } fx[FX];
static int nfx;
#define FLASHES 16
static struct { int pitch, wrong; Uint32 t0; } flash[FLASHES];
static int nflash;
#define FINGERS 10
static struct { int id, key, note, voice; const int16_t *s; } fing[FINGERS];
static int nfing;
static Uint32 combo_t;                          /* the last hit (the combo's digits swell) */
static struct { int on; Uint32 t0; } win;

static void add_fx(int pitch, int kind, Uint32 t0)
{
    if (nfx == FX) memmove(fx, fx + 1, (FX - 1) * sizeof fx[0]), nfx = FX - 1;
    fx[nfx].pitch = pitch;
    fx[nfx].kind = kind;
    fx[nfx].t0 = t0;
    nfx++;
}

static void flash_key(int pitch, int wrong, Uint32 t0)
{
    if (nflash == FLASHES) memmove(flash, flash + 1, (FLASHES - 1) * sizeof flash[0]), nflash = FLASHES - 1;
    flash[nflash].pitch = pitch;
    flash[nflash].wrong = wrong;
    flash[nflash].t0 = t0;
    nflash++;
}

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static box room;
static int W;                                   /* white keys */
static float kx, ky, kw, kh, bw, bh, lip, fr;   /* the keys' top left, a white key, a black one, the case's parts */
static float lx, ly, lw, lh, hy, bfr;           /* the lanes (the board's inside), the gold line's y, its frame */
static SDL_Texture *t_white[4], *t_black[4], *t_case, *t_board, *t_bar[2][2], *t_star, *t_ring, *t_note_small,
                   *t_badge, *t_plate;
static int bar_tw[2], bar_th[2], plate_w, plate_h;
static int made_k = -1, made_c = -1, made_b = -1;

static int white_index(int p)                   /* the white keys below p on the keyboard */
{
    int n = 0;
    for (int q = G.low; q < p; q++) n += !pfl_black(q);
    return n;
}

static void key_rect(int pitch, float *x, float *y, float *w, float *h)
{
    int wi = white_index(pitch);
    if (!pfl_black(pitch)) {
        *x = kx + wi * kw;
        *y = ky;
        *w = kw;
        *h = kh;
        return;
    }
    static const float off[12] = {0, -0.1f, 0, 0.1f, 0, 0, -0.12f, 0, 0, 0, 0.12f, 0};
    *x = kx + (wi + off[pitch % 12]) * kw - bw / 2;
    *y = ky;
    *w = bw;
    *h = bh;
}

/* a lane: its column (a note's bar is drawn a little narrower) */
static void lane(int pitch, float *x, float *w)
{
    float y, h;
    key_rect(pitch, x, &y, w, &h);
}

/* the key under a finger: on the keys (the black ones first), or in the lanes' lower part above them */
static int key_at(float x, float y)
{
    if (x < kx || x >= kx + W * kw || y < hy - 0.32f * lh || y > ky + kh + fr) return -1;
    float top = y >= ky ? bh : 1e9f;            /* in the lanes, a black lane is as tall as it goes */
    for (int p = G.low; p <= G.high; p++) {
        if (!pfl_black(p)) continue;
        float rx, rw;
        lane(p, &rx, &rw);
        if (x >= rx - 0.04f * kw && x < rx + rw + 0.04f * kw && (y < ky || y < ky + top)) return p;
    }
    for (int p = G.low; p <= G.high; p++) {
        if (pfl_black(p)) continue;
        float rx, rw;
        lane(p, &rx, &rw);
        if (x >= rx && x < rx + rw) return p;
    }
    return -1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static const uint8_t AMBER[3] = {226, 164, 58}, LACQUER[3] = {182, 52, 40}, LIT[3] = {255, 222, 130};

/* a note's bar: rounded, lit from the left, a darker rim; w x h */
static void paint_bar(int w, int h, const uint8_t c[3], uint8_t *o)
{
    float r = 0.32f * w;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f, cx = clampf(px, r, w - r), cy = clampf(py, r, h - r);
            float d = hypotf(px - cx, py - cy), a = clampf(r + 0.5f - d, 0, 1);
            float u = px / w, k = 0.78f + 0.42f * (1 - fabsf(u - 0.36f) * 1.6f);
            float edge = fminf(fminf(px, w - px), fminf(py, h - py));
            if (edge < 1.6f) k *= 0.7f;
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = pk_px8(c[i] * k + 3 * art_noise(x, y));
            p[3] = (uint8_t)(255 * a);
        }
}

/* the board: the walnut frame, inside it dark lacquer, a faint seam between the white keys' lanes, the black keys'
 * lanes a shade darker, lighter towards the gold line */
static void paint_board(int iw, int ih, int f, uint8_t *o)
{
    int w = iw + 2 * f;
    art_plank(iw, ih, f, o);
    float line = hy - ly;
    for (int y = 0; y < ih; y++)
        for (int x = 0; x < iw; x++) {
            uint8_t *p = o + 4 * ((y + f) * w + x + f);
            float t = clampf(y / fmaxf(1, line), 0, 1), k = 0.75f + 0.45f * t * t;
            int wi = (int)(x / kw), seam = x - wi * (int)kw < 1 && wi > 0;
            p[0] = pk_px8((24 + 2 * art_noise(x, y)) * k);
            p[1] = pk_px8(21 * k);
            p[2] = pk_px8(24 * k);
            if (seam) for (int i = 0; i < 3; i++) p[i] = pk_px8(p[i] + 16);
        }
    for (int q = G.low; q <= G.high; q++) {       /* the black lanes */
        if (!pfl_black(q)) continue;
        float rx, rw;
        lane(q, &rx, &rw);
        int x0 = (int)(rx - lx), x1 = (int)(rx - lx + rw);
        for (int y = 0; y < ih; y++)
            for (int x = x0 < 0 ? 0 : x0; x < x1 && x < iw; x++) {
                uint8_t *p = o + 4 * ((y + f) * w + x + f);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * 0.62f);
            }
    }
}

static void paint_icon_tinted(int icon, int d, const uint8_t rgb[3], uint8_t *o)
{
    art_icon(icon, d, o);
    for (int i = 0; i < d * d; i++) {
        o[4 * i] = rgb[0];
        o[4 * i + 1] = rgb[1];
        o[4 * i + 2] = rgb[2];
    }
}

static void make_textures(void)
{
    int w = (int)kw - 1, h = (int)kh, b = (int)bw, bhh = (int)bh, n = (int)fmaxf(4, 0.62f * ui.unit);
    int sig = w * 4099 + h * 131 + b + n * 1000003;   /* (the bar's note: the shell's unit, not the keys') */
    if (sig != made_k) {
        made_k = sig;
        pk_key_textures(w, h, b, bhh, t_white, t_black);
        for (int c = 0; c < 2; c++) {             /* the bars: 0 a white key's, 1 a black key's; lit */
            int bw2 = c ? (int)fmaxf(4, 0.86f * bw) : (int)fmaxf(4, 0.74f * kw), bh2 = 3 * bw2;
            bar_tw[c] = bw2;
            bar_th[c] = bh2;
            for (int l = 0; l < 2; l++) {
                ui_free(&t_bar[c][l]);
                paint_bar(bw2, bh2, l ? LIT : c ? LACQUER : AMBER, ui_buf(bw2, bh2));
                t_bar[c][l] = ui_tex(bw2, bh2, ui_buf(bw2, bh2));
            }
        }
        int d = (int)fmaxf(8, 0.9f * kw);
        ui_free(&t_star);
        paint_icon_tinted(ICON_STAR, d, PK_GOLD, ui_buf(d, d));
        t_star = ui_tex(d, d, ui_buf(d, d));
        ui_free(&t_ring);
        art_ring(d, 0.74f, PUZ_CREAM, 0.9f, ui_buf(d, d));
        t_ring = ui_tex(d, d, ui_buf(d, d));
        ui_free(&t_note_small);
        memset(ui_buf(n, n), 0, (size_t)n * n * 4);
        pk_paint_glyph(ui_buf(n, n), n, n, 0, 0, (float)n, PUZ_CREAM, (const uint8_t[3]){120, 110, 96});
        t_note_small = ui_tex(n, n, ui_buf(n, n));
        int bd = (int)fmaxf(8, 0.62f * ui.unit);
        ui_free(&t_badge);
        pk_paint_badge(bd, ui_buf(bd, bd));
        t_badge = ui_tex(bd, bd, ui_buf(bd, bd));
    }
    int iw = W * (int)kw, ih = (int)lip + h, f = (int)fr, csig = iw * 4099 + ih * 131 + f;
    if (csig != made_c) {
        made_c = csig;
        ui_free(&t_case);
        pk_paint_case(iw, ih, f, (int)lip, ui_buf(iw + 2 * f, ih + 2 * f));
        t_case = ui_tex(iw + 2 * f, ih + 2 * f, ui_buf(iw + 2 * f, ih + 2 * f));
    }
    int bi = (int)lw, bj = (int)lh, bf = (int)bfr, bsig = bi * 4099 + bj * 131 + bf + G.low * 7 + (int)hy;
    if (bsig != made_b) {
        made_b = bsig;
        ui_free(&t_board);
        paint_board(bi, bj, bf, ui_buf(bi + 2 * bf, bj + 2 * bf));
        t_board = ui_tex(bi + 2 * bf, bj + 2 * bf, ui_buf(bi + 2 * bf, bj + 2 * bf));
    }
}

static void layout(box b)
{
    room = b;
    W = pfl_whites(&G);
    if (W < 1) W = 8;
    float S = fminf(b.w, b.h), m = floorf(0.02f * S);
    int portrait = ui.w <= ui.h;
    fr = floorf(fmaxf(3, 0.02f * S));
    kw = floorf(fminf((b.w - 2 * m - 2 * fr) / W, 0.16f * fminf(ui.w, ui.h)));
    kh = floorf(fminf(portrait ? 4.4f * kw : 3.2f * kw, (portrait ? 0.24f : 0.3f) * b.h));
    lip = floorf(fmaxf(4, 0.26f * kw));
    bw = floorf(0.58f * kw);
    bh = floorf(0.62f * kh);
    float cw = W * kw + 2 * fr, ch = lip + kh + 2 * fr;
    float cx = floorf(b.x + (b.w - cw) / 2), cy = floorf(b.y + b.h - m - ch);
    kx = cx + fr;
    ky = cy + fr + lip;
    bfr = fr;                                   /* the board above, as wide as the case, its frame under the case's */
    lx = kx;
    lw = W * kw;
    ly = floorf(b.y + m + bfr);
    lh = cy + fr - ly;
    hy = floorf(cy - fmaxf(4, 0.28f * kw));     /* the gold line, a little above the case */
    ui_grid_fit((box){cx, ly - bfr, cw, cy + ch - ly + bfr}, 1, 1, 0);   /* the board for the shell's stars card */
    make_textures();
}

static void relayout(void)
{
    if (room.w > 0) layout(room);
}

/* ---- the song moving on ------------------------------------------------------------------------------------------ */
static void finger_off(int i)
{
    fing[i] = fing[--nfing];
}

/* the song's time reached: the notes it passed missed (a red glow), the song over at its end */
static void tick(Uint32 now)
{
    if (G.n <= 0 || clk.done) return;
    int t = judged_at(now), miss = G.miss;
    uint8_t was[PFL_MAX];
    memcpy(was, G.judge, (size_t)G.n);
    pfl_advance(&G, t);
    if (G.miss != miss)
        for (int i = 0; i < G.n; i++)
            if (was[i] == PFL_PENDING && G.judge[i] == PFL_MISS) add_fx(G.note[i].pitch, FX_MISS, now);
    if (pfl_over(&G) && song_at(now) >= pfl_end_ms(&G) + END_MS * speed()) {
        clk.done = 1;
        win.on = 1;
        win.t0 = now;
        nfing = 0;
        if (!practice) {                        /* the calibration learned from this song */
            int o = pfl_calibrate(&G, offset);
            if (o != offset) {
                offset = o;
                uint8_t v[2] = {(uint8_t)(o & 255), (uint8_t)((o >> 8) & 255)};
                ui_save_set(SAVE_KEY, v, 2);
            }
        }
    }
}

static void begin(void)
{
    uint8_t v[2];
    offset = ui_save_get(SAVE_KEY, v, 2) == 2 ? (int16_t)(v[0] | v[1] << 8) : 0;
    if (offset < -PFL_OFFSET_MAX || offset > PFL_OFFSET_MAX) offset = 0;
    pk_hush();
    nfx = nflash = nfing = 0;
    win.on = 0;
    combo_t = 0;
    memset(&clk, 0, sizeof clk);
    clk.anchor = ui_ms();
    clk.base = (G.n ? G.note[0].t : 0) - (LEAD_MS + OPEN_MS) * speed();
    made_b = -1;
    relayout();
    ui_animate();
}

/* the song again from its start (the same notes) */
static void again(void)
{
    pfl_restart(&G);
    begin();
}

static void start(int level)
{
    practice = 0;
    pfl_load(&G, level);
    begin();
}

static void start_random(int variant)
{
    practice = 0;
    pfl_random(&G, variant < 0 || variant >= PFL_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

/* ---- the keys pressed ---------------------------------------------------------------------------------------------- */
static void key_down(int id, int pitch, Uint32 now)
{
    tick(now);
    if (clk.done) { ui_sound(SND_DENY); return; }
    int t = judged_at(now), i = pfl_press(&G, pitch, t);
    if (nfing < FINGERS) {
        fing[nfing].id = id;
        fing[nfing].key = pitch;
        fing[nfing].note = i;
        fing[nfing].voice = -1;
        fing[nfing].s = NULL;
    }
    if (i < 0) {                                /* no note there: dull, the key red */
        const int16_t *s;
        pk_sound_on(pitch, 1, 0.5f, -1, &s);
        flash_key(pitch, 1, now);
    } else {
        int rest = (int)((G.note[i].t + G.note[i].len - t) / speed());
        const int16_t *s;
        int v = pk_sound_on(pitch, 0, 0.8f, rest < 120 ? 120 : rest + 40, &s);
        if (nfing < FINGERS) {
            fing[nfing].voice = v;
            fing[nfing].s = s;
        }
        add_fx(pitch, G.judge[i] == PFL_PERFECT_HIT ? FX_PERFECT : FX_GOOD, now);
        flash_key(pitch, 0, now);
        combo_t = now;
    }
    if (nfing < FINGERS) nfing++;
    ui_animate();
}

static void key_up(int id, Uint32 now)
{
    for (int k = 0; k < nfing; k++) {
        if (fing[k].id != id) continue;
        int i = fing[k].note;
        if (i >= 0 && G.hold[i] == PFL_HOLDING) {
            tick(now);
            pfl_release(&G, i, judged_at(now));
            if (G.hold[i] == PFL_BROKEN) {      /* let go too soon: its sound stops, the bar goes grey */
                pk_sound_off(fing[k].voice, fing[k].s);
                add_fx(G.note[i].pitch, FX_BROKEN, now);
            }
        }
        finger_off(k);
        ui_animate();
        return;
    }
}

static void tool(int t)
{
    if (t == T_PRACTICE) practice = !practice;
    again();
}

static void finger(int id, int phase, float x, float y)
{
    Uint32 now = clock_now();
    if (phase == FINGER_DOWN) {
        int t = ui_tool_hit(x, y);
        if (t >= 0) { tool(t); return; }
        int p = key_at(x, y);
        if (p >= 0) key_down(id, p, now);
    } else if (phase == FINGER_UP) {
        key_up(id, now);
    }
}

static void press(float x, float y) { finger(-1, FINGER_DOWN, x, y); }
static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { finger(-1, FINGER_UP, x, y); }

/* ---- drawing ------------------------------------------------------------------------------------------------------- */
/* a bar from y0 (its top) to y1 (its foot): its rounded ends kept, its middle stretched */
static void put_bar(int c, int lit, float x, float w, float y0, float y1, Uint8 alpha, Uint8 grey)
{
    SDL_Texture *t = t_bar[c][lit];
    int tw = bar_tw[c], th = bar_th[c];
    y0 = floorf(y0);                            /* whole pixels: the three parts meet without a seam */
    y1 = floorf(y1);
    float h = y1 - y0, s = w / tw, cap = fminf(floorf(tw * 0.34f), th / 2.0f), capd = floorf(cap * s);
    if (!t || h <= 0) return;
    SDL_SetTextureAlphaMod(t, alpha);
    if (grey) SDL_SetTextureColorMod(t, grey, grey, grey);
    if (h < 2 * capd) {
        SDL_Rect src = {0, 0, tw, th};
        SDL_FRect d = {x, y0, w, h};
        SDL_RenderCopyF(ui.r, t, &src, &d);
    } else {
        SDL_Rect a = {0, 0, tw, (int)cap}, m = {0, (int)cap, tw, th - 2 * (int)cap}, z = {0, th - (int)cap, tw, (int)cap};
        SDL_FRect da = {x, y0, w, capd}, dm = {x, y0 + capd, w, h - 2 * capd}, dz = {x, y1 - capd, w, capd};
        SDL_RenderCopyF(ui.r, t, &a, &da);
        SDL_RenderCopyF(ui.r, t, &m, &dm);
        SDL_RenderCopyF(ui.r, t, &z, &dz);
    }
    SDL_SetTextureAlphaMod(t, 255);
    SDL_SetTextureColorMod(t, 255, 255, 255);
}

static int held_key(int pitch)                  /* a finger on that key */
{
    for (int i = 0; i < nfing; i++)
        if (fing[i].key == pitch) return 1;
    return 0;
}

static void draw_notes(float now_song)
{
    float pps = (hy - ly) / (LEAD_MS * speed()), gap = fmaxf(4, floorf(0.16f * kw));
    SDL_Rect clip = {(int)lx, (int)ly, (int)lw, (int)(lh)};
    SDL_RenderSetClipRect(ui.r, &clip);
    for (int i = 0; i < G.n; i++) {
        const pfl_note *n = &G.note[i];
        /* a gap above each bar (a short note's smaller): notes on one key apart */
        float foot = hy - (n->t - now_song) * pps, top = foot - n->len * pps + fminf(gap, 0.3f * n->len * pps);
        if (top > ly + lh || foot < ly) continue;
        int c = pfl_black(n->pitch), j = G.judge[i], h = G.hold[i];
        float x, w;
        lane(n->pitch, &x, &w);
        float bwid = bar_tw[c];
        x = floorf(x + (w - bwid) / 2);
        if (j == PFL_PENDING) {
            put_bar(c, 0, x, bwid, top, foot, 255, 0);
        } else if (j == PFL_MISS || h == PFL_BROKEN) {
            put_bar(c, 0, x, bwid, top, h == PFL_BROKEN ? fminf(foot, hy) : foot, 150, 150);
        } else if (h == PFL_HOLDING || (h == PFL_HELD && now_song < n->t + n->len)) {
            if (top < hy) put_bar(c, 1, x, bwid, top, hy, 255, 0);   /* held: eaten at the line */
        }                                       /* (a tap hit: gone, its light at the line) */
    }
    SDL_RenderSetClipRect(ui.r, NULL);
}

static void draw_fx(Uint32 now)
{
    for (int i = 0; i < nfx; i++) {
        int dt = (int)(now - fx[i].t0);
        if (dt < 0 || dt >= FX_MS) continue;
        ui_animate();
        float f = (float)dt / FX_MS, x, w;
        lane(fx[i].pitch, &x, &w);
        float cx = x + w / 2, d = kw;
        switch (fx[i].kind) {
        case FX_PERFECT:                        /* a gold burst, a star rising from the line */
            ui_mark_gold_a(cx, hy, (1.4f + 1.2f * f) * d, 1 - f);
            SDL_SetTextureAlphaMod(t_star, (Uint8)(255 * (1 - f * f)));
            ui_put_c(t_star, cx, hy - (0.3f + 1.1f * f) * d, (0.62f + 0.2f * f) * d, (0.62f + 0.2f * f) * d);
            SDL_SetTextureAlphaMod(t_star, 255);
            break;
        case FX_GOOD:                           /* a ring opening */
            ui_mark_gold_a(cx, hy, 1.1f * d, 0.5f * (1 - f));
            SDL_SetTextureAlphaMod(t_ring, (Uint8)(220 * (1 - f)));
            ui_put_c(t_ring, cx, hy, (0.5f + 0.8f * f) * d, (0.5f + 0.8f * f) * d);
            SDL_SetTextureAlphaMod(t_ring, 255);
            break;
        default:                                /* missed, let go: a red glow at the line */
            ui_fill(x + 1, hy - 0.5f * d * (1 - f), w - 2, 0.5f * d * (1 - f), 210, 52, 40, (Uint8)(120 * (1 - f)));
            if (f < 0.4f) ui_mark_glow(cx, hy, 1.2f * d);
            break;
        }
    }
}

static float key_light(int pitch, Uint32 now, int *wrong)
{
    float a = 0;
    *wrong = 0;
    for (int i = 0; i < nflash; i++)
        if (flash[i].pitch == pitch) {
            Uint32 len = flash[i].wrong ? WRONG_MS : FLASH_MS, dt = now - flash[i].t0;
            if (dt >= len) continue;
            float f = 1 - (float)dt / len;
            if (flash[i].wrong) *wrong = (int)(255 * f);
            else a = fmaxf(a, f);
        }
    for (int i = 0; i < nfing; i++)
        if (fing[i].key == pitch && fing[i].note >= 0 && G.hold[fing[i].note] == PFL_HOLDING) a = 1;
    if (win.on && now >= win.t0) {
        float x, y, w, h;
        key_rect(pitch, &x, &y, &w, &h);
        a = fmaxf(a, puz_pulse((float)(now - win.t0), (x - kx) / kw * WAVE_STEP, WAVE_LEN));
    }
    return a;
}

static float key_depth(int pitch, Uint32 now)
{
    if (held_key(pitch)) return 1;
    float d = 0;
    for (int i = 0; i < nflash; i++)
        if (flash[i].pitch == pitch) {
            float dt = (float)(now - flash[i].t0), end = PRESS_IN + 60 + PRESS_OUT;
            if (dt < end) d = fmaxf(d, clampf(fminf(dt / PRESS_IN, (end - dt) / PRESS_OUT), 0, 1));
        }
    return d;
}

static void draw_keys(Uint32 now)
{
    ui_put(t_case, kx - fr, ky - lip - fr, W * kw + 2 * fr, lip + kh + 2 * fr);
    for (int pass = 0; pass < 2; pass++)
        for (int p = G.low; p <= G.high; p++) {
            if (pfl_black(p) != pass) continue;
            float x, y, w, h;
            int wrong;
            key_rect(p, &x, &y, &w, &h);
            float a = key_light(p, now, &wrong), dp = key_depth(p, now);
            if (a > 0 || wrong || (dp > 0 && dp < 1)) ui_animate();
            int s = a > 0.15f ? 2 : 0;
            if (pass) {                          /* its shadow on the ivory, shorter as it goes down */
                float k = 1 - 0.7f * dp;
                ui_fill(x + 0.12f * k * w, y, w, h + 0.015f * k * kh, 0, 0, 0, (Uint8)(38 - 14 * dp));
                ui_fill(x + 0.06f * k * w, y, w, h + 0.008f * k * kh, 0, 0, 0, (Uint8)(38 - 14 * dp));
                pk_put_key(t_black[s], t_black[s | 1], x, y, w, h, dp, kh);
            } else {
                pk_put_key(t_white[s], t_white[s | 1], x, y, w - 1, h, dp, kh);
            }
            y += floorf(dp * fmaxf(1, 0.014f * kh) + 0.5f);
            if (wrong) {
                ui_fill(x + 1, y, w - 2, h * (pass ? 0.92f : 0.95f), 210, 52, 40, (Uint8)(0.55f * wrong));
                ui_mark_glow(x + w / 2, y + 0.82f * h, 1.4f * w);
            }
            if (a > 0) ui_mark_gold_a(x + w / 2, y + (pass ? 0.75f : 0.8f) * h, (pass ? 1.9f : 1.7f) * w, a);
        }
}

/* the song's title over the board, once it is over: its title, its composer under it in gold */
static void draw_title(int accuracy)
{
    int tw = 0, th = 0, cw2 = 0, ch2 = 0;
    float px = fminf(0.5f * ui.unit, 0.07f * lw);
    SDL_Texture *t = G.tune >= 0 ? ui_word("melody_titles", 2 * G.tune, px, lw - 1.2f * px, PUZ_CREAM, &tw, &th) : NULL,
                *c = NULL;
    char who[96];
    if (G.tune >= 0 && text_word("melody_titles", 2 * G.tune + 1, who, sizeof who) && who[0])
        c = ui_word("melody_titles", 2 * G.tune + 1, 0.72f * px, lw - 1.2f * px, PK_GOLD, &cw2, &ch2);
    float dh = accuracy >= 0 ? floorf(1.3f * px) : 0, dw = accuracy >= 0 ? ui_number_w(accuracy, dh) : 0;
    if (!t && accuracy < 0) return;
    int w = (int)fmaxf(fmaxf(tw, cw2), dw) + (int)(1.4f * px), h = th + ch2 + (int)(0.5f * px) + (int)(dh * 1.3f);
    if (!t_plate || plate_w != w || plate_h != h) {
        static const uint8_t col[4] = {28, 34, 32, 236};
        ui_free(&t_plate);
        plate_w = w;
        plate_h = h;
        art_roundrect(w, h, 0.3f * px, col, ui_buf(w, h));
        t_plate = ui_tex(w, h, ui_buf(w, h));
    }
    float cx = lx + lw / 2, cy = ly + (accuracy >= 0 ? 0.42f : 0.13f) * (hy - ly), y = floorf(cy - h / 2.0f);
    ui_put(t_plate, floorf(cx - w / 2.0f), y, (float)w, (float)h);
    y += floorf(0.25f * px);
    if (t) ui_put(t, floorf(cx - tw / 2.0f), y, (float)tw, (float)th);
    y += th;
    if (c) ui_put(c, floorf(cx - cw2 / 2.0f), y, (float)cw2, (float)ch2);
    y += ch2;
    if (accuracy >= 0) ui_number_ex(cx, y + 0.65f * dh, dh, accuracy, PK_GOLD, 255, 0);
}

static void draw(void)
{
    if (!t_case) layout(room);
    Uint32 now = ui_ms();
    if (!clk.done) {                            /* away a while (another screen, the rules): the song waited */
        if (away(now)) clk.anchor += now - clk.last;
        clk.last = now;
        clk.real = ui_real_ms();
        ui_animate();
    }
    tick(now);
    float t = clk.done ? (float)pfl_end_ms(&G) + END_MS : song_at(now);
    int f = (int)bfr;
    ui_put(t_board, lx - f, ly - f, lw + 2 * f, lh + 2 * f);
    float end = (float)pfl_end_ms(&G), prog = end > 0 ? clampf(t / end, 0, 1) : 0;   /* the progress, along the top */
    ui_fill(lx, ly, lw * prog, fmaxf(2, floorf(0.05f * kw)), PK_GOLD[0], PK_GOLD[1], PK_GOLD[2], 170);
    draw_notes(t);
    float lt = fmaxf(2, floorf(0.07f * kw));    /* the gold line */
    ui_fill(lx, hy - lt / 2, lw, lt, 214, 170, 84, 255);
    ui_fill(lx, hy - lt / 2, lw, fmaxf(1, floorf(lt / 3)), 255, 226, 150, 255);
    draw_keys(now);
    draw_fx(now);
    if (G.combo >= 4 && !clk.done) {            /* the combo, swelling at each hit */
        float sw = clampf(1 - (float)(now - combo_t) / 220, 0, 1), dh = floorf(fminf(0.9f * ui.unit, 0.09f * lw) *
                                                                               (1 + 0.18f * sw));
        if (sw > 0) ui_animate();
        ui_number_ex(lx + lw / 2, ly + 0.14f * (hy - ly), dh, G.combo, PK_GOLD, 220, 0);
    }
    if (pk_silent()) {                          /* the sound off: a small speaker crossed out on the board */
        float d = 0.62f * ui.unit;
        ui_put_c(t_badge, lx + lw - 0.7f * d, ly + 0.9f * d, d, d);
    }
    if (win.on) {
        if (now - win.t0 < (Uint32)(W * WAVE_STEP + WAVE_LEN)) ui_animate();
        else win.on = 0;
    }
    if (clk.done && !win.on) draw_title(practice ? pfl_accuracy(&G) : -1);
    ui_tool(T_PRACTICE, ICON_HOURGLASS, practice ? UI_ON : 0);
    ui_tool(T_RESTART, ICON_REPLAY, clk.done && practice ? UI_LIT : 0);
}

/* ---- the shell's side ---------------------------------------------------------------------------------------------- */
static int status(void)
{
    tick(clock_now());
    return clk.done && !practice ? GS_WIN0 : GS_PLAY;
}

static int turn(void) { return 0; }
static int busy(void) { return win.on && ui_ms() - win.t0 < (Uint32)(W * WAVE_STEP + WAVE_LEN); }
static int level_stars(void) { return practice ? 0 : pfl_stars(pfl_accuracy(&G)); }
static int score(void) { return pfl_accuracy(&G); }

static void bar(int player, box b)              /* the notes played */
{
    (void)player;
    ui_bar_number(b, t_note_small, G.perfect + G.good);
}

/* "slow" (practice) first, then pfl_script's moves; the last moment's lights shown as they would be */
static int script(const char *moves)
{
    const char *s = moves;
    while (*s == ' ') s++;
    if (!strncmp(s, "slow", 4)) {
        practice = 1;
        s += 4;
        again();
    }
    uint8_t before[PFL_MAX];
    memcpy(before, G.judge, sizeof before);
    if (!pfl_script(&G, s)) return 0;
    Uint32 now = ui_ms();
    clk.anchor = now;
    clk.last = 0;
    clk.base = (float)G.now + offset * speed();
    for (int i = 0; i < G.n; i++) {             /* the judgements of the last moments: their lights */
        int ago = (int)((G.now - G.note[i].t) / speed());
        if (before[i] != PFL_PENDING || ago < 0 || ago >= FX_MS) continue;
        if (G.judge[i] == PFL_MISS) {
            if (ago >= PFL_GOOD_MS) add_fx(G.note[i].pitch, FX_MISS, now - (Uint32)(ago - PFL_GOOD_MS));
        } else {
            add_fx(G.note[i].pitch, G.judge[i] == PFL_PERFECT_HIT ? FX_PERFECT : FX_GOOD, now - (Uint32)ago);
            flash_key(G.note[i].pitch, 0, now - (Uint32)ago);
            combo_t = now - (Uint32)ago;
        }
    }
    for (int i = 0; i < G.n && nfing < FINGERS; i++)   /* the long notes held: their keys down */
        if (G.hold[i] == PFL_HOLDING) {
            fing[nfing].id = 1000 + i;
            fing[nfing].key = G.note[i].pitch;
            fing[nfing].note = i;
            fing[nfing].voice = -1;
            fing[nfing].s = NULL;
            nfing++;
        }
    if (pfl_over(&G) && G.now >= pfl_end_ms(&G)) {
        clk.base = (float)pfl_end_ms(&G) + END_MS * speed() + 1;
        clk.done = 1;
        win.on = 0;
    }
    return 1;
}

/* ---- the menu's card, the picker's, the chapters' tabs ------------------------------------------------------------- */
/* a small board of lanes over a keyboard, bars falling in it: (lane, top, foot) as parts of the board's height, the
 * lane counted in white keys (+0.5: the black key past that white one) */
#define KEYS_LEN 2.4f                          /* the little keyboard's keys: short, the board above it tall */
static void paint_scene(uint8_t *o, int size, int n, const float (*bars)[3], int nb, uint32_t lit)
{
    memset(o, 0, (size_t)size * size * 4);
    int w = (int)(0.92f * size), q = (w - 2) / n, f = q > 12 ? q / 6 : 2;
    int kbh = (int)(KEYS_LEN * q) + q / 2 + 1 + 2 * f, x0 = (size - w) / 2, ky0 = size - kbh - (int)(0.02f * size);
    int by0 = (int)(0.04f * size), bh0 = ky0 - by0 + f;
    static const uint8_t dark[3] = {26, 22, 24};
    art_roundrect_put(o, size, size, x0, by0, n * q + 2 * f, bh0, 0.06f * q, (const uint8_t[3]){92, 60, 40}, 1);
    art_roundrect_put(o, size, size, x0 + f, by0 + f, n * q, bh0 - f, 0, dark, 1);
    int line = ky0 - (int)(0.05f * size);
    for (int i = 0; i < nb; i++) {
        float ln = bars[i][0];
        int black = ln - floorf(ln) > 0.25f;
        int bw0 = black ? (int)(0.5f * q) : (int)(0.74f * q);
        int cx = x0 + f + (int)((black ? floorf(ln) + 1 : ln + 0.5f) * q);
        int y0 = by0 + f + (int)(bars[i][1] * (line - by0 - f)), y1 = by0 + f + (int)(bars[i][2] * (line - by0 - f));
        art_roundrect_put(o, size, size, cx - bw0 / 2, y0, bw0, y1 - y0, 0.3f * bw0, black ? LACQUER : AMBER, 1);
    }
    art_roundrect_put(o, size, size, x0 + f, line, n * q, (int)fmaxf(2, 0.025f * size), 0,
                      (const uint8_t[3]){214, 170, 84}, 1);
    pk_paint_keyboard(o, size, size, x0, ky0, w, n, KEYS_LEN, lit);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    if (variant < 0) {                          /* the menu: a few bars falling, one reaching the line (its key lit) */
        static const float b[][3] = {{1, 0.62f, 1.0f}, {3, 0.3f, 0.55f}, {4.5f, 0.06f, 0.26f}, {6, 0.0f, 0.16f},
                                     {5, 0.42f, 0.6f}};
        paint_scene(rgba, size, 7, b, 5, 1u << 2);
        return;
    }
    static const float e[][3] = {{2, 0.5f, 0.9f}, {4, 0.05f, 0.4f}};
    static const float m[][3] = {{1, 0.62f, 0.86f}, {3.5f, 0.3f, 0.5f}, {5, 0.02f, 0.22f}, {6, 0.66f, 0.9f},
                                 {2, 0.0f, 0.12f}};
    static const float h[][3] = {{0, 0.72f, 0.88f}, {2, 0.72f, 0.88f}, {1.5f, 0.46f, 0.58f}, {4, 0.22f, 0.34f},
                                 {6, 0.22f, 0.34f}, {5.5f, 0.0f, 0.1f}, {3, 0.52f, 0.62f}};
    if (variant == PFL_EASY) paint_scene(rgba, size, 7, e, 2, 0);
    else if (variant == PFL_MEDIUM) paint_scene(rgba, size, 7, m, 5, 0);
    else paint_scene(rgba, size, 7, h, 7, 0);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const float c1[][3] = {{3, 0.3f, 0.9f}};
    static const float c2[][3] = {{2, 0.5f, 0.9f}, {4, 0.05f, 0.4f}};
    static const float c3[][3] = {{3.5f, 0.45f, 0.9f}, {2, 0.05f, 0.35f}};
    static const float c4[][3] = {{1, 0.76f, 0.9f}, {2, 0.53f, 0.67f}, {3, 0.3f, 0.44f}, {4, 0.07f, 0.21f}};
    static const float c5[][3] = {{2, 0.5f, 0.9f}, {4, 0.5f, 0.9f}, {3, 0.05f, 0.35f}};
    static const float c6[][3] = {{1, 0.72f, 0.88f}, {3, 0.72f, 0.88f}, {1.5f, 0.45f, 0.58f}, {4, 0.25f, 0.38f},
                                  {6, 0.25f, 0.38f}, {4.5f, 0.0f, 0.12f}};
    static const float (*const all[6])[3] = {c1, c2, c3, c4, c5, c6};
    static const int nb[6] = {1, 2, 2, 4, 3, 6};
    int c = chapter < 0 ? 0 : chapter > 5 ? 5 : chapter;
    paint_scene(rgba, size, 7, all[c], nb[c], 0);
}

static void token(int player, int size, uint8_t *rgba)   /* an ivory disc, a note on it */
{
    (void)player;
    art_counter(COUNTER_DISC, 0, size, rgba);
    pk_paint_glyph(rgba, size, size, 0.2f * size, 0.18f * size, 0.62f * size, (const uint8_t[3]){34, 28, 26},
                   (const uint8_t[3]){12, 10, 10});
}

static const game pianofall_game = {
    .id = "pianofall", .order = 450, .variants = PFL_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = TOOLS,
    .levels = PFL_LEVEL_COUNT, .chapters = PFL_CHAPTER_COUNT, .start_random = start_random,
    .chapter_icon = chapter_icon, .level_stars = level_stars, .score = score, .finger = finger,
};
REGISTER_GAME(pianofall_game)
