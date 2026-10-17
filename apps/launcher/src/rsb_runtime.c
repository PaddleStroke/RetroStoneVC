/*
 * RetroStone Arcade: the runtime. One RetroStone VC program runs at a time (the launcher or a game); this file
 * switches between them, gives each game its own save RAM and save state files, a fresh start (its writable data
 * restored to the program's start-up values), and turns touches into the virtual SNES pad (apps/launcher/games.cfg).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A game's life in the app:
 *   start   its module's data is reset (rsb_module: the bounds come from modulize.py), the house kit's too, its save
 *           RAM is read from <save_dir>/<id>.srm, rs_host_init(); then <id>.state, if any, is loaded and the game
 *           waits for a touch or a button (the play sign) so the player is ready
 *   leave   Back (Android), Esc (desktop), the pad's Guide button: <id>.state and <id>.srm are written, the launcher
 *           starts; the same when the app goes to the background (rsb_suspend), without leaving the game
 * A game launched from the menu that comes back to it also counts for the review prompt (rsb_review.h): its best
 * score before and after the run tells a new best; <save_dir>/review.cfg keeps the counters.
 */
#include "rsb.h"
#include "rs_desktop.h"
#include "rsb_review.h"
#include "iap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const rsb_module rsb_module_house;       /* games/common, linked once (modulize.py --keep-all) */
const rs_game *rsb_launcher_game(void);         /* launcher.c */

#define RSB_LAUNCHER (-1)

static char save_dir[1024];
static void (*log_fn)(const char *line);
static struct { char *data; size_t n; } snap_house, *snap_game;

static int cur = RSB_LAUNCHER;                  /* what runs: a game index or RSB_LAUNCHER */
static int pending = -2;                        /* a switch asked during the frame (-2: none) */
static int pending_fresh;
static int last_game = -1;
static int held, quit, want_width = 320, cur_width = 320, want_height = RS_SCREEN_H;
static int sram_timer;
static const char *store_req;
static char store_market[256];
static rsb_review review;                       /* the review prompt's counters (review.cfg) */
static int review_req, from_menu, best_before;  /* a request for the frontend; the running game's launch */

/* input bookkeeping */
static rsb_pointer ptr;
static long ptr_id = -1;
static int back_now;
static uint16_t pad_mask[RS_PAD_MAX];                     /* buttons held when a program started: ignored until released */
static long consumed[RSB_FINGERS];              /* fingers down when a program started or a hold ended */
static int nconsumed;
static long pause_finger = -1;
static long ids_prev[RSB_FINGERS], ids_now[RSB_FINGERS];   /* the fingers of the previous frame and this one */
static int nids_prev, nids_now;
static uint16_t touch_now;                      /* the buttons the touches hold (the frontend's drawn buttons) */
static int touch_logging, gamepad_now;

static void say(const char *fmt, const char *a, const char *b)
{
    char line[512];
    snprintf(line, sizeof line, fmt, a, b);
    if (log_fn) log_fn(line);
    else fprintf(stderr, "%s\n", line);
}
void rsb_set_log(void (*fn)(const char *line)) { log_fn = fn; }

/* ---- files -------------------------------------------------------------------------------------------------------- */
static void file_path(char *out, size_t n, const char *id, const char *ext)
{
    snprintf(out, n, "%s%s%s", save_dir, id, ext);
}

/* ---- the in-app purchase (apps/common/iap.h: the iOS app, rsb_iap_product) ---------------------------------------
 * The games it opens are built in, locked until it is bought (their card_locked shown); the unlock is kept in
 * <save_dir>/unlock.cfg ("1"), given back to iap_init at start-up, then written whenever the store says otherwise
 * (bought, restored, refunded). Without a product (the other builds) nothing of this runs. */
static unsigned iap_seen;

static int unlock_read(void)
{
    char p[1200];
    file_path(p, sizeof p, "unlock", ".cfg");
    FILE *f = fopen(p, "rb");
    int c = f ? fgetc(f) : 0;
    if (f) fclose(f);
    return c == '1';
}

