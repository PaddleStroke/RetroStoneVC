#!/usr/bin/env python3
"""Check results geometry, readable scores, slide backgrounds and return to title.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
"""
import re
import subprocess
import sys
from pathlib import Path
from PIL import Image

binary = sys.argv[1]
out = Path(sys.argv[2] if len(sys.argv) > 2 else 'build/pogomamie-ui')
out.mkdir(parents=True, exist_ok=True)

def run(args):
    result = subprocess.run([binary] + args, capture_output=True, text=True, check=True)
    return result.stdout + result.stderr

def count(im, box, colour, tolerance=12):
    return sum(all(abs(a - b) <= tolerance for a, b in zip(pixel, colour))
               for pixel in im.crop(box).getdata())

def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print('  ok   ' + message, flush=True)

navy = (42, 68, 118)
for players in range(1, 5):
    opts = ['--opt', 'music=0', '--opt', 'seed=5', '--opt', 'ready=1', '--opt', 'botstop=3',
            '--opt', f'players={players}', '--opt', f'bot={players}', '--opt', 'strict=1']
    log = run(opts + ['--frames', '1800'])
    check('rs strict' not in log, f'{players}P: within hardware limits')
    match = re.search(r'run 1 over at frame (\d+)', log)
    check(match is not None, f'{players}P: reached game over')
    frame = int(match[1])
    settled = out / f'results-{players}.png'
    sliding = out / f'results-{players}-slide.png'
    run(opts + ['--frames', str(frame + 65), '--shot', f'{frame + 62}:{settled}',
                '--shot', f'{frame + 10}:{sliding}'])
    im = Image.open(settled).convert('RGB')
    if players == 1:
        check(count(im, (82, 74, 238, 166), navy) > 10000,
              '1P: single-player score panel remains intact')
        check(count(im, (80, 36, 240, 60), (250, 210, 90)) > 80,
              '1P: GAME OVER banner remains intact')
        row = 19
    else:
        check(count(im, (80, 56, 240, 64), navy) < 5,
              f'{players}P: clear gap under the winner banner')
        bottom = (8 + 3 * players + 4) * 8
        check(count(im, (66, 66, 254, bottom - 2), navy) > 6000,
              f'{players}P: ranking panel is visible')
        if players == 2:
            check(count(im, (80, 146, 240, 166), navy) < 5,
                  '2P: no single-player dialog below the ranking panel')
        for place in range(players):
            y = (10 + 3 * place) * 8
            check(count(im, (168, y - 4, 212, y + 12), (255, 255, 255)) > 15,
                  f'{players}P: score {place + 1} is readable')
        row = 10 + 3 * players
    check(count(im, (100, row * 8, 220, row * 8 + 8), (255, 246, 220), 16) > 35,
          f'{players}P: menu prompt is below the scores')
    slide = Image.open(sliding).convert('RGB')
    check(count(slide, (0, 0, 320, 20), navy) < 5,
          f'{players}P: background remains during the slide')
    inputs = out / f'return-{players}.input'
    inputs.write_text(f'{frame + 100} tap A\n')
    log = run(opts + ['--frames', str(frame + 105), '--opt', 'dump=1', '--input', str(inputs)])
    check(re.search(r'state: st=0 players=' + str(players) + r'\b', log) is not None,
          f'{players}P: A returns to the title with players retained')
