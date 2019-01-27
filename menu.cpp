// extern "C" _declspec(dllimport) void __stdcall OutputDebugStringA(const char* s);

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <memory.h>
#include <string.h>

#include "game.h"
#include "spec.h"
#include "menu.h"
#include "conf.h"

#include "twister.h"

HISCORE hiscore = 
{
	0,0,0,"",""
};

bool hiscore_sync = false;

struct MENU_ASSET : ASSET
{
	void Init(const ASSET* a)
	{
		mono = a->mono;
		shade = a->shade;
		color = a->color;

		const char** anim = mono;

		if (!anim || !anim[0])
		{
			w = 0;
			h = 0;
			frames = 0;
		}
		else
		{
			const char* brk = strchr(anim[0],'\n');
			if (brk)
				w = (int)(brk-anim[0]);
			else 
				w = (int)strlen(anim[0]);

			h=1;
			brk = strchr(anim[0],'\n');
			for (int i=0; brk; i++)
			{
				brk = strchr(brk+1,'\n');
				h++;
			}

			frames=0;
			for (int i=0; anim[i]; i++)
				frames++;

		}
	}

	MENU_ASSET(const ASSET* a)
	{
		Init(a);
		l=r=t=b=0;
	}

	MENU_ASSET(const ASSET* a, int left, int right, int top, int bottom)
	{
		Init(a);
		
		l=left;
		r=right;
		t=top;
		b=bottom;
	}

	int w,h,frames;
	int l,r,t,b;
};

static int menu_write(CON_OUTPUT* s, int dw, int dh, int sx, int sy, int sw, int sh)
{
	if (sw<0)
		sw=s->w;
	if (sh<0)
		sh=s->h;

	if (sx<0)
	{
		sw+=sx;
		sx=0;
	}
	if (sy<0)
	{
		sh+=sy;
		sy=0;
	}

	if (sx+sw>s->w)
	{
		sw = s->w-sx;
	}
	if (sy+sh>s->h)
	{
		sh = s->h-sy;
	}

	vsync_wait();

	return screen_write(s, dw,dh,sx,sy,sw,sh);
}

static void menu_print(CON_OUTPUT* s, int dx, int dy, const char* str, char color, int sw, const int* clip = 0)
{
	int sx=0;
	int sy=0;
	int sh=1;

	if (clip)
	{
		if (dx<clip[0])
		{
			sx+=clip[0]-dx;
			sw-=clip[0]-dx;
			dx=clip[0];
		}
		if (dy<clip[1])
		{
			sy+=clip[1]-dy;
			sh-=clip[1]-dy;
			dy=clip[1];
		}
		if (dx+sw>clip[0]+clip[2])
		{
			sw=clip[0]+clip[2]-dx;
		}
		if (dy+sh>clip[1]+clip[3])
		{
			sh=clip[1]+clip[3]-dy;
		}
	}

	if (dx<0)
	{
		sx-=dx;
		sw+=dx;
		dx=0;
	}
	if (dy<0)
	{
		sy-=dy;
		sh+=dy;
		dy=0;
	}

	if (sx<0)
	{
		dx-=sx;
		sw+=sx;
		sx=0;
	}
	if (sy<0)
	{
		dy-=sy;
		sh+=sy;
		sy=0;
	}

	if (dx+sw>s->w)
	{
		sw = s->w - dx;
	}
	if (dy+sh>s->h)
	{
		sh = s->h - dy;
	}

	if (sw<=0 || sh<=0)
		return;

	memcpy( s->buf + (s->w+1)*dy + dx, str+sx, sw);

	if (s->color)
		memset( s->color + (s->w+1)*dy + dx, color, sw);
}

static void menu_fill(CON_OUTPUT* s, int dx, int dy, char glyph, char color, int sw, int sh, const int* clip = 0)
{
	if (clip)
	{
		if (dx<clip[0])
		{
			sw-=clip[0]-dx;
			dx=clip[0];
		}
		if (dy<clip[1])
		{
			sh-=clip[1]-dy;
			dy=clip[1];
		}
		if (dx+sw>clip[0]+clip[2])
		{
			sw=clip[0]+clip[2]-dx;
		}
		if (dy+sh>clip[1]+clip[3])
		{
			sh=clip[1]+clip[3]-dy;
		}
	}

	if (dx<0)
	{
		sw+=dx;
		dx=0;
	}
	if (dy<0)
	{
		sh+=dy;
		dy=0;
	}

	if (dx+sw>s->w)
	{
		sw = s->w - dx;
	}
	if (dy+sh>s->h)
	{
		sh = s->h - dy;
	}

	if (sw<=0 || sh<=0)
		return;

	for (int y=0; y<sh; y++)
		memset( s->buf + (s->w+1)*(dy+y) + dx, glyph, sw);

	if (s->color)
		for (int y=0; y<sh; y++)
			memset( s->color + (s->w+1)*(dy+y) + dx, color, sw);

}

static void menu_blit(CON_OUTPUT* s, int dx, int dy, const MENU_ASSET* a, int sx, int sy, int sw, int sh, int fr, const int* clip = 0)
{
	if (!a->frames)
		return;

	fr %= a->frames;

	const char* glyph = s->color ? a->shade[fr] : a->mono[fr];
	const char* color = s->color ? a->color[fr] : 0;

	if (clip)
	{
		if (dx<clip[0])
		{
			sx+=clip[0]-dx;
			sw-=clip[0]-dx;
			dx=clip[0];
		}
		if (dy<clip[1])
		{
			sy+=clip[1]-dy;
			sh-=clip[1]-dy;
			dy=clip[1];
		}
		if (dx+sw>clip[0]+clip[2])
		{
			sw=clip[0]+clip[2]-dx;
		}
		if (dy+sh>clip[1]+clip[3])
		{
			sh=clip[1]+clip[3]-dy;
		}
	}

	if (dx<0)
	{
		sx-=dx;
		sw+=dx;
		dx=0;
	}
	if (dy<0)
	{
		sy-=dy;
		sh+=dy;
		dy=0;
	}

	if (sx<0)
	{
		dx-=sx;
		sw+=sx;
		sx=0;
	}
	if (sy<0)
	{
		dy-=sy;
		sh+=sy;
		sy=0;
	}

	if (dx+sw>s->w)
	{
		sw = s->w - dx;
	}
	if (dy+sh>s->h)
	{
		sh = s->h - dy;
	}

	if (sx+sw>a->w)
	{
		sw = a->w-sx;
	}
	if (sy+sh>a->h)
	{
		sh = a->h-sy;
	}

	if (sw<=0 || sh<=0)
		return;

	for (int y=0; y<sh; y++)
		memcpy( s->buf + (s->w+1)*(dy+y) + dx, glyph + (a->w+1)*(sy+y) + sx, sw);

	if (color)
		for (int y=0; y<sh; y++)
			memcpy( s->color + (s->w+1)*(dy+y) + dx, color + (a->w+1)*(sy+y) + sx, sw);
}


static void menu_stretch(CON_OUTPUT* s, int dx, int dy, int dw, int dh, const MENU_ASSET* a, int fr, const int* clip = 0)
{
	int l = dx + a->l;
	int r = dx + dw - a->r;
	int t = dy + a->t;
	int b = dy + dh - a->b;

	if (l>r)
	{
		int avr = (l+r+1)/2;
		l = avr-dx;
		r = dx+dw-avr;
	}
	else
	{
		l=a->l;
		r=a->r;
	}

	if (t>b)
	{
		int avr = (t+b+1)/2;
		t = avr-dy;
		b = dy+dh-avr;
	}
	else
	{
		t=a->t;
		b=a->b;
	}

	// corners
	menu_blit(s, dx,dy, a, 0,0, l,t, fr, clip);
	menu_blit(s, dx+dw-r,dy, a, a->w-r,0, r,t, fr, clip);
	menu_blit(s, dx,dy+dh-b, a, 0,a->h-b, l,b, fr, clip);
	menu_blit(s, dx+dw-r,dy+dh-b, a, a->w-r,a->h-b, r,b, fr, clip);

	// v-edges
	for (int y = t; y<dh-b; y++)
	{
		menu_blit(s, dx,dy+y, a, 0,t, l,1, fr, clip);
		menu_blit(s, dx+dw-r,dy+y, a, a->w-r,t, r,1, fr, clip);
	}

	// h-edges
	for (int x = l; x<dw-r; x++)
	{
		menu_blit(s, dx+x,dy, a, l,0, 1,t, fr, clip);
		menu_blit(s, dx+x,dy+dh-b, a, r,a->h-b, 1,b, fr, clip);
	}

	// interior
	char glyph = s->color ? a->shade[fr][ (a->w+1)*t+l ] : a->mono[fr][ (a->w+1)*t+l ];
	char color = a->color ? a->color[fr][ (a->w+1)*t+l ] : 0;
	menu_fill(s,dx+l,dy+t,glyph,color,dw-l-r,dh-t-b, clip);
}


static MENU_ASSET window(&menu_wnd,5,5,2,3);
static MENU_ASSET module(&menu_mod,1,1,4,1);

static unsigned char cl_menu_a[3];
static unsigned char cl_menu_b[3];
static unsigned char cl_menu_c[3];

struct MODULE
{
	int x,y;     // current rounded

	float fx,fy; // current floating
	int dx,dy;   // destination (by update layout)

	int w,h;
	int col;

	bool dbl; // 2 cols module

	int state; // 0 inactive, 1 hover, 2 focus
	const char* title;

	int (*proc)(MODULE* m, int msg, void* p1, void* p2);

	MODULE* left;
	MODULE* right;
	MODULE* upper;
	MODULE* lower;

	// only if dbl is set!
	MODULE* upper2;
	MODULE* lower2;
};

struct WINDOW
{
	unsigned long time;

	int scroll;
	int smooth;
	float fsmooth;

	int height;
	int modules;
	MODULE* module;

	MODULE* focus;
	int state; // 0: hovering , 1: focused
	int focus_col;

	int center_x;
	int columns;
	const int* layout;

	int layout_w;
	int layout_h;
	const int* layout_2;
	const int* layout_3;
	const int* layout_4;
};

static WINDOW menu_window;

static MENU_ASSET backdrop(&menu_bkg);

