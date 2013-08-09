var MetaData = require('./MetaData.js');

function AudioFrame () {
    // Other AudioFrame given? Clone frame.
    if((1 == arguments.length) && (arguments[0] instanceof AudioFrame)) {
        this.channels = arguments[0].channels;
        this.samplerate = arguments[0].samplerate;
        if(null != arguments[0].data) {
            this.data = new Buffer(arguments[0].data.length);
            arguments[0].data.copy(this.data);
        } else {
            this.data = 0;
        }
        this.meta = arguments[0].meta;
        return;
    }

    this.channels = parseInt(arguments[0]);
    this.samplerate = parseInt(arguments[1]);
    this.data = (arguments[2] instanceof Buffer) ? arguments[2] : null;
    this.meta = (arguments[3] instanceof MetaData) ? arguments[3] : null;
}

AudioFrame.prototype.samplesPerChannel = function () {
    if(null != this.data) {
        return this.data.length / (4 * this.channels);
    }
    return 0;
}

AudioFrame.prototype.duration = function () {
    return this.samplesPerChannel() / this.samplerate;
}

module.exports = AudioFrame;