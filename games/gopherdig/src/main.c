/*
 * Gopher Dig: the game flow (the title with players 2-4 joining, play, game over, pause), input (pads and touch), sound,
 * save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A (B
 * leaves); P1 starts the run at once with A (or Start, or Down); one press on the game-over panel (hu_over_back) goes
 * BACK TO THE TITLE, with the same players.
 * The pads: Left / Right walk (and, pushed against a one-block step, climb it); A drills where the gopher faces: the
 * side it walked to, or down after Down (Down + A drills down at once); A held drills again every DRILL_REPEAT frames.
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Controls"): a finger on a block next to the
 * gopher (left, right) or under it drills it (held: again and again); a finger elsewhere left or right of it walks the
 * gopher to that column (climbing steps on the way); a tap starts from the title and leaves the game-over panel.
 * What the fingers set is kept in a registered state (touch_*): a load forgets the fingers, one still down starts again.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_think); botruns=N runs; botstop=D: from D metres (P2-P4: D+20,
 *                  D+40..) it gives up and stands still, its air running out 10 times as fast (the tests' game
 *                  over); botskill=S a human-like bot (1..10)
 *   seed=N         a fixed well (default: from the frame of the start)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   dump=1         log the final state at exit (tests); music=0, sound=0
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include "game_text.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define START_INPUTS  (RS_BTN_START | RS_BTN_DOWN)       /* A always starts too (the kit) */

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done, run_best;
static int start_press;                       /* P1's start press on the title: the run starts at once */
static int touch_hold_slot = -1, touch_hold_dir, touch_hold_t, touch_walk_slot = -1, touch_seen;
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best depth (house convention: checked, versioned, committed at each game over) ------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "GOPH";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x4744;
    for (size_t i = 0; i < offsetof(save_data, sum); i++) sum = (uint16_t)(sum * 31 + p[i]);
    return sum;
}

static void save_load(void)
{
    memcpy(&SV, rs_sram(), sizeof SV);
    if (memcmp(SV.magic, MAGIC, 4) || SV.version != 1 || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, MAGIC, 4);
        SV.version = 1;
    }
}

