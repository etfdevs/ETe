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


#include "server.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/


/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
client_t *SV_GetPlayerByHandle( void ) {
	client_t	*cl;
	int			i;
	char		*s;
	char		cleanName[ MAX_NAME_LENGTH ];

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return NULL;
	}

	s = Cmd_Argv(1);

	// Check whether this is a numeric player handle
	for(i = 0; s[i] >= '0' && s[i] <= '9'; i++);
	
	if(!s[i])
	{
		int plid = atoi(s);

		// Check for numeric playerid match
		if(plid >= 0 && plid < sv_maxclients->integer)
		{
			cl = &svs.clients[plid];
			
			if(cl->state)
				return cl;
		}
	}

	// check for a name match
	for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
		if ( cl->state < CS_CONNECTED ) {
			continue;
		}
		if ( !Q_stricmp( cl->name, s ) ) {
			return cl;
		}

		Q_strncpyz( cleanName, cl->name, sizeof(cleanName) );
		Q_CleanStr( cleanName );
		if ( !Q_stricmp( cleanName, s ) ) {
			return cl;
		}
	}

	Com_Printf( "Player %s is not on the server\n", s );

	return NULL;
}


/*
==================
SV_GetPlayerByNum

Returns the player with idnum from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByNum( void ) {
	client_t	*cl;
	int			i;
	int			idnum;
	char		*s;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return NULL;
	}

	s = Cmd_Argv(1);

	for (i = 0; s[i]; i++) {
		if (s[i] < '0' || s[i] > '9') {
			Com_Printf( "Bad slot number: %s\n", s);
			return NULL;
		}
	}
	idnum = atoi( s );
	if ( idnum < 0 || idnum >= sv_maxclients->integer ) {
		Com_Printf( "Bad client slot: %i\n", idnum );
		return NULL;
	}

	cl = &svs.clients[idnum];
	if ( !cl->state ) {
		Com_Printf( "Client %i is not active\n", idnum );
		return NULL;
	}
	return cl;
}

//=========================================================

int sv_cachedGametype = 0;

/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f( void ) {
	char        *cmd;
	char        *map;
	char mapname[MAX_QPATH];
	qboolean killBots, cheat;
	char expanded[MAX_QPATH];
	int			len;

	map = Cmd_Argv(1);
	if ( !map || !*map ) {
		return;
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	Com_sprintf( expanded, sizeof( expanded ), "maps/%s.bsp", map );
	// bypass pure check so we can open downloaded map
	FS_BypassPure();
	len = FS_FOpenFileRead( expanded, NULL, qfalse );
	FS_RestorePure();
	if ( len == -1 ) {
		Com_Printf( "Can't find map %s\n", expanded );
		return;
	}

	if ( sv_gameType->latchedString )
		sv_cachedGametype = atoi(sv_gameType->latchedString);
	else
		sv_cachedGametype = sv_gameType->integer;

	Cvar_Set( "gamestate", va( "%i", GS_INITIALIZE ) );       // NERVE - SMF - reset gamestate on map/devmap

	Cvar_Set( "g_currentRound", "0" );            // NERVE - SMF - reset the current round
	Cvar_Set( "g_nextTimeLimit", "0" );           // NERVE - SMF - reset the next time limit

	cmd = Cmd_Argv( 0 );

	if ( !Q_stricmp( cmd, "devmap" ) ) {
		cheat = qtrue;
		killBots = qtrue;
	}
	else {
		cheat = qfalse;
		killBots = qfalse;
	}

	// save the map name here cause on a map restart we reload the q3config.cfg
	// and thus nuke the arguments of the map command
	Q_strncpyz(mapname, map, sizeof(mapname));

	// start up the map
	SV_SpawnServer( mapname, killBots );

	// set the cheat value
	// if the level was started with "map <levelname>", then
	// cheats will not be allowed.  If started with "devmap <levelname>"
	// then cheats will be allowed
	if ( cheat ) {
		Cvar_Set( "sv_cheats", "1" );
	} else {
		Cvar_Set( "sv_cheats", "0" );
	}
}

/*
================
SV_CheckTransitionGameState

NERVE - SMF
================
*/
static qboolean SV_CheckTransitionGameState( gamestate_t new_gs, gamestate_t old_gs ) {
	if ( old_gs == new_gs && new_gs != GS_PLAYING ) {
		return qfalse;
	}

//	if ( old_gs == GS_WARMUP && new_gs != GS_WARMUP_COUNTDOWN )
//		return qfalse;

//	if ( old_gs == GS_WARMUP_COUNTDOWN && new_gs != GS_PLAYING )
//		return qfalse;

	if ( old_gs == GS_WAITING_FOR_PLAYERS && new_gs != GS_WARMUP ) {
		return qfalse;
	}

	if ( old_gs == GS_INTERMISSION && new_gs != GS_WARMUP ) {
		return qfalse;
	}

	if ( old_gs == GS_RESET && ( new_gs != GS_WAITING_FOR_PLAYERS && new_gs != GS_WARMUP ) ) {
		return qfalse;
	}

	return qtrue;
}

/*
================
SV_TransitionGameState

NERVE - SMF
================
*/
static qboolean SV_TransitionGameState( gamestate_t new_gs, gamestate_t old_gs, int delay ) {
	if ( !SV_GameIsSinglePlayer() && !SV_GameIsCoop() ) {
		// we always do a warmup before starting match
		if ( old_gs == GS_INTERMISSION && new_gs == GS_PLAYING ) {
			new_gs = GS_WARMUP;
		}
	}

	// check if its a valid state transition
	if ( !SV_CheckTransitionGameState( new_gs, old_gs ) ) {
		return qfalse;
	}

	if ( new_gs == GS_RESET ) {
		new_gs = GS_WARMUP;
	}

	Cvar_Set( "gamestate", va( "%i", new_gs ) );

	return qtrue;
}

#ifdef MSG_FIELDINFO_SORT_DEBUG
void MSG_PrioritiseEntitystateFields( void );
void MSG_PrioritisePlayerStateFields( void );

static void SV_FieldInfo_f( void ) {
	MSG_PrioritiseEntitystateFields();
	MSG_PrioritisePlayerStateFields();
}
#endif

