#include <algorithm>
#include <windows.h>
#include <Shlobj.h>
#include <crtdbg.h>
#include <cstdio>

#include <mmsystem.h>
#include <mmreg.h>

#pragma comment(lib,"winmm.lib")

void DBG(const char* str)
{
	OutputDebugStringA(str);
}

#include "spec.h"
#include "conf.h"
#include "menu.h"

static HANDLE stdin_handle = 0;
static HANDLE stdout_handle = 0;

// #define SIMULATE_STICKY

bool get_input_len( int* r)
{
	if (r)
		*r=0;

	return !!GetNumberOfConsoleInputEvents( stdin_handle, (DWORD*)r );
}

bool spec_read_input( CON_INPUT* ir, int n, int* r)
{
	if (r)
		*r=0;

	INPUT_RECORD win_ir[256];
	int rem = n;
	int i=0;
	while (rem)
	{
		int s = (DWORD)std::min(rem,256);
		int win_r;
		bool ok = !!ReadConsoleInputA(stdin_handle,win_ir,(DWORD)s,(DWORD*)&win_r);

		for (int k=0; k<win_r; k++)
		{
			switch (win_ir[k].EventType)
			{
				case MOUSE_EVENT:
				{
					static bool down = 0;

					bool was_down = down;
					bool is_down = 0 != (win_ir[k].Event.MouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED);

					down = is_down;

					if (was_down && is_down)
						ir[i+k].EventType = CON_INPUT_TCH_MOVE;
					else
					if (was_down && !is_down)
						ir[i+k].EventType = CON_INPUT_TCH_END;
					else
					if (!was_down && is_down)
						ir[i+k].EventType = CON_INPUT_TCH_BEGIN;
					else
					{
						ir[i+k].EventType = CON_INPUT_UNK;
						break; // skip hovering
					}

					ir[i+k].Event.TouchEvent.id = 0;
					ir[i+k].Event.TouchEvent.x = win_ir[k].Event.MouseEvent.dwMousePosition.X;
					ir[i+k].Event.TouchEvent.y = win_ir[k].Event.MouseEvent.dwMousePosition.Y;

					char dbg[64];
					const char* dbg_name[] = {"BEGIN", "MOVE", "END"};
					sprintf_s(dbg,64,"%5s %d,%d\n", dbg_name[ir[i+k].EventType - CON_INPUT_TCH_BEGIN], ir[i+k].Event.TouchEvent.x, ir[i+k].Event.TouchEvent.y);
					OutputDebugStringA(dbg);

					break;
				}

				case KEY_EVENT:
				{
					#ifdef SIMULATE_STICKY
					if (!win_ir[k].Event.KeyEvent.bKeyDown)
					{
						// simulate sticky mode
						ir[i+k].EventType = CON_INPUT_UNK;
						break;
					}
					#endif

					ir[i+k].EventType = CON_INPUT_KBD;
					ir[i+k].Event.KeyEvent.bKeyDown = !!win_ir[k].Event.KeyEvent.bKeyDown;

					char code = win_ir[k].Event.KeyEvent.uChar.AsciiChar;

					if (!code)
					{
						switch (win_ir[k].Event.KeyEvent.wVirtualKeyCode)
						{
							case VK_LEFT:	code=KBD_LT; break;
							case VK_RIGHT:	code=KBD_RT; break;
							case VK_UP:		code=KBD_UP; break;
							case VK_DOWN:	code=KBD_DN; break;

							case VK_DELETE:	code=KBD_DEL; break;
							case VK_INSERT:	code=KBD_INS; break;

							case VK_HOME:
								code=KBD_HOM;
								break;

							case VK_END:
								code=KBD_END;
								break;

							case VK_PRIOR:	code=KBD_PUP; break;
							case VK_NEXT:	code=KBD_PDN; break;
						}
					}

					ir[i+k].Event.KeyEvent.uChar.AsciiChar = code;

					break;
				}

				case FOCUS_EVENT:
					ir[i+k].EventType = CON_INPUT_FOC;
					ir[i+k].Event.FocusEvent.bSetFocus = !!win_ir[k].Event.FocusEvent.bSetFocus;
					break;
				default:
					ir[i+k].EventType = CON_INPUT_UNK;
			}
		}

		if (!ok)
			return false;
		i+=win_r;
		if (r)
			*r+=win_r;
		rem-=win_r;
		if (win_r<s)
			break;
	}

	return true;
}


