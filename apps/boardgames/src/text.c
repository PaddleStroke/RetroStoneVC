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
    stbtt_InitFont(&font[0], t->font, stbtt_GetFontOffsetForIndex(t->font, 0));
    stbtt_InitFont(&font[1], t->bold, stbtt_GetFontOffsetForIndex(t->bold, 0));
    for (int b = 0; b < 2; b++)
        stbtt_InitFont(&board[b], tx_glyph_font[b], stbtt_GetFontOffsetForIndex(tx_glyph_font[b], 0));
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

int text_name(const char *game, char *buf, int size)
{
    const char *t = text_get(game);
    if (!t || size < 1) return 0;
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

static int in(int c, const int *k)
{
    for (int i = 0; k[i]; i++)
        if (c == k[i]) return 1;
    return 0;
}

/* the marks a line must not start with (they close or follow what comes before) */
static int no_start(int c)
{
    static const int k[] = {0x3001, 0x3002, 0xff0c, 0xff0e, 0xff01, 0xff1f, 0xff1a, 0xff1b, 0xff09, 0x300d, 0x300f,
                            0x3011, 0x3009, 0x300b, 0x201d, 0x2019, 0x30fc, 0x2026, ',', '.', '!', '?', ':', ';', ')',
                            0};
    return in(c, k);
}

/* the marks a line must not end with (they open what comes after) */
static int no_end(int c)
{
    static const int k[] = {0xff08, 0x300c, 0x300e, 0x3010, 0x3008, 0x300a, 0x201c, 0x2018, '(', '[', 0};
    return in(c, k);
}

typedef struct {
    stbtt_fontinfo *f;
    float scale, size, ascent;
} face;

static face face_of(int bold, float size)
{
    face fc;
    fc.f = &font[bold ? 1 : 0];
    fc.size = size;
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
    stbtt_fontinfo *b = &board[fc->f == &font[1] || fc->f == &board[1]];
    if (!stbtt_FindGlyphIndex(b, c)) return fc->f;
    *scale = stbtt_ScaleForMappingEmToPixels(b, fc->size);
    return b;
}

static float advance(const face *fc, int c)
{
    int adv, lsb;
    float scale;
    stbtt_fontinfo *f = font_for(fc, c, &scale);
    stbtt_GetCodepointHMetrics(f, c, &adv, &lsb);
    return adv * scale;
}

/* paints one glyph at (x, baseline y) into rgba (w x h, rows from y0) */
static void glyph(const face *fc, int c, float x, float y, const uint8_t rgb[3], uint8_t *rgba, int w, int h, int y0)
{
    static unsigned char *g;
    static int cap;
    int x0, yy0, x1, yy1;
    float fx = x - floorf(x), scale;
    stbtt_fontinfo *f = font_for(fc, c, &scale);
    stbtt_GetCodepointBitmapBoxSubpixel(f, c, scale, scale, fx, 0, &x0, &yy0, &x1, &yy1);
    int gw = x1 - x0, gh = yy1 - yy0;
    if (gw <= 0 || gh <= 0) return;
    int top = (int)floorf(y) + yy0 - y0, left = (int)floorf(x) + x0;
    if (top >= h || top + gh <= 0) return;
    if (gw * gh > cap) { free(g); cap = gw * gh; g = malloc((size_t)cap); }
    stbtt_MakeCodepointBitmapSubpixel(f, g, gw, gh, gw, scale, scale, fx, 0, c);
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
            if (c == ' ') brk = p;
            else if (p < end) {                 /* between two characters of the scripts without spaces */
                const char *r = p;
                int next = utf8(&r);
                if ((cjk(c) || cjk(next)) && !no_start(next) && !no_end(c) && next != ' ') brk = p;
            }
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
    face fc = face_of(bold, px);
    fc.f = &board[bold ? 1 : 0];                /* the boards' own font */
    fc.scale = stbtt_ScaleForMappingEmToPixels(fc.f, px);
    if (!stbtt_FindGlyphIndex(fc.f, c)) return 0;
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
