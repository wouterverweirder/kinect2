# Node-Kinect2

Nodejs library to access the kinect 2 data from the official MS SDK

![Screenshot](https://raw.githubusercontent.com/wouterverweirder/node-kinect2/master/node-kinect2-skeleton.png)

## Installation

### node.js

You need at least node version 0.11.13 to use this module. Older versions do not work. Just use npm install:

``` bash
$ npm install kinect2
```

### io.js

If you are using io.js, you will need to use pangyp to build this module. Install pangyp globally with npm:

``` bash
$ npm install -g pangyp
```

Npm install this module, you will get some errors on node-gyp because node-gyp does not know of the io.js download locations.

``` bash
$ npm install kinect2
```

Build this module by navigating to the directory of this module and running pangyp rebuild:

``` bash
$ cd node_modules/kinect2
$ pangyp rebuild
```

## Usage

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
