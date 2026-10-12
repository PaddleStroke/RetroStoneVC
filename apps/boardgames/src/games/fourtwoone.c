/*
 * 421 in RetroStone Board: a well of green felt on the table; the pot of bone chips in the middle during the charge;
 * the three dice in the player's colour tumble in front of the player to throw (tap the felt to throw), a tap on a die
 * keeps it (it slides down to the kept row), the round button with a check mark stops; each player's throw of the
 * round lies small on their side of the felt, next to their bar (the rules and the notation: fourtwoone_rules.c,
 * fourtwoone.h). Two to six players.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The round's best throw in a gold light, the worst in a red glow; when the round is settled the chips fly from the pot
 * (the charge) or from the winner (the discharge) to the loser. The small dice under the throw: how many throws this
 * round allows (the leader's), the ones used dimmed. Each bar: the player's chips. Face to face, the throw in the
 * middle turns to the player to throw when they sit at the top; each small throw faces its player. Coordinates:
 * fractions of the board's side, from its top left.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dicecup.h"
#include "games/fourtwoone.h"
#include "shell.h"

#define IN0 (CUP_FR + 0.012f)
#define IN1 (1 - CUP_FR - 0.012f)
#define DIE 0.13f
#define DICE_Y 0.47f                            /* the throw, in the player to throw's frame */
#define KEPT_Y 0.62f
#define PIPS_Y 0.73f
#define STOP_X 0.84f
#define STOP_Y 0.62f
#define BTN 0.12f
#define POT_Y 0.29f
#define SMALL 0.062f                            /* a throw on a player's side */
#define SIDE_Y 0.865f                           /* the side's throws: the bottom (the top: 1 - it) */
#define HOLD_MS 700                             /* a throw that ended a turn: shown after it lands */
#define FLY_MS 420
#define FLY_GAP 90

static tgame G;
static const tpos *P = &G.pos;
static int S, made = -1;
static float bx, by;
static SDL_Texture *t_board;
static struct { int on, key, player, ended; unsigned mask; uint8_t dice[FTO_DICE], kept; Uint32 t0; } shown;
static struct { int on, n; float x0, y0, x1, y1; Uint32 t0; } fly;
static Uint32 slide_t0[FTO_DICE];
static unsigned slide_mask;

static int throw_key(int rounds, int player, int throws)
{
    return (rounds * FTO_PLAYERS + player) * FTO_THROWS + throws;
}

/* ---- where things are ------------------------------------------------------------------------------------------- */
static int upside_of(int player) { return ui_upside(player); }
static float SX(float x, int up) { return bx + (up ? 1 - x : x) * S; }
static float SY(float y, int up) { return by + (up ? 1 - y : y) * S; }

static void seat(int player, float *x, float *y, int *top)   /* a player's throws on their side (the screen) */
{
    float fx, fy;
    cup_home(player, P->players, &fx, &fy);
    if (ui.flipped) { fx = 1 - fx; fy = 1 - fy; }
    *top = fy < 0.5f;
    *x = IN0 + 0.02f + fx * (IN1 - IN0 - 0.04f);
    *y = *top ? 1 - SIDE_Y : SIDE_Y;
}

