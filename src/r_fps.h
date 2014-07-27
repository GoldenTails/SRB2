// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2014 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_fps.h
/// \brief Uncapped framerate stuff.

#ifndef __R_FPS_H__
#define __R_FPS_H__

#include "m_fixed.h"
#include "p_local.h"
#include "r_state.h"

typedef struct {
	mobj_t mobj;
	fixed_t x;
	fixed_t y;
	fixed_t z;

	angle_t angle;
	angle_t aim;
} viewvars_t;

//extern viewvars_t oldview;
extern viewvars_t newview;

void R_InterpolateView(fixed_t frac);
void R_UpdateViewInterpolation();
#endif
