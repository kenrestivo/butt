// FLTK callback functions for butt
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
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#ifdef WIN32
 #define usleep(us) Sleep(us/1000)
#include "tray_agent.h"
#else
 #include <sys/wait.h>
#endif

#include <FL/fl_ask.H>
#include <FL/Fl_Color_Chooser.H>
#include <samplerate.h>

#include "gettext.h"
#include "config.h"

#include "FL/Fl_My_Native_File_Chooser.H"
//#include <FL/Fl_Native_File_Chooser.H>
//#define Fl_My_Native_File_Chooser Fl_Native_File_Chooser

#include "cfg.h"
#include "butt.h"
#include "port_audio.h"
#include "timer.h"
#include "shoutcast.h"
#include "icecast.h"
#include "lame_encode.h"
#include "opus_encode.h"
#include "fl_callbacks.h"
#include "strfuncs.h"
#include "flgui.h"
#include "util.h"
#include "fl_timer_funcs.h"
#include "fl_funcs.h"
#include "update.h"


flgui *fl_g; 
int display_info = STREAM_TIME;

pthread_t pt_connect;

int ask_user = 0;
void *connect_thread(void *data)
{
    int ret;
    int (*xc_connect)() = NULL;


    if (cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
        xc_connect = &sc_connect;
    else //(cfg.srv[cfg.selected_srv]->type == ICECAST)
        xc_connect = &ic_connect;

    //try to connect as long as xc_connect returns non-zero and try_to_connect == 1
    while ( ((ret = xc_connect()) != IC_OK) && (try_to_connect == 1) ) //xc_connect returns 0 when connected
    {
        // Stop connecting in case of a fatal error
        if (ret == IC_ABORT)
        {
            fl_g->lcd->clear();
            fl_g->lcd->print((const uchar*)_("idle"), strlen(_("idle")));
            fl_g->radio_co_logo->show();
            fl_g->radio_co_logo->redraw();
            break;
        }
        if (ret == IC_ASK)
        {
            ask_user = 1;
            while (ask_user_get_has_clicked() != 1)
                usleep(100000); // 100ms
            
            if (ask_user_get_answer() == IC_ABORT)
            {
                fl_g->lcd->clear();
                fl_g->lcd->print((const uchar*)_("idle"), strlen(_("idle")));
                
                
                fl_g->radio_co_logo->show();
                fl_g->radio_co_logo->redraw();
                ask_user_reset();
                break;
            }
            ask_user_reset();
        }
    
        usleep(100000); // 100ms
    }

    //Connection established, we are not trying to connect anymore
    try_to_connect = 0;

    return NULL;
}

// Print "Connecting..." on the LCD as long as we are trying to connect
void print_connecting_timeout(void *)
{
    static int dummy = 0;

    if (try_to_connect == 1)
    {
        if(dummy == 0)
        {
            print_lcd(_("connecting"), strlen(_("connecting")), 0, 1);
            dummy++;
        }
        else if(dummy <= 3)
        {
            print_lcd(".", 1, 0, 0);
            dummy++;
        }
        else if(dummy > 3)
        {
            dummy = 0;
        }
        else
            dummy++;

        Fl::repeat_timeout(0.25, &print_connecting_timeout);
    }
    else
    {
        dummy = 0;
    }
}

void button_connect_cb(void)
{

    char text_buf[256];

    if(connected)
        return;
    
    if(try_to_connect)
        return;

    if(cfg.main.num_of_srv < 1)
    {
        print_info(_("Error: No server entry found.\nPlease add a server in the settings-window."), 1);
        return;
    }

    if(!strcmp(cfg.audio.codec, "ogg") && (cfg.audio.bitrate < 48))
    {
        print_info(_("Error: ogg vorbis encoder doesn't support bitrates\n"
                    "lower than 48kbit"),1);
        return;
    }
    
    if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
    {
        if( (!strcmp(cfg.audio.codec, "ogg")) || (!strcmp(cfg.audio.codec, "opus")) )
        {
            snprintf(text_buf, sizeof(text_buf), _("Warning: %s is not supported by every ShoutCast version"), cfg.audio.codec);
            print_info(text_buf, 1);
        }
        if(!strcmp(cfg.audio.codec, "flac"))
        {
            print_info(_("Error: FLAC is not supported by ShoutCast"), 1);
            return;
        }
    }


    if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
        snprintf(text_buf, sizeof(text_buf), _("Connecting to %s:%u (%u) ..."),
            cfg.srv[cfg.selected_srv]->addr,
            cfg.srv[cfg.selected_srv]->port+1,
            cfg.srv[cfg.selected_srv]->port);
    else
        snprintf(text_buf, sizeof(text_buf), _("Connecting to %s:%u ..."),
            cfg.srv[cfg.selected_srv]->addr,
            cfg.srv[cfg.selected_srv]->port);

    print_info(text_buf, 0);


    //Clear libsamplerate state
    snd_reset_samplerate_conv(SND_STREAM);
    
    try_to_connect = 1;
    pthread_create(&pt_connect, NULL, connect_thread, NULL);
    Fl::add_timeout(0, &print_connecting_timeout);
    while(try_to_connect)
    {
        if (ask_user == 1)
        {
            ask_user_ask();
            ask_user = 0;
        }
        
        usleep(10000); //10 ms
        Fl::wait(0); //update gui and parse user events
    }
    
    Fl::add_timeout(0.25, &cmd_timer);

    if(!connected)
        return;


    //we have to make sure that the first audio data
    //the server sees are the ogg headers
    if(!strcmp(cfg.audio.codec, "ogg"))
        vorbis_enc_write_header(&vorbis_stream);
    
    if(!strcmp(cfg.audio.codec, "opus"))
        opus_enc_write_header(&opus_stream);
    
    // Reset the internal flac frame counter to zero to
    // make sure that the header is sent to the server upon next connect
    if(!strcmp(cfg.audio.codec, "flac"))
        flac_enc_reinit(&flac_stream);
     


    char bitrate_str[32];
    if (strcmp(cfg.audio.codec, "flac") == 0)
        snprintf(bitrate_str, sizeof(bitrate_str), "-");
    else
        snprintf(bitrate_str, sizeof(bitrate_str), "%dkbps", cfg.audio.bitrate);
        
    
    print_info(_("Connection established"), 0);
    snprintf(text_buf, sizeof(text_buf),
            "Settings:\n"
            "Type:\t\t%s\n"
            "Codec:\t\t%s\n"
            "Bitrate:\t%s\n"
            "Samplerate:\t%dHz\n",
            cfg.srv[cfg.selected_srv]->type == SHOUTCAST ? "ShoutCast" : "IceCast",
            cfg.audio.codec,
            bitrate_str,
            strcmp(cfg.audio.codec, "opus") == 0 ? 48000 : cfg.audio.samplerate
            );


    if(cfg.srv[cfg.selected_srv]->type == ICECAST)
        snprintf(text_buf+strlen(text_buf), sizeof(text_buf)-strlen(text_buf),
                "Mountpoint:\t%s\n"
                "User:\t\t%s\n"
                "SSL/TLS:\t%s\n",
                cfg.srv[cfg.selected_srv]->mount,
                cfg.srv[cfg.selected_srv]->usr,
                cfg.srv[cfg.selected_srv]->tls == 0 ? _("no") : _("yes")
                );

    print_info(text_buf, 0);

    static int called_from_connect_cb = 1;
    if (!cfg.main.song_update && !cfg.main.app_update)
        Fl::add_timeout(cfg.main.song_delay, &update_song, &called_from_connect_cb);

    //the user may not change the audio device while streaming
    fl_g->choice_cfg_dev->deactivate();
    fl_g->button_cfg_rescan_devices->deactivate();
    //the sames applies to the codecs
    fl_g->choice_cfg_codec->deactivate();
    // and the mono to stereo conversion
    fl_g->check_cfg_mono_to_stereo->deactivate();


    //Changing any audio settings while streaming does not work with
    //ogg/vorbis & ogg/opus :(
    if((!strcmp(cfg.audio.codec, "ogg")) || (!strcmp(cfg.audio.codec, "opus")))
    {
        fl_g->choice_cfg_bitrate->deactivate();
        fl_g->choice_cfg_samplerate->deactivate();
        fl_g->choice_cfg_channel->deactivate();
    }
    
    //Changing the sample rate while streaming does not work with aac
    if(!strcmp(cfg.audio.codec, "aac"))
    {
        fl_g->choice_cfg_samplerate->deactivate();
    }

    pa_new_frames = 0;

    //Just in case the record routine started a check_time timeout
    //already
    Fl::remove_timeout(&display_info_timer);

    Fl::add_timeout(0.1, &display_info_timer);
    Fl::add_timeout(0.1, &is_connected_timer);

    if(cfg.main.song_update)
    {
        static int reset = 1;
        Fl::remove_timeout(&songfile_timer);
        Fl::add_timeout(0.5, &songfile_timer, &reset);
    }
    
    if(cfg.main.app_update)
    {
        static int reset = 1;
        current_track_app = getCurrentTrackFunctionFromId(cfg.main.app_update_service);
        Fl::remove_timeout(&app_timer);
        Fl::add_timeout(0.5, &app_timer, &reset);
    }

    if(cfg.main.silence_threshold > 0)
        Fl::add_timeout(1, &stream_silence_timer);


    snd_start_stream();

    if(cfg.rec.start_rec && !recording)
    {
        button_record_cb();
        //timer_init(&rec_timer, 1);
    }

    snprintf(text_buf, sizeof(text_buf), _("Connected to: %s"), cfg.srv[cfg.selected_srv]->name);
    fl_g->window_main->label(text_buf);

    display_info = STREAM_TIME;
    
#ifdef WIN32
    if (tray_agent_is_running(NULL) == 1)
	tray_agent_send_cmd(TA_CONNECT_STATE);
#endif
    
}

void button_cfg_cb(void)
{

    if(fl_g->window_cfg->shown())
    {
        fl_g->window_cfg->hide();
        Fl::remove_timeout(&cfg_win_pos_timer);
    }
    else
    {

/*
 * This is a bit stupid. Well, its Win32...
 * We need to place the cfg window a bit more to the right, otherwise
 * the main and the cfg window would overlap
*/

#ifdef WIN32
    fl_g->window_cfg->position(fl_g->window_main->x() +
                                fl_g->window_main->w()+0,
                                fl_g->window_main->y());
#else
    fl_g->window_cfg->position(fl_g->window_main->x() +
                                fl_g->window_main->w(),
                                fl_g->window_main->y());
#endif
        fl_g->window_cfg->show();
        fill_cfg_widgets();

        if(cfg.gui.attach)
            Fl::add_timeout(0.1, &cfg_win_pos_timer);
    }
}

// add server
void button_add_srv_add_cb(void)
{
    int i;

    //error checking
    if((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_mount->value()) == 0))
    {
        fl_alert(_("No mountpoint specified\nSetting mountpoint to \"stream\""));
        fl_g->input_add_srv_mount->value("stream");
    }
    if((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_usr->value()) == 0))
    {
        fl_alert(_("No user specified\nSetting user to \"source\""));
        fl_g->input_add_srv_usr->value("source");
    }
    if(strlen(fl_g->input_add_srv_name->value()) == 0)
    {
        fl_alert(_("No name specified"));
        return;
    }                               
    if(cfg.main.srv_ent != NULL)
    {
        if(strlen(fl_g->input_add_srv_name->value()) + strlen(cfg.main.srv_ent) > 1000)
        {
            fl_alert(_("The number of characters of all your server names exeeds 1000\n"
                    "Please reduce the number of characters of each server name"));
            return;
        }
    }
    if(strpbrk(fl_g->input_add_srv_name->value(), ";\\/\n\r") != NULL)
    {
        fl_alert(_("No newline characters and ;/\\ are allowed in the name field"));
        return;
    }
    if(strlen(fl_g->input_add_srv_addr->value()) == 0)
    {
        fl_alert(_("No address specified"));
        return;
    }
    if(strlen(fl_g->input_add_srv_pwd->value()) == 0)
    {
        fl_alert(_("No password specified"));
        return;
    }
    if(strlen(fl_g->input_add_srv_port->value()) == 0)
    {
        fl_alert(_("No port specified"));
        return;
    }
    else if((atoi(fl_g->input_add_srv_port->value()) > 65535) ||
            (atoi(fl_g->input_add_srv_port->value()) < 1) )
    {
        fl_alert(_("Invalid port number\nThe port number must be between 1 and 65535"));
        return;

    }
    
    //check if the name already exists
    for(i = 0; i < cfg.main.num_of_srv; i++)
    {
        if(!strcmp(fl_g->input_add_srv_name->value(), cfg.srv[i]->name))
        {
            fl_alert(_("Server name already exist!"));
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
    
    cfg.srv[i]->cert_hash = NULL;

    //strip leading http:// from addr
    strrpl(&cfg.srv[i]->addr, (char*)"http://", (char*)"", MODE_ALL);
    strrpl(&cfg.srv[i]->addr, (char*)"https://", (char*)"", MODE_ALL);

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

    cfg.srv[i]->tls = fl_g->check_add_srv_tls->value();

    if(cfg.main.num_of_srv > 1)
    {
        cfg.main.srv_ent = (char*)realloc(cfg.main.srv_ent,
                                         strlen(cfg.main.srv_ent) +
                                         strlen(cfg.srv[i]->name) +2);
        sprintf(cfg.main.srv_ent, "%s;%s", cfg.main.srv_ent, cfg.srv[i]->name);
        cfg.main.srv = (char*)realloc(cfg.main.srv, strlen(cfg.srv[i]->name)+1);
    }
    else
    {
        cfg.main.srv_ent = (char*)malloc(strlen(cfg.srv[i]->name) +1);
        sprintf(cfg.main.srv_ent, "%s", cfg.srv[i]->name);
        cfg.main.srv = (char*)malloc(strlen(cfg.srv[i]->name)+1);
    }
    strcpy(cfg.main.srv, cfg.srv[i]->name);

    //reset the input fields and hide the window
    fl_g->input_add_srv_name->value("");
    fl_g->input_add_srv_addr->value("");
    fl_g->input_add_srv_port->value("");
    fl_g->input_add_srv_pwd->value("");
    fl_g->input_add_srv_mount->value("");
    fl_g->input_add_srv_usr->value("");
    fl_g->window_add_srv->hide();
    fl_g->check_add_srv_tls->value(0);

    fl_g->choice_cfg_act_srv->add(cfg.srv[i]->name);
    fl_g->choice_cfg_act_srv->redraw();      

    //Activate del and edit buttons
    fl_g->button_cfg_edit_srv->activate();
    fl_g->button_cfg_del_srv->activate();
    
    fl_g->choice_cfg_act_srv->activate();

    // make added server the active server
    fl_g->choice_cfg_act_srv->value(i);
    choice_cfg_act_srv_cb();

}

void button_cfg_del_srv_cb(void)
{
    int i;
    int diff;

    if(cfg.main.num_of_srv == 0)
        return;

    if(cfg.srv[cfg.selected_srv]->name != NULL)
        free(cfg.srv[cfg.selected_srv]->name);

    if(cfg.srv[cfg.selected_srv]->addr != NULL)
        free(cfg.srv[cfg.selected_srv]->addr);

    diff = cfg.main.num_of_srv-1 - cfg.selected_srv;
    for(i = 0; i < diff; i++)
    {
        memcpy(cfg.srv[cfg.selected_srv+i], cfg.srv[cfg.selected_srv+i+1], sizeof(server_t));
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
        fl_g->choice_cfg_act_srv->deactivate();
        free(cfg.main.srv);
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
}

void button_cfg_del_icy_cb(void)
{
    int i;
    int diff;
    icy_t *dst, *src;

    if(cfg.main.num_of_icy == 0)
        return;


    if(cfg.icy[cfg.selected_icy]->name != NULL)
        free(cfg.icy[cfg.selected_icy]->name);

    if(cfg.icy[cfg.selected_icy]->genre != NULL)
        free(cfg.icy[cfg.selected_icy]->genre);

    if(cfg.icy[cfg.selected_icy]->url != NULL)
        free(cfg.icy[cfg.selected_icy]->url);

    if(cfg.icy[cfg.selected_icy]->irc != NULL)
        free(cfg.icy[cfg.selected_icy]->irc);
    
    if(cfg.icy[cfg.selected_icy]->icq != NULL)
        free(cfg.icy[cfg.selected_icy]->icq);

    if(cfg.icy[cfg.selected_icy]->aim != NULL)
        free(cfg.icy[cfg.selected_icy]->aim);

    if(cfg.icy[cfg.selected_icy]->pub != NULL)
        free(cfg.icy[cfg.selected_icy]->pub);

    diff = cfg.main.num_of_icy-1 - cfg.selected_icy;
    for(i = 0; i < diff; i++)
    {
        memcpy(cfg.icy[cfg.selected_icy+i], cfg.icy[cfg.selected_icy+i+1], sizeof(icy_t));
    }

    free(cfg.icy[cfg.main.num_of_icy-1]);

    cfg.main.num_of_icy--;

     //recreate the string that contains all ICY names
    memset(cfg.main.icy_ent, 0, strlen(cfg.main.icy_ent));
    for(i = 0; i < (int)cfg.main.num_of_icy; i++)
    {
        strcat(cfg.main.icy_ent, cfg.icy[i]->name);

        //do not add a trailing seperator ";" to the last entry 
        if(i < (int)cfg.main.num_of_icy-1)
            strcat(cfg.main.icy_ent, ";");
    }


    fl_g->choice_cfg_act_icy->remove(cfg.selected_icy);
    fl_g->choice_cfg_act_icy->redraw();       

    if(cfg.main.num_of_icy == 0)
    {
        fl_g->button_cfg_edit_icy->deactivate();
        fl_g->button_cfg_del_icy->deactivate();
        fl_g->choice_cfg_act_icy->deactivate();
        free(cfg.main.icy);
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
}

void button_disconnect_cb(void)
{
    
    if(!connected && recording)
        stop_recording(true); // true = ask user if recording shall be stopped
    
    if(connected && recording && cfg.rec.stop_rec)
        stop_recording(false); // false = do not ask user

    if(!recording)
    {
        fl_g->lcd->clear();
        fl_g->lcd->print((const uchar*)_("idle"), strlen(_("idle")));
        fl_g->radio_co_logo->show();
        fl_g->radio_co_logo->redraw();
    }

    // We are not trying to connect anymore
    try_to_connect = 0;
    
    if(cfg.main.signal_threshold > 0)
        Fl::add_timeout(1, &stream_signal_timer);

    if(!connected)
        return;

    
    fl_g->choice_cfg_dev->activate();
    fl_g->choice_cfg_codec->activate();
    fl_g->choice_cfg_bitrate->activate();
    fl_g->choice_cfg_samplerate->activate();
    fl_g->choice_cfg_channel->activate();
    fl_g->check_cfg_mono_to_stereo->activate();
    fl_g->button_cfg_rescan_devices->activate();


    if(!recording)
        Fl::remove_timeout(&display_info_timer);
    else
        display_info = REC_TIME;

    Fl::remove_timeout(&songfile_timer);
    Fl::remove_timeout(&app_timer);

    if(connected)
    {
        Fl::remove_timeout(&is_connected_timer);
        Fl::remove_timeout(&stream_silence_timer);
        snd_stop_stream();

        if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
            sc_disconnect();
        else
            ic_disconnect();
        
        fl_g->window_main->label(PACKAGE_STRING);

#ifdef WIN32
    if (tray_agent_is_running(NULL) == 1)
	tray_agent_send_cmd(TA_CONNECT_STATE);
#endif

    }
    else
        print_info("Connecting canceled\n", 0);

}

bool stop_recording(bool ask)
{
    if(!recording)
        return false;

    if(ask == true)
    {
        int rc = 0;
        rc = fl_choice(_("stop recording?"), _("No"), _("Yes"), NULL);
        if(rc == 0)//if NO pressed
            return false;
    }
    
    Fl::remove_timeout(&record_silence_timer);
    snd_stop_rec();
    
    // Let the user change record settings after stopping the recording
    fl_g->choice_rec_codec->activate();
    fl_g->choice_rec_bitrate->activate();
    
    if(!connected)
    {
        fl_g->choice_cfg_channel->activate();
        fl_g->choice_cfg_dev->activate();
        fl_g->choice_cfg_samplerate->activate();
        fl_g->button_cfg_rescan_devices->activate();
        
        fl_g->lcd->clear();
        fl_g->lcd->print((const uchar*)_("idle"), strlen(_("idle")));
        fl_g->radio_co_logo->show();
        fl_g->radio_co_logo->redraw();
        Fl::remove_timeout(&display_info_timer);
    }
    else
        display_info = STREAM_TIME;
    
    if (cfg.rec.signal_threshold > 0)
        Fl::add_timeout(1, &record_signal_timer);
    
    return true;
}

void button_record_cb(void)
{
    int i;
    int rc = 0;
    int cancel = 0;
    char mode[3];
    char i_str[12];
    bool index = 0;
    char *path_with_placeholder = NULL;
    char *path_for_index_loop = NULL;
    char *path_for_index_loop_fmt = NULL;
    char *path_without_split_time = NULL;
    FILE *fd;

    if(recording)
    {
        stop_recording(true);
        return;
    }

    if(strlen(cfg.rec.filename) == 0)
    {
        fl_alert(_("No recording filename specified"));
        return;
    }


    cfg.rec.path = (char*) malloc((strlen(cfg.rec.folder) +
                strlen(cfg.rec.filename)) * sizeof(char) + 10);

    strcpy(cfg.rec.path, cfg.rec.folder);
    strcat(cfg.rec.path, cfg.rec.filename);

    cfg.rec.path_fmt = strdup(cfg.rec.path);

    //expand string like file_%d_%m_%y to file_05_11_2014
    expand_string(&cfg.rec.path);

    //check if there is an index marker in the filename
    if(strstr(cfg.rec.filename, "%i"))
	{
		index = 1;

        path_with_placeholder = strdup(cfg.rec.path);
        path_for_index_loop = strdup(cfg.rec.path);
        path_for_index_loop_fmt = strdup(cfg.rec.path_fmt);

		strrpl(&cfg.rec.path, (char*)"%i", (char*)"1", MODE_ALL);
		strrpl(&cfg.rec.path_fmt, (char*)"%i", (char*)"1", MODE_ALL);
	}
    path_without_split_time = strdup(cfg.rec.path);


    //check if the file already exists
    if((fd = fl_fopen(cfg.rec.path, "rb")) != NULL)
    {
        fclose(fd);

        if(index)
        {
            //increment the index until we find a filename that doesn't exist yet
            for(i = 2; /*inf*/; i++) // index_loop
            {
                free(cfg.rec.path);
                free(cfg.rec.path_fmt);
                cfg.rec.path = strdup(path_for_index_loop);
                cfg.rec.path_fmt = strdup(path_for_index_loop_fmt);
                snprintf(i_str, sizeof(i_str), "%d", i);
                strrpl(&cfg.rec.path, (char*)"%i", i_str, MODE_ALL);
                strrpl(&cfg.rec.path_fmt, (char*)"%i", i_str, MODE_ALL);
                
                path_without_split_time = strdup(path_with_placeholder);
                strrpl(&path_without_split_time, (char*)"%i", i_str, MODE_ALL);

                if((fd = fl_fopen(cfg.rec.path, "rb")) == NULL)
                    break;

                fclose(fd);

                if (i == 0x7FFFFFFF) // 2^31-1
                {
                    free(path_for_index_loop);
                    free(path_for_index_loop_fmt);
                    free(path_without_split_time);
                    if (path_with_placeholder != NULL)
                           free(path_with_placeholder);

                    print_info(_("Could not find a valid filename"), 0);
                    return;
                }

            }
            free(path_for_index_loop);
            free(path_for_index_loop_fmt);
            strcpy(mode, "wb");
        }
        else
        {
            rc = fl_choice(_("%s already exists!"),
                    _("cancel"), _("overwrite"), _("append"), cfg.rec.path);
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
        if (path_for_index_loop != NULL)
            free(path_for_index_loop);
        if (path_for_index_loop_fmt != NULL)
            free(path_for_index_loop_fmt);
	}
    if (path_with_placeholder != NULL)
        free(path_with_placeholder);
    

    if(cancel == 1)
    {
        if (path_without_split_time != NULL)
            free(path_without_split_time);
        return;
    }

    if((cfg.rec.fd = fl_fopen(cfg.rec.path, mode)) == NULL)
    {
        fl_alert(_("Could not open:\n%s"), cfg.rec.path);
        if (path_without_split_time != NULL)
            free(path_without_split_time);
        return;
    }

    record = 1;
    timer_init(&rec_timer, 1);


    //Clear libsamplerate state
    snd_reset_samplerate_conv(SND_REC);

    // Allow the flac codec to access the file pointed to by cfg.rec.fd
    if (!strcmp(cfg.rec.codec, "flac"))
    {
        flac_enc_init(&flac_rec);
        flac_enc_init_FILE(&flac_rec, cfg.rec.fd);
    }

    // User may not change any record related audio settings while recording
    fl_g->choice_rec_codec->deactivate();
    fl_g->choice_rec_bitrate->deactivate();
    fl_g->choice_cfg_channel->deactivate();
    fl_g->choice_cfg_dev->deactivate();
    fl_g->choice_cfg_samplerate->deactivate();
    fl_g->button_cfg_rescan_devices->deactivate();

    //create the recording thread
    snd_start_rec();

    if (cfg.rec.split_time > 0)
    {
        free(cfg.rec.path);
        cfg.rec.path = strdup(path_without_split_time);
        split_recording_file_timer();
    }
    
    if (cfg.rec.silence_threshold > 0)
        Fl::add_timeout(1, &record_silence_timer);
    
    Fl::remove_timeout(&record_signal_timer);


    if(!connected)
    {
        display_info = REC_TIME;
        Fl::add_timeout(0.1, &display_info_timer);
    }
    if (path_without_split_time != NULL)
        free(path_without_split_time);
}

void button_info_cb() //changed "Info" text to "More"
{
    if (!fl_g->info_visible)
    {
        // Show info output...
        fl_g->window_main->resize(fl_g->window_main->x(),
                                  fl_g->window_main->y(),
                                  fl_g->window_main->w(),
                                  fl_g->info_output->y() + 185);
        fl_g->info_output->show();
        fl_g->button_info->label(_("Hide log"));
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
        fl_g->button_info->label(_("Show log"));
        fl_g->info_visible = 0;
    }
}

void choice_cfg_act_srv_cb(void)
{
    cfg.selected_srv = fl_g->choice_cfg_act_srv->value();

    cfg.main.srv = (char*)realloc(cfg.main.srv,
                                  strlen(cfg.srv[cfg.selected_srv]->name)+1);

    strcpy(cfg.main.srv, cfg.srv[cfg.selected_srv]->name);

}

void choice_cfg_act_icy_cb(void)
{
    cfg.selected_icy = fl_g->choice_cfg_act_icy->value();

    cfg.main.icy = (char*)realloc(cfg.main.icy,
                                  strlen(cfg.icy[cfg.selected_icy]->name)+1);

    strcpy(cfg.main.icy, cfg.icy[cfg.selected_icy]->name);
}

void button_cfg_add_srv_cb(void)
{
    fl_g->window_add_srv->label(_("Add Server"));
    fl_g->radio_add_srv_shoutcast->setonly();
    fl_g->input_add_srv_mount->deactivate();
    fl_g->input_add_srv_usr->deactivate();

    fl_g->check_add_srv_tls->deactivate();
    fl_g->frame_add_srv_tls->deactivate();
    fl_g->button_add_srv_revoke_cert->deactivate();

    fl_g->input_add_srv_pwd->input_type(FL_SECRET_INPUT);
    fl_g->input_add_srv_pwd->redraw();
    fl_g->button_cfg_show_pw->label(_("Show"));

    fl_g->button_add_srv_save->hide();
    fl_g->button_add_srv_add->show();

    fl_g->window_add_srv->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());
    fl_g->input_add_srv_name->take_focus();
    fl_g->window_add_srv->show();
}

void button_cfg_edit_srv_cb(void)
{

    char dummy[10];
    int srv;

    if(cfg.main.num_of_srv < 1)
        return;

    fl_g->window_add_srv->label(_("Edit Server"));

    srv = fl_g->choice_cfg_act_srv->value();

    fl_g->input_add_srv_name->value(cfg.srv[srv]->name);
    fl_g->input_add_srv_addr->value(cfg.srv[srv]->addr);

    snprintf(dummy, 6, "%u", cfg.srv[srv]->port);
    fl_g->input_add_srv_port->value(dummy);
    fl_g->input_add_srv_pwd->value(cfg.srv[srv]->pwd);

    fl_g->input_add_srv_pwd->input_type(FL_SECRET_INPUT);
    fl_g->input_add_srv_pwd->redraw();
    fl_g->button_cfg_show_pw->label(_("Show"));


    if(cfg.srv[srv]->type == SHOUTCAST)
    {
        fl_g->input_add_srv_mount->value("");
        fl_g->input_add_srv_mount->deactivate();
        fl_g->input_add_srv_usr->value("");
        fl_g->input_add_srv_usr->deactivate();
        fl_g->check_add_srv_tls->deactivate();
        fl_g->frame_add_srv_tls->deactivate();
        fl_g->button_add_srv_revoke_cert->deactivate();
        fl_g->radio_add_srv_shoutcast->setonly();
    }
    else //if(cfg.srv[srv]->type == ICECAST)
    {
        fl_g->input_add_srv_mount->value(cfg.srv[srv]->mount);
        fl_g->input_add_srv_mount->activate();
        fl_g->input_add_srv_usr->value(cfg.srv[srv]->usr);
        fl_g->input_add_srv_usr->activate();
        fl_g->radio_add_srv_icecast->setonly();
#ifdef HAVE_LIBSSL
        fl_g->check_add_srv_tls->activate();
        fl_g->frame_add_srv_tls->activate();
#else
        fl_g->check_add_srv_tls->deactivate();
        fl_g->frame_add_srv_tls->deactivate();
#endif
        
        if ( (cfg.srv[srv]->cert_hash != NULL) && (strlen(cfg.srv[srv]->cert_hash) == 64) )
            fl_g->button_add_srv_revoke_cert->activate();
        else
            fl_g->button_add_srv_revoke_cert->deactivate();
    }

    fl_g->check_add_srv_tls->value(cfg.srv[srv]->tls);

    fl_g->input_add_srv_name->take_focus();

    fl_g->button_add_srv_add->hide();
    fl_g->button_add_srv_save->show();

    fl_g->window_add_srv->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());
    fl_g->window_add_srv->show();
}


void button_cfg_add_icy_cb(void)
{
    fl_g->window_add_icy->label(_("Add Server Infos"));

    fl_g->button_add_icy_save->hide();
    fl_g->button_add_icy_add->show();
    fl_g->window_add_icy->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());

    //give the "name" input field the input focus
    fl_g->input_add_icy_name->take_focus();

    fl_g->window_add_icy->show();
}

void update_song(void* user_data)
{
    static int is_updating = 0;

    int prefix_len = 0;
    int suffix_len = 0;
    char text_buf[512];
    char song_buf[512];
    song_buf[0] = '\0';
    
    int (*xc_update_song)(char *song_name) = NULL;

    int called_from_connect_cb;
    if (user_data != NULL)
        called_from_connect_cb = *((int*)user_data);
    else
        called_from_connect_cb = 0;
    
    if(!connected || cfg.main.song == NULL)
        return;
    
    
    // Make sure this function is not executed from different places at the same time
    if (is_updating == 1)
        return;
    
    is_updating = 1;
    

    
    
    if (cfg.main.song_prefix != NULL)
    {
        prefix_len = strlen(cfg.main.song_prefix);
        strncat(song_buf, cfg.main.song_prefix, sizeof(song_buf)-1);
    }

    strncat(song_buf, cfg.main.song, sizeof(song_buf)-1 - prefix_len);
    
    if (cfg.main.song_suffix != NULL)
    {
        suffix_len = strlen(cfg.main.song_suffix);
        strncat(song_buf, cfg.main.song_suffix, sizeof(song_buf)-1 - prefix_len - suffix_len);
    }
    
    
    if (!strcmp(cfg.audio.codec, "flac"))
    {
        if (called_from_connect_cb == 0)
            flac_update_song_title(&flac_stream, song_buf);
        else
            flac_set_initial_song_title(&flac_stream, song_buf);
    }
    
    if (cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
        xc_update_song = &sc_update_song;
    else //if(cfg.srv[cfg.selected_srv]->type == ICECAST)
        xc_update_song = &ic_update_song;
    
    if (xc_update_song(song_buf) == 0)
    {
        snprintf(text_buf, sizeof(text_buf),
                 _("Updated songname to:\n%s\n"),
                 song_buf);
        
        print_info(text_buf, 0);
#ifdef WIN32
        tray_agent_set_song(song_buf);
        tray_agent_send_cmd(TA_SONG_UPDATE);
#endif
    }
    else
        print_info(_("Updating songname failed"), 1);
    
    is_updating = 0;
    
}

void button_cfg_song_go_cb(void)
{
    cfg.main.song = (char*)realloc(cfg.main.song, strlen(fl_g->input_cfg_song->value())+1);
    strcpy(cfg.main.song, fl_g->input_cfg_song->value());
    
    Fl::add_timeout(cfg.main.song_delay, &update_song);

    // Set focus on the song input field and mark the whole text
    fl_g->input_cfg_song->take_focus();
    fl_g->input_cfg_song->position(0);
    fl_g->input_cfg_song->mark(fl_g->input_cfg_song->maximum_size());
}

void input_cfg_song_cb(void)
{
    if (strlen(fl_g->input_cfg_song->value()) == 0)
    {
        if (cfg.main.song != NULL)
            free(cfg.main.song);
        
        cfg.main.song = NULL;
    }
}

void input_cfg_song_prefix_cb(void)
{
    if (strlen(fl_g->input_cfg_song_prefix->value()) == 0)
    {
        if (cfg.main.song_prefix != NULL)
            free(cfg.main.song_prefix);
        
        cfg.main.song_prefix = NULL;
    }
    else
    {
        cfg.main.song_prefix = (char*)realloc(cfg.main.song_prefix, strlen(fl_g->input_cfg_song_prefix->value())+1);
        strcpy(cfg.main.song_prefix, fl_g->input_cfg_song_prefix->value());
    }
}
void input_cfg_song_suffix_cb(void)
{
    if (strlen(fl_g->input_cfg_song_suffix->value()) == 0)
    {
        if (cfg.main.song_suffix != NULL)
            free(cfg.main.song_suffix);
        
        cfg.main.song_suffix = NULL;
    }
    else
    {
        cfg.main.song_suffix = (char*)realloc(cfg.main.song_suffix, strlen(fl_g->input_cfg_song_suffix->value())+1);
        strcpy(cfg.main.song_suffix, fl_g->input_cfg_song_suffix->value());
    }
}

void input_cfg_buffer_cb(bool print_message)
{
    int ms;
    char text_buf[256];

    ms = fl_g->input_cfg_buffer->value();

    if(ms < 1)
        return;

    cfg.audio.buffer_ms = ms;
    snd_reinit();

    if(print_message)
    {
        snprintf(text_buf, sizeof(text_buf), 
                _("Audio buffer has been set to %d ms"), ms);
        print_info(text_buf, 0);
    }
}

void choice_cfg_resample_mode_cb(void)
{
    cfg.audio.resample_mode = fl_g->choice_cfg_resample_mode->value();
    snd_reinit();
    switch(cfg.audio.resample_mode)
    {
        case SRC_SINC_BEST_QUALITY:
            print_info("Changed resample quality to SINC_BEST_QUALITY", 0);
            break;
        case SRC_SINC_MEDIUM_QUALITY:
            print_info("Changed resample quality to SINC_MEDIUM_QUALITY", 0);
            break;
        case SRC_SINC_FASTEST:
            print_info("Changed resample quality to SINC_FASTEST", 0);
            break;
        case SRC_ZERO_ORDER_HOLD:
            print_info("Changed resample quality to ZERO_ORDER_HOLD", 0);
            break;
        case SRC_LINEAR:
            print_info("Changed resample quality to LINEAR", 0);
            break;
        default:
            break;
    }
    
}

void radio_add_srv_shoutcast_cb(void)
{
    fl_g->input_add_srv_mount->deactivate();
    fl_g->input_add_srv_usr->deactivate();
    fl_g->check_add_srv_tls->deactivate();
    fl_g->frame_add_srv_tls->deactivate();
}

void radio_add_srv_icecast_cb(void)
{
    fl_g->input_add_srv_mount->activate();
    fl_g->input_add_srv_usr->activate();

    fl_g->input_add_srv_mount->value("stream");
    fl_g->input_add_srv_usr->value("source");
    
#ifdef HAVE_LIBSSL
    fl_g->check_add_srv_tls->activate();
    fl_g->frame_add_srv_tls->activate();
#else
    fl_g->check_add_srv_tls->deactivate();
    fl_g->frame_add_srv_tls->deactivate();
#endif

}

void button_add_srv_show_pwd_cb(void)
{
    if(fl_g->input_add_srv_pwd->input_type() == FL_SECRET_INPUT)
    {
        fl_g->input_add_srv_pwd->input_type(FL_NORMAL_INPUT);
        fl_g->input_add_srv_pwd->redraw();
        fl_g->button_cfg_show_pw->label(_("Hide"));
    }	
    else
    {
        fl_g->input_add_srv_pwd->input_type(FL_SECRET_INPUT);
        fl_g->input_add_srv_pwd->redraw();
        fl_g->button_cfg_show_pw->label(_("Show"));
    }
}

void button_add_srv_revoke_cert_cb(void)
{
    int srv;
    srv = fl_g->choice_cfg_act_srv->value();
    
    if (cfg.srv[srv]->cert_hash != NULL)
    {
        free(cfg.srv[srv]->cert_hash);
        cfg.srv[srv]->cert_hash = NULL;
        fl_g->button_add_srv_revoke_cert->deactivate();
    }
    else
    {
        fl_alert(_("Could not revoke trust for certificate"));
    }
}

// edit server
void button_add_srv_save_cb(void)
{
    int i;

    if(cfg.main.num_of_srv < 1)
        return;

    int srv_num = fl_g->choice_cfg_act_srv->value();
    int len = 0;

    //error checking
    if((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_mount->value()) == 0))
    {
        fl_alert(_("No mountpoint specified\nSetting mountpoint to \"stream\""));
        fl_g->input_add_srv_mount->value("stream");
    }
    if((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_usr->value()) == 0))
    {
        fl_alert(_("No user specified\nSetting user to \"source\""));
        fl_g->input_add_srv_usr->value("source");
    }
    if(strlen(fl_g->input_add_srv_name->value()) == 0)
    {
        fl_alert(_("No name specified"));
        return;
    }
    if(cfg.main.srv_ent != NULL)
    {
        if(strlen(fl_g->input_add_srv_name->value()) + strlen(cfg.main.srv_ent) > 1000)
        {
            fl_alert(_("The number of characters of all your server names exeeds 1000\n"
                    "Please reduce the number of characters of each server name"));
            return;
        }
    }
    if(strpbrk(fl_g->input_add_srv_name->value(), ";\\/\n\r") != NULL)
    {
        fl_alert(_("No newline characters and ;/\\ are allowed in the name field"));
        return;
    }
    if(strlen(fl_g->input_add_srv_addr->value()) == 0)
    {
        fl_alert(_("No address specified"));
        return;
    }
    if(strlen(fl_g->input_add_srv_pwd->value()) == 0)
    {
        fl_alert(_("No password specified"));
        return;
    }
    if(strlen(fl_g->input_add_srv_port->value()) == 0)
    {
        fl_alert(_("No port specified"));
        return;
    }
    else if(( atoi(fl_g->input_add_srv_port->value()) > 65535) ||
            (atoi(fl_g->input_add_srv_port->value()) < 1) )
    {
        fl_alert(_("Invalid port number\nThe port number must be between 1 and 65535"));
        return;
    }

    //check if the name already exists
    for(i = 0; i < cfg.main.num_of_srv; i++)
    {
        if(i == srv_num) //don't check name against it self
            continue;
        if(!strcmp(fl_g->input_add_srv_name->value(), cfg.srv[i]->name))
        {
            fl_alert(_("Server name already exist!"));
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
    //allocate enough memory
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
    strrpl(&cfg.srv[srv_num]->addr, (char*)"http://", (char*)"", MODE_ALL);
    strrpl(&cfg.srv[srv_num]->addr, (char*)"https://", (char*)"", MODE_ALL);


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
    
    cfg.srv[srv_num]->tls = fl_g->check_add_srv_tls->value();

    fl_g->choice_cfg_act_srv->replace(srv_num, cfg.srv[srv_num]->name);
    fl_g->choice_cfg_act_srv->redraw();

    //reset the input fields and hide the window
    fl_g->input_add_srv_name->value("");
    fl_g->input_add_srv_addr->value("");
    fl_g->input_add_srv_port->value("");
    fl_g->input_add_srv_pwd->value("");
    fl_g->input_add_srv_mount->value("");
    fl_g->input_add_srv_usr->value("");
    fl_g->check_add_srv_tls->value(0);
    
    fl_g->window_add_srv->hide();

    choice_cfg_act_srv_cb();
}

void button_add_icy_save_cb(void)
{
    int i;
    
    if(cfg.main.num_of_icy < 1)
        return;

    int icy_num = fl_g->choice_cfg_act_icy->value();
    int len = 0;
  
    if(strlen(fl_g->input_add_icy_name->value()) == 0)
    {
        fl_alert(_("No name specified"));
        return;
    }
    if(cfg.main.icy_ent != NULL)
    {
        if(strlen(fl_g->input_add_icy_name->value()) + strlen(cfg.main.icy_ent) > 1000)
        {
            fl_alert(_("The number of characters of all your icy names exeeds 1000\n"
                    "Please reduce the count of characters of each icy name"));
            return;
        }
    }
    if(strpbrk(fl_g->input_add_icy_name->value(), ";\\/\n\r") != NULL)
    {
        fl_alert(_("No newline characters and ;/\\ are allowed in the name field"));
        return;
    }
    
    //check if the name already exists
    for(i = 0; i < cfg.main.num_of_icy; i++)
    {
        if(i == icy_num) //don't check name against it self
            continue;
        if(!strcmp(fl_g->input_add_icy_name->value(), cfg.icy[i]->name))
        {
            fl_alert(_("Icy name already exist!"));
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
}

/*
void choice_cfg_edit_srv_cb(void)
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

void choice_cfg_bitrate_cb(void)
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
#ifdef HAVE_LIBFDK_AAC
    aac_stream.bitrate = br_list[sel_br];
#endif
    opus_stream.bitrate = br_list[sel_br]*1000;


    if(fl_g->choice_cfg_codec->value() == CHOICE_MP3)
    {
        rc = lame_enc_reinit(&lame_stream);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            fl_g->choice_cfg_bitrate->value(old_br);
            fl_g->choice_cfg_bitrate->redraw();
            cfg.audio.bitrate = br_list[old_br];
            lame_stream.bitrate = br_list[old_br];
            lame_enc_reinit(&lame_stream);
            print_info(_("The previous values have been set\n"), 1);
            return;
        }
    }
    if(fl_g->choice_cfg_codec->value() == CHOICE_OGG)
    {
        rc = vorbis_enc_reinit(&vorbis_stream);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.bitrate = br_list[old_br];
            vorbis_stream.bitrate = br_list[old_br];
            fl_g->choice_cfg_bitrate->value(old_br);
            fl_g->choice_cfg_bitrate->redraw();
            vorbis_enc_reinit(&vorbis_stream);
            print_info(_("The previous values have been set\n"), 1);
            return;
        }
    }
    
    if(fl_g->choice_cfg_codec->value() == CHOICE_OPUS)
    {
        rc = opus_enc_reinit(&opus_stream);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.bitrate = br_list[old_br];
            opus_stream.bitrate = br_list[old_br]*1000;
            fl_g->choice_cfg_bitrate->value(old_br);
            fl_g->choice_cfg_bitrate->redraw();
            opus_enc_reinit(&opus_stream);
            print_info(_("The previous values have been set\n"), 1);
            return;
        }
    }

#ifdef HAVE_LIBFDK_AAC
    if(fl_g->choice_cfg_codec->value() == CHOICE_AAC)
    {
        rc = aac_enc_reinit(&aac_stream);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.bitrate = br_list[old_br];
            aac_stream.bitrate = br_list[old_br];
            fl_g->choice_cfg_bitrate->value(old_br);
            fl_g->choice_cfg_bitrate->redraw();
            aac_enc_reinit(&aac_stream);
            print_info(_("The previous values have been set\n"), 1);
            return;
        }
    }
#endif


    snprintf(text_buf, sizeof(text_buf), _("Stream bitrate set to: %dk"), cfg.audio.bitrate);
    print_info(text_buf, 0);
}

void choice_rec_bitrate_cb(void)
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
    opus_rec.bitrate = br_list[sel_br]*1000;
#ifdef HAVE_LIBFDK_AAC
    aac_rec.bitrate = br_list[sel_br];
#endif


    if(fl_g->choice_rec_codec->value() == CHOICE_MP3)
    {
        rc = lame_enc_reinit(&lame_rec);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.rec.bitrate = br_list[old_br];
            fl_g->choice_rec_bitrate->value(old_br);
            fl_g->choice_rec_bitrate->redraw();
            lame_rec.bitrate = br_list[old_br];
            lame_enc_reinit(&lame_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
    if(fl_g->choice_rec_codec->value() == CHOICE_OGG)
    {
        rc = vorbis_enc_reinit(&vorbis_rec);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.rec.bitrate = br_list[old_br];
            vorbis_rec.bitrate = br_list[old_br];
            fl_g->choice_rec_bitrate->value(old_br);
            fl_g->choice_rec_bitrate->redraw();
            vorbis_enc_reinit(&vorbis_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
    
    if(fl_g->choice_rec_codec->value() == CHOICE_OPUS)
    {
        rc = opus_enc_reinit(&opus_rec);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.rec.bitrate = br_list[old_br];
            opus_rec.bitrate = br_list[old_br]*1000;
            fl_g->choice_rec_bitrate->value(old_br);
            fl_g->choice_rec_bitrate->redraw();
            opus_enc_reinit(&opus_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

#ifdef HAVE_LIBFDK_AAC
    if(fl_g->choice_rec_codec->value() == CHOICE_AAC)
    {
        rc = aac_enc_reinit(&aac_rec);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.rec.bitrate = br_list[old_br];
            aac_rec.bitrate = br_list[old_br];
            fl_g->choice_rec_bitrate->value(old_br);
            fl_g->choice_rec_bitrate->redraw();
            aac_enc_reinit(&aac_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
#endif

    snprintf(text_buf, sizeof(text_buf), _("Record bitrate set to: %dk"), cfg.rec.bitrate);
    print_info(text_buf, 0);
}

void choice_cfg_samplerate_cb() 
{
    int rc;
    int old_sr;
    int sel_sr;
    int *sr_list;
    char text_buf[256];

    sr_list = cfg.audio.pcm_list[cfg.audio.dev_num]->sr_list;

    old_sr = cfg.audio.samplerate;

    for(int i = 0; i < 9; i++)
        if(sr_list[i] == cfg.audio.samplerate)
            old_sr = i;

    sel_sr = fl_g->choice_cfg_samplerate->value();


    cfg.audio.samplerate = sr_list[sel_sr];

    // Reinit streaming codecs
    lame_stream.samplerate = sr_list[sel_sr];
    vorbis_stream.samplerate = sr_list[sel_sr];
    opus_stream.samplerate = sr_list[sel_sr];
#ifdef HAVE_LIBFDK_AAC
    aac_stream.samplerate = sr_list[sel_sr];
#endif
    flac_stream.samplerate = sr_list[sel_sr];


    if(fl_g->choice_cfg_codec->value() == CHOICE_MP3)
    {
        rc = lame_enc_reinit(&lame_stream);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            cfg.audio.samplerate = sr_list[old_sr];
            lame_stream.samplerate = sr_list[old_sr];
            lame_enc_reinit(&lame_stream);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
    if(fl_g->choice_cfg_codec->value() == CHOICE_OGG)
    {
        rc = vorbis_enc_reinit(&vorbis_stream);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            vorbis_stream.samplerate = sr_list[old_sr]; 
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            vorbis_enc_reinit(&vorbis_stream);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

    if(fl_g->choice_cfg_codec->value() == CHOICE_OPUS)
    {
        rc = opus_enc_reinit(&opus_stream);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            opus_stream.samplerate = sr_list[old_sr]; 
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            opus_enc_reinit(&opus_stream);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

#ifdef HAVE_LIBFDK_AAC
    if(fl_g->choice_cfg_codec->value() == CHOICE_AAC)
    {
        rc = aac_enc_reinit(&aac_stream);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            aac_stream.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            aac_enc_reinit(&aac_stream);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
#endif
    
    if(fl_g->choice_cfg_codec->value() == CHOICE_FLAC)
    {
        rc = flac_enc_reinit(&flac_stream);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            flac_stream.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            flac_enc_reinit(&flac_stream);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

    //Reinit record codecs
    lame_rec.samplerate = sr_list[sel_sr];
    vorbis_rec.samplerate = sr_list[sel_sr];
    opus_rec.samplerate = sr_list[sel_sr];
#ifdef HAVE_LIBFDK_AAC
    aac_rec.samplerate = sr_list[sel_sr];
#endif
    flac_rec.samplerate = sr_list[sel_sr];

    if(fl_g->choice_rec_codec->value() == CHOICE_MP3)
    {
        rc = lame_enc_reinit(&lame_rec);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            lame_stream.samplerate = sr_list[old_sr];
            lame_rec.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            lame_enc_reinit(&lame_stream);
            lame_enc_reinit(&lame_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
        
    }
    if(fl_g->choice_rec_codec->value() == CHOICE_OGG)
    {
        rc = vorbis_enc_reinit(&vorbis_rec);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            vorbis_stream.samplerate = sr_list[old_sr];
            vorbis_rec.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            vorbis_enc_reinit(&vorbis_stream);
            vorbis_enc_reinit(&vorbis_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

    if(fl_g->choice_rec_codec->value() == CHOICE_OPUS)
    {
        rc = opus_enc_reinit(&opus_rec);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            opus_stream.samplerate = sr_list[old_sr];
            opus_rec.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            opus_enc_reinit(&opus_stream);
            opus_enc_reinit(&opus_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

#ifdef HAVE_LIBFDK_AAC
    if(fl_g->choice_rec_codec->value() == CHOICE_AAC)
    {
        rc = aac_enc_reinit(&aac_rec);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            aac_stream.samplerate = sr_list[old_sr];
            aac_rec.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            aac_enc_reinit(&aac_stream);
            aac_enc_reinit(&aac_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
#endif
    if(fl_g->choice_rec_codec->value() == CHOICE_FLAC)
    {
        rc = flac_enc_reinit(&flac_rec);
        if(rc != 0)
        {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            flac_rec.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            flac_enc_reinit(&flac_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

    
    //The buffer size is dependand on the samplerate
    input_cfg_buffer_cb(0);
    

    snprintf(text_buf, sizeof(text_buf), _("Samplerate set to: %dHz"), cfg.audio.samplerate);
    print_info(text_buf, 0);


    //reinit portaudio
    snd_reinit();
}

void choice_cfg_channel_stereo_cb(void)
{
    cfg.audio.channel = 2;
    
    // Reinit streaming codecs
    lame_stream.channel = 2;
    vorbis_stream.channel = 2;
    opus_stream.channel = 2;
#ifdef HAVE_LIBFDK_AAC
    aac_stream.channel = 2;
#endif
    flac_stream.channel = 2;


    if(fl_g->choice_cfg_codec->value() == CHOICE_MP3)
        lame_enc_reinit(&lame_stream);
    if(fl_g->choice_cfg_codec->value() == CHOICE_OGG)
        vorbis_enc_reinit(&vorbis_stream);
    if(fl_g->choice_cfg_codec->value() == CHOICE_OPUS)
        opus_enc_reinit(&opus_stream);
#ifdef HAVE_LIBFDK_AAC
    if(fl_g->choice_cfg_codec->value() == CHOICE_AAC)
        aac_enc_reinit(&aac_stream);
#endif
    if(fl_g->choice_cfg_codec->value() == CHOICE_FLAC)
        flac_enc_reinit(&flac_stream);


    // Reinit recording codecs
    lame_rec.channel = 2;
    vorbis_rec.channel = 2;
    opus_rec.channel = 2;
#ifdef HAVE_LIBFDK_AAC
    aac_rec.channel = 2;
#endif
    flac_rec.channel = 2;

    if(fl_g->choice_rec_codec->value() == CHOICE_MP3)
        lame_enc_reinit(&lame_rec);
    if(fl_g->choice_rec_codec->value() == CHOICE_OGG)
        vorbis_enc_reinit(&vorbis_rec);
    if(fl_g->choice_rec_codec->value() == CHOICE_OPUS)
        opus_enc_reinit(&opus_rec);
#ifdef HAVE_LIBFDK_AAC
    if(fl_g->choice_rec_codec->value() == CHOICE_AAC)
        aac_enc_reinit(&aac_rec);
#endif
    if(fl_g->choice_rec_codec->value() == CHOICE_FLAC)
        flac_enc_reinit(&flac_rec);

    snd_reinit();
    
    print_info(_("Channels set to: stereo"), 0);
}

void choice_cfg_channel_mono_cb(void)
{
    cfg.audio.channel = 1;
  
    // Reinit streaming codecs
    lame_stream.channel = 1;
    vorbis_stream.channel = 1;
    opus_stream.channel = 1;
#ifdef HAVE_LIBFDK_AAC
    aac_stream.channel = 1;
#endif
    flac_stream.channel = 1;
    
    if(fl_g->choice_cfg_codec->value() == CHOICE_MP3)
        lame_enc_reinit(&lame_stream);
    if(fl_g->choice_cfg_codec->value() == CHOICE_OGG)
        vorbis_enc_reinit(&vorbis_stream);
    if(fl_g->choice_cfg_codec->value() == CHOICE_OPUS)
        opus_enc_reinit(&opus_stream);
#ifdef HAVE_LIBFDK_AAC
    if(fl_g->choice_cfg_codec->value() == CHOICE_AAC)
        aac_enc_reinit(&aac_stream);
#endif
    if(fl_g->choice_cfg_codec->value() == CHOICE_FLAC)
        flac_enc_reinit(&flac_stream);

    // Reinit recording codecs
    lame_rec.channel = 1;
    vorbis_rec.channel = 1;
    opus_rec.channel = 1;
#ifdef HAVE_LIBFDK_AAC
    aac_rec.channel = 1;
#endif
    flac_rec.channel = 1;

    if(fl_g->choice_rec_codec->value() == CHOICE_MP3)
        lame_enc_reinit(&lame_rec);
    if(fl_g->choice_rec_codec->value() == CHOICE_OGG)
        vorbis_enc_reinit(&vorbis_rec);
    if(fl_g->choice_rec_codec->value() == CHOICE_OPUS)
        opus_enc_reinit(&opus_rec);
#ifdef HAVE_LIBFDK_AAC
    if(fl_g->choice_rec_codec->value() == CHOICE_AAC)
        aac_enc_reinit(&aac_rec);
#endif
    if(fl_g->choice_rec_codec->value() == CHOICE_FLAC)
        flac_enc_reinit(&flac_rec);
    
    //Reinit PortAudio
    snd_reinit();

    print_info(_("Channels set to: mono"), 0);
}

void button_add_srv_cancel_cb(void)
{
    fl_g->input_add_srv_name->value("");
    fl_g->input_add_srv_addr->value("");
    fl_g->input_add_srv_port->value("");
    fl_g->input_add_srv_pwd->value("");
    fl_g->input_add_srv_mount->value("");
    fl_g->input_add_srv_usr->value("");
    fl_g->check_add_srv_tls->value(0);

    fl_g->window_add_srv->hide();
}

void button_add_icy_add_cb(void)
{
    int i;
    //error checking
    if(strlen(fl_g->input_add_icy_name->value()) == 0)
    {
        fl_alert(_("No name specified"));
        return;
    }

    if(cfg.main.icy_ent != NULL)
    {
        if(strlen(fl_g->input_add_icy_name->value()) + strlen(cfg.main.icy_ent) > 1000)
        {
            fl_alert(_("The number of characters of all your icy names exeeds 1000\n"
                    "Please reduce the number of characters of each icy name"));
            return;
        }
    }
    if(strpbrk(fl_g->input_add_icy_name->value(), ";\\/\n\r") != NULL)
    {
        fl_alert(_("No newline characters and ;/\\ are allowed in the name field"));
        return;
    }

    
    //check if the name already exists
    for(i = 0; i < cfg.main.num_of_icy; i++)
    {
        if(!strcmp(fl_g->input_add_icy_name->value(), cfg.icy[i]->name))
        {
            fl_alert(_("Server name already exist!"));
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

    cfg.icy[i]->pub = (char*)malloc(16 * sizeof(char));
    snprintf(cfg.icy[i]->pub, 15, "%d", fl_g->check_add_icy_pub->value());

    if(cfg.main.num_of_icy > 1)
    {
        cfg.main.icy_ent = (char*)realloc(cfg.main.icy_ent,
                                         strlen(cfg.main.icy_ent) +
                                         strlen(cfg.icy[i]->name) +2);
        sprintf(cfg.main.icy_ent, "%s;%s", cfg.main.icy_ent, cfg.icy[i]->name);
        cfg.main.icy = (char*)realloc(cfg.main.icy, strlen(cfg.icy[i]->name)+1);
    }
    else
    {
        cfg.main.icy_ent = (char*)malloc(strlen(cfg.icy[i]->name) +1);
        sprintf(cfg.main.icy_ent, "%s", cfg.icy[i]->name);
        cfg.main.icy = (char*)malloc(strlen(cfg.icy[i]->name)+1);

    }
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
    
    fl_g->choice_cfg_act_icy->activate();
    
    // make added icy data the active icy entry
    fl_g->choice_cfg_act_icy->value(i);
    choice_cfg_act_icy_cb();
}

void button_add_icy_cancel_cb(void)
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

void button_cfg_edit_icy_cb(void)
{
    if(cfg.main.num_of_icy < 1)
        return;

    int icy = fl_g->choice_cfg_act_icy->value();

    fl_g->window_add_icy->label(_("Edit Server Infos"));

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

void choice_cfg_dev_cb(void)
{
    cfg.audio.dev_num = fl_g->choice_cfg_dev->value();
    
    // save current device name to config struct
    cfg.audio.dev_name = (char*)realloc(cfg.audio.dev_name, strlen(cfg.audio.pcm_list[cfg.audio.dev_num]->name)+1);
    strcpy(cfg.audio.dev_name, cfg.audio.pcm_list[cfg.audio.dev_num]->name);
    
    update_samplerates();
    snd_reinit();
    update_channel_lists();
}

void button_cfg_rescan_devices_cb(void)
{
    if (connected || recording)
        return;
    
    int dev_count;
    char *current_device = strdup(cfg.audio.pcm_list[cfg.audio.dev_num]->name);
    
    snd_close();
    snd_init();
    
    snd_free_device_list();
    cfg.audio.pcm_list = snd_get_devices(&dev_count);
    cfg.audio.dev_count = dev_count;
    
    // Save name of current device
    
    fl_g->choice_cfg_dev->clear();
    for(int i = 0; i < cfg.audio.dev_count; i++)
    {
        unsigned long dev_name_len = strlen(cfg.audio.pcm_list[i]->name)+10;
        char *dev_name = (char*)malloc(dev_name_len);
        
        snprintf(dev_name, dev_name_len, "%d: %s", i, cfg.audio.pcm_list[i]->name);
        fl_g->choice_cfg_dev->add(dev_name);
        free(dev_name);
    }
    
    cfg.audio.dev_num = snd_get_dev_num_by_name(current_device);
    
    fl_g->choice_cfg_dev->value(cfg.audio.dev_num);
    fl_g->choice_cfg_dev->take_focus();
    
    snd_open_stream();
    free(current_device);
}

void radio_cfg_ID_cb(void)
{
    cfg.audio.dev_remember = REMEMBER_BY_ID;
}

void radio_cfg_name_cb(void)
{
    cfg.audio.dev_remember = REMEMBER_BY_NAME;
    // save current device name to config struct
    cfg.audio.dev_name = (char*)realloc(cfg.audio.dev_name, strlen(cfg.audio.pcm_list[cfg.audio.dev_num]->name)+1);
    strcpy(cfg.audio.dev_name, cfg.audio.pcm_list[cfg.audio.dev_num]->name);
}

void choice_cfg_left_channel_cb(void)
{
    cfg.audio.left_ch = fl_g->choice_cfg_left_channel->value()+1;
}


void choice_cfg_right_channel_cb(void)
{
    cfg.audio.right_ch = fl_g->choice_cfg_right_channel->value()+1;
}

void choice_cfg_codec_mp3_cb(void)
{
    if(lame_enc_reinit(&lame_stream) != 0)
    {
        print_info(_("MP3 encoder doesn't support current\n"
                   "Sample-/Bitrate combination"), 1);

        if(!strcmp(cfg.audio.codec, "ogg"))
            fl_g->choice_cfg_codec->value(CHOICE_OGG);
        else if (!strcmp(cfg.audio.codec, "opus"))
            fl_g->choice_cfg_codec->value(CHOICE_OPUS);
        else if (!strcmp(cfg.audio.codec, "aac"))
            fl_g->choice_cfg_codec->value(CHOICE_AAC);
        else if (!strcmp(cfg.audio.codec, "flac"))
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);

        return;
    }
    strcpy(cfg.audio.codec, "mp3");
    print_info(_("Stream codec set to mp3"), 0);
    fl_g->choice_cfg_bitrate->activate();
    fl_g->choice_cfg_bitrate->show();
}

void choice_cfg_codec_ogg_cb(void)
{
    if(vorbis_enc_reinit(&vorbis_stream) != 0)
    {
        print_info(_("OGG Vorbis encoder doesn't support current\n"
                   "Sample-/Bitrate combination"), 1);

        if(!strcmp(cfg.audio.codec, "mp3"))
            fl_g->choice_cfg_codec->value(CHOICE_MP3);
        else if (!strcmp(cfg.audio.codec, "opus"))
            fl_g->choice_cfg_codec->value(CHOICE_OPUS);
        else if (!strcmp(cfg.audio.codec, "aac"))
            fl_g->choice_cfg_codec->value(CHOICE_AAC);
        else if (!strcmp(cfg.audio.codec, "flac"))
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);

        return;
    }
    strcpy(cfg.audio.codec, "ogg");
    print_info(_("Stream codec set to ogg/vorbis"), 0);
    fl_g->choice_cfg_bitrate->activate();
    fl_g->choice_cfg_bitrate->show();
}

void choice_cfg_codec_opus_cb(void)
{
    if(opus_enc_reinit(&opus_stream) != 0)
    {
        print_info(_("Opus encoder doesn't support current\n"
                   "Sample-/Bitrate combination"), 1);

        if(!strcmp(cfg.audio.codec, "mp3"))
            fl_g->choice_cfg_codec->value(CHOICE_MP3);
        else if (!strcmp(cfg.audio.codec, "ogg"))
            fl_g->choice_cfg_codec->value(CHOICE_OGG);
        else if (!strcmp(cfg.audio.codec, "aac"))
            fl_g->choice_cfg_codec->value(CHOICE_AAC);
        else if (!strcmp(cfg.audio.codec, "flac"))
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);
        
        return;
    }
    
    print_info(_("Stream codec set to opus"), 0);
    strcpy(cfg.audio.codec, "opus");
    fl_g->choice_cfg_bitrate->activate();
    fl_g->choice_cfg_bitrate->show();
}

void choice_cfg_codec_aac_cb(void)
{
#ifdef HAVE_LIBFDK_AAC
    if (g_aac_lib_available == 0)
    {
        fl_alert(_("Could not find aac library.\nPlease follow the instructions in the manual for adding aac support."));
        if(!strcmp(cfg.audio.codec, "ogg"))
            fl_g->choice_cfg_codec->value(CHOICE_OGG);
        else if (!strcmp(cfg.audio.codec, "opus"))
            fl_g->choice_cfg_codec->value(CHOICE_OPUS);
        else if (!strcmp(cfg.audio.codec, "mp3"))
            fl_g->choice_cfg_codec->value(CHOICE_MP3);
        else if (!strcmp(cfg.audio.codec, "flac"))
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);
        
        return;
    }
    if(aac_enc_reinit(&aac_stream) != 0)
    {
        print_info(_("AAC encoder doesn't support current\n"
                   "Sample-/Bitrate combination"), 1);

        if(!strcmp(cfg.audio.codec, "ogg"))
            fl_g->choice_cfg_codec->value(CHOICE_OGG);
        else if (!strcmp(cfg.audio.codec, "opus"))
            fl_g->choice_cfg_codec->value(CHOICE_OPUS);
        else if (!strcmp(cfg.audio.codec, "mp3"))
            fl_g->choice_cfg_codec->value(CHOICE_MP3);
        else if (!strcmp(cfg.audio.codec, "flac"))
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);

        return;
    }
    strcpy(cfg.audio.codec, "aac");
    print_info(_("Stream codec set to aac"), 0);
    fl_g->choice_cfg_bitrate->activate();
    fl_g->choice_cfg_bitrate->show();

    
#endif
}

void choice_cfg_codec_flac_cb(void)
{
    if(flac_enc_reinit(&flac_stream) != 0)
    {
        print_info(_("ERROR: While initializing flac settings"), 1);
        
        if(!strcmp(cfg.audio.codec, "mp3"))
            fl_g->choice_rec_codec->value(CHOICE_MP3);
        else if(!strcmp(cfg.audio.codec, "ogg"))
            fl_g->choice_rec_codec->value(CHOICE_OGG);
        else if(!strcmp(cfg.audio.codec, "opus"))
            fl_g->choice_rec_codec->value(CHOICE_OPUS);
        else if(!strcmp(cfg.audio.codec, "aac"))
            fl_g->choice_rec_codec->value(CHOICE_AAC);
        
        return;
    }
    strcpy(cfg.audio.codec, "flac");
    print_info(_("Stream codec set to flac"), 0);
    
    fl_g->choice_cfg_bitrate->hide();
    fl_g->window_cfg->redraw();
}

void choice_rec_codec_mp3_cb(void)
{
    if(lame_enc_reinit(&lame_rec) != 0)
    {
        print_info(_("MP3 encoder doesn't support current\n"
                   "Sample-/Bitrate combination"), 1);

        //fall back to old rec codec
        if(!strcmp(cfg.rec.codec, "ogg"))
            fl_g->choice_rec_codec->value(CHOICE_OGG);
        else if(!strcmp(cfg.rec.codec, "wav"))
            fl_g->choice_rec_codec->value(CHOICE_WAV);
        else if(!strcmp(cfg.rec.codec, "opus"))
            fl_g->choice_rec_codec->value(CHOICE_OPUS);
        else if(!strcmp(cfg.rec.codec, "aac"))
            fl_g->choice_rec_codec->value(CHOICE_AAC);
        else if(!strcmp(cfg.rec.codec, "flac"))
            fl_g->choice_rec_codec->value(CHOICE_FLAC);

        return;
    }
    strcpy(cfg.rec.codec, "mp3");
    
    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    print_info(_("Record codec set to mp3"), 0);
    fl_g->choice_rec_bitrate->activate();
    fl_g->choice_rec_bitrate->show();
}

void choice_rec_codec_ogg_cb(void)
{
    if(vorbis_enc_reinit(&vorbis_rec) != 0)
    {
        print_info(_("OGG Vorbis encoder doesn't support current\n"
                   "Sample-/Bitrate combination"), 1);

        if(!strcmp(cfg.rec.codec, "mp3"))
            fl_g->choice_rec_codec->value(CHOICE_MP3);
        else if(!strcmp(cfg.rec.codec, "wav"))
            fl_g->choice_rec_codec->value(CHOICE_WAV);
        else if(!strcmp(cfg.rec.codec, "opus"))
            fl_g->choice_rec_codec->value(CHOICE_OPUS);
        else if(!strcmp(cfg.rec.codec, "aac"))
            fl_g->choice_rec_codec->value(CHOICE_AAC);
        else if(!strcmp(cfg.rec.codec, "flac"))
            fl_g->choice_rec_codec->value(CHOICE_FLAC);

        return;
    }
    strcpy(cfg.rec.codec, "ogg");

    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    print_info(_("Record codec set to ogg/vorbis"), 0);
    fl_g->choice_rec_bitrate->activate();
    fl_g->choice_rec_bitrate->show();
}

void choice_rec_codec_opus_cb(void)
{
   if(opus_enc_reinit(&opus_rec) != 0)
    {
        print_info(_("Opus encoder doesn't support current\n"
                   "Sample-/Bitrate combination"), 1);

        if(!strcmp(cfg.rec.codec, "mp3"))
            fl_g->choice_rec_codec->value(CHOICE_MP3);
        else if(!strcmp(cfg.rec.codec, "wav"))
            fl_g->choice_rec_codec->value(CHOICE_WAV);
        else if(!strcmp(cfg.rec.codec, "ogg"))
            fl_g->choice_rec_codec->value(CHOICE_OGG);
        else if(!strcmp(cfg.rec.codec, "aac"))
            fl_g->choice_rec_codec->value(CHOICE_AAC);
        else if(!strcmp(cfg.rec.codec, "flac"))
            fl_g->choice_rec_codec->value(CHOICE_FLAC);


        return;
    }
    strcpy(cfg.rec.codec, "opus");

    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    print_info(_("Record codec set to opus"), 0);
    fl_g->choice_rec_bitrate->activate();
    fl_g->choice_rec_bitrate->show();
}

void choice_rec_codec_aac_cb(void)
{
#ifdef HAVE_LIBFDK_AAC
    if (g_aac_lib_available == 0)
    {
        fl_alert(_("Could not find aac library.\nPlease follow the instructions in the manual for adding aac support."));
        if(!strcmp(cfg.audio.codec, "ogg"))
            fl_g->choice_cfg_codec->value(CHOICE_OGG);
        else if (!strcmp(cfg.audio.codec, "opus"))
            fl_g->choice_cfg_codec->value(CHOICE_OPUS);
        else if (!strcmp(cfg.audio.codec, "mp3"))
            fl_g->choice_cfg_codec->value(CHOICE_MP3);
        else if (!strcmp(cfg.audio.codec, "flac"))
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);
        
        return;
    }
    if(aac_enc_reinit(&aac_rec) != 0)
    {
        print_info(_("AAC encoder doesn't support current\n"
                   "Sample-/Bitrate combination"), 1);

        //fall back to old rec codec
        if(!strcmp(cfg.rec.codec, "ogg"))
            fl_g->choice_rec_codec->value(CHOICE_OGG);
        else if(!strcmp(cfg.rec.codec, "wav"))
            fl_g->choice_rec_codec->value(CHOICE_WAV);
        else if(!strcmp(cfg.rec.codec, "opus"))
            fl_g->choice_rec_codec->value(CHOICE_OPUS);
        else if(!strcmp(cfg.rec.codec, "flac"))
            fl_g->choice_rec_codec->value(CHOICE_FLAC);
        else if(!strcmp(cfg.rec.codec, "mp3"))
            fl_g->choice_rec_codec->value(CHOICE_MP3);

        return;
    }
    strcpy(cfg.rec.codec, "aac");
    
    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    print_info(_("Record codec set to aac"), 0);
    fl_g->choice_rec_bitrate->activate();
    fl_g->choice_rec_bitrate->show();


    
#endif
}

void choice_rec_codec_flac_cb(void)
{
    if(flac_enc_reinit(&flac_rec) != 0)
    {
        print_info(_("ERROR: While initializing flac settings"), 1);

        if(!strcmp(cfg.rec.codec, "mp3"))
            fl_g->choice_rec_codec->value(CHOICE_MP3);
        else if(!strcmp(cfg.rec.codec, "ogg"))
            fl_g->choice_rec_codec->value(CHOICE_OGG);
        else if(!strcmp(cfg.rec.codec, "opus"))
            fl_g->choice_rec_codec->value(CHOICE_OPUS);
        else if(!strcmp(cfg.rec.codec, "wav"))
            fl_g->choice_rec_codec->value(CHOICE_WAV);
        else if(!strcmp(cfg.rec.codec, "aac"))
            fl_g->choice_rec_codec->value(CHOICE_AAC);

        return;
    }
    strcpy(cfg.rec.codec, "flac");

    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    print_info(_("Record codec set to flac"), 0);
    fl_g->choice_rec_bitrate->hide();
    fl_g->window_cfg->redraw();
}

void choice_rec_codec_wav_cb(void)
{
    fl_g->choice_rec_bitrate->hide();
    fl_g->window_cfg->redraw();

    strcpy(cfg.rec.codec, "wav");

    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    print_info(_("Record codec set to wav"), 0);

}

void input_tls_cert_file_cb(void)
{
    cfg.tls.cert_file = (char*)realloc(cfg.tls.cert_file,
                        strlen(fl_g->input_tls_cert_file->value())+1);

      strcpy(cfg.tls.cert_file, fl_g->input_tls_cert_file->value());
      fl_g->input_tls_cert_file->tooltip(cfg.tls.cert_file);
}

void input_tls_cert_dir_cb(void)
{
    int len = strlen(fl_g->input_tls_cert_dir->value());
    
    cfg.tls.cert_dir = (char*)realloc(cfg.tls.cert_dir, len +2);
    
    strcpy(cfg.tls.cert_dir, fl_g->input_tls_cert_dir->value());
    
#ifdef WIN32    //Replace all "Windows slashes" with "unix slashes"
    strrpl(&cfg.tls.cert_dir, (char*)"\\", (char*)"/", MODE_ALL);
#endif
    
    //Append an '/' if there isn't one
    if ( (len > 0) && (cfg.tls.cert_dir[len-1] != '/'))
        strcat(cfg.tls.cert_dir, "/");
    
    fl_g->input_tls_cert_dir->value(cfg.tls.cert_dir);
    fl_g->input_tls_cert_dir->tooltip(cfg.tls.cert_dir);
}

void button_tls_browse_file_cb(void)
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Select certificate file..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_FILE);
    
    switch(nfc.show())
    {
        case -1: fl_alert(_("ERROR: %s"), nfc.errmsg());
            break;
        case  1:
            break; //cancel pressed
        default:
            fl_g->input_tls_cert_file->value(nfc.filename());
            input_tls_cert_file_cb();
    }
}

void button_tls_browse_dir_cb(void)
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Select certificate directory..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_DIRECTORY);
    nfc.options(Fl_My_Native_File_Chooser::NEW_FOLDER);
    
    nfc.directory(fl_g->input_tls_cert_dir->value());
    
    switch(nfc.show())
    {
        case -1:
            fl_alert(_("ERROR: %s"), nfc.errmsg()); //error
            break;
        case  1:
            break; //cancel pressed
        default:
            fl_g->input_tls_cert_dir->value(nfc.filename());
            input_tls_cert_dir_cb();
            break;
    }
}

void ILM216_cb(void)
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

        
    }*/
}

void button_rec_browse_cb(void)
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Record to..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_DIRECTORY);
    nfc.options(Fl_My_Native_File_Chooser::NEW_FOLDER);


    nfc.directory(fl_g->input_rec_folder->value());
        
    switch(nfc.show())
    {
        case -1: fl_alert(_("ERROR: %s"), nfc.errmsg()); //error
                 break;
        case  1: break; //cancel
        default:
                 fl_g->input_rec_folder->value(nfc.filename());
                 input_rec_folder_cb();
                 
                 break;
    }
}
void button_rec_split_now_cb(void)
{
    if (recording)
    {
        split_recording_file();
    }
    else
    {
        fl_alert(_("File splitting only works if recording is active."));
    }
}

void input_rec_filename_cb(void)
{
    char *tooltip;

    cfg.rec.filename = (char*)realloc(cfg.rec.filename,
                       strlen(fl_g->input_rec_filename->value())+1);

    strcpy(cfg.rec.filename, fl_g->input_rec_filename->value());
    
    //check if the extension of the filename matches 
    //the current selected codec
    test_file_extension();

    tooltip = strdup(cfg.rec.filename);

    expand_string(&tooltip);

    fl_g->input_rec_filename->copy_tooltip(tooltip);

    
    free(tooltip);
}

void input_rec_folder_cb(void)
{
    int len = strlen(fl_g->input_rec_folder->value());

    cfg.rec.folder = (char*)realloc(cfg.rec.folder, len +2);

    strcpy(cfg.rec.folder, fl_g->input_rec_folder->value());

#ifdef WIN32    //Replace all "Windows slashes" with "unix slashes"
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
}

void input_log_filename_cb(void)
{
    cfg.main.log_file = (char*)realloc(cfg.main.log_file,
                       strlen(fl_g->input_log_filename->value())+1);

    strcpy(cfg.main.log_file, fl_g->input_log_filename->value());
    fl_g->input_log_filename->tooltip(cfg.main.log_file);
}

void button_cfg_browse_songfile_cb(void)
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Select Songfile"));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_FILE);
    switch(nfc.show())
    {
        case -1: fl_alert(_("ERROR: %s"), nfc.errmsg());
            break;
        case  1:
            break; //cancel
        default:
            fl_g->input_cfg_song_file->value(nfc.filename());
            input_cfg_song_file_cb();
    }
}

