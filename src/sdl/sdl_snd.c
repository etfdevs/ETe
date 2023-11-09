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
#include "../client/snd_local.h"
#include "../client/client.h"

qboolean snd_inited = qfalse;

extern cvar_t *s_khz;
extern cvar_t *s_device;
extern cvar_t *s_bits;
extern cvar_t *s_numchannels;
static cvar_t *s_sdlDevSamps;
static cvar_t *s_sdlMixSamps;
static cvar_t *s_sdlLevelSamps;

/* The audio callback. All the magic happens here. */
static int dmapos = 0;
static int dmasize = 0;

static SDL_AudioDeviceID sdlPlaybackDevice = 0;

#if defined USE_VOIP && SDL_VERSION_ATLEAST( 2, 0, 5 )
#define USE_SDL_AUDIO_CAPTURE

static SDL_AudioDeviceID sdlCaptureDevice = 0;
static cvar_t *s_sdlCapture;
static float sdlMasterGain = 1.0f;
#endif


/*
===============
SNDDMA_AudioCallback
===============
*/
static void SNDDMA_AudioCallback(void *userdata, Uint8 *stream, int len)
{
	int pos = (dmapos * (dma.samplebits/8));
	if (pos >= dmasize)
		dmapos = pos = 0;

	if (!snd_inited)  /* shouldn't happen, but just in case... */
	{
		memset(stream, '\0', len);
		return;
	}
	else
	{
		int tobufend = dmasize - pos;  /* bytes to buffer's end. */
		int len1 = len;
		int len2 = 0;

		if (len1 > tobufend)
		{
			len1 = tobufend;
			len2 = len - len1;
		}
		memcpy(stream, dma.buffer + pos, len1);
		if (len2 <= 0)
			dmapos += (len1 / (dma.samplebits/8));
		else  /* wraparound? */
		{
			memcpy(stream+len1, dma.buffer, len2);
			dmapos = (len2 / (dma.samplebits/8));
		}
	}

	if (dmapos >= dmasize)
		dmapos = 0;

#ifdef USE_SDL_AUDIO_CAPTURE
	if (sdlMasterGain != 1.0f)
	{
		int i;
		if (dma.isfloat && (dma.samplebits == 32))
		{
			float *ptr = (float *) stream;
			len /= sizeof (*ptr);
			for (i = 0; i < len; i++, ptr++)
			{
				*ptr *= sdlMasterGain;
			}
		}
		else if (dma.samplebits == 16)
		{
			Sint16 *ptr = (Sint16 *) stream;
			len /= sizeof (*ptr);
			for (i = 0; i < len; i++, ptr++)
			{
				*ptr = (Sint16) (((float) *ptr) * sdlMasterGain);
			}
		}
		else if (dma.samplebits == 8)
		{
			Uint8 *ptr = (Uint8 *) stream;
			len /= sizeof (*ptr);
			for (i = 0; i < len; i++, ptr++)
			{
				*ptr = (Uint8) (((float) *ptr) * sdlMasterGain);
			}
		}
	}
#endif
}

static const struct
{
	uint16_t	enumFormat;
	const char	*stringFormat;
} formatToStringTable[ ] =
{
	{ AUDIO_U8,     "AUDIO_U8" },
	{ AUDIO_S8,     "AUDIO_S8" },
	{ AUDIO_U16LSB, "AUDIO_U16LSB" },
	{ AUDIO_S16LSB, "AUDIO_S16LSB" },
	{ AUDIO_U16MSB, "AUDIO_U16MSB" },
	{ AUDIO_S16MSB, "AUDIO_S16MSB" },
	{ AUDIO_F32LSB, "AUDIO_F32LSB" },
	{ AUDIO_F32MSB, "AUDIO_F32MSB" }
};

static const int formatToStringTableSize = (int)ARRAY_LEN( formatToStringTable );

