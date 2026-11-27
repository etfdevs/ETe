#include "tr_local.h"
#include "tr_common.h"

#define COMMON_DEPTH_STENCIL
//#define DEPTH_RENDER_BUFFER
//#define USE_FBO_BLIT

// screenMap texture dimensions
#define SCR_WIDTH 128
#define SCR_HEIGHT 64

#define BLOOM_BASE 5
#define SSAO_BASE (BLOOM_BASE+(MAX_BLUR_PASSES*2))
#define FBO_COUNT (SSAO_BASE+2)

// r_ssao: samples per pixel, spread over a disc that's rotated per pixel
#define SSAO_SAMPLES 8

#if BLOOM_BASE < 2
#error no space for main/postprocess buffers
#endif

static GLuint programs[ PROGRAM_COUNT ];
static GLuint current_vp;
static GLuint current_fp;

static int programCompiled = 0;
static int programEnabled	= 0;
static qboolean ssaoCompiled = qfalse;

qboolean fboEnabled = qfalse;
qboolean fboBloomInited = qfalse;
int      fboReadIndex = 0;
GLint    fboInternalFormat;
GLint    fboTextureFormat;
GLint    fboTextureType;
int      fboBloomPasses;
int      fboBloomBlendBase;
int      fboBloomFilterSize;

qboolean windowAdjusted;
int		blitX0, blitX1;
int		blitY0, blitY1;
int		blitClear;
GLenum	blitFilter;

qboolean superSampled;

typedef struct frameBuffer_s {
	GLuint fbo;
	GLuint color;			// renderbuffer if multisampled
	GLuint depthStencil;	// renderbuffer if multisampled
	GLint  width;
	GLint  height;
	qboolean multiSampled;
} frameBuffer_t;

#ifdef USE_FBO
static GLuint commonDepthStencil;

static frameBuffer_t frameBufferMS;
static frameBuffer_t frameBuffers[ FBO_COUNT ];

static qboolean frameBufferMultiSampling = qfalse;

qboolean blitMSfbo = qfalse;
#endif

#ifndef GL_TEXTURE_IMAGE_FORMAT
#define GL_TEXTURE_IMAGE_FORMAT 0x828F
#endif

#ifndef GL_TEXTURE_IMAGE_TYPE
#define GL_TEXTURE_IMAGE_TYPE 0x8290
#endif

extern void RB_SetGL2D( void );

qboolean GL_ProgramAvailable( void )
{
	return (programCompiled != 0);
}


static void ARB_ProgramDisable( void )
{
	if ( current_vp )
		qglDisable( GL_VERTEX_PROGRAM_ARB );
	if ( current_fp )
		qglDisable( GL_FRAGMENT_PROGRAM_ARB );
	current_vp = 0;
	current_fp = 0;
	programEnabled = 0;
}


void GL_ProgramDisable( void )
{
	if ( programEnabled )
	{
		ARB_ProgramDisable();
	}
}


void ARB_ProgramEnableExt( GLuint vertexProgram, GLuint fragmentProgram )
{
	// Bind whatever program we are given. Gating this on programCompiled would
	// silently disable every ARB program whenever the dlight/FBO program set
	// fails to compile, including the VBO programs, which are compiled and
	// managed independently of it.
	//
	// The current bindings are part of the condition, not just the arguments:
	// ARB_ProgramEnableExt( 0, 0 ) still has to disable a program that is bound,
	// or it leaks into the following fixed-function drawing.
	if ( vertexProgram || fragmentProgram || current_vp || current_fp )
	{
		if ( current_vp != vertexProgram ) {
			current_vp = vertexProgram;
			if ( current_vp ) {
				qglEnable( GL_VERTEX_PROGRAM_ARB );
				qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, current_vp );
			} else {
				qglDisable( GL_VERTEX_PROGRAM_ARB );
			}
		}

		if ( current_fp != fragmentProgram ) {
			current_fp = fragmentProgram;
			if ( current_fp ) {
				qglEnable( GL_FRAGMENT_PROGRAM_ARB );
				qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, current_fp );
			} else {
				qglDisable( GL_FRAGMENT_PROGRAM_ARB );
			}
		}
		programEnabled = ( current_vp || current_fp ) ? 1 : 0;
	}
}


static void ARB_ProgramEnable( programNum vp, programNum fp )
{
	ARB_ProgramEnableExt( programs[ vp ], programs[ fp ] );
}


void GL_ProgramEnable( void )
{
	ARB_ProgramEnable( DUMMY_VERTEX, SPRITE_FRAGMENT );
}


#ifdef USE_PMLIGHT
static void ARB_Lighting( const shaderStage_t* pStage, int bundle )
{
	const dlight_t* dl;
	byte clipBits[ SHADER_MAX_VERTEXES ];
	unsigned hitIndexes[ SHADER_MAX_INDEXES ];
	int numIndexes;
	int clip;
	int i;
	qboolean isDirected;
	float radius;
	
	backEnd.pc.c_lit_vertices_lateculltest += tess.numVertexes;

	dl = tess.light;
	isDirected = (dl->flags & REF_DIRECTED_DLIGHT) != 0;
	radius = R_GetDlightRadius( dl );

	for ( i = 0; i < tess.numVertexes; ++i ) {
		vec3_t dist;

		// directed lights have no position: dl->transformed is a direction here,
		// so there is nothing meaningful to clip individual vertices against
		if ( isDirected ) {
			clipBits[ i ] = 0;
			continue;
		}

		VectorSubtract( dl->transformed, tess.xyz[i], dist );

		if ( tess.surfType != SF_GRID ) {
			if ( !isDirected && DotProduct( dist, tess.normal[i] ) <= 0.0f ) {
				clipBits[ i ] = 63;
				continue;
			}
		}

		clip = 0;
		if ( dist[0] > radius ) {
			clip |= 1;
		} else if ( dist[0] < -radius ) {
			clip |= 2;
		}
		if ( dist[1] > radius ) {
			clip |= 4;
		} else if ( dist[1] < -radius ) {
			clip |= 8;
		}
		if ( dist[2] > radius ) {
			clip |= 16;
		} else if ( dist[2] < -radius ) {
			clip |= 32;
		}

		clipBits[i] = clip;
	}

	// build a list of triangles that need light
	numIndexes = 0;

	for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
		int		a, b, c;

		a = tess.indexes[i];
		b = tess.indexes[i+1];
		c = tess.indexes[i+2];
		if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
			continue;	// not lighted
		}
		hitIndexes[numIndexes] = a;
		hitIndexes[numIndexes+1] = b;
		hitIndexes[numIndexes+2] = c;
		numIndexes += 3;
	}

	backEnd.pc.c_lit_indices_latecull_in += numIndexes;
	backEnd.pc.c_lit_indices_latecull_out += tess.numIndexes - numIndexes;

	if ( !numIndexes )
		return;

	if ( tess.dlightWorldBlend ) {
		// r_dlightMode 0 world surfaces: scale the already-lit framebuffer
		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
	} else if ( tess.shader->sort < SS_OPAQUE ) {
		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
	}

	GL_SelectTexture( 0 );

	R_FogOff();
	R_BindAnimatedImage( &pStage->bundle[ bundle ] );
	
	R_DrawElements( numIndexes, hitIndexes );
	R_FogOn();
}


static void ARB_Lighting_Fast( const shaderStage_t* pStage, int bundle )
{
	if ( !tess.numIndexes )
		return;

	if ( tess.dlightWorldBlend ) {
		// r_dlightMode 0 world surfaces: scale the already-lit framebuffer
		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
	} else if ( tess.shader->sort < SS_OPAQUE ) {
		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
	}

	GL_SelectTexture( 0 );

	R_FogOff();
	R_BindAnimatedImage( &pStage->bundle[ bundle ] );
	
	R_DrawElements( tess.numIndexes, tess.indexes );
	R_FogOn();
}


void ARB_SetupLightParams( void )
{
	programNum vertexProgram;
	programNum fragmentProgram;
	const fogProgramParms_t *fp;
	qboolean fogPass;
	const dlight_t *dl;
	vec3_t lightRGB;
	float radius;
	float intensity;

	tess.dlightUpdateParams = qfalse;
	tess.cullType = tess.shader->cullType;

	if ( !programCompiled )
		return;

	dl = tess.light;

	R_GetDlightColor( dl, lightRGB );
	if ( !glConfig.deviceSupportsGamma && !fboEnabled )
		VectorScale( lightRGB, 2 * powf( r_intensity->value, r_gamma->value ), lightRGB );

	radius = R_GetDlightRadius( dl );
	intensity = R_GetDlightIntensity( dl );

	fogPass = ( tess.fogNum && tess.shader->fogPass );
	if ( fogPass && ((tr.refdef.rdflags & RDF_SNOOPERVIEW) || tess.shader->noFog || !r_wolffog->integer ) )
		fogPass = qfalse;
	fp = NULL;

	vertexProgram = DLIGHT_VERTEX;

	if ( dl->linear ) {
		fragmentProgram = (tess.shader->cullType == CT_TWO_SIDED) ? DLIGHT_LINEAR_ABS_FRAGMENT : DLIGHT_LINEAR_FRAGMENT;
	} else if ( dl->flags & REF_DIRECTED_DLIGHT ) {
		if ( tess.dlightWorldBlend ) {
			fragmentProgram = (tess.shader->cullType == CT_TWO_SIDED) ? DLIGHT_DIRECTIONAL_WORLD_ABS_FRAGMENT : DLIGHT_DIRECTIONAL_WORLD_FRAGMENT;
		} else {
			fragmentProgram = (tess.shader->cullType == CT_TWO_SIDED) ? DLIGHT_DIRECTIONAL_ENTITY_ABS_FRAGMENT : DLIGHT_DIRECTIONAL_ENTITY_FRAGMENT;
		}
	} else {
		fragmentProgram = (tess.shader->cullType == CT_TWO_SIDED) ? DLIGHT_ABS_FRAGMENT : DLIGHT_FRAGMENT;
	}

	if ( fogPass ) {
		fp = RB_CalcFogProgramParms();
		// switch to fog programs
		if ( fp->level ) {
			vertexProgram += 2;
		} else {
			vertexProgram += 1;
		}
		++fragmentProgram;
	}

	ARB_ProgramEnable( vertexProgram, fragmentProgram );

	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, lightRGB[0], lightRGB[1], lightRGB[2], 1.0f / Square( radius ) );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 1, intensity, intensity, intensity, intensity );

	if ( dl->linear )
	{
		vec3_t ab;
		VectorSubtract( dl->transformed2, dl->transformed, ab );
		//qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 2, dl->transformed[0], dl->transformed[1], dl->transformed[2], 0 );
		//qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 3, dl->transformed2[0], dl->transformed2[1], dl->transformed2[2], 0 );
		qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 4, ab[0], ab[1], ab[2], 1.0f / DotProduct( ab, ab ) );
	}
	else if ( dl->flags & REF_DIRECTED_DLIGHT )
	{
		// Pass the directional normal (already rotated into the current
		// entity/view space by R_TransformDlights) and intensity
		qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 2, dl->transformed[0], dl->transformed[1], dl->transformed[2], intensity );
	}

	qglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 0, backEnd.orientation.viewOrigin[0], backEnd.orientation.viewOrigin[1], backEnd.orientation.viewOrigin[2], 0 );
	qglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 1, dl->transformed[0], dl->transformed[1], dl->transformed[2], 0 );

	if ( fogPass )
	{
		GL_BindTexture( 1, tr.fogImage->texnum );

		qglProgramLocalParameter4fvARB( GL_VERTEX_PROGRAM_ARB, 2, fp->fogDistanceVector );
		if ( !fp->level ) 
		{
			qglProgramLocalParameter4fvARB( GL_VERTEX_PROGRAM_ARB, 3, fp->fogDepthVector );
			qglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 4, fp->eyeT, fp->eyeInside, 0.0f, 0.0f );
		}

		GL_SelectTexture( 0 );
	}
}


void ARB_LightingPass( void )
{
	const shaderStage_t* pStage;
	int stage, bundle;

	stage = R_GetDlightStage( tess.shader, &bundle );
	if ( stage < 0 )
		return;

	// we may need to update programs for fog transitions
	if ( tess.dlightUpdateParams )
		ARB_SetupLightParams();

	RB_DeformTessGeometry();

	GL_Cull( tess.shader->cullType );

	// set polygon offset if necessary
	if ( tess.shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	pStage = tess.xstages[ stage ];

	R_ComputeTexCoords( 0, &pStage->bundle[ bundle ] );

	GL_ClientState( 1, CLS_NONE );
	GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_NORMAL_ARRAY );

	// since this is guaranteed to be a single pass, fill and lock all the arrays

	qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoordPtr[0] );
	qglNormalPointer( GL_FLOAT, sizeof( tess.normal[0] ), tess.normal );
	qglVertexPointer( 3, GL_FLOAT, sizeof( tess.xyz[0] ), tess.xyz );

	if ( qglLockArraysEXT )
		qglLockArraysEXT( 0, tess.numVertexes );

	// CPU may limit performance in following cases
	if ( tess.light->linear || gl_version >= 40 )
		ARB_Lighting_Fast( pStage, bundle );
	else
		ARB_Lighting( pStage, bundle );

	if ( qglUnlockArraysEXT )
		qglUnlockArraysEXT();

	// reset polygon offset
	if ( tess.shader->polygonOffset ) 
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
}
#endif // USE_PMLIGHT


const char *fogOutVPCode = {
	"PARAM fogDistanceVector = program.local[2]; \n"
	"PARAM fogDepthVector = program.local[3]; \n"
	"PARAM eyeT = program.local[4]; \n"
	"PARAM _01_32 = 0.03125; \n"
	"PARAM _30_32 = 0.93750; \n"
	"TEMP st; \n"
	
	// s = DotProduct( v, fogDistanceVector ) + fogDistanceVector[3];
	"DP3 st.x, fogDistanceVector, vertex.position; \n"	
	"ADD st.x, st.x, fogDistanceVector.w; \n"

	// t = DotProduct( v, fogDepthVector ) + fogDepthVector[3];
	"DP3 st.y, fogDepthVector, vertex.position; \n"	
	"ADD st.y, st.y, fogDepthVector.w; \n"

	// if ( t < 1.0 ) { t = 1.0/32; } else { t = 1.0/32 + 30.0/32 * t / ( t - eyeT ); }
	"SGE st.w, st.y, 1.0; \n"
	"SUB st.z, st.y, eyeT.x; \n"
	"RCP st.z, st.z; \n"
	"MUL st.z, st.z, st.y; \n"
	"MUL st.z, st.z, _30_32; \n"
	"MAD st.y, st.z, st.w, _01_32; \n"
	
	//"MOV st.z, {1.0}; \n"
	"MOV st.w, {1.0}; \n"

	"MOV result.texcoord[4], st; \n"
};


