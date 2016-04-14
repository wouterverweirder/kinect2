#include <nan.h>
#include <iostream>
#include <uv.h>
#include "Kinect.h"
#include "Globals.h"
#include "Structs.h"

using namespace v8;

IKinectSensor*											m_pKinectSensor = NULL;
ICoordinateMapper*									m_pCoordinateMapper = NULL;
IColorFrameReader*  								m_pColorFrameReader = NULL;
IInfraredFrameReader* 							m_pInfraredFrameReader = NULL;
ILongExposureInfraredFrameReader* 	m_pLongExposureInfraredFrameReader = NULL;
IDepthFrameReader*									m_pDepthFrameReader = NULL;
IBodyFrameReader*										m_pBodyFrameReader = NULL;
IMultiSourceFrameReader*						m_pMultiSourceFrameReader = NULL;

RGBQUAD*								m_pColorPixels = new RGBQUAD[cColorWidth * cColorHeight];
RGBQUAD*								m_pColorPixelsV8 = new RGBQUAD[cColorWidth * cColorHeight];
char*										m_pInfraredPixels = new char[cInfraredWidth * cInfraredHeight];
char*										m_pInfraredPixelsV8 = new char[cInfraredWidth * cInfraredHeight];
char*										m_pLongExposureInfraredPixels = new char[cLongExposureInfraredWidth * cLongExposureInfraredHeight];
char*										m_pLongExposureInfraredPixelsV8 = new char[cLongExposureInfraredWidth * cLongExposureInfraredHeight];
char*										m_pDepthPixels = new char[cDepthWidth * cDepthHeight];
char*										m_pDepthPixelsV8 = new char[cDepthWidth * cDepthHeight];
UINT16*									m_pRawDepthValues = new UINT16[cDepthWidth * cDepthHeight];
UINT16*									m_pRawDepthValuesV8 = new UINT16[cDepthWidth * cDepthHeight];
RGBQUAD*								m_pDepthColorPixels = new RGBQUAD[cDepthWidth * cDepthHeight];
RGBQUAD*								m_pDepthColorPixelsV8 = new RGBQUAD[cDepthWidth * cDepthHeight];

bool 										m_trackPixelsForBodyIndexV8[BODY_COUNT];

RGBQUAD									m_pBodyIndexColorPixels[BODY_COUNT][cColorWidth * cColorHeight];
RGBQUAD									m_pBodyIndexColorPixelsV8[BODY_COUNT][cColorWidth * cColorHeight];

JSBodyFrame							m_jsBodyFrame;
JSBodyFrame							m_jsBodyFrameV8;

DepthSpacePoint*				m_pDepthCoordinatesForColor = new DepthSpacePoint[cColorWidth * cColorHeight];
ColorSpacePoint*				m_pColorCoordinatesForDepth = new ColorSpacePoint[cDepthWidth * cDepthHeight];

//enabledFrameSourceTypes refers to the kinect SDK frame source types
DWORD										m_enabledFrameSourceTypes = 0;
//enabledFrameTypes is our own list of frame types
//this is the kinect SDK list + additional ones (body index in color space, etc...)
unsigned long						m_enabledFrameTypes = 0;

Nan::Callback*					m_pColorReaderCallback;
Nan::Callback*					m_pInfraredReaderCallback;
Nan::Callback*					m_pLongExposureInfraredReaderCallback;
Nan::Callback*					m_pDepthReaderCallback;
Nan::Callback*					m_pRawDepthReaderCallback;
Nan::Callback*					m_pBodyReaderCallback;
Nan::Callback*					m_pMultiSourceReaderCallback;

uv_mutex_t							m_mColorReaderMutex;
uv_mutex_t							m_mInfraredReaderMutex;
uv_mutex_t							m_mLongExposureInfraredReaderMutex;
uv_mutex_t							m_mDepthReaderMutex;
uv_mutex_t							m_mRawDepthReaderMutex;
uv_mutex_t							m_mBodyReaderMutex;
uv_mutex_t							m_mMultiSourceReaderMutex;

uv_async_t							m_aColorAsync;
uv_thread_t							m_tColorThread;
bool 										m_bColorThreadRunning = false;
Nan::Persistent<Object>	m_persistentColorPixels;
float 									m_fColorHorizontalFieldOfView;
float 									m_fColorVerticalFieldOfView;
float 									m_fColorDiagonalFieldOfView;
float 									m_fColorHorizontalFieldOfViewV8;
float 									m_fColorVerticalFieldOfViewV8;
float 									m_fColorDiagonalFieldOfViewV8;

uv_async_t							m_aInfraredAsync;
uv_thread_t							m_tInfraredThread;
bool 										m_bInfraredThreadRunning = false;
Nan::Persistent<Object>	m_persistentInfraredPixels;
float 									m_fInfraredHorizontalFieldOfView;
float 									m_fInfraredVerticalFieldOfView;
float 									m_fInfraredDiagonalFieldOfView;
float 									m_fInfraredHorizontalFieldOfViewV8;
float 									m_fInfraredVerticalFieldOfViewV8;
float 									m_fInfraredDiagonalFieldOfViewV8;

uv_async_t							m_aLongExposureInfraredAsync;
uv_thread_t							m_tLongExposureInfraredThread;
bool 										m_bLongExposureInfraredThreadRunning = false;
Nan::Persistent<Object>	m_persistentLongExposureInfraredPixels;
float 									m_fLongExposureInfraredHorizontalFieldOfView;
float 									m_fLongExposureInfraredVerticalFieldOfView;
float 									m_fLongExposureInfraredDiagonalFieldOfView;
float 									m_fLongExposureInfraredHorizontalFieldOfViewV8;
float 									m_fLongExposureInfraredVerticalFieldOfViewV8;
float 									m_fLongExposureInfraredDiagonalFieldOfViewV8;

uv_async_t							m_aDepthAsync;
uv_thread_t							m_tDepthThread;
bool 										m_bDepthThreadRunning = false;
Nan::Persistent<Object> m_persistentDepthPixels;
float 									m_fDepthHorizontalFieldOfView;
float 									m_fDepthVerticalFieldOfView;
float 									m_fDepthDiagonalFieldOfView;
float 									m_fDepthHorizontalFieldOfViewV8;
float 									m_fDepthVerticalFieldOfViewV8;
float 									m_fDepthDiagonalFieldOfViewV8;

