/*
 * Llama Drama: the game flow (the title with players 2-4 joining, play, game over, pause), input (pads and touch),
 * sound, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A
 * (B leaves); P1 starts the run at once with A (or an extra START INPUT: that press also spits); one press on the
 * game-over panel (hu_over_back) goes BACK TO THE TITLE, with the same players.
 * The pads: UP/DOWN aim (the head and the neck tilt), A spits (B, X, Y too).
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Controls"): a finger put down to the right
 * of the llama's mouth turns its head toward that spot (within the aim range) and spits there at once (the same reload
 * rule); while it stays down the head follows it. A tap (a finger lifted) starts from the title and leaves the
 * game-over panel. Touches are in screen pixels: play.c gets them in stage pixels (the same at every width).
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_decide); botruns=N runs;
 *                  botstop=S: once its score reaches S (P2-P4: S+20, S+40..) it stops playing (the tests' game over)
 *   botskill=E     a human-like bot (1..10: worse and worse), to tune the medals
 *   seed=N         a fixed run (default: from the frame the title was shown)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   score=N        (screenshots, tests) P1 starts with N points: the ramp and the sky as later in a run
 *   msg=K          (the language test) the run shows message K: 1 the dart, 2 the big hornet, 3 the swarm
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

/* the house buttons: A (B, X, Y; Up and Down) starts; A, B, X, Y spit; Select (or Start in a run) pauses */
#define SPIT_BUTTONS  (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y)
#define START_INPUTS  (RS_BTN_B | RS_BTN_X | RS_BTN_Y | RS_BTN_START | RS_BTN_UP | RS_BTN_DOWN)

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound, opt_score;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static int start_press;                       /* P1's start press on the title: the run starts at once (and spits) */
static int touch_used;                        /* the run was started or played by touch: the touch hint */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) -------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint32_t pops;
    uint16_t sum, pad2;
} save_data;
static save_data SV;
static const char MAGIC[5] = "LLAM";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x4c4c;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x11a3a5u);
    /* the lengths keep the sample memory under the 64 KiB guideline with the house set */
    ha_hiss(&s, 0, 60, 30, 1);                              /* a spit: "ptoo!" (a puff of air, a wet pop) */
    ha_tone(&s, 0, 900, 420, 50, 34, 25);
    ha_tone(&s, ha_ms(20), 1800, 2600, 25, 14, 10);
    ha_store(&s, SFX_SPIT, 80);
    ha_tone(&s, 0, 620, 240, 110, 52, 50);                  /* a hornet popped: a cartoon "boing" ... */
    ha_tone(&s, ha_ms(30), 240, 420, 120, 30, 60);
    ha_tone(&s, ha_ms(50), 2637, 3136, 60, 14, 30);         /* ... and a twinkle of stars */
    ha_store(&s, SFX_POP, 170);
    ha_tone(&s, 0, 1400, 1300, 40, 30, 15);                 /* stung: a zap, "ouch!" falling, a wobble */
    ha_tone(&s, ha_ms(30), 700, 260, 200, 52, 90);
    ha_tone(&s, ha_ms(80), 330, 300, 140, 20, 60);
    ha_hiss(&s, 0, 90, 22, 2);
    ha_store(&s, SFX_STING, 240);
    ha_tone(&s, 0, 300, 180, 60, 44, 25);                   /* a big hornet hit: a thwack */
    ha_hiss(&s, 0, 40, 26, 1);
    ha_store(&s, SFX_BIG_HIT, 70);
    for (int i = 0; i < 6; i++)                             /* a dart winds up: an angry buzz rising */
        ha_tone(&s, ha_ms(i * 30), 180 + i * 30, 200 + i * 30, 30, 18 + i * 2, 0);
    ha_store(&s, SFX_DART, 190);
    ha_tone(&s, 0, 160, 110, 60, 30, 25);                   /* a bonked hornet lands in the grass: a soft thump */
    ha_hiss(&s, 0, 30, 10, 3);
    ha_store(&s, SFX_LAND, 65);
    ha_tone(&s, 0, 300, 360, 120, 34, 0);                   /* the llamas' hum at the start: "mmmh?" */
    ha_tone(&s, ha_ms(120), 360, 420, 110, 30, 60);
    ha_store(&s, SFX_HUM, 240);
    ha_hiss(&s, 0, 40, 16, 2);                              /* the spit lands: a tiny splat */
    ha_tone(&s, 0, 500, 300, 30, 10, 10);
    ha_store(&s, SFX_SPLAT, 45);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx_at(int id, int x, int vol)
{
    ha_play_slot(id, vol, x < 0 ? 0 : x > 319 ? 319 : x, id == SFX_POP);
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    W.l[0].score = opt_score;
    int msg = rs_option_int("msg", 0);          /* the language test: a message on screen */
    if (msg > MSG_NONE && msg <= MSG_SWARM) { W.msg = msg; W.msg_t = 1 << 20; }
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

/* ---- touch (player 1) ------------------------------------------------------------------------------------------- */
/* A finger was lifted this frame that went down during the last `since` frames (on this screen, after its lock):
 * the touch screen's "press A" (a tap; a finger still down from the run before never counts). */
static int touch_lifted(int since)
{
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (t && t->released && t->frames < since) return 1;
    }
    return 0;
}

