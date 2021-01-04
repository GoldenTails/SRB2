// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  sw_things.h
/// \brief Rendering of moving objects, sprites

#ifndef __SW_THINGS__
#define __SW_THINGS__

#include "sw_plane.h"
#include "../r_patch.h"
#include "../r_picformats.h"
#include "../r_portal.h"
#include "../r_defs.h"
#include "../r_skins.h"

// ---------------------
// MASKED COLUMN DRAWING
// ---------------------

// vars for R_DrawMaskedColumn
extern INT16 *mfloorclip;
extern INT16 *mceilingclip;
extern fixed_t spryscale;
extern fixed_t sprtopscreen;
extern fixed_t sprbotscreen;
extern fixed_t windowtop;
extern fixed_t windowbottom;
extern INT32 lengthcol;

void SWR_DrawMaskedColumn(column_t *column);
void SWR_DrawFlippedMaskedColumn(column_t *column);

// ----------------
// SPRITE RENDERING
// ----------------

// Constant arrays used for psprite clipping
//  and initializing clipping.
extern INT16 negonearray[MAXVIDWIDTH];
extern INT16 screenheightarray[MAXVIDWIDTH];

//SoM: 6/5/2000: Light sprites correctly!
void SWR_AddSprites(sector_t *sec, INT32 lightlevel);
void SWR_InitSprites(void);
void SWR_ClearSprites(void);

// --------------
// MASKED DRAWING
// --------------
/** Used to count the amount of masked elements
 * per portal to later group them in separate
 * drawnode lists.
 */
typedef struct
{
	size_t drawsegs[2];
	size_t vissprites[2];
	fixed_t viewx, viewy, viewz;			/**< View z stored at the time of the BSP traversal for the view/portal. Masked sorting/drawing needs it. */
	sector_t* viewsector;
} maskcount_t;

void SWR_DrawMasked(maskcount_t* masks, UINT8 nummasks);

// ----------
// VISSPRITES
// ----------

// number of sprite lumps for spritewidth,offset,topoffset lookup tables
// Fab: this is a hack : should allocate the lookup tables per sprite
#define MAXVISSPRITES 2048 // added 2-2-98 was 128

#define VISSPRITECHUNKBITS 6	// 2^6 = 64 sprites per chunk
#define VISSPRITESPERCHUNK (1 << VISSPRITECHUNKBITS)
#define VISSPRITEINDEXMASK (VISSPRITESPERCHUNK - 1)

typedef enum
{
	// actual cuts
	SC_NONE       = 0,
	SC_TOP        = 1,
	SC_BOTTOM     = 1<<1,
	// other flags
	SC_PRECIP     = 1<<2,
	SC_LINKDRAW   = 1<<3,
	SC_FULLBRIGHT = 1<<4,
	SC_FULLDARK   = 1<<5,
	SC_VFLIP      = 1<<6,
	SC_ISSCALED   = 1<<7,
	SC_ISROTATED  = 1<<8,
	SC_SHADOW     = 1<<9,
	SC_SHEAR      = 1<<10,
	SC_SPLAT      = 1<<11,
	// masks
	SC_CUTMASK    = SC_TOP|SC_BOTTOM,
	SC_FLAGMASK   = ~SC_CUTMASK
} spritecut_e;

// A vissprite_t is a thing that will be drawn during a refresh,
// i.e. a sprite object that is partly visible.
typedef struct vissprite_s
{
	// Doubly linked list.
	struct vissprite_s *prev;
	struct vissprite_s *next;

	// Bonus linkdraw pointer.
	struct vissprite_s *linkdraw;

	mobj_t *mobj; // for easy access

	INT32 x1, x2;

	fixed_t gx, gy; // for line side calculation
	fixed_t gz, gzt; // global bottom/top for silhouette clipping
	fixed_t pz, pzt; // physical bottom/top for sorting with 3D floors

	fixed_t startfrac; // horizontal position of x1
	fixed_t xscale, scale; // projected horizontal and vertical scales
	fixed_t thingscale; // the object's scale
	fixed_t sortscale; // sortscale only differs from scale for paper sprites, floor sprites, and MF2_LINKDRAW
	fixed_t sortsplat; // the sortscale from behind the floor sprite
	fixed_t scalestep; // only for paper sprites, 0 otherwise
	fixed_t paperoffset, paperdistance; // for paper sprites, offset/dist relative to the angle
	fixed_t xiscale; // negative if flipped

	angle_t centerangle; // for paper sprites
	angle_t viewangle; // for floor sprites, the viewpoint's current angle

	struct {
		fixed_t tan; // The amount to shear the sprite vertically per row
		INT32 offset; // The center of the shearing location offset from x1
	} shear;

	fixed_t texturemid;
	patch_t *patch;

	lighttable_t *colormap; // for color translation and shadow draw
	                        // maxbright frames as well

	UINT8 *transmap; // for MF2_SHADOW sprites, which translucency table to use

	INT32 mobjflags;

	INT32 heightsec; // height sector for underwater/fake ceiling support

	extracolormap_t *extra_colormap; // global colormaps

	// Precalculated top and bottom screen coords for the sprite.
	fixed_t thingheight; // The actual height of the thing (for 3D floors)
	sector_t *sector; // The sector containing the thing.
	INT16 sz, szt;

	spritecut_e cut;
	UINT32 renderflags;
	UINT8 rotateflags;

	fixed_t spritexscale, spriteyscale;
	fixed_t spritexoffset, spriteyoffset;

	fixed_t shadowscale;

	INT16 clipbot[MAXVIDWIDTH], cliptop[MAXVIDWIDTH];

	INT32 dispoffset; // copy of info->dispoffset, affects ordering but not drawing
} vissprite_t;

extern UINT32 visspritecount;

void SWR_ClipSprites(drawseg_t* dsstart, portal_t* portal);
void SWR_ClipVisSprite(vissprite_t *spr, INT32 x1, INT32 x2, drawseg_t* dsstart, portal_t* portal);

boolean SWR_SpriteIsFlashing(vissprite_t *vis);
UINT8 *SWR_GetSpriteTranslation(vissprite_t *vis);

// ----------
// DRAW NODES
// ----------

// A drawnode is something that points to a 3D floor, 3D side, or masked
// middle texture. This is used for sorting with sprites.
typedef struct drawnode_s
{
	visplane_t *plane;
	drawseg_t *seg;
	drawseg_t *thickseg;
	ffloor_t *ffloor;
	vissprite_t *sprite;

	struct drawnode_s *next;
	struct drawnode_s *prev;
} drawnode_t;

void SWR_InitDrawNodes(void);

#endif //__SW_THINGS__
