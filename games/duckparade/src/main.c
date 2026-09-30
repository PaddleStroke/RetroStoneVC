/*
 * Duck Parade: the game flow (title, get ready, play, the death, game over, pause, Father Duck), input, sound
 * events, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=1          the screen-reading bot plays Mother Duck (bot.c), bot=2 both parents; botruns=N runs
 *   botstop=S      the bot stops hopping at score S (the fox then comes: screenshots of a game over)
 *   seed=N         a fixed course (default: from the frame of the first hop)
 *   players=2      start with Father Duck joined;  ready=1  skip the title
 *   dump=1         log the final state at exit (tests); music=0, sound=0; strict=1 (the SDK's)
 */
#include "dp.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include <stdio.h>
#include <string.h>

#define GO_BUTTONS (RS_BTN_A | RS_BTN_START)

int opt_bot;
static int opt_botstop, opt_botruns, opt_seed_fixed, opt_music, opt_sound;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done, players = 1;
static uint32_t state_hash = 2166136261u;
static int run_scores[64], run_lanes[64], nruns_logged;
static int bell_t;                        /* frames to the next crossing bell */
static int click_t = -1;                  /* the photo's click, a moment after the fanfare */

/* ---- save RAM (house convention: checked, versioned, committed at each game over) ------------------------------ */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best, best_coop, bank_best, best_lanes;
    uint32_t runs;
    uint16_t medals[4];
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
    if (memcmp(SV.magic, "DUCK", 4) || SV.version != 1 || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, "DUCK", 4);
        SV.version = 1;
    }
}

static void save_store(void)
{
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* ---- flow ---------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, players, seed);
    W.fixed_seed = opt_seed_fixed;
    draw_reset(&W);
    bot_reset();
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return (p == 0 && opt_bot) || (p == 1 && opt_bot >= 2); }

/* the pad: Right or A hops forward, Left back, Up and Down along the lane */
static int hop_pressed(int p)
{
    if (is_bot(p)) {
        if (opt_botstop && world_score(&W, p) >= opt_botstop && st == DS_PLAY) return 0;
        return bot_decide(p);
    }
    uint16_t b = rs_pad_pressed(p);
    if (b & (RS_BTN_RIGHT | RS_BTN_A)) return HOP_FWD;
    if (b & RS_BTN_UP) return HOP_UP;
    if (b & RS_BTN_DOWN) return HOP_DOWN;
    if (b & RS_BTN_LEFT) return HOP_BACK;
    return 0;
}

static void game_over(void)
{
    static const int th[4] = {MEDAL_BRONZE, MEDAL_SILVER, MEDAL_GOLD, MEDAL_PEARL};
    int s0 = world_score(&W, 0), total = world_total(&W);
    SV.runs++;
    if (W.players == 1) {
        int m = hu_medal_of(s0, th);
        if (m) SV.medals[m - 1]++;
        if (s0 > SV.best) { SV.best = (uint16_t)s0; new_best = 1; }
        int lanes = W.d[0].max_col - START_COL;
        if (lanes > SV.best_lanes) SV.best_lanes = (uint16_t)lanes;
    } else {
        if (total > SV.best_coop) { SV.best_coop = (uint16_t)total; new_best = 1; }
    }
    for (int p = 0; p < W.players; p++)
        if (W.d[p].bank_best > SV.bank_best) SV.bank_best = (uint16_t)W.d[p].bank_best;
    save_store();
    if (nruns_logged < 64) {
        run_scores[nruns_logged] = s0;
        run_lanes[nruns_logged++] = W.d[0].max_col - START_COL;
    }
    runs_done++;
    rs_log("run %d over at frame %u: score %d lanes %d banked %d (best %d)", runs_done, rs_frame_count(), s0,
           W.d[0].max_col - START_COL, W.d[0].banked, SV.best);
    sfx(SFX_SWISH);
    if (W.players == 1 && hu_medal_of(s0, th)) sfx(SFX_SPARKLE);
    go(DS_OVER);
}

