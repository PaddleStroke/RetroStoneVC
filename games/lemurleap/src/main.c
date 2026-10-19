/*
 * Lemur Leap: the game flow (the title with its wardrobe, the climb, the results, pause), input (the pad, the touches,
 * the bot), sound, the music, save RAM (the record, the coins, the looks bought, the climb kept), options, save states.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Left / Right browse the
 * wardrobe (hats and furs: one bought is worn at once; X, Y or Select buys the one shown), A starts the climb. A climb
 * left from the pause is kept in save RAM: the title then says IN PROGRESS, A continues it and B starts a new one. The
 * lemur falls out of the screen: the camera follows the fall, then the results; one press goes back to the title.
 *
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; DESIGN.md "Controls"): the finger down the longest steers by the
 * screen's half it is on (held: the lemur speeds up that way); another finger going down shoots, and so does a quick
 * tap. Taps work the title (the look's line: its halves browse, the price buys, the prompt starts), the pause (B:
 * TITLE leaves, a tap elsewhere goes on) and the results.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=1          the bot climbs (it steers for the highest leaf it can reach, shoots the pests above)
 *   seed=N         a fixed climb (default: from the frame of the start);  ready=1  skip the title
 *   height=M       the climb starts at M metres (tests, screenshots);  fall=1  ... with nothing under the lemur
 *   god=1          the pests do no harm (tests);  coins=N, unlock=1, best=M   the save RAM's progress, in memory only
 *   fakerun=1      a climb kept at 312 m (in memory);  touch=1  the hint for touch screens;  look=K  the look shown
 *   dump=1         log the final state at exit;  music=0, sound=0
 */
#include "game.h"
#include "assets.h"
#include "rs.h"
#include "house_ui.h"
#include "house_tall.h"
#include "house_audio.h"
#include "game_text.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define ZOOM_H        480           /* a phone held upright: 480 lines zoomed to the display's height (docs/spec.md) */
#define START_INPUTS  (RS_BTN_START)
#define BOT_START     30
#define SNAP_EVERY    (30 * SEC)    /* the climb kept this often (and when left from the pause) */
#define MENU_LOCK     10

/* ---- the game's state (saved in states) ----------------------------------------------------------------------- */
static world W;
static int st, st_t, paused, look, new_best, earned, runs_done, snap_t, first_run, touch_used, shots_fired;
static int opt_bot, opt_god, opt_music, opt_sound, opt_seed_fixed, opt_touch, opt_height, opt_fall;
static uint32_t opt_seed;
static int music_cur = -1, coin_combo, coin_combo_t, prop_t, snd_gap[SFX_COUNT];
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM (house convention: checked, versioned, committed at each change; never part of a state) -------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best_m;                /* offset 8 (apps/launcher/games.cfg best=8): the record, metres */
    uint16_t best_coins;
    uint32_t coins;                 /* the purse */
    uint16_t owned;                 /* bits: the looks bought (no hat and the grey fur: always) */
    uint8_t hat, fur;               /* worn */
    uint32_t runs, total_m;
    uint8_t has_run, pad2[3];
    world run;                      /* the climb kept (has_run) */
    uint16_t sum;
} save_data;
static save_data SV;
/* what the title shows: the save RAM as it was at the title's entry (saved in states: a replay must not see newer) */
static save_data PV;
static const char MAGIC[5] = "LMLP";
#define OWNED_FREE ((1 << 0) | (1 << HATS))

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
    SV.owned |= OWNED_FREE;
    if (SV.hat >= HATS || !(SV.owned & (1 << SV.hat))) SV.hat = 0;
    if (SV.fur >= FURS || !(SV.owned & (1 << (HATS + SV.fur)))) SV.fur = 0;
}

