#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/XKBlib.h>
#include <pthread.h>

#include <X11/extensions/XInput2.h>
#include <pulse/pulseaudio.h>

static bool raw_keyboard = false;
static bool use_xterm_256 = true;

static bool use_xi = false;
static int xi_opcode = -1;
static Display* dpy = 0;
static Window win; // current focus
static Window root; // default root
static Window victim; // our victim

#include <string.h>

#include <signal.h>
#include <sys/time.h>
#include <unistd.h>


#include <sys/ioctl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef FIONREAD
	// CYGWIN
	#include <sys/socket.h>
#elif defined(__APPLE__)
	#include <fcntl.h>
#else
	// LINUX
	#include <linux/kd.h>
	#include <fcntl.h>
	#define LINUX
#endif

// pollution from <pulse/pulseaudio.h>
#ifdef MIN
#undef MIN
#endif

// pollution from <pulse/pulseaudio.h>
#ifdef MAX
#undef MAX
#endif

#include "spec.h"
#include "conf.h"
#include "menu.h"

void DBG(const char* str)
{
}


bool has_key_releases()
{
	return dpy!=0 || raw_keyboard;
}

int fopen_s(FILE** f, const char* path, const char* mode)
{
	if (!f)
		return -1;
	*f = fopen(path,mode);
	return *f==0;
}

unsigned int get_time()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    unsigned int tm = now.tv_sec*1000 + now.tv_usec/1000;
    if (!tm)
		tm=1;
    return tm;
}

static char read_buf[3];
static int  read_len=0;

bool get_input_len(int* r)
{
	if (!dpy)
	{
		ioctl( fileno(stdin), FIONREAD, r);
		r+=read_len;
		return true;
	}

	*r = XPending(dpy);
    return true;
}

char read_char()
{
	while (1)
	{
		if (!read_len)
		{
			int nm=0;
			get_input_len(&nm);
			if (!nm)
				return 0;
			read_buf[0] = getchar();
			read_len++;
		}

		if (read_buf[0]==27)
		{
			if (read_len==1)
			{
				int nm=0;
				get_input_len(&nm);
				if (!nm)
				{
					read_len=0;
					return 27;
				}
				read_buf[1] = getchar();
				read_len=2;
			}

			if (read_buf[1]!='[')
			{
				read_len--;
				if (read_len)
					memmove(read_buf, read_buf+1, read_len);
				return 27;
			}

			if (read_len==2)
			{
				int nm=0;
				get_input_len(&nm);
				if (!nm)
					return 0;
				read_buf[2]=getchar();
				read_len=3;
			}

			char ch=0;
			switch (read_buf[2])
			{
				case 'A':
					ch = KBD_UP;
					break;
				case 'B':
					ch = KBD_DN;
					break;
				case 'C':
					ch = KBD_RT;
					break;
				case 'D':
					ch = KBD_LT;
					break;

				case 72: // LX
					ch = KBD_HOM;
					break;
				case 70: // LX
					ch = KBD_END;
					break;
			}

			if (ch)
			{
				read_len=0;
				return ch;
			}

			int nm=0;
			get_input_len(&nm);
			if (!nm)
				return 0;
			char tilde = getchar();

			switch (read_buf[2])
			{
				case 55: // rxvt
				case 49:
					ch = KBD_HOM;
					break;
				case 50:
					ch = KBD_INS;
					break;
				case 51:
					ch = KBD_DEL;
					break;
				case 56: // rxvt
				case 52:
					ch = KBD_END;
					break;
				case 53:
					ch = KBD_PUP;
					break;
				case 54:
					ch = KBD_PDN;
					break;
			}

			read_len=0;
			if (tilde!='~')
			{
				read_buf[0] = tilde;
				read_len = 1;
			}

			return ch;
		}
		else
		{
			char ch = read_buf[0];
			read_len--;
			if (read_len)
				memmove(read_buf, read_buf+1, read_len);

			if (ch==10)
				ch=13; // enter
			if (ch==127)
				ch=8;  // bk space

			return ch;
		}
	}
}


static const int key_map[]=
{
	0,0,0,0,0,0,0,0,

	 0,27,'!1','@2','#3','$4','%5','^6','&7','*8','(9',')0','_-','+=',8,
	   '\t','Qq','Ww','Ee','Rr','Tt','Yy','Uu','Ii','Oo','Pp','{[','}]',13,
	        0,'Aa','Ss','Dd','Ff','Gg','Hh','Jj','Kk','Ll',':;','\"\'',
   '~`',0,'|\\','Zz','Xx','Cc','Vv','Bb','Nn','Mm','<,','>.','?/',0,
};

