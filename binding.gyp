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
                'addons/OggVorbis/Encoder.cc'
            ],
            'conditions' : [
                ['OS=="linux"',
                    {
                        'cflags' : [
                            '<!@(pkg-config --cflags vorbisenc)'
                        ],
                        'libraries' : [
                            '<!@(pkg-config --libs vorbisenc)'
                        ]
                    }
                ]
            ]
        }
    ]
}
