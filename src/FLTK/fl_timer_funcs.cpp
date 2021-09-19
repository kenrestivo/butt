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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>

#ifndef WIN32
 #include <sys/wait.h>
#endif

#ifdef WIN32
 #include "tray_agent.h"
#endif

#include "gettext.h"
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
#include "fl_timer_funcs.h"
#include "command.h"

#if __APPLE__ && __MACH__
 #include "CurrentTrackOSX.h"
#endif

const char* (*current_track_app)(int);

void split_recording_file(void);
pthread_t split_recording_file_thread;

void cmd_timer(void*)
{
    command_t command;
    command_get_last_cmd(&command);
    switch(command.cmd)
    {
        case CMD_CONNECT:
            if (!connected) {
                if (command.param_size > 0) {
                    char *srv_name = (char*)command.param;
                    int idx;
                    if ((idx = fl_g->choice_cfg_act_srv->find_index(srv_name)) != -1) {
                        fl_g->choice_cfg_act_srv->value(idx);
                        fl_g->choice_cfg_act_srv->do_callback();
                        button_connect_cb();
                    }
                    else
                        Fl::repeat_timeout(0.25, &cmd_timer);
                    
                    if (command.param != NULL) {
                        free(command.param);
                    }
                }
                else
                    button_connect_cb();
            }
           //  Fl::repeat_timeout(0.25, &cmd_timer) is called in button_connect_cb() after the connect_thread() has returned (crashes if called here)
            else
                Fl::repeat_timeout(0.25, &cmd_timer);
            break;
        case CMD_DISCONNECT:
            button_disconnect_cb();
            Fl::repeat_timeout(0.25, &cmd_timer);
            break;
        case CMD_START_RECORDING:
            if (!recording)
                button_record_cb();
            Fl::repeat_timeout(0.25, &cmd_timer);
            break;
        case CMD_STOP_RECORDING:
            stop_recording(false);
            Fl::repeat_timeout(0.25, &cmd_timer);
            break;
        case CMD_SPLIT_RECORDING:
            split_recording_file();
            Fl::repeat_timeout(0.25, &cmd_timer);
            break;
        case CMD_QUIT:
            stop_recording(false);
            button_disconnect_cb();
            exit(0);
            break;
        case CMD_UPDATE_SONGNAME:
            cfg.main.song = (char*)realloc(cfg.main.song, command.param_size+1);
            strcpy(cfg.main.song, (char*)command.param);
            Fl::add_timeout(cfg.main.song_delay, &update_song);
            Fl::repeat_timeout(0.25, &cmd_timer);
            break;
        case CMD_GET_STATUS:
            uint32_t status;
            status = (connected<<STATUS_CONNECTED) | (try_to_connect<<STATUS_CONNECTING) | (recording<<STATUS_RECORDING);
            command_send_status_reply(status);
            Fl::repeat_timeout(0.25, &cmd_timer);
            break;
        default:
            Fl::repeat_timeout(0.25, &cmd_timer);
    }
}

void agent_watchdog_timer(void*)
{
#ifdef WIN32
    int is_iconfied = fl_g->window_main->shown() && !fl_g->window_main->visible();
    
    if (cfg.main.minimize_to_tray == 1 && is_iconfied == 1 && tray_agent_is_running(NULL) == 0)
        fl_g->window_main->show();
    else
        Fl::repeat_timeout(1, &agent_watchdog_timer);
#endif
}

void has_agent_been_started_timer(void*)
{
#ifdef WIN32
    if (tray_agent_is_running(NULL) != 1)
	fl_alert("Agent could not be started.");
#endif
}

void has_agent_been_stopped_timer(void*)
{
#ifdef WIN32
    if (tray_agent_is_running(NULL) != 0)
	fl_alert("Agent could not be stopped.");
#endif
}

void vu_meter_timer(void*)
{
    if(pa_new_frames)
        snd_update_vu();

    Fl::repeat_timeout(0.05, &vu_meter_timer);
}

