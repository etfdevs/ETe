/*
===========================================================================

Wolfenstein: Enemy Territory GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Wolfenstein: Enemy Territory GPL Source Code (Wolf ET Source Code).  

Wolf ET Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Wolf ET Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wolf ET Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Wolf: ET Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Wolf ET Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "tr_local.h"





/*
================
R_CullSurface

Tries to back face cull surfaces before they are lighted or
added to the sorting list.

This will also allow mirrors on both sides of a model without recursion.
================
*/
static qboolean R_CullSurface( const surfaceType_t *surface, shader_t *shader ) {
	const srfGeneric_t    *gen;
	int cull;
	float d;

	// allow culling to be disabled
	if ( r_nocull->integer ) {
		return qfalse;
	}

	// ydnar: made surface culling generic, inline with q3map2 surface classification
	switch ( *surface )
	{
	case SF_FACE:
	case SF_TRIANGLES:
		break;
	case SF_GRID:
		if ( r_nocurves->integer ) {
			return qtrue;
		}
		break;
	case SF_FOLIAGE:
		if ( !r_drawfoliage->value ) {
			return qtrue;
		}
		break;

	default:
		return qtrue;
	}

	// get generic surface
	gen = (const srfGeneric_t*) surface;

	// plane cull
	if ( gen->plane.type != PLANE_NON_PLANAR && r_facePlaneCull->integer ) {
		d = DotProduct( tr.orientation.viewOrigin, gen->plane.normal ) - gen->plane.dist;

		// don't cull exactly on the plane, because there are levels of rounding
		// through the BSP, ICD, and hardware that may cause pixel gaps if an
		// epsilon isn't allowed here
		if ( shader->cullType == CT_FRONT_SIDED ) {
			if ( d < -8.0f ) {
				tr.pc.c_plane_cull_out++;
				return qtrue;
			}
		} else if ( shader->cullType == CT_BACK_SIDED ) {
			if ( d > 8.0f ) {
				tr.pc.c_plane_cull_out++;
				return qtrue;
			}
		}

		tr.pc.c_plane_cull_in++;
	}

	{
		// try sphere cull
		if ( tr.currentEntityNum != REFENTITYNUM_WORLD ) {
			cull = R_CullLocalPointAndRadius( gen->origin, gen->radius );
		} else {
			cull = R_CullPointAndRadius( gen->origin, gen->radius );
		}
		if ( cull == CULL_OUT ) {
			tr.pc.c_sphere_cull_out++;
			return qtrue;
		}

		tr.pc.c_sphere_cull_in++;
	}

	// must be visible
	return qfalse;
}


#ifdef USE_PMLIGHT
qboolean R_LightCullBounds( const dlight_t* dl, const vec3_t mins, const vec3_t maxs )
{
	if ( dl->flags & REF_DIRECTED_DLIGHT )
		return qfalse;

	if ( dl->linear ) {
		if (dl->transformed[0] - dl->radius > maxs[0] && dl->transformed2[0] - dl->radius > maxs[0] )
			return qtrue;
		if (dl->transformed[0] + dl->radius < mins[0] && dl->transformed2[0] + dl->radius < mins[0] )
			return qtrue;

		if (dl->transformed[1] - dl->radius > maxs[1] && dl->transformed2[1] - dl->radius > maxs[1] )
			return qtrue;
		if (dl->transformed[1] + dl->radius < mins[1] && dl->transformed2[1] + dl->radius < mins[1] )
			return qtrue;

		if (dl->transformed[2] - dl->radius > maxs[2] && dl->transformed2[2] - dl->radius > maxs[2] )
			return qtrue;
		if (dl->transformed[2] + dl->radius < mins[2] && dl->transformed2[2] + dl->radius < mins[2] )
			return qtrue;

		return qfalse;
	}

	if (dl->transformed[0] - dl->radius > maxs[0])
		return qtrue;
	if (dl->transformed[0] + dl->radius < mins[0])
		return qtrue;

	if (dl->transformed[1] - dl->radius > maxs[1])
		return qtrue;
	if (dl->transformed[1] + dl->radius < mins[1])
		return qtrue;

	if (dl->transformed[2] - dl->radius > maxs[2])
		return qtrue;
	if (dl->transformed[2] + dl->radius < mins[2])
		return qtrue;

	return qfalse;
}


