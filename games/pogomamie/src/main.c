/*
 * Pogo Mamie: the game flow (title, get ready, play, the fall, game over), input, save RAM, options, sound and
 * effect events, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 *
 * Controls: Left/Right steer in the air, hold A (or B, X, Y) on a landing for a big bounce, Start pauses.
 * A or Start on the title and "get ready" screens starts; A on pad 2 there: Papi joins (a 2-player race).
 * Touch (rs_game.touch_native; player 1, DESIGN.md "Controls"): hold the left / right half of the screen to steer
 * (= Left / Right held); a swipe up arms the big bounce for the next landing (= A held until then; a glint on her
 * pogo while it is armed); a tap starts on the title and "get ready" and goes back to the title from the game-over
 * panel (= A there).
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=1          the screen-reading bot plays player 1 (bot.c), bot=2 both players; botruns=N runs in a row
 *   botstop=M      the bot gives up at M metres (screenshots of a fall and the panel)
 *   seed=N         a fixed course (default: from the frame the run starts)
 *   skip=M         start M metres along (skip=512 the Seine, 1024 Haussmann, 1536 the Eiffel Tower, 2048 night)
 *   players=2      start with Papi joined;  ready=1  skip the title (straight to "get ready")
 *   dump=1         log the final state at exit (tests; dump=2: and the buildings); music=0, sound=0
 *   evlog=1        log the big bounces, pigeons, falls, power-ups (screenshots); give=1 umbrella, 3 yarn, 4 croissant
 */
#include "pm.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

#define GO_BUTTONS (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y)
#define START_INPUTS (GO_BUTTONS | RS_BTN_START | HU_IN_DPAD)    /* start: A or the D-pad (the house table) */

int opt_bot;
static int opt_botruns, opt_botstop, opt_skip, opt_seed_fixed, opt_evlog, opt_give;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done, players = 1;
static uint32_t state_hash = 2166136261u;
static int run_dists[64], nrun_dists;

/* ---- save RAM ----------------------------------------------------------------------------------------------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint32_t best_m, best_score, best_race, runs;
    uint16_t medals[4];
    uint16_t best_chain, sum;
} save_data;
static save_data SV;

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x504d;
    for (size_t i = 0; i < offsetof(save_data, sum); i++) sum = (uint16_t)(sum * 31 + p[i]);
    return sum;
}

static void save_load(void)
{
    memcpy(&SV, rs_sram(), sizeof SV);
    if (memcmp(SV.magic, "PMAM", 4) || SV.version != 1 || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, "PMAM", 4);
        SV.version = 1;
    }
}

static void save_store(void)
{
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* ---- touch: player 1's fingers as the pad buttons they stand for (no touch: the pad game unchanged) ----------------
 * A finger held on a half steers that way (both halves: they cancel out). A swipe up (rs_touch: 20 px within 24
 * frames, also a flick from a finger held for a while) arms the big bounce: A is held for her until a landing makes
 * the big bounce (the bounce is automatic, its speed is set on the landing frame: armed in the air, it is the next
 * one). The flick must not steer: a finger steers from its 3rd frame only (33 ms: a flick has risen by then) and
 * not while it is above where it went down (more up than sideways: a flick starting) nor once it has swiped up. */
#define TOUCH_STEER_FRAMES 2    /* a new finger steers from this frame on (0 = the frame it goes down) */
#define TOUCH_RISE         4    /* px above where it went down, more up than sideways: rising, no steering */
static int touch_armed;         /* the big bounce armed by a swipe up (saved: touches are not part of a state) */

static int touch_steers(const rs_touch_point *t)
{
    int up = t->y0 - t->y, side = t->x > t->x0 ? t->x - t->x0 : t->x0 - t->x;
    return t->down && t->frames >= TOUCH_STEER_FRAMES && t->swiped != RS_SWIPE_UP && !(up >= TOUCH_RISE && up > side);
}

static uint16_t touch_buttons(void)
{
    /* the halves of the columns shown (a zoomed screen pans, DESIGN.md "Tall screen"; else the screen's halves) */
    uint16_t b = 0;
    int mid = (rs_screen_visible_x0() + rs_screen_visible_x1()) / 2;
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (t && touch_steers(t)) b |= t->x < mid ? RS_BTN_LEFT : RS_BTN_RIGHT;
    }
    return b;
}

static int touch_swiped_up(void)
{
    for (int i = 0; i < RS_TOUCH_MAX; i++) {
        const rs_touch_point *t = rs_touch(i);
        if (t && t->swipe == RS_SWIPE_UP) return 1;
    }
    return 0;
}

