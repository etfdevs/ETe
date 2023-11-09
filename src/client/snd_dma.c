/*
===========================================================================
Wolfenstein: Enemy Territory GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

ET: Legacy
Copyright (C) 2012-2017 ET:Legacy team <mail@etlegacy.com>

This file is part of the Wolfenstein: Enemy Territory GPL Source Code (Wolf ET Source Code)
This file is part of ET: Legacy - http://www.etlegacy.com

Wolf ET Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Wolf ET Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wolf ET Source Code. If not, see <http://www.gnu.org/licenses/>.

In addition, Wolfenstein: Enemy Territory GPL Source Code is also
subject to certain additional terms. You should have received a copy
of these additional terms immediately following the terms and conditions
of the GNU General Public License which accompanied the source code.
If not, please request a copy in writing from id Software at the address below.

id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

/*****************************************************************************
 * name:		snd_dma.c
 *
 * desc:		main control for any streaming sound output device
 *
 * $Archive: /MissionPack/code/client/snd_dma.c $
 *
 *****************************************************************************/

#include "snd_local.h"
#include "snd_codec.h"
#include "client.h"

static void S_Update_( int msec );
void S_Base_StopAllSounds(void);
void S_StopStreamingSound(int stream);
void S_FreeStreamingSound(int stream);
void S_UpdateStreamingSounds(void);
static void S_memoryLoad( sfx_t *sfx );

streamingSound_t streamingSounds[MAX_STREAMING_SOUNDS];
static vec3_t entityPositions[MAX_GENTITIES];


static byte		buffer2[ 0x10000 ]; // for muted painting

byte			*dma_buffer2;

// =======================================================================
// Internal sound data & structures
// =======================================================================

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define		SOUND_FULLVOLUME	80
#define     SOUND_RANGE_DEFAULT 1250

channel_t   s_channels[MAX_CHANNELS];
channel_t   loop_channels[MAX_CHANNELS];
int			numLoopChannels;

static		qboolean	s_soundStarted;
static		qboolean	s_soundMuted;

// sound fading
static float    s_volStart, s_volTarget;
static int      s_volTime1, s_volTime2;
static float    s_volFadeFrac;
static qboolean s_stopSounds;
float           s_volCurrent;

dma_t		dma;

static int			listener_number;
static vec3_t		listener_origin;
static vec3_t		listener_axis[3];

int			s_soundtime;		// sample PAIRS
int   		s_paintedtime; 		// sample PAIRS

// MAX_SFX may be larger than MAX_SOUNDS because
// of custom player sounds
#define MAX_SFX			4096
static sfx_t s_knownSfx[MAX_SFX];
static int s_numSfx = 0;

#define LOOP_HASH		128
static sfx_t *sfxHash[LOOP_HASH];

cvar_t      *s_khz;
cvar_t		*s_testsound;
cvar_t		*s_show;
static cvar_t *s_mixahead;
static cvar_t *s_mixOffset;

#if defined(__linux__) || defined(__APPLE__) || defined(__APPLE_CC__)
cvar_t		*s_device;
#endif

#if defined(USE_SDL)
cvar_t		*s_sdlDriver;
#endif

cvar_t *s_debugStreams;

// fretn
cvar_t      *s_bits;
cvar_t      *s_numchannels;

#define MAX_LOOP_SOUNDS 1024

static loopSound_t		loopSounds[MAX_LOOP_SOUNDS];
static int				numLoopSounds = 0;
static	channel_t		*freelist = NULL;

int	s_rawend[MAX_RAW_STREAMS];
portable_samplepair_t	s_rawsamples[MAX_RAW_STREAMS][MAX_RAW_SAMPLES];


// ====================================================================
// User-setable variables
// ====================================================================

/**
 * @brief S_Base_SoundInfo
 */
static void S_Base_SoundInfo( void )
{
	Com_Printf("----- Sound Info -----\n");
	if (!s_soundStarted)
	{
		Com_Printf("sound system not started\n");
	}
	else
	{
		if (s_soundMuted)
		{
			Com_Printf("sound system is muted\n");
		}

		Com_Printf("%5d channels\n", dma.channels);
		Com_Printf("%5d samples\n", dma.samples);
		Com_Printf("%5d samplebits (%s)\n", dma.samplebits, dma.isfloat ? "float" : "int");
		Com_Printf("%5d submission_chunk\n", dma.submission_chunk);
		Com_Printf("%5d speed\n", dma.speed);
		Com_Printf("%p dma buffer\n", dma.buffer);
		if ( dma.driver )
		{
			Com_Printf( "Using %s subsystem\n", dma.driver );
		}
		if (streamingSounds[0].stream)
		{
			Com_Printf("Background file: %s\n", streamingSounds[0].loopStream);
		}
		else
		{
			Com_Printf("No background file.\n");
		}

	}
	Com_Printf("----------------------\n");
}

#ifdef USE_VOIP
/**
 * @brief S_Base_StartCapture
 *
 * @todo FIXME: write me.
 */
static void S_Base_StartCapture(void)
{

}

/**
 * @brief S_Base_AvailableCaptureSamples
 * @return
 *
 * @todo FIXME: write me.
 */
static int S_Base_AvailableCaptureSamples(void)
{
	return 0;
}

/**
 * @brief S_Base_Capture
 * @param samples - unused
 * @param data - unused
 *
 * @todo FIXME: write me.
 */
static void S_Base_Capture(int samples, byte *data)
{

}

/**
 * @brief S_Base_StopCapture
 *
 * @todo FIXME: write me.
 */
static void S_Base_StopCapture(void)
{

}

/**
 * @brief S_Base_MasterGain
 * @param val - unused
 *
 * @todo FIXME: write me.
 */
static void S_Base_MasterGain(float val)
{

}
#endif

/**
 * @brief S_Base_SoundList
 */
static void S_Base_SoundList( void )
{
	int		i;
	const sfx_t *sfx;
	int		total = 0;
	const char *type[4] = { "16bit", "adpcm", "daub4", "mulaw" };
	const char *mem[2] = { "paged out", "resident" };

	for (sfx=&s_knownSfx[1], i=1 ; i<s_numSfx ; i++, sfx++) {
		int size = sfx->soundLength;
		total += size;
		Com_Printf("%6i[%s] : %s[%s]\n", size,
				type[sfx->soundCompressionMethod],
				sfx->soundName, mem[sfx->inMemory] );
	}
	Com_Printf("Total number of sounds: %i\n", s_numSfx-1);
	Com_Printf ("Total resident: %i\n", total);
	S_DisplayFreeMemory();
}

/**
 * @brief S_ChannelFree
 * @param[in,out] v
 */
static void S_ChannelFree( channel_t *v )
{
	v->thesfx = NULL;
	*(channel_t **)v = freelist;
	freelist = (channel_t*)v;
}

/**
 * @brief S_ChannelMalloc
 * @return
 */
static channel_t* S_ChannelMalloc( int allocTime )
{
	channel_t *v;
	if (freelist == NULL) {
		return NULL;
	}
	v = freelist;
	freelist = *(channel_t **)freelist;
	v->allocTime = allocTime;
	return v;
}

/**
 * @brief S_ChannelSetup
 */
static void S_ChannelSetup( void )
{
	channel_t *p, *q;

	// clear all the sounds so they don't
	Com_Memset(s_channels, 0, sizeof(s_channels));

	p = s_channels;
	q = p + MAX_CHANNELS;
	while (--q > p)
	{
		*(channel_t **)q = q - 1;
	}

	*(channel_t **)q = NULL;
	freelist = p + MAX_CHANNELS - 1;
	Com_DPrintf("Channel memory manager started\n");
}



// =======================================================================
// Load a sound
// =======================================================================

/**
 * @brief S_HashSFXName
 * @param[in] name
 * @return a hash value for the sfx name
 */
