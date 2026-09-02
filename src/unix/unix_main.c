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
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "linux_local.h"
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <libgen.h> // dirname
#include <dlfcn.h>

#ifdef __linux__
#ifdef __GLIBC__
#include <fpu_control.h> // bk001213 - force dumps on divide by zero
#endif
#endif

#if defined(__sun)
#include <sys/file.h>
#endif

// FIXME TTimo should we gard this? most *nix system should comply?
#include <termios.h>

#ifndef DEDICATED
#include "../client/client.h"
#endif


#if defined(USE_SDL3) && !defined(DEDICATED)
#include <SDL3/SDL_main.h>
#endif

unsigned sys_frame_time;

qboolean stdin_active = qfalse;
int stdin_flags = 0;

// =============================================================
// tty console variables
// =============================================================

typedef enum
{
	TTY_ENABLED,
	TTY_DISABLED,
	TTY_ERROR
} tty_err;

// enable/disabled tty input mode
// NOTE TTimo this is used during startup, cannot be changed during run
static cvar_t *ttycon = NULL;

// general flag to tell about tty console mode
static qboolean ttycon_on = qfalse;

// when printing general stuff to stdout stderr (Sys_Printf)
//   we need to disable the tty console stuff
// this increments so we can recursively disable
static int ttycon_hide = 0;

// some key codes that the terminal may be using
// TTimo NOTE: I'm not sure how relevant this is
static int tty_erase;
static int tty_eof;

static struct termios tty_tc;

static field_t tty_con;

static cvar_t *ttycon_ansicolor = NULL;
static qboolean ttycon_color_on = qfalse;

tty_err Sys_ConsoleInputInit(void);

// =======================================================================
// General routines
// =======================================================================

#define MEM_THRESHOLD 96 * 1024

/*
==================
Sys_LowPhysicalMemory
==================
*/
qboolean Sys_LowPhysicalMemory(void)
{
	FILE *fp;
	uint32_t totalkb;

	fp = fopen("/proc/meminfo", "r");
	if (!fp)
		return qfalse;
	if (fscanf(fp, "MemTotal: %d kB", &totalkb) < 1)
	{
		// failed to parse for some reason, abort;
		fclose(fp);
		return qfalse;
	}
	fclose(fp);
	return (totalkb <= MEM_THRESHOLD) ? qtrue : qfalse;
}

void Sys_BeginProfiling(void)
{
}

// =============================================================
// tty console routines
// NOTE: if the user is editing a line when something gets printed to the early console then it won't look good
//   so we provide tty_Clear and tty_Show to be called before and after a stdout or stderr output
// =============================================================

// flush stdin, I suspect some terminals are sending a LOT of shit
// FIXME TTimo relevant?
static void tty_FlushIn(void)
{
#if 1
	tcflush(STDIN_FILENO, TCIFLUSH);
#else
	char key;
	while (read(STDIN_FILENO, &key, 1) > 0)
		;
#endif
}

// do a backspace
// TTimo NOTE: it seems on some terminals just sending '\b' is not enough
//   so for now, in any case we send "\b \b" .. yeah well ..
//   (there may be a way to find out if '\b' alone would work though)
static void tty_Back(void)
{
	(void)!write(STDOUT_FILENO, "\b \b", 3);
}

// clear the display of the line currently edited
// bring cursor back to beginning of line
void tty_Hide(void)
{
	int i;

	if (!ttycon_on)
		return;

	if (ttycon_hide)
	{
		ttycon_hide++;
		return;
	}

	if (tty_con.cursor > 0)
	{
		for (i = 0; i < tty_con.cursor; i++)
		{
			tty_Back();
		}
	}
	tty_Back(); // delete "]" ? -EC-
	ttycon_hide++;
}

// show the current line
// FIXME TTimo need to position the cursor if needed??
void tty_Show(void)
{
	if (!ttycon_on)
		return;

	if (ttycon_hide > 0)
	{
		ttycon_hide--;
		if (ttycon_hide == 0)
		{
			(void)!write(STDOUT_FILENO, "]", 1); // -EC-

			if (tty_con.cursor > 0)
			{
				(void)!write(STDOUT_FILENO, tty_con.buffer, tty_con.cursor);
			}
		}
	}
}

// never exit without calling this, or your terminal will be left in a pretty bad state
void Sys_ConsoleInputShutdown(void)
{
	if (ttycon_on)
	{
		//		Com_Printf( "Shutdown tty console\n" ); // -EC-
		tty_Back(); // delete "]" ? -EC-
		tcsetattr(STDIN_FILENO, TCSADRAIN, &tty_tc);
	}

	// Restore blocking to stdin reads
	if (stdin_active)
	{
		fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
		//		fcntl( STDIN_FILENO, F_SETFL, fcntl( STDIN_FILENO, F_GETFL, 0 ) & ~O_NONBLOCK );
	}

	Com_Memset(&tty_con, 0, sizeof(tty_con));

	stdin_active = qfalse;
	ttycon_on = qfalse;

	ttycon_hide = 0;
}

