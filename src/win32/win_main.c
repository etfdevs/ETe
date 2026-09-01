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
// win_main.c

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#ifndef DEDICATED
#include "../client/client.h"
#endif
#include "win_local.h"
#include "resource.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <direct.h>
#include <io.h>

#if defined(USE_SDL3) && !defined(DEDICATED)
#include <SDL3/SDL_main.h>
#endif

#define MEM_THRESHOLD (96*1024*1024)

WinVars_t	g_wv;

/*
==================
Sys_LowPhysicalMemory
==================
*/
qboolean Sys_LowPhysicalMemory( void ) {
	MEMORYSTATUSEX statex;
	ZeroMemory( &statex, sizeof( statex ) );
	statex.dwLength = sizeof( statex );

	GlobalMemoryStatusEx( &statex );
	return (statex.ullTotalPhys <= MEM_THRESHOLD) ? qtrue : qfalse;
}

/*
==================
Sys_StartProcess

NERVE - SMF
==================
*/
void Sys_StartProcess( const char *exeName, qboolean doexit ) {
	TCHAR szPathOrig[_MAX_PATH], temp[_MAX_PATH];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory( &si, sizeof( si ) );
	si.cb = sizeof( si );

	GetCurrentDirectory( _MAX_PATH, szPathOrig );

	// Note this only works if tchar is to remain `char`
	Com_sprintf( temp, sizeof( temp ), "%s\\%s", szPathOrig, exeName );

	// JPW NERVE swiped from Sherman's SP code
	if ( !CreateProcess( NULL, temp, NULL, NULL,FALSE, 0, NULL, NULL, &si, &pi ) ) {
		// couldn't start it, popup error box
		Com_Error( ERR_DROP, "Could not start process: '%s\\%s'", szPathOrig, exeName );
		return;
	}
	// jpw

	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	// TTimo: similar way of exiting as used in Sys_OpenURL below
	if ( doexit ) {
		Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
	}
}

/*
==================
Sys_OpenURL

NERVE - SMF
==================
*/
void Sys_OpenURL( const char *url, qboolean doexit ) {
	HWND wnd;

	static qboolean doexit_spamguard = qfalse;

	if ( doexit_spamguard ) {
		Com_DPrintf( "Sys_OpenURL: already in a doexit sequence, ignoring %s\n", url );
		return;
	}

	Com_Printf( "Open URL: %s\n", url );

	if ( !ShellExecute( NULL, "open", url, NULL, NULL, SW_RESTORE ) ) {
		// couldn't start it, popup error box
		Com_Error( ERR_DROP, "Could not open url: '%s' ", url );
		return;
	}

	wnd = GetForegroundWindow();

	if ( wnd ) {
		ShowWindow( wnd, SW_MAXIMIZE );
	}

	if ( doexit ) {
		// show_bug.cgi?id=612
		doexit_spamguard = qtrue;
		Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
	}
}


/*
=============
Sys_Error

Show the early console as an error dialog
=============
*/
void Q_NO_RETURN Q_PRINTF_FUNC(1, 2) QDECL Sys_Error( const char *error, ... ) {
	va_list	argptr;
	char	text[4096];
	MSG		msg;

	va_start( argptr, error );
	Q_vsnprintf( text, sizeof( text ), error, argptr );
	va_end( argptr );

#ifndef DEDICATED
	if ( !com_errorEntered && com_cl_running && com_cl_running->integer )
		CL_Shutdown( text, qtrue );
#endif

	Conbuf_AppendText( text );
	Conbuf_AppendText( "\n" );

	Sys_SetErrorText( text );
	Sys_ShowConsole( 1, qtrue );

	timeEndPeriod( 1 );

	// wait for the user to quit
	while ( 1 ) {
		if ( GetMessage( &msg, NULL, 0, 0 ) <= 0 ) {
			Cmd_Clear();
			Com_Quit_f();
		}
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}

	SetUnhandledExceptionFilter( NULL );

	Sys_DestroyConsole();

	exit( 1 );
}


