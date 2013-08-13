#ifndef ENCODER_HH
#define ENCODER_HH

#include <node.h>
#include <node_buffer.h>
#include <queue>

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
    virtual void encode(float *p_samples, unsigned int p_sampleslen, unsigned char **p_data, unsigned int *p_len);

private:
    static v8::Handle<v8::Value> New(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> Init(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> Finish(const v8::Arguments &p_args);
    static v8::Handle<v8::Value> Encode(const v8::Arguments &p_args);

    static v8::Handle<v8::Value> ChannelsGetter(v8::Local<v8::String> p_property, const v8::AccessorInfo &p_info);
    static v8::Handle<v8::Value> SamplerateGetter(v8::Local<v8::String> p_property, const v8::AccessorInfo &p_info);
    static v8::Handle<v8::Value> BitrateGetter(v8::Local<v8::String> p_property, const v8::AccessorInfo &p_info);

    static void threadRunner(void *p_data);
    static void outSignaller(uv_async_t *p_async, int p_status);

    int m_channels;
    int m_samplerate;
    int m_bitrate;

    bool m_endThread;

    struct EncodeTask
    {
        EncodeTask() :
            samples(0), sampleslen(0),
            bufferdata(0), bufferlen(0)
        {}

        // Input
        v8::Persistent<v8::Object> frame;
        float *samples;
        unsigned int sampleslen;

        // Output
        unsigned char *bufferdata;
        unsigned int bufferlen;
    };
    std::queue<EncodeTask*> m_in;
    std::queue<EncodeTask*> m_out;

    uv_thread_t m_thread;
    uv_mutex_t m_inLock;
    uv_mutex_t m_outLock;
    uv_cond_t m_inCondition;
    uv_async_t m_outSignal;
};

#endif // ENCODER_HH