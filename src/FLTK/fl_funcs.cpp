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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

#ifndef _WIN32
 #include <sys/wait.h>
#endif

#include "config.h"

#include "cfg.h"
#include "butt.h"
#include "util.h"
#include "port_audio.h"
#include "timer.h"
#include "flgui.h"
#include "fl_funcs.h"
#include "shoutcast.h"
#include "icecast.h"
#include "strfuncs.h"




void fill_cfg_widgets()
{
    int i;

    int bitrate[] = { 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112,
                    128, 160, 192, 224, 256, 320, 0 };

  
    //fill the main section
    for(i = 0; i < cfg.audio.dev_count; i++)
        fl_g->choice_cfg_dev->add(cfg.audio.pcm_list[i]->name);

    fl_g->choice_cfg_dev->value(cfg.audio.dev_num);

    fl_g->choice_cfg_act_srv->clear();
    fl_g->choice_cfg_act_srv->redraw();
    for(i = 0; i < cfg.main.num_of_srv; i++)
        fl_g->choice_cfg_act_srv->add(cfg.srv[i]->name);

    if(cfg.main.num_of_srv > 0)
    {
        fl_g->button_cfg_edit_srv->activate();
        fl_g->button_cfg_del_srv->activate();
    }
    else
    {
        fl_g->button_cfg_edit_srv->deactivate();
        fl_g->button_cfg_del_srv->deactivate();
    }

    fl_g->choice_cfg_act_icy->clear();
    fl_g->choice_cfg_act_icy->redraw();
    for(i = 0; i < cfg.main.num_of_icy; i++)
        fl_g->choice_cfg_act_icy->add(cfg.icy[i]->name);

    if(cfg.main.num_of_icy > 0)
    {
        fl_g->button_cfg_edit_icy->activate();
        fl_g->button_cfg_del_icy->activate();
    }
    else
    {
        fl_g->button_cfg_edit_icy->deactivate();
        fl_g->button_cfg_del_icy->deactivate();
    }

    fl_g->choice_cfg_act_srv->value(cfg.selected_srv);
    fl_g->choice_cfg_act_icy->value(cfg.selected_icy);


	fl_g->check_cfg_connect->value(cfg.main.connect_at_startup);
    


    //fill the audio (stream) section
    if(!strcmp(cfg.audio.codec, "mp3"))
        fl_g->radio_cfg_codec_mp3->setonly();
    else if(!strcmp(cfg.audio.codec, "ogg"))
        fl_g->radio_cfg_codec_ogg->setonly();

    if(cfg.audio.channel == 1)
        fl_g->radio_cfg_channel_mono->setonly();
    else
        fl_g->radio_cfg_channel_stereo->setonly();

    for(i = 0; bitrate[i] != 0; i++)
        if(cfg.audio.bitrate == bitrate[i])
        {
            fl_g->choice_cfg_bitrate->value(i);
            break;
        }

    if(cfg.main.song_update)
        fl_g->check_song_update_active->value(1);

    fl_g->input_cfg_song_file->value(cfg.main.song_path);

    //fill the record section
    fl_g->input_rec_filename->value(cfg.rec.filename);
    fl_g->input_rec_folder->value(cfg.rec.folder);
    fl_g->input_rec_split_time->value(cfg.rec.split_time);

    if(!strcmp(cfg.rec.codec, "mp3"))
        fl_g->radio_rec_codec_mp3->setonly();
    else if(!strcmp(cfg.rec.codec, "ogg"))
           fl_g->radio_rec_codec_ogg->setonly();
    else //wav
    {
        fl_g->radio_rec_codec_wav->setonly();
        fl_g->choice_rec_bitrate->deactivate();
    }

    if(cfg.rec.channel == 1)
        fl_g->radio_rec_channel_mono->setonly();
    else
        fl_g->radio_rec_channel_stereo->setonly();

    for(i = 0; bitrate[i] != 0; i++)
        if(cfg.rec.bitrate == bitrate[i])
            fl_g->choice_rec_bitrate->value(i);

    if(cfg.rec.start_rec)
        fl_g->check_cfg_rec->value(1);
    else
        fl_g->check_cfg_rec->value(0);

    update_samplerates();

    //fill the GUI section
    fl_g->button_gui_bg_color->color(cfg.main.bg_color,
            fl_lighter((Fl_Color)cfg.main.bg_color));
    fl_g->button_gui_text_color->color(cfg.main.txt_color,
            fl_lighter((Fl_Color)cfg.main.txt_color));
    fl_g->check_gui_attach->value(cfg.gui.attach);
    fl_g->check_gui_ontop->value(cfg.gui.ontop);
    if(cfg.gui.ontop)
    {
        fl_g->window_main->stay_on_top(1);
        fl_g->window_cfg->stay_on_top(1);
    }

}

