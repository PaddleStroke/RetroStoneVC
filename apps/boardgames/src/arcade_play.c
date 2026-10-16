/*
 * The Arcade tab's games played inside the RetroStone app (arcade.h), built with ARCADE only (the RetroStone
 * edition: apps/boardgames/Makefile links apps/launcher/bundle.mk's board-lib, the runtime and every game as a
 * module). The frontend is the Arcade app's own, apps/launcher/src/rsb_sdl.c, compiled into this file: its picture
 * (the display's height, an integer scale when it fits, nearest pixels, blurred borders), the game's touch scheme
 * (games.cfg: its drawn buttons), the pause and back buttons, the play sign of a resumed game, the pads and keys, the
 * sound. Only its main() is left out (renamed, unused): this file runs the frames in the app's own window and
 * renderer until the game goes back to the launcher (its back button, Back, Esc), then the app's menu comes back.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifdef ARCADE

#include <SDL.h>
#undef main                                     /* (SDL_main, or -Dmain=SDL_main: the shell's main is the app's) */
#define main rsb_sdl_main                       /* the Arcade app's entry point: not used here */
#include "rsb_sdl.c"
#undef main

#define ARCADE_RUNTIME
#include "arcade.h"
#include "stats.h"

const char *save_path(void);                    /* save.c: the app's save file (NULL: the picture mode's memory) */
const char *text_lang(void);                    /* text.c: the app's language ("fr", "pt_BR") */

#define ARCADE_XP_PLAYED 5                      /* a session of ARCADE_MIN_S seconds or more */
#define ARCADE_XP_BEST 20                       /* a new best score */
#define ARCADE_MIN_S 30

static int ready;

static int game_index(const char *id)
{
    for (int g = 0; g < rsb_ngames; g++)
        if (!strcmp(rsb_games[g].id, id) && rsb_games[g].module) return g;
    return -1;
}

/* once: the runtime (its saves beside the app's save file: <dir>/arcade-<id>.state), the pads; each time: the app's
 * language for the games */
static int prepare(SDL_Renderer *r, int ww, int wh)
{
    ren = r;
    win = SDL_RenderGetWindow(r);
    if (!ready) {
        const char *sp = save_path();
        if (sp) {
            snprintf(base_dir, sizeof base_dir, "%s", sp);
            char *s = strrchr(base_dir, '/'), *b = strrchr(base_dir, '\\');
            if (b > s) s = b;
            if (s) s[1] = 0;
            else base_dir[0] = 0;
        } else {                                /* the picture mode: nothing kept in the app's folder */
            const char *t = getenv("TMPDIR") ? getenv("TMPDIR") : getenv("TEMP") ? getenv("TEMP") : "/tmp";
            snprintf(base_dir, sizeof base_dir, "%s/", t);
        }
        strncat(base_dir, "arcade-", sizeof base_dir - strlen(base_dir) - 1);
        rsb_set_log(log_fn);
        rs_host_set_log(log_fn);
        if (rsb_init(base_dir, width_for(ww, wh))) return -1;
        SDL_Log("arcade: %d games, saves %s<id>.state", rsb_ngames, base_dir);
        ready = 1;
    }
    int lang = rs_lang_find(text_lang());
    if (lang >= 0) rs_host_set_lang(lang);
    return 0;
}

/* Android: landscape while a game runs (the sensor's way round), then as the app was (fullUser: apps/android
 * app-retrostone.conf's ORIENTATION), through the activity's setRequestedOrientation (SDL's own way, from this thread) */
static void orientation(int landscape)
{
#ifdef __ANDROID__
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject act = (jobject)SDL_AndroidGetActivity();
    if (!env || !act) return;
    jclass cls = (*env)->GetObjectClass(env, act);
    jmethodID m = (*env)->GetMethodID(env, cls, "setRequestedOrientation", "(I)V");
    /* ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE 6, SCREEN_ORIENTATION_FULL_USER 13 */
    if (m) (*env)->CallVoidMethod(env, act, m, landscape ? 6 : 13);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, act);
#else
    (void)landscape;
#endif
}

static void textures_free(void)
{
    if (tex) SDL_DestroyTexture(tex);
    if (big) SDL_DestroyTexture(big);
    if (blur) SDL_DestroyTexture(blur);
    tex = big = blur = NULL;
    tex_w = blur_w = big_key = 0;
}

/* the session's XP: played long enough (the day counts in the streak, the game counts as played: the Stats page's
 * Arcade bar), a new best score */
static void award(int g, int frames, int best0)
{
    char pid[96];
    snprintf(pid, sizeof pid, "arcade_%s", rsb_games[g].id);
    int best1 = rsb_best(g);
    if (frames >= ARCADE_MIN_S * RS_FPS) stats_finish(STATS_ARCADE, pid, ARCADE_XP_PLAYED, 1, 0);
    if (best1 > 0 && best1 > best0) stats_award(STATS_ARCADE, ARCADE_XP_BEST);
    SDL_Log("arcade: %s played %d s, best %d -> %d", rsb_games[g].id, frames / RS_FPS, best0, best1);
}

