# Code quality: build warnings & static analysis

This project surfaces compiler warnings and offers two local static-analysis
passes so issues (especially buffer overflows) are caught early and new warnings
introduced by a change stay visible.

All of this is **codegen-neutral**: enabling warnings or running the analyzer
does not change the emitted binary. The `.gba` hash is the regression guard —
`make clean && make` must keep producing the same `sha256sum` as before.

## Build warnings

Enabled unconditionally in the `Makefile`:

```
-Wall -Wextra -Wshadow -Wformat=2
```

Warnings are **not** fatal (no `-Werror`): upstream/third-party translation
units may warn, and we don't want that to break the build.

Just build normally to see them:

```sh
make clean && make
```

## Static analysis

### GCC `-fanalyzer` (primary)

Path-sensitive analysis using the *exact* build configuration (defines,
includes, ARM7TDMI target). Best at the buffer-overflow / use-after-free /
uninitialized-value class. Slower than a normal build (~10–15 s here), so it is
opt-in:

```sh
make clean && make ANALYZE=1
```

### cppcheck (complementary)

A second, independent engine that catches *different* things (style, scope,
const-correctness, portability). Runs over the hand-written sources only
(`source/*.c`, i.e. **excluding** `source/ff15` = FatFs upstream):

```sh
make cppcheck
```

Requires `cppcheck` on `PATH` (Arch: `pacman -S cppcheck`).

## What we fix, and what we don't

- **Hand-written sources** (`source/*.c`, e.g. `ezkernelnew.c`, `GBApatch.c`,
  `setwindow.c`, …): fix warnings/findings **as we touch the code**, plus a
  final review pass. Not in a wholesale upfront refactor.
- **`source/ff15/*` (FatFs):** third-party upstream — kept diffable against
  ChaN's release, **not** modified to silence warnings.
- **Data headers** (`saveMODE.h`, `goomba.h`, fonts, generated tables): data,
  not hand-maintained code. `saveMODE.h` carries a deliberate
  `__attribute__((nonstring))` on its fixed-width `gamecode[4]` game-ID field
  (4-char tags, intentionally not NUL-terminated) — this is the correct,
  compiler-requested annotation, not a suppression.

## Baseline (devkitARM GCC 16.1.0)

Regenerate any time with the commands above; counts shift as code changes.

**Build warnings — 35 total:**

| Source | Count | Notes |
|---|---|---|
| Hand-written `.c` | 28 | ezkernelnew 11, showcht 8, GBApatch 7, setwindow/2 ×1 — ours to clean |
| `ff15/diskio.c` | 7 | FatFs upstream (`-Wunused-parameter`) |

Types: 18× `-Wunused-parameter`, 10× `-Wsign-compare`, 7× `-Wshadow`.
No buffer / format / bounds warnings.

**`-fanalyzer` — 22 findings, all in hand-written sources:**

| Finding | Count | Where |
|---|---|---|
| `-Wanalyzer-undefined-behavior-ptrdiff` | 16 | `GBApatch.c` |
| `-Wanalyzer-use-of-uninitialized-value` | 3 | `showcht.c:536` |
| `-Wanalyzer-infinite-loop` | 3 | `Ezcard_OP.c:572`, `ezkernelnew.c:2031`, `ezkernelnew.c:2613` |

**`make cppcheck` — 4 style findings:** `RTC.c` (3× `variableScope`,
1× `constParameterPointer`).

## Follow-up (out of scope here)

CI (GitHub Actions + devkitARM, building and running cppcheck on PRs) is added
once features start merging — see the tracking issue.
