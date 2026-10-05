#!/usr/bin/env python3
"""RetroStone VC: compile the games' translated strings and their glyphs (docs/i18n.md).

  i18n_build.py --game G --out build/gen/G     build/gen/G/game_text.c + game_text.h: the kit's and the game's string
                                               tables in the 22 languages, the wide-glyph subset (Unifont) they use,
                                               registered with the runtime
  i18n_build.py --kit-header                   (re)writes games/common/src/house_text.h (the kit's string ids)
  i18n_build.py --check [G ...]                validates: keys, %-conversions, plural forms, glyphs, room; prints the
                                               coverage and the strings longer than their room (all games by default)
  i18n_build.py --fonts [--unifont HEX]        rebuilds games/common/fonts/unifont-subset.hex: every wide character
                                               that any game's strings use, from the full Unifont .hex
  i18n_build.py --report                       a coverage table (languages x games) in Markdown

Strings: games/common/strings/<lang>.txt (the kit) and games/<game>/strings/<lang>.txt, en.txt being the source:
    #. a note for translators ("max N": the room in 8-px columns)
    KEY = TEXT                  ("..." keeps spaces at the ends)
    KEY[one] = TEXT             plural forms: one, few, many, other
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import glob
import os
import re
import sys
import unicodedata

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "sdk", "tools"))
import gen_font_ext  # noqa: E402

LANGS = ["en", "cs", "da", "de", "el", "es", "fi", "fr", "id", "it", "ja", "ko", "nb", "nl", "pl", "pt_BR", "pt_PT",
         "ru", "sv", "tr", "uk", "zh_CN"]
FORMS = ["other", "one", "few", "many"]
# the plural forms each language needs (rs_plural in sdk/src/rs_i18n.c)
NEEDS = {l: ["one", "other"] for l in LANGS}
NEEDS.update({l: ["other"] for l in ("ja", "ko", "zh_CN", "id")})
NEEDS.update({"cs": ["one", "few", "other"], "pl": ["one", "few", "many"], "ru": ["one", "few", "many"],
              "uk": ["one", "few", "many"]})
KIT_DIR = os.path.join(ROOT, "games", "common", "strings")
KIT_HEADER = os.path.join(ROOT, "games", "common", "src", "house_text.h")
FONT_SUBSET = os.path.join(ROOT, "games", "common", "fonts", "unifont-subset.hex")
CONV = re.compile(r"%(?:(\d+)\$)?[-+ 0#]*\d*(?:\.\d+)?([diucsxX%])")
KEY_RE = re.compile(r"^([A-Z][A-Z0-9_]*)(?:\[(one|few|many|other)\])?\s*=\s?(.*)$")


class Entry:
    def __init__(self, key, line):
        self.key, self.line = key, line
        self.forms = {}             # form -> text ("" key for a simple string)
        self.note, self.max = "", None

    @property
    def plural(self):
        return "" not in self.forms


def parse(path):
    """The entries of a strings file, in order: {key: Entry}."""
    out, note = {}, []
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            line = raw.rstrip("\r\n")
            if not line.strip():
                note = []
                continue
            if line.startswith("#."):
                note.append(line[2:].strip())
                continue
            if line.startswith("#"):
                continue
            m = KEY_RE.match(line)
            if not m:
                raise SystemExit("%s:%d: not KEY = TEXT: %r" % (path, n, line))
            key, form, text = m.group(1), m.group(2) or "", m.group(3)
            text = text.rstrip()
            if len(text) >= 2 and text[0] == '"' and text[-1] == '"':
                text = text[1:-1]
            e = out.get(key)
            if not e:
                e = out[key] = Entry(key, n)
                e.note = " ".join(note)
                mm = re.search(r"\bmax (\d+)", e.note)
                e.max = int(mm.group(1)) if mm else None
            if form in e.forms:
                raise SystemExit("%s:%d: %s%s twice" % (path, n, key, "[%s]" % form if form else ""))
            if form and "" in e.forms or not form and e.forms:
                raise SystemExit("%s:%d: %s is both plural and not" % (path, n, key))
            e.forms[form] = text
            note = []
    return out


def convs(text):
    """The conversions of a format: [(position, type)] with type 'i' (an int) or 's'."""
    seq, out = 0, []
    for m in CONV.finditer(text):
        if m.group(2) == "%":
            continue
        if m.group(1):
            pos = int(m.group(1))
        else:
            seq += 1
            pos = seq
        out.append((pos, "s" if m.group(2) == "s" else "i"))
    return sorted(set(out))


def upper(text, lang):
    """Capitals for the house font, in the language's way; the %-conversions stay as they are."""
    parts, last = [], 0
    for m in CONV.finditer(text):
        parts.append((False, text[last:m.start()]))
        parts.append((True, m.group(0)))
        last = m.end()
    parts.append((False, text[last:]))
    out = []
    for keep, s in parts:
        if not keep:
            if lang == "tr":
                s = s.replace("i", "\u0130").replace("\u0131", "I")
            s = s.upper()
            if lang == "el":            # Greek capitals drop the tonos (and keep the diaeresis)
                s = unicodedata.normalize("NFC", "".join(c for c in unicodedata.normalize("NFD", s)
                                                           if c not in "\u0301\u0341"))
            # typography the font does not draw
            for a, b in (("\u2019", "'"), ("\u2018", "'"), ("\u201c", '"'), ("\u201d", '"'), ("\u2026", "..."),
                         ("\u2013", "-"), ("\u2014", "-"), ("\u00a0", " "), ("\u202f", " "), ("\u2009", " ")):
                s = s.replace(a, b)
        out.append(s)
    return "".join(out)


