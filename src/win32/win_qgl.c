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
/*
** QGL_WIN.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Quake3 you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/

#ifdef USE_OPENGL_API

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderer/qgl.h"
#include "../renderercommon/tr_types.h"
#include "glw_win.h"
#include "win_local.h"

#define GLE( ret, name, ... ) ret ( APIENTRY * q##name )( __VA_ARGS__ );
QGL_Win32_PROCS;
QGL_Swp_PROCS;
#undef GLE

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.  This
** is only called during a hard shutdown of the OGL subsystem (e.g. vid_restart).
*/
void QGL_Shutdown( qboolean unloadDLL )
{
	Com_Printf( "...shutting down QGL\n" );

	if ( glw_state.OpenGLLib && unloadDLL )
	{
		Com_Printf( "...unloading OpenGL DLL\n" );
		Sys_UnloadLibrary( glw_state.OpenGLLib );
		glw_state.OpenGLLib = NULL;
	}
	
#define GLE( ret, name, ... ) q##name = NULL;
	QGL_Win32_PROCS;
	QGL_Swp_PROCS;
#undef GLE
}


void *GL_GetProcAddress( const char *name )
{
	void *ptr;

	ptr = Sys_LoadFunction( glw_state.OpenGLLib, name );
	if ( !ptr && qwglGetProcAddress )
	{
		ptr = qwglGetProcAddress( name );
	}

	return ptr;
}


/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to 
** the appropriate GL stuff.  In Windows this means doing a 
** LoadLibrary and a bunch of calls to GetProcAddress.  On other
** operating systems we need to do the right thing, whatever that
** might be.
*/
qboolean QGL_Init( const char *dllname )
{
	char libName[1024];
#ifdef UNICODE
	TCHAR buffer[1024];
#endif

#if 0
	char systemDir[1024];

#ifdef UNICODE
	TCHAR buffer[1024];
	GetSystemDirectory( buffer, ARRAYSIZE( buffer ) );
	strcpy( systemDir, WtoA( buffer ) );
#else
	GetSystemDirectory( systemDir, sizeof( systemDir ) );
#endif
#endif

	Com_Printf( "...initializing QGL\n" );

	if ( glw_state.OpenGLLib == NULL )
	{
		// NOTE: this assumes that 'dllname' is lower case (and it should be)!
#if 0
		if ( dllname[0] != '!' )
			Com_sprintf( libName, sizeof( libName ), "%s\\%s", systemDir, dllname );
		else
			Q_strncpyz( libName, dllname+1, sizeof( libName ) );
#else
		Q_strncpyz( libName, dllname, sizeof( libName ) );
#endif
		Q_strcat( libName, sizeof( libName ), ".dll" );
		glw_state.OpenGLLib = Sys_LoadLibrary( libName );
		if ( glw_state.OpenGLLib == NULL )
		{
			Com_Printf( "...loading '%s' : " S_COLOR_YELLOW "failed\n", libName );
			return qfalse;
		}

		// get exact loaded module name
#ifdef UNICODE
		GetModuleFileName( glw_state.OpenGLLib, buffer, ARRAY_LEN( buffer ) );
		buffer[ ARRAY_LEN( buffer ) - 1 ] = '\0';
		Q_strncpyz( libName, WtoA( buffer ), sizeof( libName ) );
#else
		GetModuleFileName( glw_state.OpenGLLib, libName, sizeof( libName ) );
		libName[ sizeof( libName ) - 1 ] = '\0';
#endif
		Com_Printf( "...loading '%s' : succeeded\n", libName );
	}

	Sys_LoadFunctionErrors(); // reset error count

#define GLE( ret, name, ... ) q##name = GL_GetProcAddress( XSTRING( name ) ); if ( !q##name ) { Com_Printf( "Error resolving core Win32 functions\n" ); return qfalse; }
	QGL_Win32_PROCS;
#undef GLE
	return qtrue;
}

#endif // USE_OPENGL_API
