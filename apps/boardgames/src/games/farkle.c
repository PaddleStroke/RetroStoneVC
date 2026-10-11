/*
 * Farkle (« 10 000 ») in RetroStone Board: a well of green felt between two wooden racks, on the table. The dice in
 * the player's colour tumble on the felt; a tap on a scoring die chooses it (it lifts, in a gold light); the round
 * buttons on the player's rack throw the dice left (a die in flight) or bank the turn (a stack of coins); the dice set
 * aside slide onto the rack (the rules and the notation: farkle_rules.c, farkle.h). One to six players.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * On the felt, under the dice: the points of the dice chosen (small, in gold light) over the turn's total so far with
 * them (big). A farkle: the dice in a red glow, the turn's lost points shown a moment. A bank: the points banked in
 * gold a moment. Face to face, the felt and the rack turn to the player to move when they sit at the top. Alone, a row
 * of twenty pegs counts the turns. The player who reached 10000 first: their die in gold light on the felt (the final
 * round). Coordinates: fractions of the board's side, from its top left, in the player to move's frame.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dicecup.h"
#include "games/farkle.h"
#include "shell.h"

#define IN0 (CUP_FR + 0.012f)
#define IN1 (1 - CUP_FR - 0.012f)
#define FELT0 0.2f                              /* the felt, between the racks */
#define FELT1 0.8f
#define DIE 0.12f
#define RACK_Y 0.877f                           /* the dice set aside, on the player's rack */
#define RACK_DIE 0.075f
#define BTN 0.12f
#define BANK_X 0.12f
#define THROW_X 0.88f
#define TOTAL_Y 0.72f
#define PAUSE_MS 1500                           /* a farkle shown */
#define BANKED_MS 1100                          /* a bank shown */

static fgame G;
static const fpos *P = &G.pos;
static int S, made = -1;
static float bx, by;
static SDL_Texture *t_board;
static unsigned sel;                            /* the dice chosen in the throw (bit i: die i) */
static struct { int on, key; Uint32 t0; } tumble;
static struct { int on, player, landed, nd, lost, key; uint8_t v[FK_DICE]; Uint32 t0; } dead;
static struct { int on, player, pts; Uint32 t0; } banked;
static struct { int on, n, first; float x[FK_DICE], y[FK_DICE]; Uint32 t0; } aside;   /* sliding onto the rack */

static int throw_key(int turns, int throws) { return turns * 16 + (throws < 15 ? throws : 15); }

/* ---- the frame: the player to move's (turned round face to face when they sit at the top) ----------------------- */
static int up_now(void) { return ui_upside(P->phase == FK_OVER ? 0 : P->turn); }
static float SX(float x, float y, int up) { (void)y; return bx + (up ? 1 - x : x) * S; }
static float SY(float x, float y, int up) { (void)x; return by + (up ? 1 - y : y) * S; }

/* die i of a throw of nd dice: where it rests (a grid of three by two, a little askew after throw `key`) */
static void die_spot(int i, int nd, int key, float *x, float *y, float *deg)
{
    int rows = nd > 3 ? 2 : 1, per = (nd + rows - 1) / rows, r = i / per, c = i % per;
    int in_row = r == rows - 1 ? nd - per * (rows - 1) : per;
    *x = 0.5f + (c - (in_row - 1) / 2.0f) * 0.2f;
    *y = (rows == 2 ? 0.38f + r * 0.17f : 0.46f);
    if (key >= 0) {
        *x += 0.03f * cup_jitter(key, i, 0);
        *y += 0.025f * cup_jitter(key, i, 1);
        *deg = cup_tilt(key, i);
    } else *deg = 0;
}

