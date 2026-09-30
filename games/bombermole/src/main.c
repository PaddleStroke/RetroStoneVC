/*
 * Bomber Mole: game flow (title, menus, levels, transitions, save RAM).
 * All rights reserved, 8BCraft.
 */
#include "bm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum state {
    ST_TITLE, ST_ARCS, ST_LEVELS, ST_OPTIONS, ST_CREDITS, ST_INTRO, ST_PLAY, ST_SLIDE, ST_PAUSE,
    ST_DYING, ST_GAMEOVER, ST_OUTRO, ST_CLEAR, ST_ARCDONE, ST_SPRITETEST, ST_JOIN, ST_BATTLE
};
static int join_mode;                           /* the join screen: MODE_COOP (story) or MODE_BATTLE */

int opt_music = 1, opt_sfx = 1, opt_diff = DIFF_NORMAL;
static int st, st_t, cursor, sel_arc, sel_level, unlock_all;
static level_def LV;
static pstats carry[MAX_PLAYERS];
static int view_depth, slide_from, slide_to, level_frames, forced_view = -1;
static int title_ready;
static uint32_t gt;
/* developer mode (DESIGN.md "Dev mode"): --dev, or hold L+R and press Start on the title screen */
static int dev_mode, dev_unlock_all, dev_skip, restarts, pause_quit;
static int box_key = -1;        /* the level whose start box was seen (retries skip it) */
int dev_god, dev_reveal, dev_perf;

/* ---- save RAM ------------------------------------------------------------------------------------ */
typedef struct save_data {
    char magic[4];
    uint8_t version, pad0;
    uint8_t cleared[4];
    uint8_t opts;               /* bit0 music, bit1 sound, bit2 all unlocked */
    uint8_t last_arc, last_level, pad1;   /* pad1: tutorial flags of an older version, ignored */
    uint16_t best[4][8];        /* seconds, 0 = none */
    uint16_t sum;
} save_data;
static save_data SV;

static uint16_t save_sum(const save_data *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x4d42;
    for (size_t i = 0; i < offsetof(save_data, sum); i++) sum = (uint16_t)(sum * 31 + p[i]);
    return sum;
}

static void save_load(void)
{
    memcpy(&SV, rs_sram(), sizeof SV);
    if (memcmp(SV.magic, "BMSV", 4) || SV.version != 1 || SV.sum != save_sum(&SV)) {
        memset(&SV, 0, sizeof SV);
        memcpy(SV.magic, "BMSV", 4);
        SV.version = 1;
        SV.opts = 3;
    }
    opt_music = SV.opts & 1;
    opt_sfx = (SV.opts >> 1) & 1;
    opt_diff = ((SV.opts >> 3) & 3) ? ((SV.opts >> 3) & 3) - 1 : DIFF_NORMAL;   /* 0 = not set: normal */
    MP.friendly_fire = (SV.opts >> 5) & 1;          /* multiplayer options */
    MP.merge = (SV.opts >> 6) & 1;
    MP.split_h = (SV.opts >> 7) & 1;
    unlock_all = (SV.opts >> 2) & 1;
}

static void save_store(void)
{
    SV.opts = (uint8_t)(opt_music | (opt_sfx << 1) | (unlock_all << 2) | ((opt_diff + 1) << 3) | (MP.friendly_fire << 5) |
                        (MP.merge << 6) | (MP.split_h << 7));
    SV.sum = save_sum(&SV);
    memcpy(rs_sram(), &SV, sizeof SV);
    rs_sram_commit();
}

static int unlocked(int arc, int n)
{
    if (unlock_all || (dev_mode && dev_unlock_all)) return 1;
    if (arc > 0 && SV.cleared[arc - 1] < 8) return 0;
    return SV.cleared[arc] >= n - 1;
}

/* ---- helpers -------------------------------------------------------------------------------------- */
static int st_changed;
static void go(int s) { st = s; st_t = 0; cursor = 0; st_changed = 1; }
/* menus and pause: any pad in the menus and the solo game (whoever picks up a pad can play), any human player's
   pad in multiplayer; the prompts name the buttons of the last one used (prompt_port) */
static uint16_t pressed(void)
{
    uint16_t p = 0;
    for (int i = 0; i < RS_PAD_MAX; i++) {
        int ok = MP.mode == MODE_SOLO;
        for (int k = 0; k < MP.nplayers && !ok; k++) ok = MP.port[k] == i;
        uint16_t b = ok ? rs_pad_pressed(i) : 0;
        if (b) { p |= b; prompt_port = i; }
    }
    return p;
}

/* "A (X KEY): OPEN   B (Z KEY): BACK", centred on row y */
static void keys_line(int y, const char *a, const char *b)
{
    char s[64];
    if (b) snprintf(s, sizeof s, "%s: %s   %s: %s", btn_name(RS_BTN_A, prompt_port), a, btn_name(RS_BTN_B, prompt_port), b);
    else snprintf(s, sizeof s, "PRESS %s", btn_name(RS_BTN_A, prompt_port));
    text_at(0, y, "                                        ");
    text_at((40 - (int)strlen(s)) / 2, y, s);
}
static int confirm(void) { return pressed() & (RS_BTN_A | RS_BTN_START); }
static int back(void) { return pressed() & (RS_BTN_B | RS_BTN_SELECT); }

static int menu_nav(int n)
{
    uint16_t p = pressed();
    int old = cursor;
    if (p & RS_BTN_UP) cursor = (cursor + n - 1) % n;
    if (p & RS_BTN_DOWN) cursor = (cursor + 1) % n;
    if (cursor != old) sfx(SFX_MENU_MOVE);
    return cursor;
}

static void plain_text(void) { rs_text_setup(RS_BG1, 0, 0, 1); }

static int center(const char *s) { return (40 - (int)strlen(s)) / 2; }

/* ---- title and menus -------------------------------------------------------------------------------- */
static int16_t hill_dx[240];

static void title_raster(int line, void *u)
{
    (void)u;
    /* sky gradient on the backdrop and a gold gradient on the title letters (per line, like HDMA) */
    int k = line * 31 / 239;
    rs_pal_set(0, RS_RGB(8 + k / 3, 14 + k / 3, 31 - k / 4));
    if (line >= 40 && line < 72) {
        int g = line - 40;
        rs_pal_set(1, RS_RGB(31, 31 - g / 3, 12 + g / 4));
    } else {
        rs_pal_set(1, RS_HEX(0xffffff));
    }
    rs_window(0, 0, 0);
    rs_window(1, 0, RS_SCREEN_W);
}

static void menu_backdrop(void)
{
    draw_title_vram();
    const uint16_t (*T)[4] = bm_terrain_meta[SEASON_SPRING];
    rs_bg_fill(RS_BG4, 0);
    rs_bg_fill(RS_BG3, 0);
    for (int x = 0; x < 32; x++) {
        /* near and far hills: rounded tops generated from the grass (T_HILL), grass below */
        for (int y = 12; y < 16; y++) rs_bg_meta(RS_BG3, x, y, y == 12 ? T[T_HILL + (x + 4) % 8] : y < 15 ? T[T_GRASS] : T[T_SOFT_DIRT]);
        int h = 9;
        for (int y = h; y < 16; y++) rs_bg_meta(RS_BG4, x, y, y == h ? T[T_HILL + x % 8] : T[T_GRASS]);
    }
    rs_bg_enable(RS_BG2, 0);
    rs_raster(title_raster, NULL);
    rs_clip_black(0);
    text_clear_all();
    for (int x = 0; x < 64; x++) { rs_bg_put(RS_BG1, x, 0, 0); rs_bg_put(RS_BG1, x, 1, 0); }
    title_ready = 1;
}

