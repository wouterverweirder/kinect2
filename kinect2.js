var edge = require('edge'),
	path = require('path')
	events = require('events'),
	util = require('util');

function Kinect2() {
	events.EventEmitter.call(this);
	this.edge = {};
	this.edge.open = edge.func({
		assemblyFile: __dirname + path.sep + 'NodeKinect2.dll',
	    typeName: 'NodeKinect2.Startup',
	    methodName: 'Open'
	});
	this.edge.close = edge.func({
		assemblyFile: __dirname + path.sep + 'NodeKinect2.dll',
	    typeName: 'NodeKinect2.Startup',
	    methodName: 'Close'
	});
}

util.inherits(Kinect2, events.EventEmitter);

Kinect2.prototype.open = function() {
	return this.edge.open({
		logCallback: this.logCallback.bind(this),
		bodyFrameCallback: this.bodyFrameCallback.bind(this),
		depthFrameCallback: this.depthFrameCallback.bind(this)
	}, true);
};

Kinect2.prototype.close = function() {
	return this.edge.close("", true);
};

Kinect2.prototype.logCallback = function(msg) {
	console.log('[Kinect2]', msg);
};

Kinect2.prototype.bodyFrameCallback = function(input, callback) {
	this.emit('bodyFrame', input);
};

Kinect2.prototype.depthFrameCallback = function(data) {
	this.emit('depthFrame', data);
};

module.exports = Kinect2;