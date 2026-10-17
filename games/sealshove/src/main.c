/*
 * Seal Shove: the game flow (the title with players 2-4 joining, the matches, game over, pause), input (pads and
 * touch), sound, save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Pads 2-4 join with A (B
 * leaves); P1 starts the match at once with A (or an extra START INPUT: the d-pad, Start). Always 4 seals on the floe:
 * the players first, bots fill the rest. 1 player: win a match (first to 3 rounds) and the next one comes with smarter
 * bots; lose one and it is game over. 2-4 players: one match, then the results (ranked by rounds won). One press on
 * the game-over panel (hu_over_back) goes BACK TO THE TITLE, with the same players.
 * The pads: the d-pad waddles (momentum: the ice is slippery), A belly-slides (a strong shove, then a short cooldown).
 * Touch (rs_game.touch_native, docs/spec.md "Touch"; player 1, DESIGN.md "Controls"): a virtual stick (a finger down on
 * the left part of the screen, dragged), the round DASH button at the bottom right; a tap starts from the title and
 * leaves the game-over panel. What the fingers set is kept in a registered state (TV, touch_*): a load forgets the
 * fingers, one still down starts again.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the player bot plays players 1..N (bot_think); botruns=N runs; botskill=E a human-like bot (1..10)
 *   botstop=S      a bot-played player walks off the ice every round once its score reaches S (the tests' game over)
 *   botquit=K      players K..4 (bot-played) walk off at once (botquit=1: player 1 too)
 *   seed=N         a fixed match (default: from the frame of the start)
 *   players=N      start with N players joined (1..4);  ready=1  skip the title (the match starts at once)
 *   level=L        the bots' level at the start (1 player: 0, then +1 a match won)
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

static int opt_bot, opt_botstop, opt_botquit, opt_botruns, opt_botskill, opt_seed_fixed, opt_music, opt_sound;
static int opt_level;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int run_best;                          /* the best shown during a run (a replay never reads newer saves) */
static int start_press;                       /* P1's start press on the title: it only starts */
static touch_view TV;                         /* the fingers: the stick, the button (drawn by draw.c) */
static int touch_stick_slot = -1;             /* the finger slot of the stick (-1: none) */
static uint8_t touch_btn_slot[RS_TOUCH_MAX];  /* a finger down on the DASH button */
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: the best score (house convention: checked, versioned, committed at each game over) --------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best, best_multi;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t best_matches, pad2;
    uint16_t sum;
} save_data;
static save_data SV;
static const char MAGIC[5] = "SEAL";

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x5345;
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
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x5ea15ea1u);
    ha_hiss(&s, 0, 170, 34, 2);                             /* a belly slide: a swish on the ice and a "zip" */
    ha_tone(&s, 0, 560, 240, 130, 26, 70);
    ha_store(&s, SFX_DASH, 150);
    ha_tone(&s, 0, 150, 390, 60, 70, 40);                   /* a bump: a rubbery "boing" */
    ha_tone(&s, ha_ms(45), 390, 210, 90, 46, 45);
    ha_store(&s, SFX_BUMP, 120);
    ha_tone(&s, 0, 110, 55, 110, 80, 60);                   /* a slide's smack: a thud and a big boing */
    ha_tone(&s, 0, 190, 560, 80, 64, 50);
    ha_tone(&s, ha_ms(70), 560, 240, 110, 40, 60);
    ha_hiss(&s, 0, 60, 30, 1);
    ha_store(&s, SFX_SMACK, 160);
    ha_tone(&s, 0, 980, 860, 60, 50, 30);                   /* a clash: "bonk-bonk" and the dizzy wobble */
    ha_tone(&s, ha_ms(70), 1240, 1100, 60, 46, 30);
    ha_tone(&s, ha_ms(140), 700, 820, 100, 22, 50);
    ha_store(&s, SFX_CLASH, 180);
    for (int i = 0; i < 6; i++) {                           /* the ice cracks: sharp crackles */
        int at = ha_ms(i * 28 + (i * 7) % 11);
        ha_hiss(&s, at, 14, 46 - i * 4, 0);
        ha_tone(&s, at, 2600 - i * 180, 1800 - i * 150, 12, 14, 6);
    }
    ha_store(&s, SFX_CRACK, 150);
    ha_tone(&s, 0, 95, 48, 220, 70, 120);                   /* a chunk breaks off: a low crunch and a splashy hiss */
    ha_hiss(&s, 0, 240, 52, 1);
    ha_hiss(&s, ha_ms(40), 120, 30, 0);
    ha_store(&s, SFX_BREAK, 190);
    ha_hiss(&s, 0, 270, 64, 1);                             /* a splash: a "sploosh" and a bubbly "bloop" */
    ha_tone(&s, 0, 640, 180, 160, 50, 70);
    ha_tone(&s, ha_ms(120), 400, 900, 60, 24, 30);
    ha_store(&s, SFX_SPLASH, 220);
    ha_tone(&s, 0, 1200, 1700, 35, 34, 18);                 /* a fish lands: two flops */
    ha_tone(&s, ha_ms(90), 1300, 1800, 35, 30, 18);
    ha_store(&s, SFX_FISH, 110);
    static const int up[4] = {1047, 1319, 1568, 2093};      /* a power: a quick climb (C6 E6 G6 C7) */
    for (int i = 0; i < 4; i++) ha_tone(&s, ha_ms(i * 45), up[i], up[i], 70, 40, 40);
    ha_store(&s, SFX_POWER, 180);
    ha_tone(&s, 0, 1760, 1980, 70, 44, 0);                  /* GO!: a whistle */
    ha_tone(&s, ha_ms(60), 1980, 1980, 100, 44, 50);
    ha_store(&s, SFX_GO, 160);

    ha_init(SFX_HOUSE);                                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx_at(int id, int32_t x, int vol) { ha_play_slot(id, vol, (int)(x / FX), id == SFX_SPLASH); }

