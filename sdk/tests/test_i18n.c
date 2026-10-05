/*
 * RetroStone VC SDK: unit tests of rs_i18n (languages, locale and libretro mappings, plurals, positional formatting,
 * UTF-8, the extended house font). docs/i18n.md.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "rs_i18n.h"
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/* rs_lang() reads the option "lang" when no frontend set one: none here */
const char *rs_option(const char *key) { (void)key; return NULL; }

static void test_locales(void)
{
    static const struct { const char *loc; int lang; } t[] = {
        {"en", RS_LANG_EN}, {"en_US.UTF-8", RS_LANG_EN}, {"en-GB", RS_LANG_EN}, {"fr", RS_LANG_FR},
        {"fr_FR", RS_LANG_FR}, {"fr-CA", RS_LANG_FR}, {"FR_be@euro", RS_LANG_FR}, {"de-AT", RS_LANG_DE},
        {"pt", RS_LANG_PT_PT}, {"pt-PT", RS_LANG_PT_PT}, {"pt_BR", RS_LANG_PT_BR}, {"pt-br", RS_LANG_PT_BR},
        {"pt-AO", RS_LANG_PT_PT}, {"zh", RS_LANG_ZH_CN}, {"zh-Hans", RS_LANG_ZH_CN}, {"zh-Hans-CN", RS_LANG_ZH_CN},
        {"zh_CN", RS_LANG_ZH_CN}, {"zh-SG", RS_LANG_ZH_CN}, {"zh-Hant", -1}, {"zh_TW", -1}, {"zh-HK", -1},
        {"no", RS_LANG_NB}, {"nn_NO", RS_LANG_NB}, {"nb-NO", RS_LANG_NB}, {"in_ID", RS_LANG_ID},
        {"id", RS_LANG_ID}, {"ja_JP", RS_LANG_JA}, {"ko-KR", RS_LANG_KO}, {"el_GR", RS_LANG_EL},
        {"uk_UA", RS_LANG_UK}, {"cs_CZ", RS_LANG_CS}, {"da_DK", RS_LANG_DA}, {"sv-FI", RS_LANG_SV},
        {"fi", RS_LANG_FI}, {"tr_TR", RS_LANG_TR}, {"ar", -1}, {"", -1}, {"C", -1}, {"ptx", -1},
        {"e", -1}, {NULL, -1}};
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
        CHECK(rs_lang_find(t[i].loc) == t[i].lang, "rs_lang_find(%s) = %d, not %d", t[i].loc ? t[i].loc : "NULL",
              rs_lang_find(t[i].loc), t[i].lang);
    for (int l = 0; l < RS_LANG_COUNT; l++)
        CHECK(rs_lang_find(rs_lang_code(l)) == l, "the code of %d (%s) finds it", l, rs_lang_code(l));
    CHECK(rs_lang_code(RS_LANG_COUNT) == NULL, "no code past the table");
}

static void test_retro(void)
{
    /* RETRO_LANGUAGE_*: every one of ours but Danish, nothing else */
    int seen[RS_LANG_COUNT] = {0};
    for (unsigned v = 0; v < 40; v++) {
        int l = rs_lang_from_retro(v);
        if (l >= 0) seen[l] = 1;
    }
    for (int l = 0; l < RS_LANG_COUNT; l++)
        CHECK(seen[l] == (l != RS_LANG_DA), "libretro has %s: %d", rs_lang_code(l), seen[l]);
    CHECK(rs_lang_from_retro(2) == RS_LANG_FR && rs_lang_from_retro(7) == RS_LANG_PT_BR &&
          rs_lang_from_retro(8) == RS_LANG_PT_PT && rs_lang_from_retro(34) == RS_LANG_NB &&
          rs_lang_from_retro(11) == -1 && rs_lang_from_retro(30) == RS_LANG_EN, "a few values");
}

