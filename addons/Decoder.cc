#include "Decoder.hh"
#include "AudioFrame.hh"
#include <node_internals.h>

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

    uv_cond_init(&m_inCondition);

    uv_async_init(uv_default_loop(), &m_outSignal, outSignaller);
    m_outSignal.data = this;

    uv_thread_create(&m_thread, threadRunner, this);
}

Decoder::~Decoder()
{
    finishProcessing();

    uv_close(reinterpret_cast<uv_handle_t*>(&m_outSignal), 0);
    uv_cond_destroy(&m_inCondition);
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
            delete m_in.front();
            m_in.pop();
        }

        while(!m_out.empty())
        {
            delete m_out.front()->samples;
            delete m_out.front();
            m_out.pop();
        }
    }
}

void Decoder::decode(unsigned char *p_data, unsigned int p_len, float **p_samples, unsigned int *p_sampleslen)
{
    fprintf(stderr, "Decoder::decode(): IMPLEMENT ME!\n");
}

Local<FunctionTemplate> Decoder::Template()
{
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("Decoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "decode", Decode);

    NODE_SET_METHOD(tpl->GetFunction(), "types", Types);

    return tpl;
}

void Decoder::Initialize(Handle<Object> p_exports)
{
    Local<FunctionTemplate> tpl = Template();

    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    p_exports->Set(String::NewSymbol("Decoder"), constructor);
}

Handle<Value> Decoder::New(const Arguments &p_args)
{
    HandleScope scope;

    Decoder *obj = new Decoder();
    obj->Wrap(p_args.This());

    return p_args.This();
}

Handle<Value> Decoder::Decode(const Arguments &p_args)
{
    HandleScope scope;

    if(1 < p_args.Length() || !p_args[0]->IsObject())
    {
        return ThrowTypeError("Bad argument");
    }

    // Get arguments
    Decoder *self = node::ObjectWrap::Unwrap<Decoder>(p_args.This());

    DecodeTask *task = new DecodeTask();
    task->buffer = Persistent<Object>::New(p_args[0]->ToObject());
    task->bufferdata = reinterpret_cast<unsigned char*>(Buffer::Data(p_args[0]->ToObject()));
    task->bufferlen = Buffer::Length(p_args[0]->ToObject());

    uv_mutex_lock(&self->m_inLock);

    self->m_in.push(task);
    uv_cond_signal(&self->m_inCondition);

    uv_mutex_unlock(&self->m_inLock);

    return scope.Close(Undefined());
}

Handle<Value> Decoder::Types(const Arguments &p_args)
{
    static Persistent<Array> myTypes = Persistent<Array>::New(Array::New(0));

    HandleScope scope;

    return scope.Close(myTypes);
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

        DecodeTask *task = self->m_in.front();
        self->m_in.pop();

        uv_mutex_unlock(&self->m_inLock);

        // Decode input chunk
        self->decode(
            task->bufferdata,
            task->bufferlen,
            &task->samples,
            &task->sampleslen
        );

        task->channels = self->m_channels;
        task->samplerate = self->m_samplerate;

        // Push decoded samples
        uv_mutex_lock(&self->m_outLock);

        self->m_out.push(task);

        uv_mutex_unlock(&self->m_outLock);

        uv_async_send(&self->m_outSignal);
    }
}

void Decoder::outSignaller(uv_async_t *p_async, int p_status)
{
    Decoder *self = reinterpret_cast<Decoder*>(p_async->data);

    HandleScope scope;

    uv_mutex_lock(&self->m_outLock);
    while(!self->m_out.empty())
    {
        DecodeTask *out = self->m_out.front();
        self->m_out.pop();

        if(!out->samples)
        {
            delete out;
            continue;
        }

        uv_mutex_unlock(&self->m_outLock);

        AudioFrame *frame = AudioFrame::New(out->channels, out->samplerate, out->samples, out->sampleslen);

        delete out;

        // Emit frame event
        Handle<Value> args[2] = {
            String::New("frame"),
            frame->handle_
        };

        MakeCallback(self->handle_, "emit", 2, args);

        uv_mutex_lock(&self->m_outLock);
    }
    uv_mutex_unlock(&self->m_outLock);
}