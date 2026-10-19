/*
 * Puffin Plunge: the game flow (the title, the dive from level to level, the zone cards, the shops at each level's
 * bottom and in the side rooms, the results, pause), input (the pad; touch: the app's drawn buttons, games.cfg), the bot,
 * sound, the music, save RAM (the best score, the deepest level, the best combo, the dive under way), options, save states.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu; A dives. A dive is kept in
 * save RAM at each level's start: the title then offers CONTINUE (Left / Right: or a new dive; B: a new dive). A level is
 * a shaft: fall, fire down, land, on to the bottom's floor; there the level's shop (Up / Down or Left / Right choose, A
 * buys, the last line goes on down). The twelfth level ends on the Grumpy Walrus. A lost dive (or a won one) shows the
 * results; one press goes back to the title. Select or Start pauses; paused, B goes back to the title (the dive kept).
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=1          the bot dives (it fires at what is under it, buys in the shops); botlevels=N: it stops after N levels
 *   seed=N         a fixed dive (default: from the frame of the start)
 *   ready=1        skip the title;  level=N  start at level N (1..12, with a build of that depth: tests, screenshots)
 *   god=1          no damage (tests);  frail=1  one heart;  hurtat=N  the dive lost at the level's frame N (tests)
 *   gems=N         gems in the purse;  weapon=N (0..3), passives=N (bits)
 *   shop=1         the level's shop at once;  room=N  a side room's (1 shop, 2 gift) at once
 *   door=1         the puffin starts by the first side room's door (with level=N: a level that has one)
 *   fakerun=1      a dive under way at level 2-1 (in memory);  best=N  a best score (in memory)
 *   touch=1        the hint for touch screens;  dump=1  log the final state at exit;  music=0, sound=0
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

#define ZOOM_H        480           /* a phone held upright: 480 lines zoomed to the display's height (docs/spec.md) */
#define START_INPUTS  (RS_BTN_START | RS_BTN_B | RS_BTN_X | RS_BTN_Y)
#define BOT_START     30
#define DEAD_FRAMES   90            /* the puffin down: the results after this */
#define WIN_FRAMES    170           /* the walrus beaten: the celebration, then the results */
#define CLEAR_FRAMES  40            /* the bottom reached: a breath, then the shop */
#define MENU_LOCK     14            /* frames a shop ignores the buttons (no accidental buy) */
static const int MEDALS[4] = {300, 900, 1800, 3000};

/* ---- the chunks (parsed once at start-up from the asset pack: the same every run, state_audit.txt) ------------- */
static chunk CHUNKS[MAX_CHUNK_DEFS];
static int nchunks;

/* ---- the game's state (saved in states) ----------------------------------------------------------------------- */
static world W;
static int st, st_t, paused, title_sel, shop_sel, shop_kind, menu_lock, new_best, dead_t, end_t, card_t, runs_done;
static int opt_bot, opt_botlevels, opt_god, opt_music, opt_sound, opt_seed_fixed, opt_frail, opt_touch, opt_level;
static int opt_gems, opt_weapon, opt_passives, opt_hurtat, opt_door, levels_done, first_run;
static uint32_t opt_seed, bot_st;
static int music_cur = -1, gem_chain, gem_chain_t, snd_gap[SFX_COUNT];
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */
static char text_buf[1 << 16];                /* start-up scratch (state_audit.txt) */

/* ---- save RAM (house convention: checked, versioned, committed at each change; never part of a state) -------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best;                  /* offset 8 (apps/launcher/games.cfg best=8): the best score (65535 at most) */
    uint8_t best_level, best_combo; /* the deepest level reached (1..12), the longest combo */
    uint32_t dives, wins;
    uint8_t has_run, pad2[3];
    snapshot run;                   /* the dive under way (has_run), as at its level's start */
    uint16_t sum;
} save_data;
static save_data SV;
/* what the title shows: the save RAM as it was at the title's entry (saved in states: a replay must not see newer) */
static save_data PV;
static const char MAGIC[5] = "PUFF";

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
}

