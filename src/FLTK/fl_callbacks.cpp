// FLTK callback functions for butt
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
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#ifdef _WIN32
 #define usleep(us) Sleep(us/1000)
#else
 #include <sys/wait.h>
#endif

#include <FL/fl_ask.H>
#include <FL/Fl_Color_Chooser.H>

#include "config.h"

#include "FL/Fl_My_Native_File_Chooser.H"
#include "cfg.h"
#include "butt.h"
#include "port_audio.h"
#include "timer.h"
#include "shoutcast.h"
#include "icecast.h"
#include "lame_encode.h"
#include "fl_callbacks.h"
#include "strfuncs.h"
#include "flgui.h"
#include "util.h"


flgui *fl_g; 
int display_info = STREAM_TIME;

void button_connect_cb()
{
    if(connected)
        return;


    int (*xc_connect)() = NULL;
    int rv;
    int dummy = 0;

    char text_buf[256];

    if(cfg.main.num_of_srv < 1)
    {
        print_info("No server entry found.\nPlease add a server in the settings-window.", 1);
        return;
    }

    if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
        xc_connect = &sc_connect;

    if(cfg.srv[cfg.selected_srv]->type == ICECAST)
        xc_connect = &ic_connect;

    if(!strcmp(cfg.audio.codec, "ogg") && (cfg.audio.bitrate < 48))
    {
        print_info("Ogg doesn't support bitrates \n"
                    "lower than 48kbit",1);
        return;
    }
    if(!strcmp(cfg.audio.codec, "ogg") && (cfg.srv[cfg.selected_srv]->type == SHOUTCAST))
    {
        print_info("ShoutCast doesn't support ogg", 1);
        return;
    }

    if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
        snprintf(text_buf, sizeof(text_buf), "Connecting to %s:%u (%u) ...",
            cfg.srv[cfg.selected_srv]->addr,
            cfg.srv[cfg.selected_srv]->port+1,
            cfg.srv[cfg.selected_srv]->port);
    else
        snprintf(text_buf, sizeof(text_buf), "Connecting to %s:%u ...",
            cfg.srv[cfg.selected_srv]->addr,
            cfg.srv[cfg.selected_srv]->port);

    print_info(text_buf, 0);

    try_to_connect = 1;

    while((rv = xc_connect())) //xc_connect returns 0 when connected
    {
        if( (rv == 2) || (try_to_connect == 0) )
        {
            try_to_connect = 0;  //invalid password or the user has pressed the stop button
            return;
        }

		if(dummy == 0)
		{
			print_lcd("connecting", 10, 0, 1);
			dummy++;
		}
		else if(dummy == 5 || dummy == 10 || dummy == 15)
		{
			print_lcd(".", 1, 0, 0);
			dummy++;
		}
		else if(dummy == 20)
		{
			dummy = 0;
		}
		else
			dummy++;



        Fl::wait(0);
    }

    if(connected)
    {
        //we have to make sure that the first audio data
        //the server sees are the ogg headers
        if(!strcmp(cfg.audio.codec, "ogg"))
            vorbis_enc_write_header(&vorbis_stream);

        print_info("Connection established", 0);
        snprintf(text_buf, sizeof(text_buf),
                "Settings:\n"
                "Type:       %s\n"
                "Codec:      %s\n"
                "Bitrate:    %dkbps\n"
                "Samplerate: %dHz\n",
                cfg.srv[cfg.selected_srv]->type == SHOUTCAST ? "ShoutCast" : "IceCast",
                cfg.audio.codec,
                cfg.audio.bitrate,
                cfg.audio.samplerate
                );

        if(cfg.srv[cfg.selected_srv]->type == ICECAST)
            sprintf(text_buf, "%sMountpoint: %s\n"
                                "User:       %s\n", text_buf,
                    cfg.srv[cfg.selected_srv]->mount,
                    cfg.srv[cfg.selected_srv]->usr);

        print_info(text_buf, 0);


        //the user may not change the sound device while streaming
        fl_g->choice_cfg_dev->deactivate();
        //the sames applies to the codecs
        fl_g->radio_cfg_codec_mp3->deactivate();
        fl_g->radio_cfg_codec_ogg->deactivate();

        //Changing any audio settings while streaming does not work with ogg :(
        if(!strcmp(cfg.audio.codec, "ogg"))
        {
            fl_g->choice_cfg_bitrate->deactivate();
            fl_g->choice_cfg_samplerate->deactivate();
            fl_g->radio_cfg_channel_mono->deactivate();
            fl_g->radio_cfg_channel_stereo->deactivate();
        }

        pa_new_frames = 0;

		//Just in case the record routine started a check_time timeout
		//already
		Fl::remove_timeout(&check_time);

		Fl::add_timeout(0.1, &check_time);

        Fl::add_timeout(0.1, &check_if_disconnected);

        if(cfg.main.song_update && !song_timeout_running)
        {
            Fl::add_timeout(0.1, &check_song_update);
            song_timeout_running = 1;
        }

        snd_start_stream();

        if(cfg.rec.start_rec && !recording)
        {
            button_record_cb();
            timer_init(&rec_timer, 1);
        }

        display_info = STREAM_TIME;
        button_cfg_song_go_cb();
    }
}

void button_cfg_cb()
{

    if(fl_g->window_cfg->shown())
    {
        fl_g->window_cfg->hide();
        fl_g->button_cfg->label("Settings@>");
        Fl::remove_timeout(&check_cfg_win_pos);
    }
    else
    {

/*
 * This is a bit stupid. Well, its Win32...
 * We need to place the cfg window a bit more to the right, otherwise
 * the main and the cfg window would overlap
*/

#ifdef _WIN32
    fl_g->window_cfg->position(fl_g->window_main->x() +
                                fl_g->window_main->w()+7,
                                fl_g->window_main->y());
#else
    fl_g->window_cfg->position(fl_g->window_main->x() +
                                fl_g->window_main->w(),
                                fl_g->window_main->y());
#endif
        fl_g->window_cfg->show();
        fl_g->button_cfg->label("Settings@<");

        fill_cfg_widgets();

        if(cfg.gui.attach)
            Fl::add_timeout(0.1, &check_cfg_win_pos);
    }
}

// add server
void button_add_srv_add_cb()
{
    int i;

    //error checking
    if((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_mount->value()) == 0))
    {
        fl_alert("No mountpoint specified\nSetting mountpoint to \"stream\"");
        fl_g->input_add_srv_mount->value("stream");
    }
    if((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_usr->value()) == 0))
    {
        fl_alert("No user specified\nSetting user to \"source\"");
        fl_g->input_add_srv_usr->value("source");
    }
    if(strlen(fl_g->input_add_srv_name->value()) == 0)
    {
        fl_alert("No name specified");
        return;
    }                               
    if(cfg.main.srv_ent != NULL)
    {
        if(strlen(fl_g->input_add_srv_name->value()) + strlen(cfg.main.srv_ent) > 1000)
        {
            fl_alert("The sum of characters of all your server names exeeds 1000\n"
                    "Please reduce the count of characters of each server name");
            return;
        }
    }
    if(strpbrk(fl_g->input_add_srv_name->value(), ";\\/\n\r") != NULL)
    {
        fl_alert("No newline characters and ;/\\ are allowed in the name field");
        return;
    }
    if(strlen(fl_g->input_add_srv_addr->value()) == 0)
    {
        fl_alert("No address specified");
        return;
    }
    if(strlen(fl_g->input_add_srv_pwd->value()) == 0)
    {
        fl_alert("No password specified");
        return;
    }
    if(strlen(fl_g->input_add_srv_port->value()) == 0)
    {
        fl_alert("No port specified");
        return;
    }
    else if((atoi(fl_g->input_add_srv_port->value()) > 65535) ||
            (atoi(fl_g->input_add_srv_port->value()) < 1) )
    {
        fl_alert("Invalid port number\nThe port number must be between 1 and 65535");
        return;

    }
    
    //check if the name already exists
    for(i = 0; i < cfg.main.num_of_srv; i++)
    {
        if(!strcmp(fl_g->input_add_srv_name->value(), cfg.srv[i]->name))
        {
            fl_alert("Server name already exist!");
            return;
        }
    }

    i = cfg.main.num_of_srv;
    cfg.main.num_of_srv++;

    cfg.srv = (server_t**)realloc(cfg.srv, cfg.main.num_of_srv * sizeof(server_t*));
    cfg.srv[i] = (server_t*)malloc(sizeof(server_t));

    cfg.srv[i]->name = (char*)malloc(strlen(fl_g->input_add_srv_name->value())+1);
    strcpy(cfg.srv[i]->name, fl_g->input_add_srv_name->value());

    cfg.srv[i]->addr = (char*)malloc(strlen(fl_g->input_add_srv_addr->value())+1);
    strcpy(cfg.srv[i]->addr, fl_g->input_add_srv_addr->value());

    //strip leading http:// from addr
    if(strstr(cfg.srv[i]->addr, "http://"))
        cfg.srv[i]->addr += strlen("http://");

    cfg.srv[i]->pwd = (char*)malloc(strlen(fl_g->input_add_srv_pwd->value())+1);
    strcpy(cfg.srv[i]->pwd, fl_g->input_add_srv_pwd->value());

    cfg.srv[i]->port = atoi(fl_g->input_add_srv_port->value());

    if(fl_g->radio_add_srv_icecast->value())
    {
        cfg.srv[i]->mount = (char*)malloc(strlen(fl_g->input_add_srv_mount->value())+1);
        strcpy(cfg.srv[i]->mount, fl_g->input_add_srv_mount->value());

        cfg.srv[i]->usr = (char*)malloc(strlen(fl_g->input_add_srv_usr->value())+1);
        strcpy(cfg.srv[i]->usr, fl_g->input_add_srv_usr->value());

        cfg.srv[i]->type = ICECAST;
    }
    else
    {
        cfg.srv[i]->mount = NULL;
        cfg.srv[i]->usr = NULL;
        cfg.srv[i]->type = SHOUTCAST;
    }

    if(cfg.main.num_of_srv > 1)
    {
        cfg.main.srv_ent = (char*)realloc(cfg.main.srv_ent,
                                         strlen(cfg.main.srv_ent) +
                                         strlen(cfg.srv[i]->name) +2);
        sprintf(cfg.main.srv_ent, "%s;%s", cfg.main.srv_ent, cfg.srv[i]->name);
    }
    else
    {
        cfg.main.srv_ent = (char*)malloc(strlen(cfg.srv[i]->name) +1);
        sprintf(cfg.main.srv_ent, "%s", cfg.srv[i]->name);
    }

    cfg.main.srv = (char*)realloc(cfg.main.srv, strlen(cfg.srv[i]->name)+1);

    strcpy(cfg.main.srv, cfg.srv[i]->name);

    //reset the input fields and hide the window
    fl_g->input_add_srv_name->value("");
    fl_g->input_add_srv_addr->value("");
    fl_g->input_add_srv_port->value("");
    fl_g->input_add_srv_pwd->value("");
    fl_g->input_add_srv_mount->value("");
    fl_g->input_add_srv_usr->value("");
    fl_g->window_add_srv->hide();

    fl_g->choice_cfg_act_srv->add(cfg.srv[i]->name);
    fl_g->choice_cfg_act_srv->redraw();      

    //Activate del and edit buttons
    fl_g->button_cfg_edit_srv->activate();
    fl_g->button_cfg_del_srv->activate();

    // make added server the active server
    fl_g->choice_cfg_act_srv->value(i);
    choice_cfg_act_srv_cb();

    unsaved_changes = 1;
}

