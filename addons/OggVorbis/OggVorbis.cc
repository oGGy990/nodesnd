#include <node.h>
#include <v8.h>
#include "OggVorbisDecoder.hh"

using namespace v8;

void init(Handle<Object> exports)
{
    OggVorbisDecoder::Init(exports);
}

NODE_MODULE(OggVorbis, init)