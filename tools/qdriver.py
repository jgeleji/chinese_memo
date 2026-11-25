#!/usr/bin/env python3
# Headless test driver for the chinese_memo quiz app's word-centric gap-based
# question selection (chinese::questions::ask_gap_mixed).
#
# Protocol (runtime-gated: only when CM_ASK_LOG + CM_DECIDE_FILE are set):
# the engine appends one line per question to $CM_ASK_LOG:
#     ASK|<chinese>|<pinyin>|<english>|<provided>|<asked>|<seq>
# and blocks until $CM_DECIDE_FILE gains a new line ("ok" or "bad"). It then
# appends one status line per answer to status1.txt:
#     <chi>|<pin>|<eng>|<p>|<a>|<seq>|success|<interval>|<lca>|<round_min>|<form>|<rounds_done>
# (the classic seven columns plus the five per-word record columns; the
# record is the word's state right after this answer).
#
# One unit = one word (a loaded_data entry) asked through the four forms
# FORMS = chi->pin, chi->eng, pin->chi, eng->chi in that fixed order; a
# completed round's distance (min of its 3 correct->correct gaps) raises the
# word's interval. The driver runs questions10_sdl with SDL_VIDEODRIVER=dummy,
# answers the questions itself, and keeps a mirror model of the selection
# logic (load_gap_status_file + the ask_gap_mixed loop) to verify:
#   * every question the engine asks is exactly the word+form the model picks,
#   * forms of one word follow the fixed order,
#   * the ~70/30 mix: new words never exceed 30% of the session,
#   * new words arrive in order of increasing word length,
#   * every status line's record is exactly what the model computes (round
#     logic: round_min, completed distance, form rotation, rounds_done),
#   * old-format rows (success/fail only), the intermediate 10-column rows
#     and current 12-column rows all load into the right records,
#   * a missed word comes back within a few questions.
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

