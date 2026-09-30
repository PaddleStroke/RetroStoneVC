/*
 * RetroStone VC SDK: desktop runner (SDL2), for Linux and Windows.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   <Game> [--scale N] [--fullscreen] [--data DIR] [--opt KEY=VALUE]...
 *
 * Keyboard, pad 1: arrows = D-pad, Z = B, X = A, C = Y, V = X, Q = L, E = R, Enter = Start,
 *                  Right Shift / Backspace / Esc = Select.
 *           pad 2: W A S D = D-pad, G = B, H = A, T = Start, R = Select (a second player on one keyboard).
 * F11 or Alt+Enter = fullscreen, F12 = screenshot, Esc = quit.
 * Game controllers: up to 4, SNES positions (bottom = B, right = A, left = Y, top = X).
 */
#include "rs_desktop.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_GameController *pads[RS_PAD_MAX];
static char base_dir[1024];

static void open_pads(void)
{
    int slot = 0;
    for (int i = 0; i < RS_PAD_MAX; i++) {
        if (pads[i]) SDL_GameControllerClose(pads[i]);
        pads[i] = NULL;
    }
    for (int i = 0; i < SDL_NumJoysticks() && slot < RS_PAD_MAX; i++)
        if (SDL_IsGameController(i)) {
            pads[slot] = SDL_GameControllerOpen(i);
            if (pads[slot]) {
                SDL_Log("pad %d: %s", slot + 1, SDL_GameControllerName(pads[slot]));
                slot++;
            }
        }
    /* what drives each port (button names): its pad if one is plugged in, else the keyboard (ports 1-2);
       then the last one used (frame loop) */
    for (int p = 0; p < RS_PAD_MAX; p++)
        rs_host_set_pad_device(p, pads[p] || p > 1 ? RS_DEVICE_PAD : p == 0 ? RS_DEVICE_KEYBOARD : RS_DEVICE_KEYBOARD2);
}

static uint16_t pad_state(SDL_GameController *c)
{
    static const struct { SDL_GameControllerButton b; uint16_t bit; } map[] = {
        {SDL_CONTROLLER_BUTTON_A, RS_BTN_B}, {SDL_CONTROLLER_BUTTON_B, RS_BTN_A},
        {SDL_CONTROLLER_BUTTON_X, RS_BTN_Y}, {SDL_CONTROLLER_BUTTON_Y, RS_BTN_X},
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, RS_BTN_L}, {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, RS_BTN_R},
        {SDL_CONTROLLER_BUTTON_BACK, RS_BTN_SELECT}, {SDL_CONTROLLER_BUTTON_START, RS_BTN_START},
        {SDL_CONTROLLER_BUTTON_DPAD_UP, RS_BTN_UP}, {SDL_CONTROLLER_BUTTON_DPAD_DOWN, RS_BTN_DOWN},
        {SDL_CONTROLLER_BUTTON_DPAD_LEFT, RS_BTN_LEFT}, {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, RS_BTN_RIGHT},
    };
    uint16_t b = 0;
    if (!c) return 0;
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
        if (SDL_GameControllerGetButton(c, map[i].b)) b |= map[i].bit;
    int ax = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    int ay = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
    if (ax < -16000) b |= RS_BTN_LEFT;
    if (ax > 16000) b |= RS_BTN_RIGHT;
    if (ay < -16000) b |= RS_BTN_UP;
    if (ay > 16000) b |= RS_BTN_DOWN;
    if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) b |= RS_BTN_L;
    if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) b |= RS_BTN_R;
    return b;
}

typedef struct { SDL_Scancode k; uint16_t bit; } keymap;
static const keymap KEYS_P1[] = {
    {SDL_SCANCODE_UP, RS_BTN_UP}, {SDL_SCANCODE_DOWN, RS_BTN_DOWN},
    {SDL_SCANCODE_LEFT, RS_BTN_LEFT}, {SDL_SCANCODE_RIGHT, RS_BTN_RIGHT},
    {SDL_SCANCODE_Z, RS_BTN_B}, {SDL_SCANCODE_X, RS_BTN_A},
    {SDL_SCANCODE_C, RS_BTN_Y}, {SDL_SCANCODE_V, RS_BTN_X},
    {SDL_SCANCODE_Q, RS_BTN_L}, {SDL_SCANCODE_E, RS_BTN_R},
    {SDL_SCANCODE_RETURN, RS_BTN_START}, {SDL_SCANCODE_KP_ENTER, RS_BTN_START},
    {SDL_SCANCODE_RSHIFT, RS_BTN_SELECT}, {SDL_SCANCODE_BACKSPACE, RS_BTN_SELECT},
    {SDL_SCANCODE_ESCAPE, RS_BTN_SELECT},           /* Esc = back / resume (close the window to quit) */
    {SDL_SCANCODE_UNKNOWN, 0}};
static const keymap KEYS_P2[] = {
    {SDL_SCANCODE_W, RS_BTN_UP}, {SDL_SCANCODE_S, RS_BTN_DOWN},
    {SDL_SCANCODE_A, RS_BTN_LEFT}, {SDL_SCANCODE_D, RS_BTN_RIGHT},
    {SDL_SCANCODE_G, RS_BTN_B}, {SDL_SCANCODE_H, RS_BTN_A},
    {SDL_SCANCODE_T, RS_BTN_START}, {SDL_SCANCODE_R, RS_BTN_SELECT},
    {SDL_SCANCODE_UNKNOWN, 0}};

static uint16_t keyboard_state(const keymap *map)
{
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    uint16_t b = 0;
    for (; map->bit; map++)
        if (k[map->k]) b |= map->bit;
    return b;
}

static void log_fn(const char *line) { SDL_Log("%s", line); }

