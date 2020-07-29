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

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#ifdef DEDICATED
#include "../client/keys.h"
#else
// Needed for cls struct access and cl_language
#include "../client/client.h"
#endif

int		 anykeydown;
qkey_t	 keys[MAX_KEYS];

qboolean key_overstrikeMode;

typedef struct {
	const char *name;
	int keynum;
} keyname_t;

// names not in this list can either be lowercase ascii, or '0xnn' hex sequences
static const keyname_t keynames[] =
{
	{"a", K_A},
	{"b", K_B},
	{"c", K_C},
	{"d", K_D},
	{"e", K_E},
	{"f", K_F},
	{"g", K_G},
	{"h", K_H},
	{"i", K_I},
	{"j", K_J},
	{"k", K_K},
	{"l", K_L},
	{"m", K_M},
	{"n", K_N},
	{"o", K_O},
	{"p", K_P},
	{"q", K_Q},
	{"r", K_R},
	{"s", K_S},
	{"t", K_T},
	{"u", K_U},
	{"v", K_V},
	{"w", K_W},
	{"x", K_X},
	{"y", K_Y},
	{"z", K_Z},

	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},

	{"'", K_QUOTE},
	{"+", K_PLUS},
	{",", K_COMMA},
	{"-", K_MINUS},
	{".", K_DOT}, 
	{"/", K_SLASH},
	{"SEMICOLON", K_SEMICOLON},	// because a raw semicolon separates commands
	{"=", K_EQUAL},
	{"\\",K_BACKSLASH},
	{"_", K_UNDERSCORE},
	{"[", K_BRACKET_OPEN},
	{"]", K_BRACKET_CLOSE},

	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},

	{"COMMAND", K_COMMAND},

	{"CAPSLOCK", K_CAPSLOCK},
	
	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},
	{"F13", K_F13},
	{"F14", K_F14},
	{"F15", K_F15},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},

	{"MWHEELUP",	K_MWHEELUP },
	{"MWHEELDOWN",	K_MWHEELDOWN },

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},
	{"JOY5", K_JOY5},
	{"JOY6", K_JOY6},
	{"JOY7", K_JOY7},
	{"JOY8", K_JOY8},
	{"JOY9", K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},
	{"JOY17", K_JOY17},
	{"JOY18", K_JOY18},
	{"JOY19", K_JOY19},
	{"JOY20", K_JOY20},
	{"JOY21", K_JOY21},
	{"JOY22", K_JOY22},
	{"JOY23", K_JOY23},
	{"JOY24", K_JOY24},
	{"JOY25", K_JOY25},
	{"JOY26", K_JOY26},
	{"JOY27", K_JOY27},
	{"JOY28", K_JOY28},
	{"JOY29", K_JOY29},
	{"JOY30", K_JOY30},
	{"JOY31", K_JOY31},
	{"JOY32", K_JOY32},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},

	{"KP_HOME",			K_KP_HOME },
	{"KP_UPARROW",		K_KP_UPARROW },
	{"KP_PGUP",			K_KP_PGUP },
	{"KP_LEFTARROW",	K_KP_LEFTARROW },
	{"KP_5",			K_KP_5 },
	{"KP_RIGHTARROW",	K_KP_RIGHTARROW },
	{"KP_END",			K_KP_END },
	{"KP_DOWNARROW",	K_KP_DOWNARROW },
	{"KP_PGDN",			K_KP_PGDN },
	{"KP_ENTER",		K_KP_ENTER },
	{"KP_INS",			K_KP_INS },
	{"KP_DEL",			K_KP_DEL },
	{"KP_SLASH",		K_KP_SLASH },
	{"KP_MINUS",		K_KP_MINUS },
	{"KP_PLUS",			K_KP_PLUS },
	{"KP_NUMLOCK",		K_KP_NUMLOCK },
	//{"KP_STAR",			K_KP_STAR },
	{"KP_EQUALS",		K_KP_EQUALS },

	{"PAUSE", K_PAUSE},
	
	{"SEMICOLON", ';'},	// because a raw semicolon seperates commands

	{"WORLD_0", K_WORLD_0},
	{"WORLD_1", K_WORLD_1},
	{"WORLD_2", K_WORLD_2},
	{"WORLD_3", K_WORLD_3},
	{"WORLD_4", K_WORLD_4},
	{"WORLD_5", K_WORLD_5},
	{"WORLD_6", K_WORLD_6},
	{"WORLD_7", K_WORLD_7},
	{"WORLD_8", K_WORLD_8},
	{"WORLD_9", K_WORLD_9},
	{"WORLD_10", K_WORLD_10},
	{"WORLD_11", K_WORLD_11},
	{"WORLD_12", K_WORLD_12},
	{"WORLD_13", K_WORLD_13},
	{"WORLD_14", K_WORLD_14},
	{"WORLD_15", K_WORLD_15},
	{"WORLD_16", K_WORLD_16},
	{"WORLD_17", K_WORLD_17},
	{"WORLD_18", K_WORLD_18},
	{"WORLD_19", K_WORLD_19},
	{"WORLD_20", K_WORLD_20},
	{"WORLD_21", K_WORLD_21},
	{"WORLD_22", K_WORLD_22},
	{"WORLD_23", K_WORLD_23},
	{"WORLD_24", K_WORLD_24},
	{"WORLD_25", K_WORLD_25},
	{"WORLD_26", K_WORLD_26},
	{"WORLD_27", K_WORLD_27},
	{"WORLD_28", K_WORLD_28},
	{"WORLD_29", K_WORLD_29},
	{"WORLD_30", K_WORLD_30},
	{"WORLD_31", K_WORLD_31},
	{"WORLD_32", K_WORLD_32},
	{"WORLD_33", K_WORLD_33},
	{"WORLD_34", K_WORLD_34},
	{"WORLD_35", K_WORLD_35},
	{"WORLD_36", K_WORLD_36},
	{"WORLD_37", K_WORLD_37},
	{"WORLD_38", K_WORLD_38},
	{"WORLD_39", K_WORLD_39},
	{"WORLD_40", K_WORLD_40},
	{"WORLD_41", K_WORLD_41},
	{"WORLD_42", K_WORLD_42},
	{"WORLD_43", K_WORLD_43},
	{"WORLD_44", K_WORLD_44},
	{"WORLD_45", K_WORLD_45},
	{"WORLD_46", K_WORLD_46},
	{"WORLD_47", K_WORLD_47},
	{"WORLD_48", K_WORLD_48},
	{"WORLD_49", K_WORLD_49},
	{"WORLD_50", K_WORLD_50},
	{"WORLD_51", K_WORLD_51},
	{"WORLD_52", K_WORLD_52},
	{"WORLD_53", K_WORLD_53},
	{"WORLD_54", K_WORLD_54},
	{"WORLD_55", K_WORLD_55},
	{"WORLD_56", K_WORLD_56},
	{"WORLD_57", K_WORLD_57},
	{"WORLD_58", K_WORLD_58},
	{"WORLD_59", K_WORLD_59},
	{"WORLD_60", K_WORLD_60},
	{"WORLD_61", K_WORLD_61},
	{"WORLD_62", K_WORLD_62},
	{"WORLD_63", K_WORLD_63},
	{"WORLD_64", K_WORLD_64},
	{"WORLD_65", K_WORLD_65},
	{"WORLD_66", K_WORLD_66},
	{"WORLD_67", K_WORLD_67},
	{"WORLD_68", K_WORLD_68},
	{"WORLD_69", K_WORLD_69},
	{"WORLD_70", K_WORLD_70},
	{"WORLD_71", K_WORLD_71},
	{"WORLD_72", K_WORLD_72},
	{"WORLD_73", K_WORLD_73},
	{"WORLD_74", K_WORLD_74},
	{"WORLD_75", K_WORLD_75},
	{"WORLD_76", K_WORLD_76},
	{"WORLD_77", K_WORLD_77},
	{"WORLD_78", K_WORLD_78},
	{"WORLD_79", K_WORLD_79},
	{"WORLD_80", K_WORLD_80},
	{"WORLD_81", K_WORLD_81},
	{"WORLD_82", K_WORLD_82},
	{"WORLD_83", K_WORLD_83},
	{"WORLD_84", K_WORLD_84},
	{"WORLD_85", K_WORLD_85},
	{"WORLD_86", K_WORLD_86},
	{"WORLD_87", K_WORLD_87},
	{"WORLD_88", K_WORLD_88},
	{"WORLD_89", K_WORLD_89},
	{"WORLD_90", K_WORLD_90},
	{"WORLD_91", K_WORLD_91},
	{"WORLD_92", K_WORLD_92},
	{"WORLD_93", K_WORLD_93},
	{"WORLD_94", K_WORLD_94},
	{"WORLD_95", K_WORLD_95},

	{"WINDOWS", K_SUPER},
	{"COMPOSE", K_COMPOSE},
	{"MODE", K_MODE},
	{"HELP", K_HELP},
	{"PRINT", K_PRINT},
	{"SYSREQ", K_SYSREQ},
	{"SCROLLOCK", K_SCROLLOCK },
	{"BREAK", K_BREAK},
	{"MENU", K_MENU},
	{"POWER", K_POWER},
	{"EURO", K_EURO},
	{"UNDO", K_UNDO},

	{NULL,0}
};

