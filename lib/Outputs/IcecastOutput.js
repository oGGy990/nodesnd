var Output = require('./Output.js');
var Source = require('../Source.js');
var Encoder = require('../Encoders/Encoder.js');
var util = require('util');
var net = require('net');

util.inherits(IcecastOutput, Output);

function IcecastOutput (host, port, mountpoint, user, password, encoder, meta, source, id) {
    Output.call(this, source, id);

    this._socket = null;
    this._host = host;
    this._port = port;
    this._mountpoint = mountpoint;
    this._user = user;
    this._password = password;
    this._encoder = encoder;
    this._meta = (undefined == meta) ? null : meta;

    if(!(this._encoder instanceof Object)) {
        throw "Bad encoder given!";
    } else {
        this._encoder.on('data', (function (data) {
            if(this.STATE_STREAMING == this._connState) {
                this._socket.write(data);
            }
        }).bind(this));
    }

    this._retryTimeout = 0;
    this._connState = 0;

    if(source instanceof Source && source.available) {
        this._connect();
    }
}

IcecastOutput.prototype.STATE_HTTP_LINE = 0;
IcecastOutput.prototype.STATE_HEADERS = 1;
IcecastOutput.prototype.STATE_STREAMING = 2;

IcecastOutput.prototype._connect = function () {
    if(undefined == this._port || undefined == this._host) {
        return;
    }

    this._retryTimeout = 0;
    this._printLog("Connecting to " + this._host + ":" + this._port + this._mountpoint);

    this._socket = net.createConnection(this._port, this._host, (function () {
        var request = "SOURCE " + this._mountpoint + " HTTP/1.0\r\n" +
            "Authorization: Basic " + new Buffer(this._user + ":" + this._password).toString('base64') + "\r\n" +
            "Content-Type: " + this._encoder.type + "\r\n";

        if(null != this._meta) {
            if(undefined != this._meta.name) {
                request += "ice-name: " + this._meta.name + "\r\n";
            }
            if(undefined != this._meta.desc) {
                request += "ice-description: " + this._meta.desc + "\r\n";
            }
            if(undefined != this._meta.genre) {
                request += "ice-genre: " + this._meta.genre + "\r\n";
            }
            if(undefined != this._meta.url) {
                request += "ice-url: " + this._meta.url + "\r\n";
            }
        }
        request += "\r\n";
        this._socket.write(request);

        this._connState = this.STATE_HTTP_LINE;

        this._socket.on('data', (function (data) {
            if(this.STATE_STREAMING == this._connState) {
                return;
            } else if ((this.STATE_HTTP_LINE == this._connState) || (this.STATE_HEADERS == this._connState)) {
                var lines = data.toString().split("\r\n");

                for(var i = 0; i < lines.length; i += 1) {
                    if(this.STATE_HTTP_LINE == this._connState) {
                        if(0 == lines[i].indexOf('HTTP/')) {
                            var parts = lines[i].split(' ', 3);
                            var result = parseInt(parts[1]);

                            if(401 == result) {
                                this._printWarn("Connection failed: Bad credentials");
                                this._disconnect();
                                return;
                            } else if (403 == result) {
                                this._printWarn("Connection failed: Mountpoint in use");
                                this._disconnect();
                                return;
                            } else if(200 != result) {
                                this._printWarn("Connection failed: HTTP status " + result);
                                this._disconnect();
                                return;
                            }

                            this._connState = this.STATE_HEADERS;
                        } else {
                            this._printWarn("Connection failed: Bad response");
                            this._disconnect();
                            return;
                        }
                    } else if(this.STATE_HEADERS == this._connState) {
                        if(lines[i].length == 0) {
                            this._printLog("Connection established");

                            if(!this._encoder.init()) {
                                this._printErr("Failed to init encoder, aborting...");
                                this._disconnect();
                                return;
                            } else {
                                this._printLog("Encoder " + this._encoder.constructor.name + " successfully initialized");
                            }

                            this._connState = this.STATE_STREAMING;
                            this._setAvailable(true);
                        }
                    }
                }
            }
        }).bind(this));
    }).bind(this));

    this._socket.on('error', (function (error) {
        this._printWarn("Connection failed: " + error.toString());
    }).bind(this));

    this._socket.on('close', (function (error) {
        if(!error) {
            this._printLog("Connection closed");
        }
        this._disconnect();
        this._retryTimeout = setTimeout(this._connect.bind(this), 2000);
    }).bind(this));
}

IcecastOutput.prototype._disconnect = function () {
    if(null != this._socket) {
        if(0 != this._retryTimeout) {
            clearTimeout(this._retryTimeout);
            this._retryTimeout = 0;
        }

        this._encoder.finish();

        this._socket.end();
        this._socket.destroy();
        this._socket = null;

        this._connState = this.STATE_HTTP_LINE;

        this._setAvailable(false);
    }
}

IcecastOutput.prototype._changedSourceAvailability = function (source, avail) {
    if(avail && (null == this._socket)) {
        this._connect();
    } else if(!avail && (null != this._socket)) {
        this._disconnect();
    }
}

IcecastOutput.prototype._handleFrame = function (frame) {
    if(this.STATE_STREAMING == this._connState) {
        // Check if we need to resample
        if(frame.samplerate != this._encoder.samplerate) {
            this._printErr("Need resampling " +  frame.samplerate + " -> " + this._encoder.samplerate + "!");
            return;
        }
        // Pass samples on to encoder
        this._encoder.encode(frame);
    }
}

module.exports = IcecastOutput;