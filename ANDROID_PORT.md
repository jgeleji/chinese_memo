# Android port & deployment context (chinese_memo and future console-C++→Android ports)

Feed this document (plus the repo) to a fresh agent session. It records everything
needed to (a) build, install and sync chinese_memo on the phones over USB, and
(b) port any other console C++ app to a fullscreen Android app using the same
SDL2 pattern. All facts here were verified against this repo and the real phones.

NOTE: the repo now holds a second app, **chinese write** (writing practice) -
its own context doc is `WRITE_PORT.md`; this file's machine/tool/device facts
apply to both.

---

## 1. Machines, tools, devices

| thing | value |
|---|---|
| build PC | this Linux box; repo at `~/Documents/Default Project/chinese_memo` |
| JDK | `~/tools/jdk-17.0.13+11` |
| Gradle | `~/tools/gradle-8.7/bin/gradle` (no wrapper in repo) |
| Android SDK | `~/android-sdk` (also written in `android/local.properties`) |
| adb | `~/android-sdk/platform-tools/adb` |
| NDK | `27.0.12077973` (pinned in `android/app/build.gradle`) |
| CMake for gradle | `3.22.1` (pinned in build.gradle; SDK has its own) |
| app phone | serial `e1fc29d2`, MIUI, debuggable (`run-as` works), package `com.geleji.chinesememo` |
| console phone | serial `2a7744760105`, Termux, game at `/data/data/com.termux/files/home/cpp/chinese/chinese` |
| app phone display | surface 1280x2620 usable, MIUI secure-keyboard inset **1177px** (keyboard top y≈1443), content must end by y≈1352 |

Non-interactive git push (no prompts ever):
```bash
GIT_TERMINAL_PROMPT=0 GIT_ASKPASS=/bin/true git push -q origin main
```

## 2. Build + upload APK to the phone (USB)

Everything lives under `chinese_memo/android/`. One command builds:

```bash
cd "chinese_memo/android"
JAVA_HOME=$HOME/tools/jdk-17.0.13+11 ANDROID_HOME=$HOME/android-sdk \
  $HOME/tools/gradle-8.7/bin/gradle assembleDebug --no-daemon
# output: app/build/outputs/apk/debug/app-debug.apk (~14MB)
```

Install + relaunch + verify (USB, device authorized):

```bash
ADB=$HOME/android-sdk/platform-tools/adb
$ADB -s e1fc29d2 install -r app/build/outputs/apk/debug/app-debug.apk   # prints "Success"
cp app/build/outputs/apk/debug/app-debug.apk ~/Downloads/chinese_memo-debug.apk  # keep a copy

# MIUI QUIRK: after adb install MIUI often immediately PAUSES the running activity.
# Always relaunch manually and check it is alive:
$ADB -s e1fc29d2 shell am start -n com.geleji.chinesememo/.MainActivity
$ADB -s e1fc29d2 shell pidof com.geleji.chinesememo        # a pid = running
```

Logs (all native `std::cout`/stderr and the Java watcher are tagged):
```bash
$ADB -s e1fc29d2 logcat -c
$ADB -s e1fc29d2 logcat -d -s chinese_memo
# startup must log: "assets unpacked into /data/user/0/com.geleji.chinesememo/files"
#                   "keyboard height: 1177px"  (0 then 153 then 1177 is normal)
```

**Screencap does not work on this phone** (`adb exec-out screencap` returns a black
frame for this app — secure surface). To *see* what the phone renders, use the app's
own frame dumps (section 5) or read back files via `run-as`.

### Progress-file round trip (`status1.txt`)

The repo's `status1.txt` is the master of study progress; the phone writes its copy
under internal files. After every study session:

```bash
./progress_pull.sh && git commit -am status   # then the non-interactive push
```

`progress_pull.sh` does `adb exec-out "run-as com.geleji.chinesememo cat files/status1.txt"`
→ repo `status1.txt` (keeps a `.pre-pull` backup). The next APK build packages the
pulled file back in (assets sync, section 3), so PC build and phone never diverge.
Console-phone pool sync (separate): `cd ~/cpp/chinese/chinese; rm -rf data union_q.txt; git pull`.

Row format (one line per answered question, written immediately after each answer):

```
chinese|pinyin|english|provided|asked|seq|success|interval|last_correct_at|round_min|form|rounds_done
```