/*
==================
CON_SigCont
Reinitialize console input after receiving SIGCONT, as on Linux the terminal seems to lose all
set attributes if user did CTRL+Z and then does fg again.
==================
*/
void CON_SigCont(int signum)
{
	Sys_ConsoleInputInit();
}

void CON_SigTStp(int signum)
{
	sigset_t mask;

	tty_FlushIn();
	Sys_ConsoleInputShutdown();

	sigemptyset(&mask);
	sigaddset(&mask, SIGTSTP);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	signal(SIGTSTP, SIG_DFL);

	kill(getpid(), SIGTSTP);
}

// =============================================================
// general sys routines
// =============================================================

#define MAX_CMD 1024
static char exit_cmdline[MAX_CMD] = "";
void Sys_DoStartProcess(const char *cmdline);

// single exit point (regular exit or in case of signal fault)
void Q_NO_RETURN Sys_Exit(int code)
{
	Sys_ConsoleInputShutdown();

	// we may be exiting to spawn another process
	if (exit_cmdline[0] != '\0')
	{
		// possible race conditions?
		// buggy kernels / buggy GL driver, I don't know for sure
		// but it's safer to wait an eternity before and after the fork
		sleep(1);
		Sys_DoStartProcess(exit_cmdline);
		sleep(1);
	}

#ifdef NDEBUG // regular behavior
	// We can't do this
	//  as long as GL DLL's keep installing with atexit...
	//exit(ex);
	_exit(code);
#else
	// Give me a backtrace on error exits.
	assert(code == 0);
	exit(code);
#endif
}

void Q_NO_RETURN Sys_Quit(void)
{
	Sys_Exit(0);
}

void Sys_Init(void)
{
	Cvar_Set("arch", OS_STRING " " ARCH_STRING);
	//IN_Init();   // rcg08312005 moved into glimp.
}

void Q_NO_RETURN Q_PRINTF_FUNC(1, 2) Sys_Error(const char *format, ...)
{
	va_list argptr;
	char text[1024];

	// change stdin to non blocking
	// NOTE TTimo not sure how well that goes with tty console mode
	if (stdin_active)
	{
		//		fcntl( STDIN_FILENO, F_SETFL, fcntl( STDIN_FILENO, F_GETFL, 0) & ~FNDELAY );
		fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
	}

	// don't bother do a show on this one heh
	if (ttycon_on)
	{
		tty_Hide();
	}

	va_start(argptr, format);
	Q_vsnprintf(text, sizeof(text), format, argptr);
	va_end(argptr);

#ifndef DEDICATED
	if ( !com_errorEntered && com_cl_running && com_cl_running->integer )
		CL_Shutdown(text, qtrue);
#endif

	fprintf(stderr, "Sys_Error: %s\n", text);

	Sys_Exit(1); // bk010104 - use single exit point.
}

void floating_point_exception_handler(int whatever)
{
	signal(SIGFPE, floating_point_exception_handler);
}

// initialize the console input (tty mode if wanted and possible)
// warning: might be called from signal handler
tty_err Sys_ConsoleInputInit(void)
{
	struct termios tc;
	const char *term;

	// TTimo
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=390
	// ttycon 0 or 1, if the process is backgrounded (running non interactively)
	// then SIGTTIN or SIGTOU is emitted, if not catched, turns into a SIGSTP
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	// If SIGCONT is received, reinitialize console
	signal(SIGCONT, CON_SigCont);

	if (signal(SIGTSTP, SIG_IGN) == SIG_DFL)
	{
		signal(SIGTSTP, CON_SigTStp);
	}

	stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	if (stdin_flags == -1)
	{
		stdin_active = qfalse;
		return TTY_ERROR;
	}

	// set non-blocking mode
	fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);
	stdin_active = qtrue;

	// FIXME TTimo initialize this in Sys_Init or something?
	if (!ttycon || !ttycon->integer)
	{
		ttycon_on = qfalse;
		return TTY_DISABLED;
	}
	term = getenv("TERM");
	if (isatty(STDIN_FILENO) != 1 || !term || !strcmp(term, "dumb") || !strcmp(term, "raw"))
	{
		ttycon_on = qfalse;
		return TTY_ERROR;
	}

	Field_Clear(&tty_con);
	tcgetattr(STDIN_FILENO, &tty_tc);
	tty_erase = tty_tc.c_cc[VERASE];
	tty_eof = tty_tc.c_cc[VEOF];
	tc = tty_tc;

	/*
		ECHO: don't echo input characters
		ICANON: enable canonical mode.  This  enables  the  special
			characters  EOF,  EOL,  EOL2, ERASE, KILL, REPRINT,
			STATUS, and WERASE, and buffers by lines.
		ISIG: when any of the characters  INTR,  QUIT,  SUSP,  or
			DSUSP are received, generate the corresponding signal
	*/
	tc.c_lflag &= ~(ECHO | ICANON);
	/*
		ISTRIP strip off bit 8
		INPCK enable input parity checking
	*/
	tc.c_iflag &= ~(ISTRIP | INPCK);
	tc.c_cc[VMIN] = 1;
	tc.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSADRAIN, &tc);

	if (ttycon_ansicolor && ttycon_ansicolor->integer)
	{
		ttycon_color_on = qtrue;
	}

	ttycon_on = qtrue;

	tty_Hide();
	tty_Show();

	return TTY_ENABLED;
}

