const Kinect2 = require('kinect2');
const kinect = new Kinect2();

// execute as an async function, so we can use the await keyword
const main = async () => {
  if(kinect.open()) {
    console.log("Kinect Opened");
    kinect.openColorReader();
    console.log("started color reader");
    await kinect.closeColorReader();
    console.log("stopped color reader");

    kinect.openColorReader();
    console.log("started color reader");
    await kinect.closeColorReader();
    console.log("stopped color reader");

    // you can also us the then() keyword
    kinect.openColorReader();
    console.log("started color reader");
    kinect.closeColorReader().then(() => {
      console.log("stopped color reader");

      // or use an old-school callback to wait for the kinect to finish cleaning up
      kinect.closeColorReader((err, result) => {
        console.log("stopped color reader");
        kinect.close();
      });
    });
  }
};

main();
