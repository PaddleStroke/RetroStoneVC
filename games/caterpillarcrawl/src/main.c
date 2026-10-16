/*
 * Caterpillar Crawl: the game flow (the title with players 2-4 joining, play, game over, pause), input (pads and touch),
 * sound, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A
 * (B leaves); P1 starts at once with A (or the d-pad: that press is also its first turn); one press on the game-over
 * panel (hu_over_back) goes BACK TO THE TITLE, with the same players. 1 player: the garden; 2-4: the arena.
 * The pads: a d-pad press turns the caterpillar (buffered: two quick presses make a U-turn in two steps; never straight
 * back into itself). Select (or Start in a run) pauses.
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Controls"): a swipe turns the caterpillar
 * that way (buffered like the d-pad; a finger kept down steers on: every TOUCH_SWIPE_PX it moves mostly along one axis
 * is a turn that way); a tap starts from the title and leaves the game-over panel.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_decide); botruns=N runs;
 *                  botstop=S: once its score reaches S it crawls into the first wall (the tests' game over)
 *   botskill=E     a human-like bot (1..10: worse and worse), to tune the medals
 *   seed=N         a fixed run (default: from the frame of the start)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   garden=N       1 player: start in garden N (1..; screenshots, tests); leaves=N: N of its leaves already eaten
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

/* the title's start inputs: A (always), Start and the d-pad (its press is also the first turn) */
#define DPAD          (RS_BTN_UP | RS_BTN_DOWN | RS_BTN_LEFT | RS_BTN_RIGHT)
#define START_INPUTS  (RS_BTN_A | RS_BTN_START | HU_IN_DPAD)

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound, opt_garden;
static int opt_leaves;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static int start_press;                       /* P1's start press on the title (PRESS_GO, or its first turn) */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */
/* touch: each finger's anchor (where it last turned the caterpillar), by slot */
static int16_t touch_ax[RS_TOUCH_MAX], touch_ay[RS_TOUCH_MAX];
static uint8_t touch_on[RS_TOUCH_MAX];

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) -------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best, runs;
    uint16_t medals[4];
    uint16_t best_garden, butterflies;
    uint16_t matches, pad2;
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "CATC";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x4343;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x5eedc47u);
    /* the lengths keep the sample memory under the 64 KiB guideline with the house set */
    ha_hiss(&s, 0, 45, 30, 2);                              /* chomp: a crunchy munch, two bites */
    ha_tone(&s, 0, 520, 300, 40, 40, 20);
    ha_hiss(&s, ha_ms(70), 40, 24, 2);
    ha_tone(&s, ha_ms(70), 470, 280, 40, 32, 20);
    ha_store(&s, SFX_CHOMP, 130);
    ha_tone(&s, 0, 1200, 1000, 18, 16, 10);                 /* a turn: the softest tick */
    ha_store(&s, SFX_TURN, 30);
    static const int fl[3] = {1568, 1976, 2637};            /* a flower: G6 B6 E7, a sparkly chime */
    for (int i = 0; i < 3; i++) ha_tone(&s, ha_ms(i * 45), fl[i], fl[i], 90, 30, 60);
    ha_store(&s, SFX_FLOWER, 200);
    ha_tone(&s, 0, 880, 330, 260, 40, 120);                 /* a berry: time slows, a long "wooo" down */
    ha_tone(&s, 0, 1760, 660, 200, 12, 80);
    ha_store(&s, SFX_BERRY, 300);
    ha_tone(&s, 0, 300, 140, 160, 56, 80);                  /* bonk: a soft thud and a wobble */
    ha_tone(&s, ha_ms(50), 620, 520, 120, 18, 60);
    ha_hiss(&s, 0, 80, 30, 2);
    ha_store(&s, SFX_BONK, 220);
    ha_tone(&s, 0, 440, 660, 300, 22, 150);                 /* the cocoon: a soft rising hum */
    ha_store(&s, SFX_COCOON, 320);
    static const int up[5] = {1047, 1319, 1568, 2093, 2637};   /* the butterfly: C6 E6 G6 C7 E7, a little fanfare */
    for (int i = 0; i < 5; i++) ha_tone(&s, ha_ms(i * 55), up[i], up[i], 120, 30, 70);
    ha_hiss(&s, ha_ms(200), 120, 10, 1);
    ha_store(&s, SFX_BUTTERFLY, 420);
    ha_tone(&s, 0, 900, 1500, 45, 26, 20);                  /* pop: a treat sprouts */
    ha_store(&s, SFX_POP, 70);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx_cell(int id, int cell, int vol)
{
    int x = GRID_X + (cell % GRID_W) * CELL + CELL / 2;
    ha_play_slot(id, vol, x, id == SFX_FLOWER || id == SFX_BUTTERFLY);
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    if (opt_garden > 1 && W.mode == MODE_SOLO) world_start_garden(&W, opt_garden - 1);
    if (opt_leaves > 0 && opt_leaves < LEAVES_PER_GARDEN && W.mode == MODE_SOLO) W.garden_leaves = opt_leaves;
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

/* ---- touch (player 1) ------------------------------------------------------------------------------------------- */
/* A finger lifted this frame without moving that went down during the last `since` frames: the touch screen's "A"
 * (a finger still down from the run before never counts). */
static int touch_tapped(int since)
{
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (t && t->released && !t->moved && t->frames < since) return 1;
    }
    return 0;
}

