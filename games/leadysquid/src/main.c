/*
 * Leady Squid: the game flow (the title with players 2-4 joining, play, death, game over), input,
 * save RAM, options and the test hooks. The house flow (docs/art-direction.md, "Title and players"): no "get
 * ready", the title is the only menu; P1's flap (a swim input: A, B, X, Y, Up, Start) starts the race at once; one
 * press on the panel starts the next race at once with the same players (Select: back to the title).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the screen-reading bot plays players 1..N (bot.c); botruns=N runs
 *   botstop=S      the bot stops flapping at score S (screenshots of a game over; players 2-4 at S+2, S+4, S+6)
 *   seed=N         a fixed course (default: from the frame of the first flap)
 *   skip=N         start the course at obstacle N (score N): skip=10 coral, 20 masts, 30 chains
 *   players=N      start with N players joined (1..4)
 *   ready=1        skip the title (the run waits for its first flap)
 *   dump=1         log the final state at exit (tests); music=0, sound=0
 */
#include "ls.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>

#define FLAP_BUTTONS (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y | RS_BTN_UP | RS_BTN_START)

int opt_bot, opt_botstop;
static int opt_botruns, opt_skip, opt_seed_fixed;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int retry_flap;                     /* the retry press is also P1's first flap */
static int landed_fx[MAX_PLAYERS];
static uint32_t state_hash = 2166136261u;
static int best_run_score, bot_scores[64], nbot_scores;

/* ---- save RAM ------------------------------------------------------------------------------------ */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best, best_race;
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
    if (memcmp(SV.magic, "LSQD", 4) || SV.version != 1 || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, "LSQD", 4);
        SV.version = 1;
    }
}

static void save_store(void)
{
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* ---- flow ------------------------------------------------------------------------------------------ */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed, opt_skip);
    W.fixed_seed = opt_seed_fixed;
    draw_reset_course(&W);
    memset(landed_fx, 0, sizeof landed_fx);
    bot_reset();
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

static int flap_pressed(int p)
{
    if (p == 0 && retry_flap) return 1;
    if (st == DS_TITLE && p > 0) return 0;    /* on the title only P1 starts (pads 2-4 join there) */
    if (is_bot(p)) {
        if (opt_botstop && W.sq[p].score >= opt_botstop + 2 * p && st == DS_PLAY) return 0;   /* P2-P4: +2 each */
        return bot_decide(p);
    }
    return (rs_pad_pressed(hu_player_pad(p)) & FLAP_BUTTONS) != 0;
}

static void game_over(void)
{
    int best_now = 0;
    for (int p = 0; p < W.players; p++)
        if (W.sq[p].score > best_now) best_now = W.sq[p].score;
    SV.runs++;
    if (W.players == 1) {
        int m = medal_of(W.sq[0].score);
        if (m) SV.medals[m - 1]++;
    }
    if (best_now > SV.best) { SV.best = (uint16_t)best_now; new_best = 1; }
    if (W.players >= 2 && best_now > SV.best_race) SV.best_race = (uint16_t)best_now;
    if (!opt_skip) save_store();              /* a skipped start (tests) does not count */
    if (best_now > best_run_score) best_run_score = best_now;
    if (nbot_scores < 64) bot_scores[nbot_scores++] = W.sq[0].score;
    runs_done++;
    rs_log("run %d over at frame %u: score %d (best %d)", runs_done, rs_frame_count(), W.sq[0].score, SV.best);
    sfx(SFX_SWISH);
    if (medal_of(W.sq[0].score)) sfx(SFX_SPARKLE);
    go(DS_OVER);
}

static void play_update(void)
{
    int flap[MAX_PLAYERS] = {0};
    for (int p = 0; p < W.players; p++) {
        squid *s = &W.sq[p];
        if (s->state == SQ_READY || s->state == SQ_SWIM) flap[p] = flap_pressed(p);
        if (flap[p] && is_bot(p)) bot_notify_flap(p);
    }
    retry_flap = 0;
    world_step(&W, flap);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const squid *s = &W.sq[p];
        int cy = (int)(s->y >> 16);
        if (ev & EV_FLAP) { sfx_pan(SFX_BLOOP, s->x); fx_flap(s->x, cy); }
        if (ev & EV_SCORE) sfx_pan(SFX_DING, s->x);
        if (ev & EV_HIT) {
            sfx_pan(SFX_THUD, s->x);
            rs_log("player %d hit at frame %u (score %d)", p + 1, rs_frame_count(), s->score);
        }
        if (ev & EV_LAND) {
            sfx_pan(SFX_CLANK, s->x);
            fx_land(s->x, SEABED_Y);
        }
        state_hash = (state_hash ^ (uint32_t)s->y) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(s->score * 131 + s->state)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)W.scroll) * 16777619u;
    if (st == DS_READY || st == DS_TITLE) {
        if (W.started) go(DS_PLAY);
        return;
    }
    if (st == DS_PLAY && !world_running(&W)) go(DS_DEAD);
    if (st == DS_DEAD && world_all_resting(&W)) {
        int t = 1 << 30;                      /* the panel waits after the LAST squid lands */
        for (int p = 0; p < W.players; p++) if (W.sq[p].t < t) t = W.sq[p].t;
        if (t >= PANEL_DELAY) game_over();
    }
    /* 2 players: the scroll goes on while one swims */
    if (st == DS_DEAD && world_running(&W)) go(DS_PLAY);
}

