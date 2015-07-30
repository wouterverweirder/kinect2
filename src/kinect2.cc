#include <nan.h>
#include <iostream>
#include <uv.h>
#include "Kinect.h"
#include "Globals.h"
#include "Structs.h"

using namespace v8;

IKinectSensor*											m_pKinectSensor;
ICoordinateMapper*									m_pCoordinateMapper;
IColorFrameReader*  								m_pColorFrameReader;
IInfraredFrameReader* 							m_pInfraredFrameReader;
ILongExposureInfraredFrameReader* 	m_pLongExposureInfraredFrameReader;
IDepthFrameReader*									m_pDepthFrameReader;
IBodyFrameReader*										m_pBodyFrameReader;
IMultiSourceFrameReader*						m_pMultiSourceFrameReader;

RGBQUAD*								m_pColorPixels = new RGBQUAD[cColorWidth * cColorHeight];
char*										m_pInfraredPixels = new char[cInfraredWidth * cInfraredHeight];
char*										m_pLongExposureInfraredPixels = new char[cLongExposureInfraredWidth * cLongExposureInfraredHeight];
char*										m_pDepthPixels = new char[cDepthWidth * cDepthHeight];
JSBodyFrame							m_jsBodyFrame;
int 										numTrackedBodies = 0;

DepthSpacePoint*				m_pDepthCoordinatesForColor = new DepthSpacePoint[cColorWidth * cColorHeight];

//enabledFrameSourceTypes refers to the kinect SDK frame source types
DWORD										m_enabledFrameSourceTypes = 0;
//enabledFrameTypes is our own list of frame types
//this is the kinect SDK list + additional ones (body index in color space, etc...)
unsigned long						m_enabledFrameTypes = 0;

NanCallback*						m_pColorReaderCallback;
NanCallback*						m_pInfraredReaderCallback;
NanCallback*						m_pLongExposureInfraredReaderCallback;
NanCallback*						m_pDepthReaderCallback;
NanCallback*						m_pBodyReaderCallback;
NanCallback*						m_pMultiSourceReaderCallback;

uv_mutex_t							m_mColorReaderMutex;
uv_mutex_t							m_mInfraredReaderMutex;
uv_mutex_t							m_mLongExposureInfraredReaderMutex;
uv_mutex_t							m_mDepthReaderMutex;
uv_mutex_t							m_mBodyReaderMutex;
uv_mutex_t							m_mMultiSourceReaderMutex;

uv_async_t							m_aColorAsync;
uv_thread_t							m_tColorThread;
bool 										m_bColorThreadRunning = false;
Persistent<Object>			m_persistentColorPixels;

uv_async_t							m_aInfraredAsync;
uv_thread_t							m_tInfraredThread;
bool 										m_bInfraredThreadRunning = false;
Persistent<Object>			m_persistentInfraredPixels;

uv_async_t							m_aLongExposureInfraredAsync;
uv_thread_t							m_tLongExposureInfraredThread;
bool 										m_bLongExposureInfraredThreadRunning = false;
Persistent<Object>			m_persistentLongExposureInfraredPixels;

uv_async_t							m_aDepthAsync;
uv_thread_t							m_tDepthThread;
bool 										m_bDepthThreadRunning = false;
Persistent<Object>			m_persistentDepthPixels;

uv_async_t							m_aBodyAsync;
uv_thread_t							m_tBodyThread;
bool 										m_bBodyThreadRunning = false;

uv_async_t							m_aMultiSourceAsync;
uv_thread_t							m_tMultiSourceThread;
bool 										m_bMultiSourceThreadRunning = false;
Persistent<Object>			m_persistentBodyIndexColorPixels;

NAN_METHOD(OpenFunction)
{
	NanScope();

	HRESULT hr;
	hr = GetDefaultKinectSensor(&m_pKinectSensor);
	if (FAILED(hr))
	{
		NanReturnValue(NanFalse());
	}

	if (m_pKinectSensor)
	{
		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->get_CoordinateMapper(&m_pCoordinateMapper);
		}

		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->Open();
		}
	}

	if (!m_pKinectSensor || FAILED(hr))
	{
		NanReturnValue(NanFalse());
	}
	NanReturnValue(NanTrue());
}

NAN_METHOD(CloseFunction)
{
	NanScope();

	//mutex locking
	uv_mutex_lock(&m_mColorReaderMutex);
	uv_mutex_lock(&m_mInfraredReaderMutex);
	uv_mutex_lock(&m_mLongExposureInfraredReaderMutex);
	uv_mutex_lock(&m_mDepthReaderMutex);
	uv_mutex_lock(&m_mBodyReaderMutex);
	uv_mutex_lock(&m_mMultiSourceReaderMutex);

	//release instances
	SafeRelease(m_pColorFrameReader);
	SafeRelease(m_pInfraredFrameReader);
	SafeRelease(m_pLongExposureInfraredFrameReader);
	SafeRelease(m_pDepthFrameReader);
	SafeRelease(m_pBodyFrameReader);
	SafeRelease(m_pMultiSourceFrameReader);

	// done with coordinate mapper
	SafeRelease(m_pCoordinateMapper);

	// close the Kinect Sensor
	if (m_pKinectSensor)
	{
		m_pKinectSensor->Close();
	}

	SafeRelease(m_pKinectSensor);

	//unlock mutexes
	uv_mutex_unlock(&m_mColorReaderMutex);
	uv_mutex_unlock(&m_mInfraredReaderMutex);
	uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);
	uv_mutex_unlock(&m_mDepthReaderMutex);
	uv_mutex_unlock(&m_mBodyReaderMutex);
	uv_mutex_unlock(&m_mMultiSourceReaderMutex);

	NanReturnValue(NanTrue());
}

