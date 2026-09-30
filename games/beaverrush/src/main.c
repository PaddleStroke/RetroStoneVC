/*
 * Beaver Rush: the game flow (title, get ready, play, the end of a run, game over), input, save RAM, options,
 * save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=1          the screen-reading bot plays player 1 (bot.c), bot=2 both players; botruns=N runs
 *   botstop=S      the bot stops gnawing at score S (a game over for the screenshots)
 *   seed=N         a fixed tree (default: from the frame of the new run and of the first gnaw)
 *   skip=N         start the run with N logs gnawed (the dam, the scene and the level of N logs; tests)
 *   players=2      start with player 2 joined (versus);  ready=1  skip the title
 *   dump=1         log the final state at exit (tests); music=0, sound=0
 */
#include "br.h"
#include "draw.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include <stdio.h>
#include <string.h>

#define LEFT_BUTTONS  (RS_BTN_LEFT | RS_BTN_B | RS_BTN_Y)
#define RIGHT_BUTTONS (RS_BTN_RIGHT | RS_BTN_A | RS_BTN_X)
#define GNAW_BUTTONS  (LEFT_BUTTONS | RIGHT_BUTTONS)

int opt_bot;
static int opt_botruns, opt_botstop, opt_skip, opt_seed_fixed;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done, players = 1;
static uint32_t state_hash = 2166136261u;
static int run_scores[64], nrun_scores;

/* ---- save RAM (the house convention: checked, versioned, committed at each game over) --------------------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best, best_vs;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t vs_wins[2];
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
    if (memcmp(SV.magic, "BVRU", 4) || SV.version != 1 || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, "BVRU", 4);
        SV.version = 1;
    }
}

static void save_store(void)
{
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* ---- flow ------------------------------------------------------------------------------------------------------------ */
static void go(int s) { st = s; st_t = 0; }

static uint32_t frame_seed(void) { return opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u; }

static void new_run(int state)
{
    world_init(&W, players, opt_seed_fixed ? opt_seed : frame_seed());
    if (opt_skip) world_skip(&W, opt_skip);
    draw_new_run(&W);
    bot_reset(W.seed);
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return (p == 0 && opt_bot) || (p == 1 && opt_bot >= 2); }

/* SIDE_L / SIDE_R when player p gnaws this frame */
static int gnaw_pressed(int p)
{
    if (is_bot(p)) {
        if (opt_botstop && W.bv[p].score >= opt_botstop && st == DS_PLAY) return 0;
        return bot_decide(p);
    }
    uint16_t b = rs_pad_pressed(p);
    if (b & LEFT_BUTTONS) return SIDE_L;
    if (b & RIGHT_BUTTONS) return SIDE_R;
    return 0;
}

static void game_over(void)
{
    static const int medals[4] = {MEDAL_T1, MEDAL_T2, MEDAL_T3, MEDAL_T4};
    int best_now = 0;
    for (int p = 0; p < W.players; p++)
        if (W.bv[p].score > best_now) best_now = W.bv[p].score;
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.bv[0].score, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (W.players == 1 && best_now > SV.best) { SV.best = (uint16_t)best_now; new_best = 1; }
    if (W.players == 2) {
        if (best_now > SV.best_vs) SV.best_vs = (uint16_t)best_now;
        if (W.winner >= 0) SV.vs_wins[W.winner]++;
    }
    if (!opt_skip) save_store();                  /* a skipped start (tests) does not count */
    if (nrun_scores < 64) run_scores[nrun_scores++] = W.bv[0].score;
    runs_done++;
    rs_log("run %d over at frame %u: score %d (best %d)", runs_done, rs_frame_count(), W.bv[0].score, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(DS_OVER);
}

static void play_update(void)
{
    int press[MAX_PLAYERS] = {0, 0};
    for (int p = 0; p < W.players; p++) {
        int s = W.bv[p].state;
        if (s == BV_READY || s == BV_PLAY) press[p] = gnaw_pressed(p);
    }
    uint32_t reroll = (!W.started && !opt_seed_fixed && !opt_skip) ? frame_seed() | 1 : 0;
    world_step(&W, press, reroll);
    draw_events(&W);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const beaver *b = &W.bv[p];
        int x = W.players == 2 ? (p ? 240 : 80) : (b->side == SIDE_L ? 130 : 190);
        if (ev & EV_GNAW) sfx_pan(SFX_CHOMP, x, 0);
        if (ev & EV_GOLD) sfx_pan(SFX_GOLD, x, 0);
        if (ev & EV_MILESTONE) sfx_pan(SFX_CHEER, x, 0);
        if (ev & (EV_SENT | EV_STOLEN)) sfx_pan(SFX_WHOOSH, x, 0);
        if (ev & EV_BONK) {
            sfx_pan(SFX_BONK, x, 0);
            hu_shake(3, 12);                  /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d bonked (hit %c) at frame %u (score %d)", p + 1, b->bonk_hit == 1 ? 'A' : 'B',
                   rs_frame_count(), b->score);
        }
        if (ev & EV_SLEEP) {
            sfx_pan(SFX_SLEEP, x, 0);
            rs_log("player %d out of breath at frame %u (score %d)", p + 1, rs_frame_count(), b->score);
        }
        state_hash = (state_hash ^ (uint32_t)(b->bar >> 8)) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(b->score * 131 + b->side * 7 + b->state)) * 16777619u;
    }
    if ((st == DS_READY || st == DS_TITLE) && W.started) go(DS_PLAY);
    if (st == DS_PLAY && W.over) go(DS_END);
    if (st == DS_END && st_t >= PANEL_DELAY) game_over();
}

