/*
 * Jelly Jam: the game flow (the title with players 2-4 joining, the duels against the chefs or the rounds between
 * players, game over, pause), input (pads and touch), sound, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A (B
 * leaves); P1 starts at once with A (or B, Start; a tap). One player: duels against the computer chefs, one after the
 * other, each stronger; a lost duel is the game over. Two to four: everyone against everyone, best of 3 rounds, then the
 * results. One press on the game-over panel (hu_over_back) goes BACK TO THE TITLE, with the same players.
 * The pads: left / right move the pair (held: it repeats), down drops it faster, A (or X) turns it clockwise, B (or Y)
 * counter-clockwise.
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Controls"): drag left or right to move the
 * pair column by column, tap to turn it, swipe down to drop it, a finger dragged down drops it faster; a tap starts from
 * the title and leaves the game-over panel. What the fingers set is kept in a registered state (touch_*).
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (play.c ai_play); botskill=E a human-like bot (1..10: slower, plans less,
 *                  more mistakes), to tune the medals; botruns=N runs; botstop=F: after F frames of a duel or a round
 *                  (P2-P4: F + 600, + 1200...) the bot gives up (drops its pairs at once: the tests' game over)
 *   seed=N         fixed pairs (default: from the frame of the start)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title;  level=L  start at chef L (1 player)
 *   dump=1         log the final state at exit (tests); music=0, sound=0
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include "game_text.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the house buttons: A (B, Start too) starts; Select (or Start in a run) pauses */
#define START_INPUTS  (RS_BTN_B | RS_BTN_START)
#define TURN_CW       (RS_BTN_A | RS_BTN_X)
#define TURN_CCW      (RS_BTN_B | RS_BTN_Y)

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound, opt_level;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static int start_press;                       /* P1's start press on the title: it only starts */
static touch_view TV;                         /* the finger (draw.c) */
static int touch_slot = -1;                   /* the finger that steers the pair (-1: none) */
static int touch_ax, touch_acol, touch_aseq;  /* where it was when it took the pair: x, the pair's column, the pair */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) -------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, best_level, pad[2];
    uint32_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "JLJM";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x4a4a;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x6a3e11a5u);
    ha_tone(&s, 0, 900, 1100, 18, 26, 10);                  /* a step: a tiny "tick" */
    ha_store(&s, SFX_MOVE, 22);
    ha_tone(&s, 0, 520, 980, 50, 40, 30);                   /* a turn: a wobbly "bloop" up */
    ha_store(&s, SFX_TURN, 60);
    ha_tone(&s, 0, 300, 160, 60, 50, 30);                   /* a landing: a soft "plop" */
    ha_hiss(&s, 0, 30, 10, 3);
    ha_store(&s, SFX_LAND, 70);
    ha_tone(&s, 0, 660, 1320, 70, 60, 40);                  /* a melt: a juicy "pop" (pitched up with the chain) */
    ha_tone(&s, ha_ms(20), 990, 1980, 60, 28, 30);
    ha_hiss(&s, 0, 40, 14, 2);
    ha_store(&s, SFX_POP, 110);
    ha_tone(&s, 0, 1568, 1568, 90, 40, 50);                 /* a chain step: a bright bell */
    ha_tone(&s, ha_ms(50), 2093, 2093, 120, 30, 60);
    ha_store(&s, SFX_CHAIN, 170);
    ha_tone(&s, 0, 180, 90, 140, 70, 70);                   /* rain falling on you: a heavy "thud thud" */
    ha_tone(&s, ha_ms(90), 160, 80, 140, 60, 70);
    ha_hiss(&s, 0, 200, 24, 3);
    ha_store(&s, SFX_RAIN, 210);
    ha_tone(&s, 0, 440, 110, 300, 60, 140);                 /* your field lost: a long sad slide */
    ha_hiss(&s, ha_ms(60), 240, 20, 3);
    ha_store(&s, SFX_LOSE, 310);
    ha_tone(&s, 0, 784, 784, 90, 44, 50);                   /* a duel or a round won: G5 B5 D6 G6 */
    ha_tone(&s, ha_ms(90), 988, 988, 90, 44, 50);
    ha_tone(&s, ha_ms(180), 1175, 1175, 90, 44, 50);
    ha_tone(&s, ha_ms(270), 1568, 1568, 160, 50, 90);
    ha_store(&s, SFX_WIN, 430);
    ha_hiss(&s, 0, 160, 34, 1);                             /* rain sent: a whoosh up */
    ha_tone(&s, 0, 400, 1400, 140, 24, 80);
    ha_store(&s, SFX_SEND, 170);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