static void save_store(void)
{
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* ---- sound: the game's own sounds (the house synthesiser), then the house set -------------------------------------- */
static void sound_init(void)
{
    ha_synth s;
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x60fe2d16u);
    ha_hiss(&s, 0, 70, 30, 1);                              /* drill: a quick grinding buzz */
    ha_tone(&s, 0, 180, 140, 70, 34, 40);
    ha_tone(&s, 0, 360, 300, 60, 14, 30);
    ha_store(&s, SFX_DRILL, 80);
    ha_tone(&s, 0, 520, 1100, 60, 60, 30);                  /* pop: a bubbly "plop" up */
    ha_tone(&s, ha_ms(30), 900, 1500, 50, 24, 25);
    ha_store(&s, SFX_POP, 110);
    ha_tone(&s, 0, 780, 1560, 60, 54, 30);                  /* a chain: brighter, with a sparkle */
    ha_tone(&s, ha_ms(40), 1560, 2100, 70, 26, 40);
    ha_tone(&s, ha_ms(80), 2100, 2400, 60, 16, 40);
    ha_store(&s, SFX_CHAIN, 130);
    ha_tone(&s, 0, 1300, 1150, 40, 40, 20);                 /* the drill on a hard rock: a clank */
    ha_tone(&s, 0, 2100, 1900, 30, 20, 15);
    ha_hiss(&s, 0, 30, 18, 0);
    ha_store(&s, SFX_CLANK, 70);
    ha_hiss(&s, 0, 170, 50, 2);                             /* the rock crumbles */
    ha_tone(&s, 0, 220, 90, 150, 50, 70);
    ha_store(&s, SFX_CRUMBLE, 180);
    ha_tone(&s, 0, 300, 900, 120, 40, 60);                  /* air: a rising "gloop" and bubbles */
    for (int i = 0; i < 3; i++) ha_tone(&s, ha_ms(60 + i * 45), 900 + i * 260, 1300 + i * 260, 40, 20, 20);
    ha_store(&s, SFX_AIR, 190);
    ha_hiss(&s, 0, 170, 22, 4);                             /* a loose group shakes: a low rumble */
    ha_tone(&s, 0, 70, 60, 170, 30, 0);
    ha_store(&s, SFX_RUMBLE, 180);
    ha_tone(&s, 0, 160, 90, 80, 46, 40);                    /* blocks land: a soft thud */
    ha_hiss(&s, 0, 50, 16, 3);
    ha_store(&s, SFX_LAND, 90);
    ha_tone(&s, 0, 900, 700, 20, 12, 10);                   /* a step */
    ha_store(&s, SFX_STEP, 30);
    ha_tone(&s, 0, 200, 70, 200, 70, 90);                   /* crushed: a squish and a thud */
    ha_hiss(&s, 0, 120, 40, 2);
    ha_tone(&s, ha_ms(90), 700, 300, 200, 26, 90);
    ha_store(&s, SFX_CRUSH, 200);
    ha_tone(&s, 0, 660, 160, 230, 46, 110);                 /* out of air: a sad deflating whistle */
    ha_hiss(&s, 0, 200, 14, 4);
    ha_store(&s, SFX_NOAIR, 240);
    ha_tone(&s, 0, 1760, 1760, 50, 30, 0);                  /* low air: a beep-beep */
    ha_tone(&s, ha_ms(90), 1760, 1760, 50, 30, 0);
    ha_store(&s, SFX_LOW, 130);
    ha_hiss(&s, 0, 50, 12, 1);                              /* a drill into nothing: a whoosh */
    ha_store(&s, SFX_WHIFF, 60);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx_at(int id, int x, int vol) { ha_play_slot(id, vol, x, id == SFX_CHAIN || id == SFX_AIR); }

static int pan_of(int p)
{
    if (W.players == 1) return RS_SCREEN_W / 2;
    return (p % 2) ? RS_SCREEN_W * 3 / 4 : RS_SCREEN_W / 4;
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void touch_reset(void)
{
    touch_hold_slot = touch_walk_slot = -1;
    touch_hold_t = 0;
}

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    W.started = state == ST_PLAY && !rs_option_int("ready", 0);
    new_best = 0;
    touch_reset();
    draw_new_run(&W);
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

/* ---- touch (player 1) ------------------------------------------------------------------------------------------- */
static int touch_lifted(int since)
{
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (t && t->released && t->frames < since) return 1;
    }
    return 0;
}

/* the cell of player 1's well under a screen point */
static void touch_cell(int sx, int sy, int *col, int *row)
{
    vrect v[MAX_PLAYERS];
    view_layout(W.players > 1 ? W.players : 1, v);
    int x = sx - rs_screen_origin_x(), y = sy - rs_screen_origin_y();   /* (stage coordinates: a tall screen too) */
    int wx = view_well_x(&v[0]);
    int dx = x - wx, dy = y - v[0].y + draw_cam_y(0);
    *col = dx >= 0 ? dx / CELL : -1 - (-dx - 1) / CELL;
    *row = dy >= 0 ? dy / CELL : -1 - (-dy - 1) / CELL;
}

static void touch_read(pin *in)
{
    const gopher *g = &W.w[0].go;
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (!t || !t->down) {
            if (touch_hold_slot == i) touch_hold_slot = -1;
            if (touch_walk_slot == i) touch_walk_slot = -1;
            continue;
        }
        touch_seen = 1;
        in->any = 1;
        int col, row;
        touch_cell(t->x, t->y, &col, &row);
        if (t->pressed) {
            int dx = col - g->col, dy = row - g->row;
            int side_block = (dx == 1 || dx == -1) && dy == 0 && well_kind(&W.w[0], col, row) > K_EMPTY;
            if (dx == 0 && dy >= 1) {
                in->tap_drill = 3;
            } else if (side_block) {
                in->tap_drill = (int8_t)(dx < 0 ? 1 : 2);
            } else if (dx != 0) {
                in->walk_to = (int8_t)(col < 0 ? 0 : col >= COLS ? COLS - 1 : col);
                touch_walk_slot = i;
                continue;
            } else {
                continue;
            }
            touch_hold_slot = i;
            touch_hold_dir = in->tap_drill;
            touch_hold_t = 0;
        } else if (touch_hold_slot == i) {
            if (++touch_hold_t % DRILL_REPEAT == 0) in->tap_drill = (int8_t)touch_hold_dir;
        } else if (touch_walk_slot == i && t->moved) {
            in->walk_to = (int8_t)(col < 0 ? 0 : col >= COLS ? COLS - 1 : col);
        }
    }
}