static void UpdateLayout(CON_OUTPUT* s, bool force=false)
{
	WINDOW* mw = &menu_window;

	if (s->w==mw->layout_w && s->h==mw->layout_h && !force)
		return;

	// calc columns
	int cw = s->w - window.l - window.r - 3;

	int mod_width  = 31;
	int mod_space  = 2;
	int mod_left_mrg = 1;
	int mod_right_mrg = 1;

	// left , mod_left_mrg , (mods-1) x (mod,space) , mod, mod_right_mrg , scroll | right
	int mod_cols = (cw + mod_space - mod_left_mrg - mod_right_mrg) / (mod_width + mod_space);

	if (mw->modules < mod_cols)
	{
		int cols=0;
		for (int i=0; i<mw->modules; i++)
		{
			if (mw->module[i].dbl)
				cols+=2;
			else
				cols++;
		}

		mod_cols = MIN(mod_cols, cols);
	}

	// calc height (currently we don't reorder modules)
	int height[4]={0,0,0,0}; // total heights of each module column
	MODULE* bottom[2][4]={{0,0,0,0},{0,0,0,0}}; // last module added to given column

	const int* layout=0;
	switch (mod_cols)
	{
		case 2: layout = mw->layout_2; break;
		case 3: if (mw->layout_3) layout = mw->layout_3; else { mod_cols=2; layout = mw->layout_2; } break;
		case 4: if (mw->layout_4) layout = mw->layout_4; else if (mw->layout_3) { mod_cols=3; layout = mw->layout_3; } else { mod_cols=2; layout = mw->layout_2; } break;
	}

	int center_x = window.l + mod_left_mrg + (cw - ((mod_left_mrg + mod_right_mrg + mod_cols * mod_width + (mod_cols-1) * mod_space)|1) +1 ) / 2;

	if (mw->center_x<0)
		mw->center_x = center_x;

	mw->columns = mod_cols;
	mw->layout = layout;

	if (!mw->focus)
	{
		mw->focus = mw->module + layout[0];
		mw->focus->state = 1;
		mw->state = 0; // hover over first layout item
	}

	int col=0;
	int row=0;
	for (int j=0; j<mw->modules; j++)
	{
		int i = layout[j];

		mw->module[i].col = col;
		mw->module[i].dx = center_x + col * (mod_width + mod_space);

		mw->module[i].left = 0;
		mw->module[i].right = 0;
		mw->module[i].lower = 0;
		mw->module[i].lower2 = 0;
		mw->module[i].upper = 0;
		mw->module[i].upper2 = 0;

		if (mw->module+i == mw->focus)
			mw->focus_col = col;

		if (mw->module[i].dbl)
		{
			assert(col<mod_cols-1);

			mw->module[i].w = mod_width * 2 + mod_space;
			mw->module[i].dy = MAX(height[col],height[col+1]);

			height[col] = mw->module[i].dy + mw->module[i].h      +1; // extra spacing
			height[col+1] = mw->module[i].dy + mw->module[i].h    +1;

			mw->module[i].upper = bottom[0][col];
			mw->module[i].upper2 = bottom[0][col+1];

			if (row)
			{
				if (bottom[0][col]==bottom[0][col+1])
				{
					// |*-*|
					// |*-*|
					bottom[0][col]->lower = mw->module+i;
					bottom[0][col+1]->lower2 = mw->module+i;
				}
				else
				if (col>0 && col<mod_cols-2 &&
					bottom[0][col-1]==bottom[0][col] &&
					bottom[0][col+1]==bottom[0][col+2])
				{
					// |*-*|*-*|
					//   |*-*|
					bottom[0][col]->lower2 = mw->module+i;
					bottom[0][col+1]->lower = mw->module+i;
				}
				else
				if (col>0 &&
					bottom[0][col-1]==bottom[0][col])
				{
					// |*-*|*|
					//   |*-*|
					bottom[0][col]->lower2 = mw->module+i;
					bottom[0][col+1]->lower = mw->module+i;
				}
				else
				if (col<mod_cols-2 &&
					bottom[0][col+1]==bottom[0][col+2])
				{
					// |*|*-*|
					// |*-*|
					bottom[0][col]->lower = mw->module+i;
					bottom[0][col+1]->lower = mw->module+i;
				}
				else
				{
					// |*|*|
					// |*-*|
					bottom[0][col]->lower = mw->module+i;
					bottom[0][col+1]->lower = mw->module+i;
				}
			}

			bottom[1][col] = mw->module+i;
			bottom[1][col+1] = mw->module+i;

			col+=2;
			if (col==mod_cols)
			{
				col=0;
				row++;

				bottom[0][0] = bottom[1][0];
				bottom[0][1] = bottom[1][1];
				bottom[0][2] = bottom[1][2];
				bottom[0][3] = bottom[1][3];
			}
		}
		else
		{
			mw->module[i].w = mod_width;
			mw->module[i].dy = height[col];
			
			height[col] = mw->module[i].dy + mw->module[i].h      +1; // extra spacing

			mw->module[i].upper = bottom[0][col];

			if (row)
			{
				if (col>0 && 
					bottom[0][col-1]==bottom[0][col])
				{
					// |*-*|
					//   |*|
					bottom[0][col]->lower2 = mw->module+i;
				}
				else
				if (col<mod_cols-1 && 
					bottom[0][col]==bottom[0][col+1])
				{
					// |*-*|
					// |*|
					bottom[0][col]->lower = mw->module+i;
				}
				else
				{
					// |*|
					// |*|
					bottom[0][col]->lower = mw->module+i;
				}
			}

			bottom[1][col] = mw->module+i;

			col++;
			if (col==mod_cols)
			{
				col=0;
				row++;

				bottom[0][0] = bottom[1][0];
				bottom[0][1] = bottom[1][1];
				bottom[0][2] = bottom[1][2];
				bottom[0][3] = bottom[1][3];
			}
		}
	}

	int max_height = 0;
	for (int i=0; i<mod_cols; i++)
		if (max_height<height[i])
			max_height=height[i];

	mw->height = max_height -1 -1; // -1 as underscores on top of layout don't count! and another -1 for extra space removal

	int center_y = ( s->h-window.t-window.b-1 - max_height +1 )/2;
	if (center_y<0)
		center_y=window.t;
	else
		center_y+=window.t;

	// change it to ensure focus/hover visibility
	int client_h = s->h - window.t - window.b;
	if (mw->scroll>mw->height-client_h+1)
		mw->scroll=mw->height-client_h+1;
	if (mw->scroll<0)
		mw->scroll=0;

	for (int i=0; i<mw->modules; i++)
	{
		mw->module[i].dy += center_y;
		if (mw->module[i].y<0)
		{
			mw->module[i].y = mw->module[i].dy;
			mw->module[i].fx = (float)mw->module[i].dy;
		}

		if (mw->module[i].x<0)
		{
			mw->module[i].x = mw->module[i].dx;
			mw->module[i].fx = (float)mw->module[i].dx;
		}
		else
		{
			/*
			mw->module[i].x += center_x - mw->center_x;
			mw->module[i].fx += (float)(center_x - mw->center_x);
			*/
		}
	}

	mw->center_x = center_x;

	// determine cols and rows
	// assign x,y 
	// stretch to cutify layout if possible

	mw->layout_w = s->w;
	mw->layout_h = s->h;

	mw->smooth = mw->scroll;

	// build horizontal graph for hovering
	for (int j=0; j<mw->modules; j++)
	{
		// calc center

		int left = -1;
		int right = -1;

		int left_err = 0;
		int right_err = 0;

		int yj = mw->module[j].dy + mw->module[j].h/2;

		for (int i=0; i<mw->modules; i++)
		{
			if (mw->module[j].col == mw->module[i].col)
				continue;

			if (!mw->module[i].dbl && mw->module[i].col == mw->module[j].col-1 ||
				mw->module[i].dbl && mw->module[i].col == mw->module[j].col-2)
			{
				// j->left = i
				int err = ABS(mw->module[i].dy + mw->module[i].h/2 - yj);
				if (left<0 || err<left_err)
				{
					left = i;
					left_err = err;
				}
			}
			else
			if (!mw->module[j].dbl && mw->module[i].col == mw->module[j].col+1 ||
				mw->module[j].dbl && mw->module[i].col == mw->module[j].col+2)
			{
				// j->right = i
				int err = ABS(mw->module[i].dy + mw->module[i].h/2 - yj);
				if (right<0 || err<right_err)
				{
					right = i;
					right_err = err;
				}
			}
		}

		if (left>=0)
			mw->module[j].left = mw->module + left;
		if (right>=0)
			mw->module[j].right = mw->module + right;
	}

	// smooth scroll to hovered mod
	if (mw->focus->dy - window.t - mw->scroll < 0)
		mw->scroll = mw->focus->dy - window.t;
	if (mw->focus->dy+mw->focus->h - window.t - mw->scroll > client_h)
		mw->scroll = mw->focus->dy+mw->focus->h - window.t - client_h;
}

enum MODULE_MSG
{
	MM_INIT,  // return height if ok 
	MM_LOAD,  // overwrite menu data with conf
	MM_INPUT, // p1 contains single CON_INPUT
	MM_PAINT, // p1 contains CON_OUTPUT, p2 contains clip rect to apply, use menu_window.scroll

	MM_FOCUS, 
	// p1 is flag indicating if focus is being gained or lost, 
	// if focus is being lost, p2 MUST be filled with Y coordinate of focused element in the module
	// if focus is being gained p2 contains Y coordinate of previously focused element in module the focus is comming from
};

#define LABEL_CL(m,h,f) ((m)->state==2) ? ((h)&&!(f) ? cl_menu_c[(m)->state] : cl_menu_b[(m)->state] ) : cl_menu_a[(m)->state];
#define VALUE_CL(m,h,f) ((m)->state==2) ? ((h)&&(f) ? cl_menu_c[(m)->state] : cl_menu_b[(m)->state] ) : cl_menu_b[(m)->state];

void PaintScroll(CON_OUTPUT* s, int x, int dy, int h, int pos, int size, int state, const int* clip);


