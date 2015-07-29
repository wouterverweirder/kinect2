(function(){
	var Kinect2 = require('../../../kinect2');
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

	if(kinect.open()) {
		kinect.on('multiSourceFrame', function(frame){

			//show color pixels of first user we find
			if(!colorProcessing) {
				//player
				for (var player = 0; player < frame.bodyIndexColor.bodies.length; player++) {
					if(frame.bodyIndexColor.bodies[player].buffer) {
						colorProcessing = true;
						//copy Buffer into Uint8Array
						colorPixels = new Uint8Array(frame.bodyIndexColor.bodies[player].buffer);
						//transferable object to worker thread
						colorWorkerThread.postMessage(colorPixels.buffer, [colorPixels.buffer]);
						break;
					}
				}
				if(!colorProcessing)
				{
					//still not processing -> no body has been found, clear the canvas
					colorCtx.clearRect(0, 0, colorCanvas.width, colorCanvas.height);
				}
			}
		});

		kinect.openMultiSourceReader({
			frameTypes: Kinect2.FrameTypes.BodyIndexColor
		});
	}
	})();