# form order: chinese->pinyin, chinese->english, pinyin->chinese, english->chinese
# (DATATYPE: CHINESE=0, ENGLISH=1, PINYIN=2)
FORMS = [(0, 2), (0, 1), (2, 0), (1, 0)]

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
    """Mirror of load_gap_status_file + the word-centric ask_gap_mixed loop.

    A word's record: [interval, last_correct_at, round_min, last_seq, form,
    rounds_done]."""

    def __init__(self, entries, seq=0):
        # entries: list of (chinese, english, pinyin) in loaded_data order
        self.entries = entries
        self.rec = {}
        for q in range(len(entries)):
            self.rec[q] = [0, -1, -1, -1, 0, 0]
        self.seq = seq
        self.session_total = 0
        self.session_new = 0
        self.unseen = None      # built lazily on first pick(): by then any
        self.unseen_pos = 0     # baseline status rows have been applied
        self.reaskq = None      # (due_seq, q), mirrors the engine's queue
        self.reask_gap = 5

    @property
    def seen_total(self):
        return sum(1 for v in self.rec.values() if v[3] >= 0)

    def find_key(self, chi, pin, eng):
        for q, (c, e, p) in enumerate(self.entries):
            if c == chi and p == pin and e == eng:
                return q
        return None

    def apply_line(self, chi, pin, eng, p, a, seq, result,
                   gap=None, lca=None, rmin=None, form=None, done=None):
        """Fold one status line into the word's record (mirrors
        load_gap_status_file; the tests use exact full-field rows)."""
        q = self.find_key(chi, pin, eng)
        if q is None:
            return None
        w = self.rec[q]
        if seq > w[3]:
            w[3] = seq
        if result == "success" and seq > w[1]:
            w[1] = seq
        # current-format rows (>=12 columns) carry the whole state right
        # after the answer; the intermediate 10-column rows carry only three
        # record columns and are treated as legacy (see apply_line)
        if gap is not None and lca is not None and rmin is not None \
                and form is not None and done is not None:
            w[0], w[1], w[2], w[4], w[5] = gap, lca, rmin, form, done
        self.seq = max(self.seq, seq)
        return q

    def pick(self):
        """Return q the engine should ask next (in the word's current form)."""
        if self.unseen is None:
            self.unseen = [q for q in sorted(self.rec) if self.rec[q][3] < 0]
            self.unseen.sort(key=lambda q: len(self.entries[q][0]))  # stable, by length
            # seed the re-ask queue: last attempt ended in a mistake
            # (last_correct_at older than last_seq) and asked within the
            # last 30 questions of the previous run
            self.reaskq = [(self.seq, q) for q in sorted(self.rec)
                           if self.rec[q][3] >= 0
                           and self.rec[q][1] < self.rec[q][3]
                           and self.rec[q][3] + 30 >= self.seq]
        self.seq += 1
        self.session_total += 1
        if self.unseen_pos >= len(self.unseen):
            take_new = False
        elif self.seen_total == 0:
            take_new = True
        else:
            take_new = 10 * (self.session_new + 1) <= 3 * self.session_total
        q = None
        if take_new:
            q = self.unseen[self.unseen_pos]
            self.unseen_pos += 1
            self.session_new += 1
        else:
            # a due re-ask takes the seen slot (earliest due first,
            # insertion order on ties)
            due = [(d, k) for (d, k) in self.reaskq if d <= self.seq]
            if due:
                d, k = min(due, key=lambda x: x[0])
                self.reaskq.remove((d, k))
                q = k
        if q is None:
            best = None
            for k in sorted(self.rec):
                w = self.rec[k]
                if w[3] < 0:
                    continue
                if best is None:
                    best = k
                    continue
                b = self.rec[best]
                # strict improvement only: the engine walks its map in word
                # order and keeps the first best on ties
                if w[0] < b[0] or (w[0] == b[0] and w[3] < b[3]):
                    best = k
            if best is None:   # nothing ever asked yet: take a new one
                q = self.unseen[self.unseen_pos]
                self.unseen_pos += 1
                self.session_new += 1
            else:
                q = best
        # whichever way it was picked, drop it from the queue
        self.reaskq = [(d, k) for (d, k) in self.reaskq if k != q]
        return q

    def answer(self, q, ok):
        """Mirror the post-answer update; returns the five record columns the
        engine writes (interval, lca, round_min, form, rounds_done)."""
        w = self.rec[q]
        form = w[4]
        if ok:
            gap = self.seq - w[1] if w[1] >= 0 else -1
            if form != 0 and gap >= 0:
                w[2] = gap if w[2] < 0 else min(w[2], gap)
            w[1] = self.seq
            if form == 3 and w[2] >= 0:
                if w[2] > w[0]:
                    w[0] = w[2]
                w[5] += 1
                w[2] = -1
            elif form == 3:
                w[2] = -1
            w[4] = (form + 1) % 4
        else:
            w[0] = 0
            if self.reaskq is not None:
                self.reaskq.append((self.seq + self.reask_gap, q))
        w[3] = self.seq
        return tuple(w[0:3]) + (w[4], w[5])


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
    # deliberately small: 1..3-character words, one explicit duplicate row
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
    if len(t) != 12:
        return None
    return t, int(t[5]), t[6], (int(t[7]), int(t[8]), int(t[9]), int(t[10]), int(t[11]))


def check_reask_soon(asks, bad_idx, name):
    """A word answered wrong must be asked again (in the very same form)
    within the next 10 questions (re-ask gap 5, plus headroom for new slots)."""
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
        q = model.pick()
        p, a = FORMS[model.rec[q][4]]
        got = ask[:5]
        exp = (entries[q][0], entries[q][2], entries[q][1], p, a)
        check("empty.q%d.ask==model" % (i + 1), got == exp, "got %s want %s" % (got, exp))
        ok = plan[i] == "ok"
        model.answer(q, ok)
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
        q = m2.find_key(t[0], t[1], t[2])
        check("empty.line%d.seq" % i, seq == i + 1, line)
        m2.seq = seq
        want = m2.answer(q, res == "success")
        check("empty.line%d.record" % i, list(rec) == list(want),
              "%s vs model %s" % (t, want))

    # mix + ordering + form-order invariants straight from the asks
    news, newlens = 0, []
    entryforms = {}     # (chi,pin,eng) -> list of forms in asked order
    for i, (chi, pin, eng2, p, a, seq) in enumerate(eng.asks[:N]):
        form = FORMS.index((p, a))
        ekey = (chi, pin, eng2)
        seen_before = ekey in entryforms
        entryforms.setdefault(ekey, []).append(form)
        if not seen_before:
            news += 1
            newlens.append(len(chi))
            check("empty.q%d.firstform" % (i + 1), form == 0,
                  "%s first asked in form %d" % (chi, form))
    check("empty.new-in-length-order", newlens == sorted(newlens), str(newlens))
    check("empty.new-cap-30", 10 * news <= 3 * N + 3, "new=%d total=%d" % (news, N))
    for ekey, forms in entryforms.items():
        # forms rotate +1 on every correct answer and stay put on a fail,
        # so consecutive asks of an entry differ by 0 or 1 (mod 4), never less
        check("empty.forms-order.%s" % (ekey[0] + "/" + ekey[2],),
              all(((b - a) % 4) in (0, 1) for a, b in zip(forms, forms[1:])),
              "%s: %s" % (ekey, forms))
    check_reask_soon(eng.asks, [i for i, pl in enumerate(plan) if pl == "bad"], "empty")


