var decoder = require('./Decoder.js');
var util = require('util');
var oggVorbis = require('../../build/Release/OggVorbis');

util.inherits(OggVorbisDecoder, decoder);

function OggVorbisDecoder () {
    decoder.call(this);

    this._internal = new oggVorbis.OggVorbisDecoder();
}

OggVorbisDecoder.types = function () {
    return [ 'application/ogg' ];
}

OggVorbisDecoder.prototype._transform = function(data, encoding, callback) {
    var frame = this._internal.decode(data);

    if(0 < frame.length) {
        console.log(frame.length);
        this.push(frame);
    }

    callback();
}

module.exports = OggVorbisDecoder;