// suppress unused warnings when building DEDICATED server
#ifdef __GNUC__
#define VARIABLE_IS_NOT_USED_IN_DEDICATED_SERVER __attribute__ ((unused))
#else
#define VARIABLE_IS_NOT_USED_IN_DEDICATED_SERVER
#endif

static const keyname_t VARIABLE_IS_NOT_USED_IN_DEDICATED_SERVER keynames_d[] =    //deutsch
{
	{"TAB", K_TAB},
	{"EINGABETASTE", K_ENTER},
	{"ESC", K_ESCAPE},
	{"LEERTASTE", K_SPACE},
	{"R�CKTASTE", K_BACKSPACE},
	{"PFEILT.AUF", K_UPARROW},
	{"PFEILT.UNTEN", K_DOWNARROW},
	{"PFEILT.LINKS", K_LEFTARROW},
	{"PFEILT.RECHTS", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"STRG", K_CTRL},
	{"UMSCHALLT", K_SHIFT},

	{"FESTSTELLT", K_CAPSLOCK},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"EINFG", K_INS},
	{"ENTF", K_DEL},
	{"BILD-AB", K_PGDN},
	{"BILD-AUF", K_PGUP},
	{"POS1", K_HOME},
	{"ENDE", K_END},

	{"MAUS1", K_MOUSE1},
	{"MAUS2", K_MOUSE2},
	{"MAUS3", K_MOUSE3},
	{"MAUS4", K_MOUSE4},
	{"MAUS5", K_MOUSE5},

	{"MRADOBEN", K_MWHEELUP },
	{"MRADUNTEN",    K_MWHEELDOWN },

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},
	{"JOY5", K_JOY5},
	{"JOY6", K_JOY6},
	{"JOY7", K_JOY7},
	{"JOY8", K_JOY8},
	{"JOY9", K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},
	{"JOY17", K_JOY17},
	{"JOY18", K_JOY18},
	{"JOY19", K_JOY19},
	{"JOY20", K_JOY20},
	{"JOY21", K_JOY21},
	{"JOY22", K_JOY22},
	{"JOY23", K_JOY23},
	{"JOY24", K_JOY24},
	{"JOY25", K_JOY25},
	{"JOY26", K_JOY26},
	{"JOY27", K_JOY27},
	{"JOY28", K_JOY28},
	{"JOY29", K_JOY29},
	{"JOY30", K_JOY30},
	{"JOY31", K_JOY31},
	{"JOY32", K_JOY32},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},

	{"ZB_POS1",          K_KP_HOME },
	{"ZB_PFEILT.AUF",    K_KP_UPARROW },
	{"ZB_BILD-AUF",      K_KP_PGUP },
	{"ZB_PFEILT.LINKS",  K_KP_LEFTARROW },
	{"ZB_5",         K_KP_5 },
	{"ZB_PFEILT.RECHTS",K_KP_RIGHTARROW },
	{"ZB_ENDE",          K_KP_END },
	{"ZB_PFEILT.UNTEN",  K_KP_DOWNARROW },
	{"ZB_BILD-AB",       K_KP_PGDN },
	{"ZB_ENTER",     K_KP_ENTER },
	{"ZB_EINFG",     K_KP_INS },
	{"ZB_ENTF",          K_KP_DEL },
	{"ZB_SLASH",     K_KP_SLASH },
	{"ZB_MINUS",     K_KP_MINUS },
	{"ZB_PLUS",          K_KP_PLUS },
	{"ZB_NUM",           K_KP_NUMLOCK },
	{"ZB_*",         K_KP_STAR },
	{"ZB_EQUALS",        K_KP_EQUALS },

	{"PAUSE", K_PAUSE},

	{"COMMAND", K_COMMAND},  //mac
	{NULL,0}
};  //end german

