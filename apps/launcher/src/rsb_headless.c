/*
 * RetroStone Arcade: the headless frontend (tests, screenshots). No window, no sound: the bundle's runtime with a
 * scripted pad, touches and Back.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   <app>_headless [options]
 *     --frames N         frames to run (default 60)
 *     --saves DIR        where the games' .srm and .state files go (default: build/bundle/saves/)
 *     --width W          the screen width asked for (320..560)
 *     --start ID         start this game at once (its saved state is resumed unless --fresh)
 *     --fresh            ... from scratch
 *     --input FILE       the script (below)
 *     --shot F:FILE.png  screenshot after frame F (repeatable); --png FILE.png after the last frame
 *     --opt KEY=VALUE    a runtime option (rs_option), as on the desktop runners
 *
 * Script, one event per line ('#' comments), frames counted from the start of the run:
 *     <frame> [P1..P4] <BUTTONS|->       hold BUTTONS from <frame> (as rs_headless)
 *     <frame> [P1..P4] tap <BUTTONS>     hold them 2 frames
 *     <frame> touch <x> <y>              a finger goes down at x, y (screen pixels; while a game runs: percent of the
 *                                        screen), and stays
 *     <frame> move <x> <y>               it moves
 *     <frame> up                         it is lifted
 *     <frame> tap-at <x> <y>             down for 3 frames, then up
 *     <frame> back                       the Back button
 *     <frame> suspend                    the app goes to the background and comes back (the game is held)
 *     <frame> log                        prints the running program and a hash of the picture
 *     <frame> start <id|launcher> [fresh]  starts a program now (fresh: the game ignores its saved state)
 * Every switch of program is printed ("start: <id>").
 */
#include "rsb.h"
#include "rs_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EV 4096
enum { E_PAD, E_TOUCH, E_MOVE, E_UP, E_BACK, E_SUSPEND, E_LOG, E_START };
typedef struct { int frame, kind, port; uint16_t buttons; float x, y; char id[32]; } ev_t;
static ev_t ev[MAX_EV];
static int nev;
static struct { int frame; char path[512]; } shots[64];
static int nshots;

static void add(ev_t e)
{
    if (nev >= MAX_EV) return;
    int i = nev++;
    while (i > 0 && ev[i - 1].frame > e.frame) { ev[i] = ev[i - 1]; i--; }
    ev[i] = e;
}

static int load_script(const char *path)
{
    size_t n;
    char *s = rsd_load_file(path, &n);
    if (!s) { fprintf(stderr, "cannot read %s\n", path); return -1; }
    for (char *line = s; line && *line;) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char *h = strchr(line, '#');
        if (h) *h = 0;
        char t[5][64] = {"", "", "", "", ""};
        int k = sscanf(line, "%63s %63s %63s %63s %63s", t[0], t[1], t[2], t[3], t[4]);
        line = nl ? nl + 1 : NULL;
        if (k < 2) continue;
        ev_t e = {atoi(t[0]), E_PAD, 0, 0, 0, 0, ""};
        int i = 1;
        if ((t[1][0] == 'P' || t[1][0] == 'p') && t[1][1] >= '1' && t[1][1] <= '4' && !t[1][2]) {
            e.port = t[1][1] - '1';
            i = 2;
        }
        if (i >= k) continue;
        if (!strcmp(t[i], "touch") || !strcmp(t[i], "move") || !strcmp(t[i], "tap-at")) {
            e.kind = !strcmp(t[i], "move") ? E_MOVE : E_TOUCH;
            e.x = (float)atof(t[i + 1]);
            e.y = (float)atof(t[i + 2]);
            add(e);
            if (!strcmp(t[i], "tap-at")) { e.kind = E_UP; e.frame += 3; add(e); }
        } else if (!strcmp(t[i], "up")) { e.kind = E_UP; add(e); }
        else if (!strcmp(t[i], "back")) { e.kind = E_BACK; add(e); }
        else if (!strcmp(t[i], "suspend")) { e.kind = E_SUSPEND; add(e); }
        else if (!strcmp(t[i], "log")) { e.kind = E_LOG; add(e); }
        else if (!strcmp(t[i], "start") && i + 1 < k) {
            e.kind = E_START;
            snprintf(e.id, sizeof e.id, "%s", t[i + 1]);
            e.buttons = i + 2 < k && !strcmp(t[i + 2], "fresh");
            add(e);
        }
        else if (!strcmp(t[i], "tap")) {
            e.buttons = rsd_buttons(t[i + 1]);
            add(e);
            e.buttons = 0;
            e.frame += 2;
            add(e);
        } else {
            e.buttons = strcmp(t[i], "-") ? rsd_buttons(t[i]) : 0;
            add(e);
        }
    }
    free(s);
    return 0;
}

static uint32_t fb_hash(void)
{
    const uint16_t *fb = rs_host_framebuffer();
    uint32_t h = 2166136261u;
    for (int i = 0; i < rsb_screen_w() * RS_SCREEN_H; i++) h = (h ^ fb[i]) * 16777619u;
    return h;
}

static void log_line(const char *line) { printf("%s\n", line); }