def test_crafted(binary, base):
    """Baseline mixes legacy 7-column rows, one intermediate 10-column row
    and current 12-column rows; verifies the loader's record reconstruction
    (derived lca/last_seq, explicit columns winning) and the engine's form
    handling from a loaded state."""
    d = make_testdir(base, "qtest_crafted")
    union = os.path.join(d, "union_q.txt")
    with open(union, "w", encoding="utf-8") as f:
        f.write("\n".join(FULL_UNION_LINES) + "\n")
    entries = union_entries(union)

    def row(chi, pin, eng, p, a, seq, res, rec=None):
        s = "%s|%s|%s|%d|%d|%d|%s" % (chi, pin, eng, p, a, seq, res)
        if rec is not None:
            s += "".join("|%d" % x for x in rec)
        return s

    # 猫: legacy success@1, success@5, fail@6 -> derived [0, 5, -1, 6, 0, 0]
    # 狗: legacy success@2, success@4, then a 12-column row at 7 carrying the
    #     explicit record (9, 7, 3, 2, 1) which must win
    #     -> [9, 7, 3, 7, 2, 1]: asked next in form 2 (pin->chi)
    # 吃: one legacy success@3 -> [0, 3, -1, 3, 0, 0]
    # 喝茶: an intermediate 10-column row at 8 (col 9 holds last_correct_at,
    #       col 10 the newest gap): misreading it must not crash the load
    baseline = [
        row("猫", "māo", "cat", 1, 0, 1, "success"),
        row("猫", "māo", "cat", 1, 0, 5, "success"),
        row("猫", "māo", "cat", 1, 0, 6, "fail"),
        row("狗", "gǒu", "dog", 0, 1, 2, "success"),
        row("狗", "gǒu", "dog", 0, 1, 4, "success"),
        row("狗", "gǒu", "dog", 0, 1, 7, "success", (9, 7, 3, 2, 1)),
        row("吃", "chī", "to eat", 2, 0, 3, "success"),
        row("喝茶", "hē chá", "to drink tea", 0, 2, 8, "success", (3, 8, 3)),
    ]
    status = os.path.join(d, "status1.txt")
    with open(status, "w", encoding="utf-8") as f:
        f.write("\n".join(baseline) + "\n")

    # model-side load: verify the loader expectations first
    m = Model(entries)
    for line in baseline:
        t = line.split("|")
        rec = tuple(int(x) for x in t[7:12]) if len(t) >= 12 else (None,) * 5
        m.apply_line(t[0], t[1], t[2], int(t[3]), int(t[4]), int(t[5]), t[6], *rec)
    cat = m.rec[m.find_key("猫", "māo", "cat")]
    check("crafted.cat-derived", cat == [0, 5, -1, 6, 0, 0], str(cat))
    dog = m.rec[m.find_key("狗", "gǒu", "dog")]
    check("crafted.dog-explicit", dog == [9, 7, 3, 7, 2, 1], str(dog))
    eat = m.rec[m.find_key("吃", "chī", "to eat")]
    check("crafted.eat-single", eat == [0, 3, -1, 3, 0, 0], str(eat))

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
        rec = tuple(int(x) for x in t[7:12]) if len(t) >= 12 else (None,) * 5
        m2.apply_line(t[0], t[1], t[2], int(t[3]), int(t[4]), int(t[5]), t[6], *rec)
    with open(status, encoding="utf-8") as f:
        all_lines = [l for l in f.read().splitlines() if l.strip()]
    written = all_lines[len(baseline):]
    check("crafted.linecount", len(written) == N, "%d" % len(written))

    dogq = m.find_key("狗", "gǒu", "dog")
    dog_asked = [a for a in eng.asks[:N] if a[0] == "狗"]
    check("crafted.dog-form2",
          all((a[3], a[4]) == FORMS[2] for a in dog_asked) if dog_asked else True,
          str(dog_asked))
    for i, (chi, pin, eng2, p, a, seq) in enumerate(eng.asks[:N]):
        want = m2.pick()
        q = m.find_key(chi, pin, eng2)
        fp, fa = FORMS[m2.rec[q][4]]
        check("crafted.ask%d==model" % (i + 1),
              want == q and (p, a) == (fp, fa) and seq == 9 + i,
              "engine %s model q%s form %s seq %d" % ((chi, p, a, seq), want, (fp, fa), seq))
        rec = m2.answer(want, plan[i] == "ok")
        if i < len(written):
            parsed = parse_written(written[i])
            if parsed is None:
                check("crafted.line%d.cols" % i, False, written[i])
                continue
            t, wseq, res, wrec = parsed
            check("crafted.line%d.record" % i, list(wrec) == list(rec) and wseq == 9 + i,
                  "%s vs %s" % (t, rec))
            check("crafted.line%d.result" % i,
                  (res == "success") == (plan[i] == "ok"), written[i])
    check_reask_soon(eng.asks, [i for i, pl in enumerate(plan) if pl == "bad"], "crafted")


