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
#define _GNU_SOURCE
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pwd.h>
#include <dlfcn.h>
#include <libgen.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

//=============================================================================

/*
================
Sys_Milliseconds
================
*/
/* base time in seconds, that's our origin
   timeval:tv_sec is an int: 
   assuming this wraps every 0x7fffffff - ~68 years since the Epoch (1970) - we're safe till 2038
   using unsigned long data type to work right with Sys_XTimeToSysTime */
unsigned int sys_timeBase = 0;
/* current time in ms, using sys_timeBase as origin
   NOTE: sys_timeBase*1000 + curtime -> ms since the Epoch
     0x7fffffff ms - ~24 days
   although timeval:tv_usec is an int, I'm not sure wether it is actually used as an unsigned int
     (which would affect the wrap period) */
int Sys_Milliseconds( void )
{
	int curtime;
	struct timespec ts;
	
#ifdef CLOCK_MONOTONIC_RAW
	clock_gettime( CLOCK_MONOTONIC_RAW, &ts );
#else
	clock_gettime( CLOCK_MONOTONIC, &ts );
#endif
	
	if( !sys_timeBase )
	{
		sys_timeBase = ts.tv_sec;
		return ts.tv_nsec / 1000000;
	}
	
	curtime = ( ts.tv_sec - sys_timeBase ) * 1000 + ts.tv_nsec / 1000000;
	
	return curtime;
#if 0
	struct timeval tp;
	int curtime;

	gettimeofday( &tp, NULL );
	
	if ( !sys_timeBase )
	{
		sys_timeBase = tp.tv_sec;
		return tp.tv_usec/1000;
	}

	curtime = (tp.tv_sec - sys_timeBase) * 1000 + tp.tv_usec / 1000;
	
	return curtime;
#endif
}


/*
==================
Sys_RandomBytes
==================
*/
qboolean Sys_RandomBytes( byte *string, int len )
{
	FILE *fp;

	fp = fopen( "/dev/urandom", "r" );
	if( !fp )
		return qfalse;

	setvbuf( fp, NULL, _IONBF, 0 ); // don't buffer reads from /dev/urandom

	if ( fread( string, sizeof( byte ), len, fp ) != len ) {
		fclose( fp );
		return qfalse;
	}

	fclose( fp );
	return qtrue;
}


//============================================


static int Sys_ListExtFiles( const char *directory, const char *subdir, const char *extension, const char *filter, char **list, int maxfiles, int subdirs )
{
	char		search[MAX_OSPATH * 2 + MAX_QPATH + 1];
	char		filename[MAX_OSPATH * 2];
	int		nfiles;
	struct dirent	*d;
	DIR		*fdir;
	int		extLen;
	struct stat st;
	qboolean	hasPatterns;
	const char	*x;
	qboolean	dironly;

	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		dironly = qtrue;
	} else {
		dironly = qfalse;
	}

	extLen = (int)strlen( extension );
	hasPatterns = Com_HasPatterns( extension ); // contains either '?' or '*'
	if ( hasPatterns && extension[0] == '.' && extension[1] != '\0' ) {
		extension++;
	}

	nfiles = 0;

	if ( *subdir != '\0' ) {
		Com_sprintf( search, sizeof( search ), "%s/%s", directory, subdir );
	} else {
		Com_sprintf( search, sizeof( search ), "%s", directory );
	}

	if ((fdir = opendir(search)) == NULL) {
		return nfiles;
	}

	// search
	while ((d = readdir(fdir)) != NULL) {
		if ( search[0] != '\0' ) {
			Com_sprintf( filename, sizeof( filename ), "%s/%s", search, d->d_name );
		} else {
			Q_strncpyz( filename, d->d_name, sizeof( filename ) );
		}
		if (stat(filename, &st) == -1) {
			continue;
		}
		if (st.st_mode & S_IFDIR) {
			// handle recursion
			if ( subdirs > 0 ) {
				if ( !Q_streq( d->d_name, "." ) && !Q_streq( d->d_name, ".." ) ) {
					char subdir2[MAX_OSPATH * 2 + MAX_QPATH + 1];
					if ( *subdir != '\0' ) {
						Com_sprintf( subdir2, sizeof( subdir2 ), "%s/%s", subdir, d->d_name );
					} else {
						Q_strncpyz( subdir2, d->d_name, sizeof( subdir2 ) );
					}
					if ( nfiles >= maxfiles ) {
						break;
					}
					nfiles += Sys_ListExtFiles( directory, subdir2, extension, filter, list + nfiles, maxfiles - nfiles, subdirs - 1);
				}
			}
			if ( !dironly ) {
				continue;
			}
		} else {
			if ( dironly ) {
				continue;
			}
		}
		if ( *subdir != '\0' ) {
			Com_sprintf( filename, sizeof( filename ), "%s/%s", subdir, d->d_name );
		} else {
			Q_strncpyz( filename, d->d_name, sizeof( filename ) );
		}
		if ( filter != NULL && *filter != '\0' ) {
			if ( !Com_FilterPath( filter, filename ) ) {
				continue;
			}
		} else if ( *extension != '\0' ) {
			if ( hasPatterns ) {
				x = strrchr( d->d_name, '.' );
				if ( x == NULL || !Com_FilterExt( extension, x + 1 ) ) {
					continue;
				}
			} else {
				// check for exact extension
				const int length = strlen( d->d_name );
				if ( length < extLen || Q_stricmp( d->d_name + length - extLen, extension ) ) {
					continue;
				}
			}
		}
		if ( nfiles >= maxfiles ) {
			break;
		}
		list[ nfiles++ ] = FS_CopyString( filename );
	}

	closedir( fdir );

	return nfiles;
}