void input_cfg_song_file_cb(void)
{
    int len = strlen(fl_g->input_cfg_song_file->value());

    cfg.main.song_path = (char*)realloc(cfg.main.song_path, len +1);

    strcpy(cfg.main.song_path, fl_g->input_cfg_song_file->value());

#ifdef WIN32    //Replace all "Windows slashes" with "unix slashes"
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
}

void check_gui_attach_cb(void)
{
    if(fl_g->check_gui_attach->value())
    {
        cfg.gui.attach = 1;
        Fl::add_timeout(0.1, &cfg_win_pos_timer);
    }
    else
    {
        cfg.gui.attach = 0;
        Fl::remove_timeout(&cfg_win_pos_timer);
    }
}

void check_gui_ontop_cb(void)
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

}
void check_gui_hide_log_window_cb(void)
{
    if(fl_g->check_gui_hide_log_window->value())
        cfg.gui.hide_log_window = 1;
    else
        cfg.gui.hide_log_window = 0;
}

void check_gui_remember_pos_cb(void)
{
    if(fl_g->check_gui_remember_pos->value())
        cfg.gui.remember_pos = 1;
    else
        cfg.gui.remember_pos = 0;
}

void check_gui_lcd_auto_cb(void)
{
    if(fl_g->check_gui_lcd_auto->value())
    {
        cfg.gui.lcd_auto = 1;
    }
    else
    {
        cfg.gui.lcd_auto = 0;
    }
}

