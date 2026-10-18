/*
 * The board games' texts (text.h): the device's language, the rules of each game, a page of text laid out and drawn
 * glyph by glyph with stb_truetype (the fonts cut down to the texts by tools/gen_text.py). Latin, Greek and Cyrillic
 * lines break at spaces; Japanese, Korean and Chinese between any two characters, but never before a closing mark.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "text.h"

#include <SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"     /* the library's parts this app does not use */
#include "stb_truetype.h"
#pragma GCC diagnostic pop

static int cur;                                 /* tx_langs[cur] */
static stbtt_fontinfo font[2];                  /* regular, bold */
static stbtt_fontinfo board[2];                 /* the boards' characters, the same in every language */
static stbtt_fontinfo board2[2];                /* the ones board lacks (DejaVu Sans) */
static int has_board2;
static int turkish;                             /* the language's i has a dot in capitals (İ) */

/* ---- the language ------------------------------------------------------------------------------------------------ */
static int find(const char *code)
{
    for (int i = 0; i < tx_nlangs; i++)
        if (!strcmp(tx_langs[i].code, code)) return i;
    for (int i = 0; i < tx_nlangs; i++)          /* "fr_CA": "fr" */
        if (!strncmp(tx_langs[i].code, code, 2) && !tx_langs[i].code[2]) return i;
    return -1;
}

void text_init(const char *forced)
{
    int k = forced ? find(forced) : -1;
    if (!forced) {
        SDL_Locale *l = SDL_GetPreferredLocales();
        for (int i = 0; l && l[i].language && k < 0; i++) {
            const char *lg = l[i].language, *co = l[i].country ? l[i].country : "";
            char code[16];
            if (!strcmp(lg, "pt")) SDL_snprintf(code, sizeof code, !strcmp(co, "BR") ? "pt_BR" : "pt_PT");
            else if (!strcmp(lg, "zh")) SDL_snprintf(code, sizeof code, "zh_CN");
            else if (!strcmp(lg, "no") || !strcmp(lg, "nn")) SDL_snprintf(code, sizeof code, "nb");
            else SDL_snprintf(code, sizeof code, "%s", lg);
            k = find(code);
        }
        SDL_free(l);
    }
    cur = k < 0 ? 0 : k;
    const tx_lang *t = &tx_langs[cur];
    turkish = !strcmp(t->code, "tr");
    stbtt_InitFont(&font[0], t->font, stbtt_GetFontOffsetForIndex(t->font, 0));
    stbtt_InitFont(&font[1], t->bold, stbtt_GetFontOffsetForIndex(t->bold, 0));
    static int board_ready;                     /* the boards' fonts (every language's) set once: the cards'
                                                 * painting thread reads them (text_glyph) as the language changes */
    if (board_ready) return;
    board_ready = 1;
    for (int b = 0; b < 2; b++)
        stbtt_InitFont(&board[b], tx_glyph_font[b], stbtt_GetFontOffsetForIndex(tx_glyph_font[b], 0));
    has_board2 = tx_glyph_font2[0] && tx_glyph_font2[1];
    for (int b = 0; b < 2 && has_board2; b++)
        stbtt_InitFont(&board2[b], tx_glyph_font2[b], stbtt_GetFontOffsetForIndex(tx_glyph_font2[b], 0));
}

/* the boards' font that has c (bold or not): the first, else the second; NULL: neither */
static stbtt_fontinfo *board_for(int c, int bold)
{
    if (stbtt_FindGlyphIndex(&board[bold], c)) return &board[bold];
    if (has_board2 && stbtt_FindGlyphIndex(&board2[bold], c)) return &board2[bold];
    return NULL;
}

const char *text_lang(void) { return tx_langs[cur].code; }

const char *text_get(const char *game)
{
    const char *en = NULL;
    for (int i = 0; i < tx_ntexts; i++) {
        if (strcmp(tx_texts[i].game, game)) continue;
        if (!strcmp(tx_texts[i].lang, tx_langs[cur].code)) return tx_texts[i].text;
        if (!strcmp(tx_texts[i].lang, "en")) en = tx_texts[i].text;
    }
    return en;
}

int text_name(const char *game, char *buf, int size) { return text_word(game, -1, buf, size); }