The first seven columns are the classic format (DATATYPE numbers: 0=chinese,
1=english, 2=pinyin; seq = global question number; provided/asked say which
*form* of the word this question was, see below). The last five columns are
the word's gap record (see below). **Old 7-column files load unchanged** —
the loader derives the record from the success/fail history — so the phone's
existing progress survives (a handful of intermediate 10-column rows written
by one short-lived build are treated as legacy; harmless). Phone rows often
carry empty pinyin/english: the loader then falls back to the first
registered variant of that word (as it always did).

### Question selection: gap mode (`ask_gap_mixed`), word-centric

`main_questions10.cpp` (the only ported quiz mode; PC twin `questions10_sdl`,
Android `:app`) calls `q.ask_gap_mixed(...)`. The old block10 mode
(`ask_all_until_fail_block10`) is untouched for the other console mains.

**One unit = one word** (a `loaded_data` entry). Its four forms count as one
question asked in different shapes, always in this fixed order (`GAP_FORMS`):

```
form 0: chinese -> pinyin     form 1: chinese -> english
form 2: pinyin  -> chinese    form 3: english -> chinese   (hardest last)
```

(the two forms without chinese, english<->pinyin, are not asked: they only
exercise pinyin spelling, which form 0 already covers). A word is asked in
its *next* form; the form pointer rotates on every correct answer and stays
put on a mistake. One form per scheduling turn also keeps the forms of one
word naturally spaced apart (one pool pass).

Per word the engine keeps
`gap_word{interval, last_correct_at, round_min, last_seq, form, rounds_done}`:

- correct answer → the gap `now - last_correct_at` (any form's previous
  correct answer) is folded into `round_min` (running minimum of the
  correct→correct gaps of the current round) — except form 0, which *opens*
  a round and records no gap. When form 3 is answered correctly the round
  completes: `interval = max(interval, round_min)` — the **completed
  distance** (the whole 4-form chain must have been held that long) —
  `rounds_done++` and `round_min = -1`
