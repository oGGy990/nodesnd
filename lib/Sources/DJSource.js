var Source = require('./../Source.js');
var util = require('util');
var http = require('http');
var decoderFactory = require('./../DecoderFactory.js');

util.inherits(DJSource, Source);

function DJSource (options) {
    Source.call(this);

    this._port = options.port;
    this._mountpoint = options.mountpoint;
    this._authFunction = options.auth;
    this._decoder = null;
    this._used = false;

    this._http = http.createServer((function(request, response) {
        if('SOURCE' !== request.method) {
            response.writeHead(405);
            response.write("Only SOURCE is supported!")
            response.end();
            return;
        }

        if(request.url != this._mountpoint) {
            response.writeHead(404);
            response.write("Unknown mountpoint!");
            response.end();
            return;
        }

        if(this._used) {
            response.writeHead(403);
            response.write("Mountpoint in use!");
            response.end();
            return;
        }

        response.writeHead(200);

        this._decoder = decoderFactory.newDecoderForType(request.headers['content-type']);
        this._decoder.on('data', (function(data) {
            this.push(data);
        }).bind(this));

        request.on('data', (function (data) {
            this._decoder.write(data);
        }).bind(this));
    }).bind(this));

    this._http.timeout = 30000;
    this._http.listen(this._port);
}

DJSource.prototype.isConnected = function() {
    return this._used;
}

DJSource.prototype._read = function (size) {
}

module.exports = DJSource;