#ifndef MP3_DECODER_HH
#define MP3_DECODER_HH

#include <mpg123.h>
#include "../Decoder.hh"

class MP3Decoder : public Decoder
{
public:
    ~MP3Decoder();

    static void Initialize(v8::Handle<v8::Object> p_exports);

private:
    MP3Decoder();

    virtual void decode(unsigned char *p_data, unsigned int p_len);

    static void New(const v8::FunctionCallbackInfo<v8::Value> &p_args);
    static void Types(const v8::FunctionCallbackInfo<v8::Value> &p_args);

    mpg123_handle *m_decoder;
    mpg123_frameinfo m_info;
};

#endif // MP3_DECODER_HH