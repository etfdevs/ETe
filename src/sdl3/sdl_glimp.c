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
#include "../renderercommon/tr_public.h"
#include "sdl_glw.h"
#include "sdl_icon.h"

typedef enum {
	RSERR_OK = 0,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_FATAL_ERROR,
	RSERR_UNKNOWN
} rserr_t;

glwstate_t glw_state;

SDL_Window *SDL_window = NULL;
static SDL_GLContext SDL_glContext = NULL;
#ifdef USE_VULKAN_API
static PFN_vkGetInstanceProcAddr qvkGetInstanceProcAddr;
#endif
static qboolean glw_floatFramebufferActive = qfalse;

// framebuffer attributes the current SDL_window was created with;
// the window can only be reused across \vid_restart while these still match
// because the pixel format and the OpenGL/Vulkan surface kind are fixed
// at window creation time
typedef struct {
	qboolean valid;
	qboolean vulkan;
	int colorBits;
	int depthBits;
	int stencilBits;
	qboolean stereo;
	qboolean software;
	qboolean floatFramebuffer;
} windowRecipe_t;

static windowRecipe_t glw_windowRecipe;

cvar_t *r_stereoEnabled;
cvar_t *in_nograb;

static void GLW_ShowCursor( qboolean show )
{
	if ( show ) {
		SDL_ShowCursor();
	} else {
		SDL_HideCursor();
	}
}

static SDL_Window *GLW_CreateWindow( const char *title, int x, int y, int w, int h, SDL_WindowFlags flags )
{
	SDL_Window *window;
	SDL_PropertiesID props = SDL_CreateProperties();

	if ( !props ) {
		Com_DPrintf( "SDL_CreateProperties failed: %s\n", SDL_GetError() );
		return NULL;
	}

	SDL_SetStringProperty( props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title );
	SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, (Sint64)flags );
	SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x );
	SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y );
	SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, w );
	SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, h );

	window = SDL_CreateWindowWithProperties( props );
	SDL_DestroyProperties( props );

	return window;
}

static void GLW_DestroyWindow( void )
{
	if ( SDL_glContext != NULL )
	{
		SDL_GL_DestroyContext( SDL_glContext );
		SDL_glContext = NULL;
	}
	glw_floatFramebufferActive = qfalse;

	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	Com_Memset( &glw_windowRecipe, 0, sizeof( glw_windowRecipe ) );
}

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( qboolean unloadDLL )
{
	const char* drv = SDL_GetCurrentVideoDriver();

	IN_Shutdown();

	if ( glw_state.isFullscreen ) {
		if ( drv && strcmp( drv, "x11" ) == 0 ) {
			SDL_WarpMouseGlobal( glw_state.desktop_width / 2, glw_state.desktop_height / 2 );
		} else {
			GLW_ShowCursor( qtrue );
		}
	}

	GLW_DestroyWindow();

	if ( unloadDLL )
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
}


void GLimp_FlashWindow( int state )
{
	if ( !SDL_window )
		return;

	if ( state == 1 )
		SDL_FlashWindow( SDL_window, SDL_FLASH_BRIEFLY );
	else if ( state == 2 )
		SDL_FlashWindow( SDL_window, SDL_FLASH_UNTIL_FOCUSED );
	else
		SDL_FlashWindow( SDL_window, SDL_FLASH_CANCEL );
}


/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( const char *comment )
{
}


static void GLW_SyncWindow( const char *reason )
{
	if ( SDL_window && !SDL_SyncWindow( SDL_window ) ) {
		Com_DPrintf( "SDL_SyncWindow failed after %s: %s\n", reason, SDL_GetError() );
	}
}


void GLW_UpdateWindowState( void )
{
	SDL_DisplayID display = 0;
	const SDL_DisplayMode *desktopMode;
	int numDisplays = 0;
	SDL_DisplayID *displays = SDL_GetDisplays( &numDisplays );

	if ( displays ) {
		SDL_free( displays );
	}

	if ( numDisplays > 0 ) {
		glw_state.monitorCount = numDisplays;
	} else if ( glw_state.monitorCount <= 0 ) {
		glw_state.monitorCount = 1;
	}

	if ( SDL_window ) {
		SDL_WindowFlags windowFlags = SDL_GetWindowFlags( SDL_window );
		int w, h;

		glw_state.isFullscreen = ( windowFlags & SDL_WINDOW_FULLSCREEN ) ? qtrue : qfalse;
		if ( glw_state.config ) {
			glw_state.config->isFullscreen = glw_state.isFullscreen;
		}

		if ( !SDL_GetWindowSize( SDL_window, &w, &h ) ) {
			Com_DPrintf( "SDL_GetWindowSize failed: %s\n", SDL_GetError() );
		} else {
			glw_state.window_width = w;
			glw_state.window_height = h;
		}

		if ( !SDL_GetWindowSizeInPixels( SDL_window, &w, &h ) ) {
			Com_DPrintf( "SDL_GetWindowSizeInPixels failed: %s\n", SDL_GetError() );
		} else {
			glw_state.pixel_width = w;
			glw_state.pixel_height = h;
		}

		display = SDL_GetDisplayForWindow( SDL_window );
		if ( !display ) {
			Com_DPrintf( "SDL_GetDisplayForWindow() failed: %s\n", SDL_GetError() );
		}
	}

	desktopMode = display ? SDL_GetDesktopDisplayMode( display ) : NULL;
	if ( desktopMode ) {
		glw_state.desktop_width = desktopMode->w;
		glw_state.desktop_height = desktopMode->h;
	} else if ( !glw_state.desktop_width || !glw_state.desktop_height ) {
		glw_state.desktop_width = 640;
		glw_state.desktop_height = 480;
	}
}


