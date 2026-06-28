#ifndef _SETTINGS_FILE_H
#define _SETTINGS_FILE_H

#include <gba_base.h>

// Human-readable SD settings layer over the binary SET_info store (issue #4).
//
// SETTINGS.TXT is the user-facing edit surface; the NOR SET_info store stays
// authoritative as a fallback. The two are kept in sync at the single write
// choke point (Save_SET_info) and reconciled at boot.

// Dump the given SET_info buffer to /SYSTEM/SETTINGS.TXT (SD before NOR).
// Called from Save_SET_info() so every settings write also refreshes the file.
void Write_settings_file(u16 *SET_info_buffer);

// Boot-time reconcile: seed the file from NOR when missing, otherwise apply any
// hand-edited values back into the settings store. Call after f_mount succeeds.
void Load_settings_file(void);

// Per-game settings records (issue #5). A record overrides the global boot
// options for one game, keyed by its stable Game_lookup_key() identity (4-char
// GBA code or 8 hex GB/GBC CRC). Each holds only the per-game-overridable subset
// (addons, engine, auto-save, sleep/RTS hotkeys).

// Persist the per-game subset of `buf` to this game's record. Returns 1 on success.
int Write_pergame_record(const char *key, const u16 *buf);

// Overlay this game's saved overrides onto `buf` (per-game slots only; seed `buf`
// from the current global state first). Returns 1 if a record existed.
int Load_pergame_record(const char *key, u16 *buf);

// Remove this game's record, reverting it to the global defaults.
void Delete_pergame_record(const char *key);

#endif