static void save_store(void)
{
    if (opt_bot) return;                       /* the bot's climbs leave the save RAM as it was (tests) */
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* the progress changed in PV (the copy in the state: a replay sees the same) goes to the save RAM; the climb kept
 * there stays (run_save writes it) */
static void commit(void)
{
    uint8_t has = SV.has_run;
    world run = SV.run;
    SV = PV;
    SV.run = run;
    SV.has_run = has;
    save_store();
}

/* ---- sound -------------------------------------------------------------------------------------------------------- */
/* the sounds (the house synthesiser): 1.4 s of samples in all, with the house set within the 64 KiB guideline; the
 * mushroom, the cloud, the stomp and the shield play the spring, the puff and the pop pitched */
static void sound_init(void)
{
    ha_synth s;
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x1e3au);
    ha_tone(&s, 0, 330, 660, 60, 40, 25);                  /* a bounce: a springy blip up */
    ha_store(&s, SFX_BOUNCE, 64);
    ha_tone(&s, 0, 180, 900, 170, 54, 90);                 /* a spring: boing */
    ha_tone(&s, ha_ms(30), 360, 1400, 120, 22, 60);
    ha_store(&s, SFX_SPRING, 170);
    ha_hiss(&s, 0, 90, 34, 1);                             /* a dry leaf: a crack */
    ha_tone(&s, 0, 160, 90, 70, 28, 30);
    ha_store(&s, SFX_BREAK, 95);
    ha_hiss(&s, 0, 110, 20, 2);                            /* a puff bursting: a breath of seeds */
    ha_tone(&s, 0, 900, 1300, 70, 16, 40);
    ha_store(&s, SFX_PUFF, 110);
    ha_tone(&s, 0, 1568, 1568, 40, 30, 30);                /* a coin: a bright ping (pitched up by a run of coins) */
    ha_tone(&s, ha_ms(25), 2093, 2093, 60, 24, 40);
    ha_store(&s, SFX_COIN, 90);
    ha_tone(&s, 0, 1100, 700, 45, 26, 20);                 /* a seed shot: a pop */
    ha_hiss(&s, 0, 25, 8, 2);
    ha_store(&s, SFX_SHOT, 50);
    ha_tone(&s, 0, 400, 1200, 80, 40, 40);                 /* a pest popped: a bubbly pop */
    ha_hiss(&s, 0, 50, 16, 3);
    ha_store(&s, SFX_POP, 90);
    ha_tone(&s, 0, 700, 200, 170, 56, 90);                 /* hurt: a falling whistle */
    ha_store(&s, SFX_HURT, 175);
    ha_tone(&s, 0, 784, 784, 55, 40, 40);                  /* a power-up: a little fanfare */
    ha_tone(&s, ha_ms(50), 988, 988, 55, 40, 40);
    ha_tone(&s, ha_ms(95), 1318, 1318, 70, 44, 40);
    ha_store(&s, SFX_ITEM, 165);
    ha_tone(&s, 0, 880, 160, 250, 44, 150);                /* the fall out: a slide down */
    ha_store(&s, SFX_FALL, 255);
    ha_hiss(&s, 0, 50, 10, 3);                             /* the propeller's whirr */
    ha_tone(&s, 0, 140, 160, 50, 12, 0);
    ha_store(&s, SFX_PROP, 55);
    ha_tone(&s, 0, 659, 659, 70, 34, 50);                  /* a new zone: a chime up, E G C */
    ha_tone(&s, ha_ms(60), 784, 784, 70, 34, 50);
    ha_tone(&s, ha_ms(120), 1046, 1046, 100, 40, 60);
    ha_store(&s, SFX_ZONE, 225);
    ha_init(SFX_HOUSE);
    ha_sound_on(opt_sound);
}

/* the sample a sound plays, and its pitch (the aliases above) */
static void sfx(int id, int vol, int gap, int x)
{
    static const uint8_t slot[SFX_COUNT] = {SFX_BOUNCE, SFX_SPRING, SFX_SPRING, SFX_PUFF, SFX_BREAK, SFX_PUFF, SFX_COIN,
                                            SFX_SHOT, SFX_POP, SFX_POP, SFX_HURT, SFX_ITEM, SFX_PUFF, SFX_FALL, SFX_PROP,
                                            SFX_ZONE};
    static const uint16_t pitch[SFX_COUNT] = {RS_PITCH_1, RS_PITCH_1, RS_PITCH_1 * 3 / 4, RS_PITCH_1 * 2 / 3, RS_PITCH_1,
                                              RS_PITCH_1, RS_PITCH_1, RS_PITCH_1, RS_PITCH_1, RS_PITCH_1 * 3 / 4,
                                              RS_PITCH_1, RS_PITCH_1, RS_PITCH_1 * 3 / 2, RS_PITCH_1, RS_PITCH_1,
                                              RS_PITCH_1};
    if (snd_gap[id]) return;
    snd_gap[id] = gap;
    ha_play_pitched(slot[id], vol, 64 + x, pitch[id], 0);
}

static void music(int track)
{
    if (!opt_music) {
        if (music_cur >= 0) rs_music_stop();
        music_cur = -1;
        return;
    }
    if (track == music_cur && rs_music_playing()) return;
    size_t n;
    const void *m = rs_asset(track ? "music/stars.mod" : "music/climb.mod", &n);
    if (m && !rs_music_play(m, n, 1)) rs_music_volume(HA_MUSIC_VOL);
    music_cur = track;
}

