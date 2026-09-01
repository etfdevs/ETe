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

//static Uint16 r[256];
//static Uint16 g[256];
//static Uint16 b[256];

//static Uint16 *window_gamma;
///static Uint16 *window_saved_gamma;

//bool X11_GetWindowGammaRamp( )
/*bool GetWindowGammaRamp( SDL_Window *window, Uint16 *red, Uint16 *green, Uint16 *blue) {
	if ( !SDL_window ) {
		return false;
	}

	if ( !window_gamma ) {
		window_gamma = (Uint16 *)SDL_malloc( 256 * 6 * sizeof(Uint16) );
		if( !window_gamma ) {
			//SDL_SetError()
			return false;
		}
		window_saved_gamma = window_gamma + 3*256;

		if ( 0 ) { // windows
		}
		else {
			int i;
			// Create an identity gamma ramp
            for (i = 0; i < 256; ++i) {
                Uint16 value = (Uint16)((i << 8) | i);

                window_gamma[0*256+i] = value;
                window_gamma[1*256+i] = value;
                window_gamma[2*256+i] = value;
            }
		}
		SDL_memcpy(window_saved_gamma, window_gamma, 3*256*sizeof(Uint16));
	}

	if (red) {
        SDL_memcpy(red, &window_gamma[0*256], 256*sizeof(Uint16));
    }
    if (green) {
        SDL_memcpy(green, &window_gamma[1*256], 256*sizeof(Uint16));
    }
    if (blue) {
        SDL_memcpy(blue, &window_gamma[2*256], 256*sizeof(Uint16));
    }

	SDL_GetPointerProperty( SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
}*/

void GLimp_InitGamma( glconfig_t *config )
{
#if 0
	config->deviceSupportsGamma = qfalse;

	if ( SDL_GetWindowGammaRamp( SDL_window, r, g, b ) == 0 )
	{
		config->deviceSupportsGamma = SDL_SetWindowBrightness( SDL_window, 1.0f ) >= 0 ? qtrue : qfalse;
	}
#endif
	config->deviceSupportsGamma = qfalse;
}


/*
=================
GLimp_SetGamma
=================
*/
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
#if 0
	Uint16 table[3][256];
	int i, j;

	for ( i = 0; i < 256; i++ )
	{
		table[0][i] = ( ( ( Uint16 ) red[i] ) << 8 ) | red[i];
		table[1][i] = ( ( ( Uint16 ) green[i] ) << 8 ) | green[i];
		table[2][i] = ( ( ( Uint16 ) blue[i] ) << 8 ) | blue[i];
	}

#ifdef _WIN32
#include <windows.h>

	// Win2K and newer put this odd restriction on gamma ramps...
	{
		//OSVERSIONINFO	vinfo;
		//vinfo.dwOSVersionInfoSize = sizeof( vinfo );
		//GetVersionEx( &vinfo );
		//if( vinfo.dwMajorVersion >= 5 && vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
		{
			qboolean clamped = qfalse;
			for( j = 0 ; j < 3 ; j++ )
			{
				for( i = 0 ; i < 128 ; i++ )
				{
					if( table[ j ] [ i] > ( ( 128 + i ) << 8 ) )
					{
						table[ j ][ i ] = ( 128 + i ) << 8;
						clamped = qtrue;
					}
				}

				if( table[ j ] [127 ] > 254 << 8 )
				{
					table[ j ][ 127 ] = 254 << 8;
					clamped = qtrue;
				}
			}
			if ( clamped )
			{
				Com_DPrintf( "performing gamma clamp.\n" );
			}
		}
	}
#endif

	// enforce constantly increasing
	for ( j = 0; j < 3; j++ )
	{
		for (i = 1; i < 256; i++)
		{
			if (table[j][i] < table[j][i-1])
				table[j][i] = table[j][i-1];
		}
	}

	if ( SDL_SetWindowGammaRamp( SDL_window, table[0], table[1], table[2] ) < 0 )
	{
		Com_DPrintf( "SDL_SetWindowGammaRamp() failed: %s\n", SDL_GetError() );
	}
#endif
}


/*
** GLW_RestoreGamma
*/
void GLW_RestoreGamma( void )
{
	// automatically handled by SDL?
}
