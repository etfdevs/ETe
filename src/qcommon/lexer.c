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


/*****************************************************************************
 * name:		lexer.c
 *
 * desc:		lexicographical parser
 *
 *
 *****************************************************************************/

#include "q_shared.h"
#include "qcommon.h"
#include "lexer.h"

#define PUNCTABLE

//longer punctuations first
static punctuation_t default_punctuations[] =
{
	//binary operators
	{">>=",P_RSHIFT_ASSIGN},
	{"<<=",P_LSHIFT_ASSIGN},
	//
	{"...",P_PARMS},
	//C++20 spaceship 3-way comparison operator
	//{"<=>",P_SPACESHIP},
	//define merge operator
	{"##",P_PRECOMPMERGE},
	//logic operators
	{"&&",P_LOGIC_AND},
	{"||",P_LOGIC_OR},
	{">=",P_LOGIC_GEQ},
	{"<=",P_LOGIC_LEQ},
	{"==",P_LOGIC_EQ},
	{"!=",P_LOGIC_UNEQ},
	//arithmatic operators
	{"*=",P_MUL_ASSIGN},
	{"/=",P_DIV_ASSIGN},
	{"%=",P_MOD_ASSIGN},
	{"+=",P_ADD_ASSIGN},
	{"-=",P_SUB_ASSIGN},
	{"++",P_INC},
	{"--",P_DEC},
	//binary operators
	{"&=",P_BIN_AND_ASSIGN},
	{"|=",P_BIN_OR_ASSIGN},
	{"^=",P_BIN_XOR_ASSIGN},
	{">>",P_RSHIFT},
	{"<<",P_LSHIFT},
	//reference operators
	{"->",P_POINTERREF},
	//C++
	{"::",P_CPP1},
	{".*",P_CPP2},
	//arithmatic operators
	{"*",P_MUL},
	{"/",P_DIV},
	{"%",P_MOD},
	{"+",P_ADD},
	{"-",P_SUB},
	{"=",P_ASSIGN},
	//binary operators
	{"&",P_BIN_AND},
	{"|",P_BIN_OR},
	{"^",P_BIN_XOR},
	{"~",P_BIN_NOT},
	//logic operators
	{"!",P_LOGIC_NOT},
	{">",P_LOGIC_GREATER},
	{"<",P_LOGIC_LESS},
	//reference operator
	{".",P_REF},
	//seperators
	{",",P_COMMA},
	{";",P_SEMICOLON},
	//label indication
	{":",P_COLON},
	//if statement
	{"?",P_QUESTIONMARK},
	//embracements
	{"(",P_PARENTHESESOPEN},
	{")",P_PARENTHESESCLOSE},
	{"{",P_BRACEOPEN},
	{"}",P_BRACECLOSE},
	{"[",P_SQBRACKETOPEN},
	{"]",P_SQBRACKETCLOSE},
	//
	{"\\",P_BACKSLASH},
	//precompiler operator
	{"#",P_PRECOMP},
	{"$",P_DOLLAR},
	{NULL, 0}
};

