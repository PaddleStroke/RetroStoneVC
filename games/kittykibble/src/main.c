/*
 * Kitty Kibble: the game flow (the title with players 2-4 joining, play, game over, pause), input (pads and touch),
 * sound, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A
 * (B leaves); P1 starts the run at once with A (or Start, or the d-pad: that press is also its first turn); one press
 * on the game-over panel (hu_over_back) goes BACK TO THE TITLE, with the same players.
 * The pads: the d-pad steers the kitten (a turn pressed before a junction is kept for it: play.c). Nothing else.
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Controls"): a swipe turns the kitten (the
 * very turn of the d-pad, buffered the same: a finger can swipe on and on without lifting); a tap (a finger lifted)
 * starts from the title and leaves the game-over panel. The swipe's anchor is in the world (saved by the states).
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_decide); botruns=N runs;
 *                  botstop=S: once its score reaches S (P2-P4: S+50, S+100..) it runs into a vacuum (the tests' game over)
 *   botskill=E     a human-like bot (1..10: worse and worse), to tune the medals
 *   seed=N         a fixed run (default: from the frame the title was shown)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   room=N         start in room N (the screenshots of the other rooms, tests)
 *   msg=K          (the language test) the run shows message K: 1 P2 CAUGHT!, 2 ROOM CLEAR!
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

/* the house buttons: A always starts (the kit adds it); Start and the d-pad start too; Select (or Start) pauses */
#define START_INPUTS  (RS_BTN_START | HU_IN_DPAD)

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound, opt_room;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static int start_press;                       /* P1's start press on the title: the run starts at once */
static int munch_k;                           /* the munch sound's pitch, alternating */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) -------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t best_room, pad2;
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "KITK";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x4b4b;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x6b6b6b6bu);
    /* the lengths keep the sample memory under the 64 KiB guideline with the house set */
    ha_tone(&s, 0, 760, 520, 45, 40, 30);                   /* munch: a little "nom" (two pitches, alternating) */
    ha_hiss(&s, 0, 25, 6, 2);
    ha_store(&s, SFX_MUNCH, 60);
    ha_tone(&s, 0, 260, 880, 120, 50, 60);                  /* yarn: a springy boing ... */
    ha_tone(&s, ha_ms(110), 880, 620, 120, 30, 80);         /* ... and its wobble */
    ha_store(&s, SFX_YARN, 200);
    ha_tone(&s, 0, 240, 190, 110, 30, 60);                  /* yarn time: the vacuums' worried "wub" */
    ha_store(&s, SFX_SCARED, 120);
    ha_tone(&s, 0, 980, 360, 50, 50, 30);                   /* a vacuum caught: a pop ... */
    ha_tone(&s, ha_ms(45), 600, 2400, 140, 26, 60);         /* ... and it zips away */
    ha_hiss(&s, 0, 50, 16, 2);
    ha_store(&s, SFX_CATCH, 180);
    ha_hiss(&s, 0, 140, 22, 1);                             /* the zip home: a whoosh */
    ha_store(&s, SFX_ZIP, 150);
    static const int up[3] = {1568, 2093, 2637};            /* the fish treat: G6 C7 E7 */
    for (int i = 0; i < 3; i++) ha_tone(&s, ha_ms(i * 45), up[i], up[i], 90, 30, 50);
    ha_store(&s, SFX_FISH, 210);
    ha_tone(&s, 0, 1100, 620, 220, 44, 120);                /* caught: a sad "mew" ... */
    ha_tone(&s, ha_ms(200), 700, 280, 200, 40, 120);        /* ... falling, and a bump */
    ha_hiss(&s, ha_ms(180), 90, 30, 2);
    ha_store(&s, SFX_CAUGHT, 330);
    static const int cl[4] = {1047, 1319, 1568, 2093};      /* the room clean: C6 E6 G6 C7 */
    for (int i = 0; i < 4; i++) ha_tone(&s, ha_ms(i * 70), cl[i], cl[i], 140, 32, 80);
    ha_store(&s, SFX_CLEAR, 320);
    ha_tone(&s, 0, 1400, 1400, 40, 22, 20);                 /* a vacuum leaves its dock: bleep-bloop */
    ha_tone(&s, ha_ms(50), 900, 900, 50, 22, 30);
    ha_store(&s, SFX_LEAVE, 110);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static int stage_x(int32_t qx) { return MAZE_X + (int)(qx >> 16); }