static unsigned int S_HashSFXName(const char *name) {
	unsigned int hash;
	char	letter;
	int		i;

	hash = 0;
	i = 0;
	while (name[i] != '\0') {
		letter = tolower(name[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		hash+=(int)(letter)*(i+119);
		i++;
	}
	hash &= (LOOP_HASH-1);
	return hash;
}

/**
 * @brief Will allocate a new sfx if it isn't found
 * @param[in] name
 * @return
 */
static sfx_t *S_FindName( const char *name )
{
	int		i;
	int		hash;

	sfx_t	*sfx;

	if (!name) {
		//Com_Error(ERR_FATAL, "Sound name is NULL");
		return NULL;
	}

	if (!name[0]) {
		//Com_Printf( S_COLOR_YELLOW "WARNING: Sound name is empty\n" );
		return NULL;
	}

	if (strlen(name) >= MAX_QPATH) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Sound name is too long: %s\n", name );
		return NULL;
	}

	if ( strlen(name) > 1 && name[0] == '*' /*&& name[1] != '*'*/ ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Tried to load player sound directly: %s\n", name );
		return NULL;
	}

	hash = S_HashSFXName( name );

	sfx = sfxHash[hash];
	// see if already loaded
	while (sfx) {
		if (!Q_stricmp(sfx->soundName, name) ) {
			return sfx;
		}
		sfx = sfx->next;
	}

	// find a free sfx
	for ( i=1 ; i < s_numSfx ; i++) {
		if (!s_knownSfx[i].soundName[0]) {
			break;
		}
	}

	if (i == s_numSfx) {
		if (s_numSfx >= MAX_SFX) {
			Com_Error (ERR_FATAL, "S_FindName: out of sfx_t");
		}
		s_numSfx++;
	}
	
	sfx = &s_knownSfx[i];
	Com_Memset (sfx, 0, sizeof(*sfx));
	strcpy (sfx->soundName, name);

	sfx->next = sfxHash[hash];
	sfxHash[hash] = sfx;

	return sfx;
}


/**
 * @brief S_Base_Reload
 */
static void S_Base_Reload( void )
{
	sfx_t *sfx;
	int i;

	if ( !s_soundStarted ) {
		return;
	}

	Com_Printf( "reloading sounds...\n" );

	S_Base_StopAllSounds();

	for ( sfx = s_knownSfx, i = 0; i < s_numSfx; i++, sfx++ ) {
		sfx->inMemory = qfalse;
		S_memoryLoad( sfx );
	}
}

/**
 * @brief Disables sounds until the next S_BeginRegistration.
 *       This is called when the hunk is cleared and the sounds are no longer valid.
 */
static void S_Base_DisableSounds( void )
{
	S_Base_StopAllSounds();
	s_soundMuted = qtrue;
}

/**
 * @brief Creates a default buzz sound if the file can't be loaded
 * @param[in] name
 * @param[in] compressed
 * @return
 */
static sfxHandle_t S_Base_RegisterSound( const char *name, qboolean compressed )
{
	sfx_t	*sfx;

	compressed = qfalse; // FIXME always sets to false
	if (!s_soundStarted) {
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_DPrintf( "Sound name exceeds MAX_QPATH\n" );
		return 0;
	}

	sfx = S_FindName( name );
	if ( !sfx ) {
		return 0;
	}

	if ( sfx->soundData ) {
		if ( sfx->defaultSound ) {
			if ( com_developer->integer ) {
				Com_DPrintf( S_COLOR_YELLOW "WARNING: could not find %s - using default\n", sfx->soundName );
			}
			return 0;
		}
		return sfx - s_knownSfx;
	}

	sfx->inMemory = qfalse;
	sfx->soundCompressed = compressed;

//	if (!compressed) {
	S_memoryLoad( sfx );
//	}

	if ( sfx->defaultSound ) {
		if ( com_developer->integer ) {
			Com_DPrintf( S_COLOR_YELLOW "WARNING: could not find %s - using default\n", sfx->soundName );
		}
		return 0;
	}

	return sfx - s_knownSfx;
}


/*
=====================
S_BeginRegistration

=====================
*/
static void S_Base_BeginRegistration( void ) {
	s_soundMuted = qfalse;		// we can play again

	SND_setup();

	Com_Memset( s_knownSfx, 0, sizeof( s_knownSfx ) );
	Com_Memset( sfxHash, 0, sizeof( sfxHash ) );

	s_numSfx = 1; // No need to waste an actual sound file or bytes in memory
}


static void S_memoryLoad( sfx_t *sfx ) {

	// load the sound file
	if ( !S_LoadSound ( sfx ) ) {
		sfx->defaultSound = qtrue;
	}

	sfx->inMemory = qtrue;
}

//=============================================================================

/*
=================
S_SpatializeOrigin

Used for spatializing s_channels
=================
*/
static void S_SpatializeOrigin (vec3_t origin, int master_vol, int *left_vol, int *right_vol, float range, int no_attenuation )
{
	vec_t lscale, rscale;

	if (dma.channels == 1 || no_attenuation)     // no attenuation = no spatialization
	{
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		vec_t  dist;
		vec3_t source_vec;
		vec3_t vec;
		float  dist_fullvol = range * 0.064f;

		// calculate stereo separation and distance attenuation
		VectorSubtract(origin, listener_origin, source_vec);

		dist  = VectorNormalize(source_vec);
		dist -= dist_fullvol;
		if (dist < 0.0f || no_attenuation)
		{
			dist = 0.0f;           // close enough to be at full volume
		}
		else
		{
			dist /= range;
		}

		VectorRotate(source_vec, listener_axis, vec);

		rscale = (float)(sqrt((double)(1.0f - vec[1])));
		lscale = (float)(sqrt((double)(1.0f + vec[1])));
		if (rscale < 0)
		{
			rscale = 0;
		}
		else
		{
			rscale *= 1.0f - dist;
		}
		if (lscale < 0)
		{
			lscale = 0;
		}
		else
		{
			lscale *= 1.0f - dist;
		}
	}

	// add in distance effect
	*right_vol = (int)(master_vol * rscale);
	if (*right_vol < 0)
	{
		*right_vol = 0;
	}

	*left_vol = (int)(master_vol * lscale);
	if (*left_vol < 0)
	{
		*left_vol = 0;
	}
}

// =======================================================================
// Start a sound effect
// =======================================================================

/*
=================
S_Base_HearingThroughEntity

Also see S_AL_HearingThroughEntity
=================
*/
static qboolean S_Base_HearingThroughEntity( int entityNum, vec3_t origin )
{
	if ( entityNum == listener_number )
		return qtrue;
	return qfalse;
#if 0
	float	distanceSq;
	vec3_t	sorigin;

	if (origin)
		VectorCopy(origin, sorigin);
	else
		VectorCopy(entityPositions[entityNum], sorigin);

	if( listener_number == entityNum )
	{
		// This is an outrageous hack to detect
		// whether or not the player is rendering in third person or not. We can't
		// ask the renderer because the renderer has no notion of entities and we
		// can't ask cgame since that would involve changing the API and hence mod
		// compatibility. I don't think there is any way around this, but I'll leave
		// the FIXME just in case anyone has a bright idea.
		distanceSq = DistanceSquared(
				sorigin,
				listener_origin );

		if( distanceSq > THIRD_PERSON_THRESHOLD_SQ )
			return qfalse; //we're the player, but third person
		else
			return qtrue;  //we're the player
	}
	else
		return qfalse; //not the player
#endif
}

/*
====================
S_StartSound

Validates the parms and queues the sound up
if pos is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound

  flags:  (currently apply only to non-looping sounds)
	SND_NORMAL			    0	- (default) allow sound to be cut off only by the same sound on this channel
	SND_OKTOCUT			0x001	- allow sound to be cut off by any following sounds on this channel
	SND_REQUESTCUT		0x002	- allow sound to be cut off by following sounds on this channel only for sounds who request cutoff
	SND_CUTOFF			0x004	- cut off sounds on this channel that are marked 'SND_REQUESTCUT'
	SND_CUTOFF_ALL		0x008	- cut off all sounds on this channel
====================
*/

/*
====================
S_Base_MainStartSound

Validates the parms and ques the sound up
if origin is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound
====================
*/
static void S_Base_MainStartSoundEx( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle, qboolean localSound, int flags, int volume ) {
	channel_t	*ch;
	sfx_t		*sfx;
	int		i, time;
	//int		inplay, allowed;
	qboolean	fullVolume;

	if ( !s_soundStarted || s_soundMuted || !sfxHandle ) {
		return;
	}

	if ( !origin && ( entityNum < 0 || entityNum >= MAX_GENTITIES ) ) {
		Com_Error( ERR_DROP, "S_StartSound: bad entitynum %i", entityNum );
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_StartSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = &s_knownSfx[ sfxHandle ];

	if (sfx->inMemory == qfalse) {
		S_memoryLoad(sfx);
	}

	if ( s_show->integer == 1 ) {
		Com_Printf( "%i : %s\n", s_paintedtime, sfx->soundName );
	}

	time = s_soundtime; // Com_Milliseconds();

	// borrowed from cnq3
	// a UNIQUE entity starting the same sound twice in a frame is either a bug,
	// a timedemo, or a shitmap (eg q3ctf4) giving multiple items on spawn.
	// even if you can create a case where it IS "valid", it's still pointless
	// because you implicitly can't DISTINGUISH between the sounds:
	// all that happens is the sound plays at double volume, which is just annoying

	if ( entityNum != ENTITYNUM_WORLD ) {
		ch = s_channels;
		for ( i = 0; i < MAX_CHANNELS; i++, ch++ ) {
			if ( ch->entnum != entityNum )
				continue;
			if ( ch->allocTime != time )
				continue;
			if ( ch->thesfx != sfx )
				continue;
			sfx->lastTimeUsed = time;
			//Com_Printf( S_COLOR_YELLOW "double sound start: %d %s\n", entityNum, sfx->soundName);
			return;
		}
	}

//	Com_Printf("playing %s\n", sfx->soundName);
	// pick a channel to play on

	// try to limit sound duplication
	/*if ( entityNum == listener_number )
		allowed = 16;
	else
		allowed = 8;*/

	fullVolume = qfalse;
	if (localSound || S_Base_HearingThroughEntity(entityNum, origin)) {
		fullVolume = qtrue;
	}

	ch   = s_channels;
	//inplay = 0;

	if ( entityNum >= 0 ) {
		for ( i = 1; i < MAX_STREAMING_SOUNDS; i++ ) {    // track 0 is music/cinematics
			if ( !streamingSounds[i].stream ) {
				continue;
			}
			// check to see if this character currently has another sound streaming on the same channel
			if ( ( entchannel != CHAN_AUTO ) && ( streamingSounds[i].entnum >= 0 ) && ( streamingSounds[i].channel == entchannel ) && ( streamingSounds[i].entnum == entityNum ) ) {
				// found a match, override this channel
				S_StopStreamingSound(i);
				break;
			}
		}
	}

	// shut off other sounds on this channel if necessary
	for (i = 0; i < MAX_CHANNELS ; i++, ch++)
	{
		if (ch->entnum == entityNum && ch->thesfx)
		{
			if (ch->thesfx == sfx && time - ch->allocTime < 50) // double played in one frame
			{
				return;
			}
			else if (ch->entchannel == entchannel)
			{
				if ((flags & SND_CUTOFF_ALL)) // cut the sounds that are flagged to be cut
				{
					S_ChannelFree(ch);
				}
				else if (ch->flags & SND_NOCUT) // don't cutoff
				{
					continue;
				}
				else if (ch->flags & SND_OKTOCUT) // cutoff sounds that expect to be overwritten
				{
					S_ChannelFree(ch);
				}
				else if ((ch->flags & SND_REQUESTCUT) && (flags & SND_CUTOFF)) // cutoff 'weak' sounds on channel
				{
					S_ChannelFree(ch);
				}
			}
			//else if (ch->entchannel == entchannel && (flags & SND_CUTOFF_ALL)) // cut the sounds that are flagged to be cut
			//{
			//	S_ChannelFree(ch);
			//}
			/*else
			{
				inplay++;
			}*/
		}
	}

	// too much duplicated sounds, ignore
	/*if ( inplay > allowed ) {
		return;
	}*/

	sfx->lastTimeUsed = time;

	ch = NULL;

	// re-use channel if applicable
	for ( i = 0 ; i < MAX_CHANNELS ; i++ ) {
		if ( s_channels[i].entnum == entityNum && s_channels[i].entchannel == entchannel && entchannel != CHAN_AUTO ) {
			if ( !( s_channels[i].flags & SND_NOCUT ) && s_channels[i].thesfx == sfx ) {
				ch = &s_channels[i];
				break;
			}
		}
	}

	if ( !ch )
		ch = S_ChannelMalloc( time );

	if (!ch)
	{
		int oldest = sfx->lastTimeUsed;
		int chosen = -1;

		ch = s_channels;

		for (i = 0 ; i < MAX_CHANNELS ; i++, ch++)
		{
			if (ch->entnum != listener_number && ch->entnum == entityNum && ch->allocTime - oldest < 0 && ch->entchannel != CHAN_ANNOUNCER)
			{
				oldest = ch->allocTime;
				chosen = i;
			}
		}
		if (chosen == -1)
		{
			ch = s_channels;
			for (i = 0 ; i < MAX_CHANNELS ; i++, ch++)
			{
				if (ch->entnum != listener_number && ch->allocTime - oldest < 0 && ch->entchannel != CHAN_ANNOUNCER)
				{
					oldest = ch->allocTime;
					chosen = i;
				}
			}
			if (chosen == -1)
			{
				ch = s_channels;
				if (ch->entnum == listener_number)
				{
					for (i = 0 ; i < MAX_CHANNELS ; i++, ch++)
					{
						if (ch->allocTime - oldest < 0)
						{
							oldest = ch->allocTime;
							chosen = i;
						}
					}
				}
				if (chosen == -1)
				{
					Com_DPrintf(S_COLOR_YELLOW "S_StartSound: No more channels free for %s\n", sfx->soundName);
					return;
				}
			}
		}
		ch            = &s_channels[chosen];
		ch->allocTime = sfx->lastTimeUsed;
		Com_DPrintf(S_COLOR_YELLOW "S_StartSound: No more channels free for %s, dropping earliest sound: %s\n", sfx->soundName, ch->thesfx->soundName);
	}

	if (origin)
	{
		VectorCopy(origin, ch->origin);
		ch->fixed_origin = qtrue;
	}
	else
	{
		ch->fixed_origin = qfalse;
	}

	ch->flags = flags;  //----(SA)	added
	ch->master_vol = volume; //127
	ch->entnum = entityNum;
	ch->thesfx = sfx;
	ch->startSample = START_SAMPLE_IMMEDIATE;
	ch->entchannel = entchannel;
	ch->leftvol = ch->master_vol;		// these will get calced at next spatialize
	ch->rightvol = ch->master_vol;		// unless the game isn't running
	ch->doppler = qfalse;
	ch->fullVolume = fullVolume;

/*	if ( ch->fixed_origin ) {
		S_SpatializeOrigin( ch->origin, ch->master_vol, &ch->leftvol, &ch->rightvol, SOUND_RANGE_DEFAULT, (ch->flags & SND_NO_ATTENUATION) );
	} else {
		S_SpatializeOrigin( entityPositions[ ch->entnum ], ch->master_vol, &ch->leftvol, &ch->rightvol, SOUND_RANGE_DEFAULT, (ch->flags & SND_NO_ATTENUATION) );
	}*/
}

/*
====================
S_StartSound

if origin is NULL, the sound will be dynamically sourced from the entity
====================
*/
void S_Base_StartSound( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle, int volume ) {
	S_Base_MainStartSoundEx( origin, entityNum, entchannel, sfxHandle, qfalse, 0, volume );
}

void S_Base_StartSoundEx( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle, int flags, int volume ) {
	S_Base_MainStartSoundEx( origin, entityNum, entchannel, sfxHandle, qfalse, flags, volume );
}

/*
==================
S_StartLocalSound
==================
*/
void S_Base_StartLocalSound( sfxHandle_t sfxHandle, int channelNum, int volume ) {
	if ( !s_soundStarted || s_soundMuted || !sfxHandle ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_StartLocalSound: handle %i out of range\n", sfxHandle );
		return;
	}

	S_Base_MainStartSoundEx( NULL, listener_number, channelNum, sfxHandle, qtrue, 0, volume );
}

void S_Base_ClearSounds(qboolean clearStreaming, qboolean clearMusic)
{
	if (!s_soundStarted)
	{
		return;
	}

	// stop looping sounds
	Com_Memset(loopSounds, 0, MAX_LOOP_SOUNDS * sizeof(loopSound_t));
	Com_Memset(loop_channels, 0, MAX_CHANNELS * sizeof(channel_t));
	numLoopChannels = 0;
	numLoopSounds   = 0;

	// moved this up so streaming sounds dont get updated with the music, below,
	// and leave us with a snippet off streaming sounds after we reload
	if (clearStreaming)
	{
		int              i;
		streamingSound_t *ss;
		channel_t        *ch;

		for (i = 0, ss = streamingSounds; i < MAX_STREAMING_SOUNDS; i++, ss++)
		{
			if (i > 0 || clearMusic)
			{
				S_StopStreamingSound(i);
			}
		}

		// we should also kill all channels, since we are killing streaming sounds anyway
		// (fixes siren in forest playing after a map_restart/loadgame
		ch = s_channels;
		for (i = 0; i < MAX_CHANNELS; i++, ch++)
		{
			if (ch->thesfx)
			{
				S_ChannelFree(ch);
			}
		}
	}

	if (!clearMusic)
	{
		S_UpdateStreamingSounds();
	}

	if (clearStreaming && clearMusic)
	{
		int clear;

		if (dma.samplebits == 8)
		{
			clear = 0x80;
		}
		else
		{
			clear = 0;
		}

		SNDDMA_BeginPainting();
		if (dma.buffer)
		{
			Com_Memset(dma.buffer, clear, dma.samples * dma.samplebits / 8);
		}
		SNDDMA_Submit();

		// clear out channels so they don't finish playing when audio restarts
		S_ChannelSetup();
	}
}


/*
==================
S_ClearSoundBuffer

If we are about to perform file access, clear the buffer
so sound doesn't stutter.
==================
*/
void S_Base_ClearSoundBuffer( qboolean killStreaming ) {
	if (!s_soundStarted)
		return;

	S_Base_ClearSounds( killStreaming, qtrue );

}


/*
==================
S_StopAllSounds
==================
*/
void S_Base_StopAllSounds( void ) {
	if ( !s_soundStarted ) {
		return;
	}

	S_Base_ClearSoundBuffer( qtrue );
}

/*
==============================================================

continuous looping sounds are added each frame

==============================================================
*/

static void S_Base_StopLoopingSound(int index) {
	loopSounds[index].active = qfalse;
//	loopSounds[index].sfx = 0;
	loopSounds[index].kill = qfalse;
}

/*
==================
S_ClearLoopingSounds

==================
*/
void S_Base_ClearLoopingSounds( void ) {
	int i;
	for ( i = 0 ; i < numLoopSounds ; i++) {
		S_Base_StopLoopingSound( i );
	}
	numLoopSounds = 0;
	numLoopChannels = 0;
}

/*
==================
S_AddLoopingSound

Called during entity generation for a frame
Include velocity in case I get around to doing doppler...
==================
*/

#define UNDERWATER_BIT  16

void S_Base_AddLoopingSound( const vec3_t origin, const vec3_t velocity, const int range, sfxHandle_t sfxHandle, int volume, int soundTime ) {
	sfx_t *sfx;

	if ( !s_soundStarted || s_soundMuted || !sfxHandle || cls.state != CA_ACTIVE ) {
		return;
	}

	if ( numLoopSounds >= MAX_LOOP_SOUNDS ) {
		return;
	}

	if ( !volume ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_AddLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = &s_knownSfx[ sfxHandle ];

	if (sfx->inMemory == qfalse) {
		S_memoryLoad(sfx);
	}

	if ( !sfx->soundLength ) {
		Com_Error( ERR_DROP, "S_AddLoopingSound: %s has length 0", sfx->soundName );
	}

	// ydnar: allow looped sounds to start when initially triggered, rather than in the middle of the sample
	/*loopSounds[numLoopSounds].startSample = sfx->soundLength
		? ( ( s_khz->integer * soundTime ) - s_paintedtime ) % sfx->soundLength
		: 0;

	loopSounds[numLoopSounds].startSample = sfx->soundLength
		? s_paintedtime - ( (int)( dma.speed * ( cl.serverTime - soundTime ) / 1000.0f ) )
		: 0;*/

	loopSounds[numLoopSounds].startSample = soundTime % sfx->soundLength;

	VectorCopy( origin, loopSounds[numLoopSounds].origin );
	VectorCopy( velocity, loopSounds[numLoopSounds].velocity );
	loopSounds[numLoopSounds].active = qtrue;
	loopSounds[numLoopSounds].kill = qtrue;
	loopSounds[numLoopSounds].doppler = qfalse;
	loopSounds[numLoopSounds].oldDopplerScale = 1.0;
	loopSounds[numLoopSounds].dopplerScale = 1.0;
	loopSounds[numLoopSounds].sfx = sfx;
	if ( range ) {
		loopSounds[numLoopSounds].range = range;
	} else {
		loopSounds[numLoopSounds].range = SOUND_RANGE_DEFAULT;
	}

	if ( volume & 1 << UNDERWATER_BIT ) {
		loopSounds[numLoopSounds].loudUnderWater = qtrue;
	}

	if (s_doppler->integer && VectorLengthSquared(velocity)>0.0) {
		vec3_t	out;
		float	lena, lenb;

		// don't do the doppler effect when trumpets of train and station are at the same position
		if (!VectorCompare(entityPositions[listener_number], entityPositions[numLoopSounds])) {
			loopSounds[numLoopSounds].doppler = qtrue;
		} else {
			loopSounds[numLoopSounds].doppler = qfalse;
		}
		lena = DistanceSquared(entityPositions[listener_number], loopSounds[numLoopSounds].origin);
		VectorAdd(entityPositions[numLoopSounds], loopSounds[numLoopSounds].velocity, out);
		//VectorAdd(loopSounds[numLoopSounds].origin, loopSounds[numLoopSounds].velocity, out);
		lenb = DistanceSquared(entityPositions[listener_number], out);
		if ((loopSounds[numLoopSounds].framenum+1) != cls.framecount) {
			loopSounds[numLoopSounds].oldDopplerScale = 1.0;
		} else {
			loopSounds[numLoopSounds].oldDopplerScale = loopSounds[numLoopSounds].dopplerScale;
		}
		if (lena == 0.0f) {
			loopSounds[numLoopSounds].dopplerScale = 1.0f; // no doppler
		} else {
			loopSounds[numLoopSounds].dopplerScale = lenb/(lena*100);
		}

		if (loopSounds[numLoopSounds].dopplerScale<=1.0) {
			loopSounds[numLoopSounds].doppler = qfalse;			// don't bother doing the math
		} else if (loopSounds[numLoopSounds].dopplerScale>MAX_DOPPLER_SCALE) {
			loopSounds[numLoopSounds].dopplerScale = MAX_DOPPLER_SCALE;
		}
	}

	// FIXME ET is > 65535
	if ( volume > 65535 ) {
		volume = 65535;
	} else if ( volume < 0 ) {
		volume = 0;
	}

	// FIXME ET is .vol = (int)( (float)volume * snd.volCurrent );  //----(SA)	modified
	//loopSounds[numLoopSounds].vol = volume;
	loopSounds[numLoopSounds].vol = (int)((float)volume * s_volCurrent);

	loopSounds[numLoopSounds].framenum = cls.framecount;

	numLoopSounds++;
}

/*
==================
S_AddLoopingSound

Called during entity generation for a frame
Include velocity in case I get around to doing doppler...
==================
*/
// ENSI NOTE ET has soundTime parm but it is not used
void S_Base_AddRealLoopingSound( const vec3_t origin, const vec3_t velocity, const int range, sfxHandle_t sfxHandle, int volume ) {
	sfx_t *sfx;

	if ( !s_soundStarted || s_soundMuted || !sfxHandle || !volume ) {
		return;
	}

	if ( numLoopSounds >= MAX_LOOP_SOUNDS ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_AddRealLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = &s_knownSfx[ sfxHandle ];

	if (sfx->inMemory == qfalse) {
		S_memoryLoad(sfx);
	}

	if ( !sfx->soundLength ) {
		Com_Error( ERR_DROP, "S_AddRealLoopingSound: %s has length 0", sfx->soundName );
	}
	VectorCopy( origin, loopSounds[numLoopSounds].origin );
	VectorCopy( velocity, loopSounds[numLoopSounds].velocity );
	if ( range ) {
		loopSounds[numLoopSounds].range = range;
	} else {
		loopSounds[numLoopSounds].range = SOUND_RANGE_DEFAULT;
	}
	loopSounds[numLoopSounds].sfx = sfx;
	loopSounds[numLoopSounds].active = qtrue;
	loopSounds[numLoopSounds].kill = qfalse;
	loopSounds[numLoopSounds].doppler = qfalse;
	loopSounds[numLoopSounds].oldDopplerScale = 1.0f;
	loopSounds[numLoopSounds].dopplerScale = 1.0f;

	if ( volume & 1 << UNDERWATER_BIT ) {
		loopSounds[numLoopSounds].loudUnderWater = qtrue;
	}

	// FIXME ET is > 65535
	if ( volume > 65535 ) {
		volume = 65535;
	} else if ( volume < 0 ) {
		volume = 0;
	}

	loopSounds[numLoopSounds].vol = (int)((float)volume * s_volCurrent);

	numLoopSounds++;
}



/*
==================
S_AddLoopSounds

Spatialize all of the looping sounds.
All sounds are on the same cycle, so any duplicates can just
sum up the channel multipliers.
==================
*/
void S_AddLoopSounds (void) {
	int			i, j, time;
	int			left_total, right_total, left, right;
	channel_t	*ch;
	loopSound_t	*loop, *loop2;
	static int	loopFrame;

	if (!s_soundStarted || s_soundMuted)
	{
		return;
	}

	numLoopChannels = 0;

	time = s_soundtime; // Com_Milliseconds();

	loopFrame++;
	for ( i = 0 ; i < numLoopSounds; i++) {
		loop = &loopSounds[i];
		if ( !loop->active || loop->mergeFrame == loopFrame ) {
			continue;	// already merged into an earlier sound
		}

		if (loop->kill)
		{
			S_SpatializeOrigin(loop->origin, 127, &left_total, &right_total, loop->range, qfalse);            // 3d
		}
		else
		{
			S_SpatializeOrigin(loop->origin, 90, &left_total, &right_total, loop->range, qfalse);             // sphere
		}

		// adjust according to volume
		left_total = (int)( (float)loop->vol * (float)left_total / 256.0 );
		right_total = (int)( (float)loop->vol * (float)right_total / 256.0 );

		loop->sfx->lastTimeUsed = time;

		for (j=(i+1); j< MAX_GENTITIES ; j++) {
			loop2 = &loopSounds[j];
			if ( !loop2->active || loop2->doppler || loop2->sfx != loop->sfx ||
				 loop2->startSample != loop->startSample ) { // ydnar
				continue;
			}
			loop2->mergeFrame = loopFrame;

			if (loop2->kill)
			{
				S_SpatializeOrigin(loop2->origin, 127, &left, &right, loop2->range, qfalse);               // 3d
			}
			else
			{
				S_SpatializeOrigin(loop2->origin, 90, &left, &right, loop2->range, qfalse);                // sphere
			}

			// adjust according to volume
			left = (int)( (float)loop2->vol * (float)left / 256.0 );
			right = (int)( (float)loop2->vol * (float)right / 256.0 );

			loop2->sfx->lastTimeUsed = time;
			left_total += left;
			right_total += right;
		}
		if (left_total == 0 && right_total == 0) {
			continue;		// not audible
		}

		// allocate a channel
		ch = &loop_channels[numLoopChannels];
		
		if (left_total > 255) {
			left_total = 255;
		}
		if (right_total > 255) {
			right_total = 255;
		}
		
		ch->master_vol = 127;
		ch->leftvol = left_total;
		ch->rightvol = right_total;
		ch->thesfx = loop->sfx;
		ch->doppler = loop->doppler;
		ch->dopplerScale = loop->dopplerScale;
		ch->oldDopplerScale = loop->oldDopplerScale;
		ch->fullVolume = qfalse;

		// ydnar: allow offsetting of sound samples
		ch->startSample = loop->startSample;

		numLoopChannels++;
		#if 1
		if (numLoopChannels == MAX_CHANNELS)
		{
			Com_Printf("S_AddLoopSounds warning: MAX_CHANNELS %i reached - loop sound dropped\n", MAX_CHANNELS);
			return;
		}
		#else
		if (numLoopChannels == MAX_CHANNELS) {
			i = numLoopSounds + 1;
			//return;
		}
		#endif
	}
}

//=============================================================================


/*
============
S_Base_RawSamples

Music streaming
============
*/
void S_Base_RawSamples( int stream, int samples, int rate, int width, int n_channels, const byte *data, float lvol, float rvol, int entityNum)
{
	int		i;
	int		src, dst;
	float	scale;
	int		intVolumeLeft = 0, intVolumeRight = 0;
	portable_samplepair_t *rawsamples;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( (stream < 0) || (stream >= MAX_RAW_STREAMS) ) {
		return;
	}
	
	rawsamples = s_rawsamples[stream];

	//if ( s_muted->integer ) {
	//	intVolumeLeft = intVolumeRight = 0;
	/*} else*/ {
		/*int leftvol, rightvol;

		if ( entityNum >= 0 && entityNum < MAX_GENTITIES ) {
			// support spatialized raw streams, e.g. for VoIP
			S_SpatializeOrigin( entityPositions[ entityNum ], 256, &leftvol, &rightvol, SOUND_RANGE_DEFAULT, qfalse );
		} else {
			leftvol = rightvol = 256;
		}*/

		if (stream == 0)
		{
			intVolumeLeft = 256 * lvol * s_musicVolume->value;
			intVolumeRight = 256 * rvol * s_musicVolume->value;
		}
		else
		{
			intVolumeLeft = 256/*leftvol*/ * lvol * s_volume->value;
			intVolumeRight = 256/*rightvol*/ * rvol * s_volume->value;
		}
	}

	if ( s_rawend[stream] - s_soundtime < 0 ) {
		Com_DPrintf( "S_Base_RawSamples: resetting minimum: %i < %i\n", s_rawend[stream], s_soundtime );
		s_rawend[stream] = s_soundtime;
	}

	scale = (float)rate / dma.speed;

//Com_Printf ("%i < %i < %i\n", s_soundtime, s_paintedtime, s_rawend[stream]);
	if (n_channels == 2 && width == 2)
	{
		if (scale == 1.0)
		{	// optimized case
			for (i=0 ; i<samples ; i++)
			{
				dst = s_rawend[stream]&(MAX_RAW_SAMPLES-1);
				s_rawend[stream]++;
				rawsamples[dst].left = ((short *)data)[i*2] * intVolumeLeft;
				rawsamples[dst].right = ((short *)data)[i*2+1] * intVolumeRight;
			}
		}
		else
		{
			for (i=0 ; ; i++)
			{
				src = i*scale;
				if (src >= samples)
					break;
				dst = s_rawend[stream]&(MAX_RAW_SAMPLES-1);
				s_rawend[stream]++;
				rawsamples[dst].left = ((short *)data)[src*2] * intVolumeLeft;
				rawsamples[dst].right = ((short *)data)[src*2+1] * intVolumeRight;
			}
		}
	}
	else if (n_channels == 1 && width == 2)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend[stream]&(MAX_RAW_SAMPLES-1);
			s_rawend[stream]++;
			rawsamples[dst].left = ((short *)data)[src] * intVolumeLeft;
			rawsamples[dst].right = ((short *)data)[src] * intVolumeRight;
		}
	}
	else if (n_channels == 2 && width == 1)
	{
		intVolumeLeft *= 256;
		intVolumeRight *= 256;

		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend[stream]&(MAX_RAW_SAMPLES-1);
			s_rawend[stream]++;
			rawsamples[dst].left = ((char *)data)[src*2] * intVolumeLeft;
			rawsamples[dst].right = ((char *)data)[src*2+1] * intVolumeRight;
		}
	}
	else if (n_channels == 1 && width == 1)
	{
		intVolumeLeft *= 256;
		intVolumeRight *= 256;

		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend[stream]&(MAX_RAW_SAMPLES-1);
			s_rawend[stream]++;
			rawsamples[dst].left = (((byte *)data)[src]-128) * intVolumeLeft;
			rawsamples[dst].right = (((byte *)data)[src]-128) * intVolumeRight;
		}
	}

	if ( s_rawend[stream] - s_soundtime > MAX_RAW_SAMPLES ) {
		Com_DPrintf( "S_Base_RawSamples: overflowed %i > %i\n", s_rawend[stream], s_soundtime );
	}
}

