/*
 * Prickle Patch: the game flow (the title with its gardeners and the shed, the run, the level-ups, the chests, the
 * results, pause), input (the pad, the floating touch stick, the bot), sound, the music, save RAM (the best run, the
 * coins, the boosts, the gardeners bought, the run left half-way), options, save states.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Left / Right choose the
 * gardener (a locked one: A buys it with coins), Up / Down the garden (once the second is open), A starts the run, X
 * (or Y, Select) opens the garden shed (coins buy boosts). A run left from the pause is kept in save RAM: the title
 * then says IN PROGRESS and A continues it, B starts a new one. A level-up pauses the garden on three choices (Up /
 * Down and A, or a tap); a chest shows what it gave (A). The results go back to the title with one press.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=1          the bot plays (it steers away from the pests, toward the gems; random choices); botstop=S: it stands
 *                  still from second S (a test of the game over)
 *   seed=N         a fixed run (default: from the frame of the start)
 *   ready=1        skip the title;  chr=N (0 hedgehog, 1 tortoise, 2 hen), map=N (0, 1)
 *   time=S         the run starts at second S with a build of that time (tests, screenshots, the frame cost)
 *   god=1          the gardener takes no damage (tests);  frail=1  1 health (the game over, quickly)
 *   coins=N, unlock=1, boosts=N, best=S    the save RAM's progress, in memory only (tests, screenshots)
 *   fakerun=1      a run in progress at 7:32 (in memory);  shed=1  the title opens the shed
 *   levelup=1      a level-up at the run's start;  chest=1  a chest at the run's start
 *   touch=1        the hint for touch screens (a phone)
 *   dump=1         log the final state at exit;  music=0, sound=0
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_tall.h"
#include "house_audio.h"
#include "game_text.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define ZOOM_H        440           /* a phone held upright: 440 lines zoomed to the display's height (docs/spec.md) */
#define START_INPUTS  (RS_BTN_START)
#define BOT_START     30
#define DEAD_FRAMES   80            /* the gardener down: the results after this */
#define SNAP_EVERY    (30 * SEC)    /* the run in progress saved this often (and when left from the pause) */
#define STICK_DEAD    4             /* px: the touch stick's dead zone ... */
#define STICK_FULL    22            /* ... and full speed this far from its centre ... */
#define STICK_MAX     30            /* ... which follows the finger past this */

static const int CHAR_COST[NCHAR] = {0, 250, 500};
static const int16_t SHED_BASE[B_COUNT] = {60, 50, 60, 40, 70, 80};

/* ---- the game's state (saved in states) ----------------------------------------------------------------------- */
static world W;
static int st, st_t, paused, sel, chr, map, shed_sel, new_best, earned, runs_done, dead_t, snap_t;
static int opt_bot, opt_botstop, opt_god, opt_music, opt_sound, opt_seed_fixed, opt_frail, opt_touch, opt_time;
static int opt_levelup, opt_chest;
static uint32_t opt_seed;
static int music_cur = -1, gem_combo, gem_combo_t, snd_gap[SFX_COUNT];
static int stick, stick_x0, stick_y0, stick_x, stick_y, menu_lock, first_run;
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM (house convention: checked, versioned, committed at each change; never part of a state) -------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best_secs;             /* offset 8 (apps/launcher/games.cfg best=8): the longest run, seconds */
    uint16_t best_kills;
    uint32_t coins;
    uint8_t unlocked;               /* bits: the gardeners bought (the hedgehog always) */
    uint8_t boosts[B_COUNT];
    uint8_t maps_open, last_chr, last_map, has_run, pad2;
    uint32_t runs, wins, total_kills;
    uint16_t best_level, pad3;
    snapshot run;                   /* the run in progress (has_run) */
    uint16_t sum;
} save_data;
static save_data SV;
/* what the title shows: the save RAM as it was at the title's entry (saved in states: a replay must not see newer) */
static save_data PV;
static const char MAGIC[5] = "PRKL";

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
        SV.unlocked = 1;
        SV.maps_open = 1;
    }
    SV.unlocked |= 1;
    if (!SV.maps_open) SV.maps_open = 1;
}

