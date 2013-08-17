#include "Encoder.hh"
#include <cstring>
#include <cstdlib>

using namespace v8;

OggVorbisEncoder::OggVorbisEncoder(int p_channels, int p_samplerate, int p_bitrate) :
    Encoder(p_channels, p_samplerate, p_bitrate),
    m_initialized(false),
    m_headers(false),
    m_samples_in_page(0),
    m_samples_last_page(0),
    m_max_samples_per_page(0)
{

}

OggVorbisEncoder::~OggVorbisEncoder()
{
}

bool OggVorbisEncoder::init()
{
    vorbis_info_init(&m_vorbisinfo);

    if(vorbis_encode_setup_managed(&m_vorbisinfo, channels(), samplerate(), -1, bitrate(), -1) ||
            vorbis_encode_ctl(&m_vorbisinfo, OV_ECTL_RATEMANAGE2_SET, NULL) ||
            vorbis_encode_setup_init(&m_vorbisinfo)) // VBR BR
    {
        vorbis_info_clear(&m_vorbisinfo);
        return false;
    }

    vorbis_comment_init(&m_vorbiscomment);
    vorbis_comment_add_tag(&m_vorbiscomment, "ENCODER", "NodeJS Ogg/Vorbis Module");

    if(vorbis_analysis_init(&m_vorbisdsp, &m_vorbisinfo) < 0)
    {
        //dLog->error("OGG/Vorbis", "Failed to initialize analysis unit");
        vorbis_comment_clear(&m_vorbiscomment);
        vorbis_info_clear(&m_vorbisinfo);
        return false;
    }
    if(vorbis_block_init(&m_vorbisdsp, &m_vorbisblock) < 0)
    {
        //dLog->error("OGG/Vorbis", "Failed to initialize block unit");
        vorbis_dsp_clear(&m_vorbisdsp);
        vorbis_comment_clear(&m_vorbiscomment);
        vorbis_info_clear(&m_vorbisinfo);
        return false;
    }

    ogg_stream_init(&m_oggsstate, rand());

    m_headers = false;

    m_samples_in_page = 0;
    m_samples_last_page = 0;
    m_max_samples_per_page = samplerate();

    m_initialized = true;

    return true;
}

void OggVorbisEncoder::finish()
{
    if(!m_initialized)
        return;

    ogg_stream_clear(&m_oggsstate);
    vorbis_block_clear(&m_vorbisblock);
    vorbis_dsp_clear(&m_vorbisdsp);
    vorbis_comment_clear(&m_vorbiscomment);
    vorbis_info_clear(&m_vorbisinfo);

    m_initialized = false;
}

void OggVorbisEncoder::Initialize(Handle<Object> p_exports)
{
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("OggVorbisEncoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    tpl->Inherit(Template());

    tpl->PrototypeTemplate()->Set(
        String::NewSymbol("type"),
        String::New("application/ogg"),
        static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete)
    );

    Persistent<Function> constructor(Isolate::GetCurrent(), tpl->GetFunction());
    p_exports->Set(String::NewSymbol("OggVorbisEncoder"), tpl->GetFunction());
}

void OggVorbisEncoder::New(const v8::FunctionCallbackInfo<Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    int channels = p_args[0]->Int32Value();
    int samplerate = p_args[1]->Int32Value();
    int bitrate = p_args[2]->Int32Value() * 1000;

    OggVorbisEncoder *obj = new OggVorbisEncoder(channels, samplerate, bitrate);
    obj->Wrap(p_args.This());

    p_args.GetReturnValue().Set(p_args.This());
}

void OggVorbisEncoder::encode(float *p_samples, unsigned int p_numsamples)
{
    if(!m_initialized)
        return;

    // Write headers
    if(!m_headers)
    {
        ogg_packet header, header_comm, header_code;
        vorbis_analysis_headerout(&m_vorbisdsp, &m_vorbiscomment, &header, &header_comm, &header_code);

        ogg_stream_packetin(&m_oggsstate, &header);
        ogg_stream_packetin(&m_oggsstate, &header_comm);
        ogg_stream_packetin(&m_oggsstate, &header_code);

        while(ogg_stream_flush(&m_oggsstate, &m_oggpage) > 0)
        {
            unsigned char *outbuffer = new unsigned char[m_oggpage.header_len + m_oggpage.body_len];
            memcpy(outbuffer, m_oggpage.header, m_oggpage.header_len);
            memcpy(outbuffer + m_oggpage.header_len, m_oggpage.body, m_oggpage.body_len);

            pushFrame(outbuffer, m_oggpage.header_len + m_oggpage.body_len);
        }

        m_headers = true;
    }
    else // Encode
    {
        // Feed the samples to the encoder
        long num_samples = p_numsamples / channels();
        float **buffer = vorbis_analysis_buffer(&m_vorbisdsp, num_samples);
        for(int ch = 0; ch < channels(); ch++)
        {
            for(int s = 0; s < num_samples; s++)
            {
                buffer[ch][s] = *(p_samples + ch + s * channels());
            }
        }
        vorbis_analysis_wrote(&m_vorbisdsp, num_samples);
        m_samples_in_page += num_samples;

        // Encode...
        while(vorbis_analysis_blockout(&m_vorbisdsp, &m_vorbisblock) == 1)
        {
            vorbis_analysis(&m_vorbisblock, NULL);
            vorbis_bitrate_addblock(&m_vorbisblock);

            while(vorbis_bitrate_flushpacket(&m_vorbisdsp, &m_oggpacket))
            {
                ogg_stream_packetin(&m_oggsstate, &m_oggpacket);
            }
        }

        // Emit the encoded ogg stream pages
        int eos = 0;
        while(!eos)
        {
            // Force a new page if we got too many samples in one page
            // (we're live encoding and need to send something)
            if(m_samples_in_page > m_max_samples_per_page)
            {
                if(ogg_stream_flush(&m_oggsstate, &m_oggpage) == 0)
                    break;
            }
            else if(ogg_stream_pageout(&m_oggsstate, &m_oggpage) == 0)
                break;

            m_samples_in_page -= ogg_page_granulepos(&m_oggpage) - m_samples_last_page;
            m_samples_last_page = ogg_page_granulepos(&m_oggpage);

            unsigned char *outbuffer = new unsigned char[m_oggpage.header_len + m_oggpage.body_len];
            memcpy(outbuffer, m_oggpage.header, m_oggpage.header_len);
            memcpy(outbuffer + m_oggpage.header_len, m_oggpage.body, m_oggpage.body_len);

            pushFrame(outbuffer, m_oggpage.header_len + m_oggpage.body_len);

            if(ogg_page_eos(&m_oggpage))
                eos = 1;
        }
    }
}