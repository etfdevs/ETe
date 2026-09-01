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

#ifndef __TR_PUBLIC_H__
#define __TR_PUBLIC_H__

#include "tr_types.h"
#include "vulkan/vulkan.h"

#define REF_API_VERSION     8

//
// these are the functions exported by the refresh module
//
typedef enum {
	REF_KEEP_CONTEXT, // don't destroy window and context
	REF_KEEP_WINDOW,  // destroy context, keep window
	REF_DESTROY_WINDOW,
	REF_UNLOAD_DLL
} refShutdownCode_t;

typedef struct {
	// called before the library is unloaded
	// if the system is just reconfiguring, pass destroyWindow = qfalse,
	// which will keep the screen from flashing to the desktop.
	void	(*Shutdown)( refShutdownCode_t code );

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// BeginRegistration makes any existing media pointers invalid
	// and returns the current gl configuration, including screen width
	// and height, which can be used by the client to intelligently
	// size display elements
	void ( *BeginRegistration )( glconfig_t *config );
	qhandle_t ( *RegisterModel )( const char *name );
	//qhandle_t ( *RegisterModelAllLODs )( const char *name );
	qhandle_t ( *RegisterSkin )( const char *name );
	qhandle_t ( *RegisterShader )( const char *name );
	qhandle_t ( *RegisterShaderNoMip )( const char *name );
	void ( *RegisterFont )( const char *fontName, int pointSize, fontInfo_t *font );

	void ( *LoadWorld )( const char *name );
	qboolean ( *GetSkinModel )( qhandle_t skinid, const char *type, char *name );    //----(SA)	added
	qhandle_t ( *GetShaderFromModel )( qhandle_t modelid, int surfnum, int withlightmap );                //----(SA)	added

	// the vis data is a large enough block of data that we go to the trouble
	// of sharing it with the clipmodel subsystem
	void ( *SetWorldVisData )( const byte *vis );

	// EndRegistration will draw a tiny polygon with each texture, forcing
	// them to be loaded into card memory
	void ( *EndRegistration )( void );

	// a scene is built up by calls to R_ClearScene and the various R_Add functions.
	// Nothing is drawn until R_RenderScene is called.
	void ( *ClearScene )( void );
	void ( *AddRefEntityToScene )( const refEntity_t *re, qboolean intShaderTime );
	int ( *LightForPoint )( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
	void ( *AddPolyToScene )( qhandle_t hShader, int numVerts, const polyVert_t *verts );
	// Ridah
	void ( *AddPolysToScene )( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys );
	// done.
	void ( *AddLightToScene )( const vec3_t org, float radius, float intensity, float r, float g, float b, qhandle_t hShader, int flags );
	void	(*AddLinearLightToScene)( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b );
//----(SA)
	void ( *AddCoronaToScene )( const vec3_t org, float r, float g, float b, float scale, int id, qboolean visible );
	void ( *SetFog )( int fogvar, int var1, int var2, float r, float g, float b, float density );
//----(SA)
	void ( *RenderScene )( const refdef_t *fd );

	void ( *SetColor )( const float *rgba );    // NULL = 1,1,1,1
	void ( *DrawStretchPic )( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );      // 0 = white
	void ( *DrawRotatedPic )( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, float angle );     // NERVE - SMF
	void ( *DrawStretchPicGradient )( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, const float *gradientColor, int gradientType );
	void ( *Add2dPolys )( polyVert_t* polys, int numverts, qhandle_t hShader );

	// Draw images for cinematic rendering, pass as 32 bit rgba
	void ( *DrawStretchRaw )( int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty );
	void ( *UploadCinematic )( int w, int h, int cols, int rows, byte *data, int client, qboolean dirty );

	void ( *BeginFrame )( stereoFrame_t stereoFrame );

	// if the pointers are not NULL, timing info will be returned
	void ( *EndFrame )( int *frontEndMsec, int *backEndMsec );


	int ( *MarkFragments )( int numPoints, const vec3_t *points, const vec3_t projection,
							int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

	void ( *ProjectDecal )( qhandle_t hShader, int numPoints, vec3_t *points, vec4_t projection, vec4_t color, int lifeTime, int fadeTime );
	void ( *ClearDecals )( void );

	int ( *LerpTag )( orientation_t *tag,  const refEntity_t *refent, const char *tagName, int startIndex );
	void ( *ModelBounds )( qhandle_t model, vec3_t mins, vec3_t maxs );

	void ( *RemapShader )( const char *oldShader, const char *newShader, const char *offsetTime );

	void ( *DrawDebugPolygon )( int color, int numpoints, float* points );

	void ( *DrawDebugText )( const vec3_t org, float r, float g, float b, const char *text, qboolean neverOcclude );

	qboolean ( *GetEntityToken )( char *buffer, int size );

	void ( *AddPolyBufferToScene )( polyBuffer_t* pPolyBuffer );

	void ( *SetGlobalFog )( qboolean restore, int duration, float r, float g, float b, float depthForOpaque );

	qboolean ( *inPVS )( const vec3_t p1, const vec3_t p2 );

	//bani
	qboolean ( *LoadDynamicShader )( const char *shadername, const char *shadertext );
	// fretn
	void ( *RenderToTexture )( int textureid, int x, int y, int w, int h );
	//bani
	int ( *GetTextureId )( const char *imagename );
	void ( *Finish )( void );
	
	void (*TakeVideoFrame)( int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg );

	void	(*ThrottleBackend)( void );
	void	(*FinishBloom)( void );

	void	(*SetColorMappings)( void );

	qboolean (*CanMinimize)( void ); // == fbo enabled

	const glconfig_t *(*GetConfig)( void );

	void	(*SyncRender)( void );

	const cplane_t *(*GetFrustum)( void );

	qboolean (*IsMainScene)( void );

	void* (*GetImageBuffer)(int size, bufferMemType_t bufferType);


} refexport_t;

//
// these are the functions imported by the refresh module
//
typedef struct {
	// print message on the local console
	void	(QDECL *Printf)( printParm_t printLevel, const char *fmt, ... ) Q_PRINTF_FUNC(2, 3);

	// abort the game
	void	(QDECL *Error)( errorParm_t errorLevel, const char *fmt, ... ) Q_NO_RETURN_PTR Q_PRINTF_FUNC(2, 3);

	// milliseconds should only be used for profiling, never
	// for anything game related.  Get time from the refdef
	int		(*Milliseconds)( void );

	int64_t	(*Microseconds)( void );

	// stack based memory allocation for per-level things that
	// won't be freed
#ifdef HUNK_DEBUG
	void	*(*Hunk_AllocDebug)( size_t size, ha_pref pref, const char *label, const char *file, int line );
#else
	void	*(*Hunk_Alloc)( size_t size, ha_pref pref );
#endif
	void	*(*Hunk_AllocateTempMemory)( size_t size );
	void	(*Hunk_FreeTempMemory)( void *block );

	// dynamic memory allocator for things that need to be freed
	void	*(*Malloc)( size_t bytes );
	void	(*Free)( void *buf );
	void	(*Tag_Free)( void );

	cvar_t	*(*Cvar_Get)( const char *name, const char *value, int flags );
	void	(*Cvar_Set)( const char *name, const char *value );
	void	(*Cvar_SetValue) (const char *name, float value);
	void	(*Cvar_CheckRange)( cvar_t *cv, const char *minVal, const char *maxVal, cvarValidator_t type );
	void	(*Cvar_SetDescription)( cvar_t *cv, const char *description );

	void	(*Cvar_SetGroup)( cvar_t *var, cvarGroup_t group );
	int		(*Cvar_CheckGroup)( cvarGroup_t group );
	void	(*Cvar_ResetGroup)( cvarGroup_t group, qboolean resetModifiedFlags );

	void	(*Cvar_VariableStringBuffer)( const char *var_name, char *buffer, int bufsize );
	const char *(*Cvar_VariableString)( const char *var_name );
	int		(*Cvar_VariableIntegerValue) (const char *var_name);

	void	(*Cmd_AddCommand)( const char *name, void(*cmd)(void) );
	void	(*Cmd_RemoveCommand)( const char *name );

	void	(*Cmd_RegisterList)( const cmdListItem_t* cmds, int count );
	void	(*Cmd_UnregisterModule)( void );

	int		(*Cmd_Argc) (void);
	const char	*(*Cmd_Argv) (int i);

	void	(*Cmd_ExecuteText)( cbufExec_t exec_when, const char *text );

	byte	*(*CM_ClusterPVS)(int cluster);

	// visualization for debugging collision detection
	void ( *CM_DrawDebugSurface )( void( *drawPoly ) ( int color, int numPoints, float *points ) );

	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	//int ( *FS_FileIsInPAK )( const char *name, int *pChecksum );
	int ( *FS_ReadFile )( const char *name, void **buf );
	void ( *FS_FreeFile )( void *buf );
	char ** ( *FS_ListFiles )( const char *name, const char *extension, int *numfilesfound );
	char ** ( *FS_ListFilesEx )( const char *path, const char **extensions, int numExts, int *numfiles );
	void ( *FS_FreeFileList )( char **filelist );
	void ( *FS_WriteFile )( const char *qpath, const void *buffer, int size );
	qboolean ( *FS_FileExists )( const char *file );

	const char *( *FS_GetCurrentGameDir) ( void );

	// cinematic stuff
	void	(*CIN_UploadCinematic)( int handle );
	int		(*CIN_PlayCinematic)( const char *arg0, int xpos, int ypos, int width, int height, int bits );
	e_status (*CIN_RunCinematic)( int handle );

	void	(*CL_WriteAVIVideoFrame)( const byte *buffer, int size );

	size_t	(*CL_SaveJPGToBuffer)( byte *buffer, size_t bufSize, int quality, int image_width, int image_height, byte *image_buffer, int padding );
	void	(*CL_SaveJPG)( const char *filename, int quality, int image_width, int image_height, byte *image_buffer, int padding );
	void	(*CL_LoadJPG)( const char *filename, byte **pic, int *width, int *height );

	qboolean (*CL_IsMinimized)( void );
	void	(*CL_SetScaling)( float factor, int captureWidth, int captureHeight );

	int		(*CL_CurrentGameMod)( void );

	void	(*SCR_UpdateScreen)( void );

	void	(*Sys_SetClipboardBitmap)( const byte *bitmap, int size );
	qboolean(*Sys_LowPhysicalMemory)( void );

	int		(*Com_RealTime)( qtime_t *qtime );
	int		(*Com_Filter)( const char *filter, const char *name );
	int		(*MSG_HashKey)( const char *string, int maxlen );

	// platform-dependent functions
	void	(*GLimp_InitGamma)( glconfig_t *config );
	void	(*GLimp_SetGamma)( unsigned char red[256], unsigned char green[256], unsigned char blue[256] );
	void	(*GLimp_FlashWindow)( int state );

	// OpenGL
	void	(*GLimp_Init)( glconfig_t *config );
	void	(*GLimp_Shutdown)( qboolean unloadDLL );
	void	(*GLimp_EndFrame)( void );
	int		(*GLimp_NormalFontBase)( void );
	void*	(*GL_GetProcAddress)( const char *name );

	// Vulkan
	void	(*VKimp_Init)( glconfig_t *config );
	void	(*VKimp_Shutdown)( qboolean unloadDLL );
	void*	(*VK_GetInstanceProcAddr)( VkInstance instance, const char *name );
	qboolean (*VK_CreateSurface)( VkInstance instance, VkSurfaceKHR *pSurface );

} refimport_t;

extern	refimport_t	ri;

//todo this should be same in all renderers but tr_common is currently renderer specific right now

void* R_GetImageBuffer(int size, bufferMemType_t bufferType);
void R_FreeImageBuffer(void);

// this is the only function actually exported at the linker level
// If the module can't init to a valid rendering state, NULL will be
// returned.
#ifdef USE_RENDERER_DLOPEN
typedef	refexport_t* (QDECL *GetRefAPI_t) (int apiVersion, refimport_t * rimp);
#else
refexport_t*GetRefAPI( int apiVersion, refimport_t *rimp );
#endif

#endif	// __TR_PUBLIC_H
