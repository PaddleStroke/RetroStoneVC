/*
 * RetroStone VC SDK: languages, string tables, UTF-8 and positional formatting (rs_i18n.h, docs/i18n.md).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "rs.h"
#include "rs_i18n.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *const codes[RS_LANG_COUNT] = {
    "en", "cs", "da", "de", "el", "es", "fi", "fr", "id", "it", "ja", "ko", "nb", "nl", "pl", "pt_BR", "pt_PT", "ru",
    "sv", "tr", "uk", "zh_CN"};

static int host_lang = -1;
static const rs_i18n_pack *pack;

const char *rs_lang_code(int lang) { return lang >= 0 && lang < RS_LANG_COUNT ? codes[lang] : NULL; }
void rs_host_set_lang(int lang) { host_lang = lang >= 0 && lang < RS_LANG_COUNT ? lang : -1; }
int rs_lang_wide(int lang) { return lang == RS_LANG_JA || lang == RS_LANG_KO || lang == RS_LANG_ZH_CN; }

int rs_lang(void)
{
    if (host_lang >= 0) return host_lang;
    const char *o = rs_option("lang");
    int l = o ? rs_lang_find(o) : -1;
    return l >= 0 ? l : RS_LANG_EN;
}

int rs_lang_find(const char *locale)
{
    char s[32], lang[8];
    size_t n = 0;
    if (!locale) return -1;
    for (; locale[n] && n + 1 < sizeof s; n++) {
        char c = (char)tolower((unsigned char)locale[n]);
        s[n] = c == '-' ? '_' : c;
    }
    s[n] = 0;
    s[strcspn(s, ".@")] = 0;                        /* en_US.UTF-8, sr@latin */
    size_t k = strcspn(s, "_");
    if (k == 0 || k >= sizeof lang) return -1;
    memcpy(lang, s, k);
    lang[k] = 0;
    const char *region = s[k] ? s + k + 1 : "";
    if (!strcmp(lang, "pt")) return !strncmp(region, "br", 2) ? RS_LANG_PT_BR : RS_LANG_PT_PT;
    if (!strcmp(lang, "zh")) {
        /* Simplified: zh, zh_CN, zh_SG, zh_Hans...; Traditional (zh_Hant, zh_TW, zh_HK, zh_MO) is not ours */
        if (!strncmp(region, "hant", 4) || !strncmp(region, "tw", 2) || !strncmp(region, "hk", 2) ||
            !strncmp(region, "mo", 2))
            return -1;
        return RS_LANG_ZH_CN;
    }
    if (!strcmp(lang, "no") || !strcmp(lang, "nn") || !strcmp(lang, "nb")) return RS_LANG_NB;
    if (!strcmp(lang, "in")) return RS_LANG_ID;     /* the old code of Indonesian (Java, older Androids) */
    for (int i = 0; i < RS_LANG_COUNT; i++)
        if (!strncmp(codes[i], lang, k) && (codes[i][k] == 0 || codes[i][k] == '_')) return i;
    return -1;
}

int rs_lang_from_retro(unsigned v)
{
    /* RETRO_LANGUAGE_* (libretro.h): 0 English .. 36 Thai; no Danish */
    switch (v) {
    case 0: case 30: return RS_LANG_EN;             /* English, British English */
    case 1: return RS_LANG_JA;
    case 2: return RS_LANG_FR;
    case 3: return RS_LANG_ES;
    case 4: return RS_LANG_DE;
    case 5: return RS_LANG_IT;
    case 6: return RS_LANG_NL;
    case 7: return RS_LANG_PT_BR;
    case 8: return RS_LANG_PT_PT;
    case 9: return RS_LANG_RU;
    case 10: return RS_LANG_KO;
    case 12: return RS_LANG_ZH_CN;
    case 14: return RS_LANG_PL;
    case 17: return RS_LANG_EL;
    case 18: return RS_LANG_TR;
    case 23: return RS_LANG_FI;
    case 24: return RS_LANG_ID;
    case 25: return RS_LANG_SV;
    case 26: return RS_LANG_UK;
    case 27: return RS_LANG_CS;
    case 34: return RS_LANG_NB;                     /* Norwegian (newer libretro.h) */
    default: return -1;
    }
}

