
//#include <emscripten.h>

//#include <windows.h>
#define OutputDebugStringA(a) (void)(a)

#include <memory.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "unmo3.h"
#include "spec.h"

extern unsigned char mo3[];

typedef int mo3_long;
typedef unsigned int mo3_ulong;

#define READ_CTRL_BIT(n)  \
    data<<=1; \
    carry = (data >= (1<<(n))); \
    data&=((1<<(n))-1); \
    if (data==0) { \
      data = *src++; \
      data = (data<<1) + 1; \
      carry = (data >= (1<<(n))); \
      data&=((1<<(n))-1); \
    }

/* length coded within control stream :
 * most significant bit is 1
 * than the first bit of each bits pair (noted n1),
 * until second bit is 0 (noted n0)
 */
#define DECODE_CTRL_BITS { \
   strLen++; \
   do { \
     READ_CTRL_BIT(8); \
     strLen = (strLen<<1) + carry; \
     READ_CTRL_BIT(8); \
   }while(carry); \
 }

/*
 * this smart algorithm has been designed by Ian Luck
 */
unsigned char * mo3_unpack(unsigned char *src, unsigned char *dst, mo3_long size)
{
  unsigned short data;
  char carry;  // x86 carry (used to propagate the most significant bit from one byte to another)
  mo3_long strLen; // length of previous string
  mo3_long strOffset; // string offset
  unsigned char *string; // pointer to previous string
  unsigned char *initDst;
  mo3_ulong ebp, previousPtr;
  mo3_ulong initSize;

  data = 0L;
  carry = 0;
  strLen = 0;
  previousPtr = 0;

  initDst = dst;
  initSize = size;

  *dst++ = *src++; // the first byte is not compressed
  size--; // bytes left to decompress


  while (size>0) {
    READ_CTRL_BIT(8);
    if (!carry) { // a 0 ctrl bit means 'copy', not compressed byte
      *dst++ = *src++;
      size--;
    }
    else { // a 1 ctrl bit means compressed bytes are following
      ebp = 0; // lenth adjustement
      DECODE_CTRL_BITS; // read length, anf if strLen>3 (coded using more than 1 bits pair) also part of the offset value
      strLen -=3;
      if ((mo3_long)(strLen)<0) { // means LZ ptr with same previous relative LZ ptr (saved one)
        strOffset = previousPtr;  // restore previous Ptr
        strLen++;
      }
      else { // LZ ptr in ctrl stream
         strOffset = (strLen<<8) | *src++; // read less significant offset byte from stream
         strLen = 0;
         strOffset = ~strOffset;
         if (strOffset<-1280)
           ebp++;
         ebp++; // length is always at least 1
         if (strOffset<-32000)
           ebp++;
         previousPtr = strOffset; // save current Ptr
      }

      // read the next 2 bits as part of strLen
      READ_CTRL_BIT(8);
      strLen = (strLen<<1) + carry;
      READ_CTRL_BIT(8);
      strLen = (strLen<<1) + carry;
      if (strLen==0) { // length do not fit in 2 bits
        DECODE_CTRL_BITS; // decode length : 1 is the most significant bit,
        strLen+=2;  // then first bit of each bits pairs (noted n1), until n0.
      }
      strLen = strLen + ebp; // length adjustement
      if (size>=strLen) {
        string = dst + strOffset; // pointer to previous string
        if(string < initDst || string >= dst)
          break;
        size-=strLen;
        for(; strLen>0; strLen--)
           *dst++ = *string++; // copies it
      }
      else {
        // malformed stream
        break;
      }
    } // compressed bytes

  }


  if ( (dst-initDst)!=initSize ) 
  {
  }

  return src;

}

int stb_vorbis_decode_memory(const unsigned char *mem, int len, int *channels, int *sample_rate, short **output);

unsigned short GET_WORD(void* p)
{
	return ((unsigned char*)p)[0] | (((unsigned char*)p)[1]<<8);
}

unsigned int GET_DWORD(void* p)
{
	return ((unsigned char*)p)[0] | (((unsigned char*)p)[1]<<8) | (((unsigned char*)p)[2]<<16) | (((unsigned char*)p)[3]<<24);
}

