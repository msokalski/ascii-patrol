CXX      = g++
CXXFLAGS = -Wno-multichar -O3
LD       = g++
LDFLAGS  = -pthread
LIBS     = -L/usr/X11/lib -lX11 -lXi -lpulse
OBJS     = manual.o mo3.o unmo3.o stb_vorbis.o conf.o gameover.o inter.o \
           twister.o game.o temp.o menu.o assets.o spec_nix.o

.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $*.o $<

all: depend asciipat

asciipat: $(OBJS)
	$(LD) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	$(RM) -f *.o *~ depend

depend: Makefile
	@echo Building dependencies...
	@echo > $@
	@for i in $(OBJS); do \
	    $(CXX) $(CXXFLAGS) -MM $${i%.o}.cpp; \
	done >> $@

sinclude depend