int text_word(const char *key, int i, char *buf, int size)
{
    const char *t = text_get(key);
    if (size < 1) return 0;
    buf[0] = 0;
    for (int k = -1; t && k < i; k++) {        /* past the title, then i lines */
        t = strchr(t, '\n');
        if (t) t++;
    }
    if (!t || !*t) return 0;
    int n = (int)strcspn(t, "\n");
    if (n >= size) n = size - 1;
    memcpy(buf, t, (size_t)n);
    buf[n] = 0;
    return 1;
}

/* ---- glyphs ------------------------------------------------------------------------------------------------------ */
static int utf8(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    int c = *p++, n = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
    if (n) c &= 0x3f >> n;
    while (n-- && (*p & 0xc0) == 0x80) c = c << 6 | (*p++ & 0x3f);
    *s = (const char *)p;
    return c;
}

/* the scripts written without spaces (Chinese, Japanese): a line may break between two characters. Korean (Hangul)
 * puts spaces between its words: it breaks at spaces only, like Latin. */
static int cjk(int c)
{
    return (c >= 0x2e80 && c < 0xa000) || (c >= 0xf900 && c < 0xfb00) || (c >= 0xff00 && c < 0xffef);
}

/* the characters a title never squeezes (they are square): Chinese, Japanese, Korean */
static int wide(int c) { return cjk(c) || (c >= 0x1100 && c < 0x1200) || (c >= 0xac00 && c < 0xd7b0); }
/* a letter's capital (Latin, Greek, Cyrillic: the 22 languages' letters); Greek capitals lose their accents, as
 * Greek writes them in a title. gen_text.py keeps the capitals of every letter the texts use in the fonts. */
static int upper(int c)
{
    if (c >= 'a' && c <= 'z') return turkish && c == 'i' ? 0x130 : c - 32;
    if (c < 0xe0) return c;
    if (c <= 0xfe) return c == 0xf7 ? c : c - 32;                 /* à..þ (not ÷) */
    if (c == 0xff) return 0x178;
    if (c >= 0x100 && c < 0x138) return c == 0x131 ? 'I' : c & ~1;  /* Ā..ķ: pairs, the capital even (ı: I) */
    if (c >= 0x139 && c < 0x149) return c & 1 ? c : c - 1;         /* Ĺ..ň: the capital odd */
    if (c >= 0x14a && c < 0x178) return c & ~1;                     /* Ŋ..ŷ */
    if (c >= 0x179 && c < 0x17f) return c & 1 ? c : c - 1;         /* Ź..ž */
    if (c >= 0x3ac && c <= 0x3ce) {                                 /* Greek: no accents in capitals */
        static const int acc[][2] = {{0x3ac, 0x391}, {0x3ad, 0x395}, {0x3ae, 0x397}, {0x3af, 0x399}, {0x3cc, 0x39f},
                                     {0x3cd, 0x3a5}, {0x3ce, 0x3a9}, {0x3ca, 0x3aa}, {0x3cb, 0x3ab}, {0x3c2, 0x3a3}};
        for (size_t i = 0; i < sizeof acc / sizeof acc[0]; i++)
            if (c == acc[i][0]) return acc[i][1];
        return c >= 0x3b1 && c <= 0x3c9 ? c - 32 : c;
    }
    if (c >= 0x386 && c <= 0x38f) {                                 /* capitals with an accent: without it */
        static const int cap[10] = {0x391, 0, 0x395, 0x397, 0x399, 0, 0x39f, 0, 0x3a5, 0x3a9};
        return cap[c - 0x386] ? cap[c - 0x386] : c;
    }
    if (c == 0x390) return 0x3aa;
    if (c == 0x3b0) return 0x3ab;
    if (c >= 0x430 && c < 0x450) return c - 32;                     /* Cyrillic */
    if (c >= 0x450 && c < 0x460) return c - 80;
    if (c >= 0x460 && c < 0x4c0) return c & ~1;                     /* Ґ ґ and the others: pairs */
    return c;
}

static int in(int c, const int *k)
{
    for (int i = 0; k[i]; i++)
        if (c == k[i]) return 1;
    return 0;
}

/* the marks a line must not start with (they close or follow what comes before; the kinsoku rules): the commas and
 * stops, the closing brackets and quotes, the middle dots (・), the long vowel mark and the iteration marks, the
 * small kana, the wave dash */