static int default_punctuationtable[256];
static int default_nextpunctuation[ARRAY_LEN(default_punctuations)];
static qboolean default_setup;

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void PS_CreatePunctuationTable( script_t *script, const punctuation_t *punctuations ) {
	int i, n, lastp;
	const punctuation_t *p, *newp;

	//get memory for the table
	if ( punctuations == default_punctuations ) {
		script->punctuationtable = default_punctuationtable;
		script->nextpunctuation = default_nextpunctuation;
		if ( default_setup ) {
			return;
		}
		default_setup = qtrue;
		i = (int)ARRAY_LEN(default_punctuations);
	}
	else {
		if ( !script->punctuationtable || script->punctuationtable == default_punctuationtable ) {
			script->punctuationtable = (int *)
									Z_TagMalloc( 256 * sizeof( int ), TAG_BOTLIB );
		}
		if ( script->nextpunctuation && script->nextpunctuation != default_nextpunctuation ) {
			Z_Free( script->nextpunctuation );
		}
		for( i = 0; punctuations[i].p; i++ )
		{
		}
		script->nextpunctuation = (int *) Z_TagMalloc( i * sizeof( int ), TAG_BOTLIB );
	}
	memset( script->punctuationtable, 0xFF, 256 * sizeof( int ) );
	memset( script->nextpunctuation, 0xFF, i * sizeof( int ) );
	//add the punctuations in the list to the punctuation table
	for ( i = 0; punctuations[i].p; i++ )
	{
		newp = &punctuations[i];
		lastp = -1;
		//sort the punctuations in this table entry on length (longer punctuations first)
		for ( n = script->punctuationtable[(unsigned int) newp->p[0]]; n >= 0; n = script->nextpunctuation[n] )
		{
			p = &punctuations[n];
			if ( strlen( p->p ) < strlen( newp->p ) ) {
				script->nextpunctuation[i] = n;
				if ( lastp >= 0 ) {
					script->nextpunctuation[lastp] = i;
				}
				else {
					script->punctuationtable[(unsigned int) newp->p[0]] = i;
				}
				break;
			}
			lastp = n;
		}
		if ( n < 0 ) {
			script->nextpunctuation[i] = -1;
			if ( lastp >= 0 ) {
				script->nextpunctuation[lastp] = i;
			}
			else {
				script->punctuationtable[(unsigned int) newp->p[0]] = i;
			}
		}
	}
} //end of the function PS_CreatePunctuationTable
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
const char *PunctuationFromNum( script_t *script, int num ) {
	int i;

	for ( i = 0; script->punctuations[i].p; i++ )
	{
		if ( script->punctuations[i].n == num ) {
			return script->punctuations[i].p;
		}
	}
	return "unkown punctuation";
} //end of the function PunctuationFromNum
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
/*int PunctuationNum( script_t *script, const char *p ) {
	int i;

	for ( i = 0; script->punctuations[i].p; i++ )
	{
		if ( !strcmp( script->punctuations[i].p, p ) ) {
			return script->punctuations[i].n;
		}
	}
	return 0;
}*/
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void Q_PRINTF_FUNC(2, 3) QDECL ScriptError(script_t *script, const char *fmt, ...)
{
	char text[MAXPRINTMSG];
	va_list ap;

	if ( script->flags & SCFL_NOERRORS ) {
		return;
	}

	va_start(ap, fmt);
	Q_vsnprintf(text, sizeof(text), fmt, ap);
	va_end(ap);
	Com_Printf( S_COLOR_RED "Error: file %s, line %d: %s\n", script->filename, script->line, text );
} //end of the function ScriptError
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void Q_PRINTF_FUNC(2, 3) QDECL ScriptWarning(script_t *script, const char *fmt, ...)
{
	char text[MAXPRINTMSG];
	va_list ap;

	if ( script->flags & SCFL_NOWARNINGS ) {
		return;
	}

	va_start(ap, fmt);
	Q_vsnprintf(text, sizeof(text), fmt, ap);
	va_end(ap);
	Com_Printf( S_COLOR_YELLOW "Warning: file %s, line %d: %s\n", script->filename, script->line, text );
} //end of the function ScriptWarning
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static void SetScriptPunctuations( script_t *script, const punctuation_t *p ) {
#ifdef PUNCTABLE
	if ( p ) {
		PS_CreatePunctuationTable( script, p );
	}
	else {
		PS_CreatePunctuationTable( script, default_punctuations );
	}
#endif //PUNCTABLE
	if ( p ) {
		script->punctuations = p;
	}
	else {
		script->punctuations = default_punctuations;
	}
} //end of the function SetScriptPunctuations
//============================================================================
// Reads spaces, tabs, C-like comments etc.
// When a newline character is found the scripts line counter is increased.
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
static int PS_ReadWhiteSpace( script_t *script ) {
	while ( 1 )
	{
		//skip white space
		while ( *script->script_p <= ' ' )
		{
			if ( !*script->script_p ) {
				return 0;
			}
			if ( *script->script_p == '\n' ) {
				script->line++;
			}
			script->script_p++;
		} //end while
		  //skip comments
		if ( *script->script_p == '/' ) {
			//comments //
			if ( *( script->script_p + 1 ) == '/' ) {
				script->script_p++;
				do
				{
					script->script_p++;
					if ( !*script->script_p ) {
						return 0;
					}
				} //end do
				while ( *script->script_p != '\n' );
				script->line++;
				script->script_p++;
				if ( !*script->script_p ) {
					return 0;
				}
				continue;
			} //end if
			  //comments /* */
			else if ( *( script->script_p + 1 ) == '*' ) {
				script->script_p++;
				do
				{
					script->script_p++;
					if ( !*script->script_p ) {
						return 0;
					}
					if ( *script->script_p == '\n' ) {
						script->line++;
					}
				} //end do
				while ( !( *script->script_p == '*' && *( script->script_p + 1 ) == '/' ) );
				script->script_p++;
				if ( !*script->script_p ) {
					return 0;
				}
				script->script_p++;
				if ( !*script->script_p ) {
					return 0;
				}
				continue;
			} //end if
		} //end if
		break;
	} //end while
	return 1;
} //end of the function PS_ReadWhiteSpace
//============================================================================
// Reads an escape character.
//
// Parameter:				script		: script to read from
//								ch				: place to store the read escape character
// Returns:					-
// Changes Globals:		-
//============================================================================
static int PS_ReadEscapeCharacter( script_t *script, char *ch ) {
	int c, val;

	//step over the leading '\\'
	script->script_p++;
	//determine the escape character
	switch ( *script->script_p )
	{
	case '\\': c = '\\'; break;
	case 'n': c = '\n'; break;
	case 'r': c = '\r'; break;
	case 't': c = '\t'; break;
	case 'v': c = '\v'; break;
	case 'b': c = '\b'; break;
	case 'f': c = '\f'; break;
	case 'a': c = '\a'; break;
	case '\'': c = '\''; break;
	case '\"': c = '\"'; break;
	case '\?': c = '\?'; break;
	case 'x':
	{
		script->script_p++;
		for (val = 0; ;script->script_p++)
		{
			c = *script->script_p;
			if ( c >= '0' && c <= '9' )
				c = c - '0';
			else if ( c >= 'A' && c <= 'Z' )
				c = c - 'A' + 10;
			else if ( c >= 'a' && c <= 'z' )
				c = c - 'a' + 10;
			else
				break;
			val = ( val << 4 ) + c;
		}
		script->script_p--;
		if ( val > 0xFF ) {
			ScriptWarning( script, "too large value in escape character" );
			val = 0xFF;
		}     //end if
		c = val;
		break;
	}
	default:     //NOTE: decimal ASCII code, NOT octal
	{
		if ( *script->script_p < '0' || *script->script_p > '9' ) {
			ScriptError( script, "unknown escape char" );
		}
		for ( val = 0; ;script->script_p++ )
		{
			c = *script->script_p;
			if ( c >= '0' && c <= '9' )
				c = c - '0';
			else
				break;
			val = val * 10 + c;
		}
		script->script_p--;
		if ( val > 0xFF ) {
			ScriptWarning( script, "too large value in escape character" );
			val = 0xFF;
		}     //end if
		c = val;
		break;
	}
	}
	//step over the escape character or the last digit of the number
	script->script_p++;
	//store the escape character
	*ch = (char)c;
	//succesfully read escape character
	return 1;
} //end of the function PS_ReadEscapeCharacter
//============================================================================
// Reads C-like string. Escape characters are interpretted.
// Quotes are included with the string.
// Reads two strings with a white space between them as one string.
//
// Parameter:				script		: script to read from
//								token			: buffer to store the string
// Returns:					qtrue when a string was read succesfully
// Changes Globals:		-
//============================================================================
static int PS_ReadString( script_t *script, token_t *token, int quote ) {
	int len, tmpline;
	const char *tmpscript_p;

	if ( quote == '\"' ) {
		token->type = TT_STRING;
	} else { token->type = TT_LITERAL;}

	len = 0;
	//leading quote
	token->string[len++] = *script->script_p++;
	//
	while ( 1 )
	{
		//minus 2 because trailing double quote and zero have to be appended
		if ( len >= MAX_TOKEN - 2 ) {
			ScriptError( script, "string longer than MAX_TOKEN = %d", MAX_TOKEN );
			return 0;
		} //end if
		  //if there is an escape character and
		  //if escape characters inside a string are allowed
		if ( *script->script_p == '\\' && !( script->flags & SCFL_NOSTRINGESCAPECHARS ) ) {
			if ( !PS_ReadEscapeCharacter( script, &token->string[len] ) ) {
				token->string[len] = '\0';
				return 0;
			} //end if
			len++;
		} //end if
		  //if a trailing quote
		else if ( *script->script_p == quote ) {
			//step over the double quote
			script->script_p++;
			//if white spaces in a string are not allowed
			if ( script->flags & SCFL_NOSTRINGWHITESPACES ) {
				break;
			}
			//
			tmpscript_p = script->script_p;
			tmpline = script->line;
			//read white space between possible two consecutive strings
			if ( !PS_ReadWhiteSpace( script ) ) {
				script->script_p = tmpscript_p;
				script->line = tmpline;
				break;
			} //end if
			  //if there's no leading double qoute
			if ( *script->script_p != quote ) {
				script->script_p = tmpscript_p;
				script->line = tmpline;
				break;
			} //end if
			  //step over the new leading double quote
			script->script_p++;
		} //end if
		else
		{
			if ( *script->script_p == '\0' ) {
				token->string[len] = '\0';
				ScriptError( script, "missing trailing quote" );
				return 0;
			} //end if
			if ( *script->script_p == '\n' ) {
				token->string[len] = '\0';
				ScriptError( script, "newline inside string %s", token->string );
				return 0;
			} //end if
			token->string[len++] = *script->script_p++;
		} //end else
	} //end while
	  //trailing quote
	token->string[len++] = (char)quote;
	//end string with a zero
	token->string[len] = '\0';
	//the sub type is the length of the string
	token->subtype = len;
	return 1;
} //end of the function PS_ReadString
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PS_ReadName( script_t *script, token_t *token ) {
	int len = 0;
	char c;

	token->type = TT_NAME;
	do
	{
		token->string[len++] = *script->script_p++;
		if ( len >= MAX_TOKEN ) {
			ScriptError( script, "name longer than MAX_TOKEN = %d", MAX_TOKEN );
			return 0;
		} //end if
		c = *script->script_p;
	} while ( ( c >= 'a' && c <= 'z' ) ||
			  ( c >= 'A' && c <= 'Z' ) ||
			  ( c >= '0' && c <= '9' ) ||
			  c == '_' );
	token->string[len] = '\0';
	//the sub type is the length of the name
	token->subtype = len;
	return 1;
} //end of the function PS_ReadName
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
static void NumberValue( char *string, int subtype, unsigned int *intvalue,
				  float *floatvalue ) {
	unsigned long int dotfound = 0;

	*intvalue = 0;
	*floatvalue = 0;
	//floating point number
	if ( subtype & TT_FLOAT ) {
		while ( *string )
		{
			if ( *string == '.' ) {
				if ( dotfound ) {
					return;
				}
				dotfound = 10;
				string++;
			} //end if
			if ( dotfound ) {
				*floatvalue = *floatvalue + ( float )( *string - '0' ) /
							  (float) dotfound;
				dotfound *= 10;
			} //end if
			else
			{
				*floatvalue = *floatvalue * 10.0 + ( float )( *string - '0' );
			} //end else
			string++;
		} //end while
		*intvalue = (int) *floatvalue;
	} //end if
	else if ( subtype & TT_DECIMAL ) {
		while ( *string ) *intvalue = *intvalue * 10 + ( *string++ - '0' );
		*floatvalue = *intvalue;
	} //end else if
	else if ( subtype & TT_HEX ) {
		//step over the leading 0x or 0X
		string += 2;
		while ( *string )
		{
			*intvalue <<= 4;
			if ( *string >= 'a' && *string <= 'f' ) {
				*intvalue += *string - 'a' + 10;
			} else if ( *string >= 'A' && *string <= 'F' ) {
				*intvalue += *string - 'A' + 10;
			} else { *intvalue += *string - '0';}
			string++;
		} //end while
		*floatvalue = (float)*intvalue;
	} //end else if
	else if ( subtype & TT_OCTAL ) {
		//step over the first zero
		string += 1;
		while ( *string ) *intvalue = ( *intvalue << 3 ) + ( *string++ - '0' );
		*floatvalue = (float)*intvalue;
	} //end else if
	else if ( subtype & TT_BINARY ) {
		//step over the leading 0b or 0B
		string += 2;
		while ( *string ) *intvalue = ( *intvalue << 1 ) + ( *string++ - '0' );
		*floatvalue = (float)*intvalue;
	} //end else if
} //end of the function NumberValue
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
static int PS_ReadNumber( script_t *script, token_t *token ) {
	int len = 0, i;
	int octal, dot;
	char c;

	token->type = TT_NUMBER;
	//check for a hexadecimal number
	if ( *script->script_p == '0' &&
		 ( *( script->script_p + 1 ) == 'x' ||
		   *( script->script_p + 1 ) == 'X' ) ) {
		token->string[len++] = *script->script_p++;
		token->string[len++] = *script->script_p++;
		c = *script->script_p;
		//hexadecimal
		while ( ( c >= '0' && c <= '9' ) ||
				( c >= 'a' && c <= 'f' ) ||
				( c >= 'A' && c <= 'F' ) )
		{
			token->string[len++] = *script->script_p++;
			if ( len >= MAX_TOKEN ) {
				ScriptError( script, "hexadecimal number longer than MAX_TOKEN = %d", MAX_TOKEN );
				return 0;
			} //end if
			c = *script->script_p;
		} //end while
		token->subtype |= TT_HEX;
	} //end if
#ifdef BINARYNUMBERS
	//check for a binary number
	else if ( *script->script_p == '0' &&
			  ( *( script->script_p + 1 ) == 'b' ||
				*( script->script_p + 1 ) == 'B' ) ) {
		token->string[len++] = *script->script_p++;
		token->string[len++] = *script->script_p++;
		c = *script->script_p;
		//hexadecimal
		while ( c == '0' || c == '1' )
		{
			token->string[len++] = *script->script_p++;
			if ( len >= MAX_TOKEN ) {
				ScriptError( script, "binary number longer than MAX_TOKEN = %d", MAX_TOKEN );
				return 0;
			} //end if
			c = *script->script_p;
		} //end while
		token->subtype |= TT_BINARY;
	} //end if
#endif //BINARYNUMBERS
	else //decimal or octal integer or floating point number
	{
		octal = qfalse;
		dot = qfalse;
		if ( *script->script_p == '0' ) {
			octal = qtrue;
		}
		while ( 1 )
		{
			c = *script->script_p;
			if ( c == '.' ) {
				dot = qtrue;
			} else if ( c == '8' || c == '9' ) {
				octal = qfalse;
			} else if ( c < '0' || c > '9' ) {
				break;
			}
			token->string[len++] = *script->script_p++;
			if ( len >= MAX_TOKEN - 1 ) {
				ScriptError( script, "number longer than MAX_TOKEN = %d", MAX_TOKEN );
				return 0;
			} //end if
		} //end while
		if ( octal ) {
			token->subtype |= TT_OCTAL;
		} else { token->subtype |= TT_DECIMAL;}
		if ( dot ) {
			token->subtype |= TT_FLOAT;
		}
	} //end else
	for ( i = 0; i < 2; i++ )
	{
		c = *script->script_p;
		//check for a LONG number
		if ( ( c == 'l' || c == 'L' ) &&
			 !( token->subtype & TT_LONG ) ) {
			script->script_p++;
			token->subtype |= TT_LONG;
		} //end if
		  //check for an UNSIGNED number
		else if ( ( c == 'u' || c == 'U' ) &&
				  !( token->subtype & ( TT_UNSIGNED | TT_FLOAT ) ) ) {
			script->script_p++;
			token->subtype |= TT_UNSIGNED;
		} //end if
	} //end for
	token->string[len] = '\0';
	NumberValue( token->string, token->subtype, &token->intvalue, &token->floatvalue );
	if ( !( token->subtype & TT_FLOAT ) ) {
		token->subtype |= TT_INTEGER;
	}
	return 1;
} //end of the function PS_ReadNumber
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*int PS_ReadLiteral( script_t *script, token_t *token ) {
	token->type = TT_LITERAL;
	//first quote
	token->string[0] = *script->script_p++;
	//check for end of file
	if ( !*script->script_p ) {
		ScriptError( script, "end of file before trailing \'" );
		return 0;
	} //end if
	  //if it is an escape character
	if ( *script->script_p == '\\' ) {
		if ( !PS_ReadEscapeCharacter( script, &token->string[1] ) ) {
			return 0;
		}
	} //end if
	else
	{
		token->string[1] = *script->script_p++;
	} //end else
	  //check for trailing quote
	if ( *script->script_p != '\'' ) {
		ScriptWarning( script, "too many characters in literal, ignored" );
		while ( *script->script_p &&
				*script->script_p != '\'' &&
				*script->script_p != '\n' )
		{
			script->script_p++;
		} //end while
		if ( *script->script_p == '\'' ) {
			script->script_p++;
		}
	} //end if
	  //store the trailing quote
	token->string[2] = *script->script_p++;
	//store trailing zero to end the string
	token->string[3] = '\0';
	//the sub type is the integer literal value
	token->subtype = token->string[1];
	//
	return 1;
}*/ //end of the function PS_ReadLiteral
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PS_ReadPunctuation( script_t *script, token_t *token ) {
	int len, n;
	const char *p;
	const punctuation_t *punc;

#ifdef PUNCTABLE
	for ( n = script->punctuationtable[(unsigned int)*script->script_p]; n >= 0; n = script->nextpunctuation[n] )
	{
		punc = &script->punctuations[n];
#else
	int i;

	for ( i = 0; script->punctuations[i].p; i++ )
	{
		punc = &script->punctuations[i];
#endif //PUNCTABLE
		p = punc->p;
		len = strlen( p );
		//if the script contains at least as much characters as the punctuation
		if ( script->script_p + len <= script->end_p ) {
			//if the script contains the punctuation
			if ( !strncmp( script->script_p, p, len ) ) {
				Q_strncpyz( token->string, p, sizeof( token->string ) );
				script->script_p += len;
				token->type = TT_PUNCTUATION;
				//sub type is the number of the punctuation
				token->subtype = punc->n;
				return 1;
			} //end if
		} //end if
	} //end for
	return 0;
} //end of the function PS_ReadPunctuation
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*int PS_ReadPrimitive( script_t *script, token_t *token ) {
	int len;

	len = 0;
	while ( *script->script_p > ' ' && *script->script_p != ';' )
	{
		if ( len >= MAX_TOKEN ) {
			ScriptError( script, "primitive token longer than MAX_TOKEN = %d", MAX_TOKEN );
			return 0;
		} //end if
		token->string[len++] = *script->script_p++;
	} //end while
	token->string[len] = '\0';
	//copy the token into the script structure
	memcpy( &script->token, token, sizeof( token_t ) );
	//primitive reading successfull
	return 1;
}*/ //end of the function PS_ReadPrimitive
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PS_ReadToken( script_t *script, token_t *token ) {
	//if there is a token available (from UnreadToken)
	if ( script->tokenavailable ) {
		script->tokenavailable = 0;
		memcpy( token, &script->token, sizeof( token_t ) );
		return 1;
	} //end if
	  //save script pointer
	script->lastscript_p = script->script_p;
	//save line counter
	script->lastline = script->line;
	//clear the token stuff
	memset( token, 0, sizeof( token_t ) );
	//start of the white space
	script->whitespace_p = script->script_p;
	token->whitespace_p = script->script_p;
	//read white space before token
	if ( !PS_ReadWhiteSpace( script ) ) {
		return 0;
	}
	//end of the white space
	script->endwhitespace_p = script->script_p;
	token->endwhitespace_p = script->script_p;
	//line the token is on
	token->line = script->line;
	//number of lines crossed before token
	token->linescrossed = script->line - script->lastline;
	//if there is a leading double quote
	if ( *script->script_p == '\"' ) {
		if ( !PS_ReadString( script, token, '\"' ) ) {
			return 0;
		}
	} //end if
	  //if an literal
	else if ( *script->script_p == '\'' ) {
		//if (!PS_ReadLiteral(script, token)) return 0;
		if ( !PS_ReadString( script, token, '\'' ) ) {
			return 0;
		}
	} //end if
	  //if there is a number
	else if ( ( *script->script_p >= '0' && *script->script_p <= '9' ) ||
			  ( *script->script_p == '.' &&
				( *( script->script_p + 1 ) >= '0' && *( script->script_p + 1 ) <= '9' ) ) ) {
		if ( !PS_ReadNumber( script, token ) ) {
			return 0;
		}
	} //end if
	  //if this is a primitive script
	//else if ( script->flags & SCFL_PRIMITIVE ) {
	//	return PS_ReadPrimitive( script, token );
	//} //end else if
	  //if there is a name
	else if ( ( *script->script_p >= 'a' && *script->script_p <= 'z' ) ||
			  ( *script->script_p >= 'A' && *script->script_p <= 'Z' ) ||
			  *script->script_p == '_' ) {
		if ( !PS_ReadName( script, token ) ) {
			return 0;
		}
	} //end if
	  //check for punctuations
	else if ( !PS_ReadPunctuation( script, token ) ) {
		ScriptError( script, "can't read token" );
		return 0;
	} //end if
	  //copy the token into the script structure
	memcpy( &script->token, token, sizeof( token_t ) );
	//succesfully read a token
	return 1;
} //end of the function PS_ReadToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*int PS_ExpectTokenString( script_t *script, const char *string ) {
	token_t token;

	if ( !PS_ReadToken( script, &token ) ) {
		ScriptError( script, "couldn't find expected %s", string );
		return 0;
	} //end if

	if ( strcmp( token.string, string ) ) {
		ScriptError( script, "expected %s, found %s", string, token.string );
		return 0;
	} //end if
	return 1;
}*/ //end of the function PS_ExpectToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PS_ExpectTokenType( script_t *script, int type, int subtype, token_t *token ) {
	char str[MAX_TOKEN];

	if ( !PS_ReadToken( script, token ) ) {
		ScriptError( script, "couldn't read expected token" );
		return 0;
	} //end if

	if ( token->type != type ) {
		if ( type == TT_STRING ) {
			strcpy( str, "string" );
		}
		if ( type == TT_LITERAL ) {
			strcpy( str, "literal" );
		}
		if ( type == TT_NUMBER ) {
			strcpy( str, "number" );
		}
		if ( type == TT_NAME ) {
			strcpy( str, "name" );
		}
		if ( type == TT_PUNCTUATION ) {
			strcpy( str, "punctuation" );
		}
		ScriptError( script, "expected a %s, found %s", str, token->string );
		return 0;
	} //end if
	if ( token->type == TT_NUMBER ) {
		if ( ( token->subtype & subtype ) != subtype ) {
			if ( subtype & TT_DECIMAL ) {
				strcpy( str, "decimal" );
			}
			if ( subtype & TT_HEX ) {
				strcpy( str, "hex" );
			}
			if ( subtype & TT_OCTAL ) {
				strcpy( str, "octal" );
			}
			if ( subtype & TT_BINARY ) {
				strcpy( str, "binary" );
			}
			if ( subtype & TT_LONG ) {
				strcat( str, " long" );
			}
			if ( subtype & TT_UNSIGNED ) {
				strcat( str, " unsigned" );
			}
			if ( subtype & TT_FLOAT ) {
				strcat( str, " float" );
			}
			if ( subtype & TT_INTEGER ) {
				strcat( str, " integer" );
			}
			ScriptError( script, "expected %s, found '%s'", str, token->string );
			return 0;
		} //end if
	} //end if
	else if ( token->type == TT_PUNCTUATION ) {
		if ( subtype < 0 ) {
			ScriptError( script, "BUG: wrong punctuation subtype" );
			return 0;
		} //end if
		if ( token->subtype != subtype ) {
			ScriptError( script, "expected '%s', found '%s'",
						 PunctuationFromNum( script, subtype ), token->string );
			return 0;
		} //end if
	} //end else if
	return 1;
} //end of the function PS_ExpectTokenType
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*int PS_ExpectAnyToken( script_t *script, token_t *token ) {
	if ( !PS_ReadToken( script, token ) ) {
		ScriptError( script, "couldn't read expected token" );
		return 0;
	} //end if
	else
	{
		return 1;
	} //end else
}*/ //end of the function PS_ExpectAnyToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*int PS_CheckTokenString( script_t *script, const char *string ) {
	token_t tok;

	if ( !PS_ReadToken( script, &tok ) ) {
		return 0;
	}
	//if the token is available
	if ( !strcmp( tok.string, string ) ) {
		return 1;
	}
	//token not available
	script->script_p = script->lastscript_p;
	return 0;
}*/ //end of the function PS_CheckTokenString
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*int PS_CheckTokenType( script_t *script, int type, int subtype, token_t *token ) {
	token_t tok;

	if ( !PS_ReadToken( script, &tok ) ) {
		return 0;
	}
	//if the type matches
	if ( tok.type == type &&
		 ( tok.subtype & subtype ) == subtype ) {
		memcpy( token, &tok, sizeof( token_t ) );
		return 1;
	} //end if
	  //token is not available
	script->script_p = script->lastscript_p;
	return 0;
}*/ //end of the function PS_CheckTokenType
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*int PS_SkipUntilString( script_t *script, const char *string ) {
	token_t token;

	while ( PS_ReadToken( script, &token ) )
	{
		if ( !strcmp( token.string, string ) ) {
			return 1;
		}
	} //end while
	return 0;
}*/ //end of the function PS_SkipUntilString
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*void PS_UnreadLastToken( script_t *script ) {
	script->tokenavailable = 1;
}*/ //end of the function UnreadLastToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*void PS_UnreadToken( script_t *script, token_t *token ) {
	memcpy( &script->token, token, sizeof( token_t ) );
	script->tokenavailable = 1;
}*/ //end of the function UnreadToken
//============================================================================
// returns the next character of the read white space, returns NULL if none
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*char PS_NextWhiteSpaceChar( script_t *script ) {
	if ( script->whitespace_p != script->endwhitespace_p ) {
		return *script->whitespace_p++;
	} //end if
	else
	{
		return 0;
	} //end else
}*/ //end of the function PS_NextWhiteSpaceChar
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void StripDoubleQuotes( char *string ) {
	if ( *string == '\"' ) {
		memmove( string, string + 1, strlen( string ) );
	} //end if
	if ( string[strlen( string ) - 1] == '\"' ) {
		string[strlen( string ) - 1] = '\0';
	} //end if
} //end of the function StripDoubleQuotes
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*void StripSingleQuotes( char *string ) {
	if ( *string == '\'' ) {
		strcpy( string, string + 1 );
	} //end if
	if ( string[strlen( string ) - 1] == '\'' ) {
		string[strlen( string ) - 1] = '\0';
	} //end if
}*/ //end of the function StripSingleQuotes
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*long double ReadSignedFloat( script_t *script ) {
	token_t token;
	long double sign = 1;

	PS_ExpectAnyToken( script, &token );
	if ( !strcmp( token.string, "-" ) ) {
		sign = -1;
		PS_ExpectTokenType( script, TT_NUMBER, 0, &token );
	} //end if
	else if ( token.type != TT_NUMBER ) {
		ScriptError( script, "expected float value, found %s\n", token.string );
	} //end else if
	return sign * token.floatvalue;
}*/ //end of the function ReadSignedFloat
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*signed long int ReadSignedInt( script_t *script ) {
	token_t token;
	signed long int sign = 1;

	PS_ExpectAnyToken( script, &token );
	if ( !strcmp( token.string, "-" ) ) {
		sign = -1;
		PS_ExpectTokenType( script, TT_NUMBER, TT_INTEGER, &token );
	} //end if
	else if ( token.type != TT_NUMBER || token.subtype == TT_FLOAT ) {
		ScriptError( script, "expected integer value, found %s\n", token.string );
	} //end else if
	return sign * token.intvalue;
}*/ //end of the function ReadSignedInt
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*void SetScriptFlags( script_t *script, int flags ) {
	script->flags = flags;
}*/ //end of the function SetScriptFlags
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*int GetScriptFlags( script_t *script ) {
	return script->flags;
}*/ //end of the function GetScriptFlags
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*void ResetScript( script_t *script ) {
	//pointer in script buffer
	script->script_p = script->buffer;
	//pointer in script buffer before reading token
	script->lastscript_p = script->buffer;
	//begin of white space
	script->whitespace_p = NULL;
	//end of white space
	script->endwhitespace_p = NULL;
	//set if there's a token available in script->token
	script->tokenavailable = 0;
	//
	script->line = 1;
	script->lastline = 1;
	//clear the saved token
	memset( &script->token, 0, sizeof( token_t ) );
}*/ //end of the function ResetScript
//============================================================================
// returns true if at the end of the script
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int EndOfScript( script_t *script ) {
	return script->script_p >= script->end_p;
} //end of the function EndOfScript
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*int NumLinesCrossed( script_t *script ) {
	return script->line - script->lastline;
}*/ //end of the function NumLinesCrossed
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*int ScriptSkipTo( script_t *script, char *value ) {
	int len;
	char firstchar;

	firstchar = *value;
	len = strlen( value );
	do
	{
		if ( !PS_ReadWhiteSpace( script ) ) {
			return 0;
		}
		if ( *script->script_p == firstchar ) {
			if ( !strncmp( script->script_p, value, len ) ) {
				return 1;
			} //end if
		} //end if
		script->script_p++;
	} while ( 1 );
}*/ //end of the function ScriptSkipTo
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
script_t *LoadScriptFile( const char *filename ) {
	fileHandle_t fp;
	char pathname[MAX_QPATH*2];
	int length;
	void *buffer;
	script_t *script;
	char *buf;

	Com_sprintf( pathname, sizeof( pathname ), "%s", filename );
	length = FS_FOpenFileByMode( pathname, &fp, FS_READ );
	if (!fp) return NULL;

	buffer = Z_TagMalloc( sizeof( script_t ) + (unsigned int)length + 1, TAG_BOTLIB );
	memset( buffer, 0, sizeof( script_t ) + (unsigned int)length + 1 );
	script = (script_t *) buffer;
	Com_Memset(script, 0, sizeof(script_t));
	Q_strncpyz(script->filename, filename, sizeof(script->filename));
	buf = (char *) buffer + sizeof(script_t);
	FS_Read( buf, length, fp );
	buf[length] = '\0';
	FS_FCloseFile( fp );
	script->buffer = buf;
	script->length = length;
	//pointer in script buffer
	script->script_p = script->buffer;
	//pointer in script buffer before reading token
	script->lastscript_p = script->buffer;
	//pointer to end of script buffer
	script->end_p = &script->buffer[length];
	//set if there's a token available in script->token
	script->tokenavailable = 0;
	//
	script->line = 1;
	script->lastline = 1;
	//
	SetScriptPunctuations( script, NULL );
	//
	return script;
} //end of the function LoadScriptFile
//============================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//============================================================================
script_t *LoadScriptMemory(const char *ptr, int length, const char *name)
{
	void *buffer;
	script_t *script;
	char *buf;

	buffer = Z_TagMalloc(sizeof(script_t) + (unsigned int)length + 1, TAG_BOTLIB);
	memset( buffer, 0, sizeof(script_t) + (unsigned int)length + 1 );
	script = (script_t *) buffer;
	Com_Memset(script, 0, sizeof(script_t));
	Q_strncpyz(script->filename, name, sizeof(script->filename));
	buf = (char *) buffer + sizeof(script_t);
	memcpy( buf, ptr, length );
	buf[length] = '\0';
	script->buffer = buf;
	script->length = length;
	//pointer in script buffer
	script->script_p = script->buffer;
	//pointer in script buffer before reading token
	script->lastscript_p = script->buffer;
	//pointer to end of script buffer
	script->end_p = &script->buffer[length];
	//set if there's a token available in script->token
	script->tokenavailable = 0;
	//
	script->line = 1;
	script->lastline = 1;
	//
	SetScriptPunctuations( script, NULL );
	//
	//memcpy( script->buffer, ptr, (unsigned int)length );
	//
	return script;
} //end of the function LoadScriptMemory
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void FreeScript( script_t *script ) {
#ifdef PUNCTABLE
	if( script->punctuationtable && script->punctuationtable != default_punctuationtable ) {
		Z_Free( script->punctuationtable );
		script->punctuationtable = NULL;
	}
	if( script->nextpunctuation && script->nextpunctuation != default_nextpunctuation ) {
		Z_Free( script->nextpunctuation );
		script->nextpunctuation = NULL;
	}
#endif //PUNCTABLE
	Z_Free( script );
} //end of the function FreeScript
