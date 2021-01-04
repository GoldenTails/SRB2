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
/// \file  r_things.c
/// \brief Refresh of things, i.e. objects represented by sprites

#include "doomdef.h"
#include "console.h"
#include "g_game.h"
#include "r_local.h"
#include "st_stuff.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_menu.h" // character select
#include "m_misc.h"
#include "info.h" // spr2names
#include "i_video.h" // rendermode
#include "i_system.h"
#include "r_things.h"
#include "r_patch.h"
#include "r_patchrotation.h"
#include "r_picformats.h"
#include "software/sw_plane.h"
#include "r_portal.h"
#include "software/sw_splats.h"
#include "p_tick.h"
#include "p_local.h"
#include "p_slopes.h"
#include "d_netfil.h" // blargh. for nameonly().
#include "m_cheat.h" // objectplace
#ifdef HWRENDER
#include "hardware/hw_md2.h"
#include "hardware/hw_glob.h"
#include "hardware/hw_light.h"
#include "hardware/hw_drv.h"
#endif
#include "software/sw_things.h"

//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//
spriteinfo_t spriteinfo[NUMSPRITES];

//
// INITIALIZATION FUNCTIONS
//

// variables used to look up and range check thing_t sprites patches
spritedef_t *sprites;
size_t numsprites;

static spriteframe_t sprtemp[64];
static size_t maxframe;
static const char *spritename;

// ==========================================================================
//
// Sprite loading routines: support sprites in pwad, dehacked sprite renaming,
// replacing not all frames of an existing sprite, add sprites at run-time,
// add wads at run-time.
//
// ==========================================================================