static void menu_scroll(void)
{
    rs_bg_scroll(RS_BG4, (int)(gt / 5), 24);        /* far hills */
    rs_bg_scroll(RS_BG3, (int)(gt / 2), 0);         /* near hills (drawn in front) */
    for (int i = 0; i < 240; i++) hill_dx[i] = 0;
    rs_bg_line_scroll(RS_BG3, NULL, NULL);
}

static void show_logo(int on)
{
    if (!bm_logo_tile_count) return;
    rs_bg_enable(RS_BG2, on);
    if (!on) return;
    rs_tiles_load(1024 + LOGO_TILE_BASE, bm_logo_tiles, bm_logo_tile_count);
    for (int i = 1; i < 16; i++) rs_pal_set(RS_PAL_BG(5) + i, bm_logo_pal[i]);
    rs_bg_setup(RS_BG2, 64, 32, 1024);          /* 64 wide: the 256-px logo must not wrap on 320 px */
    for (int y = 0; y < LOGO_H; y++)
        for (int x = 0; x < LOGO_W; x++) rs_bg_put(RS_BG2, 4 + x, 2 + y, bm_logo_map[y * LOGO_W + x]);
    rs_bg_scroll(RS_BG2, 0, 0);
    rs_bg_line_scroll(RS_BG2, NULL, NULL);
    rs_bg_window(RS_BG2, 0);
    rs_math(RS_MATH_OFF, 0, 0);
}

static void title_update(void)
{
    if (!title_ready) {
        menu_backdrop();
        music_play("title");
    }
    menu_scroll();
    plain_text();
    if (bm_logo_tile_count) { if (st_t == 0) show_logo(1); }
    else text_big(9, 5, "BOMBER MOLE");
    static const char *const items[] = {"STORY", "BATTLE", "OPTIONS", "CREDITS"};
    MP.mode = MODE_SOLO;
    if ((rs_pad(0) & (RS_BTN_L | RS_BTN_R)) == (RS_BTN_L | RS_BTN_R) && (pressed() & RS_BTN_START)) {
        dev_mode = dev_unlock_all = 1;              /* the developer code */
        sfx(SFX_EXIT_OPEN);
        return;
    }
    if (dev_mode) text_at(center("DEV MODE"), 25, "DEV MODE");
    int c = menu_nav(4);
    for (int i = 0; i < 4; i++) {
        text_at(16, 14 + i * 2, i == c ? ">" : " ");
        text_at(18, 14 + i * 2, items[i]);
    }
    text_at(center("(C) 2026 8BCRAFT - RETROSTONE VC"), 27, "(C) 2026 8BCRAFT - RETROSTONE VC");
    if (confirm()) {
        sfx(SFX_MENU_OK);
        text_clear_all();
        show_logo(0);
        /* the pad that confirms is P1. Story: straight to the seasons, alone (2-4 PLAYERS is an entry there);
           battle: the join screen, P1 already in */
        if (c == 0) {
            MP.mode = MODE_SOLO;
            MP.nplayers = 1;
            for (int p = 0; p < MAX_PLAYERS; p++) { MP.port[p] = (int8_t)(p ? -1 : prompt_port); MP.cpu[p] = 0; MP.colour[p] = (uint8_t)p; }
            go(ST_ARCS);
            cursor = sel_arc;
        } else if (c == 1) {
            join_mode = MODE_BATTLE;
            mp_join_begin(prompt_port);
            mp_join_enter();
            go(ST_JOIN);
        }
        else if (c == 2) go(ST_OPTIONS);
        else go(ST_CREDITS);
    }
}

static int col;                                      /* the season under the cursor (cursor 4: 2-4 PLAYERS) */
static int shown = -1;                               /* the level whose name the level select shows */

static void arcs_update(void)
{
    menu_scroll();
    plain_text();
    text_at(center("CHOOSE A SEASON"), 3, "CHOOSE A SEASON");
    uint16_t p = pressed();
    int old = cursor;
    if (cursor < 4) col = cursor;
    if (cursor < 4 && (p & RS_BTN_LEFT)) cursor = (cursor + 3) % 4;
    if (cursor < 4 && (p & RS_BTN_RIGHT)) cursor = (cursor + 1) % 4;
    if (p & (RS_BTN_UP | RS_BTN_DOWN)) cursor = cursor < 4 ? 4 : col;
    if (cursor != old) sfx(SFX_MENU_MOVE);
    static const char *const names[4] = {"SPRING", "SUMMER", "AUTUMN", "WINTER"};
    char pl[24];
    if (MP.mode == MODE_COOP) snprintf(pl, sizeof pl, "%c PLAYERS: %d (CO-OP)", cursor == 4 ? '>' : ' ', MP.nplayers);
    else snprintf(pl, sizeof pl, "%c 2-4 PLAYERS", cursor == 4 ? '>' : ' ');
    text_at(0, 14, "                                        ");
    text_at((40 - (int)strlen(pl)) / 2, 14, pl);
    for (int a = 0; a < 4; a++) {
        int x = 2 + a * 10;
        text_at(x, 8, a == cursor ? ">" : " ");
        text_at(x + 1, 8, names[a]);
        int ok = unlocked(a, 1);
        textf_at(x + 1, 10, ok ? "%d/8   " : "LOCKED", SV.cleared[a]);
        rs_bg_meta(RS_BG1, x / 2 + 1, 6, bm_hud_meta[ok ? (SV.cleared[a] == 8 ? HUD_CHECK : HUD_GRUB) : HUD_LOCK]);
    }
    keys_line(24, "OPEN", "BACK");
    if (confirm() && cursor == 4) {                  /* 2-4 PLAYERS: the join screen, this pad already P1 */
        sfx(SFX_MENU_OK);
        text_clear_all();
        join_mode = MODE_COOP;
        mp_join_begin(prompt_port);
        mp_join_enter();
        go(ST_JOIN);
        return;
    }
    if (confirm()) {
        if (unlocked(cursor, 1)) {
            sfx(SFX_MENU_OK);
            sel_arc = cursor;
            text_clear_all();
            go(ST_LEVELS);
            cursor = SV.last_arc == sel_arc ? SV.last_level : 0;
        } else {
            sfx(SFX_HURT);
        }
    }
    if (back()) { text_clear_all(); go(ST_TITLE); cursor = 0; }
}