static void unlock_poll(void)
{
    if (!rsb_iap_product[0] || iap_changes() == iap_seen) return;
    iap_seen = iap_changes();
    int on = iap_unlocked() != 0;
    if (on == unlock_read()) return;
    char p[1200];
    file_path(p, sizeof p, "unlock", ".cfg");
    FILE *f = fopen(p, "wb");
    if (f) {
        fputs(on ? "1\n" : "0\n", f);
        fclose(f);
    }
    say("in-app purchase: %s%s", on ? "unlocked" : "locked", "");
}

int rsb_locked(int g)
{
    if (g < 0 || g >= rsb_ngames) return 1;
    const rsb_entry *e = &rsb_games[g];
    return e->module == NULL || (e->card_locked.w && !iap_unlocked());
}

int rsb_best(int g)
{
    if (g < 0 || g >= rsb_ngames || rsb_games[g].best_off < 0) return -1;
    const rsb_entry *e = &rsb_games[g];
    char p[1200];
    uint8_t buf[64];
    uint8_t *src = NULL;
    if (g == cur) src = rs_host_sram();          /* the running game: its live copy */
    else {
        file_path(p, sizeof p, e->id, ".srm");
        FILE *f = fopen(p, "rb");
        if (!f) return -1;
        size_t n = fread(buf, 1, sizeof buf, f);
        fclose(f);
        if (n < (size_t)e->best_off + 2) return -1;
        src = buf;
    }
    if (e->magic && memcmp(src, e->magic, 4)) return -1;
    if (e->best_off + 2 > 64) return -1;
    int v = src[e->best_off] | src[e->best_off + 1] << 8;
    return v > 0 ? v : -1;
}

static void sram_write(void)
{
    if (cur < 0) return;
    char p[1200];
    file_path(p, sizeof p, rsb_games[cur].id, ".srm");
    if (rsd_sram_save(p)) say("cannot write %s%s", p, "");
    rs_host_sram_dirty(1);
    sram_timer = 0;
}

static void state_write(void)
{
    if (cur < 0) return;
    const rs_game *g = rsb_games[cur].module->game_main();
    size_t size = rs_host_state_size(g);
    if (!size) return;
    void *buf = malloc(size);
    if (!buf) return;
    size_t n = rs_host_state_save(buf, size);
    char p[1200];
    file_path(p, sizeof p, rsb_games[cur].id, ".state");
    if (n && rsd_save_file(p, buf, n)) say("cannot write %s%s", p, "");
    free(buf);
}

/* ---- modules ------------------------------------------------------------------------------------------------------------ */
/* each game's translated text: its pack, captured from its constructor at load (rsb_glue.c), installed at its start */
static struct { const char *id; const rs_i18n_pack *pack; } packs[32];
static int npacks;
void rsb_i18n_capture(const char *id, const rs_i18n_pack *pack)
{
    if (npacks < 32) { packs[npacks].id = id; packs[npacks].pack = pack; npacks++; }
}
/* the launcher's own pack (build/bundle/gen/launcher/game_text.c, compiled with rs_i18n_register renamed to this) */
static const rs_i18n_pack *launcher_pack;
void rsb_launcher_i18n_capture(const rs_i18n_pack *pack);
void rsb_launcher_i18n_capture(const rs_i18n_pack *pack) { launcher_pack = pack; }

static const rs_i18n_pack *pack_of(const char *id)
{
    for (int i = 0; i < npacks; i++)
        if (!strcmp(packs[i].id, id)) return packs[i].pack;
    return NULL;
}

static void snapshot(const rsb_module *m, char **data, size_t *n)
{
    *n = (size_t)(m->data_end - m->data_begin);
    *data = malloc(*n ? *n : 1);
    if (*data) memcpy(*data, m->data_begin, *n);
}

static void fresh_start(const rsb_module *m, const char *data, size_t n)
{
    if (data) memcpy(m->data_begin, data, n);
    memset(m->bss_begin, 0, (size_t)(m->bss_end - m->bss_begin));
}