static pin player_input(int p)
{
    pin in;
    memset(&in, 0, sizeof in);
    in.walk_to = -1;
    if (p == 0 && start_press) {
        in.any = 1;
        return in;                              /* the start press only starts */
    }
    if (is_bot(p)) {
        if (opt_botstop && W.w[p].go.depth >= opt_botstop + 20 * p) {
            /* it gives up: stands still, its air running out ten times as fast (the tests' game over: in seconds,
             * not a whole tank) */
            gopher *g = &W.w[p].go;
            int32_t gasp = 9 * stratum_param(g->stratum, P_DRAIN);
            g->air = g->air > gasp ? g->air - gasp : 0;
            in.any = 1;
            return in;
        }
        bot_think(&W, p, opt_botskill, &in);
        return in;
    }
    int pad = hu_player_pad(p);
    uint16_t held = rs_pad(pad), pr = rs_pad_pressed(pad);
    in.dir = (int8_t)((held & RS_BTN_RIGHT ? 1 : 0) - (held & RS_BTN_LEFT ? 1 : 0));
    in.down = (held & RS_BTN_DOWN) != 0;
    in.drill = (pr & RS_BTN_A) != 0;
    in.drill_hold = (held & RS_BTN_A) != 0;
    if (held & (RS_BTN_A | RS_BTN_B | HU_IN_DPAD)) in.any = 1;
    if (p == 0) touch_read(&in);
    return in;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_DEPTHS;
    int best_now = 0;
    for (int p = 0; p < W.players; p++)
        if (W.w[p].go.depth > best_now) best_now = W.w[p].go.depth;
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.w[0].go.depth, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (best_now > run_best) { run_best = best_now; new_best = 1; }
    if ((uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
    if (W.players >= 2 && (uint32_t)best_now > SV.best_multi) SV.best_multi = (uint32_t)best_now;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: depth %d (best %u)", runs_done, rs_frame_count(), W.w[0].go.depth, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void hash_world(void)
{
    for (int p = 0; p < W.players; p++) {
        const gopher *g = &W.w[p].go;
        state_hash = (state_hash ^ (uint32_t)(g->x * 7 + g->y * 131)) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(g->air + g->depth * 3 + g->alive * 17 + g->pops * 101)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)W.t) * 16777619u;
}

static void play_update(void)
{
    pin in[MAX_PLAYERS];
    int any = 0;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        memset(&in[p], 0, sizeof in[p]);
        in[p].walk_to = -1;
        if (p < W.players && W.w[p].go.alive) in[p] = player_input(p);
        any |= in[p].any;
    }
    start_press = 0;
    if (!W.started) {                           /* ready=1: the run waits for its first press */
        if (!any) return;
        W.started = 1;
    }
    world_step(&W, in);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p], x = pan_of(p);
        if (ev & EV_CHAIN) sfx_at(SFX_CHAIN, x, HA_VOL_FEEDBACK + 10);
        else if (ev & EV_POP) sfx_at(SFX_POP, x, HA_VOL_ACTION);
        if (ev & EV_HARD_HIT) sfx_at(SFX_CLANK, x, HA_VOL_ACTION);
        if (ev & EV_HARD_BREAK) sfx_at(SFX_CRUMBLE, x, HA_VOL_ACTION);
        if ((ev & EV_DRILL) && !(ev & (EV_POP | EV_HARD_HIT | EV_HARD_BREAK))) sfx_at(SFX_DRILL, x, 60);
        if (ev & EV_WHIFF) sfx_at(SFX_WHIFF, x, 40);
        if (ev & EV_AIR) sfx_at(SFX_AIR, x, HA_VOL_FEEDBACK);
        if (ev & EV_SHAKE) sfx_at(SFX_RUMBLE, x, 50);
        if (ev & EV_LAND) sfx_at(SFX_LAND, x, 50);
        if (ev & (EV_STEP | EV_CLIMB)) sfx_at(SFX_STEP, x, 30);
        if (ev & EV_LOW) sfx_at(SFX_LOW, x, 50);
        if (ev & EV_CHECKPOINT) {
            ha_play(HA_MEDAL);
            ha_play(HA_DING);
            rs_log("player %d reached %d m at frame %u", p + 1, W.w[p].go.depth, rs_frame_count());
        }
        if (ev & (EV_CRUSH | EV_NOAIR)) {
            sfx_at(ev & EV_CRUSH ? SFX_CRUSH : SFX_NOAIR, x, HA_VOL_CRASH);
            if (ev & EV_CRUSH) hu_shake(3, 12);  /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d %s at frame %u (depth %d)", p + 1, ev & EV_CRUSH ? "crushed" : "out of air",
                   rs_frame_count(), W.w[p].go.depth);
        }
    }
    hash_world();
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && st_t >= PANEL_DELAY) game_over();
}

