/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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

#include "sdl_local.h"

#include "../client/client.h"
#include "sdl_glw.h"

static cvar_t *in_keyboardDebug;
static cvar_t *in_forceCharset;

#ifdef USE_JOYSTICK
static SDL_Gamepad *gamepad;
static SDL_Joystick *stick = NULL;
#endif

static qboolean mouseAvailable = qfalse;
static qboolean mouseActive = qfalse;

static cvar_t *in_mouse;

#ifdef USE_JOYSTICK
static cvar_t *in_joystick;
static cvar_t *in_joystickThreshold;
static cvar_t *in_joystickNo;
static cvar_t *in_joystickUseAnalog;

cvar_t *j_pitch;
cvar_t *j_yaw;
cvar_t *j_forward;
cvar_t *j_side;
cvar_t *j_up;
cvar_t *j_pitch_axis;
cvar_t *j_yaw_axis;
cvar_t *j_forward_axis;
cvar_t *j_side_axis;
cvar_t *j_up_axis;
#endif

#define Com_QueueEvent Sys_QueEvent

static cvar_t *cl_consoleKeys;

static int in_eventTime = 0;
static qboolean mouse_focus;

#define CTRL(a) ((a)-'a'+1)

qboolean Sys_IsKeyModActive( int modnum ) {
	if ( !SDL_WasInit( SDL_INIT_VIDEO ) ) {
		return qfalse;
	}
	switch( modnum ) {
	case K_KP_NUMLOCK: return ( SDL_GetModState() & SDL_KMOD_NUM ) ? qtrue : qfalse;
	case K_CAPSLOCK: return ( SDL_GetModState() & SDL_KMOD_CAPS ) ? qtrue : qfalse;
	case K_SCROLLOCK: return ( SDL_GetModState() & SDL_KMOD_SCROLL ) ? qtrue : qfalse;
	case K_SHIFT: return ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? qtrue : qfalse;
	case K_ALT: return ( SDL_GetModState() & SDL_KMOD_ALT ) ? qtrue : qfalse;
	case K_CTRL: return ( SDL_GetModState() & SDL_KMOD_CTRL ) ? qtrue : qfalse;
	default: return qfalse;
	}
}

qboolean Sys_IsNumLockDown( void ) {
	if ( !SDL_WasInit( SDL_INIT_VIDEO ) ) {
		return qfalse;
	}
	return ( SDL_GetModState() & SDL_KMOD_NUM ) ? qtrue : qfalse;
}

static void IN_ShowCursor( qboolean show )
{
	if ( show ) {
		SDL_ShowCursor();
	} else {
		SDL_HideCursor();
	}
}

/*
===============
IN_PrintKey
===============
*/
static void IN_PrintKey( const SDL_KeyboardEvent *event, keyNum_t key, qboolean down )
{
	if( down )
		Com_Printf( "+ " );
	else
		Com_Printf( "  " );

	Com_Printf( "Scancode: 0x%02x(%s) Sym: 0x%02x(%s)",
			event->scancode, SDL_GetScancodeName( event->scancode ),
			event->key, SDL_GetKeyName( event->key ) );

	if( event->mod & SDL_KMOD_LSHIFT )   Com_Printf( " KMOD_LSHIFT" );
	if( event->mod & SDL_KMOD_RSHIFT )   Com_Printf( " KMOD_RSHIFT" );
	if( event->mod & SDL_KMOD_LCTRL )    Com_Printf( " KMOD_LCTRL" );
	if( event->mod & SDL_KMOD_RCTRL )    Com_Printf( " KMOD_RCTRL" );
	if( event->mod & SDL_KMOD_LALT )     Com_Printf( " KMOD_LALT" );
	if( event->mod & SDL_KMOD_RALT )     Com_Printf( " KMOD_RALT" );
	if( event->mod & SDL_KMOD_LGUI )     Com_Printf( " KMOD_LGUI" );
	if( event->mod & SDL_KMOD_RGUI )     Com_Printf( " KMOD_RGUI" );
	if( event->mod & SDL_KMOD_NUM )      Com_Printf( " KMOD_NUM" );
	if( event->mod & SDL_KMOD_CAPS )     Com_Printf( " KMOD_CAPS" );
	if( event->mod & SDL_KMOD_MODE )     Com_Printf( " KMOD_MODE" );
	if( event->mod & SDL_KMOD_SCROLL )   Com_Printf( " KMOD_SCROLL" );

	Com_Printf( " Q:0x%02x(%s)\n", key, Key_KeynumToString( key, qfalse ) );
}


#define MAX_CONSOLE_KEYS 16

typedef struct consoleKey_s
{
	enum
	{
		QUAKE_KEY,
		CHARACTER
	} type;

	union
	{
		keyNum_t key;
		int character;
	} u;
} consoleKey_t;

static consoleKey_t consoleKeys[ MAX_CONSOLE_KEYS ];
static int numConsoleKeys = 0;

/*
Only called on init and on cl_consoleKeys cvar modified
*/
static void IN_ParseConsoleKeys( void ) {
	const char *text_p, *token;

	cl_consoleKeys->modified = qfalse;
	text_p = cl_consoleKeys->string;
	numConsoleKeys = 0;

	while( numConsoleKeys < MAX_CONSOLE_KEYS )
	{
		consoleKey_t *c = &consoleKeys[ numConsoleKeys ];
		int charCode = 0;

		token = COM_Parse( &text_p );
		if( !token[ 0 ] )
			break;

		charCode = Com_HexStrToInt( token );

		if( charCode > 0 )
		{
			c->type = CHARACTER;
			c->u.character = charCode;
		}
		else
		{
			c->type = QUAKE_KEY;
			c->u.key = Key_StringToKeynum( token );

			// 0 isn't a key
			if ( c->u.key <= 0 )
				continue;
		}

		numConsoleKeys++;
	}
}

/*
===============
IN_IsConsoleKey

TODO: If the SDL_Scancode situation improves, use it instead of
      both of these methods
===============
*/
static qboolean IN_IsConsoleKey( keyNum_t key, int character )
{
	int i;

	/*if (key == K_GRAVE
#ifdef __APPLE__
	    || key == 60 // Same as console key
#endif
	    )
	{
		return qtrue;
	}*/

	// If the character is the same as the key, prefer the character
	if ( key == character )
		key = 0;

	for ( i = 0; i < numConsoleKeys; i++ )
	{
		consoleKey_t *c = &consoleKeys[ i ];

		switch ( c->type )
		{
			case QUAKE_KEY:
				if( key && c->u.key == key )
					return qtrue;
				break;

			case CHARACTER:
				if( c->u.character == character )
					return qtrue;
				break;
		}
	}

	return qfalse;
}