bool OpenSong(SONG* song)
{
	mo3_hdr* file_hdr = (mo3_hdr*)mo3;

	unsigned char* ptr = (unsigned char*)(file_hdr+1);
	if (file_hdr->ver<5)
		return false; // we require ver to be 5.

	unsigned char* data_hdr = (unsigned char*)malloc(file_hdr->hdrlen);

	ptr = mo3_unpack(ptr,data_hdr,file_hdr->hdrlen);

	unsigned char* hdr_ptr = data_hdr;

	char* song_name = (char*)hdr_ptr;
	hdr_ptr+=strlen(song_name)+1;

	char* it_message = (char*)hdr_ptr;
	hdr_ptr+=strlen(it_message)+1;

	mo3_nfo* nfo = (mo3_nfo*)hdr_ptr;
	hdr_ptr+=sizeof(mo3_nfo);

	unsigned char* song_seq = hdr_ptr;
	hdr_ptr += nfo->song_len;

	// voice seq (for each pattern, and each channel, number of voice data) : identical voices are detected and factorized at compression. 
	unsigned short* voice_seq = (unsigned short*)hdr_ptr;
	hdr_ptr += 2 * nfo->patterns * nfo->channels;

	// pattern length table (size=nb_patt*2) 
	unsigned short* pattern_len = (unsigned short*)hdr_ptr;
	hdr_ptr += 2*nfo->patterns;

	// voice is column in a pattern
	// because they are likely to repeat accros patterns
	// we will bind their use to each pattern channel

	// pattern is effectively VoicePtr column[channels];

	// voices are still compressed using repetitions of N type,value pairs
	// we fetch byte, upper 4 bits means repeat count, lower 4 bits means num of {type,data} pairs

	unsigned char** voice_data = (unsigned char**)malloc(sizeof(unsigned char*)*nfo->unique_voices);
	for (int i=0; i<nfo->unique_voices; i++)
	{
		// do we really need to parse'em all in order to just get head ptr to each voice?
		// no, it is enought to get voice lengths :)

		unsigned int voice_len = GET_DWORD(hdr_ptr);

		hdr_ptr+=4;

		voice_data[i] = hdr_ptr;
		hdr_ptr += voice_len;
	}

	int sfx_instr = -1;
	mo3_instr** instr_data = (mo3_instr**)malloc(sizeof(mo3_instr*)*nfo->instruments);
	for (int i=0; i<nfo->instruments; i++)
	{
		char* instr_name = (char*)hdr_ptr;
		hdr_ptr += strlen(instr_name)+1;

		if (strcmp(instr_name,"SFX")==0)
		{
			sfx_instr = i;
		}

		if (file_hdr->ver>=5)
		{
			char* file_name = (char*)hdr_ptr;
			hdr_ptr += strlen(file_name)+1;
		}

		instr_data[i] = (mo3_instr*)hdr_ptr;
		hdr_ptr += sizeof(mo3_instr);
	}

	mo3_sample** sample_data = (mo3_sample**)malloc(sizeof(mo3_sample*)*nfo->samples);
	for (int i=0; i<nfo->samples; i++)
	{
		char* sample_name = (char*)hdr_ptr;

		hdr_ptr += strlen(sample_name)+1;

		if (file_hdr->ver>=5)
		{
			char* file_name = (char*)hdr_ptr;
			hdr_ptr += strlen(file_name)+1;
		}

		sample_data[i] = (mo3_sample*)hdr_ptr;
		hdr_ptr += sizeof(mo3_sample);

		// warning, we are overwriting unused part of decded data!
		sample_data[i]->ogg_header = i;
		if ( file_hdr->ver>=5 && (sample_data[i]->flags&0x5000) == 0x5000 ) 
		{
		  sample_data[i]->ogg_header = GET_WORD(hdr_ptr);
		  hdr_ptr+=2;
		}
	}

	// there may be some extra info between current hdr_ptr and hdr_data + file_hdr->hdrLen
	// skip? ...

	unsigned char* ogg_data = ptr;

	short* temp_out[256]={0};

	for (int i=0; i<nfo->samples; i++)
	{
		temp_out[i] = 0;
		// sample_data[i]->audio_data = 0;

		// assume everything is ogged
		if (sample_data[i]->size)
		{
			int channels=0;
			int rate=0;
			short* output=0;

			if (sample_data[i]->ogg_header == i)
			{
				sample_data[i]->ogg_data = ptr-ogg_data;

				int len = stb_vorbis_decode_memory(ptr, sample_data[i]->compressed_size, &channels, &rate, &output);

				if (len != sample_data[i]->size)
				{
					int a=0;
					// problem?
				}

				temp_out[i] = output;
				//sample_data[i]->audio_data = output;
			}
			else
			{
				// make temp alloc containing shared header and this sample data
				unsigned char* ogg = (unsigned char*)malloc( sample_data[i]->ogg_header_size + sample_data[i]->compressed_size );

				int j = i+(short)sample_data[i]->ogg_header;
				memcpy(ogg, ogg_data + sample_data[j]->ogg_data, sample_data[i]->ogg_header_size);
				memcpy(ogg+sample_data[i]->ogg_header_size, ptr, sample_data[i]->compressed_size);

				int len = stb_vorbis_decode_memory(ogg, sample_data[i]->ogg_header_size + sample_data[i]->compressed_size, &channels, &rate, &output);

				if (len != sample_data[i]->size)
				{
					int a=0;
					// problem?
				}

				temp_out[i] = output;
				//sample_data[i]->audio_data = output;

				free(ogg);
			}

			ptr+=sample_data[i]->compressed_size;
		}
	}


	for (int i=0; i<nfo->samples; i++)
	{
		sample_data[i]->audio_data = 0;

		if (sample_data[i]->size)
		{
			sample_data[i]->audio_data = temp_out[i];

			// cross-mix sample at the loop point
			if (sample_data[i]->audio_data && (sample_data[i]->flags & 0x0030) == 0x0010)
			{
				// only cycle-loops (no bidi)
				int mix_len = sample_data[i]->end - sample_data[i]->start;
				mix_len = MIN(mix_len,(int)sample_data[i]->start);
				mix_len = MIN(mix_len,256); // about 5ms

				// [......aA|.........Bb|......]
				for (int m=0; m<mix_len; m++)
				{
					int ia = m + sample_data[i]->start - mix_len;
					int ib = m + sample_data[i]->end - mix_len;

					short a = sample_data[i]->audio_data[ia];
					short b = sample_data[i]->audio_data[ib];

					float wa = (float)m/mix_len;
					float wb = 1.0f-wa;

					sample_data[i]->audio_data[ib] = (short)(wa*a+wb*b);
				}

				mix_len = MIN(mix_len, (int)(sample_data[i]->size - sample_data[i]->end));

				// [........|Aa.........|bB....]
				for (int m=0; m<mix_len; m++)
				{
					int ia = m + sample_data[i]->start;
					int ib = m + sample_data[i]->end;

					short a = sample_data[i]->audio_data[ia];
					short b = sample_data[i]->audio_data[ib];

					float wb = (float)m/mix_len;
					float wa = 1.0f-wb;

					sample_data[i]->audio_data[ib] = (short)(wa*a+wb*b);
				}
			}
		}
	}

	// collect all

	song->hdr = file_hdr;
	song->nfo = nfo;

	song->_data_hdr = data_hdr;

	song->song_seq = song_seq;
	song->voice_seq = voice_seq;
	song->pattern_len = pattern_len;

	song->voice_data = voice_data;
	song->instr_data = instr_data;
	song->sample_data = sample_data;


	// last part is to extract SFX samples
	song->sfx_instr = sfx_instr;

	return true;
}