- wrong answer → `interval = 0`; the round chain continues (the re-ask's
  small gap honestly shrinks that round's distance)
- `last_seq` = when any form was last asked; -1 means never asked → new word

Selection each question (fully deterministic — no RNG shuffles):

- ~30% come from the new-word pool, in order of increasing word length
  (unicode code points), ties by data order. Rule: take new iff
  `10*(session_new+1) <= 3*session_total` → new share never exceeds 30%
  (first new lands at question 4 of a session)
- ~70% come from the seen pool: argmin of (interval asc, last_seq asc) over
  the word map — weakest demonstrated completed distance first, oldest-asked
  on ties (first word in map order wins exact ties)
- **re-ask queue**: a word answered wrong is re-asked in the same form 5
  questions later (`REASK_GAP`), taking the seen slot ahead of the argmin
  pick; the on-screen stat line then shows the extra word `re-ask`. At
  session start, words whose last attempt ended in a mistake
  (`last_correct_at < last_seq`) and were asked within the last 30 questions
  are seeded into the queue, so a session interrupted right after a miss
  picks up where it left off
- the queue needs no new status columns (the condition derives from the
  stored record)

On-screen stat line (small, one line above the question):
`answered_once/words_total >=1:n >=2:n >=4:n ... q<seq> [re-ask]` —
words answered correctly at least once out of all words, then for each power
of two the number of words whose *completed distance* (round distance of a
finished four-form round) is at least that; recomputed per question over the
in-memory map (~1900 entries, cheap).

## 3. How this app is built (the SDL2 pattern, component by component)

Data flow: `console C++ (std::cout + getch)` → same engine compiled as
`libquestions_sdl.so` → SDL2 draws the captured console text → hosted by a
SDLActivity Java shell.

```
chinese_memo/
├── CMakeLists.txt                 shared build for desktop AND Android
├── third_party/
│   ├── SDL2-2.30.9/               vendored SDL2 sources (built from source)
│   ├── stb_truetype.h, stb_image_write.h
├── input_sdl2.cpp                 THE port: whole SDL backend (this is where 95% of port work happens)
├── input_common.cpp, utils.cpp, tokenize.cpp, questions.cpp    engine, untouched by the port
├── main_sdl_questions10.cpp       SDL_main entry
├── android/
│   ├── app/build.gradle           NDK/CMake wiring + asset sync
│   ├── app/src/main/AndroidManifest.xml
│   ├── app/src/main/java/org/libsdl/app/*.java    vendored SDL2-for-Android java (patched, see IME)
│   └── app/src/main/java/com/geleji/chinesememo/MainActivity.java   the only app-specific java
└── assets/NotoSansSC.ttf          CJK font shipped with the app
```

### CMakeLists.txt branches

Console targets are wrapped in `if(NOT ANDROID)`. Then:

```cmake
option(BUILD_SDL_PC "...")            # desktop SDL build for headless testing
if(ANDROID OR BUILD_SDL_PC)
    if(ANDROID)  set(SDL_SHARED ON ...)  else() set(SDL_STATIC ON ...) + disable X11/wayland/alsa/... endif()
    add_subdirectory(third_party/SDL2-2.30.9 SDL2_build)   # SDL compiled from source, both platforms
    if(ANDROID)
        add_library(<app>_sdl SHARED main_sdl_*.cpp engine...cpp input_sdl2.cpp input_common.cpp ...)
        target_link_libraries(<app>_sdl SDL2::SDL2)
    else()
        add_executable(<app>_sdl ...same sources...) ; link SDL2::SDL2-static SDL2::SDL2main
    endif()
endif()
```

Key points:
- SDL is **built from source** (vendored tarball). No prebuilt .so, no Maven SDL AAR.
- The `SDL_main` symbol in `main_sdl_*.cpp` becomes the entry point because the game
  library is loaded **last** by SDLActivity.
- Desktop SDL target exists on purpose: it is the headless test rig (section 5) — build
  the *identical* rendering code without a phone.

### gradle (`android/app/build.gradle`) essentials

```groovy
ndkVersion '27.0.12077973'
compileSdk 34; minSdk 21; targetSdk 34
externalNativeBuild { cmake {
    arguments '-DANDROID_STL=c++_shared', '-DANDROID_PLATFORM=android-21'
    abiFilters 'arm64-v8a'          // arm64 phone only — build stays fast
    targets 'questions_sdl'         // ONLY build the lib gradle needs, not the console targets
    cppFlags '-std=c++11'
}}
externalNativeBuild { cmake { path new File(cmRoot, 'CMakeLists.txt'); version '3.22.1' } }
```

`cmRoot` = the directory holding CMakeLists.txt (gradle project lives in a subdir).
A `syncAppAssets` Copy task copies the game's data files (`union_q.txt`,
`status1.txt`, `assets/NotoSansSC.ttf`, `data/`) into `app/src/main/assets`
before every asset merge — so progress/data in the repo are what ships in the APK.

`data2/` (every character's ALTERNATE readings — 仔 is listed `zǐ` in
general-standard but reads `zǎi` in 牛仔裤) ships too, but as ONE tar: a
`packData2` Exec task runs `tar -cf assets/data2/data2.tar -C <cmRoot> data2`
and MainActivity untars it on device with `/system/bin/tar`. Reasons:
copying 8105 individual assets through Gradle took minutes and MIUI froze the
worker mid-copy; and Gradle's own file machinery cannot even stat the 196
non-BMP (above U+FFFF) file names — plain tar handles them. On-device untar
takes seconds and is verified complete (`tar -tf | wc -l` vs dir listing,
re-tarred in a loop until counts match; incomplete => no marker => retry next
launch). One observed ProcessBuilder tar run silently extracted 7794/8105
entries with rc=0, which is what the count-check guards against.

### AndroidManifest.xml essentials

```xml
android:screenOrientation="portrait"
android:windowSoftInputMode="stateAlwaysVisible|adjustPan"   <!-- keyboard never hidden -->
android:configChanges="layoutDirection|locale|orientation|uiMode|screenLayout|screenSize|smallestScreenSize|keyboard|keyboardHidden|navigation"  <!-- SDL never gets restarted -->
android:launchMode="singleInstance"
<uses-feature android:name="android.hardware.touchscreen" android:required="true"/>
```

`adjustPan` + full `configChanges` = no activity recreation, no resize dance; the
native side does its own layout against the measured keyboard height instead.

### MainActivity.java (the only Java you write)

- `extends org.libsdl.app.SDLActivity`
- `getLibraries()` → `{"SDL2", "<app>_sdl"}` — game library **last** (its SDL_main wins)
- `onCreate`: before `super.onCreate()`, unpack the assets once into `getFilesDir()`
  (`copyAssetsIfNeeded()`), because the native code does `chdir(getFilesDir())` and
  then opens its data with **relative paths** — the console app's file handling is
  preserved unchanged.
