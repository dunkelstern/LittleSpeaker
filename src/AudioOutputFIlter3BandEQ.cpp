#include <Arduino.h>
#include "AudioOutputFilter3BandEQ.h"

static const PRECISION vsa = PRECISION(1.0 / 536870911.0);   // Very small amount (Denormal Fix)

AudioOutputFilter3BandEQ::AudioOutputFilter3BandEQ(AudioOutput *sink, int lowFreq, int highFreq) {
    this->sink = sink;
    this->lowFreq = lowFreq;
    this->highFreq = highFreq;

    Serial.printf("EQ initialized with low = %d, high = %d\n", lowFreq, highFreq);

    for (int i = 0; i < 2; i++) {
        EQState *state = this->state + i;

        state->lf = PRECISION(0);
        state->f1p0 = PRECISION(0);
        state->f1p1 = PRECISION(0);
        state->f1p2 = PRECISION(0);
        state->f1p3 = PRECISION(0);

        state->hf = PRECISION(0);
        state->f2p0 = PRECISION(0);
        state->f2p1 = PRECISION(0);
        state->f2p2 = PRECISION(0);
        state->f2p3 = PRECISION(0);

        state->sdm1 = PRECISION(0);  
        state->sdm2 = PRECISION(0);
        state->sdm3 = PRECISION(0);

        // Set Low/Mid/High gains to unity
        state->lg = PRECISION(1.0);
        state->mg = PRECISION(1.0);
        state->hg = PRECISION(1.0);
    }

    Serial.printf("Value test: vsa = %d.%d (%f)\n", vsa.getInteger(), vsa.getFraction(), (float)vsa);
}


AudioOutputFilter3BandEQ::~AudioOutputFilter3BandEQ() {
}

bool AudioOutputFilter3BandEQ::SetRate(int hz) {
    for (int i = 0; i < 2; i++) {
        // Calculate filter cutoff frequencies
        this->state[i].lf = PRECISION(2.0f * sin(M_PI * ((double)lowFreq / (double)hz)));
        this->state[i].hf = PRECISION(2.0f * sin(M_PI * ((double)highFreq / (double)hz)));
    }

    Serial.printf("Low freq %0.2f (%d.%d), high freq %0.2f (%d.%d)\n",
        (float)this->state[0].lf, this->state[0].lf.getInteger(), this->state[0].lf.getFraction(),
        (float)this->state[0].hf, this->state[0].hf.getInteger(), this->state[0].hf.getFraction()
    );

    this->sample_rate = hz;
    return sink->SetRate(hz);
}

void AudioOutputFilter3BandEQ::setBandGains(float low, float mid, float high) {
    for(int i = 0; i < 2; i++) {
        this->state[i].lg = PRECISION((float)low);
        this->state[i].mg = PRECISION((float)mid);
        this->state[i].hg = PRECISION((float)high);
    }
    Serial.printf("EQ gains set to %0.2f (%d.%d), %0.2f (%d.%d), %0.2f (%d.%d)\n",
        (float)this->state[0].lg, this->state[0].lg.getInteger(), this->state[0].lg.getFraction(),
        (float)this->state[0].mg, this->state[0].mg.getInteger(), this->state[0].mg.getFraction(),
        (float)this->state[0].hg, this->state[0].hg.getInteger(), this->state[0].hg.getFraction()
    );
}

bool AudioOutputFilter3BandEQ::SetBitsPerSample(int bits) {
    return sink->SetBitsPerSample(bits);
}

bool AudioOutputFilter3BandEQ::SetChannels(int channels) {
    this->channels = channels;
    return sink->SetChannels(channels);
}

bool AudioOutputFilter3BandEQ::SetGain(float gain) {
    return sink->SetGain(gain);
}

bool AudioOutputFilter3BandEQ::begin() {
    return sink->begin();
}

bool AudioOutputFilter3BandEQ::ConsumeSample(int16_t sample[2]) {
    PRECISION l(0), m(0), h(0);      // Low / Mid / High - Sample Values
    int32_t result;

    // for(int i = 0; i < this->channels; i++) {
    int i = 0;  
        EQState *es = this->state + i;
        PRECISION s = PRECISION((double)sample[i] / (double)(INT16_MAX + 1));
 
        // Filter #1 (lowpass)

        es->f1p0  += (es->lf * (s        - es->f1p0)) + vsa;
        es->f1p1  += (es->lf * (es->f1p0 - es->f1p1));
        es->f1p2  += (es->lf * (es->f1p1 - es->f1p2));
        es->f1p3  += (es->lf * (es->f1p2 - es->f1p3));

        l          = es->f1p3;

        // Filter #2 (highpass)

        es->f2p0  += (es->hf * (s        - es->f2p0)) + vsa;
        es->f2p1  += (es->hf * (es->f2p0 - es->f2p1));
        es->f2p2  += (es->hf * (es->f2p1 - es->f2p2));
        es->f2p3  += (es->hf * (es->f2p2 - es->f2p3));

        h          = es->sdm3 - es->f2p3;

        // Calculate midrange (signal - (low + high))

        m          = es->sdm3 - (h + l);

        // Scale, Combine and store

        l         *= es->lg;
        m         *= es->mg;
        h         *= es->hg;

        // Shuffle history buffer

        es->sdm3   = es->sdm2;
        es->sdm2   = es->sdm1;
        es->sdm1   = s;

        result = (double)(l + m + h) * (double)INT16_MAX;

        // clip
        if (result < INT16_MIN) {
            Serial.printf("Clip under %d < %d\n", result, INT16_MIN);
            result = INT16_MIN;
        } else if (result > INT16_MAX) {
            Serial.printf("Clip over %d > %d\n", result, INT16_MAX);
            result = INT16_MAX;
        }

        // save
        sample[i] = (int16_t)result;
//    }

    // if (this->channels == 1) {
        sample[1] = sample[0];
    // }
    return sink->ConsumeSample(sample);
}

bool AudioOutputFilter3BandEQ::stop() {
    return sink->stop();
}
