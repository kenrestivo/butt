//
//  dsp.hpp
//  butt
//
//  Created by Melchor Garau Madrigal on 16/2/16.
//  Copyright © 2016 Daniel Nöthen. All rights reserved.
//

#ifndef dsp_hpp
#define dsp_hpp

#include <stdint.h>

class DSPEffects {
private:
    float* dsp_buff;
    uint32_t dsp_size;
    uint32_t samplerate;
	uint8_t chans;
    class Biquad* band1l, *band2l, *band3l, *band4l, *band5l, *band6l, *band7l, *band8l, *band9l, *band10l;
    class Biquad* band1r, *band2r, *band3r, *band4r, *band5r, *band6r, *band7r, *band8r, *band9r, *band10r;
	float attack_const, release_const, lowpass_const;
	float prev_power = 1.0;
	float prev_gain_dB = 0.0;
	
	void compress();

public:
    DSPEffects(uint32_t frames, uint8_t channels, uint32_t sampleRate);
    ~DSPEffects();
    
    bool is_compressing;
    bool hasToProcessSamples();
    void processSamples(short* samples);
    void reset_compressor();
};

#endif /* dsp_hpp */
