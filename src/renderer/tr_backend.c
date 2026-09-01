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

backEndData_t	*backEndData;
backEndState_t	backEnd;

const float *GL_Ortho( const float left, const float right, const float bottom, const float top, const float znear, const float zfar )
{
	static float m[ 16 ] = { 0 };

	m[0] = 2.0f / (right - left);
	m[5] = 2.0f / (top - bottom);
	m[10] = - 2.0f / (zfar - znear);
	m[12] = - (right + left)/(right - left);
	m[13] = - (top + bottom) / (top - bottom);
	m[14] = - (zfar + znear) / (zfar - znear);
	m[15] = 1.0f;

	return m;
}


/*
** GL_Bind
*/
void GL_Bind( image_t *image ) {
	GLuint texnum;

	if ( !image ) {
		ri.Printf( PRINT_WARNING, "GL_Bind: NULL image\n" );
		texnum = tr.defaultImage->texnum;
	} else {
		texnum = image->texnum;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		if ( image ) {
			image->frameUsed = tr.frameCount;
		}
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
	}
}


/*
** GL_BindTexNum
*/
void GL_BindTexNum( GLuint texnum ) {

	if ( r_nobind->integer && tr.dlightImage ) {	// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[ glState.currenttmu ] != texnum ) {
		glState.currenttextures[ glState.currenttmu ] = texnum;
		qglBindTexture( GL_TEXTURE_2D, texnum );
	}
}


/*
** GL_SelectTexture
*/
void GL_SelectTexture( int unit )
{
	if ( glState.currenttmu == unit )
	{
		return;
	}

	if ( unit >= glConfig.numTextureUnits )
	{
		ri.Error( ERR_DROP, "GL_SelectTexture: unit = %i", unit );
	}

	qglActiveTextureARB( GL_TEXTURE0_ARB + unit );

	glState.currenttmu = unit;
}


/*
** GL_SelectClientTexture
*/
static void GL_SelectClientTexture( int unit )
{
	if ( glState.currentArray == unit )
	{
		return;
	}

	if ( unit >= glConfig.numTextureUnits )
	{
		ri.Error( ERR_DROP, "GL_SelectClientTexture: unit = %i", unit );
	}

	qglClientActiveTextureARB( GL_TEXTURE0_ARB + unit );

	glState.currentArray = unit;
}


/*
** GL_BindTexture
*/
void GL_BindTexture( int unit, GLuint texnum )
{
	if ( glState.currenttextures[ unit ] != texnum )
	{
		GL_SelectTexture( unit );
		glState.currenttextures[ unit ] = texnum;
		qglBindTexture( GL_TEXTURE_2D, texnum );
	}
}


/*
** GL_Cull
*/
void GL_Cull( cullType_t cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;

	if ( cullType == CT_TWO_SIDED )
	{
		qglDisable( GL_CULL_FACE );
	}
	else
	{
		qboolean cullFront;
		qglEnable( GL_CULL_FACE );

		cullFront = (cullType == CT_FRONT_SIDED);
		if ( backEnd.viewParms.portalView == PV_MIRROR )
		{
			cullFront = !cullFront;
		}

		qglCullFace( cullFront ? GL_FRONT : GL_BACK );
	}
}


/*
** GL_TexEnv
*/
void GL_TexEnv( GLint env )
{
	if ( env == glState.texEnv[ glState.currenttmu ] )
		return;

	glState.texEnv[ glState.currenttmu ] = env;

	switch ( env )
	{
	case GL_MODULATE:
	case GL_REPLACE:
	case GL_DECAL:
	case GL_ADD:
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env );
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed", env );
		break;
	}
}


/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State( GLbitfield stateBits )
{
	GLbitfield diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
		}
		else
		{
			qglDepthFunc( GL_LEQUAL );
		}
	}

	//
	// check blend bits
	//
	if ( diff & GLS_BLEND_BITS )
	{
		GLenum srcFactor = GL_ONE, dstFactor = GL_ONE;

		if ( stateBits & GLS_BLEND_BITS )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		default:
			ri.Error( ERR_DROP, "GL_State: invalid alpha test bits" );
			break;
		}
	}

	glState.glStateBits = stateBits;
}


void GL_ClientState( int unit, GLbitfield stateBits )
{
	GLbitfield diff = stateBits ^ glState.glClientStateBits[ unit ];

	if ( diff == 0 )
	{
		if ( stateBits )
		{
			GL_SelectClientTexture( unit );
		}
		return;
	}

	GL_SelectClientTexture( unit );

	if ( diff & CLS_COLOR_ARRAY )
	{
		if ( stateBits & CLS_COLOR_ARRAY )
			qglEnableClientState( GL_COLOR_ARRAY );
		else
			qglDisableClientState( GL_COLOR_ARRAY );
	}

	if ( diff & CLS_NORMAL_ARRAY )
	{
		if ( stateBits & CLS_NORMAL_ARRAY )
			qglEnableClientState( GL_NORMAL_ARRAY );
		else
			qglDisableClientState( GL_NORMAL_ARRAY );
	}

	if ( diff & CLS_TEXCOORD_ARRAY )
	{
		if ( stateBits & CLS_TEXCOORD_ARRAY )
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		else
			qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	glState.glClientStateBits[ unit ] = stateBits;
}


void RB_SetGL2D( void );

/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void ) {
	color4ub_t c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	if ( tess.shader != tr.whiteShader ) {
		RB_EndSurface();
		RB_SetGL2D();
		RB_BeginSurface( tr.whiteShader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_SetGL2D();

	c.rgba[0] = c.rgba[1] = c.rgba[2] = (backEnd.refdef.time & 255);
	c.rgba[3] = 255;

	RB_AddQuadStamp2( backEnd.refdef.x, backEnd.refdef.y, backEnd.refdef.width, backEnd.refdef.height,
		0.0, 0.0, 0.0, 0.0, c );

	RB_EndSurface();

	tess.numIndexes = 0;
	tess.numVertexes = 0;

	backEnd.isHyperspace = qtrue;
}


static void SetViewportAndScissor( void ) {
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode( GL_MODELVIEW );

	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.scissorX, backEnd.viewParms.scissorY,
		backEnd.viewParms.scissorWidth, backEnd.viewParms.scissorHeight );
}


/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
static void RB_BeginDrawingView( void ) {
	GLbitfield clearBits = 0;

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		qglFinish();
		glState.finishCalled = qtrue;
	} else if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );


////////// (SA) modified to ensure one glclear() per frame at most

	// clear relevant buffers
	clearBits = 0;

	if ( r_shadows->integer == 2 ) {
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}
//	if(r_uiFullScreen->integer) {
//		clearBits = GL_DEPTH_BUFFER_BIT;	// (SA) always just clear depth for menus
//	}
	// ydnar: global q3 fog volume
	if ( tr.world && tr.world->globalFog >= 0 ) {
		clearBits |= GL_DEPTH_BUFFER_BIT;
		clearBits |= GL_COLOR_BUFFER_BIT;
		//
		qglClearColor( tr.world->fogs[tr.world->globalFog].shader->fogParms.color[ 0 ] * tr.identityLight,
					   tr.world->fogs[tr.world->globalFog].shader->fogParms.color[ 1 ] * tr.identityLight,
					   tr.world->fogs[tr.world->globalFog].shader->fogParms.color[ 2 ] * tr.identityLight, 1.0 );
	}
	else if ( skyboxportal ) {
		if ( backEnd.refdef.rdflags & RDF_SKYBOXPORTAL ) { // portal scene, clear whatever is necessary

			clearBits |= GL_DEPTH_BUFFER_BIT;

			if ( r_fastsky->integer || backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {  // fastsky: clear color

				// try clearing first with the portal sky fog color, then the world fog color, then finally a default
				clearBits |= GL_COLOR_BUFFER_BIT;
				if ( glfogsettings[FOG_PORTALVIEW].registered ) {
					qglClearColor( glfogsettings[FOG_PORTALVIEW].color[0], glfogsettings[FOG_PORTALVIEW].color[1], glfogsettings[FOG_PORTALVIEW].color[2], glfogsettings[FOG_PORTALVIEW].color[3] );
				} else if ( glfogNum > FOG_NONE && glfogsettings[FOG_CURRENT].registered )      {
					qglClearColor( glfogsettings[FOG_CURRENT].color[0], glfogsettings[FOG_CURRENT].color[1], glfogsettings[FOG_CURRENT].color[2], glfogsettings[FOG_CURRENT].color[3] );
				} else {
//					qglClearColor ( 1.0, 0.0, 0.0, 1.0 );	// red clear for testing portal sky clear
					qglClearColor( 0.5, 0.5, 0.5, 1.0 );
				}
			} else {                                                    // rendered sky (either clear color or draw quake sky)
				if ( glfogsettings[FOG_PORTALVIEW].registered ) {
					qglClearColor( glfogsettings[FOG_PORTALVIEW].color[0], glfogsettings[FOG_PORTALVIEW].color[1], glfogsettings[FOG_PORTALVIEW].color[2], glfogsettings[FOG_PORTALVIEW].color[3] );

					if ( glfogsettings[FOG_PORTALVIEW].clearscreen ) {    // portal fog requests a screen clear (distance fog rather than quake sky)
						clearBits |= GL_COLOR_BUFFER_BIT;
					}
				}

			}
		} else {                                        // world scene with portal sky, don't clear any buffers, just set the fog color if there is one

			clearBits |= GL_DEPTH_BUFFER_BIT;   // this will go when I get the portal sky rendering way out in the zbuffer (or not writing to zbuffer at all)

			if ( glfogNum > FOG_NONE && glfogsettings[FOG_CURRENT].registered ) {
				if ( backEnd.refdef.rdflags & RDF_UNDERWATER ) {
					if ( glfogsettings[FOG_CURRENT].mode == GL_LINEAR ) {
						clearBits |= GL_COLOR_BUFFER_BIT;
					}

				} else if ( !( r_portalsky->integer ) ) {    // portal skies have been manually turned off, clear bg color
					clearBits |= GL_COLOR_BUFFER_BIT;
				}

				qglClearColor( glfogsettings[FOG_CURRENT].color[0], glfogsettings[FOG_CURRENT].color[1], glfogsettings[FOG_CURRENT].color[2], glfogsettings[FOG_CURRENT].color[3] );
			} else if ( !( r_portalsky->integer ) ) {      // ydnar: portal skies have been manually turned off, clear bg color
				clearBits |= GL_COLOR_BUFFER_BIT;
				qglClearColor( 0.5, 0.5, 0.5, 1.0 );
			}
		}
	} else {                                              // world scene with no portal sky
		clearBits |= GL_DEPTH_BUFFER_BIT;

		// NERVE - SMF - we don't want to clear the buffer when no world model is specified
		if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
			clearBits &= ~GL_COLOR_BUFFER_BIT;
		}

		// -NERVE - SMF
		else if ( r_fastsky->integer || backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {

			clearBits |= GL_COLOR_BUFFER_BIT;

			if ( glfogsettings[FOG_CURRENT].registered ) { // try to clear fastsky with current fog color
				qglClearColor( glfogsettings[FOG_CURRENT].color[0], glfogsettings[FOG_CURRENT].color[1], glfogsettings[FOG_CURRENT].color[2], glfogsettings[FOG_CURRENT].color[3] );
			} else {
//				qglClearColor ( 0.0, 0.0, 1.0, 1.0 );	// blue clear for testing world sky clear
				qglClearColor( 0.05f, 0.05f, 0.05f, 1.0f );  // JPW NERVE changed per id req was 0.5s
			}
		} else {        // world scene, no portal sky, not fastsky, clear color if fog says to, otherwise, just set the clearcolor
			if ( glfogsettings[FOG_CURRENT].registered ) { // try to clear fastsky with current fog color
				qglClearColor( glfogsettings[FOG_CURRENT].color[0], glfogsettings[FOG_CURRENT].color[1], glfogsettings[FOG_CURRENT].color[2], glfogsettings[FOG_CURRENT].color[3] );

				if ( glfogsettings[FOG_CURRENT].clearscreen ) {   // world fog requests a screen clear (distance fog rather than quake sky)
					clearBits |= GL_COLOR_BUFFER_BIT;
				}
			}
		}
	}

	// ydnar: don't clear the color buffer when no world model is specified
	if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
		clearBits &= ~GL_COLOR_BUFFER_BIT;
	}

	if ( clearBits ) {
		qglClear( clearBits );
	}