NAUV_WORK_CB(ColorProgress_) {
	uv_mutex_lock(&m_mColorReaderMutex);
	NanScope();
	if(m_pColorReaderCallback != NULL)
	{
		//reuse the existing buffer
		v8::Local<v8::Object> v8ColorPixels = NanNew(m_persistentColorPixels);
		char* data = node::Buffer::Data(v8ColorPixels);
		memcpy(data, m_pColorPixels, cColorWidth * cColorHeight * sizeof(RGBQUAD));

		Local<Value> argv[] = {
			v8ColorPixels
		};
		m_pColorReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mColorReaderMutex);
}

void ColorReaderThreadLoop(void *arg)
{
	while(1)
	{
		uv_mutex_lock(&m_mColorReaderMutex);
		if(!m_bColorThreadRunning)
		{
			//make sure to unlock the mutex
			uv_mutex_unlock(&m_mColorReaderMutex);
			break;
		}

		IColorFrame* pColorFrame = NULL;
		IFrameDescription* pColorFrameDescription = NULL;
		int nColorWidth = 0;
		int nColorHeight = 0;
		ColorImageFormat imageFormat = ColorImageFormat_None;
		UINT nColorBufferSize = 0;
		RGBQUAD *pColorBuffer = NULL;

		HRESULT hr;

		hr = m_pColorFrameReader->AcquireLatestFrame(&pColorFrame);

		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_FrameDescription(&pColorFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameDescription->get_Width(&nColorWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameDescription->get_Height(&nColorHeight);
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
				if (SUCCEEDED(hr))
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
		if (SUCCEEDED(hr))
		{
			//notify event loop
			uv_async_send(&m_aColorAsync);
		}
		SafeRelease(pColorFrameDescription);
		SafeRelease(pColorFrame);
		uv_mutex_unlock(&m_mColorReaderMutex);
	}
}


NAN_METHOD(OpenColorReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mColorReaderMutex);
	if(m_pColorReaderCallback)
	{
		delete m_pColorReaderCallback;
		m_pColorReaderCallback = NULL;
	}

	m_pColorReaderCallback = new NanCallback(args[0].As<Function>());

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
		uv_thread_create(&m_tColorThread, ColorReaderThreadLoop, NULL);
	}

	SafeRelease(pColorFrameSource);

	uv_mutex_unlock(&m_mColorReaderMutex);
	if (SUCCEEDED(hr))
	{
		NanReturnValue(NanTrue());
	}
	else
	{
		NanReturnValue(NanFalse());
	}
}

NAN_METHOD(CloseColorReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mColorReaderMutex);
	m_bColorThreadRunning = false;
	SafeRelease(m_pColorFrameReader);
	uv_mutex_unlock(&m_mColorReaderMutex);

	NanReturnValue(NanTrue());
}

NAUV_WORK_CB(InfraredProgress_) {
	uv_mutex_lock(&m_mInfraredReaderMutex);
	NanScope();
	if(m_pInfraredReaderCallback != NULL)
	{
		//reuse the existing buffer
		v8::Local<v8::Object> v8InfraredPixels = NanNew(m_persistentInfraredPixels);
		char* data = node::Buffer::Data(v8InfraredPixels);
		memcpy(data, m_pInfraredPixels, cInfraredWidth * cInfraredHeight);

		Local<Value> argv[] = {
			v8InfraredPixels
		};
		m_pInfraredReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mInfraredReaderMutex);
}

