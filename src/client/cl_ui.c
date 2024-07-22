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


#include "client.h"

vm_t *uivm = NULL;

/*
====================
GetClientState
====================
*/
static void GetClientState( uiClientState_t *state ) {
	state->connectPacketCount = clc.connectPacketCount;
	state->connState = cls.state;
	Q_strncpyz( state->servername, cls.servername, sizeof( state->servername ) );
	Q_strncpyz( state->updateInfoString, cls.updateInfoString, sizeof( state->updateInfoString ) );
	Q_strncpyz( state->messageString, clc.serverMessage, sizeof( state->messageString ) );
	state->clientNum = cl.snap.ps.clientNum;
}


/*
====================
LAN_LoadCachedServers
====================
*/
static void LAN_LoadCachedServers( void ) {
	fileHandle_t fileIn;
	int size, file_size;
	char filename[MAX_QPATH];

	cls.numglobalservers = cls.numfavoriteservers = 0;
	cls.numGlobalServerAddresses = 0;

	if ( com_gameInfo.usesProfiles && cl_profile->string[0] ) {
		Com_sprintf( filename, sizeof( filename ), "profiles/%s/servercache.dat", cl_profile->string );
	} else {
		Q_strncpyz( filename, "servercache.dat", sizeof( filename ) );
	}

	// Arnout: moved to mod/profiles dir
	//if (FS_SV_FOpenFileRead(filename, &fileIn)) { 
	//if ( FS_FOpenFileRead( filename, &fileIn, qtrue ) ) { 
	file_size = FS_Home_FOpenFileRead( filename, &fileIn );
	if ( file_size < (3*sizeof(int)) ) {
		if ( fileIn != FS_INVALID_HANDLE ) {
			FS_FCloseFile( fileIn );
		}
		return;
	}

	FS_Read( &cls.numglobalservers, sizeof(int), fileIn );
	FS_Read( &cls.numfavoriteservers, sizeof(int), fileIn );
	FS_Read( &size, sizeof(int), fileIn );

	if ( size == sizeof(cls.globalServers) + sizeof(cls.favoriteServers) ) {
		FS_Read( &cls.globalServers, sizeof(cls.globalServers), fileIn );
		FS_Read( &cls.favoriteServers, sizeof(cls.favoriteServers), fileIn );
	} else {
		cls.numglobalservers = cls.numfavoriteservers = 0;
		cls.numGlobalServerAddresses = 0;
	}

	FS_FCloseFile( fileIn );
}


/*
====================
LAN_SaveServersToCache
====================
*/
static void LAN_SaveServersToCache( void ) {
	fileHandle_t fileOut;
	int size;
	char filename[MAX_QPATH];

	if ( com_gameInfo.usesProfiles && cl_profile->string[0] ) {
		Com_sprintf( filename, sizeof( filename ), "profiles/%s/servercache.dat", cl_profile->string );
	} else {
		Q_strncpyz( filename, "servercache.dat", sizeof( filename ) );
	}

	// Arnout: moved to mod/profiles dir
	//fileOut = FS_SV_FOpenFileWrite(filename);
	fileOut = FS_FOpenFileWrite( filename );
	if ( fileOut == FS_INVALID_HANDLE )
		return;

	FS_Write(&cls.numglobalservers, sizeof(int), fileOut);
	FS_Write(&cls.numfavoriteservers, sizeof(int), fileOut);
	size = sizeof(cls.globalServers) + sizeof(cls.favoriteServers);
	FS_Write(&size, sizeof(int), fileOut);
	FS_Write(&cls.globalServers, sizeof(cls.globalServers), fileOut);
	FS_Write(&cls.favoriteServers, sizeof(cls.favoriteServers), fileOut);

	FS_FCloseFile(fileOut);
}


/*
====================
LAN_ResetPings
====================
*/
static void LAN_ResetPings(int source) {
	int count,i;
	serverInfo_t *servers = NULL;
	count = 0;

	switch (source) {
		case AS_LOCAL :
			servers = &cls.localServers[0];
			count = MAX_OTHER_SERVERS;
			break;
		case AS_GLOBAL :
			servers = &cls.globalServers[0];
			count = MAX_GLOBAL_SERVERS;
			break;
		case AS_FAVORITES :
			servers = &cls.favoriteServers[0];
			count = MAX_OTHER_SERVERS;
			break;
	}
	if (servers) {
		for (i = 0; i < count; i++) {
			servers[i].ping = -1;
		}
	}
}


/*
====================
LAN_AddServer
====================
*/
static int LAN_AddServer(int source, const char *name, const char *address) {
	int max, *count, i;
	netadr_t adr;
	serverInfo_t *servers = NULL;
	max = MAX_OTHER_SERVERS;
	count = NULL;

	switch (source) {
		case AS_LOCAL :
			count = &cls.numlocalservers;
			servers = &cls.localServers[0];
			break;
		case AS_GLOBAL :
			max = MAX_GLOBAL_SERVERS;
			count = &cls.numglobalservers;
			servers = &cls.globalServers[0];
			break;
		case AS_FAVORITES :
			count = &cls.numfavoriteservers;
			servers = &cls.favoriteServers[0];
			break;
	}
	if (servers && *count < max) {
		NET_StringToAdr( address, &adr, NA_UNSPEC );
		for ( i = 0; i < *count; i++ ) {
			if (NET_CompareAdr(&servers[i].adr, &adr)) {
				break;
			}
		}
		if (i >= *count) {
			servers[*count].adr = adr;
			Q_strncpyz(servers[*count].hostName, name, sizeof(servers[*count].hostName));
			servers[*count].visible = qtrue;
			(*count)++;
			return 1;
		}
		return 0;
	}
	return -1;
}

