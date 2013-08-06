#ifndef OGG_VORBIS_DECODER_HH
#define OGG_VORBIS_DECODER_HH

#include <node.h>
#include <vorbis/codec.h>

class OggVorbisDecoder : public node::ObjectWrap
{
public:
    static void Init(v8::Handle<v8::Object> p_exports);

private:
    OggVorbisDecoder();
    ~OggVorbisDecoder();

    static v8::Handle<v8::Value> New(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> types(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> decode(const v8::Arguments &p_args);

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