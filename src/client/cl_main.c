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

// cl_main.c  -- client main loop

#include "client.h"

cvar_t  *cl_noprint;
cvar_t  *cl_debugMove;
cvar_t  *cl_motd;

#ifdef USE_RENDERER_DLOPEN
cvar_t	*cl_renderer;
#endif

cvar_t	*rcon_client_password;
cvar_t	*rconAddress;

cvar_t	*cl_timeout;
cvar_t	*cl_autoNudge;
cvar_t	*cl_timeNudge;
cvar_t	*cl_showTimeDelta;

cvar_t	*cl_shownet = NULL;     // NERVE - SMF - This is referenced in msg.c and we need to make sure it is NULL
cvar_t	*cl_shownuments;        // DHM - Nerve
cvar_t	*cl_showServerCommands; // NERVE - SMF
cvar_t	*cl_autoRecordDemo;

cvar_t	*cl_avidemo = NULL; // for etmain demo ui compatibility
cvar_t	*cl_aviFrameRate;
cvar_t	*cl_aviMotionJpeg;
cvar_t	*cl_forceavidemo = NULL;
cvar_t	*cl_aviPipeFormat;

cvar_t	*cl_activeAction;

cvar_t	*cl_motdString;

cvar_t	*cl_allowDownload;
cvar_t	*cl_wwwDownload;
#ifdef USE_CURL
cvar_t	*cl_mapAutoDownload;
#endif
cvar_t	*cl_conXOffset;
cvar_t	*cl_conColor;
cvar_t	*cl_inGameVideo;

cvar_t	*cl_serverStatusResendTime;

// NERVE - SMF - localization
cvar_t	*cl_language;
cvar_t	*cl_debugTranslation;
// -NERVE - SMF

cvar_t  *cl_profile;
cvar_t  *cl_defaultProfile;

static cvar_t	*cl_demorecording; // fretn
static cvar_t	*cl_demofilename; // bani
static cvar_t	*cl_demooffset; // bani
static cvar_t	*cl_silentRecord;

cvar_t  *cl_waverecording; //bani
cvar_t  *cl_wavefilename; //bani
cvar_t  *cl_waveoffset; //bani

cvar_t	*cl_lanForcePackets;

//cvar_t	*cl_guidServerUniq;

cvar_t	*cl_dlURL;
cvar_t	*cl_dlDirectory;

cvar_t	*cl_reconnectArgs;

// common cvars for GLimp modules
cvar_t	*vid_xpos;			// X coordinate of window position
cvar_t	*vid_ypos;			// Y coordinate of window position
cvar_t	*r_noborder;

cvar_t *r_allowSoftwareGL;	// don't abort out if the pixelformat claims software
cvar_t *r_swapInterval;
#ifndef USE_SDL
cvar_t *r_glDriver;
#ifdef _WIN32
cvar_t *r_allowScreenSaver;
#endif
#else
cvar_t *r_sdlDriver;
cvar_t *r_allowScreenSaver;
#endif
cvar_t *r_displayRefresh;
cvar_t *r_fullscreen;
cvar_t *r_mode;
cvar_t *r_modeFullscreen;
//cvar_t *r_oldMode;
cvar_t *r_customwidth;
cvar_t *r_customheight;
cvar_t *r_customaspect;

cvar_t *r_colorbits;
// these also shared with renderers:
cvar_t *cl_stencilbits;
cvar_t *cl_depthbits;
cvar_t *cl_drawBuffer;

#ifndef USE_SDL
// this is shared with the OS sys files
cvar_t *in_forceCharset;
#endif

#ifdef USE_DISCORD
cvar_t	*cl_discordRichPresence;
#endif

clientActive_t		cl;
clientConnection_t	clc;
clientStatic_t		cls;
vm_t				*cgvm = NULL;

netadr_t			rcon_address;

char				cl_oldGame[ MAX_QPATH ];
qboolean			cl_oldGameSet;
static	qboolean	noGameRestart = qfalse;
static	qboolean	cl_shutdownQuit = qfalse;

#ifdef USE_CURL
download_t			download;
#endif

// Structure containing functions exported from refresh DLL
refexport_t	re;
#ifdef USE_RENDERER_DLOPEN
static void	*rendererLib;
#endif

static ping_t cl_pinglist[MAX_PINGREQUESTS];

typedef struct serverStatus_s
{
	char string[BIG_INFO_STRING];
	netadr_t address;
	int time, startTime;
	qboolean pending;
	qboolean print;
	qboolean retrieved;
} serverStatus_t;

static serverStatus_t cl_serverStatusList[MAX_SERVERSTATUSREQUESTS];

static void CL_CheckForResend( void );
static void CL_ShowIP_f( void );
static void CL_ServerStatus_f( void );
static void CL_ServerStatusResponse( const netadr_t *from, msg_t *msg );
static void CL_ServerInfoPacket( const netadr_t *from, msg_t *msg );

#ifdef USE_CURL
static void CL_Download_f( void );
#endif
static void CL_LocalServers_f( void );
static void CL_GlobalServers_f( void );
static void CL_Ping_f( void );

static void CL_InitRef( void );
static void CL_ShutdownRef( refShutdownCode_t code );
static void CL_InitGLimp_Cvars( void );

static void CL_NextDemo( void );

// fretn
//void CL_WriteWaveClose( void );
//void CL_WavStopRecord_f( void );

/*
===============
CL_CDDialog

Called by Com_Error when a cd is needed
===============
*/
void CL_CDDialog( void ) {
	cls.cddialog = qtrue;	// start it next frame
}

void CL_PurgeCache( void ) {
	cls.doCachePurge = qtrue;
}

static void CL_DoPurgeCache( void ) {
	if ( !cls.doCachePurge ) {
		return;
	}

	cls.doCachePurge = qfalse;

	if ( !com_cl_running ) {
		return;
	}

	if ( !com_cl_running->integer ) {
		return;
	}

	if ( !cls.rendererStarted ) {
		return;
	}

	re.purgeCache();
}

/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/

/*
======================
CL_AddReliableCommand

The given command will be transmitted to the server, and is gauranteed to
not have future usercmd_t executed before it is executed
======================
*/
void CL_AddReliableCommand( const char *cmd, qboolean isDisconnectCmd ) {
	int		index;
	int		unacknowledged = clc.reliableSequence - clc.reliableAcknowledge;

	if ( clc.serverAddress.type == NA_BAD )
		return;

	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	// also leave one slot open for the disconnect command in this case.

	if ((isDisconnectCmd && unacknowledged > MAX_RELIABLE_COMMANDS) ||
		(!isDisconnectCmd && unacknowledged >= MAX_RELIABLE_COMMANDS))
	{
		if( com_errorEntered )
			return;
		else
			Com_Error(ERR_DROP, "Client command overflow");
	}

	clc.reliableSequence++;
	index = clc.reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( clc.reliableCommands[ index ], cmd, sizeof( clc.reliableCommands[ index ] ) );
}


/*
=======================================================================

CLIENT SIDE DEMO RECORDING

=======================================================================
*/

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
static void CL_WriteDemoMessage( msg_t *msg, int headerBytes ) {
	int		len, swlen;

	// write the packet sequence
	len = clc.serverMessageSequence;
	swlen = LittleLong( len );
	FS_Write( &swlen, 4, clc.recordfile );

	// skip the packet sequencing information
	len = msg->cursize - headerBytes;
	swlen = LittleLong(len);
	FS_Write( &swlen, 4, clc.recordfile );
	FS_Write( msg->data + headerBytes, len, clc.recordfile );
}


/*
====================
CL_StopRecording_f

stop recording a demo
====================
*/
void CL_StopRecord_f( void ) {

	if ( clc.recordfile != FS_INVALID_HANDLE ) {
		char tempName[MAX_OSPATH];
		char finalName[MAX_OSPATH];
		int protocol;
		int	len, sequence;

		// finish up
		len = -1;
		FS_Write( &len, 4, clc.recordfile );
		FS_Write( &len, 4, clc.recordfile );
		FS_FCloseFile( clc.recordfile );
		clc.recordfile = FS_INVALID_HANDLE;

		// select proper extension
		if ( clc.dm84compat || clc.demoplaying ) {
			protocol = OLD_PROTOCOL_VERSION;
		} else {
			protocol = NEW_PROTOCOL_VERSION;
		}

		if ( com_protocol->integer != DEFAULT_PROTOCOL_VERSION ) {
			protocol = com_protocol->integer;
		}

		Com_sprintf( tempName, sizeof( tempName ), "%s.tmp", clc.recordName );

		Com_sprintf( finalName, sizeof( finalName ), "%s.%s%d", clc.recordName, DEMOEXT, protocol );

		if ( clc.explicitRecordName ) {
			FS_Remove( finalName );
		} else {
			// add sequence suffix to avoid overwrite
			sequence = 0;
			while ( FS_FileExists( finalName ) && ++sequence < 1000 ) {
				Com_sprintf( finalName, sizeof( finalName ), "%s-%02d.%s%d",
					clc.recordName, sequence, DEMOEXT, protocol );
			}
		}

		FS_Rename( tempName, finalName );
	}

	if ( !clc.demorecording ) {
		Com_Printf( "Not recording a demo.\n" );
	} else if ( !cl_silentRecord->integer ) {
		Com_Printf( "Stopped demo recording.\n" );
	}

	clc.demorecording = qfalse;
	Cvar_Set( "cl_demofilename", "" ); // bani
	Cvar_Set( "cl_demooffset", "0" ); // bani
	Cvar_Set( "cl_demorecording", "0" ); // fretn
}


/*
====================
CL_WriteServerCommands
====================
*/
static void CL_WriteServerCommands( msg_t *msg ) {
	int i;

	if ( clc.serverCommandSequence - clc.demoCommandSequence > 0 ) {

		// do not write more than MAX_RELIABLE_COMMANDS
		if ( clc.serverCommandSequence - clc.demoCommandSequence > MAX_RELIABLE_COMMANDS ) {
			clc.demoCommandSequence = clc.serverCommandSequence - MAX_RELIABLE_COMMANDS;
		}

		for ( i = clc.demoCommandSequence + 1 ; i <= clc.serverCommandSequence; i++ ) {
			MSG_WriteByte( msg, svc_serverCommand );
			MSG_WriteLong( msg, i );
			MSG_WriteString( msg, clc.serverCommands[ i & (MAX_RELIABLE_COMMANDS-1) ] );
		}
	}

	clc.demoCommandSequence = clc.serverCommandSequence;
}


/*
====================
CL_WriteGamestate
====================
*/
static void CL_WriteGamestate( qboolean initial )
{
	byte		bufData[ MAX_MSGLEN_BUF ];
	char		*s;
	msg_t		msg;
	int			i;
	int			len;
	entityState_t	*ent;
	entityState_t	nullstate;

	// write out the gamestate message
	MSG_Init( &msg, bufData, MAX_MSGLEN );
	MSG_Bitstream( &msg );

	// NOTE, MRE: all server->client messages now acknowledge
	MSG_WriteLong( &msg, clc.reliableSequence );

	if ( initial ) {
		clc.demoMessageSequence = 1;
		clc.demoCommandSequence = clc.serverCommandSequence;
	} else {
		CL_WriteServerCommands( &msg );
	}

	clc.demoDeltaNum = 0; // reset delta for next snapshot

	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, clc.serverCommandSequence );

	// configstrings
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( !cl.gameState.stringOffsets[i] ) {
			continue;
		}
		s = cl.gameState.stringData + cl.gameState.stringOffsets[i];
		MSG_WriteByte( &msg, svc_configstring );
		MSG_WriteShort( &msg, i );
		MSG_WriteBigString( &msg, s );
	}

	// baselines
	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	for ( i = 0; i < MAX_GENTITIES ; i++ ) {
		if ( !cl.baselineUsed[ i ] )
			continue;
		ent = &cl.entityBaselines[ i ];
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, ent, qtrue );
	}

	// finalize message
	MSG_WriteByte( &msg, svc_EOF );

	// finished writing the gamestate stuff

	// write the client num
	MSG_WriteLong( &msg, clc.clientNum );

	// write the checksum feed
	MSG_WriteLong( &msg, clc.checksumFeed );

	// finished writing the client packet
	MSG_WriteByte( &msg, svc_EOF );

	// write it to the demo file
	if ( clc.demoplaying )
		len = LittleLong( clc.demoMessageSequence - 1 );
	else
		len = LittleLong( clc.serverMessageSequence - 1 );

	FS_Write( &len, 4, clc.recordfile );

	len = LittleLong( msg.cursize );
	FS_Write( &len, 4, clc.recordfile );
	FS_Write( msg.data, msg.cursize, clc.recordfile );
}


/*
=============
CL_EmitPacketEntities
=============
*/
static void CL_EmitPacketEntities( clSnapshot_t *from, clSnapshot_t *to, msg_t *msg, entityState_t *oldents ) {
	entityState_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;

	// generate the delta update
	if ( !from ) {
		from_num_entities = 0;
	} else {
		from_num_entities = from->numEntities;
	}

	newent = NULL;
	oldent = NULL;
	newindex = 0;
	oldindex = 0;
	while ( newindex < to->numEntities || oldindex < from_num_entities ) {
		if ( newindex >= to->numEntities ) {
			newnum = MAX_GENTITIES+1;
		} else {
			newent = &cl.parseEntities[(to->parseEntitiesNum + newindex) % MAX_PARSE_ENTITIES];
			newnum = newent->number;
		}

		if ( oldindex >= from_num_entities ) {
			oldnum = MAX_GENTITIES+1;
		} else {
			//oldent = &cl.parseEntities[(from->parseEntitiesNum + oldindex) % MAX_PARSE_ENTITIES];
			oldent = &oldents[ oldindex ];
			oldnum = oldent->number;
		}

		if ( newnum == oldnum ) {
			// delta update from old position
			// because the force parm is qfalse, this will not result
			// in any bytes being emitted if the entity has not changed at all
			MSG_WriteDeltaEntity (msg, oldent, newent, qfalse );
			oldindex++;
			newindex++;
			continue;
		}

		if ( newnum < oldnum ) {
			// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity (msg, &cl.entityBaselines[newnum], newent, qtrue );
			newindex++;
			continue;
		}

		if ( newnum > oldnum ) {
			// the old entity isn't present in the new message
			MSG_WriteDeltaEntity (msg, oldent, NULL, qtrue );
			oldindex++;
			continue;
		}
	}

	MSG_WriteBits( msg, (MAX_GENTITIES-1), GENTITYNUM_BITS );	// end of packetentities
}


/*
====================
CL_WriteSnapshot
====================
*/
static void CL_WriteSnapshot( void ) {

	static	clSnapshot_t saved_snap;
	static entityState_t saved_ents[ MAX_SNAPSHOT_ENTITIES ];

	clSnapshot_t *snap, *oldSnap;
	byte	bufData[ MAX_MSGLEN_BUF ];
	msg_t	msg;
	int		i, len;

	snap = &cl.snapshots[ cl.snap.messageNum & PACKET_MASK ]; // current snapshot
	//if ( !snap->valid ) // should never happen?
	//	return;

	if ( clc.demoDeltaNum == 0 ) {
		oldSnap = NULL;
	} else {
		oldSnap = &saved_snap;
	}

	MSG_Init( &msg, bufData, MAX_MSGLEN );
	MSG_Bitstream( &msg );

	// NOTE, MRE: all server->client messages now acknowledge
	MSG_WriteLong( &msg, clc.reliableSequence );

	// Write all pending server commands
	CL_WriteServerCommands( &msg );

	MSG_WriteByte( &msg, svc_snapshot );
	MSG_WriteLong( &msg, snap->serverTime ); // sv.time
	MSG_WriteByte( &msg, clc.demoDeltaNum ); // 0 or 1
	MSG_WriteByte( &msg, snap->snapFlags );  // snapFlags
	MSG_WriteByte( &msg, snap->areabytes );  // areabytes
	MSG_WriteData( &msg, snap->areamask, snap->areabytes );
	if ( oldSnap )
		MSG_WriteDeltaPlayerstate( &msg, &oldSnap->ps, &snap->ps );
	else
		MSG_WriteDeltaPlayerstate( &msg, NULL, &snap->ps );

	CL_EmitPacketEntities( oldSnap, snap, &msg, saved_ents );

	// finished writing the client packet
	MSG_WriteByte( &msg, svc_EOF );

	// write it to the demo file
	if ( clc.demoplaying )
		len = LittleLong( clc.demoMessageSequence );
	else
		len = LittleLong( clc.serverMessageSequence );
	FS_Write( &len, 4, clc.recordfile );

	len = LittleLong( msg.cursize );
	FS_Write( &len, 4, clc.recordfile );
	FS_Write( msg.data, msg.cursize, clc.recordfile );

	// save last sent state so if there any need - we can skip any further incoming messages
	for ( i = 0; i < snap->numEntities; i++ )
		saved_ents[ i ] = cl.parseEntities[ (snap->parseEntitiesNum + i) % MAX_PARSE_ENTITIES ];

	saved_snap = *snap;
	saved_snap.parseEntitiesNum = 0;

	clc.demoMessageSequence++;
	clc.demoDeltaNum = 1;
}


/*
====================
CL_Record_f

record <demoname>

Begins recording a demo from the current position
====================
*/
static void CL_Record_f( void ) {
	char		demoName[MAX_OSPATH];
	char		name[MAX_OSPATH];
	char		demoExt[16];
	const char	*ext;
	qtime_t		t;

	if ( Cmd_Argc() > 2 ) {
		Com_Printf( "record <demoname>\n" );
		return;
	}

	if ( clc.demorecording ) {
		Com_Printf( "Already recording.\n" );
		return;
	}

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf( "You must be in a level to record.\n" );
		return;
	}

	// ATVI Wolfenstein Misc #479 - changing this to a warning
	// sync 0 doesn't prevent recording, so not forcing it off .. everyone does g_sync 1 ; record ; g_sync 0 ..
	//if ( NET_IsLocalAddress( &clc.serverAddress ) && !Cvar_VariableIntegerValue( "g_synchronousClients" ) ) {
	//	Com_Printf (S_COLOR_YELLOW "WARNING: You should set 'g_synchronousClients 1' for smoother demo recording\n");
	//}

	if ( Cmd_Argc() == 2 ) {
		// explicit demo name specified
		Q_strncpyz( demoName, Cmd_Argv( 1 ), sizeof( demoName ) );
		ext = COM_GetExtension( demoName );
		if ( *ext ) {
			// strip demo extension
			sprintf( demoExt, "%s%d", DEMOEXT, OLD_PROTOCOL_VERSION );
			if ( Q_stricmp( ext, demoExt ) == 0 ) {
				*(strrchr( demoName, '.' )) = '\0';
			} else {
				// check both protocols
				sprintf( demoExt, "%s%d", DEMOEXT, NEW_PROTOCOL_VERSION );
				if ( Q_stricmp( ext, demoExt ) == 0 ) {
					*(strrchr( demoName, '.' )) = '\0';
				}
			}
		}
		Com_sprintf( name, sizeof( name ), "demos/%s", demoName );

		clc.explicitRecordName = qtrue;
	} else {

		Com_RealTime( &t );
		Com_sprintf( name, sizeof( name ), "demos/demo-%04d%02d%02d-%02d%02d%02d",
			1900 + t.tm_year, 1 + t.tm_mon,	t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec );

		clc.explicitRecordName = qfalse;
	}

	// save desired filename without extension
	Q_strncpyz( clc.recordName, name, sizeof( clc.recordName ) );

	if ( !cl_silentRecord->integer ) {
		Com_Printf( "recording to %s.\n", name );
	}

	// start new record with temporary extension
	Q_strcat( name, sizeof( name ), ".tmp" );

	// open the demo file
	clc.recordfile = FS_FOpenFileWrite( name );
	if ( clc.recordfile == FS_INVALID_HANDLE ) {
		Com_Printf( "ERROR: couldn't open.\n" );
		clc.recordName[0] = '\0';
		return;
	}

	clc.demorecording = qtrue;

	Com_TruncateLongString( clc.recordNameShort, clc.recordName );
	Cvar_Set( "cl_demofilename", clc.recordNameShort ); // bani
	Cvar_Set( "cl_demooffset", "0" ); // bani
	Cvar_Set( "cl_demorecording", "1" ); // fretn


	// don't start saving messages until a non-delta compressed message is received
	clc.demowaiting = qtrue;

	// we will rename record to dm_84 or dm_85 depending from this flag
	clc.dm84compat = qtrue;

	// write out the gamestate message
	CL_WriteGamestate( qtrue );

	// the rest of the demo file will be copied from net messages
}