/*
==============
Sys_Quit
==============
*/
void Q_NO_RETURN Sys_Quit( void ) {

	timeEndPeriod( 1 );

	SetUnhandledExceptionFilter( NULL );

	Sys_DestroyConsole();

	exit( 0 );
}


/*
==============
Sys_Print
==============
*/
void Sys_Print( const char *msg )
{
	Conbuf_AppendText( msg );
}


/*
=============
Sys_Sleep
=============
*/
void Sys_Sleep( int msec ) {

	if ( msec < 0 ) {
		// special case: wait for event or network packet
		DWORD dwResult;
		msec = 300;
		do {
			dwResult = MsgWaitForMultipleObjects( 0, NULL, FALSE, msec, QS_ALLEVENTS );
		}
		while ( dwResult == WAIT_TIMEOUT && NET_Sleep( 10 * 1000 ) );
		//WaitMessage();
		return;
	}

	// busy wait there because Sleep(0) will relinquish CPU - which is not what we want
	//if ( msec == 0 )
	//	return;

	Sleep( msec );
}


/*
==============
Sys_Mkdir
==============
*/
qboolean Sys_Mkdir( const char *path )
{
	if ( _mkdir( path ) == 0 ) {
		return qtrue;
	} else {
		if ( errno == EEXIST ) {
			return qtrue;
		} else {
			return qfalse;
		}
	}
}


/*
==============
Sys_FOpen
==============
*/
FILE *Sys_FOpen( const char *ospath, const char *mode )
{
	size_t length;

	// Windows API ignores all trailing spaces and periods which can get around Quake 3 file system restrictions.
	length = strlen( ospath );
	if ( length == 0 || ospath[length-1] == ' ' || ospath[length-1] == '.' ) {
		return NULL;
	}

	return fopen( ospath, mode );
}


/*
==============
Sys_ResetReadOnlyAttribute
==============
*/
qboolean Sys_ResetReadOnlyAttribute( const char *ospath ) {
	DWORD dwAttr;

	dwAttr = GetFileAttributesA( ospath );
	if ( dwAttr & FILE_ATTRIBUTE_READONLY ) {
		dwAttr &= ~FILE_ATTRIBUTE_READONLY;
		if ( SetFileAttributesA( ospath, dwAttr ) ) {
			return qtrue;
		} else {
			return qfalse;
		}
	} else {
		return qfalse;
	}
}


/*
==============
Sys_IsHiddenFolder
Note: Assumes caller has already verified ospath in other means
==============
*/
qboolean Sys_IsHiddenFolder( const char *ospath ) {
	DWORD dwAttr;

	dwAttr = GetFileAttributesA( ospath );
	if ( dwAttr & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM) ) {
		return qtrue;
	}

	if ( ospath[0] == '.' ) {
		return qtrue;
	}

	return qfalse;
}


/*
==============
Sys_Pwd
==============
*/
const char *Sys_Pwd( void )
{
	static char pwd[ MAX_OSPATH ];
	TCHAR	buffer[ MAX_OSPATH ];
	char *s;

	if ( pwd[0] )
		return pwd;

	GetModuleFileName( NULL, buffer, ARRAY_LEN( buffer ) );
	buffer[ ARRAY_LEN( buffer ) - 1 ] = '\0';

	Q_strncpyz( pwd, WtoA( buffer ), sizeof( pwd ) );

	s = strrchr( pwd, PATH_SEP );
	if ( s ) 
		*s = '\0';
	else // bogus case?
	{
		if ( _getcwd( pwd, sizeof( pwd ) - 1 ) != NULL )
			pwd[ sizeof( pwd ) - 1 ] = '\0';
		else
		{
			pwd[0] = '\0';
			return pwd;
		}
	}

	return pwd;
}


/*
==============
Sys_DefaultBasePath
==============
*/
const char *Sys_DefaultBasePath( void )
{
	return Sys_Pwd();
}


/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/