int LAN_AddFavAddr( const char *address ) {
	if ( cls.numfavoriteservers < MAX_OTHER_SERVERS ) {
		netadr_t adr;
		int i;
		if ( !NET_StringToAdr( address, &adr, NA_UNSPEC ) ) {
			return 2;
		}
		if ( adr.type == NA_BAD ) {
			return 3;
		}

		for ( i = 0; i < cls.numfavoriteservers; i++ ) {
			if ( NET_CompareAdr( &cls.favoriteServers[i].adr, &adr ) ) {
				return 0;
			}
		}
		cls.favoriteServers[cls.numfavoriteservers].adr = adr;
		Q_strncpyz( cls.favoriteServers[cls.numfavoriteservers].hostName, address,
			sizeof(cls.favoriteServers[cls.numfavoriteservers].hostName) );
		cls.favoriteServers[cls.numfavoriteservers].visible = qtrue;
		cls.numfavoriteservers++;
		return 1;
	}

	return -1;
}

/*
====================
LAN_RemoveServer
====================
*/
static void LAN_RemoveServer(int source, const char *addr) {
	int *count, i;
	serverInfo_t *servers = NULL;
	count = NULL;
	switch (source) {
		case AS_LOCAL :
			count = &cls.numlocalservers;
			servers = &cls.localServers[0];
			break;
		case AS_GLOBAL :
			count = &cls.numglobalservers;
			servers = &cls.globalServers[0];
			break;
		case AS_FAVORITES :
			count = &cls.numfavoriteservers;
			servers = &cls.favoriteServers[0];
			break;
	}
	if (servers) {
		netadr_t comp;
		NET_StringToAdr( addr, &comp, NA_UNSPEC );
		for (i = 0; i < *count; i++) {
			if (NET_CompareAdr( &comp, &servers[i].adr)) {
				int j = i;
				while (j < *count - 1) {
					Com_Memcpy(&servers[j], &servers[j+1], sizeof(servers[j]));
					j++;
				}
				(*count)--;
				break;
			}
		}
	}
}


/*
====================
LAN_GetServerCount
====================
*/
static int LAN_GetServerCount( int source ) {
	switch (source) {
		case AS_LOCAL :
			return cls.numlocalservers;
			break;
		case AS_GLOBAL :
			return cls.numglobalservers;
			break;
		case AS_FAVORITES :
			return cls.numfavoriteservers;
			break;
	}
	return 0;
}


/*
====================
LAN_GetLocalServerAddressString
====================
*/
static void LAN_GetServerAddressString( int source, int n, char *buf, int buflen ) {
	switch (source) {
		case AS_LOCAL :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				Q_strncpyz(buf, NET_AdrToStringwPort( &cls.localServers[n].adr) , buflen );
				return;
			}
			break;
		case AS_GLOBAL :
			if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
				Q_strncpyz(buf, NET_AdrToStringwPort( &cls.globalServers[n].adr) , buflen );
				return;
			}
			break;
		case AS_FAVORITES :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				Q_strncpyz(buf, NET_AdrToStringwPort( &cls.favoriteServers[n].adr) , buflen );
				return;
			}
			break;
	}
	buf[0] = '\0';
}


/*
====================
LAN_GetServerInfo
====================
*/
static void LAN_GetServerInfo( int source, int n, char *buf, int buflen ) {
	char info[MAX_STRING_CHARS];
	serverInfo_t *server = NULL;
	info[0] = '\0';
	switch (source) {
		case AS_LOCAL :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				server = &cls.localServers[n];
			}
			break;
		case AS_GLOBAL :
			if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
				server = &cls.globalServers[n];
			}
			break;
		case AS_FAVORITES :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				server = &cls.favoriteServers[n];
			}
			break;
	}
	if (server && buf) {
		buf[0] = '\0';
		Info_SetValueForKey( info, "hostname", server->hostName );
		Info_SetValueForKey( info, "serverload", va( "%i", server->load ) );
		Info_SetValueForKey( info, "mapname", server->mapName );
		Info_SetValueForKey( info, "clients", va( "%i",server->clients ) );
		Info_SetValueForKey( info, "sv_maxclients", va( "%i",server->maxClients ) );
		Info_SetValueForKey( info, "ping", va( "%i",server->ping ) );
		Info_SetValueForKey( info, "minping", va( "%i",server->minPing ) );
		Info_SetValueForKey( info, "maxping", va( "%i",server->maxPing ) );
		Info_SetValueForKey( info, "game", server->game );
		Info_SetValueForKey( info, "gametype", va( "%i",server->gameType ) );
		Info_SetValueForKey( info, "nettype", va( "%i",server->netType ) );
		Info_SetValueForKey( info, "addr", NET_AdrToStringwPort( &server->adr ) );
		Info_SetValueForKey( info, "friendlyFire", va( "%i", server->friendlyFire ) );               // NERVE - SMF
		Info_SetValueForKey( info, "maxlives", va( "%i", server->maxlives ) );                       // NERVE - SMF
		Info_SetValueForKey( info, "needpass", va( "%i", server->needpass ) );                       // NERVE - SMF
		Info_SetValueForKey( info, "punkbuster", va( "%i", server->punkbuster ) );                   // DHM - Nerve
		Info_SetValueForKey( info, "gamename", server->gameName );                                // Arnout
		Info_SetValueForKey( info, "g_antilag", va( "%i", server->antilag ) ); // TTimo
		Info_SetValueForKey( info, "weaprestrict", va( "%i", server->weaprestrict ) );
		Info_SetValueForKey( info, "balancedteams", va( "%i", server->balancedteams ) );
		Info_SetValueForKey( info, "g_oss", va( "%i", server->oss ) );
		Q_strncpyz( buf, info, buflen );
	} else {
		if (buf) {
			buf[0] = '\0';
		}
	}
}


