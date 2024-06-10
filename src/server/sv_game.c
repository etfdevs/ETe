/*
===========================================================================

Wolfenstein: Enemy Territory GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Wolfenstein: Enemy Territory GPL Source Code (Wolf ET Source Code).  

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

// sv_game.c -- interface to the game dll

#include "server.h"

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int	SV_NumForGentity( sharedEntity_t *ent ) {
	int		num;

	num = ( (byte *)ent - (byte *)sv.gentities ) / sv.gentitySize;

	return num;
}


sharedEntity_t *SV_GentityNum( int num ) {
	sharedEntity_t *ent;

	ent = (sharedEntity_t *)((byte *)sv.gentities + sv.gentitySize*(num));

	return ent;
}


playerState_t *SV_GameClientNum( int num ) {
	playerState_t	*ps;

	ps = (playerState_t *)((byte *)sv.gameClients + sv.gameClientSize*(num));

	return ps;
}


svEntity_t	*SV_SvEntityForGentity( sharedEntity_t *gEnt ) {
	if ( !gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	return &sv.svEntities[ gEnt->s.number ];
}


sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt ) {
	int		num;

	num = svEnt - sv.svEntities;
	return SV_GentityNum( num );
}


/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
static void SV_GameSendServerCommand( int clientNum, const char *text ) {
	if ( clientNum == -1 ) {
		SV_SendServerCommand( NULL, "%s", text );
	} else {
		if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
			return;
		}
		SV_SendServerCommand( svs.clients + clientNum, "%s", text );	
	}
}


/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
static void SV_GameDropClient( int clientNum, const char *reason, int length ) {
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		return;
	}
	SV_DropClient( svs.clients + clientNum, reason );
	if ( length ) {
		SV_TempBanNetAddress( &svs.clients[ clientNum ].netchan.remoteAddress, length );
	}
}


/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
static void SV_SetBrushModel( sharedEntity_t *ent, const char *name ) {
	clipHandle_t	h;
	vec3_t			mins, maxs;

	if ( !name ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: NULL" );
	}

	if ( name[0] != '*' ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
	}

	ent->s.modelindex = atoi( name + 1 );

	h = CM_InlineModel( ent->s.modelindex );
	CM_ModelBounds( h, mins, maxs );
	VectorCopy (mins, ent->r.mins);
	VectorCopy (maxs, ent->r.maxs);
	ent->r.bmodel = qtrue;

	ent->r.contents = -1;		// we don't know exactly what is in the brushes

	SV_LinkEntity( ent );		// FIXME: remove
}


/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS( const vec3_t p1, const vec3_t p2 )
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;
	if (!CM_AreasConnected (area1, area2))
		return qfalse;		// a door blocks sight
	return qtrue;
}


/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
static qboolean SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2 )
{
	int		leafnum;
	int		cluster;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);

	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;

	return qtrue;
}


/*
========================
SV_AdjustAreaPortalState
========================
*/
static void SV_AdjustAreaPortalState( sharedEntity_t *ent, qboolean open ) {
	svEntity_t	*svEnt;

	svEnt = SV_SvEntityForGentity( ent );
	if ( svEnt->areanum2 == -1 ) {
		return;
	}
	CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, open );
}


/*
==================
SV_EntityContact
==================
*/
static qboolean SV_EntityContact( const vec3_t mins, const vec3_t maxs, const sharedEntity_t *gEnt, const int capsule ) {
	const float	*origin, *angles;
	clipHandle_t	ch;
	trace_t			trace;

	// check for exact collision
	origin = gEnt->r.currentOrigin;
	angles = gEnt->r.currentAngles;

	ch = SV_ClipHandleForEntity( gEnt );
	CM_TransformedBoxTrace( &trace, vec3_origin, vec3_origin, mins, maxs, ch, -1, origin, angles, capsule );

	return trace.startsolid;
}


/*
===============
SV_GetServerinfo
===============
*/
static void SV_GetServerinfo( char *buffer, int bufferSize ) {

	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize );
	}
	if ( sv.state != SS_GAME || !sv.configstrings[ CS_SERVERINFO ] ) {
		Q_strncpyz( buffer, Cvar_InfoString( CVAR_SERVERINFO | CVAR_SERVERINFO_NOUPDATE, NULL ), bufferSize );
	} else {
		Q_strncpyz( buffer, sv.configstrings[ CS_SERVERINFO ], bufferSize );
	}
}