//=============================================================================

/*
=====================
S_UpdateEntityPosition

let the sound system know where an entity currently is
======================
*/
void S_Base_UpdateEntityPosition( int entityNum, const vec3_t origin ) {
	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entityNum );
	}
	VectorCopy( origin, entityPositions[entityNum] );
}


/*
============
S_Respatialize

Change the volumes of all the playing sounds for changes in their positions
============
*/
void S_Base_Respatialize( int entityNum, const vec3_t head, vec3_t axis[3], int inwater ) {
	int			i;
	channel_t	*ch;
	vec3_t		origin;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	listener_number = entityNum;
	VectorCopy(head, listener_origin);
	VectorCopy(axis[0], listener_axis[0]);
	VectorCopy(axis[1], listener_axis[1]);
	VectorCopy(axis[2], listener_axis[2]);

	// update spatialization for dynamic sounds	
	ch = s_channels;
	for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
		if ( !ch->thesfx ) {
			continue;
		}
		// local and first person sounds will always be full volume
		if (ch->fullVolume) {
			ch->leftvol = ch->master_vol;
			ch->rightvol = ch->master_vol;
		} else {
			if (ch->fixed_origin) {
				VectorCopy( ch->origin, origin );
			} else {
				VectorCopy( entityPositions[ ch->entnum ], origin );
			}

			S_SpatializeOrigin (origin, ch->master_vol, &ch->leftvol, &ch->rightvol, SOUND_RANGE_DEFAULT, (ch->flags & SND_NO_ATTENUATION) );
		}
	}

	// add loopsounds
	S_AddLoopSounds ();
}