static qboolean R_LightCullSurface( const surfaceType_t* surface, const dlight_t* dl )
{
	const srfGeneric_t *gen = (const srfGeneric_t *)surface;
	switch (*surface)
	{
		case SF_FACE:
		case SF_GRID:
		case SF_FOLIAGE:
		case SF_TRIANGLES:
			return R_LightCullBounds( dl, gen->bounds[0], gen->bounds[1] );
		default:
			return qfalse;
	}
}
#endif // USE_PMLIGHT


#ifdef USE_LEGACY_DLIGHTS

/*
====================
R_DlightSurface

The given surface is going to be drawn, and it touches a leaf
that is touched by one or more dlights, so try to throw out
more dlights if possible.
====================
*/

// ydnar: made this use generic surface

static int R_DlightSurface( const msurface_t *surface, int dlightBits ) {
	int i;
	vec3_t origin;
	float radius;
	srfGeneric_t    *gen;

	// ydnar: made surface dlighting generic, inline with q3map2 surface classification
	switch ( (surfaceType_t) *surface->data )
	{
	case SF_FACE:
	case SF_TRIANGLES:
	case SF_GRID:
	case SF_FOLIAGE:
		break;

	default:
		return 0;
	}

	// get generic surface
	gen = (srfGeneric_t*)surface->data;

	// debug code
	//%	gen->dlightBits[ tr.smpFrame ] = dlightBits;
	//%	return dlightBits;

	// try to cull out dlights
	for ( i = 0; i < tr.refdef.num_dlights; i++ )
	{
		if ( !( dlightBits & ( 1 << i ) ) ) {
			continue;
		}

		// junior dlights don't affect world surfaces
		if ( tr.refdef.dlights[ i ].flags & REF_JUNIOR_DLIGHT ) {
			dlightBits &= ~( 1 << i );
			continue;
		}

		// lightning dlights affect all surfaces
		if ( tr.refdef.dlights[ i ].flags & REF_DIRECTED_DLIGHT ) {
			continue;
		}

		// test surface bounding sphere against dlight bounding sphere
		VectorCopy( tr.refdef.dlights[ i ].transformed, origin );
		radius = tr.refdef.dlights[ i ].radius;

		if ( ( gen->origin[ 0 ] + gen->radius ) < ( origin[ 0 ] - radius ) ||
			 ( gen->origin[ 0 ] - gen->radius ) > ( origin[ 0 ] + radius ) ||
			 ( gen->origin[ 1 ] + gen->radius ) < ( origin[ 1 ] - radius ) ||
			 ( gen->origin[ 1 ] - gen->radius ) > ( origin[ 1 ] + radius ) ||
			 ( gen->origin[ 2 ] + gen->radius ) < ( origin[ 2 ] - radius ) ||
			 ( gen->origin[ 2 ] - gen->radius ) > ( origin[ 2 ] + radius ) ) {
			dlightBits &= ~( 1 << i );
		}
	}

	// Com_Printf( "Surf: 0x%08X dlightBits: 0x%08X\n", srf, dlightBits );

	// set counters
	if ( dlightBits == 0 ) {
		tr.pc.c_dlightSurfacesCulled++;
	} else {
		tr.pc.c_dlightSurfaces++;
	}

	// set surface dlight bits and return
	gen->dlightBits = dlightBits;
	return dlightBits;
}

#endif // USE_LEGACY_DLIGHTS
	
