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

#ifndef __CLIENT_H__
#define __CLIENT_H__

// client.h -- primary header for client

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
#include "../qcommon/vm_local.h"
#include "../ui/ui_public.h"
#include "../cgame/cg_public.h"
#include "../game/bg_public.h"
#include "snd_public.h"
#include "keys.h"

#ifndef DEDICATED
#ifndef USE_CURL
#define USE_CURL
#endif
#endif

#ifdef USE_CURL
#include "cl_curl.h"
#endif

#define RETRANSMIT_TIMEOUT  3000    // time between connection packet retransmits

#define ETKEY_FILE "etkey"
#define ETKEY_SIZE 28

// snapshots are a view of the server at a given time
typedef struct {
	qboolean valid;                 // cleared if delta parsing was invalid
	int snapFlags;                  // rate delayed and dropped commands

	int serverTime;                 // server time the message is valid for (in msec)

	int messageNum;                 // copied from netchan->incoming_sequence
	int deltaNum;                   // messageNum the delta is from
	int ping;                       // time from when cmdNum-1 was sent to time packet was reeceived
	int				areabytes;
	byte areamask[MAX_MAP_AREA_BYTES];                  // portalarea visibility bits

	int cmdNum;                     // the next cmdNum the server is expecting
	playerState_t ps;                       // complete information about the current player at this time

	int numEntities;                        // all of the entities that need to be presented
	int parseEntitiesNum;                   // at the time of this snapshot

	int serverCommandNum;                   // execute all commands up to this before
											// making the snapshot current
} clSnapshot_t;

// Arnout: for double tapping
typedef struct {
	int pressedTime[DT_NUM];
	int releasedTime[DT_NUM];

	int lastdoubleTap;
} doubleTap_t;

#ifdef USE_DISCORD
typedef struct discordState_s {
	char hostName[MAX_CVAR_VALUE_STRING];
	char mapName[MAX_QPATH];
	char gameDir[MAX_QPATH];
	qboolean isETF;
	qboolean needPassword;
	int gametype, timelimit;
	int playerCount, maxPlayers, botCount, specCount;
	int redTeam, blueTeam, yellowTeam, greenTeam;
	int playerClass, playerTeam;
} discordState_t;
#endif

/*
=============================================================================

the clientActive_t structure is wiped completely at every
new gamestate_t, potentially several times during an established connection

=============================================================================
*/

typedef struct {
	int p_cmdNumber;            // cl.cmdNumber when packet was sent
	int p_serverTime;           // usercmd->serverTime when packet was sent
	int p_realtime;             // cls.realtime when packet was sent
} outPacket_t;

// the parseEntities array must be large enough to hold PACKET_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original
#define	MAX_PARSE_ENTITIES	( PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES )
// ENSI FIXME this was #define MAX_PARSE_ENTITIES  2048 in ET

extern int g_console_field_width;

typedef struct {
	int timeoutcount;               // it requres several frames in a timeout condition
									// to disconnect, preventing debugging breaks from
									// causing immediate disconnects on continue
	clSnapshot_t snap;              // latest received from server

	int serverTime;                 // may be paused during play
	int oldServerTime;              // to prevent time from flowing bakcwards
	int oldFrameServerTime;         // to check tournament restarts
	int serverTimeDelta;            // cl.serverTime = cls.realtime + cl.serverTimeDelta
									// this value changes as net lag varies
	qboolean extrapolatedSnapshot;      // set if any cgame frame has been forced to extrapolate
	// cleared when CL_AdjustTimeDelta looks at it
	qboolean newSnapshots;          // set on parse of any valid packet

	gameState_t gameState;          // configstrings
	char mapname[MAX_QPATH];        // extracted from CS_SERVERINFO

	int parseEntitiesNum;           // index (not anded off) into cl_parse_entities[]

	int mouseDx[2], mouseDy[2];         // added to by mouse events
	int mouseIndex;
	int joystickAxis[MAX_JOYSTICK_AXIS];            // set by joystick events

	// cgame communicates a few values to the client system
	int cgameUserCmdValue;              // current weapon to add to usercmd_t
	int cgameFlags;                     // flags that can be set by the gamecode
	float cgameSensitivity;
	int cgameMpIdentClient;             // NERVE - SMF
	vec3_t cgameClientLerpOrigin;       // DHM - Nerve

	// cmds[cmdNumber] is the predicted command, [cmdNumber-1] is the last
	// properly generated command
	usercmd_t cmds[CMD_BACKUP];     // each mesage will send several old cmds
	int cmdNumber;                  // incremented each frame, because multiple
									// frames may need to be packed into a single packet

	// Arnout: double tapping
	doubleTap_t doubleTap;

	outPacket_t outPackets[PACKET_BACKUP];  // information about each packet we have sent out

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t viewangles;

	int serverId;                   // included in each client message so the server
									// can tell if it is for a prior map_restart
	// big stuff at end of structure so most offsets are 15 bits or less
	clSnapshot_t snapshots[PACKET_BACKUP];

	entityState_t entityBaselines[MAX_GENTITIES];   // for delta compression when not in previous frame

	entityState_t parseEntities[MAX_PARSE_ENTITIES];

	qboolean corruptedTranslationFile;
	char translationVersion[MAX_STRING_TOKENS];
	// -NERVE - SMF

	qboolean cameraMode;

	byte			baselineUsed[MAX_GENTITIES];

#ifdef USE_DISCORD
	discordState_t	discord;
#endif
} clientActive_t;