static void consume_input(void)
{
    nconsumed = 0;
    if (ptr.down && ptr_id != -1 && nconsumed < RSB_FINGERS) consumed[nconsumed++] = ptr_id;
    for (int p = 0; p < RS_PAD_MAX; p++) pad_mask[p] = 0xffff;   /* until each button is seen released */
}

static void stop_current(void)
{
    if (cur >= 0) {
        if (!held) state_write();               /* a held game has not moved since its state was loaded */
        sram_write();
    }
    rs_host_shutdown();
}

/* a game launched from the menu came back to it: count it, maybe ask for a review (the frontend, in the menu) */
static void review_game_ended(int g, int after)
{
    char p[1200];
    file_path(p, sizeof p, "review", ".force");
    FILE *f = fopen(p, "rb");                   /* the debug flag: <saves>/review.force or RSB_REVIEW_FORCE=1 */
    const char *env = getenv("RSB_REVIEW_FORCE");
    int force = f || (env && *env && *env != '0');
    if (f) fclose(f);
    int new_best = best_before > 0 && after > best_before;
    int why = rsb_review_game_ended(&review, rsb_review_today(), new_best, force);
    file_path(p, sizeof p, "review", ".cfg");
    if (rsb_review_save(&review, p)) say("cannot write %s%s", p, "");
    char s[200];
    snprintf(s, sizeof s, "review: %s best %d -> %d; %d games on %d days, %d returns, %d requests%s", rsb_games[g].id,
             best_before, after, review.games, review.days, review.returns, review.asks,
             why == RSB_REVIEW_ASK_BEST ? ": ask (a new best)" : why == RSB_REVIEW_ASK_RETURNS ? ": ask (the 10th return)"
             : why == RSB_REVIEW_ASK_FORCED ? ": ask (forced)" : "");
    say("%s%s", s, "");
    if (why) review_req = 1;
}

static void start(int g, int fresh)
{
    int ended = cur >= 0 && g < 0 && from_menu ? cur : -1;     /* a game back to the menu: its best now */
    int after = ended >= 0 ? rsb_best(cur) : -1;
    stop_current();
    fresh_start(&rsb_module_house, snap_house.data, snap_house.n);
    memset(rs_host_sram(), 0, RS_SRAM_SIZE);
    rs_host_sram_dirty(1);
    held = 0;
    if (g < 0) {
        cur = RSB_LAUNCHER;
        say("start: %s%s", "launcher", "");
        rs_host_set_width(want_width);
        rs_host_set_height(RS_SCREEN_H);        /* (the launcher: 240 lines) */
        rs_i18n_register(launcher_pack);        /* its strings, in the player's language */
        rs_host_init(rsb_launcher_game());
        cur_width = rs_screen_width();
        if (ended >= 0) review_game_ended(ended, after);
    } else {
        const rsb_entry *e = &rsb_games[g];
        fresh_start(e->module, snap_game[g].data, snap_game[g].n);
        char p[1200];
        file_path(p, sizeof p, e->id, ".srm");
        rsd_sram_load(p);
        rs_host_sram_dirty(1);
        from_menu = cur == RSB_LAUNCHER;
        best_before = rsb_best(g);              /* (from its file: it is not the running program yet) */
        cur = g;
        last_game = g;
        say("start: %s%s", e->id, fresh ? " (fresh)" : "");
        rs_host_set_width(want_width);          /* a game that has not opted in to widescreen gets 320 */
        rs_host_set_height(want_height);        /* ... to the tall screen, 240 */
        rs_i18n_register(pack_of(e->id));       /* its strings, in the player's language */
        rs_host_init(e->module->game_main());
        cur_width = rs_screen_width();
        if (!fresh) {
            file_path(p, sizeof p, e->id, ".state");
            size_t n = 0;
            void *d = rsd_load_file(p, &n);
            if (d) {
                if (rs_host_state_load(d, n) == 0) {
                    held = 1;
                    rs_host_render();           /* the picture of the saved moment, under the play sign */
                    say("resumed %s from %s", e->id, p);
                } else {
                    say("%s: the saved state does not fit this build, a new start%s", e->id, "");
                }
                free(d);
            }
        }
    }
    sram_timer = 0;
    touch_now = 0;
    consume_input();
}