/*
===============
SV_LocateGameData

===============
*/
static void SV_LocateGameData( sharedEntity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *clients, int sizeofGameClient ) {
	sv.gentities = gEnts;
	sv.gentitySize = sizeofGEntity_t;
	sv.num_entities = numGEntities;

	sv.gameClients = clients;
	sv.gameClientSize = sizeofGameClient;
}


/*
===============
SV_GetUsercmd
===============
*/
static void SV_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	if ( (unsigned) clientNum < sv_maxclients->integer ) {
		*cmd = svs.clients[ clientNum ].lastUsercmd;
	} else {
		Com_Error( ERR_DROP, "%s(): bad clientNum: %i", __func__, clientNum );
	}
}


static void SV_BotUsercmd( int clientNum, usercmd_t *cmd ) {
	if ( (unsigned) clientNum < sv_maxclients->integer ) {
		SV_ClientThink( &svs.clients[ clientNum ], cmd );
	} else {
		Com_Error( ERR_DROP, "%s(): bad clientNum: %i", __func__, clientNum );
	}
}


static void SV_BotClientCommand( int client, const char *command ) {
	if ( (unsigned) client < sv_maxclients->integer ) {
		SV_ExecuteClientCommand( &svs.clients[client], command, qfalse );
	} else {
		Com_Error( ERR_DROP, "%s(): bad clientNum: %i", __func__, client );
	}
}


/*
====================
SV_SendBinaryMessage
====================
*/
static void SV_SendBinaryMessage( int cno, char *buf, int buflen ) {
	if ( (unsigned) cno < sv_maxclients->integer ) {
		if ( buflen < 0 || buflen > MAX_BINARY_MESSAGE ) {
			svs.clients[cno].binaryMessageLength = 0;
			Com_Error( ERR_DROP, "%s(): bad length %i", __func__, buflen );
			return;
		}

		svs.clients[cno].binaryMessageLength = buflen;
		memcpy( svs.clients[cno].binaryMessage, buf, buflen );
	} else {
		Com_Error( ERR_DROP, "%s(): bad clientNum: %i", __func__, cno );
	}
}


/*
====================
SV_BinaryMessageStatus
====================
*/
static int SV_BinaryMessageStatus( int cno ) {
	if ( cno < 0 || cno >= sv_maxclients->integer ) {
		return qfalse;
	}

	if ( svs.clients[cno].binaryMessageLength == 0 ) {
		return MESSAGE_EMPTY;
	}

	if ( svs.clients[cno].binaryMessageOverflowed ) {
		return MESSAGE_WAITING_OVERFLOW;
	}

	return MESSAGE_WAITING;
}


/*
====================
SV_GameBinaryMessageReceived
====================
*/
void SV_GameBinaryMessageReceived( int cno, const char *buf, int buflen, int commandTime ) {
	if ( !gvm )
		return;
	VM_Call( gvm, GAME_MESSAGERECEIVED, cno, buf, buflen, commandTime );
}


qboolean SV_GameSnapshotCallback( int entityNum, int clientNum ) {
	if ( !gvm )
		return qtrue;
	return VM_Call( gvm, GAME_SNAPSHOT_CALLBACK, entityNum, clientNum );
}


//==============================================

/*static int FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}*/


/*
====================
VM_ArgPtr
====================
*/
static void *VM_ArgPtr( intptr_t intValue ) {

	if ( !intValue || gvm == NULL )
		return NULL;

	return (void *)(intValue);
}


/*
====================
GVM_ArgPtr

exported version
====================
*/
void *GVM_ArgPtr( intptr_t intValue ) 
{
	return VM_ArgPtr( intValue );
}


static qboolean SV_G_GetValue( char* value, int valueSize, const char* key )
{
	if ( !Q_stricmp( key, "trap_SV_AddCommand") ) {
		Com_sprintf( value, valueSize, "%i", G_ADDCOMMAND );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_SV_RemoveCommand") ) {
		Com_sprintf( value, valueSize, "%i", G_REMOVECOMMAND );
		return qtrue;
	}

	// UTF-8 not yet supported
	if ( !Q_stricmp( key, "cap_UTF8" ) ) {
		Com_sprintf( value, valueSize, "%i", 0 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "engine_is_ete" ) ) {
		Com_sprintf( value, valueSize, "%i", 1 );
		return qtrue;
	}

	return qfalse;
}