extern	clientActive_t		cl;

#define EM_GAMESTATE 1
#define EM_SNAPSHOT  2
#define EM_COMMAND   4
#define EM_BINARYMESSAGE 8

/*
=============================================================================

the clientConnection_t structure is wiped when disconnecting from a server,
either to go to a full screen console, play a demo, or connect to a different server

A connection can be to either a server through the network layer or a
demo through a file.

=============================================================================
*/


typedef struct {

	int clientNum;
	int lastPacketSentTime;                 // for retransmits during connection
	int lastPacketTime;                     // for timeouts

	netadr_t serverAddress;
	int connectTime;                        // for connection retransmits
	int connectPacketCount;                 // for display on connection dialog
	char serverMessage[MAX_STRING_CHARS];	// for display on connection dialog

	int challenge;                          // from the server to use for connecting
	int checksumFeed;                       // from the server for checksum calculations

	int onlyVisibleClients;                 // DHM - Nerve

	// these are our reliable messages that go to the server
	int reliableSequence;
	int reliableAcknowledge;                // the last one the server has executed
	// TTimo - NOTE: incidentally, reliableCommands[0] is never used (always start at reliableAcknowledge+1)
	char reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];

	// unreliable binary data to send to server
	int binaryMessageLength;
	char binaryMessage[MAX_BINARY_MESSAGE];
	qboolean binaryMessageOverflowed;

	// server message (unreliable) and command (reliable) sequence
	// numbers are NOT cleared at level changes, but continue to
	// increase as long as the connection is valid

	// message sequence is used by both the network layer and the
	// delta compression layer
	int serverMessageSequence;

	// reliable messages received from server
	int			serverCommandSequence;
	int			lastExecutedServerCommand;		// last server command grabbed or executed with CL_GetServerCommand
	char		serverCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
	qboolean	serverCommandsIgnore[MAX_RELIABLE_COMMANDS];

	// file transfer from server
	fileHandle_t download;
	char		downloadName[MAX_OSPATH];
	char		downloadTempName[MAX_OSPATH + 4]; // downloadName + ".tmp"
	int downloadNumber;
	int downloadBlock;          // block we are waiting for
	int downloadCount;          // how many bytes we got
	int downloadSize;           // how many bytes we got
	int downloadFlags;         // misc download behaviour flags sent by the server
	char downloadList[BIG_INFO_STRING];        // list of paks we need to download
	qboolean	downloadRestart;	// if true, we need to do another FS_Restart because we downloaded a pak
	// www downloading
	qboolean bWWWDl;    // we have a www download going
	qboolean bWWWDlAborting;    // disable the CL_WWWDownload until server gets us a gamestate (used for aborts)
	char redirectedList[MAX_INFO_STRING];        // list of files that we downloaded through a redirect since last FS_ComparePaks
	char badChecksumList[MAX_INFO_STRING];        // list of files for which wwwdl redirect is broken (wrong checksum)

	// demo information
	char		demoName[MAX_OSPATH];
	char		recordName[MAX_OSPATH]; // without extension
	qboolean	explicitRecordName;
	char		recordNameShort[TRUNCATE_LENGTH]; // for recording message
	qboolean	dm84compat;
	qboolean	demorecording;
	qboolean	demoplaying;
	qboolean	demowaiting;	// don't record until a non-delta message is received
	qboolean	firstDemoFrameSkipped;
	fileHandle_t	demofile;
	fileHandle_t	recordfile;

	qboolean waverecording;
	fileHandle_t wavefile;
	int wavetime;

	int timeDemoFrames;             // counter of rendered frames
	int timeDemoStart;              // cls.realtime before first frame
	int timeDemoBaseTime;           // each frame will be at this time + frameNum * 50

	float	aviVideoFrameRemainder;
	float	aviSoundFrameRemainder;
	int		aviFrameEndTime;
	char	videoName[MAX_OSPATH];
	int		videoIndex;

	// big stuff at end of structure so most offsets are 15 bits or less
	netchan_t	netchan;

	qboolean compat;

	// simultaneous demo playback and recording
	int		eventMask;
	int		demoCommandSequence;
	int		demoDeltaNum;
	int		demoMessageSequence;

} clientConnection_t;