bool has_key_releases()
{
	#ifdef SIMULATE_STICKY
	// simulate sticky mode
		return false;
	#endif

	return true;
}

void get_terminal_wh(int* dw, int* dh)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(stdout_handle,&csbi);
	*dw = csbi.srWindow.Right - csbi.srWindow.Left +1;
	*dh = csbi.srWindow.Bottom - csbi.srWindow.Top +1;
}

static void SetConsolePalette(HANDLE sb, const void* palette)
{
	HMODULE mod = GetModuleHandleA("kernel32.dll");
	if (!mod)
		return;

	typedef BOOL (WINAPI *PGetConsoleScreenBufferInfoEx)(HANDLE hConsoleOutput, PCONSOLE_SCREEN_BUFFER_INFOEX lpConsoleScreenBufferInfoEx);
	typedef BOOL (WINAPI *PSetConsoleScreenBufferInfoEx)(HANDLE hConsoleOutput, PCONSOLE_SCREEN_BUFFER_INFOEX lpConsoleScreenBufferInfoEx);

	PGetConsoleScreenBufferInfoEx pGetConsoleScreenBufferInfoEx =
		(PGetConsoleScreenBufferInfoEx)GetProcAddress(mod, "GetConsoleScreenBufferInfoEx");

	PSetConsoleScreenBufferInfoEx pSetConsoleScreenBufferInfoEx =
		(PSetConsoleScreenBufferInfoEx)GetProcAddress(mod, "SetConsoleScreenBufferInfoEx");

	if (pGetConsoleScreenBufferInfoEx && pSetConsoleScreenBufferInfoEx)
	{
		CONSOLE_SCREEN_BUFFER_INFOEX nfo={0};
		nfo.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
		BOOL ok = pGetConsoleScreenBufferInfoEx(sb,&nfo);

		memcpy(nfo.ColorTable,palette,sizeof(COLORREF)*16);
		ok = pSetConsoleScreenBufferInfoEx(sb,&nfo);
	}
}

static const unsigned char ansi_pal[16*4] =
{
	  0,  0,  0,  0,
	  0,  0,170,  0,
	  0,170,  0,  0,
	  0,170,170,  0,
	170,  0,  0,  0,
	170,  0,170,  0,
	170, 85,  0,  0,
	170,170,170,  0,
	 85, 85, 85,  0,
	 85, 85,255,  0,
	 85,255, 85,  0,
	 85,255,255,  0,
	255, 85, 85,  0,
	255, 85,255,  0,
	255,255, 85,  0,
	255,255,255,  0
};

int terminal_init(int argc, char* argv[], int* dw, int* dh)
{
	LoadConf();

	timeBeginPeriod(1);
	stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

	// weird things happen to ConEmu :(
	// HANDLE sb = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, 0);
	// SetConsoleActiveScreenBuffer(sb);
	// stdout_handle = sb;

	SetConsolePalette(stdout_handle, ansi_pal);

	SetConsoleOutputCP(437);
	DWORD mode;
	GetConsoleMode(stdout_handle,&mode);
	//if (mode&ENABLE_WRAP_AT_EOL_OUTPUT)
	{
		mode^=ENABLE_WRAP_AT_EOL_OUTPUT;
		SetConsoleMode(stdout_handle,mode);
	}

	CONSOLE_SCREEN_BUFFER_INFO sbi;
	GetConsoleScreenBufferInfo(stdout_handle,&sbi);

	stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
	mode = ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
	BOOL ok = SetConsoleMode(stdin_handle, mode);

	CONSOLE_CURSOR_INFO ci={100,FALSE};
	//SetConsoleCursorInfo(stdout_handle,&ci);

	int cols = std::max(80,sbi.srWindow.Right - sbi.srWindow.Left + 1);
    int rows = std::max(25,sbi.srWindow.Bottom - sbi.srWindow.Top + 1);

	*dw = cols;
	*dh = rows;

	return 0;
}

void terminal_done()
{
	free_sound();

	timeEndPeriod(1);
	_CrtDumpMemoryLeaks();
}

void sleep_ms(int ms)
{
	Sleep(ms);
}

void vsync_wait()
{
	sleep_ms(8);
}

unsigned int get_time()
{
	return timeGetTime();
}