int ControlProc(MODULE* m, int msg, void* p1, void* p2)
{

	static MENU_ASSET sld(&menu_sld,0,0,0,0);
	static MENU_ASSET swt(&menu_swt,0,0,0,0);
	static MENU_ASSET tag(&menu_tag,0,0,0,0);

	struct ControlData
	{
		int hover;
		int mus_vol;
		int sfx_vol;
		int color;
		int sticky;

		static void PaintSwt(CON_OUTPUT* s, int x, int y, int w, int fr, int val, int* clip)
		{
			if (w<5)
				w=5;
			int fill = w-4;

			if (!val)
			{
				menu_blit(s,x,y,&swt,0,0,1,2,fr,clip);
				menu_blit(s,x+1,y,&swt,swt.w/2-1,0,2,2,fr,clip);
				for (int f=x+3; f<x+3+fill; f++)
					menu_blit(s,f,y,&swt,swt.w-2,0,1,2,fr,clip);
				menu_blit(s,x+w-1,y,&swt,swt.w-1,0,1,2,fr,clip);
			}
			else
			{
				menu_blit(s,x,y,&swt,0,0,1,2,fr,clip);
				for (int f=x+1; f<x+1+fill; f++)
					menu_blit(s,f,y,&swt,1,0,1,2,fr,clip);
				menu_blit(s,x+w-3,y,&swt,swt.w/2-1,0,2,2,fr,clip);
				menu_blit(s,x+w-1,y,&swt,swt.w-1,0,1,2,fr,clip);
			}
		}

		static void PaintSld(CON_OUTPUT* s, int x, int y, int w, int fr, int val, int* clip)
		{
			if (w<5)
				w=5;
			int fill = w-4;

			menu_blit(s,x,y,&sld,0,0,1,2,fr,clip);
			for (int f=x+1; f<x+1+val; f++)
				menu_blit(s,f,y,&sld,1,0,1,2,fr,clip);
			menu_blit(s,x+1+val,y,&sld,sld.w/2-1,0,2,2,fr,clip);
			for (int f=x+3+val; f<x+w-1; f++)
				menu_blit(s,f,y,&sld,sld.w-2,0,1,2,fr,clip);
			menu_blit(s,x+w-1,y,&sld,sld.w-1,0,1,2,fr,clip);
		}
	};

	static ControlData data;

	switch (msg)
	{
		case MM_LOAD:
		{
			data.mus_vol = conf_control.mus_vol;
			data.sfx_vol = conf_control.sfx_vol;
			data.sticky = conf_control.sticky;
			data.color = conf_control.color;

			set_gain((data.mus_vol*100+16)/32,(data.sfx_vol*100+16)/32);
			SetColorMode(data.color?0x70:0x00);

			break;
		}

		case MM_INIT:
		{
			data.hover = 0;

			data.color = 1;
			data.sticky = 0;
			data.mus_vol = 16; // 0..32
			data.sfx_vol = 16; // 0..32
			set_gain((data.mus_vol*100+16)/32,(data.sfx_vol*100+16)/32);

			return 20;
		}

		case MM_FOCUS:
		{
			int f = *(int*)p1;
			break;
		}

		case MM_PAINT:
		{
			CON_OUTPUT* s = (CON_OUTPUT*)p1;
			int* clip = (int*)p2;

			int x = m->x + (module.l+2);
			int y = m->y + (module.t+1) - menu_window.smooth;

			int hover = data.hover;
			y++;
			x+=2;

			unsigned char lab_cl;
			unsigned char val_cl = VALUE_CL(m,true,true);
			int l;
			char val[32];
			
			lab_cl = LABEL_CL(m,hover==0,false);
			if (m->state==2 && hover==0)
				menu_print(s, x-2, y,  ">", cl_menu_c[m->state], 1, clip);
			menu_print(s,x,y,"Music Volume",lab_cl, 12, clip);
			ControlData::PaintSld(s,x+14,y-1,36,m->state==2?0:1,data.mus_vol,clip);
			l=sprintf_s(val,32,"%d%%",(data.mus_vol*100+16)/32);
			menu_print(s, x+51, y, val, val_cl, l, clip);
			y+=2;

			lab_cl = LABEL_CL(m,hover==1,false);
			if (m->state==2 && hover==1)
				menu_print(s, x-2, y,  ">", cl_menu_c[m->state], 1, clip);
			menu_print(s,x,y,"SFX Volume",lab_cl, 10, clip);
			ControlData::PaintSld(s,x+14,y-1,36,m->state==2?0:1,data.sfx_vol,clip);
			l=sprintf_s(val,32,"%d%%",(data.sfx_vol*100+16)/32);
			menu_print(s, x+51, y, val, val_cl, l, clip);
			y+=2;

			lab_cl = LABEL_CL(m,hover==2,false);
			if (m->state==2 && hover==2)
				menu_print(s, x-2, y,  ">", cl_menu_c[m->state], 1, clip);
			menu_print(s,x,y,"Sticky Input",lab_cl, 12, clip);
			ControlData::PaintSwt(s,x+14,y-1,8,m->state==2?0:1,data.sticky,clip);
			l=sprintf_s(val,32,"%s",data.sticky ? "Enabled" : "Disabled");
			menu_print(s, x+23, y, val, val_cl, l, clip);
			y+=2;

			lab_cl = LABEL_CL(m,hover==3,false);
			if (m->state==2 && hover==3)
				menu_print(s, x-2, y,  ">", cl_menu_c[m->state], 1, clip);
			menu_print(s,x,y,"Color Mode",lab_cl, 10, clip);
			ControlData::PaintSwt(s,x+14,y-1,8,m->state==2?0:1,data.color,clip);
			l=sprintf_s(val,32,"%s",data.color ? "16 Colors" : "Black & White");
			menu_print(s, x+23, y, val, val_cl, l, clip);
			y+=3;
			x+=3;

			lab_cl = LABEL_CL(m,hover==4,false);
			if (m->state==2 && hover==4)
				menu_print(s, x-5, y,  ">", cl_menu_c[m->state], 1, clip);
			menu_print(s,x,y,"EXIT TO OS",lab_cl, 10, clip);
			menu_blit(s,x-3,y-1,&tag,0,0,2,2,m->state==2?0:1,clip);
			y+=2;

			lab_cl = LABEL_CL(m,hover==5,false);
			if (m->state==2 && hover==5)
				menu_print(s, x-5, y,  ">", cl_menu_c[m->state], 1, clip);
			menu_print(s,x,y,"RESET CAMPAIGN PROGRESS",lab_cl, 23, clip);
			menu_blit(s,x-3,y-1,&tag,0,0,2,2,m->state==2?0:1,clip);
			y+=2;

			break;
		}

		case MM_INPUT:
		{
			CON_INPUT* ci = (CON_INPUT*)p1;
			if (ci->EventType == CON_INPUT_KBD && ci->Event.KeyEvent.bKeyDown)
			{
				int delta=1;
				switch (ConfMapInput(ci->Event.KeyEvent.uChar.AsciiChar))
				{
					case KBD_UP:
					{
						if (data.hover>0)
						{
							data.hover--;
							return 1;
						}
						break;
					}
					case KBD_DN:
					{
						if (data.hover<5)
						{
							data.hover++;
							return 1;
						}
						break;
					}

					case KBD_LT:
						delta = -1;
						// nobreak;

					case KBD_RT:
					{
						switch (data.hover)
						{
							case 0:
							{
								if (data.mus_vol+delta>=0 && data.mus_vol+delta<=32)
								{
									data.mus_vol+=delta;
									set_gain((data.mus_vol*100+16)/32,-1);

									conf_control.mus_vol = data.mus_vol;
									SaveConf();
									return 1;
								}
								break;
							}
							case 1:
							{
								if (data.sfx_vol+delta>=0 && data.sfx_vol+delta<=32)
								{
									data.sfx_vol+=delta;
									set_gain(-1,(data.sfx_vol*100+16)/32);

									conf_control.sfx_vol = data.sfx_vol;
									SaveConf();
									return 1;
								}
								break;
							}

							case 2:
							{
								if (data.sticky+delta>=0 && data.sticky+delta<=1)
								{
									data.sticky+=delta;

									conf_control.sticky = data.sticky;
									SaveConf();
									return 1;
								}
								break;
							}

							case 3:
							{
								if (data.color+delta>=0 && data.color+delta<=1)
								{
									data.color+=delta;
									SetColorMode(data.color?0x70:0x00);

									conf_control.color = data.color;
									SaveConf();
									return 1;
								}
								break;
							}
						}

						break;
					}

					case 13:
					{
						switch (data.hover)
						{
							case 2:
							{
								data.sticky = !data.sticky;

								conf_control.sticky = data.sticky;
								SaveConf();
								return 1;
							}
							case 3:
							{
								data.color = !data.color;
								SetColorMode(data.color?0x70:0x00);

								conf_control.color = data.color;
								SaveConf();
								return 1;
							}

							case 4:
							{
								app_exit();
								return 1;
							}

							case 5:
							{
								conf_campaign.course=0;
								conf_campaign.level=-1;
								conf_campaign.passed=0;

								SaveConf();
								return 1;
							}
						}

						break;
					}
				}
			}
			break;
		}
	}

	return 0;
}

int ScoreProc(MODULE* m, int msg, void* p1, void* p2)
{
	struct ScoreData
	{
		int ofs;
		int ofs_pending;
		char id[16];
	};

	static ScoreData data;

	switch (msg)
	{
		case MM_LOAD:
		{
			break;
		}

		case MM_INIT:
		{
			data.ofs = 0;
			data.ofs_pending = 0;
			data.id[0] = 0;
			return 20;
		}

		case MM_FOCUS:
		{
			int f = *(int*)p1;
			break;
		}

		case MM_PAINT:
		{
			CON_OUTPUT* s = (CON_OUTPUT*)p1;
			int* clip = (int*)p2;

			bool done = update_hiscore();
			if (data.ofs>hiscore.tot-12)
				data.ofs=hiscore.tot-12;
			if (data.ofs<0)
				data.ofs=0;

			if (strcmp(data.id,hiscore.id))
			{
				memcpy(data.id,hiscore.id,16);
				int row=hiscore.siz/2;
				for (int i=0; i<hiscore.siz; i++)
				{
					if (hiscore.buf[55*i+16]=='>')
					{
						row=i;
						break;
					}
				}
				data.ofs = hiscore.ofs + row - 6;
				if (data.ofs>hiscore.tot-12)
					data.ofs=hiscore.tot-12;
				if (data.ofs<0)
					data.ofs=0;
				data.ofs_pending = hiscore.ofs;
			}

			int x = m->x + (module.l+2);
			int y = m->y + (module.t+1) - menu_window.smooth;

			int dx = 2;

			unsigned char cl;
			
			cl = LABEL_CL(m,true,false);
			const static char head[] = "  Rank   Name               Score     Date      Time  ";
			menu_print(s,x+dx,y,head,cl,54,clip);

			cl = LABEL_CL(m,true,true);
			const static char line[] = "-------- ---------------- -------- ---------- --------";
			menu_print(s,x+dx,y+1,line,cl,54,clip);

			for (int dy=0; dy<12; dy++)
			{
				unsigned char cl = LABEL_CL(m,true,false);

				int rank = 1 + dy + data.ofs;
				char str[10];
				sprintf_s(str,10,"%8d ",rank);

				int row = dy + data.ofs - hiscore.ofs;
				if (row>=0 && row<hiscore.siz)
				{
					cl = (hiscore.buf[55*row+16]=='>') ? 0x1F : LABEL_CL(m,true,false);
					menu_print(s,x+dx,y+dy+2,str,cl,9,clip); // dy+2, makes space for table header

					cl = (hiscore.buf[55*row+16]=='>') ? 0x1F : LABEL_CL(m,true,true);
					menu_print(s,x+dx+9,y+dy+2,hiscore.buf+55*row,cl,45,clip); // dy+2, makes space for table header
				}
				else
				{
					cl = LABEL_CL(m,true,false);
					menu_print(s,x+dx,y+dy+2,str,cl,9,clip); // dy+2, makes space for table header
				}

				if (s->color)
				{
					if (rank==1)
					{
						cl = 0xB0;
					}
					else
					if (rank==2)
					{
						cl = 0x70;
					}
					else
					if (rank==3)
					{
						cl = 0x30;
					}
					else
					if (rank<=10)
					{
						cl = 0x60;
					}
					else
					if (rank<=100)
					{
						cl = 0x20;
					}
					else
					if (rank<=1000)
					{
						cl = 0xC0;
					}
					else
					if (rank<=10000)
					{
						cl = 0x48;
					}
					else
					if (rank<=100000)
					{
						cl = 0x08;
					}
					else
					{
						cl = 0x88;
					}

					menu_print(s,x+dx,y+dy+2,"__",cl,2,clip);
				}
			}

			if (!done)
			{
				// animate 
				// ...
			}

			// paint scroll
			PaintScroll(s,x+57,y+1,12,data.ofs,hiscore.tot,m->state,clip);

			break;
		}
		case MM_INPUT:
		{
			CON_INPUT* ci = (CON_INPUT*)p1;
			if (ci->EventType == CON_INPUT_KBD && ci->Event.KeyEvent.bKeyDown)
			{
				switch (ConfMapInput(ci->Event.KeyEvent.uChar.AsciiChar))
				{
					case KBD_END:
					{
						if (hiscore_sync)
							break;

						if (data.ofs < hiscore.tot - 12)
						{
							data.ofs = hiscore.tot - 12;
							if (data.ofs - data.ofs_pending > 3*13)
							{
								int ofs = data.ofs - 13;
								if (ofs>hiscore.tot-65)
									ofs=hiscore.tot-65;
								if (ofs<0)
									ofs=0;

								if (ofs != data.ofs_pending)
								{
									data.ofs_pending = ofs;
									get_hiscore(data.ofs_pending, hiscore.id);

									/*
									char dbg[100];
									sprintf_s(dbg,100,"FETCHING: (%d-%d), (center=%d)\n",data.ofs_pending,data.ofs_pending+65, data.ofs+6);
									OutputDebugStringA(dbg);
									*/
								}
							}
							return 1;
						}

						break;
					}
					case KBD_PDN:
					{
						if (hiscore_sync)
							break;

						if (data.ofs < hiscore.tot - 12)
						{
							data.ofs += 11;
							if (data.ofs > hiscore.tot - 12)
								data.ofs = hiscore.tot - 12;

							if (data.ofs - data.ofs_pending > 3*13)
							{
								int ofs = data.ofs - 13;
								if (ofs>hiscore.tot-65)
									ofs=hiscore.tot-65;
								if (ofs<0)
									ofs=0;

								if (ofs != data.ofs_pending)
								{
									data.ofs_pending = ofs;
									get_hiscore(data.ofs_pending, hiscore.id);

									/*
									char dbg[100];
									sprintf_s(dbg,100,"FETCHING: (%d-%d), (center=%d)\n",data.ofs_pending,data.ofs_pending+65, data.ofs+6);
									OutputDebugStringA(dbg);
									*/
								}
							}
							return 1;
					
						}

						break;
					}

					case KBD_DN:
					{
						if (hiscore_sync)
							break;

						if (data.ofs < hiscore.tot - 12)
						{
							data.ofs++;

							if (data.ofs - data.ofs_pending > 3*13)
							{
								int ofs = data.ofs - 13;
								if (ofs>hiscore.tot-65)
									ofs=hiscore.tot-65;
								if (ofs<0)
									ofs=0;

								if (ofs != data.ofs_pending)
								{
									data.ofs_pending = ofs;
									get_hiscore(data.ofs_pending, hiscore.id);

									/*
									char dbg[100];
									sprintf_s(dbg,100,"FETCHING: (%d-%d), (center=%d)\n",data.ofs_pending,data.ofs_pending+65, data.ofs+6);
									OutputDebugStringA(dbg);
									*/
								}
							}

							return 1;
						}
						break;
					}

					case KBD_HOM:
					{
						if (hiscore_sync)
							break;

						if (data.ofs > 0)
						{
							data.ofs=0;

							if (data.ofs - data.ofs_pending < 13)
							{
								int ofs = data.ofs - 3*13;
								if (ofs>hiscore.tot-65)
									ofs=hiscore.tot-65;
								if (ofs<0)
									ofs=0;

								if (ofs != data.ofs_pending)
								{
									data.ofs_pending = ofs;
									get_hiscore(data.ofs_pending, hiscore.id);

									/*
									char dbg[100];
									sprintf_s(dbg,100,"FETCHING: (%d-%d), (center=%d)\n",data.ofs_pending,data.ofs_pending+65, data.ofs+6);
									OutputDebugStringA(dbg);
									*/
								}
							}

							return 1;
						}

						break;
					}

					case KBD_PUP:
					{
						if (hiscore_sync)
							break;

						if (data.ofs > 0)
						{
							data.ofs-=11;
							if (data.ofs<0)
								data.ofs=0;

							if (data.ofs - data.ofs_pending < 13)
							{
								int ofs = data.ofs - 3*13;
								if (ofs>hiscore.tot-65)
									ofs=hiscore.tot-65;
								if (ofs<0)
									ofs=0;

								if (ofs != data.ofs_pending)
								{
									data.ofs_pending = ofs;
									get_hiscore(data.ofs_pending, hiscore.id);

									/*
									char dbg[100];
									sprintf_s(dbg,100,"FETCHING: (%d-%d), (center=%d)\n",data.ofs_pending,data.ofs_pending+65, data.ofs+6);
									OutputDebugStringA(dbg);
									*/
								}
							}

							return 1;
						}

						break;
					}


					case KBD_UP:
					{
						if (hiscore_sync)
							break;

						if (data.ofs > 0)
						{
							data.ofs--;

							if (data.ofs - data.ofs_pending < 13)
							{
								int ofs = data.ofs - 3*13;
								if (ofs>hiscore.tot-65)
									ofs=hiscore.tot-65;
								if (ofs<0)
									ofs=0;

								if (ofs != data.ofs_pending)
								{
									data.ofs_pending = ofs;
									get_hiscore(data.ofs_pending, hiscore.id);

									/*
									char dbg[100];
									sprintf_s(dbg,100,"FETCHING: (%d-%d), (center=%d)\n",data.ofs_pending,data.ofs_pending+65, data.ofs+6);
									OutputDebugStringA(dbg);
									*/
								}
							}

							return 1;
						}
						break;
					}
				}
			}
			break;
		}

	}

	return 0;
}

