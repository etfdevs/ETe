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
// tr_surf.c
#include "tr_local.h"

/*

  THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/


//============================================================================


/*
==============
RB_CheckOverflow
==============
*/
void RB_CheckOverflow( int verts, int indexes ) {
	if (tess.numVertexes + verts < SHADER_MAX_VERTEXES
		&& tess.numIndexes + indexes < SHADER_MAX_INDEXES) {
		return;
	}

	RB_EndSurface();

	if ( verts >= SHADER_MAX_VERTEXES ) {
		ri.Error( ERR_DROP, "RB_CheckOverflow: verts > MAX (%d > %d)", verts, SHADER_MAX_VERTEXES );
	}

	if ( indexes >= SHADER_MAX_INDEXES ) {
		ri.Error( ERR_DROP, "RB_CheckOverflow: indices > MAX (%d > %d)", indexes, SHADER_MAX_INDEXES );
	}

	RB_BeginSurface( tess.shader, tess.fogNum );
}

/*
==============
RB_AddQuadStampFadingCornersExt

  Creates a sprite with the center at colors[3] alpha, and the corners all 0 alpha
==============
*/
/*void RB_AddQuadStampFadingCornersExt( const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color, float s1, float t1, float s2, float t2 ) {
	vec3_t normal;
	int ndx;
	color4ub_t lColor;

	VBO_Flush();

	RB_CHECKOVERFLOW( 5, 12 );

	tess.surfType = SF_TRIANGLES;

	ndx = tess.numVertexes;

	// triangle indexes for a simple quad
	tess.indexes[ tess.numIndexes + 0 ] = ndx + 0;
	tess.indexes[ tess.numIndexes + 1 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 2 ] = ndx + 4;

	tess.indexes[ tess.numIndexes + 3 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 4 ] = ndx + 2;
	tess.indexes[ tess.numIndexes + 5 ] = ndx + 4;

	tess.indexes[ tess.numIndexes + 6 ] = ndx + 2;
	tess.indexes[ tess.numIndexes + 7 ] = ndx + 3;
	tess.indexes[ tess.numIndexes + 8 ] = ndx + 4;

	tess.indexes[ tess.numIndexes + 9 ] = ndx + 3;
	tess.indexes[ tess.numIndexes + 10] = ndx + 0;
	tess.indexes[ tess.numIndexes + 11] = ndx + 4;

	tess.xyz[ndx][0] = origin[0] + left[0] + up[0];
	tess.xyz[ndx][1] = origin[1] + left[1] + up[1];
	tess.xyz[ndx][2] = origin[2] + left[2] + up[2];

	tess.xyz[ndx + 1][0] = origin[0] - left[0] + up[0];
	tess.xyz[ndx + 1][1] = origin[1] - left[1] + up[1];
	tess.xyz[ndx + 1][2] = origin[2] - left[2] + up[2];

	tess.xyz[ndx + 2][0] = origin[0] - left[0] - up[0];
	tess.xyz[ndx + 2][1] = origin[1] - left[1] - up[1];
	tess.xyz[ndx + 2][2] = origin[2] - left[2] - up[2];

	tess.xyz[ndx + 3][0] = origin[0] + left[0] - up[0];
	tess.xyz[ndx + 3][1] = origin[1] + left[1] - up[1];
	tess.xyz[ndx + 3][2] = origin[2] + left[2] - up[2];

	tess.xyz[ndx + 4][0] = origin[0];
	tess.xyz[ndx + 4][1] = origin[1];
	tess.xyz[ndx + 4][2] = origin[2];

	// constant normal all the way around
	VectorSubtract( vec3_origin, backEnd.viewParms.orientation.axis[0], normal );

	tess.normal[ndx][0] = tess.normal[ndx + 1][0] = tess.normal[ndx + 2][0] = tess.normal[ndx + 3][0] = tess.normal[ndx + 4][0] = normal[0];
	tess.normal[ndx][1] = tess.normal[ndx + 1][1] = tess.normal[ndx + 2][1] = tess.normal[ndx + 3][1] = tess.normal[ndx + 4][1] = normal[1];
	tess.normal[ndx][2] = tess.normal[ndx + 1][2] = tess.normal[ndx + 2][2] = tess.normal[ndx + 3][2] = tess.normal[ndx + 4][2] = normal[2];

	// standard square texture coordinates
	tess.texCoords[0][ndx+0][0] = tess.texCoords[1][ndx+0][0] = s1;
	tess.texCoords[0][ndx+0][1] = tess.texCoords[1][ndx+0][1] = t1;

	tess.texCoords[0][ndx+1][0] = tess.texCoords[1][ndx+1][0] = s2;
	tess.texCoords[0][ndx+1][1] = tess.texCoords[1][ndx+1][1] = t1;

	tess.texCoords[0][ndx+2][0] = tess.texCoords[1][ndx+2][0] = s2;
	tess.texCoords[0][ndx+2][1] = tess.texCoords[1][ndx+2][1] = t2;

	tess.texCoords[0][ndx+3][0] = tess.texCoords[1][ndx+3][0] = s1;
	tess.texCoords[0][ndx+3][1] = tess.texCoords[1][ndx+3][1] = t2;

	tess.texCoords[0][ndx+4][0] = tess.texCoords[1][ndx+4][0] = (s1 + s2) / 2.0;
	tess.texCoords[0][ndx+4][1] = tess.texCoords[1][ndx+4][1] = (t1 + t2) / 2.0;

	// center uses full alpha
	tess.vertexColors[ndx + 4] = color;

	// fade around edges
	lColor = color;
	lColor.rgba[3] = 0;
	
	tess.vertexColors[ndx + 0] =
	tess.vertexColors[ndx + 1] =
	tess.vertexColors[ndx + 2] =
	tess.vertexColors[ndx + 3] = lColor;


	tess.numVertexes += 5;
	tess.numIndexes += 12;
}*/


/*
==============
RB_AddQuadStampExt
==============
*/
void RB_AddQuadStampExt( const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color, float s1, float t1, float s2, float t2 ) {
	vec3_t		normal;
	int			ndx;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( 4, 6 );

	tess.surfType = SF_TRIANGLES;

	ndx = tess.numVertexes;

	// triangle indexes for a simple quad
	tess.indexes[ tess.numIndexes + 0 ] = ndx + 0;
	tess.indexes[ tess.numIndexes + 1 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 2 ] = ndx + 3;

	tess.indexes[ tess.numIndexes + 3 ] = ndx + 3;
	tess.indexes[ tess.numIndexes + 4 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 5 ] = ndx + 2;

	tess.xyz[ndx][0] = origin[0] + left[0] + up[0];
	tess.xyz[ndx][1] = origin[1] + left[1] + up[1];
	tess.xyz[ndx][2] = origin[2] + left[2] + up[2];

	tess.xyz[ndx+1][0] = origin[0] - left[0] + up[0];
	tess.xyz[ndx+1][1] = origin[1] - left[1] + up[1];
	tess.xyz[ndx+1][2] = origin[2] - left[2] + up[2];

	tess.xyz[ndx+2][0] = origin[0] - left[0] - up[0];
	tess.xyz[ndx+2][1] = origin[1] - left[1] - up[1];
	tess.xyz[ndx+2][2] = origin[2] - left[2] - up[2];

	tess.xyz[ndx+3][0] = origin[0] + left[0] - up[0];
	tess.xyz[ndx+3][1] = origin[1] + left[1] - up[1];
	tess.xyz[ndx+3][2] = origin[2] + left[2] - up[2];

	// constant normal all the way around
	VectorSubtract( vec3_origin, backEnd.viewParms.orientation.axis[0], normal );

	tess.normal[ndx][0] = tess.normal[ndx+1][0] = tess.normal[ndx+2][0] = tess.normal[ndx+3][0] = normal[0];
	tess.normal[ndx][1] = tess.normal[ndx+1][1] = tess.normal[ndx+2][1] = tess.normal[ndx+3][1] = normal[1];
	tess.normal[ndx][2] = tess.normal[ndx+1][2] = tess.normal[ndx+2][2] = tess.normal[ndx+3][2] = normal[2];

	// standard square texture coordinates
	tess.texCoords[0][ndx+0][0] = tess.texCoords[1][ndx+0][0] = s1;
	tess.texCoords[0][ndx+0][1] = tess.texCoords[1][ndx+0][1] = t1;

	tess.texCoords[0][ndx+1][0] = tess.texCoords[1][ndx+1][0] = s2;
	tess.texCoords[0][ndx+1][1] = tess.texCoords[1][ndx+1][1] = t1;

	tess.texCoords[0][ndx+2][0] = tess.texCoords[1][ndx+2][0] = s2;
	tess.texCoords[0][ndx+2][1] = tess.texCoords[1][ndx+2][1] = t2;

	tess.texCoords[0][ndx+3][0] = tess.texCoords[1][ndx+3][0] = s1;
	tess.texCoords[0][ndx+3][1] = tess.texCoords[1][ndx+3][1] = t2;

	// constant color all the way around
	// should this be identity and let the shader specify from entity?
	tess.vertexColors[ndx + 0] =
	tess.vertexColors[ndx + 1] =
	tess.vertexColors[ndx + 2] =
	tess.vertexColors[ndx + 3] = color;

	tess.numVertexes += 4;
	tess.numIndexes += 6;
}


