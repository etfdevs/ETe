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

// tr_shader.c -- this file deals with the parsing and definition of shaders

static char *s_shaderText;

static const char *s_extensionOffset;
static int s_extendedShader;

// the shader is parsed into these global variables, then copied into
// dynamically allocated memory if it is valid.
static shaderStage_t stages[MAX_SHADER_STAGES];
static shader_t shader;
static texModInfo_t texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS];

// ydnar: these are here because they are only referenced while parsing a shader
static char implicitMap[ MAX_QPATH ];
static GLbitfield implicitStateBits;
static cullType_t implicitCullType;

#define FILE_HASH_SIZE      4096
static shader_t*       hashTable[FILE_HASH_SIZE];

#define MAX_SHADERTEXT_HASH		2048
static char **shaderTextHashTable[MAX_SHADERTEXT_HASH];

/*
================
return a hash value for the filename
================
*/
#ifdef __GNUCC__
  #warning TODO: check if long is ok here 
#endif

#define generateHashValue Com_GenerateHashValue

void RE_RemapShader(const char *shaderName, const char *newShaderName, const char *timeOffset) {
	char		strippedName[MAX_QPATH];
	int			hash;
	shader_t	*sh, *sh2;
	qhandle_t	h;

	sh = R_FindShaderByName( shaderName );
	if (sh == NULL || sh == tr.defaultShader) {
		h = RE_RegisterShaderLightMap(shaderName, 0);
		sh = R_GetShaderByHandle(h);
	}
	if (sh == NULL || sh == tr.defaultShader) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_RemapShader: shader %s not found\n", shaderName );
		return;
	}

	sh2 = R_FindShaderByName( newShaderName );
	if (sh2 == NULL || sh2 == tr.defaultShader) {
		h = RE_RegisterShaderLightMap(newShaderName, 0);
		sh2 = R_GetShaderByHandle(h);
	}

	if (sh2 == NULL || sh2 == tr.defaultShader) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_RemapShader: new shader %s not found\n", newShaderName );
		return;
	}

	// remap all the shaders with the given name
	// even tho they might have different lightmaps
	COM_StripExtension(shaderName, strippedName, sizeof(strippedName));
	hash = generateHashValue(strippedName, FILE_HASH_SIZE);
	for (sh = hashTable[hash]; sh; sh = sh->next) {
		if (Q_stricmp(sh->name, strippedName) == 0) {
			if (sh != sh2) {
				sh->remappedShader = sh2;
			} else {
				sh->remappedShader = NULL;
			}
		}
	}

	if ( timeOffset ) {
		sh2->timeOffset = Q_atof( timeOffset );
	}
}


/*
===============
ParseVector
===============
*/
static qboolean ParseVector( const char **text, int count, float *v ) {
	const char *token;
	int		i;

	// FIXME: spaces are currently required after parens, should change parseext...
	token = COM_ParseExt( text, qfalse );
	if ( strcmp( token, "(" ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name );
		return qfalse;
	}

	for ( i = 0 ; i < count ; i++ ) {
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name );
			return qfalse;
		}
		v[i] = Q_atof( token );
	}

	token = COM_ParseExt( text, qfalse );
	if ( strcmp( token, ")" ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name );
		return qfalse;
	}

	return qtrue;
}


/*
===============
NameToAFunc
===============
*/
static unsigned NameToAFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "GT0" ) )
	{
		return GLS_ATEST_GT_0;
	}
	else if ( !Q_stricmp( funcname, "LT128" ) )
	{
		return GLS_ATEST_LT_80;
	}
	else if ( !Q_stricmp( funcname, "GE128" ) )
	{
		return GLS_ATEST_GE_80;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname, shader.name );
	return 0;
}


/*
===============
NameToSrcBlendMode
===============
*/
static int NameToSrcBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_SRCBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_SRCBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ) )
	{
		return GLS_SRCBLEND_ALPHA_SATURATE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_SRCBLEND_ONE;
}

/*
===============
NameToDstBlendMode
===============
*/
static int NameToDstBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_DSTBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_DSTBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_SRC_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_DSTBLEND_ONE;
}


/*
===============
NameToGenFunc
===============
*/
static genFunc_t NameToGenFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "sin" ) )
	{
		return GF_SIN;
	}
	else if ( !Q_stricmp( funcname, "square" ) )
	{
		return GF_SQUARE;
	}
	else if ( !Q_stricmp( funcname, "triangle" ) )
	{
		return GF_TRIANGLE;
	}
	else if ( !Q_stricmp( funcname, "sawtooth" ) )
	{
		return GF_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "inversesawtooth" ) )
	{
		return GF_INVERSE_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "noise" ) )
	{
		return GF_NOISE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, shader.name );
	return GF_SIN;
}


/*
===================
ParseWaveForm
===================
*/
static void ParseWaveForm( const char **text, waveForm_t *wave )
{
	const char *token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->func = NameToGenFunc( token );

	// BASE, AMP, PHASE, FREQ
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->base = Q_atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->amplitude = Q_atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->phase = Q_atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->frequency = Q_atof( token );
}


