/*
 * The board games' texts: the rules of each game in 22 languages (apps/boardgames/help, tools/gen_text.py), the
 * language of the device, and a page of text drawn into a picture (stb_truetype, Noto Sans CJK cut down to what the
 * texts use).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

/* the generated tables (text_data.c) */
typedef struct {
    const char *code;                           /* "en", "pt_BR", "zh_CN" */
    const unsigned char *font;                  /* its fonts: regular, bold (OpenType) */
    int font_size;
    const unsigned char *bold;
    int bold_size;
} tx_lang;
typedef struct { const char *lang, *game, *text; } tx_text;
extern const tx_lang tx_langs[];
extern const int tx_nlangs;
extern const tx_text tx_texts[];
extern const int tx_ntexts;
extern const unsigned char *const tx_glyph_font[2];   /* the boards' characters (help/glyphs.txt): regular, bold */
extern const unsigned char *const tx_glyph_font2[2];  /* the ones the first lacks (DejaVu Sans; NULL: none) */

/* the language: forced ("fr", "pt_BR"...), or NULL: the device's (English when it is none of the 22). Called again,
 * it switches (the Settings page): the texts read from then on are the new language's. */
void text_init(const char *forced);
const char *text_lang(void);

/* a game's text in the language (English if it has none); NULL: none at all. "_app": the shell's buttons. */
const char *text_get(const char *game);
/* the game's name: the first line of its text (in buf); 0: none */
int text_name(const char *game, char *buf, int size);
/* a game's own short words, shown in the game (a bid, a call): help/<lang>/<id>_words.txt, its first line a title (for
 * the translators), then a word or a few a line. Line i (0: the first after the title) of text `key` ("<id>_words";
 * tarot's is "tarot_bids") into buf; 0: none. The shell draws one with ui_word(). */
int text_word(const char *key, int i, char *buf, int size);

/* a page: the text laid out to a width (pixels) with a font size (pixels): its first line as the title (bold,
 * larger), "# " headings (bold), "- " list items, paragraphs; colour rgb on a clear picture. Returns the height;
 * draws into rgba (w x h_max, cleared by the caller) when it is not NULL, only the rows in [y0, y0 + h_max). */
int text_page(const char *s, int w, float px, const uint8_t rgb[3], uint8_t *rgba, int y0, int h_max);

/* one character (a codepoint the boards draw: help/glyphs.txt keeps it in the fonts), bold or not, centred on its ink
 * in a w x h picture, its em `px` pixels: for pieces that carry a character (xiangqi, shogi, word tiles). 0: no such
 * glyph.
 * Draw it into a CLEAR picture (the ink's coverage becomes the alpha), then art_blend() it onto the piece. */
int text_glyph(int codepoint, float px, int bold, const uint8_t rgb[3], uint8_t *rgba, int w, int h);

/* one line (no wrapping), bold or not: its width; drawn at (x, y top) into rgba (w x h) when not NULL */
int text_line(const char *s, float px, int bold, const uint8_t rgb[3], uint8_t *rgba, int w, int h, int x, int y);

/* a title (the pages' names, their sections' headers): one line in capitals, bold, condensed (Chinese, Japanese and
 * Korean as they are); as text_line */
int text_title(const char *s, float px, const uint8_t rgb[3], uint8_t *rgba, int w, int h, int x, int y);

/* a text as a search compares it (UTF-8 into out, size bytes): small letters, no accents (é e, ß s, ё е, ά α);
 * returns its length */
int text_fold(const char *s, char *out, int size);
/* where the character before byte n of s starts (one character back: the search field's backspace) */
int text_utf8_back(const char *s, int n);

#endif
