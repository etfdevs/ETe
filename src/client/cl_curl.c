/*
===========================================================================
Copyright (C) 2006 Tony J. White (tjw@tjw.org)

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_CURL
#include "client.h"
#ifdef USE_CURL_DLOPEN
cvar_t *cl_cURLLib;
#endif

#define ALLOWED_PROTOCOLS ( CURLPROTO_HTTP | CURLPROTO_HTTPS | CURLPROTO_FTP | CURLPROTO_FTPS )

#define ALLOWED_PROTOCOLS_STR "http,https,ftp,ftps"

/*
==================================

Common CURL downloading functions

==================================
*/


/*
==================================
stristr

case-insensitive sub-string search
==================================
*/
const char* stristr( const char *source, const char *target )
{
	const char *p0, *p1, *p2, *pn;
	char c1, c2;

	if ( *target == '\0' )
	{
		return source;
	}

	pn = source;
	p1 = source;
	p2 = target;

	while ( *++p2 )
	{
		pn++;
	}

	while ( *pn != '\0' ) 
	{

		p0 = p1;
		p2 = target;

		while ( (c1 = *p1) != '\0' && (c2 = *p2) != '\0' )
		{
				if ( c1 <= 'Z' && c1 >= 'A' )
					c1 += ('a' - 'A');

				if ( c2 <= 'Z' && c2 >= 'A' )
					c2 += ('a' - 'A');

				if ( c1 != c2 )
				{
					break;
				}

				p1++;
				p2++;
		}

		if ( *p2 == '\0' )
		{
			return p0;
		}

		p1 = p0 + 1;
		pn++;
	}

	return NULL;
}


/*
==================================
replace1
==================================
*/
int replace1( const char src, const char dst, char *str )
{
	int count;

	if ( !str ) 
		return 0;

	count = 0;

	while ( *str != '\0' )
	{
		if ( *str == src )
		{
			*str = dst;
			count++;
		}
		str++;
	}

	return count;
}


/*
=================
Com_DL_Done
=================
*/
void Com_DL_Done( download_t *dl ) 
{
	if ( dl->func.lib )
		Sys_UnloadLibrary( dl->func.lib );
	dl->func.lib = NULL;
	memset( &dl->func, 0, sizeof( dl->func ) );
}


/*
=================
Com_DL_Init
=================
*/
qboolean Com_DL_Init( download_t *dl )
{
#ifdef USE_CURL_DLOPEN
	Com_Printf( "Loading \"%s\"...", cl_cURLLib->string );
	if( ( dl->func.lib = Sys_LoadLibrary( cl_cURLLib->string ) ) == NULL )
	{
#ifdef _WIN32
		return qfalse;
#else
		char fn[1024];

		Q_strncpyz( fn, Sys_Pwd(), sizeof( fn ) );
		strncat( fn, "/", sizeof( fn ) - strlen( fn ) - 1 );
		strncat( fn, cl_cURLLib->string, sizeof( fn ) - strlen( fn ) - 1 );

		if ( ( dl->func.lib = Sys_LoadLibrary( fn ) ) == NULL )
		{
#ifdef ALTERNATE_CURL_LIB
			// On some linux distributions there is no libcurl.so.3, but only libcurl.so.4. That one works too.
			if( ( dl->func.lib = Sys_LoadLibrary( ALTERNATE_CURL_LIB ) ) == NULL )
			{
				return qfalse;
			}
#else
			return qfalse;
#endif
		}
#endif /* _WIN32 */
	}

	Sys_LoadFunctionErrors(); // reset error count;

	dl->func.version = Sys_LoadFunction( dl->func.lib, "curl_version" );
	dl->func.easy_escape = Sys_LoadFunction( dl->func.lib, "curl_easy_escape" );
	dl->func.free = Sys_LoadFunction( dl->func.lib, "curl_free" );

	dl->func.easy_init = Sys_LoadFunction( dl->func.lib, "curl_easy_init" );
	dl->func.easy_setopt = Sys_LoadFunction( dl->func.lib, "curl_easy_setopt" );
	dl->func.easy_perform = Sys_LoadFunction( dl->func.lib, "curl_easy_perform" );
	dl->func.easy_cleanup = Sys_LoadFunction( dl->func.lib, "curl_easy_cleanup" );
	dl->func.easy_getinfo = Sys_LoadFunction( dl->func.lib, "curl_easy_getinfo" );
	dl->func.easy_strerror = Sys_LoadFunction( dl->func.lib, "curl_easy_strerror" );
	
	dl->func.multi_init = Sys_LoadFunction( dl->func.lib, "curl_multi_init" );
	dl->func.multi_add_handle = Sys_LoadFunction( dl->func.lib, "curl_multi_add_handle" );
	dl->func.multi_remove_handle = Sys_LoadFunction( dl->func.lib, "curl_multi_remove_handle" );
	dl->func.multi_perform = Sys_LoadFunction( dl->func.lib, "curl_multi_perform" );
	dl->func.multi_cleanup = Sys_LoadFunction( dl->func.lib, "curl_multi_cleanup" );
	dl->func.multi_info_read = Sys_LoadFunction( dl->func.lib, "curl_multi_info_read" );
	dl->func.multi_strerror = Sys_LoadFunction( dl->func.lib, "curl_multi_strerror" );

	if ( Sys_LoadFunctionErrors() )
	{
		Com_DL_Done( dl );
		Com_Printf( "FAIL: One or more symbols not found\n" );
		return qfalse;
	}

	Com_Printf( "OK\n" );

	return qtrue;
#else

	dl->func.lib = NULL;

	dl->func.version = curl_version;
	dl->func.easy_escape = curl_easy_escape;
	dl->func.free = (void (*)(char *))curl_free; // cast to silence warning

	dl->func.easy_init = curl_easy_init;
	dl->func.easy_setopt = curl_easy_setopt;
	dl->func.easy_perform = curl_easy_perform;
	dl->func.easy_cleanup = curl_easy_cleanup;
	dl->func.easy_getinfo = curl_easy_getinfo;
	dl->func.easy_strerror = curl_easy_strerror;
	
	dl->func.multi_init = curl_multi_init;
	dl->func.multi_add_handle = curl_multi_add_handle;
	dl->func.multi_remove_handle = curl_multi_remove_handle;
	dl->func.multi_perform = curl_multi_perform;
	dl->func.multi_cleanup = curl_multi_cleanup;
	dl->func.multi_info_read = curl_multi_info_read;
	dl->func.multi_strerror = curl_multi_strerror;

	return qtrue;
#endif /* USE_CURL_DLOPEN */
}