static void path_in_base(char *out, size_t n, const char *name)
{
    snprintf(out, n, "%s%s", base_dir, name);
}

int main(int argc, char **argv)
{
    int scale = 3, fullscreen = 0;
    const char *data_dir = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fullscreen")) fullscreen = 1;
        else if (!strcmp(argv[i], "--data") && i + 1 < argc) data_dir = argv[++i];
        else if (!strcmp(argv[i], "--opt") && i + 1 < argc) rsd_option(argv[++i]);
        else if (!strcmp(argv[i], "--dev")) rsd_option("dev=1");       /* the game's developer mode */
    }
    if (scale < 1) scale = 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    char *bp = SDL_GetBasePath();
    snprintf(base_dir, sizeof base_dir, "%s", bp ? bp : "./");
    SDL_free(bp);

    const rs_game *g = rs_game_main();
    char sram_path[1200], data_default[1200], title[128];
    char name[160];
    snprintf(name, sizeof name, "%s.srm", g->id);
    path_in_base(sram_path, sizeof sram_path, name);
    path_in_base(data_default, sizeof data_default, "data");
    rsd_set_data_dir(data_dir ? data_dir : data_default);
    {   /* games/<id>/ next to the exe (or in the working directory): edited levels without a rebuild */
        char gd[1200], rel[200];
        snprintf(rel, sizeof rel, "games/%s", g->id);
        path_in_base(gd, sizeof gd, rel);
        rsd_add_data_dir(gd);
        rsd_add_data_dir(rel);
        /* developer mode (--dev): also the repository's copy, searching upwards from the exe's folder
           (a shortcut to dist\windows\*.exe picks up the edited games/<id>/levels of the repo) */
        if (rs_option_int("dev", 0)) {
            char up[1500], seg[64] = "";
            for (int k = 1; k <= 4; k++) {
                strcat(seg, "../");
                snprintf(up, sizeof up, "%s%s%s", base_dir, seg, rel);
                rsd_add_data_dir(up);
            }
        }
    }

    rs_host_set_log(log_fn);
    rs_host_set_file_loader(rsd_data_loader);
    rsd_sram_load(sram_path);
    rs_host_init(g);

    snprintf(title, sizeof title, "%s - RetroStone VC", g->name);
    SDL_Window *win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       RS_SCREEN_W * scale, RS_SCREEN_H * scale,
                                       SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    SDL_RenderSetLogicalSize(ren, RS_SCREEN_W, RS_SCREEN_H);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                         RS_SCREEN_W, RS_SCREEN_H);
    SDL_ShowCursor(fullscreen ? SDL_DISABLE : SDL_ENABLE);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = RS_AUDIO_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev) SDL_PauseAudioDevice(dev, 0);
    else SDL_Log("no audio: %s", SDL_GetError());

    open_pads();
    const uint64_t freq = SDL_GetPerformanceFrequency();
    const uint64_t frame_ticks = freq / RS_FPS;
    uint64_t next = SDL_GetPerformanceCounter();
    uint64_t sram_timer = 0;
    int running = 1, shot = 0;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED) open_pads();
            else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k >= SDLK_F1 && k <= SDLK_F10) rs_host_set_dev_key((int)(k - SDLK_F1) + 1);
                else if (k == SDLK_F11 || (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT))) {
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    SDL_ShowCursor(fullscreen ? SDL_DISABLE : SDL_ENABLE);
                } else if (k == SDLK_F12) {
                    char p[1200], n[64];
                    snprintf(n, sizeof n, "screenshot-%03d.png", shot++);
                    path_in_base(p, sizeof p, n);
                    if (!rsd_write_png(p, rs_host_framebuffer(), 2)) SDL_Log("saved %s", p);
                }
            }
        }

        /* fixed 60 Hz timestep: run as many frames as real time asks for */
        uint64_t now = SDL_GetPerformanceCounter();
        int steps = 0;
        if (now + frame_ticks * 8 < next || now > next + frame_ticks * 8) next = now; /* resync */
        while (now >= next && steps < 4) {
            uint16_t kb1 = keyboard_state(KEYS_P1), kb2 = keyboard_state(KEYS_P2);
            for (int p = 0; p < RS_PAD_MAX; p++) {
                uint16_t pb = pad_state(pads[p]), kb = p == 0 ? kb1 : p == 1 ? kb2 : 0;
                if (kb && !pb) rs_host_set_pad_device(p, p == 0 ? RS_DEVICE_KEYBOARD : RS_DEVICE_KEYBOARD2);
                else if (pb && !kb) rs_host_set_pad_device(p, RS_DEVICE_PAD);
                rs_host_set_pad(p, (uint16_t)(pb | kb), p <= 1 || pads[p]);
            }
            rs_host_frame();
            if (dev) {
                int n;
                const int16_t *a = rs_host_audio(&n);
                /* keep the queue short (< ~100 ms) to bound latency */
                if (SDL_GetQueuedAudioSize(dev) < (Uint32)(RS_AUDIO_RATE / 10) * 4)
                    SDL_QueueAudio(dev, a, (Uint32)n * 4);
            }
            next += frame_ticks;
            steps++;
        }
        if (steps) {
            SDL_UpdateTexture(tex, NULL, rs_host_framebuffer(), RS_SCREEN_W * 2);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
        } else {
            SDL_Delay(1);
        }
        if (rs_host_sram_dirty(0) && ++sram_timer > 30) {
            rs_host_sram_dirty(1);
            sram_timer = 0;
            if (rsd_sram_save(sram_path)) SDL_Log("cannot save %s", sram_path);
        }
    }
    if (rs_host_sram_dirty(1)) rsd_sram_save(sram_path);
    rs_host_shutdown();
    if (dev) SDL_CloseAudioDevice(dev);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
