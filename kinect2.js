var events = require('events'),
	util = require('util');

function Kinect2() {
	events.EventEmitter.call(this);

	this.nativeKinect2 = require('./build/Release/kinect2');
}

util.inherits(Kinect2, events.EventEmitter);

Kinect2.FrameType = {
	none														: 0,
	color														: 0x1,
	infrared												: 0x2, //Not Implemented Yet
	longExposureInfrared						: 0x4, //Not Implemented Yet
	depth														: 0x8,
	bodyIndex												: 0x10, //Not Implemented Yet
	body														: 0x20,
	audio														: 0x40, //Not Implemented Yet
	bodyIndexColor									: 0x80,
	bodyIndexDepth									: 0x10, //Same as BodyIndex
	bodyIndexInfrared								: 0x100, //Not Implemented Yet
	bodyIndexLongExposureInfrared		: 0x200, //Not Implemented Yet
	rawDepth												: 0x400 //Not Implemented Yet
};

Kinect2.JointType = {
	spineBase 			: 0,
	spineMid 				: 1,
	neck 						: 2,
	head 						: 3,
	shoulderLeft 		: 4,
	elbowLeft 			: 5,
	wristLeft 			: 6,
	handLeft 				: 7,
	shoulderRight 	: 8,
	elbowRight 			: 9,
	wristRight 			: 10,
	handRight 			: 11,
	hipLeft 				: 12,
	kneeLeft 				: 13,
	ankleLeft 			: 14,
	footLeft 				: 15,
	hipRight 				: 16,
	kneeRight 			: 17,
	ankleRight 			: 18,
	footRight 			: 19,
	spineShoulder 	: 20,
	handTipLeft 		: 21,
	thumbLeft 			: 22,
	handTipRight 		: 23,
	thumbRight 			: 24
};

Kinect2.HandState = {
	unknown 		: 0,
	notTracked 	: 1,
	open 				: 2,
	closed 			: 3,
	lasso 			: 4
};

Kinect2.prototype.open = function() {
	//cheap way of preventing application exit
	this._openIntervalId = setInterval(function(){}, 1000);
	return this.nativeKinect2.open();
};

Kinect2.prototype.close = function() {
	//todo: add callback function so that kinect property closes
	clearInterval(this._openIntervalId);
	return this.nativeKinect2.close();
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

/**
 * Specify which body indices you want to have pixel data from
 * Used when running the multisource reader to get green screen effects
 * provide an array with body indices (0 -> 5)
 */
Kinect2.prototype.trackPixelsForBodyIndices = function(bodyIndices) {
	return this.nativeKinect2.trackPixelsForBodyIndices(bodyIndices);
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

Kinect2.prototype.multiSourceFrameCallback = function(frame) {
	this.emit('multiSourceFrame', frame);
};

module.exports = Kinect2;