/*
=================
Com_DL_Cleanup
=================
*/
qboolean Com_DL_InProgress( const download_t *dl )
{
	if ( dl->cURL && dl->URL[0] )
		return qtrue;
	else
		return qfalse;
}


/*
=================
Com_DL_Cleanup
=================
*/
void Com_DL_Cleanup( download_t *dl )
{
	if( dl->cURLM )
	{
		if ( dl->cURL )
		{
			dl->func.multi_remove_handle( dl->cURLM, dl->cURL );
			dl->func.easy_cleanup( dl->cURL );
		}
		dl->func.multi_cleanup( dl->cURLM );
		dl->cURLM = NULL;
		dl->cURL = NULL;
	}
	else if( dl->cURL )
	{
		dl->func.easy_cleanup( dl->cURL );
		dl->cURL = NULL;
	}
	if ( dl->fHandle != FS_INVALID_HANDLE )
	{
		FS_FCloseFile( dl->fHandle );
		dl->fHandle = FS_INVALID_HANDLE;
	}

	if ( dl->mapAutoDownload )
	{
		Cvar_Set( "cl_downloadName", "" );
		Cvar_Set( "cl_downloadSize", "0" );
		Cvar_Set( "cl_downloadCount", "0" );
		Cvar_Set( "cl_downloadTime", "0" );
	}

	dl->Size = 0;
	dl->Count = 0;

	dl->URL[0] = '\0';
	dl->Name[0] = '\0';
	if ( dl->TempName[0] )
	{
		FS_Remove( dl->TempName );
	}
	dl->TempName[0] = '\0';
	dl->progress[0] = '\0';
	dl->headerCheck = qfalse;
	dl->mapAutoDownload = qfalse;

	Com_DL_Done( dl );
}


static const char *sizeToString( int size )
{
	static char buf[ 32 ];
	if ( size < 1024 ) {
		sprintf( buf, "%iB", size );
	} else if ( size < 1024*1024 ) {
		sprintf( buf, "%iKB", size / 1024 );
	} else {
		sprintf( buf, "%i.%iMB", size / (1024*1024), (size / (1024*1024/10 )) % 10 );
	}
	return buf;
}


