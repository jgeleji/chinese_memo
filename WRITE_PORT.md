# chinese write — Android port & test context (writing-practice app)

Feed this document (plus the repo) to a fresh agent session. It records
everything needed to build, test and deploy the **chinese write** app
(character-writing practice with stroke order, graded from finger strokes).
The quiz app's own context doc is `ANDROID_PORT.md` — machines, JDK/Gradle/SDK
paths, adb and the phone serials are listed there and are NOT repeated here.

---

## 1. What this app is

- Standalone SDL2 app for learning to WRITE characters: the app shows the
  stroke skeleton (LEARN) or nothing (QUIZ), the user draws each stroke with a
  finger, each stroke is graded against Make Me a Hanzi median data.
- Single C++ file: `write_sdl.cpp` (~1800 lines). It deliberately carries its
  OWN copy of the render/font-atlas/frame-dump blocks from `input_sdl2.cpp`
  and shares NO code with the quiz engine — changes to one app never break
  the other.
- Android: module `android/appwrite`, package `com.geleji.chinesewrite`,
  native lib `libchinesewrite_sdl.so` (entry via `main_sdl_write.cpp`, which
  `#include`s write_sdl.cpp with `main` renamed — same pattern as
  `main_sdl_questions10.cpp`). No keyboard/IME plumbing at all (touch-only).
- Progress file: **`wstatus.txt`** (status2.txt belongs to questions10hsk).
  It lives only in the app's files dir; never shipped inside the APK.

## 2. Data files and regeneration

- `write_data.txt` — one line per character:
  `char \t "⿰ 女 子"|* \t "0,0,0,1,1,1"|- \t stroke1-points \t stroke2 ...`
  points are `"x y"` pairs, y UP, 0..1024 scale (Make Me a Hanzi medians).
  Header line `#G minx miny maxx maxy` (global glyph bounds).
- `write_pinyin.txt` — `char \t pinyin(syllables)`, best-effort (1240/1337).
- Both are GENERATED, do not edit by hand:
  `python3 tools/gen_write_data.py` re-reads `~/cpp/makemeahanzi/graphics.txt`
  (medians), `dictionary.txt` (IDS + strokeMatches → per-stroke component map)
  and this repo's `data/general-standard?page=N` HTML for pinyin.
  Sources of truth stay outside the repo; the generated files are committed.
- Word base: same `union_q.txt` as the quiz app (`chinese|english` lines);
  words with all characters present in write_data.txt form the pool.

## 3. Modes and screen flow

- Buttons (top row, computed in `compute_layout`): `LEARN|QUIZ` (skeleton
  shown or not), `SPLIT ON|OFF` (component mode), `NEXT`.
- Whole-char mode: draw the strokes in order inside one 米字格 box.
- Split mode (IDS ⿰⿱⿲⿳ only, single-char children with stroke data, and only
  when the per-stroke component map exists; else falls back to whole-char):
  1. PICK phase: tap the components in writing order — candidates are the
     real parts plus up to 2 dummies, shuffled. Wrong taps are rejected.
  2. DRAW phase: each component's strokes in its own sub-box (vertical stack
     for ⿱⿳, side-by-side for ⿰⿲). `enter_next_scope_or_done` advances only
     when a scope's strokes are ALL done (it once advanced per stroke — a
     real bug the wdriver now guards against).

## 4. Grading (per stroke, S = current box size in px)

- start point within 15% of the reference start,
- chord direction within 15° PLUS a bonus equal to the reference stroke's own
  curvature (bend from the median polyline, capped at +20°) — curves like 撇
  are not punished for the chord,
- chord length within 15% of the reference chord,
- fallback: resample both polylines to 16 points, mean distance ≤ 15% passes,
- dots (reference path < 6%S): start must land within 18%S, path ≤ 30%S,
- anything with path < 10%S is rejected as "too short".
All tolerances are percent of the character box width, per the design notes.

## 5. Headless PC testing (the contract to keep stable)

- Build: `cmake -DBUILD_SDL_PC=ON <repo>` then `make write_sdl`
  (desktop exe; Android uses the `chinesewrite_sdl` shared lib instead).
- Hooks (env): `CM_WINDOW=WxH`, `CM_SEED=n` (deterministic question order),
  `CM_STATE_FILE=f` (state line rewritten every repaint),
  `CM_KEYS_FILE=f` (scripted commands), `CM_DUMP_DIR=d` (frame PNGs),
  `CM_MAXFRAMES=n`.
- State line (single line, space-separated fields):
  `WAIT seq=N phase=draw|pick|done stroke=i/n comp=c/k frame=F box=x,y,s
  mode=learn|quiz split=on|off ok=T fail=F word= char= pinyin=
  [pick=i/k cands=女@x,y,s;...]` — `comp=0/0` means whole-char mode; `box` is
  the sub-box the user must draw in (the driver mirrors the app's Mapper math
  from it, y is flipped: `sc=0.88*s/max(dx,dy)`).
- Keys file commands: `@WAIT substr` (blocks until the state line contains
  substr; one command per poll cycle, later lines stay queued),
  `@TAP x,y`, `@STROKE x,y x,y ...` (injected through the same capture +
  grading path as a finger), `@QUIT` (writes wstatus.txt, exits 0).
- Driver: `python3 tools/wdriver.py [--seed N] [--dump]` — 22 scripted checks
  (whole-char exact strokes, reversed/too-short rejection + recovery, split
  pick order + per-component drawing, QUIZ toggle, NEXT counter preservation,
  wstatus rows). Runs against any seed: `--seed 42|7|2024` all pass.
  It pkills stale instances first and always reaps its child (a leftover
  instance races on the same state/keys paths).
- Debug build: `-DCMAKE_CXX_FLAGS=-DCM_DEBUG` adds stderr logs of every keys
  line, tap hit-test, stroke injection and grade verdict.

## 6. Android build & deploy

```bash
cd ~/Documents/chinese_memo/android
JAVA_HOME=$HOME/tools/jdk-17.0.13+11 ANDROID_HOME=$HOME/android-sdk \
  $HOME/tools/gradle-8.7/bin/gradle :appwrite:assembleDebug --no-daemon
$HOME/android-sdk/platform-tools/adb install -r \
  appwrite/build/outputs/apk/debug/appwrite-debug.apk
```
- MIUI may answer with `INSTALL_FAILED_USER_RESTRICTED: Install canceled by
  user` — a confirmation popup needs a tap on the phone, then retry.
- Assets unpacked by MainActivity into files dir at first start:
  `union_q.txt write_data.txt write_pinyin.txt NotoSansSC.ttf`
  (marker `.assets_version` keyed to apkInstallTime).
- Verified on `e1fc29d2`: boots at 1280x2772, `chinese_write: chars=1337
  words=1862 pool=541`, pick screen renders correctly.
- Copy the APK for manual installs:
  `cp android/appwrite/build/outputs/apk/debug/appwrite-debug.apk ~/Downloads/chinese_write-debug.apk`

## 7. Progress sync

`./progress_pull.sh write` pulls `wstatus.txt` from the phone into the repo
(`quiz` pulls status1.txt as before; `all` does both). Reinstalling the app
wipes the phone copy — push one back with
`adb push wstatus.txt /sdcard/` + `run-as` copy if needed.
