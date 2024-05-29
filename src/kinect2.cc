#define NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED

#include <cmath>
#include <thread>
#include <mutex>
#include <napi.h>
#include "Kinect.h"
#include "Globals.h"
#include "Structs.h"

IKinectSensor*                      m_pKinectSensor = NULL;
ICoordinateMapper*                  m_pCoordinateMapper = NULL;
IColorFrameReader*                  m_pColorFrameReader = NULL;
IInfraredFrameReader*               m_pInfraredFrameReader = NULL;
ILongExposureInfraredFrameReader*   m_pLongExposureInfraredFrameReader = NULL;
IDepthFrameReader*                  m_pDepthFrameReader = NULL;
IDepthFrameReader*                  m_pRawDepthFrameReader = NULL;
IBodyFrameReader*                    m_pBodyFrameReader = NULL;
IMultiSourceFrameReader*            m_pMultiSourceFrameReader = NULL;

RGBQUAD*                m_pColorPixels = new RGBQUAD[cColorWidth * cColorHeight];
char*                   m_pInfraredPixels = new char[cInfraredWidth * cInfraredHeight];
char*                   m_pLongExposureInfraredPixels = new char[cLongExposureInfraredWidth * cLongExposureInfraredHeight];
char*                   m_pDepthPixels = new char[cDepthWidth * cDepthHeight];
UINT16*                 m_pRawDepthValues = new UINT16[cDepthWidth * cDepthHeight];
RGBQUAD*                m_pDepthColorPixels = new RGBQUAD[cDepthWidth * cDepthHeight];

bool                    m_trackPixelsForBodyIndexV8[BODY_COUNT];

RGBQUAD                 m_pBodyIndexColorPixels[BODY_COUNT][cColorWidth * cColorHeight];

JSBodyFrame             m_jsBodyFrame;

DepthSpacePoint*        m_pDepthCoordinatesForColor = new DepthSpacePoint[cColorWidth * cColorHeight];
ColorSpacePoint*        m_pColorCoordinatesForDepth = new ColorSpacePoint[cDepthWidth * cDepthHeight];

//enabledFrameSourceTypes refers to the kinect SDK frame source types
DWORD                   m_enabledFrameSourceTypes = 0;
//enabledFrameTypes is our own list of frame types
//this is the kinect SDK list + additional ones (body index in color space, etc...)
unsigned long           m_enabledFrameTypes = 0;

// COLOR
std::thread               m_tColorThread;
std::mutex                m_mColorReaderMutex;
bool                      m_bColorThreadRunning = false;
Napi::ThreadSafeFunction  m_tsfnColor;
std::mutex                m_mColorThreadJoinedMutex;

float                     m_fColorHorizontalFieldOfView;
float                     m_fColorVerticalFieldOfView;
float                     m_fColorDiagonalFieldOfView;

// Infrared
std::thread               m_tInfraredThread;
std::mutex                m_mInfraredReaderMutex;
bool                      m_bInfraredThreadRunning = false;
Napi::ThreadSafeFunction  m_tsfnInfrared;
std::mutex                m_mInfraredThreadJoinedMutex;

float                     m_fInfraredHorizontalFieldOfView;
float                     m_fInfraredVerticalFieldOfView;
float                     m_fInfraredDiagonalFieldOfView;

// LongExposureInfrared
std::thread               m_tLongExposureInfraredThread;
std::mutex                m_mLongExposureInfraredReaderMutex;
bool                      m_bLongExposureInfraredThreadRunning = false;
Napi::ThreadSafeFunction  m_tsfnLongExposureInfrared;
std::mutex                m_mLongExposureInfraredThreadJoinedMutex;

float                     m_fLongExposureInfraredHorizontalFieldOfView;
float                     m_fLongExposureInfraredVerticalFieldOfView;
float                     m_fLongExposureInfraredDiagonalFieldOfView;

// Depth
std::thread               m_tDepthThread;
std::mutex                m_mDepthReaderMutex;
bool                      m_bDepthThreadRunning = false;
Napi::ThreadSafeFunction  m_tsfnDepth;
std::mutex                m_mDepthThreadJoinedMutex;

float                     m_fDepthHorizontalFieldOfView;
float                     m_fDepthVerticalFieldOfView;
float                     m_fDepthDiagonalFieldOfView;

// RawDepth
std::thread               m_tRawDepthThread;
std::mutex                m_mRawDepthReaderMutex;
bool                      m_bRawDepthThreadRunning = false;
Napi::ThreadSafeFunction  m_tsfnRawDepth;
std::mutex                m_mRawDepthThreadJoinedMutex;

// Body
std::thread               m_tBodyThread;
std::mutex                m_mBodyReaderMutex;
bool                      m_bBodyThreadRunning = false;
Napi::ThreadSafeFunction  m_tsfnBody;
std::mutex                m_mBodyThreadJoinedMutex;

// MultiSource
std::thread               m_tMultiSourceThread;
std::mutex                m_mMultiSourceReaderMutex;
bool                      m_bMultiSourceThreadRunning = false;
Napi::ThreadSafeFunction  m_tsfnMultiSource;
std::mutex                m_mMultiSourceThreadJoinedMutex;

bool                      m_includeJointFloorData = false;

template<class Interface>
void stopReader(
  std::mutex* mutex,
  bool* threadCondition,
  std::thread* thread,
  Interface* pInterfaceToRelease
)
{
  mutex->lock();
  bool wasRunning = *threadCondition;
  *threadCondition = false;
  mutex->unlock();
  SafeRelease(pInterfaceToRelease);
}

class WaitForTheadJoinWorker : public Napi::AsyncWorker {
  public:
    WaitForTheadJoinWorker(Napi::Function& callback, std::mutex * threadJoinedMutex)
    : AsyncWorker(callback) {
      this->threadJoinedMutex = threadJoinedMutex;
    }

    ~WaitForTheadJoinWorker() {}
  // This code will be executed on the worker thread
  void Execute() override {
    this->threadJoinedMutex->lock();
  }

  void OnOK() override {
    this->threadJoinedMutex->unlock();
    Napi::HandleScope scope(Env());
    Callback().Call({Env().Null(), Napi::String::New(Env(), "OK")});
  }

  private:
    std::mutex * threadJoinedMutex;
};

Napi::Value MethodOpen(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  HRESULT hr;
  hr = GetDefaultKinectSensor(&m_pKinectSensor);
  if (SUCCEEDED(hr))
  {
    hr = m_pKinectSensor->get_CoordinateMapper(&m_pCoordinateMapper);
  }
  if (SUCCEEDED(hr))
  {
    hr = m_pKinectSensor->Open();
  }
  if (!m_pKinectSensor || FAILED(hr))
  {
    return Napi::Boolean::New(env, false);
  }
  return Napi::Boolean::New(env, true);
}

Napi::Value MethodClose(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  printf("[kinect2.cc] MethodClose\n");

  // all readers will be stopped by the js context by now

  SafeRelease(m_pCoordinateMapper);
  if (m_pKinectSensor)
  {
    m_pKinectSensor->Close();
  }
  SafeRelease(m_pKinectSensor);

  printf("[kinect2.cc] closed");
  return Napi::Boolean::New(env, true);
}

HRESULT processColorFrameData(IColorFrame* pColorFrame)
{
  HRESULT hr;
  IFrameDescription* pColorFrameDescription = NULL;
  ColorImageFormat imageFormat = ColorImageFormat_None;
  UINT nColorBufferSize = 0;
  RGBQUAD *pColorBuffer = NULL;

  hr = pColorFrame->get_FrameDescription(&pColorFrameDescription);

  if (SUCCEEDED(hr))
  {
    hr = pColorFrameDescription->get_HorizontalFieldOfView(&m_fColorHorizontalFieldOfView);
  }
  if (SUCCEEDED(hr))
  {
    hr = pColorFrameDescription->get_VerticalFieldOfView(&m_fColorVerticalFieldOfView);
  }
  if (SUCCEEDED(hr))
  {
    hr = pColorFrameDescription->get_DiagonalFieldOfView(&m_fColorDiagonalFieldOfView);
  }
  if (SUCCEEDED(hr))
  {
    hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
  }
  if (SUCCEEDED(hr))
  {
    if (imageFormat == ColorImageFormat_Rgba)
    {
      hr = pColorFrame->AccessRawUnderlyingBuffer(&nColorBufferSize, reinterpret_cast<BYTE**>(&pColorBuffer));
      if(SUCCEEDED(hr))
      {
        memcpy(m_pColorPixels, pColorBuffer, cColorWidth * cColorHeight * sizeof(RGBQUAD));
      }
    }
    else if (m_pColorPixels)
    {
      pColorBuffer = m_pColorPixels;
      nColorBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
      hr = pColorFrame->CopyConvertedFrameDataToArray(nColorBufferSize, reinterpret_cast<BYTE*>(pColorBuffer), ColorImageFormat_Rgba);
    }
    else
    {
      hr = E_FAIL;
    }
  }

  SafeRelease(pColorFrameDescription);
  return hr;
}

