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

#ifndef __Q_SHARED_H__
#define __Q_SHARED_H__

// q_shared.h -- included first by ALL program modules.
// A user mod should never modify this file

#define Q3_VERSION      "ET 2.60e"

#define ETe_VERSION		"ET 2.60e"

#ifndef GIT_VERSION
#define GIT_VERSION Q3_VERSION
#endif
#define CLIENT_WINDOW_TITLE   "Enemy Territory"
#define CLIENT_WINDOW_CLASS   CLIENT_WINDOW_TITLE
#ifdef _WIN32
#define CONSOLE_WINDOW_CLASS  "ET WinConsole"
#endif
#define CONSOLE_WINDOW_TITLE  "ET Console"

#define STEAMPATH_NAME			"Wolfenstein Enemy Territory"
#define STEAMPATH_APPID			"1873030"

// 2.60d: Mac OSX universal binaries
// 2.60c: Mac OSX universal binaries
// 2.60b: CVE-2006-2082 fix
// 2.6x: Enemy Territory - ETPro team maintenance release
// 2.5x: Enemy Territory FINAL
// 2.4x: Enemy Territory RC's
// 2.3x: Enemy Territory TEST
// 2.2+: post SP removal
// 2.1+: post Enemy Territory moved standalone
// 2.x: post Enemy Territory
// 1.x: pre Enemy Territory
////
// 1.3-MP : final for release
// 1.1b - TTimo SP linux release (+ MP updates)
// 1.1b5 - Mac update merge in

#define CONFIG_NAME     "etconfig.cfg"

//#define LOCALIZATION_SUPPORT

#define NEW_ANIMS
#define MAX_TEAMNAME    32
#define MAX_MASTER_SERVERS      5	// number of supported master servers

#define GAMENAME_FOR_MASTER		"ET"
#define HEARTBEAT_FOR_MASTER	"EnemyTerritory-1"

#define DEMOEXT	"dm_"			// standard demo extension

#ifdef _MSC_VER

#pragma warning(disable : 4018)		// signed/unsigned mismatch
//#pragma warning(disable : 4032)
//#pragma warning(disable : 4051)
#pragma warning(disable : 4057)		// slightly different base types
#pragma warning(disable : 4100)		// unreferenced formal parameter
//#pragma warning(disable : 4115)
#pragma warning(disable : 4125)		// decimal digit terminates octal escape sequence
#pragma warning(disable : 4127)		// conditional expression is constant
//#pragma warning(disable : 4136)
#pragma warning(disable	: 4152)		// nonstandard extension, function/data pointer conversion in expression
#pragma warning(disable : 4200)		// nonstandard extension used: size-sided array in struct/union
//#pragma warning(disable : 4201)		// nonstandard extension used : nameless struct/union
//#pragma warning(disable : 4206)		// nonstandard extension used: translation unit is empty
//#pragma warning(disable : 4214)
#pragma warning(disable : 4267)		// conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable : 4244)
#pragma warning(disable : 4142)		// benign redefinition
#pragma warning(disable : 4305)		// truncation from const double to float
//#pragma warning(disable : 4310)	// cast truncates constant value
//#pragma warning(disable :	4505)	// unreferenced local function has been removed
//#pragma warning(disable : 4514)
#pragma warning(disable : 4702)		// unreachable code
#pragma warning(disable : 4711)		// selected for automatic inline expansion
#pragma warning(disable : 4220)		// varargs matches remaining parameters
#pragma warning(disable : 4324)		// 'q_jpeg_error_mgr_s' : structure was padded due to alignment specifier
#pragma warning(disable : 4091)		// 'typedef': ignored on lef of <..> when no variable is declared
//#pragma intrinsic( memset, memcpy )
#endif

// This warning is broken on GCC 11, which may be used on GHA still
#if !defined(__clang__) && (__GNUC__ == 11)
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

//Ignore __attribute__ on non-gcc/clang platforms
#if !defined(__GNUC__) && !defined(__clang__)
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#define UNUSED_VAR __attribute__((unused))
#else
#define UNUSED_VAR
#endif

#if (defined _MSC_VER)
#define Q_EXPORT __declspec(dllexport)
#elif (defined __SUNPRO_C)
#define Q_EXPORT __global
#elif ((__GNUC__ >= 3) && (!__EMX__) && (!sun))
#define Q_EXPORT __attribute__((visibility("default")))
#elif (defined(__clang__))
#define Q_EXPORT __attribute__((visibility("default")))
#else
#define Q_EXPORT
#endif

#if defined(__GNUC__) || defined(__clang__)
#define NORETURN __attribute__((noreturn))
#define NORETURN_PTR __attribute__((noreturn))
#elif defined(_MSC_VER)
#define NORETURN __declspec(noreturn)
// __declspec doesn't work on function pointers
#define NORETURN_PTR /* nothing */
#else
#define NORETURN /* nothing */
#define NORETURN_PTR /* nothing */
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FORMAT_PRINTF(x, y) __attribute__((format (printf, x, y)))
#else
#define FORMAT_PRINTF(x, y) /* nothing */
#endif

/**********************************************************************
  VM Considerations

  The VM can not use the standard system headers because we aren't really
  using the compiler they were meant for.  We use bg_lib.h which contains
  prototypes for the functions we define for our own use in bg_lib.c.

  When writing mods, please add needed headers HERE, do not start including
  stuff like <stdio.h> in the various .c files that make up each of the VMs
  since you will be including system headers files can will have issues.

  Remember, if you use a C library function that is not defined in bg_lib.c,
  you will have to add your own version for support in the VM.

 **********************************************************************/

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h> // rain
#include <float.h>

//endianness
short ShortSwap( short l );
int LongSwap( int l );
float FloatSwap( const float *f );

#include "q_platform.h"

//=============================================================

#if defined (_MSC_VER) && !defined(__clang__)
	#if _MSC_VER >= 1600
		#if !defined(__STDC_LIMIT_MACROS)
			#define __STDC_LIMIT_MACROS
		#endif
		#include <stdint.h>
	#else
		#include <io.h>
		typedef __int64 int64_t;
		typedef __int32 int32_t;
		typedef __int16 int16_t;
		typedef __int8 int8_t;
		typedef unsigned __int64 uint64_t;
		typedef unsigned __int32 uint32_t;
		typedef unsigned __int16 uint16_t;
		typedef unsigned __int8 uint8_t;
	#endif
#else
	#if !defined(__STDC_LIMIT_MACROS)
		#define __STDC_LIMIT_MACROS
	#endif
	#include <stdint.h>
#endif

#ifdef _WIN32
	// vsnprintf is ISO/IEC 9899:1999
	// abstracting this to make it portable
	int Q_vsnprintf( char *str, size_t size, const char *format, va_list ap );
#else
	#define Q_vsnprintf vsnprintf
#endif

int64_t Long64Swap(int64_t ll); // needs int64_t from above

#if defined (_WIN32) && !defined(_MSC_VER)
#define Q_setjmp __builtin_setjmp
#define Q_longjmp __builtin_longjmp
#else
#define Q_setjmp setjmp
#define Q_longjmp longjmp
#endif

typedef unsigned char byte;

typedef enum { qfalse = 0, qtrue = 1 } qboolean;

typedef union floatint_u
{
	int32_t i;
	uint32_t u;
	float f;
	byte b[sizeof(int32_t)];
}
floatint_t;

#if defined(USE_VULKAN)
typedef union {
	byte rgba[4];
	uint32_t u32;
} color4ub_t;
#endif

typedef int		qhandle_t;
typedef int		sfxHandle_t;
typedef int		fileHandle_t;
typedef int		clipHandle_t;

#define PAD(base, alignment)	(((base)+(alignment)-1) & ~((alignment)-1))
#define PADLEN(base, alignment)	(PAD((base), (alignment)) - (base))

#define PADP(base, alignment)	((void *) PAD((intptr_t) (base), (alignment)))

#if defined(__GNUC__) || defined(__clang__)
#define QALIGN(x) __attribute__((aligned(x)))
#elif defined(_MSC_VER)
#define QALIGN(x) /* __declspec(align(x)) */
#else
#define QALIGN(x)
#endif

//#define	SND_NORMAL			0x000	// (default) Allow sound to be cut off only by the same sound on this channel
#define     SND_OKTOCUT         0x001   // Allow sound to be cut off by any following sounds on this channel
#define     SND_REQUESTCUT      0x002   // Allow sound to be cut off by following sounds on this channel only for sounds who request cutoff
#define     SND_CUTOFF          0x004   // Cut off sounds on this channel that are marked 'SND_REQUESTCUT'
#define     SND_CUTOFF_ALL      0x008   // Cut off all sounds on this channel
#define     SND_NOCUT           0x010   // Don't cut off.  Always let finish (overridden by SND_CUTOFF_ALL)
#define     SND_NO_ATTENUATION  0x020   // don't attenuate (even though the sound is in voice channel, for example)

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef BIT
#define BIT( num ) ( 1ULL << ( num ) )
#endif

#define	MAX_QINT			0x7fffffff
#define	MIN_QINT			(-MAX_QINT-1)