/*
====================
SV_GameSystemCalls

The module is making a system call
====================
*/
static intptr_t SV_GameSystemCalls( intptr_t *args ) {
	switch ( args[0] ) {
	case G_PRINT:
		Com_Printf( "%s", (const char *)VMA( 1 ) );
		return 0;
	case G_ERROR:
		Com_Error( ERR_DROP, "%s", (const char *)VMA( 1 ) );
		return 0;
	case G_MILLISECONDS:
		return Sys_Milliseconds();
	case G_CVAR_REGISTER:
		Cvar_Register( VMA(1), VMA(2), VMA(3), args[4], gvm->privateFlag ); 
		return 0;
	case G_CVAR_UPDATE:
		Cvar_Update( VMA(1), gvm->privateFlag );
		return 0;
	case G_CVAR_SET:
		Cvar_SetSafe( (const char *)VMA( 1 ), (const char *)VMA( 2 ) );
		return 0;
	case G_CVAR_VARIABLE_INTEGER_VALUE:
		return Cvar_VariableIntegerValue( (const char *)VMA( 1 ) );
	case G_CVAR_VARIABLE_STRING_BUFFER:
		Cvar_VariableStringBufferSafe( VMA(1), VMA(2), args[3], gvm->privateFlag );
		return 0;
	case G_CVAR_LATCHEDVARIABLESTRINGBUFFER:
		Cvar_LatchedVariableStringBufferSafe( VMA( 1 ), VMA( 2 ), args[3], gvm->privateFlag );
		return 0;
	case G_ARGC:
		return Cmd_Argc();
	case G_ARGV:
		Cmd_ArgvBuffer( args[1], VMA( 2 ), args[3] );
		return 0;
	case G_SEND_CONSOLE_COMMAND:
		Cbuf_ExecuteText( args[1], VMA( 2 ) );
		return 0;

	case G_FS_FOPEN_FILE:
		return FS_VM_OpenFile( VMA( 1 ), VMA( 2 ), args[3], H_QAGAME );
	case G_FS_READ:
		return FS_VM_ReadFile( VMA( 1 ), args[2], args[3], H_QAGAME );
	case G_FS_WRITE:
		return FS_VM_WriteFile( VMA(1), args[2], args[3], H_QAGAME );
	case G_FS_RENAME:
		FS_Rename( VMA( 1 ), VMA( 2 ) );
		return 0;
	case G_FS_FCLOSE_FILE:
		FS_VM_CloseFile( args[1], H_QAGAME );
		return 0;
	case G_FS_GETFILELIST:
		return FS_GetFileList( VMA( 1 ), VMA( 2 ), VMA( 3 ), args[4] );

	case G_LOCATE_GAME_DATA:
		SV_LocateGameData( VMA( 1 ), args[2], args[3], VMA( 4 ), args[5] );
		return 0;
	case G_DROP_CLIENT:
		SV_GameDropClient( args[1], VMA( 2 ), args[3] );
		return 0;
	case G_SEND_SERVER_COMMAND:
		SV_GameSendServerCommand( args[1], VMA( 2 ) );
		return 0;
	case G_LINKENTITY:
		SV_LinkEntity( VMA( 1 ) );
		return 0;
	case G_UNLINKENTITY:
		SV_UnlinkEntity( VMA( 1 ) );
		return 0;
	case G_ENTITIES_IN_BOX:
		return SV_AreaEntities( VMA( 1 ), VMA( 2 ), VMA( 3 ), args[4] );
	case G_ENTITY_CONTACT:
		return SV_EntityContact( VMA( 1 ), VMA( 2 ), VMA( 3 ), /* int capsule */ qfalse );
	case G_ENTITY_CONTACTCAPSULE:
		return SV_EntityContact( VMA( 1 ), VMA( 2 ), VMA( 3 ), /* int capsule */ qtrue );
	case G_TRACE:
		SV_Trace( VMA( 1 ), VMA( 2 ), VMA( 3 ), VMA( 4 ), VMA( 5 ), args[6], args[7], /* int capsule */ qfalse );
		return 0;
	case G_TRACECAPSULE:
		SV_Trace( VMA( 1 ), VMA( 2 ), VMA( 3 ), VMA( 4 ), VMA( 5 ), args[6], args[7], /* int capsule */ qtrue );
		return 0;
	case G_POINT_CONTENTS:
		return SV_PointContents( VMA( 1 ), args[2] );
	case G_SET_BRUSH_MODEL:
		SV_SetBrushModel( VMA( 1 ), VMA( 2 ) );
		return 0;
	case G_IN_PVS:
		return SV_inPVS( VMA( 1 ), VMA( 2 ) );
	case G_IN_PVS_IGNORE_PORTALS:
		return SV_inPVSIgnorePortals( VMA( 1 ), VMA( 2 ) );

	case G_SET_CONFIGSTRING:
		SV_SetConfigstring( args[1], VMA( 2 ) );
		return 0;
	case G_GET_CONFIGSTRING:
		SV_GetConfigstring( args[1], VMA( 2 ), args[3] );
		return 0;
	case G_SET_USERINFO:
		SV_SetUserinfo( args[1], VMA( 2 ) );
		return 0;
	case G_GET_USERINFO:
		SV_GetUserinfo( args[1], VMA( 2 ), args[3] );
		return 0;
	case G_GET_SERVERINFO:
		SV_GetServerinfo( VMA( 1 ), args[2] );
		return 0;
	case G_ADJUST_AREA_PORTAL_STATE:
		SV_AdjustAreaPortalState( VMA( 1 ), args[2] );
		return 0;
	case G_AREAS_CONNECTED:
		return CM_AreasConnected( args[1], args[2] );

	case G_BOT_ALLOCATE_CLIENT:
		return SV_BotAllocateClient( args[1] );
	case G_BOT_FREE_CLIENT:
		SV_BotFreeClient( args[1] );
		return 0;

	case G_GET_USERCMD:
		SV_GetUsercmd( args[1], VMA( 2 ) );
		return 0;
	case G_GET_ENTITY_TOKEN:
	{
		const char *s = COM_Parse( &sv.entityParsePoint );
		//Q_strncpyz( VMA(1), s, args[2] );
		// we can't use our optimized Q_strncpyz() function
		// because of uninitialized memory bug in defrag mod
		{
			char *dst = (char*)VMA(1);
			const int size = args[2]-1;
			if ( size >= 0 ) {
				Q_strncpy( dst, s, size );
				dst[size] = '\0';
			}
		}
		if ( !sv.entityParsePoint && s[0] == '\0' ) {
			return qfalse;
		} else {
			return qtrue;
		}
	}

	case G_DEBUG_POLYGON_CREATE:
		return 0;
	case G_DEBUG_POLYGON_DELETE:
		return 0;
	case G_REAL_TIME:
		return Com_RealTime( VMA( 1 ) );
	case G_SNAPVECTOR:
		Sys_SnapVector( VMA( 1 ) );
		return 0;
	case G_GETTAG:
		return SV_GetTag( args[1], args[2], VMA( 3 ), VMA( 4 ) );

	case G_REGISTERTAG:
		return SV_LoadTag( VMA( 1 ) );

	case G_REGISTERSOUND:
		return 0;
	case G_GET_SOUND_LENGTH:
		Com_Error( ERR_DROP, "S_GetSoundLength shouldn't be used in server mods" );
		return 0;

		//====================================

	case BOTLIB_SETUP:
		return 0;
	case BOTLIB_SHUTDOWN:
		return -1;
	case BOTLIB_LIBVAR_SET:
		return 0;
	case BOTLIB_LIBVAR_GET:
		return 0;

	case BOTLIB_PC_ADD_GLOBAL_DEFINE:
		return PC_AddGlobalDefine( VMA( 1 ) );
	case BOTLIB_PC_LOAD_SOURCE:
		return PC_LoadSourceHandle( VMA( 1 ) );
	case BOTLIB_PC_FREE_SOURCE:
		return PC_FreeSourceHandle( args[1] );
	case BOTLIB_PC_READ_TOKEN:
		return PC_ReadTokenHandle( args[1], VMA( 2 ) );
	case BOTLIB_PC_SOURCE_FILE_AND_LINE:
		return PC_SourceFileAndLine( args[1], VMA( 2 ), VMA( 3 ) );
	case BOTLIB_PC_UNREAD_TOKEN:
		PC_UnreadLastTokenHandle( args[1] );
		return 0;

	case BOTLIB_GET_SNAPSHOT_ENTITY:
		return SV_BotGetSnapshotEntity( args[1], args[2] );
	case BOTLIB_GET_CONSOLE_MESSAGE:
		return SV_BotGetConsoleMessage( args[1], VMA( 2 ), args[3] );
	case BOTLIB_USER_COMMAND:
		SV_BotUsercmd( args[1], VMA( 2 ) );
		return 0;

	case BOTLIB_EA_COMMAND:
		SV_BotClientCommand( args[1], VMA( 2 ) );
		return 0;

	/*case G_MEMSET:
		Com_Memset( VMA(1), args[2], args[3] );
		return args[1];

	case G_MEMCPY:
		Com_Memcpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case G_STRNCPY:
		strncpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case G_SIN:
		return FloatAsInt( sin( VMF( 1 ) ) );

	case G_COS:
		return FloatAsInt( cos( VMF( 1 ) ) );

	case G_ATAN2:
		return FloatAsInt( atan2( VMF( 1 ), VMF( 2 ) ) );

	case G_SQRT:
		return FloatAsInt( sqrt( VMF( 1 ) ) );

	case G_MATRIXMULTIPLY:
		MatrixMultiply( VMA( 1 ), VMA( 2 ), VMA( 3 ) );
		return 0;

	case G_ANGLEVECTORS:
		AngleVectors( VMA( 1 ), VMA( 2 ), VMA( 3 ), VMA( 4 ) );
		return 0;

	case G_PERPENDICULARVECTOR:
		PerpendicularVector( VMA( 1 ), VMA( 2 ) );
		return 0;

	case G_FLOOR:
		return FloatAsInt( floor( VMF( 1 ) ) );

	case G_CEIL:
		return FloatAsInt( ceil( VMF( 1 ) ) );*/

	case PB_STAT_REPORT:
		return 0 ;

	case G_SENDMESSAGE:
		SV_SendBinaryMessage( args[1], (char *)VMA( 2 ), args[3] );
		return 0;
	case G_MESSAGESTATUS:
		return SV_BinaryMessageStatus( args[1] );

	// engine extensions
	case G_ADDCOMMAND:
		Cmd_AddCommand( VMA(1), NULL );
		Cmd_SetModule( VMA(1), MODULE_SGAME );
		return 0;
	case G_REMOVECOMMAND:
		Cmd_RemoveCommandSafe( VMA(1) );
		return 0;

	case G_TRAP_GETVALUE:
		return SV_G_GetValue( VMA(1), args[2], VMA(3) );

	default:
		Com_Error( ERR_DROP, "Bad game system trap: %ld", (long int) args[0] );
	}
	return 0;
}


