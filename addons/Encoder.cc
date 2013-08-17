#include "Encoder.hh"
#include "AudioFrame.hh"
#include <node_internals.h>
#include <node_buffer.h>
#include <iostream>

using namespace v8;
using namespace node;

Encoder::Encoder(int p_channels, int p_samplerate, int p_bitrate) :
    ObjectWrap(),
    m_channels(p_channels),
    m_samplerate(p_samplerate),
    m_bitrate(p_bitrate),
    m_endThread(true)
{
    uv_mutex_init(&m_inLock);
    uv_mutex_init(&m_outLock);
    uv_mutex_init(&m_errorLock);

    uv_cond_init(&m_inCondition);

    uv_async_init(uv_default_loop(), &m_outSignal, outSignaller);
    m_outSignal.data = this;

    uv_async_init(uv_default_loop(), &m_errorSignal, errorSignaller);
    m_errorSignal.data = this;
}

Encoder::~Encoder()
{
    finishProcessing();
    finish();

    uv_close(reinterpret_cast<uv_handle_t*>(&m_errorSignal), 0);
    uv_close(reinterpret_cast<uv_handle_t*>(&m_outSignal), 0);

    uv_cond_destroy(&m_inCondition);

    uv_mutex_destroy(&m_errorLock);
    uv_mutex_destroy(&m_outLock);
    uv_mutex_destroy(&m_inLock);
}

void Encoder::finishProcessing()
{
    if(!m_endThread)
    {
        // Tell thread to stop
        m_endThread = true;
        uv_cond_signal(&m_inCondition);

        // Wait for thread
        uv_thread_join(&m_thread);

        while(!m_in.empty())
        {
            m_in.front()->frame.Dispose();
            delete m_in.front();
            m_in.pop();
        }

        while(!m_out.empty())
        {
            EncodeTask *task = m_out.front();
            m_out.pop();

            task->frame.Dispose();

            std::list<EncodeOutput>::const_iterator iter;
            for(iter = task->out.begin(); iter != task->out.end(); ++iter)
                delete[] iter->data;

            delete task;
        }
    }
}

bool Encoder::init()
{
    return true;
}

void Encoder::finish()
{
}

void Encoder::encode(float *p_samples, unsigned int p_sampleslen)
{
    std::cerr << "Encoder::encode(): IMPLEMENT ME!" << std::endl;
}

void Encoder::pushFrame(unsigned char *p_data, unsigned int p_length)
{
    if(!p_data || !p_length)
        return;

    EncodeOutput out;
    out.data = p_data;
    out.length = p_length;

    // Only called from encode thread while task
    // is detached from main thread, so there's no
    // need to protect this action.
    m_task->out.push_back(out);
}

void Encoder::emitError(const std::string &p_err)
{
    uv_mutex_lock(&m_errorLock);
    m_errors.push(p_err);
    uv_mutex_unlock(&m_errorLock);

    uv_async_send(&m_errorSignal);
}

Local<FunctionTemplate> Encoder::Template()
{
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("Encoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "init", Init);
    NODE_SET_PROTOTYPE_METHOD(tpl, "finish", Finish);
    NODE_SET_PROTOTYPE_METHOD(tpl, "encode", Encode);

    tpl->InstanceTemplate()->SetAccessor(String::NewSymbol("channels"), ChannelsGetter, 0);
    tpl->InstanceTemplate()->SetAccessor(String::NewSymbol("samplerate"), SamplerateGetter, 0);
    tpl->InstanceTemplate()->SetAccessor(String::NewSymbol("bitrate"), BitrateGetter, 0);

    tpl->PrototypeTemplate()->Set(
        String::NewSymbol("type"),
        String::Empty(),
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete)
    );

    return tpl;
}

void Encoder::Initialize(Handle<Object> p_exports)
{
    Local<FunctionTemplate> tpl = Template();

    Persistent<Function> constructor(Isolate::GetCurrent(), tpl->GetFunction());
    p_exports->Set(String::NewSymbol("Encoder"), tpl->GetFunction());
}

void Encoder::New(const v8::FunctionCallbackInfo<Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    if(p_args.Length() < 3 ||
        !p_args[0]->IsNumber() ||
        !p_args[1]->IsNumber() ||
        !p_args[2]->IsNumber())
    {
        return ThrowTypeError("Bad argument");
    }

    Encoder *obj = new Encoder(
        p_args[0]->ToInteger()->Int32Value(),
        p_args[1]->ToInteger()->Int32Value(),
        p_args[2]->ToInteger()->Int32Value()
    );
    obj->Wrap(p_args.This());

    p_args.GetReturnValue().Set(p_args.This());
}