/* ---- string tables -------------------------------------------------------------------------------------------------- */
static const char *lookup(const rs_strings *t, int lang, int id)
{
    if (!t || id < 0 || id >= t->count) return NULL;
    const char *const *tab = lang >= 0 && lang < RS_LANG_COUNT ? t->text[lang] : NULL;
    return tab ? tab[id] : NULL;
}

const char *rs_tr(const rs_strings *t, int id)
{
    const char *s = lookup(t, rs_lang(), id);
    if (!s) s = lookup(t, RS_LANG_EN, id);
    return s ? s : "";
}

int rs_plural(int lang, int n)
{
    int a = n < 0 ? -n : n, d = a % 10, h = a % 100;
    switch (lang) {
    case RS_LANG_JA: case RS_LANG_KO: case RS_LANG_ZH_CN: case RS_LANG_ID:
        return RS_PLURAL_OTHER;
    case RS_LANG_FR: case RS_LANG_PT_BR:
        return a <= 1 ? RS_PLURAL_ONE : RS_PLURAL_OTHER;
    case RS_LANG_CS:
        return a == 1 ? RS_PLURAL_ONE : a >= 2 && a <= 4 ? RS_PLURAL_FEW : RS_PLURAL_OTHER;
    case RS_LANG_PL:
        if (a == 1) return RS_PLURAL_ONE;
        return d >= 2 && d <= 4 && (h < 12 || h > 14) ? RS_PLURAL_FEW : RS_PLURAL_MANY;
    case RS_LANG_RU: case RS_LANG_UK:
        if (d == 1 && h != 11) return RS_PLURAL_ONE;
        return d >= 2 && d <= 4 && (h < 12 || h > 14) ? RS_PLURAL_FEW : RS_PLURAL_MANY;
    default:
        return a == 1 ? RS_PLURAL_ONE : RS_PLURAL_OTHER;
    }
}

const char *rs_trn(const rs_strings *t, int id, int n)
{
    int lang = rs_lang(), f = rs_plural(lang, n);
    const char *s = lookup(t, lang, id + f);
    if (!s && f != RS_PLURAL_OTHER) s = lookup(t, lang, id);
    if (!s) {
        f = rs_plural(RS_LANG_EN, n);
        s = lookup(t, RS_LANG_EN, id + f);
        if (!s) s = lookup(t, RS_LANG_EN, id);
    }
    return s ? s : "";
}

/* ---- positional formatting -------------------------------------------------------------------------------------- */
#define FMT_ARGS 16
typedef struct spec { int pos; char conv; char text[24]; } spec;

/* parses the conversion at f (just after '%'); fills sp (its printf spec without the position); returns its length */
static int parse_spec(const char *f, spec *sp)
{
    const char *p = f;
    int pos = 0;
    if (isdigit((unsigned char)*p)) {
        const char *q = p;
        int v = 0;
        while (isdigit((unsigned char)*q)) v = v * 10 + (*q++ - '0');
        if (*q == '$') { pos = v; p = q + 1; }
    }
    const char *body = p;
    while (*p && strchr("-+ 0#", *p)) p++;
    while (isdigit((unsigned char)*p)) p++;
    if (*p == '.') { p++; while (isdigit((unsigned char)*p)) p++; }
    sp->pos = pos;
    sp->conv = *p;
    size_t n = (size_t)(p - body) + 1;
    if (n + 2 > sizeof sp->text) n = sizeof sp->text - 2;
    sp->text[0] = '%';
    memcpy(sp->text + 1, body, n);
    sp->text[n + 1] = 0;
    return *p ? (int)(p - f) + 1 : (int)(p - f);
}