- `FLAG_KEEP_SCREEN_ON`.
- **IME height watcher**: a `OnGlobalLayoutListener` measures real keyboard height two
  ways — `WindowInsets.Type.ime()` (API 30+, exact) and the old visible-display-frame
  estimate — takes the max, **max-tracks within a session** (MIUI silently swaps
  keyboards of different heights; a covered button also swallows taps, which is why
  reserved space only ever grows), and writes it to `files/cm_ime_height`.
  The native layout reads that file each frame and keeps everything above it.

### Vendored `org/libsdl/app` (patched!)

SDL2-for-Android java is vendored as source, and `SDLActivity.java` is patched
around line ~1942:

```java
inAttrs.inputType = InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD |
                    InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS | ...;
outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI | ...;
```

Why (user requirement, hard-won): the app only ever needs plain latin/pinyin keys.
Without this the phone's IME offers Chinese composition and autocomplete and the
user types Han characters into a latin answer; some Chinese IMEs also open the
full-screen extract UI. These attrs pin every keyboard to plain direct input.
**If you update or re-vendor SDLActivity.java, re-apply this patch.**
Side effect: MIUI answers with its "Mi Secure Keyboard" (1177px tall here) — that is
exactly what the IME-height watcher exists to handle.

### input_sdl2.cpp — the port itself (architecture notes)

Everything the console did through `std::cout` and `getch()` is served by this file:

- **stdout capture**: a custom `std::streambuf` installed on `std::cout` (and stderr)
  collects the engine's output into lines; ANSI SGR sequences are parsed into styled
  **runs** (`Run{ text, fg, bg, px }` inside `SLine`) — the engine's colors survive.
  (Gotcha: a color change must *end the current run*, otherwise colored words inside
  a line are lost.)
- **input**: `Input::getch()` equivalents come from SDL touch events → on-screen
  buttons/grid cells (`SDL_FINGERDOWN` → mapped key char), plus the real hardware
  keyboard/IME line for typed answers. pinyin answers need tone digits (`yi3`, `hou4`).
- **screens never "print and scroll"**: each `present()` lays out the whole captured
  buffer to fit the screen (shrink-to-fit size chains, all font sizes kept on
  multiples of 4 — every size gets its own glyph atlas, unbounded probe chains eat
  phone RAM). Text area bottom limit = keyboard band (`cm_ime_height` + margin).
- **blocking points** (`system_pause`, `show_for(ms)`) just keep presenting the last
  frame; the engine code (`questions.cpp`) is otherwise untouched.
- **fonts**: `stb_truetype` loads `NotoSansSC.ttf` (bundled in assets) — CJK+latin.
  Never depend on a system font path.
- **fixed RNG seed for the studied-question selection** (`cm_selection_seed()`,
  default 1337, overridable by `CM_SEED` / `files/cm_seed`) so scripted tests replay
  exactly the same questions; distractors stay fresh per run.

## 4. Port checklist for a NEW console app (copy this project as template)

1. Copy `chinese_memo/android/`, `third_party/` (SDL2-2.30.9 + stb headers), and one
   `main_sdl_*.cpp` from this repo into the new project.
2. In the new `CMakeLists.txt`: wrap console targets in `if(NOT ANDROID)`; add the
   `if(ANDROID OR BUILD_SDL_PC)` branch from section 3 with the new lib name.
3. In `android/`: change `namespace`, `applicationId`, app label/icon, `cmRoot`,
   `targets '<newlib>_sdl'`, `getLibraries() {"SDL2","<newlib>_sdl"}`; edit
   `syncAppAssets` to copy the new game's data files and the font.
   `MainActivity.java`: rename package, change the `files/<app>_ime_height` name,
   keep asset unpacking + IME watcher + KEEP_SCREEN_ON.
   Keep the patched `org/libsdl/app` untouched (IME attrs!).
4. Manifest: copy as-is (portrait, `stateAlwaysVisible|adjustPan`, full configChanges).
5. Write/extend the `input_sdl2.cpp`-style backend for the new app's I/O. If it also
   uses cout+getch, start by copying this file and renaming `cm_` hooks.
6. Native entry: `chdir(getFilesDir())` equivalent (pass it as argv/env from Java, or
   let Java unpack and native chdir like here), keep relative-path file access.