//Updates the samplerate drop down menu for the audio
//device the user has selected
void update_samplerates()
{
    int i;
    int *sr_list;
    char sr_asc[10];

    fl_g->choice_cfg_samplerate->clear();
    fl_g->choice_rec_samplerate->clear();

    sr_list = cfg.audio.pcm_list[cfg.audio.dev_num]->sr_list;

    for(i = 0; sr_list[i] != 0; i++)
    {
        snprintf(sr_asc, 10, "%dHz", sr_list[i]);
        fl_g->choice_cfg_samplerate->add(sr_asc);
        fl_g->choice_rec_samplerate->add(sr_asc);

        if(cfg.audio.samplerate == sr_list[i])
            fl_g->choice_cfg_samplerate->value(i);

        if(cfg.rec.samplerate == sr_list[i])
            fl_g->choice_rec_samplerate->value(i);
    }
    if(i == 0)
    {
        fl_g->choice_cfg_samplerate->add("dev. not supported");
        fl_g->choice_rec_samplerate->add("dev. not supported");
        fl_g->choice_cfg_samplerate->value(0);
        fl_g->choice_rec_samplerate->value(0);
    }
}

void print_info(const char* info, int info_type)
{
    char timebuf[10];
    time_t test;

    struct tm  *mytime;
    static struct tm time_bak;

    test = time(NULL);
    mytime = localtime(&test);

    if( (time_bak.tm_min != mytime->tm_min) || (time_bak.tm_hour != mytime->tm_hour) )
    {
        time_bak.tm_min = mytime->tm_min;
        time_bak.tm_hour = mytime->tm_hour;
        strftime(timebuf, sizeof(timebuf), "\n%H:%M:", mytime);
        fl_g->info_buffer->append(timebuf);
    }

    fl_g->info_buffer->append((const char*)"\n");
    fl_g->info_buffer->append((const char*)info);

    //always scroll to the last line
    fl_g->info_output->scroll(fl_g->info_buffer->count_lines(0,     //count the lines from char 0 to the last character
                            fl_g->info_buffer->length()),           //returns the number of characters in the buffer
                            0);
}

void print_lcd(const char *text, int len, int home, int clear)
{
    if(clear)
        fl_g->lcd->clear();

    fl_g->lcd->print((const uchar*)text, len);

    if(home)
        fl_g->lcd->cursor_pos(0);
}

void check_frames(void*)
{
    if(pa_new_frames)
        snd_update_vu();

    Fl::repeat_timeout(0.01, &check_frames);
}

void check_time(void*)
{
    char lcd_text_buf[33];

    if(display_info == SENT_DATA)
    {
        
        sprintf(lcd_text_buf, "info: on air\nsent: %0.2lfMB",
                kbytes_sent / 1024);
        print_lcd(lcd_text_buf, strlen(lcd_text_buf), 0, 1);
    }

    if(display_info == STREAM_TIME && timer_is_elapsed(&stream_timer))
    {
        sprintf(lcd_text_buf, "info: on air\ntime: %s",
                timer_get_time_str(&stream_timer));
        print_lcd(lcd_text_buf, strlen(lcd_text_buf), 0, 1);
    }

    if(display_info == REC_TIME && timer_is_elapsed(&rec_timer))
    {
        sprintf(lcd_text_buf, "info: record\ntime: %s",
                timer_get_time_str(&rec_timer));
        print_lcd(lcd_text_buf, strlen(lcd_text_buf), 0, 1);
    }

    if(display_info == REC_DATA)
    {
        sprintf(lcd_text_buf, "info: record\nsize: %0.2lfMB",
                kbytes_written / 1024);
        print_lcd(lcd_text_buf, strlen(lcd_text_buf), 0, 1);
    }

    Fl::repeat_timeout(0.1, &check_time);
}

void check_if_disconnected(void*)
{
    if(!connected)
    {
        print_info("ERROR: Connection lost\nreconnecting...", 1);
        if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
            sc_disconnect();
        else
            ic_disconnect();

        Fl::remove_timeout(&check_time);
        Fl::remove_timeout(&check_if_disconnected);

        //reconnect
        button_connect_cb();

        return;
    }

    Fl::repeat_timeout(0.5, &check_if_disconnected);
}

void check_cfg_win_pos(void*)
{

#ifdef _WIN32
    fl_g->window_cfg->position(fl_g->window_main->x() +
                                fl_g->window_main->w()+15,
                                fl_g->window_main->y());
#else //UNIX
    fl_g->window_cfg->position(fl_g->window_main->x() +
                                fl_g->window_main->w(),
                                fl_g->window_main->y());
#endif

    Fl::repeat_timeout(0.1, &check_cfg_win_pos);
}

