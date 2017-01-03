#ifndef MENU_H
#define MENU_H

void InitMenu();
void FreeMenu();
void LoadMenu();

int RunMenu(CON_OUTPUT* screen);

extern HISCORE hiscore;
extern bool hiscore_sync;

#endif