bool spec_read_input(CON_INPUT* ir, int n, int* r)
{
	int nm = 0;
	get_input_len(&nm);

	n = n>nm ? nm : n;

	static int caps=0;
	static int l_shift=0;
	static int r_shift=0;

	static int ctrl=0;

	if (raw_keyboard)
	{
		int i;
		for (i=0; i<n; i++)
		{
			int res;
			unsigned char scan;

			res = read(0, &scan, 1);
			if (!res)
			{
				break;
			}

			switch (scan & 0x7F)
			{
				case 0xe0: // escape code
					scan=0;
					break;

				case 0x2a:
					l_shift = scan<0x80;
					scan=0;
					break;

				case 0x36:
					r_shift = scan<0x80;
					scan=0;
					break;

				case 0x1d:
					// l-ctrl is 0x1d
					// r-ctrl is prefixed with 0xe0
					ctrl = scan<0x80;
					scan=0;
					break;

				case 66:
					// capslock
					scan=0;
					break;
			}

			if (scan)
			{
				ir[i].EventType = CON_INPUT_KBD;
				ir[i].Event.KeyEvent.bKeyDown = (scan & 0x80)==0;
				ir[i].Event.KeyEvent.uChar.AsciiChar = 0;

				int c = (scan+8)&0x7f;

				switch (c)
				{
					case 65: ir[i].Event.KeyEvent.uChar.AsciiChar = ' '; break;
					case 'n': ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_HOM; break;
					case 'o': ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_UP; break;
					case 'p': ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_PUP; break;
					case 'q': ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_LT; break;
					case 'r': ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_RT; break;
					case 's': ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_END; break;
					case 't': ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_DN; break;
					case 'u': ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_PDN; break;
					case 'v': ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_INS; break;
					case 'w': ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_DEL; break;

					default:
					{
						if (c*sizeof(int)<sizeof(key_map))
						{
							int s = (l_shift+r_shift) > 0;
							if ((key_map[c]&0xFF) >='a' && (key_map[c]&0xFF) <='z')
								s ^= caps;

							if (s && (key_map[c]&0xFF00))
								ir[i].Event.KeyEvent.uChar.AsciiChar = (key_map[c]>>8)&0xFF;
							else
								ir[i].Event.KeyEvent.uChar.AsciiChar = key_map[c]&0xFF;
						}
					}
				}

				if ( ctrl &&
					 (ir[i].Event.KeyEvent.uChar.AsciiChar == 'c' ||
					  ir[i].Event.KeyEvent.uChar.AsciiChar == 'C') )
				{
					// do not put ^C
					i--;
					raise(SIGINT);
				}
			}
			else
			{
				// skip
				i--;
			}
		}

		if (r)
			*r=i;
		return true;
	}

	if (!dpy)
	{
		for (int i=0; i<n; i++)
		{
			int ch = read_char();
			if (ch<=0)
			{
				if (r)
					*r = i;
				return true;
			}

			ir[i].EventType = CON_INPUT_KBD;
			ir[i].Event.KeyEvent.bKeyDown = true;
			ir[i].Event.KeyEvent.uChar.AsciiChar = ch;
		}

		if (r)
			*r = n;

	    return true;
	}

	// in non grabbed way: eat all from stdin if submitted
	int discard;
	ioctl( fileno(stdin), FIONREAD, &discard);
	while (discard)
	{
		char chunk[256];
		int bytes = discard > 256 ? 256 : discard;
		discard -= fread(chunk,1,bytes,stdin);
	}

	int i=0;
	for (int j=0; j<nm && i<n; j++)
	{
		XEvent ev;

		XGenericEventCookie *cookie = &ev.xcookie;
		XNextEvent(dpy, &ev);

		if (use_xi)
		{
	            if (cookie->type != GenericEvent ||
        		cookie->extension != xi_opcode)
	                continue;

	            if (XGetEventData(dpy, cookie))
	            {

			if (cookie->evtype == 2 || cookie->evtype == 3)
			{
			    XIDeviceEvent* key = (XIDeviceEvent*)cookie->data;

				ir[i].EventType = CON_INPUT_KBD;
				ir[i].Event.KeyEvent.bKeyDown = cookie->evtype == 2;
				ir[i].Event.KeyEvent.uChar.AsciiChar = 0;

				int c = key->detail;

				switch (c&0x7f)
				{
					case 0xe0+8: // escape code
						break;

					case 0x2a+8:
						l_shift = ir[i].Event.KeyEvent.bKeyDown;
					break;

					case 0x36+8:
						r_shift = ir[i].Event.KeyEvent.bKeyDown;
					break;

					case 66:
					{
						//if (ir[i].Event.KeyEvent.bKeyDown)
						{
							unsigned int n=0;
							XkbGetIndicatorState(dpy, XkbUseCoreKbd, &n);
							caps = n&1;
						}
						break;
					}

					case  65: ir[i].Event.KeyEvent.uChar.AsciiChar = ' '; break;
					case 110: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_HOM; break;
					case 111: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_UP; break;
					case 112: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_PUP; break;
					case 113: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_LT; break;
					case 114: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_RT; break;
					case 115: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_END; break;
					case 116: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_DN; break;
					case 117: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_PDN; break;
					case 118: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_INS; break;
					case 119: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_DEL; break;

					default:
						if (c*sizeof(int)<sizeof(key_map))
						{
							int s = (l_shift+r_shift) > 0;
							if ((key_map[c]&0xFF) >='a' && (key_map[c]&0xFF) <='z')
								s ^= caps;

							if (s && (key_map[c]&0xFF00))
								ir[i].Event.KeyEvent.uChar.AsciiChar = (key_map[c]>>8)&0xFF;
							else
								ir[i].Event.KeyEvent.uChar.AsciiChar = key_map[c]&0xFF;
						}
				}

				i++;
				break;

			}

	                XFreeEventData(dpy, &ev.xcookie);
			continue;
	            }
		    // continue;
		}

		switch(ev.type)
		{
			case FocusIn:
				{
					unsigned int n=0;
					XkbGetIndicatorState(dpy, XkbUseCoreKbd, &n);
					caps = n&1;
				}
				break;

			case FocusOut:

				int revert;
				XGetInputFocus(dpy, &win, &revert);

				if (win == PointerRoot)
                		    win = root;

				XSelectInput(dpy, win, KeyPressMask|KeyReleaseMask|FocusChangeMask);

				if (ev.xfocus.window == victim)
				{
					ir[i].EventType = CON_INPUT_FOC;
					ir[i].Event.FocusEvent.bSetFocus = 0;
					i++;
				}

			    break;

			case KeyPress:
			case KeyRelease:
			{
				if (ev.xkey.window != victim)
					break;

				ir[i].EventType = CON_INPUT_KBD;
				ir[i].Event.KeyEvent.bKeyDown = ev.type == KeyPress;
				ir[i].Event.KeyEvent.uChar.AsciiChar = 0;

				int c = ev.xkey.keycode;

				switch (c&0x7f)
				{
					case 0xe0+8: // escape code
						break;

					case 0x2a+8:
						l_shift = ir[i].Event.KeyEvent.bKeyDown;
					break;

					case 0x36+8:
						r_shift = ir[i].Event.KeyEvent.bKeyDown;
					break;

					case 66:
					{
						//if (ir[i].Event.KeyEvent.bKeyDown)
						{
							unsigned int n=0;
							XkbGetIndicatorState(dpy, XkbUseCoreKbd, &n);
							caps = n&1;
						}
						break;
					}

					case  65: ir[i].Event.KeyEvent.uChar.AsciiChar = ' '; break;
					case 110: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_HOM; break;
					case 111: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_UP; break;
					case 112: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_PUP; break;
					case 113: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_LT; break;
					case 114: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_RT; break;
					case 115: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_END; break;
					case 116: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_DN; break;
					case 117: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_PDN; break;
					case 118: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_INS; break;
					case 119: ir[i].Event.KeyEvent.uChar.AsciiChar = KBD_DEL; break;


					default:
						if (c*sizeof(int)<sizeof(key_map))
						{
							int s = (l_shift+r_shift) > 0;
							if ((key_map[c]&0xFF) >='a' && (key_map[c]&0xFF) <='z')
								s ^= caps;

							if (s && (key_map[c]&0xFF00))
								ir[i].Event.KeyEvent.uChar.AsciiChar = (key_map[c]>>8)&0xFF;
							else
								ir[i].Event.KeyEvent.uChar.AsciiChar = key_map[c]&0xFF;
						}
				}

				i++;
				break;
			}
		}
	}

	if (r)
		*r = i;

	return true;
}