static void save_store(void)
{
    if (opt_bot) return;                       /* the bot's dives leave the save RAM as it was (tests) */
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* ---- sound -------------------------------------------------------------------------------------------------------- */
static void sound_init(void)
{
    ha_synth s;
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x9f11u);
    ha_tone(&s, 0, 380, 760, 70, 50, 40);                  /* jump: a springy blip up */
    ha_hiss(&s, 0, 30, 8, 3);
    ha_store(&s, SFX_JUMP, 80);
    ha_tone(&s, 0, 980, 420, 50, 40, 20);                  /* a bubble shot: a soft "pew-blop" */
    ha_hiss(&s, 0, 30, 14, 2);
    ha_store(&s, SFX_SHOT, 60);
    ha_tone(&s, 0, 1600, 900, 160, 40, 80);                /* the beam: a bright zap */
    ha_tone(&s, 0, 800, 450, 160, 26, 80);
    ha_hiss(&s, 0, 120, 16, 1);
    ha_store(&s, SFX_BEAM, 140);
    ha_tone(&s, 0, 520, 380, 30, 30, 15);                  /* a hit: a tick */
    ha_hiss(&s, 0, 20, 10, 2);
    ha_store(&s, SFX_HIT, 40);
    ha_tone(&s, 0, 300, 900, 70, 40, 30);                  /* a critter popped: a bubbly pop */
    ha_hiss(&s, 0, 40, 14, 3);
    ha_store(&s, SFX_POP, 90);
    ha_tone(&s, 0, 240, 620, 90, 54, 40);                  /* a stomp: a boing */
    ha_hiss(&s, 0, 30, 12, 3);
    ha_store(&s, SFX_STOMP, 110);
    ha_tone(&s, 0, 1568, 1568, 50, 30, 40);                /* a gem: a glassy ping (pitched up along a chain) */
    ha_tone(&s, ha_ms(20), 2349, 2349, 60, 16, 40);
    ha_store(&s, SFX_GEM, 100);
    ha_hiss(&s, 0, 50, 22, 3);                             /* landing: a soft scuff */
    ha_tone(&s, 0, 150, 90, 40, 24, 20);
    ha_store(&s, SFX_LAND, 60);
    ha_tone(&s, 0, 320, 120, 140, 58, 60);                 /* hurt: an "oof" */
    ha_hiss(&s, 0, 80, 20, 2);
    ha_store(&s, SFX_HURT, 130);
    ha_hiss(&s, 0, 120, 34, 1);                            /* a crystal shatters */
    ha_tone(&s, 0, 2200, 1400, 90, 22, 40);
    ha_tone(&s, ha_ms(30), 2900, 1800, 70, 16, 30);
    ha_store(&s, SFX_BREAK, 140);
    ha_tone(&s, 0, 200, 160, 40, 30, 20);                  /* no bubbles left: a dry click */
    ha_store(&s, SFX_EMPTY, 50);
    ha_tone(&s, 0, 1046, 1046, 60, 40, 40);                /* a combo: two quick rising notes */
    ha_tone(&s, ha_ms(50), 1568, 1568, 90, 40, 50);
    ha_store(&s, SFX_COMBO, 160);
    ha_tone(&s, 0, 784, 784, 70, 42, 50);                  /* a buy: a coin-like chime */
    ha_tone(&s, ha_ms(60), 1175, 1175, 70, 42, 50);
    ha_tone(&s, ha_ms(120), 1568, 1568, 90, 46, 60);
    ha_store(&s, SFX_BUY, 180);
    ha_tone(&s, 0, 440, 660, 90, 36, 40);                  /* a door: a little knock and a creak */
    ha_hiss(&s, 0, 60, 18, 2);
    ha_store(&s, SFX_DOOR, 110);
    ha_tone(&s, 0, 90, 60, 200, 60, 120);                  /* the walrus: a grumpy low rumble */
    ha_hiss(&s, 0, 200, 24, 4);
    ha_store(&s, SFX_BOSS, 210);
    ha_init(SFX_HOUSE);
    ha_sound_on(opt_sound);
}

static void sfx(int id, int vol, int gap, int x)
{
    if (snd_gap[id]) return;
    snd_gap[id] = gap;
    ha_play_slot(id, vol, x + 64, 0);
}

static void music(int track)
{
    if (!opt_music) {
        if (music_cur >= 0) rs_music_stop();
        music_cur = -1;
        return;
    }
    if (track == music_cur && rs_music_playing()) return;
    char name[32];
    if (track >= ZONES) snprintf(name, sizeof name, "music/boss.mod");
    else snprintf(name, sizeof name, "music/zone%d.mod", track + 1);
    size_t n;
    const void *m = rs_asset(name, &n);
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
    if (p & RS_BTN_DOWN) in |= IN_DOWN;
    if (p & (RS_BTN_A | RS_BTN_B | RS_BTN_X | RS_BTN_Y)) in |= IN_JUMP;
    return in;
}

