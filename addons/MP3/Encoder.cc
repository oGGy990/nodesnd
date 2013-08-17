#include "Encoder.hh"
#include <sstream>

using namespace v8;

MP3Encoder::MP3Encoder(int p_channels, int p_samplerate, int p_bitrate) :
    Encoder(p_channels, p_samplerate, p_bitrate),
    m_lame(0)
{
}

MP3Encoder::~MP3Encoder()
{
}

bool MP3Encoder::init()
{
    m_lame = lame_init();
    if(!m_lame)
    {
        return false;
    }

    lame_set_in_samplerate(m_lame, samplerate());
    lame_set_num_channels(m_lame, channels());
    lame_set_quality(m_lame, 2);
    lame_set_brate(m_lame, bitrate() / 1000);

    if(-1 == lame_init_params(m_lame))
    {
        lame_close(m_lame);
        m_lame = 0;
        return false;
    }

    return true;
}

void MP3Encoder::finish()
{
    if(!m_lame)
        return;

    lame_close(m_lame);
    m_lame = 0;
}

void MP3Encoder::Initialize(Handle<Object> p_exports)
{
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("MP3Encoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    tpl->Inherit(Template());

    tpl->PrototypeTemplate()->Set(
        String::NewSymbol("type"),
        String::New("audio/mpeg"),
        static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete)
    );

    Persistent<Function> constructor(Isolate::GetCurrent(), tpl->GetFunction());
    p_exports->Set(String::NewSymbol("MP3Encoder"), tpl->GetFunction());
}

void MP3Encoder::New(const v8::FunctionCallbackInfo<Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    int channels = p_args[0]->Int32Value();
    int samplerate = p_args[1]->Int32Value();
    int bitrate = p_args[2]->Int32Value() * 1000;

    MP3Encoder *obj = new MP3Encoder(channels, samplerate, bitrate);
    obj->Wrap(p_args.This());

    p_args.GetReturnValue().Set(p_args.This());
}

void MP3Encoder::encode(float *p_samples, unsigned int p_numsamples)
{
    if(!m_lame)
        return;

    unsigned int outlen = 1.25 * (p_numsamples / channels()) + 7200;
    unsigned char *outbuffer = new unsigned char[outlen];

    int ret = lame_encode_buffer_interleaved_ieee_float(
        m_lame,
        p_samples,
        p_numsamples / channels(),
        outbuffer,
        outlen
    );

    if(ret > 0)
    {
        pushFrame(outbuffer, ret);
        return;
    }
    else if(ret < 0)
    {
        std::stringstream ss("Error while decoding: LAME failed with Code ");
        ss << ret;
        emitError(ss.str());
    }

    delete[] outbuffer;
}