#define	MAX_UINT			((unsigned)(~0))

#define ARRAY_LEN(x)		(sizeof(x) / sizeof(*(x)))
#define STRARRAY_LEN(x)		(ARRAY_LEN(x) - 1)

// We need to use EXPAND because the Microsoft MSVC preprocessor does not expand the va_args the same way as other preprocessors
// http://stackoverflow.com/questions/5134523/msvc-doesnt-expand-va-args-correctly
#define EXPAND(x) x

#define VM_CALL_END (intptr_t)(-1337)
#define GET_SYSCALL_MACRO(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, NAME, ...) NAME

#define SystemCall_0(arg) syscall(arg, VM_CALL_END)
#define SystemCall_1(arg, a1) syscall(arg, (intptr_t)(a1), VM_CALL_END)
#define SystemCall_2(arg, a1, a2) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), VM_CALL_END)
#define SystemCall_3(arg, a1, a2, a3) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), VM_CALL_END)
#define SystemCall_4(arg, a1, a2, a3, a4) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), VM_CALL_END)
#define SystemCall_5(arg, a1, a2, a3, a4, a5) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), VM_CALL_END)
#define SystemCall_6(arg, a1, a2, a3, a4, a5, a6) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), VM_CALL_END)
#define SystemCall_7(arg, a1, a2, a3, a4, a5, a6, a7) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), VM_CALL_END)
#define SystemCall_8(arg, a1, a2, a3, a4, a5, a6, a7, a8) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), VM_CALL_END)
#define SystemCall_9(arg, a1, a2, a3, a4, a5, a6, a7, a8, a9) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), VM_CALL_END)
#define SystemCall_10(arg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), VM_CALL_END)
#define SystemCall_11(arg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), VM_CALL_END)
#define SystemCall_12(arg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), (intptr_t)(a12), VM_CALL_END)
#define SystemCall_13(arg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), (intptr_t)(a12), (intptr_t)(a13), VM_CALL_END)
#define SystemCall_14(arg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14) syscall(arg, (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), (intptr_t)(a12), (intptr_t)(a13), (intptr_t)(a14), VM_CALL_END)

#define SystemCall(...) EXPAND(GET_SYSCALL_MACRO(__VA_ARGS__, SystemCall_14, SystemCall_13, SystemCall_12, SystemCall_11, SystemCall_10, SystemCall_9, SystemCall_8, SystemCall_7, SystemCall_6, SystemCall_5, SystemCall_4, SystemCall_3, SystemCall_2, SystemCall_1, SystemCall_0)(__VA_ARGS__))

// angle indexes
#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

// RF, this is just here so different elements of the engine can be aware of this setting as it changes
#define MAX_SP_CLIENTS      64      // increasing this will increase memory usage significantly

// the game guarantees that no string from the network will ever
// exceed MAX_STRING_CHARS
#define MAX_STRING_CHARS    1024    // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS   1024     // max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS     1024    // max length of an individual token

#define	MAX_INFO_STRING		1024
#define	MAX_INFO_KEY		1024
#define	MAX_INFO_VALUE		1024

#define MAX_USERINFO_LENGTH (MAX_INFO_STRING-13) // incl. length of 'connect ""' or 'userinfo ""' and reserving one byte to avoid q3msgboom
													
#define	BIG_INFO_STRING		8192  // used for system info key only
#define	BIG_INFO_KEY		  8192
#define	BIG_INFO_VALUE		8192


#define	MAX_QPATH			64		// max length of a quake game pathname
#ifdef PATH_MAX
#define MAX_OSPATH			PATH_MAX
#else
#define	MAX_OSPATH			256		// max length of a filesystem pathname
#endif

// rain - increased to 36 to match MAX_NETNAME, fixes #13 - UI stuff breaks
// with very long names
#define MAX_NAME_LENGTH     36      // max length of a client name

#define MAX_GUID_LENGTH     33

#define MAX_SAY_TEXT        150

#define MAX_BINARY_MESSAGE  32768   // max length of binary message

typedef enum {
	MESSAGE_EMPTY = 0,
	MESSAGE_WAITING,        // rate/packet limited
	MESSAGE_WAITING_OVERFLOW,   // packet too large with message
} messageStatus_t;

// parameters for command buffer stuffing
typedef enum {
	EXEC_NOW,			// don't return until completed, a VM should NEVER use this,
						// because some commands might cause the VM to be unloaded...
	EXEC_INSERT,		// insert at current position, but don't run yet
	EXEC_APPEND			// add to end of the command buffer (normal case)
} cbufExec_t;


//
// these aren't needed by any of the VMs.  put in another header?
//
#define	MAX_MAP_AREA_BYTES		32		// bit vector of area visibility


// print levels from renderer (FIXME: set up for game / cgame?)
typedef enum {
	PRINT_ALL,
	PRINT_DEVELOPER,		// only print when "developer 1"
	PRINT_WARNING,
	PRINT_ERROR
} printParm_t;

#ifdef  ERR_FATAL
#undef  ERR_FATAL               // this is be defined in malloc.h
#endif

// parameters to the main Error routine
typedef enum {
	ERR_FATAL = 0,              // exit the entire game with a popup window
	ERR_VID_FATAL,              // exit the entire game with a popup window and doesn't delete profile.pid
	ERR_DROP,                   // print to console and disconnect from game
	ERR_SERVERDISCONNECT,       // don't kill server
	ERR_DISCONNECT,             // client disconnected from the server
	ERR_NEED_CD,                // pop up the need-cd dialog
	ERR_AUTOUPDATE
} errorParm_t;


// font rendering values used by ui and cgame

#define PROP_GAP_WIDTH          3
#define PROP_SPACE_WIDTH        8
#define PROP_HEIGHT             27
#define PROP_SMALL_SIZE_SCALE   0.75

#define BLINK_DIVISOR           200
#define PULSE_DIVISOR           75

#define UI_LEFT         0x00000000  // default
#define UI_CENTER       0x00000001
#define UI_RIGHT        0x00000002
#define UI_FORMATMASK   0x00000007
#define UI_SMALLFONT    0x00000010
#define UI_BIGFONT      0x00000020  // default
#define UI_GIANTFONT    0x00000040
#define UI_DROPSHADOW   0x00000800
#define UI_BLINK        0x00001000
#define UI_INVERSE      0x00002000
#define UI_PULSE        0x00004000
// JOSEPH 10-24-99
#define UI_MENULEFT     0x00008000
#define UI_MENURIGHT    0x00010000
#define UI_EXSMALLFONT  0x00020000
#define UI_MENUFULL     0x00080000
// END JOSEPH

#define UI_SMALLFONT75  0x00100000

#if defined( _DEBUG ) && !defined( BSPC )
	#define HUNK_DEBUG
#endif

typedef enum {
	h_high,
	h_low,
	h_dontcare
} ha_pref;

#ifdef HUNK_DEBUG
#define Hunk_Alloc( size, preference )              Hunk_AllocDebug( size, preference, # size, __FILE__, __LINE__ )
void *Hunk_AllocDebug( int size, ha_pref preference, char *label, char *file, int line );
#else
void *Hunk_Alloc( int size, ha_pref preference );
#endif

#if defined(__GNUC__) && !defined(__MINGW32__) && !defined(__APPLE__)
// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=371
// custom Snd_Memset implementation for glibc memset bug workaround
void Snd_Memset (void* dest, const int val, const size_t count);
#else
#define Snd_Memset Com_Memset
#endif

#define Com_Memset memset
#define Com_Memcpy memcpy

#define CIN_system  1
#define CIN_loop    2
#define CIN_hold    4
#define CIN_silent  8
#define CIN_shader  16


/*
==============================================================

MATHLIB

==============================================================
*/


typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef vec_t quat_t[4];

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_TAU
#define M_TAU		6.28318530717958647693f
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846f	// matches value in gcc v2 math.h
#endif

#ifndef M_LN2
#define M_LN2      0.693147180559945309417
#endif

#ifdef __linux__
#ifdef __GLIBC__
#if idx64
// force version for better runtime compatibility
__asm__(".symver logf,logf@GLIBC_2.2.5");
//__asm__(".symver powf,powf@GLIBC_2.2.5");
__asm__(".symver expf,expf@GLIBC_2.2.5");
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");
#endif
#endif
#endif

#define NUMVERTEXNORMALS	162
extern	vec3_t	bytedirs[NUMVERTEXNORMALS];

// all drawing is done to a 640*480 virtual screen size
// and will be automatically scaled to the real resolution
#define SCREEN_WIDTH        640
#define SCREEN_HEIGHT       480

#define TINYCHAR_WIDTH      ( SMALLCHAR_WIDTH )
#define TINYCHAR_HEIGHT     ( SMALLCHAR_HEIGHT )

#define MINICHAR_WIDTH      8
#define MINICHAR_HEIGHT     12

#define SMALLCHAR_WIDTH     8
#define SMALLCHAR_HEIGHT    16

#define BIGCHAR_WIDTH       16
#define BIGCHAR_HEIGHT      16

#define GIANTCHAR_WIDTH     32
#define GIANTCHAR_HEIGHT    48

