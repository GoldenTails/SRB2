/*
	From the 'Wizard2' engine by Spaddlewit Inc. ( http://www.spaddlewit.com )
	An experimental work-in-progress.

	Donated to Sonic Team Junior and adapted to work with
	Sonic Robo Blast 2. The license of this code matches whatever
	the licensing is for Sonic Robo Blast 2.
*/

#include "doomdef.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_main.h"
#include "info.h"
#include "z_zone.h"
#include "r_things.h"
#include "r_model.h"
#include "r_md2load.h"
#include "r_md3load.h"
#include "u_list.h"
#include <string.h>

#ifndef errno
#include "errno.h"
#endif

#ifdef POLYRENDERER
#include "polyrenderer/r_softpoly.h"
#endif

#ifdef HWRENDER
#include "hardware/hw_glob.h"
#include "hardware/hw_drv.h"
#endif

md2_t md2_models[NUMSPRITES];
md2_t md2_playermodels[MAXSKINS];

static CV_PossibleValue_t modelinterpolation_cons_t[] = {{0, "Off"}, {1, "Sometimes"}, {2, "Always"}, {0, NULL}};
#ifdef POLYRENDERER
static CV_PossibleValue_t texturemapping_cons_t[] = {
	{TEXMAP_FIXED, "Fixed-Point"},
	{TEXMAP_FLOAT, "Floating-Point"},
	{0, NULL}};
static void CV_TextureMapping_OnChange(void);
#endif

