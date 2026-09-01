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
// tr_shade.c

#include "tr_local.h"

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/


/*
==================
R_DrawElements
==================
*/
void R_DrawElements( int numIndexes, const glIndex_t *indexes ) {
	qglDrawElements( GL_TRIANGLES, numIndexes, GL_INDEX_TYPE, indexes );
}


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;
static qboolean	setArraysOnce;

/*
=================
R_BindAnimatedImage
=================
*/
void R_BindAnimatedImage( const textureBundle_t *bundle ) {
	int64_t index;
	double	v;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

#ifdef USE_FBO
	if ( bundle->isScreenMap && backEnd.viewParms.frameSceneNum == 1 ) {
		GL_BindTexNum( FBO_ScreenTexture() );
		return;
	}
#endif

	if ( bundle->numImageAnimations <= 1 ) {
		if ( bundle->isLightmap && ( backEnd.refdef.rdflags & RDF_SNOOPERVIEW ) ) {
			GL_Bind( tr.whiteImage );
		} else {
			GL_Bind( bundle->image[0] );
		}
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	//v = tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE;
	//index = v;
	//index >>= FUNCTABLE_SIZE2;

	v = tess.shaderTime * bundle->imageAnimationSpeed; // fix for frameloss bug -EC-
	index = v;

	if ( index < 0 ) {
		index = 0;	// may happen with shader time offsets
	}
	index %= bundle->numImageAnimations;

	if ( bundle->isLightmap && ( backEnd.refdef.rdflags & RDF_SNOOPERVIEW ) ) {
		GL_Bind( tr.whiteImage );
	} else {
		GL_Bind( bundle->image[ index ] );
	}
}


void R_SetTrisColor( void ) {
	const char *s = r_trisColor->string;

	if ( *s == '0' && ( *( s + 1 ) == 'x' || *( s + 1 ) == 'X' ) ) {
		s += 2;
		if ( Q_IsHexColorString( s ) ) {
			tr.trisColor[0] = ( (float)( gethex( *( s ) ) * 16 + gethex( *( s + 1 ) ) ) ) / 255.00;
			tr.trisColor[1] = ( (float)( gethex( *( s + 2 ) ) * 16 + gethex( *( s + 3 ) ) ) ) / 255.00;
			tr.trisColor[2] = ( (float)( gethex( *( s + 4 ) ) * 16 + gethex( *( s + 5 ) ) ) ) / 255.00;

			if ( Q_HexColorStringHasAlpha( s ) ) {
				tr.trisColor[3] = ( (float)( gethex( *( s + 6 ) ) * 16 + gethex( *( s + 7 ) ) ) ) / 255.00;
			}
		}
	} else {
		int i;
		const char *token;

		for ( i = 0 ; i < 4 ; i++ ) {
			token = COM_ParseExt( &s, qfalse );
			if ( token ) {
				tr.trisColor[i] = Q_atof( token );
			} else {
				tr.trisColor[i] = 1.f;
			}
		}

		if ( !tr.trisColor[3] ) {
			tr.trisColor[3] = 1.f;
		}
	}
}


/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris( const shaderCommands_t *input ) {
	GLbitfield stateBits = 0;
	GLboolean didDepth = GL_FALSE, polygonState = GL_FALSE;

	if ( r_showtris->integer == 1 && (backEnd.drawConsole || backEnd.projection2D) )
		return;

	if ( tess.numIndexes == 0 )
		return;

	if ( r_fastsky->integer && input->shader->isSky )
		return;

	GL_ProgramDisable();

#ifdef USE_PMLIGHT
	tess.dlightUpdateParams = qtrue;
#endif

	GL_ClientState( 0, CLS_NONE );
	qglDisable( GL_TEXTURE_2D );

	if ( tr.trisColor[3] < 1.f ) {
		stateBits |= ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

#ifdef USE_PMLIGHT
	if ( tess.dlightPass )
		qglColor4f( 1.0f, 0.33f, 0.2f, tr.trisColor[3] );
	else
#endif
	qglColor4f( tr.trisColor[0], tr.trisColor[1], tr.trisColor[2], tr.trisColor[3] );

	if ( r_trisMode->integer ) {
		stateBits |= ( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
		GL_State( stateBits );
		qglDepthRange( 0, 0 );
		didDepth = GL_TRUE;
	}
	else {
		stateBits |= ( GLS_POLYMODE_LINE );
		GL_State( stateBits );
		qglEnable( GL_POLYGON_OFFSET_LINE );
		polygonState = GL_TRUE;
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	// ydnar r_showtris 2
	/*if ( r_showtris->integer == 2 ) {
		stateBits |= ( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
		GL_State( stateBits );
		qglDepthRange( 0, 0 );
		didDepth = GL_TRUE;
	}
	#ifdef CELSHADING_HACK
	else if ( r_showtris->integer == 3 ) {
		stateBits |= ( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
		GL_State( stateBits );
		qglEnable( GL_POLYGON_OFFSET_LINE );
		polygonState = GL_TRUE;
		qglPolygonOffset( 4.0, 0.5 );
		qglLineWidth( 5.0 );
	}
	#endif
	else
	{
		stateBits |= ( GLS_POLYMODE_LINE );
		GL_State( stateBits );
		qglEnable( GL_POLYGON_OFFSET_LINE );
		polygonState = GL_TRUE;
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}*/

	qglVertexPointer( 3, GL_FLOAT, sizeof( input->xyz[0] ), input->xyz );

	if ( qglLockArraysEXT ) {
		qglLockArraysEXT( 0, input->numVertexes );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	if ( qglUnlockArraysEXT ) {
		qglUnlockArraysEXT();
	}

	qglEnable( GL_TEXTURE_2D );

	if( didDepth )
		qglDepthRange( 0, 1 );
	if ( polygonState )
		qglDisable( GL_POLYGON_OFFSET_LINE );
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals( const shaderCommands_t *input ) {
	int		i;

	GL_ClientState( 0, CLS_NONE );

	qglDisable( GL_TEXTURE_2D );

	qglDepthRange( 0, 0 );	// never occluded

	GL_State( GLS_DEPTHMASK_TRUE );

	// ydnar: light direction
	if ( r_shownormals->integer == 2 ) {
		const trRefEntity_t *ent = backEnd.currentEntity;
		vec3_t temp2;

		if ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) {
			VectorSubtract( ent->e.lightingOrigin, backEnd.orientation.origin, temp2 );
		} else {
			VectorClear( temp2 );
		}
		tess.xyz[0][0] = DotProduct( temp2, backEnd.orientation.axis[ 0 ] );
        tess.xyz[0][1] = DotProduct( temp2, backEnd.orientation.axis[ 1 ] );
        tess.xyz[0][2] = DotProduct( temp2, backEnd.orientation.axis[ 2 ] );
        VectorMA( tess.xyz[0], 32, ent->lightDir, tess.xyz[1] );
        tess.numVertexes = 2;

        tess.indexes[0] = 0;
        tess.indexes[1] = 1;

        qglPointSize( 5 );
        qglColor4f( ent->ambientLight[ 0 ] / 255.0, ent->ambientLight[ 1 ] / 255.0, ent->ambientLight[ 2 ] / 255.0, 1 );
        qglVertexPointer( 3, GL_FLOAT, sizeof( tess.xyz[0] ), tess.xyz );
        tess.numIndexes = 1;
        qglDrawElements( GL_POINTS, tess.numIndexes, GL_INDEX_TYPE, tess.indexes );
        qglPointSize( 1 );
        
        if ( fabs( VectorLengthSquared( ent->lightDir ) - 1.0f ) > 0.2f ) {
            qglColor4f( 1, 0, 0, 1 );
        } else {
            qglColor4f( ent->directedLight[ 0 ] / 255.0, ent->directedLight[ 1 ] / 255.0, ent->directedLight[ 2 ] / 255.0, 1 );
        }

        qglLineWidth( 3 );
        tess.numIndexes = 2;
        qglDrawElements( GL_LINES, tess.numIndexes, GL_INDEX_TYPE, tess.indexes );
        qglLineWidth( 1 );
	}
	// ydnar: normals drawing
	else
	{
		for ( i = tess.numVertexes-1; i >= 0; i-- ) {
			VectorMA( tess.xyz[i], 2.0, tess.normal[i], tess.xyz[i*2 + 1] );
			VectorCopy( tess.xyz[i], tess.xyz[i*2] );
		}

		qglColor4f( 1, 1, 1, 1 );

		qglVertexPointer( 3, GL_FLOAT, sizeof( tess.xyz[0] ), tess.xyz );

		if ( qglLockArraysEXT ) {
			qglLockArraysEXT( 0, tess.numVertexes * 2 );
		}

		qglDrawArrays( GL_LINES, 0, tess.numVertexes * 2 );

		if ( qglUnlockArraysEXT ) {
			qglUnlockArraysEXT();
		}
	}

	qglEnable( GL_TEXTURE_2D );

	qglDepthRange( 0, 1 );
}


/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum ) {

	shader_t *state;
	
#ifdef USE_PMLIGHT
	if ( !tess.dlightPass && shader->isStaticShader && !shader->remappedShader )
#else
	if ( shader->isStaticShader )
#endif
		tess.allowVBO = qtrue;
	else
		tess.allowVBO = qfalse;

	if ( shader->remappedShader ) {
		state = shader->remappedShader;
	} else {
		state = shader;
	}

#ifdef USE_PMLIGHT
	if ( tess.fogNum != fogNum || tess.cullType != state->cullType ) {
		tess.dlightUpdateParams = qtrue;
	}
#endif

#ifdef USE_TESS_NEEDS_NORMAL
#ifdef USE_PMLIGHT
	tess.needsNormal = state->needsNormal || tess.dlightPass || r_shownormals->integer;
#else
	tess.needsNormal = state->needsNormal || r_shownormals->integer;
#endif
#endif

#ifdef USE_TESS_NEEDS_ST2
	tess.needsST2 = state->needsST2;
#endif

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.fogNum = fogNum;

#ifdef USE_LEGACY_DLIGHTS
	tess.dlightBits = 0;		// will be OR'd in by surface functions
#endif
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if ( tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime ) {
		tess.shaderTime = tess.shader->clampTime;
	}
}


/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
static void DrawMultitextured( const shaderCommands_t *input, int stage ) {
	const shaderStage_t *pStage;

	pStage = tess.xstages[ stage ];

	// Ridah
	if ( tess.shader->noFog && pStage->isFogged ) {
		R_FogOn();
	} else if ( tess.shader->noFog && !pStage->isFogged ) {
		R_FogOff(); // turn it back off
	} else {    // make sure it's on
		R_FogOn();
	}
	// done.

	GL_State( pStage->stateBits );

	if ( !setArraysOnce ) {
		R_ComputeColors( pStage );
		R_ComputeTexCoords( 0, &pStage->bundle[0] );
		R_ComputeTexCoords( 1, &pStage->bundle[1] );
		GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

		qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoordPtr[0] );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors[0].rgba );

		GL_ClientState( 1, CLS_TEXCOORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoordPtr[1] );
	}

	//
	// base
	//
	GL_SelectTexture( 0 );
	R_BindAnimatedImage( &pStage->bundle[0] );

	//
	// lightmap/secondary pass
	//
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	R_BindAnimatedImage( &pStage->bundle[1] );

	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( pStage->mtEnv );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
#ifdef USE_VBO
	if ( r_vbo->integer ) {
		// some drivers may try to load texcoord[1] data even with multi-texturing disabled
		// (and actually gpu shaders doesn't care about conventional GL_TEXTURE_2D states)
		// which doesn't cause problems while data pointer is the same or represents fixed-size set
		// but when we switch to/from vbo - texcoord[1] data may point on larger set (it's ok)
		// or smaller set - which will cause out-of-bounds index access/crash during non-multitexture rendering
		// GL_ClientState( 1, GLS_NONE );
	}
#endif // USE_VBO

	qglDisable( GL_TEXTURE_2D );

	GL_SelectTexture( 0 );
}


#ifdef USE_LEGACY_DLIGHTS
/*
DynamicLightSinglePass()
perform all dynamic lighting with a single rendering pass
*/

static void DynamicLightSinglePass( void ) {
	int i, l, a, b, c, color, *intColors;
	vec3_t origin;
	byte        *colors;
	glIndex_t hitIndexes[ SHADER_MAX_INDEXES ];
	int numIndexes;
	float radius, radiusInverseCubed;
	float intensity, remainder, modulate;
	vec3_t floatColor, dir;
	const dlight_t    *dl;

	// early out
	if ( backEnd.refdef.num_dlights == 0 ) {
		return;
	}

	// clear colors
	Com_Memset( tess.svars.colors, 0, sizeof( tess.svars.colors ) );

	// walk light list
	for ( l = 0; l < backEnd.refdef.num_dlights; l++ )
	{
		// early out
		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;
		}

		// setup
		dl = &backEnd.refdef.dlights[ l ];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		radiusInverseCubed = dl->radiusInverseCubed;
		intensity = dl->intensity;
		floatColor[ 0 ] = dl->color[ 0 ] * 255.0f;
		floatColor[ 1 ] = dl->color[ 1 ] * 255.0f;
		floatColor[ 2 ] = dl->color[ 2 ] * 255.0f;

		// directional lights have max intensity and washout remainder intensity
		if ( dl->flags & REF_DIRECTED_DLIGHT ) {
			remainder = intensity * 0.125;
		} else {
			remainder = 0.0f;
		}

		// illuminate vertexes
		colors = tess.svars.colors[ 0 ].rgba;
		for ( i = 0; i < tess.numVertexes; i++, colors += 4 )
		{
			backEnd.pc.c_dlightVertexes++;

			// directional dlight, origin is a directional normal
			if ( dl->flags & REF_DIRECTED_DLIGHT ) {
				// twosided surfaces use absolute value of the calculated lighting
				modulate = intensity * DotProduct( dl->origin, tess.normal[ i ] );
				if ( tess.shader->cullType == CT_TWO_SIDED ) {
					modulate = fabs( modulate );
				}
				modulate += remainder;
			}
			// ball dlight
			else
			{
				dir[ 0 ] = radius - fabs( origin[ 0 ] - tess.xyz[ i ][ 0 ] );
				if ( dir[ 0 ] <= 0.0f ) {
					continue;
				}
				dir[ 1 ] = radius - fabs( origin[ 1 ] - tess.xyz[ i ][ 1 ] );
				if ( dir[ 1 ] <= 0.0f ) {
					continue;
				}
				dir[ 2 ] = radius - fabs( origin[ 2 ] - tess.xyz[ i ][ 2 ] );
				if ( dir[ 2 ] <= 0.0f ) {
					continue;
				}

				modulate = intensity * dir[ 0 ] * dir[ 1 ] * dir[ 2 ] * radiusInverseCubed;
			}

			// optimizations
			if ( modulate < ( 1.0f / 128.0f ) ) {
				continue;
			} else if ( modulate > 1.0f ) {
				modulate = 1.0f;
			}

			// add to color
			color = colors[ 0 ] + Q_ftol( floatColor[ 0 ] * modulate );
			colors[ 0 ] = color > 255 ? 255 : color;
			color = colors[ 1 ] + Q_ftol( floatColor[ 1 ] * modulate );
			colors[ 1 ] = color > 255 ? 255 : color;
			color = colors[ 2 ] + Q_ftol( floatColor[ 2 ] * modulate );
			colors[ 2 ] = color > 255 ? 255 : color;
		}
	}

	// build a list of triangles that need light
	intColors = (int*) tess.svars.colors;
	numIndexes = 0;
	for ( i = 0; i < tess.numIndexes; i += 3 )
	{
		a = tess.indexes[ i ];
		b = tess.indexes[ i + 1 ];
		c = tess.indexes[ i + 2 ];
		if ( !( intColors[ a ] | intColors[ b ] | intColors[ c ] ) ) {
			continue;
		}
		hitIndexes[ numIndexes++ ] = a;
		hitIndexes[ numIndexes++ ] = b;
		hitIndexes[ numIndexes++ ] = c;
	}

	if ( numIndexes == 0 ) {
		return;
	}

	// debug code
	//%	for( i = 0; i < numIndexes; i++ )
	//%		intColors[ hitIndexes[ i ] ] = 0x000000FF;

	GL_ClientState( 1, CLS_NONE );
	GL_ClientState( 0, CLS_COLOR_ARRAY );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );

	// render the dynamic light pass
	R_FogOff();
	GL_Bind( tr.whiteImage );
	GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
	R_DrawElements( numIndexes, hitIndexes );
	backEnd.pc.c_totalIndexes += numIndexes;
	backEnd.pc.c_dlightIndexes += numIndexes;
	R_FogOn();
}



