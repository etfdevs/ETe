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

// q_shared.c -- stateless support routines that are included in each code dll
#include "q_shared.h"

float Com_ClampFloat( float min, float max, float value ) {
	if ( value < min ) {
		return min;
	}
	if ( value > max ) {
		return max;
	}
	return value;
}


int Com_ClampInt( int min, int max, int value ) {
	if ( value < min ) {
		return min;
	}
	if ( value > max ) {
		return max;
	}
	return value;
}


qboolean COM_SuffixCompare(const char *in, const char *suffixStr)
{
	int inlen, suflen;
	
	inlen = strlen(in);
	suflen = strlen(suffixStr);
	
	if(suflen <= inlen)
	{
		in += inlen - suflen;
		
		if(!Q_stricmp(in, suffixStr))
			return qtrue;
	}
	
	return qfalse;
}


/*
COM_FixPath()
unixifies a pathname
*/

void COM_FixPath( char *pathname ) {
	while ( *pathname )
	{
		if ( *pathname == '\\' ) {
			*pathname = '/';
		}
		pathname++;
	}
}


/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char	*last;
	
	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}


/*
============
COM_GetExtension
============
*/
const char *COM_GetExtension( const char *name )
{
	const char *dot = strrchr(name, '.'), *slash;
	if (dot && ((slash = strrchr(name, '/')) == NULL || slash < dot))
		return dot + 1;
	else
		return "";
}


/*
============
COM_StripExtension
============
*/
void COM_StripExtension( const char *in, char *out, int destsize )
{
	const char *dot = strrchr(in, '.'), *slash;

	if (dot && ((slash = strrchr(in, '/')) == NULL || slash < dot))
		destsize = (destsize < dot-in+1 ? destsize : dot-in+1);

	if ( in == out && destsize > 1 )
		out[destsize-1] = '\0';
	else
		Q_strncpyz(out, in, destsize);
}


/*
============
COM_CompareExtension

string compare the end of the strings and return qtrue if strings match
============
*/
qboolean COM_CompareExtension(const char *in, const char *ext)
{
	int inlen, extlen;
	
	inlen = strlen(in);
	extlen = strlen(ext);
	
	if(extlen <= inlen)
	{
		in += inlen - extlen;
		
		if(!Q_stricmp(in, ext))
			return qtrue;
	}
	
	return qfalse;
}


/*
==================
COM_DefaultExtension

if path doesn't have an extension, then append
 the specified one (which should include the .)
==================
*/
void COM_DefaultExtension( char *path, int maxSize, const char *extension )
{
	const char *dot = strrchr(path, '.'), *slash;
	if (dot && ((slash = strrchr(path, '/')) == NULL || slash < dot))
		return;
	else
		Q_strcat(path, maxSize, extension);
}


void COM_StripFilename( const char *in, char *out, int destSize ) {
	char *end;
	Q_strncpyz( out, in, destSize );
	end = COM_SkipPath( out );
	*end = 0;
}


