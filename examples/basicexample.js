var Kinect2 = require('../lib/kinect2');

var kinect = new Kinect2();

if(kinect.open()) {
	console.log("Kinect Opened");
	//listen for body frames

	kinect.on('bodyFrame', function(bodyFrame){
		for(var i = 0;  i < bodyFrame.bodies.length; i++) {
			if(bodyFrame.bodies[i].tracked) {
				console.log(bodyFrame.bodies[i]);
			}
		}
	});

	//request body frames
	kinect.openBodyReader();

	//close the kinect after 5 seconds
	setTimeout(function(){
		kinect.removeAllListeners('bodyFrame');
		kinect.close();
		console.log("Kinect Closed");
	}, 5000);
}
