#ifndef CONF_H
#define CONF_H

struct ConfCampaign
{
	int passed;
	int course;
	int level;
};

struct ConfKeyboard
{
	char map[6];
};

struct ConfPlayer
{
	char name[16];
	unsigned int avatar;
};

struct ConfControl
{
	int mus_vol;
	int sfx_vol;
	int sticky;
	int color;
};

extern ConfCampaign conf_campaign;
extern ConfKeyboard conf_keyboard;
extern ConfPlayer   conf_player;
extern ConfControl  conf_control;

extern "C"
{
	void LoadConf();
	void SaveConf();
}

char ConfMapInput(char c);

#endif