static void sounds(void)
{
    int cam = world_cam_px(&W);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const duck *d = &W.d[p];
        int x = d->at.col * CELL - cam + CELL / 2;
        if (ev & EV_HOP) {
            int q = (d->hops * 7 + p * 3) & 3;
            sfx_at(SFX_QUACK0 + q, x, RS_PITCH_1 + ((d->hops * 37) % 9 - 4) * 48 + (p ? -600 : 0));
        }
        if (ev & EV_BUMP) sfx_at(SFX_BUMP, x, 0);
        if (ev & EV_PICK) sfx_at(SFX_PEEP, x, RS_PITCH_1 + clampi(W.ev_pick_n[p], 0, 16) * 96);
        if (ev & EV_KNOCK) { sfx_at(SFX_HORN, W.ev_knock_x[p], 0); sfx_at(SFX_PEEP, W.ev_knock_x[p], RS_PITCH_1 + 700); }
        if (ev & EV_SPLASHLING) sfx_at(SFX_PLOP, W.ev_knock_x[p], 0);
        if (ev & EV_BANK) { sfx_at(SFX_FANFARE, x, 0); click_t = 24; }
        if (ev & EV_HIT) {
            sfx_at(SFX_THUD, x, 0);
            sfx_at(SFX_HORN, x, RS_PITCH_1 - 300);
            hu_shake(3, 12);                        /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d hit at frame %u (score %d)", p + 1, rs_frame_count(), world_score(&W, p));
        }
        if (ev & EV_SWEPT) {
            sfx_at(SFX_SPLASH, x, 0);
            rs_log("player %d swept away at frame %u (score %d)", p + 1, rs_frame_count(), world_score(&W, p));
        }
        if (ev & EV_CAUGHT) {
            sfx_at(SFX_GROWL, 24, RS_PITCH_1 + 400);
            rs_log("player %d caught by the fox at frame %u (score %d)", p + 1, rs_frame_count(), world_score(&W, p));
        }
        if (ev & EV_FOXWARN) sfx_at(SFX_GROWL, 16, 0);
        if ((ev & EV_LAND) && !(ev & EV_BANK)) {
            const lane *l = world_lane(&W, d->at.col);
            if (l && l->kind == LK_NEST) sfx_at(SFX_PLOP, x, RS_PITCH_1 - 500);
        }
    }
    if (click_t >= 0 && click_t-- == 0) sfx(SFX_CLICK);
    /* the crossing bells of the railways on screen */
    if (bell_t > 0) bell_t--;
    else {
        int c0 = cam >> 4;
        for (int32_t c = c0; c <= c0 + SCREEN_COLS; c++) {
            const lane *l = world_lane(&W, c);
            int y0, y1;
            if (l && l->kind == LK_RAIL && train_at(l, W.t, &y0, &y1)) {
                sfx_at(SFX_BELL, c * CELL - cam, 0);
                bell_t = 22;
                break;
            }
        }
    }
}

static void play_update(void)
{
    int press[MAX_PLAYERS] = {0, 0};
    for (int p = 0; p < W.players; p++) {
        const duck *d = &W.d[p];
        if (d->state == DK_READY || d->state == DK_ALIVE) press[p] = hop_pressed(p);
    }
    world_step(&W, press);
    sounds();
    for (int p = 0; p < W.players; p++) {
        const duck *d = &W.d[p];
        state_hash = (state_hash ^ (uint32_t)(d->at.col * 977 + d->at.y * 13 + d->at.plat)) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(world_score(&W, p) * 131 + d->state * 7 + d->nline)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)W.cam) * 16777619u;
    if (st == DS_READY) {
        if (W.started) go(DS_PLAY);
        return;
    }
    if (st == DS_PLAY && !world_running(&W)) go(DS_DEAD);
    if (st == DS_DEAD && world_all_out(&W)) game_over();
}