void InfraredReaderThreadLoop(void *arg)
{
	while(1)
	{
		uv_mutex_lock(&m_mInfraredReaderMutex);
		if(!m_bInfraredThreadRunning)
		{
			//make sure to unlock the mutex
			uv_mutex_unlock(&m_mInfraredReaderMutex);
			break;
		}

		IInfraredFrame* pInfraredFrame = NULL;
		IFrameDescription* pInfraredFrameDescription = NULL;
		int nInfraredWidth = 0;
		int nInfraredHeight = 0;
		UINT nInfraredBufferSize = 0;
		UINT16 *pInfraredBuffer = NULL;

		HRESULT hr;

		hr = m_pInfraredFrameReader->AcquireLatestFrame(&pInfraredFrame);

		if (SUCCEEDED(hr))
		{
			hr = pInfraredFrame->get_FrameDescription(&pInfraredFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pInfraredFrameDescription->get_Width(&nInfraredWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pInfraredFrameDescription->get_Height(&nInfraredHeight);
		}

		if (SUCCEEDED(hr))
		{
			hr = pInfraredFrame->AccessUnderlyingBuffer(&nInfraredBufferSize, &pInfraredBuffer);
		}

		if (SUCCEEDED(hr))
		{
			if (m_pInfraredPixels && pInfraredBuffer && (nInfraredWidth == cInfraredWidth) && (nInfraredHeight == cInfraredHeight))
			{
				char* pDest = m_pInfraredPixels;

				// end pixel is start + width*height - 1
				const UINT16* pInfraredBufferEnd = pInfraredBuffer + (nInfraredWidth * nInfraredHeight);

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
		if (SUCCEEDED(hr))
		{
			//notify event loop
			uv_async_send(&m_aInfraredAsync);
		}
		SafeRelease(pInfraredFrameDescription);
		SafeRelease(pInfraredFrame);
		uv_mutex_unlock(&m_mInfraredReaderMutex);
	}
}


NAN_METHOD(OpenInfraredReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mInfraredReaderMutex);
	if(m_pInfraredReaderCallback)
	{
		delete m_pInfraredReaderCallback;
		m_pInfraredReaderCallback = NULL;
	}

	m_pInfraredReaderCallback = new NanCallback(args[0].As<Function>());

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
		uv_thread_create(&m_tInfraredThread, InfraredReaderThreadLoop, NULL);
	}

	SafeRelease(pInfraredFrameSource);

	uv_mutex_unlock(&m_mInfraredReaderMutex);
	if (SUCCEEDED(hr))
	{
		NanReturnValue(NanTrue());
	}
	else
	{
		NanReturnValue(NanFalse());
	}
}

NAN_METHOD(CloseInfraredReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mInfraredReaderMutex);
	m_bInfraredThreadRunning = false;
	SafeRelease(m_pInfraredFrameReader);
	uv_mutex_unlock(&m_mInfraredReaderMutex);

	NanReturnValue(NanTrue());
}

NAUV_WORK_CB(LongExposureInfraredProgress_) {
	uv_mutex_lock(&m_mLongExposureInfraredReaderMutex);
	NanScope();
	if(m_pLongExposureInfraredReaderCallback != NULL)
	{
		//reuse the existing buffer
		v8::Local<v8::Object> v8LongExposureInfraredPixels = NanNew(m_persistentLongExposureInfraredPixels);
		char* data = node::Buffer::Data(v8LongExposureInfraredPixels);
		memcpy(data, m_pLongExposureInfraredPixels, cLongExposureInfraredWidth * cLongExposureInfraredHeight);

		Local<Value> argv[] = {
			v8LongExposureInfraredPixels
		};
		m_pLongExposureInfraredReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);
}

void LongExposureInfraredReaderThreadLoop(void *arg)
{
	while(1)
	{
		uv_mutex_lock(&m_mLongExposureInfraredReaderMutex);
		if(!m_bLongExposureInfraredThreadRunning)
		{
			//make sure to unlock the mutex
			uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);
			break;
		}

		ILongExposureInfraredFrame* pLongExposureInfraredFrame = NULL;
		IFrameDescription* pLongExposureInfraredFrameDescription = NULL;
		int nLongExposureInfraredWidth = 0;
		int nLongExposureInfraredHeight = 0;
		UINT nLongExposureInfraredBufferSize = 0;
		UINT16 *pLongExposureInfraredBuffer = NULL;

		HRESULT hr;

		hr = m_pLongExposureInfraredFrameReader->AcquireLatestFrame(&pLongExposureInfraredFrame);

		if (SUCCEEDED(hr))
		{
			hr = pLongExposureInfraredFrame->get_FrameDescription(&pLongExposureInfraredFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pLongExposureInfraredFrameDescription->get_Width(&nLongExposureInfraredWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pLongExposureInfraredFrameDescription->get_Height(&nLongExposureInfraredHeight);
		}

		if (SUCCEEDED(hr))
		{
			hr = pLongExposureInfraredFrame->AccessUnderlyingBuffer(&nLongExposureInfraredBufferSize, &pLongExposureInfraredBuffer);
		}

		if (SUCCEEDED(hr))
		{
			if (m_pLongExposureInfraredPixels && pLongExposureInfraredBuffer && (nLongExposureInfraredWidth == cLongExposureInfraredWidth) && (nLongExposureInfraredHeight == cLongExposureInfraredHeight))
			{
				char* pDest = m_pLongExposureInfraredPixels;

				// end pixel is start + width*height - 1
				const UINT16* pLongExposureInfraredBufferEnd = pLongExposureInfraredBuffer + (nLongExposureInfraredWidth * nLongExposureInfraredHeight);

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
		if (SUCCEEDED(hr))
		{
			//notify event loop
			uv_async_send(&m_aLongExposureInfraredAsync);
		}
		SafeRelease(pLongExposureInfraredFrameDescription);
		SafeRelease(pLongExposureInfraredFrame);
		uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);
	}
}


NAN_METHOD(OpenLongExposureInfraredReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mLongExposureInfraredReaderMutex);
	if(m_pLongExposureInfraredReaderCallback)
	{
		delete m_pLongExposureInfraredReaderCallback;
		m_pLongExposureInfraredReaderCallback = NULL;
	}

	m_pLongExposureInfraredReaderCallback = new NanCallback(args[0].As<Function>());

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
		uv_thread_create(&m_tLongExposureInfraredThread, LongExposureInfraredReaderThreadLoop, NULL);
	}

	SafeRelease(pLongExposureInfraredFrameSource);

	uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);
	if (SUCCEEDED(hr))
	{
		NanReturnValue(NanTrue());
	}
	else
	{
		NanReturnValue(NanFalse());
	}
}

NAN_METHOD(CloseLongExposureInfraredReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mLongExposureInfraredReaderMutex);
	m_bLongExposureInfraredThreadRunning = false;
	SafeRelease(m_pLongExposureInfraredFrameReader);
	uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);

	NanReturnValue(NanTrue());
}

NAUV_WORK_CB(DepthProgress_) {
	uv_mutex_lock(&m_mDepthReaderMutex);
	NanScope();
	if(m_pDepthReaderCallback != NULL)
	{
		//reuse the existing buffer
		v8::Local<v8::Object> v8DepthPixels = NanNew(m_persistentDepthPixels);
		char* data = node::Buffer::Data(v8DepthPixels);
		memcpy(data, m_pDepthPixels, cDepthWidth * cDepthHeight);

		Local<Value> argv[] = {
			v8DepthPixels
		};
		m_pDepthReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mDepthReaderMutex);
}

void DepthReaderThreadLoop(void *arg)
{
	while(1)
	{
		uv_mutex_lock(&m_mDepthReaderMutex);
		if(!m_bDepthThreadRunning)
		{
			//make sure to unlock the mutex
			uv_mutex_unlock(&m_mDepthReaderMutex);
			break;
		}

		IDepthFrame* pDepthFrame = NULL;
		IFrameDescription* pDepthFrameDescription = NULL;
		int nDepthWidth = 0;
		int nDepthHeight = 0;
		UINT nDepthBufferSize = 0;
		UINT16 *pDepthBuffer = NULL;
		USHORT nDepthMinReliableDistance = 0;
		USHORT nDepthMaxDistance = 0;
		int mapDepthToByte = 8000 / 256;

		HRESULT hr;

		hr = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);
		}
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameDescription->get_Width(&nDepthWidth);
		}
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameDescription->get_Height(&nDepthHeight);
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
			if (m_pDepthPixels && pDepthBuffer && (nDepthWidth == cDepthWidth) && (nDepthHeight == cDepthHeight))
			{
				char* pDepthPixel = m_pDepthPixels;

				// end pixel is start + width*height - 1
				const UINT16* pDepthBufferEnd = pDepthBuffer + (nDepthWidth * nDepthHeight);

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
		if (SUCCEEDED(hr))
		{
			//notify event loop
			uv_async_send(&m_aDepthAsync);
		}
		SafeRelease(pDepthFrameDescription);
		SafeRelease(pDepthFrame);
		uv_mutex_unlock(&m_mDepthReaderMutex);
	}
}