/* ---- input ---------------------------------------------------------------------------------------------------------- */
static int pad_input(void)
{
    uint16_t p = rs_pad(0);
    int in = 0;
    if (p & RS_BTN_LEFT) in |= IN_LEFT;
    if (p & RS_BTN_RIGHT) in |= IN_RIGHT;
    if (p & (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y | RS_BTN_UP)) in |= IN_SHOOT;
    return in;
}

/* the touches: the finger down the longest steers by its half; another finger going down shoots; a quick tap too */
static int touch_input(void)
{
    const rs_touch_point *first = rs_touch_first();
    int in = 0;
    if (first && first->down) in |= rs_touch_half(first->x) == RS_TOUCH_LEFT ? IN_LEFT : IN_RIGHT;
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (!t) continue;
        if ((t->pressed && t != first) || t->tap) in |= IN_SHOOT;
    }
    if (in) touch_used = 1;
    return in;
}

/* a tap this frame on one of draw.c's hot zones: its index, HOT_MAX elsewhere, -1 none */
static int tapped(int n)
{
    int x, y;
    if (!rs_touch_tap(&x, &y)) return -1;
    for (int k = 0; k < n && k < HOT_MAX; k++)
        if (draw_hot[k].y1 > draw_hot[k].y0 && y >= draw_hot[k].y0 && y < draw_hot[k].y1 && x >= draw_hot[k].x0 &&
            x < draw_hot[k].x1)
            return k;
    return HOT_MAX;
}

/* ---- the flow ---------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }
static int can_continue(void) { return !opt_bot && PV.has_run && !PV.run.over; }

static void run_save(void)
{
    if (opt_bot || W.over || W.h.knocked || st != ST_PLAY) return;
    SV.run = W;
    SV.has_run = 1;
    save_store();
}

/* the lemur the title shows wears what was bought; a look shown but not bought is only tried on */
static void wear(int k)
{
    if (!(PV.owned & (1 << k))) return;
    if (k < HATS) PV.hat = (uint8_t)k;
    else PV.fur = (uint8_t)(k - HATS);
}

static void start_run(int resume)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : (opt_seed ^ rs_frame_count() * 2654435761u);
    if (resume) {
        W = PV.run;
    } else {
        world_init(&W, seed, PV.best_m);
        PV.has_run = SV.has_run = 0;
        first_run = PV.runs == 0;
        commit();
        if (opt_height > 0 || opt_fall) world_warp(&W, opt_height > 0 ? opt_height : 100, opt_fall);
    }
    W.god = (uint8_t)opt_god;
    paused = 0;
    snap_t = 0;
    coin_combo = coin_combo_t = prop_t = 0;
    draw_reset_run();
    go(ST_PLAY);
    rs_log("climb starts at frame %u: %s, seed %u", rs_frame_count(), resume ? "continued" : "new", W.seed);
}

static void back_to_title(void)
{
    PV = SV;
    paused = 0;
    go(ST_TITLE);
}

static void run_over(void)
{
    int m = world_height_m(&W);
    earned = W.coins;
    new_best = m > PV.best_m;
    if (new_best) PV.best_m = (uint16_t)(m > 65535 ? 65535 : m);
    if (W.coins > PV.best_coins) PV.best_coins = (uint16_t)(W.coins > 65535 ? 65535 : W.coins);
    PV.coins += (uint32_t)earned;
    PV.runs++;
    PV.total_m += (uint32_t)m;
    PV.has_run = SV.has_run = 0;
    commit();
    runs_done++;
    rs_log("climb over at frame %u: %d m, coins +%d, pests %d, bounces %d", rs_frame_count(), m, earned, W.kills,
           W.bounces);
    ha_play(new_best ? HA_MEDAL : HA_GAMEOVER);
    go(ST_OVER);
}

