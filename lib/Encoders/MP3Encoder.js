var events = require('events');
var config = require('../config.js');
var nsndnative = require('../../build/' + config.buildType + '/nodesnd_native');

inherits(nsndnative.MP3Encoder, events.EventEmitter);

// extend prototype
function inherits(target, source) {
    for (var k in source.prototype)
        target.prototype[k] = source.prototype[k];
}

module.exports = nsndnative.MP3Encoder;