/*
===================
ParseTexMod
===================
*/
static void ParseTexMod( const char *_text, shaderStage_t *stage )
{
	const char *token;
	const char **text = &_text;
	texModInfo_t *tmi;

	if ( stage->bundle[0].numTexMods == TR_MAX_TEXMODS ) {
		ri.Error( ERR_DROP, "ERROR: too many tcMod stages in shader '%s'", shader.name );
		return;
	}

	tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
	stage->bundle[0].numTexMods++;

	token = COM_ParseExt( text, qfalse );

	//
	// swap
	//
	if ( !Q_stricmp( token, "swap" ) ) { // swap S/T coords (rotate 90d)
		tmi->type = TMOD_SWAP;
	}
	//
	// turb
	//
	// (SA) added 'else' so it wouldn't claim 'swap' was unknown.
	else if ( !Q_stricmp( token, "turb" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = Q_atof( token );

		tmi->type = TMOD_TURBULENT;
	}
	//
	// scale
	//
	else if ( !Q_stricmp( token, "scale" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[0] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[1] = Q_atof( token );
		tmi->type = TMOD_SCALE;
	}
	//
	// scroll
	//
	else if ( !Q_stricmp( token, "scroll" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[0] = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[1] = Q_atof( token );
		tmi->type = TMOD_SCROLL;
	}
	//
	// stretch
	//
	else if ( !Q_stricmp( token, "stretch" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.func = NameToGenFunc( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = Q_atof( token );

		tmi->type = TMOD_STRETCH;
	}
	//
	// transform
	//
	else if ( !Q_stricmp( token, "transform" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][0] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][1] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][0] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][1] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[0] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[1] = Q_atof( token );

		tmi->type = TMOD_TRANSFORM;
	}
	//
	// rotate
	//
	else if ( !Q_stricmp( token, "rotate" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->rotateSpeed = Q_atof( token );
		tmi->type = TMOD_ROTATE;
	}
	//
	// entityTranslate
	//
	else if ( !Q_stricmp( token, "entityTranslate" ) )
	{
		tmi->type = TMOD_ENTITY_TRANSLATE;
	}
	else
	{
		ri.Printf( PRINT_WARNING, "WARNING: unknown tcMod '%s' in shader '%s'\n", token, shader.name );
	}
}


/*
===================
ParseStage
===================
*/
static qboolean ParseStage( shaderStage_t *stage, const char **text )
{
	const char *token;
	GLbitfield depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0;
	qboolean depthMaskExplicit = qfalse;

	stage->active = qtrue;

	while ( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no matching '}' found\n" );
			return qfalse;
		}

		if ( token[0] == '}' )
		{
			break;
		}
		//
		// check special case for map16/map32/mapcomp/mapnocomp (compression enabled)
		if ( !Q_stricmp( token, "map16" ) ) {    // only use this texture if 16 bit color depth
			if ( glConfig.colorBits <= 16 ) {
				token = "map";   // use this map
			} else {
				COM_ParseExt( text, qfalse );   // ignore the map
				continue;
			}
		} else if ( !Q_stricmp( token, "map32" ) )    { // only use this texture if 16 bit color depth
			if ( glConfig.colorBits > 16 ) {
				token = "map";   // use this map
			} else {
				COM_ParseExt( text, qfalse );   // ignore the map
				continue;
			}
		} else if ( !Q_stricmp( token, "mapcomp" ) )    { // only use this texture if compression is enabled
			if ( glConfig.textureCompression != TC_NONE ) {
				token = "map";   // use this map
			} else {
				COM_ParseExt( text, qfalse );   // ignore the map
				continue;
			}
		} else if ( !Q_stricmp( token, "mapnocomp" ) )    { // only use this texture if compression is not available or disabled
			if ( glConfig.textureCompression == TC_NONE ) {
				token = "map";   // use this map
			} else {
				COM_ParseExt( text, qfalse );   // ignore the map
				continue;
			}
		} else if ( !Q_stricmp( token, "animmapcomp" ) )    { // only use this texture if compression is enabled
			if ( glConfig.textureCompression != TC_NONE ) {
				token = "animmap";   // use this map
			} else {
				while ( token[0] )
					COM_ParseExt( text, qfalse );   // ignore the map
				continue;
			}
		} else if ( !Q_stricmp( token, "animmapnocomp" ) )    { // only use this texture if compression is not available or disabled
			if ( glConfig.textureCompression == TC_NONE ) {
				token = "animmap";   // use this map
			} else {
				while ( token[0] )
					COM_ParseExt( text, qfalse );   // ignore the map
				continue;
			}
		}
		//
		// map <name>
		//
		else if ( !Q_stricmp( token, "map" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'map' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

//----(SA)	fixes startup error and allows polygon shadows to work again
			if ( !Q_stricmp( token, "$whiteimage" ) || !Q_stricmp( token, "*white" ) )
			{
//----(SA)	end
				stage->bundle[0].image[0] = tr.whiteImage;
				continue;
			}
//----(SA) added
			else if ( !Q_stricmp( token, "$dlight" ) ) {
				stage->bundle[0].image[0] = tr.dlightImage;
				continue;
			}
//----(SA) end
			else if ( !Q_stricmp( token, "$lightmap" ) )
			{
				stage->bundle[0].isLightmap = qtrue;
				if ( shader.lightmapIndex < 0 || !tr.lightmaps[shader.lightmapIndex] ) {
					stage->bundle[0].image[0] = tr.whiteImage;
				} else {
					stage->bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
				}
				continue;
			}
			else
			{
				imgFlags_t flags = IMGFLAG_NONE;

				if (!shader.noMipMaps)
					flags |= IMGFLAG_MIPMAP;

				if (!shader.noPicMip)
					flags |= IMGFLAG_PICMIP;

				if (shader.noLightScale)
					flags |= IMGFLAG_NOLIGHTSCALE;

				stage->bundle[0].image[0] = R_FindImageFile( token, flags );

				if ( !stage->bundle[0].image[0] )
				{
					ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
					return qfalse;
				}
			}
		}
		//
		// clampmap <name>
		//
		else if ( !Q_stricmp( token, "clampmap" ) || ( !Q_stricmp( token, "screenMap" ) && s_extendedShader ) )
		{
			imgFlags_t flags;

			if ( !Q_stricmp( token, "screenMap" ) ) {
				flags = IMGFLAG_NONE;
				if ( fboEnabled ) {
					stage->bundle[0].isScreenMap = qtrue;
					shader.hasScreenMap = qtrue;
					tr.needScreenMap = qtrue;
				}
			} else {
				flags = IMGFLAG_CLAMPTOEDGE;
			}

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for '%s' keyword in shader '%s'\n",
					stage->bundle[0].isScreenMap ? "screenMap" : "clampMap", shader.name );
				return qfalse;
			}

			if (!shader.noMipMaps)
				flags |= IMGFLAG_MIPMAP;

			if (!shader.noPicMip)
				flags |= IMGFLAG_PICMIP;

			if (shader.noLightScale)
				flags |= IMGFLAG_NOLIGHTSCALE;

			stage->bundle[0].image[0] = R_FindImageFile( token, flags );
			if ( !stage->bundle[0].image[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
				return qfalse;
			}
		}
		//
		// lightmap <name>
		//
		else if ( !Q_stricmp( token, "lightmap" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'lightmap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

//----(SA)	fixes startup error and allows polygon shadows to work again
			if ( !Q_stricmp( token, "$whiteimage" ) || !Q_stricmp( token, "*white" ) ) {
//----(SA)	end
				stage->bundle[0].image[0] = tr.whiteImage;
				continue;
			}
//----(SA) added
			else if ( !Q_stricmp( token, "$dlight" ) ) {
				stage->bundle[0].image[0] = tr.dlightImage;
				continue;
			}
//----(SA) end
			else if ( !Q_stricmp( token, "$lightmap" ) ) {
				stage->bundle[0].isLightmap = qtrue;
				if ( shader.lightmapIndex < 0 || !tr.lightmaps[shader.lightmapIndex] ) {
					stage->bundle[0].image[0] = tr.whiteImage;
				} else {
					stage->bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
				}
				continue;
			} else {
				stage->bundle[0].image[0] = R_FindImageFile( token,
						IMGFLAG_LIGHTMAP | IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE );
				if ( !stage->bundle[0].image[0] ) {
					ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
					return qfalse;
				}
				stage->bundle[0].isLightmap = qtrue;
			}
		}
		//
		// animMap <frequency> <image1> .... <imageN>
		//
		else if ( !Q_stricmp( token, "animMap" ) )
		{
			int	totalImages = 0;
			int maxAnimations = s_extendedShader ? MAX_IMAGE_ANIMATIONS : MAX_IMAGE_ANIMATIONS_VET;

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'animMap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}
			stage->bundle[0].imageAnimationSpeed = Q_atof( token );

			// parse up to MAX_IMAGE_ANIMATIONS animations
			while ( 1 ) {
				int num;

				token = COM_ParseExt( text, qfalse );
				if ( !token[0] ) {
					break;
				}
				num = stage->bundle[0].numImageAnimations;
				if ( num < maxAnimations ) {
					imgFlags_t flags = IMGFLAG_NONE;

					if (!shader.noMipMaps)
						flags |= IMGFLAG_MIPMAP;

					if (!shader.noPicMip)
						flags |= IMGFLAG_PICMIP;

					if (shader.noLightScale)
						flags |= IMGFLAG_NOLIGHTSCALE;

					stage->bundle[0].image[num] = R_FindImageFile( token, flags );
					if ( !stage->bundle[0].image[num] )
					{
						ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
						return qfalse;
					}
					stage->bundle[0].numImageAnimations++;
				}
				totalImages++;
			}

			if ( totalImages > maxAnimations ) {
				ri.Printf( PRINT_WARNING, "WARNING: ignoring excess images for 'animMap' (found %d, max is %d) in shader '%s'\n",
					totalImages, maxAnimations, shader.name );
			}
		}
		else if ( !Q_stricmp( token, "videoMap" ) )
		{
			int handle;
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'videoMap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}
			handle = ri.CIN_PlayCinematic( token, 0, 0, 256, 256, (CIN_loop | CIN_silent | CIN_shader) );
			if ( handle != -1 ) {
				if ( !tr.scratchImage[ handle ] ) {
					tr.scratchImage[ handle ] = R_CreateImage( va( "*scratch%i", handle ), NULL, NULL, 256, 256, IMGFLAG_CLAMPTOEDGE | IMGFLAG_RGB | IMGFLAG_NOSCALE );
				}
				stage->bundle[0].isVideoMap = qtrue;
				stage->bundle[0].videoMapHandle = handle;
				stage->bundle[0].image[0] = tr.scratchImage[ handle ];
			} else {
				ri.Printf( PRINT_WARNING, "WARNING: could not load '%s' for 'videoMap' keyword in shader '%s'\n", token, shader.name );
			}
		}
		//
		// alphafunc <func>
		//
		else if ( !Q_stricmp( token, "alphaFunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			atestBits = NameToAFunc( token );
		}
		//
		// depthFunc <func>
		//
		else if ( !Q_stricmp( token, "depthfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );

			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			if ( !Q_stricmp( token, "lequal" ) )
			{
				depthFuncBits = 0;
			}
			else if ( !Q_stricmp( token, "equal" ) )
			{
				depthFuncBits = GLS_DEPTHFUNC_EQUAL;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// detail
		//
		else if ( !Q_stricmp( token, "detail" ) ) {
			stage->isDetail = qtrue;
		}
		//
		// fog
		//
		else if ( !Q_stricmp( token, "fog" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for fog in shader '%s'\n", shader.name );
				continue;
			}
			if ( !Q_stricmp( token, "on" ) ) {
				stage->isFogged = qtrue;
			} else {
				stage->isFogged = qfalse;
			}
		}
		//
		// blendfunc <srcFactor> <dstFactor>
		// or blendfunc <add|filter|blend>
		//
		else if ( !Q_stricmp( token, "blendfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
				continue;
			}
			// check for "simple" blends first
			if ( !Q_stricmp( token, "add" ) ) {
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			} else if ( !Q_stricmp( token, "filter" ) ) {
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			} else if ( !Q_stricmp( token, "blend" ) ) {
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			} else {
				// complex double blends
				blendSrcBits = NameToSrcBlendMode( token );

				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
				{
					ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
					continue;
				}
				blendDstBits = NameToDstBlendMode( token );
			}

			// clear depth mask for blended surfaces
			if ( !depthMaskExplicit )
			{
				depthMaskBits = 0;
			}
		}
		//
		// rgbGen
		//
		else if ( !Q_stricmp( token, "rgbGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for rgbGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->rgbWave );
				stage->rgbGen = CGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				vec3_t	color;

				VectorClear( color );

				ParseVector( text, 3, color );
				stage->constantColor[0] = 255 * color[0];
				stage->constantColor[1] = 255 * color[1];
				stage->constantColor[2] = 255 * color[2];

				stage->rgbGen = CGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->rgbGen = CGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "identityLighting" ) )
			{
				stage->rgbGen = CGEN_IDENTITY_LIGHTING;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->rgbGen = CGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->rgbGen = CGEN_VERTEX;
				if ( stage->alphaGen == 0 ) {
					stage->alphaGen = AGEN_VERTEX;
				}
			}
			else if ( !Q_stricmp( token, "exactVertex" ) )
			{
				stage->rgbGen = CGEN_EXACT_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingDiffuse" ) )
			{
				stage->rgbGen = CGEN_LIGHTING_DIFFUSE;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// alphaGen
		//
		else if ( !Q_stricmp( token, "alphaGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for alphaGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->alphaWave );
				stage->alphaGen = AGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				token = COM_ParseExt( text, qfalse );
				stage->constantColor[3] = 255 * Q_atof( token );
				stage->alphaGen = AGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->alphaGen = AGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->alphaGen = AGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
			}
			// Ridah
			else if ( !Q_stricmp( token, "normalzfade" ) )
			{
				stage->alphaGen = AGEN_NORMALZFADE;
				token = COM_ParseExt( text, qfalse );
				if ( token[0] )
				{
					stage->constantColor[3] = 255 * Q_atof( token );
				}
				else
				{
					stage->constantColor[3] = 255;
				}

				token = COM_ParseExt( text, qfalse );
				if ( token[0] )
				{
					stage->zFadeBounds[0] = Q_atof( token );    // lower range
					token = COM_ParseExt( text, qfalse );
					stage->zFadeBounds[1] = Q_atof( token );    // upper range
				}
				else
				{
					stage->zFadeBounds[0] = -1.0;   // lower range
					stage->zFadeBounds[1] =  1.0;   // upper range
				}

			}
			// done.
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->alphaGen = AGEN_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingSpecular" ) )
			{
				stage->alphaGen = AGEN_LIGHTING_SPECULAR;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
			}
			else if ( !Q_stricmp( token, "portal" ) )
			{
				stage->alphaGen = AGEN_PORTAL;
				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
				{
					shader.portalRange = 256;
					ri.Printf( PRINT_WARNING, "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", shader.name );
				}
				else
				{
					shader.portalRange = Q_atof( token );

					if ( shader.portalRange < 0.001f )
						shader.portalRangeR = 0.0f;
					else
						shader.portalRangeR = 1.0f / shader.portalRange;
				}
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// tcGen <function>
		//
		else if ( !Q_stricmp(token, "texgen") || !Q_stricmp( token, "tcGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "environment" ) )
			{
				const char *t = *text;
				stage->bundle[0].tcGen = TCGEN_ENVIRONMENT_MAPPED;
				token = COM_ParseExt( text, qfalse );
				if ( Q_stricmp( token, "firstPerson" ) == 0 )
				{
					stage->bundle[0].tcGen = TCGEN_ENVIRONMENT_MAPPED_FP;
				}
				else
				{
					*text = t; // rewind
				}
			}
			else if ( !Q_stricmp( token, "firerisenv" ) )
			{
				stage->bundle[0].tcGen = TCGEN_FIRERISEENV_MAPPED;
			}
			else if ( !Q_stricmp( token, "lightmap" ) )
			{
				stage->bundle[0].tcGen = TCGEN_LIGHTMAP;
			}
			else if ( !Q_stricmp( token, "texture" ) || !Q_stricmp( token, "base" ) )
			{
				stage->bundle[0].tcGen = TCGEN_TEXTURE;
			}
			else if ( !Q_stricmp( token, "vector" ) )
			{
				ParseVector( text, 3, stage->bundle[0].tcGenVectors[0] );
				ParseVector( text, 3, stage->bundle[0].tcGenVectors[1] );

				stage->bundle[0].tcGen = TCGEN_VECTOR;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown texgen parm in shader '%s'\n", shader.name );
			}
		}
		//
		// tcMod <type> <...>
		//
		else if ( !Q_stricmp( token, "tcMod" ) )
		{
			char buffer[1024] = "";

			while ( 1 )
			{
				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
					break;
				Q_strcat( buffer, sizeof (buffer), token );
				Q_strcat( buffer, sizeof (buffer), " " );
			}

			ParseTexMod( buffer, stage );

			continue;
		}
		//
		// depthmask
		//
		else if ( !Q_stricmp( token, "depthwrite" ) )
		{
			depthMaskBits = GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = qtrue;

			continue;
		}
		else if ( !Q_stricmp( token, "depthFragment" ) && s_extendedShader )
		{
			stage->depthFragment = qtrue;
		}
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	//
	// if cgen isn't explicitly specified, use either identity or identitylighting
	//
	if ( stage->rgbGen == CGEN_BAD ) {
		if ( blendSrcBits == 0 ||
			 blendSrcBits == GLS_SRCBLEND_ONE ||
			 blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) {
			stage->rgbGen = CGEN_IDENTITY_LIGHTING;
		} else {
			stage->rgbGen = CGEN_IDENTITY;
		}
	}


	// ydnar: if shader stage references a lightmap, but no lightmap is present
	// (vertex-approximated surfaces), then set cgen to vertex
	if ( stage->bundle[ 0 ].isLightmap && shader.lightmapIndex < 0 &&
		 stage->bundle[ 0 ].image[ 0 ] == tr.whiteImage ) {
		stage->rgbGen = CGEN_EXACT_VERTEX;
	}


	//
	// implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
	//
	if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) &&
		 ( blendDstBits == GLS_DSTBLEND_ZERO ) )
	{
		blendDstBits = blendSrcBits = 0;
		depthMaskBits = GLS_DEPTHMASK_TRUE;
	}

	// decide which agens we can skip
	if ( stage->alphaGen == AGEN_IDENTITY ) {
		if ( stage->rgbGen == CGEN_IDENTITY
			|| stage->rgbGen == CGEN_LIGHTING_DIFFUSE ) {
			stage->alphaGen = AGEN_SKIP;
		}
	}

#if 0
	if ( depthMaskExplicit && shader.sort == SS_BAD ) {
		if ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA && stage->rgbGen == CGEN_LIGHTING_DIFFUSE && stage->alphaGen == AGEN_NORMALZFADE ) {
			//shader.sort = 16.0f;
		} else if ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA && stage->bundle[0].image[0] != tr.whiteImage && stage->rgbGen == CGEN_IDENTITY && stage->alphaGen == AGEN_VERTEX && shader.noPicMip && shader.noMipMaps ) {
		// fix decals on q3wcp18 and other maps
		} else if ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA && stage->bundle[0].image[0] != tr.whiteImage /*&& stage->rgbGen == CGEN_VERTEX*/ ) {
			depthMaskBits &= ~GLS_DEPTHMASK_TRUE;
			shader.sort = shader.polygonOffset ? SS_DECAL : SS_OPAQUE + 0.01f;
		} else if ( blendSrcBits == GLS_SRCBLEND_ZERO && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR && stage->rgbGen == CGEN_EXACT_VERTEX ) {
			depthMaskBits &= ~GLS_DEPTHMASK_TRUE;
			shader.sort = SS_SEE_THROUGH;
		}
	}
#endif

	//
	// compute state bits
	//
	stage->stateBits = depthMaskBits |
		blendSrcBits | blendDstBits |
		atestBits |
		depthFuncBits;

	return qtrue;
}


/*
===============
ParseDeform

deformVertexes wave <spread> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes normal <frequency> <amplitude>
deformVertexes move <vector> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes bulge <bulgeWidth> <bulgeHeight> <bulgeSpeed>
deformVertexes projectionShadow
deformVertexes autoSprite
deformVertexes autoSprite2
deformVertexes text[0-7]
===============
*/
static void ParseDeform( const char **text ) {
	const char	*token;
	deformStage_t	*ds;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", shader.name );
		return;
	}

	if ( shader.numDeforms == MAX_SHADER_DEFORMS ) {
		ri.Printf( PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader.name );
		return;
	}

	ds = &shader.deforms[ shader.numDeforms ];
	shader.numDeforms++;

	if ( !Q_stricmp( token, "projectionShadow" ) ) {
		ds->deformation = DEFORM_PROJECTION_SHADOW;
		return;
	}

	if ( !Q_stricmp( token, "autosprite" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE;
		return;
	}

	if ( !Q_stricmp( token, "autosprite2" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE2;
		return;
	}

	if ( !Q_stricmpn( token, "text", 4 ) ) {
		int		n;

		n = token[4] - '0';
		if ( n < 0 || n > 7 ) {
			n = 0;
		}
		ds->deformation = DEFORM_TEXT0 + n;
		return;
	}

	if ( !Q_stricmp( token, "bulge" ) )	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeWidth = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeHeight = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeSpeed = Q_atof( token );

		ds->deformation = DEFORM_BULGE;
		return;
	}

	if ( !Q_stricmp( token, "wave" ) )
	{
		float f;
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}

		f = Q_atof( token );
		if ( f != 0.0f )
		{
			ds->deformationSpread = 1.0f / f;
		}
		else
		{
			ds->deformationSpread = 100.0f;
			ri.Printf( PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_WAVE;
		return;
	}

	if ( !Q_stricmp( token, "normal" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.amplitude = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.frequency = Q_atof( token );

		ds->deformation = DEFORM_NORMALS;
		return;
	}

	if ( !Q_stricmp( token, "move" ) ) {
		int		i;

		for ( i = 0 ; i < 3 ; i++ ) {
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
				return;
			}
			ds->moveVector[i] = Q_atof( token );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_MOVE;
		return;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader.name );
}


/*
===============
ParseSkyParms

skyParms <outerbox> <cloudheight> <innerbox>
===============
*/
static void ParseSkyParms( const char **text ) {
	const char	*token;
	static const char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
	char		pathname[MAX_QPATH];
	int			i;
	imgFlags_t imgFlags = IMGFLAG_MIPMAP | IMGFLAG_PICMIP;

	if ( r_neatsky->integer ) {
		imgFlags = IMGFLAG_NONE;
	}

	// outerbox
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i=0 ; i<6 ; i++) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s.tga", token, suf[i] );
			shader.sky.outerbox[i] = R_FindImageFile( pathname, imgFlags | IMGFLAG_CLAMPTOEDGE );

			if ( !shader.sky.outerbox[i] ) {
				shader.sky.outerbox[i] = tr.defaultImage;
			}
		}
	}

	// cloudheight
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	shader.sky.cloudHeight = Q_atof( token );
	if ( shader.sky.cloudHeight == 0.0 ) {
		shader.sky.cloudHeight = 512.0;
	}
	R_InitSkyTexCoords( shader.sky.cloudHeight );

	// innerbox
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i=0 ; i<6 ; i++) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s.tga", token, suf[i] );
			shader.sky.innerbox[i] = R_FindImageFile( pathname, imgFlags );
			if ( !shader.sky.innerbox[i] ) {
				shader.sky.innerbox[i] = tr.defaultImage;
			}
		}
	}

	shader.isSky = qtrue;
}


/*
=================
ParseSort
=================
*/
static void ParseSort( const char **text ) {
	const char	*token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", shader.name );
		return;
	}

	if ( !Q_stricmp( token, "portal" ) ) {
		shader.sort = SS_PORTAL;
	} else if ( !Q_stricmp( token, "sky" ) ) {
		shader.sort = SS_ENVIRONMENT;
	} else if ( !Q_stricmp( token, "opaque" ) ) {
		shader.sort = SS_OPAQUE;
	} else if ( !Q_stricmp( token, "decal" ) ) {
		shader.sort = SS_DECAL;
	} else if ( !Q_stricmp( token, "seeThrough" ) ) {
		shader.sort = SS_SEE_THROUGH;
	} else if ( !Q_stricmp( token, "banner" ) ) {
		shader.sort = SS_BANNER;
	} else if ( !Q_stricmp( token, "additive" ) ) {
		shader.sort = SS_BLEND1;
	} else if ( !Q_stricmp( token, "nearest" ) ) {
		shader.sort = SS_NEAREST;
	} else if ( !Q_stricmp( token, "underwater" ) ) {
		shader.sort = SS_UNDERWATER;
	} else {
		shader.sort = Q_atof( token );
	}
}


// this table is also present in q3map

typedef struct {
	const char *name;
	int clearSolid, surfaceFlags, contents;
} infoParm_t;

static const infoParm_t infoParms[] = {
	// server relevant contents

//----(SA)	modified
	{"clipmissile",  1,  0, CONTENTS_MISSILECLIP},       // impact only specific weapons (rl, gl)
//----(SA)	end

	{"water",        1,  0,  CONTENTS_WATER },
	{"slime",    1,  0,  CONTENTS_SLIME },			// ENSI NOTE: Adding slime for compat reasons
	{"slag",     1,  0,  CONTENTS_SLIME },       // uses the CONTENTS_SLIME flag, but the shader reference is changed to 'slag'
	// to idendify that this doesn't work the same as 'slime' did.
	// (slime hurts instantly, slag doesn't)
//	{"slime",		1,	0,	CONTENTS_SLIME },		// mildly damaging
	{"lava",     1,  0,  CONTENTS_LAVA },        // very damaging
	{"playerclip",   1,  0,  CONTENTS_PLAYERCLIP },
	{"monsterclip",  1,  0,  CONTENTS_MONSTERCLIP },
	{"nodrop",       1,  0,  CONTENTS_NODROP },      // don't drop items or leave bodies (death fog, lava, etc)
	{"nonsolid", 1,  SURF_NONSOLID,  0},                     // clears the solid flag

	// utility relevant attributes
	{"origin",       1,  0,  CONTENTS_ORIGIN },      // center of rotating brushes
	{"trans",        0,  0,  CONTENTS_TRANSLUCENT }, // don't eat contained surfaces
	{"detail",       0,  0,  CONTENTS_DETAIL },      // don't include in structural bsp
	{"structural",   0,  0,  CONTENTS_STRUCTURAL },  // force into structural bsp even if trnas
	{"areaportal",   1,  0,  CONTENTS_AREAPORTAL },  // divides areas
	{"clusterportal", 1,0,  CONTENTS_CLUSTERPORTAL },    // for bots
	{"donotenter",  1,  0,  CONTENTS_DONOTENTER },       // for bots

	// Rafael - nopass
	{"donotenterlarge", 1, 0,    CONTENTS_DONOTENTER_LARGE }, // for larger bots

	{"fog",          1,  0,  CONTENTS_FOG},          // carves surfaces entering
	{"sky",          0,  SURF_SKY,       0 },        // emit light from an environment map
	{"lightfilter",  0,  SURF_LIGHTFILTER, 0 },      // filter light going through it
	{"alphashadow",  0,  SURF_ALPHASHADOW, 0 },      // test light on a per-pixel basis
	{"hint",     0,  SURF_HINT,      0 },        // use as a primary splitter

	// server attributes
	{"slick",            0,  SURF_SLICK,     0 },
	{"noimpact",     0,  SURF_NOIMPACT,  0 },        // don't make impact explosions or marks
	{"nomarks",          0,  SURF_NOMARKS,   0 },        // don't make impact marks, but still explode
	{"ladder",           0,  SURF_LADDER,    0 },
	{"nodamage",     0,  SURF_NODAMAGE,  0 },

	{"monsterslick", 0,  SURF_MONSTERSLICK,  0},     // surf only slick for monsters

//	{"flesh",		0,	SURF_FLESH,		0 },
	{"glass",        0,  SURF_GLASS,     0 },    //----(SA)	added
	{"splash",       0,  SURF_SPLASH,    0 },    //----(SA)	added

	// steps
	{"metal",        0,  SURF_METAL,     0 },
	{"metalsteps",   0,  SURF_METAL,     0 },    // retain bw compatibility with Q3A metal shaders... (SA)
	{"nosteps",      0,  SURF_NOSTEPS,   0 },
	{"woodsteps",    0,  SURF_WOOD,      0 },
	{"grasssteps",   0,  SURF_GRASS,     0 },
	{"gravelsteps",  0,  SURF_GRAVEL,    0 },
	{"carpetsteps",  0,  SURF_CARPET,    0 },
	{"snowsteps",    0,  SURF_SNOW,      0 },
	{"roofsteps",    0,  SURF_ROOF,      0 },    // tile roof

	{"rubble", 0, SURF_RUBBLE, 0 },

	// drawsurf attributes
	{"nodraw",       0,  SURF_NODRAW,    0 },    // don't generate a drawsurface (or a lightmap)
	{"pointlight",   0,  SURF_POINTLIGHT, 0 },   // sample lighting at vertexes
	{"nolightmap",   0,  SURF_NOLIGHTMAP,0 },        // don't generate a lightmap
	{"nodlight", 0,  SURF_NODLIGHT, 0 },     // don't ever add dynamic lights

	{"monsterslicknorth",    0, SURF_MONSLICK_N,0},
	{"monsterslickeast", 0, SURF_MONSLICK_E,0},
	{"monsterslicksouth",    0, SURF_MONSLICK_S,0},
	{"monsterslickwest", 0, SURF_MONSLICK_W,0}
};


/*
===============
ParseSurfaceParm

surfaceparm <name>
===============
*/
static void ParseSurfaceParm( const char **text ) {
	const char	*token;
	int			numInfoParms = (int)ARRAY_LEN( infoParms );
	int			i;

	token = COM_ParseExt( text, qfalse );
	for ( i = 0 ; i < numInfoParms ; i++ ) {
		if ( !Q_stricmp( token, infoParms[i].name ) ) {
			shader.surfaceFlags |= infoParms[i].surfaceFlags;
			shader.contentFlags |= infoParms[i].contents;
#if 0
			if ( infoParms[i].clearSolid ) {
				si->contents &= ~CONTENTS_SOLID;
			}
#endif
			break;
		}
	}
}


typedef enum {
	res_invalid = -1,
	res_false = 0,
	res_true = 1
} resultType;

typedef enum {
	brIF,
	brELIF,
	brELSE
} branchType;

typedef enum {
	maskOR,
	maskAND
} resultMask;


static void derefVariable( const char *name, char *buf, int size )
{
	if ( !Q_stricmp( name, "vid_width" ) ) {
		Com_sprintf( buf, size, "%i", glConfig.vidWidth );
		return;
	}
	if ( !Q_stricmp( name, "vid_height" ) ) {
		Com_sprintf( buf, size, "%i", glConfig.vidHeight );
		return;
	}
	ri.Cvar_VariableStringBuffer( name, buf, size );
}


/*
===============
ParseCondition

if ( $cvar|<integer value> [<condition> $cvar|<integer value> [ [ || .. ] && .. ] ] )
{ shader stage }
[ else
{ shader stage } ]
===============
*/
static qboolean ParseCondition( const char **text, resultType *res )
{
	char lval_str[ MAX_CVAR_VALUE_STRING ];
	char rval_str[ MAX_CVAR_VALUE_STRING ];
	tokenType_t lval_type;
	tokenType_t rval_type;
	const char *token;
	tokenType_t op;
	resultMask	rm;
	qboolean	str;
	int r, r0;

	r = 0;			// resulting value
	rm = maskOR;	// default mask

	for ( ;; )
	{
		rval_str[0] = '\0';
		rval_type = TK_GENEGIC;

		// expect l-value at least
		token = COM_ParseComplex( text, qfalse );
		if ( token[0] == '\0' ) {
			ri.Printf( PRINT_WARNING, "WARNING: expecting lvalue for condition in shader %s\n", shader.name );
			return qfalse;
		}

		Q_strncpyz( lval_str, token, sizeof( lval_str ) );
		lval_type = com_tokentype;

		// get operator
		token = COM_ParseComplex( text, qfalse );
		if ( com_tokentype >= TK_EQ && com_tokentype <= TK_LTE )
		{
			op = com_tokentype;

			// expect r-value
			token = COM_ParseComplex( text, qfalse );
			if ( token[0] == '\0' ) {
				ri.Printf( PRINT_WARNING, "WARNING: expecting rvalue for condition in shader %s\n", shader.name );
				return qfalse;
			}

			Q_strncpyz( rval_str, token, sizeof( rval_str ) );
			rval_type = com_tokentype;

			// read next token, expect '||', '&&' or ')', allow newlines
			/*token =*/ COM_ParseComplex( text, qtrue );
		}
		else if ( com_tokentype == TK_SCOPE_CLOSE || com_tokentype == TK_OR || com_tokentype == TK_AND )
		{
			// no r-value, assume 'not zero' comparison
			op = TK_NEQ;
		}
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unexpected operator '%s' for comparison in shader %s\n", token, shader.name );
			return qfalse;
		}

		str = qfalse;

		if ( lval_type == TK_QUOTED ) {
			str = qtrue;
		} else {
			// dereference l-value
			if ( lval_str[0] == '$' ) {
				derefVariable( lval_str + 1, lval_str, sizeof( lval_str ) );
			}
		}

		if ( rval_type == TK_QUOTED ) {
			str = qtrue;
		} else {
			// dereference r-value
			if ( rval_str[0] == '$' ) {
				derefVariable( rval_str + 1, rval_str, sizeof( rval_str ) );
			}
		}

		// evaluate expression
		if ( str ) {
			// string comparison
			switch ( op ) {
				case TK_EQ:  r0 = strcmp( lval_str, rval_str ) == 0; break;
				case TK_NEQ: r0 = strcmp( lval_str, rval_str ) != 0; break;
				case TK_GT:  r0 = strcmp( lval_str, rval_str ) >  0; break;
				case TK_GTE: r0 = strcmp( lval_str, rval_str ) >= 0; break;
				case TK_LT:  r0 = strcmp( lval_str, rval_str ) <  0; break;
				case TK_LTE: r0 = strcmp( lval_str, rval_str ) <= 0; break;
				default:     r0 = 0; break;
			}
		} else {
			// integer comparison
			int lval = atoi( lval_str );
			int rval = atoi( rval_str );
			switch ( op ) {
				case TK_EQ:  r0 = ( lval == rval ); break;
				case TK_NEQ: r0 = ( lval != rval ); break;
				case TK_GT:  r0 = ( lval >  rval ); break;
				case TK_GTE: r0 = ( lval >= rval ); break;
				case TK_LT:  r0 = ( lval <  rval ); break;
				case TK_LTE: r0 = ( lval <= rval ); break;
				default:     r0 = 0; break;
			}
		}

		if ( rm == maskOR )
			r |= r0;
		else
			r &= r0;

		if ( com_tokentype == TK_OR ) {
			rm = maskOR;
			continue;
		}

		if ( com_tokentype == TK_AND ) {
			rm = maskAND;
			continue;
		}

		if ( com_tokentype != TK_SCOPE_CLOSE ) {
			ri.Printf( PRINT_WARNING, "WARNING: expecting ')' in shader %s\n", shader.name );
			return qfalse;
		}

		break;
	}

	if ( res )
		*res = r ? res_true : res_false;

	return qtrue;
}


/*
=================
ParseShader

The current text pointer is at the explicit text definition of the
shader.  Parse it into the global shader variable.  Later functions
will optimize it.
=================
*/
static qboolean ParseShader( const char **text )
{
	resultType res;
	branchType branch;
	const char *token;
	int s;

	s = 0;
	tr.allowCompress = compress_allowed;   // allow compression by default

	s_extendedShader = (*text >= s_extensionOffset);

	token = COM_ParseExt( text, qtrue );
	if ( token[0] != '{' )
	{
		ri.Printf( PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, shader.name );
		return qfalse;
	}

	res = res_invalid;

	while ( 1 )
	{
		//token = COM_ParseExt( text, qtrue );
		token = COM_ParseComplex( text, qtrue );
		if ( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", shader.name );
			return qfalse;
		}
		// end of shader definition
		if ( token[0] == '}' )
		{
			tr.allowCompress = compress_allowed;   // allow compression by default
			break;
		}
		// stage definition
		else if ( token[0] == '{' )
		{
			if ( s >= MAX_SHADER_STAGES ) {
				ri.Printf( PRINT_WARNING, "WARNING: too many stages in shader %s (max is %i)\n", shader.name, MAX_SHADER_STAGES );
				return qfalse;
			}

			if ( !ParseStage( &stages[s], text ) )
			{
				return qfalse;
			}
			stages[s].active = qtrue;
			s++;

			continue;
		}
		// skip stuff that only the QuakeEdRadient needs
		else if ( !Q_stricmpn( token, "qer", 3 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		// sun parms
		else if ( !Q_stricmp( token, "q3map_sun" ) || !Q_stricmp( token, "q3map_sunExt" ) ) {
			float	a, b;

			token = COM_ParseExt( text, qfalse );
			tr.sunLight[0] = Q_atof( token );
			token = COM_ParseExt( text, qfalse );
			tr.sunLight[1] = Q_atof( token );
			token = COM_ParseExt( text, qfalse );
			tr.sunLight[2] = Q_atof( token );

			VectorNormalize( tr.sunLight );

			token = COM_ParseExt( text, qfalse );
			a = Q_atof( token );
			VectorScale( tr.sunLight, a, tr.sunLight );

			token = COM_ParseExt( text, qfalse );
			a = Q_atof( token );
			a = a / 180 * M_PI;

			token = COM_ParseExt( text, qfalse );
			b = Q_atof( token );
			b = b / 180 * M_PI;

			tr.sunDirection[0] = cos( a ) * cos( b );
			tr.sunDirection[1] = sin( a ) * cos( b );
			tr.sunDirection[2] = sin( b );

			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "deformVertexes" ) ) {
			ParseDeform( text );
			continue;
		}
		else if ( !Q_stricmp( token, "tesssize" ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "clampTime" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( token[0] ) {
				shader.clampTime = Q_atof( token );
			}
		}
		// skip stuff that only the q3map needs
		else if ( !Q_stricmpn( token, "q3map", 5 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		// skip stuff that only q3map or the server needs
		else if ( !Q_stricmp( token, "surfaceParm" ) ) {
			ParseSurfaceParm( text );
			continue;
		}
		// no mip maps
		else if ( ( !Q_stricmp( token, "nomipmaps" ) ) || ( !Q_stricmp( token,"nomipmap" ) ) )
		{
			shader.noMipMaps = qtrue;
			shader.noPicMip = qtrue;
			continue;
		}
		// no picmip adjustment
		else if ( !Q_stricmp( token, "nopicmip" ) )
		{
			shader.noPicMip = qtrue;
			continue;
		}
		// polygonOffset
		else if ( !Q_stricmp( token, "polygonOffset" ) )
		{
			shader.polygonOffset = qtrue;
			continue;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if ( !Q_stricmp( token, "entityMergable" ) )
		{
			shader.entityMergable = qtrue;
			continue;
		}
		// fogParms
		else if ( !Q_stricmp( token, "fogParms" ) ) 
		{
			vec3_t fogColor;

			if ( !ParseVector( text, 3, shader.fogParms.color ) ) {
				return qfalse;
			}

			VectorCopy( shader.fogParms.color, fogColor );

			if ( r_mapGreyScale->value > 0 ) {
				float luminance;
				luminance = LUMA( fogColor[0], fogColor[1], fogColor[2] );
				fogColor[0] = LERP( fogColor[0], luminance, r_mapGreyScale->value );
				fogColor[1] = LERP( fogColor[1], luminance, r_mapGreyScale->value );
				fogColor[2] = LERP( fogColor[2], luminance, r_mapGreyScale->value );
			}

			shader.fogParms.colorInt = ColorBytes4( fogColor[0] * tr.identityLight,
													fogColor[1] * tr.identityLight,
													fogColor[2] * tr.identityLight, 1.0 );

			shader.fogParms.color[3] = 1.0;

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_WARNING, "WARNING: 'fogParms' incomplete - missing opacity value in shader '%s' (forcing to 1)\n", shader.name ); // tcScale is 1.0f
				shader.fogParms.depthForOpaque = 1;
			}
			else {
				shader.fogParms.depthForOpaque = Q_atof( token );
				shader.fogParms.depthForOpaque = shader.fogParms.depthForOpaque < 1 ? 1 : shader.fogParms.depthForOpaque;
			}
			shader.fogParms.tcScale = 1.0f / shader.fogParms.depthForOpaque;

			// skip any old gradient directions
			SkipRestOfLine( text );
			continue;
		}
		// portal
		else if ( !Q_stricmp( token, "portal" ) ) {
			shader.sort = SS_PORTAL;
			continue;
		}
		// skyparms <cloudheight> <outerbox> <innerbox>
		else if ( !Q_stricmp( token, "skyparms" ) ) {
			ParseSkyParms( text );
			if ( r_neatsky->integer ) {
				shader.noPicMip = qtrue;
				shader.noMipMaps = qtrue;
			}
			continue;
		}
		// This is fixed fog for the skybox/clouds determined solely by the shader
		// it will not change in a level and will not be necessary
		// to force clients to use a sky fog the server says to.
		// skyfogvars <(r,g,b)> <dist>
		else if ( !Q_stricmp( token, "skyfogvars" ) ) {
			vec3_t fogColor;

			if ( !ParseVector( text, 3, fogColor ) ) {
				return qfalse;
			}
			token = COM_ParseExt( text, qfalse );

			if ( !token[0] ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing density value for sky fog\n" );
				continue;
			}

			if ( Q_atof( token ) > 1 ) {
				ri.Printf( PRINT_WARNING, "WARNING: last value for skyfogvars is 'density' which needs to be 0.0-1.0\n" );
				continue;
			}

			R_SetFog( FOG_SKY, 0, 5, fogColor[0], fogColor[1], fogColor[2], Q_atof( token ) );
			continue;
		} else if ( !Q_stricmp( token, "sunshader" ) )        {
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing shader name for 'sunshader'\n" );
				continue;
			}
			tr.sunShaderName = CopyString( token );
		}
//----(SA)	added
		else if ( !Q_stricmp( token, "lightgridmulamb" ) ) { // ambient multiplier for lightgrid
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing value for 'lightgrid ambient multiplier'\n" );
				continue;
			}
			if ( Q_atof( token ) > 0 ) {
				tr.lightGridMulAmbient = Q_atof( token );
			}
		} else if ( !Q_stricmp( token, "lightgridmuldir" ) )        { // directional multiplier for lightgrid
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing value for 'lightgrid directional multiplier'\n" );
				continue;
			}
			if ( Q_atof( token ) > 0 ) {
				tr.lightGridMulDirected = Q_atof( token );
			}
		}
//----(SA)	end
		else if ( !Q_stricmp( token, "waterfogvars" ) ) {
			vec3_t watercolor;
			float fogvar;

			if ( !ParseVector( text, 3, watercolor ) ) {
				return qfalse;
			}
			token = COM_ParseExt( text, qfalse );

			if ( !token[0] ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing density/distance value for water fog\n" );
				continue;
			}

			fogvar = Q_atof( token );

			//----(SA)	right now allow one water color per map.  I'm sure this will need
			//			to change at some point, but I'm not sure how to track fog parameters
			//			on a "per-water volume" basis yet.

			if ( fogvar == 0 ) {       // '0' specifies "use the map values for everything except the fog color
				// TODO
			} else if ( fogvar > 1 )      { // distance "linear" fog
				R_SetFog( FOG_WATER, 0, fogvar, watercolor[0], watercolor[1], watercolor[2], 1.1f );
			} else {                      // density "exp" fog
				R_SetFog( FOG_WATER, 0, 5, watercolor[0], watercolor[1], watercolor[2], fogvar );
			}

			continue;
		}
		// fogvars
		else if ( !Q_stricmp( token, "fogvars" ) ) {
			vec3_t fogColor;
			float fogDensity;
			int fogFar;

			if ( !ParseVector( text, 3, fogColor ) ) {
				return qfalse;
			}

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing density value for the fog\n" );
				continue;
			}


			//----(SA)	NOTE:	fogFar > 1 means the shader is setting the farclip, < 1 means setting
			//					density (so old maps or maps that just need softening fog don't have to care about farclip)

			fogDensity = Q_atof( token );
			if ( fogDensity > 1 ) {  // linear
				fogFar      = fogDensity;
			} else {
				fogFar      = 5;
			}

			R_SetFog( FOG_MAP, 0, fogFar, fogColor[0], fogColor[1], fogColor[2], fogDensity );
			R_SetFog( FOG_CMD_SWITCHFOG, FOG_MAP, 50, 0, 0, 0, 0 );

			continue;
		}
		// done.
		// Ridah, allow disable fog for some shaders
		else if ( !Q_stricmp( token, "nofog" ) ) {
			shader.noFog = qtrue;
			continue;
		}
		// done.
		// RF, allow each shader to permit compression if available
		else if ( !Q_stricmp( token, "allowcompress" ) ) {
			tr.allowCompress = compress_allowed;
			continue;
		} else if ( !Q_stricmp( token, "nocompress" ) )   {
			tr.allowCompress = compress_explicitBlock;
			continue;
		}
		// done.
		// light <value> determines flaring in q3map, not needed here
		else if ( !Q_stricmp( token, "light" ) ) {
			token = COM_ParseExt( text, qfalse );
			continue;
		}
		// cull <face>
		else if ( !Q_stricmp( token, "cull" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twosided" ) || !Q_stricmp( token, "disable" ) ) {
				shader.cullType = CT_TWO_SIDED;
			} else if ( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) )      {
				shader.cullType = CT_BACK_SIDED;
			} else
			{
				ri.Printf( PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token, shader.name );
			}
			continue;
		}
		// ydnar: distancecull <opaque distance> <transparent distance> <alpha threshold>
		else if ( !Q_stricmp( token, "distancecull" ) ) {
			int i;


			for ( i = 0; i < 3; i++ )
			{
				token = COM_ParseExt( text, qfalse );
				if ( token[ 0 ] == 0 ) {
					ri.Printf( PRINT_WARNING, "WARNING: missing distancecull parms in shader '%s'\n", shader.name );
				} else {
					shader.distanceCull[ i ] = Q_atof( token );
				}
			}

			if ( shader.distanceCull[ 1 ] - shader.distanceCull[ 0 ] > 0 ) {
				// distanceCull[ 3 ] is an optimization
				shader.distanceCull[ 3 ] = 1.0 / ( shader.distanceCull[ 1 ] - shader.distanceCull[ 0 ] );
			} else
			{
				shader.distanceCull[ 0 ] = 0;
				shader.distanceCull[ 1 ] = 0;
				shader.distanceCull[ 2 ] = 0;
				shader.distanceCull[ 3 ] = 0;
			}
			continue;
		}
		// sort
		else if ( !Q_stricmp( token, "sort" ) )
		{
			ParseSort( text );
			continue;
		}
		// conditional stage definition
		else if ( ( !Q_stricmp( token, "if" ) || !Q_stricmp( token, "else" ) || !Q_stricmp( token, "elif" ) ) && s_extendedShader )
		{
			if ( Q_stricmp( token, "if" ) == 0 ) {
				branch = brIF;
			} else {
				if ( res == res_invalid  ) {
					// we don't have any previous 'if' statements
					ri.Printf( PRINT_WARNING, "WARNING: unexpected '%s' in '%s'\n", token, shader.name );
					return qfalse;
				}
				if ( Q_stricmp( token, "else" ) == 0 )
					branch = brELSE;
				else
					branch = brELIF;
			}

			if ( branch != brELSE ) { // we can set/update result
				token = COM_ParseComplex( text, qfalse );
				if ( com_tokentype != TK_SCOPE_OPEN ) {
					ri.Printf( PRINT_WARNING, "WARNING: expecting '(' in '%s'\n", shader.name );
					return qfalse;
				}
				if ( !ParseCondition( text, (branch == brIF || res == res_true) ? &res : NULL ) ) {
					ri.Printf( PRINT_WARNING, "WARNING: error parsing condition in '%s'\n", shader.name );
					return qfalse;
				}
			}

			if ( res == res_false )	{
				// skip next stage or keyword until newline
				token = COM_ParseExt( text, qtrue );
				if ( token[0] == '{' )
					SkipBracedSection( text, 1 );
				else
					SkipRestOfLine( text );
			} else {
				// parse next tokens as usual
			}

			if ( branch == brELSE )
				res = res_invalid; // finalize branch
			else
				res ^= 1; // or toggle for possible "elif" / "else" statements

			continue;
		}
		// ydnar: implicit default mapping to eliminate redundant/incorrect explicit shader stages
		else if ( !Q_stricmpn( token, "implicit", 8 ) ) {
			// set implicit mapping state
			if ( !Q_stricmp( token, "implicitBlend" ) ) {
				implicitStateBits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
				implicitCullType = CT_TWO_SIDED;
			} else if ( !Q_stricmp( token, "implicitMask" ) )     {
				implicitStateBits = GLS_DEPTHMASK_TRUE | GLS_ATEST_GE_80;
				implicitCullType = CT_TWO_SIDED;
			} else    // "implicitMap"
			{
				implicitStateBits = GLS_DEPTHMASK_TRUE;
				implicitCullType = CT_FRONT_SIDED;
			}

			// get image
			token = COM_ParseExt( text, qfalse );
			if ( token[ 0 ] != '\0' ) {
				Q_strncpyz( implicitMap, token, sizeof( implicitMap ) );
			} else
			{
				implicitMap[ 0 ] = '-';
				implicitMap[ 1 ] = '\0';
			}

			continue;
		}
		// unknown directive
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown general shader parameter '%s' in '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	//
	// ignore shaders that don't have any stages, unless it is a sky or fog
	// ydnar: or have implicit mapping
	//
	if ( s == 0 && !shader.isSky && !( shader.contentFlags & CONTENTS_FOG ) && implicitMap[ 0 ] == '\0' ) {
		return qfalse;
	}

	shader.explicitlyDefined = qtrue;

	return qtrue;
}

/*
========================================================================================

SHADER OPTIMIZATION AND FOGGING

========================================================================================
*/

/*
===================
ComputeStageIteratorFunc

See if we can use on of the simple fastpath stage functions,
otherwise set to the generic stage function
===================
*/
static void ComputeStageIteratorFunc( void )
{
	//
	// see if this should go into the sky path
	//
	if ( shader.isSky )
	{
		shader.optimalStageIteratorFunc = RB_StageIteratorSky;
	}
	else
	{
		shader.optimalStageIteratorFunc = RB_StageIteratorGeneric;
	}
}


typedef struct {
	GLbitfield blendA;
	GLbitfield blendB;

	GLbitfield multitextureEnv;
	GLbitfield multitextureBlend;
} collapse_t;

static const collapse_t collapse[] = {
	{ 0, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
	  GL_MODULATE, 0 },

	{ 0, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
	  GL_MODULATE, 0 },

	{ GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
	  GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
	  GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
	  GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
	  GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ 0, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
	  GL_ADD, 0 },

	{ GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
	  GL_ADD, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE },
#if 0
	{ 0, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA,
	  GL_DECAL, 0 },
#endif
};

static const int numCollapse = (int)ARRAY_LEN( collapse );

/*
================
CollapseMultitexture

Attempt to combine two stages into a single multitexture stage
FIXME: I think modulated add + modulated add collapses incorrectly
=================
*/
static qboolean CollapseMultitexture( shaderStage_t *st0, shaderStage_t *st1, int num_stages ) {
	GLbitfield abits, bbits;
	int i;
	textureBundle_t tmpBundle;

	if ( !qglActiveTextureARB ) {
		return qfalse;
	}

	// make sure both stages are active
	if ( !st0->active || !st1->active ) {
		return qfalse;
	}

	if ( st0->depthFragment ) {
		return qfalse;
	}

	// on voodoo2, don't combine different tmus
	if ( glConfig.driverType == GLDRV_VOODOO ) {
		if ( st0->bundle[0].image[0]->TMU ==
			 st1->bundle[0].image[0]->TMU ) {
			return qfalse;
		}
	}

	abits = st0->stateBits;
	bbits = st1->stateBits;

	// make sure that both stages have identical state other than blend modes
	if ( ( abits & ~( GLS_BLEND_BITS | GLS_DEPTHMASK_TRUE ) ) !=
		( bbits & ~( GLS_BLEND_BITS | GLS_DEPTHMASK_TRUE ) ) ) {
		return qfalse;
	}

	abits &= GLS_BLEND_BITS;
	bbits &= GLS_BLEND_BITS;

	// search for a valid multitexture blend function
	for ( i = 0; i < numCollapse ; i++ ) {
		if ( abits == collapse[i].blendA
			&& bbits == collapse[i].blendB ) {
			break;
		}
	}

	// nothing found
	if ( i == numCollapse ) {
		return qfalse;
	}

	// GL_ADD is a separate extension
	if ( collapse[i].multitextureEnv == GL_ADD && !glConfig.textureEnvAddAvailable ) {
		return qfalse;
	}

	// make sure waveforms have identical parameters
	if ( ( st0->rgbGen != st1->rgbGen ) || ( st0->alphaGen != st1->alphaGen ) ) {
		return qfalse;
	}

	// an add collapse can only have identity colors
	if ( collapse[i].multitextureEnv == GL_ADD && st0->rgbGen != CGEN_IDENTITY ) {
		return qfalse;
	}

	if ( st0->rgbGen == CGEN_WAVEFORM )
	{
		if ( memcmp( &st0->rgbWave, &st1->rgbWave, sizeof( stages[0].rgbWave ) ) )
		{
			return qfalse;
		}
	}

	if ( st0->alphaGen == AGEN_WAVEFORM )
	{
		if ( memcmp( &st0->alphaWave, &st1->alphaWave, sizeof( stages[0].alphaWave ) ) )
		{
			return qfalse;
		}
	}

	// make sure that lightmaps are in bundle 1
	if ( st0->bundle[0].isLightmap )
	{
		tmpBundle = st0->bundle[0];
		st0->bundle[0] = st1->bundle[0];
		st0->bundle[1] = tmpBundle;
	}
	else
	{
		st0->bundle[1] = st1->bundle[0];
	}

	// set the new blend state bits
	shader.multitextureEnv = qtrue;
	st0->mtEnv = collapse[i].multitextureEnv;
	st0->stateBits &= ~GLS_BLEND_BITS;
	st0->stateBits |= collapse[i].multitextureBlend;
	st0->tessFlags |= TESS_ST1;

	//
	// move down subsequent shaders
	//
	if ( num_stages > 2 )
	{
		memmove( st1, st1+1, sizeof( stages[0] ) * ( num_stages - 2 ) );
	}

	Com_Memset( st0 + num_stages - 1, 0, sizeof( stages[0] ) );

	return qtrue;
}


#ifdef USE_PMLIGHT

static int tcmodWeight( const textureBundle_t *bundle )
{
	if ( bundle->numTexMods == 0 )
		return 1;

	return 0;
}


static const textureBundle_t *lightingBundle( int stageIndex, const textureBundle_t *selected ) {
	const shaderStage_t *stage = &stages[ stageIndex ];
	int i, numTexBundles;

	if ( stage->mtEnv )
		numTexBundles = 2;
	else
		numTexBundles = 1;

	for ( i = 0; i < numTexBundles; i++ ) {
		const textureBundle_t *bundle = &stage->bundle[ i ];
		if ( bundle->isLightmap ) {
			continue;
		}
		if ( bundle->image[0] == tr.whiteImage ) {
			continue;
		}
		if ( bundle->tcGen != TCGEN_TEXTURE ) {
			continue;
		}
		if ( selected ) {
			if ( stage->rgbGen == CGEN_IDENTITY && ( stage->stateBits & GLS_BLEND_BITS ) == ( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO ) ) {
				// fix for q3wcp17' textures/scanctf2/bounce_white and others
				continue;
			}
			if ( tcmodWeight( selected ) > tcmodWeight( bundle ) ) {
				continue;
			}
		}
		shader.lightingStage = stageIndex;
		shader.lightingBundle = i;
		selected = bundle;
	}

	return selected;
}


/*
====================
FindLightingStages

Find proper stage for dlight pass
====================
*/
static void FindLightingStages( void )
{
	const shaderStage_t *st;
	const textureBundle_t *bundle;
	int i;

	shader.lightingStage = -1;
	shader.lightingBundle = 0;

	if ( !qglGenProgramsARB )
		return;

	if ( shader.isSky || ( shader.surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) || shader.sort == SS_ENVIRONMENT || shader.sort >= SS_FOG )
		return;

	bundle = NULL;
	for ( i = 0; i < shader.numUnfoggedPasses; i++ ) {
		st = &stages[ i ];
		if ( !st->active )
			break;
		if ( st->isDetail && shader.lightingStage >= 0 )
			continue;
		if ( ( st->stateBits & GLS_BLEND_BITS ) == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE ) ) {
			if ( bundle && bundle->numTexMods ) {
				// already selected bundle has somewhat non-static tcgen
				// so we may accept this stage
				// this fixes jumppads on lun3dm5
			} else {
				continue;
			}
		}
		bundle = lightingBundle( i, bundle );
	}
}
#endif


