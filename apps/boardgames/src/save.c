/*
 * RetroStone Board: what the app keeps between two runs (shell.h, ui_save_get / ui_save_set): a few small values, each
 * under a key ("shell/mute", "logicgates/progress"), in one file on the device. Nothing leaves the device.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The file: board.sav in SDL_GetPrefPath("8BCraft", "RetroStoneBoard") (Windows %APPDATA%\8BCraft\RetroStoneBoard,
 * Linux ~/.local/share/8BCraft/RetroStoneBoard, Android the app's own files), or in the folder given by --save DIR.
 *   20 bytes: "RSBS", the version (1), three zero bytes, the records' count, the records' length in bytes, their
 *             CRC-32 (the numbers four bytes each, little-endian);
 *   then each record: the key's length (1 byte, 1..63), the key (printable ASCII), the value's length (2 bytes,
 *             0..4096), the value.
 * Written whole to board.sav.tmp, then renamed over board.sav: a crash leaves the old file or the new one, never half
 * of one. A file that does not read back exactly so (a wrong magic, version, length or checksum, a record running
 * past the end) is ignored: the app starts with nothing kept, and the next change writes a good file over it.
 * The picture mode keeps its values in memory only (unless --save DIR): its pictures never depend on a past run.
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L                 /* fsync, fileno */
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_NAME "board.sav"
#define SAVE_VERSION 1
#define KEY_MAX 63
#define VALUE_MAX 4096
#define ENTRIES_MAX 1024
#define FILE_MAX (1 << 22)                      /* larger: not ours */

static struct entry { char key[KEY_MAX + 1]; int n; uint8_t *v; } *ent;
static int nent, cap;
static char *path;                              /* the file (NULL: memory only) */
static int dirty;

/* ---- the values ---------------------------------------------------------------------------------------------- */
static int key_ok(const char *key)
{
    size_t n = key ? strlen(key) : 0;
    if (n < 1 || n > KEY_MAX) return 0;
    for (size_t i = 0; i < n; i++)
        if (key[i] < 0x21 || key[i] > 0x7e) return 0;
    return 1;
}

static struct entry *find(const char *key)
{
    for (int i = 0; i < nent; i++)
        if (!strcmp(ent[i].key, key)) return &ent[i];
    return NULL;
}

static void clear(void)
{
    for (int i = 0; i < nent; i++) free(ent[i].v);
    nent = 0;
}

/* a value kept (n 0: removed); 0: no room */
static int put(const char *key, const void *data, int n)
{
    struct entry *e = find(key);
    if (n <= 0) {
        if (!e) return 1;
        free(e->v);
        *e = ent[--nent];
        return 1;
    }
    if (!e) {
        if (nent >= ENTRIES_MAX) return 0;
        if (nent == cap) {
            int c = cap ? 2 * cap : 32;
            struct entry *m = realloc(ent, (size_t)c * sizeof *m);
            if (!m) return 0;
            ent = m;
            cap = c;
        }
        e = &ent[nent++];
        snprintf(e->key, sizeof e->key, "%s", key);
        e->n = 0;
        e->v = NULL;
    }
    uint8_t *v = malloc((size_t)n);
    if (!v) {
        if (!e->v) *e = ent[--nent];
        return 0;
    }
    memcpy(v, data, (size_t)n);
    free(e->v);
    e->v = v;
    e->n = n;
    return 1;
}

int ui_save_get(const char *key, void *buf, int size)
{
    const struct entry *e = key_ok(key) ? find(key) : NULL;
    if (!e) return 0;
    if (!buf || size <= 0) return e->n;         /* how long it is */
    int n = e->n < size ? e->n : size;
    memcpy(buf, e->v, (size_t)n);
    return n;
}

void ui_save_set(const char *key, const void *data, int size)
{
    if (!key_ok(key) || size > VALUE_MAX) {
        fprintf(stderr, "save: %s not kept (a key of 1..%d printable characters, a value of at most %d bytes)\n",
                key ? key : "(null)", KEY_MAX, VALUE_MAX);
        return;
    }
    if (!data) size = 0;
    const struct entry *e = find(key);
    if (size <= 0 ? !e : e && e->n == size && !memcmp(e->v, data, (size_t)size)) return;   /* the same: no write */
    if (put(key, data, size)) dirty = 1;
}

/* ---- the file ------------------------------------------------------------------------------------------------ */
static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}

static uint32_t rd32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24);
}

static FILE *open_file(const char *name, const char *mode)
{
#ifdef _WIN32                                   /* the path is UTF-8 (SDL_GetPrefPath): wide characters for Windows */
    wchar_t wn[1024], wm[8];
    if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 1024) || !MultiByteToWideChar(CP_UTF8, 0, mode, -1, wm, 8))
        return NULL;
    return _wfopen(wn, wm);
#else
    return fopen(name, mode);
#endif
}

