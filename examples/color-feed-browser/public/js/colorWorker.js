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

    function processImageData(imageBuffer) {
        var pixelArray = imageData.data;
        var newPixelData = new Uint8Array(imageBuffer);
        var imageDataSize = imageData.data.length;
        for (var i = 0; i < imageDataSize; i++) {
            imageData.data[i] = newPixelData[i];
        }
        self.postMessage({ "message": "imageReady", "imageData": imageData });
    }

    init();
})();