static void play_events(void)
{
    uint32_t ev = W.events;
    int x = W.h.x / FX;
    if (ev & EV_BOUNCE) sfx(SFX_BOUNCE, 50, 3, x);
    if (ev & EV_SPRING) sfx(SFX_SPRING, HA_VOL_ACTION, 0, x);
    if (ev & EV_SHROOM) sfx(SFX_SHROOM, HA_VOL_ACTION, 0, x);
    if (ev & EV_CLOUD) sfx(SFX_CLOUD, 60, 0, x);
    if (ev & EV_BREAK) sfx(SFX_BREAK, HA_VOL_FEEDBACK, 0, x);
    if (ev & EV_PUFF) sfx(SFX_PUFF, 60, 0, x);
    if (ev & EV_COIN) {
        int pitch = RS_PITCH_1 + coin_combo * 200;
        if (pitch > RS_PITCH_1 * 2) pitch = RS_PITCH_1 * 2;
        ha_play_pitched(SFX_COIN, 50, 64 + x, pitch, 1);
        coin_combo++;
        coin_combo_t = 40;
    }
    if (ev & EV_SHOT) { sfx(SFX_SHOT, 40, 4, x); shots_fired++; }
    if (ev & EV_POP) sfx(SFX_POP, HA_VOL_ACTION, 0, x);
    if (ev & EV_STOMP) sfx(SFX_STOMP, HA_VOL_ACTION, 0, x);
    if (ev & EV_HURT) sfx(SFX_HURT, HA_VOL_CRASH, 0, x);
    if (ev & EV_ITEM) sfx(SFX_ITEM, HA_VOL_ACTION, 0, x);
    if (ev & EV_SHIELD_POP) sfx(SFX_SHIELD, HA_VOL_FEEDBACK, 0, x);
    if (ev & EV_FALL) sfx(SFX_FALL, HA_VOL_ACTION, 0, x);
    if (ev & EV_ZONE) sfx(SFX_ZONE, HA_VOL_FEEDBACK, 0, x);
    if (ev & EV_BEST) ha_play(HA_MEDAL);
    if (W.h.fly && ++prop_t >= 9) { prop_t = 0; sfx(SFX_PROP, 34, 0, x); }
    if (ev & EV_ZONE) rs_log("zone %d at %d m (frame %u)", W.zone, world_height_m(&W), rs_frame_count());
    if (ev & EV_HURT) rs_log("knocked by a pest at %d m", world_height_m(&W));
    if (ev & EV_FALL) rs_log("fell out at %d m (frame %u)", world_height_m(&W), rs_frame_count());
}

static void play_update(void)
{
    if (W.over && W.fall_t >= FALL_FRAMES) { run_over(); return; }
    int in = opt_bot ? bot_input(&W) : pad_input() | touch_input();
    world_step(&W, in);
    draw_events(&W);
    play_events();
    state_hash = (state_hash ^ world_hash(&W)) * 16777619u;
    if (++snap_t >= SNAP_EVERY) { snap_t = 0; run_save(); }
}

static void buy(void)
{
    int cost = LOOK_COST[look];
    if (PV.owned & (1 << look)) { ha_play(HA_SELECT); return; }
    if (PV.coins >= (uint32_t)cost) {
        PV.coins -= (uint32_t)cost;
        PV.owned |= (uint16_t)(1 << look);
        wear(look);
        commit();
        ha_play(HA_MEDAL);
        rs_log("look %d bought for %d coins", look, cost);
    } else {
        ha_play(HA_PAUSE);
    }
}