//----(SA)	done

	if ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) {
		RB_Hyperspace();
		backEnd.projection2D = qfalse;
		SetViewportAndScissor();
	} else {
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;
}

#ifdef USE_PMLIGHT
static void RB_LightingPass( void );
#endif

/*
==================
RB_RenderDrawSurfList
==================
*/
static void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				fogNum;
	int				entityNum, oldEntityNum;
	int				dlighted;
	qboolean		depthRange, oldDepthRange, isCrosshair, wasCrosshair;
	int				i;
	drawSurf_t		*drawSurf;
	unsigned int	oldSort;
#ifdef USE_PMLIGHT
	float			oldShaderSort;
#endif
	double			originalTime; // -EC-

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
	oldSort = MAX_UINT;
#ifdef USE_PMLIGHT
	oldShaderSort = -1;
#endif
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		if ( drawSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}

		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from separate
		// entities merged into a single batch, like smoke and blood puff sprites
		if ( ( (oldSort ^ drawSurf->sort ) & ~QSORT_REFENTITYNUM_MASK ) || !shader->entityMergable ) {
			if ( oldShader != NULL ) {
				RB_EndSurface();
			}
#ifdef USE_PMLIGHT
			#define INSERT_POINT SS_FOG
			if ( backEnd.refdef.numLitSurfs && oldShaderSort < INSERT_POINT && shader->sort >= INSERT_POINT ) {
				//RB_BeginDrawingLitSurfs(); // no need, already setup in RB_BeginDrawingView()
				if ( depthRange ) {
					qglDepthRange( 0, 1 );
					RB_LightingPass();
					qglDepthRange( 0, 0.3 );
				} else {
					RB_LightingPass();
				}
				oldEntityNum = -1; // force matrix setup
			}
			oldShaderSort = shader->sort;
#endif
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
		}

		oldSort = drawSurf->sort;

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				if ( backEnd.currentEntity->intShaderTime )
					backEnd.refdef.floatTime = originalTime - (double)(backEnd.currentEntity->e.shaderTime.i) * 0.001;
				else
					backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime.f;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.orientation );
				// set up the dynamic lighting if needed
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
				if ( !R_GetDlightMode() )
#endif
				if ( backEnd.currentEntity->needDlights ) {
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.orientation );
				}
#endif // USE_LEGACY_DLIGHTS
				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;

					if(backEnd.currentEntity->e.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.orientation = backEnd.viewParms.world;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
				if ( !R_GetDlightMode() )
#endif
				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.orientation );
#endif // USE_LEGACY_DLIGHTS
			}

			// we have to reset the shaderTime as well otherwise image animations on
			// the world (like water) continue with the wrong frame
			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

			qglLoadMatrixf( backEnd.orientation.modelMatrix );

			//
			// change depthrange. Also change projection matrix so first person weapon does not look like coming
			// out of the screen.
			//
			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								// was not a crosshair but now is, change back proj matrix
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}
				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	}

	// draw the contents of the last shader batch
	if ( oldShader != NULL ) {
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	backEnd.currentEntity	= &tr.worldEntity;
	backEnd.refdef.floatTime = originalTime;
	backEnd.orientation = backEnd.viewParms.world;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
	if ( !R_GetDlightMode() )
#endif
		R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.orientation );
#endif // USE_LEGACY_DLIGHTS
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange(0, 1);
	}
}


#ifdef USE_PMLIGHT
/*
=================
RB_BeginDrawingLitView
=================
*/
static void RB_BeginDrawingLitSurfs( void )
{
	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	glState.faceCulling = -1;		// force face culling to set next time
}


