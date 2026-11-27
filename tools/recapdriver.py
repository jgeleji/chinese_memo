#!/usr/bin/env python3
# Real-UI scripted check of the recap screen against the IME band.
#
# One-word pool -> the block is a single question -> answer it right -> the
# block completes -> the recap screen prints and system_pause_choice blocks
# (dummy video: no events) -> the last dumped frame is the recap.
# Run with CM_IME_BAND=315 (the phone's ~41% of 768) and verify the last
# lines (weak-summary and the next-block/quit hint) are actually DRAWN:
# without the band clamp in present() the text area reached the bottom of
# the screen and those lines landed under the keyboard.
import os, re, shutil, subprocess, sys, time

BIN = "/tmp/opencode/cm_build/questions10_sdl"
REPO = "/home/jgeleji/Documents/chinese_memo"
D = "/tmp/opencode/block15/recapchk"
BAND = 315

shutil.rmtree(D, ignore_errors=True)
os.makedirs(D + "/dump")
for a in ("assets", "data"):
    os.symlink(os.path.join(REPO, a), D + "/" + a)
os.symlink(REPO + "/assets/NotoSansSC.ttf", D + "/NotoSansSC.ttf")
open(D + "/union_q.txt", "w").write("好|good|hǎo\n")
#orig: ("三角|triangle|sānjiǎo\n")
open(D + "/status1.txt", "w").close()
open(D + "/cm_dump_dir", "w").write(D + "/dump")

env = dict(os.environ, SDL_VIDEODRIVER="dummy", CM_IME_BAND=str(BAND),
           CM_AUTOKEYS_FILE=D + "/keys.txt", CM_STATE_FILE=D + "/state.txt")
p = subprocess.Popen([BIN], cwd=D, env=env,
                     stdout=subprocess.DEVNULL, stderr=open(D + "/log.txt", "w"))

def wait_seq(n, deadline=15.0):
    end = time.time() + deadline
    s = ""
    while time.time() < end:
        try: s = open(D + "/state.txt").read()
        except OSError: s = ""
        m = re.search(r"WAIT seq=(\d+) state=(\d+) keys=(\d+) frame=(\d+)", s)
        if m and int(m.group(1)) >= n and int(m.group(3)) == 0:
            return int(m.group(4))
        time.sleep(0.03)
    raise RuntimeError("no WAIT seq>=%d: %s" % (n, s))

wait_seq(0)                                   # the question is up
with open(D + "/keys.txt", "a") as kf:       # pinyin answer: san1 jiao3
    kf.write("hao3\n\nx\n")
time.sleep(3.0)                               # verdict + show_for + recap
p.kill()

fs = sorted(f for f in os.listdir(D + "/dump") if f.endswith(".txt"))
txt = open(D + "/dump/" + fs[-1]).read()
sec = txt.split("--- lines as drawn")
drawn = [re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", l) for l in
         (sec[1].splitlines() if len(sec) > 1 else [])
         if not l.strip().startswith("---")]

PASS, FAIL = [], []
def check(name, cond, detail=""):
    (PASS if cond else FAIL).append(name)
    if not cond: print("FAIL:", name, detail)

# the recap must be the screen we end on
check("recap-header", any("Block 1 complete" in l for l in drawn),
      "\n".join(drawn)[:400])
# the important tail lines must be DRAWN (no '-' = scrolled off the top)
hint = [l for l in drawn if "quit" in l]
nxt = [l for l in drawn if "next block" in l]
word = [l for l in drawn if "好" in l and ("ok (" in l or "again (" in l)]
check("hint-quit-drawn", hint and hint[0][0] != "-", str(hint))
check("hint-next-drawn", nxt and nxt[0][0] != "-", str(nxt))
check("word-list-drawn", word and word[0][0] != "-", str(word))

# pixel truth: nothing (except background) may be drawn under the band
from PIL import Image
img = Image.open(D + "/dump/" + fs[-1].replace(".txt", ".png"))
W, H = img.size
bottom = img.crop((0, H - BAND + 6, W, H)).convert("RGB")
# background is a dark gradient (luma ~12..25); text is bright. Any pixel
# brighter than the gradient by a margin means text landed under the band.
bright = [p for p in bottom.getdata() if max(p) > 90]
check("band-clean", not bright, "bright pixels under band: %d" % len(bright))

print("PASS %d FAIL %d" % (len(PASS), len(FAIL)))
sys.exit(1 if FAIL else 0)
