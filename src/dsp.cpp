//
//  dsp.cpp
//  butt
//
//  Created by Melchor Garau Madrigal on 16/2/16.
//  Copyright © 2016 Daniel Noethen. All rights reserved.
//

#include "dsp.hpp"
#include <samplerate.h>
#include <cmath>
#include <algorithm>
#include "Biquad.h"
#include "cfg.h"

DSPEffects::DSPEffects(uint32_t frames, uint8_t channels, uint32_t sampleRate) : samplerate(sampleRate) {
	chans = (channels == 2) ? 2 : 1;
    dsp_size = frames * channels;
    dsp_buff = new float[dsp_size];
	
    band1l = new Biquad(bq_type_peak, 32.0 / double(samplerate), 2, cfg.dsp.gain1);
    band2l = new Biquad(bq_type_peak, 64.0 / double(samplerate), 2, cfg.dsp.gain2);
    band3l = new Biquad(bq_type_peak, 125.0 / double(samplerate), 2, cfg.dsp.gain3);
    band4l = new Biquad(bq_type_peak, 250.0 / double(samplerate), 2, cfg.dsp.gain4);
    band5l = new Biquad(bq_type_peak, 500.0 / double(samplerate), 2, cfg.dsp.gain5);
    band6l = new Biquad(bq_type_peak, 1000.0 / double(samplerate), 2, cfg.dsp.gain6);
    band7l = new Biquad(bq_type_peak, 2000.0 / double(samplerate), 2, cfg.dsp.gain7);
    band8l = new Biquad(bq_type_peak, 4000.0 / double(samplerate), 2, cfg.dsp.gain8);
    band9l = new Biquad(bq_type_peak, 8000.0 / double(samplerate), 2, cfg.dsp.gain9);
    band10l = new Biquad(bq_type_peak, 16000.0 / double(samplerate), 2, cfg.dsp.gain10);
	
	band1r = new Biquad(bq_type_peak, 32.0 / double(samplerate), 2, cfg.dsp.gain1);
    band2r = new Biquad(bq_type_peak, 64.0 / double(samplerate), 2, cfg.dsp.gain2);
    band3r = new Biquad(bq_type_peak, 125.0 / double(samplerate), 2, cfg.dsp.gain3);
    band4r = new Biquad(bq_type_peak, 250.0 / double(samplerate), 2, cfg.dsp.gain4);
    band5r = new Biquad(bq_type_peak, 500.0 / double(samplerate), 2, cfg.dsp.gain5);
    band6r = new Biquad(bq_type_peak, 1000.0 / double(samplerate), 2, cfg.dsp.gain6);
    band7r = new Biquad(bq_type_peak, 2000.0 / double(samplerate), 2, cfg.dsp.gain7);
    band8r = new Biquad(bq_type_peak, 4000.0 / double(samplerate), 2, cfg.dsp.gain8);
    band9r = new Biquad(bq_type_peak, 8000.0 / double(samplerate), 2, cfg.dsp.gain9);
    band10r = new Biquad(bq_type_peak, 16000.0 / double(samplerate), 2, cfg.dsp.gain10);
}

bool DSPEffects::hasToProcessSamples() {
    return cfg.main.gain != 1 || cfg.dsp.equalizer || cfg.dsp.compressor;
}

