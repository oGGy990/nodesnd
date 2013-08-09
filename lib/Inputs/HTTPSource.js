var Source = require('../Source.js');
var util = require('util');
var http = require('http');
var decoderFactory = require('../DecoderFactory.js');
var AudioFrame = require('../AudioFrame.js');

util.inherits(HTTPSource, Source);

function HTTPSource (options, id) {
    Source.call(this, id, null, false);
    this._setAvailable(true);

    this._http = null;
    this._url = options.url;
    this._decoder = null;
}

HTTPSource.prototype._connect = function () {
    if(null === this._http) {
        this._printLog("Connecting to " + this._url);

        this._http = http.get(this._url, (function (resp) {
            this._printLog("Connected, searching for codec for type '" + resp.headers['content-type'] + "'");

            this._decoder = decoderFactory.newDecoderForType(resp.headers['content-type']);

            this._printLog("Using decoder " + this._decoder.constructor.name);


            this._decoder.on('data', (function(data) {
                this._pushFrame(new AudioFrame(2, 44100, data));
            }).bind(this));

            resp.on('data', (function (data) {
                this._decoder.write(data);
            }).bind(this));

            resp.on('error', (function (error) {
                this._printLog("Connection error" + error);
                this._disconnect();
                this._connect();
            }).bind(this))

            resp.on('end', (function () {
                this._printLog("Disconnect by remote");
                this._disconnect();
                if(this.active) {
                    this._connect();
                }
            }).bind(this));
        }).bind(this));

        this._http.end();
    }
}

HTTPSource.prototype._disconnect = function () {
    if(null !== this._http) {
        this._printLog("Disconnected");

        this._http.abort();
        this._http = null;

        this._decoder = null;
    }
}

HTTPSource.prototype.isConnected = function() {
    return (null !== this._http);
}

HTTPSource.prototype._activate = function () {
    if(!this.isConnected()) {
        this._connect();
    }
}

HTTPSource.prototype._deactivate = function () {
    this._disconnect();
}

module.exports = HTTPSource;