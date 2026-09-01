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

#include "q_shared.h"
#include "qcommon.h"
#include "../server/server.h"

// Arnout: gameinfo, to let the engine know which gametypes are SP and if we should use profiles.
// This can't be dependant on gamecode as we sometimes need to know about it when no game-modules
// are loaded
gameInfo_t com_gameInfo;
static qboolean firstLaunch = qtrue;

/*void Com_InitGameInfo( void ) {
	memset( &com_gameInfo, 0, sizeof( com_gameInfo ) );
}*/

void Com_GetGameInfo( void ) {
	union {
		char* c;
		void* v;
	} text;
	const char *buf, *token;

	memset( &com_gameInfo, 0, sizeof( com_gameInfo ) );

	if ( FS_ReadFile( "gameinfo.dat", &text.v ) > 0 ) {
		buf = text.c;

		while ( ( token = COM_Parse( &buf ) ) != NULL && token[0] ) {
			if ( !Q_stricmp( token, "spEnabled" ) ) {
				com_gameInfo.spEnabled = qtrue;
			} else if ( !Q_stricmp( token, "spGameTypes" ) ) {
				while ( ( token = COM_ParseExt( &buf, qfalse ) ) != NULL && token[0] ) {
					int val = atoi( token );
					if ( val >= 0 )
						com_gameInfo.spGameTypes |= ( 1 << val );
					else
						com_gameInfo.spGameTypes = 0;
				}
			} else if ( !Q_stricmp( token, "defaultSPGameType" ) ) {
				if ( ( token = COM_ParseExt( &buf, qfalse ) ) != NULL && token[0] ) {
					com_gameInfo.defaultSPGameType = atoi( token );
				} else {
					FS_FreeFile( text.v );
					Com_Error( ERR_FATAL, "Com_GetGameInfo: bad syntax." );
				}
			} else if ( !Q_stricmp( token, "coopGameTypes" ) ) {
				while ( ( token = COM_ParseExt( &buf, qfalse ) ) != NULL && token[0] ) {
					int val = atoi( token );
					if ( val >= 0 )
						com_gameInfo.coopGameTypes |= ( 1 << val );
					else
						com_gameInfo.coopGameTypes = 0;
				}
			} else if ( !Q_stricmp( token, "defaultCoopGameType" ) ) {
				if ( ( token = COM_ParseExt( &buf, qfalse ) ) != NULL && token[0] ) {
					com_gameInfo.defaultCoopGameType = atoi( token );
				} else {
					FS_FreeFile( text.v );
					Com_Error( ERR_FATAL, "Com_GetGameInfo: bad syntax." );
				}
			} else if ( !Q_stricmp( token, "defaultGameType" ) ) {
				if ( ( token = COM_ParseExt( &buf, qfalse ) ) != NULL && token[0] ) {
					com_gameInfo.defaultGameType = atoi( token );
				} else {
					FS_FreeFile( text.v );
					Com_Error( ERR_FATAL, "Com_GetGameInfo: bad syntax." );
				}
			} else if ( !Q_stricmp( token, "usesProfiles" ) ) {
				if ( ( token = COM_ParseExt( &buf, qfalse ) ) != NULL && token[0] ) {
					com_gameInfo.usesProfiles = atoi( token );
				} else {
					FS_FreeFile( text.v );
					Com_Error( ERR_FATAL, "Com_GetGameInfo: bad syntax." );
				}
			} else {
				FS_FreeFile( text.v );
				Com_Error( ERR_FATAL, "Com_GetGameInfo: bad syntax." );
			}
		}

		// all is good
		FS_FreeFile( text.v );
	}

	if ( !firstLaunch )
		Com_UpdateDefaultGametype();

	if ( firstLaunch == qtrue )
		firstLaunch = qfalse;
}

cvar_t *Cvar_Unset( cvar_t *cv );

void Com_UpdateDefaultGametype( void ) {
	if ( sv_gameType ) {
		Cvar_Unset( sv_gameType );
		sv_gameType = NULL;
	}
	sv_gameType = Cvar_Get( "g_gametype", va( "%i", com_gameInfo.defaultGameType ), CVAR_SERVERINFO | CVAR_LATCH );
}

/*
====================
SV_GameIsSinglePlayer
====================
*/
qboolean Com_GameIsSinglePlayer( void ) {
	return( com_gameInfo.spGameTypes & ( 1 << sv_gameType->integer ) );
}

/*
====================
SV_GameIsCoop

This is a modified SinglePlayer, no savegame capability for example
====================
*/
qboolean Com_GameIsCoop( void ) {
	return( com_gameInfo.coopGameTypes & ( 1 << sv_gameType->integer ) );
}
