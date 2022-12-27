#include <cstdio>
#include <cmath>

#include <dsound.h>

#include "spec.h"

#pragma comment(lib,"dsound.lib")
#pragma comment(lib,"dxguid.lib")

static IDirectSound8* idsound = 0;
static HANDLE sound_thread = 0;
HANDLE ev_sound_terminate = 0;

volatile long mo3_gain = -1;

DWORD WINAPI SoundProc(LPVOID p)
{
	FILE* f=0; //fopen("sound_click.raw","wb");


	const int nch=2;
	const int frq=44100;
	const int samples = frq/2; // max latency=0.2s
	const int bytes = sizeof(short)*nch*samples;

	short pull[nch*samples]; // intermediate linear buffer

	WAVEFORMATEX wfx;
	wfx.cbSize = sizeof(WAVEFORMATEX);
	wfx.nChannels = nch;
	wfx.nSamplesPerSec = frq;
	wfx.wBitsPerSample = 16;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nBlockAlign = wfx.nChannels*wfx.wBitsPerSample/8;
	wfx.nAvgBytesPerSec = wfx.nBlockAlign*wfx.nSamplesPerSec;

	DSBUFFERDESC bd;
	bd.dwSize = sizeof(DSBUFFERDESC);
	bd.dwFlags =
				DSBCAPS_CTRLFREQUENCY |
				DSBCAPS_CTRLPAN |
				DSBCAPS_CTRLVOLUME |
				DSBCAPS_CTRLPOSITIONNOTIFY |
				DSBCAPS_GETCURRENTPOSITION2 |
				DSBCAPS_GLOBALFOCUS;

	bd.dwBufferBytes   = bytes;
	bd.dwReserved      = 0;
	bd.lpwfxFormat     = &wfx;
	bd.guid3DAlgorithm = DS3DALG_DEFAULT;

	HRESULT hr = S_OK;

	IDirectSoundBuffer* idsb = 0;
	if ( 0 != idsound->CreateSoundBuffer(&bd,&idsb,0) )
		return 0;

	LPVOID p1,p2;
	DWORD s1,s2;

	// fill initial 0.2s prior to play
	pull_sound(nch,frq,pull,samples);
	if (f)
	{
		fwrite(pull,1,bytes,f);
	}

	if ( 0 == idsb->Lock(0,nch*sizeof(short)*samples,&p1,&s1,&p2,&s2,0) )
	{
		memcpy(p1,pull,nch*samples*2);
		idsb->Unlock(p1,s1,p2,s2);
	}
	else
	{
		idsb->Release();
		return 0;
	}

	long vol = InterlockedExchange(&mo3_gain,-1);

	if (vol>=0)
	{
		DWORD db;

		if(vol <= 0)
			db = -10000;
		else
		if(vol >= 100)
			db = 0;
		else
		{
			int coeff = -2000; // sound pressure (voltage or amplitude)
			db = (int)(coeff*log10f(100.0f/vol));
		}

		idsb->SetVolume(db);
	}

	if ( 0 != idsb->Play(0,0,DSBPLAY_LOOPING) )
	{
		idsb->Stop();
		idsb->Release();
		return 0;
	}

	DWORD written=0; // in bytes!

	// sleep quarter of buffer period ( 50ms )
	int sleep_ms = 1000 * samples / (2*frq);

	// how many bytes around playpos should we consider unsafe for writing (assume 50ms)
	int margin = 0; //(samples / 4) * sizeof(short)*nch;

	while ( WaitForSingleObject( ev_sound_terminate, sleep_ms ) == WAIT_TIMEOUT )
	{
		DWORD play_pos,write_pos;
		idsb->GetCurrentPosition(&play_pos,&write_pos);

		long vol = InterlockedExchange(&mo3_gain,-1);

		if (vol>=0)
		{
			DWORD db;

			if(vol <= 0)
			  db = -10000;
			else
			if(vol >= 100)
			  db = 0;
			else
			{
				int coeff = -2000; // sound pressure (voltage or amplitude)
				db = (int)(coeff*log10f(100.0f/vol));
			}

			idsb->SetVolume(db);
		}

		int write = (play_pos - written - margin + bytes) % bytes;

		if (write>0)
		{
			pull_sound(nch,frq,pull,write/(2*nch));
			if (f)
			{
				fwrite(pull,1,write,f);
			}

			if ( 0 == idsb->Lock(written,write,&p1,&s1,&p2,&s2,0) )
			{

				if (p1)
					memcpy(p1,pull,s1);
				if (p2)
					memcpy(p2,((char*)pull)+s1,s2);

				idsb->Unlock(p1,s1,p2,s2);

				written = (written + write) % bytes;
			}

			if (f)
			{
				fflush(f);
			}
		}
	}

	idsb->Stop();
	idsb->Release();

	return 0;
}

static CRITICAL_SECTION player_cs;

static int sfx_count=0;
static IDirectSoundBuffer* sfx_buf[96]={0};

