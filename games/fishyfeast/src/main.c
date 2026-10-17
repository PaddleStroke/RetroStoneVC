/*
 * Fishy Feast: the game flow (the title with players 2-4 joining, play, game over, pause), input (pads and touch), sound,
 * save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu (the pond goes on behind it,
 * the bots swimming). Pads 2-4 join with A (B leaves); P1 starts the run at once with A (or an extra START INPUT: B,
 * the d-pad, Start); one press on the game-over panel (hu_over_back) goes BACK TO THE TITLE, with the same players.
 * The pads: the d-pad swims in 8 directions (full speed for the fish's size), A splits, B ejects (held: again every
 * EJECT_REPEAT frames).
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Controls"): a virtual stick (a finger down
 * anywhere on the left part of player 1's view, dragged: the fish swims that way, the speed by the drag's length), two
 * drawn round buttons at the bottom right (SPLIT, EJECT); a tap starts from the title and leaves the game-over panel.
 * What the fingers set is kept in a registered state (touch_*): a load forgets the fingers, one still down starts again.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the bot plays players 1..N (bot_think, the player bot); botruns=N runs;
 *                  botstop=M: once its fish has reached mass M (P2-P4: M+50, M+100..) it swims into a bigger fish (the
 *                  tests' game over)
 *   botskill=E     a human-like bot (1..10: slower, sizes misjudged, urchins forgotten, aim off), to tune the medals
 *   seed=N         a fixed pond (default: from the frame of the start)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the run waits for a press)
 *   mass=M         (screenshots, tests) the players start at mass M
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

/* the house buttons: A (B, Start and the d-pad too) starts; Select (or Start in a run) pauses */
#define START_INPUTS  (RS_BTN_B | RS_BTN_START | HU_IN_DPAD)
#define EJECT_REPEAT  8             /* frames between pellets while B is held (after 2 x this) */

static int opt_bot, opt_botstop, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound, opt_mass;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static int start_press;                       /* P1's start press on the title: the run starts at once */
static int eject_held[MAX_PLAYERS];           /* frames B has been held */
static int nom_t;                             /* frames since the last plankton sound */
static touch_view TV;                         /* the fingers: the stick, the buttons (drawn by draw.c) */
static int touch_stick_slot = -1;             /* the finger slot of the stick (-1: none) */
static uint8_t touch_btn_slot[RS_TOUCH_MAX];  /* a finger down on a button: 1 split, 2 eject */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best mass (house convention: checked, versioned, committed at each game over) --------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "FISH";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x4649;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0xf15fea57u);
    ha_tone(&s, 0, 1300, 1900, 30, 14, 14);                 /* a plankton speck: a tiny "nip" */
    ha_store(&s, SFX_NOM, 35);
    ha_tone(&s, 0, 520, 190, 90, 60, 50);                   /* a fish eaten: a round "gulp" and a bubble */
    ha_tone(&s, ha_ms(70), 900, 1500, 50, 24, 25);
    ha_hiss(&s, 0, 50, 14, 3);
    ha_store(&s, SFX_GULP, 140);
    ha_tone(&s, 0, 300, 900, 70, 40, 40);                   /* split: a quick "bloop" up and a whoosh */
    ha_hiss(&s, ha_ms(10), 80, 22, 2);
    ha_store(&s, SFX_SPLIT, 110);
    ha_tone(&s, 0, 760, 1100, 35, 30, 20);                  /* eject: a little "pop" */
    ha_store(&s, SFX_EJECT, 50);
    ha_hiss(&s, 0, 120, 50, 1);                             /* a burst on an urchin: a pop, and the pieces scatter */
    ha_tone(&s, 0, 1800, 500, 90, 40, 40);
    for (int i = 0; i < 3; i++) ha_tone(&s, ha_ms(60 + i * 40), 1200 + i * 300, 1600 + i * 300, 40, 18, 20);
    ha_store(&s, SFX_BURST, 220);
    ha_tone(&s, 0, 440, 110, 260, 56, 120);                 /* eaten: a sad falling "wooo" with bubbles */
    ha_tone(&s, ha_ms(40), 660, 160, 200, 22, 100);
    ha_hiss(&s, 0, 160, 26, 3);
    ha_store(&s, SFX_EATEN, 300);
    ha_tone(&s, 0, 330, 494, 70, 30, 40);                   /* the pieces merge: a soft "blub" */
    ha_store(&s, SFX_MERGE, 90);
    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx_at(int id, int x, int vol) { ha_play_slot(id, vol, x, id == SFX_GULP || id == SFX_BURST); }