/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f( void ) {
	int i;
	client_t    *client;
	char        *denied;
	qboolean isBot;
	int delay = 0;
	gamestate_t new_gs, old_gs;     // NERVE - SMF

	// make sure we aren't restarting twice in the same frame
	if ( com_frameTime == sv.serverId ) {
		return;
	}

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	// ydnar: allow multiple delayed server restarts [atvi bug 3813]
	//%	if ( sv.restartTime ) {
	//%		return;
	//%	}

	if ( Cmd_Argc() > 1 ) {
		delay = atoi( Cmd_Argv( 1 ) );
	}

	if ( delay ) {
		sv.restartTime = sv.time + delay * 1000;
		SV_SetConfigstring( CS_WARMUP, va("%i", sv.restartTime) );
		return;
	}

	// NERVE - SMF - read in gamestate or just default to GS_PLAYING
	old_gs = atoi( Cvar_VariableString( "gamestate" ) );

	if ( SV_GameIsSinglePlayer() || SV_GameIsCoop() ) {
		new_gs = GS_PLAYING;
	} else {
		if ( Cmd_Argc() > 2 ) {
			new_gs = atoi( Cmd_Argv( 2 ) );
		} else {
			new_gs = GS_PLAYING;
		}
	}

	if ( !SV_TransitionGameState( new_gs, old_gs, delay ) ) {
		return;
	}

	// check for changes in variables that can't just be restarted
	// check for maxclients change
#ifdef USE_MV
	if ( sv_maxclients->modified || sv_pure->modified || sv_mvClients->modified ) {
#else
	if ( sv_maxclients->modified || sv_pure->modified ) {
#endif
		char mapname[MAX_QPATH];

		Com_Printf( "variable change -- restarting.\n" );
		// restart the map the slow way
		Q_strncpyz( mapname, Cvar_VariableString( "mapname" ), sizeof( mapname ) );

#ifdef USE_MV
		SV_MultiViewStopRecord_f(); // as an alternative: save/restore recorder state and continue recording?
#endif

		SV_SpawnServer( mapname, qfalse );
		return;
	}

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new serverid	
	// TTimo - don't update restartedserverId there, otherwise we won't deal correctly with multiple map_restart
	sv.serverId = com_frameTime;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// if a map_restart occurs while a client is changing maps, we need
	// to give them the correct time so that when they finish loading
	// they don't violate the backwards time check in cl_cgame.c
	for (i=0 ; i<sv_maxclients->integer ; i++) {
		if (svs.clients[i].state == CS_PRIMED) {
			svs.clients[i].oldServerTime = sv.restartTime;
		}
	}

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	Cvar_Set( "sv_serverRestarting", "1" );

	// make sure that level time is not zero
	sv.time = sv.time ? sv.time : 8;

	SV_RestartGameProgs();

	// run a few frames to allow everything to settle
	for ( i = 0; i < GAME_INIT_FRAMES; i++ ) {
		sv.time += FRAMETIME;
		VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
	}

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		client = &svs.clients[i];

		// send the new gamestate to all connected clients
		if ( client->state < CS_CONNECTED ) {
			continue;
		}

		if ( client->netchan.remoteAddress.type == NA_BOT ) {
			if ( SV_GameIsSinglePlayer() || SV_GameIsCoop() ) {
				continue;   // dont carry across bots in single player
			}
			isBot = qtrue;
		} else {
			isBot = qfalse;
		}

		// add the map_restart command
		SV_AddServerCommand( client, "map_restart\n" );

		// connect the client again, without the firstTime flag
		denied = GVM_ArgPtr( VM_Call( gvm, 3, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );
		if ( denied ) {
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient( client, denied );
			if ( ( !SV_GameIsSinglePlayer() ) || ( !isBot ) ) {
				Com_Printf( "SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i ); // bk010125
			}
			continue;
		}

		if ( client->state == CS_ACTIVE )
			SV_ClientEnterWorld( client, &client->lastUsercmd );
		else {
			// If we don't reset client->lastUsercmd and are restarting during map load,
			// the client will hang because we'll use the last Usercmd from the previous map,
			// which is wrong obviously.
			SV_ClientEnterWorld( client, NULL );
		}
	}	
	// run another frame to allow things to look at all the players
	sv.time += FRAMETIME;
	VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
	svs.time += FRAMETIME;

	Cvar_Set( "sv_serverRestarting", "0" );
}


/*
==================
SV_Kick_f

Kick a user off of the server  FIXME: move to game
// fretn: done
==================
*/
/*
static void SV_Kick_f( void ) {
	client_t	*cl;
	int			i;
	int			timeout = -1;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() < 2 || Cmd_Argc() > 3 ) {
		Com_Printf ("Usage: kick <player name> [timeout]\n");
		return;
	}

	if( Cmd_Argc() == 3 ) {
		timeout = atoi( Cmd_Argv( 2 ) );
	} else {
		timeout = 300;
	}

	cl = SV_GetPlayerByName();
	if ( !cl ) {
		if ( !Q_stricmp(Cmd_Argv(1), "all") ) {
			for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
				if ( !cl->state ) {
					continue;
				}
				if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
					continue;
				}
				SV_DropClient( cl, "player kicked" ); // JPW NERVE to match front menu message
				if( timeout != -1 ) {
					SV_TempBanNetAddress( cl->netchan.remoteAddress, timeout );
				}
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		} else if ( !Q_stricmp(Cmd_Argv(1), "allbots") ) {
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
				if ( !cl->state ) {
					continue;
				}
				if( cl->netchan.remoteAddress.type != NA_BOT ) {
					continue;
				}
				SV_DropClient( cl, "was kicked" );
				if( timeout != -1 ) {
					SV_TempBanNetAddress( cl->netchan.remoteAddress, timeout );
				}
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	SV_DropClient( cl, "player kicked" ); // JPW NERVE to match front menu message
	if( timeout != -1 ) {
		SV_TempBanNetAddress( cl->netchan.remoteAddress, timeout );
	}
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}
*/

#ifdef USE_BANS
/*
==================
SV_RehashBans_f

Load saved bans from file.
==================
*/
static void SV_RehashBans_f(void)
{
	int index, filelen;
	fileHandle_t readfrom;
	char *textbuf, *curpos, *maskpos, *newlinepos, *endpos;
	char filepath[MAX_QPATH];
	
	// make sure server is running
	if ( !com_sv_running->integer ) {
		return;
	}
	
	serverBansCount = 0;
	
	if(!sv_banFile->string || !*sv_banFile->string)
		return;

	Com_sprintf(filepath, sizeof(filepath), "%s/%s", FS_GetCurrentGameDir(), sv_banFile->string);

	if((filelen = FS_SV_FOpenFileRead(filepath, &readfrom)) >= 0)
	{
		if(filelen < 2)
		{
			// Don't bother if file is too short.
			FS_FCloseFile(readfrom);
			return;
		}

		curpos = textbuf = Z_Malloc(filelen);
		
		filelen = FS_Read(textbuf, filelen, readfrom);
		FS_FCloseFile(readfrom);
		
		endpos = textbuf + filelen;
		
		for(index = 0; index < SERVER_MAXBANS && curpos + 2 < endpos; index++)
		{
			// find the end of the address string
			for(maskpos = curpos + 2; maskpos < endpos && *maskpos != ' '; maskpos++);
			
			if(maskpos + 1 >= endpos)
				break;

			*maskpos = '\0';
			maskpos++;
			
			// find the end of the subnet specifier
			for(newlinepos = maskpos; newlinepos < endpos && *newlinepos != '\n'; newlinepos++);
			
			if(newlinepos >= endpos)
				break;
			
			*newlinepos = '\0';
			
			if(NET_StringToAdr(curpos + 2, &serverBans[index].ip, NA_UNSPEC))
			{
				serverBans[index].isexception = (curpos[0] != '0');
				serverBans[index].subnet = atoi(maskpos);
				
				if(serverBans[index].ip.type == NA_IP &&
				   (serverBans[index].subnet < 1 || serverBans[index].subnet > 32))
				{
					serverBans[index].subnet = 32;
				}
				else if(serverBans[index].ip.type == NA_IP6 &&
					(serverBans[index].subnet < 1 || serverBans[index].subnet > 128))
				{
					serverBans[index].subnet = 128;
				}
			}
			
			curpos = newlinepos + 1;
		}
			
		serverBansCount = index;
		
		Z_Free(textbuf);
	}
}

/*
==================
SV_WriteBans

Save bans to file.
==================
*/
static void SV_WriteBans(void)
{
	int index;
	fileHandle_t writeto;
	char filepath[MAX_QPATH];
	
	if(!sv_banFile->string || !*sv_banFile->string)
		return;
	
	Com_sprintf(filepath, sizeof(filepath), "%s/%s", FS_GetCurrentGameDir(), sv_banFile->string);

	if((writeto = FS_SV_FOpenFileWrite(filepath)))
	{
		char writebuf[128];
		serverBan_t *curban;
		
		for(index = 0; index < serverBansCount; index++)
		{
			curban = &serverBans[index];
			
			Com_sprintf(writebuf, sizeof(writebuf), "%d %s %d\n",
				    curban->isexception, NET_AdrToString(&curban->ip), curban->subnet);
			FS_Write(writebuf, strlen(writebuf), writeto);
		}

		FS_FCloseFile(writeto);
	}
}

/*
==================
SV_DelBanEntryFromList

Remove a ban or an exception from the list.
==================
*/

static qboolean SV_DelBanEntryFromList(int index)
{
	if(index == serverBansCount - 1)
		serverBansCount--;
	else if(index < ARRAY_LEN(serverBans) - 1)
	{
		memmove(serverBans + index, serverBans + index + 1, (serverBansCount - index - 1) * sizeof(*serverBans));
		serverBansCount--;
	}
	else
		return qtrue;

	return qfalse;
}

/*
==================
SV_ParseCIDRNotation

Parse a CIDR notation type string and return a netadr_t and suffix by reference
==================
*/

static qboolean SV_ParseCIDRNotation(netadr_t *dest, int *mask, char *adrstr)
{
	char *suffix;
	
	suffix = strchr(adrstr, '/');
	if(suffix)
	{
		*suffix = '\0';
		suffix++;
	}

	if(!NET_StringToAdr(adrstr, dest, NA_UNSPEC))
		return qtrue;

	if(suffix)
	{
		*mask = atoi(suffix);
		
		if(dest->type == NA_IP)
		{
			if(*mask < 1 || *mask > 32)
				*mask = 32;
		}
		else
		{
			if(*mask < 1 || *mask > 128)
				*mask = 128;
		}
	}
	else if(dest->type == NA_IP)
		*mask = 32;
	else
		*mask = 128;
	
	return qfalse;
}

/*
==================
SV_AddBanToList

Ban a user from being able to play on this server based on his ip address.
==================
*/

static void SV_AddBanToList(qboolean isexception)
{
	char *banstring;
	char addy2[NET_ADDRSTRMAXLEN];
	netadr_t ip;
	int index, argc, mask;
	serverBan_t *curban;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	argc = Cmd_Argc();
	
	if(argc < 2 || argc > 3)
	{
		Com_Printf ("Usage: %s (ip[/subnet] | clientnum [subnet])\n", Cmd_Argv(0));
		return;
	}

	if(serverBansCount >= ARRAY_LEN(serverBans))
	{
		Com_Printf ("Error: Maximum number of bans/exceptions exceeded.\n");
		return;
	}

	banstring = Cmd_Argv(1);
	
	if(strchr(banstring, '.') || strchr(banstring, ':'))
	{
		// This is an ip address, not a client num.
		
		if(SV_ParseCIDRNotation(&ip, &mask, banstring))
		{
			Com_Printf("Error: Invalid address %s\n", banstring);
			return;
		}
	}
	else
	{
		client_t *cl;
		
		// client num.
		
		cl = SV_GetPlayerByNum();

		if(!cl)
		{
			Com_Printf("Error: Playernum %s does not exist.\n", Cmd_Argv(1));
			return;
		}
		
		ip = cl->netchan.remoteAddress;
		
		if(argc == 3)
		{
			mask = atoi(Cmd_Argv(2));
			
			if(ip.type == NA_IP)
			{
				if(mask < 1 || mask > 32)
					mask = 32;
			}
			else
			{
				if(mask < 1 || mask > 128)
					mask = 128;
			}
		}
		else
			mask = (ip.type == NA_IP6) ? 128 : 32;
	}

	if(ip.type != NA_IP && ip.type != NA_IP6)
	{
		Com_Printf("Error: Can ban players connected via the internet only.\n");
		return;
	}

	// first check whether a conflicting ban exists that would supersede the new one.
	for(index = 0; index < serverBansCount; index++)
	{
		curban = &serverBans[index];
		
		if(curban->subnet <= mask)
		{
			if((curban->isexception || !isexception) && NET_CompareBaseAdrMask(&curban->ip, ip, &curban->subnet))
			{
				Q_strncpyz(addy2, NET_AdrToString(&ip), sizeof(addy2));
				
				Com_Printf("Error: %s %s/%d supersedes %s %s/%d\n", curban->isexception ? "Exception" : "Ban",
					   NET_AdrToString(&curban->ip), curban->subnet,
					   isexception ? "exception" : "ban", addy2, mask);
				return;
			}
		}
		if(curban->subnet >= mask)
		{
			if(!curban->isexception && isexception && NET_CompareBaseAdrMask(&curban->ip, &ip, mask))
			{
				Q_strncpyz(addy2, NET_AdrToString(&curban->ip), sizeof(addy2));
			
				Com_Printf("Error: %s %s/%d supersedes already existing %s %s/%d\n", isexception ? "Exception" : "Ban",
					   NET_AdrToString(&ip), mask,
					   curban->isexception ? "exception" : "ban", addy2, curban->subnet);
				return;
			}
		}
	}

	// now delete bans that are superseded by the new one
	index = 0;
	while(index < serverBansCount)
	{
		curban = &serverBans[index];
		
		if(curban->subnet > mask && (!curban->isexception || isexception) && NET_CompareBaseAdrMask(&curban->ip, &ip, mask))
			SV_DelBanEntryFromList(index);
		else
			index++;
	}

	serverBans[serverBansCount].ip = ip;
	serverBans[serverBansCount].subnet = mask;
	serverBans[serverBansCount].isexception = isexception;
	
	serverBansCount++;
	
	SV_WriteBans();

	Com_Printf("Added %s: %s/%d\n", isexception ? "ban exception" : "ban",
		   NET_AdrToString(&ip), mask);
}

/*
==================
SV_DelBanFromList

Remove a ban or an exception from the list.
==================
*/

static void SV_DelBanFromList(qboolean isexception)
{
	int index, count = 0, todel, mask;
	netadr_t ip;
	char *banstring;
	
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	
	if(Cmd_Argc() != 2)
	{
		Com_Printf ("Usage: %s (ip[/subnet] | num)\n", Cmd_Argv(0));
		return;
	}

	banstring = Cmd_Argv(1);
	
	if(strchr(banstring, '.') || strchr(banstring, ':'))
	{
		serverBan_t *curban;
		
		if(SV_ParseCIDRNotation(&ip, &mask, banstring))
		{
			Com_Printf("Error: Invalid address %s\n", banstring);
			return;
		}
		
		index = 0;
		
		while(index < serverBansCount)
		{
			curban = &serverBans[index];
			
			if(curban->isexception == isexception		&&
			   curban->subnet >= mask 			&&
			   NET_CompareBaseAdrMask(&curban->ip, &ip, mask))
			{
				Com_Printf("Deleting %s %s/%d\n",
					   isexception ? "exception" : "ban",
					   NET_AdrToString(&curban->ip), curban->subnet);
					   
				SV_DelBanEntryFromList(index);
			}
			else
				index++;
		}
	}
	else
	{
		todel = atoi(Cmd_Argv(1));

		if(todel < 1 || todel > serverBansCount)
		{
			Com_Printf("Error: Invalid ban number given\n");
			return;
		}
	
		for(index = 0; index < serverBansCount; index++)
		{
			if(serverBans[index].isexception == isexception)
			{
				count++;
			
				if(count == todel)
				{
					Com_Printf("Deleting %s %s/%d\n",
					   isexception ? "exception" : "ban",
					   NET_AdrToString(&serverBans[index].ip), serverBans[index].subnet);

					SV_DelBanEntryFromList(index);

					break;
				}
			}
		}
	}
	
	SV_WriteBans();
}


/*
==================
SV_ListBans_f

List all bans and exceptions on console
==================
*/

static void SV_ListBans_f(void)
{
	int index, count;
	serverBan_t *ban;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	
	// List all bans
	for(index = count = 0; index < serverBansCount; index++)
	{
		ban = &serverBans[index];
		if(!ban->isexception)
		{
			count++;

			Com_Printf("Ban #%d: %s/%d\n", count,
				    NET_AdrToString(&ban->ip), ban->subnet);
		}
	}
	// List all exceptions
	for(index = count = 0; index < serverBansCount; index++)
	{
		ban = &serverBans[index];
		if(ban->isexception)
		{
			count++;

			Com_Printf("Except #%d: %s/%d\n", count,
				    NET_AdrToString(&ban->ip), ban->subnet);
		}
	}
}

/*
==================
SV_FlushBans_f

Delete all bans and exceptions.
==================
*/

static void SV_FlushBans_f(void)
{
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	serverBansCount = 0;
	
	// empty the ban file.
	SV_WriteBans();
	
	Com_Printf("All bans and exceptions have been deleted.\n");
}

static void SV_BanAddr_f(void)
{
	SV_AddBanToList(qfalse);
}

static void SV_ExceptAddr_f(void)
{
	SV_AddBanToList(qtrue);
}

static void SV_BanDel_f(void)
{
	SV_DelBanFromList(qfalse);
}

static void SV_ExceptDel_f(void)
{
	SV_DelBanFromList(qtrue);
}

#endif // USE_BANS

/*
==================
==================
*/
void SV_TempBanNetAddress( const netadr_t *address, int length ) {
	int i;
	int oldesttime = 0;
	int oldest = -1;

	// local and bots cannot be banned
	if ( address->type <= NA_LOOPBACK ) {
		return;
	}

	for ( i = 0; i < MAX_TEMPBAN_ADDRESSES; i++ ) {
		if ( !svs.tempBanAddresses[ i ].endtime || svs.tempBanAddresses[ i ].endtime < svs.time ) {
			// found a free slot
			svs.tempBanAddresses[ i ].adr       = *address;
			svs.tempBanAddresses[ i ].endtime   = svs.time + ( length * 1000 );

			return;
		} else {
			if ( oldest == -1 || oldesttime > svs.tempBanAddresses[ i ].endtime ) {
				oldesttime  = svs.tempBanAddresses[ i ].endtime;
				oldest      = i;
			}
		}
	}

	svs.tempBanAddresses[ oldest ].adr      = *address;
	svs.tempBanAddresses[ oldest ].endtime  = svs.time + length;
}

qboolean SV_TempBanIsBanned( const netadr_t *address ) {
	int i;

	// local and bots cannot be banned
	if ( address->type <= NA_LOOPBACK ) {
		return qfalse;
	}

	for ( i = 0; i < MAX_TEMPBAN_ADDRESSES; i++ ) {
		if ( svs.tempBanAddresses[ i ].endtime && svs.tempBanAddresses[ i ].endtime > svs.time ) {
			if ( NET_CompareAdr( address, &svs.tempBanAddresses[ i ].adr ) ) {
				return qtrue;
			}
		}
	}

	return qfalse;
}

/*
==================
SV_KickNum_f

Kick a user off of the server  FIXME: move to game
*DONE*
==================
*/
/*
static void SV_KickNum_f( void ) {
	client_t	*cl;
	int timeout = -1;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() < 2 || Cmd_Argc() > 3 ) {
		Com_Printf ("Usage: kicknum <client number> [timeout]\n");
		return;
	}

	if( Cmd_Argc() == 3 ) {
		timeout = atoi( Cmd_Argv( 2 ) );
	} else {
		timeout = 300;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	SV_DropClient( cl, "player kicked" );
	if( timeout != -1 ) {
		SV_TempBanNetAddress( cl->netchan.remoteAddress, timeout );
	}
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}
*/


/*
** SV_Strlen -- skips color escape codes
*/
int SV_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}


/*
================
SV_Status_f
================
*/
static void SV_Status_f( void ) {
	int i, j, l;
	const client_t *cl;
	const playerState_t *ps;
	const char *s;
	int max_namelength;
	int max_addrlength;
	char names[ MAX_CLIENTS * MAX_NAME_LENGTH ], *np[ MAX_CLIENTS ], nl[ MAX_CLIENTS ], *nc;
	char addrs[ MAX_CLIENTS * 48 ], *ap[ MAX_CLIENTS ], al[ MAX_CLIENTS ], *ac;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	max_namelength = 4; // strlen( "name" )
	max_addrlength = 7; // strlen( "address" )

	nc = names; *nc = '\0';
	ac = addrs; *ac = '\0';

	Com_Memset( np, 0, sizeof( np ) );
	Com_Memset( nl, 0, sizeof( nl ) );

	Com_Memset( ap, 0, sizeof( ap ) );
	Com_Memset( al, 0, sizeof( al ) );

	// first pass: save and determine max.legths of name/address fields
	for ( i = 0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++ )
	{
		if ( cl->state == CS_FREE )
			continue;

		l = strlen( cl->name ) + 1;
		strcpy( nc, cl->name );
		np[ i ] = nc; nc += l;			// name pointer in name buffer
		nl[ i ] = SV_Strlen( cl->name );// name length without color sequences
		if ( nl[ i ] > max_namelength )
			max_namelength = nl[ i ];

		s = NET_AdrToString( &cl->netchan.remoteAddress );
		l = strlen( s ) + 1;
		strcpy( ac, s );
		ap[ i ] = ac; ac += l;			// address pointer in address buffer
		al[ i ] = l - 1;				// address length
		if ( al[ i ] > max_addrlength )
			max_addrlength = al[ i ];
	}

	Com_Printf( "map: %s\n", sv_mapname->string );

#if 0
	Com_Printf ("cl score ping name            address                                 rate \n");
	Com_Printf ("-- ----- ---- --------------- --------------------------------------- -----\n");
#else // variable-length fields
	Com_Printf( "cl score ping name" );
	for ( i = 0; i < max_namelength - 4; i++ )
		Com_Printf( " " );
	Com_Printf( " address" );
	for ( i = 0; i < max_addrlength - 7; i++ )
		Com_Printf( " " );
	Com_Printf( " rate\n" );

	Com_Printf( "-- ----- ---- " );
	for ( i = 0; i < max_namelength; i++ )
		Com_Printf( "-" );
	Com_Printf( " " );
	for ( i = 0; i < max_addrlength; i++ )
		Com_Printf( "-" );
	Com_Printf( " -----\n" );
#endif

	for ( i = 0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++ )
	{
		if ( cl->state == CS_FREE )
			continue;

		Com_Printf( "%2i ", i ); // id
		ps = SV_GameClientNum( i );
		Com_Printf( "%5i ", ps->persistant[PERS_SCORE] );

		// ping/status
		if ( cl->state == CS_PRIMED )
			Com_Printf( "PRM " );
		else if ( cl->state == CS_CONNECTED )
			Com_Printf( "CON " );
		else if ( cl->state == CS_ZOMBIE )
			Com_Printf( "ZMB " );
		else
			Com_Printf( "%4i ", cl->ping < 9999 ? cl->ping : 9999 );
	
		// variable-length name field
		s = np[ i ];
		Com_Printf( "%s", s );
		l = max_namelength - nl[ i ];
		for ( j = 0; j < l; j++ )
			Com_Printf( " " );

		// variable-length address field
		s = ap[ i ];
		Com_Printf( S_COLOR_WHITE " %s", s );
		l = max_addrlength - al[ i ];
		for ( j = 0; j < l; j++ )
			Com_Printf( " " );

		// rate
		Com_Printf( " %5i\n", cl->rate );
	}

	Com_Printf( "\n" );
}


/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f( void ) {
	char	*p;
	char	text[1024];

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc () < 2 ) {
		return;
	}

	strcpy( text, "console: " );
	p = Cmd_ArgsFrom( 1 );

	if ( strlen( p ) > 1000 ) {
		return;
	}

	if ( *p == '"' ) {
		p++;
		p[strlen(p)-1] = '\0';
	}

	strcat( text, p );

	SV_SendServerCommand( NULL, "chat \"%s\"", text );
}


