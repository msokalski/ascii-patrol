
#include <stdio.h>
#include "manual.h"

MANUAL::~MANUAL()
{
	for (int i=0; i<man.frames; i++)
	{
		// delete individual objects, not array!
		delete paper[i];
	}
}

MANUAL::MANUAL(const ASSET* a, char transp, unsigned char _tcolor) :
	man(a),
	SCREEN(43,17,transp,_tcolor)
{
	for (int i=0; i<man.frames; i++)
	{
		paper[i] = new SCREEN(43,17,transp,_tcolor);
		ClearPaper(i);
	}

	sheets = man.frames - 1;
	half_w = man.width/2;

	page=-1;
	SetPage(0,0);
}

void MANUAL::ClearPaper(int i)
{
	paper[i]->Clear();
	man.SetFrame(i);

	// we shift it 1 cell down
	// to make space for flipping page over manual
	man.Paint(paper[i],0,1,0,0,-1,-1,0,false);
}

void MANUAL::SetPage(int i, int f)
{
	if (page!=i)
	{
		page = i;

		Clear();

		// left
		for (int i=1; i<=page; i++)
			paper[i]->BlendPage(this,0,0,0,0,half_w,-1);

		//right
		for (int i=sheets; i>page; i--)
			paper[i]->BlendPage(this,man.width-half_w,0,man.width-half_w,0,half_w,-1);
	}

	anim = f;
}


void MANUAL::Paint(SCREEN* scr, int dx, int dy, int sx, int sy, int sw, int sh, bool blend)
{
	// TODO: clip to sx,sy,sw,sh !!!
	// let's ignore sx,sy,sw,sh for a moment


	// underlay stack of pages
	SCREEN::Paint(scr,dx,dy,0,0,-1,-1,true);

	// overlay with anim page
	if (anim<=0)
	{
		paper[page]->BlendPage(scr,dx,dy,0,0,-1,-1);
	}
	else
	if (anim>=22)
	{
		paper[page+1]->BlendPage(scr,dx,dy,0,0,-1,-1);
	}
	else
	{
		// right below

		// todo: if this is page0, blend bit more from the left to include binding
		int bind = page==0 ? 3:1;
		paper[page]->BlendPage(scr,dx+man.width-half_w-bind,dy,man.width-half_w-bind,0,half_w-anim+bind,-1);

		// draw edge, TODO: clip!
		int bend = man.width-anim;
		int y=2;
		scr->buf[dx+bend+(dy+y)*(scr->w+1)] = ')';
		if (scr->color)
			scr->color[dx+bend+(dy+y)*(scr->w+1)] = 0x70;

		for (y=3; y<man.height-1; y++)
		{
			scr->buf[dx+bend+(dy+y)*(scr->w+1)] = '|';
			if (scr->color)
				scr->color[dx+bend+(dy+y)*(scr->w+1)] = 0x70;
		}

		scr->buf[dx+bend+(dy+y)*(scr->w+1)] = ')';
		if (scr->color)
			scr->color[dx+bend+(dy+y)*(scr->w+1)] = 0x70;

		// 2 cols of shadow, right to the bend edge, TODO: clip!
		if (scr->color)
		{
			int ws = (12 - ABS(12-anim)+2)/4;
			for (int y=2; y<man.height; y++)
			{
				for (int xs=bend+1; xs<bend+1+ws; xs++)
					scr->color[dx+xs+(dy+y)*(scr->w+1)] = 
						(scr->color[dx+xs+(dy+y)*(scr->w+1)] & 0x0F) | 0x70;
			}
		}

		// finaly blend upper side of flipping page
		paper[page+1]->BlendPage(scr,dx+bend-anim +1,dy,0,0,anim-1,-1);
	}
}