static int no_start(int c)
{
    static const int k[] = {0x3001, 0x3002, 0xff0c, 0xff0e, 0xff01, 0xff1f, 0xff1a, 0xff1b, 0xff09, 0x300d, 0x300f,
                            0x3011, 0x3009, 0x300b, 0x3015, 0x3017, 0x3019, 0x301f, 0xff3d, 0xff5d, 0xff60, 0xff63,
                            0x201d, 0x2019, 0x30fc, 0xff70, 0x2026, 0x2025, 0x30fb, 0xff65, 0x00b7, 0x3005, 0x303b,
                            0x309d, 0x309e, 0x30fd, 0x30fe, 0x309b, 0x309c, 0x301c, 0xff5e, 0x30a0, 0x2010, 0x2013,
                            0x203c, 0x2047, 0x2048, 0x2049,
                            0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083, 0x3085, 0x3087, 0x308e, 0x3095,
                            0x3096, 0x30a1, 0x30a3, 0x30a5, 0x30a7, 0x30a9, 0x30c3, 0x30e3, 0x30e5, 0x30e7, 0x30ee,
                            0x30f5, 0x30f6, 0x31f0, 0xff67, 0xff68, 0xff69, 0xff6a, 0xff6b, 0xff6c, 0xff6d, 0xff6e,
                            0xff6f, ',', '.', '!', '?', ':', ';', ')', ']', '}', 0};
    return in(c, k);
}

/* the marks a line must not end with (they open what comes after) */
static int no_end(int c)
{
    static const int k[] = {0xff08, 0x300c, 0x300e, 0x3010, 0x3008, 0x300a, 0x3014, 0x3016, 0x3018, 0x301d, 0xff3b,
                            0xff5b, 0xff5f, 0xff62, 0x201c, 0x2018, '(', '[', '{', 0};
    return in(c, k);
}

typedef struct {
    stbtt_fontinfo *f;
    float scale, size, ascent;
    float sx;                                   /* the width, a part of the usual (a title: condensed) */
    int upper;                                  /* capitals (a title) */
} face;

static face face_of(int bold, float size)
{
    face fc;
    fc.f = &font[bold ? 1 : 0];
    fc.size = size;
    fc.sx = 1;
    fc.upper = 0;
    fc.scale = stbtt_ScaleForMappingEmToPixels(fc.f, size);
    int asc, desc, gap;
    stbtt_GetFontVMetrics(fc.f, &asc, &desc, &gap);
    fc.ascent = asc * fc.scale;
    return fc;
}

/* the font that has c: the face's own, else the boards' font (gen_text.py moves there what a language's font
 * lacks: kanji in an English text shown in DejaVu...) at the same size */
static stbtt_fontinfo *font_for(const face *fc, int c, float *scale)
{
    *scale = fc->scale;
    if (c == ' ' || stbtt_FindGlyphIndex(fc->f, c)) return fc->f;
    stbtt_fontinfo *b = board_for(c, fc->f == &font[1] || fc->f == &board[1] || fc->f == &board2[1]);
    if (!b) return fc->f;
    *scale = stbtt_ScaleForMappingEmToPixels(b, fc->size);
    return b;
}

static float advance(const face *fc, int c)
{
    int adv, lsb;
    float scale;
    stbtt_fontinfo *f = font_for(fc, c, &scale);
    stbtt_GetCodepointHMetrics(f, c, &adv, &lsb);
    return adv * scale * (wide(c) ? 1 : fc->sx);
}

/* paints one glyph at (x, baseline y) into rgba (w x h, rows from y0) */
static void glyph(const face *fc, int c, float x, float y, const uint8_t rgb[3], uint8_t *rgba, int w, int h, int y0)
{
    static _Thread_local unsigned char *g;      /* each thread its own (pictures painted on other threads) */
    static _Thread_local int cap;
    int x0, yy0, x1, yy1;
    float fx = x - floorf(x), scale;
    stbtt_fontinfo *f = font_for(fc, c, &scale);
    float sx = scale * (wide(c) ? 1 : fc->sx);
    stbtt_GetCodepointBitmapBoxSubpixel(f, c, sx, scale, fx, 0, &x0, &yy0, &x1, &yy1);
    int gw = x1 - x0, gh = yy1 - yy0;
    if (gw <= 0 || gh <= 0) return;
    int top = (int)floorf(y) + yy0 - y0, left = (int)floorf(x) + x0;
    if (top >= h || top + gh <= 0) return;
    if (gw * gh > cap) { free(g); cap = gw * gh; g = malloc((size_t)cap); }
    stbtt_MakeCodepointBitmapSubpixel(f, g, gw, gh, gw, sx, scale, fx, 0, c);
    for (int j = 0; j < gh; j++) {
        int py = top + j;
        if (py < 0 || py >= h) continue;
        for (int i = 0; i < gw; i++) {
            int px = left + i, a = g[j * gw + i];
            if (px < 0 || px >= w || !a) continue;
            uint8_t *p = rgba + 4 * (py * w + px);
            memcpy(p, rgb, 3);
            if (a > p[3]) p[3] = (uint8_t)a;
        }
    }
}