IDirectSoundBuffer* load_sfx_buffer(short* data, int size)
{
	if (!data || size<=0)
		return 0;

	const int nch=1;
	const int frq=44100;
	const int samples = size;
	const int bytes = sizeof(short)*nch*samples;

	WAVEFORMATEX wfx;
	wfx.cbSize = sizeof(WAVEFORMATEX);
	wfx.nChannels = nch;
	wfx.nSamplesPerSec = frq;
	wfx.wBitsPerSample = 16;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nBlockAlign = wfx.nChannels*wfx.wBitsPerSample/8;
	wfx.nAvgBytesPerSec = wfx.nBlockAlign*wfx.nSamplesPerSec;

	DSBUFFERDESC bd;
	bd.dwSize = sizeof(DSBUFFERDESC);
	bd.dwFlags =
				DSBCAPS_CTRLFREQUENCY |
				DSBCAPS_CTRLPAN |
				DSBCAPS_CTRLVOLUME |
				DSBCAPS_CTRLPOSITIONNOTIFY |
				DSBCAPS_GETCURRENTPOSITION2 |
				DSBCAPS_GLOBALFOCUS;

	bd.dwBufferBytes   = bytes;
	bd.dwReserved      = 0;
	bd.lpwfxFormat     = &wfx;
	bd.guid3DAlgorithm = DS3DALG_DEFAULT;

	HRESULT hr = S_OK;

	IDirectSoundBuffer* idsb = 0;
	if ( 0 != idsound->CreateSoundBuffer(&bd,&idsb,0) )
		return 0;

	LPVOID p1,p2;
	DWORD s1,s2;

	if ( 0 == idsb->Lock(0,nch*sizeof(short)*samples,&p1,&s1,&p2,&s2,0) )
	{
		memcpy(p1,data,nch*samples*2);
		idsb->Unlock(p1,s1,p2,s2);
	}
	else
	{
		idsb->Release();
		return 0;
	}

	return idsb;
}

struct VOICE
{
	unsigned int id;

	int vol; // global vol at the play time

	int gain;
	int pan;

	DWORD fade_start;
	DWORD fade_len;

	IDirectSoundBuffer* voice;
	VOICE* next;
};

static VOICE* sfx_voice = 0;
static VOICE* sfx_fade = 0;
static CRITICAL_SECTION sfx_cs;
static UINT sfx_timer=0;
static int sfx_gain = 100;

//SFXFader

void CALLBACK SFXFader(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	DWORD fade = GetTickCount();

	EnterCriticalSection(&sfx_cs);
	VOICE** p=&sfx_fade;
	VOICE* v=sfx_fade;
	while (v)
	{
		VOICE* n = v->next;

		DWORD dt = fade - v->fade_start;

		DWORD s=0;
		v->voice->GetStatus(&s);

		if (dt>=v->fade_len || !(s & DSBSTATUS_PLAYING))
		{
			*p=n;
			v->voice->Stop();
			v->voice->Release();
			delete v;
		}
		else
		{
			float gain = v->gain*v->vol/10000.0f - (float)dt / v->fade_len;
			DWORD db;

			if(gain <= 0)
			  db = -10000;
			else
			if(gain >= 1.0f)
			  db = 0;
			else
			{
				int coeff = -2000; // sound pressure (voltage or amplitude)
				db = (int)(coeff*log10f(1.0f/gain));
			}

			v->voice->SetVolume(db);

			p=&v->next;
		}

		v=n;
	}
	LeaveCriticalSection(&sfx_cs);
}

void garbage_sfx()
{
	VOICE** p = &sfx_voice;
	VOICE* v = sfx_voice;
	while (v)
	{
		VOICE* n = v->next;

		DWORD s=0;
		v->voice->GetStatus(&s);
		if ( !(s & DSBSTATUS_PLAYING) )
		{
			v->voice->Release();
			delete v;
			*p=n;
		}
		else
			p=&v->next;

		v=n;
	}
}

bool play_sfx(unsigned int id, void** voice, bool loop, int vol, int pan)
{
	if (!idsound)
		return false;

	garbage_sfx();

	if (sfx_buf[id])
	{
		IDirectSoundBuffer* dup=0;
		idsound->DuplicateSoundBuffer(sfx_buf[id],&dup);

		if (dup)
		{
			VOICE* v = new VOICE;
			v->id = id;
			v->voice = dup;
			v->next = sfx_voice;

			v->vol  = sfx_gain;
			v->gain = vol;
			v->pan  = 0;

			float gain = v->gain*v->vol/10000.0f;
			DWORD db;

			if(gain <= 0)
			  db = -10000;
			else
			if(gain >= 1.0f)
			  db = 0;
			else
			{
				int coeff = -2000; // sound pressure (voltage or amplitude)
				db = (int)(coeff*log10f(1.0f/gain));
			}

			dup->SetVolume(db);
			dup->SetPan(v->pan);

			sfx_voice = v;

			if (loop)
				dup->Play(0,0,DSBPLAY_LOOPING);
			else
				dup->Play(0,0,0);

			if (voice)
				*voice = v;

			return true;
		}
	}

	return false;
}

