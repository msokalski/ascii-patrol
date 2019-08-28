#ifndef GAME_H
#define GAME_H

#include "assets.h"
#include "spec.h"


void GamePreAlloc();

struct DIALOG
{
	int who;
	const char* text;
};

struct LEVEL
{
	char name;
	char name2;
	int time_limit;
	int time_perfect;
	ASSET* bkgnd;
	unsigned char sky;
	const char* sprite;
	const char* height;

	const DIALOG* dialog;
};

struct COURSE
{
	const char* name;
	const LEVEL* level;
	int flags;
};

extern unsigned char cl_transp;

extern unsigned char cl_label;
extern unsigned char cl_score;

extern unsigned char cl_statbkgnd;
extern unsigned char cl_statvalue;
extern unsigned char cl_statlabel;

extern unsigned char cl_ground;
extern char ch_ground;

extern unsigned char cl_bullet;
extern char ch_bullet;

extern unsigned char cl_basher;
extern unsigned char cl_drone;
extern unsigned char cl_ufo;
extern char ch_basher;
extern char ch_drone;
extern char ch_ufo;

void SetColorMode(unsigned char cl);

extern const LEVEL beginner[];
extern const LEVEL champion[];
extern /*const*/ LEVEL test_area[];

extern const COURSE campaign[];

int InterScreenInput();

struct SCREEN : CON_OUTPUT
{
	virtual ~SCREEN();
	SCREEN(int _w, int _h, char transp = ' ', unsigned char _tcolor = 0);
	void Clear();
	virtual void Resize(int _w, int _h);
	virtual void Paint(SCREEN* scr, int dx, int dy, int sx, int sy, int sw=-1, int sh=-1, bool blend=false);
	int Write(int dw, int dh, int sx, int sy, int sw=-1, int sh=-1);
	bool ClipTo(SCREEN* scr, int& dx, int& dy, int& sx, int& sy, int& sw, int& sh);

	void BlendPage(SCREEN* scr, int dx, int dy, int sx, int sy, int sw=-1, int sh=-1);

	char t;
	char  tcolor; // color used during clear
};

struct SPRITE
{
	int data_pos;
	int x_offset;
	int y_offset;
	int width;
	int height;
	int frames;
	int frame;
	const ASSET* data;
	SPRITE* next;
	SPRITE* prev;

	int group_id;

	int cookie;
	int cookie_data[4];

	const char* score_anim[2]; // mono & shade
	const char* score_color[2];
	ASSET asset;

	char score_text[81];
	char score_attr[81];

	unsigned char attrib_mask;
	unsigned char attrib_over;

	virtual ~SPRITE();

	SPRITE();
	SPRITE(const ASSET* anim);
	SPRITE(const char* name);
	SPRITE(int _score);

	void Init();
	void Init(const ASSET* anim);
	void Init(const char* name);
	void Init(int _score);

	void SetFrame(int fr);
	virtual bool HitTest(int sx, int sy);
	virtual void Paint(SCREEN* scr, int dx, int dy, int sx, int sy, int sw=-1, int sh=-1, char bgkey=0, bool blend=true);

	static int AnimWidth(const ASSET* a);
	static int AnimHeight(const ASSET* a);
	static int AnimFrames(const ASSET* a);
};

struct FNT
{
	SPRITE fnt;
	FNT(const ASSET* f);
	void Paint(SCREEN* s, int x, int y, unsigned char cl, const char* txt);
	//void Paint(SCREEN* s, int x, int y, unsigned char cl, int val);
};

struct WHEEL:SPRITE
{
	virtual ~WHEEL() {}
	WHEEL():SPRITE(&wheel) {}
};

struct CANNON:SPRITE
{
	virtual ~CANNON() {}
	CANNON():SPRITE(&cannon) {}
};

struct CHASSIS:SPRITE
{
	SPRITE b;
	int ex;
	virtual ~CHASSIS();
	CHASSIS();
	virtual void Paint(SCREEN* s, int dx, int dy, int sx, int sy, int sw=-1, int sh=-1, char underkey=0, bool blend=true);
	void Explode(int f);
};

struct SCROLL : SCREEN
{
	virtual ~SCROLL();
	SCROLL(int _w, int _h, char transp=' ', unsigned char _tcolor=0x00);

	virtual void Paint(SCREEN* scr, int dx, int dy, int sx, int sy, int sw=-1, int sh=-1, bool blend=false);
	virtual void Resize(int _w, int _h);
	virtual void Scroll(int s);

	int scroll;
};

struct BACKGROUND : SCROLL
{
	virtual ~BACKGROUND();
	BACKGROUND(int _w, int _h, unsigned char _tcolor);
	virtual void Scroll(int s);
};

struct LANDSCAPE : SCROLL
{
	int len;
	const ASSET* data;
	virtual ~LANDSCAPE();
	LANDSCAPE(int _w, int _h, const ASSET* _data, unsigned char _tcolor=0x00);

	virtual void Scroll(int s);
};

struct FIG:SPRITE
{
	virtual ~FIG();

	FIG(const ASSET* font, const char* str);
	FIG(const ASSET* font, int val, bool time);