/*
==================
COM_GenerateHashValue

used in renderer and filesystem
==================
*/
// ASCII lowcase conversion table with '\\' turned to '/' and '.' to '\0'
static const byte hash_locase[ 256 ] =
{
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
	0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
	0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x00,0x2f,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
	0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x5b,0x2f,0x5d,0x5e,0x5f,
	0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
	0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
	0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
	0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
	0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
	0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
	0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
	0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
	0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
	0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
	0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
	0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
	0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
	0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
	0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
	0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
	0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

unsigned long Com_GenerateHashValue( const char *fname, const unsigned int size )
{
	const byte *s;
	unsigned long hash;
	int		c;

	s = (byte*)fname;
	hash = 0;
	
	while ( (c = hash_locase[(byte)*s++]) != '\0' ) {
		hash = hash * 101 + c;
	}
	
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (size-1);

	return hash;
}


/*
============
Com_Split
============
*/
int Com_Split( char *in, char **out, int outsz, int delim )
{
	int c;
	char **o = out, **end = out + outsz;
	// skip leading spaces
	if ( delim >= ' ' ) {
		while( (c = *in) != '\0' && c <= ' ' )
			in++; 
	}
	*out = in; out++;
	while( out < end ) {
		while( (c = *in) != '\0' && c != delim )
			in++; 
		*in = '\0';
		if ( !c ) {
			// don't count last null value
			if ( out[-1][0] == '\0' )
				out--;
			break;
		}
		in++;
		// skip leading spaces
		if ( delim >= ' ' ) {
			while( (c = *in) != '\0' && c <= ' ' )
				in++; 
		}
		*out = in; out++;
	}
	// sanitize last value
	while( (c = *in) != '\0' && c != delim )
		in++; 
	*in = '\0';
	c = out - o;
	// set remaining out pointers
	while( out < end ) {
		*out = in; out++;
	}
	return c;
}


/*
==================
crc32_buffer
==================
*/
unsigned int crc32_buffer( const byte *buf, unsigned int len ) {
	static unsigned int crc32_table[256];
	static qboolean crc32_inited = qfalse;

	unsigned int crc = 0xFFFFFFFFUL;

	if ( !crc32_inited )
	{
		unsigned int c;
		int i, j;

		for (i = 0; i < 256; i++)
		{
			c = i;
			for ( j = 0; j < 8; j++ )
				c = (c & 1) ? (c >> 1) ^ 0xEDB88320UL : c >> 1;
			crc32_table[i] = c;
		}
		crc32_inited = qtrue;
	}

	while ( len-- )
	{
		crc = crc32_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
	}

	return crc ^ 0xFFFFFFFFUL;
}



//============================================================================
/*
==================
COM_BitCheck

  Allows bit-wise checks on arrays with more than one item (> 32 bits)
==================
*/
qboolean COM_BitCheck( const int array[], unsigned int bitNum ) {
	unsigned int i = bitNum / 32;
	unsigned int bitmask;
	bitNum  = bitNum % 32;
	bitmask = 1u << bitNum;
	return ((array[i] & bitmask) != 0);
}

/*
==================
COM_BitSet

  Allows bit-wise SETS on arrays with more than one item (> 32 bits)
==================
*/
void COM_BitSet( int array[], unsigned int bitNum ) {
	unsigned int i = bitNum / 32;
	unsigned int bitmask;
	bitNum  = bitNum % 32;
	bitmask = 1u << bitNum;

	array[i] |= bitmask;
}

/*
==================
COM_BitClear

  Allows bit-wise CLEAR on arrays with more than one item (> 32 bits)
==================
*/
void COM_BitClear( int array[], unsigned int bitNum ) {
	unsigned int i = bitNum / 32;
	unsigned int bitmask;
	bitNum  = bitNum % 32;
	bitmask = ~(1u << bitNum);

	array[i] &= bitmask;
}
//============================================================================

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

void CopyShortSwap(void *dest, void *src)
{
	byte *to = dest, *from = src;

	to[0] = from[1];
	to[1] = from[0];
}

void CopyLongSwap(void *dest, void *src)
{
	byte *to = dest, *from = src;

	to[0] = from[3];
	to[1] = from[2];
	to[2] = from[1];
	to[3] = from[0];
}

short   ShortSwap (short l)
{
	unsigned short tmp = l;
	unsigned short b1 = (tmp & 0x00ff) << 8;
	unsigned short b2 = (tmp & 0xff00) >> 8;

	return (b1 | b2);
}

short	ShortNoSwap (short l)
{
	return l;
}

int    LongSwap (int l)
{
	/* is compiled to bswap on gcc/clang/msvc */
	unsigned int tmp = l;
	unsigned int b1 = (tmp & 0x000000ff) << 24;
	unsigned int b2 = (tmp & 0x0000ff00) << 8;
	unsigned int b3 = (tmp & 0x00ff0000) >> 8;
	unsigned int b4 = (tmp & 0xff000000) >> 24;

	return (b1 | b2 | b3 | b4);
}

int	LongNoSwap (int l)
{
	return l;
}

int64_t Long64Swap (int64_t ll)
{
	uint64_t tmp = ll;
	uint64_t b1 = (tmp & 0x00000000000000ffull) << 56;
	uint64_t b2 = (tmp & 0x000000000000ff00ull) << 40;
	uint64_t b3 = (tmp & 0x0000000000ff0000ull) << 24;
	uint64_t b4 = (tmp & 0x00000000ff000000ull) << 8;
	uint64_t b5 = (tmp & 0x000000ff00000000ull) >> 8;
	uint64_t b6 = (tmp & 0x0000ff0000000000ull) >> 24;
	uint64_t b7 = (tmp & 0x00ff000000000000ull) >> 40;
	uint64_t b8 = (tmp & 0xff00000000000000ull) >> 56;

	return (b1 | b2 | b3 | b4 | b5 | b6 | b7 | b8);
}

int Long64NoSwap( int64_t ll )
{
	return ll;
}

float FloatSwap( const float *f ) 
{
	floatint_t out;

	out.f = *f;
	out.i = LongSwap( out.i );

	return out.f;
}

float FloatNoSwap( const float *f )
{
	return *f;
}

/*
============================================================================

PARSING

============================================================================
*/

static	char	com_token[MAX_TOKEN_CHARS];
static	char	com_parsename[MAX_TOKEN_CHARS];
static	int		com_lines;
static  int		com_tokenline;

// for complex parser
tokenType_t		com_tokentype;

static int backup_lines;
static int backup_tokenline;
static const char    *backup_text;
static tokenType_t		backup_tokentype;

void COM_BeginParseSession( const char *name )
{
	com_lines = 1;
	com_tokenline = 0;
	Com_sprintf(com_parsename, sizeof(com_parsename), "%s", name);
}


void COM_BackupParseSession( const char **data_p )
{
	backup_lines = com_lines;
	backup_tokenline = com_tokenline;
	backup_text = *data_p;
	backup_tokentype = com_tokentype;
}


void COM_RestoreParseSession( const char **data_p )
{
	com_lines = backup_lines;
	com_tokenline = backup_tokenline;
	*data_p = backup_text;
	com_tokentype = backup_tokentype;
}


/*void COM_SetCurrentParseLine( int line )
{
	com_lines = line;
}*/


int COM_GetCurrentParseLine( void )
{
	if ( com_tokenline )
	{
		return com_tokenline;
	}

	return com_lines;
}


const char *COM_Parse( const char **data_p )
{
	return COM_ParseExt( data_p, qtrue );
}


void Q_PRINTF_FUNC(1, 2) COM_ParseError( const char *format, ... )
{
	va_list argptr;
	static char string[4096];

	va_start( argptr, format );
	Q_vsnprintf (string, sizeof(string), format, argptr);
	va_end( argptr );

	Com_Printf( "ERROR: %s, line %d: %s\n", com_parsename, COM_GetCurrentParseLine(), string );
}


void Q_PRINTF_FUNC(1, 2) COM_ParseWarning( const char *format, ... )
{
	va_list argptr;
	static char string[4096];

	va_start( argptr, format );
	Q_vsnprintf (string, sizeof(string), format, argptr);
	va_end( argptr );

	Com_Printf( "WARNING: %s, line %d: %s\n", com_parsename, COM_GetCurrentParseLine(), string );
}


/*
==============
COM_Parse

Parse a token out of a string
Will never return NULL, just empty strings

If "allowLineBreaks" is qtrue then an empty
string will be returned if the next token is
a newline.
==============
*/
static const char *SkipWhitespace( const char *data, qboolean *hasNewLines ) {
	int c;

	while( (c = *data) <= ' ') {
		if( !c ) {
			return NULL;
		}
		if( c == '\n' ) {
			com_lines++;
			*hasNewLines = qtrue;
		}
		data++;
	}

	return data;
}


int COM_Compress( char *data_p ) {
	const char *in;
	char *out;
	int c;
	qboolean newline = qfalse, whitespace = qfalse;

	in = out = data_p;
	while ((c = *in) != '\0') {
		// skip double slash comments
		if ( c == '/' && in[1] == '/' ) {
			while (*in && *in != '\n') {
				in++;
			}
		// skip /* */ comments
		} else if ( c == '/' && in[1] == '*' ) {
			while ( *in && ( *in != '*' || in[1] != '/' ) ) 
				in++;
			if ( *in ) 
				in += 2;
			// record when we hit a newline
		} else if ( c == '\n' || c == '\r' ) {
			newline = qtrue;
			in++;
			// record when we hit whitespace
		} else if ( c == ' ' || c == '\t') {
			whitespace = qtrue;
			in++;
			// an actual token
		} else {
			// if we have a pending newline, emit it (and it counts as whitespace)
			if (newline) {
				*out++ = '\n';
				newline = qfalse;
				whitespace = qfalse;
			} else if (whitespace) {
				*out++ = ' ';
				whitespace = qfalse;
			}
			// copy quoted strings unmolested
			if (c == '"') {
				*out++ = c;
				in++;
				while (1) {
					c = *in;
					if (c && c != '"') {
						*out++ = c;
						in++;
					} else {
						break;
					}
				}
				if (c == '"') {
					*out++ = c;
					in++;
				}
			} else {
				*out++ = c;
				in++;
			}
		}
	}

	*out = '\0';

	return out - data_p;
}


const char *COM_ParseExt( const char **data_p, qboolean allowLineBreaks )
{
	int c = 0, len;
	qboolean hasNewLines = qfalse;
	const char *data;

	data = *data_p;
	len = 0;
	com_token[0] = '\0';
	com_tokenline = 0;

	// make sure incoming data is valid
	if ( !data )
	{
		*data_p = NULL;
		return com_token;
	}

	// RF, backup the session data so we can unget easily
	COM_BackupParseSession( data_p );

	while ( 1 )
	{
		// skip whitespace
		data = SkipWhitespace( data, &hasNewLines );
		if ( !data )
		{
			*data_p = NULL;
			return com_token;
		}
		if ( hasNewLines && !allowLineBreaks )
		{
			*data_p = data;
			return com_token;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while (*data && *data != '\n') {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c == '/' && data[1] == '*' )
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) )
			{
				if ( *data == '\n' )
				{
					com_lines++;
				}
				data++;
			}
			if ( *data )
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	// token starts on this line
	com_tokenline = com_lines;

	// handle quoted strings
	if ( c == '"' )
	{
		data++;
		while ( 1 )
		{
			c = *data++;
			if ( c == '\\' && *( data ) == '\"' )
			{
				// Arnout: string-in-string
				if ( len < ARRAY_LEN( com_token )-1 )
				{
					com_token[ len ] = '\"';
					len++;
				}
				data++;

				while ( 1 )
				{
					c = *data++;

					if ( !c )
					{
						com_token[ len ] = '\0';
						*data_p = data;
						break;
					}
					if ( ( c == '\\' && *( data ) == '\"' ) )
					{
						if ( len < ARRAY_LEN( com_token )-1 )
						{
							com_token[ len ] = '\"';
							len++;
						}
						data++;
						c = *data++;
						break;
					}
					if ( len < ARRAY_LEN( com_token )-1 )
					{
						com_token[ len ] = c;
						len++;
					}
				}
			}
			if ( c == '\"' || c == '\0' )
			{
				com_token[ len ] = '\0';
				*data_p = data;
				return com_token;
			}
			if ( c == '\n' )
			{
				com_lines++;
			}
			if ( len < ARRAY_LEN( com_token )-1 )
			{
				com_token[ len ] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if ( len < ARRAY_LEN( com_token )-1 )
		{
			com_token[ len ] = c;
			len++;
		}
		data++;
		c = *data;
	} while ( c > ' ' );

	com_token[ len ] = '\0';

	*data_p = data;
	return com_token;
}
	

/*
==============
COM_ParseComplex
==============
*/
char *COM_ParseComplex( const char **data_p, qboolean allowLineBreaks )
{
	static const byte is_separator[ 256 ] =
	{
	// \0 . . . . . . .\b\t\n . .\r . .
		1,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,
	//  . . . . . . . . . . . . . . . .
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	//    ! " # $ % & ' ( ) * + , - . /
		1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0, // excl. '-' '.' '/'
	//  0 1 2 3 4 5 6 7 8 9 : ; < = > ?
		0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,
	//  @ A B C D E F G H I J K L M N O
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	//  P Q R S T U V W X Y Z [ \ ] ^ _
		0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,0, // excl. '\\' '_'
	//  ` a b c d e f g h i j k l m n o
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	//  p q r s t u v w x y z { | } ~ 
		0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1
	};

	int c, len, shift;
	const byte *str;

	str = (byte*)*data_p;
	len = 0; 
	shift = 0; // token line shift relative to com_lines
	com_tokentype = TK_GENERIC;
	
__reswitch:
	switch ( *str )
	{
	case '\0':
		com_tokentype = TK_EOF;
		break;

	// whitespace
	case ' ':
	case '\t':
		str++;
		while ( (c = *str) == ' ' || c == '\t' )
			str++;
		goto __reswitch;

	// newlines
	case '\n':
	case '\r':
	com_lines++;
		if ( *str == '\r' && str[1] == '\n' )
			str += 2; // CR+LF
		else
			str++;
		if ( !allowLineBreaks ) {
			com_tokentype = TK_NEWLINE;
			break;
		}
		goto __reswitch;

	// comments, single slash
	case '/':
		// until end of line
		if ( str[1] == '/' ) {
			str += 2;
			while ( (c = *str) != '\0' && c != '\n' && c != '\r' )
				str++;
			goto __reswitch;
		}

		// comment
		if ( str[1] == '*' ) {
			str += 2;
			while ( (c = *str) != '\0' && ( c != '*' || str[1] != '/' ) ) {
				if ( c == '\n' || c == '\r' ) {
					com_lines++;
					if ( c == '\r' && str[1] == '\n' ) // CR+LF?
						str++;
				}
				str++;
			}
			if ( c != '\0' && str[1] != '\0' ) {
				str += 2;
			} else {
				// FIXME: unterminated comment?
			}
			goto __reswitch;
		}

		// single slash
		com_token[ len++ ] = *str++;
		break;
	
	// quoted string?
	case '"':
		str++; // skip leading '"'
		//com_tokenline = com_lines;
		while ( (c = *str) != '\0' && c != '"' ) {
			if ( c == '\n' || c == '\r' ) {
				com_lines++; // FIXME: unterminated quoted string?
				shift++;
			}
			if ( len < MAX_TOKEN_CHARS-1 ) // overflow check
				com_token[ len++ ] = c;
			str++;
		}
		if ( c != '\0' ) {
			str++; // skip ending '"'
		} else {
			// FIXME: unterminated quoted string?
		}
		com_tokentype = TK_QUOTED;
		break;

	// single tokens:
	case '+': case '`':
	/*case '*':*/ case '~':
	case '{': case '}':
	case '[': case ']':
	case '?': case ',':
	case ':': case ';':
	case '%': case '^':
		com_token[ len++ ] = *str++;
		break;

	case '*':
		com_token[ len++ ] = *str++;
		com_tokentype = TK_MATCH;
		break;

	case '(':
		com_token[ len++ ] = *str++;
		com_tokentype = TK_SCOPE_OPEN;
		break;

	case ')':
		com_token[ len++ ] = *str++;
		com_tokentype = TK_SCOPE_CLOSE;
		break;

	// !, !=
	case '!':
		com_token[ len++ ] = *str++;
		if ( *str == '=' ) {
			com_token[ len++ ] = *str++;
			com_tokentype = TK_NEQ;
		}
		break;

	// =, ==
	case '=':
		com_token[ len++ ] = *str++;
		if ( *str == '=' ) {
			com_token[ len++ ] = *str++;
			com_tokentype = TK_EQ;
		}
		break;

	// >, >=
	case '>':
		com_token[ len++ ] = *str++;
		if ( *str == '=' ) {
			com_token[ len++ ] = *str++;
			com_tokentype = TK_GTE;
		} else {
			com_tokentype = TK_GT;
		}
		break;

	//  <, <=
	case '<':
		com_token[ len++ ] = *str++;
		if ( *str == '=' ) {
			com_token[ len++ ] = *str++;
			com_tokentype = TK_LTE;
		} else {
			com_tokentype = TK_LT;
		}
		break;

	// |, ||
	case '|':
		com_token[ len++ ] = *str++;
		if ( *str == '|' ) {
			com_token[ len++ ] = *str++;
			com_tokentype = TK_OR;
		}
		break;

	// &, &&
	case '&':
		com_token[ len++ ] = *str++;
		if ( *str == '&' ) {
			com_token[ len++ ] = *str++;
			com_tokentype = TK_AND;
		}
		break;

	// rest of the charset
	default:
		com_token[ len++ ] = *str++;
		while ( !is_separator[ (c = *str) ] ) {
			if ( len < MAX_TOKEN_CHARS-1 )
				com_token[ len++ ] = c;
			str++;
		}
		com_tokentype = TK_STRING;
		break;

	} // switch ( *str )

	com_tokenline = com_lines - shift;
	com_token[ len ] = '\0';
	*data_p = ( char * )str;
	return com_token;
}


/*
==================
COM_MatchToken
==================
*/
static void COM_MatchToken( const char **buf_p, const char *match ) {
	const char *token;

	token = COM_Parse( buf_p );
	if ( strcmp( token, match ) ) {
		Com_Error( ERR_DROP, "MatchToken: %s != %s", token, match );
	}
}


/*
=================
SkipBracedSection

The next token should be an open brace or set depth to 1 if already parsed it.
Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
qboolean SkipBracedSection( const char **program, int depth ) {
	const char			*token;

	do {
		token = COM_ParseExt( program, qtrue );
		if( token[1] == 0 ) {
			if( token[0] == '{' ) {
				depth++;
			}
			else if( token[0] == '}' ) {
				depth--;
			}
		}
	} while( depth && *program );

	return ( depth == 0 );
}


/*
=================
SkipRestOfLine
=================
*/
void SkipRestOfLine( const char **data ) {
	const char *p;
	int		c;

	p = *data;

	if ( !*p )
		return;

	while ( (c = *p) != '\0' ) {
		p++;
		if ( c == '\n' ) {
			com_lines++;
			break;
		}
	}

	*data = p;
}


void Parse1DMatrix( const char **buf_p, int x, float *m ) {
	const char	*token;
	int		i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < x; i++) {
		token = COM_Parse( buf_p );
		m[i] = Q_atof( token );
	}

	COM_MatchToken( buf_p, ")" );
}


void Parse2DMatrix( const char **buf_p, int y, int x, float *m ) {
	int		i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < y ; i++) {
		Parse1DMatrix (buf_p, x, m + i * x);
	}

	COM_MatchToken( buf_p, ")" );
}


void Parse3DMatrix( const char **buf_p, int z, int y, int x, float *m ) {
	int		i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < z ; i++) {
		Parse2DMatrix (buf_p, y, x, m + i * x*y);
	}

	COM_MatchToken( buf_p, ")" );
}