/*
=============
Sys_ListExtFiles
=============
*/
static int Sys_ListExtFiles( const char *directory, const char *subdir, const char *extension, const char *filter, char **list, int maxfiles, int subdirs ) {
	char		search[MAX_OSPATH*2+MAX_QPATH+1];
	char		filename[MAX_OSPATH * 2];
	int			nfiles;
	struct _finddata_t findinfo;
	intptr_t	findhandle;
	int			flag;
	int			extLen;
	const char	*x;
	qboolean	hasPatterns;

	// passing a slash as extension will find directories
	if ( extension[0] == '/' && extension[1] == '\0' ) {
		extension = "";
		flag = 0;
	} else {
		flag = _A_SUBDIR;
	}

	extLen = (int)strlen( extension );
	hasPatterns = Com_HasPatterns( extension ); // contains either '?' or '*'
	if ( hasPatterns && extension[0] == '.' && extension[1] != '\0' ) {
		extension++;
	}

	nfiles = 0;

	if ( *subdir != '\0' ) {
		Com_sprintf( search, sizeof( search ), "%s\\%s\\*", directory, subdir );
	} else {
		Com_sprintf( search, sizeof( search ), "%s\\*", directory );
	}

	if ( subdirs > 0 ) {
		// handle recursion
		findhandle = _findfirst( search, &findinfo );
		if ( findhandle != -1 ) {
			do {
				if ( findinfo.attrib & _A_SUBDIR ) {
					if ( !Q_streq( findinfo.name, "." ) && !Q_streq( findinfo.name, ".." ) ) {
						char subdir2[MAX_OSPATH * 2 + MAX_QPATH + 1];
						if ( *subdir != '\0' ) {
							Com_sprintf( subdir2, sizeof( subdir2 ), "%s\\%s", subdir, findinfo.name );
						} else {
							Q_strncpyz( subdir2, findinfo.name, sizeof( subdir2 ) );
						}
						if ( nfiles >= maxfiles ) {
							break;
						}
						nfiles += Sys_ListExtFiles( directory, subdir2, extension, filter, list + nfiles, maxfiles - nfiles, subdirs - 1);
					}
				}
			} while ( _findnext( findhandle, &findinfo ) == 0 );
		}
		_findclose( findhandle );
	}

	Q_strcat( search, sizeof( search ), extension );

	findhandle = _findfirst( search, &findinfo );
	if ( findhandle == -1 ) {
		return nfiles;
	}

	do {
		if ( flag ^ ( findinfo.attrib & _A_SUBDIR ) ) {
			if ( *subdir != '\0' ) {
				Com_sprintf( filename, sizeof( filename ), "%s\\%s", subdir, findinfo.name );
			} else {
				Q_strncpyz( filename, findinfo.name, sizeof( filename ) );
			}
			if ( filter != NULL && *filter != '\0' ) {
				if ( !Com_FilterPath( filter, filename ) ) {
					continue;
				}
			} else if ( *extension != '\0' ) {
				if ( hasPatterns ) {
					x = strrchr( findinfo.name, '.' );
					if ( x == NULL || !Com_FilterExt( extension, x + 1 ) ) {
						continue;
					}
				} else {
					// check for exact extension
					const int length = strlen( findinfo.name );
					if ( length < extLen || Q_stricmp( findinfo.name + length - extLen, extension ) ) {
						continue;
					}
				}
			}
			if ( nfiles >= maxfiles ) {
				break;
			}
			list[ nfiles++ ] = FS_CopyString( filename );
		}
	} while ( _findnext( findhandle, &findinfo ) == 0 );

	_findclose( findhandle );

	return nfiles;

}

