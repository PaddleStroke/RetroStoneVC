/*
 * Goat Gorge: the game flow (the title and its chapters, the climb from room to room, the results, pause), input,
 * sound, the music, save RAM (each chapter's best time, fewest deaths and berries), options, save states, the bot.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
 *
 * The house flow (docs/art-direction.md, "Title and players"): the title is the only menu. Left / Right choose the
 * chapter (the next one opens when the one before is climbed), A starts it. A chapter is a chain of one-screen rooms:
 * leaving a room by its open side fades to the next; a death is the room again at once from its start, counted. The
 * last room's exit shows the results (the time, the deaths, the berries, each NEW BEST), one press goes back to the
 * title. Select or Start pauses; paused, B goes back to the title.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=1          the bot climbs: the recorded solutions (levels/solutions.txt), the berries' routes (botberry=0: the
 *                  plain ones); botruns=N chapters one after the other
 *   chapter=N      the chapter on the title (1..5);  room=N  start the chapter at that room (tests, screenshots)
 *   ready=1        skip the title (the chapter starts at once);  unlock=1  every chapter open
 *   god=1          no death (tests);  records=1  some records (in memory: tests, screenshots)
 *   fakerun=1      a climb under way at room 1-3 (in memory);  roomdeaths=N  the first room starts with N deaths
 *   dump=1         log the final state at exit;  music=0, sound=0
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include "house_audio.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>


#define BOT_START    30             /* frames on the title before the bot starts */
#define FADE_FRAMES  8              /* a room's change: out, then in */
#define START_INPUTS (RS_BTN_START | RS_BTN_X)
#define SKIP_DEATHS  15
#define CARD_FRAMES  150            /* the chapter's name shows this long as it starts (2.5 s) */             /* deaths in one room before the pause offers to skip it */

/* ---- the rooms (parsed once at start-up from the asset pack: the same every run, state_audit.txt) -------------- */
static room ROOMS[CHAPTERS][MAX_ROOMS];
static int nrooms[CHAPTERS], nberries[CHAPTERS];
/* the bot's inputs: each room's recorded solutions (exit, berry), frames packed one after the other */
#define SOL_MAX 120000
static uint8_t sol_in[SOL_MAX];
static int sol_off[CHAPTERS][MAX_ROOMS][2], sol_len[CHAPTERS][MAX_ROOMS][2], sol_used;

/* ---- the game's state (saved in states) --------------------------------------------------------------------------- */
static world W;
static int st, st_t, paused, chapter, room_no, sel, fade, fade_next, hud_t;
static int run_time, run_deaths, run_berries, new_time, new_deaths, new_berries, runs_done;
static int room_deaths, run_skips, card_t, opt_room_deaths;   /* card_t: the chapter's title card, frames left */              /* deaths in this room (the skip's offer), rooms skipped */
static int bot_f, opt_bot, opt_botruns, opt_botberry, opt_god, opt_music, opt_sound, opt_unlock, opt_room;
static int music_cur = -1;
static uint32_t state_hash = 2166136261u;
static int16_t snd_buf[HA_RATE / 2];          /* start-up scratch (state_audit.txt) */

/* ---- save RAM: per chapter the best time, the fewest deaths and the berries ever found (house convention: checked,
 * versioned, committed at each chapter's end; never part of a state) ------------------------------------------------ */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t score;                 /* offset 8 (apps/launcher/games.cfg best=8): the progress, for the review prompt */
    uint16_t best_deaths[CHAPTERS]; /* 0xffff: never climbed */
    uint32_t best_time[CHAPTERS];   /* frames; 0: never climbed */
    uint16_t berries[CHAPTERS];     /* a bit per room */
    uint8_t cleared[CHAPTERS];
    uint8_t pad2;
    uint32_t climbs;
    /* version 2: the climb under way (the title's CONTINUE), written at each room's start */
    int8_t run_ch;                  /* -1: none */
    uint8_t run_room;
    uint16_t run_deaths, run_berries, run_skips;
    uint32_t run_time;
    uint16_t sum;
} save_data;
static save_data SV;
/* what the title and the results read: the save RAM as it was at the title's entry (saved in states: a state's replay
 * must not see the newer save RAM a later climb wrote) */
