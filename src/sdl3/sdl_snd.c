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

#include "sdl_local.h"

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

static SDL_AudioStream *sdlPlaybackStream = NULL;

#if defined USE_VOIP
#define USE_SDL_AUDIO_CAPTURE

static SDL_AudioStream *sdlCaptureStream = NULL;
static cvar_t *s_sdlCapture;
#endif


/*
===============
SNDDMA_AudioCallback
===============
*/
static void SNDDMA_AudioCallback(void *userdata, SDL_AudioStream *stream, int /*len*/additional_amount, int total_amount)
{
	int pos = (dmapos * (dma.samplebits/8));
	if (pos >= dmasize)
		dmapos = pos = 0;

	if (!snd_inited)  /* shouldn't happen, but just in case... */
	{
		//memset(stream, '\0', len);
		return;
	}
	else
	{
		int tobufend = dmasize - pos;  /* bytes to buffer's end. */
		int len1 = additional_amount;
		int len2 = 0;

		if (len1 > tobufend)
		{
			len1 = tobufend;
			len2 = additional_amount - len1;
		}
		SDL_PutAudioStreamData(stream, dma.buffer + pos, len1);
		if (len2 <= 0)
			dmapos += (len1 / (dma.samplebits/8));
		else  /* wraparound? */
		{
			SDL_PutAudioStreamData(stream, dma.buffer, len2);
			dmapos = (len2 / (dma.samplebits/8));
		}
	}

	if (dmapos >= dmasize)
		dmapos = 0;
}

static const struct
{
	Uint16	enumFormat;
	const char	*stringFormat;
} formatToStringTable[ ] =
{
	{ SDL_AUDIO_U8,     "AUDIO_U8" },
	{ SDL_AUDIO_S8,     "AUDIO_S8" },
	{ SDL_AUDIO_S16LE, "AUDIO_S16LE" },
	{ SDL_AUDIO_S16BE, "AUDIO_S16BE" },
	//{ SDL_AUDIO_S32LE, "AUDIO_S32LE" },
	//{ SDL_AUDIO_S32BE, "AUDIO_S32BE" },
	{ SDL_AUDIO_F32LE, "AUDIO_F32LE" },
	{ SDL_AUDIO_F32BE, "AUDIO_F32BE" }
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
	Com_Printf( "  Channels: %d\n", (int) spec->channels );
}
static void SND_DeviceList(void)
{
	int i;
	int num_playbacks;
	SDL_AudioDeviceID *playbacks = SDL_GetAudioPlaybackDevices(&num_playbacks);
#ifdef USE_SDL_AUDIO_CAPTURE
	int num_captures;
	SDL_AudioDeviceID *captures = SDL_GetAudioRecordingDevices(&num_captures);
#endif

	if ( playbacks ) {
		Com_Printf("Printing audio playback device list. Number of devices: %i\n\n", num_playbacks);

		for (i = 0; i < num_playbacks; i++)
		{
			SDL_AudioDeviceID instance = playbacks[i];
			Com_Printf("  Audio device %u: %s\n", instance, SDL_GetAudioDeviceName(instance));
		}
		SDL_free( playbacks );
	}

#ifdef USE_SDL_AUDIO_CAPTURE
	if ( captures ) {
		Com_Printf("Printing audio capture device list. Number of devices: %i\n\n", num_captures);

		for (i = 0; i < num_captures; i++)
		{
			SDL_AudioDeviceID instance = captures[i];
			Com_Printf("  Audio device %u: %s\n", instance, SDL_GetAudioDeviceName(instance));
		}
		SDL_free( captures );
	}
#endif
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
		case  8: return  8000;
	}
}


