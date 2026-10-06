#!/bin/sh
# RetroStone Arcade: the bundle's tests.
#   bundle_test.sh <app>_headless <work dir> "<playable game ids, in menu order>" [free|full]
#
# 1. Same game, same pictures: each playable game, started fresh in the bundle, gives exactly the pictures of its own
#    standalone build (build/host/<id>_headless, the root Makefile, built under the lock /tmp/rsvc-build.lock) for
#    the same input; again at width 520, and after ANOTHER game ran first in the same process (the module system
#    restores the game's writable data, the house kit's and the SDK's state: no leak from one game to the next).
#    Each game's width at 520 is reported: 520 (it opted in to widescreen) or 320 (pillarboxed by the frontends).
# 2. The app's flow with touches: launcher -> tap a card -> the game (taps = its pad) -> Back saves its state ->
#    another game -> Back -> the first game again: it resumes where it was (the same picture), held until a tap;
#    free: a locked card opens "Get the full version", its button asks for the Play Store page, Back closes it;
#    full: no locked card, no store request. Back in the menu quits.
# 3. Credits & licences: the "i" opens the list, a line its licence's text, a drag scrolls it, Back, Back, Back quits.
# 4. Every game launches and its saves stay its own: each game played in turn in ONE process and left with Back
#    writes its own <id>.state (the state's header names that game); a second process resumes each of them.
# 5. The touch schemes (games.cfg): a touch on the left and on the right of each game gives pad buttons; a touch-native
#    game (rs_game.touch_native) gets the touches themselves, in screen pixels.
# 6. The launcher in the 22 languages (--lang): the kit reports no layout error (text off the screen or a panel):
#    the menu, the footer, the credits list and a licence text; the free app's panel too.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -e
B=$1
OUT=$2
GAMES=$3
PROFILE=${4:-free}
FRAMES=420
mkdir -p "$OUT"
fail() { echo "FAIL: $*"; exit 1; }
png_width() { od -An -tu1 -j16 -N4 "$1" | awk '{print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4}'; }

# ---- 1. pictures identical to the standalone games ------------------------------------------------------------------
script() {   # $1: frame offset. Taps of A, Left and Right: every game's title starts with one of them
    awk -v off="$1" 'BEGIN { for (f = 40; f < 400; f += 23) { b = (f / 23) % 3 == 0 ? "LEFT" : (f / 23) % 3 == 1 ? "A" : "RIGHT";
                                                              printf "%d tap %s\n", f + off, b } }'
}
script 0 > "$OUT/input.txt"
prev=""
wide=""
narrow=""
# the standalone games: already in $OUT (PREBUILT=1: bundle.mk check builds them with the bundle's assets,
# apps/launcher/tools/game_assets.sh), else built and copied now under ONE wait for the shared lock
if [ -z "${PREBUILT:-}" ]; then
    flock /tmp/rsvc-build.lock sh -c 'out=$1; shift; for g; do
        make --no-print-directory GAME="$g" build/host/"$g"_headless > /dev/null && cp build/host/"$g"_headless "$out/$g"_standalone || exit 1
    done' sh "$OUT" $GAMES || fail "the standalone builds"
fi
for g in $GAMES; do
    [ -x "$OUT/$g"_standalone ] || fail "$g: no standalone build ($OUT/${g}_standalone)"
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
    w=$(png_width "$OUT/$g-bundle-w.png")
    if [ "$w" -gt 320 ]; then wide="$wide $g"; else narrow="$narrow $g"; fi
    # after another game (the previous one, or the launcher), in the same process
    other=${prev:-launcher}
    { echo "0 start $other fresh"; echo "300 start $g fresh"; script 300; } > "$OUT/input2.txt"
    { awk -v off=0 'BEGIN { for (f = 40; f < 280; f += 17) printf "%d tap A\n", f }'; } >> "$OUT/input2.txt"
    rm -rf "$OUT/saves"; mkdir -p "$OUT/saves"
    "$B" --saves "$OUT/saves" --frames $((FRAMES + 300)) --input "$OUT/input2.txt" \
        --shot 420:"$OUT/$g-after-a.png" --shot $((FRAMES + 299)):"$OUT/$g-after-b.png" > "$OUT/$g-after.log"
    cmp -s "$OUT/$g-a.png" "$OUT/$g-after-a.png" || fail "$g: after $other, frame 120 differs from the standalone game"
    cmp -s "$OUT/$g-b.png" "$OUT/$g-after-b.png" || fail "$g: after $other, the last frame differs from the standalone game"
    echo "ok   $g: the same pictures as the standalone game, fresh, at width 520 ($w columns) and after $other"
    prev=$g
done
echo "widescreen (520 columns):${wide:- none}"
echo "320 columns (pillarboxed on a wide screen):${narrow:- none}"