void RB_AddQuadStamp2( float x, float y, float w, float h, float s1, float t1, float s2, float t2, color4ub_t color ) {
	int			numIndexes;
	int			numVerts;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( 4, 6 );

	tess.surfType = SF_TRIANGLES;

	numIndexes = tess.numIndexes;
	numVerts = tess.numVertexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[numIndexes + 0] = numVerts + 3;
	tess.indexes[numIndexes + 1] = numVerts + 0;
	tess.indexes[numIndexes + 2] = numVerts + 2;
	tess.indexes[numIndexes + 3] = numVerts + 2;
	tess.indexes[numIndexes + 4] = numVerts + 0;
	tess.indexes[numIndexes + 5] = numVerts + 1;

	tess.vertexColors[numVerts + 0] =
	tess.vertexColors[numVerts + 1] =
	tess.vertexColors[numVerts + 2] =
	tess.vertexColors[numVerts + 3] = color;

	tess.xyz[numVerts + 0][0] = x;
	tess.xyz[numVerts + 0][1] = y;
	tess.xyz[numVerts + 0][2] = 0;

	tess.xyz[numVerts + 1][0] = x + w;
	tess.xyz[numVerts + 1][1] = y;
	tess.xyz[numVerts + 1][2] = 0;

	tess.xyz[numVerts + 2][0] = x + w;
	tess.xyz[numVerts + 2][1] = y + h;
	tess.xyz[numVerts + 2][2] = 0;

	tess.xyz[numVerts + 3][0] = x;
	tess.xyz[numVerts + 3][1] = y + h;
	tess.xyz[numVerts + 3][2] = 0;

	tess.texCoords[0][numVerts + 0][0] = s1;
	tess.texCoords[0][numVerts + 0][1] = t1;
	tess.texCoords[0][numVerts + 1][0] = s2;
	tess.texCoords[0][numVerts + 1][1] = t1;
	tess.texCoords[0][numVerts + 2][0] = s2;
	tess.texCoords[0][numVerts + 2][1] = t2;
	tess.texCoords[0][numVerts + 3][0] = s1;
	tess.texCoords[0][numVerts + 3][1] = t2;
}


/*
==============
RB_AddQuadStamp
==============
*/
void RB_AddQuadStamp( const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color ) {
	RB_AddQuadStampExt( origin, left, up, color, 0, 0, 1, 1 );
}


/*
==============
RB_SurfaceSplash
==============
*/
static void RB_SurfaceSplash( void ) {
	vec3_t left, up;
	float radius;

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;

	VectorSet( left, -radius, 0, 0 );
	VectorSet( up, 0, radius, 0 );
	if ( backEnd.viewParms.portalView == PV_MIRROR ) {
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shader );
}

/*
==============
RB_SurfaceSprite
==============
*/
static void RB_SurfaceSprite( void ) {
	vec3_t		left, up;
	float		radius;

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;
	if ( backEnd.currentEntity->e.rotation == 0.0 ) {
		VectorScale( backEnd.viewParms.orientation.axis[1], radius, left );
		VectorScale( backEnd.viewParms.orientation.axis[2], radius, up );
	} else {
		float	s, c;
		float	ang;
		
		ang = M_PI * backEnd.currentEntity->e.rotation / 180.0;
		s = sin( ang );
		c = cos( ang );

		VectorScale( backEnd.viewParms.orientation.axis[1], c * radius, left );
		VectorMA( left, -s * radius, backEnd.viewParms.orientation.axis[2], left );

		VectorScale( backEnd.viewParms.orientation.axis[2], c * radius, up );
		VectorMA( up, s * radius, backEnd.viewParms.orientation.axis[1], up );
	}

	if ( backEnd.viewParms.portalView == PV_MIRROR ) {
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shader );
}


/*
=============
RB_SurfacePolychain
=============
*/
static void RB_SurfacePolychain( const srfPoly_t *p ) {
	int		i;
	int		numv;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( p->numVerts, 3*(p->numVerts - 2) );

	tess.surfType = SF_POLY;

	// fan triangles into the tess array
	numv = tess.numVertexes;
	for ( i = 0; i < p->numVerts; i++ ) {
		VectorCopy( p->verts[i].xyz, tess.xyz[numv] );
		tess.texCoords[0][numv][0] = p->verts[i].st[0];
		tess.texCoords[0][numv][1] = p->verts[i].st[1];
		tess.vertexColors[numv] = p->verts[ i ].modulate;

		numv++;
	}

	// generate fan indexes into the tess array
	for ( i = 0; i < p->numVerts-2; i++ ) {
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	}

	tess.numVertexes = numv;
}


