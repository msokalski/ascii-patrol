#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <pthread.h>

#include "spec.h"

// pollution from <pulse/pulseaudio.h>
#ifdef MIN
#undef MIN
#endif

// pollution from <pulse/pulseaudio.h>
#ifdef MAX
#undef MAX
#endif

static pa_threaded_mainloop* audio_api = 0;
static pa_context* audio_context = 0;
static pa_stream* audio_stream = 0;

static pthread_mutex_t audio_cs = PTHREAD_MUTEX_INITIALIZER;
static volatile int audio_error = 0;

// silence until conf fetch!
static int mo3_gain = 0; //100;
static int sfx_gain = 0; //100;

struct VOICE
{
	unsigned int id;

	int vol; // global vol at the play time

	int gain;
	int pan;
	bool loop;

	int pos;

	unsigned int fade_start;
	unsigned int fade_len;

	VOICE* next;
};

struct SAMPLE
{
	short* data;
	int size;
};

static SAMPLE sfx_buf[256]={0};
static VOICE* sfx_voice = 0;
static VOICE* sfx_fade = 0;
static pthread_mutex_t sfx_cs = PTHREAD_MUTEX_INITIALIZER;

static bool sfx_mix(VOICE* v, short* buf, int samples, int gain)
{
	int size = sfx_buf[v->id].size;
	short* data = sfx_buf[v->id].data;
	int vol = v->gain*v->vol*gain/10000;

	if (v->loop)
	{
		for (int i=0; i<samples; i++)
		{
			// todo: SATURATE to +/- 32767 !!!
			buf[2*i+0] += vol*data[v->pos]/100;
			buf[2*i+1] += vol*data[v->pos]/100;

			v->pos++;
			if (v->pos>=size)
				v->pos-=size;
		}
		return true;
	}

	for (int i=0; i<samples; i++)
	{
		// todo: SATURATE to +/- 32767 !!!
		buf[2*i+0] += vol*data[v->pos]/100;
		buf[2*i+1] += vol*data[v->pos]/100;

		v->pos++;
		if (v->pos>=size)
			return false;
	}

	return true;
}

static void audio_stream_write_cb(pa_stream *p, size_t nbytes, void *user)
{
	const int nch=2;
	const int frq=44100;
	const int smp=frq/4; // 0.25s buf

	short pull[nch*smp];

	int samples = nbytes / 4;

	pthread_mutex_lock(&sfx_cs);
	int mo3_vol = mo3_gain;

	unsigned int fade = get_time();

	while (samples)
	{
		int s = samples;
		if (s>smp)
			s=smp;

		pull_sound(nch,frq,pull,s);

		// attenuate
		for (int i=0; i<2*s; i++)
			pull[i] = pull[i] * mo3_vol / 100;

		// mix-in sfx!
		VOICE** p = &sfx_voice;
		VOICE* v = sfx_voice;
		while (v)
		{
			VOICE* n = v->next;
			if (sfx_mix(v,pull,s,100))
			{
				p=&v->next;
			}
			else
			{
				*p=n;
				delete v;
			}
			v=n;
		}

		p=&sfx_fade;
		v = sfx_fade;
		while (v)
		{
			VOICE* n = v->next;
			// calc fade val
			unsigned int dt = fade - v->fade_start;
			if (dt>=v->fade_len)
			{
				*p=n;
				delete v;
			}
			else
			{
				int gain = v->gain*v->vol/100 - dt*100 / v->fade_len;
				if (sfx_mix(v,pull,s,gain))
				{
					p=&v->next;
				}
				else
				{
					*p=n;
					delete v;
				}
			}

			v=n;
		}

		// pa_stream_begin_write();
		pa_stream_write(audio_stream, pull, s*nch*sizeof(short), 0/*free_cb*/, 0/*offset*/, PA_SEEK_RELATIVE);

		samples-=s;
	}

	pthread_mutex_unlock(&sfx_cs);
}

static void audio_stream_state_cb(pa_stream *s, void *user)
{
	switch(pa_stream_get_state(audio_stream))
	{
		case PA_STREAM_CREATING:
		{
			printf("Pulse: Stream creating...\n");
			break;
		}

		case PA_STREAM_READY:
		{
			printf("Pulse: Stream ready now\n");

			audio_error=0;
			pthread_mutex_unlock(&audio_cs);
			break;
		}

		case PA_STREAM_UNCONNECTED:
		case PA_STREAM_TERMINATED:
		case PA_STREAM_FAILED:
		{
			printf("Pulse: Stream connection failed\n");

			pa_stream_unref(audio_stream);
			audio_stream = 0;

			audio_error=-6;
			pthread_mutex_unlock(&audio_cs);
			break;
		}
	}
}