static int touch_tapped(void) { return rs_touch_tap(NULL, NULL); }  /* a tap = A on the title, "get ready", the panel */
int pm_touch_armed(void) { return touch_armed; }                    /* draw.c: the glint on her pogo */

/* ---- flow ---------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static void new_run(int state)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    world_init(&W, players, seed, opt_skip);
    W.fixed_seed = opt_seed_fixed;
    draw_reset(&W);
    bot_reset();
    new_best = 0;
    touch_armed = 0;
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

static void game_over(void)
{
    int best_d = world_dist(&W), best_s = 0;
    for (int p = 0; p < W.players; p++) best_s = W.m[p].score > best_s ? W.m[p].score : best_s;
    new_best = (uint32_t)best_d > SV.best_m;
    /* a skipped start (tests, screenshots) changes nothing, not even the in-memory copy (save states: the battery
     * save is not part of a state, a replay must see the same one) */
    if (!opt_skip) {
        SV.runs++;
        if (W.players == 1) {
            int m = medal_of(W.m[0].dist_m);
            if (m) SV.medals[m - 1]++;
            if (W.m[0].best_chain > SV.best_chain) SV.best_chain = (uint16_t)W.m[0].best_chain;
        }
        if (new_best) SV.best_m = (uint32_t)best_d;
        if ((uint32_t)best_s > SV.best_score) SV.best_score = (uint32_t)best_s;
        if (W.players >= 2 && (uint32_t)best_d > SV.best_race) SV.best_race = (uint32_t)best_d;
        save_store();
    }
    if (nrun_dists < 64) run_dists[nrun_dists++] = W.m[0].dist_m;
    runs_done++;
    rs_log("run %d over at frame %u: %d m, score %d (best %u m)", runs_done, rs_frame_count(), W.m[0].dist_m,
           W.m[0].score, SV.best_m);
    sfx(SFX_JINGLE);
    if (W.players == 1 && medal_of(W.m[0].dist_m)) sfx(SFX_MEDAL);
    go(DS_OVER);
}

static void sounds(int p)
{
    const mamie *m = &W.m[p];
    int ev = W.events[p], x = (int)(m->x >> 16) - world_camx(&W);
    hit h = {0, 0, 0, 0, 0};
    if (ev) fx_event(&W, p, ev, &h);
    if (opt_evlog && (ev & (EV_BIG | EV_PIGEON | EV_DOOMED | EV_DOWN | EV_ITEM | EV_STUNT | EV_SPRING | EV_GLASS | EV_SLING |
                            EV_POT | EV_BREAK | EV_SAVED | EV_KNOCK | EV_STUMBLE)))
        rs_log("ev f=%u p=%d%s%s%s%s%s%s%s%s%s%s%s%s%s%s x=%d item=%d", rs_frame_count(), p, ev & EV_BIG ? " big" : "",
               ev & EV_PIGEON ? " pigeon" : "", ev & EV_DOOMED ? " doomed" : "", ev & EV_DOWN ? " down" : "",
               ev & EV_ITEM ? " item" : "", ev & EV_STUNT ? " stunt" : "", ev & EV_SPRING ? " spring" : "",
               ev & EV_GLASS ? " glass" : "", ev & EV_SLING ? " sling" : "", ev & EV_POT ? " pot" : "",
               ev & EV_BREAK ? " ledge" : "", ev & EV_SAVED ? " saved" : "", ev & EV_KNOCK ? " knock" : "",
               ev & EV_STUMBLE ? " stumble" : "", (int)(m->x >> 16), m->item_got);
    if ((ev & EV_START) && opt_give) {        /* debugging and screenshots: a power-up from the start */
        mamie *mm = &W.m[p];
        if (opt_give == 1) mm->umbrella_t = UMBRELLA_T;
        if (opt_give == 3) mm->yarn = 1;
        if (opt_give == 4) mm->croissant_t = CROISSANT_T;
    }
    if (ev & EV_LAND) {
        if (m->state == MS_SLING) sfx_at(SFX_BOING, x, 0x0d00);
        else if (m->bounce == BN_SPRING) sfx_at(SFX_SPRING, x, 0);
        else if (m->bounce == BN_BIG) sfx_at(SFX_BOING_BIG, x, 0);
        else if (m->land_kind != SF_SKY) sfx_at(SFX_BOING, x, 0x1000 + (m->land_kind == SF_BUMP ? 0x0200 : 0));
    }
    if (ev & EV_SLING) sfx_at(SFX_SPRING, x, 0x0c00);
    if (ev & EV_STUNT) sfx_at(SFX_STUNT, x, 0);
    if (ev & (EV_PIGEON | EV_KNOCK)) sfx_at(SFX_COO, x, 0);
    if (ev & (EV_KNOCK | EV_STUMBLE | EV_BASKET)) hu_shake(2, 8);
    if (ev & EV_BASKET) sfx_at(SFX_CLANG, x, 0x0b00);
    if (ev & EV_RECOVER) sfx_at(SFX_THUMP, x, 0x1400);
    if (ev & EV_STUMBLE) sfx_at(SFX_CLANG, x, 0);
    if (ev & EV_GLASS) sfx_at(SFX_GLASS, x, 0);
    if (ev & (EV_POT | EV_BREAK)) sfx_at(SFX_CRACK, x, 0);
    if (ev & EV_ITEM) {
        sfx_at(SFX_PICKUP, x, 0);
        if (m->item_got == IT_UMBRELLA) sfx_at(SFX_POP, x, 0);
    }
    if (ev & EV_SAVED) sfx_at(SFX_PICKUP, x, 0x0c00);
    if (ev & EV_DOOMED) sfx_at(SFX_WHISTLE, x, 0);
    if ((ev & EV_SPRING) && m->state == MS_FALL) sfx_at(SFX_SPRING, x, 0x0b00);
    if (ev & EV_DOWN) {
        sfx_at(m->down_kind ? SFX_SPLASH : SFX_THUMP, x, 0);
        hu_shake(3, 12);
    }
    if (m->state == MS_DOWN && m->t == 24) sfx_at(SFX_GRUMBLE, x, 0);
    if (ev & EV_OUT) sfx(SFX_SWISH);
    if (ev & EV_START) sfx(SFX_JOIN);
    if (p == 0 && (ev & EV_MEOW)) sfx_at(SFX_MEOW, (int)W.c.x - world_camx(&W), 0);
}