/*
===============
IN_TranslateSDLToETKey
===============
*/
static int IN_TranslateSDLToETKey( const SDL_KeyboardEvent *event, qboolean down )
{
	int key = 0;

	if ( event->scancode >= SDL_SCANCODE_1 && event->scancode <= SDL_SCANCODE_0 )
	{
		// Always map the number keys as such even if they actually map
		// to other characters (eg, "1" is "&" on an AZERTY keyboard).
		// This is required for SDL before 2.0.6, except on Windows
		// which already had this behavior.
		key = '0'; // implicitly handles SDL_SCANCODE_0

		if ( event->scancode <= SDL_SCANCODE_9 )
			key = '1' + (event->scancode - SDL_SCANCODE_1);
	}
	else if ( in_forceCharset->integer > 0 )
	{
		if ( event->scancode >= SDL_SCANCODE_A && event->scancode <= SDL_SCANCODE_Z )
		{
			key = 'a' + (event->scancode - SDL_SCANCODE_A);
		}
		else
		{
			switch ( (int)event->scancode )
			{
				case SDL_SCANCODE_MINUS:        key = '-';  break;
				case SDL_SCANCODE_EQUALS:       key = '=';  break;
				case SDL_SCANCODE_LEFTBRACKET:  key = '[';  break;
				case SDL_SCANCODE_RIGHTBRACKET: key = ']';  break;
				case SDL_SCANCODE_NONUSBACKSLASH:
				case SDL_SCANCODE_BACKSLASH:    key = '\\'; break;
				case SDL_SCANCODE_SEMICOLON:    key = ';';  break;
				case SDL_SCANCODE_APOSTROPHE:   key = '\''; break;
				case SDL_SCANCODE_COMMA:        key = ',';  break;
				case SDL_SCANCODE_PERIOD:       key = '.';  break;
				case SDL_SCANCODE_SLASH:        key = '/';  break;
				default:
					/* key = 0 */
					break;
			}
		}
	}

	if( !key && event->key >= SDLK_SPACE && event->key < SDLK_DELETE )
	{
		// These happen to match the ASCII chars
		key = (int)event->key;
	}
	else if( !key )
	{
		switch( (int)event->key )
		{
			case SDLK_PAGEUP:       key = K_PGUP;          break;
			case SDLK_KP_9:         key = K_KP_PGUP;       break;
			case SDLK_PAGEDOWN:     key = K_PGDN;          break;
			case SDLK_KP_3:         key = K_KP_PGDN;       break;
			case SDLK_KP_7:         key = K_KP_HOME;       break;
			case SDLK_HOME:         key = K_HOME;          break;
			case SDLK_KP_1:         key = K_KP_END;        break;
			case SDLK_END:          key = K_END;           break;
			case SDLK_KP_4:         key = K_KP_LEFTARROW;  break;
			case SDLK_LEFT:         key = K_LEFTARROW;     break;
			case SDLK_KP_6:         key = K_KP_RIGHTARROW; break;
			case SDLK_RIGHT:        key = K_RIGHTARROW;    break;
			case SDLK_KP_2:         key = K_KP_DOWNARROW;  break;
			case SDLK_DOWN:         key = K_DOWNARROW;     break;
			case SDLK_KP_8:         key = K_KP_UPARROW;    break;
			case SDLK_UP:           key = K_UPARROW;       break;
			case SDLK_ESCAPE:       key = K_ESCAPE;        break;
			case SDLK_KP_ENTER:     key = K_KP_ENTER;      break;
			case SDLK_RETURN:       key = K_ENTER;         break;
			case SDLK_TAB:          key = K_TAB;           break;
			case SDLK_F1:           key = K_F1;            break;
			case SDLK_F2:           key = K_F2;            break;
			case SDLK_F3:           key = K_F3;            break;
			case SDLK_F4:           key = K_F4;            break;
			case SDLK_F5:           key = K_F5;            break;
			case SDLK_F6:           key = K_F6;            break;
			case SDLK_F7:           key = K_F7;            break;
			case SDLK_F8:           key = K_F8;            break;
			case SDLK_F9:           key = K_F9;            break;
			case SDLK_F10:          key = K_F10;           break;
			case SDLK_F11:          key = K_F11;           break;
			case SDLK_F12:          key = K_F12;           break;
			case SDLK_F13:          key = K_F13;           break;
			case SDLK_F14:          key = K_F14;           break;
			case SDLK_F15:          key = K_F15;           break;

			case SDLK_BACKSPACE:    key = K_BACKSPACE;     break;
			case SDLK_KP_PERIOD:    key = K_KP_DEL;        break;
			case SDLK_DELETE:       key = K_DEL;           break;
			case SDLK_PAUSE:        key = K_PAUSE;         break;

			case SDLK_LSHIFT:
			case SDLK_RSHIFT:       key = K_SHIFT;         break;

			case SDLK_LCTRL:
			case SDLK_RCTRL:        key = K_CTRL;          break;

#ifdef SDL_PLATFORM_MACOS
			case SDLK_RGUI:
			case SDLK_LGUI:         key = K_COMMAND;       break;
#else
			case SDLK_RGUI:
			case SDLK_LGUI:         key = K_SUPER;         break;
#endif

			case SDLK_RALT:
			case SDLK_LALT:         key = K_ALT;           break;

			case SDLK_KP_5:         key = K_KP_5;          break;
			case SDLK_INSERT:       key = K_INS;           break;
			case SDLK_KP_0:         key = K_KP_INS;        break;
			case SDLK_KP_MULTIPLY:  key = '*'; /*K_KP_STAR;*/ break;
			case SDLK_KP_PLUS:      key = K_KP_PLUS;       break;
			case SDLK_KP_MINUS:     key = K_KP_MINUS;      break;
			case SDLK_KP_DIVIDE:    key = K_KP_SLASH;      break;

			case SDLK_KP_EQUALS:    key = K_KP_EQUALS;     break;

			case SDLK_MODE:         key = K_MODE;          break;
			case SDLK_HELP:         key = K_HELP;          break;
			case SDLK_PRINTSCREEN:  key = K_PRINT;         break;
			case SDLK_SYSREQ:       key = K_SYSREQ;        break;
			case SDLK_MENU:         key = K_MENU;          break;
			case SDLK_APPLICATION:	key = K_MENU;          break;
			case SDLK_POWER:        key = K_POWER;         break;
			case SDLK_UNDO:         key = K_UNDO;          break;
			case SDLK_SCROLLLOCK:   key = K_SCROLLOCK;     break;
			case SDLK_NUMLOCKCLEAR: key = K_KP_NUMLOCK;    break;
			case SDLK_CAPSLOCK:     key = K_CAPSLOCK;      break;

			default:
#if 1
				key = 0;
#else
				if( !( keysym->sym & SDLK_SCANCODE_MASK ) && keysym->scancode <= 95 )
				{
					// Map Unicode characters to 95 world keys using the key's scan code.
					// FIXME: There aren't enough world keys to cover all the scancodes.
					// Maybe create a map of scancode to quake key at start up and on
					// key map change; allocate world key numbers as needed similar
					// to SDL 1.2.
					key = K_WORLD_0 + (int)keysym->scancode;
				}
#endif
				break;
		}
	}

	if( in_keyboardDebug->integer )
		IN_PrintKey( event, key, down );

	// Key under escape, left of 1, above Tab
	// Except if we are on Brazilian keyboard layout because that matches quotes
	// Which would make typing in console hard if you wanted to insert quotes
	// Also layouts which have the ^ character require the shift key to be held

	if ( event->scancode == SDL_SCANCODE_GRAVE && ((event->key != '\'' && event->key != '"' && event->key != '^') || (event->key == '^' && (event->mod & SDL_KMOD_SHIFT))) )
	{
		// Console keys can't be bound or generate characters

		// TODO if((event.key.mod & (SDL_KMOD_CAPS|SDL_KMOD_SHIFT|SDL_KMOD_ALT|SDL_KMOD_CTRL|SDL_KMOD_GUI)) == 0)

		key = K_CONSOLE;
	}
	else if( IN_IsConsoleKey( key, 0 ) )
	{
		// Console keys can't be bound or generate characters
		key = K_CONSOLE;
	}

	return key;
}