const char *fogVPCode = {
	"PARAM fogDistanceVector = program.local[2]; \n"
	"PARAM fogDepthVector = program.local[3]; \n"
	"PARAM eyeT = program.local[4]; \n"
	"TEMP st; \n"

	// s = DotProduct( v, fogDistanceVector ) + fogDistanceVector[3];
	"DP3 st.x, fogDistanceVector, vertex.position; \n"
	"ADD st.x, st.x, fogDistanceVector.w; \n"

	// t = DotProduct( v, fogDepthVector ) + fogDepthVector[3];
	"DP3 st.y, fogDepthVector, vertex.position; \n"
	"ADD st.y, st.y, fogDepthVector.w; \n"

	// t = t + eyeT.x * eyeT.y
	"MAD st.y, eyeT.x, eyeT.y, st.y; \n"

	//"MOV st.z, {1.0}; \n"
	"MOV st.w, {1.0}; \n"

	"MOV result.texcoord[4], st; \n"
};


const char *fogLevelVPCode = {
	"PARAM fogDistanceVector = program.local[2]; \n"
	"TEMP st; \n"

	// s = DotProduct( v, fogDistanceVector ) + fogDistanceVector[3];
	"DP3 st.x, fogDistanceVector, vertex.position; \n"
	"ADD st.x, st.x, fogDistanceVector.w; \n"

	// t = 1.0;
	"MOV st.y, 1.0; \n"

	//"MOV st.z, {1.0}; \n"
	"MOV st.w, {1.0}; \n"

	"MOV result.texcoord[4], st; \n"
};


#ifdef USE_PMLIGHT
static const char *dlightVP = {
	"!!ARBvp1.0 \n"
	"OPTION ARB_position_invariant; \n"
	"PARAM posEye = program.local[0]; \n"
	"PARAM posLight = program.local[1]; \n"
	"OUTPUT lv = result.texcoord[1]; \n" // 1
	"OUTPUT ev = result.texcoord[2]; \n" // 2
	"OUTPUT n = result.texcoord[3]; \n"  // 3
	"MOV result.texcoord[0], vertex.texcoord; \n" // 0
	"SUB lv, posLight, vertex.position; \n"
	"SUB ev, posEye, vertex.position; \n"
	"MOV n, vertex.normal; \n"
	"%s" // fog shader if needed
	"END \n"
};


static const char *ARB_BuildDlightFP( char *program, int programIndex )
{
	qboolean fog = qfalse;
	qboolean linear = qfalse;
	qboolean directional = qfalse;
	qboolean worldDirectional = qfalse;
	qboolean abslight = qfalse;

	program[0] = '\0';

	switch ( programIndex ) {
		case DLIGHT_FRAGMENT_FOG:
		case DLIGHT_ABS_FRAGMENT_FOG:
		case DLIGHT_LINEAR_FRAGMENT_FOG:
		case DLIGHT_LINEAR_ABS_FRAGMENT_FOG:
		case DLIGHT_DIRECTIONAL_ENTITY_FRAGMENT_FOG:
		case DLIGHT_DIRECTIONAL_ENTITY_ABS_FRAGMENT_FOG:
		case DLIGHT_DIRECTIONAL_WORLD_FRAGMENT_FOG:
		case DLIGHT_DIRECTIONAL_WORLD_ABS_FRAGMENT_FOG:
			fog = qtrue;
			break;
	}

	switch ( programIndex ) {
		case DLIGHT_LINEAR_FRAGMENT:
		case DLIGHT_LINEAR_FRAGMENT_FOG:
		case DLIGHT_LINEAR_ABS_FRAGMENT:
		case DLIGHT_LINEAR_ABS_FRAGMENT_FOG:
			linear = qtrue;
			break;
	}

	switch ( programIndex ) {
		case DLIGHT_DIRECTIONAL_ENTITY_FRAGMENT:
		case DLIGHT_DIRECTIONAL_ENTITY_FRAGMENT_FOG:
		case DLIGHT_DIRECTIONAL_ENTITY_ABS_FRAGMENT:
		case DLIGHT_DIRECTIONAL_ENTITY_ABS_FRAGMENT_FOG:
		case DLIGHT_DIRECTIONAL_WORLD_FRAGMENT:
		case DLIGHT_DIRECTIONAL_WORLD_FRAGMENT_FOG:
		case DLIGHT_DIRECTIONAL_WORLD_ABS_FRAGMENT:
		case DLIGHT_DIRECTIONAL_WORLD_ABS_FRAGMENT_FOG:
			directional = qtrue;
			break;
	}

	switch ( programIndex ) {
		case DLIGHT_DIRECTIONAL_WORLD_FRAGMENT:
		case DLIGHT_DIRECTIONAL_WORLD_FRAGMENT_FOG:
		case DLIGHT_DIRECTIONAL_WORLD_ABS_FRAGMENT:
		case DLIGHT_DIRECTIONAL_WORLD_ABS_FRAGMENT_FOG:
			worldDirectional = qtrue;
			break;
	}

	switch ( programIndex ) {
		case DLIGHT_ABS_FRAGMENT:
		case DLIGHT_ABS_FRAGMENT_FOG:
		case DLIGHT_LINEAR_ABS_FRAGMENT:
		case DLIGHT_LINEAR_ABS_FRAGMENT_FOG:
		case DLIGHT_DIRECTIONAL_ENTITY_ABS_FRAGMENT:
		case DLIGHT_DIRECTIONAL_ENTITY_ABS_FRAGMENT_FOG:
		case DLIGHT_DIRECTIONAL_WORLD_ABS_FRAGMENT:
		case DLIGHT_DIRECTIONAL_WORLD_ABS_FRAGMENT_FOG:
			abslight = qtrue;
			break;
	}

	strcat( program,
	"!!ARBfp1.0 \n"
	"OPTION ARB_precision_hint_fastest; \n"
	"PARAM lightRGB = program.local[0]; \n"
	"PARAM lightScale = program.local[1]; \n" );

	if ( directional ) {
		// .xyz = direction normal, .w = intensity
		strcat( program, "PARAM dirLV = program.local[2]; \n" );
	}

	//"PARAM lightRange2recip = program.local[1]; \n"
	//"PARAM fogColor = program.local[5]; \n" // fogColor

	strcat( program,
	"TEMP base, tmp; \n"
	"TEX base, fragment.texcoord[0], texture[0], 2D; \n" );

	if ( linear ) {
		strcat( program,
		"PARAM lightVector = program.local[4]; \n"
		"ATTRIB LV = fragment.texcoord[1]; \n"
		"TEMP dnLV; \n"
		// project fragment on light vector
		"DP3 tmp.w, -LV, lightVector; \n"
		"MUL_SAT tmp.x, tmp.w, lightVector.w; \n"
		// calculate light vector from projection point
		"MAD dnLV, lightVector, tmp.x, LV; \n"
		);
	} else if ( !directional ) {
		strcat( program, "ATTRIB dnLV = fragment.texcoord[1]; \n" );
	}

	strcat( program,
	"ATTRIB dnEV = fragment.texcoord[2]; \n"	// 2
	"ATTRIB n = fragment.texcoord[3]; \n" );    // 3

	if ( directional ) {
		strcat( program,
		// use the direction vector directly and normalize
		"TEMP lv; \n"
		"DP3 tmp.w, dirLV, dirLV; \n"
		"RSQ lv.w, tmp.w; \n"
		"MUL lv.xyz, dirLV, lv.w; \n"
		
		// directional lights bypass distance attenuation
		"TEMP light; \n"
		"MOV light, lightRGB; \n" );
	} else {
		strcat( program,
		// normalize light vector
		"TEMP lv; \n"
		"DP3 tmp.w, dnLV, dnLV; \n"
		"RSQ lv.w, tmp.w; \n"
		"MUL lv.xyz, dnLV, lv.w; \n"

		// calculate light intensity
		"TEMP light; \n"
		"MUL tmp.x, tmp.w, lightRGB.w; \n"
		"SUB tmp.x, {1.0}, tmp.x; \n"
		// discard blank fragments
		"KIL tmp.x; \n" );

		if ( !linear ) {
			// apply intensity and clamp the contribution like legacy dynamic
			// lights do; linear lights encode their strength in the radius
			strcat( program,
			"MUL tmp.x, tmp.x, lightScale.x; \n"
			"MIN tmp.x, tmp.x, {1.0}.x; \n" );
		}

		strcat( program,
		"MUL light, lightRGB, tmp.x; \n" ); // light.rgb
	}

	if ( r_dlightSpecColor->value > 0 )
		strcat( program, va( "PARAM specRGB = %1.2f; \n", r_dlightSpecColor->value ) );

	strcat( program, va( "PARAM specEXP = %1.2f; \n", r_dlightSpecPower->value ) );

	strcat( program,
	// normalize eye vector
	"TEMP ev; \n"
	"DP3 ev.w, dnEV, dnEV; \n"
	"RSQ ev.w, ev.w; \n"
	"MUL ev.xyz, dnEV, ev.w; \n"

	// normalize (eye + light) vector
	"ADD tmp, lv, ev; \n"
	"DP3 tmp.w, tmp, tmp; \n"
	"RSQ tmp.w, tmp.w; \n"
	"MUL tmp.xyz, tmp, tmp.w; \n" );

	// modulate specular strength
	if ( abslight ) {
		strcat( program,
		"DP3 tmp.w, n, tmp; \n"
		"ABS tmp.w, tmp.w; \n" );
	} else {
		strcat( program,
		"DP3_SAT tmp.w, n, tmp; \n" );
	}

	strcat( program,
	"POW tmp.w, tmp.w, specEXP.w; \n"
	"TEMP spec; \n" );

	if ( r_dlightSpecColor->value > 0 ) {
		// by constant
		strcat( program, "MUL spec, specRGB, tmp.w; \n" );
	} else {
		// by texture
		strcat( program, va( "MUL tmp.w, tmp.w, %1.2f; \n", -r_dlightSpecColor->value ) );
		strcat( program, "MUL spec, base, tmp.w; \n" );
	}

	// diffuse
	if ( directional ) {
		strcat( program,
		"TEMP bump; \n"
		"DP3 bump.w, n, lv; \n" );

		if ( abslight ) {
			strcat( program,
			// make sure that light and eye vectors are on the same plane side
			"DP3 tmp.w, n, ev; \n"
			"MUL tmp.w, tmp.w, bump.w; \n"
			"KIL tmp.w; \n"
			"ABS bump.w, bump.w; \n" );
		}
		
		strcat( program,
		"MUL tmp.x, dirLV.w, {0.125}.x; \n" // remainder = intensity * 0.125
		"MAD bump.w, bump.w, dirLV.w, tmp.x; \n" // modulate = dot * intensity + remainder
		"MAX bump.w, bump.w, {0.0}.x; \n" // clamp to 0
		"MIN bump.w, bump.w, {1.0}.x; \n" ); // and to 1, like legacy dlights
	} else {
		if ( abslight ) {
			strcat( program,
			"TEMP bump; \n"
			"DP3 bump.w, n, lv; \n"
			// make sure that light and eye vectors are on the same plane side
			"DP3 tmp.w, n, ev; \n"
			"MUL tmp.w, tmp.w, bump.w; \n"
			"KIL tmp.w; \n"
			"ABS bump.w, bump.w; \n" );
		} else {
			strcat( program,
			"TEMP bump; \n"
			"DP3_SAT bump.w, n, lv; \n" );
		}
	}

	if ( worldDirectional ) {
		// r_dlightMode 0 world-directed surfaces scale the already-lit
		// framebuffer instead of adding the diffuse texture again; the blend
		// state uses DST_COLOR for this path so the result is
		// dst * (1 + light * modulate). Model/bmodel directed lights stay on
		// the additive pmlight path below.
		if ( fog ) {
			strcat( program,
			"TEMP fog; \n"
			"TEX fog, fragment.texcoord[4], texture[1], 2D; \n" // fog texture
			"SUB fog.a, {1.0}, fog.a; \n"
			"MUL bump.w, bump.w, fog.a; \n" );
		}

		strcat( program,
		"MUL_SAT result.color, light, bump.w; \n"
		"END \n" );
	} else {
		strcat( program, "MAD base, base, bump.w, spec; \n" );

		if ( fog ) {
			strcat( program,
			"TEMP fog; \n"
			"TEX fog, fragment.texcoord[4], texture[1], 2D; \n" // fog texture
			//"MUL fog, fog, fogColor; \n"
			// blend with fog
			//"LRP_SAT base, fog.a, fog, base; \n"
			// modulate by inverted fog alpha
			"SUB fog.a, {1.0}, fog.a; \n"
			"MUL base, base, fog.a; \n" );
		}

		strcat( program,
		"MUL_SAT result.color, base, light; \n"
		"END \n" );
	}
	
	r_dlightSpecColor->modified = qfalse;
	r_dlightSpecPower->modified = qfalse;

	return program;
}

#endif // USE_PMLIGHT


static const char *dummyVP = {
	"!!ARBvp1.0 \n"
	"OPTION ARB_position_invariant; \n"
	"MOV result.texcoord[0], vertex.texcoord; \n"
	"END \n" 
};


static const char *spriteFP = {
	"!!ARBfp1.0 \n"
	"OPTION ARB_precision_hint_fastest; \n"
	"TEMP base; \n"
	"TEX base, fragment.texcoord[0], texture[0], 2D; \n"
	"TEMP test; \n"
	"SUB test.a, base.a, 0.85; \n"
	"KIL test.a; \n"
	"MOV base, 0.0; \n"
	"MOV result.color, base; \n"
	"MOV result.depth, fragment.position.z; \n"
	"END \n"
};


#ifdef USE_FBO
static char *ARB_BuildGreyscaleProgram( char *buf ) {
	char *s;

	if ( r_greyscale->value == 0 ) {
		*buf = '\0';
		return buf;
	}

	s = Q_stradd( buf, "PARAM sRGB = { 0.2126, 0.7152, 0.0722, 1.0 }; \n" );

	if ( r_greyscale->value == 1.0 ) {
		Q_stradd( s, "DP3 base.xyz, base, sRGB; \n"  );
	} else {
		s = Q_stradd( s, "TEMP luma; \n" );
		s = Q_stradd( s, "DP3 luma, base, sRGB; \n" );
		/*s +=*/ sprintf( s, "LRP base.xyz, %1.2f, luma, base; \n", r_greyscale->value );
	}

	return buf;
}