static void play_update(int allow_start)
{
    int dir[MAX_PLAYERS] = {0, 0}, a[MAX_PLAYERS] = {0, 0}, ap[MAX_PLAYERS] = {0, 0};
    for (int p = 0; p < W.players; p++) {
        if (is_bot(p)) {
            bot_decide(p, &dir[p], &a[p]);
            if (opt_botstop && W.m[p].dist_m >= opt_botstop) { dir[p] = 0; a[p] = 0; }   /* it lets go: cruise, no steering */
            ap[p] = allow_start && st_t == 30;
        } else {
            uint16_t b = rs_pad(hu_player_pad(p));
            if (p == 0) {                                               /* touch is player 1 */
                if (st == DS_PLAY && MS_ALIVE(W.m[0].state) && touch_swiped_up()) touch_armed = 1;
                b |= touch_buttons() | (touch_armed ? RS_BTN_A : 0);
            }
            dir[p] = (b & RS_BTN_RIGHT ? 1 : 0) - (b & RS_BTN_LEFT ? 1 : 0);
            a[p] = (b & GO_BUTTONS) != 0;
            ap[p] = allow_start &&
                    ((rs_pad_pressed(hu_player_pad(p)) & START_INPUTS) != 0 || (p == 0 && touch_tapped()));
        }
    }
    world_step(&W, dir, a, ap);
    if (touch_armed && ((W.events[0] & EV_BIG) || !MS_ALIVE(W.m[0].state))) touch_armed = 0;   /* used (or lost) */
    for (int p = 0; p < W.players; p++) {
        sounds(p);
        const mamie *m = &W.m[p];
        state_hash = (state_hash ^ (uint32_t)m->x) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)m->y) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(m->score * 131 + m->state)) * 16777619u;
    }
    state_hash = (state_hash ^ (uint32_t)W.camx ^ (uint32_t)W.camy << 8) * 16777619u;
    if ((st == DS_READY || st == DS_TITLE) && W.started) go(DS_PLAY);
    if (st == DS_PLAY && !world_running(&W)) go(DS_FALL);
    if (st == DS_FALL && world_all_down(&W)) {
        int t = 0, down = 0;
        for (int p = 0; p < W.players; p++)
            if (W.m[p].state == MS_DOWN) { down = 1; if (W.m[p].t > t) t = W.m[p].t; }
        if (t >= PANEL_DELAY || !down) game_over();      /* (2 players both out: at once) */
    }
}

