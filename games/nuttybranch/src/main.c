/*
 * Nutty Branch: the game flow (the title with players 2-4 joining, play, game over, pause), input, sound, save RAM,
 * options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A
 * (B leaves); P1 starts the run at once with A (or Left / Right, B, Start: the extra start inputs); one press on the
 * game-over panel (hu_over_back) goes back to the title, with the same players.
 *
 * Controls: Left / Right steer round the branch, A (or B) jumps (hold for higher), Select (Start) pauses.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot.c: from the screen only); botruns=N runs; botstop=M lets go of
 *                  the controls at M metres (P2-P4: M+20, M+40...)
 *   seed=N         a fixed course (default: from the frame of the start)
 *   start=M        the course starts M metres in (its difficulty, its time of day)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   dump=1         log the final state at exit (tests); music=0, sound=0
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include <stdio.h>
#include <string.h>

#define JUMP_BUTTONS  (RS_BTN_A | RS_BTN_B)
#define START_EXTRA   (HU_IN_LR | RS_BTN_B | RS_BTN_START)

static int opt_bot, opt_botstop, opt_botruns, opt_seed_fixed, opt_music, opt_sound, opt_start;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE * 7 / 10];     /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best distance and score (house convention: checked, versioned, committed at each game over) --- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best_m, best_score, best_multi;
    uint16_t pad2;
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

/* ---- sound: the game's own sounds (the house synthesiser), then the house set -------------------------------------- */
static void sound_init(void)
{
    ha_synth s;
    ha_begin(&s, snd_buf, (int)(sizeof snd_buf / sizeof snd_buf[0]), 0x5eed1234u);
    ha_tone(&s, 0, 380, 980, 80, 48, 40);               /* jump: a quick upward chirp */
    ha_hiss(&s, 0, 30, 8, 2);
    ha_store(&s, SFX_JUMP, 90);
    ha_hiss(&s, 0, 22, 40, 0);                          /* crunch: a crack and a short nutty pluck */
    ha_tone(&s, ha_ms(8), 1500, 1100, 50, 44, 18);
    ha_store(&s, SFX_CRUNCH, 70);
    ha_tone(&s, 0, 150, 112, 160, 46, 0);               /* creak: a low wooden groan, two voices beating */
    ha_tone(&s, 0, 158, 118, 160, 30, 0);
    ha_hiss(&s, 0, 150, 10, 4);
    ha_store(&s, SFX_CREAK, 160);
    ha_tone(&s, 0, 1300, 320, 290, 44, 0);              /* fall: a falling whistle */
    ha_store(&s, SFX_FALL, 300);
    ha_tone(&s, 0, 950, 720, 18, 60, 6);                /* the woodpecker: tok tok */
    ha_hiss(&s, 0, 10, 30, 1);
    ha_tone(&s, ha_ms(120), 950, 720, 18, 52, 6);
    ha_hiss(&s, ha_ms(120), 10, 26, 1);
    ha_store(&s, SFX_TOK, 150);
    ha_hiss(&s, 0, 300, 38, 3);                         /* the wind: a long filtered hiss */
    ha_tone(&s, 0, 300, 520, 300, 6, 0);
    ha_store(&s, SFX_WIND, 300);
    ha_tone(&s, 0, 430, 405, 180, 44, 0);               /* the owl: hoo-hooo */
    ha_tone(&s, ha_ms(220), 400, 372, 200, 40, 0);
    ha_store(&s, SFX_HOOT, 380);
    ha_hiss(&s, 0, 50, 22, 3);                          /* land: a soft scuff */
    ha_store(&s, SFX_LAND, 60);
    ha_tone(&s, 0, 700, 420, 90, 30, 30);               /* web: a sticky fwip */
    ha_hiss(&s, 0, 80, 18, 2);
    ha_store(&s, SFX_WEB, 100);
    ha_init(SFX_HOUSE);                                 /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

/* the crunch climbs a semitone per combo nut: 2^(n/12) in 4.12 */
static const uint16_t SEMI[COMBO_MAX + 1] = {4096, 4340, 4598, 4871, 5161, 5468, 5793, 6137, 6502};

static int screen_x(const squirrel *q) { return 160 + (int)(q->ang / ANG(1)) * 85 / 60; }

static void sfx_at(int id, int x)
{
    static const uint8_t vol[SFX_COUNT] = {HA_VOL_ACTION, HA_VOL_FEEDBACK, HA_VOL_FEEDBACK, HA_VOL_CRASH, 60, 50,
                                           60, 50, HA_VOL_FEEDBACK};
    ha_play_slot(id, vol[id], x, 0);
}

/* ---- flow ------------------------------------------------------------------------------------------------------------ */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init_at(&W, hu_players(), seed, opt_start);
    bot_reset();
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

static void read_input(int p, sq_input *in)
{
    memset(in, 0, sizeof *in);
    if (is_bot(p)) {
        if (opt_botstop && W.q[p].dist_m >= opt_botstop + 20 * p) return;
        bot_decide(p, in);
        return;
    }
    int pad = hu_player_pad(p);
    uint16_t held = rs_pad(pad), pr = rs_pad_pressed(pad);
    in->steer = ((held & RS_BTN_RIGHT) ? 1 : 0) - ((held & RS_BTN_LEFT) ? 1 : 0);
    in->jump_pressed = (pr & JUMP_BUTTONS) != 0;
    in->jump_held = (held & JUMP_BUTTONS) != 0;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_METRES;
    int best_m = 0, best_s = 0;
    for (int p = 0; p < W.players; p++) {
        if (W.q[p].dist_m > best_m) best_m = W.q[p].dist_m;
        if (W.q[p].score > best_s) best_s = W.q[p].score;
    }
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.q[0].dist_m, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (best_m > SV.best_m) { SV.best_m = (uint16_t)(best_m > 65535 ? 65535 : best_m); new_best = 1; }
    if (best_s > SV.best_score) SV.best_score = (uint16_t)(best_s > 65535 ? 65535 : best_s);
    if (W.players >= 2 && best_m > SV.best_multi) SV.best_multi = (uint16_t)(best_m > 65535 ? 65535 : best_m);
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: %d m, score %d (best %d m)", runs_done, rs_frame_count(), W.q[0].dist_m,
           W.q[0].score, SV.best_m);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void play_update(void)
{
    sq_input in[MAX_PLAYERS];
    memset(in, 0, sizeof in);
    for (int p = 0; p < W.players; p++)
        if (W.q[p].state <= SQ_RUN) read_input(p, &in[p]);
    if (opt_bot && !W.started && st_t >= BOT_START) world_start(&W);   /* ready=1: the bot sets off */
    world_step(&W, in);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const squirrel *q = &W.q[p];
        int x = screen_x(q);
        if (ev & EV_JUMP) sfx_at(SFX_JUMP, x);
        if (ev & EV_NUT) {
            ha_play_pitched(SFX_CRUNCH, HA_VOL_FEEDBACK, x, SEMI[q->combo], 0);
            if (ev & EV_WALNUT) ha_play(HA_DING);
        }
        if (ev & EV_LAND) sfx_at(SFX_LAND, x);
        if (ev & EV_CREAK) sfx_at(SFX_CREAK, x);
        if (ev & EV_WEB) sfx_at(SFX_WEB, x);
        if (ev & EV_HIT) {
            ha_play(HA_THUD);
            hu_shake(3, 12);                    /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
        }
        if (ev & (EV_HIT | EV_FALL)) {
            sfx_at(SFX_FALL, x);
            rs_log("player %d out at frame %u: %d m (fall %d)", p + 1, rs_frame_count(), q->dist_m, q->fall_kind);
        }
        state_hash = (state_hash ^ (uint32_t)q->s) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)q->ang) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(q->h + q->score * 131 + q->state)) * 16777619u;
    }
    if (W.ev_world & EVW_PECK) sfx_at(SFX_TOK, 200);
    if (W.ev_world & EVW_WIND) sfx_at(SFX_WIND, 160);
    if (W.ev_world & EVW_HOOT) sfx_at(SFX_HOOT, 120);
    if (!W.started) return;                     /* ready=1: the run waits for its first press */
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && world_all_down(&W) && st_t >= PANEL_DELAY) game_over();
}