void check_gui_start_minimized_cb(void)
{
    cfg.gui.start_minimized = fl_g->check_gui_start_minimized->value();
}

void button_gui_bg_color_cb(void)
{
    uchar r, g, b;

    Fl_Color bg;
    bg  = (Fl_Color)cfg.main.bg_color;

    //Set the r g b values the color_chooser should start with
    r = (bg & 0xFF000000) >> 24;
    g = (bg & 0x00FF0000) >> 16;
    b = (bg & 0x0000FF00) >>  8;

    fl_color_chooser(_("select background color"), r, g, b);

    //The color_chooser changes the r, g, b, values to selected color
    cfg.main.bg_color = fl_rgb_color(r, g, b);

    fl_g->button_gui_bg_color->color(cfg.main.bg_color, fl_lighter((Fl_Color)cfg.main.bg_color));
    fl_g->button_gui_bg_color->redraw();
    fl_g->lcd->redraw();
    fl_g->radio_co_logo->redraw();
}

void button_gui_text_color_cb(void)
{
    uchar r, g, b;

    Fl_Color txt;
    txt = (Fl_Color)cfg.main.txt_color;

    //Set the r g b values the color_chooser should start with
    r = (txt & 0xFF000000) >> 24;
    g = (txt & 0x00FF0000) >> 16;
    b = (txt & 0x0000FF00) >>  8;

    fl_color_chooser(_("select text color"), r, g, b);

    //The color_chooser changes the r, g, b, values to selected color
    cfg.main.txt_color = fl_rgb_color(r, g, b);

    fl_g->button_gui_text_color->color(cfg.main.txt_color, fl_lighter((Fl_Color)cfg.main.txt_color));
    fl_g->button_gui_text_color->redraw();
    fl_g->lcd->redraw();
    fl_g->radio_co_logo->redraw();
}