static const keyname_t VARIABLE_IS_NOT_USED_IN_DEDICATED_SERVER keynames_f[] =    //french
{
	{"TAB", K_TAB},
	{"ENTREE",   K_ENTER},
	{"ECHAP",    K_ESCAPE},
	{"ESPACE",   K_SPACE},
	{"RETOUR",   K_BACKSPACE},
	{"HAUT", K_UPARROW},
	{"BAS",      K_DOWNARROW},
	{"GAUCHE",   K_LEFTARROW},
	{"DROITE",   K_RIGHTARROW},

	{"ALT",      K_ALT},
	{"CTRL", K_CTRL},
	{"MAJ",      K_SHIFT},

	{"VERRMAJ", K_CAPSLOCK},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INSER", K_INS},
	{"SUPPR", K_DEL},
	{"PGBAS", K_PGDN},
	{"PGHAUT", K_PGUP},
	{"ORIGINE", K_HOME},
	{"FIN", K_END},

	{"SOURIS1", K_MOUSE1},
	{"SOURIS2", K_MOUSE2},
	{"SOURIS3", K_MOUSE3},
	{"SOURIS4", K_MOUSE4},
	{"SOURIS5", K_MOUSE5},

	{"MOLETTEHT.",   K_MWHEELUP },
	{"MOLETTEBAS",   K_MWHEELDOWN },

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},
	{"JOY5", K_JOY5},
	{"JOY6", K_JOY6},
	{"JOY7", K_JOY7},
	{"JOY8", K_JOY8},
	{"JOY9", K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},
	{"JOY17", K_JOY17},
	{"JOY18", K_JOY18},
	{"JOY19", K_JOY19},
	{"JOY20", K_JOY20},
	{"JOY21", K_JOY21},
	{"JOY22", K_JOY22},
	{"JOY23", K_JOY23},
	{"JOY24", K_JOY24},
	{"JOY25", K_JOY25},
	{"JOY26", K_JOY26},
	{"JOY27", K_JOY27},
	{"JOY28", K_JOY28},
	{"JOY29", K_JOY29},
	{"JOY30", K_JOY30},
	{"JOY31", K_JOY31},
	{"JOY32", K_JOY32},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},

	{"PN_ORIGINE",       K_KP_HOME },
	{"PN_HAUT",          K_KP_UPARROW },
	{"PN_PGBAS",     K_KP_PGUP },
	{"PN_GAUCHE",        K_KP_LEFTARROW },
	{"PN_5",         K_KP_5 },
	{"PN_DROITE",        K_KP_RIGHTARROW },
	{"PN_FIN",           K_KP_END },
	{"PN_BAS",           K_KP_DOWNARROW },
	{"PN_PGBAS",     K_KP_PGDN },
	{"PN_ENTR",          K_KP_ENTER },
	{"PN_INSER",     K_KP_INS },
	{"PN_SUPPR",     K_KP_DEL },
	{"PN_SLASH",     K_KP_SLASH },
	{"PN_MOINS",     K_KP_MINUS },
	{"PN_PLUS",          K_KP_PLUS },
	{"PN_VERRNUM",       K_KP_NUMLOCK },
	{"PN_*",         K_KP_STAR },
	{"PN_EQUALS",        K_KP_EQUALS },

	{"PAUSE", K_PAUSE},

	{"COMMAND", K_COMMAND},  //mac

	{NULL,0}
};  //end french

