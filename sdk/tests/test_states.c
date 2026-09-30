/*
 * Save-state tests for a game (linked with the game, like the headless runner): determinism across a save and
 * a load, in the same process and in a fresh one, and the refusal of bad states.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   <game>_test_states [--opt K=V]... [--input SCRIPT] --frames N --after M --out PREFIX
 *       runs N frames, saves (PREFIX.state), runs M frames hashing the state and the picture after each one,
 *       loads the state, runs the same M frames again and compares; then refuses truncated, foreign, other-
 *       version and corrupted states (the running state must stay untouched) and fuzzes the sections;
 *       writes PREFIX.hashes and PREFIX.srm (the battery save at the save)
 *   <game>_test_states [--opt K=V]... [--input SCRIPT] --resume PREFIX
 *       a fresh process: loads PREFIX.srm, then PREFIX.state before any frame (resume at start-up), runs the M
 *       frames and compares with PREFIX.hashes
 * Save RAM is not part of a state: when the game writes it during the M frames (a new best score), the second
 * pass starts from the newer save RAM, as a player loading a state would.
 *   <game>_test_states --foreign FILE     a state of another game: refused, nothing changed
 *   <game>_test_states --list             the registered objects (tools/state_audit.py)
 *   --shot FILE.png                       the picture at the save (to see what a scenario saves)
 * The input script is the headless runner's (sdk/frontends/headless/rs_headless.c): "<frame> [Pn] BUTTONS|-",
 * "<frame> [Pn] tap BUTTONS"; the pads of frame f are those of the last events at or before f.
 * The hash leaves out the music's order position and row (libxmp restarts the track at the saved row: its
 * voices cannot be restored exactly); the track itself is compared.
 */
#include "rs_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS 4096
#define HDR 64

typedef struct { int frame, port; uint16_t buttons; } event_t;
static event_t ev[MAX_EVENTS];
static int nev, failures;

#define CHECK(c, ...) do { if (!(c)) { failures++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } \
                           else if (verbose) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int verbose = 1;