void free_con_output(CON_OUTPUT* screen)
{
	CHAR_INFO* arr = (CHAR_INFO*)screen->arr;
	if (arr)
		delete [] arr;

	screen->arr = 0;
}

extern bool make_screen_shot;

void screen_shot(CON_OUTPUT* s, const char* name=0);

int screen_write(CON_OUTPUT* screen, int dw, int dh, int sx, int sy, int sw, int sh)
{
	if (make_screen_shot)
	{
		screen_shot(screen);
		make_screen_shot = false;
	}

	int w = screen->w;
	int h = screen->h;
	char* buf = screen->buf;
	char* color = screen->color;
	CHAR_INFO* arr = (CHAR_INFO*)screen->arr;

	unsigned char pal_conv[16]=
	{
		0,4,2,6,1,5,3,7,
		8,12,10,14,9,13,11,15
	};

	if (!arr)
	{
		arr = new CHAR_INFO[(w+1)*h];
		screen->arr = (char*)arr;
	}

	for (int y=0,i=0; y<sh; y++)
	{
		for (int x=0; x<sw; x++,i++)
		{
			// arr[i].Char.AsciiChar = buf[(w+1)*(y+sy)+x+sx];
			arr[i].Char.UnicodeChar = buf[(w+1)*(y+sy)+x+sx];

			if (color)
			{
				int fg = pal_conv[ color[(w+1)*(y+sy)+x+sx] & 0xF ];
				int bg = pal_conv[ (color[(w+1)*(y+sy)+x+sx]>>4) & 0xF ];

				unsigned char attr = (unsigned char)(fg | (bg<<4));
				arr[i].Attributes = attr;
			}
			else
				arr[i].Attributes = BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
		}
	}

	COORD size = { static_cast<short int>(sw), static_cast<short int>(sh) };
	COORD offs = { 0,0 };

	CONSOLE_SCREEN_BUFFER_INFO sbi;
	GetConsoleScreenBufferInfo(stdout_handle,&sbi);
    int dx = sbi.srWindow.Left;
    int dy = sbi.srWindow.Top;

	SMALL_RECT rgn = {static_cast<short int>(dx), static_cast<short int>(dy),
	static_cast<short int>(dx+sw-1), static_cast<short int>(dy+sh-1)};

	//WriteConsoleOutputA(stdout_handle,arr,size,offs,&rgn);
	WriteConsoleOutputW(stdout_handle,arr,size,offs,&rgn);

	return sw*sh;
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
	static char folder[MAX_PATH+16];
	SHGetFolderPathA(0,CSIDL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,folder);
	strcat_s(folder,MAX_PATH+16,"\\asciipat.cfg");
	return folder;
}

const char* record_path()
{
	static char folder[MAX_PATH+16];
	SHGetFolderPathA(0,CSIDL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,folder);
	strcat_s(folder,MAX_PATH+16,"\\asciipat.rec");
	return folder;
}

const char* shot_path()
{
	static char folder[MAX_PATH+16];
	SHGetFolderPathA(0,CSIDL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,folder);
	strcat_s(folder,MAX_PATH+16,"\\");
	return folder;
}

static CRITICAL_SECTION hiscore_cs;
static HANDLE hiscore_thread = 0;
static HISCORE hiscore_data[2]={{0,0,0,"",""},{0,0,0,"",""}};
static int hiscore_queue=0;

static HISCORE hiscore_async = {0,0,0,"",""};
static bool hiscore_ready = false;

