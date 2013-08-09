var Source = require('../Source.js');
var util = require('util');
var AudioFrame = require('../AudioFrame.js');

util.inherits(PrioritySource, Source);

function PrioritySource (id) {
    Source.call(this, id, null, false);

    this._priority = [];
    this._last = null;
    this._current = null;
    this._reordering = false;
}

PrioritySource.prototype.destroy = function () {
    clearInterval(this._pushInterval);
    this._pushInterval = 0;

    SafeSource.super_.prototype.destroy.apply(this);
}

PrioritySource.prototype.addSource = function (source) {
    if(!(source instanceof Source)) {
        throw "Bad source given!";
    }

    this._priority.push(source);
    source._addWatcher(this);

    this._updateSources();
}

PrioritySource.prototype._updateSources = function () {
    var next = null;

    for(var i = 0; i < this._priority.length; i += 1)
    {
        // Current source comes first
        if(this._priority[i] == this._current) {

            // Current source is no longer available?
            if(!this._current.available) {
                // Go on searching
                continue;
            }

            // No change needed
            return;
        }

        // Source is available? Take it!
        if(this._priority[i].available) {
            next = this._priority[i];
            break;
        }
    }

    this._reordering = true;

    // No sources available, we're unavailable
    if(null == next) {
        this._printLog("No sources available, going unavailable");

        if(null != this._current) {
            this._removeSource(this._current);
            this._current._addWatcher(this);
            this._current = null;
        }
        this._setAvailable(false);
    } else {
        this._printLog("Sources changed, switching to " + next.constructor.name + " (" + next.id + ")");

        this._last = this._current;
        if(null != this._current) {
            this._removeSource(this._current);
            this._current._addWatcher(this);
        }

        this._current = next;
        this._current._removeWatcher(this);
        this._addSource(this._current);

        this._setAvailable(true);
    }

    this._reordering = false;
}

PrioritySource.prototype._changedSourceAvailability = function (source, avail) {
    if(!this._reordering) {
        this._updateSources();
    }
}

module.exports = PrioritySource;