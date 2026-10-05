/*
 * RetroStone VC SDK: libretro frontend. Linking a game with this file and
 * librs.a gives <game>_libretro.so, which RetroStoneOS and RetroArch run as a
 * "no content" core.
 * Widescreen (docs/spec.md "Widescreen"): a game that opts in (rs_game.max_width) gets the core option
 * "rsvc_aspect" (Aspect: 4:3, 16:9 or Wide = 19.5:9; 4:3 by default, so RetroStoneOS is unaffected). The geometry
 * follows it; a change while the game runs restarts it at the new width from a save state (rs_host_change_width).
 * The language (docs/i18n.md): the core option "rsvc_language" (Language: Auto or one of the 22), Auto = the
 * frontend's: RetroStoneOS's variable "rsos_language" (every language, Danish too), else GET_LANGUAGE (libretro's enum
 * has no Danish), else English. It is read when the game starts (a change applies at the next start or reset).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "rs_host.h"
#include "rs_i18n.h"
#include "libretro.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static retro_environment_t        env_cb;
static retro_video_refresh_t      video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t         poll_cb;
static retro_input_state_t        input_cb;
static retro_log_printf_t         log_cb;
static const rs_game *game;
static int started;
static int cur_width = RS_SCREEN_W;           /* the width of the geometry reported */

static int game_wide(void) { return rs_game_main()->max_width > RS_SCREEN_W; }

/* the values of the core option "rsvc_language", in RS_LANG_* order after Auto (shown in the language itself) */
static const char *const lang_names[RS_LANG_COUNT] = {
    "English", "\xc4\x8c" "e\xc5\xa1tina", "Dansk", "Deutsch",
    "\xce\x95\xce\xbb\xce\xbb\xce\xb7\xce\xbd\xce\xb9\xce\xba\xce\xac", "Espa\xc3\xb1ol", "Suomi",
    "Fran\xc3\xa7" "ais", "Bahasa Indonesia", "Italiano", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
    "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4", "Norsk bokm\xc3\xa5l", "Nederlands", "Polski",
    "Portugu\xc3\xaas (Brasil)", "Portugu\xc3\xaas (Portugal)", "\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9",
    "Svenska", "T\xc3\xbcrk\xc3\xa7" "e", "\xd0\xa3\xd0\xba\xd1\x80\xd0\xb0\xd1\x97\xd0\xbd\xd1\x81\xd1\x8c\xd0\xba\xd0\xb0",
    "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87"};
static char lang_var[600];                      /* "Language; Auto|English|..." */

/* the player's language: the core option, else RetroStoneOS's rsos_language, else GET_LANGUAGE, else English */
static int option_lang(void)
{
    struct retro_variable v = {"rsvc_language", NULL};
    if (env_cb && env_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value)
        for (int l = 0; l < RS_LANG_COUNT; l++)
            if (!strcmp(v.value, lang_names[l])) return l;
    v.key = "rsos_language";
    v.value = NULL;
    if (env_cb && env_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value && rs_lang_find(v.value) >= 0)
        return rs_lang_find(v.value);
    unsigned rl = 0;
    if (env_cb && env_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &rl) && rs_lang_from_retro(rl) >= 0) return rs_lang_from_retro(rl);
    return RS_LANG_EN;
}

/* the width the core option asks for (320 when the game has not opted in) */
static int option_width(void)
{
    struct retro_variable v = {"rsvc_aspect", NULL};
    if (!game_wide() || !env_cb || !env_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) || !v.value) return RS_SCREEN_W;
    if (!strcmp(v.value, "16:9")) return rs_host_width_for(rs_game_main(), rs_host_width_for_aspect(16, 9));
    if (!strcmp(v.value, "Wide")) return rs_host_width_for(rs_game_main(), rs_host_width_for_aspect(39, 18));
    return RS_SCREEN_W;
}

static void log_bridge(const char *line)
{
    if (log_cb) log_cb(RETRO_LOG_INFO, "%s\n", line);
    else fprintf(stderr, "%s\n", line);
}

RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }

