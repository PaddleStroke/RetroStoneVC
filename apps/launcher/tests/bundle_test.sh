#!/bin/sh
# RetroStone Arcade: the bundle's tests.
#   bundle_test.sh <app>_headless <work dir> "<free game ids>"
#
# 1. Same game, same pictures: each free game, started fresh in the bundle, gives exactly the pictures of its own
#    standalone build (build/host/<id>_headless, the root Makefile, built under the lock /tmp/rsvc-build.lock) for
#    the same input; again after ANOTHER game ran first in the same process (the module system restores the
#    game's writable data, the house kit's and the SDK's state: no leak from one game to the next).
# 2. The app's flow with touches: launcher -> tap a card -> the game (taps = its pad) -> Back saves its state ->
#    another game -> Back -> the first game again: it resumes where it was (the same picture), held until a tap;
#    a locked card opens "Get the full version", its button asks for the Play Store page, Back closes it, Back quits.
# 3. The launcher in the 22 languages (--lang): the kit reports no layout error (text off the screen or a panel).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -e
B=$1
OUT=$2
GAMES=$3
FRAMES=420
mkdir -p "$OUT"
fail() { echo "FAIL: $*"; exit 1; }

# ---- 1. pictures identical to the standalone games ------------------------------------------------------------------
script() {   # $1: frame offset. Taps of A, Left and Right: every game's title starts with one of them
    awk -v off="$1" 'BEGIN { for (f = 40; f < 400; f += 23) { b = (f / 23) % 3 == 0 ? "LEFT" : (f / 23) % 3 == 1 ? "A" : "RIGHT";
                                                              printf "%d tap %s\n", f + off, b } }'
}
script 0 > "$OUT/input.txt"
prev=""
for g in $GAMES; do
    flock /tmp/rsvc-build.lock make --no-print-directory GAME="$g" build/host/"$g"_headless > /dev/null
    flock /tmp/rsvc-build.lock cp build/host/"$g"_headless "$OUT/$g"_standalone
    "$OUT/$g"_standalone --frames $FRAMES --input "$OUT/input.txt" --shot 120:"$OUT/$g-a.png" \
        --shot $((FRAMES - 1)):"$OUT/$g-b.png" > /dev/null
    rm -rf "$OUT/saves"; mkdir -p "$OUT/saves"
    "$B" --saves "$OUT/saves" --start "$g" --fresh --frames $FRAMES --input "$OUT/input.txt" \
        --shot 120:"$OUT/$g-bundle-a.png" --shot $((FRAMES - 1)):"$OUT/$g-bundle-b.png" > "$OUT/$g-bundle.log"
    cmp -s "$OUT/$g-a.png" "$OUT/$g-bundle-a.png" || fail "$g: frame 120 differs from the standalone game"
    cmp -s "$OUT/$g-b.png" "$OUT/$g-bundle-b.png" || fail "$g: frame $((FRAMES - 1)) differs from the standalone game"
    # widescreen: at 520 columns (a game that has not opted in runs at 320 in both)
    "$OUT/$g"_standalone --width 520 --frames $FRAMES --input "$OUT/input.txt" --shot $((FRAMES - 1)):"$OUT/$g-w.png" > /dev/null
    "$B" --saves "$OUT/saves" --width 520 --start "$g" --fresh --frames $FRAMES --input "$OUT/input.txt" --shot $((FRAMES - 1)):"$OUT/$g-bundle-w.png" > /dev/null
    cmp -s "$OUT/$g-w.png" "$OUT/$g-bundle-w.png" || fail "$g: at width 520, the last frame differs from the standalone game"
    # after another game (the previous one, or the launcher), in the same process
    other=${prev:-launcher}
    { echo "0 start $other fresh"; echo "300 start $g fresh"; script 300; } > "$OUT/input2.txt"
    { awk -v off=0 'BEGIN { for (f = 40; f < 280; f += 17) printf "%d tap A\n", f }'; } >> "$OUT/input2.txt"
    rm -rf "$OUT/saves"; mkdir -p "$OUT/saves"
    "$B" --saves "$OUT/saves" --frames $((FRAMES + 300)) --input "$OUT/input2.txt" \
        --shot 420:"$OUT/$g-after-a.png" --shot $((FRAMES + 299)):"$OUT/$g-after-b.png" > "$OUT/$g-after.log"
    cmp -s "$OUT/$g-a.png" "$OUT/$g-after-a.png" || fail "$g: after $other, frame 120 differs from the standalone game"
    cmp -s "$OUT/$g-b.png" "$OUT/$g-after-b.png" || fail "$g: after $other, the last frame differs from the standalone game"
    echo "ok   $g: the same pictures as the standalone game, fresh, at width 520 and after $other"
    prev=$g
