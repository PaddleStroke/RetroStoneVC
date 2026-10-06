/*
 * Mole Marbles: the game flow (the title with players 2-4 joining, play, game over, pause), input, sound, save RAM,
 * options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/molemarbles/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A
 * (B leaves); P1 starts the run at once with A (or an extra START INPUT; this press is also its first move); one press
 * on the game-over panel (hu_over_back) goes BACK TO THE TITLE, with the same players: the title is the only menu.
 * Touch (player 1, DESIGN.md "Touch"): the mole follows the finger, a quick tap shoots; a tap starts and goes back.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_decide); botruns=N runs; botstop=S stops at S (P2-P4: S+2, S+4..)
 *   seed=N         a fixed course (default: from the frame of the first press)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   dump=1         log the final state at exit (tests); music=0, sound=0
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include "house_audio.h"
#include <stdio.h>
#include <string.h>

/* the house buttons: A (and B, X, Y, Up) acts and starts (the start inputs), Select (or Start in a run) pauses */
#define ACT_BUTTONS RS_BTN_A
#define START_INPUTS RS_BTN_START

static int opt_bot, opt_botstop, opt_botruns, opt_seed_fixed, opt_music, opt_sound;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done, run_best;
static int start_press;                       /* P1's start press on the title: also its first move */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) -------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "MOLE";      /* 4 letters of the game (tools/new_game.py) */

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x5153;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x5eed1234u);
    ha_tone(&s, 0, 300, 700, 90, 60, 50);       /* harpoon: a quick rising pluck */
    ha_hiss(&s, 0, 40, 8, 3);
    ha_store(&s, SFX_JUMP, 110);
    ha_store(&s, SFX_SCORE, ha_make(&s, HA_DING));
    ha_store(&s, SFX_HIT, ha_make(&s, HA_THUD));
    ha_hiss(&s, 0, 60, 20, 3);                  /* land: a soft scuff */
    ha_store(&s, SFX_LAND, 70);
    ha_init(SFX_HOUSE);                         /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx_at(int id, int x)
{
    static const uint8_t vol[SFX_COUNT] = {HA_VOL_ACTION, HA_VOL_FEEDBACK, HA_VOL_CRASH, 50};
    ha_play_slot(id, vol[id], x, id == SFX_SCORE);
}

/* The ordinary-input dodge/shoot bot lives with the rules in play.c. */
/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }
static void touch_reset(void);
static int arena_left(void), arena_right(void);

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    world_arena(&W, arena_left(), arena_right());   /* widescreen: the whole screen */
    new_best = 0; run_best = SV.best;
    touch_reset();
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

/* ---- touch (player 1; docs/spec.md "Touch", DESIGN.md "Touch"): the mole walks at its normal speed to the finger's
 * x and stops exactly there (world goal_x: the last point stays the goal when the finger is lifted, so a tap also
 * sends it there); a quick tap shoots ------------------------------------------------------------------------------ */
#define TOUCH_FIRE  12                        /* frames a tap's shot waits for the harpoon to be ready */
static int touch_fire;                        /* a tap's shot, still to fire (frames left) */

static void touch_reset(void) { touch_fire = 0; W.goal_on[0] = 0; }
/* a screen x as the x the mole can reach */
static int arena_x(int screen_x)
{
    int x = screen_x - rs_screen_origin_x();
    return x < W.left + 12 ? W.left + 12 : x > W.right - 12 ? W.right - 12 : x;
}

/* P1's goal from the fingers; returns the buttons the touches press this frame (A: a tap's shot; 0 without touches) */
static int touch_input(void)
{
    const rs_touch_point *t = rs_touch_first();
    int x;
    if (t) W.goal_on[0] = 1, W.goal_x[0] = arena_x(t->x);
    if (rs_touch_tap(&x, NULL)) {
        W.goal_on[0] = 1;
        W.goal_x[0] = arena_x(x);
        touch_fire = TOUCH_FIRE;
    }
    if (W.goal_on[0] && !t && W.h[0].x == W.goal_x[0]) W.goal_on[0] = 0;   /* there, the finger lifted: done */
    if (rs_pad(hu_player_pad(0)) & (RS_BTN_LEFT | RS_BTN_RIGHT)) W.goal_on[0] = 0;   /* the pad steers: it wins */
    if (!touch_fire) return 0;
    touch_fire--;
    return RS_BTN_A;
}

/* the arena's walls: the whole screen (widescreen: inside the mine timbers drawn at its edges; 320: the edges) */
#define WALL 6
static int arena_left(void) { return rs_screen_width() > RS_SCREEN_W ? rs_screen_left() + WALL : 0; }
static int arena_right(void) { return rs_screen_width() > RS_SCREEN_W ? rs_screen_right() - WALL : RS_SCREEN_W; }

