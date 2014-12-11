using System.Threading.Tasks;
using Microsoft.Kinect;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Windows;

namespace NodeKinect2
{
    public class Startup
    {

        private static NodeKinect instance;

        public async Task<object> Invoke(dynamic input)
        {
            if (instance == null)
            {
                instance = new NodeKinect(input);
            }
            return true;
        }

        public async Task<object> Open(dynamic input)
        {
            return instance.Open(input);
        }

        public async Task<object> OpenDepthReader(dynamic input)
        {
            return instance.OpenDepthReader(input);
        }

        public async Task<object> OpenBodyReader(dynamic input)
        {
            return instance.OpenBodyReader(input);
        }

        public async Task<object> OpenColorReader(dynamic input)
        {
            return instance.OpenColorReader(input);
        }

        public async Task<object> OpenInfraredReader(dynamic input)
        {
            return instance.OpenInfraredReader(input);
        }

        public async Task<object> OpenLongExposureInfraredReader(dynamic input)
        {
            return instance.OpenLongExposureInfraredReader(input);
        }

        public async Task<object> Close(dynamic input)
        {
            return instance.Close(input);
        }
    }

    public class NodeKinect
    {
        private KinectSensor kinectSensor = null;

        private FrameDescription depthFrameDescription = null;
        private DepthFrameReader depthFrameReader = null;

        /// <summary>
        /// Map depth range to byte range
        /// </summary>
        private const int MapDepthToByte = 8000 / 256;
        private byte[] depthPixels = null;
        private bool processingDepthFrame = false;

        private ColorFrameReader colorFrameReader = null;
        private FrameDescription colorFrameDescription = null;
        private byte[] colorPixels = null;
        private bool processingColorFrame = false;

        private InfraredFrameReader infraredFrameReader = null;
        private FrameDescription infraredFrameDescription = null;
        private byte[] infraredPixels = null;
        private bool processingInfraredFrame = false;

        private LongExposureInfraredFrameReader longExposureInfraredFrameReader = null;
        private FrameDescription longExposureInfraredFrameDescription = null;
        private byte[] longExposureInfraredPixels = null;
        private bool processingLongExposureInfraredFrame = false;

        /// <summary>
        /// Maximum value (as a float) that can be returned by the InfraredFrame
        /// </summary>
        private const float InfraredSourceValueMaximum = (float)ushort.MaxValue;

        /// <summary>
        /// The value by which the infrared source data will be scaled
        /// </summary>
        private const float InfraredSourceScale = 0.75f;

        /// <summary>
        /// Smallest value to display when the infrared data is normalized
        /// </summary>
        private const float InfraredOutputValueMinimum = 0.01f;

        /// <summary>
        /// Largest value to display when the infrared data is normalized
        /// </summary>
        private const float InfraredOutputValueMaximum = 1.0f;

        private CoordinateMapper coordinateMapper = null;

        private BodyFrameReader bodyFrameReader = null;
        private Body[] bodies = null;

        private Func<object, Task<object>> logCallback;
        private Func<object, Task<object>> bodyFrameCallback;
        private Func<object, Task<object>> depthFrameCallback;
        private Func<object, Task<object>> colorFrameCallback;
        private Func<object, Task<object>> infraredFrameCallback;
        private Func<object, Task<object>> longExposureInfraredFrameCallback;
        

        public NodeKinect(dynamic input)
        {
            this.logCallback = (Func<object, Task<object>>)input.logCallback;
            this.logCallback("Created NodeKinect Instance");
        }

        public async Task<object> Open(dynamic input)
        {
            this.logCallback("Open");
            this.kinectSensor = KinectSensor.GetDefault();

            if (this.kinectSensor != null)
            {
                this.coordinateMapper = this.kinectSensor.CoordinateMapper;
                this.kinectSensor.Open();
                return true;
            }
            return false;
        }

        public async Task<object> OpenDepthReader(dynamic input)
        {
            this.logCallback("OpenDepthReader");
            if (this.depthFrameReader != null)
            {
                return false;
            }
            this.depthFrameCallback = (Func<object, Task<object>>)input.depthFrameCallback;

            this.depthFrameDescription = this.kinectSensor.DepthFrameSource.FrameDescription;
            this.logCallback("depth: " + this.depthFrameDescription.Width + "x" + this.depthFrameDescription.Height);

            //depth data
            this.depthFrameReader = this.kinectSensor.DepthFrameSource.OpenReader();
            this.depthFrameReader.FrameArrived += this.DepthReader_FrameArrived;
            this.depthPixels = new byte[this.depthFrameDescription.Width * this.depthFrameDescription.Height];
            return true;
        }