static void audio_init_stream()
{
	int error;
	pa_sample_spec ss;
	pa_channel_map map;

	ss.format = PA_SAMPLE_S16LE;
	ss.channels = 2;
	ss.rate = 44100;

	/* init channel map */
	pa_channel_map_init_stereo(&map);

	/* check sample spec */
	if(!pa_sample_spec_valid(&ss))
	{
		audio_error = -2;
		pthread_mutex_unlock(&audio_cs);
		return;
	}

	/* create the stream */
	audio_stream = pa_stream_new(audio_context, "ascii-patrol", &ss, &map);
	if(!audio_stream)
	{
		printf("Pulse: Error creating stream.\n");
		audio_error = -3;
		pthread_mutex_unlock(&audio_cs);
		return;
	}

	pa_stream_set_write_callback(audio_stream, audio_stream_write_cb, 0/*user*/);
	pa_stream_set_state_callback(audio_stream, audio_stream_state_cb, 0/*user*/);

	pa_buffer_attr attr={0};
	attr.fragsize = (uint32_t)-1;
	attr.tlength = 44100*2*sizeof(short) * 20/1000;
	attr.maxlength = attr.tlength;
	attr.minreq = 0;
	attr.prebuf = (uint32_t)-1;

	error = pa_stream_connect_playback(audio_stream, 0, &attr,
			pa_stream_flags(
				PA_STREAM_AUTO_TIMING_UPDATE |
				PA_STREAM_ADJUST_LATENCY |
				PA_STREAM_INTERPOLATE_TIMING |
			0),
			0/*vol*/, 0);

	if(error < 0)
	{
		pa_stream_unref(audio_stream);
		audio_stream = 0;

		printf("Pulse: Failed connecting stream.\n");
		audio_error = -4;
		pthread_mutex_unlock(&audio_cs);
		return;
	}
}

static void audio_context_state_cb(pa_context* c, void* user)
{
	switch (pa_context_get_state(c))
	{
		case PA_CONTEXT_CONNECTING:
		{
			printf("Pulse: Context connecting...\n");
			break;
		}
		case PA_CONTEXT_AUTHORIZING:
		{
			printf("Pulse: Context authorizing...\n");
			break;
		}
		case PA_CONTEXT_SETTING_NAME:
		{
			printf("Pulse: Context setting name...\n");
			break;
		}
		case PA_CONTEXT_READY:
		{
			printf("Pulse: Context ready now.\n");
			audio_init_stream();
			break;
		}
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
		{
			printf("Pulse: Connection failed.\n");
			audio_error = -1;
			pthread_mutex_unlock(&audio_cs);
			break;
		}
	}
}

void init_sound()
{
	audio_api = pa_threaded_mainloop_new();
	if (!audio_api)
		return;

	pa_proplist *plist = pa_proplist_new();

	pa_proplist_sets(plist, PA_PROP_APPLICATION_NAME, "ascii-patrol");
	pa_proplist_sets(plist, PA_PROP_APPLICATION_VERSION, "alpha 2.0");
	pa_proplist_sets(plist, PA_PROP_MEDIA_ROLE, "game");

	audio_context = pa_context_new_with_proplist( pa_threaded_mainloop_get_api(audio_api), "ascii-patrol", plist );

	pa_proplist_free(plist);

	if (!audio_context)
	{
		pa_threaded_mainloop_free(audio_api);
		audio_api = 0;
		return;
	}

	pa_context_set_state_callback(audio_context, audio_context_state_cb, 0/*user*/);

	int error = pa_context_connect(audio_context, NULL, (pa_context_flags_t)0, NULL);

	if(error < 0)
	{
		// no demon, excellent
		// player won't be loaded unnecessarily
		pa_context_unref(audio_context);
		audio_context = 0;
		pa_threaded_mainloop_free(audio_api);
		audio_api = 0;
		return;
	}

	pthread_mutex_lock(&audio_cs);

	// !!!
	load_player();

	audio_error = 1; // pending
	error = pa_threaded_mainloop_start(audio_api);

	if(error < 0)
	{
		pa_context_unref(audio_context);
		audio_context = 0;
		pa_threaded_mainloop_free(audio_api);
		audio_api = 0;

		audio_error = -5;
		pthread_mutex_unlock(&audio_cs);
	}
	else
	{
		// wait here for async error or success
		pthread_mutex_lock(&audio_cs);
		pthread_mutex_unlock(&audio_cs);

		if (audio_error < 0)
		{
			pa_threaded_mainloop_stop(audio_api);

			pa_context_unref(audio_context);
			audio_context = 0;

			pa_threaded_mainloop_free(audio_api);
			audio_api = 0;
		}
		else
		{
			// FULL SUCCESS

			// now, let's try to upload our samples

			unsigned int id[96];
			int count = get_sfx_ids(id);

			for (int i=0; i<count; i++)
			{
				int size=0;
				short* data = get_sfx_sample(id[i],&size);
				sfx_buf[ id[i] ].data = data;
				sfx_buf[ id[i] ].size = size;
			}
		}
	}
}

