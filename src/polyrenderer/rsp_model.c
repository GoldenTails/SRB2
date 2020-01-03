// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
// Copyright (C) 2019-2020 by Jaime "Lactozilla" Passos.
// Copyright (C) 2019 by Vinícius "Arkus-Kotan" Telésforo.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  rsp_model.c
/// \brief Model loading

#include "r_softpoly.h"

#ifdef __GNUC__
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../doomdef.h"
#include "../doomstat.h"
#include "../d_main.h"
#include "../r_bsp.h"
#include "../r_main.h"
#include "../m_misc.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../r_things.h"
#include "../v_video.h"
#include "../r_draw.h"
#include "../p_tick.h"
#include "../r_model.h"
#include "../r_modeltextures.h"

md2_t *RSP_ModelAvailable(spritenum_t spritenum, skin_t *skin)
{
	char filename[64];
	md2_t *md2;

	// invalid sprite number
	if ((unsigned)spritenum >= NUMSPRITES || (unsigned)spritenum == SPR_NULL)
		return NULL;

	if (skin && spritenum == SPR_PLAY) // Use the player MD2 list if the mobj has a skin and is using the player sprites
	{
		md2 = &md2_playermodels[skin-skins];
		md2->skin = skin-skins;
	}
	else if (spritenum == SPR_PLAY)	// use default model
	{
		md2 = &md2_playermodels[0];
		md2->skin = 0;
	}
	else
		md2 = &md2_models[spritenum];

	if (md2->notfound)/* Let PLAY (or lumps get fucked) */
		md2 = &md2_models[spritenum];

	if (md2->notfound)
		return NULL;

	if (!md2->model)
	{
		sprintf(filename, "models/%s", md2->filename);
		md2->model = R_LoadModel(filename);

		if (!md2->model)
		{
			md2->notfound = true;
			return NULL;
		}
	}

	return md2;
}

// Macros to improve code readability
#define MD3_XYZ_SCALE   (1.0f / 64.0f)
#define VERTEX_OFFSET   ((i * 9) + (j * 3))		/* (i * 9) = (XYZ coords * vertex count) */
#define UV_OFFSET       ((i * 6) + (j * 2))		/* (i * 6) = (UV coords * vertex count) */

static fpquaternion_t *modelquaternion = NULL;

