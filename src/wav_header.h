// wav header functions for butt
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

#ifndef WAV_HEADER_H
#define WAV_HEADER_H

#include <stdio.h>


typedef union {

    char data[44];

    struct wav_header {
        char riff_id[4];        //"RIFF"
        int riff_size;          //file_length - 8
        char riff_format[4];    //"WAVE"

        char fmt_id[4];         //"FMT "(the space is essential
        int fmt_size;           //fmt data size (16 bytes)
        short fmt_format;       //format (PCM = 1)
        short fmt_channel;      //1 = mono; 2 = stereo
        int fmt_samplerate;     //...
        int fmt_byte_rate;      //samplerate * block_align
        short fmt_block_align;  //channels * bits_per_sample / 8
        short fmt_bps;          //bits per sample = 16

        char data_id[4];        //"data"
        int data_size;          //file_length - 44
    }wav;

}wav_hdr;

int wav_write_header(FILE *fd, short ch, int srate, short bps);


#endif
