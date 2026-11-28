#!/usr/bin/env python3
# Headless check of ask_gap_block15 (block mode) in questions10_sdl.
# Protocol identical to tools/qdriver.py (CM_ASK_LOG + CM_DECIDE_FILE), plus
# the CM_BLOCK_MAX test hook to end after N completed blocks.
#
# Scenario (20-word fixture, empty status):
#   block 1: 15 new words (seen pool empty -> ratio rule takes all new)
#            decisions: ok, BAD, then 15 ok  -> 2 passes, restart at q1
#   block 2: 11 seen + 4 new (slots 4,7,10,14); decisions: q3 BAD, then all ok
#            -> 2 passes; the missed word tallies 1c/1w -> NOT >50% -> weak,
#            its verdict row is "fail", form does not advance
# Checks: restart-at-1 order, frozen form across passes, per-block selection,
# row stream (one verdict row per word per completed block, nothing else), settled state columns,
# stderr blockdone lines, clean exit at CM_BLOCK_MAX.

import os, subprocess, sys, time

BIN = "/tmp/opencode/cm_build/questions10_sdl"
D = "/tmp/opencode/block15/run"
os.makedirs(D, exist_ok=True)
for a in ("assets", "data"):
    src = os.path.join("/home/jgeleji/Documents/chinese_memo", a)
    dst = os.path.join(D, a)
    if not os.path.exists(dst): os.symlink(src, dst)
import glob
for f in glob.glob("/home/jgeleji/Documents/chinese_memo/assets/*"):
    dst = os.path.join(D, os.path.basename(f))
    if not os.path.exists(dst): os.symlink(f, dst)

WORDS = [("一","yī","one"),("二","èr","two"),("三","sān","three"),("四","sì","four"),
         ("五","wǔ","five"),("六","liù","six"),("七","qī","seven"),("八","bā","eight"),
         ("九","jiǔ","nine"),("十","shí","ten"),("好","hǎo","good"),("人","rén","person"),
         ("大","dà","big"),("小","xiǎo","small"),("上","shàng","up"),("下","xià","down"),
         ("门","mén","door"),("猫","māo","cat"),("狗","gǒu","dog"),("水","shuǐ","water")]

with open(os.path.join(D, "union_q.txt"), "w", encoding="utf-8") as f:
    for chi, pin, eng in WORDS:
        f.write("%s|%s|%s\n" % (chi, pin, eng))
open(os.path.join(D, "status1.txt"), "w").close()

asklog = os.path.join(D, "ask.log"); decide = os.path.join(D, "dec.txt")
for p in (asklog, decide): open(p, "w").close()

