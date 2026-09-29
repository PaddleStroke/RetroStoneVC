#!/usr/bin/env python3
"""The owner's art review tool: a tiny local web server (Python standard library;
the image processing is tools/art_consistency.py).

    python3 tools/art_review.py [--port 8765] [--host 127.0.0.1] [--char-size 24]
                                [--incoming games/bombermole/art/incoming]

then open http://localhost:8765 (from Windows too, when it runs in WSL).
Run it from the checkout whose TODO.md is live (the one the image agent
writes: C:\\Users\\Pierre\\Desktop\\RetroStoneVC), or pass --incoming.

Per TODO row it shows the original AI strip, the processed frames at 1x and
4x, animations at game size and 4x, the sprite on a real level screenshot,
the consistency flags, and Validate / Reject (with a note) / Reset buttons.
Filters per family and status, a family view (all animations of one character
side by side) and "validate all unflagged in this family".

TODO.md safety: a button writes ONLY that row's Status and Notes cells. The
file is re-read right before every write; if it changed between the read and
the write (the image agent saving at the same moment), the edit is re-applied
on the new content, so nobody's edit is lost. The write is atomic (temporary
file + rename) and the previous version is kept in build/art-review/backups/.
The image agent's rules still hold: only the owner validates.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import datetime
import http.server
import json
import os
import re
import shutil
import sys
import threading
import time
import traceback
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

STATUSES = ("TODO", "GENERATED", "VALIDATED", "REJECTED")
ROW_RE = re.compile(r"^\|\s*([a-z0-9_]+)\s*\|\s*([A-Z]+)\s*\|(.*)\|\s*$")
ID_RE = re.compile(r"^[a-z0-9_]+$")


# ---- TODO.md editing (standard library only) ---------------------------------------------------------
class TodoConflict(RuntimeError):
    pass


def parse_rows(text):
    """{id: {"status", "notes"}} of the table rows of TODO.md."""
    rows = {}
    for line in text.splitlines():
        m = ROW_RE.match(line)
        if not m or m.group(1) == "id":
            continue
        cells = line.strip().strip("|").split("|")
        if len(cells) < 7:
            continue
        rows[m.group(1)] = {"status": cells[1].strip(), "notes": cells[6].strip()}
    return rows


def clean_note(s):
    return " ".join(str(s).replace("|", "/").split())


def edit_line(line, status=None, notes=None):
    """The same row line with only its Status and/or Notes cells replaced."""
    body = line.rstrip("\r\n")
    eol = line[len(body):]
    parts = body.split("|")          # ['', ' id ', ' STATUS ', ... ' notes ', '']
    if len(parts) < 9:
        raise ValueError("not a TODO row: %r" % line)
    if status is not None:
        if status not in STATUSES:
            raise ValueError("bad status %r" % status)
        parts[2] = " %s " % status
    if notes is not None:
        parts[7] = " %s " % clean_note(notes)
    return "|".join(parts) + eol


def backup(data, backup_dir, keep=40):
    os.makedirs(backup_dir, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    with open(os.path.join(backup_dir, "TODO-%s.md" % stamp), "wb") as f:
        f.write(data)
    old = sorted(p for p in os.listdir(backup_dir) if p.startswith("TODO-"))
    for p in old[:-keep]:
        try:
            os.remove(os.path.join(backup_dir, p))
        except OSError:
            pass


def update_rows(path, decide, backup_dir=None, retries=10, _before_replace=None):
    """Edit TODO.md atomically. decide(rows) -> {id: (status or None, notes or None)} is called
    on the freshly read file (so it sees the image agent's latest edits). Only those rows'
    Status/Notes cells change; every other byte of the file is kept. If the file changes
    between the read and the rename, everything is redone on the new content."""
    lock = _locks.setdefault(os.path.abspath(path), threading.Lock())
    with lock:
        for _ in range(retries):
            with open(path, "rb") as f:
                data = f.read()
            text = data.decode("utf-8")
            changes = decide(parse_rows(text))
            if not changes:
                return {}
            out, done = [], set()
            for line in text.splitlines(keepends=True):
                m = ROW_RE.match(line.rstrip("\r\n"))
                if m and m.group(1) in changes and m.group(1) not in done:
                    st, no = changes[m.group(1)]
                    line = edit_line(line, st, no)
                    done.add(m.group(1))
                out.append(line)
            missing = set(changes) - done
            if missing:
                raise KeyError("rows not in TODO.md: %s" % ", ".join(sorted(missing)))
            tmp = os.path.join(os.path.dirname(path), ".%s.review-%d.tmp" % (os.path.basename(path), os.getpid()))
            with open(tmp, "wb") as f:
                f.write("".join(out).encode("utf-8"))
                f.flush()
                os.fsync(f.fileno())
            if _before_replace:
                _before_replace()
            with open(path, "rb") as f:
                now = f.read()
            if now != data:              # someone else wrote meanwhile: redo on their version
                os.remove(tmp)
                time.sleep(0.05)
                continue
            if backup_dir:
                backup(data, backup_dir)
            os.replace(tmp, path)
            return changes
    raise TodoConflict("TODO.md keeps changing; try again")


_locks = {}


def owner_note(notes, note):
    note = clean_note(note)
    if not note:
        return notes
    return (notes + " " if notes else "") + "Owner: " + note


def action_changes(rows, sid, action, note, png_exists):
    r = rows.get(sid)
    if r is None:
        raise KeyError(sid)
    if action == "validate":
        return {sid: ("VALIDATED", owner_note(r["notes"], note))}
    if action == "reject":
        return {sid: ("REJECTED", owner_note(r["notes"], note or "rejected in review"))}
    if action == "reset":
        return {sid: ("GENERATED" if png_exists else "TODO", None)}
    raise ValueError("unknown action %r" % action)


# ---- the review state (processing through art_consistency) ----------------------------------------------
class Review:
    def __init__(self, incoming, cache, char_size, game=None):
        self.incoming, self.cache, self.char_size = incoming, cache, char_size
        self.game = game                 # another game's art_game module (--game), None = Bomber Mole
        self.todo = os.path.join(incoming, "TODO.md")
        self.backups = os.path.join(cache, "backups")
        self.index, self.stamp, self.busy, self.error = {}, 0, False, None
        self.seen = {}
        self.lock = threading.Lock()

    def png(self, sid):
        return os.path.join(self.incoming, sid + ".png")

    def mtimes(self):
        out = {}
        for n in os.listdir(self.incoming):
            if n.endswith(".png") and n.count(".") == 1:
                st = os.stat(os.path.join(self.incoming, n))
                out[n[:-4]] = (st.st_mtime, st.st_size)
        return out

    def process(self):
        with self.lock:
            if self.busy:
                return
            self.busy = True
        try:
            t0 = time.time()
            import art_sync
            import art_consistency
            rows = art_sync.read_todo(self.todo)
            if self.game:
                strips = self.game.strips(self.char_size)
            else:
                sheets, _, _, _ = art_sync.load_modules(self.char_size)
                strips = {s.id: s for s in art_sync.all_strips(sheets)}
            seen = self.mtimes()
            res = art_consistency.process(self.incoming, rows, strips, ("VALIDATED", "GENERATED", "REJECTED"))
            index = art_consistency.write_review(res, self.cache)
            for sid, s in strips.items():
                if sid not in index:
                    index[sid] = {"id": sid, "family": art_consistency.display_family(s), "group": s.group,
                                  "sheet": s.sheet, "w": s.w, "h": s.h, "frames_expected": s.frames,
                                  "frames_found": None, "error": None, "flags": [], "frame_flags": [],
                                  "notes": [], "imported": False, "palette": None}
            self.index, self.seen, self.stamp, self.error = index, seen, int(time.time()), None
            print("art_review: processed %d strips in %.1f s" % (len(res), time.time() - t0), flush=True)
        except Exception:                 # noqa: BLE001 - shown in the page
            self.error = traceback.format_exc()
            print(self.error, file=sys.stderr, flush=True)
        finally:
            self.busy = False

    def process_async(self):
        threading.Thread(target=self.process, daemon=True).start()

    def state(self):
        with open(self.todo, encoding="utf-8") as f:
            rows = parse_rows(f.read())
        changed = self.mtimes() != self.seen
        out = []
        for sid, r in rows.items():
            e = dict(self.index.get(sid, {"id": sid, "family": "?", "flags": [], "frame_flags": [], "notes": []}))
            e.update(id=sid, status=r["status"], todo_notes=r["notes"], png=os.path.exists(self.png(sid)))
            out.append(e)
        bgs = []
        for d in (self.game.BG_DIRS if self.game else ("docs/art-preview", "docs/screenshots")):
            p = os.path.join(ROOT, d)
            if os.path.isdir(p):
                bgs += ["%s/%s" % (d, n) for n in sorted(os.listdir(p))
                        if n.endswith(".png") and (d.endswith("screenshots") or n.startswith("ingame-ai-"))
                        and "title" not in n and "transition" not in n and "iris" not in n]
        return {"rows": out, "stamp": self.stamp, "busy": self.busy, "error": self.error,
                "stale": changed, "char_size": self.char_size, "backgrounds": bgs, "todo": self.todo}

    def unflagged(self, fam, rows):
        ids = []
        for sid, r in rows.items():
            e = self.index.get(sid)
            if not e or e.get("family") != fam or r["status"] != "GENERATED":
                continue
            if e.get("error") or e.get("flags") or e.get("frame_flags") or not e.get("imported"):
                continue
            ids.append(sid)
        return ids


# ---- HTTP ----------------------------------------------------------------------------------------------------
def make_handler(review):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, fmt, *a):
            if "/api/set" in self.path or "/api/batch" in self.path:
                sys.stderr.write("art_review: %s\n" % (fmt % a))

        def send(self, code, body, ctype="application/json"):
            if isinstance(body, (dict, list)):
                body = json.dumps(body).encode("utf-8")
            elif isinstance(body, str):
                body = body.encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def file(self, path, ctype="image/png"):
            if not os.path.isfile(path):
                return self.send(404, {"error": "not found"})
            with open(path, "rb") as f:
                self.send(200, f.read(), ctype)

        def do_GET(self):
            u = urllib.parse.urlparse(self.path)
            p = u.path
            try:
                if p == "/":
                    page = PAGE.replace("Bomber Mole", review.game.TITLE) if review.game else PAGE
                    return self.send(200, page, "text/html; charset=utf-8")
                if p == "/api/state":
                    return self.send(200, review.state())
                m = re.match(r"^/(orig|proc)/([a-z0-9_]+)\.png$", p)
                if m:
                    base = review.incoming if m.group(1) == "orig" else os.path.join(review.cache, "frames")
                    return self.file(os.path.join(base, m.group(2) + ".png"))
                m = re.match(r"^/bg/((?:games/[a-z0-9]+/)?docs/(?:screenshots|art-preview)/[A-Za-z0-9_.-]+\.png)$", p)
                if m:
                    return self.file(os.path.join(ROOT, m.group(1)))
                return self.send(404, {"error": "not found"})
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_POST(self):
            n = int(self.headers.get("Content-Length") or 0)
            try:
                req = json.loads(self.rfile.read(n).decode("utf-8") or "{}")
            except ValueError:
                return self.send(400, {"error": "bad json"})
            try:
                if self.path == "/api/set":
                    sid = req.get("id", "")
                    if not ID_RE.match(sid):
                        return self.send(400, {"error": "bad id"})
                    ch = update_rows(review.todo, lambda rows: action_changes(
                        rows, sid, req.get("action"), req.get("note", ""), os.path.exists(review.png(sid))),
                        review.backups)
                    return self.send(200, {"ok": True, "changed": sorted(ch)})
                if self.path == "/api/batch":
                    fam = req.get("family", "")
                    ch = update_rows(review.todo, lambda rows: {sid: ("VALIDATED", None)
                                                                for sid in review.unflagged(fam, rows)},
                                     review.backups)
                    return self.send(200, {"ok": True, "changed": sorted(ch)})
                if self.path == "/api/reprocess":
                    review.process_async()
                    return self.send(200, {"ok": True})
                return self.send(404, {"error": "not found"})
            except (KeyError, ValueError, TodoConflict) as ex:
                return self.send(409, {"error": str(ex)})
    return H


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--game", default="bombermole", help="another game (games/<game>/tools/art_game.py)")
    ap.add_argument("--incoming", default=None, help="default games/<game>/art/incoming")
    ap.add_argument("--cache", default=os.path.join(ROOT, "build", "art-review"))
    ap.add_argument("--char-size", type=int, default=24)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8765)
    a = ap.parse_args()
    game = None
    if a.game != "bombermole":
        import art_sync
        game = art_sync.game_module(a.game)
        a.cache = os.path.join(a.cache, a.game)
    a.incoming = a.incoming or os.path.join(ROOT, "games", a.game, "art", "incoming")
    review = Review(os.path.abspath(a.incoming), os.path.abspath(a.cache), a.char_size, game)
    print("art_review: TODO.md = %s" % review.todo, flush=True)
    review.process_async()
    srv = http.server.ThreadingHTTPServer((a.host, a.port), make_handler(review))
    print("art_review: open http://localhost:%d  (Ctrl+C to stop)" % a.port, flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


PAGE = r"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>Bomber Mole art review</title>
<style>
:root { --bg:#1b1d26; --card:#262a36; --ink:#e8e8ee; --dim:#9aa0b4; --ok:#4fbf6a; --bad:#e0564f; --warn:#e8b04a; --gen:#6aa0e8; }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--ink); font:14px/1.4 system-ui, sans-serif; }
header { position:sticky; top:0; z-index:5; background:#12131a; padding:8px 14px; display:flex; flex-wrap:wrap; gap:10px; align-items:center; border-bottom:1px solid #333; }
header h1 { font-size:16px; margin:0 10px 0 0; }
select, input, button { font:inherit; background:#30354a; color:var(--ink); border:1px solid #4a5068; border-radius:4px; padding:3px 8px; }
button { cursor:pointer; } button:hover { background:#3c4260; }
button.v { background:#23512f; border-color:#2f7a43; } button.r { background:#5a2522; border-color:#8a3833; }
#counts span { margin-right:10px; color:var(--dim); }
#msg { color:var(--warn); }
main { padding:12px; display:grid; grid-template-columns:repeat(auto-fill, minmax(620px, 1fr)); gap:12px; }
.card { background:var(--card); border-radius:8px; padding:10px; border-left:5px solid #555; }
.card.VALIDATED { border-left-color:var(--ok); } .card.REJECTED { border-left-color:var(--bad); } .card.GENERATED { border-left-color:var(--gen); }
.head { display:flex; gap:8px; align-items:baseline; flex-wrap:wrap; }
.head b { font-size:15px; } .st { font-weight:bold; font-size:12px; padding:1px 6px; border-radius:3px; background:#444; }
.st.VALIDATED { background:var(--ok); color:#000; } .st.REJECTED { background:var(--bad); } .st.GENERATED { background:var(--gen); color:#000; }
.meta { color:var(--dim); font-size:12px; }
.row { display:flex; gap:10px; flex-wrap:wrap; align-items:flex-start; margin-top:8px; }
.box { background:repeating-conic-gradient(#3a3f52 0 25%, #30344a 0 50%) 0 0/16px 16px; border-radius:4px; padding:4px; }
.lbl { font-size:11px; color:var(--dim); display:block; margin-bottom:2px; }
.orig { max-width:590px; max-height:170px; image-rendering:auto; display:block; }
canvas { image-rendering:pixelated; image-rendering:crisp-edges; display:block; }
ul { margin:6px 0 0; padding-left:18px; } li.f { color:var(--bad); } li.n { color:var(--dim); font-size:12px; }
.err { color:var(--bad); font-weight:bold; margin-top:6px; }
.actions { margin-top:8px; display:flex; gap:6px; flex-wrap:wrap; align-items:center; }
.actions input { flex:1; min-width:180px; }
.notes { font-size:12px; color:#c9c9d6; margin-top:6px; }
.pal span { display:inline-block; width:12px; height:12px; margin-right:1px; border:1px solid #0006; }
#family { display:none; padding:12px; }
#family.on { display:block; }
#family .grid { display:flex; flex-wrap:wrap; gap:14px; }
#family .item { background:var(--card); padding:8px; border-radius:6px; }
.hide { display:none !important; }
</style></head><body>
<header>
  <h1>Bomber Mole art review</h1>
  <label>Family <select id="fam"><option value="">all</option></select></label>
  <label>Status <select id="stat"><option value="">all</option><option>GENERATED</option><option>VALIDATED</option><option>REJECTED</option><option>TODO</option></select></label>
  <label><input type="checkbox" id="flagged"> flagged only</label>
  <input id="q" placeholder="search id" size="12">
  <button id="famview">Family view</button>
  <button id="batch" class="v">Validate all unflagged in this family</button>
  <button id="reproc">Reprocess</button>
  <span id="counts"></span><span id="msg"></span>
</header>
<section id="family"></section>
<main id="list"></main>
<script>
const Z = 4, CELL = 16, HUD = 16;
let S = null, imgs = {}, anims = [], bgImgs = {};
const $ = (s, e) => (e || document).querySelector(s);
function el(tag, attrs, ...kids) { const e = document.createElement(tag); for (const k in (attrs || {})) { if (k === 'class') e.className = attrs[k]; else if (k.startsWith('on')) e[k] = attrs[k]; else e.setAttribute(k, attrs[k]); } for (const c of kids) if (c != null) e.append(c); return e; }
function img(src) { if (!imgs[src]) { const i = new Image(); i.src = src; imgs[src] = i; } return imgs[src]; }
function bgFor(r) {
  const f = r.family || '', g = r.group || '', bgs = S.backgrounds, pick = n => bgs.find(b => b.includes(n));
  let want = 'spring6-surface';
  if (/ferret|badger/.test(f)) want = 'spring6-underground1';
  else if (/farmer/.test(f) || /tomato|crate|splat/.test(r.id)) want = 'summer-farmer';
  else if (/owl|winter/.test(f) || /ice|snow|icicle/.test(r.id)) want = 'winter1';
  else if (/boss_cat/.test(f)) want = 'spring8-boss';
  else if (/fox|autumn/.test(f) || /leaves|pumpkin|apple|mushroom/.test(r.id)) want = 'autumn1';
  else if (/summer/.test(f) || /steam|corn|bee/.test(r.id)) want = 'summer1';
  else if (/windmill/.test(r.id)) want = 'spring7';
  return pick('screenshots/' + want) || bgs[0];
}
function frames(r) { return r.frames_found || r.frames_expected || 1; }
function zoom(r) { return r.w > 64 ? 1 : (r.w > 32 ? 2 : Z); }     // 4x, 2x for the 48-px bosses, 1x for the logo
function anim(canvas, r, z, opts) { anims.push({canvas, r, z, opts: opts || {}}); }
function drawFrame(ctx, r, i, x, y, z) { const im = img('/proc/' + r.id + '.png?v=' + S.stamp); if (!im.complete || !im.naturalWidth) return; ctx.imageSmoothingEnabled = false; ctx.drawImage(im, i * r.w, 0, r.w, r.h, x, y, r.w * z, r.h * z); }
function tick() {
  const t = Math.floor(performance.now() / 160);
  for (const a of anims) {
    if (!a.canvas.isConnected) continue;
    const ctx = a.canvas.getContext('2d'), r = a.r, n = frames(r), i = t % n;
    ctx.imageSmoothingEnabled = false;
    if (a.opts.scene) scene(ctx, r, i, a); else { ctx.clearRect(0, 0, a.canvas.width, a.canvas.height); drawFrame(ctx, r, i, 0, 0, a.z); }
  }
  anims = anims.filter(a => a.canvas.isConnected);
  requestAnimationFrame(tick);
}
function solid(r) { return r.group === 'terrain' || r.group === 'propbg' || r.id === 'hud_panel'; }
function scene(ctx, r, i, a) {
  const W = a.canvas.width / 2, H = a.canvas.height / 2;
  ctx.save(); ctx.scale(2, 2); ctx.clearRect(0, 0, W, H);
  if (solid(r)) {                        // a tile repeated: seams show
    for (let y = 0; y < H; y += r.h) for (let x = 0; x < W; x += r.w) drawFrame(ctx, r, i, x, y, 1);
  } else {
    const bg = img('/bg/' + (a.opts.bg || bgFor(r)));
    if (bg.complete && bg.naturalWidth) ctx.drawImage(bg, a.opts.ox * 2, a.opts.oy * 2, W * 2, H * 2, 0, 0, W, H);
    const cx = a.opts.cx, cy = a.opts.cy;
    drawFrame(ctx, r, i, cx * CELL + CELL / 2 - r.w / 2 - a.opts.ox, HUD + cy * CELL + CELL - r.h - a.opts.oy, 1);
  }
  ctx.restore();
}
function mini(r) {
  const c = el('canvas', {width: 320, height: 224, title: 'click: move the sprite to that cell'});
  const o = {ox: 80, oy: 40, cx: 9, cy: 5, bg: null};
  c.onclick = ev => { const b = c.getBoundingClientRect(); o.cx = Math.floor(((ev.clientX - b.left) / 2 + o.ox) / CELL); o.cy = Math.floor(((ev.clientY - b.top) / 2 + o.oy - HUD) / CELL); };
  anim(c, r, 1, Object.assign(o, {scene: true}));
  const sel = el('select', {}); sel.append(el('option', {value: ''}, 'auto background'));
  for (const b of S.backgrounds) sel.append(el('option', {value: b}, b.split('/').pop()));
  sel.onchange = () => { o.bg = sel.value || null; };
  return el('div', {}, el('span', {class: 'lbl'}, solid(r) ? 'repeated (seams)' : 'in game (2x, click to move)'), el('div', {class: 'box'}, c), solid(r) ? null : sel);
}
function strip(r, z) {
  const n = frames(r), c = el('canvas', {width: r.w * z * n + (n - 1) * 2 * z, height: r.h * z});
  const draw = () => { const ctx = c.getContext('2d'); for (let i = 0; i < n; i++) drawFrame(ctx, r, i, i * (r.w + 2) * z, 0, z); };
  const im = img('/proc/' + r.id + '.png?v=' + S.stamp); if (im.complete) draw(); else im.addEventListener('load', draw);
  return c;
}
function animBox(r, z, label) { const c = el('canvas', {width: r.w * z, height: r.h * z}); anim(c, r, z); return el('div', {}, el('span', {class: 'lbl'}, label), el('div', {class: 'box'}, c)); }
function post(url, body) { return fetch(url, {method: 'POST', body: JSON.stringify(body)}).then(r => r.json()); }
function act(r, action, note) {
  post('/api/set', {id: r.id, action, note}).then(j => { if (j.error) alert(j.error); load(); });
}
function card(r) {
  const flags = (r.flags || []).concat(r.frame_flags || []);
  const c = el('div', {class: 'card ' + r.status, id: 'c-' + r.id});
  c.append(el('div', {class: 'head'}, el('b', {}, r.id), el('span', {class: 'st ' + r.status}, r.status),
    el('span', {class: 'meta'}, `${r.family} | ${r.w}x${r.h} | frames ${r.frames_found ?? '?'} / ${r.frames_expected}` + (r.ai_scale ? ` | AI scale ${r.ai_scale}x` : '') + (r.norm && r.norm !== 1 ? ` | normalised x${r.norm}` : '') + (flags.length ? ` | ${flags.length} flag(s)` : ''))));
  if (r.png) c.append(el('div', {class: 'row'}, el('div', {}, el('span', {class: 'lbl'}, 'original AI strip'), el('div', {class: 'box'}, el('img', {class: 'orig', src: '/orig/' + r.id + '.png?v=' + S.stamp, loading: 'lazy'})))));
  if (r.error) c.append(el('div', {class: 'err'}, r.error));
  if (r.imported) {
    c.append(el('div', {class: 'row'}, el('div', {}, el('span', {class: 'lbl'}, 'frames 1x'), el('div', {class: 'box'}, strip(r, 1))),
      el('div', {}, el('span', {class: 'lbl'}, 'frames ' + zoom(r) + 'x'), el('div', {class: 'box'}, strip(r, zoom(r))))));
    c.append(el('div', {class: 'row'}, animBox(r, 1, 'game size'), animBox(r, zoom(r), 'animated ' + zoom(r) + 'x'), r.id === 'title_logo' ? null : mini(r)));
    if (r.palette) { const p = el('div', {class: 'pal'}); for (const q of r.palette) p.append(el('span', {style: `background:rgb(${q})`})); c.append(el('div', {class: 'row'}, el('span', {class: 'lbl'}, `palette (${r.palette.length})`), p)); }
  }
  if (flags.length) { const u = el('ul'); for (const f of flags) u.append(el('li', {class: 'f'}, f)); c.append(u); }
  if ((r.notes || []).length) { const u = el('ul'); for (const f of r.notes) u.append(el('li', {class: 'n'}, f)); c.append(u); }
  if (r.todo_notes) c.append(el('div', {class: 'notes'}, 'Notes: ' + r.todo_notes));
  const note = el('input', {placeholder: 'note (required to reject: what to fix)'});
  c.append(el('div', {class: 'actions'},
    el('button', {class: 'v', onclick: () => act(r, 'validate', note.value)}, 'Validate'),
    el('button', {class: 'r', onclick: () => { if (!note.value.trim()) { note.focus(); note.placeholder = 'write what to fix first'; return; } act(r, 'reject', note.value); }}, 'Reject'),
    el('button', {onclick: () => act(r, 'reset', '')}, 'Reset'), note));
  return c;
}
function visible(r) {
  const f = $('#fam').value, s = $('#stat').value, q = $('#q').value.trim();
  const flagged = (r.flags || []).length + (r.frame_flags || []).length + (r.error ? 1 : 0);
  return (!f || r.family === f) && (!s || r.status === s) && (!q || r.id.includes(q)) && (!$('#flagged').checked || flagged);
}
function render() {
  const list = $('#list'); list.innerHTML = ''; anims = [];
  const rows = S.rows.filter(visible);
  for (const r of rows) list.append(card(r));
  const c = {}; for (const r of S.rows) c[r.status] = (c[r.status] || 0) + 1;
  $('#counts').innerHTML = Object.keys(c).sort().map(k => `<span>${k} ${c[k]}</span>`).join('') + `<span>shown ${rows.length}</span>`;
  if ($('#family').classList.contains('on')) familyView();
}
function familyView() {
  const f = $('#fam').value, box = $('#family');
  box.innerHTML = '';
  if (!f) { box.append(el('p', {}, 'Pick a family first.')); return; }
  const g = el('div', {class: 'grid'});
  for (const r of S.rows.filter(r => r.family === f && r.imported)) {
    const z = r.w > 64 ? 2 : (r.w > 24 ? 3 : Z), c = el('canvas', {width: r.w * z, height: r.h * z}); anim(c, r, z);
    const fl = (r.flags || []).length + (r.frame_flags || []).length;
    g.append(el('div', {class: 'item'}, el('span', {class: 'lbl'}, `${r.id} [${r.status}]${fl ? ' - ' + fl + ' flag(s)' : ''}`), el('div', {class: 'box'}, c)));
  }
  box.append(el('h3', {}, 'Family ' + f + ': every animation at the same zoom'), g);
}
function load() {
  return fetch('/api/state').then(r => r.json()).then(j => {
    const first = !S; S = j;
    const fams = [...new Set(j.rows.map(r => r.family))].sort(), sel = $('#fam'), cur = sel.value;
    sel.innerHTML = '<option value="">all</option>' + fams.map(f => `<option${f === cur ? ' selected' : ''}>${f}</option>`).join('');
    $('#msg').textContent = j.busy ? 'processing the strips...' : (j.error ? 'processing error (see the server log)' : (j.stale ? 'PNG files changed: click Reprocess' : ''));
    render();
    if (j.busy) setTimeout(load, 2000);
  });
}
for (const id of ['#fam', '#stat', '#flagged', '#q']) $(id).addEventListener('input', render);
$('#famview').onclick = () => { $('#family').classList.toggle('on'); render(); };
$('#reproc').onclick = () => post('/api/reprocess', {}).then(() => setTimeout(load, 500));
$('#batch').onclick = () => {
  const f = $('#fam').value; if (!f) { alert('Pick a family first.'); return; }
  const ids = S.rows.filter(r => r.family === f && r.status === 'GENERATED' && r.imported && !r.error && !(r.flags || []).length && !(r.frame_flags || []).length).map(r => r.id);
  if (!ids.length) { alert('Nothing unflagged and GENERATED in ' + f); return; }
  if (!confirm('Validate ' + ids.length + ' unflagged strip(s) of ' + f + '?\n\n' + ids.join('\n'))) return;
  post('/api/batch', {family: f}).then(j => { if (j.error) alert(j.error); else alert('Validated: ' + (j.changed.join(', ') || 'nothing')); load(); });
};
load(); requestAnimationFrame(tick);
</script></body></html>
"""

if __name__ == "__main__":
    main()