boolean RSP_RenderModel(vissprite_t *spr)
{
	INT32 frameIndex = 0;
	INT32 nextFrameIndex = -1;
	INT32 mod;
	UINT8 spr2 = 0;
	md2_t *md2;
	INT32 meshnum;

	// Not funny id Software, didn't laugh.
	unsigned short idx = 0;
	boolean useTinyFrames;

	mobj_t *mobj = spr->mobj;
	if (!mobj)
		return false;

	// load sprite viewpoint
	if (rsp_portalrender)
		RSP_RestoreSpriteViewpoint(spr);

	if (modelquaternion == NULL)
	{
		fpquaternion_t quat = RSP_QuaternionFromEuler(0.0f, 0.0f, -90.0f);
		modelquaternion = Z_Malloc(sizeof(fpquaternion_t), PU_SOFTPOLY, NULL);
		M_Memcpy(modelquaternion, &quat, sizeof(fpquaternion_t));
	}

	// Look at R_ProjectSprite for more
	{
		fpvector4_t modelvec;
		rsp_texture_t *texture, sprtex;
		rsp_spritetexture_t *sprtexp;
		INT32 durs = mobj->state->tics;
		INT32 tics = mobj->tics;
		boolean vflip = (!(mobj->eflags & MFE_VERTICALFLIP) != !(mobj->frame & FF_VERTICALFLIP));
		boolean papersprite = (mobj->frame & FF_PAPERSPRITE);
		spritedef_t *sprdef;
		spriteframe_t *sprframe;
		float finalscale;
		float pol = 0.0f;
		INT32 skinnum = 0;
		INT32 tc = 0;

		skincolors_t skincolor = SKINCOLOR_NONE;
		UINT8 *translation = NULL;
		boolean translationset = true;

		md2 = RSP_ModelAvailable(spr->spritenum, (skin_t *)spr->skin);
		if (!md2)
		{
			// restore previous viewpoint
			if (rsp_portalrender)
				RSP_RestoreViewpoint();
			return false;
		}

		// Lactozilla: Disallow certain models from rendering
		if (!Model_AllowRendering(mobj))
		{
			// restore previous viewpoint
			if (rsp_portalrender)
				RSP_RestoreViewpoint();
			return false;
		}

		// texture blending
		if (mobj->color)
			skincolor = (skincolors_t)mobj->color;
		else if (mobj->sprite == SPR_PLAY) // Looks like a player, but doesn't have a color? Get rid of green sonic syndrome.
			skincolor = (skincolors_t)skins[0].prefcolor;

		// load normal texture
		if (!md2->texture)
		{
			if (mobj->skin && mobj->sprite == SPR_PLAY)
				skinnum = (skin_t*)mobj->skin-skins;
			Model_LoadTexture(md2, skinnum);
		}

		// set translation
		if ((mobj->flags & (MF_ENEMY|MF_BOSS)) && (mobj->flags2 & MF2_FRET) && !(mobj->flags & MF_GRENADEBOUNCE) && (leveltime & 1)) // Bosses "flash"
		{
			if (mobj->type == MT_CYBRAKDEMON)
				tc = TC_ALLWHITE;
			else if (mobj->type == MT_METALSONIC_BATTLE)
				tc = TC_METALSONIC;
			else
				tc = TC_BOSS;
			translation = R_GetTranslationColormap(tc, mobj->color, GTC_CACHE);
		}
		else if (mobj->colorized)
		{
			tc = TC_RAINBOW;
			translation = R_GetTranslationColormap(tc, mobj->color, GTC_CACHE);
		}
		else if (mobj->player && mobj->player->dashmode >= DASHMODE_THRESHOLD
			&& (mobj->player->charflags & SF_DASHMODE)
			&& ((leveltime/2) & 1))
		{
			tc = TC_RAINBOW;
			if (mobj->player->charflags & SF_MACHINE)
			{
				tc = TC_DASHMODE;
				translation = R_GetTranslationColormap(tc, 0, GTC_CACHE);
			}
			else
				translation = R_GetTranslationColormap(tc, mobj->color, GTC_CACHE);
		}
		else
		{
			if (mobj->color)
				tc = TC_DEFAULT; // intentional
			translationset = false;
		}

		// load blend texture
		if (!md2->blendtexture)
			Model_LoadBlendTexture(md2);

		// load translated texture
		if (tc < 0)
			tc = -tc;
		if (tc && md2->rsp_blendtex[skincolor][tc].data == NULL)
			RSP_CreateModelTexture(md2, tc, skincolor);

		// use corresponding texture for this model
		if (md2->rsp_blendtex[skincolor][tc].data != NULL)
			texture = &md2->rsp_blendtex[skincolor][tc];
		else
			texture = &md2->rsp_tex;

		if (mobj->skin && mobj->sprite == SPR_PLAY)
			sprdef = &((skin_t *)mobj->skin)->sprites[mobj->sprite2];
		else
			sprdef = &sprites[mobj->sprite];

		sprframe = &sprdef->spriteframes[spr->mobj->frame & FF_FRAMEMASK];

		if (!texture->data)
		{
			unsigned rot;
			angle_t ang;
			UINT8 flip;

			if (sprframe->rotate != SRF_SINGLE || papersprite)
				ang = R_PointToAngle (mobj->x, mobj->y) - (mobj->player ? mobj->player->drawangle : mobj->angle);

			if (sprframe->rotate == SRF_SINGLE)
			{
				// use single rotation for all views
				rot = 0;                        //Fab: for vis->patch below
				flip = sprframe->flip; // Will only be 0x00 or 0xFF
			}
			else
			{
				// choose a different rotation based on player view
				if ((sprframe->rotate & SRF_RIGHT) && (ang < ANGLE_180)) // See from right
					rot = 6; // F7 slot
				else if ((sprframe->rotate & SRF_LEFT) && (ang >= ANGLE_180)) // See from left
					rot = 2; // F3 slot
				else // Normal behaviour
					rot = (ang+ANGLE_202h)>>29;

				flip = sprframe->flip & (1<<rot);
			}

			// sprite translation
			if (!translationset)
			{
				if (mobj->color)
				{
					// New colormap stuff for skins Tails 06-07-2002
					if (mobj->skin && mobj->sprite == SPR_PLAY) // This thing is a player!
						translation = R_GetTranslationColormap((INT32)skinnum, mobj->color, GTC_CACHE);
					else
						translation = R_GetTranslationColormap(TC_DEFAULT, mobj->color, GTC_CACHE);
				}
				else if (mobj->sprite == SPR_PLAY) // Looks like a player, but doesn't have a color? Get rid of green sonic syndrome.
					translation = R_GetTranslationColormap(TC_DEFAULT, skins[0].prefcolor, GTC_CACHE);
				else
					translation = NULL;
			}

			// get rsp_texture
			sprtexp = &sprframe->rsp_texture[rot];
			if (!sprtexp)
			{
				// restore previous viewpoint
				if (rsp_portalrender)
					RSP_RestoreViewpoint();
				return false;
			}

			sprtex.width = sprtexp->width;
			sprtex.height = sprtexp->height;

			// make the patch
			if (!sprtexp->data)
			{
				patch_t *source;

				// uuhhh.....
				if (!sprtexp->lumpnum)
				{
					// restore previous viewpoint
					if (rsp_portalrender)
						RSP_RestoreViewpoint();
					return false;
				}

				// still not a patch yet
				// (R_CheckIfPatch has most likely failed)
				if ((sprtexp->width) < 1 || (sprtexp->height < 1))
				{
					// restore previous viewpoint
					if (rsp_portalrender)
						RSP_RestoreViewpoint();
					return false;
				}

				// cache the source patch
				source = (patch_t *)W_CacheLumpNum(sprtexp->lumpnum, PU_STATIC);

				// make the buffer
				sprtexp->data = Z_Malloc(sprtexp->width * sprtexp->height, PU_SOFTPOLY, NULL);
				memset(sprtexp->data, TRANSPARENTPIXEL, sprtexp->width * sprtexp->height);

				// generate the texture, then clear lumpnum
				RSP_GenerateTexture(source, sprtexp->data, 0, 0, sprtexp->width, sprtexp->height, flip, NULL, NULL);
				sprtexp->lumpnum = 0;

				// aight bro u have lost yuor cache privileges
				Z_Free(source);
			}

			sprtex.data = sprtexp->data;
			texture = &sprtex;
		}

		if (spr->mobj->frame & FF_ANIMATE)
		{
			// set duration and tics to be the correct values for FF_ANIMATE states
			durs = spr->mobj->state->var2;
			tics = spr->mobj->anim_duration;
		}

		frameIndex = (mobj->frame & FF_FRAMEMASK);
		if (mobj->skin && mobj->sprite == SPR_PLAY && md2->model->spr2frames)
		{
			spr2 = Model_GetSprite2(md2, mobj->skin, mobj->sprite2, mobj->player);
			mod = md2->model->spr2frames[spr2].numframes;
#ifndef DONTHIDEDIFFANIMLENGTH // by default, different anim length is masked by the mod
			if (mod > (INT32)((skin_t *)mobj->skin)->sprites[spr2].numframes)
				mod = ((skin_t *)mobj->skin)->sprites[spr2].numframes;
#endif
			if (!mod)
				mod = 1;
			frameIndex = md2->model->spr2frames[spr2].frames[frameIndex%mod];
		}
		else
		{
			mod = md2->model->meshes[0].numFrames;
			if (!mod)
				mod = 1;
		}

#ifdef USE_MODEL_NEXTFRAME
#define INTERPOLERATION_LIMIT TICRATE/4
		if (cv_modelinterpolation.value && tics <= durs && tics <= INTERPOLERATION_LIMIT)
		{
			if (durs > INTERPOLERATION_LIMIT)
				durs = INTERPOLERATION_LIMIT;

			if (mobj->skin && mobj->sprite == SPR_PLAY && md2->model->spr2frames)
			{
				if (Model_CanInterpolateSprite2(&md2->model->spr2frames[spr2])
					&& (mobj->frame & FF_ANIMATE
					|| (mobj->state->nextstate != S_NULL
					&& states[mobj->state->nextstate].sprite == SPR_PLAY
					&& ((P_GetSkinSprite2(mobj->skin, (((mobj->player && mobj->player->powers[pw_super]) ? FF_SPR2SUPER : 0)|states[mobj->state->nextstate].frame) & FF_FRAMEMASK, mobj->player) == mobj->sprite2)))))
				{
					nextFrameIndex = (mobj->frame & FF_FRAMEMASK) + 1;
					if (nextFrameIndex >= mod)
						nextFrameIndex = 0;
					if (frameIndex || !(mobj->state->frame & FF_SPR2ENDSTATE))
						nextFrameIndex = md2->model->spr2frames[spr2].frames[nextFrameIndex];
					else
						nextFrameIndex = -1;
				}
			}
			else if (Model_CanInterpolate(mobj, md2->model))
			{
				// frames are handled differently for states with FF_ANIMATE, so get the next frame differently for the interpolation
				if (mobj->frame & FF_ANIMATE)
				{
					nextFrameIndex = (mobj->frame & FF_FRAMEMASK) + 1;
					if (nextFrameIndex >= (INT32)(mobj->state->var1 + (mobj->state->frame & FF_FRAMEMASK)))
						nextFrameIndex = (mobj->state->frame & FF_FRAMEMASK) % mod;
				}
				else
				{
					if (mobj->state->nextstate != S_NULL && states[mobj->state->nextstate].sprite != SPR_NULL
					&& !(mobj->player && (mobj->state->nextstate == S_PLAY_WAIT) && mobj->state == &states[S_PLAY_STND]))
						nextFrameIndex = (states[spr->mobj->state->nextstate].frame & FF_FRAMEMASK) % mod;
				}
			}

			// don't interpolate if instantaneous or infinite in length
			if (durs != 0 && durs != -1 && tics != -1)
			{
				UINT32 newtime = (durs - tics);

				pol = (newtime)/(float)durs;

				if (pol > 1.0f)
					pol = 1.0f;

				if (pol < 0.0f)
					pol = 0.0f;
			}
		}
#undef INTERPOLERATION_LIMIT
#endif

		// SRB2CBTODO: MD2 scaling support
		finalscale = md2->scale * FIXED_TO_FLOAT(mobj->scale);
		finalscale *= 0.5f;

		// Render every mesh
		for (meshnum = 0; meshnum < md2->model->numMeshes; meshnum++)
		{
			mesh_t *mesh = &md2->model->meshes[meshnum];
			rsp_triangle_t triangle;
			float theta, cs, sn;
			fixed_t model_angle;
			UINT16 i, j;
			float scale = finalscale;

			mdlframe_t *frame = NULL, *nextframe = NULL;
			tinyframe_t *tinyframe = NULL, *tinynextframe = NULL;

			useTinyFrames = md2->model->meshes[meshnum].tinyframes != NULL;
			if (useTinyFrames)
			{
				tinyframe = &mesh->tinyframes[frameIndex % mesh->numFrames];
				if (nextFrameIndex != -1)
					tinynextframe = &mesh->tinyframes[nextFrameIndex % mesh->numFrames];
				scale *= MD3_XYZ_SCALE;
			}
			else
			{
				frame = &mesh->frames[frameIndex % mesh->numFrames];
				if (nextFrameIndex != -1)
					nextframe = &mesh->frames[nextFrameIndex % mesh->numFrames];
			}

			// clear triangle struct
			// avoid undefined behaviour.............
			memset(&triangle, 0x00, sizeof(rsp_triangle_t));

			// set triangle texture
			if (texture->data)
				triangle.texture = texture;

			// set colormap, translation and transmap
			triangle.colormap = spr->colormap;
			if (spr->extra_colormap)
			{
				if (!triangle.colormap)
					triangle.colormap = spr->extra_colormap->colormap;
				else
					triangle.colormap = &spr->extra_colormap->colormap[triangle.colormap - colormaps];
			}
			triangle.translation = translation;
			triangle.transmap = spr->transmap;

			// vertical flip
			triangle.flipped = vflip;

			// set model angle
			if (mobj->player)
				model_angle = AngleFixed(mobj->player->drawangle);
			else
				model_angle = AngleFixed(mobj->angle);
			if (!sprframe->rotate && (!papersprite))
				model_angle = AngleFixed((R_PointToAngle(mobj->x, mobj->y))-ANGLE_180);

			// model angle in radians
			theta = -(FIXED_TO_FLOAT(model_angle) * M_PI / 180.0f);
			cs = cos(theta);
			sn = sin(theta);

			// render every triangle
			for (i = 0; i < mesh->numTriangles; i++)
			{
				float x, y, z;
				float s, t;
				float *uv = mesh->uvs;

				x = FIXED_TO_FLOAT(mobj->x);
				y = FIXED_TO_FLOAT(mobj->y) + md2->offset;

				if (mobj->eflags & MFE_VERTICALFLIP)
					z = FIXED_TO_FLOAT(mobj->z + mobj->height);
				else
					z = FIXED_TO_FLOAT(mobj->z);

				// Rotate the triangle -90 degrees and invert the Z coordinate
				#define FIXTRIANGLE(rotx, roty, rotz) \
				{ \
					RSP_MakeVector4(modelvec, rotx, roty, rotz); \
					RSP_QuaternionRotateVector(&modelvec, modelquaternion); \
					rotx = modelvec.x; \
					roty = modelvec.y; \
					rotz = modelvec.z; \
					roty *= -1.0; \
				} \

				for (j = 0; j < 3; j++)
				{
					if (useTinyFrames)
					{
						idx = mesh->indices[(i * 3) + j];
						s = *(uv + (idx * 2));
						t = *(uv + (idx * 2) + 1);
					}
					else
					{
						s = uv[UV_OFFSET];
						t = uv[UV_OFFSET+1];
					}

					if (!(nextframe || tinynextframe) || fpclassify(pol) == FP_ZERO)
					{
						float vx, vy, vz;
						float mx, my, mz;

						if (useTinyFrames)
						{
							short *vert = tinyframe->vertices;
							vx = *(vert + (idx * 3)) * scale;
							vy = *(vert + (idx * 3) + 1) * scale;
							vz = *(vert + (idx * 3) + 2) * scale;
							FIXTRIANGLE(vx, vy, vz)
						}
						else
						{
							vx = frame->vertices[VERTEX_OFFSET] * scale;
							vy = frame->vertices[VERTEX_OFFSET+1] * scale;
							vz = frame->vertices[VERTEX_OFFSET+2] * scale;
							FIXTRIANGLE(vx, vy, vz)
						}

						// QUICK MATHS
						mx = (vx * cs) - (vy * sn);
						my = (vx * sn) + (vy * cs);
						mz = vz * (vflip ? -1 : 1);

						RSP_MakeVector4(triangle.vertices[j].position, (x + mx), (-z + mz), (-y + my));
					}
					else
					{
						float px1, py1, pz1;
						float px2, py2, pz2;
						float mx1, my1, mz1;
						float mx2, my2, mz2;
						float lx, ly, lz;

						// Interpolate
						if (useTinyFrames)
						{
							short *vert = tinyframe->vertices;
							short *nvert = tinynextframe->vertices;
							px1 = *(vert + (idx * 3)) * scale;
							py1 = *(vert + (idx * 3) + 1) * scale;
							pz1 = *(vert + (idx * 3) + 2) * scale;
							px2 = *(nvert + (idx * 3)) * scale;
							py2 = *(nvert + (idx * 3) + 1) * scale;
							pz2 = *(nvert + (idx * 3) + 2) * scale;
							FIXTRIANGLE(px1, py1, pz1)
							FIXTRIANGLE(px2, py2, pz2)
						}
						else
						{
							px1 = frame->vertices[VERTEX_OFFSET] * scale;
							py1 = frame->vertices[VERTEX_OFFSET+1] * scale;
							pz1 = frame->vertices[VERTEX_OFFSET+2] * scale;
							px2 = nextframe->vertices[VERTEX_OFFSET] * scale;
							py2 = nextframe->vertices[VERTEX_OFFSET+1] * scale;
							pz2 = nextframe->vertices[VERTEX_OFFSET+2] * scale;
							FIXTRIANGLE(px1, py1, pz1)
							FIXTRIANGLE(px2, py2, pz2)
						}

						// QUICK MATHS
						mx1 = (px1 * cs) - (py1 * sn);
						my1 = (px1 * sn) + (py1 * cs);
						mz1 = pz1 * (vflip ? -1 : 1);

						mx2 = (px2 * cs) - (py2 * sn);
						my2 = (px2 * sn) + (py2 * cs);
						mz2 = pz2 * (vflip ? -1 : 1);

						// interpolate
						lx = FloatLerp(mx1, mx2, pol);
						ly = FloatLerp(my1, my2, pol);
						lz = FloatLerp(mz1, mz2, pol);

						RSP_MakeVector4(triangle.vertices[j].position, (x + lx), (-z + lz), (-y + ly));
					}

					triangle.vertices[j].uv.u = s;
					triangle.vertices[j].uv.v = t;
				}

				RSP_TransformTriangle(&triangle);
			}
		}
	}

#undef FIXTRIANGLE

	RSP_ClearDepthBuffer();
	return true;
}

