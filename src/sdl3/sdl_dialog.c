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

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include "../qcommon/q_shared.h"

dialogResult_t Sys_SDLDialog(dialogType_t type, const char *message, const char *title)
{
	int                      buttonId;
	SDL_MessageBoxButtonData buttons[2];
	SDL_MessageBoxData       data;
	data.window      = NULL;
	data.colorScheme = NULL;
	data.buttons     = buttons;
	data.message     = message;
	data.title       = title;

	switch (type)
	{
	default:
	case DT_INFO:
		buttons[0].flags    = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
		buttons[0].buttonid = DR_OK;
		buttons[0].text     = _("Ok");
		data.numbuttons     = 1;
		data.flags          = SDL_MESSAGEBOX_INFORMATION;
		break;
	case DT_WARNING:
		buttons[0].flags    = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
		buttons[0].buttonid = DR_OK;
		buttons[0].text     = _("Ok");
		data.numbuttons     = 1;
		data.flags          = SDL_MESSAGEBOX_WARNING;
		break;
	case DT_ERROR:
		buttons[0].flags    = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
		buttons[0].buttonid = DR_OK;
		buttons[0].text     = _("Ok");
		data.numbuttons     = 1;
		data.flags          = SDL_MESSAGEBOX_ERROR;
		break;
	case DT_YES_NO:
		buttons[0].flags    = 0;
		buttons[0].buttonid = DR_NO;
		buttons[0].text     = _("No");
		buttons[1].flags    = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
		buttons[1].buttonid = DR_YES;
		buttons[1].text     = _("Yes");
		data.numbuttons     = 2;
		data.flags          = SDL_MESSAGEBOX_INFORMATION;
		break;
	case DT_OK_CANCEL:
		buttons[0].flags    = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		buttons[0].buttonid = DR_CANCEL;
		buttons[0].text     = _("Cancel");
		buttons[1].flags    = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
		buttons[1].buttonid = DR_OK;
		buttons[1].text     = _("Ok");
		data.numbuttons     = 2;
		data.flags          = SDL_MESSAGEBOX_WARNING;
		break;
	}

	if (SDL_ShowMessageBox(&data, &buttonId) < 0)
	{
		Com_Printf(S_COLOR_RED "error displaying message box\n");
		return DR_ERROR;
	}

	if (buttonId == -1)
	{
		Com_Printf(S_COLOR_RED "no selection\n");
		return DR_CANCEL;
	}

	return buttonId;
}