uv_async_t							m_aRawDepthAsync;
uv_thread_t							m_tRawDepthThread;
bool 										m_bRawDepthThreadRunning = false;
Nan::Persistent<Object> m_persistentRawDepthValues;

Nan::Persistent<Object> m_persistentDepthColorPixels;

uv_async_t							m_aBodyAsync;
uv_thread_t							m_tBodyThread;
bool 										m_bBodyThreadRunning = false;

uv_async_t							m_aMultiSourceAsync;
uv_thread_t							m_tMultiSourceThread;
bool 										m_bMultiSourceThreadRunning = false;
Nan::Persistent<Object>	m_persistentBodyIndexColorPixels;

bool 										m_includeJointFloorData = false;

NAN_METHOD(OpenFunction)
{
	HRESULT hr;
	hr = GetDefaultKinectSensor(&m_pKinectSensor);
	if (FAILED(hr))
	{
		info.GetReturnValue().Set(false);
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
		info.GetReturnValue().Set(false);
	}
	info.GetReturnValue().Set(true);
}

template<class Interface>
void stopReader(uv_mutex_t* mutex, bool* threadCondition, uv_thread_t* thread, uv_handle_t* async, Interface* pInterfaceToRelease)
{
	uv_mutex_lock(mutex);
	bool wasRunning = *threadCondition;
	*threadCondition = false;
	uv_mutex_unlock(mutex);
	if(wasRunning)
	{
		uv_thread_join(thread);
		uv_close(async, NULL);
	}
	SafeRelease(pInterfaceToRelease);
}

NAN_METHOD(CloseFunction)
{
	stopReader(&m_mColorReaderMutex, &m_bColorThreadRunning, &m_tColorThread, (uv_handle_t*) &m_aColorAsync, m_pColorFrameReader);
	stopReader(&m_mInfraredReaderMutex, &m_bInfraredThreadRunning, &m_tInfraredThread, (uv_handle_t*) &m_aInfraredAsync, m_pInfraredFrameReader);
	stopReader(&m_mLongExposureInfraredReaderMutex, &m_bLongExposureInfraredThreadRunning, &m_tLongExposureInfraredThread, (uv_handle_t*) &m_aLongExposureInfraredAsync, m_pLongExposureInfraredFrameReader);
	stopReader(&m_mDepthReaderMutex, &m_bDepthThreadRunning, &m_tDepthThread, (uv_handle_t*) &m_aDepthAsync, m_pDepthFrameReader);
	stopReader(&m_mRawDepthReaderMutex, &m_bRawDepthThreadRunning, &m_tRawDepthThread, (uv_handle_t*) &m_aRawDepthAsync, m_pDepthFrameReader);
	stopReader(&m_mBodyReaderMutex, &m_bBodyThreadRunning, &m_tBodyThread, (uv_handle_t*) &m_aBodyAsync, m_pBodyFrameReader);
	stopReader(&m_mMultiSourceReaderMutex, &m_bMultiSourceThreadRunning, &m_tMultiSourceThread, (uv_handle_t*) &m_aMultiSourceAsync, m_pMultiSourceFrameReader);

	// done with coordinate mapper
	SafeRelease(m_pCoordinateMapper);

	// close the Kinect Sensor
	if (m_pKinectSensor)
	{
		m_pKinectSensor->Close();
	}

	SafeRelease(m_pKinectSensor);

	info.GetReturnValue().Set(true);
}

NAUV_WORK_CB(ColorProgress_) {
	Nan::HandleScope scope;
	uv_mutex_lock(&m_mColorReaderMutex);
	if(m_pColorReaderCallback != NULL)
	{
		//reuse the existing buffer - data is already in there
		v8::Local<v8::Object> v8ColorPixels = Nan::New(m_persistentColorPixels);
		Local<Value> argv[] = {
			v8ColorPixels
		};
		m_pColorReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mColorReaderMutex);
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

void ColorReaderThreadLoop(void *arg)
{
	while(1)
	{

		IColorFrame* pColorFrame = NULL;
		HRESULT hr;

		//lock the mutex to get access to the frame reader
		uv_mutex_lock(&m_mColorReaderMutex);
		if(!m_bColorThreadRunning)
		{
			uv_mutex_unlock(&m_mColorReaderMutex);
			break;
		}
		hr = m_pColorFrameReader->AcquireLatestFrame(&pColorFrame);
		uv_mutex_unlock(&m_mColorReaderMutex);

		if (SUCCEEDED(hr))
		{
			processColorFrameData(pColorFrame);

			if (SUCCEEDED(hr))
			{
				//we have everything ready for our main loop, lock the mutex & copy the data to global memory
				uv_mutex_lock(&m_mColorReaderMutex);
				m_fColorHorizontalFieldOfViewV8 = m_fColorHorizontalFieldOfView;
				m_fColorVerticalFieldOfViewV8 = m_fColorVerticalFieldOfView;
				m_fColorDiagonalFieldOfViewV8 = m_fColorDiagonalFieldOfView;
				//copy into buffer for V8
				memcpy(m_pColorPixelsV8, m_pColorPixels, cColorWidth * cColorHeight * sizeof(RGBQUAD));
				//unlock the mutex again
				uv_mutex_unlock(&m_mColorReaderMutex);
				//notify event loop
				uv_async_send(&m_aColorAsync);
			}

		}
		SafeRelease(pColorFrame);
	}
}


NAN_METHOD(OpenColorReaderFunction)
{
	uv_mutex_lock(&m_mColorReaderMutex);
	if(m_pColorReaderCallback)
	{
		delete m_pColorReaderCallback;
		m_pColorReaderCallback = NULL;
	}

	m_pColorReaderCallback = new Nan::Callback(info[0].As<Function>());

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
		uv_async_init(uv_default_loop(), &m_aColorAsync, ColorProgress_);
		uv_thread_create(&m_tColorThread, ColorReaderThreadLoop, NULL);
	}

	SafeRelease(pColorFrameSource);

	uv_mutex_unlock(&m_mColorReaderMutex);
	if (SUCCEEDED(hr))
	{
		info.GetReturnValue().Set(true);
	}
	else
	{
		info.GetReturnValue().Set(false);
	}
}

NAN_METHOD(CloseColorReaderFunction)
{
	stopReader(&m_mColorReaderMutex, &m_bColorThreadRunning, &m_tColorThread, (uv_handle_t*) &m_aColorAsync, m_pColorFrameReader);
	info.GetReturnValue().Set(true);
}