char** Sys_ListFiles( const char *directory, const char *extension, const char *filter, int *numfiles, int subdirs )
{
	char** listCopy;
	char* list[MAX_FOUND_FILES];
	int		i, nfiles;

	if ( extension == NULL ) {
		extension = "";
	}

	nfiles = Sys_ListExtFiles( directory, "", extension, filter, list, ARRAY_LEN( list ), subdirs );

	// copy list from stack, reserve extra space for NULL
	listCopy = Z_Malloc( (nfiles + 1) * sizeof( listCopy[0] ) );
	for ( i = 0; i < nfiles; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	if ( nfiles > 1 ) {
		Com_SortList( listCopy, nfiles - 1 );
		if ( nfiles > 2 ) {
			if ( Q_streq( listCopy[0], "." ) && Q_streq( listCopy[1], ".." ) ) {
				// emulate old strgtr() function sort behavior for special entries
				char* dot1 = listCopy[0];
				char* dot2 = listCopy[1];
				for ( i = 0; i < nfiles - 2; i++ ) {
					listCopy[i] = listCopy[i + 2];
				}
				listCopy[nfiles - 2] = dot1;
				listCopy[nfiles - 1] = dot2;
			}
		}
	}

	*numfiles = nfiles;
	return listCopy;
}


/*
=============
Sys_FreeFileList
=============
*/
void Sys_FreeFileList( char **list ) {
	int		i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}


/*
=============
Sys_GetFileStats
=============
*/
qboolean Sys_GetFileStats( const char *filename, fileOffset_t *size, fileTime_t *mtime, fileTime_t *ctime ) {
	struct _stat s;

	if ( _stat( filename, &s ) == 0 ) {
		*size = (fileOffset_t)s.st_size;
		*mtime = (fileTime_t)s.st_mtime;
		*ctime = (fileTime_t)s.st_ctime;
		return qtrue;
	} else {
		*size = 0;
		*mtime = *ctime = 0;
		return qfalse;
	}
}


/*
==============
Sys_PathIsDir
Test an file given OS path:
returns -1 if not found
returns 1 if directory
returns 0 otherwise
==============
*/
int Sys_PathIsDir( const char *path ) {
	struct _stat s;

	if ( _stat( path, &s ) == -1 ) {
		return -1;
	}
	if ( s.st_mode & _S_IFDIR ) {
		return 1;
	}
	return 0;
}


qboolean Sys_PathCmp( const char *path1, const char *path2 )
{
	char *r1, *r2;

	r1 = _fullpath(NULL, path1, MAX_OSPATH);
	r2 = _fullpath(NULL, path2, MAX_OSPATH);

	if(r1 && r2 && !Q_stricmp(r1, r2))
	{
		free(r1);
		free(r2);
		return qtrue;
	}

	free(r1);
	free(r2);
	return qfalse;
}


//========================================================

/*
========================================================================

LOAD/UNLOAD DLL

========================================================================
*/

static int dll_err_count = 0;

/*
=================
Sys_LoadLibrary
=================
*/
void *Sys_LoadLibrary( const char *name )
{
	const char *ext;

	if ( !name || !*name )
		return NULL;

	if ( FS_AllowedExtension( name, qfalse, &ext ) )
	{
		Com_Error( ERR_FATAL, "Sys_LoadLibrary: Unable to load library with '%s' extension", ext );
	}

	return (void *)LoadLibrary( AtoW( name ) );
}


/*
=================
Sys_LoadFunction
=================
*/
void *Sys_LoadFunction( void *handle, const char *name )
{
	void *symbol;

	if ( handle == NULL || name == NULL || *name == '\0' ) 
	{
		dll_err_count++;
		return NULL;
	}

	symbol = GetProcAddress( handle, name );
	if ( !symbol )
		dll_err_count++;

	return symbol;
}


/*
=================
Sys_LoadFunctionErrors
=================
*/
int Sys_LoadFunctionErrors( void )
{
	int result = dll_err_count;
	dll_err_count = 0;
	return result;
}


/*
=================
Sys_UnloadLibrary
=================
*/
void Sys_UnloadLibrary( void *handle )
{
	if ( handle )
		FreeLibrary( handle );
}


#ifndef DEDICATED
static qboolean Sys_DLLNeedsUnpacking( const char* name ) {
#if defined(DEDICATED)
	return qfalse;
#else
	if ( !Q_stricmpn( name, "qagame", 6 ) )
		return qfalse;
#ifdef _DEBUG
	return cl_connectedToPureServer != 0 ? qtrue : qfalse;
#else
	return qtrue;
#endif
#endif
}
#endif


/*
=================
Sys_LoadDll

Used to load a development dll instead of a virtual machine
=================
*/

static const char *win_dlerror(void) {
	TCHAR			temp[1024+1];
	static char		ospath[2][sizeof(temp)];
	static int		toggle;

	toggle ^= 1; // flip-flop to allow two returns without clash

	FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), temp, sizeof(temp)/sizeof(temp[0]), NULL );
	Q_strncpyz( ospath[toggle], WtoA(temp), sizeof(ospath[0]) );

	return ospath[toggle];
}

