// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file m_perfstats.c
/// \brief Performance measurement tools.

#include "m_perfstats.h"
#include "v_video.h"
#include "i_video.h"
#include "d_netcmd.h"
#include "r_main.h"
#include "i_system.h"
#include "z_zone.h"
#include "p_local.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

int ps_tictime = 0;

int ps_playerthink_time = 0;
int ps_thinkertime = 0;

int ps_thlist_times[NUM_THINKERLISTS];
static const char* thlist_names[] = {
	"Polyobjects:     %d",
	"Main:            %d",
	"Mobjs:           %d",
	"Dynamic slopes:  %d",
	"Precipitation:   %d",
	NULL
};
static const char* thlist_shortnames[] = {
	"plyobjs %d",
	"main    %d",
	"mobjs   %d",
	"dynslop %d",
	"precip  %d",
	NULL
};

int ps_checkposition_calls = 0;

int ps_lua_thinkframe_time = 0;
int ps_lua_mobjhooks = 0;

// dynamically allocated resizeable array for thinkframe hook stats
ps_hookinfo_t *thinkframe_hooks = NULL;
int thinkframe_hooks_length = 0;
int thinkframe_hooks_capacity = 16;

void PS_SetThinkFrameHookInfo(int index, UINT32 time_taken, char* short_src)
{
	if (!thinkframe_hooks)
	{
		// array needs to be initialized
		thinkframe_hooks = Z_Malloc(sizeof(ps_hookinfo_t) * thinkframe_hooks_capacity, PU_STATIC, NULL);
	}
	if (index >= thinkframe_hooks_capacity)
	{
		// array needs more space, realloc with double size
		thinkframe_hooks_capacity *= 2;
		thinkframe_hooks = Z_Realloc(thinkframe_hooks,
			sizeof(ps_hookinfo_t) * thinkframe_hooks_capacity, PU_STATIC, NULL);
	}
	thinkframe_hooks[index].time_taken = time_taken;
	memcpy(thinkframe_hooks[index].short_src, short_src, LUA_IDSIZE * sizeof(char));
	// since the values are set sequentially from begin to end, the last call should leave
	// the correct value to this variable
	thinkframe_hooks_length = index + 1;
}

#define DRAWSTATS(DRAWER, x, y, videoflags, ...) {\
	if ((cursize = snprintf(NULL, 0, __VA_ARGS__)) > size)\
	{\
		s = Z_Realloc(s, cursize + 1, PU_STATIC, NULL);\
		size = cursize;\
	}\
	snprintf(s, size, __VA_ARGS__);\
	DRAWER(x, y, videoflags, s);\
}

#define DRAWSMALLSTATS(x, y, videoflags, ...) DRAWSTATS(V_DrawSmallString, x, y, V_MONOSPACE | V_ALLOWLOWERCASE | videoflags, __VA_ARGS__)
#define DRAWTHINSTATS(x, y, videoflags, ...) DRAWSTATS(V_DrawThinString, x, y, V_MONOSPACE | videoflags, __VA_ARGS__)