/*
DynamicLightPass()
perform dynamic lighting with multiple rendering passes
*/

static void DynamicLightPass( void ) {
	int i, l, a, b, c, color, *intColors;
	vec3_t origin;
	byte        *colors;
	glIndex_t hitIndexes[ SHADER_MAX_INDEXES ];
	int numIndexes;
	float radius, radiusInverseCubed;
	float intensity, remainder, modulate;
	vec3_t floatColor, dir;
	const dlight_t    *dl;

	// early out
	if ( backEnd.refdef.num_dlights == 0 ) {
		return;
	}

	// walk light list
	for ( l = 0; l < backEnd.refdef.num_dlights; l++ )
	{
		// early out
		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;
		}

		// clear colors
		Com_Memset( tess.svars.colors, 0, sizeof( tess.svars.colors ) );

		// setup
		dl = &backEnd.refdef.dlights[ l ];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		radiusInverseCubed = dl->radiusInverseCubed;
		intensity = dl->intensity;
		floatColor[ 0 ] = dl->color[ 0 ] * 255.0f;
		floatColor[ 1 ] = dl->color[ 1 ] * 255.0f;
		floatColor[ 2 ] = dl->color[ 2 ] * 255.0f;

		// directional lights have max intensity and washout remainder intensity
		if ( dl->flags & REF_DIRECTED_DLIGHT ) {
			remainder = intensity * 0.125;
		} else {
			remainder = 0.0f;
		}

		// illuminate vertexes
		colors = tess.svars.colors[ 0 ].rgba;
		for ( i = 0; i < tess.numVertexes; i++, colors += 4 )
		{
			backEnd.pc.c_dlightVertexes++;

			// directional dlight, origin is a directional normal
			if ( dl->flags & REF_DIRECTED_DLIGHT ) {
				// twosided surfaces use absolute value of the calculated lighting
				modulate = intensity * DotProduct( dl->origin, tess.normal[ i ] );
				if ( tess.shader->cullType == CT_TWO_SIDED ) {
					modulate = fabs( modulate );
				}
				modulate += remainder;
			}
			// ball dlight
			else
			{
				dir[ 0 ] = radius - fabs( origin[ 0 ] - tess.xyz[ i ][ 0 ] );
				if ( dir[ 0 ] <= 0.0f ) {
					continue;
				}
				dir[ 1 ] = radius - fabs( origin[ 1 ] - tess.xyz[ i ][ 1 ] );
				if ( dir[ 1 ] <= 0.0f ) {
					continue;
				}
				dir[ 2 ] = radius - fabs( origin[ 2 ] - tess.xyz[ i ][ 2 ] );
				if ( dir[ 2 ] <= 0.0f ) {
					continue;
				}

				modulate = intensity * dir[ 0 ] * dir[ 1 ] * dir[ 2 ] * radiusInverseCubed;
			}

			// optimizations
			if ( modulate < ( 1.0f / 128.0f ) ) {
				continue;
			} else if ( modulate > 1.0f ) {
				modulate = 1.0f;
			}

			// set color
			color = Q_ftol( floatColor[ 0 ] * modulate );
			colors[ 0 ] = color > 255 ? 255 : color;
			color = Q_ftol( floatColor[ 1 ] * modulate );
			colors[ 1 ] = color > 255 ? 255 : color;
			color = Q_ftol( floatColor[ 2 ] * modulate );
			colors[ 2 ] = color > 255 ? 255 : color;
		}

		// build a list of triangles that need light
		intColors = (int*) tess.svars.colors;
		numIndexes = 0;
		for ( i = 0; i < tess.numIndexes; i += 3 )
		{
			a = tess.indexes[ i ];
			b = tess.indexes[ i + 1 ];
			c = tess.indexes[ i + 2 ];
			if ( !( intColors[ a ] | intColors[ b ] | intColors[ c ] ) ) {
				continue;
			}
			hitIndexes[ numIndexes++ ] = a;
			hitIndexes[ numIndexes++ ] = b;
			hitIndexes[ numIndexes++ ] = c;
		}

		if ( numIndexes == 0 ) {
			continue;
		}

		// debug code (fixme, there's a bug in this function!)
		//%	for( i = 0; i < numIndexes; i++ )
		//%		intColors[ hitIndexes[ i ] ] = 0x000000FF;

		GL_ClientState( 1, CLS_NONE );
		GL_ClientState( 0, CLS_COLOR_ARRAY );

		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );

		R_FogOff();
		GL_Bind( tr.whiteImage );
		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );

		R_DrawElements( numIndexes, hitIndexes );

		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
		R_FogOn();
	}
}
#endif // USE_LEGACY_DLIGHTS


