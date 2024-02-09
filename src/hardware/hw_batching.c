// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020-2023 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file hw_batching.c
/// \brief Draw call batching and related things.

#ifdef HWRENDER
#include "hw_glob.h"
#include "hw_batching.h"
#include "hw_main.h"
#include "../i_system.h"

typedef struct
{
	FSurfaceInfo surf; // surf also has its own polyflags for some reason, but it seems unused
	unsigned int vertsIndex; // location of verts in unsortedVertices
	FUINT numVerts;
	FBITFIELD polyFlags;
	GLMipmap_t *texture;
	int shader;
	// this tells batching that the plane belongs to a horizon line and must be drawn in correct order with the skywalls
	boolean horizonSpecial;
} DrawCallInfo;

// The texture for the next polygon given to HWR_ProcessPolygon.
// Set with HWR_SetCurrentTexture.
GLMipmap_t *current_texture = NULL;

boolean currently_batching = false;

// Dynamic arrays for:
//   - unsorted draw calls
//   - sorted order of draw calls
//   - unsorted vertices
//   - final (sorted) vertices
//   - vertex indices for sorted vertices

// contains the draw calls from DrawPolygon, waiting to be processed
DrawCallInfo* drawCalls = NULL;
int drawCallsSize = 0;
int drawCallsCapacity = 65536;

// contains sorted order (array indices) for drawCalls
// (therefore size and capacity shared with it)
UINT32* drawCallOrder = NULL;

// contains unsorted vertices and texture coordinates from DrawPolygon
FOutVector* unsortedVertices = NULL;
int unsortedVerticesSize = 0;
int unsortedVerticesCapacity = 65536;

// contains subset of sorted vertices and texture coordinates to be sent to gpu
FOutVector* finalVertices = NULL;
int finalVerticesSize = 0;
int finalVerticesCapacity = 65536;

// contains vertex indices into finalVertices for glDrawElements,
// taking into account fan->triangles conversion
UINT32* finalIndices = NULL; // this is alloced with 3x finalVertices size
int finalIndicesSize = 0;

// Enables batching mode. HWR_ProcessPolygon will collect polygons instead of passing them directly to the rendering backend.
// Call HWR_RenderBatches to render all the collected geometry.
void HWR_StartBatching(void)
{
    if (currently_batching)
        I_Error("Repeat call to HWR_StartBatching without HWR_RenderBatches");

    // init arrays if that has not been done yet
	if (!finalVertices)
	{
		finalVertices = malloc(finalVerticesCapacity * sizeof(FOutVector));
		finalIndices = malloc(finalVerticesCapacity * 3 * sizeof(UINT32));
		drawCalls = malloc(drawCallsCapacity * sizeof(DrawCallInfo));
		drawCallOrder = malloc(drawCallsCapacity * sizeof(UINT32));
		unsortedVertices = malloc(unsortedVerticesCapacity * sizeof(FOutVector));
	}

    currently_batching = true;
}

// This replaces the direct calls to pfnSetTexture in cases where batching is available.
// The texture selection is saved for the next HWR_ProcessPolygon call.
// Doing this was easier than getting a texture pointer to HWR_ProcessPolygon.
void HWR_SetCurrentTexture(GLMipmap_t *texture)
{
   if (currently_batching)
		current_texture = texture;
	else
		HWD.pfnSetTexture(texture);
}

static void HWR_CollectDrawCallInfo(FSurfaceInfo *pSurf, FUINT iNumPts, FBITFIELD PolyFlags, int shader_target, boolean horizonSpecial)
{
	// make sure dynamic array has capacity
	if (drawCallsSize == drawCallsCapacity)
	{
		DrawCallInfo* new_array;
		// ran out of space, make new array double the size
		drawCallsCapacity *= 2;
		new_array = malloc(drawCallsCapacity * sizeof(DrawCallInfo));
		memcpy(new_array, drawCalls, drawCallsSize * sizeof(DrawCallInfo));
		free(drawCalls);
		drawCalls = new_array;
		// also need to redo the index array, dont need to copy it though
		free(drawCallOrder);
		drawCallOrder = malloc(drawCallsCapacity * sizeof(UINT32));
	}
	// add entry to array
	drawCalls[drawCallsSize].surf = *pSurf;
	drawCalls[drawCallsSize].vertsIndex = unsortedVerticesSize;
	drawCalls[drawCallsSize].numVerts = iNumPts;
	drawCalls[drawCallsSize].polyFlags = PolyFlags;
	drawCalls[drawCallsSize].texture = current_texture;
	drawCalls[drawCallsSize].shader = (shader_target != -1) ? HWR_GetShaderFromTarget(shader_target) : shader_target;
	drawCalls[drawCallsSize].horizonSpecial = horizonSpecial;
	drawCallsSize++;
}

