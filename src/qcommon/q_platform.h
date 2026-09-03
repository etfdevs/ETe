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
//
#ifndef __Q_PLATFORM_H__
#define __Q_PLATFORM_H__

#define QDECL

#define idppc 0
#define idppc64 0
#define id386 0
#define idx64 0
#define arm32 0
#define arm64 0

// ============================== Win32 ====================================

#ifdef _WIN32

#undef QDECL
#define QDECL __cdecl
#define Q_NEWLINE "\r\n"

#if defined (_WIN32_WINNT)
#if _WIN32_WINNT < 0x0501
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#else
#define _WIN32_WINNT 0x0501
#endif

#if !__clang__ && defined( _MSC_VER ) && _MSC_VER >= 1400 // MSVC++ 8.0 at least
#define OS_STRING "win_msvc"
#elif __clang__
#define OS_STRING "win_clang"
#elif defined __MINGW32__
#define OS_STRING "win_mingw"
#else
#error "Compiler not supported"
#endif

#define ID_INLINE __inline
#define PATH_SEP '\\'
#define PATH_SEP_FOREIGN '/'
#define DLL_EXT ".dll"
#define REN_DLL_EXT DLL_EXT

#if defined( _M_IX86 )
#define OSARCH_STRING "win32"
#define ARCH_STRING "x86"
#define Q3_LITTLE_ENDIAN
#undef id386
#define id386 1
#ifndef __WORDSIZE
#define __WORDSIZE 32
#endif
#endif

#if defined( _M_AMD64 )
#define OSARCH_STRING "win64"
#define ARCH_STRING "x64"
#define Q3_LITTLE_ENDIAN
#undef idx64
#define idx64 1
//#define UNICODE
#ifndef __WORDSIZE
#define __WORDSIZE 64
#endif
#endif

#if defined( _M_ARM64 )
#define OSARCH_STRING "winarm64"
#define ARCH_STRING "arm64"
#define Q3_LITTLE_ENDIAN
#undef arm64
#define arm64 1
#ifndef __WORDSIZE
#define __WORDSIZE 64
#endif
#endif

#else // !defined _WIN32

// common unix platforms parameters

#define Q_NEWLINE "\n"
#define PATH_SEP '/'
#define PATH_SEP_FOREIGN '\\'
#define DLL_EXT ".so"
#define REN_DLL_EXT DLL_EXT

#if defined (__ppc__)
#define ARCH_STRING "ppc"
#define Q3_BIG_ENDIAN
#undef idppc
#define idppc 1
#endif // __ppc__

#if defined(__PPC64__)
#if defined (__LITTLE_ENDIAN__)
#define ARCH_STRING "ppc64le"
#define Q3_LITTLE_ENDIAN
#undef idppc64
#define idppc64 1
#else
#define ARCH_STRING "ppc64"
#define Q3_BIG_ENDIAN
#undef idppc64
#define idppc64 1
#endif
#endif // __PPC64__

#if defined (__i386__)
#define ARCH_STRING "i386"
#define Q3_LITTLE_ENDIAN
#undef id386
#define id386 1
#endif // __i386__


#if defined (__x86_64__) || defined (__amd64__)
#define ARCH_STRING "x86_64"
#define Q3_LITTLE_ENDIAN
#undef idx64
#define idx64 1
#endif // __x86_64__ || __amd64__

#if defined (__arm__)
#define ARCH_STRING "arm"
#define Q3_LITTLE_ENDIAN
#undef arm32
#define arm32 1
#endif // __arm__

#if defined (__aarch64__)
#define ARCH_STRING "aarch64"
#define Q3_LITTLE_ENDIAN
#undef arm64
#define arm64 1
#endif // __arm64__

#endif // !_WIN32

// ============================== macOS ====================================

#if defined(__APPLE__) || defined(__APPLE_CC__)

#include <sys/types.h>
#include <machine/endian.h>

