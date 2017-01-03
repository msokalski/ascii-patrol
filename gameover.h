#ifndef GAMEOVER_H
#define GAMEOVER_H

#include "game.h"

void inter_print(SCREEN* s, int dx, int dy, const char* str, char color);

struct GAMEOVER_MODAL : MODAL
{
	SCREEN* s;

	int w,h;

	int* score;
	const LEVEL* level;

	unsigned long bt;
	int frame;

	int tx_fill;
	unsigned char* tx_mask;
	int* tx_order;

	ASSET  bk2_asset;
	SPRITE bk2;

	SPRITE bk;
	SPRITE fg;
	SPRITE tx;

	SPRITE fl1;
	SPRITE fl2;
	SPRITE fl3;

	SPRITE fl1_mask;
	SPRITE fl2_mask;
	SPRITE fl3_mask;

	FNT fnt;

	SPRITE prompt;

	void PaintFlame(SPRITE* fl, SPRITE* msk, int dx, int dy, int frm);


	virtual ~GAMEOVER_MODAL();
	GAMEOVER_MODAL(SCREEN* _s, int* _score, const LEVEL* _level);

	virtual int Run();
};

#endif