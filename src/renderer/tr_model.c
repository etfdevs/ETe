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

// tr_models.c -- model loading and caching

#include "tr_local.h"

#define LL( x ) x = LittleLong( x )

// Ridah
static qboolean R_LoadMDC( model_t *mod, int lod, void *buffer, int fileSize, const char *mod_name );
// done.
static qboolean R_LoadMD3( model_t *mod, int lod, void *buffer, int fileSize, const char *name );
static qboolean R_LoadMDS( model_t *mod, void *buffer, int fileSize, const char *name );
static qboolean R_LoadMDM( model_t *mod, void *buffer, int fileSize, const char *name );
static qboolean R_LoadMDX( model_t *mod, void *buffer, int fileSize, const char *name );

/*
====================
R_RegisterMD3
====================
*/
static qhandle_t R_RegisterMD3(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	int			lod;
	uint32_t	ident = 0;
	qboolean	loaded = qfalse;
	int			fileSize;
	char filename[MAX_QPATH], namebuf[MAX_QPATH+20];
	const char *fext;

	fext = COM_GetExtension(name); // always non-empty
	COM_StripExtension(name, filename, sizeof(filename));

	for ( lod = 0 ; lod < MD3_MAX_LODS ; lod++ ) {
		if ( lod )
			Com_sprintf(namebuf, sizeof(namebuf), "%s_%d.%s", filename, lod, fext);
		else
			Com_sprintf(namebuf, sizeof(namebuf), "%s.%s", filename, fext);

		fileSize = ri.FS_ReadFile( namebuf, &buf.v );
		if ( !buf.v )
			continue;

		if ( fileSize < sizeof( md3Header_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: truncated header for %s\n", __func__, name );
			ri.FS_FreeFile( buf.v );
			break;
		}
		
		ident = LittleLong( *buf.u );
		if ( ident == MD3_IDENT ) {
			loaded = R_LoadMD3( mod, lod, buf.v, fileSize, name );
		} else if ( ident == MDC_IDENT ) {
			loaded = R_LoadMDC( mod, lod, buf.u, fileSize, name );
			ri.Printf( PRINT_WARNING, "%s: mismatched fileid for %s, loading as mdc\n", __func__, name);
		} else {
			ri.Printf( PRINT_WARNING, "%s: unknown fileid for %s\n", __func__, name );
			loaded = qfalse;
		}
		
		ri.FS_FreeFile( buf.v );

		if ( loaded ) {
			mod->numLods++;
		} else {
			if ( ident == MDC_IDENT )
				mod->model.mdc[mod->numLods] = NULL;
			else
				mod->model.md3[mod->numLods] = NULL;
			break;
		}
	}

	if ( mod->numLods )
		return mod->index;

	ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW "%s: couldn't load %s\n", __func__, name );

	mod->type = MOD_BAD;
	return 0;
}