static void save_store(void)
{
    if (opt_bot) return;                       /* the bot's runs leave the save RAM as it was (tests) */
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* the progress changed in PV (the copy in the state: a replay sees the same) goes to the save RAM; the run in progress
 * there stays (run_save writes it) */
static void commit(void)
{
    snapshot run = SV.run;
    uint8_t has = SV.has_run;
    SV = PV;
    SV.run = run;
    SV.has_run = has;
    save_store();
}

int char_cost(int c) { return c >= 0 && c < NCHAR ? CHAR_COST[c] : 0; }
int shed_cost(int b, int level) { return SHED_BASE[b % B_COUNT] * (level + 1); }

/* ---- sound -------------------------------------------------------------------------------------------------------- */
static void sound_init(void)
{
    ha_synth s;
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x9ac4u);
    ha_tone(&s, 0, 900, 600, 40, 26, 20);                  /* a seed: a soft pop */
    ha_store(&s, SFX_SHOT, 50);
    ha_tone(&s, 0, 520, 380, 30, 30, 15);                  /* a hit: a tick */
    ha_hiss(&s, 0, 20, 10, 2);
    ha_store(&s, SFX_HIT, 40);
    ha_tone(&s, 0, 300, 900, 60, 34, 30);                  /* a pest popped: a bubbly pop */
    ha_hiss(&s, 0, 40, 12, 3);
    ha_store(&s, SFX_POP, 80);
    ha_tone(&s, 0, 1568, 1568, 50, 30, 40);                /* a gem: a glassy ping (pitched up by the combo) */
    ha_tone(&s, ha_ms(20), 2349, 2349, 60, 16, 40);
    ha_store(&s, SFX_GEM, 100);
    ha_tone(&s, 0, 784, 784, 80, 44, 60);                  /* a level up: a chime, G B D G */
    ha_tone(&s, ha_ms(70), 988, 988, 80, 44, 60);
    ha_tone(&s, ha_ms(140), 1175, 1175, 80, 44, 60);
    ha_tone(&s, ha_ms(180), 1568, 1568, 70, 48, 50);
    ha_store(&s, SFX_LEVEL, 240);
    ha_tone(&s, 0, 300, 140, 100, 56, 50);                 /* hurt: an "oof" */
    ha_hiss(&s, 0, 60, 18, 2);
    ha_store(&s, SFX_HURT, 110);
    ha_hiss(&s, 0, 100, 30, 0);                            /* the splash */
    ha_tone(&s, 0, 1200, 500, 90, 14, 40);
    ha_store(&s, SFX_SPLASH, 110);
    ha_hiss(&s, 0, 150, 44, 2);                            /* a pumpkin's burst */
    ha_tone(&s, 0, 160, 60, 140, 56, 60);
    ha_store(&s, SFX_BOOM, 160);
    ha_tone(&s, 0, 523, 523, 90, 40, 60);                  /* a chest: a fanfare */
    ha_tone(&s, ha_ms(66), 659, 659, 70, 40, 50);
    ha_tone(&s, ha_ms(133), 784, 784, 70, 40, 50);
    ha_tone(&s, ha_ms(200), 1046, 1046, 60, 48, 40);
    ha_store(&s, SFX_CHEST, 250);
    ha_tone(&s, 0, 90, 70, 250, 60, 130);                  /* a boss comes: a low rumble */
    ha_hiss(&s, 0, 250, 24, 4);
    ha_store(&s, SFX_BOSS, 260);
    ha_hiss(&s, 0, 70, 20, 1);                             /* a rake's or a mower's whoosh */
    ha_tone(&s, 0, 500, 300, 70, 14, 40);
    ha_store(&s, SFX_RAKE, 80);
    ha_init(SFX_HOUSE);
    ha_sound_on(opt_sound);
}

/* an effect, at most once every few frames (a swarm hit by everything at once stays pleasant) */
static void sfx(int id, int vol, int gap)
{
    if (snd_gap[id]) return;
    snd_gap[id] = gap;
    ha_play_slot(id, vol, 160, 0);
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
    const void *m = rs_asset(track ? "music/boss.mod" : "music/garden.mod", &n);
    if (m && !rs_music_play(m, n, 1)) rs_music_volume(HA_MUSIC_VOL);
    music_cur = track;
}