char *Sys_ConsoleInput(void)
{
	// we use this when sending back commands
	static char text[sizeof(tty_con.buffer)];
	int avail;
	char key;
	char *s;
	field_t history;

	if (ttycon_on)
	{
		avail = read(STDIN_FILENO, &key, 1);
		if (avail != -1)
		{
			// we have something
			// backspace?
			// NOTE TTimo testing a lot of values .. seems it's the only way to get it to work everywhere
			if ((key == tty_erase) || (key == 127) || (key == 8))
			{
				if (tty_con.cursor > 0)
				{
					tty_con.cursor--;
					tty_con.buffer[tty_con.cursor] = '\0';
					tty_Back();
				}
				return NULL;
			}

			// check if this is a control char
			if (key && key < ' ')
			{
				if (key == '\n')
				{
					// push it in history
					Con_SaveField(&tty_con);
					s = tty_con.buffer;
					while (*s == '\\' || *s == '/') // skip leading slashes
						s++;
					Q_strncpyz(text, s, sizeof(text));
					Field_Clear(&tty_con);
					(void)!write(STDOUT_FILENO, "\n]", 2);
					return text;
				}

				if (key == '\t')
				{
					tty_Hide();
					Field_AutoComplete(&tty_con);
					tty_Show();
					return NULL;
				}

				avail = read(STDIN_FILENO, &key, 1);
				if (avail != -1)
				{
					// VT 100 keys
					if (key == '[' || key == 'O')
					{
						avail = read(STDIN_FILENO, &key, 1);
						if (avail != -1)
						{
							switch (key)
							{
							case 'A':
								if (Con_HistoryGetPrev(&history))
								{
									tty_Hide();
									tty_con = history;
									tty_Show();
								}
								tty_FlushIn();
								return NULL;
								break;
							case 'B':
								if (Con_HistoryGetNext(&history))
								{
									tty_Hide();
									tty_con = history;
									tty_Show();
								}
								tty_FlushIn();
								return NULL;
								break;
							case 'C': // right
							case 'D': // left
									  //case 'H': // home
									  //case 'F': // end
								return NULL;
							}
						}
					}
				}

				if (key == 12) // clear teaminal
				{
					(void)!write(STDOUT_FILENO, "\ec]", 3);
					if (tty_con.cursor)
					{
						(void)!write(STDOUT_FILENO, tty_con.buffer, tty_con.cursor);
					}
					tty_FlushIn();
					return NULL;
				}

				Com_DPrintf("dropping ISCTL sequence: %d, tty_erase: %d\n", key, tty_erase);
				tty_FlushIn();
				return NULL;
			}
			if (tty_con.cursor >= sizeof(text) - 1)
				return NULL;
			// push regular character
			tty_con.buffer[tty_con.cursor] = key;
			tty_con.cursor++;
			// print the current line (this is differential)
			(void)!write(STDOUT_FILENO, &key, 1);
		}
		return NULL;
	}
	else if (stdin_active && com_dedicated->integer)
	{
		int len;
		fd_set fdset;
		struct timeval timeout;

		FD_ZERO(&fdset);
		FD_SET(STDIN_FILENO, &fdset); // stdin
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if (select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(STDIN_FILENO, &fdset))
		{
			return NULL;
		}

		len = read(STDIN_FILENO, text, sizeof(text));
		if (len == 0) // eof!
		{
			fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
			stdin_active = qfalse;
			return NULL;
		}

		if (len < 1)
			return NULL;

		text[len - 1] = '\0'; // rip off the /n and terminate
		s = text;

		while (*s == '\\' || *s == '/') // skip leading slashes
			s++;

		return s;
	}

	return NULL;
}

/*
=================
Sys_SendKeyEvents

Platform-dependent event handling
=================
*/
void Sys_SendKeyEvents(void)
{
#ifndef DEDICATED
	HandleEvents();
#endif
}