/*
====================
R_RegisterMDC
====================
*/
static qhandle_t R_RegisterMDC(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	int			lod;
	uint32_t	ident = 0;
	qboolean	loaded = qfalse;
	int			fileSize;
	char filename[MAX_QPATH], namebuf[MAX_QPATH+20];
	const char *fext;

	fext = COM_GetExtension(name); // always non-empty
	COM_StripExtension(name, filename, sizeof(filename));

	for ( lod = 0 ; lod < MD3_MAX_LODS ; lod++ ) {
		if ( lod )
			Com_sprintf(namebuf, sizeof(namebuf), "%s_%d.%s", filename, lod, fext);
		else
			Com_sprintf(namebuf, sizeof(namebuf), "%s.%s", filename, fext);

		fileSize = ri.FS_ReadFile( namebuf, &buf.v );
		if ( !buf.v )
			continue;

		if ( fileSize < sizeof( md3Header_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: truncated header for %s\n", __func__, name );
			ri.FS_FreeFile( buf.v );
			break;
		}
		
		ident = LittleLong( *buf.u );
		if ( ident == MDC_IDENT ) {
			loaded = R_LoadMDC( mod, lod, buf.u, fileSize, name );
		} else if ( ident == MD3_IDENT ) {
			loaded = R_LoadMD3( mod, lod, buf.u, fileSize, name );
			ri.Printf( PRINT_WARNING, "%s: mismatched fileid for %s, loading as md3\n", __func__, name );
		} else {
			ri.Printf( PRINT_WARNING, "%s: unknown fileid for %s\n", __func__, name );
			loaded = qfalse;
		}
		
		ri.FS_FreeFile( buf.v );

		if ( loaded ) {
			mod->numLods++;
		} else {
			if ( ident == MD3_IDENT )
				mod->model.md3[mod->numLods] = NULL;
			else
				mod->model.mdc[mod->numLods] = NULL;
			break;
		}
	}

	if ( mod->numLods )
		return mod->index;

//#ifdef _DEBUG
//	ri.Printf(PRINT_WARNING, "R_RegisterMDC: couldn't load %s\n", name);
//#endif

	mod->type = MOD_BAD;
	return 0;
}

/*
====================
R_RegisterMDS
====================
*/
static qhandle_t R_RegisterMDS(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	uint32_t	ident = 0;
	qboolean loaded = qfalse;
	int fileSize;

	fileSize = ri.FS_ReadFile( name, &buf.v );
	if( !buf.v )
	{
		mod->type = MOD_BAD;
		return 0;
	}

	if ( fileSize < sizeof( mdsHeader_t ) ) {
		mod->type = MOD_BAD;
		ri.Printf( PRINT_WARNING, "%s: truncated header for %s\n", __func__, name );
		ri.FS_FreeFile( buf.v );
		return 0;
	}
	
	ident = LittleLong( *buf.u );
	if ( ident == MDS_IDENT )
		loaded = R_LoadMDS( mod, buf.u, fileSize, name );
	else
		ri.Printf( PRINT_WARNING, "%s: unknown fileid for %s\n", __func__, name );

	ri.FS_FreeFile (buf.v);
	
	if ( !loaded )
	{
		ri.Printf( PRINT_WARNING, "%s: couldn't load mds file %s\n", __func__, name );
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}

/*
====================
R_RegisterMDM
====================
*/
static qhandle_t R_RegisterMDM(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	uint32_t	ident;
	qboolean loaded = qfalse;
	int fileSize;

	fileSize = ri.FS_ReadFile(name, &buf.v);
	if(!buf.v)
	{
		mod->type = MOD_BAD;
		return 0;
	}

	if ( fileSize < sizeof( mdmHeader_t ) ) {
		mod->type = MOD_BAD;
		ri.Printf( PRINT_WARNING, "%s: truncated header for %s\n", __func__, name );
		ri.FS_FreeFile( buf.v );
		return 0;
	}
	
	ident = LittleLong( *buf.u );
	if( ident == MDM_IDENT )
		loaded = R_LoadMDM( mod, buf.u, fileSize, name );
	else
		ri.Printf( PRINT_WARNING, "%s: unknown fileid for %s\n", __func__, name );

	ri.FS_FreeFile (buf.v);
	
	if ( !loaded )
	{
		ri.Printf( PRINT_WARNING, "%s: couldn't load mdm file %s\n", __func__, name );
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}

/*
====================
R_RegisterMDX
====================
*/
static qhandle_t R_RegisterMDX(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	uint32_t	ident;
	qboolean loaded = qfalse;
	int fileSize;

	fileSize = ri.FS_ReadFile(name, &buf.v);
	if(!buf.v)
	{
		mod->type = MOD_BAD;
		return 0;
	}

	if ( fileSize < sizeof( mdxHeader_t ) ) {
		mod->type = MOD_BAD;
		ri.Printf( PRINT_WARNING, "%s: truncated header for %s\n", __func__, name );
		ri.FS_FreeFile( buf.v );
		return 0;
	}

	ident = LittleLong( *buf.u );
	if ( ident == MDX_IDENT )
		loaded = R_LoadMDX( mod, buf.u, fileSize, name );
	else
		ri.Printf( PRINT_WARNING, "%s: unknown fileid for %s\n", __func__, name );

	ri.FS_FreeFile (buf.v);
	
	if ( !loaded )
	{
		ri.Printf(PRINT_WARNING, "%s: couldn't load mdx file %s\n", __func__, name );
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}

/*
====================
R_RegisterIQM
====================
*/
qhandle_t R_RegisterIQM(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	qboolean loaded = qfalse;
	int fileSize;

	fileSize = ri.FS_ReadFile(name, &buf.v);
	if(!buf.v)
	{
		mod->type = MOD_BAD;
		return 0;
	}

	loaded = R_LoadIQM( mod, buf.u, fileSize, name );

	ri.FS_FreeFile (buf.v);
	
	if ( !loaded )
	{
		ri.Printf( PRINT_WARNING, "%s: couldn't load %s\n", __func__, name );
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}


typedef struct
{
	const char *ext;
	qhandle_t (*ModelLoader)( const char *, model_t * );
} modelExtToLoaderMap_t;

// Note that the ordering indicates the order of preference used
// when there are multiple models of different formats available
static modelExtToLoaderMap_t modelLoaders[ ] =
{
	{ "iqm", R_RegisterIQM },
	{ "mds", R_RegisterMDS },
	{ "mdm", R_RegisterMDM },
	{ "mdx", R_RegisterMDX },
	{ "md3", R_RegisterMD3 },
	{ "mdc", R_RegisterMDC }
};

static int numModelLoaders = ARRAY_LEN(modelLoaders);

//===============================================================================

/*
** R_GetModelByHandle
*/
model_t	*R_GetModelByHandle( qhandle_t index ) {
	model_t		*mod;

	// out of range gets the default model
	if ( index < 1 || index >= tr.numModels ) {
		return tr.models[0];
	}

	mod = tr.models[index];

	return mod;
}

//===============================================================================

/*
** R_AllocModel
*/
model_t *R_AllocModel( void ) {
	model_t		*mod;

	if ( tr.numModels >= MAX_MOD_KNOWN ) {
		return NULL;
	}

	mod = ri.Hunk_Alloc( sizeof( *tr.models[tr.numModels] ), h_low );
	mod->index = tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}



/*
R_LoadModelShadow()
loads a model's shadow script
*/

static void R_LoadModelShadow( model_t *mod ) {
	union {
		char *c;
		void *v;
	} buf;
	char filename[ 1024 ];
	shader_t    *sh;

	// set default shadow
	mod->shadowShader = 0;

	// build name
	COM_StripExtension( mod->name, filename, sizeof( filename ) );
	COM_DefaultExtension( filename, 1024, ".shadow" );

	// load file
	ri.FS_ReadFile( filename, &buf.v );
	if ( buf.v != NULL ) {
		char    *shadowBits;

		shadowBits = strchr( buf.c, ' ' );
		if ( shadowBits != NULL ) {
			*shadowBits = '\0';
			shadowBits++;

			if ( strlen( buf.c ) >= MAX_QPATH ) {
				Com_Printf( "R_LoadModelShadow: Shader name exceeds MAX_QPATH\n" );
				mod->shadowShader = 0;
			} else {
				sh = R_FindShader( buf.c, LIGHTMAP_NONE, qtrue );

				if ( sh->defaultShader ) {
					mod->shadowShader = 0;
				} else {
					mod->shadowShader = sh->index;
				}
			}
			sscanf( shadowBits, "%f %f %f %f %f %f",
					&mod->shadowParms[ 0 ], &mod->shadowParms[ 1 ], &mod->shadowParms[ 2 ],
					&mod->shadowParms[ 3 ], &mod->shadowParms[ 4 ], &mod->shadowParms[ 5 ] );
		}
		ri.FS_FreeFile( buf.v );
	}
}



/*
====================
RE_RegisterModel

Loads in a model for the given name

Zero will be returned if the model fails to load.
An entry will be retained for failed models as an
optimization to prevent disk rescanning if they are
asked for again.
====================
*/
qhandle_t RE_RegisterModel( const char *name ) {
	model_t		*mod;
	qhandle_t	hModel;
	qboolean	orgNameFailed = qfalse;
	int			orgLoader = -1;
	int			i;
	char		localName[ MAX_QPATH ];
	const char	*ext;
	char		altName[ MAX_QPATH ];

	if ( !name || !name[0] ) {
		// Ridah, disabled this, we can see models that can't be found because they won't be there
		//ri.Printf( PRINT_ALL, "RE_RegisterModel: NULL name\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Model name exceeds MAX_QPATH\n" );
		return 0;
	}

	//
	// search the currently loaded models
	//
	for ( hModel = 1 ; hModel < tr.numModels; hModel++ ) {
		mod = tr.models[hModel];
		if ( !Q_stricmp( mod->name, name ) ) {
			if ( mod->type == MOD_BAD ) {
				return 0;
			}
			return hModel;
		}
	}

	// allocate a new model_t

	if ( ( mod = R_AllocModel() ) == NULL ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterModel: R_AllocModel() failed for '%s'\n", name );
		return 0;
	}

	// only set the name after the model has been successfully loaded
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );

	//R_IssuePendingRenderCommands();

	R_LoadModelShadow( mod );

	mod->type = MOD_BAD;
	mod->numLods = 0;

	//
	// load the files
	//
	Q_strncpyz( localName, name, sizeof( localName ) );

	ext = COM_GetExtension( localName );

	if( *ext )
	{
		// Look for the correct loader and use it
		for( i = 0; i < numModelLoaders; i++ )
		{
			if( !Q_stricmp( ext, modelLoaders[ i ].ext ) )
			{
				// Load
				hModel = modelLoaders[ i ].ModelLoader( localName, mod );
				break;
			}
		}

		// A loader was found
		if( i < numModelLoaders )
		{
			if( !hModel )
			{
				// Loader failed, most likely because the file isn't there;
				// try again without the extension
				orgNameFailed = qtrue;
				orgLoader = i;
				COM_StripExtension( name, localName, sizeof( localName ) );
			}
			else
			{
				// Something loaded
				return mod->index;
			}
		}
	}

	// Try and find a suitable match using all
	// the model formats supported
	for( i = 0; i < numModelLoaders; i++ )
	{
		if (i == orgLoader)
			continue;

		Com_sprintf( altName, sizeof (altName), "%s.%s", localName, modelLoaders[ i ].ext );

		// Load
		hModel = modelLoaders[ i ].ModelLoader( altName, mod );

		if( hModel )
		{
			if( orgNameFailed )
			{
				ri.Printf( PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n",
						name, altName );
			}

			break;
		}
	}

	return hModel;
}

//-------------------------------------------------------------------------------
// Ridah, mesh compression
#include "../renderercommon/anorms256.h"

// rain - unused
#if 0
/*
=============
R_MDC_GetVec
=============
*/
void R_MDC_GetVec( unsigned char anorm, vec3_t dir ) {
	VectorCopy( r_anormals[anorm], dir );
}

/*
=============
R_MDC_GetAnorm
=============
*/
unsigned char R_MDC_GetAnorm( const vec3_t dir ) {
	int i, best_start_i[3], next_start, next_end;
	int best = 0; // TTimo: init
	float best_diff, group_val, this_val, diff;
	float   *this_norm;

	// find best Z match

	if ( dir[2] > 0.097545f ) {
		next_start = 144;
		next_end = NUMMDCVERTEXNORMALS;
	} else
	{
		next_start = 0;
		next_end = 144;
	}

	best_diff = 999;
	this_val = -999;

	for ( i = next_start ; i < next_end ; i++ )
	{
		if ( r_anormals[i][2] == this_val ) {
			continue;
		} else {
			this_val = r_anormals[i][2];
		}

		if ( ( diff = Q_fabs( dir[2] - r_anormals[i][2] ) ) < best_diff ) {
			best_diff = diff;
			best_start_i[2] = i;

		}

		if ( next_start ) {
			if ( r_anormals[i][2] > dir[2] ) {
				break;  // we've gone passed the dir[2], so we can't possibly find a better match now
			}
		} else {
			if ( r_anormals[i][2] < dir[2] ) {
				break;  // we've gone passed the dir[2], so we can't possibly find a better match now
			}
		}
	}

	best_diff = -999;

	// find best match within the Z group

	for ( i = best_start_i[2], group_val = r_anormals[i][2]; i < NUMMDCVERTEXNORMALS; i++ )
	{
		this_norm = r_anormals[i];

		if ( this_norm[2] != group_val ) {
			break; // done checking the group
		}
/*
		if (	(this_norm[0] < 0 && dir[0] > 0)
			||	(this_norm[0] > 0 && dir[0] < 0)
			||	(this_norm[1] < 0 && dir[1] > 0)
			||	(this_norm[1] > 0 && dir[1] < 0))
			continue;
*/
		diff = DotProduct( dir, this_norm );

		if ( diff > best_diff ) {
			best_diff = diff;
			best = i;
		}
	}

	return (unsigned char)best;
}

/*
=================
R_MDC_EncodeOfsVec
=================
*/
qboolean R_MDC_EncodeXyzCompressed( const vec3_t vec, const vec3_t normal, mdcXyzCompressed_t *out ) {
	mdcXyzCompressed_t retval;
	int i;
	unsigned char anorm;

	i = sizeof( mdcXyzCompressed_t );

	retval.ofsVec = 0;
	for ( i = 0; i < 3; i++ ) {
		if ( Q_fabs( vec[i] ) >= MDC_MAX_DIST ) {
			return qfalse;
		}
		retval.ofsVec += ( ( (int)Q_fabs( ( vec[i] + MDC_DIST_SCALE * 0.5 ) * ( 1.0 / MDC_DIST_SCALE ) + MDC_MAX_OFS ) ) << ( i * MDC_BITS_PER_AXIS ) );
	}
	anorm = R_MDC_GetAnorm( normal );
	retval.ofsVec |= ( (int)anorm ) << 24;

	*out = retval;
	return qtrue;
}

/*
=================
R_MDC_DecodeXyzCompressed
=================
*/
//#if 0   // unoptimized version, used for finding right settings
/*void R_MDC_DecodeXyzCompressed( mdcXyzCompressed_t *xyzComp, vec3_t out, vec3_t normal ) {
	int i;

	for ( i = 0; i < 3; i++ ) {
		out[i] = ( (float)( ( xyzComp->ofsVec >> ( i * MDC_BITS_PER_AXIS ) ) & ( ( 1 << MDC_BITS_PER_AXIS ) - 1 ) ) - MDC_MAX_OFS ) * MDC_DIST_SCALE;
	}
	R_MDC_GetVec( ( unsigned char )( xyzComp->ofsVec >> 24 ), normal );
}*/
//#endif

/*
=================
R_MDC_GetXyzCompressed
=================
*/
static qboolean R_MDC_GetXyzCompressed( md3Header_t *md3, md3XyzNormal_t *newXyz, vec3_t oldPos, mdcXyzCompressed_t *out, qboolean verify ) {
	vec3_t newPos, vec;
	int i;
	vec3_t pos, dir, norm, outnorm;

	for ( i = 0; i < 3; i++ ) {
		newPos[i] = (float)newXyz->xyz[i] * MD3_XYZ_SCALE;
	}

	VectorSubtract( newPos, oldPos, vec );
	R_LatLongToNormal( norm, newXyz->normal );
	if ( !R_MDC_EncodeXyzCompressed( vec, norm, out ) ) {
		return qfalse;
	}

	// calculate the uncompressed position
	R_MDC_DecodeXyzCompressed( out->ofsVec, dir, outnorm );
	VectorAdd( oldPos, dir, pos );

	if ( verify ) {
		if ( Distance( newPos, pos ) > MDC_MAX_ERROR ) {
			return qfalse;
		}
	}

	return qtrue;
}


/*
=================
R_MDC_CompressSurfaceFrame
=================
*/
static qboolean R_MDC_CompressSurfaceFrame( md3Header_t *md3, md3Surface_t *surf, int frame, int lastBaseFrame, mdcXyzCompressed_t *out ) {
	int i, j;
	md3XyzNormal_t  *xyz, *baseXyz;
	vec3_t oldPos;

	xyz = ( md3XyzNormal_t * )( (byte *)surf + surf->ofsXyzNormals );
	baseXyz = xyz + ( lastBaseFrame * surf->numVerts );
	xyz += ( frame * surf->numVerts );

	for ( i = 0; i < surf->numVerts; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			oldPos[j] = (float)baseXyz[i].xyz[j] * MD3_XYZ_SCALE;
		}
		if ( !R_MDC_GetXyzCompressed( md3, &xyz[i], oldPos, &out[i], qtrue ) ) {
			return qfalse;
		}
	}

	return qtrue;
}

/*
=================
R_MDC_CanCompressSurfaceFrame
=================
*/
static qboolean R_MDC_CanCompressSurfaceFrame( md3Header_t *md3, md3Surface_t *surf, int frame, int lastBaseFrame ) {
	int i, j;
	md3XyzNormal_t  *xyz, *baseXyz;
	mdcXyzCompressed_t xyzComp;
	vec3_t oldPos;

	xyz = ( md3XyzNormal_t * )( (byte *)surf + surf->ofsXyzNormals );
	baseXyz = xyz + ( lastBaseFrame * surf->numVerts );
	xyz += ( frame * surf->numVerts );

	for ( i = 0; i < surf->numVerts; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			oldPos[j] = (float)baseXyz[i].xyz[j] * MD3_XYZ_SCALE;
		}
		if ( !R_MDC_GetXyzCompressed( md3, &xyz[i], oldPos, &xyzComp, qtrue ) ) {
			return qfalse;
		}
	}

	return qtrue;
}

/*
=================
R_MD3toMDC

  Converts a model_t from md3 to mdc format
=================
*/
static qboolean R_MDC_ConvertMD3( model_t *mod, int lod, const char *mod_name ) {
	int i, j, f, c, k;
	md3Surface_t        *surf;
	md3Header_t         *md3;
	int                 *baseFrames;
	int numBaseFrames;

	qboolean foundBase;

	mdcHeader_t         *mdc, mdcHeader;
	mdcSurface_t        *cSurf;
	short               *frameBaseFrames, *frameCompFrames;

	mdcTag_t            *mdcTag;
	md3Tag_t            *md3Tag;

	vec3_t axis[3], angles;
	float ftemp;

	md3 = mod->model.md3[lod];

	baseFrames = ri.Hunk_AllocateTempMemory( sizeof( *baseFrames ) * md3->numFrames );

	// the first frame is always a base frame
	numBaseFrames = 0;
	memset( baseFrames, 0, sizeof( *baseFrames ) * md3->numFrames );
	baseFrames[numBaseFrames++] = 0;

	// first calculate how many baseframes we need, and which frames they are on
	// we need to treat the entire model as a single surface, if we compress some surfaces, and not others,
	// we may get tearing between surfaces

	for ( f = 1; f < md3->numFrames; f++ ) {

		surf = ( md3Surface_t * )( (byte *)md3 + md3->ofsSurfaces );
		foundBase = qfalse;
		for ( i = 0 ; i < md3->numSurfaces ; i++ ) {

			// process the verts in this surface, checking to see if the compressed
			// version will be close enough to the actual vert
			if ( !foundBase && !R_MDC_CanCompressSurfaceFrame( md3, surf, f, baseFrames[numBaseFrames - 1] ) ) {
				baseFrames[numBaseFrames++] = f;
				foundBase = qtrue;
			}

			// find the next surface
			surf = ( md3Surface_t * )( (byte *)surf + surf->ofsEnd );
		}

	}

	// success, so fill in the necessary data to the model_t
	mdcHeader.ident = MDC_IDENT;
	mdcHeader.version = MDC_VERSION;
	Q_strncpyz( mdcHeader.name, md3->name, sizeof( mdcHeader.name ) );
	mdcHeader.flags = md3->flags;
	mdcHeader.numFrames = md3->numFrames;
	mdcHeader.numTags = md3->numTags;
	mdcHeader.numSurfaces = md3->numSurfaces;
	mdcHeader.numSkins = md3->numSkins;
	mdcHeader.ofsFrames = sizeof( mdcHeader_t );
	mdcHeader.ofsTagNames = mdcHeader.ofsFrames + mdcHeader.numFrames * sizeof( md3Frame_t );
	mdcHeader.ofsTags = mdcHeader.ofsTagNames + mdcHeader.numTags * sizeof( mdcTagName_t );
	mdcHeader.ofsSurfaces = mdcHeader.ofsTags + mdcHeader.numTags * mdcHeader.numFrames * sizeof( mdcTag_t );
	mdcHeader.ofsEnd = mdcHeader.ofsSurfaces;

	surf = ( md3Surface_t * )( (byte *)md3 + md3->ofsSurfaces );
	for ( f = 0; f < md3->numSurfaces; f++ ) {
		mdcHeader.ofsEnd += sizeof( mdcSurface_t )
							+ surf->numShaders * sizeof( md3Shader_t )
							+ surf->numTriangles * sizeof( md3Triangle_t )
							+ surf->numVerts * sizeof( md3St_t )
							+ surf->numVerts * numBaseFrames * sizeof( md3XyzNormal_t )
							+ surf->numVerts * ( md3->numFrames - numBaseFrames ) * sizeof( mdcXyzCompressed_t )
							+ sizeof( short ) * md3->numFrames
							+ sizeof( short ) * md3->numFrames;

		surf = ( md3Surface_t * )( (byte *)surf + surf->ofsEnd );
	}

	// report the memory differences
	Com_Printf( "Compressed %s. Old = %i, New = %i\n", mod_name, md3->ofsEnd, mdcHeader.ofsEnd );

	mdc = ri.Hunk_Alloc( mdcHeader.ofsEnd, h_low );
	mod->model.mdc[lod] = mdc;

	// we have the memory allocated, so lets fill it in

	// header info
	*mdc = mdcHeader;
	// frames
	memcpy( ( md3Frame_t * )( (byte *)mdc + mdc->ofsFrames ), ( md3Frame_t * )( (byte *)md3 + md3->ofsFrames ), mdcHeader.numFrames * sizeof( md3Frame_t ) );
	// tag names
	for ( j = 0; j < md3->numTags; j++ ) {
		memcpy( ( mdcTagName_t * )( (byte *)mdc + mdc->ofsTagNames ) + j, ( ( md3Tag_t * )( (byte *)md3 + md3->ofsTags ) + j )->name, sizeof( mdcTagName_t ) );
	}
	// tags
	mdcTag = ( ( mdcTag_t * )( (byte *)mdc + mdc->ofsTags ) );
	md3Tag = ( ( md3Tag_t * )( (byte *)md3 + md3->ofsTags ) );
	for ( f = 0; f < md3->numFrames; f++ ) {
		for ( j = 0; j < md3->numTags; j++, mdcTag++, md3Tag++ ) {
			for ( k = 0; k < 3; k++ ) {
				// origin
				ftemp = md3Tag->origin[k] / MD3_XYZ_SCALE;
				mdcTag->xyz[k] = (short)ftemp;
				// axis
				VectorCopy( md3Tag->axis[k], axis[k] );
			}
			// convert the axis to angles
			AxisToAngles( axis, angles );
			// copy them into the new tag
			for ( k = 0; k < 3; k++ ) {
				mdcTag->angles[k] = angles[k] / MDC_TAG_ANGLE_SCALE;
			}
		}
	}
	// surfaces
	surf = ( md3Surface_t * )( (byte *)md3 + md3->ofsSurfaces );
	cSurf = ( mdcSurface_t * )( (byte *)mdc + mdc->ofsSurfaces );
	for ( j = 0 ; j < md3->numSurfaces ; j++ ) {

		cSurf->ident = SF_MDC;
		Q_strncpyz( cSurf->name, surf->name, sizeof( cSurf->name ) );
		cSurf->flags = surf->flags;
		cSurf->numCompFrames = ( mdc->numFrames - numBaseFrames );
		cSurf->numBaseFrames = numBaseFrames;
		cSurf->numShaders = surf->numShaders;
		cSurf->numVerts = surf->numVerts;
		cSurf->numTriangles = surf->numTriangles;
		cSurf->ofsTriangles = sizeof( mdcSurface_t );
		cSurf->ofsShaders = cSurf->ofsTriangles + cSurf->numTriangles * sizeof( md3Triangle_t );
		cSurf->ofsSt = cSurf->ofsShaders + cSurf->numShaders * sizeof( md3Shader_t );
		cSurf->ofsXyzNormals = cSurf->ofsSt + cSurf->numVerts * sizeof( md3St_t );
		cSurf->ofsXyzCompressed = cSurf->ofsXyzNormals + cSurf->numVerts * numBaseFrames * sizeof( md3XyzNormal_t );
		cSurf->ofsFrameBaseFrames = cSurf->ofsXyzCompressed + cSurf->numVerts * ( mdc->numFrames - numBaseFrames ) * sizeof( mdcXyzCompressed_t );
		cSurf->ofsFrameCompFrames = cSurf->ofsFrameBaseFrames + mdc->numFrames * sizeof( short );
		cSurf->ofsEnd = cSurf->ofsFrameCompFrames + mdc->numFrames * sizeof( short );

		// triangles
		memcpy( (byte *)cSurf + cSurf->ofsTriangles, (byte *)surf + surf->ofsTriangles, cSurf->numTriangles * sizeof( md3Triangle_t ) );
		// shaders
		memcpy( (byte *)cSurf + cSurf->ofsShaders, (byte *)surf + surf->ofsShaders, cSurf->numShaders * sizeof( md3Shader_t ) );
		// st
		memcpy( (byte *)cSurf + cSurf->ofsSt, (byte *)surf + surf->ofsSt, cSurf->numVerts * sizeof( md3St_t ) );

		// rest
		frameBaseFrames = ( short * )( (byte *)cSurf + cSurf->ofsFrameBaseFrames );
		frameCompFrames = ( short * )( (byte *)cSurf + cSurf->ofsFrameCompFrames );
		for ( f = 0, i = 0, c = 0; f < md3->numFrames; f++ ) {
			if ( i < numBaseFrames && f == baseFrames[i] ) {
				// copy this baseFrame from the md3
				memcpy( (byte *)cSurf + cSurf->ofsXyzNormals + ( sizeof( md3XyzNormal_t ) * cSurf->numVerts * i ),
						(byte *)surf + surf->ofsXyzNormals + ( sizeof( md3XyzNormal_t ) * cSurf->numVerts * f ),
						sizeof( md3XyzNormal_t ) * cSurf->numVerts );
				i++;
				frameCompFrames[f] = -1;
				frameBaseFrames[f] = i - 1;
			} else {
				if ( !R_MDC_CompressSurfaceFrame( md3, surf, f, baseFrames[i - 1], ( mdcXyzCompressed_t * )( (byte *)cSurf + cSurf->ofsXyzCompressed + sizeof( mdcXyzCompressed_t ) * cSurf->numVerts * c ) ) ) {
					ri.Error( ERR_DROP, "R_MDC_ConvertMD3: tried to compress an unsuitable frame\n" );
				}
				frameCompFrames[f] = c;
				frameBaseFrames[f] = i - 1;
				c++;
			}
		}

		// find the next surface
		surf = ( md3Surface_t * )( (byte *)surf + surf->ofsEnd );
		cSurf = ( mdcSurface_t * )( (byte *)cSurf + cSurf->ofsEnd );
	}

	mod->type = MOD_MDC;

	// free allocated memory
	ri.Hunk_FreeTempMemory( baseFrames );

	// kill the md3 memory
	ri.Hunk_FreeTempMemory( md3 );
	mod->model.md3[lod] = NULL;

	return qtrue;
}
#endif
// rain - #if 0

/*
=================
R_LoadMDC
=================
*/
static qboolean R_LoadMDC( model_t *mod, int lod, void *buffer, int fileSize, const char *mod_name ) {
	int i, j;
	mdcHeader_t         *pinmodel, *hdr;
	md3Frame_t          *frame;
	mdcSurface_t        *surf;
	md3Shader_t         *shader;
	md3Triangle_t       *tri;
	md3St_t             *st;
	md3XyzNormal_t      *xyz;
	mdcXyzCompressed_t  *xyzComp;
	mdcTag_t            *tag;
	mdcTagName_t		*tagName;
	short               *ps;
	uint32_t			version;
	uint32_t			size, bytesToEnd;
	qboolean fixRadius = qfalse;

	pinmodel = (mdcHeader_t *)buffer;

	version = LittleLong( pinmodel->version );
	if ( version != MDC_VERSION ) {
		ri.Printf( PRINT_WARNING, "%s: %s has wrong version (%i should be %i)\n", __func__, mod_name, version, MDC_VERSION );
		return qfalse;
	}

	size = LittleLong( pinmodel->ofsEnd );

	if ( size > fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	mod->type = MOD_MDC;
	mod->dataSize += size;
	mod->model.mdc[lod] = ri.Hunk_Alloc( size, h_low );

	memcpy( mod->model.mdc[lod], buffer, size );

	hdr = mod->model.mdc[lod];

	LL( hdr->ident );
	LL( hdr->version );
	LL( hdr->numFrames );
	LL( hdr->numTags );
	LL( hdr->numSurfaces );
	LL( hdr->numSkins );
	LL( hdr->ofsFrames );
	LL( hdr->ofsTagNames );
	LL( hdr->ofsTags );
	LL( hdr->ofsSurfaces );
	LL( hdr->ofsEnd );
	LL( hdr->flags );

	if ( hdr->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "%s: %s has no frames\n", __func__, mod_name );
		return qfalse;
	}

	if ( hdr->ofsFrames > size || hdr->ofsTags > size || hdr->ofsSurfaces > size ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	if ( hdr->numFrames > (size - hdr->ofsFrames) / sizeof( md3Frame_t ) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}
	if ( hdr->numTags > (size - hdr->ofsTagNames) / sizeof( mdcTagName_t ) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header (TagNames)\n", __func__, mod_name );
		return qfalse;
	}
	if ( hdr->numTags > (size - hdr->ofsTags) / (sizeof( mdcTag_t ) * hdr->numFrames) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}
	if ( hdr->numSurfaces > (size - hdr->ofsSurfaces) / sizeof( mdcSurface_t ) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	if ( strstr( mod->name,"sherman" ) || strstr( mod->name, "mg42" ) ) {
		fixRadius = qtrue;
	}

	// swap all the frames
	frame = ( md3Frame_t * )( (byte *)hdr + hdr->ofsFrames );
	for ( i = 0 ; i < hdr->numFrames ; i++, frame++ ) {
		frame->radius = LittleFloat( frame->radius );
		if ( fixRadius ) {
			frame->radius = 256;
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = 128;
				frame->bounds[1][j] = -128;
				frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
			}
		}
		// Hack for Bug using plugin generated model
		else if ( frame->radius == 1 ) {
			frame->radius = 256;
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = 128;
				frame->bounds[1][j] = -128;
				frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
			}
		}
		else {
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
				frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
				frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
			}
		}
	}

	// swap all the tags
	tag = ( mdcTag_t * )( (byte *)hdr + hdr->ofsTags );
	for ( i = 0 ; i < hdr->numTags * hdr->numFrames ; i++, tag++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			tag->xyz[j] = LittleShort( tag->xyz[j] );
			tag->angles[j] = LittleShort( tag->angles[j] );
		}
	}

	tagName = ( mdcTagName_t * )( (byte *)hdr + hdr->ofsTagNames );
	for ( i = 0 ; i < hdr->numTags ; i++, tagName++ ) {
		// zero-terminate tag name
		tagName->name[sizeof( tagName->name ) - 1] = '\0';
	}

	// swap all the surfaces
	surf = ( mdcSurface_t * )( (byte *)hdr + hdr->ofsSurfaces );
	for ( i = 0 ; i < hdr->numSurfaces ; i++ ) {

		// how many bytes are remaining before end of file
		bytesToEnd = size - ((byte*)surf - (byte*)hdr);

		if ( bytesToEnd < sizeof(*surf)) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}

		LL( surf->ident );
		LL( surf->flags );
		LL( surf->numBaseFrames );
		LL( surf->numCompFrames );
		LL( surf->numShaders );
		LL( surf->numTriangles );
		LL( surf->ofsTriangles );
		LL( surf->numVerts );
		LL( surf->ofsShaders );
		LL( surf->ofsSt );
		LL( surf->ofsXyzNormals );
		LL( surf->ofsXyzCompressed );
		LL( surf->ofsFrameBaseFrames );
		LL( surf->ofsFrameCompFrames );
		LL( surf->ofsEnd );

		if ( surf->ofsTriangles > bytesToEnd || surf->ofsShaders > bytesToEnd || surf->ofsSt > bytesToEnd || surf->ofsXyzNormals > bytesToEnd
				|| surf->ofsXyzCompressed > bytesToEnd || surf->ofsFrameBaseFrames > bytesToEnd || surf->ofsFrameCompFrames > bytesToEnd || surf->ofsEnd > bytesToEnd ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numTriangles > (bytesToEnd - surf->ofsTriangles) / sizeof( md3Triangle_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numShaders > (bytesToEnd - surf->ofsShaders) / sizeof( md3Shader_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numVerts > (bytesToEnd - surf->ofsSt) / sizeof( md3St_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numBaseFrames > (bytesToEnd - surf->ofsXyzNormals) / (sizeof( md3XyzNormal_t ) * surf->numVerts) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numCompFrames > (bytesToEnd - surf->ofsXyzCompressed) / (sizeof( mdcXyzCompressed_t ) * surf->numVerts) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( hdr->numFrames > (bytesToEnd - surf->ofsFrameBaseFrames) / sizeof( short ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( hdr->numFrames > (bytesToEnd - surf->ofsFrameCompFrames) / sizeof( short ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}

		if ( surf->numVerts >= SHADER_MAX_VERTEXES ) {
			ri.Printf(PRINT_WARNING, "%s: %s has more than %i verts on %s (%i).\n", __func__,
				mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
				surf->numVerts );
			return qfalse;
		}
		if ( surf->numTriangles >= SHADER_MAX_INDEXES / 3 ) {
			ri.Printf(PRINT_WARNING, "%s: %s has more than %i triangles on %s (%i).\n", __func__,
				mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, surf->name[0] ? surf->name : "a surface",
				surf->numTriangles );
			return qfalse;
		}

		// change to surface identifier
		surf->ident = SF_MDC;

		// zero-terminate surface name
		surf->name[sizeof( surf->name ) - 1] = '\0';

		// lowercase the surface name so skin compares are faster
		Q_strlwr( surf->name );

		// strip off a trailing _1 or _2
		// this is a crutch for q3data being a mess
		j = strlen( surf->name );
		if ( j > 2 && surf->name[j - 2] == '_' ) {
			surf->name[j - 2] = 0;
		}

		// register the shaders
		shader = ( md3Shader_t * )( (byte *)surf + surf->ofsShaders );
		for ( j = 0 ; j < surf->numShaders ; j++, shader++ ) {
			shader_t    *sh;

			// zero-terminate shader name
			shader->name[sizeof( shader->name ) - 1] = '\0';

			sh = R_FindShader( shader->name, LIGHTMAP_NONE, qtrue );
			if ( sh->defaultShader ) {
				shader->shaderIndex = 0;
			} else {
				shader->shaderIndex = sh->index;
			}
		}

		// Ridah, optimization, only do the swapping if we really need to
		if ( LittleShort( 1 ) != 1 ) {

			// swap all the triangles
			tri = ( md3Triangle_t * )( (byte *)surf + surf->ofsTriangles );
			for ( j = 0 ; j < surf->numTriangles ; j++, tri++ ) {
				LL( tri->indexes[0] );
				LL( tri->indexes[1] );
				LL( tri->indexes[2] );
				if (tri->indexes[0] >= surf->numVerts ||
					tri->indexes[1] >= surf->numVerts ||
					tri->indexes[2] >= surf->numVerts) {
					ri.Printf( PRINT_WARNING, "%s: %s has corrupted indexes\n", __func__, mod_name );
					return qfalse;
				}				
			}

			// swap all the ST
			st = ( md3St_t * )( (byte *)surf + surf->ofsSt );
			for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
				st->st[0] = LittleFloat( st->st[0] );
				st->st[1] = LittleFloat( st->st[1] );
			}

			// swap all the XyzNormals
			xyz = ( md3XyzNormal_t * )( (byte *)surf + surf->ofsXyzNormals );
			for ( j = 0 ; j < surf->numVerts * surf->numBaseFrames ; j++, xyz++ )
			{
				xyz->xyz[0] = LittleShort( xyz->xyz[0] );
				xyz->xyz[1] = LittleShort( xyz->xyz[1] );
				xyz->xyz[2] = LittleShort( xyz->xyz[2] );

				xyz->normal = LittleShort( xyz->normal );
			}

			// swap all the XyzCompressed
			xyzComp = ( mdcXyzCompressed_t * )( (byte *)surf + surf->ofsXyzCompressed );
			for ( j = 0 ; j < surf->numVerts * surf->numCompFrames ; j++, xyzComp++ )
			{
				LL( xyzComp->ofsVec );
			}

			// swap the frameBaseFrames
			ps = ( short * )( (byte *)surf + surf->ofsFrameBaseFrames );
			for ( j = 0; j < hdr->numFrames; j++, ps++ )
			{
				*ps = LittleShort( *ps );
			}

			// swap the frameCompFrames
			ps = ( short * )( (byte *)surf + surf->ofsFrameCompFrames );
			for ( j = 0; j < hdr->numFrames; j++, ps++ )
			{
				*ps = LittleShort( *ps );
			}
		}
		// done.

		// find the next surface
		surf = ( mdcSurface_t * )( (byte *)surf + surf->ofsEnd );
	}

	return qtrue;
}

// done.
//-------------------------------------------------------------------------------

/*
=================
R_LoadMD3
=================
*/
static qboolean R_LoadMD3( model_t *mod, int lod, void *buffer, int fileSize, const char *mod_name ) {
	int i, j;
	md3Header_t         *pinmodel, *hdr;
	md3Frame_t          *frame;
	md3Surface_t        *surf;
	md3Shader_t         *shader;
	md3Triangle_t       *tri;
	md3St_t             *st;
	md3XyzNormal_t      *xyz;
	md3Tag_t            *tag;
	uint32_t			version;
	uint32_t			size, bytesToEnd;
	qboolean			fixRadius = qfalse;

	pinmodel = (md3Header_t *)buffer;

	version = LittleLong( pinmodel->version );
	if ( version != MD3_VERSION ) {
		ri.Printf( PRINT_WARNING, "%s: %s has wrong version (%i should be %i)\n", __func__, mod_name, version, MD3_VERSION );
		return qfalse;
	}

	size = LittleLong( pinmodel->ofsEnd );

	if ( size > fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	mod->type = MOD_MESH;
	mod->dataSize += size;
	mod->model.md3[lod] = ri.Hunk_Alloc( size, h_low );

	memcpy( mod->model.md3[lod], buffer, size );

	hdr = mod->model.md3[lod];

	LL( hdr->ident );
	LL( hdr->version );
	LL( hdr->numFrames );
	LL( hdr->numTags );
	LL( hdr->numSurfaces );
	LL( hdr->numSkins );
	LL( hdr->ofsFrames );
	LL( hdr->ofsTags );
	LL( hdr->ofsSurfaces );
	LL( hdr->ofsEnd );

	if ( hdr->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "%s: %s has no frames\n", __func__, mod_name );
		return qfalse;
	}

	if ( hdr->ofsFrames > size || hdr->ofsTags > size || hdr->ofsSurfaces > size ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	if ( hdr->numFrames > (size - hdr->ofsFrames) / sizeof( md3Frame_t ) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}
	if ( hdr->numTags > (size - hdr->ofsTags) / (sizeof( md3Tag_t ) * hdr->numFrames) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}
	if ( hdr->numSurfaces > (size - hdr->ofsSurfaces) / sizeof( md3Surface_t ) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	if ( strstr( mod->name,"sherman" ) || strstr( mod->name, "mg42" ) ) {
		fixRadius = qtrue;
	}

	// swap all the frames
	frame = ( md3Frame_t * )( (byte *)hdr + hdr->ofsFrames );
	for ( i = 0 ; i < hdr->numFrames ; i++, frame++ ) {
		frame->radius = LittleFloat( frame->radius );
		if ( fixRadius ) {
			frame->radius = 256;
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = 128;
				frame->bounds[1][j] = -128;
				frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
			}
		}
		// Hack for Bug using plugin generated model
		else if ( frame->radius == 1 ) {
			frame->radius = 256;
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = 128;
				frame->bounds[1][j] = -128;
				frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
			}
		}
		else {
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
				frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
				frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
			}
		}
	}

	// swap all the tags
	tag = ( md3Tag_t * )( (byte *)hdr + hdr->ofsTags );
	for ( i = 0 ; i < hdr->numTags * hdr->numFrames ; i++, tag++ ) {
		// zero-terminate tag name
		tag->name[sizeof( tag->name ) - 1] = '\0';
		for ( j = 0 ; j < 3 ; j++ ) {
			tag->origin[j] = LittleFloat( tag->origin[j] );
			tag->axis[0][j] = LittleFloat( tag->axis[0][j] );
			tag->axis[1][j] = LittleFloat( tag->axis[1][j] );
			tag->axis[2][j] = LittleFloat( tag->axis[2][j] );
		}
	}

	// swap all the surfaces
	surf = ( md3Surface_t * )( (byte *)hdr + hdr->ofsSurfaces );
	for ( i = 0 ; i < hdr->numSurfaces ; i++ ) {

		// how many bytes are remaining before end of file
		bytesToEnd = size - ((byte*)surf - (byte*)hdr);

		if ( bytesToEnd < sizeof(*surf)) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}

		LL( surf->ident );
		LL( surf->flags );
		LL( surf->numFrames );
		LL( surf->numShaders );
		LL( surf->numTriangles );
		LL( surf->ofsTriangles );
		LL( surf->numVerts );
		LL( surf->ofsShaders );
		LL( surf->ofsSt );
		LL( surf->ofsXyzNormals );
		LL( surf->ofsEnd );

		if ( surf->ofsTriangles > bytesToEnd || surf->ofsShaders > bytesToEnd || surf->ofsSt > bytesToEnd ||
			 surf->ofsXyzNormals > bytesToEnd || surf->ofsEnd > bytesToEnd ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numTriangles > (bytesToEnd - surf->ofsTriangles) / sizeof( md3Triangle_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numShaders > (bytesToEnd - surf->ofsShaders) / sizeof( md3Shader_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numVerts > (bytesToEnd - surf->ofsSt) / sizeof( md3St_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numVerts > (bytesToEnd - surf->ofsXyzNormals) / sizeof( md3XyzNormal_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}

		if ( surf->numVerts >= SHADER_MAX_VERTEXES ) {
			ri.Printf(PRINT_WARNING, "%s: %s has more than %i verts on %s (%i).\n", __func__,
				mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
				surf->numVerts );
			return qfalse;
		}
		if ( surf->numTriangles >= SHADER_MAX_INDEXES / 3 ) {
			ri.Printf(PRINT_WARNING, "%s: %s has more than %i triangles on %s (%i).\n", __func__,
				mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, surf->name[0] ? surf->name : "a surface",
				surf->numTriangles );
			return qfalse;
		}

		// change to surface identifier
		surf->ident = SF_MD3;

		// zero-terminate surface name
		surf->name[sizeof( surf->name ) - 1] = '\0';

		// lowercase the surface name so skin compares are faster
		Q_strlwr( surf->name );

		// strip off a trailing _1 or _2
		// this is a crutch for q3data being a mess
		j = strlen( surf->name );
		if ( j > 2 && surf->name[j-2] == '_' ) {
			surf->name[j-2] = 0;
		}

		// register the shaders
		shader = (md3Shader_t *) ( (byte *)surf + surf->ofsShaders );
		for ( j = 0 ; j < surf->numShaders ; j++, shader++ ) {
			shader_t	*sh;

			// zero-terminate shader name
			shader->name[sizeof( shader->name ) - 1] = '\0';

			sh = R_FindShader( shader->name, LIGHTMAP_NONE, qtrue );
			if ( sh->defaultShader ) {
				shader->shaderIndex = 0;
			} else {
				shader->shaderIndex = sh->index;
			}
		}

		// Ridah, optimization, only do the swapping if we really need to
		if ( LittleShort( 1 ) != 1 ) {

			// swap all the triangles
			tri = ( md3Triangle_t * )( (byte *)surf + surf->ofsTriangles );
			for ( j = 0 ; j < surf->numTriangles ; j++, tri++ ) {
				LL( tri->indexes[0] );
				LL( tri->indexes[1] );
				LL( tri->indexes[2] );
				if (tri->indexes[0] >= surf->numVerts ||
					tri->indexes[1] >= surf->numVerts ||
					tri->indexes[2] >= surf->numVerts) {
					ri.Printf( PRINT_WARNING, "%s: %s has corrupted indexes\n", __func__, mod_name );
					return qfalse;
				}				
			}

			// swap all the ST
			st = ( md3St_t * )( (byte *)surf + surf->ofsSt );
			for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
				st->st[0] = LittleFloat( st->st[0] );
				st->st[1] = LittleFloat( st->st[1] );
			}

			// swap all the XyzNormals
			xyz = ( md3XyzNormal_t * )( (byte *)surf + surf->ofsXyzNormals );
			for ( j = 0 ; j < surf->numVerts * surf->numFrames ; j++, xyz++ )
			{
				xyz->xyz[0] = LittleShort( xyz->xyz[0] );
				xyz->xyz[1] = LittleShort( xyz->xyz[1] );
				xyz->xyz[2] = LittleShort( xyz->xyz[2] );

				xyz->normal = LittleShort( xyz->normal );
			}

		}
		// done.

		// find the next surface
		surf = ( md3Surface_t * )( (byte *)surf + surf->ofsEnd );
	}

	return qtrue;
}


/*
=================
R_LoadMDS
=================
*/
static qboolean R_LoadMDS( model_t *mod, void *buffer, int fileSize, const char *mod_name ) {
	int i, j, k;
	mdsHeader_t         *pinmodel, *mds;
	mdsFrame_t          *frame;
	mdsSurface_t        *surf;
	mdsTriangle_t       *tri;
	mdsVertex_t         *v;
	mdsBoneInfo_t       *bi;
	mdsTag_t            *tag;
	int version;
	int size;
	shader_t            *sh;
	int frameSize;
	int                 *collapseMap, *boneref;

	pinmodel = (mdsHeader_t *)buffer;

	version = LittleLong( pinmodel->version );
	if ( version != MDS_VERSION ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDS: %s has wrong version (%i should be %i)\n",
				   mod_name, version, MDS_VERSION );
		return qfalse;
	}

	size = LittleLong( pinmodel->ofsEnd );

	if ( size > fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	mod->type = MOD_MDS;
	mod->dataSize += size;
	mds = mod->model.mds = ri.Hunk_Alloc( size, h_low );

	memcpy( mds, buffer, LittleLong( pinmodel->ofsEnd ) );

	LL( mds->ident );
	LL( mds->version );
	LL( mds->numFrames );
	LL( mds->numBones );
	LL( mds->numTags );
	LL( mds->numSurfaces );
	LL( mds->ofsFrames );
	LL( mds->ofsBones );
	LL( mds->ofsTags );
	LL( mds->ofsEnd );
	LL( mds->ofsSurfaces );
	mds->lodBias = LittleFloat( mds->lodBias );
	mds->lodScale = LittleFloat( mds->lodScale );
	LL( mds->torsoParent );

	if ( mds->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDS: %s has no frames\n", mod_name );
		return qfalse;
	}

	if ( LittleLong( 1 ) != 1 ) {
		// swap all the frames
		//frameSize = (int)( &((mdsFrame_t *)0)->bones[ mds->numBones ] );
		frameSize = (int) ( sizeof( mdsFrame_t ) - sizeof( mdsBoneFrameCompressed_t ) + mds->numBones * sizeof( mdsBoneFrameCompressed_t ) );
		for ( i = 0 ; i < mds->numFrames ; i++, frame++ ) {
			frame = ( mdsFrame_t * )( (byte *)mds + mds->ofsFrames + i * frameSize );
			frame->radius = LittleFloat( frame->radius );
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
				frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
				frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
				frame->parentOffset[j] = LittleFloat( frame->parentOffset[j] );
			}
			for ( j = 0 ; j < mds->numBones * sizeof( mdsBoneFrameCompressed_t ) / sizeof( short ) ; j++ ) {
				( (short *)frame->bones )[j] = LittleShort( ( (short *)frame->bones )[j] );
			}
		}

		// swap all the tags
		tag = ( mdsTag_t * )( (byte *)mds + mds->ofsTags );
		for ( i = 0 ; i < mds->numTags ; i++, tag++ ) {
			LL( tag->boneIndex );
			tag->torsoWeight = LittleFloat( tag->torsoWeight );
		}

		// swap all the bones
		for ( i = 0 ; i < mds->numBones ; i++, bi++ ) {
			bi = ( mdsBoneInfo_t * )( (byte *)mds + mds->ofsBones + i * sizeof( mdsBoneInfo_t ) );
			LL( bi->parent );
			bi->torsoWeight = LittleFloat( bi->torsoWeight );
			bi->parentDist = LittleFloat( bi->parentDist );
			LL( bi->flags );
		}
	}

	// swap all the surfaces
	surf = ( mdsSurface_t * )( (byte *)mds + mds->ofsSurfaces );
	for ( i = 0 ; i < mds->numSurfaces ; i++ ) {
		if ( LittleLong( 1 ) != 1 ) {
			//LL(surf->ident);
			LL( surf->shaderIndex );
			LL( surf->minLod );
			LL( surf->ofsHeader );
			LL( surf->ofsCollapseMap );
			LL( surf->numTriangles );
			LL( surf->ofsTriangles );
			LL( surf->numVerts );
			LL( surf->ofsVerts );
			LL( surf->numBoneReferences );
			LL( surf->ofsBoneReferences );
			LL( surf->ofsEnd );
		}

		// change to surface identifier
		surf->ident = SF_MDS;

		if ( surf->numVerts >= SHADER_MAX_VERTEXES ) {
			ri.Printf(PRINT_WARNING, "R_LoadMDS: %s has more than %i verts on %s (%i).\n",
				mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
				surf->numVerts );
			return qfalse;
		}
		if ( surf->numTriangles*3 >= SHADER_MAX_INDEXES ) {
			ri.Printf(PRINT_WARNING, "R_LoadMDS: %s has more than %i triangles on %s (%i).\n",
				mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, surf->name[0] ? surf->name : "a surface",
				surf->numTriangles );
			return qfalse;
		}

		// register the shaders
		if ( surf->shader[0] ) {
			sh = R_FindShader( surf->shader, LIGHTMAP_NONE, qtrue );
			if ( sh->defaultShader ) {
				surf->shaderIndex = 0;
			} else {
				surf->shaderIndex = sh->index;
			}
		} else {
			surf->shaderIndex = 0;
		}

		if ( LittleLong( 1 ) != 1 ) {
			// swap all the triangles
			tri = ( mdsTriangle_t * )( (byte *)surf + surf->ofsTriangles );
			for ( j = 0 ; j < surf->numTriangles ; j++, tri++ ) {
				LL( tri->indexes[0] );
				LL( tri->indexes[1] );
				LL( tri->indexes[2] );
			}

			// swap all the vertexes
			v = ( mdsVertex_t * )( (byte *)surf + surf->ofsVerts );
			for ( j = 0 ; j < surf->numVerts ; j++ ) {
				v->normal[0] = LittleFloat( v->normal[0] );
				v->normal[1] = LittleFloat( v->normal[1] );
				v->normal[2] = LittleFloat( v->normal[2] );

				v->texCoords[0] = LittleFloat( v->texCoords[0] );
				v->texCoords[1] = LittleFloat( v->texCoords[1] );

				v->numWeights = LittleLong( v->numWeights );

				for ( k = 0 ; k < v->numWeights ; k++ ) {
					v->weights[k].boneIndex = LittleLong( v->weights[k].boneIndex );
					v->weights[k].boneWeight = LittleFloat( v->weights[k].boneWeight );
					v->weights[k].offset[0] = LittleFloat( v->weights[k].offset[0] );
					v->weights[k].offset[1] = LittleFloat( v->weights[k].offset[1] );
					v->weights[k].offset[2] = LittleFloat( v->weights[k].offset[2] );
				}

				// find the fixedParent for this vert (if exists)
				v->fixedParent = -1;
				if ( v->numWeights == 2 ) {
					// find the closest parent
					if ( VectorLength( v->weights[0].offset ) < VectorLength( v->weights[1].offset ) ) {
						v->fixedParent = 0;
					} else {
						v->fixedParent = 1;
					}
					v->fixedDist = VectorLength( v->weights[v->fixedParent].offset );
				}

				v = (mdsVertex_t *)&v->weights[v->numWeights];
			}

			// swap the collapse map
			collapseMap = ( int * )( (byte *)surf + surf->ofsCollapseMap );
			for ( j = 0; j < surf->numVerts; j++, collapseMap++ ) {
				*collapseMap = LittleLong( *collapseMap );
			}

			// swap the bone references
			boneref = ( int * )( ( byte *)surf + surf->ofsBoneReferences );
			for ( j = 0; j < surf->numBoneReferences; j++, boneref++ ) {
				*boneref = LittleLong( *boneref );
			}
		}

		// find the next surface
		surf = ( mdsSurface_t * )( (byte *)surf + surf->ofsEnd );
	}

	return qtrue;
}

/*
=================
R_LoadMDM
=================
*/
static qboolean R_LoadMDM( model_t *mod, void *buffer, int fileSize, const char *mod_name ) {
	int i, j, k;
	mdmHeader_t         *pinmodel, *mdm;
//    mdmFrame_t			*frame;
	mdmSurface_t        *surf;
	mdmTriangle_t       *tri;
	mdmVertex_t         *v;
	mdmTag_t            *tag;
	int version;
	int size;
	shader_t            *sh;
	int                 *collapseMap, *boneref;

	pinmodel = (mdmHeader_t *)buffer;

	version = LittleLong( pinmodel->version );
	if ( version != MDM_VERSION ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDM: %s has wrong version (%i should be %i)\n",
				   mod_name, version, MDM_VERSION );
		return qfalse;
	}

	size = LittleLong( pinmodel->ofsEnd );

	if ( size > fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	mod->type = MOD_MDM;
	mod->dataSize += size;
	mod->model.mdm = ri.Hunk_Alloc( size, h_low );

	memcpy( mod->model.mdm, buffer, size );

	mdm = mod->model.mdm;

	LL( mdm->ident );
	LL( mdm->version );
//    LL(mdm->numFrames);
	LL( mdm->numTags );
	LL( mdm->numSurfaces );
//    LL(mdm->ofsFrames);
	LL( mdm->ofsTags );
	LL( mdm->ofsEnd );
	LL( mdm->ofsSurfaces );
	mdm->lodBias = LittleFloat( mdm->lodBias );
	mdm->lodScale = LittleFloat( mdm->lodScale );

	if ( mdm->ofsTags > size || mdm->ofsSurfaces > size ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}
	if ( mdm->ofsSurfaces + ( mdm->numSurfaces ? 1 : 0 ) * sizeof( mdmSurface_t ) > fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

/*	mdm->skel = RE_RegisterModel(mdm->bonesfile);
	if ( !mdm->skel ) {
		ri.Error (ERR_DROP, "R_LoadMDM: %s skeleton not found\n", mdm->bonesfile );
	}

	if ( mdm->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDM: %s has no frames\n", mod_name );
		return qfalse;
	}*/

	if ( LittleLong( 1 ) != 1 ) {
		// swap all the frames
		/*frameSize = (int) ( sizeof( mdmFrame_t ) );
		for ( i = 0 ; i < mdm->numFrames ; i++, frame++) {
			frame = (mdmFrame_t *) ( (byte *)mdm + mdm->ofsFrames + i * frameSize );
			frame->radius = LittleFloat( frame->radius );
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
				frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
				frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
				frame->parentOffset[j] = LittleFloat( frame->parentOffset[j] );
			}
		}*/

		// swap all the tags
		tag = ( mdmTag_t * )( (byte *)mdm + mdm->ofsTags );
		for ( i = 0 ; i < mdm->numTags ; i++ ) {
			int ii;
			// zero-terminate tag name
			tag->name[sizeof( tag->name ) - 1] = '\0';
			for ( ii = 0; ii < 3; ii++ )
			{
				tag->axis[ii][0] = LittleFloat( tag->axis[ii][0] );
				tag->axis[ii][1] = LittleFloat( tag->axis[ii][1] );
				tag->axis[ii][2] = LittleFloat( tag->axis[ii][2] );
			}

			LL( tag->boneIndex );
			//tag->torsoWeight = LittleFloat( tag->torsoWeight );
			tag->offset[0] = LittleFloat( tag->offset[0] );
			tag->offset[1] = LittleFloat( tag->offset[1] );
			tag->offset[2] = LittleFloat( tag->offset[2] );

			LL( tag->numBoneReferences );
			LL( tag->ofsBoneReferences );
			LL( tag->ofsEnd );

			// swap the bone references
			boneref = ( int * )( ( byte *)tag + tag->ofsBoneReferences );
			for ( j = 0; j < tag->numBoneReferences; j++, boneref++ ) {
				*boneref = LittleLong( *boneref );
			}

			// find the next tag
			tag = ( mdmTag_t * )( (byte *)tag + tag->ofsEnd );
		}
	}

	// swap all the surfaces
	surf = ( mdmSurface_t * )( (byte *)mdm + mdm->ofsSurfaces );
	for ( i = 0 ; i < mdm->numSurfaces ; i++ ) {
		if ( LittleLong( 1 ) != 1 ) {
			//LL(surf->ident);
			LL( surf->shaderIndex );
			LL( surf->minLod );
			LL( surf->ofsHeader );
			LL( surf->ofsCollapseMap );
			LL( surf->numTriangles );
			LL( surf->ofsTriangles );
			LL( surf->numVerts );
			LL( surf->ofsVerts );
			LL( surf->numBoneReferences );
			LL( surf->ofsBoneReferences );
			LL( surf->ofsEnd );
		}

		if ( surf->ofsEnd > fileSize || (((byte*)surf - (byte*)mdm) + surf->ofsEnd) > fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->ofsCollapseMap + surf->numVerts * sizeof( int32_t ) > fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->ofsTriangles > fileSize || surf->ofsCollapseMap > fileSize || surf->ofsVerts > fileSize || surf->ofsBoneReferences > fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->ofsTriangles + surf->numTriangles * sizeof( mdmTriangle_t ) > fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->ofsVerts + surf->numVerts * sizeof( mdmVertex_t ) > fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->ofsBoneReferences + surf->numBoneReferences * sizeof( int32_t ) > fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}

		if ( surf->numVerts >= SHADER_MAX_VERTEXES ) {
			ri.Printf( PRINT_WARNING, "R_LoadMDM: %s has more than %i verts on %s (%i).\n",
				mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
				surf->numVerts );
			return qfalse;
		}
		if ( surf->numTriangles * 3 >= SHADER_MAX_INDEXES ) {
			ri.Printf( PRINT_WARNING, "R_LoadMDM: %s has more than %i triangles on %s (%i).\n",
				mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, surf->name[0] ? surf->name : "a surface",
				surf->numTriangles );
			return qfalse;
		}

		// change to surface identifier
		surf->ident = SF_MDM;

		// zero-terminate surface name
		surf->name[sizeof( surf->name ) - 1] = '\0';

		// register the shaders
		if ( surf->shader[0] ) {
			// zero-terminate shader name
			surf->shader[sizeof( surf->shader ) - 1] = '\0';
			sh = R_FindShader( surf->shader, LIGHTMAP_NONE, qtrue );
			if ( sh->defaultShader ) {
				surf->shaderIndex = 0;
			} else {
				surf->shaderIndex = sh->index;
			}
		} else {
			surf->shaderIndex = 0;
		}

		if ( LittleLong( 1 ) != 1 ) {
			// swap all the triangles
			tri = ( mdmTriangle_t * )( (byte *)surf + surf->ofsTriangles );
			for ( j = 0 ; j < surf->numTriangles ; j++, tri++ ) {
				LL( tri->indexes[0] );
				LL( tri->indexes[1] );
				LL( tri->indexes[2] );
			}

			// swap all the vertexes
			v = ( mdmVertex_t * )( (byte *)surf + surf->ofsVerts );
			for ( j = 0 ; j < surf->numVerts ; j++ ) {
				v->normal[0] = LittleFloat( v->normal[0] );
				v->normal[1] = LittleFloat( v->normal[1] );
				v->normal[2] = LittleFloat( v->normal[2] );

				v->texCoords[0] = LittleFloat( v->texCoords[0] );
				v->texCoords[1] = LittleFloat( v->texCoords[1] );

				v->numWeights = LittleLong( v->numWeights );

				for ( k = 0 ; k < v->numWeights ; k++ ) {
					v->weights[k].boneIndex = LittleLong( v->weights[k].boneIndex );
					v->weights[k].boneWeight = LittleFloat( v->weights[k].boneWeight );
					v->weights[k].offset[0] = LittleFloat( v->weights[k].offset[0] );
					v->weights[k].offset[1] = LittleFloat( v->weights[k].offset[1] );
					v->weights[k].offset[2] = LittleFloat( v->weights[k].offset[2] );
				}

				v = (mdmVertex_t *)&v->weights[v->numWeights];
			}

			// swap the collapse map
			collapseMap = ( int * )( (byte *)surf + surf->ofsCollapseMap );
			for ( j = 0; j < surf->numVerts; j++, collapseMap++ ) {
				*collapseMap = LittleLong( *collapseMap );
			}

			// swap the bone references
			boneref = ( int * )( ( byte *)surf + surf->ofsBoneReferences );
			for ( j = 0; j < surf->numBoneReferences; j++, boneref++ ) {
				*boneref = LittleLong( *boneref );
			}
		}

		// find the next surface
		surf = ( mdmSurface_t * )( (byte *)surf + surf->ofsEnd );
	}

	return qtrue;
}

/*
=================
R_LoadMDX
=================
*/
static qboolean R_LoadMDX( model_t *mod, void *buffer, int fileSize, const char *mod_name ) {
	int i, j;
	mdxHeader_t                 *pinmodel, *mdx;
	mdxFrame_t                  *frame;
	short                       *bframe;
	mdxBoneInfo_t               *bi;
	int version;
	int size;
	int frameSize;

	pinmodel = (mdxHeader_t *)buffer;

	version = LittleLong( pinmodel->version );
	if ( version != MDX_VERSION ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDX: %s has wrong version (%i should be %i)\n",
				   mod_name, version, MDX_VERSION );
		return qfalse;
	}

	size = LittleLong( pinmodel->ofsEnd );

	if ( size > fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	mod->type = MOD_MDX;
	mod->dataSize += size;
	mod->model.mdx = ri.Hunk_Alloc( size, h_low );

	memcpy( mod->model.mdx, buffer, size );

	mdx = mod->model.mdx;

	LL( mdx->ident );
	LL( mdx->version );
	LL( mdx->numFrames );
	LL( mdx->numBones );
	LL( mdx->ofsFrames );
	LL( mdx->ofsBones );
	LL( mdx->ofsEnd );
	LL( mdx->torsoParent );

	if ( mdx->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "%s: %s has no frames\n", __func__, mod_name );
		return qfalse;
	}

	if ( mdx->ofsFrames > size || mdx->ofsBones > size ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	if ( LittleLong( 1 ) != 1 ) {
		// swap all the frames
		frameSize = (int) ( sizeof( mdxBoneFrameCompressed_t ) ) * mdx->numBones;
		for ( i = 0 ; i < mdx->numFrames ; i++ ) {
			frame = ( mdxFrame_t * )( (byte *)mdx + mdx->ofsFrames + i * frameSize + i * sizeof( mdxFrame_t ) );
			frame->radius = LittleFloat( frame->radius );
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
				frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
				frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
				frame->parentOffset[j] = LittleFloat( frame->parentOffset[j] );
			}

			bframe = ( short * )( (byte *)mdx + mdx->ofsFrames + i * frameSize + ( ( i + 1 ) * sizeof( mdxFrame_t ) ) );
			for ( j = 0 ; j < mdx->numBones * sizeof( mdxBoneFrameCompressed_t ) / sizeof( short ) ; j++ ) {
				( (short *)bframe )[j] = LittleShort( ( (short *)bframe )[j] );
			}
		}

		// swap all the bones
		for ( i = 0 ; i < mdx->numBones ; i++ ) {
			bi = ( mdxBoneInfo_t * )( (byte *)mdx + mdx->ofsBones + i * sizeof( mdxBoneInfo_t ) );
			LL( bi->parent );
			bi->torsoWeight = LittleFloat( bi->torsoWeight );
			bi->parentDist = LittleFloat( bi->parentDist );
			LL( bi->flags );
		}
	}

	return qtrue;
}


//=============================================================================

/*
** RE_BeginRegistration
*/
void RE_BeginRegistration( glconfig_t *glconfigOut ) {
	//ri.Hunk_ClearToMark();    // (SA) MEM NOTE: not in missionpack

	R_Init();

	*glconfigOut = glConfig;

	//R_IssuePendingRenderCommands();

	tr.viewCluster = -1;		// force markleafs to regenerate
	R_ClearFlares();
	RE_ClearScene();

	tr.registered = qtrue;
}

/*
===============
R_ModelInit
===============
*/
void R_ModelInit( void ) {
	model_t     *mod;

	// leave a space for NULL model
	tr.numModels = 0;

	mod = R_AllocModel();
	mod->type = MOD_BAD;
}


/*
================
R_Modellist_f
================
*/
void R_Modellist_f( void ) {
	int i;
	const model_t *mod;
	int total = 0, models = 0;
	const char *match;
	if ( ri.Cmd_Argc() > 1 ) {
		match = ri.Cmd_Argv( 1 );
	} else {
		match = NULL;
	}

	total = 0;
	for ( i = 1 ; i < tr.numModels; i++ ) {
		mod = tr.models[i];

		if ( match && !ri.Com_Filter( match, mod->name ) ) {
			continue;
		}

		ri.Printf( PRINT_ALL, "%8i : (%i) %s\n", mod->dataSize, mod->numLods, mod->name );
		total += mod->dataSize;
		models++;
	}
	ri.Printf( PRINT_ALL, "%8i : %i models found\n", total, models );

#if 0       // not working right with new hunk
	if ( tr.world ) {
		ri.Printf( PRINT_ALL, "\n%8i : %s\n", tr.world->dataSize, tr.world->name );
	}
#endif
}


//=============================================================================


/*
================
R_GetTag
================
*/
static int R_GetTag( byte *mod, int frame, const char *tagName, int startTagIndex, md3Tag_t **outTag ) {
	md3Tag_t        *tag;
	int i;
	md3Header_t     *md3;

	md3 = (md3Header_t *) mod;

	if ( frame >= md3->numFrames ) {
		// it is possible to have a bad frame while changing models, so don't error
		frame = md3->numFrames - 1;
	}

	if ( startTagIndex > md3->numTags ) {
		*outTag = NULL;
		return -1;
	}

	tag = ( md3Tag_t * )( (byte *)mod + md3->ofsTags ) + frame * md3->numTags;
	for ( i = 0 ; i < md3->numTags ; i++, tag++ ) {
		if ( ( i >= startTagIndex ) && !strcmp( tag->name, tagName ) ) {

			// if we are looking for an indexed tag, wait until we find the correct number of matches
			//if (startTagIndex) {
			//	startTagIndex--;
			//	continue;
			//}

			*outTag = tag;
			return i;   // found it
		}
	}

	*outTag = NULL;
	return -1;
}

/*
================
R_GetMDCTag
================
*/
static int R_GetMDCTag( byte *mod, int frame, const char *tagName, int startTagIndex, mdcTag_t **outTag ) {
	mdcTag_t        *tag;
	mdcTagName_t    *pTagName;
	int i;
	mdcHeader_t     *mdc;

	mdc = (mdcHeader_t *) mod;

	if ( frame >= mdc->numFrames ) {
		// it is possible to have a bad frame while changing models, so don't error
		frame = mdc->numFrames - 1;
	}

	if ( startTagIndex > mdc->numTags ) {
		*outTag = NULL;
		return -1;
	}

	pTagName = ( mdcTagName_t * )( (byte *)mod + mdc->ofsTagNames );
	for ( i = 0 ; i < mdc->numTags ; i++, pTagName++ ) {
		if ( ( i >= startTagIndex ) && !strcmp( pTagName->name, tagName ) ) {
			break;  // found it
		}
	}

	if ( i >= mdc->numTags ) {
		*outTag = NULL;
		return -1;
	}

	tag = ( mdcTag_t * )( (byte *)mod + mdc->ofsTags ) + frame * mdc->numTags + i;
	*outTag = tag;
	return i;
}

/*
================
R_GetMDSTag
================
*/
/*
// TTimo: unused
static int R_GetMDSTag( byte *mod, const char *tagName, int startTagIndex, mdsTag_t **outTag ) {
	mdsTag_t		*tag;
	int				i;
	mdsHeader_t		*mds;

	mds = (mdsHeader_t *) mod;

	if (startTagIndex > mds->numTags) {
		*outTag = NULL;
		return -1;
	}

	tag = (mdsTag_t *)((byte *)mod + mds->ofsTags);
	for ( i = 0 ; i < mds->numTags ; i++ ) {
		if ( (i >= startTagIndex) && !strcmp( tag->name, tagName ) ) {
			break;	// found it
		}

		tag = (mdsTag_t *) ((byte *)tag + sizeof(mdsTag_t) - sizeof(mdsBoneFrameCompressed_t) + mds->numFrames * sizeof(mdsBoneFrameCompressed_t) );
	}

	if (i >= mds->numTags) {
		*outTag = NULL;
		return -1;
	}

	*outTag = tag;
	return i;
}
*/

/*
================
R_LerpTag

  returns the index of the tag it found, for cycling through tags with the same name
================
*/
int R_LerpTag( orientation_t *tag, const refEntity_t *refent, const char *tagNameIn, int startIndex ) {
	md3Tag_t    *start, *end;
	md3Tag_t ustart, uend;
	int i;
	float frontLerp, backLerp;
	model_t     *model;
	vec3_t sangles, eangles;
	char tagName[MAX_QPATH];       //, *ch;
	int retval;
	qhandle_t handle;
	int startFrame, endFrame;
	float frac;

	handle = refent->hModel;
	startFrame = refent->oldframe;
	endFrame = refent->frame;
	frac = 1.0 - refent->backlerp;

	Q_strncpyz( tagName, tagNameIn, MAX_QPATH );
/*
	// if the tagName has a space in it, then it is passing through the starting tag number
	if (ch = strrchr(tagName, ' ')) {
		*ch = 0;
		ch++;
		startIndex = atoi(ch);
	}
*/
	model = R_GetModelByHandle( handle );
	if ( !model->model.md3[0] && !model->model.mdc[0] && !model->model.mds && !model->model.iqm ) {
		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return -1;
	}

	frontLerp = frac;
	backLerp = 1.0 - frac;

	if ( model->type == MOD_IQM ) {
		return R_IQMLerpTag( tag, model->model.iqm,
			startFrame, endFrame,
			frac, tagName );
	} else if ( model->type == MOD_MESH ) {
		// old MD3 style
		retval = R_GetTag( (byte *)model->model.md3[0], startFrame, tagName, startIndex, &start );
		retval = R_GetTag( (byte *)model->model.md3[0], endFrame, tagName, startIndex, &end );

	} else if ( model->type == MOD_MDS ) {    // use bone lerping

		retval = R_GetBoneTag( tag, model->model.mds, startIndex, refent, tagNameIn );

		if ( retval >= 0 ) {
			return retval;
		}

		// failed
		return -1;

	} else if ( model->type == MOD_MDM ) {    // use bone lerping

		retval = R_MDM_GetBoneTag( tag, model->model.mdm, startIndex, refent, tagNameIn );

		if ( retval >= 0 ) {
			return retval;
		}

		// failed
		return -1;

	} else {
		// psuedo-compressed MDC tags
		mdcTag_t    *cstart, *cend;

		retval = R_GetMDCTag( (byte *)model->model.mdc[0], startFrame, tagName, startIndex, &cstart );
		retval = R_GetMDCTag( (byte *)model->model.mdc[0], endFrame, tagName, startIndex, &cend );

		// uncompress the MDC tags into MD3 style tags
		if ( cstart && cend ) {
			for ( i = 0; i < 3; i++ ) {
				ustart.origin[i] = (float)cstart->xyz[i] * MD3_XYZ_SCALE;
				uend.origin[i] = (float)cend->xyz[i] * MD3_XYZ_SCALE;
				sangles[i] = (float)cstart->angles[i] * MDC_TAG_ANGLE_SCALE;
				eangles[i] = (float)cend->angles[i] * MDC_TAG_ANGLE_SCALE;
			}

			AnglesToAxis( sangles, ustart.axis );
			AnglesToAxis( eangles, uend.axis );

			start = &ustart;
			end = &uend;
		} else {
			start = NULL;
			end = NULL;
		}

	}

	if ( !start || !end ) {
		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return -1;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		tag->origin[i] = start->origin[i] * backLerp +  end->origin[i] * frontLerp;
		tag->axis[0][i] = start->axis[0][i] * backLerp +  end->axis[0][i] * frontLerp;
		tag->axis[1][i] = start->axis[1][i] * backLerp +  end->axis[1][i] * frontLerp;
		tag->axis[2][i] = start->axis[2][i] * backLerp +  end->axis[2][i] * frontLerp;
	}

	VectorNormalize( tag->axis[0] );
	VectorNormalize( tag->axis[1] );
	VectorNormalize( tag->axis[2] );

	return retval;
}

/*
===============
R_TagInfo_f
===============
*/
/*void R_TagInfo_f( void ) {

	Com_Printf( "command not functional\n" );

	int handle;
	orientation_t tag;
	int frame = -1;

	if (ri.Cmd_Argc() < 3) {
		Com_Printf("usage: taginfo <model> <tag>\n");
		return;
	}

	handle = RE_RegisterModel( ri.Cmd_Argv(1) );

	if (handle) {
		Com_Printf("found model %s..\n", ri.Cmd_Argv(1));
	} else {
		Com_Printf("cannot find model %s\n", ri.Cmd_Argv(1));
		return;
	}

	if (ri.Cmd_Argc() < 3) {
		frame = 0;
	} else {
		frame = atoi(ri.Cmd_Argv(3));
	}

	Com_Printf("using frame %i..\n", frame);

	R_LerpTag( &tag, handle, frame, frame, 0.0, (const char *)ri.Cmd_Argv(2) );

	Com_Printf("%s at position: %.1f %.1f %.1f\n", ri.Cmd_Argv(2), tag.origin[0], tag.origin[1], tag.origin[2] );

}*/

/*
====================
R_ModelBounds
====================
*/
void R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs ) {
	model_t     *model;
	md3Header_t *header;
	md3Frame_t  *frame;
	iqmData_t   *iqmData;

	model = R_GetModelByHandle( handle );

	// Gordon: fixing now that it's a union
	switch ( model->type ) {
	case MOD_BRUSH:
		VectorCopy( model->model.bmodel->bounds[0], mins );
		VectorCopy( model->model.bmodel->bounds[1], maxs );
		return;
	case MOD_MESH:
		header = model->model.md3[0];

		frame = ( md3Frame_t * )( (byte *)header + header->ofsFrames );

		VectorCopy( frame->bounds[0], mins );
		VectorCopy( frame->bounds[1], maxs );
		return;
	case MOD_MDC:
		frame = ( md3Frame_t * )( (byte *)model->model.mdc[0] + model->model.mdc[0]->ofsFrames );

		VectorCopy( frame->bounds[0], mins );
		VectorCopy( frame->bounds[1], maxs );
		return;
	case MOD_IQM:
		iqmData = model->model.iqm;

		if ( iqmData->bounds ) {
			VectorCopy( iqmData->bounds, mins );
			VectorCopy( iqmData->bounds + 3, maxs );
			return;
		}
		break;
	default:
		break;
	}

	VectorClear( mins );
	VectorClear( maxs );
	// done.
}