static const keyname_t VARIABLE_IS_NOT_USED_IN_DEDICATED_SERVER keynames_s[] =  //Spanish
{
	{"TABULADOR", K_TAB},
	{"INTRO", K_ENTER},
	{"ESC", K_ESCAPE},
	{"BARRA_ESPACIAD", K_SPACE},
	{"RETROCESO", K_BACKSPACE},
	{"CURSOR_ARRIBA", K_UPARROW},
	{"CURSOR_ABAJO", K_DOWNARROW},
	{"CURSOR_IZQDA", K_LEFTARROW},
	{"CURSOR_DERECHA", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"MAY�S", K_SHIFT},

	{"BLOQ_MAY�S", K_CAPSLOCK},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INSERT", K_INS},
	{"SUPR", K_DEL},
	{"AV_P�G", K_PGDN},
	{"RE_P�G", K_PGUP},
	{"INICIO", K_HOME},
	{"FIN", K_END},

	{"RAT�N1", K_MOUSE1},
	{"RAT�N2", K_MOUSE2},
	{"RAT�N3", K_MOUSE3},
	{"RAT�N4", K_MOUSE4},
	{"RAT�N5", K_MOUSE5},

	{"RUEDA_HACIA_ARRIBA",   K_MWHEELUP },
	{"RUEDA_HACIA_ABAJO",    K_MWHEELDOWN },

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},
	{"JOY5", K_JOY5},
	{"JOY6", K_JOY6},
	{"JOY7", K_JOY7},
	{"JOY8", K_JOY8},
	{"JOY9", K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},
	{"JOY17", K_JOY17},
	{"JOY18", K_JOY18},
	{"JOY19", K_JOY19},
	{"JOY20", K_JOY20},
	{"JOY21", K_JOY21},
	{"JOY22", K_JOY22},
	{"JOY23", K_JOY23},
	{"JOY24", K_JOY24},
	{"JOY25", K_JOY25},
	{"JOY26", K_JOY26},
	{"JOY27", K_JOY27},
	{"JOY28", K_JOY28},
	{"JOY29", K_JOY29},
	{"JOY30", K_JOY30},
	{"JOY31", K_JOY31},
	{"JOY32", K_JOY32},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},

	{"INICIO(NUM)",          K_KP_HOME },
	{"ARRIBA(NUM)",      K_KP_UPARROW },
	{"RE_P�G(NUM)",          K_KP_PGUP },
	{"IZQUIERDA(NUM)",   K_KP_LEFTARROW },
	{"5(NUM)",           K_KP_5 },
	{"DERECHA(NUM)", K_KP_RIGHTARROW },
	{"FIN(NUM)",         K_KP_END },
	{"ABAJO(NUM)",   K_KP_DOWNARROW },
	{"AV_P�G(NUM)",          K_KP_PGDN },
	{"INTRO(NUM)",       K_KP_ENTER },
	{"INS(NUM)",         K_KP_INS },
	{"SUPR(NUM)",            K_KP_DEL },
	{"/(NUM)",       K_KP_SLASH },
	{"-(NUM)",       K_KP_MINUS },
	{"+(NUM)",           K_KP_PLUS },
	{"BLOQ_NUM",     K_KP_NUMLOCK },
	{"*(NUM)",           K_KP_STAR },
	{"INTRO(NUM)",       K_KP_EQUALS },

	{"PAUSA", K_PAUSE},

	{"PUNTO_Y_COMA", ';'},    // because a raw semicolon seperates commands

	{"COMANDO", K_COMMAND},  //mac

	{NULL,0}
};