/*
==================
Sys_Sleep

Block execution for msec or until input is received.
==================
*/
void Sys_Sleep(int msec)
{
	struct timeval timeout;
	struct timespec req;
	fd_set fdset;
	int res;

	//if (msec == 0)
	//	return;

	if (msec < 0)
	{
		// special case: wait for console input or network packet
		if (stdin_active)
		{
			msec = 300;
			do
			{
				FD_ZERO(&fdset);
				FD_SET(STDIN_FILENO, &fdset);
				timeout.tv_sec = msec / 1000;
				timeout.tv_usec = (msec % 1000) * 1000;
				res = select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout);
			} while (res == 0 && NET_Sleep(10 * 1000));
		}
		else
		{
			// can happen only if no map loaded
			// which means we totally stuck as stdin is also disabled :P
			//usleep( 300 * 1000 );
			while (NET_Sleep(3000 * 1000))
				;
		}
		return;
	}
#if 1
	req.tv_sec = msec / 1000;
	req.tv_nsec = ( msec % 1000 ) * 1000000;
	nanosleep( &req, NULL );
#else
	if (com_dedicated->integer && stdin_active)
	{
		FD_ZERO(&fdset);
		FD_SET(STDIN_FILENO, &fdset);
		timeout.tv_sec = msec / 1000;
		timeout.tv_usec = (msec % 1000) * 1000;
		select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout);
	}
	else
	{
		usleep(msec * 1000);
	}
#endif
}

#ifndef DEDICATED
static qboolean Sys_DLLNeedsUnpacking(const char *name)
{
#if defined(DEDICATED)
	return qfalse;
#else
	if (!Q_stricmpn(name, "qagame", 6))
		return qfalse;
#ifdef _DEBUG
	return cl_connectedToPureServer != 0 ? qtrue : qfalse;
#else
	return qtrue;
#endif
#endif
}
#endif

/*
=================
Sys_LoadDll

Used to load a development dll instead of a virtual machine

NOTE: DLL checksuming / DLL pk3 and search paths

if we are connecting to an sv_pure server, the .so will be extracted
from a pk3 and placed in the homepath (i.e. the write area)

since DLLs are looked for in several places, it is important that
we are sure we are going to load the DLL that was just extracted

for convenience, debug builds are searching in current directory
prior to scanning homepath and basepath. this allows tu run stuff
'out of the box' with a +set fs_basepath after compile.

in release we start searching directly in fs_homepath then fs_basepath
(that makes sure we are going to load the DLL we extracted from pk3)
this should not be a problem for mod makers, since they always copy their
DLL to the main installation prior to run (sv_pure 1 would have a tendency
to kill their compiled DLL with extracted ones though :-( )
=================
*/
static void *try_dlopen(const char *base, const char *gamedir, const char *fname)
{
	void *libHandle;
	char *fn;

	fn = FS_BuildOSPath(base, gamedir, fname);
	Com_Printf("Sys_LoadGameDLL(%s)... \n", fn);

	libHandle = Sys_LoadLibrary(fn);

	if (!libHandle)
	{
		Com_Printf("Sys_LoadGameDLL(%s) failed:\n\"%s\"\n", fn, dlerror());
		return NULL;
	}

	Com_Printf("Sys_LoadGameDLL(%s): succeeded ...\n", fn);

	return libHandle;
}


// TTimo - Wolf MP specific, adding .mp. to shared objects
#ifdef __APPLE__
const char *Sys_GetDLLName(const char *name)
{
#ifdef _DEBUG
	return va("%s_d_mac", name);
#else
	return va("%s_mac", name);
#endif
}
#else
const char *Sys_GetDLLName(const char *name)
{
	return va("%s.mp." ARCH_STRING DLL_EXT, name);
}
#endif

