#include <cstdio>
#include <cmath>
#include <cstring>

#include "inter.h"
#include "spec.h"
#include "twister.h"
#include "conf.h"

#include "unmo3.h"

void inter_print(SCREEN* s, int dx, int dy, const char* str, char color)
{
	int sx=0;
	int sy=0;
	int sh=1;
	int sw=strlen(str);

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

	const char* ptr = str+sx;
	for (int x=dx; x<dx+sw; ptr++,x++)
	{
		if (*ptr!='*')
		{
			s->buf[(s->w+1)*dy + x] = *ptr;
			if (s->color)
				s->color[(s->w+1)*dy + x] = color;
		}
	}

	/*
	memcpy( s->buf + (s->w+1)*dy + dx, str+sx, sw);

	if (s->color)
		memset( s->color + (s->w+1)*dy + dx, color, sw);
	*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PERF::~PERF()
{
}

PERF::PERF(int _idx, const char* _name, int _val, int _minval, int _maxval, int _speed, int _bonus) :
	prgs(&perf_prgs),
	tick(&perf_tick),
	line(&perf_line),
	horz(&perf_horz),
	icon(&perf_icon),
	fnt(&digits_large)
{
	idx = _idx;
	name = _name;
	val = _val;
	minval = _minval;
	maxval = _maxval;
	speed = _speed;
	bonus = _bonus;

	disabled = false;
	state = 0;

	if (minval < maxval)
	{
		if (val<=minval)
			disabled = true;
		else
		if (val>maxval)
			maxval=val;
	}
	else
	if (minval > maxval )
	{
		if (val>=minval)
			disabled = true;
		else
		if (val<maxval)
			maxval=val;
	}
	else // disabled by limits
		disabled = true;

	if (disabled)
		state = 2;

	pro = 0;
}

void PERF::Paint(SCREEN* s, int x, int y, int width)
{
	// note: if width < 0  do not paint any lines
	//       if 0 do not paint horizontal extensions
	//       otherwise it is length of horizontal line

	int w=25;
	int h=15;

	// paint name
	// if state is 1 clr is yellow, otherwise white
	int len = strlen(name);
	int nx = x + (w-len)/2;
	inter_print(s, nx,y, name, state==2 ? 0x0B : 0x0F);

	// paint curval (or val?)
	char valstr[10];
	int vallen = 0;
	int valofs = 0;
	if (idx==0)
	{
		vallen = sprintf_s(valstr,10,"%d%%",maxval ? 100*val/maxval : 0);
	}
	else
	if (idx==1)
	{
		vallen = sprintf_s(valstr,10,"%d:%02d",val/60000,(val/1000)%60);
	}
	else
	if (idx==2)
	{
		valofs = icon.width+1;
		vallen = sprintf_s(valstr,10,"%d",val);
	}

	int vx = x + (w-(vallen*5-1 + valofs))/2;
	fnt.Paint(s,vx+valofs,y+1,0x02,valstr);

	if (idx==2)
	{
		// paint icon
		icon.Paint(s,vx,y+2, 0,0,-1,-1,0,false);
	}

	int px = x+(w-prgs.width)/2;
	if (disabled)
	{
		prgs.SetFrame(2);
		prgs.Paint(s,px,y+7,0,0,-1,-1,0,false);
	}
	else
	if (state==0)
	{
		int len1 = 2+ 23*(val-minval)/(maxval-minval);
		int len2 = prgs.width-len1;
		prgs.SetFrame(1);
		prgs.Paint(s,px,y+7,0,0,len1,-1,0,false);
		prgs.SetFrame(2);
		prgs.Paint(s,px+len1,y+7,len1,0,len2,-1,0,false);
	}
	else
	if (state==1)
	{
		int len0 = pro;
		int len1 = 2+ 23*(val-minval)/(maxval-minval) - len0;
		int len2 = prgs.width - len1 - len0;
		prgs.SetFrame(0);
		prgs.Paint(s,px,          y+7,0,        0,len0,-1,0,false);

		prgs.SetFrame(1);
		prgs.Paint(s,px+len0,     y+7,len0,     0,len1,-1,0,false);

		prgs.SetFrame(2);
		prgs.Paint(s,px+len0+len1,y+7,len0+len1,0,len2,-1,0,false);
	}
	else
	if (state==2)
	{
		// same as state 0 but red instead of blue
		int len1 = 2+ 23*(val-minval)/(maxval-minval);
		int len2 = prgs.width-len1;
		prgs.SetFrame(0);
		prgs.Paint(s,px,y+7,0,0,len1,-1,0,false);
		prgs.SetFrame(2);
		prgs.Paint(s,px+len1,y+7,len1,0,len2,-1,0,false);
	}


	if (!disabled)
	{
		// paint prg tick (only if !disabled)

		int tx = px + 2+23*(val-minval)/(maxval-minval);
		if (tx > px+24)
			tx = px+24;

		tick.Paint(s,tx,y+7,0,0,-1,-1,0,false);

		// paint minval & maxval (only if !disabled)
		char minstr[10];
		int minlen;
		char maxstr[10];
		int maxlen;
		if (idx==0)
		{
			minlen = sprintf_s(minstr,10,"%d%%",0);
			maxlen = sprintf_s(maxstr,10,"%d%%",100);
		}
		else
		if (idx==1)
		{
			minlen = sprintf_s(minstr,10,"%d:%02d",minval/60000,(minval/1000)%60);
			maxlen = sprintf_s(maxstr,10,"%d:%02d",maxval/60000,(maxval/1000)%60);
		}
		else
		if (idx==2)
		{
			minlen = sprintf_s(minstr,10,"%d",minval);
			maxlen = sprintf_s(maxstr,10,"%d",maxval);
		}

		inter_print(s, x+1,y+9, minstr, 0x07);
		inter_print(s, x+w-1-maxlen,y+9, maxstr, 0x07);
	}

	int line_ofs[3]={11,9,7};
	int lx = x + line_ofs[idx];

	// paint line
	// if state is 1 frame is odd otherwise even
	if (width>=0)
	{
		line.SetFrame(2*idx + (state&1));
		line.Paint(s,lx,y+9,0,0,-1,-1,0,false);
	}

	// paint horz ext
	if (width>0 && idx!=1)
	{
		horz.SetFrame( (state&1) );

		if (idx==0)
			horz.Paint(s,lx+line.width,y+13,0,0,width,-1,0,false);
		else
		if (idx==2)
			horz.Paint(s,lx-width,y+13,0,0,width,-1,0,false);
	}
}

int PERF::Animate(unsigned long t) // return progress value (from min to val)
{
	if (disabled)
		return 0;

	if (state==2)
		return bonus;

	if (state==0)
	{
		// remember initial time
		state = 1;
		time = t;
	}

	int dt = t-time;

	pro = 2+ 23*dt/speed;

	float nt = (float)dt/speed;
	int score = (int)floorf(nt*bonus*(maxval-minval)/(val-minval));
	if (score>bonus)
		score = bonus;

	if (minval<maxval)
	{
		if (dt*(maxval-minval) > speed*(val-minval))
		{
			score = bonus;
			pro = 2+ 23*(val-minval)/(maxval-minval);
			state = 2;
		}
	}
	else
	{
		if (dt*(maxval-minval) < speed*(val-minval))
		{
			score = bonus;
			pro = 2+ 23*(val-minval)/(maxval-minval);
			state = 2;
		}
	}

	return score;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DLG::~DLG()
{
	if (text)
		delete [] text;
}

DLG::DLG(const ASSET* _frame, int _tx, int _ty, unsigned char _cl, const char* _name, int _nx, int _ny, const ASSET* _avatar, unsigned long _mix, int _ax, int _ay, int _vx, int _vy) :
	frame(_frame),
	avatar(_avatar)
{
	cl = _cl;
	tx = _tx;
	ty = _ty;
	vx = _vx;
	vy = _vy;

	name = _name;
	nx = _nx;
	ny = _ny;

	vox = 0;

	mix = _mix;
	ax=_ax;
	ay=_ay;

	lines=0;
	chars=0;
	text = 0;

	done=true;
}

void DLG::Start(const char* txt, unsigned long t)
{
	if (text)
		delete [] text;
	text=0;

	done=false;
	start = t;
	time = t;

	vox=0;

	lines=0;
	chars=0;

	int nl = strlen(conf_player.name);
	int len = strlen(txt);

	if (txt)
	{
		const char* ptr = txt;

		while (1)
		{
			const char* n = strchr(ptr,'%');
			if (n)
			{
				chars+=nl-1;
				ptr = n+1;
			}
			else
				break;
		}

		ptr = txt;
		while (1)
		{
			lines++;
			const char* n = strchr(ptr,'\n');
			if (n)
			{
				chars+=n-ptr;
				ptr=n+1;
			}
			else
			{
				chars+=len-(ptr-txt);
				break;
			}
		}

		// alloc str
		text = new char [chars+lines];

		char* dst = text;
		ptr = txt;
		while (1)
		{
			const char* n = strchr(ptr,'%');
			if (n)
			{
				memcpy(dst,ptr,n-ptr);
				memcpy(dst+(n-ptr),conf_player.name,nl);
				dst += (n-ptr) + nl;
				ptr = n+1;
			}
			else
			{
				memcpy(dst,ptr,len-(ptr-txt));
				dst[len-(ptr-txt)]=0;

				int a=0;
				break;
			}
		}

	}
	else
		done=true;
}

bool DLG::Animate(unsigned long t)
{
	// calc vox
	// 200 chars / sec ?
	time=t;
	int len = (time-start)/20;
	if (len > chars)
	{
		done = true;
		len = chars;
	}

	// get some amplitude(t) from octave generator
	// inc/dec current vox value in direction of amplitude
	// inc should run faster than dec

	float phase = len*0.2f;
	float amp = floorf(20.0f * ((sinf(phase)+sinf(phase*2.0f)+sinf(phase*4.0f))/6.0f + 0.5f));
	if (amp>15.0f)
		amp=15.0f;

	if (done)
		amp=0;

	if (vox<amp)
	{
		int dec = 4;

		int dvox = (int)((amp-vox)/dec);
		if (dvox>16/dec)
			dvox=16/dec;
		if (dvox<1)
			dvox=1;
		if (vox+dvox>amp)
			dvox = (int)(amp-vox);
		vox+=dvox;
	}
	else
	{
		int dec = 4;
		if (done)
			dec = 8;

		int dvox = (int)((vox-amp)/dec);
		if (dvox>16/dec)
			dvox=16/dec;

		if (dvox<1)
			dvox=1;

		if (vox-dvox<amp)
			dvox = (int)(vox-amp);
		vox-=dvox;
	}

	return done;
}

void DLG::Finish()
{
	done = true;
}

void DLG::Clear()
{
	if (text)
		delete [] text;

	vox=0;
	text=0;
	lines=0;
	chars=0;
	done=true;
}

void DLG::Paint(SCREEN* s, int x, int y, bool blend)
{
	// paint frame
	frame.Paint(s,x,y,0,0,-1,-1,0,blend);

	// paint face
	avatar.SetFrame((mix>>0)&0xFF);
	avatar.Paint(s,x+ax,y+ay+0,0,0,-1,4,0,false);

	avatar.SetFrame((mix>>8)&0xFF);
	avatar.Paint(s,x+ax,y+ay+4,0,4,-1,1,0,false);

	avatar.SetFrame((mix>>16)&0xFF);
	avatar.Paint(s,x+ax,y+ay+5,0,5,-1,1,0,false);

	avatar.SetFrame((mix>>24)&0xFF);
	avatar.Paint(s,x+ax,y+ay+6,0,6,-1,2,0,false);

	// paint name w/color
	int nl = strlen(name);

	unsigned char xl = cl;
	if (avatar.data == &character) // alien and commander get white name
		xl = cl | 0x0F;

	int dy = y+ny;
	if (dy>=0 && dy<s->h)
	for (int i=0; i<16; i++)
	{
		int dx = x+nx+i;
		if (dx>=0 && dx<s->w)
		{
			char c = ' ';
			int ic = i-(16-nl)/2;
			if (ic>=0 && ic<nl)
				c=name[ic];
			s->buf[dy*(s->w+1)+dx] = c; // < name
			if (s->color)
			{
				s->color[dy*(s->w+1)+dx] = xl;
				if (dy-1>=0 && dy-1<s->h)
					s->color[(dy-1)*(s->w+1)+dx] = ( s->color[(dy-1)*(s->w+1)+dx] & 0xF0 ) | ( cl & 0x0F );
			}
		}
	}

	// paint vox w/color
	if (s->color)
	{
		int dy = y+vy;
		if (dy>=0 && dy<s->h)
		{
			for (int i=0; i<vox; i++)
			{
				int dx1 = x+vx+14-i;
				int dx2 = x+vx+16+i;

				unsigned char c = cl&0x0F;
				if (i*3>vox*2)
					c = (cl>>4)&0x0F;

				if (dx1>=0 && dx1<s->w)
				{
					s->buf[dx1+dy*(s->w+1)]='<';
					s->color[dx1+dy*(s->w+1)] = (s->color[dx1+dy*(s->w+1)]&0xF0) | c;
				}
				if (dx2>=0 && dx2<s->w)
				{
					s->buf[dx2+dy*(s->w+1)]='>';
					s->color[dx2+dy*(s->w+1)] = (s->color[dx2+dy*(s->w+1)]&0xF0) | c;
				}
			}
		}
	}
	else
	{
		int dy = y+vy;
		if (dy>=0 && dy<s->h)
		{
			for (int i=0; i<vox; i++)
			{
				int dx1 = x+vx+14-i;
				int dx2 = x+vx+16+i;

				if (dx1>=0 && dx1<s->w)
				{
					s->buf[dx1+dy*(s->w+1)]='<';
				}
				if (dx2>=0 && dx2<s->w)
				{
					s->buf[dx2+dy*(s->w+1)]='>';
				}
			}
		}
	}

	// paint text w/color
	if (text)
	{
		int len = (time-start)/20;
		if (done || len>chars)
			len = chars;

		int dx = x+tx;
		int dy = y+ty + (5-lines)/2;

		for (int i=0,c=0; c<len; i++)
		{
			if (text[i]=='\n')
			{
				dy++;
				dx=x+tx;
			}
			else
			{
				if (dx>=0 && dx<s->w && dy>=0 && dy<s->h)
				{
					s->buf[dx+dy*(s->w+1)]=text[i];
					if (s->color)
						s->color[dx+dy*(s->w+1)]=(s->color[dx+dy*(s->w+1)]&0xF0) | (cl&0x0F);
				}
				dx++;
				c++;
			}
		}

		// cursor
		if (!done && dx>=0 && dx<s->w && dy>=0 && dy<s->h)
		{
			int cc = dx+dy*(s->w+1);
			if (s->color)
				s->color[cc]=cl<<4;
			else
			{
				s->buf[cc]='[';
				if (dx+1<s->w)
					s->buf[cc+1]=']';
			}

		}

	}
}

INTER_MODAL::~INTER_MODAL()
{
}

INTER_MODAL::INTER_MODAL(SCREEN* _s,
						 int _lives, int _startlives, int* _score,
						 const char* _course_name, const LEVEL* _current_level,
						 int _time, int _hitval, int _hitmax) :

	dlg_player(&dialog_left, 20,5, 0x3B, conf_player.name, 18,2, &avatar, conf_player.avatar, 3,2, 40,12),
	dlg_commander(&dialog_right, 5,5, 0x2A, "Commander", 40,12, &character, 0x00000000, 58,4, 3,2),
	dlg_alien(&dialog_right, 5,5, 0x6E, "??????????????", 40,12, &character, 0x01010101, 58,4, 3,2),
	bkgnd(&menu_bkg),
	perf_clear(0, " A R E A  C L E A R ", _hitval, 0, _hitmax, 1000, _hitmax ? 100*100*_hitval/_hitmax : 0),
	perf_time(1, " T I M E ", _time, _current_level->time_limit, _current_level->time_perfect, 1000, (_current_level->time_limit-_time)/10),
	perf_lives(2, " S A V I N G S ", _lives, 1, _startlives, 1000, (_lives-1)*1000),
	fnt(&digits_large)
{

	PlayLoop(0,28,28,29);

	time = _time;
	score = _score;
	bonus = 0;

	s=_s;

	w = s->w;
	h = s->h;

	dialog_idx=0;
	level = _current_level;

	phase = -4;
	phase_t = get_time();

	dlg[0] = 0;
	dlg[1] = 0;

	if (level->dialog && level->dialog[dialog_idx].text)
	{
		if (level->dialog[dialog_idx].who=='P')
			dlg[1] = &dlg_player;
		else
		if (level->dialog[dialog_idx].who=='A')
			dlg[1] = &dlg_alien;
		else
		if (level->dialog[dialog_idx].who=='C')
			dlg[1] = &dlg_commander;
	}

	interference = 0;
	inter_gap = twister_rand()&0xFF;
	inter_pos = 0;

	prev_t = get_time();

	if (dlg[1] == &dlg_alien)
		interference = 2;
	else
	if (level->dialog && level->dialog[dialog_idx].text && level->dialog[dialog_idx+1].who=='A')
		interference = 1;
}

int INTER_MODAL::Run()
{
	// resize screen handler
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

	unsigned int t = get_time();

	inter_pos += t-prev_t;
	if (inter_pos>inter_gap)
	{
		if (inter_gap<256)
		{
			inter_pos=256;
			if (interference<2) // longer gaps
			{
				inter_gap=256 + (twister_rand()&0x1FF);
			}
			else
			{
				inter_gap=256 + (twister_rand()&0xFF);
			}
		}
		else
		{
			inter_pos=0;
			inter_gap=twister_rand()&0xFF;
		}
	}

	prev_t = t;

	int perf_w = 80;
	int perf_h = 23;
	int perf_x = (s->w - perf_w)/2;
	int perf_y = (s->h - perf_h)/2;

	int area_x  = perf_x + 0;
	int area_y  = perf_y + 4;
	int time_x  = perf_x + 27;
	int time_y  = perf_y + 0;
	int live_x  = perf_x + 54;
	int live_y  = perf_y + 4;
	int score_y = perf_y + 15;

	int x0,y0;
	int x1,y1;

	// place y0,y1 in the middle

	x0 = (s->w - dlg_player.frame.width)/2;
	y0 = (s->h+1)/2 - dlg_player.frame.height;

	x1 = x0;
	y1 = y0 + dlg_player.frame.height -2;

	if (phase==-4)
	{
		int duration = 500;
		int d = duration;
		int d2 = duration*duration;
		int tm[4] =
		{
			static_cast<int>(t-phase_t)-0,
			static_cast<int>(t-phase_t)-0,
			static_cast<int>(t-phase_t)-0,
			static_cast<int>(t-phase_t)-0,
		};

		int pos = t-phase_t;
		bool done=false;
		if (pos>duration*2)
			done=true;

		// random blend...
		if (s->color)
		{
			for (int y=0,dst=0; y<s->h; y++,dst++)
			{
				int src = (bkgnd.width+1)*y;
				for (int x=0; x<s->w; x++,dst++,src++)
				{
					if ((twister_rand()&3) == 0)
					{
						s->buf[dst] =  bkgnd.data->shade[0][src];
						s->color[dst] =  bkgnd.data->color[0][src];
					}
				}
			}
		}
		else
		{
			for (int y=0,dst=0; y<s->h; y++,dst++)
			{
				int src = (bkgnd.width+1)*y;
				for (int x=0; x<s->w; x++,dst++,src++)
				{
					if ((twister_rand()&3) == 0)
					{
						s->buf[dst] =  bkgnd.data->mono[0][src];
					}
				}
			}
		}

		for (int i=0; i<4; i++)
		{
			if (tm[i]<0)
				tm[i]=0;
			if (tm[i]>d)
				tm[i]=d;
			else
				done=false;
		}

		area_x += -s->w + tm[0]* tm[0]*s->w/(d2);
		time_y += -s->h + tm[1]* tm[1]*s->h/(d2);
		live_x +=  s->w - tm[2]* tm[2]*s->w/(d2);
		score_y += s->h - tm[3]* tm[3]*s->h/(d2);

		int key = InterScreenInput();
		if (key==-2)
			return -2;

		if (done)
		{
			phase_t = t;
			phase = -3;
		}
	}
	else
	{
		bkgnd.Paint(s,0,0,0,0,-1,-1,0,false);
	}

	if (phase==-3)
	{
		// wait a sec ...
		int duration = 1000;
		bool done = false;
		int dt = t-phase_t;
		if ( dt>duration )
			done = true;

		int key = InterScreenInput();
		if (key==-2)
			return -2;

		if (done)
		{
			phase_t = t;
			phase = -2;
		}
	}
	else
	if (phase==-2)
	{
		// count up score by bonus
		// can speed up by key press(?)
		bool done = false;

		bonus = 0;

		bool steps[3];
		steps[0] = perf_clear.state==2;
		steps[1] = perf_time.state==2;
		steps[2] = perf_lives.state==2;

		{
			int b = perf_clear.Animate(t);
			bonus += b;
		}

		if (steps[0])
		{
			int b = perf_time.Animate(t);
			bonus += b;
		}

		if (steps[1])
		{
			int b = perf_lives.Animate(t);;
			bonus += b;
		}

		if (steps[0] && steps[1] && steps[2])
			done=true;

		int key = InterScreenInput();
		if (key==-2)
			return -2;

		if (done)
		{
			*score += bonus;
			bonus = 0;
			phase_t = t;
			phase = -1;
		}
		else
		{
			bool next = false;

			if (!steps[0] && (perf_clear.state==2))
				next=true;
			if (!steps[1] && (perf_time.state==2))
				next=true;
			if (!steps[2] && (perf_lives.state==2))
				next=true;

			if (next)
			{
				phase = -3;
				phase_t = t;
			}
		}
	}
	else
	if (phase==-1)
	{
		// wait for key
		int key = InterScreenInput();
		if (key==-2)
			return -2;

		if (key)
		{
			phase_t = t;
			phase = 0;
		}
	}
	if (phase==0)
	{
		if (!dlg[1])
		{
			// no dialogs at all, exit immediately
			return -1;
		}

		int duration = 800;
		int d = 200;
		int d2 = d*d;

		int wait = 0; // extra time to wait

		int pos = t-phase_t;
		bool done=false;

		if (pos>duration+wait)
			done=true;
		if (pos>duration)
			pos=duration;
		pos = 50 - pos*50/duration;
		y1 += pos;

		{
			// hide bonuses & score
			int duration = 800;
			int tm[4] =
			{
				static_cast<int>(t-phase_t),
				static_cast<int>(t-phase_t)-200,
				static_cast<int>(t-phase_t)-100,
				static_cast<int>(t-phase_t)-300,
			};

			//bool done=true;
			for (int i=0; i<4; i++)
			{
				if (tm[i]<0)
					tm[i]=0;
				if (tm[i]>d)
					tm[i]=d;
				else
					done=false;
			}

			area_x +=  -tm[0]*tm[0]*s->w/(d2);
			time_y +=  -tm[1]*tm[1]*s->h/(d2);
			live_x +=  +tm[2]*tm[2]*s->w/(d2);
			score_y += +tm[3]*tm[3]*s->h/(d2);
		}



		int key = InterScreenInput();
		if (key==-2)
			return -2;
		if (done)
		{
			dlg[1]->Start(level->dialog[dialog_idx].text,t);
			dialog_idx++;

			phase_t=t;
			phase=1;
		}
	}


	if (phase==1)
	{
		// lower dialog is talking
		// when done or interrupted by key: phase=2

		int key = InterScreenInput();

		bool done = dlg[1]->Animate(t);

		if (key==-2)
			return -2;

		if (done /*|| key*/)
		{
			phase_t=t;
			phase=2;
		}
	}
	else
	if (phase==2)
	{
		// show full dialog text,
		// wait for key press then:
		// - if there are more dialogs then phase=3, otherwise phase = 4
		// paint 'press any key'

		if (dlg[1]==&dlg_alien)
			interference = 2;
		else
		if (level->dialog[dialog_idx].who == 'A' && !interference)
			interference = 1;

		dlg[1]->Finish();

		dlg[1]->Animate(t); // vox fadeout

		int key = InterScreenInput();
		if (key==-2)
			return -2;
		if (key)
		{
			phase_t=t;
			if (!level->dialog || !level->dialog[dialog_idx].text)
			{
				FadeOut(2000);
				phase=4;
			}
			else
			{
				if (level->dialog[dialog_idx].who=='A')
				{
					interference = 2;
				}
				else
				{
					if (interference)
						interference--;
				}

				phase=3;
			}
		}
		else
		{
			int y = y1 + dlg_player.frame.height - 1;
			int x = x1 + 7;

			unsigned char bl = (t&1023)<512 ? 0x03 : 0x0B;

			memcpy(s->buf+x+(s->w+1)*y,"P R E S S  A N Y  K E Y",23);
			if (s->color)
				memset(s->color+x+(s->w+1)*y, bl, 23);
		}
	}
	else
	if (phase==3)
	{
		int duration = 250;

		int pos = t - phase_t;

		bool done=false;
		if (pos>duration)
		{
			done=true;
			pos=duration;
		}

		pos = pos*(dlg_player.frame.height-2)/duration;

		// swap dialog frames
		// in the middle clear text in frame going down

		y0 += pos;
		y1 -= pos;

		if (y1<y0)
		{
			if (level->dialog[dialog_idx].who=='P')
			{
				// switch to alien
				dlg[0] = &dlg_player;
			}
			else
			if (level->dialog[dialog_idx].who=='A')
			{
				// switch to alien
				dlg[0] = &dlg_alien;
			}
			else
			if (level->dialog[dialog_idx].who=='C')
			{
				// switch to commander
				dlg[0] = &dlg_commander;
			}

			dlg[0]->Clear();
		}

		int key = InterScreenInput();
		if (key==-2)
			return -2;

		if (done /*|| key*/)
		{
			if (level->dialog[dialog_idx].who=='P')
			{
				// switch to alien
				dlg[0] = &dlg_player;
			}
			else
			if (level->dialog[dialog_idx].who=='A')
			{
				// switch to alien
				dlg[0] = &dlg_alien;
			}
			else
			if (level->dialog[dialog_idx].who=='C')
			{
				// switch to commander
				dlg[0] = &dlg_commander;
			}

			DLG* d;
			d = dlg[0]; dlg[0] = dlg[1]; dlg[1] = d;

			int swp;
			swp=x0; x0=x1; x1=swp;
			swp=y0; y0=y1; y1=swp;

			dlg[1]->Start(level->dialog[dialog_idx].text,t);
			dialog_idx++;

			phase_t=t;
			phase=1;
		}
	}
	else
	if (phase==4)
	{
		// shift-out dialog frames (both with full text)

		int duration = 500;

		int pos = t-phase_t;
		bool done = false;

		if (pos>duration)
		{
			done=true;
			pos=duration;
		}

		pos = pos*160/duration;

		x0 += pos;
		x1 -= pos;

		int key = InterScreenInput();
		if (key==-2)
			return -2;
		if (/*key ||*/ done)
		{
			return -1;
		}
	}


	// paint performance
	if (phase<=0 && phase>-5)
	{

		char scorestr[10];
		int scorelen = sprintf_s(scorestr,10,"%d",*score + bonus);

		int width = 20-(scorelen*5-1)/2;

		perf_clear.Paint(s,area_x,area_y,width);
		perf_time.Paint(s,time_x,time_y,0);
		perf_lives.Paint(s,live_x,live_y,width);

		fnt.Paint(s, perf_x + (perf_w-(scorelen*5-1))/2,score_y,0x06,scorestr);

		if (phase<-1)
			inter_print(s, perf_x + (perf_w-11)/2,score_y+7," S C O R E ", 0x0B);
		else
		if (phase==-1)
			inter_print(s, perf_x + (perf_w-25)/2,score_y+7," P R E S S  A N Y  K E Y ", (t&0x3ff) < 0x200 ? 0x03 : 0x0B);
	}

	// paint dialogs
	if (phase>=0)
	{

		if (dlg[0])
			dlg[0]->Paint(s,x0,y0,false);

		if (dlg[1])
			dlg[1]->Paint(s,x1,y1,false);

		if (inter_gap<256)
		{
			if (interference==1)
				Interference(s,0,80,t,1000,0,false);
			else
			if (interference==2)
				Interference(s,0,40,t,1000,0,false);
		}

	}

	// write out screen
	s->Write(dw,dh,0,0,-1,-1);

	return 0;
}