void sleep_ms(int ms)
{
	usleep(ms*1000);
}

void vsync_wait()
{
	sleep_ms(8);
}

static struct termios old, cur;
static int old_keyboard_mode;

/* Initialize new terminal i/o settings */
static void initTermios()
{
	tcgetattr(0, &old); /* grab old terminal i/o settings */

#ifdef LINUX
    struct termios tty_attr;
    int flags;

    /* save old keyboard mode */
    if (ioctl(0, KDGKBMODE, &old_keyboard_mode) >= 0)
	{

		/* make stdin non-blocking */
		// originaly before (save old keyboard mode)
		flags = fcntl(0, F_GETFL);
		flags |= O_NONBLOCK;
		fcntl(0, F_SETFL, flags);

		/* turn off buffering, echo and key processing */
		tty_attr = old;
		tty_attr.c_lflag &= ~(ICANON | ECHO | ISIG);
		tty_attr.c_iflag &= ~(ISTRIP | INLCR | ICRNL | IGNCR | IXON | IXOFF);
		tcsetattr(0, TCSANOW, &tty_attr);

		ioctl(0, KDSKBMODE, K_MEDIUMRAW);
		raw_keyboard = true;
	}
	else
#endif
	{
		raw_keyboard = false;

		setbuf(stdin, NULL);
		cur = old;
		cur.c_lflag |=  ISIG;
		cur.c_iflag &= ~IGNBRK;
		cur.c_iflag |=  BRKINT;
		cur.c_lflag &= ~ICANON; /* disable buffered i/o */
		cur.c_lflag &= ~ECHO; /* disable echo mode */
		tcsetattr(0, TCSANOW, &cur);
	}
}

/* Restore old terminal i/o settings */
static void resetTermios(void)
{
	tcsetattr(0, TCSAFLUSH, &old);
#ifndef __APPLE__
	if (raw_keyboard)
		ioctl(0, KDSKBMODE, old_keyboard_mode);
#endif
}

static int ansi_key_autorep(bool on)
{
    if (on)
	return printf("\x1B[?8h");
    else
	return printf("\x1B[?8l");
}

static int ansi_set_wrap(bool on)
{
    if (on)
	return printf("\x1B[?7h");
    else
	return printf("\x1B[?7l");
}

static int ansi_show_cursor(bool show)
{
    if (show)
	return printf("\x1B[?25h");
    else
	return printf("\x1B[?25l");
}

static int ansi_goto_xy(int x, int y)
{
    return printf("\x1B[%d;%df",y+1,x+1);
}

static int ansi_set_color(int fg, int bg)
{
    // 8 color pal
    return printf("\x1B[%d;%dm",fg%8+30,bg%8+40);
}

static int ansi_set_color_ex(int fg, int bg)
{
	// LINUX text console
	if (use_xterm_256)
	    return printf("\x1B[38;5;%d;48;5;%dm",fg%256,bg%256);

	// bold & blink
	return printf("\x1B[%d;%d;%s;%sm",fg%8+30,bg%8+40,fg<8?"22":"1",bg<8?"25":"5");
}

static int ansi_set_color_rgb(int fg, int bg)
{
    // RGB color
    return printf("\x1B[38;2;%d;%d;%d;48;2;%d;%d;%d;m",
    	fg&0xff,(fg>>8)&0xff,(fg>>16)&0xff,
    	bg&0xff,(bg>>8)&0xff,(bg>>16)&0xff);
}

static int ansi_def_color()
{
    return printf("\x1B[0m");
}

static int ansi_screen(bool alt)
{
	if (alt)
		return printf("\x1B[?1049h");
	else
		return printf("\x1B[?1049l");
}

static void get_wh(int* w, int* h)
{
    winsize ws;
    ioctl(0, TIOCGWINSZ, &ws);

    if (w)
	*w = ws.ws_col;
    if (h)
	*h = ws.ws_row;
}


void free_sound();

void terminal_done()
{
	if (dpy)
	{
		XCloseDisplay(dpy);
		dpy = 0;
	}

	resetTermios();

    ansi_def_color();
    ansi_show_cursor(true);
    ansi_set_wrap(true);
    ansi_key_autorep(true);

	ansi_screen(false);
	fflush(stdout);

	// most likely will fail, so put it at the end.
	free_sound();
}

static void terminal_signal(int signum)
{
	terminal_done();
	_exit(128 + signum);
}


