#include "OggVorbisDecoder.hh"
#include <node_buffer.h>
#include <cstring>
#include <cstdlib>

using namespace v8;

OggVorbisDecoder::OggVorbisDecoder() :
    m_serialno(-1),
    m_numpackets(0),
    m_channels(0),
    m_bitrate(0),
    m_rate(0),
    m_initialized(0)
{
    ogg_sync_init(&m_oggsync);
    ogg_stream_init(&m_oggstream, 0);
}

OggVorbisDecoder::~OggVorbisDecoder()
{
    if(m_initialized > 2)
    {
        vorbis_block_clear(&m_vorbisblock);
        vorbis_dsp_clear(&m_vorbisdsp);
    }

    if(m_initialized > 1)
    {
        vorbis_comment_clear(&m_vorbiscomment);
        vorbis_info_clear(&m_vorbisinfo);
    }

    ogg_stream_clear(&m_oggstream);
    ogg_sync_clear(&m_oggsync);
}

void OggVorbisDecoder::Init(Handle<Object> p_exports)
{
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("OggVorbisDecoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    tpl->PrototypeTemplate()->Set(String::NewSymbol("decode"), FunctionTemplate::New(decode));

    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    p_exports->Set(String::NewSymbol("OggVorbisDecoder"), constructor);
}

Handle<Value> OggVorbisDecoder::New(const Arguments &p_args)
{
    HandleScope scope;

    OggVorbisDecoder *obj = new OggVorbisDecoder();
    obj->Wrap(p_args.This());

    return p_args.This();
}

Handle<v8::Value> OggVorbisDecoder::decode(const Arguments &p_args)
{
    HandleScope scope;

    // Get arguments
    OggVorbisDecoder *self = node::ObjectWrap::Unwrap<OggVorbisDecoder>(p_args.This());
    const char *data = node::Buffer::Data(p_args[0]->ToObject());
    unsigned int len = node::Buffer::Length(p_args[0]->ToObject());

    float *outbuffer = 0;
    unsigned int outlen = 0;

    // Decode input
    char *syncbuffer = ogg_sync_buffer(&self->m_oggsync, len);
    memcpy(syncbuffer, data, len);
    ogg_sync_wrote(&self->m_oggsync, len);

    while(1 == ogg_sync_pageout(&self->m_oggsync, &self->m_oggpage))
    {
        // New logical stream?
        if(ogg_page_serialno(&self->m_oggpage) != self->m_serialno && ogg_page_bos(&self->m_oggpage))
        {
            self->m_serialno = ogg_page_serialno(&self->m_oggpage);
            ogg_stream_reset_serialno(&self->m_oggstream, self->m_serialno);
        }

        ogg_stream_pagein(&self->m_oggstream, &self->m_oggpage);

        while(ogg_stream_packetout(&self->m_oggstream, &self->m_oggpacket) == 1)
        {
            // Very first packet?
            if(!self->m_numpackets)
            {
                // Not a vorbis stream?
                if(!vorbis_synthesis_idheader(&self->m_oggpacket))
                {
                    /*dLog->error("OGG/Vorbis", QString("OGG stream is no vorbis stream!"));
                    stop();*/
                    free(outbuffer);
                    return scope.Close(Null());
                }

                vorbis_info_init(&self->m_vorbisinfo);
                vorbis_comment_init(&self->m_vorbiscomment);
                self->m_initialized = 2;
            }
            ++self->m_numpackets;

            // Feed 3 headers
            if(self->m_numpackets < 4)
            {
                vorbis_synthesis_headerin(&self->m_vorbisinfo, &self->m_vorbiscomment, &self->m_oggpacket);
                continue;
            }
            // First data packet, initialize synthesis unit
            else if(self->m_numpackets == 4)
            {
                vorbis_synthesis_init(&self->m_vorbisdsp, &self->m_vorbisinfo);
                vorbis_block_init(&self->m_vorbisdsp, &self->m_vorbisblock);
                self->m_initialized = 3;

                self->m_channels = self->m_vorbisinfo.channels;
                self->m_rate = self->m_vorbisinfo.rate;
                self->m_bitrate = self->m_vorbisinfo.bitrate_nominal;
                /*emit newFormat(m_channels, m_rate);

                dLog->info("OGG/Vorbis", QString("Stream ready: %1 Channels @ %2Hz, Bitrate %3kbps")
                           .arg(m_channels).arg(m_rate).arg(m_bitrate));*/
            }

            // Synthesize current block
            vorbis_synthesis(&self->m_vorbisblock, &self->m_oggpacket);
            vorbis_synthesis_blockin(&self->m_vorbisdsp, &self->m_vorbisblock);

            // Get all samples
            float **samples;
            int numsamples = vorbis_synthesis_pcmout(&self->m_vorbisdsp, &samples);
            vorbis_synthesis_read(&self->m_vorbisdsp, numsamples);

            // Push all samples into outbuffer
            outbuffer = reinterpret_cast<float*>(realloc(outbuffer, (outlen + numsamples * 2) * sizeof(float)));
            if(1 == self->m_channels)
            {
                for(int i = 0; i < numsamples; ++i)
                {
                    outbuffer[outlen] = samples[0][i];
                    outbuffer[outlen + 1] = samples[0][i];
                    outlen += 2;
                }
            }
            else
            {
                for(int i = 0; i < numsamples; ++i)
                {
                    outbuffer[outlen] = samples[0][i];
                    outbuffer[outlen + 1] = samples[1][i];
                    outlen += 2;
                }
            }
        }
    }

    // Build output buffer
    node::Buffer *sbuf = node::Buffer::New(reinterpret_cast<const char*>(outbuffer), outlen * sizeof(float));
    free(outbuffer);

    Local<Function> nodeBufferConstructor = Local<Function>::Cast(Context::GetCurrent()->Global()->Get(v8::String::New("Buffer")));
    Handle<Value> argv[3] = { sbuf->handle_, Integer::New(outlen * sizeof(float)), Integer::New(0) };
    Local<Object> buf = nodeBufferConstructor->NewInstance(3, argv);

    return scope.Close(buf);
}