void *Sys_LoadGameDll(const char *name, vmMain_t *entryPoint, dllSyscall_t systemcalls)
{
	void *libHandle;
	dllEntry_t dllEntry;
	#ifdef _DEBUG
		char		currpath[MAX_OSPATH];
	#endif
	char fname[MAX_OSPATH];
	const char *basepath;
	const char *homepath;
	const char *gamedir;
#if !defined(DEDICATED)
	char *fn = NULL;
	qboolean unpack = qfalse;
#endif
	const char *err = NULL;

	Q_strncpyz(fname, Sys_GetDLLName(name), sizeof(fname));

	basepath = Cvar_VariableString("fs_basepath");
	homepath = Cvar_VariableString("fs_homepath");
	gamedir = FS_GetCurrentGameDir();

	// Use Sys_Pwd() instead?
#ifdef _DEBUG
	if (getcwd(currpath, sizeof(currpath)))
		libHandle = try_dlopen(currpath, gamedir, fname);
	else
#endif
		libHandle = NULL;

#ifdef _DEBUG
#define SEARCHPATH1 basepath
#define SEARCHPATH2 homepath
#else
#define SEARCHPATH1 homepath
#define SEARCHPATH2 basepath
#endif

		// this is relevant to client only
		// this code is in for full client hosting a game, but it's not affected by it
#if !defined(DEDICATED)
	fn = FS_BuildOSPath( homepath, gamedir, fname );
	unpack = Sys_DLLNeedsUnpacking(name);

	// NERVE - SMF - extract dlls from pak file for security
	// we have to handle the game dll a little differently
	// NOTE #2: we may have found a file in fs_basepath, and if the checksum is wrong, FS_Extract will write in fs_homepath
	//   won't be a problem since we start a brand new scan next
	if (unpack)
	{
		if (!FS_CL_ExtractFromPakFile(fn, gamedir, fname, NULL))
		{
			Com_Printf("Sys_LoadGameDLL(%s/%s) failed to extract library\n", gamedir, name);
		}
		else
		{
			Com_Printf("Sys_LoadGameDLL(%s/%s) library extraction succeeded\n", gamedir, name);
		}
	}
#endif

	if (!libHandle && SEARCHPATH1 && SEARCHPATH1[0])
		libHandle = try_dlopen(SEARCHPATH1, gamedir, fname);

	if (!libHandle && SEARCHPATH2 && SEARCHPATH2[0])
		libHandle = try_dlopen(SEARCHPATH2, gamedir, fname);

#ifndef DEDICATED
	if ( Q_stricmpn( name, "qagame", 6) != 0 ) {
		if ( !libHandle && !unpack ) {
			fn = FS_BuildOSPath( homepath, gamedir, fname );
			if (!FS_CL_ExtractFromPakFile(fn, gamedir, fname, NULL))
			{
				Com_Printf("Sys_LoadGameDLL(%s/%s) failed to extract library\n", gamedir, name);
				return NULL;
			}
			Com_Printf("Sys_LoadGameDLL(%s/%s) library extraction succeeded\n", gamedir, name);
			libHandle = try_dlopen(homepath, gamedir, fname);
		}
	}
#endif

	// Last resort for missing DLLs or media mods
	// If mod requires a different cgame/ui this could cause problems
	if ( !libHandle && !strcmp( name, "ui" ) && strcmp( gamedir, BASEGAME ) != 0 )
	{
		const char *temp = va( "%s%c%s", gamedir, PATH_SEP, fname );
		FS_SetFilterFlag( FS_EXCLUDE_OTHERGAMES );
		if ( !FS_SV_FileExists( temp ) && !FS_FileIsInPAK( fname, NULL, NULL ) ) {
			Com_Printf( "Sys_LoadDLL(%s/%s) trying %s override\n", gamedir, name, BASEGAME );
			if (!libHandle && SEARCHPATH1 && SEARCHPATH1[0])
				libHandle = try_dlopen(SEARCHPATH1, BASEGAME, fname);

			if (!libHandle && SEARCHPATH2 && SEARCHPATH2[0])
				libHandle = try_dlopen(SEARCHPATH2, BASEGAME, fname);
		}
		FS_SetFilterFlag( 0 );
	}

	if (!libHandle)
	{
#ifdef _DEBUG // in debug abort on failure
		Com_Error(ERR_FATAL, "Sys_LoadGameDLL(%s/%s) failed dlopen() completely!", gamedir, name);
#else
		Com_Printf("Sys_LoadGameDLL(%s/%s) failed dlopen() completely!\n", gamedir, name);
#endif
		return NULL;
	}

	Sys_LoadFunctionErrors(); // reset counter

	dllEntry = (dllEntry_t)Sys_LoadFunction(libHandle, "dllEntry");
	*entryPoint = (vmMain_t)Sys_LoadFunction(libHandle, "vmMain");

	if (!*entryPoint || !dllEntry)
	{
		err = dlerror();
#ifdef _DEBUG
		Com_Error(ERR_FATAL, "Sys_LoadGameDLL(%s/%s) failed dlsym(vmMain):\n\"%s\" !", gamedir, name, err);
#else
		Com_Printf("Sys_LoadGameDLL(%s/%s) failed dlsym(vmMain):\n\"%s\"\n", gamedir, name, err);
#endif
		Sys_UnloadLibrary(libHandle);
		err = dlerror();
		if (err != NULL)
		{
			Com_Printf("Sys_LoadGameDLL(%s/%s) failed dlcose:\n\"%s\"\n", gamedir, name, err);
		}
		return NULL;
	}

	Com_Printf("Sys_LoadGameDLL(%s/%s) found **vmMain** at %p\n", gamedir, name, *entryPoint);
	dllEntry(systemcalls);
	Com_Printf("Sys_LoadGameDLL(%s/%s) succeeded!\n", gamedir, name);

	return libHandle;
}

/*****************************************************************************/