static void test_tables(void)
{
    static const char *const en[] = {"GAME OVER", "%d PANCAKES", "%d PANCAKE", NULL, NULL, "ONLY EN"};   /* plural slots: other, one, few, many */
    static const char *const fr[] = {"PARTIE TERMIN\xc3\x89" "E", "%d CR\xc3\x8aPE", "%d CR\xc3\x8aPES", NULL, NULL, NULL};
    static const char *const ru[] = {"\xd0\x9a\xd0\x9e\xd0\x9d\xd0\x95\xd0\xa6", "%d OTHER", "%d ONE", "%d FEW",
                                     "%d MANY", NULL};
    rs_strings t = {6, {0}};
    t.text[RS_LANG_EN] = en;
    t.text[RS_LANG_FR] = fr;
    t.text[RS_LANG_RU] = ru;
    CHECK(rs_lang() == RS_LANG_EN, "English by default");
    CHECK(!strcmp(rs_tr(&t, 0), "GAME OVER"), "English");
    rs_host_set_lang(RS_LANG_FR);
    CHECK(rs_lang() == RS_LANG_FR, "the frontend's language");
    CHECK(!strcmp(rs_tr(&t, 0), fr[0]), "French");
    CHECK(!strcmp(rs_tr(&t, 5), "ONLY EN"), "a missing translation is the English");
    CHECK(!strcmp(rs_tr(&t, 99), ""), "a bad id is empty");
    CHECK(!strcmp(rs_trn(&t, 1, 0), fr[2]) && !strcmp(rs_trn(&t, 1, 1), fr[2]) && !strcmp(rs_trn(&t, 1, 2), fr[1]),
          "French plural: 0 and 1 singular");
    rs_host_set_lang(RS_LANG_RU);
    CHECK(!strcmp(rs_trn(&t, 1, 1), "%d ONE") && !strcmp(rs_trn(&t, 1, 3), "%d FEW") &&
          !strcmp(rs_trn(&t, 1, 5), "%d MANY") && !strcmp(rs_trn(&t, 1, 11), "%d MANY") &&
          !strcmp(rs_trn(&t, 1, 21), "%d ONE") && !strcmp(rs_trn(&t, 1, 22), "%d FEW"), "Russian plural forms");
    rs_host_set_lang(RS_LANG_DE);
    CHECK(!strcmp(rs_trn(&t, 1, 1), "%d PANCAKE") && !strcmp(rs_trn(&t, 1, 0), "%d PANCAKES"),
          "an untranslated plural: the English forms");
    rs_host_set_lang(-1);
    CHECK(rs_plural(RS_LANG_PL, 22) == RS_PLURAL_FEW && rs_plural(RS_LANG_PL, 25) == RS_PLURAL_MANY &&
          rs_plural(RS_LANG_PL, 12) == RS_PLURAL_MANY && rs_plural(RS_LANG_CS, 3) == RS_PLURAL_FEW &&
          rs_plural(RS_LANG_CS, 5) == RS_PLURAL_OTHER && rs_plural(RS_LANG_JA, 1) == RS_PLURAL_OTHER &&
          rs_plural(RS_LANG_EN, 0) == RS_PLURAL_OTHER, "plural rules");
}

static void test_format(void)
{
    char b[64];
    rs_strf(b, sizeof b, "PRESS %s TO %s", "A", "SWIM");
    CHECK(!strcmp(b, "PRESS A TO SWIM"), "sequential: %s", b);
    rs_strf(b, sizeof b, "%2$s: %1$s", "A", "SWIM");
    CHECK(!strcmp(b, "SWIM: A"), "positional: %s", b);
    rs_strf(b, sizeof b, "P%d WINS! %03d %-4s| %c %%", 3, 7, "AB", 'Z');
    CHECK(!strcmp(b, "P3 WINS! 007 AB  | Z %"), "flags and width: %s", b);
    rs_strf(b, sizeof b, "%2$d M  +%1$d GOLD", 12, 345);
    CHECK(!strcmp(b, "345 M  +12 GOLD"), "two positional ints: %s", b);
    rs_strf(b, 8, "%s", "ABCDEFGHIJ");
    CHECK(!strcmp(b, "ABCDEFG"), "truncated: %s", b);
}

static void test_utf8(void)
{
    const char *s = "A\xc3\x89\xe3\x82\xb2\xf0\x9f\x98\x80\xff!";
    CHECK(rs_utf8_next(&s) == 'A' && rs_utf8_next(&s) == 0xC9 && rs_utf8_next(&s) == 0x30B2 &&
          rs_utf8_next(&s) == 0x1F600 && rs_utf8_next(&s) == 0xFFFD && rs_utf8_next(&s) == '!' &&
          rs_utf8_next(&s) == 0, "decoding");
    CHECK(rs_text_cols("GAME OVER") == 9, "ASCII columns");
    CHECK(rs_text_cols("\xe3\x82\xb2\xe3\x83\xbc\xe3\x83\xa0 1") == 8, "wide columns: %d",
          rs_text_cols("\xe3\x82\xb2\xe3\x83\xbc\xe3\x83\xa0 1"));
    CHECK(rs_glyph_wide(0xAC8C) && rs_glyph_wide(0x6E38) && !rs_glyph_wide(0x0416) && !rs_glyph_wide('A'),
          "wide or not");
}