consvar_t cv_models = {"models", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_modelinterpolation = {"modelinterpolation", "Sometimes", CV_SAVE, modelinterpolation_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

#ifdef POLYRENDERER
consvar_t cv_texturemapping = {"texturemapping", "Floating-Point", CV_SAVE|CV_CALL, texturemapping_cons_t, CV_TextureMapping_OnChange, 0, NULL, NULL, 0, 0, NULL};

static void CV_TextureMapping_OnChange(void)
{
	R_SetViewSize();
}
#endif

// Loads the model. That's it.
model_t *R_LoadModel(const char *filename)
{
	//Filename checking fixed ~Monster Iestyn and Golden
	return Model_Load(va("%s"PATHSEP"%s", srb2home, filename), PU_STATIC);
}

// Reload model stuff.
void R_ReloadModels(void)
{
	size_t i;
	INT32 s;

	for (s = 0; s < MAXSKINS; s++)
	{
		if (md2_playermodels[s].model)
			Model_LoadSprite2(md2_playermodels[s].model);
	}

	for (i = 0; i < NUMSPRITES; i++)
	{
		if (md2_models[i].model)
			Model_LoadInterpolationSettings(md2_models[i].model);
	}
}

// Don't spam the console, or the OS with fopen requests!
static boolean nomd2s = false;

void R_InitModels(void)
{
	size_t i;
	INT32 s;
	FILE *f;
	char name[18], filename[32];
	float scale, offset;

	CONS_Printf("R_InitModels()...\n");

	CV_RegisterVar(&cv_modelinterpolation);
	CV_RegisterVar(&cv_models);
	CV_RegisterVar(&cv_texturemapping);

	for (s = 0; s < MAXSKINS; s++)
	{
		md2_playermodels[s].scale = -1.0f;
		md2_playermodels[s].model = NULL;
		md2_playermodels[s].texture = NULL;
		md2_playermodels[s].skin = -1;
		md2_playermodels[s].notfound = true;
		md2_playermodels[s].error = false;
	}
	for (i = 0; i < NUMSPRITES; i++)
	{
		md2_models[i].scale = -1.0f;
		md2_models[i].model = NULL;
		md2_models[i].texture = NULL;
		md2_models[i].skin = -1;
		md2_models[i].notfound = true;
		md2_models[i].error = false;
	}

	// read the models.dat file
	//Filename checking fixed ~Monster Iestyn and Golden
	f = fopen(va("%s"PATHSEP"%s", srb2home, "models.dat"), "rt");

	if (!f)
	{
		CONS_Printf("%s %s\n", M_GetText("Error while loading models.dat:"), strerror(errno));
		nomd2s = true;
		return;
	}
	while (fscanf(f, "%19s %31s %f %f", name, filename, &scale, &offset) == 4)
	{
		if (stricmp(name, "PLAY") == 0)
		{
			CONS_Printf("Model for sprite PLAY detected in models.dat, use a player skin instead!\n");
			continue;
		}

		for (i = 0; i < NUMSPRITES; i++)
		{
			if (stricmp(name, sprnames[i]) == 0)
			{
				//if (stricmp(name, "PLAY") == 0)
					//continue;

				//CONS_Debug(DBG_RENDER, "  Found: %s %s %f %f\n", name, filename, scale, offset);
				md2_models[i].scale = scale;
				md2_models[i].offset = offset;
				md2_models[i].notfound = false;
				strcpy(md2_models[i].filename, filename);
				goto md2found;
			}
		}

		for (s = 0; s < MAXSKINS; s++)
		{
			if (stricmp(name, skins[s].name) == 0)
			{
				//CONS_Printf("  Found: %s %s %f %f\n", name, filename, scale, offset);
				md2_playermodels[s].skin = s;
				md2_playermodels[s].scale = scale;
				md2_playermodels[s].offset = offset;
				md2_playermodels[s].notfound = false;
				strcpy(md2_playermodels[s].filename, filename);
				goto md2found;
			}
		}
		// no sprite/player skin name found?!?
		//CONS_Printf("Unknown sprite/player skin %s detected in models.dat\n", name);
md2found:
		// move on to next line...
		continue;
	}
	fclose(f);
}

void R_AddPlayerModel(int skin) // For skins that were added after startup
{
	FILE *f;
	char name[18], filename[32];
	float scale, offset;

	if (nomd2s)
		return;

	//CONS_Printf("R_AddPlayerModel()...\n");

	// read the models.dat file
	//Filename checking fixed ~Monster Iestyn and Golden
	f = fopen(va("%s"PATHSEP"%s", srb2home, "models.dat"), "rt");

	if (!f)
	{
		CONS_Printf("Error while loading models.dat\n");
		nomd2s = true;
		return;
	}

	// Check for any model that match the names of player skins!
	while (fscanf(f, "%19s %31s %f %f", name, filename, &scale, &offset) == 4)
	{
		if (stricmp(name, skins[skin].name) == 0)
		{
			md2_playermodels[skin].skin = skin;
			md2_playermodels[skin].scale = scale;
			md2_playermodels[skin].offset = offset;
			md2_playermodels[skin].notfound = false;
			strcpy(md2_playermodels[skin].filename, filename);
			goto playermd2found;
		}
	}

	//CONS_Printf("Model for player skin %s not found\n", skins[skin].name);
	md2_playermodels[skin].notfound = true;
playermd2found:
	fclose(f);
}

void R_AddSpriteModel(size_t spritenum) // For sprites that were added after startup
{
	FILE *f;
	// name[18] is used to check for names in the models.dat file that match with sprites or player skins
	// sprite names are always 4 characters long, and names is for player skins can be up to 19 characters long
	char name[18], filename[32];
	float scale, offset;

	if (nomd2s)
		return;

	if (spritenum == SPR_PLAY) // Handled already NEWMD2: Per sprite, per-skin check
		return;

	// Read the models.dat file
	//Filename checking fixed ~Monster Iestyn and Golden
	f = fopen(va("%s"PATHSEP"%s", srb2home, "models.dat"), "rt");

	if (!f)
	{
		CONS_Printf("Error while loading models.dat\n");
		nomd2s = true;
		return;
	}

	// Check for any MD2s that match the names of sprite names!
	while (fscanf(f, "%19s %31s %f %f", name, filename, &scale, &offset) == 4)
	{
		if (stricmp(name, sprnames[spritenum]) == 0)
		{
			md2_models[spritenum].scale = scale;
			md2_models[spritenum].offset = offset;
			md2_models[spritenum].notfound = false;
			strcpy(md2_models[spritenum].filename, filename);
			goto spritemd2found;
		}
	}

	//CONS_Printf("MD2 for sprite %s not found\n", sprnames[spritenum]);
	md2_models[spritenum].notfound = true;
spritemd2found:
	fclose(f);
}

//
// Model_Load
// Load a model and convert it to the internal format.
//
model_t *Model_Load(const char *filename, int ztag)
{
	model_t *model;

	// What type of file?
	const char *extension = NULL;
	int i;
	for (i = (int)strlen(filename)-1; i >= 0; i--)
	{
		if (filename[i] != '.')
			continue;

		extension = &filename[i];
		break;
	}

	if (!extension)
	{
		CONS_Printf("Model %s is lacking a file extension, unable to determine type!\n", filename);
		return NULL;
	}

	if (!strcmp(extension, ".md3"))
	{
		if (!(model = MD3_LoadModel(filename, ztag, false)))
			return NULL;
	}
	else if (!strcmp(extension, ".md3s")) // MD3 that will be converted in memory to use full floats
	{
		if (!(model = MD3_LoadModel(filename, ztag, true)))
			return NULL;
	}
	else if (!strcmp(extension, ".md2"))
	{
		if (!(model = MD2_LoadModel(filename, ztag, false)))
			return NULL;
	}
	else if (!strcmp(extension, ".md2s"))
	{
		if (!(model = MD2_LoadModel(filename, ztag, true)))
			return NULL;
	}
	else
	{
		CONS_Printf("Unknown model format: %s\n", extension);
		return NULL;
	}

	model->mdlFilename = (char*)Z_Malloc(strlen(filename)+1, ztag, NULL);
	strcpy(model->mdlFilename, filename);

	Model_Optimize(model);
	Model_GeneratePolygonNormals(model, ztag);
	Model_LoadSprite2(model);
	if (!model->spr2frames)
		Model_LoadInterpolationSettings(model);

	// Default material properties
	for (i = 0 ; i < model->numMaterials; i++)
	{
		material_t *material = &model->materials[i];
		material->ambient[0] = 0.7686f;
		material->ambient[1] = 0.7686f;
		material->ambient[2] = 0.7686f;
		material->ambient[3] = 1.0f;
		material->diffuse[0] = 0.5863f;
		material->diffuse[1] = 0.5863f;
		material->diffuse[2] = 0.5863f;
		material->diffuse[3] = 1.0f;
		material->specular[0] = 0.4902f;
		material->specular[1] = 0.4902f;
		material->specular[2] = 0.4902f;
		material->specular[3] = 1.0f;
		material->shininess = 25.0f;
	}

	return model;
}

// Wouldn't it be great if C just had destructors?
void Model_Unload(model_t *model)
{
	int i;
	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (mesh->frames)
		{
			int j;
			for (j = 0; j < mesh->numFrames; j++)
			{
				if (mesh->frames[j].normals)
					Z_Free(mesh->frames[j].normals);

				if (mesh->frames[j].tangents)
					Z_Free(mesh->frames[j].tangents);

				if (mesh->frames[j].vertices)
					Z_Free(mesh->frames[j].vertices);

				if (mesh->frames[j].colors)
					Z_Free(mesh->frames[j].colors);
			}

			Z_Free(mesh->frames);
		}
		else if (mesh->tinyframes)
		{
			int j;
			for (j = 0; j < mesh->numFrames; j++)
			{
				if (mesh->tinyframes[j].normals)
					Z_Free(mesh->tinyframes[j].normals);

				if (mesh->tinyframes[j].tangents)
					Z_Free(mesh->tinyframes[j].tangents);

				if (mesh->tinyframes[j].vertices)
					Z_Free(mesh->tinyframes[j].vertices);
			}

			if (mesh->indices)
				Z_Free(mesh->indices);

			Z_Free(mesh->tinyframes);
		}

		if (mesh->uvs)
			Z_Free(mesh->uvs);

		if (mesh->lightuvs)
			Z_Free(mesh->lightuvs);
	}

	if (model->meshes)
		Z_Free(model->meshes);

	if (model->tags)
		Z_Free(model->tags);

	if (model->materials)
		Z_Free(model->materials);

	Model_DeleteVBOs(model);
	Z_Free(model);
}

// Returns a model, if available.
md2_t *Model_IsAvailable(spritenum_t spritenum, skin_t *skin)
{
	char filename[64];
	md2_t *md2;

	// invalid sprite number
	if ((unsigned)spritenum >= NUMSPRITES || (unsigned)spritenum == SPR_NULL)
		return NULL;

	if (skin && spritenum == SPR_PLAY) // Use the player model list if the mobj has a skin and is using the player sprites
	{
		md2 = &md2_playermodels[skin-skins];
		md2->skin = skin-skins;
	}
	else
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

	// Allocate texture data
	if (!md2->texture)
		md2->texture = Z_Calloc(sizeof(modeltexture_t), PU_STATIC, NULL);

#ifdef HWRENDER
	// Create mesh VBOs
	if (!md2->meshVBOs && (rendermode == render_opengl))
	{
		HWD.pfnCreateModelVBOs(md2->model);
		md2->meshVBOs = true;
	}
#endif

	return md2;
}

boolean Model_AllowRendering(mobj_t *mobj)
{
	// Signpost overlay. Not needed.
	if (mobj->state-states == S_PLAY_SIGN)
		return false;

	// Otherwise, render the model.
	return true;
}

boolean Model_CanInterpolate(mobj_t *mobj, model_t *model)
{
	if (cv_modelinterpolation.value == 2) // Always interpolate
		return true;
	return model->interpolate[(mobj->frame & FF_FRAMEMASK)];
}

boolean Model_CanInterpolateSprite2(modelspr2frames_t *spr2frame)
{
	if (cv_modelinterpolation.value == 2) // Always interpolate
		return true;
	return spr2frame->interpolate;
}

//
// Model_GetSprite2 (see P_GetSkinSprite2)
// For non-super players, tries each sprite2's immediate predecessor until it finds one with a number of frames or ends up at standing.
// For super players, does the same as above - but tries the super equivalent for each sprite2 before the non-super version.
//
UINT8 Model_GetSprite2(md2_t *md2, skin_t *skin, UINT8 spr2, player_t *player)
{
	UINT8 super = 0, i = 0;

	if (!md2 || !md2->model || !md2->model->spr2frames || !skin)
		return 0;

	if ((playersprite_t)(spr2 & ~FF_SPR2SUPER) >= free_spr2)
		return 0;

	while (!md2->model->spr2frames[spr2].numframes
		&& spr2 != SPR2_STND
		&& ++i != 32) // recursion limiter
	{
		if (spr2 & FF_SPR2SUPER)
		{
			super = FF_SPR2SUPER;
			spr2 &= ~FF_SPR2SUPER;
			continue;
		}

		switch(spr2)
		{
		// Normal special cases.
		case SPR2_JUMP:
			spr2 = ((player
					? player->charflags
					: skin->flags)
					& SF_NOJUMPSPIN) ? SPR2_SPNG : SPR2_ROLL;
			break;
		case SPR2_TIRE:
			spr2 = ((player
					? player->charability
					: skin->ability)
					== CA_SWIM) ? SPR2_SWIM : SPR2_FLY;
			break;
		// Use the handy list, that's what it's there for!
		default:
			spr2 = spr2defaults[spr2];
			break;
		}

		spr2 |= super;
	}

	if (i >= 32) // probably an infinite loop...
		return 0;

	return spr2;
}

tag_t *Model_GetTagByName(model_t *model, char *name, int frame)
{
	if (frame < model->maxNumFrames)
	{
		tag_t *iterator = &model->tags[frame * model->numTags];

		int i;
		for (i = 0; i < model->numTags; i++)
		{
			if (!stricmp(iterator[i].name, name))
				return &iterator[i];
		}
	}

	return NULL;
}

void Model_LoadInterpolationSettings(model_t *model)
{
	INT32 i;
	INT32 numframes = model->meshes[0].numFrames;
	char *framename = model->framenames;

	if (!framename)
		return;

	#define GET_OFFSET \
		memcpy(&interpolation_flag, framename + offset, 2); \
		model->interpolate[i] = (!memcmp(interpolation_flag, MODEL_INTERPOLATION_FLAG, 2));

	for (i = 0; i < numframes; i++)
	{
		int offset = (strlen(framename) - 4);
		char interpolation_flag[3];
		memset(&interpolation_flag, 0x00, 3);

		// find the +i on the frame name
		// ANIM+i00
		// so the offset is (frame name length - 4)
		GET_OFFSET;

		// maybe the frame had three digits?
		// ANIM+i000
		// so the offset is (frame name length - 5)
		if (!model->interpolate[i])
		{
			offset--;
			GET_OFFSET;
		}

		framename += 16;
	}

	#undef GET_OFFSET
}

void Model_LoadSprite2(model_t *model)
{
	INT32 i;
	modelspr2frames_t *spr2frames = NULL;
	INT32 numframes = model->meshes[0].numFrames;
	char *framename = model->framenames;

	if (!framename)
		return;

	for (i = 0; i < numframes; i++)
	{
		char prefix[6];
		char name[5];
		char interpolation_flag[3];
		char framechars[4];
		UINT8 frame = 0;
		UINT8 spr2idx;
		boolean interpolate = false;

		memset(&prefix, 0x00, 6);
		memset(&name, 0x00, 5);
		memset(&interpolation_flag, 0x00, 3);
		memset(&framechars, 0x00, 4);

		if (strlen(framename) >= 9)
		{
			boolean super;
			char *modelframename = framename;
			memcpy(&prefix, modelframename, 5);
			modelframename += 5;
			memcpy(&name, modelframename, 4);
			modelframename += 4;
			// Oh look
			memcpy(&interpolation_flag, modelframename, 2);
			if (!memcmp(interpolation_flag, MODEL_INTERPOLATION_FLAG, 2))
			{
				interpolate = true;
				modelframename += 2;
			}
			memcpy(&framechars, modelframename, 3);

			if ((super = (!memcmp(prefix, "SUPER", 5))) || (!memcmp(prefix, "SPR2_", 5)))
			{
				spr2idx = 0;
				while (spr2idx < free_spr2)
				{
					if (!memcmp(spr2names[spr2idx], name, 4))
					{
						if (!spr2frames)
							spr2frames = (modelspr2frames_t*)Z_Calloc(sizeof(modelspr2frames_t)*NUMPLAYERSPRITES*2, PU_STATIC, NULL);
						if (super)
							spr2idx |= FF_SPR2SUPER;
						if (framechars[0])
						{
							frame = atoi(framechars);
							if (spr2frames[spr2idx].numframes < frame+1)
								spr2frames[spr2idx].numframes = frame+1;
						}
						else
						{
							frame = spr2frames[spr2idx].numframes;
							spr2frames[spr2idx].numframes++;
						}
						spr2frames[spr2idx].frames[frame] = i;
						spr2frames[spr2idx].interpolate = interpolate;
						break;
					}
					spr2idx++;
				}
			}
		}

		framename += 16;
	}

	if (model->spr2frames)
		Z_Free(model->spr2frames);
	model->spr2frames = spr2frames;
}

//
// GenerateVertexNormals
//
// Creates a new normal for a vertex using the average of all of the polygons it belongs to.
//
void Model_GenerateVertexNormals(model_t *model)
{
	int i;
	for (i = 0; i < model->numMeshes; i++)
	{
		int j;

		mesh_t *mesh = &model->meshes[i];

		if (!mesh->frames)
			continue;

		for (j = 0; j < mesh->numFrames; j++)
		{
			mdlframe_t *frame = &mesh->frames[j];
			int memTag = PU_STATIC;
			float *newNormals = (float*)Z_Malloc(sizeof(float)*3*mesh->numTriangles*3, memTag, 0);
			int k;
			float *vertPtr = frame->vertices;
			float *oldNormals;

			M_Memcpy(newNormals, frame->normals, sizeof(float)*3*mesh->numTriangles*3);

/*			if (!systemSucks)
			{
				memTag = Z_GetTag(frame->tangents);
				float *newTangents = (float*)Z_Malloc(sizeof(float)*3*mesh->numTriangles*3, memTag);
				M_Memcpy(newTangents, frame->tangents, sizeof(float)*3*mesh->numTriangles*3);
			}*/

			for (k = 0; k < mesh->numVertices; k++)
			{
				float x, y, z;
				int vCount = 0;
				vector_t normal;
				int l;
				float *testPtr = frame->vertices;

				x = *vertPtr++;
				y = *vertPtr++;
				z = *vertPtr++;

				normal.x = normal.y = normal.z = 0;

				for (l = 0; l < mesh->numVertices; l++)
				{
					float testX, testY, testZ;
					testX = *testPtr++;
					testY = *testPtr++;
					testZ = *testPtr++;

					if (fabsf(x - testX) > FLT_EPSILON
						|| fabsf(y - testY) > FLT_EPSILON
						|| fabsf(z - testZ) > FLT_EPSILON)
						continue;

					// Found a vertex match! Add it...
					normal.x += frame->normals[3 * l + 0];
					normal.y += frame->normals[3 * l + 1];
					normal.z += frame->normals[3 * l + 2];
					vCount++;
				}

				if (vCount > 1)
				{
//					Vector::Normalize(&normal);
					newNormals[3 * k + 0] = (float)normal.x;
					newNormals[3 * k + 1] = (float)normal.y;
					newNormals[3 * k + 2] = (float)normal.z;

/*					if (!systemSucks)
					{
						Vector::vector_t tangent;
						Vector::Tangent(&normal, &tangent);
						newTangents[3 * k + 0] = tangent.x;
						newTangents[3 * k + 1] = tangent.y;
						newTangents[3 * k + 2] = tangent.z;
					}*/
				}
			}

			oldNormals = frame->normals;
			frame->normals = newNormals;
			Z_Free(oldNormals);

/*			if (!systemSucks)
			{
				float *oldTangents = frame->tangents;
				frame->tangents = newTangents;
				Z_Free(oldTangents);
			}*/
		}
	}
}

typedef struct materiallist_s
{
	struct materiallist_s *next;
	struct materiallist_s *prev;
	material_t *material;
} materiallist_t;

static boolean AddMaterialToList(materiallist_t **head, material_t *material)
{
	materiallist_t *node, *newMatNode;
	for (node = *head; node; node = node->next)
	{
		if (node->material == material)
			return false;
	}

	// Didn't find it, so add to the list
	newMatNode = (materiallist_t*)Z_Malloc(sizeof(materiallist_t), PU_CACHE, 0);
	newMatNode->material = material;
	ListAdd(newMatNode, (listitem_t**)head);
	return true;
}

//
// Model_Optimize
//
// Groups triangles from meshes in the model
// Only works for models with 1 frame
//
void Model_Optimize(model_t *model)
{
	int numMeshes = 0;
	int i;
	materiallist_t *matListHead = NULL;
	int memTag;
	mesh_t *newMeshes;
	materiallist_t *node;

	if (model->numMeshes <= 1)
		return; // No need

	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *curMesh = &model->meshes[i];

		if (curMesh->numFrames > 1)
			return; // Can't optimize models with > 1 frame

		if (!curMesh->frames)
			return; // Don't optimize tinyframe models (no need)

		// We are condensing to 1 mesh per material, so
		// the # of materials we use will be the new
		// # of meshes
		if (AddMaterialToList(&matListHead, curMesh->frames[0].material))
			numMeshes++;
	}

	memTag = PU_STATIC;
	newMeshes = (mesh_t*)Z_Calloc(sizeof(mesh_t) * numMeshes, memTag, 0);

	i = 0;
	for (node = matListHead; node; node = node->next)
	{
		material_t *curMat = node->material;
		mesh_t *newMesh = &newMeshes[i];
		mdlframe_t *curFrame;
		int uvCount;
		int vertCount;
		int colorCount;

		// Find all triangles with this material and count them
		int numTriangles = 0;
		int j;
		for (j = 0; j < model->numMeshes; j++)
		{
			mesh_t *curMesh = &model->meshes[j];

			if (curMesh->frames[0].material == curMat)
				numTriangles += curMesh->numTriangles;
		}

		newMesh->numFrames = 1;
		newMesh->numTriangles = numTriangles;
		newMesh->numVertices = numTriangles * 3;
		newMesh->uvs = (float*)Z_Malloc(sizeof(float)*2*numTriangles*3, memTag, 0);
//		if (node->material->lightmap)
//			newMesh->lightuvs = (float*)Z_Malloc(sizeof(float)*2*numTriangles*3, memTag, 0);
		newMesh->frames = (mdlframe_t*)Z_Calloc(sizeof(mdlframe_t), memTag, 0);
		curFrame = &newMesh->frames[0];

		curFrame->material = curMat;
		curFrame->normals = (float*)Z_Malloc(sizeof(float)*3*numTriangles*3, memTag, 0);
//		if (!systemSucks)
//			curFrame->tangents = (float*)Z_Malloc(sizeof(float)*3*numTriangles*3, memTag, 0);
		curFrame->vertices = (float*)Z_Malloc(sizeof(float)*3*numTriangles*3, memTag, 0);
		curFrame->colors = (char*)Z_Malloc(sizeof(char)*4*numTriangles*3, memTag, 0);

		// Now traverse the meshes of the model, adding in
		// vertices/normals/uvs that match the current material
		uvCount = 0;
		vertCount = 0;
		colorCount = 0;
		for (j = 0; j < model->numMeshes; j++)
		{
			mesh_t *curMesh = &model->meshes[j];

			if (curMesh->frames[0].material == curMat)
			{
				float *dest;
				float *src;
				char *destByte;
				char *srcByte;

				M_Memcpy(&newMesh->uvs[uvCount],
					curMesh->uvs,
					sizeof(float)*2*curMesh->numTriangles*3);

/*				if (node->material->lightmap)
				{
					M_Memcpy(&newMesh->lightuvs[uvCount],
						curMesh->lightuvs,
						sizeof(float)*2*curMesh->numTriangles*3);
				}*/
				uvCount += 2*curMesh->numTriangles*3;

				dest = (float*)newMesh->frames[0].vertices;
				src = (float*)curMesh->frames[0].vertices;
				M_Memcpy(&dest[vertCount],
					src,
					sizeof(float)*3*curMesh->numTriangles*3);

				dest = (float*)newMesh->frames[0].normals;
				src = (float*)curMesh->frames[0].normals;
				M_Memcpy(&dest[vertCount],
					src,
					sizeof(float)*3*curMesh->numTriangles*3);

/*				if (!systemSucks)
				{
					dest = (float*)newMesh->frames[0].tangents;
					src = (float*)curMesh->frames[0].tangents;
					M_Memcpy(&dest[vertCount],
						src,
						sizeof(float)*3*curMesh->numTriangles*3);
				}*/

				vertCount += 3 * curMesh->numTriangles * 3;

				destByte = (char*)newMesh->frames[0].colors;
				srcByte = (char*)curMesh->frames[0].colors;

				if (srcByte)
				{
					M_Memcpy(&destByte[colorCount],
						srcByte,
						sizeof(char)*4*curMesh->numTriangles*3);
				}
				else
				{
					memset(&destByte[colorCount],
						255,
						sizeof(char)*4*curMesh->numTriangles*3);
				}

				colorCount += 4 * curMesh->numTriangles * 3;
			}
		}

		i++;
	}

	//CONS_Printf("Model::Optimize(): Model reduced from %d to %d meshes.\n", model->numMeshes, numMeshes);
	model->meshes = newMeshes;
	model->numMeshes = numMeshes;
}