void CloseSong(SONG* song)
{
	if (song->_data_hdr)
	{

		for (int i=0; i<song->nfo->samples; i++)
		{
			if (song->sample_data[i]->audio_data)
				free(song->sample_data[i]->audio_data);
		}

		free(song->voice_data);
		free(song->instr_data);
		free(song->sample_data);

		free(song->_data_hdr);
		song->_data_hdr = 0;
	}
}

// commands

struct XMChannel
{
	unsigned char* voice_ptr;
	int row_reps;

	// main context
	mo3_instr* instr;
	mo3_sample* sample;
	float frq;
	int key;
	int period;
	int vol;
	int pan;
	int ofs;

	float frq0;
	int vol0;
	int pan0;

	// envelope
	int age;
	int evol0;
	int evol1;

	// tick effects
	int vol_slide;
	int tone_porta;
	int slide;
	int porta_target;
	int vibrato;
	int vibrato_memory;
	int porta_memory;
	int vibpos;

	int Smp(float t); // <- should be optimized by storing some extra context (prev sample pos?) so no % is in use
	void Evl();
};

void XMChannel::Evl()
{
	if (instr && (instr->vol_envelope.flags==1) && instr->vol_envelope.nodes>0)
	{

		// find pair of nodes sourrounding instrument age
		int i;
		for (i=0; i<instr->vol_envelope.nodes; i++)
		{
			short t = instr->vol_envelope.node[2*i];

			if (age < t)
				break;
		}

		if (i==0)
		{
			short v = instr->vol_envelope.node[1];
			evol0 = v;
			evol1 = v;
		}
		else
		if (i==instr->vol_envelope.nodes)
		{
			// stop!
			instr=0;
			sample=0;
		}
		else
		{
			short t0 = instr->vol_envelope.node[2*(i-1)];
			short t1 = instr->vol_envelope.node[2*i];
			short v0 = instr->vol_envelope.node[2*(i-1)+1];
			short v1 = instr->vol_envelope.node[2*i+1];

			evol0 = v0 + (age - t0)*(v1-v0) / (t1-t0);
			evol1 = v0 + (age+1 - t0)*(v1-v0) / (t1-t0);
		}
	}
	else
	{
		evol0 = 64;
		evol1 = 64;
	}
}