	void Update(const char* str);
	virtual void Paint(SCREEN* scr, int _dx, int _dy, int _sx, int _sy, int _sw=-1, int _sh=-1, char bgkey=0, bool blend=true);

	int pitch;
	char* text;
};


struct BULLET
{
	int x,y;
	int vx,vy;
	int fr;
	int sx;
	int oy;
	void* voice;
	int cookie;
};


struct TERRAIN;

struct Field
{
	Field();
	void Rand();
	void Animate();
	float field(int x, int y);
	char cb(int x, int y);
	static char cb(int x, int y, void* p);

	float thr;
	int generator[10][5]; // 10 generators each x,y,r,vx,vy
	TERRAIN* t;
};

struct TERRAIN : SCROLL
{
	int lives;

	char* hitbin; // permanent deaths
	int hitcur[256]; // 256 is max num of enemies per sublevel!
	int hits;

	Field fld;
	int interference_size;
	int interference_from;
	int interference_to;
	float interference_mul;

	char base_point; // name of first chkpoint
	int check_passed;
	int check_scroll;
	int check_points;
	int* check_point;

	int scr_height;

	int group_id;
	int group_len[16];
	int group_mul[16];

	int sprite_scroll;

	int len;
	char* data;

	int items_len;
	const char* items;

	// ON GROUND
	SPRITE* head; // <- inserting new sprites
	SPRITE* tail; // <- reversed painting

	// FLYING
	SPRITE* fly_head; // <- inserting new sprites
	SPRITE* fly_tail; // <- reversed painting

	int bullets;
	BULLET bullet[100];

	virtual ~TERRAIN();
	TERRAIN(int _w, int _h, int _scrh, unsigned char _tcolor, const char* sprite, const char* height, char* _hitbin, char _base_point='A', char _start_point='@', int _lives=3);

	virtual void Resize(int _w, int _h);
	int GetMaxHeight(int x, int n);
	bool HitTest(CHASSIS* ch_spr, int x, int y, int fr);
	bool BulletTest(int fr, int x, int y, int* game_score);
	bool CannonTest(int x, int y, int f, int fr, int* game_score=0);
	void DismissSprites(int fr, int speed);
	void AnimateSprites(int fr, int speed, bool expl);

	virtual void Paint(SCREEN* scr, int dx, int dy, int sx, int sy, int sw=-1, int sh=-1, bool blend=false);
	virtual void Scroll(int s);
};

struct LEVEL_MODAL : MODAL
{
	void rec_write(unsigned char r);
	int rec_len;
	unsigned char rec_buf[1024*16]; // about 10 minutes

	int x;

	int write_size;
	int write_persec;

	int w;
	int h;
	
	BACKGROUND b;
	LANDSCAPE  m;
	LANDSCAPE  d;

	int speed;

	TERRAIN t;

	CHASSIS chassis;
	WHEEL wheel[3];

	int chaisis_vert; 

	int key_state;
	
	/*
	int tch_state;
	int tch_id[32];
	CON_INPUT tch_quit;
	*/

	int speed_id; // left / right + down
	int jump_id;
	int fire_id;
	int exit_id;

	int vely;
	int posy; // grounded

	int expl;
	int expl_x;
	int wheel_posx[3];
	int wheel_vert[3];
	int wheel_vely[3];
	int wheel_velx[3];

	int fr;
	int freeze_fr;

	unsigned long start_tm;
	unsigned long lag_tm;

	int t_scroll_div;
	int d_scroll_div;
	int m_scroll_div;
	int b_scroll_div;

	// absolute hi-res scroll position
	int scroll; 

	CANNON c;
	bool cannon_hit;
	int cannon_t;
	int cannon_x;
	int cannon_x2;
	int cannon_y;
	int cannon_sx;

	int bullets;
	int bullet_fr;
	BULLET bullet[32];

	// level passed anim
	int post_split;
	int post_scroll;
	int post_x;

	SCREEN* s;

	int* score;
	int* level_time;
	int lives;
	int start_lives;
	const char* player_name;

	void* jump_voice;

	const char* course_name;

	const LEVEL* current_level;

	virtual ~LEVEL_MODAL();
	LEVEL_MODAL(SCREEN* _s, int _lives, int _start_lives, int* _score, int* _level_time, const char* _player_name, 
				const char* _course_name, const LEVEL* _current_level, int iSubLev, char* _hitbin, FILE* _rec=0);

	virtual int Run();

	int record_fr;
	bool record_rel;
	int record_cmd; // fetched command used in 'R' mode,  will be dispatched when fr>=record_dr
	int rec_score;

	FILE* record_file;
	int record_mode; // 'W' 'R' or 0
	void Record(int ev, unsigned int fr, unsigned long a, unsigned long b); 
	int Fetch(int* _fr);

	// .rec / platform input wrapper
	void GetTerminalWH(int* _w, int* _h);
	bool GetInputLen(int* len);
	void ReadInput(CON_INPUT* ir, int n, int* r);
	bool HasKeyReleases();
};

#define REC_END 0 
#define REC_KEY 1 
#define REC_FOC 2
#define REC_SIZ 3 

extern int rec_show;

void Interference(SCREEN* s, int dist, int noise, unsigned long phase, int freq, int amp, bool sync);

#endif

