
#ifdef WEB


#include <emscripten.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "spec.h"
#include "conf.h"
#include "menu.h"

static int output_width = 80;
static int output_height = 25;
static unsigned char* output_buffer = 0;

int terminal_init(int argc, char* argv[], int* dw, int* dh)
{
	output_buffer = new unsigned char[256*64*4];

	if (dw)
		*dw=output_width;
	if (dh)
		*dh=output_height;

	EM_ASM(
		FS.mkdir('/asciipat');
		FS.mount(IDBFS, {}, '/asciipat');
		FS.syncfs(true, function (err) 
		{
			if (!err)
			{
				ccall('LoadConf', 'v');
			}
		});
	);

	return 0;
}

void terminal_done()
{
	delete [] output_buffer;
}

void vsync_wait()
{
}

void sleep_ms(int ms)
{
}

unsigned int get_time()
{
	double dt = emscripten_get_now();
	unsigned int it = (unsigned int)floor(dt);
	if (!it)
		it = 1;
	return it;
}

void free_con_output(CON_OUTPUT* screen)
{
}

void get_terminal_wh(int* dw, int* dh)
{
	if (dw)
		*dw=output_width;
	if (dh)
		*dh=output_height;
}

static int valid=0;
int screen_write(CON_OUTPUT* screen, int dw, int dh, int sx, int sy, int sw, int sh)
{
	if (!output_buffer || !valid)
		return 0;

	// note:
	// regardless of output_width,height, the output_buffer is 256x64 (same size as cells texture)

	int dx=0;
	int dy=0;

	if (screen->color)
	{
		int s = (screen->w+1)*screen->h;
		for (int y=0; y<sh; y++)
		{
			for (int x=0; x<sw; x++)
			{
				int o = ((y+dy)*256 + x+dx)*4;
				int i = (y+sy)*(screen->w+1)+x+sx;
				output_buffer[o] = screen->buf[i];
				output_buffer[o+1] = screen->buf[i+s];
			}
		}
	}
	else
	{
		int s = (screen->w+1)*screen->h;
		for (int y=0; y<dh; y++)
		{
			for (int x=0; x<dw; x++)
			{
				int o = ((y+dy)*256 + x+dx)*4;
				int i = (y+sy)*(screen->w+1)+x+sx;
				output_buffer[o] = screen->buf[i];
				output_buffer[o+1] = 0xF0;
			}
		}
	}

	return 0;
}

int input_buffer[256];
int input_buffer_len=0;

bool get_input_len( int* r)
{
	if (r)
		*r=input_buffer_len;
	return true;
}

bool spec_read_input( CON_INPUT* ir, int n, int* r)
{
	int l = input_buffer_len;
	int nr = n<l ? n : l;

	for (int i=0; i<nr; i++)
	{
		int c = input_buffer[i];
		ir[i].EventType = CON_INPUT_KBD;
		ir[i].Event.KeyEvent.bKeyDown = (c&0x80)!=0;
		ir[i].Event.KeyEvent.uChar.AsciiChar=0;

		c &= 0x7f;
		switch (c)
		{
			case 37: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_LT	; break;
			case 39: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_RT	; break;
			case 38: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_UP	; break;
			case 40: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_DN	; break;
			case 46: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_DEL; break;
			case 45: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_INS; break;
			case 36: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_HOM; break;
			case 35: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_END; break;
			case 33: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_PUP; break;
			case 34: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_PDN; break;
			default:
				ir[i].Event.KeyEvent.uChar.AsciiChar = c;
		}
	}

	if (r)
		*r=nr;

	input_buffer_len-=nr;
	memmove(input_buffer,input_buffer+nr,input_buffer_len);

	return true;
}

bool has_key_releases()
{
	return true;
}

int fopen_s(FILE** f, const char* path, const char* mode)
{
	if (!f)
		return -1;
	*f = fopen(path,mode);
	return *f==0;
}


static HISCORE hiscore_async = {0,0,0,"",""};
static bool hiscore_ready = false;

short* audio_buf=0;

static unsigned int sfx_id[96];
static int sfx_count=0;

