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

#include "ui_local.h"

// this file is only included when building a dll
// syscalls.asm is included instead when building a qvm

static dllSyscall_t syscall = (dllSyscall_t)-1;

Q_EXPORT void dllEntry( dllSyscall_t syscallptr ) {
	syscall = syscallptr;
}

int PASSFLOAT( float x ) {
	floatint_t fi;
	fi.f = x;
	return fi.i;
}

void trap_Print( const char *string ) {
	SystemCall( UI_PRINT, string );
}

void NORETURN trap_Error( const char *string ) {
	SystemCall( UI_ERROR, string );
	exit( 1 );
}

int trap_Milliseconds( void ) {
	return SystemCall( UI_MILLISECONDS );
}

void trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags ) {
	SystemCall( UI_CVAR_REGISTER, cvar, var_name, value, flags );
}

void trap_Cvar_Update( vmCvar_t *cvar ) {
	SystemCall( UI_CVAR_UPDATE, cvar );
}

void trap_Cvar_Set( const char *var_name, const char *value ) {
	SystemCall( UI_CVAR_SET, var_name, value );
}

float trap_Cvar_VariableValue( const char *var_name ) {
	floatint_t temp;
	temp.i = SystemCall( UI_CVAR_VARIABLEVALUE, var_name );
	return temp.f;
}

void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	SystemCall( UI_CVAR_VARIABLESTRINGBUFFER, var_name, buffer, bufsize );
}

void trap_Cvar_LatchedVariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	SystemCall( UI_CVAR_LATCHEDVARIABLESTRINGBUFFER, var_name, buffer, bufsize );
}

void trap_Cvar_SetValue( const char *var_name, float value ) {
	SystemCall( UI_CVAR_SETVALUE, var_name, PASSFLOAT( value ) );
}

void trap_Cvar_Reset( const char *name ) {
	SystemCall( UI_CVAR_RESET, name );
}

void trap_Cvar_Create( const char *var_name, const char *var_value, int flags ) {
	SystemCall( UI_CVAR_CREATE, var_name, var_value, flags );
}

void trap_Cvar_InfoStringBuffer( int bit, char *buffer, int bufsize ) {
	SystemCall( UI_CVAR_INFOSTRINGBUFFER, bit, buffer, bufsize );
}

int trap_Argc( void ) {
	return SystemCall( UI_ARGC );
}

void trap_Argv( int n, char *buffer, int bufferLength ) {
	SystemCall( UI_ARGV, n, buffer, bufferLength );
}

void trap_Cmd_ExecuteText( int exec_when, const char *text ) {
	SystemCall( UI_CMD_EXECUTETEXT, exec_when, text );
}

void trap_AddCommand( const char *cmdName ) {
	SystemCall( UI_ADDCOMMAND, cmdName );
}

int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	return SystemCall( UI_FS_FOPENFILE, qpath, f, mode );
}

void trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	SystemCall( UI_FS_READ, buffer, len, f );
}

void trap_FS_Write( const void *buffer, int len, fileHandle_t f ) {
	SystemCall( UI_FS_WRITE, buffer, len, f );
}

void trap_FS_FCloseFile( fileHandle_t f ) {
	SystemCall( UI_FS_FCLOSEFILE, f );
}

int trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize ) {
	return SystemCall( UI_FS_GETFILELIST, path, extension, listbuf, bufsize );
}

int trap_FS_Delete( const char *filename ) {
	return SystemCall( UI_FS_DELETEFILE, filename );
}

qhandle_t trap_R_RegisterModel( const char *name ) {
	return SystemCall( UI_R_REGISTERMODEL, name );
}

qhandle_t trap_R_RegisterSkin( const char *name ) {
	return SystemCall( UI_R_REGISTERSKIN, name );
}

void trap_R_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font ) {
	SystemCall( UI_R_REGISTERFONT, fontName, pointSize, font );
}

qhandle_t trap_R_RegisterShaderNoMip( const char *name ) {
	return SystemCall( UI_R_REGISTERSHADERNOMIP, name );
}

void trap_R_ClearScene( void ) {
	SystemCall( UI_R_CLEARSCENE );
}

void trap_R_AddRefEntityToScene( const refEntity_t *re ) {
	SystemCall( UI_R_ADDREFENTITYTOSCENE, re );
}

void trap_R_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts ) {
	SystemCall( UI_R_ADDPOLYTOSCENE, hShader, numVerts, verts );
}

// ydnar: new dlight system
//%	void	trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, int overdraw ) {
//%		SystemCall( UI_R_ADDLIGHTTOSCENE, org, PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b), overdraw );
//%	}
void    trap_R_AddLightToScene( const vec3_t org, float radius, float intensity, float r, float g, float b, qhandle_t hShader, int flags ) {
	SystemCall( UI_R_ADDLIGHTTOSCENE, org, PASSFLOAT( radius ), PASSFLOAT( intensity ),
			 PASSFLOAT( r ), PASSFLOAT( g ), PASSFLOAT( b ), hShader, flags );
}

