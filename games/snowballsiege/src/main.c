/*
 * Snowball Siege: the game flow (the title with players 2-4 joining, play, game over, pause), input (pads and touch),
 * sound, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu (the battlefield behind it,
 * the camera drifting along). Pads 2-4 join with A (B leaves); P1 starts with A (or Start); one press on the
 * game-over panel (hu_over_back) goes BACK TO THE TITLE, with the same players.
 * 1 player: matches against the computer (play.c cpu_think), one after the other, the computer better each time; a
 * lost match is the game over. 2-4 players: one match, hot seat: each player's pad (and touch for P1) plays its side's
 * turn; the results rank the sides by survival, then score.
 * The pads, on your turn: Left/Right walk (a tap turns round), Up/Down aim, A held charges the throw and throws when
 * let go, B switches the throwable, L/R look along the field.
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Touch"): pull back from your critter like a
 * slingshot (the pull's direction aims, its length is the power) and let go to throw; a tap beside it shuffles it
 * that way; the round button at the bottom right switches the throwable; a finger dragged elsewhere looks along the
 * field; a tap starts from the title and leaves the game-over panel. The fingers' roles are in a registered state.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (cpu_think, no error: botskill=L plays like the computer of level L);
 *                  botruns=N runs; botstop=M (1 player): after M matches won it throws badly (the tests' game over)
 *   seed=N         a fixed battlefield (default: from the frame of the start)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title
 *   level=L        (screenshots, tests) the first match is match L
 *   msg=M          (the language test) message M (play.c MSG_*) shows from the start
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

/* the house buttons: A (and Start) starts; Select (or Start in a run) pauses */
#define START_INPUTS  RS_BTN_START

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound, opt_level, opt_msg;
static uint32_t opt_seed;
static world W;
static camera CAM;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static touch_view TV;                         /* the fingers (drawn by draw.c) */
static int sling_slot = -1, pan_slot = -1, btn_slot = -1, pan_last_x;
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) --------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t best_wins, sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "SNOW";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x534e;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x5a0eba11u);
    ha_hiss(&s, 0, 90, 26, 2);                              /* a throw: a soft whoosh and a little "hup" */
    ha_tone(&s, 0, 420, 620, 60, 30, 30);
    ha_store(&s, SFX_THROW, 120);
    ha_hiss(&s, 0, 110, 70, 3);                             /* a splat: a soft crunchy "pfff" */
    ha_tone(&s, 0, 260, 120, 50, 40, 30);
    ha_store(&s, SFX_SPLAT, 160);
    ha_hiss(&s, 0, 200, 90, 4);                             /* a big splat: longer, lower */
    ha_tone(&s, 0, 180, 70, 120, 56, 60);
    ha_store(&s, SFX_BIG, 200);
    ha_tone(&s, 0, 880, 520, 60, 40, 30);                   /* a critter hit: a chilly "brr" (two quick notes down) */
    ha_tone(&s, ha_ms(70), 740, 440, 70, 34, 40);
    ha_store(&s, SFX_HIT, 140);
    ha_hiss(&s, 0, 160, 60, 1);                             /* a splash: water, and a falling "bloop" */
    ha_tone(&s, ha_ms(20), 600, 180, 160, 50, 80);
    ha_store(&s, SFX_SPLASH, 210);
    ha_tone(&s, 0, 660, 330, 220, 46, 110);                 /* snowed under: a soft sad slide and a poof */
    ha_hiss(&s, ha_ms(120), 140, 40, 3);
    ha_store(&s, SFX_SNOWED, 200);
    ha_tone(&s, 0, 1500, 1900, 25, 30, 15);                 /* an ice cube's bounce: a clink */
    ha_tone(&s, ha_ms(10), 2400, 2600, 20, 18, 12);
    ha_store(&s, SFX_BOUNCE, 70);
    ha_hiss(&s, 0, 40, 10, 4);                              /* a step in the snow: a tiny crunch */
    ha_store(&s, SFX_STEP, 50);
    ha_tone(&s, 0, 1320, 1320, 20, 22, 10);                 /* the last seconds: a tick */
    ha_store(&s, SFX_TICK, 40);
    ha_tone(&s, 0, 520, 780, 40, 24, 20);                   /* switch the throwable: a little "plink" */
    ha_store(&s, SFX_SWITCH, 70);
    for (int i = 0; i < 4; i++)                             /* a match won: a rising jingle (C E G C) */
        ha_tone(&s, ha_ms(i * 80), (int[]){523, 659, 784, 1047}[i], (int[]){523, 659, 784, 1047}[i], 120, 40, 70);
    ha_store(&s, SFX_WON, 320);
    ha_hiss(&s, 0, 150, 40, 2);                             /* a tree knocked: the snow falling off it */
    ha_tone(&s, ha_ms(30), 300, 200, 90, 30, 40);
    ha_store(&s, SFX_TREE, 140);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

