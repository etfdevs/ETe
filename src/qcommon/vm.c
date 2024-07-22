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
// vm.c -- virtual machine

/*


intermix code and data
symbol table

a dll has one imported function: VM_SystemCall
and one exported function: Perform


*/

#include "vm_local.h"


#ifdef _DEBUG
int		vm_debugLevel;
#endif

// used by Com_Error to get rid of running vm's before longjmp
static int forced_unload;

static struct vm_s vmTable[ VM_COUNT ];

static const char *vmName[ VM_COUNT ] = {
	"qagame",
#ifndef DEDICATED
	"cgame",
	"ui"
#endif
};

static void VM_VmInfo_f( void );

#ifdef _DEBUG
void VM_Debug( int level ) {
	vm_debugLevel = level;
}
#endif

/*
==============
VM_Init
==============
*/
void VM_Init( void ) {
	Cmd_AddCommand( "vminfo", VM_VmInfo_f );
	Cmd_SetModule( "vminfo", MODULE_COMMON );

	Com_Memset( vmTable, 0, sizeof( vmTable ) );
}


void VM_Shutdown( void ) {
	Cmd_RemoveCommand( "vminfo" );
}


/*
============
VM_DllSyscall

Dlls will call this directly

 rcg010206 The horror; the horror.

  The syscall mechanism relies on stack manipulation to get it's args.
   This is likely due to C's inability to pass "..." parameters to
   a function in one clean chunk. On PowerPC Linux, these parameters
   are not necessarily passed on the stack, so while (&arg[0] == arg)
   is true, (&arg[1] == 2nd function parameter) is not necessarily
   accurate, as arg's value might have been stored to the stack or
   other piece of scratch memory to give it a valid address, but the
   next parameter might still be sitting in a register.

  Quake's syscall system also assumes that the stack grows downward,
   and that any needed types can be squeezed, safely, into a signed int.

  This hack below copies all needed values for an argument to a
   array in memory, so that Quake can get the correct values. This can
   also be used on systems where the stack grows upwards, as the
   presumably standard and safe stdargs.h macros are used.

  As for having enough space in a signed int for your datatypes, well,
   it might be better to wait for DOOM 3 before you start porting.  :)

  The original code, while probably still inherently dangerous, seems
   to work well enough for the platforms it already works on. Rather
   than add the performance hit for those platforms, the original code
   is still in use there.

  For speed, we just grab 15 arguments, and don't worry about exactly
   how many the syscall actually needs; the extra is thrown away.

============
*/
#if 0 // - disabled because now is different for each module
intptr_t QDECL VM_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
  // rcg010206 - see commentary above
  intptr_t	args[16];
  va_list	ap;
  int i;
  
  args[0] = arg;
  
  va_start( ap, arg );
  for (i = 1; i < ARRAY_LEN( args ); i++ )
    args[ i ] = va_arg( ap, intptr_t );
  va_end( ap );
  
  return currentVM->systemCall( args );
#else // original id code
	return currentVM->systemCall( &arg );
#endif
}
#endif

/*
=================
VM_Restart

Reload the data, but leave everything else in place
This allows a server to do a map_restart without changing memory allocation
=================
*/
vm_t *VM_Restart( vm_t *vm ) {
	// DLL's can't be restarted in place
	if ( vm->dllHandle ) {
		syscall_t		systemCall;
		dllSyscall_t	dllSyscall;
		vmIndex_t		index;
				
		index = vm->index;
		systemCall = vm->systemCall;
		dllSyscall = vm->dllSyscall;

		VM_Free( vm );

		vm = VM_Create( index, systemCall, dllSyscall, VMI_NATIVE );
		return vm;
	}

	return NULL;
}


/*
================
VM_Create

If image ends in .qvm it will be interpreted, otherwise
it will attempt to load as a system dll
================
*/
vm_t *VM_Create( vmIndex_t index, syscall_t systemCalls, dllSyscall_t dllSyscalls, vmInterpret_t interpret ) {
	const char	*name;
//	vmHeader_t  *header;
	vm_t        *vm;

	if ( !systemCalls ) {
		Com_Error( ERR_FATAL, "VM_Create: bad parms" );
	}

	if ( (unsigned)index >= VM_COUNT ) {
		Com_Error( ERR_DROP, "VM_Create: bad vm index %i", index );	
	}

	//remaining = Hunk_MemoryRemaining();

	vm = &vmTable[ index ];

	// see if we already have the VM
	if ( vm->name ) {
		if ( vm->index != index ) {
			Com_Error( ERR_DROP, "VM_Create: bad allocated vm index %i", vm->index );
			return NULL;
		}
		return vm;
	}

	name = vmName[ index ];

	vm->name = name;
	vm->index = index;
	vm->systemCall = systemCalls;
	vm->dllSyscall = dllSyscalls;
	vm->privateFlag = CVAR_PRIVATE;

	if ( interpret == VMI_NATIVE ) {
		// try to load as a system dll
		Com_Printf( "Loading dll file %s.\n", name );
		vm->dllHandle = Sys_LoadGameDll( name, &vm->entryPoint, dllSyscalls );
		// TTimo - never try qvm
		if ( vm->dllHandle ) {
			vm->privateFlag = 0; // allow reading private cvars
			return vm;
		}
		return NULL;
	}
	
	return NULL;
}