/*
=============
FixRenderCommandList
https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=493
Arnout: this is a nasty issue. Shaders can be registered after drawsurfaces are generated
but before the frame is rendered. This will, for the duration of one frame, cause drawsurfaces
to be rendered with bad shaders. To fix this, need to go through all render commands and fix
sortedIndex.
==============
*/
static void FixRenderCommandList( int newShader ) {
	renderCommandList_t	*cmdList = &backEndData->commands;

	if( cmdList ) {
		const void *curCmd = cmdList->cmds;

		while ( 1 ) {
			curCmd = PADP(curCmd, sizeof(void *));

			switch ( *(const int *)curCmd ) {
			case RC_SET_COLOR:
				{
				const setColorCommand_t *sc_cmd = (const setColorCommand_t *)curCmd;
				curCmd = (const void *)(sc_cmd + 1);
				break;
				}
			case RC_STRETCH_PIC:
			case RC_ROTATED_PIC:
			case RC_STRETCH_PIC_GRADIENT:
				{
				const stretchPicCommand_t *sp_cmd = (const stretchPicCommand_t *)curCmd;
				curCmd = (const void *)(sp_cmd + 1);
				break;
				}
			case RC_2DPOLYS:
				{
				const poly2dCommand_t *poly_cmd = (const poly2dCommand_t *)curCmd;
				curCmd = (const void *)(poly_cmd + 1);
				break;
				}
			case RC_DRAW_SURFS:
				{
				int i;
				drawSurf_t	*drawSurf;
				shader_t	*sh;
				int			fogNum;
				int			entityNum;
				int			dlightMap;
				int			sortedIndex;
				const drawSurfsCommand_t *ds_cmd =  (const drawSurfsCommand_t *)curCmd;

				for( i = 0, drawSurf = ds_cmd->drawSurfs; i < ds_cmd->numDrawSurfs; i++, drawSurf++ ) {
					R_DecomposeSort( drawSurf->sort, &entityNum, &sh, &fogNum, &dlightMap );
					sortedIndex = (( drawSurf->sort >> QSORT_SHADERNUM_SHIFT ) & SHADERNUM_MASK);
					if ( sortedIndex >= newShader ) {
						sortedIndex = sh->sortedIndex;
						drawSurf->sort = (sortedIndex << QSORT_SHADERNUM_SHIFT) | (entityNum << QSORT_REFENTITYNUM_SHIFT) | ( fogNum << QSORT_FOGNUM_SHIFT ) | (int)dlightMap;
					}
				}
				curCmd = (const void *)(ds_cmd + 1);
				break;
				}
			case RC_DRAW_BUFFER:
				{
				const drawBufferCommand_t *db_cmd = (const drawBufferCommand_t *)curCmd;
				curCmd = (const void *)(db_cmd + 1);
				break;
				}
			case RC_SWAP_BUFFERS:
				{
				const swapBuffersCommand_t *sb_cmd = (const swapBuffersCommand_t *)curCmd;
				curCmd = (const void *)(sb_cmd + 1);
				break;
				}
			case RC_FINISHBLOOM:
				{
				const finishBloomCommand_t *fb_cmd = (const finishBloomCommand_t *)curCmd;
				curCmd = (const void *)(fb_cmd + 1);
				break;
				}
			case RC_COLORMASK:
				{
				const colorMaskCommand_t *cm_cmd = (const colorMaskCommand_t *)curCmd;
				curCmd = (const void *)(cm_cmd + 1);
				break;
				}
			case RC_CLEARDEPTH:
				{
				const clearDepthCommand_t *cd_cmd = (const clearDepthCommand_t *)curCmd;
				curCmd = (const void *)(cd_cmd + 1);
				break;
				}
			case RC_CLEARCOLOR:
				{
				const clearColorCommand_t *cc_cmd = (const clearColorCommand_t *)curCmd;
				curCmd = (const void *)(cc_cmd + 1);
				break;
				}
			case RC_END_OF_LIST:
			default:
				return;
			}
		}
	}
}