int XMChannel::Smp(float t)
{
	unsigned int t0 = (int)floorf(t);
	unsigned int t1 = t0+1;

	float w0 = t1-t;
	float w1 = t-t0;

	if (sample->flags & 0x0010)
	{
		// loop enabled
		if (sample->flags & 0x0020) 
		{
			// bidi loop
			if (t1>=sample->end)
			{
				unsigned int period = sample->end - sample->start;
				unsigned int phase0 = (t0 - sample->start) % (2*period);
				unsigned int phase1 = (t1 - sample->start) % (2*period);

				if (phase0<period)
					t0 = phase0 + sample->start;
				else
					t0 = sample->end - (phase0-period) - 1;

				if (phase1<period)
					t1 = phase1 + sample->start;
				else
					t1 = sample->end - (phase1-period) - 1;
			}
		}
		else
		{
			// cycle loop
			if (t1>=sample->end)
			{
				unsigned int period = sample->end - sample->start;
				unsigned int phase0 = (t0 - sample->start) % period;
				unsigned int phase1 = (t1 - sample->start) % period;
				t0 = phase0 + sample->start;
				t1 = phase1 + sample->start;
			}
		}
	}
	else
	if (t1>=sample->size)
	{
		instr=0;
		sample=0;
		return 0;
	}

	short a0 = sample->audio_data[t0];
	short a1 = sample->audio_data[t1];

	return (int)(a0*w0 + a1*w1);
}

struct XMPlayer
{
	XMPlayer();
	~XMPlayer();

	void Load();
	SONG song;

	int song_seq;	// song position ( % song->nfo->song_len )
	int pattern;	// current pattern 
	int row;		// processed in current pattern ( % song->pattern_len[pattern] )
	int tick;		// processed in current row ( % song->nfo->ticks_per_row )

	int aud_frq;	// initialized with first call to pull_audio
	int aud_rem;	// how many samples are already written for current tick to output audio

	XMChannel chn[32*2];

	void Tck();
	void Row();
	void Cmd(int c, unsigned char* data, int pairs);
	void Mix(int chn, int frq, short* buffer, int samples);

// TEMPO:
// ticks_per_sec = tempo*24/60 (ie tempo=125,ticks_per_row=6,rows_per_pattern=64 -> tps=50, pattern_dur=64*6/50=7.68s)

	// EXTERNAL CONTROLL, requires CS!

	int fade_out; // -1 no fade
	int fade_ofs;

	int cmd_sync; // -1 nutting
	int cmd_start; 
	int cmd_loop_a;
	int cmd_loop_b;

	int loop_a;
	int loop_b;

	unsigned char ext_vol;
	unsigned int  tempo;
};

static XMPlayer xm_player;

XMPlayer::~XMPlayer()
{
	CloseSong(&song);
}

XMPlayer::XMPlayer()
{
	song._data_hdr = 0;
	song.sfx_instr = -1;
}

void XMPlayer::Load()
{
	if (!song._data_hdr)
		OpenSong(&song);

	ext_vol = 192;

	tempo = song.nfo->initial_tempo;

	fade_out = -1;
	cmd_sync = -1;

	loop_a = 0;
	loop_b = 28;

	song_seq = 23; // 0


	pattern = song.song_seq[song_seq];
	row = 0;
	tick = 0;

	memset(chn,0,sizeof(XMChannel)*32*2);
	for (int c=0; c<song.nfo->channels; c++)
	{
		chn[c].key = 0xFF;
		chn[c].voice_ptr = song.voice_data[ GET_WORD(song.voice_seq + pattern*song.nfo->channels + c ) ];
	}

	Row();
}

