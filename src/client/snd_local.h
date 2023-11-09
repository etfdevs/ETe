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
// snd_local.h -- private sound definitions

#ifndef __SND_LOCAL_H__
#define __SND_LOCAL_H__

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "snd_public.h"
#include "snd_codec.h"

#define	PAINTBUFFER_SIZE		4096					// this is in samples

#define SND_CHUNK_SIZE			1024					// samples
#define SND_CHUNK_SIZE_FLOAT	(SND_CHUNK_SIZE/2)		// floats
#define SND_CHUNK_SIZE_BYTE		(SND_CHUNK_SIZE*2)		// floats

typedef struct {
	int			left;	// the final values will be clamped to +/- 0x00ffff00 and shifted down
	int			right;
} portable_samplepair_t;

typedef struct adpcm_state {
	short	sample;		/* Previous output value */
	char	index;		/* Index into stepsize table */
} adpcm_state_t;

typedef	struct sndBuffer_s {
	short					sndChunk[SND_CHUNK_SIZE];
	struct sndBuffer_s		*next;
	int						size;
	adpcm_state_t			adpcm;
} sndBuffer;

typedef struct sfx_s {
	sndBuffer		*soundData;
	qboolean		defaultSound;			// couldn't be loaded, so use buzz
	qboolean		inMemory;				// not in Memory
	qboolean		soundCompressed;		// not in Memory
	int				soundCompressionMethod;
	int 			soundLength;
	int				soundChannels;
	char 			soundName[MAX_QPATH];
	int				lastTimeUsed;
	struct sfx_s	*next;
} sfx_t;

typedef struct {
	unsigned int channels;
	unsigned int samples;				// mono samples in buffer
	int			fullsamples;			// samples with all channels in buffer (samples divided by channels)
	int			submission_chunk;		// don't mix less than this #
	int			samplebits;
	int			isfloat;
	int			speed;
	byte		*buffer;
	const char	*driver;
} dma_t;

extern byte *dma_buffer2;

#define START_SAMPLE_IMMEDIATE	0x7fffffff

#define MAX_DOPPLER_SCALE 50.0f //arbitrary

#define THIRD_PERSON_THRESHOLD_SQ (48.0f*48.0f)

typedef struct loopSound_s {
	vec3_t		origin;
	vec3_t		velocity;
	float range;            //----(SA)	added
	sfx_t		*sfx;
	int			mergeFrame;
	int vol;
	qboolean loudUnderWater;    // (SA) set if this sound should be played at full vol even when under water (under water loop sound for ex.)
	qboolean	active;
	qboolean	kill;
	qboolean	doppler;
	float		dopplerScale;
	float		oldDopplerScale;
	int			framenum;
	int startTime, startSample;         // ydnar: so looping sounds can be out of phase
} loopSound_t;

typedef struct
{
	int			allocTime;
	int			startSample;	// START_SAMPLE_IMMEDIATE = set immediately on next mix
	int			entnum;			// to allow overriding a specific sound
	int			entchannel;		// to allow overriding a specific sound
	int			leftvol;		// 0-255 volume after spatialization
	int			rightvol;		// 0-255 volume after spatialization
	int			master_vol;		// 0-255 volume before spatialization
	float		dopplerScale;
	float		oldDopplerScale;
	vec3_t		origin;			// only use if fixed_origin is set
	qboolean	fixed_origin;	// use origin instead of fetching entnum's origin
	sfx_t		*thesfx;		// sfx structure
	qboolean	doppler;
	int flags;                  //----(SA)	added
	qboolean	fullVolume;
} channel_t;


#define WAV_FORMAT_PCM			0x0001
#define WAVE_FORMAT_IEEE_FLOAT	0x0003