/*
==================
SV_ConTell_f
==================
*/
static void SV_ConTell_f( void ) {
	char	*p;
	char	text[1024];
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: tell <client number> <text>\n" );
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}

	strcpy( text, S_COLOR_MAGENTA "console: " );
	p = Cmd_ArgsFrom( 2 );

	if ( strlen( p ) > 1000 ) {
		return;
	}

	if ( *p == '"' ) {
		p++;
		p[strlen(p)-1] = '\0';
	}

	strcat( text, p );

	Com_Printf( "%s\n", text );
	SV_SendServerCommand( cl, "chat \"%s\"", text );
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f( void ) {
	svs.nextHeartbeatTime = svs.time;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	const char *info;
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	Com_Printf ("Server info settings:\n");
	info = sv.configstrings[ CS_SERVERINFO ];
	if ( info ) {
		Info_Print( info );
	}
}


/*
===========
SV_Systeminfo_f

Examine the systeminfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	const char *info;
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	Com_Printf( "System info settings:\n" );
	info = sv.configstrings[ CS_SYSTEMINFO ];
	if ( info ) {
		Info_Print( info );
	}
}


/*
===========
SV_Wolfinfo_f

Examine the wolfinfo string
===========
*/
static void SV_Wolfinfo_f( void ) {
	const char *info;
	const char *gamedir = FS_GetCurrentGameDir();

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	// WOLFINFO cvars are unused in ETF
	if ( !Q_stricmp( gamedir, "etf" ) )
		return;

	Com_Printf( "Wolf info settings:\n" );
	info = sv.configstrings[CS_WOLFINFO];
	if ( info ) {
		Info_Print( info );
	}
}