DWORD WINAPI hiscore_proc(LPVOID p)
{
	int err = 0;
	bool end = false;

	while (!end)
	{
		_flushall();

		STARTUPINFOA         siStartupInfo;
		PROCESS_INFORMATION piProcessInfo;

		memset(&siStartupInfo, 0, sizeof(siStartupInfo));
		memset(&piProcessInfo, 0, sizeof(piProcessInfo));

		siStartupInfo.cb = sizeof(siStartupInfo);

		if(!CreateProcessA(	NULL,
							hiscore_data->buf,
							NULL,
							NULL,
							FALSE,

							// stop removing mouse feedback from my console!
							DETACHED_PROCESS,

							NULL,
							NULL,
							&siStartupInfo,
							&piProcessInfo))
		{
		  // Could not start application
			err = -1;
		}
		else
		{
			// Wait until application has terminated
			WaitForSingleObject(piProcessInfo.hProcess, INFINITE);
			CloseHandle(piProcessInfo.hThread);
			CloseHandle(piProcessInfo.hProcess);
			err = 0;
		}

		// check if we need to discard this data and re-fetch
		EnterCriticalSection(&hiscore_cs);

		HISCORE tmp={0,0,0,"",""};

		if (!err)
		{
			static char folder[MAX_PATH+16];
			SHGetFolderPathA(0,CSIDL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,folder);
			strcat_s(folder,MAX_PATH+16,"\\asciipat.rnk");

			FILE* f = 0;
			fopen_s(&f,folder,"rb");
			if (f)
			{
				char line[56];
				if (fgets(line,56,f))
				{
					if ( sscanf_s(line,"%d/%d",&tmp.ofs,&tmp.tot) != 2 )
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

		LeaveCriticalSection(&hiscore_cs);

		if (end)
			break;

		Sleep(250); // save OS from being flooded by pg-up/dn key auto-repeat
	}

	return err;
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

		InitializeCriticalSection(&hiscore_cs);
		hiscore_queue = 1;
		DWORD id=0;
		hiscore_thread = CreateThread(0,0,hiscore_proc, 0,0,&id);
	}
	else
	{
		EnterCriticalSection(&hiscore_cs);

		if (hiscore_queue == 0)
		{
			// we are bit late
			LeaveCriticalSection(&hiscore_cs);

			WaitForSingleObject(hiscore_thread,INFINITE);
			CloseHandle(hiscore_thread);
			hiscore_thread=0;

			memset(&(hiscore_data[0]),0,sizeof(HISCORE));
			strcpy_s(hiscore_data[0].buf,65*55,cmd);

			hiscore_queue = 1;
			DWORD id=0;
			hiscore_thread = CreateThread(0,0,hiscore_proc, 0,0,&id);
		}
		else
		{
			if (hiscore_queue == 1)
				hiscore_queue = 2;

			memset(&(hiscore_data[1]),0,sizeof(HISCORE));
			strcpy_s(hiscore_data[1].buf,65*55,cmd);

			LeaveCriticalSection(&hiscore_cs);
		}
	}
}

void get_hiscore(int ofs, const char* id)
{
	if (hiscore_sync)
		return;

	static char folder[MAX_PATH+16];
	SHGetFolderPathA(0,CSIDL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,folder);
	strcat_s(folder,MAX_PATH+16,"\\asciipat.rnk");

	char cmd[1024];

	if (id && id[0])
	{
		sprintf_s(cmd,1024,
			"curl --silent -o \"%s\" \"http://ascii-patrol.com/rank.php?rank=%d&id=%s\""
			,folder,ofs+1,id);
	}
	else
	{
		sprintf_s(cmd,1024,
			"curl --silent -o \"%s\" \"http://ascii-patrol.com/rank.php?rank=%d\""
			,folder,ofs+1);
	}

	request_hiscore(cmd);
}

void post_hiscore()
{
	static char folder[MAX_PATH+16];
	SHGetFolderPathA(0,CSIDL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,folder);
	strcat_s(folder,MAX_PATH+16,"\\asciipat.rnk");

	char cmd[1024];
	sprintf_s(cmd,1024,
		"curl --silent -o \"%s\" -F \"rec=@%s\" \"http://ascii-patrol.com/rank.php\" 2>nul"
		,folder,
		record_path());

	request_hiscore(cmd);

	hiscore_sync = true; // prevents removal of post by get requests from queue
}

bool update_hiscore()
{
	if (!hiscore_thread)
		return true;

	if (WaitForSingleObject(hiscore_thread,0) == WAIT_OBJECT_0)
	{
		CloseHandle(hiscore_thread);

		DeleteCriticalSection(&hiscore_cs);
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

	EnterCriticalSection(&hiscore_cs);

	if (hiscore_queue == 0)
	{
		// we are bit late
		LeaveCriticalSection(&hiscore_cs);

		WaitForSingleObject(hiscore_thread,INFINITE);
		CloseHandle(hiscore_thread);

		DeleteCriticalSection(&hiscore_cs);
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

		LeaveCriticalSection(&hiscore_cs);
	}

	return false;
}

void app_exit()
{
	terminal_done();
	_exit(0);
}