static void *try_dlopen(const char *base, const char *gamedir, const char *fname)
{
	void *libHandle;
	char *fn;

	fn = FS_BuildOSPath(base, gamedir, fname);
	Com_Printf("Sys_LoadGameDLL(%s)... \n", fn);

	libHandle = Sys_LoadLibrary(fn);

	if (!libHandle)
	{
		Com_Printf("Sys_LoadGameDLL(%s) failed:\n\"%s\"\n", fn, win_dlerror());
		return NULL;
	}

	Com_Printf("Sys_LoadGameDLL(%s): succeeded ...\n", fn);

	return libHandle;
}

const char* Sys_GetDLLName( const char *name ) {
	return va( "%s_mp_" ARCH_STRING DLL_EXT, name );
}

void *QDECL Sys_LoadGameDll( const char *name, vmMain_t *entryPoint, dllSyscall_t systemcalls ) {
	HINSTANCE libHandle;
	dllEntry_t	dllEntry;
	const char	*basepath;
	const char	*homepath;
	const char	*gamedir;
	char		filename[ MAX_QPATH ];
#if !defined( DEDICATED )
	char		*fn;
	qboolean	unpack = qfalse;
#endif
#ifdef _DEBUG
	TCHAR currpath[ MAX_OSPATH ];
#endif
	const char *err = NULL;

	Q_strncpyz( filename, Sys_GetDLLName( name ), sizeof( filename ) );

	basepath = Cvar_VariableString( "fs_basepath" );
	homepath = Cvar_VariableString( "fs_homepath" );
	gamedir = Cvar_VariableString( "fs_game" );
	if ( !*gamedir ) {
		gamedir = Cvar_VariableString( "fs_basegame" );
	}

#ifdef _DEBUG
	if ( GetCurrentDirectory( ARRAY_LEN( currpath ), currpath ) < ARRAY_LEN( currpath ) )
		libHandle = try_dlopen( WtoA( currpath ), gamedir, filename );
	else
#endif
		libHandle = NULL;

	// TTimo - this is only relevant for full client
	// if a full client runs a dedicated server, it's not affected by this
#if !defined( DEDICATED )
	// try gamepath first
	fn = FS_BuildOSPath( basepath, gamedir, filename );

	unpack = Sys_DLLNeedsUnpacking( name );
	// NERVE - SMF - extract dlls from pak file for security
	// we have to handle the game dll a little differently
	// TTimo - passing the exact path to check against
	//   (compatibility with other OSes loading procedure)
	if ( unpack ) {
		if ( !FS_CL_ExtractFromPakFile( fn, gamedir, filename, NULL ) ) {
			Com_Printf( "Sys_LoadGameDLL(%s/%s) failed to extract library\n", gamedir, name );
		}
		else {
			Com_Printf( "Sys_LoadGameDLL(%s/%s) library extraction succeeded\n", gamedir, name );
		}
	}
#endif
	if ( !libHandle ) {
		libHandle = try_dlopen(basepath, gamedir, filename);
	}

	if ( !libHandle && homepath && homepath[0] && Q_stricmp( basepath, homepath ) ) {
		libHandle = try_dlopen(homepath, gamedir, filename);
	}

#ifndef DEDICATED
	if ( Q_stricmpn( name, "qagame", 6) != 0 ) {
		if ( !libHandle && !unpack ) {
			fn = FS_BuildOSPath( basepath, gamedir, filename );
			if (!FS_CL_ExtractFromPakFile(fn, gamedir, filename, NULL))
			{
				Com_Printf("Sys_LoadGameDLL(%s/%s) failed to extract library\n", gamedir, name);
				return NULL;
			}
			Com_Printf("Sys_LoadGameDLL(%s/%s) library extraction succeeded\n", gamedir, name);
			libHandle = try_dlopen(basepath, gamedir, filename);
		}
	}
#endif

	// Last resort for missing DLLs or media mods
	// If mod requires a different cgame/ui this could cause problems
	if ( !libHandle && !strcmp( name, "ui" ) && strcmp( gamedir, BASEGAME ) != 0 ) {
		const char *temp = va( "%s%c%s", gamedir, PATH_SEP, filename );
		FS_SetFilterFlag( FS_EXCLUDE_OTHERGAMES );
		if ( !FS_SV_FileExists( temp ) && !FS_FileIsInPAK( filename, NULL, NULL ) ) {
			Com_Printf( "Sys_LoadDLL(%s/%s) trying %s override\n", gamedir, name, BASEGAME );
			libHandle = try_dlopen(basepath, BASEGAME, filename);
		}
		FS_SetFilterFlag( 0 );
	}

	if ( !libHandle ) {
#ifdef _DEBUG // in debug abort on failure
		Com_Error(ERR_FATAL, "Sys_LoadGameDLL(%s/%s) failed LoadLibrary() completely!", gamedir, name);
#else
		Com_Printf( "Sys_LoadGameDLL(%s/%s) failed LoadLibrary() completely!\n", gamedir, name );
#endif
		return NULL;
	}

	Sys_LoadFunctionErrors(); // reset counter

	dllEntry = ( dllEntry_t ) Sys_LoadFunction( libHandle, "dllEntry" ); 
	*entryPoint = ( vmMain_t ) Sys_LoadFunction( libHandle, "vmMain" );
	if ( !*entryPoint || !dllEntry ) {
		err = win_dlerror();
#ifdef _DEBUG
		Com_Error(ERR_FATAL, "Sys_LoadGameDLL(%s/%s) failed GetProcAddress(vmMain):\n\"%s\" !", gamedir, name, err);
#else
		Com_Printf("Sys_LoadGameDLL(%s/%s) failed GetProcAddress(vmMain):\n\"%s\"\n", gamedir, name, err);
#endif
		Sys_UnloadLibrary( libHandle );
		err = win_dlerror();
		if (err != NULL)
		{
			Com_Printf("Sys_LoadGameDLL(%s/%s) failed FreeLibrary:\n\"%s\"\n", gamedir, name, err);
		}
		return NULL;
	}

	Com_Printf( "Sys_LoadGameDLL(%s/%s) found **vmMain** at %p\n", gamedir, name, *entryPoint );
	dllEntry( systemcalls );
	Com_Printf( "Sys_LoadGameDLL(%s/%s) succeeded!\n", gamedir, name );

	return libHandle;
}


