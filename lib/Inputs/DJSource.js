var Source = require('../Source.js');
var util = require('util');
var net = require('net');
var decoderFactory = require('../DecoderFactory.js');
var AudioFrame = require('../AudioFrame.js');

util.inherits(DJSource, Source);

function DJSource (options, id) {
    Source.call(this, id, null, false);

    this._port = options.port;
    this._mountpoint = options.mountpoint;
    this._authFunction = options.auth;
    this._decoder = null;
    this._client = null;

    this._server = net.createServer((function(client) {
        var self = this;

        client.on('data', function(data) {
            if(undefined == this.sourceState || self.STATE_HEADERS == this.sourceState) {
                var lines = data.toString().split("\r\n");
                console.log(lines);

                for(var i = 0; i < (lines.length - 1); i += 1) {
                    if(undefined == this.sourceState) {
                        if(lines[i].indexOf("HTTP/1.") != (lines[i].length - 8)) {
                            this.end();
                            return;
                        }

                        if(0 == lines[i].indexOf("SOURCE ")) {
                            var mp = lines[i].substring(7, lines[i].length - 8).trim();

                            if(self._mountpoint !== mp) {
                                this._printLog("Got request for bad mountpoint: " + mp);
                                this.end("HTTP/1.0 404 File Not Found\r\n\r\nUnknown mountpoint!\r\n");
                                return;
                            }
                        } else {
                            this.end("HTTP/1.0 405 Method Not Supported\r\n\r\nOnly SOURCE is supported!\r\n");
                            return;
                        }

                        this.sourceState = self.STATE_HEADERS;
                    } else {
                        if(0 == lines[i].length) {
                            if(undefined == this.authorize) {
                                this.end("HTTP/1.0 403 Forbidden\r\n\r\nYou need to authenticate!\r\n");
                                return;
                            }

                            if(null != self._client) {
                                this.end("HTTP/1.0 403 Forbidden\r\n\r\nMountpoint in use!\r\n");
                                return;
                            }

                            if(undefined == this.contentType) {
                                this.end("HTTP/1.0 403 Bad Request\r\n\r\nNeed content-type!\r\n");
                                return;
                            }

                            self._decoder = decoderFactory.newDecoderForType(this.contentType);
                            self._decoder.on('data', (function(data) {
                                self._pushFrame(new AudioFrame(2, 44100, data));
                            }).bind(self));

                            var parts = this.authorize.split(":", 2);
                            if((2 != parts.length) || !self._authFunction(parts[0], parts[1])) {
                                this._printLog("Got invalid login: " + this.authorize);
                                this.end("HTTP/1.0 403 Forbidden\r\n\r\nBad credentials!\r\n");
                                return;
                            }

                            this.write("HTTP/1.0 200 OK\r\n\r\n");

                            self._client = this;
                            this.sourceState = self.STATE_STREAMING;
                            self._setAvailable(true);
                            return;
                        }

                        var parts = lines[i].split(":", 2);
                        if(2 != parts.length) {
                            this.end("HTTP/1.0 403 Bad Request\r\n\r\nBad header!\r\n");
                            return;
                        }

                        var name = parts[0].toLowerCase().trim();
                        var value = parts[1].trim();

                        if(name === "content-type") {
                            this.contentType = value.toLowerCase();
                        } else if(name === "authorization") {
                            value = value.trim().split(" ");
                            value = value[value.length - 1];
                            this.authorize = new Buffer(value, 'base64').toString('utf8');
                        }
                    }
                }
            } else {
                self._decoder.write(data);
            }
        });

        client.on('end', function() {
           if(this == self._client) {
               self._printLog("DJ disconnected");
               self._client = null;
               self._decoder = null;
               self._setAvailable(false);
           }
        });
    }).bind(this));

    this._server.timeout = 30000;
    this._server.listen(this._port);
}

DJSource.prototype.STATE_HEADERS = 0;
DJSource.prototype.STATE_STREAMING = 1;

DJSource.prototype.isConnected = function() {
    return (null != this._client);
}

module.exports = DJSource;