// Define for getting accurate color brightness readings according to how the human eye sees them.
// https://en.wikipedia.org/wiki/Relative_luminance
// 0.2126 to red
// 0.7152 to green
// 0.0722 to blue
#define SETBRIGHTNESS(brightness,r,g,b) \
	brightness = (UINT8)(((1063*(UINT16)(r))/5000) + ((3576*(UINT16)(g))/5000) + ((361*(UINT16)(b))/5000))

static boolean BlendTranslations(UINT8 *px, RGBA_t *sourcepx, RGBA_t *blendpx, INT32 translation)
{
	if (translation == TC_BOSS)
	{
		// Turn everything below a certain threshold white
		if ((sourcepx->s.red == sourcepx->s.green) && (sourcepx->s.green == sourcepx->s.blue) && sourcepx->s.blue <= 82)
		{
			// Lactozilla: Invert the colors
			UINT8 invcol = (255 - sourcepx->s.blue);
			*px = NearestColorSafe(invcol, invcol, invcol);
		}
		else
			*px = NearestColorSafe(sourcepx->s.red, sourcepx->s.green, sourcepx->s.blue);
		return true;
	}
	else if (translation == TC_METALSONIC)
	{
		// Turn everything below a certain blue threshold white
		if (sourcepx->s.red == 0 && sourcepx->s.green == 0 && sourcepx->s.blue <= 82)
			*px = NearestColorSafe(255, 255, 255);
		else
			*px = NearestColorSafe(sourcepx->s.red, sourcepx->s.green, sourcepx->s.blue);
		return true;
	}
	else if (translation == TC_DASHMODE && blendpx)
	{
		if (sourcepx->s.alpha == 0 && blendpx->s.alpha == 0)
		{
			// Don't bother with blending the pixel if the alpha of the blend pixel is 0
			*px = NearestColorSafe(sourcepx->s.red, sourcepx->s.green, sourcepx->s.blue);
		}
		else
		{
			UINT8 ialpha = 255 - blendpx->s.alpha, balpha = blendpx->s.alpha;
			RGBA_t icolor = *sourcepx, bcolor;

			memset(&bcolor, 0x00, sizeof(RGBA_t));

			if (blendpx->s.alpha)
			{
				bcolor.s.blue = 0;
				bcolor.s.red = 255;
				bcolor.s.green = (blendpx->s.red + blendpx->s.green + blendpx->s.blue) / 3;
			}
			if (sourcepx->s.alpha && sourcepx->s.red > sourcepx->s.green << 1) // this is pretty arbitrary, but it works well for Metal Sonic
			{
				icolor.s.red = sourcepx->s.blue;
				icolor.s.blue = sourcepx->s.red;
			}
			*px = NearestColorSafe(
				(ialpha * icolor.s.red + balpha * bcolor.s.red)/255,
				(ialpha * icolor.s.green + balpha * bcolor.s.green)/255,
				(ialpha * icolor.s.blue + balpha * bcolor.s.blue)/255);
		}
	}
	else if (translation == TC_ALLWHITE)
	{
		// Turn everything white
		*px = NearestColorSafe(255, 255, 255);
		return true;
	}
	return false;
}