extern "C"
{
	short* set_audio(int ch, int frq, int samples)
	{
		if (audio_buf)
			delete [] audio_buf;
		audio_buf=0;

		int size = ch*samples;
		if (size>0)
			audio_buf = new short[size];

		load_player();

		sfx_count = get_sfx_ids(sfx_id);

		return audio_buf;
	}

	int get_sfx_count()
	{
		return sfx_count;
	}

	unsigned int* get_sfx_arr()
	{
		return sfx_id;
	}

	int get_sfx_len(unsigned int id)
	{
		int len=0;
		get_sfx_sample(id,&len);
		return len;
	}

	short* get_sfx_data(unsigned int id)
	{
		int len=0;
		return get_sfx_sample(id,&len);
	}

	void del_sfx(int voice)
	{
		// remove from alive voices
	}

	void get_audio(int ch, int frq, int samples)
	{
		if (audio_buf)
		{
			pull_sound(ch, frq, audio_buf, samples);
		}
	}

	void set_input(int msg)
	{
		if (input_buffer_len==256)
			input_buffer_len=0;
		input_buffer[input_buffer_len]=msg;
		input_buffer_len++;
	}

	unsigned char* set_output(int w, int h)
	{
		output_width = w;
		output_height = h;

		if (modal)
			modal->Run();

		return output_buffer;
	}

	void set_hiscore(char* str)
	{
		// here we need to parse str and fill hiscore_async, + raise hiscore_ready
		if (!str)
			return;

		int len = strlen(str);

		int err = 0;

		HISCORE tmp={0,0,0,"",""};

		char* line = str;
		if (len>=55)
		{
			if ( sscanf(line,"%d/%d",&tmp.ofs,&tmp.tot) != 2 )
			{
				err = -2;
			}
			else
			{
				char* id = strchr(line,' ');
				if (id)
				{
					id++;
					char* id_end = strchr(id,' ');
					int id_len = id_end-id;
					if (id_len>15)
						id_len=15;
					memcpy(tmp.id,id,id_len);
					tmp.id[id_len]=0;
				}
			}
		}
		else
			err = -1;

		if (!err)
		{
			line+=55;
			len-=55;

			tmp.siz = 0;
			while (len>=54)
			{
				/*
				// no need to put terminator before avatar
				line[45]=0;
				*/

				memcpy(tmp.buf + 55*tmp.siz, line, MIN(55,len));
				tmp.siz++;
				line+=55;
				len-=55;
			}

			// change last cr to eof
			tmp.buf[55 * tmp.siz -1] = 0; 
		}

		if (!err)
		{
			hiscore_async = tmp;
			hiscore_ready = true;
		}
	}
}

static unsigned int hash(const char* str)
{
	unsigned int hash = 0;
	for (int i=0; str[i]; i++)
	{
		hash  = ((hash << 5) - hash) + str[i];
	}

	return hash;
}

void terminal_loop()
{
	// nutting, just listen to get_output & get_input
	unsigned int x = time(0);

	srand(x+0x1234);
	unsigned int r = rand()%1461866239;

	unsigned int token = 
		EM_ASM_INT(	
		{
			var _uy = new Array('M','d','l','h','D','f','n','c','a','o','o','o','a','l','o','h','t','c','c','s','t','o','w','a','h','u','a','t','e','o','6','r','0','m','t','3','7','r','5','C','7','e','i','8','3','6','6','o','2','n','o','9','5','7','2','d','9','t','n','6','5','2','2','e','2','6','4','7','5','6','0','A','0','4','6','2','8','1','5','t');

			var _ii = function(_3,_1) { var _2 = _uy[_3]; for (var _0=1; _0<_1; _0++) _2+=_uy[_3+(_0<<3)]; return _2; };

			var _ku = window;
			var _ij = _ii(0,4);//'Math';
			var _ff = _ii(1,8);//'document';
			var _tr = _ii(2,8);//'location';
			var _px = _ii(3,4);//'host';
			var _ou = _ii(4,4);//'Date';
			var _ww = _ii(5,5);//'floor';
			var _uo = _ii(6,3);//'now';
			var _yy = _ii(7,10);//'charCodeAt';
			var _kt = _ku[_ij][_ww](_ku[_ou][_uo]() / 1000.0)-$0;

			var _uv  = '>' + _kt + _ku[_ff][_tr][_px];
			var _qp = 0;
			var _aw = _uv.length;
			for (var _ti = 0; _ti < _aw; _ti++) 
				_qp  = (((_qp << 5) - _qp) + _uv[_yy](_ti)) | 0;
			return _qp;

		}, r);

	char enc[]={0x3e,0x1b,0x4c,0x8,0x12,0x10,0xa,0x0,0x44,0x5d,0x11,0x15,0x6,0x1d,0x3,0x42,0x4d,0xc,0x2,0x0};

	char fmt[24];
	fmt[0]=enc[0];
	for (int i=1; i<19; i++)
		fmt[i]=enc[i]^fmt[i-1];
	fmt[19]=0;

	char buf[128];
	sprintf_s(buf,128,fmt,(int)x+0-r);
	unsigned int check0 = hash(buf);
	sprintf_s(buf,128,fmt,(int)x+1-r);
	unsigned int check1 = hash(buf);
	sprintf_s(buf,128,fmt,(int)x-1-r);
	unsigned int check2 = hash(buf);

	valid = token == check0 || token == check1 || token == check2;
	if (!valid)
	{
		memmove(fmt+7,fmt+3,17);
		memset(fmt+3,0x77,3);
		fmt[6]=0x2e;

		sprintf_s(buf,128,fmt,(int)x+0-r);
		check0 = hash(buf);
		sprintf_s(buf,128,fmt,(int)x+1-r);
		check1 = hash(buf);
		sprintf_s(buf,128,fmt,(int)x-1-r);
		check2 = hash(buf);

		valid = token == check0 || token == check1 || token == check2;
	}

	valid = 1; // UNPROTECTED !!!!!
}