/* the title: players join and leave (the kit's lobby), P1 starts (A, a start input, or a tap) */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) ha_play(HA_CONFIRM);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) W.players = hu_players();
    if ((ev & HU_TITLE_START) || touch_lifted(st_t) || (opt_bot && st_t >= BOT_START)) {
        ha_play(HA_CONFIRM);
        new_run(ST_PLAY);
        W.started = 1;
        start_press = 1;                        /* the run starts at once */
        play_update();
    }
}

static void game_update(void)
{
    ha_music("music/tune.mod", opt_music);
    if ((st == ST_PLAY || st == ST_DEAD) && (rs_pad_pressed(0) & (RS_BTN_SELECT | RS_BTN_START))) {
        paused = !paused;
        ha_play(HA_PAUSE);
    }
    if (paused) {
        touch_reset();                          /* the fingers do nothing while paused */
        return;
    }
    hu_shake_step();
    switch (st) {
    case ST_TITLE:
        title_update();
        break;
    case ST_PLAY:
    case ST_DEAD:
        play_update();
        break;
    case ST_OVER: {
        int back = 0;
        if (st_t >= RETRY_LOCK) {
            back = hu_over_back();              /* house rule: after a game over, back to the title (the only menu) */
            if (touch_lifted(st_t - RETRY_LOCK)) back = 1;   /* a tap that began after the lock */
            if (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10) back = 1;
        }
        if (back) {
            ha_play(HA_SELECT);
            new_run(ST_TITLE);
        }
        break;
    }
    }
    st_t++;
}

static void game_draw(void) { draw_frame(&W, st, st_t, run_best, new_best, paused); }

static void game_init(void)
{
    rs_screen_tall(RS_SCREEN_H_MAX);          /* a phone held upright: the well above, the earth below (DESIGN.md) */
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: A always starts (and Start, Down), up to 4 players: PRESS A TO DIG */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[64] = "", os[32] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(os);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.w[p].go.depth);
        snprintf(os + b, sizeof os - b, "%s%d", p ? "," : "", W.w[p].go.out);
    }
    const gopher *g = &W.w[0].go;
    rs_log("state: st=%d players=%d score=%d best=%u runs=%d out=%s col=%d row=%d air=%d pops=%d hards=%d airs=%d "
           "stratum=%d t=%d paused=%d touch=%d hash=%08x scores=%s",
           st, W.players, g->depth, SV.best, runs_done, os, g->col, g->row, g->air, g->pops, g->hards, g->airs,
           g->stratum, W.t, paused, touch_seen, state_hash, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press); S(state_hash);
    S(touch_hold_slot); S(touch_hold_dir); S(touch_hold_t); S(touch_walk_slot); S(touch_seen);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Gopher Dig", "gopherdig", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
