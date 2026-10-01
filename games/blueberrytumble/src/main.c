/*
 * Blueberry Tumble: the game flow (the house flow: the title is the only menu, players 2-4 join there; the run, the
 * splat, game over or the 2-4 player ranking, one press to retry, pause), input, sound, the music (one track per
 * biome, started on the biome's first beat), save RAM, options, save states, test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (src/bot.c, from the screen); botruns=N runs; botstop=M stops pressing
 *                  at M metres
 *   seed=N         a fixed course (default: from the frame of the press that starts the run and the save RAM)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   god=1          the berries bounce off everything (a screenshot aid: the run goes on)
 *   dump=1         log the final state at exit (tests); evlog=1 log the events (screenshots); music=0, sound=0
 */
#include "bt.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* the house buttons: A (and B, X, Y, Up) jumps; the start inputs (A, Up: the house table) start from the title and
 * retry on the game-over panel (Start too); Select (or Start in a run) pauses; Select on the panel: the title */
#define ACT_BUTTONS  (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y | RS_BTN_UP)
#define START_INPUTS (RS_BTN_A | RS_BTN_UP)
#define RETRY_INPUTS (START_INPUTS | RS_BTN_START)

static int opt_bot, opt_botstop, opt_botruns, opt_seed_fixed, opt_music, opt_sound, opt_god, opt_evlog;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done, medal;
static int track = -1, music_wait;          /* the biome track playing; waiting for a bar to restart it */
static int32_t log_seg = -1;                /* evlog: the segment player 1 was last logged in */
static uint32_t state_hash = 2166136261u;

/* ---- save RAM (house convention: checked, versioned, committed at each game over) --------------------------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best_score, best_metres, runs, golden;
    uint16_t medals[4];
    uint32_t best_race;
    uint16_t sum;
} save_data;
static save_data SV;

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
    if (memcmp(SV.magic, GAME_MAGIC, 4) || SV.version != 1 || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, GAME_MAGIC, 4);
        SV.version = 1;
    }
}

static void save_store(void)
{
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* ---- the music: the biome's track from its first beat (the gate); after a pause, again from a bar line ------------ */
static const char *const tracks[8] = {"music/summit.mod", "music/forest.mod", "music/meadow.mod", "music/village.mod",
                                      "music/night_summit.mod", "music/night_forest.mod", "music/night_meadow.mod",
                                      "music/night_village.mod"};

static void music_play(int tr)
{
    size_t n;
    const void *m = rs_asset(tracks[tr], &n);
    if (m && !rs_music_play(m, n, 1)) rs_music_volume(HA_MUSIC_VOL);
    track = tr;
}

static void music_update(void)
{
    if (!opt_music || (st != ST_PLAY && st != ST_DEAD) || !world_running(&W)) return;
    int32_t xpx = (int32_t)(course_x(&W.course, W.f) >> 16);
    int32_t bi = xpx / (BIOME_CELLS * CELL);
    int tr = bi < NBIOMES ? (int)bi : NBIOMES + (int)(bi % NBIOMES);
    if (tr != track) { music_play(tr); music_wait = 0; }
    if (music_wait) {
        int speed = course_speed(&W.course, W.f) >> 16;
        if ((xpx & (BAR_CELLS * CELL - 1)) <= speed) { music_play(tr); music_wait = 0; }
    }
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

/* the world for the players in the lobby: a placeholder course until the press that starts a run (the start is the
 * same level ground whatever the seed: nothing visible changes) */
static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : run_seed(rs_frame_count(), SV.runs, 0);
    world_init(&W, hu_players(), seed);
    W.god = opt_god;
    draw_reset();
    bot_reset();
    new_best = 0;
    medal = 0;
    log_seg = -1;
    track = -1;
    go(state);
}

/* a run starts now: its course comes from this press (its frame) and the save RAM (the runs played, the best, the
 * golden blueberries): two runs, two sessions, never the same course (DESIGN.md "Seeds"). The press that starts it
 * is not a jump. */
static void start_run(void)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : run_seed(rs_frame_count(), SV.runs, SV.best_metres ^ SV.golden << 20);
    world_init(&W, hu_players(), seed);
    W.god = opt_god;
    W.started = 1;
    draw_reset();
    bot_reset();
    new_best = 0;
    medal = 0;
    log_seg = -1;
    track = -1;
    go(ST_PLAY);
}

static int is_bot(int p) { return p < opt_bot; }