/*
======================
R_AddWorldSurface
======================
*/
static void R_AddWorldSurface( msurface_t *surf, shader_t *shader, int dlightBits, int decalBits ) {
	int i;

	if ( surf->viewCount == tr.viewCount ) {
		return;     // already in this view

	}

	surf->viewCount = tr.viewCount;
	// FIXME: bmodel fog?

	// try to cull before dlighting or adding
	if ( R_CullSurface( surf->data, shader ) ) {
		return;
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( R_GetDlightMode() )
#endif
	{
		surf->vcVisible = tr.viewCount;

		// add decals
		if ( decalBits ) {
			// ydnar: project any decals
			for ( i = 0; i < tr.refdef.numDecalProjectors; i++ )
			{
				if ( decalBits & ( 1 << i ) ) {
					R_ProjectDecalOntoSurface( &tr.refdef.decalProjectors[ i ], surf, tr.currentBModel );
				}
			}
		}		
		
		R_AddDrawSurf( surf->data, shader, surf->fogIndex, 0 );
		return;
	}
#endif // USE_PMLIGHT

#ifdef USE_LEGACY_DLIGHTS
	// check for dlighting
	if ( dlightBits ) {
		dlightBits = R_DlightSurface( surf, dlightBits );
		dlightBits = ( dlightBits != 0 );
	}
#endif

	// add decals
	if ( decalBits ) {
		// ydnar: project any decals
		for ( i = 0; i < tr.refdef.numDecalProjectors; i++ )
		{
			if ( decalBits & ( 1 << i ) ) {
				R_ProjectDecalOntoSurface( &tr.refdef.decalProjectors[ i ], surf, tr.currentBModel );
			}
		}
	}

#ifdef USE_LEGACY_DLIGHTS
	R_AddDrawSurf( surf->data, shader, surf->fogIndex, dlightBits );
#endif // USE_LEGACY_DLIGHTS
}


/*
=============================================================
	PM LIGHTING
=============================================================
*/
#ifdef USE_PMLIGHT
static void R_AddLitSurface( msurface_t *surf, const dlight_t *light )
{
	// since we're not worried about offscreen lights casting into the frustum (ATM !!!)
	// only add the "lit" version of this surface if it was already added to the view
	//if ( surf->viewCount != tr.viewCount )
	//	return;

	// surfaces that were faceculled will still have the current viewCount in vcBSP
	// because that's set to indicate that it's BEEN vis tested at all, to avoid
	// repeated vis tests, not whether it actually PASSED the vis test or not
	// only light surfaces that are GENUINELY visible, as opposed to merely in a visible LEAF
	if ( surf->vcVisible != tr.viewCount ) {
		return;
	}

	if ( surf->shader->lightingStage < 0 ) {
		return;
	}

	if ( surf->lightCount == tr.lightCount )
		return;

	surf->lightCount = tr.lightCount;

	if ( R_LightCullSurface( surf->data, light ) ) {
		tr.pc.c_lit_culls++;
		return;
	}

	R_AddLitSurf( surf->data, surf->shader, surf->fogIndex );
}


static void R_RecursiveLightNode( const mnode_t* node )
{
	qboolean children[2];
	msurface_t** mark;
	msurface_t* surf;
	float d;
	int c;
	do {
		// if the node wasn't marked as potentially visible, exit
		if ( node->visframe != tr.visCount )
			return;

		if ( node->contents != CONTENTS_NODE )
			break;

		children[0] = children[1] = qfalse;

		d = DotProduct( tr.light->origin, node->plane->normal ) - node->plane->dist;
		if ( d > -tr.light->radius ) {
			children[0] = qtrue;
		}
		if ( d < tr.light->radius ) {
			children[1] = qtrue;
		}

		if ( tr.light->linear ) {
			d = DotProduct( tr.light->origin2, node->plane->normal ) - node->plane->dist;
			if ( d > -tr.light->radius ) {
				children[0] = qtrue;
			}
			if ( d < tr.light->radius ) {
				children[1] = qtrue;
			}
		}

		if ( children[0] && children[1] ) {
			R_RecursiveLightNode( node->children[0] );
			node = node->children[1];
		}
		else if ( children[0] ) {
			node = node->children[0];
		}
		else if ( children[1] ) {
			node = node->children[1];
		}
		else {
			return;
		}

	} while ( 1 );

	tr.pc.c_lit_leafs++;

	// add the individual surfaces
	c = node->nummarksurfaces;
	mark = node->firstmarksurface;
	while ( c-- ) {
		// the surface may have already been added if it spans multiple leafs
		surf = *mark;
		R_AddLitSurface( surf, tr.light );
		mark++;
	}
}
#endif // USE_PMLIGHT


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

//----(SA) added

/*
=================
R_BmodelFogNum

See if a sprite is inside a fog volume
Return positive with /any part/ of the brush falling within a fog volume
=================
*/

static int R_BmodelFogNum( const trRefEntity_t *re, const bmodel_t *bmodel )
{
	int i, j;
	const fog_t *fog;

	for ( i = 1; i < tr.world->numfogs; i++ )
	{
		fog = &tr.world->fogs[ i ];
		for ( j = 0; j < 3; j++ )
		{
			if ( re->e.origin[ j ] + bmodel->bounds[ 0 ][ j ] >= fog->bounds[ 1 ][ j ] ) {
				break;
			}
			if ( re->e.origin[ j ] + bmodel->bounds[ 1 ][ j ] <= fog->bounds[ 0 ][ j ] ) {
				break;
			}
		}
		if ( j == 3 ) {
			return i;
		}
	}

	return 0;
}

//----(SA) done



/*
=================
R_AddBrushModelSurfaces
=================
*/
void R_AddBrushModelSurfaces( trRefEntity_t *ent ) {
	int i, clip, fognum, decalBits;
	vec3_t mins, maxs;
	const model_t        *pModel;
	bmodel_t            *bmodel;
	int savedNumDecalProjectors, numLocalProjectors;
	decalProjector_t    *savedDecalProjectors, localProjectors[ MAX_DECAL_PROJECTORS ];

	pModel = R_GetModelByHandle( ent->e.hModel );

	bmodel = pModel->model.bmodel;

	clip = R_CullLocalBox( bmodel->bounds );
	if ( clip == CULL_OUT ) {
		return;
	}

	// ydnar: set current brush model to world
	tr.currentBModel = bmodel;

	// ydnar: set model state for decals and dynamic fog
	VectorCopy( ent->e.origin, bmodel->orientation.origin );
	VectorCopy( ent->e.axis[ 0 ], bmodel->orientation.axis[ 0 ] );
	VectorCopy( ent->e.axis[ 1 ], bmodel->orientation.axis[ 1 ] );
	VectorCopy( ent->e.axis[ 2 ], bmodel->orientation.axis[ 2 ] );
	bmodel->visible = qtrue;
	bmodel->entityNum = tr.currentEntityNum;

	// determine if in fog
	fognum = R_BmodelFogNum( ent, bmodel );

	// ydnar: project any decals
	decalBits = 0;
	numLocalProjectors = 0;
	for ( i = 0; i < tr.refdef.numDecalProjectors; i++ )
	{
		// early out
		if ( tr.refdef.decalProjectors[ i ].shader == NULL ) {
			continue;
		}

		// transform entity bbox (fixme: rotated entities have invalid bounding boxes)
		VectorAdd( bmodel->bounds[ 0 ], tr.orientation.origin, mins );
		VectorAdd( bmodel->bounds[ 1 ], tr.orientation.origin, maxs );

		// set bit
		if ( R_TestDecalBoundingBox( &tr.refdef.decalProjectors[ i ], mins, maxs ) ) {
			R_TransformDecalProjector( &tr.refdef.decalProjectors[ i ], tr.orientation.axis, tr.orientation.origin, &localProjectors[ numLocalProjectors ] );
			numLocalProjectors++;
			decalBits <<= 1;
			decalBits |= 1;
		}
	}

	// ydnar: save old decal projectors
	savedNumDecalProjectors = tr.refdef.numDecalProjectors;
	savedDecalProjectors = tr.refdef.decalProjectors;

	// set local decal projectors
	tr.refdef.numDecalProjectors = numLocalProjectors;
	tr.refdef.decalProjectors = localProjectors;
	
#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( R_GetDlightMode() )
#endif
	{
		dlight_t *dl;
		int s;

		for ( s = 0; s < bmodel->numSurfaces; s++ ) {
			msurface_t *surf = (bmodel->firstSurface + s);
			surf->fogIndex = fognum;
			// Arnout: custom shader support for brushmodels
			if ( ent->e.customShader ) {
				R_AddWorldSurface( surf, R_GetShaderByHandle( ent->e.customShader ), 0, decalBits );
			} else {
				R_AddWorldSurface( surf, surf->shader, 0, decalBits );
			}
		}

		R_SetupEntityLighting( &tr.refdef, ent );
		
		R_TransformDlights( tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.orientation );

		for ( i = 0; i < tr.viewParms.num_dlights; i++ ) {
			dl = &tr.viewParms.dlights[i];
			if ( !R_LightCullBounds( dl, bmodel->bounds[0], bmodel->bounds[1] ) ) {
				tr.lightCount++;
				tr.light = dl;
				for ( s = 0; s < bmodel->numSurfaces; s++ ) {
					R_AddLitSurface( bmodel->firstSurface + s, dl );
				}
			}
		}

		// ydnar: restore old decal projectors
		tr.refdef.numDecalProjectors = savedNumDecalProjectors;
		tr.refdef.decalProjectors = savedDecalProjectors;

		// ydnar: add decal surfaces
		R_AddDecalSurfaces( bmodel );

		// ydnar: clear current brush model
		tr.currentBModel = NULL;
		return;
	}
#endif // USE_PMLIGHT

#ifdef USE_LEGACY_DLIGHTS
	R_SetupEntityLighting( &tr.refdef, ent );
	R_DlightBmodel( bmodel );

	// add model surfaces
	for ( i = 0; i < bmodel->numSurfaces; i++ )
	{
		msurface_t *surf = (bmodel->firstSurface + i);
		surf->fogIndex = fognum;
		// Arnout: custom shader support for brushmodels
		if ( ent->e.customShader ) {
			R_AddWorldSurface( surf, R_GetShaderByHandle( ent->e.customShader ), tr.currentEntity->needDlights, decalBits );
		} else {
			R_AddWorldSurface( surf, surf->shader, tr.currentEntity->needDlights, decalBits );
		}
	}

	// ydnar: restore old decal projectors
	tr.refdef.numDecalProjectors = savedNumDecalProjectors;
	tr.refdef.decalProjectors = savedDecalProjectors;

	// ydnar: add decal surfaces
	R_AddDecalSurfaces( bmodel );

	// ydnar: clear current brush model
	tr.currentBModel = NULL;
#endif
}


/*
=============================================================

	WORLD MODEL

=============================================================
*/


/*
R_AddLeafSurfaces() - ydnar
adds a leaf's drawsurfaces
*/

static void R_AddLeafSurfaces( mnode_t *node, unsigned int dlightBits, int decalBits ) {
	int c;
	msurface_t  *surf, **mark;


	// add to count
	tr.pc.c_leafs++;

	// add to z buffer bounds
	if ( node->mins[0] < tr.viewParms.visBounds[0][0] ) {
		tr.viewParms.visBounds[0][0] = node->mins[0];
	}
	if ( node->mins[1] < tr.viewParms.visBounds[0][1] ) {
		tr.viewParms.visBounds[0][1] = node->mins[1];
	}
	if ( node->mins[2] < tr.viewParms.visBounds[0][2] ) {
		tr.viewParms.visBounds[0][2] = node->mins[2];
	}

	if ( node->maxs[0] > tr.viewParms.visBounds[1][0] ) {
		tr.viewParms.visBounds[1][0] = node->maxs[0];
	}
	if ( node->maxs[1] > tr.viewParms.visBounds[1][1] ) {
		tr.viewParms.visBounds[1][1] = node->maxs[1];
	}
	if ( node->maxs[2] > tr.viewParms.visBounds[1][2] ) {
		tr.viewParms.visBounds[1][2] = node->maxs[2];
	}

	// add the individual surfaces
	mark = node->firstmarksurface;
	c = node->nummarksurfaces;
	while ( c-- ) {
		// the surface may have already been added if it
		// spans multiple leafs
		surf = *mark;
		R_AddWorldSurface( surf, surf->shader, dlightBits, decalBits );
		mark++;
	}
}


/*
================
R_RecursiveWorldNode
================
*/
static void R_RecursiveWorldNode( mnode_t *node, unsigned int planeBits, unsigned int dlightBits, unsigned int decalBits ) {

	do {
		// if the node wasn't marked as potentially visible, exit
		if (node->visframe != tr.visCount) {
			return;
		}

		// if the bounding volume is outside the frustum, nothing
		// inside can be visible OPTIMIZE: don't do this all the way to leafs?

		if ( !r_nocull->integer ) {
			int		r;

			if ( planeBits & 1 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~1;			// all descendants will also be in front
				}
			}

			if ( planeBits & 2 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[1]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~2;			// all descendants will also be in front
				}
			}

			if ( planeBits & 4 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[2]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~4;			// all descendants will also be in front
				}
			}

			if ( planeBits & 8 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[3]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~8;			// all descendants will also be in front
				}
			}

			// ydnar: farplane culling
			if ( planeBits & 16 ) {
				r = BoxOnPlaneSide( node->mins, node->maxs, &tr.viewParms.frustum[4] );
				if ( r == 2 ) {
					return;                     // culled
				}
				if ( r == 1 ) {
					planeBits &= ~16;            // all descendants will also be in front
				}
			}

		}

		// ydnar: cull dlights
		// determine which dlights are needed
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
		if ( !R_GetDlightMode() )