/* ---- the flow ---------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

static int can_continue(void) { return !opt_bot && PV.has_run && PV.run.level >= 0 && PV.run.level < LEVELS; }

static void run_save(void)
{
    if (opt_bot || W.h.dead || W.won) return;
    world_snapshot(&W, &SV.run);
    SV.has_run = 1;
    save_store();
}

static void enter_level(int level)
{
    world_start_level(&W, level, CHUNKS, nchunks);
    bot_st = 0;
    card_t = CARD_FRAMES;
    draw_level(&W);
    run_save();
    if (level + 1 > PV.best_level) PV.best_level = SV.best_level = (uint8_t)(level + 1);
    go(ST_PLAY);
    rs_log("level %d-%d starts at frame %u: %d chunks, %d critters, %d side rooms", zone_of(level) + 1,
           level % LEVELS_PER_ZONE + 1, rs_frame_count(), W.nchunks, W.n_en, W.ndoors);
    for (int d = 0; d < W.ndoors; d++)
        rs_log("side room %d: row %d, the %s wall, %s", d + 1, W.door_row[d], W.door_col[d] ? "right" : "left",
               W.door_kind[d] ? "a gift" : "a shop");
    if (opt_door && W.ndoors) {                         /* (tests) the puffin on the first side room's ledge */
        W.h.x = (W.door_col[0] ? SHAFT_W - CELL - 8 : CELL + 8) * SUB;
        W.h.y = (W.door_row[0] + 1) * CELL * SUB;
        opt_door = 0;
    }
}

static void start_run(int resume)
{
    uint32_t seed = opt_seed_fixed ? opt_seed : opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u;
    int level = 0;
    if (resume) {
        world_resume(&W, &PV.run);
        level = W.level;
    } else {
        world_new_run(&W, seed);
        level = opt_level;
        if (level > 0) {                                /* (tests) a build of that depth */
            W.hpmax = W.hp = START_HP + level / 3;
            W.ammo_max = START_AMMO + 2 * (level / 3);
            W.gems = 20 * level;
        }
        if (opt_gems) W.gems = opt_gems;
        W.score = rs_option_int("score", 0);         /* (tests) */
        if (opt_weapon) W.weapon = opt_weapon % W_COUNT;
        if (opt_passives) W.passives = opt_passives & 31;
        if (opt_frail) W.hp = W.hpmax = 1;
        PV.dives++;                                     /* (PV: saved in states, a replay sees the same) */
        SV.dives = PV.dives;
        first_run = PV.dives == 1 && !PV.best;
    }
    paused = 0;
    dead_t = end_t = 0;
    levels_done = 0;
    rs_log("dive starts at frame %u: %s, seed %u", rs_frame_count(), resume ? "continued" : "new", W.seed);
    enter_level(level);
}

static void back_to_title(void)
{
    PV = SV;
    paused = 0;
    title_sel = 0;
    go(ST_TITLE);
}

static void run_over(void)
{
    int score = W.score;
    new_best = score > PV.best;
    if (new_best) PV.best = SV.best = (uint16_t)(score > 65535 ? 65535 : score);
    if (W.best_combo > SV.best_combo) SV.best_combo = (uint8_t)(W.best_combo > 255 ? 255 : W.best_combo);
    if (W.won) SV.wins++;
    SV.has_run = 0;
    save_store();
    runs_done++;
    rs_log("dive over at frame %u: %s at level %d, score %d, gems %d, critters %d, best combo %d", rs_frame_count(),
           W.won ? "the walrus beaten" : "the puffin down", W.level + 1, score, W.gems, W.kills, W.best_combo);
    if (W.won) ha_play(HA_MEDAL);
    else ha_play(HA_GAMEOVER);
    go(ST_OVER);
}

