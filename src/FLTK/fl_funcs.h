// FLTK GUI related functions
//
// Copyright 2007-2008 by Daniel Noethen.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#ifndef FL_FUNCS_H
#define FL_FUNCS_H

#include <Fl/fl_ask.H>

#define PRINT_LCD(msg, len, home, clear) print_lcd(msg, len, home, clear)      //prints text to the LCD
#define GUI_LOOP() Fl::run();
#define CHECK_EVENTS() Fl::check()
#define ALERT(msg) fl_alert("%s", msg)

void fill_cfg_widgets();
void update_samplerates();
void print_info(const char* info, int info_type);
void print_lcd(const char *text, int len, int home, int clear);
void check_time(void*);
void check_frames(void*);
void check_if_disconnected(void*);
void check_cfg_win_pos(void*);
void check_song_update(void*);
void check_split_time(void*);
void test_file_extension();
void expand_string(char **str);
void init_main_gui_and_audio(void);

#endif