static void sfx_at(int id, int x, int vol)
{
    ha_play_slot(id, vol, x, id == SFX_FISH || id == SFX_CLEAR);
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    if (opt_room > 1) {
        world_start_room(&W, opt_room);
        W.phase = PH_READY;
        W.wevents = 0;
    }
    int msg = rs_option_int("msg", 0);          /* the language test: a message on screen */
    if (msg == 1 && W.players > 1) { W.msg = MSG_CAUGHT; W.msg_t = 1 << 20; W.msg_player = 1; }
    if (msg == 2) { W.phase = PH_CLEAR; W.started = 1; }
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

/* ---- touch (player 1) ------------------------------------------------------------------------------------------- */
/* A finger was lifted this frame that went down during the last `since` frames (on this screen, after its lock):
 * the touch screen's "press A" (a tap; a finger still down from the run before never counts). */
static int touch_lifted(int since)
{
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (t && t->released && t->frames < since) return 1;
    }
    return 0;
}

static uint8_t pad_dirs(uint16_t b)
{
    return (uint8_t)((b & RS_BTN_RIGHT ? DIR_BIT(DIR_RIGHT) : 0) | (b & RS_BTN_DOWN ? DIR_BIT(DIR_DOWN) : 0) |
                     (b & RS_BTN_LEFT ? DIR_BIT(DIR_LEFT) : 0) | (b & RS_BTN_UP ? DIR_BIT(DIR_UP) : 0));
}

