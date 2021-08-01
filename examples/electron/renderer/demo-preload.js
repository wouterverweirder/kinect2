const { ipcRenderer } = require('electron');

ipcRenderer.on('close-kinect', (event, message) => {
  if (window.kinect) {
    // properly close the kinect and send a message back to the renderer
    console.log('close the kinect');
    window.kinect.close().then(() => {
      console.log('kinect closed');
    }).catch(e => {
      console.error(e);
    }).then(() => {
      event.sender.send('kinect closed');
    });
  } else {
    console.log("no kinect exposed");
    event.sender.send('kinect closed');
  }
});