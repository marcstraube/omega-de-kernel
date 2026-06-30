#include <gba_base.h>
#include <gba_systemcalls.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ff.h"
#include "ez_define.h"
#include "draw.h"
#include "Ezcard_OP.h"
#include "lang.h"
#include "settings_file.h"

// These live in ezkernelnew.c; no shared header declares them.
extern void CheckSwitch(void);
extern void CheckLanguage(void);

//---------------------------------------------------------------------------------
// Value kinds. Most settings are simple on/off flags; the rest need bespoke
// formatting/parsing so the file stays human-readable.
enum
{
	ST_BOOL,   // on / off
	ST_LANG,   // english / chinese  <-> 0xE1E1 / 0xE2E2
	ST_MODEB,  // rumble / ram / link  <-> 0 / 1 / 2  (NOR-standalone hardware mode)
	ST_BACKUP, // 0..BACKUP_GEN_MAX, stored tagged in SET_info
	ST_HOTKEY, // three button names, e.g. "L + R + SELECT" (three SET_info slots)
	ST_PREVIEW // standard / small  <-> 0 / 1  (in-list cover preview size, issue #8)
};

typedef struct
{
	const char *section; // grouping header, only a reading aid in the file
	const char *key;
	u16 index;
	u8 type;
	u16 def;     // fallback when NOR holds an out-of-range/uninitialised value (matches CheckSwitch)
	u8 per_game; // 1 = overridable per game (issue #5); 0 (default) = global only
} setting_desc;

// Button code (SET_info value) -> name, matching the on-device hotkey menu order.
static const char *const button_names[10] = {"A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN", "R", "L"};

// Single source of truth for both writing and parsing the file.
static const setting_desc settings[] = {
    {"general", "language", assress_language, ST_LANG, 0xE1E1, 0},
    {"general", "show_thumbnail", assress_show_Thumbnail, ST_BOOL, 0, 0},
    {"general", "ingame_rtc", assress_ingame_RTC_open_status, ST_BOOL, 1, 0},
    {"general", "per_game_settings", assress_per_game_settings, ST_BOOL, 1, 0}, // master switch (issue #5)
    {"general", "preview_size", assress_preview_size, ST_PREVIEW, PREVIEW_SIZE_STANDARD, 0},

    // Per-game (issue #5) covers only the boot-time choices that the addon boot
    // path applies through gl_* and re-reads per launch. auto_save and the sleep/
    // RTS hotkeys stay global: they are consumed outside that path (the save
    // write-back runs in a later kernel session; the hotkeys are read straight
    // from NOR by the in-game addon), so a per-game override can't reach them
    // without risky NOR writes.
    {"addons", "reset", assress_v_reset, ST_BOOL, 0, 1},
    {"addons", "rts", assress_v_rts, ST_BOOL, 0, 1},
    {"addons", "sleep", assress_v_sleep, ST_BOOL, 0, 1},
    {"addons", "cheat", assress_v_cheat, ST_BOOL, 0, 1},
    {"addons", "engine", assress_engine_sel, ST_BOOL, 1, 1},
    {"addons", "auto_save", assress_auto_save_sel, ST_BOOL, 0, 0},

    {"hotkeys", "sleep_hotkey", assress_edit_sleephotkey_0, ST_HOTKEY, 0, 0},
    {"hotkeys", "rts_hotkey", assress_edit_rtshotkey_0, ST_HOTKEY, 0, 0},

    {"led", "led", assress_led_open_sel, ST_BOOL, 1, 0},
    {"led", "breathing_red", assress_Breathing_R, ST_BOOL, 1, 0},
    {"led", "breathing_green", assress_Breathing_G, ST_BOOL, 1, 0},
    {"led", "breathing_blue", assress_Breathing_B, ST_BOOL, 1, 0},
    {"led", "sd_red", assress_SD_R, ST_BOOL, 0, 0},
    {"led", "sd_green", assress_SD_G, ST_BOOL, 0, 0},
    {"led", "sd_blue", assress_SD_B, ST_BOOL, 0, 0},

    {"hardware", "mode_b", assress_ModeB_INIT, ST_MODEB, 2, 0},
    {"hardware", "backup_count", assress_backup, ST_BACKUP, BACKUP_GEN_DEFAULT, 0},
    {"hardware", "hard_reset", assress_hard_reset, ST_BOOL, 0, 0},
};
#define SETTINGS_COUNT (sizeof(settings) / sizeof(settings[0]))

