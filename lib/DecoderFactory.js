var oga = require('./Decoders/OggVorbisDecoder.js');

function DecoderFactory () {
    this._decoders = [oga];
    this._typeMap = {};

    for(var i = 0; i < this._decoders.length; i += 1) {
        var types = this._decoders[i].types();
        for(var j = 0; j < types.length; j += 1) {
            this._typeMap[types[j]] = this._decoders[i];
        }
    }
}

DecoderFactory.prototype.availableDecoders = function () {
    return this._decoders;
}

DecoderFactory.prototype.newDecoderForType = function(type) {
    if(undefined === this._typeMap[type]) {
        return null;
    }
    return new this._typeMap[type]();
}

module.exports = new DecoderFactory();