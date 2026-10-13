/*
 * Ship, Captain and Crew in RetroStone Board: the tavern dice game for 2 to 6 (the rules and the notation:
 * shipcaptain_rules.c, shipcaptain.h). Nothing is secret (VIEW_OPEN): the view follows the player to roll, their bar
 * at the bottom (the chips won).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A plank of the house wood in its walnut frame. At the top, the round so far: each player's die and their cargo
 * (nothing yet: the die faded), the player to roll in a gold light; once a round is over, its cargoes with the best in
 * gold until the next one starts. In the middle, a well of green felt where the five dice in the player's colour
 * tumble (a tap on the felt or the right-hand button rolls the dice left), the rolls left as three small dice under
 * them. At the bottom, a rack with three slots carved in a row, a faint 6, 5 and 4 in them: the ship, the captain and
 * the crew slide into them as they come. With all three in, the cargo's sum shows in gold under the two dice left, and
 * the left-hand button (a check) stops. A turn ended: its cargo in gold, or the empty slots in a red glow (no cargo).
 * Coordinates: fractions of the board's side, from its top left.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dicecup.h"
#include "games/shipcaptain.h"
#include "shell.h"

#define IN0 (CUP_FR + 0.012f)
#define IN1 (1 - CUP_FR - 0.012f)
#define ROW_Y 0.13f                             /* the round's cargoes */
#define FELT0 0.22f
#define FELT1 0.78f
#define DIE 0.13f
#define CARGO_Y 0.64f
#define PIPS_Y 0.735f
#define RACK_Y 0.88f
#define SLOT_X0 0.355f                          /* the ship's slot; the captain's and the crew's after it */
#define SLOT_STEP 0.145f
#define SLOT_DIE 0.105f
#define BTN 0.12f
#define STOP_X 0.11f
#define ROLL_X 0.89f
#define END_MS 1250                             /* a turn's end shown */

static sccgame G;
static const sccpos *P = &G.pos;
static int S, made = -1;
static float bx, by;
static SDL_Texture *t_board;
static struct { int on, key; unsigned mask, newly; Uint32 t0; } tumble;   /* mask: the dice rolled; newly: the
                                                                         * slots filled by that roll (bit k) */
static struct { int on, rolled, landed; Uint32 t0; } ended;   /* a turn over (P->end_*): rolled: by its third roll */

/* ---- where things are ------------------------------------------------------------------------------------------- */
static float SX(float x) { return bx + x * S; }
static float SY(float y) { return by + y * S; }
static int key_of(const sccpos *p, int rolls) { return (p->rounds * 8 + p->turn) * 4 + rolls; }

/* die i on the felt: three in a row, two under them, a little askew after the roll `key` */
static void die_spot(int i, int key, float *x, float *y, float *deg)
{
    *x = i < 3 ? 0.5f + (i - 1) * 0.2f : 0.5f + (i == 3 ? -0.1f : 0.1f);
    *y = i < 3 ? 0.35f : 0.51f;
    *deg = 0;
    if (key >= 0) {
        *x += 0.025f * cup_jitter(key, i, 0);
        *y += 0.02f * cup_jitter(key, i, 1);
        *deg = cup_tilt(key, i);
    }
}

static float slot_x(int k) { return SLOT_X0 + k * SLOT_STEP; }