/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( void ) {
	const fog_t	*fog;
	int			i;

	// no fog pass in snooper
	if ( (tr.refdef.rdflags & RDF_SNOOPERVIEW) || tess.shader->noFog || !r_wolffog->integer ) {
		return;
	}

	// ydnar: no world, no fogging
	if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	fog = tr.world->fogs + tess.fogNum;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		tess.svars.colors[i] = fog->shader->fogParms.colorInt;
	}

	RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[0] );

	GL_ClientState( 1, CLS_NONE );
	GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors[0].rgba );
	qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );

	GL_SelectTexture( 0 );
	GL_Bind( tr.fogImage );

	if ( tess.shader->fogPass == FP_EQUAL ) {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

	R_DrawElements( tess.numIndexes, tess.indexes );
}


/*
===============
R_ComputeColors
===============
*/
void R_ComputeColors( const shaderStage_t *pStage )
{
	int		i;

	if ( tess.numVertexes == 0 )
		return;

	//
	// rgbGen
	//
	switch ( pStage->rgbGen )
	{
	case CGEN_IDENTITY:
		memset( tess.svars.colors, 0xff, tess.numVertexes * 4 );
		break;
	default:
	case CGEN_IDENTITY_LIGHTING:
		memset( tess.svars.colors, tr.identityLightByte, tess.numVertexes * 4 );
		break;
	case CGEN_LIGHTING_DIFFUSE:
		RB_CalcDiffuseColor( ( unsigned char * ) tess.svars.colors );
		break;
	case CGEN_EXACT_VERTEX:
		memcpy( tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
		break;
	case CGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			tess.svars.colors[i] = pStage->constantColor;
		}
		break;
	case CGEN_VERTEX:
		if ( tr.identityLight == 1 ) {
			memcpy( tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
		} else
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i].rgba[0] = tess.vertexColors[i].rgba[0] * tr.identityLight;
				tess.svars.colors[i].rgba[1] = tess.vertexColors[i].rgba[1] * tr.identityLight;
				tess.svars.colors[i].rgba[2] = tess.vertexColors[i].rgba[2] * tr.identityLight;
				tess.svars.colors[i].rgba[3] = tess.vertexColors[i].rgba[3];
			}
		}
		break;
	case CGEN_ONE_MINUS_VERTEX:
		if ( tr.identityLight == 1 ) {
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i].rgba[0] = 255 - tess.vertexColors[i].rgba[0];
				tess.svars.colors[i].rgba[1] = 255 - tess.vertexColors[i].rgba[1];
				tess.svars.colors[i].rgba[2] = 255 - tess.vertexColors[i].rgba[2];
			}
		} else
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i].rgba[0] = ( 255 - tess.vertexColors[i].rgba[0] ) * tr.identityLight;
				tess.svars.colors[i].rgba[1] = ( 255 - tess.vertexColors[i].rgba[1] ) * tr.identityLight;
				tess.svars.colors[i].rgba[2] = ( 255 - tess.vertexColors[i].rgba[2] ) * tr.identityLight;
			}
		}
		break;
	case CGEN_FOG:
	{
		const fog_t *fog;

		fog = tr.world->fogs + tess.fogNum;

		for ( i = 0; i < tess.numVertexes; i++ ) {
			tess.svars.colors[i] = fog->shader->fogParms.colorInt;
		}
	}
	break;
	case CGEN_WAVEFORM:
		RB_CalcWaveColor( &pStage->rgbWave, tess.svars.colors[0].rgba );
		break;
	case CGEN_ENTITY:
		RB_CalcColorFromEntity( tess.svars.colors[0].rgba );
		break;
	case CGEN_ONE_MINUS_ENTITY:
		RB_CalcColorFromOneMinusEntity( tess.svars.colors[0].rgba );
		break;
	}

	//
	// alphaGen
	//
	switch ( pStage->alphaGen )
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if ( ( pStage->rgbGen == CGEN_VERTEX && tr.identityLight != 1 ) ||
			 pStage->rgbGen != CGEN_VERTEX ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				tess.svars.colors[i].rgba[3] = 255;
			}
		}
		break;
	case AGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			tess.svars.colors[i].rgba[3] = pStage->constantColor.rgba[3];
		}
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &pStage->alphaWave, tess.svars.colors[0].rgba );
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( tess.svars.colors[0].rgba );
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity( tess.svars.colors[0].rgba );
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( tess.svars.colors[0].rgba );
		break;
		// Ridah
	case AGEN_NORMALZFADE:
		RB_CalcNormalZFade( pStage->constantColor.rgba[3], pStage->zFadeBounds, tess.svars.colors[0].rgba );
		break;
		// done.
	case AGEN_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			tess.svars.colors[i].rgba[3] = tess.vertexColors[i].rgba[3];
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i].rgba[3] = 255 - tess.vertexColors[i].rgba[3];
		}
		break;
	case AGEN_PORTAL:
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				unsigned char alpha;
				float len;
				vec3_t v;

				VectorSubtract( tess.xyz[i], backEnd.viewParms.orientation.origin, v );
				len = VectorLength( v ) * tess.shader->portalRangeR;

				if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				tess.svars.colors[i].rgba[3] = alpha;
			}
		}
		break;
	}

	//
	// fog adjustment for colors to fade out as fog increases
	//
	if ( tess.fogNum && !tess.shader->noFog ) {
		switch ( pStage->adjustColorsForFog )
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( tess.svars.colors[0].rgba );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( tess.svars.colors[0].rgba );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( tess.svars.colors[0].rgba );
			break;
		case ACFF_NONE:
			break;
		}
	}
}


