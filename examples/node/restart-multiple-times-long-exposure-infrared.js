const Kinect2 = require('kinect2');
const kinect = new Kinect2();

// execute as an async function, so we can use the await keyword
const main = async () => {
  if(kinect.open()) {
    console.log("Kinect Opened");
    kinect.openLongExposureInfraredReader();
    console.log("started long exposure infrared reader");
    await kinect.closeLongExposureInfraredReader();
    console.log("stopped long exposure infrared reader");

    kinect.openLongExposureInfraredReader();
    console.log("started long exposure infrared reader");
    await kinect.closeLongExposureInfraredReader();
    console.log("stopped long exposure infrared reader");

    // you can also us the then() keyword
    kinect.openLongExposureInfraredReader();
    console.log("started long exposure infrared reader");
    kinect.closeLongExposureInfraredReader().then(() => {
      console.log("stopped long exposure infrared reader");

      // or use an old-school callback to wait for the kinect to finish cleaning up
      kinect.closeLongExposureInfraredReader((err, result) => {
        console.log("stopped long exposure infrared reader");
        kinect.close();
      });
    });
  }
};

main();
