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
// win_local.h: Win32-specific Quake3 header file

#ifndef __WIN_LOCAL_H__
#define __WIN_LOCAL_H__

#define RAW_INPUT

#define FAST_MODE_SWITCH

#ifdef RAW_INPUT

#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif

#endif

//#if defined (_MSC_VER) && (_MSC_VER >= 1200)
//#pragma warning(disable : 4201)
//#pragma warning( push )
//#endif
#include <windows.h>
//#if defined (_MSC_VER) && (_MSC_VER >= 1200)
//#pragma warning( pop )
//#endif

#define HK_MOD_ALT		0x00100
#define HK_MOD_CONTROL  0x00200
#define HK_MOD_SHIFT	0x00400
#define HK_MOD_WIN		0x00800
#define HK_MOD_MASK		0x00F00
#define HK_MOD_LALT		0x01000
#define HK_MOD_RALT		0x02000
#define HK_MOD_LCONTROL	0x04000
#define HK_MOD_RCONTROL	0x08000
#define HK_MOD_LSHIFT	0x10000
#define HK_MOD_RSHIFT	0x20000
#define HK_MOD_LWIN		0x40000
#define HK_MOD_RWIN		0x80000
#define HK_MOD_XMASK	0xFF000

#define	DIRECTSOUND_VERSION	0x0300
#define	DIRECTINPUT_VERSION	0x0300

#include <mmsystem.h>
#include <dinput.h>
#include <dsound.h>
#include <shlobj.h>

#undef open
#define open _open
#undef close
#define close _close
#undef write
#define write _write

#ifndef MK_XBUTTON1
#define MK_XBUTTON1         0x0020
#endif
#ifndef MK_XBUTTON2
#define MK_XBUTTON2         0x0040
#endif

#define	WINDOW_STYLE_NORMAL          (WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_SYSMENU|WS_CAPTION|WS_MINIMIZEBOX|WS_BORDER)
#define	WINDOW_STYLE_NORMAL_NB       (WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_POPUP)
#define	WINDOW_ESTYLE_NORMAL         (0)
#define	WINDOW_STYLE_FULLSCREEN      (WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_POPUP)
#define	WINDOW_ESTYLE_FULLSCREEN     (0)
#define	WINDOW_STYLE_FULLSCREEN_MIN  (WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS)
#define	WINDOW_ESTYLE_FULLSCREEN_MIN (0)

#define T TEXT
#ifdef UNICODE
LPWSTR AtoW( const char *s );
const char *WtoA( const LPWSTR s );
#else
#define AtoW(S) (S)
#define WtoA(S) (S)
#endif

qboolean IN_MouseActive( void );
void	IN_Win32MouseEvent( int mstate );
void	IN_RawMouseEvent( LPARAM lParam );

void	Sys_CreateConsole( const char *title, int xPos, int yPos, qboolean usePos );
void	Sys_DestroyConsole( void );

void	Sys_ClearViewlog_f( void );

// Input subsystem

void	IN_Init (void);
void	IN_Shutdown (void);

void	IN_Activate( qboolean active );
void	IN_Frame( void );

void	IN_UpdateWindow( RECT *window_rect, qboolean updateClipRegion );
void	UpdateMonitorInfo( const RECT *target );

// window procedure
LRESULT WINAPI MainWndProc( HWND hWnd, UINT uMsg, WPARAM  wParam, LPARAM  lParam );
void HandleConsoleEvents( void );

void Conbuf_AppendText( const char *msg );

void SNDDMA_Activate( void );

typedef struct
{
	HINSTANCE		hInstance;
	HWND			hWnd;

	// Multi-monitor tracking
	RECT			conRect;
#ifndef DEDICATED
	RECT			winRect;
	qboolean		winRectValid;

	int				borderless;

	// when we get a windows message, we store the time off so keyboard processing
	// can know the exact time of an event
	unsigned		sysMsgTime;
#endif
} WinVars_t;

extern WinVars_t	g_wv;

void WIN_DisableHook( void );
void WIN_EnableHook( void );

void WIN_DisableAltTab( void );
void WIN_EnableAltTab( void );

void WIN_Minimize( void );

void GLW_HideFullscreenWindow( void );
void GLW_RestoreGamma( void );

void SetupDPIAwareness( void );

#endif