static qboolean GLW_EnterFullscreen( SDL_Window *window, const SDL_DisplayMode *mode )
{
	qboolean exclusiveTried = qfalse;

#ifndef MACOS_X
	if ( mode ) {
		exclusiveTried = qtrue;
		if ( !SDL_SetWindowFullscreenMode( window, mode ) ) {
			Com_DPrintf( "SDL_SetWindowFullscreenMode failed: %s\n", SDL_GetError() );
		} else if ( SDL_SetWindowFullscreen( window, true ) ) {
			return qtrue;
		} else {
			Com_DPrintf( "SDL_SetWindowFullscreen failed: %s\n", SDL_GetError() );
		}
	}
#endif

	if ( !SDL_SetWindowFullscreenMode( window, NULL ) ) {
		Com_DPrintf( "SDL_SetWindowFullscreenMode failed: %s\n", SDL_GetError() );
		return qfalse;
	}

	if ( !SDL_SetWindowFullscreen( window, true ) ) {
		Com_DPrintf( "SDL_SetWindowFullscreen failed: %s\n", SDL_GetError() );
		return qfalse;
	}

	if ( exclusiveTried ) {
		Com_Printf( "...falling back to desktop fullscreen\n" );
	}

	return qtrue;
}


static SDL_DisplayID FindNearestDisplay( int *x, int *y, int w, int h )
{
	const int cx = *x + w / 2;
	const int cy = *y + h / 2;
	int i, index, numDisplays;
	SDL_Rect *list, *m;
	SDL_DisplayID display = 0;
	SDL_DisplayID *displays;

	index = -1; // selected display index

	displays = SDL_GetDisplays( &numDisplays );
	if ( !displays || numDisplays <= 0 ) {
		if ( displays ) {
			SDL_free( displays );
		}
		return 0;
	}

	glw_state.monitorCount = numDisplays;

	list = Z_Malloc( numDisplays * sizeof( list[0] ) );

	for ( i = 0; i < numDisplays; i++ )
	{
		if ( !SDL_GetDisplayBounds( displays[i], list + i ) ) {
			list[i].x = 0;
			list[i].y = 0;
			list[i].w = 0;
			list[i].h = 0;
		}
		//Com_Printf( "[%i]: x=%i, y=%i, w=%i, h=%i\n", i, list[i].x, list[i].y, list[i].w, list[i].h );
	}

	// select display by window center intersection
	for ( i = 0; i < numDisplays; i++ )
	{
		m = list + i;
		if ( cx >= m->x && cx < (m->x + m->w) && cy >= m->y && cy < (m->y + m->h) )
		{
			index = i;
			break;
		}
	}

	// select display by nearest distance between window center and display center
	if ( index == -1 )
	{
		unsigned long nearest, dist;
		int dx, dy;
		nearest = ~0UL;
		for ( i = 0; i < numDisplays; i++ )
		{
			m = list + i;
			dx = (m->x + m->w/2) - cx;
			dy = (m->y + m->h/2) - cy;
			dist = ( dx * dx ) + ( dy * dy );
			if ( dist < nearest )
			{
				nearest = dist;
				index = i;
			}
		}
	}

	// adjust x and y coordinates if needed
	if ( index >= 0 )
	{
		m = list + index;
		display = displays[index];
		if ( *x < m->x )
			*x = m->x;

		if ( *y < m->y )
			*y = m->y;
	}

	Z_Free( list );
	SDL_free( displays );

	return display;
}

typedef struct window_Position_s {
	int x;
	int y;
} windowPosition_t;

typedef struct window_Bounds_s {
	int x;
    int y;
    int width;
    int height;
} windowBounds_t;

typedef struct window_Insets_s {
	int left;
	int top;
	int right;
	int bottom;
} windowInsets_t;

static ID_INLINE int64_t Window_NonNegative(int value) {
    return value > 0 ? (int64_t)value : 0;
}

static ID_INLINE int Window_SaturateToInt(int64_t value) {
    return value < INT_MIN
        ? INT_MIN
        : value > INT_MAX
            ? INT_MAX
            : (int)value;
}

static ID_INLINE int64_t Window_ConstrainAxis(int64_t desired,
    int64_t boundsOrigin, int64_t boundsExtent,
    int64_t contentExtent, int64_t leadingInset,
    int64_t trailingInset) {
    
    const int64_t minimum = boundsOrigin + leadingInset;
    const int64_t maximum = boundsOrigin + boundsExtent
        - contentExtent - trailingInset;

    // An oversized window cannot fit completely. Keep its leading frame and
    // title bar reachable instead of centring an inaccessible decoration.
    if (maximum < minimum) {
        return minimum;
    }
    if (desired < minimum) {
        return minimum;
    }
    if (desired > maximum) {
        return maximum;
    }
    return desired;
}

static ID_INLINE windowPosition_t Window_ConstrainClientOrigin(
    windowPosition_t desired, int clientWidth,
    int clientHeight, windowBounds_t usable, 
    windowInsets_t decorations) {

	windowPosition_t result = desired;
    
    if (usable.width <= 0 || usable.height <= 0) {
        return result;
    }

	result.x = Window_SaturateToInt( Window_ConstrainAxis( desired.x, usable.x, usable.width, 
        Window_NonNegative( clientWidth ), Window_NonNegative( decorations.left ), 
        Window_NonNegative( decorations.right ) ) );

    result.y = Window_SaturateToInt( Window_ConstrainAxis( desired.y, usable.y, usable.height, 
        Window_NonNegative( clientHeight ), Window_NonNegative( decorations.top ), 
        Window_NonNegative( decorations.bottom ) ) );

	return result;
}