static void levels_update(void)
{
    menu_scroll();
    plain_text();
    static const char *const names[4] = {"SPRING", "SUMMER", "AUTUMN", "WINTER"};
    textf_at(center("SPRING - CHOOSE A LEVEL"), 3, "%s - CHOOSE A LEVEL", names[sel_arc]);
    uint16_t p = pressed();
    int old = cursor;
    if (p & RS_BTN_LEFT) cursor = (cursor + 7) % 8;
    if (p & RS_BTN_RIGHT) cursor = (cursor + 1) % 8;
    if (p & (RS_BTN_UP | RS_BTN_DOWN)) cursor = (cursor + 4) % 8;
    if (cursor != old) sfx(SFX_MENU_MOVE);
    /* tester cheat: hold L+R and press Select */
    if ((rs_pad(0) & (RS_BTN_L | RS_BTN_R)) == (RS_BTN_L | RS_BTN_R) && (p & RS_BTN_SELECT)) {
        unlock_all = 1;
        save_store();
        sfx(SFX_EXIT_OPEN);
        return;
    }
    for (int n = 0; n < 8; n++) {
        int cx = 4 + (n % 4) * 4, cy = 4 + (n / 4) * 3;
        int icon = !unlocked(sel_arc, n + 1) ? HUD_LOCK : SV.cleared[sel_arc] > n ? HUD_CHECK : HUD_DIGIT + n + 1;
        rs_bg_meta(RS_BG1, cx, cy, bm_hud_meta[icon]);
        rs_bg_meta(RS_BG1, cx - 1, cy, bm_hud_meta[n == cursor ? HUD_CURSOR : HUD_PANEL]);
    }
    level_def *L = &LV;
    if (shown != sel_arc * 8 + cursor || st_t == 0) {
        shown = sel_arc * 8 + cursor;
        for (int y = 19; y < 26; y++) text_at(0, y, "                                        ");
        if (!level_load(L, sel_arc, cursor + 1)) {
            textf_at(center(L->name), 20, "%s", L->name);
            int b = SV.best[sel_arc][cursor];
            if (b) textf_at(14, 22, "BEST %2d:%02d", b / 60, b % 60);
            if (L->stub) text_at(center("(STUB LEVEL - TO BE DESIGNED)"), 23, "(STUB LEVEL - TO BE DESIGNED)");
        } else {
            text_at(2, 20, L->error);
        }
    }
    keys_line(26, "PLAY", "BACK");
    if (confirm()) {
        if (unlocked(sel_arc, cursor + 1) && !level_load(L, sel_arc, cursor + 1)) {
            sfx(SFX_MENU_OK);
            sel_level = cursor + 1;
            SV.last_arc = (uint8_t)sel_arc;
            SV.last_level = (uint8_t)cursor;
            for (int p2 = 0; p2 < MAX_PLAYERS; p2++) carry[p2].lives = 3;
            title_ready = 0;
            box_key = -1;
            go(ST_INTRO);
        } else {
            sfx(SFX_HURT);
        }
    }
    if (back()) {
        text_clear_all();
        go(ST_ARCS);
        cursor = sel_arc;
    }
}

static void options_update(void)
{
    static const char *const diffs[3] = {"EASY  ", "NORMAL", "HARD  "};
    menu_scroll();
    plain_text();
    text_at(center("OPTIONS"), 4, "OPTIONS");
    int n_items = dev_mode ? 9 : 8, c = menu_nav(n_items), back_item = n_items - 1;
    uint16_t p = pressed();
    textf_at(8, 7, "%c MUSIC           %s ", c == 0 ? '>' : ' ', opt_music ? "ON " : "OFF");
    textf_at(8, 9, "%c SOUND           %s ", c == 1 ? '>' : ' ', opt_sfx ? "ON " : "OFF");
    textf_at(8, 11, "%c DIFFICULTY      %s", c == 2 ? '>' : ' ', diffs[opt_diff]);
    textf_at(8, 13, "%c FRIENDLY FIRE   %s ", c == 3 ? '>' : ' ', MP.friendly_fire ? "ON " : "OFF");
    textf_at(8, 15, "%c SHARED VIEW     %s ", c == 4 ? '>' : ' ', MP.merge ? "ON " : "OFF");
    textf_at(8, 17, "%c 2-PLAYER SPLIT  %s", c == 5 ? '>' : ' ', MP.split_h ? "TOP/BOTTOM" : "LEFT/RIGHT");
    textf_at(8, 19, "%c ERASE SAVE            ", c == 6 ? '>' : ' ');
    if (dev_mode) textf_at(8, 21, "%c DEV: ALL LEVELS     %s ", c == 7 ? '>' : ' ', dev_unlock_all ? "ON " : "OFF");
    textf_at(8, dev_mode ? 23 : 21, "%c BACK                  ", c == back_item ? '>' : ' ');
    if (confirm() || (p & (RS_BTN_LEFT | RS_BTN_RIGHT))) {
        sfx(SFX_MENU_OK);
        if (c == 0) { opt_music ^= 1; audio_options(opt_music, opt_sfx); if (opt_music) music_play("title"); }
        if (c == 1) { opt_sfx ^= 1; audio_options(opt_music, opt_sfx); }
        if (c == 2) opt_diff = (opt_diff + ((p & RS_BTN_LEFT) ? 2 : 1)) % 3;
        if (c == 3) MP.friendly_fire ^= 1;
        if (c == 4) MP.merge ^= 1;
        if (c == 5) MP.split_h ^= 1;
        if (c == 6 && confirm()) {
            memset(SV.cleared, 0, sizeof SV.cleared);
            memset(SV.best, 0, sizeof SV.best);
            unlock_all = 0;
            text_at(10, 25, "SAVE ERASED");
        }
        if (dev_mode && c == 7) dev_unlock_all ^= 1;
        save_store();
        if (c == back_item && confirm()) { text_clear_all(); go(ST_TITLE); cursor = 2; }
    }
    if (back()) { text_clear_all(); go(ST_TITLE); cursor = 2; }
}

static void credits_update(void)
{
    menu_scroll();
    plain_text();
    static const char *const lines[] = {
        "BOMBER MOLE", "", "A GAME BY 8BCRAFT", "(PIERRE-LOUIS BOYER)", "",
        "FOR THE RETROSTONE VIRTUAL CONSOLE", "", "MUSIC PLAYER: LIBXMP-LITE (MIT)",
        "PLACEHOLDER ART AND SOUND", "GENERATED BY SCRIPTS", ""};
    unsigned n = sizeof lines / sizeof lines[0];
    for (unsigned i = 0; i < n; i++)
        text_at(center(lines[i]), 6 + (int)i * 1 + (i > 0 ? 1 : 0), lines[i]);
    keys_line(6 + (int)n + 1, NULL, NULL);
    if (confirm() || back()) { text_clear_all(); go(ST_TITLE); cursor = 3; }
}

/* ---- multiplayer menus: join (story or battle), the battle ---- */
static void join_update(void)
{
    menu_scroll();
    int r = mp_join_update(join_mode);
    if (r < 0) {                                     /* P1 backed out: battle to the title, story to the seasons, alone */
        text_clear_all();
        MP.mode = MODE_SOLO;
        MP.nplayers = 1;
        if (join_mode == MODE_BATTLE) { go(ST_TITLE); cursor = 1; }
        else { go(ST_ARCS); cursor = 4; }
        return;
    }
    if (r == 0) return;
    text_clear_all();
    if (MP.mode == MODE_BATTLE) { go(ST_BATTLE); mp_battle_begin(); return; }
    go(ST_ARCS);
    cursor = sel_arc;
}

static void battle_update(void)
{
    if (!mp_battle_playing() && title_ready) menu_scroll();
    if (mp_battle_update()) {
        mp_views_off();
        MP.mode = MODE_SOLO;
        title_ready = 0;
        text_clear_all();
        go(ST_TITLE);
        cursor = 1;
    }
}

/* ---- level flow ------------------------------------------------------------------------------------- */
static int player_screen_xy(int *x, int *y)
{
    actor *m = world_player(0);
    if (!m) { *x = 160; *y = 128; return 0; }
    *x = m->cx * CELL + (m->tx - m->cx) * m->prog / (SUB / CELL) + 8;
    *y = HUD_H + m->cy * CELL + (m->ty - m->cy) * m->prog / (SUB / CELL) + 6;
    return 1;
}

