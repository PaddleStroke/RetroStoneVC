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

/* the language: forced ("fr", "pt_BR"...), or NULL: the device's (English when it is none of the 22) */
void text_init(const char *forced);
const char *text_lang(void);

/* a game's text in the language (English if it has none); NULL: none at all. "_app": the shell's buttons. */
const char *text_get(const char *game);
/* the game's name: the first line of its text (in buf); 0: none */
int text_name(const char *game, char *buf, int size);

/* a page: the text laid out to a width (pixels) with a font size (pixels): its first line as the title (bold,
 * larger), "# " headings (bold), "- " list items, paragraphs; colour rgb on a clear picture. Returns the height;
 * draws into rgba (w x h_max, cleared by the caller) when it is not NULL, only the rows in [y0, y0 + h_max). */
int text_page(const char *s, int w, float px, const uint8_t rgb[3], uint8_t *rgba, int y0, int h_max);

/* one line (no wrapping), bold or not: its width; drawn at (x, y top) into rgba (w x h) when not NULL */
int text_line(const char *s, float px, int bold, const uint8_t rgb[3], uint8_t *rgba, int w, int h, int x, int y);

#endif