// r_toneMap, r_toneMapExposure, r_color*, r_vignette: final-pass colour grading.
// Values are baked in (CVG_RENDERER recompiles on change); defaults add no code at all.
static char *ARB_BuildFinalColorProgram( char *buf ) {
	const float exposure = r_toneMapExposure->value;
	const float contrast = r_colorContrast->value;
	const float saturation = r_colorSaturation->value;
	const float warmth = r_colorWarmth->value * 0.1f;
	const float vignette = r_vignette->value;
	char *s = buf;

	*s = '\0';

	if ( r_toneMap->integer ) {
		// the curves work on roughly linear colour (base^2), times exposure
		s = Q_stradd( s, "TEMP gA, gB, gC; \n" "MUL gA, base, base; \n" );
		s = Q_stradd( s, va( "MUL gA, gA, %1.4f; \n", exposure ) );
		switch ( r_toneMap->integer ) {
		default:
			// 1: ACES filmic (Narkowicz fit): deeper shadows, fuller mid-tones, highlights roll off
			s = Q_stradd( s,
				"MAD gB, gA, 2.51, 0.03; \n"
				"MUL gB, gB, gA; \n"
				"MAD gC, gA, 2.43, 0.59; \n"
				"MAD gC, gC, gA, 0.14; \n"
				"RCP gC.x, gC.x; \n"
				"RCP gC.y, gC.y; \n"
				"RCP gC.z, gC.z; \n"
				"MUL_SAT gA, gB, gC; \n" );
			break;
		case 2:
			// AgX (Sobotka; minimal fit by Wrensch): log2 encoding in an inset gamut, sigmoid, outset;
			// natural hues, gentle highlights. Its output is already display encoded
			s = Q_stradd( s,
				"MUL gB.xyz, { 0.842479, 0.042328, 0.042376, 0.0 }, gA.x; \n"
				"MAD gB.xyz, { 0.078434, 0.878469, 0.078434, 0.0 }, gA.y, gB; \n"
				"MAD gB.xyz, { 0.079224, 0.079166, 0.879143, 0.0 }, gA.z, gB; \n"
				"LG2 gC.x, gB.x; \n"
				"LG2 gC.y, gB.y; \n"
				"LG2 gC.z, gB.z; \n"
				"MAX gC.xyz, gC, -12.47393; \n"
				"MIN gC.xyz, gC, 4.026069; \n"
				"MAD gC.xyz, gC, 0.0606061, 0.7559958; \n" // ( ev - min ) / ( max - min )
				"MAD gB.xyz, gC, 15.5, -40.14; \n"
				"MAD gB.xyz, gB, gC, 31.96; \n"
				"MAD gB.xyz, gB, gC, -6.868; \n"
				"MAD gB.xyz, gB, gC, 0.4298; \n"
				"MAD gB.xyz, gB, gC, 0.1191; \n"
				"MAD gB.xyz, gB, gC, -0.00232; \n"
				"MUL gA.xyz, { 1.196879, -0.052897, -0.052972, 0.0 }, gB.x; \n"
				"MAD gA.xyz, { -0.098021, 1.151903, -0.098043, 0.0 }, gB.y, gA; \n"
				"MAD gA.xyz, { -0.099030, -0.098961, 1.151074, 0.0 }, gB.z, gA; \n"
				"MOV_SAT base.xyz, gA; \n" );
			break;
		case 3:
			// Khronos PBR Neutral: colours stay as authored, only highlights above 0.76 are compressed
			s = Q_stradd( s,
				"MIN gB.x, gA.x, gA.y; \n"
				"MIN gB.x, gB.x, gA.z; \n"
				"MUL gB.y, gB.x, gB.x; \n"
				"MAD gB.y, gB.y, -6.25, gB.x; \n"
				"SUB gB.z, gB.x, 0.08; \n"
				"CMP gB.x, gB.z, gB.y, 0.04; \n" // offset
				"SUB gA.xyz, gA, gB.x; \n"
				"MAX gB.x, gA.x, gA.y; \n"
				"MAX gB.x, gB.x, gA.z; \n" // peak
				"SUB gB.y, gB.x, 0.52; \n"
				"RCP gB.y, gB.y; \n"
				"MAD gB.y, gB.y, -0.0576, 1.0; \n" // new peak
				"RCP gB.z, gB.x; \n"
				"MUL gB.z, gB.z, gB.y; \n"
				"MUL gC.xyz, gA, gB.z; \n"
				"SUB gB.w, gB.x, gB.y; \n"
				"MAD gB.w, gB.w, 0.15, 1.0; \n"
				"RCP gB.w, gB.w; \n"
				"MAD gB.w, gB.w, -1.0, 1.0; \n" // desaturation
				"LRP gC.xyz, gB.w, gB.y, gC; \n"
				"SUB gB.x, gB.x, 0.76; \n"
				"CMP gA.xyz, gB.x, gA, gC; \n"
				"MOV_SAT gA.xyz, gA; \n" );
			break;
		}
		if ( r_toneMap->integer != 2 ) {
			s = Q_stradd( s,
				"MOV gB.w, 0.5; \n" // POW needs a register exponent
				"POW base.x, gA.x, gB.w; \n"
				"POW base.y, gA.y, gB.w; \n"
				"POW base.z, gA.z, gB.w; \n" );
		}
	} else if ( exposure != 1.0f ) {
		s = Q_stradd( s, va( "MUL base.xyz, base, %1.4f; \n", sqrtf( exposure ) ) );
	}

	if ( contrast != 1.0f ) {
		s = Q_stradd( s, va( "MAD base.xyz, base, %1.4f, %1.4f; \n", contrast, 0.5f - 0.5f * contrast ) );
	}

	if ( saturation != 1.0f ) {
		s = Q_stradd( s, "TEMP gL; \n" "DP3 gL, base, { 0.2126, 0.7152, 0.0722, 0.0 }; \n" );
		s = Q_stradd( s, va( "LRP base.xyz, %1.4f, base, gL; \n", saturation ) );
	}

	if ( warmth != 0.0f ) {
		s = Q_stradd( s, va( "MUL base.xyz, base, { %1.4f, 1.0, %1.4f, 1.0 }; \n", 1.0f + warmth, 1.0f - warmth ) );
	}

	if ( vignette != 0.0f ) {
		// darken by distance from the centre, squared so the middle stays clean: corners 1 - vignette
		s = Q_stradd( s,
			"TEMP gV; \n"
			"SUB gV.xy, fragment.texcoord[0], 0.5; \n"
			"MUL gV.x, gV.x, gV.x; \n"
			"MAD gV.x, gV.y, gV.y, gV.x; \n"
			"MUL gV.x, gV.x, 2.0; \n"
			"MUL gV.x, gV.x, gV.x; \n" );
		s = Q_stradd( s, va( "MAD_SAT gV.x, gV.x, %1.4f, 1.0; \n", -vignette ) );
		s = Q_stradd( s, "MUL base.xyz, base, gV.x; \n" );
	}

	ARB_BuildGreyscaleProgram( s );

	return buf;
}


// r_sharpen: AMD FidelityFX contrast adaptive sharpening (CAS), 5-tap version, on the raw image before
// gamma. Neighbours are one output pixel away (program.local[2].xy = output size), so a supersampled
// image is sharpened at the scale it is shown at. Empty when off.
static const char *ARB_BuildSharpenProgram( void ) {
	static char buf[2048];
	const float sharpness = r_sharpen->value;
	char *s = buf;

	buf[0] = '\0';
	if ( sharpness <= 0.0f )
		return buf;

	s = Q_stradd( s,
		"PARAM gO = program.local[2]; \n"
		"TEMP cA, cX, cY, cT, cN, cS, cE, cW, cMn, cMx; \n"
		"RCP cA.x, gO.x; \n"
		"RCP cA.y, gO.y; \n"
		"MUL cX, cA, { 1.0, 0.0, 0.0, 0.0 }; \n"
		"MUL cY, cA, { 0.0, 1.0, 0.0, 0.0 }; \n"
		"ADD cT, fragment.texcoord[0], cY; \n"
		"TEX cN, cT, texture[0], 2D; \n"
		"SUB cT, fragment.texcoord[0], cY; \n"
		"TEX cS, cT, texture[0], 2D; \n"
		"ADD cT, fragment.texcoord[0], cX; \n"
		"TEX cE, cT, texture[0], 2D; \n"
		"SUB cT, fragment.texcoord[0], cX; \n"
		"TEX cW, cT, texture[0], 2D; \n"
		// per channel: amp = sqrt( sat( min( min, 1 - max ) / max ) ), less where contrast is already high
		"MIN cMn, cN, cS; \n"
		"MIN cMn, cMn, cE; \n"
		"MIN cMn, cMn, cW; \n"
		"MIN cMn, cMn, base; \n"
		"MAX cMx, cN, cS; \n"
		"MAX cMx, cMx, cE; \n"
		"MAX cMx, cMx, cW; \n"
		"MAX cMx, cMx, base; \n"
		"MAD cT, cMx, -1.0, 1.0; \n"
		"MIN cT, cMn, cT; \n"
		"ADD cMx, cMx, 0.0001; \n"
		"RCP cA.x, cMx.x; \n"
		"RCP cA.y, cMx.y; \n"
		"RCP cA.z, cMx.z; \n"
		"MUL_SAT cT, cT, cA; \n"
		"MOV cA.w, 0.5; \n"
		"POW cT.x, cT.x, cA.w; \n"
		"POW cT.y, cT.y, cA.w; \n"
		"POW cT.z, cT.z, cA.w; \n" );
	// negative lobe weight w = amp * -1 / lerp( 8, 5, sharpness ); out = ( centre + w * sum ) / ( 1 + 4w )
	s = Q_stradd( s, va( "MUL cT, cT, %1.5f; \n", -1.0f / ( 8.0f - 3.0f * sharpness ) ) );
	/*s = */ Q_stradd( s,
		"ADD cN, cN, cS; \n"
		"ADD cN, cN, cE; \n"
		"ADD cN, cN, cW; \n"
		"MAD cN, cN, cT, base; \n"
		"MAD cT, cT, 4.0, 1.0; \n"
		"RCP cA.x, cT.x; \n"
		"RCP cA.y, cT.y; \n"
		"RCP cA.z, cT.z; \n"
		"MUL base.xyz, cN, cA; \n" );

	return buf;
}


// output size for r_sharpen in the final pass programs
static void ARB_FinalPassParams( void )
{
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 2, blitX1 - blitX0, blitY1 - blitY0, 0.0f, 0.0f );
}


static const char *gammaFP = {
	"!!ARBfp1.0 \n"
	"OPTION ARB_precision_hint_fastest; \n"
	"PARAM gamma = program.local[0]; \n"
	"TEMP base; \n"
	"TEX base, fragment.texcoord[0], texture[0], 2D; \n"
	"%s" // r_sharpen
	"POW base.x, base.x, gamma.x; \n"
	"POW base.y, base.y, gamma.y; \n"
	"POW base.z, base.z, gamma.z; \n"
	"MUL base.xyz, base, gamma.w; \n"
	"%s" // for greyscale shader if needed
	"MOV base.w, 1.0; \n"
	"MOV_SAT result.color, base; \n"
	"END \n"
};

static char *ARB_BuildBloomProgram( char *buf ) {
	char *s = buf;

	s = Q_stradd( s,
		"!!ARBfp1.0 \n"
		"OPTION ARB_precision_hint_fastest; \n"
		"PARAM thres = program.local[0]; \n" // xyz: threshold, w: 1/(1-threshold) for subtractive modes
		"PARAM satv = program.local[1]; \n" // x: saturation of extracted color
		"TEMP base; \n"
		"TEX base, fragment.texcoord[0], texture[0], 2D; \n" );

	switch ( r_bloom_threshold_mode->integer ) {
	case 0:
		// (r|g|b) >= threshold
		s = Q_stradd( s,
			"TEMP minv; \n"
			"SGE minv, base, thres; \n"
			"DP3_SAT minv.w, minv, minv; \n"
			"MUL base.rgb, base, minv.w; \n" );
		break;
	case 1:
		// (r+g+b)/3 >= threshold
		s = Q_stradd( s,
			"PARAM scale = { 0.3333, 0.3334, 0.3333, 1.0 }; \n"
			"TEMP avg; \n"
			"DP3_SAT avg, base, scale; \n"
			"SGE avg.w, avg.x, thres.x; \n"
			"MUL base.rgb, base, avg.w; \n" );
		break;
	case 2:
		// luma(r,g,b) >= threshold
		s = Q_stradd( s,
			"PARAM luma2 = { 0.2126, 0.7152, 0.0722, 1.0 }; \n"
			"TEMP intensity2; \n"
			"DP3_SAT intensity2, base, luma2; \n"
			"SGE intensity2.w, intensity2.x, thres.x; \n"
			"MUL base.rgb, base, intensity2.w; \n" );
		break;
	case 3:
		// subtractive, per-channel: max(rgb - threshold, 0), normalized
		s = Q_stradd( s,
			"SUB_SAT base.rgb, base, thres; \n"
			"MUL_SAT base.rgb, base, thres.w; \n" );
		break;
	case 4:
		// soft knee, scaled by ((r+g+b)/3 - threshold) / (1 - threshold)
		s = Q_stradd( s,
			"PARAM scale4 = { 0.3333, 0.3334, 0.3333, 1.0 }; \n"
			"TEMP avg4; \n"
			"DP3_SAT avg4, base, scale4; \n"
			"SUB_SAT avg4.x, avg4.x, thres.x; \n"
			"MUL_SAT avg4.x, avg4.x, thres.w; \n"
			"MUL base.rgb, base, avg4.x; \n" );
		break;
	default:
		// 5: soft knee, scaled by (luma(r,g,b) - threshold) / (1 - threshold)
		s = Q_stradd( s,
			"PARAM luma5 = { 0.2126, 0.7152, 0.0722, 1.0 }; \n"
			"TEMP intensity5; \n"
			"DP3_SAT intensity5, base, luma5; \n"
			"SUB_SAT intensity5.x, intensity5.x, thres.x; \n"
			"MUL_SAT intensity5.x, intensity5.x, thres.w; \n"
			"MUL base.rgb, base, intensity5.x; \n" );
		break;
	}

	// modulation
	if ( r_bloom_modulate->integer ) {
		if ( r_bloom_modulate->integer == 1 ) {
			// by itself
			s = Q_stradd( s, "MUL base, base, base; \n" );
		} else {
			// by intensity
			s = Q_stradd( s,
				"PARAM lumaM = { 0.2126, 0.7152, 0.0722, 1.0 }; \n"
				"TEMP intensityM; \n"
				"DP3_SAT intensityM, base, lumaM; \n"
				"MUL base, base, intensityM; \n" );
		}
	}

	// desaturate extracted color: sat 1 = keep source color, sat 0 = luma
	s = Q_stradd( s,
		"PARAM lumaD = { 0.2126, 0.7152, 0.0722, 1.0 }; \n"
		"TEMP desat; \n"
		"DP3_SAT desat, base, lumaD; \n"
		"LRP base.rgb, satv.x, base, desat; \n"
		"MOV base.w, 1.0; \n"
		"MOV result.color, base; \n"
		"END \n" );

	return buf;
}