// Module-private file handle and scratch buffer (kept off the small stack).
static FIL set_fil;
static u16 work_buf[0x200];

//---------------------------------------------------------------------------------
// Trim leading/trailing whitespace in place, returning a pointer into s.
static char *trim(char *s)
{
	while (*s == ' ' || *s == '\t')
	{
		s++;
	}
	char *end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
	{
		*--end = '\0';
	}
	return s;
}

// Resolve a button name (case-insensitive) to its SET_info code, or -1.
static int button_code(const char *token)
{
	for (int i = 0; i < 10; i++)
	{
		const char *a = token;
		const char *b = button_names[i];
		while (*a && *b && toupper((unsigned char)*a) == *b)
		{
			a++;
			b++;
		}
		if (*a == '\0' && *b == '\0')
		{
			return i;
		}
	}
	return -1;
}

// Clamp a raw button code to a printable name (the menu does the same on read).
static const char *button_name(u16 code)
{
	return button_names[code <= 9 ? code : 9];
}

// Clamp a raw SET_info value to a valid one, falling back to the per-setting
// default when NOR holds garbage (e.g. an uninitialised 0xFFFF slot). This keeps
// the seeded file and the reconcile aligned with the kernel's CheckSwitch
// defaults instead of mis-reading garbage as "on"/enabled.
static u16 canonical(const setting_desc *d, u16 v)
{
	switch (d->type)
	{
	case ST_BOOL:
		return (v == 0 || v == 1) ? v : d->def;
	case ST_LANG:
		return (v == 0xE1E1 || v == 0xE2E2) ? v : d->def;
	case ST_MODEB:
		return (v <= 2) ? v : d->def;
	case ST_BACKUP:
		return ((v & 0xFF00) == BACKUP_SET_TAG && (v & 0x00FF) <= BACKUP_GEN_MAX) ? v : (u16)(BACKUP_SET_TAG | d->def);
	case ST_PREVIEW:
		return (v == PREVIEW_SIZE_STANDARD || v == PREVIEW_SIZE_SMALL) ? v : d->def;
	case ST_HOTKEY:
	default:
		return v; // no CheckSwitch default; button_name() clamps on display
	}
}

//---------------------------------------------------------------------------------
// Render one setting's value as text into out.
static void format_value(char *out, const u16 *buf, const setting_desc *d)
{
	u16 v = canonical(d, buf[d->index]);
	switch (d->type)
	{
	case ST_BOOL:
		strcpy(out, v ? "on" : "off"); // canonical() guarantees 0/1
		break;
	case ST_LANG:
		strcpy(out, v == 0xE2E2 ? "chinese" : "english");
		break;
	case ST_MODEB:
		strcpy(out, v == 0 ? "rumble" : (v == 1 ? "ram" : "link"));
		break;
	case ST_BACKUP:
		sprintf(out, "%u", (unsigned)(v & 0x00FF)); // canonical() ensured tag + range
		break;
	case ST_PREVIEW:
		strcpy(out, v == PREVIEW_SIZE_SMALL ? "small" : "standard"); // canonical() guarantees 0/1
		break;
	case ST_HOTKEY:
		sprintf(out, "%s + %s + %s", button_name(buf[d->index]), button_name(buf[d->index + 1]),
		        button_name(buf[d->index + 2]));
		break;
	}
}