7. Build with gradle (section 2), install `-s e1fc29d2`, relaunch, check logcat tag.
8. Give the new app its own progress round-trip script (copy `progress_pull.sh`,
   change package + file name).

## 5. Headless testing on the PC without a phone (do this before ever installing)

Build the desktop SDL twin (identical rendering code):

```bash
cmake -S . -B build-sdl -DBUILD_SDL_PC=ON
cmake --build build-sdl --target questions10_sdl -j8
```

The backend has scripted-test hooks (env vars, all `CM_*`):

| hook | meaning |
|---|---|
| `SDL_VIDEODRIVER=dummy` | no display needed |
| `CM_WINDOW=WxH` | fake the phone geometry (`1280x2467` mirrors this phone's text area) |
| `CM_IME_BAND=px` | fake keyboard height (real value: `1011` for the text area on e1fc29d2; measured inset 1177 minus bar math) |
| `CM_DUMP_DIR=dir` | write every presented frame as `frameNNN.png` + `frameNNN.txt` (txt = exact text + `<def_px>/<px>` size of every drawn line + verdict-block y positions) |
| `CM_MAXFRAMES=n` | stop after n frames |
| `CM_AUTOKEYS_FILE=f` | driver script **appends** raw keystrokes to f |
| `CM_STATE_FILE=s` | program writes its current state there every frame |
| `CM_SEED=n` | replay a known question sequence |
| `CM_ASK_LOG=f` + `CM_DECIDE_FILE=g` | gap-mode selection driver (see `tools/qdriver.py` below): the engine appends `ASK\|<chi>\|<pin>\|<eng>\|<provided>\|<asked>\|<seq>` per question and blocks until g gains an `ok`/`bad` line; the on-screen ask and all display waits are skipped |

Gap-mode selection has its own headless test driver, **`tools/qdriver.py`** —
run it after any change to `ask_gap_mixed`/`load_gap_status_file` (it mirrors
both in Python and checks the engine question-by-question):

```bash
cmake -S . -B /tmp/opencode/cm_build -DBUILD_SDL_PC=ON
cmake --build /tmp/opencode/cm_build -j8 --target questions10_sdl
python3 -u tools/qdriver.py            # empty crafted real → "N checks passed, 0 failed"
```

- `empty`: tiny hand-written full-field union_q + empty status → exact
  ask-by-ask comparison against the Python model, record columns verified;
  also asserts form discipline (first ask of an entry = form 0, consecutive
  asks of an entry differ by 0 or 1 mod 4 — rotates on correct, holds on fail)
- `crafted`: baseline mixes legacy 7-column rows, one intermediate 10-column
  row and current 12-column rows → derived lca/last_seq, explicit columns win
  (including the `form` pointer: that word is next asked in form 2)
- `real`: full `union_q.txt` + the pulled phone `status1.txt` → structural
  + per-question record consistency over 40 scripted answers (2 fails forced):
  forms asked in order, round bookkeeping (round_min/interval/rounds_done)
- test dirs are built under `/tmp/opencode/qtest/` (assets/data symlinked);
  never point it at the repo dir — status appends
- expected mix on the real baseline: new words at 4,7,10,14,... (~29%)
- `check_reask_soon`: every scripted "bad" must be asked again (same word,
  same form) within the next 10 questions (all three tests assert this)
- engine stderr in test mode prints `gapmode words= seen= unseen= reask= seqmax=`
  (stdout goes to the SDL screen buffer, so this line is the test-visible copy
  of the on-screen "Gap mode:" line)

The scripted-driver protocol (this exact shape, see `/tmp/opencode/cm_fixtest/driver2.sh`
= answer-correctly and `driver3.sh` = force-wrong for question 44821 — they may be gone
in a fresh context; they are small bash loops, rewrite from this description if needed):

- st file line: `WAIT seq=N state=S keys=K frame=F`
- the driver waits for a `WAIT` line, decides keys from the *current* screen
  (`shot/frame{F-1}.txt`), appends keys **at most once per seq value**
- frame dumps take ~0.5s: during that the st file still shows stale `keys=0` —
  never re-send keys for the same seq, that double-answers questions
- keys file is append-only (audit trail), pinyin keys need tone digits

Gotchas that waste hours:
- **Never run the app in the repo directory** — it writes `status1.txt` there.
  Run only from scratch copies (`cp $R/status1.txt /tmp/opencode/<testdir>/ ; cd <testdir>`).
- Test dirs need `assets/NotoSansSC.ttf` copied in, or the binary exits with
  `FATAL: failed to load NotoSansSC.ttf`.
- Never `make all` in build-pc (ncurses targets).
- `pkill -f questions10_sdl` also matches (and kills) the wrapper shell running
  it — use `pkill -x questions10_sdl`. And `A && B && cmd &` backgrounds the
  whole chain, so follow-up inspection commands run in the wrong cwd.
- To verify device layouts: device frames must have bottom content row ≤1352;
  check with the frame .txt (or `/tmp/opencode/bottomrow.py`, a PNG row analyzer).

## 6. Standing rules for this project (do not regress)

- `status1.txt` progress must survive every change; targets are questions10
  (gap mode, `ask_gap_mixed`) only — old modes stay untouched for console mains.
- Gap-mode selection stays deterministic (no RNG shuffles) and the new-question
  share stays ≤30%; a missed question always comes back within ~10 questions.
  Status rows keep backward compatibility (old 7-column files must load).
  Verify with `tools/qdriver.py` before shipping.
- Performance is first-class (phone RAM: bounded font-atlas sizes, no unbounded probes).
- Keyboard: never type Chinese (VISIBLE_PASSWORD attrs), never disappears, never
  covers app buttons (IME band max-tracking + margin `g_h/40`).
- Input conventions to preserve: typing `v` stands for `ü` (phone keyboards have
  no umlaut key; `nv3` = nǚ, done inside `pinyin_flatten_1syll` so every pinyin
  path benefits, literal ü keeps working on console). A lone digit typed as a
  whole syllable is the chinese NUMBER WORD (`pinyin_convert_1syll` expands
  `3`→sān, `2`→èr, `0`→líng, ... before the flatten pass, which would otherwise
  strip it as a tone mark and end the answer): `3角2分` can be typed
  `3 ⏎ jiao3 ⏎ 2 ⏎ fen1 ⏎`; digits inside a syllable stay tone marks (`jiao3`).
  Words whose text contains arabic digits get their pinyin DERIVED
  (`convert_chinese_to_pinyin` passes bytes through), so the stored pinyin
  literally contains digits (`3角2分`→`3jiǎo2fēn`) and the stored chinese
  literally contains `3`. To make such words answerable in every form,
  `datapoint::ask()` canonizes both sides before comparing
  (`canon_pinyin_numbers`: a digit becomes its toned number word UNLESS it
  sits inside an all-ascii syllable, where it stays a legacy tone mark like
  `ping2`; `canon_chinese_numbers`: digit→number char, 〇→零). The chinese
  choice grids filter options by expected characters, so
  `expected_add_number_words()` (input_common.cpp) registers 零一二三四五六七八九
  whenever the expected answer contains a bare digit - the grids then OFFER
  三/二 etc. for `3角2分` (both tone and notone flows). Verified: all four
  forms of `3角2分` pass on the PC build (typed `3⏎jiao3⏎2⏎fen1⏎` and grid
  picks 三角二分).
  A lone `-` (or the red FIX
  cell on the on-screen bar) between the two Enters discards a typed english or
  pinyin answer and returns to typing state ("fix my mistake" flow). In the
  chinese-character flow the orange `DELETE ALL` bar cell erases the whole typed
  answer (the `-`-then-`0` cache trick made visible): typing state pushes
  `ESC + "-\n0\n"`, choose-number state pushes `0\n-\n0\n` (ESC must stay out of
  the grid path - empty raw there ENDS the answer); ESC also clears raw input in
  both backends. Bar visibility is gated by `chinese::set_input_flow()` +
  `g_active_state` (see `bar_key_active`). PC checks: `CANCEL_TYPES=chinese`
  (raw keys) and `chinese_tap` (`@TAP` on the 6th visible bar cell, same
  geometry as FIX) in driver3.py; both must end with re-answered ACCEPTED,
  wrong=0.
- Verdict screens: key line pinned at `text_top + avail_h*0.42`, fixed size
  `min(base_px*2, max_big)`; same spot for correct and wrong.
- After each batch: PC scripted checks → gradle build → `adb install -r` →
  `am start` → logcat "keyboard height: 1177px" → copy APK to `~/Downloads/chinese_memo-debug.apk`
  → commit + non-interactive push.
- Current git tip when this doc was written: `abc497f` (before it `c407fcf`), `main` synced to origin, clean tree.