void choice_gui_language_cb(void)
{
    switch (fl_g->choice_gui_language->value()) {
        case LANG_DE:
            cfg.gui.lang = LANG_DE;
            break;
        case LANG_EN:
            cfg.gui.lang = LANG_EN;
            break;
        case LANG_FR:
            cfg.gui.lang = LANG_FR;
            break;
        default:
            cfg.gui.lang = LANG_SYSTEM;
            break;
    }
    
    fl_alert(_("Please restart butt to apply new language."));

}

void radio_gui_vu_gradient_cb(void)
{
    cfg.gui.vu_mode = VU_MODE_GRADIENT;
}
void radio_gui_vu_solid_cb(void)
{
    cfg.gui.vu_mode = VU_MODE_SOLID;
}


void check_cfg_auto_start_rec_cb(void)
{
    cfg.rec.start_rec = fl_g->check_cfg_auto_start_rec->value();
    fl_g->lcd->redraw();  //update the little record icon
    fl_g->radio_co_logo->redraw();
}
void check_cfg_auto_stop_rec_cb(void)
{
    cfg.rec.stop_rec = fl_g->check_cfg_auto_stop_rec->value();
}


void check_cfg_rec_after_launch_cb(void)
{
    cfg.rec.rec_after_launch = fl_g->check_cfg_rec_after_launch->value();
}