# ---- 2. the flow, with touches --------------------------------------------------------------------------------------------
set -- $GAMES
G1=$1
G2=${2:-$1}
rm -rf "$OUT/saves"; mkdir -p "$OUT/saves"
cat > "$OUT/flow.txt" <<EOF
10 tap-at 56 68          # the first card: the first game
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
400 touch 2 3              # the in-game back button (top left): the same as Back
403 up
440 tap-at 56 68         # the first game again: resumed, held
446 log                  # (a card launches when the finger is lifted: 443)
450 touch 50 50          # the tap that wakes it (not passed to the game)
455 up
470 touch 50 50
473 up
540 back
EOF
if [ "$PROFILE" = free ]; then
    cat >> "$OUT/flow.txt" <<EOF
580 tap-at 56 156        # a locked card (second row): the panel
620 tap-at 160 212       # its big GET IT ON GOOGLE PLAY button
700 back                 # closes the panel
720 back                 # leaves the app
EOF
else
    echo "720 back                 # leaves the app" >> "$OUT/flow.txt"
fi
"$B" --saves "$OUT/saves" --frames 800 --input "$OUT/flow.txt" --shot 600:"$OUT/flow-panel.png" \
    --shot 230:"$OUT/flow-launcher.png" > "$OUT/flow.log" 2>&1 || fail "the flow run failed"
grep -q "start: $G1" "$OUT/flow.log" || fail "the first card did not start $G1"
grep -q "start: $G2" "$OUT/flow.log" || fail "the second card did not start $G2"
grep -q "resumed $G1" "$OUT/flow.log" || fail "$G1 did not resume from its state"
h1=$(grep "^frame 201:" "$OUT/flow.log" | awk '{print $5}')
h2=$(grep "^frame 446:" "$OUT/flow.log" | awk '{print $5}')
[ -n "$h1" ] && [ "$h1" = "$h2" ] || fail "the resumed picture ($h2) is not the one left ($h1)"
if [ "$PROFILE" = free ]; then
    grep -q "open market://details?id=" "$OUT/flow.log" || fail "the panel's button did not ask for the store"
else
    grep -q "open " "$OUT/flow.log" && fail "the full app asked for a store page"
fi
grep -q "^quit" "$OUT/flow.log" || fail "Back in the launcher did not quit"
[ -f "$OUT/saves/$G1.state" ] && [ -f "$OUT/saves/$G1.srm" ] || fail "no $G1.state / $G1.srm"
echo "ok   flow: launch by touch, Back saves, resume where it was (picture $h1), $([ "$PROFILE" = free ] && echo "the store button" || echo "no store"), Back quits"

# ---- 3. credits & licences ------------------------------------------------------------------------------------------------
cat > "$OUT/credits.txt" <<EOF
10 tap-at 12 14          # the "i" in the header's top-left corner
40 log
50 tap-at 160 108        # the third line (SDL): its licence
80 log
90 touch 160 180         # a drag up: the text scrolls
94 move 160 120
98 move 160 60
100 up
140 log
150 back                 # the list again
170 log
180 back                 # the menu
200 log
210 back                 # leaves the app
EOF
"$B" --saves "$OUT/saves" --frames 260 --input "$OUT/credits.txt" --shot 40:"$OUT/credits-list.png" \
    --shot 80:"$OUT/credits-sdl.png" --shot 140:"$OUT/credits-scrolled.png" > "$OUT/credits.log" 2>&1 \
    || fail "the credits run failed"
c() { grep "^frame $1:" "$OUT/credits.log" | awk '{print $5}'; }
[ "$(c 40)" != "$(c 80)" ] && [ "$(c 80)" != "$(c 140)" ] && [ "$(c 140)" != "$(c 170)" ] && [ "$(c 170)" != "$(c 200)" ] \
    || fail "credits: the list, the licence text, its scrolling and the way back do not all show different pictures"
grep "^start:" "$OUT/credits.log" | grep -qv "start: launcher" && fail "credits: a game started"
grep -q "^quit" "$OUT/credits.log" || fail "credits: Back, Back, Back did not leave the app"
echo "ok   credits: the list, a licence's full text, scrolling, Back to the list, the menu, quit"

# ---- 4. every game launches, its saves are its own -----------------------------------------------------------------------
rm -rf "$OUT/saves4"; mkdir -p "$OUT/saves4"
f=10
: > "$OUT/all.txt"
: > "$OUT/all2.txt"
for g in $GAMES; do
    { echo "$f start $g"; echo "$((f + 30)) tap A"; echo "$((f + 60)) tap A"; echo "$((f + 61)) tap LEFT";
      echo "$((f + 90)) tap RIGHT"; echo "$((f + 120)) back"; } >> "$OUT/all.txt"
    { echo "$f start $g"; echo "$((f + 40)) back"; } >> "$OUT/all2.txt"
    f=$((f + 150))
done
"$B" --saves "$OUT/saves4" --frames $((f + 20)) --input "$OUT/all.txt" > "$OUT/all.log" 2>&1 || fail "the all-games run failed"
ids=""
for g in $GAMES; do
    grep -q "^start: $g\$" "$OUT/all.log" || fail "$g did not start"
    [ -s "$OUT/saves4/$g.state" ] || fail "$g: no $g.state after Back"
    id=$(dd if="$OUT/saves4/$g.state" bs=1 skip=8 count=20 2>/dev/null | tr -d '\000')
    [ -n "$id" ] || fail "$g.state has no game id"
    case " $ids " in *" $id "*) fail "$g.state names $id, like another game's state";; esac
    ids="$ids $id"