static void M_DrawRenderStats(void)
{
	size_t cursize = 99, size = cursize;
	char *s = Z_Malloc(size + 1, PU_STATIC, NULL);

	int currenttime = I_GetTimeMicros();
	int frametime = currenttime - ps_prevframetime;
	ps_prevframetime = currenttime;

	if (vid.width < 640 || vid.height < 400) // low resolution
	{
		DRAWTHINSTATS(20, 10, V_YELLOWMAP, "frmtime %d", frametime);
		if (!(gamestate == GS_LEVEL || (gamestate == GS_TITLESCREEN && titlemapinaction)))
		{
			DRAWTHINSTATS(20, 18, V_YELLOWMAP, "ui      %d", ps_uitime);
			DRAWTHINSTATS(20, 26, V_YELLOWMAP, "finupdt %d", ps_swaptime);
			DRAWTHINSTATS(20, 38, V_GRAYMAP, "logic   %d", ps_tictime);
			return;
		}
		DRAWTHINSTATS(20, 18, V_YELLOWMAP, "drwtime %d", ps_rendercalltime);
		DRAWTHINSTATS(90, 10, V_BLUEMAP, "bspcall %d", ps_numbspcalls);
		DRAWTHINSTATS(90, 18, V_BLUEMAP, "sprites %d", ps_numsprites);
		DRAWTHINSTATS(90, 26, V_BLUEMAP, "drwnode %d", ps_numdrawnodes);
		DRAWTHINSTATS(90, 34, V_BLUEMAP, "plyobjs %d", ps_numpolyobjects);
#ifdef HWRENDER
		if (rendermode == render_opengl) // OpenGL specific stats
		{
			DRAWTHINSTATS(24, 26, V_YELLOWMAP, "skybox  %d", ps_hw_skyboxtime);
			DRAWTHINSTATS(24, 34, V_YELLOWMAP, "bsptime %d", ps_bsptime);
			DRAWTHINSTATS(24, 42, V_YELLOWMAP, "nodesrt %d", ps_hw_nodesorttime);
			DRAWTHINSTATS(24, 50, V_YELLOWMAP, "nodedrw %d", ps_hw_nodedrawtime);
			DRAWTHINSTATS(24, 58, V_YELLOWMAP, "sprsort %d", ps_hw_spritesorttime);
			DRAWTHINSTATS(24, 66, V_YELLOWMAP, "sprdraw %d", ps_hw_spritedrawtime);
			DRAWTHINSTATS(24, 74, V_YELLOWMAP, "other   %d",
				ps_rendercalltime - ps_hw_skyboxtime - ps_bsptime - ps_hw_nodesorttime
				- ps_hw_nodedrawtime - ps_hw_spritesorttime - ps_hw_spritedrawtime
				- ps_hw_batchsorttime - ps_hw_batchdrawtime);
			DRAWTHINSTATS(20, 82, V_YELLOWMAP, "ui      %d", ps_uitime);
			DRAWTHINSTATS(20, 90, V_YELLOWMAP, "finupdt %d", ps_swaptime);
			DRAWTHINSTATS(20, 102, V_GRAYMAP, "logic   %d", ps_tictime);
			if (cv_glbatching.value)
			{
				DRAWTHINSTATS(90, 46, V_REDMAP, "batsort %d", ps_hw_batchsorttime);
				DRAWTHINSTATS(90, 54, V_REDMAP, "batdraw %d", ps_hw_batchdrawtime);

				DRAWTHINSTATS(155, 10, V_PURPLEMAP, "polygon %d", ps_hw_numpolys);
				DRAWTHINSTATS(155, 18, V_PURPLEMAP, "drwcall %d", ps_hw_numcalls);
				DRAWTHINSTATS(155, 26, V_PURPLEMAP, "shaders %d", ps_hw_numshaders);
				DRAWTHINSTATS(155, 34, V_PURPLEMAP, "vertex  %d", ps_hw_numverts);
				DRAWTHINSTATS(220, 10, V_PURPLEMAP, "texture %d", ps_hw_numtextures);
				DRAWTHINSTATS(220, 18, V_PURPLEMAP, "polyflg %d", ps_hw_numpolyflags);
				DRAWTHINSTATS(220, 26, V_PURPLEMAP, "colors  %d", ps_hw_numcolors);
			}
			else
			{
				// reset these vars so the "other" measurement isn't off
				ps_hw_batchsorttime = 0;
				ps_hw_batchdrawtime = 0;
			}
		}
		else // software specific stats
#endif
		{
			DRAWTHINSTATS(24, 26, V_YELLOWMAP, "bsptime %d", ps_bsptime);
			DRAWTHINSTATS(24, 34, V_YELLOWMAP, "sprclip %d", ps_sw_spritecliptime);
			DRAWTHINSTATS(24, 42, V_YELLOWMAP, "portals %d", ps_sw_portaltime);
			DRAWTHINSTATS(24, 50, V_YELLOWMAP, "planes  %d", ps_sw_planetime);
			DRAWTHINSTATS(24, 58, V_YELLOWMAP, "masked  %d", ps_sw_maskedtime);
			DRAWTHINSTATS(24, 66, V_YELLOWMAP, "other   %d",
				ps_rendercalltime - ps_bsptime - ps_sw_spritecliptime
				- ps_sw_portaltime - ps_sw_planetime - ps_sw_maskedtime);
			DRAWTHINSTATS(20, 74, V_YELLOWMAP, "ui      %d", ps_uitime);
			DRAWTHINSTATS(20, 82, V_YELLOWMAP, "finupdt %d", ps_swaptime);
			DRAWTHINSTATS(20, 94, V_GRAYMAP, "logic   %d", ps_tictime);
		}
	}
	else // high resolution
	{
		DRAWSMALLSTATS(20, 10, V_YELLOWMAP, "Frame time:     %d", frametime);
		DRAWSMALLSTATS(20, 10, V_YELLOWMAP, "Frame time:     %d", frametime);
		if (!(gamestate == GS_LEVEL || (gamestate == GS_TITLESCREEN && titlemapinaction)))
		{
			DRAWSMALLSTATS(20, 15, V_YELLOWMAP, "UI render:      %d", ps_uitime);
			DRAWSMALLSTATS(20, 20, V_YELLOWMAP, "I_FinishUpdate: %d", ps_swaptime);
			DRAWSMALLSTATS(20, 30, V_GRAYMAP, "Game logic:     %d", ps_tictime);
			return;
		}
		DRAWSMALLSTATS(20, 15, V_YELLOWMAP, "3d rendering:   %d", ps_rendercalltime);
		DRAWSMALLSTATS(115, 10, V_BLUEMAP, "BSP calls:    %d", ps_numbspcalls);
		DRAWSMALLSTATS(115, 15, V_BLUEMAP, "Sprites:      %d", ps_numsprites);
		DRAWSMALLSTATS(115, 20, V_BLUEMAP, "Drawnodes:    %d", ps_numdrawnodes);
		DRAWSMALLSTATS(115, 25, V_BLUEMAP, "Polyobjects:  %d", ps_numpolyobjects);
#ifdef HWRENDER
		if (rendermode == render_opengl) // OpenGL specific stats
		{
			DRAWSMALLSTATS(24, 20, V_YELLOWMAP, "Skybox render:  %d", ps_hw_skyboxtime);
			DRAWSMALLSTATS(24, 25, V_YELLOWMAP, "RenderBSPNode:  %d", ps_bsptime);
			DRAWSMALLSTATS(24, 30, V_YELLOWMAP, "Drwnode sort:   %d", ps_hw_nodesorttime);
			DRAWSMALLSTATS(24, 35, V_YELLOWMAP, "Drwnode render: %d", ps_hw_nodedrawtime);
			DRAWSMALLSTATS(24, 40, V_YELLOWMAP, "Sprite sort:    %d", ps_hw_spritesorttime);
			DRAWSMALLSTATS(24, 45, V_YELLOWMAP, "Sprite render:  %d", ps_hw_spritedrawtime);
			// Remember to update this calculation when adding more 3d rendering stats!
			DRAWSMALLSTATS(24, 50, V_YELLOWMAP, "Other:          %d",
				ps_rendercalltime - ps_hw_skyboxtime - ps_bsptime - ps_hw_nodesorttime
				- ps_hw_nodedrawtime - ps_hw_spritesorttime - ps_hw_spritedrawtime
				- ps_hw_batchsorttime - ps_hw_batchdrawtime);
			DRAWSMALLSTATS(20, 55, V_YELLOWMAP, "UI render:      %d", ps_uitime);
			DRAWSMALLSTATS(20, 60, V_YELLOWMAP, "I_FinishUpdate: %d", ps_swaptime);
			DRAWSMALLSTATS(20, 70, V_GRAYMAP, "Game logic:     %d", ps_tictime);
			if (cv_glbatching.value)
			{
				DRAWSMALLSTATS(115, 35, V_REDMAP, "Batch sort:   %d", ps_hw_batchsorttime);
				DRAWSMALLSTATS(115, 40, V_REDMAP, "Batch render: %d", ps_hw_batchdrawtime);

				DRAWSMALLSTATS(200, 10, V_PURPLEMAP, "Polygons:   %d", ps_hw_numpolys);
				DRAWSMALLSTATS(200, 15, V_PURPLEMAP, "Vertices:   %d", ps_hw_numverts);
				DRAWSMALLSTATS(200, 25, V_PURPLEMAP, "Draw calls: %d", ps_hw_numcalls);
				DRAWSMALLSTATS(200, 30, V_PURPLEMAP, "Shaders:    %d", ps_hw_numshaders);
				DRAWSMALLSTATS(200, 35, V_PURPLEMAP, "Textures:   %d", ps_hw_numtextures);
				DRAWSMALLSTATS(200, 40, V_PURPLEMAP, "Polyflags:  %d", ps_hw_numpolyflags);
				DRAWSMALLSTATS(200, 45, V_PURPLEMAP, "Colors:     %d", ps_hw_numcolors);
			}
			else
			{
				// reset these vars so the "other" measurement isn't off
				ps_hw_batchsorttime = 0;
				ps_hw_batchdrawtime = 0;
			}
		}
		else // software specific stats
#endif
		{
			DRAWSMALLSTATS(24, 20, V_YELLOWMAP, "RenderBSPNode:  %d", ps_bsptime);
			DRAWSMALLSTATS(24, 25, V_YELLOWMAP, "R_ClipSprites:  %d", ps_sw_spritecliptime);
			DRAWSMALLSTATS(24, 30, V_YELLOWMAP, "Portals+Skybox: %d", ps_sw_portaltime);
			DRAWSMALLSTATS(24, 35, V_YELLOWMAP, "R_DrawPlanes:   %d", ps_sw_planetime);
			DRAWSMALLSTATS(24, 40, V_YELLOWMAP, "R_DrawMasked:   %d", ps_sw_maskedtime);
			// Remember to update this calculation when adding more 3d rendering stats!
			DRAWSMALLSTATS(24, 45, V_YELLOWMAP, "Other:          %d",
				ps_rendercalltime - ps_bsptime - ps_sw_spritecliptime
				- ps_sw_portaltime - ps_sw_planetime - ps_sw_maskedtime);
			DRAWSMALLSTATS(20, 50, V_YELLOWMAP, "UI render:      %d", ps_uitime);
			DRAWSMALLSTATS(20, 55, V_YELLOWMAP, "I_FinishUpdate: %d", ps_swaptime);
			DRAWSMALLSTATS(20, 65, V_GRAYMAP, "Game logic:     %d", ps_tictime);
		}
	}

	Z_Free(s);
}

