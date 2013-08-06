var nodesnd = require('..');

console.log(nodesnd);

//var src = new nodesnd.HTTPSource({'url' : 'http://out01.t4e.dj:80/lounge_high.ogg'});
var src = new nodesnd.DJSource({'port' : 12345, 'mountpount' : '/dj.ogg', 'auth' : function(user, password){ return true; }});

console.log(src.read());