static pin player_input(int p)
{
    pin in = {0, 0, 0};
    if (p == 0 && start_press) {
        in.start = 1;
        in.pressed = pad_dirs(rs_pad_pressed(hu_player_pad(0)));   /* a d-pad start is also the first turn */
    }
    if (is_bot(p)) {
        const kitten *k = &W.k[p];
        int skill = opt_botskill;
        if (opt_botstop && k->score >= opt_botstop + 50 * p && st == ST_PLAY && W.started) skill = -1;
        in.held = (uint8_t)bot_decide(&W, p, skill);
        return in;
    }
    int pad = hu_player_pad(p);
    in.held = pad_dirs(rs_pad(pad));
    in.pressed |= pad_dirs(rs_pad_pressed(pad));
    if (p == 0) {                               /* touch: a swipe step is a d-pad press */
        const rs_touch_point *t = rs_touch_first();
        int d = t ? touch_swipe_dir(&W, t->down, t->pressed, t->x, t->y) : touch_swipe_dir(&W, 0, 0, 0, 0);
        if (d != DIR_NONE) in.pressed |= (uint8_t)DIR_BIT(d);
    }
    return in;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int best_now = 0;
    for (int p = 0; p < W.players; p++)
        if (W.k[p].score > best_now) best_now = W.k[p].score;
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.k[0].score, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (best_now > run_best) { run_best = best_now; new_best = 1; }
    if ((uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
    if (W.room > SV.best_room) SV.best_room = (uint16_t)W.room;
    if (W.players >= 2 && (uint32_t)best_now > SV.best_multi) SV.best_multi = (uint32_t)best_now;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d room %d (best %u)", runs_done, rs_frame_count(), W.k[0].score, W.room,
           SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void play_update(void)
{
    pin in[MAX_PLAYERS];
    for (int p = 0; p < MAX_PLAYERS; p++) {
        pin none = {0, 0, 0};
        in[p] = p < W.players ? player_input(p) : none;
    }
    start_press = 0;
    world_step(&W, in);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const kitten *k = &W.k[p];
        int x = stage_x(k->x);
        if (ev & EV_MUNCH) {
            ha_play_pitched(SFX_MUNCH, 40, x, munch_k ? 0x1300 : 0x1000, 0);
            munch_k ^= 1;
        }
        if (ev & EV_YARN) {
            sfx_at(SFX_YARN, x, HA_VOL_ACTION);
            rs_log("player %d: yarn at frame %u (room %d)", p + 1, rs_frame_count(), W.room);
        }
        if (ev & EV_CATCH) {
            sfx_at(SFX_CATCH, x, HA_VOL_ACTION);
            rs_log("player %d caught a vacuum at frame %u (room %d)", p + 1, rs_frame_count(), W.room);
        }
        if (ev & EV_FISH) sfx_at(SFX_FISH, x, HA_VOL_FEEDBACK);
        if (ev & EV_CAUGHT) {
            sfx_at(SFX_CAUGHT, x, HA_VOL_CRASH);
            hu_shake(3, 12);                    /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d caught at frame %u (room %d, score %d)", p + 1, rs_frame_count(), W.room, k->score);
        }
        state_hash = (state_hash ^ (uint32_t)(k->x ^ (k->y << 3))) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(k->score * 131 + k->state * 7 + k->dir)) * 16777619u;
    }
    for (int i = 0; i < VACUUMS; i++)
        state_hash = (state_hash ^ (uint32_t)(W.v[i].x * 3 + W.v[i].y + W.v[i].state)) * 16777619u;
    int wev = W.wevents;
    if (wev & WEV_HOME) sfx_at(SFX_ZIP, 160, 40);
    if (wev & WEV_CLEAR) {
        sfx_at(SFX_CLEAR, 160, HA_VOL_FEEDBACK);
        rs_log("room %d clean at frame %u", W.room, rs_frame_count());
    }
    if (wev & WEV_ROOM) rs_log("room %d at frame %u", W.room, rs_frame_count());
    if (wev & WEV_FISH) ha_play(HA_DING);
    if (wev & WEV_CALM) rs_log("yarn time over at frame %u (room %d)", rs_frame_count(), W.room);
    if (W.phase == PH_PLAY && !W.freeze && world_yarn_time(&W) && W.t % 30 == 0) sfx_at(SFX_SCARED, 160, 36);
    for (int i = 0; i < VACUUMS; i++)
        if (W.v[i].state == VS_LEAVE && W.v[i].t == 0 && W.phase == PH_PLAY && !W.freeze)
            sfx_at(SFX_LEAVE, stage_x(W.v[i].x), 30);
    if (!W.started) return;                     /* ready=1: the run waits for its first press */
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && world_all_out(&W)) {
        int t = 0;
        for (int p = 0; p < W.players; p++) if (W.k[p].t > t) t = W.k[p].t;
        if (t >= PANEL_DELAY) game_over();
    }
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
    if ((ev & HU_TITLE_START) || touch_lifted(st_t) || (opt_bot && st_t >= BOT_START)) {
        ha_play(HA_CONFIRM);
        new_run(ST_PLAY);
        start_press = 1;                        /* the run starts at once: ROOM 1, KITCHEN */
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
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; Start and the d-pad too), up to 4 players: PRESS A TO PLAY */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, HTR(PLAY), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_room = rs_option_int("room", 1);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[64] = "", ks[48] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(ks);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.k[p].score);
        snprintf(ks + b, sizeof ks - b, "%s%d", p ? "," : "", W.k[p].state);
    }
    rs_log("state: st=%d players=%d score=%d score2=%d best=%u runs=%d ks=%s room=%d phase=%d left=%d x=%d y=%d "
           "dir=%d mode=%d yarn=%d catches=%d paused=%d hash=%08x scores=%s",
           st, W.players, W.k[0].score, W.k[1].score, SV.best, runs_done, ks, W.room, W.phase, W.kibble_left,
           (int)(W.k[0].x >> 16), (int)(W.k[0].y >> 16), W.k[0].dir, W.mode, world_yarn_time(&W), W.k[0].catches,
           paused, state_hash, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press); S(munch_k);
    S(state_hash); S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed);
    S(opt_music); S(opt_sound); S(opt_room);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Kitty Kibble", "kittykibble", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
