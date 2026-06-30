# Settings file on the SD card

The kernel mirrors its settings to a plain-text file, `/SYSTEM/SETTINGS.TXT`, so
you can see and change them on a PC instead of only through the on-device menu.

## How it works

- **Created automatically.** On the first boot with this kernel the file is
  written from your current settings. If you ever delete it, it is recreated on
  the next boot.
- **Edit on a PC, reboot to apply.** Change a value with any text editor, save
  the file, put the card back, and power on. The kernel reads the file at boot
  and applies it.
- **The on-device menu still works.** Whenever you change a setting in the menu,
  the file is updated to match — the two never drift apart.
- **Your settings are safe.** The card's internal store stays the source of
  truth and a fallback: if the file is missing, unreadable, or a line is
  invalid, the kernel simply keeps the value it already had. The file is written
  with a temporary file and an atomic rename, so a power loss or a removed card
  mid-write can never corrupt it.
- If a write fails (full or failing card) you briefly see
  **"Settings save failed!"**; successful saves are silent.

## File format

```ini
# lines starting with # are comments
[section]            # section headers are only a reading aid; they are ignored
key = value          # one setting per line
```

- **Keys and values are case-insensitive**, so `LED = ON` and `led = on` are the
  same. Spaces around `=` are optional.
- Anything after a `#` on a line is ignored, so you can add your own notes.
- Unknown keys and invalid values are ignored (the current setting is kept), so
  a typo can never brick a setting.

## Valid values

| Key | Values | Meaning |
| --- | --- | --- |
| `language` | `english`, `chinese` | Menu language |
| `show_thumbnail` | `on`, `off` | Show cover thumbnails in the list |
| `ingame_rtc` | `on`, `off` | Pass the real-time clock through to the game |
| `reset` | `on`, `off` | Reset add-on / hotkey |
| `rts` | `on`, `off` | Real-time-save add-on / hotkey |
| `sleep` | `on`, `off` | Sleep add-on / hotkey |
| `cheat` | `on`, `off` | Cheat menu |
| `engine` | `on`, `off` | Fast patch engine (only effective with an add-on on) |
| `auto_save` | `on`, `off` | `on` = automatic save write-back, `off` = manual |
| `sleep_hotkey` | three buttons, e.g. `L + R + SELECT` | Sleep button combo |
| `rts_hotkey` | three buttons, e.g. `L + R + START` | Real-time-save button combo |
| `led` | `on`, `off` | Power LED on/off |
| `breathing_red` / `breathing_green` / `breathing_blue` | `on`, `off` | Breathing-LED colour channels |
| `sd_red` / `sd_green` / `sd_blue` | `on`, `off` | SD-activity-LED colour channels |
| `mode_b` | `rumble`, `ram`, `link` | Hardware function for a NOR-standalone game (Mode B) |
| `backup_count` | `0`–`9` | Save-backup generations to keep (see `SAVE_BACKUP.md`) |
| `hard_reset` | `on`, `off` | Hard-reset the console on every launch; hold **L** to invert (see `HARD_RESET.md`) |

Booleans also accept `1`/`0` in place of `on`/`off`.

### Hotkeys

A hotkey is exactly **three** buttons joined by `+`. Valid button names:

```
A  B  SELECT  START  RIGHT  LEFT  UP  DOWN  R  L
```

Example: `sleep_hotkey = L + R + UP`. A combo with fewer or more than three
buttons, or an unknown name, is ignored and the current hotkey is kept.

## If you see "Settings save failed!"

This message means the kernel could **not** write `/SYSTEM/SETTINGS.TXT` — it
points at the **SD card**, not at the kernel. Common causes:

- the card is **full**,
- the card is **write-protected** (lock switch),
- the card was **removed** while saving, or
- the card is **failing / has a damaged file system**.

Your settings are **not lost**: they are still saved to the card's internal
store, and only the human-readable mirror failed to write. To fix it, free up
space, unlock, reseat, or replace the SD card; the file is rewritten on the next
successful save. If it keeps happening, run a file-system check on the card (or
reformat it as FAT32 and restore your files).

## Credits and differences

Prior art for an SD-based settings file: the
[DS Style kernel](https://github.com/FrankieT19/omega-de-ds-style-kernel) ships
a readable `SETTINGS.TXT`, and the
[ZaindORp kernel](https://github.com/orzgithub/omega-de-kernel_ZaindORp) uses
SD-based settings/themes. This implementation is a **layer over** the existing
binary store rather than a replacement (the on-device store stays as a
fallback), with atomic writes, per-line validation, and default-correcting reads
so an uninitialised card never enables settings by accident.
