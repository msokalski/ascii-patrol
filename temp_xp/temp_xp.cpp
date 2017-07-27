
#include <Windows.h>
#include <stdio.h>

void* (*xp_open)(const char *path, const char *mode);
int (*xp_read)(void* file, void* buf, unsigned len);
int (*xp_close)(void* file);

#define IMP(p,n) *(void**)&(p) = GetProcAddress(h,#n)

#define ABS(a) ((a)<0 ? -(a):(a))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#pragma pack(push,1)

struct Hdr
{
	int ver;
	int layers;
	int width;
	int height;
};

union Cel
{
	struct
	{
		int code;
		unsigned char fg[3];
		unsigned char bg[3];
	};

	struct
	{
		char ascii;
		unsigned char color;
	};
};

#pragma pack(pop)

int Raw2XP(const char* raw_path, const char* xp_path, int w, int h, int n)
{
	FILE* f_raw=0;
	fopen_s(&f_raw,raw_path,"rt");
	if (!f_raw)
		return -1;

	unsigned char* img = (unsigned char*)malloc(w*h*n);
	fread(img,w*n,h,f_raw);

	fclose(f_raw);

	FILE* f_xp=0;
	fopen_s(&f_xp,xp_path,"wb");
	if (!f_xp)
	{
		fclose(f_raw);
		return -2;
	}

	Hdr hdr=
	{
		-1,
		1,
		w,
		h
	};

	fwrite(&hdr,sizeof(Hdr),1,f_xp);

	for (int x=0; x<w; x++)
	{
		for (int y=0; y<h; y++)
		{
			Cel c;
			/*
			c.code = grey[w*y+x]>=128 ? ' ' : '.';
			c.bg[0] = c.bg[1] = c.bg[2] = 255;
			c.fg[0] = c.fg[1] = c.fg[2] = 0;
			*/

			c.code = '.';
			c.bg[0] = c.bg[1] = c.bg[2] = 0;

			if (n==1)
				c.fg[0] = c.fg[1] = c.fg[2] = img[n*(x+w*y)+0];
			else
			if (n>=3)
			{
				c.fg[0] = img[n*(x+w*y)+0];
				c.fg[1] = img[n*(x+w*y)+1];
				c.fg[2] = img[n*(x+w*y)+2];
			}

			fwrite(&c,sizeof(Cel),1,f_xp);
		}
	}

	free(img);

	fclose(f_xp);
	return 0;
}

//int test = Raw2XP("C:\\Users\\msokalski\\Desktop\\campaign.raw","C:\\Users\\msokalski\\Desktop\\campaign.xp", 393,117,3);


int MakeXP(const char* txt_path, const char* xp_path)
{
	FILE* f_txt=0;
	fopen_s(&f_txt,txt_path,"rt");
	if (!f_txt)
		return -1;

	char buf[1024];
	int width = 0;
	int height = 0;
	int len = 0;
	while (fgets(buf,1024,f_txt))
	{
		char* eol = strchr(buf,'\n');
		height++;
		if (!eol)
		{
			width = MAX(width, (int)strlen(buf));
			// last line or line too long
			// assume eof
			break;
		}
		width = MAX(width, (int)(eol-buf));
	}

	fseek(f_txt,0,SEEK_SET);


	FILE* f_xp=0;
	fopen_s(&f_xp,xp_path,"wb");
	if (!f_xp)
	{
		fclose(f_txt);
		return -2;
	}

	Hdr hdr=
	{
		-1,
		1,
		width,
		height
	};

	fwrite(&hdr,sizeof(Hdr),1,f_xp);

	int cells = width*height;
	Cel* image = (Cel*)malloc(cells*sizeof(Cel));
	for (int i=0; i<cells; i++)
	{
		image[i].code=' ';
		image[i].fg[0]=0;
		image[i].fg[1]=0;
		image[i].fg[2]=0;
		image[i].bg[0]=255;
		image[i].bg[1]=255;
		image[i].bg[2]=255;
	}

	int y=0;
	while (fgets(buf,1024,f_txt))
	{
		char* eol = strchr(buf,'\n');
		int w = eol ? (int)(eol-buf) : (int)strlen(buf);
		for (int x=0; x<w; x++)
		{
			int i = x*height + y;
			image[i].code=buf[x];
		}

		y++;

		if (!eol)
			break;
	}

	fwrite(image,sizeof(Cel)*width*height,1,f_xp);
	
	free(image);

	fclose(f_xp);
	fclose(f_txt);

	return 0;
}


unsigned char ansi_pal[17*3] =
{
	  0,  0,  0,
	170,  0,  0,
	  0,170,  0,
	170, 85,  0,
	  0,  0,170,
	170,  0,170,
	  0,170,170,
	170,170,170,
	 85, 85, 85,
	255, 85, 85,
	 85,255, 85,
	255,255, 85,
	 85, 85,255,
	255, 85,255,
	 85,255,255,
	255,255,255,

	255,0,255 // pseudo color!
};

