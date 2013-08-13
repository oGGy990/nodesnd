#ifndef DECODER_HH
#define DECODER_HH

#include <node.h>
#include <node_buffer.h>
#include <queue>

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
    virtual void decode(unsigned char *p_data, unsigned int p_len, float **p_samples, unsigned int *p_sampleslen);

private:
    static v8::Handle<v8::Value> New(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> Decode(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> Types(const v8::Arguments &p_args);

    static void threadRunner(void *p_data);
    static void outSignaller(uv_async_t *p_async, int p_status);

    int m_channels;
    int m_samplerate;

    bool m_endThread;

    struct DecodeTask
    {
        DecodeTask() :
            bufferdata(0), bufferlen(0),
            channels(0), samplerate(0), samples(0)
        {}

        // Input
        v8::Persistent<v8::Object> buffer;
        unsigned char *bufferdata;
        unsigned int bufferlen;

        // Output
        int channels;
        int samplerate;
        float *samples;
        unsigned int sampleslen;
    };
    std::queue<DecodeTask*> m_in;
    std::queue<DecodeTask*> m_out;

    uv_thread_t m_thread;
    uv_mutex_t m_inLock;
    uv_mutex_t m_outLock;
    uv_cond_t m_inCondition;
    uv_async_t m_outSignal;
};

#endif // DECODER_HH