static SDL_DisplayID GLW_ConstrainWindowPosition( SDL_Window *window,
	int *x, int *y, int w, int h )
{
	SDL_Rect requested = { *x, *y, w, h };
	SDL_Rect usable;
	SDL_DisplayID display;
	
	windowInsets_t decorations = {0, 0, 0, 0};
	windowPosition_t desired;
	windowBounds_t bounds;
	windowPosition_t constrained;

	display = SDL_GetDisplayForRect( &requested );
	if ( !display ) {
		display = SDL_GetPrimaryDisplay();
	}
	if ( !display ) {
		return 0;
	}

	if ( !SDL_GetDisplayUsableBounds( display, &usable ) &&
		!SDL_GetDisplayBounds( display, &usable ) ) {
		Com_DPrintf( "SDL display bounds query failed: %s\n", SDL_GetError() );
		return display;
	}

	if ( window ) {
		SDL_GetWindowBordersSize( window, &decorations.top, &decorations.left,
			&decorations.bottom, &decorations.right );
	}

	desired.x = *x;
	desired.y = *y;
	
	bounds.x = usable.x;
	bounds.y = usable.y;
	bounds.width = usable.w;
	bounds.height = usable.h;

	constrained = Window_ConstrainClientOrigin( desired, w, h, bounds, decorations );

	*x = constrained.x;
	*y = constrained.y;

	return display;
}


void GLW_EnsureWindowOnScreen( void )
{
	SDL_WindowFlags flags;
	int x, y, w, h;
	int constrainedX, constrainedY;

	if ( !SDL_window ) {
		return;
	}

	flags = SDL_GetWindowFlags( SDL_window );
	if ( flags & ( SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED ) ) {
		return;
	}

	if ( !SDL_GetWindowPosition( SDL_window, &x, &y ) ||
		!SDL_GetWindowSize( SDL_window, &w, &h ) ) {
		Com_DPrintf( "SDL window geometry query failed: %s\n", SDL_GetError() );
		return;
	}

	constrainedX = x;
	constrainedY = y;
	GLW_ConstrainWindowPosition( SDL_window, &constrainedX, &constrainedY, w, h );
	if ( constrainedX == x && constrainedY == y ) {
		return;
	}

	if ( !SDL_SetWindowPosition( SDL_window, constrainedX, constrainedY ) ) {
		Com_DPrintf( "SDL_SetWindowPosition failed while recovering window: %s\n", SDL_GetError() );
		return;
	}
	GLW_SyncWindow( "window placement recovery" );
	Cvar_SetIntegerValue( "vid_xpos", constrainedX );
	Cvar_SetIntegerValue( "vid_ypos", constrainedY );
}


static SDL_HitTestResult SDL_HitTestFunc( SDL_Window *win, const SDL_Point *area, void *data )
{
	if ( Key_GetCatcher() & KEYCATCH_CONSOLE && keys[ K_ALT ].down )
		return SDL_HITTEST_DRAGGABLE;

	return SDL_HITTEST_NORMAL;
}

/*
===============
GLW_ApplyFullscreen

Switch the window into the requested fullscreen mode and refresh the
reported display frequency.
===============
*/
static qboolean GLW_ApplyFullscreen( glconfig_t *config, SDL_DisplayID display, int colorBits )
{
	SDL_DisplayMode mode;
	const SDL_DisplayMode *currentMode;
	SDL_DisplayID fullscreenDisplay;

	SDL_zero( mode );
	mode.displayID = display;

	switch ( colorBits )
	{
		case 16: mode.format = SDL_PIXELFORMAT_RGB565; break;
		case 24: mode.format = SDL_PIXELFORMAT_RGB24;  break;
		default:
			Com_DPrintf( "colorBits is %d, can't fullscreen\n", colorBits );
			return qfalse;
	}

	mode.w = config->vidWidth;
	mode.h = config->vidHeight;
	mode.refresh_rate = /* config->displayFrequency = */ Cvar_VariableIntegerValue( "r_displayRefresh" );

	if ( !GLW_EnterFullscreen( SDL_window, &mode ) ) {
		return qfalse;
	}

	GLW_SyncWindow( "fullscreen transition" );
	GLW_UpdateWindowState();

	if ( ( currentMode = SDL_GetWindowFullscreenMode( SDL_window ) ) != NULL ) {
		config->displayFrequency = currentMode->refresh_rate;
	} else {
		fullscreenDisplay = SDL_GetDisplayForWindow( SDL_window );
		currentMode = fullscreenDisplay ? SDL_GetCurrentDisplayMode( fullscreenDisplay ) : NULL;
		if ( currentMode ) {
			config->displayFrequency = currentMode->refresh_rate;
		}
	}

	return qtrue;
}