#define OS_STRING "macOS"
#undef DLL_EXT
#undef REN_DLL_EXT
#define DLL_EXT "_mac"
#define REN_DLL_EXT ".dylib"
#if defined (__i386__)
#define OSARCH_STRING OS_STRING "32"
#elif defined (__x86_64__) || defined (__amd64__)
#define OSARCH_STRING OS_STRING "64"
#else
#define OSARCH_STRING OS_STRING ARCH_STRING
#endif
#define ID_INLINE inline

#endif // __APPLE__  || __APPLE_CC__

// ============================== Linux ====================================

#ifdef __linux__

#include <endian.h>

#define OS_STRING "linux"
#if defined (__i386__)
#define OSARCH_STRING OS_STRING "32"
#elif defined (__x86_64__) || defined (__amd64__)
#define OSARCH_STRING OS_STRING "64"
#else
#define OSARCH_STRING OS_STRING ARCH_STRING
#endif
#define ID_INLINE inline

#endif // __linux___

// =============================== BSD =====================================

#if defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__)

#include <sys/types.h>
#include <machine/endian.h>


#if defined (__FreeBSD__)
#define OS_STRING "freebsd"
#elif defined (__NetBSD__)
#define OS_STRING "netbsd"
#elif defined (__OpenBSD__)
#define OS_STRING "openbsd"
#endif

#if defined (__i386__)
#define OSARCH_STRING OS_STRING "32"
#elif defined (__x86_64__) || defined (__amd64__)
#define OSARCH_STRING OS_STRING "64"
#else
#define OSARCH_STRING OS_STRING ARCH_STRING
#endif

#define ID_INLINE inline
#if BYTE_ORDER == BIG_ENDIAN
#define Q3_BIG_ENDIAN
#else
#define Q3_LITTLE_ENDIAN
#endif

#endif // __FreeBSD__ || __NetBSD__ || __OpenBSD__

// =========================================================================

//catch missing defines in above blocks
#if !defined( OS_STRING )
#error "Operating system not supported"
#endif

#if !defined( ARCH_STRING )
#error "Architecture not supported"
#endif

#if !defined( OSARCH_STRING )
#error "OSARCH_STRING not defined"
#endif

#ifndef ID_INLINE
#error "ID_INLINE not defined"
#endif

#ifndef PATH_SEP
#error "PATH_SEP not defined"
#endif

#ifndef PATH_SEP_FOREIGN
#error "PATH_SEP_FOREIGN not defined"
#endif

#ifndef DLL_EXT
#error "DLL_EXT not defined"
#endif

// Endianess

#if defined( Q3_BIG_ENDIAN ) && defined( Q3_LITTLE_ENDIAN )

#error "Endianness defined as both big and little"

#elif defined( Q3_BIG_ENDIAN )

#define CopyLittleShort(dest, src) CopyShortSwap(dest, src)
#define CopyLittleLong(dest, src) CopyLongSwap(dest, src)
#define LittleShort(x) ShortSwap(x)
#define LittleLong(x) LongSwap(x)
#define LittleFloat(x) FloatSwap(&x)
#define BigShort
#define BigLong
#define BigFloat

#elif defined( Q3_LITTLE_ENDIAN )

#define CopyLittleShort(dest, src) Com_Memcpy(dest, src, 2)
#define CopyLittleLong(dest, src) Com_Memcpy(dest, src, 4)
#define LittleShort
#define LittleLong
#define LittleFloat
#define BigShort(x) ShortSwap(x)
#define BigLong(x) LongSwap(x)
#define BigFloat(x) FloatSwap(&x)

#else

#error "Endianness not defined"

#endif

// Platform string

#ifdef NDEBUG
#define PLATFORM_STRING OS_STRING "-" ARCH_STRING
#else
#define PLATFORM_STRING OS_STRING "-" ARCH_STRING "-debug"
#endif

#if idx64
#ifdef _MSC_VER
#define _MSC_SSE2
#else
#define _GCC_SSE2
#endif
#endif // idx64

// Modifier for printing size_t values portably

#if (defined _WIN64) || defined(_WIN32)
#define PRIz "Iu"
#elif (defined Q3_VM)
#define PRIz "u"
#else
#define PRIz "z"
#endif

#endif
