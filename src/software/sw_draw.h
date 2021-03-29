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
/// \file  sw_draw.h
/// \brief Low-level span/column drawer functions

#ifndef __SW_DRAW__
#define __SW_DRAW__

#include "../r_defs.h"

// -------------------------------
// COMMON STUFF FOR 8bpp AND 16bpp
// -------------------------------
extern UINT8 *ylookup[MAXVIDHEIGHT*4];
extern UINT8 *ylookup1[MAXVIDHEIGHT*4];
extern UINT8 *ylookup2[MAXVIDHEIGHT*4];
extern INT32 columnofs[MAXVIDWIDTH*4];
extern UINT8 *topleft;

// -------------------------
// COLUMN DRAWING CODE STUFF
// -------------------------

extern lighttable_t *dc_colormap;
extern INT32 dc_x, dc_yl, dc_yh;
extern fixed_t dc_iscale, dc_texturemid;
extern UINT8 dc_hires;

extern UINT8 *dc_source; // first pixel in a column

// translucency stuff here
extern UINT8 *dc_transmap;

// translation stuff here

extern UINT8 *dc_translation;

extern struct r_lightlist_s *dc_lightlist;
extern INT32 dc_numlights, dc_maxlights;

//Fix TUTIFRUTI
extern INT32 dc_texheight;

// -----------------------
// SPAN DRAWING CODE STUFF
// -----------------------

extern INT32 ds_y, ds_x1, ds_x2;
extern lighttable_t *ds_colormap;
extern lighttable_t *ds_translation;

extern fixed_t ds_xfrac, ds_yfrac, ds_xstep, ds_ystep;
extern INT32 ds_waterofs, ds_bgofs;

extern UINT16 ds_flatwidth, ds_flatheight;
extern boolean ds_powersoftwo;

extern UINT8 *ds_source;
extern UINT8 *ds_transmap;

typedef struct {
	float x, y, z;
} floatv3_t;

// Vectors for Software's tilted slope drawers
extern floatv3_t *ds_su, *ds_sv, *ds_sz;
extern floatv3_t *ds_sup, *ds_svp, *ds_szp;
extern float focallengthf, zeroheight;

// Variable flat sizes
extern UINT32 nflatxshift;
extern UINT32 nflatyshift;
extern UINT32 nflatshiftup;
extern UINT32 nflatmask;

// ------------------------------------------------
// r_draw.c COMMON ROUTINES FOR BOTH 8bpp and 16bpp
// ------------------------------------------------

void SWR_InitViewBuffer(INT32 width, INT32 height);
void SWR_VideoErase(size_t ofs, INT32 count);

// Rendering function.
#if 0
void SWR_FillBackScreen(void);

// If the view size is not full screen, draws a border around it.
void SWR_DrawViewBorder(void);
#endif

// -----------------
// 8bpp DRAWING CODE
// -----------------

void SWR_DrawColumn_8(void);
void SWR_DrawShadeColumn_8(void);
void SWR_DrawTranslucentColumn_8(void);
void SWR_DrawTranslatedColumn_8(void);
void SWR_DrawTranslatedTranslucentColumn_8(void);
void SWR_Draw2sMultiPatchColumn_8(void);
void SWR_Draw2sMultiPatchTranslucentColumn_8(void);
void SWR_DrawFogColumn_8(void);
void SWR_DrawColumnShadowed_8(void);

#define PLANELIGHTFLOAT (BASEVIDWIDTH * BASEVIDWIDTH / vid.width / (zeroheight - FIXED_TO_FLOAT(viewz)) / 21.0f * FIXED_TO_FLOAT(fovtan))

void SWR_DrawSpan_8(void);
void SWR_DrawTranslucentSpan_8(void);
void SWR_DrawTiltedSpan_8(void);
void SWR_DrawTiltedTranslucentSpan_8(void);

void SWR_DrawSplat_8(void);
void SWR_DrawTranslucentSplat_8(void);
void SWR_DrawTiltedSplat_8(void);

void SWR_DrawFloorSprite_8(void);
void SWR_DrawTranslucentFloorSprite_8(void);
void SWR_DrawTiltedFloorSprite_8(void);
void SWR_DrawTiltedTranslucentFloorSprite_8(void);

void SWR_CalcTiltedLighting(fixed_t start, fixed_t end);
extern INT32 tiltlighting[MAXVIDWIDTH];

void SWR_DrawTranslucentWaterSpan_8(void);
void SWR_DrawTiltedTranslucentWaterSpan_8(void);

void SWR_DrawFogSpan_8(void);

// Lactozilla: Non-powers-of-two
void SWR_DrawSpan_NPO2_8(void);
void SWR_DrawTranslucentSpan_NPO2_8(void);
void SWR_DrawTiltedSpan_NPO2_8(void);
void SWR_DrawTiltedTranslucentSpan_NPO2_8(void);

void SWR_DrawSplat_NPO2_8(void);
void SWR_DrawTranslucentSplat_NPO2_8(void);
void SWR_DrawTiltedSplat_NPO2_8(void);

void SWR_DrawFloorSprite_NPO2_8(void);
void SWR_DrawTranslucentFloorSprite_NPO2_8(void);
void SWR_DrawTiltedFloorSprite_NPO2_8(void);
void SWR_DrawTiltedTranslucentFloorSprite_NPO2_8(void);

void SWR_DrawTranslucentWaterSpan_NPO2_8(void);
void SWR_DrawTiltedTranslucentWaterSpan_NPO2_8(void);

#ifdef USEASM
void ASMCALL SWR_DrawColumn_8_ASM(void);
void ASMCALL SWR_DrawShadeColumn_8_ASM(void);
void ASMCALL SWR_DrawTranslucentColumn_8_ASM(void);
void ASMCALL SWR_Draw2sMultiPatchColumn_8_ASM(void);

void ASMCALL SWR_DrawColumn_8_MMX(void);

void ASMCALL SWR_Draw2sMultiPatchColumn_8_MMX(void);
void ASMCALL SWR_DrawSpan_8_MMX(void);
#endif

// ------------------
// 16bpp DRAWING CODE
// ------------------

#ifdef HIGHCOLOR
void SWR_DrawColumn_16(void);
void SWR_DrawWallColumn_16(void);
void SWR_DrawTranslucentColumn_16(void);
void SWR_DrawTranslatedColumn_16(void);
void SWR_DrawSpan_16(void);
#endif

// =========================================================================
#endif  // __SW_DRAW__