// Gaussian blur shader
static char *ARB_BuildBlurProgram( char *buf, int taps ) {
	int i;
	char *s = buf;

	*s = '\0';

	s = Q_stradd( s,
		"!!ARBfp1.0 \n"
		"OPTION ARB_precision_hint_fastest; \n"
		"ATTRIB tc = fragment.texcoord[0]; \n" );

	for ( i = 0; i < taps; i++ ) {
		s = Q_stradd( s, va( "PARAM p%i = program.local[%i]; \n", i, i ) ); // tex_offset_x, tex_offset_y, 0.0, weight
	}

	s = Q_stradd( s, "TEMP cc; \n"
		"MOV cc, {0.0, 0.0, 0.0, 1.0};\n" ); // initialize final color

	for ( i = 0; i < taps; i++ ) {
		s = Q_stradd( s, va( "TEMP c%i, tc%i; \n", i, i ) );
	}

	for ( i = 0; i < taps; i++ ) {
		s = Q_stradd( s, va( "ADD tc%i.xy, tc, p%i; \n", i, i ) );
	}

	for ( i = 0; i < taps; i++ ) {
		s = Q_stradd( s, va( "TEX c%i, tc%i, texture[0], 2D; \n", i, i ) );
		s = Q_stradd( s, va( "MAD cc, c%i, p%i.w, cc; \n", i, i ) ); // cc = cc + cN + pN.w
	}

	/*s = */ Q_stradd( s,
		"MOV cc.a, 1.0; \n"
		"MOV_SAT result.color, cc; \n"
		"END \n" );

	return buf;
}


static char *ARB_BuildBlendProgram( char *buf, int count ) {
	int i;
	char *s = buf;

	*s = '\0';
	s = Q_stradd( s, 
		"!!ARBfp1.0 \n"
		"OPTION ARB_precision_hint_fastest; \n"
		"ATTRIB tc = fragment.texcoord[0]; \n"
		"TEMP cx, cc;\n"
		"MOV cc, {0.0, 0.0, 0.0, 1.0}; \n" );

	for ( i = 0; i < count; i++ ) {
		s = Q_stradd( s, va( "TEX cx, fragment.texcoord[0], texture[%i], 2D; \n"
			"ADD cc, cx, cc; \n", i ) );
	}

	/*s = */ Q_stradd( s,
		"MOV cc.a, 1.0; \n"
		"MOV_SAT result.color, cc; \n"
		"END \n" );

	return buf;
}


// blend 2 texture together
static const char *blend2FP = {
	"!!ARBfp1.0 \n"
	"OPTION ARB_precision_hint_fastest; \n"
	"PARAM factor = program.local[1]; \n"
	"TEMP base; \n"
	"TEMP post; \n"
	"TEX base, fragment.texcoord[0], texture[0], 2D; \n"
	"TEX post, fragment.texcoord[0], texture[1], 2D; \n"
	"MAD base, post, factor.x, base; \n"
	//"ADD base, base, post; \n"
	"MOV base.w, 1.0; \n"
	"MOV_SAT result.color, base; \n"
	"END \n"
};

// combined blend + gamma correction pass
static const char *blend2gammaFP = {
	"!!ARBfp1.0 \n"
	"OPTION ARB_precision_hint_fastest; \n"
	"PARAM gamma = program.local[0]; \n"
	"PARAM factor = program.local[1]; \n"
	"TEMP base; \n"
	"TEMP post; \n"
	"TEX base, fragment.texcoord[0], texture[0], 2D; \n"
	"%s" // r_sharpen, before the bloom is added
	"TEX post, fragment.texcoord[0], texture[1], 2D; \n"
	//"ADD base, base, post; \n"
	"MAD base, post, factor.x, base; \n"
	"POW base.x, base.x, gamma.x; \n"
	"POW base.y, base.y, gamma.y; \n"
	"POW base.z, base.z, gamma.z; \n"
	"MUL base.xyz, base, gamma.w; \n"
	"%s" // for greyscale shader if needed
	"MOV base.w, 1.0; \n"
	"MOV_SAT result.color, base; \n"
	"END \n" 
};


// r_ssao helper: view-space position of the depth sample at texcoord uv into dst
// (x right, y up, z = distance along the view axis), uses temps tmp and dep
static char *ARB_SSAOPosition( char *s, const char *dst, const char *uv ) {
	s = Q_stradd( s, va( "TEX dep, %s, texture[0], 2D; \n", uv ) );
	s = Q_stradd( s,
		"MAD tmp.x, dep.x, 2.0, c0.x; \n"
		"RCP tmp.x, tmp.x; \n" );
	s = Q_stradd( s, va( "MUL %s.z, tmp.x, c0.y; \n", dst ) );
	s = Q_stradd( s, va( "SUB tmp.xy, %s, c1; \n", uv ) );
	s = Q_stradd( s,
		"MUL tmp.xy, tmp, c1.zwzw; \n"
		"SUB tmp.xy, tmp, 1.0; \n"
		"MUL tmp.xy, tmp, c0.zwzw; \n" );
	s = Q_stradd( s, va( "MUL %s.xy, tmp, %s.z; \n", dst, dst ) );
	return s;
}


// r_ssao occlusion pass: texture[0] = depth, texture[1] = 4x4 rotation noise
static char *ARB_BuildSSAOProgram( char *buf ) {
	char *s = buf;
	int i;

	*s = '\0';
	s = Q_stradd( s,
		"!!ARBfp1.0 \n"
		"PARAM c0 = program.local[0]; \n" // proj[10]-1, proj[14], 1/proj[0], 1/proj[5]
		"PARAM c1 = program.local[1]; \n" // viewport x, y, 2/width, 2/height, in texcoords
		"PARAM c2 = program.local[2]; \n" // disc radius in texcoords at distance 1 (x, y), 1/radius^2, strength/samples
		"PARAM c3 = program.local[3]; \n" // texel width, 0, 0, 0
		"PARAM c4 = program.local[4]; \n" // 0, texel height, 0, 0
		"PARAM c5 = program.local[5]; \n" // max disc radius in texcoords, cosine bias, depth hack limit, 4/fade distance
		"PARAM c6 = program.local[6]; \n" // -half depth texel (x, y): half-res pixel centres sit between depth texels,
		                                  // 1/distance where the normal's neighbours step one texel further out
		"ATTRIB tin = fragment.texcoord[0]; \n"
		"TEMP tc, tmp, dep, uv, pc, pq, da, db, gx, gy, nrm, rot, rad, vec, occ, dc, ox, oy; \n"
		"ADD tc, tin, c6; \n" );

	s = ARB_SSAOPosition( s, "pc", "tc" );
	// depth steps grow with distance squared, so take the normal's neighbours whole texels further out
	// with distance, keeping the depth rounding small against their spacing (else it shimmers far away)
	s = Q_stradd( s, "MOV dc, dep; \n"
		"MUL tmp.x, pc.z, c6.z; \n"
		"FLR tmp.x, tmp.x; \n"
		"MAX tmp.x, tmp.x, 1.0; \n"
		"MUL ox, c3, tmp.x; \n"
		"MUL oy, c4, tmp.x; \n" );

	// normal from the screen-space neighbours, picking the closer one on each axis so depth edges don't bend it
	s = Q_stradd( s, "ADD uv, tc, ox; \n" );
	s = ARB_SSAOPosition( s, "pq", "uv" );
	s = Q_stradd( s, "SUB da, pq, pc; \n" "SUB uv, tc, ox; \n" );
	s = ARB_SSAOPosition( s, "pq", "uv" );
	s = Q_stradd( s, "SUB db, pc, pq; \n"
		"ABS tmp.x, da.z; \n"
		"ABS tmp.y, db.z; \n"
		"SUB tmp.x, tmp.x, tmp.y; \n"
		"CMP gx, tmp.x, da, db; \n"
		"ADD uv, tc, oy; \n" );
	s = ARB_SSAOPosition( s, "pq", "uv" );
	s = Q_stradd( s, "SUB da, pq, pc; \n" "SUB uv, tc, oy; \n" );
	s = ARB_SSAOPosition( s, "pq", "uv" );
	s = Q_stradd( s, "SUB db, pc, pq; \n"
		"ABS tmp.x, da.z; \n"
		"ABS tmp.y, db.z; \n"
		"SUB tmp.x, tmp.x, tmp.y; \n"
		"CMP gy, tmp.x, da, db; \n"
		"XPD nrm, gx, gy; \n"
		"DP3 tmp.x, nrm, nrm; \n"
		"RSQ tmp.x, tmp.x; \n"
		"MUL nrm.xyz, nrm, tmp.x; \n"
		"DP3 tmp.x, nrm, pc; \n"
		"CMP nrm, tmp.x, nrm, -nrm; \n" // face the camera
		// per-pixel direction to mirror the sample disc about, repeats every 4 pixels
		"MUL tmp.xy, fragment.position, 0.25; \n"
		"TEX rot, tmp, texture[1], 2D; \n"
		"MAD rot, rot, 2.0, -1.0; \n"
		"MOV rot.zw, 0.0; \n"
		// disc radius in texcoords shrinks with distance
		"RCP tmp.x, pc.z; \n"
		"MUL rad.xy, c2, tmp.x; \n"
		"MIN rad.xy, rad, c5.x; \n"
		"MOV occ, 0.0; \n" );

	for ( i = 0; i < SSAO_SAMPLES; i++ ) {
		// golden-angle spiral evenly filling the unit disc
		const float r = sqrtf( ( i + 0.5f ) / SSAO_SAMPLES );
		const float a = i * 2.39996f;
		s = Q_stradd( s, va( "PARAM k%i = { %f, %f, 0.0, 0.0 }; \n", i, r * cosf( a ), r * sinf( a ) ) );
		s = Q_stradd( s, va(
			"DP3 tmp.x, k%i, rot; \n"
			"MUL tmp, rot, tmp.x; \n"
			"MAD tmp, tmp, -2.0, k%i; \n"
			"MAD uv.xy, tmp, rad, tc; \n", i, i ) );
		s = ARB_SSAOPosition( s, "pq", "uv" );
		// occlusion = cosine between normal and sample direction, faded out towards the radius
		s = Q_stradd( s,
			"SUB vec, pq, pc; \n"
			"DP3 tmp.y, vec, vec; \n"
			"ADD tmp.z, tmp.y, 0.0001; \n"
			"RSQ tmp.z, tmp.z; \n"
			"DP3 tmp.w, vec, nrm; \n"
			"MUL tmp.w, tmp.w, tmp.z; \n"
			"SUB_SAT tmp.w, tmp.w, c5.y; \n"
			"MAD_SAT tmp.z, tmp.y, -c2.z, 1.0; \n"
			"MAD occ.x, tmp.w, tmp.z, occ.x; \n" );
	}

	// no occlusion on depth-hacked first person weapons, faded out where depth precision runs out
	// (the sky needs no mask: its samples are all far beyond the radius)
	/*s = */ Q_stradd( s,
		"MAD_SAT occ.x, occ.x, -c2.w, 1.0; \n"
		"SLT tmp.x, dc.x, c5.z; \n"
		"MAD_SAT tmp.y, pc.z, c5.w, -3.0; \n"
		"MAX tmp.x, tmp.x, tmp.y; \n"
		"LRP occ.x, tmp.x, 1.0, occ.x; \n"
		"MOV result.color, occ.x; \n"
		"END \n" );

	return buf;
}


// r_ssao blur pass, 4x4 to match the noise period, skipping samples at other depths
// texture[0] = occlusion, texture[1] = depth
static char *ARB_BuildSSAOBlurProgram( char *buf ) {
	char *s = buf;
	int x, y;

	*s = '\0';
	s = Q_stradd( s,
		"!!ARBfp1.0 \n"
		"PARAM c0 = program.local[0]; \n" // proj[10]-1, proj[14]
		"PARAM c1 = program.local[1]; \n" // texel width, texel height, 1/depth tolerance (fraction of distance)
		"ATTRIB tc = fragment.texcoord[0]; \n"
		"TEMP tmp, dep, uv, occ, zc, sum; \n"
		"TEX dep, tc, texture[1], 2D; \n"
		"MAD tmp.x, dep.x, 2.0, c0.x; \n"
		"RCP tmp.x, tmp.x; \n"
		"MUL zc.x, tmp.x, c0.y; \n"
		"RCP zc.y, zc.x; \n"
		"MUL zc.y, zc.y, c1.z; \n"
		"MOV sum, 0.0; \n" );

	for ( y = -2; y < 2; y++ ) {
		for ( x = -2; x < 2; x++ ) {
			s = Q_stradd( s, va( "MAD uv.xy, { %i.0, %i.0, 0.0, 0.0 }, c1, tc; \n", x, y ) );
			s = Q_stradd( s,
				"TEX occ, uv, texture[0], 2D; \n"
				"TEX dep, uv, texture[1], 2D; \n"
				"MAD tmp.x, dep.x, 2.0, c0.x; \n"
				"RCP tmp.x, tmp.x; \n"
				"MUL tmp.x, tmp.x, c0.y; \n"
				"SUB tmp.x, tmp.x, zc.x; \n"
				"ABS tmp.x, tmp.x; \n"
				"MAD_SAT tmp.x, tmp.x, -zc.y, 1.0; \n"
				"MAD sum.x, occ.x, tmp.x, sum.x; \n"
				"ADD sum.y, sum.y, tmp.x; \n" );
		}
	}

	/*s = */ Q_stradd( s,
		"RCP tmp.x, sum.y; \n"
		"MUL tmp.x, sum.x, tmp.x; \n"
		"MOV result.color, tmp.x; \n"
		"END \n" );

	return buf;
}


static void RenderQuad( int w, int h )
{
	static const vec2_t t[4] = { {0.0, 1.0}, {1.0, 1.0}, {0.0, 0.0}, {1.0, 0.0} };
	static vec3_t v[4] = { { 0 } };
	
	v[1][0] = w;
	v[2][1] = h;
	v[3][0] = w;
	v[3][1] = h;

	GL_ClientState( 0, CLS_TEXCOORD_ARRAY );

	qglVertexPointer( 3, GL_FLOAT, 0, v );
	qglTexCoordPointer( 2, GL_FLOAT, 0, t );

	qglDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
}