/*
=================
Sys_SendKeyEvents

Platform-dependent event handling
=================
*/
void Sys_SendKeyEvents( void )
{
#ifndef DEDICATED
	if ( !com_dedicated->integer )
		HandleEvents();
	else
#endif
	HandleConsoleEvents();
}


//================================================================


/*
==================
SetTimerResolution

Try to set lower timer period
==================
*/
static void SetTimerResolution( void )
{
	typedef HRESULT (WINAPI *pfnNtQueryTimerResolution)( PULONG MinRes, PULONG MaxRes, PULONG CurRes );
	typedef HRESULT (WINAPI *pfnNtSetTimerResolution)( ULONG NewRes, BOOLEAN SetRes, PULONG CurRes );
	pfnNtQueryTimerResolution pNtQueryTimerResolution;
	pfnNtSetTimerResolution pNtSetTimerResolution;
	ULONG curr, minr, maxr;
	HMODULE dll;

	dll = GetModuleHandle( T( "ntdll" ) );
	if ( dll )
	{
		pNtQueryTimerResolution = (pfnNtQueryTimerResolution) GetProcAddress( dll, "NtQueryTimerResolution" );
		pNtSetTimerResolution = (pfnNtSetTimerResolution) GetProcAddress( dll, "NtSetTimerResolution" );
		if ( pNtQueryTimerResolution && pNtSetTimerResolution )
		{
			pNtQueryTimerResolution( &minr, &maxr, &curr );
			if ( maxr < 5000 ) // well, we don't need less than 0.5ms periods for select()
				maxr = 5000;
			pNtSetTimerResolution( maxr, TRUE, &curr );
		}
	}
}