int arcade_play(const char *id, SDL_Renderer *r)
{
    int g = game_index(id), ww, wh;
    if (g < 0) return 0;
    SDL_GetRendererOutputSize(r, &ww, &wh);
    if (prepare(r, ww, wh)) return 0;
    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER)) SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_InitSubSystem(SDL_INIT_AUDIO);
    orientation(1);
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = RS_AUDIO_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio) SDL_PauseAudioDevice(audio, 0);
    open_pads();
    memset(&in, 0, sizeof in);
    nfingers = mouse_down = 0;

    int best0 = rsb_best(g);
    rsb_set_width(width_for(ww, wh));
    rsb_start(g, 0);                            /* its state, if any: it resumes, held under the play sign */
    const uint64_t freq = SDL_GetPerformanceFrequency(), frame_ticks = freq / RS_FPS;
    uint64_t next = SDL_GetPerformanceCounter();
    int quit = 0, background = 0, frames = 0;
    while (!quit && rsb_in_game()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
            case SDL_APP_TERMINATING:
                rsb_suspend();
                quit = 1;
                break;
            case SDL_APP_WILLENTERBACKGROUND:
            case SDL_APP_DIDENTERBACKGROUND:
                if (!background) rsb_suspend();     /* the state and the save RAM, now */
                background = 1;
                break;
            case SDL_APP_DIDENTERFOREGROUND:
                background = 0;
                rsb_hold();
                if (audio) SDL_ClearQueuedAudio(audio);
                next = SDL_GetPerformanceCounter();
                nfingers = 0;
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
                if (e.key.keysym.sym != SDLK_AC_BACK) keyboard_used = 1;
                if (e.key.keysym.sym == SDLK_AC_BACK || e.key.keysym.sym == SDLK_ESCAPE) in.back = 1;
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
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {   /* turned to landscape, a window resized */
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
        rsb_store_request();                    /* (the launcher's: not shown here) */
        rsb_review_request();
        uint64_t now = SDL_GetPerformanceCounter();
        int steps = 0;
        if (now + frame_ticks * 8 < next || now > next + frame_ticks * 8) next = now;
        while (now >= next && steps < 4 && rsb_in_game()) {
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
            in.gamepad = keyboard_used;
            for (int p = 0; p < RS_PAD_MAX; p++) in.gamepad |= pads[p] != NULL;
            int ran = rsb_frame(&in, &view);
            frames += ran;
            in.back = in.any = 0;
            for (int i = 0; i < nfingers;)          /* lifted fingers go once they were seen */
                if (fingers[i].up) fingers[i] = fingers[--nfingers];
                else i++;
            if (ran && audio) {
                int n;
                const int16_t *a = rs_host_audio(&n);
                if (SDL_GetQueuedAudioSize(audio) < (Uint32)(RS_AUDIO_RATE / 10) * 4) SDL_QueueAudio(audio, a, (Uint32)n * 4);
            }
            next += frame_ticks;
            steps++;
        }
        if (steps && rsb_in_game()) present(1, rsb_held());
        else if (!steps) SDL_Delay(1);
    }
    if (rsb_in_game()) rsb_start(-1, 0);        /* the app closed in a game: its state saved, the launcher */
    award(g, frames, best0);
    if (audio) SDL_CloseAudioDevice(audio);
    audio = 0;
    textures_free();
    SDL_SetRenderTarget(ren, NULL);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    orientation(0);
    if (quit) {                                 /* the shell's own loop ends too */
        SDL_Event q;
        SDL_zero(q);
        q.type = SDL_QUIT;
        SDL_PushEvent(&q);
    }
    return quit;
}

int arcade_shot(const char *id, SDL_Renderer *r, SDL_Surface *surf, const char *out, int argc, char **argv)
{
    int g = game_index(id), ww, wh, ms = 3000;
    const char *pad = "";
    if (g < 0) { fprintf(stderr, "no arcade game %s\n", id); return 1; }
    for (int i = 1; i + 1 < argc; i++) {
        if (!strcmp(argv[i], "--at")) ms = atoi(argv[i + 1]);
        if (!strcmp(argv[i], "--pad")) pad = argv[i + 1];
    }
    SDL_GetRendererOutputSize(r, &ww, &wh);
    if (prepare(r, ww, wh)) return 1;
    rsb_set_width(width_for(ww, wh));
    rsb_start(g, 1);                            /* afresh: no saved state */
    memset(&in, 0, sizeof in);
    int frames = ms * RS_FPS / 1000;
    for (int f = 0; f < frames; f++) {
        uint16_t b = 0;
        for (const char *p = pad; *p;) {        /* "START@60 A@120": each pressed 6 frames from that frame */
            char name[16];
            int at = -1, n = 0;
            if (sscanf(p, "%15[A-Z]@%d%n", name, &at, &n) != 2 || n <= 0) break;
            p += n;
            while (*p == ' ') p++;
            static const struct { const char *n; uint16_t b; } B[] = {
                {"A", RS_BTN_A}, {"B", RS_BTN_B}, {"X", RS_BTN_X}, {"Y", RS_BTN_Y}, {"L", RS_BTN_L}, {"R", RS_BTN_R},
                {"START", RS_BTN_START}, {"SELECT", RS_BTN_SELECT}, {"UP", RS_BTN_UP}, {"DOWN", RS_BTN_DOWN},
                {"LEFT", RS_BTN_LEFT}, {"RIGHT", RS_BTN_RIGHT}};
            for (unsigned k = 0; k < sizeof B / sizeof B[0]; k++)
                if (!strcmp(B[k].n, name) && f >= at && f < at + 6) b |= B[k].b;
        }
        in.pad[0] = b;
        in.connected[0] = 1;
        in.device[0] = RS_DEVICE_PAD;
        in.aspect = wh > 0 ? (float)ww / wh : 16.0f / 9;
        rsb_frame(&in, &view);
    }
    present(1, rsb_held());
    textures_free();
    if (!out || SDL_SaveBMP(surf, out)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    printf("%s: %dx%d arcade %s, %d frames\n", out, ww, wh, id, frames);
    return 0;
}

#endif