/*
===============
IN_GobbleMotionEvents
===============
*/
static void IN_GobbleMouseEvents( void )
{
	SDL_Event dummy[ 1 ];
	int val = 0;

	// Gobble any mouse events
	SDL_PumpEvents();

	while( ( val = SDL_PeepEvents( dummy, ARRAY_LEN( dummy ), SDL_GETEVENT,
		SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_WHEEL ) ) > 0 ) { }

	if ( val < 0 )
		Com_Printf( "%s failed: %s\n", __func__, SDL_GetError() );
}


//#define DEBUG_EVENTS

/*
===============
IN_ActivateMouse
===============
*/
static void IN_ActivateMouse( void )
{
	if ( !mouseAvailable )
		return;

	if ( !mouseActive )
	{
		IN_GobbleMouseEvents();

		SDL_SetWindowRelativeMouseMode( SDL_window, in_mouse->integer == 1 ? true : false );
		SDL_SetWindowMouseGrab( SDL_window, true );

		if ( glw_state.isFullscreen )
			IN_ShowCursor( qfalse );

		SDL_WarpMouseInWindow( SDL_window, glw_state.window_width / 2, glw_state.window_height / 2 );

#ifdef DEBUG_EVENTS
		Com_Printf( "%4i %s\n", Sys_Milliseconds(), __func__ );
#endif
	}

	// in_nograb makes no sense in fullscreen mode
	if ( !glw_state.isFullscreen )
	{
		if ( in_nograb->modified || !mouseActive )
		{
			if ( in_nograb->integer ) {
				SDL_SetWindowRelativeMouseMode( SDL_window, false );
				SDL_SetWindowMouseGrab( SDL_window, false );
			} else {
				SDL_SetWindowRelativeMouseMode( SDL_window, in_mouse->integer == 1 ? true : false );
				SDL_SetWindowMouseGrab( SDL_window, true );
			}

			in_nograb->modified = qfalse;
		}
	}

	mouseActive = qtrue;
}


/*
===============
IN_DeactivateMouse
===============
*/
static void IN_DeactivateMouse( void )
{
	const char* drv = SDL_GetCurrentVideoDriver();

	if ( !mouseAvailable )
		return;

	if ( mouseActive )
	{
#ifdef DEBUG_EVENTS
		Com_Printf( "%4i %s\n", Sys_Milliseconds(), __func__ );
#endif
		IN_GobbleMouseEvents();

		SDL_SetWindowMouseGrab( SDL_window, false );
		SDL_SetWindowRelativeMouseMode( SDL_window, false );

		if ( gw_active )
			SDL_WarpMouseInWindow( SDL_window, glw_state.window_width / 2, glw_state.window_height / 2 );
		else
		{
			if ( glw_state.isFullscreen )
				IN_ShowCursor( qtrue );

			if ( drv && strcmp( drv, "x11" ) == 0 ) {
				SDL_WarpMouseGlobal( glw_state.desktop_width / 2, glw_state.desktop_height / 2 );
			}
		}

		mouseActive = qfalse;
	}

	// Always show the cursor when the mouse is disabled,
	// but not when fullscreen
	if ( !glw_state.isFullscreen )
		IN_ShowCursor( qtrue );
}


#ifdef USE_JOYSTICK
// We translate axes movement into keypresses
static const int joy_keys[16] = {
	K_LEFTARROW, K_RIGHTARROW,
	K_UPARROW, K_DOWNARROW,
	K_JOY17, K_JOY18,
	K_JOY19, K_JOY20,
	K_JOY21, K_JOY22,
	K_JOY23, K_JOY24,
	K_JOY25, K_JOY26,
	K_JOY27, K_JOY28
};

// translate hat events into keypresses
// the 4 highest buttons are used for the first hat ...
static const int hat_keys[16] = {
	K_JOY29, K_JOY30,
	K_JOY31, K_JOY32,
	K_JOY25, K_JOY26,
	K_JOY27, K_JOY28,
	K_JOY21, K_JOY22,
	K_JOY23, K_JOY24,
	K_JOY17, K_JOY18,
	K_JOY19, K_JOY20
};


struct
{
	qboolean buttons[SDL_GAMEPAD_BUTTON_COUNT + 1]; // +1 because old max was 16, current SDL_GAMEPAD_BUTTON_COUNT is 26
	unsigned int oldaxes;
	int oldaaxes[MAX_JOYSTICK_AXIS];
	unsigned int oldhats;
} stick_state;