def wide(c):
    o = ord(c)
    return (0x1100 <= o <= 0x115F or (0x2E80 <= o <= 0xA4CF and o != 0x303F) or 0xAC00 <= o <= 0xD7A3 or
            0xF900 <= o <= 0xFAFF or 0xFE30 <= o <= 0xFE4F or 0xFF00 <= o <= 0xFF60 or 0xFFE0 <= o <= 0xFFE6)


def cols(text):
    """Columns on screen, the conversions counted as their usual size (%d: 3 digits, %s: 4)."""
    n = 0
    for part in re.split(r"(%(?:\d+\$)?[-+ 0#]*\d*(?:\.\d+)?[diucsxX%])", text):
        if part.startswith("%") and len(part) > 1:
            n += 1 if part.endswith("%") or part.endswith("c") else 3 if part[-1] in "diuxX" else 4
        else:
            n += sum(2 if wide(c) else 1 for c in part)
    return n


_ext = None


def narrow_ok(c):
    global _ext
    if _ext is None:
        _ext = {e[0] for e in gen_font_ext.table()[1]}
    o = ord(c)
    return 32 <= o < 127 or o in _ext


def load_set(d):
    """{lang: {key: Entry}} of a strings folder (en first); {} when there is none."""
    if not os.path.isdir(d):
        return {}
    sets = {}
    for path in sorted(glob.glob(os.path.join(d, "*.txt"))):
        lang = os.path.splitext(os.path.basename(path))[0]
        if lang not in LANGS:
            raise SystemExit("%s: unknown language %r (one of %s)" % (path, lang, " ".join(LANGS)))
        sets[lang] = parse(path)
    if sets and "en" not in sets:
        raise SystemExit("%s: no en.txt" % d)
    return sets


