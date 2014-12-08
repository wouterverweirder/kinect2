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

        private CoordinateMapper coordinateMapper = null;

        private BodyFrameReader bodyFrameReader = null;
        private Body[] bodies = null;

        private Func<object, Task<object>> logCallback;
        private Func<object, Task<object>> bodyFrameCallback;
        private Func<object, Task<object>> depthFrameCallback;
        private Func<object, Task<object>> colorFrameCallback;
        

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