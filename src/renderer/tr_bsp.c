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
// tr_map.c

#include "tr_local.h"

/*

Loads and prepares a map file for scene rendering.

A single entry point:

void RE_LoadWorldMap( const char *name );

*/

static	world_t		s_worldData;
static	byte		*fileBase;

static int	c_gridVerts;

//===============================================================================

static void HSVtoRGB( float h, float s, float v, float rgb[3] )
{
	int i;
	float f;
	float p, q, t;

	h *= 5;

	i = floor( h );
	f = h - i;

	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );

	switch ( i )
	{
	case 0:
		rgb[0] = v;
		rgb[1] = t;
		rgb[2] = p;
		break;
	case 1:
		rgb[0] = q;
		rgb[1] = v;
		rgb[2] = p;
		break;
	case 2:
		rgb[0] = p;
		rgb[1] = v;
		rgb[2] = t;
		break;
	case 3:
		rgb[0] = p;
		rgb[1] = q;
		rgb[2] = v;
		break;
	case 4:
		rgb[0] = t;
		rgb[1] = p;
		rgb[2] = v;
		break;
	case 5:
		rgb[0] = v;
		rgb[1] = p;
		rgb[2] = q;
		break;
	}
}


/*
===============
R_ClampDenorm

Clamp fp values that may result in denormalization after further multiplication
===============
*/
float R_ClampDenorm( float v ) {
	if ( fabsf( v ) > 0.0f && fabsf( v ) < 1e-9f ) {
		return 0.0f;
	} else {
		return v;
	}
}


/*
===============
R_ColorShiftLightingBytes
===============
*/
static void R_ColorShiftLightingBytes( const byte in[4], byte out[4], qboolean hasAlpha ) {
	int		shift, r, g, b;

	// shift the color data based on overbright range
	shift = r_mapOverBrightBits->integer - tr.overbrightBits;

	// shift the data based on overbright range
	if ( shift >= 0 ) {
		r = in[0] << shift;
		g = in[1] << shift;
		b = in[2] << shift;
		// normalize by color instead of saturating to white
		if ( ( r | g | b ) > 255 ) {
			int max = r > g ? r : g;
			max = max > b ? max : b;
			r = r * 255 / max;
			g = g * 255 / max;
			b = b * 255 / max;
		}
	} else {
		r = in[0] >> -shift;
		g = in[1] >> -shift;
		b = in[2] >> -shift;
	}

	if ( r_mapGreyScale->integer ) {
		const byte luma = LUMA( r, g, b );
		out[0] = luma;
		out[1] = luma;
		out[2] = luma;
	} else if( r_mapGreyScale->value ) {
		const float scale = fabs( r_mapGreyScale->value );
		const float luma = LUMA( r, g, b );
		out[0] = LERP( r, luma, scale );
		out[1] = LERP( g, luma, scale );
		out[2] = LERP( b, luma, scale );
	} else {
		out[0] = r;
		out[1] = g;
		out[2] = b;
	}

	if ( hasAlpha ) {
		out[3] = in[3];
	}
}


/*
===============
R_ProcessLightmap

	returns maxIntensity
===============
*/
float R_ProcessLightmap( byte **pic, int in_padding, int width, int height, byte **pic_out ) {
	int j;
	float maxIntensity = 0;
	//double sumIntensity = 0;

	if ( r_lightmap->integer > 1 ) { // color code by intensity as development tool	(FIXME: check range)
		for ( j = 0; j < width * height; j++ )
		{
			float r = ( *pic )[j * in_padding + 0];
			float g = ( *pic )[j * in_padding + 1];
			float b = ( *pic )[j * in_padding + 2];
			float intensity;
			float out[3] = {0.0f};

			intensity = 0.33f * r + 0.685f * g + 0.063f * b;

			if ( intensity > 255 ) {
				intensity = 1.0f;
			} else {
				intensity /= 255.0f;
			}

			if ( intensity > maxIntensity ) {
				maxIntensity = intensity;
			}

			HSVtoRGB( intensity, 1.00, 0.50, out );

			if ( r_lightmap->integer == 3 ) {
				// Arnout: artists wanted the colours to be inversed
				( *pic_out )[j * 4 + 0] = out[2] * 255;
				( *pic_out )[j * 4 + 1] = out[1] * 255;
				( *pic_out )[j * 4 + 2] = out[0] * 255;
			} else {
				( *pic_out )[j * 4 + 0] = out[0] * 255;
				( *pic_out )[j * 4 + 1] = out[1] * 255;
				( *pic_out )[j * 4 + 2] = out[2] * 255;
			}
			( *pic_out )[j * 4 + 3] = 255;

			//sumIntensity += intensity;
		}
	} else {
		for ( j = 0 ; j < width * height; j++ ) {
			byte *dst = &( *pic_out )[j * 4];
			R_ColorShiftLightingBytes( &( *pic )[j * in_padding], dst, qfalse );
			dst[3] = 255;
		}
	}

	return maxIntensity;
}


#define LIGHTMAP_SIZE 128
static const imgFlags_t lightmapFlags = IMGFLAG_NOSCALE | IMGFLAG_LIGHTMAP | IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE;


/*
===============
R_LoadLightmaps
===============
*/
static void R_LoadLightmaps( const lump_t *l ) {
	byte        *buf, *buf_p, *image_p;
	int len;
	byte image[LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4];
	int i /*, j*/;
	float intensity, maxIntensity = 0;
//	double sumIntensity = 0;


	// ydnar: clear lightmaps first
	tr.numLightmaps = 0;
	memset( tr.lightmaps, 0, sizeof( *tr.lightmaps ) * MAX_LIGHTMAPS );

	//lightmapWidth = lightmapHeight = 1;

	// permedia doesn't support lightmaps
	if ( glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		return;
	}


	len = l->filelen;
	if ( !len ) {
		return;
	}
	buf = fileBase + l->fileofs;

	// create all the lightmaps
	tr.numLightmaps = len / (LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3);
	if ( tr.numLightmaps == 1 ) {
		//FIXME: HACK: maps with only one lightmap turn up fullbright for some reason.
		//this avoids this, but isn't the correct solution.
		tr.numLightmaps++;
	}

	// we are about to upload textures
	//R_IssuePendingRenderCommands();

	for ( i = 0 ; i < tr.numLightmaps ; i++ ) {
		// expand the 24 bit on-disk to 32 bit
		buf_p = buf + i * LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3;
		image_p = image;

		intensity = R_ProcessLightmap( &buf_p, 3, LIGHTMAP_SIZE, LIGHTMAP_SIZE, &image_p );
		if ( intensity > maxIntensity ) {
			maxIntensity = intensity;
		}

		tr.lightmaps[i] = R_CreateImage( va( "*lightmap%d",i ), NULL, image,
										 LIGHTMAP_SIZE, LIGHTMAP_SIZE, lightmapFlags );
	}

	if ( r_lightmap->integer > 1 ) {
		ri.Printf( PRINT_ALL, "Brightest lightmap value: %d\n", ( int ) ( maxIntensity * 255 ) );
	}
}


/*
=================
RE_SetWorldVisData

This is called by the clipmodel subsystem so we can share the 1.8 megs of
space in big maps...
=================
*/
void RE_SetWorldVisData( const byte *vis ) {
	tr.externalVisData = vis;
}


