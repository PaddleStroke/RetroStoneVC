/*
 * RetroStone VC SDK: save states.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A state = a 64-byte header and tagged sections (docs/spec.md "Save states"):
 *   header   "RSVC", format version, header size, game id, game version, the game's state_version, the build
 *            hash (format, pointer size, the registered objects' names and sizes, the asset pack's contents),
 *            payload size and an FNV-1a checksum of the payload
 *   sections tag (4 chars), size (u32), payload, always in this order:
 *            CORE (frame counter, global RNG, pad edges), PPU (VRAM, maps, CGRAM, OAM, registers, viewports),
 *            APU (voices, echo), MUS (the music track and position), TEXT (text set-up),
 *            GAME (the game's registered objects, byte for byte, pointers zeroed), PTRS (its pointers as
 *            object + offset), END
 * The size is a fixed maximum for a game build (libretro frontends require it); the unused tail is zeroed.
 * Loading checks everything (header, checksum, every section, every pointer) before it changes anything.
 */
#include "rs_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STATE_FORMAT 1
#define HDR_SIZE     64
#define MAX_VARS     192
#define MAX_PTRS     32
#define MAX_REFS     32
#define MAX_RASTERS  16
#define NAME_LEN     48
#define REF_BASE     0x10000u         /* region numbers: 0 = NULL, 1..n = var n-1, REF_BASE + i = ref i */

typedef struct { char name[NAME_LEN]; uint8_t *data; size_t size; } var_t;
typedef struct { char name[NAME_LEN]; void *pv; int var; size_t off; } ptr_t;   /* var/off: where it lives, or -1 */
typedef struct { char name[NAME_LEN]; const uint8_t *data; size_t size; } ref_t;
typedef struct { char name[NAME_LEN]; rs_raster_fn fn; } raster_t;

static var_t    g_vars[MAX_VARS];
static ptr_t    g_ptrs[MAX_PTRS];
static ref_t    g_refs[MAX_REFS];
static raster_t g_rasters[MAX_RASTERS];
static int      g_nvars, g_nptrs, g_nrefs, g_nrasters;
static const rs_game *g_frozen;      /* the game whose registrations these are */
static int      g_registering, g_reg_error;
static size_t   g_size, g_game_bytes;
static uint64_t g_layout_hash, g_build_hash;
static int      g_build_ok;

