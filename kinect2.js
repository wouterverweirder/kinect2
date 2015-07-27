var events = require('events'),
	util = require('util');

function Kinect2() {
	events.EventEmitter.call(this);

	this.nativeKinect2 = require('./build/Release/kinect2');
}

util.inherits(Kinect2, events.EventEmitter);

Kinect2.FrameTypes = {
	None														: 0,
	Color														: 0x1,
	Infrared												: 0x2, //Not Implemented Yet
	LongExposureInfrared						: 0x4, //Not Implemented Yet
	Depth														: 0x8,
	BodyIndex												: 0x10, //Not Implemented Yet
	Body														: 0x20,
	Audio														: 0x40, //Not Implemented Yet
	BodyIndexColor									: 0x80,
	BodyIndexDepth									: 0x10, //Same as BodyIndex
	BodyIndexInfrared								: 0x100, //Not Implemented Yet
	BodyIndexLongExposureInfrared		: 0x200, //Not Implemented Yet
	RawDepth												: 0x400 //Not Implemented Yet
};

Kinect2.prototype.open = function() {
	return this.nativeKinect2.open();
};

Kinect2.prototype.openMultiSourceReader = function(options) {
	return this.nativeKinect2.openMultiSourceReader({
		frameTypes: options.frameTypes,
		callback: this.multiSourceFrameCallback.bind(this)
	});
};

Kinect2.prototype.closeMultiSourceReader = function() {
	return this.nativeKinect2.closeMultiSourceReader();
};

Kinect2.prototype.openDepthReader = function() {
	return this.nativeKinect2.openDepthReader(this.depthFrameCallback.bind(this));
};

Kinect2.prototype.closeDepthReader = function() {
	return this.nativeKinect2.closeDepthReader();
};

Kinect2.prototype.openColorReader = function() {
	return this.nativeKinect2.openColorReader(this.colorFrameCallback.bind(this));
};

Kinect2.prototype.closeColorReader = function() {
	return this.nativeKinect2.closeColorReader();
};

Kinect2.prototype.openInfraredReader = function() {
	return this.nativeKinect2.openInfraredReader(this.infraredFrameCallback.bind(this));
};

Kinect2.prototype.closeInfraredReader = function() {
	return this.nativeKinect2.closeInfraredReader();
};

Kinect2.prototype.openLongExposureInfraredReader = function() {
	return this.nativeKinect2.openLongExposureInfraredReader(this.longExposureInfraredFrameCallback.bind(this));
};

Kinect2.prototype.closeLongExposureInfraredReader = function() {
	return this.nativeKinect2.closeLongExposureInfraredReader();
};

Kinect2.prototype.openBodyReader = function() {
	return this.nativeKinect2.openBodyReader(this.bodyFrameCallback.bind(this));
};

Kinect2.prototype.closeBodyReader = function() {
	return this.nativeKinect2.closeBodyReader();
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

Kinect2.prototype.multiSourceFrameCallback = function(err, frame) {
	this.emit('multiSourceFrame', err, frame);
};

module.exports = Kinect2;