void check_cfg_rec_hourly_cb(void)
{
    //cfg.rec.start_rec_hourly = fl_g->check_cfg_rec_hourly->value();
}

void check_cfg_connect_cb(void)
{
    cfg.main.connect_at_startup = fl_g->check_cfg_connect->value();
}

void check_cfg_force_reconnecting_cb(void)
{
    cfg.main.force_reconnecting = fl_g->check_cfg_force_reconnecting->value();
}

void input_cfg_signal_cb(void)
{
    // Values < 0 are not allowed
    if(fl_g->input_cfg_signal->value() <= 0)
    {
        fl_g->input_cfg_signal->value(0);
        Fl::remove_timeout(&stream_signal_timer);
    }
    else
        Fl::add_timeout(1, &stream_signal_timer);

    cfg.main.signal_threshold = fl_g->input_cfg_signal->value();
}


void input_cfg_present_level_cb(void) 
{
    if(fl_g->input_cfg_present_level->value() < -90)
    {
        fl_alert(_("Value must be a number between -90.0 and 0"));
        fl_g->input_cfg_present_level->value(-cfg.audio.signal_level);
    }

    if(fl_g->input_cfg_present_level->value() > 0) 
    {
        fl_alert(_("Value must be a number between -90.0 and 0"));
        fl_g->input_cfg_present_level->value(-cfg.audio.signal_level);
    }

    cfg.audio.signal_level = -fl_g->input_cfg_present_level->value();
}