int ProfileProc(MODULE* m, int msg, void* p1, void* p2)
{
	struct ProfileData
	{
		int hover;			  // 0:name, 1:rnd_avatar, 2,3,4,5:dna(0-3)

		char name[16];		  // maxlen = 15
		int edit_pos;		  // -1:no_edit, otherwise cursor pos
		char edit_buf[16];    // edit temp

		// current (destination dna)
		unsigned long avatar; // 4x8bits (4,1,1,2) cells verticaly

		int prev[4]; // dna positions at the start of anim
		unsigned long anim[4]; // time base for dna animation

		int shift_time; // avatar shift base time
		int shift_from; // avatar shift from
		int shift_to; // avatar shift from

		int get_shift(int mod_state, unsigned long t)
		{
			if (shift_from==shift_to)
				return shift_to;

			int dt = t - shift_time;
			int pos = shift_from + (shift_to-shift_from)*dt/128;
			if (pos<0)
				pos=0;
			if (pos>6)
				pos=6;

			if (shift_to == pos)
				shift_from = pos;

			return pos;
		}

		int get_anim(int x, unsigned long t, int w, int n)
		{
			// t = time to evaluate anim at
			// x = channel(0..3)
			// w = dna.w
			// n = dna.frames

			int len = w*n;
			int dest = w*(( avatar >> (8*x) ) & 0xFF);
			int from = prev[x];

			if (dest==from)
				return dest;

			int fr = (t - anim[x])/16;

			if (fr<=0)
				return from;

			// find shorter dir
			int dist1 = dest-from; // forward
			int dist2 = from-dest;

			if (dist1<0)
				dist1 += len;
			if (dist2<0)
				dist2 += len;

			int dir,dist;
			if (dist1<dist2)
			{
				dir = 1;
				dist = dist1;
			}
			else
			{
				dir = -1;
				dist = dist2;
			}

			// assume anim must fit in 20fr
			if (fr>=20)
				return dest;

			int ret = from + (dir*dist*fr+10)/20;
			if (ret<0)
				ret += len;
			if (ret>=len)
				ret -= len;

			return ret;

			/*
			if (fr>=dist)
				return dest;


			int ret = from + dir*fr;
			if (ret<0)
				ret += len;
			if (ret>=len)
				ret -= len;

			return ret;
			*/
		}
	};

	static MENU_ASSET dna(&avatar,0,0,0,0);
	static MENU_ASSET frame(&menu_frm,1,1,1,1);

	static ProfileData data;

	switch (msg)
	{
		case MM_LOAD:
		{
			strcpy_s(data.name,16,conf_player.name);
			data.avatar = conf_player.avatar;
			break;
		}

		case MM_INIT:
		{
			strcpy_s(data.name,16,conf_player.name);
			data.avatar = conf_player.avatar;
			/*
			data.avatar = (data.avatar<<8) | ( rand()%dna.frames );
			data.avatar = (data.avatar<<8) | ( rand()%dna.frames );
			data.avatar = (data.avatar<<8) | ( rand()%dna.frames );
			data.avatar = (data.avatar<<8) | ( rand()%dna.frames );
			*/

			data.prev[0] = ((data.avatar>>0) & 0xFF)*dna.w;
			data.prev[1] = ((data.avatar>>8) & 0xFF)*dna.w;
			data.prev[2] = ((data.avatar>>16) & 0xFF)*dna.w;
			data.prev[3] = ((data.avatar>>24) & 0xFF)*dna.w;

			data.hover = 0;
			data.edit_pos = -1;

			data.shift_from = 0;
			data.shift_to = 0;

			return 20;//19;
		}

		case MM_FOCUS:
		{
			int f = *(int*)p1;
			data.shift_from = data.get_shift(m->state,menu_window.time);
			data.shift_time = menu_window.time;
			data.shift_to = f==2 ? 6:0;
			break;
		}

		case MM_PAINT:
		{
			CON_OUTPUT* s = (CON_OUTPUT*)p1;
			int* clip = (int*)p2;

			int x = m->x + (module.l+2);
			int y = m->y + (module.t+1) - menu_window.smooth;

			if (m->state==2 && data.hover==0)
				menu_print(s, x, y,  ">", cl_menu_c[m->state], 1, clip);

			unsigned char cl = LABEL_CL(m,data.hover==0,data.edit_pos>=0);
			menu_print(s, x+2, y, "Name:", cl, 5, clip);

			char* name = data.name;
			if (data.edit_pos>=0)
				name = data.edit_buf;
			
			cl = VALUE_CL(m,data.hover==0,data.edit_pos>=0);
			menu_print(s, x+8, y, name, cl, strlen(name), clip);

			if (data.edit_pos>=0)
			{
				if ((menu_window.time & 0xFF) < 0x7F)
				{
					// show caret: in color mode, blink alternating fg/bg, in mono replace char with _
					if (s->color)
					{
						unsigned char* c = (unsigned char*)s->color + (s->w+1)*y + x+8 + data.edit_pos;
						*c = 0xF0; //((*c&0xF)<<4) | ((*c>>4)&0xF); 
					}
					else
					{
						char* c = s->buf + (s->w+1)*y + x+8 + data.edit_pos;
						*c = '_';
					}
				}
				// 
			}

			y+=2;

			cl = LABEL_CL(m,data.hover>0,0);
			menu_print(s, x+2, y, "Avatar:", cl, 7, clip);

			y++;

			///////////

			int ofs[4] = {0,4,5,6};
			int siz[4] = {4,1,1,2};
			static const char* nam[4]=
			{
				"Hair",
				"Eyes",
				"Nose",
				"Chin"
			};

			int row = y+2;

			if (m->state==2 && data.hover==1)
				menu_print(s, x, row, ">", cl_menu_c[m->state], 1, clip);

			cl = LABEL_CL(m,data.hover==1,0);
			menu_print(s, x+2, row, "Full", cl, 4, clip);

			row+=2;

			if (s->color)
			{
				for (int i=0; i<4; i++)
				{

					if (m->state==2 && data.hover==i+2)
						menu_print(s, x+2  -(2), row+i, ">", cl_menu_c[m->state], 1, clip);

					cl = LABEL_CL(m,data.hover==i+2,0);
					menu_print(s, x+2  -(2)+2, row+i, nam[i], cl, strlen(nam[i]), clip);
				}
			}
			else
			{
				for (int i=0; i<4; i++)
				{
					if (m->state==2 && data.hover==i+2)
						menu_print(s, x+2 -(2), row+i, ">", 0, 1, clip);

					menu_print(s, x+2  -(2)+2, row+i, nam[i], 0, strlen(nam[i]), clip);
				}
			}

			///////////

			int shift = data.get_shift(m->state,menu_window.time);

			menu_blit(s,x+shift-2 +(3),y,&frame,0,0,frame.w,frame.h,m->state==2,clip);

			y++;

			for (int i=0; i<4; i++)
			{
				int a = data.get_anim(i, menu_window.time, dna.w, dna.frames);

				int r = a%dna.w;
				int q = a/dna.w;
				if (r == 0)
				{
					// paint single frame
					menu_blit(s,x+shift +(3),y+ofs[i],&dna,0,ofs[i],dna.w,siz[i],q,clip);
				}
				else
				{
					int q2 = q+1;
					if (q2==dna.frames)
						q2=0;

					menu_blit(s,x+shift +(3),y+ofs[i],&dna,r,ofs[i],dna.w-r,siz[i],q,clip);
					menu_blit(s,x+shift+dna.w-r +(3),y+ofs[i],&dna,0,ofs[i],r,siz[i],q2,clip);
				}
			}

			return 1;
		}

		case MM_INPUT:
		{
			CON_INPUT* ci = (CON_INPUT*)p1;
			if (ci->EventType == CON_INPUT_KBD && ci->Event.KeyEvent.bKeyDown)
			{
				if (data.edit_pos>=0)
				{
					if (ci->Event.KeyEvent.uChar.AsciiChar>=32 && ci->Event.KeyEvent.uChar.AsciiChar<127)
					{
						if (data.edit_pos<15)
						{
							char p = data.edit_buf[data.edit_pos];
							data.edit_buf[data.edit_pos] = ci->Event.KeyEvent.uChar.AsciiChar;

							for (int i=data.edit_pos+1; true; i++)
							{
								if (i==15)
								{
									if (data.edit_pos<15)
										data.edit_pos++;
									data.edit_buf[i]=0;
									break;
								}

								char c = data.edit_buf[i];
								data.edit_buf[i]=p;
								if (!p)
								{
									data.edit_pos++;
									break;
								}
								p = c;
							}
						}
					}
					else
					if (ci->Event.KeyEvent.uChar.AsciiChar>0 && ci->Event.KeyEvent.uChar.AsciiChar<32)
					{
						switch (ci->Event.KeyEvent.uChar.AsciiChar)
						{
							case KBD_LT:
								if (data.edit_pos>0)
								{
									data.edit_pos--;
									return 1;
								}
								break;
							case KBD_RT:
								if (data.edit_buf[data.edit_pos])
								{
									data.edit_pos++;
									return 1;
								}
								break;

							case 27:
								data.edit_pos = -1;
								return 1;

							case 13:
							case KBD_UP:
							case KBD_DN:
								strcpy_s(data.name,16,data.edit_buf);
								data.edit_pos = -1;

								strcpy_s(conf_player.name,16,data.edit_buf);
								SaveConf();

								return 0;

							case KBD_PUP:
							case KBD_HOM:
								data.edit_pos = 0;
								return 1;

							case KBD_PDN:
							case KBD_END:
								data.edit_pos = strlen(data.edit_buf);
								return 1;

							case KBD_DEL:
							{
								if (data.edit_buf[data.edit_pos])
								{
									for (int i=data.edit_pos; true; i++)
									{
										char c = data.edit_buf[i+1];
										data.edit_buf[i] = c;
										if (!c)
											break;
									}
								}
								break;
							}

							case 8:
							{
								if (data.edit_pos>0)
								{
									for (int i=data.edit_pos; true; i++)
									{
										char c = data.edit_buf[i];
										data.edit_buf[i-1] = c;
										if (!c)
											break;
									}

									data.edit_pos--;
								}
								break;
							}
						}
					}

					return 1;
				}

				switch (ConfMapInput(ci->Event.KeyEvent.uChar.AsciiChar))
				{
					case KBD_LT:
					{
						if (data.hover>1)
						{
							int c = data.hover-2;
							int b = 8*c;

							// calc current interpolation for hover ch, store in prev
							data.prev[c] = data.get_anim(c, menu_window.time, dna.w, dna.frames);
							data.anim[c] = menu_window.time; // start new timer

							// update persistent destination
							int i = ( data.avatar >> b ) & 0xFF;
							i--;
							if (i==-1)
								i=dna.frames-1;
							data.avatar &= ~(0xFF << b);
							data.avatar |= i << b;

							conf_player.avatar=data.avatar;
							SaveConf();

							return 1;
						}
						else
						if (data.hover==1)
						{
							for (int c=0; c<4; c++)
							{
								int b = 8*c;

								// calc current interpolation for hover ch, store in prev
								data.prev[c] = data.get_anim(c, menu_window.time, dna.w, dna.frames);
								data.anim[c] = menu_window.time; // start new timer

								// update persistent destination
								int i = ( data.avatar >> b ) & 0xFF;
								i--;
								if (i==-1)
									i=dna.frames-1;
								data.avatar &= ~(0xFF << b);
								data.avatar |= i << b;

								conf_player.avatar=data.avatar;
								SaveConf();
							}

							return 1;
						}

						break;
					}

					case KBD_RT:
					{
						if (data.hover>1)
						{
							int c = data.hover-2;
							int b = 8*c;

							// calc current interpolation for hover ch, store in prev
							data.prev[c] = data.get_anim(c, menu_window.time, dna.w, dna.frames);
							data.anim[c] = menu_window.time; // start new timer

							int i = ( data.avatar >> b ) & 0xFF;
							i++;
							if (i==dna.frames)
								i=0;
							data.avatar &= ~(0xFF << b);
							data.avatar |= i << b;

							conf_player.avatar=data.avatar;
							SaveConf();

							return 1;
						}
						else
						if (data.hover==1)
						{
							for (int c=0; c<4; c++)
							{
								int b = 8*c;

								// calc current interpolation for hover ch, store in prev
								data.prev[c] = data.get_anim(c, menu_window.time, dna.w, dna.frames);
								data.anim[c] = menu_window.time; // start new timer

								int i = ( data.avatar >> b ) & 0xFF;
								i++;
								if (i==dna.frames)
									i=0;
								data.avatar &= ~(0xFF << b);
								data.avatar |= i << b;

								conf_player.avatar=data.avatar;
								SaveConf();

							}

							return 1;
						}

						break;
					}

					case KBD_UP:
					{
						if (data.hover>0)
						{
							data.hover--;
							return 1;
						}

						break;
					}

					case KBD_DN:
					{
						if (data.hover<5)
						{
							data.hover++;
							return 1;
						}

						break;
					}

					case 13:
					{
						if (data.hover==0)
						{
							data.edit_pos = strlen(data.name);
							strcpy_s(data.edit_buf,16,data.name);
							return 1;
						}

						if (data.hover==1)
						{
							for (int c=0; c<4; c++)
							{
								// calc current interpolation for hover ch, store in prev
								data.prev[c] = data.get_anim(c, menu_window.time, dna.w, dna.frames);
								data.anim[c] = menu_window.time; // start new timer
							}

							data.avatar = 0;

							data.avatar = (data.avatar<<8) | ( rand()%dna.frames );
							data.avatar = (data.avatar<<8) | ( rand()%dna.frames );
							data.avatar = (data.avatar<<8) | ( rand()%dna.frames );
							data.avatar = (data.avatar<<8) | ( rand()%dna.frames );

							conf_player.avatar=data.avatar;
							SaveConf();

							return 1;
						}
						else
						{
							int c = data.hover-2;
							int b = 8*c;

							// calc current interpolation for hover ch, store in prev
							data.prev[c] = data.get_anim(c, menu_window.time, dna.w, dna.frames);
							data.anim[c] = menu_window.time; // start new timer

							int i = rand()%dna.frames;
							data.avatar &= ~(0xFF << b);
							data.avatar |= i << b;

							conf_player.avatar=data.avatar;
							SaveConf();

							return 1;
						}

						break;
					}
				}
			}

		}
	}

	return 0;
}

