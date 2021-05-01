// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  am_map.c
/// \brief Code for the 'automap', former Doom feature used for DEVMODE testing

#include "sw_automap.h"
#include "sw_draw.h"
#include "../doomdef.h"
#include "../v_video.h"
#include "../am_map.h"

//
// Draws a pixel.
//
static void SWR_drawAMPixel(INT32 xx, INT32 yy, INT32 cc)
{
	UINT8 *dest = screens[0];
	if (xx < 0 || yy < 0 || xx >= vid.width || yy >= vid.height)
		return; // off the screen
	dest[(yy*vid.width) + xx] = cc;
}

//
// Classic Bresenham w/ whatever optimizations needed for speed
//
void SWR_drawAMline(const fline_t *fl, INT32 color)
{
	INT32 x, y, dx, dy, sx, sy, ax, ay, d;

#ifdef _DEBUG
	static INT32 num = 0;

	// For debugging only
	if (fl->a.x < 0 || fl->a.x >= f_w
	|| fl->a.y < 0 || fl->a.y >= f_h
	|| fl->b.x < 0 || fl->b.x >= f_w
	|| fl->b.y < 0 || fl->b.y >= f_h)
	{
		CONS_Debug(DBG_RENDER, "line clipping problem %d\n", num++);
		return;
	}
#endif

	dx = fl->b.x - fl->a.x;
	ax = 2 * (dx < 0 ? -dx : dx);
	sx = dx < 0 ? -1 : 1;

	dy = fl->b.y - fl->a.y;
	ay = 2 * (dy < 0 ? -dy : dy);
	sy = dy < 0 ? -1 : 1;

	x = fl->a.x;
	y = fl->a.y;

	if (ax > ay)
	{
		d = ay - ax/2;
		for (;;)
		{
			SWR_drawAMPixel(x, y, color);
			if (x == fl->b.x)
				return;
			if (d >= 0)
			{
				y += sy;
				d -= ax;
			}
			x += sx;
			d += ay;
		}
	}
	else
	{
		d = ax - ay/2;
		for (;;)
		{
			SWR_drawAMPixel(x, y, color);
			if (y == fl->b.y)
				return;
			if (d >= 0)
			{
				x += sx;
				d -= ay;
			}
			y += sy;
			d += ax;
		}
	}
}