/* the screen x of field i's centre (the sounds' pan) */
static int pan_of(int i)
{
    flayout L[MAX_FIELDS];
    int n = screen_layout(W.nf, L);
    return i < n ? L[i].x + L[i].w / 2 : RS_SCREEN_W / 2;
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void touch_reset(void)
{
    touch_slot = -1;
    TV.down = 0;
}

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    if (state == ST_PLAY) world_start(&W, opt_level);
    new_best = 0;
    touch_reset();
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

/* ---- touch (player 1) ------------------------------------------------------------------------------------------- */
/* A finger was lifted this frame that went down during the last `since` frames (on this screen, after its lock):
 * the touch screen's "press A" (a finger still down from the run before never counts). */
static int touch_lifted(int since)
{
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (t && t->released && t->frames < since) return 1;
    }
    return 0;
}

/* the finger steering player 1's pair: drag = the column under the drag, tap = turn, swipe / drag down = drop */
static void touch_read(pin *in)
{
    const field *f = &W.f[0];
    flayout L[MAX_FIELDS];
    screen_layout(W.nf, L);
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (t && t->pressed && touch_slot < 0) {
            touch_slot = i;
            touch_ax = t->x;
            touch_acol = f->pr.col;
            touch_aseq = f->seq;
        }
    }
    if (touch_slot < 0) return;
    const rs_touch_point *t = rs_touch(touch_slot);
    if (!t) {
        touch_reset();
        return;
    }
    TV.seen = 1;
    TV.x = t->x;
    TV.y = t->y;
    TV.down = t->down;
    if (t->released) {
        if (t->tap && f->state == FS_PAIR) in->cw = 1;
        touch_reset();
        return;
    }
    if (f->state != FS_PAIR) return;
    if (f->seq != touch_aseq) {                     /* a new pair under a finger still down: it steers from here */
        touch_ax = t->x;
        touch_acol = f->pr.col;
        touch_aseq = f->seq;
    }
    if (t->swipe == RS_SWIPE_DOWN) in->fast = 1;
    int dy = t->y - t->y0, dx = t->x - t->x0;
    if (dy >= TOUCH_SOFT_PX && dy > 2 * (dx < 0 ? -dx : dx)) {
        in->down = 1;
        return;
    }
    int c = touch_column(touch_acol, t->x - touch_ax, L[0].cell);
    if (c != f->pr.col) in->step = (int8_t)(c > f->pr.col ? 1 : -1);
}

static pin player_input(int p)
{
    pin in;
    memset(&in, 0, sizeof in);
    if (is_bot(p)) {
        if (opt_botstop && W.round_t > opt_botstop + 600 * p) in.down = 1;   /* gives up: drops every pair at once */
        else {                                              /* P2-P4 a little weaker each: varied fields */
            ai_params P;
            int sk = opt_botskill + 2 * p;
            ai_skill_params(sk > 10 ? 10 : sk, &P);
            ai_play(&W, p, &P, &in);
        }
        return in;
    }
    int pad = hu_player_pad(p);
    uint16_t held = rs_pad(pad), pr = rs_pad_pressed(pad);
    int l = (held & RS_BTN_LEFT) != 0, r = (held & RS_BTN_RIGHT) != 0;
    in.dir = (int8_t)(r - l);
    in.dir_pressed = (pr & (RS_BTN_LEFT | RS_BTN_RIGHT)) != 0;
    in.down = (held & RS_BTN_DOWN) != 0;
    in.cw = (pr & TURN_CW) != 0;
    in.ccw = (pr & TURN_CCW) != 0;
    if (p == 0) touch_read(&in);
    if (p == 0 && start_press) memset(&in, 0, sizeof in);  /* the start press only starts */
    return in;
}