HRESULT processInfraredFrameData(IInfraredFrame* pInfraredFrame)
{
  HRESULT hr;
  IFrameDescription* pInfraredFrameDescription = NULL;
  UINT nInfraredBufferSize = 0;
  UINT16 *pInfraredBuffer = NULL;

  hr = pInfraredFrame->get_FrameDescription(&pInfraredFrameDescription);

  if (SUCCEEDED(hr))
  {
    hr = pInfraredFrameDescription->get_HorizontalFieldOfView(&m_fInfraredHorizontalFieldOfView);
  }

  if (SUCCEEDED(hr))
  {
    hr = pInfraredFrameDescription->get_VerticalFieldOfView(&m_fInfraredVerticalFieldOfView);
  }

  if (SUCCEEDED(hr))
  {
    hr = pInfraredFrameDescription->get_DiagonalFieldOfView(&m_fInfraredDiagonalFieldOfView);
  }

  if (SUCCEEDED(hr))
  {
    hr = pInfraredFrame->AccessUnderlyingBuffer(&nInfraredBufferSize, &pInfraredBuffer);
  }

  if (SUCCEEDED(hr))
  {
    if (m_pInfraredPixels && pInfraredBuffer)
    {
      char* pDest = m_pInfraredPixels;

      // end pixel is start + width*height - 1
      const UINT16* pInfraredBufferEnd = pInfraredBuffer + (cInfraredWidth * cInfraredHeight);

      while (pInfraredBuffer < pInfraredBufferEnd)
      {
        // normalize the incoming infrared data (ushort) to a float ranging from
        // [InfraredOutputValueMinimum, InfraredOutputValueMaximum] by
        // 1. dividing the incoming value by the source maximum value
        float intensityRatio = static_cast<float>(*pInfraredBuffer) / InfraredSourceValueMaximum;

        // 2. dividing by the (average scene value * standard deviations)
        intensityRatio /= InfraredSceneValueAverage * InfraredSceneStandardDeviations;

        // 3. limiting the value to InfraredOutputValueMaximum
        intensityRatio = min(InfraredOutputValueMaximum, intensityRatio);

        // 4. limiting the lower value InfraredOutputValueMinimym
        intensityRatio = max(InfraredOutputValueMinimum, intensityRatio);

        // 5. converting the normalized value to a byte and using the result
        // as the RGB components required by the image
        byte intensity = static_cast<byte>(intensityRatio * 255.0f);
        *pDest = intensity;

        ++pDest;
        ++pInfraredBuffer;
      }
    }
  }
  SafeRelease(pInfraredFrameDescription);
  return hr;
}

HRESULT processLongExposureInfraredFrameData(ILongExposureInfraredFrame* pLongExposureInfraredFrame)
{
  HRESULT hr;
  IFrameDescription* pLongExposureInfraredFrameDescription = NULL;
  UINT nLongExposureInfraredBufferSize = 0;
  UINT16 *pLongExposureInfraredBuffer = NULL;

  hr = pLongExposureInfraredFrame->get_FrameDescription(&pLongExposureInfraredFrameDescription);

  if (SUCCEEDED(hr))
  {
    hr = pLongExposureInfraredFrameDescription->get_HorizontalFieldOfView(&m_fLongExposureInfraredHorizontalFieldOfView);
  }

  if (SUCCEEDED(hr))
  {
    hr = pLongExposureInfraredFrameDescription->get_VerticalFieldOfView(&m_fLongExposureInfraredVerticalFieldOfView);
  }

  if (SUCCEEDED(hr))
  {
    hr = pLongExposureInfraredFrameDescription->get_DiagonalFieldOfView(&m_fLongExposureInfraredDiagonalFieldOfView);
  }

  if (SUCCEEDED(hr))
  {
    hr = pLongExposureInfraredFrame->AccessUnderlyingBuffer(&nLongExposureInfraredBufferSize, &pLongExposureInfraredBuffer);
  }

  if (SUCCEEDED(hr))
  {
    if (m_pLongExposureInfraredPixels && pLongExposureInfraredBuffer)
    {
      char* pDest = m_pLongExposureInfraredPixels;

      // end pixel is start + width*height - 1
      const UINT16* pLongExposureInfraredBufferEnd = pLongExposureInfraredBuffer + (cLongExposureInfraredWidth * cLongExposureInfraredHeight);

      while (pLongExposureInfraredBuffer < pLongExposureInfraredBufferEnd)
      {
        // normalize the incoming infrared data (ushort) to a float ranging from
        // [InfraredOutputValueMinimum, InfraredOutputValueMaximum] by
        // 1. dividing the incoming value by the source maximum value
        float intensityRatio = static_cast<float>(*pLongExposureInfraredBuffer) / InfraredSourceValueMaximum;

        // 2. dividing by the (average scene value * standard deviations)
        intensityRatio /= InfraredSceneValueAverage * InfraredSceneStandardDeviations;

        // 3. limiting the value to InfraredOutputValueMaximum
        intensityRatio = min(InfraredOutputValueMaximum, intensityRatio);

        // 4. limiting the lower value InfraredOutputValueMinimym
        intensityRatio = max(InfraredOutputValueMinimum, intensityRatio);

        // 5. converting the normalized value to a byte and using the result
        // as the RGB components required by the image
        byte intensity = static_cast<byte>(intensityRatio * 255.0f);
        *pDest = intensity;

        ++pDest;
        ++pLongExposureInfraredBuffer;
      }
    }
  }

  SafeRelease(pLongExposureInfraredFrameDescription);
  return hr;
}

HRESULT processDepthFrameData(IDepthFrame* pDepthFrame)
{
  HRESULT hr;
  IFrameDescription* pDepthFrameDescription = NULL;
  UINT nDepthBufferSize = 0;
  UINT16 *pDepthBuffer = NULL;
  USHORT nDepthMinReliableDistance = 0;
  USHORT nDepthMaxDistance = 0;
  int mapDepthToByte = 8000 / 256;

  hr = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);

  if (SUCCEEDED(hr))
  {
    hr = pDepthFrameDescription->get_HorizontalFieldOfView(&m_fDepthHorizontalFieldOfView);
  }

  if (SUCCEEDED(hr))
  {
    hr = pDepthFrameDescription->get_VerticalFieldOfView(&m_fDepthVerticalFieldOfView);
  }

  if (SUCCEEDED(hr))
  {
    hr = pDepthFrameDescription->get_DiagonalFieldOfView(&m_fDepthDiagonalFieldOfView);
  }
  if (SUCCEEDED(hr))
  {
    hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
  }
  if (SUCCEEDED(hr))
  {
    hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
  }
  if (SUCCEEDED(hr))
  {
    hr = pDepthFrame->AccessUnderlyingBuffer(&nDepthBufferSize, &pDepthBuffer);
  }

  if (SUCCEEDED(hr))
  {
    mapDepthToByte = nDepthMaxDistance / 256;
    if (m_pDepthPixels && pDepthBuffer)
    {
      char* pDepthPixel = m_pDepthPixels;

      // end pixel is start + width*height - 1
      const UINT16* pDepthBufferEnd = pDepthBuffer + (cDepthWidth * cDepthHeight);

      while (pDepthBuffer < pDepthBufferEnd)
      {
        USHORT depth = *pDepthBuffer;

        BYTE intensity = static_cast<BYTE>(depth >= nDepthMinReliableDistance && depth <= nDepthMaxDistance ? (depth / mapDepthToByte) : 0);
        *pDepthPixel = intensity;

        ++pDepthPixel;
        ++pDepthBuffer;
      }
    }
  }

  SafeRelease(pDepthFrameDescription);
  return hr;
}

HRESULT processRawDepthFrameData(IDepthFrame* pDepthFrame)
{
  HRESULT hr;
  IFrameDescription* pDepthFrameDescription = NULL;
  USHORT nDepthMinReliableDistance;
  USHORT nDepthMaxDistance;

  hr = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);

  if (SUCCEEDED(hr))
  {
    hr = pDepthFrameDescription->get_HorizontalFieldOfView(&m_fDepthHorizontalFieldOfView);
  }

  if (SUCCEEDED(hr))
  {
    hr = pDepthFrameDescription->get_VerticalFieldOfView(&m_fDepthVerticalFieldOfView);
  }

  if (SUCCEEDED(hr))
  {
    hr = pDepthFrameDescription->get_DiagonalFieldOfView(&m_fDepthDiagonalFieldOfView);
  }
  if (SUCCEEDED(hr))
  {
    hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
  }
  if (SUCCEEDED(hr))
  {
    hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
  }
  if (SUCCEEDED(hr))
  {
    UINT nDepthValuesSize = cDepthWidth * cDepthHeight;
    hr = pDepthFrame->CopyFrameDataToArray(nDepthValuesSize, m_pRawDepthValues);
  }
  SafeRelease(pDepthFrameDescription);
  return hr;
}

void calculateCameraAngle_(Vector4 floorClipPlane, float* cameraAngleRadians, float* cosCameraAngle, float*sinCameraAngle)
{
  *cameraAngleRadians = atan(floorClipPlane.z / floorClipPlane.y);
  *cosCameraAngle = cos(*cameraAngleRadians);
  *sinCameraAngle = sin(*cameraAngleRadians);
}

float getJointDistanceFromFloor_(Vector4 floorClipPlane, Joint joint, float cameraAngleRadians, float cosCameraAngle, float sinCameraAngle)
{
  return floorClipPlane.w + joint.Position.Y * cosCameraAngle + joint.Position.Z * sinCameraAngle;
}

