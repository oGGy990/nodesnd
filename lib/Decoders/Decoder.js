var util = require('util');
var stream = require('stream');

util.inherits(Decoder, stream.Transform);

function Decoder () {
    stream.Transform.call(this);
}

Decoder.types = function () {
    return [];
}

module.exports = Decoder;