static void place_scene_bombs(void)
{
    /* debug scene for screenshots and benchmarks: a chain of bombs in the mole's row */
    actor *m = world_player(0);
    if (!m) return;
    int n = 0;
    for (int x = 1; x < GW - 1 && n < 7; x += 2) {
        int y = m->cy;
        if (!terrain_walkable(W.g[m->depth][y][x].t, 0) || (x == m->cx)) continue;
        for (int i = 0; i < MAX_BOMBS; i++)
            if (!W.b[i].active) {
                bomb *b = &W.b[i];
                memset(b, 0, sizeof *b);
                b->active = 1; b->owner = 255; b->range = 3; b->depth = m->depth;
                b->cx = b->tx = (int8_t)x; b->cy = b->ty = (int8_t)y;
                b->fuse = (int16_t)(n == 0 ? 40 : 400);
                break;
            }
        n++;
    }
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (W.g[m->depth][y][x].t == TR_FLOOR && (x * 7 + y * 3) % 11 == 0 && n < 12) {
                for (int i = 0; i < MAX_BOMBS; i++)
                    if (!W.b[i].active) {
                        bomb *b = &W.b[i];
                        memset(b, 0, sizeof *b);
                        b->active = 1; b->owner = 255; b->range = 2; b->depth = m->depth;
                        b->cx = b->tx = (int8_t)x; b->cy = b->ty = (int8_t)y;
                        b->fuse = (int16_t)(60 + n * 6);
                        break;
                    }
                n++;
            }
    m->invul = 600;
}

static void start_level(void)
{
    draw_init_vram(LV.season, LV.boss);
    world_start(&LV, carry);
    draw_variant_pals();
    ui_init_level();
    if (rs_option_int("foxrest", 0))                /* debug: the fox starts resting (tests) */
        for (int i = 0; i < W.na; i++)
            if (W.a[i].kind == AK_BOSS) { W.a[i].state = 2; W.a[i].timer = 400; }
    if (rs_option_int("owlperch", 0))               /* debug: the owl starts on its perch (tests) */
        for (int i = 0; i < W.na; i++)
            if (W.a[i].kind == AK_BOSS) { W.a[i].state = OWL_PERCHED; W.a[i].timer = 600; }
    if (rs_option_int("owlhp", 0))                  /* debug: the owl's health (tests: phase 2) */
        for (int i = 0; i < W.na; i++)
            if (W.a[i].kind == AK_BOSS) W.a[i].hp = (int16_t)rs_option_int("owlhp", 5);
    if (rs_option_int("nocrates", 0))               /* debug: the farmer's crates are gone (screenshots) */
        for (int d = 0; d < NDEPTH; d++)
            for (int y = 0; y < GH; y++)
                for (int x = 0; x < GW; x++)
                    if (W.g[d][y][x].t == TR_CRATE) W.g[d][y][x].t = TR_FLOOR;
    if (rs_option_int("opengrubs", 0)) {            /* debug: all grubs taken (screenshots) */
        for (int d = 0; d < NDEPTH; d++)
            for (int y = 0; y < GH; y++)
                for (int x = 0; x < GW; x++)
                    if (W.g[d][y][x].item == IT_GRUB) W.g[d][y][x].item = IT_NONE;
        W.grubs_left = 0;
        W.exit_open = 1;
        ui_banner_exit_open();
    }
    const char *sp = rs_option("spawn");             /* debug: "1,5,9" = depth, x, y of the mole */
    if (sp) {
        int d, x, y;
        actor *m = world_player(0);
        if (m && sscanf(sp, "%d,%d,%d", &d, &x, &y) == 3) {
            m->depth = (uint8_t)clampi(d, 0, NDEPTH - 1);
            m->cx = m->tx = (int8_t)x;
            m->cy = m->ty = (int8_t)y;
        }
    }
    if (dev_mode) {
        char name[48];
        snprintf(name, sizeof name, "levels/%s", LV.file);
        rs_log("dev: level %s from %s", LV.file, rs_asset_origin(name));
    }
    view_depth = world_player_depth(0);
    if (forced_view >= 0) view_depth = forced_view;
    view_slot = 0;
    draw_playfield_full(view_depth, view_slot);
    set_scroll_slot(slot_y(view_slot));
    text_clear_all();
    level_frames = 0;
    music_play(LV.music);
    const char *scene = rs_option("scene");
    if (scene && !strcmp(scene, "chain")) place_scene_bombs();
    if (W.mode != MODE_SOLO) mp_level_start();          /* split screen: every depth drawn, a view per mole */
    else mp_mole_palettes();                          /* alone: P1's helmet (red unless chosen on the join screen) */
}

static void intro_update(void)
{
    if (st_t == 0) start_level();
    int px, py;
    player_screen_xy(&px, &py);
    int r = st_t * 8;
    iris_set(r < 420, px, py, r);
    ui_play_overlays(view_depth);
    if (W.mode != MODE_SOLO) {                       /* co-op: a box over the views, any player starts */
        iris_set(0, 0, 0, 0);
        int key = sel_arc * 16 + sel_level;
        if (rs_option_int("nointro", 0) || box_key == key) { mp_box_off(); go(ST_PLAY); return; }
        mp_start_box(&LV);
        if (st_t > 20 && (pressed() & (RS_BTN_A | RS_BTN_START))) { sfx(SFX_MENU_OK); box_key = key; mp_box_off(); go(ST_PLAY); }
        return;
    }
    if (rs_option_int("nointro", 0)) { iris_set(0, 0, 0, 0); go(ST_PLAY); return; }
    /* the game waits behind the start box until A (or Start); a retry of the same level skips it */
    int key = sel_arc * 16 + sel_level;
    if (box_key == key) {
        if (r >= 420) { iris_set(0, 0, 0, 0); go(ST_PLAY); }
        return;
    }
    ui_start_box(&LV);
    if (st_t > 20 && (pressed() & (RS_BTN_A | RS_BTN_START))) {
        sfx(SFX_MENU_OK);
        box_key = key;
        iris_set(0, 0, 0, 0);
        text_clear_all();
        go(ST_PLAY);
    }
}

static void begin_slide(int from, int to)
{
    slide_from = from;
    slide_to = to;
    draw_playfield_full(to, view_slot ^ 1);
    sfx(SFX_DEPTH);
    go(ST_SLIDE);
}

static void play_update(void)
{
    if (pressed() & RS_BTN_START) { sfx(SFX_MENU_OK); go(ST_PAUSE); cursor = 0; pause_quit = 0; return; }
    world_update();
    level_frames++;
    if (W.mode != MODE_SOLO) mp_update_views();
    if (W.events & EV_EXIT_OPEN) ui_banner_exit_open();
    if (W.events & EV_PICKUP) ui_pickup_banner(W.pickup);
    if (dev_skip) { dev_skip = 0; W.events |= EV_EXIT; }     /* dev: skip the level */
    if (pending_depth >= 0) {
        int from = pending_from, to = pending_depth;
        pending_depth = pending_from = -1;
        actor *m = world_player(0);
        if (m) {
            m->depth = (uint8_t)to;
            m->moving = 0;
            m->tx = m->cx;
            m->ty = m->cy;
        }
        if (forced_view < 0) { text_clear_all(); begin_slide(from, to); return; }
    }
    if (W.events & EV_DEAD) {
        carry[0].lives = W.ps[0].lives - 1;
        text_clear_all();
        go(ST_DYING);
        return;
    }
    if (W.events & EV_EXIT) {
        sfx(SFX_EXIT_OPEN);
        text_clear_all();
        go(ST_OUTRO);
        return;
    }
    if (forced_view < 0) view_depth = world_player_depth(0);
    if (W.mode == MODE_SOLO) ui_play_overlays(view_depth);
}

