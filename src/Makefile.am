# what flags you want to pass to the C compiler & linker

AM_CPPFLAGS = -IFLTK


#
# # this lists the binaries to produce, the (non-PHONY, binary) targets in
# # the previous manual Makefile
bin_PROGRAMS = butt 
butt_SOURCES = butt.cpp butt.h cfg.cpp cfg.h icecast.cpp icecast.h lame_encode.cpp \
			   lame_encode.h parseconfig.cpp parseconfig.h port_audio.cpp \
			   port_audio.h ringbuffer.cpp ringbuffer.h shoutcast.cpp shoutcast.h \
			   sockfuncs.cpp sockfuncs.h strfuncs.cpp strfuncs.h timer.cpp timer.h \
			   util.cpp util.h vorbis_encode.cpp vorbis_encode.h vu_meter.cpp vu_meter.h \
			   wav_header.cpp wav_header.h opus_encode.cpp opus_encode.h flac_encode.cpp flac_encode.h \
			   dsp.cpp dsp.hpp Biquad.cpp Biquad.h \
			   FLTK/Fl_ILM216.cpp FLTK/Fl_ILM216.h FLTK/fl_callbacks.cpp \
			   FLTK/fl_callbacks.h FLTK/fl_funcs.cpp FLTK/fl_funcs.h FLTK/flgui.cpp FLTK/flgui.h \
			   FLTK/FL/Fl_My_Double_Window.H FLTK/FL/Fl_My_Value_Slider.H \
			   FLTK/Fl_My_Native_File_Chooser.cxx FLTK/FL/Fl_My_Native_File_Chooser.H \
			   FLTK/fl_timer_funcs.cpp FLTK/fl_timer_funcs.h aac_encode.cpp aac_encode.h
			   

if WINDOWS
butt_SOURCES += resource.rc currentTrack.h currentTrack.cpp
endif

if OSX
butt_SOURCES += CurrentTrackOSX.h CurrentTrackOSX.m
endif

if LINUX
butt_SOURCES += currentTrack.h currentTrackLinux.cpp
endif

# used only under MinGW to compile the resource.rc file (manifest and program icon)

.rc.o:
	windres $^ -o $@
%.o : %.rc
	windres $^ -o $@