static void play_events(void)
{
    uint32_t ev = W.events;
    int x = W.h.x / SUB;
    if (ev & EV_JUMP) sfx(SFX_JUMP, HA_VOL_ACTION, 2, x);
    if (ev & EV_SHOT) sfx(SFX_SHOT, 60, 3, x);
    if (ev & EV_BEAM) sfx(SFX_BEAM, HA_VOL_ACTION, 4, x);
    if (ev & (EV_HIT | EV_BOSSHIT)) sfx(SFX_HIT, 50, 3, x);
    if (ev & EV_KILL) sfx(SFX_POP, 70, 3, x);
    if (ev & EV_STOMP) sfx(SFX_STOMP, HA_VOL_ACTION, 3, x);
    if (ev & EV_GEM) {
        if (!snd_gap[SFX_GEM]) {
            int pitch = RS_PITCH_1 + gem_chain * 140;
            if (pitch > RS_PITCH_1 * 2) pitch = RS_PITCH_1 * 2;
            ha_play_pitched(SFX_GEM, 50, x + 64, pitch, 1);
            snd_gap[SFX_GEM] = 3;
            gem_chain++;
        }
        gem_chain_t = 40;
    }
    if (ev & EV_LAND) sfx(SFX_LAND, 46, 4, x);
    if (ev & (EV_HURT | EV_SHIELD)) sfx(SFX_HURT, HA_VOL_CRASH, 10, x);
    if (ev & EV_BREAK) sfx(SFX_BREAK, 60, 4, x);
    if (ev & EV_EMPTY) sfx(SFX_EMPTY, 50, 8, x);
    if (ev & EV_COMBO) {
        int pitch = RS_PITCH_1 + W.combo * 100;
        ha_play_pitched(SFX_COMBO, 56, x + 64, pitch > RS_PITCH_1 * 2 ? RS_PITCH_1 * 2 : pitch, 0);
    }
    if (ev & EV_BOSS) sfx(SFX_BOSS, HA_VOL_CRASH, 0, x);
    if (ev & EV_ROOM) sfx(SFX_DOOR, HA_VOL_ACTION, 0, x);
    if (ev & EV_CLEAR) ha_play(HA_CONFIRM);
    if (ev & EV_WIN) { ha_play(HA_MEDAL); rs_log("the walrus is beaten at frame %u", rs_frame_count()); }
    if (ev & EV_DEAD) rs_log("the puffin is down at level %d (frame %u, score %d)", W.level + 1, rs_frame_count(), W.score);
}

static void open_shop(int kind)
{
    shop_kind = kind;
    shop_sel = 0;
    menu_lock = MENU_LOCK;
    go(ST_SHOP);
}

static void play_update(void)
{
    if (card_t) card_t--;
    if (W.h.dead && !W.hitstop) {
        if (++dead_t >= DEAD_FRAMES) run_over();
        return;
    }
    if (W.won && !W.hitstop) {
        if (++end_t >= WIN_FRAMES) run_over();
        return;
    }
    if (W.cleared) {
        if (++end_t >= CLEAR_FRAMES) { end_t = 0; open_shop(0); }
        return;
    }
    int in = opt_bot ? bot_input(&W, &bot_st) : pad_input();
    int hp = W.hp;
    world_step(&W, in);
    if (opt_hurtat && W.t == opt_hurtat && !W.h.dead) {   /* (tests) the dive lost at that frame */
        W.hp = 0;
        W.h.dead = 1;
        W.events |= EV_DEAD;
    }
    if (opt_god && W.hp < hp) { W.hp = hp; W.h.dead = 0; W.events &= ~(uint32_t)EV_DEAD; }
    draw_events(&W);
    play_events();
    state_hash = (state_hash ^ world_hash(&W)) * 16777619u;
    if (W.events & EV_CLEAR) {
        levels_done++;
        rs_log("level %d cleared at frame %u: hp %d/%d, gems %d, score %d", W.level + 1, rs_frame_count(), W.hp, W.hpmax,
               W.gems, W.score);
    }
    if (W.room) open_shop(W.door_kind[W.room - 1] ? 2 : 1);
}

static void leave_shop(void)
{
    ha_play(HA_SELECT);
    if (shop_kind) {                                    /* a side room: back in the shaft */
        world_leave_room(&W);
        draw_events(&W);
        go(ST_PLAY);
        return;
    }
    if (opt_bot && opt_botlevels && levels_done >= opt_botlevels) {   /* the bot's levels done: it stops */
        rs_log("the bot stops after %d levels at frame %u", levels_done, rs_frame_count());
        opt_bot = 0;
        back_to_title();
        return;
    }
    enter_level(W.level + 1 < LEVELS ? W.level + 1 : LEVELS - 1);
}

