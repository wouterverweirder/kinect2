(function(){
	var Kinect2 = require('../../lib/kinect2');
	var kinect = new Kinect2();

	var colorCanvas = document.getElementById('colorCanvas');
	var colorCtx = colorCanvas.getContext('2d');
	var colorImageData = colorCtx.createImageData(colorCanvas.width, colorCanvas.height);
	var colorImageDataSize = colorImageData.data.length;
	var colorProcessing = false;
	var colorWorkerThread = new Worker("js/colorworker.js");
	var colorPixels = new Uint8Array(colorCanvas.width * colorCanvas.height * 4);
	colorWorkerThread.addEventListener("message", function (event) {
		colorCtx.clearRect (0, 0, colorCanvas.width, colorCanvas.height);
		colorCtx.putImageData(event.data, 0, 0);
		colorProcessing = false;
	});
	colorWorkerThread.postMessage(colorImageData);

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

	if(kinect.open()) {
		kinect.on('multiSourceFrame', function(frame){
			if(colorProcessing) {
				return;
			}
			var closestBodyIndex = getClosestBodyIndex(frame.body.bodies);
			if(closestBodyIndex !== trackedBodyIndex) {
				if(closestBodyIndex > -1) {
					kinect.trackPixelsForBodyIndices([closestBodyIndex]);
				} else {
					kinect.trackPixelsForBodyIndices(false);
					colorCtx.clearRect(0, 0, colorCanvas.width, colorCanvas.height);
				}
			}
			else {
				if(closestBodyIndex > -1) {
					if(frame.bodyIndexColor.bodies[closestBodyIndex].buffer) {
						colorProcessing = true;
						//copy Buffer into Uint8Array
						colorPixels = new Uint8Array(frame.bodyIndexColor.bodies[closestBodyIndex].buffer);
						//transferable object to worker thread
						colorWorkerThread.postMessage(colorPixels.buffer, [colorPixels.buffer]);
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
