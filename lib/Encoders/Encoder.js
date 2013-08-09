var util = require('util');
var stream = require('stream');

util.inherits(Encoder, stream.Transform);

function Encoder (channels, samplerate, bitrate) {
    stream.Transform.call(this);

    this.channels = channels;
    this.samplerate = samplerate;
    this.bitrate = bitrate;
}

Encoder.prototype.type = '';

Encoder.prototype.init = function () {
    return false;
}

Encoder.prototype.finish = function () {
}

module.exports = Encoder;