/* a sound at a world x: panned by where it is on the screen */
static void sfx_at(int id, int wx, int vol)
{
    int x = wx - camera_left(&CAM, rs_screen_width()) - rs_screen_margin();
    ha_play_slot(id, vol, x < 0 ? 0 : x > RS_SCREEN_W ? RS_SCREEN_W : x, id == SFX_SPLASH);
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void touch_reset(void)
{
    sling_slot = pan_slot = btn_slot = -1;
    TV.sling = 0;
    TV.btn_on = 0;
}

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    if (state == ST_PLAY) {
        if (opt_level > 1 && W.players == 1) {
            W.level = opt_level;
            W.s[1].cpu = (int8_t)opt_level;
            W.s[1].species = (int8_t)(1 + (opt_level - 1) % 3);
        }
        world_start(&W);
        if (opt_msg) {                          /* tests: a message shown for a while (the language test) */
            W.msg = opt_msg;
            W.msg_t = 30000;
        }
    }
    new_best = 0;
    touch_reset();
    draw_new_world();
    camera_init(&CAM, &W, rs_screen_width());
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

/* ---- input ----------------------------------------------------------------------------------------------------- */
/* A finger lifted this frame that went down during the last `since` frames (the touch screen's "press A") */
static int touch_lifted(int since)
{
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (t && t->released && t->frames < since) return 1;
    }
    return 0;
}

static void pad_input(int p, tin *in, int *pan)
{
    int pad = hu_player_pad(p);
    uint16_t held = rs_pad(pad), pr = rs_pad_pressed(pad);
    in->walk = (int8_t)((held & RS_BTN_RIGHT ? 1 : 0) - (held & RS_BTN_LEFT ? 1 : 0));
    in->aim = (int8_t)((held & RS_BTN_UP ? 1 : 0) - (held & RS_BTN_DOWN ? 1 : 0));
    in->a_down = (held & RS_BTN_A) != 0;
    in->b_press = (pr & (RS_BTN_B | RS_BTN_Y)) != 0;
    *pan += (held & RS_BTN_R ? CAM_PAN : 0) - (held & RS_BTN_L ? CAM_PAN : 0);
}

/* the fingers of player 1 on its turn: the slingshot, a shuffle tap, the switch button, a drag along the field */
static void touch_input(tin *in, int *pan)
{
    const critter *c = world_active_c(&W);
    int sw = rs_screen_width(), bx, by;
    int cx = (c->x >> 16) - camera_left(&CAM, sw), cy = (c->y >> 16) - CR_H / 2;   /* the critter, screen px */
    touch_button_pos(&bx, &by);
    TV.sling = 0;
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (!t) {
            if (sling_slot == i) sling_slot = -1;
            if (pan_slot == i) pan_slot = -1;
            if (btn_slot == i) { btn_slot = -1; TV.btn_on = 0; }
            continue;
        }
        rs_touch_point tp = *t;                 /* in stage px (a tall screen: below its top margin) */
        tp.y -= rs_screen_origin_y();
        tp.y0 -= rs_screen_origin_y();
        t = &tp;
        TV.seen = 1;
        if (t->pressed) {
            int dbx = t->x0 - bx, dby = t->y0 - by, dcx = t->x0 - cx, dcy = t->y0 - cy;
            if (dbx * dbx + dby * dby <= (BTN_R + 4) * (BTN_R + 4)) btn_slot = i;
            else if (sling_slot < 0 && dcx * dcx + dcy * dcy <= SLING_GRAB * SLING_GRAB) sling_slot = i;
            else if (pan_slot < 0) { pan_slot = i; pan_last_x = t->x; }
        }
        if (sling_slot == i) {
            int dx = cx - t->x, dy = cy - t->y, f, a, pw;
            int pulled = dx * dx + dy * dy >= SLING_MIN * SLING_MIN;
            sling_aim(dx, dy, &f, &a, &pw);
            in->sl_facing = (int8_t)f;
            in->sl_angle = (int16_t)a;
            in->sl_power = (int16_t)pw;
            if (t->released) {
                in->fire = (uint8_t)pulled;
                sling_slot = -1;
            } else if (pulled) {
                in->sling = 1;
                TV.sling = 1;
                TV.fx = t->x;
                TV.fy = t->y;
            }
        } else if (pan_slot == i) {
            if (t->down && t->moved) *pan += pan_last_x - t->x;
            pan_last_x = t->x;
            if (t->released) {
                if (t->tap && t->x - cx >= -SHUFFLE_ZONE && t->x - cx <= SHUFFLE_ZONE && t->y - cy >= -40 &&
                    t->y - cy <= 40)
                    in->shuffle = (int8_t)(t->x >= cx ? 1 : -1);
                pan_slot = -1;
            }
        } else if (btn_slot == i) {
            TV.btn_on = t->down;
            if (t->released) {
                int dbx = t->x - bx, dby = t->y - by;
                if (dbx * dbx + dby * dby <= (BTN_R + 6) * (BTN_R + 6)) in->b_press = 1;
                btn_slot = -1;
                TV.btn_on = 0;
            }
        }
    }
}