/*
====================
CL_CompleteRecordName
====================
*/
static void CL_CompleteRecordName( char *args, int argNum )
{
	if ( argNum == 2 )
	{
		char demoExt[ 16 ];

		Com_sprintf( demoExt, sizeof( demoExt ), ".%s%d", DEMOEXT, com_protocol->integer );
		Field_CompleteFilename( "demos", demoExt, qtrue, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}


/*
=======================================================================

CLIENT SIDE DEMO PLAYBACK

=======================================================================
*/

/*
====================
CL_StartBenchmark_f
====================
*/
static void CL_StartBenchmark_f(void) {
	if (Cmd_Argc() < 2) {
		Com_Printf("benchmark <demoname>\n");
		return;
	}

	// enable timedemo if it isn't set already (and keep track if we changed the value, so we can restore it)
	if (!Cvar_VariableIntegerValue("timedemo"))	{
		Cvar_Set("timedemo", "1");
		cls.resetTimedemoCvar = qtrue;
	}

	cls.benchmarking = qtrue;
	Cbuf_ExecuteText(EXEC_APPEND, va("demo %s", Cmd_Argv(1)));
}

/*
=================
CL_CompareFrametimes
=================
*/
static int CL_CompareFrametimes(const void *a, const void *b) {
	const uint32_t arg1 = *(const uint32_t *)a;
	const uint32_t arg2 = *(const uint32_t *)b;

	if (arg1 > arg2) {
		return 1;
	} else if (arg2 > arg1) {
		return -1;
	} else {
		return 0;
	}
}

/*
=================
CL_TimedemoResults
=================
*/
static void CL_TimedemoResults(void) {
	int64_t  time;
	int      i, numFrames;
	uint32_t sortedFrametimes[MAX_TIMEDEMO_FRAMES];
	int      onePercentIdx, pointOnePercentIdx;
	float    fps, minFps, maxFps;
	char     onePercent[8], pointOnePercent[8];
	int      desiredFrametime, numOptimalFrames;

	time = Sys_Microseconds() - clc.timedemo.timeDemoStart;

	if (time <= 0) {
		return;
	}

	// timeFrames gets incremented before we get here, but we never have a chance to measure the frametime
	// since the playback ends, therefore scrap the last frame entirely as it never gets stored
	numFrames = clc.timedemo.timeDemoFrames - 1;

	fps = numFrames * 1000000.0f / time;

	Com_Memcpy(sortedFrametimes, clc.timedemo.frametime, numFrames * sizeof(uint32_t));
	qsort(sortedFrametimes, numFrames, sizeof(uint32_t), CL_CompareFrametimes);

	minFps = 1000000.0f / sortedFrametimes[numFrames - 1];
	maxFps = 1000000.0f / sortedFrametimes[0];

	// filter out potential 0ms anomalies for maxfps
	if (sortedFrametimes[0] == 0) {
		for (i = 0; i < numFrames; i++) {
			if (sortedFrametimes[i] != 0) {
				maxFps = 1000000.0f / sortedFrametimes[i];
				break;
			}
		}
	}

	onePercentIdx = (int)(0.01f * numFrames);

	// make sure we have enough total frames to display 1% lows
	if (onePercentIdx) {
		Com_sprintf(onePercent, sizeof(onePercent), "%3.2f", 1000000.0f / sortedFrametimes[numFrames - 1 - onePercentIdx]);
	}

	pointOnePercentIdx = (int)(0.001f * numFrames);

	// make sure we have enough total frames to display 0.1% lows
	if (pointOnePercentIdx) {
		Com_sprintf(pointOnePercent, sizeof(pointOnePercent), "%3.2f", 1000000.0f / sortedFrametimes[numFrames - 1 - pointOnePercentIdx]);
	}

	desiredFrametime = 1000 / com_maxfps->integer;
	numOptimalFrames = 0;

	for (i = 0; i < numFrames; i++) {
		if (sortedFrametimes[i] / 1000.0f > desiredFrametime) {
			break;
		}

		numOptimalFrames++;
	}

	Com_Printf("\n----- Benchmark results -----\n");
	Com_Printf("\n%-18s %3.2f sec\n%-18s %i\n%-18s %3.2f\n%-18s %3.2f\n%-18s %3.2f\n%-18s %s\n%-18s %s\n%-18s %3.2f pct\n",
			   "Time elapsed:", time / 1000000.0f,
			   "Total frames:", numFrames,
			   "Minimum fps:", minFps,
			   "Maximum fps:", maxFps,
			   "Average fps:", fps,
			   "99th pct. min:", onePercentIdx ? onePercent : "--",
			   "99.9th pct. min:", pointOnePercentIdx ? pointOnePercent : "--",
			   "Stability:", (float)numOptimalFrames / (float)numFrames * 100.0f);
	Com_Printf("\n-----------------------------\n\n");
}

/*
=================
CL_DemoCompleted
=================
*/
static void CL_DemoCompleted( void ) {
	if ( com_timedemo->integer ) {
		CL_TimedemoResults();

		// reset timedemo cvar if 'benchmark' command forcibly enabled it
		if (cls.benchmarking && cls.resetTimedemoCvar) {
			Cvar_Set("timedemo", "0");
		}

		cls.benchmarking = qfalse;
		cls.resetTimedemoCvar = qfalse;
	}

	// fretn
	//if ( clc.waverecording ) {
	//	CL_WriteWaveClose();
	//	clc.waverecording = qfalse;
	//}

	CL_Disconnect( qtrue );
	CL_NextDemo();
}


/*
=================
CL_ReadDemoMessage
=================
*/
void CL_ReadDemoMessage( void ) {
	int			r;
	msg_t		buf;
	byte		bufData[ MAX_MSGLEN_BUF ];
	int			s;

	if ( clc.demofile == FS_INVALID_HANDLE ) {
		CL_DemoCompleted();
		return;
	}

	// get the sequence number
	r = FS_Read( &s, 4, clc.demofile );
	if ( r != 4 ) {
		CL_DemoCompleted();
		return;
	}
	clc.serverMessageSequence = LittleLong( s );

	// init the message
	MSG_Init( &buf, bufData, MAX_MSGLEN );

	// get the length
	r = FS_Read( &buf.cursize, 4, clc.demofile );
	if ( r != 4 ) {
		CL_DemoCompleted();
		return;
	}
	buf.cursize = LittleLong( buf.cursize );
	if ( buf.cursize == -1 ) {
		CL_DemoCompleted();
		return;
	}
	if ( buf.cursize > buf.maxsize ) {
		Com_Error (ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN");
	}
	r = FS_Read( buf.data, buf.cursize, clc.demofile );
	if ( r != buf.cursize ) {
		Com_Printf( "Demo file was truncated.\n");
		CL_DemoCompleted();
		return;
	}

	clc.lastPacketTime = cls.realtime;
	buf.readcount = 0;

	clc.demoCommandSequence = clc.serverCommandSequence;

	CL_ParseServerMessage( &buf );

	if ( clc.demorecording ) {
		// track changes and write new message
		if ( clc.eventMask & EM_GAMESTATE ) {
			CL_WriteGamestate( qfalse );
			// nothing should came after gamestate in current message
		} else if ( clc.eventMask & (EM_SNAPSHOT|EM_COMMAND) ) {
			CL_WriteSnapshot();
		}
	}
}


/*
====================
CL_WalkDemoExt
====================
*/
static int CL_WalkDemoExt( const char *arg, char *name, int name_len, fileHandle_t *handle )
{
	int i;

	*handle = FS_INVALID_HANDLE;
	i = 0;

	while ( demo_protocols[ i ] )
	{
		Com_sprintf( name, name_len, "demos/%s.%s%d", arg, DEMOEXT, demo_protocols[ i ] );
		FS_BypassPure();
		FS_FOpenFileRead( name, handle, qtrue );
		FS_RestorePure();
		if ( *handle != FS_INVALID_HANDLE )
		{
			Com_Printf( "Demo file: %s\n", name );
			return demo_protocols[ i ];
		}
		else
			Com_Printf( "Not found: %s\n", name );
		i++;
	}
	return -1;
}


/*
====================
CL_DemoExtCallback
====================
*/
static qboolean CL_DemoNameCallback_f( const char *filename, int length )
{
	const int ext_len = strlen( "." DEMOEXT );
	const int num_len = 2;
	int version;

	if ( length <= ext_len + num_len || Q_stricmpn( filename + length - (ext_len + num_len), "." DEMOEXT, ext_len ) != 0 )
		return qfalse;

	version = atoi( filename + length - num_len );
	if ( version == com_protocol->integer )
		return qtrue;

	if ( version < VERY_OLD_PROTOCOL_VERSION || version > NEW_PROTOCOL_VERSION )
		return qfalse;

	return qtrue;
}


/*
====================
CL_CompleteDemoName
====================
*/
static void CL_CompleteDemoName( char *args, int argNum )
{
	if ( argNum == 2 )
	{
		FS_SetFilenameCallback( CL_DemoNameCallback_f );
		Field_CompleteFilename( "demos", "." DEMOEXT "??", qfalse, FS_MATCH_ANY | FS_MATCH_STICK );
		FS_SetFilenameCallback( NULL );
	}
}


/*
====================
CL_PlayDemo_f

demo <demoname>

====================
*/
static void CL_PlayDemo_f( void ) {
	char		name[MAX_OSPATH];
	const char		*arg;
	char		*ext_test;
	int			protocol, i;
	char		retry[MAX_OSPATH];
	const char	*shortname, *slash;
	fileHandle_t hFile;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "demo <demoname>\n" );
		return;
	}

	// open the demo file
	arg = Cmd_Argv( 1 );

	// check for an extension .DEMOEXT_?? (?? is protocol)
	ext_test = strrchr(arg, '.');
	if ( ext_test && !Q_stricmpn(ext_test + 1, DEMOEXT, ARRAY_LEN(DEMOEXT) - 1) )
	{
		protocol = atoi(ext_test + ARRAY_LEN(DEMOEXT));

		for( i = 0; demo_protocols[ i ]; i++ )
		{
			if ( demo_protocols[ i ] == protocol )
				break;
		}

		if ( demo_protocols[ i ] || protocol == com_protocol->integer )
		{
			Com_sprintf(name, sizeof(name), "demos/%s", arg);
			FS_BypassPure();
			FS_FOpenFileRead( name, &hFile, qtrue );
			FS_RestorePure();
		}
		else
		{
			size_t len;

			Com_Printf("Protocol %d not supported for demos\n", protocol );
			len = ext_test - arg;

			if ( len > ARRAY_LEN( retry ) - 1 ) {
				len = ARRAY_LEN( retry ) - 1;
			}

			Q_strncpyz( retry, arg, len + 1);
			retry[len] = '\0';
			protocol = CL_WalkDemoExt( retry, name, sizeof( name ), &hFile );
		}
	}
	else
		protocol = CL_WalkDemoExt( arg, name, sizeof( name ), &hFile );

	if ( hFile == FS_INVALID_HANDLE ) {
		Com_Printf( S_COLOR_YELLOW "couldn't open %s\n", name );
		return;
	}

	FS_FCloseFile( hFile );
	hFile = FS_INVALID_HANDLE;

	// make sure a local server is killed
	// 2 means don't force disconnect of local client
	Cvar_Set( "sv_killserver", "2" );

	CL_Disconnect( qtrue );

	// clc.demofile will be closed during CL_Disconnect so reopen it
	if ( FS_FOpenFileRead( name, &clc.demofile, qtrue ) == -1 )
	{
		// drop this time
		Com_Error( ERR_DROP, "couldn't open %s", name );
		return;
	}

	if ( (slash = strrchr( name, '/' )) != NULL )
		shortname = slash + 1;
	else
		shortname = name;

	Q_strncpyz( clc.demoName, shortname, sizeof( clc.demoName ) );

	Con_Close();

	cls.state = CA_CONNECTED;
	clc.demoplaying = qtrue;
	Q_strncpyz( cls.servername, shortname, sizeof( cls.servername ) );

	if ( protocol <= OLD_PROTOCOL_VERSION )
		clc.compat = qtrue;
	else
		clc.compat = qfalse;

	// read demo messages until connected
#ifdef USE_CURL
	while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED && !Com_DL_InProgress( &download ) ) {
#else
	while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED ) {
#endif
		CL_ReadDemoMessage();
	}

	// don't get the first snapshot this frame, to prevent the long
	// time from the gamestate load from messing causing a time skip
	clc.firstDemoFrameSkipped = qfalse;
}


/*
==================
CL_NextDemo

Called when a demo or cinematic finishes
If the "nextdemo" cvar is set, that command will be issued
==================
*/
static void CL_NextDemo( void ) {
	char v[ MAX_CVAR_VALUE_STRING ];

	Cvar_VariableStringBuffer( "nextdemo", v, sizeof( v ) );
	Com_DPrintf( "CL_NextDemo: %s\n", v );
	if ( !v[0] ) {
		return;
	}

	Cvar_Set( "nextdemo", "" );
	Cbuf_AddText( v );
	Cbuf_AddText( "\n" );
	Cbuf_Execute();
}


//======================================================================

/*
=====================
CL_ShutdownVMs
=====================
*/
static void CL_ShutdownVMs( void )
{
	CL_ShutdownCGame();
	CL_ShutdownUI();
}


/*
=====================
CL_ShutdownAll
=====================
*/
void CL_ShutdownAll( void ) {
	aviRecordingState_t cl_recordState = CL_VideoRecording();
	// stop recording video on map change
	if ( cl_recordState == AVIDEMO_VIDEO )
		CL_CloseAVI( qfalse );
	else //if ( cl_recordState == AVIDEMO_CVAR )
		Cvar_ForceReset( "cl_avidemo" );

	// stop recording demo on map change
	if ( clc.demorecording )
		CL_StopRecord_f();

	// clear and mute all sounds until next registration
	S_DisableSounds();

#ifdef USE_CURL
	// download subsystem
	DL_Shutdown();
#endif

	// shutdown VMs
	CL_ShutdownVMs();

	// shutdown the renderer
	if ( re.Shutdown ) {
		if ( CL_GameSwitch() ) {
			if ( re.purgeCache ) {
				CL_DoPurgeCache();
			}
			CL_ShutdownRef( REF_DESTROY_WINDOW ); // shutdown renderer & GLimp
		} else {
			re.Shutdown( REF_KEEP_CONTEXT ); // don't destroy window or context
		}
	}

	if ( re.purgeCache ) {
		CL_DoPurgeCache();
	}

	cls.rendererStarted = qfalse;
	cls.soundRegistered = qfalse;

	SCR_Done();
}


/*
=================
CL_ClearMemory
=================
*/
void CL_ClearMemory( void ) {
	// if not running a server clear the whole hunk
	if ( !com_sv_running->integer ) {
		// clear the whole hunk
		Hunk_Clear();
		// clear collision map data
		CM_ClearMap();
	} else {
		// clear all the client data on the hunk
		Hunk_ClearToMark();
	}
}


/*
=================
CL_FlushMemory

Called by CL_MapLoading, CL_Connect_f, CL_PlayDemo_f, and CL_ParseGamestate the only
ways a client gets into a game
Also called by Com_Error
=================
*/
void CL_FlushMemory( void ) {

	// shutdown all the client stuff
	CL_ShutdownAll();

	CL_ClearMemory();

	CL_StartHunkUsers();
}


/*
=====================
CL_MapLoading

A local server is starting to load a map, so update the
screen to let the user know about it, then dump all client
memory on the hunk from cgame, ui, and renderer
=====================
*/
void CL_MapLoading( void ) {
	if ( com_dedicated->integer ) {
		cls.state = CA_DISCONNECTED;
		Key_SetCatcher( KEYCATCH_CONSOLE );
		return;
	}

	if ( !com_cl_running->integer ) {
		return;
	}

	Con_Close();
	Key_SetCatcher( 0 );

	// if we are already connected to the local host, stay connected
	if ( cls.state >= CA_CONNECTED && !Q_stricmp( cls.servername, "localhost" ) ) {
		cls.state = CA_CONNECTED;		// so the connect screen is drawn
		Com_Memset( cls.updateInfoString, 0, sizeof( cls.updateInfoString ) );
		Com_Memset( clc.serverMessage, 0, sizeof( clc.serverMessage ) );
		Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
		clc.lastPacketSentTime = -9999;
		cls.framecount++;
		SCR_UpdateScreen();
	} else {
		// clear nextmap so the cinematic shutdown doesn't execute it
		Cvar_Set( "nextmap", "" );
		CL_Disconnect( qtrue );
		Q_strncpyz( cls.servername, "localhost", sizeof(cls.servername) );
		cls.state = CA_CHALLENGING;		// so the connect screen is drawn
		Key_SetCatcher( 0 );
		cls.framecount++;
		SCR_UpdateScreen();
		clc.connectTime = -RETRANSMIT_TIMEOUT;
		NET_StringToAdr( cls.servername, &clc.serverAddress, NA_UNSPEC );
		// we don't need a challenge on the localhost
		CL_CheckForResend();
	}
}


/*
=====================
CL_UpdateGUID

Update cl_guid using the etkey file
=====================
*/
static void CL_UpdateGUID(void)
{
	fileHandle_t f;
	int          len;

	len = FS_SV_FOpenFileRead(BASEGAME "/" ETKEY_FILE, &f);

	if ( f )
		FS_FCloseFile(f);

	if (len < ETKEY_SIZE)
	{
		Com_Printf(S_COLOR_RED "ERROR: Could not set etkey (size mismatch).\n");
		Cvar_Set("cl_guid", "");
	}
	else
	{
		char *guid = Com_PBMD5File( ETKEY_FILE );

		if (guid)
		{
			Cvar_Set("cl_guid", guid);
		}
	}
}

/*
=====================
CL_GenerateETKey

Test for the existence of a valid etkey and generate a new one
if it doesn't exist
=====================
*/
static void CL_GenerateETKey(void)
{
	int          len = 0;
	fileHandle_t f;

	len = FS_SV_FOpenFileRead(BASEGAME "/" ETKEY_FILE, &f);
	FS_FCloseFile(f);
	if (len > 0)
	{
		Com_DPrintf(S_COLOR_CYAN "ETKEY found.\n");
		return;
	}
	else
	{
		time_t    tt;
		struct tm *t;
		int       last;
		char      buff[ETKEY_SIZE];

		buff[0] = '\0';
		tt = time(NULL);
		t  = localtime(&tt);
		srand(Sys_Milliseconds());
		last = rand() % 9999;

		Com_sprintf(buff, sizeof(buff), "0000001002%04i%02i%02i%02i%02i%02i%03i", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, last);

		f = FS_SV_FOpenFileWrite(BASEGAME "/" ETKEY_FILE);
		if (f == FS_INVALID_HANDLE)
		{
			Com_Printf(S_COLOR_RED "ERROR: Could not open %s for write.\n", ETKEY_FILE);
			return;
		}
		if (FS_Write(buff, sizeof(buff), f) > 0)
		{
			Com_Printf(S_COLOR_CYAN "ETKEY file generated.\n");
		}
		else
		{
			Com_Printf(S_COLOR_RED "ERROR: Could not write file %s.\n", ETKEY_FILE);
		}
		FS_FCloseFile( f );
	}
}

/*
=====================
CL_ClearState

Called before parsing a gamestate
=====================
*/
void CL_ClearState( void ) {

//	S_StopAllSounds();

	Com_Memset( &cl, 0, sizeof( cl ) );
}

/*
=====================
CL_ClearStaticDownload
Clear download information that we keep in cls (disconnected download support)
=====================
*/
void CL_ClearStaticDownload( void ) {
	assert( !cls.bWWWDlDisconnected ); // reset before calling
	cls.downloadRestart = qfalse;
	cls.downloadTempName[0] = '\0';
	cls.downloadName[0] = '\0';
	cls.originalDownloadName[0] = '\0';
}


/*
=====================
CL_ResetOldGame
=====================
*/
void CL_ResetOldGame( void )
{
	cl_oldGameSet = qfalse;
	cl_oldGame[0] = '\0';
}


/*
=====================
CL_RestoreOldGame

change back to previous fs_game
=====================
*/
static qboolean CL_RestoreOldGame( void )
{
	if ( cl_oldGameSet )
	{
		cl_oldGameSet = qfalse;
		Cvar_Set( "fs_game", cl_oldGame );
		FS_ConditionalRestart( clc.checksumFeed, qtrue );
		return qtrue;
	}
	return qfalse;
}


/*
=====================
CL_Disconnect

Called when a connection, demo, or cinematic is being terminated.
Goes from a connected state to either a menu state or a console state
Sends a disconnect message to the server
This is also called on Com_Error and Com_Quit, so it shouldn't cause any errors
=====================
*/
qboolean CL_Disconnect( qboolean showMainMenu ) {
	static qboolean cl_disconnecting = qfalse;
	qboolean cl_restarted = qfalse;
	qboolean cl_wasconnected = qfalse;
	aviRecordingState_t cl_recordState = AVIDEMO_NONE;

	if ( !com_cl_running || !com_cl_running->integer ) {
		return cl_restarted;
	}

	if ( cl_disconnecting ) {
		return cl_restarted;
	}

	cl_disconnecting = qtrue;

	// Stop demo recording
	if ( clc.demorecording ) {
		CL_StopRecord_f();
	}

	// Stop demo playback
	if ( clc.demofile != FS_INVALID_HANDLE ) {
		FS_FCloseFile( clc.demofile );
		clc.demofile = FS_INVALID_HANDLE;
	}

	// Finish downloads
	if ( !cls.bWWWDlDisconnected ) {
		if ( clc.download != FS_INVALID_HANDLE ) {
			FS_FCloseFile( clc.download );
			clc.download = FS_INVALID_HANDLE;
		}
		*cls.downloadTempName = *cls.downloadName = '\0';
		Cvar_Set( "cl_downloadName", "" );
	}

	cl_recordState = CL_VideoRecording();
	// Stop recording any video
	if ( cl_recordState != AVIDEMO_NONE ) {
		// Finish rendering current frame
		cls.framecount++;
		SCR_UpdateScreen();
		if ( cl_recordState == AVIDEMO_VIDEO )
			CL_CloseAVI( qfalse );
		else if ( cl_recordState == AVIDEMO_CVAR )
			Cvar_ForceReset( "cl_avidemo" );
	}
	else
		Cvar_ForceReset( "cl_avidemo" );

	if ( cgvm ) {
		// do that right after we rendered last video frame
		CL_ShutdownCGame();
	}

	SCR_StopCinematic();
	S_StopAllSounds();
	Key_ClearStates();

	if ( uivm && showMainMenu ) {
		VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_NONE );
	}

	// Remove pure paks
	FS_ClearPureServerPaks();
	//FS_PureServerSetLoadedPaks( "", "" );
	//FS_PureServerSetReferencedPaks( "", "" );

	FS_ClearPakReferences( FS_GENERAL_REF | FS_UI_REF | FS_CGAME_REF );

	if ( CL_GameSwitch() ) {
		// keep current gamestate and connection
		cl_disconnecting = qfalse;
		return qfalse;
	}

	// ETJump
	Cvar_ForceReset( "shared" );
	Cvar_ForceReset( "cm_optimizePatchPlanes" );

	// send a disconnect message to the server
	// send it a few times in case one is dropped
	if ( cls.state >= CA_CONNECTED && cls.state != CA_CINEMATIC && !clc.demoplaying ) {
		CL_AddReliableCommand( "disconnect", qtrue );
		CL_WritePacket();
		CL_WritePacket();
		CL_WritePacket();
	}

	CL_ClearState();

	// wipe the client connection
	Com_Memset( &clc, 0, sizeof( clc ) );

	if ( !cls.bWWWDlDisconnected ) {
		CL_ClearStaticDownload();
	}
	cl_wasconnected = (cls.state > CA_DISCONNECTED) ? qtrue : qfalse;
	cls.state = CA_DISCONNECTED;

	// allow cheats locally
	Cvar_Set( "sv_cheats", "1" );

	// not connected to a pure server anymore
	cl_connectedToPureServer = qfalse;

	cl_optimizedPatchServer = qfalse;

	CL_UpdateGUID();

	if ( noGameRestart )
		noGameRestart = qfalse;
	else
		cl_restarted = CL_RestoreOldGame();

	// don't try a restart if uivm is NULL, as we might be in the middle of a restart already
	// don't try a restart if shutting down, as we are in process of shutting it all down already
	if ( !cl_shutdownQuit && uivm && cl_wasconnected && !cl_restarted ) {
		CL_ShutdownUI();
		cls.uiStarted = qtrue;
		CL_InitUI();
	}

	cl_disconnecting = qfalse;

	return cl_restarted;
}


/*
===================
CL_ForwardCommandToServer

adds the current command line as a clientCommand
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void CL_ForwardCommandToServer( const char *string ) {
	const char *cmd;

	cmd = Cmd_Argv( 0 );

	// ignore key up commands
	if ( cmd[0] == '-' ) {
		return;
	}

	// no userinfo updates from command line
	if ( !strcmp( cmd, "userinfo" ) ) {
		return;
	}

	if ( clc.demoplaying || cls.state < CA_CONNECTED || cmd[0] == '+' ) {
		Com_Printf( "Unknown command \"%s" S_COLOR_WHITE "\"\n", cmd );
		return;
	}

	if ( Cmd_Argc() > 1 ) {
		CL_AddReliableCommand( string, qfalse );
	} else {
		CL_AddReliableCommand( cmd, qfalse );
	}
}


/*
===================
CL_RequestMotd

===================
*/
static void CL_RequestMotd( void ) {
	char info[MAX_INFO_STRING];

	if ( !cl_motd->integer ) {
		return;
	}
	Com_Printf( "Resolving %s\n", MOTD_SERVER_NAME );
	if ( !NET_StringToAdr( MOTD_SERVER_NAME, &cls.updateServer, NA_IP ) ) {
		Com_Printf( "Couldn't resolve address\n" );
		return;
	}
	cls.updateServer.port = BigShort( PORT_MOTD );
	Com_Printf( "%s resolved to %s\n", MOTD_SERVER_NAME, NET_AdrToStringwPort( &cls.updateServer ) );

	info[0] = '\0';
	Com_sprintf( cls.updateChallenge, sizeof( cls.updateChallenge ), "%i", rand() );

	Info_SetValueForKey( info, "challenge", cls.updateChallenge );
	Info_SetValueForKey( info, "renderer", cls.glconfig.renderer_string );
	Info_SetValueForKey( info, "version", com_version->string );

	NET_OutOfBandPrint( NS_CLIENT, &cls.updateServer, "getmotd \"%s\"\n", info );
}

/*
======================================================================

CONSOLE COMMANDS

======================================================================
*/

/*
==================
CL_ForwardToServer_f
==================
*/
static void CL_ForwardToServer_f( void ) {
	if ( cls.state != CA_ACTIVE || clc.demoplaying ) {
		Com_Printf ("Not connected to a server.\n");
		return;
	}

	if ( Cmd_Argc() <= 1 || strcmp( Cmd_Argv( 1 ), "userinfo" ) == 0 )
		return;

	// don't forward the first argument
	CL_AddReliableCommand( Cmd_ArgsFrom( 1 ), qfalse );
}


/*
==================
CL_Disconnect_f
==================
*/
void CL_Disconnect_f( void ) {
	SCR_StopCinematic();
	if ( cls.state != CA_DISCONNECTED && cls.state != CA_CINEMATIC ) {
		if ( (uivm && uivm->callLevel) || (cgvm && cgvm->callLevel) ) {
			Com_Error( ERR_DISCONNECT, "Disconnected from server" );
		} else {
			// clear any previous "server full" type messages
			clc.serverMessage[0] = '\0';
			if ( com_sv_running && com_sv_running->integer ) {
				// if running a local server, kill it
				SV_Shutdown( "Disconnected from server" );
			} else {
				Com_Printf( "Disconnected from %s\n", cls.servername );
			}
			Cvar_Set( "com_errorMessage", "" );
			if ( !CL_Disconnect( qfalse ) ) { // restart client if not done already
				CL_FlushMemory();
			}
			if ( uivm ) {
				VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
			}
		}
	}
}


/*
================
CL_Reconnect_f
================
*/
static void CL_Reconnect_f( void ) {
	if ( cl_reconnectArgs->string[0] == '\0' || Q_stricmp( cl_reconnectArgs->string, "localhost" ) == 0 )
		return;
	Cbuf_AddText( va( "connect %s\n", cl_reconnectArgs->string ) );
}


