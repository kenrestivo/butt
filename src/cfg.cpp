// config functions for butt
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

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "config.h"
#include "gettext.h"

#include "cfg.h"
#include "butt.h"
#include "flgui.h"
#include "util.h"
#include "fl_funcs.h"
#include "strfuncs.h"

#ifdef WIN32
 const char CONFIG_FILE[] = "buttrc";
#else
 const char CONFIG_FILE[] = ".buttrc";
#endif

config_t cfg;
char *cfg_path;

int cfg_write_file(char *path)
{
    int i;
    FILE *cfg_fd;
    char info_buf[256];

    if(path == NULL)
        path = cfg_path;

    cfg_fd = fl_fopen(path, "wb");
    if(cfg_fd == NULL)
    {
        snprintf(info_buf, sizeof(info_buf), _("Could not write to file: %s"), path);
        print_info(cfg_path, 1);
        return 1;
    }

    fprintf(cfg_fd, "#This is a configuration file for butt (broadcast using this tool)\n\n");
    fprintf(cfg_fd, "[main]\n");

    fprintf(cfg_fd, "bg_color = %d\n", cfg.main.bg_color);
    fprintf(cfg_fd, "txt_color = %d\n", cfg.main.txt_color);

    if(cfg.main.num_of_srv > 0)
        fprintf(cfg_fd, 
                "server = %s\n"
                "srv_ent = %s\n",
                cfg.main.srv,
                cfg.main.srv_ent
               );
    else
        fprintf(cfg_fd, 
                "server = \n"
                "srv_ent = \n"
               );

    if(cfg.main.num_of_icy > 0)
        fprintf(cfg_fd, 
                "icy = %s\n"
                "icy_ent = %s\n",
                cfg.main.icy,
                cfg.main.icy_ent
               );
    else
        fprintf(cfg_fd, 
                "icy = \n"
                "icy_ent = \n"
               );

    fprintf(cfg_fd,
            "num_of_srv = %d\n"
            "num_of_icy = %d\n",
            cfg.main.num_of_srv,
            cfg.main.num_of_icy
           );

    if(cfg.main.song_path != NULL)
        fprintf(cfg_fd, "song_path = %s\n", cfg.main.song_path);
    else
        fprintf(cfg_fd, "song_path = \n");

    fprintf(cfg_fd, "song_update = %d\n", cfg.main.song_update);
    
    fprintf(cfg_fd, "song_delay = %d\n", cfg.main.song_delay);
    
    
    if (cfg.main.song_prefix != NULL)
    {
        char *tmp = strdup(cfg.main.song_prefix);
        strrpl(&tmp, (char*)" ", (char*)"%20", MODE_ALL);
        fprintf(cfg_fd, "song_prefix = %s\n", tmp);
        free(tmp);
    }
    else
        fprintf(cfg_fd, "song_prefix = \n");
    
    if (cfg.main.song_suffix != NULL)
    {
        char *tmp = strdup(cfg.main.song_suffix);
        strrpl(&tmp, (char*)" ", (char*)"%20", MODE_ALL);
        fprintf(cfg_fd, "song_suffix = %s\n", tmp);
        free(tmp);
    }
    else
        fprintf(cfg_fd, "song_suffix = \n");


    fprintf(cfg_fd, "read_last_line = %d\n", cfg.main.read_last_line);

    
    fprintf(cfg_fd, "app_update_service = %d\n", cfg.main.app_update_service);
    fprintf(cfg_fd, "app_update = %d\n", cfg.main.app_update);
    fprintf(cfg_fd, "app_artist_title_order = %d\n", cfg.main.app_artist_title_order);


    fprintf(cfg_fd, "gain = %f\n", cfg.main.gain);

    fprintf(cfg_fd, "silence_threshold = %d\n", cfg.main.silence_threshold);
    fprintf(cfg_fd, "signal_threshold = %d\n", cfg.main.signal_threshold);
    fprintf(cfg_fd, "check_for_update = %d\n", cfg.main.check_for_update);
    fprintf(cfg_fd, "start_agent = %d\n", cfg.main.start_agent);
    fprintf(cfg_fd, "minimize_to_tray = %d\n", cfg.main.minimize_to_tray);
    fprintf(cfg_fd, "connect_at_startup = %d\n", cfg.main.connect_at_startup);
    fprintf(cfg_fd, "force_reconnecting = %d\n", cfg.main.force_reconnecting);


    if (cfg.main.ic_charset != NULL)
        fprintf(cfg_fd, "ic_charset = %s\n", cfg.main.ic_charset);
    else
        fprintf(cfg_fd, "ic_charset = \n");
        

    if (cfg.main.log_file != NULL)
        fprintf(cfg_fd, "log_file = %s\n\n", cfg.main.log_file);
    else
        fprintf(cfg_fd, "log_file = \n\n");

    fprintf(cfg_fd,
            "[audio]\n"
            "device = %d\n"
            "dev_remember = %d\n"
            "samplerate = %d\n"
            "bitrate = %d\n"
            "channel = %d\n"
            "left_ch = %d\n"
            "right_ch = %d\n"
            "codec = %s\n"
            "mono_to_stereo = %d\n"
            "aac_overwrite_aot = %d\n"
            "aac_aot = %d\n"
            "resample_mode = %d\n"
            "silence_level = %f\n"
            "signal_level = %f\n"
            "disable_dithering = %d\n"
            "buffer_ms = %d\n",
            cfg.audio.dev_num,
            cfg.audio.dev_remember,
            cfg.audio.samplerate,
            cfg.audio.bitrate,
            cfg.audio.channel,
            cfg.audio.left_ch,
            cfg.audio.right_ch,
            cfg.audio.codec,
            cfg.audio.mono_to_stereo,
            cfg.audio.aac_overwrite_aot,
            cfg.audio.aac_aot,
            cfg.audio.resample_mode,
            cfg.audio.silence_level,
            cfg.audio.signal_level,
            cfg.audio.disable_dithering,
            cfg.audio.buffer_ms
           );
    
    if (cfg.audio.dev_name != NULL)
        fprintf(cfg_fd, "dev_name = %s\n\n", cfg.audio.dev_name);
    else
        fprintf(cfg_fd, "dev_name = \n\n");


    fprintf(cfg_fd, 
            "[record]\n"
            "bitrate = %d\n"
            "codec = %s\n"
            "start_rec = %d\n"
            "stop_rec = %d\n"
            "rec_after_launch = %d\n"
            "sync_to_hour  = %d\n"
            "split_time = %d\n"
            "filename = %s\n"
            "silence_threshold = %d\n"
            "signal_threshold = %d\n"
            "folder = %s\n\n",
            cfg.rec.bitrate,
            cfg.rec.codec,
            cfg.rec.start_rec,
            cfg.rec.stop_rec,
            cfg.rec.rec_after_launch,
            cfg.rec.sync_to_hour,
            cfg.rec.split_time,
            cfg.rec.filename,
            cfg.rec.silence_threshold,
            cfg.rec.signal_threshold,
            cfg.rec.folder
           );
    
    fprintf(cfg_fd,
            "[tls]\n"
            "cert_file = %s\n"
            "cert_dir = %s\n\n",
            cfg.tls.cert_file != NULL ? cfg.tls.cert_file : "",
            cfg.tls.cert_dir  != NULL ? cfg.tls.cert_dir  : ""
           );
    
    fprintf(cfg_fd,
            "[dsp]\n"
            "equalizer = %d\n"
            "gain1 = %f\n"
            "gain2 = %f\n"
            "gain3 = %f\n"
            "gain4 = %f\n"
            "gain5 = %f\n"
            "gain6 = %f\n"
            "gain7 = %f\n"
            "gain8 = %f\n"
            "gain9 = %f\n"
            "gain10 = %f\n"
			"compressor = %d\n"
            "aggressive_mode = %d\n"
			"threshold = %f\n"
			"ratio = %f\n"
			"attack = %f\n"
			"release = %f\n"
			"makeup_gain = %f\n\n",
            cfg.dsp.equalizer,
            cfg.dsp.gain1,
            cfg.dsp.gain2,
            cfg.dsp.gain3,
            cfg.dsp.gain4,
            cfg.dsp.gain5,
            cfg.dsp.gain6,
            cfg.dsp.gain7,
            cfg.dsp.gain8,
            cfg.dsp.gain9,
            cfg.dsp.gain10,
			cfg.dsp.compressor,
            cfg.dsp.aggressive_mode,
			cfg.dsp.threshold,
			cfg.dsp.ratio,
			cfg.dsp.attack,
			cfg.dsp.release,
			cfg.dsp.makeup_gain
            );

    fprintf(cfg_fd,
            "[gui]\n"
            "attach = %d\n"
            "ontop = %d\n"
            "hide_log_window = %d\n"
            "remember_pos = %d\n"
            "x_pos = %d\n"
            "y_pos = %d\n"
            "lcd_auto = %d\n"
            "start_minimized = %d\n"
            "lang = %d\n"
            "vu_mode = %d\n\n",
            cfg.gui.attach,
            cfg.gui.ontop,
            cfg.gui.hide_log_window,
            cfg.gui.remember_pos,
            cfg.gui.x_pos,
            cfg.gui.y_pos,
            cfg.gui.lcd_auto,
            cfg.gui.start_minimized,
            cfg.gui.lang,
            cfg.gui.vu_mode
           );

    for (i = 0; i < cfg.main.num_of_srv; i++)
    {
        fprintf(cfg_fd, 
                "[%s]\n"
                "address = %s\n"
                "port = %u\n"
                "password = %s\n"
                "type = %d\n"
                "tls = %d\n",
                cfg.srv[i]->name,
                cfg.srv[i]->addr,
                cfg.srv[i]->port,
                cfg.srv[i]->pwd,
                cfg.srv[i]->type,
                cfg.srv[i]->tls
               );
        
        if (cfg.srv[i]->cert_hash != NULL)
            fprintf(cfg_fd, "cert_hash = %s\n", cfg.srv[i]->cert_hash);
        else
            fprintf(cfg_fd, "cert_hash = \n");


        if (cfg.srv[i]->type == ICECAST)
        {
            fprintf(cfg_fd, "mount = %s\n", cfg.srv[i]->mount);
            fprintf(cfg_fd, "usr = %s\n\n", cfg.srv[i]->usr);
        }
        else //Shoutcast has no mountpoint and user
        {
            fprintf(cfg_fd, "mount = (none)\n");
            fprintf(cfg_fd, "usr = (none)\n\n");
        }
        
       

    }

    for(i = 0; i < cfg.main.num_of_icy; i++)
    {
        fprintf(cfg_fd,
                "[%s]\n"
                "pub = %s\n",
                cfg.icy[i]->name,
                cfg.icy[i]->pub
               );

        if(cfg.icy[i]->desc != NULL)
            fprintf(cfg_fd, "description = %s\n", cfg.icy[i]->desc);
        else
            fprintf(cfg_fd, "description = \n");

        if(cfg.icy[i]->genre != NULL)
            fprintf(cfg_fd, "genre = %s\n", cfg.icy[i]->genre);
        else
            fprintf(cfg_fd, "genre = \n");

        if(cfg.icy[i]->url != NULL)
            fprintf(cfg_fd, "url = %s\n", cfg.icy[i]->url);
        else
            fprintf(cfg_fd, "url = \n");

        if(cfg.icy[i]->irc != NULL)
            fprintf(cfg_fd, "irc = %s\n", cfg.icy[i]->irc);
        else
            fprintf(cfg_fd, "irc = \n");

        if(cfg.icy[i]->icq != NULL)
            fprintf(cfg_fd, "icq = %s\n", cfg.icy[i]->icq);
        else
            fprintf(cfg_fd, "icq = \n");

        if(cfg.icy[i]->aim != NULL)
            fprintf(cfg_fd, "aim = %s\n\n", cfg.icy[i]->aim);
        else
            fprintf(cfg_fd, "aim = \n\n");

    }

    fclose(cfg_fd);

    snprintf(info_buf, sizeof(info_buf), _("Config written to %s"), path);
    print_info(info_buf, 0);

    return 0;
}