static int SGN(int x)
{
	return x==0 ? 0 : x>0 ? 1 : -1;
}

// defined in temp
void ClearOnHold();
bool GameOnHold(int* course=0, int* level=0, int* sublevel=0, int* percent=0, int* score=0, int* lives=0);

int CampaignProc(MODULE* m, int msg, void* p1, void* p2)
{
	static int map_coords[5][10]=
	{
		//{30,41, 0,0, 0,0, 0,0, 0,0}, // tut
		{30,41, 1,34, 23,31, 55,30, 26,20},
		{1,10, 12,5, 45,13, 68,9, 48,2},
		{0,0, 0,0, 0,0, 0,0, 0,0},
		{0,0, 0,0, 0,0, 0,0, 0,0},
		{0,0, 0,0, 0,0, 0,0, 0,0},
	};



	static const int track_dx[8]={0,1,1,1,0,-1,-1,-1};
	static const int track_dy[8]={-1,-1,0,1,1,1,0,-1};

	struct Track
	{
		int x,y;
		const char* path;
	};

	static Track track[5][5]=
	{
	/*
	   7 0 1
	   6   2
	   5 4 3
	*/

		/*
		{
			// tut
			59,50,
			"",
		},
		*/
		{
			{
				59,50,
				"322222222077" "A" "660076665566667" "B" "655433" "C" "466666666670" "D" "11077666676666" "E" 
			},
			{
				32,44,
				"46666666656666667" "F" "65666666670" "G" "11222322222222222077" "H" "666666676660" "I" "11222122222222" "J" ,
			},
			{
				24,34,
				"22332222122" "K" "34566665" "L" "4322222122112" "M" "2322222122" "N" "2211122" "O" ,
			},
			{
				60,35,
				"22122234566666665422" "P" "22332222222" "Q" "32222222222222221110" "R" "7777766766666" "S" "66766666766666" "T" , 
			},
			{
				62,30,
				"456666666601077766" "U" "54455666676" "V" "6556666667" "W" "6600011" "X" "22223222222" "Y" "22070107766" "Z" ,
			},
		},
		{
			{
				38,20,
				"4666007766" "A" "6654456666676" "B" "65566666660076" "C" "6010776670" "D" "11222232222" "E" 
			},
			{
				15,11,
				"445433222221200112" "F" "232221076667766660112222" "G" "222322222322" "H" "222334556" "I" "66554322322221" "J" 
			},
			{
				47,15,
				"22223345666655422" "K" "2233222222220766012" "L" "2221122" "M" "23345422212" "N" "211070123222" "O" 
			},
			{
				81,17,
				"432223322212" "P" "221070112" "Q" "222076666656566" "R" "66701222076" "S" "6666554466" "T" 
			},
			{
				80,13,
				"66700766655666" "U" "666665666666770" "V" "1001222322221" "W" "22332221" "X" "207012222" "Y" "23344466" "Z" 
			}
		},
		{
			{0,0,0},
			{0,0,0},
			{0,0,0},
			{0,0,0},
			{0,0,0},
		},
		{
			{0,0,0},
			{0,0,0},
			{0,0,0},
			{0,0,0},
			{0,0,0},
		},
		{
			{0,0,0},
			{0,0,0},
			{0,0,0},
			{0,0,0},
			{0,0,0},
		}

	};

	struct CampaignData
	{
		struct Segment
		{
			int from;
			int len;
		};

		struct Level
		{
			Segment seg;
			int sublevels;
			Segment sublevel[6];
		};

		struct Course
		{
			Segment seg;
			int levels;
			Level level[5];
		};

		struct Map
		{
			int courses;
			int max_levels;
			Course course[5];
			int path_xy[654*2]; // currently: 654*2 , MAX: 5*5*128 x2
			int len;
		};

		Map map;

		int hold_focus;

		int course;
		int level;
		int path;
		//int levels; //<-switch to LEVELS[]

		int anim_y;
		unsigned long anim_bt;

		unsigned long map_bt;
		int from_x;
		int from_y;

		int head;
		unsigned long head_t;

		int car_to;
		//int car_pos;


		void GetXY(int* cur_x, int* cur_y)
		{
			int lev = level<0 ? 0 : level;
			int dt = menu_window.time - map_bt;

			int to_x = map_coords[course][2*lev+0];
			int to_y = map_coords[course][2*lev+1];

			*cur_x = to_x;
			*cur_y = to_y;

			if (to_x>from_x)
			{
				*cur_x = from_x + dt*SGN(to_x - from_x)/16;
				if (*cur_x>to_x)
				{
					*cur_x=to_x;
					from_x=to_x;
				}
			}
			else
			if (to_x<from_x)
			{
				*cur_x = from_x + dt*SGN(to_x - from_x)/16;
				if (*cur_x<to_x)
				{
					*cur_x=to_x;
					from_x=to_x;
				}
			}

			if (to_y>from_y)
			{
				*cur_y = from_y + dt*SGN(to_y - from_y)/64;
				if (*cur_y>to_y)
				{
					*cur_y=to_y;
					from_y=to_y;
				}
			}
			else
			if (to_y<from_y)
			{
				*cur_y = from_y + dt*SGN(to_y - from_y)/64;
				if (*cur_y<to_y)
				{
					*cur_y=to_y;
					from_y=to_y;
				}
			}
		}
	};

	static MENU_ASSET campagin_map(&map,0,0,0,0);
	static MENU_ASSET frame(&menu_map,2,2,2,1);

	static MENU_ASSET car(&menu_car,0,0,0,0);
	static MENU_ASSET tag(&menu_tag,0,0,0,0);


	static CampaignData data;

	switch (msg)
	{
		case MM_LOAD:
		{
			data.hold_focus = 0;

			data.course = conf_campaign.course;
			data.level = conf_campaign.level;
			data.path = 0;

			int lev = data.level<0 ? 0 : data.level;

			data.car_to = data.map.course[data.course].level[lev].seg.from;

			data.from_x = map_coords[data.course][2*lev+0];
			data.from_y = map_coords[data.course][2*lev+1];

			break;
		}

		case MM_INIT:
		{
			data.hold_focus = 0;

			data.course = conf_campaign.course;
			data.level = conf_campaign.level;
			data.path = 0;

			data.anim_y = 0;

			int lev = data.level<0 ? 0 : data.level;
			data.from_x = map_coords[data.course][2*lev+0];
			data.from_y = map_coords[data.course][2*lev+1];

			data.head_t=menu_window.time;
			data.head=0;

			data.map.courses=0;
			data.map.len=0;
			data.map.max_levels=0;

			if (!campaign[0].level)
			{
				data.course=-1;
				break; // fail creation
			}

			// generate map data
			int pos = 0;
			for (int i=0; campaign[i].level; i++)
			{
				data.map.course[i].levels=0;
				data.map.course[i].seg.from = data.map.len;
				data.map.course[i].seg.len  = 0;

				for (int j=0; campaign[i].level[j].height; j++)
				{
					data.map.course[i].level[j].seg.from = data.map.len;
					data.map.course[i].level[j].seg.len = 0;
					data.map.course[i].level[j].sublevels = 0;

					const char* path = track[i][j].path;

					if (path)
					{
						int x = track[i][j].x;
						int y = track[i][j].y;

						int sub = 0;

						data.map.course[i].level[j].seg.from = pos;

						int sub_from = pos;

						while (*path)
						{
							if (*path<'0' || *path>'7') // chkpnt marker
							{
								data.map.course[i].level[j].sublevel[sub].from = sub_from;
								data.map.course[i].level[j].sublevel[sub].len = pos - sub_from;
								sub_from = pos;
								sub++;
								path++;
								continue;
							}

							x+=track_dx[*path-'0'];
							y+=track_dy[*path-'0'];

							data.map.path_xy[2*pos+0] = x;
							data.map.path_xy[2*pos+1] = y;

							path++;

							data.map.course[i].level[j].sublevel[sub].len++;
							pos++;
						}

						data.map.course[i].level[j].sublevels = sub;
						data.map.course[i].level[j].seg.len = pos - data.map.course[i].level[j].seg.from;
						data.map.course[i].levels++;
					}
				}


				if (data.map.max_levels<data.map.course[i].levels)
					data.map.max_levels=data.map.course[i].levels;

				data.map.course[i].seg.len = pos - data.map.course[i].seg.from;
				data.map.courses++;
			}

			data.map.len = pos;

			data.car_to = data.map.course[data.course].level[lev].seg.from;

			return 20; //(module.t+1) + 2*data.courses-1 + data.max_levels + (module.b);
		}

		case MM_FOCUS:
		{
			break;
		}

		case MM_PAINT:
		{
			CON_OUTPUT* s = (CON_OUTPUT*)p1;
			int* clip = (int*)p2;

			if (data.course<0)
				break;

			int x = m->x + (module.l+2);
			int y = m->y + (module.t+1) - menu_window.smooth;


			int hold_course;
			int hold_level;
			int hold_sublevel;
			int hold_percent;
			int hold_score;
			int hold_lives;

			bool hold = GameOnHold(&hold_course,&hold_level,&hold_sublevel,&hold_percent,&hold_score,&hold_lives);

			// so in case of hold we should NOT use GetXY interpolation
			// instead: simply get hold_level destination area

			// paint map frame
			menu_stretch(s,x+16,y-1,43,15,&frame,m->state==2,clip);

			// paint map
			int cur_x,cur_y;
			if (hold)
			{
				cur_x = map_coords[hold_course][2*hold_level+0];
				cur_y = map_coords[hold_course][2*hold_level+1];
			}
			else
				data.GetXY(&cur_x,&cur_y);

			int extra = m->state<2 ? 1:0;
			menu_blit(s, x+16+frame.l,y-1+frame.t-1, &campagin_map, 	cur_x,cur_y, 43-frame.l-frame.r,15-frame.t-frame.b+1 +extra, 0, clip);

			int lev = hold ? hold_level : (data.level<0 ? 0 : data.level);
			int crs = hold ? hold_course : data.course; 

			if (/*s->color &&*/ m->state==2)
			{
				if (track[crs][lev].path)
				{
					int len = 0;
					int seg = 20;
					if (!s->color)
						seg = 5;

					int from = MAX(0,data.head-seg);
					int to = MIN(data.head,data.map.course[crs].level[lev].seg.len);
					for (int pos = from; pos<to; pos++)
					{
						int hi = 2*(pos + data.map.course[crs].level[lev].seg.from);

						int hi_x = data.map.path_xy[hi+0] + x+16+frame.l  -cur_x;
						int hi_y = data.map.path_xy[hi+1] + y-1+frame.t-1 -cur_y;

						if (hi_x>=x+16+frame.l && hi_x<x+16+frame.l+43-frame.l-frame.r &&
							hi_y>=y-1+frame.t-1 && hi_y<y-1+frame.t-1+15-frame.t-frame.b+1)
						{
							if (hi_x>=clip[0] && hi_x<clip[0]+clip[2] &&
								hi_y>=clip[1] && hi_y<clip[1]+clip[3])
							{
								if (s->color)
									s->color[hi_x + hi_y*(s->w+1)] |= 0x08;
								else
									s->buf[hi_x + hi_y*(s->w+1)] = '.';
							}
						}
					}
				}

				if (data.head<120)
					data.head+=(menu_window.time-data.head_t)/8;
				else
				{
					data.head=0;
				}
			}

			if (1) // paint car regardless of m->state
			{
				int car_f = 0;

				int pos = data.car_to;

				if (hold)
				{
					if (hold_sublevel<0)
						hold_sublevel = data.map.course[hold_course].level[hold_level].sublevels-1;

					pos = data.map.course[hold_course].level[hold_level].sublevel[hold_sublevel].from;
					pos += hold_percent * data.map.course[hold_course].level[hold_level].sublevel[hold_sublevel].len / 100;

					if (pos>=data.map.len)
						pos = data.map.len-1;
				}

				int pos1 = MAX(0,pos-1);
				int pos2 = MIN(data.map.len-1,pos+1);

				if (data.map.path_xy[2*pos2+0]<data.map.path_xy[2*pos1+0])
					car_f = 1;

				int car_x = data.map.path_xy[2*pos+0] + x+16+frame.l  -cur_x -car.w/2;
				int car_y = data.map.path_xy[2*pos+1] + y-1+frame.t-1 -cur_y -car.h/2;

				int to_x = map_coords[crs][2*lev+0];
				int to_y = map_coords[crs][2*lev+1];

				if (car_x+cur_x-to_x < x+16+frame.l)
					car_x = x+16+frame.l - cur_x + to_x;
				if (car_x+cur_x-to_x >= x+16+frame.l + 39-6)
					car_x = x+16+frame.l + 39-6 - cur_x + to_x;
				if (car_y+cur_y-to_y < y-1+frame.t-1)
					car_y = y-1+frame.t-1 - cur_y + to_y;
				if (car_y+cur_y-to_y >= y-1+frame.t-1 + 13-3)
					car_y = y-1+frame.t-1 + 13-3 - cur_y + to_y;

				int clip2[4] = { x+16+frame.l, y-1+frame.t-1, 39, 13 };

				if (clip2[0]<clip[0])
				{
					clip2[2]-=clip[0]-clip2[0];
					clip2[0]=clip[0];
				}
				if (clip2[0]+clip2[2]>clip[0]+clip[2])
					clip2[2]=clip[0]+clip[2]-clip2[0];

				if (clip2[1]<clip[1])
				{
					clip2[3]-=clip[1]-clip2[1];
					clip2[1]=clip[1];
				}
				if (clip2[1]+clip2[3]>clip[1]+clip[3])
					clip2[3]=clip[1]+clip[3]-clip2[1];

				menu_blit(s,car_x,car_y,&car,0,0,6,3,car_f,clip2);
			}

			data.head_t = menu_window.time;

			if (hold)
			{
				// GAME ON HOLD INFO + resume/reset UI
				char tmp[16];
				int len;
				unsigned char cl0 = LABEL_CL(m,false,false);
				unsigned char cl1 = VALUE_CL(m,true,true);

				menu_print(s, x, y, "Game Paused !", cl1, 13, clip);
				y+=2;

				menu_print(s, x, y, "Chapter:", cl0, 8, clip);
				y++;
				menu_print(s, x+0, y, campaign[hold_course].name, cl1, strlen(campaign[hold_course].name), clip);
				y+=2;

				len = sprintf_s(tmp,16,"[%c-%c]", campaign[hold_course].level[hold_level].name, campaign[hold_course].level[hold_level].name2);
				menu_print(s, x, y, "Level: ", cl0, 7, clip);
				menu_print(s, x+7, y, tmp, cl1, len, clip);
				y++;

				len = sprintf_s(tmp,16,"%d", hold_score);
				menu_print(s, x, y, "Score: ", cl0, 7, clip);
				menu_print(s, x+7, y, tmp, cl1, len, clip);
				y++;

				len = sprintf_s(tmp,16,"%d", hold_lives);
				menu_print(s, x, y, "Lives: ", cl0, 7, clip);
				menu_print(s, x+7, y, tmp, cl1, len, clip);
				y+=3;

				if (m->state==2 && data.hold_focus==0)
					menu_print(s, x, y,  ">", cl_menu_c[m->state], 1, clip);
				unsigned char cl = LABEL_CL(m,data.hold_focus==0,false);
				menu_blit(s,x+2,y-1,&tag,0,0,2,2,m->state==2?2:3,clip);
				menu_print(s, x+5, y, "RESUME", cl, 6, clip);
				y+=2;

				if (m->state==2 && data.hold_focus==1)
					menu_print(s, x, y,  ">", cl_menu_c[m->state], 1, clip);
				cl = LABEL_CL(m,data.hold_focus==1,false);
				menu_blit(s,x+2,y-1,&tag,0,0,2,2,m->state==2?0:1,clip);
				menu_print(s, x+5, y, "CLEAR", cl, 5, clip);
				y+=2;
			}
			else
			{
				// CAMPAIGN COURSES TREE

				if (data.anim_y<0)
				{
					int dt = (menu_window.time-data.anim_bt)/32;
					data.anim_y += dt;
					if (data.anim_y>0)
						data.anim_y=0;
					data.anim_bt += 32*dt;
				}
				else
				if (data.anim_y>0)
				{
					int dt = (menu_window.time-data.anim_bt)/32;
					data.anim_y -= dt;
					if (data.anim_y<0)
						data.anim_y=0;
					data.anim_bt += 32*dt;
				}


				static char clear[16]="               ";

				for (int i=0; i<data.map.courses; i++)
				{
					if (i==data.course && data.anim_y>0 || i==data.course+1 && data.anim_y<0)
						y+=data.anim_y;

					unsigned char cl = (i==data.course /*&& data.level<0*/) ? cl_menu_c[m->state] : (i==data.course ? cl_menu_b[m->state] : cl_menu_a[m->state]);

					// paint space 1 line above
					menu_print(s, x, y-1, clear, cl, 15, clip);

					if (campaign[i].flags & 0x1)
					{
						cl &= 0xF0;
						if (!cl)
							cl = 0x08;
						menu_print(s, x, y, i==data.course ? (data.level<0 ? "[ ] ":"[ ] ") : "[ ] ", cl, 4, clip);
					}
					else
						menu_print(s, x, y, i==data.course ? (data.level<0 ? "[>] ":"[-] ") : "[+] ", cl, 4, clip);

					const char* name = campaign[i].name;
					int nlen = strlen(name);
					menu_print(s, x+4, y, name, cl, nlen, clip);
					if (nlen<11)
						menu_print(s, x+4+nlen, y, clear, cl, 11-nlen, clip);

					y++;

					if (i==data.course || i==data.course-1 && data.anim_y>0 || i==data.course+1 && data.anim_y<0)
					{
						for (int j=0; j<data.map.course[data.course].levels; j++)
						{
							if (y<m->y+m->h-module.b - menu_window.smooth)
							{
								cl = i==data.course && j==data.level ? cl_menu_c[m->state] : cl_menu_b[m->state];

								char name[32];
								int len = sprintf_s(name,32, i==data.course && j==data.level ? "> (%c-%c)" : "   %c-%c ", campaign[i].level[j].name, campaign[i].level[j].name2);
								menu_print(s, x+4, y, name, cl, len, clip);
							}
							y++;
						}

						if (i!=data.course)
							y-=data.map.course[data.course].levels;
						else
							y+=data.map.max_levels-data.map.course[data.course].levels;
					}

					if (i==data.course && data.anim_y>0 || i==data.course+1 && data.anim_y<0)
						y-=data.anim_y;

					y++;
				}
			}

			return 1;
		}

		case MM_INPUT:
		{
			CON_INPUT* ci = (CON_INPUT*)p1;
			if (ci->EventType == CON_INPUT_KBD && ci->Event.KeyEvent.bKeyDown)
			{
				bool hold = GameOnHold();

				switch (ConfMapInput(ci->Event.KeyEvent.uChar.AsciiChar))
				{
					// prev level/course, open & close courses if changed
					case KBD_UP:
					if (hold)
					{
						if (data.hold_focus>0)
						{
							data.hold_focus--;
							return 1;
						}
					}
					else
					{
						data.GetXY(&data.from_x,&data.from_y);
						data.map_bt = menu_window.time;

						if (data.level>=0)
						{
							data.level--;
							conf_campaign.course = data.course;
							conf_campaign.level  = data.level;

							int lev = data.level<0 ? 0 : data.level;
							data.car_to = data.map.course[data.course].level[lev].seg.from;

							SaveConf();

							return 1;
						}
						else
						if (data.course>0)
						{
							data.course--;

							data.anim_y = -data.map.course[data.course].levels;
							data.anim_bt = menu_window.time;

							data.level = data.map.course[data.course].levels-1;

							conf_campaign.course = data.course;
							conf_campaign.level  = data.level;

							int lev = data.level<0 ? 0 : data.level;
							data.car_to = data.map.course[data.course].level[lev].seg.from;

							SaveConf();

							return 1;
						}
					}
					break;

					// next level/course, open & close courses if changed
					case KBD_DN:
					if (hold)
					{
						if (data.hold_focus<1)
						{
							data.hold_focus++;
							return 1;
						}
					}
					else
					{
						if (conf_campaign.passed==data.course)
						{
							// only first level can be selected
							if (data.level==0)
								break;
						}

						data.GetXY(&data.from_x,&data.from_y);
						data.map_bt = menu_window.time;

						if (data.level<data.map.course[data.course].levels-1)
						{
							data.level++;
							conf_campaign.course = data.course;
							conf_campaign.level  = data.level;

							int lev = data.level<0 ? 0 : data.level;
							data.car_to = data.map.course[data.course].level[lev].seg.from;

							SaveConf();

							return 1;
						}
						else
						if (data.course<data.map.courses-1 && (campaign[data.course+1].flags & 0x1)==0)
						{
							data.course++;

							data.anim_y = data.map.course[data.course].levels;
							data.anim_bt = menu_window.time;

							data.level=-1;

							conf_campaign.course = data.course;
							conf_campaign.level  = data.level;

							int lev = data.level<0 ? 0 : data.level;
							data.car_to = data.map.course[data.course].level[lev].seg.from;

							SaveConf();

							return 1;
						}
					}

					break;

					// PLAY!!!!!!
					case 13:
					if (hold)
					{
						if (data.hold_focus==0)
						{
							// resume
							return -3;
						}
						else
						if (data.hold_focus==1)
						{
							// reset
							ClearOnHold();
							return 1;
						}
					}
					else
					{
						int j = data.level;
						if (j<0)
							j=0;

						// ensure level is not a dummy
						if (campaign[data.course].level[j].height[0])
						{
							// set hold focus to resume
							data.hold_focus = 0;
							return -3;
						}

						return 0;
					}
					break;

				}
			}

			break;
		}
	}

	return 0;
}