/* draws [s, e) on one line from x at baseline y (when rgba); returns its width */
static float run(const face *fc, const char *s, const char *e, float x, float y, const uint8_t rgb[3], uint8_t *rgba,
                 int w, int h, int y0)
{
    float x_start = x;
    while (s < e) {
        int c = utf8(&s);
        if (fc->upper) c = upper(c);
        if (rgba && c != ' ') glyph(fc, c, x, y, rgb, rgba, w, h, y0);
        x += advance(fc, c);
    }
    return x - x_start;
}

/* lays a block out in lines of at most `width` from x; y: the top of its first line, moved past its last */
static void block(const face *fc, const char *s, const char *end, float x, float width, float *y, float lead,
                  const uint8_t rgb[3], uint8_t *rgba, int w, int h, int y0)
{
    while (s < end) {
        while (s < end && *s == ' ') s++;
        if (s >= end) break;
        const char *p = s, *brk = NULL;
        float sum = 0;
        int prev = 0;
        while (p < end) {
            const char *q = p;
            int c = utf8(&q);
            float a = advance(fc, c);
            if (sum + a > width && p > s) {
                if (!brk) brk = p;              /* one long word: cut it */
                break;
            }
            sum += a;
            p = q;
            if (c == ' ') {                     /* not round a times sign: 3 × 3 stays on one line */
                const char *r = p;
                if (prev != 0xd7 && (p >= end || utf8(&r) != 0xd7)) brk = p;
            } else if (p < end) {               /* between two characters of the scripts without spaces */
                const char *r = p;
                int next = utf8(&r);
                if ((cjk(c) || cjk(next)) && !no_start(next) && !no_end(c) && next != ' ') brk = p;
            }
            prev = c;
        }
        const char *e = p >= end ? end : brk;
        const char *t = e;
        while (t > s && t[-1] == ' ') t--;
        if (rgba && *y + lead > y0 && *y < y0 + h) run(fc, s, t, x, *y + fc->ascent, rgb, rgba, w, h, y0);
        *y += lead;
        s = e;
    }
}

int text_page(const char *s, int w, float px, const uint8_t rgb[3], uint8_t *rgba, int y0, int h_max)
{
    face body = face_of(0, px), head = face_of(1, px * 1.12f), title = face_of(1, px * 1.45f);
    float y = 0, lead = px * 1.42f;
    int first = 1;
    while (*s) {
        const char *e = s + strcspn(s, "\n");
        if (e == s) { y += 0.25f * lead; s = *e ? e + 1 : e; continue; }   /* a blank line: a little room */
        if (first) {
            block(&title, s, e, 0, (float)w, &y, title.size * 1.3f, rgb, rgba, w, h_max, y0);
            y += 0.45f * lead;
            first = 0;
        } else if (!strncmp(s, "# ", 2)) {
            y += 0.55f * lead;
            block(&head, s + 2, e, 0, (float)w, &y, head.size * 1.35f, rgb, rgba, w, h_max, y0);
            y += 0.15f * lead;
        } else if (!strncmp(s, "- ", 2)) {
            static const char bullet[] = "\xe2\x80\xa2";
            float ind = 1.3f * px;
            if (rgba && y + lead > y0 && y < y0 + h_max)
                run(&body, bullet, bullet + 3, 0.35f * px, y + body.ascent, rgb, rgba, w, h_max, y0);
            block(&body, s + 2, e, ind, w - ind, &y, lead, rgb, rgba, w, h_max, y0);
            y += 0.2f * lead;
        } else {
            block(&body, s, e, 0, (float)w, &y, lead, rgb, rgba, w, h_max, y0);
            y += 0.35f * lead;
        }
        s = *e ? e + 1 : e;
    }
    return (int)ceilf(y);
}

