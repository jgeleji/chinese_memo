#!/usr/bin/env python3
# Headless test driver for the chinese_memo quiz app's gap-based question
# selection (chinese::questions::ask_gap_mixed).
#
# Protocol (CM_TEST_BUILD build only): the engine appends one line per
# question to $CM_ASK_LOG:
#     ASK|<chinese>|<pinyin>|<english>|<provided>|<asked>|<seq>
# and blocks until $CM_DECIDE_FILE gains a new line ("ok" or "bad"). It then
# appends one status line per answer to status1.txt:
#     <chi>|<pin>|<eng>|<p>|<a>|<seq>|success|<interval>|<last_correct_at>|<last_correct_interval>
# (the classic seven columns plus the three record columns; the record is the
# state right after this answer).
#
# The driver runs questions10_sdl with SDL_VIDEODRIVER=dummy, answers the
# questions itself, and keeps a mirror model of the selection logic
# (load_gap_status_file + the ask_gap_mixed loop) to verify:
#   * every question the engine asks is exactly the one the model picks,
#   * the ~70/30 mix: new questions never exceed 30% of the session,
#   * new questions arrive in order of increasing word length,
#   * every status line's record is exactly what the model computes
#     (interval grows to demonstrated gaps, resets to 0 on a fail),
#   * old-format rows (success/fail only) and new-format rows (explicit
#     record columns) load into the right records.
#
# Usage: python3 tools/qdriver.py [--binary <questions10_sdl>] [test...]
# Tests: empty, crafted, real. Default: all.

import argparse
import os
import shutil
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROVIDED_ASKED = [(p, a) for p in range(3) for a in range(3) if p != a]

PASS = []
FAIL = []


def check(name, cond, detail=""):
    if cond:
        PASS.append(name)
    else:
        FAIL.append(name)
        print("FAIL: %s %s" % (name, detail))