/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
static void SV_DumpUser_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: dumpuser <userid>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		return;
	}

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( cl->userinfo );
}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f( void ) {
	SV_Shutdown( "killserver" );
}


/*
=================
SV_Locations
=================
*/
static void SV_Locations_f( void ) {

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( !sv_clientTLD->integer ) {
		Com_Printf( "Disabled on this server.\n" );
		return;
	}

	SV_PrintLocations_f( NULL );
}


/*
=================
SV_GameCompleteStatus_f

NERVE - SMF
=================
*/
void SV_GameCompleteStatus_f( void ) {
	SV_MasterGameCompleteStatus();
}

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
void SV_CompleteMapName( char *args, int argNum ) {
	if ( argNum == 2 ) 	{
		if ( sv_pure->integer ) {
			Field_CompleteFilename( "maps", "bsp", qtrue, FS_MATCH_PK3s | FS_MATCH_STICK );
		} else {
			Field_CompleteFilename( "maps", "bsp", qtrue, FS_MATCH_ANY | FS_MATCH_STICK );
		}
	}
}


#ifdef USE_MV

#define MV_CACHE_FILE "demos/mv-cache.dat"
#define MV_FILTER "/mv-*-*.dm_85"

int mv_record_count;
int mv_total_size;
int mv_insert_index;