/* ---- flow ----------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void touch_reset(void)
{
    touch_stick_slot = -1;
    memset(touch_btn_slot, 0, sizeof touch_btn_slot);
    TV.stick_on = TV.dash_on = 0;
}

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    int humans = hu_players();
    world_init(&W, humans, seed, opt_level >= 0 ? opt_level : humans == 1 ? 0 : BOT_LEVEL_MULTI);
    new_best = 0;
    touch_reset();
    draw_new_run();
    go(state);
    if (state == ST_PLAY) rs_log("match %d starts at frame %u (level %d)", W.match_no, rs_frame_count(), W.level);
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

/* the fingers of player 1: the stick (the waddle), the button (its press this frame: in->dash) */
static void touch_read(pin *in)
{
    tbutton b;
    touch_button(vis_right(), &b);              /* (the columns shown: a zoomed screen crops its sides) */
    b.y += rs_screen_bottom() - RS_SCREEN_H;    /* a tall screen: DASH in the screen's bottom-right corner */
    int ox = rs_screen_origin_x(), oy = rs_screen_origin_y();
    TV.dash_on = 0;
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (!t || !t->down) {
            touch_btn_slot[i] = 0;
            if (touch_stick_slot == i) touch_stick_slot = -1;
            continue;
        }
        TV.seen = 1;
        if (t->pressed || (!touch_btn_slot[i] && touch_stick_slot != i)) {
            int x0 = t->x0 - ox, y0 = t->y0 - oy;   /* stage coordinates */
            if (touch_on_button(&b, x0, y0)) {
                /* only a finger going down slides (as A): not one held through a pause or a load */
                if (!touch_btn_slot[i] && t->pressed) in->dash = 1;
                touch_btn_slot[i] = 1;
            } else if (touch_stick_slot < 0 && touch_stick_zone(vis_left(), vis_right() - vis_left(), x0, y0)) {
                touch_stick_slot = i;
                TV.sx0 = t->x0;
                TV.sy0 = t->y0;
            }
        }
        if (touch_btn_slot[i]) TV.dash_on = 1;
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
    if (in->dash) in->any = 1;
}