static void game_update(void)
{
    /* pause: Select during a run */
    if ((st == DS_PLAY) && (rs_pad_pressed(0) & RS_BTN_SELECT)) {
        paused = !paused;
        sfx(SFX_PAUSE);
    }
    if (paused) return;
    switch (st) {
    case DS_TITLE: {
        music_start();
        /* players 2-4 join with A (or Start) on their pad, leave with B (the house kit's lobby) */
        int ev = hu_title_update();
        if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) {
            sfx(ev & HU_TITLE_JOINED ? SFX_JOIN : SFX_PAUSE);
            int t = st_t;
            new_run(DS_TITLE);                /* the new squids bob in the water at once */
            st_t = t;
            break;
        }
        play_update();                        /* P1's flap starts the race */
        break;
    }
    case DS_READY:
        music_start();
        play_update();
        break;
    case DS_PLAY:
    case DS_DEAD:
        play_update();
        break;
    case DS_OVER:
        if (st_t >= RETRY_LOCK && (hu_over_back() ||
            (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10))) {
            sfx(SFX_JOIN);
            new_run(DS_TITLE);
        }
        break;
    }
    fx_update(&W);
    st_t++;
}

static void game_draw(void)
{
    draw_frame(&W, st, st_t, SV.best, new_best, paused);
}

static void game_init(void)
{
    save_load();
    audio_set(rs_option_int("music", 1), rs_option_int("sound", 1));
    sfx_init();
    draw_init();
    /* the title: P1's start inputs are its swim inputs (PRESS A TO SWIM), up to 4 players */
    hu_title_cfg tc = {FLAP_BUTTONS, MAX_PLAYERS, "SWIM", NULL, 0};
    hu_title_setup(&tc);
    opt_bot = clampi(rs_option_int("bot", 0), 0, MAX_PLAYERS);
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_skip = clampi(rs_option_int("skip", 0), 0, 100000);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x51d5eed);
    int n = clampi(rs_option_int("players", 1), 1, MAX_PLAYERS);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? DS_READY : DS_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char list[400] = "";
    for (int i = 0; i < nbot_scores; i++) {
        size_t n = strlen(list);
        snprintf(list + n, sizeof list - n, "%s%d", i ? "," : "", bot_scores[i]);
    }
    char sc[48] = "";
    for (int p = 0; p < W.players; p++) {
        size_t n = strlen(sc);
        snprintf(sc + n, sizeof sc - n, "%s%d", p ? "," : "", W.sq[p].score);
    }
    rs_log("state: st=%d players=%d score=%d score2=%d best=%d runs=%d sq=%d,%d y=%d scroll=%d paused=%d "
           "hash=%08x runscores=%s scores=%s",
           st, W.players, W.sq[0].score, W.sq[1].score, SV.best, runs_done, W.sq[0].state, W.sq[1].state,
           (int)(W.sq[0].y >> 16), world_scroll_px(&W), paused, state_hash, list, sc);
}

/* test hook (tests/test_caps.c): the run and the screen being shown */
const world *ls_test_world(int *state)
{
    if (state) *state = st;
    return &W;
}

/* ---- save states: the SDK saves the console (VRAM, maps, palettes, sprites, voices, music, RNG, pads); here
 * are the game's own objects (tools/state_audit.py checks that no mutable static is forgotten). Not saved: SV,
 * the battery save's copy (best scores and medals stay in the .srm: a state never takes them back). ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(retry_flap); S(landed_fx); S(state_hash);
    S(best_run_score); S(bot_scores); S(nbot_scores);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_skip); S(opt_seed_fixed); S(opt_seed);
    draw_state();
    sfx_state();
    bot_state();
}
#undef S

static void game_state_loaded(void) { bot_state_loaded(); }

const rs_game *rs_game_main(void)
{
    /* state_version: bump it when the meaning of a saved object changes (its layout is checked) */
    static const rs_game g = {"Leady Squid", "leadysquid", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, ls_assets, game_state, game_state_loaded, 1};
    return &g;
}