/*
====================
SV_DllSyscall
====================
*/
static intptr_t QDECL SV_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
	intptr_t	args[14]; // max.count for qagame
	va_list	ap;
	int i;

	args[0] = arg;
	va_start( ap, arg );
	for (i = 1; i < ARRAY_LEN( args ); i++ )
		args[ i ] = va_arg( ap, intptr_t );
	va_end( ap );

	return SV_GameSystemCalls( args );
#else
	return SV_GameSystemCalls( &arg );
#endif
}


void SV_CompleteMapName( char *args, int argNum );
static const cmdListItem_t etf_cmds[] = {
	{ "etfdevmap", NULL, SV_CompleteMapName },
	{ "etfmap", NULL, SV_CompleteMapName }
};


/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs( void ) {
	// Even if you use game_restart on server, this will still be true.
	// currentGameMod updates after shutdown
	if ( currentGameMod == GAMEMOD_ETF ) {
		Cmd_UnregisterArray( etf_cmds );
	}
	if ( !gvm ) {
		return;
	}

	//Sys_OmnibotUnLoad();

	VM_Call( gvm, GAME_SHUTDOWN, qfalse );
	VM_Free( gvm );
	gvm = NULL;
	Cmd_UnregisterModule( MODULE_SGAME );
	FS_VM_CloseFiles( H_QAGAME );
}