static int replace_file(const char *from, const char *to)
{
#ifdef _WIN32
    wchar_t wf[1024], wt[1024];
    if (!MultiByteToWideChar(CP_UTF8, 0, from, -1, wf, 1024) || !MultiByteToWideChar(CP_UTF8, 0, to, -1, wt, 1024))
        return 0;
    return MoveFileExW(wf, wt, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(from, to) == 0;
#endif
}


#define HEAD 20

/* the file's records into the store; 0 (the store left empty): not a good file, *why says how */
static int parse(const uint8_t *b, size_t n, const char **why)
{
    *why = "too short";
    if (n < HEAD) return 0;
    *why = "not a save file";
    if (memcmp(b, "RSBS", 4)) return 0;
    *why = "another version";
    if (b[4] != SAVE_VERSION || b[5] || b[6] || b[7]) return 0;
    uint32_t count = rd32(b + 8), len = rd32(b + 12);
    *why = "cut short, or too long";
    if (len != n - HEAD || count > ENTRIES_MAX) return 0;
    *why = "a wrong checksum";
    if (crc32(b + HEAD, len) != rd32(b + 16)) return 0;
    *why = "a broken record";
    const uint8_t *p = b + HEAD, *end = b + n;
    for (uint32_t i = 0; i < count; i++) {
        if (end - p < 1) break;
        int kl = *p++;
        if (kl < 1 || kl > KEY_MAX || end - p < kl + 2) break;
        char key[KEY_MAX + 1];
        memcpy(key, p, (size_t)kl);
        key[kl] = 0;
        p += kl;
        int vl = p[0] | p[1] << 8;
        p += 2;
        if (vl > VALUE_MAX || end - p < vl || !key_ok(key) || find(key)) break;
        if (!put(key, p, vl)) break;
        p += vl;
        if (i + 1 == count && p == end) return 1;
    }
    if (count == 0 && p == end) return 1;
    clear();
    return 0;
}

void save_open(const char *dir)
{
    clear();
    free(path);
    path = NULL;
    dirty = 0;
    char *pref = NULL;
    if (!dir) dir = pref = SDL_GetPrefPath("8BCraft", "RetroStoneBoard");
    if (!dir || !*dir) {                        /* no place to keep it: memory only */
        SDL_free(pref);
        return;
    }
    size_t n = strlen(dir);
    int sep = dir[n - 1] == '/' || dir[n - 1] == '\\';
    path = malloc(n + 2 + sizeof SAVE_NAME);
    if (path) snprintf(path, n + 2 + sizeof SAVE_NAME, "%s%s%s", dir, sep ? "" : "/", SAVE_NAME);
    SDL_free(pref);
    if (!path) return;
    FILE *f = open_file(path, "rb");
    if (!f) return;                             /* none yet */
    uint8_t *b = malloc(FILE_MAX + 1);
    size_t got = b ? fread(b, 1, FILE_MAX + 1, f) : 0;
    fclose(f);
    const char *why = "too long";
    if (b && got <= FILE_MAX && parse(b, got, &why)) {
        free(b);
        return;
    }
    free(b);
    fprintf(stderr, "save: %s ignored (%s): starting afresh\n", path, why);
}

void save_flush(void)
{
    if (!dirty || !path) return;
    dirty = 0;
    size_t len = 0;
    for (int i = 0; i < nent; i++) len += 1 + strlen(ent[i].key) + 2 + (size_t)ent[i].n;
    uint8_t *b = malloc(HEAD + len), *p = b ? b + HEAD : NULL;
    if (!b) return;
    for (int i = 0; i < nent; i++) {
        size_t kl = strlen(ent[i].key);
        *p++ = (uint8_t)kl;
        memcpy(p, ent[i].key, kl);
        p += kl;
        *p++ = (uint8_t)ent[i].n;
        *p++ = (uint8_t)(ent[i].n >> 8);
        memcpy(p, ent[i].v, (size_t)ent[i].n);
        p += ent[i].n;
    }
    memcpy(b, "RSBS", 4);
    b[4] = SAVE_VERSION;
    b[5] = b[6] = b[7] = 0;
    wr32(b + 8, (uint32_t)nent);
    wr32(b + 12, (uint32_t)len);
    wr32(b + 16, crc32(b + HEAD, len));
    size_t pn = strlen(path) + 5;
    char *tmp = malloc(pn);
    FILE *f = tmp ? (snprintf(tmp, pn, "%s.tmp", path), open_file(tmp, "wb")) : NULL;
    int ok = f && fwrite(b, 1, HEAD + len, f) == HEAD + len && fflush(f) == 0;
    if (f) {
#ifdef _WIN32
        if (ok) ok = _commit(_fileno(f)) == 0;  /* on the disk before the rename */
#else
        if (ok) ok = fsync(fileno(f)) == 0;
#endif
        ok = fclose(f) == 0 && ok;
    }
    if (ok) ok = replace_file(tmp, path);
    if (!ok) {
        fprintf(stderr, "save: could not write %s\n", path);
        if (tmp) remove(tmp);
    }
    free(tmp);
    free(b);
}

const char *save_path(void) { return path; }