void rsb_start(int g, int fresh) { start(g >= 0 && g < rsb_ngames && rsb_games[g].module ? g : RSB_LAUNCHER, fresh); }

int rsb_init(const char *dir, int width)
{
    snprintf(save_dir, sizeof save_dir, "%s", dir ? dir : "");
    if (!snap_game) {                           /* once per process: the modules are pristine only before they run */
        snapshot(&rsb_module_house, &snap_house.data, &snap_house.n);
        snap_game = calloc((size_t)rsb_ngames, sizeof *snap_game);
        if (!snap_game) return -1;
        for (int g = 0; g < rsb_ngames; g++)
            if (rsb_games[g].module) snapshot(rsb_games[g].module, &snap_game[g].data, &snap_game[g].n);
    }
    quit = held = 0;                            /* (Android may run main() again in the same process) */
    char p[1200];
    file_path(p, sizeof p, "review", ".cfg");
    rsb_review_load(&review, p);
    if (rsb_iap_product[0]) {                   /* the in-app purchase: what the last run knew, then the store */
        iap_init(rsb_iap_product, unlock_read());
        iap_seen = iap_changes();
    }
    review_req = from_menu = 0;
    pending = -2;
    nconsumed = 0;
    memset(&ptr, 0, sizeof ptr);
    ptr_id = pause_finger = -1;
    nids_prev = nids_now = 0;
    rsb_set_width(width);
    cur = RSB_LAUNCHER;
    start(RSB_LAUNCHER, 0);
    return 0;
}

void rsb_set_width(int w)
{
    if (w < 320) w = 320;
    if (w > 560) w = 560;
    want_width = w & ~1;
}

int  rsb_width(void) { return cur_width; }

/* (past RS_SCREEN_H_MAX too: the display's shape, for a zoomed game's columns, rs_screen_zoom) */
void rsb_set_height(int h) { want_height = h < RS_SCREEN_H ? RS_SCREEN_H : h > RS_SCREEN_W * 4 ? RS_SCREEN_W * 4 : h; }

void rsb_resize(int w)
{
    rsb_set_width(w);
    if (cur >= 0) {             /* a widescreen or tall game restarts from a state (a zoomed one also when only the
                                 * display's shape changes: rs_host_change_size tells) */
        cur_width = rs_host_change_size(want_width, want_height);
        return;
    }
    if (want_width == cur_width) return;
    start(RSB_LAUNCHER, 0);                     /* the launcher lays its cards out again */
}
int  rsb_screen_w(void) { return rs_screen_width(); }
int  rsb_screen_h(void) { return rs_screen_height(); }
int  rsb_in_game(void) { return cur >= 0; }
int  rsb_held(void) { return held; }
void rsb_hold(void) { if (cur >= 0) held = 1; }
int  rsb_wants_quit(void) { return quit; }
int  rsb_last_game(void) { return last_game; }
const char *rsb_current_id(void) { return cur >= 0 ? rsb_games[cur].id : "launcher"; }
const rsb_pointer *rsb_pointer_get(void) { return &ptr; }
int  rsb_back_pressed(void) { return back_now; }
void rsb_quit(void) { quit = 1; }
int  rsb_gamepad(void) { return gamepad_now; }

void rsb_launch(int g)
{
    if (g >= 0 && g < rsb_ngames && !rsb_locked(g)) {
        pending = g;
        pending_fresh = 0;
    }
}