extern vec4_t colorBlack;
extern vec4_t colorRed;
extern vec4_t colorGreen;
extern vec4_t colorBlue;
extern vec4_t colorYellow;
extern vec4_t colorOrange;
extern vec4_t colorMagenta;
extern vec4_t colorCyan;
extern vec4_t colorWhite;
extern vec4_t colorLtGrey;
extern vec4_t colorMdGrey;
extern vec4_t colorDkGrey;
extern vec4_t colorMdRed;
extern vec4_t colorMdGreen;
extern vec4_t colorDkGreen;
extern vec4_t colorMdCyan;
extern vec4_t colorMdYellow;
extern vec4_t colorMdOrange;
extern vec4_t colorMdBlue;

extern vec4_t clrBrown;
extern vec4_t clrBrownDk;
extern vec4_t clrBrownLine;
extern vec4_t clrBrownText;
extern vec4_t clrBrownTextDk;
extern vec4_t clrBrownTextDk2;
extern vec4_t clrBrownTextLt;
extern vec4_t clrBrownTextLt2;
extern vec4_t clrBrownLineFull;

#define GAME_INIT_FRAMES    6
#define FRAMETIME           100                 // msec

#define Q_COLOR_ESCAPE  '^'
#define Q_IsColorString( p )  ( (p) && *( p ) == Q_COLOR_ESCAPE && *( ( p ) + 1 ) && *( ( p ) + 1 ) != Q_COLOR_ESCAPE )

#define COLOR_BLACK     '0'
#define COLOR_RED       '1'
#define COLOR_GREEN     '2'
#define COLOR_YELLOW    '3'
#define COLOR_BLUE      '4'
#define COLOR_CYAN      '5'
#define COLOR_MAGENTA   '6'
#define COLOR_WHITE     '7'
#define COLOR_ORANGE    '8'
#define COLOR_MDGREY    '9'
#define COLOR_LTGREY    ':'
//#define COLOR_LTGREY	';'
#define COLOR_MDGREEN   '<'
#define COLOR_MDYELLOW  '='
#define COLOR_MDBLUE    '>'
#define COLOR_MDRED     '?'
#define COLOR_LTORANGE  'A'
#define COLOR_MDCYAN    'B'
#define COLOR_MDPURPLE  'C'
#define COLOR_NULL      '*'


#define COLOR_BITS  31
#define ColorIndex( c )   ( ( ( c ) - '0' ) & COLOR_BITS )

#define S_COLOR_BLACK       "^0"
#define S_COLOR_RED         "^1"
#define S_COLOR_GREEN       "^2"
#define S_COLOR_YELLOW      "^3"
#define S_COLOR_BLUE        "^4"
#define S_COLOR_CYAN        "^5"
#define S_COLOR_MAGENTA     "^6"
#define S_COLOR_WHITE       "^7"
#define S_COLOR_ORANGE      "^8"
#define S_COLOR_MDGREY      "^9"
#define S_COLOR_LTGREY      "^:"
//#define S_COLOR_LTGREY		"^;"
#define S_COLOR_MDGREEN     "^<"
#define S_COLOR_MDYELLOW    "^="
#define S_COLOR_MDBLUE      "^>"
#define S_COLOR_MDRED       "^?"
#define S_COLOR_LTORANGE    "^A"
#define S_COLOR_MDCYAN      "^B"
#define S_COLOR_MDPURPLE    "^C"
#define S_COLOR_NULL        "^*"

extern const vec4_t g_color_table[32];
extern int ColorIndexFromChar( char ccode );

#define MAKERGB( v, r, g, b ) v[0] = r; v[1] = g; v[2] = b
#define MAKERGBA( v, r, g, b, a ) v[0] = r; v[1] = g; v[2] = b; v[3] = a

// Hex Color string support
#define gethex( ch ) ( ( ch ) > '9' ? ( ( ch ) >= 'a' ? ( ( ch ) - 'a' + 10 ) : ( ( ch ) - '7' ) ) : ( ( ch ) - '0' ) )
#define ishex( ch )  ( ( ch ) && ( ( ( ch ) >= '0' && ( ch ) <= '9' ) || ( ( ch ) >= 'A' && ( ch ) <= 'F' ) || ( ( ch ) >= 'a' && ( ch ) <= 'f' ) ) )
// check if it's format rrggbb r,g,b e {0..9} U {A...F}
#define Q_IsHexColorString( p ) ( ishex( *( p ) ) && ishex( *( ( p ) + 1 ) ) && ishex( *( ( p ) + 2 ) ) && ishex( *( ( p ) + 3 ) ) && ishex( *( ( p ) + 4 ) ) && ishex( *( ( p ) + 5 ) ) )
#define Q_HexColorStringHasAlpha( p ) ( ishex( *( ( p ) + 6 ) ) && ishex( *( ( p ) + 7 ) ) )

#define DEG2RAD( a ) ( ( ( a ) * M_PI ) / 180.0F )
#define RAD2DEG( a ) ( ( ( a ) * 180.0f ) / M_PI )

struct cplane_s;

extern vec3_t vec3_origin;
extern vec3_t axisDefault[3];

#define nanmask ( 255 << 23 )

#define IS_NAN( x ) ( ( ( *(int *)&x ) & nanmask ) == nanmask )

float Q_fabs( float f );
float Q_rsqrt( float f );		// reciprocal square root

float Q_log2f( float f );
float Q_exp2f( float f );

#define SQRTFAST( x ) ( (x) * Q_rsqrt( x ) )

// fast float to int conversion
#if id386 && defined(_WIN32)
long Q_ftol( float f );
#else
#define Q_ftol( x ) lrintf( x )
#endif

signed char ClampChar( int i );
signed char ClampCharMove( int i );
signed short ClampShort( int i );

// this isn't a real cheap function to call!
int DirToByte( vec3_t dir );
void ByteToDir( int b, vec3_t dir );

#ifndef SGN
#define SGN(x) (((x) >= 0) ? !!(x) : -1)
#endif

#if	1

#define DotProduct(x,y)			((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define	VectorScale(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s))
#define	VectorMA(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s))

#define DotProduct4(a,b)		((a)[0]*(b)[0] + (a)[1]*(b)[1] + (a)[2]*(b)[2] + (a)[3]*(b)[3])
#define VectorScale4(a,b,c)		((c)[0]=(a)[0]*(b),(c)[1]=(a)[1]*(b),(c)[2]=(a)[2]*(b),(c)[3]=(a)[3]*(b))

#else

#define DotProduct(x,y)			_DotProduct(x,y)
#define VectorSubtract(a,b,c)	_VectorSubtract(a,b,c)
#define VectorAdd(a,b,c)		_VectorAdd(a,b,c)
#define VectorCopy(a,b)			_VectorCopy(a,b)
#define	VectorScale(v, s, o)	_VectorScale(v,s,o)
#define	VectorMA(v, s, b, o)	_VectorMA(v,s,b,o)

#endif

#define VectorClear( a )              ( ( a )[0] = ( a )[1] = ( a )[2] = 0 )
#define VectorNegate( a,b )           ( ( b )[0] = -( a )[0],( b )[1] = -( a )[1],( b )[2] = -( a )[2] )
#define VectorSet( v, x, y, z )       ( ( v )[0] = ( x ), ( v )[1] = ( y ), ( v )[2] = ( z ) )

#define Vector2Clear( a )             ( ( a )[0] = ( a )[1] = 0 )
#define Vector2Set( v, x, y )         ( ( v )[0] = ( x ),( v )[1] = ( y ) )
#define Vector2Copy( a,b )            ( ( b )[0] = ( a )[0],( b )[1] = ( a )[1] )
#define Vector2Subtract( a,b,c )      ( ( c )[0] = ( a )[0] - ( b )[0],( c )[1] = ( a )[1] - ( b )[1] )

#define Vector4Clear( a )             ( ( a )[0] = ( a )[1] = ( a )[2] = ( a )[3] = 0 )
#define Vector4Set( v, x, y, z, w )   ( ( v )[0] = ( x ),( v )[1] = ( y ),( v )[2] = ( z ),( v )[3] = ( w ) )
#define Vector4Copy( a,b )            ( ( b )[0] = ( a )[0],( b )[1] = ( a )[1],( b )[2] = ( a )[2],( b )[3] = ( a )[3] )
#define Vector4MA( v, s, b, o )       ( ( o )[0] = ( v )[0] + ( b )[0] * ( s ),( o )[1] = ( v )[1] + ( b )[1] * ( s ),( o )[2] = ( v )[2] + ( b )[2] * ( s ),( o )[3] = ( v )[3] + ( b )[3] * ( s ) )
#define Vector4Average( v, b, s, o )  ( ( o )[0] = ( ( v )[0] * ( 1 - ( s ) ) ) + ( ( b )[0] * ( s ) ),( o )[1] = ( ( v )[1] * ( 1 - ( s ) ) ) + ( ( b )[1] * ( s ) ),( o )[2] = ( ( v )[2] * ( 1 - ( s ) ) ) + ( ( b )[2] * ( s ) ),( o )[3] = ( ( v )[3] * ( 1 - ( s ) ) ) + ( ( b )[3] * ( s ) ) )