static int held_now(int p)
{
    if (is_bot(p)) {
        if (opt_botstop && W.metres[p] >= opt_botstop && st == ST_PLAY) return 0;
        return bot_decide(p);
    }
    return (rs_pad(hu_player_pad(p)) & ACT_BUTTONS) != 0;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_METRES;
    int best_now = 0, best_m = 0;
    for (int p = 0; p < W.players; p++) {
        if (world_score(&W, p) > best_now) best_now = world_score(&W, p);
        if (W.metres[p] > best_m) best_m = W.metres[p];
        SV.golden += (uint32_t)W.coins[p];
    }
    SV.runs++;                              /* the attempts, counted when they end (the save RAM changes only here) */
    medal = W.players == 1 ? hu_medal_of(W.metres[0], medals) : 0;
    if (medal) SV.medals[medal - 1]++;
    if ((uint32_t)best_now > SV.best_score) { SV.best_score = (uint32_t)best_now; new_best = 1; }
    if ((uint32_t)best_m > SV.best_metres) SV.best_metres = (uint32_t)best_m;
    if (W.players >= 2 && (uint32_t)best_now > SV.best_race) SV.best_race = (uint32_t)best_now;   /* races, 2-4 */
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: %d m, score %d (best %u), seed %u", runs_done, rs_frame_count(), W.metres[0],
           world_score(&W, 0), SV.best_score, W.seed);
    ha_play(HA_GAMEOVER);
    if (medal) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void sounds_of(int p)
{
    int ev = W.events[p], x = world_x(&W, p) - (int)(course_x(&W.course, W.f) >> 16) + PIVOT_X;
    if (opt_evlog && (ev & (EV_PAD | EV_ORB | EV_GLIDE | EV_GROW | EV_SMASH | EV_COIN | EV_ICE | EV_DIE)))
        rs_log("ev p%d frame %u m %d:%s%s%s%s%s%s%s%s", p + 1, rs_frame_count(), W.metres[p], ev & EV_PAD ? " pad" : "",
               ev & EV_ORB ? " orb" : "", ev & EV_GLIDE ? " glide" : "", ev & EV_GROW ? " grow" : "",
               ev & EV_SMASH ? " smash" : "", ev & EV_COIN ? " coin" : "", ev & EV_ICE ? " ice" : "",
               ev & EV_DIE ? " die" : "");
    if (ev & EV_JUMP) sfx_play(SFX_JUMP, x);
    if (ev & EV_LAND) sfx_play(SFX_LAND, x);
    if (ev & EV_ORB) sfx_play(SFX_CHIME, x);
    if (ev & EV_PAD) sfx_play(SFX_BOING, x);
    if (ev & EV_SMASH) sfx_play(SFX_CRUNCH, x);
    if (ev & EV_GROW) sfx_play(SFX_GROW, x);
    if (ev & EV_SHRINK) sfx_play(SFX_SPLASH, x);
    if (ev & (EV_GLIDE | EV_GLIDE_END)) sfx_play(SFX_WHOOSH, x);
    if (ev & EV_COIN) sfx_play(SFX_COIN, x);
    if (ev & EV_ICE) sfx_play(SFX_SKID, x);
    if (ev & EV_DIE) {
        sfx_play(SFX_SQUELCH, x);
        hu_shake(3, 12);                    /* house rule: a shake on a hit only, <= 4 px, <= 16 frames */
        const bt_seg *sg = course_seg_at(&W.course, (int32_t)((W.b[p].x >> 16) / CELL));
        rs_log("player %d splat at %d m (frame %u, cause %d, in %s)", p + 1, W.metres[p], rs_frame_count(), W.b[p].cause,
               sg ? pat_name(sg->pat) : "?");
    }
}

static void play_update(void)
{
    int held[MAX_PLAYERS] = {0};
    for (int p = 0; p < W.players; p++) held[p] = held_now(p);
    int32_t gate_before = (int32_t)((course_x(&W.course, W.f) >> 16) / CELL);
    world_step(&W, held);
    int32_t col = (int32_t)((course_x(&W.course, W.f) >> 16) / CELL);
    if (col != gate_before && (course_col(&W.course, col)->flags & F_GATE)) sfx_play(SFX_GATE, PIVOT_X);
    draw_events(&W);
    if (opt_evlog) {
        /* the pattern instances player 1 meets (tools/difficulty.py: the bot's failure rate per pattern) */
        const bt_seg *sg = course_seg_at(&W.course, (int32_t)((W.b[0].x >> 16) / CELL));
        if (sg && sg->col0 != log_seg && !W.b[0].dead) {
            log_seg = sg->col0;
            rs_log("seg %s score %d target %d tier %d at %d m", pat_name(sg->pat), sg->score, sg->target, sg->tier, (int)sg->col0);
        }
    }
    for (int p = 0; p < W.players; p++) {
        sounds_of(p);
        state_hash = (state_hash ^ (uint32_t)W.b[p].h) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(W.metres[p] * 131 + W.b[p].mode * 7 + W.b[p].dead)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)W.f) * 16777619u;
    if (st == ST_PLAY && !world_running(&W)) {
        rs_music_stop();
        track = -1;
        go(ST_DEAD);
    }
    if (st == ST_DEAD) {
        int all = 1;
        for (int p = 0; p < W.players; p++) all &= W.t - W.dead_f[p] >= PANEL_DELAY;
        if (all) game_over();
    }
}