/*
====================
LAN_GetServerPing
====================
*/
static int LAN_GetServerPing( int source, int n ) {
	serverInfo_t *server = NULL;
	switch (source) {
		case AS_LOCAL :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				server = &cls.localServers[n];
			}
			break;
		case AS_GLOBAL :
			if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
				server = &cls.globalServers[n];
			}
			break;
		case AS_FAVORITES :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				server = &cls.favoriteServers[n];
			}
			break;
	}
	if (server) {
		return server->ping;
	}
	return -1;
}

/*
====================
LAN_GetServerPtr
====================
*/
static serverInfo_t *LAN_GetServerPtr( int source, int n ) {
	switch (source) {
		case AS_LOCAL :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				return &cls.localServers[n];
			}
			break;
		case AS_GLOBAL :
			if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
				return &cls.globalServers[n];
			}
			break;
		case AS_FAVORITES :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				return &cls.favoriteServers[n];
			}
			break;
	}
	return NULL;
}


/*
====================
LAN_CompareServers
====================
*/
static int LAN_CompareServers( int source, int sortKey, int sortDir, int s1, int s2 ) {
	int res;
	serverInfo_t *server1, *server2;
	char name1[ MAX_NAME_LENGTH ], name2[ MAX_NAME_LENGTH ];
	int clients1, clients2;

	server1 = LAN_GetServerPtr( source, s1 );
	server2 = LAN_GetServerPtr( source, s2 );
	if ( !server1 || !server2 ) {
		return 0;
	}

	res = 0;
	switch ( sortKey ) {
	case SORT_HOST:
		//%	res = Q_stricmp( server1->hostName, server2->hostName );
		Q_strncpyz( name1, server1->hostName, sizeof( name1 ) );
		Q_CleanStr( name1 );
		Q_strncpyz( name2, server2->hostName, sizeof( name2 ) );
		Q_CleanStr( name2 );
		res = Q_stricmp( name1, name2 );
		break;

	case SORT_MAP:
		res = Q_stricmp( server1->mapName, server2->mapName );
		break;
	case SORT_CLIENTS:
		// sub sort by max clients
		if ( server1->clients == server2->clients ) {
			clients1 = server1->maxClients;
			clients2 = server2->maxClients;
		} else {
			clients1 = server1->clients;
			clients2 = server2->clients;
		}


		if ( clients1 < clients2 ) {
			res = -1;
		} else if ( clients1 > clients2 )     {
			res = 1;
		} else {
			res = 0;
		}
		break;
	case SORT_GAME:
		if ( server1->gameType < server2->gameType ) {
			res = -1;
		} else if ( server1->gameType > server2->gameType )     {
			res = 1;
		} else {
			res = 0;
		}
		break;
	case SORT_PING:
		if ( server1->ping < server2->ping ) {
			res = -1;
		} else if ( server1->ping > server2->ping )     {
			res = 1;
		} else {
			res = 0;
		}
		break;
	}

	if (sortDir) {
		if (res < 0)
			return 1;
		if (res > 0)
			return -1;
		return 0;
	}
	return res;
}


/*
====================
LAN_GetPingQueueCount
====================
*/
static int LAN_GetPingQueueCount( void ) {
	return (CL_GetPingQueueCount());
}


/*
====================
LAN_ClearPing
====================
*/
static void LAN_ClearPing( int n ) {
	CL_ClearPing( n );
}


/*
====================
LAN_GetPing
====================
*/
static void LAN_GetPing( int n, char *buf, int buflen, int *pingtime ) {
	CL_GetPing( n, buf, buflen, pingtime );
}


/*
====================
LAN_GetPingInfo
====================
*/
static void LAN_GetPingInfo( int n, char *buf, int buflen ) {
	CL_GetPingInfo( n, buf, buflen );
}