//---------------------------------------------------------------------------------
// Parse a (lower-cased) value string for setting d into out[0..2].
// Returns the number of SET_info slots produced (1, or 3 for hotkeys), or 0 if
// the value is invalid (caller then keeps the existing NOR value).
static int parse_value(const setting_desc *d, char *val, u16 *out)
{
	switch (d->type)
	{
	case ST_BOOL:
		if (!strcmp(val, "on") || !strcmp(val, "1"))
		{
			out[0] = 1;
			return 1;
		}
		if (!strcmp(val, "off") || !strcmp(val, "0"))
		{
			out[0] = 0;
			return 1;
		}
		return 0;
	case ST_LANG:
		if (!strcmp(val, "english"))
		{
			out[0] = 0xE1E1;
			return 1;
		}
		if (!strcmp(val, "chinese"))
		{
			out[0] = 0xE2E2;
			return 1;
		}
		return 0;
	case ST_MODEB:
		if (!strcmp(val, "rumble"))
		{
			out[0] = 0;
			return 1;
		}
		if (!strcmp(val, "ram"))
		{
			out[0] = 1;
			return 1;
		}
		if (!strcmp(val, "link"))
		{
			out[0] = 2;
			return 1;
		}
		return 0;
	case ST_BACKUP:
	{
		char *rest;
		long n = strtol(val, &rest, 10);
		if (rest == val || *rest != '\0' || n < 0 || n > BACKUP_GEN_MAX)
		{
			return 0; // rest == val: empty/non-numeric value, keep the NOR value
		}
		out[0] = (u16)(BACKUP_SET_TAG | (n & 0x00FF));
		return 1;
	}
	case ST_PREVIEW:
		if (!strcmp(val, "standard") || !strcmp(val, "0"))
		{
			out[0] = PREVIEW_SIZE_STANDARD;
			return 1;
		}
		if (!strcmp(val, "small") || !strcmp(val, "1"))
		{
			out[0] = PREVIEW_SIZE_SMALL;
			return 1;
		}
		return 0;
	case ST_HOTKEY:
	{
		char *p = val;
		int count = 0;
		while (count < 3)
		{
			char *plus = strchr(p, '+');
			if (plus)
			{
				*plus = '\0';
			}
			int code = button_code(trim(p));
			if (code < 0)
			{
				return 0;
			}
			out[count++] = (u16)code;
			if (!plus)
			{
				return count == 3 ? 3 : 0; // fewer than three buttons -> reject
			}
			p = plus + 1;
		}
		return 0; // three buttons consumed but a fourth follows -> reject
	}
	}
	return 0;
}

//---------------------------------------------------------------------------------
// Show the write-failure message and hold it briefly. On boot the screen would
// otherwise be redrawn within a frame; in the menu this is the only feedback the
// user gets. Saves succeed silently; failures are rare, so the pause is free.
static void report_save_error(void)
{
	ShowbootProgress(gl_settings_fail);
	for (int i = 0; i < 90; i++) // ~1.5 s at 60 Hz
	{
		VBlankIntrWait();
	}
}

