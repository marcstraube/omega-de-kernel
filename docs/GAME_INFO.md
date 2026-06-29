# Game info panel

Show your own per-game notes — a description, controls, a walkthrough hint,
whatever you like — in a dedicated full-screen window, without cluttering the
file list. The text is rendered natively by the kernel, so no PogoShell or other
add-on is needed.

This works for **GBA, GB and GBC** titles (the same games that can carry cover
art). Other emulator formats have no lookup key yet, so they show a placeholder.

## How to use it

1. **Add a note.** Put a plain-text file for the game under `/INFO` on the SD
   card (see *Where the file goes* below). This is normally done by your
   ROM manager, which already knows each game's id.
2. **Open the panel.** In the SD game list, highlight a game and press
   **L + SELECT** (hold **L**, then tap **SELECT**). **SELECT** on its own still
   toggles the cover preview as before.
3. **Read and scroll.** If the note is longer than one screen, scroll with the
   **D-pad up / down**. The footer shows whether more text is available.
4. **Close.** Press **B** or **L** to return to the list.

A game with no `/INFO` file shows a short *"No info for this game."* placeholder,
so the gesture always gives feedback.

## Where the file goes

```
/INFO/<c0>/<c1>/<code>.txt
```

- `<code>` is the game's stable id — **the same key used for cover art**:
  - **GBA**: the 4-character cartridge code from the ROM header (offset `0xAC`).
  - **GB / GBC**: an 8-hex-digit `CRC32` of the header bytes `0x134..0x14F`
    followed by the file size as a little-endian 32-bit value (uppercase hex).
- `<c0>` and `<c1>` are the first two characters of `<code>`. The two subfolders
  mirror the cover-art layout (`/IMGS/<c0>/<c1>/...`) so a card with thousands of
  games never piles every note into one directory.

Because the key is identical to the cover key, a ROM manager that already names
cover images can name the info files the same way — only the directory (`/INFO`
instead of `/IMGS`) and the extension (`.txt` instead of `.bmp`) differ.

## Text format

- **Encoding: ASCII + GB2312/GBK double-byte**, matching the kernel font
  (`DrawHZText12`). Plain English/ASCII notes work as-is.
- **Accented Latin characters** (German umlauts, French accents, …) are **not**
  in the current ASCII font and will not render correctly yet. Full European
  text depends on the font extension tracked in the localization work (#14).
- Lines are **word-wrapped** automatically to the screen width; `\n` forces a
  break. Tabs are not expanded.
- The panel reads up to **8 KB** per file (about a few screens of scrolling);
  anything beyond that is ignored. Notes are meant to be short.

## How it works

- On **L + SELECT**, the kernel computes the cover key for the highlighted game,
  opens `/INFO/<c0>/<c1>/<code>.txt`, and reads it into the (otherwise idle)
  cover image buffer. No extra RAM is reserved — the cover is reloaded when the
  panel closes.
- The text is word-wrapped and drawn with the existing `DrawHZText12` font
  routine. The window is modeled on the built-in help window: it draws over the
  screen and returns to the list on a key press.
- **SELECT** alone keeps its stock behavior (cover preview on/off). A combo
  (`L + SELECT`) was chosen over a long-press so that the preview toggle stays
  instant.