/*
====================
LAN_MarkServerVisible
====================
*/
static void LAN_MarkServerVisible(int source, int n, qboolean visible ) {
	if (n == -1) {
		int count = MAX_OTHER_SERVERS;
		serverInfo_t *server = NULL;
		switch (source) {
			case AS_LOCAL :
				server = &cls.localServers[0];
				break;
			case AS_GLOBAL :
				server = &cls.globalServers[0];
				count = MAX_GLOBAL_SERVERS;
				break;
			case AS_FAVORITES :
				server = &cls.favoriteServers[0];
				break;
		}
		if (server) {
			for (n = 0; n < count; n++) {
				server[n].visible = visible;
			}
		}

	} else {
		switch (source) {
			case AS_LOCAL :
				if (n >= 0 && n < MAX_OTHER_SERVERS) {
					cls.localServers[n].visible = visible;
				}
				break;
			case AS_GLOBAL :
				if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
					cls.globalServers[n].visible = visible;
				}
				break;
			case AS_FAVORITES :
				if (n >= 0 && n < MAX_OTHER_SERVERS) {
					cls.favoriteServers[n].visible = visible;
				}
				break;
		}
	}
}


/*
=======================
LAN_ServerIsVisible
=======================
*/
static int LAN_ServerIsVisible(int source, int n ) {
	switch (source) {
		case AS_LOCAL :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				return cls.localServers[n].visible;
			}
			break;
		case AS_GLOBAL :
			if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
				return cls.globalServers[n].visible;
			}
			break;
		case AS_FAVORITES :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				return cls.favoriteServers[n].visible;
			}
			break;
	}
	return qfalse;
}


/*
=======================
LAN_UpdateVisiblePings
=======================
*/
static qboolean LAN_UpdateVisiblePings(int source ) {
	return CL_UpdateVisiblePings_f(source);
}


/*
====================
LAN_GetServerStatus
====================
*/
static int LAN_GetServerStatus( const char *serverAddress, char *serverStatus, int maxLen ) {
	return CL_ServerStatus( serverAddress, serverStatus, maxLen );
}