static void add_event(int f, int port, uint16_t b)
{
    if (nev >= MAX_EVENTS) return;
    int i = nev++;
    while (i > 0 && ev[i - 1].frame > f) { ev[i] = ev[i - 1]; i--; }
    ev[i].frame = f; ev[i].port = port; ev[i].buttons = b;
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
        char t[3][128] = {"", "", ""};
        int k = sscanf(line, "%127s %127s %127s", t[0], t[1], t[2]);
        line = nl ? nl + 1 : NULL;
        if (k < 2) continue;
        int f = atoi(t[0]), port = 0, i = 1;
        if ((t[1][0] == 'P' || t[1][0] == 'p') && t[1][1] >= '1' && t[1][1] <= '4' && !t[1][2]) { port = t[1][1] - '1'; i = 2; }
        if (i >= k) continue;
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

/* one frame with the pads the script gives for frame f (a pure function of f: the frames can be replayed) */
static void frame(int f)
{
    uint16_t pad[RS_PAD_MAX] = {0, 0, 0, 0};
    for (int e = 0; e < nev && ev[e].frame <= f; e++) pad[ev[e].port] = ev[e].buttons;
    for (int p = 0; p < RS_PAD_MAX; p++) rs_host_set_pad(p, pad[p], 1);
    rs_host_frame();
}

static uint64_t fnv(uint64_t h, const void *d, size_t n)
{
    const uint8_t *p = d;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void fix_checksum(uint8_t *s)
{
    uint32_t n = le32(s + 56), h = 2166136261u;
    for (uint32_t i = 0; i < n; i++) h = (h ^ s[HDR + i]) * 16777619u;
    put32(s + 60, h);
}

/* a section's payload in a state (tag: 4 chars), or NULL */
static uint8_t *section(uint8_t *s, const char *tag, uint32_t *len)
{
    uint32_t n = le32(s + 56), pos = 0;
    while (pos + 8 <= n) {
        uint32_t l = le32(s + HDR + pos + 4);
        if (!memcmp(s + HDR + pos, tag, 4)) { *len = l; return s + HDR + pos + 8; }
        pos += 8 + l;
    }
    return NULL;
}

/* the music's order position and row (the last 8 bytes of MUS) do not take part in the comparisons */
static void mask_music(uint8_t *s)
{
    uint32_t len;
    uint8_t *m = section(s, "MUS ", &len);
    if (m && len >= 8) memset(m + len - 8, 0, 8);
    memset(s + 60, 0, 4);                           /* and the checksum, which covers them */
}

static size_t g_size;
static uint8_t *save_now(void)
{
    uint8_t *b = malloc(g_size);
    if (!b || !rs_host_state_save(b, g_size)) { free(b); return NULL; }
    return b;
}
static size_t used(const uint8_t *s) { return HDR + le32(s + 56); }

static uint64_t frame_hash(void)
{
    uint8_t *s = save_now();
    if (!s) return 0;
    mask_music(s);
    uint64_t h = fnv(0xcbf29ce484222325ull, s, g_size);
    h = fnv(h, rs_host_framebuffer(), RS_SCREEN_W * RS_SCREEN_H * 2);
    free(s);
    return h;
}

static int same_state(const uint8_t *a, const uint8_t *b)
{
    uint8_t *x = malloc(g_size), *y = malloc(g_size);
    memcpy(x, a, g_size);
    memcpy(y, b, g_size);
    mask_music(x);
    mask_music(y);
    int r = !memcmp(x, y, g_size);
    free(x);
    free(y);
    return r;
}

/* a load that must be refused, leaving the running state as it was */
static void refused(const char *what, const uint8_t *s, size_t n)
{
    uint8_t *before = save_now();
    int r = rs_host_state_load(s, n);
    uint8_t *after = save_now();
    CHECK(r != 0 && before && after && !memcmp(before, after, g_size), "refuses %s, nothing changed", what);
    free(before);
    free(after);
}

static void refusals(const uint8_t *a)
{
    size_t n = used(a);
    uint8_t *s = malloc(g_size);
    refused("a truncated state (1 byte short)", a, n - 1);
    refused("a truncated state (half)", a, n / 2);
    refused("a header alone", a, HDR);
    refused("an empty buffer", a, 0);
#define PATCH(off, what) do { memcpy(s, a, g_size); s[off] ^= 0x20; refused(what, s, g_size); } while (0)
    PATCH(0, "a bad magic");
    PATCH(4, "another format version");
    PATCH(8, "another game's id");
    PATCH(28, "another game version");
    PATCH(44, "another state_version");
    PATCH(50, "another build (hash)");
    PATCH(56, "a wrong payload size");
    PATCH(HDR + 100, "a corrupted payload (checksum)");
#undef PATCH
    /* checksum right, contents wrong: the sections are checked too */
    uint32_t len;
    uint8_t *p;
    memcpy(s, a, g_size); s[HDR] = 'X'; fix_checksum(s);
    refused("an unknown section", s, g_size);
    memcpy(s, a, g_size);
    if ((p = section(s, "PPU ", &len))) {
        put32(p + RS_TILE_MAX * 32 + RS_TILE_MAX / 8 + 4 + 512 + RS_OAM_MAX * 11 + RS_OAM_MAX / 8 + 64, 33);
        fix_checksum(s);
        refused("a map 33 tiles wide", s, g_size);
    }
    memcpy(s, a, g_size);
    if ((p = section(s, "GAME", &len))) { put32(p - 4, len - 1); fix_checksum(s); refused("a short GAME section", s, g_size); }
    memcpy(s, a, g_size);
    if ((p = section(s, "PTRS", &len)) && len >= 8) { put32(p, 0x7fffffff); fix_checksum(s); refused("a pointer to nowhere", s, g_size); }
    memcpy(s, a, g_size);
    if ((p = section(s, "MUS ", &len))) { p[0] = 1; memcpy(p + 4, "music/none.xm", 14); fix_checksum(s); refused("a missing music", s, g_size); }
    memcpy(s, a, g_size);
    if ((p = section(s, "APU ", &len))) { put32(p + 4, 500); fix_checksum(s); refused("a volume of 500", s, g_size); }
    /* fuzz: random bytes of the console's sections changed, checksum fixed: loaded or refused, never a crash */
    uint32_t rng = 12345, loaded = 0, refused_n = 0, unchanged_ok = 1;
    uint32_t game_off = 0;
    if ((p = section((uint8_t *)a, "GAME", &len))) game_off = (uint32_t)(p - a - HDR - 8);
    rs_host_state_load(a, g_size);                   /* a loaded state is where each refused load must leave us */
    uint8_t *before = save_now();
    for (int it = 0; it < 300; it++) {
        memcpy(s, a, g_size);
        for (int k = 0; k < 1 + it % 4; k++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            uint32_t at = HDR + rng % (game_off ? game_off : (uint32_t)(n - HDR));
            if (it % 3 == 0) at = HDR + (rng % 64);                     /* often the section headers */
            s[at] = (uint8_t)(s[at] ^ (1u << (rng >> 24 & 7)) ^ (it % 5 == 0 ? 0xff : 0));
        }
        fix_checksum(s);
        if (!rs_host_state_load(s, g_size)) {
            loaded++;
            rs_host_state_load(a, g_size);
        } else {
            refused_n++;
            uint8_t *now = save_now();
            if (!now || memcmp(now, before, g_size)) unchanged_ok = 0;
            free(now);
        }
    }
    free(before);
    CHECK(unchanged_ok, "fuzz: 300 altered states, %u loaded, %u refused (nothing changed)", loaded, refused_n);
    free(s);
}

static void list_line(const char *kind, const char *name, size_t size)
{
    printf("%s %s %u\n", kind, name, (unsigned)size);
}

int main(int argc, char **argv)
{
    int n = -1, m = -1, list = 0;
    const char *out = NULL, *resume = NULL, *foreign = NULL, *shot = NULL;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--opt") && v) rsd_option(argv[++i]);
        else if (!strcmp(a, "--input") && v) { if (load_script(argv[++i])) return 2; }
        else if (!strcmp(a, "--frames") && v) n = atoi(argv[++i]);
        else if (!strcmp(a, "--after") && v) m = atoi(argv[++i]);
        else if (!strcmp(a, "--out") && v) out = argv[++i];
        else if (!strcmp(a, "--resume") && v) resume = argv[++i];
        else if (!strcmp(a, "--foreign") && v) foreign = argv[++i];
        else if (!strcmp(a, "--list")) list = 1;
        else if (!strcmp(a, "--shot") && v) shot = argv[++i];
        else { fprintf(stderr, "unknown or incomplete option: %s\n", a); return 2; }
    }
    const rs_game *g = rs_game_main();
    g_size = rs_host_state_size(g);
    if (list) {
        rs_host_state_list(list_line);
        printf("size %u\n", (unsigned)g_size);
        return 0;
    }
    if (!g_size) { printf("states: %s has no save states\n", g->id); return 1; }
    char path[512];
    if (resume) {                                    /* the battery save as it was at the save (it is not in states) */
        snprintf(path, sizeof path, "%s.srm", resume);
        rsd_sram_load(path);
    }
    rs_host_init(g);

    if (foreign) {
        size_t sz;
        uint8_t *f = rsd_load_file(foreign, &sz);
        if (!f) { printf("cannot read %s\n", foreign); return 2; }
        for (int f0 = 0; f0 < 10; f0++) frame(f0);
        refused("another game's state", f, sz);
        free(f);
    } else if (resume) {
        size_t sz, hz;
        snprintf(path, sizeof path, "%s.state", resume);
        uint8_t *s = rsd_load_file(path, &sz);
        snprintf(path, sizeof path, "%s.hashes", resume);
        char *hs = rsd_load_file(path, &hz);
        if (!s || !hs) { printf("cannot read %s.state / .hashes\n", resume); return 2; }
        CHECK(!rs_host_state_load(s, sz), "a fresh process loads the state before its first frame");
        char *p = hs;
        n = (int)strtol(p, &p, 10);
        m = (int)strtol(p, &p, 10);
        int bad = -1;
        for (int i = 0; i < m; i++) {
            frame(n + i);
            unsigned long long want = strtoull(p, &p, 16);
            if (bad < 0 && frame_hash() != want) bad = n + i;
        }
        CHECK(bad < 0, "fresh process: frames %d..%d the same as after the save%s", n, n + m - 1,
              bad < 0 ? "" : " (differs from a frame on)");
        if (bad >= 0) printf("  first different frame: %d\n", bad);
        free(s);
        free(hs);
    } else {
        if (n < 0 || m <= 0 || !out) { fprintf(stderr, "--frames N --after M --out PREFIX\n"); return 2; }
        for (int f = 0; f < n; f++) frame(f);
        if (shot) rsd_write_png(shot, rs_host_framebuffer(), 1);
        uint8_t *a = save_now(), *a2 = save_now();
        CHECK(a && a2 && !memcmp(a, a2, g_size), "saves at frame %d (%u of %u bytes), twice the same", n,
              a ? (unsigned)used(a) : 0, (unsigned)g_size);
        if (!a) return 1;
        CHECK(rs_host_state_size(g) == g_size, "the size is fixed");
        snprintf(path, sizeof path, "%s.srm", out);  /* for --resume: a frontend loads the .srm, then the state */
        rsd_save_file(path, rs_host_sram(), RS_SRAM_SIZE);
        uint64_t *h1 = calloc((size_t)m, 8), *h2 = calloc((size_t)m, 8);
        for (int i = 0; i < m; i++) { frame(n + i); h1[i] = frame_hash(); }
        uint8_t *f1 = save_now();
        CHECK(!rs_host_state_load(a, g_size), "loads it back");
        uint8_t *a3 = save_now();
        CHECK(a3 && same_state(a, a3), "the loaded state saves the same bytes");
        uint32_t ml1 = 0, ml2 = 0;
        uint8_t *m1 = section(a, "MUS ", &ml1), *m2 = a3 ? section(a3, "MUS ", &ml2) : NULL;
        CHECK(m1 && m2 && ml1 == ml2 && !memcmp(m1, m2, ml1), "the music restarts: the same track, order position %u, row %u",
              m1 ? le32(m1 + ml1 - 8) : 0, m1 ? le32(m1 + ml1 - 4) : 0);
        int bad = -1;
        for (int i = 0; i < m; i++) { frame(n + i); h2[i] = frame_hash(); if (bad < 0 && h1[i] != h2[i]) bad = n + i; }
        uint8_t *f2 = save_now();
        CHECK(bad < 0, "frames %d..%d replayed the same after the load (state and picture)", n, n + m - 1);
        if (bad >= 0) printf("  first different frame: %d\n", bad);
        CHECK(f1 && f2 && same_state(f1, f2), "the same state at frame %d", n + m);
        snprintf(path, sizeof path, "%s.state", out);
        rsd_save_file(path, a, used(a));
        snprintf(path, sizeof path, "%s.hashes", out);
        FILE *hf = fopen(path, "w");
        if (hf) {
            fprintf(hf, "%d %d\n", n, m);
            for (int i = 0; i < m; i++) fprintf(hf, "%016llx\n", (unsigned long long)h1[i]);
            fclose(hf);
        }
        verbose = 0;
        int before = failures;
        refusals(a);
        verbose = 1;
        CHECK(failures == before, "refuses bad states (truncated, foreign, other version or build, corrupted)");
        free(h1); free(h2); free(a); free(a2); free(a3); free(f1); free(f2);
    }
    rs_host_shutdown();
    printf("states: %s %s\n", out ? out : resume ? resume : foreign, failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
