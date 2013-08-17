var nodesnd = require('..');

var src = new nodesnd.HTTPSource(
    {
        'url' : 'http://out01.t4e.dj:80/lounge_high.mp3'
    }
);

var safe = new nodesnd.SafeSource(src);

var dj = new nodesnd.DJSource(
    {
        'port' : 12345,
        'mountpoint' : '/dj.ogg',
        'auth' : function (user, password) {
            return true;
        }
    }
);

var prio = new nodesnd.PrioritySource();
prio.addSource(dj);
prio.addSource(safe);

var out1 = new nodesnd.IcecastOutput(
    'localhost',
    8000,
    '/test.ogg',
    'source',
    'hackme',
    new nodesnd.OggVorbisEncoder(2, 44100, 128),
    {
        'name' : 'NodeJS Test'
    },
    prio
);

var out2 = new nodesnd.IcecastOutput(
    'localhost',
    8000,
    '/test.mp3',
    'source',
    'hackme',
    new nodesnd.MP3Encoder(2, 44100, 128),
    {
        'name' : 'NodeJS Test'
    },
    prio
);