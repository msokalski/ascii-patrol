#ifndef SPEC_H
#define SPEC_H

void DBG(const char* str);

struct MODAL
{
	virtual ~MODAL() {}
	virtual int Run() = 0;
};

extern MODAL* modal;

int terminal_init(int argc, char* argv[], int* dw, int* dh);
void terminal_done();

void terminal_loop();

struct CON_OUTPUT
{
	int w,h;
	char* buf;
	char* color;
	void* arr;
};

void free_con_output(CON_OUTPUT* screen);
void resize_con_output(CON_OUTPUT* s, int _w, int _h, char t); // not specialized!

void get_terminal_wh(int* dw, int* dh);

int screen_write(CON_OUTPUT* screen, int dw, int dh, int sx, int sy, int sw, int sh);

#define CON_INPUT_KBD 0x0001
#define CON_INPUT_FOC 0x0002
#define CON_INPUT_UNK 0xFFFF

#define CON_INPUT_TCH_BEGIN 0x0003
#define CON_INPUT_TCH_MOVE  0x0004
#define CON_INPUT_TCH_END   0x0005

// non-ascii key mappings
// BUT: bkspc=8, tab=9, esc=27, enter=13
#define KBD_LT	1
#define KBD_RT	2
#define KBD_UP	3
#define KBD_DN	4
#define KBD_DEL	5
#define KBD_INS 6
#define KBD_HOM 14
#define KBD_END 15
#define KBD_PUP 16
#define KBD_PDN	17

struct CON_INPUT
{
    int EventType;
    union
    {
		struct
		{
			bool bKeyDown;
			struct
			{
				char AsciiChar;
			} uChar;
		} KeyEvent;

		struct
		{
			bool bSetFocus;
		} FocusEvent;

		struct
		{
			int x,y;
			int id;
		} TouchEvent;

    } Event;
};

void vsync_wait();
void sleep_ms(int ms);
unsigned int get_time();

bool get_input_len( int* r);
bool spec_read_input( CON_INPUT* ir, int n, int* r);
bool read_input( CON_INPUT* ir, int n, int* r);
bool has_key_releases();

#ifndef _WIN32

#define sprintf_s(dst,size,...) sprintf(dst,__VA_ARGS__)

#define sscanf_s(src,fmt,...) sscanf(src,fmt,__VA_ARGS__)

#define _strdup(str) strdup(str)

#define strcpy_s(dst,size,src) strcpy(dst,src)

int fopen_s(FILE** f, const char* path, const char* mode);

#endif

#ifdef DOS
float sqrtf(float f);
float logf(float f);
float floorf(float f);
float sinf(float f);
float cosf(float f);
float expf(float f);
float powf(float f, float n);
#endif

#define ABS(a) ((a)<0 ? -(a):(a))
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

void write_fs();
const char* conf_path();
const char* record_path();
const char* shot_path();

struct HISCORE
{
	int ofs;
	int siz;
	int tot;
	char id[16]; // returned from post
	char buf[65*55]; // cols=55 (fixed), rows=65 (max)
};

// initializes asyc data exchange
void post_hiscore();
void get_hiscore(int ofs, const char* id);

// updates hiscore if got fresh data & returns true if nothing is pending
// should be called on every frame, before accessing hiscore structure
bool update_hiscore();

void app_exit();


// SOUND

void init_sound(); // impl. by spec

void free_sound();

void pull_sound(int chn, int frq, short* buffer, int samples);
void load_player();

void set_gain(int mo3, int sfx); // impl. by spec

void lock_player();
void unlock_player();

int    get_sfx_ids(unsigned int idarr[96]);
short* get_sfx_sample(unsigned int id, int* len);

// BY SPEC!
bool play_sfx(unsigned int id, void** voice, bool loop, int vol, int pan);

bool stop_sfx(int fade); // stop all sfx
bool stop_sfx(void* voice, int fade); // returns false if given voice is already stopped

bool set_sfx_params(void* voice, int vol, int pan);



#endif