env = dict(os.environ)
env["SDL_VIDEODRIVER"] = "dummy"
env["CM_ASK_LOG"] = asklog
env["CM_DECIDE_FILE"] = decide
env["CM_BLOCK_MAX"] = "2"
proc = subprocess.Popen([BIN], cwd=D, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

asks = []; pos = 0
def poll():
    global pos
    with open(asklog, "r", encoding="utf-8") as f:
        f.seek(pos); data = f.read()
        if "\n" not in data: return
        cut = data.rfind("\n"); pos += len(data[:cut+1].encode())
    for line in data[:cut+1].splitlines():
        if line.startswith("ASK|"):
            t = line.split("|")
            asks.append((t[1], t[2], t[3], int(t[4]), int(t[5]), int(t[6])))

def wait_ask(i, timeout=20):
    end = time.time() + timeout
    while len(asks) <= i:
        if proc.poll() is not None:
            raise RuntimeError("engine died rc=%s" % proc.returncode)
        poll()
        if time.time() > end: raise RuntimeError("timeout ask %d" % i)
        time.sleep(0.01)

def decide_line(what):
    with open(decide, "a") as f: f.write(what + "\n")

# plan: block1: ok,bad,ok*15 ; block2: ok,ok,bad,ok*13 (15) => 17+17 decisions
plan = ["ok","bad"] + ["ok"]*15 + ["ok","ok","bad"] + ["ok"]*15
for i, what in enumerate(plan):
    wait_ask(i)
    decide_line(what)
# wait for exit
try:
    rc = proc.wait(timeout=10)
except subprocess.TimeoutExpired:
    proc.kill(); rc = None
out = proc.stdout.read().decode("utf-8", "replace")

PASS=[]; FAIL=[]
def check(name, cond, detail=""):
    (PASS if cond else FAIL).append(name)
    if not cond: print("FAIL:", name, detail)

# words in data order = the new-word order here (fixture len all 1 char,
# stable sort keeps data order) -> block1 = first 15 words
blk1 = [w[0] for w in WORDS[:15]]

check("ask-count", len(asks) >= 34, str(len(asks)))
# block1: ask0..1 = words 0,1; ask2 restarts at word 0, form 0 (frozen)
check("b1.q1", asks[0][0] == "一" and asks[0][3:] == (0,2,1), str(asks[0]))
check("b1.q2", asks[1][0] == "二", str(asks[1]))
check("b1.restart", asks[2][0] == "一" and asks[2][3] == 0 and asks[2][4] == 2, str(asks[2]))
# pass2 covers the block in the same order
ok_seq = True
for bi in range(15):
    a = asks[2+bi]
    if a[0] != blk1[bi] or (a[3], a[4]) != (0,2): ok_seq = False; bad = a
check("b1.pass2-order", ok_seq, str(bad) if not ok_seq else "")

# block2 starts at ask 17: pass1 (3 asks: ok,ok,bad) then pass2 (15). Slots
# 4,7,10,14 (1-based) of the block are new (words 15..18)
b2 = asks[17:35]
new_idx = [15,16,17,18]
# pass2 order is the block's fixed order (positions 1..15 at ask indices 3..17)
b2_order = [a[0] for a in b2[3:18]]
# argmin tie order: interval 0 for all, last_seq asc; block1 pass2 asked all
# 15 words at seqs 3..17
last_ask_b1 = {0:3, 1:4, 2:5, 3:6, 4:7, 5:8, 6:9, 7:10, 8:11, 9:12, 10:13, 11:14, 12:15, 13:16, 14:17}
chosen = set(); exp_b2 = []; bnew = 0
for slot in range(1,16):
    if 10*(bnew+1) <= 3*slot:
        w = new_idx[bnew]; bnew += 1
    else:
        cand = [i for i in range(15) if i not in chosen]
        cand.sort(key=lambda i: last_ask_b1[i])
        w = cand[0]
    chosen.add(w); exp_b2.append(WORDS[w][0])
got_b2 = b2_order
check("b2.selection", got_b2 == exp_b2, "got %s exp %s" % (got_b2, exp_b2))
check("b2.newcount", sum(1 for w in got_b2 if w in [WORDS[i][0] for i in new_idx]) == 4, str(got_b2))
# the word missed in block2 (pos 3 -> exp_b2[2]) restarts the pass at pos1
check("b2.restart", b2[3][0] == exp_b2[0], str(b2[3]))
# frozen form for block2: 一 rotated to form 1 when block1 settled it
check("b2.restart-form", (b2[3][3], b2[3][4]) == (0,1), str(b2[3]))

# status rows: ONLY the verdict rows - 15 per completed block, nothing for
# interrupted blocks, one row per word per block no matter how many times
# the restart loop asked it
rows = [l.strip().split("|") for l in open(os.path.join(D,"status1.txt"), encoding="utf-8") if l.strip()]
check("rows-total", len(rows) == 30, str(len(rows)))
# verdict rows for block1: 一 ok (2c,1w): success, interval 0, lca=3, round_min -1, form 1
v = [r for r in rows if r[0]=="一"]
check("v.一.b1", v[0][6]=="success" and v[0][7]=="0" and v[0][8]=="3"
      and v[0][9]=="-1" and v[0][10]=="1" and v[0][11]=="0", str(v[0]))
# block2 settles 一 again at its last correct (seq 21): form 1->2
check("v.一.b2", v[1][6]=="success" and v[1][8]=="21" and v[1][10]=="2", str(v[1]))
# weak words: 二 (1c/1w in block1) and 三 (1c/1w in block2): verdict fail, form
# does not advance, interval 0
weakchi = exp_b2[2]   # 三
v = [r for r in rows if r[0]==weakchi]
last = v[-1]
check("v.weak", last[6]=="fail" and last[10]=="1" and last[8]=="5" and last[7]=="0",
      "word %s %s" % (weakchi, last))
check("v.weak.rows", sum(1 for r in v if r[6]=="success")==1 and sum(1 for r in v if r[6]=="fail")==1,
      str(v))
# 二: weak verdict at end of block1 (1c1w), then ok in block2 (2c0w)
v = [r for r in rows if r[0]=="二"]
check("v.二", len(v)==2 and v[0][6]=="fail" and v[1][6]=="success", str(v))
# stderr markers (二 weak in block1, 三 weak in block2)
check("stderr.blockdone1", "blockdone no=1 pass=2 weak=1" in out, out[-500:])
# recap lines (the text the recap screen shows): block1: 二 again (1/2), 一 ok
# (2/2); block2: 三 again (1/2)
rec1 = out.split("blockdone no=1")[1].split("blockdone no=2")[0]
rec2 = out.split("blockdone no=2")[1]
check("recap.b1", "|二|two|again|1/2" in rec1 and "|一|one|ok|2/2" in rec1, rec1[:400:])
check("recap.b2", "|三|three|again|1/2" in rec2, rec2[:400:])
check("recap.counts", rec1.count("RECAP|") == 15 and rec2.count("RECAP|") == 15,
      "%d %d" % (rec1.count("RECAP|"), rec2.count("RECAP|")))
check("stderr.blockdone2", "blockdone no=2 pass=2 weak=1" in out, out[-500:])
check("exit-code", rc == 0, str(rc))

print("PASS %d FAIL %d" % (len(PASS), len(FAIL)))
sys.exit(1 if FAIL else 0)