static pin player_input(int p)
{
    pin in = {0, 0, 0, 0, {0, 0}};
    if (is_bot(p)) {
        int quit = (opt_botquit && p + 1 >= opt_botquit) || (opt_botstop && W.s[p].score >= opt_botstop);
        if (quit) {
            bot_give_up(&W, p, &in);
        } else {
            bot_par bp;
            bot_par_player(opt_botskill, &bp);
            bot_think(&W, p, &bp, &in);
        }
        return in;
    }
    int pad = hu_player_pad(p);
    uint16_t held = rs_pad(pad), pr = rs_pad_pressed(pad);
    int dx = (held & RS_BTN_RIGHT ? 1 : 0) - (held & RS_BTN_LEFT ? 1 : 0);
    int dy = (held & RS_BTN_DOWN ? 1 : 0) - (held & RS_BTN_UP ? 1 : 0);
    in.dx = (int16_t)(dx * (dy ? 181 : 256));
    in.dy = (int16_t)(dy * (dx ? 181 : 256));
    in.dash = (pr & RS_BTN_A) != 0;
    if (held & (RS_BTN_A | HU_IN_DPAD)) in.any = 1;
    if (p == 0) touch_read(&in);
    if (p == 0 && start_press) in.dash = 0;     /* the start press only starts */
    return in;
}

static void game_over(void)
{
    static const int medals[4] = MEDAL_SCORES;
    int best_now = 0;
    for (int p = 0; p < W.humans; p++)
        if (W.s[p].score > best_now) best_now = W.s[p].score;
    SV.runs++;
    int m = W.humans == 1 ? hu_medal_of(W.s[0].score, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (W.humans == 1) {
        if (best_now > run_best) { run_best = best_now; new_best = 1; }
        if ((uint32_t)best_now > SV.best) SV.best = (uint32_t)best_now;
        if (W.s[0].matches > SV.best_matches) SV.best_matches = (uint16_t)W.s[0].matches;
    } else if ((uint32_t)best_now > SV.best_multi) {
        SV.best_multi = (uint32_t)best_now;
    }
    save_store();
    runs_done++;
    rs_log("run %d over at frame %u: score %d, matches won %d, winner %d (best %u)", runs_done, rs_frame_count(),
           W.s[0].score, W.s[0].matches, W.match_winner, SV.best);
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(ST_OVER);
}

static void hash_world(void)
{
    for (int i = 0; i < SEALS; i++) {
        const seal *s = &W.s[i];
        state_hash = (state_hash ^ (uint32_t)s->x) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)s->y) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(s->state + s->wins * 7 + s->score * 131)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)(W.gone + W.round * 31 + W.phase * 7 + W.t)) * 16777619u;
}

static void events(void)
{
    for (int i = 0; i < SEALS; i++) {
        int ev = W.events[i];
        const seal *s = &W.s[i];
        if (ev & EV_DASH) sfx_at(SFX_DASH, s->x, HA_VOL_ACTION);
        if (ev & EV_CLASH) {
            sfx_at(SFX_CLASH, s->x, HA_VOL_CRASH);
            hu_shake(3, 12);
            rs_log("clash at frame %u (seal %d)", rs_frame_count(), i + 1);
        } else if (ev & EV_SMACK) {
            sfx_at(SFX_SMACK, s->x, HA_VOL_CRASH);
            hu_shake(2, 8);
        } else if ((ev & EV_BUMP) && !(W.events[i] & EV_SMACK)) {
            sfx_at(SFX_BUMP, s->x, HA_VOL_FEEDBACK);
        }
        if (ev & EV_SPLASH) {
            sfx_at(SFX_SPLASH, s->x, HA_VOL_CRASH);
            rs_log("seal %d splashes at frame %u (round %d)", i + 1, rs_frame_count(), W.round);
        }
        if (ev & EV_POWER) sfx_at(SFX_POWER, s->x, HA_VOL_FEEDBACK);
        if (ev & EV_DOUBLE) ha_play(HA_DING);
        if (ev & EV_WIN_ROUND) ha_play(HA_DING);
    }
    if (W.gev & GEV_GO) {
        ha_play_slot(SFX_GO, HA_VOL_FEEDBACK, RS_SCREEN_W / 2, 0);
        rs_log("round %d of match %d: GO at frame %u", W.round, W.match_no, rs_frame_count());
    }
    if (W.gev & GEV_CRACK) ha_play_slot(SFX_CRACK, HA_VOL_FEEDBACK, RS_SCREEN_W / 2, 0);
    if (W.gev & GEV_BREAK) {
        ha_play_slot(SFX_BREAK, HA_VOL_ACTION, RS_SCREEN_W / 2, 0);
        hu_shake(1, 6);
    }
    if (W.gev & GEV_FISH) ha_play_slot(SFX_FISH, 60, W.fish.x, 0);
    if (W.gev & GEV_SWEEP) ha_play(HA_DING);
    if ((W.gev & GEV_ROUND_END) && W.round_winner >= 0)
        rs_log("round %d of match %d won by %d at frame %u", W.round, W.match_no, W.round_winner + 1, rs_frame_count());
    else if (W.gev & GEV_ROUND_END)
        rs_log("round %d of match %d: nobody wins, at frame %u", W.round, W.match_no, rs_frame_count());
    if (W.gev & GEV_MATCH_END) {
        ha_play(W.match_winner < W.humans ? HA_MEDAL : HA_THUD);
        rs_log("match %d won by %d at frame %u", W.match_no, W.match_winner + 1, rs_frame_count());
    }
}

