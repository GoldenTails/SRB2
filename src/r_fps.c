// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze, Andrey Budko (prboom)
// Copyright (C) 1999-2019 by Sonic Team Junior.
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
#include "p_spec.h"
#include "r_state.h"
#include "hardware/hw_main.h"

static viewvars_t p1view_old;
static viewvars_t p1view_new;
static viewvars_t p2view_old;
static viewvars_t p2view_new;
static viewvars_t sky1view_old;
static viewvars_t sky1view_new;
static viewvars_t sky2view_old;
static viewvars_t sky2view_new;

static viewvars_t *oldview = &p1view_old;
viewvars_t *newview = &p1view_new;


enum viewcontext_e viewcontext = VIEWCONTEXT_PLAYER1;

static fixed_t R_LerpFixed(fixed_t from, fixed_t to, fixed_t frac)
{
	return FixedMul(frac, to - from);
}

static angle_t R_LerpAngle(angle_t from, angle_t to, fixed_t frac)
{
	return FixedMul(frac, to - from);
}

// recalc necessary stuff for mouseaiming
// slopes are already calculated for the full possible view (which is 4*viewheight).
// 18/08/18: (No it's actually 16*viewheight, thanks Jimita for finding this out)
static void R_SetupFreelook(player_t *player, boolean skybox)
{
	INT32 dy = 0;

#ifndef HWRENDER
	(void)player;
	(void)skybox;
#endif

	// clip it in the case we are looking a hardware 90 degrees full aiming
	// (lmps, network and use F12...)
	if (rendermode == render_soft
#ifdef HWRENDER
		|| (rendermode == render_opengl
			&& (cv_glshearing.value == 1
			|| (cv_glshearing.value == 2 && R_IsViewpointThirdPerson(player, skybox))))
#endif
		)
	{
		G_SoftwareClipAimingPitch((INT32 *)&aimingangle);
	}

	if (rendermode == render_soft)
	{
		dy = (AIMINGTODY(aimingangle)>>FRACBITS) * viewwidth/BASEVIDWIDTH;
		yslope = &yslopetab[viewheight*8 - (viewheight/2 + dy)];
	}

	centery = (viewheight/2) + dy;
	centeryfrac = centery<<FRACBITS;
}

void R_InterpolateView(player_t *player, boolean skybox, fixed_t frac)
{
	if (frac < 0)
		frac = 0;
	if (frac > FRACUNIT)
		frac = FRACUNIT;

	viewx = oldview->x + R_LerpFixed(oldview->x, newview->x, frac);
	viewy = oldview->y + R_LerpFixed(oldview->y, newview->y, frac);
	viewz = oldview->z + R_LerpFixed(oldview->z, newview->z, frac);

	viewangle = oldview->angle + R_LerpAngle(oldview->angle, newview->angle, frac);
	aimingangle = oldview->aim + R_LerpAngle(oldview->aim, newview->aim, frac);

	viewsin = FINESINE(viewangle>>ANGLETOFINESHIFT);
	viewcos = FINECOSINE(viewangle>>ANGLETOFINESHIFT);

	// this is gonna create some interesting visual errors for long distance teleports...
	// might want to recalculate the view sector every frame instead...
	viewplayer = newview->player;
	viewsector = R_PointInSubsector(viewx, viewy)->sector;

	R_SetupFreelook(player, skybox);
}

void R_UpdateViewInterpolation(void)
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
			|| _viewcontext == VIEWCONTEXT_SKY1
			|| _viewcontext == VIEWCONTEXT_SKY2);
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
