// FLTK GUI related functions
//
// Copyright 2007-2018 by Daniel Noethen.
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

#include <FL/fl_ask.H>

#define GUI_LOOP() Fl::run();
#define CHECK_EVENTS() Fl::check()
#define ALERT(msg) fl_alert("%s", msg)

void fill_cfg_widgets(void);
void update_samplerates(void);
void update_channel_lists(void);
void print_info(const char* info, int info_type);
void print_lcd(const char *text, int len, int home, int clear);
void test_file_extension(void);
void expand_string(char **str);
void init_main_gui_and_audio(void);
void ask_user_set_msg(char *m);
void ask_user_set_hash(char *h);
void ask_user_reset(void);
void ask_user_ask(void);
int ask_user_get_answer(void);
int ask_user_get_has_clicked(void);


typedef const char* (*currentTrackFunction)(int);
extern currentTrackFunction getCurrentTrackFunctionFromId(int i);

#endif