void button_cfg_del_srv_cb()
{
    int i;
    int diff;

    if(cfg.main.num_of_srv == 0)
        return;

    diff = cfg.main.num_of_srv-1 - cfg.selected_srv;

    for(i = 0; i < diff; i++)
    {
        memcpy(cfg.srv[cfg.selected_srv+i], cfg.srv[cfg.selected_srv+i+1],
                sizeof(server_t));
    }

    free(cfg.srv[cfg.main.num_of_srv-1]);

    cfg.main.num_of_srv--;

    //rearrange the string that contains all server names
    memset(cfg.main.srv_ent, 0, strlen(cfg.main.srv_ent));
    for(i = 0; i < (int)cfg.main.num_of_srv; i++)
    {
        strcat(cfg.main.srv_ent, cfg.srv[i]->name);

        //the last entry doesn't have a trailing seperator ";"
        if(i < (int)cfg.main.num_of_srv-1)
            strcat(cfg.main.srv_ent, ";");

    }

    fl_g->choice_cfg_act_srv->remove(cfg.selected_srv);
    fl_g->choice_cfg_act_srv->redraw();       //Yes we need this :-(

    if(cfg.main.num_of_srv == 0)
    {
        fl_g->button_cfg_edit_srv->deactivate();
        fl_g->button_cfg_del_srv->deactivate();
    }

    if(cfg.selected_srv > 0)
        cfg.selected_srv--;
    else
        cfg.selected_srv = 0;

    if(cfg.main.num_of_srv > 0)
    {
        fl_g->choice_cfg_act_srv->value(cfg.selected_srv);
        choice_cfg_act_srv_cb();
    }

    unsaved_changes = 1;
}

void button_cfg_del_icy_cb()
{
    int i;
    int diff;

    if(cfg.main.num_of_icy == 0)
        return;

    diff = cfg.main.num_of_icy-1 - cfg.selected_icy;

    for(i = 0; i < diff; i++)
    {
        memcpy(cfg.icy[cfg.selected_icy+i], cfg.icy[cfg.selected_icy+i+1],
                sizeof(icy_t));
    }

    free(cfg.icy[cfg.main.num_of_icy-1]);

    cfg.main.num_of_icy--;

     //rearrange the string that contains all ICY names
    memset(cfg.main.icy_ent, 0, strlen(cfg.main.icy_ent));
    for(i = 0; i < (int)cfg.main.num_of_icy; i++)
    {
        strcat(cfg.main.icy_ent, cfg.icy[i]->name);

        //the last entry doesn't have a trailing seperator ";"
        if(i < (int)cfg.main.num_of_icy-1)
            strcat(cfg.main.icy_ent, ";");
    }


    fl_g->choice_cfg_act_icy->remove(cfg.selected_icy);
    fl_g->choice_cfg_act_icy->redraw();       

    if(cfg.main.num_of_icy == 0)
    {
        fl_g->button_cfg_edit_icy->deactivate();
        fl_g->button_cfg_del_icy->deactivate();
    }

    if(cfg.selected_icy > 0)
        cfg.selected_icy--;
    else
        cfg.selected_icy = 0;

    if(cfg.main.num_of_icy > 0)
    {
        fl_g->choice_cfg_act_icy->value(cfg.selected_icy);
        choice_cfg_act_icy_cb();
    }


    unsaved_changes = 1;
}

void button_disconnect_cb()
{
    int button;

    if(recording)
    {
        button = fl_choice("stop recording?", "Cancel", "Yes", "No");
        switch(button)
        {
            case 0:
                return;
            case 1:
                Fl::remove_timeout(&check_split_time);
                fl_g->input_rec_split_time->activate();
                fl_g->text_rec_split_time->activate();
                snd_stop_rec();
				Fl::remove_timeout(&check_time);
                fl_g->lcd->clear();
                fl_g->lcd->print((const uchar*)"info: idle", 10);
            default:
                break;
        }
    }
    else
    {
        fl_g->lcd->clear();
        fl_g->lcd->print((const uchar*)"info: idle", 10);
    }

    if(!try_to_connect)
        return;

    fl_g->choice_cfg_dev->activate();
    fl_g->radio_cfg_codec_mp3->activate();
    fl_g->radio_cfg_codec_ogg->activate();

    fl_g->choice_cfg_bitrate->activate();
    fl_g->choice_cfg_samplerate->activate();
    fl_g->radio_cfg_channel_mono->activate();
    fl_g->radio_cfg_channel_stereo->activate();

    try_to_connect = 0;


    if(!recording)
        Fl::remove_timeout(&check_time);
    else
        display_info = REC_TIME;

    if(song_timeout_running)
    {
        Fl::remove_timeout(&check_song_update);
        song_timeout_running = 0;
    }

    if(connected)
    {
        Fl::remove_timeout(&check_if_disconnected);
        snd_stop_stream();

        if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
            sc_disconnect();
        else
            ic_disconnect();
    }
    else
        print_info("Connecting canceled\n", 0);

}


