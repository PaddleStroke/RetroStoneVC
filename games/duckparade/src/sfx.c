/*
 * Duck Parade: the sounds (synthesised at start-up with the house synthesiser, games/common/house_audio.h),
 * the voices, the march (the house music kit's MOD) and the layers the game adds on top of it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 *
 * Voices (the 8-voice guideline counts the module's 4 channels): the layers take voices 3 (the trumpet) and 2
 * (the glockenspiel) while they play; the sound effects share the others of 0..3, the oldest is stolen.
 * The module runs at one tick per frame and 8 frames a row, so the layers follow it frame for frame.
 */
#include "dp.h"
#include "assets.h"
#include "house_audio.h"
#include <string.h>

#define HOUSE_FIRST SMP_COUNT        /* the house set's sample slots (ha_init) */
#define SFX_VOICES  4
#define LAYER_VOICE(k) (3 - (k))

static int16_t snd_buf[HA_RATE];      /* start-up scratch (state_audit.txt) */
static int16_t half_buf[HA_RATE / 2];
static int music_on = 1, sound_on = 1;
static uint32_t music_frame;          /* frames since the march started (the layers' clock) */
static int music_was_playing, layers_on;
static uint32_t voice_age[SFX_VOICES], age_clock;

/* a sound with harmonics: the fundamental and n-1 overtones at falling volumes */
static void brass(ha_synth *s, int at, int f0, int f1, int ms, int vol, int decay, int n)
{
    for (int k = 1; k <= n; k++) ha_tone(s, at, f0 * k, f1 * k, ms, vol * 2 / (k + 1), decay);
}

/* a sound with little treble kept at 16 kHz: half the sample memory (the guideline: 64 KiB, like the SNES ARAM) */
static void store_half(ha_synth *s, int slot, int ms)
{
    int n = ha_ms(ms) / 2;
    for (int i = 0; i < n; i++) half_buf[i] = (int16_t)((s->buf[2 * i] + s->buf[2 * i + 1]) / 2);
    rs_sample_pcm16(slot, half_buf, n, HA_RATE / 2, -1);
    memset(s->buf, 0, sizeof(int16_t) * (size_t)s->len);
}

void sfx_init(void)
{
    ha_synth s;
    ha_begin(&s, snd_buf, HA_RATE, 0x5eed1234u);
    /* quacks: a nasal falling buzz (a fundamental and its overtones) and a breath of noise, four voices */
    static const int qf[4][3] = {{560, 380, 120}, {620, 420, 105}, {500, 340, 135}, {600, 360, 95}};
    for (int q = 0; q < 4; q++) {
        brass(&s, 0, qf[q][0], qf[q][1], qf[q][2], 30, 60, 5);
        ha_hiss(&s, 0, 40, 10, 2);
        store_half(&s, SFX_QUACK0 + q, qf[q][2] + 20);
    }
    /* peep: a duckling's chirp, twice */
    ha_tone(&s, 0, 1900, 2500, 60, 44, 30);
    ha_tone(&s, ha_ms(80), 2000, 2700, 70, 40, 30);
    ha_store(&s, SFX_PEEP, 160);
    /* the horn: two detuned squarish tones (a honk) */
    brass(&s, 0, 392, 392, 220, 22, 0, 4);
    brass(&s, 0, 415, 415, 220, 18, 0, 3);
    ha_store(&s, SFX_HORN, 230);
    /* a splash: bright noise going dull, a low bloop */
    ha_hiss(&s, 0, 250, 55, 0);
    ha_hiss(&s, ha_ms(40), 210, 35, 3);
    ha_tone(&s, 0, 180, 90, 160, 40, 60);
    ha_store(&s, SFX_SPLASH, 260);
    /* the train bell: a struck bell, two inharmonic partials */
    ha_tone(&s, 0, 1318, 1318, 290, 40, 70);
    ha_tone(&s, 0, 1760 * 102 / 100, 1760 * 102 / 100, 260, 22, 50);
    ha_tone(&s, 0, 3520, 3520, 90, 8, 15);
    ha_store(&s, SFX_BELL, 300);
    /* the banking fanfare: a brass arpeggio D5 F#5 A5 D6 */
    static const int fan[4] = {587, 740, 880, 1175};
    for (int k = 0; k < 4; k++) brass(&s, ha_ms(95 * k), fan[k], fan[k], k == 3 ? 300 : 110, 24, k == 3 ? 120 : 60, 4);
    ha_store(&s, SFX_FANFARE, 600);
    /* the camera: a click and a whirr */
    ha_hiss(&s, 0, 18, 70, 0);
    ha_tone(&s, ha_ms(20), 2600, 1800, 60, 16, 20);
    ha_store(&s, SFX_CLICK, 90);
    /* a thud (the house one) */
    ha_store(&s, SFX_THUD, ha_make(&s, HA_THUD));
    /* the fox's growl: a low buzz with a tremolo and some breath */
    brass(&s, 0, 110, 92, 320, 26, 0, 4);
    brass(&s, 0, 114, 96, 320, 20, 0, 3);
    ha_hiss(&s, 0, 300, 16, 4);
    ha_store(&s, SFX_GROWL, 330);
    /* a bump: a soft low blip */
    ha_tone(&s, 0, 220, 150, 60, 40, 30);
    ha_store(&s, SFX_BUMP, 70);
    /* the UI sounds (swish, medal, confirm, pause) are the house kit's own (below) */
    /* a plop: a duckling into the water */
    ha_tone(&s, 0, 300, 720, 70, 40, 40);
    ha_store(&s, SFX_PLOP, 90);
    /* the layers' instruments: a trumpet (a warm brass, soft attack) and a glockenspiel (bell partials) */
    brass(&s, 0, LAYER_TRUMPET_HZ, LAYER_TRUMPET_HZ, 360, 26, 180, 6);
    store_half(&s, SMP_TRUMPET, 380);
    ha_tone(&s, 0, LAYER_GLOCK_HZ, LAYER_GLOCK_HZ, 430, 50, 110);
    ha_tone(&s, 0, LAYER_GLOCK_HZ * 276 / 100, LAYER_GLOCK_HZ * 276 / 100, 240, 16, 40);
    ha_tone(&s, 0, LAYER_GLOCK_HZ * 540 / 100, LAYER_GLOCK_HZ * 540 / 100, 90, 6, 15);
    store_half(&s, SMP_GLOCK, 440);
    /* the house UI sounds this game uses (the kit's own synthesis), and the house echo */
    static const int ui[4] = {HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_SWISH};
    for (int k = 0; k < 4; k++) ha_store(&s, HOUSE_FIRST + ui[k], ha_make(&s, ui[k]));
    rs_echo(HA_ECHO_DELAY, HA_ECHO_FB, HA_ECHO_VOL);
    memset(voice_age, 0, sizeof voice_age);
    age_clock = 0;
}

