# Fullscreen cover & gallery

View a game's box art filling the whole screen, and flip through extra images
for it — a back cover, an in-game screenshot, whatever you provide. It is a
separate mode layered on top of the file list, so it never gets in the way of
browsing.

This works for **GBA, GB and GBC** titles (the same games that can carry cover
art). Other formats have no lookup key yet.

## How to use it

1. **Open it.** In the SD game list, highlight a game and press **L + R** (hold
   **L**, then tap **R**). **R** on its own still switches list pages.
2. **Switch game.** **D-pad up / down** moves to the previous / next game
   without leaving the mode (a carousel). When you exit, the list lands on the
   game you stopped on.
3. **Browse the gallery.** **D-pad left / right** pages through the images for
   the current game (front cover, then the extras). A small `n/m` counter in the
   bottom-left appears when more than one image exists, and paging wraps around.
4. **Close.** Press **B** or **SELECT** to return to the list.

A game whose cover is missing or unusable shows a black screen with its file
name, so the gesture always gives feedback.

## Where the images go

All images live under the cover-art tree, keyed exactly like the list cover:

```
/IMGS/<c0>/<c1>/<code>.bmp      small front cover  (list preview + fallback)
/IMGS/<c0>/<c1>/<code>_0.bmp    high-res front cover  (fullscreen, optional)
/IMGS/<c0>/<c1>/<code>_1.bmp    first extra  (e.g. back cover)
/IMGS/<c0>/<c1>/<code>_2.bmp    second extra (e.g. in-game)
...                             up to _15
```

- `<code>` is the game's stable id — **the same key used for cover art and the
  info panel**:
  - **GBA**: the 4-character cartridge code from the ROM header (offset `0xAC`).
  - **GB / GBC**: an 8-hex-digit `CRC32` of the header bytes `0x134..0x14F`
    followed by the file size as a little-endian 32-bit value (uppercase hex).
- `<c0>` and `<c1>` are the first two characters of `<code>`.

### The two front covers

The front cover exists at two resolutions, and the kernel uses each where it
fits best:

- **`<code>.bmp`** — the small list thumbnail (max **120 × 80**). Shown in the
  bottom-right preview as before, never scaled.
- **`<code>_0.bmp`** — an optional **high-res** front for fullscreen. When
  present it is shown as image 0; when absent, the small `<code>.bmp` is
  upscaled instead. So every game with a cover works in fullscreen immediately,
  and a sharper front is a drop-in upgrade.

The extras `_1.._N` are probed **in order** and stop at the first gap, so number
them contiguously starting at `_1`.

Because the key is identical to the cover and info keys, a ROM manager that
already names cover images can generate these the same way — only the `_N`
suffix differs.

## Image format

- **16bpp BMP, top-down rows** (negative BMP height), uncompressed — the same
  format as the list cover. Wrong-format or bottom-up files are skipped.
- **Size limit:** each image is loaded into the 64 KB cover buffer, so its
  pixels must fit in it. That caps a source at about **32 768 pixels** — a true
  240 × 160 fullscreen image (38 400 px) does **not** fit. GBA box art is
  portrait, so a height-filling cover is only ~120 × 160 and fits comfortably;
  for landscape shots keep within the budget (e.g. 226 × 144).
- Images are scaled to fit the screen **preserving aspect ratio**, centered,
  with black letter-/pillarbox bars. They are not stretched.

## How it works

- On **L + R**, the kernel enters a modal loop. For each game it computes the
  cover key, counts the available gallery images, and loads the selected one
  into the (otherwise idle) cover image buffer.
- The image is scaled straight to the screen by a nearest-neighbour scaler
  (`Draw_scaled_to_box`) that writes directly to video memory — the cover buffer
  overlaps the kernel's drawing scratch area, so going through the scratch path
  would corrupt the image being shown.
- **L gates the trigger** so bare **R** keeps switching list pages, mirroring how
  **L + SELECT** opens the info panel while **SELECT** alone toggles the preview.