/* ---- input ---------------------------------------------------------------------------------------------------------- */
static void pad_move(int *ix, int *iy)
{
    uint16_t p = rs_pad(0);
    *ix = ((p & RS_BTN_RIGHT) ? 64 : 0) - ((p & RS_BTN_LEFT) ? 64 : 0);
    *iy = ((p & RS_BTN_DOWN) ? 64 : 0) - ((p & RS_BTN_UP) ? 64 : 0);
}

/* the floating stick: a finger down anywhere is its centre; dragging moves (the farther, the faster) */
static void touch_move(int *ix, int *iy)
{
    const rs_touch_point *t = rs_touch_first();
    if (!t || !t->down) {
        stick = 0;
        return;
    }
    if (!stick || t->pressed) {
        stick = 1;
        stick_x0 = t->x0;
        stick_y0 = t->y0;
    }
    stick_x = t->x;
    stick_y = t->y;
    int dx = stick_x - stick_x0, dy = stick_y - stick_y0, ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int l = ax > ay ? ax + ay * 3 / 8 : ay + ax * 3 / 8;
    if (l > STICK_MAX) {                                /* the centre follows the finger */
        stick_x0 = stick_x - dx * STICK_MAX / l;
        stick_y0 = stick_y - dy * STICK_MAX / l;
        dx = stick_x - stick_x0;
        dy = stick_y - stick_y0;
        l = STICK_MAX;
    }
    if (l <= STICK_DEAD) return;
    int k = l >= STICK_FULL ? 64 : 24 + (l - STICK_DEAD) * 40 / (STICK_FULL - STICK_DEAD);
    *ix = dx * k / l;
    *iy = dy * k / l;
}

/* the bot: away from the pests (the nearer, the stronger), toward the gems and chests, a slow wander */
static void bot_move(const world *w, int *ix, int *iy)
{
    int hx = (int)(w->h.x / FX), hy = (int)(w->h.y / FX);
    long fx = 0, fy = 0;
    for (int i = 0; i < w->n_en; i++) {
        const enemy *e = &w->en[i];
        int dx = hx - (int)(e->x / FX), dy = hy - (int)(e->y / FX);
        int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, d = ax > ay ? ax + ay * 3 / 8 : ay + ax * 3 / 8;
        int reach = e->type >= ET_BOSS0 ? 72 : 80;          /* (a boss: kept at a distance its own size, so the tools reach it) */
        if (d >= reach || d == 0) continue;
        long k = (long)(reach - d) * (reach - d) * (e->type >= ET_BOSS0 ? 4 : 1);
        fx += dx * k / d;
        fy += dy * k / d;
    }
    int danger = fx || fy;
    int best = -1, bd = 170;                            /* the nearest gem (or a chest, an apple) */
    for (int i = 0; i < w->n_gm; i++) {
        int dx = (int)(w->gm[i].x / FX) - hx, dy = (int)(w->gm[i].y / FX) - hy;
        int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, d = ax > ay ? ax + ay * 3 / 8 : ay + ax * 3 / 8;
        if (d < bd) { bd = d; best = i; }
    }
    long gx = 0, gy = 0;
    if (best >= 0) { gx = (int)(w->gm[best].x / FX) - hx; gy = (int)(w->gm[best].y / FX) - hy; }
    for (int i = 0; i < w->n_it; i++) {
        int dx = (int)(w->it[i].x / FX) - hx, dy = (int)(w->it[i].y / FX) - hy;
        if (dx > -200 && dx < 200 && dy > -200 && dy < 200 &&
            (w->it[i].kind == IT_CHEST || (w->it[i].kind == IT_APPLE && w->h.hp < w->h.hpmax / 2))) { gx = dx; gy = dy; }
    }
    long gl = (gx < 0 ? -gx : gx) + (gy < 0 ? -gy : gy);
    long sx = fx * 4, sy = fy * 4;
    if (gl) { sx += gx * 3000 / gl; sy += gy * 3000 / gl; }
    if (!danger && !gl) {                               /* nothing near: a slow circle */
        static const int8_t C[8][2] = {{64, 0}, {45, 45}, {0, 64}, {-45, 45}, {-64, 0}, {-45, -45}, {0, -64}, {45, -45}};
        int k = (int)(w->t / 240) % 8;
        sx = C[k][0];
        sy = C[k][1];
    }
    long l = (sx < 0 ? -sx : sx) + (sy < 0 ? -sy : sy);
    if (!l) { *ix = *iy = 0; return; }
    *ix = (int)(sx * 64 / l);
    *iy = (int)(sy * 64 / l);
    int m = (*ix < 0 ? -*ix : *ix) > (*iy < 0 ? -*iy : *iy) ? (*ix < 0 ? -*ix : *ix) : (*iy < 0 ? -*iy : *iy);
    if (m) { *ix = *ix * 64 / m; *iy = *iy * 64 / m; }
}