/*
==============
SortNewShader

Positions the most recently created shader in the tr.sortedShaders[]
array so that the shader->sort key is sorted relative to the other
shaders.

Sets shader->sortedIndex
==============
*/
static void SortNewShader( void ) {
	int		i;
	float	sort;
	shader_t	*newShader;

	newShader = tr.shaders[ tr.numShaders - 1 ];
	sort = newShader->sort;

	for ( i = tr.numShaders - 2 ; i >= 0 ; i-- ) {
		if ( tr.sortedShaders[ i ]->sort <= sort ) {
			break;
		}
		tr.sortedShaders[i+1] = tr.sortedShaders[i];
		tr.sortedShaders[i+1]->sortedIndex++;
	}

	// Arnout: fix rendercommandlist
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=493
	FixRenderCommandList( i+1 );

	newShader->sortedIndex = i+1;
	tr.sortedShaders[i+1] = newShader;
}


/*
====================
GeneratePermanentShader
====================
*/
static shader_t *GeneratePermanentShader( void ) {
	shader_t	*newShader;
	int			i, b;
	int			size, hash;

	if ( tr.numShaders >= MAX_SHADERS ) {
		ri.Printf( PRINT_WARNING, "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
		return tr.defaultShader;
	}

	// Ridah, caching system
	newShader = R_CacheShaderAlloc( shader.lightmapIndex < 0 ? va( "%s lm: %i", shader.name, shader.lightmapIndex ) : NULL, sizeof( shader_t ) );

	*newShader = shader;

	tr.shaders[ tr.numShaders ] = newShader;
	newShader->index = tr.numShaders;

	tr.sortedShaders[ tr.numShaders ] = newShader;
	newShader->sortedIndex = tr.numShaders;

	tr.numShaders++;

	for ( i = 0 ; i < newShader->numUnfoggedPasses ; i++ ) {
		if ( !stages[i].active ) {
			newShader->stages[i] = NULL;    // Ridah, make sure it's null
			break;
		}
		// Ridah, caching system
		newShader->stages[i] = R_CacheShaderAlloc( NULL, sizeof( stages[i] ) );

		*newShader->stages[i] = stages[i];

		for ( b = 0 ; b < NUM_TEXTURE_BUNDLES ; b++ ) {
			if ( !newShader->stages[i]->bundle[b].numTexMods ) {
				// make sure unalloc'd texMods aren't pointing to some random point in memory
				newShader->stages[i]->bundle[b].texMods = NULL;
				continue;
			}
			size = newShader->stages[i]->bundle[b].numTexMods * sizeof( texModInfo_t );
			// Ridah, caching system
			if ( size ) {
				newShader->stages[i]->bundle[b].texMods = R_CacheShaderAlloc( NULL, size );
				Com_Memcpy( newShader->stages[i]->bundle[b].texMods, stages[i].bundle[b].texMods, size );
			}
		}
	}

	SortNewShader();

	hash = generateHashValue(newShader->name, FILE_HASH_SIZE);
	newShader->next = hashTable[hash];
	hashTable[hash] = newShader;

	return newShader;
}


/*
=================
VertexLightingCollapse

If vertex lighting is enabled, only render a single
pass, trying to guess which is the correct one to best approximate
what it is supposed to look like.
=================
*/
/*static void VertexLightingCollapse( void ) {
	int		stage;
	shaderStage_t	*bestStage;
	int		bestImageRank;
	int		rank;

	// if we aren't opaque, just use the first pass
	if ( shader.sort == SS_OPAQUE ) {

		// pick the best texture for the single pass
		bestStage = &stages[0];
		bestImageRank = -999999;

		for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
			shaderStage_t *pStage = &stages[stage];

			if ( !pStage->active ) {
				break;
			}
			rank = 0;

			if ( pStage->bundle[0].isLightmap ) {
				rank -= 100;
			}
			if ( pStage->bundle[0].tcGen != TCGEN_TEXTURE ) {
				rank -= 5;
			}
			if ( pStage->bundle[0].numTexMods ) {
				rank -= 5;
			}
			if ( pStage->rgbGen != CGEN_IDENTITY && pStage->rgbGen != CGEN_IDENTITY_LIGHTING ) {
				rank -= 3;
			}

			if ( rank > bestImageRank  ) {
				bestImageRank = rank;
				bestStage = pStage;
			}
		}

		stages[0].bundle[0] = bestStage->bundle[0];
		stages[0].stateBits &= ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
		stages[0].stateBits |= GLS_DEPTHMASK_TRUE;
		if ( shader.lightmapIndex == LIGHTMAP_NONE ) {
			stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		} else {
			stages[0].rgbGen = CGEN_EXACT_VERTEX;
		}
		stages[0].alphaGen = AGEN_SKIP;
	} else {
		// don't use a lightmap (tesla coils)
		if ( stages[0].bundle[0].isLightmap ) {
			stages[0] = stages[1];
		}

		// if we were in a cross-fade cgen, hack it to normal
		if ( stages[0].rgbGen == CGEN_ONE_MINUS_ENTITY || stages[1].rgbGen == CGEN_ONE_MINUS_ENTITY ) {
			stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		if ( ( stages[0].rgbGen == CGEN_WAVEFORM && stages[0].rgbWave.func == GF_SAWTOOTH )
			 && ( stages[1].rgbGen == CGEN_WAVEFORM && stages[1].rgbWave.func == GF_INVERSE_SAWTOOTH ) ) {
			stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		if ( ( stages[0].rgbGen == CGEN_WAVEFORM && stages[0].rgbWave.func == GF_INVERSE_SAWTOOTH )
			 && ( stages[1].rgbGen == CGEN_WAVEFORM && stages[1].rgbWave.func == GF_SAWTOOTH ) ) {
			stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
	}

	for ( stage = 1; stage < MAX_SHADER_STAGES; stage++ ) {
		shaderStage_t *pStage = &stages[stage];

		if ( !pStage->active ) {
			break;
		}

		memset( pStage, 0, sizeof( *pStage ) );
	}
}*/


/*
===============
InitShader
===============
*/
static void InitShader( const char *name, int lightmapIndex ) {
	int i;

	// clear the global shader
	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );

	// we need to know original (unmodified) lightmap index
	// because shader search functions expects this
	// otherwise they will fail and cause massive duplication
	shader.lightmapSearchIndex = lightmapIndex;

	if (lightmapIndex >= MAX_LIGHTMAPS)
		lightmapIndex = LIGHTMAP_BY_VERTEX;

	Q_strncpyz( shader.name, name, sizeof( shader.name ) );
	shader.lightmapIndex = lightmapIndex;

	for ( i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
		stages[i].bundle[0].texMods = texMods[i];
	}
}