static void shop_update(void)
{
    uint16_t pr = rs_pad_pressed(0);
    int n = W.n_offers + 1;                             /* the offers, then "go on" */
    if (menu_lock) { menu_lock--; return; }
    if (opt_bot) {
        if (st_t < 20) return;
        int k = bot_shop(&W);
        if (k >= 0 && world_buy(&W, k)) { ha_play(HA_MEDAL); rs_log("the bot buys upgrade %d", W.offers[k].up); return; }
        leave_shop();
        return;
    }
    if (pr & (RS_BTN_UP | RS_BTN_LEFT)) { shop_sel = (shop_sel + n - 1) % n; ha_play(HA_SELECT); }
    if (pr & (RS_BTN_DOWN | RS_BTN_RIGHT)) { shop_sel = (shop_sel + 1) % n; ha_play(HA_SELECT); }
    if (pr & (RS_BTN_B | RS_BTN_START)) { leave_shop(); return; }
    if (!(pr & RS_BTN_A)) return;
    if (shop_sel >= W.n_offers) { leave_shop(); return; }
    if (world_buy(&W, shop_sel)) {
        sfx(SFX_BUY, HA_VOL_ACTION, 0, 96);
        rs_log("upgrade %d bought at frame %u (gems left %d)", W.offers[shop_sel].up, rs_frame_count(), W.gems);
        int left = 0;
        for (int i = 0; i < W.n_offers; i++) left += !W.offers[i].sold;
        if (!left) shop_sel = W.n_offers;               /* all sold: the cursor on "go on" */
    } else {
        ha_play(HA_PAUSE);
    }
}

