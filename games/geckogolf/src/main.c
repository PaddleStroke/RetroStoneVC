/*
 * Gecko Golf: the game flow (the title with players 2-4 joining, play, game over, pause), input (pads and touch), sound,
 * save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu (the first hole behind it).
 * Pads 2-4 join with A (B leaves); P1 starts the run at once with A (or an extra START INPUT: the d-pad, Start); one
 * press on the game-over panel (hu_over_back) goes BACK TO THE TITLE, with the same players.
 * The pads (the player whose shot it is): the d-pad turns the aim (Left / Right round the circle, Up towards the sky,
 * Down towards the ground; held, faster and faster), A held charges the power gauge (it fills in a second, then empties
 * again: let go at the right moment), released: the shot; B while charging: no shot.
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Controls"): the slingshot: put a finger down
 * anywhere, pull back away from where the ball should go (the aim and the power follow the pull, the arc shows), let go:
 * the shot; a short pull (TOUCH_DEAD) is no shot. A tap starts from the title and leaves the game-over panel.
 * What the finger sets is kept in a registered state (touch_*, TV): a load forgets the fingers.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_plan_shot: it simulates the shots); botruns=N runs;
 *                  botstop=H: once it has H holes (P2-P4: H+2, H+4..) it shoots out of bounds (the tests' game over)
 *   botskill=E     a human-like bot (1..10: the aim and the power off by up to E steps; 5 and more: no fine tuning)
 *   seed=N         a fixed course (default: from the frame of the start)
 *   hole=N         start at hole N (screenshots: the biomes; the holes count from there)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title
 *   showmsg=M      (the language test) a message on the screen from the start (MSG_*; 7: the biome name)
 *   strokes=N      (tests) each player starts with N strokes
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

/* the house buttons: A (Start and the d-pad too) starts; Select (or Start in a run) pauses */
#define START_INPUTS  (RS_BTN_START | HU_IN_DPAD)

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound, opt_hole;
static int opt_showmsg, opt_strokes;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static int start_press;                       /* P1's start press on the title: it only starts */
static int aim_held[MAX_PLAYERS];             /* frames the d-pad has been held (the aim speeds up) */
static bot_plan plans[MAX_PLAYERS];           /* the bots' next shots */
static touch_view TV;                         /* the slingshot's finger (drawn by draw.c) */
static int touch_slot = -1;                   /* its slot (-1: none) */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the most holes (house convention: checked, versioned, committed at each game over) --------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t aces, pad2;
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "GOLF";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x474f;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x6ec0601fu);
    ha_hiss(&s, 0, 70, 30, 1);                              /* the swing: a swish ... */
    ha_tone(&s, ha_ms(55), 1500, 900, 25, 70, 12);          /* ... and the club's crisp "tock" */
    ha_tone(&s, ha_ms(55), 700, 520, 40, 40, 20);
    ha_store(&s, SFX_SWING, 100);
    ha_tone(&s, 0, 900, 640, 30, 44, 14);                   /* the ball on grass: a soft wooden "tok" */
    ha_hiss(&s, 0, 20, 10, 3);
    ha_store(&s, SFX_TOK, 40);
    ha_hiss(&s, 0, 80, 34, 3);                              /* sand: a muffled "pff" */
    ha_tone(&s, 0, 240, 160, 40, 18, 20);
    ha_store(&s, SFX_SAND, 90);
    ha_tone(&s, 0, 2600, 2300, 40, 30, 20);                 /* ice: a glassy "tink" */
    ha_tone(&s, ha_ms(4), 3900, 3700, 30, 14, 24);
    ha_store(&s, SFX_ICE, 60);
    ha_tone(&s, 0, 180, 720, 120, 70, 60);                  /* a mushroom: "boing" */
    ha_tone(&s, ha_ms(20), 360, 1100, 90, 24, 50);
    ha_store(&s, SFX_BOING, 130);
    ha_tone(&s, 0, 520, 470, 40, 50, 20);                   /* in the cup: a hollow rattle, then a bright plink */
    ha_tone(&s, ha_ms(45), 600, 560, 30, 34, 16);
    ha_tone(&s, ha_ms(90), 1568, 1568, 120, 44, 90);
    ha_tone(&s, ha_ms(90), 2349, 2349, 110, 18, 80);
    ha_store(&s, SFX_CUP, 220);
    ha_hiss(&s, 0, 150, 60, 2);                             /* the water: a splash and a bloop */
    ha_tone(&s, ha_ms(30), 700, 220, 110, 40, 50);
    ha_store(&s, SFX_SPLASH, 160);
    ha_tone(&s, 0, 660, 330, 150, 46, 80);                 /* out of bounds: a falling "whoop" */
    ha_store(&s, SFX_OB, 150);
    ha_tone(&s, 0, 1047, 1047, 55, 44, 40);                 /* a hole in one: a happy arpeggio up */
    ha_tone(&s, ha_ms(55), 1319, 1319, 55, 44, 40);
    ha_tone(&s, ha_ms(110), 1568, 1568, 55, 44, 40);
    ha_tone(&s, ha_ms(165), 2093, 2093, 135, 50, 110);
    ha_tone(&s, ha_ms(165), 1047, 1047, 135, 22, 110);
    ha_store(&s, SFX_ACE, 300);
    ha_tone(&s, 0, 2200, 2200, 12, 24, 8);                  /* the aim: a tiny tick */
    ha_store(&s, SFX_TICK, 15);
    ha_tone(&s, 0, 392, 330, 130, 44, 80);                 /* out of strokes: two sad notes down */
    ha_tone(&s, ha_ms(140), 330, 247, 160, 44, 120);
    ha_store(&s, SFX_OUT, 300);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx_at(int id, int x, int vol) { ha_play_slot(id, vol, x, id == SFX_CUP || id == SFX_SPLASH || id == SFX_ACE); }

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void touch_reset(void)
{
    touch_slot = -1;
    TV.on = 0;
}

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed, opt_hole);
    if (opt_strokes > 0)
        for (int p = 0; p < MAX_PLAYERS; p++) W.g[p].strokes = opt_strokes;
    if (opt_showmsg) {
        W.msg = opt_showmsg;
        W.msg_n = opt_showmsg == MSG_BIOME ? BI_SNOW : opt_showmsg == MSG_ACE ? REFUND_ACE : 1;
    }
    new_best = 0;
    memset(aim_held, 0, sizeof aim_held);
    memset(plans, 0, sizeof plans);
    touch_reset();
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

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

