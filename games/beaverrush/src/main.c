/*
 * Beaver Rush: the game flow (the title and its lobby, play, the end of a run, game over and the results), input,
 * save RAM, options, save states and the test hooks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
 *
 * The house flow (docs/art-direction.md "Title and players"): the title is the only menu. Pads 2-4 join there with
 * A (or Start) and leave with B (the kit's lobby, house_ui.c); P1 starts the run with a gnaw (Left, Right, A, B),
 * which is also its first gnaw. After a game over one gnaw press starts the next run at once with the same players
 * (and gnaws); Select goes back to the title.
 *
 * Options (--opt key=value on the desktop runners):
 *   bot=N          the screen-reading bot plays players 1..N (bot.c); botruns=N runs
 *   botstop=S      the bot stops gnawing at score S (a game over for the screenshots)
 *   seed=N         a fixed tree (default: from the frame of the new run and of the first gnaw)
 *   skip=N         start the run with N logs gnawed (the dam, the scene and the level of N logs; tests)
 *   players=N      start with players 1..N joined (2-4: versus);  ready=1  skip the title (the run waits for a gnaw)
 *   dump=1         log the final state at exit (tests); music=0, sound=0
 */
#include "br.h"
#include "draw.h"
#include "assets.h"
#include "house_ui.h"
#include "house_audio.h"
#include <stdio.h>
#include <string.h>

#define LEFT_BUTTONS  (RS_BTN_LEFT | RS_BTN_B | RS_BTN_Y)
#define RIGHT_BUTTONS (RS_BTN_RIGHT | RS_BTN_A | RS_BTN_X)
#define GNAW_BUTTONS  (LEFT_BUTTONS | RIGHT_BUTTONS)
/* P1's start inputs on the title (the kit prompts "PRESS <-/-> TO GNAW" with the D-pad glyph) */
#define START_INPUTS  (RS_BTN_LEFT | RS_BTN_RIGHT | RS_BTN_A | RS_BTN_B)

int opt_bot;
static int opt_botruns, opt_botstop, opt_skip, opt_seed_fixed;
static uint32_t opt_seed;
static world W;
static int st, st_t, paused, new_best, runs_done;
static int first_gnaw[MAX_PLAYERS];     /* the press that started the run (title, retry) is also that gnaw */
static uint32_t state_hash = 2166136261u;
static int run_scores[64], nrun_scores;

/* ---- save RAM (the house convention: checked, versioned, committed at each game over) --------------------------- */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best, best_vs;             /* best_vs: the best score of a versus (2-4 players) */
    uint32_t runs;
    uint16_t medals[4];
    uint16_t vs_wins[4];                /* versus wins by pad */
    uint16_t sum;
} save_data;
typedef struct save_v1 {                /* version 1 (2 players): read once, then saved as version 2 */
    char magic[4];
    uint8_t version, pad[3];
    uint16_t best, best_vs;
    uint32_t runs;
    uint16_t medals[4];
    uint16_t vs_wins[2];
    uint16_t sum;
} save_v1;
static save_data SV;

static uint16_t sum_of(const void *s, size_t n)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x5153;
    for (size_t i = 0; i < n; i++) sum = (uint16_t)(sum * 31 + p[i]);
    return sum;
}

static void save_load(void)
{
    save_v1 v1;
    memcpy(&SV, rs_sram(), sizeof SV);
    memcpy(&v1, rs_sram(), sizeof v1);
    if (!memcmp(SV.magic, "BVRU", 4) && SV.version == 2 && SV.sum == sum_of(&SV, offsetof(save_data, sum))) return;
    memset(&SV, 0, sizeof SV);
    memcpy(SV.magic, "BVRU", 4);
    SV.version = 2;
    if (!memcmp(v1.magic, "BVRU", 4) && v1.version == 1 && v1.sum == sum_of(&v1, offsetof(save_v1, sum))) {
        SV.best = v1.best;
        SV.best_vs = v1.best_vs;
        SV.runs = v1.runs;
        memcpy(SV.medals, v1.medals, sizeof SV.medals);
        SV.vs_wins[0] = v1.vs_wins[0];
        SV.vs_wins[1] = v1.vs_wins[1];
    }
}