#define Byte4Clear( a )               ( ( a )[0] = ( a )[1] = ( a )[2] = ( a )[3] = 0 )
#define Byte4Set(v, x, y, z, w)       ( ( v )[0] = ( x ),( v )[1] = ( y ),( v )[2] = ( z ),( v )[3] = ( w ) )
#define Byte4Copy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define QuatCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define SnapVector( v ) {v[0] = ( (int)( v[0] ) ); v[1] = ( (int)( v[1] ) ); v[2] = ( (int)( v[2] ) );}

// just in case you don't want to use the macros
vec_t _DotProduct( const vec3_t v1, const vec3_t v2 );
void _VectorSubtract( const vec3_t veca, const vec3_t vecb, vec3_t out );
void _VectorAdd( const vec3_t veca, const vec3_t vecb, vec3_t out );
void _VectorCopy( const vec3_t in, vec3_t out );
void _VectorScale( const vec3_t in, float scale, vec3_t out );
void _VectorMA( const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc );

unsigned ColorBytes3( float r, float g, float b );
unsigned ColorBytes4( float r, float g, float b, float a );

float NormalizeColor( const vec3_t in, vec3_t out );

float RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
void ClearBounds( vec3_t mins, vec3_t maxs );
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );

static ID_INLINE int VectorCompare( const vec3_t v1, const vec3_t v2 ) {
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2]) {
		return 0;
	}			
	return 1;
}