int cfg_set_values(char *path)
{
    int i;
    char *srv_ent;
    char *icy_ent;
    char *strtok_buf = NULL;
    
    if(path == NULL)
        path = cfg_path;

    if(cfg_parse_file(path) == -1)
        return 1;

    cfg.main.log_file       = cfg_get_str("main", "log_file");
    cfg.main.ic_charset     = cfg_get_str("main", "ic_charset");

    cfg.audio.dev_num       = cfg_get_int("audio", "device");
    cfg.audio.dev_name      = cfg_get_str("audio", "dev_name");
    cfg.audio.dev_remember  = cfg_get_int("audio", "dev_remember");
    cfg.audio.samplerate    = cfg_get_int("audio", "samplerate");
    cfg.audio.resolution    = 16;
    cfg.audio.bitrate       = cfg_get_int("audio", "bitrate");
    cfg.audio.channel       = cfg_get_int("audio", "channel");
    cfg.audio.left_ch       = cfg_get_int("audio", "left_ch");
    cfg.audio.right_ch      = cfg_get_int("audio", "right_ch");
    cfg.audio.codec         = cfg_get_str("audio", "codec");
    cfg.audio.buffer_ms     = cfg_get_int("audio", "buffer_ms");
    cfg.audio.aac_aot       = cfg_get_int("audio", "aac_aot");
    
    cfg.audio.disable_dithering = cfg_get_int("audio", "disable_dithering");
    cfg.audio.pcm_list   = snd_get_devices(&cfg.audio.dev_count);
    
    cfg.audio.mono_to_stereo = cfg_get_int("audio", "mono_to_stereo");
    cfg.audio.resample_mode  = cfg_get_int("audio", "resample_mode");
    cfg.audio.aac_overwrite_aot = cfg_get_int("audio", "aac_overwrite_aot");
    
    if(cfg.audio.dev_num < 0)
        cfg.audio.dev_num = 0;
    
    if (cfg.audio.dev_remember < 0)
        cfg.audio.dev_remember = REMEMBER_BY_ID;
    
    if(cfg.audio.samplerate == -1)
        cfg.audio.samplerate = 44100;

    if(cfg.audio.bitrate == -1)
        cfg.audio.bitrate = 128;

    if(cfg.audio.channel == -1)
        cfg.audio.channel = 2;
    
    if(cfg.audio.left_ch == -1)
        cfg.audio.left_ch = 1;
    
    if(cfg.audio.right_ch == -1)
        cfg.audio.right_ch = 2;
    
    if(cfg.audio.mono_to_stereo == -1)
        cfg.audio.mono_to_stereo = 0;


    if(cfg.audio.codec == NULL)
    {
        cfg.audio.codec = (char*)malloc(5*sizeof(char));
        strcpy(cfg.audio.codec, "mp3");
    }
    else //Make sure that also "opus" and "flac" fits into the codec char array
        cfg.audio.codec = (char*)realloc((char*)cfg.audio.codec, 5*sizeof(char));

    if(!strcmp(cfg.audio.codec, "aac") && g_aac_lib_available == 0) {
        strcpy(cfg.audio.codec, "mp3");
    }


    cfg.audio.silence_level = cfg_get_float("audio", "silence_level");
    if(isnan(cfg.audio.silence_level))
        cfg.audio.silence_level = 50.0; // Will be interpreted as negative value (-50 dB) 
    
    cfg.audio.signal_level = cfg_get_float("audio", "signal_level");
    if(isnan(cfg.audio.signal_level))
        cfg.audio.signal_level = 50.0;
    
        
    if(cfg.audio.buffer_ms == -1)
        cfg.audio.buffer_ms = 50;

    if(cfg.audio.aac_overwrite_aot == -1)
        cfg.audio.aac_overwrite_aot = 0;
    
    if(cfg.audio.disable_dithering == -1)
        cfg.audio.disable_dithering = 0;
    
    
#ifdef HAVE_LIBFDK_AAC
    aac_stream.overwrite_aot = cfg.audio.aac_overwrite_aot;
    aac_rec.overwrite_aot = cfg.audio.aac_overwrite_aot;
#endif

    if(cfg.audio.aac_aot == -1)
        cfg.audio.aac_aot = 5;

    if(cfg.audio.resample_mode == -1)
        cfg.audio.resample_mode = 0; //SRC_SINC_BEST_QUALITY

    // catch illegal audio device number
    if(cfg.audio.dev_num > cfg.audio.dev_count - 1)
        cfg.audio.dev_num = 0;

    cfg.rec.bitrate = cfg_get_int("record", "bitrate");
    cfg.rec.start_rec = cfg_get_int("record", "start_rec");
    cfg.rec.stop_rec = cfg_get_int("record", "stop_rec");
    cfg.rec.rec_after_launch  = cfg_get_int("record", "rec_after_launch");
    cfg.rec.sync_to_hour = cfg_get_int("record", "sync_to_hour");
    cfg.rec.split_time = cfg_get_int("record", "split_time");
    cfg.rec.codec = cfg_get_str("record", "codec");
    cfg.rec.filename = cfg_get_str("record", "filename");
    cfg.rec.folder = cfg_get_str("record", "folder");
    cfg.rec.silence_threshold = cfg_get_int("record", "silence_threshold");
    cfg.rec.signal_threshold = cfg_get_int("record", "signal_threshold");


    if(cfg.rec.bitrate == -1)
        cfg.rec.bitrate = 192;

    if(cfg.rec.start_rec == -1)
        cfg.rec.start_rec = 0;
    
    if(cfg.rec.stop_rec == -1)
        cfg.rec.stop_rec = 0;

    if(cfg.rec.rec_after_launch == -1)
        cfg.rec.rec_after_launch = 0;

    if(cfg.rec.sync_to_hour == -1)
        cfg.rec.sync_to_hour = 0;

    if(cfg.rec.split_time == -1)
        cfg.rec.split_time = 0;

    if(cfg.rec.codec == NULL)
    {
        cfg.rec.codec = (char*)malloc(5*sizeof(char));
        strcpy(cfg.rec.codec, "mp3");
    }
    else
        cfg.rec.codec = (char*)realloc(cfg.rec.codec, 5*sizeof(char));


    if(!strcmp(cfg.rec.codec, "aac") && g_aac_lib_available == 0) {
        strcpy(cfg.rec.codec, "mp3");
    }

    if(cfg.rec.filename == NULL)
    {
        cfg.rec.filename = (char*)malloc(strlen("rec_%Y%m%d-%H%M%S_%i.mp3")+1);
        strcpy(cfg.rec.filename, "rec_%Y%m%d-%H%M%S_%i.mp3");
    }

    if(cfg.rec.folder == NULL)
    {
        cfg.rec.folder = (char*)malloc(3*sizeof(char));
        strcpy(cfg.rec.folder, "./");
    }
    
    if(cfg.rec.silence_threshold == -1)
        cfg.rec.silence_threshold = 0;
    
    if(cfg.rec.signal_threshold == -1)
        cfg.rec.signal_threshold = 0;

    
    cfg.tls.cert_file = cfg_get_str("tls", "cert_file");
    cfg.tls.cert_dir = cfg_get_str("tls", "cert_dir");
    

    cfg.selected_srv = 0;

    cfg.main.num_of_srv = cfg_get_int("main", "num_of_srv");
    if(cfg.main.num_of_srv == -1)
    {
        fl_alert(_("error while parsing config. Illegal value (-1) for num_of_srv.\nbutt is going to close now"));
        exit(1);
    }
    
    if(cfg.main.num_of_srv > 0)
    {
        cfg.main.srv = cfg_get_str("main", "server");
        if(cfg.main.srv == NULL)
        {
            fl_alert(_("error while parsing config. Missing main/server entry.\nbutt is going to close now"));
            exit(1);
        }

        cfg.main.srv_ent = cfg_get_str("main", "srv_ent");
        if(cfg.main.srv_ent == NULL)
        {
            fl_alert(_("error while parsing config. Missing main/srv_ent entry.\nbutt is going to close now"));
            exit(1);
        }

        cfg.srv = (server_t**)malloc(sizeof(server_t*) * cfg.main.num_of_srv);

        for(i = 0; i < cfg.main.num_of_srv; i++)
            cfg.srv[i] = (server_t*)malloc(sizeof(server_t));

        strtok_buf = strdup(cfg.main.srv_ent);
        srv_ent = strtok(strtok_buf, ";");

        for(i = 0; srv_ent != NULL; i++)
        {
            cfg.srv[i]->name = (char*)malloc(strlen(srv_ent)+1);
            snprintf(cfg.srv[i]->name, strlen(srv_ent)+1, "%s", srv_ent);

            cfg.srv[i]->addr = cfg_get_str(srv_ent, "address");
            if(cfg.srv[i]->addr == NULL)
            {
                fl_alert(_("error while parsing config. Missing address entry for server \"%s\"."
                        "\nbutt is going to close now"), srv_ent);
                exit(1);
            }

            cfg.srv[i]->port  = cfg_get_int(srv_ent, "port");
            if(cfg.srv[i]->port == -1)
            {
                fl_alert(_("error while parsing config. Missing port entry for server \"%s\"."
                        "\nbutt is going to close now"), srv_ent);
                exit(1);
            }

            cfg.srv[i]->pwd   = cfg_get_str(srv_ent, "password");
            if(cfg.srv[i]->pwd == NULL)
            {
                fl_alert(_("error while parsing config. Missing password entry for server \"%s\"."
                        "\nbutt is going to close now"), srv_ent);
                exit(1);
            }

            cfg.srv[i]->type  = cfg_get_int(srv_ent, "type");
            if(cfg.srv[i]->type == -1)
            {
                fl_alert(_("error while parsing config. Missing type entry for server \"%s\"."
                        "\nbutt is going to close now"), srv_ent);
                exit(1);
            }

            cfg.srv[i]->mount = cfg_get_str(srv_ent, "mount");
            if((cfg.srv[i]->mount == NULL) && (cfg.srv[i]->type == ICECAST))
            {
                fl_alert(_("error while parsing config. Missing mount entry for server \"%s\"."
                        "\nbutt is going to close now"), srv_ent);
                exit(1);
            }

            cfg.srv[i]->usr = cfg_get_str(srv_ent, "usr");
            if((cfg.srv[i]->usr == NULL) && (cfg.srv[i]->type == ICECAST))
            {
                cfg.srv[i]->usr = (char*)malloc(strlen("source")+1);
                strcpy(cfg.srv[i]->usr, "source");
            }
#ifdef HAVE_LIBSSL            
            cfg.srv[i]->tls  = cfg_get_int(srv_ent, "tls");
#else
            cfg.srv[i]->tls = 0;
#endif
            if( (cfg.srv[i]->tls != 0) && (cfg.srv[i]->tls != 1) )
                cfg.srv[i]->tls = 0;
            
            cfg.srv[i]->cert_hash = cfg_get_str(srv_ent, "cert_hash");
            if( (cfg.srv[i]->cert_hash != NULL) && (strlen(cfg.srv[i]->cert_hash) != 64) )
            {
                free(cfg.srv[i]->cert_hash);
                cfg.srv[i]->cert_hash = NULL;
            }

            if(!strcmp(cfg.srv[i]->name, cfg.main.srv))
                cfg.selected_srv = i;

            srv_ent = strtok(NULL, ";");
        }
        free(strtok_buf);
    }// if(cfg.main.num_of_srv > 0)
    else
        cfg.main.srv = NULL;

    cfg.main.num_of_icy = cfg_get_int("main", "num_of_icy");
    if(cfg.main.num_of_icy == -1)
    {
        fl_alert(_("error while parsing config. Illegal value (-1) for num_of_icy.\nbutt is going to close now"));
        exit(1);
    }
    
    cfg.selected_icy = 0;
    if(cfg.main.num_of_icy > 0)
    {
        cfg.main.icy = cfg_get_str("main", "icy");
        if(cfg.main.icy == NULL)
        {
            fl_alert(_("error while parsing config. Missing main/icy entry.\nbutt is going to close now"));
            exit(1);
        }
        cfg.main.icy_ent = cfg_get_str("main", "icy_ent");          //icy entries
        if(cfg.main.icy_ent == NULL)
        {
            fl_alert(_("error while parsing config. Missing main/icy_ent entry.\nbutt is going to close now"));
            exit(1);
        }

        cfg.icy = (icy_t**)malloc(sizeof(icy_t*) * cfg.main.num_of_icy);

        for(i = 0; i < cfg.main.num_of_icy; i++)
            cfg.icy[i] = (icy_t*)malloc(sizeof(icy_t));

        strtok_buf = strdup(cfg.main.icy_ent);
        icy_ent = strtok(strtok_buf, ";");

        for(i = 0; icy_ent != NULL; i++)
        {
            cfg.icy[i]->name = (char*)malloc(strlen(icy_ent)+1);
            snprintf(cfg.icy[i]->name, strlen(icy_ent)+1, "%s", icy_ent);

            cfg.icy[i]->desc  = cfg_get_str(icy_ent, "description");
            cfg.icy[i]->genre = cfg_get_str(icy_ent, "genre");
            cfg.icy[i]->url   = cfg_get_str(icy_ent, "url");
            cfg.icy[i]->irc   = cfg_get_str(icy_ent, "irc");
            cfg.icy[i]->icq   = cfg_get_str(icy_ent, "icq");
            cfg.icy[i]->aim   = cfg_get_str(icy_ent, "aim");
            cfg.icy[i]->pub   = cfg_get_str(icy_ent, "pub");
            if(cfg.icy[i]->pub == NULL)
            {
                fl_alert(_("error while parsing config. Missing pub entry for icy \"%s\"."
                        "\nbutt is going to close now"), icy_ent);
                exit(1);
            }

            if(!strcmp(cfg.icy[i]->name, cfg.main.icy))
                cfg.selected_icy = i;

            icy_ent = strtok(NULL, ";");
        }
        free(strtok_buf);
    }//if(cfg.main.num_of_icy > 0)
    else
        cfg.main.icy = NULL;

    cfg.main.song_path = cfg_get_str("main", "song_path");
    
    cfg.main.song_prefix = cfg_get_str("main", "song_prefix");
    if (cfg.main.song_prefix != NULL)
        strrpl(&cfg.main.song_prefix, (char*)"%20", (char*)" ", MODE_ALL);

    cfg.main.song_suffix = cfg_get_str("main", "song_suffix");
    if (cfg.main.song_suffix  != NULL)
        strrpl(&cfg.main.song_suffix, (char*)"%20", (char*)" ", MODE_ALL);


    cfg.main.song_update = cfg_get_int("main", "song_update");
    if(cfg.main.song_update == -1)
        cfg.main.song_update = 0; //song update from file is default set to off
    
    cfg.main.song_delay = cfg_get_int("main", "song_delay");
    if(cfg.main.song_delay == -1)
        cfg.main.song_delay = 0;
    
    cfg.main.read_last_line = cfg_get_int("main", "read_last_line");
    if(cfg.main.read_last_line == -1)
        cfg.main.read_last_line = 0; // read first line per default
    
    
    cfg.main.app_update = cfg_get_int("main", "app_update");
    if(cfg.main.app_update == -1)
        cfg.main.app_update = 0; //Default value, off
    
    cfg.main.app_update_service = cfg_get_int("main", "app_update_service");
    if(cfg.main.app_update_service == -1)
        cfg.main.app_update_service = 0; //Default value, first one
    
    cfg.main.app_artist_title_order = cfg_get_int("main", "app_artist_title_order");
    if(cfg.main.app_artist_title_order == -1)
        cfg.main.app_artist_title_order = APP_TITLE_FIRST; //Default value, "Title - Artist"

    
    cfg.main.silence_threshold = cfg_get_int("main", "silence_threshold");
    if(cfg.main.silence_threshold == -1)
        cfg.main.silence_threshold = 0;
    
    cfg.main.signal_threshold = cfg_get_int("main", "signal_threshold");
    if(cfg.main.signal_threshold == -1)
        cfg.main.signal_threshold = 0;

	cfg.main.connect_at_startup = cfg_get_int("main", "connect_at_startup");
	if(cfg.main.connect_at_startup == -1)
		cfg.main.connect_at_startup = 0;
    
    cfg.main.force_reconnecting = cfg_get_int("main", "force_reconnecting");
    if(cfg.main.force_reconnecting == -1)
        cfg.main.force_reconnecting = 0;


	cfg.main.check_for_update = cfg_get_int("main", "check_for_update");
	if(cfg.main.check_for_update == -1)
		cfg.main.check_for_update = 1;
    
    cfg.main.start_agent = cfg_get_int("main", "start_agent");
    if(cfg.main.start_agent == -1)
        cfg.main.start_agent = 0;
    
    cfg.main.minimize_to_tray = cfg_get_int("main", "minimize_to_tray");
    if(cfg.main.minimize_to_tray == -1)
        cfg.main.minimize_to_tray = 0;

	cfg.main.gain = cfg_get_float("main", "gain");
	if(isnan(cfg.main.gain))
		cfg.main.gain = 1.0;
    else if(cfg.main.gain > util_db_to_factor(24))
        cfg.main.gain = util_db_to_factor(24);
    else if(cfg.main.gain < 0)
        cfg.main.gain = util_db_to_factor(-24);
    

    // DSP
    cfg.dsp.equalizer = cfg_get_int("dsp", "equalizer");
    if (cfg.dsp.equalizer == -1)
        cfg.dsp.equalizer = 0;
    
    cfg.dsp.gain1 = cfg_get_float("dsp", "gain1");
    if (isnan(cfg.dsp.gain1))
        cfg.dsp.gain1 = 0.0;
    
    cfg.dsp.gain2 = cfg_get_float("dsp", "gain2");
    if (isnan(cfg.dsp.gain2))
        cfg.dsp.gain2 = 0.0;
    
    cfg.dsp.gain3 = cfg_get_float("dsp", "gain3");
    if (isnan(cfg.dsp.gain3))
        cfg.dsp.gain3 = 0.0;
    
    cfg.dsp.gain4 = cfg_get_float("dsp", "gain4");
    if (isnan(cfg.dsp.gain4))
        cfg.dsp.gain4 = 0.0;
    
    cfg.dsp.gain5 = cfg_get_float("dsp", "gain5");
    if (isnan(cfg.dsp.gain5))
        cfg.dsp.gain5 = 0.0;
    
    cfg.dsp.gain6 = cfg_get_float("dsp", "gain6");
    if (isnan(cfg.dsp.gain6))
        cfg.dsp.gain6 = 0.0;
    
    cfg.dsp.gain7 = cfg_get_float("dsp", "gain7");
    if (isnan(cfg.dsp.gain7))
        cfg.dsp.gain7 = 0.0;
    
    cfg.dsp.gain8 = cfg_get_float("dsp", "gain8");
    if (isnan(cfg.dsp.gain8))
        cfg.dsp.gain8 = 0.0;
    
    cfg.dsp.gain9 = cfg_get_float("dsp", "gain9");
    if (isnan(cfg.dsp.gain9))
        cfg.dsp.gain9 = 0.0;
    
    cfg.dsp.gain10 = cfg_get_float("dsp", "gain10");
    if (isnan(cfg.dsp.gain10)) {
        // Reset all EQ gains if band 10 has no config value. This prevents false values when the user updates from an earlier 5-band EQ to the new 10-band EQ
        cfg.dsp.gain1 = 0.0;
        cfg.dsp.gain2 = 0.0;
        cfg.dsp.gain3 = 0.0;
        cfg.dsp.gain4 = 0.0;
        cfg.dsp.gain5 = 0.0;
        cfg.dsp.gain6 = 0.0;
        cfg.dsp.gain7 = 0.0;
        cfg.dsp.gain8 = 0.0;
        cfg.dsp.gain9 = 0.0;
        cfg.dsp.gain10 = 0.0;
    }
	
	cfg.dsp.compressor = cfg_get_int("dsp", "compressor");
    if (cfg.dsp.compressor == -1)
        cfg.dsp.compressor = 0;

    cfg.dsp.aggressive_mode = cfg_get_int("dsp", "aggressive_mode");
    if (cfg.dsp.aggressive_mode == -1)
        cfg.dsp.aggressive_mode = 0;
    
	cfg.dsp.threshold = cfg_get_float("dsp", "threshold");
    if (isnan(cfg.dsp.threshold))
        cfg.dsp.threshold = -20.0f; // -20 dBFS
        
	cfg.dsp.ratio = cfg_get_float("dsp", "ratio");
    if (isnan(cfg.dsp.ratio))
        cfg.dsp.ratio = 5.0;
	
	cfg.dsp.attack = cfg_get_float("dsp", "attack");
    if (isnan(cfg.dsp.attack))
        cfg.dsp.attack = 0.01; // seconds

	cfg.dsp.release = cfg_get_float("dsp", "release");
    if (isnan(cfg.dsp.release))
        cfg.dsp.release = 1.0; // seconds
	
	cfg.dsp.makeup_gain = cfg_get_float("dsp", "makeup_gain");
    if (isnan(cfg.dsp.makeup_gain))
        cfg.dsp.makeup_gain = 0.0; // 0 dB

    
    //read GUI stuff 
    cfg.gui.attach = cfg_get_int("gui", "attach");
    if(cfg.gui.attach == -1)
        cfg.gui.attach = 0;

    cfg.gui.ontop = cfg_get_int("gui", "ontop");
    if(cfg.gui.ontop == -1)
        cfg.gui.ontop = 0;
    
    cfg.gui.hide_log_window = cfg_get_int("gui", "hide_log_window");
    if(cfg.gui.hide_log_window == -1)
        cfg.gui.hide_log_window = 0;
    
    cfg.gui.remember_pos = cfg_get_int("gui", "remember_pos");
    if(cfg.gui.remember_pos == -1)
        cfg.gui.remember_pos = 0;
    
    cfg.gui.x_pos = cfg_get_int("gui", "x_pos");
    if(cfg.gui.x_pos == -1)
        cfg.gui.x_pos = 0;
    
    cfg.gui.y_pos = cfg_get_int("gui", "y_pos");
    if(cfg.gui.y_pos == -1)
        cfg.gui.y_pos = 0;

    cfg.gui.lcd_auto = cfg_get_int("gui", "lcd_auto");
    if(cfg.gui.lcd_auto == -1)
        cfg.gui.lcd_auto = 0;
   
    cfg.gui.start_minimized = cfg_get_int("gui", "start_minimized");
    if(cfg.gui.start_minimized == -1)
        cfg.gui.start_minimized = 0;
        
    cfg.gui.lang = cfg_get_int("gui", "lang");
    if(cfg.gui.lang == -1)
        cfg.gui.lang = LANG_SYSTEM;
    
    cfg.gui.vu_mode = cfg_get_int("gui", "vu_mode");
    if(cfg.gui.vu_mode == -1)
        cfg.gui.vu_mode = VU_MODE_SOLID;

    //read FLTK related stuff
    cfg.main.bg_color = cfg_get_int("main", "bg_color");
    if(cfg.main.bg_color == -1)
        cfg.main.bg_color = 252645120; //dark gray

    cfg.main.txt_color = cfg_get_int("main", "txt_color");
    if(cfg.main.txt_color == -1)
        cfg.main.txt_color = -256; //white

    return 0;
}

