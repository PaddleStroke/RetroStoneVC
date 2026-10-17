/*
 * Cosmo Corgi: the game flow (the title with players 2-4 joining, play, game over, pause), input (pads and touch),
 * sound, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A
 * (B leaves); P1 starts the run at once with A (or an extra START INPUT); one press on the game-over panel
 * (hu_over_back) goes BACK TO THE TITLE, with the same players.
 * The pads: the d-pad flies the rocket in 8 directions; it fires by itself (the owner's rule: no fire button).
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Controls"): a relative drag anywhere on the
 * screen: the rocket moves by the finger's movement (1:1, at most TOUCH_MAX px a frame), so the finger never hides it;
 * a tap (a finger lifted) starts from the title and leaves the game-over panel. The finger's last point is kept in a
 * registered state (touch_*): a load forgets the fingers, and one still down starts a new drag.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_decide); botruns=N runs;
 *                  botstop=S: once its score reaches S (P2-P4: S+100, S+200..) it flies into the robots (the tests'
 *                  game over)
 *   botskill=E     a human-like bot (1..10: worse and worse), to tune the medals
 *   seed=N         a fixed run (default: from the frame the title was shown)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   section=N      start at section N (0..3: the nebula, the asteroids, the station, the comets); loop=L (0, 1..)
 *   boss=1         start at the section's boss (the screenshots, tests)
 *   power=K        (tests, screenshots) every corgi starts with weapon K (1 spread, 2 beam, 3 homing) at level 3,
 *                  two pods, a shield and one speed boost
 *   msg=K          (the language test) the run shows message K: 1-6 the power-ups, 7 MAX POWER!; 8 a boss popped,
 *                  9 a boss flew away, 10 BIG ROBOT!
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

/* the house buttons: A (B, X, Y and the d-pad too) starts; Select (or Start in a run) pauses */
#define START_INPUTS  (RS_BTN_B | RS_BTN_X | RS_BTN_Y | RS_BTN_START | HU_IN_DPAD)
#define ANY_BUTTON    (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y | HU_IN_DPAD)

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound, opt_section,
    opt_loop, opt_boss, opt_power;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static int start_press;                       /* P1's start press on the title: the run starts at once */