/* a free voice of the pool (the layers' voices excluded), or the oldest */
static int pick_voice(void)
{
    int n = SFX_VOICES - layers_on, best = 0;
    uint32_t oldest = 0xffffffffu;
    for (int v = 0; v < n; v++) {
        if (!rs_voice_active(v)) return v;
        if (voice_age[v] < oldest) { oldest = voice_age[v]; best = v; }
    }
    return best;
}

void sfx_at(int id, int x, int pitch)
{
    static const uint8_t vol[SFX_COUNT] = {
        HA_VOL_ACTION, HA_VOL_ACTION, HA_VOL_ACTION, HA_VOL_ACTION,      /* quacks: the main action */
        HA_VOL_FEEDBACK, 96, 100, 60, 96, HA_VOL_FEEDBACK, HA_VOL_CRASH, 90, 50, 50, HA_VOL_UI, HA_VOL_UI, HA_VOL_UI, 60};
    if (!sound_on || id < 0 || id >= SFX_COUNT) return;
    int pan = clampi(24 + x * 80 / RS_SCREEN_W, 0, 127);
    int v = pick_voice();
    static const int8_t house[SFX_COUNT] = {[SFX_SWISH] = HA_SWISH + 1, [SFX_SPARKLE] = HA_MEDAL + 1, [SFX_JOIN] = HA_CONFIRM + 1,
                                            [SFX_PAUSE] = HA_PAUSE + 1};
    int slot = house[id] ? HOUSE_FIRST + house[id] - 1 : id;
    rs_voice_play(v, slot, pitch ? pitch : RS_PITCH_1, vol[id], pan, NULL);
    rs_voice_echo(v, id == SFX_SPLASH || id == SFX_PLOP || id == SFX_BELL || id == SFX_FANFARE);
    voice_age[v] = ++age_clock;
}

void sfx(int id) { sfx_at(id, RS_SCREEN_W / 2, 0); }

/* ---- the march and its layers --------------------------------------------------------------------------------------- */
void music_start(void) { ha_music("music/march.mod", music_on); }
void music_stop(void) { ha_music("music/march.mod", 0); }

void music_update(int layers)
{
    ha_music("music/march.mod", music_on);
    int playing = music_on && rs_music_playing();
    if (playing && !music_was_playing) music_frame = 0;
    music_was_playing = playing;
    if (!playing) layers = 0;
    if (layers > LAYER_COUNT) layers = LAYER_COUNT;
    for (int k = layers; k < layers_on; k++) rs_voice_release(LAYER_VOICE(k));
    layers_on = layers;
    if (playing && music_frame % MUSIC_ROW_FRAMES == 0) {
        int row = (int)((music_frame / MUSIC_ROW_FRAMES) % LAYER_ROWS);
        for (int k = 0; k < layers; k++) {
            const dp_note *n = &dp_layers[k][row];
            if (!n->pitch) continue;
            static const rs_adsr env[2] = {{12, 120, 90, 90}, {2, 80, 40, 200}};
            rs_voice_play(LAYER_VOICE(k), SMP_TRUMPET + k, n->pitch, n->vol * HA_MUSIC_VOL / 64, k ? 84 : 48, &env[k]);
            rs_voice_echo(LAYER_VOICE(k), k == 1);
        }
    }
    if (playing) music_frame++;
}

void audio_set(int music, int sound)
{
    music_on = music;
    sound_on = sound;
    ha_sound_on(sound);
    if (!music_on) music_stop();
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("sfx." #v, &(v), sizeof(v))
void sfx_state(void)
{
    S(music_on); S(sound_on); S(music_frame); S(music_was_playing); S(layers_on); S(voice_age); S(age_clock);
}
#undef S