/*
===============
GLW_SetupDrawableContext

Create the OpenGL context on the current window - or just publish the
framebuffer attributes for Vulkan - and report the resulting configuration.
===============
*/
static qboolean GLW_SetupDrawableContext( glconfig_t *config, qboolean vulkan, int testColorBits, int testDepthBits, int testStencilBits, qboolean requestFloatFramebuffer )
{
#ifdef USE_VULKAN_API
	if ( vulkan )
	{
		config->colorBits = testColorBits;
		config->depthBits = testDepthBits;
		config->stencilBits = testStencilBits;
	}
	else
#endif
	{
		int realColorBits[3];

		if ( !SDL_glContext )
		{
			SDL_glContext = SDL_GL_CreateContext( SDL_window );
			if ( !SDL_glContext )
			{
				/*if ( GLW_ShouldRequestGLxDebugContext() )
				{
					Com_DPrintf( "SDL_GL_CreateContext with debug flag failed: %s\n", SDL_GetError( ) );
					SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
					SDL_glContext.reset( SDL_GL_CreateContext( SDL_window ) );
				}*/

				if ( !SDL_glContext )
				{
					Com_DPrintf( "SDL_GL_CreateContext failed: %s\n", SDL_GetError( ) );
					return qfalse;
				}

				/*if ( GLW_ShouldRequestGLxDebugContext() )
				{
					Com_Printf( "...SDL debug context unavailable, using regular OpenGL context\n" );
				}*/
			}
			/*else if ( GLW_ShouldRequestGLxDebugContext() )
			{
				Com_Printf( "...created SDL OpenGL debug context\n" );
			}*/
		}

		if ( !SDL_GL_SetSwapInterval( r_swapInterval->integer ) )
		{
			Com_DPrintf( "SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError( ) );
		}

		SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &realColorBits[0] );
		SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &realColorBits[1] );
		SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &realColorBits[2] );
		SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &config->depthBits );
		SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &config->stencilBits );
		{
			int realFloatFramebuffer = 0;
			if ( SDL_GL_GetAttribute( SDL_GL_FLOATBUFFERS, &realFloatFramebuffer ) ) {
				glw_floatFramebufferActive = realFloatFramebuffer ? qtrue : qfalse;
			} else {
				glw_floatFramebufferActive = qfalse;
			}
			if ( requestFloatFramebuffer && !glw_floatFramebufferActive ) {
				Com_DPrintf( "SDL did not provide a floating-point OpenGL framebuffer for HDR output: %s\n",
					SDL_GetError() );
			}
		}

		config->colorBits = realColorBits[0] + realColorBits[1] + realColorBits[2];
	}

	Com_Printf( "Using %d color bits, %d depth, %d stencil display.\n", config->colorBits, config->depthBits, config->stencilBits );

	return qtrue;
}


/*
===============
GLW_CanReuseWindow
===============
*/
static qboolean GLW_CanReuseWindow( qboolean vulkan, int colorBits, int depthBits, int stencilBits, qboolean requestFloatFramebuffer )
{
	if ( SDL_window == NULL || !glw_windowRecipe.valid ) {
		return qfalse;
	}

	if ( glw_windowRecipe.vulkan != vulkan ) {
		return qfalse;
	}

	if ( vulkan ) {
		return qtrue;
	}

	// the GL pixel format is fixed at window creation time: any change
	// there requires a full window re-creation
	if ( glw_windowRecipe.colorBits != colorBits ||
		glw_windowRecipe.depthBits != depthBits ||
		glw_windowRecipe.stencilBits != stencilBits ||
		glw_windowRecipe.stereo != ( r_stereoEnabled->integer ? qtrue : qfalse ) ||
		glw_windowRecipe.software != ( ( r_allowSoftwareGL && r_allowSoftwareGL->integer ) ? qtrue : qfalse ) ||
		glw_windowRecipe.floatFramebuffer != requestFloatFramebuffer ) {
		return qfalse;
	}

	return qtrue;
}


/*
===============
GLW_ReuseExistingWindow

\vid_restart fast path: morph the existing window in place - windowed or
fullscreen state, borders, geometry - and rebuild the drawable context on
it instead of destroying it. Any failure makes the caller fall back to a
full window re-creation, so a partially morphed window is never kept.
===============
*/
static qboolean GLW_ReuseExistingWindow( glconfig_t *config, SDL_DisplayID display, qboolean fullscreen, qboolean vulkan, int colorBits, int depthBits, int stencilBits, qboolean requestFloatFramebuffer )
{
	SDL_WindowFlags windowFlags;

	if ( !GLW_CanReuseWindow( vulkan, colorBits, depthBits, stencilBits, requestFloatFramebuffer ) ) {
		return qfalse;
	}

	config->stereoEnabled = glw_windowRecipe.stereo;

	if ( fullscreen )
	{
		if ( !GLW_ApplyFullscreen( config, display, colorBits ) ) {
			return qfalse;
		}
	}
	else
	{
		int x, y;
		//const qboolean retainOsGeometry = CL_IsWindowResizeRestart();

		if ( !SDL_SetWindowFullscreen( SDL_window, false ) ) {
			Com_DPrintf( "SDL_SetWindowFullscreen failed: %s\n", SDL_GetError() );
			return qfalse;
		}

		if ( !SDL_SetWindowBordered( SDL_window, r_noborder->integer ? false : true ) ) {
			Com_DPrintf( "SDL_SetWindowBordered failed: %s\n", SDL_GetError() );
		}
		if ( !SDL_SetWindowResizable( SDL_window, true ) ) {
			Com_DPrintf( "SDL_SetWindowResizable failed: %s\n", SDL_GetError() );
		}

		//if ( !retainOsGeometry ) {
			if ( !SDL_SetWindowSize( SDL_window, config->vidWidth, config->vidHeight ) ) {
				Com_DPrintf( "SDL_SetWindowSize failed: %s\n", SDL_GetError() );
			}
			GLW_SyncWindow( "windowed style transition" );
			x = vid_xpos->integer;
			y = vid_ypos->integer;
			GLW_ConstrainWindowPosition( SDL_window, &x, &y,
				config->vidWidth, config->vidHeight );
			if ( !SDL_SetWindowPosition( SDL_window, x, y ) ) {
				Com_DPrintf( "SDL_SetWindowPosition failed: %s\n", SDL_GetError() );
			}
		//}

		GLW_SyncWindow( "windowed transition" );
		GLW_UpdateWindowState();
	}

/*#ifdef USE_VULKAN_API
	if ( !vulkan )
#endif
	{
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS,
			GLW_ShouldRequestGLxDebugContext() ? SDL_GL_CONTEXT_DEBUG_FLAG : 0 );
	}*/

	if ( !GLW_SetupDrawableContext( config, vulkan, colorBits, depthBits, stencilBits, requestFloatFramebuffer ) ) {
		return qfalse;
	}

	// the window keeps its focus state through the restart and no focus
	// events will arrive to refresh these, so derive them from live flags
	windowFlags = SDL_GetWindowFlags( SDL_window );
	gw_active = ( windowFlags & SDL_WINDOW_INPUT_FOCUS ) ? qtrue : qfalse;
	gw_minimized = ( windowFlags & SDL_WINDOW_MINIMIZED ) ? qtrue : qfalse;

	Com_Printf( "...reusing existing window (%s)\n", fullscreen ? "fullscreen" : "windowed" );

	return qtrue;
}


