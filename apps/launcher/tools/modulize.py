#!/usr/bin/env python3
"""Link a RetroStone VC game into a MODULE: one relocatable object that many games can share a program with.

    modulize.py --id ID [--house] --flavor elf|coff --cc CC --cflags "..." --ld LD --objcopy OBJCOPY
                --objdump OBJDUMP --nm NM --out OUT.o OBJ...

Every game is a standalone program: its own rs_game_main(), and globals that other games also define (opt_bot,
draw_init...). A module makes them private, and lets the runtime restart the game from scratch:
  1. its objects' writable data sections (.data, .data.rel, .data.rel.local) are renamed into one section, and
     .bss into another (ELF: .data.rsgame / .bss.rsgame; COFF: .data$rsg_<id>_1 / .bss$rsg_<id>_1);
  2. ld -r links apps/launcher/src/rsb_glue.c (begin markers), the objects and the glue again (end markers and
     the descriptor rsb_module_<id>) into one object: the markers bound the game's writable data;
  3. objcopy --keep-global-symbol=rsb_module_<id> makes every other symbol it defines local (its references to the
     SDK, the house kit and libc stay undefined: they are linked once, shared by all the games);
  4. checks: no writable data left outside the renamed sections, one global symbol.
The game's translated text registers its pack from a constructor (rs_i18n_register): the call is renamed to
rsb_glue_i18n_register (rsb_glue.c), so the runtime keeps every game's pack and installs the right one at its start.
--house: the house kit (games/common): steps 1, 2 and 4 only, its functions stay global (the games call them).
The runtime (rsb_runtime.c) copies the data at start-up and restores it, and clears the bss, at each start.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GLUE = os.path.join(HERE, "..", "src", "rsb_glue.c")


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(" ".join(shlex.quote(c) for c in cmd) + "\n" + r.stdout + r.stderr)
        raise SystemExit(1)
    return r.stdout


def sections(objdump, path):
    """(name, size) of every section (objdump -h, GNU and LLVM formats)."""
    out = []
    for line in run([objdump, "-h", path]).splitlines():
        t = line.split()
        if len(t) >= 3 and t[0].isdigit():
            try:
                out.append((t[1], int(t[2], 16)))
            except ValueError:
                pass
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--id", required=True)
    ap.add_argument("--house", action="store_true")
    ap.add_argument("--flavor", choices=("elf", "coff"), required=True)
    ap.add_argument("--cc", required=True)
    ap.add_argument("--cflags", default="")
    ap.add_argument("--ld", required=True)
    ap.add_argument("--objcopy", required=True)
    ap.add_argument("--objdump", required=True)
    ap.add_argument("--nm", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("objs", nargs="+")
    a = ap.parse_args()

    gid = a.id
    if a.flavor == "elf":
        dsec, bsec = ".data.rsgame", ".bss.rsgame"
    else:
        dsec, bsec = f".data$rsg_{gid}_1", f".bss$rsg_{gid}_1"
    work = a.out + ".work"
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    cc = shlex.split(a.cc) + shlex.split(a.cflags)
    game = "0" if a.house else "1"
    glue = []
    for n in (1, 2):
        o = os.path.join(work, f"glue{n}.o")
        run(cc + [f"-DRSB_GLUE={n}", f"-DRSB_ID={gid}", f"-DRSB_GAME={game}", "-c", GLUE, "-o", o])
        glue.append(o)
    objs = []
    for i, src in enumerate(a.objs):
        o = os.path.join(work, f"{i:03d}-{os.path.basename(src)}")
        cmd = [a.objcopy]
        for s in (".data", ".data.rel", ".data.rel.local"):
            cmd += ["--rename-section", f"{s}={dsec}"]
        if not a.house:     # the translated text's constructor: the runtime keeps each game's pack (rsb_glue.c)
            cmd += ["--redefine-sym", "rs_i18n_register=rsb_glue_i18n_register"]
        cmd += ["--rename-section", f".bss={bsec}", src, o]
        run(cmd)
        objs.append(o)
    linked = os.path.join(work, "linked.o")
    run([a.ld, "-r", "-o", linked, glue[0]] + objs + [glue[1]])
    if a.house:
        cmd = [a.objcopy]
        for s in ("rsb_glue_data_begin", "rsb_glue_data_end", "rsb_glue_bss_begin", "rsb_glue_bss_end"):
            cmd += ["--localize-symbol", s]
        run(cmd + [linked, a.out])
    else:
        run([a.objcopy, f"--keep-global-symbol=rsb_module_{gid}", linked, a.out])

    # checks
    bad = []
    for name, size in sections(a.objdump, a.out):
        if size == 0 or name in (dsec, bsec):
            continue
        if a.flavor == "elf":
            if name in (".data", ".bss", ".sdata", ".sbss", ".tdata", ".tbss") or \
               (name.startswith((".data.", ".bss.")) and not name.startswith(".data.rel.ro")):
                bad.append(name)
        else:
            if name in (".data", ".bss", ".tls") or name.startswith((".data$", ".bss$")) and \
               not name.startswith((f".data$rsg_{gid}_", f".bss$rsg_{gid}_")):
                bad.append(name)
    if bad:
        raise SystemExit(f"modulize {gid}: writable data outside the module's sections: {', '.join(bad)}")
    if not a.house:
        glob = [l.split()[-1] for l in run([a.nm, "-g", "--defined-only", a.out]).splitlines() if l.strip()]
        if glob != [f"rsb_module_{gid}"]:
            raise SystemExit(f"modulize {gid}: global symbols left: {glob}")
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