bool stop_sfx(int fade) // stop all sfx
{
	garbage_sfx();

	if (fade<=0)
	{
		// just kill all
		VOICE* v = sfx_voice;
		while (v)
		{
			VOICE* n = v->next;
			v->voice->Stop();
			v->voice->Release();
			delete v;
			v=n;
		}
		sfx_voice = 0;

		// kill all current fades
		EnterCriticalSection(&sfx_cs);

		v = sfx_fade;
		while (v)
		{
			VOICE* n = v->next;
			v->voice->Stop();
			v->voice->Release();
			delete v;
			v=n;
		}
		sfx_fade = 0;

		LeaveCriticalSection(&sfx_cs);
	}
	else
	{
		DWORD fade_begin = GetTickCount();
		// append all to fade list and assign timers

		EnterCriticalSection(&sfx_cs);

		VOICE* v = sfx_fade;
		DWORD end = fade_begin + fade;
		while (v)
		{
			DWORD e = v->fade_start + v->fade_len;
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

		LeaveCriticalSection(&sfx_cs);
	}

	return true;
}

bool stop_sfx(void* voice, int fade)
{
	garbage_sfx();

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
				v->voice->Stop();
				v->voice->Release();
				delete v;
				return true;
			}

			p=&v->next;
			v=n;
		}
	}
	else
	{
		DWORD fade_begin = GetTickCount();
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
			EnterCriticalSection(&sfx_cs);
			v->fade_start = fade_begin;
			v->fade_len = fade;
			v->next = sfx_fade;
			sfx_fade = v;
			LeaveCriticalSection(&sfx_cs);
			return true;
		}
	}

	return false;
}

bool set_sfx_params(void* voice, int vol, int pan)
{
	garbage_sfx();

	VOICE* v = sfx_voice;

	while (v)
	{
		if (voice == (void*)v)
		{
			v->gain = vol;
			v->pan = pan;

			float gain = v->gain*v->vol/10000.0f;
			DWORD db;

			if(gain <= 0)
			  db = -10000;
			else
			if(gain >= 1.0f)
			  db = 0;
			else
			{
				int coeff = -2000; // sound pressure (voltage or amplitude)
				db = (int)(coeff*log10f(1.0f/gain));
			}

			v->voice->SetVolume(db);
			v->voice->SetPan(v->pan);

			return true;
		}

		v=v->next;
	}

	return false;
}

void init_sound()
{
	idsound = 0;
	sound_thread = 0;
	ev_sound_terminate = 0;

	if ( 0 == DirectSoundCreate8(&DSDEVID_DefaultPlayback,&idsound,0) )
	{
		if ( 0 != idsound->SetCooperativeLevel(GetConsoleWindow(),DSSCL_EXCLUSIVE) )
		{
			idsound->Release();
			idsound=0;
		}

		if (idsound)
		{
			load_player();

			// init SFX
			{
				unsigned int id[96];
				int count = get_sfx_ids(id);
				for (int i=0; i<count; i++)
				{
					int size=0;
					short* data = get_sfx_sample(id[i],&size);
					sfx_buf[id[i]] = load_sfx_buffer( data, size );
				}
				sfx_count = count;

				InitializeCriticalSection(&sfx_cs);

				// fadout timer (common for all voices)
				sfx_timer = timeSetEvent(10, 1, SFXFader, 0, TIME_PERIODIC | TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS);
			}

			DWORD id;
			ev_sound_terminate = CreateEvent(0,FALSE,FALSE,0);
			sound_thread = CreateThread(0,0,SoundProc,0,0,&id);

			InitializeCriticalSection(&player_cs);

			SetThreadPriority(sound_thread, THREAD_PRIORITY_TIME_CRITICAL);
		}
	}
}

void free_sound()
{
	if (sound_thread)
	{
		stop_sfx(-1);
		if (sfx_timer)
			timeKillEvent(sfx_timer);
		sfx_timer = 0;
		sfx_voice = sfx_fade;
		stop_sfx(-1);

		DeleteCriticalSection(&sfx_cs);

		SetEvent(ev_sound_terminate);
		WaitForSingleObject(sound_thread,INFINITE);
		CloseHandle(sound_thread);
		CloseHandle(ev_sound_terminate);
		ev_sound_terminate = 0;
		sound_thread = 0;

		DeleteCriticalSection(&player_cs);

		for (int i=0; i<96; i++)
		{
			if (sfx_buf[i])
			{
				sfx_buf[i]->Release();
				sfx_buf[i]=0;
			}
		}
		sfx_count=0;
	}

	if (idsound)
	{
		// idsound->Release();
		idsound=0;
	}
}

void set_gain(int mo3, int sfx)
{
	if (mo3>=0)
		InterlockedExchange(&mo3_gain,mo3);
	if (sfx>=0)
	{
		sfx_gain = sfx;
	}
}

void lock_player()
{
	if (sound_thread)
		EnterCriticalSection(&player_cs);
}

void unlock_player()
{
	if (sound_thread)
		LeaveCriticalSection(&player_cs);
}
