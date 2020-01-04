// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_modeltextures.h
/// \brief 3D model texture loading.

#ifndef _R_MODELTEXTURES_H_
#define _R_MODELTEXTURES_H_

#include "doomtype.h"
#include "r_model.h"

boolean Model_LoadTexture(modelinfo_t *model, INT32 skinnum);
boolean Model_LoadBlendTexture(modelinfo_t *model);

#endif
