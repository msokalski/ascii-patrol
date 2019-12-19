#!/bin/sh
g++ \
	-pthread \
	-Wno-multichar \
	-O3 \
	-D NIX \
	manual.cpp \
	mo3.cpp \
	unmo3.cpp \
	stb_vorbis.cpp \
	conf.cpp \
	gameover.cpp \
	inter.cpp \
	twister.cpp \
	game.cpp \
	temp.cpp \
	menu.cpp \
	assets.cpp \
	spec_dos.cpp \
	spec_win.cpp \
	spec_nix.cpp \
	spec_web.cpp \
	-L/usr/X11/lib \
	-lX11 \
	-lXi \
	-lpulse \
	-o asciipat