//
//
//
static void R_InstallSpriteLump(UINT16 wad,            // graphics patch
                                UINT16 lump,
                                size_t lumpid,      // identifier
                                UINT8 frame,
                                UINT8 rotation,
                                UINT8 flipped)
{
	char cn = R_Frame2Char(frame), cr = R_Rotation2Char(rotation); // for debugging

	INT32 r;
	lumpnum_t lumppat = wad;
	lumppat <<= 16;
	lumppat += lump;

	if (maxframe ==(size_t)-1 || frame > maxframe)
		maxframe = frame;

#ifdef ROTSPRITE
	for (r = 0; r < 16; r++)
	{
		sprtemp[frame].rotated[0][r] = NULL;
		sprtemp[frame].rotated[1][r] = NULL;
	}
#endif

	if (rotation == 0)
	{
		// the lump should be used for all rotations
		if (sprtemp[frame].rotate == SRF_SINGLE)
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has multiple rot = 0 lump\n", spritename, cn);
		else if (sprtemp[frame].rotate != SRF_NONE) // Let's bundle 1-8/16 and L/R rotations into one debug message.
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has rotations and a rot = 0 lump\n", spritename, cn);

		sprtemp[frame].rotate = SRF_SINGLE;
		for (r = 0; r < 16; r++)
		{
			sprtemp[frame].lumppat[r] = lumppat;
			sprtemp[frame].lumpid[r] = lumpid;
		}
		sprtemp[frame].flip = flipped ? 0xFFFF : 0; // 1111111111111111 in binary
		return;
	}

	if (rotation == ROT_L || rotation == ROT_R)
	{
		UINT8 rightfactor = ((rotation == ROT_R) ? 4 : 0);

		// the lump should be used for half of all rotations
		if (sprtemp[frame].rotate == SRF_NONE)
			sprtemp[frame].rotate = SRF_SINGLE;
		else if (sprtemp[frame].rotate == SRF_SINGLE)
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has L/R rotations and a rot = 0 lump\n", spritename, cn);
		else if (sprtemp[frame].rotate == SRF_3D)
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has both L/R and 1-8 rotations\n", spritename, cn);
		else if (sprtemp[frame].rotate == SRF_3DGE)
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has both L/R and 1-G rotations\n", spritename, cn);
		else if ((sprtemp[frame].rotate & SRF_LEFT) && (rotation == ROT_L))
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has multiple L rotations\n", spritename, cn);
		else if ((sprtemp[frame].rotate & SRF_RIGHT) && (rotation == ROT_R))
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has multiple R rotations\n", spritename, cn);

		sprtemp[frame].rotate |= ((rotation == ROT_R) ? SRF_RIGHT : SRF_LEFT);
		if ((sprtemp[frame].rotate & SRF_2D) == SRF_2D)
			sprtemp[frame].rotate &= ~SRF_3DMASK; // SRF_3D|SRF_2D being enabled at the same time doesn't HURT in the current sprite angle implementation, but it DOES mean more to check in some of the helper functions. Let's not allow this scenario to happen.

		// load into every relevant angle, including the front one
		for (r = 0; r < 4; r++)
		{
			sprtemp[frame].lumppat[r + rightfactor] = lumppat;
			sprtemp[frame].lumpid[r + rightfactor] = lumpid;
			sprtemp[frame].lumppat[r + rightfactor + 8] = lumppat;
			sprtemp[frame].lumpid[r + rightfactor + 8] = lumpid;

		}

		if (flipped)
			sprtemp[frame].flip |= (0x0F0F<<rightfactor); // 0000111100001111 or 1111000011110000 in binary, depending on rotation being ROT_L or ROT_R
		else
			sprtemp[frame].flip &= ~(0x0F0F<<rightfactor); // ditto

		return;
	}

	if (sprtemp[frame].rotate == SRF_NONE)
		sprtemp[frame].rotate = SRF_SINGLE;
	else if (sprtemp[frame].rotate == SRF_SINGLE)
		CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has 1-8/G rotations and a rot = 0 lump\n", spritename, cn);
	else if (sprtemp[frame].rotate & SRF_2D)
		CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has both L/R and 1-8/G rotations\n", spritename, cn);

	// make 0 based
	rotation--;

	{
		// SRF_3D|SRF_3DGE being enabled at the same time doesn't HURT in the current sprite angle implementation, but it DOES mean more to check in some of the helper functions. Let's not allow this scenario to happen.
		UINT8 threedrot = (rotation > 7) ? SRF_3DGE : (sprtemp[frame].rotate & SRF_3DMASK);
		if (!threedrot)
			threedrot = SRF_3D;

		if (rotation == 0 || rotation == 4) // Front or back...
			sprtemp[frame].rotate = threedrot; // Prevent L and R changeover
		else if ((rotation & 7) > 3) // Right side
			sprtemp[frame].rotate = (threedrot | (sprtemp[frame].rotate & SRF_LEFT)); // Continue allowing L frame changeover
		else // if ((rotation & 7) <= 3) // Left side
			sprtemp[frame].rotate = (threedrot | (sprtemp[frame].rotate & SRF_RIGHT)); // Continue allowing R frame changeover
	}

	if (sprtemp[frame].lumppat[rotation] != LUMPERROR)
		CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s: %c%c has two lumps mapped to it\n", spritename, cn, cr);

	// lumppat & lumpid are the same for original Doom, but different
	// when using sprites in pwad : the lumppat points the new graphics
	sprtemp[frame].lumppat[rotation] = lumppat;
	sprtemp[frame].lumpid[rotation] = lumpid;
	if (flipped)
		sprtemp[frame].flip |= (1<<rotation);
	else
		sprtemp[frame].flip &= ~(1<<rotation);
}

