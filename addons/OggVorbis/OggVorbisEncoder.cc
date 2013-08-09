#include "OggVorbisEncoder.hh"
#include <node_buffer.h>
#include <cstring>
#include <cstdlib>

using namespace v8;

OggVorbisEncoder::OggVorbisEncoder(int p_channels, int p_samplerate, int p_bitrate) :
    m_initialized(false),
    m_headers(false),
    m_channels(p_channels),
    m_samplerate(p_samplerate),
    m_bitrate(p_bitrate),
    m_samples_in_page(0),
    m_samples_last_page(0),
    m_max_samples_per_page(0)
{

}

OggVorbisEncoder::~OggVorbisEncoder()
{
    finish();
}

bool OggVorbisEncoder::init()
{
    vorbis_info_init(&m_vorbisinfo);

    if(vorbis_encode_setup_managed(&m_vorbisinfo, m_channels, m_samplerate, -1, m_bitrate, -1) ||
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
    m_max_samples_per_page = m_samplerate;

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

    tpl->PrototypeTemplate()->Set(String::NewSymbol("init"), FunctionTemplate::New(Init));
    tpl->PrototypeTemplate()->Set(String::NewSymbol("finish"), FunctionTemplate::New(Finish));
    tpl->PrototypeTemplate()->Set(String::NewSymbol("encode"), FunctionTemplate::New(encode));

    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    p_exports->Set(String::NewSymbol("OggVorbisEncoder"), constructor);
}

Handle<Value> OggVorbisEncoder::New(const Arguments &p_args)
{
    HandleScope scope;

    int channels = p_args[0]->Int32Value();
    int samplerate = p_args[1]->Int32Value();
    int bitrate = p_args[2]->Int32Value() * 1000;

    OggVorbisEncoder *obj = new OggVorbisEncoder(channels, samplerate, bitrate);
    obj->Wrap(p_args.This());

    return p_args.This();
}

Handle<Value> OggVorbisEncoder::Init(const Arguments &p_args)
{
    HandleScope scope;

    OggVorbisEncoder *self = node::ObjectWrap::Unwrap<OggVorbisEncoder>(p_args.This());

    return scope.Close(Boolean::New(self->init()));
}

Handle<Value> OggVorbisEncoder::Finish(const Arguments &p_args)
{
    HandleScope scope;

    OggVorbisEncoder *self = node::ObjectWrap::Unwrap<OggVorbisEncoder>(p_args.This());

    self->finish();

    return scope.Close(Undefined());
}

Handle<v8::Value> OggVorbisEncoder::encode(const Arguments &p_args)
{
    HandleScope scope;

    // Get arguments
    OggVorbisEncoder *self = node::ObjectWrap::Unwrap<OggVorbisEncoder>(p_args.This());
    const float *data = reinterpret_cast<const float*>(node::Buffer::Data(p_args[0]->ToObject()));
    unsigned int len = node::Buffer::Length(p_args[0]->ToObject()) / sizeof(float);

    if(!self->m_initialized)
    {
        return scope.Close(Null());
    }

    unsigned char *outbuffer = 0;
    unsigned int outlen = 0;

    // Write headers
    if(!self->m_headers)
    {
        ogg_packet header, header_comm, header_code;
        vorbis_analysis_headerout(&self->m_vorbisdsp, &self->m_vorbiscomment, &header, &header_comm, &header_code);

        ogg_stream_packetin(&self->m_oggsstate, &header);
        ogg_stream_packetin(&self->m_oggsstate, &header_comm);
        ogg_stream_packetin(&self->m_oggsstate, &header_code);

        while(ogg_stream_flush(&self->m_oggsstate, &self->m_oggpage) > 0)
        {
            outbuffer = reinterpret_cast<unsigned char*>(realloc(outbuffer, outlen + self->m_oggpage.header_len + self->m_oggpage.body_len));
            memcpy(outbuffer + outlen, self->m_oggpage.header, self->m_oggpage.header_len);
            memcpy(outbuffer + outlen + self->m_oggpage.header_len, self->m_oggpage.body, self->m_oggpage.body_len);

            outlen += self->m_oggpage.header_len + self->m_oggpage.body_len;
        }

        self->m_headers = true;
    }
    else // Encode
    {
        // Feed the samples to the encoder
        long num_samples = len / self->m_channels;
        float **buffer = vorbis_analysis_buffer(&self->m_vorbisdsp, num_samples);
        for(unsigned int ch = 0; ch < self->m_channels; ch++)
        {
            for(int s = 0; s < num_samples; s++)
            {
                buffer[ch][s] = *(data + ch + s * self->m_channels);
            }
        }
        vorbis_analysis_wrote(&self->m_vorbisdsp, num_samples);
        self->m_samples_in_page += num_samples;

        // Encode...
        while(vorbis_analysis_blockout(&self->m_vorbisdsp, &self->m_vorbisblock) == 1)
        {
            vorbis_analysis(&self->m_vorbisblock, NULL);
            vorbis_bitrate_addblock(&self->m_vorbisblock);

            while(vorbis_bitrate_flushpacket(&self->m_vorbisdsp, &self->m_oggpacket))
            {
                ogg_stream_packetin(&self->m_oggsstate, &self->m_oggpacket);
            }
        }

        // Emit the encoded ogg stream pages
        int eos = 0;
        while(!eos)
        {
            // Force a new page if we got too many samples in one page
            // (we're live encoding and need to send something)
            if(self->m_samples_in_page > self->m_max_samples_per_page)
            {
                if(ogg_stream_flush(&self->m_oggsstate, &self->m_oggpage) == 0)
                    break;
            }
            else if(ogg_stream_pageout(&self->m_oggsstate, &self->m_oggpage) == 0)
                break;

            self->m_samples_in_page -= ogg_page_granulepos(&self->m_oggpage) - self->m_samples_last_page;
            self->m_samples_last_page = ogg_page_granulepos(&self->m_oggpage);

            outbuffer = reinterpret_cast<unsigned char*>(realloc(outbuffer, outlen + self->m_oggpage.header_len + self->m_oggpage.body_len));
            memcpy(outbuffer + outlen, self->m_oggpage.header, self->m_oggpage.header_len);
            memcpy(outbuffer + outlen + self->m_oggpage.header_len, self->m_oggpage.body, self->m_oggpage.body_len);

            outlen += self->m_oggpage.header_len + self->m_oggpage.body_len;

            if(ogg_page_eos(&self->m_oggpage))
                eos = 1;
        }
    }

    // Build output buffer
    node::Buffer *sbuf = node::Buffer::New(reinterpret_cast<const char*>(outbuffer), outlen);
    free(outbuffer);

    Local<Function> nodeBufferConstructor = Local<Function>::Cast(Context::GetCurrent()->Global()->Get(v8::String::New("Buffer")));
    Handle<Value> argv[3] = { sbuf->handle_, Integer::New(outlen), Integer::New(0) };
    Local<Object> buf = nodeBufferConstructor->NewInstance(3, argv);

    return scope.Close(buf);
}