        public async Task<object> OpenColorReader(dynamic input)
        {
            this.logCallback("OpenColorReader");
            if (this.colorFrameReader != null)
            {
                return false;
            }
            this.colorFrameCallback = (Func<object, Task<object>>)input.colorFrameCallback;

            this.colorFrameDescription = this.kinectSensor.ColorFrameSource.CreateFrameDescription(ColorImageFormat.Rgba);
            this.logCallback("color: " + this.colorFrameDescription.Width + "x" + this.colorFrameDescription.Height);

            this.colorFrameReader = this.kinectSensor.ColorFrameSource.OpenReader();
            this.colorFrameReader.FrameArrived += this.ColorReader_ColorFrameArrived;
            this.colorPixels = new byte[4 * this.colorFrameDescription.Width * this.colorFrameDescription.Height];

            return true;
        }

        public async Task<object> OpenInfraredReader(dynamic input)
        {
            this.logCallback("OpenInfraredReader");
            if (this.infraredFrameReader != null)
            {
                return false;
            }
            this.infraredFrameCallback = (Func<object, Task<object>>)input.infraredFrameCallback;

            this.infraredFrameDescription = this.kinectSensor.InfraredFrameSource.FrameDescription;
            this.logCallback("infrared: " + this.infraredFrameDescription.Width + "x" + this.infraredFrameDescription.Height);

            //depth data
            this.infraredFrameReader = this.kinectSensor.InfraredFrameSource.OpenReader();
            this.infraredFrameReader.FrameArrived += this.InfraredReader_FrameArrived;
            this.infraredPixels = new byte[this.infraredFrameDescription.Width * this.infraredFrameDescription.Height];
            return true;
        }

        public async Task<object> OpenLongExposureInfraredReader(dynamic input)
        {
            this.logCallback("OpenLongExposureInfraredReader");
            if (this.longExposureInfraredFrameReader != null)
            {
                return false;
            }
            this.longExposureInfraredFrameCallback = (Func<object, Task<object>>)input.longExposureInfraredFrameCallback;

            this.longExposureInfraredFrameDescription = this.kinectSensor.LongExposureInfraredFrameSource.FrameDescription;
            this.logCallback("longExposureInfrared: " + this.longExposureInfraredFrameDescription.Width + "x" + this.longExposureInfraredFrameDescription.Height);

            //depth data
            this.longExposureInfraredFrameReader = this.kinectSensor.LongExposureInfraredFrameSource.OpenReader();
            this.longExposureInfraredFrameReader.FrameArrived += this.LongExposureInfraredReader_FrameArrived;
            this.longExposureInfraredPixels = new byte[this.longExposureInfraredFrameDescription.Width * this.longExposureInfraredFrameDescription.Height];
            return true;
        }

        public async Task<object> OpenBodyReader(dynamic input)
        {
            this.logCallback("OpenBodyReader");
            if (this.bodyFrameReader != null)
            {
                return false;
            }
            this.bodyFrameCallback = (Func<object, Task<object>>)input.bodyFrameCallback;
            this.bodies = new Body[this.kinectSensor.BodyFrameSource.BodyCount];
            this.bodyFrameReader = this.kinectSensor.BodyFrameSource.OpenReader();
            this.bodyFrameReader.FrameArrived += this.BodyReader_FrameArrived;
            return true;
        }

        public async Task<object> Close(object input)
        {
            if (this.depthFrameReader != null)
            {
                this.depthFrameReader.Dispose();
                this.depthFrameReader = null;
            }

            if (this.colorFrameReader != null)
            {
                this.colorFrameReader.Dispose();
                this.colorFrameReader = null;
            }

            if (this.infraredFrameReader != null)
            {
                this.infraredFrameReader.Dispose();
                this.infraredFrameReader = null;
            }

            if (this.longExposureInfraredFrameReader != null)
            {
                this.longExposureInfraredFrameReader.Dispose();
                this.longExposureInfraredFrameReader = null;
            }

            if (this.bodyFrameReader != null)
            {
                this.bodyFrameReader.Dispose();
                this.bodyFrameReader = null;
            }

            if (this.kinectSensor != null)
            {
                this.kinectSensor.Close();
                this.kinectSensor = null;
            }
            return true;
        }

