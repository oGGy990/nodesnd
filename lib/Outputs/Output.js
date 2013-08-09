var Source = require('../Source.js');
var util = require('util');

util.inherits(Output, Source);

function Output (source, id) {
    Source.call(this, id, source, true);
}

module.exports = Output;