/*
=================
Com_DL_CallbackProgress
=================
*/
#if CURL_AT_LEAST_VERSION(7, 32, 0)
static int Com_DL_CallbackProgress( void *data, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow )
#else
static int Com_DL_CallbackProgress( void *data, double dltotal, double dlnow, double ultotal, double ulnow )
#endif
{
#if CURL_AT_LEAST_VERSION(7, 55, 0)
	curl_off_t speed;
#else
	double speed;
#endif

#if CURL_AT_LEAST_VERSION(7, 32, 0)
	curl_off_t percentage;
#else
	double percentage;
#endif

	download_t *dl = (download_t *)data;

	dl->Size = (int)dltotal;
	dl->Count = (int)dlnow;

	if ( dl->mapAutoDownload && cls.state == CA_CONNECTED )
	{
		if ( Key_IsDown( K_ESCAPE ) )
		{
			Com_Printf( "%s: aborted\n", dl->Name );
			return -1;
		}
		Cvar_SetIntegerValue( "cl_downloadSize", dl->Size );
		Cvar_SetIntegerValue( "cl_downloadCount", dl->Count );
	}

	if ( dl->Size ) {
#if CURL_AT_LEAST_VERSION(7, 32, 0)
		percentage = ( dlnow * 100 ) / dltotal;
#else
		percentage = ( dlnow / dltotal ) * 100.0;
#endif
		sprintf( dl->progress, " downloading %s: %s (%i%%)", dl->Name, sizeToString( dl->Count ), (int)percentage );
	} else {
		sprintf( dl->progress, " downloading %s: %s", dl->Name, sizeToString( dl->Count ) );
	}

#if CURL_AT_LEAST_VERSION(7, 55, 0)
	if ( dl->func.easy_getinfo( dl->cURL, CURLINFO_SPEED_DOWNLOAD_T, &speed ) == CURLE_OK )
#else
	if ( dl->func.easy_getinfo( dl->cURL, CURLINFO_SPEED_DOWNLOAD, &speed ) == CURLE_OK )
#endif
	{
		Q_strcat( dl->progress, sizeof( dl->progress ), va( " %s/s", sizeToString( (int)speed ) ) );
	}

	return 0;
}


/*
=================
Com_DL_CallbackWrite
=================
*/
static size_t Com_DL_CallbackWrite( void *ptr, size_t size, size_t nmemb, void *userdata )
{
	download_t *dl;

	dl = (download_t *)userdata;

	if ( dl->fHandle == FS_INVALID_HANDLE )
	{
		if ( !CL_ValidPakSignature( ptr, size*nmemb ) ) 
		{
			Com_Printf( S_COLOR_YELLOW "Com_DL_CallbackWrite(): invalid pak signature for %s.\n",
				dl->Name );
			return (size_t)-1;
		}

		dl->fHandle = FS_SV_FOpenFileWrite( dl->TempName );
		if ( dl->fHandle == FS_INVALID_HANDLE ) 
		{
			return (size_t)-1;
		}
	}

	FS_Write( ptr, size*nmemb, dl->fHandle );

	return (size * nmemb);
}


/*
=================
Com_DL_ValidFileName
=================
*/
qboolean Com_DL_ValidFileName( const char *fileName )
{
	int c;
	while ( (c = *fileName++) != '\0' )
	{
		if ( c == '/' || c == '\\' || c == ':' )
			return qfalse;
		if ( c < ' ' || c > '~' )
			return qfalse;
	}
	return qtrue;
}


/*
=================
Com_DL_HeaderCallback
=================
*/
static size_t Com_DL_HeaderCallback( void *ptr, size_t size, size_t nmemb, void *userdata )
{
	char name[MAX_OSPATH];
	char header[1024], *s, quote, *d;
	download_t *dl;
	int len;

	if ( size*nmemb >= sizeof( header ) )
	{
		Com_Printf( S_COLOR_RED "Com_DL_HeaderCallback: header is too large." );
		return (size_t)-1;
	}

	dl = (download_t *)userdata;
	
	memcpy( header, ptr, size*nmemb+1 );
	header[ size*nmemb ] = '\0';

	//Com_Printf( "h: %s\n--------------------------\n", header );

	s = (char*)stristr( header, "content-disposition:" );
	if ( s ) 
	{
		s += 20; // strlen( "content-disposition:" )
		s = (char*)stristr( s, "filename=" );
		if ( s ) 
		{
			s += 9; // strlen( "filename=" )
			
			d = name;
			replace1( '\r', '\0', s );
			replace1( '\n', '\0', s );

			// prevent overflow
			if ( strlen( s ) >= sizeof( name ) )
				s[ sizeof( name ) - 1 ] = '\0';

			if ( *s == '\'' || *s == '"' )
				quote = *s++;
			else
				quote = '\0';

			// copy filename
			while ( *s != '\0' && *s != quote ) 
				*d++ = *s++;
			len = d - name;
			*d++ = '\0';

			// validate
			if ( len < 5 || !stristr( name + len - 4, ".pk3" ) || !Com_DL_ValidFileName( name ) )
			{
				Com_Printf( S_COLOR_RED "Com_DL_HeaderCallback: bad file name '%s'\n", name );
				return (size_t)-1;
			}

			// strip extension
			FS_StripExt( name, ".pk3" );

			// store in
			strcpy( dl->Name, name );
		}
	}

	return size*nmemb;
}