/*
================
Sys_Init

Called after the common systems (cvars, files, etc)
are initialized
================
*/
void Sys_Init( void ) {

	// make sure the timer is high precision, otherwise
	// NT gets 18ms resolution
	timeBeginPeriod( 1 );

	SetTimerResolution();

	Cmd_AddCommand( "clearviewlog", Sys_ClearViewlog_f );

	Cvar_Set( "arch", "winnt" );
}

//=======================================================================


static const char *GetExceptionName( DWORD code )
{
	static char buf[ 32 ];

	switch ( code )
	{
		case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
		case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
		case EXCEPTION_FLT_DENORMAL_OPERAND: return "FLT_DENORMAL_OPERAND";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
		case EXCEPTION_FLT_INEXACT_RESULT: return "FLT_INEXACT_RESULT";
		case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
		case EXCEPTION_FLT_OVERFLOW: return "FLT_OVERFLOW";
		case EXCEPTION_FLT_STACK_CHECK: return "FLT_STACK_CHECK";
		case EXCEPTION_FLT_UNDERFLOW: return "FLT_UNDERFLOW";
		case EXCEPTION_GUARD_PAGE: return "GUARD_PAGE";
		case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
		case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
		case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
		case EXCEPTION_INT_OVERFLOW: return "INT_OVERFLOW";
		case EXCEPTION_INVALID_DISPOSITION: return "INVALID_DISPOSITION";
		case EXCEPTION_INVALID_HANDLE: return "INVALID_HANDLE";
		case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
		case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
		case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
		default: break;
	}

	sprintf( buf, "0x%08X", (unsigned int)code );
	return buf;
}


/*
==================
ExceptionFilter

Restore gamma and hide fullscreen window in case of crash
==================
*/
static LONG WINAPI ExceptionFilter( struct _EXCEPTION_POINTERS *ExceptionInfo )
{
#ifndef DEDICATED
	if ( com_dedicated->integer == 0 ) {
		extern cvar_t *com_cl_running;
		if ( com_cl_running  && com_cl_running->integer ) {
			// assume we can restart client module
		} else {
			GLW_RestoreGamma();
			GLW_HideFullscreenWindow();
		}
	}
#endif

	if ( ExceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT )
	{
		char msg[256], name[MAX_OSPATH];
		const char *basename;
		HMODULE hModule, hKernel32;
		byte *addr;

		hModule = NULL;
		name[0] = '\0';
		basename = name;
		addr = (byte*)ExceptionInfo->ExceptionRecord->ExceptionAddress;

		hKernel32 = GetModuleHandleA( "kernel32" );
		if ( hKernel32 != NULL ) {
			typedef BOOL (WINAPI *PFN_GetModuleHandleExA)( DWORD dwFlags, LPCSTR lpModuleName, HMODULE *phModule );
			PFN_GetModuleHandleExA pGetModuleHandleExA;

			pGetModuleHandleExA = (PFN_GetModuleHandleExA) GetProcAddress( hKernel32, "GetModuleHandleExA" );
			if ( pGetModuleHandleExA != NULL ) {
				if ( pGetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)addr, &hModule ) ) {
					if (GetModuleFileNameA( hModule, name, ARRAY_LEN(name) - 1) != 0 ) {
						name[ARRAY_LEN(name) - 1] = '\0';
						basename = strrchr( name, '\\' );
						if ( basename ) {
							basename = basename + 1;
						}
						else {
							basename = strrchr( name, '/' );
							if ( basename ) {
								basename = basename + 1;
							}
						}
					}
				}
			}
		}

		if ( basename && *basename ) {
			Com_sprintf( msg, sizeof( msg ), "Exception Code: %s\nException Address: %s@%x",
				GetExceptionName( ExceptionInfo->ExceptionRecord->ExceptionCode ),
				basename, (uint32_t)(addr - (byte*)hModule) );
		} else {
			Com_sprintf( msg, sizeof( msg ), "Exception Code: %s\nException Address: %p",
				GetExceptionName( ExceptionInfo->ExceptionRecord->ExceptionCode ),
				addr );
		}

		Com_Error( ERR_DROP, "Unhandled exception caught\n%s", msg );
	}

	return EXCEPTION_EXECUTE_HANDLER;
}


