# Kinect2 Library for Node / Electron

[![npm](https://img.shields.io/node/v/kinect2.svg)](https://nodejs.org/en/)
[![npm](https://img.shields.io/npm/v/kinect2.svg)](https://npmjs.org/package/kinect2)
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=NUZP3U3QZEQV2&currency_code=EUR&source=url)

This library enables you to use the Kinect v2 in your nodejs or electron apps.

![screenshot of multi stream demo](examples/screenshots/multi-source-reader.png)

Features:

- get rgb camera feed
- get depth feed
- get ir feed
- point cloud (greyscale and colored)
- get skeleton joints (2d and 3d)
- user masking

Check out [my kinect-azure library](https://github.com/wouterverweirder/kinect-azure) for the Azure Kinect sensor.

## Installation

You will need to install [the official Kinect 2 SDK](https://www.microsoft.com/en-us/download/details.aspx?id=44561) before you can use this module.

Just npm install like you would do with any regular module. 

```
$ npm install kinect2
```

## Examples

There are nodejs and electron examples in the examples/ folder of this repo. To run them, execute npm install and npm start:

```
$ cd examples/electron
$ npm install
$ npm start
```

The electron examples have the javascript code inside the html files. You can find these html files in [examples/electron/renderer/demos](examples/electron/renderer/demos).

## Donate

Like this library? Always welcome to buy me a beer 🍺

[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=NUZP3U3QZEQV2&currency_code=EUR&source=url)