/*
=============
RB_SurfaceTriangles
=============
*/
static void RB_SurfaceTriangles( const srfTriangles_t *srf ) {
	int			i;
	const drawVert_t	*dv;
	float		*xyz, *normal;
	float		*texCoords0;
	float		*texCoords1;
	uint32_t	*color;
#ifdef USE_LEGACY_DLIGHTS
	int			dlightBits;
#endif

#ifdef USE_VBO
#ifdef USE_LEGACY_DLIGHTS
	if ( tess.allowVBO && srf->vboItemIndex && !srf->dlightBits ) {
#else
	if ( tess.allowVBO && srf->vboItemIndex ) {
#endif
		// transition to vbo render list
		if ( tess.vboIndex == 0 ) {
			RB_EndSurface();
			RB_BeginSurface( tess.shader, tess.fogNum );
			// set some dummy parameters for RB_EndSurface
			tess.numIndexes = 1;
			tess.numVertexes = 0;
			VBO_ClearQueue();
		}
		tess.surfType = SF_TRIANGLES;
		tess.vboIndex = srf->vboItemIndex;
		VBO_QueueItem( srf->vboItemIndex );
		return; // no need to tesselate anything
	}

	VBO_Flush();
#endif // USE_VBO

	// ydnar: moved before overflow so dlights work properly
	RB_CHECKOVERFLOW( srf->numVerts, srf->numIndexes );

#ifdef USE_LEGACY_DLIGHTS
	dlightBits = srf->dlightBits;
	tess.dlightBits |= dlightBits;
#endif

	tess.surfType = SF_TRIANGLES;

	for ( i = 0 ; i < srf->numIndexes ; i += 3 ) {
		tess.indexes[ tess.numIndexes + i + 0 ] = tess.numVertexes + srf->indexes[ i + 0 ];
		tess.indexes[ tess.numIndexes + i + 1 ] = tess.numVertexes + srf->indexes[ i + 1 ];
		tess.indexes[ tess.numIndexes + i + 2 ] = tess.numVertexes + srf->indexes[ i + 2 ];
	}
	tess.numIndexes += srf->numIndexes;

	dv = srf->verts;
	xyz = tess.xyz[ tess.numVertexes ];
	normal = tess.normal[ tess.numVertexes ];
	texCoords0 = tess.texCoords[0][ tess.numVertexes ];
	texCoords1 = tess.texCoords[1][ tess.numVertexes ];
	color = &tess.vertexColors[ tess.numVertexes ].u32;

	for ( i = 0; i < srf->numVerts; i++, dv++, xyz += 4, normal += 4, texCoords0 += 2, color++ ) {
		xyz[0] = dv->xyz[0];
		xyz[1] = dv->xyz[1];
		xyz[2] = dv->xyz[2];

#ifdef USE_TESS_NEEDS_NORMAL
		if ( tess.needsNormal )
#endif
		{
			normal[0] = dv->normal[0];
			normal[1] = dv->normal[1];
			normal[2] = dv->normal[2];
		}

		texCoords0[0] = dv->st[0];
		texCoords0[1] = dv->st[1];

#ifdef USE_TESS_NEEDS_ST2
		if ( tess.needsST2 )
#endif
		{
			texCoords1[0] = dv->lightmap[0];
			texCoords1[1] = dv->lightmap[1];
			texCoords1 += 2;
		}

		*color = dv->color.u32;
	}

	tess.numVertexes += srf->numVerts;
}


/*
=============
RB_SurfaceFoliage - ydnar
=============
*/

void RB_SurfaceFoliage( const srfFoliage_t *srf ) {
	int o, i;// , numVerts, numIndexes;
#ifdef USE_DISTANCE_CULL
	int a;
	vec4_t distanceCull, distanceVector;
	float alpha, z, dist, fovScale;
	vec3_t viewOrigin, local;
#endif
	vec_t	*xyz, *normal;
	float	*texCoords0;
	float	*texCoords1;
	uint32_t	*color;
	int dlightBits;
	const foliageInstance_t   *instance;

#ifdef USE_VBO
#ifdef USE_LEGACY_DLIGHTS
	if ( tess.allowVBO && srf->vboItemIndex && !srf->dlightBits ) {
#else
	if ( tess.allowVBO && srf->vboItemIndex ) {
#endif
		// transition to vbo render list
		if ( tess.vboIndex == 0 ) {
			RB_EndSurface();
			RB_BeginSurface( tess.shader, tess.fogNum );
			// set some dummy parameters for RB_EndSurface
			tess.numIndexes = 1;
			tess.numVertexes = 0;
			VBO_ClearQueue();
		}
		tess.surfType = SF_FOLIAGE;
		tess.vboIndex = srf->vboItemIndex;
		VBO_QueueItem( srf->vboItemIndex );
		return; // no need to tesselate anything
	}
#endif

#ifdef USE_DISTANCE_CULL
	VectorCopy( backEnd.orientation.viewOrigin, viewOrigin );

	// set fov scale
	fovScale = backEnd.viewParms.fovX * ( 1.0 / 90.0 );

	// calculate distance vector
	VectorSubtract( backEnd.orientation.origin, backEnd.viewParms.orientation.origin, local );
	distanceVector[ 0 ] = -backEnd.orientation.modelMatrix[ 2 ];
	distanceVector[ 1 ] = -backEnd.orientation.modelMatrix[ 6 ];
	distanceVector[ 2 ] = -backEnd.orientation.modelMatrix[ 10 ];
	distanceVector[ 3 ] = DotProduct( local, backEnd.viewParms.orientation.axis[ 0 ] );

	// attempt distance cull
	Vector4Copy( tess.shader->distanceCull, distanceCull );
	if ( distanceCull[ 1 ] > 0 && !tr.mapLoading ) {
		//VectorSubtract( srf->localOrigin, viewOrigin, delta );
		//alpha = (distanceCull[ 1 ] - VectorLength( delta ) + srf->radius) * distanceCull[ 3 ];
		z = fovScale * ( DotProduct( srf->origin, distanceVector ) + distanceVector[ 3 ] - srf->radius );
		alpha = ( distanceCull[ 1 ] - z ) * distanceCull[ 3 ];
		if ( alpha < distanceCull[ 2 ] ) {
			return;
		}
	}
#endif

#ifdef USE_VBO
	VBO_Flush();
#endif

	// set dlight bits
	dlightBits = srf->dlightBits;
	tess.dlightBits |= dlightBits;

	// iterate through origin list
	instance = srf->instances;
	for ( o = 0; o < srf->numInstances; o++, instance++ )
	{
#ifdef USE_DISTANCE_CULL
		// fade alpha based on distance between inner and outer radii
		if ( distanceCull[ 1 ] > 0.0f && !tr.mapLoading ) {
			// calculate z distance
			z = fovScale * ( DotProduct( instance->origin, distanceVector ) + distanceVector[ 3 ] );
			if ( z < -64.0f ) {  // epsilon so close-by foliage doesn't pop in and out
				continue;
			}

			// check against frustum planes
			for ( i = 0; i < 5; i++ )
			{
				dist = DotProduct( instance->origin, backEnd.viewParms.frustum[ i ].normal ) - backEnd.viewParms.frustum[ i ].dist;
				if ( dist < -64.0 ) {
					break;
				}
			}
			if ( i != 5 ) {
				continue;
			}

			// radix
			if ( o & 1 ) {
				z *= 1.25;
				if ( o & 2 ) {
					z *= 1.25;
				}
			}

			// calculate alpha
			alpha = ( distanceCull[ 1 ] - z ) * distanceCull[ 3 ];
			if ( alpha < distanceCull[ 2 ] ) {
				continue;
			}

			// set color
			a = alpha > 1.0f ? 255 : alpha * 255;
#ifdef Q3_BIG_ENDIAN // LBO 3/15/05. Byte-swap fix for Mac - alpha is in the LSB.
			srcColor = ( *( (int*) instance->color ) & 0xFFFFFF00 ) | ( a & 0xff );
#else
			srcColor = ( *( (int*) instance->color ) & 0xFFFFFF ) | ( a << 24 );
#endif
		} else
#endif
		/*{
			srcColor = *( (int*) instance->color );
		}*/

		// Com_Printf( "Color: %d %d %d %d\n", srf->colors[ o ][ 0 ], srf->colors[ o ][ 1 ], srf->colors[ o ][ 2 ], alpha );

		if ( tess.numVertexes + srf->numVerts > SHADER_MAX_VERTEXES || tess.numIndexes + srf->numIndexes > SHADER_MAX_INDEXES ) {
			if ( tr.mapLoading ) {
				// estimate and flush
				if ( srf->vboItemIndex ) {
					VBO_PushData( srf->vboItemIndex, &tess );
					tess.numIndexes = 0;
					tess.numVertexes = 0;
				}
				else {
					ri.Error( ERR_DROP, "Unexpected foliage flush during map loading!\n" );
				}
			}
			else {
				RB_EndSurface();
				RB_BeginSurface( tess.shader, tess.fogNum );
#ifdef USE_LEGACY_DLIGHTS
				tess.dlightBits |= dlightBits;  // ydnar: for proper dlighting
#endif
			}
		}

		
		tess.surfType = SF_FOLIAGE;

		for (i = 0; i < srf->numIndexes; i++) {
			tess.indexes[ tess.numIndexes + i ] = srf->indexes[ i ] + tess.numVertexes;
		}

		xyz = tess.xyz[tess.numVertexes];
		normal = tess.normal[tess.numVertexes];
		texCoords0 = tess.texCoords[0][tess.numVertexes];
		texCoords1 = tess.texCoords[1][tess.numVertexes];
		color = &tess.vertexColors[ tess.numVertexes ].u32;

		for (i = 0; i < srf->numVerts; i++, xyz += 4, normal += 4, texCoords0 += 2, color++) {
			VectorAdd( srf->xyz[i], instance->origin, xyz );

#ifdef USE_TESS_NEEDS_NORMAL
			if (tess.needsNormal)
#endif
			{
				normal[0] = srf->normal[i][0];
				normal[1] = srf->normal[i][1];
				normal[2] = srf->normal[i][2];
			}

			texCoords0[0] = srf->texCoords[i][0];
			texCoords0[1] = srf->texCoords[i][1];

#ifdef USE_TESS_NEEDS_ST2
			if (tess.needsST2)
#endif
			{
				texCoords1[0] = srf->lmTexCoords[i][0];
				texCoords1[1] = srf->lmTexCoords[i][1];
				texCoords1 += 2;
			}

			*color = instance->color.u32;
		}

		// increment
		tess.numIndexes += srf->numIndexes;
		tess.numVertexes += srf->numVerts;
	}
}


/*
==============
RB_SurfaceBeam
==============
*/
static void RB_SurfaceBeam( void )
{
#define NUM_BEAM_SEGS 6
	const refEntity_t *e;
	int	i;
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t points[NUM_BEAM_SEGS+1][2]; // [startPoint,endPoint]
	vec3_t oldorigin, origin;

	e = &backEnd.currentEntity->e;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );

	VectorScale( perpvec, 4, perpvec );

	for ( i = 0; i <= NUM_BEAM_SEGS; i++ )
	{
		RotatePointAroundVector( points[i][0], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( points[i][0], direction, points[i][1] );
	}

	qglDisable( GL_TEXTURE_2D );

	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );

	qglColor4f( 1, 0, 0, 1 );

	GL_ClientState( 0, CLS_NONE );

	qglVertexPointer( 3, GL_FLOAT, 0, &points[0][0] );
	qglDrawArrays( GL_TRIANGLE_STRIP, 0, (NUM_BEAM_SEGS+1)*2 );

	qglEnable( GL_TEXTURE_2D );
}

//================================================================================

static void DoRailCore( const vec3_t start, const vec3_t end, const vec3_t up, float len, float spanWidth )
{
	float		spanWidth2;
	int			vbase;
	float		t; //= len / 256.0f;

	RB_CHECKOVERFLOW( 4, 6 );

	vbase = tess.numVertexes;

	// Gordon: configurable tile
	if ( backEnd.currentEntity->e.radius > 0 ) {
		t = len / backEnd.currentEntity->e.radius;
	} else {
		t = len / 256.f;
	}

	spanWidth2 = -spanWidth;

	// FIXME: use quad stamp?
	VectorMA( start, spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 0;
	tess.texCoords[0][tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes].rgba[0] = backEnd.currentEntity->e.shader.rgba[0];
	tess.vertexColors[tess.numVertexes].rgba[1] = backEnd.currentEntity->e.shader.rgba[1];
	tess.vertexColors[tess.numVertexes].rgba[2] = backEnd.currentEntity->e.shader.rgba[2];
	tess.vertexColors[tess.numVertexes].rgba[3] = backEnd.currentEntity->e.shader.rgba[3];
	tess.numVertexes++;

	VectorMA( start, spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 0;
	tess.texCoords[0][tess.numVertexes][1] = 1;
	tess.vertexColors[tess.numVertexes].rgba[0] = backEnd.currentEntity->e.shader.rgba[0];
	tess.vertexColors[tess.numVertexes].rgba[1] = backEnd.currentEntity->e.shader.rgba[1];
	tess.vertexColors[tess.numVertexes].rgba[2] = backEnd.currentEntity->e.shader.rgba[2];
	tess.vertexColors[tess.numVertexes].rgba[3] = backEnd.currentEntity->e.shader.rgba[3];
	tess.numVertexes++;

	VectorMA( end, spanWidth, up, tess.xyz[tess.numVertexes] );

	tess.texCoords[0][tess.numVertexes][0] = t;
	tess.texCoords[0][tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes].rgba[0] = backEnd.currentEntity->e.shader.rgba[0];
	tess.vertexColors[tess.numVertexes].rgba[1] = backEnd.currentEntity->e.shader.rgba[1];
	tess.vertexColors[tess.numVertexes].rgba[2] = backEnd.currentEntity->e.shader.rgba[2];
	tess.vertexColors[tess.numVertexes].rgba[3] = backEnd.currentEntity->e.shader.rgba[3];
	tess.numVertexes++;

	VectorMA( end, spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = t;
	tess.texCoords[0][tess.numVertexes][1] = 1;
	tess.vertexColors[tess.numVertexes].rgba[0] = backEnd.currentEntity->e.shader.rgba[0];
	tess.vertexColors[tess.numVertexes].rgba[1] = backEnd.currentEntity->e.shader.rgba[1];
	tess.vertexColors[tess.numVertexes].rgba[2] = backEnd.currentEntity->e.shader.rgba[2];
	tess.vertexColors[tess.numVertexes].rgba[3] = backEnd.currentEntity->e.shader.rgba[3];
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = vbase;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 2;

	tess.indexes[tess.numIndexes++] = vbase + 2;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 3;
}


static void DoRailDiscs( int numSegs, const vec3_t start, const vec3_t dir, const vec3_t right, const vec3_t up )
{
	int i;
	vec3_t	pos[4];
	vec3_t	v;
	int		spanWidth = r_railWidth->integer;
	float c, s;
	float		scale;

	if ( numSegs > 1 )
		numSegs--;
	if ( !numSegs )
		return;

	scale = 0.25;

	for ( i = 0; i < 4; i++ )
	{
		c = cos( DEG2RAD( 45 + i * 90 ) );
		s = sin( DEG2RAD( 45 + i * 90 ) );
		v[0] = ( right[0] * c + up[0] * s ) * scale * spanWidth;
		v[1] = ( right[1] * c + up[1] * s ) * scale * spanWidth;
		v[2] = ( right[2] * c + up[2] * s ) * scale * spanWidth;
		VectorAdd( start, v, pos[i] );

		if ( numSegs > 1 )
		{
			// offset by 1 segment if we're doing a long distance shot
			VectorAdd( pos[i], dir, pos[i] );
		}
	}

	for ( i = 0; i < numSegs; i++ )
	{
		int j;

		RB_CHECKOVERFLOW( 4, 6 );

		for ( j = 0; j < 4; j++ )
		{
			VectorCopy( pos[j], tess.xyz[tess.numVertexes] );
			tess.texCoords[0][tess.numVertexes][0] = ( j < 2 );
			tess.texCoords[0][tess.numVertexes][1] = ( j && j != 3 );
			tess.vertexColors[tess.numVertexes].rgba[0] = backEnd.currentEntity->e.shader.rgba[0];
			tess.vertexColors[tess.numVertexes].rgba[1] = backEnd.currentEntity->e.shader.rgba[1];
			tess.vertexColors[tess.numVertexes].rgba[2] = backEnd.currentEntity->e.shader.rgba[2];
			tess.numVertexes++;

			VectorAdd( pos[j], dir, pos[j] );
		}

		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 0;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 1;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 3;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 3;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 1;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 2;
	}
}


/*
** RB_SurfaceRailRinges
*/
static void RB_SurfaceRailRings( void ) {
	const refEntity_t *e;
	int			numSegs;
	int			len;
	vec3_t		vec;
	vec3_t		right, up;
	vec3_t		start, end;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, start );
	VectorCopy( e->origin, end );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	MakeNormalVectors( vec, right, up );
	numSegs = ( len ) / r_railSegmentLength->value;
	if ( numSegs <= 0 ) {
		numSegs = 1;
	}

	VectorScale( vec, r_railSegmentLength->value, vec );

	DoRailDiscs( numSegs, start, vec, right, up );
}


/*
** RB_SurfaceRailCore
*/
static void RB_SurfaceRailCore( void ) {
	const refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, start );
	VectorCopy( e->origin, end );

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.orientation.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.orientation.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	DoRailCore( start, end, right, len, e->frame > 0 ? e->frame : 1 );
}


/*
** RB_SurfaceLightningBolt
*/
static void RB_SurfaceLightningBolt( void ) {
	const refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;
	int			i;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, end );
	VectorCopy( e->origin, start );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.orientation.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.orientation.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	for ( i = 0 ; i < 4 ; i++ ) {
		vec3_t	temp;

		DoRailCore( start, end, right, len, 8 );
		RotatePointAroundVector( temp, vec, right, 45 );
		VectorCopy( temp, right );
	}
}




/*
** LerpMeshVertexes
*/
static void LerpMeshVertexes(md3Surface_t *surf, float backlerp)
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	float	*outXyz, *outNormal;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	outXyz = tess.xyz[tess.numVertexes];
	outNormal = tess.normal[tess.numVertexes];

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			newXyz += 4, newNormals += 4,
			outXyz += 4, outNormal += 4) 
		{

			outXyz[0] = newXyz[0] * newXyzScale;
			outXyz[1] = newXyz[1] * newXyzScale;
			outXyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			outNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			outNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
			outXyz += 4, outNormal += 4) 
		{
			//%	vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			outXyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			outXyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			outXyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			// ydnar: ok :)
			#if 0
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= ( FUNCTABLE_SIZE / 256 );
			lng *= ( FUNCTABLE_SIZE / 256 );
			uncompressedNewNormal[0] = tr.sinTable[( lat + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[( lng + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= ( FUNCTABLE_SIZE / 256 );
			lng *= ( FUNCTABLE_SIZE / 256 );

			uncompressedOldNormal[0] = tr.sinTable[( lat + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[( lng + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK];

			outNormal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			outNormal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			outNormal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;
			#else
			lat = Q_ftol( ( ( ( oldNormals[ 0 ] >> 8 ) & 0xFF ) * ( FUNCTABLE_SIZE / 256 ) * newNormalScale ) +
						  ( ( ( oldNormals[ 0 ] >> 8 ) & 0xFF ) * ( FUNCTABLE_SIZE / 256 ) * oldNormalScale ) );
			lng = Q_ftol( ( ( oldNormals[ 0 ] & 0xFF ) * ( FUNCTABLE_SIZE / 256 ) * newNormalScale ) +
						  ( ( oldNormals[ 0 ] & 0xFF ) * ( FUNCTABLE_SIZE / 256 ) * oldNormalScale ) );

			outNormal[ 0 ] = tr.sinTable[ ( lat + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK ] * tr.sinTable[ lng ];
			outNormal[ 1 ] = tr.sinTable[ lat ] * tr.sinTable[ lng ];
			outNormal[ 2 ] = tr.sinTable[ ( lng + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK ];
			#endif

//			VectorNormalize (outNormal);
		}

		// ydnar: unecessary because of lat/lng lerping
		//%	VectorArrayNormalize((vec4_t *)tess.normal[tess.numVertexes].v, numVerts);
	}
}

/*
=============
RB_SurfaceMesh
=============
*/
static void RB_SurfaceMesh(md3Surface_t *surface) {
	int				j;
	float			backlerp;
	int				*triangles;
	float			*texCoords;
	int				indexes;
	int				Bob, Doug;
	int				numVerts;

	// RF, check for REFLAG_HANDONLY
	if ( backEnd.currentEntity->e.reFlags & REFLAG_ONLYHAND ) {
		if ( !strstr( surface->name, "hand" ) ) {
			return;
		}
	}

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( surface->numVerts, surface->numTriangles * 3 );

	tess.surfType = SF_MD3;

	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

	LerpMeshVertexes (surface, backlerp);

	triangles = (int *) ((byte *)surface + surface->ofsTriangles);
	indexes = surface->numTriangles * 3;
	Bob = tess.numIndexes;
	Doug = tess.numVertexes;
	for (j = 0 ; j < indexes ; j++) {
		tess.indexes[Bob + j] = Doug + triangles[j];
	}
	tess.numIndexes += indexes;

	texCoords = (float *) ((byte *)surface + surface->ofsSt);

	numVerts = surface->numVerts;
	for ( j = 0; j < numVerts; j++ ) {
		tess.texCoords[0][Doug + j][0] = texCoords[j*2+0];
		tess.texCoords[0][Doug + j][1] = texCoords[j*2+1];
		// FIXME: fill in lightmapST for completeness?
	}

	tess.numVertexes += surface->numVerts;

}

/*
** R_LatLongToNormal
*/
void R_LatLongToNormal( vec3_t outNormal, short latLong ) {
	unsigned lat, lng;

	lat = ( latLong >> 8 ) & 0xff;
	lng = ( latLong & 0xff );
	lat *= ( FUNCTABLE_SIZE / 256 );
	lng *= ( FUNCTABLE_SIZE / 256 );

	// decode X as cos( lat ) * sin( long )
	// decode Y as sin( lat ) * sin( long )
	// decode Z as cos( long )

	outNormal[0] = tr.sinTable[( lat + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK] * tr.sinTable[lng];
	outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
	outNormal[2] = tr.sinTable[( lng + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK];
}

// Ridah
/*
** LerpCMeshVertexes
*/
static void LerpCMeshVertexes( mdcSurface_t *surf, float backlerp ) {
	short   *oldXyz, *newXyz, *oldNormals, *newNormals;
	float   *outXyz, *outNormal;
	float oldXyzScale, newXyzScale;
	float oldNormalScale, newNormalScale;
	int vertNum;
	unsigned lat, lng;
	int numVerts;

	int oldBase, newBase;
	short   *oldComp = NULL, *newComp = NULL; // TTimo: init
	mdcXyzCompressed_t *oldXyzComp = NULL, *newXyzComp = NULL; // TTimo: init
	vec3_t oldOfsVec, newOfsVec;

	qboolean hasComp;

	outXyz = tess.xyz[tess.numVertexes];
	outNormal = tess.normal[tess.numVertexes];

	newBase = (int)*( ( short * )( (byte *)surf + surf->ofsFrameBaseFrames ) + backEnd.currentEntity->e.frame );
	newXyz = ( short * )( (byte *)surf + surf->ofsXyzNormals )
			 + ( newBase * surf->numVerts * 4 );
	newNormals = newXyz + 3;

	hasComp = ( surf->numCompFrames > 0 );
	if ( hasComp ) {
		newComp = ( ( short * )( (byte *)surf + surf->ofsFrameCompFrames ) + backEnd.currentEntity->e.frame );
		if ( *newComp >= 0 ) {
			newXyzComp = ( mdcXyzCompressed_t * )( (byte *)surf + surf->ofsXyzCompressed )
						 + ( *newComp * surf->numVerts );
		}
	}

	newXyzScale = MD3_XYZ_SCALE * ( 1.0 - backlerp );
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		//
		// just copy the vertexes
		//
		for ( vertNum = 0 ; vertNum < numVerts ; vertNum++,
			  newXyz += 4, newNormals += 4,
			  outXyz += 4, outNormal += 4 )
		{

			outXyz[0] = newXyz[0] * newXyzScale;
			outXyz[1] = newXyz[1] * newXyzScale;
			outXyz[2] = newXyz[2] * newXyzScale;

			// add the compressed ofsVec
			if ( hasComp && *newComp >= 0 ) {
				R_MDC_DecodeXyzCompressed( newXyzComp->ofsVec, newOfsVec, outNormal );
				newXyzComp++;
				VectorAdd( outXyz, newOfsVec, outXyz );
			} else {
				lat = ( newNormals[0] >> 8 ) & 0xff;
				lng = ( newNormals[0] & 0xff );
				lat *= ( FUNCTABLE_SIZE / 256 );    // was 4 :sigh:
				lng *= ( FUNCTABLE_SIZE / 256 );

				outNormal[0] = tr.sinTable[( lat + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK] * tr.sinTable[lng];
				outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
				outNormal[2] = tr.sinTable[( lng + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK];

				// ydnar: testing anorms table
				//%	VectorCopy( r_anormals[ (lng & (0xF << 4)) | ((lat >> 4) & 0xF) ], outNormal );
			}
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldBase = (int)*( ( short * )( (byte *)surf + surf->ofsFrameBaseFrames ) + backEnd.currentEntity->e.oldframe );
		oldXyz = ( short * )( (byte *)surf + surf->ofsXyzNormals )
				 + ( oldBase * surf->numVerts * 4 );
		oldNormals = oldXyz + 3;

		if ( hasComp ) {
			oldComp = ( ( short * )( (byte *)surf + surf->ofsFrameCompFrames ) + backEnd.currentEntity->e.oldframe );
			if ( *oldComp >= 0 ) {
				oldXyzComp = ( mdcXyzCompressed_t * )( (byte *)surf + surf->ofsXyzCompressed )
							 + ( *oldComp * surf->numVerts );
			}
		}

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for ( vertNum = 0 ; vertNum < numVerts ; vertNum++,
			  oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
			  outXyz += 4, outNormal += 4 )
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			outXyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			outXyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			outXyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// add the compressed ofsVec
			if ( hasComp && *newComp >= 0 ) {
				R_MDC_DecodeXyzCompressed( newXyzComp->ofsVec, newOfsVec, uncompressedNewNormal );
				newXyzComp++;
				VectorMA( outXyz, 1.0 - backlerp, newOfsVec, outXyz );
			} else {
				lat = ( newNormals[0] >> 8 ) & 0xff;
				lng = ( newNormals[0] & 0xff );
				lat *= ( FUNCTABLE_SIZE / 256 );    // was 4 :sigh:
				lng *= ( FUNCTABLE_SIZE / 256 );

				uncompressedNewNormal[0] = tr.sinTable[( lat + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK] * tr.sinTable[lng];
				uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
				uncompressedNewNormal[2] = tr.sinTable[( lng + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK];
			}

			if ( hasComp && *oldComp >= 0 ) {
				R_MDC_DecodeXyzCompressed( oldXyzComp->ofsVec, oldOfsVec, uncompressedOldNormal );
				oldXyzComp++;
				VectorMA( outXyz, backlerp, oldOfsVec, outXyz );
			} else {
				lat = ( oldNormals[0] >> 8 ) & 0xff;
				lng = ( oldNormals[0] & 0xff );
				lat *= ( FUNCTABLE_SIZE / 256 );    // was 4 :sigh:
				lng *= ( FUNCTABLE_SIZE / 256 );

				uncompressedOldNormal[0] = tr.sinTable[( lat + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK] * tr.sinTable[lng];
				uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
				uncompressedOldNormal[2] = tr.sinTable[( lng + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK];
			}

			outNormal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			outNormal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			outNormal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

			// ydnar: wee bit faster (fixme: use lat/lng lerping)
			//%	VectorNormalize (outNormal);
			VectorNormalizeFast( outNormal );
		}
	}
}

/*
=============
RB_SurfaceCMesh
=============
*/
static void RB_SurfaceCMesh( mdcSurface_t *surface ) {
	int j;
	float backlerp;
	int             *triangles;
	float           *texCoords;
	int indexes;
	int Bob, Doug;
	int numVerts;

	// RF, check for REFLAG_HANDONLY
	if ( backEnd.currentEntity->e.reFlags & REFLAG_ONLYHAND ) {
		if ( !strstr( surface->name, "hand" ) ) {
			return;
		}
	}

#ifdef USE_VBO
	VBO_Flush();
#endif

	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

	RB_CHECKOVERFLOW( surface->numVerts, surface->numTriangles * 3 );

	tess.surfType = SF_MDC;

	LerpCMeshVertexes( surface, backlerp );

	triangles = ( int * )( (byte *)surface + surface->ofsTriangles );
	indexes = surface->numTriangles * 3;
	Bob = tess.numIndexes;
	Doug = tess.numVertexes;
	for ( j = 0 ; j < indexes ; j++ ) {
		tess.indexes[Bob + j] = Doug + triangles[j];
	}
	tess.numIndexes += indexes;

	texCoords = ( float * )( (byte *)surface + surface->ofsSt );

	numVerts = surface->numVerts;
	for ( j = 0; j < numVerts; j++ ) {
		tess.texCoords[0][Doug + j][0] = texCoords[j * 2 + 0];
		tess.texCoords[0][Doug + j][1] = texCoords[j * 2 + 1];
		// FIXME: fill in lightmapST for completeness?
	}

	tess.numVertexes += surface->numVerts;

}
// done.

/*
==============
RB_SurfaceFace
==============
*/
static void RB_SurfaceFace( const srfSurfaceFace_t *surf ) {
	int			i;
	unsigned	*indices;
	glIndex_t	*tessIndexes;
	const float	*v;
	//const float	*normal;
	int			ndx;
	int			Bob;
	int			numPoints;
#ifdef USE_LEGACY_DLIGHTS
	int			dlightBits;
#endif

#ifdef USE_VBO
#ifdef USE_LEGACY_DLIGHTS
	if ( tess.allowVBO && surf->vboItemIndex && !surf->dlightBits ) {
#else
	if ( tess.allowVBO && surf->vboItemIndex ) {
#endif
		// transition to vbo render list
		if ( tess.vboIndex == 0 ) {
			RB_EndSurface();
			RB_BeginSurface( tess.shader, tess.fogNum );
			// set some dummy parameters for RB_EndSurface
			tess.numIndexes = 1;
			tess.numVertexes = 0;
			VBO_ClearQueue();
		}
		tess.surfType = SF_FACE;
		tess.vboIndex = surf->vboItemIndex;
		VBO_QueueItem( surf->vboItemIndex );
		return; // no need to tesselate anything
	}

	VBO_Flush();
#endif // USE_VBO

	RB_CHECKOVERFLOW( surf->numPoints, surf->numIndices );

	tess.surfType = SF_FACE;

#ifdef USE_LEGACY_DLIGHTS
	dlightBits = surf->dlightBits;
	tess.dlightBits |= dlightBits;
#endif

	indices = ( unsigned * ) ( ( ( char  * ) surf ) + surf->ofsIndices );

	Bob = tess.numVertexes;
	tessIndexes = tess.indexes + tess.numIndexes;
	for ( i = surf->numIndices-1 ; i >= 0  ; i-- ) {
		tessIndexes[i] = indices[i] + Bob;
	}

	tess.numIndexes += surf->numIndices;

	numPoints = surf->numPoints;

/*#ifdef USE_TESS_NEEDS_NORMAL
	if ( tess.needsNormal )
#endif
	{
		if ( surf->normals ) {
			// per-vertex normals for non-coplanar faces
			memcpy( &tess.normal[ tess.numVertexes ], surf->normals, numPoints * sizeof( vec4_t ) );
		} else {
			normal = surf->plane.normal;
			for ( i = 0, ndx = tess.numVertexes; i < numPoints; i++, ndx++ ) {
				VectorCopy( normal, tess.normal[ndx] );
			}
		}
	}*/

	for ( i = 0, v = surf->points[0], ndx = tess.numVertexes; i < numPoints; i++, v += VERTEXSIZE, ndx++ ) {
		VectorCopy( v, tess.xyz[ndx]);
		tess.texCoords[0][ndx][0] = v[3];
		tess.texCoords[0][ndx][1] = v[4];
#ifdef USE_TESS_NEEDS_ST2
		if ( tess.needsST2 )
#endif
		{
			tess.texCoords[1][ndx][0] = v[5];
			tess.texCoords[1][ndx][1] = v[6];
		}
		* ( unsigned int * ) &tess.vertexColors[ndx] = * ( unsigned int * ) &v[7];
	}

	tess.numVertexes += surf->numPoints;
}


static float    LodErrorForVolume( vec3_t local, float radius ) {
	vec3_t world;
	float d;

	// never let it go lower than 1
	if ( r_lodCurveError->value < 1 ) {
		return 1;
	}

	world[0] = local[0] * backEnd.orientation.axis[0][0] + local[1] * backEnd.orientation.axis[1][0] +
			   local[2] * backEnd.orientation.axis[2][0] + backEnd.orientation.origin[0];
	world[1] = local[0] * backEnd.orientation.axis[0][1] + local[1] * backEnd.orientation.axis[1][1] +
			   local[2] * backEnd.orientation.axis[2][1] + backEnd.orientation.origin[1];
	world[2] = local[0] * backEnd.orientation.axis[0][2] + local[1] * backEnd.orientation.axis[1][2] +
			   local[2] * backEnd.orientation.axis[2][2] + backEnd.orientation.origin[2];

	VectorSubtract( world, backEnd.viewParms.orientation.origin, world );
	d = DotProduct( world, backEnd.viewParms.orientation.axis[0] );

	if ( d < 0 ) {
		d = -d;
	}
	d -= radius;
	if ( d < 1 ) {
		d = 1;
	}

	return r_lodCurveError->value / d;
}

#ifdef USE_VBO_GRID
void RB_SurfaceGridEstimate( srfGridMesh_t *cv, int *numVertexes, int *numIndexes )
{
	int		lodWidth, lodHeight;
	float	lodError;
	int		i, used, rows;
	int		nVertexes = 0;
	int		nIndexes = 0;
	int		irows, vrows;

	lodError = r_lodCurveError->value; // fixed quality for VBO

	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			lodWidth++;
		}
	}
	lodWidth++;

	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			lodHeight++;
		}
	}
	lodHeight++;

	used = 0;
	while ( used < lodHeight - 1 ) {
		// see how many rows of both verts and indexes we can add without overflowing
		do {
			vrows = ( SHADER_MAX_VERTEXES - tess.numVertexes ) / lodWidth;
			irows = ( SHADER_MAX_INDEXES - tess.numIndexes ) / ( lodWidth * 6 );

			// if we don't have enough space for at least one strip, flush the buffer
			if ( vrows < 2 || irows < 1 ) {
				nVertexes += tess.numVertexes;
				nIndexes += tess.numIndexes;
				tess.numIndexes = 0;
				tess.numVertexes = 0;
			} else {
				break;
			}
		} while ( 1 );
		
		rows = irows;
		if ( vrows < irows + 1 ) {
			rows = vrows - 1;
		}
		if ( used + rows > lodHeight ) {
			rows = lodHeight - used;
		}

		tess.numIndexes += (rows-1)*(lodWidth-1)*6;
		tess.numVertexes += rows * lodWidth;
		used += rows - 1;
	}

	*numVertexes = nVertexes + tess.numVertexes;
	*numIndexes = nIndexes + tess.numIndexes;
	tess.numVertexes = 0;
	tess.numIndexes = 0;
}
#endif // USE_VBO_GRID

/*
=============
RB_SurfaceGrid

Just copy the grid of points and triangulate
=============
*/
static void RB_SurfaceGrid( srfGridMesh_t *cv ) {
	int		i, j;
	float	*xyz;
	float	*texCoords0;
	float	*texCoords1;
	float	*normal;
	uint32_t *color;
	drawVert_t *dv;
	int		rows, irows, vrows;
	int		used;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		numVertexes;
#ifdef USE_LEGACY_DLIGHTS
	int		dlightBits;
#endif

#ifdef USE_VBO_GRID
#ifdef USE_LEGACY_DLIGHTS
	if ( tess.allowVBO && cv->vboItemIndex && !cv->dlightBits ) {
#else
	if ( tess.allowVBO && cv->vboItemIndex ) {
#endif
		// transition to vbo render list
		if ( tess.vboIndex == 0 ) {
			RB_EndSurface();
			RB_BeginSurface( tess.shader, tess.fogNum );
			// set some dummy parameters for RB_EndSurface
			tess.numIndexes = 1;
			tess.numVertexes = 0;
			VBO_ClearQueue();
		}
		tess.surfType = SF_GRID;
		tess.vboIndex = cv->vboItemIndex;
		VBO_QueueItem( cv->vboItemIndex );
		return; // no need to tesselate anything
	}

	VBO_Flush();
#else
#ifdef USE_VBO
	VBO_Flush();
#endif
#endif

#ifdef USE_LEGACY_DLIGHTS
	dlightBits = cv->dlightBits;
	tess.dlightBits |= dlightBits;
#endif

	tess.surfType = SF_GRID;

	// determine the allowable discrepance
#ifdef USE_PMLIGHT
	if ( cv->vboItemIndex && ( tr.mapLoading || ( tess.dlightPass && tess.shader->isStaticShader ) ) )
#else
	if ( cv->vboItemIndex && tr.mapLoading )
#endif
		lodError = r_lodCurveError->value; // fixed quality for VBO
	else
		lodError = LodErrorForVolume( cv->lodOrigin, cv->lodRadius );

	// determine which rows and columns of the subdivision
	// we are actually going to use
	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}
	widthTable[lodWidth] = cv->width-1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}
	heightTable[lodHeight] = cv->height-1;
	lodHeight++;

	// very large grids may have more points or indexes than can be fit
	// in the tess structure, so we may have to issue it in multiple passes

	used = 0;
	rows = 0;
	while ( used < lodHeight - 1 ) {
		// see how many rows of both verts and indexes we can add without overflowing
		do {
			vrows = ( SHADER_MAX_VERTEXES - tess.numVertexes ) / lodWidth;
			irows = ( SHADER_MAX_INDEXES - tess.numIndexes ) / ( lodWidth * 6 );

			// if we don't have enough space for at least one strip, flush the buffer
			if ( vrows < 2 || irows < 1 ) {
				if ( tr.mapLoading ) {
#ifdef USE_VBO_GRID
					// estimate and flush
					if ( cv->vboItemIndex ) {
						VBO_PushData( cv->vboItemIndex, &tess );
						tess.numIndexes = 0;
						tess.numVertexes = 0;
					} else
#endif
					ri.Error( ERR_DROP, "Unexpected grid flush during map loading!\n" );
				} else {
					RB_EndSurface();
					RB_BeginSurface( tess.shader, tess.fogNum );
#ifdef USE_LEGACY_DLIGHTS
					tess.dlightBits |= dlightBits;  // ydnar: for proper dlighting
#endif
				}
			} else {
				break;
			}
		} while ( 1 );

		rows = irows;
		if ( vrows < irows + 1 ) {
			rows = vrows - 1;
		}
		if ( used + rows > lodHeight ) {
			rows = lodHeight - used;
		}

		numVertexes = tess.numVertexes;

		xyz = tess.xyz[numVertexes];
		normal = tess.normal[numVertexes];
		texCoords0 = tess.texCoords[0][numVertexes];
		texCoords1 = tess.texCoords[1][numVertexes];
		color = &tess.vertexColors[numVertexes].u32;
		for ( i = 0 ; i < rows ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++ ) {
				dv = cv->verts + heightTable[ used + i ] * cv->width
					+ widthTable[ j ];

				xyz[0] = dv->xyz[0];
				xyz[1] = dv->xyz[1];
				xyz[2] = dv->xyz[2];
				texCoords0[0] = dv->st[0];
				texCoords0[1] = dv->st[1];
#ifdef USE_TESS_NEEDS_ST2
				if ( tess.needsST2 )
#endif
				{
					texCoords1[0] = dv->lightmap[0];
					texCoords1[1] = dv->lightmap[1];
					texCoords1 += 2;
				}
#ifdef USE_TESS_NEEDS_NORMAL
				if ( tess.needsNormal )
#endif
				{
					normal[0] = dv->normal[0];
					normal[1] = dv->normal[1];
					normal[2] = dv->normal[2];
					normal += 4;
				}
				*color = dv->color.u32;
				xyz += 4;
				texCoords0 += 2;
				color++;
			}
		}

		// add the indexes
		{
			int		numIndexes;
			int		w, h;

			h = rows - 1;
			w = lodWidth - 1;
			numIndexes = tess.numIndexes;
			for (i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;
			
					// vertex order to be reckognized as tristrips
					v1 = numVertexes + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;

					tess.indexes[numIndexes] = v2;
					tess.indexes[numIndexes+1] = v3;
					tess.indexes[numIndexes+2] = v1;
					
					tess.indexes[numIndexes+3] = v1;
					tess.indexes[numIndexes+4] = v3;
					tess.indexes[numIndexes+5] = v4;
					numIndexes += 6;
				}
			}

			tess.numIndexes = numIndexes;
		}

		tess.numVertexes += rows * lodWidth;

		used += rows - 1;
	}
}


/*
===========================================================================

NULL MODEL

===========================================================================
*/

/*
===================
RB_SurfaceAxis

Draws x/y/z lines from the origin for orientation debugging
===================
*/
static void RB_SurfaceAxis( void ) {
	vec3_t xyz[6];
	color4ub_t colors[6];
	int i;

	GL_ClientState( 0, CLS_COLOR_ARRAY );

	qglDisable( GL_TEXTURE_2D );
	GL_State( GLS_DEFAULT );

	qglLineWidth( 3 );

	Com_Memset( xyz, 0, sizeof( xyz ) );
	xyz[1][0] = 16.0;
	xyz[3][1] = 16.0;
	xyz[5][2] = 16.0;

	Com_Memset( colors, 0, sizeof( colors ) );
	for ( i = 0; i < 6; i++ ) {
		colors[i].rgba[3] = 255;
	}

	colors[0].rgba[0] = 255;
	colors[1].rgba[0] = 255;
	colors[2].rgba[1] = 255;
	colors[3].rgba[1] = 255;
	colors[4].rgba[2] = 255;
	colors[5].rgba[2] = 255;

	qglVertexPointer( 3, GL_FLOAT, 0, xyz );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colors[0].rgba );

	qglDrawArrays( GL_LINES, 0, 6 );

	qglLineWidth( 1 );

	qglEnable( GL_TEXTURE_2D );
}

//===========================================================================

/*
====================
RB_SurfaceEntity

Entities that have a single procedurally generated surface
====================
*/
static void RB_SurfaceEntity( surfaceType_t *surfType ) {
#ifdef USE_VBO
	VBO_Flush();
#endif
	switch( backEnd.currentEntity->e.reType ) {
	case RT_SPLASH:
		RB_SurfaceSplash();
		break;
	case RT_SPRITE:
		RB_SurfaceSprite();
		break;
	case RT_BEAM:
		RB_SurfaceBeam();
		break;
	case RT_RAIL_CORE:
		RB_SurfaceRailCore();
		break;
	case RT_RAIL_RINGS:
		RB_SurfaceRailRings();
		break;
	case RT_LIGHTNING:
		RB_SurfaceLightningBolt();
		break;
	default:
		RB_SurfaceAxis();
		break;
	}
	tess.surfType = SF_ENTITY;
}


static void RB_SurfaceBad( surfaceType_t *surfType ) {
	tess.surfType = SF_BAD;
	ri.Printf( PRINT_ALL, "Bad surface tesselated.\n" );
}

#if 0

void RB_SurfaceFlare( srfFlare_t *surf ) {
	vec3_t left, up;
	float radius;
	byte color[4];
	vec3_t dir;
	vec3_t origin;
	float d;

#ifdef USE_VBO
	VBO_Flush();
#endif

	// calculate the xyz locations for the four corners
	radius = 30;
	VectorScale( backEnd.viewParms.orientation.axis[1], radius, left );
	VectorScale( backEnd.viewParms.orientation.axis[2], radius, up );
	if ( backEnd.viewParms.portalView == PV_MIRROR ) {
		VectorSubtract( vec3_origin, left, left );
	}

	color[0] = color[1] = color[2] = color[3] = 255;

	VectorMA( surf->origin, 3, surf->normal, origin );
	VectorSubtract( origin, backEnd.viewParms.orientation.origin, dir );
	VectorNormalize( dir );
	VectorMA( origin, r_ignore->value, dir, origin );

	d = -DotProduct( dir, surf->normal );
	if ( d < 0 ) {
		return;
	}
#if 0
	color[0] *= d;
	color[1] *= d;
	color[2] *= d;
#endif

	RB_AddQuadStamp( origin, left, up, color );
}

#else

void RB_SurfaceFlare( srfFlare_t *surf ) {
#if 0
	vec3_t left, up;
	byte color[4];

	color[0] = surf->color[0] * 255;
	color[1] = surf->color[1] * 255;
	color[2] = surf->color[2] * 255;
	color[3] = 255;

	VectorClear( left );
	VectorClear( up );

	left[0] = r_ignore->value;

	up[1] = r_ignore->value;

	RB_AddQuadStampExt( surf->origin, left, up, color, 0, 0, 1, 1 );
#endif
}

#endif


static void RB_SurfacePolyBuffer( srfPolyBuffer_t *surf ) {
	int		i;
	int		numv;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( surf->pPolyBuffer->numVerts, surf->pPolyBuffer->numIndicies );

	tess.surfType = SF_POLYBUFFER;

	numv = tess.numVertexes;
	for ( i = 0; i < surf->pPolyBuffer->numVerts; i++ ) {
		VectorCopy( surf->pPolyBuffer->xyz[i], tess.xyz[numv] );
		tess.texCoords[0][numv][0] = surf->pPolyBuffer->st[i][0];
		tess.texCoords[0][numv][1] = surf->pPolyBuffer->st[i][1];
		tess.vertexColors[numv] = surf->pPolyBuffer->color[i];

		numv++;
	}

	for ( i = 0; i < surf->pPolyBuffer->numIndicies; i++ ) {
		tess.indexes[tess.numIndexes++] = tess.numVertexes + surf->pPolyBuffer->indicies[i];
	}

	tess.numVertexes = numv;
}

// ydnar: decal surfaces
void RB_SurfaceDecal( srfDecal_t *srf ) {
	int i;
	int numv;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( srf->numVerts, 3 * ( srf->numVerts - 2 ) );

	tess.surfType = SF_DECAL;

	// fan triangles into the tess array
	numv = tess.numVertexes;
	for ( i = 0; i < srf->numVerts; i++ )
	{
		VectorCopy( srf->verts[ i ].xyz, tess.xyz[ numv ] );
		tess.texCoords[ 0 ][ numv ][ 0 ] = srf->verts[ i ].st[ 0 ];
		tess.texCoords[ 0 ][ numv ][ 1 ] = srf->verts[ i ].st[ 1 ];
		tess.vertexColors[ numv ] = srf->verts[ i ].modulate;
		numv++;
	}

	/* generate fan indexes into the tess array */
	for ( i = 0; i < srf->numVerts - 2; i++ )
	{
		tess.indexes[ tess.numIndexes + 0 ] = tess.numVertexes;
		tess.indexes[ tess.numIndexes + 1 ] = tess.numVertexes + i + 1;
		tess.indexes[ tess.numIndexes + 2 ] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	}

	tess.numVertexes = numv;
}


static void RB_SurfaceSkip( void *surf ) {
	tess.surfType = SF_SKIP;
}


void( *rb_surfaceTable[SF_NUM_SURFACE_TYPES] ) ( void * ) = {
	( void( * ) ( void* ) )RB_SurfaceBad,          // SF_BAD,
	( void( * ) ( void* ) )RB_SurfaceSkip,         // SF_SKIP,
	( void( * ) ( void* ) )RB_SurfaceFace,         // SF_FACE,
	( void( * ) ( void* ) )RB_SurfaceGrid,         // SF_GRID,
	( void( * ) ( void* ) )RB_SurfaceTriangles,    // SF_TRIANGLES,
	( void( * ) ( void* ) )RB_SurfaceFoliage,      // SF_FOLIAGE,
	( void( * ) ( void* ) )RB_SurfacePolychain,    // SF_POLY,
	( void( * ) ( void* ) )RB_SurfaceMesh,         // SF_MD3,
	( void( * ) ( void* ) )RB_SurfaceCMesh,        // SF_MDC,
	( void( * ) ( void* ) )RB_SurfaceAnim,         // SF_MDS,
	( void( * ) ( void* ) )RB_MDM_SurfaceAnim,     // SF_MDM,
	( void( * ) ( void* ) )RB_IQMSurfaceAnim,      // SF_IQM,
	( void( * ) ( void* ) )RB_SurfaceFlare,        // SF_FLARE,
	( void( * ) ( void* ) )RB_SurfaceEntity,       // SF_ENTITY
	( void( * ) ( void* ) )RB_SurfacePolyBuffer,   // SF_POLYBUFFER
	( void( * ) ( void* ) )RB_SurfaceDecal,        // SF_DECAL
};


