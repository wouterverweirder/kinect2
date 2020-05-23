const Kinect2 = require('kinect2');
const kinect = new Kinect2();

// execute as an async function, so we can use the await keyword
const main = async () => {
  if(kinect.open()) {
    console.log("Kinect Opened");
    kinect.openMultiSourceReader({ frameTypes: Kinect2.FrameType.color | Kinect2.FrameType.depth | Kinect2.FrameType.body });
    console.log("started multi source reader");
    await kinect.closeMultiSourceReader();
    console.log("stopped multi source reader");

    kinect.openMultiSourceReader({ frameTypes: Kinect2.FrameType.color | Kinect2.FrameType.depth | Kinect2.FrameType.body });
    console.log("started multi source reader");
    await kinect.closeMultiSourceReader();
    console.log("stopped multi source reader");

    // you can also us the then() keyword
    kinect.openMultiSourceReader({ frameTypes: Kinect2.FrameType.color | Kinect2.FrameType.depth | Kinect2.FrameType.body });
    console.log("started multi source reader");
    kinect.closeMultiSourceReader().then(() => {
      console.log("stopped multi source reader");

      // or use an old-school callback to wait for the kinect to finish cleaning up
      kinect.closeMultiSourceReader((err, result) => {
        console.log("stopped multi source reader");
        kinect.close();
      });
    });
  }
};

main();