void button_record_cb()
{
    int i;
    int rc = 0;
    int cancel = 0;
    static int initial_call = 1;
    char mode[3];
    char i_str[10];
    bool index = 0;
    char *path_with_placeholder;
    char *path_for_index_loop;
    char *path_without_split_time;
    char *ext;
    FILE *fd;
    



    if(recording)
    {
        rc = fl_choice("stop recording?", "NO", "YES", NULL);
        if(rc == 0)//if NO pressed
            return;

        fl_g->input_rec_split_time->activate();
        fl_g->text_rec_split_time->activate();

        Fl::remove_timeout(&check_split_time);
        snd_stop_rec();
        if(!connected)
        {
            fl_g->lcd->clear();
            fl_g->lcd->print((const uchar*)"info: idle", 10);
            Fl::remove_timeout(&check_time);
        }
        else
            display_info = STREAM_TIME;

        return;
    }

    if(strlen(cfg.rec.filename) == 0)
    {
        fl_alert("No recording filename specified");
        return;
    }

    //Don't allow recording when ogg or wav is selected as record codec
    //only lame (mp3) is able to convert the samplerate
    if(((fl_g->radio_rec_codec_wav->value() || (fl_g->radio_rec_codec_ogg->value())) &&
       fl_g->choice_rec_samplerate->value() !=  fl_g->choice_cfg_samplerate->value()))
    {
        fl_alert("Stream and Record samplerate do not match\nRecording canceled");
        return;
    }


    cfg.rec.path = (char*) malloc((strlen(cfg.rec.folder) +
                strlen(cfg.rec.filename)) * sizeof(char) + 10);


    strcpy(cfg.rec.path, cfg.rec.folder);
    strcat(cfg.rec.path, cfg.rec.filename);

    //expand string like file_%d_%m_%y to file_05_11_2014
    expand_string(&cfg.rec.path);

    //check if there is an index marker in the filename
    if(strstr(cfg.rec.filename, "%i"))
	{
		index = 1;

        path_with_placeholder = strdup(cfg.rec.path);
        path_for_index_loop = strdup(cfg.rec.path);

		strrpl(&cfg.rec.path, (char*)"%i", (char*)"0");
	}
    path_without_split_time = strdup(cfg.rec.path);

    if(cfg.rec.split_time > 0)
    {
        ext = util_get_file_extension(cfg.rec.filename);
        if(ext == NULL)
        {
            print_info("Could not find a file extension (mp3/ogg/wav) in current filename\n"
                    "Automatic file splitting is deactivated", 0);
        }
        else
        {
            if(index == 1)
            {
                free(path_for_index_loop);
                path_for_index_loop = strdup(path_with_placeholder);
                strinsrt(&path_for_index_loop, (char*)"-1", strrstr(path_for_index_loop, ext)-1);
            }
            strinsrt(&cfg.rec.path, (char*)"-1", strrstr(cfg.rec.path, ext)-1);
        }
    }

    

    //check if the file already exists
if((fd = fl_fopen(cfg.rec.path, "rb")) != NULL)
    {
        fclose(fd);

        if(index)
        {
	    //increment the index until we find a filename that doesn't exist yet
            for(i = 1; ; i++)
            {
                free(cfg.rec.path);
                cfg.rec.path = strdup(path_for_index_loop);
                snprintf(i_str, 10, "%d", i);
                strrpl(&cfg.rec.path, (char*)"%i", i_str);

                
                path_without_split_time = strdup(path_with_placeholder);
                strrpl(&path_without_split_time, (char*)"%i", i_str);

                if((fd = fl_fopen(cfg.rec.path, "rb")) == NULL)
                    break;

                fclose(fd);
            }
            free(path_for_index_loop);
            strcpy(mode, "wb");
        }
        else
        {
            rc = fl_choice("%s already exists!",
                    "cancel", "overwrite", "append", cfg.rec.path);
            switch(rc)
            {
                case 0:                   //cancel pressed
                    cancel = 1;
                    break;
                case 1:                   //overwrite pressed
                    strcpy(mode, "wb");
                    break;
                case 2:                   //append pressed
                    strcpy(mode, "ab");
            }
        }
    }
    else //selected file doesn't exist yet
	{
        strcpy(mode, "wb");
	}

    if(cancel == 1)
        return;

    if((cfg.rec.fd = fl_fopen(cfg.rec.path, mode)) == NULL)
    {
        fl_alert("Could not open:\n%s", cfg.rec.path);
        return;
    }

    record = 1;
    timer_init(&rec_timer, 1);

    fl_g->input_rec_split_time->deactivate();
    fl_g->text_rec_split_time->deactivate();

    //create the recording thread
    snd_start_rec();

    if (cfg.rec.split_time > 0)
    {
        free(cfg.rec.path);
        cfg.rec.path = strdup(path_without_split_time);
        Fl::add_timeout(60*cfg.rec.split_time, &check_split_time, &initial_call);

    }

    if(!connected)
    {
        display_info = REC_TIME;
        Fl::add_timeout(0.1, &check_time);
    }
}

void button_info_cb() //changed "Info" text to "More"
{
    if (!fl_g->info_visible)
    {
        // Show info output...
        fl_g->window_main->resize(fl_g->window_main->x(),
                                  fl_g->window_main->y(),
                                  fl_g->window_main->w(),
                                  fl_g->info_output->y() + 205);
        fl_g->info_output->show();
        fl_g->button_info->label("Less @2<");
        fl_g->info_visible = 1;
    }
    else
    {
        // Hide info output...
        fl_g->window_main->resize(fl_g->window_main->x(),
                                  fl_g->window_main->y(),
                                  fl_g->window_main->w(),
                                  fl_g->info_output->y() - 30);
        fl_g->info_output->hide();
        fl_g->button_info->label("More @2>");
        fl_g->info_visible = 0;
    }
}

void choice_cfg_act_srv_cb()
{
    cfg.selected_srv = fl_g->choice_cfg_act_srv->value();

    cfg.main.srv = (char*)realloc(cfg.main.srv,
                                  strlen(cfg.srv[cfg.selected_srv]->name)+1);

    strcpy(cfg.main.srv, cfg.srv[cfg.selected_srv]->name);

    unsaved_changes = 1;
}

void choice_cfg_act_icy_cb()
{
    cfg.selected_icy = fl_g->choice_cfg_act_icy->value();

    cfg.main.icy = (char*)realloc(cfg.main.icy,
                                  strlen(cfg.icy[cfg.selected_icy]->name)+1);

    strcpy(cfg.main.icy, cfg.icy[cfg.selected_icy]->name);

    unsaved_changes = 1;
}

void button_cfg_add_srv_cb()
{
    fl_g->window_add_srv->label("Add Server");
    fl_g->radio_add_srv_shoutcast->setonly();
    fl_g->input_add_srv_mount->deactivate();
    fl_g->input_add_srv_usr->deactivate();

    fl_g->input_add_srv_pwd->input_type(FL_SECRET_INPUT);
    fl_g->input_add_srv_pwd->redraw();
    fl_g->button_cfg_show_pw->label("Show Password");

    fl_g->button_add_srv_save->hide();
    fl_g->button_add_srv_add->show();

    fl_g->window_add_srv->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());
    fl_g->input_add_srv_name->take_focus();
    fl_g->window_add_srv->show();
}

void button_cfg_edit_srv_cb()
{

    char dummy[10];
    int srv;

    if(cfg.main.num_of_srv < 1)
        return;

    fl_g->window_add_srv->label("Edit Server");

    srv = fl_g->choice_cfg_act_srv->value();

    fl_g->input_add_srv_name->value(cfg.srv[srv]->name);
    fl_g->input_add_srv_addr->value(cfg.srv[srv]->addr);

    snprintf(dummy, 6, "%u", cfg.srv[srv]->port);
    fl_g->input_add_srv_port->value(dummy);
    fl_g->input_add_srv_pwd->value(cfg.srv[srv]->pwd);

    fl_g->input_add_srv_pwd->input_type(FL_SECRET_INPUT);
    fl_g->input_add_srv_pwd->redraw();
    fl_g->button_cfg_show_pw->label("Show Password");


    if(cfg.srv[srv]->type == SHOUTCAST)
    {
        fl_g->input_add_srv_mount->value("");
        fl_g->input_add_srv_mount->deactivate();
        fl_g->input_add_srv_usr->value("");
        fl_g->input_add_srv_usr->deactivate();
        fl_g->radio_add_srv_shoutcast->setonly();
    }
    else //if(cfg.srv[srv]->type == ICECAST)
    {
        fl_g->input_add_srv_mount->value(cfg.srv[srv]->mount);
        fl_g->input_add_srv_mount->activate();
        fl_g->input_add_srv_usr->value(cfg.srv[srv]->usr);
        fl_g->input_add_srv_usr->activate();
        fl_g->radio_add_srv_icecast->setonly();
    }


    fl_g->input_add_srv_name->take_focus();

    fl_g->button_add_srv_add->hide();
    fl_g->button_add_srv_save->show();

    fl_g->window_add_srv->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());
    fl_g->window_add_srv->show();
}


void button_cfg_add_icy_cb()
{
    fl_g->window_add_icy->label("Add Server Infos");

    fl_g->button_add_icy_save->hide();
    fl_g->button_add_icy_add->show();
    fl_g->window_add_icy->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());

    //give the "name" input field the input focus
    fl_g->input_add_icy_name->take_focus();

    fl_g->window_add_icy->show();
}

void button_cfg_song_go_cb()
{
    char text_buf[256];

    int (*xc_update_song)() = NULL;

    if(!connected || cfg.main.song == NULL)
        return;

    if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
        xc_update_song = &sc_update_song;

    if(cfg.srv[cfg.selected_srv]->type == ICECAST)
        xc_update_song = &ic_update_song;

    if(xc_update_song() == 0)
    {
        snprintf(text_buf, sizeof(text_buf),
                "Updated songname to:\n%s\n",
                cfg.main.song);

        print_info(text_buf, 0);
    }
    else
        print_info("Updating songname failed", 1);
}


void input_cfg_song_cb()
{
     cfg.main.song =
        (char*)realloc(cfg.main.song,
                  strlen(fl_g->input_cfg_song->value())+1 );

    strcpy(cfg.main.song, fl_g->input_cfg_song->value());

    unsaved_changes = 1;
}

void radio_add_srv_shoutcast_cb()
{
    fl_g->input_add_srv_mount->deactivate();
    fl_g->input_add_srv_usr->deactivate();
}

void radio_add_srv_icecast_cb()
{
    fl_g->input_add_srv_mount->activate();
    fl_g->input_add_srv_usr->activate();

    fl_g->input_add_srv_mount->value("stream");
    fl_g->input_add_srv_usr->value("source");
}

