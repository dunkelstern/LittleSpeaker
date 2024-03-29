#ifndef LITTLESPEAKER_AUDIOOUTPUTFILTER3BANDEQ_H
#define LITTLESPEAKER_AUDIOOUTPUTFILTER3BANDEQ_H

#include "AudioOutput.h"
#include "FixedPoints.h"

#define PRECISION SFixed<2, 29>

typedef struct _EQState {
  // Filter #1 (Low band)
  PRECISION  lf;       // Frequency
  PRECISION  f1p0;     // Poles ...

  // Filter #2 (High band)
  PRECISION  hf;       // Frequency
  PRECISION  f2p0;     // Poles ...

  // Gain Controls
  PRECISION  lg;       // low  gain
  PRECISION  mg;       // mid  gain
  PRECISION  hg;       // high gain
} EQState;


class AudioOutputFilter3BandEQ : public AudioOutput
{
  public:
    AudioOutputFilter3BandEQ(AudioOutput *sink, int lowFreq = 880, int highFreq = 5000);
    virtual ~AudioOutputFilter3BandEQ() override;
    virtual bool SetRate(int hz) override;
    virtual bool SetBitsPerSample(int bits) override;
    virtual bool SetChannels(int chan) override;
    virtual bool SetGain(float f) override;
    virtual bool begin() override;
    virtual bool ConsumeSample(int16_t sample[2]) override;
    virtual bool stop() override;

    void setBandGains(float low, float mid, float high);
    void processBuffer(int16_t *samples, int len, int stride, int channels);

  protected:
    AudioOutput *sink;

  private:
    EQState state[2]; // 2 channels
    int lowFreq = 880;
    int highFreq = 5000;
    uint8_t channels = 2;
    int sample_rate;

};

#endif