extern	clientConnection_t clc;

/*
==================================================================

the clientStatic_t structure is never wiped, and is used even when
no client connection is active at all

==================================================================
*/

typedef struct {
	netadr_t adr;
	int start;
	int time;
	char info[MAX_INFO_STRING];
} ping_t;

typedef struct {
	netadr_t adr;
	char hostName[MAX_NAME_LENGTH];
	int load;
	char mapName[MAX_NAME_LENGTH];
	char game[MAX_NAME_LENGTH];
	int netType;
	int gameType;
	int clients;
	int maxClients;
	int minPing;
	int maxPing;
	int ping;
	qboolean visible;
	int friendlyFire;               // NERVE - SMF
	int maxlives;                   // NERVE - SMF
	int needpass;
	int punkbuster;                 // DHM - Nerve
	int antilag;         // TTimo
	int weaprestrict;
	int balancedteams;
	int oss;
	char gameName[MAX_NAME_LENGTH];         // Arnout
} serverInfo_t;

typedef struct {
	connstate_t	state;				// connection status
	qboolean	gameSwitch;

	qboolean cddialog;              // bring up the cd needed dialog next frame

	qboolean doCachePurge;          // Arnout: empty the renderer cache as soon as possible

	char		servername[MAX_OSPATH];		// name of server from original connect (used by reconnect)

	// when the server clears the hunk, all of these must be restarted
	qboolean	rendererStarted;
	qboolean	soundStarted;
	qboolean	soundRegistered;
	qboolean	uiStarted;
	qboolean	cgameStarted;

	int			framecount;
	int			frametime;			// msec since last frame

	int			realtime;			// ignores pause
	int			realFrametime;		// ignoring pause, so console always works

#ifdef USE_DISCORD
	qboolean	discordInit;
	int			discordUpdateTime;
#endif

	int			numlocalservers;
	serverInfo_t	localServers[MAX_OTHER_SERVERS];

	int			numglobalservers;
	serverInfo_t  globalServers[MAX_GLOBAL_SERVERS];
	// additional global servers
	int			numGlobalServerAddresses;
	netadr_t		globalServerAddresses[MAX_GLOBAL_SERVERS];

	int			numfavoriteservers;
	serverInfo_t	favoriteServers[MAX_OTHER_SERVERS];

	int pingUpdateSource;		// source currently pinging or updating

	// update server info
	netadr_t	updateServer;
	char		updateChallenge[MAX_TOKEN_CHARS];
	char		updateInfoString[MAX_INFO_STRING];

	// rendering info
	glconfig_t glconfig;
	qhandle_t charSetShader;
	qhandle_t whiteShader;
	qhandle_t consoleShader;
	qhandle_t consoleShader2;       // NERVE - SMF - merged from WolfSP

	//int			lastVidRestart;

	// www downloading
	// in the static stuff since this may have to survive server disconnects
	// if new stuff gets added, CL_ClearStaticDownload code needs to be updated for clear up
	qboolean bWWWDlDisconnected; // keep going with the download after server disconnect
	char downloadName[MAX_OSPATH];
	char downloadTempName[MAX_OSPATH+4];    // in wwwdl mode, this is OS path (it's a qpath otherwise)
	char originalDownloadName[MAX_QPATH];    // if we get a redirect, keep a copy of the original file path
	qboolean downloadRestart; // if true, we need to do another FS_Restart because we downloaded a pak

	qboolean	startCgame;

	int			captureWidth;
	int			captureHeight;

	float		con_factor;

	float		scale;
	float		biasX;
	float		biasY;

} clientStatic_t;

extern int bigchar_width;
extern int bigchar_height;
extern int smallchar_width;
extern int smallchar_height;

extern	clientStatic_t		cls;