void XMPlayer::Tck()
{
	for (int c=0; c<song.nfo->channels; c++)
	{
		chn[c].vol0 = chn[c].vol;
		chn[c].pan0 = chn[c].pan;
		chn[c].frq0 = chn[c].frq;
	}

	// clear all faders
	memset(chn+song.nfo->channels,0,sizeof(XMChannel)*song.nfo->channels);

	tick++;

	if (tick == song.nfo->ticks_per_row)
	{
		bool done = false;

		lock_player();
		if (cmd_sync==0 || cmd_sync==1 && row+1 == GET_WORD(song.pattern_len+pattern))
		{
			xm_player.fade_out = -1;
			xm_player.fade_ofs = 0;

			if (cmd_loop_a>=0 && cmd_loop_a<song.nfo->song_len)
				loop_a = cmd_loop_a;
			if (cmd_loop_b>loop_a && cmd_loop_b<=song.nfo->song_len)
				loop_b = cmd_loop_b;
			if (cmd_start>=0 && cmd_start<song.nfo->song_len)
			{
				done = true;

				song_seq = cmd_start;
				pattern = song.song_seq[song_seq];
				row=0;
				tick=0;

				for (int c=0; c<song.nfo->channels; c++)
				{
					chn[c].sample=0;
					chn[c].instr=0;

					chn[c].row_reps=0;
					chn[c].voice_ptr = song.voice_data[ GET_WORD(song.voice_seq + pattern*song.nfo->channels + c ) ];
				}

				Row();
			}

			cmd_sync = -1;
		}
		unlock_player();

		if (!done)
		{
			tick=0;
			row++;
			if (row == GET_WORD(song.pattern_len+pattern))
			{
				row=0;
				song_seq++;
				if (song_seq == loop_b || song_seq == song.nfo->song_len)
				{
					song_seq = loop_a;

					for (int c=0; c<song.nfo->channels; c++)
					{
						chn[c].sample=0;
						chn[c].instr=0;
					}
				}

				pattern = song.song_seq[song_seq];

				OutputDebugStringA("\n");

				for (int c=0; c<song.nfo->channels; c++)
				{
					chn[c].row_reps=0;
					chn[c].voice_ptr = song.voice_data[ GET_WORD(song.voice_seq + pattern*song.nfo->channels + c ) ];
				}
			}

			Row();
		}
	}

	// calc slopes for current tick

	for (int c=0; c<song.nfo->channels; c++)
	{
		// apply tck fx
		if (tick) {
			chn[c].vol += chn[c].vol_slide;
			if (chn[c].sample) {
				if (chn[c].tone_porta) {
					int i = chn[c].tone_porta;
					while (i--) {
						int diff = chn[c].porta_target-chn[c].period;
						if (!diff)
							break;
						chn[c].period += diff > 0 ?1:-1;
					}
				}
				if (chn[c].slide) {
					chn[c].period += chn[c].slide;
				}
			}
		}
		if (chn[c].sample) {
			float vib = 0;
			if (chn[c].vibrato) {
				if (chn[c].vibrato) {
					chn[c].vibpos += chn[c].vibrato & 0xf0;
					vib = sin(6.28 * chn[c].vibpos/1024.0) * (chn[c].vibrato & 0xf);
				}
			}
			chn[c].frq = 8363.0f * powf( 2.0f, (chn[c].period/16.0 - 48 + (chn[c].sample->transpose + ((int)chn[c].sample->finetune-128)/128.0f) + vib/16.0 ) / 12.0f );
		}
		if (chn[c].vol<0)
			chn[c].vol=0;
		if (chn[c].vol>64)
			chn[c].vol=64;

		// calc envelope slope
		chn[c].age++;
		chn[c].Evl();
	}
}

void XMPlayer::Row()
{

	// reset fx on new a row
	for (int c=0; c<song.nfo->channels; c++)
	{
		chn[c].vol_slide = 0;
		chn[c].tone_porta = 0;
		chn[c].slide = 0;
		chn[c].vibrato = 0;
	}

	char r[32];
	sprintf_s(r,32,"%02d / %02d : ", pattern, row);
	OutputDebugStringA(r);

	// main task is to advance 
	for (int c=0; c<song.nfo->channels; c++)
	{
		if (!chn[c].voice_ptr)
		{
			Cmd(c,0,0); // dummy dump
			continue;
		}

		unsigned char* ptr = chn[c].voice_ptr;
		unsigned char n = *ptr;
		if (n==0)
		{
			chn[c].voice_ptr = 0;

			Cmd(c,0,0); // dummy dump
			continue;
		}

		ptr++;

		int reps = (n>>4)&0x0F;
		chn[c].row_reps++;

		int pairs = n&0x0F;
		
		if (pairs)
		{
			Cmd(c,ptr,pairs);
			ptr+=2*pairs;
		}
		else
		{
			Cmd(c,0,0); // dummy dump
		}

		if (chn[c].row_reps == reps)
		{
			chn[c].row_reps=0;
			chn[c].voice_ptr = ptr;
			if (*ptr==0)
				chn[c].voice_ptr=0;
		}
	}

	OutputDebugStringA("\n");
}

const static char* notes[12]=
{
	"C-",
	 	"C#",
	"D-",
	 	"D#",
	"E-",
	 
	"F-",
	 	"F#",
	"G-",
	 	"G#",
	"A-",
	 	"A#",
	"B-"
};

