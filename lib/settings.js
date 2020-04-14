const path = require('path');

module.exports = () => {
  const KINECT_SENSOR_SDK_URL = 'https://www.nuget.org/api/v2/package/Microsoft.Kinect/2.0.1410.19000';
  
  const TARGET_SDK_DIR = path.resolve(__dirname, '../sdk');
  const TARGET_KINECT_SENSOR_SDK_ZIP = path.resolve(TARGET_SDK_DIR, 'sensor-sdk-2.0.1410.19000.zip');

  const TARGET_KINECT_SENSOR_SDK_DIR = path.resolve(TARGET_SDK_DIR, 'sensor-sdk-2.0.1410.19000');

  const appRootDlls = {
  };

  return {
    KINECT_SENSOR_SDK_URL,
    TARGET_SDK_DIR,
    TARGET_KINECT_SENSOR_SDK_ZIP,
    TARGET_KINECT_SENSOR_SDK_DIR,
    appRootDlls
  }
};