static void DetectNeeds( void )
{
	texCoordGen_t t1;
	texCoordGen_t t2;
	int i;

	for ( i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		if ( !stages[i].active )
			break;

		t1 = stages[i].bundle[0].tcGen;
		t2 = stages[i].bundle[1].tcGen;

		if ( t1 == TCGEN_LIGHTMAP || t2 == TCGEN_LIGHTMAP )
		{
			shader.needsST2 = qtrue;
		}
		if ( t1 == TCGEN_ENVIRONMENT_MAPPED || t1 == TCGEN_ENVIRONMENT_MAPPED_FP || t1 == TCGEN_FIRERISEENV_MAPPED )
		{
			shader.needsNormal = qtrue;
		}
		if ( t2 == TCGEN_ENVIRONMENT_MAPPED || t2 == TCGEN_ENVIRONMENT_MAPPED_FP || t2 == TCGEN_FIRERISEENV_MAPPED )
		{
			shader.needsNormal = qtrue;
		}
		if ( stages[i].alphaGen == AGEN_LIGHTING_SPECULAR || stages[i].alphaGen == AGEN_NORMALZFADE || stages[i].rgbGen == CGEN_LIGHTING_DIFFUSE )
		{
			shader.needsNormal = qtrue;
		}
	}
	for ( i = 0; i < shader.numDeforms; i++ )
	{
		if ( shader.deforms[i].deformation == DEFORM_WAVE || shader.deforms[i].deformation == DEFORM_NORMALS || shader.deforms[i].deformation == DEFORM_BULGE ) {
			shader.needsNormal = qtrue;
		}
		if ( shader.deforms[i].deformation >= DEFORM_TEXT0 && shader.deforms[i].deformation <= DEFORM_TEXT7 ) {
			shader.needsNormal = qtrue;
		}
	}
}


/*
=========================
FinishShader

Returns a freshly allocated shader with all the needed info
from the current global working shader
=========================
*/
static shader_t *FinishShader( void ) {
	int			stage, i, n, m;
	qboolean	hasLightmapStage;
	qboolean	colorBlend;
	qboolean	depthMask;

	hasLightmapStage = qfalse;
	colorBlend = qfalse;
	depthMask = qfalse;

	//
	// set sky stuff appropriate
	//
	if ( shader.isSky ) {
		shader.sort = SS_ENVIRONMENT;
	}

	//
	// set polygon offset
	//
	if ( shader.polygonOffset && shader.sort == SS_BAD ) {
		shader.sort = SS_DECAL;
	}

	//
	// set appropriate stage information
	//
	for ( stage = 0; stage < MAX_SHADER_STAGES; ) {
		shaderStage_t *pStage = &stages[stage];

		if ( !pStage->active ) {
			break;
		}

		// check for a missing texture
		if ( !pStage->bundle[0].image[0] ) {
			ri.Printf( PRINT_WARNING, "Shader %s has a stage with no image\n", shader.name );
			pStage->active = qfalse;
			stage++;
			continue;
		}

		//
		// ditch this stage if it's detail and detail textures are disabled
		//
		if ( pStage->isDetail && !r_detailTextures->integer )
		{
			int index;

			for(index = stage + 1; index < MAX_SHADER_STAGES; index++)
			{
				if(!stages[index].active)
					break;
			}

			if(index < MAX_SHADER_STAGES)
				memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage));
			else
			{
				if(stage + 1 < MAX_SHADER_STAGES)
					memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage - 1));

				Com_Memset(&stages[index - 1], 0, sizeof(*stages));
			}

			continue;
		}

		//
		// default texture coordinate generation
		//
		if ( pStage->bundle[0].isLightmap ) {
			if ( pStage->bundle[0].tcGen == TCGEN_BAD ) {
				pStage->bundle[0].tcGen = TCGEN_LIGHTMAP;
			}
			hasLightmapStage = qtrue;
		} else {
			if ( pStage->bundle[0].tcGen == TCGEN_BAD ) {
				pStage->bundle[0].tcGen = TCGEN_TEXTURE;
			}
		}

		// not a true lightmap but we want to leave existing
		// behaviour in place and not print out a warning
		//if (pStage->rgbGen == CGEN_VERTEX) {
		//  vertexLightmap = qtrue;
		//}

		if ( pStage->stateBits & GLS_DEPTHMASK_TRUE ) {
			depthMask = qtrue;
		}

		//
		// determine fog color adjustment
		//
		if ( ( pStage->stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) &&
			 ( stages[0].stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) ) {
			GLbitfield blendSrcBits = pStage->stateBits & GLS_SRCBLEND_BITS;
			GLbitfield blendDstBits = pStage->stateBits & GLS_DSTBLEND_BITS;

			// fog color adjustment only works for blend modes that have a contribution
			// that aproaches 0 as the modulate values aproach 0 --
			// GL_ONE, GL_ONE
			// GL_ZERO, GL_ONE_MINUS_SRC_COLOR
			// GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA

			// modulate, additive
			if ( ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE ) ) ||
				( ( blendSrcBits == GLS_SRCBLEND_ZERO ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) ) {
				pStage->adjustColorsForFog = ACFF_MODULATE_RGB;
			}
			// strict blend
			else if ( ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
			{
				pStage->adjustColorsForFog = ACFF_MODULATE_ALPHA;
			}
			// premultiplied alpha
			else if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
			{
				pStage->adjustColorsForFog = ACFF_MODULATE_RGBA;
			}
			// ydnar: zero + blended alpha, one + blended alpha
			else if ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA || blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
			{
				pStage->adjustColorsForFog = ACFF_MODULATE_ALPHA;
			} else {
				// we can't adjust this one correctly, so it won't be exactly correct in fog
			}

			colorBlend = qtrue;
		}

		stage++;
	}

	// there are times when you will need to manually apply a sort to
	// opaque alpha tested shaders that have later blend passes
	if ( shader.sort == SS_BAD ) {
		if ( colorBlend ) {
			// see through item, like a grill or grate
			if ( depthMask ) {
				shader.sort = SS_SEE_THROUGH;
			} else {
				shader.sort = SS_BLEND0;
			}
		} else {
			shader.sort = SS_OPAQUE;
		}
	}

	DetectNeeds();

	// fix alphaGen flags to avoid redundant comparisons in R_ComputeColors()
	for ( i = 0; i < MAX_SHADER_STAGES; i++ ) {
		shaderStage_t *pStage = &stages[ i ];
		if ( !pStage->active )
			break;
		if ( pStage->rgbGen == CGEN_IDENTITY && pStage->alphaGen == AGEN_IDENTITY )
			pStage->alphaGen = AGEN_SKIP;
		else if ( pStage->rgbGen == CGEN_CONST && pStage->alphaGen == AGEN_CONST )
			pStage->alphaGen = AGEN_SKIP;
		else if ( pStage->rgbGen == CGEN_VERTEX && pStage->alphaGen == AGEN_VERTEX )
			pStage->alphaGen = AGEN_SKIP;
	}

	// whiteimage + "filter" texture == texture
	if ( stage > 1 && stages[0].bundle[0].image[0] == tr.whiteImage && stages[0].bundle[0].numImageAnimations <= 1 && stages[0].rgbGen == CGEN_IDENTITY && stages[0].alphaGen == AGEN_SKIP ) {
		if ( stages[1].stateBits == (GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO) ) {
			stages[1].stateBits = stages[0].stateBits & (GLS_DEPTHMASK_TRUE | GLS_DEPTHTEST_DISABLE | GLS_DEPTHFUNC_EQUAL);
			memmove( &stages[0], &stages[1], sizeof(stages[0]) * (stage-1) );
			stages[stage-1].active = qfalse;
			stage--;
		}
	}

	for ( i = 0; i < stage; i++ ) {
		stages[i].tessFlags = TESS_ST0;
	}

	//
	// look for multitexture potential
	//
	if ( r_ext_multitexture->integer ) {
		for ( i = 0; i < stage-1; i++ ) {
			if ( CollapseMultitexture( &stages[i+0], &stages[i+1], stage-i ) ) {
				stage--;
			}
		}
	}

	if ( shader.lightmapIndex >= 0 && !hasLightmapStage ) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name );
		shader.lightmapIndex = LIGHTMAP_NONE;
	}


	//
	// compute number of passes
	//
	shader.numUnfoggedPasses = stage;

	// fogonly shaders don't have any normal passes
	if ( stage == 0 && !shader.isSky )
		shader.sort = SS_FOG;

	if ( shader.sort <= SS_SEE_THROUGH ) {  // ydnar: was SS_DECAL, this allows grates to be fogged
		shader.fogPass = FP_EQUAL;
	} else if ( shader.contentFlags & CONTENTS_FOG ) {
		shader.fogPass = FP_LE;
	}