void XMPlayer::Cmd(int c, unsigned char* data, int pairs)
{
	unsigned char* ptr;

	char dbg[32]="--- -- -- - -- | ";

	bool has_instr = false, porta = false;
	int p;

	for (p=0, ptr=data; p<pairs; p++,ptr+=2)
	{
		switch ((Mo3CmdType)ptr[0])
		{
			case tone_portamento:
			case vol_slide_tone_porta:
			{
				porta = true;
				break;
			}
		}
	}

	for (p=0, ptr=data; p<pairs; p++,ptr+=2)
	{
		switch ((Mo3CmdType)ptr[0])
		{
			case note:
			{
				int key = ptr[1];

				if (key==0xff)
				{
					dbg[0]='X';
					dbg[1]='X';
					dbg[2]='X';				
				}
				else if (key==0xfe)
				{
					dbg[0]='^';
					dbg[1]='^';
					dbg[2]='^';				
				}
				else if (chn[c].sample && !porta)
				{
					// if prev note is still alive
					// we should move its context to nano-fadeout 
					// current params with upto 1-tick fade during mixing

					int fade_ch = c+song.nfo->channels;
					chn[fade_ch] = chn[c];

					// here we store fade progress in output samples
					chn[fade_ch].vol_slide = 0; 

					// clear others
					chn[fade_ch].tone_porta=0;
					chn[fade_ch].vibrato=0;
				}
				if (porta) {
					chn[c].porta_target = key*16;
				} else {
					chn[c].key = key;
					chn[c].period = key*16;
				}
				int o = key/12;
				int n = key-12*o;

				dbg[0]=notes[n][0];
				dbg[1]=notes[n][1];
				dbg[2]='1'+o;

				dbg[6]='v';
				dbg[7]='0'+chn[c].vol/10;
				dbg[8]='0'+chn[c].vol%10;

				break;
			}

			case instr:
			{
				has_instr = true;
				int i = ptr[1];

				dbg[4]='0'+(i+1)/10;
				dbg[5]='0'+(i+1)%10;

				chn[c].instr = song.instr_data[i];
				chn[c].sample = song.sample_data[ chn[c].instr->sample_map[chn[c].key] >> 16 ];

				chn[c].vol = chn[c].sample->volume;
				chn[c].pan = chn[c].sample->panning;
				chn[c].ofs = 0;
				chn[c].vibpos = 0;

				//chn[c].frq = 8363.0f * powf( 2.0f, (chn[c].period/16.0 - 48 + (chn[c].sample->transpose + ((int)chn[c].sample->finetune-128)/128.0f) ) / 12.0f );

				chn[c].vol0 = chn[c].vol;
				chn[c].pan0 = chn[c].pan;
				chn[c].frq0 = chn[c].frq;

				chn[c].age = 0;
				chn[c].Evl();

				dbg[6]='v';
				dbg[7]='0'+chn[c].vol/10;
				dbg[8]='0'+chn[c].vol%10;

				break;
			}

			case set_vol:
			{
				chn[c].vol = ptr[1];

				if (has_instr)
					chn[c].vol0 = chn[c].vol;

				dbg[6]='v';
				dbg[7]='0'+chn[c].vol/10;
				dbg[8]='0'+chn[c].vol%10;

				break;
			}

			case set_pan:
			{
				chn[c].pan = ptr[1]; // multiplied by 4! (it's OK because it matches sample->panning)

				if (has_instr)
					chn[c].pan0 = chn[c].pan;

				dbg[6]='p';
				dbg[7]='0'+(chn[c].pan>>2)/10;
				dbg[8]='0'+(chn[c].pan>>2)%10;

				break;
			}

			case sample_offs:
			{
				if (has_instr)
				{
					// hi risk of audible pop, let's fadein
					// check if sample at this offset is grater than -20db?
					// then fade in
					chn[c].vol0=0;
					chn[c].ofs = ptr[1]*256;
				}

				dbg[10]='9';
				dbg[12]="0123456789ABCDEF" [ ptr[1]/16 ];
				dbg[13]="0123456789ABCDEF" [ ptr[1]%16 ];

				break;
			}

			case vol_slide:
			{
				if ( (ptr[1]&0x0F) && !(ptr[1]&0xF0) )
					chn[c].vol_slide = - ptr[1];
				else
				if ( !(ptr[1]&0x0F) && (ptr[1]&0xF0) )
					chn[c].vol_slide = (ptr[1]>>4)&0x0F;

				dbg[10]='A';
				dbg[12]="0123456789ABCDEF" [ ptr[1]/16 ];
				dbg[13]="0123456789ABCDEF" [ ptr[1]%16 ];

				break;
			}

			case tone_portamento:
			{
				chn[c].tone_porta = chn[c].porta_memory = ptr[1];

				dbg[10]='3';
				dbg[12]="0123456789ABCDEF" [ ptr[1]/16 ];
				dbg[13]="0123456789ABCDEF" [ ptr[1]%16 ];

				break;
			}

			case vibrato:
			{
				if (ptr[1] & 0xf) {
					chn[c].vibrato_memory &= 0xf0;
					chn[c].vibrato_memory |= ptr[1] & 0xf;
				}
				if (ptr[1] & 0xf0) {
					chn[c].vibrato_memory &= 0xf;
					chn[c].vibrato_memory |= ptr[1] & 0xf0;
				}
				chn[c].vibrato = chn[c].vibrato_memory;

				dbg[10]='4';
				dbg[12]="0123456789ABCDEF" [ ptr[1]/16 ];
				dbg[13]="0123456789ABCDEF" [ ptr[1]%16 ];

				break;
			}

			case vol_slide_vibrato:
			{
				if ( (ptr[1]&0x0F) && !(ptr[1]&0xF0) )
					chn[c].vol_slide = - ptr[1];
				else
				if ( !(ptr[1]&0x0F) && (ptr[1]&0xF0) )
					chn[c].vol_slide = (ptr[1]>>4)&0x0F;

				chn[c].vibrato = chn[c].vibrato_memory;

				dbg[10]='6';
				dbg[12]="0123456789ABCDEF" [ ptr[1]/16 ];
				dbg[13]="0123456789ABCDEF" [ ptr[1]%16 ];

				break;
			}

			case portamento_down:
			{
				chn[c].slide = -ptr[1];

				dbg[10]='2';
				dbg[12]="0123456789ABCDEF" [ ptr[1]/16 ];
				dbg[13]="0123456789ABCDEF" [ ptr[1]%16 ];

				break;
			}

			case portamento_up:
			{
				chn[c].slide = ptr[1];

				dbg[10]='1';
				dbg[12]="0123456789ABCDEF" [ ptr[1]/16 ];
				dbg[13]="0123456789ABCDEF" [ ptr[1]%16 ];

				break;
			}

			case vol_slide_tone_porta:
			{
				if ( (ptr[1]&0x0F) && !(ptr[1]&0xF0) )
					chn[c].vol_slide = - ptr[1];
				else
				if ( !(ptr[1]&0x0F) && (ptr[1]&0xF0) )
					chn[c].vol_slide = (ptr[1]>>4)&0x0F;

				chn[c].tone_porta = chn[c].porta_memory;

				dbg[10]='5';
				dbg[12]="0123456789ABCDEF" [ ptr[1]/16 ];
				dbg[13]="0123456789ABCDEF" [ ptr[1]%16 ];

				break;
			}

			default:
			{
				int a=0;
				break;
			}
		}
	}

	OutputDebugStringA(dbg);
}