int main(int argc, char **argv)
{
    int frames = 60, width = 320, fresh = 0;
    const char *saves = "build/bundle/saves/", *start = NULL, *png = NULL;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--frames") && v) frames = atoi(argv[++i]);
        else if (!strcmp(a, "--saves") && v) saves = argv[++i];
        else if (!strcmp(a, "--width") && v) width = atoi(argv[++i]);
        else if (!strcmp(a, "--start") && v) start = argv[++i];
        else if (!strcmp(a, "--fresh")) fresh = 1;
        else if (!strcmp(a, "--input") && v) { if (load_script(argv[++i])) return 1; }
        else if (!strcmp(a, "--opt") && v) rsd_option(argv[++i]);
        else if (!strcmp(a, "--lang") && v) rsd_lang_arg(argv[++i]);
        else if (!strcmp(a, "--png") && v) png = argv[++i];
        else if (!strcmp(a, "--shot") && v && nshots < 64) {
            const char *colon = strchr(argv[++i], ':');
            if (!colon) { fprintf(stderr, "--shot needs FRAME:FILE\n"); return 1; }
            shots[nshots].frame = atoi(argv[i]);
            snprintf(shots[nshots].path, sizeof shots[0].path, "%s", colon + 1);
            nshots++;
        } else {
            fprintf(stderr, "unknown or incomplete option: %s\n", a);
            return 1;
        }
    }
    rsd_lang_env();                             /* RS_LANG, as the SDK's headless runner (never the system's) */
    char dir[1100];
    snprintf(dir, sizeof dir, "%s%s", saves, saves[0] && saves[strlen(saves) - 1] != '/' ? "/" : "");
    rsb_set_log(log_line);
    rs_host_set_log(log_line);
    if (rsb_init(dir, width)) return 1;
    if (start) {
        int g = -1;
        for (int i = 0; i < rsb_ngames; i++)
            if (!strcmp(rsb_games[i].id, start)) g = i;
        if (g < 0 || !rsb_games[g].module) { fprintf(stderr, "%s: not a playable game of this app\n", start); return 1; }
        rsb_start(g, fresh);
    }

    rsb_input in;
    memset(&in, 0, sizeof in);
    in.connected[0] = 1;
    in.device[0] = RS_DEVICE_KEYBOARD;
    in.device[1] = RS_DEVICE_KEYBOARD2;
    in.aspect = 4.0f / 3.0f;
    rsb_view view = {0, 0, 1, 1};
    int e = 0, finger = 0, lift = 0;
    for (int f = 0; f < frames && !rsb_wants_quit(); f++) {
        in.back = in.any = 0;
        if (lift) { in.nfingers = 0; lift = 0; }
        while (e < nev && ev[e].frame <= f) {
            ev_t *x = &ev[e++];
            switch (x->kind) {
            case E_PAD:
                if (x->buttons & ~in.pad[x->port]) in.any = 1;
                in.pad[x->port] = x->buttons;
                in.connected[x->port] = 1;
                break;
            case E_TOUCH:
            case E_MOVE: {
                /* screen pixels for the launcher, percent of the screen in a game */
                float fx = rsb_in_game() ? x->x / 100 : x->x / rsb_screen_w();
                float fy = rsb_in_game() ? x->y / 100 : x->y / RS_SCREEN_H;
                if (x->kind == E_TOUCH && !in.nfingers) finger++;
                in.nfingers = 1;
                in.finger[0].id = finger;
                in.finger[0].x = fx;
                in.finger[0].y = fy;
                break;
            }
            case E_UP: lift = 1; break;
            case E_BACK: in.back = 1; break;
            case E_SUSPEND: rsb_suspend(); rsb_hold(); break;
            case E_START: {
                int g = -1;
                for (int i = 0; i < rsb_ngames; i++)
                    if (!strcmp(rsb_games[i].id, x->id)) g = i;
                rsb_start(g, x->buttons);       /* -1 (launcher or an unknown id): the launcher */
                break;
            }
            case E_LOG: printf("frame %d: %s hash %08x\n", f, rsb_current_id(), fb_hash()); break;
            }
        }
        if (lift) { /* the finger is seen up after this frame: a tap lasts at least one frame */ }
        rsb_frame(&in, &view);
        const char *url = rsb_store_request();
        if (url) printf("frame %d: open %s (else %s)\n", f, url, rsb_store_fallback());
        for (int s = 0; s < nshots; s++)
            if (shots[s].frame == f) {
                if (rsd_write_png(shots[s].path, rs_host_framebuffer(), 1)) fprintf(stderr, "cannot write %s\n", shots[s].path);
                else printf("frame %d -> %s (%s)\n", f, shots[s].path, rsb_current_id());
            }
    }
    if (rsb_wants_quit()) printf("quit\n");
    if (png && !rsd_write_png(png, rs_host_framebuffer(), 1)) printf("-> %s (%s)\n", png, rsb_current_id());
    printf("end: %s hash %08x\n", rsb_current_id(), fb_hash());
    rsb_suspend();
    rsb_shutdown();
    return 0;
}