/*
========================
S_ScanChannelStarts

Returns qtrue if any new sounds were started since the last mix
========================
*/
static qboolean S_ScanChannelStarts( void ) {
	channel_t		*ch;
	int				i;
	qboolean		newSamples;

	newSamples = qfalse;
	ch = s_channels;

	for ( i = 0; i < MAX_CHANNELS; i++, ch++ ) {
		if ( !ch->thesfx ) {
			continue;
		}
		// if this channel was just started this frame,
		// set the sample count to it begins mixing
		// into the very first sample
		if ( ch->startSample == START_SAMPLE_IMMEDIATE ) {
			ch->startSample = s_paintedtime;
			newSamples = qtrue;
			continue;
		}

		// if it is completely finished by now, clear it
		if ( ch->startSample + (ch->thesfx->soundLength) - s_soundtime <= 0 ) {
			S_ChannelFree( ch );
		}
	}

	return newSamples;
}


/*
============
S_Update

Called once each time through the main loop
============
*/
static void S_Base_Update( int msec ) {
	int			i;
	int			total;
	channel_t	*ch;

	if ( !s_soundStarted || s_soundMuted ) {
//		Com_DPrintf ("not started or muted\n");
		return;
	}

	//
	// debugging output
	//
	if ( s_show->integer == 2 ) {
		total = 0;
		ch = s_channels;
		for (i=0 ; i<MAX_CHANNELS; i++, ch++) {
			if (ch->thesfx && (ch->leftvol || ch->rightvol) ) {
				Com_Printf ("%d %d %s\n", ch->leftvol, ch->rightvol, ch->thesfx->soundName);
				total++;
			}
		}

		Com_Printf ("----(%i)---- painted: %i\n", total, s_paintedtime);
	}

	// mix some sound
	S_Update_( msec );
}