/*
===============================================================
Com_DL_Begin()

Start downloading file from remoteURL and save it under fs_game/localName
==============================================================
*/
qboolean Com_DL_Begin( download_t *dl, const char *localName, const char *remoteURL, qboolean autoDownload )
{
	char *s;

	if ( Com_DL_InProgress( dl ) )
	{
		Com_Printf( S_COLOR_YELLOW " already downloading %s\n", dl->Name );
		return qfalse;
	}

	Com_DL_Cleanup( dl );

	if ( !Com_DL_Init( dl ) ) 
	{
		Com_Printf( S_COLOR_YELLOW "Error initializing cURL library\n" );
		return qfalse;
	}

	dl->cURL = dl->func.easy_init();
	if ( !dl->cURL ) 
	{
		Com_Printf( S_COLOR_RED "Com_DL_Begin: easy_init() failed\n" );
		Com_DL_Cleanup( dl );
		return qfalse;
	}

	{
		char *escapedName = dl->func.easy_escape( dl->cURL, localName, 0 );
		if ( !escapedName ) 
		{
			Com_Printf( S_COLOR_RED "Com_DL_Begin: easy_escape() failed\n" );
			Com_DL_Cleanup( dl );
			return qfalse;
		}

		Q_strncpyz( dl->URL, remoteURL, sizeof( dl->URL ) );

		if ( cl_dlDirectory->integer ) {
			Q_replace( "%game%", FS_GetBaseGameDir(), dl->URL, sizeof( dl->URL ) );
		} else {
			Q_replace( "%game%", FS_GetCurrentGameDir(), dl->URL, sizeof( dl->URL ) );
		}

		if ( !Q_replace( "%1", escapedName, dl->URL, sizeof( dl->URL ) ) )
		{
			if ( dl->URL[strlen(dl->URL)] != '/' )
				Q_strcat( dl->URL, sizeof( dl->URL ), "/" );
			Q_strcat( dl->URL, sizeof( dl->URL ), escapedName );
			dl->headerCheck = qfalse;
		}
		else
		{
			dl->headerCheck = qtrue;
		}
		dl->func.free( escapedName );
	}

	Com_Printf( "URL: %s\n", dl->URL );

	if ( cl_dlDirectory->integer ) {
		Q_strncpyz( dl->gameDir, FS_GetBaseGameDir(), sizeof( dl->gameDir ) );
	} else {
		Q_strncpyz( dl->gameDir, FS_GetCurrentGameDir(), sizeof( dl->gameDir ) );
	}

	// try to extract game path from localName
	// dl->Name should contain only pak name without game dir and extension
	s = strrchr( localName, '/' );
	if ( s ) 
		Q_strncpyz( dl->Name, s+1, sizeof( dl->Name ) );
	else
		Q_strncpyz( dl->Name, localName, sizeof( dl->Name ) );

	FS_StripExt( dl->Name, ".pk3" );
	if ( !dl->Name[0] )
	{
		Com_Printf( S_COLOR_YELLOW " empty filename after extension strip.\n" );
		return qfalse;
	}

	Com_sprintf( dl->TempName, sizeof( dl->TempName ), 
		"%s%c%s.%08x.tmp", dl->gameDir, PATH_SEP, dl->Name, rand() | (rand() << 16) );

	if ( com_developer->integer )
		dl->func.easy_setopt( dl->cURL, CURLOPT_VERBOSE, 1 );

	dl->func.easy_setopt( dl->cURL, CURLOPT_URL, dl->URL );
	dl->func.easy_setopt( dl->cURL, CURLOPT_TRANSFERTEXT, 0 );
	//dl->func.easy_setopt( dl->cURL, CURLOPT_REFERER, "q3a://127.0.0.1" );
	dl->func.easy_setopt( dl->cURL, CURLOPT_REFERER, dl->URL );
	dl->func.easy_setopt( dl->cURL, CURLOPT_USERAGENT, Q3_VERSION );
	dl->func.easy_setopt( dl->cURL, CURLOPT_WRITEFUNCTION, Com_DL_CallbackWrite );
	dl->func.easy_setopt( dl->cURL, CURLOPT_WRITEDATA, dl );
	if ( dl->headerCheck ) 
	{
		dl->func.easy_setopt( dl->cURL, CURLOPT_HEADERFUNCTION, Com_DL_HeaderCallback );
		dl->func.easy_setopt( dl->cURL, CURLOPT_HEADERDATA, dl );
	}
	dl->func.easy_setopt( dl->cURL, CURLOPT_NOPROGRESS, 0 );
#if CURL_AT_LEAST_VERSION(7, 32, 0)
	dl->func.easy_setopt( dl->cURL, CURLOPT_XFERINFOFUNCTION, Com_DL_CallbackProgress );
	dl->func.easy_setopt( dl->cURL, CURLOPT_XFERINFODATA, dl );
#else
	dl->func.easy_setopt( dl->cURL, CURLOPT_PROGRESSFUNCTION, Com_DL_CallbackProgress );
	dl->func.easy_setopt( dl->cURL, CURLOPT_PROGRESSDATA, dl );
#endif
	dl->func.easy_setopt( dl->cURL, CURLOPT_FAILONERROR, 1 );
	dl->func.easy_setopt( dl->cURL, CURLOPT_FOLLOWLOCATION, 1 );
	dl->func.easy_setopt( dl->cURL, CURLOPT_MAXREDIRS, 5 );
#if CURL_AT_LEAST_VERSION(7, 85, 0)
	dl->func.easy_setopt( dl->cURL, CURLOPT_PROTOCOLS_STR, ALLOWED_PROTOCOLS_STR );
#else
	dl->func.easy_setopt( dl->cURL, CURLOPT_PROTOCOLS, ALLOWED_PROTOCOLS );
#endif

#ifdef CURL_MAX_READ_SIZE
	dl->func.easy_setopt( dl->cURL, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE );
#endif

	dl->cURLM = dl->func.multi_init();

	if ( !dl->cURLM )
	{
		Com_DL_Cleanup( dl );
		Com_Printf( S_COLOR_RED "Com_DL_Begin: multi_init() failed\n" );
		return qfalse;
	}

	if ( dl->func.multi_add_handle( dl->cURLM, dl->cURL ) != CURLM_OK ) 
	{
		Com_DL_Cleanup( dl );
		Com_Printf( S_COLOR_RED "Com_DL_Begin: multi_add_handle() failed\n" );
		return qfalse;
	}

	dl->mapAutoDownload = autoDownload;

	if ( dl->mapAutoDownload )
	{
		Cvar_Set( "cl_downloadName", dl->Name );
		Cvar_Set( "cl_downloadSize", "0" );
		Cvar_Set( "cl_downloadCount", "0" );
		Cvar_SetIntegerValue( "cl_downloadTime", cls.realtime );
	}

	return qtrue;
}