static void play_update(void)
{
    pin in[SEALS];
    memset(in, 0, sizeof in);
    for (int p = 0; p < W.humans; p++) in[p] = player_input(p);
    start_press = 0;
    world_step(&W, in);
    events();
    hash_world();
    if (st == ST_PLAY && W.match_over) {
        if (W.humans == 1 && W.match_winner == 0) {
            world_next_match(&W);               /* 1 player: the next match, smarter bots */
            rs_log("match %d starts at frame %u (level %d)", W.match_no, rs_frame_count(), W.level);
        } else {
            go(ST_DEAD);
        }
    }
    if (st == ST_DEAD && st_t >= PANEL_DELAY) game_over();
}

/* the title: players join and leave (the kit's lobby), P1 starts (A, a start input, or a tap) */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) ha_play(HA_CONFIRM);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) W.humans = hu_players();
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
    rs_screen_zoom(ZOOM_H);                   /* a phone held upright: zoomed on the floe (draw.c, DESIGN.md) */
    save_load();
    run_best = (int)SV.best;                    /* then only game_over() raises it (states never read the .srm) */
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    sound_init();
    draw_init();                                /* hu_init (draw.c) */
    /* the title: P1's start inputs (A always; the d-pad and Start too), up to 4 players: PRESS A TO SHOVE */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = rs_option_int("bot", 0);
    if (opt_bot > MAX_PLAYERS) opt_bot = MAX_PLAYERS;
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_botquit = rs_option_int("botquit", 0);
    opt_botskill = rs_option_int("botskill", 0);
    opt_level = rs_option_int("level", -1);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    int n = rs_option_int("players", 1);
    hu_players_set(n > opt_bot ? n : opt_bot > 0 ? opt_bot : 1);
    new_run(rs_option_int("ready", 0) ? ST_PLAY : ST_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char sc[64] = "", wn[32] = "", ss[32] = "";
    for (int p = 0; p < SEALS; p++) {
        size_t a = strlen(sc), b = strlen(wn), c = strlen(ss);
        snprintf(sc + a, sizeof sc - a, "%s%d", p ? "," : "", W.s[p].score);
        snprintf(wn + b, sizeof wn - b, "%s%d", p ? "," : "", W.s[p].wins);
        snprintf(ss + c, sizeof ss - c, "%s%d", p ? "," : "", W.s[p].state);
    }
    int gone = 0;
    for (int c = 0; c < W.nchunks; c++) gone += W.ch[c].state == CH_GONE;
    const seal *s = &W.s[0];
    rs_log("state: st=%d players=%d score=%d best=%u runs=%d match=%d round=%d phase=%d wins=%s states=%s x=%d y=%d "
           "face=%d dashcd=%d power=%d gone=%d dashes=%d bumps=%d clashes=%d falls=%d matches=%d paused=%d touch=%d "
           "hash=%08x scores=%s",
           st, W.humans, s->score, SV.best, runs_done, W.match_no, W.round, W.phase, wn, ss, (int)(s->x / FX),
           (int)(s->y / FX), s->face, s->dash_cd, s->power, gone, W.dashes, W.bumps, W.clashes, W.falls, s->matches,
           paused, TV.seen, state_hash, sc);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(run_best); S(start_press);
    S(TV); S(touch_stick_slot); S(touch_btn_slot); S(state_hash);
    S(opt_bot); S(opt_botstop); S(opt_botquit); S(opt_botruns); S(opt_botskill); S(opt_level); S(opt_seed_fixed);
    S(opt_seed); S(opt_music); S(opt_sound);
    draw_state();
    hu_state();                                 /* the kit, with the title's lobby (who plays, on which pad) */
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: DESIGN.md); touch_native: it reads the touches */
    static const rs_game g = {"Seal Shove", "sealshove", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, gm_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