/*
===============
R_ComputeTexCoords
===============
*/
void R_ComputeTexCoords( const int b, const textureBundle_t *bundle ) {
	int	i;
	int tm;
	vec2_t *src, *dst;

	if ( !tess.numVertexes )
		return;

	src = dst = tess.svars.texcoords[b];

	//
	// generate the texture coordinates
	//
	switch ( bundle->tcGen )
	{
	case TCGEN_IDENTITY:
		src = tess.texCoords00;
		break;
	case TCGEN_TEXTURE:
		src = tess.texCoords[0];
		break;
	case TCGEN_LIGHTMAP:
		src = tess.texCoords[1];
		break;
	case TCGEN_VECTOR:
		for ( i = 0 ; i < tess.numVertexes ; i++ ) {
			dst[i][0] = DotProduct( tess.xyz[i], bundle->tcGenVectors[0] );
			dst[i][1] = DotProduct( tess.xyz[i], bundle->tcGenVectors[1] );
		}
		break;
	case TCGEN_FOG:
		RB_CalcFogTexCoords( ( float * ) dst );
		break;
	case TCGEN_ENVIRONMENT_MAPPED:
		RB_CalcEnvironmentTexCoords( ( float * ) dst );
		break;
	case TCGEN_ENVIRONMENT_MAPPED_FP:
		RB_CalcEnvironmentTexCoordsFP( ( float * ) dst, bundle->isScreenMap );
		break;
	case TCGEN_FIRERISEENV_MAPPED:
		RB_CalcFireRiseEnvTexCoords( ( float * ) dst );
		break;
	case TCGEN_BAD:
		return;
	}

	//
	// alter texture coordinates
	//
	for ( tm = 0; tm < bundle->numTexMods ; tm++ ) {
		switch ( bundle->texMods[tm].type )
		{
		case TMOD_NONE:
			tm = TR_MAX_TEXMODS; // break out of for loop
			break;

		case TMOD_SWAP:
			RB_CalcSwapTexCoords( (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_TURBULENT:
			RB_CalcTurbulentTexCoords( &bundle->texMods[tm].wave, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_ENTITY_TRANSLATE:
			RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_SCROLL:
			RB_CalcScrollTexCoords( bundle->texMods[tm].scroll, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_SCALE:
			RB_CalcScaleTexCoords( bundle->texMods[tm].scale, (float *) src, (float *) dst );
			src = dst;
			break;

		case TMOD_STRETCH:
			RB_CalcStretchTexCoords( &bundle->texMods[tm].wave, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_TRANSFORM:
			RB_CalcTransformTexCoords( &bundle->texMods[tm], (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_ROTATE:
			RB_CalcRotateTexCoords( bundle->texMods[tm].rotateSpeed, (float *) src, (float *) dst );
			src = dst;
			break;

		default:
			ri.Error( ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'", bundle->texMods[tm].type, tess.shader->name );
			break;
		}
	}


	tess.svars.texcoordPtr[ b ] = src;
}


extern void R_Fog( glfog_t *curfog );

/*
==============
SetIteratorFog
	set the fog parameters for this pass
==============
*/
void SetIteratorFog( void ) {
	// changed for problem when you start the game with r_fastsky set to '1'
//	if(r_fastsky->integer || backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
	if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
		R_FogOff();
		return;
	}

	if ( backEnd.refdef.rdflags & RDF_DRAWINGSKY ) {
		if ( glfogsettings[FOG_SKY].registered ) {
			R_Fog( &glfogsettings[FOG_SKY] );
		} else {
			R_FogOff();
		}

		return;
	}

	if ( skyboxportal && backEnd.refdef.rdflags & RDF_SKYBOXPORTAL ) {
		if ( glfogsettings[FOG_PORTALVIEW].registered ) {
			R_Fog( &glfogsettings[FOG_PORTALVIEW] );
		} else {
			R_FogOff();
		}
	} else {
		if ( glfogNum > FOG_NONE ) {
			R_Fog( &glfogsettings[FOG_CURRENT] );
		} else {
			R_FogOff();
		}
	}
}


/*
** RB_IterateStagesGeneric
*/
static void RB_IterateStagesGeneric( const shaderCommands_t *input )
{
	const shaderStage_t *pStage;
	int stage;

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		pStage = tess.xstages[ stage ];
		if ( !pStage )
			break;

		//
		// do multitexture
		//
		if ( pStage->mtEnv )
		{
			DrawMultitextured( input, stage );
		}
		else
		{
			int fadeStart, fadeEnd;

			if ( !setArraysOnce )
			{
				R_ComputeTexCoords( 0, &pStage->bundle[0] );
				R_ComputeColors( pStage );

				GL_ClientState( 1, CLS_NONE );
				GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

				qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoordPtr[0] );
				qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors[0].rgba );
			}

			//
			// set state
			//
			R_BindAnimatedImage( &pStage->bundle[0] );

			// Ridah, per stage fogging (detail textures)
			if ( tess.shader->noFog && pStage->isFogged ) {
				R_FogOn();
			} else if ( tess.shader->noFog && !pStage->isFogged ) {
				R_FogOff(); // turn it back off
			} else {    // make sure it's on
				R_FogOn();
			}
			// done.

			//----(SA)	fading model stuff
			fadeStart = backEnd.currentEntity->e.fadeStartTime;

			if ( fadeStart ) {
				fadeEnd = backEnd.currentEntity->e.fadeEndTime;
				if ( fadeStart > tr.refdef.time ) {       // has not started to fade yet
					GL_State( pStage->stateBits );
				} else
				{
					int i;
					GLbitfield tempState;
					float alphaval;

					if ( fadeEnd < tr.refdef.time ) {     // entity faded out completely
						continue;
					}

					alphaval = (float)( fadeEnd - tr.refdef.time ) / (float)( fadeEnd - fadeStart );

					tempState = pStage->stateBits;
					// remove the current blend, and don't write to Z buffer
					tempState &= ~( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS | GLS_DEPTHMASK_TRUE );
					// set the blend to src_alpha, dst_one_minus_src_alpha
					tempState |= ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
					GL_State( tempState );
					GL_Cull( CT_FRONT_SIDED );
					// modulate the alpha component of each vertex in the render list
					for ( i = 0; i < tess.numVertexes; i++ ) {
						tess.svars.colors[i].rgba[0] *= alphaval;
						tess.svars.colors[i].rgba[1] *= alphaval;
						tess.svars.colors[i].rgba[2] *= alphaval;
						tess.svars.colors[i].rgba[3] *= alphaval;
					}
				}
			}
			//----(SA)	end
			// ydnar: lightmap stages should be GL_ONE GL_ZERO so they can be seen
			else if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap ) ) {
				GLbitfield stateBits;


				stateBits = ( pStage->stateBits & ~( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) |
							( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
				GL_State( stateBits );
			} else {
				GL_State( pStage->stateBits );
			}

			//
			// draw
			//
			R_DrawElements( input->numIndexes, input->indexes );
			if ( pStage->depthFragment )
			{
				GL_State( pStage->stateBits | GLS_DEPTHMASK_TRUE );
				GL_ProgramEnable();
				R_DrawElements( input->numIndexes, input->indexes );
				GL_ProgramDisable();
			}
		}

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap ) )
			break;
	}
}


/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	const shaderCommands_t *input;
	shader_t		*shader;

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( R_GetDlightMode() ) 
#endif
	{
		if ( tess.dlightPass ) 
		{
			ARB_LightingPass();
			return;
		}

		GL_ProgramDisable();
	}
#endif // USE_PMLIGHT

#ifdef USE_VBO
	if ( tess.vboIndex )
	{
		RB_StageIteratorVBO();
		return;
	}

	VBO_UnBind();
#endif

	input = &tess;
	shader = input->shader;

	RB_DeformTessGeometry();

	//
	// set GL fog
	//
	SetIteratorFog();

	//
	// set face culling appropriately
	//
	GL_Cull( shader->cullType );

	// set polygon offset if necessary
	if ( shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	//
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	if ( tess.numPasses > 1 )
	{
		setArraysOnce = qfalse;

		GL_ClientState( 1, CLS_NONE );
		GL_ClientState( 0, CLS_NONE );
	}
	else
	{
		// FIXME: we can't do that if going to lighting/fog later?
		setArraysOnce = qtrue;

		GL_ClientState( 0, CLS_COLOR_ARRAY | CLS_TEXCOORD_ARRAY );

		if ( tess.xstages[0] )
		{
			R_ComputeColors( tess.xstages[0] );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );
			R_ComputeTexCoords( 0, &tess.xstages[0]->bundle[0] );
			qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoordPtr[0] );
			if ( shader->multitextureEnv )
			{
				GL_ClientState( 1, CLS_TEXCOORD_ARRAY );
				R_ComputeTexCoords( 1, &tess.xstages[0]->bundle[1] );
				qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoordPtr[1] );
			}
			else
			{
				GL_ClientState( 1, CLS_NONE );
			}
		}
	}

	qglVertexPointer( 3, GL_FLOAT, sizeof( input->xyz[0] ), input->xyz ); // padded for SIMD

	//
	// lock XYZ
	//
	if ( qglLockArraysEXT )
	{
		qglLockArraysEXT( 0, input->numVertexes );
	}

	//
	// call shader function
	//
	RB_IterateStagesGeneric( input );

	//
	// now do any dynamic lighting needed
	//
	//%	tess.dlightBits = 255;	// HACK!
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
	if ( !R_GetDlightMode() )