// colors hash from http://wolfwiki.anime.net/index.php/Color_Codes
static const int etAnsiHash[][6] =
{
	// here should be black, but it's invisible in terminal
	// so you see dark grey here. like in 9 color
	{ 1,  30, 0,   '0', 'p',  'P'  },
	{ 1,  31, 0,   '1', 'q',  'Q'  },
	{ 1,  32, 0,   '2', 'r',  'R'  },
	{ 1,  33, 0,   '3', 's',  'S'  },
	{ 1,  34, 0,   '4', 't',  'T'  },
	{ 1,  36, 0,   '5', 'u',  'U'  },
	{ 1,  35, 0,   '6', 'v',  'V'  },
	{ 1,  37, 0,   '7', 'w',  'W'  },

	{ 38, 5,  208, '8', 'x',  'X'  },
	{ 1,  30, 0,   '9', 'y',  'Y'  },
	{ 0,  37, 0,   'z', 'Z',  ':'  }, // the same
	{ 0,  37, 0,   '[', '{',  ';'  }, // --------
	{ 0,  32, 0,   '<', '\\', '|'  },
	{ 0,  33, 0,   '=', ']',  '}'  },
	{ 0,  34, 0,   '>', '~',  '^'  },
	{ 0,  31, 0,   '?', '_',  0    },
	{ 38, 5,  94,  '@', '`',  0    },

	{ 38, 5,  214, 'a', 'A',  '!'  },
	{ 38, 5,  30,  'b', 'B',  '"'  },
	{ 38, 5,  90,  'c', 'C',  '#'  },
	{ 38, 5,  33,  'd', 'D',  '$'  },
	{ 38, 5,  93,  'e', 'E',  '%'  },
	{ 38, 5,  38,  'f', 'F',  '&'  },
	{ 38, 5,  194, 'g', 'G',  '\'' },
	{ 38, 5,  29,  'h', 'H',  '('  },

	{ 38, 5,  197, 'i', 'I',  ')'  },
	{ 38, 5,  124, 'j', 'J',  0    },
	{ 38, 5,  130, 'k', 'K',  '+'  },
	{ 38, 5,  179, 'l', 'L',  ','  },
	{ 38, 5,  143, 'm', 'M',  '-'  },
	{ 38, 5,  229, 'n', 'N',  '.'  },
	{ 38, 5,  228, 'o', 'O',  '/'  }
};

static const char *getANSIcolor(const int ETcolor)
{
	int i, j;

	if (ETcolor == COLOR_NULL)
	{
		// '^*' is return previous color in some cases
		// The in-game console treats it as COLOR_WHITE
		// Note: see cl_console.c define CONSOLE_COLOR
		return "1;37";
	}

	for (i = 0; i < (int)ARRAY_LEN(etAnsiHash); i++)
	{
		for(j = 3; j < 6; j++)
		{
			if (etAnsiHash[i][j] != 0 && ETcolor == etAnsiHash[i][j])
			{
				if (etAnsiHash[i][2] == 0)
				{
					return va("%d;%d", etAnsiHash[i][0], etAnsiHash[i][1]);
				}
				else
				{
					return va("%d;%d;%d", etAnsiHash[i][0], etAnsiHash[i][1], etAnsiHash[i][2]);
				}
			}
		}
	}
	return NULL;
}

static qboolean printableChar(const char c)
{
	if ((c >= ' ' && c <= '~') || c == '\n' || c == '\r' || c == '\t')
		return qtrue;
	else
		return qfalse;
}

void Sys_ANSIColorify(const char *msg, char *buffer, int bufferSize)
{
	int msgLength;
	int i;
	char tempBuffer[15];
	const char *ANSIcolor;

	if (!msg || !buffer)
		return;

	msgLength = strlen(msg);
	i = 0;
	buffer[0] = '\0';

	while (i < msgLength)
	{
		if (msg[i] == '\n')
		{
			Com_sprintf(tempBuffer, sizeof(tempBuffer), "%c[0m\n", 0x1B);
			strncat(buffer, tempBuffer, bufferSize - 1);
			i += 1;
		}
		else if (msg[i] == Q_COLOR_ESCAPE && (ANSIcolor = getANSIcolor(msg[i + 1])) != NULL)
		{
			Com_sprintf(tempBuffer, sizeof(tempBuffer), "%c[%sm", 0x1B, ANSIcolor);
			strncat(buffer, tempBuffer, bufferSize - 1);
			i += 2;
		}
		else
		{
			if (printableChar(msg[i]))
			{
				Com_sprintf(tempBuffer, sizeof(tempBuffer), "%c", msg[i]);
				strncat(buffer, tempBuffer, bufferSize - 1);
			}
			i += 1;
		}
	}
}

void Sys_Print(const char *msg)
{
	char printmsg[MAXPRINTMSG];
	size_t len;

	if (ttycon_on)
	{
		tty_Hide();
	}

	if (ttycon_on && ttycon_color_on)
	{
		Sys_ANSIColorify(msg, printmsg, sizeof(printmsg));
		len = strlen(printmsg);
	}
	else
	{
		char *out = printmsg;
		while (*msg != '\0' && out < printmsg + sizeof(printmsg))
		{
			if (printableChar(*msg))
				*out++ = *msg;
			msg++;
		}
		len = out - printmsg;
	}

	(void)!write(STDERR_FILENO, printmsg, len);

	if (ttycon_on)
	{
		tty_Show();
	}
}

