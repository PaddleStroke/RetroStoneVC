/*
 * Unicorn Hop: the game flow (the title with players 2-4 joining, play, game over, pause), input, sound, save RAM,
 * options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/unicornhop/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A
 * (B leaves); P1 starts the run at once with A (or an extra START INPUT; this press is also its first jump); one press
 * on the game-over panel (hu_over_back) goes BACK TO THE TITLE, with the same players: the title is the only menu.
 * Touch: the apps turn a tap anywhere into A (the default for a game that is not touch-native).
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_decide: it times its jumps); botruns=N runs;
 *                  botstop=S: it misses on purpose once its score reaches S (P2-P4: S+2, S+4..)
 *   botskill=E     the bot misjudges each landing by up to E px (a human-like player, to tune the medals)
 *   seed=N         a fixed sky (default: from the frame the title was shown)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   dump=1         log the final state at exit (tests); music=0, sound=0
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

/* the house buttons: A (and B, X, Y, Up) jumps and starts (the start inputs), Select (or Start in a run) pauses */
#define ACT_BUTTONS   (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y | RS_BTN_UP)
#define START_INPUTS  (ACT_BUTTONS | RS_BTN_START)

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static int start_press;                       /* P1's start press on the title: also its first jump */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) -------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t best_height, rainbows;
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "UNIH";

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
    /* the lengths keep the sample memory under the 64 KiB guideline with the house set (about 36 KiB) */
    ha_tone(&s, 0, 420, 980, 120, 58, 70);                  /* jump: a soft "boing" up, a sparkle on top */
    ha_tone(&s, ha_ms(30), 1568, 2093, 70, 14, 30);
    ha_store(&s, SFX_JUMP, 140);
    ha_tone(&s, 0, 300, 1200, 180, 60, 110);                /* the pink bounce: a longer, springier boing */
    ha_tone(&s, ha_ms(20), 600, 2400, 150, 18, 70);
    ha_tone(&s, ha_ms(110), 2093, 2637, 80, 14, 40);
    ha_store(&s, SFX_BIG, 190);
    ha_hiss(&s, 0, 60, 16, 4);                              /* land: a soft fluffy puff */
    ha_tone(&s, 0, 220, 160, 60, 22, 30);
    ha_store(&s, SFX_LAND, 70);
    ha_tone(&s, 0, 1760, 1760, 140, 40, 60);                /* star: a two-note chime (A6, E7) */
    ha_tone(&s, ha_ms(60), 2637, 2637, 140, 36, 70);
    ha_tone(&s, ha_ms(60), 5274, 5274, 100, 8, 40);
    ha_store(&s, SFX_STAR, 200);
    ha_tone(&s, 0, 784, 330, 260, 48, 200);                 /* miss: a sad slide down */
    ha_tone(&s, ha_ms(50), 988, 392, 210, 16, 160);
    ha_store(&s, SFX_MISS, 260);
    ha_hiss(&s, 0, 170, 70, 0);                             /* zap: a crackle and a buzz */
    ha_tone(&s, 0, 140, 90, 170, 40, 100);
    ha_tone(&s, ha_ms(10), 1900, 600, 60, 20, 20);
    ha_store(&s, SFX_ZAP, 170);
    ha_tone(&s, 0, 1400, 900, 55, 34, 25);                  /* drip: a plip */
    ha_store(&s, SFX_DRIP, 60);
    ha_hiss(&s, 0, 160, 30, 3);                             /* melt: a fizzle */
    ha_tone(&s, 0, 600, 200, 160, 18, 110);
    ha_store(&s, SFX_MELT, 160);
    static const int jingle[6] = {1047, 1319, 1568, 2093, 2637, 3136};   /* rainbow: C6 E6 G6 C7 E7 G7, a sparkle */
    for (int i = 0; i < 6; i++) ha_tone(&s, ha_ms(i * 55), jingle[i], jingle[i], 180, 34, 80);
    ha_tone(&s, ha_ms(300), 4186, 4186, 170, 12, 80);
    ha_store(&s, SFX_RAINBOW, 480);                         /* (the scratch buffer holds 500 ms) */
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx_at(int id, int x, int vol)
{
    ha_play_slot(id, vol, x, id == SFX_STAR || id == SFX_RAINBOW || id == SFX_DRIP);
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

/* a human-like bot (botskill=E): it misjudges the landing of each jump by -E..E px (by the level and the player) */
static int bot_press(int p)
{
    const hero *h = &W.h[p];
    int stop = opt_botstop && h->score >= opt_botstop + 2 * p && st == ST_PLAY, err = 0;
    if (opt_botskill) {
        uint32_t k = (W.seed ^ (uint32_t)(h->level * 7919 + p * 104729)) * 2654435761u;
        err = (int)((k >> 16) % (uint32_t)(2 * opt_botskill + 1)) - opt_botskill;
    }
    return bot_decide(&W, p, stop, err);
}

static int act_pressed(int p)
{
    if (p == 0 && start_press) return 1;
    if (is_bot(p)) return bot_press(p);
    return (rs_pad_pressed(hu_player_pad(p)) & ACT_BUTTONS) != 0;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int best_now = 0, height = 0;
    for (int p = 0; p < W.players; p++) {
        if (W.h[p].score > best_now) best_now = W.h[p].score;
        if (W.h[p].height > height) height = W.h[p].height;
    }
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.h[0].score, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (best_now > run_best) { run_best = best_now; new_best = 1; }
    if (best_now > SV.best) SV.best = (uint16_t)best_now;
    if (height > SV.best_height) SV.best_height = (uint16_t)height;
    if (W.players >= 2 && best_now > SV.best_multi) SV.best_multi = (uint16_t)best_now;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d height %d stars %d (best %d)", runs_done, rs_frame_count(),
           W.h[0].score, W.h[0].height, W.h[0].stars, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void play_update(void)
{
    int press[MAX_PLAYERS] = {0};
    for (int p = 0; p < W.players; p++) {
        const hero *h = &W.h[p];
        if (h->state == HS_READY || h->state == HS_STAND) press[p] = act_pressed(p);
    }
    start_press = 0;
    world_step(&W, press);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const hero *h = &W.h[p];
        int x = h->x >> 16;
        if (ev & EV_JUMP) sfx_at((ev & EV_BIG) ? SFX_BIG : SFX_JUMP, x, HA_VOL_ACTION);
        if (ev & EV_LAND) sfx_at(SFX_LAND, x, 50);
        if (ev & EV_STAR) sfx_at(SFX_STAR, x, HA_VOL_FEEDBACK);
        if (ev & EV_DRIP) sfx_at(SFX_DRIP, x, HA_VOL_FEEDBACK);
        if (ev & EV_RAINBOW) sfx_at(SFX_RAINBOW, CENTER_X, HA_VOL_FEEDBACK);
        if (ev & (EV_MISS | EV_MELT | EV_OUT)) {
            sfx_at(SFX_MISS, x, HA_VOL_FEEDBACK);
            rs_log("player %d fell at frame %u (%s; height %d, score %d)", p + 1, rs_frame_count(),
                   (ev & EV_MISS) ? "missed" : (ev & EV_MELT) ? "the rain cloud melted" : "left below", h->height,
                   h->score);
        }
        if ((ev & EV_RAINBOW) && opt_bot) rs_log("player %d: rainbow at frame %u (height %d)", p + 1, rs_frame_count(),
                                                h->height);
        if (ev & EV_ZAP) {
            sfx_at(SFX_ZAP, x, HA_VOL_CRASH);
            hu_shake(3, 12);                    /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d zapped at frame %u (height %d, score %d)", p + 1, rs_frame_count(), h->height, h->score);
        }
        state_hash = (state_hash ^ (uint32_t)h->dy) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)h->x) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(h->score * 131 + h->state)) * 16777619u;
    }
    if (W.wevents & WEV_MELT) sfx_at(SFX_MELT, CENTER_X, 50);
    state_hash = (state_hash ^ (uint32_t)W.cam) * 16777619u;
    if (!W.started) return;                     /* ready=1: the run waits for its first press */
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && world_all_down(&W)) {
        int t = 0;
        for (int p = 0; p < W.players; p++) if (W.h[p].t > t) t = W.h[p].t;
        if (t >= PANEL_DELAY) game_over();
    }
}