/* the bot that gives up (botstop): it throws a little snowball straight up, onto itself */
static void bot_give_up(tin *in)
{
    if (W.angle < AIM_MAX) { in->aim = 1; return; }
    in->a_down = !W.charging;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int best_now = 0;
    for (int s = 0; s < W.nsides; s++)
        if (W.s[s].player >= 0 && W.s[s].score > best_now) best_now = W.s[s].score;
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.s[0].score, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (best_now > run_best) { run_best = best_now; new_best = 1; }
    if ((uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
    if (W.players >= 2 && (uint32_t)best_now > SV.best_multi) SV.best_multi = (uint32_t)best_now;
    if (W.players == 1 && W.wins > SV.best_wins) SV.best_wins = (uint16_t)W.wins;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d, %d matches won (best %u)", runs_done, rs_frame_count(), W.s[0].score,
           W.wins, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void hash_world(void)
{
    for (int i = 0; i < W.ncrit; i++) {
        const critter *c = &W.c[i];
        state_hash = (state_hash ^ (uint32_t)c->x) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(c->y + c->hp * 7 + c->state * 131)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)(W.p.x + W.p.y * 3 + W.angle * 5 + W.power * 7 + W.phase)) * 16777619u;
    state_hash = (state_hash ^ (uint32_t)(W.dirty_seq * 31 + W.turns * 7 + W.s[0].score)) * 16777619u;
}

static void sounds(void)
{
    int e = W.events, x = W.ev_x;
    if (e & EV_THROW) sfx_at(SFX_THROW, x, HA_VOL_ACTION);
    if (e & EV_SPLAT) sfx_at(W.p.kind == PK_BIG ? SFX_BIG : SFX_SPLAT, x, HA_VOL_ACTION);
    if (e & EV_HIT) { sfx_at(SFX_HIT, x, HA_VOL_FEEDBACK); hu_shake(2, 10); }
    if (e & EV_SPLASH) { sfx_at(SFX_SPLASH, x, HA_VOL_CRASH); hu_shake(3, 12); }
    if (e & EV_PLOP) sfx_at(SFX_SPLASH, x, 50);
    if (e & EV_SNOWED) sfx_at(SFX_SNOWED, x, HA_VOL_CRASH);
    if (e & EV_BOUNCE) sfx_at(SFX_BOUNCE, x, HA_VOL_FEEDBACK);
    if (e & EV_STEP) sfx_at(SFX_STEP, x, 50);
    if (e & EV_TICK) sfx_at(SFX_TICK, x, HA_VOL_FEEDBACK);
    if (e & EV_SWITCH) sfx_at(SFX_SWITCH, x, HA_VOL_FEEDBACK);
    if (e & EV_TREE) sfx_at(SFX_TREE, x, 50);
    if (e & EV_TURN) ha_play(HA_SELECT);
    if (e & EV_TIMEUP) ha_play(HA_THUD);
    if (e & EV_WON) sfx_at(SFX_WON, x, HA_VOL_ACTION);
    if (e & EV_THROW) rs_log("frame %u turn %d: side %d throws (kind %d)", rs_frame_count(), W.turns, W.cur_side, W.p.kind);
    if (e & (EV_SPLAT | EV_SPLASH | EV_SNOWED))
        rs_log("frame %u turn %d: %s at x %d (scores %d,%d)", rs_frame_count(), W.turns,
               e & EV_SPLASH ? "splash" : e & EV_SNOWED ? "snowed under" : "splat", x, W.s[0].score, W.s[1].score);
    if (e & EV_MATCH) rs_log("frame %u: match %d", rs_frame_count(), W.level);
}

static void play_update(void)
{
    tin in;
    memset(&in, 0, sizeof in);
    int pan = 0, hp = world_human_turn(&W);
    if (hp >= 0 && st == ST_PLAY) {
        if (is_bot(hp)) {
            if (opt_botstop && W.players == 1 && W.wins >= opt_botstop) bot_give_up(&in);
            else cpu_think(&W, opt_botskill, &in);
        } else {
            pad_input(hp, &in, &pan);
            if (hp == 0 && W.phase == PH_AIM) touch_input(&in, &pan);
        }
    }
    if (hp != 0 || W.phase != PH_AIM) touch_reset();
    world_step(&W, &in);
    camera_step(&CAM, &W, rs_screen_width(), pan, st_t);
    if (W.events & EV_MATCH) draw_new_world();
    sounds();
    hash_world();
    if (st == ST_PLAY && world_over(&W)) go(ST_DEAD);
    if (st == ST_DEAD && st_t >= PANEL_DELAY) game_over();
}

/* the title: players join and leave (the kit's lobby), P1 starts (A, Start or a tap); the field behind */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) ha_play(HA_CONFIRM);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) {
        int t = st_t;
        new_run(ST_TITLE);                      /* the field follows the players (a side each) */
        st_t = t;
    }
    if ((ev & HU_TITLE_START) || touch_lifted(st_t) || (opt_bot && st_t >= BOT_START)) {
        ha_play(HA_CONFIRM);
        new_run(ST_PLAY);
        return;
    }
    world_step(&W, NULL);
    camera_step(&CAM, &W, rs_screen_width(), 0, st_t);
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
        world_step(&W, NULL);                   /* the snow goes on behind the panel */
        camera_step(&CAM, &W, rs_screen_width(), 0, st_t);
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

