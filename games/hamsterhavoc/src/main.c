/*
 * Hamster Havoc: the game flow (the title with players 2-4 joining, play, game over, pause), input (pads and touch),
 * sound, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A
 * (B leaves); P1 starts the run at once with A (or an extra START INPUT); one press on the game-over panel
 * (hu_over_back) goes BACK TO THE TITLE, with the same players.
 * The pads: the d-pad moves the ship, A (B, X, Y too) fires, held: a seed as soon as the last one is gone.
 * Touch (player 1, DESIGN.md "Touch"): the game reads no touches (touch_native off). The phone apps turn them into
 * P1's pad (apps/launcher/games.cfg): the left / right half holds Left / Right, two drawn shoot buttons hold A. So a
 * tap starts from the title and leaves the game-over panel as a press of Left, Right or A does.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_decide); botruns=N runs;
 *                  botstop=S: once its score reaches S (P2-P4: S+100, S+200..) it stops dodging (the tests' game over)
 *   botskill=E     a human-like bot (1..10: worse and worse), to tune the medals
 *   seed=N         a fixed run (default: from the frame the title was shown)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   wave=N         start at wave N (the screenshots of the mothership, tests)
 *   msg=K          (the language test) the run shows message K: 1-3 the power-ups, 4 THEY LANDED!
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

/* the house buttons: A (B, X, Y too) fires and starts, the d-pad moves (and starts), Select (or Start in a run) pauses */
#define ACT_BUTTONS   (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y)
#define START_INPUTS  (ACT_BUTTONS | RS_BTN_START | HU_IN_LR)

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound, opt_wave;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static int start_press;                       /* P1's start press on the title: the run starts at once */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) -------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t best_wave, pad2;
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "HAMH";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x4848;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x5eedcafeu);
    /* the lengths keep the sample memory under the 64 KiB guideline with the house set */
    ha_tone(&s, 0, 1300, 2500, 55, 34, 30);                 /* seed: a little "pip" up */
    ha_hiss(&s, 0, 30, 8, 1);
    ha_store(&s, SFX_SEED, 70);
    ha_tone(&s, 0, 980, 320, 50, 52, 30);                   /* pop: a bubble pop, then a sparkle */
    ha_tone(&s, ha_ms(35), 2637, 3520, 80, 16, 40);
    ha_hiss(&s, 0, 40, 18, 2);
    ha_store(&s, SFX_POP, 125);
    ha_tone(&s, 0, 560, 170, 230, 54, 120);                 /* hit: a "bonk", a wobble, dust */
    ha_tone(&s, ha_ms(60), 330, 440, 160, 18, 80);
    ha_hiss(&s, 0, 140, 36, 2);
    ha_store(&s, SFX_HIT, 260);
    ha_tone(&s, 0, 131, 128, 100, 66, 55);                  /* the march beat: a soft woody "doo" (C3; pitched) */
    ha_tone(&s, 0, 262, 262, 50, 14, 20);
    ha_store(&s, SFX_BEAT, 105);
    ha_tone(&s, 0, 900, 520, 35, 16, 20);                   /* a crumb drops: a tiny plip */
    ha_store(&s, SFX_BOMB, 40);
    ha_tone(&s, 0, 700, 1150, 70, 22, 0);                   /* the saucer: a warble */
    ha_tone(&s, ha_ms(70), 1150, 700, 70, 22, 0);
    ha_store(&s, SFX_SAUCER, 140);
    static const int up[4] = {1047, 1319, 1568, 2093};      /* the saucer popped: a quick run up and a fizz */
    for (int i = 0; i < 4; i++) ha_tone(&s, ha_ms(i * 35), up[i], up[i], 70, 30, 40);
    ha_hiss(&s, 0, 60, 24, 1);
    ha_store(&s, SFX_SAUCER_POP, 180);
    static const int pw[3] = {1319, 1568, 2093};            /* a power-up: E6 G6 C7 */
    for (int i = 0; i < 3; i++) ha_tone(&s, ha_ms(i * 50), pw[i], pw[i], 90, 32, 60);
    ha_store(&s, SFX_POWER, 180);
    ha_tone(&s, 0, 420, 1250, 80, 40, 40);                  /* the bubble pops: "blup" */
    ha_hiss(&s, ha_ms(20), 40, 20, 1);
    ha_store(&s, SFX_SHIELD, 110);
    ha_hiss(&s, 0, 45, 26, 3);                              /* a splat on the cheese or the moon */
    ha_store(&s, SFX_SPLAT, 50);
    ha_tone(&s, 0, 310, 250, 60, 40, 30);                   /* the mothership hit: a clonk */
    ha_tone(&s, 0, 1250, 1100, 30, 14, 15);
    ha_store(&s, SFX_BOSS_HIT, 70);
    ha_hiss(&s, 0, 300, 66, 1);                             /* the mothership pops: a big puff, a fall, sparkles */
    ha_tone(&s, 0, 420, 80, 300, 50, 140);
    ha_tone(&s, ha_ms(120), 2093, 3136, 180, 16, 80);
    ha_store(&s, SFX_BOSS_DOWN, 320);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx_at(int id, int x, int vol)
{
    ha_play_slot(id, vol, x, id == SFX_SAUCER_POP || id == SFX_POWER || id == SFX_BOSS_DOWN);
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    if (opt_wave > 1) {
        world_start_wave(&W, opt_wave);
        W.phase = PH_READY;
        W.wevents = 0;
    }
    int msg = rs_option_int("msg", 0);          /* the language test: a message on screen (4: THEY LANDED!) */
    if (msg >= MSG_DOUBLE && msg <= MSG_SHIELD) W.msg = msg;
    if (msg == 4) W.landed = 1;
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

static pin player_input(int p)
{
    pin in = {0, 0, -1};
    if (p == 0 && start_press) in.fire = 1;
    if (is_bot(p)) {
        const hero *h = &W.h[p];
        if (opt_botstop && h->score >= opt_botstop + 100 * p && st == ST_PLAY && W.started) {
            /* it stops dodging: under the lowest crumb (the tests' game over) */
            int best = -1;
            for (int i = 0; i < BOMB_MAX; i++)
                if (W.bombs[i].on && (best < 0 || W.bombs[i].y > W.bombs[best].y)) best = i;
            if (best >= 0) in.target = W.bombs[best].x >> 16;
            return in;
        }
        pin b = bot_decide(&W, p, opt_botskill);
        b.fire |= in.fire;
        return b;
    }
    uint16_t pad = rs_pad(hu_player_pad(p));
    in.dir = (pad & RS_BTN_RIGHT ? 1 : 0) - (pad & RS_BTN_LEFT ? 1 : 0);
    if (pad & ACT_BUTTONS) in.fire = 1;
    return in;
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
    if (best_now > run_best) { run_best = best_now; new_best = 1; }
    if ((uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
    if (W.wave > SV.best_wave) SV.best_wave = (uint16_t)W.wave;
    if (W.players >= 2 && (uint32_t)best_now > SV.best_multi) SV.best_multi = (uint32_t)best_now;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d wave %d%s (best %u)", runs_done, rs_frame_count(), W.h[0].score, W.wave,
           W.landed ? " (the mice landed)" : "", SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void play_update(void)
{
    pin in[MAX_PLAYERS];
    for (int p = 0; p < MAX_PLAYERS; p++) {
        pin none = {0, 0, -1};
        in[p] = p < W.players ? player_input(p) : none;
    }
    start_press = 0;
    world_step(&W, in);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const hero *h = &W.h[p];
        int x = h->x >> 16;
        if (ev & EV_FIRE) sfx_at(SFX_SEED, x, 50);
        if (ev & EV_POP) sfx_at(SFX_POP, x, HA_VOL_FEEDBACK);
        if (ev & EV_SAUCER) sfx_at(SFX_SAUCER_POP, x, HA_VOL_FEEDBACK);
        if (ev & EV_POWER) sfx_at(SFX_POWER, x, HA_VOL_FEEDBACK);
        if (ev & EV_SHIELD) sfx_at(SFX_SHIELD, x, HA_VOL_FEEDBACK);
        if (ev & EV_BOSS_HIT) sfx_at(SFX_BOSS_HIT, x, 60);
        if (ev & EV_BOSS_DOWN) {
            sfx_at(SFX_BOSS_DOWN, x, HA_VOL_CRASH);
            hu_shake(2, 12);
            rs_log("player %d popped the mothership at frame %u (wave %d)", p + 1, rs_frame_count(), W.wave);
        }
        if (ev & EV_HIT) {
            sfx_at(SFX_HIT, x, HA_VOL_CRASH);
            hu_shake(3, 12);                    /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d hit at frame %u (wave %d, score %d)", p + 1, rs_frame_count(), W.wave, h->score);
        }
        if (ev & EV_SHIELD) rs_log("player %d: the bubble took a hit at frame %u", p + 1, rs_frame_count());
        state_hash = (state_hash ^ (uint32_t)h->x) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(h->score * 131 + h->state * 7 + (h->shield_t > 0))) * 16777619u;
    }
    int wev = W.wevents;
    if (wev & WEV_BEAT) {                       /* the march beat: C E G E, faster as the mice thin out */
        static const int pitch[4] = {0x1000, 0x1428, 0x17f8, 0x1428};
        ha_play_pitched(SFX_BEAT, 64, 160, pitch[W.beat], 0);
    }
    if (wev & (WEV_BOMB | WEV_BOSS_FIRE)) sfx_at(SFX_BOMB, 160, 40);
    if (wev & (WEV_SPLAT | WEV_SHELTER)) sfx_at(SFX_SPLAT, 160, 36);
    if (W.saucer_on && W.t % 24 == 0) sfx_at(SFX_SAUCER, W.saucer_x >> 16, 44);
    if (wev & WEV_WAVE) {
        ha_play(HA_DING);
        rs_log("wave %d at frame %u%s", W.wave, rs_frame_count(), world_is_boss(&W) ? " (the mothership)" : "");
    }
    if (wev & WEV_LANDED) rs_log("the mice landed at frame %u (wave %d)", rs_frame_count(), W.wave);
    state_hash = (state_hash ^ (uint32_t)(W.fx * 977 + W.fy * 31 + W.alive_n)) * 16777619u;
    if (!W.started) return;                     /* ready=1: the run waits for its first press */
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && world_all_down(&W)) {
        int t = 0;
        for (int p = 0; p < W.players; p++) if (W.h[p].t > t) t = W.h[p].t;
        if (t >= PANEL_DELAY) game_over();
    }
}

/* the title: players join and leave (the kit's lobby), P1 starts (A or a start input; a tap) */
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
    if ((ev & HU_TITLE_START) || (opt_bot && st_t >= BOT_START)) {
        ha_play(HA_CONFIRM);
        new_run(ST_PLAY);
        start_press = 1;                        /* the run starts at once: WAVE 1, the mice fly in */
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
    rs_screen_tall(RS_SCREEN_H_MAX);            /* a phone held upright: more space above, the moon below (draw.c) */
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; B, X, Y, Start and Left / Right too), up to 4 players: PRESS A TO PLAY */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, HTR(PLAY), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_wave = rs_option_int("wave", 1);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[64] = "", hs[48] = "", lv[48] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(hs), c = strlen(lv);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.h[p].score);
        snprintf(hs + b, sizeof hs - b, "%s%d", p ? "," : "", W.h[p].state);
        snprintf(lv + c, sizeof lv - c, "%s%d", p ? "," : "", W.h[p].shield_t > 0);
    }
    rs_log("state: st=%d players=%d score=%d score2=%d best=%u runs=%d hs=%s shields=%s wave=%d phase=%d alive=%d fx=%d "
           "fy=%d x=%d shots=%d paused=%d hash=%08x scores=%s",
           st, W.players, W.h[0].score, W.h[1].score, SV.best, runs_done, hs, lv, W.wave, W.phase, W.alive_n, W.fx,
           W.fy, (int)(W.h[0].x >> 16), W.h[0].shots, paused, state_hash, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press); S(state_hash);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound); S(opt_wave);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md) */
    static const rs_game g = {"Hamster Havoc", "hamsterhavoc", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX,
                              0 /* touch_native off: the app's halves and drawn A buttons (apps/launcher/games.cfg;
                                 * the owner's choice: as Mole Marbles) */};
    return &g;
}