/*
===============
GLimp_SetMode
===============
*/
static int GLW_SetMode( int mode, const char *modeFS, qboolean fullscreen, qboolean vulkan )
{
	glconfig_t *config = glw_state.config;
	int perChannelColorBits;
	int colorBits, depthBits, stencilBits;
	int i;
	const SDL_DisplayMode *desktopMode;
	SDL_DisplayID display = 0;
	int x = vid_xpos->integer;
	int y = vid_ypos->integer;
	SDL_WindowFlags flags = 0;
	qboolean requestFloatFramebuffer = qfalse;
	qboolean reusedWindow;
	char windowTitle[sizeof(cl_title)+(sizeof(ARCH_STRING)-1)+6] = { 0 };

#ifdef USE_VULKAN_API
	if ( vulkan ) {
		flags |= SDL_WINDOW_VULKAN;
		Com_Printf( "Initializing Vulkan display\n");
	} else
#endif
	{
		flags |= SDL_WINDOW_OPENGL;
		Com_Printf( "Initializing OpenGL display\n");
	}

	// If a window exists, note its display index
	if ( SDL_window != NULL )
	{
		display = SDL_GetDisplayForWindow( SDL_window );
		if ( !display )
		{
			Com_DPrintf( "SDL_GetDisplayForWindow() failed: %s\n", SDL_GetError() );
		}
	}
	else
	{
		// find out to which display our window belongs to
		// according to previously stored \vid_xpos and \vid_ypos coordinates
		display = GLW_ConstrainWindowPosition( NULL, &x, &y, 640, 480 );
	}

/*#ifdef USE_VULKAN_API
	if ( !vulkan )
#endif
	{
		requestFloatFramebuffer = GLW_ShouldRequestFloatFramebuffer( display );
		if ( requestFloatFramebuffer ) {
			Com_Printf( "...requesting floating-point OpenGL framebuffer for HDR output\n" );
		}
	}*/

	desktopMode = display ? SDL_GetDesktopDisplayMode( display ) : NULL;
	if ( desktopMode ) {
		glw_state.desktop_width = desktopMode->w;
		glw_state.desktop_height = desktopMode->h;
	} else {
		glw_state.desktop_width = 640;
		glw_state.desktop_height = 480;
	}

	config->isFullscreen = fullscreen;
	glw_state.isFullscreen = fullscreen;

	if ( fullscreen && *modeFS )
		Com_Printf( "...setting mode %d:", atoi(modeFS) );
	else
		Com_Printf( "...setting mode %d:", mode );

	if ( !CL_GetModeInfo( &config->vidWidth, &config->vidHeight, &config->windowAspect, mode, modeFS, glw_state.desktop_width, glw_state.desktop_height, fullscreen ) )
	{
		Com_Printf( " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}

	Com_Printf( " %d %d\n", config->vidWidth, config->vidHeight );

	colorBits = r_colorbits->value;

	if ( colorBits == 0 || colorBits > 24 )
		colorBits = 24;

	if ( cl_depthbits->integer == 0 )
	{
		// implicitly assume Z-buffer depth == desktop color depth
		if ( colorBits > 16 )
			depthBits = 24;
		else
			depthBits = 16;
	}
	else
		depthBits = cl_depthbits->integer;

	stencilBits = cl_stencilbits->integer;

	// do not allow stencil if Z-buffer depth likely won't contain it
	if ( depthBits < 24 )
		stencilBits = 0;

	Com_sprintf( windowTitle, sizeof(windowTitle), "%s ( %s )", cl_title, ARCH_STRING );

	// Destroy existing state if it exists
	if ( SDL_glContext )
	{
		SDL_GL_DestroyContext(SDL_glContext);
		SDL_glContext = NULL;
	}

	reusedWindow = qfalse;

	if ( SDL_window != NULL )
	{
		//GLW_RestoreGamma();

		// \vid_restart fast keeps the window alive: morph it in place -
		// the usual case for windowed/fullscreen toggles - and only fall
		// back to a full re-creation when that is not possible
		reusedWindow = GLW_ReuseExistingWindow( config, display, fullscreen, vulkan,
			colorBits, depthBits, stencilBits, requestFloatFramebuffer );

		if ( !reusedWindow )
		{
			SDL_GetWindowPosition( SDL_window, &x, &y );
			Com_DPrintf( "Existing window at %dx%d before being destroyed\n", x, y );
			GLW_DestroyWindow();
		}
	}

	if ( !reusedWindow )
	{
		gw_active = qfalse;
		gw_minimized = qtrue;

		if ( !fullscreen && r_noborder->integer )
		{
			flags |= SDL_WINDOW_BORDERLESS;
		}

		flags |= SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
		if ( !fullscreen ) {
			GLW_ConstrainWindowPosition( NULL, &x, &y,
				config->vidWidth, config->vidHeight );
		}
	}

	for ( i = 0; i < 16 && !reusedWindow; i++ )
	{
		int testColorBits, testDepthBits, testStencilBits;
		//int realColorBits[3];
		qboolean testFloatFramebuffer;

		// 0 - default
		// 1 - minus colorBits
		// 2 - minus depthBits
		// 3 - minus stencil
		if ((i % 4) == 0 && i)
		{
			// one pass, reduce
			switch (i / 4)
			{
				case 2 :
					if (colorBits == 24)
						colorBits = 16;
					break;
				case 1 :
					if (depthBits == 24)
						depthBits = 16;
					else if (depthBits == 16)
						depthBits = 8;
				case 3 :
					if (stencilBits == 24)
						stencilBits = 16;
					else if (stencilBits == 16)
						stencilBits = 8;
			}
		}

		testColorBits = colorBits;
		testDepthBits = depthBits;
		testStencilBits = stencilBits;

		if ((i % 4) == 3)
		{ // reduce colorBits
			if (testColorBits == 24)
				testColorBits = 16;
		}

		if ((i % 4) == 2)
		{ // reduce depthBits
			if (testDepthBits == 24)
				testDepthBits = 16;
		}

		if ((i % 4) == 1)
		{ // reduce stencilBits
			if (testStencilBits == 8)
				testStencilBits = 0;
		}

		if ( testColorBits == 24 )
			perChannelColorBits = 8;
		else
			perChannelColorBits = 4;

		testFloatFramebuffer = ( requestFloatFramebuffer && i < 4 ) ? qtrue : qfalse;

#ifdef USE_VULKAN_API
		if ( !vulkan )
#endif
		{
	
#ifdef __sgi /* Fix for SGIs grabbing too many bits of color */
			if (perChannelColorBits == 4)
				perChannelColorBits = 0; /* Use minimum size for 16-bit color */

			/* Need alpha or else SGIs choose 36+ bit RGB mode */
			SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 1 );
#endif

			SDL_GL_SetAttribute( SDL_GL_RED_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, perChannelColorBits );
#ifndef __sgi
			// prefer alpha if available, otherwise prefer smallest available alpha
			SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, perChannelColorBits == 8 ? 8 : 0 );
#endif
			SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, testDepthBits );
			SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, testStencilBits );

			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0 );

			if ( r_stereoEnabled->integer )
			{
				config->stereoEnabled = qtrue;
				SDL_GL_SetAttribute( SDL_GL_STEREO, 1 );
			}
			else
			{
				config->stereoEnabled = qfalse;
				SDL_GL_SetAttribute( SDL_GL_STEREO, 0 );
			}
		
			SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

			if ( !r_allowSoftwareGL->integer )
				SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
		}

		if ( ( SDL_window = GLW_CreateWindow( windowTitle, x, y, config->vidWidth, config->vidHeight, flags ) ) == NULL )
		{
			Com_DPrintf( "SDL_CreateWindowWithProperties failed: %s\n", SDL_GetError() );
			continue;
		}

		if ( !SDL_SetWindowResizable( SDL_window, true ) ) {
			Com_DPrintf( "SDL_SetWindowResizable failed: %s\n", SDL_GetError() );
		}
		if ( !SDL_SetWindowMinimumSize( SDL_window, 320, 240 ) ) {
			Com_DPrintf( "SDL_SetWindowMinimumSize failed: %s\n", SDL_GetError() );
		}

		if ( fullscreen )
		{
			if ( !GLW_ApplyFullscreen( config, display, testColorBits ) ) {
				GLW_DestroyWindow();
				continue;
			}
		}
		else
		{
			GLW_SyncWindow( "resizable window creation" );
			GLW_ConstrainWindowPosition( SDL_window, &x, &y,
				config->vidWidth, config->vidHeight );
			if ( !SDL_SetWindowPosition( SDL_window, x, y ) ) {
				Com_DPrintf( "SDL_SetWindowPosition failed: %s\n", SDL_GetError() );
			}
			GLW_SyncWindow( "window creation placement" );
			GLW_UpdateWindowState();
		}

		if ( !GLW_SetupDrawableContext( config, vulkan, testColorBits, testDepthBits, testStencilBits, testFloatFramebuffer ) )
		{
			GLW_DestroyWindow();
			continue;
		}

		glw_windowRecipe.valid = qtrue;
		glw_windowRecipe.vulkan = vulkan;
		glw_windowRecipe.colorBits = testColorBits;
		glw_windowRecipe.depthBits = testDepthBits;
		glw_windowRecipe.stencilBits = testStencilBits;
		glw_windowRecipe.stereo = config->stereoEnabled;
		glw_windowRecipe.software = ( r_allowSoftwareGL && r_allowSoftwareGL->integer ) ? qtrue : qfalse;
		glw_windowRecipe.floatFramebuffer = testFloatFramebuffer;

		break;
	}

	if ( SDL_window )
	{
#ifdef USE_ICON
		SDL_Surface *icon = SDL_CreateSurfaceFrom(
			CLIENT_WINDOW_ICON.width,
			CLIENT_WINDOW_ICON.height,
			SDL_GetPixelFormatForMasks(
				CLIENT_WINDOW_ICON.bytes_per_pixel * 8,
#ifdef Q3_LITTLE_ENDIAN
				0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
				0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
			),
			(void *)CLIENT_WINDOW_ICON.pixel_data,
			CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width
		);
		if ( icon )
		{
			SDL_SetWindowIcon( SDL_window, icon );
			SDL_DestroySurface( icon );
		}
#endif
	}
	else
	{
		Com_Printf( "Couldn't get a visual\n" );
		return RSERR_INVALID_MODE;
	}

	if ( !fullscreen && r_noborder->integer )
		SDL_SetWindowHitTest( SDL_window, SDL_HitTestFunc, NULL );

	if ( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_HIDDEN )
	{
		if ( !SDL_ShowWindow( SDL_window ) ) {
			Com_DPrintf( "SDL_ShowWindow failed: %s\n", SDL_GetError() );
		}
		if ( !SDL_RaiseWindow( SDL_window ) ) {
			Com_DPrintf( "SDL_RaiseWindow failed: %s\n", SDL_GetError() );
		}
		GLW_SyncWindow( "window show" );
	}

	if ( !fullscreen ) {
		GLW_EnsureWindowOnScreen();
	}

	GLW_UpdateWindowState();

	if ( !SDL_GetWindowSizeInPixels( SDL_window, &config->vidWidth, &config->vidHeight ) )
	{
		Com_DPrintf( "SDL_GetWindowSizeInPixels failed: %s\n", SDL_GetError() );
		config->vidWidth = glw_state.window_width;
		config->vidHeight = glw_state.window_height;
	}

	SDL_WarpMouseInWindow( SDL_window, glw_state.window_width / 2, glw_state.window_height / 2 );

	return RSERR_OK;
}