NAN_METHOD(OpenDepthReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mDepthReaderMutex);
	if(m_pDepthReaderCallback)
	{
		delete m_pDepthReaderCallback;
		m_pDepthReaderCallback = NULL;
	}

	m_pDepthReaderCallback = new NanCallback(args[0].As<Function>());

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
		uv_thread_create(&m_tDepthThread, DepthReaderThreadLoop, NULL);
	}

	SafeRelease(pDepthFrameSource);

	uv_mutex_unlock(&m_mDepthReaderMutex);
	if (SUCCEEDED(hr))
	{
		NanReturnValue(NanTrue());
	}
	else
	{
		NanReturnValue(NanFalse());
	}
}

NAN_METHOD(CloseDepthReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mDepthReaderMutex);
	m_bDepthThreadRunning = false;
	SafeRelease(m_pDepthFrameReader);
	uv_mutex_unlock(&m_mDepthReaderMutex);

	NanReturnValue(NanTrue());
}

NAUV_WORK_CB(BodyProgress_) {
	uv_mutex_lock(&m_mBodyReaderMutex);
	NanScope();
	if(m_pBodyReaderCallback != NULL)
	{
		//save the bodies as a V8 object structure
		v8::Local<v8::Array> v8bodies = NanNew<v8::Array>(numTrackedBodies);
		int bodyIndex = 0;
		for(int i = 0; i < BODY_COUNT; i++)
		{
			if(m_jsBodyFrame.bodies[i].tracked)
			{
				//create a body object
				v8::Local<v8::Object> v8body = NanNew<v8::Object>();
				v8body->Set(NanNew<v8::String>("trackingId"), NanNew<v8::Number>(static_cast<double>(m_jsBodyFrame.bodies[i].trackingId)));
				//hand states
				v8body->Set(NanNew<v8::String>("leftHandState"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].leftHandState));
				v8body->Set(NanNew<v8::String>("rightHandState"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].rightHandState));
				v8::Local<v8::Object> v8joints = NanNew<v8::Object>();
				//joints
				for (int j = 0; j < _countof(m_jsBodyFrame.bodies[i].joints); ++j)
				{
					v8::Local<v8::Object> v8joint = NanNew<v8::Object>();
					v8joint->Set(NanNew<v8::String>("depthX"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].depthX));
					v8joint->Set(NanNew<v8::String>("depthY"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].depthY));
					v8joint->Set(NanNew<v8::String>("colorX"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].colorX));
					v8joint->Set(NanNew<v8::String>("colorY"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].colorY));
					v8joint->Set(NanNew<v8::String>("cameraX"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].cameraX));
					v8joint->Set(NanNew<v8::String>("cameraY"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].cameraY));
					v8joint->Set(NanNew<v8::String>("cameraZ"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].cameraZ));
					v8joints->Set(NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].jointType), v8joint);
				}
				v8body->Set(NanNew<v8::String>("joints"), v8joints);

				v8bodies->Set(bodyIndex, v8body);
				bodyIndex++;
			}
		}

		v8::Local<v8::Value> argv[] = {
			v8bodies
		};
		m_pBodyReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mBodyReaderMutex);
}

void BodyReaderThreadLoop(void *arg)
{
	while(1)
	{
		uv_mutex_lock(&m_mBodyReaderMutex);
		if(!m_bBodyThreadRunning)
		{
			//make sure to unlock the mutex
			uv_mutex_unlock(&m_mBodyReaderMutex);
			break;
		}

		IBodyFrame* pBodyFrame = NULL;
		IBody* ppBodies[BODY_COUNT] = {0};
		numTrackedBodies = 0;
		HRESULT hr;

		hr = m_pBodyFrameReader->AcquireLatestFrame(&pBodyFrame);
		if(SUCCEEDED(hr))
		{
			hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);
		}

		if (SUCCEEDED(hr))
		{
			DepthSpacePoint depthPoint = {0};
			ColorSpacePoint colorPoint = {0};
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
						numTrackedBodies++;
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
						hr = pBody->GetJoints(_countof(joints), joints);
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

								m_jsBodyFrame.bodies[i].joints[j].jointType = joints[j].JointType;
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
		if (SUCCEEDED(hr))
		{
			//notify event loop
			uv_async_send(&m_aBodyAsync);
		}
		SafeRelease(pBodyFrame);
		for (int i = 0; i < _countof(ppBodies); ++i)
		{
			SafeRelease(ppBodies[i]);
		}
		uv_mutex_unlock(&m_mBodyReaderMutex);
	}
}


NAN_METHOD(OpenBodyReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mBodyReaderMutex);
	if(m_pBodyReaderCallback)
	{
		delete m_pBodyReaderCallback;
		m_pBodyReaderCallback = NULL;
	}

	m_pBodyReaderCallback = new NanCallback(args[0].As<Function>());

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
		uv_thread_create(&m_tBodyThread, BodyReaderThreadLoop, NULL);
	}

	SafeRelease(pBodyFrameSource);

	uv_mutex_unlock(&m_mBodyReaderMutex);
	if (SUCCEEDED(hr))
	{
		NanReturnValue(NanTrue());
	}
	else
	{
		NanReturnValue(NanFalse());
	}
}