/* a tap this frame on one of draw.c's hot zones: its index, -1 none */
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

static int can_continue(void) { return !opt_bot && PV.has_run && PV.run.chr >= 0 && PV.run.chr < NCHAR; }

static void run_save(void)
{
    if (opt_bot || W.dead) return;
    world_snapshot(&W, &SV.run);
    SV.has_run = 1;
    save_store();
}

static void start_run(int resume)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    if (resume) {
        world_resume(&W, &PV.run, PV.boosts);
        chr = W.chr;
    } else {
        world_init(&W, chr, map, PV.boosts, seed);
        PV.has_run = SV.has_run = 0;
        PV.last_chr = (uint8_t)chr;
        PV.last_map = (uint8_t)map;
        first_run = PV.runs == 0;
        commit();
    }
    if (opt_time > 0 && !resume) {                      /* (tests) a run from second S, with a build of that time */
        W.t = opt_time * SEC;
        int levels = opt_time / 18;
        for (int i = 0; i < levels; i++) {
            W.level++;
            world_offer(&W);
            world_choose(&W, (int)(rs_rng_next(&W.rng) % (unsigned)(W.n_offer ? W.n_offer : 1)));
        }
        W.xp_need = xp_for(W.level);
        W.h.hp = W.h.hpmax;
        world_skip_to(&W, opt_time);                    /* the spawner's past events done, its bosses beaten */
    }
    paused = 0;
    dead_t = 0;
    snap_t = 0;
    stick = 0;
    draw_reset_run();
    go(ST_PLAY);
    if (opt_levelup) { W.pending = 1; opt_levelup = 0; }
    if (opt_chest) { W.chest_open = 3; opt_chest = 0; }
    rs_log("run starts at frame %u: %s, gardener %d, garden %d", rs_frame_count(), resume ? "continued" : "new", W.chr,
           W.map);
}

static void back_to_title(void)
{
    PV = SV;
    paused = 0;
    stick = 0;
    go(ST_TITLE);
}

static void run_over(void)
{
    int secs = W.t / SEC;
    earned = W.coins + secs / 12 + W.boss_kills * 25;
    new_best = secs > PV.best_secs;
    if (new_best) PV.best_secs = (uint16_t)(secs > 65535 ? 65535 : secs);
    if (new_best || (secs == PV.best_secs && W.kills > PV.best_kills)) PV.best_kills = (uint16_t)(W.kills > 65535 ? 65535 : W.kills);
    if (W.level > PV.best_level) PV.best_level = (uint16_t)W.level;
    PV.coins += (uint32_t)earned;
    PV.runs++;
    PV.total_kills += (uint32_t)W.kills;
    if (W.won) PV.wins++;
    if (secs >= 600 && PV.maps_open < 2) PV.maps_open = 2;
    PV.has_run = SV.has_run = 0;
    commit();
    runs_done++;
    rs_log("run over at frame %u: %s at %d:%02d, level %d, pests %d, coins +%d", rs_frame_count(),
           W.won ? "past 15:00" : "before 15:00", secs / 60, secs % 60, W.level, W.kills, earned);
    ha_play(HA_GAMEOVER);
    go(ST_OVER);
}

