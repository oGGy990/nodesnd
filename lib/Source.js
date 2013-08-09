var events = require('events');
var util = require('util');
var AudioFrame = require('./AudioFrame.js');
var Context = require('./Context.js');

util.inherits(Source, events.EventEmitter);

function Source (id, source, isSink) {
    events.EventEmitter.call(this);

    if(undefined == id || Context.hasSource(id)) {
        this.id = Context.getSourceId();
    } else {
        this.id = id;
    }

    this._sink = (undefined == isSink) ? false : !!(isSink);
    this.available = false;
    this.active = false;
    this._sources = [];
    this._destinations = [];
    this._watchers = [];

    Context._registerSource(this);

    if(source instanceof Source) {
        this._addSource(source);
    }
}

Source.prototype.destroy = function () {
    for(var i = 0; i < this._sources.length; i += 1) {
        this._sources[i]._removeDestination(this);
    }

    Context._unregisterSource(this);
}

Source.prototype._updateState = function() {
    var state = (this.available && ((this._destinations.length > 0) || this._sink));

    if(state != this.active) {
        this.active = state;

        if(this.active) {
            this.emit('active', this);

            this._activate();

            for(var i = 0; i < this._sources.length; i += 1) {
                this._sources[i]._addDestination(this);
            }
        } else {
            this.emit('inactive', this);

            this._deactivate();

            for(var i = 0; i < this._sources.length; i += 1) {
                this._sources[i]._removeDestination(this);
            }
        }
    }
}

Source.prototype._setAvailable = function(avail) {
    if(this.available != avail) {
        this.available = avail;

        this._updateState();

        for(var i = 0; i < this._watchers.length; i += 1) {
            this._watchers[i]._changedSourceAvailability(this, this.available);
        }
    }
}

// To be overwritten by subclasses
Source.prototype._activate = function () {
}

// To be overwritten by subclasses
Source.prototype._deactivate = function () {
}

Source.prototype._addDestination = function (source) {
    if(!(source instanceof Source)) {
        throw "Invalid source given!";
    }

    this._destinations.push(source);

    if(1 == this._destinations.length) {
        this._updateState();
    }
}

Source.prototype._removeDestination = function (source) {
    for(var i = 0; i < this._destinations.length; i += 1) {
        if(this._destinations[i] == source) {
            this._destinations.splice(i, 1);

            if(0 == this._destinations.length) {
                this._updateState();
            }

            return;
        }
    }
}

Source.prototype._addWatcher = function (source) {
    if(!(source instanceof Source)) {
        throw "Invalid source given!";
    }

    this._watchers.push(source);
}

Source.prototype._removeWatcher = function (source) {
    for(var i = 0; i < this._watchers.length; i += 1) {
        if(this._watchers[i] == source) {
            this._watchers.splice(i, 1);
            return;
        }
    }
}

Source.prototype._addSource = function (source) {
    if(!(source instanceof Source)) {
        throw "Invalid source given!";
    }

    this._sources.push(source);
    source._addWatcher(this);
    if(this.active) {
        source._addDestination(this);
    }

    this._changedSourceAvailability(source, source.available);
}

Source.prototype._removeSource = function (source) {
    for(var i = 0; i < this._sources.length; i += 1) {
        if(this._sources[i] == source) {
            this._sources.splice(i, 1);

            if(this.active) {
                source._removeDestination(this);
            }
            source._removeWatcher(this);

            this._changedSourceAvailability(source, false);

            return;
        }
    }
}

/*
    Called whenever a source changes its availability.
    Simply makes this source available, when any of its sources is available.
    Reimplement in subclass if you need different behaviour.
 */
Source.prototype._changedSourceAvailability = function (source, avail) {
    if(avail && !this.available) {
        // The first source is available, we are as well!
        this._setAvailable(true);
    } else if(!avail && this.available) {
        // Check for any available source
        for(var i = 0; i < this._sources.length; i += 1) {
            if(this._sources[i].available) {
                return;
            }
        }

        // All sources are unavailable, set also unavailable
        this._setAvailable(false);
    }
}

// Internal
Source.prototype.__handleFrame = function (frame) {
    if(!this.active) {
        return;
    }

    this._handleFrame(frame);
}

// To be overwritten by subclass
Source.prototype._handleFrame = function (frame) {
    this._pushFrame(frame);
}

Source.prototype._pushFrame = function (frame) {
    for(var i = 0; i < this._destinations.length; i += 1) {
        if(i < (this._destinations.length - 1)) {
            this._destinations[i].__handleFrame(new AudioFrame(frame));
        } else {
            this._destinations[i].__handleFrame(frame);
        }
    }
}

Source.prototype._printLog = function (text) {
    console.log(this.constructor.name + " (" + this.id + "): " + text);
}

Source.prototype._printWarn = function (text) {
    console.warn(this.constructor.name + " (" + this.id + "): " + text);
}

Source.prototype._printErr = function (text) {
    console.error(this.constructor.name + " (" + this.id + "): " + text);
}

module.exports = Source;
