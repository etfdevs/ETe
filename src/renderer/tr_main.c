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
// tr_main.c -- main control flow for each frame

#include "tr_local.h"

#include <string.h> // memcpy

trGlobals_t		tr;

static const float s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};


refimport_t	ri;

// entities that will have procedurally generated surfaces will just
// point at this for their sorting surface
static surfaceType_t entitySurface = SF_ENTITY;

// fog stuff
glfog_t glfogsettings[NUM_FOGS];
glfogType_t glfogNum = FOG_NONE;
qboolean fogIsOn = qfalse;


/*
=================
R_Fog (void)
=================
*/
void R_Fog( glfog_t *curfog ) {
	if ( !r_wolffog->integer ) {
		R_FogOff();
		return;
	}

	if ( !curfog->registered ) {   //----(SA)
		R_FogOff();
		return;
	}

	//----(SA) assme values of '0' for these parameters means 'use default'
	if ( !curfog->density ) {
		curfog->density = 1;
	}
	if ( !curfog->hint ) {
		curfog->hint = GL_DONT_CARE;
	}
	if ( !curfog->mode ) {
		curfog->mode = GL_LINEAR;
	}
	//----(SA)	end


	R_FogOn();

	// only send changes if necessary

//	if(curfog->mode != setfog.mode || !setfog.registered) {
	qglFogi( GL_FOG_MODE, curfog->mode );
//		setfog.mode = curfog->mode;
//	}
//	if(curfog->color[0] != setfog.color[0] || curfog->color[1] != setfog.color[1] || curfog->color[2] != setfog.color[2] || !setfog.registered) {
	qglFogfv( GL_FOG_COLOR, curfog->color );
//		VectorCopy(setfog.color, curfog->color);
//	}
//	if(curfog->density != setfog.density || !setfog.registered) {
	qglFogf( GL_FOG_DENSITY, curfog->density );
//		setfog.density = curfog->density;
//	}
//	if(curfog->hint != setfog.hint || !setfog.registered) {
	qglHint( GL_FOG_HINT, curfog->hint );
//		setfog.hint = curfog->hint;
//	}
//	if(curfog->start != setfog.start || !setfog.registered) {
	qglFogf( GL_FOG_START, curfog->start );
//		setfog.start = curfog->start;
//	}

	if ( r_zfar->value ) {             // (SA) allow override for helping level designers test fog distances
//		if(setfog.end != r_zfar->value || !setfog.registered) {
		qglFogf( GL_FOG_END, r_zfar->value );
//			setfog.end = r_zfar->value;
//		}
	} else {
//		if(curfog->end != setfog.end || !setfog.registered) {
		qglFogf( GL_FOG_END, curfog->end );
//			setfog.end = curfog->end;
//		}
	}

	qglClearColor( curfog->color[0], curfog->color[1], curfog->color[2], curfog->color[3] );
}

// Ridah, allow disabling fog temporarily
void R_FogOff( void ) {
	if ( !fogIsOn ) {
		return;
	}
	qglDisable( GL_FOG );
	fogIsOn = qfalse;
}

void R_FogOn( void ) {
	if ( fogIsOn ) {
		return;
	}

//	if(r_uiFullScreen->integer) {	// don't fog in the menu
//		R_FogOff();
//		return;
//	}

	if ( !r_wolffog->integer ) {
		return;
	}

//	if(backEnd.viewParms.isGLFogged) {
//		if(!(backEnd.viewParms.glFog.registered))
//			return;
//	}

	if ( backEnd.refdef.rdflags & RDF_SKYBOXPORTAL ) { // don't force world fog on portal sky
		if ( !( glfogsettings[FOG_PORTALVIEW].registered ) ) {
			return;
		}
	} else if ( !glfogNum )     {
		return;
	}

	qglEnable( GL_FOG );
	fogIsOn = qtrue;
}
// done.



//----(SA)
/*
==============
R_SetFog

  if fogvar == FOG_CMD_SWITCHFOG {
	fogvar is the command
	var1 is the fog to switch to
	var2 is the time to transition
  }
  else {
	fogvar is the fog that's being set
	var1 is the near fog z value
	var2 is the far fog z value
	rgb = color
	density is density, and is used to derive the values of 'mode', 'drawsky', and 'clearscreen'
  }
==============
*/
void R_SetFog( int fogvar, int var1, int var2, float r, float g, float b, float density ) {
	if ( fogvar != FOG_CMD_SWITCHFOG ) {   // just set the parameters and return

		if ( var1 == 0 && var2 == 0 ) {    // clear this fog
			glfogsettings[fogvar].registered = qfalse;
			return;
		}

		glfogsettings[fogvar].color[0]      = r * tr.identityLight;
		glfogsettings[fogvar].color[1]      = g * tr.identityLight;
		glfogsettings[fogvar].color[2]      = b * tr.identityLight;
		glfogsettings[fogvar].color[3]      = 1;
		glfogsettings[fogvar].start         = var1;
		glfogsettings[fogvar].end           = var2;
		if ( density >= 1 ) {
			glfogsettings[fogvar].mode          = GL_LINEAR;
			glfogsettings[fogvar].drawsky       = qfalse;
			glfogsettings[fogvar].clearscreen   = qtrue;
			glfogsettings[fogvar].density       = 1.0;
		} else
		{
			glfogsettings[fogvar].mode          = GL_EXP;
			glfogsettings[fogvar].drawsky       = qtrue;
			glfogsettings[fogvar].clearscreen   = qfalse;
			glfogsettings[fogvar].density       = density;
		}
		glfogsettings[fogvar].hint          = GL_DONT_CARE;
		glfogsettings[fogvar].registered    = qtrue;

		return;
	}

	// don't switch to invalid fogs
	if ( glfogsettings[var1].registered != qtrue ) {
		return;
	}

	glfogNum = var1;

	// transitioning to new fog, store the current values as the 'from'

	if ( glfogsettings[FOG_CURRENT].registered ) {
		memcpy( &glfogsettings[FOG_LAST], &glfogsettings[FOG_CURRENT], sizeof( glfog_t ) );
	} else {
		// if no current fog fall back to world fog
		// FIXME: handle transition if there is no FOG_MAP fog
		memcpy( &glfogsettings[FOG_LAST], &glfogsettings[FOG_MAP], sizeof( glfog_t ) );
	}

	memcpy( &glfogsettings[FOG_TARGET], &glfogsettings[glfogNum], sizeof( glfog_t ) );

	// setup transition times
	glfogsettings[FOG_TARGET].startTime = tr.refdef.time;
	glfogsettings[FOG_TARGET].finishTime = tr.refdef.time + var2;
}

//----(SA) end

/*
=================
R_CullLocalBox

Returns CULL_IN, CULL_CLIP, or CULL_OUT
=================
*/
int R_CullLocalBox( vec3_t bounds[2] ) {
	int		i, j;
	vec3_t	transformed[8];
	float	dists[8];
	vec3_t	v;
	cplane_t	*frust;
	int			anyBack;
	int			front, back;

	if ( r_nocull->integer ) {
		return CULL_CLIP;
	}

	// transform into world space
	for (i = 0 ; i < 8 ; i++) {
		v[0] = bounds[i&1][0];
		v[1] = bounds[(i>>1)&1][1];
		v[2] = bounds[(i>>2)&1][2];

		VectorCopy( tr.orientation.origin, transformed[i] );
		VectorMA( transformed[i], v[0], tr.orientation.axis[0], transformed[i] );
		VectorMA( transformed[i], v[1], tr.orientation.axis[1], transformed[i] );
		VectorMA( transformed[i], v[2], tr.orientation.axis[2], transformed[i] );
	}

	// check against frustum planes
	anyBack = 0;
	for ( i = 0 ; i < 6 ; i++ ) {
		frust = &tr.viewParms.frustum[i];

		front = back = 0;
		for (j = 0 ; j < 8 ; j++) {
			dists[j] = DotProduct(transformed[j], frust->normal);
			if ( dists[j] > frust->dist ) {
				front = 1;
				if ( back ) {
					break;		// a point is in front
				}
			} else {
				back = 1;
			}
		}
		if ( !front ) {
			// all points were behind one of the planes
			return CULL_OUT;
		}
		anyBack |= back;
	}

	if ( !anyBack ) {
		return CULL_IN;		// completely inside frustum
	}

	return CULL_CLIP;		// partially clipped
}


/*
** R_CullLocalPointAndRadius
*/
int R_CullLocalPointAndRadius( const vec3_t pt, float radius )
{
	vec3_t transformed;

	R_LocalPointToWorld( pt, transformed );

	return R_CullPointAndRadius( transformed, radius );
}