static void play_events(void)
{
    uint32_t ev = W.events;
    if (ev & EV_SHOT) sfx(SFX_SHOT, 34, 6);
    if (ev & EV_HIT) sfx(SFX_HIT, 30, 4);
    if (ev & EV_KILL) sfx(SFX_POP, 44, 4);
    if (ev & EV_GEM) {
        if (!snd_gap[SFX_GEM]) {
            int pitch = RS_PITCH_1 + gem_combo * 160;
            if (pitch > RS_PITCH_1 * 2) pitch = RS_PITCH_1 * 2;
            ha_play_pitched(SFX_GEM, 46, 160, pitch, 1);
            snd_gap[SFX_GEM] = 3;
            gem_combo++;
        }
        gem_combo_t = 40;
    }
    if (ev & EV_LEVEL) sfx(SFX_LEVEL, HA_VOL_ACTION, 0);
    if (ev & EV_HURT) sfx(SFX_HURT, HA_VOL_ACTION, 10);
    if (ev & EV_SPLASH) sfx(SFX_SPLASH, 50, 8);
    if (ev & EV_BOOM) sfx(SFX_BOOM, 60, 6);
    if (ev & (EV_MOWER | EV_RAKE)) sfx(SFX_RAKE, 44, 10);
    if (ev & EV_CHEST) sfx(SFX_CHEST, HA_VOL_ACTION, 0);
    if (ev & EV_BOSS) sfx(SFX_BOSS, HA_VOL_CRASH, 0);
    if (ev & EV_COIN) ha_play_pitched(SFX_GEM, 50, 160, RS_PITCH_1 * 3 / 2, 1);   /* a coin: the ping, higher */
    if (ev & (EV_HEAL | EV_MAGNET)) ha_play(HA_CONFIRM);
    if (ev & EV_WIN) ha_play(HA_MEDAL);                /* the 15:00 milestone */
    if (ev & EV_BOSSKILL) { ha_play_pitched(SFX_BOOM, HA_VOL_CRASH, 160, RS_PITCH_1 * 2 / 3, 0); ha_play(HA_MEDAL); }
    if (ev & EV_ELITE) rs_log("an elite at %d:%02d", W.t / SEC / 60, W.t / SEC % 60);
    if (ev & EV_BOSS) rs_log("boss %d at %d:%02d", W.boss_count, W.t / SEC / 60, W.t / SEC % 60);
    if (ev & EV_BOSSKILL) rs_log("boss chased off at %d:%02d (frame %u)", W.t / SEC / 60, W.t / SEC % 60, rs_frame_count());
}

static void play_update(void)
{
    if (W.hitstop) {                                    /* a boss chased off: the garden holds its breath */
        W.hitstop--;
        return;
    }
    if (W.dead) {
        if (++dead_t >= DEAD_FRAMES) run_over();
        return;
    }
    int ix = 0, iy = 0;
    if (opt_bot) {
        if (!opt_botstop || W.t < opt_botstop * SEC) bot_move(&W, &ix, &iy);
    } else {
        pad_move(&ix, &iy);
        if (!ix && !iy) touch_move(&ix, &iy);
        else stick = 0;
    }
    int hp = W.h.hp;
    world_step(&W, ix, iy);
    if (opt_god) W.h.hp = hp > W.h.hp ? hp : W.h.hp, W.dead = 0;
    if (opt_god && (W.events & EV_DEAD)) W.events &= ~(uint32_t)EV_DEAD;
    draw_events(&W);
    play_events();
    if (W.events & EV_DEAD) rs_log("the gardener is down at %d:%02d (level %d, pests %d)", W.t / SEC / 60,
                                   W.t / SEC % 60, W.level, W.kills);
    if (W.events & EV_WIN) rs_log("the garden is saved (the milestone) at %d:%02d (frame %u)", W.t / SEC / 60, W.t / SEC % 60, rs_frame_count());
    state_hash = (state_hash ^ world_hash(&W)) * 16777619u;
    if (++snap_t >= SNAP_EVERY) { snap_t = 0; run_save(); }
    if (W.chest_open) {
        world_open_chest(&W, W.chest_open);
        menu_lock = MENU_LOCK;
        stick = 0;
        go(ST_CHEST);
        return;
    }
    if (W.pending > 0 && !W.dead) {
        world_offer(&W);
        sel = 0;
        menu_lock = MENU_LOCK;
        stick = 0;
        go(ST_LEVELUP);
    }
}

