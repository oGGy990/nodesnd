{
    "targets" : [
        {
            "target_name" : "OggVorbis",
            "sources" : [
                "addons/OggVorbis/OggVorbis.cc",
                "addons/OggVorbis/OggVorbisDecoder.cc",
                "addons/OggVorbis/OggVorbisEncoder.cc"
            ],
            "libraries" : [ "-lvorbis -lvorbisenc" ]
        }
    ]
}