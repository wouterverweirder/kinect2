const Kinect2 = require('.');
const kinect = new Kinect2();

if(kinect.open()) {
  console.log("Kinect Opened");
  kinect.openMultiSourceReader({ frameTypes: Kinect2.FrameType.color | Kinect2.FrameType.depth | Kinect2.FrameType.body });
  console.log("started multi source reader");

  kinect.on('multiSourceFrame', (frame) => {
    const hasColorInfo = (frame.color !== null && frame.color.buffer !== null);
    const hasDepthInfo = (frame.depth !== null && frame.depth.buffer !== null);
    const hasBodyInfo = (frame.body !== null);
    const hasAllInfo = hasColorInfo && hasDepthInfo && hasBodyInfo;

    console.log(`color: ${hasColorInfo}, depth: ${hasDepthInfo}, body: ${hasBodyInfo}`);

    if (hasAllInfo) {
      console.log("close multi source reader");
      kinect.closeMultiSourceReader(() => {
        console.log("stopped multi source reader");
        kinect.close();
      });
    }
  });
  
}