static save_data PV;
static const char MAGIC[5] = "GOAT";
#define SAVE_V1_SUM offsetof(save_data, run_ch)   /* version 1 ended there (its sum right after) */

static uint16_t sum_of(const save_data *s, size_t n)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x5153;
    for (size_t i = 0; i < n; i++) sum = (uint16_t)(sum * 31 + p[i]);
    return sum;
}
static uint16_t save_sum(const save_data *s) { return sum_of(s, offsetof(save_data, sum)); }

static int bits(unsigned v) { int n = 0; while (v) { n += (int)(v & 1); v >>= 1; } return n; }

static void save_load(void)
{
    memcpy(&SV, rs_sram(), sizeof SV);
    uint16_t v1sum;
    memcpy(&v1sum, (const uint8_t *)&SV + SAVE_V1_SUM, 2);
    if (!memcmp(SV.magic, MAGIC, 4) && SV.version == 1 && v1sum == sum_of(&SV, SAVE_V1_SUM)) {
        SV.version = 2;                         /* a version-1 save: its records kept, no climb under way */
        SV.run_ch = -1;
        SV.run_room = 0;
        SV.run_deaths = SV.run_berries = SV.run_skips = 0;
        SV.run_time = 0;
        SV.sum = save_sum(&SV);
    }
    if (memcmp(SV.magic, MAGIC, 4) || SV.version != 2 || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, MAGIC, 4);
        SV.version = 2;
        SV.run_ch = -1;
        for (int c = 0; c < CHAPTERS; c++) SV.best_deaths[c] = 0xffff;
    }
}