/*
==============
VM_Free
==============
*/
void VM_Free( vm_t *vm ) {

	if( !vm ) {
		return;
	}

	if ( vm->callLevel ) {
		if ( !forced_unload ) {
			Com_Error( ERR_FATAL, "VM_Free(%s) on running vm", vm->name );
			return;
		} else {
			Com_Printf( "forcefully unloading %s vm\n", vm->name );
		}
	}

	if ( vm->dllHandle )
		Sys_UnloadLibrary( vm->dllHandle );

	Com_Memset( vm, 0, sizeof( *vm ) );
}


void VM_Clear( void ) {
	int i;
	for ( i = 0; i < VM_COUNT; i++ ) {
		VM_Free( &vmTable[ i ] );
	}
}


void VM_Forced_Unload_Start(void) {
	forced_unload = 1;
}


void VM_Forced_Unload_Done(void) {
	forced_unload = 0;
}


/*
==============
VM_Call


Upon a system call, the stack will look like:

sp+32	parm1
sp+28	parm0
sp+24	return value
sp+20	return address
sp+16	local1
sp+14	local0
sp+12	arg1
sp+8	arg0
sp+4	return stack
sp		return address

An interpreted function will immediately execute
an OP_ENTER instruction, which will subtract space for
locals from sp
==============
*/

intptr_t QDECL VM_CallFunc( vm_t *vm, int nargs, int callnum, ... )
{
	intptr_t r = 0;
	int i;

	if ( !vm ) {
		Com_Error( ERR_FATAL, "VM_Call with NULL vm" );
	}

#ifdef _DEBUG
	if ( vm_debugLevel ) {
		Com_Printf( "VM_Call( %d )\n", callnum );
	}

	if ( nargs >= MAX_VMMAIN_CALL_ARGS ) {
		Com_Error( ERR_DROP, "VM_Call: nargs >= MAX_VMMAIN_CALL_ARGS" );
	}
#endif

	++vm->callLevel;
	// if we have a dll loaded, call it directly
	if ( vm->entryPoint ) 
	{
		//rcg010207 -  see dissertation at top of VM_DllSyscall() in this file.
		intptr_t args[MAX_VMMAIN_CALL_ARGS-1] = { 0 };
		va_list ap;
		va_start( ap, callnum );
		for ( i = 0; i < nargs; i++ ) {
			args[i] = va_arg( ap, intptr_t );
		}
		va_end(ap);

		// add more agruments if you're changed MAX_VMMAIN_CALL_ARGS:
		r = vm->entryPoint( callnum, args[0], args[1], args[2], args[3], args[4], args[5], args[6] );
	}
	--vm->callLevel;

	return r;
}


//=================================================================


/*
==============
VM_NameToVM
==============
*/
/*static vm_t *VM_NameToVM( const char *name ) 
{
	vmIndex_t index;

	if ( !Q_stricmp( name, "game" ) )
		index = VM_GAME;
#ifndef DEDICATED
	else if ( !Q_stricmp( name, "cgame" ) )
		index = VM_CGAME;
	else if ( !Q_stricmp( name, "ui" ) )
		index = VM_UI;
#endif
	else {
		Com_Printf( " unknown VM name '%s'\n", name );
		return NULL;
	}

	if ( !vmTable[ index ].name ) {
		Com_Printf( " %s is not running.\n", name );
		return NULL;
	}

	return &vmTable[ index ];
}*/


/*
==============
VM_VmInfo_f
==============
*/
static void VM_VmInfo_f( void ) {
	const vm_t	*vm;
	int		i;

	Com_Printf( "Registered virtual machines:\n" );
	for ( i = 0 ; i < VM_COUNT ; i++ ) {
		vm = &vmTable[i];
		if ( !vm->name ) {
			continue;
		}
		Com_Printf( "%s : ", vm->name );
		if ( vm->dllHandle ) {
			Com_Printf( "native\n" );
			continue;
		}
		/*if ( vm->compiled ) {
			Com_Printf( "compiled on load\n" );
		} else {
			Com_Printf( "interpreted\n" );
		}
		Com_Printf( "    code length : %7i\n", vm->codeLength );
		Com_Printf( "    table length: %7i\n", vm->instructionCount*4 );
		Com_Printf( "    data length : %7i\n", vm->dataMask + 1 );*/
	}
}