static int SND_SamplesForFreq( int freq, int level )
{
	int samples;

	switch ( freq )
	{
		case  8000: samples = 128; break;
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
	SDL_AudioSpec spec;
	SDL_AudioDeviceID devid = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
	int tmp, samples;

	if ( snd_inited )
		return qtrue;

	s_sdlDevSamps = Cvar_Get( "s_sdlDevSamps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_sdlDevSamps, "Number of audio samples to provide to the SDL audio output device. When set to 0 it picks a value based on s_khz" );
	s_sdlMixSamps = Cvar_Get( "s_sdlMixSamps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_sdlMixSamps, "Number of audio samples for Enemy Territory's audio mixer when using SDL audio output" );
	s_sdlLevelSamps = Cvar_Get( "s_sdlLevelSamps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_sdlLevelSamps, "0", "2", CV_INTEGER );

	if ( s_sdlDriver->string[0] != '\0' )
	{
		SDL_SetHint( SDL_HINT_AUDIO_DRIVER, s_sdlDriver->string );
	}

	Com_Printf( "SDL_Init( SDL_INIT_AUDIO )... " );

	if ( !SDL_Init( SDL_INIT_AUDIO ) )
	{
		Com_Printf( "FAILED (%s)\n", SDL_GetError() );
		return qfalse;
	}

	Cmd_AddCommand( "s_devlist", SND_DeviceList );

	Com_Printf( "OK\n" );

	Com_Printf( "SDL audio driver is \"%s\".\n", SDL_GetCurrentAudioDriver() );

	SDL_zero( spec );

	spec.freq = SNDDMA_KHzToHz( s_khz->integer );
	if ( spec.freq == 0 )
		spec.freq = 22050;

	tmp = s_bits->integer; // maybe do != 16 && != 8
	if ( tmp < 16 )
		tmp = 8;

	spec.format = ((tmp == 16) ? SDL_AUDIO_S16 : SDL_AUDIO_U8);

	// I dunno if this is the best idea, but I'll give it a try...
	//  should probably check a cvar for this...
	if ( s_sdlDevSamps->integer > 0 )
		samples = s_sdlDevSamps->value;
	else
	{
		samples = SND_SamplesForFreq(spec.freq, s_sdlLevelSamps->integer);
	}

	spec.channels = s_numchannels->integer;

	if ( !Q_stricmp(s_device->string, "default") || !*s_device->string || s_device->integer == 0 ) {
		Com_Printf( "Acquiring default audio device\n" );
	}
	else {
		int num_playbacks;
		SDL_AudioDeviceID *playbacks = SDL_GetAudioPlaybackDevices(&num_playbacks);

		if ( playbacks ) {
			if ( Q_isanumber( s_device->string ) && Q_isintegral( s_device->value ) ) {
				if ( s_device->integer > 0 && s_device->integer < num_playbacks ) {
					devid = (SDL_AudioDeviceID)s_device->integer;
					Com_Printf( "Acquiring audio device %u: %s\n", devid, SDL_GetAudioDeviceName( devid ) );
				}
				else {
					Com_Printf( "SDL audio device out of range: %i\n", s_device->integer );
					devid = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
					Com_Printf( "Acquiring default audio device\n" );
					Cvar_ForceReset( "s_device" );
				}
			}
			else {
				Com_Printf( "SDL audio device invalid: %s\n", s_device->string );
				devid = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
				Com_Printf( "Acquiring default audio device\n" );
				Cvar_ForceReset( "s_device" );
			}
			SDL_free( playbacks );
		}
		else {
			Com_Printf( "Failed to get audio playback device list: %s\n", SDL_GetError() );
			devid = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
			Com_Printf( "Acquiring default audio device\n" );
			Cvar_ForceReset( "s_device" );
		}
	}

	sdlPlaybackStream = SDL_OpenAudioDeviceStream( devid, &spec, SNDDMA_AudioCallback, NULL );
	if ( sdlPlaybackStream == NULL )
	{
		Com_Printf( "SDL_OpenAudioDeviceStream() failed: %s\n", SDL_GetError() );
		SDL_QuitSubSystem( SDL_INIT_AUDIO );
		return qfalse;
	}

	SNDDMA_PrintAudiospec( "SDL_AudioSpec", &spec );

	// dma.samples needs to be big, or id's mixer will just refuse to
	//  work at all; we need to keep it significantly bigger than the
	//  amount of SDL callback samples, and just copy a little each time
	//  the callback runs.
	// 32768 is what the OSS driver filled in here on my system. I don't
	//  know if it's a good value overall, but at least we know it's
	//  reasonable...this is why I let the user override.
	tmp = s_sdlMixSamps->integer;
	if ( !tmp )
		tmp = (samples * spec.channels) * 10;

	// samples must be divisible by number of channels
	tmp -= tmp % spec.channels;
	// round up to next power of 2
	tmp = log2pad( tmp, 1 );

	dmapos = 0;
	dma.samplebits = SDL_AUDIO_BITSIZE( spec.format );
	dma.isfloat = SDL_AUDIO_ISFLOAT( spec.format );
	dma.channels = spec.channels;
	dma.samples = tmp;
	dma.fullsamples = dma.samples / dma.channels;
	dma.submission_chunk = 1;
	dma.speed = spec.freq;
	dmasize = (dma.samples * (dma.samplebits/8));
	dma.buffer = calloc(1, dmasize);

#ifdef USE_SDL_AUDIO_CAPTURE
	// !!! FIXME: some of these SDL_OpenAudioDevice() values should be cvars.
	s_sdlCapture = Cvar_Get( "s_sdlCapture", "1", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( s_sdlCapture, "Set to 1 to enable SDL audio capture" );
	if (!s_sdlCapture->integer)
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
		SDL_AudioSpec capturespec;
		SDL_zero(capturespec);
		capturespec.freq = 48000;
		capturespec.format = SDL_AUDIO_S16;
		capturespec.channels = 1;
		sdlCaptureStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &capturespec, NULL, NULL);
		Com_Printf( "SDL capture device %s.\n",
				    (sdlCaptureStream == NULL) ? "failed to open" : "opened");
	}
#endif

	Com_Printf("Starting SDL audio callback...\n");
	SDL_ResumeAudioStreamDevice(sdlPlaybackStream);
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
	if (sdlPlaybackStream != NULL)
	{
		Com_Printf("Closing SDL audio playback device...\n");
		SDL_PauseAudioStreamDevice(sdlPlaybackStream);
		SDL_DestroyAudioStream(sdlPlaybackStream);
		Com_Printf("SDL audio playback device closed.\n");
		sdlPlaybackStream = NULL;
	}

#ifdef USE_SDL_AUDIO_CAPTURE
	if (sdlCaptureStream != NULL)
	{
		Com_Printf("Closing SDL audio capture device...\n");
		SDL_PauseAudioStreamDevice(sdlCaptureStream);
		SDL_DestroyAudioStream(sdlCaptureStream);
		Com_Printf("SDL audio capture device closed.\n");
		sdlCaptureStream = NULL;
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
	SDL_UnlockAudioStream( sdlPlaybackStream );
}


/*
===============
SNDDMA_BeginPainting
===============
*/
void SNDDMA_BeginPainting( void )
{

	SDL_LockAudioStream( sdlPlaybackStream );
}


#ifdef USE_VOIP
void SNDDMA_StartCapture(void)
{
#ifdef USE_SDL_AUDIO_CAPTURE
	if (sdlCaptureStream != NULL)
	{
		SDL_ClearAudioStream(sdlCaptureStream);
		SDL_ResumeAudioStreamDevice(sdlCaptureStream);
	}
#endif
}


int SNDDMA_AvailableCaptureSamples(void)
{
#ifdef USE_SDL_AUDIO_CAPTURE
	// divided by 2 to convert from bytes to (mono16) samples.
	return sdlCaptureStream ? (SDL_GetAudioStreamAvailable(sdlCaptureStream) / 2) : 0;
#else
	return 0;
#endif
}


void SNDDMA_Capture(int samples, byte *data)
{
#ifdef USE_SDL_AUDIO_CAPTURE
	// multiplied by 2 to convert from (mono16) samples to bytes.
	if (sdlCaptureStream != NULL)
	{
		SDL_GetAudioStreamData(sdlCaptureStream, data, samples * 2);
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
	if (sdlCaptureStream != NULL)
	{
		SDL_PauseAudioStreamDevice(sdlCaptureStream);
	}
#endif
}

void SNDDMA_MasterGain( float val )
{
#ifdef USE_SDL_AUDIO_CAPTURE
	SDL_SetAudioStreamGain( sdlCaptureStream, val );
#endif
}
#endif