static void levelup_update(void)
{
    uint16_t pr = rs_pad_pressed(0);
    if (menu_lock) { menu_lock--; return; }
    if (pr & RS_BTN_UP) { sel = (sel + W.n_offer - 1) % W.n_offer; ha_play(HA_SELECT); }
    if (pr & RS_BTN_DOWN) { sel = (sel + 1) % W.n_offer; ha_play(HA_SELECT); }
    int pick = -1;
    if (pr & (RS_BTN_A | RS_BTN_START)) pick = sel;
    int k = tapped(W.n_offer);
    if (k >= 0 && k < W.n_offer) pick = k;
    if (opt_bot && st_t >= 20) pick = (int)(rs_rng_next(&W.rng) % (unsigned)W.n_offer);
    if (pick < 0) return;
    ha_play(HA_CONFIRM);
    world_choose(&W, pick);
    if (W.pending > 0) {                                /* another level waiting */
        world_offer(&W);
        sel = 0;
        menu_lock = MENU_LOCK;
        st_t = 0;
        return;
    }
    go(ST_PLAY);
}

static void chest_update(void)
{
    if (menu_lock) { menu_lock--; return; }
    int ok = (rs_pad_pressed(0) & (RS_BTN_A | RS_BTN_B | RS_BTN_START)) || tapped(HOT_MAX) >= 0 || (opt_bot && st_t >= 40);
    if (!ok) return;
    ha_play(HA_CONFIRM);
    W.n_chest = 0;
    go(ST_PLAY);
}

