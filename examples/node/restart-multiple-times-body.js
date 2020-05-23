const Kinect2 = require('kinect2');
const kinect = new Kinect2();

// execute as an async function, so we can use the await keyword
const main = async () => {
  if(kinect.open()) {
    console.log("Kinect Opened");
    kinect.openBodyReader();
    console.log("started body reader");
    await kinect.closeBodyReader();
    console.log("stopped body reader");

    kinect.openBodyReader();
    console.log("started body reader");
    await kinect.closeBodyReader();
    console.log("stopped body reader");

    // you can also us the then() keyword
    kinect.openBodyReader();
    console.log("started body reader");
    kinect.closeBodyReader().then(() => {
      console.log("stopped body reader");

      // or use an old-school callback to wait for the kinect to finish cleaning up
      kinect.closeBodyReader((err, result) => {
        console.log("stopped body reader");
        kinect.close();
      });
    });
  }
};

main();