/*
** R_CullPointAndRadius
*/
int R_CullPointAndRadius( const vec3_t pt, float radius )
{
	int		i;
	float	dist;
	cplane_t	*frust;
	qboolean mightBeClipped = qfalse;

	if ( r_nocull->integer ) {
		return CULL_CLIP;
	}

	// check against frustum planes
	for ( i = 0 ; i < 6 ; i++ )
	{
		frust = &tr.viewParms.frustum[i];

		dist = DotProduct( pt, frust->normal) - frust->dist;
		if ( dist < -radius )
		{
			return CULL_OUT;
		}
		else if ( dist <= radius ) 
		{
			mightBeClipped = qtrue;
		}
	}

	if ( mightBeClipped )
	{
		return CULL_CLIP;
	}

	return CULL_IN;		// completely inside frustum
}


/*
** R_CullDlight
*/
int R_CullDlight( const dlight_t* dl )
{
	int		i;
	float	dist, dist2;
	cplane_t	*frust;
	qboolean mightBeClipped = qfalse;

	if ( r_nocull->integer )
		return CULL_CLIP;

	if ( dl->flags & REF_DIRECTED_DLIGHT )
		return CULL_IN;

	if ( dl->linear ) {
		for ( i = 0 ; i < 6 ; i++ ) {
			frust = &tr.viewParms.frustum[i];
			dist = DotProduct( dl->transformed, frust->normal) - frust->dist;
			dist2 = DotProduct( dl->transformed2, frust->normal) - frust->dist;
			if ( dist < -dl->radius && dist2 < -dl->radius )
				return CULL_OUT;
			else if ( dist <= dl->radius || dist2 <= dl->radius ) 
				mightBeClipped = qtrue;
		}
	} 
	else
	// check against frustum planes
	for ( i = 0 ; i < 6 ; i++ ) {
		frust = &tr.viewParms.frustum[i];
		dist = DotProduct( dl->transformed, frust->normal) - frust->dist;
		if ( dist < -dl->radius )
			return CULL_OUT;
		else if ( dist <= dl->radius ) 
			mightBeClipped = qtrue;
	}

	if ( mightBeClipped )
		return CULL_CLIP;

	return CULL_IN;	// completely inside frustum
}


/*
=================
R_LocalNormalToWorld
=================
*/
void R_LocalNormalToWorld( const vec3_t local, vec3_t world ) {
	world[0] = local[0] * tr.orientation.axis[0][0] + local[1] * tr.orientation.axis[1][0] + local[2] * tr.orientation.axis[2][0];
	world[1] = local[0] * tr.orientation.axis[0][1] + local[1] * tr.orientation.axis[1][1] + local[2] * tr.orientation.axis[2][1];
	world[2] = local[0] * tr.orientation.axis[0][2] + local[1] * tr.orientation.axis[1][2] + local[2] * tr.orientation.axis[2][2];
}


/*
=================
R_LocalPointToWorld
=================
*/
void R_LocalPointToWorld( const vec3_t local, vec3_t world ) {
	world[0] = local[0] * tr.orientation.axis[0][0] + local[1] * tr.orientation.axis[1][0] + local[2] * tr.orientation.axis[2][0] + tr.orientation.origin[0];
	world[1] = local[0] * tr.orientation.axis[0][1] + local[1] * tr.orientation.axis[1][1] + local[2] * tr.orientation.axis[2][1] + tr.orientation.origin[1];
	world[2] = local[0] * tr.orientation.axis[0][2] + local[1] * tr.orientation.axis[1][2] + local[2] * tr.orientation.axis[2][2] + tr.orientation.origin[2];
}


/*
=================
R_WorldToLocal
=================
*/
void R_WorldToLocal( const vec3_t world, vec3_t local ) {
	local[0] = DotProduct( world, tr.orientation.axis[0] );
	local[1] = DotProduct( world, tr.orientation.axis[1] );
	local[2] = DotProduct( world, tr.orientation.axis[2] );
}


/*
==========================
R_TransformModelToClip
==========================
*/
void R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst ) {
	int i;

	for ( i = 0 ; i < 4 ; i++ ) {
		eye[i] = 
			src[0] * modelMatrix[ i + 0 * 4 ] +
			src[1] * modelMatrix[ i + 1 * 4 ] +
			src[2] * modelMatrix[ i + 2 * 4 ] +
			1 * modelMatrix[ i + 3 * 4 ];
	}

	for ( i = 0 ; i < 4 ; i++ ) {
		dst[i] = 
			eye[0] * projectionMatrix[ i + 0 * 4 ] +
			eye[1] * projectionMatrix[ i + 1 * 4 ] +
			eye[2] * projectionMatrix[ i + 2 * 4 ] +
			eye[3] * projectionMatrix[ i + 3 * 4 ];
	}
}


/*
==========================
R_TransformModelToClipMVP
==========================
*/
static void R_TransformModelToClipMVP( const vec3_t src, const float *mvp, vec4_t clip ) {
	int i;

	for ( i = 0 ; i < 4 ; i++ ) {
		clip[i] = 
			src[0] * mvp[ i + 0 * 4 ] +
			src[1] * mvp[ i + 1 * 4 ] +
			src[2] * mvp[ i + 2 * 4 ] +
			1 * mvp[ i + 3 * 4 ];
	}
}


/*
==========================
R_TransformClipToWindow
==========================
*/
void R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window ) {
	normalized[0] = clip[0] / clip[3];
	normalized[1] = clip[1] / clip[3];
	normalized[2] = ( clip[2] + clip[3] ) / ( 2 * clip[3] );

	window[0] = 0.5f * ( 1.0f + normalized[0] ) * view->viewportWidth;
	window[1] = 0.5f * ( 1.0f + normalized[1] ) * view->viewportHeight;
	window[2] = normalized[2];

	window[0] = (int) ( window[0] + 0.5 );
	window[1] = (int) ( window[1] + 0.5 );
}


/*
==========================
myGlMultMatrix
==========================
*/
void myGlMultMatrix( const float *a, const float *b, float *out ) {
	int		i, j;

	for ( i = 0 ; i < 4 ; i++ ) {
		for ( j = 0 ; j < 4 ; j++ ) {
			out[ i * 4 + j ] =
				a [ i * 4 + 0 ] * b [ 0 * 4 + j ]
				+ a [ i * 4 + 1 ] * b [ 1 * 4 + j ]
				+ a [ i * 4 + 2 ] * b [ 2 * 4 + j ]
				+ a [ i * 4 + 3 ] * b [ 3 * 4 + j ];
		}
	}
}


/*
=================
R_RotateForEntity

Generates an orientation for an entity and viewParms
Does NOT produce any GL calls
Called by both the front end and the back end
=================
*/
void R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms,
					   orientationr_t *or ) {
	float	glMatrix[16];
	vec3_t	delta;
	float	axisLength;

	if ( ent->e.reType != RT_MODEL ) {
		*or = viewParms->world;
		return;
	}

	VectorCopy( ent->e.origin, or->origin );

	VectorCopy( ent->e.axis[0], or->axis[0] );
	VectorCopy( ent->e.axis[1], or->axis[1] );
	VectorCopy( ent->e.axis[2], or->axis[2] );

	glMatrix[0] = or->axis[0][0];
	glMatrix[4] = or->axis[1][0];
	glMatrix[8] = or->axis[2][0];
	glMatrix[12] = or->origin[0];

	glMatrix[1] = or->axis[0][1];
	glMatrix[5] = or->axis[1][1];
	glMatrix[9] = or->axis[2][1];
	glMatrix[13] = or->origin[1];

	glMatrix[2] = or->axis[0][2];
	glMatrix[6] = or->axis[1][2];
	glMatrix[10] = or->axis[2][2];
	glMatrix[14] = or->origin[2];

	glMatrix[3] = 0;
	glMatrix[7] = 0;
	glMatrix[11] = 0;
	glMatrix[15] = 1;

	myGlMultMatrix( glMatrix, viewParms->world.modelMatrix, or->modelMatrix );

	// calculate the viewer origin in the model's space
	// needed for fog, specular, and environment mapping
	VectorSubtract( viewParms->orientation.origin, or->origin, delta );

	// compensate for scale in the axes if necessary
	if ( ent->e.nonNormalizedAxes ) {
		axisLength = VectorLength( ent->e.axis[0] );
		if ( !axisLength ) {
			axisLength = 0;
		} else {
			axisLength = 1.0f / axisLength;
		}
	} else {
		axisLength = 1.0f;
	}

	or->viewOrigin[0] = DotProduct( delta, or->axis[0] ) * axisLength;
	or->viewOrigin[1] = DotProduct( delta, or->axis[1] ) * axisLength;
	or->viewOrigin[2] = DotProduct( delta, or->axis[2] ) * axisLength;
}


