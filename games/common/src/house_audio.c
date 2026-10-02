/*
 * The 8BCraft house audio kit (house_audio.h): Leady Squid's synthesiser, shared.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "house_audio.h"
#include <stdlib.h>
#include <string.h>

static int ha_first = -1, ha_sound = 1, ha_music_started;

int ha_ms(int ms) { return ms * HA_RATE / 1000; }

void ha_begin(ha_synth *s, int16_t *buf, int len, uint32_t noise_seed)
{
    s->buf = buf;
    s->len = len;
    s->noise = noise_seed;
    memset(buf, 0, (size_t)len * sizeof *buf);
}

static int noise(ha_synth *s)
{
    s->noise ^= s->noise << 13;
    s->noise ^= s->noise >> 17;
    s->noise ^= s->noise << 5;
    return (int)(s->noise & 0xffff) - 32768;
}

/* integer sine, phase 0..65535 -> -16384..16384 (a parabola per half wave) */
static int isin(uint32_t ph)
{
    int p = (int)(ph & 0xffff);
    int x = p < 32768 ? p : p - 32768;
    int v = (int)((int64_t)x * (32768 - x) / 16384);
    return p < 32768 ? v : -v;
}

static void put(ha_synth *s, int i, int v)
{
    v += s->buf[i];
    s->buf[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

void ha_tone(ha_synth *s, int at, int f0, int f1, int ms, int vol, int decay_ms)
{
    int n = ms * HA_RATE / 1000;
    uint32_t ph = 0;
    for (int k = 0; k < n && at + k < s->len; k++) {
        int f = f0 + (f1 - f0) * k / (n ? n : 1);
        ph += (uint32_t)(f * 65536 / HA_RATE);
        int att = k < 96 ? k * 256 / 96 : 256;
        int dec = decay_ms ? 256 * decay_ms * HA_RATE / 1000 / (decay_ms * HA_RATE / 1000 + k) : 256;
        int rel = n - k < 400 ? (n - k) * 256 / 400 : 256;
        put(s, at + k, isin(ph) * vol / 100 * att / 256 * dec / 256 * rel / 256 * 2);
    }
}

void ha_hiss(ha_synth *s, int at, int ms, int vol, int lp_shift)
{
    int n = ms * HA_RATE / 1000, lp = 0;
    for (int k = 0; k < n && at + k < s->len; k++) {
        lp += (noise(s) - lp) >> lp_shift;
        int env = (n - k) * 256 / n;
        put(s, at + k, lp * vol / 100 * env / 256 * env / 256);
    }
}

void ha_store(ha_synth *s, int slot, int ms)
{
    int n = ms * HA_RATE / 1000;
    if (n > s->len) n = s->len;
    rs_sample_pcm16(slot, s->buf, n, HA_RATE, -1);
    memset(s->buf, 0, (size_t)s->len * sizeof *s->buf);
}

int ha_make(ha_synth *s, int id)
{
    const int R = HA_RATE;
    switch (id) {
    case HA_CONFIRM:
        ha_tone(s, 0, 523, 523, 110, 40, 60);
        ha_tone(s, R * 100 / 1000, 784, 784, 200, 40, 80);
        return 300;
    case HA_PAUSE:
        ha_tone(s, 0, 440, 330, 80, 40, 40);
        return 90;
    case HA_MEDAL:
        ha_tone(s, 0, 2093, 2093, 70, 25, 30);
        ha_tone(s, R * 60 / 1000, 2637, 2637, 70, 25, 30);
        ha_tone(s, R * 120 / 1000, 3136, 3136, 160, 25, 50);
        return 300;
    case HA_GAMEOVER:
        ha_hiss(s, 0, 160, 22, 4);
        ha_tone(s, R * 60 / 1000, 392, 392, 150, 40, 80);
        ha_tone(s, R * 200 / 1000, 311, 311, 150, 40, 80);
        ha_tone(s, R * 340 / 1000, 262, 250, 360, 44, 140);
        ha_tone(s, R * 340 / 1000, 131, 125, 360, 20, 140);
        return 720;
    case HA_SWISH:
        ha_hiss(s, 0, 180, 30, 4);
        return 180;
    case HA_DING:
        ha_tone(s, 0, 1568, 1568, 420, 42, 90);
        ha_tone(s, 0, 2349, 2349, 300, 18, 50);
        ha_tone(s, 0, 3951, 3951, 120, 6, 20);
        return 430;
    case HA_SELECT:
        ha_tone(s, 0, 1760, 1600, 30, 26, 12);
        return 40;
    case HA_THUD:
        ha_tone(s, 0, 120, 45, 240, 95, 120);
        ha_hiss(s, 0, 90, 40, 2);
        return 250;
    }
    return 0;
}

void ha_init(int first_slot)
{
    int len = HA_RATE;              /* 1 s: the longest house sound is 0.72 s */
    int16_t *buf = malloc((size_t)len * sizeof *buf);
    if (!buf) return;
    ha_synth s;
    ha_begin(&s, buf, len, 0x5eed1234u);
    for (int id = 0; id < HA_COUNT; id++) ha_store(&s, first_slot + id, ha_make(&s, id));
    free(buf);
    ha_first = first_slot;
    ha_music_started = 0;
    rs_echo(HA_ECHO_DELAY, HA_ECHO_FB, HA_ECHO_VOL);
}

int ha_play_pitched(int slot, int vol, int x, int pitch, int echo)
{
    if (!ha_sound) return -1;
    int pan = 24 + x * 80 / RS_SCREEN_W;
    pan = pan < 0 ? 0 : pan > 127 ? 127 : pan;
    /* House music has four channels. Reuse a sounding effect at the limit instead of adding a ninth voice. */
    int active = 0, replace = -1, budget = rs_music_playing() ? RS_VOICES - 4 : RS_VOICES;
    for (int i = 0; i < RS_VOICES; i++)
        if (rs_voice_active(i)) { active++; replace = i; }
    int v;
    if (active >= budget && replace >= 0) {
        v = rs_voice_play(replace, slot, pitch, vol, pan, NULL) == 0 ? replace : -1;
    } else v = rs_sfx(slot, vol, pan, pitch);
    if (v >= 0 && echo) rs_voice_echo(v, 1);
    return v;
}

int ha_play_slot(int slot, int vol, int x, int echo)
{
    return ha_play_pitched(slot, vol, x, RS_PITCH_1, echo);
}

int ha_play(int id)
{
    static const uint8_t vol[HA_COUNT] = {80, 70, 70, 80, 50, 70, 50, 110};
    if (ha_first < 0 || id < 0 || id >= HA_COUNT) return -1;
    return ha_play_slot(ha_first + id, vol[id], RS_SCREEN_W / 2, id == HA_DING);
}

void ha_sound_on(int on) { ha_sound = on; }

void ha_music(const char *asset, int music_on)
{
    if (!music_on) { rs_music_stop(); ha_music_started = 0; return; }
    if (ha_music_started && rs_music_playing()) return;
    size_t n;
    const void *m = rs_asset(asset, &n);
    if (m && !rs_music_play(m, n, 1)) {
        rs_music_volume(HA_MUSIC_VOL);
        ha_music_started = 1;
    }
}

#define S(v) rs_state_var("house_audio." #v, &(v), sizeof(v))
void ha_state(void) { S(ha_first); S(ha_sound); S(ha_music_started); }
#undef S
