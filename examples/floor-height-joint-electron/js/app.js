(function(){

	var Kinect2 = require('../../lib/kinect2');
	var kinect = new Kinect2();

	var colorCanvas = document.getElementById('colorCanvas');
	var colorCtx = colorCanvas.getContext('2d');

	var hiddenCanvas = document.createElement('canvas');
	hiddenCanvas.setAttribute('width', 1920);
	hiddenCanvas.setAttribute('height', 1080);

	var ImageBufferRendererWebgl = require('./js/image-buffer-renderer-webgl.js');
	var colorRenderer = new ImageBufferRendererWebgl(hiddenCanvas);
	var trackedBodyIndex = -1;
	var emptyPixels = new Uint8Array(1920 * 1080 * 4);

	function processImageData(imageBuffer, width, height) {

		if (colorRenderer.isProcessing || (width <= 0) || (height <= 0)) {
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

	if(kinect.open()) {
		kinect.on('multiSourceFrame', function(frame){
			//draw camera feed - fast to hidden webgl canvas - copy to visible 2D canvas
			processImageData(frame.color.buffer, colorCanvas.width, colorCanvas.height);
			colorCtx.drawImage(hiddenCanvas, 0, 0);
			//
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
					//measure distance from floor
					if(frame.body.floorClipPlane)
					{
						//get position of left hand
						var joint = frame.body.bodies[closestBodyIndex].joints[Kinect2.JointType.handLeft];

						//https://social.msdn.microsoft.com/Forums/en-US/594cf9ed-3fa6-4700-872c-68054cac5bf0/angle-of-kinect-device-and-effect-on-xyz-positional-data?forum=kinectv2sdk
						var cameraAngleRadians= Math.atan(frame.body.floorClipPlane.z / frame.body.floorClipPlane.y);
						var cosCameraAngle = Math.cos(cameraAngleRadians);
						var sinCameraAngle = Math.sin(cameraAngleRadians);
						var yprime = joint.cameraY * cosCameraAngle + joint.cameraZ * sinCameraAngle;
						var jointDistanceFromFloor = frame.body.floorClipPlane.w + yprime;

						//show height in canvas
						colorCtx.beginPath();
						colorCtx.fillStyle = "red";
						colorCtx.arc(joint.colorX * 1920, joint.colorY * 1080, 10, 0, Math.PI * 2, true);
						colorCtx.fill();
						colorCtx.closePath();
						colorCtx.font = "48px sans";
						colorCtx.fillText(jointDistanceFromFloor.toFixed(2) + "m", 20 + joint.colorX * 1920, joint.colorY * 1080);
					}
				}
			}

			trackedBodyIndex = closestBodyIndex;
		});

		kinect.openMultiSourceReader({
			frameTypes: Kinect2.FrameType.body | Kinect2.FrameType.color
		});
	}
	})();