void rsb_open_store(void)
{
    snprintf(store_market, sizeof store_market, "market://details?id=%s", rsb_paid_id);
    store_req = store_market;
}
const char *rsb_store_request(void)
{
    const char *r = store_req;
    store_req = NULL;
    return r;
}
const char *rsb_store_fallback(void) { return rsb_paid_url; }
int rsb_review_request(void)
{
    int r = review_req;
    review_req = 0;
    return r;
}

void rsb_suspend(void)
{
    if (cur >= 0) {
        if (!held) state_write();
        sram_write();
        held = 1;
    }
}

void rsb_shutdown(void)
{
    stop_current();
    cur = RSB_LAUNCHER;
}

void rsb_pause_rect(float aspect, float *x, float *y, float *w, float *h)
{
    float s = 0.11f, m = 0.025f;
    if (aspect < 1) s *= aspect, m *= aspect;   /* a tall display: as big as on its side (a share of the width) */
    *h = s;
    *w = s / aspect;
    *y = m;
    *x = 1.0f - m / aspect - *w;
}

void rsb_back_rect(float aspect, float *x, float *y, float *w, float *h)
{
    rsb_pause_rect(aspect, x, y, w, h);
    *x = aspect < 1 ? 0.025f : 0.025f / aspect; /* the mirror of the pause button */
}

/* a finger on the back button (a slightly bigger target than the drawn button) */
static int in_back(const rsb_finger *fg, float aspect)
{
    float bx, by, bw, bh;
    rsb_back_rect(aspect, &bx, &by, &bw, &bh);
    return fg->x <= bx + bw * 1.3f && fg->y <= by + bh * 1.3f;
}

/* ---- one frame ---------------------------------------------------------------------------------------------------------- */
static int is_consumed(long id)
{
    for (int i = 0; i < nconsumed; i++)
        if (consumed[i] == id) return 1;
    return 0;
}

static void forget_lifted(const rsb_input *in)
{
    for (int i = 0; i < nconsumed;) {
        int still = 0;
        for (int f = 0; f < in->nfingers; f++) still |= in->finger[f].id == consumed[i];
        if (still) i++;
        else consumed[i] = consumed[--nconsumed];
    }
}


/* the fingers of the previous frame: a finger not among them has just gone down (the back and pause buttons only take
 * a finger that goes down on them, not one that slides onto them) */
static void remember_fingers(const rsb_input *in)
{
    memcpy(ids_prev, ids_now, sizeof ids_now);
    nids_prev = nids_now;
    nids_now = 0;
    for (int f = 0; f < in->nfingers && nids_now < RSB_FINGERS; f++) ids_now[nids_now++] = in->finger[f].id;
}
static int is_new(long id)
{
    for (int i = 0; i < nids_prev; i++)
        if (ids_prev[i] == id) return 0;
    return 1;
}
/* the launcher's pointer: the first finger down, in screen pixels */
static void pointer_update(const rsb_input *in, const rsb_view *v)
{
    int found = -1;
    for (int f = 0; f < in->nfingers; f++)
        if (in->finger[f].id == ptr_id) found = f;
    if (found < 0 && ptr.down) {                /* lifted */
        ptr.down = 0;
        ptr.released = 1;
        ptr.pressed = 0;
        ptr_id = -1;
        return;
    }
    ptr.pressed = ptr.released = 0;
    if (found < 0) {
        for (int f = 0; f < in->nfingers; f++)
            if (!is_consumed(in->finger[f].id)) { found = f; break; }
        if (found < 0) return;
        ptr_id = in->finger[found].id;
        ptr.down = ptr.pressed = 1;
        ptr.frames = 0;
    } else {
        ptr.frames++;
    }
    float fx = v && v->w > 0 ? (in->finger[found].x - v->x) / v->w : in->finger[found].x;
    float fy = v && v->h > 0 ? (in->finger[found].y - v->y) / v->h : in->finger[found].y;
    ptr.x = (int)(fx * rsb_screen_w());
    ptr.y = (int)(fy * rs_screen_height());
    if (ptr.pressed) { ptr.x0 = ptr.x; ptr.y0 = ptr.y; }
}