static float die_x(int i) { return 0.5f + (i - 1) * 0.165f; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void reset_ui(void)
{
    shown.on = 0;
    fly.on = 0;
    slide_mask = 0;
}

static void start(int variant)
{
    (void)variant;
    fto_new(&G, ui.players);
    cup_mem_reset();
    reset_ui();
}

static unsigned winners(void) { return fto_winners(P); }

static int status(void)
{
    if (P->phase != FTO_OVER) return GS_PLAY;
    int w = P->first_out >= 0 && P->first_out != P->loser ? P->first_out : !P->loser;
    return gs_win(w);
}

static int turn(void) { return P->turn; }
static int holding(void) { return shown.on && shown.ended && ui_ms() - shown.t0 < UI_DICE_MS + HOLD_MS; }
static int flying(void) { return fly.on && ui_ms() - fly.t0 < (Uint32)(FLY_MS + FLY_GAP * (fly.n - 1) + 120); }
static int busy(void) { return (shown.on && ui_ms() - shown.t0 < UI_DICE_MS) || holding() || flying(); }
static void home(int player, float *fx, float *fy) { cup_home(player, P->players, fx, fy); }

/* the chips of the round just settled: from the pot (in the frame of the one to throw next) or the winner, to the
 * loser */
static void start_fly(int pot_up, int delay)
{
    float x, y;
    int top;
    fly.on = 1;
    fly.n = P->last_amount;
    fly.t0 = ui_ms() + (Uint32)delay;           /* once the last throw has landed */
    if (P->last_pot) {
        fly.x0 = pot_up ? 1 - 0.5f : 0.5f;
        fly.y0 = pot_up ? 1 - POT_Y : POT_Y;
    } else {
        seat(P->last_win, &x, &y, &top);
        fly.x0 = x;
        fly.y0 = y;
    }
    seat(P->last_lose, &x, &y, &top);
    fly.x1 = x;
    fly.y1 = y;
    for (int k = 0; k < fly.n; k++)
        ui_sound_later(SND_PLACE, delay + FLY_MS + FLY_GAP * k);
}

static void throw_dice(void)
{
    int me = P->turn, rounds = P->rounds, v[FTO_DICE], key = throw_key(P->rounds, me, P->throws);
    unsigned kept = P->throws ? P->kept : 0;
    int up_pot = P->phase == FTO_CHARGE;
    cup_roll(key, FTO_DICE, v);
    if (!fto_throw(&G, kept, v)) { ui_sound(SND_DENY); return; }
    const tpos *b = &G.past[G.n - 1];
    shown.on = 1;
    shown.key = key;
    shown.player = me;
    shown.mask = ~kept & 7u;
    shown.kept = (uint8_t)kept;
    for (int i = 0; i < FTO_DICE; i++) shown.dice[i] = (uint8_t)(kept >> i & 1 ? b->dice[i] : v[i]);
    shown.t0 = ui_ms();
    shown.ended = P->turn != me || P->rounds != rounds;
    slide_mask = 0;
    ui_sound(SND_DICE);
    if (P->rounds != rounds)
        start_fly(up_pot && P->phase != FTO_OVER ? upside_of(P->turn) : upside_of(me), UI_DICE_MS);
}

static void stop(void)
{
    int me = P->turn, rounds = P->rounds;
    if (!fto_stop(&G)) { ui_sound(SND_DENY); return; }
    ui_sound(SND_PLACE);
    shown.on = 0;
    if (P->rounds != rounds) start_fly(upside_of(P->phase == FTO_OVER ? me : P->turn), 0);
}

static void toggle_keep(int i)
{
    if (!P->throws || P->throws >= P->limit) { ui_sound(SND_DENY); return; }
    if (!fto_keep(&G, P->kept ^ (1u << i))) { ui_sound(SND_DENY); return; }
    slide_mask |= 1u << i;
    slide_t0[i] = ui_ms();
    if (P->kept >> i & 1) ui_sound(SND_PLACE);
    else ui_sound_soft(SND_TAP, 0.5f);
}

static int undo(void)
{
    if (!fto_undo(&G)) return 0;
    reset_ui();
    if (P->throws) {                            /* the throw to go on with, at rest */
        shown.on = 1;
        shown.key = throw_key(P->rounds, P->turn, P->throws - 1);
        shown.player = P->turn;
        shown.mask = 0;
        shown.ended = 0;
        memcpy(shown.dice, P->dice, sizeof shown.dice);
        shown.t0 = ui_ms() - UI_DICE_MS;
    }
    return 1;
}

/* "d651 d221 d43- s": throws and stops (fourtwoone.h) */
static int script(const char *s)
{
    int ok = fto_script(&G, s);
    for (int i = 0; i < G.n; i++)               /* its throws are the game's from now on */
        if (G.ev[i] & FTO_THROW_EV) {
            const tpos *b = &G.past[i];
            int v[FTO_DICE];
            unsigned kept = b->throws ? b->kept : 0;
            for (int k = 0; k < FTO_DICE; k++) v[k] = kept >> k & 1 ? 0 : (G.ev[i] >> (3 * k) & 7);
            cup_remember(throw_key(b->rounds, b->turn, b->throws), FTO_DICE, v);
        }
    reset_ui();
    if (P->throws) {
        shown.on = 1;
        shown.key = throw_key(P->rounds, P->turn, P->throws - 1);
        shown.player = P->turn;
        shown.mask = 0;
        shown.ended = 0;
        memcpy(shown.dice, P->dice, sizeof shown.dice);
        shown.t0 = ui_ms() - UI_DICE_MS;
    }
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void layout(box b)
{
    S = (int)floorf(b.w);
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    if (made == S) return;
    made = S;
    uint8_t *o = malloc((size_t)S * S * 4);
    if (!o) return;
    cup_board(o, S);
    cup_well(o, S, IN0, IN0, IN1, IN1);
    ui_free(&t_board);
    t_board = ui_tex(S, S, o);
    free(o);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void chip_at(float x, float y, float d)
{
    SDL_Texture *t = cup_chip((int)(d * S + 0.5f));
    ui_shadow_flat(bx + x * S, by + y * S, d * S, d * S, 0.6f);
    ui_put_c(t, bx + x * S, by + y * S, d * S, d * S);
}

static void draw_pot(int up)                    /* the chips left in the pot, in a heap */
{
    float cx = up ? 1 - 0.5f : 0.5f, cy = up ? 1 - POT_Y : POT_Y;
    for (int k = 0; k < P->pot; k++) {
        float a = race_hash(k, 3) * 2 * RACE_PI, r = 0.06f * sqrtf((k + 0.5f) / FTO_CHIPS);
        chip_at(cx + r * cosf(a) - 0.05f, cy + 0.7f * r * sinf(a), 0.055f);
    }
    if (!P->pot) return;
    if (up) ui_number_rot(bx + (cx + 0.07f) * S, by + cy * S, 0.05f * S, P->pot, 1, 180);
    else ui_number(bx + (cx + 0.07f) * S, by + cy * S, 0.05f * S, P->pot, 1);
}

/* the throws of the round on each player's side: this round's so far, else the last round's (its best in gold, its
 * worst in red) */
static void draw_sides(void)
{
    int last = P->norder == 0 && P->rounds > 0;
    int best = last ? P->last_win : fto_best(P), worst = last ? P->last_lose : fto_worst(P);
    for (int q = 0; q < P->players; q++) {
        const uint8_t *d = last ? P->last_res[q] : P->res[q];
        int played = last ? P->last_in >> q & 1 : P->done >> q & 1;
        int throws = last ? P->last_throws[q] : P->res_throws[q];
        if (!played) continue;
        if (shown.on && shown.ended && shown.player == q && holding()) continue;   /* still in the middle */
        float x, y;
        int top;
        seat(q, &x, &y, &top);
        int up = ui_upside(q), marks = last || P->norder > 1;   /* best and worst: once two have played */
        if (marks && q == best) ui_mark_gold(bx + x * S, by + y * S, 0.26f * S);
        if (marks && q == worst && worst != best) ui_mark_glow(bx + x * S, by + y * S, 0.24f * S);
        for (int i = 0; i < FTO_DICE; i++) {
            float dx = x + (up ? -1 : 1) * (i - 1) * SMALL * 1.12f;
            ui_die(bx + dx * S, by + y * S, SMALL * S, q, d[i], up ? 180 : 0, 0, 1, 1);
        }
        for (int k = 0; k < throws; k++) {     /* how many throws it took: small dots under it */
            float dx = x + (k - (throws - 1) / 2.0f) * 0.022f, dy = y + (top ? -1 : 1) * 0.05f;
            ui_mark_dot(bx + dx * S, by + dy * S, 0.014f * S);
        }
    }
}

static void draw_fly(Uint32 now)
{
    if (!fly.on) return;
    int any = 0;
    for (int k = 0; k < fly.n; k++) {
        float f = ((float)(int)(now - fly.t0) - FLY_GAP * k) / FLY_MS;
        if (f < 0) { any = 1; continue; }
        if (f >= 1) continue;
        any = 1;
        float x = cup_mix(fly.x0, fly.x1, f), y = cup_mix(fly.y0, fly.y1, f) - 0.06f * sinf(RACE_PI * f);
        chip_at(x, y, 0.055f);
    }
    if (any) ui_animate();
    else fly.on = 0;
}

static void draw(void)
{
    Uint32 now = ui_ms();
    ui_put(t_board, bx, by, (float)S, (float)S);
    if (P->phase == FTO_CHARGE || (fly.on && P->last_pot)) draw_pot(upside_of(P->turn));
    draw_sides();
    draw_fly(now);
    int t = shown.on ? (int)(now - shown.t0) : UI_DICE_MS;
    if (holding() || (shown.on && shown.ended && t < UI_DICE_MS)) {   /* the throw that ended a turn */
        int up = upside_of(shown.player);
        for (int i = 0; i < FTO_DICE; i++) {
            float x = die_x(i), y = DICE_Y, deg = cup_tilt(shown.key, i);
            if (shown.mask >> i & 1) ui_die_thrown(SX(x, up), SY(y, up), DIE * S, shown.player, shown.dice[i],
                                                    deg + (up ? 180 : 0), i, t, 1);
            else ui_die(SX(x, up), SY(y, up), DIE * S, shown.player, shown.dice[i], up ? 180 : 0, 0, 1, 1);
        }
        ui_animate();
        return;
    }
    if (P->phase == FTO_OVER) return;
    int me = P->turn, up = upside_of(me);
    int can_throw = P->throws < P->limit && P->kept != 7;
    for (int k = 0; k < P->limit; k++) {        /* the throws this round allows */
        float x = 0.5f + (k - (P->limit - 1) / 2.0f) * 0.07f;
        int left = k >= P->throws;
        if (left && can_throw) ui_mark_gold(SX(x, up), SY(PIPS_Y, up), 0.085f * S);
        ui_die(SX(x, up), SY(PIPS_Y, up), 0.045f * S, me, 6 - k, up ? 180 : 0, 0, left ? 1 : 0.28f, 1);
    }
    if (!P->throws) {                           /* waiting to be thrown */
        static const int idle[FTO_DICE] = {4, 2, 1};
        for (int i = 0; i < FTO_DICE; i++)
            ui_die(SX(die_x(i), up), SY(DICE_Y, up), DIE * S, me, idle[i], up ? 180 : 0, 0, 0.9f, 1);
        return;
    }
    for (int i = 0; i < FTO_DICE; i++) {
        int kept = P->kept >> i & 1;
        float x = die_x(i), y = kept ? KEPT_Y : DICE_Y, deg = kept ? 0 : cup_tilt(shown.key, i);
        if (slide_mask >> i & 1) {
            float f = (float)(now - slide_t0[i]) / CUP_SLIDE_MS;
            if (f < 1) {
                y = cup_mix(kept ? DICE_Y : KEPT_Y, y, f);
                ui_animate();
            }
        }
        if (shown.on && (shown.mask >> i & 1) && t < UI_DICE_MS)
            ui_die_thrown(SX(x, up), SY(y, up), DIE * S, me, P->dice[i], deg + (up ? 180 : 0), i, t, 1);
        else ui_die(SX(x, up), SY(y, up), DIE * S, me, P->dice[i], deg + (up ? 180 : 0), 0, 1, 1);
    }
    if (P->limit > 1) cup_button_at(CUP_STOP, SX(STOP_X, up), SY(STOP_Y, up), BTN * S, fto_can_stop(P), up ? 180 : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (P->phase == FTO_OVER || (shown.on && ui_ms() - shown.t0 < UI_DICE_MS) || holding()) return;
    int up = upside_of(P->turn);
    float u = (x - bx) / S, v = (y - by) / S;
    if (up) { u = 1 - u; v = 1 - v; }
    if (u < IN0 || u > IN1 || v < IN0 || v > IN1) { ui_sound(SND_DENY); return; }
    if (P->limit > 1 && hypotf(u - STOP_X, v - STOP_Y) < 0.6f * BTN) { stop(); return; }
    if (P->throws)
        for (int i = 0; i < FTO_DICE; i++) {
            float dy = P->kept >> i & 1 ? KEPT_Y : DICE_Y;
            if (fabsf(u - die_x(i)) < 0.6f * DIE && fabsf(v - dy) < 0.6f * DIE) { toggle_keep(i); return; }
        }
    if (P->throws < P->limit) throw_dice();
    else ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ui_bar_count(b, cup_chip((int)(0.6f * ui.unit)), P->chips[player]); }

static void token(int player, int size, uint8_t *rgba) { cup_token(player, size, rgba); }

/* the menu's card: 4-2-1 on the felt, chips beside */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    cup_board(rgba, size);
    cup_well(rgba, size, IN0, IN0, IN1, IN1);
    int c = (int)(0.2f * size);
    uint8_t *b = malloc((size_t)c * c * 4);
    if (b) {
        cup_chip_pic(c, b);
        static const float at[5][2] = {{0.2f, 0.72f}, {0.33f, 0.78f}, {0.27f, 0.62f}, {0.42f, 0.68f}, {0.16f, 0.84f}};
        for (int k = 0; k < 5; k++) art_blend(rgba, size, size, b, c, c, (int)(at[k][0] * size) - c / 2,
                                              (int)(at[k][1] * size) - c / 2);
        free(b);
    }
    static const int faces[3] = {4, 2, 1};
    for (int i = 0; i < 3; i++)
        art_die_blend(rgba, size, size, (0.3f + 0.22f * i) * size, (0.32f + (i == 1 ? 0.04f : 0)) * size,
                      (int)(0.22f * size), 0, faces[i], (float)(i * 17 - 15));
}

static const game fourtwoone_game = {
    .id = "fourtwoone", .order = 155, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1u << 2 | 1u << 3 | 1u << 4 | 1u << 5 | 1u << 6, .home = home,
};
REGISTER_GAME(fourtwoone_game)