static void title_update(void)
{
    int cont = can_continue();
    hu_title_cfg tc = {START_INPUTS, 1, cont && title_sel == 0 ? TR(VERB_CONTINUE) : TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    int ev = hu_title_update();
    uint16_t pr = rs_pad_pressed(0);
    if (cont && (pr & (RS_BTN_LEFT | RS_BTN_RIGHT | RS_BTN_UP | RS_BTN_DOWN))) { title_sel ^= 1; ha_play(HA_SELECT); }
    int start = (ev & HU_TITLE_START) || (opt_bot && st_t >= BOT_START);
    if (cont && (pr & RS_BTN_B)) { title_sel = 1; start = 1; }    /* B: a new dive (the one under way dropped) */
    if (!start) return;
    ha_play(HA_CONFIRM);
    if (cont && title_sel == 0) start_run(1);
    else { PV.has_run = SV.has_run = 0; start_run(0); }
}

static void game_update(void)
{
    for (int i = 0; i < SFX_COUNT; i++) if (snd_gap[i]) snd_gap[i]--;
    if (gem_chain_t && !--gem_chain_t) gem_chain = 0;
    int boss_on = W.boss >= 0 && W.boss < W.n_en && W.en[W.boss].alive && W.en[W.boss].st > 0;
    music(st == ST_TITLE ? 0 : boss_on ? ZONES : zone_of(W.level));
    if (st == ST_PLAY && (rs_pad_pressed(0) & (RS_BTN_SELECT | RS_BTN_START))) {
        paused = !paused;
        ha_play(HA_PAUSE);
    } else if (st == ST_PLAY && paused && (rs_pad_pressed(0) & RS_BTN_B)) {
        ha_play(HA_SELECT);
        back_to_title();                                /* the dive kept (saved at its level's start) */
        return;
    } else if (st == ST_PLAY && paused && (rs_pad_pressed(0) & RS_BTN_A)) {
        paused = 0;
        ha_play(HA_PAUSE);
    }
    if (paused) return;
    hu_shake_step();
    switch (st) {
    case ST_TITLE: title_update(); break;
    case ST_PLAY: play_update(); break;
    case ST_SHOP: shop_update(); break;
    case ST_OVER: {
        int back = st_t >= RETRY_LOCK && hu_over_back();
        if (opt_bot && st_t == RETRY_LOCK + 40) back = 1;
        if (back) {
            ha_play(HA_SELECT);
            back_to_title();
            if (opt_bot) opt_bot = 0;                  /* the bot's dive done: it stops on the title */
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
    v.state = st == ST_PLAY && card_t ? ST_PLAY : st;
    v.st_t = st_t;
    v.paused = paused;
    v.best = PV.best;
    v.new_best = new_best;
    v.can_continue = can_continue();
    v.run_level = PV.run.level;
    v.title_sel = title_sel;
    v.medal = hu_medal_of(W.score, MEDALS);
    v.shop_sel = shop_sel;
    v.shop_kind = shop_kind;
    v.card = st == ST_PLAY ? card_t : 0;
    v.first_run = first_run;
    v.bot = opt_bot;
    v.touch = opt_touch;
    draw_frame(&W, &v);
}

static void chunks_load(void)
{
    size_t n;
    const char *p = rs_asset("levels/chunks.txt", &n);
    char err[200];
    if (!p) { rs_log("levels/chunks.txt missing"); nchunks = 0; return; }
    if (n >= sizeof text_buf) n = sizeof text_buf - 1;
    memcpy(text_buf, p, n);
    text_buf[n] = 0;
    nchunks = chunks_parse(text_buf, CHUNKS, MAX_CHUNK_DEFS, err, sizeof err);
    if (nchunks < 0) { rs_log("levels/chunks.txt: %s", err); nchunks = 0; }
}

static void game_init(void)
{
    rs_screen_zoom(ZOOM_H);                     /* a phone held upright: the shaft zoomed, more of it below the puffin */
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    opt_bot = rs_option_int("bot", 0);
    opt_botlevels = rs_option_int("botlevels", 0);
    opt_god = rs_option_int("god", 0);
    opt_frail = rs_option_int("frail", 0);
    opt_touch = rs_option_int("touch", 0);
    opt_level = rs_option_int("level", 1) - 1;
    if (opt_level < 0 || opt_level >= LEVELS) opt_level = 0;
    opt_gems = rs_option_int("gems", 0);
    opt_hurtat = rs_option_int("hurtat", 0);
    opt_door = rs_option_int("door", 0);
    opt_weapon = rs_option_int("weapon", 0);
    opt_passives = rs_option_int("passives", 0);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0x5eed);
    if (rs_option("best")) SV.best = (uint16_t)rs_option_int("best", 0);
    if (rs_option_int("fakerun", 0)) {          /* (tests, screenshots) a dive under way at level 2-1, in memory only */
        world tmp;
        world_new_run(&tmp, 1234);
        tmp.level = 3;
        tmp.gems = 57;
        tmp.score = 812;
        tmp.hpmax = 5;
        tmp.hp = 3;
        world_snapshot(&tmp, &SV.run);
        SV.has_run = 1;
    }
    sound_init();
    chunks_load();
    draw_init();
    hu_title_cfg tc = {START_INPUTS, 1, TR(VERB), NULL, 0};
    hu_title_setup(&tc);
    hu_players_set(1);
    PV = SV;
    world_new_run(&W, 1);
    world_start_level(&W, 0, CHUNKS, nchunks);
    draw_level(&W);
    go(ST_TITLE);
    if (rs_option_int("ready", 0)) start_run(0);
    int shop = rs_option_int("shop", 0), room = rs_option_int("room", 0);
    if (shop && st == ST_PLAY) { world_make_offers(&W, MAX_OFFER, 0); open_shop(0); }
    if (room && st == ST_PLAY) { W.room = 1; world_make_offers(&W, 2, room == 2 ? 2 : 1); open_shop(room == 2 ? 2 : 1); }
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    long vram = (long)rs_tiles_used() * 32;
    for (int l = 0; l < RS_BG_COUNT; l++)
        vram += (long)(rs_screen_width() > 320 && rs_bg_map_w(l) > 64 ? 64 : rs_bg_map_w(l)) * rs_bg_map_h(l) * 2;
    rs_log("vram %ld bytes (tiles %d)", vram, rs_tiles_used());
    rs_log("state: st=%d level=%d hp=%d/%d ammo=%d gems=%d score=%d kills=%d combo=%d depth=%d dead=%d won=%d paused=%d "
           "weapon=%d passives=%d best=%d dives=%u runs=%d hash=%08x",
           st, W.level + 1, W.hp, W.hpmax, W.ammo, W.gems, W.score, W.kills, W.best_combo, W.depth_px, W.h.dead, W.won,
           paused, W.weapon, W.passives, SV.best, SV.dives, runs_done, state_hash);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(title_sel); S(shop_sel); S(shop_kind); S(menu_lock); S(new_best); S(dead_t);
    S(end_t); S(card_t); S(runs_done); S(opt_bot); S(opt_botlevels); S(opt_god); S(opt_music); S(opt_sound);
    S(opt_seed_fixed); S(opt_frail); S(opt_touch); S(opt_level); S(opt_gems); S(opt_weapon); S(opt_passives); S(opt_hurtat); S(opt_door);
    S(levels_done); S(first_run); S(opt_seed); S(bot_st); S(music_cur); S(gem_chain); S(gem_chain_t); S(snd_gap);
    S(state_hash); S(PV);
    draw_state();
    hu_state();
    ha_state();
}
#undef S

static void game_state_loaded(void) { draw_state_loaded(&W); }

const rs_game *rs_game_main(void)
{
    /* widescreen: any width (more of the rock beside the shaft) */
    static const rs_game g = {"Puffin Plunge", "puffinplunge", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, pp_assets, game_state, game_state_loaded, 1, RS_SCREEN_W_MAX, 0};
    return &g;
}