def check_set(name, sets, wide_font, problems, flags):
    """Validates the translations against the English; fills problems (errors) and flags (too long)."""
    en = sets.get("en", {})
    for lang, ents in sets.items():
        where = "%s/%s" % (name, lang)
        for key, e in ents.items():
            src = en.get(key)
            if not src:
                problems.append("%s: %s is not in en.txt" % (where, key))
                continue
            if src.plural != e.plural:
                problems.append("%s: %s must %sbe plural, like the English" % (where, key, "" if src.plural else "not "))
                continue
            ref = convs(next(iter(src.forms.values())))
            for form, text in e.forms.items():
                if form and form not in FORMS:
                    problems.append("%s: %s[%s]: no such form" % (where, key, form))
                if convs(text) != ref and not (src.plural and lang != "en" and
                                               [t for t in convs(text) if t[1] == "s"] == [t for t in ref if t[1] == "s"]
                                               and len(convs(text)) <= len(ref)):
                    problems.append("%s: %s%s: the conversions %s differ from the English %s" %
                                    (where, key, "[%s]" % form if form else "", convs(text), ref))
                shown = upper(text, lang)
                for c in shown:
                    if wide(c):
                        if wide_font is not None and ord(c) not in wide_font:
                            problems.append("%s: %s: no wide glyph for %s (U+%04X): run i18n_build.py --fonts" %
                                            (where, key, c, ord(c)))
                    elif not narrow_ok(c):
                        problems.append("%s: %s: the house font has no %r (U+%04X)" % (where, key, c, ord(c)))
                # the room: "max N" in the note, else the English; measured like the English (the conversions
                # at their usual size), so a translation may grow by the room the English leaves
                en_cols = max(cols(t) for t in src.forms.values())
                room = max(src.max or en_cols, en_cols) if src.max is None else src.max
                slack = max(0, room - en_cols)
                if lang != "en" and cols(shown) - en_cols > slack:
                    flags.append((where, key, cols(shown) - en_cols + min(room, en_cols), room, shown))
            if src.plural and lang != "en":
                for need in NEEDS[lang]:
                    if need not in e.forms and not (need == "many" and "other" in e.forms):
                        problems.append("%s: %s: the plural form [%s] is missing" % (where, key, need))
        if lang == "en":
            for key, e in ents.items():
                if e.plural and not ("one" in e.forms and "other" in e.forms):
                    problems.append("%s: %s: an English plural has [one] and [other]" % (where, key))


def ids(en):
    """The ids of a set: [(KEY, first id, plural)] and the count."""
    out, n = [], 0
    for key, e in en.items():
        out.append((key, n, e.plural))
        n += 4 if e.plural else 1
    return out, n


def c_str(s):
    out = []
    for b in s.encode("utf-8"):
        c = chr(b)
        if c == '"' or c == "\\":
            out.append("\\" + c)
        elif 32 <= b < 127 and c != "?":
            out.append(c)
        else:
            out.append("\\%03o" % b)
    return '"' + "".join(out) + '"'


def table_rows(sets, lang, idlist, count):
    """The strings of one language in id order (None = not translated)."""
    ents = sets.get(lang, {})
    rows = [None] * count
    for key, first, plural in idlist:
        e = ents.get(key)
        if not e:
            continue
        if plural:
            forms = dict(e.forms)
            if "other" not in forms and "many" in forms:
                forms["other"] = forms["many"]
            for i, form in enumerate(FORMS):
                if form in forms:
                    rows[first + i] = upper(forms[form], lang)
        else:
            rows[first] = upper(e.forms[""], lang)
    return rows


def load_unifont(path):
    font = {}
    with open(path, encoding="ascii") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            cp, bits = line.split(":")
            font[int(cp, 16)] = bits
    return font


def wide_chars(*setlists):
    chars = set()
    for sets in setlists:
        for lang, ents in sets.items():
            for e in ents.values():
                for t in e.forms.values():
                    chars.update(c for c in upper(t, lang) if wide(c))
    return chars