static void ARB_BlurParams( int width, int height, int ksize, qboolean horizontal )
{
	static float weight[ MAX_FILTER_SIZE ];
	static int old_ksize = -1;

	static const float x_k[ MAX_FILTER_SIZE+1 ][ MAX_FILTER_SIZE + 1 ] = {
		// [1/weight], coeff.1, coeff.2, [...]
		{ 0 },
		{ 1.0/1, 1 },
		{ 1.0/2, 1, 1 },
	//	{ 1/4,   1, 2, 1 },
		{ 1.0/16,  5, 6, 5 },
		{ 1.0/8,   1, 3, 3, 1 },
		{ 1.0/16,  1, 4, 6, 4, 1 },
		{ 1.0/32,  1, 5, 10, 10, 5, 1 },
		{ 1.0/64,  1, 6, 15, 20, 15, 6, 1 },
		{ 1.0/128, 1, 7, 21, 35, 35, 21, 7, 1 },
		{ 1.0/256, 1, 8, 28, 56, 70, 56, 28, 8, 1 },
		{ 1.0/512, 1, 9, 36, 84, 126, 126, 84, 36, 9, 1 },
		{ 1.0/1024, 1, 10, 45, 120, 210, 252, 210, 120, 45, 10, 1 },
		{ 1.0/2048, 1, 11, 55, 165, 330, 462, 462, 330, 165, 55, 11, 1 },
		{ 1.0/4096, 1, 12, 66, 220, 495, 792, 924, 792, 495, 220, 66, 12, 1 },
		{ 1.0/8192, 1, 13, 78, 286, 715, 1287, 1716, 1716, 1287, 715, 286, 78, 13, 1 },
		{ 1.0/16384, 1, 14, 91, 364, 1001, 2002, 3003, 3432, 3003, 2002, 1001, 364, 91, 14, 1 },
		{ 1.0/32768, 1, 15, 105, 455, 1365, 3003, 5005, 6435, 6435, 5005, 3003, 1365, 455, 105, 15, 1 },
		{ 1.0/65536, 1, 16, 120, 560, 1820, 4368, 8008, 11440, 12870, 11440, 8008, 4368, 1820, 560, 120, 16, 1 },
		{ 1.0/131072, 1, 17, 136, 680, 2380, 6188, 12376, 19448, 24310, 24310, 19448, 12376, 6188, 2380, 680, 136, 17, 1 },
		{ 1.0/262144, 1, 18, 153, 816, 3060, 8568, 18564, 31824, 43758, 48620, 43758, 31824, 18564, 8568, 3060, 816, 153, 18, 1 },
		{ 1.0/524288, 1, 19, 171, 969, 3876, 11628, 27132, 50388, 75582, 92378, 92378, 75582, 50388, 27132, 11628, 3876, 969, 171, 19, 1 },

	};

	static const float x_o[ MAX_FILTER_SIZE+1 ][ MAX_FILTER_SIZE ] = {
		{ 0 },
		{ 0.0 },
		{ -0.5, 0.5 },
	//	{ -1.0, 0.0, 1.0 },
		{ -1.2f, 0.0, 1.2f },
		{ -1.5, -0.5, 0.5, 1.5 },
		{ -2.0, -1.0, 0.0, 1.0, 2.0 },
		{ -2.5, -1.5, -0.5, 0.5, 1.5, 2.5 },
		{ -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0 },
		{ -3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5 },
		{ -4.0, -3.0, -2.0, -1.0, 0.0, 1.0,	2.0, 3.0, 4.0 },
		{ -4.5, -3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5, 4.5 },
		{ -5.0, -4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0 },
		{ -5.5, -4.5, -3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5, 4.5, 5.5 },
		{ -6.0, -5.0, -4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 },
		{ -6.5, -5.5, -4.5, -3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5 },
		{ -7.0, -6.0, -5.0, -4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0 },
		{ -7.5, -6.5, -5.5, -4.5, -3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5 },
		{ -8.0, -7.0, -6.0, -5.0, -4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0 },
		{ -8.5, -7.5, -6.5, -5.5, -4.5, -3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5 },
		{ -9.0, -8.0, -7.0, -6.0, -5.0, -4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 },
		{ -9.5, -8.5, -7.5, -6.5, -5.5, -4.5, -3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5 },
	};

	const float *coeffs = x_k[ ksize ] + 1;
	const float *off = x_o[ ksize ];

	int i;
	float rsum;
	float texel_size_x;
	float texel_size_y;
	float offset[ MAX_FILTER_SIZE ][ 2 ]; // xy

	// texel size
	texel_size_x = 1.0 / (float) width;
	texel_size_y = 1.0 / (float) height;
	rsum = x_k[ ksize ][ 0 ];

	if ( old_ksize != ksize ) {
		old_ksize = ksize;
		for ( i = 0; i < ksize; i++ ) {
			weight[i] = coeffs[i] * rsum;
		}
	}

	// calculate texture offsets for lookup
	for ( i = 0; i < ksize; i++ ) {
		offset[i][0] = texel_size_x * off[i];
		offset[i][1] = texel_size_y * off[i];
	}

	if ( horizontal ) {
		// horizontal pass
		for (  i = 0; i < ksize; i++ )
			qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, i, offset[i][0], 0.0, 0.0, weight[i] );
	} else {
		// vertical pass
		for (  i = 0; i < ksize; i++ )
			qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, i, 0.0, offset[i][1], 0.0, weight[i] );
	}
}
#endif // USE_FBO


static void ARB_DeletePrograms( void )
{
	qglDeleteProgramsARB( ARRAY_LEN( programs ) - PROGRAM_BASE, programs + PROGRAM_BASE );
	Com_Memset( programs, 0, sizeof( programs ) );
	programCompiled = 0;
}


qboolean ARB_CompileProgram( programType ptype, const char *text, GLuint program )
{
	GLint errorPos;
	unsigned int errCode;
	int kind;

	if ( ptype == Fragment )
		kind = GL_FRAGMENT_PROGRAM_ARB;
	else
		kind = GL_VERTEX_PROGRAM_ARB;

	qglBindProgramARB( kind, program );
	qglProgramStringARB( kind, GL_PROGRAM_FORMAT_ASCII_ARB, strlen( text ), text );

	// Compiling binds a program behind ARB_ProgramEnableExt()'s back; drop the
	// cached binding so the next enable rebinds and re-enables properly.
	current_vp = 0;
	current_fp = 0;

	qglGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errorPos );
	if ( (errCode = qglGetError()) != GL_NO_ERROR || errorPos != -1 )
	{
		// we may receive error with active FBO but compiled programs will continue to work properly
		if ( (errCode == GL_INVALID_OPERATION && !fboEnabled) || errorPos != -1 )
		{
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW "%s Compile Error(%i,%i): %s\n" S_COLOR_CYAN "%s\n", (ptype == Fragment) ? "FP" : "VP",
				errCode, errorPos, qglGetString( GL_PROGRAM_ERROR_STRING_ARB ), text );
			qglBindProgramARB( kind, 0 );
			ARB_DeletePrograms();
			return qfalse;
		}
	}

	return qtrue;
}


#ifdef USE_FBO
// unlike ARB_CompileProgram, a failure here only turns the optional feature off
static qboolean ARB_CompileOptionalFP( const char *name, const char *text, GLuint program )
{
	GLint errorPos;

	qglGetError();
	qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, program );
	qglProgramStringARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen( text ), text );
	qglGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errorPos );
	if ( errorPos != -1 ) {
		ri.Printf( PRINT_WARNING, "%s is unavailable, FP compile error at %i: %s\n", name, errorPos, qglGetString( GL_PROGRAM_ERROR_STRING_ARB ) );
		qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, 0 );
		return qfalse;
	}
	return qtrue;
}
#endif


qboolean ARB_UpdatePrograms( void )
{
#ifdef USE_PMLIGHT
	const char *program;
	int i;
#endif
#if defined (USE_FBO) || defined (USE_PMLIGHT)
	char buf[16384]; // r_ssao programs need about 12K
#endif

	if ( !qglGenProgramsARB )
		return qfalse;

	if ( programCompiled ) // delete old programs
	{
		ARB_ProgramDisable();
		ARB_DeletePrograms();
	}

	qglGenProgramsARB( ARRAY_LEN( programs ) - PROGRAM_BASE, programs + PROGRAM_BASE );

#ifdef USE_PMLIGHT
	if ( !ARB_CompileProgram( Vertex, va( dlightVP, "" ), programs[ DLIGHT_VERTEX ] ) )
		return qfalse;
	if ( !ARB_CompileProgram( Vertex, va( dlightVP, fogVPCode ), programs[ DLIGHT_VERTEX_FOG ] ) )
		return qfalse;
	if ( !ARB_CompileProgram( Vertex, va( dlightVP, fogLevelVPCode ), programs[ DLIGHT_VERTEX_FOG_LEVEL ] ) )
		return qfalse;

	for ( i = DLIGHT_FRAGMENT; i <= DLIGHT_DIRECTIONAL_WORLD_ABS_FRAGMENT_FOG; i++ ) {
		program = ARB_BuildDlightFP( buf, i );
		if ( !ARB_CompileProgram( Fragment, program, programs[ i ] ) ) {
			return qfalse;
		}
	}
#endif // USE_PMLIGHT

	if ( !ARB_CompileProgram( Vertex, dummyVP, programs[ DUMMY_VERTEX ] ) )
		return qfalse;

	if ( !ARB_CompileProgram( Fragment, spriteFP, programs[ SPRITE_FRAGMENT ] ) )
		return qfalse;

#ifdef USE_FBO
	if ( !ARB_CompileProgram( Fragment, va( gammaFP, ARB_BuildSharpenProgram(), ARB_BuildFinalColorProgram( buf ) ), programs[ GAMMA_FRAGMENT ] ) )
		return qfalse;

	if ( !ARB_CompileProgram( Fragment, ARB_BuildBloomProgram( buf ), programs[ BLOOM_EXTRACT_FRAGMENT ] ) )
		return qfalse;
	
	// only 1, 2, 3, 6, 8, 10, 12, 14, 16, 18 and 20 produces real visual difference
	fboBloomFilterSize = r_bloom_filter_size->integer;
	if ( !ARB_CompileProgram( Fragment, ARB_BuildBlurProgram( buf, fboBloomFilterSize ), programs[ BLUR_FRAGMENT ] ) )
		return qfalse;

	if ( !ARB_CompileProgram( Fragment, ARB_BuildBlurProgram( buf, 6 ), programs[ BLUR2_FRAGMENT ] ) )
		return qfalse;

	fboBloomBlendBase = r_bloom_blend_base->integer;
	if ( !ARB_CompileProgram( Fragment, ARB_BuildBlendProgram( buf, r_bloom_passes->integer - fboBloomBlendBase ), programs[ BLENDX_FRAGMENT ] ) )
		return qfalse;

	if ( !ARB_CompileProgram( Fragment, blend2FP, programs[ BLEND2_FRAGMENT ] ) )
		return qfalse;

	if ( !ARB_CompileProgram( Fragment, va( blend2gammaFP, ARB_BuildSharpenProgram(), ARB_BuildFinalColorProgram( buf ) ), programs[ BLEND2_GAMMA_FRAGMENT ] ) )
		return qfalse;

	ssaoCompiled = ARB_CompileOptionalFP( "r_ssao", ARB_BuildSSAOProgram( buf ), programs[ SSAO_FRAGMENT ] )
		&& ARB_CompileOptionalFP( "r_ssao", ARB_BuildSSAOBlurProgram( buf ), programs[ SSAO_BLUR_FRAGMENT ] );
#endif // USE_FBO

	programCompiled = 1;

	return qtrue;
}

#ifdef USE_FBO

static void FBO_Bind( GLuint target, GLuint buffer );

void FBO_Clean( frameBuffer_t *fb )
{
	if ( fb->fbo )
	{
		FBO_Bind( GL_FRAMEBUFFER, fb->fbo );
		if ( fb->multiSampled )
		{
			qglBindRenderbuffer( GL_RENDERBUFFER, 0 );
			if ( fb->color )
			{
				qglDeleteRenderbuffers( 1, &fb->color );
				fb->color = 0;
			}
			if ( fb->depthStencil )
			{
				qglDeleteRenderbuffers( 1, &fb->depthStencil );
				fb->depthStencil = 0;
			}
		}
		else
		{
			GL_BindTexture( 0, 0 );
			if ( fb->color )
			{
				qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0 );
				qglDeleteTextures( 1, &fb->color );
				fb->color = 0;
			}
			if ( fb->depthStencil )
			{
#ifdef DEPTH_RENDER_BUFFER
				if ( fb->depthStencil && fb->depthStencil != commonDepthStencil )
				{
					qglDeleteRenderbuffers( 1, &fb->depthStencil );
					fb->depthStencil = 0;
				}
#else
				if ( glConfig.stencilBits == 0 )
					qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0 );
				else
					qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0 );

				if ( fb->depthStencil && fb->depthStencil != commonDepthStencil )
				{
					qglDeleteTextures( 1, &fb->depthStencil );
					fb->depthStencil = 0;
				}
#endif
			}
		}
		FBO_Bind( GL_FRAMEBUFFER, 0 );
		qglDeleteFramebuffers( 1, &fb->fbo );
		fb->fbo = 0;
	}
	Com_Memset( fb, 0, sizeof( *fb ) );
}


static void FBO_CleanBloom( void )
{
	int i;
	for ( i = 0; i < MAX_BLUR_PASSES; i++ )
	{
		FBO_Clean( &frameBuffers[ i * 2 + BLOOM_BASE + 0 ] );
		FBO_Clean( &frameBuffers[ i * 2 + BLOOM_BASE + 1 ] );
	}
}


static void FBO_CleanDepth( void )
{
#ifdef COMMON_DEPTH_STENCIL
	if ( commonDepthStencil )
	{
#ifdef DEPTH_RENDER_BUFFER
		qglDeleteRenderbuffers( 1, &commonDepthStencil );
#else
		GL_BindTexture( 0, 0 );
		qglDeleteTextures( 1, &commonDepthStencil );
#endif
		commonDepthStencil = 0;
	}
#endif
}


static GLuint FBO_CreateDepthTextureOrBuffer( GLsizei width, GLsizei height )
{
#ifdef DEPTH_RENDER_BUFFER
	GLuint buffer;
	qglGenRenderbuffers( 1, &buffer );
	qglBindRenderbuffer( GL_RENDERBUFFER, buffer );
	if ( glConfig.stencilBits == 0 )
		qglRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, width, height );
	else
		qglRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height );
	return buffer;
#else
	GLuint tex;
	qglGenTextures( 1, &tex );
	GL_BindTexture( 0, tex );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	// r_ssao samples it, don't wrap around at screen edges
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_clamp_mode );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_clamp_mode );
	if ( glConfig.stencilBits == 0 )
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL );
	else
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL );
	return tex;
#endif
}