HRESULT processBodyFrameData(IBodyFrame* pBodyFrame)
{
  IBody* ppBodies[BODY_COUNT] = {0};
  HRESULT hr, hr2;
  hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);
  if (SUCCEEDED(hr))
  {
    DepthSpacePoint depthPoint = {0};
    ColorSpacePoint colorPoint = {0};
    Vector4 floorClipPlane = {0};
    //floor clip plane
    m_jsBodyFrame.hasFloorClipPlane = false;
    hr2 = pBodyFrame->get_FloorClipPlane(&floorClipPlane);
    if(SUCCEEDED(hr2))
    {
      m_jsBodyFrame.hasFloorClipPlane = true;
      m_jsBodyFrame.floorClipPlaneX = floorClipPlane.x;
      m_jsBodyFrame.floorClipPlaneY = floorClipPlane.y;
      m_jsBodyFrame.floorClipPlaneZ = floorClipPlane.z;
      m_jsBodyFrame.floorClipPlaneW = floorClipPlane.w;
      //camera angle
      calculateCameraAngle_(floorClipPlane, &m_jsBodyFrame.cameraAngle, &m_jsBodyFrame.cosCameraAngle, &m_jsBodyFrame.sinCameraAngle);
    }
    for (int i = 0; i < _countof(ppBodies); ++i)
    {
      IBody* pBody = ppBodies[i];
      if (pBody)
      {
        BOOLEAN bTracked = false;
        hr = pBody->get_IsTracked(&bTracked);
        m_jsBodyFrame.bodies[i].tracked = bTracked;

        if(bTracked)
        {
          UINT64 iTrackingId = 0;
          hr = pBody->get_TrackingId(&iTrackingId);
          m_jsBodyFrame.bodies[i].trackingId = iTrackingId;
          //hand state
          HandState leftHandState = HandState_Unknown;
          HandState rightHandState = HandState_Unknown;
          pBody->get_HandLeftState(&leftHandState);
          pBody->get_HandRightState(&rightHandState);
          m_jsBodyFrame.bodies[i].leftHandState = leftHandState;
          m_jsBodyFrame.bodies[i].rightHandState = rightHandState;
          //go through the joints
          Joint joints[JointType_Count];
          JointOrientation jointOrientations[JointType_Count];
          hr = pBody->GetJoints(_countof(joints), joints);
          if(SUCCEEDED(hr))
          {
            hr = pBody->GetJointOrientations(_countof(jointOrientations), jointOrientations);
          }
          if (SUCCEEDED(hr))
          {
            for (int j = 0; j < _countof(joints); ++j)
            {
              m_pCoordinateMapper->MapCameraPointToDepthSpace(joints[j].Position, &depthPoint);
              m_pCoordinateMapper->MapCameraPointToColorSpace(joints[j].Position, &colorPoint);

              m_jsBodyFrame.bodies[i].joints[j].depthX = depthPoint.X / cDepthWidth;
              m_jsBodyFrame.bodies[i].joints[j].depthY = depthPoint.Y / cDepthHeight;

              m_jsBodyFrame.bodies[i].joints[j].colorX = colorPoint.X / cColorWidth;
              m_jsBodyFrame.bodies[i].joints[j].colorY = colorPoint.Y / cColorHeight;

              m_jsBodyFrame.bodies[i].joints[j].cameraX = joints[j].Position.X;
              m_jsBodyFrame.bodies[i].joints[j].cameraY = joints[j].Position.Y;
              m_jsBodyFrame.bodies[i].joints[j].cameraZ = joints[j].Position.Z;

              m_jsBodyFrame.bodies[i].joints[j].orientationX = jointOrientations[j].Orientation.x;
              m_jsBodyFrame.bodies[i].joints[j].orientationY = jointOrientations[j].Orientation.y;
              m_jsBodyFrame.bodies[i].joints[j].orientationZ = jointOrientations[j].Orientation.z;
              m_jsBodyFrame.bodies[i].joints[j].orientationW = jointOrientations[j].Orientation.w;

              m_jsBodyFrame.bodies[i].joints[j].jointType = joints[j].JointType;
              m_jsBodyFrame.bodies[i].joints[j].trackingState = joints[j].TrackingState;
            }
            //calculate body ground position
            if(m_includeJointFloorData && m_jsBodyFrame.hasFloorClipPlane)
            {
              for (int j = 0; j < _countof(joints); ++j)
              {
                m_jsBodyFrame.bodies[i].joints[j].hasFloorData = true;
                //distance of joint to floor
                float distance = getJointDistanceFromFloor_(floorClipPlane, joints[j], m_jsBodyFrame.cameraAngle, m_jsBodyFrame.cosCameraAngle, m_jsBodyFrame.sinCameraAngle);
                CameraSpacePoint p;
                p.X = joints[j].Position.X;
                p.Y = joints[j].Position.Y - distance;
                p.Z = joints[j].Position.Z;

                m_pCoordinateMapper->MapCameraPointToDepthSpace(p, &depthPoint);
                m_pCoordinateMapper->MapCameraPointToColorSpace(p, &colorPoint);

                m_jsBodyFrame.bodies[i].joints[j].floorDepthX = depthPoint.X / cDepthWidth;
                m_jsBodyFrame.bodies[i].joints[j].floorDepthY = depthPoint.Y / cDepthHeight;

                m_jsBodyFrame.bodies[i].joints[j].floorColorX = colorPoint.X / cColorWidth;
                m_jsBodyFrame.bodies[i].joints[j].floorColorY = colorPoint.Y / cColorHeight;

                m_jsBodyFrame.bodies[i].joints[j].floorCameraX = p.X;
                m_jsBodyFrame.bodies[i].joints[j].floorCameraY = p.Y;
                m_jsBodyFrame.bodies[i].joints[j].floorCameraZ = p.Z;
              }
            }
            else
            {
              for (int j = 0; j < _countof(joints); ++j)
              {
                m_jsBodyFrame.bodies[i].joints[j].hasFloorData = false;
              }
            }
          }
        }
        else
        {
          m_jsBodyFrame.bodies[i].trackingId = 0;
        }
      }
    }
  }

  for (int i = 0; i < _countof(ppBodies); ++i)
  {
    SafeRelease(ppBodies[i]);
  }
  return hr;
}

Napi::Object getV8BodyFrame(Napi::Env env, JSBodyFrame* bodyFrameRef)
{
  Napi::Object v8BodyResult = Napi::Object::New(env);

  //bodies
  Napi::Array v8bodies = Napi::Array::New(env, BODY_COUNT);
  for(int i = 0; i < BODY_COUNT; i++)
  {
    //create a body object
    Napi::Object v8body = Napi::Object::New(env);

    v8body.Set(Napi::String::New(env, "bodyIndex"), Napi::Number::New(env, i));
    v8body.Set(Napi::String::New(env, "tracked"), Napi::Boolean::New(env, m_jsBodyFrame.bodies[i].tracked));
    if(m_jsBodyFrame.bodies[i].tracked)
    {
      v8body.Set(Napi::String::New(env, "trackingId"), Napi::String::New(env, std::to_string(m_jsBodyFrame.bodies[i].trackingId)));
      //hand states
      v8body.Set(Napi::String::New(env, "leftHandState"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].leftHandState));
      v8body.Set(Napi::String::New(env, "rightHandState"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].rightHandState));
      Napi::Array v8joints = Napi::Array::New(env, _countof(m_jsBodyFrame.bodies[i].joints));
      //joints
      for (int j = 0; j < _countof(m_jsBodyFrame.bodies[i].joints); ++j)
      {
        Napi::Object v8joint = Napi::Object::New(env);
        v8joint.Set(Napi::String::New(env, "depthX"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].depthX));
        v8joint.Set(Napi::String::New(env, "depthY"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].depthY));
        v8joint.Set(Napi::String::New(env, "colorX"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].colorX));
        v8joint.Set(Napi::String::New(env, "colorY"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].colorY));
        v8joint.Set(Napi::String::New(env, "cameraX"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].cameraX));
        v8joint.Set(Napi::String::New(env, "cameraY"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].cameraY));
        v8joint.Set(Napi::String::New(env, "cameraZ"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].cameraZ));
        //orientation
        v8joint.Set(Napi::String::New(env, "orientationX"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].orientationX));
        v8joint.Set(Napi::String::New(env, "orientationY"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].orientationY));
        v8joint.Set(Napi::String::New(env, "orientationZ"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].orientationZ));
        v8joint.Set(Napi::String::New(env, "orientationW"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].orientationW));
        //body ground
        if(m_jsBodyFrame.bodies[i].joints[j].hasFloorData)
        {
          v8joint.Set(Napi::String::New(env, "floorDepthX"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].floorDepthX));
          v8joint.Set(Napi::String::New(env, "floorDepthY"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].floorDepthY));
          v8joint.Set(Napi::String::New(env, "floorColorX"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].floorColorX));
          v8joint.Set(Napi::String::New(env, "floorColorY"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].floorColorY));
          v8joint.Set(Napi::String::New(env, "floorCameraX"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].floorCameraX));
          v8joint.Set(Napi::String::New(env, "floorCameraY"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].floorCameraY));
          v8joint.Set(Napi::String::New(env, "floorCameraZ"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].floorCameraZ));
        }
        //joint type
        v8joint.Set(Napi::String::New(env, "jointType"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].jointType));
        //tracking state
        v8joint.Set(Napi::String::New(env, "trackingState"), Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].trackingState));
        //insert in array
        v8joints.Set(Napi::Number::New(env, m_jsBodyFrame.bodies[i].joints[j].jointType), v8joint);
      }
      v8body.Set(Napi::String::New(env, "joints"), v8joints);
    }
    v8bodies.Set(i, v8body);
  }
  v8BodyResult.Set(Napi::String::New(env, "bodies"), v8bodies);

  //floor plane
  if(m_jsBodyFrame.hasFloorClipPlane) {
    Napi::Object v8FloorClipPlane = Napi::Object::New(env);
    v8FloorClipPlane.Set(Napi::String::New(env, "x"), Napi::Number::New(env, m_jsBodyFrame.floorClipPlaneX));
    v8FloorClipPlane.Set(Napi::String::New(env, "y"), Napi::Number::New(env, m_jsBodyFrame.floorClipPlaneY));
    v8FloorClipPlane.Set(Napi::String::New(env, "z"), Napi::Number::New(env, m_jsBodyFrame.floorClipPlaneZ));
    v8FloorClipPlane.Set(Napi::String::New(env, "w"), Napi::Number::New(env, m_jsBodyFrame.floorClipPlaneW));
    v8BodyResult.Set(Napi::String::New(env, "floorClipPlane"), v8FloorClipPlane);
  }

  return v8BodyResult;
}

