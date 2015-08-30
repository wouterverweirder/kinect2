var Kinect2 = require('../../lib/kinect2'), //change to 'kinect2' in a project of your own
	express = require('express'),
	app = express(),
	server = require('http').createServer(app),
	io = require('socket.io').listen(server),
	zlib = require('zlib');

var kinect = new Kinect2();

if(kinect.open()) {
	server.listen(8000);
	console.log('Server listening on port 8000');
	console.log('Point your browser to http://localhost:8000');

	app.get('/', function(req, res) {
		res.sendFile(__dirname + '/public/index.html');
	});

	app.use(express.static(__dirname + '/public'));

	// compression is used as a factor to resize the image
	// the higher this number, the smaller the image
	// make sure that the width and height (1920 x 1080) are dividable by this number
	// also make sure the canvas size in the html matches the resized size
	var compression = 3;

	var origWidth = 1920;
	var origHeight = 1080;
	var origLength = 4 * origWidth * origHeight;
	var compressedWidth = origWidth / compression;
	var compressedHeight = origHeight / compression;
	var resizedLength = 4 * compressedWidth * compressedHeight;
	//we will send a smaller image (1 / 10th size) over the network
	var resizedBuffer = new Buffer(resizedLength);
	var compressing = false;
	kinect.on('colorFrame', function(data){
		//compress the depth data using zlib
		if(!compressing) {
			compressing = true;
			//data is HD bitmap image, which is a bit too heavy to handle in our browser
			//only send every x pixels over to the browser
			var y2 = 0;
			for(var y = 0; y < origHeight; y+=compression) {
				y2++;
				var x2 = 0;
				for(var x = 0; x < origWidth; x+=compression) {
					var i = 4 * (y * origWidth + x);
					var j = 4 * (y2 * compressedWidth + x2);
					resizedBuffer[j] = data[i];
					resizedBuffer[j+1] = data[i+1];
					resizedBuffer[j+2] = data[i+2];
					resizedBuffer[j+3] = data[i+3];
					x2++;
				}
			}

			zlib.deflate(resizedBuffer, function(err, result){
				if(!err) {
					var buffer = result.toString('base64');
					io.sockets.sockets.forEach(function(socket){
						socket.volatile.emit('colorFrame', buffer);
					});
				}
				compressing = false;
			});
		}
	});

	kinect.openColorReader();
}
