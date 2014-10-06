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
        /// <summary>
        /// Active Kinect sensor
        /// </summary>
        private KinectSensor kinectSensor = null;

        /// <summary>
        /// Coordinate mapper to map one type of point to another
        /// </summary>
        private CoordinateMapper coordinateMapper = null;

        /// <summary>
        /// Reader for body frames
        /// </summary>
        private BodyFrameReader bodyFrameReader = null;

        /// <summary>
        /// Array for the bodies
        /// </summary>
        private Body[] bodies = null;

        /// <summary>
        /// Width of display (depth space)
        /// </summary>
        private int displayWidth;

        /// <summary>
        /// Height of display (depth space)
        /// </summary>
        private int displayHeight;

        private Func<object, Task<object>> bodyFrameCallback;

        public async Task<object> Open(dynamic input)
        {
            this.kinectSensor = KinectSensor.GetDefault();
            this.bodyFrameCallback = (Func<object, Task<object>>)input.bodyFrameCallback;
            if (this.kinectSensor != null)
            {
                this.coordinateMapper = this.kinectSensor.CoordinateMapper;
                this.kinectSensor.Open();
                FrameDescription frameDescription = this.kinectSensor.DepthFrameSource.FrameDescription;
                this.displayWidth = frameDescription.Width;
                this.displayHeight = frameDescription.Height;
                this.bodies = new Body[this.kinectSensor.BodyFrameSource.BodyCount];
                this.bodyFrameReader = this.kinectSensor.BodyFrameSource.OpenReader();
                this.bodyFrameReader.FrameArrived += this.Reader_FrameArrived;
                return true;
            }
            return false;
        }

        public async Task<object> Close(object input)
        {
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

        /// <summary>
        /// Handles the body frame data arriving from the sensor
        /// </summary>
        /// <param name="sender">object sending the event</param>
        /// <param name="e">event arguments</param>
        private void Reader_FrameArrived(object sender, BodyFrameArrivedEventArgs e)
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

                    // The first time GetAndRefreshBodyData is called, Kinect will allocate each Body in the array.
                    // As long as those body objects are not disposed and not set to null in the array,
                    // those body objects will be re-used.
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
            /*
            BodyFrameReference frameReference = e.FrameReference;

            try
            {
                BodyFrame frame = frameReference.AcquireFrame();

                if (frame != null)
                {
                    // BodyFrame is IDisposable
                    using (frame)
                    {
                        this.framesSinceUpdate++;
                        frame.GetAndRefreshBodyData(this.bodies);
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
                                    jsJoints[jointType.ToString()] = new Point(depthSpacePoint.X, depthSpacePoint.Y);
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
            catch (Exception)
            {
                // ignore if the frame is no longer available
            }
             * */
        }
    }
}