int rs_strf(char *buf, size_t size, const char *fmt, ...)
{
    union { int i; const char *s; } arg[FMT_ARGS + 1];
    char type[FMT_ARGS + 1];
    int seq = 0, maxpos = 0;
    memset(type, 0, sizeof type);
    /* pass 1: the type of each argument */
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') continue;
        if (f[1] == '%') { f++; continue; }
        spec sp;
        int len = parse_spec(f + 1, &sp);
        int pos = sp.pos ? sp.pos : ++seq;
        if (pos > FMT_ARGS) pos = FMT_ARGS;
        if (pos > maxpos) maxpos = pos;
        type[pos] = sp.conv == 's' ? 's' : 'i';
        f += len;
    }
    va_list ap;
    va_start(ap, fmt);
    for (int i = 1; i <= maxpos; i++) {
        if (type[i] == 's') arg[i].s = va_arg(ap, const char *);
        else arg[i].i = va_arg(ap, int);           /* d i u c (and an unused position: an int) */
    }
    va_end(ap);
    /* pass 2: the text */
    size_t o = 0;
    seq = 0;
    if (!size) return 0;
    for (const char *f = fmt; *f && o + 1 < size; f++) {
        if (*f != '%') { buf[o++] = *f; continue; }
        if (f[1] == '%') { buf[o++] = '%'; f++; continue; }
        spec sp;
        int len = parse_spec(f + 1, &sp);
        int pos = sp.pos ? sp.pos : ++seq;
        if (pos > FMT_ARGS) pos = FMT_ARGS;
        f += len;
        int w;
        switch (sp.conv) {
        case 's': w = snprintf(buf + o, size - o, sp.text, arg[pos].s ? arg[pos].s : "(null)"); break;
        case 'd': case 'i': case 'u': case 'c': case 'x': case 'X':
            w = snprintf(buf + o, size - o, sp.text, arg[pos].i); break;
        default: w = 0; break;
        }
        if (w < 0) w = 0;
        o += (size_t)w < size - o ? (size_t)w : size - o - 1;
        if (!*f) break;
    }
    buf[o] = 0;
    return (int)o;
}

/* ---- UTF-8 ----------------------------------------------------------------------------------------------------------- */
uint32_t rs_utf8_next(const char **sp)
{
    const unsigned char *s = (const unsigned char *)*sp;
    uint32_t c = s[0];
    int n;
    if (!c) return 0;
    if (c < 0x80) { *sp += 1; return c; }
    if ((c & 0xe0) == 0xc0) { n = 1; c &= 0x1f; }
    else if ((c & 0xf0) == 0xe0) { n = 2; c &= 0x0f; }
    else if ((c & 0xf8) == 0xf0) { n = 3; c &= 0x07; }
    else { *sp += 1; return 0xfffd; }
    for (int i = 1; i <= n; i++) {
        if ((s[i] & 0xc0) != 0x80) { *sp += i; return 0xfffd; }
        c = c << 6 | (s[i] & 0x3f);
    }
    *sp += n + 1;
    return c;
}

int rs_glyph_wide(uint32_t c)
{
    return (c >= 0x1100 && c <= 0x115f) || (c >= 0x2e80 && c <= 0xa4cf && c != 0x303f) ||
           (c >= 0xac00 && c <= 0xd7a3) || (c >= 0xf900 && c <= 0xfaff) || (c >= 0xfe30 && c <= 0xfe4f) ||
           (c >= 0xff00 && c <= 0xff60) || (c >= 0xffe0 && c <= 0xffe6);
}

int rs_text_cols(const char *s)
{
    int n = 0;
    uint32_t c;
    if (!s) return 0;
    while ((c = rs_utf8_next(&s)) != 0) n += rs_glyph_wide(c) ? 2 : 1;
    return n;
}

/* ---- the text pack --------------------------------------------------------------------------------------------------- */
void rs_i18n_register(const rs_i18n_pack *p) { pack = p; }
const rs_i18n_pack *rs_i18n(void) { return pack; }

const uint8_t *rs_wide_glyph(uint32_t cp)
{
    const rs_wide_font *w = pack ? pack->wide : NULL;
    if (!w) return NULL;
    int lo = 0, hi = w->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (w->cp[mid] == cp) return w->bits[mid];
        if (w->cp[mid] < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}
