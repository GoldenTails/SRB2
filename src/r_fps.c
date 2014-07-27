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

#include "r_fps.h"

#include "r_main.h"
#include "g_game.h"
#include "i_video.h"
#include "r_plane.h"

viewvars_t oldview;
viewvars_t newview;

// taken from r_main.c
// WARNING: a should be unsigned but to add with 2048, it isn't!
#define AIMINGTODY(a) ((FINETANGENT((2048+(((INT32)a)>>ANGLETOFINESHIFT)) & FINEMASK)*160)>>FRACBITS)

void R_InterpolateView(fixed_t frac)
{
	INT32 dy = 0;
	fixed_t diffangle;
	fixed_t diffaim;

	if (FIXED_TO_FLOAT(frac) < 0)
		frac = 0;

	viewx = oldview.x + FixedMul(frac, newview.x - oldview.x);
	viewy = oldview.y + FixedMul(frac, newview.y - oldview.y);
	viewz = oldview.z + FixedMul(frac, newview.z - oldview.z);

	diffangle = newview.angle - oldview.angle;
	diffaim = newview.aim - oldview.aim;

	viewangle = oldview.angle + FixedMul(frac, diffangle);
	aimingangle = oldview.aim + FixedMul(frac, diffaim);

	viewsin = FINESINE(viewangle>>ANGLETOFINESHIFT);
	viewcos = FINECOSINE(viewangle>>ANGLETOFINESHIFT);

	if (rendermode == render_soft)
	{
		// clip it in the case we are looking a hardware 90 degrees full aiming
		// (lmps, network and use F12...)
		G_SoftwareClipAimingPitch((INT32 *)&aimingangle);

		dy = AIMINGTODY(aimingangle) * viewwidth/BASEVIDWIDTH;

		yslope = &yslopetab[(3*viewheight/2) - dy];
	}
	centery = (viewheight/2) + dy;
	centeryfrac = centery<<FRACBITS;
}

void R_UpdateViewInterpolation()
{
	oldview = newview;
	/*oldview.viewx = newview.viewx;
	oldview.viewy = newview.viewy;
	oldview.viewz = newview.viewz;
	oldview.viewangle = newview.viewangle;*/
}
