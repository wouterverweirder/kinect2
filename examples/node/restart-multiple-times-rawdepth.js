const Kinect2 = require('kinect2');
const kinect = new Kinect2();

// execute as an async function, so we can use the await keyword
const main = async () => {
  if(kinect.open()) {
    console.log("Kinect Opened");
    kinect.openRawDepthReader();
    console.log("started raw depth reader");
    await kinect.closeRawDepthReader();
    console.log("stopped raw depth reader");

    kinect.openRawDepthReader();
    console.log("started raw depth reader");
    await kinect.closeRawDepthReader();
    console.log("stopped raw depth reader");

    // you can also us the then() keyword
    kinect.openRawDepthReader();
    console.log("started raw depth reader");
    kinect.closeRawDepthReader().then(() => {
      console.log("stopped raw depth reader");

      // or use an old-school callback to wait for the kinect to finish cleaning up
      kinect.closeRawDepthReader((err, result) => {
        console.log("stopped raw depth reader");
        kinect.close();
      });
    });
  }
};

main();