/*
==================
WinMain
==================
*/
#if defined( USE_SDL3 ) && !defined( DEDICATED )
void Sys_GetSDLVersion( uint32_t *major, uint32_t *minor, uint32_t *patch );
#endif

#if defined(USE_SDL3)
int main( int argc, char **argv )
#else
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
#endif
{
	static char	sys_cmdline[ MAX_STRING_CHARS ];
	char con_title[ MAX_CVAR_VALUE_STRING ];
	int xpos, ypos;
	qboolean useXYpos;
	HANDLE hProcess;
	DWORD dwPriority;
#if defined( USE_SDL3 ) && !defined( DEDICATED )
	uint32_t major, minor, patch;
	Sys_GetSDLVersion( &major, &minor, &patch );
#endif

#if !defined(USE_SDL3)
	// should never get a previous instance in Win32
	if ( hPrevInstance ) {
		return 0;
	}
#endif

	// slightly boost process priority if it set to default
	hProcess = GetCurrentProcess();
	dwPriority = GetPriorityClass( hProcess );
	if ( dwPriority == NORMAL_PRIORITY_CLASS || dwPriority == ABOVE_NORMAL_PRIORITY_CLASS ) {
		SetPriorityClass( hProcess, HIGH_PRIORITY_CLASS );
	}

	SetupDPIAwareness();

#if defined(USE_SDL3)
	g_wv.hInstance = (HINSTANCE)GetModuleHandle(NULL);
	Q_strncpyz( sys_cmdline, GetCommandLine(), sizeof( sys_cmdline ) );
#else
	g_wv.hInstance = hInstance;
	Q_strncpyz( sys_cmdline, lpCmdLine, sizeof( sys_cmdline ) );
#endif

	useXYpos = Com_EarlyParseCmdLine( sys_cmdline, con_title, sizeof( con_title ), &xpos, &ypos );

	// done before Com/Sys_Init since we need this for error output
	Sys_CreateConsole( con_title, xpos, ypos, useXYpos );

	// no abort/retry/fail errors
	SetErrorMode( SEM_FAILCRITICALERRORS );

	SetUnhandledExceptionFilter( ExceptionFilter );

	Com_Init( sys_cmdline );

#if ( defined( USE_SDL2 ) || defined( USE_SDL3 ) ) && !defined( DEDICATED )
	Com_Printf( "Using SDL Version %u.%u.%u\n", major, minor, patch );
#endif

	// hide the early console since we've reached the point where we
	// have a working graphics subsystems
	if ( !com_dedicated->integer && !com_viewlog->integer ) {
		Sys_ShowConsole( 0, qfalse );
	}

	// main game loop
	while ( 1 ) {
		// set low precision every frame, because some system calls
		// reset it arbitrarily
		// _controlfp( _PC_24, _MCW_PC );
		// _controlfp( -1, _MCW_EM  ); // no exceptions, even if some crappy syscall turns them back on!

#ifdef DEDICATED
		// run the game
		Com_Frame( qfalse );
#else
		// make sure mouse and joystick are only called once a frame
		IN_Frame();
		// run the game
		Com_Frame( CL_NoDelay() );
#endif
	}

	// never gets here
	return 0;
}

int Sys_GetPID( void ) {
	return (int)GetCurrentProcessId();
}