/*
=================
R_RotateForViewer

Sets up the modelview matrix for a given viewParm
=================
*/
static void R_RotateForViewer( void )
{
	float	viewerMatrix[16];
	vec3_t	origin;

	memset( &tr.orientation, 0, sizeof( tr.orientation ) );
	tr.orientation.axis[0][0] = 1;
	tr.orientation.axis[1][1] = 1;
	tr.orientation.axis[2][2] = 1;
	VectorCopy( tr.viewParms.orientation.origin, tr.orientation.viewOrigin );

	// transform by the camera placement
	VectorCopy( tr.viewParms.orientation.origin, origin );

	viewerMatrix[0] = tr.viewParms.orientation.axis[0][0];
	viewerMatrix[4] = tr.viewParms.orientation.axis[0][1];
	viewerMatrix[8] = tr.viewParms.orientation.axis[0][2];
	viewerMatrix[12] = -origin[0] * viewerMatrix[0] + - origin[1] * viewerMatrix[4] + - origin[2] * viewerMatrix[8];

	viewerMatrix[1] = tr.viewParms.orientation.axis[1][0];
	viewerMatrix[5] = tr.viewParms.orientation.axis[1][1];
	viewerMatrix[9] = tr.viewParms.orientation.axis[1][2];
	viewerMatrix[13] = -origin[0] * viewerMatrix[1] + - origin[1] * viewerMatrix[5] + - origin[2] * viewerMatrix[9];

	viewerMatrix[2] = tr.viewParms.orientation.axis[2][0];
	viewerMatrix[6] = tr.viewParms.orientation.axis[2][1];
	viewerMatrix[10] = tr.viewParms.orientation.axis[2][2];
	viewerMatrix[14] = -origin[0] * viewerMatrix[2] + - origin[1] * viewerMatrix[6] + - origin[2] * viewerMatrix[10];

	viewerMatrix[3] = 0;
	viewerMatrix[7] = 0;
	viewerMatrix[11] = 0;
	viewerMatrix[15] = 1;

	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	myGlMultMatrix( viewerMatrix, s_flipMatrix, tr.orientation.modelMatrix );

	tr.viewParms.world = tr.orientation;
}


/*
==============
R_SetFrameFog
==============
*/
void R_SetFrameFog( void ) {

	// Arnout: new style global fog transitions
	if ( tr.world->globalFogTransEndTime ) {
		if ( tr.world->globalFogTransEndTime >= tr.refdef.time ) {
			float lerpPos;
			int fadeTime;

			fadeTime = tr.world->globalFogTransEndTime - tr.world->globalFogTransStartTime;
			lerpPos = (float)( tr.refdef.time - tr.world->globalFogTransStartTime ) / (float)fadeTime;
			if ( lerpPos > 1 ) {
				lerpPos = 1;
			}

			tr.world->fogs[tr.world->globalFog].shader->fogParms.color[ 0 ] = tr.world->globalTransStartFog[ 0 ] + ( ( tr.world->globalTransEndFog[ 0 ] - tr.world->globalTransStartFog[ 0 ] ) * lerpPos );
			tr.world->fogs[tr.world->globalFog].shader->fogParms.color[ 1 ] = tr.world->globalTransStartFog[ 1 ] + ( ( tr.world->globalTransEndFog[ 1 ] - tr.world->globalTransStartFog[ 1 ] ) * lerpPos );
			tr.world->fogs[tr.world->globalFog].shader->fogParms.color[ 2 ] = tr.world->globalTransStartFog[ 2 ] + ( ( tr.world->globalTransEndFog[ 2 ] - tr.world->globalTransStartFog[ 2 ] ) * lerpPos );

			tr.world->fogs[tr.world->globalFog].shader->fogParms.colorInt = ColorBytes4( tr.world->fogs[tr.world->globalFog].shader->fogParms.color[ 0 ] * tr.identityLight,
																						 tr.world->fogs[tr.world->globalFog].shader->fogParms.color[ 1 ] * tr.identityLight,
																						 tr.world->fogs[tr.world->globalFog].shader->fogParms.color[ 2 ] * tr.identityLight, 1.0 );

			tr.world->fogs[tr.world->globalFog].shader->fogParms.depthForOpaque = tr.world->globalTransStartFog[ 3 ] + ( ( tr.world->globalTransEndFog[ 3 ] - tr.world->globalTransStartFog[ 3 ] ) * lerpPos );
			tr.world->fogs[tr.world->globalFog].shader->fogParms.tcScale = 1.0f / ( tr.world->fogs[tr.world->globalFog].shader->fogParms.depthForOpaque * 8 );
		} else {
			// transition complete
			VectorCopy( tr.world->globalTransEndFog, tr.world->fogs[tr.world->globalFog].shader->fogParms.color );
			tr.world->fogs[tr.world->globalFog].shader->fogParms.colorInt = ColorBytes4( tr.world->globalTransEndFog[ 0 ] * tr.identityLight,
																						 tr.world->globalTransEndFog[ 1 ] * tr.identityLight,
																						 tr.world->globalTransEndFog[ 2 ] * tr.identityLight, 1.0 );
			tr.world->fogs[tr.world->globalFog].shader->fogParms.depthForOpaque = tr.world->globalTransEndFog[ 3 ];
			tr.world->fogs[tr.world->globalFog].shader->fogParms.tcScale = 1.0f / ( tr.world->fogs[tr.world->globalFog].shader->fogParms.depthForOpaque * 8 );

			tr.world->globalFogTransEndTime = 0;
		}
	}

	if ( r_speeds->integer == 5 ) {
		if ( !glfogsettings[FOG_TARGET].registered ) {
			ri.Printf( PRINT_ALL, "no fog - calc zFar: %0.1f\n", tr.viewParms.zFar );
			return;
		}
	}

	// DHM - Nerve :: If fog is not valid, don't use it
	if ( !glfogsettings[FOG_TARGET].registered ) {
		return;
	}

	// still fading
	if ( glfogsettings[FOG_TARGET].finishTime && glfogsettings[FOG_TARGET].finishTime >= tr.refdef.time ) {
		float lerpPos;
		int fadeTime;

		// transitioning from density to distance
		if ( glfogsettings[FOG_LAST].mode == GL_EXP && glfogsettings[FOG_TARGET].mode == GL_LINEAR ) {
			// for now just fast transition to the target when dissimilar fogs are
			memcpy( &glfogsettings[FOG_CURRENT], &glfogsettings[FOG_TARGET], sizeof( glfog_t ) );
			glfogsettings[FOG_TARGET].finishTime = 0;
		}
		// transitioning from distance to density
		else if ( glfogsettings[FOG_LAST].mode == GL_LINEAR && glfogsettings[FOG_TARGET].mode == GL_EXP ) {
			memcpy( &glfogsettings[FOG_CURRENT], &glfogsettings[FOG_TARGET], sizeof( glfog_t ) );
			glfogsettings[FOG_TARGET].finishTime = 0;
		}
		// transitioning like fog modes
		else {

			fadeTime = glfogsettings[FOG_TARGET].finishTime - glfogsettings[FOG_TARGET].startTime;
			if ( fadeTime <= 0 ) {
				fadeTime = 1;   // avoid divide by zero


			}
			lerpPos = (float)( tr.refdef.time - glfogsettings[FOG_TARGET].startTime ) / (float)fadeTime;
			if ( lerpPos > 1 ) {
				lerpPos = 1;
			}

			// lerp near/far
			glfogsettings[FOG_CURRENT].start        = glfogsettings[FOG_LAST].start + ( ( glfogsettings[FOG_TARGET].start - glfogsettings[FOG_LAST].start ) * lerpPos );
			glfogsettings[FOG_CURRENT].end          = glfogsettings[FOG_LAST].end + ( ( glfogsettings[FOG_TARGET].end - glfogsettings[FOG_LAST].end ) * lerpPos );

			// lerp color
			glfogsettings[FOG_CURRENT].color[0]     = glfogsettings[FOG_LAST].color[0] + ( ( glfogsettings[FOG_TARGET].color[0] - glfogsettings[FOG_LAST].color[0] ) * lerpPos );
			glfogsettings[FOG_CURRENT].color[1]     = glfogsettings[FOG_LAST].color[1] + ( ( glfogsettings[FOG_TARGET].color[1] - glfogsettings[FOG_LAST].color[1] ) * lerpPos );
			glfogsettings[FOG_CURRENT].color[2]     = glfogsettings[FOG_LAST].color[2] + ( ( glfogsettings[FOG_TARGET].color[2] - glfogsettings[FOG_LAST].color[2] ) * lerpPos );

			glfogsettings[FOG_CURRENT].density      = glfogsettings[FOG_TARGET].density;
			glfogsettings[FOG_CURRENT].mode         = glfogsettings[FOG_TARGET].mode;
			glfogsettings[FOG_CURRENT].registered   = qtrue;

			// if either fog in the transition clears the screen, clear the background this frame to avoid hall of mirrors
			glfogsettings[FOG_CURRENT].clearscreen  = ( glfogsettings[FOG_TARGET].clearscreen || glfogsettings[FOG_LAST].clearscreen );
		}
	} else {
		// probably usually not necessary to copy the whole thing.
		// potential FIXME: since this is the most common occurance, diff first and only set changes
		memcpy( &glfogsettings[FOG_CURRENT], &glfogsettings[FOG_TARGET], sizeof( glfog_t ) );
	}


	// shorten the far clip if the fog opaque distance is closer than the procedural farcip dist

	if ( glfogsettings[FOG_CURRENT].mode == GL_LINEAR ) {
		if ( glfogsettings[FOG_CURRENT].end < tr.viewParms.zFar ) {
			tr.viewParms.zFar = glfogsettings[FOG_CURRENT].end;
		}
	}
//	else
//		glfogsettings[FOG_CURRENT].end = 5;


	if ( r_speeds->integer == 5 ) {
		if ( glfogsettings[FOG_CURRENT].mode == GL_LINEAR ) {
			ri.Printf( PRINT_ALL, "farclip fog - den: %0.1f  calc zFar: %0.1f  fog zfar: %0.1f\n", glfogsettings[FOG_CURRENT].density, tr.viewParms.zFar, glfogsettings[FOG_CURRENT].end );
		} else {
			ri.Printf( PRINT_ALL, "density fog - den: %0.4f  calc zFar: %0.1f  fog zFar: %0.1f\n", glfogsettings[FOG_CURRENT].density, tr.viewParms.zFar, glfogsettings[FOG_CURRENT].end );
		}
	}
}


