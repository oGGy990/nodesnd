var Source = require('./../Source.js');
var util = require('util');
var http = require('http');
var decoderFactory = require('./../DecoderFactory.js');

util.inherits(HTTPSource, Source);

function HTTPSource (options) {
    Source.call(this);

    this._http = null;
    this._url = options.url;
    this._decoder = null;
}

HTTPSource.prototype._connect = function () {
    if(null === this._http) {
        this._http = http.get(this._url, (function (resp) {
            console.log(resp.headers);

            this._decoder = decoderFactory.newDecoderForType(resp.headers['content-type']);
            this._decoder.on('data', (function(data) {
                this.push(data);
            }).bind(this));

            resp.on('data', (function (data) {
                this._decoder.write(data);
            }).bind(this));
        }).bind(this));

        this._http.end();
    }
}

HTTPSource.prototype._disconnect = function () {
    if(null !== this._http) {
        this._http.abort();
        this._http = null;

        this._decoder = null;
    }
}

HTTPSource.prototype.isConnected = function() {
    return (null !== this._http);
}

HTTPSource.prototype._read = function (size) {
    if(!this.isConnected()) {
        this._connect();
    }
}

module.exports = HTTPSource;