static void select_events(Display *dpy, Window win)
{
    XIEventMask evmasks[2];
    unsigned char mask1[(XI_LASTEVENT + 7)/8];
    unsigned char mask2[(XI_LASTEVENT + 7)/8];

    memset(mask1, 0, sizeof(mask1));

    /* select for button and key events from all master devices */
//    XISetMask(mask1, XI_ButtonPress);
//    XISetMask(mask1, XI_ButtonRelease);
    XISetMask(mask1, XI_KeyPress);
    XISetMask(mask1, XI_KeyRelease);

    evmasks[0].deviceid = XIAllMasterDevices;
    evmasks[0].mask_len = sizeof(mask1);
    evmasks[0].mask = mask1;

    memset(mask2, 0, sizeof(mask2));

    /* Select for motion from the default cursor */
    XISetMask(mask2, XI_Motion);

    evmasks[1].deviceid = 2; /* the default cursor */
    evmasks[1].mask_len = sizeof(mask2);
    evmasks[1].mask = mask2;

    XISelectEvents(dpy, win, evmasks, 2);
    XFlush(dpy);
}



int terminal_init(int argc, char* argv[], int* dw, int* dh)
{
	// todo:
	// try to setup ansi palette
	// try to enable mouse events (enough to track moves when left button is down only)

	const char* term = getenv("TERM");
	if (strcmp(term,"linux")==0)
		use_xterm_256 = false;

    initTermios();

	LoadConf();

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_handler = terminal_signal;
    act.sa_flags = 0;
    sigaction(SIGHUP,  &act, NULL);
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGXCPU, &act, NULL);
    sigaction(SIGXFSZ, &act, NULL);
    sigaction(SIGIO,   &act, NULL);
    sigaction(SIGPIPE, &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    int cols,rows;
    get_wh(&cols,&rows);

    printf("W=%d, H=%d\n",cols,rows);

	if (raw_keyboard)
	{
		printf("Using RAW Keyboard... excelent!\n");
	}
	else
	{
		printf("Falling back to X11 input\n");
		dpy = XOpenDisplay(NULL);


		if (!dpy)
		{
			printf("unable to connect to X11 display\nfalling back to sticky stdin!\n");
		}
		else
		{



			Bool supported = false;
			XkbSetDetectableAutoRepeat(dpy, True, &supported);

			int revert;

			XGetInputFocus(dpy, &win, &revert);

			if (win == PointerRoot)
				win = root;

			if (win)
			{
			    int event, error;
			    if (XQueryExtension(dpy, "XInputExtension", &xi_opcode,&event,&error))
			    {
				select_events(dpy,win);
				use_xi = true;
			    }
			    else
			    {

				printf("connecting input to window = %lu\n",win);

				victim = win;
				root = DefaultRootWindow(dpy);
				XSelectInput(dpy, win, FocusChangeMask|KeyPressMask|KeyReleaseMask);

				/*
				XGrabKeyboard(dpy, win,
									  False,
									  GrabModeAsync,
									  GrabModeAsync,
									  CurrentTime);
				*/

				XFlush(dpy);
			    }
			}
			else
			{
				XCloseDisplay(dpy);
				dpy = 0;
				printf("unable to get x11 active window\nfalling back to sticky stdin!\n");
			}
		}
	}

	ansi_screen(true);
    ansi_set_wrap(false);
    ansi_show_cursor(false);
    ansi_key_autorep(false);

	*dw=cols;
	*dh=rows;

	return 0;
}

void get_terminal_wh(int* dw, int* dh)
{
	get_wh(dw,dh);
}

void free_con_output(CON_OUTPUT* screen)
{
	char* arr = (char*)screen->arr;
	if (arr)
		delete [] arr;

	screen->arr = 0;
}

int screen_write(CON_OUTPUT* screen, int dw, int dh, int sx, int sy, int sw, int sh)
{
	int w = screen->w;
	int h = screen->h;
	char* buf = screen->buf;
	char* color = screen->color;
	char* arr = (char*)screen->arr;

	ansi_goto_xy(0,0);

	if (!color)
		ansi_set_color_ex(0,15);
	else
	{
		for (int y=0; y<sh; y++)
			color[(w+1)*y+w] = color[(w+1)*y+w-1];
		if (arr)
			for (int y=0; y<sh; y++)
				arr[(w+1)*(y+h)+w] = color[(w+1)*y+w];
	}

	int size = 0;

	if (arr)
	{
		// compare line by line (winner)
		char cr[51];
		memset(cr,'\n',50);
		cr[50]=0;

		int i=0;
		int j=0;
		while (i<sh)
		{
			while (i<sh &&
				memcmp(buf+(w+1)*i,arr+(w+1)*i,w+1)==0 &&
				(!color || memcmp(color+(w+1)*i,arr+(w+1)*(i+h),w+1)==0))
				i++;

			if (i>=sh)
				break;

			if (i>j)
				size += printf("%s",cr + (50 - (i-j)) - (j>0));

			memcpy(arr+(w+1)*i,buf+(w+1)*i,w+1);
			if (color)
				memcpy(arr+(w+1)*(i+h),color+(w+1)*i,w+1);


			j=i+1;
			while (j<sh)
			{
				if (memcmp(buf+(w+1)*j,arr+(w+1)*j,w+1)==0 &&
					(!color|| memcmp(color+(w+1)*j,arr+(w+1)*(j+h),w+1)==0))
				{
					break;
				}
				else
				{
					memcpy(arr+(w+1)*j,buf+(w+1)*j,w+1);
					if (color)
						memcpy(arr+(w+1)*(j+h),color+(w+1)*j,w+1);

				}
				j++;
			}

			// clip
			if (!color)
			{
				char swap = buf[(w+1)*j-1];
				buf[(w+1)*j-1] = 0;

				size += printf("%s",buf+(w+1)*i);

				// unclip
				buf[(w+1)*j-1] = swap;
			}
			else
			{
				int cl = 256;
				int from = (w+1)*i;
				for (int y=i; y<j; y++)
				{
					for (int x=0; x<=sw; x++)
					{
						if (color[(w+1)*y+x]!=cl || x==sw && y==j-1)
						{
							int chars = (w+1)*y+x - from;
							if (chars)
							{
								size += ansi_set_color_ex(cl&0x0F,(cl>>4)&0x0F);
								char swap = buf[(w+1)*y+x];
								buf[(w+1)*y+x]=0;
								printf("%s",buf+from);
								buf[(w+1)*y+x]=swap;
								from = (w+1)*y+x;
								size += chars;
							}
							cl = color[(w+1)*y+x];
						}
					}
				}
			}

			i=j;
		}
	}
	else
	{
		if (!color)
		{
			// clip
			char swap = buf[(w+1)*sh-1];
			buf[(w+1)*sh-1] = 0;

			printf("%s",buf);

			// unclip
			buf[(w+1)*sh-1] = swap;

			arr = new char[(w+1)*h];
			memcpy(arr,buf,(w+1)*h);

			size = (w+1)*sh;

			screen->arr = arr;
		}
		else
		{
			int from = 0;
			int cl = 256;
			for (int y=0; y<sh; y++)
			{
				for (int x=0; x<=sw; x++)
				{
					if (color[(w+1)*y+x]!=cl  || x==sw && y==sh-1)
					{
						int chars = (w+1)*y+x - from;
						if (chars)
						{
							size += ansi_set_color_ex(cl&0x0F,(cl>>4)&0x0F);
							char swap = buf[(w+1)*y+x];
							buf[(w+1)*y+x]=0;
							printf("%s",buf+from);
							buf[(w+1)*y+x]=swap;
							from = (w+1)*y+x;
							size += chars;
						}
						cl = color[(w+1)*y+x];
					}
				}
			}

			arr = new char[(w+1)*h*2];
			memcpy(arr,buf,(w+1)*h*2);

			screen->arr = arr;
		}
	}

	fflush(stdout);
	return size;
}