void Encoder::Init(const v8::FunctionCallbackInfo<Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_args.This());

    // Init subclass
    bool ok = self->init();

    if(ok)
    {
        self->m_endThread = false;

        uv_thread_create(&self->m_thread, threadRunner, self);
    }

    p_args.GetReturnValue().Set(Boolean::New(ok));
}

void Encoder::Finish(const v8::FunctionCallbackInfo<Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_args.This());

    // End thread
    self->finishProcessing();

    // End subclass
    self->finish();
}

void Encoder::Encode(const v8::FunctionCallbackInfo<Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    if(1 < p_args.Length() || !p_args[0]->IsObject())
    {
        return ThrowTypeError("Bad argument");
    }

    // Get arguments
    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_args.This());
    AudioFrame *frame = ObjectWrap::Unwrap<AudioFrame>(p_args[0]->ToObject());

    EncodeTask *task = new EncodeTask();
    task->frame.Reset(p_args.GetIsolate(), p_args[0]->ToObject());
    task->samples = frame->samples();
    task->numsamples = frame->numSamples();

    uv_mutex_lock(&self->m_inLock);

    self->m_in.push(task);
    uv_cond_signal(&self->m_inCondition);

    uv_mutex_unlock(&self->m_inLock);
}

void Encoder::ChannelsGetter(Local<String> p_property, const PropertyCallbackInfo<Value> &p_info)
{
    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_info.Holder());
    p_info.GetReturnValue().Set(Integer::New(self->m_channels, p_info.GetIsolate()));
}

void Encoder::SamplerateGetter(Local<String> p_property, const PropertyCallbackInfo<Value> &p_info)
{
    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_info.Holder());
    p_info.GetReturnValue().Set(Integer::New(self->m_samplerate, p_info.GetIsolate()));
}

void Encoder::BitrateGetter(Local<String> p_property, const PropertyCallbackInfo<Value> &p_info)
{
    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_info.Holder());
    p_info.GetReturnValue().Set(Integer::New(self->m_bitrate, p_info.GetIsolate()));
}

void Encoder::threadRunner(void *p_data)
{
    Encoder *self = reinterpret_cast<Encoder*>(p_data);

    while(!self->m_endThread)
    {
        // Get next input chunk
        uv_mutex_lock(&self->m_inLock);

        // Wait until we got some input
        if(self->m_in.empty())
        {
            uv_cond_wait(&self->m_inCondition, &self->m_inLock);

            // The thread may have to end when woken up
            if(self->m_endThread)
            {
                uv_mutex_unlock(&self->m_inLock);
                return;
            }
        }

        self->m_task = self->m_in.front();
        self->m_in.pop();

        uv_mutex_unlock(&self->m_inLock);

        // Encode input samples
        self->encode(
            self->m_task->samples,
            self->m_task->numsamples
        );

        // Push encoded data
        uv_mutex_lock(&self->m_outLock);

        self->m_out.push(self->m_task);

        uv_mutex_unlock(&self->m_outLock);

        uv_async_send(&self->m_outSignal);
    }
}

void Encoder::outSignaller(uv_async_t *p_async, int p_status)
{
    Encoder *self = reinterpret_cast<Encoder*>(p_async->data);

    HandleScope scope(Isolate::GetCurrent());

    uv_mutex_lock(&self->m_outLock);
    while(!self->m_out.empty())
    {
        EncodeTask *out = self->m_out.front();
        self->m_out.pop();

        uv_mutex_unlock(&self->m_outLock);

        out->frame.Dispose();

        std::list<EncodeOutput>::const_iterator iter;
        for(iter = out->out.begin(); iter != out->out.end(); ++iter)
        {
            Local<Object> buf = Buffer::Use(reinterpret_cast<char*>(iter->data), iter->length);

            // Emit data event
            Handle<Value> args[2] = {
                String::New("data"),
                buf
            };

            MakeCallback(self->handle(), "emit", 2, args);
        }

        delete out;

        uv_mutex_lock(&self->m_outLock);
    }
    uv_mutex_unlock(&self->m_outLock);
}

void Encoder::errorSignaller(uv_async_t *p_async, int p_status)
{
    Encoder *self = reinterpret_cast<Encoder*>(p_async->data);

    HandleScope scope(Isolate::GetCurrent());

    uv_mutex_lock(&self->m_errorLock);
    while(!self->m_errors.empty())
    {
        std::string err = self->m_errors.front();
        self->m_errors.pop();

        uv_mutex_unlock(&self->m_errorLock);

        // Emit error event
        Handle<Value> args[2] = {
            String::New("error"),
            String::New(err.c_str())
        };

        MakeCallback(self->handle(), "emit", 2, args);

        uv_mutex_lock(&self->m_errorLock);
    }
    uv_mutex_unlock(&self->m_errorLock);
}