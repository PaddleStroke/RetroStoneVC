/*
 * Pancake Tower: the game flow (title, ready, play, game over, pause, player 2), input, sounds, save RAM,
 * options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=1          the screen-reading bot plays player 1 (bot.c), bot=2 both players; botruns=N runs
 *   seed=N         the bot's seed (its human-like timing jitter); the game itself has no randomness
 *   botstop=H      the bot misses on purpose at height H (screenshots of a game over)
 *   start=N        start with a tower of N perfect pancakes already stacked (screenshots high up; not saved)
 *   players=2      start with player 2 joined;  ready=1  skip the title
 *   dump=1         log the final state at exit (tests); music=0, sound=0; strict=1 (the SDK's guideline checks)
 */
#include "pt.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include <stdio.h>
#include <string.h>

/* the house buttons: A (and B, X, Y, Up) drops, Start starts and retries, Start or Select pauses a run */
#define ACT_BUTTONS (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y | RS_BTN_UP)
#define GO_BUTTONS  (ACT_BUTTONS | RS_BTN_START)

int opt_bot;
static int opt_botstop, opt_botruns, opt_start, opt_music, opt_sound;
static uint32_t opt_seed;
static match M;
static int st, st_t, paused, new_best, runs_done, players = 1, over_t;
static uint32_t state_hash = 2166136261u;
static int bot_heights[64], nbot_heights;

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) --------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best, best_height, best_versus, best_chain;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t sum;
} save_data;
static save_data SV;

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x5054;
    for (size_t i = 0; i < offsetof(save_data, sum); i++) sum = (uint16_t)(sum * 31 + p[i]);
    return sum;
}

static void save_load(void)
{
    memcpy(&SV, rs_sram(), sizeof SV);
    if (memcmp(SV.magic, "PTWR", 4) || SV.version != 1 || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, "PTWR", 4);
        SV.version = 1;
    }
}