qboolean Com_DL_Perform( download_t *dl )
{
	char name[ sizeof( dl->TempName ) ];
	CURLMcode res;
	CURLMsg *msg;
	long code;
	int c, n;
	int i;

	res = dl->func.multi_perform( dl->cURLM, &c );

	n = 128;

	i = 0;
	while( res == CURLM_CALL_MULTI_PERFORM && i < n )
	{
		res = dl->func.multi_perform( dl->cURLM, &c );
		i++;
	}
	if( res == CURLM_CALL_MULTI_PERFORM )
	{
		return qtrue;
	}

	msg = dl->func.multi_info_read( dl->cURLM, &c );
	if( msg == NULL )
	{
		return qtrue;
	}

	if ( dl->fHandle != FS_INVALID_HANDLE )
	{
		FS_FCloseFile( dl->fHandle );
		dl->fHandle = FS_INVALID_HANDLE;
	}

	if ( msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK )
	{
		qboolean autoDownload = dl->mapAutoDownload;

		Com_sprintf( name, sizeof( name ), "%s%c%s.pk3", dl->gameDir, PATH_SEP, dl->Name );

		if ( !FS_SV_FileExists( name ) )
		{
			FS_SV_Rename( dl->TempName, name );
		}
		else
		{
			n = FS_GetZipChecksum( name );
			Com_sprintf( name, sizeof( name ), "%s%c%s.%08x.pk3", dl->gameDir, PATH_SEP, dl->Name, n );

			if ( FS_SV_FileExists( name ) )
				FS_Remove( name );

			FS_SV_Rename( dl->TempName, name );
		}

		Com_DL_Cleanup( dl );
		FS_Reload(); //clc.downloadRestart = qtrue;
		Com_Printf( S_COLOR_GREEN "%s downloaded\n", name );
		if ( autoDownload )
		{
			if ( cls.state == CA_CONNECTED && !clc.demoplaying )
			{
				CL_AddReliableCommand( "donedl", qfalse ); // get new gamestate info from server
			} 
			else if ( clc.demoplaying )
			{
				// FIXME: there might be better solution than vid_restart
				cls.startCgame = qtrue;
				Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
			}
		}
		return qfalse;
	}
	else
	{
		qboolean autoDownload = dl->mapAutoDownload;
		dl->func.easy_getinfo( msg->easy_handle, CURLINFO_RESPONSE_CODE, &code );
		Com_Printf( S_COLOR_RED "Download Error: %s Code: %ld\n",
			dl->func.easy_strerror( msg->data.result ), code );
		strcpy( name, dl->TempName );
		Com_DL_Cleanup( dl );
		FS_Remove( name );
		if ( autoDownload )
		{
			if ( cls.state == CA_CONNECTED )
			{
				Com_Error( ERR_DROP, "%s", "download error" );
			}
		}
	}

	return qtrue;
}

#define APP_NAME        "ID_DOWNLOAD"
#define APP_VERSION     "2.0"

static qboolean dl_curl_enabled = qfalse;

static CURLM *dl_multi = NULL;
static CURL *dl_request = NULL;
static FILE *dl_file = NULL;