typedef struct {
	int			format;
	int			rate;
	int			width;
	int			channels;
	int			samples;
	int			dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

// Interface between Q3 sound "api" and the sound backend
typedef struct
{
	void (*Shutdown)(void);
	void (*Reload)( void );
	void (*StartSound)( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx, int volume );
	void (*StartSoundEx)( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx, int flags, int volume );
	void (*StartLocalSound)( sfxHandle_t sfx, int channelNum, int volume );
	void (*StartBackgroundTrack)( const char *intro, const char *loop, int fadeupTime );
	void (*StopBackgroundTrack)( void );
	float (*StartStreamingSound)( const char *intro, const char *loop, int entnum, int channel, int attenuation );
	void (*StopEntStreamingSound)( int entNum );
	void (*FadeStreamingSound)( float targetvol, int time, int ssNum );
	void (*RawSamples)(int stream, int samples, int rate, int width, int channels, const byte *data, float lvol, float rvol, int entityNum);
	void (*ClearSounds)( qboolean clearStreaming, qboolean clearMusic );
	void (*StopAllSounds)( void );
	void (*FadeAllSounds)( float targetvol, int time, qboolean stopsounds );
	void (*ClearLoopingSounds)( void );
	void (*AddLoopingSound)( const vec3_t origin, const vec3_t velocity, const int range, sfxHandle_t sfx, int volume, int soundTime );
	void (*AddRealLoopingSound)( const vec3_t origin, const vec3_t velocity, const int range, sfxHandle_t sfx, int volume );
	void (*Respatialize)( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
	void (*UpdateEntityPosition)( int entityNum, const vec3_t origin );
	void (*Update)( int msec );
	void (*DisableSounds)( void );
	void (*BeginRegistration)( void );
	sfxHandle_t (*RegisterSound)( const char *sample, qboolean compressed );
	void (*ClearSoundBuffer)( qboolean killStreaming );
	void (*SoundInfo)( void );
	void (*SoundList)( void );

	int (*GetVoiceAmplitude)( int entityNum );
	// START	xkan, 9/23/2002
	// returns how long the sound lasts in milliseconds
	int (*GetSoundLength)( sfxHandle_t sfxHandle );
	// END		xkan, 9/23/2002
	// ydnar: for looped sound synchronization
	int (*GetCurrentSoundTime)( void );

#ifdef USE_VOIP
	void (*StartCapture)( void );
	int (*AvailableCaptureSamples)( void );
	void (*Capture)( int samples, byte *data );
	void (*StopCapture)( void );
	void (*MasterGain)( float gain );
#endif
} soundInterface_t;


/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init(void);

// gets the current DMA position
int		SNDDMA_GetDMAPos(void);

// shutdown the DMA xfer.
void	SNDDMA_Shutdown(void);

void	SNDDMA_BeginPainting (void);

void	SNDDMA_Submit(void);

//====================================================================

#define	MAX_CHANNELS			96

extern	channel_t   s_channels[MAX_CHANNELS];
extern	channel_t   loop_channels[MAX_CHANNELS];
extern	int		numLoopChannels;

extern	int		s_soundtime;
extern	int		s_paintedtime;
extern	vec3_t	listener_forward;
extern	vec3_t	listener_right;
extern	vec3_t	listener_up;
extern	dma_t	dma;

// Ridah, streaming sounds
typedef struct {
	snd_stream_t *stream;
	char name[MAX_QPATH];
	char loopStream[MAX_QPATH];
	char queueStream[MAX_QPATH];
	int queueStreamType;
	int entnum;
	int channel;
	qboolean attenuation;
	int fadeStart;
	int fadeEnd;
	float fadeStartVol;
	float fadeTargetVol;
} streamingSound_t;

#define	MAX_RAW_SAMPLES	16384
#define MAX_RAW_STREAMS (MAX_CLIENTS * 2 + 1)
#define MAX_STREAMING_SOUNDS 12

#define RAW_STREAM_MUSIC 0
#ifdef USE_VOIP
#define RAW_STREAM_VOIP (RAW_STREAM_MUSIC + 1)
#define RAW_STREAM_SOUNDS (RAW_STREAM_VOIP + MAX_CLIENTS)
#else
#define RAW_STREAM_SOUNDS (RAW_STREAM_MUSIC + 1)
#endif
#define RAW_STREAM(x) ((x ? RAW_STREAM_SOUNDS + x - 1 : 0))

extern streamingSound_t streamingSounds[MAX_STREAMING_SOUNDS];
extern	portable_samplepair_t s_rawsamples[MAX_RAW_STREAMS][MAX_RAW_SAMPLES];
extern	int		s_rawend[MAX_RAW_STREAMS];

extern cvar_t *s_volume;
extern cvar_t *s_musicVolume;
//extern cvar_t *s_muted;
extern cvar_t *s_doppler;
extern cvar_t *s_mute;
extern cvar_t *s_muteWhenMinimized;
extern cvar_t *s_muteWhenUnfocused;

extern cvar_t *s_testsound;

#if defined(USE_SDL)
extern cvar_t *s_sdlDriver;
#endif

extern float s_volCurrent;

qboolean S_LoadSound( sfx_t *sfx );

void		SND_free(sndBuffer *v);
sndBuffer*	SND_malloc( void );
void		SND_setup( void );
void		SND_shutdown( void );

void S_PaintChannels(int endtime);

// spatializes a channel
void S_Spatialize(channel_t *ch);

// adpcm functions
int  S_AdpcmMemoryNeeded( const wavinfo_t *info );
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples );
void S_AdpcmGetSamples(sndBuffer *chunk, short *to);

// wavelet function

#define SENTINEL_MULAW_ZERO_RUN 127
#define SENTINEL_MULAW_FOUR_BIT_RUN 126

qboolean S_FreeOldestSound( void );

#define	NXStream byte

void encodeWavelet(sfx_t *sfx, short *packets);
void decodeWavelet( sndBuffer *stream, short *packets);

void encodeMuLaw( sfx_t *sfx, short *packets);
extern short mulawToShort[256];

extern short *sfxScratchBuffer;
extern sfx_t *sfxScratchPointer;
extern int	   sfxScratchIndex;

qboolean S_Base_Init( soundInterface_t *si );

#endif