/* the title: players 2-4 join and leave (the kit's lobby); P1's start inputs (A, Up) start the run */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) ha_play(HA_CONFIRM);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) {
        int t = st_t;
        new_run(ST_TITLE);                      /* the world follows the lobby: the joined berries wait on the slope */
        st_t = t;
    }
    if ((ev & HU_TITLE_START) || (opt_bot && st_t == 30)) {
        ha_play(HA_CONFIRM);
        start_run();
    }
}

static void game_update(void)
{
    if (st == ST_PLAY && (rs_pad_pressed(0) & (RS_BTN_SELECT | RS_BTN_START))) {
        paused = !paused;
        ha_play(HA_PAUSE);
        if (paused) { rs_music_stop(); track = -1; }
        else if (opt_music) music_wait = 1;
    }
    if (paused) return;
    hu_shake_step();
    switch (st) {
    case ST_TITLE:
        title_update();
        break;
    case ST_READY:
        /* --opt ready=1 only (tests): the berries wait at the start for a start input (a bot: on frame 20) */
        if ((opt_bot && st_t == 20) || (!opt_bot && (rs_pad_pressed(0) & RETRY_INPUTS))) start_run();
        break;
    case ST_PLAY:
    case ST_DEAD:
        play_update();
        music_update();
        break;
    case ST_OVER: {
        int again = 0, back = 0;
        if (st_t >= RETRY_LOCK) {
            for (int p = 0; p < W.players; p++) {
                if (is_bot(p)) continue;
                uint16_t b = rs_pad_pressed(hu_player_pad(p));
                again |= (b & RETRY_INPUTS) != 0;
                back |= (b & RS_BTN_SELECT) != 0;
            }
            if (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10) again = 1;
        }
        if (back) {                             /* Select: back to the title (players join or leave there) */
            ha_play(HA_SELECT);
            new_run(ST_TITLE);
        } else if (again) {                     /* house rule: one press, instant retry, straight into the run */
            ha_play(HA_CONFIRM);
            start_run();
        }
        break;
    }
    }
    st_t++;
}

static void game_draw(void)
{
    draw_frame(&W, st, st_t, (int)SV.best_score, new_best, paused, (int)SV.runs + (st == ST_OVER ? 0 : 1), medal);
}

static void game_init(void)
{
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sfx_init(opt_sound);
    if (opt_music) { music_play(0); rs_music_stop(); track = -1; }   /* the SDK then keeps "loop" set: every state says the same */
    draw_init();                                /* hu_init (draw.c) resets the lobby to P1 alone */
    /* the title: P1's start inputs (A, Up: the house table, the prompt shows the A), up to 4 players */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, "ROLL", NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_god = rs_option_int("god", 0);
    opt_evlog = rs_option_int("evlog", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    if (n < opt_bot) n = opt_bot;
    hu_players_set(n < 1 ? 1 : n > MAX_PLAYERS ? MAX_PLAYERS : n);
    new_run(rs_option_int("ready", 0) ? ST_READY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    rs_log("state: st=%d players=%d m=%d m2=%d m3=%d m4=%d score=%d best=%u runs=%u done=%d coins=%d mode=%d h=%d f=%d "
           "paused=%d hash=%08x",
           st, W.players, W.metres[0], W.metres[1], W.metres[2], W.metres[3], world_score(&W, 0), SV.best_score, SV.runs,
           runs_done, W.coins[0], W.b[0].mode, W.b[0].h >> 16, W.f, paused, state_hash);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(medal); S(track); S(music_wait);
    S(state_hash); S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound); S(opt_god); S(opt_evlog); S(log_seg);
    draw_state();
    bot_state();
    hu_state();
    ha_state();
}
#undef S

/* after a state was loaded: the save RAM is not part of states, read it again (the best, the attempts) */
static void game_state_loaded(void) { save_load(); }

const rs_game *rs_game_main(void)
{
    static const rs_game g = {GAME_NAME, GAME_ID, "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, game_state_loaded, 1};
    return &g;
}