/*
================
CL_Connect_f
================
*/
static void CL_Connect_f( void ) {
	netadrtype_t family;
	netadr_t	addr;
	char	buffer[ sizeof( cls.servername ) ];  // same length as cls.servername
	char	args[ sizeof( cls.servername ) + MAX_CVAR_VALUE_STRING ];
	const char	*server;
	const char	*serverString;
	int		len;
	int		argc;

	argc = Cmd_Argc();
	family = NA_UNSPEC;

	if ( argc != 2 && argc != 3 ) {
		Com_Printf( "usage: connect [-4|-6] <server>\n");
		return;
	}

	if ( argc == 2 ) {
		server = Cmd_Argv(1);
	} else {
		if( !strcmp( Cmd_Argv(1), "-4" ) )
			family = NA_IP;
#ifdef USE_IPV6
		else if( !strcmp( Cmd_Argv(1), "-6" ) )
			family = NA_IP6;
		else
			Com_Printf( S_COLOR_YELLOW "warning: only -4 or -6 as address type understood.\n" );
#else
		else
			Com_Printf( S_COLOR_YELLOW "warning: only -4 as address type understood.\n" );
#endif
		server = Cmd_Argv(2);
	}

	Q_strncpyz( buffer, server, sizeof( buffer ) );
	server = buffer;

	// skip leading "et:/" in connection string
	if ( !Q_stricmpn( server, "et:/", 4 ) ) {
		server += 5;
	}

	// skip all slash prefixes
	while ( *server == '/' ) {
		server++;
	}

	len = strlen( server );
	if ( len <= 0 ) {
		return;
	}

	// some programs may add ending slash
	if ( buffer[len-1] == '/' ) {
		buffer[len-1] = '\0';
	}

	if ( !*server ) {
		return;
	}

	// try resolve remote server first
	if ( !NET_StringToAdr( server, &addr, family ) ) {
		Com_Printf( S_COLOR_YELLOW "Bad server address - %s\n", server );
		//cls.state = CA_DISCONNECTED;
		Cvar_Set( "ui_connecting", "0" );
		return;
	}

	// save arguments for reconnect
	Q_strncpyz( args, Cmd_ArgsFrom( 1 ), sizeof( args ) );

	S_StopAllSounds();      // NERVE - SMF

	Cvar_Set( "ui_connecting", "1" );

	// fire a message off to the motd server
	CL_RequestMotd();

	// clear any previous "server full" type messages
	clc.serverMessage[0] = '\0';

	// if running a local server, kill it
	if ( com_sv_running->integer && !strcmp( server, "localhost" ) ) {
		SV_Shutdown( "Server quit" );
	}

	// make sure a local server is killed
	Cvar_Set( "sv_killserver", "1" );
	SV_Frame( 0 );

	noGameRestart = qtrue;
	CL_Disconnect( qtrue );
	Con_Close();

	Q_strncpyz( cls.servername, server, sizeof( cls.servername ) );

	// copy resolved address
	clc.serverAddress = addr;

	if (clc.serverAddress.port == 0) {
		clc.serverAddress.port = BigShort( PORT_SERVER );
	}

	serverString = NET_AdrToStringwPort( &clc.serverAddress );

	Com_Printf( "%s resolved to %s\n", cls.servername, serverString );

	CL_UpdateGUID();
	// if we aren't playing on a lan, we need to authenticate
	// with the cd key
	if ( NET_IsLocalAddress( &clc.serverAddress ) ) {
		cls.state = CA_CHALLENGING;
	} else {
		cls.state = CA_CONNECTING;

		// Set a client challenge number that ideally is mirrored back by the server.
		//clc.challenge = ((rand() << 16) ^ rand()) ^ Com_Milliseconds();
		Com_RandomBytes( (byte*)&clc.challenge, sizeof( clc.challenge ) );
	}

	//Cvar_Set( "cl_avidemo", "0" );

	// show_bug.cgi?id=507
	// prepare to catch a connection process that would turn bad
	Cvar_Set( "com_errorDiagnoseIP", serverString );
	// ATVI Wolfenstein Misc #439
	// we need to setup a correct default for this, otherwise the first val we set might reappear
	Cvar_Set( "com_errorMessage", "" );

	Key_SetCatcher( 0 );
	clc.connectTime = -99999;	// CL_CheckForResend() will fire immediately
	clc.connectPacketCount = 0;

	Cvar_Set( "cl_reconnectArgs", args );

	// server connection string
	Cvar_Set( "cl_currentServerAddress", server );
	Cvar_Set( "cl_currentServerIP", serverString );
}

#define MAX_RCON_MESSAGE (MAX_STRING_CHARS+4)

/*
==================
CL_CompleteRcon
==================
*/
static void CL_CompleteRcon( char *args, int argNum )
{
	if ( argNum >= 2 )
	{
		// Skip "rcon "
		char *p = Com_SkipTokens( args, 1, " " );

		if ( p > args )
			Field_CompleteCommand( p, qtrue, qtrue );
	}
}


/*
=====================
CL_Rcon_f

Send the rest of the command line over as
an unconnected command.
=====================
*/
static void CL_Rcon_f( void ) {
	char message[MAX_RCON_MESSAGE];
	const char *sp;
	int len;

	if ( !rcon_client_password->string[0] ) {
		Com_Printf( "You must set 'rconPassword' before\n"
			"issuing an rcon command.\n" );
		return;
	}

	if ( cls.state >= CA_CONNECTED ) {
		rcon_address = clc.netchan.remoteAddress;
	} else {
		if ( !rconAddress->string[0] ) {
			Com_Printf( "You must either be connected,\n"
				"or set the 'rconAddress' cvar\n"
				"to issue rcon commands\n" );
			return;
		}
		if ( !NET_StringToAdr( rconAddress->string, &rcon_address, NA_UNSPEC ) ) {
			return;
		}
		if ( rcon_address.port == 0 ) {
			rcon_address.port = BigShort( PORT_SERVER );
		}
	}

	message[0] = -1;
	message[1] = -1;
	message[2] = -1;
	message[3] = -1;
	message[4] = '\0';

	// we may need to quote password if it contains spaces
	sp = strchr( rcon_client_password->string, ' ' );

	len = Com_sprintf( message+4, sizeof( message )-4,
		sp ? "rcon \"%s\" %s" : "rcon %s %s",
		rcon_client_password->string,
		Cmd_Cmd() + 5 ) + 4 + 1; // including OOB marker and '\0'

	NET_SendPacket( NS_CLIENT, len, message, &rcon_address );
}


/*
=================
CL_SendPureChecksums
=================
*/
static void CL_SendPureChecksums( void ) {
	char cMsg[ MAX_STRING_CHARS-1 ];
	int len;

	if ( !cl_connectedToPureServer || clc.demoplaying )
		return;

	// if we are pure we need to send back a command with our referenced pk3 checksums
	len = sprintf( cMsg, "cp %d ", cl.serverId );
	strcpy( cMsg + len, FS_ReferencedPakPureChecksums( sizeof( cMsg ) - len - 1 ) );

	CL_AddReliableCommand( cMsg, qfalse );
}


/*
=================
CL_ResetPureClientAtServer
=================
*/
static void CL_ResetPureClientAtServer( void ) {
	CL_AddReliableCommand( "vdr", qfalse );
}


/*
=================
CL_Vid_Restart

Restart the video subsystem

we also have to reload the UI and CGame because the renderer
doesn't know what graphics to reload
=================
*/
static void CL_Vid_Restart( refShutdownCode_t shutdownCode ) {
	aviRecordingState_t cl_recordState = CL_VideoRecording();
	// RF, don't show percent bar, since the memory usage will just sit at the same level anyway
	com_expectedhunkusage = -1;

	// Settings may have changed so stop recording now
	if ( cl_recordState == AVIDEMO_VIDEO )
		CL_CloseAVI( qfalse );
	else //if ( cl_recordState == AVIDEMO_CVAR )
		Cvar_ForceReset( "cl_avidemo" );

	if ( clc.demorecording )
		CL_StopRecord_f();

	// clear and mute all sounds until next registration
	S_DisableSounds();

	// shutdown VMs
	CL_ShutdownVMs();

	// shutdown the renderer and clear the renderer interface
	CL_ShutdownRef( shutdownCode ); // REF_KEEP_CONTEXT, REF_KEEP_WINDOW, REF_DESTROY_WINDOW

	// client is no longer pure until new checksums are sent
	CL_ResetPureClientAtServer();

	// clear pak references
	FS_ClearPakReferences( FS_UI_REF | FS_CGAME_REF );

	// reinitialize the filesystem if the game directory or checksum has changed
	if ( !clc.demoplaying ) // -EC-
		FS_ConditionalRestart( clc.checksumFeed, qfalse );

	cls.soundRegistered = qfalse;

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );

	CL_ClearMemory();

	// startup all the client stuff
	CL_StartHunkUsers();

	// start the cgame if connected
	if ( ( cls.state > CA_CONNECTED && cls.state != CA_CINEMATIC ) || cls.startCgame ) {
		cls.cgameStarted = qtrue;
		CL_InitCGame();
		// send pure checksums
		CL_SendPureChecksums();
	}

	cls.startCgame = qfalse;
}


/*
=================
CL_Vid_Restart_f

Wrapper for CL_Vid_Restart
=================
*/
static void CL_Vid_Restart_f( void ) {

	if ( Q_stricmp( Cmd_Argv( 1 ), "keep_window" ) == 0 || Q_stricmp( Cmd_Argv( 1 ), "fast" ) == 0 ) {
		// fast path: keep window
		CL_Vid_Restart( REF_KEEP_WINDOW );
	} else {
		CL_Vid_Restart( REF_DESTROY_WINDOW );
	}
}


/*
=================
CL_UI_Restart_f

Restart the ui subsystem
=================
*/
static void CL_UI_Restart_f( void ) {          // NERVE - SMF
	// shutdown the UI
	CL_ShutdownUI();
	cls.uiStarted = qtrue;
	// init the UI
	CL_InitUI();
}

/*
=================
CL_Snd_Reload_f

Reloads sounddata from disk, retains soundhandles.
=================
*/
static void CL_Snd_Reload_f( void ) {
	S_Reload();
}


/*
=================
CL_Snd_Restart_f

Restart the sound subsystem
The cgame and game must also be forced to restart because
handles will be invalid
=================
*/
static void CL_Snd_Restart_f( void )
{
	S_Shutdown();

	// sound will be reinitialized by vid_restart
	CL_Vid_Restart( REF_KEEP_CONTEXT /*REF_KEEP_WINDOW*/ );
}


/*
==================
CL_PK3List_f
==================
*/
static void CL_OpenedPK3List_f( void ) {
	qboolean dummy = qfalse;
	Com_Printf("Opened PK3 Names: %s\n", FS_LoadedPakNames(qfalse, &dummy));
}


/*
==================
CL_PureList_f
==================
*/
static void CL_ReferencedPK3List_f( void ) {
	Com_Printf( "Referenced PK3 Names: %s\n", FS_ReferencedPakNames(NULL) );
}


/*
==================
CL_Configstrings_f
==================
*/
static void CL_Configstrings_f( void ) {
	int		i;
	int		ofs;

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf( "Not connected to a server.\n");
		return;
	}

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		ofs = cl.gameState.stringOffsets[ i ];
		if ( !ofs ) {
			continue;
		}
		Com_Printf( "%4i: %s\n", i, cl.gameState.stringData + ofs );
	}
}


/*
==============
CL_Clientinfo_f
==============
*/
static void CL_Clientinfo_f( void ) {
	Com_Printf( "--------- Client Information ---------\n" );
	Com_Printf( "state: %i\n", cls.state );
	Com_Printf( "Server: %s\n", cls.servername );
	Com_Printf ("User info settings:\n");
	Info_Print( Cvar_InfoString( CVAR_USERINFO, NULL ) );
	Com_Printf( "--------------------------------------\n" );
}


/*
==============
CL_Serverinfo_f
==============
*/
static void CL_Serverinfo_f( void ) {
	int		ofs;

	ofs = cl.gameState.stringOffsets[ CS_SERVERINFO ];
	if ( !ofs )
		return;

	Com_Printf( "Server info settings:\n" );
	Info_Print( cl.gameState.stringData + ofs );
}


/*
===========
CL_Systeminfo_f
===========
*/
static void CL_Systeminfo_f( void ) {
	int ofs;

	ofs = cl.gameState.stringOffsets[ CS_SYSTEMINFO ];
	if ( !ofs )
		return;

	Com_Printf( "System info settings:\n" );
	Info_Print( cl.gameState.stringData + ofs );
}


/*
===========
CL_Wolfinfo_f
===========
*/
static void CL_Wolfinfo_f( void ) {
	int ofs;

	// WOLFINFO cvars are unused in ETF
	if ( currentGameMod == GAMEMOD_ETF )
		return;

	ofs = cl.gameState.stringOffsets[ CS_WOLFINFO ];
	if ( !ofs )
		return;

	Com_Printf( "Wolf info settings:\n" );
	Info_Print( cl.gameState.stringData + ofs );
}


//====================================================================

/*
=================
CL_DownloadsComplete

Called when all downloading has been completed
=================
*/
static void CL_DownloadsComplete( void ) {

	// if we downloaded files we need to restart the file system
	if ( clc.downloadRestart ) {
		clc.downloadRestart = qfalse;

		FS_Restart(clc.checksumFeed); // We possibly downloaded a pak, restart the file system to load it

		if ( !cls.bWWWDlDisconnected ) {
			// inform the server so we get new gamestate info
			CL_AddReliableCommand( "donedl", qfalse );
		}
		// we can reset that now
		cls.bWWWDlDisconnected = qfalse;
		CL_ClearStaticDownload();

		// by sending the donedl command we request a new gamestate
		// so we don't want to load stuff yet
		return;
	}

	// TTimo: I wonder if that happens - it should not but I suspect it could happen if a download fails in the middle or is aborted
	//assert( !cls.bWWWDlDisconnected );

	// let the client game init and load data
	cls.state = CA_LOADING;

	// Pump the loop, this may change gamestate!
	Com_EventLoop();

	// if the gamestate was changed by calling Com_EventLoop
	// then we loaded everything already and we don't want to do it again.
	if ( cls.state != CA_LOADING ) {
		return;
	}

	// flush client memory and start loading stuff
	// this will also (re)load the UI
	// if this is a local client then only the client part of the hunk
	// will be cleared, note that this is done after the hunk mark has been set
	CL_FlushMemory();

	// initialize the CGame
	cls.cgameStarted = qtrue;
	CL_InitCGame();

	// set pure checksums
	CL_SendPureChecksums();

	CL_WritePacket();
	CL_WritePacket();
	CL_WritePacket();
}


/*
=================
CL_BeginDownload

Requests a file to download from the server.  Stores it in the current
game directory.
=================
*/
static void CL_BeginDownload( const char *localName, const char *remoteName ) {

	Com_DPrintf("***** CL_BeginDownload *****\n"
				"Localname: %s\n"
				"Remotename: %s\n"
				"****************************\n", localName, remoteName);

	Q_strncpyz ( cls.downloadName, localName, sizeof(cls.downloadName) );
	Com_sprintf( cls.downloadTempName, sizeof(cls.downloadTempName), "%s.tmp", localName );

	// Set so UI gets access to it
	Cvar_Set( "cl_downloadName", remoteName );
	Cvar_Set( "cl_downloadSize", "0" );
	Cvar_Set( "cl_downloadCount", "0" );
	Cvar_SetIntegerValue( "cl_downloadTime", cls.realtime );

	clc.downloadBlock = 0; // Starting new file
	clc.downloadCount = 0;

	CL_AddReliableCommand( va("download %s", remoteName), qfalse );
}


/*
=================
CL_NextDownload

A download completed or failed
=================
*/
void CL_NextDownload( void )
{
	char *s;
	char *remoteName, *localName;

	// A download has finished, check whether this matches a referenced checksum
	if(*cls.downloadName)
	{
		const char *zippath = FS_BuildOSPath(Cvar_VariableString("fs_homepath"), cls.downloadName, NULL );

		if(!FS_CompareZipChecksum(zippath))
			Com_Error(ERR_DROP, "Incorrect checksum for file: %s", cls.downloadName);
	}

	*cls.downloadTempName = *cls.downloadName = '\0';
	Cvar_Set("cl_downloadName", "");

	// We are looking to start a download here
	if (*clc.downloadList) {
		s = clc.downloadList;

		// format is:
		//  @remotename@localname@remotename@localname, etc.

		if (*s == '@')
			s++;
		remoteName = s;

		if ( (s = strchr(s, '@')) == NULL ) {
			CL_DownloadsComplete();
			return;
		}

		*s++ = '\0';
		localName = s;
		if ( (s = strchr(s, '@')) != NULL )
			*s++ = '\0';
		else
			s = localName + strlen(localName); // point at the null byte

		if( !cl_allowDownload->integer ) {
			Com_Error(ERR_DROP, "UDP Downloads are disabled on your client. "
					"(cl_allowDownload is %d)", cl_allowDownload->integer);
			return;
		}
		else {
			CL_BeginDownload( localName, remoteName );
		}
		clc.downloadRestart = qtrue;

		// move over the rest
		memmove( clc.downloadList, s, strlen(s) + 1 );

		return;
	}

	CL_DownloadsComplete();
}


/*
=================
CL_InitDownloads

After receiving a valid game state, we valid the cgame and local zip files here
and determine if we need to download them
=================
*/
void CL_InitDownloads( void ) {

	char missingfiles[ MAXPRINTMSG ];

	// TTimo
	// init some of the www dl data
	clc.bWWWDl = qfalse;
	clc.bWWWDlAborting = qfalse;
	cls.bWWWDlDisconnected = qfalse;
	CL_ClearStaticDownload();

	// whatever autodownlad configuration, store missing files in a cvar, use later in the ui maybe
	if ( FS_ComparePaks( missingfiles, sizeof( missingfiles ), qfalse ) ) {
		Cvar_Set( "com_missingFiles", missingfiles );
	} else {
		Cvar_Set( "com_missingFiles", "" );
	}

	// reset the redirect checksum tracking
	clc.redirectedList[0] = '\0';

	if ( cl_allowDownload->integer && FS_ComparePaks( clc.downloadList, sizeof( clc.downloadList ), qtrue ) ) {
		// this gets printed to UI, i18n
		Com_Printf( CL_TranslateStringBuf( "Need paks: %s\n" ), clc.downloadList );

		if ( *clc.downloadList ) {
			// if autodownloading is not enabled on the server
			cls.state = CA_CONNECTED;

			*cls.downloadTempName = *cls.downloadName = '\0';
			Cvar_Set( "cl_downloadName", "" );

			CL_NextDownload();
			return;
		}

	}

#ifdef USE_CURL
	if ( cl_mapAutoDownload->integer && ( clc.demoplaying || (clc.download == FS_INVALID_HANDLE && !*cls.downloadName) ) )
	{
		const char *info, *mapname, *bsp;

		// get map name and BSP file name
		info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
		mapname = Info_ValueForKey( info, "mapname" );
		bsp = va( "maps/%s.bsp", mapname );

		if ( FS_FOpenFileRead( bsp, NULL, qfalse ) == -1 )
		{
			if ( CL_Download( "dlmap", mapname, qtrue ) )
			{
				cls.state = CA_CONNECTED; // prevent continue loading and shows the ui download progress screen
				return;
			}
		}
	}
#endif // USE_CURL

	CL_DownloadsComplete();
}


/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
static void CL_CheckForResend( void ) {
	int		port, len;
	char	info[MAX_INFO_STRING*2]; // larger buffer to detect overflows
	char	data[MAX_INFO_STRING];
	qboolean	notOverflowed;
	qboolean	infoTruncated;

	// don't send anything if playing back a demo
	if ( clc.demoplaying ) {
		return;
	}

	// resend if we haven't gotten a reply yet
	if ( cls.state != CA_CONNECTING && cls.state != CA_CHALLENGING ) {
		return;
	}

	if ( cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT ) {
		return;
	}

	clc.connectTime = cls.realtime;	// for retransmit requests
	clc.connectPacketCount++;

	switch ( cls.state ) {
	case CA_CONNECTING:
		// requesting a challenge .. IPv6 users always get in as authorize server supports no ipv6.
		// The challenge request shall be followed by a client challenge so no malicious server can hijack this connection.
		NET_OutOfBandPrint( NS_CLIENT, &clc.serverAddress, "getchallenge %d %s", clc.challenge, GAMENAME_FOR_MASTER );
		break;

	case CA_CHALLENGING:
		// sending back the challenge
		port = Cvar_VariableIntegerValue( "net_qport" );

		infoTruncated = qfalse;
		Q_strncpyz( info, Cvar_InfoString( CVAR_USERINFO, &infoTruncated ), sizeof( info ) );

		len = strlen( info );
		if ( len > MAX_USERINFO_LENGTH ) {
			notOverflowed = qfalse;
		} else {
			notOverflowed = qtrue;
		}

		if ( com_protocol->integer != DEFAULT_PROTOCOL_VERSION ) {
			notOverflowed &= Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "protocol",
				com_protocol->string );
		} else {
			notOverflowed &= Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "protocol",
				clc.compat ? XSTRING( OLD_PROTOCOL_VERSION ) : XSTRING( NEW_PROTOCOL_VERSION ) );
		}

		notOverflowed &= Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "qport",
			va( "%i", port ) );

		notOverflowed &= Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "challenge",
			va( "%i", clc.challenge ) );

		// for now - this will be used to inform server about q3msgboom fix
		// this is optional key so will not trigger oversize warning
		Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "client", Q3_VERSION );

		if ( !notOverflowed ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: oversize userinfo, you might be not able to join remote server!\n" );
		}

		len = Com_sprintf( data, sizeof( data ), "connect \"%s\"", info );
		// NOTE TTimo don't forget to set the right data length!
		NET_OutOfBandCompress( NS_CLIENT, &clc.serverAddress, (byte *) &data[0], len );
		// the most current userinfo has been sent, so watch for any
		// newer changes to userinfo variables
		cvar_modifiedFlags &= ~CVAR_USERINFO;

		// ... but force re-send if userinfo was truncated in any way
		if ( infoTruncated || !notOverflowed ) {
			cvar_modifiedFlags |= CVAR_USERINFO;
		}
		break;

	default:
		Com_Error( ERR_FATAL, "CL_CheckForResend: bad cls.state" );
	}
}

/*
===================
CL_DisconnectPacket

Sometimes the server can drop the client and the netchan based
disconnect can be lost.  If the client continues to send packets
to the server, the server will send out of band disconnect packets
to the client so it doesn't have to wait for the full timeout period.
===================
*/
void CL_DisconnectPacket( const netadr_t *from ) {
	if ( cls.state < CA_AUTHORIZING ) {
		return;
	}

	// if not from our server, ignore it
	if ( !NET_CompareAdr( from, &clc.netchan.remoteAddress ) ) {
		return;
	}

	// if we have received packets within three seconds, ignore (it might be a malicious spoof)
	// NOTE TTimo:
	// there used to be a  clc.lastPacketTime = cls.realtime; line in CL_PacketEvent before calling CL_ConnectionLessPacket
	// therefore .. packets never got through this check, clients never disconnected
	// switched the clc.lastPacketTime = cls.realtime to happen after the connectionless packets have been processed
	// you still can't spoof disconnects, cause legal netchan packets will maintain realtime - lastPacketTime below the threshold
	if ( cls.realtime - clc.lastPacketTime < 3000 ) {
		return;
	}

	// if we are doing a disconnected download, leave the 'connecting' screen on with the progress information
	if ( !cls.bWWWDlDisconnected ) {
		// drop the connection
		Com_Printf( "Server disconnected for unknown reason\n" );
		Cvar_Set( "com_errorMessage", "Server disconnected for unknown reason" );
		CL_Disconnect( qtrue );
	} else {
		CL_Disconnect( qfalse );
		Cvar_Set( "ui_connecting", "1" );
		Cvar_Set( "ui_dl_running", "1" );
	}
}


/*
===================
CL_MotdPacket
===================
*/
static void CL_MotdPacket( const netadr_t *from ) {
	const char *challenge;
	const char *info;

	// if not from our server, ignore it
	if ( !NET_CompareAdr( from, &cls.updateServer ) ) {
		return;
	}

	info = Cmd_Argv(1);

	// check challenge
	challenge = Info_ValueForKey( info, "challenge" );
	if ( strcmp( challenge, cls.updateChallenge ) ) {
		return;
	}

	challenge = Info_ValueForKey( info, "motd" );

	Q_strncpyz( cls.updateInfoString, info, sizeof( cls.updateInfoString ) );
	Cvar_Set( "cl_motdString", challenge );
}

/*
===================
CL_PrintPackets
an OOB message from server, with potential markups
print OOB are the only messages we handle markups in
[err_dialog]: used to indicate that the connection should be aborted
  no further information, just do an error diagnostic screen afterwards
[err_prot]: HACK. This is a protocol error. The client uses a custom
  protocol error message (client sided) in the diagnostic window.
  The space for the error message on the connection screen is limited
  to 256 chars.
===================
*/
static qboolean CL_PrintPacket( const netadr_t *from, msg_t *msg ) {
	const char *s = NULL;
	qboolean fromserver = NET_CompareAdr( from, &clc.serverAddress );

	// NOTE: we may have to add exceptions for auth and update servers
	if ( !fromserver && !NET_CompareAdr( from, &rcon_address ) ) {
		//if ( com_developer->integer ) {
			Com_Printf( "Got erroneous print packet from %s\n", NET_AdrToStringwPort( from ) );
		//}
		return fromserver;
	}

	s = MSG_ReadBigString( msg );

	if ( !Q_stricmpn( s, "[err_dialog]", 12 ) ) {
		Q_strncpyz( clc.serverMessage, s + 12, sizeof( clc.serverMessage ) );
		// Cvar_Set("com_errorMessage", clc.serverMessage );
		Com_Error( ERR_DROP, "%s", clc.serverMessage );
	} else if ( !Q_stricmpn( s, "[err_prot]", 10 ) )       {
		Q_strncpyz( clc.serverMessage, s + 10, sizeof( clc.serverMessage ) );
		// Cvar_Set("com_errorMessage", CL_TranslateStringBuf( PROTOCOL_MISMATCH_ERROR_LONG ) );
		Com_Error( ERR_DROP, "%s", CL_TranslateStringBuf( PROTOCOL_MISMATCH_ERROR_LONG ) );
	} else if ( !Q_stricmpn( s, "[err_update]", 12 ) )       {
		Q_strncpyz( clc.serverMessage, s + 12, sizeof( clc.serverMessage ) );
		Com_Error( ERR_AUTOUPDATE, "%s", clc.serverMessage );
	} else if ( fromserver && !Q_stricmpn( s, "ET://", 5 ) )       { // fretn
		Q_strncpyz( clc.serverMessage, s, sizeof( clc.serverMessage ) );
		Cvar_Set( "com_errorMessage", clc.serverMessage );
		Com_Error( ERR_DROP, "%s", clc.serverMessage );
	} else {
		Q_strncpyz( clc.serverMessage, s, sizeof( clc.serverMessage ) );
	}
	Com_Printf( "%s", clc.serverMessage );
	return fromserver;
}