typedef struct {
	char name[28];
	int  size;
} mv_file_record_t;
mv_file_record_t mvrecords[ MAX_MV_FILES ];


static void SV_TrimRecords( int add_count, int add_size );

static void SV_CreateRecordCache( void )
{
	mv_file_record_t *mvr;
	int nfiles, i, len, size;
	char **list, *name;

	//Com_Printf( S_COLOR_CYAN "...creating record cache\n" );

	mv_insert_index = mv_record_count;
	mv_record_count = 0;
	mv_total_size = 0;

	Com_Memset( mvrecords, 0, sizeof( mvrecords ) );
	list = FS_Home_ListFilteredFiles( "demos", ".dm_85", MV_FILTER, &nfiles );
	for ( i = 0; i < nfiles; i++ ) 
	{
		name = list[i];
		if ( name[0] == '\\' || name[0] == '/' )
			name++;
		len = (int)strlen( list[i] );
		if ( len < 22 || len >= sizeof( mvr->name ) )
			continue;

		size = FS_Home_FileSize( va( "demos/%s", name ) );
		if ( size <= 0 )
			continue;

		mvr = &mvrecords[ mv_record_count ];
		mvr->size = PAD( size, 4096 );
		strcpy( mvr->name, name );
			
		mv_total_size += mvr->size;

		mv_record_count++;

		if ( mv_record_count >= MAX_MV_FILES )
			break;
	}
	FS_FreeFileList( list );

	mv_insert_index = mv_record_count;
	mv_insert_index &= (MAX_MV_FILES-1);
}