void RSP_CreateModelTexture(md2_t *model, INT32 tcnum, INT32 skincolor)
{
	rsp_modeltexture_t *texture = model->texture;
	rsp_modeltexture_t *blendtexture = model->blendtexture;
	rsp_texture_t *ttex;
	rsp_texture_t *ntex;
	size_t i, size = 0;

	// vanilla port
	UINT8 translation[16];
	memset(translation, 0, sizeof(translation));

	// get texture size
	if (texture)
		size = (texture->width * texture->height);

	// has skincolor but no blend texture?
	// don't try to make translated textures
	if (skincolor && ((!blendtexture) || (blendtexture && !blendtexture->data)))
		skincolor = 0;

	if (tcnum < 0)
		tcnum = -tcnum;
	ttex = &model->rsp_blendtex[skincolor][tcnum];
	ntex = &model->rsp_tex;

	// base texture
	if (!tcnum)
	{
		RGBA_t *image = texture->data;
		// doesn't exist?
		if (!image)
			return;

		ntex->width = texture->width;
		ntex->height = texture->height;

		if (ntex->data)
			Z_Free(ntex->data);
		ntex->data = Z_Calloc(size, PU_SOFTPOLY, NULL);

		for (i = 0; i < size; i++)
		{
			if (image[i].s.alpha < 1)
				ntex->data[i] = TRANSPARENTPIXEL;
			else
				ntex->data[i] = NearestColorSafe(image[i].s.red, image[i].s.green, image[i].s.blue);
		}
	}
	else
	{
		// create translations
		RGBA_t blendcolor;

		ttex->width = 1;
		ttex->height = 1;
		ttex->data = NULL;

		// doesn't exist?
		if (!texture->data)
			return;

		ttex->width = texture->width;
		ttex->height = texture->height;
		ttex->data = Z_Calloc(size, PU_SOFTPOLY, NULL);

		blendcolor = V_GetColor(0); // initialize
		if (skincolor != SKINCOLOR_NONE)
			memcpy(&translation, &Color_Index[skincolor - 1], 16);

		for (i = 0; i < size; i++)
		{
			RGBA_t *image = texture->data;
			RGBA_t *blendimage = blendtexture->data;

			if (image[i].s.alpha < 1)
				ttex->data[i] = TRANSPARENTPIXEL;
			else if (!BlendTranslations(&ttex->data[i], &image[i], &blendimage[i], tcnum))
			{
				UINT16 brightness;

				// Don't bother with blending the pixel if the alpha of the blend pixel is 0
				if (tcnum == TC_RAINBOW)
				{
					if (image[i].s.alpha == 0 && blendimage[i].s.alpha == 0)
					{
						ttex->data[i] = TRANSPARENTPIXEL;
						continue;
					}
					else
					{
						UINT16 imagebright, blendbright;
						SETBRIGHTNESS(imagebright,image[i].s.red,image[i].s.green,image[i].s.blue);
						SETBRIGHTNESS(blendbright,blendimage[i].s.red,blendimage[i].s.green,blendimage[i].s.blue);
						// slightly dumb average between the blend image color and base image colour, usually one or the other will be fully opaque anyway
						brightness = (imagebright*(255-blendimage[i].s.alpha))/255 + (blendbright*blendimage[i].s.alpha)/255;
					}
				}
				else
				{
					if (blendimage[i].s.alpha == 0)
					{
						ttex->data[i] = model->rsp_tex.data[i];
						continue;
					}
					else
					{
						SETBRIGHTNESS(brightness,blendimage[i].s.red,blendimage[i].s.green,blendimage[i].s.blue);
					}
				}

				// Calculate a sort of "gradient" for the skincolor
				// (Me splitting this into a function didn't work, so I had to ruin this entire function's groove...)
				{
					RGBA_t nextcolor;
					UINT8 firsti, secondi, mul;
					UINT32 r, g, b;

					// Rainbow needs to find the closest match to the textures themselves, instead of matching brightnesses to other colors.
					// Ensue horrible mess.
					if (tcnum == TC_RAINBOW)
					{
						UINT16 brightdif = 256;
						UINT8 colorbrightnesses[16];
						INT32 compare, m, d;
						UINT8 i;

						// Ignore pure white & pitch black
						if (brightness > 253 || brightness < 2)
						{
							ttex->data[i] = NearestColorSafe(image[i].s.red,image[i].s.green,image[i].s.blue);
							continue;
						}

						firsti = 0;
						mul = 0;

						for (i = 0; i < 16; i++)
						{
							RGBA_t tempc = V_GetColor(translation[i]);
							SETBRIGHTNESS(colorbrightnesses[i], tempc.s.red, tempc.s.green, tempc.s.blue); // store brightnesses for comparison
						}

						for (i = 0; i < 16; i++)
						{
							if (brightness > colorbrightnesses[i]) // don't allow greater matches (because calculating a makeshift gradient for this is already a huge mess as is)
								continue;
							compare = abs((INT16)(colorbrightnesses[i]) - (INT16)(brightness));
							if (compare < brightdif)
							{
								brightdif = (UINT16)compare;
								firsti = i; // best matching color that's equal brightness or darker
							}
						}

						secondi = firsti+1; // next color in line
						if (secondi == 16)
						{
							m = (INT16)brightness; // - 0;
							d = (INT16)colorbrightnesses[firsti]; // - 0;
						}
						else
						{
							m = (INT16)brightness - (INT16)colorbrightnesses[secondi];
							d = (INT16)colorbrightnesses[firsti] - (INT16)colorbrightnesses[secondi];
						}

						if (m >= d)
							m = d-1;

						// calculate the "gradient" multiplier based on how close this color is to the one next in line
						if (m <= 0 || d <= 0)
							mul = 0;
						else
							mul = 15 - ((m * 16) / d);
					}
					else
					{
						// Thankfully, it's normally way more simple.
						// Just convert brightness to a skincolor value, use remainder to find the gradient multipler
						firsti = ((UINT8)(255-brightness) / 16);
						secondi = firsti+1;
						mul = ((UINT8)(255-brightness) % 16);
					}

					blendcolor = V_GetColor(translation[firsti]);

					if (mul > 0 // If it's 0, then we only need the first color.
						&& translation[firsti] != translation[secondi]) // Some colors have duplicate colors in a row, so let's just save the process
					{
						if (secondi == 16) // blend to black
							nextcolor = V_GetColor(31);
						else
							nextcolor = V_GetColor(translation[secondi]);

						// Find difference between points
						r = (UINT32)(nextcolor.s.red - blendcolor.s.red);
						g = (UINT32)(nextcolor.s.green - blendcolor.s.green);
						b = (UINT32)(nextcolor.s.blue - blendcolor.s.blue);

						// Find the gradient of the two points
						r = ((mul * r) / 16);
						g = ((mul * g) / 16);
						b = ((mul * b) / 16);

						// Add gradient value to color
						blendcolor.s.red += r;
						blendcolor.s.green += g;
						blendcolor.s.blue += b;
					}
				}

				if (tcnum == TC_RAINBOW)
				{
					UINT32 tempcolor;
					UINT16 colorbright;
					UINT8 red, green, blue;

					SETBRIGHTNESS(colorbright,blendcolor.s.red,blendcolor.s.green,blendcolor.s.blue);
					if (colorbright == 0)
						colorbright = 1; // no dividing by 0 please

					tempcolor = (brightness * blendcolor.s.red) / colorbright;
					tempcolor = min(255, tempcolor);
					red = (UINT8)tempcolor;

					tempcolor = (brightness * blendcolor.s.green) / colorbright;
					tempcolor = min(255, tempcolor);
					green = (UINT8)tempcolor;

					tempcolor = (brightness * blendcolor.s.blue) / colorbright;
					tempcolor = min(255, tempcolor);
					blue = (UINT8)tempcolor;
					ttex->data[i] = NearestColorSafe(red, green, blue);
				}
				else
				{
					// Color strength depends on image alpha
					INT32 tempcolor;
					UINT8 red, green, blue;

					tempcolor = ((image[i].s.red * (255-blendimage[i].s.alpha)) / 255) + ((blendcolor.s.red * blendimage[i].s.alpha) / 255);
					tempcolor = min(255, tempcolor);
					red = (UINT8)tempcolor;

					tempcolor = ((image[i].s.green * (255-blendimage[i].s.alpha)) / 255) + ((blendcolor.s.green * blendimage[i].s.alpha) / 255);
					tempcolor = min(255, tempcolor);
					green = (UINT8)tempcolor;

					tempcolor = ((image[i].s.blue * (255-blendimage[i].s.alpha)) / 255) + ((blendcolor.s.blue * blendimage[i].s.alpha) / 255);
					tempcolor = min(255, tempcolor);
					blue = (UINT8)tempcolor;
					ttex->data[i] = NearestColorSafe(red, green, blue);
				}
			}
		}
	}
}

#undef SETBRIGHTNESS

void RSP_FreeModelTexture(md2_t *model)
{
	rsp_modeltexture_t *texture = model->texture;
	model->rsp_tex.width = 1;
	model->rsp_tex.height = 1;

	if (texture)
	{
		if (texture->data)
			Z_Free(texture->data);
		Z_Free(model->texture);
	}
	if (model->rsp_tex.data)
		Z_Free(model->rsp_tex.data);

	model->texture = NULL;
	model->rsp_tex.data = NULL;
}

void RSP_FreeModelBlendTexture(md2_t *model)
{
	rsp_modeltexture_t *blendtexture = model->blendtexture;
	if (model->blendtexture)
	{
		if (blendtexture->data)
			Z_Free(blendtexture->data);
		Z_Free(model->blendtexture);
	}
	model->blendtexture = NULL;
}