/* the d-pad's aim: Left / Right round the circle, Up towards the sky, Down towards the ground; held: faster */
static int pad_turn(int p, uint16_t held)
{
    int aim = W.g[p].aim, dir = 0;
    if (held & RS_BTN_LEFT) dir++;
    if (held & RS_BTN_RIGHT) dir--;
    if (held & RS_BTN_UP) dir += aim < 180 ? 1 : aim > 180 ? -1 : 0;
    if (held & RS_BTN_DOWN) dir += aim <= 180 ? -1 : 1;
    if (!(held & HU_IN_DPAD)) {
        aim_held[p] = 0;
        return 0;
    }
    int f = aim_held[p]++, step = f == 0 ? 1 : f < 10 ? 0 : f < 30 ? 1 : f < 60 ? 2 : 4;
    int d = dir * step;
    if ((held & RS_BTN_UP) && !(held & HU_IN_LR) && (aim - 180) * (aim + d - 180) < 0) d = 180 - aim;   /* up: stop at the top */
    return d;
}

/* the slingshot (player 1): a finger down anywhere, pulled back; let go: the shot */
static void touch_read(pin *in)
{
    if (touch_slot < 0 && W.phase == PH_CHARGE && W.charge_touch)
        in->cancel = 1;                                 /* the finger that charged is gone (a pause): no shot */
    if (touch_slot < 0) {
        for (int i = 0; i < RS_TOUCH_MAX; i++) {
            const rs_touch_point *t = rs_touch(i);
            if (t && t->pressed) {
                touch_slot = i;
                TV.x0 = t->x0;
                TV.y0 = t->y0;
                TV.x = t->x;
                TV.y = t->y;
                TV.seen = 1;
                break;
            }
        }
        if (touch_slot < 0) return;
    }
    const rs_touch_point *t = rs_touch(touch_slot);
    if (!t) {                                           /* gone without a release (a load): nothing */
        touch_reset();
        return;
    }
    TV.x = t->x;
    TV.y = t->y;
    int aim, gauge, shot = touch_shot(TV.x0 - t->x, TV.y0 - t->y, &aim, &gauge);
    in->any = 1;
    if (t->down && !t->released) {
        TV.on = 1;
        if (shot) {
            in->set_aim = (int16_t)aim;
            in->set_gauge = (int16_t)gauge;
        } else if (W.phase == PH_CHARGE && W.charge_touch) {
            in->set_gauge = 0;                          /* back inside the dead zone: an empty gauge */
        }
        return;
    }
    if (shot && gauge >= GAUGE_MIN_SHOT) {
        in->set_aim = (int16_t)aim;
        in->set_gauge = (int16_t)gauge;
        in->shoot = 1;
    } else {
        in->cancel = 1;
    }
    touch_reset();
}

