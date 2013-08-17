#ifndef DECODER_HH
#define DECODER_HH

#include <node.h>
#include <list>
#include <queue>
#include <string>

class AudioFrame;

class Decoder : public node::ObjectWrap
{
public:
    virtual ~Decoder();

    static void Initialize(v8::Handle<v8::Object> p_exports);

    int channels() const { return m_channels; };
    int samplerate() const { return m_samplerate; };

protected:
    static v8::Local<v8::FunctionTemplate> Template();

    Decoder();

    void finishProcessing();

    void setChannels(int p_channels) { m_channels = p_channels; }
    void setSamplerate(int p_rate) { m_samplerate = p_rate; }

    // Called on a different thread
    virtual void decode(unsigned char *p_data, unsigned int p_len);

    // To be called by decode method for each decoded frame
    void pushSamples(float *p_samples, unsigned int p_num);
    void emitError(const std::string &p_err);

private:
    static void New(const v8::FunctionCallbackInfo<v8::Value> &p_args);
    static void Decode(const v8::FunctionCallbackInfo<v8::Value> &p_args);
    static void Types(const v8::FunctionCallbackInfo<v8::Value> &p_args);

    static void threadRunner(void *p_data);
    static void outSignaller(uv_async_t *p_async, int p_status);
    static void errorSignaller(uv_async_t *p_async, int p_status);

    int m_channels;
    int m_samplerate;

    bool m_endThread;

    struct DecodeOutput
    {
        DecodeOutput() :
            channels(0), samplerate(0),
            samples(0), numsamples(0)
        {}

        int channels;
        int samplerate;
        float *samples;
        unsigned int numsamples;
    };

    struct DecodeTask
    {
        DecodeTask() :
            bufferdata(0), bufferlen(0)
        {}

        // Input
        v8::Persistent<v8::Object> buffer;
        unsigned char *bufferdata;
        unsigned int bufferlen;

        // Output
        std::list<DecodeOutput> out;
    };
    std::queue<DecodeTask*> m_in;
    std::queue<DecodeTask*> m_out;
    DecodeTask *m_task;

    std::queue<std::string> m_errors;

    uv_thread_t m_thread;

    uv_mutex_t m_inLock;
    uv_cond_t m_inCondition;

    uv_mutex_t m_outLock;
    uv_async_t m_outSignal;

    uv_mutex_t m_errorLock;
    uv_async_t m_errorSignal;
};

#endif // DECODER_HH