int text_glyph(int c, float px, int bold, const uint8_t rgb[3], uint8_t *rgba, int w, int h)
{
    face fc = {board_for(c, bold ? 1 : 0), 0, px, 0, 1, 0};   /* the boards' own fonts (not the language's: other
                                                             * threads paint with this) */
    if (!fc.f) return 0;
    fc.scale = stbtt_ScaleForMappingEmToPixels(fc.f, px);
    int x0, y0, x1, y1;                         /* centred on its ink, not on its advance box */
    stbtt_GetCodepointBitmapBox(fc.f, c, fc.scale, fc.scale, &x0, &y0, &x1, &y1);
    float x = (w - (x1 - x0)) / 2.0f - x0, base = (h - (y1 - y0)) / 2.0f - y0;
    glyph(&fc, c, x, base, rgb, rgba, w, h, 0);
    return 1;
}

int text_line(const char *s, float px, int bold, const uint8_t rgb[3], uint8_t *rgba, int w, int h, int x, int y)
{
    face fc = face_of(bold, px);
    const char *e = s + strcspn(s, "\n");
    return (int)ceilf(run(&fc, s, e, (float)x, y + fc.ascent, rgb, rgba, w, h, 0));
}

/* ---- titles, and words to search for ----------------------------------------------------------------------------- */
#define TITLE_SX 0.84f                          /* a title's width: condensed */

int text_title(const char *s, float px, const uint8_t rgb[3], uint8_t *rgba, int w, int h, int x, int y)
{
    face fc = face_of(1, px);
    fc.sx = TITLE_SX;
    fc.upper = 1;
    const char *e = s + strcspn(s, "\n");
    return (int)ceilf(run(&fc, s, e, (float)x, y + fc.ascent, rgb, rgba, w, h, 0));
}

/* a letter as a search compares it: small, without its accents (é e, ß s, ё е, ά α) */
static int fold(int c)
{
    /* Latin-1's letters from 0xc0, then Latin Extended-A's from 0x100: their bare letter */
    static const char l1[] = "aaaaaaaceeeeiiiidnooooo/ouuuuytsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
    static const char ea[] = "aaaaaa" "cccccccc" "dddd" "eeeeeeeeee" "gggggggg" "hhhh" "iiiiiiiiii" "ii" "jj" "kkk"
                             "llllllllll" "nnnnnnnnn" "oooooooo" "rrrrrr" "ssssssss" "tttttt" "uuuuuuuuuuuu" "ww" "yyy"
                             "zzzzzz" "s";
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xc0 && c < 0x100) return c == 0xd7 || c == 0xf7 ? c : l1[c - 0xc0];
    if (c >= 0x100 && c < 0x180) return ea[c - 0x100];
    if (c >= 0x386 && c < 0x3d0) {               /* Greek: small, no accents, final sigma as any */
        c = upper(c);
        if (c == 0x3aa) c = 0x399;
        if (c == 0x3ab) c = 0x3a5;
        return c >= 0x391 && c <= 0x3a9 ? c + 32 : c;
    }
    if (c >= 0x400 && c < 0x410) return c == 0x401 ? 0x435 : c + 80;   /* Ё ё: е */
    if (c >= 0x410 && c < 0x430) return c + 32;
    if (c == 0x451) return 0x435;
    if (c == 0x490) return 0x491;
    return c;
}

int text_fold(const char *s, char *out, int size)
{
    int n = 0;
    while (*s && n < size - 5) {
        int c = fold(utf8(&s));
        if (c < 0x80) out[n++] = (char)c;
        else if (c < 0x800) { out[n++] = (char)(0xc0 | c >> 6); out[n++] = (char)(0x80 | (c & 0x3f)); }
        else if (c < 0x10000) {
            out[n++] = (char)(0xe0 | c >> 12);
            out[n++] = (char)(0x80 | (c >> 6 & 0x3f));
            out[n++] = (char)(0x80 | (c & 0x3f));
        } else {
            out[n++] = (char)(0xf0 | c >> 18);
            out[n++] = (char)(0x80 | (c >> 12 & 0x3f));
            out[n++] = (char)(0x80 | (c >> 6 & 0x3f));
            out[n++] = (char)(0x80 | (c & 0x3f));
        }
    }
    out[n] = 0;
    return n;
}

int text_utf8_back(const char *s, int n)
{
    while (n > 0 && ((unsigned char)s[n - 1] & 0xc0) == 0x80) n--;
    return n > 0 ? n - 1 : 0;
}
