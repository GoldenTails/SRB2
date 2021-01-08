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
/// \file  sw_video.c
/// \brief Gamma correction LUT stuff
///        Functions to draw patches (by post) directly to screen.
///        Functions to blit a block to the screen.

#include "../doomdef.h"
#include "../r_local.h"
#include "../p_local.h" // stplyr
#include "../g_game.h" // players
#include "../v_video.h"
#include "../st_stuff.h"
#include "../hu_stuff.h"
#include "../f_finale.h"
#include "../r_draw.h"
#include "../console.h"

#include "../i_video.h" // rendermode
#include "../z_zone.h"
#include "../m_misc.h"
#include "../m_random.h"
#include "../doomstat.h"

#include "sw_video.h"

// Each screen is [vid.width*vid.height];
UINT8 *screens[5];
// screens[0] = main display window
// screens[1] = back screen, alternative blitting
// screens[2] = screenshot buffer, gif movie buffer
// screens[3] = fade screen start
// screens[4] = fade screen end, postimage tempoarary buffer

// -------------+
// SWR_SetPalette : Set the current palette to use for palettized graphics
//              :
// -------------+
void SWR_SetPalette(RGBA_t* palette)
{
	I_SetPalette(palette);
}

static UINT8 hudplusalpha[11]  = { 10,  8,  6,  4,  2,  0,  0,  0,  0,  0,  0};
static UINT8 hudminusalpha[11] = { 10,  9,  9,  8,  8,  7,  7,  6,  6,  5,  5};

static const UINT8 *v_colormap = NULL;
static const UINT8 *v_translevel = NULL;

static inline UINT8 standardpdraw(const UINT8 *dest, const UINT8 *source, fixed_t ofs)
{
	(void)dest; return source[ofs>>FRACBITS];
}
static inline UINT8 mappedpdraw(const UINT8 *dest, const UINT8 *source, fixed_t ofs)
{
	(void)dest; return *(v_colormap + source[ofs>>FRACBITS]);
}
static inline UINT8 translucentpdraw(const UINT8 *dest, const UINT8 *source, fixed_t ofs)
{
	return *(v_translevel + ((source[ofs>>FRACBITS]<<8)&0xff00) + (*dest&0xff));
}
static inline UINT8 transmappedpdraw(const UINT8 *dest, const UINT8 *source, fixed_t ofs)
{
	return *(v_translevel + (((*(v_colormap + source[ofs>>FRACBITS]))<<8)&0xff00) + (*dest&0xff));
}