void trap_R_AddCoronaToScene( const vec3_t org, float r, float g, float b, float scale, int id, qboolean visible ) {
	SystemCall( UI_R_ADDCORONATOSCENE, org, PASSFLOAT( r ), PASSFLOAT( g ), PASSFLOAT( b ), PASSFLOAT( scale ), id, visible  );
}

void trap_R_RenderScene( const refdef_t *fd ) {
	SystemCall( UI_R_RENDERSCENE, fd );
}

void trap_R_SetColor( const float *rgba ) {
	SystemCall( UI_R_SETCOLOR, rgba );
}

void trap_R_Add2dPolys( polyVert_t* verts, int numverts, qhandle_t hShader ) {
	SystemCall( UI_R_DRAW2DPOLYS, verts, numverts, hShader );
}

void trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	SystemCall( UI_R_DRAWSTRETCHPIC, PASSFLOAT( x ), PASSFLOAT( y ), PASSFLOAT( w ), PASSFLOAT( h ), PASSFLOAT( s1 ), PASSFLOAT( t1 ), PASSFLOAT( s2 ), PASSFLOAT( t2 ), hShader );
}

void trap_R_DrawRotatedPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, float angle ) {
	SystemCall( UI_R_DRAWROTATEDPIC, PASSFLOAT( x ), PASSFLOAT( y ), PASSFLOAT( w ), PASSFLOAT( h ), PASSFLOAT( s1 ), PASSFLOAT( t1 ), PASSFLOAT( s2 ), PASSFLOAT( t2 ), hShader, PASSFLOAT( angle ) );
}

void trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) {
	SystemCall( UI_R_MODELBOUNDS, model, mins, maxs );
}

void trap_UpdateScreen( void ) {
	SystemCall( UI_UPDATESCREEN );
}

int trap_CM_LerpTag( orientation_t *tag, const refEntity_t *refent, const char *tagName, int startIndex ) {
	return SystemCall( UI_CM_LERPTAG, tag, refent, tagName, 0 );           // NEFVE - SMF - fixed
}

void trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum ) {
	SystemCall( UI_S_STARTLOCALSOUND, sfx, channelNum, 127 /* Gordon: default volume always for the moment*/ );
}

sfxHandle_t trap_S_RegisterSound( const char *sample, qboolean compressed ) {
	int i = SystemCall( UI_S_REGISTERSOUND, sample, qfalse /* compressed */ );
#ifdef _DEBUG
	if ( i == 0 ) {
		Com_Printf( "^1Warning: Failed to load sound: %s\n", sample );
	}
#endif
	return i;
}

void    trap_S_FadeBackgroundTrack( float targetvol, int time, int num ) {   // yes, i know.  fadebackground coming in, fadestreaming going out.  will have to see where functionality leads...
	SystemCall( UI_S_FADESTREAMINGSOUND, PASSFLOAT( targetvol ), time, num ); // 'num' is '0' if it's music, '1' if it's "all streaming sounds"
}

void    trap_S_FadeAllSound( float targetvol, int time, qboolean stopsound ) {
	SystemCall( UI_S_FADEALLSOUNDS, PASSFLOAT( targetvol ), time, stopsound );
}

void trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen ) {
	SystemCall( UI_KEY_KEYNUMTOSTRINGBUF, keynum, buf, buflen );
}

void trap_Key_GetBindingBuf( int keynum, char *buf, int buflen ) {
	SystemCall( UI_KEY_GETBINDINGBUF, keynum, buf, buflen );
}

// binding MUST be lower case
void trap_Key_KeysForBinding( const char* binding, int* key1, int* key2 ) {
	SystemCall( UI_KEY_BINDINGTOKEYS, binding, key1, key2 );
}

void trap_Key_SetBinding( int keynum, const char *binding ) {
	SystemCall( UI_KEY_SETBINDING, keynum, binding );
}

qboolean trap_Key_IsDown( int keynum ) {
	return SystemCall( UI_KEY_ISDOWN, keynum );
}

qboolean trap_Key_GetOverstrikeMode( void ) {
	return SystemCall( UI_KEY_GETOVERSTRIKEMODE );
}

void trap_Key_SetOverstrikeMode( qboolean state ) {
	SystemCall( UI_KEY_SETOVERSTRIKEMODE, state );
}

void trap_Key_ClearStates( void ) {
	SystemCall( UI_KEY_CLEARSTATES );
}

int trap_Key_GetCatcher( void ) {
	return SystemCall( UI_KEY_GETCATCHER );
}

void trap_Key_SetCatcher( int catcher ) {
	SystemCall( UI_KEY_SETCATCHER, catcher );
}

