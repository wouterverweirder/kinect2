# Node-Kinect2

Nodejs library to access the kinect 2 data from the official MS SDK

Still very early / experimental.

![Screenshot](../../blob/master/node-kinect2-skeleton.png?raw=true)

```
var Kinect2 = require('kinect2');

var kinect = new Kinect2();

if(kinect.open()) {
	console.log("Kinect Opened");
	//listen for body frames
	kinect.on('bodyFrame', function(bodies){
		for(var i = 0;  i < bodies.length; i++) {
			console.log(bodies[i]);
		}
	});

	//request body frames
	kinect.openBodyReader();

	//close the kinect after 5 seconds
	setTimeout(function(){
		kinect.close();
		console.log("Kinect Closed");
	}, 5000);
}
```