/* the running game reads the touches itself (rs_game.touch_native, docs/spec.md "Touch"): no zones, no drawn buttons */
static int native_touch(void) { return cur >= 0 && rsb_games[cur].module->game_main()->touch_native; }

/* a finger on the pause button (a slightly bigger target than the drawn button) */
static int in_pause(const rsb_finger *fg, float aspect)
{
    float px, py, pw, ph;
    rsb_pause_rect(aspect, &px, &py, &pw, &ph);
    return fg->x >= px - pw * 0.3f && fg->y <= py + ph * 1.3f;
}

/* the pause button: a finger that WENT DOWN on it presses the game's pause input (games.cfg pause=) while it stays
 * there; it never reaches the game. Then, a scheme of zones: each other finger presses the buttons of its zone. */
static uint16_t touch_buttons(const rsb_input *in, const rsb_view *v, float aspect, int zones)
{
    const rsb_touch *t = &rsb_games[cur].touch;
    uint16_t b = 0;
    int pause_seen = 0;
    for (int f = 0; f < in->nfingers; f++) {
        const rsb_finger *fg = &in->finger[f];
        if (is_consumed(fg->id)) continue;
        if (pause_finger == -1 && is_new(fg->id) && in_pause(fg, aspect)) pause_finger = fg->id;
        if (pause_finger == fg->id) {
            if (in_pause(fg, aspect)) b |= t->pause;
            pause_seen = 1;
            continue;
        }
        if (!zones || in_back(fg, aspect)) continue;
        int x = (int)(fg->x * 100), y = (int)(fg->y * 100);
        /* a tall display: a zone that does not span the whole height is in the band below the picture */
        float fb = v && v->zh > 0 ? (fg->y - v->zy) / v->zh : fg->y;
        int yb = fb < 0 ? -1 : (int)(fb * 100);
        for (int z = 0; z < t->nzones; z++) {
            const rsb_zone *zn = &t->zone[z];
            int yz = zn->y0 == 0 && zn->y1 == 100 ? y : yb;
            if (x >= zn->x0 && x < zn->x1 + (zn->x1 == 100) && yz >= zn->y0 && yz < zn->y1 + (zn->y1 == 100)) {
                b |= zn->buttons;
                break;
            }
        }
    }
    if (!pause_seen) pause_finger = -1;
    return b;
}

/* a touch-native game: the fingers that went down on its picture (not on the back or pause button, not consumed)
 * are its touches, in screen pixels, until they are lifted (they may then slide anywhere) */
static long game_ids[RSB_FINGERS];
static int ngame_ids, native_logged = -1;
static int native_touches(const rsb_input *in, const rsb_view *v, float aspect, rs_host_touch *out)
{
    int n = 0;
    long keep[RSB_FINGERS];
    int nkeep = 0;
    for (int f = 0; f < in->nfingers; f++) {
        const rsb_finger *fg = &in->finger[f];
        int mine = 0;
        for (int i = 0; i < ngame_ids; i++) mine |= game_ids[i] == fg->id;
        if (is_consumed(fg->id) || fg->id == pause_finger) continue;
        if (!mine && (!is_new(fg->id) || in_back(fg, aspect) ||
                      in_pause(fg, aspect)))
            continue;
        keep[nkeep++] = fg->id;
        if (n >= RS_TOUCH_MAX) continue;
        float fx = v && v->w > 0 ? (fg->x - v->x) / v->w : fg->x;
        float fy = v && v->h > 0 ? (fg->y - v->y) / v->h : fg->y;
        out[n].id = fg->id;
        out[n].x = (int)(fx * rsb_screen_w());
        out[n].y = (int)(fy * rs_screen_height());
        n++;
    }
    memcpy(game_ids, keep, sizeof keep[0] * (size_t)nkeep);
    ngame_ids = nkeep;
    return n;
}

void rsb_touch_log(int on) { touch_logging = on; }
const rsb_touch *rsb_touch_scheme(void) { return cur >= 0 && !native_touch() ? &rsb_games[cur].touch : NULL; }
uint16_t rsb_touch_held(void) { return cur >= 0 && !held ? touch_now : 0; }

