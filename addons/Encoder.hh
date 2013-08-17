#ifndef ENCODER_HH
#define ENCODER_HH

#include <node.h>
#include <queue>
#include <list>
#include <string>

class AudioFrame;

class Encoder : public node::ObjectWrap
{
public:
    virtual ~Encoder();

    static void Initialize(v8::Handle<v8::Object> p_exports);

    int channels() const { return m_channels; };
    int samplerate() const { return m_samplerate; };
    int bitrate() const { return m_bitrate; };

protected:
    static v8::Local<v8::FunctionTemplate> Template();

    Encoder(int p_channels, int p_samplerate, int p_bitrate);

    void finishProcessing();

    // Called on a different thread
    virtual bool init();
    virtual void finish();
    virtual void encode(float *p_samples, unsigned int p_sampleslen);

    void pushFrame(unsigned char *p_data, unsigned int p_len);
    void emitError(const std::string &p_err);

private:
    static void New(const v8::FunctionCallbackInfo<v8::Value> &p_args);
    static void Init(const v8::FunctionCallbackInfo<v8::Value> &p_args);
    static void Finish(const v8::FunctionCallbackInfo<v8::Value> &p_args);
    static void Encode(const v8::FunctionCallbackInfo<v8::Value> &p_args);

    static void ChannelsGetter(v8::Local<v8::String> p_property, const v8::PropertyCallbackInfo<v8::Value> &p_info);
    static void SamplerateGetter(v8::Local<v8::String> p_property, const v8::PropertyCallbackInfo<v8::Value> &p_info);
    static void BitrateGetter(v8::Local<v8::String> p_property, const v8::PropertyCallbackInfo<v8::Value> &p_info);

    static void threadRunner(void *p_data);
    static void outSignaller(uv_async_t *p_async, int p_status);
    static void errorSignaller(uv_async_t *p_async, int p_status);

    int m_channels;
    int m_samplerate;
    int m_bitrate;

    bool m_endThread;

    struct EncodeOutput
    {
        EncodeOutput() :
            data(0), length(0)
        {}

        unsigned char *data;
        unsigned int length;
    };

    struct EncodeTask
    {
        EncodeTask() :
            samples(0), numsamples(0)
        {}

        // Input
        v8::Persistent<v8::Object> frame;
        float *samples;
        unsigned int numsamples;

        // Output
        std::list<EncodeOutput> out;
    };
    std::queue<EncodeTask*> m_in;
    std::queue<EncodeTask*> m_out;
    EncodeTask *m_task;

    std::queue<std::string> m_errors;

    uv_thread_t m_thread;

    uv_mutex_t m_inLock;
    uv_cond_t m_inCondition;

    uv_mutex_t m_outLock;
    uv_async_t m_outSignal;

    uv_mutex_t m_errorLock;
    uv_async_t m_errorSignal;
};

#endif // ENCODER_HH