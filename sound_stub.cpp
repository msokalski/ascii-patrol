void free_sound(){ return; }
void init_sound(){ return; }
void lock_player(){ return; }
void unlock_player(){ return; }

bool play_sfx(unsigned int id, void** voice, bool loop, int vol, int pan){ return false;}

void set_gain(int mo3, int sfx){ return; }

bool stop_sfx(int fade){ return false; }

bool stop_sfx(void* voice, int fade){ return false; }