/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static rserr_t GLimp_StartDriverAndSetMode( int mode, const char *modeFS, qboolean fullscreen, qboolean vulkan )
{
	rserr_t err;

	if ( fullscreen && in_nograb->integer )
	{
		Com_Printf( "Fullscreen not allowed with \\in_nograb 1\n");
		Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}

	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
	{
		const char *driverName;

		if ( r_sdlDriver->string[0] != '\0' )
		{
			SDL_SetHint( SDL_HINT_VIDEO_DRIVER, r_sdlDriver->string );
		}

		if ( r_allowScreenSaver->integer )
		{
			SDL_SetHint( SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1" );
		}

		/*
			Starting from SDL2 2.0.14 The default value for SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS 
			is now false for better compatibility with modern window managers, however it 
			prevented the game from alt-tab/minimize, set to 1 before calling SDL_Init fix it.
		*/
		SDL_SetHint( SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1" );

		if ( !SDL_Init( SDL_INIT_VIDEO ) )
		{
			Com_Printf( "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError() );
			return RSERR_FATAL_ERROR;
		}

		driverName = SDL_GetCurrentVideoDriver();

		Com_Printf( "SDL using driver \"%s\"\n", driverName );
	}

	err = GLW_SetMode( mode, modeFS, fullscreen, vulkan );

	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			Com_Printf( "...WARNING: fullscreen unavailable in this mode\n" );
			return err;
		case RSERR_INVALID_MODE:
			Com_Printf( "...WARNING: could not set the given mode (%d)\n", mode );
			return err;
		default:
			break;
	}

	return RSERR_OK;
}


/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init( glconfig_t *config )
{
	rserr_t err;

	// REF_KEEP_WINDOW re-enters platform initialization without calling
	// GLimp_Shutdown. Tear down window-bound input first so text input,
	// controllers, and commands are rebound exactly once to the retained (or
	// replacement fallback) window.
	if ( SDL_window ) {
		IN_Shutdown();
	}

#ifndef _WIN32
	InitSig();
#endif

	Com_DPrintf( "GLimp_Init()\n" );

	glw_state.config = config; // feedback renderer configuration

	in_nograb = Cvar_Get( "in_nograb", "0", 0 );
	Cvar_SetDescription( in_nograb, "Do not capture mouse in game, may be useful during online streaming" );

	r_allowSoftwareGL = Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
	Cvar_SetDescription( r_allowSoftwareGL, "Toggle the use of the default software OpenGL driver supplied by the Operating System" );

	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( r_swapInterval, "V-blanks to wait before swapping buffers\n 0: No V-Sync\n 1: Synced to the monitor's refresh rate" );
	r_stereoEnabled = Cvar_Get( "r_stereoEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( r_stereoEnabled, "Enable stereo rendering for techniques like shutter glasses" );

	// Create the window and set up the context
	err = GLimp_StartDriverAndSetMode( r_mode->integer, r_modeFullscreen->string, r_fullscreen->integer, qfalse );
	if ( err != RSERR_OK )
	{
		if ( err == RSERR_FATAL_ERROR )
		{
			Com_Error( ERR_VID_FATAL, "GLimp_Init() - could not load OpenGL subsystem" );
			return;
		}

		if ( r_mode->integer != 3 || ( r_fullscreen->integer && atoi( r_modeFullscreen->string ) != 3 ) )
		{
			Com_Printf( "Setting \\r_mode %d failed, falling back on \\r_mode %d\n", r_mode->integer, 3 );
			if ( GLimp_StartDriverAndSetMode( 3, "", r_fullscreen->integer, qfalse ) != RSERR_OK )
			{
				// Nothing worked, give up
				Com_Error( ERR_VID_FATAL, "GLimp_Init() - could not load OpenGL subsystem" );
				return;
			}
		}
	}

	// These values force the UI to disable driver selection
	config->driverType = GLDRV_ICD;
	config->hardwareType = GLHW_GENERIC;

	if ( Sys_IsSteamOverlayAttached() ) {
		Com_Printf( S_COLOR_CYAN "Steam Overlay Detected\n" );
	}
	else {
		Com_Printf( S_COLOR_WHITE "Steam Overlay Not Detected\n" );
	}

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init();

	HandleEvents();

	Key_ClearStates();
}


/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame( void )
{
	// don't flip if drawing to front buffer
	if ( Q_stricmp( cl_drawBuffer->string, "GL_FRONT" ) != 0 )
	{
		SDL_GL_SwapWindow( SDL_window );
	}
}


/*
===============
GL_GetProcAddress

Used by opengl renderers to resolve all qgl* function pointers
===============
*/
void *GL_GetProcAddress( const char *symbol )
{
	return (void *)SDL_GL_GetProcAddress( symbol );
}


#ifdef USE_VULKAN_API
/*
===============
VKimp_Init

This routine is responsible for initializing the OS specific portions
of Vulkan
===============
*/
void VKimp_Init( glconfig_t *config )
{
	rserr_t err;

	// REF_KEEP_WINDOW re-enters platform initialization without calling
	// GLimp_Shutdown. Tear down window-bound input first so text input,
	// controllers, and commands are rebound exactly once to the retained (or
	// replacement fallback) window.
	if ( SDL_window ) {
		IN_Shutdown();
	}

#ifndef _WIN32
	InitSig();
#endif

	Com_DPrintf( "VKimp_Init()\n" );

	in_nograb = Cvar_Get( "in_nograb", "0", 0 );
	Cvar_SetDescription( in_nograb, "Do not capture mouse in game, may be useful during online streaming" );

	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_stereoEnabled = Cvar_Get( "r_stereoEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( r_stereoEnabled, "Enable stereo rendering for techniques like shutter glasses" );

	// feedback to renderer configuration
	glw_state.config = config;

	// Create the window and set up the context
	err = GLimp_StartDriverAndSetMode( r_mode->integer, r_modeFullscreen->string, r_fullscreen->integer, qtrue /* Vulkan */ );
	if ( err != RSERR_OK )
	{
		if ( err == RSERR_FATAL_ERROR )
		{
			Com_Error( ERR_VID_FATAL, "VKimp_Init() - could not load Vulkan subsystem" );
			return;
		}

		Com_Printf( "Setting r_mode %d failed, falling back on r_mode %d\n", r_mode->integer, 3 );

		err = GLimp_StartDriverAndSetMode( 3, "", r_fullscreen->integer, qtrue /* Vulkan */ );
		if( err != RSERR_OK )
		{
			// Nothing worked, give up
			Com_Error( ERR_VID_FATAL, "VKimp_Init() - could not load Vulkan subsystem" );
			return;
		}
	}

	qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();

	if ( qvkGetInstanceProcAddr == NULL )
	{
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
		Com_Error( ERR_VID_FATAL, "VKimp_Init: qvkGetInstanceProcAddr is NULL" );
	}

	// These values force the UI to disable driver selection
	config->driverType = GLDRV_ICD;
	config->hardwareType = GLHW_GENERIC;

	if ( Sys_IsSteamOverlayAttached() ) {
		Com_Printf( S_COLOR_CYAN "Steam Overlay Detected\n" );
	}
	else {
		Com_Printf( S_COLOR_WHITE "Steam Overlay Not Detected\n" );
	}

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init();

	HandleEvents();

	Key_ClearStates();
}


/*
===============
VK_GetInstanceProcAddr
===============
*/
void *VK_GetInstanceProcAddr( VkInstance instance, const char *name )
{
	return qvkGetInstanceProcAddr( instance, name );
}


/*
===============
VK_CreateSurface
===============
*/
qboolean VK_CreateSurface( VkInstance instance, VkSurfaceKHR *surface )
{
	if ( SDL_Vulkan_CreateSurface( SDL_window, instance, NULL, surface ) )
		return qtrue;
	else
		return qfalse;
}


/*
===============
VKimp_Shutdown
===============
*/
void VKimp_Shutdown( qboolean unloadDLL )
{
	const char* drv = SDL_GetCurrentVideoDriver();

	IN_Shutdown();

	if ( glw_state.isFullscreen ) {
		if ( drv && strcmp( drv, "x11" ) == 0 ) {
			SDL_WarpMouseGlobal( glw_state.desktop_width / 2, glw_state.desktop_height / 2 );
		} else {
			GLW_ShowCursor( qtrue );
		}
	}

	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if ( unloadDLL )
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
}
#endif // USE_VULKAN_API


/*
================
GLW_HideFullscreenWindow
================
*/
void GLW_HideFullscreenWindow( void ) {
	if ( SDL_window && glw_state.isFullscreen ) {
		SDL_HideWindow( SDL_window );
	}
}


/*
===============
Sys_GetClipboardData
===============
*/
char *Sys_GetClipboardText( void )
{
#ifdef DEDICATED
	return NULL;
#else
	return SDL_GetClipboardText();
#endif
}

void Sys_FreeClipboardText( char *data ) {
	SDL_free( data );
}


void Sys_SetClipboardText( const char *text )
{
#ifndef DEDICATED
	SDL_SetClipboardText( text );
#endif
}

#ifndef DEDICATED
// Structure to hold the copied bitmap in memory
typedef struct {
    void *data;
    size_t length;
} ClipboardData;

// 1. Callback to provide data to the OS when another app pastes
static const void * SDLCALL ProvideClipboardData(void *userdata, const char *mime_type, size_t *size) {
    ClipboardData *ctx = (ClipboardData *)userdata;
    
    // Check if the requested format is our advertised BMP format
    if (ctx && SDL_strcmp(mime_type, "image/bmp") == 0) {
        *size = ctx->length;
        return ctx->data;
    }
    
    *size = 0;
    return NULL;
}

// 2. Callback to free the memory when the clipboard is cleared or overwritten
static void SDLCALL CleanupClipboardData(void *userdata) {
    ClipboardData *ctx = (ClipboardData *)userdata;
    if (ctx) {
        if (ctx->data) {
            SDL_free(ctx->data);
        }
        SDL_free(ctx);
    }
}
#endif

/*
===============
Sys_SetClipboardBitmap

todo use SDL_SetClipboardData
===============
*/
void Sys_SetClipboardBitmap( const byte *bitmap, int length )
{
#ifndef DEDICATED
	if (!bitmap || length <= 0) {
        return;
    }

    // Allocate our tracking structure
    ClipboardData *ctx = (ClipboardData *)SDL_malloc(sizeof(ClipboardData));
    if (!ctx) return;

    // Allocate and copy the actual bitmap data so it lives beyond this function
    ctx->length = (size_t)length;
    ctx->data = SDL_malloc(ctx->length);
    if (!ctx->data) {
        SDL_free(ctx);
        return;
    }
    SDL_memcpy(ctx->data, bitmap, ctx->length);

    // SDL3 identifies clipboard types via standard MIME types
    const char *mime_types[] = { "image/bmp" };

    // Offer the data to the OS via SDL3
    if (!SDL_SetClipboardData(ProvideClipboardData, CleanupClipboardData, ctx, mime_types, 1)) {
        // If initialization fails, clean up the allocated memory immediately
        CleanupClipboardData(ctx);
    }
#endif
}

int GLimp_NormalFontBase( void ) {
    return 0;
}