/*
===============
Com_ParseInfos
===============
*/
int Com_ParseInfos( const char *buf, int max, char infos[][MAX_INFO_STRING] ) {
	const char  *token;
	int count;
	char key[MAX_TOKEN_CHARS];

	count = 0;

	while ( 1 ) {
		token = COM_Parse( &buf );
		if ( !token[0] ) {
			break;
		}
		if ( strcmp( token, "{" ) ) {
			Com_Printf( "Missing { in info file\n" );
			break;
		}

		if ( count == max ) {
			Com_Printf( "Max infos exceeded\n" );
			break;
		}

		infos[count][0] = 0;
		while ( 1 ) {
			token = COM_Parse( &buf );
			if ( !token[0] ) {
				Com_Printf( "Unexpected end of info file\n" );
				break;
			}
			if ( !strcmp( token, "}" ) ) {
				break;
			}
			Q_strncpyz( key, token, sizeof( key ) );

			token = COM_ParseExt( &buf, qfalse );
			if ( !token[0] ) {
				token = "<NULL>";
			}
			Info_SetValueForKey( infos[count], key, token );
		}
		count++;
	}

	return count;
}


static int Hex( char c )
{
	if ( c >= '0' && c <= '9' ) {
		return c - '0';
	}
	else
	if ( c >= 'A' && c <= 'F' ) {
		return 10 + c - 'A';
	}
	else
	if ( c >= 'a' && c <= 'f' ) {
		return 10 + c - 'a';
	}

	return -1;
}