/*
===================
CL_InitServerInfo
===================
*/
static void CL_InitServerInfo( serverInfo_t *server, const netadr_t *address ) {
	server->adr = *address;
	server->clients = 0;
	server->hostName[0] = '\0';
	server->mapName[0] = '\0';
	server->load = -1;
	server->maxClients = 0;
	server->maxPing = 0;
	server->minPing = 0;
	server->ping = -1;
	server->game[0] = '\0';
	server->gameType = 0;
	server->netType = 0;
	server->friendlyFire = 0;
	server->maxlives = 0;
	server->needpass = 0;
	server->punkbuster = 0;
	server->antilag = 0;
	server->weaprestrict = 0;
	server->balancedteams = 0;
	server->gameName[0] = '\0';
	server->oss = 0;
}

#define MAX_SERVERSPERPACKET	256

typedef struct hash_chain_s {
	netadr_t             addr;
	struct hash_chain_s *next;
} hash_chain_t;

static hash_chain_t *hash_table[1024];
static hash_chain_t hash_list[MAX_GLOBAL_SERVERS];
static unsigned int hash_count = 0;

static unsigned int hash_func( const netadr_t *addr ) {

	const byte		*ip = NULL;
	unsigned int	size;
	unsigned int	i;
	unsigned int	hash = 0;

	switch ( addr->type ) {
		case NA_IP:  ip = addr->ipv._4; size = 4;  break;
#ifdef USE_IPV6
		case NA_IP6: ip = addr->ipv._6; size = 16; break;
#endif
		default: size = 0; break;
	}

	for ( i = 0; i < size; i++ )
		hash = hash * 101 + (int)( *ip++ );

	hash = hash ^ ( hash >> 16 );

	return (hash & 1023);
}

static void hash_insert( const netadr_t *addr )
{
	hash_chain_t **tab, *cur;
	unsigned int hash;
	if ( hash_count >= MAX_GLOBAL_SERVERS )
		return;
	hash = hash_func( addr );
	tab = &hash_table[ hash ];
	cur = &hash_list[ hash_count++ ];
	cur->addr = *addr;
	if ( cur != *tab )
		cur->next = *tab;
	else
		cur->next = NULL;
	*tab = cur;
}

static void hash_reset( void )
{
	hash_count = 0;
	memset( hash_list, 0, sizeof( hash_list ) );
	memset( hash_table, 0, sizeof( hash_table ) );
}

static hash_chain_t *hash_find( const netadr_t *addr )
{
	hash_chain_t *cur;
	cur = hash_table[ hash_func( addr ) ];
	while ( cur != NULL ) {
		if ( NET_CompareAdr( addr, &cur->addr ) )
			return cur;
		cur = cur->next;
	}
	return NULL;
}


/*
===================
CL_ServersResponsePacket
===================
*/
static void CL_ServersResponsePacket( const netadr_t* from, msg_t *msg, qboolean extended ) {
	int				i, count, total;
	netadr_t addresses[MAX_SERVERSPERPACKET];
	int				numservers;
	byte*			buffptr;
	byte*			buffend;
	serverInfo_t	*server;

	Com_Printf("CL_ServersResponsePacket from %s\n", NET_AdrToStringwPort(from) );

	if (cls.numglobalservers == -1) {
		// state to detect lack of servers or lack of response
		cls.numglobalservers = 0;
		cls.numGlobalServerAddresses = 0;
		hash_reset();
	}

	// parse through server response string
	numservers = 0;
	buffptr    = msg->data;
	buffend    = buffptr + msg->cursize;

	// advance to initial token
	do
	{
		if(*buffptr == '\\' || (extended && *buffptr == '/'))
			break;

		buffptr++;
	} while (buffptr < buffend);

	while (buffptr + 1 < buffend)
	{
		// IPv4 address
		if (*buffptr == '\\')
		{
			buffptr++;

			if (buffend - buffptr < sizeof(addresses[numservers].ipv._4) + sizeof(addresses[numservers].port) + 1)
				break;

			for(i = 0; i < sizeof(addresses[numservers].ipv._4); i++)
				addresses[numservers].ipv._4[i] = *buffptr++;

			addresses[numservers].type = NA_IP;
		}
#ifdef USE_IPV6
		// IPv6 address, if it's an extended response
		else if (extended && *buffptr == '/')
		{
			buffptr++;

			if (buffend - buffptr < sizeof(addresses[numservers].ipv._6) + sizeof(addresses[numservers].port) + 1)
				break;

			for(i = 0; i < sizeof(addresses[numservers].ipv._6); i++)
				addresses[numservers].ipv._6[i] = *buffptr++;

			addresses[numservers].type = NA_IP6;
			addresses[numservers].scope_id = from->scope_id;
		}
#endif
		else
			// syntax error!
			break;

		// parse out port
		addresses[numservers].port = (*buffptr++) << 8;
		addresses[numservers].port += *buffptr++;
		addresses[numservers].port = BigShort( addresses[numservers].port );

		// syntax check
		if (*buffptr != '\\' && *buffptr != '/')
			break;

		numservers++;
		if (numservers >= MAX_SERVERSPERPACKET)
			break;

		// parse out EOT
		if ( buffptr[1] == 'E' && buffptr[2] == 'O' && buffptr[3] == 'T' ) {
			break;
		}
	}

	count = cls.numglobalservers;

	for (i = 0; i < numservers && count < MAX_GLOBAL_SERVERS; i++) {

		// Tequila: It's possible to have sent many master server requests. Then
		// we may receive many times the same addresses from the master server.
		// We just avoid to add a server if it is still in the global servers list.
		if ( hash_find( &addresses[i] ) )
			continue;

		hash_insert( &addresses[i] );

		// build net address
		server = &cls.globalServers[count];

		CL_InitServerInfo( server, &addresses[i] );
		// advance to next slot
		count++;
	}

	// if getting the global list
	if ( count >= MAX_GLOBAL_SERVERS && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS )
	{
		// if we couldn't store the servers in the main list anymore
		for (; i < numservers && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS; i++)
		{
			// just store the addresses in an additional list
			cls.globalServerAddresses[cls.numGlobalServerAddresses++] = addresses[i];
		}
	}

	cls.numglobalservers = count;
	total = count + cls.numGlobalServerAddresses;

	Com_Printf( "%d servers parsed (total %d)\n", numservers, total);
}


/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc

return true only for commands indicating that our server is alive
or connection sequence is going into the right way
=================
*/
static qboolean CL_ConnectionlessPacket( const netadr_t *from, msg_t *msg ) {
	const char *s;
	const char *c;
	int challenge = 0;

	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );	// skip the -1

	s = MSG_ReadStringLine( msg );

	Cmd_TokenizeString( s );

	c = Cmd_Argv(0);

	if ( com_developer->integer ) {
		Com_Printf( "CL packet %s: %s\n", NET_AdrToStringwPort( from ), s );
	}

	// challenge from the server we are connecting to
	if ( !Q_stricmp(c, "challengeResponse" ) ) {

		if ( cls.state != CA_CONNECTING ) {
			Com_DPrintf( "Unwanted challenge response received. Ignored.\n" );
			return qfalse;
		}

		c = Cmd_Argv( 3 );
		if ( *c != '\0' )
			challenge = atoi( c );

		clc.compat = qtrue;
		s = Cmd_Argv( 4 ); // analyze server protocol version
		if ( *s != '\0' ) {
			int sv_proto = atoi( s );
			if ( sv_proto > OLD_PROTOCOL_VERSION ) {
				if ( sv_proto == NEW_PROTOCOL_VERSION || sv_proto == com_protocol->integer ) {
					clc.compat = qfalse;
				} else {
					int cl_proto = com_protocol->integer;
					if ( cl_proto == DEFAULT_PROTOCOL_VERSION ) {
						// we support new protocol features by default
						cl_proto = NEW_PROTOCOL_VERSION;
					}
					Com_Printf( S_COLOR_YELLOW "Warning: Server reports protocol version %d, "
						"we have %d. Trying legacy protocol %d.\n",
						sv_proto, cl_proto, OLD_PROTOCOL_VERSION );
				}
			}
		}

		if ( clc.compat )
		{
			if ( !NET_CompareAdr( from, &clc.serverAddress ) )
			{
				// This challenge response is not coming from the expected address.
				// Check whether we have a matching client challenge to prevent
				// connection hi-jacking.
				if ( *c == '\0' || challenge != clc.challenge )
				{
					Com_DPrintf( "Challenge response received from unexpected source. Ignored.\n" );
					return qfalse;
				}
			}
		}
		else
		{
			if ( *c == '\0' || challenge != clc.challenge )
			{
				Com_Printf( "Bad challenge for challengeResponse. Ignored.\n" );
				return qfalse;
			}
		}

		if ( Cmd_Argc() > 2 ) {
			clc.onlyVisibleClients = atoi( Cmd_Argv( 2 ) );         // DHM - Nerve
		} else {
			clc.onlyVisibleClients = 0;
		}

		// start sending connect instead of challenge request packets
		clc.challenge = atoi(Cmd_Argv(1));
		cls.state = CA_CHALLENGING;
		clc.connectPacketCount = 0;
		clc.connectTime = -99999;

		// take this address as the new server address.  This allows
		// a server proxy to hand off connections to multiple servers
		clc.serverAddress = *from;
		Com_DPrintf( "challengeResponse: %d\n", clc.challenge );
		return qtrue;
	}

	// server connection
	if ( !Q_stricmp(c, "connectResponse") ) {
		if ( cls.state >= CA_CONNECTED ) {
			Com_Printf( "Dup connect received. Ignored.\n" );
			return qfalse;
		}
		if ( cls.state != CA_CHALLENGING ) {
			Com_Printf( "connectResponse packet while not connecting. Ignored.\n" );
			return qfalse;
		}
		if ( !NET_CompareAdr( from, &clc.serverAddress ) ) {
			Com_Printf( "connectResponse from wrong address. Ignored.\n" );
			return qfalse;
		}

		if ( !clc.compat ) {
			// first argument: challenge response
			c = Cmd_Argv( 1 );
			if ( *c != '\0' ) {
				challenge = atoi( c );
			} else {
				Com_Printf( "Bad connectResponse received. Ignored.\n" );
				return qfalse;
			}

			if ( challenge != clc.challenge ) {
				Com_Printf( "ConnectResponse with bad challenge received. Ignored.\n" );
				return qfalse;
			}

			if ( com_protocolCompat ) {
				// enforce dm68-compatible stream for legacy/unknown servers
				clc.compat = qtrue;
			}

			// second (optional) argument: actual protocol version used on server-side
			c = Cmd_Argv( 2 );
			if ( *c != '\0' ) {
				int protocol = atoi( c );
				if ( protocol > 0 ) {
					if ( protocol <= OLD_PROTOCOL_VERSION ) {
						clc.compat = qtrue;
					} else {
						clc.compat = qfalse;
					}
				}
			}
		}

		Netchan_Setup( NS_CLIENT, &clc.netchan, from, Cvar_VariableIntegerValue( "net_qport" ), clc.challenge, clc.compat );

		cls.state = CA_CONNECTED;
		clc.lastPacketSentTime = -9999;		// send first packet immediately
		return qtrue;
	}

	// server responding to an info broadcast
	if ( !Q_stricmp(c, "infoResponse") ) {
		CL_ServerInfoPacket( from, msg );
		return qfalse;
	}

	// server responding to a get playerlist
	if ( !Q_stricmp(c, "statusResponse") ) {
		CL_ServerStatusResponse( from, msg );
		return qfalse;
	}

	// a disconnect message from the server, which will happen if the server
	// dropped the connection but it is still getting packets from us
	if ( !Q_stricmp( c, "disconnect" ) ) {
		CL_DisconnectPacket( from );
		return qfalse;
	}

	// echo request from server
	if ( !Q_stricmp(c, "echo") ) {
		qboolean fromserver;
		// NOTE: we may have to add exceptions for auth and update servers
		if ( (fromserver = NET_CompareAdr( from, &clc.serverAddress )) != qfalse || NET_CompareAdr( from, &rcon_address ) ) {
			NET_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv(1) );
		}
		return fromserver;
	}

	// cd check
	if ( !Q_stricmp(c, "keyAuthorize") ) {
		// we don't use these now, so dump them on the floor
		return qfalse;
	}

	// global MOTD from id
	if ( !Q_stricmp(c, "motd") ) {
		CL_MotdPacket( from );
		return qfalse;
	}

	// print string from server
	if ( !Q_stricmp(c, "print") ) {
		return CL_PrintPacket( from, msg );
	}

	// DHM - Nerve :: Auto-update server response message
	//if ( !Q_stricmp( c, "updateResponse" ) ) {
//		CL_UpdateInfoPacket( from );
//		return;
//	}
	// DHM - Nerve

	// NERVE - SMF - bugfix, make this compare first n chars so it doesnt bail if token is parsed incorrectly
	// list of servers sent back by a master server (classic)
	if ( !Q_strncmp(c, "getserversResponse", 18) ) {
		CL_ServersResponsePacket( from, msg, qfalse );
		return qfalse;
	}

	// list of servers sent back by a master server (extended)
	if ( !Q_strncmp(c, "getserversExtResponse", 21) ) {
		CL_ServersResponsePacket( from, msg, qtrue );
		return qfalse;
	}

	Com_DPrintf( "Unknown connectionless packet command.\n" );
	return qfalse;
}


/*
=================
CL_PacketEvent

A packet has arrived from the main event loop
=================
*/
void CL_PacketEvent( const netadr_t *from, msg_t *msg ) {
	int		headerBytes;

	if ( msg->cursize < 5 ) {
		Com_DPrintf( "%s: Runt packet\n", NET_AdrToStringwPort( from ) );
		return;
	}

	if ( *(int *)msg->data == -1 ) {
		if ( CL_ConnectionlessPacket( from, msg ) )
			clc.lastPacketTime = cls.realtime;
		return;
	}

	if ( cls.state < CA_CONNECTED ) {
		return;		// can't be a valid sequenced packet
	}

	//
	// packet from server
	//
	if ( !NET_CompareAdr( from, &clc.netchan.remoteAddress ) ) {
		if ( com_developer->integer ) {
			Com_Printf( "%s:sequenced packet without connection\n",
				NET_AdrToStringwPort( from ) );
		}
		// FIXME: send a client disconnect?
		return;
	}

	if ( !CL_Netchan_Process( &clc.netchan, msg ) ) {
		return;		// out of order, duplicated, etc
	}

	// the header is different lengths for reliable and unreliable messages
	headerBytes = msg->readcount;

	// track the last message received so it can be returned in
	// client messages, allowing the server to detect a dropped
	// gamestate
	clc.serverMessageSequence = LittleLong( *(int32_t *)msg->data );

	clc.lastPacketTime = cls.realtime;
	CL_ParseServerMessage( msg );

	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//
	if ( clc.demorecording && !clc.demowaiting && !clc.demoplaying ) {
		CL_WriteDemoMessage( msg, headerBytes );
	}
}


/*
==================
CL_CheckTimeout
==================
*/
static void CL_CheckTimeout( void ) {
	//
	// check timeout
	//
	if ( ( !CL_CheckPaused() || !sv_paused->integer )
		&& cls.state >= CA_CONNECTED && cls.state != CA_CINEMATIC
		&& cls.realtime - clc.lastPacketTime > cl_timeout->integer * 1000 ) {
		if ( ++cl.timeoutcount > 5 ) { // timeoutcount saves debugger
			Com_Printf( "\nServer connection timed out.\n" );
			Cvar_Set( "com_errorMessage", "Server connection timed out." );
			if ( !CL_Disconnect( qfalse ) ) { // restart client if not done already
				CL_FlushMemory();
			}
			if ( uivm ) {
				VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
			}
			return;
		}
	} else {
		cl.timeoutcount = 0;
	}
}


/*
==================
CL_CheckPaused
Check whether client has been paused.
==================
*/
qboolean CL_CheckPaused( void )
{
	// if cl_paused->modified is set, the cvar has only been changed in
	// this frame. Keep paused in this frame to ensure the server doesn't
	// lag behind.
	if(cl_paused->integer || cl_paused->modified)
		return qtrue;

	return qfalse;
}


/*
==================
CL_NoDelay
==================
*/
qboolean CL_NoDelay( void )
{
	if ( CL_VideoRecording() != AVIDEMO_NONE || ( com_timedemo->integer && clc.demofile != FS_INVALID_HANDLE ) )
		return qtrue;

	return qfalse;
}


/*
==================
CL_ConnectedToRemoteServer
==================
*/
qboolean CL_ConnectedToRemoteServer( void )
{
	if ( (!com_sv_running || !com_sv_running->integer) && cls.state >= CA_CONNECTED && !clc.demoplaying )
		return qtrue;

	return qfalse;
}


/*
==================
CL_CheckUserinfo
==================
*/
static void CL_CheckUserinfo( void ) {

	// don't add reliable commands when not yet connected
	if ( cls.state < CA_CONNECTED )
		return;

	// don't overflow the reliable command buffer when paused
	if ( CL_CheckPaused() )
		return;

	// send a reliable userinfo update if needed
	if ( cvar_modifiedFlags & CVAR_USERINFO )
	{
		qboolean infoTruncated = qfalse;
		const char *info;

		cvar_modifiedFlags &= ~CVAR_USERINFO;

		info = Cvar_InfoString( CVAR_USERINFO, &infoTruncated );
		if ( strlen( info ) > MAX_USERINFO_LENGTH || infoTruncated ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: oversize userinfo, you might be not able to play on remote server!\n" );
		}

		CL_AddReliableCommand( va( "userinfo \"%s\"", info ), qfalse );
	}
}

/*
==================
CL_WWWDownload
==================
*/
void CL_WWWDownload( void ) {
	char *to_ospath;
	dlStatus_t ret;
	static qboolean bAbort = qfalse;

	if ( clc.bWWWDlAborting ) {
		if ( !bAbort ) {
			Com_DPrintf( "CL_WWWDownload: WWWDlAborting\n" );
			bAbort = qtrue;
		}
		return;
	}
	if ( bAbort ) {
		Com_DPrintf( "CL_WWWDownload: WWWDlAborting done\n" );
		bAbort = qfalse;
	}

	ret = DL_DownloadLoop();

	if ( ret == DL_CONTINUE ) {
		return;
	}

	if ( ret == DL_DONE ) {
		// taken from CL_ParseDownload
		// we work with OS paths
		clc.download = 0;
		to_ospath = FS_BuildOSPath( Cvar_VariableString( "fs_homepath" ), cls.originalDownloadName, "" );
		to_ospath[strlen( to_ospath ) - 1] = '\0';
		if ( rename( cls.downloadTempName, to_ospath ) ) {
			FS_CopyFile( cls.downloadTempName, to_ospath );
			remove( cls.downloadTempName );
		}
		*cls.downloadTempName = *cls.downloadName = 0;
		Cvar_Set( "cl_downloadName", "" );
		if ( cls.bWWWDlDisconnected ) {
			// reconnect to the server, which might send us to a new disconnected download
			Cbuf_ExecuteText( EXEC_APPEND, "reconnect\n" );
		} else {
			CL_AddReliableCommand( "wwwdl done", qfalse );
			// tracking potential web redirects leading us to wrong checksum - only works in connected mode
			if ( strlen( clc.redirectedList ) + strlen( cls.originalDownloadName ) + 1 >= sizeof( clc.redirectedList ) ) {
				// just to be safe
				Com_Printf( "ERROR: redirectedList overflow (%s)\n", clc.redirectedList );
			} else {
				Q_strcat( clc.redirectedList, sizeof( clc.redirectedList), "@" );
				Q_strcat( clc.redirectedList, sizeof( clc.redirectedList), cls.originalDownloadName );
			}
		}
	} else
	{
		if ( cls.bWWWDlDisconnected ) {
			// in a connected download, we'd tell the server about failure and wait for a reply
			// but in this case we can't get anything from server
			// if we just reconnect it's likely we'll get the same disconnected download message, and error out again
			// this may happen for a regular dl or an auto update
			const char *error = va( "Download failure while getting '%s'", cls.downloadName ); // get the msg before clearing structs
			cls.bWWWDlDisconnected = qfalse; // need clearing structs before ERR_DROP, or it goes into endless reload
			CL_ClearStaticDownload();
			Com_Error( ERR_DROP, "%s", error );
		} else {
			// see CL_ParseDownload, same abort strategy
			Com_Printf( "Download failure while getting '%s'\n", cls.downloadName );
			CL_AddReliableCommand( "wwwdl fail", qfalse );
			clc.bWWWDlAborting = qtrue;
		}
		return;
	}

	clc.bWWWDl = qfalse;
	CL_NextDownload();
}

/*
==================
CL_WWWBadChecksum

FS code calls this when doing FS_ComparePaks
we can detect files that we got from a www dl redirect with a wrong checksum
this indicates that the redirect setup is broken, and next dl attempt should NOT redirect
==================
*/
qboolean CL_WWWBadChecksum( const char *pakname ) {
	if ( strstr( clc.redirectedList, va( "@%s", pakname ) ) ) {
		Com_Printf( "WARNING: file %s obtained through download redirect has wrong checksum\n", pakname );
		Com_Printf( "         this likely means the server configuration is broken\n" );
		if ( strlen( clc.badChecksumList ) + strlen( pakname ) + 1 >= sizeof( clc.badChecksumList ) ) {
			Com_Printf( "ERROR: badChecksumList overflowed (%s)\n", clc.badChecksumList );
			return qfalse;
		}
		Q_strcat( clc.badChecksumList, sizeof(clc.badChecksumList), "@" );
		Q_strcat( clc.badChecksumList, sizeof(clc.badChecksumList), pakname );
		Com_DPrintf( "bad checksums: %s\n", clc.badChecksumList );
		return qtrue;
	}
	return qfalse;
}

