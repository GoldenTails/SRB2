/*
	From the 'Wizard2' engine by Spaddlewit Inc. ( http://www.spaddlewit.com )
	An experimental work-in-progress.

	Donated to Sonic Team Junior and adapted to work with
	Sonic Robo Blast 2. The license of this code matches whatever
	the licensing is for Sonic Robo Blast 2.
*/

#ifndef _R_MODEL_H_
#define _R_MODEL_H_

#include "doomdata.h"
#include "doomtype.h"
#include "doomstat.h"
#include "r_things.h"

extern consvar_t cv_models;
extern consvar_t cv_modelinterpolation;

typedef struct
{
	float x, y, z;
} vector_t;

typedef struct
{
	float ambient[4], diffuse[4], specular[4], emissive[4];
	float shininess;
	boolean spheremap;
//	Texture::texture_t *texture;
//	Texture::texture_t *lightmap;
} material_t;

typedef struct
{
	material_t *material; // Pointer to the allocated 'materials' list in model_t
	float *vertices;
	float *normals;
	float *tangents;
	char *colors;
	unsigned int vboID;
	vector_t *polyNormals;
} mdlframe_t;

typedef struct
{
	material_t *material;
	short *vertices;
	char *normals;
	char *tangents;
	unsigned int vboID;
} tinyframe_t;

// Equivalent to MD3's many 'surfaces'
typedef struct mesh_s
{
	int numVertices;
	int numTriangles;

	float *uvs;
	float *lightuvs;

	int numFrames;
	mdlframe_t *frames;
	tinyframe_t *tinyframes;
	unsigned short *indices;
} mesh_t;

typedef struct tag_s
{
	char name[64];
//	matrix_t transform;
} tag_t;

#define MODEL_INTERPOLATION_FLAG "+i"

typedef struct
{
	INT32 frames[256];
	UINT8 numframes;
	boolean interpolate;
} modelspr2frames_t;

typedef struct model_s
{
	int maxNumFrames;

	int numMaterials;
	material_t *materials;
	int numMeshes;
	mesh_t *meshes;
	int numTags;
	tag_t *tags;

	char *mdlFilename;
	boolean unloaded;

	char *framenames;
	boolean interpolate[256];
	modelspr2frames_t *spr2frames;
} model_t;

typedef struct
{
	char        filename[32];
	float       scale;
	float       offset;
	model_t     *model;
#ifdef HWRENDER
	void        *grpatch;
	void        *blendgrpatch;
#endif
	boolean     notfound;
	INT32       skin;
	boolean     error;
} md2_t;

extern md2_t md2_models[NUMSPRITES];
extern md2_t md2_playermodels[MAXSKINS];

// Model initialization
void R_InitModels(void);
void R_ReloadModels(void);
void R_AddPlayerModel(INT32 skin);
void R_AddSpriteModel(size_t spritenum);
model_t *R_LoadModel(const char *filename);

// Model loading and unloading
model_t *Model_Load(const char *filename, int ztag);
void Model_Unload(model_t *model);

// Model rendering
boolean Model_AllowRendering(mobj_t *mobj);
boolean Model_CanInterpolate(mobj_t *mobj, model_t *model);
boolean Model_CanInterpolateSprite2(modelspr2frames_t *spr2frame);
UINT8 Model_GetSprite2(md2_t *md2, skin_t *skin, UINT8 spr2, player_t *player);

// Miscellaneous stuff
void Model_Optimize(model_t *model);
void Model_LoadInterpolationSettings(model_t *model);
void Model_LoadSprite2(model_t *model);
void Model_GenerateVertexNormals(model_t *model);
void Model_GeneratePolygonNormals(model_t *model, int ztag);
void Model_CreateVBOTiny(mesh_t *mesh, tinyframe_t *frame);
void Model_CreateVBO(mesh_t *mesh, mdlframe_t *frame);
void Model_DeleteVBOs(model_t *model);
tag_t *Model_GetTagByName(model_t *model, char *name, int frame);

#endif