/* in play: a finger put down aims and spits there; one held down, the head follows it (stage pixels) */
static void touch_aim(pin *in)
{
    const rs_touch_point *t = NULL;
    for (int i = 0; i < RS_TOUCH_MAX; i++) {           /* a new finger first, else the one down the longest */
        const rs_touch_point *f = rs_touch(i);
        if (f && f->pressed) t = f;
    }
    if (!t) t = rs_touch_first();
    if (!t || !t->down) return;
    in->touch = t->pressed ? 1 : 2;
    in->tx = t->x - rs_screen_origin_x();
    in->ty = t->y;
    touch_used = 1;
}

static pin player_input(int p)
{
    pin in = {0, 0, 0, 0, 0, 0};
    if (p == 0 && start_press) in.any = 1;
    if (is_bot(p)) {
        if (opt_botstop && W.l[p].score >= opt_botstop + 20 * p && st == ST_PLAY && W.started) return in;
        pin b = bot_decide(&W, p, opt_botskill);
        b.any |= in.any;
        return b;
    }
    int pad = hu_player_pad(p);
    uint16_t held = rs_pad(pad);
    in.dy = (held & RS_BTN_DOWN ? 1 : 0) - (held & RS_BTN_UP ? 1 : 0);
    if (rs_pad_pressed(pad) & SPIT_BUTTONS) in.spit = 1;
    if (held & SPIT_BUTTONS) in.any = 1;
    if (p == 0) touch_aim(&in);
    return in;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int best_now = 0;
    for (int p = 0; p < W.players; p++) {
        if (W.l[p].score > best_now) best_now = W.l[p].score;
        SV.pops += (uint32_t)W.l[p].pops;
    }
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.l[0].score, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (best_now > run_best) { run_best = best_now; new_best = 1; }
    if ((uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
    if (W.players >= 2 && (uint32_t)best_now > SV.best_multi) SV.best_multi = (uint32_t)best_now;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d pops %d heat %d (best %u)", runs_done, rs_frame_count(), W.l[0].score,
           W.l[0].pops, W.heat, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void play_update(void)
{
    pin in[MAX_PLAYERS];
    for (int p = 0; p < MAX_PLAYERS; p++) {
        pin none = {0, 0, 0, 0, 0, 0};
        in[p] = p < W.players ? player_input(p) : none;
    }
    start_press = 0;
    world_step(&W, in);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const llama *l = &W.l[p];
        int32_t mx, my;
        llama_mouth(p, l->aim, &mx, &my);
        int x = (int)(mx >> 16);
        if (ev & EV_SPIT) sfx_at(SFX_SPIT, x, HA_VOL_ACTION);
        if (ev & EV_POP) sfx_at(SFX_POP, 200, HA_VOL_FEEDBACK);
        if (ev & EV_BIG_HIT) sfx_at(SFX_BIG_HIT, 200, HA_VOL_FEEDBACK);
        if (ev & EV_START) sfx_at(SFX_HUM, x, 60);
        if (ev & EV_HIT) {
            sfx_at(SFX_STING, x, HA_VOL_CRASH);
            hu_shake(3, 12);                    /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d stung at frame %u (score %d, heat %d; a hornet of kind %d, pattern %d)", p + 1,
                   rs_frame_count(), l->score, W.heat, W.sting_kind, W.sting_pat);
        }
        state_hash = (state_hash ^ (uint32_t)l->aim) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(l->score * 131 + l->state * 7 + l->spits)) * 16777619u;
    }
    int wev = W.wevents;
    if (wev & WEV_DART) sfx_at(SFX_DART, 288, 50);
    if (wev & WEV_LAND) sfx_at(SFX_LAND, 160, 40);
    if (wev & WEV_MSG) ha_play(HA_DING);
    for (int k = 0; k < MAX_HORNETS; k++)
        if (W.hn[k].on) state_hash = (state_hash ^ ((uint32_t)W.hn[k].x ^ ((uint32_t)W.hn[k].y << 3))) * 16777619u;
    state_hash = (state_hash ^ (uint32_t)(W.t + W.waves * 977 + W.heat * 31)) * 16777619u;
    if (!W.started) return;                     /* ready=1: the run waits for its first press */
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && world_all_out(&W)) {
        int t = 0;
        for (int p = 0; p < W.players; p++) if (W.l[p].t > t) t = W.l[p].t;
        if (t >= PANEL_DELAY) game_over();
    }
}

