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

**`-fanalyzer` — 0 findings** after the #16 triage. Resolved:
- 3× `-Wanalyzer-use-of-uninitialized-value` (`showcht.c`, `val_buf`) — a
  real bug, now initialized before the parse loop.
- 16× `-Wanalyzer-undefined-behavior-ptrdiff` (`GBApatch.c`) — formal ISO-C
  UB in the linker-symbol offset idiom, switched to integer subtraction.
- 3× `-Wanalyzer-infinite-loop` (`Ezcard_OP.c:572`, `ezkernelnew.c:2031`,
  `ezkernelnew.c:2613`) — intentional halts (FW-update "power off manual",
  fatal SD-mount error, post-hard-reset wait). Annotated in place with
  `#pragma GCC diagnostic ignored "-Wanalyzer-infinite-loop"` + a comment;
  codegen-neutral, so the `.gba` stays byte-identical.
- Adjacent buffer-overflow risk in the same cheat parser (unbounded
  `val_buf`/`address_buf` writes) is tracked separately in #17.

**`make cppcheck` — 0 findings** (the 4 `RTC.c` style findings —
3× `variableScope`, 1× `constParameterPointer` — were fixed in the #16 triage).

With both analyzers at 0, any new finding from a later change stands out
immediately.

> Note: the `val_buf`, `ptrdiff` and `RTC` fixes are **value-equivalent but
> not byte-identical** — under `-Os` GCC re-selects equivalent instructions,
> so each moves the `.gba` hash. They are verified by reasoning here and need a
> real-hardware smoke-test before release (folded in with the GCC-16 toolchain
> jump, which is also not yet hardware-verified).

## Follow-up (out of scope here)

CI (GitHub Actions + devkitARM, building and running cppcheck on PRs) is added
once features start merging — see the tracking issue.