static void game_draw(void) { draw_frame(&W, &CAM, st, st_t, run_best, new_best, paused, &TV); }

static void game_init(void)
{
    rs_screen_tall(RS_SCREEN_H_MAX);           /* a phone held upright: more sky and ground (draw.c, DESIGN.md "Tall screen") */
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: A (and Start) starts, up to 4 players: PRESS A TO THROW */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_level = rs_option_int("level", 1);
    opt_msg = rs_option_int("msg", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[64] = "", hp[64] = "", xs[96] = "";
    for (int s = 0; s < W.nsides; s++) {
        size_t a = strlen(sc);
        snprintf(sc + a, sizeof sc - a, "%s%d", s ? "," : "", W.s[s].score);
    }
    for (int i = 0; i < W.ncrit; i++) {
        size_t a = strlen(hp), b = strlen(xs);
        snprintf(hp + a, sizeof hp - a, "%s%d", i ? "," : "", W.c[i].state >= CS_SNOWED ? -1 : W.c[i].hp);
        snprintf(xs + b, sizeof xs - b, "%s%d", i ? "," : "", W.c[i].x >> 16);
    }
    int solid = 0;
    for (int y = 0; y < WORLD_H; y++)
        for (int x = 0; x < WORLD_W / 8; x++) solid += __builtin_popcount(W.ter[y][x]);
    rs_log("state: st=%d players=%d score=%d wins=%d level=%d phase=%d turns=%d side=%d cur=%d facing=%d angle=%d "
           "power=%d weapon=%d wind=%d moved=%d best=%u runs=%d paused=%d touch=%d cam=%d sx=%d sy=%d solid=%d hash=%08x hp=%s "
           "xs=%s scores=%s",
           st, W.players, W.s[0].score, W.wins, W.level, W.phase, W.turns, W.cur_side, W.cur, W.c[W.cur].facing,
           W.angle, W.power, W.weapon, W.wind, W.moved, SV.best, runs_done, paused, TV.seen, CAM.x,
           (W.c[W.cur].x >> 16) - (CAM.x - RS_SCREEN_W / 2), (W.c[W.cur].y >> 16) - CR_H / 2, solid, state_hash,
           hp, xs, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(CAM); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(TV); S(sling_slot);
    S(pan_slot); S(btn_slot); S(pan_last_x); S(state_hash);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound); S(opt_level); S(opt_msg);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Snowball Siege", "snowballsiege", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
