
#include <cstdio>
#include <cstring>

#include "assets.h"
#include "spec.h"
#include "gameover.h"
#include "twister.h"

#include "unmo3.h"

GAMEOVER_MODAL::~GAMEOVER_MODAL()
{
	if (tx_mask)
		delete [] tx_mask;

	if (tx_order)
		delete [] tx_order;

}

GAMEOVER_MODAL::GAMEOVER_MODAL(SCREEN* _s, int* _score, const LEVEL* _level) :
	bk(&game_over_bk),
	bk2(_level->bkgnd),
	fg(&game_over_fg),
	tx(&game_over_tx),
	fl1(&flame1),
	fl2(&flame2),
	fl3(&flame3),
	fl1_mask(&flame1_mask),
	fl2_mask(&flame2_mask),
	fl3_mask(&flame3_mask),
	fnt(&digits_large),
	prompt(" P R E S S  A N Y  K E Y  T O  C O N T I N U E ")
{

	PlayLoop(0,21,0,29);

	s=_s;
	score = _score;
	level = _level;

	bk2_asset.mono = _level->bkgnd->mono;
	bk2_asset.shade = _level->bkgnd->mono;
	bk2_asset.color = _level->bkgnd->color;

	prompt.attrib_mask=0;

	bk2.data = &bk2_asset;
	bk2.attrib_mask = 0x00;
	bk2.attrib_over = 0x04;

	bt = get_time();
	frame = 0;

	int siz = tx.width*tx.height;
	tx_fill = 0;
	tx_mask = new unsigned char [siz];
	memset(tx_mask,0,siz);

	tx_order = new int [siz];
	for (int i=0; i<siz; i++)
		tx_order[i]=i;

	for (int i=siz-1; i>=1; i--)
	{
		int j = twister_rand()%(i+1);
		int k = tx_order[i];
		tx_order[i] = tx_order[j];
		tx_order[j] = k;
	}

}

void GAMEOVER_MODAL::PaintFlame(SPRITE* fl, SPRITE* msk, int dx, int dy, int frm)
{
	for (int y=0; y<msk->height; y++)
	{
		int fy = (y+frm)&0xF;
		for (int x=0; x<msk->width; x++)
		{
			int m = x+y*(msk->width+1);
			if (msk->data->mono[0][m]!='*')
			{
				int f = 1+ x+fy*(fl->width+1);
				int b = x+dx+(y+dy)*(s->w+1);
				if (s->color)
				{
					if (fl->data->shade[0][f]!='*')
					{
						s->buf[b] = fl->data->shade[0][f];
						s->color[b] = fl->data->color[0][f];
					}
				}
				else
				{
					if (fl->data->mono[0][f]!='*')
					{
						s->buf[b] = fl->data->mono[0][f];
					}
				}
			}
		}
	}
}

int GAMEOVER_MODAL::Run()
{
	unsigned long ct = get_time();

	int fr = (ct-bt);
	if (fr>=8064000)
		fr-=4032000;

	if (fr>1500 && tx_fill < 4*tx.width*tx.height)
	{
		int dfr = (fr-frame)*3;
		for (int i=0; i<dfr; i++)
		{
			int m = tx_order[tx_fill&0x1FF];
			tx_mask[m]++;
			tx_fill++;
			if (tx_fill == 4*tx.width*tx.height)
				break;
		}
	}

	frame = fr;

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
		s->Resize(nw,nh);

		w=nw;
		h=nh;
	}

	if (s->color)
		s->tcolor = 0x0F;
	s->Clear();

	int tm_y = 0,  tm_h = 10;
	int bk_y = 10, bk_h = 20;
	int fg_y = 30, fg_h = 10;
	int bm_y = 40, bm_h = 10;

	int wx = (160 - s->w)/2;
	int wy = 17 - 17*(s->h-25)/25;

	int tx_x = (s->w - tx.width -1)/2;
	int tx_y = 19 - 4*(s->h-25)/25;

	// paint game bk
	bk2.Paint(s,0,3-wy,wx,0,-1,-1);

	// paint fg
	fg.Paint(s,0,fg_y-wy,wx,0);

	bk.Paint(s,0,bk_y-wy,wx,0);

	// animated paint fl1,fl2,fl3
	// x positions of fl* are 1 cell to the left relatively to its mask!

	int fl1_x = 64-wx, fl1_y = 28-wy;
	int fl2_x = 73-wx, fl2_y = 31-wy;
	int fl3_x = 94-wx, fl3_y = 29-wy;

	PaintFlame(&fl1,&fl1_mask,fl1_x,fl1_y,frame/70);
	PaintFlame(&fl2,&fl2_mask,fl2_x,fl2_y,frame/80);
	PaintFlame(&fl3,&fl3_mask,fl3_x,fl3_y,frame/90);


	if (fr>4000)
	{
		char str[16];
		int len = sprintf_s(str,16,"%d",*score);

		int cx = (s->w - (5*len-1))/2;

		fnt.Paint(s,cx,tx_y-wy+2,0x06,str);

		cx = (s->w - 11)/2;
		inter_print(s,cx,tx_y-wy+1, "YOUR SCORE:", 0x0F);
	}

	// paint tx comming out of fog!
	//tx.Paint(s,tx_x,tx_y-wy,0,0);
	if (fr<3000)
	for (int y=0; y<tx.height; y++)
	{
		for (int x=0; x<tx.width; x++)
		{
			if (tx_mask[x+y*tx.width] > (twister_rand()&0x3))
			{
				int dst=x+tx_x+(tx_y-wy+y)*(s->w+1);
				int src=x+y*(tx.width+1);

				if (s->color)
				{
					if (tx.data->shade[0][src]!='*')
					{
						s->buf[dst] = tx.data->shade[0][src];
						s->color[dst] = tx.data->color[0][src];
					}
				}
				else
				{
					if (tx.data->mono[0][src]!='*')
					{
						s->buf[dst] = tx.data->mono[0][src];
					}
				}
			}
		}
	}
	else
	if (fr<4000)
	{
		tx.Paint(s,tx_x,tx_y-wy,0,0,-1,-1,0,false);
	}
	else
	if (fr<5000)
	{
		// split in half
		int dx = (fr-4000)*(fr-4000)/1024;
		tx.Paint(s,tx_x-dx,tx_y-wy,0,0,tx.width/2,-1,0,false);
		tx.Paint(s,tx_x+dx+tx.width/2,tx_y-wy,0,0,tx.width-tx.width/2,-1,0,false);
	}

	if (fr>4000)
	{
		if ((fr&0x3FF)<0x200)
			prompt.attrib_over=0x07;
		else
			prompt.attrib_over=0x0F;

		int pr_y = s->h-2 - 4*(s->h-25)/25;

		if (s->color || (fr&0x2FF)<0x200)
			prompt.Paint(s, (s->w-prompt.width)/2 , pr_y, 0,0);
	}

	s->Write(dw,dh,0,0,-1,-1);

	if (s->color)
		s->tcolor = cl_transp;

	int ret = InterScreenInput();

	if (ret==-2)
		return -2;

	if (ret && fr>4000)
		return ret;

	return 0;
}
