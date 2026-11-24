#!/usr/bin/env python3
# ---------------------------------------------------------------------------
# wdriver.py - scripted headless test driver for write_sdl (chinese write)
#
# Launches the desktop build with SDL_VIDEODRIVER=dummy and the CM_* hooks,
# then scripts real interactions through the exact same capture+grading path
# a finger uses:
#   - reads the state line (CM_STATE_FILE): phase, stroke index, comp index,
#     pick progress, the current practice box x,y,s, candidate boxes, counters
#   - computes where each reference stroke must land by replicating the
#     app's make_mapper()/Mapper::map math (write_sdl.cpp) from write_data.txt
#   - sends @STROKE drags along those points: exact strokes must be accepted,
#     reversed/too-short ones must be rejected and raise the fail counter
#   - in split mode taps components in writing order (cands= from the state
#     line), first tapping a wrong one to check it is not accepted
#
# Usage: python3 tools/wdriver.py [--build DIR] [--run DIR] [--seed N] [--dump]
# Exits 0 when every check passed. Frame PNGs land in <run>/frames with --dump.
# ---------------------------------------------------------------------------

import argparse
import math
import os
import shutil
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ap = argparse.ArgumentParser()
ap.add_argument("--build", default="/tmp/opencode/cm_build")
ap.add_argument("--run", default="/tmp/opencode/wrun")
ap.add_argument("--seed", default="42")
ap.add_argument("--win", default="720x1440")
ap.add_argument("--dump", action="store_true")
args = ap.parse_args()

W, H = (int(v) for v in args.win.split("x"))
RUN = args.run

fails = []
checks = 0


def check(cond, what):
    global checks
    checks += 1
    print("  %s %s" % ("ok  " if cond else "FAIL", what))
    if not cond:
        fails.append(what)


# ---------------------------------------------------------------------------
# run dir + launch
# ---------------------------------------------------------------------------
if os.path.isdir(RUN):
    shutil.rmtree(RUN)
os.makedirs(RUN)
for f in ("write_data.txt", "write_pinyin.txt", "union_q.txt"):
    shutil.copy(os.path.join(REPO, f), RUN)
os.makedirs(os.path.join(RUN, "assets"))
shutil.copy(os.path.join(REPO, "assets", "NotoSansSC.ttf"),
            os.path.join(RUN, "assets"))
KEYS = os.path.join(RUN, "keys.txt")
STATE = os.path.join(RUN, "state.txt")
open(KEYS, "w").close()

# no stale instances: an earlier crashed driver run may have left one behind,
# and it would race on the same state/keys paths after rmtree+recreate
subprocess.call(["pkill", "-x", "write_sdl"])
time.sleep(0.2)

env = dict(os.environ)
env.update({
    "SDL_VIDEODRIVER": "dummy",
    "CM_WINDOW": args.win,
    "CM_SEED": args.seed,
    "CM_KEYS_FILE": KEYS,
    "CM_STATE_FILE": STATE,
})
if args.dump:
    os.makedirs(os.path.join(RUN, "frames"))
    env["CM_DUMP_DIR"] = os.path.join(RUN, "frames")

applog = open(os.path.join(RUN, "app.log"), "w")
proc = subprocess.Popen([os.path.join(args.build, "write_sdl")], cwd=RUN, env=env,
                        stdout=applog, stderr=applog)


def reap():
    """never leave an app instance behind (also on SystemExit timeouts)"""
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
    applog.close()


import atexit
atexit.register(reap)

# ---------------------------------------------------------------------------
# app data (the same files the app reads)
# ---------------------------------------------------------------------------
chars = {}
with open(os.path.join(RUN, "write_data.txt"), encoding="utf-8") as fh:
    for line in fh:
        s = line.rstrip("\n")
        if s.startswith("#G ") or not s:
            continue
        f = s.split("\t")
        if len(f) < 4:
            continue
        ids = f[1].split(" ") if f[1] != "*" else []
        comp = [int(v) for v in f[2].split(",")] if f[2] != "-" else []
        strokes = []
        for poly in f[3:]:
            v = [float(t) for t in poly.split()]
            strokes.append([(v[i], v[i + 1]) for i in range(0, len(v), 2)])
        chars[f[0]] = {"ids": ids, "comp": comp, "strokes": strokes}