static void S_GetSoundtime( void )
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	aviRecordingState_t aviRecord = CL_VideoRecording();

	if ( aviRecord != AVIDEMO_NONE )
	{
		const float duration = MAX( (float)dma.speed / (aviRecord == AVIDEMO_CVAR ? cl_avidemo->value : cl_aviFrameRate->value), 1.0f );
		const float frameDuration = duration + clc.aviSoundFrameRemainder;
		const int msec = (int)frameDuration;

		s_soundtime += msec;
		clc.aviSoundFrameRemainder = frameDuration - msec;

		// use same offset as in game
		s_paintedtime = s_soundtime + (int)(s_mixOffset->value * (float)dma.speed);

		// render exactly one frame of audio data
		clc.aviFrameEndTime = s_paintedtime + (int)(duration + clc.aviSoundFrameRemainder);
		return;
	}

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();
	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped
		
		if (s_paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			s_paintedtime = dma.fullsamples;
			S_Base_StopAllSounds ();
		}
	}
	oldsamplepos = samplepos;

	s_soundtime = buffers * dma.fullsamples + samplepos/dma.channels;

	if ( dma.submission_chunk < 256 ) {
		s_paintedtime = s_soundtime + s_mixOffset->value * dma.speed;
	} else {
		s_paintedtime = s_soundtime + dma.submission_chunk;
	}
}