/*
==================
RB_RenderLitSurfList
==================
*/
static void RB_RenderLitSurfList( dlight_t* dl ) {
	shader_t		*shader, *oldShader;
	int				fogNum;
	int				entityNum, oldEntityNum;
	qboolean		depthRange, oldDepthRange, isCrosshair, wasCrosshair;
	const litSurf_t	*litSurf;
	unsigned int	oldSort;
	double			originalTime; // -EC-

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
	oldSort = MAX_UINT;
	depthRange = qfalse;

	tess.dlightUpdateParams = qtrue;

	for ( litSurf = dl->head; litSurf; litSurf = litSurf->next ) {
		//if ( litSurf->sort == sort ) {
		if ( litSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *litSurf->surface ]( litSurf->surface );
			continue;
		}

		R_DecomposeLitSort( litSurf->sort, &entityNum, &shader, &fogNum );

		// anything BEFORE opaque is sky/portal, anything AFTER it should never have been added
		//assert( shader->sort == SS_OPAQUE );
		// !!! but MIRRORS can trip that assert, so just do this for now
		//if ( shader->sort < SS_OPAQUE )
		//	continue;

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from separate
		// entities merged into a single batch, like smoke and blood puff sprites
		if ( ( (oldSort ^ litSurf->sort) & ~QSORT_REFENTITYNUM_MASK ) || !shader->entityMergable ) {
			if ( oldShader != NULL ) {
				RB_EndSurface();
			}
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
		}

		oldSort = litSurf->sort;

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];

				if ( backEnd.currentEntity->intShaderTime )
					backEnd.refdef.floatTime = originalTime - (double)(backEnd.currentEntity->e.shaderTime.i) * 0.001;
				else
					backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime.f;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.orientation );

				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;

					if(backEnd.currentEntity->e.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.orientation = backEnd.viewParms.world;
			}

			// we have to reset the shaderTime as well otherwise image animations on
			// the world (like water) continue with the wrong frame
			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

			// set up the dynamic lighting
			R_TransformDlights( 1, dl, &backEnd.orientation );
			tess.dlightUpdateParams = qtrue;

			qglLoadMatrixf( backEnd.orientation.modelMatrix );

			//
			// change depthrange. Also change projection matrix so first person weapon does not look like coming
			// out of the screen.
			//

			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								// was not a crosshair but now is, change back proj matrix
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}
				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *litSurf->surface ]( litSurf->surface );
	}

	// draw the contents of the last shader batch
	if ( oldShader != NULL ) {
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	backEnd.currentEntity	= &tr.worldEntity;
	backEnd.refdef.floatTime = originalTime;
	backEnd.orientation = backEnd.viewParms.world;
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}
}
#endif // USE_PMLIGHT


/*
============================================================================

RENDER BACK END FUNCTIONS

============================================================================
*/


/*
================
RB_SetGL2D
================
*/
void RB_SetGL2D( void ) {

	if ( backEnd.projection2D ) {
		return;
	}

	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( GL_Ortho( 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1 ) );
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

	GL_State( GLS_DEPTHTEST_DISABLE |
		GLS_SRCBLEND_SRC_ALPHA |
		GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_CLIP_PLANE0 );

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = (double)backEnd.refdef.time * 0.001; // -EC-: cast to double
}


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty ) {
	int			i, j;
	int			start, end;

	if ( !tr.registered ) {
		return;
	}

	start = 0;
	if ( r_speeds->integer ) {
		start = ri.Milliseconds();
	}

	// make sure rows and cols are powers of 2
	for ( i = 0 ; ( 1 << i ) < cols ; i++ ) {
	}
	for ( j = 0 ; ( 1 << j ) < rows ; j++ ) {
	}
	if ( ( 1 << i ) != cols || ( 1 << j ) != rows) {
		ri.Error (ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	RE_UploadCinematic( w, h, cols, rows, data, client, dirty );

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	tr.cinematicShader->stages[0]->bundle[0].image[0] = tr.scratchImage[client];
	RE_StretchPic( x, y, w, h, 0.5f / cols, 0.5f / rows, 1.0f - 0.5f / cols, 1.0f - 0.5 / rows, tr.cinematicShader->index );
}


void RE_UploadCinematic( int w, int h, int cols, int rows, byte *data, int client, qboolean dirty ) {

	image_t *image;

	if ( !tr.scratchImage[ client ] ) {
		tr.scratchImage[ client ] = R_CreateImage( va( "*scratch%i", client ), NULL, data, cols, rows, IMGFLAG_CLAMPTOEDGE | IMGFLAG_RGB | IMGFLAG_NOSCALE );
		return;
	}

	image = tr.scratchImage[ client ];

	GL_Bind( image );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != image->width || rows != image->height ) {
		image->width = image->uploadWidth = cols;
		image->height = image->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, image->internalFormat, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_clamp_mode );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_clamp_mode );
	} else if ( dirty ) {
		// otherwise, just subimage upload it so that drivers can tell we are going to be changing
		// it and don't try and do a texture compression
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
	}
}


/*
=============
RB_SetColor
=============
*/
static const void *RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D.rgba[0] = cmd->color[0] * 255;
	backEnd.color2D.rgba[1] = cmd->color[1] * 255;
	backEnd.color2D.rgba[2] = cmd->color[2] * 255;
	backEnd.color2D.rgba[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}


/*
=============
RB_StretchPic
=============
*/
static const void *RB_StretchPic( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;

	cmd = (const stretchPicCommand_t *)data;

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		RB_EndSurface();
		RB_SetGL2D(); // set correct shader time before RB_BeginSurface() on 3D->2D transition
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_SetGL2D();

#ifdef USE_FBO
	//Check if it's time for BLOOM!
	R_BloomScreen();
#endif

	RB_AddQuadStamp2( cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1, cmd->s2, cmd->t2, backEnd.color2D );

	return (const void *)(cmd + 1);
}


