// flac encoding functions for butt
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
#include <stdlib.h>
#include <string.h>

#include "flac_encode.h"

FLAC__uint64 g_bytes_written = 0;

static void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data);

static FLAC__StreamEncoderWriteStatus ogg_stream_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data);

static FLAC__bool flac_enc_init_ogg_stream(flac_enc *flac);

uint8_t *g_enc_p = NULL;
uint8_t *g_hdr_p;
uint8_t g_hdr[256];

int g_hdr_written;
int g_hdr_size = 0;

int flac_enc_init(flac_enc *flac)
{
    FLAC__bool ok = true;
    g_hdr_written = 0;
   
    if((flac->encoder = FLAC__stream_encoder_new()) == NULL) 
    {
        printf("ERROR: allocating encoder\n");
        return 1;
    }

    ok &= FLAC__stream_encoder_set_verify(flac->encoder, false);
    ok &= FLAC__stream_encoder_set_compression_level(flac->encoder, 5);
    ok &= FLAC__stream_encoder_set_channels(flac->encoder, flac->channel);
    ok &= FLAC__stream_encoder_set_bits_per_sample(flac->encoder, 16);
    ok &= FLAC__stream_encoder_set_sample_rate(flac->encoder, flac->samplerate);
    ok &= FLAC__stream_encoder_set_total_samples_estimate(flac->encoder, 0);
    
    g_hdr_p = g_hdr;
    
    if (flac->enc_type == FLAC_ENC_TYPE_STREAM)
        ok &= flac_enc_init_ogg_stream(flac);

    return ok == true ? 0 : 1;
}


int flac_enc_init_FILE(flac_enc *flac, FILE *fout)
{
    FLAC__StreamEncoderInitStatus init_status;

    init_status = FLAC__stream_encoder_init_FILE(flac->encoder, fout, progress_callback, /*client_data=*/NULL);
    if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) 
    {
        fprintf(stderr, "ERROR: initializing encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
        return 1;
    }

    return 0;
}

FLAC__bool flac_enc_init_ogg_stream(flac_enc *flac)
{
    FLAC__StreamEncoderInitStatus init_status;
    
    FLAC__stream_encoder_set_ogg_serial_number(flac->encoder, rand());
    
    init_status = FLAC__stream_encoder_init_ogg_stream(flac->encoder, NULL, ogg_stream_callback, NULL, NULL, NULL, NULL);
    if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
    {
        fprintf(stderr, "ERROR: initializing encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
        return false;
    }
    
    return true;
}

int flac_enc_encode(flac_enc *flac, short *pcm_buf, int samples_per_chan, int channel)
{
    int i;
    int samples_left;
    int chunk_size;
    int pcm_32[8192];
    
    int samples_written = 0;
    
    chunk_size = 2048;
    if (chunk_size > samples_per_chan)
        chunk_size = samples_per_chan;

    samples_left = samples_per_chan;
    
    while(samples_left > 0)
    {
        // Convert 16bit samples to 32bit samples 
        for(i = 0; i < chunk_size * channel; i++)
            pcm_32[i] = (int)pcm_buf[i+samples_written];

        FLAC__stream_encoder_process_interleaved(flac->encoder, pcm_32, chunk_size);

        samples_written += chunk_size*channel;
        samples_left -= chunk_size;

        if(samples_left < chunk_size)
            chunk_size = samples_left;
    }

    return 0;
}

int flac_enc_encode_stream(flac_enc *flac, short *pcm_buf, uint8_t *enc_buf, int samples_per_chan, int channel)
{
    int i;
    int samples_left;
    int chunk_size;
    int bytes_written;
    int pcm_32[8192];
    
    g_enc_p = enc_buf;
    
    if (g_hdr_written == 0)
    {
        memcpy(g_enc_p, g_hdr, g_hdr_size);
        g_enc_p += g_hdr_size;
        g_hdr_written = 1;
    }
    
    int samples_written = 0;
    
    chunk_size = 2048;
    if (chunk_size > samples_per_chan)
        chunk_size = samples_per_chan;
    
    samples_left = samples_per_chan;
    
    
    while(samples_left > 0)
    {
        // Convert 16bit samples to 32bit samples
        for(i = 0; i < chunk_size * channel; i++)
            pcm_32[i] = (int)pcm_buf[i+samples_written];
        
        FLAC__stream_encoder_process_interleaved(flac->encoder, pcm_32, chunk_size);
        
        samples_written += chunk_size*channel;
        samples_left -= chunk_size;
        
        if(samples_left < chunk_size)
            chunk_size = samples_left;
    }
    
    bytes_written = (int)(g_enc_p - enc_buf);
    
    return bytes_written;
}

int flac_enc_reinit(flac_enc *flac)
{
    flac_enc_close(flac);
    return flac_enc_init(flac);
}

void flac_enc_close(flac_enc *flac)
{
    if(flac->encoder != NULL)
    {
	FLAC__stream_encoder_delete(flac->encoder);
    }

    flac->encoder = NULL;
}


void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data)
{
    g_bytes_written = bytes_written;
}

FLAC__StreamEncoderWriteStatus ogg_stream_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data)
{
 
    if (current_frame == 0)
    {
        // assemble header
        memcpy(g_hdr_p, buffer, bytes);
        g_hdr_p += bytes;
        g_hdr_size += bytes;
        return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
    }
    
    if (g_enc_p != NULL)
    {
        memcpy(g_enc_p, buffer, bytes);
        g_enc_p += bytes;
    }
   
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLAC__uint64 flac_enc_get_bytes_written(void)
{
    return g_bytes_written;
}