#endif
		if ( dlightBits ) {  //%	&& node->contents != -1 )
			int i;

			for ( i = 0; i < tr.refdef.num_dlights; i++ ) {
				dlight_t	*dl;

				if ( dlightBits & ( 1 << i ) ) {
					dl = &tr.refdef.dlights[i];
					// directional dlights don't get culled
					if ( dl->flags & REF_DIRECTED_DLIGHT ) {
						continue;
					}

					// test dlight bounds against node surface bounds
					if ( node->surfMins[ 0 ] >= ( dl->origin[ 0 ] + dl->radius ) || node->surfMaxs[ 0 ] <= ( dl->origin[ 0 ] - dl->radius ) ||
						 node->surfMins[ 1 ] >= ( dl->origin[ 1 ] + dl->radius ) || node->surfMaxs[ 1 ] <= ( dl->origin[ 1 ] - dl->radius ) ||
						 node->surfMins[ 2 ] >= ( dl->origin[ 2 ] + dl->radius ) || node->surfMaxs[ 2 ] <= ( dl->origin[ 2 ] - dl->radius ) ) {
						dlightBits &= ~( 1 << i );
					}
				}
			}
		}
#endif // USE_LEGACY_DLIGHTS

		// ydnar: cull decals
		if ( decalBits ) {
			int i;
			for ( i = 0; i < tr.refdef.numDecalProjectors; i++ )
			{
				if ( decalBits & ( 1 << i ) ) {
					// test decal bounds against node surface bounds
					if ( tr.refdef.decalProjectors[ i ].shader == NULL ||
						 !R_TestDecalBoundingBox( &tr.refdef.decalProjectors[ i ], node->surfMins, node->surfMaxs ) ) {
						decalBits &= ~( 1 << i );
					}
				}
			}
		}

		// handle leaf nodes
		if ( node->contents != CONTENTS_NODE ) {
			break;
		}

		// recurse down the children, front side first
		R_RecursiveWorldNode( node->children[ 0 ], planeBits, dlightBits, decalBits );

		// tail recurse
		node = node->children[1];
	} while ( 1 );

	// short circuit
	if ( node->nummarksurfaces == 0 ) {
		return;
	}

	// ydnar: moved off to separate function
	R_AddLeafSurfaces( node, dlightBits, decalBits );
}


