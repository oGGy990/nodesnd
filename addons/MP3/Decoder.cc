#include "Decoder.hh"
#include <node_buffer.h>
#include <sstream>
#include <iostream>

using namespace v8;

MP3Decoder::MP3Decoder() :
    Decoder(),
    m_decoder(0)
{
    int err;
    m_decoder = mpg123_new(0, &err);

    if(m_decoder)
    {
        if(MPG123_OK != mpg123_format_none(m_decoder))
        {
            std::cerr << "MP3Decoder: " << "Failed to clear output formats" << std::endl;
        }
        if(MPG123_OK != mpg123_format(m_decoder, 8000, MPG123_STEREO | MPG123_MONO, MPG123_ENC_FLOAT_32))
        {
            std::cerr << "MP3Decoder: " << "Failed to set 8000Hz format" << std::endl;
        }
        if(MPG123_OK != mpg123_format(m_decoder, 44100, MPG123_STEREO | MPG123_MONO, MPG123_ENC_FLOAT_32))
        {
            std::cerr << "MP3Decoder: " << "Failed to set 44100Hz format" << std::endl;
        }
        if(MPG123_OK != mpg123_format(m_decoder, 48000, MPG123_STEREO | MPG123_MONO, MPG123_ENC_FLOAT_32))
        {
            std::cerr << "MP3Decoder: " << "Failed to set 48000Hz format" << std::endl;
        }

        if(MPG123_OK != mpg123_open_feed(m_decoder))
        {
            std::cerr << "MP3Decoder: " << "Failed to open feed" << std::endl;
        }
    }
}

MP3Decoder::~MP3Decoder()
{
    finishProcessing();

    if(m_decoder)
    {
        mpg123_close(m_decoder);
        mpg123_delete(m_decoder);
        m_decoder = 0;
    }
}

void MP3Decoder::Initialize(Handle<Object> p_exports)
{
    mpg123_init();

    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("MP3Decoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    tpl->Inherit(Template());

    Local<Function> func = tpl->GetFunction();

    NODE_SET_METHOD(func, "types", Types);

    Persistent<Function> constructor(Isolate::GetCurrent(), func);
    p_exports->Set(String::NewSymbol("MP3Decoder"), func);
}

void MP3Decoder::New(const v8::FunctionCallbackInfo<Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    MP3Decoder *obj = new MP3Decoder();
    obj->Wrap(p_args.This());

    p_args.GetReturnValue().Set(p_args.This());
}

void MP3Decoder::Types(const v8::FunctionCallbackInfo<Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    Local<Array> myTypes = Array::New(1);
    myTypes->Set(0, String::New("audio/mpeg"));

    p_args.GetReturnValue().Set(myTypes);
}

void MP3Decoder::decode(unsigned char *p_data, unsigned int p_len)
{
    if(!m_decoder)
        return;

    // Feed the decoder all available data
    int ret = mpg123_feed(m_decoder, p_data, p_len);

    // Some error while feeding?
    if(ret != MPG123_OK)
    {
        std::stringstream ss("Error while feeding: ");
        ss << mpg123_plain_strerror(ret);

        emitError(ss.str());
        return;
    }

    while(1)
    {
        float *outbuffer = new float[1024];

        size_t decoded = 0;
        ret = mpg123_read(m_decoder, reinterpret_cast<unsigned char*>(outbuffer), 1024 * sizeof(float), &decoded);

        if(decoded > 0)
            pushSamples(outbuffer, decoded / sizeof(float));
        else
            delete[] outbuffer;

        switch(ret)
        {
        case MPG123_OK:
            continue;

        case MPG123_NEW_FORMAT:
            {
                long rate;
                int channels, enc;
                mpg123_getformat(m_decoder, &rate, &channels, &enc);
                mpg123_info(m_decoder, &m_info);

                setSamplerate(rate);
                setChannels(channels);
                continue;
            }

        case MPG123_NEED_MORE:
            return;

        default:
            {
                std::stringstream ss("Error while decoding: ");
                if(MPG123_ERR == ret)
                    ss << mpg123_strerror(m_decoder);
                else
                    ss << mpg123_plain_strerror(ret);

                emitError(ss.str());
                return;
            }
        }
    }
}