static ID_INLINE vec_t VectorLength( const vec3_t v ) {
	return (vec_t)sqrtf (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static ID_INLINE vec_t VectorLengthSquared( const vec3_t v ) {
	return (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static ID_INLINE vec_t Distance( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return VectorLength( v );
}

static ID_INLINE vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
static ID_INLINE void VectorNormalizeFast( vec3_t v )
{
	float ilength;

	ilength = Q_rsqrt( DotProduct( v, v ) );

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

static ID_INLINE void VectorInverse( vec3_t v ){
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

static ID_INLINE void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross ) {
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

vec_t VectorNormalize (vec3_t v);		// returns vector length
vec_t VectorNormalize2( const vec3_t v, vec3_t out );
void Vector4Scale( const vec4_t in, vec_t scale, vec4_t out );
void VectorRotate( const vec3_t in, const vec3_t matrix[3], vec3_t out );
int Q_log2(int val);

float Q_acos(float c);

int		Q_rand( int *seed );
float	Q_random( int *seed );
float	Q_crandom( int *seed );

#define random()	((rand () & 0x7fff) / ((float)0x7fff))
#define crandom()	(2.0 * (random() - 0.5))

void vectoangles( const vec3_t value1, vec3_t angles );
float vectoyaw( const vec3_t vec );
void AnglesToAxis( const vec3_t angles, vec3_t axis[3] );
// TTimo: const vec_t ** would require explicit casts for ANSI C conformance
// see unix/const-arg.c
void AxisToAngles( /*const*/ vec3_t axis[3], vec3_t angles );
float VectorDistance( vec3_t v1, vec3_t v2 );
float VectorDistanceSquared( vec3_t v1, vec3_t v2 );


void AxisClear( vec3_t axis[3] );
void AxisCopy( vec3_t in[3], vec3_t out[3] );

void SetPlaneSignbits( struct cplane_s *out );
int BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const struct cplane_s *plane );

/*qboolean BoundsIntersect(const vec3_t mins, const vec3_t maxs,
		const vec3_t mins2, const vec3_t maxs2);
qboolean BoundsIntersectSphere(const vec3_t mins, const vec3_t maxs,
		const vec3_t origin, vec_t radius);
qboolean BoundsIntersectPoint(const vec3_t mins, const vec3_t maxs,
		const vec3_t origin);*/
float   AngleMod( float a );
float   LerpAngle( float from, float to, float frac );
void    LerpPosition( vec3_t start, vec3_t end, float frac, vec3_t out );
float   AngleSubtract( float a1, float a2 );
void    AnglesSubtract( vec3_t v1, vec3_t v2, vec3_t v3 );

float AngleNormalize2Pi( float angle );
float AngleNormalize360( float angle );
float AngleNormalize180( float angle );
float AngleDelta( float angle1, float angle2 );

qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
//void RotatePointAroundVertex( vec3_t pnt, float rot_x, float rot_y, float rot_z, const vec3_t origin );
void RotateAroundDirection( vec3_t axis[3], float yaw );
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up );
// perpendicular vector could be replaced by this

//int PlaneTypeForNormal( vec3_t normal );

void MatrixMultiply( float in1[3][3], float in2[3][3], float out[3][3] );
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );
void PerpendicularVector( vec3_t dst, const vec3_t src );
int Q_isnan( float x );
float Q_atof( const char *str );

#ifndef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#endif

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif

// Ridah
void GetPerpendicularViewVector( const vec3_t point, const vec3_t p1, const vec3_t p2, vec3_t up );
void ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj );
void ProjectPointOntoVectorBounded( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj );
float DistanceFromLineSquared( vec3_t p, vec3_t lp1, vec3_t lp2 );
float DistanceFromVectorSquared( vec3_t p, vec3_t lp1, vec3_t lp2 );
// done.

//=============================================

float Com_ClampFloat( float min, float max, float value );
int Com_ClampInt( int min, int max, int value );

qboolean COM_SuffixCompare( const char *in, const char *suffixStr );

char    *COM_SkipPath( char *pathname );
void    COM_FixPath( char *pathname );
const char	*COM_GetExtension( const char *name );
void	COM_StripExtension(const char *in, char *out, int destsize);
qboolean COM_CompareExtension(const char *in, const char *ext);
void	COM_DefaultExtension( char *path, int maxSize, const char *extension );
void COM_StripFilename( const char *in, char *out, int destSize );

unsigned long Com_GenerateHashValue( const char *fname, const unsigned int size );

void    COM_BeginParseSession( const char *name );
void    COM_RestoreParseSession( const char **data_p );
//void    COM_SetCurrentParseLine( int line );
int     COM_GetCurrentParseLine( void );
char	*COM_Parse( const char **data_p );
char	*COM_ParseExt( const char **data_p, qboolean allowLineBreak );
int     COM_Compress( char *data_p );
void    COM_ParseError( const char *format, ... ) FORMAT_PRINTF(1, 2);
void    COM_ParseWarning( const char *format, ... ) FORMAT_PRINTF(1, 2);
int Com_ParseInfos( const char *buf, int max, char infos[][MAX_INFO_STRING] );

char	*COM_ParseComplex( const char **data_p, qboolean allowLineBreak );

typedef enum {
	TK_GENEGIC = 0, // for single-char tokens
	TK_STRING,
	TK_QUOTED,
	TK_EQ,
	TK_NEQ,
	TK_GT,
	TK_GTE,
	TK_LT,
	TK_LTE,
	TK_MATCH,
	TK_OR,
	TK_AND,
	TK_SCOPE_OPEN,
	TK_SCOPE_CLOSE,
	TK_NEWLINE,
	TK_EOF,
} tokenType_t;

extern tokenType_t com_tokentype;

qboolean COM_BitCheck( const int array[], unsigned int bitNum );
void COM_BitSet( int array[], unsigned int bitNum );
void COM_BitClear( int array[], unsigned int bitNum );

#define MAX_TOKENLENGTH     1024

#ifndef TT_STRING
//token types
#define TT_STRING                   1           // string
#define TT_LITERAL                  2           // literal
#define TT_NUMBER                   3           // number
#define TT_NAME                     4           // name
#define TT_PUNCTUATION              5           // punctuation
#endif

typedef struct pc_token_s
{
	int type;
	int subtype;
	int intvalue;
	float floatvalue;
	char string[MAX_TOKENLENGTH];
	int line;
	int linescrossed;
} pc_token_t;

// data is an in/out parm, returns a parsed out token

qboolean SkipBracedSection( const char **program, int depth );
void SkipRestOfLine( const char **data );

void Parse1DMatrix( const char **buf_p, int x, float *m);
void Parse2DMatrix( const char **buf_p, int y, int x, float *m);
void Parse3DMatrix( const char **buf_p, int z, int y, int x, float *m);

int  QDECL Com_sprintf( char *dest, int size, const char *fmt, ... ) FORMAT_PRINTF( 3, 4 );

char *Com_SkipTokens( char *s, int numTokens, char *sep );
char *Com_SkipCharset( char *s, char *sep );

void Com_RandomBytes( byte *string, int len );

void Com_SortFileList( char **list, int nfiles, int fastSort );

// mode parm for FS_FOpenFile
typedef enum {
	FS_READ,
	FS_WRITE,
	FS_APPEND,
	FS_APPEND_SYNC
} fsMode_t;

typedef enum {
	FS_SEEK_CUR,
	FS_SEEK_END,
	FS_SEEK_SET
} fsOrigin_t;

//=============================================

extern const byte locase[ 256 ];
extern const byte upcase[ 256 ];

int Q_isprint( int c );
int Q_islower( int c );
int Q_isupper( int c );
int Q_isalpha( int c );
int Q_isnumeric( int c );
int Q_isalphanumeric( int c );
int Q_isforfilename( int c );

qboolean Q_streq( const char *s1, const char *s2 );

// portable case (in)sensitive compare
int     Q_strcmp( const char *s1, const char *s2 );
int     Q_stricmp( const char *s1, const char *s2 );
int     Q_strncmp( const char *s1, const char *s2, int n );
int     Q_stricmpn( const char *s1, const char *s2, int n );
char    *Q_strlwr( char *s1 );
char    *Q_strupr( char *s1 );
const char	*Q_stristr( const char *s, const char *find);

qboolean Q_isanumber( const char *s );
qboolean Q_isintegral( float f );

#ifdef _WIN32
#define Q_putenv _putenv
#else
#define Q_putenv putenv
#endif

// buffer size safe library replacements
void    Q_strncpyz( char *dest, const char *src, int destsize );
void    Q_strcat( char *dest, int size, const char *src );

int     Q_replace( const char *str1, const char *str2, char *src, int max_len );

char	*Q_stradd( char *dst, const char *src );
char	*Q_strncpy( char *dest, const char *src, int destsize );

// strlen that discounts Quake color sequences
int Q_PrintStrlen( const char *string );
// removes color sequences from string
char *Q_CleanStr( char *string );
// Count the number of char tocount encountered in string
int Q_CountChar(const char *string, char tocount);
// removes whitespaces and other bad directory characters
char *Q_CleanDirName( char *dirname );

//=============================================

typedef intptr_t (*syscall_t)( intptr_t *parms );
typedef intptr_t (QDECL *vmMain_t)( int command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6 );
typedef intptr_t (QDECL *dllSyscall_t)( intptr_t callNum, ... );
typedef void (QDECL *dllEntry_t)( dllSyscall_t syscallptr );

const char *QDECL va( const char *format, ... ) FORMAT_PRINTF(1, 2);
float   *tv( float x, float y, float z );

#define TRUNCATE_LENGTH	64
void Com_TruncateLongString( char *buffer, const char *s );

//=============================================

//
// key / value info strings
//
char *Info_ValueForKey( const char *s, const char *key );
void Info_Tokenize( const char *s );
const char *Info_ValueForKeyToken( const char *key );
#define Info_SetValueForKey( buf, key, value ) Info_SetValueForKey_s( (buf), MAX_INFO_STRING, (key), (value) )
qboolean Info_SetValueForKey_s( char *s, int slen, const char *key, const char *value );
qboolean Info_Validate( const char *s );
qboolean Info_ValidateKeyValue( const char *s );
const char *Info_NextPair( const char *s, char *key, char *value );
int Info_RemoveKey( char *s, const char *key );

// this is only here so the functions in q_shared.c and bg_*.c can link
void	NORETURN QDECL Com_Error( errorParm_t level, const char *fmt, ... ) FORMAT_PRINTF(2, 3);
void	QDECL Com_Printf( const char *msg, ... ) FORMAT_PRINTF(1, 2);

/*
==========================================================

  RELOAD STATES

==========================================================
*/

#define RELOAD_SAVEGAME         0x01
#define RELOAD_NEXTMAP          0x02
#define RELOAD_NEXTMAP_WAITING  0x04
#define RELOAD_FAILED           0x08
#define RELOAD_ENDGAME          0x10


/*
==========================================================

CVARS (console variables)

Many variables can be used for cheating purposes, so when
cheats is zero, force all unspecified variables to their
default values.
==========================================================
*/

typedef enum module_e {
	MODULE_NONE = 0,
	MODULE_COMMON,
	MODULE_SERVER,
	MODULE_CLIENT,
	MODULE_RENDERER,
	MODULE_SOUND,
	MODULE_NETWORK,
	MODULE_INPUT,
	//MODULE_SYSTEM,
	//MODULE_DOWNLOAD,
	MODULE_CONSOLE,
	MODULE_CGAME,
	MODULE_SGAME,
	MODULE_UI,
	MODULE_COUNT
} module_t;

#define CVAR_NONE 0

#define CVAR_ARCHIVE        0x0001   // set to cause it to be saved to vars.rc
								// used for system variables, not for player
								// specific configurations
#define CVAR_USERINFO       0x0002   // sent to server on connect or change
#define CVAR_SERVERINFO     0x0004   // sent in response to front end requests
#define CVAR_SYSTEMINFO     0x0008   // these cvars will be duplicated on all clients
#define CVAR_INIT           0x0010  // don't allow change from console at all,
								// but can be set from the command line
#define CVAR_LATCH          0x0020  // will only change when C code next does
								// a Cvar_Get(), so it can't be changed
								// without proper initialization.  modified
								// will be set, even though the value hasn't
								// changed yet
#define CVAR_ROM            0x0040  // display only, cannot be set by user at all
#define CVAR_USER_CREATED   0x0080 // created by a set command
#define CVAR_TEMP           0x0100 // can be set even when cheats are disabled, but is not archived
#define CVAR_CHEAT          0x0200 // can not be changed if cheats are disabled
#define CVAR_NORESTART      0x0400    // do not clear when a cvar_restart is issued
#define CVAR_WOLFINFO       0x0800    // DHM - NERVE :: Like userinfo, but for wolf multiplayer info

#define CVAR_UNSAFE         0x1000    // ydnar: unsafe system cvars (renderer, sound settings, anything that might cause a crash)
#define CVAR_SERVERINFO_NOUPDATE        0x2000    // gordon: WONT automatically send this to clients, but server browsers will see it

#define CVAR_SERVER_CREATED	0x4000	// cvar was created by a server the client connected to.
#define CVAR_VM_CREATED		0x8000	// cvar was created exclusively in one of the VMs.
#define CVAR_PROTECTED		0x10000	// prevent modifying this var from VMs or the server

#define CVAR_NODEFAULT		0x20000	// do not write to config if matching with default value

#define CVAR_PRIVATE		0x40000	// can't be read from VM

#define CVAR_DEVELOPER		0x80000	// can be set only in developer mode
#define CVAR_NOTABCOMPLETE	0x100000 // no tab completion in console

#define CVAR_ARCHIVE_ND		(CVAR_ARCHIVE | CVAR_NODEFAULT)

// These flags are only returned by the Cvar_Flags() function
#define CVAR_MODIFIED		0x40000000	// Cvar was modified
#define CVAR_NONEXISTENT	0x80000000	// Cvar doesn't exist.

typedef enum {
	CV_NONE = 0,
	CV_FLOAT,
	CV_INTEGER,
	CV_FSPATH,
	CV_USERINFO, // clean userinfo of > 127 chars
	CV_MAX,
} cvarValidator_t;

typedef enum {
	CVG_NONE = 0,
	CVG_RENDERER,
	CVG_CLIENT,
	CVG_SERVER,
	CVG_LANGUAGE,
	CVG_MAX,
} cvarGroup_t;

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s cvar_t;

struct cvar_s {
	char		*name;
	char		*string;
	char		*resetString;		// cvar_restart will reset to this value
	char		*latchedString;		// for CVAR_LATCH vars
	int			flags;
	qboolean	modified;			// set each time the cvar is changed
	int			modificationCount;	// incremented each time the cvar is changed
	float		value;				// Q_atof( string )
	int			integer;			// atoi( string )
	cvarValidator_t validator;
	char		*mins;
	char		*maxs;
	char		*description;

	cvar_t		*next;
	cvar_t		*prev;
	cvar_t		*hashNext;
	cvar_t		*hashPrev;
	int			hashIndex;
	cvarGroup_t	group;				// to track changes
};

#define	MAX_CVAR_VALUE_STRING	256

typedef int	cvarHandle_t;

// the modules that run in the virtual machine can't access the cvar_t directly,
// so they must ask for structured updates
typedef struct {
	cvarHandle_t	handle;
	int			modificationCount;
	float		value;
	int			integer;
	char		string[MAX_CVAR_VALUE_STRING];
} vmCvar_t;

typedef void (*updateFunc_t) (vmCvar_t *cv);
typedef struct vmCvarTableItem_s {
	vmCvar_t		*cvar;
	const char		*name;
	const char		*defaultValue;
	int				flags;
#ifdef GAMEDLL
	qboolean		trackChange;           // track this variable, and announce if changed
	qboolean		fConfigReset;          // OSP: set this var to the default on a config reset
#endif
	updateFunc_t	update;
} vmCvarTableItem_t;

/*
==============================================================

COLLISION DETECTION

==============================================================
*/

#include "../game/surfaceflags.h"            // shared with the q3map utility

// plane types are used to speed some tests
// 0-2 are axial planes
#define PLANE_X             0
#define PLANE_Y             1
#define PLANE_Z             2
#define PLANE_NON_AXIAL     3
#define PLANE_NON_PLANAR    4


/*
=================
PlaneTypeForNormal
=================
*/

//#define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL) ) )
#define PlaneTypeForNormal( x ) ( x[0] == 1.0 ? PLANE_X : ( x[1] == 1.0 ? PLANE_Y : ( x[2] == 1.0 ? PLANE_Z : ( x[0] == 0.f && x[1] == 0.f && x[2] == 0.f ? PLANE_NON_PLANAR : PLANE_NON_AXIAL ) ) ) )


// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
typedef struct cplane_s {
	vec3_t normal;
	float dist;
	byte type;              // for fast side tests: 0,1,2 = axial, 3 = nonaxial
	byte signbits;          // signx + (signy<<1) + (signz<<2), used as lookup during collision
	byte pad[2];
} cplane_t;

#define CPLANE


// a trace is returned when a box is swept through the world
typedef struct {
	qboolean allsolid;      // if true, plane is not valid
	qboolean startsolid;    // if true, the initial point was in a solid area
	float fraction;         // time completed, 1.0 = didn't hit anything
	vec3_t endpos;          // final position
	cplane_t plane;         // surface normal at impact, transformed to world space
	int surfaceFlags;           // surface hit
	int contents;           // contents on other side of surface hit
	int entityNum;          // entity the contacted sirface is a part of
} trace_t;

// trace->entityNum can also be 0 to (MAX_GENTITIES-1)
// or ENTITYNUM_NONE, ENTITYNUM_WORLD


// markfragments are returned by CM_MarkFragments()
typedef struct {
	int firstPoint;
	int numPoints;
} markFragment_t;



typedef struct {
	vec3_t origin;
	vec3_t axis[3];
} orientation_t;

//=====================================================================


// in order from highest priority to lowest
// if none of the catchers are active, bound key strings will be executed
#define KEYCATCH_CONSOLE        0x0001
#define KEYCATCH_UI             0x0002
#define KEYCATCH_MESSAGE        0x0004
#define KEYCATCH_CGAME          0x0008


// sound channels
// channel 0 never willingly overrides
// other channels will allways override a playing sound on that channel
typedef enum {
	CHAN_AUTO,
	CHAN_LOCAL,     // menu sounds, etc
	CHAN_WEAPON,
	CHAN_VOICE,
	CHAN_ITEM,
	CHAN_BODY,
	CHAN_LOCAL_SOUND,   // chat messages, etc
	CHAN_ANNOUNCER,     // announcer voices, etc
	CHAN_VOICE_BG,  // xkan - background sound for voice (radio static, etc.)
} soundChannel_t;


/*
========================================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

========================================================================
*/
#define ANIM_BITS       10

#define ANGLE2SHORT( x )  ( (int)( ( x ) * 65536 / 360 ) & 65535 )
#define SHORT2ANGLE( x )  ( ( x ) * ( 360.0 / 65536 ) )

#define SNAPFLAG_RATE_DELAYED   1
#define SNAPFLAG_NOT_ACTIVE     2   // snapshot used during connection and for zombies
#define SNAPFLAG_SERVERCOUNT    4   // toggled every map_restart so transitions can be detected

//
// per-level limits
//
#define MAX_CLIENTS         64 // JPW NERVE back to q3ta default was 128		// absolute limit

#define GENTITYNUM_BITS     10  // JPW NERVE put q3ta default back for testing	// don't need to send any more
#define MAX_GENTITIES       ( 1 << GENTITYNUM_BITS )

// entitynums are communicated with GENTITY_BITS, so any reserved
// values thatare going to be communcated over the net need to
// also be in this range
#define ENTITYNUM_NONE      ( MAX_GENTITIES - 1 )
#define ENTITYNUM_WORLD     ( MAX_GENTITIES - 2 )
#define ENTITYNUM_MAX_NORMAL    ( MAX_GENTITIES - 2 )


#define MAX_MODELS          256     // these are sent over the net as 8 bits (Gordon: upped to 9 bits, erm actually it was already at 9 bits, wtf? NEVAR TRUST GAMECODE COMMENTS, comments are evil :E, lets hope it doesnt horribly break anything....)
#define MAX_SOUNDS          256     // so they cannot be blindly increased
#define MAX_CS_SKINS        64
#define MAX_CSSTRINGS       32

#define MAX_CS_SHADERS      32
#define MAX_SERVER_TAGS     256
#define MAX_TAG_FILES       64

#define MAX_MULTI_SPAWNTARGETS  16 // JPW NERVE

#define MAX_CONFIGSTRINGS   1024

#define MAX_DLIGHT_CONFIGSTRINGS    16
#define MAX_SPLINE_CONFIGSTRINGS    8

#define PARTICLE_SNOW128    1
#define PARTICLE_SNOW64     2
#define PARTICLE_SNOW32     3
#define PARTICLE_SNOW256    0

#define PARTICLE_BUBBLE8    4
#define PARTICLE_BUBBLE16   5
#define PARTICLE_BUBBLE32   6
#define PARTICLE_BUBBLE64   7

// these are the only configstrings that the system reserves, all the
// other ones are strictly for servergame to clientgame communication
#define CS_SERVERINFO       0       // an info string with all the serverinfo cvars
#define CS_SYSTEMINFO       1       // an info string for server system to client system configuration (timescale, etc)

#define RESERVED_CONFIGSTRINGS  2   // game can't modify below this, only the system can

#define MAX_GAMESTATE_CHARS 16000
typedef struct {
	int stringOffsets[MAX_CONFIGSTRINGS];
	char stringData[MAX_GAMESTATE_CHARS];
	int dataCount;
} gameState_t;

// xkan, 1/10/2003 - adapted from original SP
typedef enum
{
	AISTATE_RELAXED,
	AISTATE_QUERY,
	AISTATE_ALERT,
	AISTATE_COMBAT,

	MAX_AISTATES
} aistateEnum_t;

#define REF_FORCE_DLIGHT    0x80000000u // RF, passed in through overdraw parameter, force this dlight under all conditions
#define REF_JUNIOR_DLIGHT   0x40000000u // (SA) this dlight does not light surfaces.  it only affects dynamic light grid
#define REF_DIRECTED_DLIGHT 0x20000000u // ydnar: global directional light, origin should be interpreted as a normal vector

// bit field limits
#define MAX_STATS               16
#define MAX_PERSISTANT          16
#define MAX_POWERUPS            16
#define MAX_WEAPONS             64  // (SA) and yet more!

// Ridah, increased this
//#define	MAX_PS_EVENTS			2
// ACK: I'd really like to make this 4, but that seems to cause network problems
#define MAX_EVENTS              4   // max events per frame before we drop events
//#define	MAX_EVENTS				2	// max events per frame before we drop events


#define PS_PMOVEFRAMECOUNTBITS  6

// playerState_t is the information needed by both the client and server
// to predict player motion and actions
// nothing outside of pmove should modify these, or some degree of prediction error
// will occur

// you can't add anything to this without modifying the code in msg.c
// (Gordon: unless it doesnt need transmitted over the network, in which case it should prolly go in the new pmext struct anyway)

// playerState_t is a full superset of entityState_t as it is used by players,
// so if a playerState_t is transmitted, the entityState_t can be fully derived
// from it.
//
// NOTE: all fields in here must be 32 bits (or those within sub-structures)
typedef struct playerState_s {
	int commandTime;            // cmd->serverTime of last executed command
	int pm_type;
	int bobCycle;               // for view bobbing and footstep generation
	int pm_flags;               // ducked, jump_held, etc
	int pm_time;

	vec3_t origin;
	vec3_t velocity;
	int weaponTime;
	int weaponDelay;            // for weapons that don't fire immediately when 'fire' is hit (grenades, venom, ...)
	int grenadeTimeLeft;            // for delayed grenade throwing.  this is set to a #define for grenade
									// lifetime when the attack button goes down, then when attack is released
									// this is the amount of time left before the grenade goes off (or if it
									// gets to 0 while in players hand, it explodes)


	int gravity;
	float leanf;                // amount of 'lean' when player is looking around corner //----(SA)	added

	int speed;
	int delta_angles[3];            // add to command angles to get view direction
									// changed by spawns, rotating objects, and teleporters

	int groundEntityNum;        // ENTITYNUM_NONE = in air

	int legsTimer;              // don't change low priority animations until this runs out
	int legsAnim;               // mask off ANIM_TOGGLEBIT

	int torsoTimer;             // don't change low priority animations until this runs out
	int torsoAnim;              // mask off ANIM_TOGGLEBIT

	int movementDir;            // a number 0 to 7 that represents the reletive angle
								// of movement to the view angle (axial and diagonals)
								// when at rest, the value will remain unchanged
								// used to twist the legs during strafing



	int eFlags;                 // copied to entityState_t->eFlags

	int eventSequence;          // pmove generated events
	int events[MAX_EVENTS];
	int eventParms[MAX_EVENTS];
	int oldEventSequence;           // so we can see which events have been added since we last converted to entityState_t

	int externalEvent;          // events set on player from another source
	int externalEventParm;
	int externalEventTime;

	int clientNum;              // ranges from 0 to MAX_CLIENTS-1

	// weapon info
	int weapon;                 // copied to entityState_t->weapon
	int weaponstate;

	// item info
	int item;

	vec3_t viewangles;          // for fixed views
	int viewheight;

	// damage feedback
	int damageEvent;            // when it changes, latch the other parms
	int damageYaw;
	int damagePitch;
	int damageCount;

	int stats[MAX_STATS];
	int persistant[MAX_PERSISTANT];         // stats that aren't cleared on death
	int powerups[MAX_POWERUPS];         // level.time that the powerup runs out
	int ammo[MAX_WEAPONS];              // total amount of ammo
	int ammoclip[MAX_WEAPONS];          // ammo in clip
	int holdable[16];
	int holding;                        // the current item in holdable[] that is selected (held)
	int weapons[MAX_WEAPONS / ( sizeof( int ) * 8 )];   // 64 bits for weapons held

	// Ridah, allow for individual bounding boxes
	vec3_t mins, maxs;
	float crouchMaxZ;
	float crouchViewHeight, standViewHeight, deadViewHeight;
	// variable movement speed
	float runSpeedScale, sprintSpeedScale, crouchSpeedScale;
	// done.

	// Ridah, view locking for mg42
	int viewlocked;
	int viewlocked_entNum;

	float friction;

	int nextWeapon;
	int teamNum;                        // Arnout: doesn't seem to be communicated over the net

	// Rafael
	//int			gunfx;

	// RF, burning effect is required for view blending effect
	int onFireStart;

	int serverCursorHint;               // what type of cursor hint the server is dictating
	int serverCursorHintVal;            // a value (0-255) associated with the above

	trace_t serverCursorHintTrace;      // not communicated over net, but used to store the current server-side cursorhint trace

	// ----------------------------------------------------------------------
	// So to use persistent variables here, which don't need to come from the server,
	// we could use a marker variable, and use that to store everything after it
	// before we read in the new values for the predictedPlayerState, then restore them
	// after copying the structure recieved from the server.

	// Arnout: use the pmoveExt_t structure in bg_public.h to store this kind of data now (presistant on client, not network transmitted)

	int ping;                   // server to game info for scoreboard
	int pmove_framecount;
	int entityEventSequence;

	int sprintExertTime;

	// JPW NERVE -- value for all multiplayer classes with regenerating "class weapons" -- ie LT artillery, medic medpack, engineer build points, etc
	int classWeaponTime;                // Arnout : DOES get send over the network
	int jumpTime;                   // used in MP to prevent jump accel
	// jpw

	int weapAnim;                   // mask off ANIM_TOGGLEBIT										//----(SA)	added		// Arnout : DOES get send over the network

	qboolean releasedFire;

	float aimSpreadScaleFloat;          // (SA) the server-side aimspreadscale that lets it track finer changes but still only
										// transmit the 8bit int to the client
	int aimSpreadScale;                 // 0 - 255 increases with angular movement		// Arnout : DOES get send over the network
	int lastFireTime;                   // used by server to hold last firing frame briefly when randomly releasing trigger (AI)

	int quickGrenTime;

	int leanStopDebounceTime;

//----(SA)	added

	// seems like heat and aimspread could be tied together somehow, however, they (appear to) change at different rates and
	// I can't currently see how to optimize this to one server->client transmission "weapstatus" value.
	int weapHeat[MAX_WEAPONS];          // some weapons can overheat.  this tracks (server-side) how hot each weapon currently is.
	int curWeapHeat;                    // value for the currently selected weapon (for transmission to client)		// Arnout : DOES get send over the network
	int identifyClient;                 // NERVE - SMF
	int identifyClientHealth;

	aistateEnum_t aiState;          // xkan, 1/10/2003
} playerState_t;


//====================================================================


//
// usercmd_t->button bits, many of which are generated by the client system,
// so they aren't game/cgame only definitions
//
#define BUTTON_ATTACK       1
#define BUTTON_TALK         2           // displays talk balloon and disables actions
//#define	BUTTON_USE_HOLDABLE	4
#define BUTTON_GESTURE      8
#define	BUTTON_WALKING		16			// walking can't just be inferred from MOVE_RUN
										// because a key pressed late in the frame will
										// only generate a small move value for that frame
										// walking will use different animations and
										// won't generate footsteps
//----(SA)	added
#define BUTTON_SPRINT       32
#define BUTTON_ACTIVATE     64
//----(SA)	end

#define BUTTON_ANY          128         // any key whatsoever




//----(SA) wolf buttons
#define WBUTTON_ATTACK2     1
#define WBUTTON_ZOOM        2
#define WBUTTON_RELOAD      8
#define WBUTTON_LEANLEFT    16
#define WBUTTON_LEANRIGHT   32
#define WBUTTON_DROP        64 // JPW NERVE
#define WBUTTON_PRONE       128 // Arnout: wbutton now
//----(SA) end

#define MOVE_RUN            120         // if forwardmove or rightmove are >= MOVE_RUN,
										// then BUTTON_WALKING should be set

// Arnout: doubleTap buttons - DT_NUM can be max 8
typedef enum {
	DT_NONE,
	DT_MOVELEFT,
	DT_MOVERIGHT,
	DT_FORWARD,
	DT_BACK,
	DT_LEANLEFT,
	DT_LEANRIGHT,
	DT_UP,
	DT_NUM
} dtType_t;

// usercmd_t is sent to the server each client frame
typedef struct usercmd_s {
	int serverTime;
	byte buttons;
	byte wbuttons;
	byte weapon;
	byte flags;
	int angles[3];

	signed char forwardmove, rightmove, upmove;
	byte doubleTap;             // Arnout: only 3 bits used

	// rain - in ET, this can be any entity, and it's used as an array
	// index, so make sure it's unsigned
	byte identClient;           // NERVE - SMF
} usercmd_t;

//===================================================================

// if entityState->solid == SOLID_BMODEL, modelindex is an inline model number
#define SOLID_BMODEL    0xffffff

typedef enum {
	TR_STATIONARY,
	TR_INTERPOLATE,             // non-parametric, but interpolate between snapshots
	TR_LINEAR,
	TR_LINEAR_STOP,
	TR_LINEAR_STOP_BACK,        //----(SA)	added.  so reverse movement can be different than forward
	TR_SINE,                    // value = base + sin( time / duration ) * delta
	TR_GRAVITY,
	// Ridah
	TR_GRAVITY_LOW,
	TR_GRAVITY_FLOAT,           // super low grav with no gravity acceleration (floating feathers/fabric/leaves/...)
	TR_GRAVITY_PAUSED,          //----(SA)	has stopped, but will still do a short trace to see if it should be switched back to TR_GRAVITY
	TR_ACCELERATE,
	TR_DECCELERATE,
	// Gordon
	TR_SPLINE,
	TR_LINEAR_PATH
} trType_t;

typedef struct {
	trType_t trType;
	int trTime;
	int trDuration;             // if non 0, trTime + trDuration = stop time
//----(SA)	removed
	vec3_t trBase;
	vec3_t trDelta;             // velocity, etc
//----(SA)	removed
} trajectory_t;

// entityState_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
// Different eTypes may use the information in different ways
// The messages are delta compressed, so it doesn't really matter if
// the structure size is fairly large
//
// NOTE: all fields in here must be 32 bits (or those within sub-structures)

typedef enum {
	ET_GENERAL,
	ET_PLAYER,
	ET_ITEM,
	ET_MISSILE,
	ET_MOVER,
	ET_BEAM,
	ET_PORTAL,
	ET_SPEAKER,
	ET_PUSH_TRIGGER,
	ET_TELEPORT_TRIGGER,
	ET_INVISIBLE,
	ET_CONCUSSIVE_TRIGGER,  // JPW NERVE trigger for concussive dust particles
	ET_OID_TRIGGER,         // DHM - Nerve :: Objective Info Display
	ET_EXPLOSIVE_INDICATOR, // NERVE - SMF

	//---- (SA) Wolf
	ET_EXPLOSIVE,           // brush that will break into smaller bits when damaged
	ET_EF_SPOTLIGHT,
	ET_ALARMBOX,
	ET_CORONA,
	ET_TRAP,

	ET_GAMEMODEL,           // misc_gamemodel.  similar to misc_model, but it's a dynamic model so we have LOD
	ET_FOOTLOCKER,  //----(SA)	added
	//---- end

	ET_FLAMEBARREL,
	ET_FP_PARTS,

	// FIRE PROPS
	ET_FIRE_COLUMN,
	ET_FIRE_COLUMN_SMOKE,
	ET_RAMJET,

	ET_FLAMETHROWER_CHUNK,      // DHM - NERVE :: Used in server side collision detection for flamethrower

	ET_EXPLO_PART,

	ET_PROP,

	ET_AI_EFFECT,

	ET_CAMERA,
	ET_MOVERSCALED,

	ET_CONSTRUCTIBLE_INDICATOR,
	ET_CONSTRUCTIBLE,
	ET_CONSTRUCTIBLE_MARKER,
	ET_BOMB,
	ET_WAYPOINT,
	ET_BEAM_2,
	ET_TANK_INDICATOR,
	ET_TANK_INDICATOR_DEAD,
	// Start - TAT - 8/29/2002
	// An indicator object created by the bot code to show where the bots are moving to
	ET_BOTGOAL_INDICATOR,
	// End - TA - 8/29/2002
	ET_CORPSE,              // Arnout: dead player
	ET_SMOKER,              // Arnout: target_smoke entity

	ET_TEMPHEAD,            // Gordon: temporary head for clients for bullet traces
	ET_MG42_BARREL,         // Arnout: MG42 barrel
	ET_TEMPLEGS,            // Arnout: temporary leg for clients for bullet traces
	ET_TRIGGER_MULTIPLE,
	ET_TRIGGER_FLAGONLY,
	ET_TRIGGER_FLAGONLY_MULTIPLE,
	ET_GAMEMANAGER,
	ET_AAGUN,
	ET_CABINET_H,
	ET_CABINET_A,
	ET_HEALER,
	ET_SUPPLIER,

	ET_LANDMINE_HINT,       // Gordon: landmine hint for botsetgoalstate filter
	ET_ATTRACTOR_HINT,      // Gordon: attractor hint for botsetgoalstate filter
	ET_SNIPER_HINT,         // Gordon: sniper hint for botsetgoalstate filter
	ET_LANDMINESPOT_HINT,   // Gordon: landminespot hint for botsetgoalstate filter

	ET_COMMANDMAP_MARKER,

	ET_WOLF_OBJECTIVE,

	ET_EVENTS               // any of the EV_* events can be added freestanding
							// by setting eType to ET_EVENTS + eventNum
							// this avoids having to set eFlags and eventNum
} entityType_t;

typedef struct entityState_s {
	int number;                     // entity index
	entityType_t eType;             // entityType_t
	int eFlags;

	trajectory_t pos;       // for calculating position
	trajectory_t apos;      // for calculating angles

	int time;
	int time2;

	vec3_t origin;
	vec3_t origin2;

	vec3_t angles;
	vec3_t angles2;

	int otherEntityNum;     // shotgun sources, etc
	int otherEntityNum2;

	int groundEntityNum;        // -1 = in air

	int constantLight;      // r + (g<<8) + (b<<16) + (intensity<<24)
	int dl_intensity;       // used for coronas
	int loopSound;          // constantly loop this sound

	int modelindex;
	int modelindex2;
	int clientNum;          // 0 to (MAX_CLIENTS - 1), for players and corpses
	int frame;

	int solid;              // for client side prediction, trap_linkentity sets this properly

	// old style events, in for compatibility only
	int event;
	int eventParm;

	int eventSequence;      // pmove generated events
	int events[MAX_EVENTS];
	int eventParms[MAX_EVENTS];

	// for players
	int powerups;           // bit flags	// Arnout: used to store entState_t for non-player entities (so we know to draw them translucent clientsided)
	int weapon;             // determines weapon and flash model, etc
	int legsAnim;           // mask off ANIM_TOGGLEBIT
	int torsoAnim;          // mask off ANIM_TOGGLEBIT
//	int		weapAnim;		// mask off ANIM_TOGGLEBIT	//----(SA)	removed (weap anims will be client-side only)

	int density;            // for particle effects

	int dmgFlags;           // to pass along additional information for damage effects for players/ Also used for cursorhints for non-player entities

	// Ridah
	int onFireStart, onFireEnd;

	int nextWeapon;
	int teamNum;

	int effect1Time, effect2Time, effect3Time;

	aistateEnum_t aiState;      // xkan, 1/10/2003
	int animMovetype;       // clients can't derive movetype of other clients for anim scripting system
} entityState_t;

typedef enum {
	CA_UNINITIALIZED,
	CA_DISCONNECTED,    // not talking to a server
	CA_AUTHORIZING,     // not used any more, was checking cd key
	CA_CONNECTING,      // sending request packets to the server
	CA_CHALLENGING,     // sending challenge packets to the server
	CA_CONNECTED,       // netchan_t established, getting gamestate
	CA_LOADING,         // only during cgame initialization, never during main loop
	CA_PRIMED,          // got gamestate, waiting for first frame
	CA_ACTIVE,          // game views should be displayed
	CA_CINEMATIC        // playing a cinematic or a static pic, not connected to a server
} connstate_t;

// font support

#define GLYPH_START 0
#define GLYPH_END 255
#define GLYPH_CHARSTART 32
#define GLYPH_CHAREND 127
#define GLYPHS_PER_FONT (GLYPH_END - GLYPH_START + 1)
typedef struct {
	int height;       // number of scan lines
	int top;          // top of glyph in buffer
	int bottom;       // bottom of glyph in buffer
	int pitch;        // width for copying
	int xSkip;        // x adjustment
	int imageWidth;   // width of actual image
	int imageHeight;  // height of actual image
	float s;          // x offset in image where glyph starts
	float t;          // y offset in image where glyph starts
	float s2;
	float t2;
	qhandle_t glyph;  // handle to the shader with the glyph
	char shaderName[32];
} glyphInfo_t;

typedef struct {
	glyphInfo_t glyphs [GLYPHS_PER_FONT];
	float glyphScale;
	char name[MAX_QPATH];
} fontInfo_t;

#define Q_square( x ) ( ( x ) * ( x ) )
#define Q_cube( x) ( ( x ) * ( x ) * ( x ) )
#define Square( x ) Q_square( x )

// real time
//=============================================

typedef struct qtime_s {
	int tm_sec;     /* seconds after the minute - [0,59] */
	int tm_min;     /* minutes after the hour - [0,59] */
	int tm_hour;    /* hours since midnight - [0,23] */
	int tm_mday;    /* day of the month - [1,31] */
	int tm_mon;     /* months since January - [0,11] */
	int tm_year;    /* years since 1900 */
	int tm_wday;    /* days since Sunday - [0,6] */
	int tm_yday;    /* days since January 1 - [0,365] */
	int tm_isdst;   /* daylight savings time flag */
} qtime_t;


// server browser sources
#define AS_LOCAL        0
#define AS_GLOBAL       1           // NERVE - SMF - modified
#define AS_FAVORITES    2

// cinematic states
typedef enum {
	FMV_IDLE,
	FMV_PLAY,       // play
	FMV_EOF,        // all other conditions, i.e. stop/EOF/abort
	FMV_ID_BLT,
	FMV_ID_IDLE,
	FMV_LOOPED,
	FMV_ID_WAIT
} e_status;

#define MAX_GLOBAL_SERVERS          4096
#define MAX_OTHER_SERVERS           128
#define MAX_PINGREQUESTS            16
#define MAX_SERVERSTATUSREQUESTS    16

#define CDKEY_LEN 16
#define CDCHKSUM_LEN 2

// NERVE - SMF - localization
typedef enum {
#if !defined(__APPLE__) && !defined(__APPLE_CC__)   //DAJ USA
	LANGUAGE_FRENCH = 0,
	LANGUAGE_GERMAN,
	LANGUAGE_ITALIAN,
	LANGUAGE_SPANISH,
#endif
	MAX_LANGUAGES
} languages_t;

// NERVE - SMF - wolf server/game states
typedef enum {
	GS_INITIALIZE = -1,
	GS_PLAYING,
	GS_WARMUP_COUNTDOWN,
	GS_WARMUP,
	GS_INTERMISSION,
	GS_WAITING_FOR_PLAYERS,
	GS_RESET
} gamestate_t;

#define LERP( a, b, w ) ( ( a ) * ( 1.0f - ( w ) ) + ( b ) * ( w ) )
#define LUMA( red, green, blue ) ( 0.2126f * ( red ) + 0.7152f * ( green ) + 0.0722f * ( blue ) )

#define SQR( a ) ( ( a ) * ( a ) )

typedef int( *cmpFunc_t )( const void *a, const void *b );

void *Q_LinearSearch( const void *key, const void *ptr, size_t count,
	size_t size, cmpFunc_t cmp );

typedef enum gameMod_e {
	GAMEMOD_UNKNOWN = 0,
	GAMEMOD_BASEGAME, // etmain
	GAMEMOD_ETF,
	GAMEMOD_ETPRO,
	GAMEMOD_LEGACY,
	GAMEMOD_ETPUB,
	GAMEMOD_NITMOD,
	GAMEMOD_SILENT,
	GAMEMOD_JAYMOD,
	GAMEMOD_NOQUARTER,
	GAMEMOD_ETJUMP,
	GAMEMOD_ETRUN,
	GAMEMOD_TCETEST,
	GAMEMOD_CQBTEST,
	GAMEMOD_ETNAM,
	GAMEMOD_DUKE
	//GAMEMOD_MAX = 512
} gameMod_t;

typedef enum ossInfoFlag_e {
	OSS_AUTO = 0,
	OSS_WIN32 = BIT(0),
	OSS_WIN64 = BIT(1),
	OSS_LINUX32 = BIT(2),
	OSS_LINUX64 = BIT(3),
	OSS_LINUXARM64 = BIT(4),
	OSS_MACOSPPC = BIT(5),
	OSS_MACOSINTEL32 = BIT(6),
	OSS_MACOSINTEL64 = BIT(7),
	OSS_MACOSARM64 = BIT(8),
	OSS_ANDROID = BIT(9), // multiple android arches?
	OSS_RASBERRYPI = BIT(10),
	OSS_WINARM64 = BIT(11),
	//OSS_MAX = BIT(32)
} ossInfoFlag_t;

#endif  // __Q_SHARED_H__
