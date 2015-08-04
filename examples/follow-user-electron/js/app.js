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
					//get body ground position - when use jumps this point stays on the ground
					if(frame.body.bodies[closestBodyIndex].joints[Kinect2.JointType.spineMid].floorColorY)
					{
						//calculate the source rectangle
						var leftJoint = frame.body.bodies[closestBodyIndex].joints[0],
								topJoint = frame.body.bodies[closestBodyIndex].joints[0],
								rightJoint = frame.body.bodies[closestBodyIndex].joints[0];
						for(var i = 1; i < frame.body.bodies[closestBodyIndex].joints.length; i++) {
							var joint = frame.body.bodies[closestBodyIndex].joints[i];
							if(joint.colorX < leftJoint.colorX) {
								leftJoint = joint;
							}
							if(joint.colorX > rightJoint.colorX) {
								rightJoint = joint;
							}
							if(joint.colorY < topJoint.colorY) {
								topJoint = joint;
							}
						}
						var srcRect = {
							x: leftJoint.colorX * hiddenCanvas.width,
							y: topJoint.colorY * hiddenCanvas.height,
							width: (rightJoint.colorX - leftJoint.colorX) * hiddenCanvas.width,
							height: (frame.body.bodies[closestBodyIndex].joints[Kinect2.JointType.spineMid].floorColorY - topJoint.colorY) * hiddenCanvas.height
						};
						var dstRect = {
							x: Math.round((colorCanvas.width - srcRect.width) * 0.5),
							y: colorCanvas.height - srcRect.height,
							width: srcRect.width,
							height: srcRect.height
						};
						processImageData(frame.color.buffer, hiddenCanvas.width, hiddenCanvas.height);
						colorCtx.clearRect(0, 0, colorCanvas.width, colorCanvas.height);
						colorCtx.drawImage(hiddenCanvas, srcRect.x, srcRect.y, srcRect.width, srcRect.height, dstRect.x, dstRect.y, dstRect.width, dstRect.height);
					}
				}
			}

			trackedBodyIndex = closestBodyIndex;
		});

		//include the projected floor positions - we want to keep the floor on the bottom, not crop out the user in the middle of a jump
		kinect.openMultiSourceReader({
			frameTypes: Kinect2.FrameType.body | Kinect2.FrameType.color,
			includeJointFloorData: true
		});
	}
	})();