/*
==================
CL_Frame
==================
*/
void CL_Frame( int msec, int realMsec ) {
	float fps;
	float frameDuration;
	aviRecordingState_t aviRecord = AVIDEMO_NONE;
	int64_t frameStart = 0;

	if (clc.demoplaying && com_timedemo && com_timedemo->integer) {
		frameStart = Sys_Microseconds();
	}

	CL_TrackCvarChanges( qfalse );

#ifdef USE_CURL
	if ( download.cURL ) {
		Com_DL_Perform( &download );
	}
#endif

	if ( !com_cl_running->integer ) {
		return;
	}

	// save the msec before checking pause
	cls.realFrametime = realMsec;

	if ( cls.cddialog ) {
		// bring up the cd error dialog if needed
		cls.cddialog = qfalse;
		VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_NEED_CD );
	} else	if ( cls.state == CA_DISCONNECTED && !( Key_GetCatcher( ) & KEYCATCH_UI )
		&& !com_sv_running->integer && uivm ) {
		// if disconnected, bring up the menu
		S_StopAllSounds();
		VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
	}

	aviRecord = CL_VideoRecording();
	// if recording an avi, lock to a fixed fps
	if ( aviRecord != AVIDEMO_NONE && msec ) {
		// save the current screen
		if ( cls.state == CA_ACTIVE || cl_forceavidemo->integer ) {

			if ( com_timescale->value > 0.0001f )
				fps = MIN( (aviRecord == AVIDEMO_CVAR ? cl_avidemo->value : cl_aviFrameRate->value) / com_timescale->value, 1000.0f );
			else
				fps = 1000.0f;

			frameDuration = MAX( 1000.0f / fps, 1.0f ) + clc.aviVideoFrameRemainder;

			if ( aviRecord == AVIDEMO_CVAR )
				Cbuf_ExecuteText( EXEC_NOW, "screenshot silent\n" );
			else
				CL_TakeVideoFrame();

			msec = (int)frameDuration;
			clc.aviVideoFrameRemainder = frameDuration - msec;

			realMsec = msec; // sync sound duration
		}
	}

	if ( cl_autoRecordDemo->integer && !clc.demoplaying ) {
		if ( cls.state == CA_ACTIVE && !clc.demorecording ) {
			// If not recording a demo, and we should be, start one
			qtime_t	now;
			const char	*nowString;
			char		*p;
			char		mapName[ MAX_QPATH ];
			char		serverName[ MAX_OSPATH ];

			Com_RealTime( &now );
			nowString = va( "%04d%02d%02d%02d%02d%02d",
					1900 + now.tm_year,
					1 + now.tm_mon,
					now.tm_mday,
					now.tm_hour,
					now.tm_min,
					now.tm_sec );

			Q_strncpyz( serverName, cls.servername, MAX_OSPATH );
			// Replace the ":" in the address as it is not a valid
			// file name character
			p = strchr( serverName, ':' );
			if( p ) {
				*p = '.';
			}

			Q_strncpyz( mapName, COM_SkipPath( cl.mapname ), sizeof( cl.mapname ) );
			COM_StripExtension(mapName, mapName, sizeof(mapName));

			Cbuf_ExecuteText( EXEC_NOW,
					va( "record %s-%s-%s", nowString, serverName, mapName ) );
		}
		else if( cls.state != CA_ACTIVE && clc.demorecording ) {
			// Recording, but not CA_ACTIVE, so stop recording
			CL_StopRecord_f( );
		}
	}

	// decide the simulation time
	cls.frametime = msec;
	cls.realtime += msec;

	if ( cl_timegraph->integer ) {
		SCR_DebugGraph( msec * 0.25f );
	}

	// see if we need to update any userinfo
	CL_CheckUserinfo();

	// if we haven't gotten a packet in a long time, drop the connection
	if ( !clc.demoplaying ) {
		CL_CheckTimeout();
	}

	// wwwdl download may survive a server disconnect
	if ( ( cls.state == CA_CONNECTED && clc.bWWWDl ) || cls.bWWWDlDisconnected ) {
		CL_WWWDownload();
	}

	// send intentions now
	CL_SendCmd();

	// resend a connection request if necessary
	CL_CheckForResend();

	// decide on the serverTime to render
	CL_SetCGameTime();

	// update the screen
	cls.framecount++;
	SCR_UpdateScreen();

	// update audio
	S_Update( realMsec );

	// advance local effects for next frame
	SCR_RunCinematic();

	Con_RunConsole();

#ifdef USE_DISCORD
	if (cl_discordRichPresence->integer) {
		if ( cls.realtime >= 5000 && !cls.discordInit )
		{ //we just turned it on
			CL_DiscordInitialize();
			cls.discordInit = qtrue;
		}
	
		if ( cls.realtime >= cls.discordUpdateTime && cls.discordInit)
		{
			CL_DiscordUpdatePresence();
			cls.discordUpdateTime = cls.realtime + 500;
		}
	}
	else if (cls.discordInit) { //we just turned it off
		CL_DiscordShutdown();
		cls.discordUpdateTime = 0;
		cls.discordInit = qfalse;
	}
#endif

	if (clc.demoplaying && clc.timedemo.timeDemoFrames && com_timedemo && com_timedemo->integer) {
		// use circular buffer to store frametimes, the array should hold enough frames for ~3h of demo playback,
		// but in case of extremely long demos, just start dropping frames from the beginning
		clc.timedemo.frametime[(clc.timedemo.timeDemoFrames - 1) % MAX_TIMEDEMO_FRAMES] = Sys_Microseconds() - frameStart;
	}
}


//============================================================================
// Ridah, startup-caching system
typedef struct {
	char name[MAX_QPATH];
	int hits;
	int lastSetIndex;
} cacheItem_t;
typedef enum {
	CACHE_SOUNDS = 0,
	CACHE_MODELS,
	CACHE_IMAGES,

	CACHE_NUMGROUPS
} cacheGroup_t;
static const char *cacheGroups[CACHE_NUMGROUPS] = {
	"sound",
	"model",
	"image"
};
#define MAX_CACHE_ITEMS     4096
#define CACHE_HIT_RATIO     0.75        // if hit on this percentage of maps, it'll get cached

static int cacheIndex;
static cacheItem_t cacheItems[CACHE_NUMGROUPS][MAX_CACHE_ITEMS];

static void CL_Cache_StartGather_f( void ) {
	cacheIndex = 0;
	memset( cacheItems, 0, sizeof( cacheItems ) );

	Cvar_Set( "cl_cacheGathering", "1" );
}

static cacheGroup_t CacheGroupFromString( const char *str ) {
	if ( !Q_stricmp( str, cacheGroups[0] ) )
		return CACHE_SOUNDS;
	else if ( !Q_stricmp( str, cacheGroups[1] ) )
		return CACHE_MODELS;
	else if ( !Q_stricmp( str, cacheGroups[2] ) )
		return CACHE_IMAGES;
	else
		return CACHE_NUMGROUPS;
}

static void CL_Cache_UsedFile_f( void ) {
	char itemStr[MAX_QPATH];
	int i;
	cacheGroup_t group;
	cacheItem_t *item;

	if ( Cmd_Argc() < 2 ) {
		Com_Error( ERR_DROP, "usedfile without enough parameters" );
		return;
	}

	Q_strncpyz( itemStr, Cmd_ArgsFrom( 2 ), sizeof( itemStr ) );
	Q_strlwr( itemStr );

	// find the cache group
	group = CacheGroupFromString( Cmd_Argv( 1 ) );
	if ( /*group < CACHE_SOUNDS ||*/ group >= CACHE_NUMGROUPS ) {
		Com_Error( ERR_DROP, "usedfile without a valid cache group" );
	}

	// see if it's already there
	for ( i = 0, item = cacheItems[group]; i < MAX_CACHE_ITEMS; i++, item++ ) {
		if ( !item->name[0] ) {
			// didn't find it, so add it here
			Q_strncpyz( item->name, itemStr, sizeof(item->name) );
			if ( cacheIndex > 9999 ) { // hack, but yeh
				item->hits = cacheIndex;
			} else {
				item->hits++;
			}
			item->lastSetIndex = cacheIndex;
			break;
		}
		if ( item->name[0] == itemStr[0] && !strcmp( item->name, itemStr ) ) {
			if ( item->lastSetIndex != cacheIndex ) {
				item->hits++;
				item->lastSetIndex = cacheIndex;
			}
			break;
		}
	}
}

static void CL_Cache_SetIndex_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Error( ERR_DROP, "setindex needs an index" );
		return;
	}

	cacheIndex = atoi( Cmd_Argv( 1 ) );
}

static void CL_Cache_MapChange_f( void ) {
	cacheIndex++;
}

static void CL_Cache_EndGather_f( void ) {
	// save the frequently used files to the cache list file
	int i, j, handle, cachePass;
	char filename[MAX_QPATH];

	cachePass = (int)floor( (float)cacheIndex * CACHE_HIT_RATIO );

	for ( i = 0; i < CACHE_NUMGROUPS; i++ ) {
		Com_sprintf( filename, sizeof( filename ), "%s.cache", cacheGroups[i] );

		handle = FS_FOpenFileWrite( filename );

		for ( j = 0; j < MAX_CACHE_ITEMS; j++ ) {
			// if it's a valid filename, and it's been hit enough times, cache it
			if ( cacheItems[i][j].hits >= cachePass && strchr( cacheItems[i][j].name, '/' ) ) {
				FS_Write( cacheItems[i][j].name, strlen( cacheItems[i][j].name ), handle );
				FS_Write( "\n", 1, handle );
			}
		}

		FS_FCloseFile( handle );
	}

	Cvar_Set( "cl_cacheGathering", "0" );
}

// done.
//============================================================================

/*
================
CL_SetRecommended_f
================
*/
static void CL_SetRecommended_f( void ) {
	Com_SetRecommended();
}


/*
================
CL_RefPrintf
================
*/
static FORMAT_PRINTF(2, 3) void QDECL CL_RefPrintf( printParm_t level, const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	switch ( level ) {
		default: Com_Printf( "%s", msg ); break;
		case PRINT_DEVELOPER: Com_DPrintf( "%s", msg ); break;
		case PRINT_WARNING: Com_Printf( S_COLOR_YELLOW "%s", msg ); break;
		case PRINT_ERROR: Com_Printf( S_COLOR_RED "%s", msg ); break;
	}
}


/*
============
CL_ShutdownRef
============
*/
static void CL_ShutdownRef( refShutdownCode_t code ) {

#ifdef USE_RENDERER_DLOPEN
	if ( cl_renderer->modified ) {
		code = REF_UNLOAD_DLL;
	}
#endif

	// clear and mute all sounds until next registration
	// S_DisableSounds();

	if ( code >= REF_DESTROY_WINDOW ) { // +REF_UNLOAD_DLL
		// shutdown sound system before renderer
		// because it may depend from window handle
		S_Shutdown();
	}

	SCR_Done();

	if ( re.Shutdown ) {
		re.Shutdown( code );
	}

#ifdef USE_RENDERER_DLOPEN
	if ( rendererLib ) {
		Sys_UnloadLibrary( rendererLib );
		rendererLib = NULL;
	}
#endif

	Com_Memset( &re, 0, sizeof( re ) );

	cls.rendererStarted = qfalse;
}


/*
============
CL_InitRenderer
============
*/
static void CL_InitRenderer( void ) {

	// fixup renderer -EC-
	if ( !re.BeginRegistration ) {
		CL_InitRef();
	}

	// this sets up the renderer and calls R_Init
	re.BeginRegistration( &cls.glconfig );

	// load character sets
	cls.charSetShader = re.RegisterShader( "gfx/2d/consolechars" );
	cls.whiteShader = re.RegisterShader( "white" );

// JPW NERVE

	cls.consoleShader = re.RegisterShader( "console-16bit" ); // JPW NERVE shader works with 16bit
	cls.consoleShader2 = re.RegisterShader( "console2-16bit" ); // JPW NERVE same

	Con_CheckResize();

	g_console_field_width = ((cls.glconfig.vidWidth / smallchar_width)) - 2;
	g_consoleField.widthInChars = g_console_field_width;

	// for 640x480 virtualized screen
	cls.biasY = 0;
	cls.biasX = 0;
	if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
		// wide screen, scale by height
		cls.scale = cls.glconfig.vidHeight * (1.0/480.0);
		cls.biasX = 0.5 * ( cls.glconfig.vidWidth - ( cls.glconfig.vidHeight * (640.0/480.0) ) );
	} else {
		// no wide screen, scale by width
		cls.scale = cls.glconfig.vidWidth * (1.0/640.0);
		cls.biasY = 0.5 * ( cls.glconfig.vidHeight - ( cls.glconfig.vidWidth * (480.0/640) ) );
	}

	SCR_Init();
}


/*
============================
CL_StartHunkUsers

After the server has cleared the hunk, these will need to be restarted
This is the only place that any of these functions are called from
============================
*/
void CL_StartHunkUsers( void ) {

	if ( !com_cl_running || !com_cl_running->integer ) {
		return;
	}

	if ( cls.state >= CA_LOADING ) {
		// try to apply map-depending configuration from cvar cl_mapConfig_<mapname> cvars
		const char *info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
		const char *mapname = Info_ValueForKey( info, "mapname" );
		if ( mapname && *mapname != '\0' ) {
			const char *fmt = "cl_mapConfig_%s";
			const char *cmd = Cvar_VariableString( va( fmt, mapname ) );
			if ( cmd && *cmd != '\0' ) {
				Cbuf_AddText( cmd );
				Cbuf_AddText( "\n" );
			} else {
				// apply mapname "default" if present
				cmd = Cvar_VariableString( va( fmt, "default" ) );
				if ( cmd && *cmd != '\0' ) {
					Cbuf_AddText( cmd );
					Cbuf_AddText( "\n" );
				}
			}
		}
	}

	if ( !cls.rendererStarted ) {
		cls.rendererStarted = qtrue;
		CL_InitRenderer();
	}

	if ( !cls.soundStarted ) {
		cls.soundStarted = qtrue;
		S_Init();
	}

	if ( !cls.soundRegistered ) {
		cls.soundRegistered = qtrue;
		S_BeginRegistration();
	}

	if ( !cls.uiStarted ) {
		cls.uiStarted = qtrue;
		CL_InitUI();
	}
}


/*
============
CL_RefMalloc
============
*/
static void *CL_RefMalloc( int size ) {
	return Z_TagMalloc( size, TAG_RENDERER );
}


/*
============
CL_RefTagFree
============
*/
static void CL_RefTagFree( void ) {
	Z_FreeTags( TAG_RENDERER );
}


/*
============
CL_ScaledMilliseconds
============
*/
int CL_ScaledMilliseconds( void ) {
	return Sys_Milliseconds()*com_timescale->value;
}


/*
============
CL_IsMinimized
============
*/
static qboolean CL_IsMininized( void ) {
	return gw_minimized;
}

static int CL_CurrentGameMod( void ) {
	return currentGameMod;
}


/*
============
CL_SetScaling

Sets console chars height
============
*/
static void CL_SetScaling( float factor, int captureWidth, int captureHeight ) {

	if ( cls.con_factor != factor ) {
		// rescale console
		con_scale->modified = qtrue;
	}

	cls.con_factor = factor;

	// set custom capture resolution
	cls.captureWidth = captureWidth;
	cls.captureHeight = captureHeight;
}

static void Ref_Cmd_RegisterList( const cmdListItem_t *cmds, int count ) {
	Cmd_RegisterList( cmds, count, MODULE_RENDERER );
}

static void Ref_Cmd_UnregisterModule( void ) {
	Cmd_UnregisterModule( MODULE_RENDERER );
}


/*
============
CL_InitRef
============
*/
static void CL_InitRef( void ) {
	refimport_t	rimp;
	refexport_t	*ret;
#ifdef USE_RENDERER_DLOPEN
	GetRefAPI_t		GetRefAPI;
	char			dllName[ MAX_OSPATH ];
#endif

	CL_InitGLimp_Cvars();

	Com_Printf( "----- Initializing Renderer ----\n" );

#ifdef USE_RENDERER_DLOPEN

#if defined (__linux__) && id386
#define REND_ARCH_STRING "x86"
#else
#define REND_ARCH_STRING ARCH_STRING
#endif

	Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s_" REND_ARCH_STRING REN_DLL_EXT, cl_renderer->string );
	rendererLib = FS_LoadLibrary( dllName );
	if ( !rendererLib )
	{
		Cvar_ForceReset( "cl_renderer" );
		Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s_" REND_ARCH_STRING REN_DLL_EXT, cl_renderer->string );
		rendererLib = FS_LoadLibrary( dllName );
		if ( !rendererLib )
		{
			Com_Error( ERR_FATAL, "Failed to load renderer %s", dllName );
		}
	}

	GetRefAPI = Sys_LoadFunction( rendererLib, "GetRefAPI" );
	if( !GetRefAPI )
	{
		Com_Error( ERR_FATAL, "Can't load symbol GetRefAPI" );
		return;
	}

	cl_renderer->modified = qfalse;
#endif

	Com_Memset( &rimp, 0, sizeof( rimp ) );

	rimp.Cmd_AddCommand = Cmd_AddCommand;
	rimp.Cmd_RemoveCommand = Cmd_RemoveCommand;
	rimp.Cmd_RegisterList = Ref_Cmd_RegisterList;
	rimp.Cmd_UnregisterModule = Ref_Cmd_UnregisterModule;
	rimp.Cmd_Argc = Cmd_Argc;
	rimp.Cmd_Argv = Cmd_Argv;
	rimp.Cmd_ExecuteText = Cbuf_ExecuteText;
	rimp.Printf = CL_RefPrintf;
	rimp.Error = Com_Error;
	rimp.Milliseconds = CL_ScaledMilliseconds;
	rimp.Microseconds = Sys_Microseconds;
	rimp.Malloc = CL_RefMalloc;
	rimp.Free = Z_Free;
	rimp.Tag_Free = CL_RefTagFree;
	rimp.Hunk_Clear = Hunk_ClearToMark;
#ifdef HUNK_DEBUG
	rimp.Hunk_AllocDebug = Hunk_AllocDebug;
#else
	rimp.Hunk_Alloc = Hunk_Alloc;
#endif
	rimp.Hunk_AllocateTempMemory = Hunk_AllocateTempMemory;
	rimp.Hunk_FreeTempMemory = Hunk_FreeTempMemory;

	rimp.CM_ClusterPVS = CM_ClusterPVS;
	rimp.CM_DrawDebugSurface = CM_DrawDebugSurface;

	rimp.FS_ReadFile = FS_ReadFile;
	rimp.FS_FreeFile = FS_FreeFile;
	rimp.FS_WriteFile = FS_WriteFile;
	rimp.FS_FreeFileList = FS_FreeFileList;
	rimp.FS_ListFiles = FS_ListFiles;
	rimp.FS_ListFilesEx = FS_ListFilesEx;
	//rimp.FS_FileIsInPAK = FS_FileIsInPAK;
	rimp.FS_FileExists = FS_FileExists;
	rimp.FS_GetCurrentGameDir = FS_GetCurrentGameDir;
	rimp.CL_CurrentGameMod = CL_CurrentGameMod;

	rimp.Cvar_Get = Cvar_Get;
	rimp.Cvar_Set = Cvar_Set;
	rimp.Cvar_SetValue = Cvar_SetValue;
	rimp.Cvar_CheckRange = Cvar_CheckRange;
	rimp.Cvar_SetDescription = Cvar_SetDescription;
	rimp.Cvar_VariableStringBuffer = Cvar_VariableStringBuffer;
	rimp.Cvar_VariableString = Cvar_VariableString;
	rimp.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;

	rimp.Cvar_SetGroup = Cvar_SetGroup;
	rimp.Cvar_CheckGroup = Cvar_CheckGroup;
	rimp.Cvar_ResetGroup = Cvar_ResetGroup;

	// cinematic stuff

	rimp.CIN_UploadCinematic = CIN_UploadCinematic;
	rimp.CIN_PlayCinematic = CIN_PlayCinematic;
	rimp.CIN_RunCinematic = CIN_RunCinematic;

	rimp.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;
	rimp.CL_SaveJPGToBuffer = CL_SaveJPGToBuffer;
	rimp.CL_SaveJPG = CL_SaveJPG;
	rimp.CL_LoadJPG = CL_LoadJPG;

	rimp.CL_IsMinimized = CL_IsMininized;
	rimp.CL_SetScaling = CL_SetScaling;
	rimp.SCR_UpdateScreen = SCR_UpdateScreen;

	rimp.Sys_SetClipboardBitmap = Sys_SetClipboardBitmap;
	rimp.Sys_LowPhysicalMemory = Sys_LowPhysicalMemory;
	rimp.Sys_OmnibotRender = Sys_OmnibotRender;
	rimp.Com_RealTime = Com_RealTime;
	rimp.Com_Filter = Com_Filter;
	rimp.MSG_HashKey = MSG_HashKey;

	rimp.GLimp_InitGamma = GLimp_InitGamma;
	rimp.GLimp_SetGamma = GLimp_SetGamma;
	// OpenGL API
#ifdef USE_OPENGL_API
	rimp.GLimp_Init = GLimp_Init;
	rimp.GLimp_Shutdown = GLimp_Shutdown;
	rimp.GL_GetProcAddress = GL_GetProcAddress;
	rimp.GLimp_EndFrame = GLimp_EndFrame;
	rimp.GLimp_NormalFontBase = GLimp_NormalFontBase;
#endif

	// Vulkan API
#ifdef USE_VULKAN_API
	rimp.VKimp_Init = VKimp_Init;
	rimp.VKimp_Shutdown = VKimp_Shutdown;
	rimp.VK_GetInstanceProcAddr = VK_GetInstanceProcAddr;
	rimp.VK_CreateSurface = VK_CreateSurface;
#endif

	ret = GetRefAPI( REF_API_VERSION, &rimp );

	Com_Printf( "-------------------------------\n");

	if ( !ret ) {
		Com_Error (ERR_FATAL, "Couldn't initialize refresh" );
	}

	re = *ret;

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );
}

#if !defined(__APPLE__) && !defined(__APPLE_CC__)
static void CL_SaveTranslations_f( void ) {
	CL_SaveTransTable( "scripts/translation.cfg", qfalse );
}

static void CL_SaveNewTranslations_f( void ) {
	char fileName[MAX_QPATH];

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: SaveNewTranslations <filename>\n" );
		return;
	}

	Com_sprintf( fileName, sizeof(fileName), "translations/%s.cfg", Cmd_Argv( 1 ) );

	CL_SaveTransTable( fileName, qtrue );
}

static void CL_LoadTranslations_f( void ) {
	CL_ReloadTranslation();
}
// -NERVE - SMF
#endif

//===========================================================================================


/*
===============
CL_Video_f

video
video [filename]
===============
*/
static void CL_Video_f( void )
{
	char filename[ MAX_OSPATH ];
	const char *ext;
	qboolean pipe;
	int i;

	if( !clc.demoplaying )
	{
		Com_Printf( "The %s command can only be used when playing back demos\n", Cmd_Argv( 0 ) );
		return;
	}

	if ( cl_avidemo->integer > 0 ) {
		Com_Printf( "The %s command cannot be used when cl_avidemo is non-zero\n", Cmd_Argv( 0 ) );
		return;
	}

	pipe = ( Q_stricmp( Cmd_Argv( 0 ), "video-pipe" ) == 0 );

	if ( pipe )
		ext = "mp4";
	else
		ext = "avi";

	if ( Cmd_Argc() == 2 )
	{
		const char *arg = Cmd_Argv( 1 );
		// explicit filename
		if ( !Q_stricmp( arg, "$demo") ) {
			char demoName[ MAX_OSPATH ];
			COM_StripExtension( clc.demoName, demoName, sizeof( clc.demoName ) );
			Com_sprintf( filename, sizeof( filename ), "videos/%s", demoName );
		}
		else {
			Com_sprintf( filename, sizeof( filename ), "videos/%s", arg );
		}

		// override video file extension
		if ( pipe )
		{
			char *sep = strrchr( filename, '/' ); // last path separator
			char *e = strrchr( filename, '.' );

			if ( e && e > sep && *(e+1) != '\0' ) {
				ext = e + 1;
				*e = '\0';
			}
		}
	}
	else
	{
		 // scan for a free filename
		for ( i = 0; i <= 9999; i++ )
		{
			Com_sprintf( filename, sizeof( filename ), "videos/video%04d.%s", i, ext );
			if ( !FS_FileExists( filename ) )
				break; // file doesn't exist
		}

		if ( i > 9999 )
		{
			Com_Printf( S_COLOR_RED "ERROR: no free file names to create video\n" );
			return;
		}

		// without extension
		Com_sprintf( filename, sizeof( filename ), "videos/video%04d", i );
	}


	clc.aviSoundFrameRemainder = 0.0f;
	clc.aviVideoFrameRemainder = 0.0f;

	Q_strncpyz( clc.videoName, filename, sizeof( clc.videoName ) );
	clc.videoIndex = 0;

	CL_OpenAVIForWriting( va( "%s.%s", clc.videoName, ext ), pipe, qfalse );
}


/*
===============
CL_StopVideo_f
===============
*/
static void CL_StopVideo_f( void )
{
	CL_CloseAVI( qfalse );
}