static void save_store(void)
{
    int score = 0;
    for (int c = 0; c < CHAPTERS; c++) score += SV.cleared[c] * 20 + bits(SV.berries[c]);
    SV.score = (uint16_t)score;
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

static int unlocked(int c) { return opt_unlock || c == 0 || PV.cleared[c - 1]; }
static int last_unlocked(void) { int c = 0; while (c + 1 < CHAPTERS && nrooms[c + 1] && unlocked(c + 1)) c++; return c; }

/* ---- the levels and the solutions from the asset pack ----------------------------------------------------------- */
static char text_buf[1 << 17];                 /* start-up scratch (state_audit.txt) */
static const char *asset_text(const char *name)
{
    size_t n;
    const char *p = rs_asset(name, &n);
    if (!p) return NULL;
    if (n >= sizeof text_buf) n = sizeof text_buf - 1;
    memcpy(text_buf, p, n);
    text_buf[n] = 0;
    return text_buf;
}

static void levels_load(void)
{
    char name[32], err[160];
    for (int c = 0; c < CHAPTERS; c++) {
        snprintf(name, sizeof name, "levels/ch%d.txt", c + 1);
        const char *s = asset_text(name);
        nrooms[c] = s ? chapter_parse(s, ROOMS[c], MAX_ROOMS, err, sizeof err) : 0;
        if (nrooms[c] < 0) {
            rs_log("%s: %s", name, err);
            nrooms[c] = 0;
        }
        nberries[c] = 0;
        for (int i = 0; i < nrooms[c]; i++) nberries[c] += ROOMS[c][i].has_berry;
    }
    const char *s = asset_text("levels/solutions.txt");
    sol_used = 0;
    memset(sol_len, 0, sizeof sol_len);
    while (s && *s) {
        const char *e = strchr(s, '\n');
        int c, r, frames, at = 0;
        char kind[16];
        if (sscanf(s, "%d %d %15s %d %n", &c, &r, kind, &frames, &at) == 4 && c >= 1 && c <= CHAPTERS && r >= 1 &&
            r <= MAX_ROOMS) {
            char line[40000];
            int len = (int)(e ? e - (s + at) : (int)strlen(s + at));
            if (len > (int)sizeof line - 1) len = (int)sizeof line - 1;
            memcpy(line, s + at, (size_t)len);
            line[len] = 0;
            int k = !strcmp(kind, "berry");
            int n = sol_decode(line, sol_in + sol_used, SOL_MAX - sol_used);
            if (n > 0) {
                sol_off[c - 1][r - 1][k] = sol_used;
                sol_len[c - 1][r - 1][k] = n;
                sol_used += n;
            }
        }
        s = e ? e + 1 : NULL;
    }
}

/* ---- sound: the game's own (the house synthesiser), then the house set --------------------------------------------- */
static void sound_init(void)
{
    ha_synth s;
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x60a7u);
    ha_tone(&s, 0, 420, 780, 70, 52, 40);                  /* jump: a springy blip up */
    ha_hiss(&s, 0, 30, 8, 3);
    ha_store(&s, SFX_JUMP, 80);
    ha_tone(&s, 0, 520, 900, 60, 50, 30);                  /* wall jump: a little higher, with a scrape */
    ha_hiss(&s, 0, 50, 18, 2);
    ha_store(&s, SFX_WALLJUMP, 80);
    ha_hiss(&s, 0, 150, 46, 1);                            /* dash: a whoosh, a low punch under it */
    ha_tone(&s, 0, 260, 120, 120, 40, 50);
    ha_store(&s, SFX_DASH, 160);
    ha_hiss(&s, 0, 50, 22, 3);                             /* land: a soft scuff of hooves */
    ha_tone(&s, 0, 140, 90, 40, 24, 20);
    ha_store(&s, SFX_LAND, 60);
    ha_tone(&s, 0, 600, 140, 220, 56, 90);                 /* death: a quick fall, a pop */
    ha_hiss(&s, ha_ms(60), 160, 30, 2);
    ha_store(&s, SFX_DEATH, 260);
    ha_tone(&s, 0, 1046, 1046, 70, 44, 50);                /* berry: two bright notes */
    ha_tone(&s, ha_ms(70), 1568, 1568, 160, 48, 80);
    ha_store(&s, SFX_BERRY, 240);
    ha_tone(&s, 0, 1318, 1760, 90, 40, 40);                /* gem: a glassy ping */
    ha_tone(&s, ha_ms(30), 2637, 2637, 120, 22, 50);
    ha_store(&s, SFX_GEM, 160);
    ha_tone(&s, 0, 200, 820, 160, 56, 90);                 /* spring: boing */
    ha_store(&s, SFX_SPRING, 180);
    ha_hiss(&s, 0, 120, 20, 3);                            /* crumble: a gravelly rustle */
    ha_tone(&s, 0, 110, 80, 90, 18, 40);
    ha_store(&s, SFX_CRUMBLE, 130);
    ha_tone(&s, 0, 784, 784, 90, 44, 60);                  /* a room cleared (the chapter: the house medal) */
    ha_tone(&s, ha_ms(90), 1046, 1046, 140, 44, 80);
    ha_store(&s, SFX_CLEAR, 240);
    ha_tone(&s, 0, 660, 990, 110, 30, 50);                 /* respawn: a soft rising chime */
    ha_store(&s, SFX_RESPAWN, 120);
    ha_init(SFX_HOUSE);                                    /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(opt_sound);
}

static void sfx(int id, int x)
{
    static const uint8_t vol[SFX_COUNT] = {HA_VOL_ACTION, HA_VOL_ACTION, HA_VOL_ACTION, 50, HA_VOL_CRASH,
                                           HA_VOL_FEEDBACK, HA_VOL_FEEDBACK, HA_VOL_ACTION, 60, HA_VOL_FEEDBACK, 50};
    ha_play_slot(id, vol[id], x, id == SFX_BERRY || id == SFX_GEM);
}

