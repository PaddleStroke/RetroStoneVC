/*
 * RetroStone VC SDK: headless runner. Runs N frames of a game with scripted
 * input, dumps PNG screenshots and measures the per-frame cost.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   <game>_headless [options]
 *     --frames N          frames to run (default 60)
 *     --input FILE        input script (see below)
 *     --shot F:FILE.png   screenshot after frame F (repeatable)
 *     --png FILE.png      screenshot after the last frame
 *     --scale N           PNG scale factor (default 1)
 *     --width N           widescreen: the screen width (320..560; the game may keep 320: rs_game.max_width);
 *                         default: the environment variable RS_WIDTH, else 320
 *     --resize F:N        change the width live after frame F (rs_host_change_width: a save state, a restart at
 *                         the new width, the state loaded; what the desktop runner does when its window is resized)
 *     --opt KEY=VALUE     game option (repeatable)
 *     --lang CODE         the language (en, fr, pt_BR, zh_CN...; docs/i18n.md); default: the environment variable
 *                         RS_LANG, else English (never the system's: the runs stay the same everywhere)
 *     --data DIR          data directory override
 *     --sram FILE         load (and save back) the save RAM
 *     --bench [FROM]      print frame-time statistics (frames >= FROM)
 *     --wav FILE.wav      record the audio
 *     --device Pn=DEV     what drives port n: pad, kb (first key set) or kb2 (second key set);
 *                         default: P1 kb, P2 kb2, P3-P4 pad (the desktop keyboard, no controller)
 *
 * Input script: one event per line, '#' starts a comment.
 *     <frame> [P1..P4] <BUTTONS|->       hold BUTTONS (B+UP...) from <frame>
 *     <frame> [P1..P4] tap <BUTTONS>     hold for 2 frames, then release
 *     <frame> key F<n>                   press the development key Fn (rs_dev_key) at <frame>
 *   Touches (player 1; docs/spec.md "Touch"), finger T1 unless T1..T4 is given:
 *     <frame> [T1..T4] touch <x> <y>     a finger goes down at x, y and stays
 *     <frame> [T1..T4] move <x> <y>      it moves there
 *     <frame> [T1..T4] up                it is lifted
 *     <frame> [T1..T4] tap-at <x> <y>    down for 3 frames, then up (a tap)
 *     <frame> [T1..T4] swipe <x> <y> <dx> <dy>   down at x, y, moved by dx, dy (pixels) over 4 frames, up 2 frames later
 *   x and y: screen pixels (0..width-1, 0..239); N% = percent of the screen's width / height; @N = the STAGE
 *   coordinate N (x: N + rs_screen_origin_x(): the same spot of a centred game at any width).
 */
#include "rs_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS 4096
#define MAX_SHOTS 64

enum { EV_PAD, EV_KEY, EV_TOUCH, EV_MOVE, EV_UP };
typedef struct { float v; char mode; } coord_t;    /* mode: 0 pixels, '%' percent, '@' stage */
typedef struct { int frame, kind, port; uint16_t buttons; coord_t x, y; } event_t;
static event_t ev[MAX_EVENTS];
static int nev;
static struct { int frame; char path[512]; } shots[MAX_SHOTS];
static int nshots;
static struct { int frame, width; } resizes[8];
static int nresizes;

static void add(event_t e)
{
    if (nev >= MAX_EVENTS) return;
    /* keep the list sorted by frame (stable) */
    int i = nev++;
    while (i > 0 && ev[i - 1].frame > e.frame) { ev[i] = ev[i - 1]; i--; }
    ev[i] = e;
}
static void add_event(int f, int port, uint16_t b)
{
    event_t e = {f, port < 0 ? EV_KEY : EV_PAD, port, b, {0, 0}, {0, 0}};
    add(e);
}
static coord_t coord(const char *s)
{
    coord_t c = {0, 0};
    if (*s == '@') { c.mode = '@'; s++; }
    c.v = (float)atof(s);
    if (strchr(s, '%')) c.mode = '%';
    return c;
}
static void add_touch(int f, int kind, int finger, coord_t x, coord_t y)
{
    event_t e = {f, kind, finger, 0, x, y};
    add(e);
}

/* the fingers down (headless: ids 1..4 = T1..T4) */
static rs_host_touch fingers[RS_TOUCH_MAX];
static int nfingers;
static int resolve(coord_t c, int horizontal)
{
    int size = horizontal ? rs_screen_width() : RS_SCREEN_H;
    if (c.mode == '%') return (int)(c.v * size / 100.0f);
    if (c.mode == '@') return (int)c.v + (horizontal ? rs_screen_origin_x() : 0);
    return (int)c.v;
}
static void touch_event(const event_t *e)
{
    int k = 0;
    while (k < nfingers && fingers[k].id != e->port + 1) k++;
    if (e->kind == EV_UP) {
        if (k < nfingers) fingers[k] = fingers[--nfingers];
    } else {
        if (k == nfingers) {
            if (nfingers >= RS_TOUCH_MAX) return;
            nfingers++;
        }
        fingers[k].id = e->port + 1;
        fingers[k].x = resolve(e->x, 1);
        fingers[k].y = resolve(e->y, 0);
    }
    rs_host_set_touches(nfingers, fingers);
}

