var Kinect2 = require('../../lib/kinect2'),
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

	app.use(express.static(__dirname + '/public'));

	var compressing = false;
	kinect.on('depthFrame', function(data){
		//compress the depth data using zlib
		if(!compressing) {
			compressing = true;
			zlib.deflate(data, function(err, result){
				if(!err) {
					io.sockets.emit('depthFrame', result.toString('base64'));
				}
				compressing = false;
			});
		}
	});

	kinect.openDepthReader();
}
