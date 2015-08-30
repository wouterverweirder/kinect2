var Kinect2 = require('../../lib/kinect2'), //change to 'kinect2' in a project of your own
	lwip = require('lwip'),
	express = require('express'),
	app = express(),
	server = require('http').createServer(app),
	io = require('socket.io').listen(server);

var kinect = new Kinect2();

if(kinect.open()) {
	server.listen(8000);
	console.log('Server listening on port 8000');
	console.log('Point your browser to http://localhost:8000');

	app.get('/', function(req, res) {
		res.sendFile(__dirname + '/public/index.html');
	});

	app.use(express.static(__dirname + '/public'));

	var w = 1920;
	var h = 1080;

	var size = 4 * w * h;
	var channelSize = size / 4;
	var redOffset = 0;
	var greenOffset = channelSize;
	var blueOffset = channelSize * 2;
	var alphaOffset = channelSize * 3;
	var lwipBuffer = new Buffer(size);
	var processingColorFrame = false;

	kinect.on('colorFrame', function(data){
		if(processingColorFrame) {
			return;
		}
		processingColorFrame = true;
		//swap data image to lwip buffer
		for(var i = 0; i < channelSize; i++) {
			lwipBuffer[i + redOffset] = data[i * 4];
			lwipBuffer[i + greenOffset] = data[i * 4 + 1];
			lwipBuffer[i + blueOffset] = data[i * 4 + 2];
			lwipBuffer[i + alphaOffset] = 100;
		}
		lwip.open(lwipBuffer, {width: w, height: h}, function(err, image){
			if(err) {
				processingColorFrame = false;
				return;
			}
			image.batch()
				.toBuffer('jpg', { quality: 50 }, function(err, buffer){
					if(err) {
						processingColorFrame = false;
						return;
					}
					io.sockets.sockets.forEach(function(socket){
						socket.volatile.emit('colorFrame', buffer);
					});
					processingColorFrame = false;
				});
		});
	});
	kinect.openColorReader();
}
