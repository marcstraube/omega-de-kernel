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

#endif