void free_sound()
{
	if (audio_api)
	{
		pa_context_disconnect(audio_context);
		pa_threaded_mainloop_stop(audio_api);

		pa_context_unref(audio_context);
		audio_context = 0;

		pa_threaded_mainloop_free(audio_api);
		audio_api = 0;

		//pthread_mutex_lock(&sfx_cs);
		stop_sfx(-1);
		sfx_voice = sfx_fade;
		stop_sfx(-1);
		//pthread_mutex_unlock(&sfx_cs);

	}
}

void set_gain(int mo3, int sfx)
{
	// todo:
	// we should use interlocked swaps

	if (mo3>=0)
	{
		mo3_gain = mo3;
	}
	if (sfx>=0)
	{
		sfx_gain = sfx;
	}
}

void lock_player()
{
	pthread_mutex_lock(&audio_cs);
}

void unlock_player()
{
	pthread_mutex_unlock(&audio_cs);
}


bool play_sfx(unsigned int id, void** voice, bool loop, int vol, int pan)
{
	if (!audio_context)
		return false;

	pthread_mutex_lock(&sfx_cs);

	if (sfx_buf[id].data)
	{
		VOICE* v = new VOICE;

		v->id = id; // data & size is @ sfx_buf[v->id]
		v->next = sfx_voice;

		v->vol  = sfx_gain;
		v->gain = vol;
		v->pan  = 0;
		v->loop = loop;
		v->pos  = 0;

		sfx_voice = v;

		if (voice)
			*voice = v;

		pthread_mutex_unlock(&sfx_cs);
		return true;
	}

	pthread_mutex_unlock(&sfx_cs);
	return false;
}

bool stop_sfx(int fade)
{
	pthread_mutex_lock(&sfx_cs);
	if (fade<=0)
	{
		// just kill all
		VOICE* v = sfx_voice;
		while (v)
		{
			VOICE* n = v->next;
			delete v;
			v=n;
		}
		sfx_voice = 0;

		// kill all current fades
		v = sfx_fade;
		while (v)
		{
			VOICE* n = v->next;
			delete v;
			v=n;
		}
		sfx_fade = 0;
	}
	else
	{
		unsigned int fade_begin = get_time();
		// append all to fade list and assign timers

		VOICE* v = sfx_fade;
		unsigned int end = fade_begin + fade;
		while (v)
		{
			unsigned int e = v->fade_start + v->fade_len;
			if ( (int)(e-end) > 0 )
			{
				int delta = (fade*v->fade_len)/(fade_begin - v->fade_start) - fade;
				v->fade_start = fade_begin - delta;
				v->fade_len = fade + delta;
			}

			v=v->next;
		}

		v = sfx_voice;
		while (v)
		{
			VOICE* n = v->next;

			v->fade_start = fade_begin;
			v->fade_len = fade;

			v->next = sfx_fade;
			sfx_fade = v;

			v=n;
		}
		sfx_voice = 0;
	}

	pthread_mutex_unlock(&sfx_cs);
	return true;
}

bool stop_sfx(void* voice, int fade)
{
	pthread_mutex_lock(&sfx_cs);
	if (fade<=0)
	{
		VOICE** p = &sfx_voice;
		VOICE* v = sfx_voice;

		while (v)
		{
			VOICE* n = v->next;

			if (voice == (void*)v)
			{
				*p = n;
				delete v;

				pthread_mutex_unlock(&sfx_cs);
				return true;
			}

			p=&v->next;
			v=n;
		}
	}
	else
	{
		unsigned int fade_begin = get_time();
		// append all to fade list and assign timers

		VOICE** p = &sfx_voice;
		VOICE* v = sfx_voice;

		while (v)
		{
			VOICE* n = v->next;

			if (voice == (void*)v)
			{
				*p = n;
				break;
			}

			p=&v->next;
			v=n;
		}

		if (v)
		{
			v->fade_start = fade_begin;
			v->fade_len = fade;
			v->next = sfx_fade;
			sfx_fade = v;

			pthread_mutex_unlock(&sfx_cs);
			return true;
		}
	}

	pthread_mutex_unlock(&sfx_cs);
	return false;
}

bool set_sfx_params(void* voice, int vol, int pan)
{
	pthread_mutex_lock(&sfx_cs);

	VOICE* v = sfx_voice;

	while (v)
	{
		if (voice == (void*)v)
		{
			v->gain = vol;
			v->pan = pan;

			pthread_mutex_unlock(&sfx_cs);

			return true;
		}

		v=v->next;
	}

	pthread_mutex_unlock(&sfx_cs);
	return false;
}
