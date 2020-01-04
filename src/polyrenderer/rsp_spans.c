// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2020 by Jaime "Lactozilla" Passos.
// Copyright (C) 2019 by Vinícius "Arkus-Kotan" Telésforo.
// Copyright (C) 2017 by Krzysztof Kondrak.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  rsp_spans.c
/// \brief Span drawers

#include "r_softpoly.h"

// triangle drawer functions
void (*rsp_curtrifunc)(rsp_triangle_t *tri, rsp_trimode_t type);
void (*rsp_fixedtrifunc)(rsp_triangle_t *tri, rsp_trimode_t type);
void (*rsp_floattrifunc)(rsp_triangle_t *tri, rsp_trimode_t type);

static UINT32 tex_width, tex_height;
static UINT16 *tex_data;
static UINT8 *tex_translation;
static UINT8 *tex_colormap;
static UINT8 *tex_transmap;

// Fixed-point texture mapping
static inline void texspanloop(fixed_t y, fixed_t startXPrestep, fixed_t endXPrestep, fixed_t startX, fixed_t startInvZ, fixed_t endInvZ, fixed_t startU, fixed_t endU, fixed_t startV, fixed_t endV, fixed_t invLineLength)
{
	fixed_t x, r, z;
	UINT16 u, v;
	UINT16 pixel = 0;
	boolean depth_only = ((rsp_target.mode & (RENDERMODE_DEPTH|RENDERMODE_COLOR)) == RENDERMODE_DEPTH);
	INT32 ix, clipleft, clipright;

	// avoid a crash here
	if (!rsp_curpixelfunc)
		I_Error("texspanloop: no pixel drawer set!");

	rsp_tpix = tex_transmap;
	rsp_ypix = y>>FRACBITS;
	if (rsp_target.aiming)
		rsp_ypix += SOFTWARE_AIMING;

	clipleft = rsp_portalclip[0];
	clipright = rsp_portalclip[1];

	if (startXPrestep < (clipleft<<FRACBITS))
		startXPrestep += (clipleft<<FRACBITS);
	if (startXPrestep < 0)
		startXPrestep = 0;
	if (endXPrestep > (clipright<<FRACBITS))
		endXPrestep = (clipright<<FRACBITS);
	if (endXPrestep >= (viewwidth<<FRACBITS))
		endXPrestep = (viewwidth-1)<<FRACBITS;

	for (x = startXPrestep; x <= endXPrestep; x += FRACUNIT)
	{
		ix = x>>FRACBITS;
		if (rsp_mfloorclip && rsp_mceilingclip)
		{
			if (rsp_ypix >= rsp_mfloorclip[ix]) continue;
			if (rsp_ypix <= rsp_mceilingclip[ix]) continue;
		}
		// interpolate 1/z for each pixel in the scanline
		r = FixedMul((x - startX), invLineLength);
		rsp_xpix = ix;
		rsp_zpix = FixedLerp(startInvZ, endInvZ, r);
		z = FixedDiv(FRACUNIT, rsp_zpix);
		if (!depth_only)
		{
			u = FixedMul(z, FixedLerp(startU, endU, r))>>FRACBITS;
			v = FixedMul(z, FixedLerp(startV, endV, r))>>FRACBITS;
			u %= tex_width;
			v %= tex_height;
			pixel = tex_data[(v * tex_width) + u];
			if (pixel & 0xFF00)
			{
				pixel &= 0x00FF;
				if (tex_translation)
					pixel = tex_translation[pixel];
				if (tex_colormap)
					pixel = tex_colormap[pixel];
				rsp_cpix = pixel;
				rsp_curpixelfunc();
			}
		}
		else
			rsp_curpixelfunc();
	}
}