static void bot_input(int p, pin *in)
{
    const golfer *g = &W.g[p];
    bot_plan *pl = &plans[p];
    int key = g->hole_strokes + W.shots * 16;
    in->any = 1;
    if (W.phase == PH_AIM) {
        if (!pl->valid || pl->hole != W.hole || pl->stroke != key) {
            if (W.phase_t < BOT_THINK / 2) return;
            if (opt_botstop && g->holes >= opt_botstop + 2 * p) bot_give_up(&W, p, pl);
            else bot_plan_shot(&W, p, opt_botskill, pl);
        }
        if (W.phase_t < BOT_THINK) return;
        int d = pl->aim - g->aim;
        if (d) in->turn = (int16_t)(d > 4 ? 4 : d < -4 ? -4 : d);
        else in->hold = 1;
        return;
    }
    if (W.phase == PH_CHARGE) in->hold = W.gauge < pl->gauge;
}

static pin player_input(int p)
{
    pin in;
    pin_none(&in);
    if (is_bot(p)) {
        bot_input(p, &in);
        return in;
    }
    int pad = hu_player_pad(p);
    uint16_t held = rs_pad(pad), pr = rs_pad_pressed(pad);
    in.turn = (int16_t)pad_turn(p, held);
    in.hold = (held & RS_BTN_A) != 0;
    in.cancel = (pr & RS_BTN_B) != 0;
    if (held & (RS_BTN_A | RS_BTN_B | HU_IN_DPAD)) in.any = 1;
    if (p == 0) {
        if (W.phase == PH_CHARGE && !W.charge_touch) {
            touch_reset();                              /* the pad charges: a finger then is no let-go (no shot) */
        } else {
            touch_read(&in);
            if (TV.on || in.shoot || in.set_gauge >= 0) in.hold = 0;   /* the finger leads */
        }
    }
    return in;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int best_now = 0, aces = 0;
    for (int p = 0; p < W.players; p++) {
        if (W.g[p].holes > best_now) best_now = W.g[p].holes;
        aces += W.g[p].aces;
    }
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.g[0].holes, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (best_now > run_best) { run_best = best_now; new_best = 1; }
    if (best_now > SV.best) SV.best = (uint16_t)best_now;
    if (W.players >= 2 && best_now > SV.best_multi) SV.best_multi = (uint16_t)best_now;
    SV.aces = (uint16_t)(SV.aces + aces);
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: %d holes, %d strokes, %d aces (best %u)", runs_done, rs_frame_count(),
           W.g[0].holes, W.g[0].total, W.g[0].aces, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void hash_world(void)
{
    for (int p = 0; p < W.players; p++) {
        const golfer *g = &W.g[p];
        state_hash = (state_hash ^ (uint32_t)g->b.x) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)g->b.y) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(g->strokes * 131 + g->holes * 7 + g->aim * 3 + g->out)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)(W.phase * 1000 + W.gauge + W.turn * 17)) * 16777619u;
    state_hash = (state_hash ^ (uint32_t)W.view_cam) * 16777619u;
}