extern	char		cl_oldGame[MAX_QPATH];
extern	qboolean	cl_oldGameSet;

#ifdef USE_CURL

extern		download_t	download;
qboolean	Com_DL_Perform( download_t *dl );
void		Com_DL_Cleanup( download_t *dl );
qboolean	Com_DL_Begin( download_t *dl, const char *localName, const char *remoteURL, qboolean autoDownload );
qboolean	Com_DL_InProgress( const download_t *dl );
qboolean	Com_DL_ValidFileName( const char *fileName );
qboolean	CL_Download( const char *cmd, const char *pakname, qboolean autoDownload );

#endif

//=============================================================================

extern	vm_t			*cgvm;	// interface to cgame dll or vm
extern	vm_t			*uivm;	// interface to ui dll or vm
extern	refexport_t		re;		// interface to refresh .dll


//
// cvars
//
extern cvar_t  *cl_noprint;
extern cvar_t  *cl_debugMove;
extern cvar_t  *cl_timegraph;
extern cvar_t  *cl_shownet;
extern cvar_t  *cl_shownuments;             // DHM - Nerve
extern cvar_t  *cl_showServerCommands;      // NERVE - SMF
extern	cvar_t	*cl_autoNudge;
extern	cvar_t	*cl_timeNudge;
extern	cvar_t	*cl_showTimeDelta;

extern cvar_t  *cl_bypassMouseInput;    // NERVE - SMF

extern	cvar_t	*com_timedemo;
extern	cvar_t	*cl_avidemo;
extern	cvar_t	*cl_aviFrameRate;
extern	cvar_t	*cl_aviMotionJpeg;
extern	cvar_t	*cl_forceavidemo;
extern	cvar_t	*cl_aviPipeFormat;

extern	cvar_t	*cl_activeAction;

extern	cvar_t	*cl_allowDownload;
#ifdef USE_CURL
extern	cvar_t	*cl_mapAutoDownload;
extern	cvar_t	*cl_dlDirectory;
#endif
extern	cvar_t	*cl_conXOffset;
extern	cvar_t	*cl_conColor;
extern	cvar_t	*cl_inGameVideo;

// NERVE - SMF - localization
extern cvar_t  *cl_language;
// -NERVE - SMF

extern cvar_t  *cl_profile;
extern cvar_t  *cl_defaultProfile;

extern	cvar_t	*cl_lanForcePackets;
extern	cvar_t	*cl_autoRecordDemo;

#ifdef USE_DISCORD
extern	cvar_t	*cl_discordRichPresence;
#endif

extern	cvar_t	*com_maxfps;

extern	cvar_t	*vid_xpos;
extern	cvar_t	*vid_ypos;
extern	cvar_t	*r_noborder;

extern	cvar_t	*r_allowSoftwareGL;
extern	cvar_t	*r_swapInterval;
#ifndef USE_SDL
extern	cvar_t	*r_glDriver;
#else
extern	cvar_t	*r_sdlDriver;
extern	cvar_t	*r_sdlAllowScreenSaver;
#endif
extern	cvar_t	*r_displayRefresh;
extern	cvar_t	*r_fullscreen;
extern	cvar_t	*r_mode;
extern	cvar_t	*r_modeFullscreen;
extern	cvar_t	*r_customwidth;
extern	cvar_t	*r_customheight;
extern	cvar_t	*r_customaspect;
extern	cvar_t	*r_colorbits;
extern	cvar_t	*cl_stencilbits;
extern	cvar_t	*cl_depthbits;
extern	cvar_t	*cl_drawBuffer;
#ifndef USE_SDL
extern	cvar_t	*in_forceCharset;
#endif

//=================================================

//
// cl_main
//
void CL_AddReliableCommand( const char *cmd, qboolean isDisconnectCmd );

void CL_StartHunkUsers( void );

void CL_Disconnect_f( void );
void CL_ReadDemoMessage( void );
void CL_StopRecord_f(void);

void CL_InitDownloads(void);
void CL_NextDownload(void);

void CL_GetPing( int n, char *buf, int buflen, int *pingtime );
void CL_GetPingInfo( int n, char *buf, int buflen );
void CL_ClearPing( int n );
int CL_GetPingQueueCount( void );

void CL_ClearState( void );

int CL_ServerStatus( const char *serverAddress, char *serverStatusString, int maxLen );

// NERVE - SMF - localization
void CL_InitTranslation();
void CL_SaveTransTable( const char *fileName, qboolean newOnly );
void CL_ReloadTranslation();
void CL_TranslateString( const char *string, char *dest_buffer );
const char* CL_TranslateStringBuf( const char *string ); // TTimo
// -NERVE - SMF