RETRO_API void retro_set_environment(retro_environment_t cb)
{
    env_cb = cb;
    bool no_game = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
    static const struct retro_controller_description pads[] = {
        {"RetroStone pad (SNES layout)", RETRO_DEVICE_JOYPAD},
    };
    static const struct retro_controller_info ports[] = {
        {pads, 1}, {pads, 1}, {pads, 1}, {pads, 1}, {NULL, 0},
    };
    cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void *)ports);
    static const struct retro_input_descriptor desc[] = {
#define D(p, id, name) {p, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_##id, name}
#define PORT(p) D(p, LEFT, "Left"), D(p, UP, "Up"), D(p, DOWN, "Down"), D(p, RIGHT, "Right"), \
        D(p, B, "B"), D(p, A, "A"), D(p, Y, "Y"), D(p, X, "X"), D(p, L, "L"), D(p, R, "R"), \
        D(p, SELECT, "Select"), D(p, START, "Start")
        PORT(0), PORT(1), PORT(2), PORT(3),
#undef PORT
#undef D
        {0, 0, 0, 0, NULL},
    };
    cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void *)desc);
    {   /* the core options: the language (every game), the aspect (the widescreen games) */
        size_t k = (size_t)snprintf(lang_var, sizeof lang_var, "Language; Auto");
        for (int l = 0; l < RS_LANG_COUNT && k < sizeof lang_var; l++)
            k += (size_t)snprintf(lang_var + k, sizeof lang_var - k, "|%s", lang_names[l]);
        static struct retro_variable vars[3];
        int n = 0;
        if (game_wide()) vars[n++] = (struct retro_variable){"rsvc_aspect", "Aspect (widescreen); 4:3|16:9|Wide"};
        vars[n++] = (struct retro_variable){"rsvc_language", lang_var};
        vars[n] = (struct retro_variable){NULL, NULL};
        cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)vars);
    }
}
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }

RETRO_API void retro_init(void)
{
    struct retro_log_callback l;
    if (env_cb && env_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &l)) log_cb = l.log;
    rs_host_set_log(log_bridge);
    game = rs_game_main();
}

RETRO_API void retro_deinit(void)
{
    if (started) rs_host_shutdown();
    started = 0;
}

RETRO_API void retro_get_system_info(struct retro_system_info *info)
{
    const rs_game *g = rs_game_main();
    memset(info, 0, sizeof *info);
    info->library_name = g->name;
    info->library_version = g->version ? g->version : RS_VERSION;
    info->valid_extensions = "";
    info->need_fullpath = false;
    info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
    memset(info, 0, sizeof *info);
    info->geometry.base_width = (unsigned)cur_width;
    info->geometry.base_height = RS_SCREEN_H;
    info->geometry.max_width = (unsigned)cur_width;
    info->geometry.max_height = RS_SCREEN_H;
    info->geometry.aspect_ratio = cur_width == RS_SCREEN_W ? 4.0f / 3.0f : (float)cur_width / RS_SCREEN_H;
    info->timing.fps = RS_FPS;
    info->timing.sample_rate = RS_AUDIO_RATE;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }

RETRO_API void retro_reset(void)
{
    if (started) rs_host_shutdown();
    rs_host_set_lang(option_lang());                /* the language: at every start (docs/i18n.md) */
    rs_host_init(game);
    started = 1;
}

static uint16_t read_pad(unsigned p)
{
    static const struct { unsigned id; uint16_t bit; } map[] = {
        {RETRO_DEVICE_ID_JOYPAD_B, RS_BTN_B},         {RETRO_DEVICE_ID_JOYPAD_Y, RS_BTN_Y},
        {RETRO_DEVICE_ID_JOYPAD_SELECT, RS_BTN_SELECT}, {RETRO_DEVICE_ID_JOYPAD_START, RS_BTN_START},
        {RETRO_DEVICE_ID_JOYPAD_UP, RS_BTN_UP},       {RETRO_DEVICE_ID_JOYPAD_DOWN, RS_BTN_DOWN},
        {RETRO_DEVICE_ID_JOYPAD_LEFT, RS_BTN_LEFT},   {RETRO_DEVICE_ID_JOYPAD_RIGHT, RS_BTN_RIGHT},
        {RETRO_DEVICE_ID_JOYPAD_A, RS_BTN_A},         {RETRO_DEVICE_ID_JOYPAD_X, RS_BTN_X},
        {RETRO_DEVICE_ID_JOYPAD_L, RS_BTN_L},         {RETRO_DEVICE_ID_JOYPAD_R, RS_BTN_R},
    };
    uint16_t b = 0;
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
        if (input_cb(p, RETRO_DEVICE_JOYPAD, 0, map[i].id)) b |= map[i].bit;
    return b;
}