void display_info_timer(void*)
{
    char lcd_text_buf[33];

    if(try_to_connect == 1)
    {
        Fl::repeat_timeout(0.2, &display_info_timer);
        return;
    }

    if(display_info == SENT_DATA)
    {
        snprintf(lcd_text_buf, sizeof(lcd_text_buf), _("stream sent\n%0.2lfMB"),
                kbytes_sent / 1024);
        print_lcd(lcd_text_buf, strlen(lcd_text_buf), 0, 1);
    }

    if(display_info == STREAM_TIME && timer_is_elapsed(&stream_timer))
    {
        snprintf(lcd_text_buf, sizeof(lcd_text_buf), _("stream time\n%s"),
                timer_get_time_str(&stream_timer));
        print_lcd(lcd_text_buf, strlen(lcd_text_buf), 0, 1);
    }

    if(display_info == REC_TIME && timer_is_elapsed(&rec_timer))
    {
        snprintf(lcd_text_buf, sizeof(lcd_text_buf), _("record time\n%s"),
                timer_get_time_str(&rec_timer));
        print_lcd(lcd_text_buf, strlen(lcd_text_buf), 0, 1);
    }

    if(display_info == REC_DATA)
    {
        snprintf(lcd_text_buf, sizeof(lcd_text_buf), _("record size\n%0.2lfMB"),
                kbytes_written / 1024);
        print_lcd(lcd_text_buf, strlen(lcd_text_buf), 0, 1);
    }

    Fl::repeat_timeout(0.2, &display_info_timer);
}

void display_rotate_timer(void*) 
{

    if(!connected && !recording)
        goto exit;

    if (!cfg.gui.lcd_auto)
        goto exit;

    switch(display_info)
    {
        case STREAM_TIME:
            display_info = SENT_DATA;
            break;
        case SENT_DATA:
            if(recording)
                display_info = REC_TIME;
            else
                display_info = STREAM_TIME;
            break;
        case REC_TIME:
            display_info = REC_DATA;
            break;
        case REC_DATA:
            if(connected)
                display_info = STREAM_TIME;
            else
                display_info = REC_TIME;
            break;
        default:
            break;
    }

exit:
    Fl::repeat_timeout(5, &display_rotate_timer);

}

void is_connected_timer(void*)
{
    if(!connected)
    {
        print_info(_("ERROR: Connection lost\nreconnecting..."), 1);
        if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
            sc_disconnect();
        else
            ic_disconnect();

	
#ifdef WIN32
	tray_agent_send_cmd(TA_CONNECT_STATE);
#endif

        Fl::remove_timeout(&display_info_timer);
        Fl::remove_timeout(&is_connected_timer);

        //reconnect
        button_connect_cb();

        return;
    }

    Fl::repeat_timeout(0.5, &is_connected_timer);
}

void cfg_win_pos_timer(void*)
{

#ifdef WIN32
    fl_g->window_cfg->position(fl_g->window_main->x() +
                                fl_g->window_main->w()+0,
                                fl_g->window_main->y());
#else //UNIX
    fl_g->window_cfg->position(fl_g->window_main->x() +
                                fl_g->window_main->w(),
                                fl_g->window_main->y());
#endif

    Fl::repeat_timeout(0.1, &cfg_win_pos_timer);
}

void *split_recording_file_thread_func(void *init)
{
       
    int synced_to_full_hour;
    time_t start_time;
        
    time_t current_time;
    struct tm *current_time_tm;

    int split_time_seconds = 60*cfg.rec.split_time;
    
    struct timespec wait_400ms;
    wait_400ms.tv_sec = 0;
    wait_400ms.tv_nsec = (400*1000*1000);
    
    start_time = time(NULL);
    synced_to_full_hour = 0;
    
    set_max_thread_priority();
    
    while(record)
    {
        current_time = time(NULL);
        current_time_tm = localtime(&current_time);
        
        if((cfg.rec.sync_to_hour == 1)      &&
           (synced_to_full_hour == 0)       &&
           (current_time_tm->tm_min == 0)   &&
           (current_time_tm->tm_sec >= 0))
        {
            start_time = current_time-current_time_tm->tm_sec;
            synced_to_full_hour = 1;
            split_recording_file();
        }
        else if(current_time-start_time >= split_time_seconds)
        {
            start_time = current_time;
            split_recording_file();
        }
        
        nanosleep(&wait_400ms, NULL);
        
    }
    
    return NULL;
}



void split_recording_file_timer(void)
{
    pthread_create(&split_recording_file_thread, NULL, split_recording_file_thread_func, NULL);
}