static int ease(int t, int n) { return t * t * (3 * n - 2 * t) / (n * n); }  /* smoothstep, 0..n */

static void slide_update(void)
{
    const int N = 36;
    if (st_t >= N) {
        view_slot ^= 1;
        view_depth = slide_to;
        set_scroll_slot(slot_y(view_slot));
        go(ST_PLAY);
    }
}

/* dev: move the mole to the next depth (the nearest open cell there) */
static void dev_next_depth(void)
{
    actor *m = world_player(0);
    if (!m || st != ST_PLAY) return;
    int from = m->depth, to = (m->depth + 1) % NDEPTH, best = -1, bx = m->cx, by = m->cy;
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int t = W.g[to][y][x].t;
            if (!terrain_walkable(t, 0) || t == TR_WATER) continue;
            int dd = abs(x - m->cx) + abs(y - m->cy);
            if (best < 0 || dd < best) { best = dd; bx = x; by = y; }
        }
    if (best < 0) return;
    m->depth = (uint8_t)to;
    m->cx = m->tx = (int8_t)bx;
    m->cy = m->ty = (int8_t)by;
    m->moving = 0;
    text_clear_all();
    begin_slide(from, to);
}

static void dev_all_powerups(void)
{
    pstats *ps = &W.ps[0];
    ps->bombs = 8; ps->range = 8; ps->speed = 3; ps->remote = 1; ps->hearts = 3;
    sfx(SFX_POWERUP);
}

/* dev: reload the level's text file (desktop: games/<id>/levels next to the exe, or the data dir) */
static void dev_reload(void)
{
    static level_def tmp;
    char name[64];
    snprintf(name, sizeof name, "levels/%s-%d.txt", season_name(sel_arc), sel_level);
    rs_asset_reload(name, NULL);
    if (level_load(&tmp, sel_arc, sel_level)) { rs_log("reload %s: %s", name, tmp.error); sfx(SFX_HURT); return; }
    LV = tmp;
    rs_log("reloaded %s", name);
    text_clear_all();
    ui_screen_done();
    rs_math(RS_MATH_ADD | RS_MATH_HALF, RS_MATH_BG2, 0);
    go(ST_INTRO);
}

/* dev keys (desktop F1..F7): god, power-ups, skip, depth, reload, reveal, frame-time overlay */
static void dev_keys(void)
{
    int k = rs_dev_key();
    int in_level = st == ST_PLAY || st == ST_PAUSE || st == ST_SLIDE;
    switch (k) {
    case 1: dev_god ^= 1; sfx(SFX_MENU_OK); break;
    case 2: if (in_level) dev_all_powerups(); break;
    case 3: if (st == ST_PLAY) dev_skip = 1; break;
    case 4: dev_next_depth(); break;
    case 5: if (in_level || st == ST_INTRO) dev_reload(); break;
    case 6: dev_reveal ^= 1; sfx(SFX_MENU_OK); break;
    case 7: dev_perf ^= 1; break;
    default: break;
    }
}

static void pause_resume(void)
{
    if (W.mode != MODE_SOLO) mp_box_off();
    text_clear_all();
    ui_screen_done();
    rs_math(RS_MATH_ADD | RS_MATH_HALF, RS_MATH_BG2, 0);
    st = ST_PLAY;
}

/* Pause: Left/Right (and Up/Down) move the cursor, A or Start picks the highlighted item (the cursor
 * starts on RESUME, so Start still resumes), B or Esc resumes. QUIT asks first. In dev mode a second
 * row holds the cheats. */
static void pause_update(void)
{
    uint16_t p = pressed();
    int n = dev_mode ? 3 + UI_DEV_ITEMS : 3;
    if (pause_quit) {                                /* "QUIT TO TITLE?" YES / NO (NO first) */
        if (p & (RS_BTN_LEFT | RS_BTN_RIGHT | RS_BTN_UP | RS_BTN_DOWN)) { pause_quit ^= 3; sfx(SFX_MENU_MOVE); }
        if (W.mode != MODE_SOLO) mp_pause_box(cursor, pause_quit);
        else ui_pause_screen(cursor, dev_mode, pause_quit);
        if (p & (RS_BTN_A | RS_BTN_START)) {
            sfx(SFX_MENU_OK);
            if (pause_quit == 1) {                   /* YES */
                pause_resume();
                mp_views_off();
                title_ready = 0;
                text_clear_all();
                go(ST_TITLE);
                return;
            }
            pause_quit = 0;
        } else if (p & (RS_BTN_B | RS_BTN_SELECT)) {
            pause_quit = 0;
        }
        return;
    }
    int old = cursor;
    if (p & (RS_BTN_LEFT | RS_BTN_UP)) cursor = (cursor + n - 1) % n;
    if (p & (RS_BTN_RIGHT | RS_BTN_DOWN)) cursor = (cursor + 1) % n;
    if (cursor != old) sfx(SFX_MENU_MOVE);
    if (W.mode != MODE_SOLO) mp_pause_box(cursor % 3, 0);
    else ui_pause_screen(cursor, dev_mode, 0);
    if (p & (RS_BTN_B | RS_BTN_SELECT)) { sfx(SFX_MENU_OK); pause_resume(); return; }
    if (!(p & (RS_BTN_A | RS_BTN_START))) return;
    sfx(SFX_MENU_OK);
    switch (cursor) {
    case 0: pause_resume(); return;
    case 1: restarts++; pause_resume(); go(ST_INTRO); return;
    case 2: pause_quit = 2; return;                  /* ask, with NO highlighted */
    case 3: dev_god ^= 1; return;
    case 4: dev_all_powerups(); return;
    case 5: pause_resume(); dev_skip = 1; return;
    case 6: pause_resume(); dev_next_depth(); return;
    case 7: dev_reveal ^= 1; return;
    case 8: dev_perf ^= 1; return;
    default: return;
    }
}

static void dying_update(void)
{
    int px, py;
    player_screen_xy(&px, &py);
    if (W.mode == MODE_SOLO) iris_set(1, px, py, 420 - st_t * 10);
    if (st_t >= 42) {
        iris_set(0, 0, 0, 0);
        if (W.mode != MODE_SOLO) mp_views_off();
        if (carry[0].lives > 0) go(ST_INTRO);
        else go(ST_GAMEOVER);
    }
}

static void gameover_update(void)
{
    if (st_t == 0) { rs_oam_clear(); text_clear_all(); iris_set(1, 160, 128, 0); rs_music_stop(); }
    text_box(10, 9, 20, 10);
    text_at(center("GAME OVER"), 10, "GAME OVER");
    int c = menu_nav(2);
    textf_at(13, 13, "%c CONTINUE", c == 0 ? '>' : ' ');
    textf_at(13, 15, "%c QUIT", c == 1 ? '>' : ' ');
    plain_text();
    iris_set(0, 0, 0, 0);
    rs_clip_black(0);
    if (confirm()) {
        text_clear_all();
        if (c == 0) { carry[0].lives = 3; go(ST_INTRO); }
        else { title_ready = 0; go(ST_LEVELS); cursor = sel_level - 1; }
    }
}