/*
=================
R_LoadVisibility
=================
*/
static void R_LoadVisibility( const lump_t *l ) {
	unsigned numClusters, clusterBytes, len;
	byte	*buf;

	len = PAD( s_worldData.numClusters, 64 ) >> 3;
	s_worldData.novis = ri.Hunk_Alloc( len, h_low );
	Com_Memset( s_worldData.novis, 0xff, len );

	len = l->filelen;
	if ( !len ) {
		return;
	}

	if ( len < VIS_HEADER ) {
		Com_Error( ERR_DROP, "%s: lump too short", __func__ );
	}

	buf = fileBase + l->fileofs;
	numClusters = LittleLong( ((int *)buf)[0] );
	clusterBytes = LittleLong( ((int *)buf)[1] );

	buf += VIS_HEADER;
	len -= VIS_HEADER;

	if ( (uint64_t)numClusters * clusterBytes > len ) {
		Com_Error( ERR_DROP, "%s: lump too short", __func__ );
	}
	if ( numClusters < s_worldData.numClusters ) {
		Com_Error( ERR_DROP, "%s: bad numClusters", __func__ );
	}
	if ( clusterBytes < (numClusters + 7) >> 3 ) {
		Com_Error( ERR_DROP, "%s: bad clusterBytes", __func__ );
	}

	s_worldData.numClusters = numClusters;
	s_worldData.clusterBytes = clusterBytes;

	// CM_Load should have given us the vis data to share, so
	// we don't need to allocate another copy
	if ( tr.externalVisData ) {
		s_worldData.vis = tr.externalVisData;
	} else {
		byte	*dest;

		dest = ri.Hunk_Alloc( len, h_low );
		Com_Memcpy( dest, buf, len );
		s_worldData.vis = dest;
	}
}

//===============================================================================


/*
===============
ShaderForShaderNum
===============
*/
static shader_t *ShaderForShaderNum( const int shaderNum, int lightmapNum ) {
	shader_t	*shader;
	const dshader_t *dsh;

	if ( shaderNum < 0 || shaderNum >= s_worldData.numShaders ) {
		ri.Error( ERR_DROP, "ShaderForShaderNum: bad num %i", shaderNum );
	}

	dsh = &s_worldData.shaders[ shaderNum ];

	if ( glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		lightmapNum = LIGHTMAP_BY_VERTEX;
	}

	shader = R_FindShader( dsh->shader, lightmapNum, qtrue );

	// if the shader had errors, just use default shader
	if ( shader->defaultShader ) {
		return tr.defaultShader;
	}

	if ( r_singleShader->integer && !shader->isSky ) {
		return tr.defaultShader;
	}

	return shader;
}

// Ridah, optimizations here
// memory block for use by surfaces
static byte *surfHunkPtr;
static int surfHunkSize;
#define SURF_HUNK_MAXSIZE (0x40000)
#define LL( x ) LittleLong( x )

/*
==============
R_InitSurfMemory
==============
*/
void R_InitSurfMemory( void ) {
	// allocate a new chunk
	surfHunkPtr = ri.Hunk_Alloc( SURF_HUNK_MAXSIZE, h_low );
	surfHunkSize = 0;
}

/*
==============
R_GetSurfMemory
==============
*/
void *R_GetSurfMemory( int size ) {
	byte *retval;

	// round to cacheline
	size = PAD( size, 32 );

	surfHunkSize += size;
	if ( surfHunkSize >= SURF_HUNK_MAXSIZE ) {
		// allocate a new chunk
		R_InitSurfMemory();
		surfHunkSize += size;   // since it just got reset
	}
	retval = surfHunkPtr;
	surfHunkPtr += size;

	return (void *)retval;
}


/*
SphereFromBounds() - ydnar
creates a bounding sphere from a bounding box
*/

static void SphereFromBounds( vec3_t mins, vec3_t maxs, vec3_t origin, float *radius ) {
	vec3_t temp;

	VectorAdd( mins, maxs, origin );
	VectorScale( origin, 0.5, origin );
	VectorSubtract( maxs, origin, temp );
	*radius = VectorLength( temp );
}


/*
FinishGenericSurface() - ydnar
handles final surface classification
*/

static void FinishGenericSurface( const dsurface_t *ds, srfGeneric_t *gen, vec3_t pt ) {
	// set bounding sphere
	SphereFromBounds( gen->bounds[ 0 ], gen->bounds[ 1 ], gen->origin, &gen->radius );

	// take the plane normal from the lightmap vector and classify it
	gen->plane.normal[ 0 ] = LittleFloat( ds->lightmapVecs[ 2 ][ 0 ] );
	gen->plane.normal[ 1 ] = LittleFloat( ds->lightmapVecs[ 2 ][ 1 ] );
	gen->plane.normal[ 2 ] = LittleFloat( ds->lightmapVecs[ 2 ][ 2 ] );
	gen->plane.dist = DotProduct( pt, gen->plane.normal );
	SetPlaneSignbits( &gen->plane );
	gen->plane.type = PlaneTypeForNormal( gen->plane.normal );
}


/*
===============
ParseMesh
===============
*/
static void ParseMesh( const dsurface_t *ds, const drawVert_t *verts, int numVerts, msurface_t *surf ) {
	srfGridMesh_t	*grid;
	int				i, j;
	unsigned		width, height, numPoints;
	drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE];
	int				lightmapNum;
	vec3_t			bounds[2];
	vec3_t			tmpVec;
	static surfaceType_t	skipData = SF_SKIP;

	lightmapNum = LittleLong( ds->lightmapNum );


	// get shader value
	surf->shader = ShaderForShaderNum( LittleLong( ds->shaderNum ), lightmapNum );

	// we may have a nodraw surface, because they might still need to
	// be around for movement clipping
	if ( s_worldData.shaders[ LittleLong( ds->shaderNum ) ].surfaceFlags & SURF_NODRAW ) {
		surf->data = &skipData;
		return;
	}

	width = LittleLong( ds->patchWidth );
	height = LittleLong( ds->patchHeight );

	// mirror CM_GeneratePatchCollide() checks
	if (width <= 2 || height <= 2 || !(width & 1) || !(height & 1) ||
		width > MAX_GRID_SIZE || height > MAX_GRID_SIZE ||
		width * height > ARRAY_LEN(points))
		ri.Error( ERR_DROP, "%s: bad patch size", __func__ );

	numPoints = width * height;
	if (numPoints > numVerts)
		ri.Error( ERR_DROP, "%s: verts out of range", __func__ );

	for ( i = 0 ; i < numPoints ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			points[i].xyz[j] = LittleFloat( verts[i].xyz[j] );
			points[i].normal[j] = R_ClampDenorm( LittleFloat( verts[i].normal[j] ) );
		}
		for ( j = 0 ; j < 2 ; j++ ) {
			points[i].st[j] = LittleFloat( verts[i].st[j] );
			points[i].lightmap[j] = LittleFloat( verts[i].lightmap[j] );
		}
		R_ColorShiftLightingBytes( verts[i].color.rgba, points[i].color.rgba, qtrue );
	}

	// pre-tesseleate
	grid = R_SubdividePatchToGrid( width, height, points );
	surf->data = (surfaceType_t *)grid;

	// copy the level of detail origin, which is the center
	// of the group of all curves that must subdivide the same
	// to avoid cracking
	for ( i = 0 ; i < 3 ; i++ ) {
		bounds[0][i] = LittleFloat( ds->lightmapVecs[0][i] );
		bounds[1][i] = LittleFloat( ds->lightmapVecs[1][i] );
	}
	VectorAdd( bounds[0], bounds[1], bounds[1] );
	VectorScale( bounds[1], 0.5f, grid->lodOrigin );
	VectorSubtract( bounds[0], grid->lodOrigin, tmpVec );
	grid->lodRadius = VectorLength( tmpVec );

	// finish surface
	FinishGenericSurface( ds, (srfGeneric_t*) grid, grid->verts[ 0 ].xyz );
}