void input_cfg_absent_level_cb(void)
{
    if(fl_g->input_cfg_absent_level->value() < -90)
    {
        fl_alert(_("Value must be a number between -90.0 and 0"));
        fl_g->input_cfg_absent_level->value(-cfg.audio.silence_level);
    }

    if(fl_g->input_cfg_absent_level->value() > 0) 
    {
        fl_alert(_("Value must be a number between -90.0 and 0"));
        fl_g->input_cfg_absent_level->value(-cfg.audio.silence_level);
    }

    cfg.audio.silence_level = -fl_g->input_cfg_absent_level->value();
}

void input_cfg_silence_cb(void)
{
    // Values < 0 are not allowed
    if(fl_g->input_cfg_silence->value() < 0)
    {
        fl_g->input_cfg_silence->value(0);
        Fl::remove_timeout(&stream_silence_timer);
    }
    else
        Fl::add_timeout(1, &stream_silence_timer);


    cfg.main.silence_threshold = fl_g->input_cfg_silence->value();
}

void input_rec_signal_cb(void)
{
    // Values < 0 are not allowed
    if(fl_g->input_rec_signal->value() <= 0)
    {
        fl_g->input_rec_signal->value(0);
        Fl::remove_timeout(&record_signal_timer);
    }
    else
        Fl::add_timeout(1, &record_signal_timer);

    cfg.rec.signal_threshold = fl_g->input_rec_signal->value();
}