/* the screen x of player p's view (the sounds' pan) */
static int pan_of(int p)
{
    if (W.players == 1) return RS_SCREEN_W / 2;
    return (p % 2) ? RS_SCREEN_W * 3 / 4 : RS_SCREEN_W / 4;
}

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void touch_reset(void)
{
    touch_stick_slot = -1;
    memset(touch_btn_slot, 0, sizeof touch_btn_slot);
    TV.stick_on = TV.split_on = TV.eject_on = 0;
}

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, hu_players(), seed);
    if (state == ST_PLAY) {
        world_start(&W);
        if (opt_mass > START_MASS)
            for (int i = 0; i < MAX_CELLS; i++)
                if (W.c[i].on && W.c[i].owner < MAX_PLAYERS) {
                    W.c[i].mass = opt_mass * MQ;
                    W.c[i].r = fish_radius(W.c[i].mass);
                    W.own[W.c[i].owner].mass = W.c[i].mass;
                    W.own[W.c[i].owner].best = opt_mass;
                }
    }
    new_best = 0;
    memset(eject_held, 0, sizeof eject_held);
    touch_reset();
    draw_new_run(&W);
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

static void p1_view(vrect *v)
{
    vrect r[4];
    int n = view_layout(W.players, rs_screen_left(), rs_screen_width(), r);
    views_tall(r, n, rs_screen_top(), rs_screen_bottom());     /* a tall screen: the views reach its edges */
    *v = r[0];
}

/* the fingers of player 1: the stick (the steering), the buttons (their presses this frame: in->split, in->eject) */
static void touch_read(pin *in)
{
    vrect v;
    p1_view(&v);
    tbutton bs, be;
    touch_buttons(&v, &bs, &be);
    int ox = rs_screen_origin_x(), oy = rs_screen_origin_y();    /* screen pixels -> stage coordinates */
    TV.split_on = TV.eject_on = 0;
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
            /* a button presses as the finger goes down (not again for one still down after a pause or a load) */
            if (touch_on_button(&bs, x0, y0)) {
                if (t->pressed) in->split = 1;
                touch_btn_slot[i] = 1;
            } else if (touch_on_button(&be, x0, y0)) {
                if (t->pressed) in->eject = 1;
                touch_btn_slot[i] = 2;
            } else if (touch_stick_slot < 0 && touch_stick_zone(&v, x0, y0)) {
                touch_stick_slot = i;
                TV.sx0 = t->x0;
                TV.sy0 = t->y0;
            }
        }
        if (touch_btn_slot[i] == 1) TV.split_on = 1;
        if (touch_btn_slot[i] == 2) TV.eject_on = 1;
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
        in->any = 1;
    }
    if (in->split || in->eject) in->any = 1;
}

/* the bot that gives up (botstop): it swims into the nearest fish that could eat it */
static pin bot_give_up(int p)
{
    pin in = {0, 0, 0, 0, 1, 0};
    const owner *ow = &W.own[p];
    int32_t mine = 0;
    for (int i = 0; i < MAX_CELLS; i++)
        if (W.c[i].on && W.c[i].owner == p && W.c[i].mass > mine) mine = W.c[i].mass;
    int best = -1;
    int64_t bd = 0;
    for (int i = 0; i < MAX_CELLS; i++) {
        const cell *c = &W.c[i];
        if (!c->on || c->owner == p || !can_eat(c->mass, mine)) continue;
        int64_t dx = (c->x - ow->cx) / FX, dy = (c->y - ow->cy) / FX, d = dx * dx + dy * dy;
        if (best < 0 || d < bd) { best = i; bd = d; }
    }
    if (best < 0) return bot_think(&W, p, opt_botskill, &in), in;
    int64_t dx = W.c[best].x - ow->cx, dy = W.c[best].y - ow->cy, d = isqrt64(dx * dx + dy * dy);
    if (d > 0) {
        in.dx = (int16_t)(dx * 256 / d);
        in.dy = (int16_t)(dy * 256 / d);
    }
    in.any = 1;
    return in;
}

static pin player_input(int p)
{
    pin in = {0, 0, 0, 0, 0, 0};
    if (p == 0 && start_press) in.any = 1;
    if (is_bot(p)) {
        pin b;
        if (opt_botstop && W.own[p].best >= opt_botstop + 50 * p && W.started) b = bot_give_up(p);
        else bot_think(&W, p, opt_botskill, &b);
        b.any = 1;
        return b;
    }
    int pad = hu_player_pad(p);
    uint16_t held = rs_pad(pad), pr = rs_pad_pressed(pad);
    int dx = (held & RS_BTN_RIGHT ? 1 : 0) - (held & RS_BTN_LEFT ? 1 : 0);
    int dy = (held & RS_BTN_DOWN ? 1 : 0) - (held & RS_BTN_UP ? 1 : 0);
    in.dx = (int16_t)(dx * (dy ? 181 : 256));
    in.dy = (int16_t)(dy * (dx ? 181 : 256));
    in.split = (pr & RS_BTN_A) != 0;
    if (held & RS_BTN_B) {
        eject_held[p]++;
        in.eject = (pr & RS_BTN_B) || (eject_held[p] >= 2 * EJECT_REPEAT && eject_held[p] % EJECT_REPEAT == 0);
    } else {
        eject_held[p] = 0;
    }
    if (held & (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y | HU_IN_DPAD)) in.any = 1;
    if (p == 0) touch_read(&in);
    if (p == 0 && start_press) in.split = in.eject = 0;    /* the start press only starts */
    return in;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int best_now = 0;
    for (int p = 0; p < W.players; p++)
        if (W.own[p].best > best_now) best_now = W.own[p].best;
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.own[0].best, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (best_now > run_best) { run_best = best_now; new_best = 1; }
    if ((uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
    if (W.players >= 2 && (uint32_t)best_now > SV.best_multi) SV.best_multi = (uint32_t)best_now;
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: mass %d (best %u)", runs_done, rs_frame_count(), W.own[0].best, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void hash_world(void)
{
    for (int p = 0; p < W.players; p++) {
        const owner *ow = &W.own[p];
        state_hash = (state_hash ^ (uint32_t)ow->cx) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)ow->cy) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(ow->mass + ow->ncells * 7 + ow->out * 131)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)(W.eats * 31 + W.splits * 7 + W.bursts * 3 + W.merges + W.t)) * 16777619u;
}