/*
===============
ParseTriSurf
===============
*/
static void ParseTriSurf( const dsurface_t *ds, const drawVert_t *verts, int numVerts, msurface_t *surf, int *indexes, int numIndexes ) {
	srfTriangles_t	*tri;
	int				i, j;
	int				lightmapNum;

	// get lightmap num
	lightmapNum = LittleLong( ds->lightmapNum );

	// get shader
	surf->shader = ShaderForShaderNum( LittleLong( ds->shaderNum ), lightmapNum );  //%	LIGHTMAP_BY_VERTEX );

	//tri = ri.Hunk_Alloc( sizeof( *tri ) + numVerts * sizeof( tri->verts[0] )
	//	+ numIndexes * sizeof( tri->indexes[0] ) );
	tri = R_GetSurfMemory( sizeof( *tri ) + numVerts * sizeof( tri->verts[0] )
						   + numIndexes * sizeof( tri->indexes[0] ) );

	tri->surfaceType = SF_TRIANGLES;
	tri->numVerts = numVerts;
	tri->numIndexes = numIndexes;
	tri->verts = ( drawVert_t * )( tri + 1 );
	tri->indexes = ( int * )( tri->verts + tri->numVerts );

	surf->data = (surfaceType_t *)tri;

	// copy vertexes
	ClearBounds( tri->bounds[0], tri->bounds[1] );
	for ( i = 0 ; i < numVerts ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			tri->verts[i].xyz[j] = LittleFloat( verts[i].xyz[j] );
			tri->verts[i].normal[j] = R_ClampDenorm( LittleFloat( verts[i].normal[j] ) );
		}
		AddPointToBounds( tri->verts[i].xyz, tri->bounds[0], tri->bounds[1] );
		for ( j = 0 ; j < 2 ; j++ ) {
			tri->verts[i].st[j] = LittleFloat( verts[i].st[j] );
			tri->verts[i].lightmap[j] = LittleFloat( verts[i].lightmap[j] );
		}

		R_ColorShiftLightingBytes( verts[i].color.rgba, tri->verts[i].color.rgba, qtrue );
	}

	// copy indexes
	for ( i = 0 ; i < numIndexes ; i++ ) {
		tri->indexes[i] = LittleLong( indexes[i] );
		if ( tri->indexes[i] < 0 || tri->indexes[i] >= numVerts ) {
			ri.Error( ERR_DROP, "Bad index in triangle surface" );
		}
	}

	// finish surface
	FinishGenericSurface( ds, (srfGeneric_t*) tri, tri->verts[ 0 ].xyz );
}



/*
ParseFoliage() - ydnar
parses a foliage drawsurface
*/

static void ParseFoliage( const dsurface_t *ds, const drawVert_t *verts, int numVerts, msurface_t *surf, int *indexes, int numIndexes ) {
	srfFoliage_t    *foliage;
	int i, j, numInstances, size;
//	vec4_t          *xyz, *normal /*, *origin*/;
//	fcolor4ub_t		*color;
	vec3_t bounds[ 2 ], boundsTranslated[ 2 ];
	float scale;

	// get shader
	surf->shader = ShaderForShaderNum( ds->shaderNum, LIGHTMAP_BY_VERTEX );

	// foliage surfaces have their actual vert count in patchHeight
	// and the instance count in patchWidth
	// the instances are just additional drawverts

	// get counts
	numInstances = LittleLong( ds->patchWidth );

	// calculate size
	size = sizeof( *foliage ) +
		   numVerts * ( sizeof( foliage->xyz[ 0 ] ) + sizeof( foliage->normal[ 0 ] ) + sizeof( foliage->texCoords[ 0 ] ) + sizeof( foliage->lmTexCoords[ 0 ] ) ) +
		   numIndexes * sizeof( foliage->indexes[ 0 ] ) +
		   numInstances * sizeof( foliage->instances[ 0 ] );

	// get memory
	foliage = R_GetSurfMemory( size );

	// set up surface
	foliage->surfaceType = SF_FOLIAGE;
	foliage->numVerts = numVerts;
	foliage->numIndexes = numIndexes;
	foliage->numInstances = numInstances;
	foliage->xyz = ( vec4_t* )( foliage + 1 );
	foliage->normal = ( vec4_t* )( foliage->xyz + foliage->numVerts );
	foliage->texCoords = ( vec2_t* )( foliage->normal + foliage->numVerts );
	foliage->lmTexCoords = ( vec2_t* )( foliage->texCoords + foliage->numVerts );
	foliage->indexes = ( glIndex_t* )( foliage->lmTexCoords + foliage->numVerts );
	foliage->instances = ( foliageInstance_t* )( foliage->indexes + foliage->numIndexes );

	surf->data = (surfaceType_t*) foliage;

	// get foliage drawscale
	scale = r_drawfoliage->value;
	if ( scale < 0.0f ) {
		scale = 1.0f;
	} else if ( scale > 2.0f ) {
		scale = 2.0f;
	}

	// copy vertexes
	ClearBounds( bounds[ 0 ], bounds[ 1 ] );

	for ( i = 0; i < numVerts; i++ )
	{
		// copy xyz and normal
		for ( j = 0; j < 3; j++ )
		{
			foliage->xyz[ i ][ j ] = LittleFloat( verts[ i ].xyz[ j ] );
			foliage->normal[ i ][ j ] = R_ClampDenorm( LittleFloat( verts[ i ].normal[ j ] ) );
		}

		// scale height
		foliage->xyz[ i ][ 2 ] *= scale;

		// finish
		foliage->xyz[ i ][ 3 ] = foliage->normal[ i ][ 3 ] = 0;
		AddPointToBounds( foliage->xyz[ i ], bounds[ 0 ], bounds[ 1 ] );

		// copy texture coordinates
		for ( j = 0; j < 2; j++ )
		{
			foliage->texCoords[ i ][ j ] = LittleFloat( verts[ i ].st[ j ] );
			foliage->lmTexCoords[ i ][ j ] = LittleFloat( verts[ i ].lightmap[ j ] );
		}
	}

	// copy indexes
	for ( i = 0; i < numIndexes; i++ )
	{
		foliage->indexes[ i ] = LittleLong( indexes[ i ] );
		if ( /*foliage->indexes[ i ] < 0 ||*/ foliage->indexes[ i ] >= numVerts ) {
			ri.Error( ERR_DROP, "Bad index in triangle surface" );
		}
	}

	// copy origins and colors
	ClearBounds( foliage->bounds[ 0 ], foliage->bounds[ 1 ] );
	verts += numVerts;
	for ( i = 0; i < numInstances; i++ )
	{
		// copy xyz
		for ( j = 0; j < 3; j++ )
			foliage->instances[ i ].origin[ j ] = LittleFloat( verts[ i ].xyz[ j ] );
		VectorAdd( bounds[ 0 ], foliage->instances[ i ].origin, boundsTranslated[ 0 ] );
		VectorAdd( bounds[ 1 ], foliage->instances[ i ].origin, boundsTranslated[ 1 ] );
		AddPointToBounds( boundsTranslated[ 0 ], foliage->bounds[ 0 ], foliage->bounds[ 1 ] );
		AddPointToBounds( boundsTranslated[ 1 ], foliage->bounds[ 0 ], foliage->bounds[ 1 ] );

		// copy color
		R_ColorShiftLightingBytes( verts[ i ].color.rgba, foliage->instances[ i ].color.rgba, qtrue );
	}

	// finish surface
	FinishGenericSurface( ds, (srfGeneric_t*) foliage, foliage->xyz[ 0 ] );
}


/*
===============
ParseFlare
===============
*/
static void ParseFlare( const dsurface_t *ds, msurface_t *surf ) {
	srfFlare_t		*flare;
	int				i;

	// get shader
	surf->shader = ShaderForShaderNum( LittleLong( ds->shaderNum ), LIGHTMAP_BY_VERTEX );

	flare = ri.Hunk_Alloc( sizeof( *flare ), h_low );
	flare->surfaceType = SF_FLARE;

	surf->data = (surfaceType_t *)flare;

	for ( i = 0 ; i < 3 ; i++ ) {
		flare->origin[i] = LittleFloat( ds->lightmapOrigin[i] );
		flare->color[i] = LittleFloat( ds->lightmapVecs[0][i] );
		flare->normal[i] = R_ClampDenorm( LittleFloat( ds->lightmapVecs[2][i] ) );
	}
}