// Install a single sprite, given its identifying name (4 chars)
//
// (originally part of R_AddSpriteDefs)
//
// Pass: name of sprite : 4 chars
//       spritedef_t
//       wadnum         : wad number, indexes wadfiles[], where patches
//                        for frames are found
//       startlump      : first lump to search for sprite frames
//       endlump        : AFTER the last lump to search
//
// Returns true if the sprite was succesfully added
//
boolean R_AddSingleSpriteDef(const char *sprname, spritedef_t *spritedef, UINT16 wadnum, UINT16 startlump, UINT16 endlump)
{
	UINT16 l;
	UINT8 frame;
	UINT8 rotation;
	lumpinfo_t *lumpinfo;
	softwarepatch_t patch;
	UINT8 numadded = 0;

	memset(sprtemp,0xFF, sizeof (sprtemp));
	maxframe = (size_t)-1;

	spritename = sprname;

	// are we 'patching' a sprite already loaded ?
	// if so, it might patch only certain frames, not all
	if (spritedef->numframes) // (then spriteframes is not null)
	{
		// copy the already defined sprite frames
		M_Memcpy(sprtemp, spritedef->spriteframes,
		 spritedef->numframes * sizeof (spriteframe_t));
		maxframe = spritedef->numframes - 1;
	}

	// scan the lumps,
	//  filling in the frames for whatever is found
	lumpinfo = wadfiles[wadnum]->lumpinfo;
	if (endlump > wadfiles[wadnum]->numlumps)
		endlump = wadfiles[wadnum]->numlumps;

	for (l = startlump; l < endlump; l++)
	{
		if (memcmp(lumpinfo[l].name,sprname,4)==0)
		{
			INT32 width, height;
			INT16 topoffset, leftoffset;
#ifndef NO_PNG_LUMPS
			boolean isPNG = false;
#endif

			frame = R_Char2Frame(lumpinfo[l].name[4]);
			rotation = R_Char2Rotation(lumpinfo[l].name[5]);

			if (frame >= 64 || rotation == 255) // Give an actual NAME error -_-...
			{
				CONS_Alert(CONS_WARNING, M_GetText("Bad sprite name: %s\n"), W_CheckNameForNumPwad(wadnum,l));
				continue;
			}

			// skip NULL sprites from very old dmadds pwads
			if (W_LumpLengthPwad(wadnum,l)<=8)
				continue;

			// store sprite info in lookup tables
			//FIXME : numspritelumps do not duplicate sprite replacements

#ifndef NO_PNG_LUMPS
			{
				softwarepatch_t *png = W_CacheLumpNumPwad(wadnum, l, PU_STATIC);
				size_t len = W_LumpLengthPwad(wadnum, l);

				if (Picture_IsLumpPNG((UINT8 *)png, len))
				{
					Picture_PNGDimensions((UINT8 *)png, &width, &height, &topoffset, &leftoffset, len);
					isPNG = true;
				}

				Z_Free(png);
			}

			if (!isPNG)
#endif
			{
				W_ReadLumpHeaderPwad(wadnum, l, &patch, sizeof(INT16) * 4, 0);
				width = (INT32)(SHORT(patch.width));
				height = (INT32)(SHORT(patch.height));
				topoffset = (INT16)(SHORT(patch.topoffset));
				leftoffset = (INT16)(SHORT(patch.leftoffset));
			}

			spritecachedinfo[numspritelumps].width = width<<FRACBITS;
			spritecachedinfo[numspritelumps].offset = leftoffset<<FRACBITS;
			spritecachedinfo[numspritelumps].topoffset = topoffset<<FRACBITS;
			spritecachedinfo[numspritelumps].height = height<<FRACBITS;

			// BP: we cannot use special tric in hardware mode because feet in ground caused by z-buffer
			spritecachedinfo[numspritelumps].topoffset += FEETADJUST;

			//----------------------------------------------------

			R_InstallSpriteLump(wadnum, l, numspritelumps, frame, rotation, 0);

			if (lumpinfo[l].name[6])
			{
				frame = R_Char2Frame(lumpinfo[l].name[6]);
				rotation = R_Char2Rotation(lumpinfo[l].name[7]);

				if (frame >= 64 || rotation == 255) // Give an actual NAME error -_-...
				{
					CONS_Alert(CONS_WARNING, M_GetText("Bad sprite name: %s\n"), W_CheckNameForNumPwad(wadnum,l));
					continue;
				}
				R_InstallSpriteLump(wadnum, l, numspritelumps, frame, rotation, 1);
			}

			if (++numspritelumps >= max_spritelumps)
			{
				max_spritelumps *= 2;
				Z_Realloc(spritecachedinfo, max_spritelumps*sizeof(*spritecachedinfo), PU_STATIC, &spritecachedinfo);
			}

			++numadded;
		}
	}

	//
	// if no frames found for this sprite
	//
	if (maxframe == (size_t)-1)
	{
		// the first time (which is for the original wad),
		// all sprites should have their initial frames
		// and then, patch wads can replace it
		// we will skip non-replaced sprite frames, only if
		// they have already have been initially defined (original wad)

		//check only after all initial pwads added
		//if (spritedef->numframes == 0)
		//    I_Error("R_AddSpriteDefs: no initial frames found for sprite %s\n",
		//             namelist[i]);

		// sprite already has frames, and is not replaced by this wad
		return false;
	}
	else if (!numadded)
	{
		// Nothing related to this spritedef has been changed
		// so there is no point going back through these checks again.
		return false;
	}

	maxframe++;

	//
	//  some checks to help development
	//
	for (frame = 0; frame < maxframe; frame++)
	{
		switch (sprtemp[frame].rotate)
		{
			case SRF_NONE:
			// no rotations were found for that frame at all
			I_Error("R_AddSingleSpriteDef: No patches found for %.4s frame %c", sprname, R_Frame2Char(frame));
			break;

			case SRF_SINGLE:
			// only the first rotation is needed
			break;

			case SRF_2D: // both Left and Right rotations
			// we test to see whether the left and right slots are present
			if ((sprtemp[frame].lumppat[2] == LUMPERROR) || (sprtemp[frame].lumppat[6] == LUMPERROR))
				I_Error("R_AddSingleSpriteDef: Sprite %.4s frame %c is missing rotations (L-R mode)",
				sprname, R_Frame2Char(frame));
			break;

			default:
			// must have all 8/16 frames
				rotation = ((sprtemp[frame].rotate & SRF_3DGE) ? 16 : 8);
				while (rotation--)
				// we test the patch lump, or the id lump whatever
				// if it was not loaded the two are LUMPERROR
				if (sprtemp[frame].lumppat[rotation] == LUMPERROR)
					I_Error("R_AddSingleSpriteDef: Sprite %.4s frame %c is missing rotations (1-%c mode)",
					        sprname, R_Frame2Char(frame), ((sprtemp[frame].rotate & SRF_3DGE) ? 'G' : '8'));
			break;
		}
	}

	// allocate space for the frames present and copy sprtemp to it
	if (spritedef->numframes &&             // has been allocated
		spritedef->numframes < maxframe)   // more frames are defined ?
	{
		Z_Free(spritedef->spriteframes);
		spritedef->spriteframes = NULL;
	}

	// allocate this sprite's frames
	if (!spritedef->spriteframes)
		spritedef->spriteframes =
		 Z_Malloc(maxframe * sizeof (*spritedef->spriteframes), PU_STATIC, NULL);

	spritedef->numframes = maxframe;
	M_Memcpy(spritedef->spriteframes, sprtemp, maxframe*sizeof (spriteframe_t));

	return true;
}