int KeyboardProc(MODULE* m, int msg, void* p1, void* p2)
{
	struct KeyboardData
	{
		char map[6];
		int hover;
		bool focus;
	};

	static KeyboardData data;

	static const char* label[6]=
	{
		"Left  (brake)",
		"Right (accel.)",
		"Up    (jump)",
		"Down  (crouch)",
		"Enter (fire)",
		"Quit"
	};

	const static char* syskey[6]=
	{
		"Lft",
		"Rgt",
		"Up",
		"Dwn",
		"Ent",
		"Esc"
	};

	switch (msg)
	{
		case MM_LOAD:
		{
			memcpy(data.map, conf_keyboard.map, 6);
			break;
		}

		case MM_INIT:
		{
			memcpy(data.map, conf_keyboard.map, 6);
			data.hover = 0;
			data.focus = false;

			return 20; //(module.t+1) + 12;
		}

		case MM_INPUT:
		{
			CON_INPUT* ci = (CON_INPUT*)p1;
			if (ci->EventType == CON_INPUT_KBD && ci->Event.KeyEvent.bKeyDown)
			{
				if (!data.focus)
				{
					switch (ConfMapInput(ci->Event.KeyEvent.uChar.AsciiChar))
					{
						case KBD_UP:
							if (data.hover>0)
								data.hover--;
							return 1;
						case KBD_DN:
							if (data.hover<5)
								data.hover++;
							return 1;

						case 13:
							data.focus = true;
							return 1;
					}
				}
				else
				{
					char c = ci->Event.KeyEvent.uChar.AsciiChar;
					if (c>='a' && c<='z')
						c+='A'-'a';
					if (c>='0' && c<='9' ||
						c>='A' && c<='Z' || c==' ')
					{
						data.map[data.hover] = c;

						memcpy(conf_keyboard.map,data.map,6);
						SaveConf();
					}

					data.focus = false;
					return 1;
				}

				break;
			}

			break;
		}

		case MM_PAINT:
		{
			CON_OUTPUT* s = (CON_OUTPUT*)p1;
			int* clip = (int*)p2;

			int x = m->x + (module.l+2);
			int y = m->y + (module.t+1) - menu_window.smooth + 1;

			unsigned char cl;

			if (s->color)
			{
				for (int i=0; i<6; i++)
				{

					if (m->state==2 && data.hover==i)
						menu_print(s, x+2  -(2), y+2*i, ">", cl_menu_c[m->state], 1, clip);

					cl = LABEL_CL(m,data.hover==i,data.focus);
					menu_print(s, x+2  -(2)+2, y+2*i, label[i], cl, strlen(label[i]), clip);

					cl = VALUE_CL(m,data.hover==i,data.focus);

					const char* m = &data.map[i];
					int  l = 1;

					if (*m==' ')
					{
						m = "Spc";
						l = 3;
					}

					for (int j=0; j<6; j++)
					{
						if (i==j)
							continue;

						if (data.map[i] == data.map[j])
							cl = (cl&0xF8) | 0x01;
					}

					menu_print(s, x+2  -(2)+2 + 18 -l/2, y+2*i, m, cl, l, clip);
				}
			}
			else
			{
				for (int i=0; i<6; i++)
				{
					if (m->state==2 && data.hover==i)
						menu_print(s, x+2 -(2), y+2*i, ">", 0, 1, clip);

					menu_print(s, x+2  -(2)+2, y+2*i, label[i], 0, strlen(label[i]), clip);

					const char* m = &data.map[i];
					int  l = 1;

					if (*m==' ')
					{
						m = "Spc";
						l = 3;
					}

					menu_print(s, x+2  -(2)+2 + 18 -l/2, y+2*i, m, 0, l, clip);
				}
			}

			break;
		}
	}

	return 0;
}