static void outro_update(void)
{
    int px, py;
    player_screen_xy(&px, &py);
    if (W.mode == MODE_SOLO) iris_set(1, px, py, 420 - st_t * 12);
    if (st_t >= 36) {
        iris_set(0, 0, 0, 0);
        if (W.mode != MODE_SOLO) mp_views_off();
        int secs = level_frames / 60;
        int a = sel_arc, n = sel_level - 1;
        if (!dev_mode) {                            /* dev mode never writes the save's progress */
            if (!SV.best[a][n] || secs < SV.best[a][n]) SV.best[a][n] = (uint16_t)(secs ? secs : 1);
            if (SV.cleared[a] < sel_level) SV.cleared[a] = (uint8_t)sel_level;
            save_store();
        }
        carry[0].lives = W.ps[0].lives;
        go(ST_CLEAR);
    }
}

static void clear_update(void)
{
    if (st_t == 0) { rs_oam_clear(); text_clear_all(); rs_clip_black(0); }
    int secs = level_frames / 60, b = SV.best[sel_arc][sel_level - 1];
    text_box(9, 8, 22, 11);
    text_at(center("LEVEL CLEAR!"), 9, "LEVEL CLEAR!");
    textf_at(center(LV.name), 11, "%s", LV.name);
    textf_at(12, 13, "TIME   %2d:%02d", secs / 60, secs % 60);
    textf_at(12, 14, "BEST   %2d:%02d", b / 60, b % 60);
    textf_at(12, 15, "GRUBS  %2d", W.grubs_total);
    char k[32];
    snprintf(k, sizeof k, "PRESS %s", btn_name(RS_BTN_A, prompt_port));
    text_at(center(k), 17, k);
    plain_text();
    rs_brightness(15);
    if (st_t > 30 && confirm()) {
        text_clear_all();
        if (sel_level >= 8) { go(ST_ARCDONE); return; }
        sel_level++;
        if (!level_load(&LV, sel_arc, sel_level)) go(ST_INTRO);
        else { title_ready = 0; go(ST_LEVELS); }
    }
}

/* the arc final screen: a seasonal emblem spinning on the affine layer */
static void arcdone_update(void)
{
    static rs_affine m;
    if (st_t == 0) {
        rs_oam_clear();
        text_clear_all();
        rs_bg_enable(RS_BG3, 0);
        rs_bg_enable(RS_BG4, 0);
        rs_bg_setup(RS_BG2, 32, 32, 1024);
        rs_bg_enable(RS_BG2, 1);
        rs_bg_window(RS_BG2, 0);
        rs_bg_line_scroll(RS_BG2, NULL, NULL);
        rs_math(RS_MATH_OFF, 0, 0);
        const uint16_t (*T)[4] = bm_terrain_meta[LV.season];
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                int dx = x * 2 - 15, dy = y * 2 - 15, r2 = dx * dx + dy * dy;
                const uint16_t *mt = r2 < 30 ? T[T_EXIT_OPEN] : r2 < 90 ? T[T_GRASS] : r2 < 170 ? T[T_SOFT_DIRT] :
                                     r2 < 230 ? T[T_ROOTS] : T[T_TUNNEL];
                rs_bg_meta(RS_BG2, x, y, mt);
            }
        music_play("title");
    }
    /* integer sine table (8.8), 64 steps per turn */
    static const int16_t S[16] = {0, 25, 50, 74, 98, 121, 142, 162, 181, 198, 213, 226, 237, 245, 251, 255};
    int ang = (int)(st_t / 2) & 63, q = ang >> 4, i = ang & 15;
    int s = q == 0 ? S[i] : q == 1 ? S[15 - i] : q == 2 ? -S[i] : -S[15 - i];
    int c = q == 0 ? S[15 - i] : q == 1 ? -S[i] : q == 2 ? -S[15 - i] : S[i];
    int zoom = 256 + 96 * ((st_t % 128) < 64 ? (st_t % 64) : 64 - (st_t % 64)) / 64;
    m.a = c * 256 / zoom;
    m.b = -s * 256 / zoom;
    m.c = s * 256 / zoom;
    m.d = c * 256 / zoom;
    m.cx = 128;
    m.cy = 128;
    m.wrap = 1;
    rs_bg_scroll(RS_BG2, 128 - 160, 128 - 128);
    rs_bg_affine(RS_BG2, &m);
    static const char *const names[4] = {"SPRING", "SUMMER", "AUTUMN", "WINTER"};
    text_box(10, 24, 20, 4);
    textf_at(center("SPRING COMPLETE!"), 25, "%s COMPLETE!", names[sel_arc]);
    char k[32];
    snprintf(k, sizeof k, "PRESS %s", btn_name(RS_BTN_A, prompt_port));
    text_at(center(k), 26, k);
    plain_text();
    if (st_t > 60 && confirm()) {
        rs_bg_affine(RS_BG2, NULL);
        title_ready = 0;
        text_clear_all();
        go(ST_ARCS);
        cursor = sel_arc < 3 ? sel_arc + 1 : 3;
    }
}

/* ---- rs_game callbacks ---------------------------------------------------------------------------------- */
static void game_init(void)
{
    save_load();
    audio_options(opt_music, opt_sfx);
    sfx_init();
    if (rs_option_int("unlock", 0)) unlock_all = 1;
    if (rs_option_int("dev", 0)) dev_mode = dev_unlock_all = 1;
    if (rs_option("difficulty")) opt_diff = clampi(rs_option_int("difficulty", 1), 0, 2);
    forced_view = rs_option_int("view", -1);
    if (rs_option_int("spritetest", 0)) {             /* test screen: walk cycles (facing tests) */
        draw_init_vram(SEASON_SPRING, 0);
        for (int l = 0; l < 4; l++) rs_bg_enable(l, 0);
        rs_backdrop(RS_HEX(0x203040));
        go(ST_SPRITETEST);
        return;
    }
    const char *mpm = rs_option("mp");
    if (mpm) {                                       /* multiplayer: "coop" (with level=) or "battle" (with arena=) */
        int n = clampi(rs_option_int("players", 2), 1, MAX_PLAYERS), cpus = clampi(rs_option_int("cpus", 0), 0, n);
        MP.mode = !strcmp(mpm, "battle") ? MODE_BATTLE : MODE_COOP;
        MP.nplayers = n;
        MP.friendly_fire = (uint8_t)rs_option_int("ff", 0);
        MP.merge = (uint8_t)rs_option_int("merge", 0);
        MP.split_h = (uint8_t)rs_option_int("splith", 0);
        MP.map3 = (uint8_t)rs_option_int("map3", 1);
        if (rs_option("rounds")) MP.rounds = clampi(rs_option_int("rounds", 3), 1, 9);
        if (rs_option("roundtime")) MP.round_time = clampi(rs_option_int("roundtime", 90), 1, 600) * 60;
        for (int p = 0; p < MAX_PLAYERS; p++) {
            MP.port[p] = (int8_t)(p < n - cpus ? p : -1);
            MP.cpu[p] = (uint8_t)(p >= n - cpus && p < n ? clampi(rs_option_int("skill", 2), 1, 3) : 0);
            MP.colour[p] = (uint8_t)p;
        }
        if (MP.mode == MODE_BATTLE) {
            const char *ar = rs_option("arena");
            int ai = ar ? mp_arena_index(ar) : 0;
            go(ST_BATTLE);
            mp_battle_quick(ai < 0 ? 0 : ai);
            return;
        }
    }
    if (rs_option_int("battlesim", 0)) {             /* the fairness test: rounds of 4 CPUs, no picture */
        const char *ar = rs_option("arena");
        mp_battle_sim(ar ? ar : "molehill-maze", rs_option_int("battlesim", 0));
        MP.mode = MODE_SOLO;
    }
    const char *scr = rs_option("screen");
    if (scr && !strcmp(scr, "join")) {               /* tests: the battle join screen, nobody in yet (story=1: story's) */
        join_mode = rs_option_int("story", 0) ? MODE_COOP : MODE_BATTLE;
        mp_join_begin(-1);
        menu_backdrop();
        mp_join_enter();
        go(ST_JOIN);
        return;
    }
    const char *lv = rs_option("level");
    if (lv) {                                        /* "spring-3": jump straight into a level */
        char season[16] = "";
        int n = 1;
        if (sscanf(lv, "%15[a-z]-%d", season, &n) == 2) {
            for (int a = 0; a < SEASONS; a++)
                if (!strcmp(season, season_name(a))) sel_arc = a;
            sel_level = n;
            for (int p = 0; p < MAX_PLAYERS; p++) carry[p].lives = 3;
            if (!level_load(&LV, sel_arc, sel_level)) { go(ST_INTRO); return; }
            rs_log("cannot start %s: %s", lv, LV.error);
        }
    }
    go(ST_TITLE);
}

