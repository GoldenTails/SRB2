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
/// \file  r_things.h
/// \brief Rendering of moving objects, sprites

#ifndef __R_THINGS__
#define __R_THINGS__

#include "software/sw_plane.h"
#include "software/sw_things.h"
#include "r_patch.h"
#include "r_picformats.h"
#include "r_portal.h"
#include "r_defs.h"
#include "r_skins.h"

// --------------
// SPRITE LOADING
// --------------

#define FEETADJUST (4<<FRACBITS) // R_AddSingleSpriteDef

boolean R_AddSingleSpriteDef(const char *sprname, spritedef_t *spritedef, UINT16 wadnum, UINT16 startlump, UINT16 endlump);

//faB: find sprites in wadfile, replace existing, add new ones
//     (only sprites from namelist are added or replaced)
void R_AddSpriteDefs(UINT16 wadnum);

// ----------------
// SPRITE RENDERING
// ----------------

fixed_t R_GetShadowZ(mobj_t *thing, pslope_t **shadowslope);

//SoM: 6/5/2000: Light sprites correctly!
void R_InitSprites(void);

boolean R_ThingVisible (mobj_t *thing);

boolean R_ThingVisibleWithinDist (mobj_t *thing,
		fixed_t        draw_dist,
		fixed_t nights_draw_dist);

boolean R_PrecipThingVisible (precipmobj_t *precipthing,
		fixed_t precip_draw_dist);

boolean R_ThingHorizontallyFlipped (mobj_t *thing);
boolean R_ThingVerticallyFlipped (mobj_t *thing);

boolean R_ThingIsPaperSprite (mobj_t *thing);
boolean R_ThingIsFloorSprite (mobj_t *thing);

boolean R_ThingIsFullBright (mobj_t *thing);
boolean R_ThingIsFullDark (mobj_t *thing);

// -----------------------
// SPRITE FRAME CHARACTERS
// -----------------------

// Functions to go from sprite character ID to frame number
// for 2.1 compatibility this still uses the old 'A' + frame code
// The use of symbols tends to be painful for wad editors though
// So the future version of this tries to avoid using symbols
// as much as possible while also defining all 64 slots in a sane manner
// 2.1:    [[ ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~   ]]
// Future: [[ ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz!@ ]]
FUNCMATH FUNCINLINE static ATTRINLINE char R_Frame2Char(UINT8 frame)
{
#if 0 // 2.1 compat
	return 'A' + frame;
#else
	if (frame < 26) return 'A' + frame;
	if (frame < 36) return '0' + (frame - 26);
	if (frame < 62) return 'a' + (frame - 36);
	if (frame == 62) return '!';
	if (frame == 63) return '@';
	return '\xFF';
#endif
}

FUNCMATH FUNCINLINE static ATTRINLINE UINT8 R_Char2Frame(char cn)
{
#if 0 // 2.1 compat
	if (cn == '+') return '\\' - 'A'; // PK3 can't use backslash, so use + instead
	return cn - 'A';
#else
	if (cn >= 'A' && cn <= 'Z') return (cn - 'A');
	if (cn >= '0' && cn <= '9') return (cn - '0') + 26;
	if (cn >= 'a' && cn <= 'z') return (cn - 'a') + 36;
	if (cn == '!') return 62;
	if (cn == '@') return 63;
	return 255;
#endif
}

// "Left" and "Right" character symbols for additional rotation functionality
#define ROT_L 17
#define ROT_R 18

FUNCMATH FUNCINLINE static ATTRINLINE char R_Rotation2Char(UINT8 rot)
{
	if (rot <=     9) return '0' + rot;
	if (rot <=    16) return 'A' + (rot - 10);
	if (rot == ROT_L) return 'L';
	if (rot == ROT_R) return 'R';
	return '\xFF';
}

FUNCMATH FUNCINLINE static ATTRINLINE UINT8 R_Char2Rotation(char cn)
{
	if (cn >= '0' && cn <= '9') return (cn - '0');
	if (cn >= 'A' && cn <= 'G') return (cn - 'A') + 10;
	if (cn == 'L') return ROT_L;
	if (cn == 'R') return ROT_R;
	return 255;
}

#endif //__R_THINGS__
