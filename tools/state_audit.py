#!/usr/bin/env python3
"""Save-state audit: every mutable static object of a game must be saved, or be listed as scratch.

    state_audit.py --game GAME TEST_STATES_BINARY OBJECT.o...

The game registers its objects as "<file>.<name>" (rs_state_var("draw.iris_on", ...), rs_state_ptr for a pointer
variable of its own; <file> is the source file's stem), which
TEST_STATES_BINARY --list prints. Every object in a writable data section (.data, .bss, not .data.rel.ro) of
each OBJECT.o must be one of them, or appear in games/GAME/state_audit.txt ("<file>.<name>  why" lines: scratch
buffers, caches rebuilt the same). A static inside a function (name.N) cannot be registered: move it to the file
or list it. Needs objdump (binutils).

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def registered(binary):
    out = subprocess.run([binary, "--list"], capture_output=True, text=True, check=True).stdout
    names = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] in ("var", "ptr"):     # a pointer variable of its own counts too
            names.add(parts[1])
    return names


def allowed(game):
    path = os.path.join(ROOT, "games", game, "state_audit.txt")
    names = {}
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if line:
                    name, _, why = line.partition(" ")
                    names[name] = why.strip()
    return names


def objects(obj):
    """(name, section, size) of the data objects of an object file"""
    out = subprocess.run(["objdump", "-t", obj], capture_output=True, text=True, check=True).stdout
    found = []
    for line in out.splitlines():
        # "0000000000000000 l     O .bss	0000000000000004 st"
        parts = line.split()
        if "O" in parts[1:3] and len(parts) >= 5:
            i = parts.index("O", 1)
            if i + 3 < len(parts) and re.match(r"^[0-9a-f]+$", parts[i + 2]):
                found.append((parts[i + 3], parts[i + 1], int(parts[i + 2], 16)))
    return found


def writable(section):
    if section.startswith(".data.rel.ro") or section.startswith(".rodata"):
        return False
    return section.startswith(".data") or section.startswith(".bss") or section == "*COM*"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("binary")
    ap.add_argument("objects", nargs="+")
    a = ap.parse_args()
    reg, allow = registered(a.binary), allowed(a.game)
    missing, used_allow, count = [], set(), 0
    for obj in a.objects:
        stem = os.path.splitext(os.path.basename(obj))[0]
        for name, section, size in objects(obj):
            if not writable(section) or size == 0 or name.startswith(("__odr_asan", "__asan", "__ubsan")):
                continue
            count += 1
            key = "%s.%s" % (stem, name.split(".")[0] if re.match(r"^\w+\.\d+$", name) else name)
            if re.match(r"^\w+\.\d+$", name):
                if key in allow:
                    used_allow.add(key)
                    continue
                missing.append("%s (a static inside a function, %d bytes: move it to the file and register it, "
                               "or list it in games/%s/state_audit.txt)" % (key, size, a.game))
            elif key in reg:
                continue
            elif key in allow:
                used_allow.add(key)
            else:
                missing.append("%s (%d bytes, %s): not in the save states (rs_state_var) nor in "
                               "games/%s/state_audit.txt" % (key, size, section, a.game))
    stale = sorted(set(allow) - used_allow)
    for m in missing:
        print("state audit: %s" % m)
    for s in stale:     # a note only: the optimiser may remove an unused static (-O2 vs a sanitizer build)
        print("state audit: note: games/%s/state_audit.txt lists %s, not in these objects" % (a.game, s))
    print("state audit: %s: %d mutable objects, %d registered, %d scratch listed%s" % (
        a.game, count, len(reg), len(used_allow), ": FAILED" if missing else ", all accounted for"))
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