/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void SV_InitGameVM( qboolean restart ) {
	int		i;

	// start the entity parsing at the beginning
	sv.entityParsePoint = CM_EntityString();

	// clear all gentity pointers that might still be set from
	// a previous level
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=522
	// now done before GAME_INIT call
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		svs.clients[i].gentity = NULL;
	}

	if ( currentGameMod == GAMEMOD_ETF ) {
		Cmd_RegisterArray( etf_cmds, MODULE_SERVER );
	}

	// use the current msec count for a random seed
	// init for this gamestate
	if ( currentGameMod == GAMEMOD_LEGACY )
		VM_Call( gvm, GAME_INIT, sv.time, Com_Milliseconds(), restart, qtrue, com_legacyVersion->integer );
	else
		VM_Call( gvm, GAME_INIT, sv.time, Com_Milliseconds(), restart );
}


/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs( void ) {
	if ( !gvm ) {
		return;
	}

	// unload the refs during a restart
	//Sys_OmnibotUnLoad();

	VM_Call( gvm, GAME_SHUTDOWN, qtrue );

	// do a restart instead of a free
	gvm = VM_Restart( gvm );
	if ( !gvm ) {
		Com_Error( ERR_DROP, "VM_Restart on game failed" );
	}

	SV_InitGameVM( qtrue );

	// load userinfo filters
	SV_LoadFilters( sv_filter->string );
}