/* a swipe: the direction a finger moved TOUCH_SWIPE_PX from its anchor, mostly along one axis (-1: none) */
static int touch_swipe(void)
{
    int dir = -1;
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (!t) {
            touch_on[i] = 0;
            continue;
        }
        if (t->pressed || !touch_on[i]) {
            touch_on[i] = 1;
            touch_ax[i] = t->pressed ? t->x0 : t->x;
            touch_ay[i] = t->pressed ? t->y0 : t->y;
        }
        int dx = t->x - touch_ax[i], dy = t->y - touch_ay[i];
        int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        if ((ax >= TOUCH_SWIPE_PX || ay >= TOUCH_SWIPE_PX) && (ax >= 2 * ay || ay >= 2 * ax)) {
            dir = ax > ay ? (dx > 0 ? DIR_RIGHT : DIR_LEFT) : (dy > 0 ? DIR_DOWN : DIR_UP);
            touch_ax[i] = t->x;
            touch_ay[i] = t->y;
        } else if (ax >= 3 * TOUCH_SWIPE_PX || ay >= 3 * TOUCH_SWIPE_PX) {   /* a slanted drag: start again here */
            touch_ax[i] = t->x;
            touch_ay[i] = t->y;
        }
        if (t->released) touch_on[i] = 0;
    }
    return dir;
}

static int pad_dir(uint16_t pressed)
{
    if (pressed & RS_BTN_UP) return DIR_UP;
    if (pressed & RS_BTN_DOWN) return DIR_DOWN;
    if (pressed & RS_BTN_LEFT) return DIR_LEFT;
    if (pressed & RS_BTN_RIGHT) return DIR_RIGHT;
    return -1;
}

static int player_press(int p, int swipe)
{
    if (is_bot(p)) {
        int stop = opt_botstop && W.mode == MODE_SOLO && W.c[p].score >= opt_botstop && st == ST_PLAY;
        int pr = bot_decide(&W, p, opt_botskill, stop);
        if (p == 0 && start_press && !pr) pr = PRESS_GO;
        return pr;
    }
    if (p == 0 && start_press) return start_press;
    uint16_t pressed = rs_pad_pressed(hu_player_pad(p));
    int d = pad_dir(pressed);
    if (p == 0 && d < 0) d = swipe;
    int pr = d >= 0 ? 1 + d : 0;
    if (pressed & RS_BTN_A) pr |= PRESS_GO;
    return pr;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int best_now = W.mode == MODE_SOLO ? W.c[0].score : 0;
    SV.runs++;
    int m = W.mode == MODE_SOLO ? hu_medal_of(W.c[0].score, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (W.mode == MODE_SOLO) {
        if (best_now > run_best) { run_best = best_now; new_best = 1; }
        if ((uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
        if (W.garden + 1 > SV.best_garden) SV.best_garden = (uint16_t)(W.garden + 1);
        SV.butterflies = (uint16_t)(SV.butterflies + W.c[0].butterflies);
    } else {
        SV.matches++;
    }
    save_store();
    runs_done++;
    if (W.mode == MODE_SOLO)
        rs_log("run %d over at frame %u: score %d garden %d leaves %d butterflies %d (best %u)", runs_done,
               rs_frame_count(), W.c[0].score, W.garden + 1, W.c[0].leaves, W.c[0].butterflies, SV.best);
    else
        rs_log("run %d over at frame %u: match of %d rounds, wins %d,%d,%d,%d", runs_done, rs_frame_count(), W.round,
               W.c[0].wins, W.c[1].wins, W.c[2].wins, W.c[3].wins);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void play_update(void)
{
    int press[MAX_PLAYERS] = {0};
    int swipe = st == ST_PLAY ? touch_swipe() : -1;
    for (int p = 0; p < W.players; p++) press[p] = player_press(p, swipe);
    start_press = 0;
    world_step(&W, press);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const cat *c = &W.c[p];
        int head = c->len > 0 ? cat_cell(c, 0) : 0;
        if (ev & EV_LEAF) sfx_cell(SFX_CHOMP, head, HA_VOL_ACTION);
        if (ev & EV_FLOWER) sfx_cell(SFX_FLOWER, head, HA_VOL_FEEDBACK);
        if (ev & EV_BERRY) sfx_cell(SFX_BERRY, head, HA_VOL_FEEDBACK);
        if ((ev & EV_TURN) && !is_bot(p)) sfx_cell(SFX_TURN, head, 30);
        if (ev & EV_WIN) ha_play(HA_DING);
        if (ev & EV_HIT) {
            sfx_cell(SFX_BONK, head, HA_VOL_CRASH);
            hu_shake(3, 12);                    /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d hit (%d) at frame %u (score %d, length %d)", p + 1, c->why, rs_frame_count(), c->score,
                   c->len);
        }
        state_hash = (state_hash ^ (uint32_t)(head * 977 + c->len * 31 + c->state)) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(c->score * 131 + c->wins * 7 + c->dir)) * 16777619u;
    }
    int wev = W.wevents;
    if (wev & WEV_COCOON) {
        sfx_cell(SFX_COCOON, W.meta_cell, HA_VOL_FEEDBACK);
        rs_log("the cocoon at frame %u (garden %d)", rs_frame_count(), W.garden + 1);
    }
    if (wev & WEV_BUTTERFLY) sfx_cell(SFX_BUTTERFLY, W.meta_cell, HA_VOL_FEEDBACK);
    if (wev & WEV_GARDEN) {
        ha_play(HA_SELECT);
        rs_log("garden %d at frame %u", W.garden + 1, rs_frame_count());
    }
    if (wev & WEV_ROUND) {
        ha_play(HA_SELECT);
        rs_log("round %d at frame %u", W.round, rs_frame_count());
    }
    if (wev & WEV_GO) ha_play(HA_CONFIRM);
    if (wev & WEV_TREAT) sfx_cell(SFX_POP, W.treat, 50);
    if (wev & WEV_ROUND_END) rs_log("round %d won by %d at frame %u", W.round, W.round_winner + 1, rs_frame_count());
    state_hash = (state_hash ^ (uint32_t)(W.leaf * 31 + W.treat * 7 + W.phase + (int)W.acc)) * 16777619u;
    if (!W.started) return;                     /* ready=1: the run waits for its first press */
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && world_over_t(&W) >= PANEL_DELAY) game_over();
}

