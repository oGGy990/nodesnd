#ifndef OGG_VORBIS_ENCODER_HH
#define OGG_VORBIS_ENCODER_HH

#include "../Encoder.hh"
#include <vorbis/vorbisenc.h>

class OggVorbisEncoder : public Encoder
{
public:
    ~OggVorbisEncoder();

    static void Initialize(v8::Handle<v8::Object> p_exports);

protected:
    virtual bool init();
    virtual void finish();

    virtual void encode(float *p_samples, unsigned int p_sampleslen, unsigned char **p_data, unsigned int *p_len);

private:
    OggVorbisEncoder(int p_channels, int p_samplerate, int p_bitrate);

    static v8::Handle<v8::Value> New(const v8::Arguments &p_args);

    ogg_stream_state    m_oggsstate;
    ogg_page            m_oggpage;
    ogg_packet          m_oggpacket;
    vorbis_info         m_vorbisinfo;
    vorbis_comment      m_vorbiscomment;
    vorbis_dsp_state    m_vorbisdsp;
    vorbis_block        m_vorbisblock;

    bool                m_initialized;
    bool                m_headers;

    unsigned long       m_samples_in_page;
    unsigned long       m_samples_last_page;
    unsigned long       m_max_samples_per_page;
};

#endif // OGG_VORBIS_ENCODER_HH