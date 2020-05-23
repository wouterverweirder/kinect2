const { ipcRenderer } = require('electron');
ipcRenderer.on('close-kinect', (event, message) => {
  if (window.kinect) {
    // properly close the kinect and send a message back to the renderer
    window.kinect.close().then(() => {
      event.sender.send('kinect closed');
    });
  } else {
    event.sender.send('kinect closed');
  }
});