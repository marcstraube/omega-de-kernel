#ifndef INFOWINDOW_H
#define INFOWINDOW_H

#include <gba_base.h>

// Full-screen, word-wrapped, scrollable text window for per-game info (#6).
// Modeled on Show_help_window(): draws over the screen and returns on B or L.
//
// buf  : text bytes (ASCII + GBK double-byte, the encoding DrawHZText12 expects).
//        MUST be NUL-terminated at buf[len] -- DrawHZText12 falls back to
//        strlen() for empty lines and when validating the requested length, so a
//        missing terminator would read past the buffer.
// len  : text length in bytes (0 -> a short placeholder is shown instead).
// title: NUL-terminated C string shown at the top (e.g. the ROM file name).
void Show_game_info_window(const char *buf, u32 len, const char *title);

#endif
