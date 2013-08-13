var nodesnd = require('..');

var src = new nodesnd.HTTPSource({'url' : 'http://out01.t4e.dj:80/lounge_high.ogg'});
var safe = new nodesnd.SafeSource(src);

var dj = new nodesnd.DJSource({'port' : 12345, 'mountpoint' : '/dj.ogg', 'auth' : function(user, password){ return true; }});

var prio = new nodesnd.PrioritySource();
prio.addSource(dj);
prio.addSource(safe);

var out = new nodesnd.IcecastOutput('lw3.t4e.dj', 3000, '/test.ogg', 'source', '****', new nodesnd.OggVorbisEncoder(2, 44100, 128), {'name':'NodeJS Test'}, prio);