static void title_update(void)
{
    int cont = can_continue();
    hu_title_cfg tc = {START_INPUTS, 1, cont ? TR(VERB_CONTINUE) : TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    int ev = hu_title_update();
    uint16_t pr = rs_pad_pressed(0);
    int k = tapped(HOT_MAX);
    if ((pr & RS_BTN_LEFT) || k == HOT_PREV) { look = (look + LOOKS - 1) % LOOKS; wear(look); ha_play(HA_SELECT); }
    if ((pr & RS_BTN_RIGHT) || k == HOT_NEXT) { look = (look + 1) % LOOKS; wear(look); ha_play(HA_SELECT); }
    if ((pr & (RS_BTN_X | RS_BTN_Y | RS_BTN_SELECT)) || k == HOT_BUY) buy();
    int start = (ev & HU_TITLE_START) || k == HOT_PROMPT || k == HOT_MAX || (opt_bot && st_t >= BOT_START);   /* (a tap elsewhere: start) */
    if (cont && ((pr & RS_BTN_B) || k == HOT_NEWRUN)) {    /* B: a new climb (the one kept is dropped) */
        cont = 0;
        start = 1;
    }
    if (!start) return;
    ha_play(HA_CONFIRM);
    commit();                                           /* (the looks worn) */
    start_run(cont);
}

static void game_update(void)
{
    for (int i = 0; i < SFX_COUNT; i++) if (snd_gap[i]) snd_gap[i]--;
    if (coin_combo_t && !--coin_combo_t) coin_combo = 0;
    music(st == ST_PLAY && world_height_m(&W) >= ZONE_SKY ? 1 : 0);
    int tap = paused ? tapped(HOT_MAX) : -1;            /* paused, a tap: on the quit line, the title; elsewhere, go on */
    if (st == ST_PLAY && ((rs_pad_pressed(0) & (RS_BTN_SELECT | RS_BTN_START)) || (tap >= 0 && tap != HOT_QUIT))) {
        paused = !paused;
        ha_play(HA_PAUSE);
    } else if (st == ST_PLAY && paused && ((rs_pad_pressed(0) & RS_BTN_B) || tap == HOT_QUIT)) {
        ha_play(HA_SELECT);
        run_save();                                     /* the climb kept: the title offers to continue it */
        back_to_title();
        return;
    }
    if (paused) return;
    hu_shake_step();
    switch (st) {
    case ST_TITLE: title_update(); break;
    case ST_PLAY: play_update(); break;
    case ST_OVER: {
        int back = st_t >= RETRY_LOCK && (hu_over_back() || tapped(HOT_MAX) >= 0);
        if (opt_bot && st_t == RETRY_LOCK + 40) back = 1;
        if (back) {
            ha_play(HA_SELECT);
            back_to_title();
            if (opt_bot) opt_bot = 0;                  /* the bot's climb done: it stops on the title */
        }
        break;
    }
    }
    st_t++;
}

static void game_draw(void)
{
    view v;
    memset(&v, 0, sizeof v);
    v.state = st;
    v.st_t = st_t;
    v.paused = paused;
    v.look = look;
    v.hat = PV.hat;
    v.fur = st == ST_TITLE && look >= HATS ? look - HATS : PV.fur;
    v.owned = PV.owned;
    v.coins = (int)PV.coins;
    v.best_m = PV.best_m;
    v.best_coins = PV.best_coins;
    v.runs = (int)PV.runs;
    v.can_continue = can_continue();
    v.run_m = world_height_m(&PV.run);
    v.new_best = new_best;
    v.earned = earned;
    v.touch = opt_touch || touch_used;
    v.first_run = first_run;
    draw_frame(&W, &v);
}

static void game_init(void)
{
    rs_screen_zoom(ZOOM_H);                     /* a phone held upright: zoomed, the climb above the stage */
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    opt_bot = rs_option_int("bot", 0);
    opt_god = rs_option_int("god", 0);
    opt_touch = rs_option_int("touch", 0);
    opt_height = rs_option_int("height", 0);
    opt_fall = rs_option_int("fall", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    if (rs_option("coins")) SV.coins = (uint32_t)rs_option_int("coins", 0);
    if (rs_option_int("unlock", 0)) SV.owned = 0xffff;
    if (rs_option("best")) SV.best_m = (uint16_t)rs_option_int("best", 0);
    if (rs_option_int("fakerun", 0)) {          /* (tests, screenshots) a climb kept, in memory only */
        world_init(&SV.run, 1234, SV.best_m);
        world_warp(&SV.run, 312, 0);
        SV.run.coins = 27;
        SV.has_run = 1;
    }
    sound_init();
    draw_init();
    hu_title_cfg tc = {START_INPUTS, 1, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    hu_players_set(1);
    PV = SV;
    look = PV.hat;
    if (rs_option("look")) look = rs_option_int("look", 0) % LOOKS;
    world_init(&W, 1, PV.best_m);
    go(ST_TITLE);
    if (rs_option_int("ready", 0)) start_run(0);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    long vram = (long)rs_tiles_used() * 32;
    for (int l = 0; l < RS_BG_COUNT; l++)
        vram += (long)(rs_screen_width() > 320 && rs_bg_map_w(l) > 64 ? 64 : rs_bg_map_w(l)) * rs_bg_map_h(l) * 2;
    rs_log("vram %ld bytes (tiles %d)", vram, rs_tiles_used());
    rs_log("state: st=%d m=%d coins=%d kills=%d bounces=%d shots=%d t=%d x=%d over=%d knocked=%d paused=%d look=%d hat=%d "
           "fur=%d purse=%u best=%d owned=%x runs=%d kept=%d hash=%08x",
           st, world_height_m(&W), W.coins, W.kills, W.bounces, shots_fired, W.t, W.h.x / FX, W.over, W.h.knocked, paused,
           look, SV.hat, SV.fur, SV.coins, SV.best_m, SV.owned, runs_done, SV.has_run, state_hash);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(look); S(new_best); S(earned); S(runs_done); S(snap_t); S(first_run);
    S(touch_used); S(shots_fired); S(opt_bot); S(opt_god); S(opt_music); S(opt_sound); S(opt_seed_fixed); S(opt_touch); S(opt_height);
    S(opt_fall); S(opt_seed); S(music_cur); S(coin_combo); S(coin_combo_t); S(prop_t); S(snd_gap); S(state_hash); S(PV);
    draw_state();
    hu_state();
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* widescreen: any width (wider panels); touch_native: the halves steer, a second finger shoots, the menus' taps */
    static const rs_game g = {"Lemur Leap", "lemurleap", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, ll_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