void RSP_TexturedMappedTriangle(rsp_triangle_t *triangle, rsp_trimode_t type)
{
	rsp_vertex_t *v0 = &triangle->vertices[0];
	rsp_vertex_t *v1 = &triangle->vertices[1];
	rsp_vertex_t *v2 = &triangle->vertices[2];
	fixed_t y, invDy, dxLeft, dxRight, prestep, yDir = FRACUNIT;
	fixed_t startX, endX, startXPrestep, endXPrestep;
	UINT32 texW = triangle->texture->width;
	UINT32 texH = triangle->texture->height;
	INT32 currLine, numScanlines;
	fixed_t invZ0, invZ1, invZ2, invY02 = FRACUNIT;

	fixed_t v0x = FLOAT_TO_FIXED(v0->position.x);
	fixed_t v0y = FLOAT_TO_FIXED(v0->position.y);
	fixed_t v0z = FLOAT_TO_FIXED(v0->position.z);
	fixed_t v0u = FLOAT_TO_FIXED(v0->uv.u);
	fixed_t v0v = FLOAT_TO_FIXED(v0->uv.v);

	fixed_t v1x = FLOAT_TO_FIXED(v1->position.x);
	fixed_t v1y = FLOAT_TO_FIXED(v1->position.y);
	fixed_t v1z = FLOAT_TO_FIXED(v1->position.z);
	fixed_t v1u = FLOAT_TO_FIXED(v1->uv.u);
	fixed_t v1v = FLOAT_TO_FIXED(v1->uv.v);

	fixed_t v2x = FLOAT_TO_FIXED(v2->position.x);
	fixed_t v2y = FLOAT_TO_FIXED(v2->position.y);
	fixed_t v2z = FLOAT_TO_FIXED(v2->position.z);
	fixed_t v2u = FLOAT_TO_FIXED(v2->uv.u);
	fixed_t v2v = FLOAT_TO_FIXED(v2->uv.v);

	(void)v1y;

	tex_data = triangle->texture->data;
	tex_width = texW;
	tex_height = texH;
	tex_translation = triangle->translation;
	tex_colormap = triangle->colormap;
	tex_transmap = triangle->transmap;

	// set pixel drawer
	rsp_curpixelfunc = rsp_basepixelfunc;
	if (tex_transmap)
		rsp_curpixelfunc = rsp_transpixelfunc;

	if (type == TRI_FLATBOTTOM)
	{
		invDy = FixedDiv(FRACUNIT, FixedCeil(v2y - v0y));
		numScanlines = FixedCeil(v2y - v0y)>>FRACBITS;
		prestep = (FixedCeil(v0y) - v0y);
		if ((v2y - v0y) < FRACUNIT)
			return;
	}
	else if (type == TRI_FLATTOP)
	{
		yDir = -FRACUNIT;
		invDy = FixedDiv(FRACUNIT, FixedCeil(v0y - v2y));
		numScanlines = FixedCeil(v0y - v2y)>>FRACBITS;
		prestep = (FixedCeil(v2y) - v2y);
		if ((v0y - v2y) < FRACUNIT)
			return;
	}
	else
		I_Error("RSP_TexturedMappedTriangle: unknown triangle type");

	if (numScanlines >= rsp_target.height)
		return;

	dxLeft = FixedMul((v2x - v0x), invDy);
	dxRight = FixedMul((v1x - v0x), invDy);
	startX = endX = v0x;
	startXPrestep = startX + FixedMul(dxLeft, prestep);
	endXPrestep = endX + FixedMul(dxRight, prestep);

	invZ0 = FixedDiv(FRACUNIT, v0z);
	invZ1 = FixedDiv(FRACUNIT, v1z);
	invZ2 = FixedDiv(FRACUNIT, v2z);
	invY02 = FixedDiv(FRACUNIT, (v0y - v2y));

	for (currLine = 0, y = v0y; currLine <= numScanlines; y += yDir, currLine++)
	{
		fixed_t startInvZ, endInvZ, r1, invLineLength;
		fixed_t startU, startV, endU, endV;
		fixed_t lineLength = (endX - startX);

		// skip zero-length lines
		if (lineLength > 0)
		{
			r1 = FixedMul((v0y - y), invY02);
			startInvZ = FixedLerp(invZ0, invZ2, r1);
			endInvZ = FixedLerp(invZ0, invZ1, r1);

			startU = FixedMul(texW<<FRACBITS, FixedLerp(FixedMul(v0u, invZ0), FixedMul(v2u, invZ2), r1));
			startV = FixedMul(texH<<FRACBITS, FixedLerp(FixedMul(v0v, invZ0), FixedMul(v2v, invZ2), r1));
			endU = FixedMul(texW<<FRACBITS, FixedLerp(FixedMul(v0u, invZ0), FixedMul(v1u, invZ1), r1));
			endV = FixedMul(texH<<FRACBITS, FixedLerp(FixedMul(v0v, invZ0), FixedMul(v1v, invZ1), r1));

			invLineLength = FixedDiv(FRACUNIT, lineLength);
			texspanloop(y, startXPrestep, endXPrestep, startX, startInvZ, endInvZ, startU, endU, startV, endV, invLineLength);
		}

		startX += dxLeft;
		endX += dxRight;
		if (currLine < numScanlines-1)
		{
			startXPrestep += dxLeft;
			endXPrestep += dxRight;
		}
	}
}

