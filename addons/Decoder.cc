#include "Decoder.hh"
#include "AudioFrame.hh"
#include <node_internals.h>
#include <iostream>

using namespace v8;
using namespace node;

Decoder::Decoder() :
    ObjectWrap(),
    m_channels(0),
    m_samplerate(0),
    m_endThread(false)
{
    uv_mutex_init(&m_inLock);
    uv_mutex_init(&m_outLock);
    uv_mutex_init(&m_errorLock);

    uv_cond_init(&m_inCondition);

    uv_async_init(uv_default_loop(), &m_outSignal, outSignaller);
    m_outSignal.data = this;

    uv_async_init(uv_default_loop(), &m_errorSignal, errorSignaller);
    m_errorSignal.data = this;

    uv_thread_create(&m_thread, threadRunner, this);
}

Decoder::~Decoder()
{
    finishProcessing();

    uv_close(reinterpret_cast<uv_handle_t*>(&m_errorSignal), 0);
    uv_close(reinterpret_cast<uv_handle_t*>(&m_outSignal), 0);

    uv_cond_destroy(&m_inCondition);

    uv_mutex_destroy(&m_errorLock);
    uv_mutex_destroy(&m_outLock);
    uv_mutex_destroy(&m_inLock);
}

void Decoder::finishProcessing()
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
            m_in.front()->buffer.Dispose();
            delete m_in.front();
            m_in.pop();
        }

        while(!m_out.empty())
        {
            DecodeTask *task = m_out.front();
            m_out.pop();

            std::list<DecodeOutput>::const_iterator iter;
            for(iter = task->out.begin(); iter != task->out.end(); ++iter)
            {
                delete[] iter->samples;
            }

            task->buffer.Dispose();
            delete task;
        }
    }
}

void Decoder::decode(unsigned char *p_data, unsigned int p_len)
{
    std::cerr << "Decoder::decode(): IMPLEMENT ME!" << std::endl;
}

void Decoder::pushSamples(float *p_samples, unsigned int p_num)
{
    if(!p_samples || !p_num)
        return;

    DecodeOutput out;
    out.channels = m_channels;
    out.samplerate = m_samplerate;
    out.samples = p_samples;
    out.numsamples = p_num;

    // Only called from decode thread while task
    // is detached from main thread, so there's no
    // need to protect this action.
    m_task->out.push_back(out);
}

void Decoder::emitError(const std::string &p_err)
{
    uv_mutex_lock(&m_errorLock);
    m_errors.push(p_err);
    uv_mutex_unlock(&m_errorLock);

    uv_async_send(&m_errorSignal);
}

Local<FunctionTemplate> Decoder::Template()
{
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("Decoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "decode", Decode);

    return tpl;
}

void Decoder::Initialize(Handle<Object> p_exports)
{
    Local<FunctionTemplate> tpl = Template();

    Persistent<Function> constructor(Isolate::GetCurrent(), tpl->GetFunction());
    p_exports->Set(String::NewSymbol("Decoder"), tpl->GetFunction());
}

void Decoder::New(const v8::FunctionCallbackInfo<v8::Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    Decoder *obj = new Decoder();
    obj->Wrap(p_args.This());

    p_args.GetReturnValue().Set(p_args.This());
}

void Decoder::Decode(const v8::FunctionCallbackInfo<v8::Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    if(1 < p_args.Length() || !p_args[0]->IsObject())
    {
        return ThrowTypeError("Bad argument");
    }

    // Get arguments
    Decoder *self = node::ObjectWrap::Unwrap<Decoder>(p_args.This());

    DecodeTask *task = new DecodeTask();
    task->buffer.Reset(p_args.GetIsolate(), p_args[0]->ToObject());
    task->bufferdata = reinterpret_cast<unsigned char*>(Buffer::Data(p_args[0]->ToObject()));
    task->bufferlen = Buffer::Length(p_args[0]->ToObject());

    if(!task->bufferdata || !task->bufferlen)
    {
        task->buffer.Dispose();
        delete task;

        return;
    }

    uv_mutex_lock(&self->m_inLock);

    self->m_in.push(task);
    uv_cond_signal(&self->m_inCondition);

    uv_mutex_unlock(&self->m_inLock);
}

void Decoder::Types(const v8::FunctionCallbackInfo<v8::Value> &p_args)
{
    HandleScope scope(p_args.GetIsolate());

    p_args.GetReturnValue().Set(Array::New(0));
}

void Decoder::threadRunner(void *p_data)
{
    Decoder *self = reinterpret_cast<Decoder*>(p_data);

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

        // Decode input chunk
        self->decode(
            self->m_task->bufferdata,
            self->m_task->bufferlen
        );

        // Push decoded samples
        uv_mutex_lock(&self->m_outLock);

        self->m_out.push(self->m_task);

        uv_mutex_unlock(&self->m_outLock);

        uv_async_send(&self->m_outSignal);
    }
}

void Decoder::outSignaller(uv_async_t *p_async, int p_status)
{
    Decoder *self = reinterpret_cast<Decoder*>(p_async->data);

    HandleScope scope(Isolate::GetCurrent());

    uv_mutex_lock(&self->m_outLock);
    while(!self->m_out.empty())
    {
        DecodeTask *out = self->m_out.front();
        self->m_out.pop();

        uv_mutex_unlock(&self->m_outLock);

        std::list<DecodeOutput>::const_iterator iter;
        for(iter = out->out.begin(); iter != out->out.end(); ++iter)
        {
            // Takes ownership of samples
            AudioFrame *frame = AudioFrame::New(
                iter->channels,
                iter->samplerate,
                iter->samples,
                iter->numsamples * sizeof(float)
            );

            if(!frame)
                continue;

            // Emit frame event
            Handle<Value> args[2] = {
                String::New("frame"),
                frame->handle()
            };

            MakeCallback(self->handle(), "emit", 2, args);
        }

        out->buffer.Dispose();
        delete out;

        uv_mutex_lock(&self->m_outLock);
    }
    uv_mutex_unlock(&self->m_outLock);
}

void Decoder::errorSignaller(uv_async_t *p_async, int p_status)
{
    Decoder *self = reinterpret_cast<Decoder*>(p_async->data);

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