/*
===============
SNDDMA_PrintAudiospec
===============
*/
static void SNDDMA_PrintAudiospec(const char *str, const SDL_AudioSpec *spec)
{
	const char *fmt = NULL;
	int i;

	Com_Printf( "%s:\n", str );

	for ( i = 0; i < formatToStringTableSize; i++ ) {
		if( spec->format == formatToStringTable[ i ].enumFormat ) {
			fmt = formatToStringTable[ i ].stringFormat;
		}
	}

	if ( fmt ) {
		Com_Printf( "  Format:   %s\n", fmt );
	} else {
		Com_Printf( "  Format:   " S_COLOR_RED "UNKNOWN\n");
	}

	Com_Printf( "  Freq:     %d\n", (int) spec->freq );
	Com_Printf( "  Samples:  %d\n", (int) spec->samples );
	Com_Printf( "  Channels: %d\n", (int) spec->channels );
}


static void SND_DeviceList(void)
{
	int i;
	int count = SDL_GetNumAudioDevices(SDL_FALSE);

	Com_Printf("Printing audio device list. Number of devices: %i\n\n", count);

	for (i = 0; i < count; i++)
	{
		Com_Printf("  Audio device %d: %s\n", i, SDL_GetAudioDeviceName(i, SDL_FALSE));
	}
}


static int SNDDMA_KHzToHz( int khz )
{
	switch ( khz )
	{
		case 48: return 48000;
		case 44: return 44100;
		default:
		case 22: return 22050;
		case 11: return 11025;
	}
}


static int SND_SamplesForFreq( int freq, int level )
{
	int samples;

	switch ( freq )
	{
		case 11025: samples = 256; break;
		case 22050: samples = 512; break;
		case 44100: samples = 1024; break;
		default:
		case 48000: samples = 2048; break;
	}

	if ( level == 1 )
	{
		samples /= 2;
	}
	else if ( level == 2 )
	{
		samples /= 4;
	}

	return samples;
}


