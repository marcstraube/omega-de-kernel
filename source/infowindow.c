#include <gba_base.h>
#include <gba_input.h>
#include <gba_systemcalls.h>
#include <stdio.h>
#include <string.h>

#include "ez_define.h"
#include "ezkernel.h"
#include "draw.h"
#include "infowindow.h"

// Layout (240x160). Title on top, then a block of word-wrapped text lines, then
// a footer hint. INFO_USABLE_W is chosen so a full line (<= INFO_USABLE_W px)
// never trips the DrawHZText12 right-edge clip at x = INFO_MARGIN_X.
#define INFO_MARGIN_X 4
#define INFO_USABLE_W (240 - 2 * INFO_MARGIN_X) // 232 px
// The window draws over gImage_SD, whose top ~20px hold the list tabs. Keep all
// content below that strip (the settings screen likewise starts its content at
// y=24) so the title no longer sits on the tabs.
#define INFO_TITLE_Y 22
#define INFO_TEXT_TOP 36
#define INFO_LINE_H 13
#define INFO_VIS_LINES 8 // 36 + 8*13 = 140, above the footer
// Footer at y=148 so the 12px glyph rows (148..159) stay fully on the 240x160
// screen; y=149 would push the bottom pixel row off-screen.
#define INFO_FOOTER_Y 148
#define INFO_MAX_LINES 400

// Wrapped-line table: byte offset + byte length into the text buffer. A line is
// never split inside a GBK double-byte pair (the wrapper steps two bytes at a
// time over them), so DrawHZText12 always sees aligned lead/trail bytes.
static u16 g_line_off[INFO_MAX_LINES];
static u16 g_line_len[INFO_MAX_LINES];

// Greedy word-wrap of buf[0..len) into the line table. Pixel width is exact:
// ASCII is 1 byte / 6 px, GBK double-byte is 2 bytes / 12 px, matching
// DrawHZText12. Breaks at the last space when possible, otherwise hard-splits an
// over-long word. '\n' forces a break; '\r' is ignored. Returns the line count.
static u32 wrap_text(const char *buf, u32 len)
{
	u32 nlines = 0;
	u32 i = 0;

	while (i < len && nlines < INFO_MAX_LINES)
	{
		u32 start = i;
		u32 width = 0;
		s32 last_space = -1; // byte index of the last space on this line
		u32 brk;             // line end (exclusive)
		u32 next;            // start of the next line

		for (;;)
		{
			if (i >= len) // end of buffer
			{
				brk = len;
				next = len;
				break;
			}
			u8 b = (u8)buf[i];
			if (b == '\n') // hard line break
			{
				brk = i;
				next = i + 1;
				break;
			}
			if (b == '\r') // ignore carriage returns
			{
				i++;
				continue;
			}

			u32 nb = (b >= 0x80 && i + 1 < len) ? 2 : 1; // GBK pair vs single byte
			u32 cw = (nb == 2) ? 12 : 6;

			if (width + cw > INFO_USABLE_W) // line is full, wrap before this char
			{
				if (last_space > (s32)start) // break at the word boundary
				{
					brk = (u32)last_space;
					next = brk + 1; // drop the space
				}
				else // single word wider than the line: hard split
				{
					brk = i;
					next = i;
				}
				break;
			}

			if (b == ' ')
				last_space = (s32)i;
			width += cw;
			i += nb;
		}

		g_line_off[nlines] = (u16)start;
		g_line_len[nlines] = (u16)(brk - start);
		nlines++;
		i = next;
	}

	return nlines;
}

