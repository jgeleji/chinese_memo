#!/usr/bin/env python3
# Generate the writing-practice data files for the chinese write app.
#
# Sources (all local):
#   ~/cpp/makemeahanzi/graphics.txt   per-stroke median polylines (0..~1024 units, y UP)
#   ~/cpp/makemeahanzi/dictionary.txt IDS decomposition (⿰⿱... ) per character
#   union_q.txt                       the word base (chinese|english per line)
#   data/general-standard?page=N      per-character pinyin (same source the quiz uses)
#
# Outputs (committed, shipped in the APK as assets):
#   write_data.txt    one line per character:
#       <char>\t<ids-or-*>\t<comp-per-stroke>\t<x y> <x y>...  (tab-separated)
#     ids: space-separated IDS tokens ("⿰ 女 子") or * when no decomposition
#     comp-per-stroke: comma-separated component index per stroke (from
#       dictionary.txt "matches", only when every entry is a single index),
#       or - ; the app uses it to draw one component in its own sub-box
#     first line: #G <minx> <miny> <maxx> <maxy>  global bounding box for scaling
#   write_pinyin.txt  <char>\t<pinyin> for every character we know a reading for
#
# Included characters: every hanzi in the word base PLUS every direct single-char
# component of their decompositions (needed to draw components in split mode).

import json, re, sys, os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MMH = os.path.expanduser("~/cpp/makemeahanzi")

OPS2 = set("\u2ff0\u2ff1\u2ff4\u2ff5\u2ff6\u2ff7\u2ff8\u2ff9\u2ffa\u2ffb\u2ffC")  # 2-child
OPS3 = set("\u2ff2\u2ff3")  # 3-child
IDS = OPS2 | OPS3

def parse_ids(s, i=0):
    """parse one IDS node; returns (node, next_index). node: char | None | (op, [nodes])"""
    if i >= len(s):
        return None, i
    c = s[i]
    n = 2 if c in OPS2 else (3 if c in OPS3 else 0)
    if n:
        kids = []
        i += 1
        for _ in range(n):
            k, i = parse_ids(s, i)
            kids.append(k)
        return (c, kids), i
    return (None if c == "？" else c), i + 1

def ids_tokens(node, out):
    if node is None:
        out.append("？")
    elif isinstance(node, str):
        out.append(node)
    else:
        out.append(node[0])
        for k in node[1]:
            ids_tokens(k, out)

def main():
    # ---- word-base characters
    chars = set()
    for ln in open(os.path.join(REPO, "union_q.txt"), encoding="utf-8"):
        for part in ln.rstrip("\n").split("|"):
            for c in part:
                if "\u4e00" <= c <= "\u9fff" or "\u3400" <= c <= "\u4dbf":
                    chars.add(c)

    decomp = {}
    matches_map = {}
    for line in open(os.path.join(MMH, "dictionary.txt"), encoding="utf-8"):
        d = json.loads(line)
        decomp[d["character"]] = d.get("decomposition") or ""
        matches_map[d["character"]] = d.get("matches") or []

    # direct single-char components only (the app draws one split level; nested
    # groups fall back to whole-character drawing)
    need = set(chars)
    ids_map = {}
    flat_children = {}
    for c in chars:
        dec = decomp.get(c, "")
        if not dec or dec[0] not in IDS:
            continue
        node, _ = parse_ids(dec)
        if node is None or not isinstance(node, tuple):
            continue
        toks = []
        ids_tokens(node, toks)
        if "？" in toks:
            continue  # incomplete decomposition
        ids_map[c] = toks
        flat_children[c] = len(node[1])
        for k in node[1]:
            if isinstance(k, str):
                need.add(k)

    # ---- stroke medians (points are [x,y] arrays; older variants use dicts)
    def pt(p):
        return (p["x"], p["y"]) if isinstance(p, dict) else (p[0], p[1])
    strokes = {}
    for line in open(os.path.join(MMH, "graphics.txt"), encoding="utf-8"):
        d = json.loads(line)
        if d["character"] in need:
            strokes[d["character"]] = [[pt(p) for p in poly] for poly in d["medians"]]
    missing = sorted(need - set(strokes))
    if missing:
        sys.exit("FATAL: no stroke data for: " + "".join(missing))

    minx = min(p[0] for polys in strokes.values() for poly in polys for p in poly)
    maxx = max(p[0] for polys in strokes.values() for poly in polys for p in poly)
    miny = min(p[1] for polys in strokes.values() for poly in polys for p in poly)
    maxy = max(p[1] for polys in strokes.values() for poly in polys for p in poly)

    comp_ok = set()
    for c in need:
        m = matches_map.get(c, [])
        if c in flat_children and len(m) == len(strokes[c]) and \
                all(isinstance(e, list) and len(e) == 1 and 0 <= e[0] < flat_children[c] for e in m):
            comp_ok.add(c)

    with open(os.path.join(REPO, "write_data.txt"), "w", encoding="utf-8") as f:
        f.write("#G %d %d %d %d\n" % (minx, miny, maxx, maxy))
        for c in sorted(need):
            ids = " ".join(ids_map[c]) if c in ids_map else "*"
            comp = "-"
            if c in comp_ok:
                comp = ",".join(str(e[0]) for e in matches_map[c])
            polycls = []
            for poly in strokes[c]:
                polycls.append(" ".join("%d %d" % p for p in poly))
            f.write("%s\t%s\t%s\t%s\n" % (c, ids, comp, "\t".join(polycls)))
    print("write_data.txt: %d chars (%d with IDS split, %d with stroke->component map)" %
          (len(need), len(ids_map), len(comp_ok)))

    # ---- pinyin (same table the quiz driver used)
    pref = {}
    row_re = re.compile(r'^<tr><td><a href="/character/([^"]+)"')
    for i in range(1, 83):
        p = os.path.join(REPO, "data", "general-standard?page=%d" % i)
        try:
            fh = open(p, encoding="utf-8")
        except Exception:
            continue
        for line in fh:
            m = row_re.match(line)
            if not m:
                continue
            m2 = re.search(r'</td><td>(?:<span[^>]*>)?([^<]*)', line)
            py = m2.group(1).strip() if m2 else ""
            if py:
                pref[m.group(1)] = py
        fh.close()
    for ch, raw in [("谁", "shei2"), ("家", "jia1"), ("吗", "ma"),
                    ("么", "me"), ("哪儿", "na")]:
        pref.setdefault(ch, raw)
    n = 0
    ok_py = re.compile(r"[A-Za-z0-9 üāáǎàēéěèīíǐìōóǒòūúǔùǖǘǚǜ]+\Z")
    with open(os.path.join(REPO, "write_pinyin.txt"), "w", encoding="utf-8") as f:
        for c in sorted(need):
            if c in pref and ok_py.match(pref[c]):
                f.write("%s\t%s\n" % (c, pref[c]))
                n += 1
    print("write_pinyin.txt: %d/%d chars" % (n, len(need)))

main()