/* the chapter's tune (the title: the first one) */
static void music(int track)
{
    if (!opt_music) {
        if (music_cur >= 0) rs_music_stop();
        music_cur = -1;
        return;
    }
    if (track == music_cur && rs_music_playing()) return;
    char name[24];
    snprintf(name, sizeof name, "music/ch%d.mod", track + 1);
    size_t n;
    const void *m = rs_asset(name, &n);
    if (m && !rs_music_play(m, n, 1)) rs_music_volume(HA_MUSIC_VOL);
    music_cur = track;
}

/* ---- input: the pad onto the rules' bits ------------------------------------------------------------------------- */
static int pad_input(void)
{
    uint16_t p = rs_pad(0);
    int in = 0;
    if (p & RS_BTN_LEFT) in |= IN_LEFT;
    if (p & RS_BTN_RIGHT) in |= IN_RIGHT;
    if (p & RS_BTN_UP) in |= IN_UP;
    if (p & RS_BTN_DOWN) in |= IN_DOWN;
    if (p & (RS_BTN_A | RS_BTN_X)) in |= IN_JUMP;
    if (p & (RS_BTN_B | RS_BTN_Y | RS_BTN_L | RS_BTN_R)) in |= IN_DASH;
    return in;
}

static int bot_input(void)
{
    int k = opt_botberry && sol_len[chapter][room_no][1] ? 1 : 0;
    if (bot_f < sol_len[chapter][room_no][k]) return sol_in[sol_off[chapter][room_no][k] + bot_f++];
    return 0;
}

/* ---- the flow ------------------------------------------------------------------------------------------------------- */
static void go(int s) { st = s; st_t = 0; }

/* the climb under way, in save RAM: the title offers to CONTINUE it (not the bot's: the tests' save RAM stays) */
static void run_save(void)
{
    if (opt_bot) return;
    SV.run_ch = (int8_t)chapter;
    SV.run_room = (uint8_t)room_no;
    SV.run_time = (uint32_t)run_time;
    SV.run_deaths = (uint16_t)run_deaths;
    SV.run_berries = (uint16_t)run_berries;
    SV.run_skips = (uint16_t)run_skips;
    save_store();
}

static void enter_room(int n)
{
    room_no = n;
    bot_f = 0;
    hud_t = 0;
    room_deaths = opt_room_deaths;              /* (tests: roomdeaths=N, the first room played only) */
    opt_room_deaths = 0;
    world_spawn(&W, &ROOMS[chapter][n], opt_bot ? 0 : pad_input());
    draw_room(&ROOMS[chapter][n], chapter, n);
}

static void start_chapter(int c)
{
    chapter = c;
    run_time = run_deaths = run_berries = run_skips = 0;
    paused = 0;
    fade = 0;
    int r = opt_room > 0 && opt_room <= nrooms[c] ? opt_room - 1 : 0;
    enter_room(r);
    run_save();
    card_t = CARD_FRAMES;
    fade = -FADE_FRAMES;                              /* fading in */
    go(ST_PLAY);
    rs_log("chapter %d starts at frame %u", c + 1, rs_frame_count());
}

/* the title's CONTINUE: the climb saved at its last room's start */
static void continue_run(void)
{
    chapter = PV.run_ch;
    run_time = (int)PV.run_time;
    run_deaths = PV.run_deaths;
    run_berries = PV.run_berries;
    run_skips = PV.run_skips;
    paused = 0;
    fade = 0;
    enter_room(PV.run_room < nrooms[chapter] ? PV.run_room : 0);
    card_t = CARD_FRAMES;
    fade = -FADE_FRAMES;
    go(ST_PLAY);
    rs_log("chapter %d continues at room %d at frame %u", chapter + 1, room_no + 1, rs_frame_count());
}
static int can_continue(void) { return !opt_bot && PV.run_ch >= 0 && PV.run_ch < CHAPTERS && nrooms[PV.run_ch]; }