void split_recording_file(void)
{
    int i;
    char *insert_pos;
    char *path;
    char *ext;
    char file_num_str[12];
    char *path_for_index_loop;
    
    if (!recording)
        return;
        
    path = strdup(cfg.rec.path_fmt);
    expand_string(&path);
    ext = util_get_file_extension(cfg.rec.filename);
    if(ext == NULL)
    {
        print_info(_("Could not find a file extension in current filename\n"
                "Automatic file splitting is deactivated"), 0);
        free(path);
        return;
    }

    path_for_index_loop = strdup(path);

    //check if the file already exists
    if((next_fd = fl_fopen(path, "rb")) != NULL)
    {
        fclose(next_fd);

        //increment the index until we find a filename that doesn't exist yet
        for(i = 1; /*inf*/; i++) // index_loop
        {
   
            free(path);
            path = strdup(path_for_index_loop);

            //find beginn of the file extension
            insert_pos = strrstr(path, ext);

            //Put index between end of file name end beginning of extension
            snprintf(file_num_str, sizeof(file_num_str), "-%d", i);
            strinsrt(&path, file_num_str, insert_pos-1);

            if((next_fd = fl_fopen(path, "rb")) == NULL)
                break; // found valid file name

            fclose(next_fd);

            if (i == 0x7FFFFFFF) // 2^31-1
            {
                free(path);
                free(path_for_index_loop);
                print_info(_("Could not find a valid filename for next file"
                        "\nbutt keeps recording to current file"), 0);
                return;
            }
        }
    }

    free(path_for_index_loop);

    if((next_fd = fl_fopen(path, "wb")) == NULL)
    {
        fl_alert(_("Could not open:\n%s"), path);
        free(path);
        return;
    }

    print_info(_("Recording to:"), 0);
    print_info(path, 0);

    next_file = 1;
    free(path);
}

void stream_signal_timer(void*)
{
   //printf("stream_signal_timer\n");
    static sec_timer signal_timer;
    if (signal_detected == true)
    {
        if (signal_timer.is_running == false)
            timer_init(&signal_timer, cfg.main.signal_threshold);

        if (timer_is_elapsed(&signal_timer))
        {
            //print_info("Audio signal detected", 0);
            button_connect_cb();
            timer_reset(&signal_timer);
            return;
        }
    }
    else
        timer_reset(&signal_timer);
    
    Fl::repeat_timeout(1, &stream_signal_timer);
}

void record_signal_timer(void*)
{
    //printf("record_signal_timer\n");

    static sec_timer signal_timer;
    if (signal_detected == true)
    {
        if (signal_timer.is_running == false)
            timer_init(&signal_timer, cfg.rec.signal_threshold);

        if (timer_is_elapsed(&signal_timer))
        {
            //print_info("Audio signal detected", 0);
            button_record_cb();
            timer_reset(&signal_timer);
            return;
        }
    }
    else
        timer_reset(&signal_timer);
    
    Fl::repeat_timeout(1, &record_signal_timer);
}

void stream_silence_timer(void*)
{
    //printf("stream_silence_timer\n");

    static sec_timer silence_timer;
    if (silence_detected == true)
    {
        if (silence_timer.is_running == false)
            timer_init(&silence_timer, cfg.main.silence_threshold);

        if (timer_is_elapsed(&silence_timer))
        {
            //print_info("Streaming silence threshold has been reached", 0);
            button_disconnect_cb();
            timer_reset(&silence_timer);
            return;
        }
    }
    else
        timer_reset(&silence_timer);
    
    Fl::repeat_timeout(1, &stream_silence_timer);
}

void record_silence_timer(void*)
{
    //printf("record_silence_timer\n");

    static sec_timer silence_timer;
    if (silence_detected == true)
    {
        if (silence_timer.is_running == false)
            timer_init(&silence_timer, cfg.rec.silence_threshold);

        if (timer_is_elapsed(&silence_timer))
        {
            //print_info("Recording silence threshold has been reached", 0);
            stop_recording(false);
            timer_reset(&silence_timer);
            return;
        }
    }
    else
        timer_reset(&silence_timer);
    
    Fl::repeat_timeout(1, &record_silence_timer);
}