/*
===================
Com_HexStrToInt
===================
*/
int Com_HexStrToInt( const char *str )
{
	if ( !str )
		return -1;

	// check for hex code
	if ( str[ 0 ] == '0' && str[ 1 ] == 'x' && str[ 2 ] != '\0' )
	{
		int i, digit, n = 0, len = strlen( str );

		for( i = 2; i < len; i++ )
		{
			n *= 16;

			digit = Hex( str[ i ] );

			if ( digit < 0 )
				return -1;

			n += digit;
		}

		return n;
	}

	return -1;
}


qboolean Com_GetHashColor( const char *str, byte *color )
{
	int i, len, hex[6];

	color[0] = color[1] = color[2] = 0;

	if ( *str++ != '#' ) {
		return qfalse;
	}

	len = (int)strlen( str );
	if ( len <= 0 || len > 6 ) {
		return qfalse;
	}

	for ( i = 0; i < len; i++ ) {
		hex[i] = Hex( str[i] );
		if ( hex[i] < 0 ) {
			return qfalse;
		}
	}

	switch ( len ) {
		case 3: // #rgb
			color[0] = hex[0] << 4 | hex[0];
			color[1] = hex[1] << 4 | hex[1];
			color[2] = hex[2] << 4 | hex[2];
			break;
		case 6: // #rrggbb
			color[0] = hex[0] << 4 | hex[1];
			color[1] = hex[2] << 4 | hex[3];
			color[2] = hex[4] << 4 | hex[5];
			break;
		default: // unsupported format
			return qfalse;
	}

	return qtrue;
}


