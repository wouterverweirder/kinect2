const Kinect2 = require('kinect2');
const kinect = new Kinect2();

// execute as an async function, so we can use the await keyword
const main = async () => {
  if(kinect.open()) {
    console.log("Kinect Opened");
    kinect.openDepthReader();
    console.log("started depth reader");
    await kinect.closeDepthReader();
    console.log("stopped depth reader");

    kinect.openDepthReader();
    console.log("started depth reader");
    await kinect.closeDepthReader();
    console.log("stopped depth reader");

    // you can also us the then() keyword
    kinect.openDepthReader();
    console.log("started depth reader");
    kinect.closeDepthReader().then(() => {
      console.log("stopped depth reader");

      // or use an old-school callback to wait for the kinect to finish cleaning up
      kinect.closeDepthReader((err, result) => {
        console.log("stopped depth reader");
        kinect.close();
      });
    });
  }
};

main();