static void HWR_CollectDrawCallVertices(FOutVector *pOutVerts, FUINT iNumPts)
{
	// make sure dynamic array has capacity
	while (unsortedVerticesSize + (int)iNumPts > unsortedVerticesCapacity)
	{
		FOutVector* new_array;
		// need more space for vertices in unsortedVertices
		unsortedVerticesCapacity *= 2;
		new_array = malloc(unsortedVerticesCapacity * sizeof(FOutVector));
		memcpy(new_array, unsortedVertices, unsortedVerticesSize * sizeof(FOutVector));
		free(unsortedVertices);
		unsortedVertices = new_array;
	}
	// add vertices to array
	memcpy(&unsortedVertices[unsortedVerticesSize], pOutVerts, iNumPts * sizeof(FOutVector));
	unsortedVerticesSize += iNumPts;
}

// If batching is enabled, this function collects the polygon data and the chosen texture
// for later use in HWR_RenderBatches. Otherwise the rendering backend is used to
// render the polygon immediately.
void HWR_ProcessPolygon(FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags, int shader_target, boolean horizonSpecial)
{
	if (currently_batching)
	{
		if (!pSurf)
		{
			// handling null FSurfaceInfo is not implemented
			I_Error("Got a null FSurfaceInfo in batching");
		}

		HWR_CollectDrawCallInfo(pSurf, iNumPts, PolyFlags, shader_target, horizonSpecial);
		HWR_CollectDrawCallVertices(pOutVerts, iNumPts);
	}
	else
	{
		HWD.pfnSetShader((shader_target != SHADER_NONE) ? HWR_GetShaderFromTarget(shader_target) : shader_target);
		HWD.pfnDrawPolygon(pSurf, pOutVerts, iNumPts, PolyFlags);
	}
}

static int compareDrawCalls(const void *p1, const void *p2)
{
	UINT32 index1 = *(const UINT32*)p1;
	UINT32 index2 = *(const UINT32*)p2;
	DrawCallInfo* poly1 = &drawCalls[index1];
	DrawCallInfo* poly2 = &drawCalls[index2];
	int diff;
	INT64 diff64;
	UINT32 downloaded1 = 0;
	UINT32 downloaded2 = 0;

	int shader1 = poly1->shader;
	int shader2 = poly2->shader;
	// make skywalls and horizon lines first in order
	if (poly1->polyFlags & PF_NoTexture || poly1->horizonSpecial)
		shader1 = -1;
	if (poly2->polyFlags & PF_NoTexture || poly2->horizonSpecial)
		shader2 = -1;
	diff = shader1 - shader2;
	if (diff != 0) return diff;

	// skywalls and horizon lines must retain their order for horizon lines to work
	if (shader1 == -1 && shader2 == -1)
		return index1 - index2;

	if (poly1->texture)
		downloaded1 = poly1->texture->downloaded; // there should be a opengl texture name here, usable for comparisons
	if (poly2->texture)
		downloaded2 = poly2->texture->downloaded;
	diff64 = downloaded1 - downloaded2;
	if (diff64 != 0) return diff64;

	diff = poly1->polyFlags - poly2->polyFlags;
	if (diff != 0) return diff;

	diff64 = poly1->surf.PolyColor.rgba - poly2->surf.PolyColor.rgba;
	if (diff64 < 0) return -1; else if (diff64 > 0) return 1;
	diff64 = poly1->surf.TintColor.rgba - poly2->surf.TintColor.rgba;
	if (diff64 < 0) return -1; else if (diff64 > 0) return 1;
	diff64 = poly1->surf.FadeColor.rgba - poly2->surf.FadeColor.rgba;
	if (diff64 < 0) return -1; else if (diff64 > 0) return 1;

	diff = poly1->surf.LightInfo.light_level - poly2->surf.LightInfo.light_level;
	if (diff != 0) return diff;
	diff = poly1->surf.LightInfo.fade_start - poly2->surf.LightInfo.fade_start;
	if (diff != 0) return diff;
	diff = poly1->surf.LightInfo.fade_end - poly2->surf.LightInfo.fade_end;
	return diff;
}