/*
==================
SV_LoadRecordCache
==================
*/
void SV_LoadRecordCache( void ) 
{
	mv_file_record_t *mvr;
	fileHandle_t fh;
	int fileSize, i;

	mv_record_count = 0;
	mv_insert_index = 0;
	mv_total_size = 0;

	fileSize = FS_Home_FOpenFileRead( MV_CACHE_FILE, &fh );
	if ( fh == FS_INVALID_HANDLE )
	{
		SV_CreateRecordCache();
		SV_TrimRecords( 0, 0 );
		return;
	}

	if ( fileSize != sizeof( mvrecords ) )
	{
		FS_FCloseFile( fh );
		SV_CreateRecordCache();
		SV_TrimRecords( 0, 0 );
		return;
	}

	//Com_Printf( S_COLOR_CYAN "...reading record cache from file\n" );
	FS_Read( mvrecords, sizeof( mvrecords ), fh );
	FS_FCloseFile( fh );

	mvr = mvrecords;
	for ( i = 0; i < MAX_MV_FILES; i++, mvr++ )
	{
		if ( !mvr->name[0] || mvr->size <= 0 )
			break;
		mv_total_size += PAD( mvr->size, 4096 );
	}

	mv_record_count = i;
	mv_insert_index = i & (MAX_MV_FILES-1);

	SV_TrimRecords( 0, 0 );

	//Com_Printf( S_COLOR_CYAN "cache: %i items, %i bytes\n", mv_record_count, mv_total_size );
}


void SV_SaveRecordCache( void )
{
	mv_file_record_t z;
	fileHandle_t fh;
	int start;
	int n, count, pad;

	fh = FS_FOpenFileWrite( MV_CACHE_FILE );
	if ( fh == FS_INVALID_HANDLE ) 
		return;

	count = mv_record_count;
	if ( count > MAX_MV_FILES )
		count = MAX_MV_FILES;

	pad = MAX_MV_FILES - count;
	Com_Memset( &z, 0, sizeof( z ) );

	start = ( mv_insert_index - count ) & (MAX_MV_FILES-1);
	//Com_Printf( S_COLOR_CYAN "writing %i cache records from %i\n", count, start );

	while ( count > 0 )
	{
		n = count;
		if ( start + n > MAX_MV_FILES )
			n = MAX_MV_FILES - start;

		FS_Write( &mvrecords[ start ], sizeof( mv_file_record_t ) * n, fh );
		start = ( start + n ) & (MAX_MV_FILES-1);
		count -= n;
	}

	for ( n = 0; n < pad; n++ )
	{
		FS_Write( &z, sizeof( z ), fh );
	}

	FS_FCloseFile( fh );
}


static void SV_TrimRecords( int add_count, int add_size )
{
	int max_count;
	mv_file_record_t *mvr;

	//Com_Printf( S_COLOR_YELLOW "trim records count:%i size%i\n", mv_record_count, mv_total_size );

	// by file count
	if (  sv_mvFileCount->integer > 0 || mv_record_count + add_count > MAX_MV_FILES )
	{
		if ( sv_mvFileCount->integer > 0 && sv_mvFileCount->integer < MAX_MV_FILES )
			max_count = sv_mvFileCount->integer;
		else
			max_count = MAX_MV_FILES;

		while ( mv_record_count + add_count > max_count && mv_record_count > 0 )
		{
				mvr = mvrecords + (( mv_insert_index - mv_record_count ) & (MAX_MV_FILES-1));
				//Com_Printf( S_COLOR_RED "trim.count %i %s\n", mvr->size, mvr->name );
				if ( mvr->name[0] ) 
				{
					FS_HomeRemove( va( "demos/%s", mvr->name ) );
					mv_total_size -= mvr->size;
				}
				Com_Memset( mvr, 0, sizeof( *mvr ) );
				mv_record_count--;
		}
	}

	// by total size
	if ( sv_mvFolderSize->integer > 0 )
	{
		while ( (mv_total_size + add_size) > (sv_mvFolderSize->integer * 1024 * 1024) && mv_record_count > 0 ) 
		{
				mvr = mvrecords + (( mv_insert_index - mv_record_count ) & (MAX_MV_FILES-1));
				//Com_Printf( S_COLOR_RED "trim.size %i %s\n", mvr->size, mvr->name );
				if ( mvr->name[0] ) 
				{
					FS_HomeRemove( va( "demos/%s", mvr->name ) );
					mv_total_size -= mvr->size;
				}
				Com_Memset( mvr, 0, sizeof( *mvr ) );
				mv_record_count--;
		}
	}
}


