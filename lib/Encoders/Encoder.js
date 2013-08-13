var events = require('events');
var nsndnative = require('../../build/Release/nodesnd_native');

inherits(nsndnative.Encoder, events.EventEmitter);

// extend prototype
function inherits(target, source) {
    for (var k in source.prototype)
        target.prototype[k] = source.prototype[k];
}

module.exports = nsndnative.Encoder;