static void buy_char(void)
{
    if (PV.coins >= (uint32_t)CHAR_COST[chr]) {
        PV.coins -= (uint32_t)CHAR_COST[chr];
        PV.unlocked |= (uint8_t)(1 << chr);
        commit();
        ha_play(HA_MEDAL);
        rs_log("gardener %d bought", chr);
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
    if ((pr & RS_BTN_LEFT) || k == HOT_PREV) { chr = (chr + NCHAR - 1) % NCHAR; ha_play(HA_SELECT); }
    if ((pr & RS_BTN_RIGHT) || k == HOT_NEXT) { chr = (chr + 1) % NCHAR; ha_play(HA_SELECT); }
    if (((pr & (RS_BTN_UP | RS_BTN_DOWN)) || k == HOT_MAP) && PV.maps_open >= 2) { map = !map; ha_play(HA_SELECT); }
    if ((pr & (RS_BTN_X | RS_BTN_Y | RS_BTN_SELECT)) || k == HOT_SHED) {
        ha_play(HA_CONFIRM);
        shed_sel = 0;
        go(ST_SHED);
        return;
    }
    int start = (ev & HU_TITLE_START) || k == HOT_PROMPT || (opt_bot && st_t >= BOT_START);
    if (cont && ((pr & RS_BTN_B) || k == HOT_NEWRUN)) {    /* B: a new run (the one in progress dropped) */
        cont = 0;
        start = 1;
    }
    if (!start) return;
    if (cont) {
        ha_play(HA_CONFIRM);
        start_run(1);
    } else if (PV.unlocked & (1 << chr)) {
        ha_play(HA_CONFIRM);
        start_run(0);
    } else {
        buy_char();
    }
}

static void shed_update(void)
{
    uint16_t pr = rs_pad_pressed(0);
    int k = tapped(HOT_MAX), buy = 0;
    if (pr & RS_BTN_UP) { shed_sel = (shed_sel + B_COUNT - 1) % B_COUNT; ha_play(HA_SELECT); }
    if (pr & RS_BTN_DOWN) { shed_sel = (shed_sel + 1) % B_COUNT; ha_play(HA_SELECT); }
    if (k >= 0 && k < B_COUNT) {
        if (k == shed_sel) buy = 1;
        else { shed_sel = k; ha_play(HA_SELECT); }
    }
    if (pr & RS_BTN_A) buy = 1;
    if ((pr & (RS_BTN_B | RS_BTN_X | RS_BTN_Y | RS_BTN_SELECT | RS_BTN_START)) || k == HOT_MAX - 1 || k == HOT_MAX) {
        ha_play(HA_SELECT);
        back_to_title();
        return;
    }
    if (!buy) return;
    int lv = PV.boosts[shed_sel], cost = shed_cost(shed_sel, lv);
    if (lv < BOOST_MAX && PV.coins >= (uint32_t)cost) {
        PV.coins -= (uint32_t)cost;
        PV.boosts[shed_sel]++;
        commit();
        ha_play(HA_MEDAL);
    } else {
        ha_play(HA_PAUSE);
    }
}

static void game_update(void)
{
    for (int i = 0; i < SFX_COUNT; i++) if (snd_gap[i]) snd_gap[i]--;
    if (gem_combo_t && !--gem_combo_t) gem_combo = 0;
    music(st != ST_TITLE && st != ST_SHED && W.boss >= 0 ? 1 : 0);
    int in_run = st == ST_PLAY || st == ST_LEVELUP || st == ST_CHEST;
    int tap = paused ? tapped(HOT_MAX) : -1;            /* paused, a tap: on the quit line, the title; elsewhere, go on */
    if (in_run && (((rs_pad_pressed(0) & (RS_BTN_SELECT | RS_BTN_START)) && !(st != ST_PLAY && !paused)) ||
                   (tap >= 0 && tap != HOT_QUIT))) {
        paused = !paused;
        ha_play(HA_PAUSE);
    } else if (in_run && paused && ((rs_pad_pressed(0) & RS_BTN_B) || tap == HOT_QUIT)) {
        ha_play(HA_SELECT);
        run_save();                                     /* the run kept: the title offers to continue it */
        back_to_title();
        return;
    }
    if (paused) return;
    hu_shake_step();
    switch (st) {
    case ST_TITLE: title_update(); break;
    case ST_SHED: shed_update(); break;
    case ST_PLAY: play_update(); break;
    case ST_LEVELUP: levelup_update(); break;
    case ST_CHEST: chest_update(); break;
    case ST_OVER: {
        int back = st_t >= RETRY_LOCK && (hu_over_back() || tapped(HOT_MAX) >= 0);
        if (opt_bot && st_t == RETRY_LOCK + 40) back = 1;
        if (back) {
            ha_play(HA_SELECT);
            back_to_title();
            if (opt_bot) opt_bot = 0;                  /* the bot's run done: it stops on the title */
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
    v.sel = sel;
    v.chr = chr;
    v.map = map;
    v.coins = (int)PV.coins;
    v.best_secs = PV.best_secs;
    v.best_kills = PV.best_kills;
    v.can_continue = can_continue();
    v.run_secs = PV.run.t / SEC;
    v.run_level = PV.run.level;
    v.run_chr = PV.run.chr;
    v.unlocked = PV.unlocked;
    v.new_best = new_best;
    v.earned = earned;
    v.shed_sel = shed_sel;
    v.maps_open = PV.maps_open;
    for (int i = 0; i < B_COUNT; i++) v.boosts[i] = PV.boosts[i];
    v.stick = stick && st == ST_PLAY && !paused;
    v.stick_x0 = stick_x0;
    v.stick_y0 = stick_y0;
    v.stick_x = stick_x;
    v.stick_y = stick_y;
    v.first_run = first_run;
    v.bot = opt_touch ? 2 : opt_bot;
    draw_frame(&W, &v);
}

static void game_init(void)
{
    rs_screen_zoom(ZOOM_H);                     /* a phone held upright: the garden zoomed, more of it above and below */
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    opt_bot = rs_option_int("bot", 0);
    opt_botstop = rs_option_int("botstop", 0);
    opt_god = rs_option_int("god", 0);
    opt_frail = rs_option_int("frail", 0);
    opt_touch = rs_option_int("touch", 0);
    opt_time = rs_option_int("time", 0);
    opt_levelup = rs_option_int("levelup", 0);
    opt_chest = rs_option_int("chest", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    if (rs_option("coins")) SV.coins = (uint32_t)rs_option_int("coins", 0);
    if (rs_option_int("unlock", 0)) { SV.unlocked = 7; SV.maps_open = 2; }
    if (rs_option("boosts")) for (int i = 0; i < B_COUNT; i++) SV.boosts[i] = (uint8_t)rs_option_int("boosts", 0);
    if (rs_option("best")) { SV.best_secs = (uint16_t)rs_option_int("best", 0); SV.best_kills = 321; }
    if (rs_option_int("fakerun", 0)) {          /* (tests, screenshots) a run in progress, in memory only */
        world tmp;
        world_init(&tmp, CHR_HEDGEHOG, 0, SV.boosts, 1234);
        tmp.t = (7 * 60 + 32) * SEC;
        tmp.level = 18;
        tmp.kills = 640;
        world_snapshot(&tmp, &SV.run);
        SV.has_run = 1;
    }
    sound_init();
    draw_init();
    hu_title_cfg tc = {START_INPUTS, 1, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    hu_players_set(1);
    chr = SV.last_chr < NCHAR ? SV.last_chr : 0;
    map = SV.last_map && SV.maps_open >= 2 ? 1 : 0;
    if (rs_option("chr")) chr = rs_option_int("chr", 0) % NCHAR;
    if (rs_option("map")) map = rs_option_int("map", 0) ? 1 : 0;
    if (opt_bot && !(SV.unlocked & (1 << chr))) SV.unlocked |= (uint8_t)(1 << chr);
    PV = SV;
    world_init(&W, chr, map, SV.boosts, 1);
    go(ST_TITLE);
    if (rs_option_int("shed", 0)) go(ST_SHED);
    if (rs_option_int("ready", 0)) start_run(0);
    if (opt_frail) { W.h.hpmax = W.h.hp = FX; W.n_wp = 0; }      /* (tests) down at the first touch */
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    long vram = (long)rs_tiles_used() * 32;
    for (int l = 0; l < RS_BG_COUNT; l++)
        if (l != RS_BG3) vram += (long)(rs_screen_width() > 320 && rs_bg_map_w(l) > 64 ? 64 : rs_bg_map_w(l)) * rs_bg_map_h(l) * 2;
    rs_log("vram %ld bytes (tiles %d)", vram, rs_tiles_used());
    rs_log("state: st=%d chr=%d map=%d t=%d level=%d kills=%d hp=%d pests=%d shots=%d gems=%d weapons=%d passives=%d "
           "dead=%d won=%d paused=%d coins=%u best=%d runs=%d soft=%d overflow=%d hash=%08x",
           st, chr, map, W.t, W.level, W.kills, W.h.hp / FX, W.n_en, W.n_sh, W.n_gm, W.n_wp, W.n_pas, W.dead, W.won,
           paused, SV.coins, SV.best_secs, runs_done, soft_used(), soft_overflow(), state_hash);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(sel); S(chr); S(map); S(shed_sel); S(new_best); S(earned); S(runs_done);
    S(dead_t); S(snap_t); S(opt_bot); S(opt_botstop); S(opt_god); S(opt_music); S(opt_sound);
    S(opt_seed_fixed); S(opt_frail); S(opt_touch); S(opt_time); S(opt_levelup); S(opt_chest); S(opt_seed);
    S(music_cur); S(gem_combo); S(gem_combo_t); S(snd_gap); S(stick); S(stick_x0); S(stick_y0); S(stick_x); S(stick_y);
    S(menu_lock); S(first_run); S(state_hash); S(PV);
    draw_state();
    hu_state();
    ha_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* widescreen: any width (more garden); touch_native: the floating stick and the menus' taps */
    static const rs_game g = {"Prickle Patch", "pricklepatch", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, pp_assets, game_state, draw_state_loaded, 1, RS_SCREEN_W_MAX, 1};
    return &g;
}
