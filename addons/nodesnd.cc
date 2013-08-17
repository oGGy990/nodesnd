#include <node.h>
#include <v8.h>
#include "AudioFrame.hh"
#include "Decoder.hh"
#include "Encoder.hh"
#include "OggVorbis/Decoder.hh"
#include "OggVorbis/Encoder.hh"
#include "MP3/Decoder.hh"
#include "MP3/Encoder.hh"

using namespace v8;

void init(Handle<Object> exports)
{
    AudioFrame::Initialize(exports);
    Decoder::Initialize(exports);
    Encoder::Initialize(exports);
    OggVorbisDecoder::Initialize(exports);
    OggVorbisEncoder::Initialize(exports);
    MP3Decoder::Initialize(exports);
    MP3Encoder::Initialize(exports);
}

NODE_MODULE(nodesnd_native, init)