static void chapter_done(void)
{
    int c = chapter, got = bits((unsigned)run_berries);
    /* a climb with rooms skipped opens the next chapter and keeps its berries, but sets no record */
    new_time = !run_skips && (!PV.best_time[c] || (uint32_t)run_time < PV.best_time[c]);
    new_deaths = !run_skips && (PV.best_deaths[c] == 0xffff || run_deaths < PV.best_deaths[c]);
    new_berries = bits((unsigned)(PV.berries[c] | run_berries)) > bits(PV.berries[c]);
    if (new_time) SV.best_time[c] = (uint32_t)run_time;
    if (new_deaths) SV.best_deaths[c] = (uint16_t)run_deaths;
    SV.berries[c] = (uint16_t)(SV.berries[c] | run_berries);
    SV.cleared[c] = 1;
    SV.climbs++;
    if (!opt_bot) SV.run_ch = -1;                     /* the climb is over: nothing to continue */
    save_store();
    runs_done++;
    rs_log("chapter %d climbed at frame %u: time %d deaths %d berries %d/%d skipped %d", c + 1, rs_frame_count(),
           run_time, run_deaths, got, nberries[c], run_skips);
    ha_play(HA_MEDAL);
    go(ST_RESULTS);
}

static void back_to_title(void)
{
    PV = SV;
    paused = 0;
    fade = 0;
    if (sel > last_unlocked()) sel = last_unlocked();
    go(ST_TITLE);
}

static void play_update(void)
{
    const room *r = &ROOMS[chapter][room_no];
    run_time++;
    if (fade) {                                       /* a room's change: out (> 0), in (< 0) */
        if (fade > 0) {
            if (++fade > FADE_FRAMES) {
                if (fade_next < nrooms[chapter]) {
                    enter_room(fade_next);
                    run_save();
                    fade = -FADE_FRAMES;
                } else {
                    fade = 0;
                    chapter_done();
                }
            }
        } else {
            fade++;
        }
        return;
    }
    hud_t++;
    if (card_t) card_t--;
    int in = opt_bot ? bot_input() : pad_input();
    world_step(&W, r, in);
    if (opt_god && (W.events & EV_DEATH)) {           /* (tests) the death undone */
        W.h.dead_t = 0;
        W.events &= ~(uint32_t)EV_DEATH;
    }
    draw_events(&W, r);
    int x = W.h.x + HB_W / 2, ev = (int)W.events;
    if (ev & EV_JUMP) sfx(SFX_JUMP, x);
    if (ev & EV_WALLJUMP) sfx(SFX_WALLJUMP, x);
    if (ev & EV_DASH) sfx(SFX_DASH, x);
    if (ev & EV_LAND) sfx(SFX_LAND, x);
    if (ev & EV_SPRING) sfx(SFX_SPRING, x);
    if (ev & EV_GEM) sfx(SFX_GEM, x);
    if (ev & EV_BERRY) sfx(SFX_BERRY, x);
    if (ev & EV_CRUMBLE) sfx(SFX_CRUMBLE, x);
    if (ev & EV_RESPAWN) sfx(SFX_RESPAWN, x);
    if (ev & EV_DEATH) {
        sfx(SFX_DEATH, x);
        run_deaths++;
        room_deaths++;
        rs_log("death %d in room %d-%d at frame %u", run_deaths, chapter + 1, room_no + 1, rs_frame_count());
    }
    if (ev & EV_EXIT) {
        if (W.berry_got) run_berries |= 1 << room_no;
        sfx(SFX_CLEAR, x);
        fade_next = room_no + 1;
        fade = 1;
    }
    state_hash = (state_hash ^ world_hash(&W)) * 16777619u;
}

static void title_update(void)
{
    int cont = can_continue() && PV.run_ch == sel;
    hu_title_cfg tc = {START_INPUTS, 1, cont ? TR(VERB_CONTINUE) : TR(VERB), NULL, 0};
    hu_title_setup(&tc);                              /* PRESS A TO CLIMB, or to CONTINUE the climb under way */
    int ev = hu_title_update();
    uint16_t pr = rs_pad_pressed(0);
    if (pr & RS_BTN_LEFT && sel > 0) { sel--; ha_play(HA_SELECT); }
    if (pr & RS_BTN_RIGHT && sel < CHAPTERS - 1) {
        if (unlocked(sel + 1) && nrooms[sel + 1]) { sel++; ha_play(HA_SELECT); }
        else ha_play(HA_PAUSE);
    }
    if (((ev & HU_TITLE_START) || (opt_bot && st_t >= BOT_START)) && nrooms[sel]) {
        ha_play(HA_CONFIRM);
        if (cont) continue_run();
        else start_chapter(sel);
    } else if (cont && (pr & RS_BTN_B)) {             /* B: a new climb of the chapter, from room 1 */
        ha_play(HA_CONFIRM);
        start_chapter(sel);
    }
}

