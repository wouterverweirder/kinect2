var events = require('events'),
	util = require('util');

function Kinect2(options) {
	events.EventEmitter.call(this);

	this.nativeKinect2 = require('./build/Release/kinect2');
}

util.inherits(Kinect2, events.EventEmitter);

Kinect2.prototype.open = function() {
	return this.nativeKinect2.open();
};

Kinect2.prototype.openDepthReader = function() {
};

Kinect2.prototype.openColorReader = function() {
};

Kinect2.prototype.openInfraredReader = function() {
};

Kinect2.prototype.openLongExposureInfraredReader = function() {
};

Kinect2.prototype.openBodyReader = function() {
	return this.nativeKinect2.openBodyReader(this.bodyFrameCallback.bind(this));
};

Kinect2.prototype.close = function() {
	return this.nativeKinect2.close();
};

Kinect2.prototype.logCallback = function(msg) {
	console.log('[Kinect2]', msg);
};

Kinect2.prototype.bodyFrameCallback = function(bodies) {
	this.emit('bodyFrame', bodies);
};

Kinect2.prototype.depthFrameCallback = function(data) {
	this.emit('depthFrame', data);
};

Kinect2.prototype.colorFrameCallback = function(data) {
	this.emit('colorFrame', data);
};

Kinect2.prototype.infraredFrameCallback = function(data) {
	this.emit('infraredFrame', data);
};

Kinect2.prototype.longExposureInfraredFrameCallback = function(data) {
	this.emit('longExposureInfraredFrame', data);
};

module.exports = Kinect2;