// Clear the text block to the list background and draw the visible lines.
static void draw_text_block(const char *buf, u32 nlines, u32 scroll)
{
	ClearWithBG((u16 *)gImage_SD, 0, INFO_TEXT_TOP, 240, INFO_VIS_LINES * INFO_LINE_H, 1);

	for (u32 row = 0; row < INFO_VIS_LINES; row++)
	{
		u32 ln = scroll + row;
		if (ln >= nlines)
			break;
		u16 off = g_line_off[ln];
		u16 llen = g_line_len[ln];
		if (llen == 0) // blank line: skip (DrawHZText12 len==0 means "use strlen")
			continue;
		DrawHZText12((char *)(buf + off), llen, INFO_MARGIN_X, INFO_TEXT_TOP + row * INFO_LINE_H, gl_color_text, 1);
	}
}

// Number of leading bytes of str that fit within max_px, never splitting a GBK
// double-byte pair. Uses the same 6px/byte metric as DrawHZText12, so the result
// is a real width clip (DrawHZText12's own clip at draw.c:152 is dead code -- it
// trims the unused 'len' copy, not the strlen-derived length it actually draws).
static u32 fit_bytes(const char *str, u32 max_px)
{
	u32 i = 0;
	u32 width = 0;
	while (str[i])
	{
		u8 b = (u8)str[i];
		if (b >= 0x80 && !str[i + 1])
			break; // unpaired trailing GBK lead byte: drop it (mirrors the loader trim)
		u32 nb = (b >= 0x80) ? 2 : 1;
		u32 cw = (nb == 2) ? 12 : 6;
		if (width + cw > max_px)
			break;
		width += cw;
		i += nb;
	}
	return i;
}

// Draw the footer hint, including scroll affordance when the text overflows.
static void draw_footer(u32 scrollable)
{
	ClearWithBG((u16 *)gImage_SD, 0, INFO_FOOTER_Y, 240, INFO_LINE_H, 1);
	if (scrollable)
		DrawHZText12("UP/DOWN: Scroll   B: Back", 0, INFO_MARGIN_X, INFO_FOOTER_Y, gl_color_selected, 1);
	else
		DrawHZText12("B: Back", 0, INFO_MARGIN_X, INFO_FOOTER_Y, gl_color_selected, 1);
}

void Show_game_info_window(const char *buf, u32 len, const char *title)
{
	u32 nlines;
	u32 max_scroll;
	u32 scroll = 0;

	// Full-screen background so the text block and footer clear cleanly.
	DrawPic((u16 *)gImage_SD, 0, 0, 240, 160, 0, 0, 1);

	// Title (the ROM file name), clipped to the usable width here because
	// DrawHZText12 does not clip a strlen-measured string (see fit_bytes).
	if (title)
	{
		u32 tlen = fit_bytes(title, INFO_USABLE_W);
		if (tlen) // tlen==0 would make DrawHZText12 fall back to strlen
			DrawHZText12((char *)title, tlen, INFO_MARGIN_X, INFO_TITLE_Y, gl_color_selected, 1);
	}

	if (len == 0) // no per-game info file: show a placeholder instead of text
	{
		const char *msg = "No info for this game.";
		nlines = wrap_text(msg, strlen(msg));
		draw_text_block(msg, nlines, 0);
		draw_footer(0);
	}
	else
	{
		nlines = wrap_text(buf, len);
		max_scroll = (nlines > INFO_VIS_LINES) ? (nlines - INFO_VIS_LINES) : 0;
		draw_text_block(buf, nlines, scroll);
		draw_footer(max_scroll > 0);

		while (1)
		{
			VBlankIntrWait();
			scanKeys();
			u16 keys = keysDown();
			u16 rep = keysDownRepeat();

			if (keys & (KEY_B | KEY_L)) // return to the list
				return;

			if (max_scroll > 0)
			{
				u32 old = scroll;
				if ((rep & KEY_DOWN) && scroll < max_scroll)
					scroll++;
				if ((rep & KEY_UP) && scroll > 0)
					scroll--;
				if (scroll != old)
					draw_text_block(buf, nlines, scroll);
			}
		}
	}

	// Placeholder path falls through to here: wait for a key, then return.
	while (1)
	{
		VBlankIntrWait();
		scanKeys();
		if (keysDown() & (KEY_B | KEY_L))
			return;
	}
}