/* ---- writer / reader ------------------------------------------------------------------------------------ */
void wr_bytes(rs_wr *w, const void *d, size_t n)
{
    if (w->err || n > w->cap - w->n) { w->err = 1; return; }
    if (d) memcpy(w->p + w->n, d, n);
    else memset(w->p + w->n, 0, n);
    w->n += n;
}
void wr_u8(rs_wr *w, uint8_t v) { wr_bytes(w, &v, 1); }
void wr_u16(rs_wr *w, uint16_t v)
{
    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    wr_bytes(w, b, 2);
}
void wr_u32(rs_wr *w, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    wr_bytes(w, b, 4);
}
void rd_bytes(rs_rd *r, void *d, size_t n)
{
    if (r->err || n > r->n - r->pos) {
        r->err = 1;
        if (d) memset(d, 0, n);
        return;
    }
    if (d) memcpy(d, r->p + r->pos, n);
    r->pos += n;
}
uint8_t rd_u8(rs_rd *r)
{
    uint8_t b = 0;
    rd_bytes(r, &b, 1);
    return b;
}
uint16_t rd_u16(rs_rd *r)
{
    uint8_t b[2];
    rd_bytes(r, b, 2);
    return (uint16_t)(b[0] | b[1] << 8);
}
uint32_t rd_u32(rs_rd *r)
{
    uint8_t b[4];
    rd_bytes(r, b, 4);
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* ---- hashes ----------------------------------------------------------------------------------------------- */
static uint64_t fnv64(uint64_t h, const void *d, size_t n)
{
    const uint8_t *p = d;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}
static uint64_t fnv64_str(uint64_t h, const char *s) { return fnv64(h, s, strlen(s) + 1); }
static uint64_t fnv64_u32(uint64_t h, uint32_t v)
{
    uint8_t b[4];
    put32(b, v);
    return fnv64(h, b, 4);
}
static uint32_t fnv32(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

/* ---- registration ------------------------------------------------------------------------------------------ */
static int reg_ok(const char *what, const char *name)
{
    if (!g_registering) {
        rs_log("state: %s(%s) outside rs_game.state: ignored", what, name ? name : "?");
        return 0;
    }
    if (!name || !*name || strlen(name) >= NAME_LEN) {
        rs_log("state: %s: a name of 1..%d characters is needed", what, NAME_LEN - 1);
        g_reg_error = 1;
        return 0;
    }
    return 1;
}

void rs_state_var(const char *name, void *data, size_t size)
{
    if (!reg_ok("rs_state_var", name)) return;
    for (int i = 0; i < g_nvars; i++)
        if (!strcmp(g_vars[i].name, name)) { rs_log("state: %s registered twice", name); g_reg_error = 1; return; }
    if (g_nvars >= MAX_VARS || !data || !size || size > 64u * 1024 * 1024) {
        rs_log("state: cannot register %s (%u bytes; %d objects at most)", name, (unsigned)size, MAX_VARS);
        g_reg_error = 1;
        return;
    }
    var_t *v = &g_vars[g_nvars++];
    snprintf(v->name, sizeof v->name, "%s", name);
    v->data = data;
    v->size = size;
}

void rs_state_ptr(const char *name, void *pointer_variable)
{
    if (!reg_ok("rs_state_ptr", name)) return;
    if (g_nptrs >= MAX_PTRS || !pointer_variable) {
        rs_log("state: cannot register the pointer %s (%d at most)", name, MAX_PTRS);
        g_reg_error = 1;
        return;
    }
    ptr_t *p = &g_ptrs[g_nptrs++];
    snprintf(p->name, sizeof p->name, "%s", name);
    p->pv = pointer_variable;
    p->var = -1;
}

void rs_state_ref(const char *name, const void *data, size_t size)
{
    if (!reg_ok("rs_state_ref", name)) return;
    if (g_nrefs >= MAX_REFS || !data || !size) {
        rs_log("state: cannot register the reference %s (%d at most)", name, MAX_REFS);
        g_reg_error = 1;
        return;
    }
    ref_t *r = &g_refs[g_nrefs++];
    snprintf(r->name, sizeof r->name, "%s", name);
    r->data = data;
    r->size = size;
}

void rs_state_raster(const char *name, rs_raster_fn fn)
{
    if (!reg_ok("rs_state_raster", name)) return;
    if (g_nrasters >= MAX_RASTERS || !fn) {
        rs_log("state: cannot register the raster callback %s (%d at most)", name, MAX_RASTERS);
        g_reg_error = 1;
        return;
    }
    snprintf(g_rasters[g_nrasters].name, NAME_LEN, "%s", name);
    g_rasters[g_nrasters++].fn = fn;
}

static int var_of(const void *p, size_t need, size_t *off)
{
    uintptr_t a = (uintptr_t)p;
    for (int i = 0; i < g_nvars; i++) {
        uintptr_t b = (uintptr_t)g_vars[i].data;
        if (a >= b && a - b <= g_vars[i].size && need <= g_vars[i].size - (a - b)) {
            *off = a - b;
            return i;
        }
    }
    return -1;
}

/* Registers the game's objects once (they are static: the size never changes) and computes the fixed size. */
static int freeze(const rs_game *game)
{
    if (game == g_frozen) return g_size != 0;
    g_frozen = game;
    g_nvars = g_nptrs = g_nrefs = g_nrasters = 0;
    g_size = g_game_bytes = 0;
    g_build_ok = 0;
    g_reg_error = 0;
    if (!game || !game->state) return 0;
    g_registering = 1;
    game->state();
    g_registering = 0;
    for (int i = 0; i < g_nptrs; i++) {
        size_t off = 0;
        g_ptrs[i].var = var_of(g_ptrs[i].pv, sizeof(void *), &off);
        g_ptrs[i].off = off;
    }
    if (g_reg_error) {
        rs_log("state: registration errors: save states are off");
        return 0;
    }
    uint64_t h = 0xcbf29ce484222325ull;
    static const uint32_t endian = 0x01020304u;
    h = fnv64(h, "RSVC", 4);
    h = fnv64_u32(h, STATE_FORMAT);
    h = fnv64_u32(h, (uint32_t)sizeof(void *));
    h = fnv64(h, &endian, 4);
    h = fnv64_str(h, game->id ? game->id : "");
    h = fnv64_str(h, game->version ? game->version : "");
    h = fnv64_u32(h, game->state_version);
    for (int i = 0; i < g_nvars; i++) {
        h = fnv64_str(h, g_vars[i].name);
        h = fnv64_u32(h, (uint32_t)g_vars[i].size);
        g_game_bytes += g_vars[i].size;
    }
    for (int i = 0; i < g_nptrs; i++) h = fnv64_str(h, g_ptrs[i].name);
    for (int i = 0; i < g_nrefs; i++) {
        h = fnv64_str(h, g_refs[i].name);
        h = fnv64_u32(h, (uint32_t)g_refs[i].size);
    }
    for (int i = 0; i < g_nrasters; i++) h = fnv64_str(h, g_rasters[i].name);
    g_layout_hash = h;
    g_size = HDR_SIZE + 8 * 8 + STATE_CORE_MAX + STATE_PPU_MAX + STATE_APU_MAX + STATE_MUSIC_MAX + STATE_TEXT_MAX +
             g_game_bytes + (size_t)g_nptrs * 8;
    return 1;
}

/* the layout hash plus the asset pack's contents (art or levels changed: the saved VRAM would not match) */
static uint64_t build_hash(void)
{
    if (g_build_ok) return g_build_hash;
    uint64_t h = g_layout_hash;
    if (g_frozen->assets)
        for (const rs_asset_entry *e = g_frozen->assets; e->name; e++) {
            h = fnv64_str(h, e->name);
            h = fnv64_u32(h, (uint32_t)e->size);
            h = fnv64(h, e->data, e->size);
        }
    g_build_hash = h;
    g_build_ok = 1;
    return h;
}

/* ---- pointers held by the runtime ---------------------------------------------------------------------------- */
int state_ptr_put(rs_wr *w, const void *p, size_t need, const char *what)
{
    if (!p) { wr_u32(w, 0); wr_u32(w, 0); return 0; }
    size_t off;
    int v = var_of(p, need, &off);
    if (v >= 0) { wr_u32(w, (uint32_t)v + 1); wr_u32(w, (uint32_t)off); return 0; }
    uintptr_t a = (uintptr_t)p;
    for (int i = 0; i < g_nrefs; i++) {
        uintptr_t b = (uintptr_t)g_refs[i].data;
        if (a >= b && a - b <= g_refs[i].size && need <= g_refs[i].size - (a - b)) {
            wr_u32(w, REF_BASE + (uint32_t)i);
            wr_u32(w, (uint32_t)(a - b));
            return 0;
        }
    }
    rs_log("state: %s points outside the registered objects (rs_state_var / rs_state_ref)", what);
    w->err = 1;
    return -1;
}

int state_ptr_get(rs_rd *r, const void **p, size_t need)
{
    uint32_t reg = rd_u32(r), off = rd_u32(r);
    *p = NULL;
    if (r->err) return -1;
    if (!reg) return off ? -1 : 0;
    if (reg >= 1 && reg <= (uint32_t)g_nvars) {
        const var_t *v = &g_vars[reg - 1];
        if (off > v->size || need > v->size - off) return -1;
        *p = v->data + off;
        return 0;
    }
    if (reg >= REF_BASE && reg - REF_BASE < (uint32_t)g_nrefs) {
        const ref_t *f = &g_refs[reg - REF_BASE];
        if (off > f->size || need > f->size - off) return -1;
        *p = f->data + off;
        return 0;
    }
    return -1;
}

int state_raster_put(rs_wr *w, rs_raster_fn fn)
{
    if (!fn) { wr_u32(w, 0); return 0; }
    for (int i = 0; i < g_nrasters; i++)
        if (g_rasters[i].fn == fn) { wr_u32(w, (uint32_t)i + 1); return 0; }
    rs_log("state: the raster callback is not registered (rs_state_raster)");
    w->err = 1;
    return -1;
}

int state_raster_get(rs_rd *r, rs_raster_fn *fn)
{
    uint32_t i = rd_u32(r);
    *fn = NULL;
    if (r->err || i > (uint32_t)g_nrasters) return -1;
    if (i) *fn = g_rasters[i - 1].fn;
    return 0;
}

/* ---- the game's objects ------------------------------------------------------------------------------------ */
static void game_save(rs_wr *w)
{
    size_t start = w->n;
    for (int i = 0; i < g_nvars; i++) wr_bytes(w, g_vars[i].data, g_vars[i].size);
    if (w->err) return;
    /* no raw addresses in the file: the pointers inside the objects are zeroed (PTRS holds them) */
    for (int i = 0; i < g_nptrs; i++) {
        if (g_ptrs[i].var < 0) continue;
        size_t at = start;
        for (int k = 0; k < g_ptrs[i].var; k++) at += g_vars[k].size;
        memset(w->p + at + g_ptrs[i].off, 0, sizeof(void *));
    }
}
static int game_load(rs_rd *r, int apply)
{
    if (r->n != g_game_bytes) return -1;
    for (int i = 0; i < g_nvars; i++) rd_bytes(r, apply ? g_vars[i].data : NULL, g_vars[i].size);
    return 0;
}
static void ptrs_save(rs_wr *w)
{
    for (int i = 0; i < g_nptrs; i++) {
        void *v;
        memcpy(&v, g_ptrs[i].pv, sizeof v);
        state_ptr_put(w, v, 0, g_ptrs[i].name);
    }
}
static int ptrs_load(rs_rd *r, int apply)
{
    for (int i = 0; i < g_nptrs; i++) {
        const void *v;
        if (state_ptr_get(r, &v, 0)) {
            rs_log("state: pointer %s is invalid", g_ptrs[i].name);
            return -1;
        }
        if (apply) memcpy(g_ptrs[i].pv, &v, sizeof v);
    }
    return 0;
}

/* ---- sections -------------------------------------------------------------------------------------------- */
enum { S_CORE, S_PPU, S_APU, S_MUS, S_TEXT, S_GAME, S_PTRS, S_END, S_COUNT };
static const char TAGS[S_COUNT][5] = {"CORE", "PPU ", "APU ", "MUS ", "TEXT", "GAME", "PTRS", "END "};

static void section_save(rs_wr *w, int s)
{
    wr_bytes(w, TAGS[s], 4);
    size_t at = w->n;
    wr_u32(w, 0);
    size_t start = w->n;
    switch (s) {
    case S_CORE: core_state_save(w); break;
    case S_PPU: ppu_state_save(w); break;
    case S_APU: apu_state_save(w); break;
    case S_MUS: music_state_save(w); break;
    case S_TEXT: text_state_save(w); break;
    case S_GAME: game_save(w); break;
    case S_PTRS: ptrs_save(w); break;
    default: break;
    }
    static const size_t MAX[S_COUNT] = {STATE_CORE_MAX, STATE_PPU_MAX, STATE_APU_MAX, STATE_MUSIC_MAX, STATE_TEXT_MAX,
                                        (size_t)-1, (size_t)-1, 0};
    if (!w->err && w->n - start > MAX[s]) {          /* the fixed size would be wrong: a bug in the SDK */
        rs_log("state: section %s is %u bytes, more than its maximum %u", TAGS[s], (unsigned)(w->n - start),
               (unsigned)MAX[s]);
        w->err = 1;
    }
    if (!w->err) put32(w->p + at, (uint32_t)(w->n - start));
}

static int section_load(int s, rs_rd *r, int apply)
{
    switch (s) {
    case S_CORE: return core_state_load(r, apply);
    case S_PPU: return ppu_state_load(r, apply);
    case S_APU: return apu_state_load(r, apply);
    case S_MUS: return music_state_load(r, apply);
    case S_TEXT: return text_state_load(r, apply);
    case S_GAME: return game_load(r, apply);
    case S_PTRS: return ptrs_load(r, apply);
    default: return r->n ? -1 : 0;
    }
}

/* ---- host API ------------------------------------------------------------------------------------------------ */
size_t rs_host_state_size(const rs_game *game) { return freeze(game) ? g_size : 0; }

void rs_host_state_list(void (*fn)(const char *kind, const char *name, size_t size))
{
    for (int i = 0; i < g_nvars; i++) fn("var", g_vars[i].name, g_vars[i].size);
    for (int i = 0; i < g_nptrs; i++) fn("ptr", g_ptrs[i].name, sizeof(void *));
    for (int i = 0; i < g_nrefs; i++) fn("ref", g_refs[i].name, g_refs[i].size);
    for (int i = 0; i < g_nrasters; i++) fn("raster", g_rasters[i].name, 0);
}

static void hdr_text(uint8_t *d, size_t n, const char *s)
{
    memset(d, 0, n);
    if (s) memcpy(d, s, strlen(s) < n - 1 ? strlen(s) : n - 1);
}

size_t rs_host_state_save(void *buf, size_t size)
{
    const rs_game *g = core_game();
    if (!g || !rs_host_state_size(g) || !buf) return 0;
    if (size < g_size) {
        rs_log("state: buffer of %u bytes, %u needed", (unsigned)size, (unsigned)g_size);
        return 0;
    }
    uint8_t *b = buf;
    memset(b, 0, size);
    rs_wr w = {b + HDR_SIZE, 0, g_size - HDR_SIZE, 0};
    for (int s = 0; s < S_COUNT; s++) section_save(&w, s);
    if (w.err) {
        rs_log("state: cannot save");
        memset(b, 0, size);
        return 0;
    }
    memcpy(b, "RSVC", 4);
    b[4] = STATE_FORMAT & 255; b[5] = STATE_FORMAT >> 8;
    b[6] = HDR_SIZE; b[7] = 0;
    hdr_text(b + 8, 20, g->id);
    hdr_text(b + 28, 16, g->version);
    put32(b + 44, g->state_version);
    uint64_t h = build_hash();
    put32(b + 48, (uint32_t)h);
    put32(b + 52, (uint32_t)(h >> 32));
    put32(b + 56, (uint32_t)w.n);
    put32(b + 60, fnv32(b + HDR_SIZE, w.n));
    return HDR_SIZE + w.n;
}

static int refuse(const char *why)
{
    rs_log("state: not loaded: %s", why);
    return -1;
}

int rs_host_state_load(const void *buf, size_t size)
{
    const rs_game *g = core_game();
    if (!g || !rs_host_state_size(g)) return refuse("this game has no save states");
    const uint8_t *b = buf;
    if (!b || size < HDR_SIZE) return refuse("truncated (no header)");
    if (memcmp(b, "RSVC", 4)) return refuse("not a RetroStone VC state");
    if ((b[4] | b[5] << 8) != STATE_FORMAT || b[6] != HDR_SIZE || b[7]) return refuse("another state format version");
    uint8_t id[20], ver[16];
    hdr_text(id, sizeof id, g->id);
    hdr_text(ver, sizeof ver, g->version);
    if (memcmp(b + 8, id, sizeof id)) return refuse("a state of another game");
    if (memcmp(b + 28, ver, sizeof ver) || le32(b + 44) != g->state_version) return refuse("another version of the game");
    uint64_t h = build_hash();
    if (le32(b + 48) != (uint32_t)h || le32(b + 52) != (uint32_t)(h >> 32))
        return refuse("another build of the game (its saved objects or assets differ)");
    uint32_t n = le32(b + 56);
    if (n > size - HDR_SIZE || n > g_size - HDR_SIZE) return refuse("truncated");
    if (fnv32(b + HDR_SIZE, n) != le32(b + 60)) return refuse("corrupted (checksum)");
    /* pass 0 checks every section, pass 1 applies them */
    for (int apply = 0; apply < 2; apply++) {
        size_t pos = 0;
        const uint8_t *p = b + HDR_SIZE;
        for (int s = 0; s < S_COUNT; s++) {
            if (n - pos < 8 || memcmp(p + pos, TAGS[s], 4)) return refuse("missing or unexpected section");
            uint32_t len = le32(p + pos + 4);
            pos += 8;
            if (len > n - pos) return refuse("truncated section");
            rs_rd r = {p + pos, len, 0, 0};
            if (section_load(s, &r, apply) || r.err || r.pos != len) {
                char why[64];
                snprintf(why, sizeof why, "invalid section %s", TAGS[s]);
                return refuse(why);   /* pass 1 never gets here: pass 0 read the same bytes */
            }
            pos += len;
        }
        if (pos != n) return refuse("data after the END section");
    }
    if (g->state_loaded) g->state_loaded();
    return 0;
}
