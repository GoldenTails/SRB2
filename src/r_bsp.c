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
/// \file  r_bsp.c
/// \brief BSP traversal, handling of LineSegs for rendering

#include "doomdef.h"
#include "g_game.h"
#include "r_local.h"
#include "r_state.h"
#include "r_portal.h" // Add seg portals

#include "p_local.h" // camera
#include "software/sw_things.h"
#include "p_slopes.h"
#include "z_zone.h" // Check R_Prep3DFloors
#include "taglist.h"

seg_t *curline;
side_t *sidedef;
line_t *linedef;
sector_t *frontsector;
sector_t *backsector;

// Fix from boom.
#define MAXSEGS (MAXVIDWIDTH/2+1)

// newend is one past the last valid seg
static cliprange_t *newend;
static cliprange_t solidsegs[MAXSEGS];

//
// If player's view height is underneath fake floor, lower the
// drawn ceiling to be just under the floor height, and replace
// the drawn floor and ceiling textures, and light level, with
// the control sector's.
//
// Similar for ceiling, only reflected.
//
sector_t *R_FakeFlat(sector_t *sec, sector_t *tempsec, INT32 *floorlightlevel,
	INT32 *ceilinglightlevel, boolean back)
{
	if (floorlightlevel)
		*floorlightlevel = sec->floorlightsec == -1 ?
			sec->lightlevel : sectors[sec->floorlightsec].lightlevel;

	if (ceilinglightlevel)
		*ceilinglightlevel = sec->ceilinglightsec == -1 ?
			sec->lightlevel : sectors[sec->ceilinglightsec].lightlevel;

	// if (sec->midmap != -1)
	//	mapnum = sec->midmap;
	// In original colormap code, this block did not run if sec->midmap was set
	if (!sec->extra_colormap && sec->heightsec != -1)
	{
		const sector_t *s = &sectors[sec->heightsec];
		mobj_t *viewmobj = viewplayer->mo;
		INT32 heightsec;
		boolean underwater;

		if (splitscreen && viewplayer == &players[secondarydisplayplayer] && camera2.chase)
			heightsec = R_PointInSubsector(camera2.x, camera2.y)->sector->heightsec;
		else if (camera.chase && viewplayer == &players[displayplayer])
			heightsec = R_PointInSubsector(camera.x, camera.y)->sector->heightsec;
		else if (viewmobj)
			heightsec = R_PointInSubsector(viewmobj->x, viewmobj->y)->sector->heightsec;
		else
			return sec;
		underwater = heightsec != -1 && viewz <= sectors[heightsec].floorheight;

		// Replace sector being drawn, with a copy to be hacked
		*tempsec = *sec;

		// Replace floor and ceiling height with other sector's heights.
		tempsec->floorheight = s->floorheight;
		tempsec->ceilingheight = s->ceilingheight;

		if ((underwater && (tempsec->  floorheight = sec->floorheight,
			tempsec->ceilingheight = s->floorheight - 1, !back)) || viewz <= s->floorheight)
		{ // head-below-floor hack
			tempsec->floorpic = s->floorpic;
			tempsec->floor_xoffs = s->floor_xoffs;
			tempsec->floor_yoffs = s->floor_yoffs;
			tempsec->floorpic_angle = s->floorpic_angle;

			if (underwater)
			{
				if (s->ceilingpic == skyflatnum)
				{
					tempsec->floorheight = tempsec->ceilingheight+1;
					tempsec->ceilingpic = tempsec->floorpic;
					tempsec->ceiling_xoffs = tempsec->floor_xoffs;
					tempsec->ceiling_yoffs = tempsec->floor_yoffs;
					tempsec->ceilingpic_angle = tempsec->floorpic_angle;
				}
				else
				{
					tempsec->ceilingpic = s->ceilingpic;
					tempsec->ceiling_xoffs = s->ceiling_xoffs;
					tempsec->ceiling_yoffs = s->ceiling_yoffs;
					tempsec->ceilingpic_angle = s->ceilingpic_angle;
				}
			}

			tempsec->lightlevel = s->lightlevel;

			if (floorlightlevel)
				*floorlightlevel = s->floorlightsec == -1 ? s->lightlevel
					: sectors[s->floorlightsec].lightlevel;

			if (ceilinglightlevel)
				*ceilinglightlevel = s->ceilinglightsec == -1 ? s->lightlevel
					: sectors[s->ceilinglightsec].lightlevel;
		}
		else if (heightsec != -1 && viewz >= sectors[heightsec].ceilingheight
			&& sec->ceilingheight > s->ceilingheight)
		{ // Above-ceiling hack
			tempsec->ceilingheight = s->ceilingheight;
			tempsec->floorheight = s->ceilingheight + 1;

			tempsec->floorpic = tempsec->ceilingpic = s->ceilingpic;
			tempsec->floor_xoffs = tempsec->ceiling_xoffs = s->ceiling_xoffs;
			tempsec->floor_yoffs = tempsec->ceiling_yoffs = s->ceiling_yoffs;
			tempsec->floorpic_angle = tempsec->ceilingpic_angle = s->ceilingpic_angle;

			if (s->floorpic == skyflatnum) // SKYFIX?
			{
				tempsec->ceilingheight = tempsec->floorheight-1;
				tempsec->floorpic = tempsec->ceilingpic;
				tempsec->floor_xoffs = tempsec->ceiling_xoffs;
				tempsec->floor_yoffs = tempsec->ceiling_yoffs;
				tempsec->floorpic_angle = tempsec->ceilingpic_angle;
			}
			else
			{
				tempsec->ceilingheight = sec->ceilingheight;
				tempsec->floorpic = s->floorpic;
				tempsec->floor_xoffs = s->floor_xoffs;
				tempsec->floor_yoffs = s->floor_yoffs;
				tempsec->floorpic_angle = s->floorpic_angle;
			}

			tempsec->lightlevel = s->lightlevel;

			if (floorlightlevel)
				*floorlightlevel = s->floorlightsec == -1 ? s->lightlevel :
			sectors[s->floorlightsec].lightlevel;

			if (ceilinglightlevel)
				*ceilinglightlevel = s->ceilinglightsec == -1 ? s->lightlevel :
			sectors[s->ceilinglightsec].lightlevel;
		}
		sec = tempsec;
	}

	return sec;
}

