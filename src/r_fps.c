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

static viewvars_t p1view_old;
static viewvars_t p1view_new;
static viewvars_t p2view_old;
static viewvars_t p2view_new;
static viewvars_t sky1view_old;
static viewvars_t sky1view_new;
static viewvars_t sky2view_old;
static viewvars_t sky2view_new;

viewvars_t *oldview = &p1view_old;
viewvars_t *newview = &p1view_new;


enum viewcontext_e viewcontext = VIEWCONTEXT_PLAYER1;

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

	viewx = oldview->x + FixedMul(frac, newview->x - oldview->x);
	viewy = oldview->y + FixedMul(frac, newview->y - oldview->y);
	viewz = oldview->z + FixedMul(frac, newview->z - oldview->z);

	diffangle = newview->angle - oldview->angle;
	diffaim = newview->aim - oldview->aim;

	viewangle = oldview->angle + FixedMul(frac, diffangle);
	aimingangle = oldview->aim + FixedMul(frac, diffaim);

	viewsin = FINESINE(viewangle>>ANGLETOFINESHIFT);
	viewcos = FINECOSINE(viewangle>>ANGLETOFINESHIFT);

	// this is gonna create some interesting visual errors for long distance teleports...
	// might want to recalculate the view sector every frame instead...
	viewplayer = frac >= FRACUNIT ? newview->player : oldview->player;
	viewsector = frac >= FRACUNIT ? newview->sector : oldview->sector;
	viewsky = frac >= FRACUNIT ? newview->sky : oldview->sky;

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
	p1view_old = p1view_new;
	p2view_old = p2view_new;
	sky1view_old = sky1view_new;
	sky2view_old = sky2view_new;
}

void R_SetViewContext(enum viewcontext_e _viewcontext)
{
	I_Assert(_viewcontext == VIEWCONTEXT_PLAYER1
			|| _viewcontext == VIEWCONTEXT_PLAYER2
			|| _viewcontext == VIEWCONTEXT_SKY);
	viewcontext = _viewcontext;

	switch (viewcontext)
	{
		case VIEWCONTEXT_PLAYER1:
			oldview = &p1view_old;
			newview = &p1view_new;
			break;
		case VIEWCONTEXT_PLAYER2:
			oldview = &p2view_old;
			newview = &p2view_new;
			break;
		case VIEWCONTEXT_SKY1:
			oldview = &sky1view_old;
			newview = &sky1view_new;
			break;
		case VIEWCONTEXT_SKY2:
			oldview = &sky2view_old;
			newview = &sky2view_new;
			break;
		default:
			I_Error("viewcontext value is invalid: we should never get here without an assert!!");
			break;
	}
}