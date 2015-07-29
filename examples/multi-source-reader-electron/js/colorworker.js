// -----------------------------------------------------------------------
// Inspired by KinectWorker-1.8.0.js from the 1.8 Microsoft SDK.
// -----------------------------------------------------------------------

(function(){

	var imageData;
	var imageDataSize;

	function init() {
		addEventListener('message', function (event) {
			if(!imageData)
			{
				imageData = event.data;
				imageDataSize = imageData.data.length;
			}
			else
			{
				processImageData(event.data);
			}
		});
	}

	//newPixelData is an ArrayBuffer with RGBA Data
	function processImageData(newPixelData) {
		//create new Uint8Array to be able to read from the ArrayBuffer
		var newPixelData = new Uint8Array(newPixelData);
		var pixelArray = imageData.data;
		for (var i = 0; i < imageDataSize; i++) {
			imageData.data[i] = newPixelData[i];
		}
		self.postMessage(imageData);
	}

	init();
})();