Napi::Value MethodOpenColorReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (m_bColorThreadRunning) {
    Napi::TypeError::New(env, "color thread already running")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Wrong number of arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  } else if (!info[0].IsFunction()){
    throw Napi::TypeError::New( env, "Expected first arg to be function" );
  }

  HRESULT hr;
  IColorFrameSource* pColorFrameSource = NULL;
  hr = m_pKinectSensor->get_ColorFrameSource(&pColorFrameSource);
  if (SUCCEEDED(hr))
  {
    hr = pColorFrameSource->OpenReader(&m_pColorFrameReader);
  }
  if (SUCCEEDED(hr))
  {
    m_bColorThreadRunning = true;

    m_tsfnColor = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),     // JavaScript function called asynchronously
      "Kinect2 Color Listener",              // Name
      1,                                // 1 call in queue
      1,                                // Only one thread will use this initially
      []( Napi::Env ) {                 // Finalizer used to clean threads up
        m_tColorThread.join();
        m_mColorThreadJoinedMutex.unlock();
        printf("[kinect2.cc] color thread joined\n");
      });

    m_mColorThreadJoinedMutex.lock();
    m_tColorThread = std::thread( [] {
      
      auto callback = []( Napi::Env env, Napi::Function jsCallback, RGBQUAD* pixelsRef ) {
        if (!m_bColorThreadRunning) {
          return;
        }
        m_mColorReaderMutex.lock();
        Napi::Buffer<RGBQUAD> colorBuffer = Napi::Buffer<RGBQUAD>::NewOrCopy(env, m_pColorPixels, cColorWidth * cColorHeight);
        jsCallback.Call( { colorBuffer } );
        m_mColorReaderMutex.unlock();
      };

      while(m_bColorThreadRunning)
      {
        IColorFrame* pColorFrame = NULL;
        HRESULT hr = E_FAIL;
        m_mColorReaderMutex.lock();
        hr = m_pColorFrameReader->AcquireLatestFrame(&pColorFrame);
        if (SUCCEEDED(hr))
        {
          processColorFrameData(pColorFrame);
        }
        if (!m_bColorThreadRunning)
        {
          m_mColorReaderMutex.unlock();
          break;
        }
        m_mColorReaderMutex.unlock();
        if (SUCCEEDED(hr))
        {
          napi_status status = m_tsfnColor.BlockingCall( m_pColorPixels, callback );
          if ( status != napi_ok )
          {
            break;
          }
        }
        SafeRelease(pColorFrame);
      }
      m_bColorThreadRunning = false;
      m_tsfnColor.Release();
    } );
  }

  SafeRelease(pColorFrameSource);
  // m_mColorReaderMutex.unlock();
  if (FAILED(hr))
  {
    return Napi::Boolean::New(env, false);
  }
  return Napi::Boolean::New(env, true);
}

Napi::Value MethodCloseColorReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  stopReader(&m_mColorReaderMutex, &m_bColorThreadRunning, &m_tColorThread, m_pColorFrameReader);
  Napi::Function cb = info[0].As<Napi::Function>();
  WaitForTheadJoinWorker* wk = new WaitForTheadJoinWorker(cb, &m_mColorThreadJoinedMutex);
  wk->Queue();
  return info.Env().Undefined();
}

Napi::Value MethodOpenInfraredReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (m_bInfraredThreadRunning) {
    Napi::TypeError::New(env, "Infrared thread already running")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Wrong number of arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  } else if (!info[0].IsFunction()){
    throw Napi::TypeError::New( env, "Expected first arg to be function" );
  }

  HRESULT hr;
  IInfraredFrameSource* pInfraredFrameSource = NULL;
  hr = m_pKinectSensor->get_InfraredFrameSource(&pInfraredFrameSource);
  if (SUCCEEDED(hr))
  {
    hr = pInfraredFrameSource->OpenReader(&m_pInfraredFrameReader);
  }
  if (SUCCEEDED(hr))
  {
    m_bInfraredThreadRunning = true;

    m_tsfnInfrared = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),     // JavaScript function called asynchronously
      "Kinect2 Infrared Listener",              // Name
      1,                                // 1 call in queue
      1,                                // Only one thread will use this initially
      []( Napi::Env ) {                 // Finalizer used to clean threads up
        m_tInfraredThread.join();
        m_mInfraredThreadJoinedMutex.unlock();
        printf("[kinect2.cc] ir thread joined\n");
      });

    m_mInfraredThreadJoinedMutex.lock();
    m_tInfraredThread = std::thread( [] {
      
      auto callback = []( Napi::Env env, Napi::Function jsCallback, char* pixelsRef ) {
        if (!m_bInfraredThreadRunning) {
          return;
        }
        m_mInfraredReaderMutex.lock();
        Napi::Buffer<char> infraredBuffer = Napi::Buffer<char>::NewOrCopy(env, m_pInfraredPixels, cInfraredWidth * cInfraredHeight);
        jsCallback.Call( { infraredBuffer } );
        m_mInfraredReaderMutex.unlock();
      };

      while(m_bInfraredThreadRunning)
      {
        IInfraredFrame* pInfraredFrame = NULL;
        HRESULT hr = E_FAIL;
        m_mInfraredReaderMutex.lock();
        hr = m_pInfraredFrameReader->AcquireLatestFrame(&pInfraredFrame);
        if (SUCCEEDED(hr))
        {
          processInfraredFrameData(pInfraredFrame);
        }
        if (!m_bInfraredThreadRunning)
        {
          m_mInfraredReaderMutex.unlock();
          break;
        }
        m_mInfraredReaderMutex.unlock();
        if (SUCCEEDED(hr))
        {
          napi_status status = m_tsfnInfrared.BlockingCall( m_pInfraredPixels, callback );
          if ( status != napi_ok )
          {
            break;
          }
        }
        SafeRelease(pInfraredFrame);
      }
      m_bInfraredThreadRunning = false;
      m_tsfnInfrared.Release();
    } );
  }

  SafeRelease(pInfraredFrameSource);
  // m_mInfraredReaderMutex.unlock();
  if (FAILED(hr))
  {
    return Napi::Boolean::New(env, false);
  }
  return Napi::Boolean::New(env, true);
}

Napi::Value MethodCloseInfraredReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  stopReader(&m_mInfraredReaderMutex, &m_bInfraredThreadRunning, &m_tInfraredThread, m_pInfraredFrameReader);
  Napi::Function cb = info[0].As<Napi::Function>();
  WaitForTheadJoinWorker* wk = new WaitForTheadJoinWorker(cb, &m_mInfraredThreadJoinedMutex);
  wk->Queue();
  return info.Env().Undefined();
}

Napi::Value MethodOpenLongExposureInfraredReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (m_bLongExposureInfraredThreadRunning) {
    Napi::TypeError::New(env, "LongExposureInfrared thread already running")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Wrong number of arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  } else if (!info[0].IsFunction()){
    throw Napi::TypeError::New( env, "Expected first arg to be function" );
  }

  HRESULT hr;
  ILongExposureInfraredFrameSource* pLongExposureInfraredFrameSource = NULL;
  hr = m_pKinectSensor->get_LongExposureInfraredFrameSource(&pLongExposureInfraredFrameSource);
  if (SUCCEEDED(hr))
  {
    hr = pLongExposureInfraredFrameSource->OpenReader(&m_pLongExposureInfraredFrameReader);
  }
  if (SUCCEEDED(hr))
  {
    m_bLongExposureInfraredThreadRunning = true;

    m_tsfnLongExposureInfrared = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),     // JavaScript function called asynchronously
      "Kinect2 LongExposureInfrared Listener",              // Name
      1,                                // 1 call in queue
      1,                                // Only one thread will use this initially
      []( Napi::Env ) {                 // Finalizer used to clean threads up
        m_tLongExposureInfraredThread.join();
        m_mLongExposureInfraredThreadJoinedMutex.unlock();
        printf("[kinect2.cc] long ir thread joined\n");
      });

    m_mLongExposureInfraredThreadJoinedMutex.lock();
    m_tLongExposureInfraredThread = std::thread( [] {
      
      auto callback = []( Napi::Env env, Napi::Function jsCallback, char* pixelsRef ) {
        if (!m_bLongExposureInfraredThreadRunning) {
          return;
        }
        m_mLongExposureInfraredReaderMutex.lock();
        Napi::Buffer<char> longExposureInfraredBuffer = Napi::Buffer<char>::NewOrCopy(env, m_pLongExposureInfraredPixels, cLongExposureInfraredWidth * cLongExposureInfraredHeight);
        jsCallback.Call( { longExposureInfraredBuffer } );
        m_mLongExposureInfraredReaderMutex.unlock();
      };

      while(m_bLongExposureInfraredThreadRunning)
      {
        ILongExposureInfraredFrame* pLongExposureInfraredFrame = NULL;
        HRESULT hr = E_FAIL;
        m_mLongExposureInfraredReaderMutex.lock();
        hr = m_pLongExposureInfraredFrameReader->AcquireLatestFrame(&pLongExposureInfraredFrame);
        if (SUCCEEDED(hr))
        {
          processLongExposureInfraredFrameData(pLongExposureInfraredFrame);
        }
        if (!m_bLongExposureInfraredThreadRunning)
        {
          m_mLongExposureInfraredReaderMutex.unlock();
          break;
        }
        m_mLongExposureInfraredReaderMutex.unlock();
        if (SUCCEEDED(hr))
        {
          napi_status status = m_tsfnLongExposureInfrared.BlockingCall( m_pLongExposureInfraredPixels, callback );
          if ( status != napi_ok )
          {
            break;
          }
        }
        SafeRelease(pLongExposureInfraredFrame);
      }
      m_bLongExposureInfraredThreadRunning = false;
      m_tsfnLongExposureInfrared.Release();
    } );
  }

  SafeRelease(pLongExposureInfraredFrameSource);
  // m_mLongExposureInfraredReaderMutex.unlock();
  if (FAILED(hr))
  {
    return Napi::Boolean::New(env, false);
  }
  return Napi::Boolean::New(env, true);
}