/* the title: players join and leave (the kit's lobby), P1 starts (A, Start, the d-pad, or a tap) */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) ha_play(HA_CONFIRM);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) {
        int t = st_t;
        new_run(ST_TITLE);                      /* the world follows the players */
        st_t = t;
    }
    if ((ev & HU_TITLE_START) || touch_tapped(st_t) || (opt_bot && st_t >= BOT_START)) {
        int d = pad_dir(rs_pad_pressed(0));
        ha_play(HA_CONFIRM);
        new_run(ST_PLAY);
        start_press = PRESS_GO | (d >= 0 ? 1 + d : 0);   /* the start press: also the first turn */
        memset(touch_on, 0, sizeof touch_on);
        play_update();
    }
}

static void game_update(void)
{
    ha_music("music/tune.mod", opt_music);
    if (st == ST_PLAY && (rs_pad_pressed(0) & (RS_BTN_SELECT | RS_BTN_START))) {
        paused = !paused;
        ha_play(HA_PAUSE);
    }
    if (paused) {
        touch_swipe();                          /* a finger's moves while paused are forgotten */
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
            if (touch_tapped(st_t - RETRY_LOCK)) back = 1;   /* a tap that began after the lock */
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
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; Start and the d-pad too), up to 4 players: PRESS A TO CRAWL */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_garden = rs_option_int("garden", 1);
    opt_leaves = rs_option_int("leaves", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[48] = "", hs[48] = "", wn[48] = "", ln[48] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(hs), c = strlen(wn), d = strlen(ln);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.c[p].score);
        snprintf(hs + b, sizeof hs - b, "%s%d", p ? "," : "", W.c[p].state);
        snprintf(wn + c, sizeof wn - c, "%s%d", p ? "," : "", W.c[p].wins);
        snprintf(ln + d, sizeof ln - d, "%s%d", p ? "," : "", W.c[p].len);
    }
    const cat *c = &W.c[0];
    int head = c->len > 0 ? cat_cell(c, 0) : -1;
    rs_log("state: st=%d players=%d score=%d best=%u runs=%d hs=%s len=%s dir=%d hx=%d hy=%d garden=%d leaves=%d "
           "phase=%d round=%d wins=%s slow=%d paused=%d hash=%08x scores=%s",
           st, W.players, c->score, SV.best, runs_done, hs, ln, c->dir, head % GRID_W, head / GRID_W, W.garden + 1,
           c->leaves, W.phase, W.round, wn, W.slow_t, paused, state_hash, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press); S(state_hash);
    S(touch_ax); S(touch_ay); S(touch_on);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound); S(opt_garden); S(opt_leaves);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Caterpillar Crawl", "caterpillarcrawl", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