static const keyname_t VARIABLE_IS_NOT_USED_IN_DEDICATED_SERVER keynames_i[] =  //Italian
{
	{"TAB", K_TAB},
	{"INVIO", K_ENTER},
	{"ESC", K_ESCAPE},
	{"SPAZIO", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"FRECCIASU", K_UPARROW},
	{"FRECCIAGI�", K_DOWNARROW},
	{"FRECCIASX", K_LEFTARROW},
	{"FRECCIADX", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"MAIUSC", K_SHIFT},

	{"BLOCMAIUSC", K_CAPSLOCK},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"CANC", K_DEL},
	{"PAGGI�", K_PGDN},
	{"PAGGSU", K_PGUP},
	{"HOME", K_HOME},
	{"FINE", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},

	{"ROTELLASU",    K_MWHEELUP },
	{"ROTELLAGI�",   K_MWHEELDOWN },

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},
	{"JOY5", K_JOY5},
	{"JOY6", K_JOY6},
	{"JOY7", K_JOY7},
	{"JOY8", K_JOY8},
	{"JOY9", K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},
	{"JOY17", K_JOY17},
	{"JOY18", K_JOY18},
	{"JOY19", K_JOY19},
	{"JOY20", K_JOY20},
	{"JOY21", K_JOY21},
	{"JOY22", K_JOY22},
	{"JOY23", K_JOY23},
	{"JOY24", K_JOY24},
	{"JOY25", K_JOY25},
	{"JOY26", K_JOY26},
	{"JOY27", K_JOY27},
	{"JOY28", K_JOY28},
	{"JOY29", K_JOY29},
	{"JOY30", K_JOY30},
	{"JOY31", K_JOY31},
	{"JOY32", K_JOY32},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},

	{"TN_HOME",          K_KP_HOME },
	{"TN_FRECCIASU",     K_KP_UPARROW },
	{"TN_PAGGSU",            K_KP_PGUP },
	{"TN_FRECCIASX", K_KP_LEFTARROW },
	{"TN_5",         K_KP_5 },
	{"TN_FRECCIA_DX",    K_KP_RIGHTARROW },
	{"TN_FINE",          K_KP_END },
	{"TN_FRECCIAGI�",    K_KP_DOWNARROW },
	{"TN_PAGGI�",            K_KP_PGDN },
	{"TN_INVIO",     K_KP_ENTER },
	{"TN_INS",           K_KP_INS },
	{"TN_CANC",          K_KP_DEL },
	{"TN_/",     K_KP_SLASH },
	{"TN_-",     K_KP_MINUS },
	{"TN_+",         K_KP_PLUS },
	{"TN_BLOCNUM",       K_KP_NUMLOCK },
	{"TN_*",         K_KP_STAR },
	{"TN_=",     K_KP_EQUALS },

	{"PAUSA", K_PAUSE},

	{"�", ';'},   // because a raw semicolon seperates commands

	{"COMMAND", K_COMMAND},  //mac

	{NULL,0}
};


