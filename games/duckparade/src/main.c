/*
 * Duck Parade: the game flow (the title and its lobby, play, the death, game over and the results, pause), input,
 * sound events, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 *
 * The title is the only menu (docs/art-direction.md "Title and players"): pads 2-4 join there with A (or Start) and
 * leave with B (the house kit's lobby); P1 starts with any arrow or A, and that press is also Mother's first hop.
 * On the game-over panel one press (an arrow, A or Start) starts the next run at once with the same parents (its
 * press is a first hop too); Select goes back to the title.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the screen-reading bot plays parents 1..N (bot.c); botruns=N runs
 *   botstop=S      the bot stops hopping at score S (the fox then comes: screenshots of a game over)
 *   seed=N         a fixed course (default: from the frame the title (or the retry) made the run)
 *   players=N      start with N parents joined (1..4; at least bot=N);  ready=1  skip the title
 *   dump=1         log the final state at exit (tests); music=0, sound=0; strict=1 (the SDK's)
 */
#include "dp.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include <stdio.h>
#include <string.h>

#define START_INPUTS (HU_IN_DPAD | RS_BTN_A)               /* P1 starts from the title: a hop (the prompt says so) */
#define RETRY_INPUTS (START_INPUTS | RS_BTN_START)         /* the game-over panel: one press, the next run */

int opt_bot;
static int opt_botstop, opt_botruns, opt_seed_fixed, opt_music, opt_sound, opt_scenes, opt_record;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
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

/* version 2: best_coop is the family score of 2-4 parents (version 1 kept the sum of two parents' scores: that best
 * is dropped, the rest kept) */