boolean R_IsEmptyLine(seg_t *line, sector_t *front, sector_t *back)
{
	return (
		!line->polyseg &&
		back->ceilingpic == front->ceilingpic
		&& back->floorpic == front->floorpic
		&& back->f_slope == front->f_slope
		&& back->c_slope == front->c_slope
		&& back->lightlevel == front->lightlevel
		&& !line->sidedef->midtexture
		// Check offsets too!
		&& back->floor_xoffs == front->floor_xoffs
		&& back->floor_yoffs == front->floor_yoffs
		&& back->floorpic_angle == front->floorpic_angle
		&& back->ceiling_xoffs == front->ceiling_xoffs
		&& back->ceiling_yoffs == front->ceiling_yoffs
		&& back->ceilingpic_angle == front->ceilingpic_angle
		// Consider altered lighting.
		&& back->floorlightsec == front->floorlightsec
		&& back->ceilinglightsec == front->ceilinglightsec
		// Consider colormaps
		&& back->extra_colormap == front->extra_colormap
		&& ((!front->ffloors && !back->ffloors)
		|| Tag_Compare(&front->tags, &back->tags)));
}

size_t numpolys;        // number of polyobjects in current subsector
size_t num_po_ptrs;     // number of polyobject pointers allocated
polyobj_t **po_ptrs; // temp ptr array to sort polyobject pointers

//
// R_PolyobjCompare
//
// Callback for qsort that compares the z distance of two polyobjects.
// Returns the difference such that the closer polyobject will be
// sorted first.
//
static int R_PolyobjCompare(const void *p1, const void *p2)
{
	const polyobj_t *po1 = *(const polyobj_t * const *)p1;
	const polyobj_t *po2 = *(const polyobj_t * const *)p2;

	return po1->zdist - po2->zdist;
}

//
// R_SortPolyObjects
//
// haleyjd 03/03/06: Here's the REAL meat of Eternity's polyobject system.
// Hexen just figured this was impossible, but as mentioned in polyobj.c,
// it is perfectly doable within the confines of the BSP tree. Polyobjects
// must be sorted to draw in DOOM's front-to-back order within individual
// subsectors. This is a modified version of R_SortVisSprites.
//
void R_SortPolyObjects(subsector_t *sub)
{
	if (numpolys)
	{
		polyobj_t *po;
		INT32 i = 0;

		// allocate twice the number needed to minimize allocations
		if (num_po_ptrs < numpolys*2)
		{
			// use free instead realloc since faster (thanks Lee ^_^)
			free(po_ptrs);
			po_ptrs = malloc((num_po_ptrs = numpolys*2)
				* sizeof(*po_ptrs));
		}

		po = sub->polyList;

		while (po)
		{
			po->zdist = R_PointToDist2(viewx, viewy,
				po->centerPt.x, po->centerPt.y);
			po_ptrs[i++] = po;
			po = (polyobj_t *)(po->link.next);
		}

		// the polyobjects are NOT in any particular order, so use qsort
		// 03/10/06: only bother if there are actually polys to sort
		if (numpolys >= 2)
		{
			qsort(po_ptrs, numpolys, sizeof(polyobj_t *),
				R_PolyobjCompare);
		}
	}
}

