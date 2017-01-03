#include <stdio.h>

#include "conf.h"
#include "spec.h"
#include "menu.h"

// open file?
// how does it work in browsers?

ConfCampaign conf_campaign=
{
	0,0,0
};

ConfKeyboard conf_keyboard=
{
	{'A','D','W','S','L','Q'}
};

ConfPlayer conf_player=
{
	"Player 1",
	0x00000000
};

ConfControl conf_control=
{
	16,
	16,
	0,
	1
};

struct ConfFile1
{
	struct Campaign
	{
		int course;
		int level;
	} conf_campaign;
	ConfKeyboard conf_keyboard;
	ConfPlayer conf_player;
};

struct ConfFile2
{
	ConfCampaign conf_campaign;
	ConfKeyboard conf_keyboard;
	ConfPlayer conf_player;
};

struct ConfFile3
{
	ConfCampaign conf_campaign;
	ConfKeyboard conf_keyboard;
	ConfPlayer conf_player;
	ConfControl conf_control;
};

void LoadConf()
{
	FILE* f=0;
	fopen_s(&f,conf_path(),"rb");
	if (!f)
		return;

	int version=0;
	int ret = fread(&version,4,1,f);
	if (ret!=1)
	{
		fclose(f);
		return;
	}

	if (version==1)
	{
		ConfFile1 conf;
		int ret = fread(&conf,sizeof(ConfFile1),1,f);
		fclose(f);

		if (ret != 1)
			return;

		conf_campaign.passed = 0;
		conf_campaign.course = 0;
		conf_campaign.level = 0;
		conf_keyboard = conf.conf_keyboard;
		conf_player   = conf.conf_player;

		conf_control.mus_vol=16;
		conf_control.sfx_vol=16;
		conf_control.sticky=0;
		conf_control.color=1;
	}

	if (version==2)
	{
		ConfFile2 conf;
		int ret = fread(&conf,sizeof(ConfFile2),1,f);
		fclose(f);

		if (ret != 1)
			return;

		conf_campaign = conf.conf_campaign;
		conf_keyboard = conf.conf_keyboard;
		conf_player   = conf.conf_player;

		conf_control.mus_vol=16;
		conf_control.sfx_vol=16;
		conf_control.sticky=0;
		conf_control.color=1;
	}

	if (version==3)
	{
		ConfFile3 conf;
		int ret = fread(&conf,sizeof(ConfFile3),1,f);
		fclose(f);

		if (ret != 1)
			return;

		conf_campaign = conf.conf_campaign;
		conf_keyboard = conf.conf_keyboard;
		conf_player   = conf.conf_player;
		conf_control  = conf.conf_control;
	}

	LoadMenu();
}

void SaveConf()
{
	FILE* f=0;
	fopen_s(&f,conf_path(),"wb");

	if (!f)
		return;

	int version = 3;
	fwrite(&version,4,1,f);

	ConfFile3 conf;
	conf.conf_campaign = conf_campaign;
	conf.conf_keyboard = conf_keyboard;
	conf.conf_player = conf_player;
	conf.conf_control = conf_control;

	fwrite(&conf,sizeof(ConfFile3),1,f);
	fclose(f);

	write_fs();
}

char ConfMapInput(char c)
{
	static const char m[6]=
	{
		KBD_LT,KBD_RT,KBD_UP,KBD_DN,13,27
	};

	while (1)
	{
		for (int i=0; i<6; i++)
		{
			if (conf_keyboard.map[i] == c)
				return m[i];
		}

		if (c>='a' && c<='z')
			c+='A'-'a';
		else
			break;
	}

	return c;
}