void trap_GetClipboardData( char *buf, int bufsize ) {
	SystemCall( UI_GETCLIPBOARDDATA, buf, bufsize );
}

void trap_GetClientState( uiClientState_t *state ) {
	SystemCall( UI_GETCLIENTSTATE, state );
}

void trap_GetGlconfig( glconfig_t *glconfig ) {
	SystemCall( UI_GETGLCONFIG, glconfig );
}

int trap_GetConfigString( int index, char* buff, int buffsize ) {
	return SystemCall( UI_GETCONFIGSTRING, index, buff, buffsize );
}

int trap_LAN_GetLocalServerCount( void ) {
	return SystemCall( UI_LAN_GETLOCALSERVERCOUNT );
}

void trap_LAN_GetLocalServerAddressString( int n, char *buf, int buflen ) {
	SystemCall( UI_LAN_GETLOCALSERVERADDRESSSTRING, n, buf, buflen );
}

int trap_LAN_GetGlobalServerCount( void ) {
	return SystemCall( UI_LAN_GETGLOBALSERVERCOUNT );
}

void trap_LAN_GetGlobalServerAddressString( int n, char *buf, int buflen ) {
	SystemCall( UI_LAN_GETGLOBALSERVERADDRESSSTRING, n, buf, buflen );
}

int trap_LAN_GetPingQueueCount( void ) {
	return SystemCall( UI_LAN_GETPINGQUEUECOUNT );
}

void trap_LAN_ClearPing( int n ) {
	SystemCall( UI_LAN_CLEARPING, n );
}

void trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime ) {
	SystemCall( UI_LAN_GETPING, n, buf, buflen, pingtime );
}

void trap_LAN_GetPingInfo( int n, char *buf, int buflen ) {
	SystemCall( UI_LAN_GETPINGINFO, n, buf, buflen );
}

// NERVE - SMF
qboolean trap_LAN_UpdateVisiblePings( int source ) {
	return SystemCall( UI_LAN_UPDATEVISIBLEPINGS, source );
}

int trap_LAN_GetServerCount( int source ) {
	return SystemCall( UI_LAN_GETSERVERCOUNT, source );
}

int trap_LAN_CompareServers( int source, int sortKey, int sortDir, int s1, int s2 ) {
	return SystemCall( UI_LAN_COMPARESERVERS, source, sortKey, sortDir, s1, s2 );
}

void trap_LAN_GetServerAddressString( int source, int n, char *buf, int buflen ) {
	SystemCall( UI_LAN_GETSERVERADDRESSSTRING, source, n, buf, buflen );
}

void trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen ) {
	SystemCall( UI_LAN_GETSERVERINFO, source, n, buf, buflen );
}

int trap_LAN_AddServer( int source, const char *name, const char *addr ) {
	return SystemCall( UI_LAN_ADDSERVER, source, name, addr );
}

void trap_LAN_RemoveServer( int source, const char *addr ) {
	SystemCall( UI_LAN_REMOVESERVER, source, addr );
}

int trap_LAN_GetServerPing( int source, int n ) {
	return SystemCall( UI_LAN_GETSERVERPING, source, n );
}

int trap_LAN_ServerIsVisible( int source, int n ) {
	return SystemCall( UI_LAN_SERVERISVISIBLE, source, n );
}

int trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen ) {
	return SystemCall( UI_LAN_SERVERSTATUS, serverAddress, serverStatus, maxLen );
}

qboolean trap_LAN_ServerIsInFavoriteList( int source, int n  ) {
	return SystemCall( UI_LAN_SERVERISINFAVORITELIST, source, n );
}

void trap_LAN_SaveCachedServers() {
	SystemCall( UI_LAN_SAVECACHEDSERVERS );
}

void trap_LAN_LoadCachedServers() {
	SystemCall( UI_LAN_LOADCACHEDSERVERS );
}

void trap_LAN_MarkServerVisible( int source, int n, qboolean visible ) {
	SystemCall( UI_LAN_MARKSERVERVISIBLE, source, n, visible );
}

// DHM - Nerve :: PunkBuster
void trap_SetPbClStatus( int status ) {
	SystemCall( UI_SET_PBCLSTATUS, status );
}
// DHM - Nerve

// TTimo: also for Sv
void trap_SetPbSvStatus( int status ) {
	SystemCall( UI_SET_PBSVSTATUS, status );
}

void trap_LAN_ResetPings( int n ) {
	SystemCall( UI_LAN_RESETPINGS, n );
}
// -NERVE - SMF

int trap_MemoryRemaining( void ) {
	return SystemCall( UI_MEMORY_REMAINING );
}

void trap_GetCDKey( char *buf, int buflen ) {
	SystemCall( UI_GET_CDKEY, buf, buflen );
}

