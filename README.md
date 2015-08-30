# Node-Kinect2

Nodejs library to access the kinect 2 data from the official MS SDK on Windows.

![Screenshot](https://raw.githubusercontent.com/wouterverweirder/node-kinect2/master/node-kinect2-skeleton.png)

## Installation

You will need to install [the official Kinect 2 SDK](https://www.microsoft.com/en-us/download/details.aspx?id=44561) before you can use this module.

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

After this, npm install should work.

``` bash
$ npm install kinect2
```

An alternative option is to npm install pangyp, and change your npm config (change to the path on your computer):

``` bash
$ npm config set node_gyp C:\Users\Wouter\.nvmw\iojs\v2.3.1\node_modules\pangyp\bin\node-gyp.js
```

### electron

If you want to use this module inside an electron application, you will need to [build this module for electron usage](https://github.com/atom/electron/blob/master/docs/tutorial/using-native-node-modules.md).

``` bash
$ cd node_modules/kinect2
$ node-gyp rebuild --target=0.30.2 --arch=x64 --dist-url=https://atom.io/download/atom-shell
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