def gen_game(game, out):
    kit = load_set(KIT_DIR)
    gsets = load_set(os.path.join(ROOT, "games", game, "strings"))
    font = load_unifont(FONT_SUBSET) if os.path.exists(FONT_SUBSET) else {}
    problems, flags = [], []
    check_set("common", kit, set(font), problems, flags)
    if gsets:
        check_set(game, gsets, set(font), problems, flags)
    if problems:
        raise SystemExit("i18n_build: %s:\n  %s" % (game, "\n  ".join(problems)))
    kit_ids, kit_n = ids(kit.get("en", {}))
    g_ids, g_n = ids(gsets.get("en", {}))
    chars = sorted(ord(c) for c in wide_chars(kit, gsets))
    os.makedirs(out, exist_ok=True)
    h = ["/* Generated by tools/i18n_build.py from games/%s/strings: the game's string ids (docs/i18n.md). */" % game,
         "#ifndef GAME_TEXT_H", "#define GAME_TEXT_H", '#include "rs_i18n.h"', "", "enum {"]
    for key, first, plural in g_ids:
        h.append("    GS_%s = %d,%s" % (key, first, "  /* plural: 4 slots */" if plural else ""))
    h += ["    GS_COUNT = %d" % g_n, "};", "extern const rs_strings gs_strings;",
          "/* the game's string KEY in the player's language (UTF-8; English when not translated) */",
          "#define TR(key) rs_tr(&gs_strings, GS_##key)", "#define TRN(key, n) rs_trn(&gs_strings, GS_##key, (n))",
          "#endif", ""]
    c = ["/* Generated by tools/i18n_build.py from games/common/strings and games/%s/strings: do not edit." % game,
         " * The wide glyphs are a subset of GNU Unifont 13.0.06 (SIL OFL 1.1, THIRD_PARTY.md). */",
         '#include "game_text.h"', ""]

    def emit_tables(prefix, sets, idlist, count):
        names = []
        for lang in LANGS:
            if lang not in sets or count == 0:
                names.append("NULL")
                continue
            rows = table_rows(sets, lang, idlist, count)
            name = "%s_%s" % (prefix, lang.lower())
            c.append("static const char *const %s[%d] = {" % (name, count))
            for r in rows:
                c.append("    %s," % (c_str(r) if r is not None else "NULL"))
            c.append("};")
            names.append(name)
        return names

    kn = emit_tables("kit", kit, kit_ids, kit_n)
    c.append("static const rs_strings kit_strings = {%d, {%s}};" % (kit_n, ", ".join(kn)))
    gn = emit_tables("game", gsets, g_ids, g_n)
    c.append("const rs_strings gs_strings = {%d, {%s}};" % (g_n, ", ".join(gn)))
    c.append("")
    if chars:
        c.append("static const uint16_t wide_cp[%d] = {" % len(chars))
        for i in range(0, len(chars), 12):
            c.append("    " + " ".join("0x%04x," % v for v in chars[i:i + 12]))
        c.append("};")
        c.append("static const uint8_t wide_bits[%d][32] = {" % len(chars))
        for v in chars:
            bits = font[v]
            if len(bits) == 32:             # an 8x16 glyph: centre it in 16 columns
                bits = "".join("%04x" % (int(bits[i:i + 2], 16) << 4) for i in range(0, 32, 2))
            c.append("    {%s}, /* U+%04X */" % (", ".join("0x" + bits[i:i + 2] for i in range(0, 64, 2)), v))
        c.append("};")
        c.append("static const rs_wide_font wide = {%d, wide_cp, wide_bits};" % len(chars))
    else:
        c.append("static const rs_wide_font wide = {0, NULL, NULL};")
    c += ["static const rs_i18n_pack pack = {&kit_strings, &gs_strings, &wide};", "",
          "/* registered before main() (or when the libretro core is loaded): the kit finds the strings there */",
          "__attribute__((constructor)) static void game_text_register(void) { rs_i18n_register(&pack); }", ""]
    write_if_changed(os.path.join(out, "game_text.h"), "\n".join(h))
    write_if_changed(os.path.join(out, "game_text.c"), "\n".join(c))
    for f in flags:
        print("i18n_build: note: %s: %s is %d columns, room %d: %s" % f)


def write_if_changed(path, text):
    try:
        with open(path, encoding="utf-8") as f:
            if f.read() == text:
                return
    except OSError:
        pass
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def kit_header():
    kit = load_set(KIT_DIR)
    en = kit["en"]
    idlist, n = ids(en)
    h = ["/* Generated by tools/i18n_build.py --kit-header from games/common/strings/en.txt: the house kit's string ids",
         " * (docs/i18n.md). Committed: house_ui.c is compiled once for every game. */",
         "#ifndef HOUSE_TEXT_H", "#define HOUSE_TEXT_H", "", "enum {"]
    for key, first, plural in idlist:
        h.append("    HS_%s = %d,%s" % (key, first, "  /* plural: 4 slots */" if plural else ""))
    h += ["    HS_COUNT = %d" % n, "};", "",
          "#ifdef HOUSE_TEXT_ENGLISH",
          "/* the English, when the game has no text pack (house_ui.c) */",
          "static const char *const hs_english[HS_COUNT] = {"]
    rows = table_rows(kit, "en", idlist, n)
    for r in rows:
        h.append("    %s," % (c_str(r) if r is not None else "NULL"))
    h += ["};", "#endif", "#endif", ""]
    write_if_changed(KIT_HEADER, "\n".join(h))