static void SV_InsertFileRecord( const char *name )
{
	mv_file_record_t *mvr;
	int size, len;

	if ( !Q_stricmpn( name, "demos/", 6 ) )
		name += 5;
	else
		return;

	if ( !Com_FilterPath( MV_FILTER, name ) ) {
		//Com_Printf( "filtered %s\n", name );
		return;
	}
	name++; // skip '/'

	len = strlen( name );
	if ( len < 22 || len >= sizeof( mvr->name ) ) {
		//Com_Printf( "filtered1 %s\n", name );
		return;
	}

	size = FS_Home_FileSize( va( "demos/%s", name ) );
	if ( size <= 0 ) {
		return;
	}

	size = PAD( size, 4096 );

	SV_TrimRecords( 1, size );

	mvr = &mvrecords[ mv_insert_index ];
	strcpy( mvr->name, name );
	mvr->size = size;

	mv_total_size += mvr->size;
	mv_insert_index = ( mv_insert_index + 1 ) & (MAX_MV_FILES-1);

	if ( mv_record_count < MAX_MV_FILES )
		mv_record_count++;

	//Com_Printf( S_COLOR_CYAN "Record index %i, count %i\n", mv_insert_index, mv_record_count );
	//SV_SaveRecordCache();
}


/*
==================
SV_SetTargetClient
==================
*/
void SV_SetTargetClient( int clientNum )
{
	sv_lastAck = 0; // force to fetch latest target' reliable acknowledge
	sv_lastClientSeq = 0;
	sv_demoClientID = clientNum;
}


/*
==================
SV_ForwardServerCommands
==================
*/
void SV_ForwardServerCommands( client_t *recorder /*, const client_t *client */ )
{
	const client_t *client;
	const char *cmd;
	int src_index;
	int	dst_index;
	int i;

	if ( sv_demoClientID < 0 )
		return;

	client = svs.clients + sv_demoClientID;

	// FIXME: track reliableSequence globally?
	if ( !sv_lastAck ) {
		sv_lastAck = client->reliableAcknowledge;
	}

	//if ( client->reliableAcknowledge >= client->reliableSequence )
	if ( sv_lastAck >= client->reliableSequence )
		return; // nothing to send

	//for ( i = client->reliableAcknowledge + 1 ; i <= client->reliableSequence ; i++ ) {
	for ( i = sv_lastAck + 1 ; i <= client->reliableSequence ; i++ ) {
		src_index = i & ( MAX_RELIABLE_COMMANDS - 1 );
		cmd = client->reliableCommands[ src_index ];
		// filter commands here:
		if ( strncmp( cmd, "tell ", 5 ) == 0 ) // TODO: other commands
			continue;
		dst_index = ++recorder->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
		Q_strncpyz( recorder->reliableCommands[ dst_index ], cmd, sizeof( recorder->reliableCommands[ dst_index ] ) );
	}

	sv_lastAck = client->reliableSequence;
}


/*
==================
SV_MultiViewRecord_f
==================
*/
static void SV_MultiViewRecord_f( void )
{
	entityState_t *base, nullstate;

	client_t	*recorder;	// recorder slot
	byte		msgData[ MAX_MSGLEN_BUF ];
	msg_t		msg;

	char demoName[ MAX_QPATH ];
	char name[ MAX_QPATH ];
	const char *s;
	int i, cid, len;

	if ( Cmd_Argc() > 2 ) {
		Com_Printf( "usage: mvrecord [filename]\n" );
		return;
	}

	if ( sv_demoFile != FS_INVALID_HANDLE ) {
		Com_Printf( "Already recording multiview.\n" );
		return;
	}	

	if ( sv.state != SS_GAME || !svs.clients ) {
		Com_Printf( "Game is not running.\n" );
		return;
	}

	cid = SV_FindActiveClient( qfalse /* checkCommands */, -1 /* skipClientNum */, 0 /* minActive */ );

	if ( cid < 0 ) {
		Com_Printf( "No active clients connected.\n" );
		return;
	}

	if ( Cmd_Argc() == 2 ) {
		s = Cmd_Argv( 1 );
		Q_strncpyz( demoName, s, sizeof( demoName ) );
		Com_sprintf( name, sizeof( name ), "demos/%s.%s%d", demoName, DEMOEXT, NEW_PROTOCOL_VERSION );
	} else {
		qtime_t t;
		
		Com_RealTime( &t );
		// name in format mv-YYMMDD-HHmmSS.dm_85 (23+4) = 27, ok
		sprintf( demoName, "mv-%02i%02i%02i-%02i%02i%02i", t.tm_year-100, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec );
		Com_sprintf( name, sizeof( name ), "demos/%s.%s%d", demoName, DEMOEXT, NEW_PROTOCOL_VERSION );
				
		i = 0;
		// try with suffix: mv-YYMMDD-HHmmSS-[0..999].dm_85
		while ( FS_FileExists( name ) && i++ <= 999 )
			Com_sprintf( name, sizeof( name ), "demos/%s-%i.%s%d", demoName, i, DEMOEXT, NEW_PROTOCOL_VERSION );
	}

	strcpy( sv_demoFileNameLast, name );
	strcpy( sv_demoFileName, name );
	// add .tmp to server-side demos so we can rename them later
	Q_strcat( sv_demoFileName, sizeof( sv_demoFileName ), ".tmp" );

	Com_Printf( S_COLOR_CYAN "start recording to %s using primary client id %i.\n", sv_demoFileName, cid );
	sv_demoFile = FS_FOpenFileWrite( sv_demoFileName );

	if ( sv_demoFile == FS_INVALID_HANDLE ) {
		Com_Printf( "ERROR: couldn't open %s.\n", sv_demoFileName );
		sv_demoFileName[0] = '\0';
		sv_demoFileNameLast[0] = '\0';
		return;
	}

	recorder = svs.clients + sv_maxclients->integer; // reserved recorder slot

	SV_SetTargetClient( cid );

	Com_Memset( recorder, 0, sizeof( *recorder ) );

	recorder->multiview.protocol = MV_PROTOCOL_VERSION;
	recorder->multiview.recorder = qtrue;
	recorder->state = CS_ACTIVE;

	recorder->deltaMessage = -1; // reset delta encoding in next snapshot
	recorder->netchan.outgoingSequence = 1;
	recorder->netchan.remoteAddress.type = NA_LOOPBACK;

	// empty command buffer
	recorder->reliableSequence = 0;
	recorder->reliableAcknowledge = 0;

	recorder->lastClientCommand = 1;

	MSG_Init( &msg, msgData, MAX_MSGLEN );
	MSG_Bitstream( &msg );

	// NOTE, MRE: all server->client messages now acknowledge
	MSG_WriteLong( &msg, recorder->lastClientCommand );

	SV_UpdateServerCommandsToClient( recorder, &msg );	

#ifdef USE_MV_ZCMD
	// we are resetting delta sequence after gamestate
	recorder->multiview.z.deltaSeq = 0;
#endif

	MSG_WriteByte( &msg, svc_gamestate );

	// all future zcmds must have reliableSequence greater than this
	MSG_WriteLong( &msg, recorder->reliableSequence );

	// write the configstrings
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( sv.configstrings[i][0] ) {
			MSG_WriteByte( &msg, svc_configstring );
			MSG_WriteShort( &msg, i );
			MSG_WriteBigString( &msg, sv.configstrings[i] );
		}
	}

	// write the baselines
	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	for ( i = 0 ; i < MAX_GENTITIES; i++ ) {
		base = &sv.svEntities[ i ].baseline;
		if ( !sv.baselineUsed[ i ] ) {
			continue;
		}
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, base, qtrue );
	}

	MSG_WriteByte( &msg, svc_EOF );

	MSG_WriteLong( &msg, sv_demoClientID ); // selected client id

	// write the checksum feed
	MSG_WriteLong( &msg, sv.checksumFeed );

	// finalize packet
	MSG_WriteByte( &msg, svc_EOF );

	len = LittleLong( recorder->netchan.outgoingSequence - 1 );
	FS_Write( &len, 4, sv_demoFile );

	// data size
	len = LittleLong( msg.cursize );
	FS_Write( &len, 4, sv_demoFile );

	// data
	FS_Write( msg.data, msg.cursize, sv_demoFile );
}