/*
=================
R_MergedWidthPoints

returns qtrue if there are grid points merged on a width edge
=================
*/
static qboolean R_MergedWidthPoints( const srfGridMesh_t *grid, int offset ) {
	int i, j;

	for (i = 1; i < grid->width-1; i++) {
		for (j = i + 1; j < grid->width-1; j++) {
			if ( fabs(grid->verts[i + offset].xyz[0] - grid->verts[j + offset].xyz[0]) > .1) continue;
			if ( fabs(grid->verts[i + offset].xyz[1] - grid->verts[j + offset].xyz[1]) > .1) continue;
			if ( fabs(grid->verts[i + offset].xyz[2] - grid->verts[j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}


/*
=================
R_MergedHeightPoints

returns qtrue if there are grid points merged on a height edge
=================
*/
static qboolean R_MergedHeightPoints( const srfGridMesh_t *grid, int offset ) {
	int i, j;

	for (i = 1; i < grid->height-1; i++) {
		for (j = i + 1; j < grid->height-1; j++) {
			if ( fabs(grid->verts[grid->width * i + offset].xyz[0] - grid->verts[grid->width * j + offset].xyz[0]) > .1) continue;
			if ( fabs(grid->verts[grid->width * i + offset].xyz[1] - grid->verts[grid->width * j + offset].xyz[1]) > .1) continue;
			if ( fabs(grid->verts[grid->width * i + offset].xyz[2] - grid->verts[grid->width * j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}


/*
=================
R_FixSharedVertexLodError_r

NOTE: never sync LoD through grid edges with merged points!

FIXME: write generalized version that also avoids cracks between a patch and one that meets half way?
=================
*/
static void R_FixSharedVertexLodError_r( int start, srfGridMesh_t *grid1 ) {
	int j, k, l, m, n, offset1, offset2, touch;
	srfGridMesh_t *grid2;

	for ( j = start; j < s_worldData.numsurfaces; j++ ) {
		//
		grid2 = (srfGridMesh_t *) s_worldData.surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// if the LOD errors are already fixed for this patch
		if ( grid2->lodFixed == 2 ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		touch = qfalse;
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = (grid1->height-1) * grid1->width;
			else offset1 = 0;
			if (R_MergedWidthPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->width-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = grid1->width-1;
			else offset1 = 0;
			if (R_MergedHeightPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->height-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		if (touch) {
			grid2->lodFixed = 2;
			R_FixSharedVertexLodError_r ( start, grid2 );
			//NOTE: this would be correct but makes things really slow
			//grid2->lodFixed = 1;
		}
	}
}


/*
=================
R_FixSharedVertexLodError

This function assumes that all patches in one group are nicely stitched together for the highest LoD.
If this is not the case this function will still do its job but won't fix the highest LoD cracks.
=================
*/
static void R_FixSharedVertexLodError( void ) {
	int i;
	srfGridMesh_t *grid1;

	for ( i = 0; i < s_worldData.numsurfaces; i++ ) {
		//
		grid1 = (srfGridMesh_t *) s_worldData.surfaces[i].data;
		// if this surface is not a grid
		if ( grid1->surfaceType != SF_GRID )
			continue;
		//
		if ( grid1->lodFixed )
			continue;
		//
		grid1->lodFixed = 2;
		// recursively fix other patches in the same LOD group
		R_FixSharedVertexLodError_r( i + 1, grid1);
	}
}


/*
===============
R_StitchPatches
===============
*/
static int R_StitchPatches( int grid1num, int grid2num ) {
	float *v1, *v2;
	srfGridMesh_t *grid1, *grid2;
	int k, l, m, n, offset1, offset2, row, column;

	grid1 = (srfGridMesh_t *) s_worldData.surfaces[grid1num].data;
	grid2 = (srfGridMesh_t *) s_worldData.surfaces[grid2num].data;
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height-1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->width-2; k += 2) {

			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
									grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
					//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width-1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->height-2; k += 2) {
			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
									grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
									grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height-1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = grid1->width-1; k > 1; k -= 2) {

			for (m = 0; m < 2; m++) {

				if ( !grid2 || grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
										grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (!grid2 || grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k+1]);
					if (!grid2)
						break;
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width-1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = grid1->height-1; k > 1; k -= 2) {
			for (m = 0; m < 2; m++) {

				if ( !grid2 || grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
										grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (!grid2 || grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}


/*
===============
R_TryStitchPatch

This function will try to stitch patches in the same LoD group together for the highest LoD.

Only single missing vertex cracks will be fixed.

Vertices will be joined at the patch side a crack is first found, at the other side
of the patch (on the same row or column) the vertices will not be joined and cracks
might still appear at that side.
===============
*/
static int R_TryStitchingPatch( int grid1num ) {
	int j, numstitches;
	srfGridMesh_t *grid1, *grid2;

	numstitches = 0;
	grid1 = (srfGridMesh_t *) s_worldData.surfaces[grid1num].data;
	for ( j = 0; j < s_worldData.numsurfaces; j++ ) {
		//
		grid2 = (srfGridMesh_t *) s_worldData.surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		while (R_StitchPatches(grid1num, j))
		{
			numstitches++;
		}
	}
	return numstitches;
}


/*
===============
R_StitchAllPatches
===============
*/
static void R_StitchAllPatches( void ) {
	int i, stitched, numstitches;
	srfGridMesh_t *grid1;

	numstitches = 0;
	do
	{
		stitched = qfalse;
		for ( i = 0; i < s_worldData.numsurfaces; i++ ) {
			//
			grid1 = (srfGridMesh_t *) s_worldData.surfaces[i].data;
			// if this surface is not a grid
			if ( grid1->surfaceType != SF_GRID )
				continue;
			//
			if ( grid1->lodStitched )
				continue;
			//
			grid1->lodStitched = qtrue;
			stitched = qtrue;
			//
			numstitches += R_TryStitchingPatch( i );
		}
	}
	while (stitched);
	ri.Printf( PRINT_ALL, "stitched %d LoD cracks\n", numstitches );
}


/*
===============
R_MovePatchSurfacesToHunk
===============
*/
static void R_MovePatchSurfacesToHunk( void ) {
	int i, j, k, n, size;
	srfGridMesh_t *grid, *hunkgrid;

	for ( i = 0; i < s_worldData.numsurfaces; i++ ) {
		//
		grid = (srfGridMesh_t *) s_worldData.surfaces[i].data;
		// if this surface is not a grid
		if ( grid->surfaceType != SF_GRID )
			continue;
		//
		n = grid->width * grid->height - 1;
		size = n * sizeof( drawVert_t ) + sizeof( *grid );

		for (j = 0; j < n; j++) {
			for (k = 0; k < 3; k++) {
				grid->verts[j].normal[k] = R_ClampDenorm( grid->verts[j].normal[k] );
			}
		}

		hunkgrid = ri.Hunk_Alloc( size, h_low );
		Com_Memcpy(hunkgrid, grid, size);

		hunkgrid->widthLodError = ri.Hunk_Alloc( grid->width * 4, h_low );
		Com_Memcpy( hunkgrid->widthLodError, grid->widthLodError, grid->width * 4 );

		hunkgrid->heightLodError = ri.Hunk_Alloc( grid->height * 4, h_low );
		// rain - copy into hunkgrid instead of copying grid overtop of itself
		Com_Memcpy( hunkgrid->heightLodError, grid->heightLodError, grid->height * 4 );

		R_FreeSurfaceGridMesh( grid );

		s_worldData.surfaces[i].data = (void *) hunkgrid;
	}
}


/*
===============
R_LoadSurfaces
===============
*/
static void R_LoadSurfaces( const lump_t *surfs, const lump_t *verts, const lump_t *indexLump, const lump_t *fogLump ) {
	const dsurface_t  *in;
	msurface_t  *out;
	const drawVert_t  *dv;
	int         *indexes;
	int			count, totalVerts, totalIndexes, totalFogs;
	int numFaces, numMeshes, numTriSurfs, numFlares, numFoliage;
	unsigned	firstVert, numVerts, firstIndex, numIndexes;
	int			i;

	numFaces = 0;
	numMeshes = 0;
	numTriSurfs = 0;
	numFlares = 0;
	numFoliage = 0;

	in = (void *)(fileBase + surfs->fileofs);
	if (surfs->filelen % sizeof(*in))
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	count = surfs->filelen / sizeof(*in);

	dv = (void *)(fileBase + verts->fileofs);
	if (verts->filelen % sizeof(*dv))
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	totalVerts = verts->filelen / sizeof(*dv);

	indexes = (void *)(fileBase + indexLump->fileofs);
	if ( indexLump->filelen % sizeof(*indexes))
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	totalIndexes = indexLump->filelen / sizeof(*indexes);

	if ( fogLump->filelen % sizeof( dfog_t ) ) {
		totalFogs = 0;
	} else {
		totalFogs = (fogLump->filelen / sizeof( dfog_t ) + 1);
	}

	out = ri.Hunk_Alloc( count * sizeof(*out), h_low );

	s_worldData.surfaces = out;
	s_worldData.numsurfaces = count;

	// Ridah, init the surface memory. This is optimization, so we don't have to
	// look for memory for each surface, we allocate a big block and just chew it up
	// as we go
	R_InitSurfMemory();
	// shut up compiler
	firstVert = numVerts = 0;
	firstIndex = numIndexes = 0;

	for ( i = 0 ; i < count ; i++, in++, out++ ) {
		unsigned type = LittleLong( in->surfaceType );
		unsigned fogIndex;

		// todo MST_FOLIAGE VALIDATION and the proper in values
		if ( type != MST_FLARE ) {
			firstVert = LittleLong( in->firstVert );
			if ( type == MST_PATCH ) {
				numVerts = 0;	// use patch size
			} else if ( type == MST_FOLIAGE ) {
				// Foliage stores its vertex count in patchHeight, plus instance count in patchWidth
				numVerts = LittleLong( in->patchHeight ) + LittleLong( in->patchWidth );
			} else {
				numVerts = LittleLong( in->numVerts );
			}
			if ( (uint64_t)firstVert + numVerts > totalVerts )
				ri.Error( ERR_DROP, "%s: bad verts", __func__ );

			if ( type != MST_PATCH ) {
				firstIndex = LittleLong( in->firstIndex );
				numIndexes = LittleLong( in->numIndexes );
				if ( (uint64_t)firstIndex + numIndexes > totalIndexes )
					ri.Error( ERR_DROP, "%s: bad indexes", __func__ );

				// don't allow partial triangles
				if ( numIndexes % 3 ) {
					ri.Printf( PRINT_WARNING, "%s: odd number of indexes: %u\n", __func__, numIndexes );
					numIndexes -= numIndexes % 3;
				}
			}
		}

		// get fog volume
		fogIndex = LittleLong( in->fogNum ) + 1U;
		if ( fogIndex >= totalFogs ) {
			if ( type != MST_FLARE )
				ri.Printf( PRINT_WARNING, "%s: bad fog index: %u on surface type %u\n", __func__, fogIndex, type );
			fogIndex = 0;
		}
		out->fogIndex = fogIndex;

		switch ( type ) {
		case MST_PATCH:
			ParseMesh( in, dv + firstVert, totalVerts - firstVert, out );
			numMeshes++;
			break;
		case MST_TRIANGLE_SOUP:
			ParseTriSurf( in, dv + firstVert, numVerts, out, indexes + firstIndex, numIndexes );
			numTriSurfs++;
			break;
		case MST_PLANAR:
			// ydnar: faces and triangle surfaces are now homogenous
			//%	ParseFace( in, dv, out, indexes );
			ParseTriSurf( in, dv + firstVert, numVerts, out, indexes + firstIndex, numIndexes );
			numFaces++;
			break;
		case MST_FLARE:
			ParseFlare( in, out );
			numFlares++;
			break;
		case MST_FOLIAGE:   // ydnar
			//ParseFoliage( in, dv, out, indexes );
			ParseFoliage( in, dv + firstVert, LittleLong( in->patchHeight ), out, indexes + firstIndex, numIndexes );
			// todo?
			numFoliage++;
			break;
		default:
			ri.Error( ERR_DROP, "%s: bad surfaceType: %u", __func__, type );
		}
	}

#ifdef PATCH_STITCHING
	R_StitchAllPatches();
#endif

	R_FixSharedVertexLodError();

#ifdef PATCH_STITCHING
	R_MovePatchSurfacesToHunk();
#endif

	ri.Printf( PRINT_ALL, "...loaded %d faces, %i meshes, %i trisurfs, %i flares %i foliage\n",
			   numFaces, numMeshes, numTriSurfs, numFlares, numFoliage );
}


/*
=================
R_LoadSubmodels
=================
*/
static void R_LoadSubmodels( const lump_t *l ) {
	const dmodel_t *in;
	bmodel_t	*out;
	int			i, j, count;
	unsigned	firstSurface, numSurfaces;

	in = (void *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	count = l->filelen / sizeof(*in);

	s_worldData.numBModels = count;

	s_worldData.bmodels = out = ri.Hunk_Alloc( count * sizeof( *out ), h_low );

	for ( i = 0 ; i < count ; i++, in++, out++ ) {
		model_t *model;

		model = R_AllocModel();

		if ( model == NULL ) {
			ri.Error( ERR_DROP, "R_LoadSubmodels: R_AllocModel() failed" );
		}

		model->type = MOD_BRUSH; //-V1004
		model->model.bmodel = out;
		Com_sprintf( model->name, sizeof( model->name ), "*%d", i );

		for ( j = 0 ; j < 3 ; j++ ) {
			out->bounds[0][j] = LittleFloat( in->mins[j] );
			out->bounds[1][j] = LittleFloat( in->maxs[j] );
		}

		firstSurface = LittleLong( in->firstSurface );
		numSurfaces = LittleLong( in->numSurfaces );
		if ( (uint64_t)firstSurface + numSurfaces > s_worldData.numsurfaces ) {
			ri.Error( ERR_DROP, "%s: bad surfaces", __func__ );
		}

		out->firstSurface = s_worldData.surfaces + firstSurface;
		out->numSurfaces = numSurfaces;

		// ydnar: for attaching fog brushes to models
		out->firstBrush = LittleLong( in->firstBrush );
		out->numBrushes = LittleLong( in->numBrushes );

		// ydnar: allocate decal memory
		j = ( i == 0 ? MAX_WORLD_DECALS : MAX_ENTITY_DECALS );
		out->decals = ri.Hunk_Alloc( j * sizeof( *out->decals ), h_low );
		memset( out->decals, 0, j * sizeof( *out->decals ) );
	}
}



//==================================================================

/*
=================
R_SetParent
=================
*/
static void R_SetParent( mnode_t *node, mnode_t *parent )
{
	if ( node->parent )
		ri.Error( ERR_DROP, "%s: cycle encountered", __func__ );
	node->parent = parent;

	// handle leaf nodes
	if ( node->contents != CONTENTS_NODE ) {
		// add node surfaces to bounds
		if ( node->nummarksurfaces > 0 ) {
			int c;
			msurface_t      **mark;
			srfGeneric_t    *gen;


			// add node surfaces to bounds
			mark = node->firstmarksurface;
			c = node->nummarksurfaces;
			while ( c-- )
			{
				gen = ( srfGeneric_t* )( **mark ).data;
				if ( gen->surfaceType != SF_FACE &&
					 gen->surfaceType != SF_GRID &&
					 gen->surfaceType != SF_TRIANGLES &&
					 gen->surfaceType != SF_FOLIAGE ) {
					mark++;
					continue;
				}
				AddPointToBounds( gen->bounds[ 0 ], node->surfMins, node->surfMaxs );
				AddPointToBounds( gen->bounds[ 1 ], node->surfMins, node->surfMaxs );
				mark++;
			}
		}

		// go back
		return;
	}

	// recurse to child nodes
	R_SetParent( node->children[ 0 ], node );
	R_SetParent( node->children[ 1 ], node );

	// ydnar: surface bounds
	AddPointToBounds( node->children[ 0 ]->surfMins, node->surfMins, node->surfMaxs );
	AddPointToBounds( node->children[ 0 ]->surfMaxs, node->surfMins, node->surfMaxs );
	AddPointToBounds( node->children[ 1 ]->surfMins, node->surfMins, node->surfMaxs );
	AddPointToBounds( node->children[ 1 ]->surfMaxs, node->surfMins, node->surfMaxs );
}


/*
=================
R_LoadNodesAndLeafs
=================
*/
static void R_LoadNodesAndLeafs( const lump_t *nodeLump, const lump_t *leafLump ) {
	int			i, j;
	unsigned	p, firstmarksurface, nummarksurfaces;
	const dnode_t		*in;
	dleaf_t		*inLeaf;
	mnode_t 	*out;
	int			numNodes, numLeafs;

	in = (void *)(fileBase + nodeLump->fileofs);
	if (nodeLump->filelen % sizeof(dnode_t) ||
		leafLump->filelen % sizeof(dleaf_t) ) {
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	}
	numNodes = nodeLump->filelen / sizeof(dnode_t);
	numLeafs = leafLump->filelen / sizeof(dleaf_t);

	out = ri.Hunk_Alloc ( (numNodes + numLeafs) * sizeof(*out), h_low);	

	s_worldData.nodes = out;
	s_worldData.numnodes = numNodes + numLeafs;
	s_worldData.numDecisionNodes = numNodes;

	// ydnar: skybox optimization
	s_worldData.numSkyNodes = 0;
	s_worldData.skyNodes = ri.Hunk_Alloc( WORLD_MAX_SKY_NODES * sizeof( *s_worldData.skyNodes ), h_low );

	// load nodes
	for ( i = 0 ; i < numNodes; i++, in++, out++ )
	{
		for ( j = 0 ; j < 3 ; j++ )
		{
			out->mins[j] = LittleLong( in->mins[j] );
			out->maxs[j] = LittleLong( in->maxs[j] );
		}

		// ydnar: surface bounds
		VectorCopy( out->mins, out->surfMins );
		VectorCopy( out->maxs, out->surfMaxs );

		p = LittleLong( in->planeNum );
		if ( p >= s_worldData.numplanes ) {
			ri.Error( ERR_DROP, "%s: bad planeNum", __func__ );
		}
		out->plane = s_worldData.planes + p;

		out->contents = CONTENTS_NODE;  // differentiate from leafs

		for ( j = 0 ; j < 2 ; j++ )
		{
			p = LittleLong( in->children[j] );
			if (p & 0x80000000) {
				p = ~p;
				if ( p >= numLeafs ) {
					ri.Error( ERR_DROP, "%s: bad leaf", __func__ );
				}
				out->children[j] = s_worldData.nodes + numNodes + p;
			} else {
				if ( p >= numNodes ) {
					ri.Error( ERR_DROP, "%s: bad node", __func__ );
				}
				out->children[j] = s_worldData.nodes + p;
			}
		}
	}

	// load leafs
	inLeaf = ( void * )( fileBase + leafLump->fileofs );
	for ( i = 0 ; i < numLeafs ; i++, inLeaf++, out++ )
	{
		for ( j = 0 ; j < 3 ; j++ )
		{
			out->mins[j] = LittleLong( inLeaf->mins[j] );
			out->maxs[j] = LittleLong( inLeaf->maxs[j] );
		}

		// ydnar: surface bounds
		ClearBounds( out->surfMins, out->surfMaxs );

		out->cluster = LittleLong( inLeaf->cluster );
		if ( out->cluster + 1U > INT_MAX - 63U )
			Com_Error( ERR_DROP, "%s: bad cluster", __func__ );

		out->area = LittleLong(inLeaf->area);
		if ( out->area + 1U > MAX_MAP_AREAS )
			Com_Error( ERR_DROP, "%s: bad area", __func__ );

		if ( out->cluster >= s_worldData.numClusters ) {
			s_worldData.numClusters = out->cluster + 1;
		}

		firstmarksurface = LittleLong(inLeaf->firstLeafSurface);
		nummarksurfaces = LittleLong(inLeaf->numLeafSurfaces);
		if ( (uint64_t)firstmarksurface + nummarksurfaces > s_worldData.nummarksurfaces ) {
			ri.Error( ERR_DROP, "%s: bad marksurfaces", __func__ );
		}

		out->firstmarksurface = s_worldData.marksurfaces + firstmarksurface;
		out->nummarksurfaces = nummarksurfaces;
	}	

	// chain descendants
	R_SetParent( s_worldData.nodes, NULL );
}

//=============================================================================


/*
=================
R_ReplaceShaders

replaces some buggy map shaders
=================
*/
static void R_ReplaceMapShaders( dshader_t *out, int count ) 
{
	if ( Q_stricmp( s_worldData.baseName, "etf_opposition" ) == 0 ) {
		int i;
		qboolean etf_sunburn = ri.FS_ReadFile( "scripts/etf_sunburn.shader", NULL ) > 0 ? qtrue : qfalse;
		for ( i = 0; i < count; i++ ) {
			int len = (int)strlen(out[i].shader);
			if ( etf_sunburn ) {
				if( len >= 20 && strncmp( out[i].shader, "textures/q3f_sunburn", 20 ) == 0 ) {
					out[i].shader[9] = 'e';
					out[i].shader[10] = 't';
				}
			}
			if( len >= 20 && strncmp( out[i].shader, "textures/q3f_banners", 20 ) == 0 ) {
				Q_replace( "q3f", "etf", out[i].shader, sizeof(out[i].shader) );
				if ( strstr( out[i].shader, "long_logo_nonsolid") ) {
					Q_replace( "long_logo_nonsolid", "logo2_long", out[i].shader, sizeof(out[i].shader) );
				}
				if ( strstr( out[i].shader, "logo4_nonsolid") ) {
					Q_replace( "logo4_nonsolid", "logo2", out[i].shader, sizeof(out[i].shader) );
				}
			}
		}
	}
}


/*
=================
R_LoadShaders
=================
*/
static void R_LoadShaders( const lump_t *l ) {
	int		i, count;
	dshader_t	*in, *out;
	
	in = (void *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	count = l->filelen / sizeof(*in);
	out = ri.Hunk_Alloc ( count*sizeof(*out), h_low );

	s_worldData.shaders = out;
	s_worldData.numShaders = count;

	memcpy( out, in, count * sizeof( *out ) );

	R_ReplaceMapShaders( out, count );

	for ( i = 0 ; i < count ; i++ ) {
		out[i].surfaceFlags = LittleLong( out[i].surfaceFlags );
		out[i].contentFlags = LittleLong( out[i].contentFlags );
	}
}


/*
=================
R_LoadMarksurfaces
=================
*/
static void R_LoadMarksurfaces( const lump_t *l )
{	
	int		i, count;
	int		*in;
	msurface_t **out;
	
	in = (void *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	count = l->filelen / sizeof(*in);
	out = ri.Hunk_Alloc ( count*sizeof(*out), h_low);

	s_worldData.marksurfaces = out;
	s_worldData.nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		unsigned j = LittleLong(in[i]);
		if ( j >= s_worldData.numsurfaces ) {
			if ( j == 0xFFFFFFFF ) {
				j = 0; // fix for ut43_azurea_b1 map
			} else {
				ri.Error( ERR_DROP, "%s: bad surface", __func__ );
			}
		}
		out[i] = s_worldData.surfaces + j;
	}
}


/*
=================
R_LoadPlanes
=================
*/
static void R_LoadPlanes( const lump_t *l ) {
	int i, j;
	cplane_t    *out;
	const dplane_t 	*in;
	int count;
	int bits;

	in = (void *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	count = l->filelen / sizeof(*in);
	out = ri.Hunk_Alloc( count*2*sizeof(*out), h_low );

	s_worldData.planes = out;
	s_worldData.numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++) {
		bits = 0;
		for (j=0 ; j<3 ; j++) {
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0) {
				bits |= 1<<j;
			}
		}

		out->dist = LittleFloat (in->dist);
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
	}
}


/*
=================
R_LoadFogs
=================
*/
static void R_LoadFogs( const lump_t *l, const lump_t *brushesLump, const lump_t *sidesLump ) {
	int i, j;
	fog_t       *out;
	const dfog_t		*fogs;
	const dbrush_t 	*brushes, *brush;
	const dbrushside_t	*sides;
	int count, brushesCount, sidesCount;
	unsigned	sideNum, planeNum, firstSide;
	shader_t    *shader;
	float		d;

	fogs = (void *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*fogs)) {
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	}
	count = l->filelen / sizeof(*fogs);

	// create fog structures for them
	s_worldData.numfogs = count + 1;
	s_worldData.fogs = ri.Hunk_Alloc( s_worldData.numfogs * sizeof( *out ), h_low );
	out = s_worldData.fogs + 1;

	// ydnar: reset global fog
	s_worldData.globalFog = -1;

	if ( !count ) {
		return;
	}

	brushes = (void *)(fileBase + brushesLump->fileofs);
	if (brushesLump->filelen % sizeof(*brushes)) {
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	}
	brushesCount = brushesLump->filelen / sizeof(*brushes);

	sides = (void *)(fileBase + sidesLump->fileofs);
	if (sidesLump->filelen % sizeof(*sides)) {
		ri.Error( ERR_DROP, "%s(): funny lump size in %s", __func__, s_worldData.name );
	}
	sidesCount = sidesLump->filelen / sizeof(*sides);
	if (sidesCount < 6) {
		ri.Error( ERR_DROP, "%s(): sides lump too short in %s", __func__, s_worldData.name );
	}

	for ( i=0 ; i<count ; i++, fogs++) {
		out->originalBrushNumber = LittleLong( fogs->brushNum );

		// ydnar: global fog has a brush number of -1, and no visible side
		if ( out->originalBrushNumber == -1 ) {
			VectorSet( out->bounds[ 0 ], MIN_WORLD_COORD, MIN_WORLD_COORD, MIN_WORLD_COORD );
			VectorSet( out->bounds[ 1 ], MAX_WORLD_COORD, MAX_WORLD_COORD, MAX_WORLD_COORD );
			firstSide = 0;
		} else
		{
			if ( (unsigned)out->originalBrushNumber >= brushesCount ) {
				ri.Error( ERR_DROP, "%s(): fog brushNumber out of range in %s", __func__, s_worldData.name );
			}

			// ydnar: find which bsp submodel the fog volume belongs to
			for ( j = 0; j < s_worldData.numBModels; j++ )
			{
				if ( out->originalBrushNumber >= s_worldData.bmodels[ j ].firstBrush &&
					 out->originalBrushNumber < ( s_worldData.bmodels[ j ].firstBrush + s_worldData.bmodels[ j ].numBrushes ) ) {
					out->modelNum = j;
					break;
				}
			}

			brush = brushes + out->originalBrushNumber;

			firstSide = LittleLong( brush->firstSide );

			if ( firstSide > sidesCount - 6 ) {
				ri.Error( ERR_DROP, "fog brush sideNumber out of range" );
			}

			// brushes are always sorted with the axial sides first
			for ( j = 0; j < 6; j++ ) {
				sideNum = firstSide + j;
				planeNum = LittleLong( sides[ sideNum ].planeNum );
				if ( planeNum >= s_worldData.numplanes ) {
					ri.Error( ERR_DROP, "fog brush planeNum out of range" );
				}
				d = s_worldData.planes[ planeNum ].dist;
				out->bounds[j & 1][j >> 1] = (j & 1) ? d : -d;
			}
		}

		// get information from the shader for fog parameters
		shader = R_FindShader( fogs->shader, LIGHTMAP_NONE, qtrue );

		out->parms = shader->fogParms;

		// Arnout: colorInt is now set in the shader so we can modify it
		out->shader = shader;

		// ydnar: global fog sets clearcolor/zfar
		if ( out->originalBrushNumber == -1 ) {
			s_worldData.globalFog = i + 1;
			VectorCopy( shader->fogParms.color, s_worldData.globalOriginalFog );
			s_worldData.globalOriginalFog[ 3 ] =  shader->fogParms.depthForOpaque;
		}

		// set the gradient vector
		sideNum = LittleLong( fogs->visibleSide );

		if ( sideNum == -1 ) {
			out->hasSurface = qfalse;
		} else {
			if ( sideNum >= sidesCount - firstSide ) {
				ri.Printf( PRINT_WARNING, "bad fog side offset\n" );
				out->hasSurface = qfalse;
			} else {
				out->hasSurface = qtrue;
				planeNum = LittleLong( sides[ firstSide + sideNum ].planeNum );
				if ( planeNum >= s_worldData.numplanes ) {
					ri.Error( ERR_DROP, "fog brush planeNum out of range" );
				}
				VectorSubtract( vec3_origin, s_worldData.planes[ planeNum ].normal, out->surface );
				out->surface[3] = -s_worldData.planes[ planeNum ].dist;
			}
		}

		out++;
	}
}


/*
================
R_LoadLightGrid
================
*/
static void R_LoadLightGrid( const lump_t *l ) {
	int		i;
	vec3_t	maxs;
	unsigned bounds[3];
	unsigned numGridPoints;
	world_t	*w;
	float	*wMins, *wMaxs;

	w = &s_worldData;

	if ( !l->filelen ) {
		w->lightGridData = NULL;
		return;
	}

	w->lightGridInverseSize[0] = 1.0f / w->lightGridSize[0];
	w->lightGridInverseSize[1] = 1.0f / w->lightGridSize[1];
	w->lightGridInverseSize[2] = 1.0f / w->lightGridSize[2];

	wMins = w->bmodels[0].bounds[0];
	wMaxs = w->bmodels[0].bounds[1];

	for ( i = 0 ; i < 3 ; i++ ) {
		w->lightGridOrigin[i] = w->lightGridSize[i] * ceil( wMins[i] / w->lightGridSize[i] );
		maxs[i] = w->lightGridSize[i] * floor( wMaxs[i] / w->lightGridSize[i] );
		bounds[i] = (maxs[i] - w->lightGridOrigin[i])/w->lightGridSize[i] + 1;
	}

	if ( (uint64_t)bounds[0] * bounds[1] > INT_MAX ||
		 (uint64_t)bounds[0] * bounds[1] * bounds[2] > INT_MAX) {
		ri.Printf( PRINT_WARNING, "WARNING: bad light grid bounds\n" );
		w->lightGridData = NULL;
		return;
	}

	numGridPoints = bounds[0] * bounds[1] * bounds[2];

	if ( l->filelen % 8 || l->filelen / 8 != numGridPoints ) {
		ri.Printf( PRINT_WARNING, "WARNING: light grid mismatch\n" );
		w->lightGridData = NULL;
		return;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		w->lightGridBounds[i] = bounds[i];
	}

	w->lightGridData = ri.Hunk_Alloc( l->filelen, h_low );
	Com_Memcpy( w->lightGridData, (void *)(fileBase + l->fileofs), l->filelen );

	// deal with overbright bits
	for ( i = 0 ; i < numGridPoints ; i++ ) {
		R_ColorShiftLightingBytes( &w->lightGridData[i*8], &w->lightGridData[i*8], qfalse );
		R_ColorShiftLightingBytes( &w->lightGridData[i*8+3], &w->lightGridData[i*8+3], qfalse );
	}
}


/*
================
R_LoadEntities
================
*/
static void R_LoadEntities( const lump_t *l ) {
	const char *p, *token;
	char *s;
	char keyname[MAX_TOKEN_CHARS];
	char value[MAX_TOKEN_CHARS], *v[3];
	world_t	*w;

	w = &s_worldData;
	w->lightGridSize[0] = 64;
	w->lightGridSize[1] = 64;
	w->lightGridSize[2] = 128;

	// store for reference by the cgame
	w->entityString = ri.Hunk_Alloc( l->filelen + 1, h_low );
	memcpy( w->entityString, fileBase + l->fileofs, l->filelen );
	w->entityString[l->filelen] = '\0';
	w->entityParsePoint = w->entityString;

	p = w->entityString;
	token = COM_ParseExt( &p, qtrue );
	if (*token != '{') {
		return;
	}

	// only parse the world spawn
	while ( 1 ) {	
		// parse key
		token = COM_ParseExt( &p, qtrue );

		if ( !*token || *token == '}' ) {
			break;
		}
		Q_strncpyz(keyname, token, sizeof(keyname));

		// parse value
		token = COM_ParseExt( &p, qtrue );

		if ( !*token || *token == '}' ) {
			break;
		}
		Q_strncpyz(value, token, sizeof(value));

		// check for remapping of shaders
		s = "remapshader";
		if (!Q_strncmp(keyname, s, (int)strlen(s)) ) {
			s = strchr(value, ';');
			if (!s) {
				ri.Printf( PRINT_WARNING, "WARNING: no semi colon in shaderremap '%s'\n", value );
				break;
			}
			*s++ = '\0';
			RE_RemapShader(value, s, "0");
			continue;
		}
		// check for a different grid size
		if (!Q_stricmp(keyname, "gridsize")) {
			//sscanf(value, "%f %f %f", &w->lightGridSize[0], &w->lightGridSize[1], &w->lightGridSize[2] );
			Com_Split( value, v, 3, ' ' );
			w->lightGridSize[0] = Q_atof( v[0] );
			w->lightGridSize[1] = Q_atof( v[1] );
			w->lightGridSize[2] = Q_atof( v[2] );
			continue;
		}
		if (!Q_stricmp(keyname, "_texturenpot")) {
			if( atoi(value) > 0 ) {
				tr.mapNonPowerTwo = qtrue;
			}
		}
		if (!Q_stricmp(keyname, "_nobloom")) {
			if( atoi(value) > 0 ) {
				tr.mapNoBloom = qtrue;
			}
		}
	}
}


/*
=================
RE_GetEntityToken
=================
*/
qboolean RE_GetEntityToken( char *buffer, int size ) {
	const char	*s;

	s = COM_Parse( &s_worldData.entityParsePoint );
	Q_strncpyz( buffer, s, size );
	if ( !s_worldData.entityParsePoint || !s[0] ) {
		s_worldData.entityParsePoint = s_worldData.entityString;
		return qfalse;
	} else {
		return qtrue;
	}
}


/*
=================
RE_LoadWorldMap

Called directly from cgame
=================
*/
void RE_LoadWorldMap( const char *name ) {
	int			i;
	int32_t		size;
	dheader_t	*header;
	union {
		byte *b;
		void *v;
	} buffer;
	byte		*startMarker;

	skyboxportal = 0;

	if ( tr.worldMapLoaded ) {
		ri.Error( ERR_DROP, "ERROR: attempted to redundantly load world map" );
	}

	// set default sun direction to be used if it isn't
	// overridden by a shader
	tr.sunDirection[0] = 0.45f;
	tr.sunDirection[1] = 0.3f;
	tr.sunDirection[2] = 0.9f;

	tr.sunShader = NULL;   // clear sunshader so it's not there if the level doesn't specify it

	// invalidate fogs (likely to be re-initialized to new values by the current map)
	// TODO:(SA)this is sort of silly.  I'm going to do a general cleanup on fog stuff
	//			now that I can see how it's been used.  (functionality can narrow since
	//			it's not used as much as it's designed for.)
	R_SetFog( FOG_SKY,       0, 0, 0, 0, 0, 0 );
	R_SetFog( FOG_PORTALVIEW,0, 0, 0, 0, 0, 0 );
	R_SetFog( FOG_HUD,       0, 0, 0, 0, 0, 0 );
	R_SetFog( FOG_MAP,       0, 0, 0, 0, 0, 0 );
	R_SetFog( FOG_CURRENT,   0, 0, 0, 0, 0, 0 );
	R_SetFog( FOG_TARGET,    0, 0, 0, 0, 0, 0 );
	R_SetFog( FOG_WATER,     0, 0, 0, 0, 0, 0 );
	R_SetFog( FOG_SERVER,    0, 0, 0, 0, 0, 0 );

	VectorNormalize( tr.sunDirection );

	tr.worldMapLoaded = qtrue;
	tr.worldRawName[0] = '\0';

	// load it
	size = ri.FS_ReadFile( name, &buffer.v );
	if ( !buffer.b ) {
		ri.Error( ERR_DROP, "%s: couldn't load %s", __func__, name );
	}
	if ( size < sizeof( dheader_t ) ) {
		ri.Error( ERR_DROP, "%s: %s has truncated header", __func__, name );
	}

	tr.mapLoading = qtrue;

	// ydnar: set map meta dir
	Q_strncpyz( tr.worldRawName, name, sizeof( tr.worldRawName ) );
	COM_StripExtension( tr.worldRawName, tr.worldRawName, sizeof( tr.worldRawName ) );

	// clear tr.world so if the level fails to load, the next
	// try will not look at the partially loaded version
	tr.world = NULL;

	Com_Memset( &s_worldData, 0, sizeof( s_worldData ) );
	Q_strncpyz( s_worldData.name, name, sizeof( s_worldData.name ) );

	Q_strncpyz( s_worldData.baseName, COM_SkipPath( s_worldData.name ), sizeof( s_worldData.name ) );
	COM_StripExtension(s_worldData.baseName, s_worldData.baseName, sizeof(s_worldData.baseName));

	startMarker = ri.Hunk_Alloc(0, h_low);
	c_gridVerts = 0;

	header = (dheader_t *)buffer.b;
	fileBase = (byte *)header;

	// swap all the lumps
	for ( i = 0; i < sizeof( dheader_t ) / sizeof(int32_t); i++ ) {
		( (int32_t *)header )[i] = LittleLong( ( (int32_t *)header )[i] );
	}

	if ( header->version != BSP_VERSION ) {
		ri.Error( ERR_DROP, "%s: %s has wrong version number (%i should be %i)", __func__, name, header->version, BSP_VERSION );
	}

	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		uint32_t ofs = header->lumps[i].fileofs;
		uint32_t len = header->lumps[i].filelen;
		if ( (uint64_t)ofs + len > size ) {
			ri.Error( ERR_DROP, "%s: %s has wrong lump[%i] size/offset", __func__, name, i );
		}
	}

	// load into heap
	ri.SCR_UpdateScreen();
	R_LoadShaders( &header->lumps[LUMP_SHADERS] );
	ri.SCR_UpdateScreen();
	R_LoadLightmaps( &header->lumps[LUMP_LIGHTMAPS] );
	ri.SCR_UpdateScreen();
	R_LoadPlanes( &header->lumps[LUMP_PLANES] );
	ri.SCR_UpdateScreen();
	//%	R_LoadFogs( &header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES] );
	//%	ri.SCR_UpdateScreen();
	R_LoadSurfaces( &header->lumps[LUMP_SURFACES], &header->lumps[LUMP_DRAWVERTS], &header->lumps[LUMP_DRAWINDEXES], &header->lumps[LUMP_FOGS] ); // added fog lump to surfs for finding count to verify
	ri.SCR_UpdateScreen();
	R_LoadMarksurfaces( &header->lumps[LUMP_LEAFSURFACES] );
	ri.SCR_UpdateScreen();
	R_LoadNodesAndLeafs( &header->lumps[LUMP_NODES], &header->lumps[LUMP_LEAFS] );
	ri.SCR_UpdateScreen();
	R_LoadSubmodels( &header->lumps[LUMP_MODELS] );
	ri.SCR_UpdateScreen();

	// moved fog lump loading here, so fogs can be tagged with a model num
	R_LoadFogs( &header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES] );
	ri.SCR_UpdateScreen();

	R_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	ri.SCR_UpdateScreen();
	R_LoadEntities( &header->lumps[LUMP_ENTITIES] );
	ri.SCR_UpdateScreen();
	R_LoadLightGrid( &header->lumps[LUMP_LIGHTGRID] );
	ri.SCR_UpdateScreen();

#ifdef USE_VBO
	R_BuildWorldVBO( s_worldData.surfaces, s_worldData.numsurfaces );
#endif

	tr.mapLoading = qfalse;

	s_worldData.dataSize = (byte *)ri.Hunk_Alloc( 0, h_low ) - startMarker;

	// only set tr.world now that we know the entire level has loaded properly
	tr.world = &s_worldData;

	// reset fog to world fog (if present)
	R_SetFog( FOG_CMD_SWITCHFOG, FOG_MAP,20,0,0,0,0 );

//----(SA)	set the sun shader if there is one
	if ( *tr.sunShaderName ) {
		tr.sunShader = R_FindShader( tr.sunShaderName, LIGHTMAP_NONE, qtrue );
	}

//----(SA)	end
	ri.FS_FreeFile( buffer.v );
}