#ifdef USE_PMLIGHT
	FindLightingStages();
#endif

	// make sure that amplitude for TMOD_STRETCH is not zero
	for ( i = 0; i < shader.numUnfoggedPasses; i++ ) {
		if ( !stages[i].active ) {
			continue;
		}
		for ( n = 0; n < 2; n++ ) {
			for ( m = 0; m < stages[i].bundle[n].numTexMods; m++ ) {
				if ( stages[i].bundle[n].texMods[m].type == TMOD_STRETCH ) {
					if ( fabsf( stages[i].bundle[n].texMods[m].wave.amplitude ) < 1e-6f ) {
						if ( stages[i].bundle[n].texMods[m].wave.amplitude >= 0.0f ) {
							stages[i].bundle[n].texMods[m].wave.amplitude = 1e-6f;
						} else {
							stages[i].bundle[n].texMods[m].wave.amplitude = -1e-6f;
						}
					}
				}
			}
		}
	}

	// determine which stage iterator function is appropriate
	ComputeStageIteratorFunc();

	// default back to no compression for next shader
	if (r_ext_compressed_textures->integer == 2)
	{
		tr.allowCompress = compress_implicitBlock;
	}

	return GeneratePermanentShader();
}

//========================================================================================

//bani - dynamic shader list
typedef struct dynamicshader dynamicshader_t;
struct dynamicshader {
	char *shadertext;
	dynamicshader_t *next;
};
static dynamicshader_t *dshader = NULL;

/*
====================
RE_LoadDynamicShader

bani - load a new dynamic shader

if shadertext is NULL, looks for matching shadername and removes it

returns qtrue if request was successful, qfalse if the gods were angered
====================
*/
qboolean RE_LoadDynamicShader( const char *shadername, const char *shadertext ) {
	dynamicshader_t *dptr, *lastdptr;
	const char *q, *token;

	if ( !shadername && shadertext ) {
		ri.Printf( PRINT_WARNING, "WARNING: %s: called with NULL shadername and non-NULL shadertext:\n%s\n", __func__, shadertext );
		return qfalse;
	}

	if ( shadername && strlen( shadername ) >= MAX_QPATH ) {
		ri.Printf( PRINT_WARNING, "WARNING: %s: shadername %s exceeds MAX_QPATH\n", __func__, shadername );
		return qfalse;
	}

	//empty the whole list
	if ( !shadername && !shadertext ) {
		dptr = dshader;
		while ( dptr ) {
			lastdptr = dptr->next;
			Z_Free( dptr->shadertext );
			Z_Free( dptr );
			dptr = lastdptr;
		}
		dshader = NULL;
		return qtrue;
	}

	//walk list for existing shader to delete, or end of the list
	dptr = dshader;
	lastdptr = NULL;
	while ( dptr ) {
		q = dptr->shadertext;

		token = COM_ParseExt( &q, qtrue );

		if ( ( token[0] != 0 ) && !Q_stricmp( token, shadername ) ) {
			//request to nuke this dynamic shader
			if ( !shadertext ) {
				if ( !lastdptr ) {
					dshader = NULL;
				} else {
					lastdptr->next = dptr->next;
				}
				Z_Free( dptr->shadertext );
				Z_Free( dptr );
				return qtrue;
			}
			ri.Printf( PRINT_WARNING, "WARNING: %s: shader %s already exists!\n", __func__, shadername );
			return qfalse;
		}
		lastdptr = dptr;
		dptr = dptr->next;
	}

	//cant add a new one with empty shadertext
	if ( !shadertext || !strlen( shadertext ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: %s: new shader %s has NULL shadertext!\n", __func__, shadername );
		return qfalse;
	}

	//create a new shader
	dptr = (dynamicshader_t *)Z_Malloc( sizeof( *dptr ) );
	if ( !dptr ) {
		Com_Error( ERR_FATAL, "%s: Couldn't allocate struct for dynamic shader %s", __func__, shadername );
	}
	if ( lastdptr ) {
		lastdptr->next = dptr;
	}
	dptr->shadertext = Z_Malloc( strlen( shadertext ) + 1 );
	if ( !dptr->shadertext ) {
		Com_Error( ERR_FATAL, "%s: Couldn't allocate buffer for dynamic shader %s", __func__, shadername );
	}
	Q_strncpyz( dptr->shadertext, shadertext, strlen( shadertext ) + 1 );
	dptr->next = NULL;
	if ( !dshader ) {
		dshader = dptr;
	}

//	ri.Printf( PRINT_ALL, "Loaded dynamic shader [%s] with shadertext [%s]\n", shadername, shadertext );

	return qtrue;
}

/*
====================
FindShaderInShaderText

Scans the combined text description of all the shader files for
the given shader name.

return NULL if not found

If found, it will return a valid shader
=====================
*/
static const char *FindShaderInShaderText( const char *shadername ) {

	const char *token, *p;

	int i, hash;
#ifdef SH_LOADTIMING
	static int total = 0;

	int start = Sys_Milliseconds();
#endif // _DEBUG

	if ( !s_shaderText ) {
		return NULL;
	}

	//bani - if we have any dynamic shaders loaded, check them first
	if ( dshader ) {
		dynamicshader_t *dptr;
		const char *q;

		dptr = dshader;
		i = 0;
		while ( dptr ) {
			if ( !dptr->shadertext || !strlen( dptr->shadertext ) ) {
				ri.Printf( PRINT_WARNING, "WARNING: dynamic shader %s(%d) has no shadertext\n", shadername, i );
			} else {
				q = dptr->shadertext;

				token = COM_ParseExt( &q, qtrue );

				if ( ( token[0] != 0 ) && !Q_stricmp( token, shadername ) ) {
#ifdef SH_LOADTIMING
					total += Sys_Milliseconds() - start;
					Com_Printf( "Shader lookup: %i, total: %i\n", Sys_Milliseconds() - start, total );
#endif // _DEBUG
//					ri.Printf( PRINT_ALL, "Found dynamic shader [%s] with shadertext [%s]\n", shadername, dptr->shadertext );
					return q;
				}
			}
			i++;
			dptr = dptr->next;
		}
	}

	hash = generateHashValue(shadername, MAX_SHADERTEXT_HASH);

	if(shaderTextHashTable[hash])
	{
		for (i = 0; shaderTextHashTable[hash][i]; i++)
		{
			p = shaderTextHashTable[hash][i];
			token = COM_ParseExt(&p, qtrue);
			if(!Q_stricmp(token, shadername))
				return p;
		}
	}


#ifdef SH_LOADTIMING
	total += Sys_Milliseconds() - start;
	Com_Printf( "Shader lookup: %i, total: %i\n", Sys_Milliseconds() - start, total );
#endif // _DEBUG
	return NULL;
}


/*
==================
R_FindShaderByName

Will always return a valid shader, but it might be the
default shader if the real one can't be found.
==================
*/
shader_t *R_FindShaderByName( const char *name ) {
	char strippedName[MAX_QPATH];
	int hash;
	shader_t    *sh;

	if ( ( name == NULL ) || ( name[0] == 0 ) ) {
		return tr.defaultShader;
	}

	COM_StripExtension( name, strippedName, sizeof( strippedName ) );
	COM_FixPath( strippedName );

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	if ( tr.tcRenderShader && !tr.tcRenderShader->defaultShader && !Q_stricmp( name, tr.tcRenderShader->name ) ) {
		return tr.tcRenderShader;
	}

	//
	// see if the shader is already loaded
	//
	for ( sh = hashTable[hash]; sh; sh = sh->next ) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( Q_stricmp( sh->name, strippedName ) == 0 ) {
			// match found
			return sh;
		}
	}

	return tr.defaultShader;
}