done

# ---- 2. the flow, with touches --------------------------------------------------------------------------------------------
set -- $GAMES
G1=$1
G2=${2:-$1}
rm -rf "$OUT/saves"; mkdir -p "$OUT/saves"
cat > "$OUT/flow.txt" <<EOF
10 tap-at 56 68          # the first card: the first free game
60 touch 50 50           # in the game: a tap anywhere
63 up
100 touch 30 60
103 up
140 touch 70 40
143 up
201 log
201 back                 # Back: its state is saved, the launcher
240 tap-at 160 68        # the second card
300 touch 50 50
303 up
400 back
440 tap-at 56 68         # the first game again: resumed, held
446 log                  # (a card launches when the finger is lifted: 443)
450 touch 50 50          # the tap that wakes it (not passed to the game)
455 up
470 touch 50 50
473 up
540 back
580 tap-at 56 156        # a locked card (second row): the panel
620 tap-at 100 212       # its GET IT ON GOOGLE PLAY button
700 back                 # closes the panel
720 back                 # leaves the app
EOF
"$B" --saves "$OUT/saves" --frames 800 --input "$OUT/flow.txt" --shot 600:"$OUT/flow-panel.png" \
    --shot 230:"$OUT/flow-launcher.png" > "$OUT/flow.log" 2>&1 || fail "the flow run failed"
grep -q "start: $G1" "$OUT/flow.log" || fail "the first card did not start $G1"
grep -q "start: $G2" "$OUT/flow.log" || fail "the second card did not start $G2"
grep -q "resumed $G1" "$OUT/flow.log" || fail "$G1 did not resume from its state"
h1=$(grep "^frame 201:" "$OUT/flow.log" | awk '{print $5}')
h2=$(grep "^frame 446:" "$OUT/flow.log" | awk '{print $5}')
[ -n "$h1" ] && [ "$h1" = "$h2" ] || fail "the resumed picture ($h2) is not the one left ($h1)"
grep -q "open market://details?id=" "$OUT/flow.log" || fail "the panel's button did not ask for the store"
grep -q "^quit" "$OUT/flow.log" || fail "Back in the launcher did not quit"
[ -f "$OUT/saves/$G1.state" ] && [ -f "$OUT/saves/$G1.srm" ] || fail "no $G1.state / $G1.srm"
echo "ok   flow: launch by touch, Back saves, resume where it was (picture $h1), the store button, Back quits"
# ---- 3. the launcher in the 22 languages: no layout error from the kit ("i18n: ..."), the panel and the footer --------
cat > "$OUT/lang.txt" <<EOT
10 tap-at 56 156         # a locked card: the panel (its title, text and buttons)
80 back
90 P1 DOWN               # down twice: the footer's "full version" banner selected (the N GAMES line)
92 P1 -
94 P1 DOWN
96 P1 -
EOT
for l in en cs da de el es fi fr id it ja ko nb nl pl pt_BR pt_PT ru sv tr uk zh_CN; do
    "$B" --saves "$OUT/saves" --lang $l --frames 120 --input "$OUT/lang.txt" --shot 60:"$OUT/panel-$l.png" \
        --shot 119:"$OUT/menu-$l.png" > "$OUT/lang-$l.log" 2>&1 || fail "the launcher in $l"
    grep -q "i18n:" "$OUT/lang-$l.log" && fail "$l: $(grep -m1 "i18n:" "$OUT/lang-$l.log")"
done
echo "ok   the launcher in 22 languages (the panel, the footer): no layout error"
echo "bundle tests passed"
