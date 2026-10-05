/*
 * RetroStone Arcade: the SDL2 frontend, for Android (the app's libmain.so) and the desktop (Linux, Windows).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   desktop: <App> [--scale N] [--fullscreen] [--saves DIR] [--opt KEY=VALUE]...
 *
 * Picture: the screen fills the display's height. When the display is a whole multiple of 240 px high (or close:
 *   within 10%), the pixels are scaled by that integer; otherwise the picture is drawn at the next integer size
 *   and smoothed down ("sharp bilinear": crisp pixels, even rows). A game narrower than the display (320 px,
 *   no widescreen) gets side borders: a blurred, darkened copy of its own picture.
 * Input: touches (Android) or the mouse (desktop) go to the runtime (apps/launcher/games.cfg maps them to the pad),
 *   the keyboard as the SDK's desktop runner (arrows, X = A, Z = B, Enter = Start, Backspace = Select), game
 *   controllers in the SNES layout, Back = Android's Back button, Esc, or the pad's Guide button.
 * Lifecycle (Android): going to the background saves the game's state and save RAM and holds it; it waits for a
 *   touch or a button when it comes back (the play sign). The "Get the full version" button opens
 *   market://details?id=<paid app>, or the https page when there is no Play Store.
 */
#include "rsb.h"
#include "rs_desktop.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex, *big, *blur;
static int tex_w, blur_w, big_key;
static SDL_GameController *pads[RS_PAD_MAX];
static SDL_AudioDeviceID audio;
static rsb_input in;

static int mouse_down;
static char base_dir[1024];

/* fingers: kept one frame after they are lifted, so that a quick tap is seen */
static struct { long id; float x, y; int up; } fingers[RSB_FINGERS];
static int nfingers;

static void log_fn(const char *line) { SDL_Log("%s", line); }