/*
====================
CL_CompleteVideoName
====================
*/
static void CL_CompleteVideoName( char *args, int argNum )
{
	if ( argNum == 2 )
	{
		Field_CompleteFilename( "videos", ".avi", qfalse, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}

static void CL_AddFavorite_f( void ) {
	const qboolean connected = (cls.state == CA_ACTIVE) && !clc.demoplaying;
	const int argc = Cmd_Argc();
	if ( !connected && argc != 2 ) {
		Com_Printf( "syntax: addFavorite <ip or hostname>\n" );
		return;
	}

	if ( argc != 2 && com_sv_running->integer ) {
		Com_Printf( "error adding favorite server: cannot add localhost to favorites\n" );
		return;
	}

	{
		const char *server = ( argc == 2 ) ? Cmd_Argv( 1 ) : NET_AdrToString( &clc.serverAddress );
		const int status = LAN_AddFavAddr( server );
		switch ( status ) {
		case -1:
			Com_Printf( "error adding favorite server: too many favorite servers\n" );
			break;
		case 0:
			Com_Printf( "error adding favorite server: server already exists\n" );
			break;
		case 1:
			Com_Printf( "successfully added favorite server \"%s\"\n", server );
			break;
		default:
			Com_Printf( "unknown error (%i) adding favorite server\n", status );
			break;
		}
	}
}

static void CL_ListFavorites_f( void ) {
	int i;

	if ( !cls.numfavoriteservers ) {
		Com_Printf( "No favorite servers in the currently loaded mod.\n" );
		return;
	}

	for ( i = 0; i < cls.numfavoriteservers; i++ ) {
		if ( cls.favoriteServers[i].adr.type == NA_IP || cls.favoriteServers[i].adr.type == NA_IP6 ) {
			Com_Printf( "Fav Server: %s \"%s\"\n", NET_AdrToStringwPort( &cls.favoriteServers[i].adr ), cls.favoriteServers[i].hostName );
		}
	}
}


/*
** CL_GetModeInfo
*/
typedef struct vidmode_s
{
	const char	*description;
	int			width, height;
	float		pixelAspect;		// pixel width / height
} vidmode_t;

static const vidmode_t cl_vidModes[] =
{
	{ "Mode  0: 320x240",			320,	240,	1 },
	{ "Mode  1: 400x300",			400,	300,	1 },
	{ "Mode  2: 512x384",			512,	384,	1 },
	{ "Mode  3: 640x480",			640,	480,	1 },
	{ "Mode  4: 800x600",			800,	600,	1 },
	{ "Mode  5: 960x720",			960,	720,	1 },
	{ "Mode  6: 1024x768",			1024,	768,	1 },
	{ "Mode  7: 1152x864",			1152,	864,	1 },
	{ "Mode  8: 1280x1024 (5:4)",	1280,	1024,	1 },
	{ "Mode  9: 1600x1200",			1600,	1200,	1 },
	{ "Mode 10: 2048x1536",			2048,	1536,	1 },
	{ "Mode 11: 856x480 (wide)",	856,	480,	1 },
	// extra modes:
	{ "Mode 12: 1280x960",			1280,	960,	1 },
	{ "Mode 13: 1280x720",			1280,	720,	1 },
	{ "Mode 14: 1280x800 (16:10)",	1280,	800,	1 },
	{ "Mode 15: 1366x768",			1366,	768,	1 },
	{ "Mode 16: 1440x900 (16:10)",	1440,	900,	1 },
	{ "Mode 17: 1600x900",			1600,	900,	1 },
	{ "Mode 18: 1680x1050 (16:10)",	1680,	1050,	1 },
	{ "Mode 19: 1920x1080",			1920,	1080,	1 },
	{ "Mode 20: 1920x1200 (16:10)",	1920,	1200,	1 },
	{ "Mode 21: 2560x1080 (21:9)",	2560,	1080,	1 },
	{ "Mode 22: 3440x1440 (21:9)",	3440,	1440,	1 },
	{ "Mode 23: 3840x2160 (4K UHD)",3840,	2160,	1 },
	{ "Mode 24: 4096x2160 (4K)",	4096,	2160,	1 }
};
static const int s_numVidModes = ARRAY_LEN( cl_vidModes );

qboolean CL_GetModeInfo( int *width, int *height, float *windowAspect, int mode, const char *modeFS, int dw, int dh, qboolean fullscreen )
{
	const	vidmode_t *vm;
	float	pixelAspect;

	// set dedicated fullscreen mode
	if ( fullscreen && *modeFS )
		mode = atoi( modeFS );

	if ( mode < -2 )
		return qfalse;

	if ( mode >= s_numVidModes )
		return qfalse;

	// fix unknown desktop resolution
	if ( mode == -2 && (dw == 0 || dh == 0) )
		mode = 3; // 640x480

	if ( mode == -2 ) { // desktop resolution
		*width = dw;
		*height = dh;
		pixelAspect = r_customaspect->value;
	} else if ( mode == -1 ) { // custom resolution
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		pixelAspect = r_customaspect->value;
	} else { // predefined resolution
		vm = &cl_vidModes[ mode ];
		*width  = vm->width;
		*height = vm->height;
		pixelAspect = vm->pixelAspect;
	}

	*windowAspect = (float)*width / ( *height * pixelAspect );

	return qtrue;
}


/*
** CL_ModeList_f
*/
static void CL_ModeList_f( void )
{
	int i;

	Com_Printf( "\n" );
	Com_Printf( "Mode -2: Desktop Resolution\n" );
	Com_Printf( "Mode -1: Custom Resolution (%ix%i)\n", r_customwidth->integer, r_customheight->integer );
	Com_Printf( "         Set r_customWidth and r_customHeight cvars to change\n" );
	Com_Printf( "\n" );
	for ( i = 0; i < s_numVidModes; i++ )
	{
		Com_Printf( "%s\n", cl_vidModes[ i ].description );
	}
	Com_Printf( "\n" );
}


#ifdef USE_RENDERER_DLOPEN
static qboolean isValidRenderer( const char *s ) {
	while ( *s ) {
		if ( !((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '1' && *s <= '9')) )
			return qfalse;
		++s;
	}
	return qtrue;
}
#endif


static void CL_InitGLimp_Cvars( void )
{
	// shared with GLimp
	r_allowSoftwareGL = Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
	Cvar_SetDescription( r_allowSoftwareGL, "Toggle the use of the default software OpenGL driver supplied by the Operating System" );
	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( r_swapInterval, "V-blanks to wait before swapping buffers\n 0: No V-Sync\n 1: Synced to the monitor's refresh rate" );
#ifndef USE_SDL
	r_glDriver = Cvar_Get( "r_glDriver", OPENGL_DRIVER_NAME, CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_UNSAFE );
	Cvar_SetDescription( r_glDriver, "Specifies the OpenGL driver to use, will revert back to default if driver name set is invalid" );
#ifdef _WIN32
	r_allowScreenSaver = Cvar_Get( "r_allowScreenSaver", "0", CVAR_ARCHIVE_ND | CVAR_UNSAFE );
	Cvar_SetDescription( r_allowScreenSaver, "Allow screen to sleep while the game is running" );
#endif
#else
	r_sdlDriver = Cvar_Get( "r_sdlDriver", "", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_UNSAFE );
	Cvar_SetDescription( r_sdlDriver, "Override hint to SDL which video driver to use, example \"x11\" or \"wayland\"" );
	r_allowScreenSaver = Cvar_Get( "r_allowScreenSaver", "0", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_UNSAFE );
	Cvar_SetDescription( r_allowScreenSaver, "Allow screen to sleep while the game is running, requires full restart" );
#endif

	r_displayRefresh = Cvar_Get( "r_displayRefresh", "0", CVAR_LATCH | CVAR_UNSAFE );
	Cvar_CheckRange( r_displayRefresh, "0", "1000", CV_INTEGER );
	Cvar_SetDescription( r_displayRefresh, "Override monitor refresh rate in fullscreen mode:\n  0 - use current monitor refresh rate\n >0 - use custom refresh rate" );

	vid_xpos = Cvar_Get( "vid_xpos", "3", CVAR_ARCHIVE );
	vid_ypos = Cvar_Get( "vid_ypos", "22", CVAR_ARCHIVE );
	Cvar_CheckRange( vid_xpos, NULL, NULL, CV_INTEGER );
	Cvar_CheckRange( vid_ypos, NULL, NULL, CV_INTEGER );
	Cvar_SetDescription( vid_xpos, "Saves/Sets window X-coordinate when windowed, requires \\vid_restart" );
	Cvar_SetDescription( vid_ypos, "Saves/Sets window Y-coordinate when windowed, requires \\vid_restart" );

	r_noborder = Cvar_Get( "r_noborder", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( r_noborder, "0", "1", CV_INTEGER );
	Cvar_SetDescription( r_noborder, "Setting to 1 will remove window borders and title bar in windowed mode, hold ALT to drag & drop it with opened console" );

	r_mode = Cvar_Get( "r_mode", "-2", CVAR_ARCHIVE | CVAR_LATCH );
	r_modeFullscreen = Cvar_Get( "r_modeFullscreen", "-2", CVAR_ARCHIVE | CVAR_LATCH );
	/*r_oldMode =*/ Cvar_Get( "r_oldMode", "", CVAR_ARCHIVE | CVAR_NOTABCOMPLETE );                             // ydnar: previous "good" video mode
	Cvar_CheckRange( r_mode, "-2", va( "%i", s_numVidModes-1 ), CV_INTEGER );
	Cvar_SetDescription( r_mode, "Set video mode:\n -2 - use current desktop resolution\n -1 - use \\r_customWidth and \\r_customHeight\n  0..N - enter \\modelist for details" );
	Cvar_SetDescription( r_modeFullscreen, "Dedicated fullscreen mode, set to \"\" to use \\r_mode in all cases" );

	r_fullscreen = Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( r_fullscreen, "Fullscreen mode. Set to 0 for windowed mode" );
	r_customaspect = Cvar_Get( "r_customAspect", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_customwidth = Cvar_Get( "r_customWidth", "1280", CVAR_ARCHIVE | CVAR_LATCH );
	r_customheight = Cvar_Get( "r_customHeight", "720", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_CheckRange( r_customaspect, "0.001", NULL, CV_FLOAT ); // check better ranges
	Cvar_CheckRange( r_customwidth, "4", NULL, CV_INTEGER );
	Cvar_CheckRange( r_customheight, "4", NULL, CV_INTEGER );
	Cvar_SetDescription( r_customaspect, "Enables custom aspect of the screen, with \\r_mode -1" );
	Cvar_SetDescription( r_customwidth, "Custom width to use with \\r_mode -1" );
	Cvar_SetDescription( r_customheight, "Custom height to use with \\r_mode -1" );

	r_colorbits = Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_UNSAFE );
	Cvar_CheckRange( r_colorbits, "0", "32", CV_INTEGER );
	Cvar_SetDescription( r_colorbits, "Sets color bit depth, set to 0 to use desktop settings" );

	// shared with renderer:
	cl_stencilbits = Cvar_Get( "r_stencilbits", "8", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_UNSAFE );
	Cvar_CheckRange( cl_stencilbits, "0", "8", CV_INTEGER );
	Cvar_SetDescription( cl_stencilbits, "Stencil buffer size, required to be 8 for stencil shadows" );
	cl_depthbits = Cvar_Get( "r_depthbits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_UNSAFE );
	Cvar_CheckRange( cl_depthbits, "0", "32", CV_INTEGER );
	Cvar_SetDescription( cl_depthbits, "Sets precision of Z-buffer" );

	cl_drawBuffer = Cvar_Get( "r_drawBuffer", "GL_BACK", CVAR_CHEAT );
	Cvar_SetDescription( cl_drawBuffer, "Specifies buffer to draw from: GL_FRONT or GL_BACK" );
#ifdef USE_RENDERER_DLOPEN
#ifdef RENDERER_DEFAULT
	cl_renderer = Cvar_Get( "cl_renderer", XSTRING( RENDERER_DEFAULT ), CVAR_ARCHIVE | CVAR_LATCH );
#else
	cl_renderer = Cvar_Get( "cl_renderer", "opengl", CVAR_ARCHIVE | CVAR_LATCH );
#endif
	Cvar_SetDescription( cl_renderer, "Sets your desired renderer, requires \\vid_restart." );

	if ( !isValidRenderer( cl_renderer->string ) ) {
		Cvar_ForceReset( "cl_renderer" );
	}
#endif
}


static const cmdListItem_t cl_cmds[] = {
	{ "addFavorite", CL_AddFavorite_f, NULL },
	{ "benchmark", CL_StartBenchmark_f, CL_CompleteDemoName },
	{ "cache_endgather", CL_Cache_EndGather_f, NULL },
	{ "cache_mapchange", CL_Cache_MapChange_f, NULL },
	{ "cache_setindex", CL_Cache_SetIndex_f, NULL },
	{ "cache_startgather", CL_Cache_StartGather_f, NULL },
	{ "cache_usedfile", CL_Cache_UsedFile_f, NULL },
	{ "cinematic", CL_PlayCinematic_f, CL_CompleteCinematicName },
	{ "clientinfo", CL_Clientinfo_f, NULL },
	{ "cmd", CL_ForwardToServer_f, CL_CompleteRcon },
	{ "configstrings", CL_Configstrings_f, NULL },
	{ "connect", CL_Connect_f, NULL },
	{ "demo", CL_PlayDemo_f, CL_CompleteDemoName },
	{ "disconnect", CL_Disconnect_f, NULL },
#ifdef USE_CURL
	{ "dlmap", CL_Download_f, NULL },
	{ "download", CL_Download_f, NULL },
#endif
	{ "fs_openedList", CL_OpenedPK3List_f, NULL },
	{ "fs_referencedList", CL_ReferencedPK3List_f, NULL },
	{ "globalservers", CL_GlobalServers_f, NULL },
	{ "listFavorites", CL_ListFavorites_f, NULL },
#if !defined(__APPLE__) && !defined(__APPLE_CC__)
	{ "LoadTranslations", CL_LoadTranslations_f, NULL },
#endif
	{ "localservers", CL_LocalServers_f, NULL },
	{ "modelist", CL_ModeList_f, NULL },
	{ "ping", CL_Ping_f, NULL },
	{ "rcon", CL_Rcon_f, CL_CompleteRcon },
	{ "reconnect", CL_Reconnect_f, NULL },
	{ "record", CL_Record_f, CL_CompleteRecordName },
#if !defined(__APPLE__) && !defined(__APPLE_CC__)
	{ "SaveNewTranslations", CL_SaveNewTranslations_f, NULL },
	{ "SaveTranslations", CL_SaveTranslations_f, NULL },
#endif
	{ "serverinfo", CL_Serverinfo_f, NULL },
	{ "serverstatus", CL_ServerStatus_f, NULL },
	{ "setRecommended", CL_SetRecommended_f, NULL },
	{ "showip", CL_ShowIP_f, NULL },
	{ "snd_reload", CL_Snd_Reload_f, NULL },
	{ "snd_restart", CL_Snd_Restart_f, NULL },
	{ "stoprecord", CL_StopRecord_f, NULL },
	{ "stopvideo", CL_StopVideo_f, NULL },
	{ "systeminfo", CL_Systeminfo_f, NULL },
	{ "ui_restart", CL_UI_Restart_f, NULL },
	{ "updatehunkusage", CL_UpdateLevelHunkUsage, NULL },
	{ "updatescreen", SCR_UpdateScreen, NULL },
	//{ "userinfo", CL_EatMe_f, NULL },
	{ "vid_restart", CL_Vid_Restart_f, NULL },
	{ "video-pipe", CL_Video_f, CL_CompleteVideoName },
	{ "video", CL_Video_f, CL_CompleteVideoName },
//	{ "wav_record", CL_WavRecord_f NULL },
//	{ "wav_stoprecord", CL_WavStopRecord_f NULL },
	{ "wolfinfo", CL_Wolfinfo_f, NULL },
};

/*
====================
CL_Init
====================
*/
void CL_Init( void ) {
	const char *s;
	cvar_t *cv;

	Com_Printf( "----- Client Initialization -----\n" );

	Con_Init();

	CL_ClearState();
	cls.state = CA_DISCONNECTED;	// no longer CA_UNINITIALIZED

	CL_ResetOldGame();

	cls.realtime = 0;

	CL_InitInput();

	//
	// register client variables
	//
	cl_noprint = Cvar_Get( "cl_noprint", "0", 0 );
	Cvar_SetDescription( cl_noprint, "Disable printing of information in the console" );
	cl_motd = Cvar_Get( "cl_motd", "1", 0 );
	Cvar_SetDescription( cl_motd, "Toggle the display of the 'Message of the day'. When Enemy Territory starts a map up, it sends the GL_RENDERER string to the Message Of The Day server at id. This responds back with a message of the day to the client" );

	cl_timeout = Cvar_Get( "cl_timeout", "200", 0 );
	Cvar_CheckRange( cl_timeout, "5", NULL, CV_INTEGER );
	Cvar_SetDescription( cl_timeout, "Duration of receiving nothing from server for client to decide it must be disconnected (in seconds)" );

	cl_autoNudge = Cvar_Get( "cl_autoNudge", "0", CVAR_TEMP );
	Cvar_CheckRange( cl_autoNudge, "0", "1", CV_FLOAT );
	Cvar_SetDescription( cl_autoNudge, "Automatic time nudge that uses your average ping as the time nudge, values:\n  0 - use fixed \\cl_timeNudge\n (0..1] - factor of median average ping to use as timenudge" );
	cl_timeNudge = Cvar_Get( "cl_timeNudge", "0", CVAR_TEMP );
	Cvar_CheckRange( cl_timeNudge, "-30", "30", CV_INTEGER );
	Cvar_SetDescription( cl_timeNudge, "Allows more or less latency to be added in the interest of better smoothness or better responsiveness" );

	cl_shownet = Cvar_Get( "cl_shownet", "0", CVAR_TEMP );
	Cvar_SetDescription( cl_shownet, "Toggle the display of current network status" );
	cl_shownuments = Cvar_Get( "cl_shownuments", "0", CVAR_TEMP );
	Cvar_SetDescription( cl_shownuments, "Toggle the display of parsed packet entity counts" );
	cl_showServerCommands = Cvar_Get( "cl_showServerCommands", "0", 0 );
	Cvar_SetDescription( cl_showServerCommands, "Prints incoming server commands to console. Requires \\developer 1" );
	cl_showTimeDelta = Cvar_Get( "cl_showTimeDelta", "0", CVAR_TEMP );
	Cvar_SetDescription( cl_showTimeDelta, "Prints the time delta of each packet to the console (the time delta between server updates)." );
	rcon_client_password = Cvar_Get( "rconPassword", "", CVAR_TEMP );
	Cvar_SetDescription( rcon_client_password, "Sets a remote console password so clients may change server settings without direct access to the server console" );
	cl_activeAction = Cvar_Get( "activeAction", "", CVAR_TEMP | CVAR_PROTECTED );
	Cvar_SetDescription( cl_activeAction, "Contents of this variable will be executed upon first frame of play.\nNote: It is cleared every time it is executed" );

	cl_autoRecordDemo = Cvar_Get ("cl_autoRecordDemo", "0", CVAR_ARCHIVE);
	Cvar_SetDescription( cl_autoRecordDemo, "Auto-record demos when starting or joining a game" );
	cl_avidemo = Cvar_Get( "cl_avidemo", "0", CVAR_TEMP ); // 0
	Cvar_CheckRange( cl_avidemo, "0", "0", CV_INTEGER );
	//Cvar_CheckRange( cl_avidemo, "0", "1000", CV_INTEGER );

	cl_aviFrameRate = Cvar_Get ("cl_aviFrameRate", "25", CVAR_ARCHIVE);
	Cvar_CheckRange( cl_aviFrameRate, "1", "1000", CV_INTEGER );
	Cvar_SetDescription( cl_aviFrameRate, "The framerate used for capturing video." );
	cl_aviMotionJpeg = Cvar_Get ("cl_aviMotionJpeg", "1", CVAR_ARCHIVE);
	Cvar_SetDescription( cl_aviMotionJpeg, "Enable/disable the MJPEG codec for avi output" );
	Cvar_CheckRange( cl_aviMotionJpeg, "0", "1", CV_INTEGER );
	cl_forceavidemo = Cvar_Get ("cl_forceavidemo", "0", 0);
	Cvar_SetDescription( cl_forceavidemo, "Forces all demo recording into a sequence of screenshots in TGA format" );
	Cvar_CheckRange( cl_forceavidemo, "0", "1", CV_INTEGER );

	cl_aviPipeFormat = Cvar_Get( "cl_aviPipeFormat",
		"-preset medium -crf 23 -c:v libx264 -flags +cgop -pix_fmt yuvj420p "
		"-bf 2 -c:a aac -strict -2 -b:a 160k -movflags faststart",
		CVAR_ARCHIVE );
	Cvar_SetDescription( cl_aviPipeFormat, "Encoder parameters used for \\video-pipe" );

	rconAddress = Cvar_Get( "rconAddress", "", 0 );
	Cvar_SetDescription( rconAddress, "The IP address of the remote console you wish to connect to" );

	cl_allowDownload = Cvar_Get( "cl_allowDownload", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_allowDownload, "Enables downloading of content needed in server" );
	Cvar_CheckRange( cl_allowDownload, "0", "1", CV_INTEGER );
#ifdef USE_CURL
	cl_mapAutoDownload = Cvar_Get( "cl_mapAutoDownload", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_mapAutoDownload, "Automatic map download for play and demo playback (via automatic \\dlmap call)" );
	Cvar_CheckRange( cl_mapAutoDownload, "0", "1", CV_INTEGER );
#ifdef USE_CURL_DLOPEN
	cl_cURLLib = Cvar_Get( "cl_cURLLib", DEFAULT_CURL_LIB, 0 );
	Cvar_SetDescription( cl_cURLLib, "Filename of cURL library to load." );
#endif
#endif
	cl_wwwDownload = Cvar_Get( "cl_wwwDownload", "1", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_wwwDownload, "Enables/Disables downloading of content needed in server over HTTP/FTP from server provided URL.\n Requires \\cl_allowDownload >= 1 but not including bit 4" );
#ifdef USE_CURL
	Cvar_CheckRange( cl_wwwDownload, "0", "1", CV_INTEGER );
#else
	Cvar_CheckRange( cl_wwwDownload, "0", "0", CV_INTEGER );
#endif

	cl_profile = Cvar_Get( "cl_profile", "", CVAR_INIT );
	Cvar_SetDescription( cl_profile, "Current game user profile. Can only be set as startup parameter or through menus" );
	cl_defaultProfile = Cvar_Get( "cl_defaultProfile", "", CVAR_ROM );
	Cvar_SetDescription( cl_defaultProfile, "The default loaded game user profile. Can only be set in menus" );

	// init autoswitch so the ui will have it correctly even
	// if the cgame hasn't been started
	// -NERVE - SMF - disabled autoswitch by default
	Cvar_Get( "cg_autoswitch", "0", CVAR_ARCHIVE | CVAR_VM_CREATED );

	// Rafael - particle switch
	Cvar_Get( "cg_wolfparticles", "1", CVAR_ARCHIVE_ND | CVAR_VM_CREATED );
	// done

	cl_conXOffset = Cvar_Get( "cl_conXOffset", "0", 0 );
	Cvar_SetDescription( cl_conXOffset, "Console notifications X-offset" );
	cl_conColor = Cvar_Get( "cl_conColor", "", 0 );
	Cvar_SetDescription( cl_conColor, "Console background color, set as R G B A values from 0-255, use with \\seta to save in config" );
	cl_inGameVideo = Cvar_Get( "r_inGameVideo", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_inGameVideo, "Controls whether in game video should be drawn" );

	cl_serverStatusResendTime = Cvar_Get( "cl_serverStatusResendTime", "750", 0 );
	Cvar_SetDescription( cl_serverStatusResendTime, "Time between re-sending server status requests if no response is received (in milliseconds)" );

	cl_motdString = Cvar_Get( "cl_motdString", "", CVAR_ROM );
	Cvar_SetDescription( cl_motdString, "Message of the day string from id's master server, it is a read only variable" );

	//bani - make these cvars visible to cgame
	cl_demorecording = Cvar_Get( "cl_demorecording", "0", CVAR_ROM );
	cl_demofilename = Cvar_Get( "cl_demofilename", "", CVAR_ROM );
	cl_demooffset = Cvar_Get( "cl_demooffset", "0", CVAR_ROM );
	cl_waverecording = Cvar_Get( "cl_waverecording", "0", CVAR_ROM );
	cl_wavefilename = Cvar_Get( "cl_wavefilename", "", CVAR_ROM );
	cl_waveoffset = Cvar_Get( "cl_waveoffset", "0", CVAR_ROM );
	cl_silentRecord = Cvar_Get( "cl_silentRecord", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_silentRecord, "Whether to print recording messages to console on begin/stop recording" );

	cv = Cvar_Get( "cl_maxPing", "800", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cv, "100", "999", CV_INTEGER );
	Cvar_SetDescription( cv, "Specify the maximum allowed ping to a server." );

	cl_lanForcePackets = Cvar_Get( "cl_lanForcePackets", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_lanForcePackets, "Bypass \\cl_maxpackets for LAN games, send packets every frame" );

	// NERVE - SMF
	Cvar_Get( "cg_drawCompass", "1", CVAR_ARCHIVE | CVAR_VM_CREATED );
	Cvar_Get( "cg_drawNotifyText", "1", CVAR_ARCHIVE | CVAR_VM_CREATED );
	Cvar_Get( "cg_quickMessageAlt", "1", CVAR_ARCHIVE | CVAR_VM_CREATED );
	Cvar_Get( "cg_popupLimboMenu", "1", CVAR_ARCHIVE | CVAR_VM_CREATED );
	Cvar_Get( "cg_descriptiveText", "1", CVAR_ARCHIVE | CVAR_VM_CREATED );
	Cvar_Get( "cg_drawTeamOverlay", "2", CVAR_ARCHIVE | CVAR_VM_CREATED );
//	Cvar_Get( "cg_uselessNostalgia", "0", CVAR_ARCHIVE ); // JPW NERVE
	Cvar_Get( "cg_drawGun", "1", CVAR_ARCHIVE | CVAR_VM_CREATED );
	Cvar_Get( "cg_cursorHints", "1", CVAR_ARCHIVE | CVAR_VM_CREATED );
	Cvar_Get( "cg_voiceSpriteTime", "6000", CVAR_ARCHIVE | CVAR_VM_CREATED );
//	Cvar_Get( "cg_teamChatsOnly", "0", CVAR_ARCHIVE );
//	Cvar_Get( "cg_noVoiceChats", "0", CVAR_ARCHIVE );
//	Cvar_Get( "cg_noVoiceText", "0", CVAR_ARCHIVE );
	Cvar_Get( "cg_crosshairSize", "48", CVAR_ARCHIVE | CVAR_VM_CREATED );
	Cvar_Get( "cg_drawCrosshair", "1", CVAR_ARCHIVE | CVAR_VM_CREATED );
	Cvar_Get( "cg_zoomDefaultSniper", "20", CVAR_ARCHIVE | CVAR_VM_CREATED );
	Cvar_Get( "cg_zoomstepsniper", "2", CVAR_ARCHIVE | CVAR_VM_CREATED );

//	Cvar_Get( "mp_playerType", "0", 0 );
//	Cvar_Get( "mp_currentPlayerType", "0", 0 );
//	Cvar_Get( "mp_weapon", "0", 0 );
//	Cvar_Get( "mp_team", "0", 0 );
//	Cvar_Get( "mp_currentTeam", "0", 0 );
	// -NERVE - SMF

	// ENSI NOTE need a URL for this
	cl_dlURL = Cvar_Get( "cl_dlURL", ""/*"http://ws.q3df.org/maps/download/%1"*/, CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_dlURL, "URL for downloads initiated by \\dlmap and \\download commands" );

	cl_dlDirectory = Cvar_Get( "cl_dlDirectory", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_dlDirectory, "0", "1", CV_INTEGER );
	s = va( "Save downloads initiated by \\dlmap and \\download commands in:\n"
		" 0 - current game directory\n"
		" 1 - fs_basegame (%s) directory", FS_GetBaseGameDir() );
	Cvar_SetDescription( cl_dlDirectory, s );

	cl_reconnectArgs = Cvar_Get( "cl_reconnectArgs", "", CVAR_ARCHIVE_ND | CVAR_NOTABCOMPLETE );

	// userinfo
	Cvar_Get( "name", "ETPlayer", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get( "clan", "", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get( "rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE );     // NERVE - SMF - changed from 3000
	Cvar_Get( "snaps", "40", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get( "cl_anonymous", "0", CVAR_USERINFO | CVAR_ARCHIVE_ND );

	Cvar_Get( "password", "", CVAR_USERINFO | CVAR_NORESTART );
	Cvar_Get( "cg_predictItems", "1", CVAR_ARCHIVE | CVAR_VM_CREATED );

//----(SA) added
	Cvar_Get( "cg_autoactivate", "1", CVAR_ARCHIVE_ND | CVAR_VM_CREATED );
//----(SA) end

	// cgame might not be initialized before menu is used
	Cvar_Get( "cg_viewsize", "100", CVAR_ARCHIVE_ND | CVAR_VM_CREATED );
	// Make sure cg_stereoSeparation is zero as that variable is deprecated and should not be used anymore.
	Cvar_Get ("cg_stereoSeparation", "0", CVAR_ROM);

	Cvar_Get( "cg_autoReload", "1", CVAR_ARCHIVE_ND | CVAR_VM_CREATED );

	// NERVE - SMF - localization
	cl_language = Cvar_Get( "cl_language", "0", CVAR_ARCHIVE_ND );
	cl_debugTranslation = Cvar_Get( "cl_debugTranslation", "0", 0 );
	// -NERVE - SMF

#ifndef USE_SDL
	in_forceCharset = Cvar_Get( "in_forceCharset", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( in_forceCharset, "Try to translate non-ASCII chars in keyboard input or force EN/US keyboard layout" );
#endif

#ifdef USE_DISCORD
	cl_discordRichPresence = Cvar_Get("cl_discordRichPresence", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_discordRichPresence, "Allow/disallow sharing current game information on Discord profile status" );
#endif

	// ETJump
	Cvar_Get( "shared", "0", CVAR_SYSTEMINFO | CVAR_ROM );

	//
	// register client commands
	//
	Cmd_RegisterArray( cl_cmds, MODULE_CLIENT );

	Cvar_Set( "cl_running", "1" );

	CL_GenerateETKey();
	Cvar_Get( "cl_guid", "", CVAR_USERINFO | CVAR_ROM | CVAR_PROTECTED );
	CL_UpdateGUID();

#if !defined(__APPLE__) && !defined(__APPLE_CC__)  //DAJ USA
	CL_InitTranslation();       // NERVE - SMF - localization
#endif

#ifdef USE_DISCORD
	if (cl_discordRichPresence->integer) {
		CL_DiscordInitialize();
		cls.discordInit = qtrue;
	}
#endif

#ifndef USE_SDL
	Cvar_SetGroup( cl_language, CVG_LANGUAGE );
	Cvar_SetGroup( in_forceCharset, CVG_LANGUAGE );
#endif

	CL_TrackCvarChanges( qtrue );

	Com_Printf( "----- Client Initialization Complete -----\n" );
}


/*
===============
CL_Shutdown

Called on fatal error, quit and dedicated mode switch
===============
*/
void CL_Shutdown( const char *finalmsg, qboolean quit ) {
	static qboolean recursive = qfalse;

	// check whether the client is running at all.
	if ( !( com_cl_running && com_cl_running->integer ) )
		return;

	Com_Printf( "----- Client Shutdown (%s) -----\n", finalmsg );

	if ( recursive ) {
		Com_Printf( "WARNING: Recursive CL_Shutdown()\n" );
		return;
	}
	recursive = qtrue;

	//if ( clc.waverecording ) { // fretn - write wav header when we quit
	//	CL_WavStopRecord_f();
	//}

	noGameRestart = quit;
	cl_shutdownQuit = quit;
	CL_Disconnect( qfalse );

	// clear and mute all sounds until next registration
	S_DisableSounds();

	CL_ShutdownVMs();

	CL_ShutdownRef( quit ? REF_UNLOAD_DLL : REF_DESTROY_WINDOW );

#ifdef USE_CURL
	DL_Shutdown();
	Com_DL_Cleanup( &download );
#endif

	Cmd_UnregisterModule( MODULE_CLIENT );

#ifdef USE_DISCORD
	if (cl_discordRichPresence->integer || cls.discordInit)
		CL_DiscordShutdown();
#endif

	CL_ShutdownInput();
	Con_Shutdown();
	Cvar_Set( "cl_running", "0" );

	recursive = qfalse;

	Com_Memset( &cls, 0, sizeof( cls ) );
	Key_SetCatcher( 0 );
	Com_Printf( "-----------------------\n" );
}


static void CL_SetServerInfo( serverInfo_t *server, const char *info, int ping ) {
	if ( server ) {
		if ( info ) {
			server->clients = atoi( Info_ValueForKey( info, "clients" ) );
			Q_strncpyz( server->hostName, Info_ValueForKey( info, "hostname" ), sizeof( server->hostName ) );
			server->load = atoi( Info_ValueForKey( info, "serverload" ) );
			Q_strncpyz( server->mapName, Info_ValueForKey( info, "mapname" ), sizeof( server->mapName ) );
			server->maxClients = atoi( Info_ValueForKey( info, "sv_maxclients" ) );
			Q_strncpyz( server->game, Info_ValueForKey( info, "game" ), sizeof( server->game ) );
			server->gameType = atoi( Info_ValueForKey( info, "gametype" ) );
			server->netType = atoi( Info_ValueForKey( info, "nettype" ) );
			server->minPing = atoi( Info_ValueForKey( info, "minping" ) );
			server->maxPing = atoi( Info_ValueForKey( info, "maxping" ) );
			server->friendlyFire = atoi( Info_ValueForKey( info, "friendlyFire" ) );         // NERVE - SMF
			server->maxlives = atoi( Info_ValueForKey( info, "maxlives" ) );                 // NERVE - SMF
			server->needpass = atoi( Info_ValueForKey( info, "needpass" ) );                 // NERVE - SMF
			server->punkbuster = atoi( Info_ValueForKey( info, "punkbuster" ) );             // DHM - Nerve
			Q_strncpyz( server->gameName, Info_ValueForKey( info, "gamename" ), sizeof( server->gameName ) );   // Arnout
			server->antilag = atoi( Info_ValueForKey( info, "g_antilag" ) );
			server->weaprestrict = atoi( Info_ValueForKey( info, "weaprestrict" ) );
			server->balancedteams = atoi( Info_ValueForKey( info, "balancedteams" ) );
			server->oss = atoi( Info_ValueForKey( info, "oss" ) );
		}
		server->ping = ping;
	}
}


static void CL_SetServerInfoByAddress(const netadr_t *from, const char *info, int ping) {
	int i;

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, &cls.localServers[i].adr) ) {
			CL_SetServerInfo(&cls.localServers[i], info, ping);
		}
	}

	for (i = 0; i < MAX_GLOBAL_SERVERS; i++) {
		if (NET_CompareAdr(from, &cls.globalServers[i].adr)) {
			CL_SetServerInfo(&cls.globalServers[i], info, ping);
		}
	}

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, &cls.favoriteServers[i].adr)) {
			CL_SetServerInfo(&cls.favoriteServers[i], info, ping);
		}
	}
}


/*
===================
CL_ServerInfoPacket
===================
*/
static void CL_ServerInfoPacket( const netadr_t *from, msg_t *msg ) {
	int		i, type, len;
	char	info[MAX_INFO_STRING];
	const char *infoString;
	const char *gameName;
	int		prot;

	infoString = MSG_ReadString( msg );

	// if this isn't the correct protocol version, ignore it
	prot = atoi( Info_ValueForKey( infoString, "protocol" ) );
	if ( prot != OLD_PROTOCOL_VERSION && prot != NEW_PROTOCOL_VERSION && prot != com_protocol->integer ) {
		Com_DPrintf( "Different protocol info packet: %s\n", infoString );
		return;
	}

	// Arnout: if this isn't the correct game, ignore it
	gameName = Info_ValueForKey( infoString, "gamename" );
	if ( !gameName[0] || Q_stricmp( gameName, GAMENAME_STRING ) ) {
		Com_DPrintf( "Different game info packet: %s\n", infoString );
		return;
	}

	// iterate servers waiting for ping response
	for (i=0; i<MAX_PINGREQUESTS; i++)
	{
		if ( cl_pinglist[i].adr.port && !cl_pinglist[i].time && NET_CompareAdr( from, &cl_pinglist[i].adr ) )
		{
			// calc ping time
			cl_pinglist[i].time = Sys_Milliseconds() - cl_pinglist[i].start;
			if ( cl_pinglist[i].time < 1 )
			{
				cl_pinglist[i].time = 1;
			}
			if ( com_developer->integer )
			{
				Com_Printf( "ping time %dms from %s\n", cl_pinglist[i].time, NET_AdrToString( from ) );
			}

			// save of info
			Q_strncpyz( cl_pinglist[i].info, infoString, sizeof( cl_pinglist[i].info ) );

			// tack on the net type
			// NOTE: make sure these types are in sync with the netnames strings in the UI
			switch (from->type)
			{
				case NA_BROADCAST:
				case NA_IP:
					type = 1;
					break;
#ifdef USE_IPV6
				case NA_IP6:
					type = 2;
					break;
#endif
				default:
					type = 0;
					break;
			}

			Info_SetValueForKey( cl_pinglist[i].info, "nettype", va( "%d", type ) );
			CL_SetServerInfoByAddress( from, infoString, cl_pinglist[i].time );

			return;
		}
	}

	// if not just sent a local broadcast or pinging local servers
	if (cls.pingUpdateSource != AS_LOCAL) {
		return;
	}

	for ( i = 0 ; i < MAX_OTHER_SERVERS ; i++ ) {
		// empty slot
		if ( cls.localServers[i].adr.port == 0 ) {
			break;
		}

		// avoid duplicate
		if ( NET_CompareAdr( from, &cls.localServers[i].adr ) ) {
			return;
		}
	}

	if ( i == MAX_OTHER_SERVERS ) {
		Com_DPrintf( "MAX_OTHER_SERVERS hit, dropping infoResponse\n" );
		return;
	}

	// add this to the list
	cls.numlocalservers = i+1;
	CL_InitServerInfo( &cls.localServers[i], from );

	Q_strncpyz( info, MSG_ReadString( msg ), sizeof( info ) );
	len = (int) strlen( info );
	if ( len > 0 ) {
		if ( info[ len-1 ] == '\n' ) {
			info[ len-1 ] = '\0';
		}
		Com_Printf( "%s: %s\n", NET_AdrToStringwPort( from ), info );
	}
}


/*
===================
CL_GetServerStatus
===================
*/
static serverStatus_t *CL_GetServerStatus( const netadr_t *from ) {
	int i, oldest, oldestTime;

	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( NET_CompareAdr( from, &cl_serverStatusList[i].address ) ) {
			return &cl_serverStatusList[i];
		}
	}
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( cl_serverStatusList[i].retrieved ) {
			return &cl_serverStatusList[i];
		}
	}
	oldest = -1;
	oldestTime = 0;
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if (oldest == -1 || cl_serverStatusList[i].startTime < oldestTime) {
			oldest = i;
			oldestTime = cl_serverStatusList[i].startTime;
		}
	}
	return &cl_serverStatusList[oldest];
}


/*
===================
CL_ServerStatus
===================
*/
int CL_ServerStatus( const char *serverAddress, char *serverStatusString, int maxLen ) {
	int i;
	netadr_t	to;
	serverStatus_t *serverStatus;

	// if no server address then reset all server status requests
	if ( !serverAddress ) {
		for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
			cl_serverStatusList[i].address.port = 0;
			cl_serverStatusList[i].retrieved = qtrue;
		}
		return qfalse;
	}
	// get the address
	if ( !NET_StringToAdr( serverAddress, &to, NA_UNSPEC ) ) {
		return qfalse;
	}
	serverStatus = CL_GetServerStatus( &to );
	// if no server status string then reset the server status request for this address
	if ( !serverStatusString ) {
		serverStatus->retrieved = qtrue;
		return qfalse;
	}

	// if this server status request has the same address
	if ( NET_CompareAdr( &to, &serverStatus->address) ) {
		// if we received a response for this server status request
		if (!serverStatus->pending) {
			Q_strncpyz(serverStatusString, serverStatus->string, maxLen);
			serverStatus->retrieved = qtrue;
			serverStatus->startTime = 0;
			return qtrue;
		}
		// resend the request regularly
		else if ( Sys_Milliseconds() - serverStatus->startTime > cl_serverStatusResendTime->integer ) {
			serverStatus->print = qfalse;
			serverStatus->pending = qtrue;
			serverStatus->retrieved = qfalse;
			serverStatus->time = 0;
			serverStatus->startTime = Sys_Milliseconds();
			NET_OutOfBandPrint( NS_CLIENT, &to, "getstatus" );
			return qfalse;
		}
	}
	// if retrieved
	else if ( serverStatus->retrieved ) {
		serverStatus->address = to;
		serverStatus->print = qfalse;
		serverStatus->pending = qtrue;
		serverStatus->retrieved = qfalse;
		serverStatus->startTime = Sys_Milliseconds();
		serverStatus->time = 0;
		NET_OutOfBandPrint( NS_CLIENT, &to, "getstatus" );
		return qfalse;
	}
	return qfalse;
}


