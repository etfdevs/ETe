#ifndef __SDL_LOCAL_HEADERS__
#define __SDL_LOCAL_HEADERS__

#define SDL_FUNCTION_POINTER_IS_VOID_POINTER 1

#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL.h"
#else
#	include <SDL3/SDL.h>
#endif

#if defined(USE_VULKAN_API)
#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL_vulkan.h"
#else
#	include <SDL3/SDL_vulkan.h>
#endif
#endif

#endif