// Draws a patch scaled to arbitrary size.
void SWR_DrawStretchyFixedPatch(fixed_t x, fixed_t y, fixed_t pscale, fixed_t vscale, INT32 scrn, patch_t *patch, const UINT8 *colormap)
{
	UINT8 (*patchdrawfunc)(const UINT8*, const UINT8*, fixed_t);
	UINT32 alphalevel = 0;

	fixed_t col, ofs, colfrac, rowfrac, fdup, vdup;
	INT32 dupx, dupy;
	const column_t *column;
	UINT8 *desttop, *dest, *deststart, *destend;
	const UINT8 *source, *deststop;
	fixed_t pwidth; // patch width
	fixed_t offx = 0; // x offset

	UINT8 perplayershuffle = 0;

	patchdrawfunc = standardpdraw;

	v_translevel = NULL;
	if ((alphalevel = ((scrn & V_ALPHAMASK) >> V_ALPHASHIFT)))
	{
		if (alphalevel == 13)
			alphalevel = hudminusalpha[st_translucency];
		else if (alphalevel == 14)
			alphalevel = 10 - st_translucency;
		else if (alphalevel == 15)
			alphalevel = hudplusalpha[st_translucency];

		if (alphalevel >= 10)
			return; // invis

		if (alphalevel)
		{
			v_translevel = R_GetTranslucencyTable(alphalevel);
			patchdrawfunc = translucentpdraw;
		}
	}

	v_colormap = NULL;
	if (colormap)
	{
		v_colormap = colormap;
		patchdrawfunc = (v_translevel) ? transmappedpdraw : mappedpdraw;
	}

	dupx = vid.dupx;
	dupy = vid.dupy;
	if (scrn & V_SCALEPATCHMASK) switch ((scrn & V_SCALEPATCHMASK) >> V_SCALEPATCHSHIFT)
	{
		case 1: // V_NOSCALEPATCH
			dupx = dupy = 1;
			break;
		case 2: // V_SMALLSCALEPATCH
			dupx = vid.smalldupx;
			dupy = vid.smalldupy;
			break;
		case 3: // V_MEDSCALEPATCH
			dupx = vid.meddupx;
			dupy = vid.meddupy;
			break;
		default:
			break;
	}

	// only use one dup, to avoid stretching (har har)
	dupx = dupy = (dupx < dupy ? dupx : dupy);
	fdup = vdup = FixedMul(dupx<<FRACBITS, pscale);
	if (vscale != pscale)
		vdup = FixedMul(dupx<<FRACBITS, vscale);
	colfrac = FixedDiv(FRACUNIT, fdup);
	rowfrac = FixedDiv(FRACUNIT, vdup);

	// So it turns out offsets aren't scaled in V_NOSCALESTART unless V_OFFSET is applied ...poo, that's terrible
	// For now let's just at least give V_OFFSET the ability to support V_FLIP
	// I'll probably make a better fix for 2.2 where I don't have to worry about breaking existing support for stuff
	// -- Monster Iestyn 29/10/18
	{
		fixed_t offsetx = 0, offsety = 0;

		// left offset
		if (scrn & V_FLIP)
			offsetx = FixedMul((patch->width - patch->leftoffset)<<FRACBITS, pscale) + 1;
		else
			offsetx = FixedMul(patch->leftoffset<<FRACBITS, pscale);

		// top offset
		// TODO: make some kind of vertical version of V_FLIP, maybe by deprecating V_OFFSET in future?!?
		offsety = FixedMul(patch->topoffset<<FRACBITS, vscale);

		if ((scrn & (V_NOSCALESTART|V_OFFSET)) == (V_NOSCALESTART|V_OFFSET)) // Multiply by dupx/dupy for crosshairs
		{
			offsetx = FixedMul(offsetx, dupx<<FRACBITS);
			offsety = FixedMul(offsety, dupy<<FRACBITS);
		}

		// Subtract the offsets from x/y positions
		x -= offsetx;
		y -= offsety;
	}

	if (splitscreen && (scrn & V_PERPLAYER))
	{
		fixed_t adjusty = ((scrn & V_NOSCALESTART) ? vid.height : BASEVIDHEIGHT)<<(FRACBITS-1);
		vdup >>= 1;
		rowfrac <<= 1;
		y >>= 1;
#ifdef QUADS
		if (splitscreen > 1) // 3 or 4 players
		{
			fixed_t adjustx = ((scrn & V_NOSCALESTART) ? vid.height : BASEVIDHEIGHT)<<(FRACBITS-1));
			fdup >>= 1;
			colfrac <<= 1;
			x >>= 1;
			if (stplyr == &players[displayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				if (!(scrn & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 4;
				scrn &= ~V_SNAPTOBOTTOM|V_SNAPTORIGHT;
			}
			else if (stplyr == &players[secondarydisplayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				if (!(scrn & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 8;
				x += adjustx;
				scrn &= ~V_SNAPTOBOTTOM|V_SNAPTOLEFT;
			}
			else if (stplyr == &players[thirddisplayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				if (!(scrn & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 4;
				y += adjusty;
				scrn &= ~V_SNAPTOTOP|V_SNAPTORIGHT;
			}
			else //if (stplyr == &players[fourthdisplayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				if (!(scrn & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 8;
				x += adjustx;
				y += adjusty;
				scrn &= ~V_SNAPTOTOP|V_SNAPTOLEFT;
			}
		}
		else
#endif
		// 2 players
		{
			if (stplyr == &players[displayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle = 1;
				scrn &= ~V_SNAPTOBOTTOM;
			}
			else //if (stplyr == &players[secondarydisplayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle = 2;
				y += adjusty;
				scrn &= ~V_SNAPTOTOP;
			}
		}
	}

	desttop = screens[scrn&V_PARAMMASK];

	if (!desttop)
		return;

	deststop = desttop + vid.rowbytes * vid.height;

	if (scrn & V_NOSCALESTART)
	{
		x >>= FRACBITS;
		y >>= FRACBITS;
		desttop += (y*vid.width) + x;
	}
	else
	{
		x = FixedMul(x,dupx<<FRACBITS);
		y = FixedMul(y,dupy<<FRACBITS);
		x >>= FRACBITS;
		y >>= FRACBITS;

		// Center it if necessary
		if (!(scrn & V_SCALEPATCHMASK))
		{
			// if it's meant to cover the whole screen, black out the rest (ONLY IF TOP LEFT ISN'T TRANSPARENT)
			if (x == 0 && patch->width == BASEVIDWIDTH && y == 0 && patch->height == BASEVIDHEIGHT)
			{
				column = (const column_t *)((const UINT8 *)(patch->columns) + (patch->columnofs[0]));
				if (!column->topdelta)
				{
					source = (const UINT8 *)(column) + 3;
					SWR_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, source[0]);
				}
			}

			if (vid.width != BASEVIDWIDTH * dupx)
			{
				// dupx adjustments pretend that screen width is BASEVIDWIDTH * dupx,
				// so center this imaginary screen
				if (scrn & V_SNAPTORIGHT)
					x += (vid.width - (BASEVIDWIDTH * dupx));
				else if (!(scrn & V_SNAPTOLEFT))
					x += (vid.width - (BASEVIDWIDTH * dupx)) / 2;
				if (perplayershuffle & 4)
					x -= (vid.width - (BASEVIDWIDTH * dupx)) / 4;
				else if (perplayershuffle & 8)
					x += (vid.width - (BASEVIDWIDTH * dupx)) / 4;
			}
			if (vid.height != BASEVIDHEIGHT * dupy)
			{
				// same thing here
				if (scrn & V_SNAPTOBOTTOM)
					y += (vid.height - (BASEVIDHEIGHT * dupy));
				else if (!(scrn & V_SNAPTOTOP))
					y += (vid.height - (BASEVIDHEIGHT * dupy)) / 2;
				if (perplayershuffle & 1)
					y -= (vid.height - (BASEVIDHEIGHT * dupy)) / 4;
				else if (perplayershuffle & 2)
					y += (vid.height - (BASEVIDHEIGHT * dupy)) / 4;
			}
		}

		desttop += (y*vid.width) + x;
	}

	if (pscale != FRACUNIT) // scale width properly
	{
		pwidth = patch->width<<FRACBITS;
		pwidth = FixedMul(pwidth, pscale);
		pwidth = FixedMul(pwidth, dupx<<FRACBITS);
		pwidth >>= FRACBITS;
	}
	else
		pwidth = patch->width * dupx;

	deststart = desttop;
	destend = desttop + pwidth;

	for (col = 0; (col>>FRACBITS) < patch->width; col += colfrac, ++offx, desttop++)
	{
		INT32 topdelta, prevdelta = -1;
		if (scrn & V_FLIP) // offx is measured from right edge instead of left
		{
			if (x+pwidth-offx < 0) // don't draw off the left of the screen (WRAP PREVENTION)
				break;
			if (x+pwidth-offx >= vid.width) // don't draw off the right of the screen (WRAP PREVENTION)
				continue;
		}
		else
		{
			if (x+offx < 0) // don't draw off the left of the screen (WRAP PREVENTION)
				continue;
			if (x+offx >= vid.width) // don't draw off the right of the screen (WRAP PREVENTION)
				break;
		}
		column = (const column_t *)((const UINT8 *)(patch->columns) + (patch->columnofs[col>>FRACBITS]));

		while (column->topdelta != 0xff)
		{
			topdelta = column->topdelta;
			if (topdelta <= prevdelta)
				topdelta += prevdelta;
			prevdelta = topdelta;
			source = (const UINT8 *)(column) + 3;
			dest = desttop;
			if (scrn & V_FLIP)
				dest = deststart + (destend - desttop);
			dest += FixedInt(FixedMul(topdelta<<FRACBITS,vdup))*vid.width;

			for (ofs = 0; dest < deststop && (ofs>>FRACBITS) < column->length; ofs += rowfrac)
			{
				if (dest >= screens[scrn&V_PARAMMASK]) // don't draw off the top of the screen (CRASH PREVENTION)
					*dest = patchdrawfunc(dest, source, ofs);
				dest += vid.width;
			}
			column = (const column_t *)((const UINT8 *)column + column->length + 4);
		}
	}
}

// Draws a patch cropped and scaled to arbitrary size.
void SWR_DrawCroppedPatch(fixed_t x, fixed_t y, fixed_t pscale, INT32 scrn, patch_t *patch, fixed_t sx, fixed_t sy, fixed_t w, fixed_t h)
{
	UINT8 (*patchdrawfunc)(const UINT8*, const UINT8*, fixed_t);
	UINT32 alphalevel = 0;
	// boolean flip = false;

	fixed_t col, ofs, colfrac, rowfrac, fdup;
	INT32 dupx, dupy;
	const column_t *column;
	UINT8 *desttop, *dest;
	const UINT8 *source, *deststop;

	UINT8 perplayershuffle = 0;

	patchdrawfunc = standardpdraw;

	v_translevel = NULL;
	if ((alphalevel = ((scrn & V_ALPHAMASK) >> V_ALPHASHIFT)))
	{
		if (alphalevel == 13)
			alphalevel = hudminusalpha[st_translucency];
		else if (alphalevel == 14)
			alphalevel = 10 - st_translucency;
		else if (alphalevel == 15)
			alphalevel = hudplusalpha[st_translucency];

		if (alphalevel >= 10)
			return; // invis

		if (alphalevel)
		{
			v_translevel = R_GetTranslucencyTable(alphalevel);
			patchdrawfunc = translucentpdraw;
		}
	}

	// only use one dup, to avoid stretching (har har)
	dupx = dupy = (vid.dupx < vid.dupy ? vid.dupx : vid.dupy);
	fdup = FixedMul(dupx<<FRACBITS, pscale);
	colfrac = FixedDiv(FRACUNIT, fdup);
	rowfrac = FixedDiv(FRACUNIT, fdup);

	y -= FixedMul(patch->topoffset<<FRACBITS, pscale);
	x -= FixedMul(patch->leftoffset<<FRACBITS, pscale);

	if (splitscreen && (scrn & V_PERPLAYER))
	{
		fixed_t adjusty = ((scrn & V_NOSCALESTART) ? vid.height : BASEVIDHEIGHT)<<(FRACBITS-1);
		fdup >>= 1;
		rowfrac <<= 1;
		y >>= 1;
		sy >>= 1;
		h >>= 1;
#ifdef QUADS
		if (splitscreen > 1) // 3 or 4 players
		{
			fixed_t adjustx = ((scrn & V_NOSCALESTART) ? vid.height : BASEVIDHEIGHT)<<(FRACBITS-1));
			colfrac <<= 1;
			x >>= 1;
			sx >>= 1;
			w >>= 1;
			if (stplyr == &players[displayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				if (!(scrn & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 4;
				scrn &= ~V_SNAPTOBOTTOM|V_SNAPTORIGHT;
			}
			else if (stplyr == &players[secondarydisplayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				if (!(scrn & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 8;
				x += adjustx;
				sx += adjustx;
				scrn &= ~V_SNAPTOBOTTOM|V_SNAPTOLEFT;
			}
			else if (stplyr == &players[thirddisplayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				if (!(scrn & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 4;
				y += adjusty;
				sy += adjusty;
				scrn &= ~V_SNAPTOTOP|V_SNAPTORIGHT;
			}
			else //if (stplyr == &players[fourthdisplayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				if (!(scrn & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 8;
				x += adjustx;
				sx += adjustx;
				y += adjusty;
				sy += adjusty;
				scrn &= ~V_SNAPTOTOP|V_SNAPTOLEFT;
			}
		}
		else
#endif
		// 2 players
		{
			if (stplyr == &players[displayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				scrn &= ~V_SNAPTOBOTTOM;
			}
			else //if (stplyr == &players[secondarydisplayplayer])
			{
				if (!(scrn & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				y += adjusty;
				sy += adjusty;
				scrn &= ~V_SNAPTOTOP;
			}
		}
	}

	desttop = screens[scrn&V_PARAMMASK];

	if (!desttop)
		return;

	deststop = desttop + vid.rowbytes * vid.height;

	if (scrn & V_NOSCALESTART) {
		x >>= FRACBITS;
		y >>= FRACBITS;
		desttop += (y*vid.width) + x;
	}
	else
	{
		x = FixedMul(x,dupx<<FRACBITS);
		y = FixedMul(y,dupy<<FRACBITS);
		x >>= FRACBITS;
		y >>= FRACBITS;

		// Center it if necessary
		if (!(scrn & V_SCALEPATCHMASK))
		{
			// if it's meant to cover the whole screen, black out the rest
			// no the patch is cropped do not do this ever

			if (vid.width != BASEVIDWIDTH * dupx)
			{
				// dupx adjustments pretend that screen width is BASEVIDWIDTH * dupx,
				// so center this imaginary screen
				if (scrn & V_SNAPTORIGHT)
					x += (vid.width - (BASEVIDWIDTH * dupx));
				else if (!(scrn & V_SNAPTOLEFT))
					x += (vid.width - (BASEVIDWIDTH * dupx)) / 2;
				if (perplayershuffle & 4)
					x -= (vid.width - (BASEVIDWIDTH * dupx)) / 4;
				else if (perplayershuffle & 8)
					x += (vid.width - (BASEVIDWIDTH * dupx)) / 4;
			}
			if (vid.height != BASEVIDHEIGHT * dupy)
			{
				// same thing here
				if (scrn & V_SNAPTOBOTTOM)
					y += (vid.height - (BASEVIDHEIGHT * dupy));
				else if (!(scrn & V_SNAPTOTOP))
					y += (vid.height - (BASEVIDHEIGHT * dupy)) / 2;
				if (perplayershuffle & 1)
					y -= (vid.height - (BASEVIDHEIGHT * dupy)) / 4;
				else if (perplayershuffle & 2)
					y += (vid.height - (BASEVIDHEIGHT * dupy)) / 4;
			}
		}

		desttop += (y*vid.width) + x;
	}

	for (col = sx<<FRACBITS; (col>>FRACBITS) < patch->width && ((col>>FRACBITS) - sx) < w; col += colfrac, ++x, desttop++)
	{
		INT32 topdelta, prevdelta = -1;
		if (x < 0) // don't draw off the left of the screen (WRAP PREVENTION)
			continue;
		if (x >= vid.width) // don't draw off the right of the screen (WRAP PREVENTION)
			break;
		column = (const column_t *)((const UINT8 *)(patch->columns) + (patch->columnofs[col>>FRACBITS]));

		while (column->topdelta != 0xff)
		{
			topdelta = column->topdelta;
			if (topdelta <= prevdelta)
				topdelta += prevdelta;
			prevdelta = topdelta;
			source = (const UINT8 *)(column) + 3;
			dest = desttop;
			if (topdelta-sy > 0)
			{
				dest += FixedInt(FixedMul((topdelta-sy)<<FRACBITS,fdup))*vid.width;
				ofs = 0;
			}
			else
				ofs = (sy-topdelta)<<FRACBITS;

			for (; dest < deststop && (ofs>>FRACBITS) < column->length && (((ofs>>FRACBITS) - sy) + topdelta) < h; ofs += rowfrac)
			{
				if (dest >= screens[scrn&V_PARAMMASK]) // don't draw off the top of the screen (CRASH PREVENTION)
					*dest = patchdrawfunc(dest, source, ofs);
				dest += vid.width;
			}
			column = (const column_t *)((const UINT8 *)column + column->length + 4);
		}
	}
}

//
// SWR_DrawBlock
// Draw a linear block of pixels into the view buffer.
//
void SWR_DrawBlock(INT32 x, INT32 y, INT32 scrn, INT32 width, INT32 height, const UINT8 *src)
{
	UINT8 *dest;
	const UINT8 *deststop;

#ifdef RANGECHECK
	if (x < 0 || x + width > vid.width || y < 0 || y + height > vid.height || (unsigned)scrn > 4)
		I_Error("Bad SWR_DrawBlock");
#endif

	dest = screens[scrn] + y*vid.width + x;
	deststop = screens[scrn] + vid.rowbytes * vid.height;

	while (height--)
	{
		M_Memcpy(dest, src, width);

		src += width;
		dest += vid.width;
		if (dest > deststop)
			return;
	}
}

//  Draw a linear pic, scaled, TOTALLY CRAP CODE!!! OPTIMISE AND ASM!!
//
void SWR_DrawPic(INT32 rx1, INT32 ry1, INT32 scrn, INT32 lumpnum)
{
	INT32 dupx, dupy;
	INT32 x, y;
	UINT8 *src, *dest;
	INT32 width, height;

	pic_t *pic = W_CacheLumpNum(lumpnum, PU_CACHE);

	width = SHORT(pic->width);
	height = SHORT(pic->height);
	scrn &= V_PARAMMASK;

	if (pic->mode != 0)
	{
		CONS_Debug(DBG_RENDER, "pic mode %d not supported in Software\n", pic->mode);
		return;
	}

	dest = screens[scrn] + max(0, ry1 * vid.width) + max(0, rx1);
	// y cliping to the screen
	if (ry1 + height * vid.dupy >= vid.width)
		height = (vid.width - ry1) / vid.dupy - 1;
	// WARNING no x clipping (not needed for the moment)

	for (y = max(0, -ry1 / vid.dupy); y < height; y++)
	{
		for (dupy = vid.dupy; dupy; dupy--)
		{
			src = pic->data + y * width;
			for (x = 0; x < width; x++)
			{
				for (dupx = vid.dupx; dupx; dupx--)
					*dest++ = *src;
				src++;
			}
			dest += vid.width - vid.dupx * width;
		}
	}
}

//
// Fills a box of pixels with a single color, NOTE: scaled to screen size
//
void SWR_DrawFill(INT32 x, INT32 y, INT32 w, INT32 h, INT32 c)
{
	UINT8 *dest;
	const UINT8 *deststop;

	UINT8 perplayershuffle = 0;

	if (splitscreen && (c & V_PERPLAYER))
	{
		fixed_t adjusty = ((c & V_NOSCALESTART) ? vid.height : BASEVIDHEIGHT)>>1;
		h >>= 1;
		y >>= 1;
#ifdef QUADS
		if (splitscreen > 1) // 3 or 4 players
		{
			fixed_t adjustx = ((c & V_NOSCALESTART) ? vid.height : BASEVIDHEIGHT)>>1;
			w >>= 1;
			x >>= 1;
			if (stplyr == &players[displayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 4;
				c &= ~V_SNAPTOBOTTOM|V_SNAPTORIGHT;
			}
			else if (stplyr == &players[secondarydisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 8;
				x += adjustx;
				c &= ~V_SNAPTOBOTTOM|V_SNAPTOLEFT;
			}
			else if (stplyr == &players[thirddisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 4;
				y += adjusty;
				c &= ~V_SNAPTOTOP|V_SNAPTORIGHT;
			}
			else //if (stplyr == &players[fourthdisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 8;
				x += adjustx;
				y += adjusty;
				c &= ~V_SNAPTOTOP|V_SNAPTOLEFT;
			}
		}
		else
#endif
		// 2 players
		{
			if (stplyr == &players[displayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				c &= ~V_SNAPTOBOTTOM;
			}
			else //if (stplyr == &players[secondarydisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				y += adjusty;
				c &= ~V_SNAPTOTOP;
			}
		}
	}

	if (!(c & V_NOSCALESTART))
	{
		INT32 dupx = vid.dupx, dupy = vid.dupy;

		if (x == 0 && y == 0 && w == BASEVIDWIDTH && h == BASEVIDHEIGHT)
		{ // Clear the entire screen, from dest to deststop. Yes, this really works.
			memset(screens[0], (c&255), vid.width * vid.height * vid.bpp);
			return;
		}

		x *= dupx;
		y *= dupy;
		w *= dupx;
		h *= dupy;

		// Center it if necessary
		if (vid.width != BASEVIDWIDTH * dupx)
		{
			// dupx adjustments pretend that screen width is BASEVIDWIDTH * dupx,
			// so center this imaginary screen
			if (c & V_SNAPTORIGHT)
				x += (vid.width - (BASEVIDWIDTH * dupx));
			else if (!(c & V_SNAPTOLEFT))
				x += (vid.width - (BASEVIDWIDTH * dupx)) / 2;
			if (perplayershuffle & 4)
				x -= (vid.width - (BASEVIDWIDTH * dupx)) / 4;
			else if (perplayershuffle & 8)
				x += (vid.width - (BASEVIDWIDTH * dupx)) / 4;
		}
		if (vid.height != BASEVIDHEIGHT * dupy)
		{
			// same thing here
			if (c & V_SNAPTOBOTTOM)
				y += (vid.height - (BASEVIDHEIGHT * dupy));
			else if (!(c & V_SNAPTOTOP))
				y += (vid.height - (BASEVIDHEIGHT * dupy)) / 2;
			if (perplayershuffle & 1)
				y -= (vid.height - (BASEVIDHEIGHT * dupy)) / 4;
			else if (perplayershuffle & 2)
				y += (vid.height - (BASEVIDHEIGHT * dupy)) / 4;
		}
	}

	if (x >= vid.width || y >= vid.height)
		return; // off the screen
	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}

	if (w <= 0 || h <= 0)
		return; // zero width/height wouldn't draw anything
	if (x + w > vid.width)
		w = vid.width - x;
	if (y + h > vid.height)
		h = vid.height - y;

	dest = screens[0] + y*vid.width + x;
	deststop = screens[0] + vid.rowbytes * vid.height;

	c &= 255;

	for (;(--h >= 0) && dest < deststop; dest += vid.width)
		memset(dest, c, w * vid.bpp);
}

// THANK YOU MPC!!!
// and thanks toaster for cleaning it up.

void SWR_DrawConsoleFill(INT32 x, INT32 y, INT32 w, INT32 h, INT32 c)
{
	UINT8 *dest;
	const UINT8 *deststop;
	INT32 u;
	UINT8 *fadetable;
	UINT32 alphalevel = 0;
	UINT8 perplayershuffle = 0;

	if ((alphalevel = ((c & V_ALPHAMASK) >> V_ALPHASHIFT)))
	{
		if (alphalevel == 13)
			alphalevel = hudminusalpha[st_translucency];
		else if (alphalevel == 14)
			alphalevel = 10 - st_translucency;
		else if (alphalevel == 15)
			alphalevel = hudplusalpha[st_translucency];

		if (alphalevel >= 10)
			return; // invis
	}

	if (splitscreen && (c & V_PERPLAYER))
	{
		fixed_t adjusty = ((c & V_NOSCALESTART) ? vid.height : BASEVIDHEIGHT)>>1;
		h >>= 1;
		y >>= 1;
#ifdef QUADS
		if (splitscreen > 1) // 3 or 4 players
		{
			fixed_t adjustx = ((c & V_NOSCALESTART) ? vid.height : BASEVIDHEIGHT)>>1;
			w >>= 1;
			x >>= 1;
			if (stplyr == &players[displayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 4;
				c &= ~V_SNAPTOBOTTOM|V_SNAPTORIGHT;
			}
			else if (stplyr == &players[secondarydisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 8;
				x += adjustx;
				c &= ~V_SNAPTOBOTTOM|V_SNAPTOLEFT;
			}
			else if (stplyr == &players[thirddisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 4;
				y += adjusty;
				c &= ~V_SNAPTOTOP|V_SNAPTORIGHT;
			}
			else //if (stplyr == &players[fourthdisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 8;
				x += adjustx;
				y += adjusty;
				c &= ~V_SNAPTOTOP|V_SNAPTOLEFT;
			}
		}
		else
#endif
		// 2 players
		{
			if (stplyr == &players[displayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				c &= ~V_SNAPTOBOTTOM;
			}
			else //if (stplyr == &players[secondarydisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				y += adjusty;
				c &= ~V_SNAPTOTOP;
			}
		}
	}

	if (!(c & V_NOSCALESTART))
	{
		INT32 dupx = vid.dupx, dupy = vid.dupy;

		x *= dupx;
		y *= dupy;
		w *= dupx;
		h *= dupy;

		// Center it if necessary
		if (vid.width != BASEVIDWIDTH * dupx)
		{
			// dupx adjustments pretend that screen width is BASEVIDWIDTH * dupx,
			// so center this imaginary screen
			if (c & V_SNAPTORIGHT)
				x += (vid.width - (BASEVIDWIDTH * dupx));
			else if (!(c & V_SNAPTOLEFT))
				x += (vid.width - (BASEVIDWIDTH * dupx)) / 2;
			if (perplayershuffle & 4)
				x -= (vid.width - (BASEVIDWIDTH * dupx)) / 4;
			else if (perplayershuffle & 8)
				x += (vid.width - (BASEVIDWIDTH * dupx)) / 4;
		}
		if (vid.height != BASEVIDHEIGHT * dupy)
		{
			// same thing here
			if (c & V_SNAPTOBOTTOM)
				y += (vid.height - (BASEVIDHEIGHT * dupy));
			else if (!(c & V_SNAPTOTOP))
				y += (vid.height - (BASEVIDHEIGHT * dupy)) / 2;
			if (perplayershuffle & 1)
				y -= (vid.height - (BASEVIDHEIGHT * dupy)) / 4;
			else if (perplayershuffle & 2)
				y += (vid.height - (BASEVIDHEIGHT * dupy)) / 4;
		}
	}

	if (x >= vid.width || y >= vid.height)
		return; // off the screen
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}

	if (w <= 0 || h <= 0)
		return; // zero width/height wouldn't draw anything
	if (x + w > vid.width)
		w = vid.width-x;
	if (y + h > vid.height)
		h = vid.height-y;

	dest = screens[0] + y*vid.width + x;
	deststop = screens[0] + vid.rowbytes * vid.height;

	c &= 255;

	// Jimita (12-04-2018)
	if (alphalevel)
	{
		fadetable = R_GetTranslucencyTable(alphalevel) + (c*256);
		for (;(--h >= 0) && dest < deststop; dest += vid.width)
		{
			u = 0;
			while (u < w)
			{
				dest[u] = fadetable[consolebgmap[dest[u]]];
				u++;
			}
		}
	}
	else
	{
		for (;(--h >= 0) && dest < deststop; dest += vid.width)
		{
			u = 0;
			while (u < w)
			{
				dest[u] = consolebgmap[dest[u]];
				u++;
			}
		}
	}
}

//
// If color is 0x00 to 0xFF, draw transtable (strength range 0-9).
// Else, use COLORMAP lump (strength range 0-31).
// c is not color, it is for flags only. transparency flags will be ignored.
// IF YOU ARE NOT CAREFUL, THIS CAN AND WILL CRASH!
// I have kept the safety checks for strength out of this function;
// I don't trust Lua users with it, so it doesn't matter.
//
void SWR_DrawFadeFill(INT32 x, INT32 y, INT32 w, INT32 h, INT32 c, UINT16 color, UINT8 strength)
{
	UINT8 *dest;
	const UINT8 *deststop;
	INT32 u;
	UINT8 *fadetable;
	UINT8 perplayershuffle = 0;

	if (splitscreen && (c & V_PERPLAYER))
	{
		fixed_t adjusty = ((c & V_NOSCALESTART) ? vid.height : BASEVIDHEIGHT)>>1;
		h >>= 1;
		y >>= 1;
#ifdef QUADS
		if (splitscreen > 1) // 3 or 4 players
		{
			fixed_t adjustx = ((c & V_NOSCALESTART) ? vid.height : BASEVIDHEIGHT)>>1;
			w >>= 1;
			x >>= 1;
			if (stplyr == &players[displayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 4;
				c &= ~V_SNAPTOBOTTOM|V_SNAPTORIGHT;
			}
			else if (stplyr == &players[secondarydisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 8;
				x += adjustx;
				c &= ~V_SNAPTOBOTTOM|V_SNAPTOLEFT;
			}
			else if (stplyr == &players[thirddisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 4;
				y += adjusty;
				c &= ~V_SNAPTOTOP|V_SNAPTORIGHT;
			}
			else //if (stplyr == &players[fourthdisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				if (!(c & (V_SNAPTOLEFT|V_SNAPTORIGHT)))
					perplayershuffle |= 8;
				x += adjustx;
				y += adjusty;
				c &= ~V_SNAPTOTOP|V_SNAPTOLEFT;
			}
		}
		else
#endif
		// 2 players
		{
			if (stplyr == &players[displayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 1;
				c &= ~V_SNAPTOBOTTOM;
			}
			else //if (stplyr == &players[secondarydisplayplayer])
			{
				if (!(c & (V_SNAPTOTOP|V_SNAPTOBOTTOM)))
					perplayershuffle |= 2;
				y += adjusty;
				c &= ~V_SNAPTOTOP;
			}
		}
	}

	if (!(c & V_NOSCALESTART))
	{
		INT32 dupx = vid.dupx, dupy = vid.dupy;

		x *= dupx;
		y *= dupy;
		w *= dupx;
		h *= dupy;

		// Center it if necessary
		if (vid.width != BASEVIDWIDTH * dupx)
		{
			// dupx adjustments pretend that screen width is BASEVIDWIDTH * dupx,
			// so center this imaginary screen
			if (c & V_SNAPTORIGHT)
				x += (vid.width - (BASEVIDWIDTH * dupx));
			else if (!(c & V_SNAPTOLEFT))
				x += (vid.width - (BASEVIDWIDTH * dupx)) / 2;
			if (perplayershuffle & 4)
				x -= (vid.width - (BASEVIDWIDTH * dupx)) / 4;
			else if (perplayershuffle & 8)
				x += (vid.width - (BASEVIDWIDTH * dupx)) / 4;
		}
		if (vid.height != BASEVIDHEIGHT * dupy)
		{
			// same thing here
			if (c & V_SNAPTOBOTTOM)
				y += (vid.height - (BASEVIDHEIGHT * dupy));
			else if (!(c & V_SNAPTOTOP))
				y += (vid.height - (BASEVIDHEIGHT * dupy)) / 2;
			if (perplayershuffle & 1)
				y -= (vid.height - (BASEVIDHEIGHT * dupy)) / 4;
			else if (perplayershuffle & 2)
				y += (vid.height - (BASEVIDHEIGHT * dupy)) / 4;
		}
	}

	if (x >= vid.width || y >= vid.height)
		return; // off the screen
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}

	if (w <= 0 || h <= 0)
		return; // zero width/height wouldn't draw anything
	if (x + w > vid.width)
		w = vid.width-x;
	if (y + h > vid.height)
		h = vid.height-y;

	dest = screens[0] + y*vid.width + x;
	deststop = screens[0] + vid.rowbytes * vid.height;

	c &= 255;

	fadetable = ((color & 0xFF00) // Color is not palette index?
		? ((UINT8 *)colormaps + strength*256) // Do COLORMAP fade.
		: ((UINT8 *)R_GetTranslucencyTable((9-strength)+1) + color*256)); // Else, do TRANSMAP** fade.
	for (;(--h >= 0) && dest < deststop; dest += vid.width)
	{
		u = 0;
		while (u < w)
		{
			dest[u] = fadetable[dest[u]];
			u++;
		}
	}
}

//
// Fills a box of pixels using a flat texture as a pattern, scaled to screen size.
//
void SWR_DrawFlatFill(INT32 x, INT32 y, INT32 w, INT32 h, lumpnum_t flatnum)
{
	INT32 u, v, dupx, dupy;
	fixed_t dx, dy, xfrac, yfrac;
	const UINT8 *src, *deststop;
	UINT8 *flat, *dest;
	size_t size, lflatsize, flatshift;

	size = W_LumpLength(flatnum);

	switch (size)
	{
		case 4194304: // 2048x2048 lump
			lflatsize = 2048;
			flatshift = 10;
			break;
		case 1048576: // 1024x1024 lump
			lflatsize = 1024;
			flatshift = 9;
			break;
		case 262144:// 512x512 lump
			lflatsize = 512;
			flatshift = 8;
			break;
		case 65536: // 256x256 lump
			lflatsize = 256;
			flatshift = 7;
			break;
		case 16384: // 128x128 lump
			lflatsize = 128;
			flatshift = 7;
			break;
		case 1024: // 32x32 lump
			lflatsize = 32;
			flatshift = 5;
			break;
		default: // 64x64 lump
			lflatsize = 64;
			flatshift = 6;
			break;
	}

	flat = W_CacheLumpNum(flatnum, PU_CACHE);

	dupx = dupy = (vid.dupx < vid.dupy ? vid.dupx : vid.dupy);

	dest = screens[0] + y*dupy*vid.width + x*dupx;
	deststop = screens[0] + vid.rowbytes * vid.height;

	// from SWR_DrawScaledPatch
	if (vid.width != BASEVIDWIDTH * dupx)
	{
		// dupx adjustments pretend that screen width is BASEVIDWIDTH * dupx,
		// so center this imaginary screen
		dest += (vid.width - (BASEVIDWIDTH * dupx)) / 2;
	}
	if (vid.height != BASEVIDHEIGHT * dupy)
	{
		// same thing here
		dest += (vid.height - (BASEVIDHEIGHT * dupy)) * vid.width / 2;
	}

	w *= dupx;
	h *= dupy;

	dx = FixedDiv(FRACUNIT, dupx<<(FRACBITS-2));
	dy = FixedDiv(FRACUNIT, dupy<<(FRACBITS-2));

	yfrac = 0;
	for (v = 0; v < h; v++, dest += vid.width)
	{
		xfrac = 0;
		src = flat + (((yfrac>>FRACBITS) & (lflatsize - 1)) << flatshift);
		for (u = 0; u < w; u++)
		{
			if (&dest[u] > deststop)
				return;
			dest[u] = src[(xfrac>>FRACBITS)&(lflatsize-1)];
			xfrac += dx;
		}
		yfrac += dy;
	}
}

//
// Fade all the screen buffer, so that the menu is more readable,
// especially now that we use the small hufont in the menus...
// If color is 0x00 to 0xFF, draw transtable (strength range 0-9).
// Else, use COLORMAP lump (strength range 0-31).
// IF YOU ARE NOT CAREFUL, THIS CAN AND WILL CRASH!
// I have kept the safety checks out of this function;
// the v.fadeScreen Lua interface handles those.
//
void SWR_DrawFadeScreen(UINT16 color, UINT8 strength)
{
	const UINT8 *fadetable = ((color & 0xFF00) // Color is not palette index?
	? ((UINT8 *)(((color & 0x0F00) == 0x0A00) ? fadecolormap // Do fadecolormap fade.
	: (((color & 0x0F00) == 0x0B00) ? fadecolormap + (256 * FADECOLORMAPROWS) // Do white fadecolormap fade.
	: colormaps)) + strength*256) // Do COLORMAP fade.
	: ((UINT8 *)R_GetTranslucencyTable((9-strength)+1) + color*256)); // Else, do TRANSMAP** fade.
	const UINT8 *deststop = screens[0] + vid.rowbytes * vid.height;
	UINT8 *buf = screens[0];

	// heavily simplified -- we don't need to know x or y
	// position when we're doing a full screen fade
	for (; buf < deststop; ++buf)
		*buf = fadetable[*buf];
}

// Simple translucency with one color, over a set number of lines starting from the top.
void SWR_DrawConsoleBack(INT32 plines)
{
	UINT8 *deststop, *buf;

	// heavily simplified -- we don't need to know x or y position,
	// just the stop position
	deststop = screens[0] + vid.rowbytes * min(plines, vid.height);
	for (buf = screens[0]; buf < deststop; ++buf)
		*buf = consolebgmap[*buf];
}

// Very similar to F_DrawFadeConsBack, except we draw from the middle(-ish) of the screen to the bottom.
void SWR_DrawPromptBack(INT32 color, INT32 boxheight)
{
	UINT8 *deststop, *buf;

	CON_SetupBackColormapEx(color, true);

	// heavily simplified -- we don't need to know x or y position,
	// just the start and stop positions
	buf = deststop = screens[0] + vid.rowbytes * vid.height;
	if (boxheight < 0)
		buf += vid.rowbytes * boxheight;
	else // 4 lines of space plus gaps between and some leeway
		buf -= vid.rowbytes * ((boxheight * 4) + (boxheight/2)*5);
	for (; buf < deststop; ++buf)
		*buf = promptbgmap[*buf];
}

boolean *heatshifter = NULL;
INT32 lastheight = 0;
INT32 heatindex[2] = { 0, 0 };

//
// SWR_DoPostProcessor
//
// Perform a particular image postprocessing function.
//
void SWR_DoPostProcessor(INT32 view, postimg_t type, INT32 param)
{
#if NUMSCREENS < 5
	// do not enable image post processing for ARM, SH and MIPS CPUs
	(void)view;
	(void)type;
	(void)param;
#else
	INT32 height, yoffset;

	if (view < 0 || view >= 2 || (view == 1 && !splitscreen))
		return;

	if (splitscreen)
		height = vid.height/2;
	else
		height = vid.height;

	if (view == 1)
		yoffset = vid.height/2;
	else
		yoffset = 0;

	if (type == postimg_water)
	{
		UINT8 *tmpscr = screens[4];
		UINT8 *srcscr = screens[0];
		INT32 y;
		angle_t disStart = (leveltime * 128) & FINEMASK; // in 0 to FINEANGLE
		INT32 newpix;
		INT32 sine;
		//UINT8 *transme = R_GetTranslucencyTable(tr_trans50);

		for (y = yoffset; y < yoffset+height; y++)
		{
			sine = (FINESINE(disStart)*5)>>FRACBITS;
			newpix = abs(sine);

			if (sine < 0)
			{
				M_Memcpy(&tmpscr[y*vid.width+newpix], &srcscr[y*vid.width], vid.width-newpix);

				// Cleanup edge
				while (newpix)
				{
					tmpscr[y*vid.width+newpix] = srcscr[y*vid.width];
					newpix--;
				}
			}
			else
			{
				M_Memcpy(&tmpscr[y*vid.width+0], &srcscr[y*vid.width+sine], vid.width-newpix);

				// Cleanup edge
				while (newpix)
				{
					tmpscr[y*vid.width+vid.width-newpix] = srcscr[y*vid.width+(vid.width-1)];
					newpix--;
				}
			}

/*
Unoptimized version
			for (x = 0; x < vid.width*vid.bpp; x++)
			{
				newpix = (x + sine);

				if (newpix < 0)
					newpix = 0;
				else if (newpix >= vid.width)
					newpix = vid.width-1;

				tmpscr[y*vid.width + x] = srcscr[y*vid.width+newpix]; // *(transme + (srcscr[y*vid.width+x]<<8) + srcscr[y*vid.width+newpix]);
			}*/
			disStart += 22;//the offset into the displacement map, increment each game loop
			disStart &= FINEMASK; //clip it to FINEMASK
		}

		VID_BlitLinearScreen(tmpscr+vid.width*vid.bpp*yoffset, screens[0]+vid.width*vid.bpp*yoffset,
				vid.width*vid.bpp, height, vid.width*vid.bpp, vid.width);
	}
	else if (type == postimg_motion) // Motion Blur!
	{
		UINT8 *tmpscr = screens[4];
		UINT8 *srcscr = screens[0];
		INT32 x, y;

		// TODO: Add a postimg_param so that we can pick the translucency level...
		UINT8 *transme = R_GetTranslucencyTable(param);

		for (y = yoffset; y < yoffset+height; y++)
		{
			for (x = 0; x < vid.width; x++)
			{
				tmpscr[y*vid.width + x]
					=     colormaps[*(transme     + (srcscr   [y*vid.width+x ] <<8) + (tmpscr[y*vid.width+x]))];
			}
		}
		VID_BlitLinearScreen(tmpscr+vid.width*vid.bpp*yoffset, screens[0]+vid.width*vid.bpp*yoffset,
				vid.width*vid.bpp, height, vid.width*vid.bpp, vid.width);
	}
	else if (type == postimg_flip) // Flip the screen upside-down
	{
		UINT8 *tmpscr = screens[4];
		UINT8 *srcscr = screens[0];
		INT32 y, y2;

		for (y = yoffset, y2 = yoffset+height - 1; y < yoffset+height; y++, y2--)
			M_Memcpy(&tmpscr[y2*vid.width], &srcscr[y*vid.width], vid.width);

		VID_BlitLinearScreen(tmpscr+vid.width*vid.bpp*yoffset, screens[0]+vid.width*vid.bpp*yoffset,
				vid.width*vid.bpp, height, vid.width*vid.bpp, vid.width);
	}
	else if (type == postimg_heat) // Heat wave
	{
		UINT8 *tmpscr = screens[4];
		UINT8 *srcscr = screens[0];
		INT32 y;

		// Make sure table is built
		if (heatshifter == NULL || lastheight != height)
		{
			if (heatshifter)
				Z_Free(heatshifter);

			heatshifter = Z_Calloc(height * sizeof(boolean), PU_STATIC, NULL);

			for (y = 0; y < height; y++)
			{
				if (M_RandomChance(FRACUNIT/8)) // 12.5%
					heatshifter[y] = true;
			}

			heatindex[0] = heatindex[1] = 0;
			lastheight = height;
		}

		for (y = yoffset; y < yoffset+height; y++)
		{
			if (heatshifter[heatindex[view]++])
			{
				// Shift this row of pixels to the right by 2
				tmpscr[y*vid.width] = srcscr[y*vid.width];
				M_Memcpy(&tmpscr[y*vid.width+vid.dupx], &srcscr[y*vid.width], vid.width-vid.dupx);
			}
			else
				M_Memcpy(&tmpscr[y*vid.width], &srcscr[y*vid.width], vid.width);

			heatindex[view] %= height;
		}

		heatindex[view]++;
		heatindex[view] %= vid.height;

		VID_BlitLinearScreen(tmpscr+vid.width*vid.bpp*yoffset, screens[0]+vid.width*vid.bpp*yoffset,
				vid.width*vid.bpp, height, vid.width*vid.bpp, vid.width);
	}
#endif
}

// SWR_Init
// old software stuff, buffers are allocated at video mode setup
// here we set the screens[x] pointers accordingly
// WARNING: called at runtime (don't init cvar here)
void SWR_Init(void)
{
	INT32 i;
	UINT8 *base = vid.buffer;
	const INT32 screensize = vid.rowbytes * vid.height;

	for (i = 0; i < NUMSCREENS; i++)
		screens[i] = NULL;

	// start address of NUMSCREENS * width*height vidbuffers
	if (base)
	{
		for (i = 0; i < NUMSCREENS; i++)
			screens[i] = base + i*screensize;
	}

	if (vid.direct)
		screens[0] = vid.direct;

#ifdef DEBUG
	CONS_Debug(DBG_RENDER, "SWR_Init done:\n");
	for (i = 0; i < NUMSCREENS; i++)
		CONS_Debug(DBG_RENDER, " screens[%d] = %x\n", i, screens[i]);
#endif
}
