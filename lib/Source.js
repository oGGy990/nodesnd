var stream = require('stream');
var util = require('util');

util.inherits(Source, stream.Readable);

function Source () {
    stream.Readable.call(this);
}

module.exports = Source;
