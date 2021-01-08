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
/// \file  sw_video.h
/// \brief Gamma correction LUT

#ifndef __SW_VIDEO__
#define __SW_VIDEO__

#include "../doomdef.h"
#include "../doomtype.h"
#include "../r_defs.h"
#include "../v_video.h"

//
// VIDEO
//

// Screen 0 is the screen updated by I_Update screen.
// Screen 1 is an extra buffer.

extern UINT8 *screens[5];

// Allocates buffer screens, call before R_Init.
void SWR_Init(void);

// Set the current RGB palette lookup to use for palettized graphics
void SWR_SetPalette(RGBA_t* palette);

// defines for old functions
#define SWR_DrawPatch(x,y,s,p) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT, s|V_NOSCALESTART|V_NOSCALEPATCH, p, NULL)
#define SWR_DrawTranslucentMappedPatch(x,y,s,p,c) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT, s, p, c)
#define SWR_DrawSmallTranslucentMappedPatch(x,y,s,p,c) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT/2, s, p, c)
#define SWR_DrawTinyTranslucentMappedPatch(x,y,s,p,c) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT/4, s, p, c)
#define SWR_DrawMappedPatch(x,y,s,p,c) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT, s, p, c)
#define SWR_DrawSmallMappedPatch(x,y,s,p,c) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT/2, s, p, c)
#define SWR_DrawTinyMappedPatch(x,y,s,p,c) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT/4, s, p, c)
#define SWR_DrawScaledPatch(x,y,s,p) SWR_DrawFixedPatch((x)*FRACUNIT, (y)<<FRACBITS, FRACUNIT, s, p, NULL)
#define SWR_DrawSmallScaledPatch(x,y,s,p) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT/2, s, p, NULL)
#define SWR_DrawTinyScaledPatch(x,y,s,p) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT/4, s, p, NULL)
#define SWR_DrawTranslucentPatch(x,y,s,p) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT, s, p, NULL)
#define SWR_DrawSmallTranslucentPatch(x,y,s,p) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT/2, s, p, NULL)
#define SWR_DrawTinyTranslucentPatch(x,y,s,p) SWR_DrawFixedPatch((x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT/4, s, p, NULL)
#define SWR_DrawSciencePatch(x,y,s,p,sc) SWR_DrawFixedPatch(x,y,sc,s,p,NULL)
#define SWR_DrawFixedPatch(x,y,sc,s,p,c) SWR_DrawStretchyFixedPatch(x,y,sc,sc,s,p,c)
void SWR_DrawStretchyFixedPatch(fixed_t x, fixed_t y, fixed_t pscale, fixed_t vscale, INT32 scrn, patch_t *patch, const UINT8 *colormap);
void SWR_DrawCroppedPatch(fixed_t x, fixed_t y, fixed_t pscale, INT32 scrn, patch_t *patch, fixed_t sx, fixed_t sy, fixed_t w, fixed_t h);

// Draw a linear block of pixels into the view buffer.
void SWR_DrawBlock(INT32 x, INT32 y, INT32 scrn, INT32 width, INT32 height, const UINT8 *src);

// draw a pic_t, SCALED
void SWR_DrawPic(INT32 px1, INT32 py1, INT32 scrn, INT32 lumpnum);

// fill a box with a single color
void SWR_DrawFill(INT32 x, INT32 y, INT32 w, INT32 h, INT32 c);
void SWR_DrawConsoleFill(INT32 x, INT32 y, INT32 w, INT32 h, INT32 c);
// fill a box with a flat as a pattern
void SWR_DrawFlatFill(INT32 x, INT32 y, INT32 w, INT32 h, lumpnum_t flatnum);

// fade down the screen buffer before drawing the menu over
void SWR_DrawFadeScreen(UINT16 color, UINT8 strength);
// available to lua over my dead body, which will probably happen in this heat
void SWR_DrawFadeFill(INT32 x, INT32 y, INT32 w, INT32 h, INT32 c, UINT16 color, UINT8 strength);

void SWR_DrawConsoleBack(INT32 plines);
void SWR_DrawPromptBack(INT32 color, INT32 boxheight);

void SWR_DoPostProcessor(INT32 view, postimg_t type, INT32 param);

void SWR_BlitLinearScreen(const UINT8 *srcptr, UINT8 *destptr, INT32 width, INT32 height, size_t srcrowbytes,
	size_t destrowbytes);

#endif
