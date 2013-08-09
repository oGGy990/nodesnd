#ifndef OGG_VORBIS_ENCODER_HH
#define OGG_VORBIS_ENCODER_HH

#include <node.h>
#include <vorbis/vorbisenc.h>

class OggVorbisEncoder : public node::ObjectWrap
{
public:
    static void Initialize(v8::Handle<v8::Object> p_exports);

private:
    OggVorbisEncoder(int p_channels, int p_samplerate, int p_bitrate);
    ~OggVorbisEncoder();

    bool init();
    void finish();

    static v8::Handle<v8::Value> New(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> Init(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> Finish(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> encode(const v8::Arguments &p_args);

    ogg_stream_state    m_oggsstate;
    ogg_page            m_oggpage;
    ogg_packet          m_oggpacket;
    vorbis_info         m_vorbisinfo;
    vorbis_comment      m_vorbiscomment;
    vorbis_dsp_state    m_vorbisdsp;
    vorbis_block        m_vorbisblock;

    bool                m_initialized;
    bool                m_headers;

    unsigned int        m_channels;
    unsigned int        m_samplerate;
    unsigned int        m_bitrate;

    unsigned long       m_samples_in_page;
    unsigned long       m_samples_last_page;
    unsigned long       m_max_samples_per_page;
};

#endif // OGG_VORBIS_ENCODER_HH