NAUV_WORK_CB(InfraredProgress_)
{
	Nan::HandleScope scope;
	uv_mutex_lock(&m_mInfraredReaderMutex);
	if(m_pInfraredReaderCallback != NULL)
	{
		//reuse the existing buffer
		v8::Local<v8::Object> v8InfraredPixels = Nan::New(m_persistentInfraredPixels);
		Local<Value> argv[] = {
			v8InfraredPixels
		};
		m_pInfraredReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mInfraredReaderMutex);
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

void InfraredReaderThreadLoop(void *arg)
{
	while(1)
	{

		IInfraredFrame* pInfraredFrame = NULL;
		HRESULT hr;

		//lock the mutex to get access to the frame reader
		uv_mutex_lock(&m_mInfraredReaderMutex);
		if(!m_bInfraredThreadRunning)
		{
			uv_mutex_unlock(&m_mInfraredReaderMutex);
			break;
		}
		hr = m_pInfraredFrameReader->AcquireLatestFrame(&pInfraredFrame);
		uv_mutex_unlock(&m_mInfraredReaderMutex);

		if(SUCCEEDED(hr))
		{
			hr = processInfraredFrameData(pInfraredFrame);

			if (SUCCEEDED(hr))
			{
				//we have everything ready for our main loop, lock the mutex & copy the data to global memory
				uv_mutex_lock(&m_mInfraredReaderMutex);
				m_fInfraredHorizontalFieldOfViewV8 = m_fInfraredHorizontalFieldOfView;
				m_fInfraredVerticalFieldOfViewV8 = m_fInfraredVerticalFieldOfView;
				m_fInfraredDiagonalFieldOfViewV8 = m_fInfraredDiagonalFieldOfView;
				//copy into buffer for V8
				memcpy(m_pInfraredPixelsV8, m_pInfraredPixels, cInfraredWidth * cInfraredHeight);
				//unlock the mutex again
				uv_mutex_unlock(&m_mInfraredReaderMutex);
				//notify event loop
				uv_async_send(&m_aInfraredAsync);
			}
		}
		SafeRelease(pInfraredFrame);
	}
}


NAN_METHOD(OpenInfraredReaderFunction)
{

	uv_mutex_lock(&m_mInfraredReaderMutex);
	if(m_pInfraredReaderCallback)
	{
		delete m_pInfraredReaderCallback;
		m_pInfraredReaderCallback = NULL;
	}

	m_pInfraredReaderCallback = new Nan::Callback(info[0].As<Function>());

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
		uv_async_init(uv_default_loop(), &m_aInfraredAsync, InfraredProgress_);
		uv_thread_create(&m_tInfraredThread, InfraredReaderThreadLoop, NULL);
	}

	SafeRelease(pInfraredFrameSource);

	uv_mutex_unlock(&m_mInfraredReaderMutex);
	if (SUCCEEDED(hr))
	{
		info.GetReturnValue().Set(true);
	}
	else
	{
		info.GetReturnValue().Set(false);
	}
}

NAN_METHOD(CloseInfraredReaderFunction)
{
	stopReader(&m_mInfraredReaderMutex, &m_bInfraredThreadRunning, &m_tInfraredThread, (uv_handle_t*) &m_aInfraredAsync, m_pInfraredFrameReader);
	info.GetReturnValue().Set(true);
}

NAUV_WORK_CB(LongExposureInfraredProgress_) {
	Nan::HandleScope scope;
	uv_mutex_lock(&m_mLongExposureInfraredReaderMutex);
	if(m_pLongExposureInfraredReaderCallback != NULL)
	{
		//reuse the existing buffer
		v8::Local<v8::Object> v8LongExposureInfraredPixels = Nan::New(m_persistentLongExposureInfraredPixels);
		Local<Value> argv[] = {
			v8LongExposureInfraredPixels
		};
		m_pLongExposureInfraredReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);
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

void LongExposureInfraredReaderThreadLoop(void *arg)
{
	while(1)
	{

		ILongExposureInfraredFrame* pLongExposureInfraredFrame = NULL;
		HRESULT hr;

		//lock the mutex to get access to the frame reader
		uv_mutex_lock(&m_mLongExposureInfraredReaderMutex);
		if(!m_bLongExposureInfraredThreadRunning)
		{
			uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);
			break;
		}
		hr = m_pLongExposureInfraredFrameReader->AcquireLatestFrame(&pLongExposureInfraredFrame);
		uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);

		if (SUCCEEDED(hr))
		{
			hr = processLongExposureInfraredFrameData(pLongExposureInfraredFrame);

			if (SUCCEEDED(hr))
			{
				//we have everything ready for our main loop, lock the mutex & copy the data to global memory
				uv_mutex_lock(&m_mLongExposureInfraredReaderMutex);
				m_fLongExposureInfraredHorizontalFieldOfViewV8 = m_fLongExposureInfraredHorizontalFieldOfView;
				m_fLongExposureInfraredVerticalFieldOfViewV8 = m_fLongExposureInfraredVerticalFieldOfView;
				m_fLongExposureInfraredDiagonalFieldOfViewV8 = m_fLongExposureInfraredDiagonalFieldOfView;
				//copy into buffer for V8
				memcpy(m_pLongExposureInfraredPixelsV8, m_pLongExposureInfraredPixels, cLongExposureInfraredWidth * cLongExposureInfraredHeight);
				//unlock the mutex again
				uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);
				//notify event loop
				uv_async_send(&m_aLongExposureInfraredAsync);
			}
		}
		SafeRelease(pLongExposureInfraredFrame);
	}
}


NAN_METHOD(OpenLongExposureInfraredReaderFunction)
{

	uv_mutex_lock(&m_mLongExposureInfraredReaderMutex);
	if(m_pLongExposureInfraredReaderCallback)
	{
		delete m_pLongExposureInfraredReaderCallback;
		m_pLongExposureInfraredReaderCallback = NULL;
	}

	m_pLongExposureInfraredReaderCallback = new Nan::Callback(info[0].As<Function>());

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
		uv_async_init(uv_default_loop(), &m_aLongExposureInfraredAsync, LongExposureInfraredProgress_);
		uv_thread_create(&m_tLongExposureInfraredThread, LongExposureInfraredReaderThreadLoop, NULL);
	}

	SafeRelease(pLongExposureInfraredFrameSource);

	uv_mutex_unlock(&m_mLongExposureInfraredReaderMutex);
	if (SUCCEEDED(hr))
	{
		info.GetReturnValue().Set(true);
	}
	else
	{
		info.GetReturnValue().Set(false);
	}
}