void Model_GeneratePolygonNormals(model_t *model, int ztag)
{
	int i;
	for (i = 0; i < model->numMeshes; i++)
	{
		int j;
		mesh_t *mesh = &model->meshes[i];

		if (!mesh->frames)
			continue;

		for (j = 0; j < mesh->numFrames; j++)
		{
			int k;
			mdlframe_t *frame = &mesh->frames[j];
			const float *vertices = frame->vertices;
			vector_t *polyNormals;

			frame->polyNormals = (vector_t*)Z_Malloc(sizeof(vector_t) * mesh->numTriangles, ztag, 0);

			polyNormals = frame->polyNormals;

			for (k = 0; k < mesh->numTriangles; k++)
			{
//				Vector::Normal(vertices, polyNormals);
				vertices += 3 * 3;
				polyNormals++;
			}
		}
	}
}

void Model_DeleteVBOs(model_t *model)
{
	(void)model;
/*	for (int i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (mesh->frames)
		{
			for (int j = 0; j < mesh->numFrames; j++)
			{
				mdlframe_t *frame = &mesh->frames[j];
				if (!frame->vboID)
					continue;
				bglDeleteBuffers(1, &frame->vboID);
				frame->vboID = 0;
			}
		}
		else if (mesh->tinyframes)
		{
			for (int j = 0; j < mesh->numFrames; j++)
			{
				tinyframe_t *frame = &mesh->tinyframes[j];
				if (!frame->vboID)
					continue;
				bglDeleteBuffers(1, &frame->vboID);
				frame->vboID = 0;
			}
		}
	}*/
}
