#include "Decoder.hh"
#include <node_buffer.h>
#include <cstring>
#include <cstdlib>

using namespace v8;

OggVorbisDecoder::OggVorbisDecoder() :
    Decoder(),
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
    finishProcessing();

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

void OggVorbisDecoder::Initialize(Handle<Object> p_exports)
{
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("OggVorbisDecoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    tpl->Inherit(Template());

    NODE_SET_METHOD(tpl->GetFunction(), "types", Types);

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

Handle<Value> OggVorbisDecoder::Types(const Arguments &p_args)
{
    HandleScope scope;

    Local<Array> myTypes = Array::New(1);
    myTypes->Set(0, String::New("application/ogg"));

    return scope.Close(myTypes);
}

void OggVorbisDecoder::decode(unsigned char *p_data, unsigned int p_len, float **p_samples, unsigned int *p_sampleslen)
{
    float *outbuffer = 0;
    unsigned int outlen = 0;

    // Decode input
    char *syncbuffer = ogg_sync_buffer(&m_oggsync, p_len);
    memcpy(syncbuffer, p_data, p_len);
    ogg_sync_wrote(&m_oggsync, p_len);

    while(1 == ogg_sync_pageout(&m_oggsync, &m_oggpage))
    {
        // New logical stream?
        if(ogg_page_serialno(&m_oggpage) != m_serialno && ogg_page_bos(&m_oggpage))
        {
            m_serialno = ogg_page_serialno(&m_oggpage);
            ogg_stream_reset_serialno(&m_oggstream, m_serialno);
        }

        ogg_stream_pagein(&m_oggstream, &m_oggpage);

        while(ogg_stream_packetout(&m_oggstream, &m_oggpacket) == 1)
        {
            // Very first packet?
            if(!m_numpackets)
            {
                // Not a vorbis stream?
                if(!vorbis_synthesis_idheader(&m_oggpacket))
                {
                    /*dLog->error("OGG/Vorbis", QString("OGG stream is no vorbis stream!"));
                    stop();*/
                    free(outbuffer);
                    return;
                }

                vorbis_info_init(&m_vorbisinfo);
                vorbis_comment_init(&m_vorbiscomment);
                m_initialized = 2;
            }
            ++m_numpackets;

            // Feed 3 headers
            if(m_numpackets < 4)
            {
                vorbis_synthesis_headerin(&m_vorbisinfo, &m_vorbiscomment, &m_oggpacket);
                continue;
            }
            // First data packet, initialize synthesis unit
            else if(m_numpackets == 4)
            {
                vorbis_synthesis_init(&m_vorbisdsp, &m_vorbisinfo);
                vorbis_block_init(&m_vorbisdsp, &m_vorbisblock);
                m_initialized = 3;

                m_channels = m_vorbisinfo.channels;
                m_rate = m_vorbisinfo.rate;
                m_bitrate = m_vorbisinfo.bitrate_nominal;
                setChannels(m_vorbisinfo.channels);
                setSamplerate(m_vorbisinfo.rate);
                /*emit newFormat(m_channels, m_rate);

                dLog->info("OGG/Vorbis", QString("Stream ready: %1 Channels @ %2Hz, Bitrate %3kbps")
                           .arg(m_channels).arg(m_rate).arg(m_bitrate));*/
            }

            // Synthesize current block
            vorbis_synthesis(&m_vorbisblock, &m_oggpacket);
            vorbis_synthesis_blockin(&m_vorbisdsp, &m_vorbisblock);

            // Get all samples
            float **samples;
            int numsamples = vorbis_synthesis_pcmout(&m_vorbisdsp, &samples);
            vorbis_synthesis_read(&m_vorbisdsp, numsamples);

            // Push all samples into outbuffer
            outbuffer = reinterpret_cast<float*>(realloc(outbuffer, (outlen + numsamples * 2) * sizeof(float)));
            if(1 == m_channels)
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

    *p_samples = outbuffer;
    *p_sampleslen = outlen * sizeof(float);
}