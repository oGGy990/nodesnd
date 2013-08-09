var Source = require('../Source.js');
var util = require('util');
var AudioFrame = require('../AudioFrame.js');

util.inherits(SafeSource, Source);

function SafeSource (source, id) {
    if(!(source instanceof Source)) {
        throw "Bad source given!";
    }

    Source.call(this, id, source, false);
    this._setAvailable(true);
}

SafeSource.prototype.destroy = function () {
    clearInterval(this._pushInterval);
    this._pushInterval = 0;

    SafeSource.super_.prototype.destroy.apply(this);
}

SafeSource.prototype._changedSourceAvailability = function (source, avail) {
    if(avail) {
        this._printLog("Source " + source.constructor.name + " (" + source.id + ") is available, forwarding...");

        if((undefined != this._pushInterval) && (0 != this._pushInterval)) {
            clearInterval(this._pushInterval);
        }
        this._pushInterval = 0;
    } else {
        this._printLog("Source " + source.constructor.name + " (" + source.id + ") failed, playing silence...");

        this._pushInterval = setInterval((function () {
            var buf = new Buffer(2 * 22050 * 4);
            buf.fill(0);
            this._pushFrame(new AudioFrame(2, 44100, buf));
        }).bind(this), 900);
    }
}

module.exports = SafeSource;