/* the k-th die set aside on the rack (a small gap between the throws they came from) */
static float rack_x(const fpos *p, int k)
{
    int gaps = 0, total = 0;
    for (int j = 1; j < p->naside; j++) total += p->group[j] != p->group[j - 1];
    for (int j = 1; j <= k && j < p->naside; j++) gaps += p->group[j] != p->group[j - 1];
    float step = RACK_DIE * 1.12f, gap = 0.025f, w = (p->naside - 1) * step + total * gap;
    return 0.5f - w / 2 + k * step + gaps * gap;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void reset_ui(void)
{
    sel = 0;
    tumble.on = 0;
    dead.on = 0;
    banked.on = 0;
    aside.on = 0;
}

static void start(int variant)
{
    (void)variant;
    fk_new(&G, ui.players);
    cup_mem_reset();
    reset_ui();
}

static int status(void)
{
    if (P->phase != FK_OVER) return GS_PLAY;
    if (P->players == 1) return P->winner == 0 ? GS_WIN0 : GS_WIN1;
    return P->winner >= 0 ? gs_win(P->winner) : GS_DRAW;
}

static unsigned winners(void)                   /* a tie for the highest score: they share the win */
{
    unsigned m = 0;
    if (P->phase != FK_OVER || P->players < 2) return 0;
    int w = fk_leader(P);
    for (int q = 0; q < P->players; q++)
        if (P->score[q] == P->score[w]) m |= 1u << q;
    return m;
}

static int turn(void) { return P->turn; }
static int rolling(void) { return tumble.on && ui_ms() - tumble.t0 < RACE_TUMBLE_MS; }
static int busy(void)
{
    return rolling() || dead.on || banked.on || (aside.on && ui_ms() - aside.t0 < 2 * CUP_SLIDE_MS);
}
static void home(int player, float *fx, float *fy) { cup_home(player, P->players, fx, fy); }

static void throw_dice(void)
{
    fpos before = *P;
    int n = fk_to_throw(P, sel), v[FK_DICE], key = throw_key(P->turns, P->throws), selv = fk_sel_value(P, sel);
    if (P->phase == FK_CHOOSE && !selv) { ui_sound(SND_DENY); return; }
    cup_roll(key, n, v);
    int r = fk_throw(&G, sel, v);
    if (!r) { ui_sound(SND_DENY); return; }
    aside.on = 0;
    if (before.phase == FK_CHOOSE && P->naside) {   /* the dice chosen slide onto the rack (not after hot dice) */
        aside.on = 1;
        aside.n = 0;
        aside.first = before.naside;
        aside.t0 = ui_ms();
        int bkey = throw_key(before.turns, before.throws - 1);
        for (int i = 0; i < before.nd; i++)
            if (sel >> i & 1) {
                float d;
                die_spot(i, before.nd, bkey, &aside.x[aside.n], &aside.y[aside.n], &d);
                aside.n++;
            }
    }
    sel = 0;
    tumble.on = 1;
    tumble.key = key;
    tumble.t0 = ui_ms();
    ui_sound(SND_DICE);
    if (r == FK_FARKLE) {
        dead.on = 1;
        dead.landed = 0;
        dead.player = before.turn;
        dead.nd = n;
        dead.key = key;
        dead.lost = before.phase == FK_CHOOSE ? before.turn_pts + selv : 0;
        for (int i = 0; i < n; i++) dead.v[i] = (uint8_t)v[i];
    }
}

static void bank(void)
{
    int me = P->turn, pts = P->turn_pts + fk_sel_value(P, sel);
    if (!fk_can_bank(P, sel)) { ui_sound(SND_DENY); return; }
    if (!fk_bank(&G, sel)) { ui_sound(SND_DENY); return; }
    sel = 0;
    aside.on = 0;
    banked.on = 1;
    banked.player = me;
    banked.pts = pts;
    banked.t0 = ui_ms();
    ui_sound(SND_PLACE);
}

static void tap_die(int i)
{
    unsigned b = 1u << i;
    if (sel & b) {
        sel &= ~b;
        ui_sound_soft(SND_TAP, 0.5f);
        return;
    }
    if (!(fk_scoring(P) & b)) { ui_sound(SND_DENY); return; }   /* this die cannot score in this throw */
    sel |= b;
    ui_sound(SND_PLACE);
}

static int undo(void)
{
    if (sel) { sel = 0; return 1; }
    if (!fk_undo(&G)) return 0;
    reset_ui();
    if (P->phase == FK_CHOOSE) {                /* the throw at rest again */
        tumble.on = 1;
        tumble.key = throw_key(P->turns, P->throws - 1);
        tumble.t0 = ui_ms() - RACE_TUMBLE_MS;
    }
    return 1;
}

/* "d115234 a115 d225 a5 b": throws, dice set aside, banks (farkle.h) */
static int script(const char *s)
{
    int ok = fk_script(&G, s);
    for (int i = 0; i < G.n; i++)               /* its throws are the game's from now on */
        if (G.ev[i].kind == FE_THROW) {
            const fpos *b = &G.past[i];
            int v[FK_DICE] = {0};
            for (int k = 0; k < G.ev[i].nd; k++) v[k] = G.ev[i].v[k];
            cup_remember(throw_key(b->turns, b->throws), G.ev[i].nd, v);
        }
    reset_ui();
    if (P->phase == FK_CHOOSE) {
        tumble.on = 1;
        tumble.key = throw_key(P->turns, P->throws - 1);
        tumble.t0 = ui_ms() - RACE_TUMBLE_MS;
    } else if (G.n && G.ev[G.n - 1].kind == FE_THROW) {   /* the last throw a farkle: its pause to come */
        const fpos *b = &G.past[G.n - 1];
        const fevent *e = &G.ev[G.n - 1];
        dead.on = 1;
        dead.landed = 0;
        dead.player = b->turn;
        dead.nd = e->nd;
        dead.key = throw_key(b->turns, b->throws);
        dead.lost = b->phase == FK_CHOOSE ? b->turn_pts + fk_sel_value(b, e->sel) : 0;
        memcpy(dead.v, e->v, sizeof dead.v);
        tumble.on = 1;
        tumble.key = dead.key;
        tumble.t0 = ui_ms() - RACE_TUMBLE_MS;
    }
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void paint_board(uint8_t *o, int n)
{
    cup_board(o, n);
    cup_well(o, n, IN0, FELT0, IN1, FELT1);
    static const float groove[3] = {0.82f, 0.8f, 0.78f};
    for (int s = 0; s < 2; s++) {               /* the racks' grooves, for the dice set aside */
        float y = s ? RACK_Y : 1 - RACK_Y;
        race_inlay(o, n, n, 0.2f * n, (y - 0.05f) * n, 0.6f * n, 0.1f * n, 0.03f * n, groove, 0.006f * n);
    }
}

static void layout(box b)
{
    S = (int)floorf(b.w);
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    if (made == S) return;
    made = S;
    uint8_t *o = malloc((size_t)S * S * 4);
    if (!o) return;
    paint_board(o, S);
    ui_free(&t_board);
    t_board = ui_tex(S, S, o);
    free(o);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void number_f(float x, float y, float h, int v, int up)
{
    if (up) ui_number_rot(SX(x, y, 1), SY(x, y, 1), h * S, v, 1, 180);
    else ui_number(SX(x, y, 0), SY(x, y, 0), h * S, v, 1);
}

static void draw_rack(const fpos *p, int me, int up, Uint32 now)
{
    for (int k = 0; k < p->naside; k++) {
        float x = rack_x(p, k), y = RACK_Y;
        if (aside.on && k >= aside.first && k - aside.first < aside.n) {   /* sliding from the felt */
            float f = (float)(now - aside.t0) / (2 * CUP_SLIDE_MS);
            if (f < 1) {
                x = cup_mix(aside.x[k - aside.first], x, f);
                y = cup_mix(aside.y[k - aside.first], y, f);
                ui_animate();
            }
        }
        cup_die(SX(x, y, up), SY(x, y, up), RACK_DIE * S, me, p->aside[k], up ? 180 : 0, 0, 1);
    }
}

static void draw_pegs(void)                     /* alone: the twenty turns */
{
    for (int k = 0; k < FK_SOLO_TURNS; k++) {
        float x = 0.5f + (k - (FK_SOLO_TURNS - 1) / 2.0f) * 0.036f, y = FELT0 + 0.04f;
        if (k == P->turns && P->phase != FK_OVER) ui_mark_gold(SX(x, y, 0), SY(x, y, 0), 0.06f * S);
        if (k < P->turns)
            ui_fill(SX(x, y, 0) - 0.006f * S, SY(x, y, 0) - 0.006f * S, 0.012f * S, 0.012f * S, 20, 50, 30, 200);
        else ui_mark_dot(SX(x, y, 0), SY(x, y, 0), 0.02f * S);
    }
}

static void draw(void)
{
    Uint32 now = ui_ms();
    ui_put(t_board, bx, by, (float)S, (float)S);
    if (P->players == 1) draw_pegs();
    else if (P->final_by >= 0) {                /* the final round: who reached the target, in gold light */
        int up = up_now();
        float x = 0.1f, y = FELT0 + 0.06f;
        ui_mark_gold(SX(x, y, up), SY(x, y, up), 0.1f * S);
        cup_die(SX(x, y, up), SY(x, y, up), 0.055f * S, P->final_by, 5, 0, 0, 1);
    }
    if (banked.on) {                            /* the points banked, a moment */
        if (now - banked.t0 >= BANKED_MS) banked.on = 0;
        else {
            int up = ui_upside(banked.player);
            ui_mark_gold(SX(0.5f, 0.5f, up), SY(0.5f, 0.5f, up), 0.3f * S);
            number_f(0.5f, 0.5f, 0.09f, banked.pts, up);
            ui_animate();
            return;
        }
    }
    if (dead.on) {                              /* a farkle: the dice in a red glow, the points lost */
        int up = ui_upside(dead.player), t = (int)(now - tumble.t0);
        if (!dead.landed && t >= RACE_TUMBLE_MS) {
            dead.landed = 1;
            dead.t0 = now;
            ui_sound(SND_DENY);
        }
        if (dead.landed && now - dead.t0 >= PAUSE_MS) dead.on = 0;
        else {
            for (int i = 0; i < dead.nd; i++) {
                float x, y, deg;
                die_spot(i, dead.nd, dead.key, &x, &y, &deg);
                if (dead.landed) ui_mark_glow(SX(x, y, up), SY(x, y, up), 1.9f * DIE * S);
                cup_die_thrown(SX(x, y, up), SY(x, y, up), DIE * S, dead.player, dead.v[i], deg + (up ? 180 : 0), i, t,
                               1);
            }
            if (dead.landed && dead.lost) {
                ui_mark_glow(SX(0.5f, TOTAL_Y, up), SY(0.5f, TOTAL_Y, up), 0.22f * S);
                number_f(0.5f, TOTAL_Y, 0.06f, dead.lost, up);
            }
            ui_animate();
            return;
        }
    }
    if (P->phase == FK_OVER) return;
    int me = P->turn, up = up_now();
    float bd = BTN * S;
    draw_rack(P, me, up, now);
    if (P->phase == FK_THROW) {                 /* the six dice waiting, the throw button lit */
        static const int idle[FK_DICE] = {5, 3, 6, 1, 4, 2};
        for (int i = 0; i < FK_DICE; i++) {
            float x, y, deg;
            die_spot(i, FK_DICE, -1, &x, &y, &deg);
            cup_die(SX(x, y, up), SY(x, y, up), DIE * S, me, idle[i], up ? 180 : 0, 0, 0.9f);
        }
        cup_button_at(CUP_THROW, SX(THROW_X, RACK_Y, up), SY(THROW_X, RACK_Y, up), bd, 1, up ? 180 : 0);
        cup_button_at(CUP_BANK, SX(BANK_X, RACK_Y, up), SY(BANK_X, RACK_Y, up), bd, 0, up ? 180 : 0);
        return;
    }
    int t = rolling() ? (int)(now - tumble.t0) : RACE_TUMBLE_MS, settled = t >= RACE_TUMBLE_MS;
    unsigned can = settled ? fk_scoring(P) : 0;
    for (int i = 0; i < P->nd; i++) {
        float x, y, deg;
        die_spot(i, P->nd, tumble.key, &x, &y, &deg);
        int chosen = sel >> i & 1;
        if (chosen) {
            ui_mark_gold(SX(x, y, up), SY(x, y, up), 1.8f * DIE * S);
            y -= 0.015f;
        }
        float a = settled && !chosen && !(can >> i & 1) ? 0.7f : 1;   /* a die that cannot score: darker */
        cup_die_thrown(SX(x, y, up), SY(x, y, up), DIE * S, me, P->dice[i], deg + (up ? 180 : 0), i, t, a);
    }
    if (!settled) return;
    int v = sel ? fk_sel_value(P, sel) : 0;
    if (v) {                                    /* the dice chosen: their points, in gold light */
        ui_mark_gold(SX(0.5f, TOTAL_Y - 0.075f, up), SY(0.5f, TOTAL_Y - 0.075f, up), 0.16f * S);
        number_f(0.5f, TOTAL_Y - 0.065f, 0.042f, v, up);
    }
    number_f(0.5f, TOTAL_Y + 0.005f, 0.07f, P->turn_pts + v, up);
    cup_button_at(CUP_THROW, SX(THROW_X, RACK_Y, up), SY(THROW_X, RACK_Y, up), bd, v > 0, up ? 180 : 0);
    cup_button_at(CUP_BANK, SX(BANK_X, RACK_Y, up), SY(BANK_X, RACK_Y, up), bd, fk_can_bank(P, sel), up ? 180 : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (dead.on) {                              /* a tap ends the pause */
        if (dead.landed) dead.on = 0;
        return;
    }
    if (banked.on) { banked.on = 0; }
    if (P->phase == FK_OVER || rolling()) return;
    int up = up_now();
    float u = (x - bx) / S, v = (y - by) / S;
    if (up) { u = 1 - u; v = 1 - v; }
    if (hypotf(u - THROW_X, v - RACK_Y) < 0.6f * BTN) { throw_dice(); return; }
    if (hypotf(u - BANK_X, v - RACK_Y) < 0.6f * BTN) {
        if (P->phase == FK_CHOOSE) bank();
        else ui_sound(SND_DENY);
        return;
    }
    if (u < IN0 || u > IN1 || v < FELT0 || v > FELT1) { ui_sound(SND_DENY); return; }
    if (P->phase == FK_THROW) { throw_dice(); return; }   /* the felt throws at a turn's start */
    for (int i = 0; i < P->nd; i++) {
        float dx, dy, deg;
        die_spot(i, P->nd, tumble.key, &dx, &dy, &deg);
        if (fabsf(u - dx) < 0.6f * DIE && fabsf(v - dy) < 0.6f * DIE) { tap_die(i); return; }
    }
    ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the player's banked score */
{
    float h = 0.5f * ui.unit;
    int s = P->score[player], n = s >= 10000 ? 5 : s >= 1000 ? 4 : s >= 100 ? 3 : s >= 10 ? 2 : 1;
    ui_number(b.x + (0.74f * (n - 1) + 0.62f) * h / 2, b.y + b.h / 2, h, s, 1);
}

static void token(int player, int size, uint8_t *rgba) { cup_token(player, size, rgba); }

/* the menu's card: the felt, three dice chosen in gold light, the bank's coins */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    cup_board(rgba, size);
    cup_well(rgba, size, IN0, IN0, IN1, 0.74f);
    static const int faces[6] = {1, 5, 1, 3, 1, 6};
    int d = (int)(0.2f * size);
    for (int i = 0; i < 6; i++) {
        float x = (0.2f + 0.3f * (i % 3)) * size, y = (0.22f + 0.28f * (i / 3)) * size;
        cup_paint_die(rgba, size, size, x, y, d, 0, faces[i], (float)((i * 37) % 40 - 20));
    }
    int bsz = (int)(0.24f * size);
    uint8_t *b = malloc((size_t)bsz * bsz * 4);
    if (b) {
        cup_button_pic(CUP_BANK, bsz, b);
        art_blend(rgba, size, size, b, bsz, bsz, (int)(0.2f * size) - bsz / 2, (int)(0.86f * size) - bsz / 2);
        cup_button_pic(CUP_THROW, bsz, b);
        art_blend(rgba, size, size, b, bsz, bsz, (int)(0.8f * size) - bsz / 2, (int)(0.86f * size) - bsz / 2);
        free(b);
    }
}

static const game farkle_game = {
    .id = "farkle", .order = 154, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1u << 1 | 1u << 2 | 1u << 3 | 1u << 4 | 1u << 5 | 1u << 6, .home = home,
};
REGISTER_GAME(farkle_game)