static void save_store(void)
{
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* ---- flow -------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

/* start=N: stack N perfect pancakes through the rules (as a perfect player would), before the run is shown */
static void prestack(tower *tw, int n)
{
    while (tw->pancakes < n && tower_alive(tw)) {
        for (int i = 0; i < 400 && tw->state != TS_SLIDE; i++) tower_step(tw, 0);
        int x = tower_top(tw)->x;
        if (tw->syrup) x -= (tw->vx > 0 ? 1 : -1) * tw->g.slip;
        tw->sx = (int32_t)x << 16;
        tower_step(tw, 1);
        for (int i = 0; i < 100 && (tw->state == TS_DROP || tw->state == TS_SLIP); i++) tower_step(tw, 0);
    }
    for (int i = 0; i < 400 && tw->state != TS_SLIDE; i++) tower_step(tw, 0);
    tw->events = 0;
}

static void new_run(int state)
{
    match_init(&M, players);
    for (int p = 0; p < M.players; p++)
        if (opt_start) prestack(&M.tw[p], opt_start);
    draw_reset(&M);
    bot_reset(opt_seed * 2654435761u + (uint32_t)runs_done * 40503u);
    new_best = 0;
    over_t = 0;
    go(state);
}

static int is_bot(int p) { return (p == 0 && opt_bot) || (p == 1 && opt_bot >= 2); }

static int act_pressed(int p)
{
    if (is_bot(p)) return bot_decide(p);
    return (rs_pad_pressed(p) & ACT_BUTTONS) != 0;
}

static void game_over(void)
{
    static const int medals[4] = {MEDAL_BRONZE, MEDAL_SILVER, MEDAL_GOLD, MEDAL_PEARL};
    int best_now = 0, height = 0, chain = 0;
    for (int p = 0; p < M.players; p++) {
        if (M.tw[p].score > best_now) best_now = M.tw[p].score;
        if (M.tw[p].pancakes > height) height = M.tw[p].pancakes;
        if (M.tw[p].best_chain > chain) chain = M.tw[p].best_chain;
    }
    int m = M.players == 1 ? hu_medal_of(M.tw[0].score, medals) : 0;
    if (!opt_start) {                          /* a pre-stacked tower (screenshots) does not count */
        SV.runs++;
        if (m) SV.medals[m - 1]++;
        if (M.players == 1 && best_now > SV.best) { SV.best = (uint16_t)best_now; new_best = 1; }
        if (height > SV.best_height) SV.best_height = (uint16_t)height;
        if (M.players == 2 && height > SV.best_versus) SV.best_versus = (uint16_t)height;
        if (chain > SV.best_chain) SV.best_chain = (uint16_t)chain;
        save_store();
    }
    if (nbot_heights < 64) bot_heights[nbot_heights++] = M.tw[0].pancakes;
    runs_done++;
    rs_log("run %d over at frame %u: height %d score %d chain %d (best %d)", runs_done, rs_frame_count(),
           M.tw[0].pancakes, M.tw[0].score, M.tw[0].best_chain, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(DS_OVER);
}

static void sounds(void)
{
    static const uint8_t scale[8] = {0, 2, 4, 5, 7, 9, 11, 12};    /* the perfect chain climbs a major scale */
    for (int p = 0; p < M.players; p++) {
        const tower *tw = &M.tw[p];
        int ev = tw->events, x = M.players == 1 ? RS_SCREEN_W / 2 : 80 + 160 * p;
        if (ev & EV_LAND) sfx_play(SFX_FLOP, x, clampi(tw->pancakes / 6, 0, 12));
        if (ev & EV_PERFECT) sfx_play(SFX_DING, x, scale[clampi(tw->chain - 1, 0, 7)] + (tw->chain > 8 ? 2 : 0));
        if (ev & EV_REGROW) sfx_play(SFX_FANFARE, x, 0);
        if (ev & EV_CUT) sfx_play(SFX_SPLAT, x, clampi(12 - tw->cut.w / 4, 0, 12));
        if (ev & EV_MISS) sfx_play(SFX_WHOOSH, x, 0);
        if (ev & EV_SLIP) sfx_play(SFX_SQUISH, x, 0);
        if (ev & EV_POUR) sfx_play(SFX_POUR, x, 0);
        if (ev & EV_TOPPING_LAND) { sfx_play(SFX_PLOP, x, 0); sfx_play(SFX_FANFARE, x, 5); }
        if (ev & EV_SPLASHED) sfx_play(SFX_SPLASH, x, 0);
        if (ev & EV_CEILING) { sfx_play(SFX_CRASH, x, 0); rs_log("player %d crashed through the ceiling at frame %u", p + 1, rs_frame_count()); }
        if (ev & EV_ROOF) { sfx_play(SFX_ROOF, x, 0); rs_log("player %d broke through the roof at frame %u", p + 1, rs_frame_count()); }
        if (ev & EV_TOPPING_LAND) rs_log("player %d topping %d landed at frame %u", p + 1, tower_top(tw)->kind, rs_frame_count());
        if ((ev & EV_PERFECT) && tw->chain >= 5) rs_log("player %d perfect chain %d at frame %u", p + 1, tw->chain, rs_frame_count());
        if ((ev & (EV_LAND | EV_MISS)) && rs_option_int("botlog", 0))
            rs_log("land p%d f%u: h=%d w=%d cut=%d perfect=%d chain=%d syrupslip=%d", p + 1, rs_frame_count(), tw->pancakes,
                   tower_top(tw)->w, (ev & EV_CUT) ? tw->cut.w : 0, (ev & EV_PERFECT) != 0, tw->chain, tw->slip_dir);
        if (ev & EV_MISS) rs_log("player %d missed at frame %u (height %d)", p + 1, rs_frame_count(), tw->pancakes);
    }
}

static void play_update(void)
{
    int press[MAX_PLAYERS] = {0, 0};
    for (int p = 0; p < M.players; p++) {
        int a = act_pressed(p);                 /* the bot watches the screen every frame */
        if (M.tw[p].state == TS_SLIDE) press[p] = a;
    }
    match_step(&M, press);
    sounds();
    draw_events(&M);
    for (int p = 0; p < M.players; p++) {
        const tower *tw = &M.tw[p];
        state_hash = (state_hash ^ (uint32_t)tw->sx) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(tw->pancakes * 131 + tw->state * 7 + tw->score * 1031)) * 16777619u;
    }
    if (st == DS_READY) {
        for (int p = 0; p < M.players; p++)
            if (M.tw[p].state != TS_SLIDE || M.tw[p].pancakes) go(DS_PLAY);
        return;
    }
    if (st == DS_PLAY && match_over(&M) && ++over_t >= OVER_DELAY) game_over();
}

static void try_join(void)
{
    /* player 2 joins with A on pad 2, on the title or ready */
    if (players == 1 && (rs_pad_pressed(1) & GO_BUTTONS)) {
        players = 2;
        ha_play(HA_CONFIRM);
        new_run(DS_READY);
    }
}

static void game_update(void)
{
    if (st == DS_PLAY && (rs_pad_pressed(0) & (RS_BTN_SELECT | RS_BTN_START))) {
        paused = !paused;
        ha_play(HA_PAUSE);
    }
    int seg = segment_of(draw_camera(0) + 120);
    for (int p = 1; p < M.players; p++) {
        int s2 = segment_of(draw_camera(p) + 120);
        if (s2 > seg) seg = s2;
    }
    music_update(seg, !paused);
    if (paused) return;
    hu_shake_step();
    switch (st) {
    case DS_TITLE: {
        int pr[MAX_PLAYERS] = {0, 0};
        match_step(&M, pr);                    /* the slider slides on the title */
        try_join();
        if (st == DS_TITLE && (opt_bot || (rs_pad_pressed(0) & GO_BUTTONS))) {
            ha_play(HA_CONFIRM);
            go(DS_READY);
        }
        break;
    }
    case DS_READY:
        try_join();
        play_update();
        break;
    case DS_PLAY:
        play_update();
        break;
    case DS_OVER: {
        int again = 0;
        if (st_t >= RETRY_LOCK) {
            for (int p = 0; p < M.players; p++) again |= (rs_pad_pressed(p) & GO_BUTTONS) != 0;
            if (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10) again = 1;
        }
        if (again) {                           /* house rule: one button, instant retry */
            ha_play(HA_CONFIRM);
            new_run(DS_READY);
        }
        break;
    }
    }
    draw_update(&M);
    st_t++;
}

static void game_draw(void)
{
    draw_frame(&M, st, st_t, SV.best, new_best, paused);
    draw_oam_log();
}

static void game_init(void)
{
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    audio_set(opt_music, opt_sound);
    sfx_init();
    draw_init();
    opt_bot = rs_option_int("bot", 0);
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_start = clampi(rs_option_int("start", 0), 0, 5000);
    opt_seed = (uint32_t)rs_option_int("seed", 1);
    players = rs_option_int("players", 1) >= 2 ? 2 : 1;
    new_run(rs_option_int("ready", 0) ? DS_READY : DS_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char list[400] = "";
    for (int i = 0; i < nbot_heights; i++) {
        size_t n = strlen(list);
        snprintf(list + n, sizeof list - n, "%s%d", i ? "," : "", bot_heights[i]);
    }
    int jit[5];
    bot_jitter_counts(jit);
    rs_log("state: st=%d players=%d height=%d score=%d height2=%d score2=%d best=%d runs=%d ts=%d,%d sx=%d "
           "paused=%d cam=%d chain=%d hash=%08x jitter=%d,%d,%d,%d,%d vramtiles=%d heights=%s",
           st, M.players, M.tw[0].pancakes, M.tw[0].score, M.tw[1].pancakes, M.tw[1].score, SV.best, runs_done,
           M.tw[0].state, M.tw[1].state, tower_slider_x(&M.tw[0]), paused, draw_camera(0), M.tw[0].best_chain,
           state_hash, jit[0], jit[1], jit[2], jit[3], jit[4], rs_tiles_used(), list);
}

/* test hook: the match and the screen being shown */
const match *pt_test_match(int *state)
{
    if (state) *state = st;
    return &M;
}

int bot_stop_height(void) { return opt_botstop; }
int pt_players(void) { return M.players; }

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them).
 * Not saved: SV, the battery save's copy (a state never takes the best score back). ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(M); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(players); S(over_t); S(state_hash);
    S(bot_heights); S(nbot_heights);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_start); S(opt_seed); S(opt_music); S(opt_sound);
    draw_state();
    sfx_state();
    bot_state();
    hu_state();
}
#undef S

static void game_state_loaded(void) { draw_state_loaded(); }

const rs_game *rs_game_main(void)
{
    /* state_version: bump it when the meaning of a saved object changes (its layout is checked) */
    static const rs_game g = {"Pancake Tower", "pancaketower", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, pt_assets, game_state, game_state_loaded, 1};
    return &g;
}
