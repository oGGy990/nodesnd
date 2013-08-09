var uuid = require('node-uuid');

function Context () {
    this.version = '0.1.0';
    this.versionMajor = 0;
    this.versionMinor = 1;
    this.versionPatch = 0;

    this._sourceMap = {};
}

Context.prototype._registerSource = function(source) {
    if(this.hasSource(source.id)) {
        throw "Two sources with identical id \'" + source.id + "\'!";
    }

    this._sourceMap[source.id] = source;
}

Context.prototype._unregisterSource = function(source) {
    if(this.hasSource(source.id)) {
        return;
    }

    this._sourceMap[source.id] = null;
}

Context.prototype.hasSource = function(id) {
    return !((undefined == this._sourceMap[id]) || (null == this._sourceMap[id]));
}

Context.prototype.getSource = function(id) {
    var src = this._sourceMap[id];
    if(undefined == src) {
        return null;
    }
    return src;
}

Context.prototype.getSourceId = function() {
    return uuid.v4();
}

module.exports = new Context();