/*
 * Ladybug Laps: the game flow (the title with players 2-4 joining, the races, the results between them, game over,
 * pause), input (pads and touch), sound, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu (the four bots race behind
 * it). Pads 2-4 join with A (B leaves); P1 starts at once with A (or an extra START INPUT: B, Start, the d-pad; a tap):
 * 1 player: the championship, 10 races (the 5 tracks, then the 5 driven the other way), a top-3 place goes on; 2-4
 * players: a cup of 3 races. After the last race (or a 4th place in 1 player) the game-over panel; one press
 * (hu_over_back) goes BACK TO THE TITLE, with the same players.
 * The pads: Left / Right rotate the ladybug (its nose to its left / right), A (or Up) is the gas, B the brake (and
 * reverse).
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Controls"): a virtual stick (a finger down
 * anywhere on the left part, dragged: the ladybug turns toward it and accelerates while the finger is down), a round
 * BRAKE button at the bottom right; a tap starts from the title, goes on from the results and leaves the game-over panel.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_think: the player bot); botruns=N runs; botskill=E a human-like
 *                  bot (1..10, to tune the medals); botout=1: the player bot does not drive (it finishes last: game over)
 *   seed=N         fixed grids and bots (default: from the frame of the start)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title
 *   race=N         start the run at race N (0..);  track=T  the first race on track T (0..4)
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

/* the house buttons: A (B, Start and the d-pad too) starts; Select (or Start in a race) pauses */
#define START_INPUTS  (RS_BTN_B | RS_BTN_START | HU_IN_DPAD)

static int opt_bot, opt_botruns, opt_botskill, opt_botout, opt_seed_fixed, opt_music, opt_sound, opt_race, opt_track;
static uint32_t opt_seed;
static world W;
static run_info RI;
static int st, st_t, paused, new_best, runs_done, bg_track, cup_first;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static touch_view TV;                         /* the fingers: the stick, the button (drawn by draw.c) */
static int touch_stick_slot = -1;             /* the finger slot of the stick (-1: none) */
static uint8_t touch_btn_slot[RS_TOUCH_MAX];  /* a finger down on the BRAKE button */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best championship (house convention: checked, versioned, committed at each game over) ------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best, best_multi;
    uint32_t runs, champions;
    uint16_t medals[4];
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "LADY";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x4c41;
    for (size_t i = 0; i < offsetof(save_data, sum); i++) sum = (uint16_t)(sum * 31 + p[i]);
    return sum;
}