/*
** SetFarClip
*/
static void R_SetFarClip( void )
{
	float	farthestCornerDistance;
	int		i;

	// if not rendering the world (icons, menus, etc)
	// set a 2k far clip plane
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		tr.viewParms.zFar = 2048;
		return;
	}

	//----(SA)	this lets you use r_zfar from the command line to experiment with different
	//			distances, but setting it back to 0 uses the map (or procedurally generated) default
	if ( r_zfar->value ) {

		tr.viewParms.zFar = r_zfar->integer;
		R_SetFrameFog();

		if ( r_speeds->integer == 5 ) {
			ri.Printf( PRINT_ALL, "r_zfar value forcing farclip at: %f\n", tr.viewParms.zFar );
		}

		return;
	}

	//
	// set far clipping planes dynamically
	//
	farthestCornerDistance = 0;
	for ( i = 0; i < 8; i++ )
	{
		vec3_t v;
		vec3_t vecTo;
		float distance;

		if ( i & 1 )
		{
			v[0] = tr.viewParms.visBounds[0][0];
		}
		else
		{
			v[0] = tr.viewParms.visBounds[1][0];
		}

		if ( i & 2 )
		{
			v[1] = tr.viewParms.visBounds[0][1];
		}
		else
		{
			v[1] = tr.viewParms.visBounds[1][1];
		}

		if ( i & 4 )
		{
			v[2] = tr.viewParms.visBounds[0][2];
		}
		else
		{
			v[2] = tr.viewParms.visBounds[1][2];
		}

		VectorSubtract( v, tr.viewParms.orientation.origin, vecTo );

		distance = vecTo[0] * vecTo[0] + vecTo[1] * vecTo[1] + vecTo[2] * vecTo[2];

		if ( distance > farthestCornerDistance )
		{
			farthestCornerDistance = distance;
		}
	}
	tr.viewParms.zFar = sqrt( farthestCornerDistance );

	// ydnar: add global q3 fog
	if ( tr.world != NULL && tr.world->globalFog >= 0 && tr.world->fogs[tr.world->globalFog].shader->fogParms.depthForOpaque < tr.viewParms.zFar ) {
		tr.viewParms.zFar = tr.world->fogs[tr.world->globalFog].shader->fogParms.depthForOpaque;
	}

	R_SetFrameFog();
}

/*
=================
R_SetupFrustum

Set up the culling frustum planes for the current view using the results we got from computing the first two rows of
the projection matrix.
=================
*/
static void R_SetupFrustum( viewParms_t *dest, float xmin, float xmax, float ymax, float zProj, float stereoSep )
{
	vec3_t ofsorigin;
	float oppleg, adjleg, length;
	int i;

	// set the projection matrix (and view frustum) here
	// first with max or fog distance so we can have proper
	// arbitrary frustum farplane culling optimization
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL || tr.world == NULL ) {
		VectorSet( dest->visBounds[ 0 ], MIN_WORLD_COORD, MIN_WORLD_COORD, MIN_WORLD_COORD );
		VectorSet( dest->visBounds[ 1 ], MAX_WORLD_COORD, MAX_WORLD_COORD, MAX_WORLD_COORD );
	} else {
		VectorCopy( tr.world->nodes->mins, dest->visBounds[ 0 ] );
		VectorCopy( tr.world->nodes->maxs, dest->visBounds[ 1 ] );
	}

	if(stereoSep == 0 && xmin == -xmax)
	{
		// symmetric case can be simplified
		VectorCopy(dest->orientation.origin, ofsorigin);

		length = sqrt(xmax * xmax + zProj * zProj);
		oppleg = xmax / length;
		adjleg = zProj / length;

		VectorScale(dest->orientation.axis[0], oppleg, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, adjleg, dest->orientation.axis[1], dest->frustum[0].normal);

		VectorScale(dest->orientation.axis[0], oppleg, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -adjleg, dest->orientation.axis[1], dest->frustum[1].normal);
	}
	else
	{
		// In stereo rendering, due to the modification of the projection matrix, dest->orientation.origin is not the
		// actual origin that we're rendering so offset the tip of the view pyramid.
		VectorMA(dest->orientation.origin, stereoSep, dest->orientation.axis[1], ofsorigin);
	
		oppleg = xmax + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest->orientation.axis[0], oppleg / length, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, zProj / length, dest->orientation.axis[1], dest->frustum[0].normal);

		oppleg = xmin + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest->orientation.axis[0], -oppleg / length, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -zProj / length, dest->orientation.axis[1], dest->frustum[1].normal);
	}

	length = sqrt(ymax * ymax + zProj * zProj);
	oppleg = ymax / length;
	adjleg = zProj / length;

	VectorScale(dest->orientation.axis[0], oppleg, dest->frustum[2].normal);
	VectorMA(dest->frustum[2].normal, adjleg, dest->orientation.axis[2], dest->frustum[2].normal);

	VectorScale(dest->orientation.axis[0], oppleg, dest->frustum[3].normal);
	VectorMA(dest->frustum[3].normal, -adjleg, dest->orientation.axis[2], dest->frustum[3].normal);
	
	for (i=0 ; i<4 ; i++) {
		dest->frustum[i].type = PLANE_NON_AXIAL;
		dest->frustum[i].dist = DotProduct (ofsorigin, dest->frustum[i].normal);
		SetPlaneSignbits( &dest->frustum[i] );
	}

	// ydnar: farplane (testing! use farplane for real)
	VectorScale( dest->orientation.axis[ 0 ], -1, dest->frustum[ 4 ].normal );
	dest->frustum[ 4 ].dist = DotProduct( dest->orientation.origin, dest->frustum[ 4 ].normal ) - dest->zFar;
	dest->frustum[ 4 ].type = PLANE_NON_AXIAL;
	SetPlaneSignbits( &dest->frustum[ 4 ] );

	// near clipping plane
	VectorCopy( dest->orientation.axis[ 0 ], dest->frustum[ 5 ].normal );
	dest->frustum[5].type = PLANE_NON_AXIAL;
	dest->frustum[5].dist = DotProduct( ofsorigin, dest->frustum[5].normal ) + r_znear->value;
	SetPlaneSignbits( &dest->frustum[5] );
}


/*
===============
R_SetupProjection
===============
*/
void R_SetupProjection( viewParms_t *dest, float zProj, qboolean computeFrustum )
{
	float	xmin, xmax, ymin, ymax;
	float	width, height, stereoSep = r_stereoSeparation->value;

	/*
	 * offset the view origin of the viewer for stereo rendering 
	 * by setting the projection matrix appropriately.
	 */

	if ( stereoSep != 0 )
	{
		if ( dest->stereoFrame == STEREO_LEFT )
			stereoSep = zProj / stereoSep;
		else if ( dest->stereoFrame == STEREO_RIGHT )
			stereoSep = zProj / -stereoSep;
		else
			stereoSep = 0;
	}

	ymax = zProj * tan(dest->fovY * M_PI / 360.0f);
	ymin = -ymax;

	xmax = zProj * tan(dest->fovX * M_PI / 360.0f);
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;
	
	dest->projectionMatrix[0] = 2 * zProj / width;
	dest->projectionMatrix[4] = 0;
	dest->projectionMatrix[8] = (xmax + xmin + 2 * stereoSep) / width;
	dest->projectionMatrix[12] = 2 * zProj * stereoSep / width;

	dest->projectionMatrix[1] = 0;
	dest->projectionMatrix[5] = 2 * zProj / height;
	dest->projectionMatrix[9] = ( ymax + ymin ) / height;	// normally 0
	dest->projectionMatrix[13] = 0;

	dest->projectionMatrix[3] = 0;
	dest->projectionMatrix[7] = 0;
	dest->projectionMatrix[11] = -1;
	dest->projectionMatrix[15] = 0;
	
	// Now that we have all the data for the projection matrix we can also setup the view frustum.
	if ( computeFrustum )
	{
		// dynamically compute far clip plane distance
		R_SetFarClip();

		R_SetupFrustum(dest, xmin, xmax, ymax, zProj, stereoSep);
	}
}

