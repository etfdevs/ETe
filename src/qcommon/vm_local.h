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
#ifndef __VM_LOCAL_H__
#define __VM_LOCAL_H__

#include "q_shared.h"
#include "qcommon.h"

// we don't need more than 6 arguments (counting callnum) for vmMain, at least in ET
// 6 arguments for vET 8 for etlegacy cgame compatibility
#define MAX_VMMAIN_CALL_ARGS 8

struct vm_s {
	syscall_t	systemCall;

	//------------------------------------

	const char	*name;
	vmIndex_t	index;

	// for dynamic linked modules
	void			*dllHandle;
	vmMain_t		entryPoint;
	dllSyscall_t	dllSyscall;

	int callLevel;                  // for debug indenting

	int			privateFlag;
};

#endif // VM_LOCAL_H