static void save_load(void)
{
    memcpy(&SV, rs_sram(), sizeof SV);
    /* version 2: the scoring of the races changed (3, 2, 1, 0 a race): the bests of version 1 do not compare */
    if (memcmp(SV.magic, MAGIC, 4) || SV.version != 2 || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, MAGIC, 4);
        SV.version = 2;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x1adb06u);
    ha_tone(&s, 0, 300, 180, 60, 40, 30);                   /* two ladybugs bump: a soft "bonk" */
    ha_hiss(&s, 0, 30, 18, 2);
    ha_store(&s, SFX_BUMP, 80);
    ha_tone(&s, 0, 200, 110, 90, 56, 40);                   /* into a cup or a wall: a "thock" */
    ha_hiss(&s, 0, 50, 26, 1);
    ha_store(&s, SFX_WALL, 90);
    ha_tone(&s, 0, 420, 980, 140, 34, 90);                  /* a hop: "boing" up, the wings buzzing */
    ha_tone(&s, ha_ms(20), 1400, 1500, 110, 10, 60);
    ha_store(&s, SFX_HOP, 110);
    ha_tone(&s, 0, 260, 160, 50, 30, 25);                   /* landing: a little "pat" */
    ha_store(&s, SFX_LAND, 60);
    ha_tone(&s, 0, 900, 160, 300, 42, 200);                 /* off the table: a falling "wheee" */
    ha_store(&s, SFX_FALL, 200);
    ha_tone(&s, 0, 520, 300, 120, 40, 70);                  /* left behind: a sad "boop" down */
    ha_tone(&s, ha_ms(90), 300, 200, 110, 34, 70);
    ha_store(&s, SFX_DROP, 160);
    ha_tone(&s, 0, 1568, 1568, 60, 36, 40);                 /* the leader takes a point: a bright "ting-ting" */
    ha_tone(&s, ha_ms(60), 2093, 2093, 90, 34, 60);
    ha_store(&s, SFX_POINT, 120);
    ha_tone(&s, 0, 880, 880, 120, 44, 0);                   /* the start lights: a beep */
    ha_store(&s, SFX_BEEP, 100);
    ha_tone(&s, 0, 1760, 1760, 200, 46, 140);               /* GO: a higher, longer beep */
    ha_store(&s, SFX_GO, 150);
    ha_hiss(&s, 0, 160, 40, 1);                             /* a puddle: a splash */
    ha_tone(&s, ha_ms(10), 1200, 600, 80, 16, 40);
    ha_store(&s, SFX_SPLASH, 100);
    ha_tone(&s, 0, 140, 90, 160, 40, 90);                   /* jam: a sticky "squelch" */
    ha_hiss(&s, 0, 120, 16, 4);
    ha_store(&s, SFX_JAM, 100);
    ha_tone(&s, 0, 1047, 1047, 70, 34, 50);                 /* a lap: two notes up */
    ha_tone(&s, ha_ms(80), 1568, 1568, 120, 34, 80);
    ha_store(&s, SFX_LAP, 140);
    for (int i = 0; i < 4; i++)                             /* the finish: a little fanfare, G C E G */
        ha_tone(&s, ha_ms(i * 60), (int[]){784, 1047, 1319, 1568}[i], (int[]){784, 1047, 1319, 1568}[i], i == 3 ? 120 : 70,
                38, i == 3 ? 100 : 50);
    ha_store(&s, SFX_FINISH, 250);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

/* a racer's sound, panned by where it is on the screen; not when it is far off it */
static void sfx_car(int id, int i, int vol)
{
    int x = (W.c[i].x - W.camx) / FX + ZONE_HW, y = (W.c[i].y - W.camy) / FX + ZONE_HH;
    if (x < -120 || x > RS_SCREEN_W + 120 || y < -60 || y > RS_SCREEN_H + 60) return;
    if (x < 0) x = 0;
    if (x > RS_SCREEN_W - 1) x = RS_SCREEN_W - 1;
    ha_play_slot(id, vol, x, id == SFX_SPLASH);
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void touch_reset(void)
{
    touch_stick_slot = -1;
    memset(touch_btn_slot, 0, sizeof touch_btn_slot);
    TV.stick_on = TV.brake_on = 0;
}

static uint32_t new_seed(int k)
{
    return opt_seed_fixed ? opt_seed + (uint32_t)k * 7919u : (opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u) + (uint32_t)k;
}

/* the title's race: four bots on one track after another */
static void title_race(void)
{
    world_init(&W, bg_track, 0, CUP_LEVEL, 0, new_seed(bg_track));
    draw_new_race(&W);
}

static void start_race(int k)
{
    int n = hu_players(), track, rev, level;
    RI.race = k;
    if (n == 1) {
        track = champ_track(k);
        rev = champ_reverse(k);
        level = k;
    } else {
        track = (cup_first + k) % TRACKS;
        rev = 0;
        level = CUP_LEVEL;
    }
    if (opt_track >= 0 && k == opt_race) track = opt_track % TRACKS;
    world_init(&W, track, rev, level, n, new_seed(k + 1));
    W.skill = opt_botskill;
    touch_reset();
    draw_new_race(&W);
    go(ST_PLAY);
}

static void new_run(void)
{
    memset(&RI, 0, sizeof RI);
    RI.races = hu_players() == 1 ? CHAMP_RACES : CUP_RACES;
    cup_first = (int)(new_seed(0) % TRACKS);
    new_best = 0;
    start_race(opt_race >= 0 && opt_race < RI.races ? opt_race : 0);   /* race=-1: not tracks[-1] */
}

static int is_bot(int p) { return p < opt_bot; }

/* ---- touch (player 1) ------------------------------------------------------------------------------------------- */
/* A finger was lifted this frame that went down during the last `since` frames (on this screen, after its lock):
 * the touch screen's "press A" (a tap; a finger still down from the race before never counts). */
static int touch_lifted(int since)
{
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (t && t->released && t->frames < since) return 1;
    }
    return 0;
}