/*
===============
R_SetupProjectionZ

Sets the z-component transformation part in the projection matrix
===============
*/
static void R_SetupProjectionZ( viewParms_t *dest )
{
	float zNear, zFar, depth;
	
	zNear	= r_znear->value;

	// ydnar: high fov values let players see through walls
	// solution is to move z near plane inward, which decreases zbuffer precision
	// but if a player wants to play with fov 160, then they can deal with z-fighting
	// assume fov 90 = scale 1, fov 180 = scale 1/16th
	if ( tr.refdef.fov_x > 90.0f ) {
		zNear /= ( ( tr.refdef.fov_x - 90.0f ) * 0.09f + 1.0f );
	}

	if ( r_zfar->value ) {
		zFar	= r_zfar->value;	// (SA) allow override for helping level designers test fog distances
	} else {
		zFar	= dest->zFar;
	}

	depth	= zFar - zNear;

	dest->projectionMatrix[2] = 0;
	dest->projectionMatrix[6] = 0;
	dest->projectionMatrix[10] = -( zFar + zNear ) / depth;
	dest->projectionMatrix[14] = -2 * zFar * zNear / depth;

	if ( dest->portalView != PV_NONE )
	{
		float	plane[4];
		float	plane2[4];
		vec4_t q, c;

		// transform portal plane into camera space
		plane[0] = dest->portalPlane.normal[0];
		plane[1] = dest->portalPlane.normal[1];
		plane[2] = dest->portalPlane.normal[2];
		plane[3] = dest->portalPlane.dist;

		plane2[0] = -DotProduct( dest->orientation.axis[1], plane );
		plane2[1] =  DotProduct( dest->orientation.axis[2], plane );
		plane2[2] = -DotProduct( dest->orientation.axis[0], plane );
		plane2[3] =  DotProduct( plane, dest->orientation.origin) - plane[3];

		// Lengyel, Eric. "Modifying the Projection Matrix to Perform Oblique Near-plane Clipping".
		// Terathon Software 3D Graphics Library, 2004. http://www.terathon.com/code/oblique.html
		q[0] = (SGN(plane2[0]) + dest->projectionMatrix[8]) / dest->projectionMatrix[0];
		q[1] = (SGN(plane2[1]) + dest->projectionMatrix[9]) / dest->projectionMatrix[5];
		q[2] = -1.0f;
		q[3] = (1.0f + dest->projectionMatrix[10]) / dest->projectionMatrix[14];

		VectorScale4( plane2, 2.0f / DotProduct4(plane2, q), c );

		dest->projectionMatrix[2]  = c[0];
		dest->projectionMatrix[6]  = c[1];
		dest->projectionMatrix[10] = c[2] + 1.0f;
		dest->projectionMatrix[14] = c[3];
	}
}


/*
=================
R_MirrorPoint
=================
*/
static void R_MirrorPoint( const vec3_t in, const orientation_t *surface, const orientation_t *camera, vec3_t out ) {
	int		i;
	vec3_t	local;
	vec3_t	transformed;
	float	d;

	VectorSubtract( in, surface->origin, local );

	VectorClear( transformed );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct( local, surface->axis[i] );
		VectorMA( transformed, d, camera->axis[i], transformed );
	}

	VectorAdd( transformed, camera->origin, out );
}


static void R_MirrorVector( const vec3_t in, const orientation_t *surface, const orientation_t *camera, vec3_t out ) {
	int		i;
	float	d;

	VectorClear( out );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct(in, surface->axis[i]);
		VectorMA( out, d, camera->axis[i], out );
	}
}


/*
=============
R_PlaneForSurface
=============
*/
static void R_PlaneForSurface( const surfaceType_t *surfType, cplane_t *plane ) {
	srfTriangles_t	*tri;
	srfPoly_t		*poly;
	drawVert_t		*v1, *v2, *v3;
	vec4_t			plane4;

	if (!surfType) {
		Com_Memset (plane, 0, sizeof(*plane));
		plane->normal[0] = 1;
		return;
	}
	switch (*surfType) {
	case SF_FACE:
		*plane = ((srfSurfaceFace_t *)surfType)->plane;
		return;
	case SF_TRIANGLES:
		tri = (srfTriangles_t *)surfType;
		v1 = tri->verts + tri->indexes[0];
		v2 = tri->verts + tri->indexes[1];
		v3 = tri->verts + tri->indexes[2];
		PlaneFromPoints( plane4, v1->xyz, v2->xyz, v3->xyz );
		VectorCopy( plane4, plane->normal ); 
		plane->dist = plane4[3];
		return;
	case SF_POLY:
		poly = (srfPoly_t *)surfType;
		PlaneFromPoints( plane4, poly->verts[0].xyz, poly->verts[1].xyz, poly->verts[2].xyz );
		VectorCopy( plane4, plane->normal ); 
		plane->dist = plane4[3];
		return;
	default:
		Com_Memset (plane, 0, sizeof(*plane));
		plane->normal[0] = 1;
		return;
	}
}


/*
=================
R_GetPortalOrientation

entityNum is the entity that the portal surface is a part of, which may
be moving and rotating.

Returns qtrue if it should be mirrored
=================
*/
static qboolean R_GetPortalOrientations( const drawSurf_t *drawSurf, int entityNum,
							 orientation_t *surface, orientation_t *camera,
							 vec3_t pvsOrigin, portalView_t *portalView ) {
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;
	vec3_t		transformed;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	// rotate the plane if necessary
	if ( entityNum != REFENTITYNUM_WORLD ) {
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		// get the orientation of the entity
		R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.orientation );

		// rotate the plane, but keep the non-rotated version for matching
		// against the portalSurface entities
		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.orientation.origin );

		// translate the original plane
		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.orientation.origin );
	} else {
		plane = originalPlane;
	}

	VectorCopy( plane.normal, surface->axis[0] );
	PerpendicularVector( surface->axis[1], surface->axis[0] );
	CrossProduct( surface->axis[0], surface->axis[1], surface->axis[2] );

	// locate the portal entity closest to this plane.
	// origin will be the origin of the portal, origin2 will be
	// the origin of the camera
	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) {
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		// get the pvsOrigin from the entity
		VectorCopy( e->e.oldorigin, pvsOrigin );

		// if the entity is just a mirror, don't use as a camera point
		if ( e->e.oldorigin[0] == e->e.origin[0] && 
			e->e.oldorigin[1] == e->e.origin[1] && 
			e->e.oldorigin[2] == e->e.origin[2] ) {
			VectorScale( plane.normal, plane.dist, surface->origin );
			VectorCopy( surface->origin, camera->origin );
			VectorSubtract( vec3_origin, surface->axis[0], camera->axis[0] );
			VectorCopy( surface->axis[1], camera->axis[1] );
			VectorCopy( surface->axis[2], camera->axis[2] );

			*portalView = PV_MIRROR;
			return qtrue;
		}

		// project the origin onto the surface plane to get
		// an origin point we can rotate around
		d = DotProduct( e->e.origin, plane.normal ) - plane.dist;
		VectorMA( e->e.origin, -d, surface->axis[0], surface->origin );

		// now get the camera origin and orientation
		VectorCopy( e->e.oldorigin, camera->origin );
		AxisCopy( e->e.axis, camera->axis );
		VectorSubtract( vec3_origin, camera->axis[0], camera->axis[0] );
		VectorSubtract( vec3_origin, camera->axis[1], camera->axis[1] );

		// optionally rotate
		if ( e->e.oldframe ) {
			// if a speed is specified
			if ( e->e.frame ) {
				// continuous rotate
				d = (tr.refdef.time/1000.0f) * e->e.frame;
				VectorCopy( camera->axis[1], transformed );
				RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
				CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
			} else {
				// bobbing rotate, with skinNum being the rotation offset
				d = sin( tr.refdef.time * 0.003f );
				d = e->e.skinNum + d * 4;
				VectorCopy( camera->axis[1], transformed );
				RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
				CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
			}
		}
		else if ( e->e.skinNum ) {
			d = e->e.skinNum;
			VectorCopy( camera->axis[1], transformed );
			RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
			CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
		}

		*portalView = PV_PORTAL;
		return qtrue;
	}

	// if we didn't locate a portal entity, don't render anything.
	// We don't want to just treat it as a mirror, because without a
	// portal entity the server won't have communicated a proper entity set
	// in the snapshot

	// unfortunately, with local movement prediction it is easily possible
	// to see a surface before the server has communicated the matching
	// portal surface entity, so we don't want to print anything here...

	//ri.Printf( PRINT_ALL, "Portal surface without a portal entity\n" );

	return qfalse;
}