void songfile_timer(void* user_data)
{
    size_t len;
	int i;
	int num_of_lines;
	int num_of_newlines;
    char song[501];
    char msg[100];
    float repeat_time = 1;

    int reset;
    if (user_data != NULL)
        reset = *((int*)user_data);
    else
        reset = 0;
    
#ifdef WIN32
    struct _stat s;
#else
    struct stat s;
#endif

    static time_t old_t;
    
    if (reset == 1) {
        old_t = 0;
    }
    
    char *last_line = NULL;

    if( (cfg.main.song_path == NULL) || (connected == 0) )
        goto exit;

    if(fl_stat(cfg.main.song_path, (struct stat*)&s) != 0)
    {
        // File was probably locked by another application
        // retry in 5 seconds
        repeat_time = 5;
        goto exit;
    }

    if(old_t == s.st_mtime) //file hasn't changed
        goto exit;

   if((cfg.main.song_fd = fl_fopen(cfg.main.song_path, "rb")) == NULL)
   {
	   snprintf(msg, sizeof(msg), _("Warning\nCould not open: %s.\nWill retry in 5 seconds"), 
					   cfg.main.song_path); 

       print_info(msg, 1);
       repeat_time = 5;
       goto exit;
   }

    old_t = s.st_mtime;
    
    if(cfg.main.read_last_line == 1)
    {
        /* Read last line instead of first */
        
        fseek(cfg.main.song_fd, -100, SEEK_END);
        len = fread(song, sizeof(char), 100, cfg.main.song_fd);
        if(len == 0)
        {
            fclose(cfg.main.song_fd);
            goto exit;
        }

		// Count number of lines within the last 100 characters of the file
		// Some programs add a new line to the end of the file and some don't
		// We have to take this into account when counting the number of lines
		num_of_newlines = 0;
		for(i = 0; i < len; i++) 
		{
			if (song[i] == '\n')
				num_of_newlines++;
		}

		if(num_of_newlines == 0) 
			num_of_lines = 1;
		else if (num_of_newlines > 0 && song[len-1] != '\n')
			num_of_lines = num_of_newlines+1;
		else
			num_of_lines = num_of_newlines;
        
        if(num_of_lines > 1) // file has multiple lines
        {
			// Remove newlines at end of file
            if(song[len-2] == '\r') // Windows
                song[len-2] = '\0';
            else if(song[len-1] == '\n') // OSX, Linux
                song[len-1] = '\0';
			else
				song[len] = '\0';

            last_line = strrchr(song, '\n')+1;
        }
        else // file has only one line
        {
		
			// Remove newlines at end of file
            if(song[len-2] == '\r') // Windows
                song[len-2] = '\0';
            else if(song[len-1] == '\n') // OSX, Linux
                song[len-1] = '\0';
			else
				song[len] = '\0';

			last_line = song;
        }
        
        // Remove UTF-8 BOM
        if ((uint8_t)last_line[0] == 0xEF && (uint8_t)last_line[1] == 0xBB && (uint8_t)last_line[2] == 0xBF)
        {
            cfg.main.song = (char*) realloc(cfg.main.song, strlen(last_line) +1);
            strcpy(cfg.main.song, last_line+3);
        }
        else
        {
            cfg.main.song = (char*) realloc(cfg.main.song, strlen(last_line) +1);
            strcpy(cfg.main.song, last_line);
        }

        Fl::add_timeout(cfg.main.song_delay, &update_song);

    }
    else
    {
		// read first line
        if(fgets(song, 500, cfg.main.song_fd) != NULL)
        {
            len = strlen(song);
            
            //remove newline character
            if(song[len-2] == '\r') // Windows
                song[len-2] = '\0';
            else if(song[len-1] == '\n') // OSX, Linux
                song[len-1] = '\0';
            
            // Remove UTF-8 BOM
            if ((uint8_t)song[0] == 0xEF && (uint8_t)song[1] == 0xBB && (uint8_t)song[2] == 0xBF)
            {
                cfg.main.song = (char*) realloc(cfg.main.song, strlen(song) +1);
                strcpy(cfg.main.song, song+3);
            }
            else
            {
                cfg.main.song = (char*) realloc(cfg.main.song, strlen(song) +1);
                strcpy(cfg.main.song, song);
            }
            
            Fl::add_timeout(cfg.main.song_delay, &update_song);
        }
    }

   fclose(cfg.main.song_fd);

exit:
    Fl::repeat_timeout(repeat_time, &songfile_timer);
}


void app_timer(void* user_data)
{
    int reset;
    
    if (user_data != NULL)
        reset = *((int*)user_data);
    else
        reset = 0;

    
    if(current_track_app != NULL)
    {
        const char* track = current_track_app(cfg.main.app_artist_title_order);
        if(track != NULL)
        {
            if(cfg.main.song == NULL || strcmp(cfg.main.song, track) || reset == 1)
            {
                cfg.main.song = (char*) realloc(cfg.main.song, strlen(track) + 1);
                strcpy(cfg.main.song, track);
                Fl::add_timeout(cfg.main.song_delay, &update_song);
            }
            free((void*)track);
        }
        else
        {
            if(cfg.main.song != NULL && strcmp(cfg.main.song, ""))
            {
                cfg.main.song = (char*) realloc(cfg.main.song, 1);
                strcpy(cfg.main.song, "");
                Fl::add_timeout(cfg.main.song_delay, &update_song);
            }
        }
    }
    
    Fl::repeat_timeout(1, &app_timer);
}