/*
==================
SV_MultiViewStopRecord_f
==================
*/
void SV_MultiViewStopRecord_f( void )
{
	client_t *recorder;

	if ( !svs.clients )
		return;

	recorder = svs.clients + sv_maxclients->integer; // recorder slot

	if ( sv_demoFile != FS_INVALID_HANDLE ) {

		FS_FCloseFile( sv_demoFile );
		sv_demoFile = FS_INVALID_HANDLE;

		// rename final file
		if ( sv_demoFileNameLast[0] && sv_demoFileName[0] ) {
			FS_Rename( sv_demoFileName, sv_demoFileNameLast );
		}
		
		// store in cache
		SV_InsertFileRecord( sv_demoFileNameLast );

		Com_Printf( S_COLOR_CYAN "stopped multiview recording.\n" );
		return;
	}

	sv_demoFileName[0] = '\0';
	sv_demoFileNameLast[0] = '\0';

	SV_SetTargetClient( -1 );
#if 1
	Com_Memset( recorder, 0, sizeof( *recorder ) );
#else
	recorder->netchan.outgoingSequence = 0;
	recorder->multiview.protocol = 0;
	recorder->multiview.recorder = qfalse;
	recorder->state = CS_FREE;
#endif
}


/*
==================
SV_TrackDisconnect
==================
*/
void SV_TrackDisconnect( int clientNum ) 
{
	int cid;

	svs.clients[ clientNum ].multiview.scoreQueryTime = 0;

	if ( clientNum == sv_demoClientID ) {
		cid = SV_FindActiveClient( qfalse, sv_demoClientID, 0 ); // TODO: count sv_autoRecord?
		if ( cid < 0 ) {
			SV_MultiViewStopRecord_f();
			return;			
		}
		Com_DPrintf( "mvrecorder: switch primary client id to %i\n", cid );
		SV_SetTargetClient( cid );
	}
}
#endif // USE_MV


/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {
	static qboolean	initialized;

	if ( initialized ) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand( "heartbeat", SV_Heartbeat_f );
// fretn - moved to qagame
	/*Cmd_AddCommand ("kick", SV_Kick_f);
	Cmd_AddCommand ("clientkick", SV_KickNum_f);*/
	Cmd_AddCommand( "status", SV_Status_f );
	Cmd_AddCommand( "dumpuser", SV_DumpUser_f );
	Cmd_AddCommand( "map_restart", SV_MapRestart_f );
#ifdef MSG_FIELDINFO_SORT_DEBUG
	Cmd_AddCommand( "fieldinfo", SV_FieldInfo_f );
#endif
	Cmd_AddCommand( "sectorlist", SV_SectorList_f );
	Cmd_AddCommand( "map", SV_Map_f );
	Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
	Cmd_AddCommand( "gameCompleteStatus", SV_GameCompleteStatus_f );      // NERVE - SMF
	Cmd_AddCommand( "devmap", SV_Map_f );
	Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
	Cmd_AddCommand( "killserver", SV_KillServer_f );
#ifdef USE_BANS	
	Cmd_AddCommand("rehashbans", SV_RehashBans_f);
	Cmd_AddCommand("listbans", SV_ListBans_f);
	Cmd_AddCommand("banaddr", SV_BanAddr_f);
	Cmd_AddCommand("exceptaddr", SV_ExceptAddr_f);
	Cmd_AddCommand("bandel", SV_BanDel_f);
	Cmd_AddCommand("exceptdel", SV_ExceptDel_f);
	Cmd_AddCommand("flushbans", SV_FlushBans_f);
#endif
	Cmd_AddCommand( "filter", SV_AddFilter_f );
	Cmd_AddCommand( "filtercmd", SV_AddFilterCmd_f );
#ifdef USE_MV
	Cmd_AddCommand( "mvrecord", SV_MultiViewRecord_f );
	Cmd_AddCommand( "mvstoprecord", SV_MultiViewStopRecord_f );
#endif
}


/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand( "heartbeat" );
	Cmd_RemoveCommand( "kick" );
	Cmd_RemoveCommand( "banUser" );
	Cmd_RemoveCommand( "banClient" );
	Cmd_RemoveCommand( "status" );
	Cmd_RemoveCommand( "dumpuser" );
	Cmd_RemoveCommand( "map_restart" );
	Cmd_RemoveCommand( "sectorlist" );
#endif
}


void SV_AddDedicatedCommands( void )
{
	Cmd_AddCommand( "serverinfo", SV_Serverinfo_f );
	Cmd_AddCommand( "systeminfo", SV_Systeminfo_f );
	Cmd_AddCommand( "wolfinfo", SV_Wolfinfo_f );
	Cmd_AddCommand( "tell", SV_ConTell_f );
	Cmd_AddCommand( "say", SV_ConSay_f );
	Cmd_AddCommand( "locations", SV_Locations_f );
}


void SV_RemoveDedicatedCommands( void )
{
	Cmd_RemoveCommand( "serverinfo" );
	Cmd_RemoveCommand( "systeminfo" );
	Cmd_RemoveCommand( "wolfinfo" );
	Cmd_RemoveCommand( "tell" );
	Cmd_RemoveCommand( "say" );
	Cmd_RemoveCommand( "locations" );
}