/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

const byte locase[ 256 ] = {
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
	0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
	0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
	0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f,
	0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
	0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
	0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
	0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
	0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
	0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
	0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
	0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
	0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
	0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
	0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
	0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
	0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
	0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
	0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
	0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
	0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

const byte upcase[ 256 ] = {
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
	0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
	0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
	0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
	0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
	0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
	0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
	0x60,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
	0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
	0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
	0x58,0x59,0x5a,0x7b,0x7c,0x7d,0x7e,0x7f,
	0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
	0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
	0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
	0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
	0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
	0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
	0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
	0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
	0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
	0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
	0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
	0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
	0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
	0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
	0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
	0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff,
};


int Q_isprint( int c )
{
	if ( c >= 0x20 && c <= 0x7E )
		return ( 1 );
	return ( 0 );
}


int Q_islower( int c )
{
	if (c >= 'a' && c <= 'z')
		return ( 1 );
	return ( 0 );
}


int Q_isupper( int c )
{
	if (c >= 'A' && c <= 'Z')
		return ( 1 );
	return ( 0 );
}


int Q_isalpha( int c )
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return ( 1 );
	return ( 0 );
}


int Q_isnumeric( int c )
{
	if (c >= '0' && c <= '9')
		return ( 1 );
	return ( 0 );
}


int Q_isalphanumeric( int c )
{
	if ( Q_isalpha( c ) || Q_isnumeric( c ) )
		return( 1 );
	return ( 0 );
}


int Q_isforfilename( int c )
{
	// space not allowed in filename
	if ( ( Q_isalphanumeric( c ) || c == '_' ) && c != ' ' )
		return( 1 );

	return ( 0 );
}


qboolean Q_isanumber( const char *s )
{
    char *p;

	if( *s == '\0' )
        return qfalse;

	strtod( s, &p );

    return *p == '\0';
}


qboolean Q_isintegral( float f )
{
    return (int)f == f;
}


#if defined (_MSC_VER) && _MSC_VER < 1900
/*
=============
Q_vsnprintf
 
Special wrapper function for Microsoft's broken _vsnprintf() function. mingw-w64
however, uses Microsoft's broken _vsnprintf() function.
=============
*/
int Q_vsnprintf( char *str, size_t size, const char *format, va_list ap )
{
	int retval;
	
	retval = _vsnprintf( str, size, format, ap );

	if ( retval < 0 || (size_t)retval == size )
	{
		// Microsoft doesn't adhere to the C99 standard of vsnprintf,
		// which states that the return value must be the number of
		// bytes written if the output string had sufficient length.
		//
		// Obviously we cannot determine that value from Microsoft's
		// implementation, so we have no choice but to return size.
		
		str[size - 1] = '\0';
		return size;
	}
	
	return retval;
}
#endif


/*
=============
Q_strncpyz
 
Safe strncpy that ensures a trailing zero
=============
*/
void Q_strncpyz( char *dest, const char *src, int destsize ) 
{
	if ( !dest )
	{
		Com_Error( ERR_FATAL, "Q_strncpyz: NULL dest" );
	}

	if ( !src )
	{
		Com_Error( ERR_FATAL, "Q_strncpyz: NULL src" );
	}

	if ( destsize < 1 )
	{
		Com_Error(ERR_FATAL,"Q_strncpyz: destsize < 1" );
	}
#if 0 // WAS 1 in Q3e
	// do not fill whole remaining buffer with zeros
	// this is obvious behavior change but actually it may affect only buggy QVMs
	// which passes overlapping or short buffers to cvar reading routines
	// what is rather good than bad because it will no longer cause overwrites, maybe
	while ( --destsize > 0 && (*dest++ = *src++) != '\0' )
		;
	*dest = '\0';
#else
	strncpy( dest, src, destsize-1 );
	dest[ destsize-1 ] = '\0';
#endif
}


/*
=============
Q_strncpy

allows src and dest to be overlapped for QVM compatibility purposes
=============
*/
char *Q_strncpy( char *dest, char *src, int destsize )
{
	char *s = src, *start = dest;
	int src_len;

	while ( *s != '\0' )
		++s;
	src_len = (int)(s - src);
	
	if ( src_len > destsize ) {
		src_len = destsize;
	}
	destsize -= src_len;

	if ( dest > src && dest < src + src_len ) {
		int i;
#ifdef _DEBUG
		Com_Printf( S_COLOR_YELLOW "Q_strncpy: overlapped (dest > src) buffers\n" );
#endif
		for ( i = src_len - 1; i >= 0; --i ) {
			dest[i] = src[i]; // back overlapping
		}
		dest += src_len;
	} else {
#ifdef _DEBUG
		if ( src >= dest && src < dest + src_len ) {
			Com_Printf( S_COLOR_YELLOW "Q_strncpy: overlapped (src >= dst) buffers\n" );
#ifdef _MSC_VER
			// __debugbreak();
#endif 
		}
#endif
		while ( src_len > 0 ) {
			*dest++ = *src++;
			--src_len;
		}
	}

	while ( destsize > 0 ) {
		*dest++ = '\0';
		--destsize;
	}

	return start;
}