void CL_TrackCvarChanges( qboolean force );

void CL_OpenURL( const char *url ); // TTimo

qboolean CL_CheckPaused( void );
qboolean CL_NoDelay( void );

qboolean CL_GetModeInfo( int *width, int *height, float *windowAspect, int mode, const char *modeFS, int dw, int dh, qboolean fullscreen );


//
// cl_input
//

typedef enum {
	KB_LEFT = 0,
	KB_RIGHT,
	KB_FORWARD,
	KB_BACK,
	KB_LOOKUP,
	KB_LOOKDOWN,
	KB_MOVELEFT,
	KB_MOVERIGHT,
	KB_STRAFE,
	KB_SPEED,
	KB_UP,
	KB_DOWN,
	KB_BUTTONS0,
	KB_BUTTONS1,
	KB_BUTTONS2,
	KB_BUTTONS3,
	KB_BUTTONS4,
	KB_BUTTONS5,
	KB_BUTTONS6,
	KB_BUTTONS7,
	KB_WBUTTONS0,
	KB_WBUTTONS1,
	KB_WBUTTONS2,
	KB_WBUTTONS3,
	KB_WBUTTONS4,
	KB_WBUTTONS5,
	KB_WBUTTONS6,
	KB_WBUTTONS7,
	KB_MLOOK,
	NUM_BUTTONS
} kbuttons_t;


void CL_InitInput( void );
void CL_ShutdownInput( void );
void CL_SendCmd( void );

void CL_WritePacket( void );
//void IN_CenterView (void);

//
// cl_keys.c
//
extern  field_t     chatField;
extern  field_t     g_consoleField;
extern	qboolean	chat_team;
extern	qboolean	chat_buddy;

void Field_Draw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape );
void Field_BigDraw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape );

const char *Key_KeynumToString( int keynum, qboolean bTranslate );

//
// cl_parse.c
//
extern int cl_connectedToPureServer;
extern int cl_connectedToCheatServer;
extern int cl_optimizedPatchServer;

void CL_ParseServerInfo(void);
#ifdef USE_DISCORD
void CL_ParsePlayerInfo(void);
#endif

void CL_ParseServerMessage( msg_t *msg );

//====================================================================

qboolean CL_UpdateVisiblePings_f( int source );
qboolean CL_ValidPakSignature( const byte *data, int len );


//
// console
//
#define NUM_CON_TIMES 4

//#define		CON_TEXTSIZE	32768
//#define     CON_TEXTSIZE    65536   // (SA) DM want's more console...
#define     CON_TEXTSIZE    (256*1024)

typedef struct {
	qboolean initialized;

	short text[CON_TEXTSIZE];
	int current;            // line where next message will be printed
	int x;                  // offset in current line for next print
	int display;            // bottom of console displays this line

	int linewidth;          // characters across screen
	int totallines;         // total lines in console scrollback

	float xadjust;          // for wide aspect screens

	float displayFrac;      // aproaches finalFrac at scr_conspeed
	float finalFrac;        // 0.0 to 1.0 lines of console to display
	float desiredFrac;      // ydnar: for variable console heights

	int vislines;           // in scanlines

	int times[NUM_CON_TIMES];       // cls.realtime time the line was generated
	// for transparent notify lines
	vec4_t color;

	int		viswidth;
	int		vispage;

	qboolean newline;

	int acLength;           // Arnout: autocomplete buffer length
} console_t;

extern console_t con;

extern cvar_t *con_scale;

void Con_CheckResize( void );
void Con_Init( void );
void Con_Shutdown( void );
void Con_ToggleConsole_f( void );
void Con_ClearNotify( void );
void Con_RunConsole( void );
void Con_DrawConsole( void );
void Con_PageUp( int lines );
void Con_PageDown( int lines );
void Con_Top( void );
void Con_Bottom( void );
void Con_Close( void );

//
// cl_scrn.c
//
void	SCR_Init( void );
void	SCR_Done( void );
void	SCR_UpdateScreen( void );

void    SCR_DebugGraph( float value );

int     SCR_GetBigStringWidth( const char *str );   // returns in virtual 640x480 coordinates

void    SCR_AdjustFrom640( float *x, float *y, float *w, float *h );
void    SCR_FillRect( float x, float y, float width, float height,
					  const float *color );
void    SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader );
void    SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname );

void	SCR_DrawBigString( int x, int y, const char *s, float alpha, qboolean noColorEscape );			// draws a string with embedded color control characters with fade
void	SCR_DrawStringExt( int x, int y, float size, const char *string, const float *setColor, qboolean forceColor, qboolean noColorEscape );
void	SCR_DrawSmallStringExt( int x, int y, const char *string, const float *setColor, qboolean forceColor, qboolean noColorEscape );
void	SCR_DrawSmallChar( int x, int y, int ch );
void	SCR_DrawSmallString( int x, int y, const char *s, int len );

//
// cl_cin.c
//

void CL_CompleteCinematicName( char *args, int argNum );
void CL_PlayCinematic_f( void );
void SCR_DrawCinematic( void );
void SCR_RunCinematic( void );
void SCR_StopCinematic( void );
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits );
e_status CIN_StopCinematic( int handle );
e_status CIN_RunCinematic( int handle );
void CIN_DrawCinematic( int handle );
void CIN_SetExtents( int handle, int x, int y, int w, int h );
void CIN_SetLooping( int handle, qboolean loop );
void CIN_UploadCinematic( int handle );
void CIN_CloseAllVideos( void );

//
// cl_cgame.c
//
void CL_InitCGame( void );
void CL_ShutdownCGame( void );
qboolean CL_GameCommand( void );
qboolean CL_CgameRunning( void );
void CL_CGameRendering( stereoFrame_t stereo );
void CL_SetCGameTime( void );
void CL_UpdateLevelHunkUsage( void );
void CL_CGameBinaryMessageReceived( const char *buf, int buflen, int serverTime );

//
// cl_ui.c
//
void CL_InitUI( void );
void CL_ShutdownUI( void );
int Key_GetCatcher( void );
void Key_SetCatcher( int catcher );
int LAN_AddFavAddr( const char *address );


//
// cl_net_chan.c
//
void CL_Netchan_Transmit( netchan_t *chan, msg_t *msg );
qboolean CL_Netchan_Process( netchan_t *chan, msg_t *msg );

//
// cl_avi.c
//

typedef enum aviRecordingState_e {
	AVIDEMO_NONE = 0,
	AVIDEMO_VIDEO,
	AVIDEMO_CVAR,
	AVIDEMO_MAX
} aviRecordingState_t;

qboolean CL_OpenAVIForWriting( const char *filename, qboolean pipe, qboolean reopen );
void CL_TakeVideoFrame( void );
void CL_WriteAVIVideoFrame( const byte *imageBuffer, int size );
void CL_WriteAVIAudioFrame( const byte *pcmBuffer, int size );
qboolean CL_CloseAVI( qboolean reopen );
aviRecordingState_t CL_VideoRecording( void );

//
// cl_tc_vis.c
//
void tc_vis_init(void);
void tc_vis_render(void);

//
// cl_jpeg.c
//
size_t	CL_SaveJPGToBuffer( byte *buffer, size_t bufSize, int quality, int image_width, int image_height, byte *image_buffer, int padding );
void	CL_SaveJPG( const char *filename, int quality, int image_width, int image_height, byte *image_buffer, int padding );
void	CL_LoadJPG( const char *filename, byte **pic, int *width, int *height );

//
// cl_wav.c
qboolean CL_OpenWAVForWriting( const char *filename, qboolean pipe );
void CL_WriteWAVAudioFrame( const byte *pcmBuffer, int size );
qboolean CL_CloseWAV( void );
qboolean CL_WAVRecording( void );

#ifdef USE_DISCORD
void CL_DiscordInitialize(void);
void CL_DiscordShutdown(void);
void CL_DiscordUpdatePresence(void);
#endif

// base backend functions
void	HandleEvents( void );

// platform-specific
void	GLimp_InitGamma( glconfig_t *config );
void	GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] );

// OpenGL
#ifdef USE_OPENGL_API
void	GLimp_Init( glconfig_t *config );
void	GLimp_Shutdown( qboolean unloadDLL );
void	GLimp_EndFrame( void );
int		GLimp_NormalFontBase( void );
void	*GL_GetProcAddress( const char *name );
#endif

// Vulkan
#ifdef USE_VULKAN_API
void	VKimp_Init( glconfig_t *config );
void	VKimp_Shutdown( qboolean unloadDLL );
void	*VK_GetInstanceProcAddr( VkInstance instance, const char *name );
qboolean VK_CreateSurface( VkInstance instance, VkSurfaceKHR* pSurface );
#endif
#endif