static qboolean IsMirror( const drawSurf_t *drawSurf, int entityNum )
{
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	// rotate the plane if necessary
	if ( entityNum != REFENTITYNUM_WORLD )
	{
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		// get the orientation of the entity
		R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.orientation );

		// rotate the plane, but keep the non-rotated version for matching
		// against the portalSurface entities
		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.orientation.origin );

		// translate the original plane
		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.orientation.origin );
	}
	else
	{
		plane = originalPlane;
	}

	// locate the portal entity closest to this plane.
	// origin will be the origin of the portal, origin2 will be
	// the origin of the camera
	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) 
	{
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		// if the entity is just a mirror, don't use as a camera point
		if ( e->e.oldorigin[0] == e->e.origin[0] && 
			e->e.oldorigin[1] == e->e.origin[1] && 
			e->e.oldorigin[2] == e->e.origin[2] ) 
		{
			return qtrue;
		}

		return qfalse;
	}
	return qfalse;
}


/*
** SurfIsOffscreen
**
** Determines if a surface is completely offscreen.
*/
static qboolean SurfIsOffscreen( const drawSurf_t *drawSurf, qboolean *isMirror ) {
	float shortest = 100000000;
	int entityNum;
	int numTriangles;
	shader_t *shader;
	int		fogNum;
	int dlighted;
	vec4_t clip, eye;
	int i;
	unsigned int pointAnd = (unsigned int)~0;

	*isMirror = qfalse;

	R_RotateForViewer();

	R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
	RB_BeginSurface( shader, fogNum );
	tess.allowVBO = qfalse;
#ifdef USE_TESS_NEEDS_NORMAL
	tess.needsNormal = qtrue;
#endif
	rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );

	for ( i = 0; i < tess.numVertexes; i++ )
	{
		int j;
		unsigned int pointFlags = 0;

		R_TransformModelToClip( tess.xyz[i], tr.orientation.modelMatrix, tr.viewParms.projectionMatrix, eye, clip );

		for ( j = 0; j < 3; j++ )
		{
			if ( clip[j] >= clip[3] )
			{
				pointFlags |= (1 << (j*2));
			}
			else if ( clip[j] <= -clip[3] )
			{
				pointFlags |= ( 1 << (j*2+1));
			}
		}
		pointAnd &= pointFlags;
	}

	// trivially reject
	if ( pointAnd )
	{
		tess.numIndexes = 0;
		return qtrue;
	}

	// determine if this surface is backfaced and also determine the distance
	// to the nearest vertex so we can cull based on portal range.  Culling
	// based on vertex distance isn't 100% correct (we should be checking for
	// range to the surface), but it's good enough for the types of portals
	// we have in the game right now.
	numTriangles = tess.numIndexes / 3;

	for ( i = 0; i < tess.numIndexes; i += 3 )
	{
		vec3_t normal;
		float len;

		VectorSubtract( tess.xyz[tess.indexes[i]], tr.viewParms.orientation.origin, normal );

		len = VectorLengthSquared( normal );			// lose the sqrt
		if ( len < shortest )
		{
			shortest = len;
		}

		if ( DotProduct( normal, tess.normal[tess.indexes[i]] ) >= 0 )
		{
			numTriangles--;
		}
	}
	tess.numIndexes = 0;
	if ( !numTriangles )
	{
		return qtrue;
	}

	// mirrors can early out at this point, since we don't do a fade over distance
	// with them (although we could)
	if ( IsMirror( drawSurf, entityNum ) )
	{
		*isMirror = qtrue;
		return qfalse;
	}

	if ( shortest > (tess.shader->portalRange*tess.shader->portalRange) )
	{
		return qtrue;
	}

	return qfalse;
}


/*
================
R_GetModelViewBounds
================
*/
static void R_GetModelViewBounds( int *mins, int *maxs )
{
	float			minn[2];
	float			maxn[2];
	float			norm[2];
	float			mvp[16];
	float			dist[4];
	vec4_t			clip;
	int				i,j;

	minn[0] = minn[1] =  1.0;
	maxn[0] = maxn[1] = -1.0;

	// premultiply
	myGlMultMatrix( tr.orientation.modelMatrix, tr.viewParms.projectionMatrix, mvp );

	for ( i = 0; i < tess.numVertexes; i++ ) {
		R_TransformModelToClipMVP( tess.xyz[i], mvp, clip );
		if ( clip[3] <= 0.0 ) {
			dist[0] = DotProduct( tess.xyz[i], tr.viewParms.frustum[0].normal );
			dist[1] = DotProduct( tess.xyz[i], tr.viewParms.frustum[1].normal );
			dist[2] = DotProduct( tess.xyz[i], tr.viewParms.frustum[2].normal );
			dist[3] = DotProduct( tess.xyz[i], tr.viewParms.frustum[3].normal );
			if ( dist[0] <= tr.viewParms.frustum[0].dist ) maxn[0] =  1.0f;
			if ( dist[1] <= tr.viewParms.frustum[1].dist ) minn[0] = -1.0f;
			if ( dist[2] <= tr.viewParms.frustum[2].dist ) minn[1] = -1.0f;
			if ( dist[3] <= tr.viewParms.frustum[3].dist ) maxn[1] =  1.0f;
		} else {
			for ( j = 0; j < 2; j++ ) {
				if ( clip[j] >  clip[3] ) clip[j] =  clip[3]; else
				if ( clip[j] < -clip[3] ) clip[j] = -clip[3];
			}
			norm[0] = clip[0] / clip[3];
			norm[1] = clip[1] / clip[3];
			for ( j = 0; j < 2; j++ ) {
				if ( norm[j] < minn[j] ) minn[j] = norm[j];
				if ( norm[j] > maxn[j] ) maxn[j] = norm[j];
			}
		}
	}

	mins[0] = (int)(-0.5 + 0.5 * ( 1.0 + minn[0] ) * tr.viewParms.viewportWidth);
	mins[1] = (int)(-0.5 + 0.5 * ( 1.0 + minn[1] ) * tr.viewParms.viewportHeight);
	maxs[0] = (int)(0.5 + 0.5 * ( 1.0 + maxn[0] ) * tr.viewParms.viewportWidth);
	maxs[1] = (int)(0.5 + 0.5 * ( 1.0 + maxn[1] ) * tr.viewParms.viewportHeight);
}


/*
========================
R_MirrorViewBySurface

Returns qtrue if another view has been rendered
========================
*/
extern int r_numdlights;
static qboolean R_MirrorViewBySurface( const drawSurf_t *drawSurf, int entityNum ) {
	viewParms_t		newParms;
	viewParms_t		oldParms;
	orientation_t	surface, camera;
	qboolean		isMirror;

	// don't recursively mirror
	if ( tr.viewParms.portalView != PV_NONE ) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: recursive mirror/portal found\n" );
		return qfalse;
	}

//	if ( r_noportals->integer || r_fastsky->integer || tr.levelGLFog) {
	if ( r_noportals->integer > 1 /*|| r_fastsky->integer*/ ) {
		return qfalse;
	}

	// trivially reject portal/mirror
	if ( SurfIsOffscreen( drawSurf, &isMirror ) ) {
		return qfalse;
	}

	if ( !isMirror && r_noportals->integer ) {
		return qfalse;
	}

	// save old viewParms so we can return to it after the mirror view
	oldParms = tr.viewParms;

	newParms = tr.viewParms;
	newParms.portalView = PV_NONE;

	if ( !R_GetPortalOrientations( drawSurf, entityNum, &surface, &camera, 
		newParms.pvsOrigin, &newParms.portalView ) ) {
		return qfalse;		// bad portal, no portalentity
	}

#ifdef USE_PMLIGHT
	// create dedicated set for each view
	if ( r_numdlights + oldParms.num_dlights <= ARRAY_LEN( backEndData->dlights ) ) {
		int i;
		newParms.dlights = oldParms.dlights + oldParms.num_dlights;
		newParms.num_dlights = oldParms.num_dlights;
		r_numdlights += oldParms.num_dlights;
		for ( i = 0; i < oldParms.num_dlights; i++ )
			newParms.dlights[i] = oldParms.dlights[i];
	}