void Q_PRINTF_FUNC(1, 2) QDECL Sys_SetStatus(const char *format, ...)
{
	return;
}

void Sys_ConfigureFPU(void) // bk001213 - divide by zero
{
#ifdef __linux__
#if id386
#ifdef __GLIBC__
#ifdef _DEBUG
	// bk0101022 - enable FPE's in debug mode
	static int fpu_word = _FPU_DEFAULT & ~(_FPU_MASK_ZM | _FPU_MASK_IM);
	int current = 0;
	_FPU_GETCW(current);
	if (current != fpu_word)
	{
#if 0
		Com_Printf("FPU Control 0x%x (was 0x%x)\n", fpu_word, current );
		_FPU_SETCW( fpu_word );
		_FPU_GETCW( current );
		assert(fpu_word==current);
#endif
	}
#else  // NDEBUG
	static int fpu_word = _FPU_DEFAULT;
	_FPU_SETCW(fpu_word);
#endif // NDEBUG
#endif // __GLIBC__
#endif // id386
#endif // __linux
}

void Sys_PrintBinVersion(const char *name)
{
	const char *date = __DATE__;
	const char *time = __TIME__;
	const char *sep = "==============================================================";

	fprintf(stdout, "\n\n%s\n", sep);
#ifdef DEDICATED
	fprintf(stdout, "Linux %s Dedicated Server [%s %s]\n", Q3_VERSION, date, time);
#else
	fprintf(stdout, "Linux %s Full Executable  [%s %s]\n", Q3_VERSION, date, time);
#endif
	fprintf(stdout, " local install: %s\n", name);
	fprintf(stdout, "%s\n\n", sep);
}

/*
==================
chmod OR on a file
==================
*/
void Sys_Chmod(const char *file, int mode)
{
	struct stat s_buf;
	int perm;
	if (stat(file, &s_buf) != 0)
	{
		Com_Printf("stat('%s')  failed: errno %d\n", file, errno);
		return;
	}
	perm = s_buf.st_mode | mode;
	if (chmod(file, perm) != 0)
	{
		Com_Printf("chmod('%s', %d) failed: errno %d\n", file, perm, errno);
	}
	Com_DPrintf("chmod +%d '%s'\n", mode, file);
}

/*
==================
Sys_DoStartProcess
actually forks and starts a process

UGLY HACK:
  Sys_StartProcess works with a command line only
  if this command line is actually a binary with command line parameters,
  the only way to do this on unix is to use a system() call
  but system doesn't replace the current process, which leads to a situation like:
  wolf.x86--spawned_process.x86
  in the case of auto-update for instance, this will cause write access denied on wolf.x86:
  wolf-beta/2002-March/000079.html
  we hack the implementation here, if there are no space in the command line, assume it's a straight process and use execl
  otherwise, use system ..
  The clean solution would be Sys_StartProcess and Sys_StartProcess_Args..
==================
*/
void Sys_DoStartProcess(const char *cmdline)
{
	switch (fork())
	{
	case -1:
		// main thread
		break;
	case 0:
		if (strchr(cmdline, ' '))
		{
			int res = system(cmdline);
			if (!res)
			{
				printf("system call for %s failed with result: %d\n", cmdline, res);
			}
		}
		else
		{
			execl(cmdline, cmdline, NULL);
			printf("execl failed: %s\n", strerror(errno));
		}
		_exit(0);
		break;
	}
}

/*
==================
Sys_StartProcess
if !doexit, start the process asap
otherwise, push it for execution at exit
(i.e. let complete shutdown of the game and freeing of resources happen)
NOTE: might even want to add a small delay?
==================
*/
void Sys_StartProcess(const char *cmdline, qboolean doexit)
{
	if (doexit)
	{
		Com_DPrintf("Sys_StartProcess %s (delaying to final exit)\n", cmdline);
		Q_strncpyz(exit_cmdline, cmdline, MAX_CMD);
		Cbuf_ExecuteText(EXEC_APPEND, "quit\n");
		return;
	}

	Com_DPrintf("Sys_StartProcess %s\n", cmdline);
	Sys_DoStartProcess(cmdline);
}

/*
=================
Sys_OpenURL
=================
*/
void Sys_OpenURL(const char *url, qboolean doexit)
{
#ifndef DEDICATED
	char cmdline[MAX_CMD];

	static qboolean doexit_spamguard = qfalse;

	if (doexit_spamguard)
	{
		Com_DPrintf("Sys_OpenURL: already in a doexit sequence, ignoring %s\n", url);
		return;
	}

	Com_Printf("Open URL: %s\n", url);
	// opening an URL on *nix can mean a lot of things ..
	// just spawn a script instead of deciding for the user :-)

#ifdef __APPLE__
	Com_sprintf(cmdline, sizeof(cmdline), "open '%s' &", url);
#else
	Com_sprintf(cmdline, sizeof(cmdline), "xdg-open '%s' &", url);
#endif

	if (doexit)
	{
		doexit_spamguard = qtrue;
	}

	Sys_StartProcess(cmdline, doexit);
#endif
}