static void save_load(void)
{
    memcpy(&SV, rs_sram(), sizeof SV);
    if (memcmp(SV.magic, "DUCK", 4) || (SV.version != 1 && SV.version != 2) || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, "DUCK", 4);
        SV.version = 2;
    }
    if (SV.version == 1) {
        SV.version = 2;
        SV.best_coop = 0;
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

/* a run with the lobby's parents; seed: the course (the title's world keeps its seed when P1 starts it) */
static void new_run_seed(int state, uint32_t seed)
{
    world_init(&W, hu_players(), seed);
    W.fixed_seed = opt_seed_fixed;
    draw_reset(&W);
    bot_reset();
    new_best = 0;
    go(state);
}

static void new_run(int state)
{
    new_run_seed(state, opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u);
}

static int is_bot(int p) { return p < opt_bot; }

/* the pad of parent p (the lobby maps players to pads): Right or A hops forward, Left back, Up and Down along the lane */
static int hop_pressed(int p)
{
    if (is_bot(p)) {
        if (opt_botstop && world_score(&W, p) >= opt_botstop && st == DS_PLAY) return 0;
        int b = bot_decide(p);
        if (b && opt_record) rs_log("press %u P%d %s", rs_frame_count(), p + 1, b == HOP_FWD ? "RIGHT" : b == HOP_BACK ? "LEFT" : b == HOP_UP ? "UP" : "DOWN");
        return b;
    }
    uint16_t b = rs_pad_pressed(hu_player_pad(p));
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
    } else {                                         /* 2-4 parents: the family score */
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
    rs_log("run %d over at frame %u: score %d lanes %d banked %d (best %d) players %d family %d", runs_done,
           rs_frame_count(), s0, W.d[0].max_col - START_COL, W.d[0].banked, SV.best, W.players, total);
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
            static const int16_t voice[MAX_PLAYERS] = {0, -600, 300, -300};   /* Father lower, P3 higher, P4 between */
            int q = (d->hops * 7 + p * 3) & 3;
            sfx_at(SFX_QUACK0 + q, x, RS_PITCH_1 + ((d->hops * 37) % 9 - 4) * 48 + voice[p]);
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

/* --opt scenes=1: the first frame of each kind of scene (tools/screenshots.sh picks its frames from them) */
static int scene_seen;
static void log_scenes(void)
{
    const duck *d = &W.d[0];
    const lane *l = world_lane(&W, d->at.col);
    int cam = world_cam_px(&W), f = (int)rs_frame_count();
    if (d->state != DK_ALIVE || !l) return;
    int x = d->at.col * CELL - cam;
    if (!(scene_seen & 1) && l->kind == LK_ROAD && !d->h.dir && x > 96 && x < 200) { scene_seen |= 1; rs_log("scene road %d", f); }
    if (!(scene_seen & 2) && l->kind == LK_RIVER && !d->h.dir && d->nline >= 2) { scene_seen |= 2; rs_log("scene river %d", f); }
    for (int32_t c = cam >> 4; c <= (cam >> 4) + SCREEN_COLS && !(scene_seen & 4); c++) {
        const lane *r = world_lane(&W, c);
        int y0, y1;
        if (r && r->kind == LK_RAIL && train_at(r, W.t, &y0, &y1) == 2 && y0 > 40 && y1 < 200 && absi(c - d->at.col) <= 3) { scene_seen |= 4; rs_log("scene train %d", f); }
    }
    if (!(scene_seen & 8) && d->nline >= 8 && !d->h.dir) { scene_seen |= 8; rs_log("scene parade %d", f); }
    if (!(scene_seen & 16) && (W.events[0] & EV_BANK) && W.ev_bank_n[0] >= 4) { scene_seen |= 16; rs_log("scene bank %d", f); }
    if (!(scene_seen & 32) && d->fox_warn && x < 40) { scene_seen |= 32; rs_log("scene fox %d", f); }
}

static void play_update(void)
{
    int press[MAX_PLAYERS] = {0};
    for (int p = 0; p < W.players; p++) {
        const duck *d = &W.d[p];
        if (d->state == DK_READY || d->state == DK_ALIVE) press[p] = hop_pressed(p);
    }
    world_step(&W, press);
    sounds();
    if (opt_scenes) log_scenes();
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

/* the title: parents join (A or Start on pads 2-4: a quack in their voice) and leave (B); P1's arrow or A starts the
 * run with the title's course, and that press is also Mother's first hop (DS_READY: the run starts with the first
 * hop that is not blocked) */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) {
        int p = hu_players() - 1;
        static const int16_t voice[MAX_PLAYERS] = {0, -600, 300, -300};
        sfx(SFX_JOIN);
        sfx_at(SFX_QUACK0 + (p & 3), RS_SCREEN_W / 2, RS_PITCH_1 + voice[p & 3]);
    }
    if (ev & HU_TITLE_LEFT) sfx(SFX_BUMP);
    if (opt_bot) {                                 /* the bot: straight into the run (it hops by itself) */
        new_run_seed(DS_READY, W.seed);
    } else if (ev & HU_TITLE_START) {
        new_run_seed(DS_READY, W.seed);
        play_update();                             /* the start press is the first hop */
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
        title_update();
        break;
    case DS_READY:
    case DS_PLAY:
    case DS_DEAD:
        play_update();
        break;
    case DS_OVER: {
        int again = 0, back = 0, bot_again = 0;
        if (st_t >= RETRY_LOCK) {
            for (int p = 0; p < W.players; p++) {
                uint16_t b = rs_pad_pressed(hu_player_pad(p));
                again |= (b & RETRY_INPUTS) != 0;
                back |= (b & RS_BTN_SELECT) != 0;
            }
            if (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10) bot_again = 1;
        }
        if (back) {                                /* Select: back to the title (parents join or leave there) */
            sfx(SFX_BUMP);
            new_run(DS_TITLE);
        } else if (again || bot_again) {           /* house rule: one press, instant retry, the same parents */
            sfx(SFX_JOIN);
            new_run(DS_READY);
            if (again) play_update();              /* the press is the first hop (an arrow or A) */
        }
        break;
    }
    }
    st_t++;
}

static void game_draw(void)
{
    int coop = (st == DS_TITLE ? hu_players() : W.players) > 1;
    draw_frame(&W, st, st_t, coop ? SV.best_coop : SV.best, new_best, paused);
}

static void game_init(void)
{
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sfx_init();
    audio_set(opt_music, opt_sound);
    draw_init();                                   /* hu_init (draw.c) */
    /* the title: P1 starts with an arrow or A ("PRESS ANY ARROW TO HOP", the D-pad glyph), up to 4 parents */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, "HOP", NULL, 0};
    hu_title_setup(&tc);
    opt_bot = clampi(rs_option_int("bot", 0), 0, MAX_PLAYERS);
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_scenes = rs_option_int("scenes", 0);
    opt_record = rs_option_int("record", 0);   /* log the bot's presses as an input script (tools/bench.sh) */
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0xd0c5eed);
    int n = clampi(rs_option_int("players", 1), 1, MAX_PLAYERS);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);   /* bot=N plays parents 1..N */
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
    rs_log("state: st=%d players=%d score=%d score2=%d score3=%d score4=%d family=%d lanes=%d line=%d banked=%d best=%d "
           "runs=%d ds=%d,%d,%d,%d cols=%d,%d,%d,%d lines=%d,%d,%d,%d lobby=%d cam=%d paused=%d hash=%08x judged=%d "
           "rerolls=%d fallbacks=%d bestcoop=%d runscores=%s runlanes=%s",
           st, W.players, world_score(&W, 0), W.players > 1 ? world_score(&W, 1) : 0,
           W.players > 2 ? world_score(&W, 2) : 0, W.players > 3 ? world_score(&W, 3) : 0, world_total(&W),
           W.d[0].max_col - START_COL, W.d[0].nline, W.d[0].banked, SV.best, runs_done, W.d[0].state, W.d[1].state,
           W.d[2].state, W.d[3].state, (int)W.d[0].at.col, (int)W.d[1].at.col, (int)W.d[2].at.col, (int)W.d[3].at.col,
           W.d[0].nline, W.d[1].nline, W.d[2].nline, W.d[3].nline, hu_players(), world_cam_px(&W), paused, state_hash,
           W.L.g.judged, W.L.g.rerolls, W.L.g.fallbacks, SV.best_coop, list, lanes);
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
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(state_hash);
    S(run_scores); S(run_lanes); S(nruns_logged); S(bell_t); S(click_t); S(scene_seen); S(opt_scenes); S(opt_record);
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
                              game_shutdown, dp_assets, game_state, game_state_loaded, 2};
    return &g;
}