static void game_update(void)
{
    hu_shake_step();
    if ((st == DS_PLAY || st == DS_FALL) && (rs_pad_pressed(0) & (RS_BTN_START | RS_BTN_SELECT))) {
        paused = !paused;
        sfx(SFX_PAUSE);
    }
    if (paused) return;
    int xc = world_camx(&W) + rs_screen_left() + rs_screen_width() / 2;  /* the tune: the district at the screen's centre */
    music_district(district_at(xc), night_at(xc) > 0);
    switch (st) {
    case DS_TITLE: {
        int ev = hu_title_update();
        if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) {
            players = hu_players();
            int t = st_t;
            new_run(DS_TITLE);
            st_t = t;
            sfx(SFX_JOIN);
        }
        if ((ev & HU_TITLE_START) || touch_tapped() || (opt_bot && st_t >= 40)) {
            players = hu_players();
            new_run(DS_PLAY);
            int dir[MAX_PLAYERS] = {0}, held[MAX_PLAYERS] = {0}, press[MAX_PLAYERS] = {1};
            world_step(&W, dir, held, press);
            for (int p = 0; p < W.players; p++) sounds(p);
        } else play_update(0);
        break;
    }
    case DS_READY:
        play_update(1);
        break;
    case DS_PLAY:
    case DS_FALL:
        play_update(0);
        break;
    case DS_OVER:
        if (st_t >= RETRY_LOCK && (hu_over_back() || touch_tapped() ||
            (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10))) {
            players = hu_players();
            new_run(DS_TITLE);
        } else play_update(0);
        break;
    }
    fx_update(&W);
    st_t++;
}

static void game_draw(void)
{
    draw_frame(&W, st, st_t, paused, (int)SV.best_m);
    ui_frame(&W, st, st_t, (int)SV.best_m, (int)SV.best_score, new_best, paused);
}

static void game_init(void)
{
    rs_screen_zoom(ZOOM_H);               /* a phone held upright: zoomed, Paris from the sky to the street (DESIGN.md) */
    save_load();
    audio_set(rs_option_int("music", 1), rs_option_int("sound", 1));
    sfx_init();
    draw_init();
    ui_init();
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    opt_bot = clampi(rs_option_int("bot", 0), 0, MAX_PLAYERS);
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_skip = clampi(rs_option_int("skip", 0), 0, 1000000);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x9090);
    players = clampi(rs_option_int("players", 1), 1, MAX_PLAYERS);
    if (players < opt_bot) players = opt_bot;
    hu_players_set(players);
    opt_evlog = rs_option_int("evlog", 0);
    opt_give = rs_option_int("give", 0);
    new_run(rs_option_int("ready", 0) ? DS_READY : DS_TITLE);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char list[512] = "";
    for (int i = 0; i < nrun_dists; i++) {
        size_t n = strlen(list);
        snprintf(list + n, sizeof list - n, "%s%d", i ? "," : "", run_dists[i]);
    }
    rs_log("state: st=%d players=%d dist=%d dist2=%d score=%d best=%u runs=%d ms=%d,%d x=%d y=%d camx=%d camy=%d "
           "paused=%d landings=%d big=%d pigeons=%d hash=%08x rundists=%s",
           st, W.players, W.m[0].dist_m, W.m[1].dist_m, W.m[0].score, SV.best_m, runs_done, W.m[0].state, W.m[1].state,
           (int)(W.m[0].x >> 16), (int)(W.m[0].y >> 16), world_camx(&W), world_camy(&W), paused, W.m[0].landings,
           W.m[0].big_bounces, W.m[0].pigeons, state_hash, list);
    if (rs_option_int("dump", 0) >= 2)                /* the buildings around (debugging the course) */
        for (int i = 0; i < W.nb; i++)
            rs_log("  building %d: kind %d roof %d x %d..%d top %d", (int)W.b[i].index, W.b[i].kind, W.b[i].roof,
                   (int)W.b[i].x0, (int)W.b[i].x1, W.b[i].top);
}

const world *pm_test_world(int *state)
{
    if (state) *state = st;
    return &W;
}

/* ---- save states: the SDK saves the console; here are the game's own objects (tools/state_audit.py checks that no
 * mutable static is forgotten). Not saved: SV, the battery save's copy. ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(players); S(state_hash); S(run_dists); S(nrun_dists);
    S(touch_armed);
    S(opt_bot); S(opt_botruns); S(opt_botstop); S(opt_skip); S(opt_seed_fixed); S(opt_seed); S(opt_evlog); S(opt_give);
    draw_state();
    ui_state();
    sfx_state();
    bot_state();
}
#undef S

static void game_state_loaded(void)
{
    bot_state_loaded();
    draw_state_loaded();                     /* widescreen: a state saved at another width */
}

const rs_game *rs_game_main(void)
{
    static const rs_game g = {"Pogo Mamie", "pogomamie", "0.1.0", game_init, game_update, game_draw, game_shutdown,
                              pm_assets, game_state, game_state_loaded, 2,
                              RS_SCREEN_W_MAX,    /* widescreen: any width (DESIGN.md "Widescreen") */
                              1};                 /* touch_native: the touch controls above (DESIGN.md "Controls") */
    return &g;
}
