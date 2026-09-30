/*
 * Beaver Rush: the sound effects (synthesised at start-up with the house synthesiser, games/common
 * house_audio.c), the house UI sounds, and the music: a banjo loop whose next 8 bars are picked at the tempo
 * of the timer's drain (tools/make_music.py renders each section at four tempos).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
 */
#include "br.h"
#include "house_audio.h"
#include <stdio.h>
#include <string.h>

#define SFX_HOUSE SFX_COUNT             /* the house set follows ours */
static int16_t snd_buf[HA_RATE];         /* start-up scratch (state_audit.txt) */
static int music_on = 1, sound_on = 1;
static int section;                      /* the next section to play: 0 = A, 1 = B */
static int tempo_shown;                  /* the tempo of the section playing */
static rs_rng pitch_rng;

void sfx_init(void)
{
    ha_synth s;
    ha_begin(&s, snd_buf, HA_RATE, 0x5eed1234u);
    /* chomp: a crunchy bite: a bright noise crunch in three quick bursts over a woody knock */
    ha_hiss(&s, 0, 34, 70, 0);
    ha_hiss(&s, ha_ms(14), 30, 52, 1);
    ha_hiss(&s, ha_ms(30), 40, 34, 2);
    ha_tone(&s, 0, 620, 300, 55, 62, 14);
    ha_tone(&s, ha_ms(8), 1250, 900, 30, 24, 8);
    ha_store(&s, SFX_CHOMP, 90);
    /* splash: a low-passed rush of water and two bubbles */
    ha_hiss(&s, 0, 300, 60, 3);
    ha_hiss(&s, 0, 90, 30, 1);
    ha_tone(&s, ha_ms(40), 300, 950, 80, 22, 30);
    ha_tone(&s, ha_ms(120), 450, 1300, 60, 14, 20);
    ha_store(&s, SFX_SPLASH, 320);
    /* golden chime: C6 E6 G6 C7, bell partials */
    static const int chime[4] = {1047, 1319, 1568, 2093};
    for (int i = 0; i < 4; i++) {
        ha_tone(&s, ha_ms(55 * i), chime[i], chime[i], 380 - 40 * i, 34, 90);
        ha_tone(&s, ha_ms(55 * i), chime[i] * 3, chime[i] * 3, 120, 8, 20);
    }
    ha_store(&s, SFX_GOLD, 600);
    /* branch bonk: a hollow wood knock and a boing */
    ha_tone(&s, 0, 190, 150, 110, 90, 45);
    ha_tone(&s, 0, 410, 330, 70, 44, 18);
    ha_hiss(&s, 0, 25, 50, 1);
    ha_tone(&s, ha_ms(50), 240, 560, 150, 40, 200);
    ha_tone(&s, ha_ms(200), 560, 250, 220, 34, 120);
    ha_store(&s, SFX_BONK, 460);
    /* milestone cheer: a bright rising arpeggio (G5 B5 D6 G6) over a crowd-like swell of noise */
    static const int cheer[4] = {784, 988, 1175, 1568};
    for (int i = 0; i < 4; i++) ha_tone(&s, ha_ms(70 * i), cheer[i], cheer[i], 240, 30, 110);
    for (int i = 0; i < 6; i++) ha_hiss(&s, ha_ms(40 * i), 160, 10 + i * 2, 3);
    ha_tone(&s, ha_ms(280), 1568, 1760, 300, 22, 150);
    ha_store(&s, SFX_CHEER, 640);
    /* out of breath: a long falling sigh */
    ha_tone(&s, 0, 460, 210, 460, 44, 260);
    ha_hiss(&s, 0, 380, 18, 4);
    ha_store(&s, SFX_SLEEP, 480);
    /* the woodpecker's tok: a tiny high knock */
    ha_tone(&s, 0, 1500, 1250, 26, 36, 6);
    ha_hiss(&s, 0, 8, 20, 0);
    ha_store(&s, SFX_TOK, 32);
    /* stolen chip: a whoosh */
    ha_hiss(&s, 0, 260, 44, 1);
    ha_tone(&s, 0, 300, 900, 240, 12, 0);
    ha_store(&s, SFX_WHOOSH, 270);
    ha_init(SFX_HOUSE);                  /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER, HA_SWISH, ... */
    ha_sound_on(sound_on);
    rs_rng_seed(&pitch_rng, 0xc40bu);
}

/* the house loudness: crash 110, action 88, feedback 70, soft 40-50 */
static const uint8_t vol[SFX_COUNT] = {HA_VOL_ACTION, 56, HA_VOL_FEEDBACK, HA_VOL_CRASH, HA_VOL_FEEDBACK,
                                       HA_VOL_ACTION, 34, 60, 50, 70, 80, 70};

void sfx_pan(int id, int x, int pitch)
{
    if (!sound_on || id < 0 || id >= SFX_COUNT) return;
    int house = -1;
    switch (id) {                        /* the UI sounds are the house's */
    case SFX_PANEL: house = HA_SWISH; break;
    case SFX_PAUSE: house = HA_PAUSE; break;
    case SFX_JOIN: house = HA_CONFIRM; break;
    case SFX_SPARKLE: house = HA_MEDAL; break;
    }
    if (house >= 0) { ha_play(house); return; }
    if (id == SFX_CHOMP && !pitch)       /* each bite a little different: +-6% */
        pitch = RS_PITCH_1 + rs_rng_range(&pitch_rng, 491) - 245;
    int pan = clampi(24 + x * 80 / RS_SCREEN_W, 0, 127);
    int v = rs_sfx(id, vol[id], pan, pitch ? pitch : RS_PITCH_1);
    if (v >= 0 && (id == SFX_SPLASH || id == SFX_GOLD)) rs_voice_echo(v, 1);   /* the "wet" sounds */
}

void sfx(int id) { sfx_pan(id, RS_SCREEN_W / 2, 0); }

/* the next 8 bars: A and B in turn, at the tempo of the drain (the gnaws per second that hold the bar) */
void music_update(int level, int playing)
{
    if (!music_on) { rs_music_stop(); return; }
    if (rs_music_playing()) return;
    int rate = rate_q16(level) * 10 / 65536;     /* tenths of a gnaw per second */
    int tempo = !playing ? 0 : rate < 50 ? 0 : rate < 58 ? 1 : rate < 64 ? 2 : 3;
    char name[24];
    snprintf(name, sizeof name, "music/%c%d.mod", section ? 'b' : 'a', tempo);
    size_t n;
    const void *m = rs_asset(name, &n);
    if (m && !rs_music_play(m, n, 0)) {
        rs_music_volume(HA_MUSIC_VOL);
        section ^= 1;
        tempo_shown = tempo;
    }
}

int music_tempo(void) { return tempo_shown; }

void audio_set(int music, int sound)
{
    music_on = music;
    sound_on = sound;
    ha_sound_on(sound);
    if (!music_on) rs_music_stop();
}

/* ---- save states (main.c): the options and the music's place (the SDK restarts the module where it was) ---- */
#define S(v) rs_state_var("sfx." #v, &(v), sizeof(v))
void sfx_state(void)
{
    S(music_on); S(sound_on); S(section); S(tempo_shown); S(pitch_rng);
    ha_state();
}
#undef S