/*
=================
Sys_BinName

This resolves any symlinks to the binary. It's disabled for debug
builds because there are situations where you are likely to want
to symlink to binaries and /not/ have the links resolved.
=================
*/
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
const char *Sys_BinName(const char *arg0)
{
	static char dst[PATH_MAX];

#ifdef NDEBUG

#if defined(__linux__)
	int n = readlink("/proc/self/exe", dst, PATH_MAX - 1);

	if (n >= 0 && n < PATH_MAX)
		dst[n] = '\0';
	else
		Q_strncpyz(dst, arg0, PATH_MAX);
#elif defined(__APPLE__)
	uint32_t bufsize = sizeof(dst);

	if (_NSGetExecutablePath(dst, &bufsize) == -1)
	{
		Q_strncpyz(dst, arg0, PATH_MAX);
	}
#else

#warning Sys_BinName not implemented
	Q_strncpyz(dst, arg0, PATH_MAX);
#endif

#else // DEBUG
	Q_strncpyz(dst, arg0, PATH_MAX);
#endif
	return dst;
}

int Sys_ParseArgs(int argc, const char *argv[])
{
	if (argc == 2)
	{
		if ((!strcmp(argv[1], "--version")) || (!strcmp(argv[1], "-v")))
		{
			Sys_PrintBinVersion(Sys_BinName(argv[0]));
			return 1;
		}
	}

	return 0;
}

#if (defined(USE_SDL2) || defined(USE_SDL3)) && !defined(DEDICATED)
void Sys_GetSDLVersion(uint32_t *major, uint32_t *minor, uint32_t *patch);
#endif
int main(int argc, const char *argv[])
{
	char con_title[MAX_CVAR_VALUE_STRING];
	int xpos, ypos;
	//qboolean useXYpos;
	char *cmdline;
	int len, i;
	tty_err err;
#if (defined(USE_SDL2) || defined(USE_SDL3)) && !defined(DEDICATED)
	uint32_t major, minor, patch;
	Sys_GetSDLVersion(&major, &minor, &patch);
#endif

#ifdef __APPLE__
	// This is passed if we are launched by double-clicking
	if (argc >= 2 && Q_strncmp(argv[1], "-psn", 4) == 0)
		argc = 1;
#endif

	if (Sys_ParseArgs(argc, argv)) // added this for support
		return 0;

	// merge the command line, this is kinda silly
	for (len = 1, i = 1; i < argc; i++)
		len += strlen(argv[i]) + 1;

	cmdline = malloc(len);
	*cmdline = '\0';
	for (i = 1; i < argc; i++)
	{
		if (i > 1)
			strcat(cmdline, " ");
		strcat(cmdline, argv[i]);
	}

	/*useXYpos = */ Com_EarlyParseCmdLine(cmdline, con_title, sizeof(con_title), &xpos, &ypos);

	// bk000306 - clear queues
	//	memset( &eventQue[0], 0, sizeof( eventQue ) );
	//	memset( &sys_packetReceived[0], 0, sizeof( sys_packetReceived ) );

	Com_Init( cmdline );

#if (defined(USE_SDL2) || defined(USE_SDL3)) && !defined(DEDICATED)
	Com_Printf("Using SDL Version %u.%u.%u\n", major, minor, patch);
#endif

	// Sys_ConsoleInputInit() might be called in signal handler
	// so modify/init any cvars here
	ttycon = Cvar_Get("ttycon", "1", 0);
	Cvar_SetDescription(ttycon, "Terminal tab-completion and history support");
	ttycon_ansicolor = Cvar_Get("ttycon_ansicolor", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(ttycon_ansicolor, "Terminal display console color codes");

	err = Sys_ConsoleInputInit();
	if (err == TTY_ENABLED)
	{
		Com_Printf("Started tty console (use +set ttycon 0 to disable)\n");
	}
	else
	{
		if (err == TTY_ERROR)
		{
			Com_Printf("stdin is not a tty, tty console mode failed\n");
			Cvar_Set("ttycon", "0");
		}
	}

#ifdef DEDICATED
	// init here for dedicated, as we don't have GLimp_Init
	InitSig();
#endif

	while (1)
	{
#ifdef __linux__
		Sys_ConfigureFPU();
#endif

#ifdef DEDICATED
		// run the game
		Com_Frame( qfalse );
#else
		// check for other input devices
		IN_Frame();
		// run the game
		Com_Frame( CL_NoDelay() );
#endif
	}
	// never gets here
	return 0;
}

int Sys_GetPID(void)
{
	return (int)getpid();
}