/*
===================
CL_ServerStatusResponse
===================
*/
static void CL_ServerStatusResponse( const netadr_t *from, msg_t *msg ) {
	const char	*s;
	char	info[MAX_INFO_STRING];
	char	buf[64], *v[2];
	int		i, l, score, ping;
	int		len;
	serverStatus_t *serverStatus;

	serverStatus = NULL;
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( NET_CompareAdr( from, &cl_serverStatusList[i].address ) ) {
			serverStatus = &cl_serverStatusList[i];
			break;
		}
	}
	// if we didn't request this server status
	if (!serverStatus) {
		return;
	}

	s = MSG_ReadStringLine( msg );

	len = 0;
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "%s", s);

	if (serverStatus->print) {
		Com_Printf("Server settings:\n");
		// print cvars
		while (*s) {
			for (i = 0; i < 2 && *s; i++) {
				if (*s == '\\')
					s++;
				l = 0;
				while (*s) {
					info[l++] = *s;
					if (l >= MAX_INFO_STRING-1)
						break;
					s++;
					if (*s == '\\') {
						break;
					}
				}
				info[l] = '\0';
				if (i) {
					Com_Printf("%s\n", info);
				}
				else {
					Com_Printf("%-24s", info);
				}
			}
		}
	}

	len = strlen(serverStatus->string);
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

	if (serverStatus->print) {
		Com_Printf("\nPlayers:\n");
		Com_Printf("num: score: ping: name:\n");
	}
	for (i = 0, s = MSG_ReadStringLine( msg ); *s; s = MSG_ReadStringLine( msg ), i++) {

		len = strlen(serverStatus->string);
		Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\%s", s);

		if (serverStatus->print) {
			//score = ping = 0;
			//sscanf(s, "%d %d", &score, &ping);
			Q_strncpyz( buf, s, sizeof (buf) );
			Com_Split( buf, v, 2, ' ' );
			score = atoi( v[0] );
			ping = atoi( v[1] );
			s = strchr(s, ' ');
			if (s)
				s = strchr(s+1, ' ');
			if (s)
				s++;
			else
				s = "unknown";
			Com_Printf("%-2d   %-3d    %-3d   %s\n", i, score, ping, s );
		}
	}
	len = strlen(serverStatus->string);
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

	serverStatus->time = Sys_Milliseconds();
	serverStatus->address = *from;
	serverStatus->pending = qfalse;
	if (serverStatus->print) {
		serverStatus->retrieved = qtrue;
	}
}


/*
==================
CL_LocalServers_f
==================
*/
static void CL_LocalServers_f( void ) {
	char		*message;
	int			i, j, n;
	netadr_t	to;

	Com_Printf( "Scanning for servers on the local network...\n");

	// reset the list, waiting for response
	cls.numlocalservers = 0;
	cls.pingUpdateSource = AS_LOCAL;

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		qboolean b = cls.localServers[i].visible;
		Com_Memset(&cls.localServers[i], 0, sizeof(cls.localServers[i]));
		cls.localServers[i].visible = b;
	}
	Com_Memset( &to, 0, sizeof( to ) );

	// The 'xxx' in the message is a challenge that will be echoed back
	// by the server.  We don't care about that here, but master servers
	// can use that to prevent spoofed server responses from invalid ip
	message = "\377\377\377\377getinfo xxx";
	n = (int)strlen( message );

	// send each message twice in case one is dropped
	for ( i = 0 ; i < 2 ; i++ ) {
		// send a broadcast packet on each server port
		// we support multiple server ports so a single machine
		// can nicely run multiple servers
		for ( j = 0 ; j < NUM_SERVER_PORTS ; j++ ) {
			to.port = BigShort( (short)(PORT_SERVER + j) );

			to.type = NA_BROADCAST;
			NET_SendPacket( NS_CLIENT, n, message, &to );
#ifdef USE_IPV6
			to.type = NA_MULTICAST6;
			NET_SendPacket( NS_CLIENT, n, message, &to );
#endif
		}
	}
}


/*
==================
CL_GlobalServers_f

Originally master 0 was Internet and master 1 was MPlayer.
ioquake3 2008; added support for requesting five separate master servers using 0-4.
ioquake3 2017; made master 0 fetch all master servers and 1-5 request a single master server.
==================
*/
static void CL_GlobalServers_f( void ) {
	netadr_t	to;
	int			count, i, masterNum;
	char		command[1024];
	const char	*masteraddress;

	if ( (count = Cmd_Argc()) < 3 || (masterNum = atoi(Cmd_Argv(1))) < 0 || masterNum > MAX_MASTER_SERVERS )
	{
		Com_Printf( "usage: globalservers <master# 0-%d> <protocol> [keywords]\n", MAX_MASTER_SERVERS );
		return;
	}

	// request from all master servers
	if ( masterNum == 0 ) {
		int numAddress = 0;

		for ( i = 1; i <= MAX_MASTER_SERVERS; i++ ) {
			Com_sprintf( command, sizeof( command ), "sv_master%d", i );
			masteraddress = Cvar_VariableString( command );

			if ( !*masteraddress )
				continue;

			numAddress++;

			Com_sprintf( command, sizeof( command ), "globalservers %d %s %s\n", i, Cmd_Argv( 2 ), Cmd_ArgsFrom( 3 ) );
			Cbuf_AddText( command );
		}

		if ( !numAddress ) {
			Com_Printf( "CL_GlobalServers_f: Error: No master server addresses.\n");
		}
		return;
	}

	Com_sprintf( command, sizeof( command ), "sv_master%d", masterNum );
	masteraddress = Cvar_VariableString( command );

	if ( !*masteraddress )
	{
		Com_Printf( "CL_GlobalServers_f: Error: No master server address given for %s.\n", command);
		return;	
	}

	// reset the list, waiting for response
	// -1 is used to distinguish a "no response"

	i = NET_StringToAdr( masteraddress, &to, NA_UNSPEC );

	if ( i == 0 )
	{
		Com_Printf( "CL_GlobalServers_f: Error: could not resolve address of master %s\n", masteraddress );
		return;
	}
	else if ( i == 2 )
		to.port = BigShort( PORT_MASTER );

	Com_Printf( "Requesting servers from %s (%s)...\n", masteraddress, NET_AdrToStringwPort( &to ) );

	cls.numglobalservers = -1;
	cls.pingUpdateSource = AS_GLOBAL;

	// Use the extended query for IPv6 masters
#ifdef USE_IPV6
	if ( to.type == NA_IP6 || to.type == NA_MULTICAST6 )
	{
		int v4enabled = Cvar_VariableIntegerValue( "net_enabled" ) & NET_ENABLEV4;

		if ( v4enabled )
		{
			Com_sprintf( command, sizeof( command ), "getserversExt %s %s",
				GAMENAME_FOR_MASTER, Cmd_Argv(2) );
		}
		else
		{
			Com_sprintf( command, sizeof( command ), "getserversExt %s %s ipv6",
				GAMENAME_FOR_MASTER, Cmd_Argv(2) );
		}
	}
	else
#endif
		Com_sprintf( command, sizeof( command ), "getservers %s", Cmd_Argv(2) );

	for ( i = 3; i < count; i++ )
	{
		const char *arg = Cmd_Argv( i );
		if ( !Q_stricmp( arg, "\\game\\etf" ) )
			continue;
		Q_strcat( command, sizeof( command ), " " );
		Q_strcat( command, sizeof( command ), arg );
	}

	NET_OutOfBandPrint( NS_SERVER, &to, "%s", command );
}


/*
==================
CL_GetPing
==================
*/
void CL_GetPing( int n, char *buf, int buflen, int *pingtime )
{
	const char	*str;
	int		time;
	int		maxPing;

	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
	{
		// empty or invalid slot
		buf[0]    = '\0';
		*pingtime = 0;
		return;
	}

	str = NET_AdrToStringwPort( &cl_pinglist[n].adr );
	Q_strncpyz( buf, str, buflen );

	time = cl_pinglist[n].time;
	if ( time == 0 )
	{
		// check for timeout
		time = Sys_Milliseconds() - cl_pinglist[n].start;
		maxPing = Cvar_VariableIntegerValue( "cl_maxPing" );
		if ( time < maxPing )
		{
			// not timed out yet
			time = 0;
		}
	}

	CL_SetServerInfoByAddress(&cl_pinglist[n].adr, cl_pinglist[n].info, cl_pinglist[n].time);

	*pingtime = time;
}


/*
==================
CL_GetPingInfo
==================
*/
void CL_GetPingInfo( int n, char *buf, int buflen )
{
	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
	{
		// empty or invalid slot
		if (buflen)
			buf[0] = '\0';
		return;
	}

	Q_strncpyz( buf, cl_pinglist[n].info, buflen );
}


/*
==================
CL_ClearPing
==================
*/
void CL_ClearPing( int n )
{
	if (n < 0 || n >= MAX_PINGREQUESTS)
		return;

	cl_pinglist[n].adr.port = 0;
}


/*
==================
CL_GetPingQueueCount
==================
*/
int CL_GetPingQueueCount( void )
{
	int		i;
	int		count;
	ping_t*	pingptr;

	count   = 0;
	pingptr = cl_pinglist;

	for (i=0; i<MAX_PINGREQUESTS; i++, pingptr++ ) {
		if (pingptr->adr.port) {
			count++;
		}
	}

	return (count);
}


/*
==================
CL_GetFreePing
==================
*/
static ping_t* CL_GetFreePing( void )
{
	ping_t* pingptr;
	ping_t* best;
	int		oldest;
	int		i;
	int		time, msec;

	msec = Sys_Milliseconds();
	pingptr = cl_pinglist;
	for ( i = 0; i < ARRAY_LEN( cl_pinglist ); i++, pingptr++ )
	{
		// find free ping slot
		if ( pingptr->adr.port )
		{
			if ( pingptr->time == 0 )
			{
				if ( msec - pingptr->start < 500 )
				{
					// still waiting for response
					continue;
				}
			}
			else if ( pingptr->time < 500 )
			{
				// results have not been queried
				continue;
			}
		}

		// clear it
		pingptr->adr.port = 0;
		return pingptr;
	}

	// use oldest entry
	pingptr = cl_pinglist;
	best    = cl_pinglist;
	oldest  = INT_MIN;
	for ( i = 0; i < ARRAY_LEN( cl_pinglist ); i++, pingptr++ )
	{
		// scan for oldest
		time = msec - pingptr->start;
		if ( time > oldest )
		{
			oldest = time;
			best   = pingptr;
		}
	}

	return best;
}


