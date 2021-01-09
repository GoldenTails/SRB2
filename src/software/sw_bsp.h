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
/// \file  sw_bsp.h
/// \brief Refresh module, BSP traversal and handling

#ifndef __SW_BSP__
#define __SW_BSP__

#ifdef __GNUG__
#pragma interface
#endif

// drawsegs are allocated on the fly... see r_segs.c

extern drawseg_t *curdrawsegs;
extern drawseg_t *drawsegs;
extern drawseg_t *ds_p;
extern INT32 doorclosed;

// BSP?
void SWR_ClearClipSegs(void);
void SWR_PortalClearClipSegs(INT32 start, INT32 end);
void SWR_ClearDrawSegs(void);
void SWR_RenderBSPNode(INT32 bspnum);

#endif