void XMPlayer::Mix(int ch, int frq, short* buffer, int samples)
{
	int ticks_per_minute = xm_player.tempo*24;

	// xxx is num of samples to be pulled before next tick occurs

	memset(buffer,0,ch*sizeof(short)*samples);

	int tick_samples = 60*frq / ticks_per_minute;
	int fade_samples = MIN(tick_samples, frq/20); // Min(20ms, 50ms)

	while (samples)
	{
		int xxx = tick_samples - xm_player.aud_rem;

		bool tick = false;
		if (xxx>samples)
			xxx=samples;
		else
			tick=true;

		int amp0=256;
		int amp1=256;

		if (fade_out>=0)
		{
			int fade_samples = fade_out * frq / 1000;
			if (fade_ofs>fade_samples)
			{
				amp0=0;
				amp1=0;
			}
			else
			{
				int tick_ofs = (fade_ofs/tick_samples)*tick_samples;
				amp0 = 256-tick_ofs*256 / fade_samples;
				amp1 = 256-(tick_ofs+tick_samples)*256 / fade_samples;

				if (amp0<0)
					amp0=0;
				if (amp1<0)
					amp1=0;

				fade_ofs += xxx;
			}
		}


		for (int c=0; c<song.nfo->channels  *2 ; c++)
		{
			if (chn[c].sample)
			{
				float scale = chn[c].frq / frq;

				// norm = 256*64
				int vol0 = amp0 * ext_vol * chn[c].vol0*chn[c].evol0 / 65536; 
				int vol1 = amp1 * ext_vol * chn[c].vol*chn[c].evol1 / 65536;

				if (c>=song.nfo->channels)
					vol1=0; // fadeout faders

				if (ch==1)
				{
					for (int i=0; i<xxx && chn[c].sample; i++)
					{
						int a = (ext_vol * chn[c].Smp(chn[c].ofs + i * scale))/256;

						// APPLY TCK interpolator (mono)
						int vol = vol0 + (vol1-vol0) * (xm_player.aud_rem+i)/tick_samples;
						
						a = buffer[i] + a*vol / (256*64);

						if (a>32767)
							a=32767;
						else
						if (a<-32768)
							a=-32768;

						buffer[i] = (short)a;
					}

					chn[c].ofs += (int)(xxx*scale);
				}
				else
				if (ch==2)
				{
					// norm = 256*64
					int vol0L = vol0 * MIN(128,255-chn[c].pan0)/128;
					int vol0R = vol0 * MAX(128,chn[c].pan0)/128;
					int vol1L = vol1 * MIN(128,255-chn[c].pan)/128;
					int vol1R = vol1 * MAX(128,chn[c].pan)/128;

					for (int i=0; i<xxx && chn[c].sample; i++)
					{
						int a = chn[c].Smp(chn[c].ofs + i * scale);

						// APPLY TCK interpolator (stereo)
						int volL = vol0L + (vol1L-vol0L) * (xm_player.aud_rem+i)/tick_samples;
						int volR = vol0R + (vol1R-vol0R) * (xm_player.aud_rem+i)/tick_samples;

						int al = buffer[2*i+0] + a * volL / (256*64);
						int ar = buffer[2*i+1] + a * volR / (256*64);

						if (al>32767)
							al=32767;
						else
						if (al<-32768)
							al=-32768;

						if (ar>32767)
							ar=32767;
						else
						if (ar<-32768)
							ar=-32768;

						buffer[2*i+0] = al;
						buffer[2*i+1] = ar;
					}

					chn[c].ofs += (int)(xxx*scale);
				}
			}
		}

		if (tick)
		{
			xm_player.aud_rem = 0;
			xm_player.Tck(); // <- it may change tempo!
			ticks_per_minute = xm_player.tempo*24;
			tick_samples = 60*frq / ticks_per_minute;
			fade_samples = MIN(tick_samples, frq/20); // Min(20ms, 50ms)
		}
		else
			xm_player.aud_rem += samples;

		buffer+=ch*xxx;
		samples-=xxx;
	}
}

