#include <node.h>
#include <v8.h>
#include "OggVorbisDecoder.hh"
#include "OggVorbisEncoder.hh"

using namespace v8;

void init(Handle<Object> exports)
{
    OggVorbisDecoder::Initialize(exports);
    OggVorbisEncoder::Initialize(exports);
}

NODE_MODULE(OggVorbis, init)