void DSPEffects::processSamples(short *samples) {
    if(dsp_buff == NULL)
        return;
    	
    if(cfg.dsp.equalizer) {
        band1l->setPeakGain(cfg.dsp.gain1);
        band2l->setPeakGain(cfg.dsp.gain2);
        band3l->setPeakGain(cfg.dsp.gain3);
        band4l->setPeakGain(cfg.dsp.gain4);
        band5l->setPeakGain(cfg.dsp.gain5);
        band6l->setPeakGain(cfg.dsp.gain6);
        band7l->setPeakGain(cfg.dsp.gain7);
        band8l->setPeakGain(cfg.dsp.gain8);
        band9l->setPeakGain(cfg.dsp.gain9);
        band10l->setPeakGain(cfg.dsp.gain10);
		
		band1r->setPeakGain(cfg.dsp.gain1);
        band2r->setPeakGain(cfg.dsp.gain2);
        band3r->setPeakGain(cfg.dsp.gain3);
        band4r->setPeakGain(cfg.dsp.gain4);
        band5r->setPeakGain(cfg.dsp.gain5);
        band6r->setPeakGain(cfg.dsp.gain6);
        band7r->setPeakGain(cfg.dsp.gain7);
        band8r->setPeakGain(cfg.dsp.gain8);
        band9r->setPeakGain(cfg.dsp.gain9);
        band10r->setPeakGain(cfg.dsp.gain10);
    }

    src_short_to_float_array(samples, dsp_buff, dsp_size);
	
	 if(cfg.main.gain != 1) {
		 for(uint32_t sample = 0; sample < dsp_size; sample += chans) {
            dsp_buff[sample] *= cfg.main.gain;
			
			if (chans == 2) {
				dsp_buff[sample+1] *= cfg.main.gain;
			}
		}
	}
	
	if (cfg.dsp.compressor) {
		attack_const = expf(-1.0f / (cfg.dsp.attack * samplerate));
		release_const = expf(-1.0f / (cfg.dsp.release * samplerate));
		
        if (cfg.dsp.aggressive_mode == 1) {
            lowpass_const = 0;
        }
        else {
            float lowpass_time = std::min(cfg.dsp.attack, cfg.dsp.release);
            lowpass_time = std::max(0.002f, lowpass_time);
            lowpass_const = expf(-1.0f / (lowpass_time * samplerate));
        }
        compress();
    }
    
	for(uint32_t sample = 0; sample < dsp_size; sample += chans) {
        if(cfg.dsp.equalizer) {
            float s = band10l->process(dsp_buff[sample]);
            s = band9l->process(s);
            s = band8l->process(s);
            s = band7l->process(s);
            s = band6l->process(s);
            s = band5l->process(s);
            s = band4l->process(s);
            s = band3l->process(s);
            s = band2l->process(s);
            dsp_buff[sample] = band1l->process(s);
			
			if (chans == 2) {
				float s = band10r->process(dsp_buff[sample+1]);
                s = band9r->process(s);
                s = band8r->process(s);
                s = band7r->process(s);
                s = band6r->process(s);
                s = band5r->process(s);
                s = band4r->process(s);
                s = band3r->process(s);
                s = band2r->process(s);
				dsp_buff[sample+1] = band1r->process(s);
			}
        }
    }
    src_float_to_short_array(dsp_buff, samples, dsp_size);
}

DSPEffects::~DSPEffects() {
    delete[] dsp_buff;
	
    delete band1l;
    delete band2l;
    delete band3l;
    delete band4l;
    delete band5l;
    delete band6l;
    delete band7l;
    delete band8l;
    delete band9l;
    delete band10l;

	
	delete band1r;
    delete band2r;
    delete band3r;
    delete band4r;
    delete band5r;
    delete band6r;
    delete band7r;
    delete band8r;
    delete band9r;
    delete band10r;
}


// loosely based on https://openaudio.blogspot.com/2017/01/basic-dynamic-range-compressor.html
// https://github.com/chipaudette/OpenAudio_ArduinoLibrary/blob/master/AudioEffectCompressor_F32.h
void DSPEffects::compress() {
	float one_minus_attack_const = 1.0f - attack_const;
	float one_minus_release_const = 1.0f - release_const;
	float c1 = lowpass_const, c2 = 1.0f - c1;
	
	float ratio = cfg.dsp.ratio;
    
	if (ratio < 0.001)
        ratio = 1.0f;
    
    is_compressing = false;
		
    for (uint32_t sample = 0; sample < dsp_size; sample += chans) {
      
        // calculate instantaneous signal power
        // (square signal and average channels)
        float power = dsp_buff[sample] * dsp_buff[sample];
        if (chans == 2) {
            power += dsp_buff[sample+1] * dsp_buff[sample+1];
            power /= 2;
        }
		
		// low-pass filter
		power = c1*prev_power + c2*power;
		prev_power = power;
        
        // Do not apply compression if signal power is nearly zero to prevent -inf results for log10 calculations
        if (power < 1.0E-13)
            continue;
		        
		// convert to dB
		float power_dB = 10.0f * std::log10(power);
		
        // calculate instantaneous target gain
		float above_threshold = power_dB - cfg.dsp.threshold;
		float gain_dB = (above_threshold / ratio) - above_threshold;
        
        if (above_threshold > 0)
            is_compressing = true;

		if (gain_dB > 0.0f)
            gain_dB = 0.0f;
        		
		// smooth gain by attack and release
		if (std::isnan(prev_gain_dB))
            prev_gain_dB = 0.0f;
        
		if (gain_dB < prev_gain_dB) {
			// attack phase
			gain_dB = attack_const*prev_gain_dB + one_minus_attack_const*gain_dB;
		} else {
			// release phase
			gain_dB = release_const*prev_gain_dB + one_minus_release_const*gain_dB;
		}
		prev_gain_dB = gain_dB;
		
		// convert to linear gain
        float gain_linear = pow(10, (gain_dB+cfg.dsp.makeup_gain) / 20.0f);
        
		// apply gain to samples
		dsp_buff[sample] *= gain_linear;
		if (chans == 2) {
			dsp_buff[sample+1] *= gain_linear;
		}
	}
	
	// limit the amount that the state of the smoothing filter can go towards -inf
	if (prev_power < (1.0E-13))
        prev_power = 1.0E-13; // never go less than -130 dbFS
    
}




void DSPEffects::reset_compressor()
{
    prev_power = 1.0;
    prev_gain_dB = 0.0;
}