void button_add_srv_show_pwd_cb()
{
    if(fl_g->input_add_srv_pwd->input_type() == FL_SECRET_INPUT)
    {
        fl_g->input_add_srv_pwd->input_type(FL_NORMAL_INPUT);
        fl_g->input_add_srv_pwd->redraw();
        fl_g->button_cfg_show_pw->label("Hide Password");
    }	
    else
    {
        fl_g->input_add_srv_pwd->input_type(FL_SECRET_INPUT);
        fl_g->input_add_srv_pwd->redraw();
        fl_g->button_cfg_show_pw->label("Show Password");
    }
}


// edit server
void button_add_srv_save_cb()
{
    int i;

    if(cfg.main.num_of_srv < 1)
        return;

    int srv_num = fl_g->choice_cfg_act_srv->value();
    int len = 0;

    //error checking
    if((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_mount->value()) == 0))
    {
        fl_alert("No mountpoint specified\nSetting mountpoint to \"stream\"");
        fl_g->input_add_srv_mount->value("stream");
    }
    if((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_usr->value()) == 0))
    {
        fl_alert("No user specified\nSetting user to \"source\"");
        fl_g->input_add_srv_usr->value("source");
    }
    if(strlen(fl_g->input_add_srv_name->value()) == 0)
    {
        fl_alert("No name specified");
        return;
    }
    if(cfg.main.srv_ent != NULL)
    {
        if(strlen(fl_g->input_add_srv_name->value()) + strlen(cfg.main.srv_ent) > 1000)
        {
            fl_alert("The sum of characters of all your server names exeeds 1000\n"
                    "Please reduce the count of characters of each server name");
            return;
        }
    }
    if(strpbrk(fl_g->input_add_srv_name->value(), ";\\/\n\r") != NULL)
    {
        fl_alert("No newline characters and ;/\\ are allowed in the name field");
        return;
    }
    if(strlen(fl_g->input_add_srv_addr->value()) == 0)
    {
        fl_alert("No address specified");
        return;
    }
    if(strlen(fl_g->input_add_srv_pwd->value()) == 0)
    {
        fl_alert("No password specified");
        return;
    }
    if(strlen(fl_g->input_add_srv_port->value()) == 0)
    {
        fl_alert("No port specified");
        return;
    }
    else if(( atoi(fl_g->input_add_srv_port->value()) > 65535) ||
            (atoi(fl_g->input_add_srv_port->value()) < 1) )
    {
        fl_alert("Invalid port number\nThe port number must be between 1 and 65535");
        return;
    }

    //check if the name already exists
    for(i = 0; i < cfg.main.num_of_srv; i++)
    {
        if(i == srv_num) //don't check name against it self
            continue;
        if(!strcmp(fl_g->input_add_srv_name->value(), cfg.srv[i]->name))
        {
            fl_alert("Server name already exist!");
            return;
        }
    }

    
    //update current server name
    cfg.srv[srv_num]->name =
        (char*) realloc(cfg.srv[srv_num]->name,
                sizeof(char) * strlen(fl_g->input_add_srv_name->value())+1);

    strcpy(cfg.srv[srv_num]->name, fl_g->input_add_srv_name->value());

    //rewrite the string that contains all server names
    //first get the needed memory space
    for(int i = 0; i < cfg.main.num_of_srv; i++)
        len += strlen(cfg.srv[i]->name) + 1;
    //reserve enough memory
    cfg.main.srv_ent = (char*) realloc(cfg.main.srv_ent, sizeof(char)*len +1);

    memset(cfg.main.srv_ent, 0, len);
    //now append the server strings
    for(int i = 0; i < cfg.main.num_of_srv; i++)
    {
        strcat(cfg.main.srv_ent, cfg.srv[i]->name);
        if(i < cfg.main.num_of_srv-1)
            strcat(cfg.main.srv_ent, ";");
    }

    //update current server address
    cfg.srv[srv_num]->addr =
        (char*) realloc(cfg.srv[srv_num]->addr,
                sizeof(char) * strlen(fl_g->input_add_srv_addr->value())+1);

    strcpy(cfg.srv[srv_num]->addr, fl_g->input_add_srv_addr->value());
    
    //strip leading http:// from addr
    if(strstr(cfg.srv[srv_num]->addr, "http://"))
        cfg.srv[srv_num]->addr += strlen("http://");

    //update current server port
    cfg.srv[srv_num]->port = (unsigned int)atoi(fl_g->input_add_srv_port->value());

    //update current server password
    cfg.srv[srv_num]->pwd =
        (char*) realloc(cfg.srv[srv_num]->pwd,
                    strlen(fl_g->input_add_srv_pwd->value())+1);

    strcpy(cfg.srv[srv_num]->pwd, fl_g->input_add_srv_pwd->value());

    //update current server type
    if(fl_g->radio_add_srv_shoutcast->value())
        cfg.srv[srv_num]->type = SHOUTCAST;
    if(fl_g->radio_add_srv_icecast->value())
        cfg.srv[srv_num]->type = ICECAST;

    //update current server mountpoint and user
    if(cfg.srv[srv_num]->type == ICECAST)
    {
        cfg.srv[srv_num]->mount =
            (char*) realloc(cfg.srv[srv_num]->mount,
                    sizeof(char) * strlen(fl_g->input_add_srv_mount->value())+1);
        strcpy(cfg.srv[srv_num]->mount, fl_g->input_add_srv_mount->value());

        cfg.srv[srv_num]->usr =
            (char*) realloc(cfg.srv[srv_num]->usr,
                    sizeof(char) * strlen(fl_g->input_add_srv_usr->value())+1);
        strcpy(cfg.srv[srv_num]->usr, fl_g->input_add_srv_usr->value());

    }

    fl_g->choice_cfg_act_srv->replace(srv_num, cfg.srv[srv_num]->name);
    fl_g->choice_cfg_act_srv->redraw();

    //reset the input fields and hide the window
    fl_g->input_add_srv_name->value("");
    fl_g->input_add_srv_addr->value("");
    fl_g->input_add_srv_port->value("");
    fl_g->input_add_srv_pwd->value("");
    fl_g->input_add_srv_mount->value("");
    fl_g->input_add_srv_usr->value("");
    fl_g->window_add_srv->hide();

    choice_cfg_act_srv_cb();

    unsaved_changes = 1;
}


void button_add_icy_save_cb()
{
    int i;
    
    if(cfg.main.num_of_icy < 1)
        return;

    int icy_num = fl_g->choice_cfg_act_icy->value();
    int len = 0;
  
    if(strlen(fl_g->input_add_icy_name->value()) == 0)
    {
        fl_alert("No name specified");
        return;
    }
    if(cfg.main.icy_ent != NULL)
    {
        if(strlen(fl_g->input_add_icy_name->value()) + strlen(cfg.main.icy_ent) > 1000)
        {
            fl_alert("The sum of characters of all your icy names exeeds 1000\n"
                    "Please reduce the count of characters of each icy name");
            return;
        }
    }
    if(strpbrk(fl_g->input_add_icy_name->value(), ";\\/\n\r") != NULL)
    {
        fl_alert("No newline characters and ;/\\ are allowed in the name field");
        return;
    }
    
    //check if the name already exists
    for(i = 0; i < cfg.main.num_of_icy; i++)
    {
        if(i == icy_num) //don't check name against it self
            continue;
        if(!strcmp(fl_g->input_add_icy_name->value(), cfg.icy[i]->name))
        {
            fl_alert("Icy name already exist!");
            return;
        }
    }

    //update current icy name
    cfg.icy[icy_num]->name =
        (char*) realloc(cfg.icy[icy_num]->name,
                sizeof(char) * strlen(fl_g->input_add_icy_name->value())+1);

    strcpy(cfg.icy[icy_num]->name, fl_g->input_add_icy_name->value());

    //rewrite the string that contains all server names
    //first get the needed memory space
    for(int i = 0; i < cfg.main.num_of_icy; i++)
        len += strlen(cfg.icy[i]->name) + 1;
    //reserve enough memory
    cfg.main.icy_ent = (char*) realloc(cfg.main.icy_ent, sizeof(char)*len +1);

    memset(cfg.main.icy_ent, 0, len);
    //now append the server strings
    for(int i = 0; i < cfg.main.num_of_icy; i++)
    {
        strcat(cfg.main.icy_ent, cfg.icy[i]->name);
        if(i < cfg.main.num_of_icy-1)
            strcat(cfg.main.icy_ent, ";");
    }

    cfg.icy[icy_num]->desc =
        (char*)realloc(cfg.icy[icy_num]->desc,
                  strlen(fl_g->input_add_icy_desc->value())+1 );
    strcpy(cfg.icy[icy_num]->desc, fl_g->input_add_icy_desc->value());

    cfg.icy[icy_num]->genre =
        (char*)realloc(cfg.icy[icy_num]->genre,
                  strlen(fl_g->input_add_icy_genre->value())+1 );
    strcpy(cfg.icy[icy_num]->genre, fl_g->input_add_icy_genre->value());

    cfg.icy[icy_num]->url =
        (char*)realloc(cfg.icy[icy_num]->url,
                  strlen(fl_g->input_add_icy_url->value())+1 );
    strcpy(cfg.icy[icy_num]->url, fl_g->input_add_icy_url->value());

    cfg.icy[icy_num]->icq =
        (char*)realloc(cfg.icy[icy_num]->icq,
                  strlen(fl_g->input_add_icy_icq->value())+1 );
    strcpy(cfg.icy[icy_num]->icq, fl_g->input_add_icy_icq->value());

    cfg.icy[icy_num]->irc =
        (char*)realloc(cfg.icy[icy_num]->irc,
                  strlen(fl_g->input_add_icy_irc->value())+1 );
    strcpy(cfg.icy[icy_num]->irc, fl_g->input_add_icy_irc->value());

    cfg.icy[icy_num]->aim =
        (char*)realloc(cfg.icy[icy_num]->aim,
                  strlen(fl_g->input_add_icy_aim->value())+1 );
    strcpy(cfg.icy[icy_num]->aim, fl_g->input_add_icy_aim->value());

    sprintf(cfg.icy[icy_num]->pub, "%d", fl_g->check_add_icy_pub->value());

    fl_g->input_add_icy_name->value("");
    fl_g->input_add_icy_desc->value("");
    fl_g->input_add_icy_url->value("");
    fl_g->input_add_icy_genre->value("");
    fl_g->input_add_icy_irc->value("");
    fl_g->input_add_icy_icq->value("");
    fl_g->input_add_icy_aim->value("");
    fl_g->check_add_icy_pub->value(0);

    fl_g->window_add_icy->hide();


    fl_g->choice_cfg_act_icy->replace(icy_num, cfg.icy[icy_num]->name);
    fl_g->choice_cfg_act_icy->redraw();
    choice_cfg_act_icy_cb();

    unsaved_changes = 1;
}