        private void DepthReader_FrameArrived(object sender, DepthFrameArrivedEventArgs e)
        {
            if (this.processingDepthFrame)
            {
                return;
            }
            this.processingDepthFrame = true;
            bool depthFrameProcessed = false;

            using (DepthFrame depthFrame = e.FrameReference.AcquireFrame())
            {
                if (depthFrame != null)
                {
                    // the fastest way to process the body index data is to directly access 
                    // the underlying buffer
                    using (Microsoft.Kinect.KinectBuffer depthBuffer = depthFrame.LockImageBuffer())
                    {
                        // verify data and write the color data to the display bitmap
                        if (((this.depthFrameDescription.Width * this.depthFrameDescription.Height) == (depthBuffer.Size / this.depthFrameDescription.BytesPerPixel)))
                        {
                            // Note: In order to see the full range of depth (including the less reliable far field depth)
                            // we are setting maxDepth to the extreme potential depth threshold
                            //ushort maxDepth = ushort.MaxValue;

                            // If you wish to filter by reliable depth distance, uncomment the following line:
                            ushort maxDepth = depthFrame.DepthMaxReliableDistance;

                            this.ProcessDepthFrameData(depthBuffer.UnderlyingBuffer, depthBuffer.Size, depthFrame.DepthMinReliableDistance, maxDepth);

                            depthFrameProcessed = true;
                        }
                    }
                }
            }

            if (depthFrameProcessed)
            {
                this.depthFrameCallback(this.depthPixels);
                //this.RenderDepthPixels();
            }
            this.processingDepthFrame = false;
        }

        /// <summary>
        /// Directly accesses the underlying image buffer of the DepthFrame to 
        /// create a displayable bitmap.
        /// This function requires the /unsafe compiler option as we make use of direct
        /// access to the native memory pointed to by the depthFrameData pointer.
        /// </summary>
        /// <param name="depthFrameData">Pointer to the DepthFrame image data</param>
        /// <param name="depthFrameDataSize">Size of the DepthFrame image data</param>
        /// <param name="minDepth">The minimum reliable depth value for the frame</param>
        /// <param name="maxDepth">The maximum reliable depth value for the frame</param>
        private unsafe void ProcessDepthFrameData(IntPtr depthFrameData, uint depthFrameDataSize, ushort minDepth, ushort maxDepth)
        {
            // depth frame data is a 16 bit value
            ushort* frameData = (ushort*)depthFrameData;

            // convert depth to a visual representation
            for (int i = 0; i < (int)(depthFrameDataSize / this.depthFrameDescription.BytesPerPixel); ++i)
            {
                // Get the depth for this pixel
                ushort depth = frameData[i];

                // To convert to a byte, we're mapping the depth value to the byte range.
                // Values outside the reliable depth range are mapped to 0 (black).
                this.depthPixels[i] = (byte)(depth >= minDepth && depth <= maxDepth ? (depth / MapDepthToByte) : 0);
            }
        }

        private void ColorReader_ColorFrameArrived(object sender, ColorFrameArrivedEventArgs e)
        {
            if (this.processingColorFrame)
            {
                return;
            }
            this.processingColorFrame = true;
            bool colorFrameProcessed = false;
            // ColorFrame is IDisposable
            using (ColorFrame colorFrame = e.FrameReference.AcquireFrame())
            {
                if (colorFrame != null)
                {
                    FrameDescription colorFrameDescription = colorFrame.FrameDescription;

                    using (KinectBuffer colorBuffer = colorFrame.LockRawImageBuffer())
                    {
                        colorFrame.CopyConvertedFrameDataToArray(this.colorPixels, ColorImageFormat.Rgba);
                        colorFrameProcessed = true;
                    }
                }
            }
            if (colorFrameProcessed)
            {
                this.colorFrameCallback(this.colorPixels);
            }
            this.processingColorFrame = false;
        }

        private void InfraredReader_FrameArrived(object sender, InfraredFrameArrivedEventArgs e)
        {
            if (this.processingInfraredFrame)
            {
                return;
            }
            this.processingInfraredFrame = true;
            bool infraredFrameProcessed = false;

            using (InfraredFrame infraredFrame = e.FrameReference.AcquireFrame())
            {
                if (infraredFrame != null)
                {
                    // the fastest way to process the body index data is to directly access 
                    // the underlying buffer
                    using (Microsoft.Kinect.KinectBuffer infraredBuffer = infraredFrame.LockImageBuffer())
                    {
                        // verify data and write the color data to the display bitmap
                        if (((this.infraredFrameDescription.Width * this.infraredFrameDescription.Height) == (infraredBuffer.Size / this.infraredFrameDescription.BytesPerPixel)))
                        {
                            this.ProcessInfraredFrameData(infraredBuffer.UnderlyingBuffer, infraredBuffer.Size, this.infraredFrameDescription.BytesPerPixel);
                            infraredFrameProcessed = true;
                        }
                    }
                }
            }

            if (infraredFrameProcessed)
            {
                this.infraredFrameCallback(this.infraredPixels);
            }
            this.processingInfraredFrame = false;
        }

