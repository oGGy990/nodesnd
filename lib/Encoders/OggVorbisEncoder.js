var encoder = require('./Encoder.js');
var util = require('util');
var oggVorbis = require('../../build/Release/OggVorbis');

util.inherits(OggVorbisEncoder, encoder);

function OggVorbisEncoder (channels, samplerate, bitrate) {
    encoder.call(this, channels, samplerate, bitrate);

    this._internal = new oggVorbis.OggVorbisEncoder(channels, samplerate, bitrate);
}

OggVorbisEncoder.prototype.type = 'application/ogg';

OggVorbisEncoder.prototype.init = function () {
    return this._internal.init();
}

OggVorbisEncoder.prototype.finish = function () {
    this._internal.finish();
}

OggVorbisEncoder.prototype._transform = function(data, encoding, callback) {
    var frame = this._internal.encode(data);

    if(0 < frame.length) {
        this.push(frame);
    }

    callback();
}

module.exports = OggVorbisEncoder;