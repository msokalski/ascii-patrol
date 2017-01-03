#ifndef UNMO3_H
#define UNMO3_H

#pragma pack(push,1)

struct mo3_hdr
{
	char mo3[3];			// "MO3"
	char ver;				// version (0 with mp3 and lossless, 4 with v2.1, 1 with ogg) related with sample compression, 4 should means "with no LAME header". 3 means v2.2 and 5 v2.4 
	unsigned int hdrlen;	// uncompressed length of header (music data) 

	// only ver=5!
	int offs;				// data offset in compressed data after decompression
};

struct mo3_nfo
{
	unsigned char channels; // 0x20 for .xm, 0x04 for .mod

	unsigned short song_len;
	unsigned short restart_pos;
	unsigned short patterns;
	unsigned short unique_voices;
	unsigned short instruments;
	unsigned short samples;

	unsigned char  ticks_per_row;
	unsigned char  initial_tempo;

	unsigned int   flags;

	unsigned char  global_vol;
	unsigned char  pan_sep;
	unsigned char  internal_vol;
	unsigned char  def_chn_vol[64];
	unsigned char  def_chn_pan[64];
	unsigned char  it_midi_macro1[16];
	unsigned char  it_midi_macro2[128*2];
};

struct mo3_envelope
{
	unsigned char flags;
	unsigned char nodes;
	unsigned char loop_begin;
	unsigned char loop_end;
	unsigned char sust_loop_begin;
	unsigned char sust_loop_end;
	short         node[2*25]; 
	// vol nodes: position (short), value 0->64 (short) 
	// pan nodes: position (short), value +32/-32 (short) 
	// pitch nodes: position (short), value +32/-32 (short) 
};

struct mo3_instr // 0x33a 
{
	unsigned int   flags;
	unsigned int   sample_map[10*12]; // 10 octaves, 12 semi-tones, 4 bytes (sample_id is in 2 higher bytes)

	mo3_envelope   vol_envelope;
	mo3_envelope   pan_envelope;
	mo3_envelope   pitch_envelope; 

	unsigned char  vibrato_type; // (0=sine, 1=Ramp down, 2=square, 3=random) 
	unsigned char  vibrato_sweep;
	unsigned char  vibrato_depth;
	unsigned char  vibrato_rate;
	unsigned short fade_out;      // time to keep sound after note off ???

	unsigned char  midi_channel;
	unsigned char  midi_bank;
	unsigned char  midi_patch;
	unsigned char  midi_bend;
	unsigned char  glb_volume; // *2?
	short          panning;

	// .it
	unsigned char  it_stuff[11];
};

struct mo3_sample
{
	unsigned int   finetune;  // (0x00 in file = -128, 0x80 = 0, 0x76 = -10, 0x90 = 16) [MOD,MTM, XM] or "C4/5 speed" for S3M and IT (with Amiga slides), unless linear bit is also set. 
	unsigned char  transpose;
	unsigned char  volume; // max 64
	short          panning;
	unsigned int   size; // (in bytes for 8bits, in short for 16bits). if size==0 and end!=0 : means removed sample (not used) 
	unsigned int   start;
	unsigned int   end;

	unsigned short flags;
	/*
		bit #0 (0x0001): 1=16bits, 0=8bits
		bit #4 (0x0010): 1=loop
		bit #5 (0x0030): 1=bi-loop (both set with bit#4) [IT]
		bit #12 (0x1000): 1=lossy compression (1=mp3 0x1000, together with bit#13 (0x3000) means ogg)
		bit #13 (0x2000) : lossless compression 'delta' (with bit#12 cleared)
		bit #14 (0x4000) : lossless compression 'delta prediction'
		0x0000 means "not compressed" if size!=0
	*/

	union
	{
		unsigned char  it_stuff[13];

		struct
		{
			unsigned short ogg_header;
			unsigned int   ogg_data;
		};

		short* audio_data;
	};

	unsigned int compressed_size;

	union
	{
		unsigned short encoder_delay;
		unsigned short ogg_header_size;
	};

};

#pragma pack(pop)


// .xm voice data
enum Mo3CmdType				// PARAM
{
	note = 0x01,			// note id (0=C-0, ...	0x5b=G-7, ... , 0xff means "note off",  0xfe means "^^"????)
	instr = 0x02,			// instrument id (0 based)
	portamento_up  = 0x04,
	portamento_down = 0x05,
	tone_portamento = 0x06,
	vibrato = 0x07,
	vol_slide_tone_porta = 0x08,
	vol_slide_vibrato = 0x09,
	set_pan = 0x0B,			//  'p02' is coded '0b 00', 'p62' '0b f0', 'p10' '0b 20'
	sample_offs = 0x0c,
	vol_slide = 0x0d,
	set_vol = 0x0f,
	pattern_brk = 0x10,
	pattern_delay = 0x11,
	set_speed = 0x12,
	vol_slide_up = 0x14,	// 'c03' is coded "14 30" 
	fine_vol_donw = 0x15,
	set_glb_vol = 0x16
};


struct SONG
{
	mo3_hdr* hdr;
	mo3_nfo* nfo;

	unsigned char* _data_hdr; //*
	
	unsigned char* song_seq;
	unsigned short* voice_seq;
	unsigned short* pattern_len;

	unsigned char** voice_data; //*
	mo3_instr** instr_data; //*
	mo3_sample** sample_data; //* + sample_data[]->audio_data

	int sfx_instr;
};

bool OpenSong(SONG* song);

void CloseSong(SONG* song);

void FadeOut(int time);
void PlayLoop(int sync, int start, int loop_a, int loop_b);

enum SFX
{
	CAR_LASER = 24,
	CAR_CANNON = 25,
	FLY_BOMB = 26,
	TANK_SHOT = 27,
	BULLET_BULLET = 28,
	BOMB_GROUND = 29,
	BOMB_EXPL = 30,

	CAR_JUMP = 36,
	WHEEL_BOUNCE0 = 37,
	WHEEL_BOUNCE1 = 38,
	WHEEL_BOUNCE2 = 39,

	CAR_EXPLODE = 48,
	DRONE_DEATH = 49,
	UFO_DEATH = 50,
	BOMBER_DEATH = 51,
	HEAP_DEATH = 52,
	BALL_DEATH = 53,
	TANK_DEATH = 54,
	CREATURE_DEATH = 55,
	CREATURE_CORPS = 56,
	ROCKET_DEATH = 57,
	FLY_BONUS = 58,

	FIELD_NOISE = 60, // (TODO) loop!
	CHECKPOINT_PASS,
	LAST_CHECKPOINT,
	ROCKET_FOLLOW,    // (TODO) loop! change vol on burst
};

void PlaySFX(SFX sfx, void** voice=0, bool loop=false, int vol=100, int pan=0);
void StopSFX(void* voice, int fade);

#endif