void load_player()
{
	xm_player.Load();
}

int  get_sfx_ids(unsigned int idarr[96])
{
	if (xm_player.song.sfx_instr<0 || !idarr)
	{
		return 0;
	}

	mo3_instr* instr = xm_player.song.instr_data [ xm_player.song.sfx_instr ];

	int len=0;
	for (int i=0; i<96; i++)
	{
		int j;
		unsigned int id = instr->sample_map[i] >> 16;
		mo3_sample* sample = xm_player.song.sample_data[ id ];
		if (!sample || !sample->audio_data)
			continue;

		for (j=0; j<len; j++)
		{
			if (idarr[j] == id)
				break;
		}
		if (j==len)
		{
			idarr[len] = id;
			len++;
		}
	}

	return len;
}

short* get_sfx_sample(unsigned int id, int* len)
{
	if (xm_player.song.sfx_instr<0)
	{
		if (len)
			*len=0;
		return 0;
	}

	mo3_sample* sample = xm_player.song.sample_data[ id ];

	if (len)
		*len = sample->size;

	return sample->audio_data;
}

void pull_sound(int chn, int frq, short* buffer, int samples)
{
	xm_player.Mix(chn, frq, buffer, samples);
}

void FadeOut(int time)
{
	if (time<=0)
		return;

	lock_player();

	if (xm_player.fade_out<0)
	{
		xm_player.fade_out = time;
		xm_player.fade_ofs = 0;
	}

	unlock_player();
}

// sync: 0=after current row, 1-after current pattern
// if in fadout state, it will be cleared at the sync time
void PlayLoop(int sync, int start, int loop_a, int loop_b)
{
	lock_player();

	xm_player.cmd_sync = sync;
	xm_player.cmd_start = start;
	xm_player.cmd_loop_a = loop_a;
	xm_player.cmd_loop_b = loop_b;

	unlock_player();
}

void PlaySFX(SFX sfx, void** voice, bool loop, int vol, int pan)
{
	if (xm_player.song.sfx_instr<0)
	{
		if (voice)
			*voice=0;
		return;
	}
	mo3_instr* instr = xm_player.song.instr_data [ xm_player.song.sfx_instr ];
	unsigned int id = instr->sample_map[(int)sfx] >> 16;

	play_sfx(id, voice, loop, vol, pan);
}

void StopSFX(void* voice, int fade)
{
	if (xm_player.song.sfx_instr<0)
		return;

	if (!voice)
		stop_sfx(fade);
	else
		stop_sfx(voice,fade);
}