//
// Search for sprites replacements in a wad whose names are in namelist
//
void R_AddSpriteDefs(UINT16 wadnum)
{
	size_t i, addsprites = 0;
	UINT16 start, end;
	char wadname[MAX_WADPATH];

	// Find the sprites section in this resource file.
	switch (wadfiles[wadnum]->type)
	{
	case RET_WAD:
		start = W_CheckNumForMarkerStartPwad("S_START", wadnum, 0);
		if (start == INT16_MAX)
			start = W_CheckNumForMarkerStartPwad("SS_START", wadnum, 0); //deutex compatib.

		end = W_CheckNumForNamePwad("S_END",wadnum,start);
		if (end == INT16_MAX)
			end = W_CheckNumForNamePwad("SS_END",wadnum,start);     //deutex compatib.
		break;
	case RET_PK3:
		start = W_CheckNumForFolderStartPK3("Sprites/", wadnum, 0);
		end = W_CheckNumForFolderEndPK3("Sprites/", wadnum, start);
		break;
	default:
		return;
	}

	if (start == INT16_MAX)
	{
		// ignore skin wads (we don't want skin sprites interfering with vanilla sprites)
		if (W_CheckNumForNamePwad("S_SKIN", wadnum, 0) != UINT16_MAX)
			return;

		start = 0; //let say S_START is lump 0
	}

	if (end == INT16_MAX || start >= end)
	{
		CONS_Debug(DBG_SETUP, "no sprites in pwad %d\n", wadnum);
		return;
	}


	//
	// scan through lumps, for each sprite, find all the sprite frames
	//
	for (i = 0; i < numsprites; i++)
	{
		if (sprnames[i][4] && wadnum >= (UINT16)sprnames[i][4])
			continue;

		if (R_AddSingleSpriteDef(sprnames[i], &sprites[i], wadnum, start, end))
		{
#ifdef HWRENDER
			if (rendermode == render_opengl)
				HWR_AddSpriteModel(i);
#endif
			// if a new sprite was added (not just replaced)
			addsprites++;
#ifndef ZDEBUG
			CONS_Debug(DBG_SETUP, "sprite %s set in pwad %d\n", sprnames[i], wadnum);
#endif
		}
	}

	nameonly(strcpy(wadname, wadfiles[wadnum]->filename));
	CONS_Printf(M_GetText("%s added %d frames in %s sprites\n"), wadname, end-start, sizeu1(addsprites));
}

