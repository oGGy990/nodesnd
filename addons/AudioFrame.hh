#ifndef AUDIO_FRAME_HH
#define AUDIO_FRAME_HH

#include <node.h>

class AudioFrame : public node::ObjectWrap
{
public:
    ~AudioFrame();

    int channels() const { return m_channels; }
    int samplerate() const { return m_samplerate; }
    float *samples() const;
    unsigned int numSamples() const;

    static void Initialize(v8::Handle<v8::Object> p_exports);

    static AudioFrame *New(int p_channels, int p_samplerate, float *p_samples, unsigned int p_samplelen);

private:
    static v8::Handle<v8::Value> New(const v8::Arguments &p_args);

    static v8::Handle<v8::Value> ChannelsGetter(v8::Local<v8::String> p_property, const v8::AccessorInfo &p_info);
    static v8::Handle<v8::Value> SamplerateGetter(v8::Local<v8::String> p_property, const v8::AccessorInfo &p_info);
    static v8::Handle<v8::Value> DataGetter(v8::Local<v8::String> p_property, const v8::AccessorInfo &p_info);

    static v8::Persistent<v8::FunctionTemplate> m_constructorTemplate;

    AudioFrame(int p_channels, int p_samplerate, v8::Local<v8::Object> p_buffer);
    AudioFrame(const AudioFrame &p_other);

    int m_channels;
    int m_samplerate;
    v8::Persistent<v8::Object> m_buffer;
};

#endif // AUDIO_FRAME_HH