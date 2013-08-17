#include "AudioFrame.hh"
#include "node_buffer.h"
#include "node_internals.h"

using namespace v8;
using namespace node;

Persistent<Function> AudioFrame::m_constructorTemplate;

AudioFrame::AudioFrame(int p_channels, int p_samplerate, Local<Object> p_buffer) :
    ObjectWrap(),
    m_channels(p_channels),
    m_samplerate(p_samplerate),
    m_buffer(Isolate::GetCurrent(), p_buffer)
{
}

AudioFrame::AudioFrame(const AudioFrame &p_other) :
    node::ObjectWrap(),
    m_channels(p_other.m_channels),
    m_samplerate(p_other.m_samplerate)
{
    HandleScope scope;

    Local<Object> buf = Buffer::New(reinterpret_cast<char*>(p_other.samples()), p_other.numSamples() * sizeof(float));
    m_buffer.Reset(Isolate::GetCurrent(), buf);
}

AudioFrame::~AudioFrame()
{
    m_buffer.Dispose();
}

float *AudioFrame::samples() const
{
    return reinterpret_cast<float*>(Buffer::Data(const_cast<AudioFrame*>(this)->m_buffer));
}

unsigned int AudioFrame::numSamples() const
{
    return Buffer::Length(const_cast<AudioFrame*>(this)->m_buffer) / sizeof(float);
}

void AudioFrame::Initialize(v8::Handle<v8::Object> p_exports)
{
    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->SetClassName(String::NewSymbol("AudioFrame"));
    t->InstanceTemplate()->SetInternalFieldCount(1);

    t->InstanceTemplate()->SetAccessor(String::NewSymbol("channels"), ChannelsGetter, 0);
    t->InstanceTemplate()->SetAccessor(String::NewSymbol("samplerate"), SamplerateGetter, 0);
    t->InstanceTemplate()->SetAccessor(String::NewSymbol("data"), DataGetter, 0);

    p_exports->Set(String::NewSymbol("AudioFrame"), t->GetFunction());

    m_constructorTemplate.Reset(Isolate::GetCurrent(), t->GetFunction());
}

AudioFrame *AudioFrame::New(int p_channels, int p_samplerate, float *p_samples, unsigned int p_samplelen)
{
    HandleScope scope(Isolate::GetCurrent());

    Local<Object> buf = Buffer::Use(reinterpret_cast<char*>(p_samples), p_samplelen);

    // Call constructor
    Handle<Value> constargv[3] = {
        Integer::New(p_channels, Isolate::GetCurrent()),
        Integer::New(p_samplerate, Isolate::GetCurrent()),
        buf
    };
    Local<Function> constr = Local<Function>::New(Isolate::GetCurrent(), m_constructorTemplate);
    Local<Object> obj = constr->NewInstance(3, constargv);

    return ObjectWrap::Unwrap<AudioFrame>(obj);
}

void AudioFrame::New(const v8::FunctionCallbackInfo<Value>& p_args)
{
    HandleScope scope(Isolate::GetCurrent());

    if(p_args.Length() < 1)
    {
        return ThrowRangeError("Too less arguments");
    }
    else if(p_args.Length() == 1)
    {
        if(!p_args[0]->IsObject())
        {
            return ThrowTypeError("Bad argument");
        }

        AudioFrame *obj = new AudioFrame(*ObjectWrap::Unwrap<AudioFrame>(p_args[0]->ToObject()));
        obj->Wrap(p_args.This());
    }
    else if(p_args.Length() == 3)
    {
        if(!p_args[0]->IsNumber())
        {
            return ThrowTypeError("Bad argument");
        }
        else if(!p_args[1]->IsNumber())
        {
            return ThrowTypeError("Bad argument");
        }
        else if(!p_args[2]->IsObject())
        {
            return ThrowTypeError("Bad argument");
        }

        int channels = p_args[0]->ToInteger()->Int32Value();
        int samplerate = p_args[1]->ToInteger()->Int32Value();
        AudioFrame *obj = new AudioFrame(channels, samplerate, p_args[2]->ToObject());
        obj->Wrap(p_args.This());
    }
    else
    {
        return ThrowRangeError("Bad number of arguments");
    }

    return p_args.GetReturnValue().Set(p_args.This());
}

void AudioFrame::ChannelsGetter(Local<String> p_property, const PropertyCallbackInfo<Value> &p_info)
{
    AudioFrame *self = ObjectWrap::Unwrap<AudioFrame>(p_info.Holder());
    return p_info.GetReturnValue().Set(Integer::New(self->m_channels, p_info.GetIsolate()));
}

void AudioFrame::SamplerateGetter(Local<String> p_property, const PropertyCallbackInfo<Value> &p_info)
{
    AudioFrame *self = ObjectWrap::Unwrap<AudioFrame>(p_info.Holder());
    return p_info.GetReturnValue().Set(Integer::New(self->m_samplerate, p_info.GetIsolate()));
}

void AudioFrame::DataGetter(Local<String> p_property, const PropertyCallbackInfo<Value> &p_info)
{
    AudioFrame *self = ObjectWrap::Unwrap<AudioFrame>(p_info.Holder());
    return p_info.GetReturnValue().Set(self->m_buffer);
}