/*
===============
R_CreateDefaultShading
===============
*/
static void R_CreateDefaultShading( image_t *image ) {
	// set implicit cull type
	if ( implicitCullType != CT_FRONT_SIDED && shader.cullType == CT_FRONT_SIDED ) {
		shader.cullType = implicitCullType;
	}

	// set shader stages
	switch ( shader.lightmapIndex )
	{
		// dynamic colors at vertexes
	case LIGHTMAP_NONE:
		stages[ 0 ].bundle[ 0 ].image[ 0 ] = image;
		stages[ 0 ].active = qtrue;
		stages[ 0 ].rgbGen = CGEN_LIGHTING_DIFFUSE;
		stages[ 0 ].stateBits = implicitStateBits;
		break;

		// gui elements (note state bits are overridden)
	case LIGHTMAP_2D:
		stages[ 0 ].bundle[ 0 ].image[ 0 ] = image;
		stages[ 0 ].active = qtrue;
		stages[ 0 ].rgbGen = CGEN_VERTEX;
		stages[ 0 ].alphaGen = AGEN_SKIP;
		stages[ 0 ].stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		break;

		// fullbright level
	case LIGHTMAP_WHITEIMAGE:
		stages[0].active = qtrue;
		stages[0].bundle[0].image[0] = image;
		stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		stages[0].stateBits = implicitStateBits;
		break;

		// explicit colors at vertexes
	case LIGHTMAP_BY_VERTEX:
		stages[ 0 ].bundle[ 0 ].image[ 0 ] = image;
		stages[ 0 ].active = qtrue;
		stages[ 0 ].rgbGen = CGEN_EXACT_VERTEX;
		stages[ 0 ].alphaGen = AGEN_SKIP;
		stages[ 0 ].stateBits = implicitStateBits;
		break;

		// use lightmap pass
	default:
		// masked or blended implicit shaders need texture first
		if ( implicitStateBits & ( GLS_ATEST_BITS | GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) {
			stages[ 0 ].bundle[ 0 ].image[ 0 ] = image;
			stages[ 0 ].active = qtrue;
			stages[ 0 ].rgbGen = CGEN_IDENTITY;
			stages[ 0 ].stateBits = implicitStateBits;

			stages[ 1 ].bundle[ 0 ].image[ 0 ] = tr.lightmaps[ shader.lightmapIndex ];
			stages[ 1 ].bundle[ 0 ].isLightmap = qtrue;
			stages[ 1 ].active = qtrue;
			stages[ 1 ].rgbGen = CGEN_IDENTITY;
			stages[ 1 ].stateBits = GLS_DEFAULT | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_EQUAL;
		}
		// otherwise do standard lightmap + texture
		else
		{
			stages[ 0 ].bundle[ 0 ].image[ 0 ] = tr.lightmaps[ shader.lightmapIndex ];
			stages[ 0 ].bundle[ 0 ].isLightmap = qtrue;
			stages[ 0 ].active = qtrue;
			stages[ 0 ].rgbGen = CGEN_IDENTITY;
			stages[ 0 ].stateBits = GLS_DEFAULT;

			stages[ 1 ].bundle[ 0 ].image[ 0 ] = image;
			stages[ 1 ].active = qtrue;
			stages[ 1 ].rgbGen = CGEN_IDENTITY;
			stages[ 1 ].stateBits = GLS_DEFAULT | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
		}
		break;
	}
}


#if 0
static const char *imageFormats[] = {
	"png",
	"tga",
	"jpg",
	"jpeg",
	"pcx",
	"bmp"
};
static const size_t numImageFormats = ARRAY_LEN( imageFormats );

int R_NumLightmapFiles( void ) {
	char **imageFiles;
	int numImageFiles, i, count = 0;

	if ( !*tr.worldRawName ) {
		return 0;
	}

	imageFiles = ri.FS_ListFilesEx( tr.worldRawName, imageFormats, numImageFormats, &numImageFiles );

	if ( !imageFiles || !numImageFiles ) {
		if( imageFiles )
			ri.FS_FreeFileList( imageFiles );
		return 0;
	}

	for ( i = 0; i < numImageFiles; i++ ) {
		if ( !Q_stricmpn( imageFiles[i], "lm_", 3 ) )
			++count;
	}

	ri.FS_FreeFileList( imageFiles );

	return count;
}
#endif


/*
===============
R_FindLightmap - ydnar
given a (potentially erroneous) lightmap index, attempts to load
an external lightmap image and/or sets the index to a valid number
===============
*/

#define EXTERNAL_LIGHTMAP   "lm_%04d.tga"    // THIS MUST BE IN SYNC WITH Q3MAP2

static void R_FindLightmap( int *lightmapIndex ) {
	image_t     *image;
	char fileName[ MAX_QPATH ];


	// don't fool with bogus lightmap indexes
	if ( *lightmapIndex < 0 || *lightmapIndex >= MAX_LIGHTMAPS ) {
		return;
	}

	// does this lightmap already exist?
	if ( tr.lightmaps[ *lightmapIndex ] != NULL ) {
		return;
	}

	// bail if no world dir
	if ( !*tr.worldRawName ) {
		*lightmapIndex = LIGHTMAP_BY_VERTEX;
		return;
	}

	//R_IssuePendingRenderCommands();

	// attempt to load an external lightmap
	Com_sprintf( fileName, sizeof (fileName), "%s/" EXTERNAL_LIGHTMAP, tr.worldRawName, *lightmapIndex );
	image = R_FindImageFile( fileName, IMGFLAG_NOSCALE | IMGFLAG_LIGHTMAP | IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE );
	if ( image == NULL ) {
		*lightmapIndex = LIGHTMAP_BY_VERTEX;
		return;
	}

	// add it to the lightmap list
	if ( *lightmapIndex >= tr.numLightmaps ) {
		tr.numLightmaps = *lightmapIndex + 1;
	}
	tr.lightmaps[ *lightmapIndex ] = image;
}

/*
===============
R_FindShader

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

In the interest of not requiring an explicit shader text entry to
be defined for every single image used in the game, three default
shader behaviors can be auto-created for any image:

If lightmapIndex == LIGHTMAP_NONE, then the image will have
dynamic diffuse lighting applied to it, as apropriate for most
entity skin surfaces.

If lightmapIndex == LIGHTMAP_2D, then the image will be used
for 2D rendering unless an explicit shader is found

If lightmapIndex == LIGHTMAP_BY_VERTEX, then the image will use
the vertex rgba modulate values, as apropriate for misc_model
pre-lit surfaces.

Other lightmapIndex values will have a lightmap stage created
and src*dest blending applied with the texture, as apropriate for
most world construction surfaces.

===============
*/
shader_t *R_FindShader( const char *name, int lightmapIndex, qboolean mipRawImage ) {
	char		strippedName[MAX_QPATH];
	char		fileName[MAX_QPATH];
	unsigned long hash;
	const char	*shaderText;
	image_t		*image;
	shader_t	*sh;

	if ( name[0] == '\0' ) {
		return tr.defaultShader;
	}

	// ydnar: validate lightmap index
	R_FindLightmap( &lightmapIndex );

	COM_StripExtension( name, strippedName, sizeof( strippedName ) );
	COM_FixPath( strippedName );

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	if ( tr.tcRenderShader && !tr.tcRenderShader->defaultShader && !Q_stricmp( name, tr.tcRenderShader->name ) ) {
		return tr.tcRenderShader;
	}

	//
	// see if the shader is already loaded
	//
#if 1
	for ( sh = hashTable[hash]; sh; sh = sh->next ) {
		// index by name

		// ydnar: the original way was correct
		if ( sh->lightmapSearchIndex == lightmapIndex &&
			 !Q_stricmp( sh->name, strippedName ) ) {
			// match found
			return sh;
		}

		// Ridah, modified this so we don't keep trying to load an invalid lightmap shader
		#if 0
		if ( ( ( sh->lightmapSearchIndex == lightmapIndex ) || ( sh->lightmapIndex < 0 && lightmapIndex >= 0 ) ) &&
			 !Q_stricmp( sh->name, strippedName ) ) {
			// match found
			return sh;
		}
		#endif
	}
#else
	for ( sh = hashTable[hash]; sh; sh = sh->next ) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( ( sh->lightmapSearchIndex == lightmapIndex || sh->defaultShader ) &&
			 !Q_stricmp( sh->name, strippedName ) ) {
			// match found
			return sh;
		}
	}
#endif

	// Ridah, check the cache
	// assignment used as truth value
	// ydnar: don't cache shaders using lightmaps
	if ( lightmapIndex < 0 ) {
		sh = R_FindCachedShader( strippedName, lightmapIndex, hash );
		if ( sh != NULL ) {
			return sh;
		}
	}
	// done.

	InitShader( strippedName, lightmapIndex );

	// FIXME: set these "need" values apropriately
	//shader.needsNormal = qtrue;
	//shader.needsST1 = qtrue;
	//shader.needsST2 = qtrue;
	//shader.needsColor = qtrue;

	// ydnar: default to no implicit mappings
	implicitMap[ 0 ] = '\0';
	implicitStateBits = GLS_DEFAULT;
	implicitCullType = CT_FRONT_SIDED;

	//
	// attempt to define shader from an explicit parameter file
	//
	shaderText = FindShaderInShaderText( strippedName );
	if ( shaderText ) {
		// enable this when building a pak file to get a global list
		// of all explicit shaders
		if ( r_printShaders->integer ) {
			ri.Printf( PRINT_ALL, "*SHADER* %s\n", name );
		}

		if ( !ParseShader( &shaderText ) ) {
			// had errors, so use default shader
			shader.defaultShader = qtrue;
			sh = FinishShader();
			return sh;
		}

		// ydnar: allow implicit mappings
		if ( implicitMap[ 0 ] == '\0' ) {
			sh = FinishShader();
			return sh;
		}
	}


	// ydnar: allow implicit mapping ('-' = use shader name)
	if ( implicitMap[ 0 ] == '\0' || implicitMap[ 0 ] == '-' ) {
		Q_strncpyz( fileName, name, sizeof( fileName ) );
	} else {
		Q_strncpyz( fileName, implicitMap, sizeof( fileName ) );
	}
	COM_DefaultExtension( fileName, sizeof( fileName ), ".tga" );

	// ydnar: implicit shaders were breaking nopicmip/nomipmaps
	if ( !mipRawImage ) {
		shader.noMipMaps = qtrue;
		shader.noPicMip = qtrue;
	}

	//
	// if not defined in the in-memory shader descriptions,
	// look for a single supported image file
	//
	{
		imgFlags_t flags = IMGFLAG_NONE;

		if ( !shader.noMipMaps )
			flags |= IMGFLAG_MIPMAP;

		if ( !shader.noPicMip )
			flags |= IMGFLAG_PICMIP;

		if ( !mipRawImage )
			flags |= IMGFLAG_CLAMPTOEDGE;

		image = R_FindImageFile( fileName, flags );
		if ( !image ) {
			//ri.Printf( PRINT_DEVELOPER, "Couldn't find image for shader %s\n", name );
			ri.Printf( PRINT_WARNING, "Couldn't find image file for shader %s\n", name );
			shader.defaultShader = qtrue;
			return FinishShader();
		}
	}

	//
	// create the default shading commands
	//
	R_CreateDefaultShading( image );

	return FinishShader();
}


qhandle_t RE_RegisterShaderFromImage( const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage ) {
	int hash;
	shader_t    *sh;

	hash = generateHashValue(name, FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for ( sh = hashTable[hash]; sh; sh = sh->next ) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( ( sh->lightmapSearchIndex == lightmapIndex || sh->defaultShader ) &&
			 // index by name
			 !Q_stricmp( sh->name, name ) ) {
			// match found
			return sh->index;
		}
	}

	InitShader( name, lightmapIndex );

	// FIXME: set these "need" values appropriately
	//shader.needsNormal = qtrue;
	//shader.needsST1 = qtrue;
	//shader.needsST2 = qtrue;
	//shader.needsColor = qtrue;

	//
	// create the default shading commands
	//
	R_CreateDefaultShading( image );

	sh = FinishShader();

	return sh->index;
}


/*
====================
RE_RegisterShaderLightMap

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShaderLightMap( const char *name, int lightmapIndex ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, lightmapIndex, qtrue );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}


/*
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShader( const char *name ) {
	shader_t	*sh;

	if ( !name ) {
		ri.Printf( PRINT_ALL, "NULL shader\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, LIGHTMAP_2D, qtrue );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}


/*
====================
RE_RegisterShaderNoMip

For menu graphics that should never be picmiped
====================
*/
qhandle_t RE_RegisterShaderNoMip( const char *name ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, LIGHTMAP_2D, qfalse );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}