static void M_DrawLogicStats(void)
{
	size_t cursize = 99, size = cursize;
	char *s = Z_Malloc(size + 1, PU_STATIC, NULL);

	int i = 0;
	thinker_t *thinker;
	int thinkercount = 0;
	int polythcount = 0;
	int mainthcount = 0;
	int mobjcount = 0;
	int nothinkcount = 0;
	int scenerycount = 0;
	int dynslopethcount = 0;
	int precipcount = 0;
	int removecount = 0;
	// y offset for drawing columns
	int yoffset1 = 0;
	int yoffset2 = 0;

	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
		for (thinker = thlist[i].next; thinker != &thlist[i]; thinker = thinker->next)
		{
			thinkercount++;
			if (thinker->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
				removecount++;
			else if (i == THINK_POLYOBJ)
				polythcount++;
			else if (i == THINK_MAIN)
				mainthcount++;
			else if (i == THINK_MOBJ)
			{
				if (thinker->function.acp1 == (actionf_p1)P_MobjThinker)
				{
					mobj_t *mobj = (mobj_t*)thinker;
					mobjcount++;
					if (mobj->flags & MF_NOTHINK)
						nothinkcount++;
					else if (mobj->flags & MF_SCENERY)
						scenerycount++;
				}
			}
			else if (i == THINK_DYNSLOPE)
				dynslopethcount++;
			else if (i == THINK_PRECIP)
				precipcount++;
		}
	}

	if (vid.width < 640 || vid.height < 400) // low resolution
	{
		DRAWTHINSTATS(20, 10, V_YELLOWMAP, "logic   %d", ps_tictime);
		if (!(gamestate == GS_LEVEL || (gamestate == GS_TITLESCREEN && titlemapinaction)))
			return;
		DRAWTHINSTATS(24, 18, V_YELLOWMAP, "plrthnk %d", ps_playerthink_time);
		DRAWTHINSTATS(24, 26, V_YELLOWMAP, "thnkers %d", ps_thinkertime);
		for (i = 0; i < NUM_THINKERLISTS; i++)
		{
			yoffset1 += 8;
			DRAWTHINSTATS(28, 26+yoffset1, V_YELLOWMAP, thlist_shortnames[i], ps_thlist_times[i]);
		}
		DRAWTHINSTATS(24, 34+yoffset1, V_YELLOWMAP, "lthinkf %d", ps_lua_thinkframe_time);
		DRAWTHINSTATS(24, 42+yoffset1, V_YELLOWMAP, "other   %d",
			ps_tictime - ps_playerthink_time - ps_thinkertime - ps_lua_thinkframe_time);

		DRAWTHINSTATS(90, 10, V_BLUEMAP, "thnkers %d", thinkercount);
		DRAWTHINSTATS(94, 18, V_BLUEMAP, "plyobjs %d", polythcount);
		DRAWTHINSTATS(94, 26, V_BLUEMAP, "main    %d", mainthcount);
		DRAWTHINSTATS(94, 34, V_BLUEMAP, "mobjs   %d", mobjcount);
		DRAWTHINSTATS(98, 42, V_BLUEMAP, "regular %d", mobjcount - scenerycount - nothinkcount);
		DRAWTHINSTATS(98, 50, V_BLUEMAP, "scenery %d", scenerycount);
		if (nothinkcount)
		{
			DRAWTHINSTATS(98, 58, V_BLUEMAP, "nothink %d", nothinkcount);
			yoffset2 += 8;
		}
		DRAWTHINSTATS(94, 58+yoffset2, V_BLUEMAP, "dynslop %d", dynslopethcount);
		DRAWTHINSTATS(94, 66+yoffset2, V_BLUEMAP, "precip  %d", precipcount);
		DRAWTHINSTATS(94, 74+yoffset2, V_BLUEMAP, "remove  %d", removecount);

		DRAWTHINSTATS(170, 10, V_PURPLEMAP, "lmhooks %d", ps_lua_mobjhooks);
		DRAWTHINSTATS(170, 18, V_PURPLEMAP, "chkpos  %d", ps_checkposition_calls);
	}
	else // high resolution
	{
		DRAWSMALLSTATS(20, 10, V_YELLOWMAP, "Game logic:      %d", ps_tictime);
		if (!(gamestate == GS_LEVEL || (gamestate == GS_TITLESCREEN && titlemapinaction)))
			return;
		DRAWSMALLSTATS(24, 15, V_YELLOWMAP, "P_PlayerThink:   %d", ps_playerthink_time);
		DRAWSMALLSTATS(24, 20, V_YELLOWMAP, "P_RunThinkers:   %d", ps_thinkertime);
		for (i = 0; i < NUM_THINKERLISTS; i++)
		{
			yoffset1 += 5;
			DRAWSMALLSTATS(28, 20+yoffset1, V_YELLOWMAP, thlist_names[i], ps_thlist_times[i]);
		}
		DRAWSMALLSTATS(24, 25+yoffset1, V_YELLOWMAP, "LUAh_ThinkFrame: %d", ps_lua_thinkframe_time);
		DRAWSMALLSTATS(24, 30+yoffset1, V_YELLOWMAP, "Other:           %d",
			ps_tictime - ps_playerthink_time - ps_thinkertime - ps_lua_thinkframe_time);

		DRAWSMALLSTATS(115, 10+yoffset2, V_BLUEMAP, "Thinkers:        %d", thinkercount);
		DRAWSMALLSTATS(119, 15+yoffset2, V_BLUEMAP, "Polyobjects:     %d", polythcount);
		DRAWSMALLSTATS(119, 20+yoffset2, V_BLUEMAP, "Main:            %d", mainthcount);
		DRAWSMALLSTATS(119, 25+yoffset2, V_BLUEMAP, "Mobjs:           %d", mobjcount);
		DRAWSMALLSTATS(123, 30+yoffset2, V_BLUEMAP, "Regular:         %d", mobjcount - scenerycount - nothinkcount);
		DRAWSMALLSTATS(123, 35+yoffset2, V_BLUEMAP, "Scenery:         %d", scenerycount);
		if (nothinkcount)
		{
			DRAWSMALLSTATS(123, 40+yoffset2, V_BLUEMAP, "Nothink:         %d", nothinkcount);
			yoffset2 += 5;
		}
		DRAWSMALLSTATS(119, 40+yoffset2, V_BLUEMAP, "Dynamic slopes:  %d", dynslopethcount);
		DRAWSMALLSTATS(119, 45+yoffset2, V_BLUEMAP, "Precipitation:   %d", precipcount);
		DRAWSMALLSTATS(119, 50+yoffset2, V_BLUEMAP, "Pending removal: %d", removecount);

		DRAWSMALLSTATS(212, 10, V_PURPLEMAP, "Calls:");
		DRAWSMALLSTATS(216, 15, V_PURPLEMAP, "Lua mobj hooks:  %d", ps_lua_mobjhooks);
		DRAWSMALLSTATS(216, 20, V_PURPLEMAP, "P_CheckPosition: %d", ps_checkposition_calls);
	}

	Z_Free(s);
}