static void open_pads(void)
{
    int slot = 0;
    for (int i = 0; i < RS_PAD_MAX; i++) {
        if (pads[i]) SDL_GameControllerClose(pads[i]);
        pads[i] = NULL;
    }
    for (int i = 0; i < SDL_NumJoysticks() && slot < RS_PAD_MAX; i++)
        if (SDL_IsGameController(i) && (pads[slot] = SDL_GameControllerOpen(i)) != NULL) {
            SDL_Log("pad %d: %s", slot + 1, SDL_GameControllerName(pads[slot]));
            slot++;
        }
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
static const keymap KEYS[] = {
    {SDL_SCANCODE_UP, RS_BTN_UP}, {SDL_SCANCODE_DOWN, RS_BTN_DOWN},
    {SDL_SCANCODE_LEFT, RS_BTN_LEFT}, {SDL_SCANCODE_RIGHT, RS_BTN_RIGHT},
    {SDL_SCANCODE_Z, RS_BTN_B}, {SDL_SCANCODE_X, RS_BTN_A},
    {SDL_SCANCODE_C, RS_BTN_Y}, {SDL_SCANCODE_V, RS_BTN_X},
    {SDL_SCANCODE_Q, RS_BTN_L}, {SDL_SCANCODE_E, RS_BTN_R},
    {SDL_SCANCODE_RETURN, RS_BTN_START}, {SDL_SCANCODE_KP_ENTER, RS_BTN_START},
    {SDL_SCANCODE_SPACE, RS_BTN_A},
    {SDL_SCANCODE_RSHIFT, RS_BTN_SELECT}, {SDL_SCANCODE_BACKSPACE, RS_BTN_SELECT},
    {SDL_SCANCODE_UNKNOWN, 0}};

static uint16_t keyboard_state(void)
{
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    uint16_t b = 0;
    for (const keymap *m = KEYS; m->bit; m++)
        if (k[m->k]) b |= m->bit;
    return b;
}

static void finger_set(long id, float x, float y, int up)
{
    for (int i = 0; i < nfingers; i++)
        if (fingers[i].id == id) {
            fingers[i].x = x;
            fingers[i].y = y;
            if (up) fingers[i].up = 1;
            return;
        }
    if (up || nfingers >= RSB_FINGERS) return;
    fingers[nfingers].id = id;
    fingers[nfingers].x = x;
    fingers[nfingers].y = y;
    fingers[nfingers].up = 0;
    nfingers++;
    in.any = 1;
}

/* ---- the picture ------------------------------------------------------------------------------------------------- */
static rsb_view view;

static void present(int show_pause, int show_held)
{
    int ww, wh, fw = rsb_screen_w();
    SDL_GetRendererOutputSize(ren, &ww, &wh);
    if (!tex || tex_w != fw) {
        if (tex) SDL_DestroyTexture(tex);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, fw, RS_SCREEN_H);
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
        tex_w = fw;
    }
    SDL_UpdateTexture(tex, NULL, rs_host_framebuffer(), fw * 2);

    /* the size: the display's height; an integer scale when it fits within 10% */
    int k = wh / RS_SCREEN_H;
    if (k < 1) k = 1;
    int dh = (k * RS_SCREEN_H * 10 >= wh * 9) ? k * RS_SCREEN_H : wh;
    int dw = (int)((long long)fw * dh / RS_SCREEN_H);
    if (dw > ww) {                                   /* a tall display (portrait, a narrow window) */
        dw = ww;
        dh = (int)((long long)RS_SCREEN_H * dw / fw);
        k = dh / RS_SCREEN_H;
        if (k >= 1 && k * RS_SCREEN_H * 10 >= dh * 9) { dh = k * RS_SCREEN_H; dw = fw * k; }
    }
    SDL_Rect dst = {(ww - dw) / 2, (wh - dh) / 2, dw, dh};
    view.x = (float)dst.x / ww;
    view.y = (float)dst.y / wh;
    view.w = (float)dst.w / ww;
    view.h = (float)dst.h / wh;

    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    if (dw < ww || dh < wh) {
        /* the borders: the picture stretched over the whole display, blurred (a 1/16 copy, smoothed) and dark */
        int bw = fw / 16, bh = RS_SCREEN_H / 16;
        if (!blur || blur_w != fw) {
            if (blur) SDL_DestroyTexture(blur);
            blur = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, bw, bh);
            SDL_SetTextureScaleMode(blur, SDL_ScaleModeLinear);
            blur_w = fw;
        }
        static uint32_t px[(560 / 16) * (240 / 16)];
        const uint16_t *fb = rs_host_framebuffer();
        for (int by = 0; by < bh; by++)
            for (int bx = 0; bx < bw; bx++) {
                unsigned r = 0, g = 0, b = 0;
                for (int y = 0; y < 16; y += 2)
                    for (int x = 0; x < 16; x += 2) {
                        uint16_t c = fb[(by * 16 + y) * fw + bx * 16 + x];
                        r += c >> 11;
                        g += (c >> 5) & 63;
                        b += c & 31;
                    }
                r = r * 255 / (31 * 64);
                g = g * 255 / (63 * 64);
                b = b * 255 / (31 * 64);
                px[by * bw + bx] = (r * 2 / 5) << 16 | (g * 2 / 5) << 8 | (b * 2 / 5);
            }
        SDL_UpdateTexture(blur, NULL, px, bw * 4);
        SDL_Rect all = {-ww / 8, -wh / 8, ww + ww / 4, wh + wh / 4};
        SDL_RenderCopy(ren, blur, NULL, &all);
    }
    if (dh % RS_SCREEN_H == 0) {
        SDL_RenderCopy(ren, tex, NULL, &dst);
    } else {
        /* sharp bilinear: nearest to the integer size above, then smoothed down */
        int kk = dh / RS_SCREEN_H + 1;
        if (!big || big_key != fw * 1000 + kk) {
            if (big) SDL_DestroyTexture(big);
            big = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_TARGET, fw * kk, RS_SCREEN_H * kk);
            if (big) SDL_SetTextureScaleMode(big, SDL_ScaleModeLinear);
            big_key = fw * 1000 + kk;
        }
        if (big && SDL_SetRenderTarget(ren, big) == 0) {
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_SetRenderTarget(ren, NULL);
            SDL_RenderCopy(ren, big, NULL, &dst);
        } else {
            SDL_RenderCopy(ren, tex, NULL, &dst);
        }
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    if (show_held) {
        /* the play sign: the picture dimmed, a round button with a triangle */
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 110);
        SDL_RenderFillRect(ren, &dst);
        float cx = dst.x + dst.w / 2.0f, cy = dst.y + dst.h / 2.0f, r = dst.h * 0.12f;
        SDL_Vertex v[3 + 48 * 3];
        int n = 0;
        SDL_Color disc = {16, 24, 56, 200}, tri = {255, 246, 220, 255};
        for (int i = 0; i < 48; i++) {
            float a0 = i * 6.2831853f / 48, a1 = (i + 1) * 6.2831853f / 48;
            v[n++] = (SDL_Vertex){{cx, cy}, disc, {0, 0}};
            v[n++] = (SDL_Vertex){{cx + r * SDL_cosf(a0), cy + r * SDL_sinf(a0)}, disc, {0, 0}};
            v[n++] = (SDL_Vertex){{cx + r * SDL_cosf(a1), cy + r * SDL_sinf(a1)}, disc, {0, 0}};
        }
        v[n++] = (SDL_Vertex){{cx - r * 0.35f, cy - r * 0.5f}, tri, {0, 0}};
        v[n++] = (SDL_Vertex){{cx + r * 0.55f, cy}, tri, {0, 0}};
        v[n++] = (SDL_Vertex){{cx - r * 0.35f, cy + r * 0.5f}, tri, {0, 0}};
        SDL_RenderGeometry(ren, NULL, v, n, NULL, 0);
    } else if (show_pause) {
        /* the pause button: a small translucent square with two bars, top-right */
        float px, py, pw, ph;
        rsb_pause_rect((float)ww / wh, &px, &py, &pw, &ph);
        SDL_Rect b = {(int)(px * ww), (int)(py * wh), (int)(pw * ww), (int)(ph * wh)};
        SDL_SetRenderDrawColor(ren, 16, 24, 56, 90);
        SDL_RenderFillRect(ren, &b);
        SDL_SetRenderDrawColor(ren, 255, 246, 220, 150);
        SDL_Rect l = {b.x + b.w * 3 / 10, b.y + b.h / 4, b.w / 7, b.h / 2};
        SDL_Rect r = {b.x + b.w - b.w * 3 / 10 - b.w / 7, b.y + b.h / 4, b.w / 7, b.h / 2};
        SDL_RenderFillRect(ren, &l);
        SDL_RenderFillRect(ren, &r);
    }
    SDL_RenderPresent(ren);
}

