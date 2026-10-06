#!/bin/sh
# Blueberry Tumble widescreen (DESIGN.md "Widescreen"): the picture is as wide as asked, the world is revealed wider
# (the left share of the extra columns behind the berry, the rest ahead) and the GAMEPLAY IS THE SAME at every width: every width starts exactly like 320 (the first pattern at the same
# distance, on the same beat), and with the same seed the bot (it plays from the screen) has the same events, pattern
# instances, splats, metres and score on the same frames; a live change of width in a run changes nothing of it; a
# save state made at one width loads at another.
#   wide_test.sh <blueberrytumble_headless> <blueberrytumble_test_states> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
H=$1
T=$2
O=${3:-/tmp}/blueberrytumble-wide
WS=${4:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
png_w() { od -An -tu1 -j16 -N4 "$1" | awk '{print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4}'; }
run() { $H --opt music=0 --opt sound=0 "$@" 2>&1; }

# ---- the picture's width -----------------------------------------------------------------------------------------------
for w in 320 $WS 560; do
    run --width "$w" --frames 400 --opt bot=1 --opt seed=7 --opt ready=1 --png "$O/w$w.png" > "$O/w$w.log"
    pw=$(png_w "$O/w$w.png")
    if [ "$pw" = "$w" ]; then ok "--width $w: a ${pw}x240 picture"; else ko "--width $w: the picture is $pw wide"; fi
done

# ---- the left share (WIDE_LEFT_SHARE_NUM / _DEN, src/tuning.h): a share of the extra columns is shown on the left,
# behind the berry, the rest ahead. The 320 picture's left part (the berry, the slope under it, behind it and just
# ahead) is found in the wide picture of the same frame at that column, not at 0: the berry at screen x 96 + share --
TUN=$(dirname "$0")/../src/tuning.h
num=$(sed -n 's/^#define WIDE_LEFT_SHARE_NUM *\([0-9]*\).*/\1/p' "$TUN")
den=$(sed -n 's/^#define WIDE_LEFT_SHARE_DEN *\([0-9]*\).*/\1/p' "$TUN")
for w in 320 $WS 560; do
    run --width "$w" --frames 301 --opt bot=1 --opt seed=7 --opt ready=1 --shot 300:"$O/share$w.png" > /dev/null
done
for w in $WS 560; do
    want=$(( ((w - 320) * num + den / 2) / den ))
    got=$(python3 - "$O/share320.png" "$O/share$w.png" <<'EOF'
import struct, sys, zlib
def load(path):
    d = open(path, 'rb').read(); i = 8; idat = b''
    while i < len(d):
        n, t = struct.unpack('>I', d[i:i + 4])[0], d[i + 4:i + 8]
        if t == b'IHDR': w, h, bd, ct = struct.unpack('>IIBB', d[i + 8:i + 18])
        elif t == b'IDAT': idat += d[i + 8:i + 8 + n]
        i += 12 + n
    bpp = {2: 3, 6: 4}[ct]; raw = zlib.decompress(idat); st = w * bpp; prev = bytearray(st); rows = []; p = 0
    for y in range(h):
        f = raw[p]; ln = bytearray(raw[p + 1:p + 1 + st]); p += 1 + st
        for x in range(st):
            a = ln[x - bpp] if x >= bpp else 0; b = prev[x]; c = prev[x - bpp] if x >= bpp else 0
            if f == 1: ln[x] = (ln[x] + a) & 255
            elif f == 2: ln[x] = (ln[x] + b) & 255
            elif f == 3: ln[x] = (ln[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                ln[x] = (ln[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([bytes(ln[x * bpp:x * bpp + bpp]) for x in range(w)]); prev = ln
    return w, rows
w0, a = load(sys.argv[1]); w1, b = load(sys.argv[2])
best, bs = -1, -1
for s in range(w1 - w0 + 1):
    n = sum(1 for y in range(100, 240) for x in range(0, 200) if a[y][x] == b[y][x + s])
    if n > best: best, bs = n, s
print(bs, best * 100 // (140 * 200))
EOF
)
    s=${got% *}; pc=${got#* }
    if [ "$s" = "$want" ] && [ "${pc:-0}" -ge 90 ]; then
        ok "the left share at $w: the 320 view at column $s ($num/$den of $((w - 320)), $pc% the same): the berry at screen x $((96 + s))"
    else ko "the left share at $w: the 320 view found at column '$s' ($pc% the same), $want wanted"; fi
done

# ---- the gameplay at every width ------------------------------------------------------------------------------------------
# events <log>: "frame what": the bot's events (with the metres), the splats, the end of the run (metres, score)
events() {
    sed -n -e 's/.*ev \(p[0-9]\) frame \([0-9]*\) \(.*\)$/\2 \1 \3/p' \
           -e 's/.*player \([0-9]\) splat at \([0-9]*\) m (frame \([0-9]*\), \(.*\))$/\3 splat p\1 at \2 m, \4/p' \
           -e 's/.*run 1 over at frame \([0-9]*\): \(.*\)$/\1 over: \2/p' "$1"
}
# same <name> <frames> <opts...>: the run at 320 and at each width, frame for frame (no offset: every width starts like
# 320, the course clock from 0): the same events, the same pattern instances (each logged with the frame player 1
# enters it and its column: the first pattern on the same beat at the same distance), the same final state and hash
same() {
    name=$1; n=$2; shift 2
    run --width 320 --frames "$n" --opt evlog=1 --opt dump=1 "$@" > "$O/$name-320.log"
    events "$O/$name-320.log" > "$O/$name-320.ev"
    grep -o "seg .*" "$O/$name-320.log" > "$O/$name-320.seg"
    grep -o "state: .*" "$O/$name-320.log" > "$O/$name-320.st"
    first=$(grep -v "seg \(start\|bridge\|gate\) " "$O/$name-320.seg" | head -1)
    ne=$(wc -l < "$O/$name-320.ev")
    [ "$ne" -ge 3 ] && [ -n "$first" ] && [ -s "$O/$name-320.st" ] || ko "$name: only $ne events at 320 ('$first')"
    for w in $WS 560; do
        run --width "$w" --frames "$n" --opt evlog=1 --opt dump=1 "$@" > "$O/$name-$w.log"
        events "$O/$name-$w.log" > "$O/$name-$w.ev"
        grep -o "seg .*" "$O/$name-$w.log" > "$O/$name-$w.seg"
        grep -o "state: .*" "$O/$name-$w.log" > "$O/$name-$w.st"
        firstw=$(grep -v "seg \(start\|bridge\|gate\) " "$O/$name-$w.seg" | head -1)
        if [ "$firstw" = "$first" ] && cmp -s "$O/$name-320.ev" "$O/$name-$w.ev" && cmp -s "$O/$name-320.seg" "$O/$name-$w.seg" &&
           cmp -s "$O/$name-320.st" "$O/$name-$w.st"; then
            ok "$name at $w: offset 0, the first pattern as at 320 ($first), the same $ne events, $(wc -l < "$O/$name-320.seg") pattern instances and end state"
        else
            ko "$name at $w: first pattern '$firstw' (320: '$first'); events, patterns or state differ: $(diff "$O/$name-320.ev" "$O/$name-$w.ev" | head -2 | tr '
' ' ')"
        fi
    done
}
same solo 9000 --opt bot=1 --opt ready=1 --opt seed=11
same gold 9000 --opt bot=1 --opt ready=1 --opt seed=2
same title 3000 --opt bot=1 --opt seed=4
same race2 3000 --opt bot=2 --opt players=2 --opt ready=1 --opt seed=6
same race4 1500 --opt bot=4 --opt players=4 --opt ready=1 --opt seed=9 --opt botstop=60

# ---- a live change of width (the desktop runner's resized window: a state, a restart at the new width, the state
# loaded): in a run, it goes on exactly as it would have (the same events, frame for frame); on the title too ------------------------------------------------------------
run --frames 2400 --opt evlog=1 --opt dump=1 --opt bot=1 --opt seed=7 --opt ready=1 > "$O/plain.log"
run --frames 2400 --opt evlog=1 --opt dump=1 --opt bot=1 --opt seed=7 --opt ready=1 --resize 400:520 --resize 650:320 \
    --resize 900:427 --resize 1200:560 --shot 500:"$O/resize-520.png" --shot 800:"$O/resize-320.png" > "$O/resized.log"
events "$O/plain.log" > "$O/plain.ev"
events "$O/resized.log" > "$O/resized.ev"
if [ "$(png_w "$O/resize-520.png")" = 520 ] && [ "$(png_w "$O/resize-320.png")" = 320 ] &&
   grep -q "frame 400: width 520" "$O/resized.log" && grep -q "frame 1200: width 560" "$O/resized.log" &&
   [ -s "$O/plain.ev" ] && cmp -s "$O/plain.ev" "$O/resized.ev" &&
   [ "$(grep 'state:' "$O/plain.log")" = "$(grep 'state:' "$O/resized.log" | tail -1)" ]; then
    ok "a live change of width in a run (320 -> 520 -> 320 -> 427 -> 560): it goes on, the same $(wc -l < "$O/plain.ev") events"
else ko "a live change of width in a run: $(grep 'width\|state:' "$O/resized.log" | tr '\n' ' ')"; fi
run --width 520 --frames 3000 --opt evlog=1 --opt bot=1 --opt seed=4 > "$O/lobby520.log"
run --width 320 --frames 3000 --opt evlog=1 --opt bot=1 --opt seed=4 --resize 10:520 > "$O/lobby.log"
events "$O/lobby520.log" > "$O/lobby520.ev"
events "$O/lobby.log" > "$O/lobby.ev"
if [ -s "$O/lobby.ev" ] && cmp -s "$O/lobby520.ev" "$O/lobby.ev"; then ok "a live change of width on the title: the run is the new width's"
else ko "a live change of width on the title (320 -> 520): its run differs from a 520 run"; fi

# ---- a state saved at one width, loaded at another (a fresh process, before its first frame): it loads and plays on ----
scenario() {   # scenario <name> <save width> <frames> <after> <opts...>
    name=$1; sw=$2; n=$3; m=$4; shift 4
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/$name.input" ] || : > "$O/$name.input"
    out=$($T --width "$sw" $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/bt-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then ok "$name: saved at width $sw (frame $n), replayed the same"
    else ko "$name at width $sw:"; echo "$out" | grep "FAIL\|first"; fi
    for lw in 320 $WS 560; do
        [ "$lw" = "$sw" ] && continue
        out=$($T --width "$lw" $opts --input "$O/$name.input" --resume "$O/bt-$name" --any-width 2>&1)
        if echo "$out" | grep -q "all passed"; then ok "$name: the width-$sw state loads at width $lw and plays $m frames"
        else ko "$name: width $sw -> $lw:"; echo "$out" | grep "FAIL"; fi
    done
}
scenario run 427 1500 300 bot=1 ready=1 seed=21
scenario start 520 60 300 bot=1 ready=1 seed=21
scenario title 320 60 200
scenario lobby 560 25 200 players=3
printf "400 tap SELECT\n" > "$O/paused.input"
scenario paused 520 450 120 bot=1 ready=1
scenario race4 320 600 300 bot=4 players=4 ready=1 seed=9
scenario results4 427 760 300 bot=4 players=4 ready=1 seed=9 botstop=60 botruns=2
exit $fail