void input_rec_silence_cb(void)
{
    // Values < 0 are not allowed
    if(fl_g->input_rec_silence->value() < 0)
    {
        fl_g->input_rec_silence->value(0);
        Fl::remove_timeout(&record_silence_timer);
    }
    else
        Fl::add_timeout(1, &record_silence_timer);

    cfg.rec.silence_threshold = fl_g->input_rec_silence->value();
}

void check_song_update_active_cb(void)
{
   if(fl_g->check_song_update_active->value())
   {
       if(connected)
       {
           static int reset = 1;
           Fl::remove_timeout(&songfile_timer);
           Fl::add_timeout(0.1, &songfile_timer, &reset);
       }
       cfg.main.song_update = 1;
   }
   else
   {
       Fl::remove_timeout(&songfile_timer);
       if (cfg.main.song != NULL)
       {
           free(cfg.main.song);
           cfg.main.song = NULL;
       }
       cfg.main.song_update = 0;
   }
}

void check_read_last_line_cb(void)
{
    cfg.main.read_last_line = fl_g->check_read_last_line->value();
}

void check_sync_to_full_hour_cb(void)
{
   if(fl_g->check_sync_to_full_hour->value())
       cfg.rec.sync_to_hour = 1;
   else
       cfg.rec.sync_to_hour = 0;
}