/*
=======================
LAN_ServerIsInFavoriteList
=======================
*/
qboolean LAN_ServerIsInFavoriteList( int source, int n ) {
	int i;
	serverInfo_t *server = NULL;

	switch ( source ) {
	case AS_LOCAL:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			server = &cls.localServers[n];
		}
		break;
	case AS_GLOBAL:
		if ( n >= 0 && n < MAX_GLOBAL_SERVERS ) {
			server = &cls.globalServers[n];
		}
		break;
	case AS_FAVORITES:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			return qtrue;
		}
		break;
	}

	if ( !server ) {
		return qfalse;
	}

	for ( i = 0; i < cls.numfavoriteservers; i++ ) {
		if ( NET_CompareAdr( &cls.favoriteServers[i].adr, &server->adr ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

/*
====================
CL_GetGlConfig
====================
*/
static void CL_GetGlconfig( glconfig_t *config ) {
	*config = *re.GetConfig();
}


/*
====================
CL_GetClipboardData
====================
*/
static void CL_GetClipboardData( char *buf, int buflen ) {
	char	*cbd;

	cbd = Sys_GetClipboardData();

	if ( !cbd ) {
		*buf = '\0';
		return;
	}

	Q_strncpyz( buf, cbd, buflen );

	Z_Free( cbd );
}


/*
====================
CLUI_GetCDKey
====================
*/
static void CLUI_GetCDKey( char *buf, int buflen ) {
	*buf = '\0';
}


/*
====================
GetConfigString
====================
*/
static int GetConfigString(int index, char *buf, int size)
{
	int		offset;

	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		return qfalse;

	offset = cl.gameState.stringOffsets[index];
	if (!offset) {
		if( size ) {
			buf[0] = '\0';
		}
		return qfalse;
	}

	Q_strncpyz( buf, cl.gameState.stringData+offset, size);

	return qtrue;
}


/*
====================
FloatAsInt
====================
*/
static int FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}


/*
====================
VM_ArgPtr
====================
*/
static void *VM_ArgPtr( intptr_t intValue ) {

	if ( !intValue || uivm == NULL )
	  return NULL;

	//if ( uivm->entryPoint )
		return (void *)(intValue);
	//else
	//	return (void *)(uivm->dataBase + (intValue & uivm->dataMask));
}


static qboolean CL_UI_GetValue( char* value, int valueSize, const char* key ) {

	if ( !Q_stricmp( key, "trap_R_AddRefEntityToScene2" ) ) {
		Com_sprintf( value, valueSize, "%i", UI_R_ADDREFENTITYTOSCENE2 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_R_AddLinearLightToScene_ETE" ) && re.AddLinearLightToScene ) {
		Com_sprintf( value, valueSize, "%i", UI_R_ADDLINEARLIGHTTOSCENE );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_RemoveCommand") ) {
		Com_sprintf( value, valueSize, "%i", UI_REMOVECOMMAND );
		return qtrue;
	}

	// UTF-8 not yet supported
	if ( !Q_stricmp( key, "cap_UTF8" ) ) {
		Com_sprintf( value, valueSize, "%i", 0 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "cap_TTF_RegisterFont" ) ) {
		Com_sprintf( value, valueSize, "%i", 0 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "cap_SVG" ) ) {
		Com_sprintf( value, valueSize, "%i", 1 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "cap_PNG" ) ) {
		Com_sprintf( value, valueSize, "%i", 1 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "engine_is_ete" ) ) {
		Com_sprintf( value, valueSize, "%i", 1 );
		return qtrue;
	}

	return qfalse;
}


void SV_CompleteMapName( char *args, int argNum );

/*
====================
CL_UISystemCalls

The ui module is making a system call
====================
*/
static intptr_t CL_UISystemCalls( intptr_t *args ) {
	switch( args[0] ) {
	case UI_ERROR:
		Com_Error( ERR_DROP, "%s", (const char*)VMA(1) );
		return 0;

	case UI_PRINT:
		Com_Printf( "%s", (const char*)VMA(1) );
		return 0;

	case UI_MILLISECONDS:
		return Sys_Milliseconds();

	case UI_CVAR_REGISTER:
		Cvar_Register( VMA(1), VMA(2), VMA(3), args[4], uivm->privateFlag );
		return 0;

	case UI_CVAR_UPDATE:
		Cvar_Update( VMA(1), uivm->privateFlag );
		return 0;

	case UI_CVAR_SET:
		Cvar_SetSafe( VMA(1), VMA(2) );
		return 0;

	case UI_CVAR_VARIABLEVALUE:
		return FloatAsInt( Cvar_VariableValue( VMA(1) ) );

	case UI_CVAR_VARIABLESTRINGBUFFER:
		Cvar_VariableStringBufferSafe( VMA(1), VMA(2), args[3], uivm->privateFlag );
		return 0;

	case UI_CVAR_LATCHEDVARIABLESTRINGBUFFER:
		Cvar_LatchedVariableStringBufferSafe( VMA(1), VMA(2), args[3], uivm->privateFlag );
		return 0;

	case UI_CVAR_SETVALUE:
		Cvar_SetValueSafe( VMA(1), VMF(2) );
		return 0;

	case UI_CVAR_RESET:
		Cvar_Reset( VMA(1) );
		return 0;

	case UI_CVAR_CREATE:
		Cvar_Register( NULL, VMA(1), VMA(2), args[3], uivm->privateFlag );
		return 0;

	case UI_CVAR_INFOSTRINGBUFFER:
		Cvar_InfoStringBuffer( args[1], VMA(2), args[3] );
		return 0;

	case UI_ARGC:
		return Cmd_Argc();

	case UI_ARGV:
		Cmd_ArgvBuffer( args[1], VMA(2), args[3] );
		return 0;

	case UI_CMD_EXECUTETEXT:
		{
			const char *cmd = (const char *)VMA(2);
			if(args[1] == EXEC_NOW
				&& (!strncmp(cmd, "snd_restart", 11)
				|| !strncmp(cmd, "vid_restart", 11)
				|| !strncmp(cmd, "game_restart", 12)
				|| !strncmp(cmd, "disconnect", 10)
				|| !strncmp(cmd, "quit", 5)))
			{
				Com_Printf (S_COLOR_YELLOW "turning EXEC_NOW '%.11s' into EXEC_INSERT\n", cmd);
				args[1] = EXEC_INSERT;
			}
			if(args[1] == EXEC_APPEND && !strncmp(cmd, "exec preset_", 12))
			{
				Com_Printf (S_COLOR_YELLOW "bypassing preset command '%s'\n", cmd);
				return 0;
			}
			//if( args[1] == EXEC_APPEND ) {
			//	nestedCmdOffset = Cbuf_Add( cmd, nestedCmdOffset );
			//}
			//else
				Cbuf_ExecuteText( args[1], cmd );
		}
		return 0;

	case UI_ADDCOMMAND:
		Cmd_AddCommand( VMA(1), NULL );
		Cmd_SetModule( VMA(1), MODULE_UI );
		// Check to see if mod defines any new/additional map commands
		// Hook them up to the completion functor
		{
			const char *cmd_name = VMA(1);
			if( COM_SuffixCompare( cmd_name, "devmap" ) ||
				COM_SuffixCompare( cmd_name, "map" ) )
			{
				Cmd_SetCommandCompletionFunc( cmd_name, SV_CompleteMapName );
			}
		}
		return 0;

	case UI_FS_FOPENFILE:
		return FS_VM_OpenFile( VMA(1), VMA(2), args[3], H_Q3UI );

	case UI_FS_READ:
		FS_VM_ReadFile( VMA(1), args[2], args[3], H_Q3UI );
		return 0;

	case UI_FS_WRITE:
		FS_VM_WriteFile( VMA(1), args[2], args[3], H_Q3UI );
		return 0;

	case UI_FS_FCLOSEFILE:
		FS_VM_CloseFile( args[1], H_Q3UI );
		return 0;

	case UI_FS_DELETEFILE:
		return FS_Delete( VMA(1) );

	case UI_FS_GETFILELIST:
		return FS_GetFileList( VMA(1), VMA(2), VMA(3), args[4] );

	case UI_R_REGISTERMODEL:
		return re.RegisterModel( VMA(1) );

	case UI_R_REGISTERSKIN:
		return re.RegisterSkin( VMA(1) );

	case UI_R_REGISTERSHADERNOMIP:
		return re.RegisterShaderNoMip( VMA(1) );

	case UI_R_CLEARSCENE:
		re.ClearScene();
		return 0;

	case UI_R_ADDREFENTITYTOSCENE:
		re.AddRefEntityToScene( VMA(1), qfalse );
		return 0;

	case UI_R_ADDPOLYTOSCENE:
		re.AddPolyToScene( args[1], args[2], VMA(3) );
		return 0;

		// Ridah
	case UI_R_ADDPOLYSTOSCENE:
		re.AddPolysToScene( args[1], args[2], VMA(3), args[4] );
		return 0;
		// done.

	case UI_R_ADDLIGHTTOSCENE:
		// ydnar: new dlight code
		//%	re.AddLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5), args[6] );
		re.AddLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), args[7], args[8] );
		return 0;

	case UI_R_ADDCORONATOSCENE:
		re.AddCoronaToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5), args[6], args[7] );
		return 0;

	case UI_R_RENDERSCENE:
		re.RenderScene( VMA(1) );
		return 0;

	case UI_R_SETCOLOR:
		re.SetColor( VMA(1) );
		return 0;

	case UI_R_DRAW2DPOLYS:
		re.Add2dPolys( VMA(1), args[2], args[3] );
		return 0;

	case UI_R_DRAWSTRETCHPIC:
		re.DrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
		return 0;

	case UI_R_DRAWROTATEDPIC:
		re.DrawRotatedPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9], VMF(10) );
		return 0;

	case UI_R_MODELBOUNDS:
		re.ModelBounds( args[1], VMA(2), VMA(3) );
		return 0;

	case UI_UPDATESCREEN:
		SCR_UpdateScreen();
		return 0;

	case UI_CM_LERPTAG:
		return re.LerpTag( VMA(1), VMA(2), VMA(3), args[4] );

	case UI_S_REGISTERSOUND:
		return S_RegisterSound( VMA(1), args[2] );

	case UI_S_STARTLOCALSOUND:
		S_StartLocalSound( args[1], args[2], args[3] );
		return 0;

	case UI_S_FADESTREAMINGSOUND:
		S_FadeStreamingSound( VMF(1), args[2], args[3] );
		return 0;

	case UI_S_FADEALLSOUNDS:
		S_FadeAllSounds( VMF(1), args[2], args[3] );
		return 0;

	case UI_KEY_KEYNUMTOSTRINGBUF:
		Key_KeynumToStringBuf( args[1], VMA(2), args[3] );
		return 0;

	case UI_KEY_GETBINDINGBUF:
		Key_GetBindingBuf( args[1], VMA(2), args[3] );
		return 0;

	case UI_KEY_SETBINDING:
		Key_SetBinding( args[1], VMA(2) );
		return 0;

	case UI_KEY_BINDINGTOKEYS:
		Key_GetBindingByString( VMA(1), VMA(2), VMA(3) );
		return 0;


	case UI_KEY_ISDOWN:
		return Key_IsDown( args[1] );

	case UI_KEY_GETOVERSTRIKEMODE:
		return Key_GetOverstrikeMode();

	case UI_KEY_SETOVERSTRIKEMODE:
		Key_SetOverstrikeMode( args[1] );
		return 0;

	case UI_KEY_CLEARSTATES:
		Key_ClearStates();
		return 0;

	case UI_KEY_GETCATCHER:
		return Key_GetCatcher();

	case UI_KEY_SETCATCHER:
		Key_VM_SetCatcher( args[1] );
		return 0;

	case UI_GETCLIPBOARDDATA:
		CL_GetClipboardData( VMA(1), args[2] );
		return 0;

	case UI_GETCLIENTSTATE:
		GetClientState( VMA(1) );
		return 0;

	case UI_GETGLCONFIG:
		CL_GetGlconfig( VMA(1) );
		return 0;

	case UI_GETCONFIGSTRING:
		return GetConfigString( args[1], VMA(2), args[3] );

	case UI_LAN_LOADCACHEDSERVERS:
		LAN_LoadCachedServers();
		return 0;

	case UI_LAN_SAVECACHEDSERVERS:
		LAN_SaveServersToCache();
		return 0;

	case UI_LAN_ADDSERVER:
		return LAN_AddServer( args[1], VMA(2), VMA(3) );

	case UI_LAN_REMOVESERVER:
		LAN_RemoveServer( args[1], VMA(2) );
		return 0;

	case UI_LAN_GETPINGQUEUECOUNT:
		return LAN_GetPingQueueCount();

	case UI_LAN_CLEARPING:
		LAN_ClearPing( args[1] );
		return 0;

	case UI_LAN_GETPING:
		LAN_GetPing( args[1], VMA(2), args[3], VMA(4) );
		return 0;

	case UI_LAN_GETPINGINFO:
		LAN_GetPingInfo( args[1], VMA(2), args[3] );
		return 0;

	case UI_LAN_GETSERVERCOUNT:
		return LAN_GetServerCount( args[1] );

	case UI_LAN_GETSERVERADDRESSSTRING:
		LAN_GetServerAddressString( args[1], args[2], VMA(3), args[4] );
		return 0;

	case UI_LAN_GETSERVERINFO:
		LAN_GetServerInfo( args[1], args[2], VMA(3), args[4] );
		return 0;

	case UI_LAN_GETSERVERPING:
		return LAN_GetServerPing( args[1], args[2] );

	case UI_LAN_MARKSERVERVISIBLE:
		LAN_MarkServerVisible( args[1], args[2], args[3] );
		return 0;

	case UI_LAN_SERVERISVISIBLE:
		return LAN_ServerIsVisible( args[1], args[2] );

	case UI_LAN_UPDATEVISIBLEPINGS:
		return LAN_UpdateVisiblePings( args[1] );

	case UI_LAN_RESETPINGS:
		LAN_ResetPings( args[1] );
		return 0;

	case UI_LAN_SERVERSTATUS:
		return LAN_GetServerStatus( VMA(1), VMA(2), args[3] );

	case UI_LAN_SERVERISINFAVORITELIST:
		return LAN_ServerIsInFavoriteList( args[1], args[2] );

	case UI_SET_PBCLSTATUS:
		return 0;

	case UI_SET_PBSVSTATUS:
		return 0;

	case UI_LAN_COMPARESERVERS:
		return LAN_CompareServers( args[1], args[2], args[3], args[4], args[5] );

	case UI_MEMORY_REMAINING:
		return Hunk_MemoryRemaining();

	case UI_GET_CDKEY:
		CLUI_GetCDKey( VMA(1), args[2] );
		return 0;

	case UI_SET_CDKEY:
		return 0;

	case UI_R_REGISTERFONT:
		re.RegisterFont( VMA(1), args[2], VMA(3) );
		return 0;

	/*case UI_MEMSET:
		Com_Memset( VMA(1), args[2], args[3] );
		return args[1];

	case UI_MEMCPY:
		Com_Memcpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case UI_STRNCPY:
		strncpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case UI_SIN:
		return FloatAsInt( sin( VMF(1) ) );

	case UI_COS:
		return FloatAsInt( cos( VMF(1) ) );

	case UI_ATAN2:
		return FloatAsInt( atan2( VMF(1), VMF(2) ) );

	case UI_SQRT:
		return FloatAsInt( sqrt( VMF(1) ) );

	case UI_FLOOR:
		return FloatAsInt( floor( VMF(1) ) );

	case UI_CEIL:
		return FloatAsInt( ceil( VMF(1) ) );*/

	case UI_PC_ADD_GLOBAL_DEFINE:
		return PC_AddGlobalDefine( VMA(1) );
	case UI_PC_REMOVE_ALL_GLOBAL_DEFINES:
		PC_RemoveAllGlobalDefines();
		return 0;
	case UI_PC_LOAD_SOURCE:
		return PC_LoadSourceHandle( VMA(1) );
	case UI_PC_FREE_SOURCE:
		return PC_FreeSourceHandle( args[1] );
	case UI_PC_READ_TOKEN:
		return PC_ReadTokenHandle( args[1], VMA(2) );
	case UI_PC_SOURCE_FILE_AND_LINE:
		return PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );
	case UI_PC_UNREAD_TOKEN:
		PC_UnreadLastTokenHandle( args[1] );
		return 0;

	case UI_S_STOPBACKGROUNDTRACK:
		S_StopBackgroundTrack();
		return 0;
	case UI_S_STARTBACKGROUNDTRACK:
		S_StartBackgroundTrack( VMA(1), VMA(2), args[3] );   //----(SA)	added fadeup time
		return 0;

	case UI_REAL_TIME:
		return Com_RealTime( VMA(1) );

	case UI_CIN_PLAYCINEMATIC:
		Com_DPrintf( "UI_CIN_PlayCinematic\n" );
		return CIN_PlayCinematic( VMA(1), args[2], args[3], args[4], args[5], args[6] );

	case UI_CIN_STOPCINEMATIC:
		return CIN_StopCinematic( args[1] );

	case UI_CIN_RUNCINEMATIC:
		return CIN_RunCinematic( args[1] );

	case UI_CIN_DRAWCINEMATIC:
		CIN_DrawCinematic( args[1] );
		return 0;

	case UI_CIN_SETEXTENTS:
		CIN_SetExtents( args[1], args[2], args[3], args[4], args[5] );
		return 0;

	case UI_R_REMAP_SHADER:
		re.RemapShader( VMA(1), VMA(2), VMA(3) );
		return 0;

	case UI_VERIFY_CDKEY:
		return qtrue;

		// NERVE - SMF
	case UI_CL_GETLIMBOSTRING:
		return qtrue;
		//return CL_GetLimboString( args[1], VMA(2) );

	case UI_CL_TRANSLATE_STRING:
		CL_TranslateString( VMA(1), VMA(2) );
		return 0;
		// -NERVE - SMF

		// DHM - Nerve
	case UI_CHECKAUTOUPDATE:
		return 0;

	case UI_GET_AUTOUPDATE:
		return 0;
		// DHM - Nerve

	case UI_OPENURL:
		CL_OpenURL( (const char *)VMA(1) );
		return 0;

	case UI_GETHUNKDATA:
		Com_GetHunkInfo( VMA(1), VMA(2) );
		return 0;

	// engine extensions
	case UI_R_ADDREFENTITYTOSCENE2:
		re.AddRefEntityToScene( VMA(1), qtrue );
		return 0;

	// engine extensions
	case UI_R_ADDLINEARLIGHTTOSCENE:
		re.AddLinearLightToScene( VMA(1), VMA(2), VMF(3), VMF(4), VMF(5), VMF(6) );
		return 0;

	// engine extensions
	case UI_REMOVECOMMAND:
		Cmd_RemoveCommandSafe( VMA(1) );
		return 0;

	case UI_TRAP_GETVALUE:
		return CL_UI_GetValue( VMA(1), args[2], VMA(3) );

	default:
		Com_Error( ERR_DROP, "Bad UI system trap: %ld", (long int) args[0] );

	}

	return 0;
}


