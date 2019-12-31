#include <audio_io.h>
#include <dlog.h>
#include <stdlib.h>

#include "platform.h"

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "ATARI800"

#if !defined(PACKAGE)
#define PACKAGE "io.github.atari800"
#endif

static audio_out_h audio_handle;

static void* buffer = NULL;
static size_t buffer_size = 0;
static void audio_callback(audio_out_h handle, size_t nbytes, void *userdata)
{
	if (buffer_size < nbytes) {
		free(buffer);
		buffer = malloc(nbytes);
		buffer_size = nbytes;
	}
	Sound_Callback(buffer, nbytes);
	audio_out_write(handle, buffer, nbytes);
}

int PLATFORM_SoundSetup(Sound_setup_t *setup)
{
	dlog_print(DLOG_INFO, LOG_TAG,
		"SoundSetup: freq=%u sample_size=%d channels=%u buffer_ms=%u buffer_frames=%u",
		setup->freq, setup->sample_size, setup->channels, setup->buffer_ms, setup->buffer_frames);

	int res = audio_out_create(
		setup->freq,
		setup->channels == 2 ? AUDIO_CHANNEL_STEREO : AUDIO_CHANNEL_MONO,
		setup->sample_size == 2 ? AUDIO_SAMPLE_TYPE_S16_LE : AUDIO_SAMPLE_TYPE_U8,
		SOUND_TYPE_MEDIA,
		&audio_handle);

	if (res != 0) {
		dlog_print(DLOG_ERROR, LOG_TAG, "SoundSetup failed %d", res);
		return FALSE;
	}

	if (setup->buffer_frames == 0)
		setup->buffer_frames = setup->freq / 50;
	setup->buffer_frames = Sound_NextPow2(setup->buffer_frames) * 8;

	res = audio_out_set_stream_cb(
		audio_handle,
		&audio_callback,
		NULL);

	if (res != 0) {
		audio_out_destroy(audio_handle);
		dlog_print(DLOG_ERROR, LOG_TAG, "SoundSetup: audio_out_set_stream_cb failed %d", res);
	}

	dlog_print(DLOG_INFO, LOG_TAG, "SoundSetup ok");

	return TRUE;
}

void PLATFORM_SoundExit(void)
{
	audio_out_destroy(audio_handle);
	dlog_print(DLOG_INFO, LOG_TAG, "SoundExit ok");
	free(buffer);
	buffer = NULL;
	buffer_size = 0;
}

void PLATFORM_SoundPause(void)
{
	audio_out_unprepare(audio_handle);
}

void PLATFORM_SoundContinue(void)
{
	audio_out_prepare(audio_handle);
}

void PLATFORM_SoundLock(void)
{
}

void PLATFORM_SoundUnlock(void)
{
}
