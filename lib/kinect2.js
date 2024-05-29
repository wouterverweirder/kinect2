const { EventEmitter } = require('events');

const kinect = require('../index.js');

class Kinect2 extends EventEmitter {
  constructor() {
    super({});
    this.colorFrameCallback = this.colorFrameCallback.bind(this);
    this.infraredFrameCallback = this.infraredFrameCallback.bind(this);
    this.longExposureInfraredFrameCallback = this.longExposureInfraredFrameCallback.bind(this);
    this.depthFrameCallback = this.depthFrameCallback.bind(this);
    this.rawDepthFrameCallback = this.rawDepthFrameCallback.bind(this);
    this.bodyFrameCallback = this.bodyFrameCallback.bind(this);
    this.multiSourceFrameCallback = this.multiSourceFrameCallback.bind(this);
  }

  open() { return kinect.open() }
  
  close(cb) {
    return new Promise((resolve, reject) => {
      Promise.resolve()
      .then(() => this.closeColorReader())
      .then(() => this.closeInfraredReader())
      .then(() => this.closeLongExposureInfraredReader())
      .then(() => this.closeDepthReader())
      .then(() => this.closeRawDepthReader())
      .then(() => this.closeBodyReader())
      .then(() => this.closeMultiSourceReader())
      .then(() => {
        kinect.close();
        if (cb) {
          cb(false, true);
        } else {
          resolve(true);
        }
      });
    });
  }

  openColorReader() {
    return kinect.openColorReader(this.colorFrameCallback);
  };

  colorFrameCallback(data) {
    this.emit('colorFrame', data);
  }
  
  closeColorReader(cb) {
    return new Promise((resolve, reject) => {
      kinect.closeColorReader((err, result) => {
        if (err) {
          reject (err);
        } else {
          resolve(result);
          if (cb) {
            cb(err, result);
          }
        }
      });
    });
  };

  openInfraredReader() {
    return kinect.openInfraredReader(this.infraredFrameCallback);
  };

  infraredFrameCallback(data) {
    this.emit('infraredFrame', data);
  }
  
  closeInfraredReader(cb) {
    return new Promise((resolve, reject) => {
      kinect.closeInfraredReader((err, result) => {
        if (err) {
          reject (err);
        } else {
          resolve(result);
          if (cb) {
            cb(err, result);
          }
        }
      });
    });
  };
  
  openLongExposureInfraredReader() {
    return kinect.openLongExposureInfraredReader(this.longExposureInfraredFrameCallback);
  };

  longExposureInfraredFrameCallback(data) {
    this.emit('longExposureInfraredFrame', data);
  }
  
  closeLongExposureInfraredReader(cb) {
    return new Promise((resolve, reject) => {
      kinect.closeLongExposureInfraredReader((err, result) => {
        if (err) {
          reject (err);
        } else {
          resolve(result);
          if (cb) {
            cb(err, result);
          }
        }
      });
    });
  };

  openDepthReader() {
    return kinect.openDepthReader(this.depthFrameCallback);
  };

  depthFrameCallback(data) {
    this.emit('depthFrame', data);
  }
  
  closeDepthReader(cb) {
    return new Promise((resolve, reject) => {
      kinect.closeDepthReader((err, result) => {
        if (err) {
          reject (err);
        } else {
          resolve(result);
          if (cb) {
            cb(err, result);
          }
        }
      });
    });
  };

  openRawDepthReader() {
    return kinect.openRawDepthReader(this.rawDepthFrameCallback);
  };

  rawDepthFrameCallback(data) {
    this.emit('rawDepthFrame', data);
  }
  
  closeRawDepthReader(cb) {
    return new Promise((resolve, reject) => {
      kinect.closeRawDepthReader((err, result) => {
        if (err) {
          reject (err);
        } else {
          resolve(result);
          if (cb) {
            cb(err, result);
          }
        }
      });
    });
  };

  openBodyReader() {
    return kinect.openBodyReader(this.bodyFrameCallback);
  };

  bodyFrameCallback(data) {
    this.emit('bodyFrame', data);
  }
  
  closeBodyReader(cb) {
    return new Promise((resolve, reject) => {
      kinect.closeBodyReader((err, result) => {
        if (err) {
          reject (err);
        } else {
          resolve(result);
          if (cb) {
            cb(err, result);
          }
        }
      });
    });
  };

  /**
   * Possible options:
   *
   * frameTypes: a binary combination of Kinect2.FrameTypes. (ex: frameTypes: Kinect2.FrameType.body | Kinect2.FrameType.color)
   * includeJointFloorData: boolean, calculate & include projected positions of joints on floor.
   */
  openMultiSourceReader(options) {
    options.callback = this.multiSourceFrameCallback;
    return kinect.openMultiSourceReader(options);
  };

  closeMultiSourceReader(cb) {
    return new Promise((resolve, reject) => {
      kinect.closeMultiSourceReader((err, result) => {
        if (err) {
          reject (err);
        } else {
          resolve(result);
          if (cb) {
            cb(err, result);
          }
        }
      });
    });
  };

  multiSourceFrameCallback(frame) {
    this.emit('multiSourceFrame', frame);
  };

  /**
 * Specify which body indices you want to have pixel data from
 * Used when running the multisource reader to get green screen effects
 * provide an array with body indices (0 -> 5)
 */
  trackPixelsForBodyIndices(bodyIndices) {
    return kinect.trackPixelsForBodyIndices(bodyIndices);
  };
}

Kinect2.FrameType = {
  none                            : 0,
  color                            : 0x1,
  infrared                        : 0x2,
  longExposureInfrared            : 0x4,
  depth                            : 0x8,
  bodyIndex                        : 0x10, //Not Implemented Yet
  body                            : 0x20,
  audio                            : 0x40, //Not Implemented Yet
  bodyIndexColor                  : 0x80,
  bodyIndexDepth                  : 0x10, //Same as BodyIndex
  bodyIndexInfrared                : 0x100, //Not Implemented Yet
  bodyIndexLongExposureInfrared    : 0x200, //Not Implemented Yet
  rawDepth                        : 0x400,
  depthColor                      : 0x800
};

Kinect2.JointType = {
  spineBase       : 0,
  spineMid        : 1,
  neck            : 2,
  head            : 3,
  shoulderLeft    : 4,
  elbowLeft       : 5,
  wristLeft       : 6,
  handLeft        : 7,
  shoulderRight   : 8,
  elbowRight      : 9,
  wristRight      : 10,
  handRight       : 11,
  hipLeft         : 12,
  kneeLeft        : 13,
  ankleLeft       : 14,
  footLeft        : 15,
  hipRight        : 16,
  kneeRight       : 17,
  ankleRight      : 18,
  footRight       : 19,
  spineShoulder   : 20,
  handTipLeft     : 21,
  thumbLeft       : 22,
  handTipRight    : 23,
  thumbRight      : 24
};

Kinect2.HandState = {
  unknown      : 0,
  notTracked   : 1,
  open         : 2,
  closed       : 3,
  lasso        : 4
};

Kinect2.TrackingState = {
  notTracked   : 0,
  inferred     : 1,
  tracked      : 2
};

module.exports = Kinect2;