//
// R_InitSprites
// Called at program start.
//
void R_InitSprites(void)
{
	size_t i;

	SWR_InitSprites();

	//
	// count the number of sprite names, and allocate sprites table
	//
	numsprites = 0;
	for (i = 0; i < NUMSPRITES + 1; i++)
		if (sprnames[i][0] != '\0') numsprites++;

	if (!numsprites)
		I_Error("R_AddSpriteDefs: no sprites in namelist\n");

	sprites = Z_Calloc(numsprites * sizeof (*sprites), PU_STATIC, NULL);

	// find sprites in each -file added pwad
	for (i = 0; i < numwadfiles; i++)
		R_AddSpriteDefs((UINT16)i);

	//
	// now check for skins
	//

	// it can be is do before loading config for skin cvar possible value
	R_InitSkins();
	for (i = 0; i < numwadfiles; i++)
	{
		R_AddSkins((UINT16)i);
		R_PatchSkins((UINT16)i);
		R_LoadSpriteInfoLumps(i, wadfiles[i]->numlumps);
	}
	ST_ReloadSkinFaceGraphics();

	//
	// check if all sprites have frames
	//
	/*
	for (i = 0; i < numsprites; i++)
		if (sprites[i].numframes < 1)
			CONS_Debug(DBG_SETUP, "SWR_InitSprites: sprite %s has no frames at all\n", sprnames[i]);
	*/
}

//
// R_GetShadowZ(thing, shadowslope)
// Get the first visible floor below the object for shadows
// shadowslope is filled with the floor's slope, if provided
//
fixed_t R_GetShadowZ(mobj_t *thing, pslope_t **shadowslope)
{
	boolean isflipped = thing->eflags & MFE_VERTICALFLIP;
	fixed_t z, groundz = isflipped ? INT32_MAX : INT32_MIN;
	pslope_t *slope, *groundslope = NULL;
	msecnode_t *node;
	sector_t *sector;
	ffloor_t *rover;
#define CHECKZ (isflipped ? z > thing->z+thing->height/2 && z < groundz : z < thing->z+thing->height/2 && z > groundz)

	for (node = thing->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		sector = node->m_sector;

		slope = sector->heightsec != -1 ? NULL : (isflipped ? sector->c_slope : sector->f_slope);

		if (sector->heightsec != -1)
			z = isflipped ? sectors[sector->heightsec].ceilingheight : sectors[sector->heightsec].floorheight;
		else
			z = isflipped ? P_GetSectorCeilingZAt(sector, thing->x, thing->y) : P_GetSectorFloorZAt(sector, thing->x, thing->y);

		if CHECKZ
		{
			groundz = z;
			groundslope = slope;
		}

		if (sector->ffloors)
			for (rover = sector->ffloors; rover; rover = rover->next)
			{
				if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES) || (rover->alpha < 90 && !(rover->flags & FF_SWIMMABLE)))
					continue;

				z = isflipped ? P_GetFFloorBottomZAt(rover, thing->x, thing->y) : P_GetFFloorTopZAt(rover, thing->x, thing->y);
				if CHECKZ
				{
					groundz = z;
					groundslope = isflipped ? *rover->b_slope : *rover->t_slope;
				}
			}
	}

	if (isflipped ? (thing->ceilingz < groundz - (!groundslope ? 0 : FixedMul(abs(groundslope->zdelta), thing->radius*3/2)))
		: (thing->floorz > groundz + (!groundslope ? 0 : FixedMul(abs(groundslope->zdelta), thing->radius*3/2))))
	{
		groundz = isflipped ? thing->ceilingz : thing->floorz;
		groundslope = NULL;
	}

