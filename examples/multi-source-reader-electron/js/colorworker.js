// -----------------------------------------------------------------------
// Inspired by KinectWorker-1.8.0.js from the 1.8 Microsoft SDK.
// -----------------------------------------------------------------------

(function(){

	var imageData;

	function init() {
		addEventListener('message', function (event) {
			switch (event.data.message) {
				case "setImageData":
					imageData = event.data.imageData;
					break;
				case "processImageData":
					processImageData(event.data.imageBuffer);
					break;
			}
		});
	}

	//newPixelData is Uint8Array() in RGBA format
	function processImageData(newPixelData) {
		var pixelArray = imageData.data;
		var imageDataSize = imageData.data.length;
		for (var i = 0; i < imageDataSize; i++) {
			imageData.data[i] = newPixelData[i];
		}
		self.postMessage({ "message": "imageReady", "imageData": imageData });
	}

	init();
})();