/*
void choice_cfg_edit_srv_cb()
{
    char dummy[10];
    int server = fl_g->choice_cfg_edit_srv->value();

    fl_g->input_cfg_addr->value(cfg.srv[server]->addr);

    snprintf(dummy, 6, "%u", cfg.srv[server]->port);
    fl_g->input_cfg_port->value(dummy);
    fl_g->input_cfg_passwd->value(cfg.srv[server]->pwd);

    if(cfg.srv[server]->type == SHOUTCAST)
    {
        fl_g->input_cfg_mount->value("");
        fl_g->input_cfg_mount->deactivate();
        fl_g->radio_cfg_shoutcast->value(1);
        fl_g->radio_cfg_icecast->value(0);
    }
    else //if(cfg.srv[server]->type == ICECAST)
    {
        fl_g->input_cfg_mount->value(cfg.srv[server]->mount);
        fl_g->input_cfg_mount->activate();
        fl_g->radio_cfg_icecast->value(1);
        fl_g->radio_cfg_shoutcast->value(0);
    }
}
*/

void choice_cfg_bitrate_cb()
{
    int rc;
    int old_br;
    int sel_br;
    int br_list[] = { 8, 16, 24, 32, 40, 48, 56, 64, 80, 96,
                      112, 128, 160, 192, 224, 256, 320 };
    char text_buf[256];

    old_br = cfg.audio.bitrate;
    for(int i = 0; i < 14; i++)
        if(br_list[i] == cfg.audio.bitrate)
            old_br = i;

    sel_br = fl_g->choice_cfg_bitrate->value();
    cfg.audio.bitrate = br_list[sel_br];
    lame_stream.bitrate = br_list[sel_br];
    vorbis_stream.bitrate = br_list[sel_br];

    if(fl_g->radio_cfg_codec_mp3->value())
    {
        rc = lame_enc_reinit(&lame_stream);
        if(rc != 0)
        {
            print_info("Warning:\nThe stream Sample-/Bitrate combination is invalid", 1);
            fl_g->choice_cfg_bitrate->value(old_br);
            fl_g->choice_cfg_bitrate->redraw();
            cfg.audio.bitrate = br_list[old_br];
            lame_stream.bitrate = br_list[old_br];
            lame_enc_reinit(&lame_stream);
            print_info("The previous values have been set\n", 1);
            return;
        }
    }
    if(fl_g->radio_cfg_codec_ogg->value())
    {
        rc = vorbis_enc_reinit(&vorbis_stream);
        if(rc != 0)
        {
            print_info("Warning:\nThe stream Sample-/Bitrate combination is invalid", 1);
            cfg.audio.bitrate = br_list[old_br];
            vorbis_stream.bitrate = br_list[old_br];
            fl_g->choice_cfg_bitrate->value(old_br);
            fl_g->choice_cfg_bitrate->redraw();
            vorbis_enc_reinit(&vorbis_stream);
            print_info("The previous values have been set\n", 1);
            return;
        }
    }

    snprintf(text_buf, sizeof(text_buf), "Stream bitrate set to: %dk", cfg.audio.bitrate);
    print_info(text_buf, 0);

    unsaved_changes = 1;
}

void choice_rec_bitrate_cb()
{
    int rc;
    int old_br;
    int sel_br;
    int br_list[] = { 8, 16, 24, 32, 40, 48, 56, 64, 80, 96,
                      112, 128, 160, 192, 224, 256, 320 };
    char text_buf[256];

    old_br = cfg.rec.bitrate;
    for(int i = 0; i < 14; i++)
        if(br_list[i] == cfg.rec.bitrate)
            old_br = i;


    sel_br = fl_g->choice_rec_bitrate->value();
    cfg.rec.bitrate = br_list[sel_br];
    lame_rec.bitrate = br_list[sel_br];
    vorbis_rec.bitrate = br_list[sel_br];

    if(fl_g->radio_rec_codec_mp3->value())
    {
        rc = lame_enc_reinit(&lame_rec);
        if(rc != 0)
        {
            print_info("Warning:\nThe record Sample-/Bitrate combination is invalid", 1);
            fl_g->choice_rec_bitrate->value(old_br);
            fl_g->choice_rec_bitrate->redraw();
            cfg.rec.bitrate = br_list[old_br];
            lame_rec.bitrate = br_list[old_br];
            lame_enc_reinit(&lame_rec);
            print_info("The previous values have been set", 1);
            return;
        }
    }
    if(fl_g->radio_rec_codec_ogg->value())
    {
        rc = vorbis_enc_reinit(&vorbis_rec);
        if(rc != 0)
        {
            print_info("Warning:\nThe record Sample-/Bitrate combination is invalid", 1);
            cfg.rec.bitrate = br_list[old_br];
            vorbis_rec.bitrate = br_list[old_br];
            fl_g->choice_rec_bitrate->value(old_br);
            fl_g->choice_rec_bitrate->redraw();
            vorbis_enc_reinit(&vorbis_rec);
            print_info("The previous values have been set", 1);
            return;
        }
    }

    snprintf(text_buf, sizeof(text_buf), "Record bitrate set to: %dk", cfg.rec.bitrate);
    print_info(text_buf, 0);

    unsaved_changes = 1;
}

void choice_cfg_samplerate_cb() 
{
    int rc;
    int old_sr;
    int old_rec_sr;
    int sel_sr;
    int *sr_list;
    char text_buf[256];

    sr_list = cfg.audio.pcm_list[cfg.audio.dev_num]->sr_list;

    old_sr = cfg.audio.samplerate;
    old_rec_sr = lame_rec.samplerate_in;

    for(int i = 0; i < 9; i++)
        if(sr_list[i] == cfg.audio.samplerate)
            old_sr = i;

    sel_sr = fl_g->choice_cfg_samplerate->value();

    cfg.audio.samplerate = sr_list[sel_sr];
    lame_rec.samplerate_in = sr_list[sel_sr];
    lame_stream.samplerate_in = sr_list[sel_sr];
    lame_stream.samplerate_out = sr_list[sel_sr];
    vorbis_stream.samplerate = sr_list[sel_sr]; 


    if(fl_g->radio_cfg_codec_mp3->value())
    {
        rc = lame_enc_reinit(&lame_stream);
        if(rc != 0)
        {
            print_info("Warning:\nThe stream Sample-/Bitrate combination is invalid", 1);
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            cfg.audio.samplerate = sr_list[old_sr];
            lame_stream.samplerate_in = sr_list[old_sr];
            lame_stream.samplerate_out = sr_list[old_sr];
            lame_rec.samplerate_in = old_rec_sr;
            lame_enc_reinit(&lame_stream);
            print_info("The previous values have been set", 1);
            return;
        }
        rc = lame_enc_reinit(&lame_rec);
        if(rc != 0)
        {
            print_info("Warning:\nThe stream Sample-/Bitrate combination is invalid", 1);
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            cfg.audio.samplerate = sr_list[old_sr];
            lame_stream.samplerate_in = sr_list[old_sr];
            lame_stream.samplerate_out = sr_list[old_sr];
            lame_rec.samplerate_in = old_rec_sr;
            lame_enc_reinit(&lame_stream);
            print_info("The previous values have been set", 1);
            return;
        }
    }
    if(fl_g->radio_cfg_codec_ogg->value())
    {
        rc = vorbis_enc_reinit(&vorbis_stream);
        if(rc != 0)
        {
            print_info("Warning:\nThe stream Sample-/Bitrate combination is invalid", 1);
            cfg.audio.samplerate = sr_list[old_sr];
            vorbis_stream.samplerate = sr_list[old_sr]; 
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            vorbis_enc_reinit(&vorbis_stream);
            print_info("The previous values have been set", 1);
            return;
        }
    }
    snprintf(text_buf, sizeof(text_buf), "Stream samplerate set to: %dHz", cfg.audio.samplerate);
    print_info(text_buf, 0);

    //if wav or ogg is selected as record output
    //then the audio/record samplerate are always the same
    if(((fl_g->radio_rec_codec_wav->value() || (fl_g->radio_rec_codec_ogg->value())) &&
       fl_g->choice_rec_samplerate->value(sel_sr) !=  fl_g->choice_cfg_samplerate->value(sel_sr)))
    {
        print_info("You can only have different samplerates when recording in mp3\n"
                "Trying to set the record samplerate to the same value as the stream samplerate...", 0);

        fl_g->choice_rec_samplerate->value(sel_sr);
        choice_rec_samplerate_cb();
    }


    //reinit portaudio
    snd_reinit();


    unsaved_changes = 1;
}

