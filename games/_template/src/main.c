/*
 * @NAME@: the game flow (title, get ready, play, game over, pause, player 2), input, sound, save RAM,
 * options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/@ID@/LICENSE.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=1          the bot plays player 1 (bot_decide), bot=2 both players; botruns=N runs; botstop=S stops at S
 *   seed=N         a fixed course (default: from the frame of the first press)
 *   players=2      start with player 2 joined;  ready=1  skip the title
 *   dump=1         log the final state at exit (tests); music=0, sound=0
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include <stdio.h>
#include <string.h>

/* the house buttons: A (and B, X, Y, Up) acts, Start starts and retries, Select (or Start in a run) pauses */
#define ACT_BUTTONS (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y | RS_BTN_UP)
#define GO_BUTTONS  (ACT_BUTTONS | RS_BTN_START)

static int opt_bot, opt_botstop, opt_botruns, opt_seed_fixed, opt_music, opt_sound;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done, players = 1;
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) -------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best, best_race;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "@MAGIC@";      /* 4 letters of the game (tools/new_game.py) */

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
    ha_tone(&s, 0, 300, 700, 90, 60, 50);       /* jump: a quick rising blip */
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

/* ---- the bot (test hook): jumps when a crate is close; plays from the world, not the pad ------------------------- */
int bot_decide(const world *w, int player)
{
    const hero *h = &w->h[player];
    if (h->state == HS_READY) return (w->t % 40) == 20;
    if (!hero_on_ground(h)) return 0;
    int d = world_block_ahead(w, player);
    return d >= 0 && d <= 22 + (w->t % 3);
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, players, seed);
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return (p == 0 && opt_bot) || (p == 1 && opt_bot >= 2); }

static int act_pressed(int p)
{
    if (is_bot(p)) {
        if (opt_botstop && W.h[p].score >= opt_botstop && st == ST_PLAY) return 0;
        return bot_decide(&W, p);
    }
    return (rs_pad_pressed(p) & ACT_BUTTONS) != 0;
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
    if (best_now > SV.best) { SV.best = (uint16_t)best_now; new_best = 1; }
    if (W.players == 2 && best_now > SV.best_race) SV.best_race = (uint16_t)best_now;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d (best %d)", runs_done, rs_frame_count(), W.h[0].score, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void play_update(void)
{
    int press[MAX_PLAYERS] = {0, 0};
    for (int p = 0; p < W.players; p++) {
        const hero *h = &W.h[p];
        if (h->state == HS_READY || h->state == HS_RUN) press[p] = act_pressed(p);
    }
    world_step(&W, press);
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
    if (st == ST_READY) {
        if (W.started) go(ST_PLAY);
        return;
    }
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && world_all_down(&W)) {
        int t = 0;
        for (int p = 0; p < W.players; p++) if (W.h[p].t > t) t = W.h[p].t;
        if (t >= PANEL_DELAY) game_over();
    }
}

static void try_join(void)
{
    /* player 2 joins with A on pad 2, on the title or "get ready" */
    if (players == 1 && (rs_pad_pressed(1) & GO_BUTTONS)) {
        players = 2;
        ha_play(HA_CONFIRM);
        new_run(ST_READY);
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
        try_join();
        if (st == ST_TITLE && (opt_bot || (rs_pad_pressed(0) & GO_BUTTONS))) {
            ha_play(HA_CONFIRM);
            go(ST_READY);
        }
        break;
    case ST_READY:
        try_join();
        play_update();
        break;
    case ST_PLAY:
    case ST_DEAD:
        play_update();
        break;
    case ST_OVER: {
        int again = 0;
        if (st_t >= RETRY_LOCK) {
            for (int p = 0; p < W.players; p++) again |= (rs_pad_pressed(p) & GO_BUTTONS) != 0;
            if (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10) again = 1;
        }
        if (again) {                            /* house rule: one button, instant retry */
            ha_play(HA_CONFIRM);
            new_run(ST_READY);
        }
        break;
    }
    }
    st_t++;
}

static void game_draw(void) { draw_frame(&W, st, st_t, SV.best, new_best, paused); }

static void game_init(void)
{
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();
    opt_bot = rs_option_int("bot", 0);
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    players = rs_option_int("players", 1) >= 2 ? 2 : 1;
    new_run(rs_option_int("ready", 0) ? ST_READY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    rs_log("state: st=%d players=%d score=%d score2=%d best=%d runs=%d hs=%d,%d y=%d scroll=%d paused=%d hash=%08x",
           st, W.players, W.h[0].score, W.h[1].score, SV.best, runs_done, W.h[0].state, W.h[1].state,
           (int)(W.h[0].y >> 16), (int)W.scroll, paused, state_hash);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(players); S(state_hash);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_seed_fixed); S(opt_seed); S(opt_music); S(opt_sound);
    draw_state();
    hu_state();
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    static const rs_game g = {"@NAME@", "@ID@", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, NULL, 1};
    return &g;
}
