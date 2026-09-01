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

#ifndef __CL_KEYS_H__
#define __CL_KEYS_H__

#include "keycodes.h"

typedef struct {
	qboolean	down;
	qboolean	bound;
	int			repeats;		// if > 1, it is autorepeating
	char		*binding;
} qkey_t;

extern	qboolean	key_overstrikeMode;
extern	qkey_t		keys[MAX_KEYS];

extern  int         anykeydown;

// NOTE TTimo the declaration of field_t and Field_Clear is now in qcommon/qcommon.h

void Key_WriteBindings( fileHandle_t f );
void Key_SetBinding( int keynum, const char *binding );
void Key_GetBindingBuf( int keynum, char *buf, int buflen );
void Key_GetBindingByString( const char* binding, int* key1, int* key2 );
const char *Key_GetBinding( int keynum );
void Key_ParseBinding( int key, qboolean down, unsigned int time );

int Key_GetKey( const char *binding );
const char *Key_KeynumToString( int keynum, qboolean bTranslate );
void Key_KeynumToStringBuf( int keynum, char *buf, int buflen );
int Key_StringToKeynum( const char *str );

qboolean Key_IsDown( int keynum );
void Key_ClearStates( void );

void Key_PreserveModifiers( void );
void Key_RestoreModifiers( void );

qboolean Key_GetOverstrikeMode( void );
void Key_SetOverstrikeMode( qboolean state );

void Com_InitKeyCommands( void );
#endif
