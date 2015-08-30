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

	var compressing = false;
	kinect.on('infraredFrame', function(data){
		//compress the infrared data using zlib
		if(!compressing) {
			compressing = true;
			zlib.deflate(data, function(err, result){
				if(!err) {
					var buffer = result.toString('base64');
					io.sockets.sockets.forEach(function(socket){
						socket.volatile.emit('infraredFrame', buffer);
					});
				}
				compressing = false;
			});
		}
	});

	kinect.openInfraredReader();
}