/*
====================
R_GetShaderByHandle

When a handle is passed in by another module, this range checks
it and returns a valid (possibly default) shader_t to be used internally.
====================
*/
shader_t *R_GetShaderByHandle( qhandle_t hShader ) {
	if ( hShader < 0 ) {
		ri.Printf( PRINT_DEVELOPER, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader ); // bk: FIXME name
		return tr.defaultShader;
	}
	if ( hShader >= tr.numShaders ) {
		ri.Printf( PRINT_DEVELOPER, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
		return tr.defaultShader;
	}
	return tr.shaders[hShader];
}

/*
===============
R_ShaderList_f

Dump information on all valid shaders to the console
A second parameter will cause it to print in sorted order
===============
*/
void	R_ShaderList_f (void) {
	int			i;
	int			count;
	const shader_t *sh;
	const char *match;

	if ( Cmd_Argc() > 1 ) {
		match = Cmd_Argv( 1 );
	} else {
		match = NULL;
	}

	ri.Printf (PRINT_ALL, "-----------------------------\n");
	ri.Printf (PRINT_ALL, "P L MT    E func order   name \n");

	count = 0;
	for ( i = 0 ; i < tr.numShaders ; i++ ) {
		sh = tr.sortedShaders[i];
		if ( match && !Com_Filter( match, sh->name ) ) {
			continue;
		}

		ri.Printf( PRINT_ALL, "%i ", sh->numUnfoggedPasses );

		if ( sh->lightmapIndex >= 0 ) {
			ri.Printf (PRINT_ALL, "L ");
		} else {
			ri.Printf (PRINT_ALL, "  ");
		}
		if ( sh->multitextureEnv ) {
			ri.Printf( PRINT_ALL, "MT(x) " ); // TODO: per-stage statistics?
		} else {
			ri.Printf( PRINT_ALL, "      " );
		}
		if ( sh->explicitlyDefined ) {
			ri.Printf( PRINT_ALL, "E " );
		} else {
			ri.Printf( PRINT_ALL, "  " );
		}

		if ( sh->optimalStageIteratorFunc == RB_StageIteratorGeneric ) {
			ri.Printf( PRINT_ALL, "gen " );
		} else if ( sh->optimalStageIteratorFunc == RB_StageIteratorSky ) {
			ri.Printf( PRINT_ALL, "sky " );
		} else {
			ri.Printf( PRINT_ALL, "    " );
		}

		ri.Printf( PRINT_ALL, "%5.2f ", sh->sort );

		if ( sh->defaultShader ) {
			ri.Printf( PRINT_ALL, ": %s (DEFAULTED)\n", sh->name );
		} else {
			ri.Printf( PRINT_ALL, ": %s\n", sh->name );
		}
		count++;
	}
	ri.Printf (PRINT_ALL, "%i shaders found\n", count);
	ri.Printf (PRINT_ALL, "-----------------------------\n");
}


#define	MAX_SHADER_FILES 16384

static int loadShaderBuffers( char **shaderFiles, const int numShaderFiles, char **buffers )
{
	char filename[MAX_QPATH+8];
	char shaderName[MAX_QPATH];
	const char *p, *token;
	long summand, sum = 0;
	int shaderLine;
	int i;
	const char *shaderStart;
	qboolean denyErrors;

	// load and parse shader files
	for ( i = 0; i < numShaderFiles; i++ )
	{
		Com_sprintf( filename, sizeof( filename ), "scripts/%s", shaderFiles[i] );
		//ri.Printf( PRINT_DEVELOPER, "...loading '%s'\n", filename );
		summand = ri.FS_ReadFile( filename, (void **)&buffers[i] );

		if ( !buffers[i] )
			ri.Error( ERR_DROP, "Couldn't load %s", filename );

		// comment broken buggy shader in old etf_gfx.shader
		if ( summand == 25640 && strcmp( shaderFiles[i], "etf_gfx.shader" ) == 0 ) {
			if( memcmp( buffers[i] + 0x53C5, "gf", 2 ) == 0 )
			{
				memcpy( buffers[i] + 0x53C5, "/*", 2 );
				memcpy( buffers[i] + 0x5459, "*/", 2 );
			}
		}
		
		// comment some buggy shaders from pak0
		//if ( summand == 35910 && strcmp( shaderFiles[i], "sky.shader" ) == 0) 
		//{
		//	if ( memcmp( buffers[i] + 0x3D3E, "\tcloudparms ", 12 ) == 0 ) 
		//	{
		//		memcpy( buffers[i] + 0x27D7, "/*", 2 );
		//		memcpy( buffers[i] + 0x2A93, "*/", 2 );

		//		memcpy( buffers[i] + 0x3CA9, "/*", 2 );
		//		memcpy( buffers[i] + 0x3FC2, "*/", 2 );
		//	}
		//}

		p = buffers[i];
		COM_BeginParseSession( filename );
		
		shaderStart = NULL;
		denyErrors = qfalse;

		while ( 1 )
		{
			token = COM_ParseExt( &p, qtrue );
			
			if ( !*token )
				break;

			Q_strncpyz( shaderName, token, sizeof( shaderName ) );
			shaderLine = COM_GetCurrentParseLine();

			token = COM_ParseExt( &p, qtrue );
			if ( token[0] != '{' || token[1] != '\0' )
			{
				ri.Printf( PRINT_DEVELOPER, "File %s: shader \"%s\" " \
					"on line %d missing opening brace", filename, shaderName, shaderLine );
				if ( token[0] )
					ri.Printf( PRINT_DEVELOPER, " (found \"%s\" on line %d)\n", token, COM_GetCurrentParseLine() );
				else
					ri.Printf( PRINT_DEVELOPER, "\n" );

				if ( denyErrors || !p )
				{
					ri.Printf( PRINT_WARNING, "Ignoring entire file '%s' due to error.\n", filename );
					ri.FS_FreeFile( buffers[i] );
					buffers[i] = NULL;
					break;
				}

				SkipRestOfLine( &p );
				shaderStart = p;
				continue;
			}

			if ( !SkipBracedSection( &p, 1 ) )
			{
				ri.Printf(PRINT_WARNING, "WARNING: Ignoring shader file %s. Shader \"%s\" " \
					"on line %d missing closing brace.\n", filename, shaderName, shaderLine );
				ri.FS_FreeFile( buffers[i] );
				buffers[i] = NULL;
				break;
			}

			denyErrors = qtrue;
		}

		if ( buffers[ i ] ) {
			if ( shaderStart ) {
				summand -= (shaderStart - buffers[i]);
				if ( summand >= 0 ) {
					memmove( buffers[i], shaderStart, summand + 1 );
				}
			}
			COM_FixPath( buffers[ i ] ); // ydnar: unixify all shaders (moved here)
			//sum += summand;
			sum += COM_Compress( buffers[ i ] );
		}
	}

	return sum;
}


/*
====================
ScanAndLoadShaderFiles

Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
static void ScanAndLoadShaderFiles( void )
{
	char **shaderFiles, **shaderxFiles;
	char *buffers[MAX_SHADER_FILES];
	char *xbuffers[MAX_SHADER_FILES];
	int numShaderFiles, numShaderxFiles;
	int i;
	char *token, *hashMem, *textEnd;
	const char *p, *oldp;
	int shaderTextHashTableSizes[MAX_SHADERTEXT_HASH], hash, size;

	long sum = 0;

	// scan for legacy shader files
	shaderFiles = ri.FS_ListFiles( "scripts", ".shader", &numShaderFiles );

	if ( GL_ProgramAvailable() ) {
		// if ARB shaders available - scan for extended shader files
		shaderxFiles = ri.FS_ListFiles( "scripts", ".shaderx", &numShaderxFiles );
	} else {
		shaderxFiles = NULL;
		numShaderxFiles = 0;
	}

	if ( (!shaderFiles || !numShaderFiles) && (!shaderxFiles || !numShaderxFiles) ) {
		ri.Printf( PRINT_WARNING, "WARNING: no shader files found\n" );
		return;
	}

	if ( numShaderFiles > MAX_SHADER_FILES ) {
		numShaderFiles = MAX_SHADER_FILES;
	}
	if ( numShaderxFiles > MAX_SHADER_FILES ) {
		numShaderxFiles = MAX_SHADER_FILES;
	}

	sum = 0;
	sum += loadShaderBuffers( shaderxFiles, numShaderxFiles, xbuffers );
	sum += loadShaderBuffers( shaderFiles, numShaderFiles, buffers );

	// build single large buffer
	s_shaderText = ri.Hunk_Alloc( sum + numShaderxFiles*2 + numShaderFiles*2 + 1, h_low );
	s_shaderText[ 0 ] = s_shaderText[ sum + numShaderxFiles*2 + numShaderFiles*2 ] = '\0';

	textEnd = s_shaderText;

	// free in reverse order, so the temp files are all dumped
	// legacy shaders
	for ( i = numShaderFiles - 1; i >= 0 ; i-- ) {
		if ( buffers[ i ] ) {
			textEnd = Q_stradd( textEnd, buffers[ i ] );
			textEnd = Q_stradd( textEnd, "\n" );
			ri.FS_FreeFile( buffers[ i ] );
		}
	}

	// if shader text >= s_extensionOffset then it is an extended shader
	// normal shaders will never encounter that
	s_extensionOffset = textEnd;

	// extended shaders
	for ( i = numShaderxFiles - 1; i >= 0 ; i-- ) {
		if ( xbuffers[ i ] ) {
			textEnd = Q_stradd( textEnd, xbuffers[ i ] );
			textEnd = Q_stradd( textEnd, "\n" );
			ri.FS_FreeFile( xbuffers[ i ] );
		}
	}

	// free up memory
	if ( shaderxFiles )
		ri.FS_FreeFileList( shaderxFiles );
	if ( shaderFiles )
		ri.FS_FreeFileList( shaderFiles );

	Com_Memset( shaderTextHashTableSizes, 0, sizeof( shaderTextHashTableSizes ) );
	size = 0;

	p = s_shaderText;
	// look for shader names
	while ( 1 ) {
		token = COM_ParseExt( &p, qtrue );
		if ( token[0] == 0 ) {
			break;
		}
		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		shaderTextHashTableSizes[hash]++;
		size++;
		SkipBracedSection(&p, 0);
	}

	size += MAX_SHADERTEXT_HASH;

	hashMem = ri.Hunk_Alloc( size * sizeof(char *), h_low );

	for (i = 0; i < MAX_SHADERTEXT_HASH; i++) {
		shaderTextHashTable[i] = (char **) hashMem;
		hashMem = ((char *) hashMem) + ((shaderTextHashTableSizes[i] + 1) * sizeof(char *));
	}

	p = s_shaderText;
	// look for shader names
	while ( 1 ) {
		oldp = p;
		token = COM_ParseExt( &p, qtrue );
		if ( token[0] == 0 ) {
			break;
		}

		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		shaderTextHashTable[hash][--shaderTextHashTableSizes[hash]] = (char*)oldp;

		SkipBracedSection(&p, 0);
	}
}


/*
====================
CreateInternalShaders
====================
*/
static void CreateInternalShaders( void ) {
	tr.numShaders = 0;

	// init the default shader
	InitShader( "<default>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].active = qtrue;
	stages[0].stateBits = GLS_DEFAULT;
	tr.defaultShader = FinishShader();

	InitShader( "<whiteShader>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.whiteImage;
	stages[0].active = qtrue;
	stages[0].rgbGen = CGEN_EXACT_VERTEX;
	stages[0].stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	tr.whiteShader = FinishShader();

	// shadow shader is just a marker
	InitShader( "<stencil shadow>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].active = qtrue;
	stages[0].stateBits = GLS_DEFAULT;
	shader.sort = SS_STENCIL_SHADOW;
	tr.shadowShader = FinishShader();

	InitShader( "<cinematic>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.defaultImage; // will be updated by specific cinematic images
	stages[0].active = qtrue;
	stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
	stages[0].stateBits = GLS_DEPTHTEST_DISABLE;
	tr.cinematicShader = FinishShader();

	InitShader( "<tcRender>", LIGHTMAP_NONE );
	shader.polygonOffset = qtrue;
	shader.noPicMip = qtrue;
	stages[0].bundle[0].image[0] = tr.tcRenderImage;
	stages[0].active = qtrue;
	stages[0].rgbGen = CGEN_VERTEX;
	stages[0].alphaGen = AGEN_VERTEX;
	stages[0].stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
	tr.tcRenderShader = FinishShader();
}


/*
====================
CreateExternalShaders
====================
*/
static void CreateExternalShaders( void ) {
	tr.projectionShadowShader = R_FindShader( "projectionShadow", LIGHTMAP_NONE, qtrue );
	tr.flareShader = R_FindShader( "flareShader", LIGHTMAP_NONE, qtrue );

	// Hack to make fogging work correctly on flares. Fog colors are calculated
	// in tr_flare.c already.
	if(!tr.flareShader->defaultShader)
	{
		int index;

		for(index = 0; index < tr.flareShader->numUnfoggedPasses; index++)
		{
			tr.flareShader->stages[index]->adjustColorsForFog = ACFF_NONE;
			tr.flareShader->stages[index]->stateBits |= GLS_DEPTHTEST_DISABLE;
		}
	}

//	tr.sunShader = R_FindShader( "sun", LIGHTMAP_NONE, qtrue );	//----(SA)	let sky shader set this
	tr.sunflareShader[0] = R_FindShader( "sunflare1", LIGHTMAP_NONE, qtrue );
	tr.dlightShader = R_FindShader( "dlightshader", LIGHTMAP_NONE, qtrue );
}

//=============================================================================
// Ridah, shader caching
static int numBackupShaders = 0;
static shader_t *backupShaders[MAX_SHADERS];
static shader_t *backupHashTable[FILE_HASH_SIZE];

/*
===============
R_CacheShaderAlloc
===============
*/
//int g_numshaderallocs = 0;
//void *R_CacheShaderAlloc( int size ) {
void* R_CacheShaderAllocExt( const char* name, int size, const char* file, int line ) {
	if ( r_cache->integer && r_cacheShaders->integer ) {
		void* ptr = ri.Malloc( size );

//		g_numshaderallocs++;

//		if( name ) {
//			Com_Printf( "Zone Malloc from %s: size %i: pointer %p: %i in use\n", name, size, ptr, g_numshaderallocs );
//		}

		//return malloc( size );
		return ptr;
	} else {
		return ri.Hunk_Alloc( size, h_low );
	}
}

/*
===============
R_CacheShaderFree
===============
*/
//void R_CacheShaderFree( void *ptr ) {
void R_CacheShaderFreeExt( const char* name, void *ptr, const char* file, int line ) {
	if ( r_cache->integer && r_cacheShaders->integer ) {
//		g_numshaderallocs--;

//		if( name ) {
//			Com_Printf( "Zone Free from %s: pointer %p: %i in use\n", name, ptr, g_numshaderallocs );
//		}

		//free( ptr );
		ri.Free( ptr );
	}
}

/*
===============
R_PurgeShaders
===============
*/

qboolean purgeallshaders = qfalse;
void R_PurgeShaders( int count ) {
	/*int i, j, c, b;
	shader_t **sh;
	static int lastPurged = 0;

	if (!numBackupShaders) {
		lastPurged = 0;
		return;
	}

	// find the first shader still in memory
	c = 0;
	sh = (shader_t **)&backupShaders;
	for (i = lastPurged; i < numBackupShaders; i++, sh++) {
		if (*sh) {
			// free all memory associated with this shader
			for ( j = 0 ; j < (*sh)->numUnfoggedPasses ; j++ ) {
				if ( !(*sh)->stages[j] ) {
					break;
				}
				for ( b = 0 ; b < NUM_TEXTURE_BUNDLES ; b++ ) {
					if ((*sh)->stages[j]->bundle[b].texMods)
						R_CacheShaderFree( NULL, (*sh)->stages[j]->bundle[b].texMods );
				}
				R_CacheShaderFree( NULL, (*sh)->stages[j] );
			}
			R_CacheShaderFree( (*sh)->lightmapIndex ? va( "%s lm: %i", (*sh)->name, (*sh)->lightmapIndex) : NULL, *sh );
			*sh = NULL;

			if (++c >= count) {
				lastPurged = i;
				return;
			}
		}
	}
	lastPurged = 0;
	numBackupShaders = 0;*/

	if ( !numBackupShaders ) {
		return;
	}

	purgeallshaders = qtrue;

	R_PurgeLightmapShaders();

	purgeallshaders = qfalse;
}

qboolean R_ShaderCanBeCached( shader_t *sh ) {
	int i,j,b;

	if ( purgeallshaders ) {
		return qfalse;
	}

	if ( sh->isSky ) {
		return qfalse;
	}

	for ( i = 0; i < sh->numUnfoggedPasses; i++ ) {
		shaderStage_t* st = sh->stages[i];
		if ( st && st->active ) {
			for ( b = 0 ; b < NUM_TEXTURE_BUNDLES ; b++ ) {
				// rain - swapped order of for() comparisons so that
				// image[16] (out of bounds) isn't dereferenced
				//for (j=0; sh->stages[i]->bundle[b].image[j] && j < MAX_IMAGE_ANIMATIONS; j++) {
				for ( j = 0; j < MAX_IMAGE_ANIMATIONS && st->bundle[b].image[j]; j++ ) {
					image_t *img = st->bundle[b].image[j];
					if ( (img->flags & IMGFLAG_LIGHTMAP) || img->imgName[0] == '*' ) {
						return qfalse;
					}
				}
			}
		}
	}
	return qtrue;
}

void R_PurgeLightmapShaders( void ) {
	int j, b, i = 0;
	shader_t *sh, *shPrev, *next;

	for ( i = 0; i < (int)ARRAY_LEN(backupHashTable); i++ ) {
		sh = backupHashTable[i];

		shPrev = NULL;
		next = NULL;

		while ( sh ) {
			if ( sh->lightmapIndex >= 0 || !R_ShaderCanBeCached( sh ) ) {
				next = sh->next;

				if ( !shPrev ) {
					backupHashTable[i] = sh->next;
				} else {
					shPrev->next = sh->next;
				}

				backupShaders[sh->index] = NULL;    // make sure we don't try and free it

				numBackupShaders--;

				for ( j = 0 ; j < sh->numUnfoggedPasses ; j++ ) {
					if ( !sh->stages[j] ) {
						break;
					}
					for ( b = 0 ; b < NUM_TEXTURE_BUNDLES ; b++ ) {
						if ( sh->stages[j]->bundle[b].texMods ) {
							R_CacheShaderFree( NULL, sh->stages[j]->bundle[b].texMods );
						}
					}
					R_CacheShaderFree( NULL, sh->stages[j] );
				}
				R_CacheShaderFree( sh->lightmapIndex < 0 ? va( "%s lm: %i", sh->name, sh->lightmapIndex ) : NULL, sh );

				sh = next;

				continue;
			}

			shPrev = sh;
			sh = sh->next;
		}
	}
}

/*
===============
R_BackupShaders
===============
*/
void R_BackupShaders( void ) {
//	int i;
//	long hash;

	if ( !r_cache->integer ) {
		return;
	}
	if ( !r_cacheShaders->integer ) {
		return;
	}

	// copy each model in memory across to the backupModels
	memcpy( backupShaders, tr.shaders, sizeof( backupShaders ) );
	// now backup the hashTable
	memcpy( backupHashTable, hashTable, sizeof( hashTable ) );

	numBackupShaders = tr.numShaders;
	tr.numShaders = 0;

	// Gordon: ditch all lightmapped shaders
	R_PurgeLightmapShaders();

//	Com_Printf( "Backing up %i images\n", numBackupShaders );

//	for( i = 0; i < tr.numShaders; i++ ) {
//		if( backupShaders[ i ] ) {
//			Com_Printf( "Shader: %s: lm %i\n", backupShaders[ i ]->name, backupShaders[i]->lightmapIndex );
//		}
//	}

//	Com_Printf( "=======================================\n" );
}

/*
=================
R_RegisterShaderImages

  Make sure all images that belong to this shader remain valid
=================
*/
static qboolean R_RegisterShaderImages( shader_t *sh ) {
	int i,j,b;

	if ( sh->isSky ) {
		return qfalse;
	}

	for ( i = 0; i < sh->numUnfoggedPasses; i++ ) {
		if ( sh->stages[i] && sh->stages[i]->active ) {
			for ( b = 0 ; b < NUM_TEXTURE_BUNDLES ; b++ ) {
				for ( j = 0; sh->stages[i]->bundle[b].image[j] && j < MAX_IMAGE_ANIMATIONS; j++ ) {
					if ( !R_TouchImage( sh->stages[i]->bundle[b].image[j] ) ) {
						return qfalse;
					}
				}
			}
		}
	}
	return qtrue;
}

/*
===============
R_FindCachedShader

  look for the given shader in the list of backupShaders
===============
*/
shader_t *R_FindCachedShader( const char *name, int lightmapIndex, int hash ) {
	shader_t *sh, *shPrev;

	if ( !r_cacheShaders->integer ) {
		return NULL;
	}

	if ( !numBackupShaders ) {
		return NULL;
	}

	if ( !name ) {
		return NULL;
	}

	sh = backupHashTable[hash];
	shPrev = NULL;
	while ( sh ) {
		if ( sh->lightmapSearchIndex == lightmapIndex && !Q_stricmp( sh->name, name ) ) {
			if ( tr.numShaders == MAX_SHADERS ) {
				ri.Printf( PRINT_WARNING, "WARNING: R_FindCachedShader - MAX_SHADERS hit\n" );
				return NULL;
			}

			// make sure the images stay valid
			if ( !R_RegisterShaderImages( sh ) ) {
				return NULL;
			}

			// this is the one, so move this shader into the current list

			if ( !shPrev ) {
				backupHashTable[hash] = sh->next;
			} else {
				shPrev->next = sh->next;
			}

			sh->next = hashTable[hash];
			hashTable[hash] = sh;

			backupShaders[sh->index] = NULL;    // make sure we don't try and free it

			// set the index up, and add it to the current list
			tr.shaders[ tr.numShaders ] = sh;
			sh->index = tr.numShaders;

			tr.sortedShaders[ tr.numShaders ] = sh;
			sh->sortedIndex = tr.numShaders;

			tr.numShaders++;

			numBackupShaders--;

			sh->remappedShader = NULL;  // Arnout: remove any remaps

			SortNewShader();    // make sure it renders in the right order

//			Com_Printf( "Removing %s from the cache: lm: %i\n", sh->name, sh->lightmapIndex );

			return sh;
		}

		shPrev = sh;
		sh = sh->next;
	}

	return NULL;
}

/*
===============
R_LoadCacheShaders
===============
*/
void R_LoadCacheShaders( void ) {
	int len;
	byte *buf;
	const char    *token, *pString;
	char name[MAX_QPATH];

	if ( !r_cacheShaders->integer ) {
		return;
	}

	// don't load the cache list in between level loads, only on startup, or after a vid_restart
	if ( numBackupShaders > 0 ) {
		return;
	}

	len = ri.FS_ReadFile( "shader.cache", NULL );

	if ( len <= 0 ) {
		return;
	}

	buf = (byte *)ri.Hunk_AllocateTempMemory( len );
	ri.FS_ReadFile( "shader.cache", (void **)&buf );
	pString = (const char *)buf;

	while ( ( token = COM_ParseExt( &pString, qtrue ) ) != NULL && token[0] ) {
		Q_strncpyz( name, token, sizeof( name ) );
		RE_RegisterShader( name );
	}

	ri.Hunk_FreeTempMemory( buf );
}
// done.
//=============================================================================

/*
==================
R_InitShaders
==================
*/
void R_InitShaders( void ) {

	glfogNum = FOG_NONE;

	ri.Printf( PRINT_ALL, "Initializing Shaders\n" );

	Com_Memset(hashTable, 0, sizeof(hashTable));

	CreateInternalShaders();

	ScanAndLoadShaderFiles();

	CreateExternalShaders();

	// Ridah
	R_LoadCacheShaders();
}
