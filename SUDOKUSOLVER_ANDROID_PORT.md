# Port brief: singlefile_sudokusolver → Android app (SDL2, same pattern as chinese_memo)

Feed this file + `chinese_memo/ANDROID_PORT.md` (in this repo) to a fresh session.
`ANDROID_PORT.md` holds the verified machine facts (build commands, device serials,
MIUI quirks, progress round-trip, headless test protocol) — do not re-derive those,
trust them. This file is the project-specific brief for the *next* port.

---

## 0. The assignment (what a fresh agent must do)

Turn the console sudoku solver below into a fullscreen Android app on the same
phone (`e1fc29d2`), built from the same toolchain, by copying the chinese_memo
android/CMake/SDL skeleton — the same way chinese_memo itself was ported.
No design help is requested beyond what is written here; deliver the working APK.

## 1. The source

- File: `/media/jgeleji/8622f086-b4c7-449f-903f-c9f9229a9342/home/jani129/singlefile_sudokusolver.cpp`
  (741 lines, one compilation unit, dated 2023, no build system, no git repo).
  Mounted external drive — if the drive is gone, ask the user to plug it in;
  nothing else on the PC contains this code.
- No ncurses, no `getch`, no stdin, no files: pure `std::cout`. Include graph is
  header-guard fakes (`#ifndef SUDOKUSTATE_H` etc.) inside one file — it compiles
  standalone. Standard C++11, `<random>` included but the shown path is deterministic
  (bitsets + backtracking); keep it deterministic and fixed-seed anyway for testability.
- Structure:
  - `CellState` — a cell: known flag, `std::bitset<9>` possibles, `display_state()`
    returns 3 text rows per cell.
  - `SudokuConstraintField` — constraint bookkeeping.
  - `SudokuState` — 9x9 cells; `load_raw(std::vector<std::string>)` — nine strings of
    9 chars, `' '` = empty, `'1'..'9'` = given; `wet_advance(n, solutions_set)` drives
    the solver; `display(std::ostream&)` / `display()` return the ASCII board
    (27 text rows) + a `progress()` score line.
  - `main()` (line ~172) is only a demo driver: prints gcc version, solves one
    hardcoded puzzle, prints `st`, the board and all solutions. **This is not the app.**

## 2. The porting insight that makes this one *easier* than chinese_memo

chinese_memo was interactive: the port captured its `std::cout` and replayed it.
This one has **no interactive I/O at all** — `main()` is a demo. So the port is:

1. keep the core (`CellState` … `SudokuState`, verbatim, untouched) as
   `sudokusolver_core.cpp` (+ a small header to split the compilation unit),
2. **write the missing app**: an SDL2 UI that
   - shows the 9x9 board big, tap-to-edit cells, tap-to-enter digits (the existing
     `input_sdl2.cpp` already has a tappable button/grid system — reuse it),
   - runs `wet_advance` on a "solve" action, shows result boards (the core already
     hands you `display()` as `vector<string>` rows — trivially feedable to the
     chinese_memo text renderer, its "lines" abstraction is exactly that shape),
   - solves on the UI thread for normal puzzles (backtracing this kind of solver on a
     phone is milliseconds), but guard against pathological hard boards: cap work per
     tap or run it with a periodic frame dump so the app never looks frozen.
3. the stdout-capture machinery from `input_sdl2.cpp` is NOT needed for the engine —
   it stays useful only as the text/lines renderer (ANSI runs, sublines, layout).
   Strip the streambuf capture, keep the renderer + touch/button/bar code.

## 3. Concrete port decisions (already thought through — follow them)

- package / applicationId: `com.geleji.sudokusolver`; native lib: `sudokusolver_sdl`;
  entry: `main_sdl_sudokusolver.cpp` (copy `main_sdl_questions10.cpp`).
- copy from chinese_memo (`/home/jgeleji/Documents/Default Project/chinese_memo`):
  `android/` whole, `third_party/` whole (SDL2-2.30.9 + stb headers + the **patched**
  `org/libsdl/app`), `input_sdl2.cpp` as renderer starting point, `CMakeLists.txt`
  ANDROID/BUILD_SDL_PC branch pattern, gradle files incl. `syncAppAssets` pattern.
  See `ANDROID_PORT.md` §4 checklist.
- NO keyboard needed: the whole UI is taps (cells + digit pad). So remove the IME
  apparatus — that reclaims ~1177px:
  - manifest: drop `stateAlwaysVisible|adjustPan` → `stateAlwaysHidden`,
  - MainActivity: the IME-height watcher may stay but will report 0;
  - native: the `ime_band()` reserve reads `files/*_ime_height` → 0 automatically,
    leave the code, it just never reserves space.
  Do NOT remove the VISIBLE_PASSWORD patch from the vendored SDLActivity java —
  harmless if IME never shows, and re-vendoring accidents are what breaks keyboards.
- font: `input_sdl2.cpp` hard-loads `NotoSansSC.ttf` (fopen at its top, both paths
  tried, fatal if missing). Digits/latin only here — but simplest correct first move
  is to ship the same TTF in assets via `syncAppAssets` (APK +~10MB); optimize the
  font later if the APK bothers anyone.
- puzzles: solver demo puzzle can seed the initial board. If save/load of user
  puzzles is wanted, use the chinese_memo file round-trip pattern: files in
  `getFilesDir()`, pulled via `adb exec-out "run-as com.geleji.sudokusolver cat files/..."`,
  own copy of `progress_pull.sh`.
- build/deploy: exact commands in `ANDROID_PORT.md` §2 — gradle `assembleDebug`,
  `adb -s e1fc29d2 install -r`, MIUI pauses after install → `am start` + `pidof`,
  copy APK to `~/Downloads/sudokusolver-debug.apk`. logcat tag: pick one short tag
  (chinese_memo uses `chinese_memo` → here `sudokusolver`).

## 4. Testing order (same discipline as chinese_memo)

1. `g++ -O2 singlefile_sudokusolver.cpp -o /tmp/solve_demo && /tmp/solve_demo` on the
   untouched file → record expected output; re-check after splitting into core+header.
2. PC SDL twin (`-DBUILD_SDL_PC=ON`) with the `CM_*` hooks from `input_sdl2.cpp`
   (window 1280x2467, `SDL_VIDEODRIVER=dummy`, `CM_DUMP_DIR`, scripted autokeys via
   `CM_AUTOKEYS_FILE`/`CM_STATE_FILE` — protocol rules in `ANDROID_PORT.md` §5;
   no IME band needed here, pass none). Frame-dump PNGs are the visual proof.
3. only then: gradle build → install → relaunch → `pidof` + logcat.

## 5. Open product questions (decide with the user, do not guess silently)

- enter a puzzle by tapping cells+digit pad (assumed), or type digits on the hardware
  keyboard too (then IME machinery comes back and the 1177px layout rules apply)?
- show candidate bitsets in empty cells (core's 3-row `display_state` shows them in
  ASCII; on screen this is small-cell candidate rendering) or only known digits while solving step-by-step?
- all solutions listed, or first solution + "N more" counter?

Everything else: follow chinese_memo's precedent, and when a detail is ambiguous,
the chinese_memo repo is the authority on how this user likes these apps done
(progress files tracked in git, performance first-class, big tappable cells,
verdict/result text big and in a fixed spot).