int cfg_create_default(void)
{
    FILE *cfg_fd;
    char *p;
    char def_rec_folder[PATH_MAX];

    cfg_fd = fl_fopen(cfg_path, "wb");
    if(cfg_fd == NULL)
        return 1;

#ifdef WIN32
    p = fl_getenv("USERPROFILE");
    if (p != NULL)
        snprintf(def_rec_folder, PATH_MAX, "%s\\Music\\", p);
    else
        snprintf(def_rec_folder, PATH_MAX, "./");
#elif __APPLE__
    p = fl_getenv("HOME");
    if (p != NULL)
        snprintf(def_rec_folder, PATH_MAX, "%s/Music/", p);
    else
        snprintf(def_rec_folder, PATH_MAX, "./");
#else //UNIX
    p = fl_getenv("HOME");
    if (p != NULL)
        snprintf(def_rec_folder, PATH_MAX, "%s/", p);
    else
        snprintf(def_rec_folder, PATH_MAX, "./");
#endif


    fprintf(cfg_fd, "#This is a configuration file for butt (broadcast using this tool)\n\n");
    fprintf(cfg_fd, 
            "[main]\n"
            "server =\n"
            "icy =\n"
            "num_of_srv = 0\n"
            "num_of_icy = 0\n"
            "srv_ent =\n"
            "icy_ent =\n"
            "song_path =\n"
            "song_update = 0\n"
            "song_delay = 0\n"
            "song_prefix = \n"
            "song_suffix = \n"
            "app_update = 0\n"
            "app_update_service = 0\n"
            "app_artist_title_order = 1\n"
            "read_last_line = 0\n"
            "log_file =\n"
            "gain = 1.0\n"
            "ic_charset =\n"
            "silence_threshhold = 0\n"
            "signal_threshhold = 0\n"
            "check_for_update = 0\n"
            "start_agent = 0\n"
            "minimize_to_tray = 0\n"
            "force_reconnecting = 0\n"
            "connect_at_startup = 0\n\n"
           );

    fprintf(cfg_fd,
            "[audio]\n"
            "device = default\n"
            "samplerate = 44100\n"
            "bitrate = 128\n"
            "channel = 2\n"
            "codec = mp3\n"
            "mono_to_stereo = 0\n"
            "resample_mode = 0\n" //SRC_SINC_BEST_QUALITY
            "aac_aot = 5\n" // aac+ v1
            "aac_overwrite_aot = 0\n"
            "silence_level = 50.0\n"
            "signal_level = 50.0\n"
            "disable_dithering = 0\n"
            "buffer_ms = 50\n\n"
           );

    fprintf(cfg_fd,
            "[record]\n"
            "samplerate = 44100\n"
            "bitrate = 192\n"
            "channel = 2\n"
            "codec = mp3\n"
            "start_rec = 0\n"
            "stop_rec = 0\n"
            "rec_after_launch = 0\n"
            "sync_to_hour = 0\n"
            "split_time = 0\n"
            "filename = rec_%%Y%%m%%d-%%H%%M%%S.mp3\n"
            "silence_threshhold = 0\n"
            "signal_threshhold = 0\n"
            "folder = %s\n\n", def_rec_folder
           );
    
    fprintf(cfg_fd,
            "[tls]\n"
            "cert_file =\n"
            "cert_dir =\n\n"
           );
    
    fprintf(cfg_fd,
            "[dsp]\n"
            "equalizer = 0\n"
            "gain1 = 0.0\n"
            "gain2 = 0.0\n"
            "gain3 = 0.0\n"
            "gain4 = 0.0\n"
            "gain5 = 0.0\n"
		    "compressor = 0\n"
            "aggressive_mode = 0\n"
			"threshold = -20.0\n"
			"ratio = 5\n"
			"attack = 0.01\n"
			"release = 1.0\n"
			"makeup_gain = 0.0\n"		
	);

    fprintf(cfg_fd, 
            "[gui]\n"
            "attach = 0\n"
            "ontop = 0\n"
            "hide_log_window = 0\n"
            "remember_pos = 0\n"
            "x_pos = 0\n"
            "y_pos = 0\n"
            "lcd_auto = 0\n"
            "start_minimized = 0\n"
            "lang = 0\n\n"
            );

    fclose(cfg_fd);

    return 0;
}