/*
===============
IN_InitJoystick
===============
*/
static void IN_InitJoystick( void )
{
	int i = 0;
	int numJoysticks = 0;
	SDL_JoystickID *sticks = NULL, joyid = 0;
	char buf[16384] = "";
	cvar_t *cv;

	if (gamepad)
		SDL_CloseGamepad(gamepad);

	if (stick != NULL)
		SDL_CloseJoystick(stick);

	stick = NULL;
	gamepad = NULL;
	memset(&stick_state, '\0', sizeof(stick_state));

	// SDL 2.0.4 requires SDL_INIT_JOYSTICK to be initialized separately from
	// SDL_INIT_GAMEPAD for SDL_JoystickOpen() to work correctly,
	// despite https://wiki.libsdl.org/SDL_Init (retrieved 2016-08-16)
	// indicating SDL_INIT_JOYSTICK should be initialized automatically.
	if (!SDL_WasInit(SDL_INIT_JOYSTICK))
	{
		Com_DPrintf("Calling SDL_Init(SDL_INIT_JOYSTICK)...\n");
		if (!SDL_Init(SDL_INIT_JOYSTICK))
		{
			Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) failed: %s\n", SDL_GetError());
			return;
		}
		Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) passed.\n");
	}

	if (!SDL_WasInit(SDL_INIT_GAMEPAD))
	{
		Com_DPrintf("Calling SDL_Init(SDL_INIT_GAMEPAD)...\n");
		if (!SDL_Init(SDL_INIT_GAMEPAD))
		{
			Com_DPrintf("SDL_Init(SDL_INIT_GAMEPAD) failed: %s\n", SDL_GetError());
			return;
		}
		Com_DPrintf("SDL_Init(SDL_INIT_GAMEPAD) passed.\n");
	}

	sticks = SDL_GetJoysticks(&numJoysticks);
	if( sticks != NULL ) {
		Com_DPrintf("%d possible joysticks\n", numJoysticks);
	}

	// Print list and build cvar to allow ui to select joystick.
	for (i = 0; i < numJoysticks; i++)
	{
		const char *name = SDL_GetJoystickNameForID(sticks[i]);
		Q_strcat(buf, sizeof(buf), name ? name : "[unknown joystick]");
		Q_strcat(buf, sizeof(buf), "\n");
	}

	cv = Cvar_Get( "in_availableJoysticks", "", CVAR_ROM );
	Cvar_SetDescription( cv, "List of available Joysticks" );

	// Update cvar on in_restart or controller add/remove.
	Cvar_Set( "in_availableJoysticks", buf );

	if( !in_joystick->integer ) {
		Com_DPrintf( "Joystick is not active.\n" );
		SDL_free(sticks);
		SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
		return;
	}

	in_joystickNo = Cvar_Get( "in_joystickNo", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( in_joystickNo, "Select which joystick to use" );
	if( in_joystickNo->integer < 0 || in_joystickNo->integer >= numJoysticks )
		Cvar_Set( "in_joystickNo", "0" );

	in_joystickUseAnalog = Cvar_Get( "in_joystickUseAnalog", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( in_joystickUseAnalog, "Do not translate joystick axis events to keyboard commands" );
	in_joystickThreshold = Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE );

	joyid = sticks[in_joystickNo->integer];
	SDL_free(sticks);

	stick = SDL_OpenJoystick( joyid );

	if (stick == NULL) {
		Com_DPrintf( "No joystick opened: %s\n", SDL_GetError() );
		return;
	}

	if (SDL_IsGamepad(joyid))
		gamepad = SDL_OpenGamepad(joyid);

	Com_DPrintf( "Joystick %d (ID:%d) opened\n", in_joystickNo->integer, joyid );
	Com_DPrintf( "Name:         %s\n", SDL_GetJoystickName(stick) );
	Com_DPrintf( "Axes:         %d\n", SDL_GetNumJoystickAxes(stick) );
	Com_DPrintf( "Hats:         %d\n", SDL_GetNumJoystickHats(stick) );
	Com_DPrintf( "Buttons:      %d\n", SDL_GetNumJoystickButtons(stick) );
	Com_DPrintf( "Balls:        %d\n", SDL_GetNumJoystickBalls(stick) );
	Com_DPrintf( "Use Analog:   %s\n", in_joystickUseAnalog->integer ? "Yes" : "No" );
	Com_DPrintf( "Threshold:    %f\n", in_joystickThreshold->value );
	Com_DPrintf( "Is gamepad:   %s\n", gamepad ? "Yes" : "No" );
	if( gamepad ) {
		Com_DPrintf( "Gamepad Type: %s\n", SDL_GetGamepadStringForType(SDL_GetGamepadType(gamepad)) );
	}

	SDL_SetJoystickEventsEnabled(true); //SDL_JoystickEventState(SDL_QUERY);
	SDL_SetGamepadEventsEnabled(true); //SDL_GamepadEventState(SDL_QUERY);
}


/*
===============
IN_ShutdownJoystick
===============
*/
static void IN_ShutdownJoystick( void )
{
	if ( !SDL_WasInit( SDL_INIT_GAMEPAD ) )
		return;

	if ( !SDL_WasInit( SDL_INIT_JOYSTICK ) )
		return;

	if (gamepad)
	{
		SDL_CloseGamepad(gamepad);
		gamepad = NULL;
	}

	if (stick)
	{
		SDL_CloseJoystick(stick);
		stick = NULL;
	}

	SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}