/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init( void )
{
	SDL_AudioSpec desired;
	SDL_AudioSpec obtained;
	const char	*device_name;
	int tmp;

	if ( snd_inited )
		return qtrue;

	s_sdlDevSamps = Cvar_Get( "s_sdlDevSamps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_sdlDevSamps, "Number of audio samples to provide to the SDL audio output device. When set to 0 it picks a value based on s_khz" );
	s_sdlMixSamps = Cvar_Get( "s_sdlMixSamps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_sdlMixSamps, "Number of audio samples for Enemy Territory's audio mixer when using SDL audio output" );
	s_sdlLevelSamps = Cvar_Get( "s_sdlLevelSamps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_sdlLevelSamps, "0", "2", CV_INTEGER );

#if defined(USE_SDL)
	if ( s_sdlDriver->string )
	{
		SDL_setenv("SDL_AUDIODRIVER", s_sdlDriver->string, 0);
	}
#endif

	Com_Printf( "SDL_Init( SDL_INIT_AUDIO )... " );

	if ( SDL_Init( SDL_INIT_AUDIO ) != 0 )
	{
		Com_Printf( "FAILED (%s)\n", SDL_GetError() );
		return qfalse;
	}

	Cmd_AddCommand( "s_devlist", SND_DeviceList );

	Com_Printf( "OK\n" );

	Com_Printf( "SDL audio driver is \"%s\".\n", SDL_GetCurrentAudioDriver() );

	memset( &desired, '\0', sizeof (desired) );
	memset( &obtained, '\0', sizeof (obtained) );

	desired.freq = SNDDMA_KHzToHz( s_khz->integer );
	if ( desired.freq == 0 )
		desired.freq = 22050;

	tmp = s_bits->integer;
	if ( tmp < 16 )
		tmp = 8;

	desired.format = ((tmp == 16) ? AUDIO_S16SYS : AUDIO_U8);

	// I dunno if this is the best idea, but I'll give it a try...
	//  should probably check a cvar for this...
	if ( s_sdlDevSamps->integer > 0 )
		desired.samples = s_sdlDevSamps->value;
	else
	{
		desired.samples = SND_SamplesForFreq(desired.freq, s_sdlLevelSamps->integer);
	}

	desired.channels = s_numchannels->integer;
	desired.callback = SNDDMA_AudioCallback;

	if ( !Q_stricmp(s_device->string, "default") || !*s_device->string ) {
		device_name = NULL;
		Com_Printf( "Acquiring default audio device\n" );
	}
	else {
		if ( Q_isanumber( s_device->string ) && Q_isintegral( s_device->value ) ) {
			if ( s_device->integer >= 0 && s_device->integer < SDL_GetNumAudioDevices( SDL_FALSE ) ) {
				device_name = SDL_GetAudioDeviceName( s_device->integer, SDL_FALSE );
				Com_Printf( "Acquiring audio device: %s\n", device_name );
			}
			else {
				Com_Printf( "SDL audio device out of range: %i\n", s_device->integer );
				device_name = NULL;
				Com_Printf( "Acquiring default audio device\n" );
				Cvar_ForceReset( "s_device" );
			}
		}
		else {
			Com_Printf( "SDL audio device invalid: %s\n", s_device->string );
			device_name = NULL;
			Com_Printf( "Acquiring default audio device\n" );
			Cvar_ForceReset( "s_device" );
		}
	}

	sdlPlaybackDevice = SDL_OpenAudioDevice( device_name, SDL_FALSE, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE );
	if ( sdlPlaybackDevice == 0 )
	{
		Com_Printf( "SDL_OpenAudioDevice() failed: %s\n", SDL_GetError() );
		SDL_QuitSubSystem( SDL_INIT_AUDIO );
		return qfalse;
	}

	SNDDMA_PrintAudiospec( "SDL_AudioSpec", &obtained );

	// dma.samples needs to be big, or id's mixer will just refuse to
	//  work at all; we need to keep it significantly bigger than the
	//  amount of SDL callback samples, and just copy a little each time
	//  the callback runs.
	// 32768 is what the OSS driver filled in here on my system. I don't
	//  know if it's a good value overall, but at least we know it's
	//  reasonable...this is why I let the user override.
	tmp = s_sdlMixSamps->integer;
	if ( !tmp )
		tmp = (obtained.samples * obtained.channels) * 10;

	// samples must be divisible by number of channels
	tmp -= tmp % obtained.channels;
	// round up to next power of 2
	tmp = log2pad( tmp, 1 );

	dmapos = 0;
	dma.samplebits = SDL_AUDIO_BITSIZE( obtained.format );
	dma.isfloat = SDL_AUDIO_ISFLOAT( obtained.format );
	dma.channels = obtained.channels;
	dma.samples = tmp;
	dma.fullsamples = dma.samples / dma.channels;
	dma.submission_chunk = 1;
	dma.speed = obtained.freq;
	dmasize = (dma.samples * (dma.samplebits/8));
	dma.buffer = calloc(1, dmasize);

#ifdef USE_SDL_AUDIO_CAPTURE
	// !!! FIXME: some of these SDL_OpenAudioDevice() values should be cvars.
	s_sdlCapture = Cvar_Get( "s_sdlCapture", "1", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( s_sdlCapture, "Set to 1 to enable SDL audio capture" );
	// !!! FIXME: pulseaudio capture records audio the entire time the program is running. https://bugzilla.libsdl.org/show_bug.cgi?id=4087
	if (Q_stricmp(SDL_GetCurrentAudioDriver(), "pulseaudio") == 0)
	{
		Com_Printf("SDL audio capture support disabled for pulseaudio (https://bugzilla.libsdl.org/show_bug.cgi?id=4087)\n");
	}
	else if (!s_sdlCapture->integer)
	{
		Com_Printf("SDL audio capture support disabled by user ('+set s_sdlCapture 1' to enable)\n");
	}
#if USE_MUMBLE
	else if (cl_useMumble->integer)
	{
		Com_Printf("SDL audio capture support disabled for Mumble support\n");
	}
#endif
	else
	{
		/* !!! FIXME: list available devices and let cvar specify one, like OpenAL does */
		SDL_AudioSpec spec;
		SDL_zero(spec);
		spec.freq = 48000;
		spec.format = AUDIO_S16SYS;
		spec.channels = 1;
		spec.samples = VOIP_MAX_PACKET_SAMPLES * 4;
		sdlCaptureDevice = SDL_OpenAudioDevice(NULL, SDL_TRUE, &spec, NULL, 0);
		Com_Printf( "SDL capture device %s.\n",
				    (sdlCaptureDevice == 0) ? "failed to open" : "opened");
	}

	sdlMasterGain = 1.0f;
#endif

	Com_Printf("Starting SDL audio callback...\n");
	SDL_PauseAudioDevice(sdlPlaybackDevice, 0);  // start callback.
	// don't unpause the capture device; we'll do that in StartCapture.

	Com_Printf("SDL audio initialized.\n");
	snd_inited = qtrue;
	return qtrue;
}


/*
===============
SNDDMA_GetDMAPos
===============
*/
int SNDDMA_GetDMAPos( void )
{
	return dmapos;
}


/*
===============
SNDDMA_Shutdown
===============
*/
void SNDDMA_Shutdown( void )
{
	if (sdlPlaybackDevice != 0)
	{
		Com_Printf("Closing SDL audio playback device...\n");
		SDL_CloseAudioDevice(sdlPlaybackDevice);
		Com_Printf("SDL audio playback device closed.\n");
		sdlPlaybackDevice = 0;
	}

#ifdef USE_SDL_AUDIO_CAPTURE
	if (sdlCaptureDevice)
	{
		Com_Printf("Closing SDL audio capture device...\n");
		SDL_CloseAudioDevice(sdlCaptureDevice);
		Com_Printf("SDL audio capture device closed.\n");
		sdlCaptureDevice = 0;
	}
#endif

	Cmd_RemoveCommand("s_devlist");

	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	free(dma.buffer);
	dma.buffer = NULL;
	dmapos = dmasize = 0;
	snd_inited = qfalse;
	Com_Printf("SDL audio shut down.\n");
}


/*
===============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit( void )
{
	SDL_UnlockAudioDevice( sdlPlaybackDevice );
}


/*
===============
SNDDMA_BeginPainting
===============
*/
void SNDDMA_BeginPainting( void )
{
	SDL_LockAudioDevice( sdlPlaybackDevice );
}


#ifdef USE_VOIP
void SNDDMA_StartCapture(void)
{
#ifdef USE_SDL_AUDIO_CAPTURE
	if (sdlCaptureDevice)
	{
		SDL_ClearQueuedAudio(sdlCaptureDevice);
		SDL_PauseAudioDevice(sdlCaptureDevice, 0);
	}
#endif
}


int SNDDMA_AvailableCaptureSamples(void)
{
#ifdef USE_SDL_AUDIO_CAPTURE
	// divided by 2 to convert from bytes to (mono16) samples.
	return sdlCaptureDevice ? (SDL_GetQueuedAudioSize(sdlCaptureDevice) / 2) : 0;
#else
	return 0;
#endif
}


void SNDDMA_Capture(int samples, byte *data)
{
#ifdef USE_SDL_AUDIO_CAPTURE
	// multiplied by 2 to convert from (mono16) samples to bytes.
	if (sdlCaptureDevice)
	{
		SDL_DequeueAudio(sdlCaptureDevice, data, samples * 2);
	}
	else
#endif
	{
		SDL_memset(data, '\0', samples * 2);
	}
}

void SNDDMA_StopCapture(void)
{
#ifdef USE_SDL_AUDIO_CAPTURE
	if (sdlCaptureDevice)
	{
		SDL_PauseAudioDevice(sdlCaptureDevice, 1);
	}
#endif
}

void SNDDMA_MasterGain( float val )
{
#ifdef USE_SDL_AUDIO_CAPTURE
	sdlMasterGain = val;
#endif
}
#endif