/* the fingers of player 1: the stick (the direction; the gas while it is down), the BRAKE button */
static void touch_read(pin *in)
{
    tbutton b;
    touch_brake_button(rs_screen_left(), rs_screen_width(), &b);
    b.y += rs_screen_bottom() - RS_SCREEN_H;            /* a tall screen: BRAKE at its bottom */
    int ox = rs_screen_origin_x(), oy = rs_screen_origin_y();   /* screen pixels -> stage coordinates */
    TV.brake_on = 0;
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (!t || !t->down) {
            touch_btn_slot[i] = 0;
            if (touch_stick_slot == i) touch_stick_slot = -1;
            continue;
        }
        TV.seen = 1;
        if (t->pressed || (!touch_btn_slot[i] && touch_stick_slot != i)) {
            int x0 = t->x0 - ox, y0 = t->y0 - oy;
            if (touch_on_button(&b, x0, y0)) touch_btn_slot[i] = 1;
            else if (touch_stick_slot < 0 && touch_stick_zone(rs_screen_left(), rs_screen_width(), x0, y0)) {
                touch_stick_slot = i;
                TV.sx0 = t->x0;
                TV.sy0 = t->y0;
            }
        }
        if (touch_btn_slot[i]) TV.brake_on = 1;
        if (touch_stick_slot == i) {
            TV.sx = t->x;
            TV.sy = t->y;
        }
    }
    TV.stick_on = touch_stick_slot >= 0;
    if (TV.stick_on) {
        int16_t dx, dy;
        touch_stick(TV.sx0, TV.sy0, TV.sx, TV.sy, &dx, &dy);
        if (!in->dx && !in->dy) {
            in->dx = dx;
            in->dy = dy;
        }
        in->gas = 1;
        in->any = 1;
    }
    if (TV.brake_on) {
        in->brake = 1;
        in->gas = 0;
        in->any = 1;
    }
}