static int load_script(const char *path)
{
    size_t n;
    char *s = rsd_load_file(path, &n);
    if (!s) { fprintf(stderr, "cannot read %s\n", path); return -1; }
    char *line = s;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char *h = strchr(line, '#');
        if (h) *h = 0;
        char t[6][128] = {"", "", "", "", "", ""};
        int k = sscanf(line, "%127s %127s %127s %127s %127s %127s", t[0], t[1], t[2], t[3], t[4], t[5]);
        line = nl ? nl + 1 : NULL;
        if (k < 2) continue;
        int f = atoi(t[0]), port = 0, i = 1;
        if ((t[1][0] == 'P' || t[1][0] == 'p' || t[1][0] == 'T' || t[1][0] == 't') && t[1][1] >= '1' &&
            t[1][1] <= '4' && !t[1][2]) {
            port = t[1][1] - '1';
            i = 2;
        }
        if (i >= k) continue;
        if (!strcmp(t[i], "touch") || !strcmp(t[i], "move") || !strcmp(t[i], "tap-at")) {
            if (i + 2 >= k) { fprintf(stderr, "%s: %s needs x y\n", path, t[i]); free(s); return -1; }
            coord_t x = coord(t[i + 1]), y = coord(t[i + 2]);
            add_touch(f, !strcmp(t[i], "move") ? EV_MOVE : EV_TOUCH, port, x, y);
            if (!strcmp(t[i], "tap-at")) add_touch(f + 3, EV_UP, port, x, y);
            continue;
        }
        if (!strcmp(t[i], "up")) { coord_t z = {0, 0}; add_touch(f, EV_UP, port, z, z); continue; }
        if (!strcmp(t[i], "swipe")) {
            if (i + 4 >= k) { fprintf(stderr, "%s: swipe needs x y dx dy\n", path); free(s); return -1; }
            coord_t x = coord(t[i + 1]), y = coord(t[i + 2]);
            float dx = (float)atof(t[i + 3]), dy = (float)atof(t[i + 4]);
            add_touch(f, EV_TOUCH, port, x, y);
            for (int st = 1; st <= 4; st++) {
                coord_t mx = x, my = y;
                /* dx, dy in the start's unit (pixels; percent after an N% start) */
                mx.v += dx * st / 4;
                my.v += dy * st / 4;
                add_touch(f + st, EV_MOVE, port, mx, my);
            }
            coord_t z = {0, 0};
            add_touch(f + 6, EV_UP, port, z, z);
            continue;
        }
        if (!strcmp(t[i], "key") && i + 1 < k && (t[i + 1][0] == 'F' || t[i + 1][0] == 'f')) {
            add_event(f, -1, (uint16_t)atoi(t[i + 1] + 1));
            continue;
        }
        if (!strcmp(t[i], "tap")) {
            if (i + 1 >= k) continue;
            add_event(f, port, rsd_buttons(t[i + 1]));
            add_event(f + 2, port, 0);
        } else {
            add_event(f, port, strcmp(t[i], "-") ? rsd_buttons(t[i]) : 0);
        }
    }
    free(s);
    return 0;
}

static void wav_header(FILE *f, uint32_t frames)
{
    uint32_t data = frames * 4, riff = 36 + data, rate = RS_AUDIO_RATE, brate = RS_AUDIO_RATE * 4, fmt = 16;
    uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f);
    fwrite(&brate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
}