static void hash_world(void)
{
    for (int i = 0; i < W.nf; i++) {
        const field *f = &W.f[i];
        state_hash = (state_hash ^ (uint32_t)(f->score * 7 + f->seq * 131 + f->state)) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(f->pr.col * 1000 + f->pr.rot * 100000 + f->pr.y)) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(field_count(f) + f->rain_q * 97)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)(W.phase * 31 + W.level * 7 + W.round)) * 16777619u;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int best_now = 0;
    for (int p = 0; p < W.players; p++)
        if (W.f[p].score > best_now) best_now = W.f[p].score;
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.f[0].score, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (W.players == 1) {
        if (best_now > run_best) { run_best = best_now; new_best = 1; }
        if ((uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
        if (W.level > SV.best_level) SV.best_level = (uint8_t)(W.level > 255 ? 255 : W.level);
    } else if ((uint32_t)best_now > SV.best_multi) {
        SV.best_multi = (uint32_t)best_now;
    }
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d level %d (best %u)", runs_done, rs_frame_count(), W.f[0].score, W.level,
           SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void sounds(void)
{
    for (int i = 0; i < W.nf; i++) {
        int ev = W.events[i], x = pan_of(i);
        const field *f = &W.f[i];
        int mine = W.mode == MODE_VS || i == 0;             /* the chef's small noises are quieter */
        if (ev & EV_MOVE) ha_play_slot(SFX_MOVE, mine ? 40 : 24, x, 0);
        if (ev & EV_TURN) ha_play_slot(SFX_TURN, mine ? 56 : 30, x, 0);
        if (ev & EV_LAND) ha_play_slot(SFX_LAND, mine ? 60 : 36, x, 0);
        if (ev & EV_MELT) {
            int step = f->chain < 1 ? 0 : f->chain > 8 ? 7 : f->chain - 1;
            static const int pitch[8] = {0x1000, 0x1200, 0x1430, 0x1560, 0x1800, 0x1af0, 0x1e30, 0x2000};
            ha_play_pitched(SFX_POP, HA_VOL_ACTION, x, pitch[step], 1);
            if (f->chain >= 2) ha_play_slot(SFX_CHAIN, HA_VOL_FEEDBACK, x, 1);
            if (f->chain >= 3) rs_log("field %d chain %d at frame %u", i + 1, f->chain, rs_frame_count());
        }
        if (ev & EV_SENT) ha_play_slot(SFX_SEND, 60, x, 0);
        if (ev & EV_RAIN) {
            ha_play_slot(SFX_RAIN, HA_VOL_CRASH, x, 0);
            hu_shake(2, 10);                         /* house rule: a shake on a hit only, <= 4 px, <= 16 frames */
        }
        if (ev & EV_LOSE) {
            ha_play_slot(SFX_LOSE, HA_VOL_CRASH, x, 0);
            hu_shake(3, 12);
            rs_log("field %d lost at frame %u (score %d, round %d, level %d)", i + 1, rs_frame_count(), f->score, W.round,
                   W.level);
        }
        if (ev & EV_WIN) {
            ha_play_slot(SFX_WIN, HA_VOL_FEEDBACK, RS_SCREEN_W / 2, 1);
            rs_log("field %d won at frame %u (level %d, round %d)", i + 1, rs_frame_count(), W.level, W.round);
        }
    }
}

static void play_update(void)
{
    pin in[MAX_FIELDS];
    memset(in, 0, sizeof in);
    if (W.phase == PH_PLAY)
        for (int p = 0; p < W.players && p < W.nf; p++)
            if (W.f[p].alive && W.f[p].kind == FK_HUMAN) in[p] = player_input(p);
    start_press = 0;
    world_step(&W, in);
    sounds();
    hash_world();
    if (st == ST_PLAY && world_over(&W)) go(ST_DEAD);
    if (st == ST_DEAD && st_t >= PANEL_DELAY) game_over();
}

/* the title: players join and leave (the kit's lobby), P1 starts (A, a start input, or a tap) */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) ha_play(HA_CONFIRM);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) W.players = hu_players();
    if ((ev & HU_TITLE_START) || touch_lifted(st_t) || (opt_bot && st_t >= BOT_START)) {
        ha_play(HA_CONFIRM);
        new_run(ST_PLAY);
        start_press = 1;
        play_update();
    }
}

static void game_update(void)
{
    ha_music("music/tune.mod", opt_music);
    if ((st == ST_PLAY || st == ST_DEAD) && (rs_pad_pressed(0) & (RS_BTN_SELECT | RS_BTN_START))) {
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
        pin none[MAX_FIELDS];
        memset(none, 0, sizeof none);
        world_step(&W, none);                   /* the fields' animations go on behind the panel */
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
    rs_screen_zoom(ZOOM_H);               /* a phone held upright: zoomed, the jars side by side (draw.c, DESIGN.md) */
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; B and Start too), up to 4 players: PRESS A TO COOK */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_level = rs_option_int("level", 1);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[64] = "", ws[32] = "", rq[48] = "";
    for (int i = 0; i < W.nf; i++) {
        size_t a = strlen(sc), b = strlen(ws), c = strlen(rq);
        snprintf(sc + a, sizeof sc - a, "%s%d", i ? "," : "", W.f[i].score);
        snprintf(ws + b, sizeof ws - b, "%s%d", i ? "," : "", W.f[i].wins);
        snprintf(rq + c, sizeof rq - c, "%s%d", i ? "," : "", W.f[i].rain_q);
    }
    const field *f = &W.f[0];
    rs_log("state: st=%d players=%d score=%d level=%d round=%d phase=%d mode=%d best=%u runs=%d col=%d rot=%d fstate=%d "
           "seq=%d count=%d chain=%d melted=%d paused=%d touch=%d hash=%08x scores=%s wins=%s rain=%s",
           st, W.players, f->score, W.level, W.round, W.phase, W.mode, SV.best, runs_done, f->pr.col, f->pr.rot, f->state,
           f->seq, field_count(f), f->max_chain, f->melted, paused, TV.seen, state_hash, sc, ws, rq);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press);
    S(TV); S(touch_slot); S(touch_ax); S(touch_acol); S(touch_aseq); S(state_hash);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound); S(opt_level);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Jelly Jam", "jellyjam", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