#ifdef USE_CURL_DLOPEN

char* (*dl_curl_version)(void);

char* (*dl_curl_easy_escape)(CURL *curl, const char *string, int length);
void (*dl_curl_free)(char *ptr);

CURL* (*dl_curl_easy_init)(void);
CURLcode (*dl_curl_easy_setopt)(CURL *curl, CURLoption option, ...);
CURLcode (*dl_curl_easy_perform)(CURL *curl);
void (*dl_curl_easy_cleanup)(CURL *curl);
CURLcode (*dl_curl_easy_getinfo)(CURL *curl, CURLINFO info, ...);
CURL* (*dl_curl_easy_duphandle)(CURL *curl);
void (*dl_curl_easy_reset)(CURL *curl);
const char *(*dl_curl_easy_strerror)(CURLcode);

CURLM* (*dl_curl_multi_init)(void);
CURLMcode (*dl_curl_multi_add_handle)(CURLM *multi_handle,
                                                CURL *curl_handle);
CURLMcode (*dl_curl_multi_remove_handle)(CURLM *multi_handle,
                                                CURL *curl_handle);
CURLMcode (*dl_curl_multi_fdset)(CURLM *multi_handle,
                                                fd_set *read_fd_set,
                                                fd_set *write_fd_set,
                                                fd_set *exc_fd_set,
                                                int *max_fd);
CURLMcode (*dl_curl_multi_perform)(CURLM *multi_handle,
                                                int *running_handles);
CURLMcode (*dl_curl_multi_cleanup)(CURLM *multi_handle);
CURLMsg *(*dl_curl_multi_info_read)(CURLM *multi_handle,
                                                int *msgs_in_queue);
const char *(*dl_curl_multi_strerror)(CURLMcode);

static void *dl_cURLLib = NULL;

/*
=================
GPA
=================
*/
static void *DLGPA(const char *str)
{
	void *rv;

	rv = Sys_LoadFunction(dl_cURLLib, str);
	if(!rv)
	{
		Com_Printf("Can't load symbol %s\n", str);
		dl_curl_enabled = qfalse;
		return NULL;
	}
	else
	{
		Com_DPrintf("Loaded symbol %s (0x%p)\n", str, rv);
        return rv;
	}
}
#endif /* USE_CURL_DLOPEN */

/*
=================
DL_Init
=================
*/
qboolean DL_Init( void )
{
#ifdef USE_CURL_DLOPEN
	if(dl_cURLLib)
		return qtrue;

	Com_Printf("Loading \"%s\"...", cl_cURLLib->string);
	if( (dl_cURLLib = Sys_LoadLibrary(cl_cURLLib->string)) == 0 )
	{
#ifdef _WIN32
		return qfalse;
#else
		char fn[1024];

		Q_strncpyz( fn, Sys_Pwd(), sizeof( fn ) );
		strncat( fn, "/", sizeof( fn ) - strlen( fn ) - 1 );
		strncat( fn, cl_cURLLib->string, sizeof( fn ) - strlen( fn ) - 1 );

		if((dl_cURLLib = Sys_LoadLibrary(fn)) == 0)
		{
#ifdef ALTERNATE_CURL_LIB
			// On some linux distributions there is no libcurl.so.3, but only libcurl.so.4. That one works too.
			if( (dl_cURLLib = Sys_LoadLibrary(ALTERNATE_CURL_LIB)) == 0 )
			{
				return qfalse;
			}
#else
			return qfalse;
#endif
		}
#endif /* _WIN32 */
	}

	dl_curl_enabled = qtrue;

	dl_curl_version = DLGPA("curl_version");

	dl_curl_easy_init = DLGPA("curl_easy_init");
	dl_curl_easy_setopt = DLGPA("curl_easy_setopt");
	dl_curl_easy_perform = DLGPA("curl_easy_perform");
	dl_curl_easy_cleanup = DLGPA("curl_easy_cleanup");
	dl_curl_easy_getinfo = DLGPA("curl_easy_getinfo");
	dl_curl_easy_duphandle = DLGPA("curl_easy_duphandle");
	dl_curl_easy_reset = DLGPA("curl_easy_reset");
	dl_curl_easy_strerror = DLGPA("curl_easy_strerror");
	
	dl_curl_multi_init = DLGPA("curl_multi_init");
	dl_curl_multi_add_handle = DLGPA("curl_multi_add_handle");
	dl_curl_multi_remove_handle = DLGPA("curl_multi_remove_handle");
	dl_curl_multi_fdset = DLGPA("curl_multi_fdset");
	dl_curl_multi_perform = DLGPA("curl_multi_perform");
	dl_curl_multi_cleanup = DLGPA("curl_multi_cleanup");
	dl_curl_multi_info_read = DLGPA("curl_multi_info_read");
	dl_curl_multi_strerror = DLGPA("curl_multi_strerror");

	if(!dl_curl_enabled)
	{
		DL_Shutdown();
		Com_Printf("FAIL One or more symbols not found\n");
		return qfalse;
	}
	Com_Printf("OK\n");

	return qtrue;
#else
	dl_curl_enabled = qtrue;
	return qtrue;
#endif /* USE_CURL_DLOPEN */
}