static void play_update(void)
{
    pin in[MAX_PLAYERS];
    for (int p = 0; p < MAX_PLAYERS; p++) {
        pin none = {0, 0, 0, 0, 0, 0};
        in[p] = p < W.players && W.own[p].alive ? player_input(p) : none;
    }
    start_press = 0;
    world_step(&W, in);
    nom_t++;
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p], x = pan_of(p);
        if ((ev & (EV_PLANKTON | EV_PELLET)) && nom_t >= 4) {
            sfx_at(SFX_NOM, x, 40);
            nom_t = 0;
        }
        if (ev & EV_EAT) sfx_at(SFX_GULP, x, HA_VOL_FEEDBACK);
        if (ev & EV_SPLIT) sfx_at(SFX_SPLIT, x, HA_VOL_ACTION);
        if (ev & EV_EJECT) sfx_at(SFX_EJECT, x, 60);
        if (ev & EV_MERGE) sfx_at(SFX_MERGE, x, 50);
        if (ev & EV_TOP) ha_play(HA_DING);
        if (ev & EV_BURST) {
            sfx_at(SFX_BURST, x, HA_VOL_CRASH);
            hu_shake(2, 10);
            rs_log("player %d burst on an urchin at frame %u (mass %d)", p + 1, rs_frame_count(), owner_mass(&W, p));
        }
        if (ev & EV_OUT) {
            sfx_at(SFX_EATEN, x, HA_VOL_CRASH);
            hu_shake(3, 12);                    /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d eaten at frame %u (best %d) by %d", p + 1, rs_frame_count(), W.own[p].best,
                   W.own[p].eaten_by);
        }
    }
    hash_world();
    if (!W.started) return;                     /* ready=1: the run waits for its first press */
    if (st == ST_PLAY && !world_running(&W)) go(ST_DEAD);
    if (st == ST_DEAD && st_t >= PANEL_DELAY) game_over();
}

/* the title: players join and leave (the kit's lobby), P1 starts (A, a start input, or a tap); the pond goes on */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) ha_play(HA_CONFIRM);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) W.players = hu_players();
    if ((ev & HU_TITLE_START) || touch_lifted(st_t) || (opt_bot && st_t >= BOT_START)) {
        ha_play(HA_CONFIRM);
        new_run(ST_PLAY);
        start_press = 1;                        /* the run starts at once */
        play_update();
        return;
    }
    pin none[MAX_PLAYERS];
    memset(none, 0, sizeof none);
    world_step(&W, none);
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
        pin none[MAX_PLAYERS];
        memset(none, 0, sizeof none);
        world_step(&W, none);                   /* the pond goes on behind the panel */
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
    rs_screen_tall(RS_SCREEN_H_MAX);      /* a phone held upright: more of the pond above and below (DESIGN.md) */
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; B, Start and the d-pad too), up to 4 players: PRESS A TO SWIM */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_mass = rs_option_int("mass", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[64] = "", ms[64] = "", os[32] = "";
    for (int p = 0; p < W.players; p++) {
        size_t a = strlen(sc), b = strlen(ms), c = strlen(os);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.own[p].best);
        snprintf(ms + b, sizeof ms - b, "%s%d", p ? "," : "", owner_mass(&W, p));
        snprintf(os + c, sizeof os - c, "%s%d", p ? "," : "", W.own[p].out);
    }
    const owner *h = &W.own[0];
    rs_log("state: st=%d players=%d score=%d mass=%d cells=%d best=%u runs=%d out=%s x=%d y=%d t=%d rank=%d "
           "splits=%d bursts=%d merges=%d eats=%d kills=%d paused=%d touch=%d hash=%08x scores=%s masses=%s",
           st, W.players, h->best, owner_mass(&W, 0), h->ncells, SV.best, runs_done, os, h->cx / FX, h->cy / FX, W.t,
           owner_rank(&W, 0), W.splits, W.bursts, W.merges, W.eats, h->kills, paused, TV.seen, state_hash, sc, ms);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press); S(eject_held); S(nom_t);
    S(TV); S(touch_stick_slot); S(touch_btn_slot); S(state_hash);
    S(opt_bot); S(opt_botstop); S(opt_botruns); S(opt_botskill); S(opt_seed_fixed); S(opt_seed); S(opt_music);
    S(opt_sound); S(opt_mass);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Fishy Feast", "fishyfeast", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