/* the width the games may use: the display's aspect at 240 px high, even, 320..560 */
static int width_for(int ww, int wh)
{
    int w = wh > 0 ? (int)((long long)RS_SCREEN_H * ww / wh) : 320;
    return w < 320 ? 320 : w > 560 ? 560 : w & ~1;
}

/* the system's language (docs/i18n.md): the first of the user's preferred locales the games have (-1: none) */
static int system_lang(void)
{
    int l = -1;
    SDL_Locale *loc = SDL_GetPreferredLocales();
    for (SDL_Locale *p = loc; p && p->language && l < 0; p++) {
        char s[48];
        snprintf(s, sizeof s, "%s%s%s", p->language, p->country ? "_" : "", p->country ? p->country : "");
        l = rs_lang_find(s);
    }
    SDL_free(loc);
    return l;
}

static void open_store(const char *url)
{
    if (SDL_OpenURL(url) != 0) {
        SDL_Log("cannot open %s (%s): %s", url, SDL_GetError(), rsb_store_fallback());
        SDL_OpenURL(rsb_store_fallback());
    }
}

int main(int argc, char **argv)
{
    int scale = 3, fullscreen = 0, lang = rsd_lang_env();
    const char *saves = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fullscreen")) fullscreen = 1;
        else if (!strcmp(argv[i], "--saves") && i + 1 < argc) saves = argv[++i];
        else if (!strcmp(argv[i], "--opt") && i + 1 < argc) rsd_option(argv[++i]);
        else if (!strcmp(argv[i], "--lang") && i + 1 < argc) lang = rsd_lang_arg(argv[++i]), lang = lang < 0 ? 0 : lang;
    }
    if (scale < 1) scale = 1;
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    /* the saves: the app's private folder (Android: internal storage; Windows: %APPDATA%\8BCraft\<app id>) */
    if (saves) snprintf(base_dir, sizeof base_dir, "%s%s", saves, saves[strlen(saves) - 1] == '/' ? "" : "/");
    else {
        char *p = SDL_GetPrefPath("8BCraft", rsb_app_id);
        snprintf(base_dir, sizeof base_dir, "%s", p ? p : "./");
        SDL_free(p);
    }
    rsb_set_log(log_fn);
    rs_host_set_log(log_fn);

    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __ANDROID__
    flags |= SDL_WINDOW_FULLSCREEN;
    int w0 = 1280, h0 = 720;