NAN_METHOD(CloseBodyReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mBodyReaderMutex);
	m_bBodyThreadRunning = false;
	SafeRelease(m_pBodyFrameReader);
	uv_mutex_unlock(&m_mBodyReaderMutex);

	NanReturnValue(NanTrue());
}

NAUV_WORK_CB(MultiSourceProgress_) {
	uv_mutex_lock(&m_mMultiSourceReaderMutex);
	NanScope();
	if(m_pMultiSourceReaderCallback != NULL)
	{
		//build object for callback
		v8::Local<v8::Object> v8Result = NanNew<v8::Object>();

		if(NodeKinect2FrameTypes::FrameTypes_Color & m_enabledFrameTypes)
		{
			//reuse the existing buffer
			v8::Local<v8::Object> v8ColorPixels = NanNew(m_persistentColorPixels);
			char* data = node::Buffer::Data(v8ColorPixels);
			memcpy(data, m_pColorPixels, cColorWidth * cColorHeight * sizeof(RGBQUAD));

			v8::Local<v8::Object> v8ColorResult = NanNew<v8::Object>();
			v8ColorResult->Set(NanNew<v8::String>("buffer"), v8ColorPixels);
			v8Result->Set(NanNew<v8::String>("color"), v8ColorResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_Depth & m_enabledFrameTypes)
		{
			//reuse the existing buffer
			v8::Local<v8::Object> v8DepthPixels = NanNew(m_persistentDepthPixels);
			char* data = node::Buffer::Data(v8DepthPixels);
			memcpy(data, m_pDepthPixels, cDepthWidth * cDepthHeight);

			v8::Local<v8::Object> v8DepthResult = NanNew<v8::Object>();
			v8DepthResult->Set(NanNew<v8::String>("buffer"), v8DepthPixels);
			v8Result->Set(NanNew<v8::String>("depth"), v8DepthResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_Body & m_enabledFrameTypes)
		{
			v8::Local<v8::Object> v8BodyResult = NanNew<v8::Object>();

			v8::Local<v8::Array> v8bodies = NanNew<v8::Array>(numTrackedBodies);
			int bodyIndex = 0;
			for(int i = 0; i < BODY_COUNT; i++)
			{
				if(m_jsBodyFrame.bodies[i].tracked)
				{
					//create a body object
					v8::Local<v8::Object> v8body = NanNew<v8::Object>();
					v8body->Set(NanNew<v8::String>("trackingId"), NanNew<v8::Number>(static_cast<double>(m_jsBodyFrame.bodies[i].trackingId)));
					//hand states
					v8body->Set(NanNew<v8::String>("leftHandState"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].leftHandState));
					v8body->Set(NanNew<v8::String>("rightHandState"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].rightHandState));
					v8::Local<v8::Object> v8joints = NanNew<v8::Object>();
					//joints
					for (int j = 0; j < _countof(m_jsBodyFrame.bodies[i].joints); ++j)
					{
						v8::Local<v8::Object> v8joint = NanNew<v8::Object>();
						v8joint->Set(NanNew<v8::String>("depthX"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].depthX));
						v8joint->Set(NanNew<v8::String>("depthY"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].depthY));
						v8joint->Set(NanNew<v8::String>("colorX"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].colorX));
						v8joint->Set(NanNew<v8::String>("colorY"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].colorY));
						v8joint->Set(NanNew<v8::String>("cameraX"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].cameraX));
						v8joint->Set(NanNew<v8::String>("cameraY"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].cameraY));
						v8joint->Set(NanNew<v8::String>("cameraZ"), NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].cameraZ));
						v8joints->Set(NanNew<v8::Number>(m_jsBodyFrame.bodies[i].joints[j].jointType), v8joint);
					}
					v8body->Set(NanNew<v8::String>("joints"), v8joints);

					v8bodies->Set(bodyIndex, v8body);
					bodyIndex++;
				}
			}
			v8BodyResult->Set(NanNew<v8::String>("bodies"), v8bodies);

			v8Result->Set(NanNew<v8::String>("body"), v8BodyResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_BodyIndexColor & m_enabledFrameTypes)
		{
			v8::Local<v8::Object> v8BodyIndexColorResult = NanNew<v8::Object>();

			//persistent handle of array with color buffers
			v8::Local<v8::Object> v8BodyIndexColorPixels = NanNew(m_persistentBodyIndexColorPixels);

			v8::Local<v8::Array> v8bodies = NanNew<v8::Array>(BODY_COUNT);
			int numBodiesWithPixels = 0;
			for(int i = 0; i < BODY_COUNT; i++)
			{
				v8::Local<v8::Object> v8body = NanNew<v8::Object>();
				//something weird is going on: every x frames hasPixels is false, but body is still tracked?
				if(m_jsBodyFrame.bodies[i].hasPixels || m_jsBodyFrame.bodies[i].tracked) {
					numBodiesWithPixels++;
					//reuse the existing buffer
					v8::Local<v8::Value> v8ColorPixels = v8BodyIndexColorPixels->Get(i);
					char* data = node::Buffer::Data(v8ColorPixels);
					size_t len = node::Buffer::Length(v8ColorPixels);
					memcpy(data, m_jsBodyFrame.bodies[i].colorPixels, cColorWidth * cColorHeight * sizeof(RGBQUAD));
					v8body->Set(NanNew<v8::String>("buffer"), v8ColorPixels);
				}
				v8bodies->Set(i, v8body);
			}
			v8BodyIndexColorResult->Set(NanNew<v8::String>("bodies"), v8bodies);

			v8Result->Set(NanNew<v8::String>("bodyIndexColor"), v8BodyIndexColorResult);
		}

		v8::Local<v8::Value> argv[] = {
			v8Result
		};

		m_pMultiSourceReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mMultiSourceReaderMutex);
}