def polylen(pts):
    return sum(math.dist(pts[i], pts[i + 1]) for i in range(len(pts) - 1))


def scope_for(ch, comp):
    d = chars[ch]
    if comp < 0:
        return list(range(len(d["strokes"])))
    return [i for i, c in enumerate(d["comp"]) if c == comp]


# replicates make_mapper() + Mapper::map from write_sdl.cpp (C truncates ints)
def mapped_pts(ch, scope, box):
    x, y, s = box
    pts = [p for si in scope for p in chars[ch]["strokes"][si]]
    x0, y0 = min(p[0] for p in pts), min(p[1] for p in pts)
    x1, y1 = max(p[0] for p in pts), max(p[1] for p in pts)
    sc = 0.88 * s / max(max(1.0, x1 - x0), max(1.0, y1 - y0))
    cxm, cym = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    return [(int(x + s / 2.0 + (p[0] - cxm) * sc),
             int(y + s / 2.0 - (p[1] - cym) * sc))
            for si in scope for p in chars[ch]["strokes"][si]], sc


def densify(pts):
    """subdivide every segment to ~7px so the app's 3px filter keeps each point"""
    out = [pts[0]]
    for a, b in zip(pts, pts[1:]):
        n = max(1, int(math.dist(a, b) / 7))
        for k in range(1, n + 1):
            f = k / n
            out.append((a[0] + f * (b[0] - a[0]), a[1] + f * (b[1] - a[1])))
    return out


# ---------------------------------------------------------------------------
# protocol
# ---------------------------------------------------------------------------
def send(line):
    with open(KEYS, "a") as fh:
        fh.write(line + "\n")


def state():
    try:
        with open(STATE, encoding="utf-8") as fh:
            return fh.read().strip()
    except OSError:
        return ""


def parse_state(s):
    d = {}
    for tok in s.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            d[k] = v
    return d


def cur():
    return parse_state(state())


def wait_state(sub, why, timeout=15.0):
    send("@WAIT " + sub)
    t0 = time.time()
    while time.time() - t0 < timeout:
        if proc.poll() is not None:
            raise SystemExit("app exited early (rc=%s) waiting for %r (%s)"
                             % (proc.returncode, sub, why))
        st = parse_state(state())
        if sub in state():
            return st
        time.sleep(0.01)
    raise SystemExit("TIMEOUT waiting for %r (%s); last state: %s"
                     % (sub, why, state()))


def read_box(st):
    return tuple(int(v) for v in st["box"].split(","))


def comp_of(st):
    """component being drawn (-1 = whole character) from the state line"""
    i, n = (int(v) for v in st["comp"].split("/"))
    return i - 1 if n else -1