static void try_join(void)
{
    /* player 2 joins with a gnaw button on pad 2, on the title or "get ready" */
    if (players == 1 && !opt_bot && (rs_pad_pressed(1) & (GNAW_BUTTONS | RS_BTN_START))) {
        players = 2;
        sfx(SFX_JOIN);
        new_run(DS_READY);
    }
}

static void game_update(void)
{
    int lvl = W.bv[0].level > W.bv[1].level ? W.bv[0].level : W.bv[1].level;
    music_update(lvl, st == DS_PLAY);
    if (st == DS_PLAY && (rs_pad_pressed(0) & RS_BTN_START)) {
        paused = !paused;
        sfx(SFX_PAUSE);
    }
    if (paused) return;
    switch (st) {
    case DS_TITLE:
        try_join();
        if (st == DS_TITLE && (rs_pad_pressed(0) & RS_BTN_START)) {
            sfx(SFX_JOIN);
            go(DS_READY);
            break;
        }
        play_update();                        /* a gnaw starts the run at once */
        break;
    case DS_READY:
        try_join();
        play_update();
        break;
    case DS_PLAY:
    case DS_END:
        play_update();
        break;
    case DS_OVER: {
        int again = 0;
        if (st_t == 1) sfx(SFX_PANEL);
        if (st_t >= RETRY_LOCK) {
            for (int p = 0; p < W.players; p++) again |= (rs_pad_pressed(p) & (GNAW_BUTTONS | RS_BTN_START)) != 0;
            if (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10) again = 1;
        }
        if (again) {                          /* house rule: one button, instant retry */
            sfx(SFX_JOIN);
            new_run(DS_READY);
        }
        break;
    }
    }
    draw_update(&W, st);
    st_t++;
}

static void game_draw(void) { draw_frame(&W, st, st_t, W.players == 2 ? SV.best_vs : SV.best, new_best, paused); }

static void game_init(void)
{
    save_load();
    audio_set(rs_option_int("music", 1), rs_option_int("sound", 1));
    sfx_init();
    draw_init();
    opt_bot = rs_option_int("bot", 0);
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_skip = clampi(rs_option_int("skip", 0), 0, 5000);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0xbea7e5);
    players = clampi(rs_option_int("players", 1), 1, 2);
    new_run(rs_option_int("ready", 0) || players == 2 ? DS_READY : DS_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char list[400] = "";
    for (int i = 0; i < nrun_scores; i++) {
        size_t n = strlen(list);
        snprintf(list + n, sizeof list - n, "%s%d", i ? "," : "", run_scores[i]);
    }
    rs_log("state: st=%d players=%d score=%d score2=%d best=%d runs=%d bv=%d,%d side=%d logs=%d bar=%d paused=%d "
           "winner=%d stage=%d level=%d tempo=%d dam=%d hash=%08x runscores=%s",
           st, W.players, W.bv[0].score, W.bv[1].score, SV.best, runs_done, W.bv[0].state, W.bv[1].state,
           W.bv[0].side, W.bv[0].logs, (int)((int64_t)W.bv[0].bar * 1000 / BAR_FULL), paused, W.winner,
           world_stage(&W), W.bv[0].level, music_tempo(), dam_logs_drawn(), state_hash, list);
}

/* test hook (tests): the run and the screen being shown */
const world *br_test_world(int *state)
{
    if (state) *state = st;
    return &W;
}

/* ---- save states: the SDK saves the console (VRAM, maps, palettes, sprites, voices, music, RNG, pads); here are the
 * game's own objects (tools/state_audit.py checks that none is forgotten). Not saved: SV, the battery save's copy. ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(players); S(state_hash); S(run_scores);
    S(nrun_scores); S(opt_bot); S(opt_botruns); S(opt_botstop); S(opt_skip); S(opt_seed_fixed); S(opt_seed);
    draw_state();
    sfx_state();
    bot_state();
    hu_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* state_version: bump it when the meaning of a saved object changes (its layout is checked) */
    static const rs_game g = {"Beaver Rush", "beaverrush", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, br_assets, game_state, NULL, 1};
    return &g;
}