void choice_rec_samplerate_cb()
{
    int rc;
    int old_sr;
    int sel_sr;
    int *sr_list;
    char text_buf[256];

    sr_list = cfg.audio.pcm_list[cfg.audio.dev_num]->sr_list;
    sel_sr = fl_g->choice_rec_samplerate->value();
    old_sr = cfg.rec.samplerate;
    for(int i = 0; i < 9; i++)
        if(sr_list[i] == cfg.rec.samplerate)
            old_sr = i;

    cfg.rec.samplerate = sr_list[sel_sr];
    lame_rec.samplerate_out = sr_list[sel_sr];
    vorbis_rec.samplerate = sr_list[sel_sr];


    if(fl_g->radio_rec_codec_mp3->value())
    {
        rc = lame_enc_reinit(&lame_rec);
        if(rc != 0)
        {
            print_info("Warning:\nThe record Sample-/Bitrate combination is invalid", 1);
            fl_g->choice_rec_samplerate->value(old_sr);
            fl_g->choice_rec_samplerate->redraw();
            cfg.rec.samplerate = sr_list[old_sr];
            lame_rec.samplerate_out = sr_list[old_sr];
            lame_enc_reinit(&lame_rec);
            print_info("The previous values have been set", 1);
            return;
        }
    }
    if(fl_g->radio_rec_codec_ogg->value())
    {
        rc = vorbis_enc_reinit(&vorbis_rec);
        if(rc != 0)
        {
            print_info("Warning:\nThe record Sample-/Bitrate combination is invalid", 1);
            fl_g->choice_rec_samplerate->value(old_sr);
            fl_g->choice_rec_samplerate->redraw();
            cfg.rec.samplerate = sr_list[old_sr];
            vorbis_rec.samplerate = sr_list[old_sr];
            vorbis_enc_reinit(&vorbis_rec);
            print_info("The previous values have been set", 1);
            return;
        }
    }

    snprintf(text_buf, sizeof(text_buf), "Record samplerate set to: %dHz", cfg.rec.samplerate);
    print_info(text_buf, 0);

    //if wav or ogg is selected as record output
    //then the audio/record samplerate are always the same
    if(((fl_g->radio_rec_codec_wav->value() || (fl_g->radio_rec_codec_ogg->value())) &&
       fl_g->choice_rec_samplerate->value(sel_sr) !=  fl_g->choice_cfg_samplerate->value(sel_sr)))
    {
        print_info("You can only have different samplerates when recording in mp3\n"
                "Trying to set the stream samplerate to the same value as the record samplerate...", 0);
        fl_g->choice_cfg_samplerate->value(sel_sr);
        choice_cfg_samplerate_cb();
    }

    unsaved_changes = 1;
}

void radio_cfg_channel_stereo_cb()
{

    cfg.audio.channel = 2;
    lame_stream.channel = 2;
    vorbis_stream.channel = 2;
 
    if(fl_g->radio_cfg_codec_mp3->value())
        lame_enc_reinit(&lame_stream);
    if(fl_g->radio_cfg_codec_ogg->value())
        vorbis_enc_reinit(&vorbis_stream);

    snd_reinit();
    
    print_info("Stream channels set to: stereo", 0);

   
    //if wav is selected as record output
    //then the audio/record channel are always the same
    if(fl_g->radio_rec_codec_wav->value() && cfg.rec.channel != 2)
    {
        fl_g->radio_rec_channel_stereo->setonly();
        radio_rec_channel_stereo_cb();
    }


    unsaved_changes = 1;
}

void radio_rec_channel_stereo_cb()
{
    cfg.rec.channel = 2;
    lame_rec.channel = 2;
    vorbis_rec.channel = 2;
 
    if(fl_g->radio_rec_codec_mp3->value())
        lame_enc_reinit(&lame_rec);
    if(fl_g->radio_rec_codec_ogg->value())
        vorbis_enc_reinit(&vorbis_rec);

    print_info("Record channels set to: stereo", 0);

   
    //if wav is selected as record output
    //then the audio/record channel are always the same
    if(fl_g->radio_rec_codec_wav->value() && cfg.audio.channel != 2)
    {
        fl_g->radio_cfg_channel_stereo->setonly();
        radio_cfg_channel_stereo_cb();
    }

    unsaved_changes = 1;
}

void radio_cfg_channel_mono_cb()
{
    cfg.audio.channel = 1;
    lame_stream.channel = 1;
    vorbis_stream.channel = 1;
    
  
    if(fl_g->radio_cfg_codec_mp3->value())
        lame_enc_reinit(&lame_stream);
    if(fl_g->radio_cfg_codec_ogg->value())
        vorbis_enc_reinit(&vorbis_stream);
    
    snd_reinit();

    print_info("Stream channels set to: mono", 0);
    
    //if wav is selected as record output
    //then the audio/record channel are always the same
    if(fl_g->radio_rec_codec_wav->value() && cfg.rec.channel != 1)
    {
        fl_g->radio_rec_channel_mono->setonly();
        radio_rec_channel_mono_cb();
    }


    unsaved_changes = 1;
}

void radio_rec_channel_mono_cb()
{
    cfg.rec.channel = 1;
    lame_rec.channel = 1;
    vorbis_rec.channel = 1;
    

    if(fl_g->radio_rec_codec_mp3->value())
        lame_enc_reinit(&lame_rec);
    if(fl_g->radio_rec_codec_ogg->value())
        vorbis_enc_reinit(&vorbis_rec);

    print_info("Record channels set to: mono", 0);

    //if wav is selected as record output
    //then the audio/record channel are always the same
    if(fl_g->radio_rec_codec_wav->value() && cfg.audio.channel != 1)
    {
        fl_g->radio_cfg_channel_mono->setonly();
        radio_cfg_channel_mono_cb();
    }

    unsaved_changes = 1;
}

void button_add_srv_cancel_cb()
{
    fl_g->input_add_srv_name->value("");
    fl_g->input_add_srv_addr->value("");
    fl_g->input_add_srv_port->value("");
    fl_g->input_add_srv_pwd->value("");
    fl_g->input_add_srv_mount->value("");
    fl_g->input_add_srv_usr->value("");

    fl_g->window_add_srv->hide();
}