static void test_font(void)
{
    const uint8_t *body, *e;
    int above, below;
    rs_font_ext('E', &e, NULL, NULL);
    CHECK(rs_font_ext(0xC9, &body, &above, &below) && body == e && above == RS_MARK_ACUTE && !below, "E acute");
    CHECK(rs_font_ext(0xE9, &body, &above, &below) && body == e && above == RS_MARK_ACUTE, "e acute: the capital");
    CHECK(rs_font_ext(0xC7, &body, &above, &below) && !above && below == RS_MARK_CEDILLA, "C cedilla");
    CHECK(rs_font_ext(0x0130, &body, &above, &below) && above == RS_MARK_DOT, "Turkish dotted I");
    CHECK(rs_font_ext(0x0419, &body, &above, &below) && above == RS_MARK_BREVE, "Cyrillic short I");
    CHECK(rs_font_ext(0x0439, &body, &above, &below) && above == RS_MARK_BREVE, "its lowercase");
    CHECK(rs_font_ext(0x0410, &body, &above, &below) && body == (rs_font_ext('A', &e, NULL, NULL), e) && !above,
          "Cyrillic A is the Latin A");
    CHECK(rs_font_ext(0x0386, &body, &above, &below) && !above, "Greek capitals drop the tonos");
    CHECK(rs_font_ext(0x03C9, &body, &above, &below), "Greek omega (lowercase)");
    CHECK(rs_font_ext(0x0141, &body, &above, &below) && !above, "L stroke");
    CHECK(!rs_font_ext(0x30B2, &body, &above, &below), "no wide glyph in the house font");
    CHECK(rs_font_ext_base(0xD6) == 'O' && rs_font_ext_base(0x0416) == '?', "the base letter");
    static const char *const langs_text[] = {
        "\xc3\x80\xc3\x82\xc3\x86\xc3\x87\xc3\x89\xc3\x88\xc3\x8a\xc3\x8b\xc3\x8e\xc3\x8f\xc3\x94\xc5\x92\xc3\x99"
        "\xc3\x9b\xc3\x9c\xc5\xb8",                                     /* French */
        "\xc4\x84\xc4\x86\xc4\x98\xc5\x81\xc5\x83\xc3\x93\xc5\x9a\xc5\xb9\xc5\xbb",   /* Polish */
        "\xc4\x8c\xc4\x8e\xc4\x9a\xc5\x87\xc5\x98\xc5\xa0\xc5\xa4\xc5\xae\xc5\xbd\xc3\x9d",  /* Czech */
        "\xc4\x9e\xc4\xb0\xc5\x9e\xc3\x96\xc3\x9c\xc3\x87\xc4\xb1",      /* Turkish */
        "\xc3\x85\xc3\x84\xc3\x96\xc3\x98\xc3\x86\xc3\x91\xc3\x83\xc3\x95\xc2\xa1\xc2\xbf\xc2\xba", /* Nordic, Iberian */
    };
    for (unsigned i = 0; i < sizeof langs_text / sizeof langs_text[0]; i++) {
        const char *s = langs_text[i];
        uint32_t c;
        while ((c = rs_utf8_next(&s)) != 0) CHECK(rs_font_ext(c, &body, &above, &below), "U+%04X has a glyph", c);
    }
    for (uint32_t c = 0x0410; c <= 0x044F; c++) CHECK(rs_font_ext(c, &body, &above, &below), "Cyrillic U+%04X", c);
    for (uint32_t c = 0x0391; c <= 0x03A9; c++)
        if (c != 0x03A2) CHECK(rs_font_ext(c, &body, &above, &below), "Greek U+%04X", c);
    static const uint32_t uk[] = {0x0404, 0x0406, 0x0407, 0x0490, 0x0454, 0x0456, 0x0457, 0x0491, 0x0401, 0x0451};
    for (unsigned i = 0; i < sizeof uk / sizeof uk[0]; i++)
        CHECK(rs_font_ext(uk[i], &body, &above, &below), "Ukrainian/Russian U+%04X", uk[i]);
}

int main(void)
{
    test_locales();
    test_retro();
    test_tables();
    test_format();
    test_utf8();
    test_font();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "all passed", checks, fails);
    return fails != 0;
}