void LoadMenu()
{
	WINDOW* mw = &menu_window;
	// overwrite menu data with conf
	for (int i=0; i<mw->modules; i++)
	{
		// init module, calc size
		if (mw->module[i].proc)
			mw->module[i].proc(mw->module+i,MM_LOAD,0,0);
	}
}

void InitMenu()
{
	cl_menu_a[0]=module.color[0][(module.w+1)*6+6];
	cl_menu_a[1]=module.color[1][(module.w+1)*6+6];
	cl_menu_a[2]=module.color[2][(module.w+1)*6+6];

	cl_menu_b[0]=module.color[0][(module.w+1)*7+6];
	cl_menu_b[1]=module.color[1][(module.w+1)*7+6];
	cl_menu_b[2]=module.color[2][(module.w+1)*7+6];

	cl_menu_c[0]=module.color[0][(module.w+1)*8+6];
	cl_menu_c[1]=module.color[1][(module.w+1)*8+6];
	cl_menu_c[2]=module.color[2][(module.w+1)*8+6];

	WINDOW* mw = &menu_window;
	memset(mw,0,sizeof(WINDOW));

	mw->time = get_time();

	mw->modules = 5;
	mw->module = new MODULE[mw->modules];
	memset(mw->module,0,sizeof(MODULE)*mw->modules);

	mw->module[0].title = "Player Profile";
	mw->module[0].h = 20; // <- will be automaticaly calculated!
	mw->module[0].proc = ProfileProc;
	mw->module[0].dbl=false;

	mw->module[1].title = "Play Campaign";
	mw->module[1].h = 20; // <- will be automaticaly calculated!
	mw->module[1].proc = CampaignProc;
	mw->module[1].dbl=true;

	mw->module[2].title = "Keyboard Input";
	mw->module[2].h = 20; // <- will be automaticaly calculated!
	mw->module[2].proc = KeyboardProc;
	mw->module[2].dbl=false;

	mw->module[3].title = "Hi Scores";
	mw->module[3].h = 20; // <- will be automaticaly calculated!
	mw->module[3].proc = ScoreProc;
	mw->module[3].dbl=true;

	mw->module[4].title = "Control Panel";
	mw->module[4].h = 20; // <- will be automaticaly calculated!
	mw->module[4].proc = ControlProc;
	mw->module[4].dbl=true;

	// now construct order of modules for each of 3 layouts (2,3,4 columns)

	static const int layout_2[]=
	{
		0,2,
		 1,
		 3,
		 4
	};

	static const int layout_3[]=
	{
		0,1,
		3,2,
		4
	};

	static const int layout_4[]=
	{
		0,1,2,
		 3,4
	};

	mw->layout = 0;
	mw->layout_2 = layout_2;
	mw->layout_3 = layout_3;
	mw->layout_4 = layout_4;

	mw->scroll = 0;
	mw->smooth = 0;
	mw->columns = 0;

	for (int i=0; i<mw->modules; i++)
	{
		// init module, calc size
		if (mw->module[i].proc)
		{
			mw->module[i].h = mw->module[i].proc(mw->module+i,MM_INIT,0,0);
			mw->module[i].x = -1;
			mw->module[i].y = -1;
		}
	}

	mw->state = 0; // hovering, (should we initialy focus to quick guide module?)
	mw->focus = 0;
}


void FreeMenu()
{
	WINDOW* mw = &menu_window;

	if (mw->module)
		delete [] mw->module;
	mw->modules=0;
	mw->module=0;

	mw->layout_w = 0;
	mw->layout_h = 0;

	mw->layout_2=0;
	mw->layout_3=0;
	mw->layout_4=0;
}

static void PaintWindow(CON_OUTPUT* s, int pos, int size)
{
	menu_stretch(s,0,0,s->w,s->h,&window,0);

	// menu_blit(s,window.l+1,window.t,&backdrop,(backdrop.w - (s->w-window.l-window.r-2))/2,pos/3,s->w-window.l-window.r-2,s->h-window.t-window.b,0);

	if (pos<0)
		return;

	int client = s->h-window.t-window.b-1;
	if (client>=size || client<3)
		return;

	int th_size = client*client / size;
	//if (th_size>client-2)
	//	th_size = client-2;
	if (th_size<=0)
		th_size=1;

	if (pos>size-client)
		pos = size-client;

	int th_pos = (2 * pos * (client-th_size) + (size-client)) / (2*(size-client));
	if (th_pos==0 && pos!=0)
		th_pos=1;
	if (th_pos==client-th_size && pos!=size-client)
		th_pos=client-th_size-1;

	for (int y=window.t; y<window.t+th_pos; y++)
		menu_blit(s,s->w-window.r-3,y,&window,window.w-window.r-3,window.t,3,1,0);

	menu_blit(s,s->w-window.r-3,window.t+th_pos,&window,window.w-window.r-3,window.t+1,3,1,0);

	for (int y=window.t+th_pos+1; y<window.t+th_pos+th_size; y++)
		menu_blit(s,s->w-window.r-3,y,&window,window.w-window.r-3,window.t+3,3,1,0);

	menu_blit(s,s->w-window.r-3,window.t+th_pos+th_size,&window,window.w-window.r-3,window.t+4,3,1,0);

	for (int y=window.t+th_pos+th_size+1; y<s->h-window.b; y++)
		menu_blit(s,s->w-window.r-3,y,&window,window.w-window.r-3,window.h-window.b-1,3,1,0);
}

static void PostPaint(CON_OUTPUT* s)
{
	int y = window.t;
	int l = window.l;
	int r = s->w-window.r-3;
	for (int x=l; x<r; x++)
	{
		// should we allow '.' as well?
		if (s->buf[(s->w+1)*y+x] != '_' && s->buf[(s->w+1)*y+x] != '.')
			s->buf[(s->w+1)*y+x] = ' ';
	}

	if (s->color)
	{
		for (int x=l; x<r; x++)
		{
			s->color[(s->w+1)*y+x]&=0x0F;
		}
	}

	WINDOW* mw = &menu_window;

	/*
	// cast 'shadows' below last module in each column
	for (int i=mw->modules - mw->columns; i<mw->modules; i++)
	{
		MODULE* m = mw->module + mw->layout[i];

		int y = m->y + m->h - menu_window.smooth;
		if (y<s->h - window.b)
		{
			for (int x=m->x; x<m->x+m->w; x++)
			{
				if (s->buf[x+(s->w+1)*y] != '.' && s->buf[x+(s->w+1)*y] != '_')
					s->buf[x+(s->w+1)*y] = ' ';
			}
		}
	}
	*/
}