/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void SV_InitGameProgs( void ) {
	sv.num_tagheaders = 0;
	sv.num_tags = 0;

	// load the dll
	gvm = VM_Create( VM_GAME, SV_GameSystemCalls, SV_DllSyscall, VMI_NATIVE );
	if ( !gvm ) {
		Com_Error( ERR_DROP, "VM_Create on game failed\n\nMake sure " S_COLOR_GREEN "%s" S_COLOR_NULL " exists in the mod's folder (%s)\nIf it does not, the mod may be incompatible with your system or a file extraction error has occured!", SYS_DLLNAME_QAGAME, FS_GetCurrentGameDir() );
	}

	SV_InitGameVM( qfalse );

	// unload the refs during a map change
	//Sys_OmnibotLoad();

	// load userinfo filters
	SV_LoadFilters( sv_filter->string );
}


/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand( void ) {
	if ( sv.state != SS_GAME ) {
		return qfalse;
	}

	return VM_Call( gvm, GAME_CONSOLE_COMMAND );
}

/*
====================
SV_GameIsSinglePlayer
====================
*/
qboolean SV_GameIsSinglePlayer( void ) {
	return( com_gameInfo.spGameTypes & ( 1 << sv_gameType->integer ) );
}

/*
====================
SV_GameIsCoop

	This is a modified SinglePlayer, no savegame capability for example
====================
*/
qboolean SV_GameIsCoop( void ) {
	return( com_gameInfo.coopGameTypes & ( 1 << sv_gameType->integer ) );
}

/*
====================
SV_GetTag

  return qfalse if unable to retrieve tag information for this client
====================
*/
#ifndef DEDICATED
qboolean CL_GetTag( int clientNum, char *tagname, orientation_t *or );
#endif

qboolean SV_GetTag( int clientNum, int tagFileNumber, char *tagname, orientation_t *or ) {
	int i;

	if ( tagFileNumber > 0 && tagFileNumber <= sv.num_tagheaders ) {
		for ( i = sv.tagHeadersExt[tagFileNumber - 1].start; i < sv.tagHeadersExt[tagFileNumber - 1].start + sv.tagHeadersExt[tagFileNumber - 1].count; i++ ) {
			if ( !Q_stricmp( sv.tags[i].name, tagname ) ) {
				VectorCopy( sv.tags[i].origin, or->origin );
				VectorCopy( sv.tags[i].axis[0], or->axis[0] );
				VectorCopy( sv.tags[i].axis[1], or->axis[1] );
				VectorCopy( sv.tags[i].axis[2], or->axis[2] );
				return qtrue;
			}
		}
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS )
		return qfalse;

	// Gordon: lets try and remove the inconsitancy between ded/non-ded servers...
	// Gordon: bleh, some code in clientthink_real really relies on this working on player models...
#ifndef DEDICATED // TTimo: dedicated only binary defines DEDICATED
	if ( com_dedicated->integer ) {
		return qfalse;
	}

	return CL_GetTag( clientNum, tagname, or );
#else
	return qfalse;
#endif
}