NAN_METHOD(CloseLongExposureInfraredReaderFunction)
{
	stopReader(&m_mLongExposureInfraredReaderMutex, &m_bLongExposureInfraredThreadRunning, &m_tLongExposureInfraredThread, (uv_handle_t*) &m_aLongExposureInfraredAsync, m_pLongExposureInfraredFrameReader);
	info.GetReturnValue().Set(true);
}

NAUV_WORK_CB(DepthProgress_) {
	Nan::HandleScope scope;
	uv_mutex_lock(&m_mDepthReaderMutex);
	if(m_pDepthReaderCallback != NULL)
	{
		//reuse the existing buffer
		v8::Local<v8::Object> v8DepthPixels = Nan::New(m_persistentDepthPixels);
		Local<Value> argv[] = {
			v8DepthPixels
		};
		m_pDepthReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mDepthReaderMutex);
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

void DepthReaderThreadLoop(void *arg)
{
	while(1)
	{

		IDepthFrame* pDepthFrame = NULL;
		HRESULT hr;

		//lock the mutex to get access to the frame reader
		uv_mutex_lock(&m_mDepthReaderMutex);
		if(!m_bDepthThreadRunning)
		{
			uv_mutex_unlock(&m_mDepthReaderMutex);
			break;
		}
		hr = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);
		uv_mutex_unlock(&m_mDepthReaderMutex);

		if (SUCCEEDED(hr))
		{
			hr = processDepthFrameData(pDepthFrame);

			if (SUCCEEDED(hr))
			{
				//we have everything ready for our main loop, lock the mutex & copy the data to global memory
				uv_mutex_lock(&m_mDepthReaderMutex);
				m_fDepthHorizontalFieldOfViewV8 = m_fDepthHorizontalFieldOfView;
				m_fDepthVerticalFieldOfViewV8 = m_fDepthVerticalFieldOfView;
				m_fDepthDiagonalFieldOfViewV8 = m_fDepthDiagonalFieldOfView;
				//copy into buffer for V8
				memcpy(m_pDepthPixelsV8, m_pDepthPixels, cDepthWidth * cDepthHeight);
				//unlock the mutex again
				uv_mutex_unlock(&m_mDepthReaderMutex);
				//notify event loop
				uv_async_send(&m_aDepthAsync);
			}
		}
		SafeRelease(pDepthFrame);
	}
}


NAN_METHOD(OpenDepthReaderFunction)
{

	uv_mutex_lock(&m_mDepthReaderMutex);
	if(m_pDepthReaderCallback)
	{
		delete m_pDepthReaderCallback;
		m_pDepthReaderCallback = NULL;
	}

	m_pDepthReaderCallback = new Nan::Callback(info[0].As<Function>());

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
		uv_async_init(uv_default_loop(), &m_aDepthAsync, DepthProgress_);
		uv_thread_create(&m_tDepthThread, DepthReaderThreadLoop, NULL);
	}

	SafeRelease(pDepthFrameSource);

	uv_mutex_unlock(&m_mDepthReaderMutex);
	if (SUCCEEDED(hr))
	{
		info.GetReturnValue().Set(true);
	}
	else
	{
		info.GetReturnValue().Set(false);
	}
}

NAN_METHOD(CloseDepthReaderFunction)
{
	stopReader(&m_mDepthReaderMutex, &m_bDepthThreadRunning, &m_tDepthThread, (uv_handle_t*) &m_aDepthAsync, m_pDepthFrameReader);
	info.GetReturnValue().Set(true);
}

NAUV_WORK_CB(RawDepthProgress_) {
	Nan::HandleScope scope;
	uv_mutex_lock(&m_mRawDepthReaderMutex);
	if(m_pRawDepthReaderCallback != NULL)
	{
		//reuse the existing buffer
		v8::Local<v8::Object> v8RawDepthValues = Nan::New(m_persistentRawDepthValues);
		Local<Value> argv[] = {
			v8RawDepthValues
		};
		m_pRawDepthReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mRawDepthReaderMutex);
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

void RawDepthReaderThreadLoop(void *arg)
{
	while(1)
	{

		IDepthFrame* pDepthFrame = NULL;
		HRESULT hr;

		//lock the mutex to get access to the frame reader
		uv_mutex_lock(&m_mRawDepthReaderMutex);
		if(!m_bRawDepthThreadRunning)
		{
			uv_mutex_unlock(&m_mRawDepthReaderMutex);
			break;
		}
		hr = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);
		uv_mutex_unlock(&m_mRawDepthReaderMutex);

		if (SUCCEEDED(hr))
		{
			hr = processRawDepthFrameData(pDepthFrame);

			if (SUCCEEDED(hr))
			{
				//we have everything ready for our main loop, lock the mutex & copy the data to global memory
				uv_mutex_lock(&m_mRawDepthReaderMutex);
				m_fDepthHorizontalFieldOfViewV8 = m_fDepthHorizontalFieldOfView;
				m_fDepthVerticalFieldOfViewV8 = m_fDepthVerticalFieldOfView;
				m_fDepthDiagonalFieldOfViewV8 = m_fDepthDiagonalFieldOfView;
				//copy into buffer for V8
				memcpy(m_pRawDepthValuesV8, m_pRawDepthValues, cDepthWidth * cDepthHeight * sizeof(UINT16));
				//unlock the mutex again
				uv_mutex_unlock(&m_mRawDepthReaderMutex);
				//notify event loop
				uv_async_send(&m_aRawDepthAsync);
			}
		}
		SafeRelease(pDepthFrame);
	}
}