/* the title: players join and leave (the kit's lobby), P1 starts */
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
        world_start(&W);                        /* the start press sets them off (it is not a jump) */
    }
}

static void game_update(void)
{
    ha_music("music/tune.mod", opt_music);
    if ((st == ST_PLAY || st == ST_DEAD) && (rs_pad_pressed(0) & (RS_BTN_SELECT | RS_BTN_START))) {
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

static void game_draw(void) { draw_frame(&W, st, st_t, SV.best_m, new_best, paused); }

static void game_init(void)
{
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    hu_title_cfg tc = {START_EXTRA, MAX_PLAYERS, "RUN", NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_start = rs_option_int("start", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[48] = "", ds[64] = "", hs[48] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(ds), c = strlen(hs);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.q[p].score);
        snprintf(ds + b, sizeof ds - b, "%s%d", p ? "," : "", (int)W.q[p].dist_m);
        snprintf(hs + c, sizeof hs - c, "%s%d", p ? "," : "", W.q[p].state);
    }
    rs_log("state: st=%d players=%d dist=%d score=%d best=%d bestscore=%d runs=%d hs=%s s=%d paused=%d "
           "hash=%08x fall=%d dists=%s scores=%s",
           st, W.players, (int)W.q[0].dist_m, W.q[0].score, SV.best_m, SV.best_score, runs_done, hs,
           (int)(W.q[0].s >> 8), paused, state_hash, W.q[0].fall_kind, ds, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(state_hash);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_seed_fixed); S(opt_seed); S(opt_music); S(opt_sound);
    S(opt_start);
    draw_state();
    bot_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    static const rs_game g = {GAME_NAME, GAME_ID, "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, NULL, 1};
    return &g;
}