#ifdef USE_PMLIGHT
static void RB_LightingPass( void )
{
	dlight_t	*dl;
	int	i;

#ifdef USE_VBO
	VBO_Flush();

	tess.allowVBO = qfalse; // for now
#endif

	tess.dlightPass = qtrue;

	for ( i = 0; i < backEnd.viewParms.num_dlights; i++ )
	{
		dl = &backEnd.viewParms.dlights[i];
		if ( dl->head )
		{
			tess.light = dl;
			RB_RenderLitSurfList( dl );
		}
	}

	tess.dlightPass = qfalse;

	backEnd.viewParms.num_dlights = 0;
	GL_ProgramDisable();
}
#endif


static const void* RB_Draw2dPolys( const void* data ) {
	const poly2dCommand_t* cmd;
	shader_t *shader;
	int i;

	cmd = (const poly2dCommand_t* )data;

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		RB_EndSurface();
		RB_SetGL2D(); // set correct shader time before RB_BeginSurface() on 3D->2D transition
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_SetGL2D();

#ifdef USE_FBO
	//Check if it's time for BLOOM!
	R_BloomScreen();
#endif

	RB_CHECKOVERFLOW( cmd->numverts, ( cmd->numverts - 2 ) * 3 );

	for ( i = 0; i < cmd->numverts - 2; i++ ) {
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	}

	for ( i = 0; i < cmd->numverts; i++ ) {
		tess.xyz[ tess.numVertexes ][0] = cmd->verts[i].xyz[0];
		tess.xyz[ tess.numVertexes ][1] = cmd->verts[i].xyz[1];
		tess.xyz[ tess.numVertexes ][2] = 0;

		tess.texCoords[0][ tess.numVertexes ][0] = cmd->verts[i].st[0];
		tess.texCoords[0][ tess.numVertexes ][1] = cmd->verts[i].st[1];

		tess.vertexColors[ tess.numVertexes ] = cmd->verts[i].modulate;
		tess.numVertexes++;
	}

	return (const void *)( cmd + 1 );
}


// NERVE - SMF
/*
=============
RB_RotatedPic
=============
*/
static const void *RB_RotatedPic( const void *data ) {
	const stretchPicCommand_t   *cmd;
	shader_t *shader;
	int numVerts, numIndexes;
	float angle;

	cmd = (const stretchPicCommand_t *)data;

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		RB_EndSurface();
		RB_SetGL2D(); // set correct shader time before RB_BeginSurface() on 3D->2D transition
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_SetGL2D();

#ifdef USE_FBO
	//Check if it's time for BLOOM!
	R_BloomScreen();
#endif

	RB_CHECKOVERFLOW( 4, 6 );
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

	tess.vertexColors[numVerts + 0] =
	tess.vertexColors[numVerts + 1] =
	tess.vertexColors[numVerts + 2] =
	tess.vertexColors[numVerts + 3] = backEnd.color2D;

	angle = cmd->angle * M_TAU;
	tess.xyz[ numVerts ][0] = cmd->x + ( cos( angle ) * cmd->w );
	tess.xyz[ numVerts ][1] = cmd->y + ( sin( angle ) * cmd->h );
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[0][ numVerts + 0 ][0] = cmd->s1;
	tess.texCoords[0][ numVerts + 0 ][1] = cmd->t1;

	angle = cmd->angle * M_TAU + 0.25 * M_TAU;
	tess.xyz[ numVerts + 1 ][0] = cmd->x + ( cos( angle ) * cmd->w );
	tess.xyz[ numVerts + 1 ][1] = cmd->y + ( sin( angle ) * cmd->h );
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[0][ numVerts + 1 ][0] = cmd->s2;
	tess.texCoords[0][ numVerts + 1 ][1] = cmd->t1;

	angle = cmd->angle * M_TAU + 0.50 * M_TAU;
	tess.xyz[ numVerts + 2 ][0] = cmd->x + ( cos( angle ) * cmd->w );
	tess.xyz[ numVerts + 2 ][1] = cmd->y + ( sin( angle ) * cmd->h );
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[0][ numVerts + 2 ][0] = cmd->s2;
	tess.texCoords[0][ numVerts + 2 ][1] = cmd->t2;

	angle = cmd->angle * M_TAU + 0.75 * M_TAU;
	tess.xyz[ numVerts + 3 ][0] = cmd->x + ( cos( angle ) * cmd->w );
	tess.xyz[ numVerts + 3 ][1] = cmd->y + ( sin( angle ) * cmd->h );
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[0][ numVerts + 3 ][0] = cmd->s1;
	tess.texCoords[0][ numVerts + 3 ][1] = cmd->t2;

	return (const void *)( cmd + 1 );
}
// -NERVE - SMF