//
// R_Prep3DFloors
//
// This function creates the lightlists that the given sector uses to light
// floors/ceilings/walls according to the 3D floors.
void R_Prep3DFloors(sector_t *sector)
{
	ffloor_t *rover;
	ffloor_t *best;
	fixed_t bestheight, maxheight;
	INT32 count, i;
	sector_t *sec;
	pslope_t *bestslope = NULL;
	fixed_t heighttest; // I think it's better to check the Z height at the sector's center
	                    // than assume unsloped heights are accurate indicators of order in sloped sectors. -Red

	count = 1;
	for (rover = sector->ffloors; rover; rover = rover->next)
	{
		if ((rover->flags & FF_EXISTS) && (!(rover->flags & FF_NOSHADE)
			|| (rover->flags & FF_CUTLEVEL) || (rover->flags & FF_CUTSPRITES)))
		{
			count++;
			if (rover->flags & FF_DOUBLESHADOW)
				count++;
		}
	}

	if (count != sector->numlights)
	{
		Z_Free(sector->lightlist);
		sector->lightlist = Z_Calloc(sizeof (*sector->lightlist) * count, PU_LEVEL, NULL);
		sector->numlights = count;
	}
	else
		memset(sector->lightlist, 0, sizeof (lightlist_t) * count);

	heighttest = P_GetSectorCeilingZAt(sector, sector->soundorg.x, sector->soundorg.y);

	sector->lightlist[0].height = heighttest + 1;
	sector->lightlist[0].slope = sector->c_slope;
	sector->lightlist[0].lightlevel = &sector->lightlevel;
	sector->lightlist[0].caster = NULL;
	sector->lightlist[0].extra_colormap = &sector->extra_colormap;
	sector->lightlist[0].flags = 0;

	maxheight = INT32_MAX;
	for (i = 1; i < count; i++)
	{
		bestheight = INT32_MAX * -1;
		best = NULL;
		for (rover = sector->ffloors; rover; rover = rover->next)
		{
			rover->lastlight = 0;
			if (!(rover->flags & FF_EXISTS) || (rover->flags & FF_NOSHADE
				&& !(rover->flags & FF_CUTLEVEL) && !(rover->flags & FF_CUTSPRITES)))
			continue;

			heighttest = P_GetFFloorTopZAt(rover, sector->soundorg.x, sector->soundorg.y);

			if (heighttest > bestheight && heighttest < maxheight)
			{
				best = rover;
				bestheight = heighttest;
				bestslope = *rover->t_slope;
				continue;
			}
			if (rover->flags & FF_DOUBLESHADOW) {
				heighttest = P_GetFFloorBottomZAt(rover, sector->soundorg.x, sector->soundorg.y);

				if (heighttest > bestheight
					&& heighttest < maxheight)
				{
					best = rover;
					bestheight = heighttest;
					bestslope = *rover->b_slope;
					continue;
				}
			}
		}
		if (!best)
		{
			sector->numlights = i;
			return;
		}

		sector->lightlist[i].height = maxheight = bestheight;
		sector->lightlist[i].caster = best;
		sector->lightlist[i].flags = best->flags;
		sector->lightlist[i].slope = bestslope;
		sec = &sectors[best->secnum];

		if (best->flags & FF_NOSHADE)
		{
			sector->lightlist[i].lightlevel = sector->lightlist[i-1].lightlevel;
			sector->lightlist[i].extra_colormap = sector->lightlist[i-1].extra_colormap;
		}
		else if (best->flags & FF_COLORMAPONLY)
		{
			sector->lightlist[i].lightlevel = sector->lightlist[i-1].lightlevel;
			sector->lightlist[i].extra_colormap = &sec->extra_colormap;
		}
		else
		{
			sector->lightlist[i].lightlevel = best->toplightlevel;
			sector->lightlist[i].extra_colormap = &sec->extra_colormap;
		}

		if (best->flags & FF_DOUBLESHADOW)
		{
			heighttest = P_GetFFloorBottomZAt(best, sector->soundorg.x, sector->soundorg.y);
			if (bestheight == heighttest) ///TODO: do this in a more efficient way -Red
			{
				sector->lightlist[i].lightlevel = sector->lightlist[best->lastlight].lightlevel;
				sector->lightlist[i].extra_colormap =
					sector->lightlist[best->lastlight].extra_colormap;
			}
			else
				best->lastlight = i - 1;
		}
	}
}

INT32 R_GetPlaneLight(sector_t *sector, fixed_t planeheight, boolean underside)
{
	INT32 i;

	if (!underside)
	{
		for (i = 1; i < sector->numlights; i++)
			if (sector->lightlist[i].height <= planeheight)
				return i - 1;

		return sector->numlights - 1;
	}

	for (i = 1; i < sector->numlights; i++)
		if (sector->lightlist[i].height < planeheight)
			return i - 1;

	return sector->numlights - 1;
}