void trap_SetCDKey( char *buf ) {
	SystemCall( UI_SET_CDKEY, buf );
}

int trap_PC_AddGlobalDefine( const char *define ) {
	return SystemCall( UI_PC_ADD_GLOBAL_DEFINE, define );
}

void trap_PC_RemoveAllGlobalDefines( void ) {
	SystemCall( UI_PC_REMOVE_ALL_GLOBAL_DEFINES );
}

int trap_PC_LoadSource( const char *filename ) {
	return SystemCall( UI_PC_LOAD_SOURCE, filename );
}

int trap_PC_FreeSource( int handle ) {
	return SystemCall( UI_PC_FREE_SOURCE, handle );
}

int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) {
	return SystemCall( UI_PC_READ_TOKEN, handle, pc_token );
}

int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	return SystemCall( UI_PC_SOURCE_FILE_AND_LINE, handle, filename, line );
}

int trap_PC_UnReadToken( int handle ) {
	return SystemCall( UI_PC_UNREAD_TOKEN, handle );
}

void trap_S_StopBackgroundTrack( void ) {
	SystemCall( UI_S_STOPBACKGROUNDTRACK );
}

void trap_S_StartBackgroundTrack( const char *intro, const char *loop, int fadeupTime ) {
	SystemCall( UI_S_STARTBACKGROUNDTRACK, intro, loop, fadeupTime );
}

int trap_RealTime( qtime_t *qtime ) {
	return SystemCall( UI_REAL_TIME, qtime );
}

// this returns a handle.  arg0 is the name in the format "idlogo.roq", set arg1 to NULL, alteredstates to qfalse (do not alter gamestate)
int trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits ) {
	return SystemCall( UI_CIN_PLAYCINEMATIC, arg0, xpos, ypos, width, height, bits );
}

// stops playing the cinematic and ends it.  should always return FMV_EOF
// cinematics must be stopped in reverse order of when they are started
e_status trap_CIN_StopCinematic( int handle ) {
	return SystemCall( UI_CIN_STOPCINEMATIC, handle );
}


// will run a frame of the cinematic but will not draw it.  Will return FMV_EOF if the end of the cinematic has been reached.
e_status trap_CIN_RunCinematic( int handle ) {
	return SystemCall( UI_CIN_RUNCINEMATIC, handle );
}


// draws the current frame
void trap_CIN_DrawCinematic( int handle ) {
	SystemCall( UI_CIN_DRAWCINEMATIC, handle );
}


// allows you to resize the animation dynamically
void trap_CIN_SetExtents( int handle, int x, int y, int w, int h ) {
	SystemCall( UI_CIN_SETEXTENTS, handle, x, y, w, h );
}


void    trap_R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset ) {
	SystemCall( UI_R_REMAP_SHADER, oldShader, newShader, timeOffset );
}

qboolean trap_VerifyCDKey( const char *key, const char *chksum ) {
	return SystemCall( UI_VERIFY_CDKEY, key, chksum );
}

// NERVE - SMF
qboolean trap_GetLimboString( int index, char *buf ) {
	return SystemCall( UI_CL_GETLIMBOSTRING, index, buf );
}

#define MAX_VA_STRING       32000

char* trap_TranslateString( const char *string ) {
	static char staticbuf[2][MAX_VA_STRING];
	static int bufcount = 0;
	char *buf;

	buf = staticbuf[bufcount++ % 2];

#ifdef LOCALIZATION_SUPPORT
	SystemCall( UI_CL_TRANSLATE_STRING, string, buf );
#else
	Q_strncpyz( buf, string, MAX_VA_STRING );
#endif // LOCALIZATION_SUPPORT
	return buf;
}
// -NERVE - SMF

// DHM - Nerve
void trap_CheckAutoUpdate( void ) {
	SystemCall( UI_CHECKAUTOUPDATE );
}

void trap_GetAutoUpdate( void ) {
	SystemCall( UI_GET_AUTOUPDATE );
}
// DHM - Nerve

void trap_openURL( const char *s ) {
	SystemCall( UI_OPENURL, s );
}

void trap_GetHunkData( int* hunkused, int* hunkexpected ) {
	SystemCall( UI_GETHUNKDATA, hunkused, hunkexpected );
}

// extension interface

qboolean trap_GetValue( char *value, int valueSize, const char *key ) {
	return SystemCall( dll_com_trapGetValue, value, valueSize, key );
}

void trap_R_AddRefEntityToScene2( const refEntity_t *re ) {
	SystemCall( dll_trap_R_AddRefEntityToScene2, re );
}

void trap_R_AddLinearLightToScene( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b ) {
	SystemCall( dll_trap_R_AddLinearLightToScene, start, end, intensity, r, g, b );
}

void trap_RemoveCommand( const char *cmdName ) {
	SystemCall( dll_trap_RemoveCommand, cmdName );
}