static qboolean KeyToAxisAndSign(int keynum, int *outAxis, int *outSign)
{
	const char *bind;

	if (!keynum)
		return qfalse;

	bind = Key_GetBinding(keynum);

	if (!bind || *bind != '+')
		return qfalse;

	*outSign = 0;

	if (Q_stricmp(bind, "+forward") == 0)
	{
		*outAxis = j_forward_axis->integer;
		*outSign = j_forward->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+back") == 0)
	{
		*outAxis = j_forward_axis->integer;
		*outSign = j_forward->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveleft") == 0)
	{
		*outAxis = j_side_axis->integer;
		*outSign = j_side->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveright") == 0)
	{
		*outAxis = j_side_axis->integer;
		*outSign = j_side->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+lookup") == 0)
	{
		*outAxis = j_pitch_axis->integer;
		*outSign = j_pitch->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+lookdown") == 0)
	{
		*outAxis = j_pitch_axis->integer;
		*outSign = j_pitch->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+left") == 0)
	{
		*outAxis = j_yaw_axis->integer;
		*outSign = j_yaw->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+right") == 0)
	{
		*outAxis = j_yaw_axis->integer;
		*outSign = j_yaw->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveup") == 0)
	{
		*outAxis = j_up_axis->integer;
		*outSign = j_up->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+movedown") == 0)
	{
		*outAxis = j_up_axis->integer;
		*outSign = j_up->value > 0.0f ? -1 : 1;
	}

	return *outSign != 0;
}


/*
===============
IN_GamepadMove
===============
*/
static void IN_GamepadMove( void )
{
	int i;
	int translatedAxes[MAX_JOYSTICK_AXIS];
	qboolean translatedAxesSet[MAX_JOYSTICK_AXIS];
	qboolean uiConvert = qfalse;

	SDL_UpdateGamepads();
	
	if ( Key_GetCatcher() & KEYCATCH_UI ) {
		if ( uivm && !VM_Call( uivm, UI_WANTSBINDKEYS ) ) {
			uiConvert = qtrue;
		}
	}

	// check buttons
	for (i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
	{
		qboolean pressed = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH + i);
		if ( i == SDL_GAMEPAD_BUTTON_GUIDE )
			continue;
		if (pressed != stick_state.buttons[i])
		{
			if ( uiConvert ) {
				switch( i ) {
					case SDL_GAMEPAD_BUTTON_SOUTH:
						Com_QueueEvent(in_eventTime, SE_KEY, K_MOUSE1, pressed, 0, NULL);
						break;
					case SDL_GAMEPAD_BUTTON_EAST:
					case SDL_GAMEPAD_BUTTON_BACK:
						Com_QueueEvent(in_eventTime, SE_KEY, K_ESCAPE, pressed, 0, NULL);
						break;
					case SDL_GAMEPAD_BUTTON_DPAD_UP:
						Com_QueueEvent(in_eventTime, SE_KEY, K_UPARROW, pressed, 0, NULL);
						break;
					case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
						Com_QueueEvent(in_eventTime, SE_KEY, K_DOWNARROW, pressed, 0, NULL);
						break;
					case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
						Com_QueueEvent(in_eventTime, SE_KEY, K_LEFTARROW, pressed, 0, NULL);
						break;
					case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
						Com_QueueEvent(in_eventTime, SE_KEY, K_RIGHTARROW, pressed, 0, NULL);
						break;
					default:
						if ( i >= SDL_GAMEPAD_BUTTON_MISC1 ) {
							Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_MISC1 + i - SDL_GAMEPAD_BUTTON_MISC1, pressed, 0, NULL);
						}
						else {
							Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_A + i, pressed, 0, NULL);
						}
						break;
				}
			}
			else {
				if ( i >= SDL_GAMEPAD_BUTTON_MISC1 ) {
					Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_MISC1 + i - SDL_GAMEPAD_BUTTON_MISC1, pressed, 0, NULL);
			}
			else {
					Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_A + i, pressed, 0, NULL);
				}
			}
			stick_state.buttons[i] = pressed;
		}
	}

	// must defer translated axes until all real axes are processed
	// must be done this way to prevent a later mapped axis from zeroing out a previous one
	if (in_joystickUseAnalog->integer)
	{
		for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
		{
			translatedAxes[i] = 0;
			translatedAxesSet[i] = qfalse;
		}
	}

	// check axes
	for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++)
	{
		int axis = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX + i);
		int oldAxis = stick_state.oldaaxes[i];

		// Smoothly ramp from dead zone to maximum value
		float f = ((float)abs(axis) / 32767.0f - in_joystickThreshold->value) / (1.0f - in_joystickThreshold->value);

		if (f < 0.0f)
			f = 0.0f;

		axis = (int)(32767 * ((axis < 0) ? -f : f));

		if (axis != oldAxis)
		{
			const int negMap[SDL_GAMEPAD_AXIS_COUNT] = { K_PAD0_LEFTSTICK_LEFT,  K_PAD0_LEFTSTICK_UP,   K_PAD0_RIGHTSTICK_LEFT,  K_PAD0_RIGHTSTICK_UP, 0, 0 };
			const int posMap[SDL_GAMEPAD_AXIS_COUNT] = { K_PAD0_LEFTSTICK_RIGHT, K_PAD0_LEFTSTICK_DOWN, K_PAD0_RIGHTSTICK_RIGHT, K_PAD0_RIGHTSTICK_DOWN, K_PAD0_LEFTTRIGGER, K_PAD0_RIGHTTRIGGER };

			qboolean posAnalog = qfalse, negAnalog = qfalse;
			int negKey = negMap[i];
			int posKey = posMap[i];

			if (in_joystickUseAnalog->integer)
			{
				int posAxis = 0, posSign = 0, negAxis = 0, negSign = 0;

				// get axes and axes signs for keys if available
				posAnalog = KeyToAxisAndSign(posKey, &posAxis, &posSign);
				negAnalog = KeyToAxisAndSign(negKey, &negAxis, &negSign);

				// positive to negative/neutral -> keyup if axis hasn't yet been set
				if (posAnalog && !translatedAxesSet[posAxis] && oldAxis > 0 && axis <= 0)
				{
					translatedAxes[posAxis] = 0;
					translatedAxesSet[posAxis] = qtrue;
				}

				// negative to positive/neutral -> keyup if axis hasn't yet been set
				if (negAnalog && !translatedAxesSet[negAxis] && oldAxis < 0 && axis >= 0)
				{
					translatedAxes[negAxis] = 0;
					translatedAxesSet[negAxis] = qtrue;
				}

				// negative/neutral to positive -> keydown
				if (posAnalog && axis > 0)
				{
					translatedAxes[posAxis] = axis * posSign;
					translatedAxesSet[posAxis] = qtrue;
				}

				// positive/neutral to negative -> keydown
				if (negAnalog && axis < 0)
				{
					translatedAxes[negAxis] = -axis * negSign;
					translatedAxesSet[negAxis] = qtrue;
				}
			}

			// keyups first so they get overridden by keydowns later

			// positive to negative/neutral -> keyup
			if (!posAnalog && posKey && oldAxis > 0 && axis <= 0)
				Com_QueueEvent(in_eventTime, SE_KEY, posKey, qfalse, 0, NULL);

			// negative to positive/neutral -> keyup
			if (!negAnalog && negKey && oldAxis < 0 && axis >= 0)
				Com_QueueEvent(in_eventTime, SE_KEY, negKey, qfalse, 0, NULL);

			// negative/neutral to positive -> keydown
			if (!posAnalog && posKey && oldAxis <= 0 && axis > 0)
				Com_QueueEvent(in_eventTime, SE_KEY, posKey, qtrue, 0, NULL);

			// positive/neutral to negative -> keydown
			if (!negAnalog && negKey && oldAxis >= 0 && axis < 0)
				Com_QueueEvent(in_eventTime, SE_KEY, negKey, qtrue, 0, NULL);

			stick_state.oldaaxes[i] = axis;
		}
	}

	// set translated axes
	if (in_joystickUseAnalog->integer)
	{
		for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
		{
			if (translatedAxesSet[i])
				Com_QueueEvent(in_eventTime, SE_JOYSTICK_AXIS, i, translatedAxes[i], 0, NULL);
		}
	}
}


/*
===============
IN_JoyMove
===============
*/
static void IN_JoyMove( void )
{
	unsigned int axes = 0;
	unsigned int hats = 0;
	int total = 0;
	int i = 0;

	in_eventTime = Sys_Milliseconds();

	if (gamepad)
	{
		IN_GamepadMove();
		return;
	}

	if (!stick)
		return;

	SDL_UpdateJoysticks();

	// update the ball state.
	total = SDL_GetNumJoystickBalls(stick);
	if (total > 0)
	{
		int balldx = 0;
		int balldy = 0;
		for (i = 0; i < total; i++)
		{
			int dx = 0;
			int dy = 0;
			SDL_GetJoystickBall(stick, i, &dx, &dy);
			balldx += dx;
			balldy += dy;
		}
		if (balldx || balldy)
		{
			// !!! FIXME: is this good for stick balls, or just mice?
			// Scale like the mouse input...
			if (abs(balldx) > 1)
				balldx *= 2;
			if (abs(balldy) > 1)
				balldy *= 2;
			Com_QueueEvent( in_eventTime, SE_MOUSE, balldx, balldy, 0, NULL );
		}
	}

	// now query the stick buttons...
	total = SDL_GetNumJoystickButtons(stick);
	if (total > 0)
	{
		if (total > ARRAY_LEN(stick_state.buttons))
			total = ARRAY_LEN(stick_state.buttons);
		for (i = 0; i < total; i++)
		{
			qboolean pressed = (SDL_GetJoystickButton(stick, i) != 0);
			if (pressed != stick_state.buttons[i])
			{
				Com_QueueEvent( in_eventTime, SE_KEY, K_JOY1 + i, pressed, 0, NULL );
				stick_state.buttons[i] = pressed;
			}
		}
	}

	// look at the hats...
	total = SDL_GetNumJoystickHats(stick);
	if (total > 0)
	{
		if (total > 4) total = 4;
		for (i = 0; i < total; i++)
		{
			((Uint8 *)&hats)[i] = SDL_GetJoystickHat(stick, i);
		}
	}

	// update hat state
	if (hats != stick_state.oldhats)
	{
		for( i = 0; i < 4; i++ ) {
			if( ((Uint8 *)&hats)[i] != ((Uint8 *)&stick_state.oldhats)[i] ) {
				// release event
				switch( ((Uint8 *)&stick_state.oldhats)[i] ) {
					case SDL_HAT_UP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_DOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					default:
						break;
				}
				// press event
				switch( ((Uint8 *)&hats)[i] ) {
					case SDL_HAT_UP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_DOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					default:
						break;
				}
			}
		}
	}

	// save hat state
	stick_state.oldhats = hats;

	// finally, look at the axes...
	total = SDL_GetNumJoystickAxes(stick);
	if (total > 0)
	{
		if (in_joystickUseAnalog->integer)
		{
			if (total > MAX_JOYSTICK_AXIS) total = MAX_JOYSTICK_AXIS;
			for (i = 0; i < total; i++)
			{
				Sint16 axis = SDL_GetJoystickAxis(stick, i);
				float f = ( (float) abs(axis) ) / 32767.0f;
				
				if( f < in_joystickThreshold->value ) axis = 0;

				if ( axis != stick_state.oldaaxes[i] )
				{
					Com_QueueEvent( in_eventTime, SE_JOYSTICK_AXIS, i, axis, 0, NULL );
					stick_state.oldaaxes[i] = axis;
				}
			}
		}
		else
		{
			if (total > 16) total = 16;
			for (i = 0; i < total; i++)
			{
				Sint16 axis = SDL_GetJoystickAxis(stick, i);
				float f = ( (float) axis ) / 32767.0f;
				if( f < -in_joystickThreshold->value ) {
					axes |= ( 1 << ( i * 2 ) );
				} else if( f > in_joystickThreshold->value ) {
					axes |= ( 1 << ( ( i * 2 ) + 1 ) );
				}
			}
		}
	}

	/* Time to update axes state based on old vs. new. */
	if (axes != stick_state.oldaxes)
	{
		for( i = 0; i < 16; i++ ) {
			if( ( axes & ( 1 << i ) ) && !( stick_state.oldaxes & ( 1 << i ) ) ) {
				Com_QueueEvent( in_eventTime, SE_KEY, joy_keys[i], qtrue, 0, NULL );
			}

			if( !( axes & ( 1 << i ) ) && ( stick_state.oldaxes & ( 1 << i ) ) ) {
				Com_QueueEvent( in_eventTime, SE_KEY, joy_keys[i], qfalse, 0, NULL );
			}
		}
	}

	/* Save for future generations. */
	stick_state.oldaxes = axes;
}
#endif  // USE_JOYSTICK

#ifdef DEBUG_EVENTS
static const char *eventName( Uint32 event )
{
	static char buf[32];

	switch ( event )
	{
		case SDL_EVENT_WINDOW_SHOWN: return "SHOWN";
		case SDL_EVENT_WINDOW_HIDDEN: return "HIDDEN";
		case SDL_EVENT_WINDOW_EXPOSED: return "EXPOSED";
		case SDL_EVENT_WINDOW_MOVED: return "MOVED";
		case SDL_EVENT_WINDOW_RESIZED: return "RESIZED";
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: return "SIZE_CHANGED";
		case SDL_EVENT_WINDOW_MINIMIZED: return "MINIMIZED";
		case SDL_EVENT_WINDOW_MAXIMIZED: return "MAXIMIZED";
		case SDL_EVENT_WINDOW_RESTORED: return "RESTORED";
		case SDL_EVENT_WINDOW_MOUSE_ENTER: return "ENTER";
		case SDL_EVENT_WINDOW_MOUSE_LEAVE: return "LEAVE";
		case SDL_EVENT_WINDOW_FOCUS_GAINED: return "FOCUS_GAINED";
		case SDL_EVENT_WINDOW_FOCUS_LOST: return "FOCUS_LOST";
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED: return "CLOSE";
		case SDL_EVENT_WINDOW_HIT_TEST: return "HIT_TEST"; 
		default:
			sprintf( buf, "EVENT#%u", (unsigned int)event );
			return buf;
	}
}
#endif

/*
===============
IN_SyncModifiers
===============
*/
static void IN_SyncModifiers( void ) {
    SDL_Keymod mod = SDL_GetModState();

    keys[K_CTRL].down  = (mod & SDL_KMOD_CTRL)  ? qtrue : qfalse;
    keys[K_SHIFT].down = (mod & SDL_KMOD_SHIFT) ? qtrue : qfalse;
    keys[K_ALT].down   = (mod & SDL_KMOD_ALT)   ? qtrue : qfalse;
    keys[K_CAPSLOCK].down = (mod & SDL_KMOD_CAPS)   ? qtrue : qfalse;
    keys[K_KP_NUMLOCK].down = (mod & SDL_KMOD_NUM)   ? qtrue : qfalse;
    keys[K_SCROLLOCK].down = (mod & SDL_KMOD_SCROLL)   ? qtrue : qfalse;
}

static void IN_HandleWindowEvent( Uint32 type, const SDL_WindowEvent *window, keyNum_t *lastKeyDown )
{
#ifdef DEBUG_EVENTS
	Com_Printf( "%4i %s\n", window->timestamp, eventName( type ) );
#endif

	switch ( type )
	{
		case SDL_EVENT_WINDOW_MOVED:
			GLW_UpdateWindowState();
			if ( gw_active && !gw_minimized && !glw_state.isFullscreen ) {
				Cvar_SetIntegerValue( "vid_xpos", window->data1 );
				Cvar_SetIntegerValue( "vid_ypos", window->data2 );
			}
			break;

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
		case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
		case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
		case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
			GLW_UpdateWindowState();
			break;

		case SDL_EVENT_WINDOW_HIDDEN:
		case SDL_EVENT_WINDOW_MINIMIZED:
			gw_active = qfalse;
			gw_minimized = qtrue;
			mouse_focus = qfalse;
			break;

		case SDL_EVENT_WINDOW_OCCLUDED:
			if ( glw_state.isFullscreen ) {
				gw_minimized = qtrue;
				mouse_focus = qfalse;
			}
			break;

		case SDL_EVENT_WINDOW_EXPOSED:
		case SDL_EVENT_WINDOW_SHOWN:
		case SDL_EVENT_WINDOW_RESTORED:
		case SDL_EVENT_WINDOW_MAXIMIZED:
			if ( gw_active || !glw_state.isFullscreen ) {
				gw_minimized = qfalse;
			}
			GLW_UpdateWindowState();
			break;

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			*lastKeyDown = 0;
			Key_ClearStates();
			IN_SyncModifiers();
			gw_active = qfalse;
			if ( glw_state.isFullscreen ) {
				gw_minimized = qtrue;
			}
			mouse_focus = qfalse;
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			*lastKeyDown = 0;
			Key_ClearStates();
			IN_SyncModifiers();
			gw_active = qtrue;
			gw_minimized = qfalse;
			mouse_focus = qtrue;
			GLW_UpdateWindowState();
			if ( re.SetColorMappings ) {
				re.SetColorMappings();
			}
			break;

		case SDL_EVENT_WINDOW_MOUSE_ENTER:
			mouse_focus = qtrue;
			break;

		case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			if ( glw_state.isFullscreen ) {
				mouse_focus = qfalse;
			}
			break;

		default:
			break;
	}
}

/*
===============
HandleEvents
===============
*/
//static void IN_ProcessEvents( void )
void HandleEvents( void )
{
	SDL_Event e;
	keyNum_t key = 0;
	static keyNum_t lastKeyDown = 0;

	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
		return;

	in_eventTime = Sys_Milliseconds();

	IN_SyncModifiers();

	while ( SDL_PollEvent( &e ) )
	{
		switch( e.type )
		{
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
			{
				qboolean down = (e.type == SDL_EVENT_KEY_DOWN) ? qtrue : qfalse;
				if ( down && e.key.repeat && Key_GetCatcher() == 0 )
					break;
				key = IN_TranslateSDLToETKey( &e.key, down );

				if ( key == K_ENTER && down && ( e.key.mod & SDL_KMOD_ALT ) ) {
					Cvar_SetIntegerValue( "r_fullscreen", glw_state.isFullscreen ? 0 : 1 );
					Cbuf_AddText( "vid_restart\n" );
					break;
				}

				if ( key ) {
					Com_QueueEvent( in_eventTime, SE_KEY, key, down, 0, NULL );

					if ( down ) {
						if ( key == K_BACKSPACE )
							Com_QueueEvent( in_eventTime, SE_CHAR, CTRL('h'), 0, 0, NULL );
						else if ( key == K_ESCAPE )
							Com_QueueEvent( in_eventTime, SE_CHAR, key, 0, 0, NULL );
						else if( keys[K_CTRL].down && key >= 'a' && key <= 'z' )
							Com_QueueEvent( in_eventTime, SE_CHAR, CTRL(key), 0, 0, NULL );
#ifdef SDL_PLATFORM_MACOS
						else if( keys[K_COMMAND].down && key == 'v' )
							Com_QueueEvent( in_eventTime, SE_CHAR, CTRL(key), 0, 0, NULL );
#endif
					}
				}

				if ( down )
					lastKeyDown = key;
				else
					lastKeyDown = 0;
				break;
			}

			case SDL_EVENT_TEXT_INPUT:
				if( lastKeyDown != K_CONSOLE )
				{
					const char *c = e.text.text;

					// Quick and dirty UTF-8 to UTF-32 conversion
					while ( *c )
					{
						int utf32 = 0;

						if( ( *c & 0x80 ) == 0 )
							utf32 = *c++;
						else if( ( *c & 0xE0 ) == 0xC0 ) // 110x xxxx
						{
							utf32 |= ( *c++ & 0x1F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else if( ( *c & 0xF0 ) == 0xE0 ) // 1110 xxxx
						{
							utf32 |= ( *c++ & 0x0F ) << 12;
							utf32 |= ( *c++ & 0x3F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else if( ( *c & 0xF8 ) == 0xF0 ) // 1111 0xxx
						{
							utf32 |= ( *c++ & 0x07 ) << 18;
							utf32 |= ( *c++ & 0x3F ) << 12;
							utf32 |= ( *c++ & 0x3F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else
						{
							Com_DPrintf( "Unrecognised UTF-8 lead byte: 0x%x\n", (unsigned int)*c );
							c++;
						}

						if( utf32 != 0 )
						{
							if ( IN_IsConsoleKey( 0, utf32 ) )
							{
								Com_QueueEvent( in_eventTime, SE_KEY, K_CONSOLE, qtrue, 0, NULL );
								Com_QueueEvent( in_eventTime, SE_KEY, K_CONSOLE, qfalse, 0, NULL );
							}
							else
								Com_QueueEvent( in_eventTime, SE_CHAR, utf32, 0, 0, NULL );
						}
					}
				}
				break;

			case SDL_EVENT_MOUSE_MOTION:
				if( mouseActive )
				{
					if( !e.motion.xrel && !e.motion.yrel )
						break;
					Com_QueueEvent( in_eventTime, SE_MOUSE, e.motion.xrel, e.motion.yrel, 0, NULL );
				}
				break;

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				{
					int b;
					switch( e.button.button )
					{
						case SDL_BUTTON_LEFT:   b = K_MOUSE1;     break;
						case SDL_BUTTON_MIDDLE: b = K_MOUSE3;     break;
						case SDL_BUTTON_RIGHT:  b = K_MOUSE2;     break;
						case SDL_BUTTON_X1:     b = K_MOUSE4;     break;
						case SDL_BUTTON_X2:     b = K_MOUSE5;     break;
						default:                b = K_AUX1 + ( e.button.button - SDL_BUTTON_X2 + 1 ) % 16; break;
					}
					Com_QueueEvent( in_eventTime, SE_KEY, b,
						( e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? qtrue : qfalse ), 0, NULL );
				}
				break;

			case SDL_EVENT_MOUSE_WHEEL:
				if( e.wheel.y > 0 )
				{
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
				}
				else if( e.wheel.y < 0 )
				{
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
				}
				break;

#ifdef USE_JOYSTICK
			case SDL_EVENT_JOYSTICK_ADDED:
			case SDL_EVENT_JOYSTICK_REMOVED:
			case SDL_EVENT_GAMEPAD_ADDED:
			case SDL_EVENT_GAMEPAD_REMOVED:
				if ( in_joystick->integer )
					IN_InitJoystick();
				break;
#endif

			case SDL_EVENT_QUIT:
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				Cbuf_ExecuteText( EXEC_NOW, "quit Closed window\n" );
				break;

			case SDL_EVENT_WINDOW_MOVED:
			case SDL_EVENT_WINDOW_HIDDEN:
			case SDL_EVENT_WINDOW_MINIMIZED:
			case SDL_EVENT_WINDOW_SHOWN:
			case SDL_EVENT_WINDOW_RESTORED:
			case SDL_EVENT_WINDOW_MAXIMIZED:
			case SDL_EVENT_WINDOW_FOCUS_LOST:
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			case SDL_EVENT_WINDOW_MOUSE_ENTER:
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			case SDL_EVENT_WINDOW_RESIZED:
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
			case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
			case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
			case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
				IN_HandleWindowEvent( e.type, &e.window, &lastKeyDown );
				break;

			default:
				break;
		}
	}
}


/*
===============
IN_Minimize

Minimize the game so that user is back at the desktop
===============
*/
static void IN_Minimize( void )
{
	if ( CL_VideoRecording() == AVIDEMO_NONE || ( re.CanMinimize && re.CanMinimize() ) )
	{
		SDL_MinimizeWindow( SDL_window );
	}
}


/*
===============
IN_Frame
===============
*/
void IN_Frame( void )
{
#ifdef USE_JOYSTICK
	IN_JoyMove();
#endif

	if ( Key_GetCatcher() & KEYCATCH_CONSOLE ) {
		// temporarily deactivate if not in the game and
		// running on the desktop with multimonitor configuration
		if ( !glw_state.isFullscreen || glw_state.monitorCount > 1 ) {
			IN_DeactivateMouse();
			return;
		}
	}

	if ( !gw_active || !mouse_focus || in_nograb->integer ) {
		IN_DeactivateMouse();
		return;
	}

	IN_ActivateMouse();

	// Only parse the variable when it changes
	if ( cl_consoleKeys->modified )
	{
		IN_ParseConsoleKeys();
	}

	//IN_ProcessEvents();
	//HandleEvents();

	// Set event time for next frame to earliest possible time an event could happen
	//in_eventTime = Sys_Milliseconds();
}


/*
===============
IN_Restart
===============
*/
static void IN_Restart( void )
{
#ifdef USE_JOYSTICK
	IN_ShutdownJoystick();
#endif
	IN_Shutdown();
	IN_Init();
}


/*
===============
IN_Init
===============
*/
void IN_Init( void )
{
	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
	{
		Com_Error( ERR_VID_FATAL, "IN_Init called before SDL_Init( SDL_INIT_VIDEO )" );
		return;
	}

	Com_DPrintf( "\n------- Input Initialization -------\n" );

	in_keyboardDebug = Cvar_Get( "in_keyboardDebug", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( in_keyboardDebug, "Print keyboard debug info" );
	in_forceCharset = Cvar_Get( "in_forceCharset", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( in_forceCharset, "Try to translate non-ASCII chars in keyboard input or force EN/US keyboard layout" );

	// mouse variables
	in_mouse = Cvar_Get( "in_mouse", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( in_mouse, "-1", "1", CV_INTEGER );
	Cvar_SetDescription( in_mouse,
		"Mouse data input source:\n" \
		" -1 - SDL mouse (relative mode disabled)\n" \
		"  0 - disable mouse input\n" \
		"  1 - SDL raw mouse (relative mode)" );

#ifdef USE_JOYSTICK
	in_joystick = Cvar_Get( "in_joystick", "0", CVAR_ARCHIVE|CVAR_LATCH );
	Cvar_SetDescription( in_joystick, "Whether or not joystick support is on" );
	in_joystickThreshold = Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE );
	Cvar_SetDescription( in_joystickThreshold, "Threshold of joystick moving dictance" );

	j_pitch =        Cvar_Get( "j_pitch",        "0.022", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( j_pitch, "Joystick pitch rotation speed/direction" );
	j_yaw =          Cvar_Get( "j_yaw",          "-0.022", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( j_yaw, "Joystick yaw rotation speed/direction" );
	j_forward =      Cvar_Get( "j_forward",      "-0.25", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( j_forward, "Joystick forward movement speed/direction" );
	j_side =         Cvar_Get( "j_side",         "0.25", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( j_side, "Joystick side movement speed/direction" );
	j_up =           Cvar_Get( "j_up",           "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( j_up, "Joystick up movement speed/direction" );

	j_pitch_axis =   Cvar_Get( "j_pitch_axis",   "3", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( j_pitch_axis,   "0", va("%i",MAX_JOYSTICK_AXIS-1), CV_INTEGER );
	Cvar_SetDescription( j_pitch_axis, "Selects which joystick axis controls pitch" );
	j_yaw_axis =     Cvar_Get( "j_yaw_axis",     "2", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( j_yaw_axis,     "0", va("%i",MAX_JOYSTICK_AXIS-1), CV_INTEGER );
	Cvar_SetDescription( j_yaw_axis, "Selects which joystick axis controls yaw" );
	j_forward_axis = Cvar_Get( "j_forward_axis", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( j_forward_axis, "0", va("%i",MAX_JOYSTICK_AXIS-1), CV_INTEGER );
	Cvar_SetDescription( j_forward_axis, "Selects which joystick axis controls forward/back" );
	j_side_axis =    Cvar_Get( "j_side_axis",    "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( j_side_axis,    "0", va("%i",MAX_JOYSTICK_AXIS-1), CV_INTEGER );
	Cvar_SetDescription( j_side_axis, "Selects which joystick axis controls left/right" );
	j_up_axis =      Cvar_Get( "j_up_axis",      "4", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( j_up_axis,      "0", va("%i",MAX_JOYSTICK_AXIS-1), CV_INTEGER );
	Cvar_SetDescription( j_up_axis, "Selects which joystick axis controls up/down" );
#endif

	// ~ and `, as keys and characters
	cl_consoleKeys = Cvar_Get( "cl_consoleKeys", "~ ` 0x7e 0x60", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_consoleKeys, "Space delimited list of key names or characters that toggle the console" );

	// Parse the console keys at least once
	IN_ParseConsoleKeys();

	mouseAvailable = ( in_mouse->value != 0 ) ? qtrue : qfalse;
	mouse_focus = ( SDL_GetMouseFocus() == SDL_window ) ? qtrue : ( glw_state.isFullscreen ? qtrue : qfalse );

	if ( SDL_window && !SDL_StartTextInput( SDL_window ) ) {
		Com_DPrintf( "SDL_StartTextInput failed: %s\n", SDL_GetError() );
	}

	//IN_DeactivateMouse();

#ifdef USE_JOYSTICK
	IN_InitJoystick();
#endif

	Cmd_AddCommand( "minimize", IN_Minimize );
	Cmd_AddCommand( "in_restart", IN_Restart );

	Com_DPrintf( "------------------------------------\n" );
}


/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void )
{
	if ( SDL_window ) {
		SDL_StopTextInput( SDL_window );
	}

	IN_DeactivateMouse();

	mouseAvailable = qfalse;
	mouse_focus = qfalse;

#ifdef USE_JOYSTICK
	IN_ShutdownJoystick();
#endif

	Cmd_RemoveCommand( "minimize" );
	Cmd_RemoveCommand( "in_restart" );
}
