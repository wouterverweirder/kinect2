var Kinect2 = require('../kinect2');

var kinect2 = new Kinect2();

if(kinect2.open()) {
	console.log("Kinect Opened");
	//listen for body frames
	kinect2.on('bodyFrame', function(bodies){
		for(var i = 0;  i < bodies.length; i++) {
			console.log(bodies[i]);
		}
	});
	//close the kinect after 5 seconds
	setTimeout(function(){
		kinect2.close();
		console.log("Kinect Closed");
	}, 5000);
}