//---------------------------------------------------------------------------------
// Write the descriptor entries selected by per_game_only to `final` via a temp
// file + atomic rename: a power loss or card removal mid-write can never corrupt
// the canonical file (it stays the old complete version until the rename
// succeeds). per_game_only: 0 = whole table (global SETTINGS.TXT), 1 = only the
// per-game-overridable entries (a per-game record). Returns 1 on success, 0 on
// any FatFS failure (temp discarded, the existing file left intact). The caller
// must ensure the target folder exists. Shared by the global and per-game writers.
static int write_descriptor_table(const char *tmp, const char *final, const char *const *header, u32 header_count,
                                  int per_game_only, const u16 *buf)
{
	char line[160];
	char val[48];
	const char *cur_section = "";
	int ok = 1;

	if (f_open(&set_fil, tmp, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
	{
		return 0;
	}

	for (u32 h = 0; h < header_count; h++)
	{
		if (f_puts(header[h], &set_fil) < 0)
		{
			ok = 0;
		}
	}

	for (u32 i = 0; i < SETTINGS_COUNT; i++)
	{
		if (per_game_only && !settings[i].per_game)
		{
			continue;
		}
		if (strcmp(settings[i].section, cur_section) != 0)
		{
			cur_section = settings[i].section;
			sprintf(line, "\r\n[%s]\r\n", cur_section);
			if (f_puts(line, &set_fil) < 0)
			{
				ok = 0;
			}
		}
		format_value(val, buf, &settings[i]);
		sprintf(line, "%s = %s\r\n", settings[i].key, val);
		if (f_puts(line, &set_fil) < 0)
		{
			ok = 0;
		}
	}

	if (f_close(&set_fil) != FR_OK)
	{
		ok = 0;
	}

	if (!ok) // partial write (card full/removed) -> discard temp, keep old file
	{
		f_unlink(tmp);
		return 0;
	}

	// Atomic swap. f_rename refuses an existing target, so drop the old file
	// first; a power loss in this tiny window just makes the next boot re-seed.
	f_unlink(final);
	return f_rename(tmp, final) == FR_OK;
}

//---------------------------------------------------------------------------------
void Write_settings_file(u16 *SET_info_buffer)
{
	static const char *const header[] = {
	    "# EZ-FLASH Omega DE kernel - settings\r\n",
	    "# Edit a value, save, then reboot to apply.\r\n",
	    "# Booleans use on/off. Hotkeys: up to three buttons joined by +.\r\n",
	    "# Buttons: A B SELECT START RIGHT LEFT UP DOWN R L\r\n",
	    "# language: english | chinese.  mode_b: rumble | ram | link.\r\n",
	};

	f_mkdir(SETTINGS_FOLDER); // harmless if it already exists
	if (!write_descriptor_table(SETTINGS_TMP, SETTINGS_FILE, header, sizeof(header) / sizeof(header[0]), 0,
	                            SET_info_buffer))
	{
		report_save_error();
	}
}

//---------------------------------------------------------------------------------
// Apply a single "key = value" line to buf. When per_game_only is set, only
// per-game-overridable keys are accepted (a per-game record can never touch a
// global-only setting, even if hand-edited). Returns 1 if it changed a value.
static int apply_line(char *line, u16 *buf, int per_game_only)
{
	char *hash = strchr(line, '#'); // strip inline comment
	if (hash)
	{
		*hash = '\0';
	}

	char *body = trim(line);
	if (*body == '\0' || *body == '[')
	{
		return 0;
	}

	char *eq = strchr(body, '=');
	if (!eq)
	{
		return 0;
	}
	*eq = '\0';
	char *key = trim(body);
	char *val = trim(eq + 1);
	for (char *c = key; *c; c++) // keys are case-insensitive, like the values
	{
		*c = (char)tolower((unsigned char)*c);
	}
	for (char *c = val; *c; c++)
	{
		*c = (char)tolower((unsigned char)*c);
	}

	for (u32 i = 0; i < SETTINGS_COUNT; i++)
	{
		if (strcmp(key, settings[i].key) != 0)
		{
			continue;
		}
		if (per_game_only && !settings[i].per_game)
		{
			return 0; // a per-game record may not carry a global-only key
		}
		u16 parsed[3];
		int slots = parse_value(&settings[i], val, parsed);
		int changed = 0;
		for (int s = 0; s < slots; s++)
		{
			if (buf[settings[i].index + s] != parsed[s])
			{
				buf[settings[i].index + s] = parsed[s];
				changed = 1;
			}
		}
		return changed;
	}
	return 0;
}

//---------------------------------------------------------------------------------
void Load_settings_file(void)
{
	for (u32 i = 0; i < 0x200; i++)
	{
		work_buf[i] = Read_SET_info(i);
	}

	if (f_open(&set_fil, SETTINGS_FILE, FA_READ) != FR_OK)
	{
		Write_settings_file(work_buf); // seed the file from the current NOR state
		return;
	}

	int changed = 0;
	char line[160];
	while (f_gets(line, sizeof line, &set_fil))
	{
		changed |= apply_line(line, work_buf, 0);
	}
	f_close(&set_fil);

	if (changed)
	{
		Save_SET_info(work_buf, 0x200); // reconcile: rewrites SD (canonical) + NOR
		CheckSwitch();                  // reload the gl_* runtime copies
		CheckLanguage();                // language may have changed -> reload strings
	}
}

//---------------------------------------------------------------------------------
// Per-game settings records (issue #5): one "/SYSTEM/GAMES/<key>.TXT" per game,
// where <key> is the stable Game_lookup_key() identity (4-char GBA code or 8 hex
// GB/GBC CRC). A record carries only the per-game-overridable subset of the
// settings table and overrides the global boot options for that game alone.

// A per-game key must be a non-empty run of ASCII alphanumerics. The game code
// is read straight from the (untrusted) cartridge header, so this stops a crafted
// code containing '/', '\\' or ".." from escaping /SYSTEM/GAMES/ when it is
// concatenated into a FatFS path below.
static int pergame_key_valid(const char *key)
{
	if (key == NULL)
	{
		return 0;
	}
	int len = 0;
	for (const char *p = key; *p != '\0'; p++)
	{
		unsigned char c = (unsigned char)*p;
		if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
		{
			return 0;
		}
		len++;
	}
	return len >= 2 && len <= 8; // 2 for the /<c0>/<c1>/ split; 8 = longest key (GB/GBC CRC), bounds the path
}

// Build "/SYSTEM/GAMES/<c0>/<c1>/<key>.<ext>" into out (out must hold >= 40 bytes).
// The two subfolders (by the first two key characters) mirror the cover-art
// layout (/IMGS/<c0>/<c1>/...) so no single directory fills up on a large
// collection the way one flat folder would.
static void pergame_path(char *out, const char *key, const char *ext)
{
	sprintf(out, "%s/%c/%c/%s.%s", GAMES_FOLDER, key[0], key[1], key, ext);
}

//---------------------------------------------------------------------------------
// Persist the per-game subset of buf as this game's record. Returns 1 on success.
int Write_pergame_record(const char *key, const u16 *buf)
{
	static const char *const header[] = {
	    "# EZ-FLASH Omega DE kernel - per-game settings (issue #5)\r\n",
	    "# Overrides the global boot options for this game only.\r\n",
	    "# Booleans use on/off.\r\n",
	};
	char tmp[40];
	char final[40];

	if (!pergame_key_valid(key))
	{
		return 0;
	}
	// Create the folder chain /SYSTEM/GAMES/<c0>/<c1>. FatFS only creates the final
	// component, so make each level; every f_mkdir is harmless if it already exists.
	char sub[24];
	f_mkdir(SETTINGS_FOLDER);
	f_mkdir(GAMES_FOLDER);
	sprintf(sub, "%s/%c", GAMES_FOLDER, key[0]);
	f_mkdir(sub);
	sprintf(sub, "%s/%c/%c", GAMES_FOLDER, key[0], key[1]);
	f_mkdir(sub);
	pergame_path(tmp, key, "TMP");
	pergame_path(final, key, "TXT");
	if (!write_descriptor_table(tmp, final, header, sizeof(header) / sizeof(header[0]), 1, buf))
	{
		report_save_error();
		return 0;
	}
	return 1;
}

//---------------------------------------------------------------------------------
// Overlay this game's saved overrides onto buf (per-game slots only; the caller
// seeds buf from the current global state first). Returns 1 if a record existed.
int Load_pergame_record(const char *key, u16 *buf)
{
	if (!pergame_key_valid(key))
	{
		return 0;
	}
	char path[40];
	pergame_path(path, key, "TXT");
	if (f_open(&set_fil, path, FA_READ) != FR_OK)
	{
		return 0;
	}

	char line[160];
	while (f_gets(line, sizeof line, &set_fil))
	{
		apply_line(line, buf, 1); // per_game_only: ignore any stray global-only keys
	}
	f_close(&set_fil);
	return 1;
}

//---------------------------------------------------------------------------------
// Remove this game's record, reverting it to the global defaults ("reset to
// global" in the per-game editor). A missing record is not an error.
void Delete_pergame_record(const char *key)
{
	if (!pergame_key_valid(key))
	{
		return;
	}
	char path[40];
	pergame_path(path, key, "TXT");
	f_unlink(path);
}