#endif

	if ( tess.numVertexes > 2 && r_fastsky->integer ) {
		int mins[2], maxs[2];
		R_GetModelViewBounds( mins, maxs );
		newParms.scissorX = newParms.viewportX + mins[0];
		newParms.scissorY = newParms.viewportY + mins[1];
		newParms.scissorWidth = maxs[0] - mins[0];
		newParms.scissorHeight = maxs[1] - mins[1];
	}

	R_MirrorPoint( oldParms.orientation.origin, &surface, &camera, newParms.orientation.origin );

	VectorSubtract( vec3_origin, camera.axis[0], newParms.portalPlane.normal );
	newParms.portalPlane.dist = DotProduct( camera.origin, newParms.portalPlane.normal );

	R_MirrorVector( oldParms.orientation.axis[0], &surface, &camera, newParms.orientation.axis[0] );
	R_MirrorVector( oldParms.orientation.axis[1], &surface, &camera, newParms.orientation.axis[1] );
	R_MirrorVector( oldParms.orientation.axis[2], &surface, &camera, newParms.orientation.axis[2] );

	// OPTIMIZE: restrict the viewport on the mirrored view

	// render the mirror view
	R_RenderView( &newParms );

	tr.viewParms = oldParms;

	return qtrue;
}


/*
=================
R_SpriteFogNum

See if a sprite is inside a fog volume
=================
*/
static int R_SpriteFogNum( const trRefEntity_t *ent ) {
	int				i, j;
	const fog_t		*fog;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}

	if ( ent->e.renderfx & RF_CROSSHAIR ) {
		return 0;
	}

	for ( i = 1 ; i < tr.world->numfogs ; i++ ) {
		fog = &tr.world->fogs[i];
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( ent->e.origin[j] - ent->e.radius >= fog->bounds[1][j] ) {
				break;
			}
			if ( ent->e.origin[j] + ent->e.radius <= fog->bounds[0][j] ) {
				break;
			}
		}
		if ( j == 3 ) {
			return i;
		}
	}

	return 0;
}

/*
==========================================================================================

DRAWSURF SORTING

==========================================================================================
*/

/*
===============
R_Radix
===============
*/
static void R_Radix( int byte, int size, const drawSurf_t *source, drawSurf_t *dest )
{
  int           count[ 256 ] = { 0 };
  int           index[ 256 ];
  int           i;
  unsigned char *sortKey;
  unsigned char *end;

  sortKey = ( (unsigned char *)&source[ 0 ].sort ) + byte;
  end = sortKey + ( size * sizeof( drawSurf_t ) );
  for( ; sortKey < end; sortKey += sizeof( drawSurf_t ) )
    ++count[ *sortKey ];

  index[ 0 ] = 0;

  for( i = 1; i < 256; ++i )
    index[ i ] = index[ i - 1 ] + count[ i - 1 ];

  sortKey = ( (unsigned char *)&source[ 0 ].sort ) + byte;
  for( i = 0; i < size; ++i, sortKey += sizeof( drawSurf_t ) )
    dest[ index[ *sortKey ]++ ] = source[ i ];
}


/*
===============
R_RadixSort

Radix sort with 4 byte size buckets
===============
*/
static void R_RadixSort( drawSurf_t *source, int size )
{
  static drawSurf_t scratch[ MAX_DRAWSURFS ];
#ifdef Q3_LITTLE_ENDIAN
  R_Radix( 0, size, source, scratch );
  R_Radix( 1, size, scratch, source );
  R_Radix( 2, size, source, scratch );
  R_Radix( 3, size, scratch, source );
#else
  R_Radix( 3, size, source, scratch );
  R_Radix( 2, size, scratch, source );
  R_Radix( 1, size, source, scratch );
  R_Radix( 0, size, scratch, source );
#endif //Q3_LITTLE_ENDIAN
}


#ifdef USE_PMLIGHT

typedef struct litSurf_tape_s {
	struct litSurf_s *first;
	struct litSurf_s *last;
	unsigned count;
} litSurf_tape_t;

// Philip Erdelsky gets all the credit for this one...

static void R_SortLitsurfs( dlight_t* dl )
{
	litSurf_tape_t tape[ 4 ];
	int				base;
	litSurf_t		*p;
	litSurf_t		*next;
	unsigned		block_size;
	litSurf_tape_t	*tape0;
	litSurf_tape_t	*tape1;
	int				dest;
	litSurf_tape_t	*output_tape;
	litSurf_tape_t	*chosen_tape;
	unsigned		n0, n1;

	// distribute the records alternately to tape[0] and tape[1]

	tape[0].count = tape[1].count = 0;
	tape[0].first = tape[1].first = NULL;

	base = 0;
	p = dl->head;

	while ( p ) {
		next = p->next;
		p->next = tape[base].first;
		tape[base].first = p;
		tape[base].count++;
		p = next;
		base ^= 1;
	}

	// merge from the two active tapes into the two idle ones
	// doubling the number of records and pingponging the tape sets as we go

	block_size = 1;
	for ( base = 0; tape[base+1].count; base ^= 2, block_size <<= 1 )
	{
		tape0 = tape + base;
		tape1 = tape + base + 1;
		dest = base ^ 2;

		tape[dest].count = tape[dest+1].count = 0;
		for (; tape0->count; dest ^= 1)
		{
			output_tape = tape + dest;
			n0 = n1 = block_size;

			while (1)
			{
				if (n0 == 0 || tape0->count == 0)
				{
					if (n1 == 0 || tape1->count == 0)
						break;
					chosen_tape = tape1;
					n1--;
				}
				else if (n1 == 0 || tape1->count == 0)
				{
					chosen_tape = tape0;
					n0--;
				}
				else if (tape0->first->sort > tape1->first->sort)
				{
					chosen_tape = tape1;
					n1--;
				}
				else
				{
					chosen_tape = tape0;
					n0--;
				}
				chosen_tape->count--;
				p = chosen_tape->first;
				chosen_tape->first = p->next;
				if (output_tape->count == 0)
					output_tape->first = p;
				else
					output_tape->last->next = p;
				output_tape->last = p;
				output_tape->count++;
			}
		}
	}

	if (tape[base].count > 1)
		tape[base].last->next = NULL;

	dl->head = tape[base].first;
}


/*
=================
R_AddLitSurf
=================
*/
void R_AddLitSurf( surfaceType_t *surface, shader_t *shader, int fogIndex )
{
	struct litSurf_s *litsurf;

	if ( tr.refdef.numLitSurfs >= ARRAY_LEN( backEndData->litSurfs ) )
		return;

	tr.pc.c_lit_surfs++;

	litsurf = &tr.refdef.litSurfs[ tr.refdef.numLitSurfs++ ];

	litsurf->sort = (shader->sortedIndex << QSORT_SHADERNUM_SHIFT) 
		| tr.shiftedEntityNum | ( fogIndex << QSORT_FOGNUM_SHIFT );
	litsurf->surface = surface;

	if ( !tr.light->head )
		tr.light->head = litsurf;
	if ( tr.light->tail )
		tr.light->tail->next = litsurf;

	tr.light->tail = litsurf;
	tr.light->tail->next = NULL;
}


/*
=================
R_DecomposeLitSort
=================
*/
void R_DecomposeLitSort( unsigned sort, int *entityNum, shader_t **shader, int *fogNum ) {
	*fogNum = ( sort >> QSORT_FOGNUM_SHIFT ) & FOGNUM_MASK;
	*shader = tr.sortedShaders[ ( sort >> QSORT_SHADERNUM_SHIFT ) & SHADERNUM_MASK ];
	*entityNum = ( sort >> QSORT_REFENTITYNUM_SHIFT ) & REFENTITYNUM_MASK;
}

#endif // USE_PMLIGHT


//==========================================================================================

/*
=================
R_AddDrawSurf
=================
*/
void R_AddDrawSurf( surfaceType_t *surface, shader_t *shader, 
				   int fogIndex, int dlightMap ) {
	int			index;

	// instead of checking for overflow, we just mask the index
	// so it wraps around
	// we can't just wrap around here, because the surface count will be
	// off.  Check for overflow, and drop new surfaces on overflow.
	// ENSI NOTE Q3 doesn't check, ET does
	if ( tr.refdef.numDrawSurfs >= MAX_DRAWSURFS ) {
		return;
	}

	index = tr.refdef.numDrawSurfs & DRAWSURF_MASK;
	// the sort data is packed into a single 32 bit value so it can be
	// compared quickly during the qsorting process
	tr.refdef.drawSurfs[index].sort = (shader->sortedIndex << QSORT_SHADERNUM_SHIFT) 
		| tr.shiftedEntityNum | ( fogIndex << QSORT_FOGNUM_SHIFT ) | (int)dlightMap;
	tr.refdef.drawSurfs[index].surface = surface;
	tr.refdef.numDrawSurfs++;
}


/*
=================
R_DecomposeSort
=================
*/
void R_DecomposeSort( unsigned sort, int *entityNum, shader_t **shader, 
					 int *fogNum, int *dlightMap ) {
	*fogNum = ( sort >> QSORT_FOGNUM_SHIFT ) & FOGNUM_MASK;
	*shader = tr.sortedShaders[ ( sort >> QSORT_SHADERNUM_SHIFT ) & SHADERNUM_MASK ];
	*entityNum = ( sort >> QSORT_REFENTITYNUM_SHIFT ) & REFENTITYNUM_MASK;
	*dlightMap = sort & DLIGHT_MASK;
}