#if 0 // Unfortunately, this drops CEZ2 down to sub-17 FPS on my i7.
	// NOTE: this section was not updated to reflect reverse gravity support
	// Check polyobjects and see if groundz needs to be altered, for rings only because they don't update floorz
	if (thing->type == MT_RING)
	{
		INT32 xl, xh, yl, yh, bx, by;

		xl = (unsigned)(thing->x - thing->radius - bmaporgx)>>MAPBLOCKSHIFT;
		xh = (unsigned)(thing->x + thing->radius - bmaporgx)>>MAPBLOCKSHIFT;
		yl = (unsigned)(thing->y - thing->radius - bmaporgy)>>MAPBLOCKSHIFT;
		yh = (unsigned)(thing->y + thing->radius - bmaporgy)>>MAPBLOCKSHIFT;

		BMBOUNDFIX(xl, xh, yl, yh);

		validcount++;

		for (by = yl; by <= yh; by++)
			for (bx = xl; bx <= xh; bx++)
			{
				INT32 offset;
				polymaplink_t *plink; // haleyjd 02/22/06

				if (bx < 0 || by < 0 || bx >= bmapwidth || by >= bmapheight)
					continue;

				offset = by*bmapwidth + bx;

				// haleyjd 02/22/06: consider polyobject lines
				plink = polyblocklinks[offset];

				while (plink)
				{
					polyobj_t *po = plink->po;

					if (po->validcount != validcount) // if polyobj hasn't been checked
					{
						po->validcount = validcount;

						if (!P_MobjInsidePolyobj(po, thing) || !(po->flags & POF_RENDERPLANES))
						{
							plink = (polymaplink_t *)(plink->link.next);
							continue;
						}

						// We're inside it! Yess...
						z = po->lines[0]->backsector->ceilingheight;

						if (z < thing->z+thing->height/2 && z > groundz)
						{
							groundz = z;
							groundslope = NULL;
						}
					}
					plink = (polymaplink_t *)(plink->link.next);
				}
			}
	}
#endif

	if (shadowslope != NULL)
		*shadowslope = groundslope;

	return groundz;
#undef CHECKZ
}

/* Check if thing may be drawn from our current view. */
boolean R_ThingVisible (mobj_t *thing)
{
	return (!(
				thing->sprite == SPR_NULL ||
				( thing->flags2 & (MF2_DONTDRAW) ) ||
				(r_viewmobj && (thing == r_viewmobj || (r_viewmobj->player && r_viewmobj->player->followmobj == thing)))
	));
}

boolean R_ThingVisibleWithinDist (mobj_t *thing,
		fixed_t      limit_dist,
		fixed_t hoop_limit_dist)
{
	fixed_t approx_dist;

	if (! R_ThingVisible(thing))
		return false;

	approx_dist = P_AproxDistance(viewx-thing->x, viewy-thing->y);

	if (thing->sprite == SPR_HOOP)
	{
		if (hoop_limit_dist && approx_dist > hoop_limit_dist)
			return false;
	}
	else
	{
		if (limit_dist && approx_dist > limit_dist)
			return false;
	}

	return true;
}

/* Check if precipitation may be drawn from our current view. */
boolean R_PrecipThingVisible (precipmobj_t *precipthing,
		fixed_t limit_dist)
{
	fixed_t approx_dist;

	if (( precipthing->precipflags & PCF_INVISIBLE ))
		return false;

	approx_dist = P_AproxDistance(viewx-precipthing->x, viewy-precipthing->y);

	return ( approx_dist <= limit_dist );
}

boolean R_ThingHorizontallyFlipped(mobj_t *thing)
{
	return (thing->frame & FF_HORIZONTALFLIP || thing->renderflags & RF_HORIZONTALFLIP);
}

boolean R_ThingVerticallyFlipped(mobj_t *thing)
{
	return (thing->frame & FF_VERTICALFLIP || thing->renderflags & RF_VERTICALFLIP);
}

boolean R_ThingIsPaperSprite(mobj_t *thing)
{
	return (thing->frame & FF_PAPERSPRITE || thing->renderflags & RF_PAPERSPRITE);
}

boolean R_ThingIsFloorSprite(mobj_t *thing)
{
	return (thing->flags2 & MF2_SPLAT || thing->renderflags & RF_FLOORSPRITE);
}

boolean R_ThingIsFullBright(mobj_t *thing)
{
	return (thing->frame & FF_FULLBRIGHT || thing->renderflags & RF_FULLBRIGHT);
}

boolean R_ThingIsFullDark(mobj_t *thing)
{
	return (thing->renderflags & RF_FULLDARK);
}
