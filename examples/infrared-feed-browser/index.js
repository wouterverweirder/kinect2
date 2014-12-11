var Kinect2 = require('../../kinect2'),
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

	kinect.on('infraredFrame', function(data){
		io.sockets.emit('infraredFrame', data);
	});

	kinect.openInfraredReader();
}