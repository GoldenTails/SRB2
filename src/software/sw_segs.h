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
/// \file  sw_segs.h
/// \brief Refresh module, drawing LineSegs from BSP

#ifndef __SW_SEGS__
#define __SW_SEGS__

#ifdef __GNUG__
#pragma interface
#endif

void SWR_RenderMaskedSegRange(drawseg_t *ds, INT32 x1, INT32 x2);
void SWR_RenderThickSideRange(drawseg_t *ds, INT32 x1, INT32 x2, ffloor_t *pffloor);
void SWR_StoreWallRange(INT32 start, INT32 stop);

#endif
