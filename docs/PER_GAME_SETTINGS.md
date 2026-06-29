# Per-game settings

Some games misbehave with an add-on you otherwise want on all the time (for
example a game that crashes with the real-time-save add-on). Per-game settings
let you **override the boot options for one game only**, so you can keep your
add-ons on globally and turn them off just for the game that needs it.

This applies to **GBA games**. GB/GBC titles run in an emulator and do not use
these add-ons, so they have no per-game settings.

## What you can override per game

| Setting | Meaning |
| --- | --- |
| `reset` | Reset add-on |
| `rts` | Real-time-save add-on |
| `sleep` | Sleep add-on |
| `cheat` | Cheat menu |
| `engine` | Fast patch engine (only effective with an add-on on) |

Everything else stays global. In particular the **sleep / RTS hotkeys** and
**auto-save** are deliberately not per-game: the in-game add-on reads the hotkeys
straight from the cartridge and auto-save runs in a later step, so a per-game
value could not reach them.

## How to use it

1. **The feature is on by default.** A master switch, *Game settings*, lives in
   the second on-device settings screen (next to Mode B / LED / backup) and shows
   `ENABLED` / `DISABLED`. It is also mirrored in `/SYSTEM/SETTINGS.TXT` as
   `per_game_settings = on | off` (see `SETTINGS_FILE.md`). With it off, per-game
   records are ignored and the editor is hidden.
2. **Open the editor.** Press **A** on a GBA game to bring up its boot menu;
   **GAME SETTINGS** is the last entry. Select it.
3. **Edit and save.** Toggle the five options with **A**, then choose **Save**.
   A game you have never customised shows your current *global* values as the
   starting point — changing and saving creates that game's record.
4. **Reset to global.** Choose **Reset to global** in the editor to delete the
   game's record so it follows the global settings again.

When you then boot that game with **Boot with add-on**, its saved choices are
used instead of the global ones. A game with no record always uses the global
settings, so nothing changes until you deliberately customise a game.

## How it works

- Each customised game gets a small text file
  `/SYSTEM/GAMES/<c0>/<c1>/<code>.TXT`, where `<code>` is the game's 4-character
  cartridge code (the stable id also used for cover art) and `<c0>`/`<c1>` are its
  first two characters. The two subfolders mirror the cover-art layout so no
  single directory fills up on a large collection. Renaming the ROM file does not
  lose the settings.
- The file uses the same `key = value` format as `SETTINGS.TXT`, but holds only
  the five per-game keys, under an `[addons]` section.
- At boot, if the master switch is on and a record exists, its values replace the
  global add-on/engine choices **for that boot only**.
- Writes are atomic (temp file + rename), so a power loss or removed card
  mid-save cannot corrupt a record.
- Records are independent of the global store: a hand-edited record can only ever
  contain the five per-game keys; any other key is ignored, and a record can
  never change your global settings.

## Known limitation

The **Boot with add-on** menu entry is only selectable when at least one add-on
is enabled **globally**. So to launch a game with its per-game add-ons, keep the
relevant add-on on globally and switch it off per game where needed — you cannot
yet enable an add-on for a single game while all add-ons are off globally. (A
later release is planned to consult the per-game record already in the boot
menu.)

## Credits and differences

This is, to our knowledge, the first Omega DE kernel to make boot add-ons
configurable **per game**. The design keeps add-ons useful globally while letting
you exempt the few titles that break with them — instead of forcing an
all-or-nothing global choice. It builds on the human-readable settings layer
(`SETTINGS_FILE.md`): records are plain text, atomically written, and validated
so a stray file can never corrupt your settings.
