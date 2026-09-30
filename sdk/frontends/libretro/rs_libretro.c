/*
 * RetroStone VC SDK: libretro frontend. Linking a game with this file and
 * librs.a gives <game>_libretro.so, which RetroStoneOS and RetroArch run as a
 * "no content" core.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "rs_host.h"
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
    info->geometry.base_width = RS_SCREEN_W;
    info->geometry.base_height = RS_SCREEN_H;
    info->geometry.max_width = RS_SCREEN_W;
    info->geometry.max_height = RS_SCREEN_H;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
    info->timing.fps = RS_FPS;
    info->timing.sample_rate = RS_AUDIO_RATE;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }

RETRO_API void retro_reset(void)
{
    if (started) rs_host_shutdown();
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

RETRO_API void retro_run(void)
{
    if (!started) retro_reset();
    poll_cb();
    for (unsigned p = 0; p < RS_PAD_MAX; p++) {
        rs_host_set_pad((int)p, read_pad(p), 1);
        rs_host_set_pad_device((int)p, RS_DEVICE_PAD);          /* a RetroPad: the buttons keep their names */
    }
    rs_host_frame();
    video_cb(rs_host_framebuffer(), RS_SCREEN_W, RS_SCREEN_H, RS_SCREEN_W * 2);
    int n = 0;
    const int16_t *a = rs_host_audio(&n);
    while (n > 0) {
        size_t done = audio_batch_cb(a, (size_t)n);
        if (!done) break;
        a += done * 2;
        n -= (int)done;
    }
}

RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
RETRO_API bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
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
