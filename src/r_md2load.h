/*
	From the 'Wizard2' engine by Spaddlewit Inc. ( http://www.spaddlewit.com )
	An experimental work-in-progress.

	Donated to Sonic Team Junior and adapted to work with
	Sonic Robo Blast 2. The license of this code matches whatever
	the licensing is for Sonic Robo Blast 2.
*/

#ifndef _R_MD2LOAD_H_
#define _R_MD2LOAD_H_

#include "r_model.h"
#include "doomtype.h"

// Load the Model
model_t *MD2_LoadModel(const char *fileName, int ztag, boolean useFloat);

#endif