static void M_DrawThinkFrameStats(void)
{
	size_t cursize = 99, size = cursize;
	char *s = Z_Malloc(size + 1, PU_STATIC, NULL);

	if (!(gamestate == GS_LEVEL || (gamestate == GS_TITLESCREEN && titlemapinaction)))
		return;
	if (vid.width < 640 || vid.height < 400) // low resolution
	{
		// it's not gonna fit very well..
		DRAWTHINSTATS(30, 30, V_MONOSPACE | V_ALLOWLOWERCASE | V_YELLOWMAP, "Not available for resolutions below 640x400");
	}
	else // high resolution
	{
		int i;
		// text writing position
		int x = 2;
		int y = 4;
		UINT32 text_color;
		char tempbuffer[LUA_IDSIZE];
		char last_mod_name[LUA_IDSIZE];
		last_mod_name[0] = '\0';
		for (i = 0; i < thinkframe_hooks_length; i++)
		{
			char* str = thinkframe_hooks[i].short_src;
			char* tempstr = tempbuffer;
			int len = (int)strlen(str);
			char* str_ptr;
			if (strcmp(".lua", str + len - 4) == 0)
			{
				str[len-4] = '\0'; // remove .lua at end
				len -= 4;
			}
			// we locate the wad/pk3 name in the string and compare it to
			// what we found on the previous iteration.
			// if the name has changed, print it out on the screen
			strcpy(tempstr, str);
			str_ptr = strrchr(tempstr, '|');
			if (str_ptr)
			{
				*str_ptr = '\0';
				str = str_ptr + 1; // this is the name of the hook without the mod file name
				str_ptr = strrchr(tempstr, PATHSEP[0]);
				if (str_ptr)
					tempstr = str_ptr + 1;
				// tempstr should now point to the mod name, (wad/pk3) possibly truncated
				if (strcmp(tempstr, last_mod_name) != 0)
				{
					strcpy(last_mod_name, tempstr);
					len = (int)strlen(tempstr);
					if (len > 25)
						tempstr += len - 25;
					DRAWSMALLSTATS(x, y, V_GRAYMAP, "%s", tempstr);
					y += 4; // repeated code!
					if (y > 192)
					{
						y = 4;
						x += 106;
						if (x > 214)
							break;
					}
				}
				text_color = V_YELLOWMAP;
			}
			else
			{
				// probably a standalone lua file
				// cut off the folder if it's there
				str_ptr = strrchr(tempstr, PATHSEP[0]);
				if (str_ptr)
					str = str_ptr + 1;
				text_color = 0; // white
			}
			len = (int)strlen(str);
			if (len > 20)
				str += len - 20;
			DRAWSMALLSTATS(x, y, text_color, "%20s: %u", str, thinkframe_hooks[i].time_taken);
			y += 4; // repeated code!
			if (y > 192)
			{
				y = 4;
				x += 106;
				if (x > 214)
					break;
			}
		}
	}

	Z_Free(s);
}

void M_DrawPerfStats(void)
{
	// This function is so tiny now... ~Golden

	switch (cv_perfstats.value)
	{
		case 1: // rendering
			M_DrawRenderStats();
			break;
		case 2: // logic
			M_DrawLogicStats();
			break;
		case 3: // lua thinkframe
			M_DrawThinkFrameStats();
			break;
		default:
			break;
	}
}