/*
=============
Q_stricmpn
=============
*/
int Q_stricmpn( const char *s1, const char *s2, int n ) {
	unsigned char c1, c2;

	if ( s1 == NULL )
	{
		if ( s2 == NULL )
			return 0;
		else
			return -1;
	}
	else if ( s2 == NULL )
		return 1;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( !n-- )
			return 0;		// strings are equal until end point

		if ( c1 != c2 )
		{
			if ( c1 <= 'Z' && c1 >= 'A' )
				c1 += ('a' - 'A');

			if ( c2 <= 'Z' && c2 >= 'A' )
				c2 += ('a' - 'A');

			if ( c1 != c2 )
				return c1 < c2 ? -1 : 1;
		}
	}
	while ( c1 != '\0' );
	
	return 0;		// strings are equal
}


int Q_strncmp( const char *s1, const char *s2, int n ) {
	unsigned char c1, c2;

	if ( s1 == NULL )
	{
		if ( s2 == NULL )
			return 0;
		else
			return -1;
	}
	else if ( s2 == NULL )
		return 1;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( !n-- )
			return 0;		// strings are equal until end point

		if ( c1 != c2 )
			return c1 < c2 ? -1 : 1;
	}
	while ( c1 != '\0' );

	return 0;		// strings are equal
}


qboolean Q_streq( const char *s1, const char *s2 ) {
	unsigned char c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;
		if ( c1 != c2 )
			return qfalse;
	}
	while ( c1 != '\0' );

	return qtrue;
}


int Q_strcmp( const char *s1, const char *s2 ) {
	unsigned char c1, c2;

	if ( s1 == NULL )
	{
		if ( s2 == NULL )
			return 0;
		else
			return -1;
	}
	else if ( s2 == NULL )
		return 1;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( c1 != c2 )
			return c1 < c2 ? -1 : 1;
	}
	while ( c1 != '\0' );

	return 0;		// strings are equal
}


int Q_stricmp( const char *s1, const char *s2 ) 
{
	unsigned char c1, c2;

	if ( s1 == NULL )
	{
		if ( s2 == NULL )
			return 0;
		else
			return -1;
	}
	else if ( s2 == NULL )
		return 1;

	do 
	{
		c1 = *s1++;
		c2 = *s2++;

		if ( c1 != c2 )
		{
			if ( c1 <= 'Z' && c1 >= 'A' )
				c1 += ('a' - 'A');

			if ( c2 <= 'Z' && c2 >= 'A' )
				c2 += ('a' - 'A');

			if ( c1 != c2 )
				return c1 < c2 ? -1 : 1;
		}
	}
	while ( c1 != '\0' );

	return 0;
}


char *Q_strlwr( char *s1 ) {
	char	*s;

	s = s1;
	while ( *s ) {
		*s = locase[(byte)*s];
		s++;
	}
	return s1;
}


char *Q_strupr( char *s1 ) {
	char	*s;

	s = s1;
	while ( *s ) {
		*s = upcase[(byte)*s];
		/*if ( *s >= 'a' && *s <= 'z' )
			*s = *s - 'a' + 'A';*/
		s++;
	}
	return s1;
}


// never goes past bounds or leaves without a terminating 0
void Q_strcat( char *dest, int size, const char *src ) {
	int		l1;

	l1 = strlen( dest );
	if ( l1 >= size ) {
		Com_Error( ERR_FATAL, "Q_strcat: already overflowed" );
	}
	Q_strncpyz( dest + l1, src, size - l1 );
}


char *Q_stradd( char *dst, const char *src )
{
	char c;
	while ( (c = *src++) != '\0' )
		*dst++ = c;
	*dst = '\0';
	return dst;
}


/*
* Find the first occurrence of find in s.
*/
const char *Q_stristr( const char *s, const char *find)
{
	size_t len = strlen(find);
	for (; *s; s++) if (!Q_stricmpn(s, find, len)) return /*(char *)*/s;
	return NULL;
}


int Q_replace( const char *str1, const char *str2, char *src, int max_len ) 
{
	int len1, len2, d, count;
	const char *s0, *s1, *s2, *max;
	char *match, *dst;

	match = strstr( src, str1 );

	if ( !match )
		return 0;

	count = 0; // replace count

    len1 = strlen( str1 );
    len2 = strlen( str2 );
    d = len2 - len1;

    if ( d > 0 ) // expand and replace mode    
    {
        max = src + max_len;
        src += strlen( src );

        do  
        {
            // expand source string
			s1 = src;
            src += d;
            if ( src >= max )
                return count;
            dst = src;
            
            s0 = match + len1;

            while ( s1 >= s0 )
                *dst-- = *s1--;
			
			// replace match
            s2 = str2;
			while ( *s2 ) {
                *match++ = *s2++;
			}
            match = strstr( match, str1 );

            count++;
        }
        while ( match );

        return count;
    } 
    else
    if ( d < 0 ) // shrink and replace mode
    {
        do 
        {
            // shrink source string
            s1 = match + len1;
            dst = match + len2;
            while ( (*dst++ = *s1++) != '\0' );
			
			//replace match
            s2 = str2;
			while ( *s2 ) {
				*match++ = *s2++;
			}

            match = strstr( match, str1 );

            count++;
        } 
        while ( match );

        return count;
    }
    else
    do  // just replace match
    {
        s2 = str2;
		while ( *s2 ) {
			*match++ = *s2++;
		}

        match = strstr( match, str1 );
        count++;
	} 
    while ( match );

	return count;
}


int Q_PrintStrlen( const char *string ) {
	int			len;
	const char	*p;

	if( !string ) {
		return 0;
	}

	len = 0;
	p = string;
	while( *p ) {
		if( Q_IsColorString( p ) ) {
			p += 2;
			continue;
		}
		p++;
		len++;
	}

	return len;
}