// Floating-point texture mapping
static inline void texspanloop_fp(float y, float startXPrestep, float endXPrestep, float startX, float startInvZ, float endInvZ, float startU, float endU, float startV, float endV, float invLineLength)
{
	float x, r, z, z2;
	UINT16 u, v;
	UINT16 pixel = 0;
	boolean depth_only = ((rsp_target.mode & (RENDERMODE_DEPTH|RENDERMODE_COLOR)) == RENDERMODE_DEPTH);
	INT32 ix, clipleft, clipright;

	// avoid a crash here
	if (!rsp_curpixelfunc)
		I_Error("texspanloop_fp: no pixel drawer set!");

	rsp_tpix = tex_transmap;
	rsp_ypix = FLOAT_TO_FIXED(y)>>FRACBITS;
	if (rsp_target.aiming)
		rsp_ypix += SOFTWARE_AIMING;

	clipleft = rsp_portalclip[0];
	clipright = rsp_portalclip[1];

	if (startXPrestep < clipleft)
		startXPrestep += clipleft;
	if (startXPrestep < 0.0f)
		startXPrestep = 0.0f;
	if (endXPrestep > clipright)
		endXPrestep = (float)clipright;
	if (endXPrestep > viewwidth-1)
		endXPrestep = (float)viewwidth-1;

	for (x = startXPrestep; x <= endXPrestep; x++)
	{
		ix = FLOAT_TO_FIXED(x)>>FRACBITS;
		if (rsp_mfloorclip && rsp_mceilingclip)
		{
			if (rsp_ypix >= rsp_mfloorclip[ix]) continue;
			if (rsp_ypix <= rsp_mceilingclip[ix]) continue;
		}
		// interpolate 1/z for each pixel in the scanline
		r = ((x - startX) * invLineLength);
		rsp_xpix = ix;
		z2 = FloatLerp(startInvZ, endInvZ, r);
		z = 1.0f / z2;
		rsp_zpix = FLOAT_TO_FIXED(FloatLerp(startInvZ, endInvZ, r));
		if (!depth_only)
		{
			u = FLOAT_TO_FIXED(z * FloatLerp(startU, endU, r))>>FRACBITS;
			v = FLOAT_TO_FIXED(z * FloatLerp(startV, endV, r))>>FRACBITS;
			u %= tex_width;
			v %= tex_height;
			pixel = tex_data[(v * tex_width) + u];
			if (pixel & 0xFF00)
			{
				pixel &= 0x00FF;
				if (tex_translation)
					pixel = tex_translation[pixel];
				if (tex_colormap)
					pixel = tex_colormap[pixel];
				rsp_cpix = pixel;
				rsp_curpixelfunc();
			}
		}
		else
			rsp_curpixelfunc();
	}
}