Napi::Value MethodCloseLongExposureInfraredReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  stopReader(&m_mLongExposureInfraredReaderMutex, &m_bLongExposureInfraredThreadRunning, &m_tLongExposureInfraredThread, m_pLongExposureInfraredFrameReader);
  Napi::Function cb = info[0].As<Napi::Function>();
  WaitForTheadJoinWorker* wk = new WaitForTheadJoinWorker(cb, &m_mLongExposureInfraredThreadJoinedMutex);
  wk->Queue();
  return info.Env().Undefined();
}

Napi::Value MethodOpenDepthReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (m_bDepthThreadRunning) {
    Napi::TypeError::New(env, "Depth thread already running")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Wrong number of arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  } else if (!info[0].IsFunction()){
    throw Napi::TypeError::New( env, "Expected first arg to be function" );
  }

  HRESULT hr;
  IDepthFrameSource* pDepthFrameSource = NULL;
  hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
  if (SUCCEEDED(hr))
  {
    hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
  }
  if (SUCCEEDED(hr))
  {
    m_bDepthThreadRunning = true;

    m_tsfnDepth = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),     // JavaScript function called asynchronously
      "Kinect2 Depth Listener",              // Name
      1,                                // 1 call in queue
      1,                                // Only one thread will use this initially
      []( Napi::Env ) {                 // Finalizer used to clean threads up
        m_tDepthThread.join();
        m_mDepthThreadJoinedMutex.unlock();
        printf("[kinect2.cc] depth thread joined\n");
      });

    m_mDepthThreadJoinedMutex.lock();
    m_tDepthThread = std::thread( [] {
      
      auto callback = []( Napi::Env env, Napi::Function jsCallback, char* pixelsRef ) {
        if (!m_bDepthThreadRunning) {
          return;
        }
        m_mDepthReaderMutex.lock();
        Napi::Buffer<char> depthPixelsBuffer = Napi::Buffer<char>::NewOrCopy(env, m_pDepthPixels, cDepthWidth * cDepthHeight);
        jsCallback.Call( { depthPixelsBuffer } );
        m_mDepthReaderMutex.unlock();
      };

      while(m_bDepthThreadRunning)
      {
        IDepthFrame* pDepthFrame = NULL;
        HRESULT hr = E_FAIL;
        m_mDepthReaderMutex.lock();
        hr = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);
        if (SUCCEEDED(hr))
        {
          processDepthFrameData(pDepthFrame);
        }
        if (!m_bDepthThreadRunning)
        {
          m_mDepthReaderMutex.unlock();
          break;
        }
        m_mDepthReaderMutex.unlock();
        if (SUCCEEDED(hr))
        {
          napi_status status = m_tsfnDepth.BlockingCall( m_pDepthPixels, callback );
          if ( status != napi_ok )
          {
            break;
          }
        }
        SafeRelease(pDepthFrame);
      }
      m_bDepthThreadRunning = false;
      m_tsfnDepth.Release();
    } );
  }

  SafeRelease(pDepthFrameSource);
  // m_mDepthReaderMutex.unlock();
  if (FAILED(hr))
  {
    return Napi::Boolean::New(env, false);
  }
  return Napi::Boolean::New(env, true);
}

Napi::Value MethodCloseDepthReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  stopReader(&m_mDepthReaderMutex, &m_bDepthThreadRunning, &m_tDepthThread, m_pDepthFrameReader);
  Napi::Function cb = info[0].As<Napi::Function>();
  WaitForTheadJoinWorker* wk = new WaitForTheadJoinWorker(cb, &m_mDepthThreadJoinedMutex);
  wk->Queue();
  return info.Env().Undefined();
}

Napi::Value MethodOpenRawDepthReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (m_bRawDepthThreadRunning) {
    Napi::TypeError::New(env, "Raw Depth thread already running")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Wrong number of arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  } else if (!info[0].IsFunction()){
    throw Napi::TypeError::New( env, "Expected first arg to be function" );
  }

  HRESULT hr;
  IDepthFrameSource* pDepthFrameSource = NULL;
  hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
  if (SUCCEEDED(hr))
  {
    hr = pDepthFrameSource->OpenReader(&m_pRawDepthFrameReader);
  }
  if (SUCCEEDED(hr))
  {
    m_bRawDepthThreadRunning = true;

    m_tsfnRawDepth = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),     // JavaScript function called asynchronously
      "Kinect2 Depth Listener",              // Name
      1,                                // 1 call in queue
      1,                                // Only one thread will use this initially
      []( Napi::Env ) {                 // Finalizer used to clean threads up
        m_tRawDepthThread.join();
        m_mRawDepthThreadJoinedMutex.unlock();
        printf("[kinect2.cc] raw depth thread joined\n");
      });

    m_mRawDepthThreadJoinedMutex.lock();
    m_tRawDepthThread = std::thread( [] {
      
      auto callback = []( Napi::Env env, Napi::Function jsCallback, UINT16* pixelsRef ) {
        if (!m_bRawDepthThreadRunning) {
          return;
        }
        m_mRawDepthReaderMutex.lock();
        Napi::Buffer<UINT16> rawDepthValuesBuffer = Napi::Buffer<UINT16>::NewOrCopy(env, m_pRawDepthValues, cDepthWidth * cDepthHeight);
        jsCallback.Call( { rawDepthValuesBuffer } );
        m_mRawDepthReaderMutex.unlock();
      };

      while(m_bRawDepthThreadRunning)
      {
        IDepthFrame* pDepthFrame = NULL;
        HRESULT hr = E_FAIL;
        m_mRawDepthReaderMutex.lock();
        hr = m_pRawDepthFrameReader->AcquireLatestFrame(&pDepthFrame);
        if (SUCCEEDED(hr))
        {
          processRawDepthFrameData(pDepthFrame);
        }
        if (!m_bRawDepthThreadRunning)
        {
          m_mRawDepthReaderMutex.unlock();
          break;
        }
        m_mRawDepthReaderMutex.unlock();
        if (SUCCEEDED(hr))
        {
          napi_status status = m_tsfnRawDepth.BlockingCall( m_pRawDepthValues, callback );
          if ( status != napi_ok )
          {
            break;
          }
        }
        SafeRelease(pDepthFrame);
      }
      m_bRawDepthThreadRunning = false;
      m_tsfnRawDepth.Release();
    } );
  }

  SafeRelease(pDepthFrameSource);
  // m_mRawDepthReaderMutex.unlock();
  if (FAILED(hr))
  {
    return Napi::Boolean::New(env, false);
  }
  return Napi::Boolean::New(env, true);
}

Napi::Value MethodCloseRawDepthReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  stopReader(&m_mRawDepthReaderMutex, &m_bRawDepthThreadRunning, &m_tRawDepthThread, m_pRawDepthFrameReader);
  Napi::Function cb = info[0].As<Napi::Function>();
  WaitForTheadJoinWorker* wk = new WaitForTheadJoinWorker(cb, &m_mRawDepthThreadJoinedMutex);
  wk->Queue();
  return info.Env().Undefined();
}

Napi::Value MethodOpenBodyReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (m_bBodyThreadRunning) {
    Napi::TypeError::New(env, "Body thread already running")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Wrong number of arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  } else if (!info[0].IsFunction()){
    throw Napi::TypeError::New( env, "Expected first arg to be function" );
  }

  HRESULT hr;
  IBodyFrameSource* pBodyFrameSource = NULL;
  hr = m_pKinectSensor->get_BodyFrameSource(&pBodyFrameSource);
  if (SUCCEEDED(hr))
  {
    hr = pBodyFrameSource->OpenReader(&m_pBodyFrameReader);
  }
  if (SUCCEEDED(hr))
  {
    m_bBodyThreadRunning = true;

    m_tsfnBody = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),     // JavaScript function called asynchronously
      "Kinect2 Body Listener",              // Name
      1,                                // 1 call in queue
      1,                                // Only one thread will use this initially
      []( Napi::Env ) {                 // Finalizer used to clean threads up
        m_tBodyThread.join();
        m_mBodyThreadJoinedMutex.unlock();
        printf("[kinect2.cc] Body thread joined\n");
      });

    m_mBodyThreadJoinedMutex.lock();
    m_tBodyThread = std::thread( [] {
      
      auto callback = []( Napi::Env env, Napi::Function jsCallback ) {
        if (!m_bBodyThreadRunning) {
          return;
        }
        m_mBodyReaderMutex.lock();

        Napi::Object v8BodyResult = getV8BodyFrame(env, &m_jsBodyFrame);

        jsCallback.Call( { v8BodyResult } );
        m_mBodyReaderMutex.unlock();
      };

      while(m_bBodyThreadRunning)
      {
        IBodyFrame* pBodyFrame = NULL;
        HRESULT hr = E_FAIL;
        m_mBodyReaderMutex.lock();
        hr = m_pBodyFrameReader->AcquireLatestFrame(&pBodyFrame);
        if (SUCCEEDED(hr))
        {
          processBodyFrameData(pBodyFrame);
        }
        if (!m_bBodyThreadRunning)
        {
          m_mBodyReaderMutex.unlock();
          break;
        }
        m_mBodyReaderMutex.unlock();
        if (SUCCEEDED(hr))
        {
          napi_status status = m_tsfnBody.BlockingCall( callback );
          if ( status != napi_ok )
          {
            break;
          }
        }
        SafeRelease(pBodyFrame);
      }
      m_bBodyThreadRunning = false;
      m_tsfnBody.Release();
    } );
  }

  SafeRelease(pBodyFrameSource);
  // m_mBodyReaderMutex.unlock();
  if (FAILED(hr))
  {
    return Napi::Boolean::New(env, false);
  }
  return Napi::Boolean::New(env, true);
}

