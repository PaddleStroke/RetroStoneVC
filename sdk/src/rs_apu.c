/*
 * RetroStone VC SDK: the audio unit ("APU").
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 8 sample voices (guideline) with pitch, volume, pan and ADSR, one global
 * echo, and tracker music through libxmp-lite (MIT), mixed at 32 kHz stereo.
 * Integer mixing only.
 */
#include "rs_internal.h"
#include <stdlib.h>
#include <string.h>

#define LIBXMP_STATIC 1
#include "xmp.h"

#define ENV_MAX   (1 << 22)
#define ECHO_MAX  (RS_AUDIO_RATE * 240 / 1000)

enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };

typedef struct sample_t {
    int16_t *data;
    int frames, rate, loop;
    int mem;                    /* bytes counted against the guideline */
    uint32_t hash;              /* of the data: a save state checks that the samples it refers to are the same */
} sample_t;

typedef struct voice_t {
    const sample_t *s;
    uint32_t pos, step;         /* 20.12 fixed point frames */
    int vol, pan, lg, rg;
    int env, state;
    int att, dec, sus, rel;
    int echo;
    uint32_t age;
} voice_t;

static sample_t g_smp[RS_SAMPLE_MAX];
static voice_t  g_v[RS_VOICE_MAX];
static uint32_t g_age;
static int      g_sfx_vol = 127;
static int16_t  g_echo[ECHO_MAX][2];
static int      g_echo_len, g_echo_pos, g_echo_fb, g_echo_vol;
static int      g_lp_l, g_lp_r;
static xmp_context g_xmp;
static int      g_music_on, g_music_loop, g_music_vol = 100, g_music_chn;
static int16_t  g_mbuf[(RS_AUDIO_RATE / RS_FPS + 8) * 2];
static const void *g_music_data;    /* the module playing (a save state names its asset) */

static void sample_free(sample_t *s)
{
    free(s->data);
    memset(s, 0, sizeof *s);
}

static void music_unload(void)
{
    if (g_xmp && g_music_on) {
        xmp_end_player(g_xmp);
        xmp_release_module(g_xmp);
    }
    g_music_on = 0;
    g_music_chn = 0;
    g_music_data = NULL;
}

void apu_reset(void)
{
    music_unload();
    for (int i = 0; i < RS_SAMPLE_MAX; i++) sample_free(&g_smp[i]);
    memset(g_v, 0, sizeof g_v);
    memset(g_echo, 0, sizeof g_echo);
    g_echo_len = g_echo_pos = g_echo_fb = g_echo_vol = 0;
    g_lp_l = g_lp_r = 0;
    g_age = 0;
    g_sfx_vol = 127;
    g_music_vol = 100;
}

void apu_shutdown(void)
{
    apu_reset();
    if (g_xmp) xmp_free_context(g_xmp);
    g_xmp = NULL;
}

static void check_sample_mem(void)
{
    int total = 0;
    for (int i = 0; i < RS_SAMPLE_MAX; i++) total += g_smp[i].mem;
    if (total > 65536)
        rs_warn(RS_WARN_SAMPLE_MEM, "sample memory %d bytes as 4-bit ADPCM (guideline 64 KiB, like ARAM)", total);
}

int rs_sample_pcm16(int slot, const int16_t *d, int frames, int rate, int loop)
{
    if ((unsigned)slot >= RS_SAMPLE_MAX || frames <= 0 || frames >= (1 << 20)) return -1;
    sample_t *s = &g_smp[slot];
    for (int i = 0; i < RS_VOICE_MAX; i++)
        if (g_v[i].s == s) g_v[i].state = ENV_OFF, g_v[i].s = NULL;
    sample_free(s);
    s->data = malloc(((size_t)frames + 1) * sizeof(int16_t));
    if (!s->data) return -1;
    memcpy(s->data, d, (size_t)frames * sizeof(int16_t));
    /* guard sample for interpolation */
    s->data[frames] = loop >= 0 ? d[loop] : 0;
    s->frames = frames;
    s->rate = rate > 0 ? rate : RS_AUDIO_RATE;
    s->loop = loop < frames ? loop : -1;
    s->mem = (frames + 1) / 2;  /* counted as ADPCM (4 bits per sample) */
    s->hash = 2166136261u;
    for (int i = 0; i <= frames; i++) s->hash = (s->hash ^ (uint16_t)s->data[i]) * 16777619u;
    check_sample_mem();
    return 0;
}