/*
==============
RB_StretchPicGradient
==============
*/
static const void *RB_StretchPicGradient( const void *data ) {
	const stretchPicCommand_t   *cmd;
	shader_t *shader;
	int numVerts, numIndexes;

	cmd = (const stretchPicCommand_t *)data;

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		RB_EndSurface();
		RB_SetGL2D(); // set correct shader time before RB_BeginSurface() on 3D->2D transition
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_SetGL2D();

#ifdef USE_FBO
	//Check if it's time for BLOOM!
	R_BloomScreen();
#endif

	RB_CHECKOVERFLOW( 4, 6 );
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

//	*(int *)tess.vertexColors[ numVerts ] =
//		*(int *)tess.vertexColors[ numVerts + 1 ] =
//		*(int *)tess.vertexColors[ numVerts + 2 ] =
//		*(int *)tess.vertexColors[ numVerts + 3 ] = *(int *)backEnd.color2D;

	tess.vertexColors[ numVerts + 0 ] =
	tess.vertexColors[ numVerts + 1 ] = backEnd.color2D;

	tess.vertexColors[ numVerts + 2 ] =
	tess.vertexColors[ numVerts + 3 ] = cmd->gradientColor;

	tess.xyz[ numVerts ][0] = cmd->x;
	tess.xyz[ numVerts ][1] = cmd->y;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[0][ numVerts + 0 ][0] = cmd->s1;
	tess.texCoords[0][ numVerts + 0 ][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 1 ][1] = cmd->y;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[0][ numVerts + 1 ][0] = cmd->s2;
	tess.texCoords[0][ numVerts + 1 ][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 2 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[0][ numVerts + 2 ][0] = cmd->s2;
	tess.texCoords[0][ numVerts + 2 ][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x;
	tess.xyz[ numVerts + 3 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[0][ numVerts + 3 ][0] = cmd->s1;
	tess.texCoords[0][ numVerts + 3 ][1] = cmd->t2;

	return (const void *)( cmd + 1 );
}


static void transform_to_eye_space( const vec3_t v, vec3_t v_eye )
{
	const float *m = backEnd.viewParms.world.modelMatrix;
	v_eye[0] = m[0]*v[0] + m[4]*v[1] + m[8 ]*v[2] + m[12];
	v_eye[1] = m[1]*v[0] + m[5]*v[1] + m[9 ]*v[2] + m[13];
	v_eye[2] = m[2]*v[0] + m[6]*v[1] + m[10]*v[2] + m[14];
};


/*
================
RB_DebugPolygon
================
*/
/*static */void RB_DebugPolygon( int color, int numPoints, float *points ) {
	vec3_t pa;
	vec3_t pb;
	vec3_t p;
	vec3_t q;
	vec3_t n;
	int i;

	if ( numPoints < 3 ) {
		return;
	}

	transform_to_eye_space( &points[0], pa );
	transform_to_eye_space( &points[3], pb );
	VectorSubtract( pb, pa, p );

	for ( i = 2; i < numPoints; i++ ) {
		transform_to_eye_space( &points[3*i], pb );
		VectorSubtract( pb, pa, q );
		CrossProduct( q, p, n );
		if ( VectorLength( n ) > 1e-5 ) {
			break;
		}
	}

	if ( DotProduct( n, pa ) >= 0 ) {
		return; // discard backfacing polygon
	}

	GL_SelectTexture( 0 );
	qglDisable( GL_TEXTURE_2D );

	GL_ClientState( 0, CLS_NONE );
	qglVertexPointer( 3, GL_FLOAT, 0, points );

	// draw solid shade
	GL_State( GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );
	qglColor4f( color&1, (color>>1)&1, (color>>2)&1, 1 );
	qglDrawArrays( GL_TRIANGLE_FAN, 0, numPoints );

	// draw wireframe outline
	qglDepthRange( 0, 0 );
	qglColor4f( 1, 1, 1, 1 );
	qglDrawArrays( GL_LINE_LOOP, 0, numPoints );
	qglDepthRange( 0, 1 );

	qglEnable( GL_TEXTURE_2D );
}


/*
================
RB_DebugText
================
*/
void RB_DebugText( const vec3_t org, float r, float g, float b, const char *text, qboolean neverOcclude ) {

	if ( neverOcclude ) {
		qglDepthRange( 0, 0 );  // never occluded

	}
	qglColor4f( r, g, b, 1 );
	qglRasterPos3fv( org );
	qglPushAttrib( GL_LIST_BIT );
	qglListBase( ri.GLimp_NormalFontBase() );
	qglCallLists( strlen( text ), GL_UNSIGNED_BYTE, text );
	qglListBase( 0 );
	qglPopAttrib();

	if ( neverOcclude ) {
		qglDepthRange( 0, 1 );
	}
}


/*
====================
RB_DebugGraphics

Visualization aid for movement clipping debugging
====================
*/
static void RB_DebugGraphics( void ) {

	if ( !r_debugSurface->integer ) {
		return;
	}

	R_FogOff(); // moved this in here to keep from /always/ doing the fog state change

	GL_Bind( tr.whiteImage );
	GL_Cull( CT_FRONT_SIDED );

	ri.CM_DrawDebugSurface( RB_DebugPolygon );
}


/*
=============
RB_DrawSurfs
=============
*/
static const void *RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t *cmd;

	// finish any 2D drawing if needed
	RB_EndSurface();

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

#ifdef USE_VBO
	VBO_UnBind();
#endif

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView();

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_DrawSun();

	// darken down any stencil shadows
	RB_ShadowFinish();

	// add light flares on lights that aren't obscured
	RB_RenderFlares();

#ifdef USE_PMLIGHT
	if ( backEnd.refdef.numLitSurfs ) {
		RB_BeginDrawingLitSurfs();
		RB_LightingPass();
	}
#endif

#ifdef USE_FBO
	if ( !backEnd.doneSurfaces && tr.needScreenMap ) {
		if ( backEnd.viewParms.frameSceneNum == 1 ) {
			FBO_CopyScreen();
		}
	}
#endif

	// draw main system development information (surface outlines, etc)
	R_FogOff();
	RB_DebugGraphics();
	R_FogOn();

	//TODO Maybe check for rdf_noworld stuff but q3mme has full 3d ui
	if ( !( backEnd.refdef.rdflags & RDF_SKYBOXPORTAL ) ) {
		backEnd.doneSurfaces = qtrue; // for bloom
	}

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer
=============
*/
static const void *RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

#ifdef USE_FBO
	if ( fboEnabled ) {
		FBO_BindMain();
		qglDrawBuffer( GL_COLOR_ATTACHMENT0 );
	} else {
		qglDrawBuffer( cmd->buffer );
	}
#else
	qglDrawBuffer( cmd->buffer );
#endif

	// clear screen for debugging
	if ( r_clear->integer ) {
		qglClearColor( 1, 0, 0.5, 1 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	return (const void *)(cmd + 1);
}


/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages( void ) {
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;
	const vec2_t t[4] = { {0,0}, {1,0}, {0,1}, {1,1} };
	vec3_t v[4];

	RB_SetGL2D();

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	GL_ClientState( 0, CLS_TEXCOORD_ARRAY );
	qglTexCoordPointer( 2, GL_FLOAT, 0, t );

	start = ri.Milliseconds();

	for ( i = 0; i < tr.numImages; i++ ) {
		image = tr.images[ i ];

		w = glConfig.vidWidth / 40;
		h = glConfig.vidHeight / 30;

		x = i % 40 * w;
		y = i / 30 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind( image );

		VectorSet(v[0],x,y,0);
		VectorSet(v[1],x+w,y,0);
		VectorSet(v[2],x,y+h,0);
		VectorSet(v[3],x+w,y+h,0);

		qglVertexPointer( 3, GL_FLOAT, 0, v );
		qglDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	}

	qglFinish();

	end = ri.Milliseconds();
	ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );
}


/*
=============
RB_ColorMask
=============
*/
static const void *RB_ColorMask( const void *data )
{
	const colorMaskCommand_t *cmd = data;

	qglColorMask( cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3] );

	return (const void *)(cmd + 1);
}


/*
=============
RB_ClearDepth
=============
*/
static const void *RB_ClearDepth( const void *data )
{
	const clearDepthCommand_t *cmd = data;

	RB_EndSurface();

	qglClear( GL_DEPTH_BUFFER_BIT );

	return (const void *)(cmd + 1);
}


/*
=============
RB_ClearColor
=============
*/
static const void *RB_ClearColor( const void *data )
{
	const clearColorCommand_t *cmd = data;

	if ( cmd->fullscreen )
	{
		qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
		qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	}

	if ( cmd->colorMask )
	{
		qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	}

	qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );

	if ( cmd->frontAndBack )
	{
		qglDrawBuffer( GL_FRONT );
		qglClear( GL_COLOR_BUFFER_BIT );
		qglDrawBuffer( GL_BACK );
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	return (const void *)(cmd + 1);
}


/*
=============
RB_FinishBloom
=============
*/
#ifdef USE_FBO
static const void *RB_FinishBloom( const void *data )
{
	const finishBloomCommand_t *cmd = data;

	RB_EndSurface();

	if ( fboEnabled )
	{
		// let's always render console with the same quality
		// TODO: fix this to work with multiple views and opened console
		if ( blitMSfbo && tr.frameSceneNum == 1 )
		{
			FBO_BlitMS( qfalse );
			blitMSfbo = qfalse;
		}

		if ( r_bloom->integer && qglActiveTextureARB )
		{
			if ( !backEnd.doneBloom && backEnd.doneSurfaces )
			{
				RB_SetGL2D();
				qglColor4f( 1, 1, 1, 1 );
				FBO_Bloom( 0, 0, qfalse );
			}
		}
	}

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages();
	}

	backEnd.drawConsole = qtrue;

	return (const void *)(cmd + 1);
}
#endif // USE_FBO


static const void *RB_SwapBuffers( const void *data ) {

	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	RB_EndSurface();

#ifdef USE_VBO
	VBO_UnBind();
#endif

	// texture swapping test
	if ( r_showImages->integer && !backEnd.drawConsole ) {
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	if ( backEnd.doneSurfaces && !glState.finishCalled ) {
		qglFinish();
	}

#ifdef USE_FBO
	if ( fboEnabled ) {
		FBO_PostProcess();
	}
#endif

	// buffer swap may take undefined time to complete, we can't measure it in a reliable way
	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;

	if ( backEnd.screenshotMask && tr.frameCount > 1 ) {
#ifdef USE_FBO
		if ( superSampled ) {
			qglScissor( 0, 0, gls.captureWidth, gls.captureHeight );
			qglViewport( 0, 0, gls.captureWidth, gls.captureHeight );
			FBO_BlitSS();
		}
#endif
		if ( backEnd.screenshotMask & SCREENSHOT_TGA && backEnd.screenshotTGA[0] ) {
			RB_TakeScreenshot( 0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotTGA );
			if ( !backEnd.screenShotTGAsilent ) {
				ri.Printf( PRINT_ALL, "Wrote %s\n", backEnd.screenshotTGA );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_JPG && backEnd.screenshotJPG[0] ) {
			RB_TakeScreenshotJPEG( 0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotJPG );
			if ( !backEnd.screenShotJPGsilent ) {
				ri.Printf( PRINT_ALL, "Wrote %s\n", backEnd.screenshotJPG );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_BMP && ( backEnd.screenshotBMP[0] || ( backEnd.screenshotMask & SCREENSHOT_BMP_CLIPBOARD ) ) ) {
			RB_TakeScreenshotBMP( 0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotBMP, backEnd.screenshotMask & SCREENSHOT_BMP_CLIPBOARD );
			if ( !backEnd.screenShotBMPsilent ) {
				ri.Printf( PRINT_ALL, "Wrote %s\n", backEnd.screenshotBMP );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_AVI ) {
			RB_TakeVideoFrameCmd( &backEnd.vcmd );
		}

		backEnd.screenshotJPG[0] = '\0';
		backEnd.screenshotTGA[0] = '\0';
		backEnd.screenshotBMP[0] = '\0';
		backEnd.screenshotMask = 0;
	}

	ri.GLimp_EndFrame();

#ifdef USE_FBO
	FBO_BindMain();
#endif

	backEnd.projection2D = qfalse;
	backEnd.doneBloom = qfalse;
	backEnd.doneSurfaces = qfalse;
	backEnd.drawConsole = qfalse;

	r_anaglyphMode->modified = qfalse;

	return (const void *)(cmd + 1);
}



/*
=============
RB_DrawBounds - ydnar
=============
*/

#if 0 //unused
void RB_DrawBounds( vec3_t mins, vec3_t maxs ) {
	vec3_t center;

	GL_Bind( tr.whiteImage );
	GL_State( GLS_POLYMODE_LINE );

	// box corners
	qglBegin( GL_LINES );
	qglColor4f( 1, 1, 1, 1 );

	qglVertex3f( mins[ 0 ], mins[ 1 ], mins[ 2 ] );
	qglVertex3f( maxs[ 0 ], mins[ 1 ], mins[ 2 ] );
	qglVertex3f( mins[ 0 ], mins[ 1 ], mins[ 2 ] );
	qglVertex3f( mins[ 0 ], maxs[ 1 ], mins[ 2 ] );
	qglVertex3f( mins[ 0 ], mins[ 1 ], mins[ 2 ] );
	qglVertex3f( mins[ 0 ], mins[ 1 ], maxs[ 2 ] );

	qglVertex3f( maxs[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	qglVertex3f( mins[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	qglVertex3f( maxs[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	qglVertex3f( maxs[ 0 ], mins[ 1 ], maxs[ 2 ] );
	qglVertex3f( maxs[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	qglVertex3f( maxs[ 0 ], maxs[ 1 ], mins[ 2 ] );
	qglEnd();

	center[ 0 ] = ( mins[ 0 ] + maxs[ 0 ] ) * 0.5;
	center[ 1 ] = ( mins[ 1 ] + maxs[ 1 ] ) * 0.5;
	center[ 2 ] = ( mins[ 2 ] + maxs[ 2 ] ) * 0.5;

	// center axis
	qglBegin( GL_LINES );
	qglColor4f( 1, 0.85, 0, 1 );

	qglVertex3f( mins[ 0 ], center[ 1 ], center[ 2 ] );
	qglVertex3f( maxs[ 0 ], center[ 1 ], center[ 2 ] );
	qglVertex3f( center[ 0 ], mins[ 1 ], center[ 2 ] );
	qglVertex3f( center[ 0 ], maxs[ 1 ], center[ 2 ] );
	qglVertex3f( center[ 0 ], center[ 1 ], mins[ 2 ] );
	qglVertex3f( center[ 0 ], center[ 1 ], maxs[ 2 ] );
	qglEnd();
}
#endif

//bani
/*
=============
RB_RenderToTexture

=============
*/
static const void *RB_RenderToTexture( const void *data ) {
	const renderToTextureCommand_t  *cmd;

//	ri.Printf( PRINT_ALL, "RB_RenderToTexture\n" );

	cmd = (const renderToTextureCommand_t *)data;

	GL_Bind( cmd->image );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
	qglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, cmd->x, cmd->y, cmd->w, cmd->h, 0 );
//	qglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cmd->x, cmd->y, cmd->w, cmd->h );

	return (const void *)( cmd + 1 );
}

//bani
/*
=============
RB_Finish

=============
*/
static const void *RB_Finish( const void *data ) {
	const renderFinishCommand_t *cmd;

//	ri.Printf( PRINT_ALL, "RB_Finish\n" );

	cmd = (const renderFinishCommand_t *)data;

	// sync with gl if needed
	// ENSI NOTE ET didn't have r_finish check
	if ( r_finish->integer == 1 ) {
		qglFinish();
	}

	return (const void *)( cmd + 1 );
}

/*
====================
RB_ExecuteRenderCommands
====================
*/
void RB_ExecuteRenderCommands( const void *data ) {

	backEnd.pc.msec = ri.Milliseconds();

	while ( 1 ) {
		data = PADP(data, sizeof(void *));

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_2DPOLYS:
			data = RB_Draw2dPolys( data );
			break;
		case RC_ROTATED_PIC:
			data = RB_RotatedPic( data );
			break;
		case RC_STRETCH_PIC_GRADIENT:
			data = RB_StretchPicGradient( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
#ifdef USE_FBO
		case RC_FINISHBLOOM:
			data = RB_FinishBloom(data);
			break;
#endif
		case RC_COLORMASK:
			data = RB_ColorMask(data);
			break;
		case RC_CLEARDEPTH:
			data = RB_ClearDepth(data);
			break;
			//bani
		case RC_CLEARCOLOR:
			data = RB_ClearColor(data);
			break;
		case RC_RENDERTOTEXTURE:
			data = RB_RenderToTexture( data );
			break;
			//bani
		case RC_FINISH:
			data = RB_Finish( data );
			break;
		case RC_END_OF_LIST:
		default:
			// stop rendering
			return;
		}
	}
}