/* the title: players join and leave (the kit's lobby), P1 starts (A, a start input, or a tap) */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) ha_play(HA_CONFIRM);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) {
        int t = st_t;
        new_run(ST_TITLE);                      /* the world follows the players (the herd on the title) */
        st_t = t;
    }
    int tap = touch_lifted(st_t);
    if ((ev & HU_TITLE_START) || tap || (opt_bot && st_t >= BOT_START)) {
        ha_play(HA_CONFIRM);
        new_run(ST_PLAY);
        touch_used = tap;
        start_press = 1;                        /* the run starts at once: the start press spits */
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
            if (touch_lifted(st_t - RETRY_LOCK)) back = 1;   /* a tap that began after the lock */
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

static void game_draw(void) { draw_frame(&W, st, st_t, run_best, new_best, paused, touch_used); }

static void game_init(void)
{
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; B, X, Y, Start, Up and Down too), up to 4 players: PRESS A TO SPIT */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, TITLE_SLOT_ROW};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_score = rs_option_int("score", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static int hornets_on(void)
{
    int n = 0;
    for (int k = 0; k < MAX_HORNETS; k++) n += W.hn[k].on && (W.hn[k].x >> 16) < STAGE_W;   /* on the stage */
    return n;
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[64] = "", hs[48] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(hs);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.l[p].score);
        snprintf(hs + b, sizeof hs - b, "%s%d", p ? "," : "", W.l[p].state);
    }
    const llama *l = &W.l[0];
    rs_log("state: st=%d players=%d score=%d score2=%d best=%u runs=%d hs=%s aim=%d spits=%d pops=%d heat=%d "
           "waves=%d sky=%d t=%d hn=%d paused=%d hash=%08x scores=%s",
           st, W.players, l->score, W.l[1].score, SV.best, runs_done, hs, l->aim, l->spits, l->pops, W.heat, W.waves,
           W.sky, W.t, hornets_on(), paused, state_hash, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press); S(touch_used);
    S(state_hash);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound); S(opt_score);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Llama Drama", "llamadrama", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
