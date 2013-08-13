#ifndef OGG_VORBIS_DECODER_HH
#define OGG_VORBIS_DECODER_HH

#include <vorbis/codec.h>
#include "../Decoder.hh"

class OggVorbisDecoder : public Decoder
{
public:
    ~OggVorbisDecoder();

    static void Initialize(v8::Handle<v8::Object> p_exports);

private:
    OggVorbisDecoder();

    virtual void decode(unsigned char *p_data, unsigned int p_len, float **p_samples, unsigned int *p_sampleslen);

    static v8::Handle<v8::Value> New(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> Types(const v8::Arguments &p_args);

    ogg_sync_state      m_oggsync;
    ogg_stream_state    m_oggstream;
    ogg_page            m_oggpage;
    ogg_packet          m_oggpacket;
    int                 m_serialno;

    vorbis_info         m_vorbisinfo;
    vorbis_comment      m_vorbiscomment;
    vorbis_dsp_state    m_vorbisdsp;
    vorbis_block        m_vorbisblock;

    int                 m_numpackets;
    int                 m_channels;
    int                 m_bitrate;
    int                 m_rate;
    int                 m_initialized;
};

#endif // OGG_VORBIS_DECODER_HH