static void S_Update_( int msec ) {
	unsigned		endtime;
	int				mixAhead[2];
	int				thisTime, sane;
	static int		ot = -1;
	static int		lastTime = 0;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	thisTime = Com_Milliseconds();

	// Updates s_soundtime
	S_GetSoundtime();

	if ( s_soundtime == ot ) {
		return;
	}

	ot = s_soundtime;

	// clear any sound effects that end before the current time,
	// and start any new sounds
	S_ScanChannelStarts();

	sane = thisTime - lastTime;
	if ( sane < msec ) {
		sane = msec;
	}

	mixAhead[0] = s_mixahead->value * (float)dma.speed;
	mixAhead[1] = sane * 0.0015f * (float)dma.speed;

	if ( mixAhead[0] < mixAhead[1] ) {
		mixAhead[0] = mixAhead[1];
	}

	// mix ahead of current position
	endtime = s_paintedtime + mixAhead[0];

	// mix to an even submission block size
	endtime = (endtime + dma.submission_chunk-1)
		& ~(dma.submission_chunk-1);

	// never mix more than the complete buffer
	if ( endtime - s_paintedtime > dma.fullsamples ) {
		endtime = s_paintedtime + dma.fullsamples;
	}

	// add raw data from streamed samples
	S_UpdateStreamingSounds();

	// global volume fading

	// endtime or s_paintedtime or s_soundtime...
	if (s_soundtime < s_volTime2)     // still has fading to do
	{
		if (s_soundtime > s_volTime1)     // has started fading
		{
			s_volFadeFrac = ((float)(s_soundtime - s_volTime1) / (float)(s_volTime2 - s_volTime1));
			s_volCurrent  = ((1.0f - s_volFadeFrac) * s_volStart + s_volFadeFrac * s_volTarget);
		}
		else
		{
			s_volCurrent = s_volStart;
		}
	}
	else
	{
		s_volCurrent = s_volTarget;
		if (s_stopSounds)
		{
			// stop playing any sounds if they are all faded out
			S_StopAllSounds();
			s_stopSounds = qfalse;
		}
	}

	SNDDMA_BeginPainting();

	S_PaintChannels( endtime );

	SNDDMA_Submit();

	lastTime = thisTime;
}


/*
===============================================================================
streamed sound functions
===============================================================================
*/

/**
 * @brief S_StartStreamingSoundEx
 * @param[in] intro
 * @param[in] loop
 * @param[in] entnum
 * @param[in] channel
 * @param[in] music
 * @param[in] param
 * @return
 */
float S_StartStreamingSoundEx(const char *intro, const char *loop, int entnum, int channel,
                              qboolean music, int param)
{
	streamingSound_t *ss       = 0;
	int              freeTrack = 0;

	if (music)
	{
		ss = &streamingSounds[0];
		if (param < 0)
		{
			if (intro && *intro)
			{
				Q_strncpyz(ss->queueStream, intro, sizeof(ss->queueStream));
				ss->queueStreamType = param;
				// Cvar for save game
				if (param == -2)
				{
					Cvar_Set("s_currentMusic", intro);
				}
				if (s_debugStreams->integer)
				{
					if (param == -1)
					{
						Com_Printf("S_StartStreamingSoundEx: queueing '%s' for play once\n", intro);
					}
					else if (param == -2)
					{
						Com_Printf("S_StartStreamingSoundEx: queueing '%s' as new loop\n", intro);
					}
				}
			}
			else
			{
				ss->loopStream[0]   = '\0';
				ss->queueStream[0]  = '\0';
				ss->queueStreamType = 0;
				if (s_debugStreams->integer)
				{
					Com_Printf("S_StartStreamingSoundEx: queue cleared\n");
				}
			}
		}
	}
	else
	{
		int i;

		for (i = 1; i < MAX_STREAMING_SOUNDS; i++)
		{
			if (!streamingSounds[i].stream && !freeTrack)
			{
				freeTrack = i;
			}
			else if ((entnum >= 0) && (channel != CHAN_AUTO) &&
			         (streamingSounds[i].entnum == entnum))
			{
				// found a match, override this channel
				S_StopStreamingSound(i);
				ss = &streamingSounds[i];
				break;
			}
		}
	}

	// Normal streaming sounds, select a free track
	if (!ss && freeTrack)
	{
		ss = &streamingSounds[freeTrack];
		if (s_debugStreams->integer)
		{
			Com_Printf("S_StartStreamingSoundEx: Free slot found, %d\n", freeTrack);
		}
	}

	// cannot find a free track, bail...
	if (!ss)
	{
		//if (!s_mute->integer)
		{
			Com_Printf("S_StartStreamingSoundEx: No free streaming tracks\n");
		}
		return 0.0f;
	}

	// Set the name of the track
	if ( intro && *intro )
	{
		Q_strncpyz(ss->name, intro, sizeof(ss->name));
	}
	else
	{
		ss->name[0] = '\0';
	}

	// looping track passed to stream
	if (loop)
	{
		// if we don't hit the special case in music, "onetimeonly",
		// copy the new loop stream over
		if (!music || Q_stricmp(loop, "onetimeonly") != 0)
		{
			Q_strncpyz(ss->loopStream, loop, sizeof(ss->loopStream));
		}
	}
	else
	{
		ss->loopStream[0] = '\0';
	}

	// clear current music cvar
	Cvar_Set("s_currentMusic", "");

	// set the stream parameters
	ss->channel = channel;
	// not music track, parameter is attenuation
	if (!music)
	{
		ss->attenuation = param;
	}
	ss->entnum       = entnum;
	ss->fadeStartVol = 0;
	// music track, positive parameter is fadeUpTime
	if (music && param > 0)
	{
		ss->fadeStart     = s_soundtime;
		ss->fadeEnd       = ss->fadeStart + (((float)(ss->stream->info.rate) / 1000.0f) * param);
		ss->fadeTargetVol = 1.0f;
	}
	else
	{
		ss->fadeStart     = 0;
		ss->fadeEnd       = 0;
		ss->fadeTargetVol = 0;
	}

	// close the stream
	if (ss->stream)
	{
		S_CodecCloseStream(ss->stream);
	}
	// Open stream
	ss->stream = (intro && *intro) ? S_CodecOpenStream(intro) : NULL;

	if (!ss->stream)
	{
		Com_Printf(S_COLOR_YELLOW "WARNING S_StartStreamingSoundEx: couldn't open stream file %s\n", ss->name);
		return 0.0f;
	}

	if (ss->stream->info.channels != 2 || ss->stream->info.rate != 22050)
	{
		Com_DPrintf(S_COLOR_YELLOW "WARNING S_StartStreamingSoundEx: stream file %s is not 22050k [%i] stereo [%i]\n", ss->name, ss->stream->info.rate, ss->stream->info.channels);
	}

	// return the length of sound
	return (ss->stream->info.samples / (float)ss->stream->info.rate) * 1000.0f;
}

/**
 * @brief S_Base_StartStreamingSound
 * @param[in] intro
 * @param[in] loop
 * @param[in] entnum
 * @param[in] channel
 * @param[in] attenuation
 * @return
 */
float S_Base_StartStreamingSound(const char *intro, const char *loop, int entnum, int channel, int attenuation)
{
	return S_StartStreamingSoundEx(intro, loop, entnum, channel, qfalse, attenuation);
}

/**
 * @brief Frees a streaming sound so that it can be used again, but does not terminate the sound.
 * @param stream
 */
void S_FreeStreamingSound(int stream)
{
	streamingSound_t *ss;

	if (stream < 0 || stream >= MAX_STREAMING_SOUNDS)
	{
		return;
	}

	ss = &streamingSounds[stream];

	if (!ss->stream)
	{
		return;
	}

	S_CodecCloseStream(ss->stream);
	ss->stream          = NULL;
	ss->loopStream[0]   = '\0';
	ss->queueStream[0]  = '\0';
	ss->queueStreamType = 0;
}

/**
 * @brief Stops a streaming sound completely in its tracks.
 * @param stream
 */
void S_StopStreamingSound(int stream)
{
	S_FreeStreamingSound(stream);
	s_rawend[RAW_STREAM(stream)] = 0;
}