void terminal_loop()
{
	while (modal)
	{
		if (modal->Run()==-2)
			break;
	}
}

void write_fs()
{
}

const char* conf_path()
{
	static char path[1024]="";

	if (!path[0])
	{
		const char* dir = getenv("ASCII_PATROL_CONFIG_DIR");
		if (!dir || !dir[0])
			dir = getenv("HOME");
		if (!dir || !dir[0])
			dir = ".";
		sprintf_s(path,1024,"%s/asciipat.cfg", dir);
	}

	return path;
}

const char* record_path()
{
	static char path[1024]="";

	if (!path[0])
	{
		const char* dir = getenv("ASCII_PATROL_RECORD_DIR");
		if (!dir || !dir[0])
			dir = getenv("HOME");
		if (!dir || !dir[0])
			dir=".";
		sprintf_s(path,1024,"%s/asciipat.rec", dir);
	}

	return path;
}

const char* shot_path()
{
	static char path[1024]="";

	if (!path[0])
	{
		const char* dir = getenv("ASCII_PATROL_SHOT_DIR");
		if (!dir || !dir[0])
			dir = getenv("HOME");
		if (!dir || !dir[0])
			dir=".";
		sprintf_s(path,1024,"%s/", dir);
	}

	return path;
}

static pthread_mutex_t hiscore_cs = PTHREAD_MUTEX_INITIALIZER;
static pthread_t hiscore_thread = 0;
static HISCORE hiscore_data[2]={{0,0,0,"",""},{0,0,0,"",""}};
static int hiscore_queue=0;
static int hiscore_running=0;

static HISCORE hiscore_async = {0,0,0,"",""};
static bool hiscore_ready = false;