/*
=================
DL_Shutdown
=================
*/
void DL_Shutdown( void )
{
	DL_Cleanup();
#ifdef USE_CURL_DLOPEN
	if(dl_cURLLib)
	{
		Sys_UnloadLibrary(dl_cURLLib);
		dl_cURLLib = NULL;
	}
	dl_curl_easy_init = NULL;
	dl_curl_easy_setopt = NULL;
	dl_curl_easy_perform = NULL;
	dl_curl_easy_cleanup = NULL;
	dl_curl_easy_getinfo = NULL;
	dl_curl_easy_duphandle = NULL;
	dl_curl_easy_reset = NULL;

	dl_curl_multi_init = NULL;
	dl_curl_multi_add_handle = NULL;
	dl_curl_multi_remove_handle = NULL;
	dl_curl_multi_fdset = NULL;
	dl_curl_multi_perform = NULL;
	dl_curl_multi_cleanup = NULL;
	dl_curl_multi_info_read = NULL;
	dl_curl_multi_strerror = NULL;
#endif /* USE_CURL_DLOPEN */
}

void DL_Cleanup(void)
{
	if(dl_multi) {
		CURLMcode result;

		if(dl_request) {
			result = dl_curl_multi_remove_handle(dl_multi, dl_request);
			if(result != CURLM_OK) {
				Com_DPrintf("%s: dl_curl_multi_remove_handle failed: %s\n", __func__, dl_curl_multi_strerror(result));
			}
			dl_curl_easy_cleanup(dl_request);
		}
		result = dl_curl_multi_cleanup(dl_multi);
		if(result != CURLM_OK) {
			Com_DPrintf("%s: dl_curl_multi_cleanup failed: %s\n", __func__, dl_curl_multi_strerror(result));
		}
		dl_multi = NULL;
		dl_request = NULL;
	}
	else if(dl_request) {
		dl_curl_easy_cleanup(dl_request);
		dl_request = NULL;
	}

	if(dl_file) {
		fclose(dl_file);
		dl_file = NULL;
	}
}

/*
** Write to file
*/
static size_t DL_cb_FWriteFile( void *ptr, size_t size, size_t nmemb, void *stream ) {
	FILE *file = (FILE*)stream;
	long pos = ftell( file );
	if ( pos == 0 ) {
		if ( !CL_ValidPakSignature( ptr, size*nmemb ) ) 
		{
			Com_Printf( S_COLOR_YELLOW "DL_cb_FWriteFile(): invalid pak signature for %s.\n",
				Cvar_VariableString("cl_downloadName") );
			return (size_t)-1;
		}
	}
	return fwrite( ptr, size, nmemb, file );
}

/*
** Print progress
*/
#if CURL_AT_LEAST_VERSION(7, 32, 0)
static int DL_cb_Progress( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow )
#else
static int DL_cb_Progress( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow )
#endif
{
	/* cl_downloadSize and cl_downloadTime are set by the Q3 protocol...
	   and it would probably be expensive to verify them here.   -zinx */

	clc.downloadSize = (int)dltotal;
	Cvar_SetIntegerValue( "cl_downloadSize", clc.downloadSize );
	clc.downloadCount = (int)dlnow;
	Cvar_SetIntegerValue( "cl_downloadCount", clc.downloadCount );

	//Cvar_SetValue( "cl_downloadCount", (float)dlnow );
	return 0;
}