char *Q_CleanStr( char *string ) {
	char*	d;
	char*	s;
	int		c;

	s = string;
	d = string;
	while ((c = *s) != 0 ) {
		if ( Q_IsColorString( s ) ) {
			s++;
		}		
		else if ( c >= 0x20 && c <= 0x7E ) {
			*d++ = c;
		}
		s++;
	}
	*d = '\0';

	return string;
}

// strips whitespaces and bad characters
qboolean Q_isBadDirChar( char c ) {
	const char badchars[] = { ';', '&', '(', ')', '|', '<', '>', '*', '?', '[', ']', '~', '+', '@', '!', '\\', '/', ' ', '\'', '\"', '\0' };
	int i;

	for ( i = 0; badchars[i] != '\0'; i++ ) {
		if ( c == badchars[i] ) {
			return qtrue;
		}
	}

	return qfalse;
}

char *Q_CleanDirName( char *dirname ) {
	char*   d;
	char*   s;

	s = dirname;
	d = dirname;

	// clear trailing .'s
	while ( *s == '.' ) {
		s++;
	}

	while ( *s != '\0' ) {
		if ( !Q_isBadDirChar( *s ) ) {
			*d++ = *s;
		}
		s++;
	}
	*d = '\0';

	return dirname;
}

int Q_CountChar(const char *string, char tocount)
{
	int count;
	
	for(count = 0; *string; string++)
	{
		if(*string == tocount)
			count++;
	}
	
	return count;
}