/*
===============
R_PointInLeaf
===============
*/
static mnode_t *R_PointInLeaf( const vec3_t p ) {
	mnode_t		*node;
	float		d;
	const cplane_t	*plane;
	
	if ( !tr.world ) {
		ri.Error (ERR_DROP, "R_PointInLeaf: bad model");
	}

	node = tr.world->nodes;
	while( 1 ) {
		if (node->contents != CONTENTS_NODE ) {
			break;
		}
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0) {
			node = node->children[0];
		} else {
			node = node->children[1];
		}
	}
	
	return node;
}

/*
==============
R_ClusterPVS
==============
*/
static const byte *R_ClusterPVS (int cluster) {
	// tr.world cannot and should not be null where this is called so do not add somewhere tr.world isn't setup
	if ( !tr.world->vis || cluster < 0 || cluster >= tr.world->numClusters ) {
		return tr.world->novis;
	}

	return tr.world->vis + cluster * tr.world->clusterBytes;
}

/*
=================
R_inPVS
=================
*/
qboolean R_inPVS( const vec3_t p1, const vec3_t p2 ) {
	const mnode_t *leaf;
	const byte	*vis;

	leaf = R_PointInLeaf( p1 );
	if ( leaf->cluster < 0 )
		return qfalse;
	vis = ri.CM_ClusterPVS( leaf->cluster );
	leaf = R_PointInLeaf( p2 );
	if ( leaf->cluster < 0 )
		return qfalse;

	if ( !(vis[leaf->cluster>>3] & (1<<(leaf->cluster&7))) ) {
		return qfalse;
	}
	return qtrue;
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
static void R_MarkLeaves (void) {
	const byte	*vis;
	mnode_t	*leaf, *parent;
	int		i;
	int		cluster;

	// lockpvs lets designers walk around to determine the
	// extent of the current pvs
	if ( r_lockpvs->integer ) {
		return;
	}

	// current viewcluster
	leaf = R_PointInLeaf( tr.viewParms.pvsOrigin );
	cluster = leaf->cluster;

	// if the cluster is the same and the area visibility matrix
	// hasn't changed, we don't need to mark everything again

	// if r_showcluster was just turned on, remark everything 
	if ( tr.viewCluster == cluster && !tr.refdef.areamaskModified 
		&& !r_showcluster->modified ) {
		return;
	}

	if ( r_showcluster->modified || r_showcluster->integer ) {
		r_showcluster->modified = qfalse;
		if ( r_showcluster->integer ) {
			ri.Printf( PRINT_ALL, "cluster:%i  area:%i\n", cluster, leaf->area );
		}
	}

	tr.visCount++;
	tr.viewCluster = cluster;

	if ( r_novis->integer || tr.viewCluster == -1 ) {
		for (i=0 ; i<tr.world->numnodes ; i++) {
			if (tr.world->nodes[i].contents != CONTENTS_SOLID) {
				tr.world->nodes[i].visframe = tr.visCount;
			}
		}
		return;
	}

	vis = R_ClusterPVS (tr.viewCluster);
	
	for (i=0,leaf=tr.world->nodes ; i<tr.world->numnodes ; i++, leaf++) {
		cluster = leaf->cluster;
		if ( cluster < 0 || cluster >= tr.world->numClusters ) {
			continue;
		}

		// check general pvs
		if ( !(vis[cluster>>3] & (1<<(cluster&7))) ) {
			continue;
		}

		// check for door connection
		if ( leaf->area >= 8 * (int)ARRAY_LEN(tr.refdef.areamask) ||
			 (leaf->area >= 0 && tr.refdef.areamask[leaf->area>>3] & (1<<(leaf->area&7)) ) ) {
			continue;		// not visible
		}

		// ydnar: don't want to walk the entire bsp to add skybox surfaces
		if ( tr.refdef.rdflags & RDF_SKYBOXPORTAL ) {
			// this only happens once, as game/cgame know the origin of the skybox
			// this also means the skybox portal cannot move, as this list is calculated once and never again
			if ( tr.world->numSkyNodes < WORLD_MAX_SKY_NODES ) {
				tr.world->skyNodes[ tr.world->numSkyNodes++ ] = leaf;
			}
			R_AddLeafSurfaces( leaf, 0, 0 );
			continue;
		}

		parent = leaf;
		do {
			if (parent->visframe == tr.visCount)
				break;
			parent->visframe = tr.visCount;
			parent = parent->parent;
		} while (parent);
	}
}


/*
=============
R_AddWorldSurfaces
=============
*/
void R_AddWorldSurfaces( void ) {
#ifdef USE_PMLIGHT
	dlight_t* dl;
	int i;
#endif

	if ( !r_drawworld->integer ) {
		return;
	}

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	// ydnar: set current brush model to world
	tr.currentBModel = &tr.world->bmodels[ 0 ];

	// clear out the visible min/max
	ClearBounds( tr.viewParms.visBounds[0], tr.viewParms.visBounds[1] );

	// render sky or world?
	if ( tr.refdef.rdflags & RDF_SKYBOXPORTAL && tr.world->numSkyNodes > 0 ) {
		//int i;
		mnode_t **node;

		for ( i = 0, node = tr.world->skyNodes; i < tr.world->numSkyNodes; i++, node++ )
			R_AddLeafSurfaces( *node, ( 1ULL << tr.refdef.num_dlights ) - 1/*tr.refdef.dlightBits*/, 0 );    // no decals on skybox nodes
	} else
	{
		// determine which leaves are in the PVS / areamask
		R_MarkLeaves();

		// perform frustum culling and add all the potentially visible surfaces
		if ( tr.refdef.num_dlights > MAX_DLIGHTS ) {
			tr.refdef.num_dlights = MAX_DLIGHTS;
		}

		R_RecursiveWorldNode( tr.world->nodes, 255, ( 1ULL << tr.refdef.num_dlights ) - 1 /*tr.refdef.dlightBits*/, tr.refdef.decalBits );

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
		if ( !R_GetDlightMode() )
		{
			// ydnar: add decal surfaces
			R_AddDecalSurfaces( tr.world->bmodels );

			// clear brush model
			tr.currentBModel = NULL;

			return;
		}
#endif // USE_LEGACY_DLIGHTS

		// "transform" all the dlights so that dl->transformed is actually populated
		// (even though HERE it's == dl->origin) so we can always use R_LightCullBounds
		// instead of having copypasted versions for both world and local cases

		R_TransformDlights( tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.viewParms.world );
		for ( i = 0; i < tr.viewParms.num_dlights; i++ ) 
		{
			dl = &tr.viewParms.dlights[i];	
			dl->head = dl->tail = NULL;
			if ( R_CullDlight( dl ) == CULL_OUT ) {
				tr.pc.c_light_cull_out++;
				continue;
			}
			tr.pc.c_light_cull_in++;
			tr.lightCount++;
			tr.light = dl;
			R_RecursiveLightNode( tr.world->nodes );
		}

		// ydnar: add decal surfaces
		R_AddDecalSurfaces( tr.world->bmodels );
	}

	// clear brush model
	tr.currentBModel = NULL;
#endif // USE_PMLIGHT
}