/*
===============
inspired from http://www.w3.org/Library/Examples/LoadToFile.c
setup the download, return once we have a connection
===============
*/
int DL_BeginDownload( const char *localName, const char *remoteName, int debug ) {
	if ( dl_request ) {
		Com_Printf( S_COLOR_YELLOW "ERROR: DL_BeginDownload called with a download request already active\n" );
		return 0;
	}

	if ( !localName || !remoteName ) {
		Com_DPrintf( "Empty download URL or empty local file name\n" );
		return 0;
	}

	DL_Cleanup();

	if ( !DL_Init() ) {
		Com_Printf( S_COLOR_YELLOW "Error initializing cURL library\n" );
		return 0;
	}

	if ( FS_CreatePath( localName ) ) {
		Com_Printf( S_COLOR_RED "ERROR: DL_BeginDownload unable to create path '%s'\n", localName );
		DL_Cleanup();
		return 0;
	}
	dl_file = Sys_FOpen( localName, "wb+" );
	if ( !dl_file ) {
		Com_Printf( S_COLOR_RED "ERROR: DL_BeginDownload unable to open '%s' for writing\n", localName );
		DL_Cleanup();
		return 0;
	}

	dl_request = dl_curl_easy_init();
	if ( !dl_request ) {
		Com_Printf( S_COLOR_RED "ERROR: DL_BeginDownload: easy_init() failed\n" );
		DL_Cleanup();
		return 0;
	}

	dl_multi = dl_curl_multi_init();
	if ( !dl_multi ) {
		Com_Printf( S_COLOR_RED "ERROR: DL_BeginDownload: multi_init() failed\n" );
		DL_Cleanup();
		return 0;
	}

	Com_Printf( "URL: %s\n", remoteName );
	if ( debug )
		dl_curl_easy_setopt( dl_request, CURLOPT_VERBOSE, 1 );
	dl_curl_easy_setopt( dl_request, CURLOPT_USERAGENT, va( "%s %s", APP_NAME "/" APP_VERSION, dl_curl_version() ) );
	dl_curl_easy_setopt( dl_request, CURLOPT_REFERER, va( "ET://%s", Cvar_VariableString( "cl_currentServerIP" ) ) );
	dl_curl_easy_setopt( dl_request, CURLOPT_URL, remoteName );
	dl_curl_easy_setopt( dl_request, CURLOPT_WRITEFUNCTION, DL_cb_FWriteFile );
	dl_curl_easy_setopt( dl_request, CURLOPT_WRITEDATA, (void*)dl_file );
	dl_curl_easy_setopt( dl_request, CURLOPT_NOPROGRESS, 0 );
#if CURL_AT_LEAST_VERSION(7, 32, 0)
	dl_curl_easy_setopt( dl_request, CURLOPT_XFERINFOFUNCTION, DL_cb_Progress );
	dl_curl_easy_setopt( dl_request, CURLOPT_XFERINFODATA, NULL );
#else
	dl_curl_easy_setopt( dl_request, CURLOPT_PROGRESSFUNCTION, DL_cb_Progress );
	dl_curl_easy_setopt( dl_request, CURLOPT_XFERINFODATA, NULL );
#endif
	dl_curl_easy_setopt( dl_request, CURLOPT_FAILONERROR, 1 );
	dl_curl_easy_setopt( dl_request, CURLOPT_FOLLOWLOCATION, 1 );
	dl_curl_easy_setopt( dl_request, CURLOPT_MAXREDIRS, 5 );
#if CURL_AT_LEAST_VERSION(7, 85, 0)
	dl_curl_easy_setopt( dl_request, CURLOPT_PROTOCOLS_STR, ALLOWED_PROTOCOLS_STR );
#else
	dl_curl_easy_setopt( dl_request, CURLOPT_PROTOCOLS, ALLOWED_PROTOCOLS );
#endif

#ifdef CURL_MAX_READ_SIZE
	dl_curl_easy_setopt( dl_request, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE );
#endif

	if ( dl_curl_multi_add_handle( dl_multi, dl_request ) != CURLM_OK ) {
		Com_Printf( S_COLOR_RED "ERROR: DL_BeginDownload: multi_add_handle() failed\n" );
		DL_Cleanup();
		return 0;
	}

	Cvar_Set( "cl_downloadName", remoteName );

	return 1;
}

// (maybe this should be CL_DL_DownloadLoop)
dlStatus_t DL_DownloadLoop( void ) {
	CURLMcode status;
	CURLMsg *msg;
	int dls = 0;
	const char *err = NULL;
	long code = 0L;

	if ( !dl_request ) {
		Com_DPrintf( "DL_DownloadLoop: unexpected call with dl_request == NULL\n" );
		return DL_DONE;
	}

	if ( ( status = dl_curl_multi_perform( dl_multi, &dls ) ) == CURLM_CALL_MULTI_PERFORM && dls ) {
		return DL_CONTINUE;
	}

	while ( ( msg = dl_curl_multi_info_read( dl_multi, &dls ) ) != NULL && msg->easy_handle != dl_request )
		;

	if ( !msg || msg->msg != CURLMSG_DONE ) {
		return DL_CONTINUE;
	}

	if ( msg->data.result != CURLE_OK ) {
		code = dl_curl_easy_getinfo( msg->easy_handle, CURLINFO_RESPONSE_CODE, &code );
//#ifdef __MACOS__ // ���
//		err = "unknown curl error.";
//#else
		err = dl_curl_easy_strerror( msg->data.result );
//#endif
	} else {
		err = NULL;
	}

	DL_Cleanup();

	Cvar_Set( "ui_dl_running", "0" );

	if ( err ) {
		Com_DPrintf( "DL_DownloadLoop: request terminated with failure status '%s' (%ld)\n", err, code );
		return DL_FAILED;
	}

	return DL_DONE;
}


#endif /* USE_CURL */