#else
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    int w0 = 426 * scale, h0 = RS_SCREEN_H * scale;     /* 16:9 */
#endif
    win = SDL_CreateWindow(rsb_app_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w0, h0, flags);
    if (!win) { SDL_Log("SDL_CreateWindow: %s", SDL_GetError()); return 1; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) { SDL_Log("SDL_CreateRenderer: %s", SDL_GetError()); return 1; }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = RS_AUDIO_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio) SDL_PauseAudioDevice(audio, 0);
    else SDL_Log("no audio: %s", SDL_GetError());
    open_pads();

    if (lang < 0 && (lang = system_lang()) >= 0) rs_host_set_lang(lang);   /* the games' language: the phone's */
    int ww, wh;
    SDL_GetRendererOutputSize(ren, &ww, &wh);
    if (rsb_init(base_dir, width_for(ww, wh))) return 1;
    SDL_Log("%s %s: saves in %s", rsb_app_name, rsb_app_version, base_dir);

    const uint64_t freq = SDL_GetPerformanceFrequency(), frame_ticks = freq / RS_FPS;
    uint64_t next = SDL_GetPerformanceCounter();
    int running = 1, shot = 0, background = 0;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_APP_WILLENTERBACKGROUND:
            case SDL_APP_DIDENTERBACKGROUND:
                if (!background) {
                    rsb_suspend();                          /* the state and the save RAM, now */
                    background = 1;
                }
                break;
            case SDL_APP_DIDENTERFOREGROUND:
                background = 0;
                rsb_hold();
                if (audio) SDL_ClearQueuedAudio(audio);
                next = SDL_GetPerformanceCounter();
                nfingers = 0;
                break;
            case SDL_APP_TERMINATING:
                rsb_suspend();
                running = 0;
                break;
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
                open_pads();
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                in.any = 1;
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) in.back = 1;
                break;
            case SDL_KEYDOWN:
                if (e.key.repeat) break;
                in.any = 1;
                if (e.key.keysym.sym == SDLK_AC_BACK || e.key.keysym.sym == SDLK_ESCAPE) in.back = 1;
                else if (e.key.keysym.sym == SDLK_F11 ||
                         (e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT))) {
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                } else if (e.key.keysym.sym == SDLK_F12) {
                    char p[1200];
                    snprintf(p, sizeof p, "%sscreenshot-%03d.png", base_dir, shot++);
                    if (!rsd_write_png(p, rs_host_framebuffer(), 2)) SDL_Log("saved %s", p);
                }
                break;
            case SDL_FINGERDOWN:
            case SDL_FINGERMOTION:
            case SDL_FINGERUP:
                finger_set((long)e.tfinger.fingerId, e.tfinger.x, e.tfinger.y, e.type == SDL_FINGERUP);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEMOTION:
                if (e.type == SDL_MOUSEMOTION ? e.motion.which == SDL_TOUCH_MOUSEID : e.button.which == SDL_TOUCH_MOUSEID)
                    break;
                if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) mouse_down = 1;
                if (mouse_down) {
                    int mx = e.type == SDL_MOUSEMOTION ? e.motion.x : e.button.x;
                    int my = e.type == SDL_MOUSEMOTION ? e.motion.y : e.button.y;
                    int sw, sh;
                    SDL_GetWindowSize(win, &sw, &sh);
                    finger_set(-1000, (float)mx / sw, (float)my / sh,
                               e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT);
                }
                if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) mouse_down = 0;
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    SDL_GetRendererOutputSize(ren, &ww, &wh);
                    rsb_resize(width_for(ww, wh));
                }
                break;
            }
        }
        if (background) {
            SDL_Delay(50);
            continue;
        }
        const char *url = rsb_store_request();
        if (url) open_store(url);

        uint64_t now = SDL_GetPerformanceCounter();
        int steps = 0;
        if (now + frame_ticks * 8 < next || now > next + frame_ticks * 8) next = now;
        while (now >= next && steps < 4) {
            uint16_t kb = keyboard_state();
            for (int p = 0; p < RS_PAD_MAX; p++) {
                uint16_t pb = pad_state(pads[p]);
                in.pad[p] = (uint16_t)(pb | (p == 0 ? kb : 0));
                in.connected[p] = p == 0 || pads[p] != NULL;
                in.device[p] = p == 0 && kb && !pb ? RS_DEVICE_KEYBOARD : RS_DEVICE_PAD;
            }
            in.nfingers = nfingers;
            for (int i = 0; i < nfingers; i++) {
                in.finger[i].id = fingers[i].id;
                in.finger[i].x = fingers[i].x;
                in.finger[i].y = fingers[i].y;
            }
            in.aspect = wh > 0 ? (float)ww / wh : 16.0f / 9;
            int ran = rsb_frame(&in, &view);
            in.back = in.any = 0;
            for (int i = 0; i < nfingers;)                  /* lifted fingers go once they were seen */
                if (fingers[i].up) fingers[i] = fingers[--nfingers];
                else i++;
            if (ran && audio) {
                int n;
                const int16_t *a = rs_host_audio(&n);
                if (SDL_GetQueuedAudioSize(audio) < (Uint32)(RS_AUDIO_RATE / 10) * 4) SDL_QueueAudio(audio, a, (Uint32)n * 4);
            }
            if (rsb_wants_quit()) running = 0;
            next += frame_ticks;
            steps++;
        }
        if (steps) present(rsb_in_game(), rsb_held());
        else SDL_Delay(1);
    }
    rsb_suspend();
    rsb_shutdown();
    if (audio) SDL_CloseAudioDevice(audio);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