static pin player_input(int p)
{
    pin in = {0, 0, 0, 0, 0, 0};
    if (is_bot(p)) {
        if (opt_botout && p == 0) return in;
        bot_think(&W, p, &in);
        return in;
    }
    int pad = hu_player_pad(p);
    uint16_t held = rs_pad(pad);
    in.turn = (int8_t)((held & RS_BTN_RIGHT ? 1 : 0) - (held & RS_BTN_LEFT ? 1 : 0));   /* rotate */
    in.gas = (held & (RS_BTN_A | RS_BTN_UP)) != 0;
    in.brake = (held & RS_BTN_B) != 0;
    if (held & (RS_BTN_A | RS_BTN_B | HU_IN_DPAD)) in.any = 1;
    if (p == 0) touch_read(&in);
    return in;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int players = hu_players(), best_now = 0;
    for (int p = 0; p < players; p++)
        if (RI.score[p] > best_now) best_now = RI.score[p];
    SV.runs++;
    int m = players == 1 ? hu_medal_of(RI.score[0], medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (RI.champion) SV.champions++;
    if (players == 1 && best_now > run_best) { run_best = best_now; new_best = 1; }
    if (players == 1 && (uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
    if (players >= 2 && (uint32_t)best_now > SV.best_multi) SV.best_multi = (uint32_t)best_now;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d, race %d of %d, champion %d (best %u)", runs_done, rs_frame_count(),
           RI.score[0], RI.race + 1, RI.races, RI.champion, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void hash_world(void)
{
    for (int i = 0; i < RACERS; i++) {
        const car *c = &W.c[i];
        state_hash = (state_hash ^ (uint32_t)c->x) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)c->y) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(c->ang + c->points * 1024 + c->lap * 65536 + c->state * 7)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)(W.t + W.leader * 7 + W.phase * 131)) * 16777619u;
}

static void race_sounds(void)
{
    for (int i = 0; i < RACERS; i++) {
        int ev = W.events[i];
        if (!ev) continue;
        if (ev & EV_BUMP) sfx_car(SFX_BUMP, i, HA_VOL_FEEDBACK);
        if (ev & EV_WALL) sfx_car(SFX_WALL, i, HA_VOL_ACTION);
        if (ev & EV_HOP) sfx_car(SFX_HOP, i, HA_VOL_FEEDBACK);
        if (ev & EV_LAND) sfx_car(SFX_LAND, i, 50);
        if (ev & EV_FALL) sfx_car(SFX_FALL, i, HA_VOL_ACTION);
        if (ev & EV_SPLASH) sfx_car(SFX_SPLASH, i, HA_VOL_FEEDBACK);
        if (ev & EV_JAM) sfx_car(SFX_JAM, i, HA_VOL_FEEDBACK);
        if (ev & EV_DROP) ha_play_slot(SFX_DROP, W.c[i].human ? HA_VOL_CRASH : 60, RS_SCREEN_W / 2, 0);
        if ((ev & EV_LAP) && (W.c[i].human || i == race_focus(&W))) ha_play_slot(SFX_LAP, HA_VOL_FEEDBACK, RS_SCREEN_W / 2, 0);
        if (ev & EV_BEEP) ha_play_slot(SFX_BEEP, HA_VOL_UI, RS_SCREEN_W / 2, 0);
        if (ev & EV_GO) ha_play_slot(SFX_GO, HA_VOL_ACTION, RS_SCREEN_W / 2, 0);
        if ((ev & EV_FINISH) && i == 0) ha_play_slot(SFX_FINISH, HA_VOL_ACTION, RS_SCREEN_W / 2, 1);
        if ((ev & EV_DROP) && W.c[i].human) {
            hu_shake(2, 10);                    /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d left behind (out) at frame %u (race %d)", W.c[i].human, rs_frame_count(), RI.race + 1);
        }
        if ((ev & EV_FALL) && W.c[i].human) {
            hu_shake(3, 12);
            rs_log("player %d fell off at frame %u", W.c[i].human, rs_frame_count());
        }
    }
}

static void race_done(void)
{
    int players = hu_players();
    for (int i = 0; i < RACERS; i++) RI.total[i] += W.c[i].points;
    for (int p = 0; p < players && p < MAX_PLAYERS; p++) RI.score[p] = RI.total[p];
    RI.out = players == 1 && !champ_qualifies(W.final_place[0]);
    RI.champion = players == 1 && !RI.out && RI.race + 1 >= RI.races;
    rs_log("race %d done at frame %u: track %d%s, places %d %d %d %d, points %d %d %d %d", RI.race + 1, rs_frame_count(),
           W.track, W.reverse ? " reversed" : "", W.final_place[0], W.final_place[1], W.final_place[2], W.final_place[3],
           W.c[0].points, W.c[1].points, W.c[2].points, W.c[3].points);
    ha_play(RI.out ? HA_THUD : HA_CONFIRM);
    go(ST_RESULTS);
}

static void play_update(void)
{
    pin in[RACERS];
    memset(in, 0, sizeof in);
    for (int p = 0; p < W.humans && p < RACERS; p++) in[p] = player_input(p);
    world_step(&W, in);
    race_sounds();
    hash_world();
    if (W.phase == PH_DONE) race_done();
}

/* the title: players join and leave (the kit's lobby), P1 starts (A, a start input, or a tap); the bots race behind */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) ha_play(HA_CONFIRM);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if ((ev & HU_TITLE_START) || touch_lifted(st_t) || (opt_bot && st_t >= BOT_START)) {
        ha_play(HA_CONFIRM);
        new_run();
        return;
    }
    world_step(&W, NULL);
    if (W.phase == PH_DONE) {
        bg_track = (bg_track + 1) % TRACKS;
        title_race();
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
        touch_reset();                          /* the fingers do nothing while paused */
        return;
    }
    hu_shake_step();
    switch (st) {
    case ST_TITLE:
        title_update();
        break;
    case ST_PLAY:
        play_update();
        break;
    case ST_RESULTS: {
        world_step(&W, NULL);                   /* the race goes on behind the panel */
        int next = 0;
        if (st_t >= RESULTS_LOCK) {
            next = hu_over_back() || touch_lifted(st_t - RESULTS_LOCK) || st_t >= RESULTS_FRAMES;
            if (opt_bot && !opt_botout && st_t == RESULTS_LOCK + 10) next = 1;
        }
        if (next) {
            ha_play(HA_SELECT);
            if (RI.out || RI.race + 1 >= RI.races) game_over();
            else start_race(RI.race + 1);
        }
        break;
    }
    case ST_OVER: {
        world_step(&W, NULL);
        int back = 0;
        if (st_t >= RETRY_LOCK) {
            back = hu_over_back();              /* house rule: after a game over, back to the title (the only menu) */
            if (touch_lifted(st_t - RETRY_LOCK)) back = 1;   /* a tap that began after the lock */
            if (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10) back = 1;
        }
        if (back) {
            ha_play(HA_SELECT);
            title_race();
            go(ST_TITLE);
        }
        break;
    }
    }
    st_t++;
}