/*
==================
CL_Ping_f
==================
*/
static void CL_Ping_f( void ) {
	netadr_t	to;
	ping_t*		pingptr;
	const char*		server;
	int			argc;
	netadrtype_t	family = NA_UNSPEC;

	argc = Cmd_Argc();

	if ( argc != 2 && argc != 3 ) {
		Com_Printf( "usage: ping [-4|-6] <server>\n");
		return;
	}

	if ( argc == 2 )
		server = Cmd_Argv(1);
	else
	{
		if( !strcmp( Cmd_Argv(1), "-4" ) )
			family = NA_IP;
#ifdef USE_IPV6
		else if( !strcmp( Cmd_Argv(1), "-6" ) )
			family = NA_IP6;
		else
			Com_Printf( "warning: only -4 or -6 as address type understood.\n" );
#else
		else
			Com_Printf( "warning: only -4 as address type understood.\n" );
#endif

		server = Cmd_Argv(2);
	}

	Com_Memset( &to, 0, sizeof( to ) );

	if ( !NET_StringToAdr( server, &to, family ) ) {
		return;
	}

	pingptr = CL_GetFreePing();

	memcpy( &pingptr->adr, &to, sizeof (netadr_t) );
	pingptr->start = Sys_Milliseconds();
	pingptr->time  = 0;

	CL_SetServerInfoByAddress( &pingptr->adr, NULL, 0 );

	NET_OutOfBandPrint( NS_CLIENT, &to, "getinfo xxx" );
}


/*
==================
CL_UpdateVisiblePings_f
==================
*/
qboolean CL_UpdateVisiblePings_f(int source) {
	int			slots, i;
	char		buff[MAX_STRING_CHARS];
	int			pingTime;
	int			max;
	qboolean status = qfalse;

	if (source < 0 || source > AS_FAVORITES) {
		return qfalse;
	}

	cls.pingUpdateSource = source;

	slots = CL_GetPingQueueCount();
	if (slots < MAX_PINGREQUESTS) {
		serverInfo_t *server = NULL;

		switch (source) {
			case AS_LOCAL :
				server = &cls.localServers[0];
				max = cls.numlocalservers;
			break;
			case AS_GLOBAL :
				server = &cls.globalServers[0];
				max = cls.numglobalservers;
			break;
			case AS_FAVORITES :
				server = &cls.favoriteServers[0];
				max = cls.numfavoriteservers;
			break;
			default:
				return qfalse;
		}
		for (i = 0; i < max; i++) {
			if (server[i].visible) {
				if (server[i].ping == -1) {
					int j;

					if (slots >= MAX_PINGREQUESTS) {
						break;
					}
					for (j = 0; j < MAX_PINGREQUESTS; j++) {
						if (!cl_pinglist[j].adr.port) {
							continue;
						}
						if (NET_CompareAdr( &cl_pinglist[j].adr, &server[i].adr)) {
							// already on the list
							break;
						}
					}
					if (j >= MAX_PINGREQUESTS) {
						status = qtrue;
						for (j = 0; j < MAX_PINGREQUESTS; j++) {
							if (!cl_pinglist[j].adr.port) {
								memcpy(&cl_pinglist[j].adr, &server[i].adr, sizeof(netadr_t));
								cl_pinglist[j].start = Sys_Milliseconds();
								cl_pinglist[j].time = 0;
								NET_OutOfBandPrint(NS_CLIENT, &cl_pinglist[j].adr, "getinfo xxx");
								slots++;
								break;
							}
						}
					}
				}
				// if the server has a ping higher than cl_maxPing or
				// the ping packet got lost
				else if (server[i].ping == 0) {
					// if we are updating global servers
					if (source == AS_GLOBAL) {
						//
						if ( cls.numGlobalServerAddresses > 0 ) {
							// overwrite this server with one from the additional global servers
							cls.numGlobalServerAddresses--;
							CL_InitServerInfo(&server[i], &cls.globalServerAddresses[cls.numGlobalServerAddresses]);
							// NOTE: the server[i].visible flag stays untouched
						}
					}
				}
			}
		}
	}

	if (slots) {
		status = qtrue;
	}
	for (i = 0; i < MAX_PINGREQUESTS; i++) {
		if (!cl_pinglist[i].adr.port) {
			continue;
		}
		CL_GetPing( i, buff, MAX_STRING_CHARS, &pingTime );
		if (pingTime != 0) {
			CL_ClearPing(i);
			status = qtrue;
		}
	}

	return status;
}


/*
==================
CL_ServerStatus_f
==================
*/
static void CL_ServerStatus_f( void ) {
	netadr_t	to, *toptr = NULL;
	const char		*server;
	serverStatus_t *serverStatus;
	int			argc;
	netadrtype_t	family = NA_UNSPEC;

	argc = Cmd_Argc();

	if ( argc != 2 && argc != 3 )
	{
		if (cls.state != CA_ACTIVE || clc.demoplaying)
		{
			Com_Printf( "Not connected to a server.\n" );
#ifdef USE_IPV6
			Com_Printf( "usage: serverstatus [-4|-6] <server>\n" );
#else
			Com_Printf("usage: serverstatus <server>\n");
#endif
			return;
		}

		toptr = &clc.serverAddress;
	}

	if ( !toptr )
	{
		Com_Memset( &to, 0, sizeof( to ) );

		if ( argc == 2 )
			server = Cmd_Argv(1);
		else
		{
			if ( !strcmp( Cmd_Argv(1), "-4" ) )
				family = NA_IP;
#ifdef USE_IPV6
			else if ( !strcmp( Cmd_Argv(1), "-6" ) )
				family = NA_IP6;
			else
				Com_Printf( "warning: only -4 or -6 as address type understood.\n" );
#else
			else
				Com_Printf( "warning: only -4 as address type understood.\n" );
#endif

			server = Cmd_Argv(2);
		}

		toptr = &to;
		if ( !NET_StringToAdr( server, toptr, family ) )
			return;
	}

	NET_OutOfBandPrint( NS_CLIENT, toptr, "getstatus" );

	serverStatus = CL_GetServerStatus( toptr );
	serverStatus->address = *toptr;
	serverStatus->print = qtrue;
	serverStatus->pending = qtrue;
}


/*
==================
CL_ShowIP_f
==================
*/
static void CL_ShowIP_f( void ) {
	Sys_ShowIP();
}


// NERVE - SMF - Localization code
#define FILE_HASH_SIZE      1024
#define MAX_VA_STRING       32000
#define MAX_TRANS_STRING    4096

#if !defined(__APPLE__) && !defined(__APPLE_CC__)   //DAJ USA
typedef struct trans_s {
	char original[MAX_TRANS_STRING];
	char translated[MAX_LANGUAGES][MAX_TRANS_STRING];
	struct  trans_s *next;
	float x_offset;
	float y_offset;
	qboolean fromFile;
} trans_t;

static trans_t* transTable[FILE_HASH_SIZE];

/*
=======================
AllocTrans
=======================
*/
static trans_t* AllocTrans( const char *original, char *translated[MAX_LANGUAGES] ) {
	trans_t *t;
	int i;

	t = malloc( sizeof( trans_t ) );
	if ( !t )
		return NULL;

	memset( t, 0, sizeof( trans_t ) );

	if ( original ) {
		Q_strncpyz( t->original, original, sizeof(t->original) );
	}

	if ( translated ) {
		for ( i = 0; i < MAX_LANGUAGES; i++ )
			Q_strncpyz( t->translated[i], translated[i], sizeof(t->translated[0]) );
	}

	return t;
}


#define generateHashValue Com_GenerateHashValue

/*
=======================
LookupTrans
=======================
*/
static trans_t* LookupTrans( const char *original, char *translated[MAX_LANGUAGES], qboolean isLoading ) {
	trans_t *t, *newt, *prev = NULL;
	int hash;

	hash = generateHashValue( original, FILE_HASH_SIZE );

	for ( t = transTable[hash]; t; prev = t, t = t->next ) {
		if ( !Q_stricmp( original, t->original ) ) {
			if ( isLoading ) {
				Com_DPrintf( S_COLOR_YELLOW "WARNING: Duplicate string found: \"%s\"\n", original );
			}
			return t;
		}
	}

	newt = AllocTrans( original, translated );

	if ( prev ) {
		prev->next = newt;
	} else {
		transTable[hash] = newt;
	}

	if ( cl_debugTranslation->integer >= 1 && !isLoading ) {
		Com_Printf( "Missing translation: \'%s\'\n", original );
	}

	// see if we want to save out the translation table everytime a string is added
	//if ( cl_debugTranslation->integer == 2 && !isLoading ) {
	//	CL_SaveTransTable();
	//}

	return newt;
}

/*
=======================
CL_SaveTransTable
=======================
*/
void CL_SaveTransTable( const char *fileName, qboolean newOnly ) {
	int bucketlen, bucketnum, maxbucketlen, avebucketlen;
	int untransnum, transnum;
	const char *buf;
	fileHandle_t f;
	trans_t *t;
	int i, j, len;

	if ( cl.corruptedTranslationFile ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Cannot save corrupted translation file. Please reload first." );
		return;
	}

	FS_FOpenFileByMode( fileName, &f, FS_WRITE );

	bucketnum = 0;
	maxbucketlen = 0;
	avebucketlen = 0;
	transnum = 0;
	untransnum = 0;

	// write out version, if one
	if ( strlen( cl.translationVersion ) ) {
		buf = va( "#version\t\t\"%s\"\n", cl.translationVersion );
	} else {
		buf = va( "#version\t\t\"1.0 01/01/01\"\n" );
	}

	len = strlen( buf );
	FS_Write( buf, len, f );

	// write out translated strings
	for ( j = 0; j < 2; j++ ) {

		for ( i = 0; i < FILE_HASH_SIZE; i++ ) {
			t = transTable[i];

			if ( !t || ( newOnly && t->fromFile ) ) {
				continue;
			}

			bucketlen = 0;

			for ( ; t; t = t->next ) {
				bucketlen++;

				if ( strlen( t->translated[0] ) ) {
					if ( j ) {
						continue;
					}
					transnum++;
				} else {
					if ( !j ) {
						continue;
					}
					untransnum++;
				}

				buf = va( "{\n\tenglish\t\t\"%s\"\n", t->original );
				len = strlen( buf );
				FS_Write( buf, len, f );

				buf = va( "\tfrench\t\t\"%s\"\n", t->translated[LANGUAGE_FRENCH] );
				len = strlen( buf );
				FS_Write( buf, len, f );

				buf = va( "\tgerman\t\t\"%s\"\n", t->translated[LANGUAGE_GERMAN] );
				len = strlen( buf );
				FS_Write( buf, len, f );

				buf = va( "\titalian\t\t\"%s\"\n", t->translated[LANGUAGE_ITALIAN] );
				len = strlen( buf );
				FS_Write( buf, len, f );

				buf = va( "\tspanish\t\t\"%s\"\n", t->translated[LANGUAGE_SPANISH] );
				len = strlen( buf );
				FS_Write( buf, len, f );

				buf = "}\n";
				len = strlen( buf );
				FS_Write( buf, len, f );
			}

			if ( bucketlen > maxbucketlen ) {
				maxbucketlen = bucketlen;
			}

			if ( bucketlen ) {
				bucketnum++;
				avebucketlen += bucketlen;
			}
		}
	}

	Com_Printf( "Saved translation table.\nTotal = %i, Translated = %i, Untranslated = %i, aveblen = %2.2f, maxblen = %i\n",
				transnum + untransnum, transnum, untransnum, (float)avebucketlen / bucketnum, maxbucketlen );

	FS_FCloseFile( f );
}

/*
=======================
CL_CheckTranslationString

NERVE - SMF - compare formatting characters
=======================
*/
static qboolean CL_CheckTranslationString( const char *original, const char *translated ) {
	char format_org[128], format_trans[128];
	int len, i;

	memset( format_org, 0, sizeof(format_org) );
	memset( format_trans, 0, sizeof(format_trans) );

	// generate formatting string for original
	len = strlen( original );

	for ( i = 0; i < len; i++ ) {
		if ( original[i] != '%' ) {
			continue;
		}

		strcat( format_org, va( "%c%c ", '%', original[i + 1] ) );
	}

	// generate formatting string for translated
	len = strlen( translated );
	if ( !len ) {
		return qtrue;
	}

	for ( i = 0; i < len; i++ ) {
		if ( translated[i] != '%' ) {
			continue;
		}

		strcat( format_trans, va( "%c%c ", '%', translated[i + 1] ) );
	}

	// compare
	len = strlen( format_org );

	if ( len != (int)strlen( format_trans ) ) {
		return qfalse;
	}

	for ( i = 0; i < len; i++ ) {
		if ( format_org[i] != format_trans[i] ) {
			return qfalse;
		}
	}

	return qtrue;
}

/*
=======================
CL_LoadTransTable
=======================
*/
void CL_LoadTransTable( const char *fileName ) {
	char translated[MAX_LANGUAGES][MAX_VA_STRING];
	char original[MAX_VA_STRING];
	qboolean aborted;
	char *text;
	fileHandle_t f;
	const char *text_p;
	const char *token;
	int len, i;
	trans_t *t;
	int count;

	count = 0;
	aborted = qfalse;
	cl.corruptedTranslationFile = qfalse;

	len = FS_FOpenFileByMode( fileName, &f, FS_READ );
	if ( len <= 0 ) {
		return;
	}

	// Gordon: shouldn't this be a z_malloc or something?
	text = malloc( len + 1 );
	if ( !text ) {
		return;
	}

	FS_Read( text, len, f );
	text[len] = '\0';
	FS_FCloseFile( f );

	// parse the text
	text_p = text;

	do {
		token = COM_Parse( &text_p );
		if ( Q_stricmp( "{", token ) ) {
			// parse version number
			if ( !Q_stricmp( "#version", token ) ) {
				token = COM_Parse( &text_p );
				strcpy( cl.translationVersion, token );
				continue;
			}

			break;
		}

		// english
		token = COM_Parse( &text_p );
		if ( Q_stricmp( "english", token ) ) {
			aborted = qtrue;
			break;
		}

		token = COM_Parse( &text_p );
		strcpy( original, token );

		if ( cl_debugTranslation->integer == 3 ) {
			Com_Printf( "%i Loading: \"%s\"\n", count, original );
		}

		// french
		token = COM_Parse( &text_p );
		if ( Q_stricmp( "french", token ) ) {
			aborted = qtrue;
			break;
		}

		token = COM_Parse( &text_p );
		strcpy( translated[LANGUAGE_FRENCH], token );
		if ( !CL_CheckTranslationString( original, translated[LANGUAGE_FRENCH] ) ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: Translation formatting doesn't match up with English version!\n" );
			aborted = qtrue;
			break;
		}

		// german
		token = COM_Parse( &text_p );
		if ( Q_stricmp( "german", token ) ) {
			aborted = qtrue;
			break;
		}

		token = COM_Parse( &text_p );
		strcpy( translated[LANGUAGE_GERMAN], token );
		if ( !CL_CheckTranslationString( original, translated[LANGUAGE_GERMAN] ) ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: Translation formatting doesn't match up with English version!\n" );
			aborted = qtrue;
			break;
		}

		// italian
		token = COM_Parse( &text_p );
		if ( Q_stricmp( "italian", token ) ) {
			aborted = qtrue;
			break;
		}

		token = COM_Parse( &text_p );
		strcpy( translated[LANGUAGE_ITALIAN], token );
		if ( !CL_CheckTranslationString( original, translated[LANGUAGE_ITALIAN] ) ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: Translation formatting doesn't match up with English version!\n" );
			aborted = qtrue;
			break;
		}

		// spanish
		token = COM_Parse( &text_p );
		if ( Q_stricmp( "spanish", token ) ) {
			aborted = qtrue;
			break;
		}

		token = COM_Parse( &text_p );
		strcpy( translated[LANGUAGE_SPANISH], token );
		if ( !CL_CheckTranslationString( original, translated[LANGUAGE_SPANISH] ) ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: Translation formatting doesn't match up with English version!\n" );
			aborted = qtrue;
			break;
		}

		// do lookup
		t = LookupTrans( original, NULL, qtrue );

		if ( t ) {
			t->fromFile = qtrue;

			for ( i = 0; i < MAX_LANGUAGES; i++ )
				Q_strncpyz( t->translated[i], translated[i], MAX_TRANS_STRING );
		}

		token = COM_Parse( &text_p );

		// set offset if we have one
		if ( !Q_stricmp( "offset", token ) ) {
			token = COM_Parse( &text_p );
			t->x_offset = Q_atof( token );

			token = COM_Parse( &text_p );
			t->y_offset = Q_atof( token );

			token = COM_Parse( &text_p );
		}

		if ( Q_stricmp( "}", token ) ) {
			aborted = qtrue;
			break;
		}

		count++;
	} while ( token );

	if ( aborted ) {
		int line = 1;

		for ( i = 0; i < len; i++ ) {
			if ( text[i] == '\n' ) {
				line++;
			}
		}

		Com_Printf( S_COLOR_YELLOW "WARNING: Problem loading %s on line %i\n", fileName, line );
		cl.corruptedTranslationFile = qtrue;
	} else {
		Com_Printf( "Loaded %i translation strings from %s\n", count, fileName );
	}

	// cleanup
	free( text );
}

/*
=======================
CL_ReloadTranslation
=======================
*/
void CL_ReloadTranslation() {
	char    **fileList;
	int numFiles, i;

	for ( i = 0; i < FILE_HASH_SIZE; i++ ) {
		if ( transTable[i] ) {
			free( transTable[i] );
		}
	}

	memset( transTable, 0, sizeof( trans_t* ) * FILE_HASH_SIZE );
	CL_LoadTransTable( "scripts/translation.cfg" );

	fileList = FS_ListFiles( "translations", "cfg", &numFiles );

	for ( i = 0; i < numFiles; i++ ) {
		CL_LoadTransTable( va( "translations/%s", fileList[i] ) );
	}
}

/*
=======================
CL_InitTranslation
=======================
*/
void CL_InitTranslation() {
	char    **fileList;
	int numFiles, i;

	memset( transTable, 0, sizeof( trans_t* ) * FILE_HASH_SIZE );
	CL_LoadTransTable( "scripts/translation.cfg" );

	fileList = FS_ListFiles( "translations", ".cfg", &numFiles );

	for ( i = 0; i < numFiles; i++ ) {
		CL_LoadTransTable( va( "translations/%s", fileList[i] ) );
	}
}

#else
typedef struct trans_s {
	char original[MAX_TRANS_STRING];
	struct  trans_s *next;
	float x_offset;
	float y_offset;
} trans_t;

#endif  //DAJ USA

/*
=======================
CL_TranslateString
=======================
*/
void CL_TranslateString( const char *string, char *dest_buffer ) {
#if !defined(__APPLE__) && !defined(__APPLE_CC__)
	int i, count;
	trans_t *t;
	qboolean newline = qfalse;
#endif
	int currentLanguage;
	char *buf;

	buf = dest_buffer;
	currentLanguage = cl_language->integer - 1;

	// early bail if we only want english or bad language type
	if ( !string ) {
		strcpy( buf, "(null)" );
		return;
	} else if ( currentLanguage < 0 || currentLanguage >= MAX_LANGUAGES || !strlen( string ) )   {
		strcpy( buf, string );
		return;
	}
#if !defined(__APPLE__) && !defined(__APPLE_CC__)
	// ignore newlines
	if ( string[strlen( string ) - 1] == '\n' ) {
		newline = qtrue;
	}

	for ( i = 0, count = 0; string[i] != '\0'; i++ ) {
		if ( string[i] != '\n' ) {
			buf[count++] = string[i];
		}
	}
	buf[count] = '\0';

	t = LookupTrans( buf, NULL, qfalse );

	if ( t && strlen( t->translated[currentLanguage] ) ) {
		int offset = 0;

		if ( cl_debugTranslation->integer >= 1 ) {
			buf[0] = '^';
			buf[1] = '1';
			buf[2] = '[';
			offset = 3;
		}

		strcpy( buf + offset, t->translated[currentLanguage] );

		if ( cl_debugTranslation->integer >= 1 ) {
			int len2 = strlen( buf );

			buf[len2] = ']';
			buf[len2 + 1] = '^';
			buf[len2 + 2] = '7';
			buf[len2 + 3] = '\0';
		}

		if ( newline ) {
			int len2 = strlen( buf );

			buf[len2] = '\n';
			buf[len2 + 1] = '\0';
		}
	} else {
		int offset = 0;

		if ( cl_debugTranslation->integer >= 1 ) {
			buf[0] = '^';
			buf[1] = '1';
			buf[2] = '[';
			offset = 3;
		}

		strcpy( buf + offset, string );

		if ( cl_debugTranslation->integer >= 1 ) {
			int len2 = strlen( buf );
			qboolean addnewline = qfalse;

			if ( buf[len2 - 1] == '\n' ) {
				len2--;
				addnewline = qtrue;
			}

			buf[len2] = ']';
			buf[len2 + 1] = '^';
			buf[len2 + 2] = '7';
			buf[len2 + 3] = '\0';

			if ( addnewline ) {
				buf[len2 + 3] = '\n';
				buf[len2 + 4] = '\0';
			}
		}
	}
#endif //DAJ USA
}

/*
=======================
CL_TranslateStringBuf
TTimo - handy, stores in a static buf, converts \n to chr(13)
=======================
*/
const char* CL_TranslateStringBuf( const char *string ) {
	char *p;
	int i,l;
	static char buf[MAX_VA_STRING];
	CL_TranslateString( string, buf );
	while ( ( p = strstr( buf, "\\n" ) ) != NULL )
	{
		*p = '\n';
		p++;
		// Com_Memcpy(p, p+1, strlen(p) ); b0rks on win32
		l = strlen( p );
		for ( i = 0; i < l; i++ )
		{
			*p = *( p + 1 );
			p++;
		}
	}
	return buf;
}


void CL_TrackCvarChanges( qboolean force ) {
#ifndef USE_SDL
	if ( force || Cvar_CheckGroup( CVG_LANGUAGE ) ) {
		if ( cl_language->integer > 0 && in_forceCharset->integer > 0 ) {
			Com_Printf( "WARNING: in_forceCharset incompatible with non-English languages!\n"
						"Setting in_forceCharset to 0.\n");
			Cvar_Set( "in_forceCharset", "0" );
		}

		Cvar_ResetGroup( CVG_LANGUAGE, qfalse );
	}
#endif
}

/*
=======================
CL_OpenURLForCvar
=======================
*/
void CL_OpenURL( const char *url ) {
	if ( !url || !strlen( url ) ) {
		Com_Printf( "%s", CL_TranslateStringBuf( "invalid/empty URL\n" ) );
		return;
	}
	Sys_OpenURL( url, qtrue );
}

#ifdef USE_CURL

qboolean CL_Download( const char *cmd, const char *pakname, qboolean autoDownload )
{
	char url[MAX_OSPATH];
	char name[MAX_CVAR_VALUE_STRING];
	const char *s;

	if ( cl_dlURL->string[0] == '\0' )
	{
		Com_Printf( S_COLOR_YELLOW "cl_dlURL cvar is not set\n" );
		return qfalse;
	}

	// skip leading slashes
	while ( *pakname == '/' || *pakname == '\\' )
		pakname++;

	// skip gamedir
	s = strrchr( pakname, '/' );
	if ( s )
		pakname = s+1;

	if ( !Com_DL_ValidFileName( pakname ) )
	{
		Com_Printf( S_COLOR_YELLOW "invalid file name: '%s'.\n", pakname );
		return qfalse;
	}

	if ( !Q_stricmp( cmd, "dlmap" ) )
	{
		Q_strncpyz( name, pakname, sizeof( name ) );
		FS_StripExt( name, ".pk3" );
		if ( !name[0] )
			return qfalse;
		s = va( "maps/%s.bsp", name );
		if ( FS_FileIsInPAK( s, NULL, url ) )
		{
			Com_Printf( S_COLOR_YELLOW " map %s already exists in %s.pk3\n", name, url );
			return qfalse;
		}
	}

	return Com_DL_Begin( &download, pakname, cl_dlURL->string, autoDownload );
}


/*
==================
CL_Download_f
==================
*/
static void CL_Download_f( void )
{
	if ( Cmd_Argc() < 2 || *Cmd_Argv( 1 ) == '\0' )
	{
		Com_Printf( "usage: %s <mapname>\n", Cmd_Argv( 0 ) );
		return;
	}

	if ( !strcmp( Cmd_Argv(1), "-" ) )
	{
		Com_DL_Cleanup( &download );
		return;
	}

	CL_Download( Cmd_Argv( 0 ), Cmd_Argv( 1 ), qfalse );
}
#endif // USE_CURL