int Palettize(const unsigned char* clr, const char** wrn_path)
{
	int e=1000;
	int i=-1;
	for (int p=0; p<16; p++)
	{
		int pe = 0;
		for (int c=0; c<3; c++)
		{
			int d = clr[c]-ansi_pal[3*p+c];
			if (d<0)
				d=-d;
			pe += d;
		}

		if (pe<e)
		{
			e=pe;
			i=p;
		}
	}

	if (e>10 && wrn_path && *wrn_path)
	{
		printf("warning: non ansi palette color in %s!\n", *wrn_path);
		*wrn_path = 0;
	}
	return i;
}

bool Export(const char* xp_path, FILE* cpp_file, FILE* h_file)
{
	const char* name1 = strrchr(xp_path,'/');
	const char* name2 = strrchr(xp_path,'\\');
	const char* name = 0;

	if (name1-name2 > 0)
		name = name1+1;
	else
	if (name2-name1 > 0)
		name = name2+1;
	else
		name = xp_path;

	int len = (int)strlen(name);
	if (len>=256)
	{
		printf("%s\nasset name too long, skipping!\n",name);
		return true;
	}

	char asset_name[256];
	strcpy_s(asset_name,256,name);

	if (len>3 && _stricmp(asset_name + len-3,".xp")==0)
		asset_name[len-3]=0;

	for (int i=0; asset_name[i]; i++)
	{
		if (asset_name[i]>='a' && asset_name[i]<='z' ||
			asset_name[i]>='A' && asset_name[i]<='Z' ||
			asset_name[i]=='_' || asset_name[i]=='$' ||
			i>0 && asset_name[i]>='0' && asset_name[i]<='9')
		{
		}
		else
		{
			asset_name[i]='_';
		}
	}


	void* xp = xp_open(xp_path,"rb");
	if (!xp)
		return false;
	
	Hdr hdr;
	Cel* img; // first layer only!
	int cells;
	
	int size = xp_read(xp,&hdr,sizeof(Hdr));
	if (size<sizeof(Hdr))
	{
		xp_close(xp);
		return false;
	}

	if (hdr.ver != -1 ||
		hdr.width>1024 ||
		hdr.height>1024 ||
		hdr.layers<1 || hdr.layers>4)
	{
		xp_close(xp);
		return false;
	}

	cells = hdr.width*hdr.height;
	img = new Cel[cells];
	size = xp_read(xp,img,sizeof(Cel)*cells);

	if (size < (int)sizeof(Cel)*cells)
	{
		delete [] img;
		xp_close(xp);
		return false;
	}
	
	xp_close(xp);

	// parse asset info from first row 

	char* info = new char[hdr.width+1];
	for (int x=0; x<hdr.width; x++)
	{
		info[x] = (char)img[x*hdr.height].code;

		for (int y=0; y<hdr.height; y++)
		{
			int fg = Palettize(img[x*hdr.height+y].fg, &xp_path);
			int bg = Palettize(img[x*hdr.height+y].bg, &xp_path);

			if (fg == 16)
			{
				fg = 0;
			}

			if (bg == 16)
			{
				// transparent
				bg = 0;
			}

			img[x*hdr.height+y].color = (unsigned char)(fg | (bg<<4));
		}
	}
	info[hdr.width]=0;

	// TODO: optimize color array to minimize number of color changes
	for (int y=0; y<hdr.height; y++)
	{
		int a=0;
		for (int x=1; x<hdr.width; x++)
		{
			int cur=x*hdr.height+y;
			int prv=(x-1)*hdr.height+y;

			if ((img[cur].color & 0x0F) == ((img[cur].color >> 4) & 0x0F))
				img[cur].ascii = ' ';

			unsigned char c = img[cur].ascii;

			if (c<32 || c>=128)
			{
				// transp char
				img[cur].color = img[prv].color;
			}
			else
			if (c==32)
			{
				// space
				img[cur].color = (img[cur].color & 0xF0) | (img[prv].color & 0x0F);
			}
		}
	}
	// if transparent char: set its full attr to previous char
	// if space char: set its fg color to prev char fg color
	// if fg == bg: set char to space, set its fg color to prev char fg color


	// info formats:
	// 5 frames in color over mono
	// -5 text mono only vertical span

	// integer: color frames over mono frames
	// 

	int frames=1;
	int width=hdr.width;
	int height=hdr.height/2;
	int color_y = 0;
	int mono_y = hdr.height-height;
	int frame_dx=0;
	int frame_dy=0;

	int f=0;
	if (sscanf_s(info,"%d",&f) && f>0)
	{
		frames = f;
		height = (hdr.height-1)/2;
		color_y = 1;
		mono_y = hdr.height-height; // optional vertical space
		width = (hdr.width - 2*(frames-1))/frames; // frames MUST be separated with 2 columns
		frame_dx=width+2;
	}
	else
	if (f<0)
	{
		// in this mode we need to place \n after each line!
		frames = -f;
		height = (hdr.height-1 - (frames-1))/frames; // frames MUST be separated with 1 row
		color_y = -1;
		mono_y = 1;
		frame_dy=height+1;
	}

	delete [] info;

	// ready to export

	fprintf(cpp_file,"const char* %s_mono[]=\n{\n", asset_name);

	// MONO
	for (int i=0; i<frames; i++)
	{
		for (int y=0; y<height; y++)
		{
			char row[2050];
			int len=0;

			int ofs = (i*frame_dx)*hdr.height+y+mono_y+i*frame_dy;
			for (int x=0; x<width; x++,ofs+=hdr.height)
			{
				char c = img[ofs].ascii;
				if (c<32 || c>=128)
					c='*';
				if (c=='\\' || c=='\'' || c=='\"')
					row[len++] = '\\';
				row[len++] = c;
			}
			row[len]=0;
			fprintf(cpp_file,"\t\"%s%s\n",row,y<height-1?"\\n\"":"\",");
		}
	}
	fprintf(cpp_file,"\t0\n};\n\n");

	if (frame_dx>0) // otherwise it is mono tutorial
	{
		// SHADE
		fprintf(cpp_file,"const char* %s_shade[]=\n{\n", asset_name);
		for (int i=0; i<frames; i++)
		{
			for (int y=0; y<height; y++)
			{
				char row[2050];
				int len=0;

				int ofs = (i*frame_dx)*hdr.height+y+color_y+i*frame_dy;
				for (int x=0; x<width; x++,ofs+=hdr.height)
				{
					char c = img[ofs].ascii;
					if (c<32 || c>=128)
						c='*';
					if (c=='\\' || c=='\'' || c=='\"')
						row[len++] = '\\';
					row[len++] = c;
				}
				row[len]=0;
				fprintf(cpp_file,"\t\"%s%s\n",row,y<height-1?"\\n\"":"\",");
			}
		}
		fprintf(cpp_file,"\t0\n};\n\n");

		// colors for all frames are in single array
		fprintf(cpp_file,"const unsigned char %s_colordata[]=\n{\n", asset_name);
		for (int i=0; i<frames; i++)
		{
			for (int y=0; y<height; y++)
			{
				char row[5200];
				int len=0;

				int ofs = (i*frame_dx)*hdr.height+y+color_y+i*frame_dy;
				for (int x=0; x<width; x++,ofs+=hdr.height)
				{
					char c = img[ofs].color;
					char ch = (c>>4)&0xF;
					char cl = c&0xF;

					row[len++] = '0';
					row[len++] = 'x';
					row[len++] = ch<10 ? '0' + ch : 'A' + ch-10;
					row[len++] = cl<10 ? '0' + cl : 'A' + cl-10;
					row[len++] = ',';
				}

				// dummy \n or \0
				row[len++] = '0';
				row[len++] = 'x';
				row[len++] = '0';
				row[len++] = '0';
				row[len++] = ',';

				row[len]=0;
				fprintf(cpp_file,"\t%s\n",row);
			}
		}
		fprintf(cpp_file,"};\n\n");

		fprintf(cpp_file,"const char* %s_color[]=\n{\n", asset_name);
		for (int i=0; i<frames; i++)
		{
			// offsets into frames array
			fprintf(cpp_file,"\t(const char*)%s_colordata + %d,\n", asset_name, (width+1)*height*i);
		}
		fprintf(cpp_file,"\t0\n};\n\n");
	}


	fprintf(h_file,"extern ASSET %s;\n", asset_name);
	fprintf(cpp_file,"ASSET %s=\n", asset_name);
	fprintf(cpp_file,"{\n");
	fprintf(cpp_file,"\t%s_mono,\n", asset_name);

	if (frame_dx>0)
	{
		fprintf(cpp_file,"\t%s_shade,\n", asset_name);
		fprintf(cpp_file,"\t%s_color\n", asset_name);
	}
	else
	{
		fprintf(cpp_file,"\t%s_mono,\n", asset_name);
		fprintf(cpp_file,"\t0\n");
	}

	fprintf(cpp_file,"};\n\n");

	delete [] img;

	return true;
}