static const char *glDefToStr( GLint define )
{
	#define CASE_STR(x) case (x): return #x
	static int index;
	static char buf[8][32];
	char *s;

	switch ( define )
	{
		// texture formats
		CASE_STR(GL_BGR);
		CASE_STR(GL_BGRA);
		CASE_STR(GL_RGB);
		CASE_STR(GL_RGBA);
		CASE_STR(GL_RGBA4);
		CASE_STR(GL_RGBA8);
		CASE_STR(GL_RGBA12);
		CASE_STR(GL_RGBA16);
		CASE_STR(GL_RGB10_A2);
		CASE_STR(GL_R11F_G11F_B10F);
		// data types
		CASE_STR(GL_BYTE);
		CASE_STR(GL_UNSIGNED_BYTE);
		CASE_STR(GL_SHORT);
		CASE_STR(GL_UNSIGNED_SHORT);
		CASE_STR(GL_INT);
		CASE_STR(GL_UNSIGNED_INT);
		CASE_STR(GL_FLOAT);
		CASE_STR(GL_DOUBLE);
		CASE_STR(GL_UNSIGNED_SHORT_4_4_4_4);
		CASE_STR(GL_UNSIGNED_INT_8_8_8_8);
		CASE_STR(GL_UNSIGNED_INT_10_10_10_2);
		CASE_STR(GL_UNSIGNED_SHORT_4_4_4_4_REV);
		CASE_STR(GL_UNSIGNED_INT_8_8_8_8_REV);
		CASE_STR(GL_UNSIGNED_INT_2_10_10_10_REV);
		CASE_STR(GL_UNSIGNED_NORMALIZED);
		// error codes
		CASE_STR(GL_NO_ERROR);
		CASE_STR(GL_INVALID_ENUM);
		CASE_STR(GL_INVALID_VALUE);
		CASE_STR(GL_INVALID_OPERATION);
		CASE_STR(GL_STACK_OVERFLOW);
		CASE_STR(GL_STACK_UNDERFLOW);
		CASE_STR(GL_OUT_OF_MEMORY);
		// fbo error codes
		CASE_STR(GL_FRAMEBUFFER_COMPLETE);
		CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
		CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
		CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
		CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
		CASE_STR(GL_FRAMEBUFFER_UNSUPPORTED);
	}
	s = buf[ index ]; // to handle multiple invocations as function parameters
	sprintf( s, "0x%04x", define );
	index = ( index + 1 ) & 7;
	return s;
}


static void getPreferredFormatAndType( GLint format, GLint *pFormat, GLint *pType )
{
	GLint preferredFormat;
	GLint preferredType;

	if ( qglGetInternalformativ && gl_version >= 43 ) {
		qglGetInternalformativ( GL_TEXTURE_2D, /*GL_RGBA8*/ format, GL_TEXTURE_IMAGE_FORMAT, 1, &preferredFormat );
		if ( qglGetError() != GL_NO_ERROR ) {
			goto __fallback;
		}
		qglGetInternalformativ( GL_TEXTURE_2D, /*GL_RGBA8*/ format, GL_TEXTURE_IMAGE_TYPE, 1, &preferredType );
		if ( qglGetError() != GL_NO_ERROR ) {
			goto __fallback;
		}
		if ( preferredFormat == 0 ) // nVidia ION drivers can do that
			preferredFormat = GL_RGBA;
		if ( preferredType == GL_UNSIGNED_NORMALIZED ) { // Intel HD 530 drivers can do that as well
			if ( format == GL_RGBA12 || format == GL_RGBA16 )
				preferredType = GL_UNSIGNED_SHORT;
			else
				preferredType = GL_UNSIGNED_BYTE;
		}
	} else {
__fallback:
		if ( format == GL_RGBA12 || format == GL_RGBA16 ) {
			preferredFormat = GL_RGBA;
			preferredType = GL_UNSIGNED_SHORT;
		} else {
			preferredFormat = GL_RGBA;
			preferredType = GL_UNSIGNED_BYTE;
		}
	}

	*pFormat = preferredFormat;
	*pType = preferredType;
}


static qboolean FBO_Create( frameBuffer_t *fb, GLsizei width, GLsizei height, qboolean depthStencil, GLint *outFormat, GLint *outType )
{
	int fboStatus;
	GLint internalFormat;
	GLint textureFormat;
	GLint textureType;

	fb->multiSampled = qfalse;
	fb->depthStencil = 0;

	// color texture
	qglGenTextures( 1, &fb->color );
	GL_BindTexture( 0, fb->color );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_clamp_mode );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_clamp_mode );

	// always use GL_RGB10_A2 for bloom textures which is fast as usual GL_RGBA8
	// (GL_R11F_G11F_B10F is a bit slower at least on AMD GPUs)
	// but can provide better precision for blurring, also we barely need more than 10 bits for that,
	// texture formats that doesn't fit into 32bits are just performance-killers for bloom
	if ( fb - frameBuffers >= BLOOM_BASE )
		internalFormat = GL_RGB10_A2;
	else
		internalFormat = fboInternalFormat;

	getPreferredFormatAndType( internalFormat, &textureFormat, &textureType );

	qglTexImage2D( GL_TEXTURE_2D, 0, internalFormat, width, height, 0, textureFormat, textureType, NULL );
	// TODO: handle GL_INVALID_OPERATION in case of unsupported internalFormat/textureFormat

	if ( outFormat )
		*outFormat = textureFormat;
	if ( outType )
		*outType = textureType;

	qglGenFramebuffers( 1, &fb->fbo );
	FBO_Bind( GL_FRAMEBUFFER, fb->fbo );

	qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->color, 0 );

	if ( depthStencil )
	{
#ifdef COMMON_DEPTH_STENCIL
		if ( !commonDepthStencil )
			commonDepthStencil = FBO_CreateDepthTextureOrBuffer( width, height );

		fb->depthStencil = commonDepthStencil;
#else
		fb->depthStencil = FBO_CreateDepthTextureOrBuffer( width, height );
#endif
#ifdef DEPTH_RENDER_BUFFER
		if ( glConfig.stencilBits == 0 )
			qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->depthStencil );
		else
			qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb->depthStencil );
#else
		if ( glConfig.stencilBits == 0 )
			qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fb->depthStencil, 0 );
		else
			qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, fb->depthStencil, 0 );
#endif
	}

	GL_BindTexture( 0, 0 );

	fboStatus = qglCheckFramebufferStatus( GL_FRAMEBUFFER );
	if ( fboStatus != GL_FRAMEBUFFER_COMPLETE )
	{
		ri.Printf( PRINT_ALL, "Failed to create %s (%s:%s) FBO (status %s, error %s)\n",
			glDefToStr( internalFormat ), glDefToStr( textureFormat ), glDefToStr( textureType ),
			glDefToStr( fboStatus ), glDefToStr( (int)qglGetError() ) );
		FBO_Clean( fb );
		return qfalse;
	}

	fb->width = width;
	fb->height = height;

	qglClearColor( 0.0, 0.0, 0.0, 1.0 );
	if ( depthStencil )
		qglClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
	else
		qglClear( GL_COLOR_BUFFER_BIT );

	FBO_Bind( GL_FRAMEBUFFER, 0 );

	return qtrue;
}


static qboolean FBO_CreateMS( frameBuffer_t *fb, int width, int height )
{
	GLsizei nSamples = r_ext_multisample->integer;
	int fboStatus;
	
	fb->multiSampled = qtrue;

	if ( nSamples <= 0 || !qglRenderbufferStorageMultisample )
	{
		return qfalse;
	}
	nSamples = PAD( nSamples, 2 );

	qglGenFramebuffers( 1, &fb->fbo );
	FBO_Bind( GL_FRAMEBUFFER, fb->fbo );

	qglGenRenderbuffers( 1, &fb->color );
	qglBindRenderbuffer( GL_RENDERBUFFER, fb->color );
	while ( nSamples > 0 ) {
		qglRenderbufferStorageMultisample( GL_RENDERBUFFER, nSamples, fboInternalFormat, width, height );
		if ( (int)qglGetError() == GL_INVALID_VALUE/* != GL_NO_ERROR */ ) {
			ri.Printf( PRINT_ALL, "...%ix MSAA is not available\n", nSamples );
			nSamples -= 2;
		} else {
			ri.Printf( PRINT_ALL, "...using %ix MSAA\n", nSamples );
			break;
		}
	}

	if ( nSamples <= 0 )
	{
		FBO_Clean( fb );
		return qfalse;
	}
	qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fb->color );

	qglGenRenderbuffers( 1, &fb->depthStencil );
	qglBindRenderbuffer( GL_RENDERBUFFER, fb->depthStencil );
	if ( glConfig.stencilBits == 0 )
		qglRenderbufferStorageMultisample( GL_RENDERBUFFER, nSamples, GL_DEPTH_COMPONENT32, width, height );
	else
		qglRenderbufferStorageMultisample( GL_RENDERBUFFER, nSamples, GL_DEPTH24_STENCIL8, width, height );

	if ( (int)qglGetError() != GL_NO_ERROR )
	{
		FBO_Clean( fb );
		return qfalse;
	}

	if ( glConfig.stencilBits == 0 )
		qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->depthStencil );
	else
		qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb->depthStencil );

	fboStatus = qglCheckFramebufferStatus( GL_FRAMEBUFFER );
	if ( fboStatus != GL_FRAMEBUFFER_COMPLETE )
	{
		ri.Printf( PRINT_WARNING, "Failed to create MS FBO (status %s, error %s)\n", glDefToStr( fboStatus ), glDefToStr( (int)qglGetError() ) );
		FBO_Clean( fb );
		return qfalse;
	}

	fb->width = width;
	fb->height = height;

	qglClearColor( 0.0, 0.0, 0.0, 1.0 );
	qglClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );

	FBO_Bind( GL_FRAMEBUFFER, 0 );

	return qtrue;
}


static qboolean FBO_CreateBloom( void )
{
	int width = glConfig.vidWidth;
	int height = glConfig.vidHeight;
	int i;

	fboBloomPasses = 0;

	if ( glConfig.numTextureUnits < r_bloom_passes->integer )
	{
		ri.Printf( PRINT_WARNING, "...not enough texture units (%i) for %i-pass bloom\n",
			glConfig.numTextureUnits, r_bloom_passes->integer );
		return qfalse;
	}

	for ( i = 0; i < r_bloom_passes->integer; i++ )
	{
		// we may need depth/stencil buffers for first bloom buffer in \r_bloom 2 mode
		if ( !FBO_Create( &frameBuffers[ i*2 + BLOOM_BASE + 0 ], width, height, i == 0 ? qtrue : qfalse, NULL, NULL ) ||
			!FBO_Create( &frameBuffers[ i*2 + BLOOM_BASE + 1 ], width, height, qfalse, NULL, NULL ) ) {
			return qfalse;
		}
		width = width / 2;
		height = height / 2;
		fboBloomPasses++;
		if ( width < 2 || height < 2 )
			break;
	}

	ri.Printf( PRINT_ALL, "...%i bloom passes\n", fboBloomPasses );

	return qtrue;
}


GLuint FBO_ScreenTexture( void )
{
	return frameBuffers[ 2 ].color;
}


static void FBO_Bind( GLuint target, GLuint buffer )
{
#if 1
	static GLuint draw_buffer = (GLuint)-1;
	static GLuint read_buffer = (GLuint)-1;
	if ( target == GL_FRAMEBUFFER ) {
		if ( draw_buffer != buffer || read_buffer != buffer )
			qglBindFramebuffer( GL_FRAMEBUFFER, buffer );
		draw_buffer = buffer;
		read_buffer = buffer;
	} else {
		if ( target == GL_READ_FRAMEBUFFER ) {
			if ( read_buffer != buffer )
				qglBindFramebuffer( GL_READ_FRAMEBUFFER, buffer );
			read_buffer = buffer;
		} else {
			if ( draw_buffer != buffer )
				qglBindFramebuffer( GL_DRAW_FRAMEBUFFER, buffer );
			draw_buffer = buffer;
		}
	}
#else
	qglBindFramebuffer( target, buffer );
#endif
}


void FBO_BindMain( void )
{
	if ( fboEnabled )
	{
		const frameBuffer_t *fb;
		if ( frameBufferMultiSampling )
		{
			blitMSfbo = qtrue;
			fb = &frameBufferMS;
		}
		else
		{
			blitMSfbo = qfalse;
			fb = &frameBuffers[ 0 ];
		}
		FBO_Bind( GL_FRAMEBUFFER, fb->fbo );
		fboReadIndex = 0;
	}
}


static void FBO_BlitToBackBuffer( int index )
{
	const frameBuffer_t *src = &frameBuffers[ index ];

	FBO_Bind( GL_READ_FRAMEBUFFER, src->fbo );
	FBO_Bind( GL_DRAW_FRAMEBUFFER, 0 );
	//qglReadBuffer( GL_COLOR_ATTACHMENT0 );
	qglDrawBuffer( GL_BACK );

	if ( windowAdjusted )
	{
		if ( blitClear > 0 )
		{
			blitClear--;
			qglClearColor( 0.0, 0.0, 0.0, 1.0 );
			qglClear( GL_COLOR_BUFFER_BIT );
		}
		qglViewport( blitX0, blitY0, blitX1 - blitX0, blitY1 - blitY0 );
		qglScissor( blitX0, blitY0, blitX1 - blitX0, blitY1 - blitY0 );
	}

	qglBlitFramebuffer( 0, 0, src->width, src->height, blitX0, blitY0, blitX1, blitY1, GL_COLOR_BUFFER_BIT, blitFilter );
	fboReadIndex = index;
}


void FBO_BlitSS( void )
{
	const frameBuffer_t *src = &frameBuffers[ fboReadIndex ];
	const frameBuffer_t *dst = &frameBuffers[ 4 ];

	FBO_Bind( GL_DRAW_FRAMEBUFFER, dst->fbo );
	
	qglBlitFramebuffer( 0, 0, src->width, src->height, 0, 0, dst->width, dst->height, GL_COLOR_BUFFER_BIT, GL_LINEAR );

	FBO_Bind( GL_READ_FRAMEBUFFER, dst->fbo );
}


void FBO_BlitMS( qboolean depthOnly )
{
	//if ( blitMSfbo )
	//{
	const int w = glConfig.vidWidth;
	const int h = glConfig.vidHeight;

	const frameBuffer_t *r = &frameBufferMS;
	const frameBuffer_t *d = &frameBuffers[ 0 ];

	fboReadIndex = 0;

	FBO_Bind( GL_READ_FRAMEBUFFER, r->fbo );
	FBO_Bind( GL_DRAW_FRAMEBUFFER, d->fbo );

	if ( depthOnly )
	{
		qglBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
		FBO_Bind( GL_READ_FRAMEBUFFER, d->fbo );
		return;
	}

	qglBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	// bind all further reads to main buffer
	FBO_Bind( GL_READ_FRAMEBUFFER, d->fbo );
}