static void game_draw(void) { draw_frame(&W, st, st_t, run_best, new_best, paused, &TV, &RI); }

static void game_init(void)
{
    rs_screen_tall(RS_SCREEN_H_MAX);      /* a phone held upright: more of the track above and below (DESIGN.md) */
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; B, Start and the d-pad too), up to 4 players: PRESS A TO RACE */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botskill = rs_option_int("botskill", 0);
    opt_botout = rs_option_int("botout", 0);
    opt_race = rs_option_int("race", 0);
    opt_track = rs_option_int("track", -1);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    bg_track = 0;
    title_race();
    go(ST_TITLE);
    if (rs_option_int("ready", 0)) new_run();
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[64] = "", pl[32] = "";
    for (int p = 0; p < W.humans; p++) {
        size_t a = strlen(sc);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", RI.score[p]);
    }
    for (int i = 0; i < RACERS; i++) {
        size_t a = strlen(pl);
        snprintf(pl + a, sizeof pl - a, "%s%d", i ? "," : "", W.c[i].points);
    }
    const car *c = &W.c[0];
    rs_log("state: st=%d players=%d race=%d track=%d rev=%d phase=%d lap=%d place=%d points=%d score=%d best=%u runs=%d "
           "out=%d champ=%d x=%d y=%d ang=%d t=%d drops=%d falls=%d leader=%d paused=%d touch=%d hash=%08x scores=%s "
           "pts=%s",
           st, hu_players(), RI.race, W.track, W.reverse, W.phase, c->lap, c->place, c->points, RI.score[0], SV.best,
           runs_done, RI.out, RI.champion, c->x / FX, c->y / FX, c->ang, W.t, c->drops, c->falls, W.leader, paused, TV.seen,
           state_hash, sc, pl);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(RI); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(bg_track); S(cup_first);
    S(TV); S(touch_stick_slot); S(touch_btn_slot); S(state_hash);
    S(opt_bot); S(opt_botruns); S(opt_botskill); S(opt_botout); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound); S(opt_race); S(opt_track);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Ladybug Laps", "ladybuglaps", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