Napi::Value MethodCloseBodyReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  stopReader(&m_mBodyReaderMutex, &m_bBodyThreadRunning, &m_tBodyThread, m_pBodyFrameReader);
  Napi::Function cb = info[0].As<Napi::Function>();
  WaitForTheadJoinWorker* wk = new WaitForTheadJoinWorker(cb, &m_mBodyThreadJoinedMutex);
  wk->Queue();
  return info.Env().Undefined();
}

Napi::Value MethodOpenMultiSourceReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (m_bMultiSourceThreadRunning) {
    Napi::TypeError::New(env, "MultiSource thread already running")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Wrong number of arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  } else if (!info[0].IsObject()){
    throw Napi::TypeError::New( env, "Expected first arg to be object" );
  }

  Napi::Object jsOptions = info[0].As<Napi::Object>();
  Napi::Function jsCallback = jsOptions.Get("callback").As<Napi::Function>();

  // m_enabledFrameTypes = static_cast<unsigned long>(jsOptions.Get("frameTypes")).As<Napi::Number>()->Int32Value());
  m_enabledFrameTypes = jsOptions.Get("frameTypes").As<Napi::Number>().Int32Value();
  printf("[kinect2.cc] m_enabledFrameTypes: %i\n", m_enabledFrameTypes);

  m_includeJointFloorData = false;

  Napi::Value v8IncludeJointFloorData = jsOptions.Get("includeJointFloorData");
  if(v8IncludeJointFloorData.IsBoolean())
  {
    m_includeJointFloorData = v8IncludeJointFloorData.As<Napi::Boolean>();
  }

  //map our own frame types to the correct frame source types
  m_enabledFrameSourceTypes = FrameSourceTypes::FrameSourceTypes_None;
  if(
    m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_Color
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexColor
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_DepthColor
  ) {
    m_enabledFrameSourceTypes |= FrameSourceTypes::FrameSourceTypes_Color;
  }
  if(
    m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_Infrared
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexInfrared
  ) {
    m_enabledFrameSourceTypes |= FrameSourceTypes::FrameSourceTypes_Infrared;
  }
  if(
    m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_LongExposureInfrared
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexLongExposureInfrared
  ) {
    m_enabledFrameSourceTypes |= FrameSourceTypes::FrameSourceTypes_LongExposureInfrared;
  }
  if(
    m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_Depth
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndex
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexColor
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexDepth
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexInfrared
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexLongExposureInfrared
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_RawDepth
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_DepthColor
  ) {
    m_enabledFrameSourceTypes |= FrameSourceTypes::FrameSourceTypes_Depth;
  }
  if(
    m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndex
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexColor
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexDepth
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexInfrared
    || m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexLongExposureInfrared
  ) {
    m_enabledFrameSourceTypes |= FrameSourceTypes::FrameSourceTypes_BodyIndex;
  }
  if(
    m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_Body
  ) {
    m_enabledFrameSourceTypes |= FrameSourceTypes::FrameSourceTypes_Body;
  }
  if(
    m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_Audio
  ) {
    m_enabledFrameSourceTypes |= FrameSourceTypes::FrameSourceTypes_Audio;
  }

  printf("[kinect2.cc] m_enabledFrameSourceTypes: %i\n", m_enabledFrameSourceTypes);

  HRESULT hr;
  hr = m_pKinectSensor->OpenMultiSourceFrameReader(m_enabledFrameSourceTypes, &m_pMultiSourceFrameReader);
  if (SUCCEEDED(hr))
  {
    m_bMultiSourceThreadRunning = true;

    m_tsfnMultiSource = Napi::ThreadSafeFunction::New(
      env,
      jsCallback,     // JavaScript function called asynchronously
      "Kinect2 MultiSource Listener",              // Name
      1,                                // 1 call in queue
      1,                                // Only one thread will use this initially
      []( Napi::Env ) {                 // Finalizer used to clean threads up
        m_tMultiSourceThread.join();
        m_mMultiSourceThreadJoinedMutex.unlock();
        printf("[kinect2.cc] MultiSource thread joined\n");
      });

    m_mMultiSourceThreadJoinedMutex.lock();
    m_tMultiSourceThread = std::thread( [] {
      
      auto callback = []( Napi::Env env, Napi::Function jsCallback ) {
        if (!m_bMultiSourceThreadRunning) {
          return;
        }
        m_mMultiSourceReaderMutex.lock();

        //build object for callback
        Napi::Object v8Result = Napi::Object::New(env);

        if(NodeKinect2FrameTypes::FrameTypes_Color & m_enabledFrameTypes)
        {
          Napi::Object v8ColorResult = Napi::Object::New(env);
          Napi::Buffer<RGBQUAD> colorBuffer = Napi::Buffer<RGBQUAD>::NewOrCopy(env, m_pColorPixels, cColorWidth * cColorHeight);

          v8ColorResult.Set(Napi::String::New(env, "buffer"), colorBuffer);

          //field of view
          v8ColorResult.Set(Napi::String::New(env, "horizontalFieldOfView"), Napi::Number::New(env, m_fColorHorizontalFieldOfView));
          v8ColorResult.Set(Napi::String::New(env, "verticalFieldOfView"), Napi::Number::New(env, m_fColorVerticalFieldOfView));
          v8ColorResult.Set(Napi::String::New(env, "diagonalFieldOfView"), Napi::Number::New(env, m_fColorDiagonalFieldOfView));

          v8Result.Set(Napi::String::New(env, "color"), v8ColorResult);
        }

        if(NodeKinect2FrameTypes::FrameTypes_Depth & m_enabledFrameTypes)
        {
          Napi::Object v8DepthResult = Napi::Object::New(env);
          Napi::Buffer<char> depthPixelsBuffer = Napi::Buffer<char>::NewOrCopy(env, m_pDepthPixels, cDepthWidth * cDepthHeight);
          v8DepthResult.Set(Napi::String::New(env, "buffer"), depthPixelsBuffer);

          //field of view
          v8DepthResult.Set(Napi::String::New(env, "horizontalFieldOfView"), Napi::Number::New(env, m_fDepthHorizontalFieldOfView));
          v8DepthResult.Set(Napi::String::New(env, "verticalFieldOfView"), Napi::Number::New(env, m_fDepthVerticalFieldOfView));
          v8DepthResult.Set(Napi::String::New(env, "diagonalFieldOfView"), Napi::Number::New(env, m_fDepthDiagonalFieldOfView));

          v8Result.Set(Napi::String::New(env, "depth"), v8DepthResult);
        }

        if(NodeKinect2FrameTypes::FrameTypes_Infrared & m_enabledFrameTypes)
        {
          Napi::Object v8InfraredResult = Napi::Object::New(env);
          Napi::Buffer<char> infraredBuffer = Napi::Buffer<char>::NewOrCopy(env, m_pInfraredPixels, cInfraredWidth * cInfraredHeight);
          v8InfraredResult.Set(Napi::String::New(env, "buffer"), infraredBuffer);

          //field of view
          v8InfraredResult.Set(Napi::String::New(env, "horizontalFieldOfView"), Napi::Number::New(env, m_fDepthHorizontalFieldOfView));
          v8InfraredResult.Set(Napi::String::New(env, "verticalFieldOfView"), Napi::Number::New(env, m_fDepthVerticalFieldOfView));
          v8InfraredResult.Set(Napi::String::New(env, "diagonalFieldOfView"), Napi::Number::New(env, m_fDepthDiagonalFieldOfView));

          v8Result.Set(Napi::String::New(env, "infrared"), v8InfraredResult);
        }

        if(NodeKinect2FrameTypes::FrameTypes_LongExposureInfrared & m_enabledFrameTypes)
        {
          Napi::Object v8LongExposureInfraredResult = Napi::Object::New(env);
          Napi::Buffer<char> longExposureInfraredBuffer = Napi::Buffer<char>::NewOrCopy(env, m_pLongExposureInfraredPixels, cLongExposureInfraredWidth * cLongExposureInfraredHeight);
          v8LongExposureInfraredResult.Set(Napi::String::New(env, "buffer"), longExposureInfraredBuffer);

          //field of view
          v8LongExposureInfraredResult.Set(Napi::String::New(env, "horizontalFieldOfView"), Napi::Number::New(env, m_fDepthHorizontalFieldOfView));
          v8LongExposureInfraredResult.Set(Napi::String::New(env, "verticalFieldOfView"), Napi::Number::New(env, m_fDepthVerticalFieldOfView));
          v8LongExposureInfraredResult.Set(Napi::String::New(env, "diagonalFieldOfView"), Napi::Number::New(env, m_fDepthDiagonalFieldOfView));

          v8Result.Set(Napi::String::New(env, "longExposureInfrared"), v8LongExposureInfraredResult);
        }

        if(NodeKinect2FrameTypes::FrameTypes_RawDepth & m_enabledFrameTypes)
        {
          Napi::Object v8RawDepthResult = Napi::Object::New(env);
          Napi::Buffer<UINT16> rawDepthValuesBuffer = Napi::Buffer<UINT16>::NewOrCopy(env, m_pRawDepthValues, cDepthWidth * cDepthHeight);
          v8RawDepthResult.Set(Napi::String::New(env, "buffer"), rawDepthValuesBuffer);

          //field of view
          v8RawDepthResult.Set(Napi::String::New(env, "horizontalFieldOfView"), Napi::Number::New(env, m_fDepthHorizontalFieldOfView));
          v8RawDepthResult.Set(Napi::String::New(env, "verticalFieldOfView"), Napi::Number::New(env, m_fDepthVerticalFieldOfView));
          v8RawDepthResult.Set(Napi::String::New(env, "diagonalFieldOfView"), Napi::Number::New(env, m_fDepthDiagonalFieldOfView));

          v8Result.Set(Napi::String::New(env, "rawDepth"), v8RawDepthResult);
        }

        if(NodeKinect2FrameTypes::FrameTypes_DepthColor & m_enabledFrameTypes)
        {
          Napi::Object v8DepthColorResult = Napi::Object::New(env);
          Napi::Buffer<char> depthColorPixelsBuffer = Napi::Buffer<char>::NewOrCopy(env, (char *)m_pDepthColorPixels, cDepthWidth * cDepthHeight * sizeof(RGBQUAD));
          v8DepthColorResult.Set(Napi::String::New(env, "buffer"), depthColorPixelsBuffer);

          v8Result.Set(Napi::String::New(env, "depthColor"), v8DepthColorResult);
        }

        if(NodeKinect2FrameTypes::FrameTypes_Body & m_enabledFrameTypes)
        {
          Napi::Object v8BodyResult = getV8BodyFrame(env, &m_jsBodyFrame);

          v8Result.Set(Napi::String::New(env, "body"), v8BodyResult);
        }

        if(NodeKinect2FrameTypes::FrameTypes_BodyIndexColor & m_enabledFrameTypes)
        {
          Napi::Object v8BodyIndexColorResult = Napi::Object::New(env);

          Napi::Array v8bodies = Napi::Array::New(env, BODY_COUNT);

          Napi::Array bodyIndexColorPixelsBuffers = Napi::Array::New(env, BODY_COUNT);
          for(int i = 0; i < BODY_COUNT; i++)
          {
            Napi::Object v8body = Napi::Object::New(env);
            v8body.Set(Napi::String::New(env, "bodyIndex"), Napi::Number::New(env, i));
            if(m_jsBodyFrame.bodies[i].hasPixels && m_trackPixelsForBodyIndexV8[i]) {
              Napi::Buffer<RGBQUAD> bodyIndexColorPixelsBuffer = Napi::Buffer<RGBQUAD>::NewOrCopy(env, m_pBodyIndexColorPixels[i], cColorWidth * cColorHeight);
              v8body.Set(Napi::String::New(env, "buffer"), bodyIndexColorPixelsBuffer);
            }
            v8bodies.Set(i, v8body);
          }
          v8BodyIndexColorResult.Set(Napi::String::New(env, "bodies"), v8bodies);

          //field of view pf color camera
          v8BodyIndexColorResult.Set(Napi::String::New(env, "horizontalFieldOfView"), Napi::Number::New(env, m_fColorHorizontalFieldOfView));
          v8BodyIndexColorResult.Set(Napi::String::New(env, "verticalFieldOfView"), Napi::Number::New(env, m_fColorVerticalFieldOfView));
          v8BodyIndexColorResult.Set(Napi::String::New(env, "diagonalFieldOfView"), Napi::Number::New(env, m_fColorDiagonalFieldOfView));

          v8Result.Set(Napi::String::New(env, "bodyIndexColor"), v8BodyIndexColorResult);
        }

        jsCallback.Call( { v8Result } );
        m_mMultiSourceReaderMutex.unlock();
      };

      while(m_bMultiSourceThreadRunning)
      {
        IMultiSourceFrame* pMultiSourceFrame = NULL;
        HRESULT hr = E_FAIL;
        m_mMultiSourceReaderMutex.lock();
        hr = m_pMultiSourceFrameReader->AcquireLatestFrame(&pMultiSourceFrame);
        if(SUCCEEDED(hr)) {
          //get the frames
          IColorFrameReference* pColorFrameReference = NULL;
          IColorFrame* pColorFrame = NULL;

          IInfraredFrameReference* pInfraredFrameReference = NULL;
          IInfraredFrame* pInfraredFrame = NULL;

          ILongExposureInfraredFrameReference* pLongExposureInfraredFrameReference = NULL;
          ILongExposureInfraredFrame* pLongExposureInfraredFrame = NULL;

          IDepthFrameReference* pDepthFrameReference = NULL;
          IDepthFrame* pDepthFrame = NULL;
          UINT nDepthBufferSize = 0;
          UINT16 *pDepthBuffer = NULL;

          IBodyIndexFrameReference* pBodyIndexFrameReference = NULL;
          IBodyIndexFrame* pBodyIndexFrame = NULL;
          IFrameDescription* pBodyIndexFrameDescription = NULL;
          UINT nBodyIndexBufferSize = 0;
          BYTE *pBodyIndexBuffer = NULL;
          for(int i = 0; i < BODY_COUNT; i++)
          {
            m_jsBodyFrame.bodies[i].hasPixels = false;
          }

          IBodyFrameReference* pBodyFrameReference = NULL;
          IBodyFrame* pBodyFrame = NULL;

          if(SUCCEEDED(hr) && FrameSourceTypes::FrameSourceTypes_Color & m_enabledFrameSourceTypes)
          {
            hr = pMultiSourceFrame->get_ColorFrameReference(&pColorFrameReference);
            if (SUCCEEDED(hr))
            {
              hr = pColorFrameReference->AcquireFrame(&pColorFrame);
            }
            if (SUCCEEDED(hr) && (
              NodeKinect2FrameTypes::FrameTypes_Color & m_enabledFrameTypes ||
              NodeKinect2FrameTypes::FrameTypes_BodyIndexColor & m_enabledFrameTypes ||
              NodeKinect2FrameTypes::FrameTypes_DepthColor & m_enabledFrameTypes
            ))
            {
              hr = processColorFrameData(pColorFrame);
            }
          }
          if(SUCCEEDED(hr) && FrameSourceTypes::FrameSourceTypes_Infrared & m_enabledFrameSourceTypes)
          {
            hr = pMultiSourceFrame->get_InfraredFrameReference(&pInfraredFrameReference);
            if (SUCCEEDED(hr))
            {
              hr = pInfraredFrameReference->AcquireFrame(&pInfraredFrame);
            }
            if (SUCCEEDED(hr) && NodeKinect2FrameTypes::FrameTypes_Infrared & m_enabledFrameTypes)
            {
              hr = processInfraredFrameData(pInfraredFrame);
            }
          }
          if(SUCCEEDED(hr) && FrameSourceTypes::FrameSourceTypes_LongExposureInfrared & m_enabledFrameSourceTypes)
          {
            hr = pMultiSourceFrame->get_LongExposureInfraredFrameReference(&pLongExposureInfraredFrameReference);
            if (SUCCEEDED(hr))
            {
              hr = pLongExposureInfraredFrameReference->AcquireFrame(&pLongExposureInfraredFrame);
            }
            if (SUCCEEDED(hr) && NodeKinect2FrameTypes::FrameTypes_LongExposureInfrared & m_enabledFrameTypes)
            {
              hr = processLongExposureInfraredFrameData(pLongExposureInfraredFrame);
            }
          }
          if(SUCCEEDED(hr) && FrameSourceTypes::FrameSourceTypes_Depth & m_enabledFrameSourceTypes)
          {
            hr = pMultiSourceFrame->get_DepthFrameReference(&pDepthFrameReference);
            if (SUCCEEDED(hr))
            {
              hr = pDepthFrameReference->AcquireFrame(&pDepthFrame);
            }
            if (SUCCEEDED(hr) && NodeKinect2FrameTypes::FrameTypes_Depth & m_enabledFrameTypes)
            {
              hr = processDepthFrameData(pDepthFrame);
            }
            if (SUCCEEDED(hr) && (
              NodeKinect2FrameTypes::FrameTypes_RawDepth & m_enabledFrameTypes ||
              NodeKinect2FrameTypes::FrameTypes_DepthColor & m_enabledFrameTypes
            ))
            {
              hr = processRawDepthFrameData(pDepthFrame);
            }
            if (SUCCEEDED(hr))
            {
              hr = pDepthFrame->AccessUnderlyingBuffer(&nDepthBufferSize, &pDepthBuffer);
            }
          }
          if(SUCCEEDED(hr) && FrameSourceTypes::FrameSourceTypes_BodyIndex & m_enabledFrameSourceTypes)
          {
            hr = pMultiSourceFrame->get_BodyIndexFrameReference(&pBodyIndexFrameReference);
            if (SUCCEEDED(hr))
            {
              hr = pBodyIndexFrameReference->AcquireFrame(&pBodyIndexFrame);
            }
            if (SUCCEEDED(hr))
            {
              hr = pBodyIndexFrame->get_FrameDescription(&pBodyIndexFrameDescription);
            }
            if (SUCCEEDED(hr))
            {
              hr = pBodyIndexFrame->AccessUnderlyingBuffer(&nBodyIndexBufferSize, &pBodyIndexBuffer);
            }
          }
          if(SUCCEEDED(hr) && FrameSourceTypes::FrameSourceTypes_Body & m_enabledFrameSourceTypes)
          {
            hr = pMultiSourceFrame->get_BodyFrameReference(&pBodyFrameReference);
            if (SUCCEEDED(hr))
            {
              hr = pBodyFrameReference->AcquireFrame(&pBodyFrame);
            }
            if(SUCCEEDED(hr))
            {
              hr = processBodyFrameData(pBodyFrame);
            }
          }

          //additional data processing combining sources
          /*
          FrameTypes_None  = 0,
          FrameTypes_Color  = 0x1,
          FrameTypes_Infrared  = 0x2,
          FrameTypes_LongExposureInfrared  = 0x4,
          FrameTypes_Depth  = 0x8,
          FrameTypes_BodyIndex  = 0x10,
          FrameTypes_Body  = 0x20,
          FrameTypes_Audio  = 0x40,
          FrameTypes_BodyIndexColor  = 0x80,
          FrameTypes_BodyIndexDepth  = 0x10,
          FrameTypes_BodyIndexInfrared  = 0x100,
          FrameTypes_BodyIndexLongExposureInfrared  = 0x200,
          FrameTypes_RawDepth  = 0x400
          FrameTypes_DepthColor  = 0x800
          */

          //we do something with color data
          if(SUCCEEDED(hr) && FrameSourceTypes::FrameSourceTypes_Color & m_enabledFrameSourceTypes)
          {
            //what are we doing with the color data?
            //we could have color and / or bodyindexcolor and / or depthcolor
            if(SUCCEEDED(hr) && NodeKinect2FrameTypes::FrameTypes_BodyIndexColor & m_enabledFrameTypes)
            {
              hr = m_pCoordinateMapper->MapColorFrameToDepthSpace(cDepthWidth * cDepthHeight, (UINT16*)pDepthBuffer, cColorWidth * cColorHeight, m_pDepthCoordinatesForColor);
              if (SUCCEEDED(hr))
              {
                //default transparent colors
                for( int i = 0 ; i < BODY_COUNT ; i++ ) {
                  memset(m_pBodyIndexColorPixels[i], 0, cColorWidth * cColorHeight * sizeof(RGBQUAD));
                }

                for (int colorIndex = 0; colorIndex < (cColorWidth*cColorHeight); ++colorIndex)
                {

                  DepthSpacePoint p = m_pDepthCoordinatesForColor[colorIndex];

                  // Values that are negative infinity means it is an invalid color to depth mapping so we
                  // skip processing for this pixel
                  if (p.X != -std::numeric_limits<float>::infinity() && p.Y != -std::numeric_limits<float>::infinity())
                  {
                    int depthX = static_cast<int>(p.X + 0.5f);
                    int depthY = static_cast<int>(p.Y + 0.5f);
                    if ((depthX >= 0 && depthX < cDepthWidth) && (depthY >= 0 && depthY < cDepthHeight))
                    {
                      BYTE player = pBodyIndexBuffer[depthX + (depthY * cDepthWidth)];

                      // if we're tracking a player for the current pixel, draw from the color camera
                      if (player != 0xff)
                      {
                        m_jsBodyFrame.bodies[player].hasPixels = true;
                        // set source for copy to the color pixel
                        m_pBodyIndexColorPixels[player][colorIndex] = m_pColorPixels[colorIndex];
                      }
                    }
                  }
                }
              }
            }

            if(SUCCEEDED(hr) && NodeKinect2FrameTypes::FrameTypes_DepthColor & m_enabledFrameTypes)
            {
              hr = m_pCoordinateMapper->MapDepthFrameToColorSpace(cDepthWidth * cDepthHeight, (UINT16*)pDepthBuffer, cDepthWidth * cDepthHeight, m_pColorCoordinatesForDepth);
              if (SUCCEEDED(hr))
              {
                //default transparent colors
                memset(m_pDepthColorPixels, 0, cDepthWidth * cDepthHeight * sizeof(RGBQUAD));

                for (int depthIndex = 0; depthIndex < (cDepthWidth*cDepthHeight); ++depthIndex)
                {

                  ColorSpacePoint p = m_pColorCoordinatesForDepth[depthIndex];

                  // Values that are negative infinity means it is an invalid color to depth mapping so we
                  // skip processing for this pixel
                  if (p.X != -std::numeric_limits<float>::infinity() && p.Y != -std::numeric_limits<float>::infinity())
                  {
                    int colorX = static_cast<int>( std::floor( p.X + 0.5f ) );
                    int colorY = static_cast<int>( std::floor( p.Y + 0.5f ) );
                    if( ( 0 <= colorX ) && ( colorX < cColorWidth ) && ( 0 <= colorY ) && ( colorY < cColorHeight ) ){
                      m_pDepthColorPixels[depthIndex] = m_pColorPixels[colorY * cColorWidth + colorX];
                    }
                  }
                }
              }
            }
          }

          //release memory
          SafeRelease(pColorFrame);
          SafeRelease(pColorFrameReference);

          SafeRelease(pInfraredFrame);
          SafeRelease(pInfraredFrameReference);

          SafeRelease(pLongExposureInfraredFrame);
          SafeRelease(pLongExposureInfraredFrameReference);

          SafeRelease(pDepthFrame);
          SafeRelease(pDepthFrameReference);

          SafeRelease(pBodyIndexFrameDescription);
          SafeRelease(pBodyIndexFrame);
          SafeRelease(pBodyIndexFrameReference);

          SafeRelease(pBodyFrame);
          SafeRelease(pBodyFrameReference);
        }
        if (!m_bMultiSourceThreadRunning)
        {
          m_mMultiSourceReaderMutex.unlock();
          break;
        }
        m_mMultiSourceReaderMutex.unlock();
        if (SUCCEEDED(hr))
        {
          napi_status status = m_tsfnMultiSource.BlockingCall( callback );
          if ( status != napi_ok )
          {
            break;
          }
        }
        SafeRelease(pMultiSourceFrame);
      }
      m_bMultiSourceThreadRunning = false;
      m_tsfnMultiSource.Release();
    } );
  }

  // m_mMultiSourceReaderMutex.unlock();
  if (FAILED(hr))
  {
    return Napi::Boolean::New(env, false);
  }
  return Napi::Boolean::New(env, true);
}

