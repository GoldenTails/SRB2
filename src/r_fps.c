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

	if (FIXED_TO_FLOAT(frac) < 0)
		frac = 0;

	viewx = oldview->x + R_LerpFixed(oldview->x, newview->x, frac);
	viewy = oldview->y + R_LerpFixed(oldview->y, newview->y, frac);
	viewz = oldview->z + R_LerpFixed(oldview->z, newview->z, frac);

	viewangle = oldview->angle + R_LerpAngle(oldview->angle, newview->angle, frac);
	aimingangle = oldview->aim + R_LerpAngle(oldview->aim, newview->aim, frac);

	viewsin = FINESINE(viewangle>>ANGLETOFINESHIFT);
	viewcos = FINECOSINE(viewangle>>ANGLETOFINESHIFT);

	// this is gonna create some interesting visual errors for long distance teleports...
	// might want to recalculate the view sector every frame instead...
	if (frac >= FRACUNIT)
	{
		viewplayer = newview->player;
		viewsector = newview->sector;
		viewsky    = newview->sky;
	}
	else
	{
		viewplayer = oldview->player;
		viewsector = oldview->sector;
		viewsky    = oldview->sky;
	}

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

void R_SetThinkerOldStates(void)
{
	thinker_t *th;

	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th == NULL)
		{
			break;
		}
		if (th->function.acp1 == (actionf_p1)P_MobjThinker)
		{
			mobj_t *mo = (mobj_t *) th;
			mo->old_x = mo->new_x;
			mo->old_y = mo->new_y;
			mo->old_z = mo->new_z;
		}
		if (th->function.acp1 == (actionf_p1)P_RainThinker || th->function.acp1 == (actionf_p1)P_SnowThinker)
		{
			precipmobj_t *mo = (precipmobj_t *) th;
			mo->old_x = mo->new_x;
			mo->old_y = mo->new_y;
			mo->old_z = mo->new_z;
		}
	}
}

void R_SetThinkerNewStates(void)
{
	thinker_t *th;

	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th == NULL)
		{
			break;
		}
		if (th->function.acp1 == (actionf_p1)P_MobjThinker)
		{
			mobj_t *mo = (mobj_t *) th;
			if (mo->firstlerp == 0)
			{
				mo->firstlerp = 1;
				mo->old_x = mo->x;
				mo->old_y = mo->y;
				mo->old_z = mo->z;
			}
			mo->new_x = mo->x;
			mo->new_y = mo->y;
			mo->new_z = mo->z;
		}
		if (th->function.acp1 == (actionf_p1)P_RainThinker || th->function.acp1 == (actionf_p1)P_SnowThinker)
		{
			precipmobj_t *mo = (precipmobj_t *) th;
			if (mo->firstlerp == 0)
			{
				mo->firstlerp = 1;
				mo->old_x = mo->x;
				mo->old_y = mo->y;
				mo->old_z = mo->z;
			}
			mo->new_x = mo->x;
			mo->new_y = mo->y;
			mo->new_z = mo->z;
		}
	}
}

void R_DoThinkerLerp(fixed_t frac)
{
	thinker_t *th;

	if (cv_capframerate.value != 0)
	{
		return;
	}

	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th == NULL)
		{
			break;
		}
		if (th->function.acp1 == (actionf_p1)P_MobjThinker)
		{
			mobj_t *mo = (mobj_t *) th;
			if (mo->firstlerp < 1) continue;
			mo->x = mo->old_x + R_LerpFixed(mo->old_x, mo->new_x, frac);
			mo->y = mo->old_y + R_LerpFixed(mo->old_y, mo->new_y, frac);
			mo->z = mo->old_z + R_LerpFixed(mo->old_z, mo->new_z, frac);
		}
		if (th->function.acp1 == (actionf_p1)P_RainThinker || th->function.acp1 == (actionf_p1)P_SnowThinker)
		{
			precipmobj_t *mo = (precipmobj_t *) th;
			if (mo->firstlerp < 1) continue;
			mo->x = R_LerpFixed(mo->old_x, mo->new_x, frac);
			mo->y = R_LerpFixed(mo->old_y, mo->new_y, frac);
			mo->z = R_LerpFixed(mo->old_z, mo->new_z, frac);
		}
	}
}

void R_ResetThinkerLerp(void)
{
	thinker_t *th;

	if (cv_capframerate.value != 0)
	{
		return;
	}

	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th == NULL)
		{
			break;
		}
		if (th->function.acp1 == (actionf_p1)P_MobjThinker)
		{
			mobj_t *mo = (mobj_t *) th;
			if (mo->firstlerp < 1) continue;
			mo->x = mo->new_x;
			mo->y = mo->new_y;
			mo->z = mo->new_z;
		}
		if (th->function.acp1 == (actionf_p1)P_RainThinker || th->function.acp1 == (actionf_p1)P_SnowThinker)
		{
			precipmobj_t *mo = (precipmobj_t *) th;
			if (mo->firstlerp < 1) continue;
			mo->x = mo->new_x;
			mo->y = mo->new_y;
			mo->z = mo->new_z;
		}
	}
}

fixed_t R_LerpFixed(fixed_t from, fixed_t to, fixed_t frac)
{
	return FixedMul(frac, to - from);
}

INT32 R_LerpInt32(INT32 from, INT32 to, fixed_t frac)
{
	return FixedInt(FixedMul(frac, (to*FRACUNIT) - (from*FRACUNIT)));
}

angle_t R_LerpAngle(angle_t from, angle_t to, fixed_t frac)
{
	return FixedMul(frac, to - from);
}