static const int16_t ima_step[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60,
    66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371,
    408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
    2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845,
    8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
    29794, 32767};
static const int8_t ima_index[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

int rs_sample_adpcm(int slot, const uint8_t *d, int frames, int rate, int loop)
{
    if (frames <= 0) return -1;
    int16_t *pcm = malloc((size_t)frames * sizeof(int16_t));
    if (!pcm) return -1;
    int pred = 0, idx = 0;
    for (int i = 0; i < frames; i++) {
        int nib = (d[i >> 1] >> ((i & 1) * 4)) & 15;
        int step = ima_step[idx];
        int diff = step >> 3;
        if (nib & 1) diff += step >> 2;
        if (nib & 2) diff += step >> 1;
        if (nib & 4) diff += step;
        pred += (nib & 8) ? -diff : diff;
        if (pred > 32767) pred = 32767; else if (pred < -32768) pred = -32768;
        idx += ima_index[nib];
        if (idx < 0) idx = 0; else if (idx > 88) idx = 88;
        pcm[i] = (int16_t)pred;
    }
    int r = rs_sample_pcm16(slot, pcm, frames, rate, loop);
    free(pcm);
    return r;
}

static void voice_gains(voice_t *v)
{
    int l = (127 - v->pan) * 2, r = v->pan * 2;
    if (l > 127) l = 127;
    if (r > 127) r = 127;
    v->lg = v->vol * l / 127;
    v->rg = v->vol * r / 127;
}

static int ms_to_frames(int ms) { int f = ms * (RS_AUDIO_RATE / 1000); return f > 0 ? f : 1; }

int rs_voice_play(int n, int slot, int pitch, int vol, int pan, const rs_adsr *e)
{
    if ((unsigned)n >= RS_VOICE_MAX || (unsigned)slot >= RS_SAMPLE_MAX || !g_smp[slot].data) return -1;
    voice_t *v = &g_v[n];
    static const rs_adsr def = {0, 0, 127, 10};
    if (!e) e = &def;
    v->s = &g_smp[slot];
    v->pos = 0;
    v->step = (uint32_t)((uint64_t)(uint32_t)pitch * (uint32_t)v->s->rate / RS_AUDIO_RATE);
    v->vol = vol < 0 ? 0 : vol > 127 ? 127 : vol;
    v->pan = pan < 0 ? 0 : pan > 127 ? 127 : pan;
    voice_gains(v);
    v->sus = (int)((int64_t)ENV_MAX * (e->sustain > 127 ? 127 : e->sustain) / 127);
    v->att = e->attack_ms ? ENV_MAX / ms_to_frames(e->attack_ms) : ENV_MAX;
    v->dec = e->decay_ms ? (ENV_MAX - v->sus) / ms_to_frames(e->decay_ms) + 1 : ENV_MAX;
    v->rel = ENV_MAX / ms_to_frames(e->release_ms ? e->release_ms : 1) + 1;
    v->env = 0;
    v->state = ENV_ATTACK;
    v->age = ++g_age;
    if (n >= RS_VOICES)
        rs_warn(RS_WARN_VOICES, "voice %d used (guideline: %d voices)", n, RS_VOICES);
    return 0;
}

void rs_voice_set(int n, int pitch, int vol, int pan)
{
    if ((unsigned)n >= RS_VOICE_MAX || !g_v[n].s) return;
    voice_t *v = &g_v[n];
    if (pitch >= 0) v->step = (uint32_t)((uint64_t)(uint32_t)pitch * (uint32_t)v->s->rate / RS_AUDIO_RATE);
    if (vol >= 0) v->vol = vol > 127 ? 127 : vol;
    if (pan >= 0) v->pan = pan > 127 ? 127 : pan;
    voice_gains(v);
}
void rs_voice_release(int n)
{
    if ((unsigned)n < RS_VOICE_MAX && g_v[n].state != ENV_OFF) g_v[n].state = ENV_RELEASE;
}
void rs_voice_echo(int n, int on) { if ((unsigned)n < RS_VOICE_MAX) g_v[n].echo = !!on; }
int rs_voice_active(int n) { return (unsigned)n < RS_VOICE_MAX && g_v[n].state != ENV_OFF; }

int rs_sfx(int slot, int vol, int pan, int pitch)
{
    int best = -1;
    uint32_t oldest = 0xffffffffu;
    int active = 0;
    int nv = RS_VOICES;
    for (int i = 0; i < nv; i++) if (g_v[i].state != ENV_OFF) active++;
    if (active + g_music_chn >= RS_VOICES)
        rs_warn(RS_WARN_VOICES, "%d sfx voices + %d music channels (guideline %d voices)",
                active + 1, g_music_chn, RS_VOICES);
    for (int i = nv - 1; i >= 0; i--) {
        if (g_v[i].state == ENV_OFF) { best = i; break; }
        if (g_v[i].age < oldest) { oldest = g_v[i].age; best = i; }
    }
    if (best < 0) return -1;
    rs_voice_play(best, slot, pitch ? pitch : RS_PITCH_1, vol, pan, NULL);
    return best;
}

void rs_echo(int ms, int fb, int vol)
{
    int len = ms * (RS_AUDIO_RATE / 1000);
    if (len > ECHO_MAX) len = ECHO_MAX;
    if (len < 0) len = 0;
    if (len != g_echo_len) {
        memset(g_echo, 0, sizeof g_echo);
        g_echo_pos = 0;
    }
    g_echo_len = len;
    g_echo_fb = fb < 0 ? 0 : fb > 127 ? 127 : fb;
    g_echo_vol = vol < 0 ? 0 : vol > 127 ? 127 : vol;
}

void rs_sound_volume(int v) { g_sfx_vol = v < 0 ? 0 : v > 127 ? 127 : v; }

/* ---- music --------------------------------------------------------------- */
int rs_music_play(const void *mod, size_t size, int loop)
{
    music_unload();
    if (!mod || !size) return -1;
    if (!g_xmp) g_xmp = xmp_create_context();
    if (!g_xmp) return -1;
    if (xmp_load_module_from_memory(g_xmp, mod, (long)size) != 0) {
        rs_log("music: cannot load module (%u bytes)", (unsigned)size);
        return -1;
    }
    if (xmp_start_player(g_xmp, RS_AUDIO_RATE, 0) != 0) {
        xmp_release_module(g_xmp);
        return -1;
    }
    struct xmp_module_info mi;
    xmp_get_module_info(g_xmp, &mi);
    g_music_chn = mi.mod ? mi.mod->chn : 0;
    if (g_music_chn > RS_VOICES)
        rs_warn(RS_WARN_VOICES, "module has %d channels (guideline %d voices)", g_music_chn, RS_VOICES);
    g_music_on = 1;
    g_music_loop = loop;
    g_music_data = mod;
    return 0;
}
void rs_music_stop(void) { music_unload(); }
void rs_music_volume(int v) { g_music_vol = v < 0 ? 0 : v > 127 ? 127 : v; }
int rs_music_playing(void) { return g_music_on; }

/* ---- mixer --------------------------------------------------------------- */
static void env_step(voice_t *v)
{
    switch (v->state) {
    case ENV_ATTACK:
        v->env += v->att;
        if (v->env >= ENV_MAX) { v->env = ENV_MAX; v->state = ENV_DECAY; }
        break;
    case ENV_DECAY:
        v->env -= v->dec;
        if (v->env <= v->sus) { v->env = v->sus; v->state = ENV_SUSTAIN; }
        break;
    case ENV_RELEASE:
        v->env -= v->rel;
        if (v->env <= 0) { v->env = 0; v->state = ENV_OFF; }
        break;
    default:
        break;
    }
}

void apu_render(int16_t *out, int frames)
{
    int music = 0;
    if (g_music_on) {
        int r = xmp_play_buffer(g_xmp, g_mbuf, frames * 4, g_music_loop ? 0 : 1);
        if (r == 0) music = 1;
        else music_unload();
    }
    for (int f = 0; f < frames; f++) {
        int32_t L = 0, R = 0, EL = 0, ER = 0;
        for (int i = 0; i < RS_VOICE_MAX; i++) {
            voice_t *v = &g_v[i];
            if (v->state == ENV_OFF) continue;
            const sample_t *s = v->s;
            uint32_t ip = v->pos >> 12;
            if (ip >= (uint32_t)s->frames) {
                if (s->loop < 0) { v->state = ENV_OFF; continue; }
                v->pos -= (uint32_t)(s->frames - s->loop) << 12;
                ip = v->pos >> 12;
            }
            int a = s->data[ip], b = s->data[ip + 1];
            int smp = a + (((b - a) * (int)(v->pos & 4095)) >> 12);
            v->pos += v->step;
            env_step(v);
            smp = (smp * (v->env >> 8)) >> 14;
            int32_t l = (smp * v->lg) >> 7, r = (smp * v->rg) >> 7;
            L += l;
            R += r;
            if (v->echo) { EL += l; ER += r; }
        }
        L = (L * g_sfx_vol) >> 7;
        R = (R * g_sfx_vol) >> 7;
        if (g_echo_len) {
            int16_t *e = g_echo[g_echo_pos];
            L += (e[0] * g_echo_vol) >> 7;
            R += (e[1] * g_echo_vol) >> 7;
            int32_t il = ((EL * g_sfx_vol) >> 7) + ((e[0] * g_echo_fb) >> 7);
            int32_t ir = ((ER * g_sfx_vol) >> 7) + ((e[1] * g_echo_fb) >> 7);
            g_lp_l += (il - g_lp_l) >> 1;   /* one-pole low-pass, stands in for the SNES FIR */
            g_lp_r += (ir - g_lp_r) >> 1;
            e[0] = (int16_t)(g_lp_l > 32767 ? 32767 : g_lp_l < -32768 ? -32768 : g_lp_l);
            e[1] = (int16_t)(g_lp_r > 32767 ? 32767 : g_lp_r < -32768 ? -32768 : g_lp_r);
            if (++g_echo_pos >= g_echo_len) g_echo_pos = 0;
        }
        if (music) {
            L += (g_mbuf[f * 2] * g_music_vol) >> 7;
            R += (g_mbuf[f * 2 + 1] * g_music_vol) >> 7;
        }
        out[f * 2] = (int16_t)(L > 32767 ? 32767 : L < -32768 ? -32768 : L);
        out[f * 2 + 1] = (int16_t)(R > 32767 ? 32767 : R < -32768 ? -32768 : R);
    }
}

/* ---- save states: voices (their sample as a slot number), echo, and the music track and position --------- */
void apu_state_save(rs_wr *w)
{
    wr_u32(w, g_age);
    wr_i32(w, g_sfx_vol); wr_i32(w, g_music_vol);
    wr_i32(w, g_echo_len); wr_i32(w, g_echo_pos); wr_i32(w, g_echo_fb); wr_i32(w, g_echo_vol);
    wr_i32(w, g_lp_l); wr_i32(w, g_lp_r);
    for (int i = 0; i < RS_VOICE_MAX; i++) {
        const voice_t *v = &g_v[i];
        wr_u8(w, (uint8_t)(v->s ? v->s - g_smp + 1 : 0));
        wr_u32(w, v->pos); wr_u32(w, v->step);
        wr_i32(w, v->vol); wr_i32(w, v->pan); wr_i32(w, v->lg); wr_i32(w, v->rg);
        wr_i32(w, v->env); wr_i32(w, v->state);
        wr_i32(w, v->att); wr_i32(w, v->dec); wr_i32(w, v->sus); wr_i32(w, v->rel);
        wr_i32(w, v->echo); wr_u32(w, v->age);
    }
    for (int i = 0; i < ECHO_MAX; i++) { wr_u16(w, (uint16_t)g_echo[i][0]); wr_u16(w, (uint16_t)g_echo[i][1]); }
    for (int i = 0; i < RS_SAMPLE_MAX; i++) {         /* the samples are not saved: the game loads them in init() */
        wr_u32(w, g_smp[i].data ? (uint32_t)g_smp[i].frames : 0);
        wr_u32(w, g_smp[i].data ? g_smp[i].hash : 0);
    }
}

static int vol_ok(int v) { return v >= 0 && v <= 127; }

int apu_state_load(rs_rd *r, int apply)
{
    uint32_t age = rd_u32(r);
    int sv = rd_i32(r), mv = rd_i32(r);
    int elen = rd_i32(r), epos = rd_i32(r), efb = rd_i32(r), evol = rd_i32(r), lpl = rd_i32(r), lpr = rd_i32(r);
    if (r->err || !vol_ok(sv) || !vol_ok(mv) || elen < 0 || elen > ECHO_MAX || epos < 0 || (elen && epos >= elen) ||
        (!elen && epos) || !vol_ok(efb) || !vol_ok(evol))
        return -1;
    voice_t nv[RS_VOICE_MAX];
    for (int i = 0; i < RS_VOICE_MAX; i++) {
        voice_t *v = &nv[i];
        int slot = rd_u8(r);
        v->pos = rd_u32(r); v->step = rd_u32(r);
        v->vol = rd_i32(r); v->pan = rd_i32(r); v->lg = rd_i32(r); v->rg = rd_i32(r);
        v->env = rd_i32(r); v->state = rd_i32(r);
        v->att = rd_i32(r); v->dec = rd_i32(r); v->sus = rd_i32(r); v->rel = rd_i32(r);
        v->echo = rd_i32(r) ? 1 : 0; v->age = rd_u32(r);
        if (r->err || slot > RS_SAMPLE_MAX || v->state < ENV_OFF || v->state > ENV_RELEASE) return -1;
        if (!vol_ok(v->vol) || !vol_ok(v->pan) || !vol_ok(v->lg) || !vol_ok(v->rg) || v->env < 0 || v->env > ENV_MAX)
            return -1;
        v->s = slot ? &g_smp[slot - 1] : NULL;
        if (v->state != ENV_OFF) {                   /* the mixer reads the sample: keep it in bounds */
            const sample_t *s = v->s;
            if (!s || !s->data) return -1;
            /* a looping voice may be up to one loop past the end (the mixer wraps it once); a one-shot stops */
            if (s->loop >= 0 && ((uint64_t)v->pos >= (uint64_t)(2 * s->frames - s->loop) << 12 ||
                                 v->step > (uint32_t)(s->frames - s->loop) << 12))
                return -1;
        }
    }
    if (apply) {
        g_age = age;
        g_sfx_vol = sv; g_music_vol = mv;
        g_echo_len = elen; g_echo_pos = epos; g_echo_fb = efb; g_echo_vol = evol; g_lp_l = lpl; g_lp_r = lpr;
        memcpy(g_v, nv, sizeof g_v);
        for (int i = 0; i < ECHO_MAX; i++) { g_echo[i][0] = (int16_t)rd_u16(r); g_echo[i][1] = (int16_t)rd_u16(r); }
    } else {
        rd_bytes(r, NULL, (size_t)ECHO_MAX * 4);
    }
    for (int i = 0; i < RS_SAMPLE_MAX; i++) {
        uint32_t frames = rd_u32(r), hash = rd_u32(r);
        uint32_t cf = g_smp[i].data ? (uint32_t)g_smp[i].frames : 0, ch = g_smp[i].data ? g_smp[i].hash : 0;
        if (frames != cf || hash != ch) {
            rs_log("state: sample slot %d differs from the saved one", i);
            return -1;
        }
    }
    return r->err ? -1 : 0;
}

static struct xmp_frame_info g_fi;

void music_state_save(rs_wr *w)
{
    char name[64];
    memset(name, 0, sizeof name);
    int pos = 0, row = 0;
    if (g_music_on) {
        const char *n = core_asset_name(g_music_data);
        if (n && strlen(n) < sizeof name) memcpy(name, n, strlen(n));
        else rs_log("state: the music playing is not an asset: it will not be restored");
        xmp_get_frame_info(g_xmp, &g_fi);
        pos = g_fi.pos;
        row = g_fi.row;
    }
    wr_u8(w, (uint8_t)g_music_on); wr_u8(w, (uint8_t)g_music_loop); wr_u8(w, 0); wr_u8(w, 0);
    wr_bytes(w, name, sizeof name);
    wr_i32(w, pos); wr_i32(w, row);
}

int music_state_load(rs_rd *r, int apply)
{
    char name[64];
    int on = rd_u8(r), loop = rd_u8(r);
    rd_u8(r); rd_u8(r);
    rd_bytes(r, name, sizeof name);
    int pos = rd_i32(r), row = rd_i32(r);
    if (r->err || name[sizeof name - 1] || on > 1 || pos < 0 || row < 0) return -1;
    size_t size = 0;
    const void *mod = on && name[0] ? core_asset_find(name, &size) : NULL;
    if (on && name[0] && !mod) {
        rs_log("state: the music %s is not in this game", name);
        return -1;
    }
    if (!apply) return 0;
    /* the same module restarts at the saved order position and row (libxmp cannot restore its voices) */
    if (!mod || rs_music_play(mod, size, loop)) {
        music_unload();
        return 0;
    }
    xmp_set_position(g_xmp, pos);                   /* returns -1 for position 0 (a restart): not an error */
    xmp_set_row(g_xmp, row);
    return 0;
}