void button_add_icy_add_cb()
{
    int i;
    //error checking
    if(strlen(fl_g->input_add_icy_name->value()) == 0)
    {
        fl_alert("No name specified");
        return;
    }

    if(cfg.main.icy_ent != NULL)
    {
        if(strlen(fl_g->input_add_icy_name->value()) + strlen(cfg.main.icy_ent) > 1000)
        {
            fl_alert("The sum of characters of all your icy names exeeds 1000\n"
                    "Please reduce the count of characters of each icy name");
            return;
        }
    }
    if(strpbrk(fl_g->input_add_icy_name->value(), ";\\/\n\r") != NULL)
    {
        fl_alert("No newline characters and ;/\\ are allowed in the name field");
        return;
    }

    
    //check if the name already exists
    for(i = 0; i < cfg.main.num_of_icy; i++)
    {
        if(!strcmp(fl_g->input_add_icy_name->value(), cfg.icy[i]->name))
        {
            fl_alert("Server name already exist!");
            return;
        }
    }

    i = cfg.main.num_of_icy;
    cfg.main.num_of_icy++;

    cfg.icy = (icy_t**)realloc(cfg.icy, cfg.main.num_of_icy * sizeof(icy_t*));
    cfg.icy[i] = (icy_t*)malloc(sizeof(icy_t));

    cfg.icy[i]->name = (char*)malloc(strlen(fl_g->input_add_icy_name->value())+1 );
    strcpy(cfg.icy[i]->name, fl_g->input_add_icy_name->value());

    cfg.icy[i]->desc = (char*)malloc(strlen(fl_g->input_add_icy_desc->value())+1 );
    strcpy(cfg.icy[i]->desc, fl_g->input_add_icy_desc->value());

    cfg.icy[i]->url = (char*)malloc(strlen(fl_g->input_add_icy_url->value())+1 );
    strcpy(cfg.icy[i]->url, fl_g->input_add_icy_url->value());

    cfg.icy[i]->genre = (char*)malloc(strlen(fl_g->input_add_icy_genre->value())+1 );
    strcpy(cfg.icy[i]->genre, fl_g->input_add_icy_genre->value());

    cfg.icy[i]->irc = (char*)malloc(strlen(fl_g->input_add_icy_irc->value())+1 );
    strcpy(cfg.icy[i]->irc, fl_g->input_add_icy_irc->value());

    cfg.icy[i]->icq = (char*)malloc(strlen(fl_g->input_add_icy_icq->value())+1 );
    strcpy(cfg.icy[i]->icq, fl_g->input_add_icy_icq->value());

    cfg.icy[i]->aim = (char*)malloc(strlen(fl_g->input_add_icy_aim->value())+1 );
    strcpy(cfg.icy[i]->aim, fl_g->input_add_icy_aim->value());

    cfg.icy[i]->pub = (char*)malloc(2 * sizeof(char));
    sprintf(cfg.icy[i]->pub, "%d", fl_g->check_add_icy_pub->value());

    if(cfg.main.num_of_icy > 1)
    {
        cfg.main.icy_ent = (char*)realloc(cfg.main.icy_ent,
                                         strlen(cfg.main.icy_ent) +
                                         strlen(cfg.icy[i]->name) +2);
        sprintf(cfg.main.icy_ent, "%s;%s", cfg.main.icy_ent, cfg.icy[i]->name);
    }
    else
    {
        cfg.main.icy_ent = (char*)malloc(strlen(cfg.icy[i]->name) +1);
        sprintf(cfg.main.icy_ent, "%s", cfg.icy[i]->name);
    }

    cfg.main.icy = (char*)realloc(cfg.main.icy, strlen(cfg.icy[i]->name)+1);

    strcpy(cfg.main.icy, cfg.icy[i]->name);

    fl_g->input_add_icy_name->value("");
    fl_g->input_add_icy_desc->value("");
    fl_g->input_add_icy_url->value("");
    fl_g->input_add_icy_genre->value("");
    fl_g->input_add_icy_irc->value("");
    fl_g->input_add_icy_icq->value("");
    fl_g->input_add_icy_aim->value("");
    fl_g->check_add_icy_pub->value(0);

    fl_g->window_add_icy->hide();

    fl_g->choice_cfg_act_icy->add(cfg.icy[i]->name);
    fl_g->choice_cfg_act_icy->redraw();

    fl_g->button_cfg_edit_icy->activate();
    fl_g->button_cfg_del_icy->activate();
    
    // make added icy data the active icy entry
    fl_g->choice_cfg_act_icy->value(i);
    choice_cfg_act_icy_cb();

    unsaved_changes = 1;
}

void button_add_icy_cancel_cb()
{
    fl_g->input_add_icy_name->value("");
    fl_g->input_add_icy_desc->value("");
    fl_g->input_add_icy_url->value("");
    fl_g->input_add_icy_genre->value("");
    fl_g->input_add_icy_irc->value("");
    fl_g->input_add_icy_icq->value("");
    fl_g->input_add_icy_aim->value("");
    fl_g->check_add_icy_pub->value(0);
    fl_g->window_add_icy->hide();
}

void button_cfg_edit_icy_cb()
{
    if(cfg.main.num_of_icy < 1)
        return;

    int icy = fl_g->choice_cfg_act_icy->value();

    fl_g->window_add_icy->label("Edit Server Infos");

    fl_g->button_add_icy_add->hide();
    fl_g->button_add_icy_save->show();

    fl_g->input_add_icy_name->value(cfg.icy[icy]->name);
    fl_g->input_add_icy_desc->value(cfg.icy[icy]->desc);
    fl_g->input_add_icy_genre->value(cfg.icy[icy]->genre);
    fl_g->input_add_icy_url->value(cfg.icy[icy]->url);
    fl_g->input_add_icy_irc->value(cfg.icy[icy]->irc);
    fl_g->input_add_icy_icq->value(cfg.icy[icy]->icq);
    fl_g->input_add_icy_aim->value(cfg.icy[icy]->aim);

    if(!strcmp(cfg.icy[icy]->pub, "1"))
        fl_g->check_add_icy_pub->value(1);
    else
        fl_g->check_add_icy_pub->value(0);

    fl_g->window_add_icy->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());

    //give the "name" input field the input focus
    fl_g->input_add_icy_name->take_focus();
    fl_g->window_add_icy->show();
}


void choice_cfg_dev_cb()
{
    cfg.audio.dev_num = fl_g->choice_cfg_dev->value();
    update_samplerates();
    snd_reinit();
    unsaved_changes = 1;
}

void radio_cfg_codec_mp3_cb()
{
    if(lame_enc_reinit(&lame_stream) != 0)
    {
        print_info("Lame doesn't support current\n"
                   "Sample-/Bitrate combination", 1);

        fl_g->radio_cfg_codec_mp3->setonly();
        return;
    }
    strcpy(cfg.audio.codec, "mp3");
    print_info("Stream format set to: mp3", 0);
    unsaved_changes = 1;
}

void radio_cfg_codec_ogg_cb()
{
    if(vorbis_enc_reinit(&vorbis_stream) != 0)
    {
        print_info("Vorbis doesn't support current\n"
                   "Sample-/Bitrate combination", 1);

        fl_g->radio_cfg_codec_mp3->setonly();
        return;
    }
    strcpy(cfg.audio.codec, "ogg");
    print_info("Stream format set to: ogg", 0);
    unsaved_changes = 1;
}

void radio_rec_codec_mp3_cb()
{
    if(lame_enc_reinit(&lame_rec) != 0)
    {
        print_info("Lame doesn't support current\n"
                   "Sample-/Bitrate combination", 1);

        //fall back to old rec codec
        if(!strcmp(cfg.rec.codec, "ogg"))
            fl_g->radio_rec_codec_ogg->setonly();
        else 
            fl_g->radio_rec_codec_wav->setonly();
        return;
    }
    strcpy(cfg.rec.codec, "mp3");
    
    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    print_info("Record format set to: mp3", 0);
    fl_g->choice_rec_bitrate->activate();
    

    unsaved_changes = 1;
}

void radio_rec_codec_ogg_cb()
{
    if(vorbis_enc_reinit(&vorbis_rec) != 0)
    {
        print_info("Vorbis doesn't support current\n"
                   "Sample-/Bitrate combination", 1);

        //fall back to old rec codec
        if(!strcmp(cfg.rec.codec, "mp3"))
            fl_g->radio_rec_codec_mp3->setonly();
        else
            fl_g->radio_rec_codec_wav->setonly();
        return;
    }
    strcpy(cfg.rec.codec, "ogg");

    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    print_info("Record format set to: ogg", 0);
    fl_g->choice_rec_bitrate->activate();

    unsaved_changes = 1;
}

void radio_rec_codec_wav_cb()
{
    int audio_sr = fl_g->choice_cfg_samplerate->value();

 
    fl_g->choice_rec_bitrate->deactivate();

    strcpy(cfg.rec.codec, "wav");

    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    print_info("Record format set to: wav", 0);
    
    // match audio sample rate and record sample rate
    if (audio_sr != fl_g->choice_rec_samplerate->value())
    {
      fl_g->choice_rec_samplerate->value(audio_sr);
      choice_rec_samplerate_cb();
    }

    unsaved_changes = 1;
}

void ILM216_cb()
{
    if(Fl::event_button() == 1) //left mouse button
    {
        //change the display mode only when connected or recording
        //this will prevent confusing the user
        if(!connected && !recording)
            return;

        switch(display_info)
        {
            case STREAM_TIME:
                if(recording)
                    display_info = REC_TIME;
                else
                    display_info = SENT_DATA;
                break;

            case REC_TIME:
                if(connected)
                    display_info = SENT_DATA;
                else
                    display_info = REC_DATA;
                break;

            case SENT_DATA:
                if(recording)
                    display_info = REC_DATA;
                else
                    display_info = STREAM_TIME;
                break;

            case REC_DATA:
                if(connected)
                    display_info = STREAM_TIME;
                else
                    display_info = REC_TIME;
        }
    }
   /* if(Fl::event_button() == 3) //right mouse button
    {
        uchar r, g, b;

        Fl_Color bg, txt;
        bg  = (Fl_Color)cfg.main.bg_color;
        txt = (Fl_Color)cfg.main.txt_color;

        //Set the r g b values the color_chooser should start with
        r = (bg & 0xFF000000) >> 24;
        g = (bg & 0x00FF0000) >> 16;
        b = (bg & 0x0000FF00) >>  8;

        fl_color_chooser((const char*)"select background color", r, g, b);

        //The color_chooser changes the r, g, b, values to selected color
        cfg.main.bg_color = fl_rgb_color(r, g, b);

        fl_g->lcd->redraw();

        r = (txt & 0xFF000000) >> 24;
        g = (txt & 0x00FF0000) >> 16;
        b = (txt & 0x0000FF00) >>  8;

        fl_color_chooser((const char*)"select text color", r, g, b);
        cfg.main.txt_color = fl_rgb_color(r, g, b);

        fl_g->lcd->redraw();

        unsaved_changes = 1;
    }*/
}