void slider_gain_cb(void)
{
    float gain_db;

    //Without redrawing the main window the slider knob is not redrawn correctly
    fl_g->window_main->redraw();

    gain_db = (float)fl_g->slider_gain->value();

    if((int)gain_db == 0)
        cfg.main.gain = 1;
    else
        cfg.main.gain = util_db_to_factor(gain_db);

    fl_g->slider_gain->value_cb2("dB");
    
}

void input_rec_split_time_cb(void)
{
    // Values < 0 are not allowed
    if (fl_g->input_rec_split_time->value() < 0)
    {
        fl_g->input_rec_split_time->value(0);
    }

    cfg.rec.split_time = fl_g->input_rec_split_time->value();
}

void button_cfg_export_cb(void)
{
    char *filename;
    
    Fl_My_Native_File_Chooser nfc;
    
    nfc.title(_("Export to..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_SAVE_FILE);
    nfc.options(Fl_My_Native_File_Chooser::NEW_FOLDER);
    
    switch(nfc.show())
    {
        case -1: fl_alert(_("ERROR: %s"), nfc.errmsg()); //error
            return;
            break;
        case  1: return; // cancel
            break;
        default:
            filename = (char*)nfc.filename();
    }
   
    cfg_write_file(filename);
}

void button_cfg_import_cb(void)
{
    char *filename;
    char info_buf[256];

    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Import..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_FILE);

    switch(nfc.show())
    {
        case -1: fl_alert(_("ERROR: %s"), nfc.errmsg()); //error
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
        snprintf(info_buf, sizeof(info_buf), _("Could not import config %s"), filename);
        print_info(info_buf, 1);
        return;
    }

    //re-initialize some stuff after config has been successfully imported
    init_main_gui_and_audio();
    fill_cfg_widgets();
    snd_reinit();

    snprintf(info_buf, sizeof(info_buf), _("Config imported %s"), filename);
    print_info(info_buf, 1);
}

void check_update_at_startup_cb(void)
{
    cfg.main.check_for_update = fl_g->check_update_at_startup->value();
}

void check_start_agent_cb(void)
{
    cfg.main.start_agent = fl_g->check_start_agent->value();
}

void button_start_agent_cb(void)
{
#ifdef WIN32
    if (tray_agent_is_running(NULL) == 0)
    {
        if (tray_agent_start() == 0)
	{
	    tray_agent_send_cmd(TA_START);
	    Fl::add_timeout(1, &has_agent_been_started_timer);
	}
    }
    else
    {
        fl_alert("butt agent is already running.");
    }
#endif
}

void button_stop_agent_cb(void)
{
#ifdef WIN32
    if (tray_agent_is_running(NULL) == 1)
    {
        tray_agent_stop(); 
        Fl::add_timeout(1, &has_agent_been_stopped_timer);
    }
    else
    {
        fl_alert("butt agent is currently not running.");
    }
    
#endif
}

void check_minimize_to_tray_cb(void)
{
    cfg.main.minimize_to_tray = fl_g->check_minimize_to_tray->value();
    
    if (cfg.main.minimize_to_tray == 1)
        fl_g->window_main->minimize_to_tray = true;
    else
        fl_g->window_main->minimize_to_tray = false;
}

void button_cfg_check_for_updates_cb(void)
{
    int rc;
    char uri[100];
    char *new_version;
    int ret = update_check_for_new_version();

    switch(ret) {
        case UPDATE_NEW_VERSION:
            new_version = update_get_version();
            rc = fl_choice(_("New version available: %s\nYou have version %s"), _("Cancel"), _("Get new version"), NULL, new_version, VERSION);
            if(rc == 1)
            {
                //snprintf(uri, sizeof(uri)-1, "https://sourceforge.net/projects/butt/files/butt/butt-%s/", new_version);
                snprintf(uri, sizeof(uri)-1, "https://danielnoethen.de/butt/index.html#_download");
                fl_open_uri(uri);
            }

            break;
        case UPDATE_SOCKET_ERROR:
            fl_alert(_("Could not get update information.\nReason: Network error"));
            break;
        case UPDATE_ILLEGAL_ANSWER:
            fl_alert(_("Could not get update information.\nReason: Unknown answer from server"));
            break;
        case UPDATE_UP_TO_DATE:
            fl_alert(_("You have the latest version!"));
            break;
        default:
            fl_alert(_("Could not get update information.\nReason: Unknown"));
            break;
    }
}

void check_cfg_mono_to_stereo_cb(void)
{
    cfg.audio.mono_to_stereo = fl_g->check_cfg_mono_to_stereo->value();
}

void button_cfg_log_browse_cb(void)
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Select logfile..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_SAVE_FILE);
    nfc.options(Fl_My_Native_File_Chooser::NEW_FOLDER);
    
    switch(nfc.show())
    {
        case -1: fl_alert(_("ERROR: %s"), nfc.errmsg());
            break;
        case  1:
            break; //cancel
        default:
            fl_g->input_log_filename->value(nfc.filename());
            input_log_filename_cb();
    }
}

void window_main_close_cb(void)
{
    
    
    if(connected || recording)
    {
        int ret;
        if(connected)
        {
            
            fl_message_title("Streaming");
            ret = fl_choice(_("butt is currently streaming.\n"
                            "Do you really want to close butt now?"),
                            _("no"), _("yes"), NULL);
        }
        else
        {
            fl_message_title("Recording");
            ret = fl_choice(_("butt is currently recording.\n"
                            "Do you really want to close butt now?"),
                            _("no"), _("yes"), NULL);
        }
            
        if(ret == 0)
            return;
    }
    
    stop_recording(false);
    button_disconnect_cb();
  
    if (cfg.gui.remember_pos)
    {
        cfg.gui.x_pos = fl_g->window_main->x_root();
        cfg.gui.y_pos = fl_g->window_main->y_root();
    }
    
    cfg_write_file(NULL);
    exit(0);
}

void check_cfg_use_app_cb(void)
{
    if(fl_g->check_cfg_use_app->value())
    {
        int app = fl_g->choice_cfg_app->value();
        current_track_app = getCurrentTrackFunctionFromId(app);
        cfg.main.app_update = 1;
        cfg.main.app_update_service = app;
        
        static int reset = 1;
        Fl::remove_timeout(&app_timer);
        Fl::add_timeout(0.1, &app_timer, &reset);
    }
    else
    {
        cfg.main.app_update = 0;
        Fl::remove_timeout(&app_timer);
        
       if (cfg.main.song != NULL)
       {
           free(cfg.main.song);
           cfg.main.song = NULL;
       }
    }
}

void choice_cfg_app_cb(void)
{
    current_track_app = getCurrentTrackFunctionFromId(fl_g->choice_cfg_app->value());
    cfg.main.app_update_service = fl_g->choice_cfg_app->value();
}

void radio_cfg_artist_title_cb(void)
{
    cfg.main.app_artist_title_order = APP_ARTIST_FIRST;
}

void radio_cfg_title_artist_cb(void)
{
    cfg.main.app_artist_title_order = APP_TITLE_FIRST;
}

void choice_cfg_song_delay_cb(void)
{
    cfg.main.song_delay = 2*fl_g->choice_cfg_song_delay->value();
}

void check_activate_eq_cb(void)
{
    cfg.dsp.equalizer = fl_g->check_activate_eq->value();
}

void slider_equalizer1_cb(double v)
{
    static char str[10];
    cfg.dsp.gain1 = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain1->label(str);
    fl_g->equalizerSlider1->value_cb2("dB"); // updates the tooltip
    
}

void slider_equalizer2_cb(double v)
{
    static char str[10];
    cfg.dsp.gain2 = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain2->label(str);
    fl_g->equalizerSlider2->value_cb2("dB");
}

void slider_equalizer3_cb(double v)
{
    static char str[10];
    cfg.dsp.gain3 = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain3->label(str);
    fl_g->equalizerSlider3->value_cb2("dB");
}

void slider_equalizer4_cb(double v)
{
    static char str[10];
    cfg.dsp.gain4 = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain4->label(str);
    fl_g->equalizerSlider4->value_cb2("dB");
}

void slider_equalizer5_cb(double v)
{
    static char str[10];
    cfg.dsp.gain5 = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain5->label(str);
    fl_g->equalizerSlider5->value_cb2("dB");
}

void slider_equalizer6_cb(double v)
{
    static char str[10];
    cfg.dsp.gain6 = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain6->label(str);
    fl_g->equalizerSlider6->value_cb2("dB");
}

void slider_equalizer7_cb(double v)
{
    static char str[10];
    cfg.dsp.gain7 = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain7->label(str);
    fl_g->equalizerSlider7->value_cb2("dB");
}

void slider_equalizer8_cb(double v)
{
    static char str[10];
    cfg.dsp.gain8 = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain8->label(str);
    fl_g->equalizerSlider8->value_cb2("dB");
}

void slider_equalizer9_cb(double v)
{
    static char str[10];
    cfg.dsp.gain9 = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain9->label(str);
    fl_g->equalizerSlider9->value_cb2("dB");
}

void slider_equalizer10_cb(double v)
{
    static char str[10];
    cfg.dsp.gain10 = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain10->label(str);
    fl_g->equalizerSlider10->value_cb2("dB");
}


void check_activate_drc_cb(void)
{
    cfg.dsp.compressor = fl_g->check_activate_drc->value();
    
    if (cfg.dsp.compressor == 0)
        fl_g->LED_comp_threshold->set_state(LED::LED_OFF);
    
    // Make sure compressor starts in a clean state
    snd_reset_compressor();
}

void check_aggressive_mode_cb(void)
{
    cfg.dsp.aggressive_mode = fl_g->check_aggressive_mode->value();
    snd_reset_compressor();
}

void slider_threshold_cb(double v)
{
    static char str[10];
    cfg.dsp.threshold = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->threshold->label(str);
    fl_g->thresholdSlider->value_cb2("dBFS"); // updates the tooltip
    
    snd_reset_compressor();
    
}

void slider_ratio_cb(double v)
{
    static char str[10];
    cfg.dsp.ratio = v;
    snprintf(str, 10, "%.1f", v);
    fl_g->ratio->label(str);
    fl_g->ratioSlider->value_cb2(":1");
    
    snd_reset_compressor();
}

void slider_attack_cb(double v)
{
    static char str[10];
    cfg.dsp.attack = v;
    snprintf(str, 10, "%.2fs", v);
    fl_g->attack->label(str);
    fl_g->attackSlider->value_cb2("s");
    
    snd_reset_compressor();
}

void slider_release_cb(double v)
{
    static char str[10];
    cfg.dsp.release = v;
    snprintf(str, 10, "%.2fs", v);
    fl_g->release->label(str);
    fl_g->releaseSlider->value_cb2("s");
    
    snd_reset_compressor();
}

void slider_makeup_cb(double v)
{
    static char str[10];
    cfg.dsp.makeup_gain = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->makeup->label(str);
    fl_g->makeupSlider->value_cb2("dB");
}

