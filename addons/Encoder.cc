#include "Encoder.hh"
#include "AudioFrame.hh"
#include <node_internals.h>

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

    uv_cond_init(&m_inCondition);

    uv_async_init(uv_default_loop(), &m_outSignal, outSignaller);
    m_outSignal.data = this;
}

Encoder::~Encoder()
{
    finishProcessing();
    finish();

    uv_close(reinterpret_cast<uv_handle_t*>(&m_outSignal), 0);
    uv_cond_destroy(&m_inCondition);
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
            delete m_in.front();
            m_in.pop();
        }

        while(!m_out.empty())
        {
            delete m_out.front()->bufferdata;
            delete m_out.front();
            m_out.pop();
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

void Encoder::encode(float *p_samples, unsigned int p_sampleslen, unsigned char **p_data, unsigned int *p_len)
{
    fprintf(stderr, "Encoder::encode(): IMPLEMENT ME!\n");
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
        String::New(""),
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete)
    );

    return tpl;
}

void Encoder::Initialize(Handle<Object> p_exports)
{
    Local<FunctionTemplate> tpl = Template();

    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    p_exports->Set(String::NewSymbol("Encoder"), constructor);
}

Handle<Value> Encoder::New(const Arguments &p_args)
{
    HandleScope scope;

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

    return p_args.This();
}

Handle<Value> Encoder::Init(const Arguments &p_args)
{
    HandleScope scope;

    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_args.This());

    // Init subclass
    bool ok = self->init();

    if(ok)
    {
        self->m_endThread = false;

        uv_thread_create(&self->m_thread, threadRunner, self);
    }

    return scope.Close(Boolean::New(ok));
}

Handle<Value> Encoder::Finish(const Arguments &p_args)
{
    HandleScope scope;

    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_args.This());

    // End thread
    self->finishProcessing();

    // End subclass
    self->finish();

    return scope.Close(Undefined());
}

Handle<Value> Encoder::Encode(const Arguments &p_args)
{
    HandleScope scope;

    if(1 < p_args.Length() || !p_args[0]->IsObject())
    {
        return ThrowTypeError("Bad argument");
    }

    // Get arguments
    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_args.This());
    AudioFrame *frame = ObjectWrap::Unwrap<AudioFrame>(p_args[0]->ToObject());

    EncodeTask *task = new EncodeTask();
    task->frame = Persistent<Object>::New(p_args[0]->ToObject());
    task->samples = frame->samples();
    task->sampleslen = frame->numSamples() * sizeof(float);

    uv_mutex_lock(&self->m_inLock);

    self->m_in.push(task);
    uv_cond_signal(&self->m_inCondition);

    uv_mutex_unlock(&self->m_inLock);

    return scope.Close(Undefined());
}

Handle<Value> Encoder::ChannelsGetter(Local<String> p_property, const AccessorInfo &p_info)
{
    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_info.Holder());
    return Integer::New(self->m_channels);
}

Handle<Value> Encoder::SamplerateGetter(Local<String> p_property, const AccessorInfo &p_info)
{
    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_info.Holder());
    return Integer::New(self->m_samplerate);
}

Handle<Value> Encoder::BitrateGetter(Local<String> p_property, const AccessorInfo &p_info)
{
    Encoder *self = ObjectWrap::Unwrap<Encoder>(p_info.Holder());
    return Integer::New(self->m_bitrate);
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

        EncodeTask *task = self->m_in.front();
        self->m_in.pop();

        uv_mutex_unlock(&self->m_inLock);

        // Encode input samples
        self->encode(
            task->samples,
            task->sampleslen,
            &task->bufferdata,
            &task->bufferlen
        );

        // Push encoded data
        uv_mutex_lock(&self->m_outLock);

        self->m_out.push(task);

        uv_mutex_unlock(&self->m_outLock);

        uv_async_send(&self->m_outSignal);
    }
}

void Encoder::outSignaller(uv_async_t *p_async, int p_status)
{
    Encoder *self = reinterpret_cast<Encoder*>(p_async->data);

    HandleScope scope;

    uv_mutex_lock(&self->m_outLock);
    while(!self->m_out.empty())
    {
        EncodeTask *out = self->m_out.front();
        self->m_out.pop();

        if(!out->bufferdata)
        {
            delete out;
            continue;
        }

        uv_mutex_unlock(&self->m_outLock);

        Buffer *sbuf = Buffer::New(reinterpret_cast<const char*>(out->bufferdata), out->bufferlen);
        delete out->bufferdata;
        delete out;

        // Create fast buffer
        Local<Function> nodeBufferConstructor = Local<Function>::Cast(Context::GetCurrent()->Global()->Get(String::New("Buffer")));
        Handle<Value> bufargv[3] = { sbuf->handle_, Integer::New(out->bufferlen), Integer::New(0) };
        Local<Object> buf = nodeBufferConstructor->NewInstance(3, bufargv);

        // Emit frame event
        Handle<Value> args[2] = {
            String::New("data"),
            buf
        };

        MakeCallback(self->handle_, "emit", 2, args);

        uv_mutex_lock(&self->m_outLock);
    }
    uv_mutex_unlock(&self->m_outLock);
}