static void try_join(void)
{
    /* Father Duck joins with A on pad 2, on the title or "get ready" */
    if (players == 1 && (rs_pad_pressed(1) & GO_BUTTONS)) {
        players = 2;
        sfx(SFX_JOIN);
        new_run(DS_READY);
    }
}

static void game_update(void)
{
    music_update(st == DS_TITLE ? 1 : (world_line_total(&W) >= LAYER2_AT) + (world_line_total(&W) >= LAYER1_AT));
    if ((st == DS_PLAY || st == DS_DEAD) && (rs_pad_pressed(0) & (RS_BTN_START | RS_BTN_SELECT))) {
        paused = !paused;
        sfx(SFX_PAUSE);
    }
    if (paused) return;
    hu_shake_step();
    switch (st) {
    case DS_TITLE:
        try_join();
        if (st == DS_TITLE && (opt_bot || (rs_pad_pressed(0) & (GO_BUTTONS | RS_BTN_RIGHT)))) {
            sfx(SFX_JOIN);
            go(DS_READY);
        }
        break;
    case DS_READY:
        try_join();
        play_update();
        break;
    case DS_PLAY:
    case DS_DEAD:
        play_update();
        break;
    case DS_OVER: {
        int again = 0;
        if (st_t >= RETRY_LOCK) {
            for (int p = 0; p < W.players; p++) again |= (rs_pad_pressed(p) & GO_BUTTONS) != 0;
            if (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10) again = 1;
        }
        if (again) {                               /* house rule: one button, instant retry */
            sfx(SFX_JOIN);
            new_run(DS_READY);
        }
        break;
    }
    }
    st_t++;
}

static void game_draw(void)
{
    draw_frame(&W, st, st_t, W.players == 1 ? SV.best : SV.best_coop, new_best, paused);
}

static void game_init(void)
{
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sfx_init();
    audio_set(opt_music, opt_sound);
    draw_init();
    opt_bot = rs_option_int("bot", 0);
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0xd0c5eed);
    players = clampi(rs_option_int("players", 1), 1, 2);
    new_run(rs_option_int("ready", 0) ? DS_READY : DS_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char list[600] = "", lanes[600] = "";
    for (int i = 0; i < nruns_logged; i++) {
        size_t n = strlen(list), m = strlen(lanes);
        snprintf(list + n, sizeof list - n, "%s%d", i ? "," : "", run_scores[i]);
        snprintf(lanes + m, sizeof lanes - m, "%s%d", i ? "," : "", run_lanes[i]);
    }
    rs_log("state: st=%d players=%d score=%d score2=%d lanes=%d line=%d banked=%d best=%d runs=%d ds=%d,%d cam=%d "
           "paused=%d hash=%08x judged=%d rerolls=%d fallbacks=%d runscores=%s runlanes=%s",
           st, W.players, world_score(&W, 0), W.players > 1 ? world_score(&W, 1) : 0, W.d[0].max_col - START_COL,
           W.d[0].nline, W.d[0].banked, SV.best, runs_done, W.d[0].state, W.d[1].state, world_cam_px(&W), paused,
           state_hash, W.L.g.judged, W.L.g.rerolls, W.L.g.fallbacks, list, lanes);
}

/* test hook (tests/test_ui.c): the run and the screen being shown */
const world *dp_test_world(int *state)
{
    if (state) *state = st;
    return &W;
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(players); S(state_hash);
    S(run_scores); S(run_lanes); S(nruns_logged); S(bell_t); S(click_t);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_seed_fixed); S(opt_seed); S(opt_music); S(opt_sound);
    draw_state();
    sfx_state();
    bot_state();
    hu_state();
    ha_state();
}
#undef S

static void game_state_loaded(void) { bot_state_loaded(); }

const rs_game *rs_game_main(void)
{
    /* state_version: bump it when the meaning of a saved object changes (its layout is checked) */
    static const rs_game g = {"Duck Parade", "duckparade", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, dp_assets, game_state, game_state_loaded, 1};
    return &g;
}
