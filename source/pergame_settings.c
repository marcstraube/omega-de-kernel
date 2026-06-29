#include <gba_systemcalls.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gba_base.h>
#include <gba_input.h>

#include "ez_define.h"
#include "lang.h"
#include "ezkernel.h"
#include "draw.h"
#include "Ezcard_OP.h"
#include "settings_file.h"

// Per-game settings editor (issue #5). Lets the user override the boot-time
// addon/engine choices for one GBA game, keyed by its 4-char game code. Saving
// writes a record (/SYSTEM/GAMES/<code>.TXT); "reset to global" deletes it so
// the game falls back to the global defaults again. The caller draws the
// background (gImage_SET) before calling and restores the list menu afterwards.

// Defined in setwindow.c (no shared header declares it).
extern void Draw_select_icon(u32 X, u32 Y, u32 mode);

// gl_reset_on/rts/sleep/cheat come from ezkernel.h; gl_engine_sel is declared here.
extern u16 gl_engine_sel;

#define PG_ROWS 5  // reset, rts, sleep, cheat, engine
#define PG_SAVE 5  // selection index of the "save" action
#define PG_RESET 6 // selection index of the "reset to global" action
#define PG_ITEMS 7 // total selectable rows

// SET_info slot for each editable row, in display order.
static const u16 pg_slot[PG_ROWS] = {
    assress_v_reset, assress_v_rts, assress_v_sleep, assress_v_cheat, assress_engine_sel,
};

//---------------------------------------------------------------------------------
// Load the effective values into v[]: the current global state, overlaid with
// this game's record when use_record is set.
static void pg_seed(u16 *v, const char *key, int use_record)
{
	u16 buf[16] = {0};
	buf[assress_v_reset] = gl_reset_on;
	buf[assress_v_rts] = gl_rts_on;
	buf[assress_v_sleep] = gl_sleep_on;
	buf[assress_v_cheat] = gl_cheat_on;
	buf[assress_engine_sel] = gl_engine_sel;
	if (use_record)
	{
		Load_pergame_record(key, buf);
	}
	for (u32 i = 0; i < PG_ROWS; i++)
	{
		v[i] = buf[pg_slot[i]] ? 1 : 0;
	}
}

//---------------------------------------------------------------------------------
static void draw_pergame(const char *key, const u16 *v, u32 sel)
{
	char *label[PG_ROWS] = {gl_reset, gl_rts, gl_sleep, gl_cheat, gl_use_engine};
	u32 y0 = 44;
	u32 dy = 14;
	u32 icon_x = 24;
	u32 text_x = 40;
	char codestr[16];

	ClearWithBG((u16 *)gImage_SET, 0, 0, 240, 160, 1); // repaint over the background

	DrawHZText12(gl_game_settings, 0, 8, 26, gl_color_selected, 1); // title, below the tab strip
	// Game code at the clock's height, with a label so its meaning is obvious.
	sprintf(codestr, "Code: %s", key);
	DrawHZText12(codestr, 0, 240 - 8 - strlen(codestr) * 6, 3, gl_color_text, 1);

	for (u32 i = 0; i < PG_ROWS; i++)
	{
		Draw_select_icon(icon_x, y0 + i * dy, v[i]);
		DrawHZText12(label[i], 0, text_x, y0 + i * dy, (sel == i) ? gl_color_selected : gl_color_text, 1);
	}

	// Save and reset-to-global as buttons (filled box + label), side by side with a
	// gap above so they do not look cramped; highlighted when selected, matching the
	// Set buttons on the system settings screen. gl_save carries layout padding
	// spaces (it doubles as a right-aligned header elsewhere), so skip them.
	const char *save_lbl = gl_save;
	while (*save_lbl == ' ')
	{
		save_lbl++;
	}
	u32 btn_y = y0 + PG_ROWS * dy + 8;
	u32 save_w = strlen(save_lbl) * 6 + 6;
	u32 reset_w = strlen(gl_reset_global) * 6 + 6;
	u32 save_x = (240 - (save_w + 6 + reset_w)) / 2; // centre the Save+Reset pair
	u32 reset_x = save_x + save_w + 6;
	Clear(save_x, btn_y - 2, save_w, 14, (sel == PG_SAVE) ? gl_color_btn_clean : gl_color_MENU_btn, 1);
	DrawHZText12((char *)save_lbl, 0, save_x + 3, btn_y, gl_color_text, 1);
	Clear(reset_x, btn_y - 2, reset_w, 14, (sel == PG_RESET) ? gl_color_btn_clean : gl_color_MENU_btn, 1);
	DrawHZText12(gl_reset_global, 0, reset_x + 3, btn_y, gl_color_text, 1);

	DrawHZText12(gl_menu_btn, 0, (240 - strlen(gl_menu_btn) * 6) / 2, 146, gl_color_text, 1); // centred hint
}

//---------------------------------------------------------------------------------
void Pergame_settings_window(const char *key)
{
	u16 v[PG_ROWS];
	u32 sel = 0;
	u32 re_show = 1;

	pg_seed(v, key, 1); // current global values, overlaid with this game's record

	while (1)
	{
		VBlankIntrWait();
		if (re_show)
		{
			draw_pergame(key, v, sel);
			re_show = 0;
		}

		scanKeys();
		u16 down = keysDown();

		if (down & KEY_UP)
		{
			sel = (sel == 0) ? (PG_ITEMS - 1) : (sel - 1);
			re_show = 1;
		}
		else if (down & KEY_DOWN)
		{
			sel = (sel == PG_ITEMS - 1) ? 0 : (sel + 1);
			re_show = 1;
		}
		else if (down & KEY_A)
		{
			if (sel < PG_ROWS)
			{
				v[sel] ^= 1;
				re_show = 1;
			}
			else if (sel == PG_SAVE)
			{
				u16 out[16] = {0};
				for (u32 i = 0; i < PG_ROWS; i++)
				{
					out[pg_slot[i]] = v[i];
				}
				Write_pergame_record(key, out);
				return;
			}
			else // PG_RESET
			{
				Delete_pergame_record(key);
				pg_seed(v, key, 0); // record gone -> show the global values, stay on screen
				re_show = 1;
			}
		}
		else if (down & KEY_B)
		{
			return; // discard changes
		}
	}
}
