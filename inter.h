
#ifndef INTER_H
#define INTER_H

#include "game.h"

struct PERF
{
	~PERF();
	PERF(int _idx, const char* _name, int _val, int _minval, int _maxval, int _speed, int _bonus);

	void Paint(SCREEN* s, int x, int y, int width);
	int  Animate(unsigned long t); // return progress value (from min to val)

	SPRITE prgs;
	SPRITE tick;
	SPRITE line;
	SPRITE horz; // not used by lineidx==1
	SPRITE icon; // used only if unit==0
	FNT fnt;

	unsigned long time;
	int pro;

	int idx;
	const char* name;

	int minval, val, maxval;

	int speed;
	int bonus;

	bool disabled;
	int state; // 0-waiting, 1-animating, 2-done
};

struct DLG
{
	~DLG();
	DLG(const ASSET* _frame, int _tx, int _ty, unsigned char _cl, const char* _name, int _nx, int _ny, const ASSET* _avatar, unsigned long _mix, int _ax, int _ay, int _vx, int _vy);

	void Start(const char* txt, unsigned long t);
	bool Animate(unsigned long t);
	void Finish();
	void Clear();

	void Paint(SCREEN* s, int x, int y, bool blend);

	unsigned long start;
	unsigned long time;
	bool done;
	int vox;
	int vx,vy;

	char* text;
	int lines;
	int chars;

	unsigned char cl;
	int tx,ty;

	const char* name;
	int nx,ny;

	SPRITE frame;

	SPRITE avatar;
	unsigned long mix;
	int ax,ay;

};

struct INTER_MODAL : MODAL
{
	SCREEN* s;
	int w,h;

	const LEVEL* level;

	PERF perf_clear;
	PERF perf_time;
	PERF perf_lives;
	FNT fnt;

	int dialog_idx;
	int dialog_pos;

	DLG dlg_player;
	DLG dlg_commander;
	DLG dlg_alien;

	int interference;
	int inter_gap;
	int inter_pos;
	unsigned long prev_t;

	DLG* dlg[2];

	SPRITE bkgnd;

	int phase;
	unsigned long phase_t;

	int time;
	int* score;
	int bonus;

	virtual ~INTER_MODAL();
	INTER_MODAL(SCREEN* _s, 
				int _lives, int _startlives, int* _score, 
				const char* _course_name, const LEVEL* _current_level, 
				int _time, int _hitval, int _hitmax);

	virtual int Run();
};

#endif