static void play_sounds(void)
{
    int p = W.turn;
    const golfer *g = &W.g[p];
    int x = (int)(g->b.x >> 16) + (int)(W.hole_cam - W.view_cam);
    for (int q = 0; q < W.players; q++) {
        int ev = W.events[q];
        if (ev & EV_SHOT) sfx_at(SFX_SWING, x, HA_VOL_ACTION);
        if (ev & EV_AIM) sfx_at(SFX_TICK, x, 30);
        if (ev & EV_MUSH) sfx_at(SFX_BOING, x, HA_VOL_FEEDBACK);
        else if (ev & EV_BOUNCE) {
            int s = W.bounce_surf;
            sfx_at(s == SF_SAND ? SFX_SAND : s == SF_ICE ? SFX_ICE : SFX_TOK, x, s == SF_SAND ? 60 : 50);
        }
        if (ev & EV_ACE) sfx_at(SFX_ACE, x, HA_VOL_FEEDBACK);
        else if (ev & EV_CUP) sfx_at(SFX_CUP, x, HA_VOL_FEEDBACK);
        if (ev & EV_REFUND) ha_play(HA_DING);
        if (ev & EV_SPLASH) sfx_at(SFX_SPLASH, x, HA_VOL_FEEDBACK);
        if (ev & EV_OB) sfx_at(SFX_OB, x, HA_VOL_FEEDBACK);
        if (ev & EV_OUT) {
            sfx_at(SFX_OUT, x, HA_VOL_CRASH);
            rs_log("player %d out of strokes at frame %u (%d holes)", q + 1, rs_frame_count(), W.g[q].holes);
        }
        if ((ev & EV_TURN) && W.players > 1) ha_play(HA_SELECT);
    }
    if (W.wevents & EV_PAN) {
        ha_play(HA_SWISH);
        rs_log("hole %d at frame %u", W.hole + 1, rs_frame_count());
    }
    if (W.wevents & EV_BIOME) ha_play(HA_CONFIRM);
}

static void play_update(void)
{
    pin in[MAX_PLAYERS];
    for (int p = 0; p < MAX_PLAYERS; p++) pin_none(&in[p]);
    if (st == ST_PLAY && (W.phase == PH_AIM || W.phase == PH_CHARGE) && !start_press) in[W.turn] = player_input(W.turn);
    else if (!is_bot(0)) touch_reset();
    start_press = 0;
    world_step(&W, in);
    play_sounds();
    hash_world();
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && st_t >= PANEL_DELAY) game_over();
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
        start_press = 1;                        /* the start press only starts */
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
        touch_reset();                          /* the fingers do nothing while paused */
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

static void game_draw(void) { draw_frame(&W, st, st_t, run_best, new_best, paused, &TV); }

static void game_init(void)
{
    rs_screen_zoom(ZOOM_H);               /* a phone held upright: zoomed, the camera pans (draw.c, DESIGN.md) */
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; Start and the d-pad too), up to 4 players: PRESS A TO SWING */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_hole = rs_option_int("hole", 0);
    opt_showmsg = rs_option_int("showmsg", 0);
    opt_strokes = rs_option_int("strokes", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[48] = "", sk[48] = "", os[24] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(sk), c = strlen(os);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.g[p].holes);
        snprintf(sk + b, sizeof sk - b, "%s%d", p ? "," : "", W.g[p].strokes);
        snprintf(os + c, sizeof os - c, "%s%d", p ? "," : "", W.g[p].out);
    }
    const golfer *g = &W.g[0];
    rs_log("state: st=%d players=%d score=%d strokes=%d best=%d runs=%d hole=%d phase=%d turn=%d shots=%d aim=%d "
           "gauge=%d x=%d y=%d ball=%d total=%d aces=%d cam=%d paused=%d touch=%d hash=%08x scores=%s left=%s out=%s",
           st, W.players, g->holes, g->strokes, (int)SV.best, runs_done, W.hole, W.phase, W.turn, W.shots, g->aim,
           W.gauge, (int)(g->b.x >> 16), (int)(g->b.y >> 16), g->b.state, g->total, g->aces, (int)W.view_cam, paused,
           TV.seen, state_hash, sc, sk, os);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press); S(aim_held); S(plans);
    S(TV); S(touch_slot); S(state_hash);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound); S(opt_hole); S(opt_showmsg); S(opt_strokes);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Gecko Golf", "geckogolf", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