const char* cpp_header = "#include \"%s\"\n\n";
const char* h_header = 
	"#ifndef %s\n"
	"#define %s\n\n"
	"struct ASSET\n"
	"{\n"
	"\tconst char** mono;\n"
	"\tconst char** shade;\n"
	"\tconst char** color;\n"
	"};\n\n";

const char* cpp_footer = "";
const char* h_footer = "\n#endif\n\n";


int main(int argc, char* argv[])
{
	// USAGE:
	// ======
	
	// convert XP assets to cpp/h source files
	// ---------------------------------------
	//	temp_xp.exe "c:\xp_folder" "d:\body.cpp" "e:\header.h"
	//	temp_xp.exe
	// (assumes defaults: "." ".\assets.cpp" and ".\assets.h")
	//	temp_xp.exe "c:\xp_folder"
	// (assumes .cpp and .h file to be ".\assets.cpp" and ".\assets.h")
	//	temp_xp.exe "c:\xp_folder" "d:\custom.cpp"
	// (assumes .h file to be "d:\custom.h")

	// convert TXT file to XP (input file extension MUST be txt!)
	// ----------------------
	//	temp_xp.exe "c:\something.txt" "d:\else.xp"
	//	temp_xp.exe "c:\something.txt"
	// (assumes output file "c:\something.xp")


	if (argc>1)
	{
		// test if it is txt file
		int len=strlen(argv[1]);
		if (len>4 && len<1024 && _stricmp(argv[1]+len-4,".txt")==0)
		{
			char xp_path[1024];
			if (argc>2)
			{
				strcpy_s(xp_path,1024,argv[2]);
			}
			else
			{
				strcpy_s(xp_path,1024,argv[1]);
				strcpy_s(xp_path+len-4,1024-len+4,".xp");
			}

			return MakeXP(argv[1],xp_path);
		}
	}

	HMODULE h = (HMODULE)LoadLibrary(L"zlib1.dll");
	if (!h)
	{
		printf("cannot load zlib1.dll\n");
		return -1;
	}

	IMP(xp_open,gzopen);
	IMP(xp_read,gzread);
	IMP(xp_close,gzclose);

	if (!xp_open || !xp_read || !xp_close)
	{
		printf("cannot find gzip handlers in zlib1.dll\n");
		FreeLibrary(h);
		return -1;
	}

	char dir_path[1024]  = ".\\*.xp";
	const char* cpp_path = "assets.cpp";
	const char* h_path   = "assets.h";

	if (argc>1)
	{
		sprintf_s(dir_path,1024,"%s\\*.xp",argv[1]);
	}
	if (argc>2)
		cpp_path = argv[2];
	if (argc>3)
		h_path = argv[3];

	WIN32_FIND_DATAA wfd;
	HANDLE hf = FindFirstFileA(dir_path,&wfd);
	if (!hf || hf==INVALID_HANDLE_VALUE)
	{
		printf("invalid xp images directory!\n");
		FreeLibrary(h);
		return -1;
	}

	FILE* h_file=0;
	FILE* cpp_file=0;

	fopen_s(&cpp_file,cpp_path,"wt");
	if (!cpp_file)
	{
		printf("cannot open %s for writing!\n",cpp_path);
		FindClose(hf);
		FreeLibrary(h);
		return -1;
	}

	fopen_s(&h_file,h_path,"wt");
	if (!h_file)
	{
		printf("cannot open %s for writing!\n",h_path);
		fclose(cpp_file);
		FindClose(hf);
		FreeLibrary(h);
		return -1;
	}

	const char* hdr_name1 = strrchr(h_path,'/');
	const char* hdr_name2 = strrchr(h_path,'\\');
	const char* hdr_name = 0;

	if (hdr_name1-hdr_name2 > 0)
		hdr_name = hdr_name1+1;
	else
	if (hdr_name2-hdr_name1 > 0)
		hdr_name = hdr_name2+1;
	else
		hdr_name = h_path;

	// write headers
	fprintf(cpp_file,cpp_header,hdr_name);

	char once[32];
	sprintf_s(once,32,"%s",hdr_name);
	for (int i=0; once[i]; i++)
	{
		if (once[i]>='a' && once[i]<='z' ||
			once[i]>='A' && once[i]<='Z' ||
			once[i]=='_' || once[i]=='$' ||
			i>0 && once[i]>='0' && once[i]<='9')
		{
		}
		else
		{
			once[i]='_';
		}
	}

	_strupr_s(once,32);

	fprintf(h_file,h_header,once,once);

	while(1)
	{
		if (!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			sprintf_s(dir_path,1024,"%s\\%s", argc>1 ? argv[1] : ".", wfd.cFileName);

			if (!Export(dir_path,cpp_file,h_file))
				printf("warning: cannot read %s\n",wfd.cFileName);
		}

		if (!FindNextFileA(hf,&wfd))
			break;
	}

	// write footers
	fprintf(h_file,"%s",h_footer);
	fprintf(cpp_file,"%s",cpp_footer);

	FindClose(hf);

	fclose(cpp_file);
	fclose(h_file);

	FreeLibrary(h);

	return 0;
}
