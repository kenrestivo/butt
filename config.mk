# butts's config.mk
#_______________________
DIST_VER=butt-0.1.12
VERSION=\"butt-0.1.12\"
prefix=/usr/local
CXX=g++
CXXFLAGS=
LDFLAGS=
LIBS=
#_______________________

#_______________________
LIBS+= -lportaudio

CXXFLAGS+=
LDFLAGS+=
#_______________________
WITH_FLTK:=1
CXXFLAGS_FLTK:= -I/usr/include/cairo -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/pixman-1 -I/usr/include/freetype2 -I/usr/include/libpng12   -I/usr/include/freetype2 -I/usr/include/cairo -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/pixman-1 -I/usr/include/freetype2 -I/usr/include/libpng12    -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_THREAD_SAFE -D_REENTRANT -DFLTK1
LIBS_FLTK:=/usr/lib/x86_64-linux-gnu/libfltk_images.a -lpng -lz -ljpeg /usr/lib/x86_64-linux-gnu/libfltk.a -lXext -lXft -lfontconfig -lfontconfig -lXinerama -ldl -lm -lX11
SRC_FLTK:=FLTK/fl_callbacks.cpp FLTK/fl_funcs.cpp FLTK/flgui.cpp FLTK/Fl_ILM216.cpp FLTK/Fl_Native_File_Chooser.cpp

#_______________________
WITH_LIBVORBIS:=1
CXXFLAGS_LIBVORBIS:=
CXXFLAGS+=
LDFLAGS+=
LIBS_LIBVORBIS:=-lvorbisenc
SRC_LIBVORBIS:=vorbis_encode.cpp

#_______________________
WITH_LIBLAME:=1
CXXFLAGS_LIBLAME:=
CXXFLAGS+=
LDFLAGS+=
LIBS_LIBLAME:=-lmp3lame
SRC_LIBLAME:=lame_encode.cpp

LDFLAGS += -lmp3lame
#_______________________