int main(int argc, char **argv)
{
    int frames = 60, scale = 1, bench = 0, bench_from = 0;
    const char *png = NULL, *sram = NULL, *wav = NULL;
    int device[RS_PAD_MAX] = {RS_DEVICE_KEYBOARD, RS_DEVICE_KEYBOARD2, RS_DEVICE_PAD, RS_DEVICE_PAD};
    rsd_width_env();
    rsd_lang_env();
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--frames") && v) frames = atoi(argv[++i]);
        else if (!strcmp(a, "--input") && v) { if (load_script(argv[++i])) return 1; }
        else if (!strcmp(a, "--shot") && v && nshots < MAX_SHOTS) {
            const char *colon = strchr(argv[++i], ':');
            if (!colon) { fprintf(stderr, "--shot needs FRAME:FILE\n"); return 1; }
            shots[nshots].frame = atoi(argv[i]);
            snprintf(shots[nshots].path, sizeof shots[0].path, "%s", colon + 1);
            nshots++;
        }
        else if (!strcmp(a, "--png") && v) png = argv[++i];
        else if (!strcmp(a, "--scale") && v) scale = atoi(argv[++i]);
        else if (!strcmp(a, "--width") && v) rs_host_set_width(atoi(argv[++i]));
        else if (!strcmp(a, "--resize") && v && nresizes < 8) {
            const char *colon = strchr(argv[++i], ':');
            if (!colon) { fprintf(stderr, "--resize needs FRAME:WIDTH\n"); return 1; }
            resizes[nresizes].frame = atoi(argv[i]);
            resizes[nresizes++].width = atoi(colon + 1);
        }
        else if (!strcmp(a, "--opt") && v) rsd_option(argv[++i]);
        else if (!strcmp(a, "--lang") && v) rsd_lang_arg(argv[++i]);
        else if (!strcmp(a, "--dev")) rsd_option("dev=1");
        else if (!strcmp(a, "--data") && v) rsd_set_data_dir(argv[++i]);
        else if (!strcmp(a, "--sram") && v) sram = argv[++i];
        else if (!strcmp(a, "--wav") && v) wav = argv[++i];
        else if (!strcmp(a, "--device") && v) {
            const char *d = argv[++i];
            int port = (d[0] == 'P' || d[0] == 'p') ? d[1] - '1' : -1;
            if (port < 0 || port >= RS_PAD_MAX || d[2] != '=') { fprintf(stderr, "--device needs Pn=pad|kb|kb2\n"); return 1; }
            device[port] = !strcmp(d + 3, "kb") ? RS_DEVICE_KEYBOARD : !strcmp(d + 3, "kb2") ? RS_DEVICE_KEYBOARD2 : RS_DEVICE_PAD;
        }
        else if (!strcmp(a, "--bench")) {
            bench = 1;
            if (v && v[0] >= '0' && v[0] <= '9') bench_from = atoi(argv[++i]);
        } else {
            fprintf(stderr, "unknown or incomplete option: %s\n", a);
            return 1;
        }
    }
    const rs_game *g = rs_game_main();
    rs_host_set_file_loader(rsd_data_loader);
    if (sram) rsd_sram_load(sram);
    for (int p = 0; p < RS_PAD_MAX; p++) rs_host_set_pad_device(p, device[p]);
    rs_host_init(g);

    FILE *wf = NULL;
    uint32_t wav_frames = 0;
    if (wav && (wf = fopen(wav, "wb"))) wav_header(wf, 0);

    uint64_t sum_u = 0, sum_r = 0, sum_a = 0, sum_t = 0;
    uint32_t max_t = 0, max_r = 0, max_u = 0;
    int worst = -1, measured = 0, max_spr = 0, max_line = 0;
    int e = 0;
    for (int f = 0; f < frames; f++) {
        while (e < nev && ev[e].frame <= f) {
            if (ev[e].kind == EV_KEY) rs_host_set_dev_key(ev[e].buttons);
            else if (ev[e].kind == EV_PAD) rs_host_set_pad(ev[e].port, ev[e].buttons, 1);
            else touch_event(&ev[e]);
            e++;
        }
        rs_host_frame();
        const rs_host_stats *st = rs_host_stats_last();
        if (bench && f >= bench_from) {
            uint32_t t = st->update_us + st->render_us + st->audio_us;
            sum_u += st->update_us; sum_r += st->render_us; sum_a += st->audio_us; sum_t += t;
            if (t > max_t) { max_t = t; worst = f; }
            if (st->render_us > max_r) max_r = st->render_us;
            if (st->update_us > max_u) max_u = st->update_us;
            if (st->sprites > max_spr) max_spr = st->sprites;
            if (st->max_sprites_line > max_line) max_line = st->max_sprites_line;
            measured++;
        }
        if (wf) {
            int n;
            const int16_t *a = rs_host_audio(&n);
            fwrite(a, 4, (size_t)n, wf);
            wav_frames += (uint32_t)n;
        }
        for (int s = 0; s < nshots; s++)
            if (shots[s].frame == f) {
                if (rsd_write_png(shots[s].path, rs_host_framebuffer(), scale))
                    fprintf(stderr, "cannot write %s\n", shots[s].path);
                else
                    printf("frame %d -> %s\n", f, shots[s].path);
            }
        for (int r = 0; r < nresizes; r++)
            if (resizes[r].frame == f) {
                int w = rs_host_change_width(resizes[r].width);
                printf("frame %d: width %d\n", f, w);
            }
    }
    if (png) {
        if (rsd_write_png(png, rs_host_framebuffer(), scale)) fprintf(stderr, "cannot write %s\n", png);
        else printf("frame %d -> %s\n", frames - 1, png);
    }
    if (wf) {
        fseek(wf, 0, SEEK_SET);
        wav_header(wf, wav_frames);
        fclose(wf);
    }
    if (bench && measured) {
        printf("bench: %d frames (from frame %d)\n", measured, bench_from);
        printf("  update+draw  avg %7.1f us   max %6u us\n", (double)sum_u / measured, max_u);
        printf("  render (PPU) avg %7.1f us   max %6u us\n", (double)sum_r / measured, max_r);
        printf("  audio (APU)  avg %7.1f us\n", (double)sum_a / measured);
        printf("  total        avg %7.1f us   max %6u us (frame %d)   budget 16667 us\n",
               (double)sum_t / measured, max_t, worst);
        printf("  sprites max %d, max per line %d\n", max_spr, max_line);
    }
    if (sram && rs_host_sram_dirty(1)) rsd_sram_save(sram);
    rs_host_shutdown();
    return 0;
}