char** Sys_ListFiles( const char *directory, const char *extension, const char *filter, int *numfiles, int subdirs )
{
	char**	listCopy;
	char*	list[MAX_FOUND_FILES];
	int	i, nfiles;

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
=================
Sys_FreeFileList
=================
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
	struct stat s;

	if ( stat( filename, &s ) == 0 ) {
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


int Sys_PathIsDir( const char *path ) {
	struct stat s;

	if ( stat( path, &s ) == -1 ) {
		return -1;
	}
	if ( S_ISDIR( s.st_mode ) ) {
		return 1;
	}
	return 0;
}


/*
=================
Sys_Mkdir
=================
*/
qboolean Sys_Mkdir( const char *path )
{
	if ( mkdir( path, 0750 ) == 0 ) {
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
=================
Sys_FOpen
=================
*/
FILE *Sys_FOpen( const char *ospath, const char *mode )
{
	struct stat buf;

	// check if path exists and it is not a directory
	if ( stat( ospath, &buf ) == 0 && S_ISDIR( buf.st_mode ) )
		return NULL;

	return fopen( ospath, mode );
}


/*
==============
Sys_ResetReadOnlyAttribute
==============
*/
qboolean Sys_ResetReadOnlyAttribute( const char *ospath )
{
	return qfalse;
}


/*
==============
Sys_IsHiddenFolder
Note: Assumes caller has already verified ospath in other means
==============
*/
qboolean Sys_IsHiddenFolder( const char *ospath )
{
	if ( ospath[0] == '.' ) {
		return qtrue;
	}

	return qfalse;
}


/*
=================
Sys_Pwd
=================
*/
const char *Sys_Pwd( void ) 
{
	static char pwd[ MAX_OSPATH ];

	if ( pwd[0] )
		return pwd;

	// more reliable, linux-specific
	if ( readlink( "/proc/self/exe", pwd, sizeof( pwd ) - 1 ) != -1 )
	{
		pwd[ sizeof( pwd ) - 1 ] = '\0';
		dirname( pwd );
		return pwd;
	}

	if ( !getcwd( pwd, sizeof( pwd ) ) )
	{
		pwd[0] = '\0';
	}

	return pwd;
}


/*
=================
Sys_DefaultBasePath
=================
*/
const char *Sys_DefaultBasePath( void )
{
	return Sys_Pwd();
}


/*
=================
Sys_DefaultHomePath
=================
*/
const char *Sys_DefaultHomePath( void )
{
	// Used to determine where to store user-specific files
	static char homePath[ MAX_OSPATH ];

	const char *p;

	if ( *homePath )
		return homePath;

#ifdef __APPLE__
	if ( (p = getenv("HOME")) != NULL ) 
	{
		Q_strncpyz( homePath, p, sizeof( homePath ) );

		Q_strcat( homePath, sizeof(homePath), "/Library/Application Support/Wolfenstein ET" );
		if ( mkdir( homePath, 0750 ) ) 
		{
			if ( errno != EEXIST ) 
				Com_Error( ERR_FATAL, "Unable to create directory \"%s\", error is %s(%d)\n", 
					homePath, strerror( errno ), errno );
		}
		return homePath;
	}
#else
	if ( (p = getenv("FLATPAK_ID")) != NULL && *p != '\0' )
	{
		if ( (p = getenv("XDG_DATA_HOME")) != NULL && *p != '\0' )
		{
			Q_strncpyz( homePath, p, sizeof( homePath ) );
			Q_strcat( homePath, sizeof( homePath ), "/.etwolf" );
		}
		else if( ( p = getenv( "HOME" ) ) != NULL && *p != '\0' )
		{
			Q_strncpyz( homePath, p, sizeof( homePath ) );
			Q_strcat( homePath, sizeof( homePath ), "/.etwolf" );
		}
		if ( mkdir( homePath, 0750 ) )
		{
			if ( errno != EEXIST )
				Com_Error( ERR_FATAL, "Unable to create directory \"%s\", error is %s(%d)\n",
					homePath, strerror( errno ), errno );
		}
	}
	if ( (p = getenv("HOME")) != NULL )
	{
		Q_strncpyz( homePath, p, sizeof( homePath ) );
		Q_strcat( homePath, sizeof( homePath ), "/.etwolf" );
		if ( mkdir( homePath, 0750 ) )
		{
			if ( errno != EEXIST )
				Com_Error( ERR_FATAL, "Unable to create directory \"%s\", error is %s(%d)\n",
					homePath, strerror( errno ), errno );
		}
		return homePath;
	}
#endif
	return ""; // assume current dir
}


/*
================
Sys_SteamPath
================
*/
const char *Sys_SteamPath( void )
{
	static char steamPath[ MAX_OSPATH ];
#if 0
	const char *p;

	if( ( p = getenv( "HOME" ) ) != NULL )
	{
		// Assumes steam root path as follows, and steam native not steam inside of wine
#ifdef __APPLE__
		const char *steamPathEnd = "/Library/Application Support/Steam/SteamApps/common/" STEAMPATH_NAME;
#else
		const char *steamPathEnd = "/.steam/steam/steamapps/common/" STEAMPATH_NAME;
#endif
		Com_sprintf(steamPath, sizeof(steamPath), "%s%s", p, steamPathEnd);
	}
#endif
	return steamPath;
}


const char *Sys_GogPath( void )
{
	return "";
}


const char *Sys_MicrosoftStorePath( void )
{
	return "";
}


/*
=================
Sys_ShowConsole
=================
*/
void Sys_ShowConsole( int visLevel, qboolean quitOnClose )
{
	// not implemented
}


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
	void *handle;

	if ( FS_AllowedExtension( name, qfalse, &ext ) )
	{
		Com_Error( ERR_FATAL, "Sys_LoadLibrary: Unable to load library with '%s' extension", ext );
	}

	handle = dlopen( name, RTLD_NOW );
	return handle;
}


/*
=================
Sys_UnloadLibrary
=================
*/
void Sys_UnloadLibrary( void *handle )
{
	if ( handle != NULL )
		dlclose( handle );
}


/*
=================
Sys_LoadFunction
=================
*/
void *Sys_LoadFunction( void *handle, const char *name )
{
	const char *error;
	char buf[1024];
	void *symbol;
	size_t nlen;

	if ( handle == NULL || name == NULL || *name == '\0' ) 
	{
		dll_err_count++;
		return NULL;
	}

	dlerror(); /* clear old error state */
	symbol = dlsym( handle, name );
	error = dlerror();
	if ( error != NULL )
	{
		nlen = strlen( name ) + 1;
		if ( nlen >= sizeof( buf ) )
			return NULL;
		buf[0] = '_';
		strcpy( buf+1, name );
		dlerror(); /* clear old error state */
		symbol = dlsym( handle, buf );
	}

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


#ifdef USE_AFFINITY_MASK
/*
=================
Sys_GetAffinityMask
=================
*/
uint64_t Sys_GetAffinityMask( void )
{
	cpu_set_t cpu_set;

	if ( sched_getaffinity( getpid(), sizeof( cpu_set ), &cpu_set ) == 0 ) {
		uint64_t mask = 0;
		int cpu;
		for ( cpu = 0; cpu < sizeof( mask ) * 8; cpu++ ) {
			if ( CPU_ISSET( cpu, &cpu_set ) ) {
				mask |= (1ULL << cpu);
			}
		}
		return mask;
	} else {
		return 0;
	}
}


/*
=================
Sys_SetAffinityMask
=================
*/
qboolean Sys_SetAffinityMask( const uint64_t mask )
{
	cpu_set_t cpu_set;
	int cpu;

	CPU_ZERO( &cpu_set );
	for ( cpu = 0; cpu < sizeof( mask ) * 8; cpu++ ) {
		if ( mask & (1ULL << cpu) ) {
			CPU_SET( cpu, &cpu_set );
		}
	}

	if ( sched_setaffinity( getpid(), sizeof( cpu_set ), &cpu_set ) == 0 ) {
		return qtrue;
	} else {
		return qfalse;
	}
}
#endif // USE_AFFINITY_MASK


qboolean Sys_IsSteamOverlayAttached( void )
{
#ifdef __linux__
	// See http://syprog.blogspot.ru/2011/12/listing-loaded-shared-objects-in-linux.html
	struct lmap
	{
		void *base_address;
		char *path;
		void *unused;
		struct lmap *next;
		struct lmap *prev;
	};

	struct dummy
	{
		void *pointers[3];
		struct dummy *ptr;
	};

	qboolean result = qfalse;
	struct dummy *processHandle = (struct dummy *)dlopen(NULL, RTLD_NOW);
	if (processHandle != NULL)
	{
		struct dummy *p = (struct dummy *)(processHandle)->ptr;
		struct lmap *pl = (struct lmap *)(p->ptr);
		while (pl != NULL)
		{
			if (strstr(pl->path, "gameoverlayrenderer.so") != NULL)
			{
				result = qtrue;
				break;
			}
			pl = pl->next;
		}
		dlclose(processHandle);
	}
	return result;
#else
	return qfalse; // Needed for OpenBSD, likely all other Unixes.
#endif
}