void PaintModule(CON_OUTPUT* s, int x, int y, int w, int h, const char* title, int state, const int* clip = 0)
{
	menu_stretch(s,x,y,w,h,&module,state,clip);

	const char* glyph = s->color ? module.shade[state] : module.mono[state];
	const char* color = s->color ? module.color[state] : 0;

	char titbuf[64];
	int len = strlen(title);
	if (len>w-12)
		len=w-12;
	else
	if ( (len&1) != (w&1) && len<64)
	{
		const char* spc = strchr(title,' ');
		if (spc)
		{
			int sp = spc-title+1;
			memcpy(titbuf,title,sp);
			memcpy(titbuf+sp,spc,len-(spc-title));
			title=titbuf;
			len++;
		}
	}

	int cx = (w-len)/2;

	menu_print(s,x+cx,y+2,title, color ? color[(module.w+1)*2+6] : 0, len, clip);

	menu_fill(s,x+cx-2,y+2, glyph[(module.w+1)*2+4], color ? color[(module.w+1)*2+4] : 0, 1,1, clip);
	menu_fill(s,x+cx+len+1,y+2, glyph[(module.w+1)*2+8], color ? color[(module.w+1)*2+8] : 0, 1,1, clip);

	menu_fill(s,x+3,y+2, glyph[(module.w+1)*2+3], color ? color[(module.w+1)*2+3] : 0, cx-5,1, clip);
	menu_fill(s,x+cx+len+2,y+2, glyph[(module.w+1)*2+9], color ? color[(module.w+1)*2+9] : 0, w-5 -cx-len,1, clip);

}

void PaintScroll(CON_OUTPUT* s, int x, int dy, int h, int pos, int size, int state, const int* clip)
{
	// (using module scroll asset)

	if (size<=h)
	{
		// no need to draw it, right?
		return;
	}

	int th_size = h*h / size;
	//if (th_size>client-2)
	//	th_size = client-2;
	if (th_size<=0)
		th_size=1;

	if (pos>size-h)
		pos = size-h;

	int th_pos = (2 * pos * (h-th_size) + (size-h)) / (2*(size-h));
	if (th_pos==0 && pos!=0)
		th_pos=1;
	if (th_pos==h-th_size && pos!=size-h)
		th_pos=h-th_size-1;

	// upper dots
	for (int y=0; y<th_pos; y++)
		menu_blit(s,x,y+dy,&module,module.w-module.r-3,module.t,3,1,state,clip);

	// upper thumb
	menu_blit(s,x,dy+th_pos,&module,module.w-module.r-3,module.t+1,3,1,state,clip);

	// middle thumb
	for (int y=th_pos+1; y<th_pos+th_size; y++)
		menu_blit(s,x,y+dy,&module,module.w-module.r-3,module.t+3,3,1,state,clip);

	// lower thumb
	menu_blit(s,x,dy+th_pos+th_size,&module,module.w-module.r-3,module.t+4,3,1,state,clip);

	// lower dots
	for (int y=th_pos+th_size+1; y<=h; y++)
		menu_blit(s,x,dy+y,&module,module.w-module.r-3,module.h-module.b-1,3,1,state,clip);

}

int RunMenu(CON_OUTPUT* s)
{
	WINDOW* mw = &menu_window;

	static int w = s->w;
	static int h = s->h;

	static int fr=0;

	// while (1)
	{
		fr++;

		int dw = 0;
		int dh = 0;
		get_terminal_wh(&dw,&dh);
		int nw = dw;
		int nh = dh;

		
		if (nw>160)
			nw=160;
		if (nw<80)
			nw=80;
		if (nh>50)
			nh=50;
		if (nh<25)
			nh=25;

		if (w!=nw || h!=nh)
		{
			resize_con_output(s,nw,nh, ' ');
			w=nw;
			h=nh;
		}

		UpdateLayout(s);

		unsigned long t = get_time();

		if (t>mw->time)
		{
			float transition = expf( (-(int)(t-mw->time))/100.0f );

			if (mw->smooth != mw->scroll)
			{
				if (mw->smooth < mw->scroll)
				{
					mw->fsmooth = mw->scroll - transition*(mw->scroll-mw->fsmooth);
					mw->smooth = (int)floorf(mw->fsmooth + 0.5f);
					if (mw->smooth > mw->scroll)
						mw->smooth = mw->scroll;
				}
				else
				{
					mw->fsmooth = mw->scroll - transition*(mw->scroll-mw->fsmooth);
					mw->smooth = (int)floorf(mw->fsmooth + 0.5f);
					if (mw->smooth < mw->scroll)
						mw->smooth = mw->scroll;
				}
			}

			for (int i=0; i<mw->modules; i++)
			{
				MODULE* m = mw->module + i;
				if (m->x != m->dx)
				{
					if (m->x < m->dx)
					{
						m->fx = m->dx - transition*(m->dx-m->fx);
						m->x = (int)floorf(m->fx + 0.5f);
						if (m->x > m->dx)
							m->x = m->dx;
					}
					else
					{
						m->fx = m->dx - transition*(m->dx-m->fx);
						m->x = (int)floorf(m->fx + 0.5f);
						if (m->x < m->dx)
							m->x = m->dx;
					}
				}

				if (m->y != m->dy)
				{
					if (m->y < m->dy)
					{
						m->fy = m->dy - transition*(m->dy-m->fy);
						m->y = (int)floorf(m->fy + 0.5f);
						if (m->y > m->dy)
							m->y = m->dy;
					}
					else
					{
						m->fy = m->dy - transition*(m->dy-m->fy);
						m->y = (int)floorf(m->fy + 0.5f);
						if (m->y < m->dy)
							m->y = m->dy;
					}
				}
			}
		}

		mw->time = t;

		// paint window
		PaintWindow(s,mw->smooth,mw->height);

		int client_w = s->w - window.l - window.r -3;
		int client_h = s->h - window.t - window.b;

		// paint modules
		int clip[4] = { window.l, window.t, client_w, client_h };

		// non hovered modules first
		for (int i=0; i<mw->modules; i++)
		{
			if ( mw->module+i == mw->focus )
				continue;

			PaintModule( s, mw->module[i].x , mw->module[i].y - mw->smooth, mw->module[i].w, mw->module[i].h, mw->module[i].title, mw->module[i].state, clip);
			if (mw->module[i].proc)
				mw->module[i].proc( mw->module+i, MM_PAINT, (void*)s, (void*)clip );
		}
	
		// then hovered one
		PaintModule( s, mw->focus->x , mw->focus->y - mw->smooth, mw->focus->w, mw->focus->h, mw->focus->title, mw->focus->state, clip);
		if (mw->focus->proc)
			mw->focus->proc( mw->focus, MM_PAINT, (void*)s, (void*)clip );

		PostPaint(s); // fix top client row, allows only '.' and ' ' glyphs and forces black background 

		if (1)
		{
			char buf[81];
			int len = sprintf_s(buf,81,"ascii-patrol ver. alpha 1.3 by Gumix");
			menu_print(s, (s->w - len)/2, s->h-1, buf,(unsigned char)0x8F, len, 0);
		}

		menu_write(s,dw,dh,0,0,-1,-1);

		CON_INPUT ir[4];
		int irn=0;

		bool hit = false;
		get_input_len(&irn);
		while (irn)
		{
			read_input(ir,4,&irn);
			for (int i=0; i<irn; i++)
			{
				int input_handled=0;
				
				if (mw->state==1) // module is focused!
				{
					if (mw->focus->proc)
					{
						input_handled = mw->focus->proc( mw->focus, MM_INPUT, (void*)(ir+i), 0 );

						// temporarily to handle play campaing
						if (input_handled == -3)
						{
							return -3;
						}
					}
				}

				if (!input_handled && ir[i].EventType == CON_INPUT_KBD)
				{
					if (ir[i].Event.KeyEvent.bKeyDown)
					{
						switch (ConfMapInput(ir[i].Event.KeyEvent.uChar.AsciiChar))
						{
							case 27:
							{
								if (mw->state==1)
								{
									int fl=1;
									int ey=0;
									if (mw->focus->proc)
										mw->focus->proc(mw->focus,MM_FOCUS,&fl,&ey);

									// focus -> hover
									mw->state = 0;
									mw->focus->state = 1;
									break;
								}
								else
								{
									// we should display some message box here,
									// to make sure player really intented to exit
									return -2;
								}
							}

							case 13:
							{
								if (mw->state==0)
								{
									int fl=2;
									int ey=0;
									if (mw->focus->proc)
										mw->focus->proc(mw->focus,MM_FOCUS,&fl,&ey);

									// hover -> focus
									mw->state = 1;
									mw->focus->state = 2;
								}

								break;
							}

							case KBD_LT: // --
							{
								if (mw->state || !mw->focus->left)
									break;

								int fl=0,ey;
								if (mw->focus->proc)
									mw->focus->proc(mw->focus,MM_FOCUS,&fl,&ey);
								mw->focus->state = 0;

								fl = 1;
								mw->focus = mw->focus->left;
								if (mw->focus->proc)
									mw->focus->proc(mw->focus,MM_FOCUS,&fl,&ey);
								mw->focus->state = 1;

								mw->focus_col = mw->focus->col;

								// smooth scroll to hovered mod
								if (mw->focus->dy - window.t - mw->scroll < 0)
									mw->scroll = mw->focus->dy - window.t;
								if (mw->focus->dy+mw->focus->h - window.t - mw->scroll > client_h)
									mw->scroll = mw->focus->dy+mw->focus->h - window.t - client_h;

								break;
							}

							case KBD_RT: // ++
							{
								if (mw->state || !mw->focus->right)
									break;

								int fl=0,ey;
								if (mw->focus->proc)
									mw->focus->proc(mw->focus,MM_FOCUS,&fl,&ey);
								mw->focus->state = 0;

								fl = 1;
								mw->focus = mw->focus->right;
								if (mw->focus->proc)
									mw->focus->proc(mw->focus,MM_FOCUS,&fl,&ey);
								mw->focus->state = 1;

								mw->focus_col = mw->focus->col;

								// smooth scroll to hovered mod
								if (mw->focus->dy - window.t - mw->scroll < 0)
									mw->scroll = mw->focus->dy - window.t;
								if (mw->focus->dy+mw->focus->h - window.t - mw->scroll > client_h)
									mw->scroll = mw->focus->dy+mw->focus->h - window.t - client_h;

								break;
							}

							case KBD_UP:
							{
								if (mw->state || !mw->focus->upper && !mw->focus->upper2)
									break;

								MODULE* upper=0;
								if (!mw->focus->upper)
									upper = mw->focus->upper2;
								else
								if (!mw->focus->upper2)
									upper = mw->focus->upper;
								else
								{
									// choose better one
									upper = mw->focus->col == mw->focus_col ? mw->focus->upper : mw->focus->upper2;
								}

								int fl=0,ey;
								if (mw->focus->proc)
									mw->focus->proc(mw->focus,MM_FOCUS,&fl,&ey);
								mw->focus->state = 0;

								fl = 1;
								mw->focus = upper;
								if (mw->focus->proc)
									mw->focus->proc(mw->focus,MM_FOCUS,&fl,&ey);
								mw->focus->state = 1;

								// smooth scroll to hovered mod
								if (mw->focus->dy - window.t - mw->scroll < 0)
									mw->scroll = mw->focus->dy - window.t;
								if (mw->focus->dy+mw->focus->h - window.t - mw->scroll > client_h)
									mw->scroll = mw->focus->dy+mw->focus->h - window.t - client_h;

								break;
							}
							case KBD_DN:
							{
								if (mw->state || !mw->focus->lower && !mw->focus->lower2)
									break;

								MODULE* lower=0;
								if (!mw->focus->lower)
									lower = mw->focus->lower2;
								else
								if (!mw->focus->lower2)
									lower = mw->focus->lower;
								else
								{
									// choose better one
									lower = mw->focus->col == mw->focus_col ? mw->focus->lower : mw->focus->lower2;
								}

								int fl=0,ey;
								if (mw->focus->proc)
									mw->focus->proc(mw->focus,MM_FOCUS,&fl,&ey);
								mw->focus->state = 0;

								fl = 1;
								mw->focus = lower;
								if (mw->focus->proc)
									mw->focus->proc(mw->focus,MM_FOCUS,&fl,&ey);
								mw->focus->state = 1;

								// smooth scroll to hovered mod
								if (mw->focus->dy - window.t - mw->scroll < 0)
									mw->scroll = mw->focus->dy - window.t;
								if (mw->focus->dy+mw->focus->h - window.t - mw->scroll > client_h)
									mw->scroll = mw->focus->dy+mw->focus->h - window.t - client_h;

								break;
							}
						}
					}
				}
			}

			irn=0;
			get_input_len(&irn);
		}

	}

	return 0;
}