def all_games():
    return sorted(os.path.basename(os.path.dirname(d)) for d in glob.glob(os.path.join(ROOT, "games", "*", "strings"))
                  if os.path.basename(os.path.dirname(d)) != "common")


def check(games):
    font = load_unifont(FONT_SUBSET) if os.path.exists(FONT_SUBSET) else None
    problems, flags = [], []
    kit = load_set(KIT_DIR)
    check_set("common", kit, set(font) if font is not None else None, problems, flags)
    for g in games:
        check_set(g, load_set(os.path.join(ROOT, "games", g, "strings")), set(font) if font is not None else None,
                  problems, flags)
    for f in flags:
        print("too long: %s: %s is %d columns, room %d: %s" % f)
    for p in problems:
        print("error: " + p)
    if os.path.exists(KIT_HEADER):
        idlist, n = ids(kit["en"])
        with open(KIT_HEADER, encoding="utf-8") as f:
            have = f.read()
        if any("HS_%s = %d," % (k, i) not in have for k, i, _ in idlist) or "HS_COUNT = %d\n" % n not in have:
            problems.append("games/common/src/house_text.h is out of date: run tools/i18n_build.py --kit-header")
            print("error: " + problems[-1])
    print("%d errors, %d strings longer than their room" % (len(problems), len(flags)))
    return 1 if problems else 0


def find_unifont(path):
    if path:
        return path
    for pat in (os.path.join(ROOT, "build", "deps", "SDL2-*", "test", "unifont-*.hex"),
                "/root/rsvc/dl/SDL2-*/test/unifont-*.hex", os.environ.get("UNIFONT_HEX", "")):
        hits = sorted(glob.glob(pat)) if pat else []
        if hits:
            return hits[-1]
    raise SystemExit("no Unifont .hex: give --unifont (SDL2's source has test/unifont-13.0.06.hex)")


def fonts(path):
    full = load_unifont(find_unifont(path))
    sets = [load_set(KIT_DIR)] + [load_set(os.path.join(ROOT, "games", g, "strings")) for g in all_games()]
    chars = sorted(ord(c) for c in wide_chars(*sets))
    missing = [c for c in chars if c not in full]
    if missing:
        raise SystemExit("Unifont has no glyph for " + " ".join("U+%04X" % c for c in missing))
    os.makedirs(os.path.dirname(FONT_SUBSET), exist_ok=True)
    lines = ["# GNU Unifont 13.0.06 (SIL Open Font License 1.1, see LICENSE-unifont-OFL.txt): the wide characters",
             "# (CJK, kana, hangul) that the games' strings use. Rebuilt by tools/i18n_build.py --fonts; do not edit."]
    lines += ["%04X:%s" % (c, full[c]) for c in chars]
    write_if_changed(FONT_SUBSET, "\n".join(lines) + "\n")
    print("%s: %d glyphs" % (os.path.relpath(FONT_SUBSET, ROOT), len(chars)))


def report():
    games = all_games()
    kit = load_set(KIT_DIR)
    print("| | kit | " + " | ".join(games) + " |")
    print("|---|---|" + "---|" * len(games))
    gsets = {g: load_set(os.path.join(ROOT, "games", g, "strings")) for g in games}
    for lang in LANGS:
        row = []
        for sets in [kit] + [gsets[g] for g in games]:
            en = sets.get("en", {})
            have = sets.get(lang, {})
            row.append("%d/%d" % (sum(1 for k in en if k in have), len(en)))
        print("| %s | %s |" % (lang, " | ".join(row)))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--game")
    ap.add_argument("--out")
    ap.add_argument("--kit-header", action="store_true")
    ap.add_argument("--check", nargs="*")
    ap.add_argument("--fonts", action="store_true")
    ap.add_argument("--unifont")
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    if a.kit_header:
        kit_header()
    if a.fonts:
        fonts(a.unifont)
    if a.game:
        gen_game(a.game, a.out or os.path.join(ROOT, "build", "gen", a.game))
    if a.report:
        report()
    if a.check is not None:
        sys.exit(check(a.check or all_games()))


if __name__ == "__main__":
    main()