void MultiSourceReaderThreadLoop(void *arg)
{
	while(1)
	{
		uv_mutex_lock(&m_mMultiSourceReaderMutex);
		if(!m_bMultiSourceThreadRunning)
		{
			//make sure to unlock the mutex
			uv_mutex_unlock(&m_mMultiSourceReaderMutex);
			break;
		}

		HRESULT hr;
		IMultiSourceFrame* pMultiSourceFrame = NULL;

		hr = m_pMultiSourceFrameReader->AcquireLatestFrame(&pMultiSourceFrame);

		if(SUCCEEDED(hr)) {
			//get the frames
			IColorFrameReference* pColorFrameReference = NULL;
			IColorFrame* pColorFrame = NULL;
			IFrameDescription* pColorFrameDescription = NULL;
			int nColorWidth = 0;
			int nColorHeight = 0;
			ColorImageFormat colorImageFormat = ColorImageFormat_None;
			UINT nColorBufferSize = 0;
			RGBQUAD *pColorBuffer = NULL;

			IInfraredFrameReference* pInfraredFrameReference = NULL;
			IInfraredFrame* pInfraredFrame = NULL;

			ILongExposureInfraredFrameReference* pLongExposureInfraredFrameReference = NULL;
			ILongExposureInfraredFrame* pLongExposureInfraredFrame = NULL;

			IDepthFrameReference* pDepthFrameReference = NULL;
			IDepthFrame* pDepthFrame = NULL;
			IFrameDescription* pDepthFrameDescription = NULL;
			int nDepthWidth = 0;
			int nDepthHeight = 0;
			UINT nDepthBufferSize = 0;
			UINT16 *pDepthBuffer = NULL;
			USHORT nDepthMinReliableDistance = 0;
			USHORT nDepthMaxDistance = 0;
			int mapDepthToByte = 8000 / 256;

			IBodyIndexFrameReference* pBodyIndexFrameReference = NULL;
			IBodyIndexFrame* pBodyIndexFrame = NULL;
			IFrameDescription* pBodyIndexFrameDescription = NULL;
			int nBodyIndexWidth = 0;
			int nBodyIndexHeight = 0;
			UINT nBodyIndexBufferSize = 0;
			BYTE *pBodyIndexBuffer = NULL;
			for(int i = 0; i < BODY_COUNT; i++)
			{
				m_jsBodyFrame.bodies[i].hasPixels = false;
			}

			IBodyFrameReference* pBodyFrameReference = NULL;
			IBodyFrame* pBodyFrame = NULL;
			IBody* ppBodies[BODY_COUNT] = {0};
			numTrackedBodies = 0;

			if(FrameSourceTypes::FrameSourceTypes_Color & m_enabledFrameSourceTypes)
			{
				hr = pMultiSourceFrame->get_ColorFrameReference(&pColorFrameReference);
				if (SUCCEEDED(hr))
				{
					hr = pColorFrameReference->AcquireFrame(&pColorFrame);
				}
				if (SUCCEEDED(hr))
				{
					hr = pColorFrame->get_FrameDescription(&pColorFrameDescription);
				}

				if (SUCCEEDED(hr))
				{
					hr = pColorFrameDescription->get_Width(&nColorWidth);
				}

				if (SUCCEEDED(hr))
				{
					hr = pColorFrameDescription->get_Height(&nColorHeight);
				}

				if (SUCCEEDED(hr))
				{
					hr = pColorFrame->get_RawColorImageFormat(&colorImageFormat);
				}

				if (SUCCEEDED(hr))
				{
					if (colorImageFormat == ColorImageFormat_Rgba)
					{
						hr = pColorFrame->AccessRawUnderlyingBuffer(&nColorBufferSize, reinterpret_cast<BYTE**>(&pColorBuffer));
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
			}
			if(FrameSourceTypes::FrameSourceTypes_Infrared & m_enabledFrameSourceTypes)
			{
				hr = pMultiSourceFrame->get_InfraredFrameReference(&pInfraredFrameReference);
				if (SUCCEEDED(hr))
				{
					hr = pInfraredFrameReference->AcquireFrame(&pInfraredFrame);
				}
			}
			if(FrameSourceTypes::FrameSourceTypes_LongExposureInfrared & m_enabledFrameSourceTypes)
			{
				hr = pMultiSourceFrame->get_LongExposureInfraredFrameReference(&pLongExposureInfraredFrameReference);
				if (SUCCEEDED(hr))
				{
					hr = pLongExposureInfraredFrameReference->AcquireFrame(&pLongExposureInfraredFrame);
				}
			}
			if(FrameSourceTypes::FrameSourceTypes_Depth & m_enabledFrameSourceTypes)
			{
				hr = pMultiSourceFrame->get_DepthFrameReference(&pDepthFrameReference);
				if (SUCCEEDED(hr))
				{
					hr = pDepthFrameReference->AcquireFrame(&pDepthFrame);
				}
				if (SUCCEEDED(hr))
				{
					hr = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);
				}
				if (SUCCEEDED(hr))
				{
					hr = pDepthFrameDescription->get_Width(&nDepthWidth);
				}
				if (SUCCEEDED(hr))
				{
					hr = pDepthFrameDescription->get_Height(&nDepthHeight);
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
			}
			if(FrameSourceTypes::FrameSourceTypes_BodyIndex & m_enabledFrameSourceTypes)
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
					hr = pBodyIndexFrameDescription->get_Width(&nBodyIndexWidth);
				}
				if (SUCCEEDED(hr))
				{
					hr = pBodyIndexFrameDescription->get_Height(&nBodyIndexHeight);
				}
				if (SUCCEEDED(hr))
				{
					hr = pBodyIndexFrame->AccessUnderlyingBuffer(&nBodyIndexBufferSize, &pBodyIndexBuffer);
				}
			}
			if(FrameSourceTypes::FrameSourceTypes_Body & m_enabledFrameSourceTypes)
			{
				hr = pMultiSourceFrame->get_BodyFrameReference(&pBodyFrameReference);
				if (SUCCEEDED(hr))
				{
					hr = pBodyFrameReference->AcquireFrame(&pBodyFrame);
				}
				if(SUCCEEDED(hr))
				{
					hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);
				}
			}

			//process data according to requested frame types
			/*
			FrameTypes_None	= 0,
			FrameTypes_Color	= 0x1,
			FrameTypes_Infrared	= 0x2,
			FrameTypes_LongExposureInfrared	= 0x4,
			FrameTypes_Depth	= 0x8,
			FrameTypes_BodyIndex	= 0x10,
			FrameTypes_Body	= 0x20,
			FrameTypes_Audio	= 0x40,
			FrameTypes_BodyIndexColor	= 0x80,
			FrameTypes_BodyIndexDepth	= 0x10,
			FrameTypes_BodyIndexInfrared	= 0x100,
			FrameTypes_BodyIndexLongExposureInfrared	= 0x200,
			FrameTypes_RawDepth	= 0x400
			*/

			//we do something with color data
			if(FrameSourceTypes::FrameSourceTypes_Color & m_enabledFrameSourceTypes)
			{
				//what are we doing with the color data?
				//we could have color and / or bodyindexcolor
				if(NodeKinect2FrameTypes::FrameTypes_BodyIndexColor & m_enabledFrameTypes)
				{
					if (SUCCEEDED(hr))
					{
						hr = m_pCoordinateMapper->MapColorFrameToDepthSpace(nDepthWidth * nDepthHeight, (UINT16*)pDepthBuffer, nColorWidth * nColorHeight, m_pDepthCoordinatesForColor);
					}
					if (SUCCEEDED(hr))
					{
						//default transparent colors
						for( int i = 0 ; i < BODY_COUNT ; i++ ) {
							memset(m_jsBodyFrame.bodies[i].colorPixels, 0, cColorWidth * cColorHeight * sizeof(RGBQUAD));
						}

						for (int colorIndex = 0; colorIndex < (nColorWidth*nColorHeight); ++colorIndex)
						{

							DepthSpacePoint p = m_pDepthCoordinatesForColor[colorIndex];

							// Values that are negative infinity means it is an invalid color to depth mapping so we
							// skip processing for this pixel
							if (p.X != -std::numeric_limits<float>::infinity() && p.Y != -std::numeric_limits<float>::infinity())
							{
								int depthX = static_cast<int>(p.X + 0.5f);
								int depthY = static_cast<int>(p.Y + 0.5f);
								if ((depthX >= 0 && depthX < nDepthWidth) && (depthY >= 0 && depthY < nDepthHeight))
								{
									BYTE player = pBodyIndexBuffer[depthX + (depthY * nDepthWidth)];

									// if we're tracking a player for the current pixel, draw from the color camera
									if (player != 0xff)
									{
										m_jsBodyFrame.bodies[player].hasPixels = true;
										// set source for copy to the color pixel
										m_jsBodyFrame.bodies[player].colorPixels[colorIndex] = pColorBuffer[colorIndex];
									}
								}
							}
						}
					}
				}
				if(NodeKinect2FrameTypes::FrameTypes_Color & m_enabledFrameTypes)
				{
					//copy the color data to the result buffer
					if (SUCCEEDED(hr))
					{
						memcpy(m_pColorPixels, pColorBuffer, cColorWidth * cColorHeight * sizeof(RGBQUAD));
					}
				}
			}

			if(FrameSourceTypes::FrameSourceTypes_Depth & m_enabledFrameSourceTypes)
			{
				//depth image
				if(NodeKinect2FrameTypes::FrameTypes_Depth & m_enabledFrameTypes)
				{
					if (SUCCEEDED(hr))
					{
						mapDepthToByte = nDepthMaxDistance / 256;
						if (m_pDepthPixels && pDepthBuffer && (nDepthWidth == cDepthWidth) && (nDepthHeight == cDepthHeight))
						{
							char* pDepthPixel = m_pDepthPixels;

							// end pixel is start + width*height - 1
							const UINT16* pDepthBufferEnd = pDepthBuffer + (nDepthWidth * nDepthHeight);

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
				}
			}

			//todo: add raw depth & body index depth to above

			if(FrameSourceTypes::FrameSourceTypes_Body & m_enabledFrameSourceTypes)
			{
				//body frame
				if(NodeKinect2FrameTypes::FrameTypes_Body & m_enabledFrameTypes)
				{
					DepthSpacePoint depthPoint = {0};
					ColorSpacePoint colorPoint = {0};
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
								numTrackedBodies++;
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
								hr = pBody->GetJoints(_countof(joints), joints);
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

										m_jsBodyFrame.bodies[i].joints[j].jointType = joints[j].JointType;
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
			}

			//todo: infrared
			//todo: long exposure infrared

			//release memory
			SafeRelease(pColorFrameDescription);
			SafeRelease(pColorFrame);
			SafeRelease(pColorFrameReference);

			SafeRelease(pInfraredFrame);
			SafeRelease(pInfraredFrameReference);

			SafeRelease(pLongExposureInfraredFrame);
			SafeRelease(pLongExposureInfraredFrameReference);

			SafeRelease(pDepthFrameDescription);
			SafeRelease(pDepthFrame);
			SafeRelease(pDepthFrameReference);

			SafeRelease(pBodyIndexFrameDescription);
			SafeRelease(pBodyIndexFrame);
			SafeRelease(pBodyIndexFrameReference);

			for (int i = 0; i < _countof(ppBodies); ++i)
			{
				SafeRelease(ppBodies[i]);
			}
			SafeRelease(pBodyFrame);
			SafeRelease(pBodyFrameReference);

		}

		if (SUCCEEDED(hr))
		{
			//notify event loop
			uv_async_send(&m_aMultiSourceAsync);
		}

		SafeRelease(pMultiSourceFrame);

		uv_mutex_unlock(&m_mMultiSourceReaderMutex);
	}
}


NAN_METHOD(OpenMultiSourceReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mMultiSourceReaderMutex);
	if(m_pMultiSourceReaderCallback)
	{
		delete m_pMultiSourceReaderCallback;
		m_pMultiSourceReaderCallback = NULL;
	}

	Local<Object> jsOptions = args[0].As<Object>();
	Local<Function> jsCallback = jsOptions->Get(NanNew<v8::String>("callback")).As<Function>();
	m_enabledFrameTypes = static_cast<unsigned long>(jsOptions->Get(NanNew<v8::String>("frameTypes")).As<Number>()->Value());

	//map our own frame types to the correct frame source types
	m_enabledFrameSourceTypes = FrameSourceTypes::FrameSourceTypes_None;
	if(
		m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_Color
		|| m_enabledFrameTypes & NodeKinect2FrameTypes::FrameTypes_BodyIndexColor
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

	m_pMultiSourceReaderCallback = new NanCallback(jsCallback);

	HRESULT hr = m_pKinectSensor->OpenMultiSourceFrameReader(m_enabledFrameSourceTypes, &m_pMultiSourceFrameReader);

	if (SUCCEEDED(hr))
	{
		m_bMultiSourceThreadRunning = true;
		uv_thread_create(&m_tMultiSourceThread, MultiSourceReaderThreadLoop, NULL);
	}

	uv_mutex_unlock(&m_mMultiSourceReaderMutex);
	if (SUCCEEDED(hr))
	{
		NanReturnValue(NanTrue());
	}
	else
	{
		NanReturnValue(NanFalse());
	}
}

NAN_METHOD(CloseMultiSourceReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_mMultiSourceReaderMutex);
	m_bMultiSourceThreadRunning = false;
	SafeRelease(m_pMultiSourceFrameReader);
	uv_mutex_unlock(&m_mMultiSourceReaderMutex);

	NanReturnValue(NanTrue());
}

void Init(Handle<Object> exports)
{
	NanScope();

	//color
	uv_mutex_init(&m_mColorReaderMutex);
	uv_async_init(uv_default_loop(), &m_aColorAsync, ColorProgress_);
	NanAssignPersistent(m_persistentColorPixels, NanNewBufferHandle((char *)m_pColorPixels, cColorWidth * cColorHeight * sizeof(RGBQUAD)));

	//infrared
	uv_mutex_init(&m_mInfraredReaderMutex);
	uv_async_init(uv_default_loop(), &m_aInfraredAsync, InfraredProgress_);
	NanAssignPersistent(m_persistentInfraredPixels, NanNewBufferHandle(m_pInfraredPixels, cInfraredWidth * cInfraredHeight));

	//long exposure infrared
	uv_mutex_init(&m_mLongExposureInfraredReaderMutex);
	uv_async_init(uv_default_loop(), &m_aLongExposureInfraredAsync, LongExposureInfraredProgress_);
	NanAssignPersistent(m_persistentLongExposureInfraredPixels, NanNewBufferHandle(m_pLongExposureInfraredPixels, cLongExposureInfraredWidth * cLongExposureInfraredHeight));

	//depth
	uv_mutex_init(&m_mDepthReaderMutex);
	uv_async_init(uv_default_loop(), &m_aDepthAsync, DepthProgress_);
	NanAssignPersistent(m_persistentDepthPixels, NanNewBufferHandle(m_pDepthPixels, cDepthWidth * cDepthHeight));

	//body
	uv_mutex_init(&m_mBodyReaderMutex);
	uv_async_init(uv_default_loop(), &m_aBodyAsync, BodyProgress_);

	//multisource
	uv_mutex_init(&m_mMultiSourceReaderMutex);
	uv_async_init(uv_default_loop(), &m_aMultiSourceAsync, MultiSourceProgress_);
	v8::Local<v8::Object> v8BodyIndexColorPixels = NanNew<v8::Object>();
	for(int i = 0; i < BODY_COUNT; i++)
	{
		v8BodyIndexColorPixels->Set(i, NanNewBufferHandle((char *)m_jsBodyFrame.bodies[i].colorPixels, cColorWidth * cColorHeight * sizeof(RGBQUAD)));
	}
	NanAssignPersistent(m_persistentBodyIndexColorPixels, v8BodyIndexColorPixels);

	//disposing should happen with NanDisposePersistent(handle);

	exports->Set(NanNew<String>("open"),
		NanNew<FunctionTemplate>(OpenFunction)->GetFunction());
	exports->Set(NanNew<String>("close"),
		NanNew<FunctionTemplate>(CloseFunction)->GetFunction());
	exports->Set(NanNew<String>("openColorReader"),
		NanNew<FunctionTemplate>(OpenColorReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("closeColorReader"),
		NanNew<FunctionTemplate>(CloseColorReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("openInfraredReader"),
		NanNew<FunctionTemplate>(OpenInfraredReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("closeInfraredReader"),
		NanNew<FunctionTemplate>(CloseInfraredReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("openLongExposureInfraredReader"),
		NanNew<FunctionTemplate>(OpenLongExposureInfraredReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("closeLongExposureInfraredReader"),
		NanNew<FunctionTemplate>(CloseLongExposureInfraredReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("openDepthReader"),
		NanNew<FunctionTemplate>(OpenDepthReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("closeDepthReader"),
		NanNew<FunctionTemplate>(CloseDepthReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("openBodyReader"),
		NanNew<FunctionTemplate>(OpenBodyReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("closeBodyReader"),
		NanNew<FunctionTemplate>(CloseBodyReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("openMultiSourceReader"),
		NanNew<FunctionTemplate>(OpenMultiSourceReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("closeMultiSourceReader"),
		NanNew<FunctionTemplate>(CloseInfraredReaderFunction)->GetFunction());
}

NODE_MODULE(kinect2, Init)