void button_rec_browse_cb()
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title("Record to...");
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_DIRECTORY);
    nfc.options(Fl_My_Native_File_Chooser::NEW_FOLDER);

    char *bla = strdup(fl_g->input_rec_folder->value());
    bla[strlen(bla)-1] = '\0';
    nfc.directory(fl_g->input_rec_folder->value());
        
    switch(nfc.show())
    {
        case -1: fl_alert("ERROR: %s", nfc.errmsg()); //error
                 break;
        case  1: break; //cancel
        default:
                 fl_g->input_rec_folder->value(nfc.filename());
                 input_rec_folder_cb();
                 unsaved_changes = 1;
                 break;
    }
}

void input_rec_filename_cb()
{
    char *tooltip;

    cfg.rec.filename = (char*)realloc(cfg.rec.filename,
                       strlen(fl_g->input_rec_filename->value())+1);

    strcpy(cfg.rec.filename, fl_g->input_rec_filename->value());
    tooltip = strdup(cfg.rec.filename);

    expand_string(&tooltip);

    fl_g->input_rec_filename->copy_tooltip(tooltip);

    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    unsaved_changes = 1;
    free(tooltip);
}

void input_rec_folder_cb()
{
    int len = strlen(fl_g->input_rec_folder->value());

    cfg.rec.folder = (char*)realloc(cfg.rec.folder, len +2);

    strcpy(cfg.rec.folder, fl_g->input_rec_folder->value());

#ifdef _WIN32    //Replace all "Windows slashes" with "unix slashes"
    char *p;
    p = cfg.rec.folder;
    while(*p != '\0')
    {
        if(*p == '\\')
            *p = '/';
        p++;
    }
#endif

    //Append an '/' if there isn't one
    if(cfg.rec.folder[len-1] != '/')
        strcat(cfg.rec.folder, "/");

    fl_g->input_rec_folder->value(cfg.rec.folder);
    fl_g->input_rec_folder->tooltip(cfg.rec.folder);

    unsaved_changes = 1;
}

void button_cfg_browse_songfile_cb()
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title("Select Songfile");
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_FILE);
    switch(nfc.show())
    {
        case -1: fl_alert("ERROR: %s", nfc.errmsg());
                 break;
        case  1: break; //cancel
        default:
                 fl_g->input_cfg_song_file->value(nfc.filename());
                 input_cfg_song_file_cb();
    }

    unsaved_changes = 1;
}

void input_cfg_song_file_cb()
{
    int len = strlen(fl_g->input_cfg_song_file->value());

    cfg.main.song_path = (char*)realloc(cfg.main.song_path, len +1);

    strcpy(cfg.main.song_path, fl_g->input_cfg_song_file->value());

#ifdef _WIN32    //Replace all "Windows slashes" with "unix slashes"
    char *p;
    p = cfg.main.song_path;
    while(*p != '\0')
    {
        if(*p == '\\')
            *p = '/';
        p++;
    }
#endif

    fl_g->input_cfg_song_file->value(cfg.main.song_path);
    fl_g->input_cfg_song_file->tooltip(cfg.main.song_path);

    unsaved_changes = 1;
}

void check_gui_attach_cb()
{
    if(fl_g->check_gui_attach->value())
    {
        cfg.gui.attach = 1;
        Fl::add_timeout(0.1, &check_cfg_win_pos);
    }
    else
    {
        cfg.gui.attach = 0;
        Fl::remove_timeout(&check_cfg_win_pos);
    }

    unsaved_changes = 1;
}

void check_gui_ontop_cb()
{
    if(fl_g->check_gui_ontop->value())
    {
        fl_g->window_main->stay_on_top(1);
        fl_g->window_cfg->stay_on_top(1);
        cfg.gui.ontop = 1;
    }
    else
    {
        fl_g->window_main->stay_on_top(0);
        fl_g->window_cfg->stay_on_top(0);
        cfg.gui.ontop = 0;
    }

    unsaved_changes = 1;
}

void button_gui_bg_color_cb()
{
    uchar r, g, b;

    Fl_Color bg;
    bg  = (Fl_Color)cfg.main.bg_color;

    //Set the r g b values the color_chooser should start with
    r = (bg & 0xFF000000) >> 24;
    g = (bg & 0x00FF0000) >> 16;
    b = (bg & 0x0000FF00) >>  8;

    fl_color_chooser((const char*)"select background color", r, g, b);

    //The color_chooser changes the r, g, b, values to selected color
    cfg.main.bg_color = fl_rgb_color(r, g, b);

    fl_g->button_gui_bg_color->color(cfg.main.bg_color, fl_lighter((Fl_Color)cfg.main.bg_color));
    fl_g->button_gui_bg_color->redraw();
    fl_g->lcd->redraw();

    unsaved_changes = 1;
}

void button_gui_text_color_cb()
{
    uchar r, g, b;

    Fl_Color txt;
    txt = (Fl_Color)cfg.main.txt_color;

    //Set the r g b values the color_chooser should start with
    r = (txt & 0xFF000000) >> 24;
    g = (txt & 0x00FF0000) >> 16;
    b = (txt & 0x0000FF00) >>  8;

    fl_color_chooser((const char*)"select text color", r, g, b);

    //The color_chooser changes the r, g, b, values to selected color
    cfg.main.txt_color = fl_rgb_color(r, g, b);

    fl_g->button_gui_text_color->color(cfg.main.txt_color, fl_lighter((Fl_Color)cfg.main.txt_color));
    fl_g->button_gui_text_color->redraw();
    fl_g->lcd->redraw();

    unsaved_changes = 1;
}

void check_cfg_rec_cb()
{
    cfg.rec.start_rec = fl_g->check_cfg_rec->value();
    fl_g->lcd->redraw();  //update the little record icon
    unsaved_changes = 1;
}

void check_cfg_connect_cb()
{
    cfg.main.connect_at_startup = fl_g->check_cfg_connect->value();
    unsaved_changes = 1;
}

void check_song_update_active_cb()
{
   if(fl_g->check_song_update_active->value())
   {
       if(connected)
       {
           Fl::add_timeout(0.1, &check_song_update);
           song_timeout_running = 1;
       }
       cfg.main.song_update = 1;
   }
   else
   {
       if(song_timeout_running)
           Fl::remove_timeout(&check_song_update);
       cfg.main.song_update = 0;
   }

   unsaved_changes = 1;
}

void slider_gain_cb()
{
    float gain_db;

    //Without redrawing the main window the slider knob is not redrawn
    fl_g->window_main->redraw();

    gain_db = (float)fl_g->slider_gain->value();

    if((int)gain_db == 0)
        cfg.main.gain = 1;
    else
        cfg.main.gain = util_db_to_factor(gain_db);

    gain_db = (float)fl_g->slider_gain->value();
    fl_g->slider_gain->value_cb2();

    unsaved_changes = 1;
}

void input_rec_split_time_cb()
{
    // Values < 0 are not allowed
    if (fl_g->input_rec_split_time->value() < 0)
    {
        fl_g->input_rec_split_time->value(0);
        return;
    }

    cfg.rec.split_time = fl_g->input_rec_split_time->value();
    
    unsaved_changes = 1;
}

void button_cfg_export_cb()
{
    char *filename;

    Fl_My_Native_File_Chooser nfc;
    nfc.title("Export to...");
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_SAVE_FILE);
    nfc.options(Fl_My_Native_File_Chooser::NEW_FOLDER);

        
    switch(nfc.show())
    {
        case -1: fl_alert("ERROR: %s", nfc.errmsg()); //error
                 return;
                 break;
        case  1: return; // cancel
                 break; 
        default: filename = (char*)nfc.filename();
    }

    cfg_write_file(filename);

}
void button_cfg_import_cb()
{
    char *filename;
    char info_buf[256];

    Fl_My_Native_File_Chooser nfc;
    nfc.title("Import...");
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_FILE);

    switch(nfc.show())
    {
        case -1: fl_alert("ERROR: %s", nfc.errmsg()); //error
                 return;
                 break;
        case  1: return; // cancel
                 break; 
        default: filename = (char*)nfc.filename();
                 break;
    }

    //read config and initialize config struct
    if(cfg_set_values(filename) != 0)     
    {
        snprintf(info_buf, sizeof(info_buf), "Could not import config %s", filename);
        print_info(info_buf, 1);
        return;
    }

    //re-initialize some stuff after config has been successfully imported
    init_main_gui_and_audio();
    fill_cfg_widgets();
    snd_reinit();

    snprintf(info_buf, sizeof(info_buf), "Config imported %s", filename);
    print_info(info_buf, 1);

}