static void save_store(void)
{
    SV.sum = sum_of(&SV, offsetof(save_data, sum));
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

/* ---- flow ------------------------------------------------------------------------------------------------------------ */
static void go(int s) { st = s; st_t = 0; }

static uint32_t frame_seed(void) { return opt_seed ^ (uint32_t)rs_frame_count() * 2654435761u; }
static uint32_t run_seed(void) { return opt_seed_fixed ? opt_seed : frame_seed(); }

static void new_run(int state, int players, uint32_t seed)
{
    world_init(&W, players, seed);
    if (opt_skip) world_skip(&W, opt_skip);
    draw_new_run(&W);
    bot_reset(W.seed);
    memset(first_gnaw, 0, sizeof first_gnaw);
    new_best = 0;
    go(state);
}

static int is_bot(int p) { return p < opt_bot; }

static int side_of(uint16_t b) { return (b & LEFT_BUTTONS) ? SIDE_L : (b & RIGHT_BUTTONS) ? SIDE_R : 0; }

/* SIDE_L / SIDE_R when player p gnaws this frame */
static int gnaw_pressed(int p)
{
    if (first_gnaw[p]) {
        int s = first_gnaw[p];
        first_gnaw[p] = 0;
        return s;
    }
    if (is_bot(p)) {
        if (opt_botstop && W.bv[p].score >= opt_botstop && st == DS_PLAY) return 0;
        return bot_decide(p);
    }
    return side_of(rs_pad_pressed(hu_player_pad(p)));
}

static void game_over(void)
{
    static const int medals[4] = {MEDAL_T1, MEDAL_T2, MEDAL_T3, MEDAL_T4};
    int best_now = 0;
    for (int p = 0; p < W.players; p++)
        if (W.bv[p].score > best_now) best_now = W.bv[p].score;
    SV.runs++;
    int m = W.players == 1 ? hu_medal_of(W.bv[0].score, medals) : 0;
    if (m) SV.medals[m - 1]++;
    if (W.players == 1 && best_now > SV.best) { SV.best = (uint16_t)best_now; new_best = 1; }
    if (W.players >= 2) {
        if (best_now > SV.best_vs) SV.best_vs = (uint16_t)best_now;
        if (W.winner >= 0) SV.vs_wins[hu_player_pad(W.winner) & 3]++;
    }
    if (!opt_skip) save_store();                  /* a skipped start (tests) does not count */
    if (nrun_scores < 64) run_scores[nrun_scores++] = W.bv[0].score;
    runs_done++;
    rs_log("run %d over at frame %u: score %d (best %d)", runs_done, rs_frame_count(), W.bv[0].score, SV.best);
    if (W.players >= 2) {
        int key[MAX_PLAYERS];
        hu_standing s;
        char line[64] = "";
        world_rank_keys(&W, key);
        hu_rank(&s, W.players, key, NULL);
        for (int i = 0; i < s.n; i++) {
            size_t n = strlen(line);
            snprintf(line + n, sizeof line - n, " %d:P%d", s.rank[i] + 1, s.order[i] + 1);
        }
        rs_log("versus over: winner %d, ranking%s", W.winner + 1, line);
    }
    ha_play(HA_GAMEOVER);
    if (m) ha_play(HA_MEDAL);
    go(DS_OVER);
}

/* one frame of the world; pads = 0: nobody gnaws (the title's idle beaver) */
static void play_update(int pads)
{
    int press[MAX_PLAYERS] = {0};
    for (int p = 0; p < W.players && pads; p++) {
        int s = W.bv[p].state;
        if (s == BV_READY || s == BV_PLAY) press[p] = gnaw_pressed(p);
    }
    uint32_t reroll = (!W.started && !opt_seed_fixed && !opt_skip) ? frame_seed() | 1 : 0;
    int standing = world_standing(&W);
    world_step(&W, press, reroll);
    draw_events(&W);
    for (int p = 0; p < W.players; p++) {
        int ev = W.events[p];
        const beaver *b = &W.bv[p];
        int x = draw_view_x(&W, p);
        if ((ev & EV_GNAW) && !(ev & EV_BONK)) sfx_pan(SFX_CHOMP, x, 0);       /* a bonk replaces the bite */
        if (ev & EV_GOLD) {
            sfx_pan(SFX_GOLD, x, 0);
            rs_log("player %d golden log at frame %u (score %d)", p + 1, rs_frame_count(), b->score);
        }
        if (ev & EV_MILESTONE) {
            sfx_pan(SFX_CHEER, x, 0);
            rs_log("player %d milestone %d at frame %u", p + 1, b->logs / STAGE_LOGS, rs_frame_count());
        }
        if (ev & EV_SENT) {
            char sc[40] = "";
            for (int q = 0; q < W.players; q++) {
                size_t n = strlen(sc);
                snprintf(sc + n, sizeof sc - n, "%s%d", q ? "," : "", W.bv[q].score);
            }
            rs_log("player %d sends a branch to player %d at frame %u (scores %s)", p + 1, b->sent_to + 1,
                   rs_frame_count(), sc);
        }
        if (ev & (EV_SENT | EV_STOLEN)) sfx_pan(SFX_WHOOSH, x, 0);
        if (ev & EV_BONK) {
            sfx_pan(SFX_BONK, x, 0);
            hu_shake(3, 12);                  /* house rule: shake on a hit only, <= 4 px, <= 16 frames */
            rs_log("player %d bonked (hit %c) at frame %u (score %d)", p + 1, b->bonk_hit == 1 ? 'A' : 'B',
                   rs_frame_count(), b->score);
        }
        if (ev & EV_SLEEP) {
            sfx_pan(SFX_SLEEP, x, 0);
            rs_log("player %d out of breath at frame %u (score %d)", p + 1, rs_frame_count(), b->score);
        }
        state_hash = (state_hash ^ (uint32_t)(b->bar >> 8)) * 16777619u;
        state_hash = (state_hash ^ (uint32_t)(b->score * 131 + b->side * 7 + b->state)) * 16777619u;
    }
    if (W.players >= 2 && W.started && world_standing(&W) < standing)
        rs_log("versus: %d of %d beavers left at frame %u", world_standing(&W), W.players, rs_frame_count());
    if (st == DS_PLAY && W.over) go(DS_END);
    if (st == DS_END && st_t >= PANEL_DELAY) game_over();
}

/* the run starts with player p's press (side): straight into play, the press is also its first gnaw */
static void start_run(int p, int side, int fresh)
{
    int n = hu_players();
    if (fresh) new_run(DS_PLAY, n, run_seed());
    else if (n != W.players) new_run(DS_PLAY, n, W.seed);      /* the title's tree, for everyone */
    else go(DS_PLAY);                                            /* one player: the title's run goes on */
    first_gnaw[p] = side;
    play_update(1);
}

/* the title: players join and leave (the kit's lobby); P1 starts with a gnaw */
static void title_update(void)
{
    int ev = hu_title_update();
    if (ev & HU_TITLE_JOINED) sfx(SFX_JOIN);
    if (ev & HU_TITLE_LEFT) ha_play(HA_SELECT);
    if (ev & (HU_TITLE_JOINED | HU_TITLE_LEFT)) rs_log("lobby: %d players at frame %u", hu_players(), rs_frame_count());
    int side = 0;
    if (ev & HU_TITLE_START) side = side_of(rs_pad_pressed(hu_player_pad(0)) & START_INPUTS);
    else if (is_bot(0)) side = bot_decide(0);
    if (side) {                                     /* no start sound: the first bite is the answer */
        start_run(0, side, 0);
        return;
    }
    play_update(0);                                 /* the title's beaver idles */
}

static void game_update(void)
{
    int lvl = 0;
    for (int p = 0; p < W.players; p++)
        if (W.bv[p].level > lvl) lvl = W.bv[p].level;
    music_update(lvl, st == DS_PLAY);
    if (st == DS_PLAY && (rs_pad_pressed(0) & (RS_BTN_START | RS_BTN_SELECT))) {
        paused = !paused;
        sfx(SFX_PAUSE);
    }
    if (paused) return;
    switch (st) {
    case DS_TITLE:
        title_update();
        break;
    case DS_PLAY:
    case DS_END:
        play_update(1);
        break;
    case DS_OVER:
        if (st_t == 1) sfx(SFX_PANEL);
        if (st_t >= RETRY_LOCK && (hu_over_back() ||
            (opt_bot && runs_done < opt_botruns && st_t == RETRY_LOCK + 10))) {
            ha_play(HA_CONFIRM);
            new_run(DS_TITLE, 1, run_seed());
        }
        break;
    }
    draw_update(&W, st);
    st_t++;
}

static void game_draw(void) { draw_frame(&W, st, st_t, W.players >= 2 ? SV.best_vs : SV.best, new_best, paused); }

static void game_init(void)
{
    save_load();
    audio_set(rs_option_int("music", 1), rs_option_int("sound", 1));
    sfx_init();
    draw_init();                          /* hu_init (draw.c) */
    hu_title_cfg tc = {START_INPUTS, MAX_PLAYERS, "GNAW", NULL, 0};
    hu_title_setup(&tc);
    opt_bot = clampi(rs_option_int("bot", 0), 0, MAX_PLAYERS);
    opt_botruns = rs_option_int("botruns", 1);
    opt_botstop = rs_option_int("botstop", 0);
    opt_skip = clampi(rs_option_int("skip", 0), 0, 5000);
    opt_seed_fixed = rs_option("seed") != NULL;
    opt_seed = (uint32_t)rs_option_int("seed", 0xbea7e5);
    int n = clampi(rs_option_int("players", 1), 1, MAX_PLAYERS);
    if (opt_bot > n) n = opt_bot;
    if (n > 1) hu_players_set(n);
    if (rs_option_int("ready", 0)) new_run(DS_PLAY, n, run_seed());
    else new_run(DS_TITLE, 1, run_seed());
}

static void list_of(char *out, size_t size, const int *v, int n)
{
    out[0] = 0;
    for (int i = 0; i < n; i++) {
        size_t k = strlen(out);
        snprintf(out + k, size - k, "%s%d", i ? "," : "", v[i]);
    }
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    char list[400], scores[48], states[48], outs[64];
    int sc[MAX_PLAYERS], sv[MAX_PLAYERS], ot[MAX_PLAYERS];
    for (int p = 0; p < MAX_PLAYERS; p++) {
        sc[p] = W.bv[p].score;
        sv[p] = W.bv[p].state;
        ot[p] = W.bv[p].out_t;
    }
    list_of(list, sizeof list, run_scores, nrun_scores);
    list_of(scores, sizeof scores, sc, W.players);
    list_of(states, sizeof states, sv, W.players);
    list_of(outs, sizeof outs, ot, W.players);
    int vram = rs_tiles_used() * RS_TILE_BYTES;    /* the guideline's count: tiles + the enabled maps */
    for (int l = 0; l < RS_BG_COUNT; l++) vram += rs_bg_map_w(l) * rs_bg_map_h(l) * 2;
    rs_log("state: st=%d players=%d score=%d score2=%d best=%d runs=%d bv=%d,%d side=%d logs=%d bar=%d paused=%d "
           "winner=%d stage=%d level=%d tempo=%d dam=%d hash=%08x lobby=%d scores=%s bvs=%s outs=%s vram=%d "
           "runscores=%s",
           st, W.players, W.bv[0].score, W.bv[1].score, SV.best, runs_done, W.bv[0].state, W.bv[1].state,
           W.bv[0].side, W.bv[0].logs, (int)((int64_t)W.bv[0].bar * 1000 / BAR_FULL), paused, W.winner,
           world_stage(&W), W.bv[0].level, music_tempo(), dam_logs_drawn(), state_hash, hu_players(), scores, states,
           outs, vram, list);
}

/* test hook (tests): the run and the screen being shown */
const world *br_test_world(int *state)
{
    if (state) *state = st;
    return &W;
}

/* ---- save states: the SDK saves the console (VRAM, maps, palettes, sprites, voices, music, RNG, pads); here are the
 * game's own objects (tools/state_audit.py checks that none is forgotten). Not saved: SV, the battery save's copy. ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(W); S(st); S(st_t); S(paused); S(new_best); S(runs_done); S(first_gnaw); S(state_hash); S(run_scores);
    S(nrun_scores); S(opt_bot); S(opt_botruns); S(opt_botstop); S(opt_skip); S(opt_seed_fixed); S(opt_seed);
    draw_state();
    sfx_state();
    bot_state();
    hu_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* state_version: bump it when the meaning of a saved object changes (its layout is checked) */
    static const rs_game g = {"Beaver Rush", "beaverrush", "0.2.0", game_init, game_update, game_draw,
                              game_shutdown, br_assets, game_state, NULL, 2};
    return &g;
}
