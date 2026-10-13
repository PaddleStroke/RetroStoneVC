/*
 * Pig in RetroStone Board: the push-your-luck dice game for 2 to 6 (the rules and the notation: pig_rules.c, pig.h).
 * Nothing is secret (VIEW_OPEN): the view follows the player to roll, their bar at the bottom.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A plank of the house wood in its walnut frame. At the top, the race to 100: a groove for each player, their die
 * along it at their score, the finish post in gold at the far end; the player to roll's groove in a gold light, the
 * turn's total running ahead of their die in gold (where a hold would take it). In the middle, a well of green felt:
 * the die (two in two-dice pig) in the player's colour, a tap on the felt or the right-hand button (a die in flight)
 * rolls it; under it the turn's total. At the bottom, a rack: the turn's rolls lined up small in its groove, the bank
 * button on the left (a stack of coins) holds. A 1: the dice in a red glow, the total lost shown in red a moment (two
 * 1s: the player's die slides back to the start). A hold: the points in gold, the die sliding along to its new score.
 * Coordinates: fractions of the board's side, from its top left.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dicecup.h"
#include "games/pig.h"
#include "shell.h"

#define IN0 (CUP_FR + 0.012f)
#define IN1 (1 - CUP_FR - 0.012f)
#define TRACK0 (IN0 + 0.012f)                   /* the race's grooves */
#define TRACK1 0.25f
#define TX0 0.1f                                /* 0 points, 100 points */
#define TX1 0.87f
#define FELT0 0.27f
#define FELT1 0.79f
#define DIE 0.16f
#define DICE_Y 0.47f
#define TOTAL_Y 0.69f
#define RACK_Y 0.88f
#define RACK_X0 0.21f                           /* the turn's rolls, in the rack's groove */
#define RACK_X1 0.79f
#define BTN 0.12f
#define HOLD_X 0.11f
#define ROLL_X 0.89f
#define LOST_MS 1300                            /* a 1 shown */
#define BANK_MS 900                             /* a hold shown */
#define SLIDE_MS 520                            /* a die sliding along its groove */

static piggame G;
static const pigpos *P = &G.pos;
static int S, made = -1, made_n = -1;
static float bx, by;
static SDL_Texture *t_board;
static struct { int on; Uint32 t0; } tumble;
static struct { int on, landed; Uint32 t0; } lost;    /* a turn lost on a 1 (P->ended, P->end_dice...) */
static struct { int on, player, from, to; Uint32 t0; } bank;   /* a hold: the score from, to */

/* ---- where things are ------------------------------------------------------------------------------------------- */
static float lane_h(void) { return (TRACK1 - TRACK0) / P->players; }
static float lane_y(int q) { return TRACK0 + (q + 0.5f) * lane_h(); }
static float score_x(int pts) { return TX0 + (TX1 - TX0) * clampf(pts / (float)PIG_TARGET, 0, 1.06f); }
static float SX(float x) { return bx + x * S; }
static float SY(float y) { return by + y * S; }