static void game_update(void)
{
    gt++;
    st_changed = 0;
    if (dev_mode) dev_keys();
    switch (st) {
    case ST_TITLE: title_update(); break;
    case ST_ARCS: arcs_update(); break;
    case ST_LEVELS: levels_update(); break;
    case ST_OPTIONS: options_update(); break;
    case ST_CREDITS: credits_update(); break;
    case ST_INTRO: intro_update(); break;
    case ST_PLAY: play_update(); break;
    case ST_SLIDE: slide_update(); break;
    case ST_PAUSE: pause_update(); break;
    case ST_DYING: world_update(); dying_update(); break;
    case ST_GAMEOVER: gameover_update(); break;
    case ST_OUTRO: outro_update(); break;
    case ST_CLEAR: clear_update(); break;
    case ST_ARCDONE: arcdone_update(); break;
    case ST_SPRITETEST: break;
    case ST_JOIN: join_update(); break;
    case ST_BATTLE: battle_update(); break;
    }
    if (!st_changed) st_t++;
    if (rs_option_int("transition", 0) && st == ST_PLAY && st_t == 30 && view_depth == 0) {
        /* debug: slide to the next depth for screenshots */
        begin_slide(0, 1);
    }
}

static void draw_menu_sprites(void)
{
    rs_oam_clear();
    /* a mole runs across the meadow, a ferret after it */
    int x = (int)((gt * 2) % 460) - 60;
    spr_draw(SPR_MOLE_WALK_RIGHT + (int[]){1, 0, 2, 0}[(gt / 6) % 4], x, 176, 0, 2);
    spr_draw(SPR_FERRET_WALK_RIGHT + (gt / 8) % 2, x - 44, 176, 0, 2);
    spr_draw(SPR_BOMB + (gt / 8) % 3, 40, 176, 0, 2);
    spr_draw(SPR_GRUB + (gt / 16) % 2, 272, 176, 0, 2);
}

/* test screen (--opt spritetest=1): rows = ferret, cat, mole, dog walk cycles; columns = left frames,
 * then right frames, every 48 px from x = 8; rows every 48 px from y = 8 (bottom-aligned in 40 px) */
static void draw_sprite_test(void)
{
    static const int rows[4][2] = {{SPR_FERRET_WALK_LEFT, SPR_FERRET_WALK_RIGHT}, {SPR_CAT_WALK_LEFT, SPR_CAT_WALK_RIGHT},
                                   {SPR_MOLE_WALK_LEFT, SPR_MOLE_WALK_RIGHT}, {SPR_DOG_WALK_LEFT, SPR_DOG_WALK_RIGHT}};
    static const int frames[4] = {2, 2, 3, 2};
    rs_oam_clear();
    for (int r = 0; r < 4; r++)
        for (int side = 0; side < 2; side++)
            for (int f = 0; f < frames[r]; f++) {
                int s = rows[r][side] + f;
                int col = side * frames[r] + f;
                spr_draw(s, 8 + col * 48 + (40 - bm_spr[s].w) / 2, 8 + r * 48 + 40 - bm_spr[s].h, 0, 2);
            }
}

static void game_draw(void)
{
    if (st == ST_SPRITETEST) { draw_sprite_test(); return; }
    switch (st) {
    case ST_TITLE: case ST_ARCS: case ST_LEVELS: case ST_OPTIONS: case ST_CREDITS:
        if (title_ready) draw_menu_sprites();
        return;
    case ST_JOIN:
        mp_join_draw();
        return;
    case ST_BATTLE:
        if (mp_battle_playing()) mp_draw_play();
        else if (title_ready) draw_menu_sprites();
        return;
    case ST_GAMEOVER: case ST_CLEAR: case ST_ARCDONE:
        return;
    default:
        break;
    }
    if (W.mode != MODE_SOLO) { mp_draw_play(); return; }   /* co-op: the split screen */
    if (st == ST_SLIDE) {
        const int N = 36;
        int k = ease(st_t < N ? st_t : N, N);
        int dir = slide_to > slide_from ? 1 : -1;
        int off = k * 256 / N;
        set_scroll_slot(slot_y(view_slot) + dir * off);
        draw_cells_dirty(slide_to, view_slot ^ 1);
        draw_world_sprites(slide_from, -dir * off, 1);
        draw_world_sprites(slide_to, dir * (256 - off), 0);
        draw_weather(0);
    } else {
        draw_cells_dirty(view_depth, view_slot);
        set_scroll_slot(slot_y(view_slot));
        draw_world_sprites(view_depth, W.shake ? ((W.shake & 1) ? 1 : -1) : 0, 1);
        draw_weather(view_depth == 0);
    }
    for (int d = 0; d < NDEPTH; d++)
        if (d != view_depth && !(st == ST_SLIDE && d == slide_to)) {
            /* other depths redraw when shown; keep their dirty flags */
        }
    ui_glow_pulse(W.t);
    draw_hud();
    if (st == ST_PAUSE)
        rs_math(RS_MATH_SUB | RS_MATH_FIXED, RS_MATH_BG2 | RS_MATH_BG3 | RS_MATH_BG4 | RS_MATH_OBJ | RS_MATH_BACK,
                RS_RGB(10, 10, 10));
    /* dark levels: the helmet lamp */
    if (LV.dark && st != ST_INTRO && st != ST_DYING && st != ST_OUTRO) {
        int px, py;
        player_screen_xy(&px, &py);
        lamp_set(1, px, py, 60);
    } else if (LV.night && (st == ST_PLAY || st == ST_PAUSE || st == ST_SLIDE)) {  /* night: the helmet lamp flickers */
        int px, py;
        player_screen_xy(&px, &py);
        int flicker = ((W.t * 37u) % 211u) < 9 ? -6 : (int)((W.t / 6) % 3) - 1;
        night_set(1, px, py, LV.night + flicker);
    } else if (LV.fog && (st == ST_PLAY || st == ST_PAUSE || st == ST_SLIDE)) {   /* fog: a clear circle round the mole */
        int px, py;
        player_screen_xy(&px, &py);
        fog_set(1, px, py, LV.fog);
    } else {
        fog_set(0, 0, 0, 0);
    }
}

