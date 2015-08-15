(function(){
	var Kinect2 = require('../../lib/kinect2');
	var kinect = new Kinect2();

	var colorCanvas = document.getElementById('colorCanvas');

	var ImageBufferRendererWebgl = require('../shared/js/image-buffer-renderer-webgl.js');
	var colorRenderer = new ImageBufferRendererWebgl(colorCanvas);

	function processImageData(imageBuffer, width, height) {

		if (colorRenderer.isProcessing || (width <= 0) || (height <= 0)) {
			// Don't start processing new data when we are in the middle of
			// processing data already.
			// Also, Only do work if image data to process is of the expected size
			return;
		}

		colorRenderer.isProcessing = true;
		colorRenderer.processImageData(imageBuffer, width, height);
		colorRenderer.isProcessing = false;
	}

	function getClosestBodyIndex(bodies) {
		var closestZ = Number.MAX_VALUE;
		var closestBodyIndex = -1;
		for(var i = 0; i < bodies.length; i++) {
			if(bodies[i].tracked && bodies[i].joints[Kinect2.JointType.spineMid].cameraZ < closestZ) {
				closestZ = bodies[i].joints[Kinect2.JointType.spineMid].cameraZ;
				closestBodyIndex = i;
			}
		}
		return closestBodyIndex;
	}

	var trackedBodyIndex = -1;
	var emptyPixels = new Uint8Array(1920 * 1080 * 4);

	if(kinect.open()) {
		kinect.on('multiSourceFrame', function(frame){
			var closestBodyIndex = getClosestBodyIndex(frame.body.bodies);
			if(closestBodyIndex !== trackedBodyIndex) {
				if(closestBodyIndex > -1) {
					kinect.trackPixelsForBodyIndices([closestBodyIndex]);
				} else {
					kinect.trackPixelsForBodyIndices(false);
					//clear canvas
					processImageData(emptyPixels.buffer, colorCanvas.width, colorCanvas.height);
				}
			}
			else {
				if(closestBodyIndex > -1) {
					if(frame.bodyIndexColor.bodies[closestBodyIndex].buffer) {
						processImageData(frame.bodyIndexColor.bodies[closestBodyIndex].buffer, colorCanvas.width, colorCanvas.height);
					}
				}
			}
			trackedBodyIndex = closestBodyIndex;
		});

		kinect.openMultiSourceReader({
			frameTypes: Kinect2.FrameType.bodyIndexColor | Kinect2.FrameType.body
		});
	}
	})();
