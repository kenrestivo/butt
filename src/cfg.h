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

#ifndef CFG_H
#define CFG_H

#include "port_audio.h"
#include "parseconfig.h"

enum {

    SHOUTCAST = 0,
    ICECAST = 1
};

enum {
    CHOICE_STEREO = 0,
    CHOICE_MONO = 1
};

enum {
    CHOICE_MP3 = 0,
    CHOICE_OGG = 1,
    CHOICE_OPUS = 2,
    CHOICE_AAC = 3,
    CHOICE_FLAC = 4,
    CHOICE_WAV = 5
};

enum {
    LANG_SYSTEM = 0,
    LANG_DE = 1,
    LANG_EN = 2,
    LANG_FR = 3
};

enum {
    APP_ARTIST_FIRST = 0,
    APP_TITLE_FIRST = 1
};

enum {
    VU_MODE_GRADIENT = 0,
    VU_MODE_SOLID = 1
};

enum {
    REMEMBER_BY_ID = 0,
    REMEMBER_BY_NAME = 1
};

extern const char CONFIG_FILE[];
typedef struct
{
    char *name;
    char *addr;
    char *pwd;
    char *mount;        //mountpoint for icecast server
    char *usr;          //user for icecast server
    char *cert_hash;    //sha256 hash of trusted certificate
    int port;
    int type;           //SHOUTCAST or ICECAST
    int tls;            //use tls: 0 = no, 1 = yes
}server_t;


typedef struct
{
        char *name;
        char *desc; //description
        char *genre;
        char *url;
        char *irc;
        char *icq;
        char *aim;
        char *pub;
}icy_t;


typedef struct
{
    int selected_srv;
    int selected_icy;

    struct
    {
        char *srv;
        char *icy;
        char *srv_ent;
        char *icy_ent;
        char *song;
        char *song_path;
        FILE *song_fd;
        char *song_prefix;
        char *song_suffix;
        int song_delay;
        int song_update;   //1 = song info will be read from file
        int read_last_line;
        int app_update;
        int app_update_service;
        int app_artist_title_order;
        int num_of_srv;
        int num_of_icy;
        int bg_color, txt_color;
        int connect_at_startup;
        int force_reconnecting;
        int silence_threshold; // timeout duration of automatic stream/record stop
        int signal_threshold;  // timeout duration of automatic stream/record start
        int check_for_update;
        int start_agent;
        int minimize_to_tray;
        float gain;
        char *log_file;
        char *ic_charset;
    }main;

    struct
    {
        int dev_count;
        int dev_num;
        char *dev_name;
        int dev_remember;       // Remember device by ID or Name
        snd_dev_t **pcm_list;
        int samplerate;
        int resolution;
        int channel;
        int left_ch;
        int right_ch;
        int bitrate;
        int mono_to_stereo;
        int buffer_ms;
        int resample_mode;
        int aac_aot;
        float silence_level;
        float signal_level;
        int aac_overwrite_aot;
        int disable_dithering;
        char *codec;
    }audio;

    struct
    {
        int channel;
        int bitrate;
        int quality;
        int samplerate;
        char *codec;
        char *filename;
        char *folder;
        char *path;
        char *path_fmt;
        FILE *fd;
        int start_rec;
        int stop_rec;
        int rec_after_launch;
        int split_time;
        int sync_to_hour;
        int silence_threshold;
        int signal_threshold;
    }rec;
    
    struct
    {
        char *cert_file;
        char *cert_dir;
    }tls;
    
    struct
    {
        int equalizer;
        double gain1, gain2, gain3, gain4, gain5, gain6, gain7, gain8, gain9, gain10;
		int compressor;
        int aggressive_mode;
		double threshold, ratio, attack, release, makeup_gain;
    }dsp;

    struct
    {
        int attach;
        int ontop;
        int lcd_auto;
        int hide_log_window;
        int remember_pos;
        int x_pos, y_pos;
        int lang;
        int vu_mode;
        int start_minimized;
    }gui;

    server_t **srv;
    icy_t **icy;

}config_t;



extern char *cfg_path;      //Path to config file
extern config_t cfg;        //Holds config parameters

int cfg_write_file(char *path); //Writes current config_t struct to path or cfg_path if path is NULL
int cfg_set_values(char *path); //Reads config file from path or cfg_path if path is NULL and fills the config_t struct
int cfg_create_default(void);   //Creates a default config file, if there isn't one yet

#endif