        private void LongExposureInfraredReader_FrameArrived(object sender, LongExposureInfraredFrameArrivedEventArgs e)
        {
            if (this.processingLongExposureInfraredFrame)
            {
                return;
            }
            this.processingLongExposureInfraredFrame = true;
            bool longExposureInfraredFrameProcessed = false;

            using (LongExposureInfraredFrame longExposureInfraredFrame = e.FrameReference.AcquireFrame())
            {
                if (longExposureInfraredFrame != null)
                {
                    using (Microsoft.Kinect.KinectBuffer longExposureInfraredBuffer = longExposureInfraredFrame.LockImageBuffer())
                    {
                        // verify data and write the color data to the display bitmap
                        if (((this.longExposureInfraredFrameDescription.Width * this.longExposureInfraredFrameDescription.Height) == (longExposureInfraredBuffer.Size / this.longExposureInfraredFrameDescription.BytesPerPixel)))
                        {
                            this.ProcessLongExposureInfraredFrameData(longExposureInfraredBuffer.UnderlyingBuffer, longExposureInfraredBuffer.Size, this.longExposureInfraredFrameDescription.BytesPerPixel);
                            longExposureInfraredFrameProcessed = true;
                        }
                    }
                }
            }

            if (longExposureInfraredFrameProcessed)
            {
                this.longExposureInfraredFrameCallback(this.longExposureInfraredPixels);
            }
            this.processingLongExposureInfraredFrame = false;
        }

        private unsafe void ProcessInfraredFrameData(IntPtr frameData, uint frameDataSize, uint bytesPerPixel)
        {
            ushort* lframeData = (ushort*)frameData;
            int len = (int)(frameDataSize / bytesPerPixel);
            for (int i = 0; i < len; ++i)
            {
                this.infraredPixels[i] = (byte) (0xff * Math.Min(InfraredOutputValueMaximum, (((float)lframeData[i] / InfraredSourceValueMaximum * InfraredSourceScale) * (1.0f - InfraredOutputValueMinimum)) + InfraredOutputValueMinimum));
            }
        }

        private unsafe void ProcessLongExposureInfraredFrameData(IntPtr frameData, uint frameDataSize, uint bytesPerPixel)
        {
            ushort* lframeData = (ushort*)frameData;
            int len = (int)(frameDataSize / bytesPerPixel);
            for (int i = 0; i < len; ++i)
            {
                this.longExposureInfraredPixels[i] = (byte)(0xff * Math.Min(InfraredOutputValueMaximum, (((float)lframeData[i] / InfraredSourceValueMaximum * InfraredSourceScale) * (1.0f - InfraredOutputValueMinimum)) + InfraredOutputValueMinimum));
            }
        }

        private void BodyReader_FrameArrived(object sender, BodyFrameArrivedEventArgs e)
        {
            bool dataReceived = false;

            using (BodyFrame bodyFrame = e.FrameReference.AcquireFrame())
            {
                if (bodyFrame != null)
                {
                    if (this.bodies == null)
                    {
                        this.bodies = new Body[bodyFrame.BodyCount];
                    }

                    bodyFrame.GetAndRefreshBodyData(this.bodies);
                    dataReceived = true;
                }
            }

            if (dataReceived)
            {
                var jsBodies = new ArrayList();
                foreach (Body body in this.bodies)
                {
                    if (body.IsTracked)
                    {
                        IReadOnlyDictionary<JointType, Joint> joints = body.Joints;

                        // convert the joint points to depth (display) space
                        IDictionary<String, Object> jsJoints = new Dictionary<String, Object>();
                        foreach (JointType jointType in joints.Keys)
                        {
                            DepthSpacePoint depthSpacePoint = this.coordinateMapper.MapCameraPointToDepthSpace(joints[jointType].Position);
                            jsJoints[jointType.ToString()] = new
                            {
                                x = depthSpacePoint.X,
                                y = depthSpacePoint.Y
                            };
                        }
                        var jsBody = new
                        {
                            trackingId = body.TrackingId,
                            handLeftState = body.HandLeftState,
                            handRightState = body.HandRightState,
                            joints = jsJoints
                        };
                        jsBodies.Add(jsBody);
                    }
                }
                this.bodyFrameCallback(jsBodies);
            }
        }
    }
}