/*
===================
Key_SetOverstrikeMode
===================
*/
qboolean Key_GetOverstrikeMode( void ) 
{
	return key_overstrikeMode;
}


/*
===================
Key_SetOverstrikeMode
===================
*/
void Key_SetOverstrikeMode( qboolean state ) 
{
	key_overstrikeMode = state;
}


/*
===================
Key_IsDown
===================
*/
qboolean Key_IsDown( int keynum ) 
{
	if ( keynum < 0 || keynum >= MAX_KEYS ) 
	{
		return qfalse;
	}

	return keys[keynum].down;
}


/*
===================
Key_StringToKeynum

Returns a key number to be used to index keys[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.

0x11 will be interpreted as raw hex, which will allow new controlers

to be configured even if they don't have defined names.
===================
*/
int Key_StringToKeynum( const char *str ) {
	const keyname_t	*kn;
	
	if ( !str || str[0] == '\0' ) {
		return -1;
	}
	if ( str[1] == '\0' ) {
		return tolower(str[0]);
	}

	// check for hex code
	if ( strlen( str ) == 4 ) {
		int n = Com_HexStrToInt( str );
		
		if ( n >= 0 ) {
			return n;
		}
	}

	// scan for a text match
	for ( kn = keynames ; kn->name ; kn++ ) {
		if ( !Q_stricmp( str, kn->name ) )
			return kn->keynum;
	}

	return -1;
}


/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, a K_* name, or a 0x11 hex string) for the
given keynum.
===================
*/
const char *Key_KeynumToString( int keynum, qboolean bTranslate ) {
	const keyname_t *kn;
	static char tinystr[5];
	int i, j;

	if ( keynum == -1 ) {
		return "<KEY NOT FOUND>";
	}

	if ( keynum < 0 || keynum >= MAX_KEYS ) {
		return "<OUT OF RANGE>";
	}

	// check for printable ascii (don't use quote)
	//if ( keynum > 32 && keynum < 127 && keynum != '"' && keynum != ';' ) {
	if ( keynum > ' ' && keynum < '~' && keynum != '"' ) {
		tinystr[0] = keynum;
		tinystr[1] = '\0';
		if ( keynum == ';' && !bTranslate ) {
			//fall through and use keyname table
		} else {
			return tinystr;
		}
	}

	kn = keynames;    //init to english
#ifndef DEDICATED
#ifndef __MACOS__   //DAJ USA
	if ( bTranslate ) {
		if ( cl_language->integer - 1 == LANGUAGE_FRENCH ) {
			kn = keynames_f;  //use french
		} else if ( cl_language->integer - 1 == LANGUAGE_GERMAN ) {
			kn = keynames_d;  //use german
		} else if ( cl_language->integer - 1 == LANGUAGE_ITALIAN ) {
			kn = keynames_i;  //use italian
		} else if ( cl_language->integer - 1 == LANGUAGE_SPANISH ) {
			kn = keynames_s;  //use spanish
		}
	}
#endif
#endif

	// check for a key string
	for ( ; kn->name ; kn++ ) {
		if ( keynum == kn->keynum ) {
			return kn->name;
		}
	}

	// make a hex string
	i = keynum >> 4;
	j = keynum & 15;

	tinystr[0] = '0';
	tinystr[1] = 'x';
	tinystr[2] = i > 9 ? i - 10 + 'a' : i + '0';
	tinystr[3] = j > 9 ? j - 10 + 'a' : j + '0';
	tinystr[4] = 0;

	return tinystr;
}