// ENSI NOTE use combination of Q3e and ioquake3 instead??
int Q_PRINTF_FUNC( 3, 4 ) QDECL Com_sprintf( char *dest, int size, const char *fmt, ... ) {
	int ret;
	va_list argptr;

	va_start( argptr,fmt );
	ret = Q_vsnprintf( dest, size, fmt, argptr );
	va_end( argptr );
	if ( ret == -1 ) {
		Com_Printf( "%s: overflow of %i bytes buffer\n", __func__, size );
		return 0;
	}
	return ret;
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday

Ridah, modified this into a circular list, to further prevent stepping on
previous strings
============
*/
const char * Q_PRINTF_FUNC(1, 2) QDECL va( const char *format, ... ) 
{
	char	*buf;
	va_list		argptr;
	#define MAX_VA_STRING   32000
	static int	index = 0;
	static char	string[2][MAX_VA_STRING];	// in case va is called by nested functions
	int ret;

	buf = string[ index ];
	index ^= 1;

	va_start( argptr, format );
	ret = Q_vsnprintf( buf, sizeof(string[0]), format, argptr );
	va_end( argptr );

	//if ( ( len = strlen( temp_buffer ) ) >= MAX_VA_STRING ) {
	if ( ret == -1 ) {
		Com_Printf( "%s: overflow of %i bytes buffer\n", __func__, MAX_VA_STRING );
		//Com_Error( ERR_DROP, "Attempted to overrun string in call to va()\n" );
	}

	return buf;
}


/*
============
Com_TruncateLongString

Assumes buffer is at least TRUNCATE_LENGTH big
============
*/
void Com_TruncateLongString( char *buffer, const char *s )
{
	int length = strlen( s );

	if( length <= TRUNCATE_LENGTH )
		Q_strncpyz( buffer, s, TRUNCATE_LENGTH );
	else
	{
		Q_strncpyz( buffer, s, ( TRUNCATE_LENGTH / 2 ) - 3 );
		Q_strcat( buffer, TRUNCATE_LENGTH, " ... " );
		Q_strcat( buffer, TRUNCATE_LENGTH, s + length - ( TRUNCATE_LENGTH / 2 ) + 3 );
	}
}

/*
=============
TempVector

(SA) this is straight out of g_utils.c around line 210

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float   *tv( float x, float y, float z ) {
	static int index;
	static vec3_t vecs[8];
	float   *v;

	// use an array so that multiple tempvectors won't collide
	// for a while
	v = vecs[index];
	index = ( index + 1 ) & 7;

	v[0] = x;
	v[1] = y;
	v[2] = z;

	return v;
}

/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

static qboolean Q_strkey( const char *str, const char *key, int key_len )
{
	int i;

	for ( i = 0; i < key_len; i++ )
	{
		if ( locase[ (byte)str[i] ] != locase[ (byte)key[i] ] )
		{
			return qfalse;
		}
	}

	return qtrue;
}


/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
const char *Info_ValueForKey( const char *s, const char *key )
{
	static	char value[2][BIG_INFO_VALUE];	// use two buffers so compares
											// work without stomping on each other
	static	int	valueindex = 0;
	const char *v, *pkey;
	char	*o, *o2;
	int		klen, len;
	
	if ( !s || !key || !*key )
		return "";

	klen = (int)strlen( key );

	if ( *s == '\\' )
		s++;

	while (1)
	{
		pkey = s;
		while ( *s != '\\' )
		{
			if ( *s == '\0' )
				return "";
			++s;
		}
		len = (int)(s - pkey);
		s++; // skip '\\'

		v = s;
		while ( *s != '\\' && *s !='\0' )
			s++;

		if ( len == klen && Q_strkey( pkey, key, klen ) )
		{
			o = o2 = value[ valueindex ^= 1 ];
			if ( (int)(s - v) >= BIG_INFO_VALUE )
			{
				Com_Error( ERR_DROP, "Info_ValueForKey: oversize infostring [%s] [%s]", s, key );
			}
			else 
			{
				while ( v < s )
					*o++ = *v++;
			}
			*o = '\0';
			return o2;
		}

		if ( *s == '\0' )
			break;

		s++;
	}

	return "";
}


#define MAX_INFO_TOKENS ((MAX_INFO_STRING/3)+2)

static const char *info_keys[ MAX_INFO_TOKENS ];
static const char *info_values[ MAX_INFO_TOKENS ];
static int info_tokens;

/*
===================
Info_Tokenize

Tokenizes all key/value pairs from specified infostring
NOT suitable for big infostrings
===================
*/
void Info_Tokenize( const char *s )
{
	static char tokenBuffer[ MAX_INFO_STRING ];
	char *o = tokenBuffer;

	info_tokens = 0;
	*o = '\0';

	for ( ;; )
	{
		while ( *s == '\\' ) // skip leading/trailing separators
			s++;

		if ( *s == '\0' )
			break;

		info_keys[ info_tokens ] = o;
		while ( *s != '\\' )
		{
			if ( *s == '\0' )
			{
				*o = '\0'; // terminate key
				info_values[ info_tokens++ ] = o;
				return;
			}
			*o++ = *s++;
		}
		*o++ = '\0'; // terminate key
		s++; // skip '\\'

		info_values[ info_tokens++ ] = o;
		while ( *s != '\\' && *s != '\0' )
		{
			*o++ = *s++;
		}
		*o++ = '\0';
	}
}


/*
===================
Info_ValueForKeyToken

Fast lookup from tokenized infostring
===================
*/
const char *Info_ValueForKeyToken( const char *key )
{
	int i;

	for ( i = 0; i < info_tokens; i++ ) 
	{
		if ( Q_stricmp( info_keys[ i ], key ) == 0 )
		{
			return info_values[ i ];
		}
	}

	return "";
}


/*
===================
Info_NextPair

Used to iterate through all the key/value pairs in an info string
===================
*/
const char *Info_NextPair( const char *s, char *key, char *value ) {
	char	*o;

	if ( *s == '\\' ) {
		s++;
	}

	key[0] = '\0';
	value[0] = '\0';

	o = key;
	while ( *s != '\\' ) {
		if ( !*s ) {
			*o = '\0';
			return s;
		}
		*o++ = *s++;
	}
	*o = '\0';
	s++;

	o = value;
	while ( *s != '\\' && *s ) {
		*o++ = *s++;
	}
	*o = '\0';

	return s;
}


/*
===================
Info_RemoveKey

return removed character count
===================
*/
int Info_RemoveKey( char *s, const char *key )
{
	char *start;
	const char *pkey;
	int	key_len, len, ret;

	key_len = (int)strlen( key );
	ret = 0;

	while ( 1 ) {
		start = s;
		if ( *s == '\\' ) {
			++s;
		}
		pkey = s;
		while ( *s != '\\' ) {
			if ( *s == '\0' ) {
				if ( s != start ) {
					// remove any trailing empty keys
					*start = '\0';
					ret += (int)(s - start);
				}
				return ret;
			}
			++s;
		}
		len = (int)(s - pkey);
		++s; // skip '\\'

		while ( *s != '\\' && *s != '\0' ) {
			++s;
		}

		if ( len == key_len && Q_strkey( pkey, key, key_len ) ) {
			memmove( start, s, strlen( s ) + 1 ); // remove this part
			ret += (int)(s - start);
			s = start;
		}

		if ( *s == '\0' ) {
			break;
		}

	}

	return ret;
}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate( const char *s )
{
	for ( ;; )
	{
		switch ( *s++ )
		{
		case '\0':
			return qtrue;
		case '\"':
		case ';':
			return qfalse;
		default:
			break;
		}
	}
}


/*
==================
Info_ValidateKeyValue

Some characters are illegal in key values because they
can mess up the server's parsing
==================
*/
qboolean Info_ValidateKeyValue( const char *s )
{
	for ( ;; )
	{
		switch ( *s++ )
		{
		case '\0':
			return qtrue;
		case '\\':
		case '\"':
		case ';':
			return qfalse;
		default:
			break;
		}
	}
}


/*
==================
Info_SetValueForKey_s

Changes or adds a key/value pair
==================
*/
qboolean Info_SetValueForKey_s( char *s, int slen, const char *key, const char *value ) {
	char	newi[BIG_INFO_STRING+2];
	int		len1, len2;

	len1 = (int)strlen( s );

	if ( len1 >= slen ) {
		Com_Printf( S_COLOR_YELLOW "Info_SetValueForKey(%s): oversize infostring\n", key );
		return qfalse;
	}

	if ( !key || !Info_ValidateKeyValue( key ) || *key == '\0' ) {
		Com_Printf( S_COLOR_YELLOW "Invalid key name: '%s'\n", key );
		return qfalse;
	}

	if ( value && !Info_ValidateKeyValue( value ) ) {
		Com_Printf( S_COLOR_YELLOW "Invalid value name: '%s'\n", value );
		return qfalse;
	}

	len1 -= Info_RemoveKey( s, key );
	if ( value == NULL || *value == '\0' ) {
		return qtrue;
	}

	len2 = Com_sprintf( newi, sizeof( newi ), "\\%s\\%s", key, value );

	if ( len1 + len2 >= slen ) {
		Com_Printf( S_COLOR_YELLOW "Info string length exceeded for key '%s'\n", key );
		return qfalse;
	}

	strcpy( s + len1, newi );
	return qtrue;
}


//====================================================================

/*
==================
Com_CharIsOneOfCharset
==================
*/
static qboolean Com_CharIsOneOfCharset( char c, const char *set )
{
	int i, n = (int)(strlen(set));

	for( i = 0; i < n; i++ )
	{
		if( set[ i ] == c )
			return qtrue;
	}

	return qfalse;
}


/*
==================
Com_SkipCharset
==================
*/
const char *Com_SkipCharset( const char *s, const char *sep )
{
	const char	*p = s;

	while( p )
	{
		if( Com_CharIsOneOfCharset( *p, sep ) )
			p++;
		else
			break;
	}

	return p;
}


/*
==================
Com_SkipTokens
==================
*/
const char *Com_SkipTokens( const char *s, int numTokens, const char *sep )
{
	int		sepCount = 0;
	const char	*p = s;

	while( sepCount < numTokens )
	{
		if( Com_CharIsOneOfCharset( *p++, sep ) )
		{
			sepCount++;
			while( Com_CharIsOneOfCharset( *p, sep ) )
				p++;
		}
		else if( *p == '\0' )
			break;
	}

	if( sepCount == numTokens )
		return p;
	else
		return s;
}

void *Q_LinearSearch( const void *key, const void *ptr, size_t count,
	size_t size, cmpFunc_t cmp )
{
	size_t i;
	for ( i = 0; i < count; i++ )
	{
		if ( cmp( key, ptr ) == 0 ) return (void *)ptr;
		ptr = (const char *)ptr + size;
	}
	return NULL;
}