/* the title: players join and leave (the kit's lobby), P1 starts; the clouds drift above */
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
        new_run(ST_PLAY);                       /* level 1 starts right above: the start press is the first jump */
        start_press = 1;
        play_update();
        return;
    }
    static const int none[MAX_PLAYERS] = {0};
    world_step(&W, none);
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
    rs_screen_tall(RS_SCREEN_H_MAX);      /* a phone held upright: more sky, more clouds, the castle's land below (draw.c, DESIGN.md) */
    save_load();
    run_best = SV.best;                         /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; B, X, Y, Up and Start too), up to 4 players: PRESS A TO HOP */
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
    char sc[48] = "", hs[48] = "", ht[48] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(hs), c = strlen(ht);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.h[p].score);
        snprintf(hs + b, sizeof hs - b, "%s%d", p ? "," : "", W.h[p].state);
        snprintf(ht + c, sizeof ht - c, "%s%d", p ? "," : "", W.h[p].height);
    }
    rs_log("state: st=%d players=%d score=%d score2=%d best=%d runs=%d hs=%s alt=%d cam=%d paused=%d hash=%08x "
           "heights=%s stars=%d scores=%s",
           st, W.players, W.h[0].score, W.h[1].score, SV.best, runs_done, hs, hero_alt(&W.h[0]), cam_px(&W),
           paused, state_hash, ht, W.h[0].stars, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press); S(state_hash);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    static const rs_game g = {"Unicorn Hop", "unicornhop", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1,
                              RS_SCREEN_W_MAX};     /* widescreen: any width (docs/spec.md "Widescreen") */
    return &g;
}