// ENSI CHECKME do we need the hash binds from ET?

/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding( int keynum, const char *binding ) {
	if ( keynum < 0 || keynum >= MAX_KEYS ) {
		return;
	}

	// free old bindings
	if ( keys[ keynum ].binding ) {
		Z_Free( keys[ keynum ].binding );
	}
		
	// allocate memory for new binding
	keys[ keynum ].binding = CopyString( binding );

	// consider this like modifying an archived cvar, so the
	// file write will be triggered at the next oportunity
	cvar_modifiedFlags |= CVAR_ARCHIVE;
}


/*
===================
Key_GetBinding
===================
*/
const char *Key_GetBinding( int keynum ) {
	if ( keynum < 0 || keynum >= MAX_KEYS ) {
		return "";
	}

	return keys[ keynum ].binding;
}


// binding MUST be lower case
void Key_GetBindingByString( const char* binding, int* key1, int* key2 ) {
	int i;

	*key1 = -1;
	*key2 = -1;

	for ( i = 0; i < MAX_KEYS; i++ ) {
		if ( !Q_stricmp( binding, keys[i].binding ) ) {
			if ( *key1 == -1 ) {
				*key1 = i;
			} else if ( *key2 == -1 ) {
				*key2 = i;
				return;
			}
		}
	}
}


/* 
===================
Key_GetKey
===================
*/
int Key_GetKey( const char *binding ) {
	int i;

	if ( binding ) {
		for ( i = 0 ; i < MAX_KEYS ; i++ ) {
			if ( keys[i].binding && Q_stricmp( binding, keys[i].binding ) == 0 ) {
				return i;
			}
		}
	}
	return -1;
}