static void game_update(void)
{
    music(st == ST_PLAY ? chapter : 0);
    if (st == ST_PLAY && (rs_pad_pressed(0) & (RS_BTN_SELECT | RS_BTN_START))) {
        paused = !paused;
        ha_play(HA_PAUSE);
    } else if (st == ST_PLAY && paused && (rs_pad_pressed(0) & RS_BTN_B)) {
        ha_play(HA_SELECT);
        if (!fade) run_save();                        /* the climb kept: the title offers to continue it */
        back_to_title();
        return;
    } else if (st == ST_PLAY && paused && room_deaths >= SKIP_DEATHS && !fade && (rs_pad_pressed(0) & RS_BTN_A)) {
        ha_play(HA_CONFIRM);                          /* the room skipped: not completed, no berry */
        paused = 0;
        run_skips++;
        rs_log("room %d-%d skipped after %d deaths", chapter + 1, room_no + 1, room_deaths);
        fade_next = room_no + 1;
        fade = 1;
        return;
    }
    if (paused) return;
    hu_shake_step();
    switch (st) {
    case ST_TITLE:
        title_update();
        break;
    case ST_PLAY:
        play_update();
        break;
    case ST_RESULTS: {
        int back = st_t >= RETRY_LOCK && hu_over_back();
        if (opt_bot && st_t == RETRY_LOCK + 40) back = 1;
        if (back) {
            ha_play(HA_SELECT);
            PV = SV;                                      /* (the chapter just climbed opens the next) */
            if (chapter + 1 < CHAPTERS && nrooms[chapter + 1] && unlocked(chapter + 1)) sel = chapter + 1;
            back_to_title();
            if (opt_bot && runs_done >= opt_botruns) opt_bot = 0;     /* the bot's chapters done: it stops */
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
    v.chapter = chapter;
    v.room = room_no;
    v.rooms = nrooms[chapter];
    v.fade = fade > 0 ? fade * 16 / FADE_FRAMES : fade < 0 ? -fade * 16 / FADE_FRAMES : 0;
    v.time = run_time;
    v.deaths = run_deaths;
    v.berries = bits((unsigned)run_berries) + (W.berry_got && st == ST_PLAY ? 1 : 0);
    v.berries_max = nberries[chapter];
    v.sel = sel;
    v.unlocked = last_unlocked();
    for (int c = 0; c < CHAPTERS; c++) {
        v.best_time[c] = (int)PV.best_time[c];
        v.best_deaths[c] = PV.best_deaths[c] == 0xffff ? -1 : PV.best_deaths[c];
        v.best_berries[c] = bits(PV.berries[c]);
        v.cleared[c] = PV.cleared[c];
        v.room_count[c] = nrooms[c];
        v.berry_count[c] = nberries[c];
    }
    v.new_time = new_time;
    v.new_deaths = new_deaths;
    v.new_berries = new_berries;
    v.hud_t = hud_t;
    v.run_ch = can_continue() ? PV.run_ch : -1;
    v.run_room = PV.run_room;
    v.can_skip = st == ST_PLAY && room_deaths >= SKIP_DEATHS;
    v.skipped = run_skips;
    v.card = st == ST_PLAY ? card_t : 0;
    const room *r = &ROOMS[chapter][room_no < nrooms[chapter] ? room_no : 0];
    draw_frame(&W, r, &v);
}

static void game_init(void)
{
    rs_screen_tall(RS_SCREEN_H_MAX);          /* a phone held upright: every column, the room with sky above (DESIGN.md) */
    save_load();
    opt_music = rs_option_int("music", 1);
    opt_sound = rs_option_int("sound", 1);
    opt_bot = rs_option_int("bot", 0);
    opt_botruns = rs_option_int("botruns", 1);
    opt_botberry = rs_option_int("botberry", 1);
    opt_god = rs_option_int("god", 0);
    opt_unlock = rs_option_int("unlock", 0);
    opt_room = rs_option_int("room", 0);
    if (rs_option_int("fakerun", 0)) {          /* (tests, screenshots) a climb under way at 1-3, in memory only */
        SV.run_ch = 0;
        SV.run_room = 2;
        SV.run_time = 1234;
        SV.run_deaths = 5;
    }
    if (rs_option_int("records", 0)) {          /* (tests, screenshots) some records, in memory only */
        for (int c = 0; c < 3; c++) {
            SV.cleared[c] = 1;
            SV.best_time[c] = (uint32_t)(5000 + 1234 * c);
            SV.best_deaths[c] = (uint16_t)(3 + 9 * c);
            SV.berries[c] = (uint16_t)(0x0fff >> (c * 3));
        }
    }
    sound_init();
    levels_load();
    draw_init();
    hu_title_cfg tc0 = {START_INPUTS, 1, TR(VERB), NULL, 0};
    hu_title_setup(&tc0);
    hu_players_set(1);
    sel = 0;                                    /* the furthest chapter the climbs opened (not unlock=1's) */
    PV = SV;
    while (sel + 1 < CHAPTERS && nrooms[sel + 1] && PV.cleared[sel]) sel++;
    if (can_continue()) sel = PV.run_ch;           /* the climb under way first */
    if (rs_option("chapter")) sel = rs_option_int("chapter", 1) - 1;
    if (sel < 0 || sel >= CHAPTERS || !nrooms[sel]) sel = 0;
    if (!unlocked(sel)) opt_unlock = opt_unlock || opt_bot;
    if (!unlocked(sel)) sel = last_unlocked();
    chapter = sel;
    enter_room(0);
    opt_room_deaths = rs_option_int("roomdeaths", 0);
    go(ST_TITLE);
    if (rs_option_int("ready", 0)) start_chapter(sel);
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    rs_log("state: st=%d chapter=%d room=%d time=%d deaths=%d berries=%d runs=%d paused=%d sel=%d x=%d y=%d "
           "dead=%d cleared=%d%d%d%d%d score=%d hash=%08x",
           st, chapter + 1, room_no + 1, run_time, run_deaths, bits((unsigned)run_berries), runs_done, paused, sel + 1,
           W.h.x, W.h.y, W.h.dead_t, SV.cleared[0], SV.cleared[1], SV.cleared[2], SV.cleared[3], SV.cleared[4],
           SV.score, state_hash);
}

/* ---- save states: the SDK saves the console; here are the game's objects (tools/state_audit.py checks them) ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(chapter); S(room_no); S(sel); S(fade); S(fade_next); S(hud_t);
    S(run_time); S(run_deaths); S(run_berries); S(room_deaths); S(run_skips); S(card_t); S(opt_room_deaths); S(PV); S(new_time); S(new_deaths); S(new_berries); S(runs_done);
    S(bot_f); S(opt_bot); S(opt_botruns); S(opt_botberry); S(opt_god); S(opt_music); S(opt_sound); S(opt_unlock);
    S(opt_room); S(music_cur); S(state_hash);
    draw_state();
    hu_state();
    ha_state();
}
#undef S

static void game_state_loaded(void)
{
    draw_relayout();
}

const rs_game *rs_game_main(void)
{
    /* max_width: the game draws any width up to 560 (widescreen: the mountain past the room's sides) */
    static const rs_game g = {"Goat Gorge", "goatgorge", "0.1.0", game_init, game_update, game_draw, game_shutdown,
                              gg_assets, game_state, game_state_loaded, 1, RS_SCREEN_W_MAX, 0};
    return &g;
}
