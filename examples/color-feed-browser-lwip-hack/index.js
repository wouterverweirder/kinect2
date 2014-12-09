var Kinect2 = require('../../kinect2'),
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
	lwip.create(w, h, function(err, image){
		if(err) {
			return;
		}
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
			//write the lwip buffer
			//hopefully something like this will be part of the official lwip api in the future
			image.__lock();
			image.__lwip.paste(0, 0, lwipBuffer, w, h, function(err){
				image.__release();
				if(err) {
					processingColorFrame = false;
					return;
				}
				//clone into a new one to resize
				image.clone(function(err, clone){
					if(err) {
						processingColorFrame = false;
						return;
					}
					clone.batch()
						.scale(0.5)
						.toBuffer('jpg', { quality: 50 }, function(err, buffer){
							if(err) {
								processingColorFrame = false;
								return;
							}
							io.sockets.emit('colorFrame', buffer);
							processingColorFrame = false;
						});
				});
			});
		});
		kinect.openColorReader();
	});
}