/*
=================
R_SortDrawSurfs
=================
*/
static void R_SortDrawSurfs( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader;
	int				fogNum;
	int				entityNum;
	int				dlighted;
	int				i;

	// it is possible for some views to not have any surfaces
	if ( numDrawSurfs < 1 ) {
		// we still need to add it for hyperspace cases
		R_AddDrawSurfCmd( drawSurfs, numDrawSurfs );
		return;
	}

	// sort the drawsurfs by sort type, then orientation, then shader
	R_RadixSort( drawSurfs, numDrawSurfs );

	// check for any pass through drawing, which
	// may cause another view to be rendered first
	for ( i = 0 ; i < numDrawSurfs ; i++ ) {
		R_DecomposeSort( (drawSurfs+i)->sort, &entityNum, &shader, &fogNum, &dlighted );

		if ( shader->sort > SS_PORTAL ) {
			break;
		}

		// no shader should ever have this sort type
		if ( shader->sort == SS_BAD ) {
			ri.Error (ERR_DROP, "Shader '%s'with sort == SS_BAD", shader->name );
		}

		// if the mirror was completely clipped away, we may need to check another surface
		if ( R_MirrorViewBySurface( (drawSurfs+i), entityNum) ) {
			// this is a debug option to see exactly what is being mirrored
			if ( r_portalOnly->integer ) {
				return;
			}
			if ( r_fastsky->integer == 0 ) {
				break;	// only one mirror view at a time
			}
		}
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer ) 
#endif
	{
		dlight_t *dl;
		// all the lit surfaces are in a single queue
		// but each light's surfaces are sorted within its subsection
		for ( i = 0; i < tr.refdef.num_dlights; ++i ) { 
			dl = &tr.refdef.dlights[ i ];
			if ( dl->head ) {
				R_SortLitsurfs( dl );
			}
		}
	}
#endif // USE_PMLIGHT

	R_AddDrawSurfCmd( drawSurfs, numDrawSurfs );
}


/*
=============
R_AddEntitySurfaces
=============
*/
void R_AddEntitySurfaces( void ) {
	trRefEntity_t	*ent;
	shader_t		*shader;

	if ( !r_drawentities->integer ) {
		return;
	}

	for ( tr.currentEntityNum = 0;
			tr.currentEntityNum < tr.refdef.num_entities;
			tr.currentEntityNum++ ) {
		ent = tr.currentEntity = &tr.refdef.entities[tr.currentEntityNum];
#ifdef USE_LEGACY_DLIGHTS
		ent->needDlights = 0;
#endif
		// preshift the value we are going to OR into the drawsurf sort
		tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

		//
		// the weapon model must be handled special --
		// we don't want the hacked weapon position showing in 
		// mirrors, because the true body position will already be drawn
		//
		if ( (ent->e.renderfx & RF_FIRST_PERSON) && (tr.viewParms.portalView != PV_NONE) ) {
			continue;
		}

		// simple generated models, like sprites and beams, are not culled
		switch ( ent->e.reType ) {
		case RT_PORTALSURFACE:
			break;		// don't draw anything
		case RT_SPRITE:
		case RT_SPLASH:
		case RT_BEAM:
		case RT_LIGHTNING:
		case RT_RAIL_CORE:
		case RT_RAIL_CORE_TAPER:
		case RT_RAIL_RINGS:
			// self blood sprites, talk balloons, etc should not be drawn in the primary
			// view.  We can't just do this check for all entities, because md3
			// entities may still want to cast shadows from them
			if ( (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE) ) {
				continue;
			}
			shader = R_GetShaderByHandle( ent->e.customShader );
			R_AddDrawSurf( &entitySurface, shader, R_SpriteFogNum( ent ), 0 );
			break;

		case RT_MODEL:
			// we must set up parts of tr.or for model culling
			R_RotateForEntity( ent, &tr.viewParms, &tr.orientation );

			tr.currentModel = R_GetModelByHandle( ent->e.hModel );
			if (!tr.currentModel) {
				R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
			} else {
				switch ( tr.currentModel->type ) {
				case MOD_MESH:
					R_AddMD3Surfaces( ent );
					break;
					// Ridah
				case MOD_MDC:
					R_AddMDCSurfaces( ent );
					break;
					// done.
				case MOD_MDS:
					R_AddAnimSurfaces( ent );
					break;
				case MOD_MDM:
					R_MDM_AddAnimSurfaces( ent );
					break;
				case MOD_IQM:
					R_AddIQMSurfaces( ent );
					break;
				case MOD_BRUSH:
					R_AddBrushModelSurfaces( ent );
					break;
				case MOD_BAD:		// null model axis
					if ( (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE) ) {
						break;
					}
					shader = R_GetShaderByHandle( ent->e.customShader );
					R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
					break;
				default:
					ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad modeltype" );
					break;
				}
			}
			break;
		default:
			ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad reType" );
		}
	}

}


/*
====================
R_GenerateDrawSurfs
====================
*/
void R_GenerateDrawSurfs( void ) {
	// set the projection matrix (and view frustum) here
	// first with max or fog distance so we can have proper
	// arbitrary frustum farplane culling optimization
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL || tr.world == NULL ) {
		VectorSet( tr.viewParms.visBounds[ 0 ], MIN_WORLD_COORD, MIN_WORLD_COORD, MIN_WORLD_COORD );
		VectorSet( tr.viewParms.visBounds[ 1 ], MAX_WORLD_COORD, MAX_WORLD_COORD, MAX_WORLD_COORD );
	} else
	{
		VectorCopy( tr.world->nodes->mins, tr.viewParms.visBounds[ 0 ] );
		VectorCopy( tr.world->nodes->maxs, tr.viewParms.visBounds[ 1 ] );
	}

	R_SetupProjection(&tr.viewParms, r_zproj->value, qtrue);

	R_CullDecalProjectors();
/*#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
	if ( !r_dlightMode->integer )
#endif // USE_PMLIGHT
		R_CullDlights();
#endif // USE_LEGACY_DLIGHTS*/

	R_AddWorldSurfaces();

	R_AddPolygonSurfaces();

	R_AddPolygonBufferSurfaces();

	// set the projection matrix with the minimum zfar
	// now that we have the world bounded
	// this needs to be done before entities are
	// added, because they use the projection
	// matrix for lod calculation
	R_SetupProjection( &tr.viewParms, r_zproj->value, qtrue );

	// we know the size of the clipping volume. Now set the rest of the projection matrix.
	R_SetupProjectionZ( &tr.viewParms );

	R_AddEntitySurfaces();
}


/*
================
R_RenderView

A view may be either the actual camera view,
or a mirror / remote location
================
*/
void R_RenderView( const viewParms_t *parms ) {
	int		firstDrawSurf;
	int		numDrawSurfs;

	if ( parms->viewportWidth <= 0 || parms->viewportHeight <= 0 ) {
		return;
	}

	// Ridah, purge media that were left over from the last level
	if ( r_cache->integer ) {
		extern void R_PurgeBackupImages( int purgeCount );
		static int lastTime;

		if ( ( lastTime > tr.refdef.time ) || ( lastTime < ( tr.refdef.time - 200 ) ) ) {
			R_FreeImageBuffer();    // clear all image buffers
			R_PurgeShaders( 10 );
			R_PurgeBackupImages( 1 );
			R_PurgeModels( 1 );
			lastTime = tr.refdef.time;
		}
	}
	// done.

	tr.viewCount++;

	tr.viewParms = *parms;
	tr.viewParms.frameSceneNum = tr.frameSceneNum;
	tr.viewParms.frameCount = tr.frameCount;

	firstDrawSurf = tr.refdef.numDrawSurfs;

	////tr.viewCount++; // disabled in q3e?

	// set viewParms.world
	R_RotateForViewer();

	// ydnar: removed this because we do a full frustum/projection setup
	// based on world bounds zfar or fog bounds
	// R_SetupFrustum ();

	R_GenerateDrawSurfs();

	// if we overflowed MAX_DRAWSURFS, the drawsurfs
	// wrapped around in the buffer and we will be missing
	// the first surfaces, not the last ones
	numDrawSurfs = tr.refdef.numDrawSurfs;
	if ( numDrawSurfs > MAX_DRAWSURFS ) {
		numDrawSurfs = MAX_DRAWSURFS;
	}

	R_SortDrawSurfs( tr.refdef.drawSurfs + firstDrawSurf, numDrawSurfs - firstDrawSurf );

	// draw main system development information (surface outlines, etc)
	/*R_FogOff();
	R_DebugGraphics();
	R_FogOn();*/

}