/*
====================
UI_DllSyscall
====================
*/
static intptr_t QDECL UI_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
	intptr_t	args[12]; // max.count for UI + VM_CALL_END
	va_list	ap;
	int i;
	size_t len = ARRAY_LEN(args);

	args[0] = arg;
	va_start(ap, arg);

	for (i = 1; i < len; i++) {
		args[i] = va_arg(ap, intptr_t);

		if (VM_CALL_END == (int)args[i]) {
			args[i] = 0;
			break;
		}
	}

	va_end(ap);

	return CL_UISystemCalls( args );
#else
	return CL_UISystemCalls( &arg );
#endif
}


/*
====================
CL_ShutdownUI
====================
*/
void CL_ShutdownUI( void ) {
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
	cls.uiStarted = qfalse;
	if ( !uivm ) {
		return;
	}
	VM_Call( uivm, UI_SHUTDOWN );
	VM_Free( uivm );
	uivm = NULL;
	Cmd_UnregisterModule( MODULE_UI );
	FS_VM_CloseFiles( H_Q3UI );
}


/*
====================
CL_InitUI
====================
*/

void CL_InitUI( void ) {
	int v;


	uivm = VM_Create( VM_UI, CL_UISystemCalls, UI_DllSyscall, VMI_NATIVE );
	if ( !uivm ) {
		if ( cl_connectedToPureServer && CL_GameSwitch() ) {
			// server-side modification may require and reference only single custom ui.qvm
			// so allow referencing everything until we download all files
			// new gamestate will be requested after downloads complete
			// which will correct filesystem permissions
			fs_reordered = qfalse;
			FS_ClearPureServerPaks();
			//FS_PureServerSetLoadedPaks( "", "" );
			uivm = VM_Create( VM_UI, CL_UISystemCalls, UI_DllSyscall, VMI_NATIVE );
			if ( !uivm ) {
				Com_Error( ERR_DROP, "VM_Create on UI failed\n\nMake sure " S_COLOR_GREEN "%s" S_COLOR_NULL " exists in the mod folder (%s)\nIf it does not, the mod may be incompatible with your system or a file extraction error has occured!", SYS_DLLNAME_UI, FS_GetCurrentGameDir() );
			}
		} else {
			Com_Error( ERR_DROP, "VM_Create on UI failed\n\nMake sure " S_COLOR_GREEN "%s" S_COLOR_NULL " exists in the mod folder (%s)\nIf it does not, the mod may be incompatible with your system or a file extraction error has occured!", SYS_DLLNAME_UI, FS_GetCurrentGameDir() );
		}
	}

	// sanity check
	v = VM_Call( uivm, UI_GETAPIVERSION );
	if ( v != UI_API_VERSION ) {
		// Free uivm now, so UI_SHUTDOWN doesn't get called later.
		VM_Free( uivm );
		uivm = NULL;

		Com_Error( ERR_DROP, "User Interface is version %d, expected %d", v, UI_API_VERSION );
		cls.uiStarted = qfalse;
	}
	else {
		// init for this gamestate
		if ( currentGameMod == GAMEMOD_LEGACY || currentGameMod == GAMEMOD_ETJUMP )
			VM_Call( uivm, UI_INIT, ( cls.state >= CA_AUTHORIZING && cls.state < CA_ACTIVE ), qtrue, com_legacyVersion->integer );
		else
			VM_Call( uivm, UI_INIT, ( cls.state >= CA_AUTHORIZING && cls.state < CA_ACTIVE ) );
	}
}


qboolean UI_usesUniqueCDKey() {
	if ( uivm ) {
		return ( VM_Call( uivm, UI_HASUNIQUECDKEY ) == qtrue );
	} else {
		return qfalse;
	}
}

qboolean UI_checkKeyExec( int key ) {
	if ( uivm ) {
		return VM_Call( uivm, UI_CHECKEXECKEY, key );
	} else {
		return qfalse;
	}
}

/*
====================
UI_GameCommand

See if the current console command is claimed by the ui
====================
*/
qboolean UI_GameCommand( void ) {
	if ( !uivm ) {
		return qfalse;
	}

	return VM_Call( uivm, UI_CONSOLE_COMMAND, cls.realtime );
}