/**
 * @brief S_Base_StopEntStreamingSound
 * @param[in] entnum
 */
void S_Base_StopEntStreamingSound(int entnum)
{
	int i;

	for (i = 1; i < MAX_STREAMING_SOUNDS; i++)
	{
		// is the stream active
		if (!streamingSounds[i].stream)
		{
			continue;
		}
		// is it the right entity or is it all
		if (streamingSounds[i].entnum != entnum && entnum != -1)
		{
			continue;
		}
		S_StopStreamingSound(i);
	}
}

/**
 * @brief S_Base_FadeAllSounds
 * @param[in] targetVol
 * @param[in] time
 * @param[in] stopsounds
 */
void S_Base_FadeAllSounds(float targetVol, int time, qboolean stopsounds)
{
	// Because of strange timing issues, sometimes we try to fade up before the fade down completed
	// If that's the case, just force an immediate stop to all sounds
	if (s_soundtime < s_volTime2 && s_stopSounds)
	{
		S_StopAllSounds();
	}

	s_volStart  = s_volCurrent;
	s_volTarget = targetVol;

	s_volTime1 = s_soundtime;
	s_volTime2 = s_soundtime + (((float)(dma.speed) / 1000.0f) * time);

	s_stopSounds = stopsounds;

	// instant
	if (!time)
	{
		s_volTarget = s_volStart = s_volCurrent = targetVol;  // set it
		s_volTime1  = s_volTime2 = 0; // no fading
	}
}

/**
 * @brief S_Base_FadeStreamingSound
 * @param[in] targetVol
 * @param[in] time
 * @param[in] stream
 */
void S_Base_FadeStreamingSound(float targetVol, int time, int stream)
{
	streamingSound_t *ss;

	if (stream < 0 || stream >= MAX_STREAMING_SOUNDS)
	{
		return;
	}

	ss = &streamingSounds[stream];

	if (!ss->stream)
	{
		return;
	}

	ss->fadeStartVol = 1.0f;

	if (stream == 0)
	{
		if (s_debugStreams->integer)
		{
			Com_Printf("S_Base_FadeStreamingSound: Fade: %0.2f %d\n", (double)targetVol, time);
		}
	}

	// get current fraction if already fading
	if (ss->fadeStart)
	{
		ss->fadeStartVol = (ss->fadeEnd <= s_soundtime) ? ss->fadeTargetVol :
		                   ((float)(s_soundtime - ss->fadeStart) / (float)(ss->fadeEnd - ss->fadeStart));
	}

	ss->fadeStart     = s_soundtime;
	ss->fadeEnd       = s_soundtime + (int)((ss->stream->info.rate / 1000.0f) * time);
	ss->fadeTargetVol = targetVol;
}

/**
 * @brief S_GetStreamingFade
 * @param[in] ss
 * @return
 */
float S_GetStreamingFade(streamingSound_t *ss)
{
	float oldfrac, newfrac;

	// no fading, use full volume
	if (!ss->fadeStart)
	{
		return 1.0f;
	}

	if (ss->fadeEnd <= s_soundtime)        // it's hit it's target
	{
		return ss->fadeTargetVol;
	}

	newfrac = (float)(s_soundtime - ss->fadeStart) / (float)(ss->fadeEnd - ss->fadeStart);
	oldfrac = 1.0f - newfrac;

	return (oldfrac * ss->fadeStartVol) + (newfrac * ss->fadeTargetVol);
}

/**
 * @brief S_Base_StopBackgroundTrack
 */
static void S_Base_StopBackgroundTrack(void)
{
	S_StopStreamingSound(0);
}

/**
 * @brief S_Base_StartBackgroundTrack
 * @param[in] intro
 * @param[in] loop
 * @param[in] fadeupTime
 */
static void S_Base_StartBackgroundTrack(const char *intro, const char *loop, int fadeupTime)
{
	S_StartStreamingSoundEx(intro, loop, -1, -1, qtrue, fadeupTime);
}

/**
 * @brief S_UpdateStreamingSounds
 */
void S_UpdateStreamingSounds(void)
{
	int              bufferSamples;
	int              fileSamples;
	byte             raw[30000]; // just enough to fit in a mac stack frame
	int              fileBytes;
	int              rs;
	int              i, j;
	float            lvol, rvol;
	float            streamingVol;
	streamingSound_t *ss;

	for (i = 0; i < MAX_STREAMING_SOUNDS; i++)
	{
		ss = &streamingSounds[i];
		if (!ss->stream)
		{
			continue;
		}
		// don't bother playing anything if musicvolume is 0
		if (i == 0 && s_musicVolume->value == 0.0f)
		{
			continue;
		}
		// get the raw stream index
		j = RAW_STREAM(i);

		// see how many samples should be copied into the raw buffer
		if (s_rawend[j] - s_soundtime < 0)
		{
			s_rawend[j] = s_soundtime;
		}

		while (s_rawend[j] - s_soundtime < MAX_RAW_SAMPLES)
		{
			bufferSamples = MAX_RAW_SAMPLES - (s_rawend[j] - s_soundtime);

			// decide how much data needs to be read from the file
			fileSamples = bufferSamples * ss->stream->info.rate / dma.speed;

			if ( fileSamples == 0 ) {
				break;
			}

			// our max buffer size
			fileBytes = fileSamples * (ss->stream->info.width * ss->stream->info.channels);
			if (fileBytes > sizeof(raw))
			{
				fileBytes   = sizeof(raw);
				fileSamples = fileBytes / (ss->stream->info.width * ss->stream->info.channels);
			}

			// read stream
			rs = S_CodecReadStream(ss->stream, fileBytes, raw);
			if (rs < fileBytes)
			{
				fileSamples = rs / (ss->stream->info.width * ss->stream->info.channels);
			}

			// calculate the streaming volume based on stream fade and global fade
			streamingVol = S_GetStreamingFade(ss);
			// stop the stream if we had faded out of existence
			if (streamingVol == 0.0f)
			{
				S_StopStreamingSound(i);
				break;
			}
			streamingVol *= s_volCurrent;

			if (rs > 0)
			{
				if (i == 0)
				{
					lvol = rvol = s_musicVolume->value * streamingVol;
				}
				else
				{
					// attenuate if required
					if (ss->entnum >= 0 && ss->attenuation)
					{
						int r, l;

						S_SpatializeOrigin(entityPositions[ss->entnum], s_volume->value * 255.0f, &l, &r, SOUND_RANGE_DEFAULT, qfalse);
						if ((lvol = ((float)l / 255.0f)) > 1.0f)
						{
							lvol = 1.0f;
						}
						if ((rvol = ((float)r / 255.0f)) > 1.0f)
						{
							rvol = 1.0f;
						}
						lvol *= streamingVol;
						rvol *= streamingVol;
					}
					else
					{
						lvol = rvol = streamingVol;
					}
				}
				// add to raw buffer
				S_Base_RawSamples(j, fileSamples, ss->stream->info.rate,
				                  ss->stream->info.width,
				                  ss->stream->info.channels,
				                  raw, lvol, rvol, ss->entnum >= 0 ? ss->entnum : -1);
			}
			else
			{
				if (s_debugStreams->integer)
				{
					Com_Printf("STREAM %d: ending...\n", i);
					Com_Printf("Queue stream: %s\nLoopStream: %s\n", ss->queueStream, ss->loopStream);
				}
				// special case for music track queue
				if (i == 0 && ss->queueStreamType && *ss->queueStream)
				{
					switch (ss->queueStreamType)
					{
					case QUEUED_PLAY_ONCE_SILENT:
						break;
					case QUEUED_PLAY_ONCE:
						S_StartBackgroundTrack(ss->queueStream, ss->name, 0);
						break;
					case QUEUED_PLAY_LOOPED:
						S_StartBackgroundTrack(ss->queueStream, ss->queueStream, 0);
						break;
					}
					// queue is done, clear it
					ss->queueStream[0]  = '\0';
					ss->queueStreamType = 0;
					break;
				}

				// loop
				if ( ss->loopStream[0] != '\0' )
				{
					// TODO: Implement a rewind?
					char loopStream[MAX_QPATH];

					Q_strncpyz(loopStream, ss->loopStream, MAX_QPATH);
					S_StartStreamingSoundEx(loopStream, loopStream,
					                        ss->entnum, ss->channel, i == 0, i == 0 ? 0 : ss->attenuation);
				}
				else
				{
					S_FreeStreamingSound(i);
				}
				break;
			}
		}
	}
}

/*
======================
S_GetVoiceAmplitude
======================
*/
static int S_Base_GetVoiceAmplitude( int entityNum ) {
	// FIXME: Stub
	return 0;
}


