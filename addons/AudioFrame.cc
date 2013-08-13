#include "AudioFrame.hh"
#include "node_buffer.h"
#include "node_internals.h"
#include <cstdlib>

using namespace v8;
using namespace node;

Persistent<FunctionTemplate> AudioFrame::m_constructorTemplate;

AudioFrame::AudioFrame(int p_channels, int p_samplerate, Local<Object> p_buffer) :
    ObjectWrap(),
    m_channels(p_channels),
    m_samplerate(p_samplerate),
    m_buffer(Persistent<Object>::New(p_buffer))
{
}

AudioFrame::AudioFrame(const AudioFrame &p_other) :
    node::ObjectWrap(),
    m_channels(p_other.m_channels),
    m_samplerate(p_other.m_samplerate)
{
    HandleScope scope;

    Buffer *sbuf = Buffer::New(reinterpret_cast<const char*>(p_other.samples()), p_other.numSamples() * sizeof(float));

    // Create fast buffer
    Local<Function> nodeBufferConstructor = Local<Function>::Cast(Context::GetCurrent()->Global()->Get(String::New("Buffer")));
    Handle<Value> bufargv[3] = { sbuf->handle_, Integer::New(p_other.numSamples() * sizeof(float)), Integer::New(0) };
    Local<Object> buf = nodeBufferConstructor->NewInstance(3, bufargv);

    m_buffer = Persistent<Object>::New(buf);
}

AudioFrame::~AudioFrame()
{
    m_buffer.Dispose();
}

float *AudioFrame::samples() const
{
    return reinterpret_cast<float*>(Buffer::Data(m_buffer));
}

unsigned int AudioFrame::numSamples() const
{
    return Buffer::Length(m_buffer) / sizeof(float);
}

void AudioFrame::Initialize(v8::Handle<v8::Object> p_exports)
{
    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    m_constructorTemplate = Persistent<FunctionTemplate>::New(t);
    m_constructorTemplate->SetClassName(String::NewSymbol("AudioFrame"));
    m_constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);

    m_constructorTemplate->InstanceTemplate()->SetAccessor(String::NewSymbol("channels"), ChannelsGetter, 0);
    m_constructorTemplate->InstanceTemplate()->SetAccessor(String::NewSymbol("samplerate"), SamplerateGetter, 0);
    m_constructorTemplate->InstanceTemplate()->SetAccessor(String::NewSymbol("data"), DataGetter, 0);

    p_exports->Set(String::NewSymbol("AudioFrame"), m_constructorTemplate->GetFunction());
}

AudioFrame *AudioFrame::New(int p_channels, int p_samplerate, float *p_samples, unsigned int p_samplelen)
{
    HandleScope scope;

    Buffer *sbuf = Buffer::New(reinterpret_cast<const char*>(p_samples), p_samplelen);
    free(p_samples);

    // Create fast buffer
    Local<Function> nodeBufferConstructor = Local<Function>::Cast(Context::GetCurrent()->Global()->Get(String::New("Buffer")));
    Handle<Value> bufargv[3] = { sbuf->handle_, Integer::New(p_samplelen), Integer::New(0) };
    Local<Object> buf = nodeBufferConstructor->NewInstance(3, bufargv);

    // Call constructor
    Handle<Value> constargv[3] = { Integer::New(p_channels), Integer::New(p_samplerate), buf };
    Local<Object> obj = m_constructorTemplate->GetFunction()->NewInstance(3, constargv);

    return ObjectWrap::Unwrap<AudioFrame>(obj);
}

Handle<Value> AudioFrame::New(const Arguments &p_args)
{
    HandleScope scope;

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

    return p_args.This();
}

Handle<Value> AudioFrame::ChannelsGetter(Local<String> p_property, const AccessorInfo &p_info)
{
    AudioFrame *self = ObjectWrap::Unwrap<AudioFrame>(p_info.Holder());
    return Integer::New(self->m_channels);
}

Handle<Value> AudioFrame::SamplerateGetter(Local<String> p_property, const AccessorInfo &p_info)
{
    AudioFrame *self = ObjectWrap::Unwrap<AudioFrame>(p_info.Holder());
    return Integer::New(self->m_samplerate);
}

Handle<Value> AudioFrame::DataGetter(Local<String> p_property, const AccessorInfo &p_info)
{
    AudioFrame *self = ObjectWrap::Unwrap<AudioFrame>(p_info.Holder());
    return self->m_buffer;
}