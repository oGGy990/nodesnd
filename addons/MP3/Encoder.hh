#ifndef MP3_ENCODER_HH
#define MP3_ENCODER_HH

#include "../Encoder.hh"
#include <lame/lame.h>

class MP3Encoder : public Encoder
{
public:
    ~MP3Encoder();

    static void Initialize(v8::Handle<v8::Object> p_exports);

protected:
    virtual bool init();
    virtual void finish();

    virtual void encode(float *p_samples, unsigned int p_numsamples);

private:
    MP3Encoder(int p_channels, int p_samplerate, int p_bitrate);

    static void New(const v8::FunctionCallbackInfo<v8::Value> &p_args);

    lame_global_flags *m_lame;
};

#endif // OGG_VORBIS_ENCODER_HH