class Engine:
    """Run one scripted ask_gap_mixed session; feed decisions, collect asks."""

    def __init__(self, binary, testdir, asklog, decide):
        self.asklog = asklog
        self.decide = decide
        env = dict(os.environ)
        env["SDL_VIDEODRIVER"] = "dummy"
        env["CM_ASK_LOG"] = asklog
        env["CM_DECIDE_FILE"] = decide
        with open(decide, "w"):
            pass
        self.proc = subprocess.Popen(
            [binary], cwd=testdir, env=env,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.asks = []          # (chi,pin,eng,p,a,seq) per ASK line
        self.pos = 0
        self.stdout_seen = ""

    def _poll_asks(self):
        try:
            with open(self.asklog, "r", encoding="utf-8", errors="replace") as f:
                f.seek(self.pos)
                data = f.read()
                if "\n" not in data:
                    return False
                cut = data.rfind("\n")
                whole = data[:cut + 1]
                self.pos += len(whole.encode("utf-8"))
        except FileNotFoundError:
            return False
        got = False
        for line in whole.splitlines():
            if line.startswith("ASK|"):
                t = line.split("|")
                self.asks.append((t[1], t[2], t[3], int(t[4]), int(t[5]), int(t[6])))
                got = True
        return got

    def wait_ask(self, index, timeout=20.0):
        """Wait until at least index+1 ASK lines are in, or the engine dies."""
        end = time.time() + timeout
        while len(self.asks) <= index:
            if self.proc.poll() is not None:
                raise RuntimeError("engine exited with %s, stdout:\n%s" % (
                    self.proc.returncode, self.read_stdout()))
            if not self._poll_asks() and time.time() > end:
                raise RuntimeError("timeout waiting for ask %d, stdout:\n%s" % (
                    index, self.read_stdout()))
            time.sleep(0.01)
        return self.asks[index]

    def decide_ok(self):
        self._decide("ok")

    def decide_bad(self):
        self._decide("bad")

    def _decide(self, what):
        with open(self.decide, "a") as f:
            f.write(what + "\n")

    def read_stdout(self):
        if self.proc.stdout is None:
            return self.stdout_seen
        try:
            self.stdout_seen += self.proc.stdout.read().decode("utf-8", "replace")
        except Exception:
            pass
        return self.stdout_seen

    def finish(self, n):
        """All n questions decided: question n's status line is guaranteed
        written once the ASK for question n+1 shows up; then stop the engine."""
        try:
            self.wait_ask(n)          # index n == question n+1
        except RuntimeError:
            pass                      # engine already gone
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()


class Model:
    """Mirror of load_gap_status_file + the ask_gap_mixed selection loop."""

    def __init__(self, entries, seq=0):
        # entries: list of (chinese, english, pinyin) in loaded_data order
        self.entries = entries
        self.rec = {}
        for q in range(len(entries)):
            for (p, a) in PROVIDED_ASKED:
                self.rec[(q, p, a)] = [0, -1, -1, -1]  # interval, lca, lci, last_seq
        self.seq = seq
        self.session_total = 0
        self.session_new = 0
        self.unseen = None      # built lazily on first pick(): by then any
        self.unseen_pos = 0     # baseline status rows have been applied
        self.reaskq = None      # (due_seq, key), mirrors the engine's queue
        self.reask_gap = 5

    @property
    def seen_total(self):
        return sum(1 for v in self.rec.values() if v[3] >= 0)

    def find_key(self, chi, pin, eng):
        for q, (c, e, p) in enumerate(self.entries):
            if c == chi and p == pin and e == eng:
                return q
        return None

    def apply_line(self, chi, pin, eng, p, a, seq, result, gap=None, lca=None, lci=None):
        """Fold one status line into the records (mirrors load_gap_status_file;
        these tests always use exact full-field rows)."""
        key = self.find_key(chi, pin, eng)
        if key is None:
            return None
        g = self.rec[(key, p, a)]
        if seq > g[3]:
            g[3] = seq
        if result == "success":
            if g[1] >= 0:
                g[2] = seq - g[1]
            g[1] = seq
        elif result == "fail":
            g[2] = -1
        if gap is not None:
            g[0], g[1], g[2] = gap, lca, lci
        self.seq = max(self.seq, seq)
        return key

    def pick(self):
        """Return (q,p,a) the engine should ask next."""
        if self.unseen is None:
            self.unseen = [k for k in sorted(self.rec) if self.rec[k][3] < 0]
            self.unseen.sort(key=lambda k: len(self.entries[k[0]][0]))  # by length
            # seed the re-ask queue: last attempt ended in a mistake
            # (last_correct_at older than last_seq) and asked within the
            # last 30 questions of the previous run
            self.reaskq = [(self.seq, k) for k in sorted(self.rec)
                           if self.rec[k][3] >= 0
                           and self.rec[k][1] < self.rec[k][3]
                           and self.rec[k][3] + 30 >= self.seq]
        self.seq += 1
        self.session_total += 1
        if self.unseen_pos >= len(self.unseen):
            take_new = False
        elif self.seen_total == 0:
            take_new = True
        else:
            take_new = 10 * (self.session_new + 1) <= 3 * self.session_total
        key = None
        if take_new:
            key = self.unseen[self.unseen_pos]
            self.unseen_pos += 1
            self.session_new += 1
        else:
            # a due re-ask takes the seen slot (earliest due first,
            # insertion order on ties)
            due = [(d, k) for (d, k) in self.reaskq if d <= self.seq]
            if due:
                d, k = min(due, key=lambda x: x[0])
                self.reaskq.remove((d, k))
                key = k
        if key is None:
            best = None
            for k in sorted(self.rec):
                g = self.rec[k]
                if g[3] < 0:
                    continue
                if best is None:
                    best = k
                    continue
                b = self.rec[best]
                if g[0] < b[0] or (g[0] == b[0] and g[3] < b[3]):
                    best = k
            if best is None:   # nothing ever asked yet: take a new one
                key = self.unseen[self.unseen_pos]
                self.unseen_pos += 1
                self.session_new += 1
            else:
                key = best
        # whichever way it was picked, drop it from the queue
        self.reaskq = [(d, k) for (d, k) in self.reaskq if k != key]
        return key

    def answer(self, key, ok):
        """Mirror the post-answer update; returns the record written."""
        g = self.rec[key]
        if ok:
            if g[1] >= 0:
                iv = self.seq - g[1]
                if iv > g[0]:
                    g[0] = iv
                g[2] = iv
            elif g[0] < 1:
                g[0] = 1
            g[1] = self.seq
        else:
            g[0] = 0
            g[2] = -1
            if self.reaskq is not None:
                self.reaskq.append((self.seq + self.reask_gap, key))
        g[3] = self.seq
        return tuple(g[:3])   # the three columns the engine writes


def union_entries(path):
    """Mirror of load_file for full-field rows: chinese|english[;eng]|pinyin.
    Rows without a pinyin column are skipped (the model could not know the
    dictionary-derived pinyin) -- the unit tests only feed full-field files."""
    entries = []
    seen = set()
    with open(path, encoding="utf-8") as f:
        for line in f:
            t = line.rstrip("\r\n").split("|")
            if len(t) < 3:
                continue
            chi, pinyin = t[0], t[2]
            for eng in t[1].split(";"):
                eng = eng.lower()
                if (chi, pinyin, eng) in seen:
                    continue
                seen.add((chi, pinyin, eng))
                entries.append((chi, eng, pinyin))
    return entries


def make_testdir(base, name):
    d = os.path.join(base, name)
    if os.path.exists(d):
        shutil.rmtree(d)
    os.makedirs(d)
    os.symlink(os.path.join(REPO, "assets"), os.path.join(d, "assets"))
    os.symlink(os.path.join(REPO, "data"), os.path.join(d, "data"))
    return d


def status_lines(path, start_size=0):
    with open(path, "rb") as f:
        f.seek(start_size)
        data = f.read().decode("utf-8", "replace")
    return [l for l in data.splitlines() if l.strip()]


# ---------------------------------------------------------------------------

FULL_UNION_LINES = [
    # deliberately small: 1..3-character words, one explicit duplicate row,
    # assorted directions exercised through the p/a choices
    "猫|cat|māo",
    "狗|dog|gǒu",
    "我|me;i|wǒ",
    "你好|hello;nihao|nǐ hǎo",
    "猫|cat|māo",                # duplicate: load_file must skip it
    "女朋友|girlfriend|nǚ péng you",
    "牛仔裤|jeans|niú zǐ kù",
    "女|woman;female|nǚ",
    "吃|to eat|chī",
    "喝茶|to drink tea|hē chá",
]


def parse_written(line):
    t = line.split("|")
    if len(t) != 10:
        return None
    return t, int(t[5]), t[6], (int(t[7]), int(t[8]), int(t[9]))


def check_reask_soon(asks, bad_idx, name):
    """A question answered wrong must be asked again within the next 10
    questions (re-ask gap 5, plus headroom for new-question slots)."""
    for i in bad_idx:
        key = (asks[i][0], asks[i][3], asks[i][4])
        nxt = None
        for j in range(i + 1, min(i + 11, len(asks))):
            if (asks[j][0], asks[j][3], asks[j][4]) == key:
                nxt = j
                break
        check("%s.reask%d" % (name, i + 1),
              nxt is not None and 2 <= nxt - i <= 10,
              "missed q%d %s next-ask=%s" % (i + 1, key, nxt))


def test_empty(binary, base):
    d = make_testdir(base, "qtest_empty")
    union = os.path.join(d, "union_q.txt")
    with open(union, "w", encoding="utf-8") as f:
        f.write("\n".join(FULL_UNION_LINES) + "\n")
    open(os.path.join(d, "status1.txt"), "w").close()

    entries = union_entries(union)
    model = Model(entries)

    eng = Engine(binary, d, os.path.join(d, "ask.log"), os.path.join(d, "dec.txt"))
    N = 30
    plan = ["ok"] * N
    plan[12] = "bad"      # late enough that the seen pool is non-degenerate
    for i in range(N):
        ask = eng.wait_ask(i)
        want = model.pick()
        q, p, a = want
        got = ask[:5]
        exp = (entries[q][0], entries[q][2], entries[q][1], p, a)
        check("empty.q%d.ask==model" % (i + 1), got == exp, "got %s want %s" % (got, exp))
        ok = plan[i] == "ok"
        model.answer(want, ok)
        eng.decide_ok() if ok else eng.decide_bad()
    eng.finish(N)

    lines = status_lines(os.path.join(d, "status1.txt"))
    check("empty.linecount", len(lines) == N, "got %d" % len(lines))

    # replay the lines against a fresh model: every record must match
    m2 = Model(entries)
    for i, line in enumerate(lines):
        parsed = parse_written(line)
        if parsed is None:
            check("empty.line%d.cols" % i, False, line)
            continue
        t, seq, res, rec = parsed
        key = m2.find_key(t[0], t[1], t[2])
        check("empty.line%d.seq" % i, seq == i + 1, line)
        m2.seq = seq
        want = m2.answer((key, int(t[3]), int(t[4])), res == "success")
        check("empty.line%d.record" % i, list(rec) == list(want),
              "%s vs model %s" % (t, want))

    # mix + ordering invariants straight from the asks
    news, seenkeys, newlens = 0, set(), []
    for i, (chi, pin, eng2, p, a, seq) in enumerate(eng.asks[:N]):
        q = m2.find_key(chi, pin, eng2)
        k = (q, p, a)
        if k not in seenkeys:
            seenkeys.add(k)
            news += 1
            newlens.append(len(chi))
    check("empty.new-in-length-order", newlens == sorted(newlens), str(newlens))
    check("empty.new-cap-30", 10 * news <= 3 * N + 3, "new=%d total=%d" % (news, N))
    check_reask_soon(eng.asks, [i for i, pl in enumerate(plan) if pl == "bad"], "empty")


def test_crafted(binary, base):
    """Baseline mixes old 7-column rows and new 10-column rows; verifies the
    loader's record reconstruction (chain growth, fail breaking the chain,
    explicit record columns winning)."""
    d = make_testdir(base, "qtest_crafted")
    union = os.path.join(d, "union_q.txt")
    with open(union, "w", encoding="utf-8") as f:
        f.write("\n".join(FULL_UNION_LINES) + "\n")
    entries = union_entries(union)

    def row(chi, pin, eng, p, a, seq, res, rec=None):
        s = "%s|%s|%s|%d|%d|%d|%s" % (chi, pin, eng, p, a, seq, res)
        if rec is not None:
            s += "|%d|%d|%d" % rec
        return s

    # 猫: success@1, success@5 (chain 4), fail@6 -> [0, 5, -1, 6]
    # 狗: legacy success@2, success@4, then a new-format row at 7 carrying
    #     the explicit record (9, 7, 3), which must win -> [9, 7, 3, 7]
    # 吃: one success@3, no pair -> [0, 3, -1, 3]
    baseline = [
        row("猫", "māo", "cat", 1, 0, 1, "success"),
        row("猫", "māo", "cat", 1, 0, 5, "success"),
        row("猫", "māo", "cat", 1, 0, 6, "fail"),
        row("狗", "gǒu", "dog", 0, 1, 2, "success"),
        row("狗", "gǒu", "dog", 0, 1, 4, "success"),
        row("狗", "gǒu", "dog", 0, 1, 7, "success", (9, 7, 3)),
        row("吃", "chī", "to eat", 2, 0, 3, "success"),
    ]
    status = os.path.join(d, "status1.txt")
    with open(status, "w", encoding="utf-8") as f:
        f.write("\n".join(baseline) + "\n")

    # model-side load: verify the loader expectations first
    m = Model(entries)
    for line in baseline:
        t = line.split("|")
        rec = (int(t[7]), int(t[8]), int(t[9])) if len(t) >= 10 else (None, None, None)
        m.apply_line(t[0], t[1], t[2], int(t[3]), int(t[4]), int(t[5]), t[6], *rec)
    cat = m.rec[(m.find_key("猫", "māo", "cat"), 1, 0)]
    check("crafted.cat-chain", cat == [0, 5, -1, 6], str(cat))
    dog = m.rec[(m.find_key("狗", "gǒu", "dog"), 0, 1)]
    check("crafted.dog-explicit", dog == [9, 7, 3, 7], str(dog))
    eat = m.rec[(m.find_key("吃", "chī", "to eat"), 2, 0)]
    check("crafted.eat-single", eat == [0, 3, -1, 3], str(eat))

    N = 10
    eng = Engine(binary, d, os.path.join(d, "ask.log"), os.path.join(d, "dec.txt"))
    plan = ["ok"] * N
    plan[2] = "bad"
    for i in range(N):
        eng.wait_ask(i)
        eng.decide_ok() if plan[i] == "ok" else eng.decide_bad()
    eng.finish(N)

    # the engine's own view must agree with the model: replay asks+decisions
    m2 = Model(entries, seq=max(int(l.split("|")[5]) for l in baseline))
    for line in baseline:                       # same baseline the engine loaded
        t = line.split("|")
        rec = (int(t[7]), int(t[8]), int(t[9])) if len(t) >= 10 else (None, None, None)
        m2.apply_line(t[0], t[1], t[2], int(t[3]), int(t[4]), int(t[5]), t[6], *rec)
    with open(status, encoding="utf-8") as f:
        all_lines = [l for l in f.read().splitlines() if l.strip()]
    written = all_lines[len(baseline):]
    check("crafted.linecount", len(written) == N, "%d" % len(written))
    for i, (chi, pin, eng2, p, a, seq) in enumerate(eng.asks[:N]):
        want = m2.pick()
        q = m.find_key(chi, pin, eng2)
        check("crafted.ask%d==model" % (i + 1),
              want == (q, p, a) and seq == 8 + i,
              "engine %s model %s seq %d" % ((chi, p, a, seq), want, seq))
        rec = m2.answer(want, plan[i] == "ok")
        if i < len(written):
            parsed = parse_written(written[i])
            if parsed is None:
                check("crafted.line%d.cols" % i, False, written[i])
                continue
            t, wseq, res, wrec = parsed
            check("crafted.line%d.record" % i, list(wrec) == list(rec) and wseq == 8 + i,
                  "%s vs %s" % (t, rec))
            check("crafted.line%d.result" % i,
                  (res == "success") == (plan[i] == "ok"), written[i])
    check_reask_soon(eng.asks, [i for i, pl in enumerate(plan) if pl == "bad"], "crafted")


def test_real(binary, base):
    """Full union_q.txt + the real phone status1.txt: structural checks only
    (row shape, per-question record self-consistency, sequence numbers)."""
    d = make_testdir(base, "qtest_real")
    shutil.copy(os.path.join(REPO, "union_q.txt"), os.path.join(d, "union_q.txt"))
    shutil.copy(os.path.join(REPO, "status1.txt"), os.path.join(d, "status1.txt"))
    baseline_seq = 0
    with open(os.path.join(d, "status1.txt"), encoding="utf-8") as f:
        for line in f:
            t = line.rstrip("\r\n").split("|")
            if len(t) >= 6:
                try:
                    baseline_seq = max(baseline_seq, int(t[5]))
                except ValueError:
                    pass
    start_size = os.path.getsize(os.path.join(d, "status1.txt"))

    N = 40
    eng = Engine(binary, d, os.path.join(d, "ask.log"), os.path.join(d, "dec.txt"))
    bad_at = {9, 24}
    for i in range(N):
        eng.wait_ask(i)
        eng.decide_ok() if i not in bad_at else eng.decide_bad()
    eng.finish(N)

    # ("Gap mode: N seen, M new words" goes to the SDL screen buffer, not
    # stdout -- it is shown to the user at session start on a real display)
    lines = status_lines(os.path.join(d, "status1.txt"), start_size)
    check("real.linecount", len(lines) == N, "%d" % len(lines))

    state = {}   # (chi,pin,eng,p,a) -> (interval,lca,lci) seen on last line
    for i, line in enumerate(lines):
        parsed = parse_written(line)
        if parsed is None:
            check("real.line%d.cols" % i, False, line)
            continue
        t, seq, res, (interval, lca, lci) = parsed
        key = (t[0], t[1], t[2], int(t[3]), int(t[4]))
        check("real.seq%d" % i, seq == baseline_seq + 1 + i, "%d" % seq)
        if res == "fail":
            check("real.fail%d.record" % i, interval == 0 and lci == -1, line)
            if key in state:
                check("real.fail%d.lca" % i, lca == state[key][1], line)
        else:
            check("real.ok%d.lca" % i, lca == seq, line)
            if key in state:
                p_int, p_lca, p_lci = state[key]
                if p_lca >= 0:
                    check("real.ok%d.chain" % i, lci == seq - p_lca, line)
                    check("real.ok%d.intv" % i, interval == max(p_int, lci), line)
                else:
                    check("real.ok%d.first" % i, interval == max(p_int, 1), line)
        state[key] = (interval, lca, lci)
    check_reask_soon(eng.asks, sorted(bad_at), "real")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", default="/tmp/opencode/cm_build/questions10_sdl")
    ap.add_argument("tests", nargs="*", default=["empty", "crafted", "real"])
    args = ap.parse_args()
    if not os.path.exists(args.binary):
        print("no binary at %s" % args.binary)
        return 2
    base = "/tmp/opencode/qtest"
    os.makedirs(base, exist_ok=True)
    tmap = {"empty": test_empty, "crafted": test_crafted, "real": test_real}
    for name in args.tests:
        print("--- %s" % name)
        tmap[name](args.binary, base)
    print("\n%d checks passed, %d failed" % (len(PASS), len(FAIL)))
    return 0 if not FAIL else 1


if __name__ == "__main__":
    sys.exit(main())