static int act_pressed(int p)
{
    if (p == 0 && start_press) return RS_BTN_A | (rs_pad(hu_player_pad(p)) & (RS_BTN_LEFT | RS_BTN_RIGHT));
    if (is_bot(p)) {
        if (opt_botstop && W.h[p].score >= opt_botstop + 2 * p && st == ST_PLAY) return 0;   /* P2-P4: +2 each */
        return bot_decide(&W, p);
    }
    return (rs_pad(hu_player_pad(p)) & (RS_BTN_A | RS_BTN_LEFT | RS_BTN_RIGHT)) | (p == 0 ? touch_input() : 0);
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int best_now = 0;
    for (int p = 0; p < W.players; p++)
        if (W.h[p].score > best_now) best_now = W.h[p].score;
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.h[0].score, medals) : 0;
    if (m) SV.medals[m - 1]++;
    new_best = best_now > run_best && best_now > 0;     /* beats the best before this run */
    if (best_now > SV.best) SV.best = (uint16_t)best_now;
    if (W.players >= 2 && best_now > SV.best_multi) SV.best_multi = (uint16_t)best_now;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d (best %d)", runs_done, rs_frame_count(), W.h[0].score, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void play_update(void)
{
    int press[MAX_PLAYERS] = {0};
    for (int p = 0; p < W.players; p++) {
        const hero *h = &W.h[p];
        if (h->state == HS_READY || h->state == HS_RUN) press[p] = act_pressed(p);
    }
    start_press = 0;
    world_step(&W, press);
    if (W.events[0] & EV_JUMP) touch_fire = 0;         /* a tap's shot fired */
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const hero *h = &W.h[p];
        if (ev & EV_JUMP) sfx_at(SFX_JUMP, h->x);
        if (ev & EV_SCORE) sfx_at(SFX_SCORE, h->x);
        if (ev & EV_LAND) sfx_at(SFX_LAND, h->x);
        if (ev & EV_HIT) {
            sfx_at(SFX_HIT, h->x);
            hu_shake(3, 12);                    /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d hit at frame %u (score %d)", p + 1, rs_frame_count(), h->score);
        }
        state_hash = (state_hash ^ (uint32_t)h->y) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(h->score * 131 + h->state)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)W.scroll) * 16777619u;
    if (!W.started) return;                     /* ready=1: the run waits for its first press */
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && world_all_down(&W)) {
        int t = 0;
        for (int p = 0; p < W.players; p++) if (W.h[p].t > t) t = W.h[p].t;
        if (t >= PANEL_DELAY) game_over();
    }
}

/* the title: players join and leave (the kit's lobby), P1 starts */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) ha_play(HA_CONFIRM);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) {
        int t = st_t;
        new_run(ST_TITLE);                      /* the world follows the players (the title shows their heroes) */
        st_t = t;
    }
    if ((ev & HU_TITLE_START) || rs_touch_tap(NULL, NULL) || (opt_bot && st_t >= BOT_START)) {   /* a tap starts too */
        ha_play(HA_CONFIRM);
        go(ST_PLAY);
        start_press = 1;                        /* the start press is also P1's first move */
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
    if (paused) return;
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
            back = hu_over_back() || rs_touch_tap(NULL, NULL);   /* house rule: back to the title (the only menu); a tap too */
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

/* the best shown: the one before the run on the title, the one after it on the game-over panel (a new best = the score) */
static int shown_best(void) { return st == ST_OVER ? SV.best : run_best; }
static void game_draw(void) { draw_frame(&W, st, st_t, shown_best(), new_best, paused); }

static void game_init(void)
{
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (the game's natural play inputs: the prompt shows them), up to 4 players */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[48] = "", hs[48] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(hs);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.h[p].score);
        snprintf(hs + b, sizeof hs - b, "%s%d", p ? "," : "", W.h[p].state);
    }
    rs_log("state: st=%d players=%d score=%d score2=%d best=%d runs=%d hs=%s x=%d y=%d scroll=%d paused=%d hash=%08x "
           "arena=%d,%d shot=%d panel=%d record=%d scores=%s",
           st, W.players, W.h[0].score, W.h[1].score, SV.best, runs_done, hs, W.h[0].x, (int)(W.h[0].y >> 16), (int)W.scroll,
           paused, state_hash, W.left, W.right, W.h[0].shot, shown_best(), new_best, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
/* after a state was loaded: one saved at another width brings that width's maps and arena (widescreen: the arena is
 * the whole screen): this width's are set up, the marbles and moles kept inside the walls */
static void game_state_loaded(void)
{
    draw_relayout();
    if (W.left != arena_left() || W.right != arena_right()) world_arena(&W, arena_left(), arena_right());
}

#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(run_best); S(runs_done); S(start_press); S(state_hash);
    S(touch_fire);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_seed_fixed); S(opt_seed); S(opt_music); S(opt_sound);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md) */
    static const rs_game g = {"Mole Marbles", "molemarbles", "1.0.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, game_state_loaded, 1, RS_SCREEN_W_MAX,
                              1 /* touch_native: DESIGN.md "Touch" */};
    return &g;
}