void write_fs()
{
	EM_ASM(	
		FS.syncfs( function(err) 
		{ 
		}); 
	);
}

const char* conf_path()
{
	return "/asciipat/asciipat.cfg";
}

const char* record_path()
{
	return "/asciipat/asciipat.rec";
}

const char* shot_path()
{
	return "/asciipat/";
}

void post_hiscore()
{
	char script[100];
	sprintf_s(script,100,"post_hiscore('https://ascii-patrol.com/rank.php');");

	FILE* f;
	fopen_s(&f,record_path(),"rb");
	if (f)
	{
		fseek(f,0,SEEK_END);
		int siz = ftell(f);
		if (siz)
		{
			fseek(f,0,SEEK_SET);
			char* buf = new char[siz];
			fread(buf,1,siz,f);
			fclose(f);

			EM_ASM_INT(
			{
				xhr_ptr = $0;
				xhr_siz = $1;
			}, buf, siz);

			emscripten_run_script_int(script);

			delete [] buf;
		}
		else
			fclose(f);
	}
}

void get_hiscore(int ofs, const char* id)
{
	char script[100];
	if (id && id[0])
		sprintf_s(script,100,"get_hiscore('https://ascii-patrol.com/rank.php?rank=%d&id=%s');",ofs+1,id);
	else
		sprintf_s(script,100,"get_hiscore('https://ascii-patrol.com/rank.php?rank=%d');",ofs+1);

	emscripten_run_script_int(script);
}

bool update_hiscore()
{
	if (hiscore_ready)
	{
		hiscore_ready = false;

		if (hiscore.id[0]!=0 && hiscore_async.id[0]==0)
			memcpy(hiscore_async.id,hiscore.id,16);

		hiscore = hiscore_async;
	}

	return true;
}

void app_exit()
{
	EM_ASM(
		window.history.back();
	);
}

void init_sound()
{
	// nutting here
	// sound is initialized from html
}

void set_gain(int mo3, int sfx)
{
	EM_ASM_INT( { SetGain($0,$1); }, mo3,sfx );
}

void lock_player()
{
	// nutting here
	// webaudio is on same thread
}

void unlock_player()
{
	// nutting here
	// webaudio is on same thread
}

static unsigned int sfx_counter = 1;

bool play_sfx(unsigned int id, void** voice, bool loop, int vol, int pan)
{
	char* pv = (char*)0 + sfx_counter;

	if (voice)
		*voice = pv;

	int v = sfx_counter;
	int l = loop?1:0;

	sfx_counter++;
	if (!sfx_counter)
		sfx_counter=1;

	EM_ASM_ARGS( { PlaySfx($0,$1,$2,$3,$4); }, id,v,l,vol,pan );

	return true;
}

bool stop_sfx(int fade) // stop all sfx
{
	EM_ASM_ARGS( { StopSfx(0,$0); }, fade );

	return true;
}

bool stop_sfx(void* voice, int fade) // returns false if given voice is already stopped
{
	if (!voice)
		return false;

	unsigned int v = (char*)voice - (char*)0;
	EM_ASM_ARGS( { StopSfx($0,$1); }, v,fade );

	return true;
}

bool set_sfx_params(void* voice, int vol, int pan)
{
	unsigned int v = (char*)voice - (char*)0;
	EM_ASM_ARGS( { SetSfxParams($0,$1,$2); }, v,vol,pan );

	return true;
}


#endif