static void game_shutdown(void)
{
    if (!rs_option_int("dump", 0)) return;
    {                                                /* the prompts' button names, per port (by its device) */
        char s[160] = "buttons:";
        for (int p = 0; p < RS_PAD_MAX; p++) {
            char t[48];
            snprintf(t, sizeof t, " P%d=%s|%s|%s", p + 1, btn_name(RS_BTN_A, p), btn_name(RS_BTN_B, p), btn_name(RS_BTN_START, p));
            strcat(s, t);
        }
        rs_log("%s p1port=%d prompt=%d", s, MP.port[0], prompt_port);
    }
    if (W.mode != MODE_SOLO) mp_dump();
    if (st == ST_JOIN) mp_join_dump();
    actor *m = world_player(0);
    int rocks = 0, dirt = 0, bombs = 0, snow = 0, thin = 0;
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                rocks += W.g[d][y][x].t == TR_ROCK;
                snow += W.g[d][y][x].t == TR_SNOW;
                thin += W.g[d][y][x].t == TR_THIN_ICE;
                dirt += W.g[d][y][x].t == TR_DIRT;
            }
    for (int i = 0; i < MAX_BOMBS; i++) bombs += W.b[i].active;
    {
        int faces = 0, tiles = 0, jitter = 0;
        for (int i = 0; i < W.na; i++) {
            const actor *a = &W.a[i];
            if (a->kind != AK_FERRET && a->kind != AK_CAT && a->kind != AK_BOSS && a->kind != AK_DOG) continue;
            faces += a->face_changes;
            tiles += a->tiles_moved;
            jitter += a->jitter;
        }
        rs_log("facing: changes=%d tiles=%d jitter=%d", faces, tiles, jitter);
    }
    rs_log("state: st=%d level=%s grubs_left=%d/%d depth=%d x=%d y=%d hearts=%d lives=%d rocks=%d dirt=%d bombs=%d enemies=%d,%d,%d exit_open=%d",
           st, LV.file, W.grubs_left, W.grubs_total, m ? m->depth : -1, m ? m->cx : -1, m ? m->cy : -1,
           W.ps[0].hearts, W.ps[0].lives, rocks, dirt, bombs, world_enemies(0), world_enemies(1), world_enemies(2), W.exit_open);
    {
        int lanes[4];
        world_wind_lanes(lanes);
        rs_log("wind: lanes=%d,%d,%d,%d particles=%d,%d,%d,%d", lanes[0], lanes[1], lanes[2], lanes[3],
               W.wind_fx[0], W.wind_fx[1], W.wind_fx[2], W.wind_fx[3]);
        rs_log("menu: restarts=%d dev=%d god=%d lever1=%d remote=%d", restarts, dev_mode, dev_god, W.lever[1],
               W.ps[0].remote);
        int boss_depth = -1;
        for (int i = 0; i < W.na; i++)
            if (W.a[i].kind == AK_BOSS && W.a[i].alive) boss_depth = W.a[i].depth;
        rs_log("summer: burnt=%d stings=%d beekills=%d shaken=%d warns=%d crushed=%d gas=%d holes=%d bstuns=%d "
               "boss_depth=%d catsee=%d", W.stat.burnt, W.stat.stings, W.stat.bee_kills, W.stat.shaken, W.stat.warns,
               W.stat.crushed, W.stat.gas_stuns, W.stat.badger_holes, W.stat.badger_stuns, boss_depth,
               world_cats_seeing());
        rs_log("croc: bites=%d stuns=%d", W.stat.croc_bites, W.stat.croc_stuns);
        extern int g_fog_eyes;
        rs_log("autumn: pushes=%d smashes=%d plugs=%d apples=%d hops=%d bhops=%d rides=%d cartkills=%d anthome=%d "
               "antdrop=%d dashes=%d rests=%d foxhits=%d eyes=%d leafblow=%d glowing=%d", W.stat.pushes, W.stat.smashes,
               W.stat.plugs, W.stat.apple_stuns, W.stat.hops, W.stat.bomb_hops, W.stat.rides, W.stat.crushed_by_cart,
               W.stat.ants_home, W.stat.ants_dropped, W.stat.fox_dashes, W.stat.fox_rests, W.stat.fox_hits, g_fog_eyes,
               W.stat.leaves_blown, ui_glow_count(0));
        rs_log("winter: drowned=%d edrowned=%d thinbreaks=%d icebreaks=%d croccracks=%d drifts=%d rolls=%d grows=%d "
               "crushes=%d shatters=%d icicles=%d icehits=%d rides=%d cranks=%d swoops=%d swoophits=%d perches=%d "
               "owlhits=%d drops=%d phase2=%d bucket=%d safe=%d,%d,%d snow=%d thin=%d owl=%d", W.stat.drowned, W.stat.enemies_drowned,
               W.stat.thin_breaks, W.stat.ice_breaks, W.stat.croc_cracks, W.stat.drifts_made, W.stat.rolls,
               W.stat.ball_grows, W.stat.ball_crushes, W.stat.ball_shatters, W.stat.icicles_fallen, W.stat.icicle_hits,
               W.stat.bucket_rides, W.stat.cranks, W.stat.owl_swoops, W.stat.owl_swoop_hits, W.stat.owl_perches,
               W.stat.owl_hits, W.stat.owl_drops, W.stat.owl_phase2, W.buckets[1].at, W.safe_d[0], W.safe_x[0], W.safe_y[0],
               snow, thin, world_owl(NULL, NULL));
    }
}

/* ---- save states: the SDK saves the console (VRAM, maps, palettes, sprites, voices, music, RNG, pads); here
 * are the game's own objects, every file its own (tools/state_audit.py checks that no mutable static is
 * forgotten; games/bombermole/state_audit.txt lists the scratch buffers left out on purpose).
 * Not saved: SV, the battery save's copy (progress stays in the .srm: a state never takes it back), and
 * unlock_all, which follows it. ---- */
#define S(v) rs_state_var("main." #v, &(v), sizeof(v))
static void game_state(void)
{
    S(join_mode); S(opt_music); S(opt_sfx); S(opt_diff);
    S(st); S(st_t); S(cursor); S(sel_arc); S(sel_level); S(st_changed); S(col); S(shown);
    S(LV); S(carry); S(view_depth); S(slide_from); S(slide_to); S(level_frames); S(forced_view);
    S(title_ready); S(gt);
    S(dev_mode); S(dev_unlock_all); S(dev_skip); S(restarts); S(pause_quit); S(box_key);
    S(dev_god); S(dev_reveal); S(dev_perf);
    RS_STATE_RASTER(title_raster);
    draw_state();
    ui_state();
    mp_state();
    sfx_state();
    world_state();
}
#undef S

const rs_game *rs_game_main(void)
{
    /* state_version: bump it when the meaning of a saved object changes (its layout is checked) */
    static const rs_game g = {"Bomber Mole", "bombermole", "0.1.0", game_init, game_update, game_draw,
                              game_shutdown, bm_assets, game_state, NULL, 1};
    return &g;
}
