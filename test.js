const Kinect2 = require('.');
const kinect = new Kinect2();

if(kinect.open()) {
  console.log("Kinect Opened");
  // kinect.on('colorFrame', (frame) => {
  //   console.log(frame);
  // });
  // kinect.openColorReader();
  kinect.openMultiSourceReader({ frameTypes: Kinect2.FrameType.color | Kinect2.FrameType.longExposureInfrared });
  console.log("started multi source reader");

  kinect.on('multiSourceFrame', (frame) => {
    const hasColorInfo = (frame.color !== undefined && frame.color.buffer !== undefined);
    const hasDepthInfo = (frame.depth !== undefined && frame.depth.buffer !== undefined);
    const hasInfraredInfo = (frame.infrared !== undefined && frame.infrared.buffer !== undefined);
    const hasLongExposureInfraredInfo = (frame.longExposureInfrared !== undefined && frame.longExposureInfrared.buffer !== undefined);
    const hasBodyInfo = (frame.body !== undefined);
    const hasAllInfo = hasColorInfo && hasDepthInfo && hasBodyInfo;

    console.log(`color: ${hasColorInfo}, depth: ${hasDepthInfo}, body: ${hasBodyInfo}, infrared: ${hasInfraredInfo}, long exposure infrared: ${hasLongExposureInfraredInfo}`);

    // if (hasAllInfo) {
    //   console.log("close multi source reader");
    //   kinect.closeMultiSourceReader(() => {
    //     console.log("stopped multi source reader");
    //     kinect.close();
    //   });
    // }
  });
  
}
