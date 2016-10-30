# Node-Kinect2

Nodejs library to access the kinect 2 data from the official MS SDK on Windows.

![Screenshot](https://raw.githubusercontent.com/wouterverweirder/node-kinect2/master/node-kinect2-skeleton.png)

## Installation

You will need to install [the official Kinect 2 SDK](https://www.microsoft.com/en-us/download/details.aspx?id=44561) before you can use this module.

### node.js

You need at least node version 0.12 to use this module. Older versions do not work. Just use npm install:

``` bash
npm install kinect2
```

### electron

If you want to use this module inside an electron application, you will need to [build this module for electron usage](https://github.com/atom/electron/blob/master/docs/tutorial/using-native-node-modules.md). I've provided a build script which does just that.

You will need to have node-gyp & it's dependencies installed (https://github.com/nodejs/node-gyp) before you can continue.

``` bash
# cd into the directory of kinect2
cd node_modules\kinect2
# run my build script to create a native binary for electron
npm run build:electron
```

## Usage

```
var Kinect2 = require('kinect2');

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
		kinect.close();
		console.log("Kinect Closed");
	}, 5000);
}
```