/*
======================
S_FreeOldestSound
======================
*/
qboolean S_FreeOldestSound( void ) {
	int	i, oldest, used;
	sfx_t	*sfx;
	sndBuffer	*buffer, *nbuffer;

	// all sounds may be loaded with (s_soundtime + 1) at this moment
	// so we need to trigger match condition at least once
	oldest = s_soundtime + 2; // Com_Milliseconds();

	used = 0;

	for ( i = 1 ; i < s_numSfx ; i++ ) {
		sfx = &s_knownSfx[i];
		if ( sfx->inMemory && sfx->lastTimeUsed - oldest < 0 ) {
			used = i;
			oldest = sfx->lastTimeUsed;
		}
	}

	if ( used == 0 )
		return qfalse;

	sfx = &s_knownSfx[used];

	Com_DPrintf("S_FreeOldestSound: freeing sound %s\n", sfx->soundName);

	buffer = sfx->soundData;
	while(buffer != NULL) {
		nbuffer = buffer->next;
		SND_free(buffer);
		buffer = nbuffer;
	}
	sfx->inMemory = qfalse;
	sfx->soundData = NULL;

	return qtrue;
}

// START	xkan, 9/23/2002
// returns how long the sound lasts in milliseconds
static int S_Base_GetSoundLength( sfxHandle_t sfxHandle ) {
	if ( !s_soundStarted || !sfxHandle ) {
		return 0;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_DPrintf( S_COLOR_YELLOW "S_GetSoundLength: handle %i out of range\n", sfxHandle );
		return -1;
	}
	return (int)( (float)s_knownSfx[ sfxHandle ].soundLength / dma.speed * 1000.0 );
}
// END		xkan, 9/23/2002

// ydnar: for looped sound synchronization
static int S_Base_GetCurrentSoundTime( void ) {
	if ( !s_soundStarted ) {
		return 0;
	}

	return s_soundtime + dma.speed;
//	 return s_paintedtime;
}

// =======================================================================
// Shutdown sound engine
// =======================================================================

static void S_Base_Shutdown( void ) {

	if ( !s_soundStarted ) {
		return;
	}

	SNDDMA_Shutdown();

	// release sound buffers only when switching to dedicated 
	// to avoid redundand reallocation at client restart
	if ( com_dedicated->integer )
		SND_shutdown();

	s_soundStarted = qfalse;

	if ( dma_buffer2 != buffer2 )
		free( dma_buffer2 );
	dma_buffer2 = NULL;

	cls.soundRegistered = qfalse;
}


/*
================
S_Init
================
*/
qboolean S_Base_Init( soundInterface_t *si ) {
	qboolean	r;

	if (!si)
	{
		Com_Printf("Invalid sound interface NULL.\n");
		return qfalse;
	}

	s_khz = Cvar_Get( "s_khz", "44", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_khz, "0", "48", CV_INTEGER );
	Cvar_SetDescription( s_khz, "Specifies the sound sampling rate, (11, 22, 44, 48) in kHz" );

	switch( s_khz->integer ) {
		case 48:
		case 44:
		case 22:
		case 11:
			// these are legal values
			break;
		default:
			// anything else is illegal
			Com_Printf( "WARNING: cvar 's_khz' must be one of (11, 22, 44, 48), setting to '%s'\n", s_khz->resetString );
			Cvar_ForceReset( "s_khz" );
			break;
	}

	Cvar_Get( "s_currentMusic", "", CVAR_ROM );

	s_mixahead = Cvar_Get( "s_mixAhead", "0.2", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixahead, "0.001", "0.5", CV_FLOAT );
	Cvar_SetDescription( s_mixahead, "Amount of time to pre-mix sound data to avoid potential skips/stuttering in case of unstable framerate. Higher values add more CPU usage" );

	s_mixOffset = Cvar_Get( "s_mixOffset", "0", CVAR_ARCHIVE_ND | CVAR_DEVELOPER );
	Cvar_CheckRange( s_mixOffset, "0", "0.5", CV_FLOAT );

	s_show = Cvar_Get( "s_show", "0", CVAR_CHEAT );
	Cvar_SetDescription( s_show, "Debugging output (used sound files)" );
	s_testsound = Cvar_Get( "s_testsound", "0", CVAR_CHEAT );
	Cvar_SetDescription( s_testsound, "Debugging tool that plays a simple sine wave tone to test the sound system" );
#if defined(__linux__) && !defined(USE_SDL)
	s_device = Cvar_Get( "s_device", "default", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_device, "Set ALSA output device\n"
		" Use \"default\", \"sysdefault\", \"front\", etc.\n"
		" Enter " S_COLOR_CYAN "aplay -L "S_COLOR_WHITE"in your shell to see all options.\n"
		S_COLOR_YELLOW " Please note that only mono/stereo devices are acceptable." );
#elif defined(__linux__) && defined(USE_SDL)
	s_device = Cvar_Get( "s_device", "default", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_device, "Set SDL audio output device index\n"
		"Use \"default\" to let system pick or choose one from \\s_devlist output." );
#elif (defined(__APPLE__) || defined(__APPLE_CC__)) && defined(USE_SDL)
	s_device = Cvar_Get( "s_device", "default", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_device, "Set SDL audio output device index\n"
		"Use \"default\" to let system pick or choose one from \\s_devlist output." );
#endif

#if defined(USE_SDL)
	s_sdlDriver = Cvar_Get( "s_sdlDriver", "", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_UNSAFE );
	Cvar_SetDescription( s_sdlDriver, "Set SDL audio driver, e.g. \"pulseaudio\" or \"pipewire\"" );
#endif

	s_debugStreams = Cvar_Get("s_debugStreams", "0", CVAR_TEMP);

	// fretn
	s_bits = Cvar_Get( "s_bits", "16", CVAR_LATCH | CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_bits, "8", "16", CV_INTEGER );
	Cvar_SetDescription( s_bits, "Bits per-sample to request for audio output (possible options: 8 or 16). When set to 0 it uses 16" );
	s_numchannels = Cvar_Get( "s_channels", "2", CVAR_LATCH | CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_numchannels, "1", "2", CV_INTEGER );
	Cvar_SetDescription( s_numchannels, "Number of audio channels to request for audio output. The Enemy Territory audio mixer only supports mono and stereo. Additional channels are silent" );

	r = SNDDMA_Init();

	if ( r ) {
		s_soundStarted = qtrue;
		s_soundMuted = qtrue;
		s_numSfx = 0;

		Com_Memset( streamingSounds, 0, sizeof(streamingSounds) );
		Com_Memset( sfxHash, 0, sizeof( sfxHash ) );

		s_soundtime = 0;
		s_paintedtime = 0;

		S_Base_StopAllSounds();

		// setup(likely) or allocate (unlikely) buffer for muted painting
		if ( dma.samples * dma.samplebits/8 <= sizeof( buffer2 ) ) {
			dma_buffer2 = buffer2;
		} else {
			dma_buffer2 = malloc( dma.samples * dma.samplebits/8 );
			memset( dma_buffer2, 0, dma.samples * dma.samplebits/8 );
		}
	} else {
		return qfalse;
	}

	si->Shutdown              = S_Base_Shutdown;
	si->Reload                = S_Base_Reload;
	si->StartSound            = S_Base_StartSound;
	si->StartSoundEx          = S_Base_StartSoundEx;
	si->StartLocalSound       = S_Base_StartLocalSound;
	si->StartBackgroundTrack  = S_Base_StartBackgroundTrack;
	si->StopBackgroundTrack   = S_Base_StopBackgroundTrack;
	si->StartStreamingSound   = S_Base_StartStreamingSound;
	si->StopEntStreamingSound = S_Base_StopEntStreamingSound;
	si->FadeStreamingSound    = S_Base_FadeStreamingSound;
	si->RawSamples            = S_Base_RawSamples;
	si->ClearSounds           = S_Base_ClearSounds;
	si->StopAllSounds         = S_Base_StopAllSounds;
	si->FadeAllSounds         = S_Base_FadeAllSounds;
	si->ClearLoopingSounds    = S_Base_ClearLoopingSounds;
	si->AddLoopingSound       = S_Base_AddLoopingSound;
	si->AddRealLoopingSound   = S_Base_AddRealLoopingSound;
	si->Respatialize          = S_Base_Respatialize;
	si->UpdateEntityPosition  = S_Base_UpdateEntityPosition;
	si->Update                = S_Base_Update;
	si->DisableSounds         = S_Base_DisableSounds;
	si->BeginRegistration     = S_Base_BeginRegistration;
	si->RegisterSound         = S_Base_RegisterSound;
	si->ClearSoundBuffer      = S_Base_ClearSoundBuffer;
	si->SoundInfo             = S_Base_SoundInfo;
	si->SoundList             = S_Base_SoundList;
	si->GetVoiceAmplitude     = S_Base_GetVoiceAmplitude;
	si->GetSoundLength        = S_Base_GetSoundLength;
	si->GetCurrentSoundTime   = S_Base_GetCurrentSoundTime;

#ifdef USE_VOIP
	si->StartCapture            = S_Base_StartCapture;
	si->AvailableCaptureSamples = S_Base_AvailableCaptureSamples;
	si->Capture                 = S_Base_Capture;
	si->StopCapture             = S_Base_StopCapture;
	si->MasterGain              = S_Base_MasterGain;
#endif

	return qtrue;
}
