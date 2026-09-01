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

// cl_console.c

#include "client.h"

#define DEFAULT_CONSOLE_WIDTH   78
#define MAX_CONSOLE_WIDTH 120

#define CONSOLE_COLOR  COLOR_WHITE //COLOR_BLACK

int bigchar_width;
int bigchar_height;
int smallchar_width;
int smallchar_height;

console_t con;

static cvar_t	*con_conspeed;
static cvar_t	*con_notifytime;
static cvar_t	*con_autoclear;
static cvar_t	*con_drawnotify;
static cvar_t	*con_maxnotify;
static cvar_t	*con_height;
// DHM - Nerve :: Must hold CTRL + SHIFT + ~ to get console
static cvar_t	*con_restricted;
cvar_t			*con_scale;

int				g_console_field_width;

static const vec4_t console_highlightcolor = {0.5, 0.5, 0.2, 0.45};

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void ) {
	qboolean consoleOpen = qfalse;
	con.acLength = 0;
	
	// Can't toggle the console when it's the only thing available
	if ( cls.state == CA_DISCONNECTED && Key_GetCatcher() == KEYCATCH_CONSOLE ) {
		return;
	}

	if ( con_restricted->integer && ( !keys[K_CTRL].down || !keys[K_SHIFT].down ) ) {
		return;
	}

	// ydnar: persistent console input is more useful
	// Arnout: added cvar
	if ( con_autoclear->integer ) {
		Field_Clear( &g_consoleField );
	}

	g_consoleField.widthInChars = g_console_field_width;

	Con_ClearNotify();

	// ydnar: multiple console size support
	consoleOpen = (Key_GetCatcher() & KEYCATCH_CONSOLE) ? qtrue : qfalse;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_CONSOLE );
	if ( consoleOpen ) {
		con.desiredFrac = 0.0;
	} else {
		// short console
		if ( keys[ K_CTRL ].down ) {
			con.desiredFrac = ( 5.0 * smallchar_height ) / cls.glconfig.vidHeight;
		}
		// full console
		else if ( keys[ K_ALT ].down ) {
			con.desiredFrac = 1.0;
		}
		// normal half-screen console
		else {
			con.desiredFrac = Com_ClampFloat(( 5.0 * smallchar_height ) / cls.glconfig.vidHeight, 1.0f, con_height->value);
		}
	}
}

/*
===================
Con_ToggleMenu_f
===================
*/
void Con_ToggleMenu_f( void ) {
	CL_KeyEvent( K_ESCAPE, qtrue, Sys_Milliseconds() );
	CL_KeyEvent( K_ESCAPE, qfalse, Sys_Milliseconds() );
}

/*
================
Con_MessageMode_f
================
*/
static void Con_MessageMode_f( void ) {
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;

	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}


/*
================
Con_MessageMode2_f
================
*/
static void Con_MessageMode2_f( void ) {
	chat_team = qtrue;
	Field_Clear( &chatField );
	chatField.widthInChars = 25;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}


/*
================
Con_MessageMode3_f
================
*/
static void Con_MessageMode3_f( void ) {
	chat_team = qfalse;
	chat_buddy = qtrue;
	Field_Clear( &chatField );
	chatField.widthInChars = 26;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}


