#ifndef TWISTER_H
#define TWISTER_H

void twister_switch(int i);

void crypt_ini(); 
void crypt_enc(unsigned char* b);
void crypt_enc(int* i);
void crypt_dec(unsigned char* b);
void crypt_dec(int* i);
unsigned int crypt_chk();

void twister_seed(unsigned int s);
int  twister_rand();
int  twister_write(FILE* f);
int  twister_read(FILE* f);


/*
#define twister_seed(s) srand(s)
#define twister_rand() rand()
*/

#endif