static int compareDrawCallsNoShaders(const void *p1, const void *p2)
{
	UINT32 index1 = *(const UINT32*)p1;
	UINT32 index2 = *(const UINT32*)p2;
	DrawCallInfo* poly1 = &drawCalls[index1];
	DrawCallInfo* poly2 = &drawCalls[index2];
	int diff;
	INT64 diff64;

	GLMipmap_t *texture1 = poly1->texture;
	GLMipmap_t *texture2 = poly2->texture;
	UINT32 downloaded1 = 0;
	UINT32 downloaded2 = 0;
	if (poly1->polyFlags & PF_NoTexture || poly1->horizonSpecial)
		texture1 = NULL;
	if (poly2->polyFlags & PF_NoTexture || poly2->horizonSpecial)
		texture2 = NULL;
	if (texture1)
		downloaded1 = texture1->downloaded; // there should be a opengl texture name here, usable for comparisons
	if (texture2)
		downloaded2 = texture2->downloaded;
	// skywalls and horizon lines must retain their order for horizon lines to work
	if (!texture1 && !texture2)
		return index1 - index2;
	diff64 = downloaded1 - downloaded2;
	if (diff64 != 0) return diff64;

	diff = poly1->polyFlags - poly2->polyFlags;
	if (diff != 0) return diff;

	diff64 = poly1->surf.PolyColor.rgba - poly2->surf.PolyColor.rgba;
	if (diff64 < 0) return -1; else if (diff64 > 0) return 1;

	return 0;
}

static void HWR_CollectVerticesIntoBatch(DrawCallInfo *drawCall)
{
	int firstVIndex;
	int lastVIndex;
	int numVerts = drawCall->numVerts;
	// before writing, check if there is enough room
	// using 'while' instead of 'if' here makes sure that there will *always* be enough room.
	// probably never will this loop run more than once though
	while (finalVerticesSize + numVerts > finalVerticesCapacity)
	{
		FOutVector* new_array;
		UINT32* new_index_array;
		finalVerticesCapacity *= 2;
		new_array = malloc(finalVerticesCapacity * sizeof(FOutVector));
		memcpy(new_array, finalVertices, finalVerticesSize * sizeof(FOutVector));
		free(finalVertices);
		finalVertices = new_array;
		// also increase size of index array, 3x of vertex array since
		// going from fans to triangles increases vertex count to 3x
		new_index_array = malloc(finalVerticesCapacity * 3 * sizeof(UINT32));
		memcpy(new_index_array, finalIndices, finalIndicesSize * sizeof(UINT32));
		free(finalIndices);
		finalIndices = new_index_array;
	}
	// write the vertices of the polygon
	memcpy(&finalVertices[finalVerticesSize], &unsortedVertices[drawCall->vertsIndex],
		numVerts * sizeof(FOutVector));
	// write the indexes, pointing to the fan vertexes but in triangles format
	firstVIndex = finalVerticesSize;
	lastVIndex = finalVerticesSize + numVerts;
	finalVerticesSize += 2;
	while (finalVerticesSize < lastVIndex)
	{
		finalIndices[finalIndicesSize++] = firstVIndex;
		finalIndices[finalIndicesSize++] = finalVerticesSize - 1;
		finalIndices[finalIndicesSize++] = finalVerticesSize++;
	}
}

static GLMipmap_t *HWR_GetDrawCallTexture(DrawCallInfo *dc)
{
	return (dc->polyFlags & PF_NoTexture) ? NULL : dc->texture;
}

// flags returned by HWR_MarkStateChanges
#define SHADER_CHANGED      1
#define TEXTURE_CHANGED     2
#define POLYFLAGS_CHANGED   4
#define SURFACEINFO_CHANGED 8

static unsigned int HWR_MarkStateChanges(DrawCallInfo *current, DrawCallInfo *next)
{
	unsigned int stateChanges = 0;
	FSurfaceInfo *currentSI = &current->surf;
	FSurfaceInfo *nextSI = &next->surf;

	// check if a state change is required and flag it to stateChanges

	if (current->shader != next->shader && cv_grshaders.value && gr_shadersavailable)
		stateChanges |= SHADER_CHANGED;
	if (HWR_GetDrawCallTexture(current) != HWR_GetDrawCallTexture(next))
		stateChanges |= TEXTURE_CHANGED;
	if (current->polyFlags != next->polyFlags)
		stateChanges |= POLYFLAGS_CHANGED;
	if (cv_grshaders.value && gr_shadersavailable)
	{
		if (currentSI->PolyColor.rgba != nextSI->PolyColor.rgba ||
			currentSI->TintColor.rgba != nextSI->TintColor.rgba ||
			currentSI->FadeColor.rgba != nextSI->FadeColor.rgba ||
			currentSI->LightInfo.light_level != nextSI->LightInfo.light_level ||
			currentSI->LightInfo.fade_start != nextSI->LightInfo.fade_start ||
			currentSI->LightInfo.fade_end != nextSI->LightInfo.fade_end)
		{
			stateChanges |= SURFACEINFO_CHANGED;
		}
	}
	else
	{
		if (currentSI->PolyColor.rgba != nextSI->PolyColor.rgba)
			stateChanges |= SURFACEINFO_CHANGED;
	}

	return stateChanges;
}