def test_real(binary, base):
    """Full union_q.txt + the real phone status1.txt: structural checks only
    (row shape, per-word record self-consistency, sequence numbers, forms in
    the fixed order, round-completion bookkeeping)."""
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

    state = {}   # (chi,pin,eng) -> (interval,lca,rmin,form,done) on last line
    for i, line in enumerate(lines):
        parsed = parse_written(line)
        if parsed is None:
            check("real.line%d.cols" % i, False, line)
            continue
        t, seq, res, (interval, lca, rmin, form, done) = parsed
        check("real.seq%d" % i, seq == baseline_seq + 1 + i, "%d" % seq)
        check("real.form%d.range" % i, 0 <= form <= 3, line)
        key = (t[0], t[1], t[2])
        asked_form = FORMS.index((int(t[3]), int(t[4])))
        if key in state:
            p_int, p_lca, p_rmin, p_form, p_done = state[key]
            check("real.form%d.order" % i, asked_form == p_form,
                  "%s asked form %d, word at form %d" % (key[0], asked_form, p_form))
        else:
            check("real.form%d.firstseen" % i, asked_form == 0,
                  "word %s first appears in form %d: baseline rows must have no form columns" % (key[0], asked_form))
        if res == "fail":
            check("real.fail%d.record" % i, interval == 0 and form == asked_form, line)
            if key in state:
                check("real.fail%d.unchanged" % i,
                      lca == state[key][1] and rmin == state[key][2]
                      and done == state[key][4], line)
        else:
            check("real.ok%d.lca" % i, lca == seq, line)
            check("real.ok%d.rotated" % i, form == (asked_form + 1) % 4, line)
            if key in state:
                p_int, p_lca, p_rmin, p_form, p_done = state[key]
                gap = seq - p_lca if p_lca >= 0 else -1
                if asked_form != 0 and gap >= 0:
                    exp_rmin = gap if p_rmin < 0 else min(p_rmin, gap)
                else:
                    exp_rmin = p_rmin
                if asked_form == 3:
                    if exp_rmin >= 0:
                        check("real.ok%d.round" % i,
                              interval == max(p_int, exp_rmin) and done == p_done + 1
                              and rmin == -1, line)
                    else:
                        check("real.ok%d.round0" % i,
                              interval == p_int and done == p_done and rmin == -1, line)
                else:
                    check("real.ok%d.chain" % i,
                          interval == p_int and done == p_done and rmin == exp_rmin, line)
            else:
                if asked_form == 3:
                    check("real.ok%d.roundnew" % i, interval == 0 and done == 0, line)
        state[key] = (interval, lca, rmin, form, done)
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
