const Kinect2 = require('kinect2');
const kinect = new Kinect2();

// execute as an async function, so we can use the await keyword
const main = async () => {
  if(kinect.open()) {
    console.log("Kinect Opened");
    kinect.openInfraredReader();
    console.log("started infrared reader");
    await kinect.closeInfraredReader();
    console.log("stopped infrared reader");

    kinect.openInfraredReader();
    console.log("started infrared reader");
    await kinect.closeInfraredReader();
    console.log("stopped infrared reader");

    // you can also us the then() keyword
    kinect.openInfraredReader();
    console.log("started infrared reader");
    kinect.closeInfraredReader().then(() => {
      console.log("stopped infrared reader");

      // or use an old-school callback to wait for the kinect to finish cleaning up
      kinect.closeInfraredReader((err, result) => {
        console.log("stopped infrared reader");
        kinect.close();
      });
    });
  }
};

main();