static void FBO_Blur( const frameBuffer_t *fb1, const frameBuffer_t *fb2,  const frameBuffer_t *fb3 )
{
	const int w = glConfig.vidWidth;
	const int h = glConfig.vidHeight;

	qglViewport( 0, 0, fb1->width, fb1->height );

	// apply horizontal blur - render from FBO1 to FBO2
	FBO_Bind( GL_DRAW_FRAMEBUFFER, fb2->fbo );
	GL_BindTexture( 0, fb1->color );
	ARB_ProgramEnable( DUMMY_VERTEX, BLUR_FRAGMENT );
	ARB_BlurParams( fb1->width, fb1->height, fboBloomFilterSize, qtrue );
	RenderQuad( w, h );

	// apply vectical blur - render from FBO2 to FBO3
	FBO_Bind( GL_DRAW_FRAMEBUFFER, fb3->fbo );
	GL_BindTexture( 0, fb2->color );
	ARB_BlurParams( fb1->width, fb1->height, fboBloomFilterSize, qfalse );
	RenderQuad( w, h );
}


static void FBO_Blur2( const frameBuffer_t *fb1, const frameBuffer_t *fb2,  const frameBuffer_t *fb3 )
{
	const int w = glConfig.vidWidth;
	const int h = glConfig.vidHeight;

	qglViewport( 0, 0, fb1->width, fb1->height );

	// apply horizontal blur - render from FBO1 to FBO2
	FBO_Bind( GL_DRAW_FRAMEBUFFER, fb2->fbo );
	GL_BindTexture( 0, fb1->color );
	ARB_ProgramEnable( DUMMY_VERTEX, BLUR2_FRAGMENT );
	ARB_BlurParams( fb1->width, fb1->height, 6, qtrue );
	RenderQuad( w, h );

	// apply vectical blur - render from FBO2 to FBO3
	FBO_Bind( GL_DRAW_FRAMEBUFFER, fb3->fbo );
	GL_BindTexture( 0, fb2->color );
	ARB_BlurParams( fb1->width, fb1->height, 6, qfalse );
	RenderQuad( w, h );
}


void FBO_CopyScreen( void )
{
	const frameBuffer_t *dst;
	const frameBuffer_t *src;
	int yCrop;

	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );

	// resolve multisample buffer first
	if ( blitMSfbo )
	{
		src = &frameBufferMS;
		dst = &frameBuffers[ 0 ];
		FBO_Bind( GL_READ_FRAMEBUFFER, src->fbo );
		FBO_Bind( GL_DRAW_FRAMEBUFFER, dst->fbo );
		qglBlitFramebuffer( 0, 0, src->width, src->height, 0, 0, dst->width, dst->height, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	}

	src = &frameBuffers[ 0 ];
	dst = &frameBuffers[ 2 ];
	FBO_Bind( GL_READ_FRAMEBUFFER, src->fbo );
	FBO_Bind( GL_DRAW_FRAMEBUFFER, dst->fbo );

	yCrop = backEnd.viewParms.viewportHeight / 4;

	qglBlitFramebuffer( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY + yCrop,
		backEnd.viewParms.viewportWidth + backEnd.viewParms.viewportX,
		backEnd.viewParms.viewportHeight + backEnd.viewParms.viewportY,
		0, 0, dst->width, dst->height, GL_COLOR_BUFFER_BIT, GL_LINEAR );

	//if ( !backEnd.projection2D )
	{
		qglMatrixMode( GL_PROJECTION );
		qglLoadMatrixf( GL_Ortho( 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1 ) );
		qglMatrixMode( GL_MODELVIEW );
		qglLoadIdentity();
		GL_Cull( CT_TWO_SIDED );
		qglDisable( GL_CLIP_PLANE0 );
	}

	qglColor4f( 1, 1, 1, 1 );
	GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
	FBO_Blur2( dst, dst+1, dst );
	ARB_ProgramDisable();

	//restore viewport and scissor
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight ); 
	qglScissor( backEnd.viewParms.scissorX, backEnd.viewParms.scissorY,
		backEnd.viewParms.scissorWidth, backEnd.viewParms.scissorHeight ); 

	FBO_BindMain();
}


static void R_Setup_Quad_Lens( float offset, vec4_t color, vec3_t *verts, vec2_t *coords, vec4_t *colors )
{
	static const vec2_t t[6] = { {1.0, 0.0}, {0.0, 0.0}, {0.0, 1.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0} };

	const float width = (float)glConfig.vidWidth;
	const float height = (float)glConfig.vidHeight;
	int i;
	
	for ( i = 0; i < 6; i++ ) {
		coords[i][0] = t[i][0];
		coords[i][1] = t[i][1];
		Vector4Copy( color, colors[i] );
		verts[i][2] = 0.0;
	}

	verts[0][0] = -offset;
	verts[0][1] = -offset;

	verts[1][0] = width + offset;
	verts[1][1] = -offset;

	verts[2][0] = width + offset;
	verts[2][1] = height + offset;

	verts[3][0] = width + offset;
	verts[3][1] = height + offset;

	verts[4][0] = -offset;
	verts[4][1] = height + offset;

	verts[5][0] = -offset;
	verts[5][1] = -offset;
}


static void R_Bloom_LensEffect( float alpha )
{
	// lens rainbow colors
	static const GLfloat lc[][3] = {
		{ 0.78f, 0.23f, 0.34f },
		{ 0.78f, 0.39f, 0.21f },
		{ 0.78f, 0.59f, 0.21f },
		{ 0.71f, 0.75f, 0.21f },
		{ 0.52f, 0.78f, 0.21f },
		{ 0.32f, 0.78f, 0.21f },
		{ 0.21f, 0.78f, 0.28f },
		{ 0.21f, 0.78f, 0.47f },
		{ 0.21f, 0.77f, 0.66f },
		{ 0.21f, 0.67f, 0.78f },
		{ 0.21f, 0.47f, 0.78f },
		{ 0.21f, 0.28f, 0.78f },
		{ 0.35f, 0.21f, 0.78f },
		{ 0.53f, 0.21f, 0.78f },
		{ 0.72f, 0.21f, 0.75f },
		{ 0.78f, 0.21f, 0.59f },
	};
	int i;

	vec3_t verts[ ARRAY_LEN(lc) * 6 ];
	vec2_t coords[ ARRAY_LEN(lc) * 6 ];
	vec4_t colors[ ARRAY_LEN(lc) * 6 ];
	vec4_t color;

	alpha /= (float)ARRAY_LEN( lc );
	for ( i = 0; i < ARRAY_LEN( lc ); i++ ) {
		VectorCopy( lc[i], color ); color[3] = alpha;
		R_Setup_Quad_Lens( (i+1)*144, color, &verts[i*6], &coords[i*6], &colors[i*6] );
	}

	GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

	qglVertexPointer( 3, GL_FLOAT, 0, verts );
	qglTexCoordPointer( 2, GL_FLOAT, 0, coords );
	qglColorPointer( 4, GL_FLOAT, 0, colors );

	qglDrawArrays( GL_TRIANGLES, 0, ARRAY_LEN( verts ) );
}


qboolean FBO_Bloom( const float gamma, const float obScale, qboolean finalStage )
{
	const int w = glConfig.vidWidth;
	const int h = glConfig.vidHeight;
	const float threshold = r_bloom_threshold->value;
	const float invThreshold = ( threshold < 1.0f ) ? ( 1.0f / ( 1.0f - threshold ) ) : 1.0f;

	frameBuffer_t *src, *dst;
	int finalBloomFBO;
	int i;

	if ( backEnd.doneBloom || !backEnd.doneSurfaces )
	{
		return qfalse;
	}

	backEnd.doneBloom = qtrue;

	if ( !fboBloomInited )
	{
		if ( (fboBloomInited = FBO_CreateBloom() ) == qfalse )
		{
			ri.Printf( PRINT_WARNING, "...error creating framebuffers for bloom\n" );
			ri.Cvar_Set( "r_bloom", "0" );
			FBO_CleanBloom();
			return qfalse;
		}
		else
		{
			ri.Printf( PRINT_ALL, "...bloom framebuffers created\n" );
		}
	}

	if ( blitMSfbo )
	{
		FBO_BlitMS( qfalse );
		blitMSfbo = qfalse;
	}

	// extract intensity from main FBO to BLOOM_BASE
	src = &frameBuffers[ 0 ];
	dst = &frameBuffers[ BLOOM_BASE ];
	FBO_Bind( GL_FRAMEBUFFER, dst->fbo );
	GL_BindTexture( 0, src->color );
	qglViewport( 0, 0, dst->width, dst->height );
	ARB_ProgramEnable( DUMMY_VERTEX, BLOOM_EXTRACT_FRAGMENT );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, threshold, threshold, threshold, invThreshold );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 1, r_bloom_saturation->value, 1.0, 1.0, 1.0 );
	RenderQuad( w, h );

	// downscale and blur
	src = frameBuffers + BLOOM_BASE;
	for ( i = 1; i < fboBloomPasses; i++, src+=2 ) {
		dst = src + 2;
		// copy image to next level
#ifdef USE_FBO_BLIT
		FBO_Bind( GL_READ_FRAMEBUFFER, src->fbo );
		FBO_Bind( GL_DRAW_FRAMEBUFFER, dst->fbo );
		qglBlitFramebuffer( 0, 0, src->width, src->height, 0, 0, dst->width, dst->height, GL_COLOR_BUFFER_BIT, GL_LINEAR );
#else
		ARB_ProgramDisable();
		FBO_Bind( GL_FRAMEBUFFER, dst->fbo );
		GL_BindTexture( 0, src->color );
		GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
		qglViewport( 0, 0, dst->width, dst->height );
		RenderQuad( w, h );
#endif
		FBO_Blur( dst, dst+1, dst );
	}

	// restore viewport
	qglViewport( 0, 0, w, h );

	// blend all bloom buffers to BLOOM_BASE+1 texture
	finalBloomFBO = BLOOM_BASE+1;
	GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
	FBO_Bind( GL_FRAMEBUFFER, frameBuffers[ finalBloomFBO ].fbo );
	ARB_ProgramEnable( DUMMY_VERTEX, BLENDX_FRAGMENT );
	// setup all texture units
	for ( i = 0; i < fboBloomPasses - fboBloomBlendBase; i++ ) {
		GL_BindTexture( i, frameBuffers[ (i+fboBloomBlendBase)*2 + BLOOM_BASE ].color );
	}
	RenderQuad( w, h );

	if ( r_bloom_reflection->value )
	{
		ARB_ProgramDisable();

		// copy final bloom image to some downscaled buffer
		src = &frameBuffers[ finalBloomFBO ];
		dst = &frameBuffers[ BLOOM_BASE + 2 + 2 ]; // 4x downscale
		FBO_Bind( GL_DRAW_FRAMEBUFFER, dst->fbo );
		FBO_Bind( GL_READ_FRAMEBUFFER, src->fbo );
		qglBlitFramebuffer( 0, 0, src->width, src->height, 0, 0, dst->width, dst->height, GL_COLOR_BUFFER_BIT, GL_LINEAR );
		
		// set render target to paired destination buffer and draw reflections
		FBO_Bind( GL_DRAW_FRAMEBUFFER, (dst+1)->fbo );
		GL_BindTexture( 0, dst->color );
		GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE );
		qglViewport( 0, 0, dst->width, dst->height );
		R_Bloom_LensEffect( fabs( r_bloom_reflection->value ) );
		
		// restore color and blend mode
		qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
		
		// blur lens effect in paired buffer
		FBO_Blur( dst+1, dst, dst+1 );
		ARB_ProgramDisable();

		// add lens effect to final bloom buffer
		FBO_Bind( GL_FRAMEBUFFER, src->fbo );
		if ( r_bloom_reflection->value > 0 ) {
			GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );
		} else {
			// negative reflection values will replace bloom texture with just lens effect
		}
		qglViewport( 0, 0, w, h );
		GL_BindTexture( 0, (dst+1)->color );
		RenderQuad( w, h );

		// restore blend mode
		GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
	}

	if ( windowAdjusted || backEnd.screenshotMask ) {
		finalStage = qfalse; // can't blit directly into back buffer in this case
	}

	// if we don't need to read pixels later - blend directly to back buffer
	if ( finalStage ) {
		if ( backEnd.screenshotMask ) {
			FBO_Bind( GL_FRAMEBUFFER, frameBuffers[ BLOOM_BASE ].fbo );
		} else {
			FBO_Bind( GL_FRAMEBUFFER, 0 );
		}
	} else {
		FBO_Bind( GL_FRAMEBUFFER, frameBuffers[ BLOOM_BASE ].fbo );
	}

	GL_BindTexture( 1, frameBuffers[ finalBloomFBO ].color ); // final bloom texture
	GL_BindTexture( 0, frameBuffers[ 0 ].color ); // original image
	if ( finalStage ) {
		// blend & apply gamma in one pass
		ARB_ProgramEnable( DUMMY_VERTEX, BLEND2_GAMMA_FRAGMENT );
		qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, gamma, gamma, gamma, obScale );
		qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 1, r_bloom_intensity->value, 0, 0, 0 );
		ARB_FinalPassParams();
	} else {
		// just blend
		ARB_ProgramEnable( DUMMY_VERTEX, BLEND2_FRAGMENT );
		qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 1, r_bloom_intensity->value, 0, 0, 0 );
	}
	RenderQuad( w, h );
	ARB_ProgramDisable();

	if ( finalStage ) {
		if ( backEnd.screenshotMask ) {
			FBO_BlitToBackBuffer( BLOOM_BASE ); // so any further qglReadPixels() will read from BLOOM_BASE
			 // fboReadIndex = 0;
		} else {
			//	already in back buffer
			fboReadIndex = 0;
		}
	} else {
		// we need depth/stencil buffers there
		fboReadIndex = BLOOM_BASE;
	}

	return finalStage;
}


void R_BloomScreen( void )
{
	if ( r_bloom->integer == 1 && fboEnabled && qglActiveTextureARB )
	{
		if ( !backEnd.doneBloom && backEnd.doneSurfaces )
		{
			RB_SetGL2D();
			qglColor4f( 1, 1, 1, 1 );
			FBO_Bloom( 0, 0, qfalse );
		}
	}
}


static GLuint ssaoNoise;
static qboolean fboSSAOInited = qfalse;

static void FBO_CleanSSAO( void )
{
	FBO_Clean( &frameBuffers[ SSAO_BASE + 0 ] );
	FBO_Clean( &frameBuffers[ SSAO_BASE + 1 ] );
	if ( ssaoNoise )
	{
		GL_BindTexture( 1, 0 );
		GL_BindTexture( 0, 0 );
		qglDeleteTextures( 1, &ssaoNoise );
		ssaoNoise = 0;
	}
	fboSSAOInited = qfalse;
}