void check_split_time(void *initial_call)
{
    int zero = 0;
    char *insert_pos;
    char *path;
    char *ext;
    char file_num_str[10];
    static int file_num;
    
    if(*((int*)initial_call) == 1)
        file_num = 2;

    /*
    bei rec_cb diesen timer starten, falls split_time > 0 ist
    darauf achten, dass zwischen öffnen und schließen der dateien
    1. keine Daten verloren gehen und
    2. der snd_rec thread nicht in die datei zu schließende datei schreibt
    (Vielleiht kann man dazu mit rec_pause und rec_resume arbeiten

*/

    // Values < 0 are not allowed
    if(fl_g->input_rec_split_time->value() < 0)
    {
        fl_g->input_rec_split_time->value(0);
        return;
    }

    path = strdup(cfg.rec.path);
    ext = util_get_file_extension(cfg.rec.filename);
    if(ext == NULL)
    {
        print_info("Could not find a file extension (mp3/ogg/wav) in current filename\n"
                "Automatic file splitting is deactivated", 0);
        return;
    }


    snprintf(file_num_str, sizeof(file_num_str), "-%d", file_num); 

    insert_pos = strrstr(path, ext);
    strinsrt(&path, file_num_str, insert_pos-1);


    if((next_fd = fl_fopen(path, "rb")) != NULL)
    {
        print_info("Next file ", 0);
        print_info(path, 0);
        print_info("already exists\nbutt keeps recording to current file", 0);
        fclose(next_fd);
        return;
    }

    if((next_fd = fl_fopen(path, "wb")) == NULL)
    {
        fl_alert("Could not open:\n%s", path);
        return;
    }

    print_info("Recording to:", 0);
    print_info(path, 0);

    file_num++;


    next_file = 1;
    free(path);

    Fl::repeat_timeout(60*cfg.rec.split_time, &check_split_time, &zero);
}
void check_song_update(void*)
{
    int len;
    char song[501];
    struct stat s;
    static time_t old_t;


    if(cfg.main.song_path == NULL)
        goto exit;

    if(stat(cfg.main.song_path, &s) != 0)
    {

        // File was probably locked by another application
        // retry in 5 seconds
        Fl::repeat_timeout(5.0, &check_song_update);
        return;
    }

    if(old_t == s.st_mtime) //file hasn't changed
        goto exit;

    old_t = s.st_mtime;

   if((cfg.main.song_fd = fl_fopen(cfg.main.song_path, "rb")) == NULL)
   {
       fl_alert("could not open:\n%s\nplease check permissions", cfg.main.song_path);
       fl_g->check_song_update_active->value(0);
       fl_g->check_song_update_active->redraw();
       song_timeout_running = 0;
       return;
   }

   if(fgets(song, 500, cfg.main.song_fd) != NULL)
   {
       len = strlen(song);
       //remove newline character
       if(song[len-1] == '\n' || song[len-1] == '\r')
           song[len-1] = '\0';

       cfg.main.song = (char*) realloc(cfg.main.song, strlen(song) +1);
       strcpy(cfg.main.song, song);
       button_cfg_song_go_cb();
   }

   fclose(cfg.main.song_fd);

exit:
    Fl::repeat_timeout(1.0, &check_song_update);
}

void expand_string(char **str)
{
    char m[3], d[3], y[5];
    struct tm *date;
    const time_t t = time(NULL);

    date = localtime(&t);

    snprintf(m, 3, "%02d", date->tm_mon+1);
    snprintf(d, 3, "%02d", date->tm_mday);
    snprintf(y, 5, "%d",   date->tm_year+1900);

    strrpl(str, (char*)"%m", m);
    strrpl(str, (char*)"%d", d);
    strrpl(str, (char*)"%y", y);
}

void test_file_extension()
{
    char *ext;

    ext = util_get_file_extension(cfg.rec.filename);
    if(ext == NULL)
    {
        print_info("Warning:\nRecord filename hasn't got an extension (mp3/ogg/wav)", 1);
        return;
    }

    if(strcmp(ext, cfg.rec.codec))
        print_info("Warning:\nThe extension of your record filename\n"
                "does not match your record codec", 1);
}



void init_main_gui_and_audio(void)
{
    fl_g->slider_gain->value(util_factor_to_db(cfg.main.gain));
    fl_g->window_main->redraw();

    if(cfg.gui.ontop)
        fl_g->window_main->stay_on_top(1);

    lame_stream.channel = cfg.audio.channel;
    lame_stream.bitrate = cfg.audio.bitrate;
    lame_stream.samplerate_in = cfg.audio.samplerate;
    lame_stream.samplerate_out = cfg.audio.samplerate;
    lame_enc_reinit(&lame_stream);

    lame_rec.channel = cfg.rec.channel;
    lame_rec.bitrate = cfg.rec.bitrate;
    lame_rec.samplerate_in = cfg.audio.samplerate;
    lame_rec.samplerate_out = cfg.rec.samplerate;
    lame_enc_reinit(&lame_rec);

    vorbis_stream.channel = cfg.audio.channel;
    vorbis_stream.bitrate = cfg.audio.bitrate;
    vorbis_stream.samplerate = cfg.audio.samplerate;
    vorbis_enc_reinit(&vorbis_stream);

    vorbis_rec.channel = cfg.rec.channel;
    vorbis_rec.bitrate = cfg.rec.bitrate;
    vorbis_rec.samplerate = cfg.rec.samplerate;
    vorbis_enc_reinit(&vorbis_rec);
}

