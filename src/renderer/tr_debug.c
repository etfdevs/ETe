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
#include "../renderercommon/simplex.h"	// line font definition

/*
==============================================================================

Debug line, point and text rendering.

Vertices are accumulated in a small scratch buffer and drawn with
qglDrawArrays(), replacing the legacy qglBegin()/qglEnd() paths.

==============================================================================
*/

#define MAX_DEBUG_LINE_VERTS	512

typedef struct {
	vec3_t      xyz[MAX_DEBUG_LINE_VERTS];
	color4ub_t  colors[MAX_DEBUG_LINE_VERTS];
	int         numVerts;
	GLenum      mode;
} debugLineBuffer_t;

static debugLineBuffer_t s_debugLine;

const color4ub_t debugWhite = { { 255, 255, 255, 255 } };
const color4ub_t debugGrey = { { 153, 153, 153, 255 } };
const color4ub_t debugMesh = { { 0, 0, 204, 255 } };
const color4ub_t debugOrange = { { 255, 102, 13, 89 } };
const color4ub_t debugWeight2 = { { 102, 102, 0, 255 } };
const color4ub_t debugWeight3 = { { 204, 102, 0, 255 } };
const color4ub_t debugWeight4 = { { 255, 102, 0, 255 } };

/*
==============
R_DebugLineFlush

Draws the accumulated debug primitives and resets the buffer
==============
*/
void R_DebugLineFlush( void ) {
	if ( !s_debugLine.numVerts ) {
		return;
	}

	GL_ClientState( 0, CLS_COLOR_ARRAY );

	qglVertexPointer( 3, GL_FLOAT, 0, s_debugLine.xyz[0] );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, s_debugLine.colors[0].rgba );

	qglDrawArrays( s_debugLine.mode, 0, s_debugLine.numVerts );

	GL_ClientState( 0, CLS_NONE );

	s_debugLine.numVerts = 0;
}

/*
==============
R_DebugVertex
==============
*/
static void R_DebugVertex( const vec3_t xyz, color4ub_t color, GLenum mode ) {
	if ( s_debugLine.numVerts && s_debugLine.mode != mode ) {
		R_DebugLineFlush();
	}

	if ( s_debugLine.numVerts >= MAX_DEBUG_LINE_VERTS ) {
		R_DebugLineFlush();
	}

	s_debugLine.mode = mode;
	VectorCopy( xyz, s_debugLine.xyz[s_debugLine.numVerts] );
	s_debugLine.colors[s_debugLine.numVerts] = color;
	s_debugLine.numVerts++;
}

/*
==============
R_DebugLine
==============
*/
void R_DebugLine( const vec3_t start, const vec3_t end, color4ub_t color ) {
	if ( s_debugLine.numVerts + 2 > MAX_DEBUG_LINE_VERTS ) {
		R_DebugLineFlush();
	}

	R_DebugVertex( start, color, GL_LINES );
	R_DebugVertex( end, color, GL_LINES );
}

/*
==============
R_DebugPoint
==============
*/
void R_DebugPoint( const vec3_t xyz, color4ub_t color ) {
	R_DebugVertex( xyz, color, GL_POINTS );
}

/*
==============================================================================

Debug text, drawn as a billboarded line font

==============================================================================
*/

#define DEBUG_TEXT_PIXEL_HEIGHT	14.0f	// desired on-screen glyph height
#define DEBUG_TEXT_GLYPH_HEIGHT	24.0f	// line font units for an uppercase glyph

/*
==============
R_DebugTextAxes

Builds the text axes in the same space as the geometry being debugged
==============
*/
static void R_DebugTextAxes( vec3_t axis[3] ) {
	const orientationr_t *or = &backEnd.orientation;
	float len[3];
	int i, j;

	for ( i = 0; i < 3; i++ ) {
		len[i] = DotProduct( or->axis[i], or->axis[i] );
		if ( len[i] <= 0.0f ) {
			len[i] = 1.0f;
		}
	}

	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			axis[i][j] = DotProduct( backEnd.viewParms.orientation.axis[i], or->axis[j] ) / len[j];
		}
	}
}

/*
==============
RB_DebugText

Draws world text using the line font, keeps a constant on-screen size and
does not depend on platform specific font display lists
==============
*/
void RB_DebugText( const vec3_t org, float r, float g, float b, const char *text, qboolean neverOcclude ) {
	vec3_t viewAxis[3];
	vec3_t start, p1, p2, delta;
	color4ub_t color;
	float scale, dist, spacing;
	int i, len, num, index, charIndex, line;

	if ( !text || !*text ) {
		return;
	}

	R_DebugTextAxes( viewAxis );

	// axis[1] points left, the line font advances to the right
	VectorNegate( viewAxis[1], viewAxis[1] );

	// keep the text readable at any distance
	VectorSubtract( org, backEnd.orientation.viewOrigin, delta );
	dist = VectorLength( delta );

	if ( backEnd.viewParms.projectionMatrix[5] > 0.0f && backEnd.viewParms.viewportHeight > 0 ) {
		scale = DEBUG_TEXT_PIXEL_HEIGHT * 2.0f * dist /
				( DEBUG_TEXT_GLYPH_HEIGHT * backEnd.viewParms.projectionMatrix[5] * backEnd.viewParms.viewportHeight );
	} else {
		scale = 0.5f;
	}

	scale = Com_ClampFloat( 0.01f, 2.0f, scale );

	color.rgba[0] = (byte)( Com_ClampFloat( 0.0f, 1.0f, r ) * 255.0f );
	color.rgba[1] = (byte)( Com_ClampFloat( 0.0f, 1.0f, g ) * 255.0f );
	color.rgba[2] = (byte)( Com_ClampFloat( 0.0f, 1.0f, b ) * 255.0f );
	color.rgba[3] = 255;

	GL_Bind( tr.whiteImage );

	qglLineWidth( 1 );

	if ( neverOcclude ) {
		qglDepthRange( 0, 0 );	// never occluded
	}

	VectorCopy( org, start );
	line = 0;
	len = strlen( text );

	for ( i = 0; i < len; i++ ) {
		if ( text[i] == '\n' ) {
			line++;
			VectorCopy( org, start );
			VectorMA( start, -line * DEBUG_TEXT_GLYPH_HEIGHT * 1.5f * scale, viewAxis[2], start );
			continue;
		}

		charIndex = text[i] - 32;
		if ( charIndex < 0 || charIndex >= NUM_SIMPLEX_CHARS ) {
			continue;
		}

		num = simplex[charIndex][0] * 2;
		spacing = simplex[charIndex][1];
		index = 2;

		while ( index - 2 < num ) {
			if ( simplex[charIndex][index] < 0 ) {
				index++;
				continue;
			}
			VectorCopy( start, p1 );
			VectorMA( p1, scale * simplex[charIndex][index], viewAxis[1], p1 );
			VectorMA( p1, scale * simplex[charIndex][index + 1], viewAxis[2], p1 );
			index += 2;
			if ( simplex[charIndex][index] < 0 ) {
				index++;
				continue;
			}
			VectorCopy( start, p2 );
			VectorMA( p2, scale * simplex[charIndex][index], viewAxis[1], p2 );
			VectorMA( p2, scale * simplex[charIndex][index + 1], viewAxis[2], p2 );

			R_DebugLine( p1, p2, color );
		}

		VectorMA( start, spacing * scale, viewAxis[1], start );
	}

	R_DebugLineFlush();

	if ( neverOcclude ) {
		qglDepthRange( 0, 1 );
	}
}