static qboolean FBO_CreateSSAO( void )
{
	// 16 directions over half a circle (mirroring about d and -d is the same) in 4x4 Bayer order
	static const byte order[ 16 ] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
	byte noise[ 16 * 4 ];
	int i;

	// half resolution: a quarter of the cost, upscaled bilinearly by the composite
	const int w = ( glConfig.vidWidth + 1 ) / 2;
	const int h = ( glConfig.vidHeight + 1 ) / 2;

	if ( !FBO_Create( &frameBuffers[ SSAO_BASE + 0 ], w, h, qfalse, NULL, NULL ) ||
		!FBO_Create( &frameBuffers[ SSAO_BASE + 1 ], w, h, qfalse, NULL, NULL ) )
		return qfalse;

	for ( i = 0; i < 16; i++ )
	{
		const float a = order[ i ] * ( (float)M_PI / 16.0f );
		noise[ i * 4 + 0 ] = (byte)( ( cosf( a ) * 0.5f + 0.5f ) * 255.0f + 0.5f );
		noise[ i * 4 + 1 ] = (byte)( ( sinf( a ) * 0.5f + 0.5f ) * 255.0f + 0.5f );
		noise[ i * 4 + 2 ] = 128;
		noise[ i * 4 + 3 ] = 255;
	}

	qglGenTextures( 1, &ssaoNoise );
	GL_BindTexture( 0, ssaoNoise );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, noise );
	GL_BindTexture( 0, 0 );

	return qtrue;
}


/*
=============
FBO_SSAO

Darkens the main 3D view by screen-space ambient occlusion, computed from its depth buffer.
Runs once the view is drawn, so translucent surfaces get the occlusion of what's behind them.
=============
*/
void FBO_SSAO( void )
{
	const viewParms_t *vp = &backEnd.viewParms;
	const float *pm = vp->projectionMatrix;
	const int w = glConfig.vidWidth;
	const int h = glConfig.vidHeight;
	const GLuint depth = frameBuffers[ 0 ].depthStencil;
	int aw, ah;
	float sx, sy, radius, zNear, fade;

	if ( !r_ssao->integer || !fboEnabled || !programCompiled || !ssaoCompiled || !qglActiveTextureARB )
		return;

	// main world view only: no portals, skybox portal views or 3D UI models
	if ( ( backEnd.refdef.rdflags & ( RDF_NOWORLDMODEL | RDF_SKYBOXPORTAL ) ) || vp->portalView != PV_NONE )
		return;

	// multisampling without r_flares creates no resolved depth buffer, see QGL_InitFBO
	if ( !depth )
		return;

	if ( !fboSSAOInited )
	{
		if ( ( fboSSAOInited = FBO_CreateSSAO() ) == qfalse )
		{
			ri.Printf( PRINT_WARNING, "...error creating framebuffers for SSAO\n" );
			ri.Cvar_Set( "r_ssao", "0" );
			FBO_CleanSSAO();
			return;
		}
	}

	aw = frameBuffers[ SSAO_BASE + 0 ].width;
	ah = frameBuffers[ SSAO_BASE + 0 ].height;

	if ( blitMSfbo )
		FBO_BlitMS( qtrue ); // resolve depth into frameBuffers[ 0 ]

	ARB_ProgramDisable();
	qglViewport( 0, 0, w, h );
	qglScissor( 0, 0, w, h );
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( GL_Ortho( 0, w, h, 0, 0, 1 ) );
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();
	backEnd.projection2D = qtrue;
	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
	GL_Cull( CT_TWO_SIDED );

	sx = 2.0f * w / vp->viewportWidth;
	sy = 2.0f * h / vp->viewportHeight;
	radius = r_ssaoRadius->value;

	// 1: occlusion, at the half size of the AO buffers
	qglViewport( 0, 0, aw, ah );
	FBO_Bind( GL_FRAMEBUFFER, frameBuffers[ SSAO_BASE + 0 ].fbo );
	GL_BindTexture( 1, ssaoNoise );
	GL_BindTexture( 0, depth );
	ARB_ProgramEnable( DUMMY_VERTEX, SSAO_FRAGMENT );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, pm[10] - 1.0f, pm[14], 1.0f / pm[0], 1.0f / pm[5] );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 1, (float)vp->viewportX / w, (float)vp->viewportY / h, sx, sy );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 2, radius * pm[0] / sx, radius * pm[5] / sy,
		1.0f / ( radius * radius ), 2.0f * r_ssaoIntensity->value / SSAO_SAMPLES );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 3, 1.0f / w, 0, 0, 0 );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 4, 0, 1.0f / h, 0, 0 );
	// weapons are drawn with glDepthRange( 0, 0.3 ); fade out over 75-100% of the distance where
	// a 24-bit depth step (about z^2 / zNear / 2^24) grows past 1/16 of the radius
	zNear = pm[14] / ( pm[10] - 1.0f );
	fade = sqrtf( radius * 0.0625f * zNear * 16777216.0f );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 5, 0.1f, 0.1f, 0.3f, 4.0f / fade );
	// a texel spans about 2z / ( proj[0] * viewport width ), so the depth step is 5% of it at distance
	// 0.05 * 2 zNear 2^24 / ( proj[0] * viewport width )
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 6, -0.5f / w, -0.5f / h,
		pm[0] * vp->viewportWidth / ( 0.1f * zNear * 16777216.0f ), 0 );
	RenderQuad( w, h );

	// 2: blur away the noise pattern
	FBO_Bind( GL_FRAMEBUFFER, frameBuffers[ SSAO_BASE + 1 ].fbo );
	GL_BindTexture( 1, depth );
	GL_BindTexture( 0, frameBuffers[ SSAO_BASE + 0 ].color );
	ARB_ProgramEnable( DUMMY_VERTEX, SSAO_BLUR_FRAGMENT );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, pm[10] - 1.0f, pm[14], 0, 0 );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 1, 1.0f / aw, 1.0f / ah, 1.0f / 0.05f, 0 );
	RenderQuad( w, h );
	GL_BindTexture( 1, 0 );

	// 3: multiply into the scene, inside the view's viewport (fixed function, fog is off here)
	ARB_ProgramDisable();
	FBO_BindMain();
	qglViewport( 0, 0, w, h );
	qglScissor( vp->viewportX, vp->viewportY, vp->viewportWidth, vp->viewportHeight );
	GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO );
	GL_BindTexture( 0, frameBuffers[ SSAO_BASE + 1 ].color );
	RenderQuad( w, h );
	qglScissor( 0, 0, w, h );
}


void FBO_PostProcess( void )
{
	const float obScale = 1 << tr.overbrightBits;
	const float gamma = 1.0f / r_gamma->value;
	const float w = glConfig.vidWidth;
	const float h = glConfig.vidHeight;
	qboolean minimized;

	ARB_ProgramDisable();

	if ( !backEnd.projection2D )
	{
		qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
		qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
		qglMatrixMode( GL_PROJECTION );
		qglLoadMatrixf( GL_Ortho( 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1 ) );
		qglMatrixMode( GL_MODELVIEW );
		qglLoadIdentity();
		backEnd.projection2D = qtrue;
		// no need to setup shader time there
	}

	if ( blitMSfbo )
	{
		FBO_BlitMS( qfalse );
		blitMSfbo = qfalse;
	}

	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
	GL_Cull( CT_TWO_SIDED );
	if ( r_anaglyphMode->integer )
		qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	minimized = ri.CL_IsMinimized();

	if ( r_bloom->integer && programCompiled && qglActiveTextureARB ) {
		if ( FBO_Bloom( gamma, obScale, !minimized ) ) {
			return;
		}
	}

	// check if we can perform final draw directly into back buffer
	if ( backEnd.screenshotMask == 0 && !windowAdjusted && !minimized ) {
		FBO_Bind( GL_FRAMEBUFFER, 0 );
		GL_BindTexture( 0, frameBuffers[ fboReadIndex ].color );
		ARB_ProgramEnable( DUMMY_VERTEX, GAMMA_FRAGMENT );
		qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, gamma, gamma, gamma, obScale );
		ARB_FinalPassParams();
		RenderQuad( w, h );
		ARB_ProgramDisable();
		return;
	}

	// apply gamma shader
	FBO_Bind( GL_FRAMEBUFFER, frameBuffers[ 1 ].fbo ); // destination - secondary buffer
	GL_BindTexture( 0, frameBuffers[ fboReadIndex ].color );  // source - main color buffer
	ARB_ProgramEnable( DUMMY_VERTEX, GAMMA_FRAGMENT );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, gamma, gamma, gamma, obScale );
	ARB_FinalPassParams();
	RenderQuad( w, h );
	ARB_ProgramDisable();

	if ( !minimized ) {
		FBO_BlitToBackBuffer( 1 );
	}
}


void QGL_SetRenderScale( qboolean verbose )
{
	windowAdjusted = qfalse;

	blitX0 = blitY0 = 0;
	blitX1 = gls.windowWidth;
	blitY1 = gls.windowHeight;

	blitFilter = GL_NEAREST;

	superSampled = qfalse;

	if ( !qglGenProgramsARB || !qglGenFramebuffers )
		return;

	if ( !r_fbo->integer )
	{
		if ( verbose && r_renderScale->integer )
		{
			ri.Printf( PRINT_ALL, "...ignoring \r_renderScale due to disabled FBO\n" );
		}
		if ( verbose && r_ext_supersample->integer )
		{
			ri.Printf( PRINT_ALL, "...ignoring \r_ext_supersample due to disabled FBO\n" );
		}
		return;
	}

	if ( r_ext_supersample->integer )
	{
		superSampled = qtrue;
		blitFilter = GL_LINEAR; // default value for (r_renderScale==0) case
	}

	if ( gls.windowWidth != glConfig.vidWidth || gls.windowHeight != glConfig.vidHeight )
	{
		if ( r_renderScale->integer > 0 )
		{
			int scaleMode = r_renderScale->integer - 1;
			if ( scaleMode & 1 )
			{
				// preserve aspect ratio (black bars on sides)
				float windowAspect = (float) gls.windowWidth / (float) gls.windowHeight;
				float renderAspect = (float) glConfig.vidWidth / (float) glConfig.vidHeight;
				if ( windowAspect >= renderAspect ) 
				{
					float scale = (float) gls.windowHeight / ( float ) glConfig.vidHeight;
					int bias = ( gls.windowWidth - scale * (float) glConfig.vidWidth ) / 2;
					blitX0 += bias;
					blitX1 -= bias;
				}
				else
				{
					float scale = (float) gls.windowWidth / ( float ) glConfig.vidWidth;
					int bias = ( gls.windowHeight - scale * (float) glConfig.vidHeight ) / 2;
					blitY0 += bias;
					blitY1 -= bias;
				}
			}
			// linear filtering
			if ( scaleMode & 2 )
				blitFilter = GL_LINEAR;
			else
				blitFilter = GL_NEAREST;
		}

		windowAdjusted = qtrue;
	}
}


void QGL_DoneFBO( void )
{
	if ( qglGenFramebuffers )
	{
		FBO_Bind(GL_FRAMEBUFFER, 0);
		FBO_Clean(&frameBufferMS);
		FBO_Clean(&frameBuffers[0]);
		FBO_Clean(&frameBuffers[1]);
		FBO_Clean(&frameBuffers[2]);
		FBO_Clean(&frameBuffers[3]);
		FBO_Clean(&frameBuffers[4]);
		FBO_CleanBloom();
		FBO_CleanSSAO();
		FBO_CleanDepth();
		fboEnabled = qfalse;
		fboBloomInited = qfalse;
	}
}


void QGL_InitFBO( void )
{
	int w, h;
	qboolean depthStencil;
	qboolean result = qfalse;

	QGL_DoneFBO();

	w = glConfig.vidWidth;
	h = glConfig.vidHeight;
	
	fboEnabled = qfalse;
	frameBufferMultiSampling = qfalse;

	if ( r_fbo->integer && ( !qglGenProgramsARB || !qglGenFramebuffers ) )
		ri.Printf( PRINT_WARNING, "...FBO is not available\n" );

	if ( !r_fbo->integer || !qglGenProgramsARB || !qglGenFramebuffers )
		return;

	qglGetError(); // reset error code

	if ( windowAdjusted )
		blitClear = 2; // front & back buffers
	else
		blitClear = 0;

	switch ( r_hdr->integer )
	{
		case -1: fboInternalFormat = GL_RGBA4; break;
		case 0: fboInternalFormat = GL_RGBA8; break;
		default: fboInternalFormat = GL_RGBA16; break;
	}

	if ( FBO_CreateMS( &frameBufferMS, w, h ) )
	{
		frameBufferMultiSampling = qtrue;
		if ( r_flares->integer || r_ssao->integer ) // they read the resolved depth
			depthStencil = qtrue;
		else
			depthStencil = qfalse;
		result = FBO_Create( &frameBuffers[ 0 ], w, h, depthStencil, &fboTextureFormat, &fboTextureType )
			&& FBO_Create( &frameBuffers[ 1 ], w, h, depthStencil, NULL, NULL )
			&& FBO_Create( &frameBuffers[ 2 ], SCR_WIDTH, SCR_HEIGHT, qfalse, NULL, NULL )
			&& FBO_Create( &frameBuffers[ 3 ], SCR_WIDTH, SCR_HEIGHT, qfalse, NULL, NULL );
		frameBufferMultiSampling = result;
	}
	else
	{
		result = FBO_Create( &frameBuffers[ 0 ], w, h, qtrue, &fboTextureFormat, &fboTextureType )
			&& FBO_Create( &frameBuffers[ 1 ], w, h, qtrue, NULL, NULL )
			&& FBO_Create( &frameBuffers[ 2 ], SCR_WIDTH, SCR_HEIGHT, qfalse, NULL, NULL )
			&& FBO_Create( &frameBuffers[ 3 ], SCR_WIDTH, SCR_HEIGHT, qfalse, NULL, NULL );
	}

	if ( result && superSampled )
	{
		result &= FBO_Create( &frameBuffers[ 4 ], gls.captureWidth, gls.captureHeight, qfalse, NULL, NULL );
	}

	if ( result )
	{
		fboEnabled = qtrue;
		FBO_BindMain();
		ri.Printf( PRINT_ALL, "...using %s (%s:%s) FBO\n", glDefToStr( fboInternalFormat ),
			glDefToStr( fboTextureFormat ), glDefToStr( fboTextureType ) );
	}
	else
	{
		QGL_DoneFBO();
	}
}
#endif // USE_FBO


void QGL_InitARB( void )
{
	ARB_UpdatePrograms();
#ifdef USE_FBO
	QGL_SetRenderScale( qtrue );
	QGL_InitFBO();
#endif
	ri.Cvar_ResetGroup( CVG_RENDERER, qtrue );
}


void QGL_DoneARB( void )
{
#ifdef USE_FBO
	QGL_DoneFBO();
#endif
	if ( programCompiled )
	{
		ARB_ProgramDisable();
		ARB_DeletePrograms();
	}
}