static void touch_log_line(uint16_t b)
{
    static const char *names[16] = {"", "", "", "", "R", "L", "X", "A",                 /* RS_BTN_* bits 0..15 */
                                    "RIGHT", "LEFT", "DOWN", "UP", "START", "SELECT", "Y", "B"};
    char s[96] = "touch:";
    for (int i = 15; i >= 0; i--)
        if (b & (1u << i) && names[i][0]) {
            strncat(s, " ", sizeof s - strlen(s) - 1);
            strncat(s, names[i], sizeof s - strlen(s) - 1);
        }
    if (!b) strncat(s, " -", sizeof s - strlen(s) - 1);
    say("%s%s", s, "");
}

int rsb_frame(const rsb_input *in, const rsb_view *view)
{
    unlock_poll();
    back_now = in->back;
    gamepad_now = in->gamepad;
    forget_lifted(in);
    remember_fingers(in);
    pointer_update(in, view);
    float aspect = in->aspect > 0 ? in->aspect : 16.0f / 9;

    /* the in-game back button (top left): a finger that goes down on it is Back */
    long tapped = -1;
    if (cur >= 0)
        for (int f = 0; f < in->nfingers && tapped == -1; f++) {
            const rsb_finger *fg = &in->finger[f];
            if (!is_consumed(fg->id) && is_new(fg->id) && in_back(fg, aspect)) tapped = fg->id;
        }
    if (cur >= 0 && (in->back || tapped != -1)) {   /* Back in a game: its state is saved, the launcher */
        start(RSB_LAUNCHER, 0);
        if (tapped != -1 && nconsumed < RSB_FINGERS && !is_consumed(tapped)) consumed[nconsumed++] = tapped;
        ptr.down = 0;
        ptr_id = -1;
        back_now = 0;
        return 0;
    }
    if (held) {
        int wake = in->any;
        for (int f = 0; f < in->nfingers; f++) wake |= !is_consumed(in->finger[f].id);
        if (wake) {
            held = 0;
            consume_input();
            for (int f = 0; f < in->nfingers && nconsumed < RSB_FINGERS; f++)
                if (!is_consumed(in->finger[f].id)) consumed[nconsumed++] = in->finger[f].id;
            ptr.down = 0;
            ptr_id = -1;
        }
        return 0;
    }

    int native = native_touch();
    uint16_t touch = cur >= 0 ? touch_buttons(in, view, aspect, !native) : 0;
    rs_host_touch fingers[RS_TOUCH_MAX];
    int nf = native ? native_touches(in, view, aspect, fingers) : 0;
    if (!native) ngame_ids = 0;
    rs_host_set_touches(nf, fingers);
    if (touch_logging && cur >= 0 && touch != touch_now) touch_log_line(touch);
    if (touch_logging && native && nf != native_logged) {
        char s[64];
        if (nf) snprintf(s, sizeof s, "touch: native %d at %d,%d", nf, fingers[0].x, fingers[0].y);
        else snprintf(s, sizeof s, "touch: native -");
        say("%s%s", s, "");
    }
    native_logged = native ? nf : -1;
    touch_now = touch;
    for (int p = 0; p < RS_PAD_MAX; p++) {
        uint16_t b = in->pad[p];
        if (p == 0) b |= touch;
        pad_mask[p] &= b;                       /* a masked button counts again once released */
        b &= (uint16_t)~pad_mask[p];
        rs_host_set_pad_device(p, in->device[p]);
        rs_host_set_pad(p, b, in->connected[p]);
    }
    rs_host_frame();

    /* save RAM: written a moment after the game commits it */
    if (cur >= 0 && rs_host_sram_dirty(0) && ++sram_timer > 30) sram_write();

    if (pending != -2) {
        int g = pending;
        pending = -2;
        start(g, pending_fresh);
    }
    return 1;
}