NAN_METHOD(OpenRawDepthReaderFunction)
{

	uv_mutex_lock(&m_mRawDepthReaderMutex);
	if(m_pRawDepthReaderCallback)
	{
		delete m_pRawDepthReaderCallback;
		m_pRawDepthReaderCallback = NULL;
	}

	m_pRawDepthReaderCallback = new Nan::Callback(info[0].As<Function>());

	HRESULT hr;
	IDepthFrameSource* pDepthFrameSource = NULL;

	hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
	if (SUCCEEDED(hr))
	{
		hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
	}
	if (SUCCEEDED(hr))
	{
		m_bRawDepthThreadRunning = true;
		uv_async_init(uv_default_loop(), &m_aRawDepthAsync, RawDepthProgress_);
		uv_thread_create(&m_tRawDepthThread, RawDepthReaderThreadLoop, NULL);
	}

	SafeRelease(pDepthFrameSource);

	uv_mutex_unlock(&m_mRawDepthReaderMutex);
	if (SUCCEEDED(hr))
	{
		info.GetReturnValue().Set(true);
	}
	else
	{
		info.GetReturnValue().Set(false);
	}
}

NAN_METHOD(CloseRawDepthReaderFunction)
{
	stopReader(&m_mRawDepthReaderMutex, &m_bRawDepthThreadRunning, &m_tRawDepthThread, (uv_handle_t*) &m_aRawDepthAsync, m_pDepthFrameReader);
	info.GetReturnValue().Set(true);
}

v8::Local<v8::Object> getV8BodyFrame_()
{
	Nan::EscapableHandleScope scope;
	v8::Local<v8::Object> v8BodyResult = Nan::New<v8::Object>();

	//bodies
	v8::Local<v8::Array> v8bodies = Nan::New<v8::Array>(BODY_COUNT);
	for(int i = 0; i < BODY_COUNT; i++)
	{
		//create a body object
		v8::Local<v8::Object> v8body = Nan::New<v8::Object>();

		Nan::Set(v8body, Nan::New<String>("bodyIndex").ToLocalChecked(), Nan::New<v8::Number>(i));
		Nan::Set(v8body, Nan::New<v8::String>("tracked").ToLocalChecked(), Nan::New<v8::Boolean>(m_jsBodyFrameV8.bodies[i].tracked));
		if(m_jsBodyFrameV8.bodies[i].tracked)
		{
			Nan::Set(v8body, Nan::New<v8::String>("trackingId").ToLocalChecked(), Nan::New<v8::String>(std::to_string(m_jsBodyFrameV8.bodies[i].trackingId)).ToLocalChecked());
			//hand states
			Nan::Set(v8body, Nan::New<v8::String>("leftHandState").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].leftHandState));
			Nan::Set(v8body, Nan::New<v8::String>("rightHandState").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].rightHandState));
			v8::Local<v8::Array> v8joints = Nan::New<v8::Array>();
			//joints
			for (int j = 0; j < _countof(m_jsBodyFrameV8.bodies[i].joints); ++j)
			{
				v8::Local<v8::Object> v8joint = Nan::New<v8::Object>();
				Nan::Set(v8joint, Nan::New<v8::String>("depthX").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].depthX));
				Nan::Set(v8joint, Nan::New<v8::String>("depthY").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].depthY));
				Nan::Set(v8joint, Nan::New<v8::String>("colorX").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].colorX));
				Nan::Set(v8joint, Nan::New<v8::String>("colorY").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].colorY));
				Nan::Set(v8joint, Nan::New<v8::String>("cameraX").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].cameraX));
				Nan::Set(v8joint, Nan::New<v8::String>("cameraY").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].cameraY));
				Nan::Set(v8joint, Nan::New<v8::String>("cameraZ").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].cameraZ));
				//orientation
				Nan::Set(v8joint, Nan::New<v8::String>("orientationX").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].orientationX));
				Nan::Set(v8joint, Nan::New<v8::String>("orientationY").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].orientationY));
				Nan::Set(v8joint, Nan::New<v8::String>("orientationZ").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].orientationZ));
				Nan::Set(v8joint, Nan::New<v8::String>("orientationW").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].orientationW));
				//body ground
				if(m_jsBodyFrameV8.bodies[i].joints[j].hasFloorData)
				{
					Nan::Set(v8joint, Nan::New<v8::String>("floorDepthX").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].floorDepthX));
					Nan::Set(v8joint, Nan::New<v8::String>("floorDepthY").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].floorDepthY));
					Nan::Set(v8joint, Nan::New<v8::String>("floorColorX").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].floorColorX));
					Nan::Set(v8joint, Nan::New<v8::String>("floorColorY").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].floorColorY));
					Nan::Set(v8joint, Nan::New<v8::String>("floorCameraX").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].floorCameraX));
					Nan::Set(v8joint, Nan::New<v8::String>("floorCameraY").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].floorCameraY));
					Nan::Set(v8joint, Nan::New<v8::String>("floorCameraZ").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].floorCameraZ));
				}
				Nan::Set(v8joints, Nan::New<v8::Number>(m_jsBodyFrameV8.bodies[i].joints[j].jointType), v8joint);
			}
			Nan::Set(v8body, Nan::New<v8::String>("joints").ToLocalChecked(), v8joints);
		}
		Nan::Set(v8bodies, i, v8body);
	}
	Nan::Set(v8BodyResult, Nan::New<v8::String>("bodies").ToLocalChecked(), v8bodies);

	//floor plane
	if(m_jsBodyFrameV8.hasFloorClipPlane) {
		v8::Local<v8::Object> v8FloorClipPlane = Nan::New<v8::Object>();
		Nan::Set(v8FloorClipPlane, Nan::New<v8::String>("x").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.floorClipPlaneX));
		Nan::Set(v8FloorClipPlane, Nan::New<v8::String>("y").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.floorClipPlaneY));
		Nan::Set(v8FloorClipPlane, Nan::New<v8::String>("z").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.floorClipPlaneZ));
		Nan::Set(v8FloorClipPlane, Nan::New<v8::String>("w").ToLocalChecked(), Nan::New<v8::Number>(m_jsBodyFrameV8.floorClipPlaneW));
		Nan::Set(v8BodyResult, Nan::New<v8::String>("floorClipPlane").ToLocalChecked(), v8FloorClipPlane);
	}

	return scope.Escape(v8BodyResult);
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