#endif	
	//%	if( tess.dlightBits && tess.shader->sort <= SS_OPAQUE &&
	if ( tess.dlightBits && tess.shader->fogPass &&
		 !( tess.shader->surfaceFlags & ( SURF_NODLIGHT | SURF_SKY ) ) ) {
		if ( r_dynamiclight->integer == 2 ) {
			DynamicLightPass();
		} else {
			DynamicLightSinglePass();
		}
	}
#endif // USE_LEGACY_DLIGHTS

	//
	// now do fog
	//
	if ( tess.fogNum && tess.shader->fogPass )
	{
		RB_FogPass();
	}

	//
	// unlock arrays
	//
	if ( qglUnlockArraysEXT )
	{
		qglUnlockArraysEXT();
	}

	GL_ClientState( 1, CLS_NONE );

	//
	// reset polygon offset
	//
	if ( shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
}


/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	const shaderCommands_t *input;

	input = &tess;

	if ( input->numIndexes == 0 ) {
#ifdef USE_VBO
		VBO_UnBind();
#endif
		return;
	}

	if ( input->numIndexes > SHADER_MAX_INDEXES ) {
		ri.Error( ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit" );
	}

	if ( input->numVertexes > SHADER_MAX_VERTEXES ) {
		ri.Error( ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit" );
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort && !backEnd.doneSurfaces ) {
#ifdef USE_VBO
		VBO_UnBind();
#endif
		return;
	}

	//
	// update performance counters
	//
#ifdef USE_PMLIGHT
	if ( tess.dlightPass ) {
		backEnd.pc.c_lit_batches++;
		backEnd.pc.c_lit_vertices += tess.numVertexes;
		backEnd.pc.c_lit_indices += tess.numIndexes;
	} else
#endif
	{
		backEnd.pc.c_shaders++;
		backEnd.pc.c_vertexes += tess.numVertexes;
		backEnd.pc.c_indexes += tess.numIndexes;
	}
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	//
	// call off to shader specific tess end function
	//
	tess.shader->optimalStageIteratorFunc();

	//
	// draw debugging stuff
	//
#ifdef USE_VBO
	if ( !VBO_Active() ) {
#else
	{
#endif
		if ( r_showtris->integer ) {
			DrawTris( input );
		}
		if ( r_shownormals->integer ) {
			DrawNormals( input );
		}
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.numVertexes = 0;
}