static void HWR_ExecuteStateChanges(unsigned int stateChanges, DrawCallInfo *dc)
{
	if ((stateChanges & SHADER_CHANGED) && cv_grshaders.value && gr_shadersavailable)
	{
		HWD.pfnSetShader(dc->shader);
		ps_hw_numshaders.value.i++;
	}
	if (stateChanges & TEXTURE_CHANGED)
	{
		// for PF_NoTexture the texture is set in DrawIndexedTriangles
		if (!(dc->polyFlags & PF_NoTexture))
		{
			// texture should be already ready for use from calls to
			// SetTexture during batch collection
			HWD.pfnSetTexture(dc->texture);
		}
		ps_hw_numtextures.value.i++;
	}
	// these two are just parameters to DrawIndexedTriangles
	// (changes to polyflags states are tracked in r_opengl.c though)
	if (stateChanges & POLYFLAGS_CHANGED)
		ps_hw_numpolyflags.value.i++;
	if (stateChanges & SURFACEINFO_CHANGED)
		ps_hw_numcolors.value.i++;
}


static void HWR_InitBatchingStats(void)
{
	ps_hw_numpolys.value.i = drawCallsSize;
	ps_hw_numcalls.value.i = ps_hw_numverts.value.i
		= ps_hw_numshaders.value.i = ps_hw_numtextures.value.i
		= ps_hw_numpolyflags.value.i = ps_hw_numcolors.value.i = 0;
}

// This function organizes the geometry and draw calls collected by HWR_ProcessPolygon
// calls into batches and uses the rendering backend to draw them.
void HWR_RenderBatches(void)
{
	int drawCallReadPos = 0; // position in drawCallOrder
	int i;

	if (!currently_batching)
		I_Error("HWR_RenderBatches called without starting batching");

	currently_batching = false; // no longer collecting batches

	HWR_InitBatchingStats();

	if (!drawCallsSize)
		return; // nothing to draw

	// init drawCallOrder
	for (i = 0; i < drawCallsSize; i++)
		drawCallOrder[i] = i;

	// sort the draw calls
	PS_START_TIMING(ps_hw_batchsorttime);
	if (cv_grshaders.value && gr_shadersavailable)
		qsort(drawCallOrder, drawCallsSize, sizeof(UINT32), compareDrawCalls);
	else
		qsort(drawCallOrder, drawCallsSize, sizeof(UINT32), compareDrawCallsNoShaders);
	PS_STOP_TIMING(ps_hw_batchsorttime);
	// sort grouping order
	// 1. shader
	// 2. texture
	// 3. polyflags
	// 4. colors + light level
	// not sure about what order of the last 2 should be, or if it even matters

	PS_START_TIMING(ps_hw_batchdrawtime);

	// set state for first batch
	HWR_ExecuteStateChanges(0xFFFF, &drawCalls[drawCallOrder[0]]);

	// - iterate through draw calls
	// - accumulate converted vertices and indices into finalVertices and finalIndices
	//   and send them as a combined draw call when a state change occurs
	while (1)
	{
		boolean stopFlag = false;
		unsigned int stateChanges = 0; // flags defined above HWR_MarkStateChanges
		DrawCallInfo *currentDrawCall = &drawCalls[drawCallOrder[drawCallReadPos++]];
		DrawCallInfo *nextDrawCall = NULL;

		HWR_CollectVerticesIntoBatch(currentDrawCall);

		if (drawCallReadPos >= drawCallsSize) // was that the last draw call?
		{
			stopFlag = true;
		}
		else
		{
			nextDrawCall = &drawCalls[drawCallOrder[drawCallReadPos]];
			stateChanges = HWR_MarkStateChanges(currentDrawCall, nextDrawCall);
		}

		if (stateChanges || stopFlag)
		{
			// execute combined draw call
			HWD.pfnDrawIndexedTriangles(&currentDrawCall->surf, finalVertices,
					finalIndicesSize, currentDrawCall->polyFlags, finalIndices);
			// update stats
			ps_hw_numcalls.value.i++;
			ps_hw_numverts.value.i += finalIndicesSize;
			// reset final geometry collection arrays
			finalVerticesSize = 0;
			finalIndicesSize = 0;
		}
		else
			continue;

		// if we're here then either its time to stop or time to change state
		if (stopFlag)
			break;
		else
			HWR_ExecuteStateChanges(stateChanges, nextDrawCall);
	}
	// reset the arrays (set sizes to 0)
	drawCallsSize = 0;
	unsortedVerticesSize = 0;

	PS_STOP_TIMING(ps_hw_batchdrawtime);
}

#endif // HWRENDER