/* the aspect option changed: the new width (restarting a running game from a save state), the new geometry */
static void check_width(void)
{
    bool changed = false;
    if (!game_wide() || !env_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &changed) || !changed) return;
    int w = option_width();
    if (w == cur_width) return;
    if (started) w = rs_host_change_width(w);
    else rs_host_set_width(w);
    if (w == cur_width) return;
    cur_width = w;
    struct retro_system_av_info av;
    retro_get_system_av_info(&av);
    env_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
}

RETRO_API void retro_run(void)
{
    check_width();
    if (!started) retro_reset();
    poll_cb();
    for (unsigned p = 0; p < RS_PAD_MAX; p++) {
        rs_host_set_pad((int)p, read_pad(p), 1);
        rs_host_set_pad_device((int)p, RS_DEVICE_PAD);          /* a RetroPad: the buttons keep their names */
    }
    rs_host_frame();
    video_cb(rs_host_framebuffer(), (unsigned)rs_screen_width(), RS_SCREEN_H, (size_t)rs_screen_width() * 2);
    int n = 0;
    const int16_t *a = rs_host_audio(&n);
    while (n > 0) {
        size_t done = audio_batch_cb(a, (size_t)n);
        if (!done) break;
        a += done * 2;
        n -= (int)done;
    }
}

/* Save states (docs/spec.md "Save states"): a fixed size per game build, 0 when the game has none. */
RETRO_API size_t retro_serialize_size(void) { return rs_host_state_size(rs_game_main()); }
RETRO_API bool retro_serialize(void *data, size_t size)
{
    /* before the first frame there is nothing to save (and starting now would read the save RAM too early) */
    if (!started || !rs_host_state_size(rs_game_main())) return false;
    return rs_host_state_save(data, size) != 0;
}
RETRO_API bool retro_unserialize(const void *data, size_t size)
{
    if (!rs_host_state_size(rs_game_main())) return false;
    if (!started) retro_reset();            /* a state loaded before the first frame (resume at start-up) */
    return rs_host_state_load(data, size) == 0;
}
RETRO_API void retro_cheat_reset(void) {}
RETRO_API void retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }

RETRO_API bool retro_load_game(const struct retro_game_info *info)
{
    (void)info;
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    if (!env_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        log_bridge("[rs] RGB565 is not supported by the frontend");
        return false;
    }
    if (rs_host_state_size(rs_game_main())) {  /* the game's objects are saved raw: same platform only */
        uint64_t quirks = RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT | RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT;
        env_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &quirks);
    }
    /* the width: from the core option, before the frontend asks for the geometry */
    cur_width = option_width();
    rs_host_set_width(cur_width);
    /* The runtime starts at the first retro_run(), after the frontend has
     * loaded the save RAM into retro_get_memory_data(). */
    started = 0;
    return true;
}
RETRO_API bool retro_load_game_special(unsigned t, const struct retro_game_info *i, size_t n)
{
    (void)t; (void)i; (void)n;
    return false;
}
RETRO_API void retro_unload_game(void)
{
    if (started) rs_host_shutdown();
    started = 0;
}
RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

RETRO_API void *retro_get_memory_data(unsigned id)
{
    return id == RETRO_MEMORY_SAVE_RAM ? rs_host_sram() : NULL;
}
RETRO_API size_t retro_get_memory_size(unsigned id)
{
    return id == RETRO_MEMORY_SAVE_RAM ? RS_SRAM_SIZE : 0;
}