done
"$B" --saves "$OUT/saves4" --frames $((f + 20)) --input "$OUT/all2.txt" > "$OUT/all2.log" 2>&1 || fail "the resume run failed"
for g in $GAMES; do grep -q "resumed $g " "$OUT/all2.log" || fail "$g did not resume its own state"; done
echo "ok   $(echo $GAMES | wc -w) games launch in one process, each writes and resumes its own state ($(ls "$OUT/saves4" | wc -l) save files)"

# ---- 5. the touch schemes -------------------------------------------------------------------------------------------------
for g in $GAMES; do
    printf '10 touch 10 80\n20 up\n30 touch 90 50\n40 up\n50 touch 50 15\n60 up\n' > "$OUT/touch.txt"
    "$B" --saves "$OUT/saves4" --start "$g" --fresh --touch-log --frames 80 --input "$OUT/touch.txt" > "$OUT/touch-$g.log" 2>&1
    t=$(grep "^touch:" "$OUT/touch-$g.log" | grep -v "touch: -" | grep -v "touch: native -" | sed 's/touch: //' | tr '\n' '/' | sed 's#/$##')
    [ -n "$t" ] || fail "$g: the touches gave no button"
    if grep -q "^touch: native" "$OUT/touch-$g.log"; then
        # a touch-native game: the fingers that go down on the pause or the back button never reach it; a finger
        # that went down on its picture stays its touch when it slides over the pause button
        printf '10 touch 95 5\n20 up\n30 touch 60 30\n34 move 95 5\n40 up\n60 touch 3 5\n65 up\n' > "$OUT/touch2.txt"
        "$B" --saves "$OUT/saves4" --start "$g" --fresh --touch-log --frames 80 --input "$OUT/touch2.txt" \
            > "$OUT/touch2-$g.log" 2>&1
        sed -n '/^touch: native 1/q;p' "$OUT/touch2-$g.log" | grep -q "^touch: [A-Z]" \
            || fail "$g: the pause button pressed nothing (or its finger reached the game)"
        [ "$(grep -c "^touch: native 1" "$OUT/touch2-$g.log")" = 1 ] \
            || fail "$g: the pause or back button's finger reached the game, or the game's finger was lost"
        [ "$(grep -c "^start: launcher" "$OUT/touch2-$g.log")" = 2 ] || fail "$g: the back button did not go back"
        t="$t; the pause and back buttons keep their fingers"
    fi
    echo "ok   touch $g: $t"
done

# ---- 6. the launcher in the 22 languages: no layout error from the kit ("i18n: ..."): the dialogs (touch only, no pad
# hints), then the menu, the footer and the credits with a gamepad (its hints) -------------------------------------------
{
    [ "$PROFILE" = free ] && printf '10 tap-at 56 156\n50 tap-at 12 14\n'   # a locked card: the panel; its back arrow
    cat <<EOT
70 tap-at 308 14         # the "?": the help
110 tap-at 12 14         # closed by its back arrow
130 tap-at 12 14         # the "i": the credits
170 tap-at 160 108       # a licence
200 back
210 back
220 P1 DOWN              # a gamepad: the footer shows the A glyph and the selected game
222 P1 -
240 P1 UP                # up: the corner buttons (the footer names them)
242 P1 -
244 P1 UP
246 P1 -
248 P1 UP
250 P1 -
270 P1 A                 # the "i" or the "?" with the pad's hints
272 P1 -
EOT
    [ "$PROFILE" = free ] && printf '300 P1 B\n302 P1 -\n320 tap-at 56 156\n'  # the panel with the pad's hints
} > "$OUT/lang.txt"
for l in en cs da de el es fi fr id it ja ko nb nl pl pt_BR pt_PT ru sv tr uk zh_CN; do
    "$B" --saves "$OUT/saves" --lang $l --frames 360 --input "$OUT/lang.txt" --shot 40:"$OUT/panel-$l.png" \
        --shot 100:"$OUT/help-$l.png" --shot 160:"$OUT/credits-$l.png" --shot 195:"$OUT/licence-$l.png" \
        --shot 260:"$OUT/menu-pad-$l.png" --shot 290:"$OUT/corner-pad-$l.png" --shot 359:"$OUT/panel-pad-$l.png" \
        > "$OUT/lang-$l.log" 2>&1 || fail "the launcher in $l"
    grep -q "i18n:" "$OUT/lang-$l.log" && fail "$l: $(grep -m1 "i18n:" "$OUT/lang-$l.log")"
done
echo "ok   the launcher in 22 languages (the menu, the footer, the help, the credits$([ "$PROFILE" = free ] && echo ", the panel"), with and without a gamepad): no layout error"
echo "bundle tests passed ($PROFILE)"