static int slot_of(const int8_t *aside, int i)  /* the slot die i is in (-1: on the felt) */
{
    for (int k = 0; k < 3; k++)
        if (aside[k] == i) return k;
    return -1;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void reset_ui(void)
{
    tumble.on = ended.on = 0;
    if (P->rolls) {                             /* the last roll at rest */
        tumble.on = 1;
        tumble.key = key_of(P, P->rolls - 1);
        tumble.mask = tumble.newly = 0;
        tumble.t0 = ui_ms() - UI_DICE_MS;
    }
}

static void start(int variant)
{
    (void)variant;
    scc_new(&G, ui.players);
    reset_ui();
}

static int status(void)
{
    if (P->phase != SCC_OVER) return GS_PLAY;
    for (int q = 0; q < P->players; q++)
        if (P->winners >> q & 1) return gs_win(q);
    return GS_DRAW;
}

static unsigned winners(void) { return P->phase == SCC_OVER ? P->winners : 0; }
static int turn(void) { return P->turn; }
static int rolling(void) { return tumble.on && ui_ms() - tumble.t0 < UI_DICE_MS + 2 * CUP_SLIDE_MS; }
static int busy(void) { return rolling() || ended.on; }

static void roll(void)
{
    sccpos was = *P;
    int faces[SCC_DICE];
    for (int i = 0; i < SCC_DICE; i++) faces[i] = 1 + ui_random(6);
    if (!scc_roll(&G, faces)) { ui_sound(SND_DENY); return; }
    tumble.on = 1;
    tumble.t0 = ui_ms();
    tumble.key = key_of(&was, was.rolls);
    tumble.mask = scc_free(&was);
    const int8_t *aside = P->turn == was.turn && P->rounds == was.rounds && P->phase == SCC_PLAY ? P->aside
                                                                                                   : P->end_aside;
    tumble.newly = 0;
    for (int k = 0; k < 3; k++) tumble.newly |= (unsigned)(aside[k] >= 0 && was.aside[k] < 0) << k;
    ui_sound(SND_DICE);
    if (tumble.newly) ui_sound_later(SND_PLACE, UI_DICE_MS + 2 * CUP_SLIDE_MS);
    if (P->turn != was.turn || P->rounds != was.rounds || P->phase != SCC_PLAY) {   /* the third roll: over */
        ended.on = 1;
        ended.rolled = 1;
        ended.landed = 0;
        ui_handoff(P->turn);
    }
}

static void stop(void)
{
    if (!scc_stop(&G)) { ui_sound(SND_DENY); return; }
    ui_sound(SND_PLACE);
    ended.on = 1;
    ended.rolled = 0;
    ended.landed = 1;
    ended.t0 = ui_ms();
    tumble.on = 0;
    ui_handoff(P->turn);
}

static int undo(void)
{
    if (!scc_undo(&G)) return 0;
    reset_ui();
    ui_handoff(P->turn);
    return 1;
}

/* "r65432 r66 s": scc.h */
static int script(const char *s)
{
    int ok = scc_script(&G, s);
    reset_ui();
    ui_handoff(P->turn);
    return ok;
}

/* ---- the bot (shipcaptain_bot.c) --------------------------------------------------------------------------------- */
const sccgame *shipcaptain_live(void) { return &G; }

/* a bot's move, as a tap makes it: "r" and a "?" for each die thrown (the game's dice), "s" a stop */
int shipcaptain_show(const char *move)
{
    if (busy() || P->phase != SCC_PLAY) return 0;
    if (!strcmp(move, "s")) {
        if (!scc_can_stop(P)) return 0;
        stop();
        return 1;
    }
    if (move[0] != 'r' || (int)strlen(move + 1) != __builtin_popcount(scc_free(P)) || P->rolls >= SCC_ROLLS) return 0;
    for (const char *s = move + 1; *s; s++)
        if (*s != '?') return 0;
    roll();
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a little ship (the hull, a mast, two sails) over a picture size x size at (x, y), w wide: fractions of the picture */
static void paint_ship(uint8_t *o, int size, float x, float y, float w, const uint8_t ink[3], const uint8_t sail[3])
{
    art_fig *f = art_fig_new();
    const float hull[] = {x, y + 0.62f * w, x + w, y + 0.62f * w, x + 0.84f * w, y + 0.86f * w, x + 0.14f * w,
                          y + 0.86f * w};
    art_poly(f, ART_PART, hull, 4);
    art_rect(f, ART_PART, x + 0.47f * w, y + 0.06f * w, x + 0.53f * w, y + 0.64f * w);
    art_fig_paint(o, size, f, ink, NULL, 0, 0.3f);
    f = art_fig_new();
    const float s1[] = {x + 0.43f * w, y + 0.1f * w, x + 0.43f * w, y + 0.56f * w, x + 0.12f * w, y + 0.56f * w},
                s2[] = {x + 0.57f * w, y + 0.16f * w, x + 0.57f * w, y + 0.56f * w, x + 0.86f * w, y + 0.56f * w};
    art_poly(f, ART_PART, s1, 3);
    art_poly(f, ART_PART, s2, 3);
    art_fig_paint(o, size, f, sail, ink, 0.006f, 0.25f);
}

static void paint_board(uint8_t *o, int n)
{
    static const float groove[3] = {0.8f, 0.78f, 0.76f};
    cup_board(o, n);
    cup_well(o, n, IN0, FELT0, IN1, FELT1);
    art_inlay(o, n, n, (slot_x(0) - 0.07f) * n, (RACK_Y - 0.065f) * n, (2 * SLOT_STEP + 0.14f) * n, 0.13f * n,
              0.03f * n, groove, 0.006f * n);
    for (int k = 0; k < 3; k++)                 /* the three slots, carved */
        for (int y = (int)((RACK_Y - 0.06f) * n); y < (int)((RACK_Y + 0.06f) * n); y++)
            for (int x = (int)((slot_x(k) - 0.06f) * n); x < (int)((slot_x(k) + 0.06f) * n); x++) {
                float f = art_recess(x - slot_x(k) * n, y - RACK_Y * n, 0.052f * n, 0.008f * n, 0.62f, 0.2f, 0.18f);
                uint8_t *p = o + 4 * ((size_t)y * n + x);
                for (int c = 0; c < 3; c++) p[c] = (uint8_t)clampf(p[c] * f, 0, 255);
            }
    art_inlay(o, n, n, (IN0 + 0.02f) * n, (ROW_Y - 0.06f) * n, (IN1 - IN0 - 0.04f) * n, 0.12f * n, 0.03f * n, groove,
              0.006f * n);
}

static void layout(box b)
{
    S = (int)floorf(fminf(b.w, b.h));
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
static void number_at(float x, float y, float h, int v, float alpha)
{
    static const uint8_t cream[3] = {246, 238, 220};
    ui_number_ex(SX(x), SY(y), h * S, v, cream, (Uint8)(255 * alpha), 0);
}

/* the round so far (or the last one, until the next starts): each player's die and cargo */
static void draw_row(void)
{
    int last = P->done == 0 && P->rolls == 0 && P->rounds > 0;
    const int8_t *res = last ? P->last_res : P->res;
    float w = (IN1 - IN0 - 0.06f) / P->players, d = fminf(0.06f, 0.36f * w);
    for (int q = 0; q < P->players; q++) {
        float cx = IN0 + 0.03f + (q + 0.5f) * w, x = cx - (res[q] >= 10 ? 0.55f : 0.4f) * d;
        int played = res[q] >= 0;
        if (P->phase == SCC_PLAY && !last && q == P->turn && !ended.on) ui_mark_gold(SX(cx), SY(ROW_Y), 0.9f * w * S);
        if (last && (P->last_won >> q & 1)) ui_mark_gold(SX(cx), SY(ROW_Y), 0.9f * w * S);
        ui_die(SX(x), SY(ROW_Y), d * S, q, 5, 0, 0, played || q == P->turn ? 1 : 0.4f, 1);
        if (played) number_at(x + 0.62f * d + (res[q] >= 10 ? 0.75f : 0.5f) * d, ROW_Y, 0.045f, res[q],
                              res[q] ? 1 : 0.45f);
    }
}

/* the dice of a turn: those set aside in their slots (sliding there after the roll that brought them), the others on
 * the felt (tumbling while t < UI_DICE_MS for those rolled); the cargo's sum */
static void draw_dice(int me, const uint8_t *dice, const int8_t *aside, int t, Uint32 now)
{
    for (int k = 0; k < 3; k++)                 /* the empty slots: a faint 6, 5, 4 */
        if (aside[k] < 0) ui_die(SX(slot_x(k)), SY(RACK_Y), SLOT_DIE * S, me, 6 - k, 0, 0, 0.22f, 1);
    for (int i = 0; i < SCC_DICE; i++) {
        if (!dice[i]) continue;
        int k = slot_of(aside, i), rolled = tumble.on && (tumble.mask >> i & 1);
        float x, y, deg;
        die_spot(i, rolled || k < 0 ? tumble.key : -1, &x, &y, &deg);
        if (k >= 0 && !(rolled && (tumble.newly >> k & 1))) {
            ui_die(SX(slot_x(k)), SY(RACK_Y), SLOT_DIE * S, me, dice[i], 0, 0, 1, 1);
            continue;
        }
        if (k >= 0) {                           /* brought by this roll: lands, then slides into its slot */
            float f = (float)(t - UI_DICE_MS) / (2 * CUP_SLIDE_MS);
            if (f > 0) {
                ui_animate();
                float sz = cup_mix(DIE, SLOT_DIE, f);
                ui_die(SX(cup_mix(x, slot_x(k), f)), SY(cup_mix(y, RACK_Y, f)), sz * S, me, dice[i],
                       deg * (1 - clampf(f, 0, 1)), 0, 1, 1);
                continue;
            }
        }
        if (rolled && t < UI_DICE_MS) ui_die_thrown(SX(x), SY(y), DIE * S, me, dice[i], deg, i, t, 1);
        else ui_die(SX(x), SY(y), DIE * S, me, dice[i], deg, 0, 1, 1);
    }
    (void)now;
}

static void draw(void)
{
    Uint32 now = ui_ms();
    ui_put(t_board, bx, by, (float)S, (float)S);
    draw_row();
    float bd = BTN * S;
    int t = tumble.on ? (int)(now - tumble.t0) : UI_DICE_MS + 2 * CUP_SLIDE_MS;
    if (tumble.on && t < UI_DICE_MS + 2 * CUP_SLIDE_MS) ui_animate();
    if (ended.on) {                             /* a turn over: its cargo, or none */
        if (!ended.landed && t >= UI_DICE_MS + 2 * CUP_SLIDE_MS) {
            ended.landed = 1;
            ended.t0 = now;
            ui_sound(P->end_cargo ? SND_PLACE : SND_DENY);
        }
        if (ended.landed && now - ended.t0 >= END_MS) ended.on = 0;
        else {
            draw_dice(P->end_by, P->end_dice, P->end_aside, ended.rolled ? t : 1 << 20, now);
            if (ended.landed) {
                if (P->end_cargo) {
                    ui_mark_gold(SX(0.5f), SY(CARGO_Y), 0.24f * S);
                    number_at(0.5f, CARGO_Y, 0.08f, P->end_cargo, 1);
                } else
                    for (int k = 0; k < 3; k++)
                        if (P->end_aside[k] < 0) ui_mark_glow(SX(slot_x(k)), SY(RACK_Y), 1.7f * SLOT_DIE * S);
            }
            ui_animate();
            return;
        }
    }
    if (P->phase == SCC_OVER) return;
    int me = P->turn;
    if (!P->rolls) {                            /* waiting to be rolled */
        static const int idle[SCC_DICE] = {6, 2, 5, 3, 4};
        for (int k = 0; k < 3; k++) ui_die(SX(slot_x(k)), SY(RACK_Y), SLOT_DIE * S, me, 6 - k, 0, 0, 0.22f, 1);
        for (int i = 0; i < SCC_DICE; i++) {
            float x, y, deg;
            die_spot(i, -1, &x, &y, &deg);
            ui_die(SX(x), SY(y), DIE * S, me, idle[i], 0, 0, 0.9f, 1);
        }
    } else {
        draw_dice(me, P->dice, P->aside, t, now);
        if (scc_complete(P) && t >= UI_DICE_MS + 2 * CUP_SLIDE_MS) {
            ui_mark_gold(SX(0.5f), SY(CARGO_Y), 0.2f * S);
            number_at(0.5f, CARGO_Y, 0.07f, scc_cargo(P), 1);
        }
    }
    for (int k = 0; k < SCC_ROLLS; k++) {       /* the rolls left */
        float x = 0.5f + (k - 1) * 0.055f;
        int left = k >= P->rolls;
        ui_die(SX(x), SY(PIPS_Y), 0.038f * S, me, 6 - k, 0, 0, left ? 1 : 0.28f, 1);
    }
    int idle = !rolling();
    cup_button_at(CUP_THROW, SX(ROLL_X), SY(RACK_Y), bd, idle && P->rolls < SCC_ROLLS, 0);
    ui_button(SX(STOP_X), SY(RACK_Y), bd, ICON_TICK, idle && scc_can_stop(P) ? UI_LIT : UI_DIM);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (ended.on) {                             /* a tap ends the pause */
        if (ended.landed) ended.on = 0;
        return;
    }
    if (P->phase != SCC_PLAY || rolling()) return;
    float u = (x - bx) / S, v = (y - by) / S;
    if (hypotf(u - ROLL_X, v - RACK_Y) < 0.6f * BTN) { roll(); return; }
    if (hypotf(u - STOP_X, v - RACK_Y) < 0.6f * BTN) { stop(); return; }
    if (u >= IN0 && u <= IN1 && v >= FELT0 && v <= FELT1) { roll(); return; }
    ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ui_bar_number(b, cup_chip((int)(0.6f * ui.unit)), P->chips[player]); }

static void token(int player, int size, uint8_t *rgba) { cup_token(player, size, rgba); }

/* the menu's card: a little ship on the felt, the 6, 5 and 4 in a row in a gold light, two cargo dice */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    static const uint8_t ink[3] = {104, 62, 36}, sail[3] = {240, 230, 204}, gold[3] = {255, 196, 64};
    cup_board(rgba, size);
    cup_well(rgba, size, IN0, IN0, IN1, IN1);
    paint_ship(rgba, size, 0.07f, 0.08f, 0.36f, ink, sail);
    int g = (int)(0.8f * size);
    uint8_t *b = malloc((size_t)g * g * 4);
    if (b) {
        art_disc(g, gold, 0.55f, 1.6f, b);
        art_blend(rgba, size, size, b, g, g, (int)(0.5f * size) - g / 2, (int)(0.63f * size) - g / 2);
        free(b);
    }
    for (int k = 0; k < 3; k++)
        art_die_blend(rgba, size, size, (0.25f + k * 0.25f) * size, 0.63f * size, (int)(0.21f * size), 0, 6 - k, 0);
    art_die_blend(rgba, size, size, 0.62f * size, 0.26f * size, (int)(0.2f * size), 0, 6, -16);
    art_die_blend(rgba, size, size, 0.84f * size, 0.3f * size, (int)(0.2f * size), 0, 5, 14);
}

static const game shipcaptain_game = {
    .id = "shipcaptain", .order = 160, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1u << 2 | 1u << 3 | 1u << 4 | 1u << 5 | 1u << 6, .view = VIEW_OPEN,
};
REGISTER_GAME(shipcaptain_game)
