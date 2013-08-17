{
    'targets' : [
        {
            'target_name' : 'nodesnd_native',
            'sources' : [
                'addons/nodesnd.cc',
                'addons/AudioFrame.cc',
                'addons/Decoder.cc',
                'addons/Encoder.cc',
                'addons/OggVorbis/Decoder.cc',
                'addons/OggVorbis/Encoder.cc',
                'addons/MP3/Decoder.cc',
                'addons/MP3/Encoder.cc'
            ],
            'conditions' : [
                ['OS=="linux"',
                    {
                        'cflags' : [
                            '<!@(pkg-config --cflags vorbisenc)',
                            '<!@(pkg-config --cflags libmpg123)'
                        ],
                        'libraries' : [
                            '<!@(pkg-config --libs vorbisenc)',
                            '<!@(pkg-config --libs libmpg123)',
                            '-lmp3lame'
                        ]
                    }
                ]
            ]
        }
    ]
}
