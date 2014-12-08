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

	var origLength = 4 * 1920 * 1080;
	var resizedLength = 4 * 192 * 108;
	//we will send a smaller image (1 / 10th size) over the network
	var resizedBuffer = new Buffer(resizedLength);
	kinect.on('colorFrame', function(data){
		//data is HD bitmap image, which is a bit too heavy to handle in our browser
		//only send every 10 pixels over to the browser
		var y2 = 0;
		for(var y = 0; y < 1080; y+=10) {
			y2++;
			var x2 = 0;
			for(var x = 0; x < 1920; x+=10) {
				var i = 4 * (y * 1920 + x);
				var j = 4 * (y2 * 192 + x2);
				resizedBuffer[j] = data[i];
				resizedBuffer[j+1] = data[i+1];
				resizedBuffer[j+2] = data[i+2];
				resizedBuffer[j+3] = data[i+3];
				x2++;
			}
		}
		io.sockets.emit('colorFrame', resizedBuffer);
	});

	kinect.openColorReader();
}