static void die_spot(int i, int n, float *x, float *y)
{
    *x = n == 1 ? 0.5f : 0.5f + (i ? 1 : -1) * 0.12f;
    *y = DICE_Y;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void reset_ui(void)
{
    tumble.on = lost.on = bank.on = 0;
    if (P->rolls) {                             /* the last roll at rest */
        tumble.on = 1;
        tumble.t0 = ui_ms() - UI_DICE_MS;
    }
}

static void start(int variant)
{
    pig_new(&G, variant, ui.players);
    reset_ui();
}

static int status(void) { return P->phase == PIG_OVER ? gs_win(P->winner) : GS_PLAY; }
static int turn(void) { return P->turn; }
static int rolling(void) { return tumble.on && ui_ms() - tumble.t0 < UI_DICE_MS; }
static int busy(void) { return rolling() || lost.on || bank.on; }

static void roll(void)
{
    if (P->phase != PIG_PLAY) return;
    int d1 = 1 + ui_random(6), d2 = pig_ndice(P) == 2 ? 1 + ui_random(6) : 0, me = P->turn;
    if (!pig_roll(&G, d1, d2)) { ui_sound(SND_DENY); return; }
    tumble.on = 1;
    tumble.t0 = ui_ms();
    ui_sound(SND_DICE);
    if (P->turn != me) {                        /* a 1: the turn lost, once the dice land */
        lost.on = 1;
        lost.landed = 0;
        ui_handoff(P->turn);
    }
}

static void hold(void)
{
    int me = P->turn, from = P->score[me];
    if (!pig_hold(&G)) { ui_sound(SND_DENY); return; }
    bank.on = 1;
    bank.player = me;
    bank.from = from;
    bank.to = P->score[me];
    bank.t0 = ui_ms();
    tumble.on = 0;
    ui_sound(SND_PLACE);
    ui_handoff(P->turn);
}

static int undo(void)
{
    if (!pig_undo(&G)) return 0;
    reset_ui();
    ui_handoff(P->turn);
    return 1;
}

/* "r4 r3 h" (two dice: "r34"): pig.h */
static int script(const char *s)
{
    int ok = pig_script(&G, s);
    reset_ui();
    const char *last = s + strlen(s);
    while (last > s && last[-1] == ' ') last--;
    while (last > s && last[-1] != ' ') last--;
    if (*last == 'r' && P->rolls == 0 && P->turns) {   /* the last roll lost the turn: its pause to come */
        lost.on = 1;
        lost.landed = 0;
        tumble.on = 1;
        tumble.t0 = ui_ms() - UI_DICE_MS;
    }
    ui_handoff(P->turn);
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void paint_board(uint8_t *o, int n, int players)
{
    static const float groove[3] = {0.8f, 0.78f, 0.76f};
    cup_board(o, n);
    cup_well(o, n, IN0, FELT0, IN1, FELT1);
    art_inlay(o, n, n, RACK_X0 * n - 0.02f * n, (RACK_Y - 0.05f) * n, (RACK_X1 - RACK_X0 + 0.04f) * n, 0.1f * n,
              0.03f * n, groove, 0.006f * n);
    float lh = (TRACK1 - TRACK0) / players, gh = fminf(0.032f, 0.62f * lh);
    for (int q = 0; q < players; q++) {         /* the grooves of the race, a light mark every ten points */
        float y = TRACK0 + (q + 0.5f) * lh;
        art_inlay(o, n, n, (TX0 - 0.03f) * n, (y - gh / 2) * n, (TX1 - TX0 + 0.06f) * n, gh * n, gh / 2 * n,
                  groove, 0.004f * n);
        for (int k = 1; k < 10; k++)
            art_tint_rect(o, n, n, (TX0 + (TX1 - TX0) * k / 10 - 0.0015f) * n, (y - gh * 0.3f) * n,
                          (TX0 + (TX1 - TX0) * k / 10 + 0.0015f) * n, (y + gh * 0.3f) * n, CUP_CREAM, 0.5f);
    }
    static const uint8_t gold[3] = {214, 168, 64};   /* the finish post */
    art_tint_rect(o, n, n, (TX1 - 0.003f) * n, (TRACK0 - 0.004f) * n, (TX1 + 0.003f) * n, (TRACK1 + 0.004f) * n,
                  gold, 0.9f);
}

static void layout(box b)
{
    S = (int)floorf(fminf(b.w, b.h));
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    if (made == S && made_n == P->players) return;
    made = S;
    made_n = P->players;
    uint8_t *o = malloc((size_t)S * S * 4);
    if (!o) return;
    paint_board(o, S, P->players);
    ui_free(&t_board);
    t_board = ui_tex(S, S, o);
    free(o);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void number_at(float x, float y, float h, int v) { ui_number(SX(x), SY(y), h * S, v, 1); }

/* the race: each player's die at their score (sliding after a hold, back after two 1s), the turn's total ahead of
 * the player to roll in gold */
static void draw_track(Uint32 now)
{
    float lh = lane_h(), d = fminf(0.05f, 0.82f * lh);
    for (int q = 0; q < P->players; q++) {
        float y = lane_y(q), x = score_x(P->score[q]);
        if (bank.on && bank.player == q) {
            float f = clampf((now - bank.t0) / (float)SLIDE_MS, 0, 1);
            x = cup_mix(score_x(bank.from), score_x(bank.to), f);
        } else if (lost.on && P->ended == PIG_WIPED && P->ended_by == q) {
            float f = lost.landed ? clampf((now - lost.t0) / (float)SLIDE_MS, 0, 1) : 0;
            x = cup_mix(score_x(P->ended_score), score_x(0), f);
        }
        int moving = q == P->turn && P->phase == PIG_PLAY && !busy();
        if (moving && P->total > 0) {           /* the turn's total, running ahead */
            float x1 = score_x(P->score[q] + P->total);
            ui_fill(SX(x), SY(y - 0.006f), (x1 - x) * S, 0.012f * S, 255, 196, 64, 150);
            ui_die(SX(x1), SY(y), d * S, q, 5, 0, 0, 0.45f, 1);
        }
        if (moving) ui_mark_gold(SX(x), SY(y), 2.2f * d * S);
        if (P->phase == PIG_OVER && q == P->winner) ui_mark_gold(SX(x), SY(y), 3 * d * S);
        ui_die(SX(x), SY(y), d * S, q, 5, 0, 0, 1, 1);
    }
}

/* the turn's rolls, small in the rack's groove (the latest ones when they are many) */
static void draw_rolls(int me)
{
    int two = pig_ndice(P) == 2, n = P->rolls < PIG_SEQ ? P->rolls : PIG_SEQ, per = two ? 2 : 1;
    if (!n) return;
    float w = RACK_X1 - RACK_X0, d = fminf(0.055f, w / (n * per * 1.12f + (n - 1) * (two ? 0.3f : 0)));
    float step = d * (1.12f * per + (two ? 0.3f : 0)), x = 0.5f - (n * step - (two ? 0.3f * d : 0) - 0.12f * d) / 2;
    if (rolling() && n) n--;                    /* the roll still in the air: not in the rack yet */
    for (int k = 0; k < n; k++)
        for (int j = 0; j < per; j++)
            ui_die(SX(x + k * step + (j + 0.5f) * 1.12f * d), SY(RACK_Y), d * S, me, P->seq[k][j], 0, 0, 1, 1);
}

static void draw(void)
{
    Uint32 now = ui_ms();
    if (made_n != P->players) layout((box){bx, by, (float)S, (float)S});
    ui_put(t_board, bx, by, (float)S, (float)S);
    draw_track(now);
    float bd = BTN * S;
    if (bank.on) {                              /* a hold: the points banked in gold */
        if (now - bank.t0 >= BANK_MS) bank.on = 0;
        else {
            ui_mark_gold(SX(0.5f), SY(DICE_Y + 0.05f), 0.32f * S);
            number_at(0.5f, DICE_Y + 0.05f, 0.1f, bank.to - bank.from);
            ui_animate();
            return;
        }
    }
    if (lost.on) {                              /* a 1: the dice in a red glow, the total lost */
        int t = (int)(now - tumble.t0), n = P->end_dice[1] ? 2 : 1;
        if (!lost.landed && t >= UI_DICE_MS) {
            lost.landed = 1;
            lost.t0 = now;
            ui_sound(SND_DENY);
        }
        if (lost.landed && now - lost.t0 >= LOST_MS) lost.on = 0;
        else {
            for (int i = 0; i < n; i++) {
                float x, y;
                die_spot(i, n, &x, &y);
                if (lost.landed && P->end_dice[i] == 1) ui_mark_glow(SX(x), SY(y), 1.9f * DIE * S);
                ui_die_thrown(SX(x), SY(y), DIE * S, P->ended_by, P->end_dice[i], cup_tilt(P->turns, i), i, t, 1);
            }
            if (lost.landed && P->ended_pts) {
                ui_mark_glow(SX(0.5f), SY(TOTAL_Y), 0.24f * S);
                number_at(0.5f, TOTAL_Y, 0.075f, P->ended_pts);
            }
            ui_animate();
            return;
        }
    }
    if (P->phase == PIG_OVER) return;
    int me = P->turn, n = pig_ndice(P);
    draw_rolls(me);
    if (!P->rolls) {                            /* waiting to be rolled */
        static const int idle[2] = {6, 3};
        for (int i = 0; i < n; i++) {
            float x, y;
            die_spot(i, n, &x, &y);
            ui_die(SX(x), SY(y), DIE * S, me, idle[i], 0, 0, 0.9f, 1);
        }
    } else {
        int t = rolling() ? (int)(now - tumble.t0) : UI_DICE_MS;
        for (int i = 0; i < n; i++) {
            float x, y;
            die_spot(i, n, &x, &y);
            ui_die_thrown(SX(x), SY(y), DIE * S, me, P->dice[i], cup_tilt(P->turns * 64 + P->rolls, i), i, t, 1);
        }
        int shown = P->total - (rolling() ? P->dice[0] + (n == 2 ? P->dice[1] : 0) : 0);
        number_at(0.5f, TOTAL_Y, 0.075f, shown);
    }
    cup_button_at(CUP_THROW, SX(ROLL_X), SY(RACK_Y), bd, !rolling(), 0);
    cup_button_at(CUP_BANK, SX(HOLD_X), SY(RACK_Y), bd, pig_can_hold(P) && !rolling(), 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (lost.on) {                              /* a tap ends the pause */
        if (lost.landed) lost.on = 0;
        return;
    }
    if (bank.on) bank.on = 0;
    if (P->phase != PIG_PLAY || rolling()) return;
    float u = (x - bx) / S, v = (y - by) / S;
    if (hypotf(u - ROLL_X, v - RACK_Y) < 0.6f * BTN) { roll(); return; }
    if (hypotf(u - HOLD_X, v - RACK_Y) < 0.6f * BTN) { hold(); return; }
    if (u >= IN0 && u <= IN1 && v >= FELT0 && v <= FELT1) { roll(); return; }
    ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)             /* the player's score */
{
    int s = P->score[player];
    if (bank.on && bank.player == player) s = bank.from;
    if (lost.on && P->ended == PIG_WIPED && P->ended_by == player) s = P->ended_score;
    ui_bar_number(b, NULL, s);
}

static void token(int player, int size, uint8_t *rgba) { cup_token(player, size, rgba); }

/* the menu's card and the picker's: the felt, the die (two dice for two-dice pig), the race's grooves above with
 * three players' dice along them, the bank's coins */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const float groove[3] = {0.8f, 0.78f, 0.76f};
    static const uint8_t gold[3] = {214, 168, 64};
    cup_board(rgba, size);
    cup_well(rgba, size, IN0, 0.36f, IN1, IN1);
    static const float at[3] = {0.62f, 0.3f, 0.45f};
    for (int q = 0; q < 3; q++) {
        float y = 0.1f + q * 0.085f;
        art_inlay(rgba, size, size, 0.08f * size, (y - 0.025f) * size, 0.82f * size, 0.05f * size, 0.025f * size,
                  groove, 0.005f * size);
        art_die_blend(rgba, size, size, at[q] * size, y * size, (int)(0.075f * size), q, 5, 0);
    }
    art_tint_rect(rgba, size, size, 0.84f * size, 0.05f * size, 0.85f * size, 0.31f * size, gold, 0.9f);
    if (variant == PIG_TWO) {
        art_die_blend(rgba, size, size, 0.36f * size, 0.6f * size, (int)(0.26f * size), 0, 4, -14);
        art_die_blend(rgba, size, size, 0.64f * size, 0.64f * size, (int)(0.26f * size), 0, 6, 12);
    } else art_die_blend(rgba, size, size, 0.5f * size, 0.62f * size, (int)(0.3f * size), 0, 6, -10);
    if (variant < 0) ui_button_pic(rgba, size, size, 0.18f * size, 0.84f * size, (int)(0.22f * size), cup_button_pic,
                                   CUP_BANK, 1, 0);
}

static const game pig_game = {
    .id = "pig", .order = 159, .variants = 2,     /* pig, two-dice pig */
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
    .players = 1u << 2 | 1u << 3 | 1u << 4 | 1u << 5 | 1u << 6, .view = VIEW_OPEN,
};
REGISTER_GAME(pig_game)