NAUV_WORK_CB(BodyProgress_)
{
	Nan::HandleScope scope;
	uv_mutex_lock(&m_mBodyReaderMutex);
	if(m_pBodyReaderCallback != NULL)
	{
		v8::Local<v8::Object> v8BodyResult = getV8BodyFrame_();

		v8::Local<v8::Value> argv[] = {
			v8BodyResult
		};
		m_pBodyReaderCallback->Call(1, argv);
	}
	uv_mutex_unlock(&m_mBodyReaderMutex);
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

void BodyReaderThreadLoop(void *arg)
{
	while(1)
	{

		IBodyFrame* pBodyFrame = NULL;
		HRESULT hr;

		//lock the mutex to get access to the frame reader
		uv_mutex_lock(&m_mBodyReaderMutex);
		if(!m_bBodyThreadRunning)
		{
			uv_mutex_unlock(&m_mBodyReaderMutex);
			break;
		}
		hr = m_pBodyFrameReader->AcquireLatestFrame(&pBodyFrame);
		uv_mutex_unlock(&m_mBodyReaderMutex);

		if(SUCCEEDED(hr))
		{
			hr = processBodyFrameData(pBodyFrame);
			if (SUCCEEDED(hr))
			{
				//we have everything ready for our main loop, lock the mutex & copy the data to global memory
				uv_mutex_lock(&m_mBodyReaderMutex);
				//copy into object for V8
				memcpy(&m_jsBodyFrameV8, &m_jsBodyFrame, sizeof(JSBodyFrame));
				//unlock the mutex again
				uv_mutex_unlock(&m_mBodyReaderMutex);
				//notify event loop
				uv_async_send(&m_aBodyAsync);
			}
		}
		SafeRelease(pBodyFrame);
	}
}


NAN_METHOD(OpenBodyReaderFunction)
{

	uv_mutex_lock(&m_mBodyReaderMutex);
	if(m_pBodyReaderCallback)
	{
		delete m_pBodyReaderCallback;
		m_pBodyReaderCallback = NULL;
	}

	m_pBodyReaderCallback = new Nan::Callback(info[0].As<Function>());

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
		uv_async_init(uv_default_loop(), &m_aBodyAsync, BodyProgress_);
		uv_thread_create(&m_tBodyThread, BodyReaderThreadLoop, NULL);
	}

	SafeRelease(pBodyFrameSource);

	uv_mutex_unlock(&m_mBodyReaderMutex);
	if (SUCCEEDED(hr))
	{
		info.GetReturnValue().Set(true);
	}
	else
	{
		info.GetReturnValue().Set(false);
	}
}

NAN_METHOD(CloseBodyReaderFunction)
{
	stopReader(&m_mBodyReaderMutex, &m_bBodyThreadRunning, &m_tBodyThread, (uv_handle_t*) &m_aBodyAsync, m_pBodyFrameReader);
	info.GetReturnValue().Set(true);
}