def stroke_request(ch, st, kind="exact"):
    """densified window-px points for the stroke the app waits for"""
    comp = comp_of(st)
    idx = int(st["stroke"].split("/")[0]) - 1
    box = read_box(st)
    scope = scope_for(ch, comp)
    rp, sc = mapped_pts(ch, scope, box)
    base = sum(len(chars[ch]["strokes"][si]) for si in scope[:idx])
    n = len(chars[ch]["strokes"][scope[idx]])
    seg = rp[base:base + n]
    if kind == "reversed":
        seg = seg[::-1]
    elif kind == "short":
        seg = seg[:max(2, len(seg) // 4)]
    return densify(seg), box, sc


def ref_len_px(ch, st):
    comp = comp_of(st)
    idx = int(st["stroke"].split("/")[0]) - 1
    scope = scope_for(ch, comp)
    _, sc = mapped_pts(ch, scope, read_box(st))
    return polylen(chars[ch]["strokes"][scope[idx]]) * sc


def do_stroke(pts):
    send("@STROKE " + " ".join("%d,%d" % (int(px), int(py)) for px, py in pts))


def next_expect(st):
    """state substring proving the just-drawn stroke was accepted"""
    i, n = (int(v) for v in st["stroke"].split("/"))
    if i < n:
        return "stroke=%d/" % (i + 1)
    ci, cn = (int(v) for v in st["comp"].split("/"))
    if cn and ci < cn:
        return "comp=%d/" % (ci + 1)
    return "phase=done"


def draw_until_done(ch, kind="exact", limit=40):
    """draw every remaining stroke of the current scope chain"""
    for _ in range(limit):
        st = wait_state("phase=draw", "draw %s" % ch)
        before = st
        pts, _, _ = stroke_request(ch, st, kind)
        do_stroke(pts)
        st = wait_state(next_expect(before), "accept stroke %s of %s"
                        % (before["stroke"], ch))
        if st["phase"] == "done":
            return st
    raise SystemExit("draw_until_done ran away on %s" % ch)


def settle_draw(ch=None, tries=20):
    """make sure we are in draw phase (finish a pick first if needed)"""
    st = wait_state("frame=", "any state")
    for _ in range(tries):
        st = cur()
        if st["phase"] == "draw":
            return st
        if st["phase"] == "pick":
            i, n = (int(v) for v in st["pick"].split("/"))
            comps = chars[st["char"]]["ids"][1:]
            tap_cand(st["cands"], comps[i])
            time.sleep(0.15)
            continue
        if st["phase"] == "done":
            time.sleep(0.4)
            continue
    raise SystemExit("never reached draw phase")


# ---------------------------------------------------------------------------
# taps: buttons are laid out exactly like compute_layout() does
# ---------------------------------------------------------------------------
m = max(12, W // 30)
bh = max(46, min(96, W // 18))
bw = (W - 2 * m) // 3
BTN = {"MODE": 0, "SPLIT": 1, "NEXT": 2}


def tap_btn(name):
    i = BTN[name]
    x = m + i * bw + (bw - m // 2) // 2
    y = m + bh // 2
    send("@TAP %d,%d" % (x, y))
    time.sleep(0.15)


def tap_cand(cands, target):
    for c in cands.split(";"):
        ch_, geo = c.split("@")
        if ch_ == target:
            x, y, s = (int(v) for v in geo.split(","))
            send("@TAP %d,%d" % (x + s // 2, y + s // 2))
            time.sleep(0.15)
            return
    raise SystemExit("candidate %s not among %s" % (target, cands))


def wait_phase_one_of(phases, why, tries=25):
    for _ in range(tries):
        st = cur()
        if st.get("phase") in phases:
            return st
        tap_btn("NEXT")
    raise SystemExit("never reached %s (%s)" % (phases, why))


# ---------------------------------------------------------------------------
# scenarios
# ---------------------------------------------------------------------------
print("== boot")
st = wait_state("WAIT seq=", "first state line")
check(st["phase"] in ("draw", "pick"), "boot reaches a phase (char=%s)" % st["char"])
check(st["mode"] == "learn", "starts in LEARN")
check(st["split"] == "on", "starts with SPLIT ON")

print("== whole character, SPLIT OFF, exact strokes")
tap_btn("SPLIT")
st = wait_state("split=off", "split off")
st = settle_draw()
ch = st["char"]
fail0 = int(st["fail"])
st = draw_until_done(ch)
check(st["phase"] == "done", "%s drawn stroke by stroke reaches done" % ch)
check(int(st["fail"]) == fail0, "exact strokes caused no fails")
check(int(st["ok"]) >= 1, "done raised the ok counter to %s" % st["ok"])
ok1 = int(st["ok"])

print("== wrong stroke is rejected, then accepted when fixed")
st = wait_phase_one_of(("draw", "pick"), "fresh question")
st = settle_draw()
ch = st["char"]
while ref_len_px(ch, st) < 0.12 * read_box(st)[2]:   # a dot cannot be "reversed" meaningfully
    tap_btn("NEXT")
    st = settle_draw()
    ch = st["char"]
fail0 = int(st["fail"])
before = st
pts, _, _ = stroke_request(ch, st, "reversed")
do_stroke(pts)
time.sleep(0.3)
st = cur()
check(st["stroke"] == before["stroke"] and st["comp"] == before["comp"],
      "reversed stroke rejected without advancing (%s)" % ch)
check(int(st["fail"]) == fail0 + 1, "rejection increments the fail counter")
pts, _, _ = stroke_request(ch, st, "exact")
do_stroke(pts)
st = wait_state(next_expect(before), "same stroke accepted after fix")
check(st["stroke"] != before["stroke"] or st["phase"] == "done",
      "exact stroke accepted after the rejection")
check(int(st["fail"]) == fail0 + 1, "the accepted fix adds no extra fail")

print("== too-short swipe rejected")
st = wait_phase_one_of(("draw",), "fresh single-stroke question", tries=30)
st = settle_draw()
ch = st["char"]
while int(st["stroke"].split("/")[1]) == 1 or ref_len_px(ch, st) < 0.12 * read_box(st)[2]:
    tap_btn("NEXT")
    st = settle_draw()
    ch = st["char"]
fail0 = int(st["fail"])
before = st
pts, _, _ = stroke_request(ch, st, "short")
do_stroke(pts)
time.sleep(0.3)
st = cur()
check(st["stroke"] == before["stroke"], "short swipe rejected on %s" % ch)
check(int(st["fail"]) == fail0 + 1, "short swipe counted as fail")
pts, _, _ = stroke_request(ch, st, "exact")
do_stroke(pts)
wait_state(next_expect(before), "recovered after short swipe")

print("== split mode: pick components in writing order, then draw each")
tap_btn("SPLIT")
st = wait_state("split=on", "split on")
st = wait_phase_one_of(("pick",), "a splittable character")
ch = st["char"]
comps = chars[ch]["ids"][1:]
cands = st["cands"]
check(all(c in [c.split("@")[0] for c in cands.split(";")] for c in comps),
      "candidates include every component of %s (%s)" % (ch, comps))
wrong = next((c.split("@")[0] for c in cands.split(";")
              if c.split("@")[0] not in comps), None)
if wrong:
    tap_cand(cands, wrong)
    st2 = cur()
    check(st2.get("pick") == st.get("pick"),
          "wrong-part tap does not advance the pick (%s)" % ch)
for k, want in enumerate(comps):
    st = wait_state("phase=pick", "pick step %d" % k)
    tap_cand(st["cands"], want)
    if k + 1 < len(comps):
        wait_state("pick=%d/" % (k + 1), "pick advanced to %d" % (k + 1))
    else:
        wait_state("phase=draw", "drawing starts after last pick")
st = cur()
check(int(st["comp"].split("/")[1]) == len(comps),
      "%s draws %d components, one per sub-box" % (ch, len(comps)))
ok2 = int(st["ok"])
st = draw_until_done(ch)
check(st["phase"] == "done", "split character %s completed" % ch)
check(int(st["ok"]) == ok2 + 1, "split completion raises ok by exactly one")

print("== QUIZ toggle: label flips, drawing keeps working, nothing restarts")
st = settle_draw()
before = st
tap_btn("MODE")
st = wait_state("mode=quiz", "quiz label in state")
check(st["stroke"] == before["stroke"] and st["char"] == before["char"],
      "QUIZ toggle does not restart the character")
ch = st["char"]
pts, _, _ = stroke_request(ch, st, "exact")
do_stroke(pts)
st = wait_state(next_expect(before), "stroke accepted in QUIZ mode")
check(st["mode"] == "quiz", "drawing works with the skeleton hidden")

print("== NEXT skips cleanly")
st = cur()
ok0, fail0 = int(st["ok"]), int(st["fail"])
tap_btn("NEXT")
st = cur()
check(int(st["ok"]) == ok0 and int(st["fail"]) == fail0, "NEXT keeps the counters")

print("== quit + progress file")
send("@QUIT")
t0 = time.time()
while proc.poll() is None and time.time() - t0 < 5:
    time.sleep(0.05)
if proc.poll() is None:
    proc.kill()
    check(False, "@QUIT exits the app")
else:
    check(proc.returncode == 0, "@QUIT exits the app cleanly")
ws = os.path.join(RUN, "wstatus.txt")
if os.path.exists(ws):
    with open(ws, encoding="utf-8") as fh:
        rows = [l for l in fh if not l.startswith("#") and l.strip()]
    check(len(rows) >= 1, "wstatus.txt written with %d rows" % len(rows))
else:
    check(False, "wstatus.txt written")

print("\n%d checks, %d failures" % (checks, len(fails)))
for f in fails:
    print("  - %s" % f)
sys.exit(1 if fails else 0)