/*
================
Con_Clear_f
================
*/
static void Con_Clear_f( void ) {
	int		i;

	for ( i = 0 ; i < con.linewidth ; i++ ) {
		con.text[i] = ( ColorIndex( CONSOLE_COLOR ) << 8 ) | ' ';
	}

	con.x = 0;
	con.current = 0;
	con.newline = qtrue;

	Con_Bottom();		// go to end
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
static void Con_Dump_f( void )
{
	int		l, x, i, n;
	short	*line;
	fileHandle_t	f;
	int		bufferlen;
	char	*buffer;
	char	filename[ MAX_OSPATH ];
	const char *ext;

	if ( Cmd_Argc() != 2 )
	{
		Com_Printf( "usage: condump <filename>\n" );
		return;
	}

	Q_strncpyz( filename, Cmd_Argv( 1 ), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".txt" );

	if ( !FS_AllowedExtension( filename, qfalse, &ext ) ) {
		Com_Printf( "%s: Invalid filename extension '%s'.\n", __func__, ext );
		return;
	}

	f = FS_FOpenFileWrite( filename );
	if ( f == FS_INVALID_HANDLE )
	{
		Com_Printf( "ERROR: couldn't open %s.\n", filename );
		return;
	}

	Com_Printf( "Dumped console text to %s.\n", filename );

	if ( con.current >= con.totallines ) {
		n = con.totallines;
		l = con.current + 1;
	} else {
		n = con.current + 1;
		l = 0;
	}

	bufferlen = con.linewidth + ARRAY_LEN( Q_NEWLINE ) * sizeof( char );
	buffer = Hunk_AllocateTempMemory( bufferlen );

	// write the remaining lines
	buffer[ bufferlen - 1 ] = '\0';

	for ( i = 0; i < n ; i++, l++ ) 
	{
		line = con.text + (l % con.totallines) * con.linewidth;
		// store line
		for( x = 0; x < con.linewidth; x++ )
			buffer[ x ] = line[ x ] & 0xff;
		buffer[ con.linewidth ] = '\0';
		// terminate on ending space characters
		for ( x = con.linewidth - 1 ; x >= 0 ; x-- ) {
			if ( buffer[ x ] == ' ' )
				buffer[ x ] = '\0';
			else
				break;
		}
		Q_strcat( buffer, bufferlen, Q_NEWLINE );
		FS_Write( buffer, strlen( buffer ), f );
	}

	Hunk_FreeTempMemory( buffer );
	FS_FCloseFile( f );
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void ) {
	int		i;
	
	for ( i = 0 ; i < NUM_CON_TIMES ; i++ ) {
		con.times[i] = 0;
	}
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize( void )
{
	int		i, j, width, oldwidth, oldtotallines, oldcurrent, numlines, numchars;
	short	tbuf[CON_TEXTSIZE], *src, *dst;
	static int old_width, old_vispage;
	int		vispage;
	float	scale;

	if ( con.viswidth == cls.glconfig.vidWidth && !con_scale->modified ) {
		return;
	}

	scale = con_scale->value;

	con.viswidth = cls.glconfig.vidWidth;

	smallchar_width = SMALLCHAR_WIDTH * scale * cls.con_factor;
	smallchar_height = SMALLCHAR_HEIGHT * scale * cls.con_factor;
	bigchar_width = BIGCHAR_WIDTH * scale * cls.con_factor;
	bigchar_height = BIGCHAR_HEIGHT * scale * cls.con_factor;

	if ( cls.glconfig.vidWidth == 0 ) // video hasn't been initialized yet
	{
		g_console_field_width = DEFAULT_CONSOLE_WIDTH;
		width = DEFAULT_CONSOLE_WIDTH * scale;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		con.vispage = 4;

		Con_Clear_f();
	}
	else
	{
		width = ((cls.glconfig.vidWidth / smallchar_width) - 2);

		g_console_field_width = width;
		g_consoleField.widthInChars = g_console_field_width;

		if ( width > MAX_CONSOLE_WIDTH )
			width = MAX_CONSOLE_WIDTH;

		vispage = cls.glconfig.vidHeight / ( smallchar_height * 2 ) - 1;

		if ( old_vispage == vispage && old_width == width )
			return;

		oldwidth = con.linewidth;
		oldtotallines = con.totallines;
		oldcurrent = con.current;

		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		con.vispage = vispage;

		old_vispage = vispage;
		old_width = width;

		numchars = oldwidth;
		if ( numchars > con.linewidth )
			numchars = con.linewidth;

		if ( oldcurrent > oldtotallines )
			numlines = oldtotallines;	
		else
			numlines = oldcurrent + 1;	

		if ( numlines > con.totallines )
			numlines = con.totallines;

		Com_Memcpy( tbuf, con.text, CON_TEXTSIZE * sizeof( short ) );

		for ( i = 0; i < CON_TEXTSIZE; i++ ) 
			con.text[i] = (ColorIndex(CONSOLE_COLOR)<<8) | ' ';

		for ( i = 0; i < numlines; i++ )
		{
			src = &tbuf[ ((oldcurrent - i + oldtotallines) % oldtotallines) * oldwidth ];
			dst = &con.text[ (numlines - 1 - i) * con.linewidth ];
			for ( j = 0; j < numchars; j++ )
				*dst++ = *src++;
		}

		Con_ClearNotify();

		con.current = numlines - 1;
	}

	con.display = con.current;

	con_scale->modified = qfalse;
}


/*
==================
Cmd_CompleteTxtName
==================
*/
static void Cmd_CompleteTxtName( const char *args, int argNum ) {
	if ( argNum == 2 ) {
		Field_CompleteFilename( "", "txt", qfalse, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}


static const cmdListItem_t con_cmds[] = {
	{ "clear", Con_Clear_f, NULL },
	{ "clMessageMode", Con_MessageMode_f, NULL },
	{ "clMessageMode2", Con_MessageMode2_f, NULL },
	{ "clMessageMode3", Con_MessageMode3_f, NULL },
	{ "condump", Con_Dump_f, Cmd_CompleteTxtName },
	{ "toggleConsole", Con_ToggleConsole_f, NULL },
	{ "toggleMenu", Con_ToggleMenu_f, NULL },
};


/*
================
Con_Init
================
*/
void Con_Init( void ) 
{
	con_notifytime = Cvar_Get( "con_notifytime", "7", 0 ); // JPW NERVE increased per id req for obits
	con_conspeed = Cvar_Get( "scr_conspeed", "3", 0 );
	Cvar_SetDescription( con_conspeed, "Console open/close speed" );
	con_autoclear = Cvar_Get( "con_autoclear", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( con_autoclear, "Automatically clear console input on close" );
	con_restricted = Cvar_Get( "con_restricted", "0", CVAR_INIT );        // DHM - Nerve

	con_scale = Cvar_Get( "con_scale", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( con_scale, "0.5", "8", CV_FLOAT );
	Cvar_SetDescription( con_scale, "Text scale modifier for all console text" );

	con_height = Cvar_Get( "con_height", "0.5", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( con_height, "0.01", "1", CV_FLOAT );
	Cvar_SetDescription( con_height, "Default height for normal console open" );

	con_drawnotify = Cvar_Get( "con_drawnotify", "0", CVAR_CHEAT );
	Cvar_SetDescription( con_drawnotify, "Draw console print messages at top of screen" );

	con_maxnotify = Cvar_Get( "con_maxnotify", "4", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( con_maxnotify, "0", XSTRING( NUM_CON_TIMES ), CV_INTEGER );
	Cvar_SetDescription( con_maxnotify, "Maximum number console print messages at top of screen" );

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Cmd_RegisterArray( con_cmds, MODULE_CONSOLE );
}


/*
================
Con_Shutdown
================
*/
void Con_Shutdown( void )
{
	Cmd_UnregisterModule( MODULE_CONSOLE );
}


/*
===============
Con_Fixup
===============
*/
static void Con_Fixup( void ) 
{
	int filled;

	if ( con.current >= con.totallines ) {
		filled = con.totallines;
	} else {
		filled = con.current + 1;
	}

	if ( filled <= con.vispage ) {
		con.display = con.current;
	} else if ( con.current - con.display > filled - con.vispage ) {
		con.display = con.current - filled + con.vispage;
	} else if ( con.display > con.current ) {
		con.display = con.current;
	}
}


/*
===============
Con_Linefeed

Move to newline only when we _really_ need this
===============
*/
static void Con_NewLine( void )
{
	short *s;
	int i;

	// follow last line
	if ( con.display == con.current )
		con.display++;
	con.current++;

	s = &con.text[ ( con.current % con.totallines ) * con.linewidth ];
	for ( i = 0; i < con.linewidth ; i++ ) 
		*s++ = (ColorIndex(CONSOLE_COLOR)<<8) | ' ';

	con.x = 0;
}


/*
===============
Con_Linefeed
===============
*/
static void Con_Linefeed( qboolean skipnotify )
{
	// mark time for transparent overlay
	if ( con.current >= 0 )	{
		if ( skipnotify )
			con.times[ con.current % NUM_CON_TIMES ] = 0;
		else
			con.times[ con.current % NUM_CON_TIMES ] = cls.realtime;
	}

	if ( con.newline ) {
		Con_NewLine();
	} else {
		con.newline = qtrue;
		con.x = 0;
	}

	Con_Fixup();
}


/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void CL_ConsolePrint( const char *txt ) {
	int		y;
	int		c, l;
	int		colorIndex;
	qboolean skipnotify = qfalse;		// NERVE - SMF
	int prev;							// NERVE - SMF

	// NERVE - SMF - work around for text that shows up in console but not in notify
	if ( !Q_strncmp( txt, "[skipnotify]", 12 ) ) {
		skipnotify = qtrue;
		txt += 12;
	}

	// for some demos we don't want to ever show anything on the console
	if ( cl_noprint && cl_noprint->integer ) {
		return;
	}
	
	if ( !con.initialized ) {
		static cvar_t null_cvar = { 0 };
		con.color[0] =
		con.color[1] =
		con.color[2] =
		con.color[3] = 1.0f;
		con.viswidth = -9999;
		cls.con_factor = 1.0f;
		con_scale = &null_cvar;
		con_scale->value = 1.0f;
		con_scale->modified = qtrue;
		Con_CheckResize();
		con.initialized = qtrue;
	}

	colorIndex = ColorIndex( COLOR_WHITE );

	while ( (c = *txt) != 0 ) {
		if ( Q_IsColorString( txt ) && *(txt+1) != '\n' ) {
			if ( *(txt + 1) == COLOR_NULL ) {
				colorIndex = ColorIndex( CONSOLE_COLOR );
			} else {
				colorIndex = ColorIndexFromChar( *(txt+1) );
			}
			txt += 2;
			continue;
		}

		// count word length
		for ( l = 0 ; l < con.linewidth ; l++ ) {
			if ( txt[l] <= ' ' ) {
				break;
			}
		}

		// word wrap
		if ( l != con.linewidth && ( con.x + l >= con.linewidth ) ) {
			Con_Linefeed( skipnotify );
		}

		txt++;

		switch( c )
		{
		case '\n':
			Con_Linefeed( skipnotify );
			break;
		case '\r':
			con.x = 0;
			break;
		default:
			if ( con.newline ) {
				Con_NewLine();
				Con_Fixup();
				con.newline = qfalse;
			}
			// display character and advance
			y = con.current % con.totallines;
			// rain - sign extension caused the character to carry over
			// into the color info for high ascii chars; casting c to unsigned
			////con.text[y * con.linewidth + con.x] = ( color << 8 ) | (unsigned char)c;
			con.text[y * con.linewidth + con.x ] = (colorIndex << 8) | (unsigned char)c;
			con.x++;
			if ( con.x >= con.linewidth ) {
				Con_Linefeed( skipnotify );
			}
			break;
		}
	}

	// mark time for transparent overlay
	if ( con.current >= 0 ) {
		if ( skipnotify ) {
			prev = con.current % NUM_CON_TIMES - 1;
			if ( prev < 0 )
				prev = NUM_CON_TIMES - 1;
			con.times[ prev ] = 0;
		} else {
			con.times[ con.current % NUM_CON_TIMES ] = cls.realtime;
		}
	}
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

Draw the editline after a ] prompt
================
*/
static void Con_DrawInput( void ) {
	int		y;

	if ( cls.state != CA_DISCONNECTED && !(Key_GetCatcher( ) & KEYCATCH_CONSOLE ) ) {
		return;
	}

	y = con.vislines - ( smallchar_height * 2 );

	// hightlight the current autocompleted part
	if ( con.acLength ) {
		int cmdLen;
		Cmd_TokenizeString( g_consoleField.buffer );
		cmdLen = (int)strlen( Cmd_Argv( 0 ) );

		if ( cmdLen - con.acLength > 0 ) {
			re.SetColor( console_highlightcolor );
			re.DrawStretchPic( con.xadjust + ( 2 + con.acLength ) * smallchar_width,
							   y + 2,
							   ( cmdLen - con.acLength ) * smallchar_width,
							   smallchar_height - 2, 0, 0, 0, 0, cls.whiteShader );
		}
	}

	re.SetColor( con.color );

	SCR_DrawSmallChar( con.xadjust + 1 * smallchar_width, y, ']' );

	Field_Draw( &g_consoleField, con.xadjust + 2 * smallchar_width, y,
		SCREEN_WIDTH - 3 * smallchar_width, qtrue, qtrue );
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
static void Con_DrawNotify( void )
{
	int		x, v;
	short	*text;
	int		i;
	int		time;
	int		skip;
	int		currentColorIndex;
	int		colorIndex;

	// assumes con_maxnotify is > 0, checked before caller

	currentColorIndex = ColorIndex( COLOR_WHITE );
	re.SetColor( g_color_table[ currentColorIndex ] );

	v = 0;
	for (i= con.current-con_maxnotify->integer+1 ; i<=con.current ; i++)
	{
		if (i < 0)
			continue;
		time = con.times[i % con_maxnotify->integer];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if ( time >= con_notifytime->value*1000 )
			continue;
		text = con.text + (i % con.totallines)*con.linewidth;

		if (cl.snap.ps.pm_type != PM_INTERMISSION && Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
			continue;
		}

		for (x = 0 ; x < con.linewidth ; x++) {
			if ( ( text[x] & 0xff ) == ' ' ) {
				continue;
			}
			colorIndex = ( text[x] >> 8 ) & COLOR_BITS;
			if ( currentColorIndex != colorIndex ) {
				currentColorIndex = colorIndex;
				re.SetColor( g_color_table[ colorIndex ] );
			}
			SCR_DrawSmallChar( cl_conXOffset->integer + con.xadjust + (x+1)*smallchar_width, v, text[x] & 0xff );
		}

		v += smallchar_height;
	}

	re.SetColor( NULL );

	if ( Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
		return;
	}

	// draw the chat line
	if ( Key_GetCatcher( ) & KEYCATCH_MESSAGE )
	{
		// rescale to virtual 640x480 space
		v /= cls.glconfig.vidHeight / 480.0;

		if ( chat_team ) {
			char buf[128];
			CL_TranslateString( "say_team:", buf );
			SCR_DrawBigString( SMALLCHAR_WIDTH, v, buf, 1.0f, qfalse );
			skip = strlen( buf ) + 2;
		} else if ( chat_buddy ) {
			char buf[128];
			CL_TranslateString( "say_fireteam:", buf );
			SCR_DrawBigString( SMALLCHAR_WIDTH, v, buf, 1.0f, qfalse );
			skip = strlen( buf ) + 2;
		} else {
			char buf[128];
			CL_TranslateString( "say:", buf );
			SCR_DrawBigString( SMALLCHAR_WIDTH, v, buf, 1.0f, qfalse );
			skip = strlen( buf ) + 1;
		}

		Field_BigDraw( &chatField, skip * BIGCHAR_WIDTH, v,
			SCREEN_WIDTH - ( skip + 1 ) * BIGCHAR_WIDTH, qtrue, qtrue );
	}
}

#define CONSOLE_BUILD_STRING "Build Date: " __DATE__


/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
static void Con_DrawSolidConsole( float frac ) {

	static float conColorValue[4] = { 0.0, 0.0, 0.0, 0.0 };
	// for cvar value change tracking
	static char  conColorString[ MAX_CVAR_VALUE_STRING ] = { '\0' };

	int				i, x, y;
	int				rows;
	short			*text;
	int				row;
	int				lines;
	int				currentColorIndex;
	int				colorIndex;
	vec4_t			color;
	float			yf, wf;
	char			buf[ MAX_CVAR_VALUE_STRING ], *v[4];

	lines = cls.glconfig.vidHeight * frac;
	if ( lines <= 0 )
		return;

	if ( re.FinishBloom )
		re.FinishBloom();

	if ( lines > cls.glconfig.vidHeight )
		lines = cls.glconfig.vidHeight;

	wf = SCREEN_WIDTH;

	// draw the background
	yf = frac * SCREEN_HEIGHT;

	// on wide screens, we will center the text
	con.xadjust = 0;
	SCR_AdjustFrom640( &con.xadjust, &yf, &wf, NULL );

	if ( yf < 1.0 ) {
		yf = 0;
	} else {
		// custom console background color
		if ( cl_conColor->string[0] ) {
			// track changes
			if ( strcmp( cl_conColor->string, conColorString ) ) 
			{
				Q_strncpyz( conColorString, cl_conColor->string, sizeof( conColorString ) );
				Q_strncpyz( buf, cl_conColor->string, sizeof( buf ) );
				Com_Split( buf, v, 4, ' ' );
				for ( i = 0; i < 4 ; i++ ) {
					conColorValue[ i ] = Q_atof( v[ i ] ) / 255.0f;
					if ( conColorValue[ i ] > 1.0f ) {
						conColorValue[ i ] = 1.0f;
					} else if ( conColorValue[ i ] < 0.0f ) {
						conColorValue[ i ] = 0.0f;
					}
				}
			}
			re.SetColor( conColorValue );
			re.DrawStretchPic( 0, 0, wf, yf, 0, 0, 1, 1, cls.whiteShader );
		} else {
			re.SetColor( g_color_table[ ColorIndex( COLOR_WHITE ) ] );
			re.DrawStretchPic( 0, 0, wf, yf, 0, 0, 1, 1, cls.consoleShader );
			
			// NERVE - SMF - merged from WolfSP
			if ( frac >= 0.5f ) {
				color[0] = color[1] = color[2] = frac * 2.0f;
				color[3] = 1.0f;
				re.SetColor( color );

				// draw the logo
				SCR_DrawPic( 192, 70, 256, 128, cls.consoleShader2 );
				re.SetColor( NULL );
			}
			// -NERVE - SMF
		}

	}

	// ydnar: matching light text
	color[0] = 0.75;
	color[1] = 0.75;
	color[2] = 0.75;
	color[3] = 1.0f;
	if ( frac < 1.0 ) {
		re.SetColor( color );
		re.DrawStretchPic( 0, yf, wf, 2, 0, 0, 1, 1, cls.whiteShader );
		//SCR_FillRect( 0, yf, SCREEN_WIDTH, 1.25, color );
	}


	// draw the version number
	re.SetColor( g_color_table[ ColorIndex( CONSOLE_COLOR ) ] );

	SCR_DrawSmallString( cls.glconfig.vidWidth - ( ARRAY_LEN( ETe_VERSION ) ) * smallchar_width,
		lines - smallchar_height, ETe_VERSION, ARRAY_LEN( ETe_VERSION ) - 1 );

	SCR_DrawSmallString( cls.glconfig.vidWidth - ( ARRAY_LEN( CONSOLE_BUILD_STRING ) ) * smallchar_width,
		lines - (smallchar_height*2), CONSOLE_BUILD_STRING, ARRAY_LEN( CONSOLE_BUILD_STRING ) - 1 );

	SCR_DrawSmallString( cls.glconfig.vidWidth - ( ARRAY_LEN( PLATFORM_STRING ) ) * smallchar_width,
		lines - ( smallchar_height * 3 ), PLATFORM_STRING, ARRAY_LEN( PLATFORM_STRING ) - 1 );

	// draw the text
	con.vislines = lines;

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput();

	//rows = ( lines - SMALLCHAR_WIDTH ) / SMALLCHAR_WIDTH;     // rows of text to draw
	rows = lines / smallchar_width - 1;	// rows of text to draw

	y = lines - (smallchar_height * 3);

	row = con.display;

	// draw from the bottom up
	if ( con.display != con.current )
	{
		// draw arrows to show the buffer is backscrolled
		re.SetColor( g_color_table[ ColorIndex( CONSOLE_COLOR ) ] );
		for ( x = 0 ; x < con.linewidth ; x += 4 )
			SCR_DrawSmallChar( con.xadjust + (x+1)*smallchar_width, y, '^' );
		y -= smallchar_height;
		row--;
	}

#ifdef USE_CURL
	if ( download.progress[ 0 ] ) 
	{
		currentColorIndex = ColorIndex( COLOR_CYAN );
		re.SetColor( g_color_table[ currentColorIndex ] );

		i = strlen( download.progress );
		for ( x = 0 ; x < i ; x++ ) 
		{
			SCR_DrawSmallChar( ( x + 1 ) * smallchar_width,
				lines - smallchar_height, download.progress[x] );
		}
	}
#endif

	currentColorIndex = ColorIndex( COLOR_WHITE );
	re.SetColor( g_color_table[ currentColorIndex ] );

	for ( i = 0 ; i < rows ; i++, y -= smallchar_height, row-- )
	{
		if ( row < 0 )
			break;

		if ( con.current - row >= con.totallines ) {
			// past scrollback wrap point
			continue;
		}

		text = con.text + (row % con.totallines) * con.linewidth;

		for ( x = 0 ; x < con.linewidth ; x++ ) {
			// skip rendering whitespace
			if ( ( text[x] & 0xff ) == ' ' ) {
				continue;
			}
			// track color changes
			colorIndex = ( text[ x ] >> 8 ) & COLOR_BITS;
			if ( currentColorIndex != colorIndex ) {
				currentColorIndex = colorIndex;
				re.SetColor( g_color_table[ colorIndex ] );
			}
			SCR_DrawSmallChar( con.xadjust + (x + 1) * smallchar_width, y, text[x] & 0xff );
		}
	}

	re.SetColor( NULL );
}

/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole( void ) {

	// check for console width changes from a vid mode change
	Con_CheckResize();

	// if disconnected, render console full screen
	if ( cls.state == CA_DISCONNECTED ) {
		if ( !( Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME)) ) {
			Con_DrawSolidConsole( 1.0 );
			return;
		}
	}

	if ( con.displayFrac ) {
		Con_DrawSolidConsole( con.displayFrac );
	} else {
		// draw notify lines
		if ( cls.state == CA_ACTIVE && con_drawnotify->integer && con_maxnotify->integer > 0 ) {
			Con_DrawNotify();
		}
	}
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole( void ) 
{
	// decide on the destination height of the console
	// ydnar: added short console support (via shift+~)
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
		con.finalFrac = con.desiredFrac;	// half-screen or custom
	else
		con.finalFrac = 0.0;	// none visible
	
	// scroll towards the destination height
	if ( con.finalFrac < con.displayFrac )
	{
		con.displayFrac -= con_conspeed->value * cls.realFrametime * 0.001;
		if ( con.finalFrac > con.displayFrac )
			con.displayFrac = con.finalFrac;

	}
	else if ( con.finalFrac > con.displayFrac )
	{
		con.displayFrac += con_conspeed->value * cls.realFrametime * 0.001;
		if ( con.finalFrac < con.displayFrac )
			con.displayFrac = con.finalFrac;
	}
}


void Con_PageUp( int lines )
{
	if ( lines == 0 )
		lines = con.vispage - 2;

	con.display -= lines;
	
	Con_Fixup();
}


void Con_PageDown( int lines )
{
	if ( lines == 0 )
		lines = con.vispage - 2;

	con.display += lines;

	Con_Fixup();
}


void Con_Top( void )
{
	// this is generally incorrect but will be adjusted in Con_Fixup()
	con.display = con.current - con.totallines;

	Con_Fixup();
}


void Con_Bottom( void )
{
	con.display = con.current;

	Con_Fixup();
}


void Con_Close( void )
{
	if ( !com_cl_running->integer )
		return;

	Field_Clear( &g_consoleField );
	Con_ClearNotify();
	Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CONSOLE );
	con.finalFrac = 0.0;			// none visible
	con.displayFrac = 0.0;
}