/*
===================
Key_Unbind_f
===================
*/
static void Key_Unbind_f( void )
{
	int		b;

	if ( Cmd_Argc() != 2 )
	{
		Com_Printf( "unbind <key> : remove commands from a key\n" );
		return;
	}
	
	b = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if ( b == -1 )
	{
		Com_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	Key_SetBinding( b, "" );
}


/*
===================
Key_Unbindall_f
===================
*/
static void Key_Unbindall_f( void )
{
	int		i;
	
	for ( i = 0 ; i < MAX_KEYS; i++ ) 
	{
		if ( keys[i].binding )
		{
			Key_SetBinding( i, "" );
		}
	}
}


/*
===================
Key_Bind_f
===================
*/
static void Key_Bind_f( void )
{
	int c, b;
	
	c = Cmd_Argc();

	if ( c < 2 )
	{
		Com_Printf( "bind <key> [command] : attach a command to a key\n" );
		return;
	}

	b = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if ( b == -1 )
	{
		Com_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	if ( c == 2 )
	{
		if ( keys[b].binding && keys[b].binding[0] )
			Com_Printf( "\"%s\" = \"%s\"\n", Cmd_Argv( 1 ), keys[b].binding );
		else
			Com_Printf( "\"%s\" is not bound\n", Cmd_Argv( 1 ) );
		return;
	}
	
	// copy the rest of the command line
	Key_SetBinding( b, Cmd_ArgsFrom( 2 ) );
}


/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings( fileHandle_t f ) {
	int		i;

	FS_Printf( f, "unbindall" Q_NEWLINE );

	for ( i = 0 ; i < MAX_KEYS ; i++ ) {
		if ( keys[i].binding && keys[i].binding[0] ) {
			FS_Printf( f, "bind %s \"%s\"" Q_NEWLINE, Key_KeynumToString(i, qfalse), keys[i].binding );
		}
	}
}


/*
============
Key_Bindlist_f
============
*/
static void Key_Bindlist_f( void ) {
	int		i;

	for ( i = 0 ; i < MAX_KEYS ; i++ ) {
		if ( keys[i].binding && keys[i].binding[0] ) {
			Com_Printf( "%s \"%s\"\n", Key_KeynumToString(i, qfalse), keys[i].binding );
		}
	}
}


/*
============
Key_KeynameCompletion
============
*/
// ENSI TODO, use language specific arrays
void Key_KeynameCompletion( void(*callback)(const char *s) ) {
	int	i;

	for( i = 0; keynames[ i ].name != NULL; i++ )
		callback( keynames[ i ].name );
}


/*
====================
Key_CompleteBind
====================
*/
static void Key_CompleteBind( char *args, int argNum )
{
	char *p;

	if ( argNum == 2 )
	{
		// Skip "bind "
		p = Com_SkipTokens( args, 1, " " );

		if ( p > args )
			Field_CompleteKeyname();
	}
	else if ( argNum >= 3 )
	{
		int key;
		// Skip "bind <key> "
		p = Com_SkipTokens( args, 2, " " );
		if ( *p == '\0' && ( key = Key_StringToKeynum( Cmd_Argv( 1 ) ) ) >= 0 ) {
			Field_CompleteKeyBind( key );
		} else if ( p > args ) {
			Field_CompleteCommand( p, qtrue, qtrue );
		}
	}
}


/*
====================
Key_CompleteUnbind
====================
*/
static void Key_CompleteUnbind( char *args, int argNum )
{
	if ( argNum == 2 )
	{
		// Skip "unbind "
		char *p = Com_SkipTokens( args, 1, " " );

		if ( p > args )
			Field_CompleteKeyname();
	}
}

#ifndef DEDICATED
qboolean CL_BindUICommand( const char *cmd );
#endif

/*
===================
Key_ParseBinding

Execute the commands in the bind string
===================
*/
void Key_ParseBinding( int key, qboolean down, unsigned time, qboolean forceAll )
{
	char buf[ MAX_STRING_CHARS ], *p, *end;
	qboolean allCommands, allowUpCmds;

#ifndef DEDICATED
	if ( cls.state == CA_DISCONNECTED && Key_GetCatcher() == 0 )
		return;
#endif

	if( !keys[key].binding || !keys[key].binding[0] )
		return;

	p = buf;

	Q_strncpyz( buf, keys[key].binding, sizeof( buf ) );

#ifndef DEDICATED
	// run all bind commands if console, ui, etc aren't reading keys
	allCommands = forceAll || ( Key_GetCatcher() == 0 );

	// allow button up commands if in game even if key catcher is set
	allowUpCmds = ( cls.state != CA_DISCONNECTED );
#else
	allCommands = forceAll || qtrue;

	allowUpCmds = qfalse;
#endif

	while( 1 )
	{
		while ( isspace( *p ) )
			p++;
		end = strchr( p, ';' );
		if ( end )
			*end = '\0';
		if ( *p == '+' )
		{
			// button commands add keynum and time as parameters
			// so that multiple sources can be discriminated and
			// subframe corrected
			if ( allCommands || ( allowUpCmds && !down ) ) {
				char cmd[1024];
				Com_sprintf( cmd, sizeof( cmd ), "%c%s %d %d\n",
					( down ) ? '+' : '-', p + 1, key, time );
				Cbuf_AddText( cmd );
			}
		}
		else if( down )
		{
			// normal commands only execute on key press
#ifndef DEDICATED
			if ( allCommands || CL_BindUICommand( p ) )
#else
			if ( allCommands )
#endif
			{

				Cbuf_AddText( p );
				Cbuf_AddText( "\n" );
			}
		}
		if( !end )
			break;
		p = end + 1;
	}
}


/*
===================
Com_InitKeyCommands
===================
*/
void Com_InitKeyCommands( void ) 
{
	// register our functions
	Cmd_AddCommand( "bind", Key_Bind_f );
	Cmd_SetCommandCompletionFunc( "bind", Key_CompleteBind );
	Cmd_AddCommand( "unbind", Key_Unbind_f );
	Cmd_SetCommandCompletionFunc( "unbind", Key_CompleteUnbind );
	Cmd_AddCommand( "unbindall", Key_Unbindall_f );
	Cmd_AddCommand( "bindlist", Key_Bindlist_f );
}