Napi::Value MethodCloseMultiSourceReader(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  stopReader(&m_mMultiSourceReaderMutex, &m_bMultiSourceThreadRunning, &m_tMultiSourceThread, m_pMultiSourceFrameReader);
  Napi::Function cb = info[0].As<Napi::Function>();
  WaitForTheadJoinWorker* wk = new WaitForTheadJoinWorker(cb, &m_mMultiSourceThreadJoinedMutex);
  wk->Queue();
  return info.Env().Undefined();
}

Napi::Value MethodTrackPixelsForBodyIndices(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Wrong number of arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value jsOptions = info[0].As<Napi::Value>();

  int i;
  for(i = 0; i < BODY_COUNT; i++)
  {
    m_trackPixelsForBodyIndexV8[i] = false;
  }

  if (jsOptions.IsArray())
  {
    Napi::Array jsBodyIndices = info[0].As<Napi::Array>();
    int len = jsBodyIndices.Length();
    for(i = 0; i < len; i++)
    {
      // uint32_t index = static_cast<uint32_t>(jsBodyIndices->Get(i).As<Number>()->Value());
      uint32_t index = jsBodyIndices.Get(i).As<Napi::Number>().Int32Value();
      index = min(index, BODY_COUNT - 1);
      m_trackPixelsForBodyIndexV8[index] = true;
    }
  }

  return info.Env().Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  printf("[kinect2.cc] Init\n");
  exports.Set(Napi::String::New(env, "open"), Napi::Function::New(env, MethodOpen));
  exports.Set(Napi::String::New(env, "close"), Napi::Function::New(env, MethodClose));

  exports.Set(Napi::String::New(env, "openColorReader"), Napi::Function::New(env, MethodOpenColorReader));
  exports.Set(Napi::String::New(env, "closeColorReader"), Napi::Function::New(env, MethodCloseColorReader));
  
  exports.Set(Napi::String::New(env, "openInfraredReader"), Napi::Function::New(env, MethodOpenInfraredReader));
  exports.Set(Napi::String::New(env, "closeInfraredReader"), Napi::Function::New(env, MethodCloseInfraredReader));
  
  exports.Set(Napi::String::New(env, "openLongExposureInfraredReader"), Napi::Function::New(env, MethodOpenLongExposureInfraredReader));
  exports.Set(Napi::String::New(env, "closeLongExposureInfraredReader"), Napi::Function::New(env, MethodCloseLongExposureInfraredReader));
  
  exports.Set(Napi::String::New(env, "openDepthReader"), Napi::Function::New(env, MethodOpenDepthReader));
  exports.Set(Napi::String::New(env, "closeDepthReader"), Napi::Function::New(env, MethodCloseDepthReader));

  exports.Set(Napi::String::New(env, "openRawDepthReader"), Napi::Function::New(env, MethodOpenRawDepthReader));
  exports.Set(Napi::String::New(env, "closeRawDepthReader"), Napi::Function::New(env, MethodCloseRawDepthReader));

  exports.Set(Napi::String::New(env, "openBodyReader"), Napi::Function::New(env, MethodOpenBodyReader));
  exports.Set(Napi::String::New(env, "closeBodyReader"), Napi::Function::New(env, MethodCloseBodyReader));

  exports.Set(Napi::String::New(env, "openMultiSourceReader"), Napi::Function::New(env, MethodOpenMultiSourceReader));
  exports.Set(Napi::String::New(env, "closeMultiSourceReader"), Napi::Function::New(env, MethodCloseMultiSourceReader));

  exports.Set(Napi::String::New(env, "trackPixelsForBodyIndices"), Napi::Function::New(env, MethodTrackPixelsForBodyIndices));

  return exports;
}

NODE_API_MODULE(hello, Init)