NAUV_WORK_CB(MultiSourceProgress_) {
	Nan::HandleScope scope;
	uv_mutex_lock(&m_mMultiSourceReaderMutex);
	if(m_pMultiSourceReaderCallback != NULL)
	{
		//build object for callback
		v8::Local<v8::Object> v8Result = Nan::New<v8::Object>();

		if(NodeKinect2FrameTypes::FrameTypes_Color & m_enabledFrameTypes)
		{
			//reuse the existing buffer
			v8::Local<v8::Object> v8ColorPixels = Nan::New(m_persistentColorPixels);

			v8::Local<v8::Object> v8ColorResult = Nan::New<v8::Object>();
			Nan::Set(v8ColorResult, Nan::New<v8::String>("buffer").ToLocalChecked(), v8ColorPixels);

			//field of view
			Nan::Set(v8ColorResult, Nan::New<v8::String>("horizontalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fColorHorizontalFieldOfViewV8));
			Nan::Set(v8ColorResult, Nan::New<v8::String>("verticalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fColorVerticalFieldOfViewV8));
			Nan::Set(v8ColorResult, Nan::New<v8::String>("diagonalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fColorDiagonalFieldOfViewV8));

			Nan::Set(v8Result, Nan::New<v8::String>("color").ToLocalChecked(), v8ColorResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_Depth & m_enabledFrameTypes)
		{
			//reuse the existing buffer
			v8::Local<v8::Object> v8DepthPixels = Nan::New(m_persistentDepthPixels);

			v8::Local<v8::Object> v8DepthResult = Nan::New<v8::Object>();
			Nan::Set(v8DepthResult, Nan::New<v8::String>("buffer").ToLocalChecked(), v8DepthPixels);

			//field of view
			Nan::Set(v8DepthResult, Nan::New<v8::String>("horizontalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fDepthHorizontalFieldOfViewV8));
			Nan::Set(v8DepthResult, Nan::New<v8::String>("verticalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fDepthVerticalFieldOfViewV8));
			Nan::Set(v8DepthResult, Nan::New<v8::String>("diagonalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fDepthDiagonalFieldOfViewV8));

			Nan::Set(v8Result, Nan::New<v8::String>("depth").ToLocalChecked(), v8DepthResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_RawDepth & m_enabledFrameTypes)
		{
			//reuse the existing buffer
			v8::Local<v8::Object> v8RawDepthValues = Nan::New(m_persistentRawDepthValues);

			v8::Local<v8::Object> v8RawDepthResult = Nan::New<v8::Object>();
			Nan::Set(v8RawDepthResult, Nan::New<v8::String>("buffer").ToLocalChecked(), v8RawDepthValues);

			//field of view
			Nan::Set(v8RawDepthResult, Nan::New<v8::String>("horizontalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fDepthHorizontalFieldOfViewV8));
			Nan::Set(v8RawDepthResult, Nan::New<v8::String>("verticalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fDepthVerticalFieldOfViewV8));
			Nan::Set(v8RawDepthResult, Nan::New<v8::String>("diagonalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fDepthDiagonalFieldOfViewV8));

			Nan::Set(v8Result, Nan::New<v8::String>("rawDepth").ToLocalChecked(), v8RawDepthResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_DepthColor & m_enabledFrameTypes)
		{
			//reuse the existing buffer
			v8::Local<v8::Object> v8DepthColorPixels = Nan::New(m_persistentDepthColorPixels);

			v8::Local<v8::Object> v8DepthColorResult = Nan::New<v8::Object>();
			Nan::Set(v8DepthColorResult, Nan::New<v8::String>("buffer").ToLocalChecked(), v8DepthColorPixels);

			Nan::Set(v8Result, Nan::New<v8::String>("depthColor").ToLocalChecked(), v8DepthColorResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_Body & m_enabledFrameTypes)
		{
			v8::Local<v8::Object> v8BodyResult = getV8BodyFrame_();

			Nan::Set(v8Result, Nan::New<v8::String>("body").ToLocalChecked(), v8BodyResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_BodyIndexColor & m_enabledFrameTypes)
		{
			v8::Local<v8::Object> v8BodyIndexColorResult = Nan::New<v8::Object>();

			//persistent handle of array with color buffers
			v8::Local<v8::Object> v8BodyIndexColorPixels = Nan::New(m_persistentBodyIndexColorPixels);

			v8::Local<v8::Array> v8bodies = Nan::New<v8::Array>(BODY_COUNT);
			for(int i = 0; i < BODY_COUNT; i++)
			{
				v8::Local<v8::Object> v8body = Nan::New<v8::Object>();
				Nan::Set(v8body, Nan::New<v8::String>("bodyIndex").ToLocalChecked(), Nan::New<v8::Number>(i));
				if(m_jsBodyFrameV8.bodies[i].hasPixels && m_trackPixelsForBodyIndexV8[i]) {
					//reuse the existing buffer
					v8::Local<v8::Value> v8ColorPixels = v8BodyIndexColorPixels->Get(i);
					Nan::Set(v8body, Nan::New<v8::String>("buffer").ToLocalChecked(), v8ColorPixels);
				}
				Nan::Set(v8bodies, i, v8body);
			}
			Nan::Set(v8BodyIndexColorResult, Nan::New<v8::String>("bodies").ToLocalChecked(), v8bodies);

			//field of view pf color camera
			Nan::Set(v8BodyIndexColorResult, Nan::New<v8::String>("horizontalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fColorHorizontalFieldOfViewV8));
			Nan::Set(v8BodyIndexColorResult, Nan::New<v8::String>("verticalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fColorVerticalFieldOfViewV8));
			Nan::Set(v8BodyIndexColorResult, Nan::New<v8::String>("diagonalFieldOfView").ToLocalChecked(), Nan::New<v8::Number>(m_fColorDiagonalFieldOfViewV8));

			Nan::Set(v8Result, Nan::New<v8::String>("bodyIndexColor").ToLocalChecked(), v8BodyIndexColorResult);
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
		HRESULT hr;
		IMultiSourceFrame* pMultiSourceFrame = NULL;

		//lock the mutex to get access to the frame reader
		uv_mutex_lock(&m_mMultiSourceReaderMutex);
		if(!m_bMultiSourceThreadRunning)
		{
			uv_mutex_unlock(&m_mMultiSourceReaderMutex);
			break;
		}
		hr = m_pMultiSourceFrameReader->AcquireLatestFrame(&pMultiSourceFrame);
		uv_mutex_unlock(&m_mMultiSourceReaderMutex);

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
			FrameTypes_DepthColor	= 0x800
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

			//todo: infrared
			//todo: long exposure infrared

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

		if (SUCCEEDED(hr))
		{
			//we have everything ready for our main loop, lock the mutex & copy the data to global memory
			uv_mutex_lock(&m_mMultiSourceReaderMutex);
			//field of views are also included in body index pixel frames, so always set them
			m_fColorHorizontalFieldOfViewV8 = m_fColorHorizontalFieldOfView;
			m_fColorVerticalFieldOfViewV8 = m_fColorVerticalFieldOfView;
			m_fColorDiagonalFieldOfViewV8 = m_fColorDiagonalFieldOfView;
			m_fDepthHorizontalFieldOfViewV8 = m_fDepthHorizontalFieldOfView;
			m_fDepthVerticalFieldOfViewV8 = m_fDepthVerticalFieldOfView;
			m_fDepthDiagonalFieldOfViewV8 = m_fDepthDiagonalFieldOfView;
			if(NodeKinect2FrameTypes::FrameTypes_Color & m_enabledFrameTypes)
			{
				//copy into buffer for V8
				memcpy(m_pColorPixelsV8, m_pColorPixels, cColorWidth * cColorHeight * sizeof(RGBQUAD));
			}
			//depth image
			if(NodeKinect2FrameTypes::FrameTypes_Depth & m_enabledFrameTypes)
			{
				//copy into buffer for V8
				memcpy(m_pDepthPixelsV8, m_pDepthPixels, cDepthWidth * cDepthHeight);
			}
			//raw depth
			if(NodeKinect2FrameTypes::FrameTypes_RawDepth & m_enabledFrameTypes)
			{
				//copy into buffer for V8
				memcpy(m_pRawDepthValuesV8, m_pRawDepthValues, cDepthWidth * cDepthHeight * sizeof(UINT16));
			}
			//depth colors
			if(NodeKinect2FrameTypes::FrameTypes_DepthColor & m_enabledFrameTypes)
			{
				//copy into buffer for V8
				memcpy(m_pDepthColorPixelsV8, m_pDepthColorPixels, cDepthWidth * cDepthHeight * sizeof(RGBQUAD));
			}
			if(NodeKinect2FrameTypes::FrameTypes_Body & m_enabledFrameTypes)
			{
				//copy into object for V8
				memcpy(&m_jsBodyFrameV8, &m_jsBodyFrame, sizeof(JSBodyFrame));
			}
			if(NodeKinect2FrameTypes::FrameTypes_BodyIndexColor & m_enabledFrameTypes)
			{
				//copy into object for V8
				memcpy(m_pBodyIndexColorPixelsV8, m_pBodyIndexColorPixels, BODY_COUNT * cColorWidth * cColorHeight * sizeof(RGBQUAD));
			}
			//unlock the mutex
			uv_mutex_unlock(&m_mMultiSourceReaderMutex);
			//notify event loop
			uv_async_send(&m_aMultiSourceAsync);
		}

		SafeRelease(pMultiSourceFrame);
	}
}


NAN_METHOD(OpenMultiSourceReaderFunction)
{
	uv_mutex_lock(&m_mMultiSourceReaderMutex);
	if(m_pMultiSourceReaderCallback)
	{
		delete m_pMultiSourceReaderCallback;
		m_pMultiSourceReaderCallback = NULL;
	}

	Local<Object> jsOptions = info[0].As<Object>();
	Local<Function> jsCallback = jsOptions->Get(Nan::New<v8::String>("callback").ToLocalChecked()).As<Function>();
	m_enabledFrameTypes = static_cast<unsigned long>(jsOptions->Get(Nan::New<v8::String>("frameTypes").ToLocalChecked()).As<Number>()->Value());

	m_includeJointFloorData = false;

	Nan::MaybeLocal<v8::Value> v8IncludeJointFloorData = Nan::Get(jsOptions, Nan::New<v8::String>("includeJointFloorData").ToLocalChecked());
	if(!v8IncludeJointFloorData.IsEmpty())
	{
		m_includeJointFloorData = Nan::To<bool>(v8IncludeJointFloorData.ToLocalChecked()).FromJust();
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

	m_pMultiSourceReaderCallback = new Nan::Callback(jsCallback);

	HRESULT hr = m_pKinectSensor->OpenMultiSourceFrameReader(m_enabledFrameSourceTypes, &m_pMultiSourceFrameReader);

	if (SUCCEEDED(hr))
	{
		m_bMultiSourceThreadRunning = true;
		uv_async_init(uv_default_loop(), &m_aMultiSourceAsync, MultiSourceProgress_);
		uv_thread_create(&m_tMultiSourceThread, MultiSourceReaderThreadLoop, NULL);
	}

	uv_mutex_unlock(&m_mMultiSourceReaderMutex);
	if (SUCCEEDED(hr))
	{
		info.GetReturnValue().Set(true);
	}
	else
	{
		info.GetReturnValue().Set(false);
	}
}

NAN_METHOD(CloseMultiSourceReaderFunction)
{
	stopReader(&m_mMultiSourceReaderMutex, &m_bMultiSourceThreadRunning, &m_tMultiSourceThread, (uv_handle_t*) &m_aMultiSourceAsync, m_pMultiSourceFrameReader);
	info.GetReturnValue().Set(true);
}

NAN_METHOD(TrackPixelsForBodyIndicesFunction)
{
	uv_mutex_lock(&m_mMultiSourceReaderMutex);
	int i;
	for(i = 0; i < BODY_COUNT; i++)
	{
		m_trackPixelsForBodyIndexV8[i] = false;
	}
	Local<Object> jsOptions = info[0].As<Object>();
	if(jsOptions->IsArray())
	{
		Local<Array> jsBodyIndices = info[0].As<Array>();
		int len = jsBodyIndices->Length();
		for(i = 0; i < len; i++)
		{
			uint32_t index = static_cast<uint32_t>(jsBodyIndices->Get(i).As<Number>()->Value());
			index = min(index, BODY_COUNT - 1);
			m_trackPixelsForBodyIndexV8[index] = true;
		}
	}

	uv_mutex_unlock(&m_mMultiSourceReaderMutex);
	info.GetReturnValue().Set(true);
}

NAN_MODULE_INIT(Init)
{
	//color
	uv_mutex_init(&m_mColorReaderMutex);
	m_persistentColorPixels.Reset<v8::Object>(Nan::NewBuffer((char *)m_pColorPixelsV8, cColorWidth * cColorHeight * sizeof(RGBQUAD)).ToLocalChecked());

	//infrared
	uv_mutex_init(&m_mInfraredReaderMutex);
	m_persistentInfraredPixels.Reset<v8::Object>(Nan::NewBuffer(m_pInfraredPixelsV8, cInfraredWidth * cInfraredHeight).ToLocalChecked());

	//long exposure infrared
	uv_mutex_init(&m_mLongExposureInfraredReaderMutex);
	m_persistentLongExposureInfraredPixels.Reset<v8::Object>(Nan::NewBuffer(m_pLongExposureInfraredPixelsV8, cLongExposureInfraredWidth * cLongExposureInfraredHeight).ToLocalChecked());

	//depth
	uv_mutex_init(&m_mDepthReaderMutex);
	m_persistentDepthPixels.Reset<v8::Object>(Nan::NewBuffer(m_pDepthPixelsV8, cDepthWidth * cDepthHeight).ToLocalChecked());

	//raw depth
	uv_mutex_init(&m_mRawDepthReaderMutex);
	m_persistentRawDepthValues.Reset<v8::Object>(Nan::NewBuffer((char *)m_pRawDepthValuesV8, cDepthWidth * cDepthHeight * sizeof(UINT16)).ToLocalChecked());

	//body
	uv_mutex_init(&m_mBodyReaderMutex);

	//multisource
	uv_mutex_init(&m_mMultiSourceReaderMutex);
	v8::Local<v8::Object> v8BodyIndexColorPixels = Nan::New<v8::Object>();
	for(int i = 0; i < BODY_COUNT; i++)
	{
		Nan::Set(v8BodyIndexColorPixels, i, Nan::NewBuffer((char *)m_pBodyIndexColorPixelsV8[i], cColorWidth * cColorHeight * sizeof(RGBQUAD)).ToLocalChecked());
	}
	m_persistentBodyIndexColorPixels.Reset<v8::Object>(v8BodyIndexColorPixels);
	//Depth colors
	m_persistentDepthColorPixels.Reset<v8::Object>(Nan::NewBuffer((char *)m_pDepthColorPixelsV8, cDepthWidth * cDepthHeight * sizeof(RGBQUAD)).ToLocalChecked());

	Nan::Set(target, Nan::New<String>("open").ToLocalChecked(),
		Nan::New<FunctionTemplate>(OpenFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("close").ToLocalChecked(),
		Nan::New<FunctionTemplate>(CloseFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("openColorReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(OpenColorReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("closeColorReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(CloseColorReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("openInfraredReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(OpenInfraredReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("closeInfraredReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(CloseInfraredReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("openLongExposureInfraredReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(OpenLongExposureInfraredReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("closeLongExposureInfraredReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(CloseLongExposureInfraredReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("openDepthReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(OpenDepthReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("closeDepthReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(CloseDepthReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("openRawDepthReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(OpenRawDepthReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("closeRawDepthReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(CloseRawDepthReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("openBodyReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(OpenBodyReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("closeBodyReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(CloseBodyReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("openMultiSourceReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(OpenMultiSourceReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("closeMultiSourceReader").ToLocalChecked(),
		Nan::New<FunctionTemplate>(CloseMultiSourceReaderFunction)->GetFunction());
	Nan::Set(target, Nan::New<String>("trackPixelsForBodyIndices").ToLocalChecked(),
		Nan::New<FunctionTemplate>(TrackPixelsForBodyIndicesFunction)->GetFunction());
}

NODE_MODULE(kinect2, Init)