static int touch_on, touch_x0, touch_y0, touch_lx, touch_ly;   /* P1's dragging finger: where it went down, was last */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) -------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t best_stage, pad2;
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "COSM";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x4343;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0xc0561e5u);
    /* the lengths keep the sample memory under the 64 KiB guideline with the house set */
    ha_tone(&s, 0, 1700, 1050, 40, 16, 22);                 /* the bark shot: a soft "pew" */
    ha_store(&s, SFX_SHOT, 45);
    ha_tone(&s, 0, 980, 320, 50, 50, 30);                   /* a robot pops: a bubble pop, a sparkle, a bolt's clink */
    ha_tone(&s, ha_ms(35), 2637, 3520, 70, 16, 40);
    ha_tone(&s, ha_ms(20), 3200, 3100, 30, 12, 20);
    ha_hiss(&s, 0, 40, 18, 2);
    ha_store(&s, SFX_POP, 115);
    ha_tone(&s, 0, 560, 170, 230, 54, 120);                 /* a corgi hit: a "bonk", a wobble, dust */
    ha_tone(&s, ha_ms(60), 330, 440, 160, 18, 80);
    ha_hiss(&s, 0, 140, 36, 2);
    ha_store(&s, SFX_HIT, 230);
    ha_tone(&s, 0, 880, 1320, 35, 14, 20);                  /* a robot fires: a tiny plip */
    ha_store(&s, SFX_ESHOT, 40);
    static const int pw[3] = {1319, 1568, 2093};            /* a capsule: E6 G6 C7 */
    for (int i = 0; i < 3; i++) ha_tone(&s, ha_ms(i * 50), pw[i], pw[i], 90, 32, 60);
    ha_store(&s, SFX_CAPSULE, 170);
    ha_tone(&s, 0, 420, 1250, 80, 40, 40);                  /* the shield pops: "blup" */
    ha_hiss(&s, ha_ms(20), 40, 20, 1);
    ha_store(&s, SFX_SHIELD, 110);
    ha_tone(&s, 0, 310, 250, 60, 34, 30);                   /* the boss hit: a clonk */
    ha_tone(&s, 0, 1250, 1100, 30, 12, 15);
    ha_store(&s, SFX_BOSS_HIT, 65);
    ha_hiss(&s, 0, 280, 66, 1);                             /* the boss pops: a big puff, a fall, sparkles */
    ha_tone(&s, 0, 420, 80, 280, 50, 140);
    ha_tone(&s, ha_ms(120), 2093, 3136, 160, 16, 80);
    ha_store(&s, SFX_BOSS_DOWN, 260);
    for (int i = 0; i < 4; i++)                             /* BIG ROBOT!: a cute two-tone alarm */
        ha_tone(&s, ha_ms(i * 70), i % 2 ? 880 : 660, i % 2 ? 880 : 660, 60, 26, 30);
    ha_store(&s, SFX_WARN, 300);
    ha_tone(&s, 0, 260, 1500, 150, 28, 40);                 /* a dasher winds up and zips */
    ha_store(&s, SFX_DASH, 160);
    ha_tone(&s, 0, 540, 360, 60, 48, 30);                   /* the corgi barks: "arf!" */
    ha_hiss(&s, 0, 40, 22, 2);
    ha_tone(&s, ha_ms(85), 620, 420, 50, 30, 25);
    ha_store(&s, SFX_BARK, 150);
    ha_hiss(&s, 0, 50, 10, 2);                              /* a bone whooshes off */
    ha_tone(&s, 0, 700, 940, 45, 8, 20);
    ha_store(&s, SFX_BONE, 55);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx_at(int id, int x, int vol)
{
    ha_play_slot(id, vol, x, id == SFX_CAPSULE || id == SFX_BOSS_DOWN);
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    if (opt_section || opt_loop) world_start_section(&W, opt_section, opt_loop);
    if (opt_boss) world_to_boss(&W);
    if (opt_power >= 1 && opt_power <= 3)
        for (int p = 0; p < W.players; p++) {
            W.h[p].weapon = opt_power;
            W.h[p].wlevel = WEAPON_MAX;
            W.h[p].pods = PODS_MAX;
            W.h[p].shield = 1;
            W.h[p].speed = 1;
        }
    int msg = rs_option_int("msg", 0);          /* the language test: a message on screen */
    if (msg >= MSG_SPREAD && msg <= MSG_MAX) { W.msg = msg; W.msg_t = 1 << 20; }
    if (msg == 8 || msg == 9) { W.phase = PH_CLEAR; W.boss_away = msg == 9; }
    if (msg == 10) W.phase = PH_WARN;
    new_best = 0;
    touch_on = 0;
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

/* the drag: the movement of P1's finger since the last frame (a new finger, or one seen again after a load, starts
 * a new drag: no movement on its first frame) */
static void touch_drag(pin *in)
{
    const rs_touch_point *t = rs_touch_first();
    if (!t || !t->down) {
        touch_on = 0;
        return;
    }
    in->touch = 1;
    if (!touch_on || t->pressed || t->x0 != touch_x0 || t->y0 != touch_y0) {
        touch_on = 1;
        touch_x0 = t->x0;
        touch_y0 = t->y0;
    } else {
        in->tdx = t->x - touch_lx;
        in->tdy = t->y - touch_ly;
    }
    touch_lx = t->x;
    touch_ly = t->y;
}

/* the bot that stops dodging (botstop): it flies into the nearest robot or shot */
static pin bot_give_up(int p)
{
    pin in = {0, 0, 0, 0, 0, 1};
    const hero *h = &W.h[p];
    int32_t bx = 0, by = 0;
    int bd = 1 << 30;
    for (int i = 0; i < MAX_EBULLETS; i++)
        if (W.eb[i].on) {
            int d = ((W.eb[i].x - h->x) >> 16) * ((W.eb[i].x - h->x) >> 16) + ((W.eb[i].y - h->y) >> 16) * ((W.eb[i].y - h->y) >> 16);
            if (d < bd) { bd = d; bx = W.eb[i].x; by = W.eb[i].y; }
        }
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (W.en[i].on && W.en[i].t >= 0 && (W.en[i].x >> 16) < 300) {
            int d = ((W.en[i].x - h->x) >> 16) * ((W.en[i].x - h->x) >> 16) + ((W.en[i].y - h->y) >> 16) * ((W.en[i].y - h->y) >> 16);
            if (d < bd) { bd = d; bx = W.en[i].x; by = W.en[i].y; }
        }
    if (W.b.on && W.b.phase == BP_FIGHT) { bx = W.b.x; by = W.b.y; bd = 0; }
    if (bd == 1 << 30) return in;
    in.dx = bx > h->x + Q16_ONE ? 1 : bx < h->x - Q16_ONE ? -1 : 0;
    in.dy = by > h->y + Q16_ONE ? 1 : by < h->y - Q16_ONE ? -1 : 0;
    return in;
}

static pin player_input(int p)
{
    pin in = {0, 0, 0, 0, 0, 0};
    if (p == 0 && start_press) in.any = 1;
    if (is_bot(p)) {
        if (opt_botstop && W.h[p].score >= opt_botstop + 100 * p && st == ST_PLAY && W.started) return bot_give_up(p);
        pin b = bot_decide(&W, p, opt_botskill);
        b.any |= in.any;
        return b;
    }
    uint16_t pad = rs_pad(hu_player_pad(p));
    in.dx = (pad & RS_BTN_RIGHT ? 1 : 0) - (pad & RS_BTN_LEFT ? 1 : 0);
    in.dy = (pad & RS_BTN_DOWN ? 1 : 0) - (pad & RS_BTN_UP ? 1 : 0);
    if (pad & ANY_BUTTON) in.any = 1;
    if (p == 0) touch_drag(&in);
    return in;
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
    if (best_now > run_best) { run_best = best_now; new_best = 1; }
    if ((uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
    if (world_stage(&W) > SV.best_stage) SV.best_stage = (uint16_t)world_stage(&W);
    if (W.players >= 2 && (uint32_t)best_now > SV.best_multi) SV.best_multi = (uint32_t)best_now;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d stage %d (best %u)", runs_done, rs_frame_count(), W.h[0].score,
           world_stage(&W), SV.best);
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
        const hero *h = &W.h[p];
        int x = h->x >> 16;
        if ((ev & EV_FIRE) && h->shots % 2 == 0) sfx_at(SFX_SHOT, x, 22);
        if (ev & EV_BONE) sfx_at(SFX_BONE, x, 30);
        if (ev & EV_POP) sfx_at(SFX_POP, x, HA_VOL_FEEDBACK);
        if (ev & EV_CAPSULE) sfx_at(SFX_CAPSULE, x, HA_VOL_FEEDBACK);
        if (ev & EV_GROUP) ha_play(HA_DING);
        if (ev & EV_SHIELD) sfx_at(SFX_SHIELD, x, HA_VOL_ACTION);
        if ((ev & EV_BOSS_HIT) && W.t % 4 == 0) sfx_at(SFX_BOSS_HIT, x, 50);
        if (ev & EV_START) sfx_at(SFX_BARK, x, HA_VOL_ACTION);
        if (ev & EV_BOSS_DOWN) {
            sfx_at(SFX_BOSS_DOWN, x, HA_VOL_CRASH);
            sfx_at(SFX_BARK, x, HA_VOL_ACTION);
            hu_shake(2, 12);
            rs_log("player %d popped the boss at frame %u (stage %d)", p + 1, rs_frame_count(), world_stage(&W));
        }
        if (ev & EV_HIT) {
            sfx_at(SFX_HIT, x, HA_VOL_CRASH);
            hu_shake(3, 12);                    /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d hit at frame %u (stage %d, score %d)", p + 1, rs_frame_count(), world_stage(&W), h->score);
        }
        state_hash = (state_hash ^ (uint32_t)h->x) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)h->y) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(h->score * 131 + h->state * 7 + h->shots)) * 16777619u;
    }
    int wev = W.wevents;
    if (wev & (WEV_ESHOT | WEV_BOSS_FIRE)) sfx_at(SFX_ESHOT, 240, 36);
    if (wev & WEV_DASH) sfx_at(SFX_DASH, 260, 44);
    if (wev & WEV_WARN) {
        sfx_at(SFX_WARN, 160, HA_VOL_UI);
        rs_log("boss warning at frame %u (stage %d)", rs_frame_count(), world_stage(&W));
    }
    if (wev & WEV_SECTION) {
        ha_play(HA_DING);
        rs_log("section %d (loop %d) at frame %u, score %d", W.sec, W.loop + 1, rs_frame_count(), W.h[0].score);
    }
    if (wev & WEV_BOSS_AWAY) rs_log("the boss flew away at frame %u (stage %d)", rs_frame_count(), world_stage(&W));
    state_hash = (state_hash ^ (uint32_t)(W.scroll + W.phase * 977 + W.sec * 31)) * 16777619u;
    if (!W.started) return;                     /* ready=1: the run waits for its first press */
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && world_all_out(&W)) {
        int t = 0;
        for (int p = 0; p < W.players; p++) if (W.h[p].t > t) t = W.h[p].t;
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
        new_run(ST_TITLE);                      /* the world follows the players */
        st_t = t;
    }
    if ((ev & HU_TITLE_START) || touch_lifted(st_t) || (opt_bot && st_t >= BOT_START)) {
        ha_play(HA_CONFIRM);
        new_run(ST_PLAY);
        start_press = 1;                        /* the run starts at once: the corgis take off */
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
    if (paused) {
        pin none = {0, 0, 0, 0, 0, 0};
        touch_drag(&none);                      /* the finger moves on: no jump when the game resumes */
        return;
    }
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

static void game_draw(void) { draw_frame(&W, st, st_t, run_best, new_best, paused); }

static void game_init(void)
{
    rs_screen_tall(RS_SCREEN_H_MAX);          /* a phone held upright: more space above and below (draw.c, DESIGN.md) */
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; B, X, Y, Start and the d-pad too), up to 4 players: PRESS A TO FLY */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_section = rs_option_int("section", 0);
    opt_loop = rs_option_int("loop", 0);
    opt_boss = rs_option_int("boss", 0);
    opt_power = rs_option_int("power", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[64] = "", hs[48] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(hs);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.h[p].score);
        snprintf(hs + b, sizeof hs - b, "%s%d", p ? "," : "", W.h[p].state);
    }
    const hero *h = &W.h[0];
    rs_log("state: st=%d players=%d score=%d score2=%d best=%u runs=%d hs=%s stage=%d phase=%d x=%d y=%d shots=%d "
           "weapon=%d level=%d pods=%d speed=%d shield=%d caps=%d pops=%d paused=%d hash=%08x scores=%s",
           st, W.players, h->score, W.h[1].score, SV.best, runs_done, hs, world_stage(&W), W.phase, (int)(h->x >> 16),
           (int)(h->y >> 16), h->shots, h->weapon, h->wlevel, h->pods, h->speed, h->shield, h->caps, h->pops, paused,
           state_hash, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press); S(state_hash);
    S(touch_on); S(touch_x0); S(touch_y0); S(touch_lx); S(touch_ly);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound); S(opt_section); S(opt_loop); S(opt_boss); S(opt_power);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Cosmo Corgi", "cosmocorgi", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