void* hiscore_proc(void* p)
{
	int err = 0;
	bool end = false;

	static char path[1024];
	const char* home = getenv("HOME");
	if (!home || !home[0])
		home=".";
	sprintf_s(path,1024,"%s/asciipat.rnk", home);

	while (!end)
	{
		fflush(0);
		err = system(hiscore_data->buf);

		// check if we need to discard this data and re-fetch
		pthread_mutex_lock(&hiscore_cs);

		HISCORE tmp={0,0,0,"",""};

		if (!err)
		{
			FILE* f = 0;
			fopen_s(&f,path,"rb");
			if (f)
			{
				char line[56];
				if (fgets(line,56,f))
				{
					if ( sscanf(line,"%d/%d",&tmp.ofs,&tmp.tot) != 2 )
					{
						fclose(f);
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

				if (!err)
				{
					tmp.siz = 0;
					while (fgets(line,56,f))
					{
						/*
						// no need to put terminator before avatar
						line[45]=0;
						*/
						memcpy(tmp.buf + 55*tmp.siz, line, 55);
						tmp.siz++;
					}

					// change last cr to eof
					tmp.buf[55 * tmp.siz -1] = 0;
				}

				fclose(f);
			}
			else
				err = -3;
		}
		else
			err = -1;

		if (!err)
		{
			// prevent overwriting id if no new id is returned
			if (hiscore_async.id[0] && !tmp.id[0])
				memcpy(tmp.id,hiscore_async.id,16);

			hiscore_async = tmp;
			hiscore_ready = true;
		}

		hiscore_queue--;

		end = (hiscore_queue==0);

		if (!end)
			hiscore_data[0] = hiscore_data[1];

		pthread_mutex_unlock(&hiscore_cs);

		if (end)
			break;

		sleep_ms(250); // save OS from being flooded by pg-up/dn key auto-repeat
	}

	*(int*)p = 0; // clear running flag

	pthread_exit((void*)(intptr_t)err);
	return (void*)(intptr_t)err;
}

void request_hiscore(const char* cmd)
{
	// need to handle 4 situations
	// 1. no thread is running, copy data[1] to data[0] and create new thread
	// 2. thread is running but queue len is 0, wait for thread to finish, and crate new as above
	// 3. thread is running and queue len is 1, pump up len to 2 and let it run
	// 4. thread is running and queue len is 2, just let it run

	if (!hiscore_thread)
	{
		memset(&(hiscore_data[0]),0,sizeof(HISCORE));
		strcpy_s(hiscore_data[0].buf,65*55,cmd);

		hiscore_queue = 1;
		hiscore_running = 1;
		pthread_create(&hiscore_thread, 0, hiscore_proc, &hiscore_running);
	}
	else
	{
		pthread_mutex_lock(&hiscore_cs);

		if (hiscore_queue == 0)
		{
			// we are bit late
			pthread_mutex_unlock(&hiscore_cs);

			pthread_join(hiscore_thread, 0);
			hiscore_thread=0;

			memset(&(hiscore_data[0]),0,sizeof(HISCORE));
			strcpy_s(hiscore_data[0].buf,65*55,cmd);

			hiscore_queue = 1;
			hiscore_running = 1;
			pthread_create(&hiscore_thread, 0, hiscore_proc, &hiscore_running);
		}
		else
		{
			if (hiscore_queue == 1)
				hiscore_queue = 2;

			memset(&(hiscore_data[1]),0,sizeof(HISCORE));
			strcpy_s(hiscore_data[1].buf,65*55,cmd);

			pthread_mutex_unlock(&hiscore_cs);
		}
	}
}

void get_hiscore(int ofs, const char* id)
{
	if (hiscore_sync)
		return;

	static char path[1024];
	const char* home = getenv("HOME");
	if (!home || !home[0])
		home=".";
	sprintf_s(path,1024,"%s/asciipat.rnk", home);

	char cmd[1024];

	if (id && id[0])
	{
		sprintf_s(cmd,1024,
			"curl --silent -o \"%s\" \"http://ascii-patrol.com/rank.php?rank=%d&id=%s\" 2>/dev/null"
			,path,ofs+1,id);
	}
	else
	{
		sprintf_s(cmd,1024,
			"curl --silent -o \"%s\" \"http://ascii-patrol.com/rank.php?rank=%d\" 2>/dev/null"
			,path,ofs+1);
	}

	request_hiscore(cmd);
}

void post_hiscore()
{
	static char path[1024];
	const char* home = getenv("HOME");
	if (!home || !home[0])
		home=".";
	sprintf_s(path,1024,"%s/asciipat.rnk", home);

	char cmd[1024];
	sprintf_s(cmd,1024,
		"curl --silent -o \"%s\" -F \"rec=@%s\" \"http://ascii-patrol.com/rank.php\" 2>/dev/null"
		,path,
		record_path());

	request_hiscore(cmd);

	hiscore_sync = true; // prevents removal of post by get requests from queue
}

bool update_hiscore()
{
	if (!hiscore_thread)
		return true;

	if (!hiscore_running)
	{
		pthread_join(hiscore_thread,0);
		hiscore_thread=0;

		if (hiscore_ready)
		{
			hiscore_ready = false;

			// prevent overwriting id if no new id is returned
			if (hiscore.id[0]!=0 && hiscore_async.id[0]==0)
				memcpy(hiscore_async.id,hiscore.id,16);

			hiscore = hiscore_async;
		}

		hiscore_sync = false;

		return true;
	}

	pthread_mutex_lock(&hiscore_cs);

	if (hiscore_queue == 0)
	{
		// we are bit late
		pthread_mutex_unlock(&hiscore_cs);

		pthread_join(hiscore_thread,0);

		hiscore_thread=0;

		if (hiscore_ready)
		{
			hiscore_ready = false;
			hiscore = hiscore_async;
		}

		hiscore_sync = false;

		return true;
	}
	else
	{
		if (hiscore_ready)
		{
			hiscore_ready = false;
			hiscore = hiscore_async;
		}

		pthread_mutex_unlock(&hiscore_cs);
	}

	return false;
}

void app_exit()
{
	terminal_done();
	_exit(0);
}

static pa_threaded_mainloop* audio_api = 0;
static pa_context* audio_context = 0;
static pa_stream* audio_stream = 0;

static pthread_mutex_t audio_cs = PTHREAD_MUTEX_INITIALIZER;
static volatile int audio_error = 0;

// silence until conf fetch!
static int mo3_gain = 0; //100;
static int sfx_gain = 0; //100;

struct VOICE
{
	unsigned int id;

	int vol; // global vol at the play time

	int gain;
	int pan;
	bool loop;

	int pos;

	unsigned int fade_start;
	unsigned int fade_len;

	VOICE* next;
};

struct SAMPLE
{
	short* data;
	int size;
};

static SAMPLE sfx_buf[256]={0};
static VOICE* sfx_voice = 0;
static VOICE* sfx_fade = 0;
static pthread_mutex_t sfx_cs = PTHREAD_MUTEX_INITIALIZER;

static bool sfx_mix(VOICE* v, short* buf, int samples, int gain)
{
	int size = sfx_buf[v->id].size;
	short* data = sfx_buf[v->id].data;
	int vol = v->gain*v->vol*gain/10000;

	if (v->loop)
	{
		for (int i=0; i<samples; i++)
		{
			// todo: SATURATE to +/- 32767 !!!
			buf[2*i+0] += vol*data[v->pos]/100;
			buf[2*i+1] += vol*data[v->pos]/100;

			v->pos++;
			if (v->pos>=size)
				v->pos-=size;
		}
		return true;
	}

	for (int i=0; i<samples; i++)
	{
		// todo: SATURATE to +/- 32767 !!!
		buf[2*i+0] += vol*data[v->pos]/100;
		buf[2*i+1] += vol*data[v->pos]/100;

		v->pos++;
		if (v->pos>=size)
			return false;
	}

	return true;
}

static void audio_stream_write_cb(pa_stream *p, size_t nbytes, void *user)
{
	const int nch=2;
	const int frq=44100;
	const int smp=frq/4; // 0.25s buf

	short pull[nch*smp];

	int samples = nbytes / 4;

	pthread_mutex_lock(&sfx_cs);
	int mo3_vol = mo3_gain;

	unsigned int fade = get_time();

	while (samples)
	{
		int s = samples;
		if (s>smp)
			s=smp;

		pull_sound(nch,frq,pull,s);

		// attenuate
		for (int i=0; i<2*s; i++)
			pull[i] = pull[i] * mo3_vol / 100;

		// mix-in sfx!
		VOICE** p = &sfx_voice;
		VOICE* v = sfx_voice;
		while (v)
		{
			VOICE* n = v->next;
			if (sfx_mix(v,pull,s,100))
			{
				p=&v->next;
			}
			else
			{
				*p=n;
				delete v;
			}
			v=n;
		}

		p=&sfx_fade;
		v = sfx_fade;
		while (v)
		{
			VOICE* n = v->next;
			// calc fade val
			unsigned int dt = fade - v->fade_start;
			if (dt>=v->fade_len)
			{
				*p=n;
				delete v;
			}
			else
			{
				int gain = v->gain*v->vol/100 - dt*100 / v->fade_len;
				if (sfx_mix(v,pull,s,gain))
				{
					p=&v->next;
				}
				else
				{
					*p=n;
					delete v;
				}
			}

			v=n;
		}

		// pa_stream_begin_write();
		pa_stream_write(audio_stream, pull, s*nch*sizeof(short), 0/*free_cb*/, 0/*offset*/, PA_SEEK_RELATIVE);

		samples-=s;
	}

	pthread_mutex_unlock(&sfx_cs);
}

static void audio_stream_state_cb(pa_stream *s, void *user)
{
	switch(pa_stream_get_state(audio_stream))
	{
		case PA_STREAM_CREATING:
		{
			printf("Pulse: Stream creating...\n");
			break;
		}

		case PA_STREAM_READY:
		{
			printf("Pulse: Stream ready now\n");

			audio_error=0;
			pthread_mutex_unlock(&audio_cs);
			break;
		}

		case PA_STREAM_UNCONNECTED:
		case PA_STREAM_TERMINATED:
		case PA_STREAM_FAILED:
		{
			printf("Pulse: Stream connection failed\n");

			pa_stream_unref(audio_stream);
			audio_stream = 0;

			audio_error=-6;
			pthread_mutex_unlock(&audio_cs);
			break;
		}
	}
}

static void audio_init_stream()
{
	int error;
	pa_sample_spec ss;
	pa_channel_map map;

	ss.format = PA_SAMPLE_S16LE;
	ss.channels = 2;
	ss.rate = 44100;

	/* init channel map */
	pa_channel_map_init_stereo(&map);

	/* check sample spec */
	if(!pa_sample_spec_valid(&ss))
	{
		audio_error = -2;
		pthread_mutex_unlock(&audio_cs);
		return;
	}

	/* create the stream */
	audio_stream = pa_stream_new(audio_context, "ascii-patrol", &ss, &map);
	if(!audio_stream)
	{
		printf("Pulse: Error creating stream.\n");
		audio_error = -3;
		pthread_mutex_unlock(&audio_cs);
		return;
	}

	pa_stream_set_write_callback(audio_stream, audio_stream_write_cb, 0/*user*/);
	pa_stream_set_state_callback(audio_stream, audio_stream_state_cb, 0/*user*/);

	pa_buffer_attr attr={0};
	attr.fragsize = (uint32_t)-1;
	attr.tlength = 44100*2*sizeof(short) * 20/1000;
	attr.maxlength = attr.tlength;
	attr.minreq = 0;
	attr.prebuf = (uint32_t)-1;

	error = pa_stream_connect_playback(audio_stream, 0, &attr,
			pa_stream_flags(
				PA_STREAM_AUTO_TIMING_UPDATE |
				PA_STREAM_ADJUST_LATENCY |
				PA_STREAM_INTERPOLATE_TIMING |
			0),
			0/*vol*/, 0);

	if(error < 0)
	{
		pa_stream_unref(audio_stream);
		audio_stream = 0;

		printf("Pulse: Failed connecting stream.\n");
		audio_error = -4;
		pthread_mutex_unlock(&audio_cs);
		return;
	}
}

static void audio_context_state_cb(pa_context* c, void* user)
{
	switch (pa_context_get_state(c))
	{
		case PA_CONTEXT_CONNECTING:
		{
			printf("Pulse: Context connecting...\n");
			break;
		}
		case PA_CONTEXT_AUTHORIZING:
		{
			printf("Pulse: Context authorizing...\n");
			break;
		}
		case PA_CONTEXT_SETTING_NAME:
		{
			printf("Pulse: Context setting name...\n");
			break;
		}
		case PA_CONTEXT_READY:
		{
			printf("Pulse: Context ready now.\n");
			audio_init_stream();
			break;
		}
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
		{
			printf("Pulse: Connection failed.\n");
			audio_error = -1;
			pthread_mutex_unlock(&audio_cs);
			break;
		}
	}
}

void init_sound()
{
	audio_api = pa_threaded_mainloop_new();
	if (!audio_api)
		return;

	pa_proplist *plist = pa_proplist_new();

	pa_proplist_sets(plist, PA_PROP_APPLICATION_NAME, "ascii-patrol");
	pa_proplist_sets(plist, PA_PROP_APPLICATION_VERSION, "alpha 2.0");
	pa_proplist_sets(plist, PA_PROP_MEDIA_ROLE, "game");

	audio_context = pa_context_new_with_proplist( pa_threaded_mainloop_get_api(audio_api), "ascii-patrol", plist );

	pa_proplist_free(plist);

	if (!audio_context)
	{
		pa_threaded_mainloop_free(audio_api);
		audio_api = 0;
		return;
	}

	pa_context_set_state_callback(audio_context, audio_context_state_cb, 0/*user*/);

	int error = pa_context_connect(audio_context, NULL, (pa_context_flags_t)0, NULL);

	if(error < 0)
	{
		// no demon, excellent
		// player won't be loaded unnecessarily
		pa_context_unref(audio_context);
		audio_context = 0;
		pa_threaded_mainloop_free(audio_api);
		audio_api = 0;
		return;
	}

	pthread_mutex_lock(&audio_cs);

	// !!!
	load_player();

	audio_error = 1; // pending
	error = pa_threaded_mainloop_start(audio_api);

	if(error < 0)
	{
		pa_context_unref(audio_context);
		audio_context = 0;
		pa_threaded_mainloop_free(audio_api);
		audio_api = 0;

		audio_error = -5;
		pthread_mutex_unlock(&audio_cs);
	}
	else
	{
		// wait here for async error or success
		pthread_mutex_lock(&audio_cs);
		pthread_mutex_unlock(&audio_cs);

		if (audio_error < 0)
		{
			pa_threaded_mainloop_stop(audio_api);

			pa_context_unref(audio_context);
			audio_context = 0;

			pa_threaded_mainloop_free(audio_api);
			audio_api = 0;
		}
		else
		{
			// FULL SUCCESS

			// now, let's try to upload our samples

			unsigned int id[96];
			int count = get_sfx_ids(id);

			for (int i=0; i<count; i++)
			{
				int size=0;
				short* data = get_sfx_sample(id[i],&size);
				sfx_buf[ id[i] ].data = data;
				sfx_buf[ id[i] ].size = size;
			}
		}
	}
}

void free_sound()
{
	if (audio_api)
	{
		pa_context_disconnect(audio_context);
		pa_threaded_mainloop_stop(audio_api);

		pa_context_unref(audio_context);
		audio_context = 0;

		pa_threaded_mainloop_free(audio_api);
		audio_api = 0;

		//pthread_mutex_lock(&sfx_cs);
		stop_sfx(-1);
		sfx_voice = sfx_fade;
		stop_sfx(-1);
		//pthread_mutex_unlock(&sfx_cs);

	}
}

void set_gain(int mo3, int sfx)
{
	// todo:
	// we should use interlocked swaps

	if (mo3>=0)
	{
		mo3_gain = mo3;
	}
	if (sfx>=0)
	{
		sfx_gain = sfx;
	}
}

void lock_player()
{
	pthread_mutex_lock(&audio_cs);
}

void unlock_player()
{
	pthread_mutex_unlock(&audio_cs);
}


bool play_sfx(unsigned int id, void** voice, bool loop, int vol, int pan)
{
	if (!audio_context)
		return false;

	pthread_mutex_lock(&sfx_cs);

	if (sfx_buf[id].data)
	{
		VOICE* v = new VOICE;

		v->id = id; // data & size is @ sfx_buf[v->id]
		v->next = sfx_voice;

		v->vol  = sfx_gain;
		v->gain = vol;
		v->pan  = 0;
		v->loop = loop;
		v->pos  = 0;

		sfx_voice = v;

		if (voice)
			*voice = v;

		pthread_mutex_unlock(&sfx_cs);
		return true;
	}

	pthread_mutex_unlock(&sfx_cs);
	return false;
}

bool stop_sfx(int fade)
{
	pthread_mutex_lock(&sfx_cs);
	if (fade<=0)
	{
		// just kill all
		VOICE* v = sfx_voice;
		while (v)
		{
			VOICE* n = v->next;
			delete v;
			v=n;
		}
		sfx_voice = 0;

		// kill all current fades
		v = sfx_fade;
		while (v)
		{
			VOICE* n = v->next;
			delete v;
			v=n;
		}
		sfx_fade = 0;
	}
	else
	{
		unsigned int fade_begin = get_time();
		// append all to fade list and assign timers

		VOICE* v = sfx_fade;
		unsigned int end = fade_begin + fade;
		while (v)
		{
			unsigned int e = v->fade_start + v->fade_len;
			if ( (int)(e-end) > 0 )
			{
				int delta = (fade*v->fade_len)/(fade_begin - v->fade_start) - fade;
				v->fade_start = fade_begin - delta;
				v->fade_len = fade + delta;
			}

			v=v->next;
		}

		v = sfx_voice;
		while (v)
		{
			VOICE* n = v->next;

			v->fade_start = fade_begin;
			v->fade_len = fade;

			v->next = sfx_fade;
			sfx_fade = v;

			v=n;
		}
		sfx_voice = 0;
	}

	pthread_mutex_unlock(&sfx_cs);
	return true;
}

bool stop_sfx(void* voice, int fade)
{
	pthread_mutex_lock(&sfx_cs);
	if (fade<=0)
	{
		VOICE** p = &sfx_voice;
		VOICE* v = sfx_voice;

		while (v)
		{
			VOICE* n = v->next;

			if (voice == (void*)v)
			{
				*p = n;
				delete v;

				pthread_mutex_unlock(&sfx_cs);
				return true;
			}

			p=&v->next;
			v=n;
		}
	}
	else
	{
		unsigned int fade_begin = get_time();
		// append all to fade list and assign timers

		VOICE** p = &sfx_voice;
		VOICE* v = sfx_voice;

		while (v)
		{
			VOICE* n = v->next;

			if (voice == (void*)v)
			{
				*p = n;
				break;
			}

			p=&v->next;
			v=n;
		}

		if (v)
		{
			v->fade_start = fade_begin;
			v->fade_len = fade;
			v->next = sfx_fade;
			sfx_fade = v;

			pthread_mutex_unlock(&sfx_cs);
			return true;
		}
	}

	pthread_mutex_unlock(&sfx_cs);
	return false;
}

bool set_sfx_params(void* voice, int vol, int pan)
{
	pthread_mutex_lock(&sfx_cs);

	VOICE* v = sfx_voice;

	while (v)
	{
		if (voice == (void*)v)
		{
			v->gain = vol;
			v->pan = pan;

			pthread_mutex_unlock(&sfx_cs);

			return true;
		}

		v=v->next;
	}

	pthread_mutex_unlock(&sfx_cs);
	return false;
}