void RSP_TexturedMappedTriangleFP(rsp_triangle_t *triangle, rsp_trimode_t type)
{
	rsp_vertex_t *v0 = &triangle->vertices[0];
	rsp_vertex_t *v1 = &triangle->vertices[1];
	rsp_vertex_t *v2 = &triangle->vertices[2];
	float y, invDy, dxLeft, dxRight, prestep, yDir = 1.0f;
	float startX, endX, startXPrestep, endXPrestep;
	UINT32 texW = triangle->texture->width;
	UINT32 texH = triangle->texture->height;
	float currLine, numScanlines;
	float invZ0, invZ1, invZ2, invY02 = 1.0f;

	float v0x = (v0->position.x);
	float v0y = (v0->position.y);
	float v0z = (v0->position.z);
	float v0u = (v0->uv.u);
	float v0v = (v0->uv.v);

	float v1x = (v1->position.x);
	float v1y = (v1->position.y);
	float v1z = (v1->position.z);
	float v1u = (v1->uv.u);
	float v1v = (v1->uv.v);

	float v2x = (v2->position.x);
	float v2y = (v2->position.y);
	float v2z = (v2->position.z);
	float v2u = (v2->uv.u);
	float v2v = (v2->uv.v);

	(void)v1y;

	tex_data = triangle->texture->data;
	tex_width = texW;
	tex_height = texH;
	tex_translation = triangle->translation;
	tex_colormap = triangle->colormap;
	tex_transmap = triangle->transmap;

	// set pixel drawer
	rsp_curpixelfunc = rsp_basepixelfunc;
	if (tex_transmap)
		rsp_curpixelfunc = rsp_transpixelfunc;

	if (type == TRI_FLATBOTTOM)
	{
		invDy = 1.0f / ceil(v2y - v0y);
		numScanlines = ceil(v2y - v0y);
		prestep = (ceil(v0y) - v0y);
		if ((v2y - v0y) < 1.0f)
			return;
	}
	else if (type == TRI_FLATTOP)
	{
		yDir = -1.0f;
		invDy = 1.0f / ceil(v0y - v2y);
		numScanlines = ceil(v0y - v2y);
		prestep = (ceil(v2y) - v2y);
		if ((v0y - v2y) < 1.0f)
			return;
	}
	else
		I_Error("RSP_TexturedMappedTriangleFP: unknown triangle type");

	if (numScanlines >= rsp_target.height)
		return;

	dxLeft = ((v2x - v0x) * invDy);
	dxRight = ((v1x - v0x) * invDy);
	startX = endX = v0x;
	startXPrestep = startX + (dxLeft * prestep);
	endXPrestep = endX + (dxRight * prestep);

	invZ0 = 1.0f / v0z;
	invZ1 = 1.0f / v1z;
	invZ2 = 1.0f / v2z;
	invY02 = 1.0f / (v0y - v2y);

	for (currLine = 0, y = v0y; currLine <= numScanlines; y += yDir, currLine++)
	{
		float startInvZ, endInvZ, r1, invLineLength;
		float startU, startV, endU, endV;
		float lineLength = (endX - startX);

		// skip zero-length lines
		if (lineLength > 0)
		{
			r1 = (v0y - y) * invY02;
			startInvZ = FloatLerp(invZ0, invZ2, r1);
			endInvZ = FloatLerp(invZ0, invZ1, r1);

			startU = texW * FloatLerp((v0u * invZ0), (v2u * invZ2), r1);
			startV = texH * FloatLerp((v0v * invZ0), (v2v * invZ2), r1);
			endU = texW * FloatLerp((v0u * invZ0), (v1u * invZ1), r1);
			endV = texH * FloatLerp((v0v * invZ0), (v1v * invZ1), r1);

			invLineLength = 1.0f / lineLength;
			texspanloop_fp(y, startXPrestep, endXPrestep, startX, startInvZ, endInvZ, startU, endU, startV, endV, invLineLength);
		}

		startX += dxLeft;
		endX += dxRight;
		if (currLine < numScanlines-1)
		{
			startXPrestep += dxLeft;
			endXPrestep += dxRight;
		}
	}
}
