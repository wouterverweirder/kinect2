#include <nan.h>
#include <iostream>
#include "uv.h"
#include "Kinect.h"
#include "Globals.h"
#include "Structs.h"
#include "BodyFrameWorker.h"
#include "DepthFrameWorker.h"
#include "ColorFrameWorker.h"
#include "InfraredFrameWorker.h"
#include "LongExposureInfraredFrameWorker.h"
#include "MultiSourceFrameWorker.h"

using namespace v8;

IKinectSensor*											m_pKinectSensor;
ICoordinateMapper*									m_pCoordinateMapper;
IBodyFrameReader*										m_pBodyFrameReader;
IDepthFrameReader*									m_pDepthFrameReader;
IColorFrameReader*  								m_pColorFrameReader;
IInfraredFrameReader* 							m_pInfraredFrameReader;
ILongExposureInfraredFrameReader* 	m_pLongExposureInfraredFrameReader;
IMultiSourceFrameReader*						m_pMultiSourceFrameReader;

const int 							cDepthWidth  = 512;
const int 							cDepthHeight = 424;
char*										m_pDepthPixels = new char[cDepthWidth * cDepthHeight];

const int 							cColorWidth  = 1920;
const int 							cColorHeight = 1080;
RGBQUAD*								m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];

const int 							cInfraredWidth  = 512;
const int 							cInfraredHeight = 424;
char*										m_pInfraredPixels = new char[cInfraredWidth * cInfraredHeight];

const int 							cLongExposureInfraredWidth  = 512;
const int 							cLongExposureInfraredHeight = 424;
char*										m_pLongExposureInfraredPixels = new char[cLongExposureInfraredWidth * cLongExposureInfraredHeight];

DepthSpacePoint*				m_pDepthCoordinatesForColor = new DepthSpacePoint[cColorWidth * cColorHeight];

RGBQUAD*								m_pBodyIndexColorPixels = new RGBQUAD[cColorWidth * cColorHeight];

//enabledFrameSourceTypes refers to the kinect SDK frame source types
DWORD										m_enabledFrameSourceTypes = 0;
//enabledFrameTypes is our own list of frame types
//this is the kinect SDK list + additional ones (body index in color space, etc...)
unsigned long						m_enabledFrameTypes = 0;

NanCallback*						m_pBodyReaderCallback;
NanCallback*						m_pDepthReaderCallback;
NanCallback*						m_pColorReaderCallback;
NanCallback*						m_pInfraredReaderCallback;
NanCallback*						m_pLongExposureInfraredReaderCallback;
NanCallback*						m_pMultiSourceReaderCallback;

uv_mutex_t							m_bodyReaderMutex;
uv_mutex_t							m_depthReaderMutex;
uv_mutex_t							m_colorReaderMutex;
uv_mutex_t							m_infraredReaderMutex;
uv_mutex_t							m_longExposureInfraredReaderMutex;
uv_mutex_t							m_multiSourceReaderMutex;

NAN_METHOD(OpenFunction);
NAN_METHOD(CloseFunction);

NAN_METHOD(OpenBodyReaderFunction);
NAN_METHOD(_BodyFrameArrived);
NAN_METHOD(CloseBodyReaderFunction);

NAN_METHOD(OpenDepthReaderFunction);
NAN_METHOD(_DepthFrameArrived);
NAN_METHOD(CloseDepthReaderFunction);

NAN_METHOD(OpenColorReaderFunction);
NAN_METHOD(_ColorFrameArrived);
NAN_METHOD(CloseColorReaderFunction);

NAN_METHOD(OpenInfraredReaderFunction);
NAN_METHOD(_InfraredFrameArrived);
NAN_METHOD(CloseInfraredReaderFunction);

NAN_METHOD(OpenLongExposureInfraredReaderFunction);
NAN_METHOD(_LongExposureInfraredFrameArrived);
NAN_METHOD(CloseLongExposureInfraredReaderFunction);

NAN_METHOD(OpenMultiSourceReaderFunction);
NAN_METHOD(_MultiSourceFrameArrived);
NAN_METHOD(CloseMultiSourceReaderFunction);

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

	uv_mutex_lock(&m_bodyReaderMutex);
	uv_mutex_lock(&m_depthReaderMutex);
	uv_mutex_lock(&m_colorReaderMutex);
	uv_mutex_lock(&m_infraredReaderMutex);
	uv_mutex_lock(&m_longExposureInfraredReaderMutex);
	uv_mutex_lock(&m_multiSourceReaderMutex);

	SafeRelease(m_pBodyFrameReader);
	SafeRelease(m_pDepthFrameReader);
	SafeRelease(m_pColorFrameReader);
	SafeRelease(m_pInfraredFrameReader);
	SafeRelease(m_pLongExposureInfraredFrameReader);
	SafeRelease(m_pMultiSourceFrameReader);

	// done with coordinate mapper
	SafeRelease(m_pCoordinateMapper);

	// close the Kinect Sensor
	if (m_pKinectSensor)
	{
		m_pKinectSensor->Close();
	}

	SafeRelease(m_pKinectSensor);

	uv_mutex_unlock(&m_bodyReaderMutex);
	uv_mutex_unlock(&m_depthReaderMutex);
	uv_mutex_unlock(&m_colorReaderMutex);
	uv_mutex_unlock(&m_infraredReaderMutex);
	uv_mutex_unlock(&m_longExposureInfraredReaderMutex);
	uv_mutex_unlock(&m_multiSourceReaderMutex);

	NanReturnValue(NanTrue());
}

NAN_METHOD(OpenBodyReaderFunction)
{
	NanScope();

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
		if (SUCCEEDED(hr))
		{
			//start async worker
			NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_BodyFrameArrived)->GetFunction());
			NanAsyncQueueWorker(new BodyFrameWorker(callback, &m_bodyReaderMutex, m_pCoordinateMapper, m_pBodyFrameReader));
		}
	}
	else
	{
		NanReturnValue(NanFalse());
	}

	SafeRelease(pBodyFrameSource);

	NanReturnValue(NanTrue());
}

NAN_METHOD(_BodyFrameArrived)
{
	NanScope();
	if(m_pBodyReaderCallback != NULL)
	{
		Local<Value> argv[] = {
			args[0].As<Array>()
		};
		m_pBodyReaderCallback->Call(1, argv);
	}
	if(m_pBodyFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_BodyFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new BodyFrameWorker(callback, &m_bodyReaderMutex, m_pCoordinateMapper, m_pBodyFrameReader));
	}
}

NAN_METHOD(CloseBodyReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_bodyReaderMutex);
	SafeRelease(m_pBodyFrameReader);
	uv_mutex_unlock(&m_bodyReaderMutex);

	NanReturnValue(NanTrue());
}

NAN_METHOD(OpenDepthReaderFunction)
{
	NanScope();

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
		if (SUCCEEDED(hr))
		{
			//start async worker
			NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_DepthFrameArrived)->GetFunction());
			NanAsyncQueueWorker(new DepthFrameWorker(callback, &m_depthReaderMutex, m_pDepthFrameReader, m_pDepthPixels, cDepthWidth, cDepthHeight));
		}
	}
	else
	{
		NanReturnValue(NanFalse());
	}

	SafeRelease(pDepthFrameSource);

	NanReturnValue(NanTrue());
}

NAN_METHOD(_DepthFrameArrived)
{
	NanScope();
	if(m_pDepthReaderCallback != NULL)
	{
		Local<Value> argv[] = {
			args[0].As<Object>()
		};
		m_pDepthReaderCallback->Call(1, argv);
	}
	if(m_pDepthFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_DepthFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new DepthFrameWorker(callback, &m_depthReaderMutex, m_pDepthFrameReader, m_pDepthPixels, cDepthWidth, cDepthHeight));
	}
}

NAN_METHOD(CloseDepthReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_depthReaderMutex);
	SafeRelease(m_pDepthFrameReader);
	uv_mutex_unlock(&m_depthReaderMutex);

	NanReturnValue(NanTrue());
}

NAN_METHOD(OpenColorReaderFunction)
{
	NanScope();

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
		if (SUCCEEDED(hr))
		{
			//start async worker
			NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_ColorFrameArrived)->GetFunction());
			NanAsyncQueueWorker(new ColorFrameWorker(callback, &m_colorReaderMutex, m_pColorFrameReader, m_pColorRGBX, cColorWidth, cColorHeight));
		}
	}
	else
	{
		NanReturnValue(NanFalse());
	}

	SafeRelease(pColorFrameSource);

	NanReturnValue(NanTrue());
}

NAN_METHOD(_ColorFrameArrived)
{
	NanScope();
	if(m_pColorReaderCallback != NULL)
	{
		Local<Value> argv[] = {
			args[0].As<Object>()
		};
		m_pColorReaderCallback->Call(1, argv);
	}
	if(m_pColorFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_ColorFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new ColorFrameWorker(callback, &m_colorReaderMutex, m_pColorFrameReader, m_pColorRGBX, cColorWidth, cColorHeight));
	}
}

NAN_METHOD(CloseColorReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_colorReaderMutex);
	SafeRelease(m_pColorFrameReader);
	uv_mutex_unlock(&m_colorReaderMutex);

	NanReturnValue(NanTrue());
}

NAN_METHOD(OpenInfraredReaderFunction)
{
	NanScope();

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
		if (SUCCEEDED(hr))
		{
			//start async worker
			NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_InfraredFrameArrived)->GetFunction());
			NanAsyncQueueWorker(new InfraredFrameWorker(callback, &m_infraredReaderMutex, m_pInfraredFrameReader, m_pInfraredPixels, cInfraredWidth, cInfraredHeight));
		}
	}
	else
	{
		NanReturnValue(NanFalse());
	}

	SafeRelease(pInfraredFrameSource);

	NanReturnValue(NanTrue());
}

NAN_METHOD(_InfraredFrameArrived)
{
	NanScope();
	if(m_pInfraredReaderCallback != NULL)
	{
		Local<Value> argv[] = {
			args[0].As<Object>()
		};
		m_pInfraredReaderCallback->Call(1, argv);
	}
	if(m_pInfraredFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_InfraredFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new InfraredFrameWorker(callback, &m_infraredReaderMutex, m_pInfraredFrameReader, m_pInfraredPixels, cInfraredWidth, cInfraredHeight));
	}
}

NAN_METHOD(CloseInfraredReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_infraredReaderMutex);
	SafeRelease(m_pInfraredFrameReader);
	uv_mutex_unlock(&m_infraredReaderMutex);

	NanReturnValue(NanTrue());
}

NAN_METHOD(OpenLongExposureInfraredReaderFunction)
{
	NanScope();

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
		if (SUCCEEDED(hr))
		{
			//start async worker
			NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_LongExposureInfraredFrameArrived)->GetFunction());
			NanAsyncQueueWorker(new LongExposureInfraredFrameWorker(callback, &m_longExposureInfraredReaderMutex, m_pLongExposureInfraredFrameReader, m_pLongExposureInfraredPixels, cLongExposureInfraredWidth, cLongExposureInfraredHeight));
		}
	}
	else
	{
		NanReturnValue(NanFalse());
	}

	SafeRelease(pLongExposureInfraredFrameSource);

	NanReturnValue(NanTrue());
}

NAN_METHOD(_LongExposureInfraredFrameArrived)
{
	NanScope();
	if(m_pLongExposureInfraredReaderCallback != NULL)
	{
		Local<Value> argv[] = {
			args[0].As<Object>()
		};
		m_pLongExposureInfraredReaderCallback->Call(1, argv);
	}
	if(m_pLongExposureInfraredFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_LongExposureInfraredFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new LongExposureInfraredFrameWorker(callback, &m_longExposureInfraredReaderMutex, m_pLongExposureInfraredFrameReader, m_pLongExposureInfraredPixels, cLongExposureInfraredWidth, cLongExposureInfraredHeight));
	}
}

NAN_METHOD(CloseLongExposureInfraredReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_longExposureInfraredReaderMutex);
	SafeRelease(m_pLongExposureInfraredFrameReader);
	uv_mutex_unlock(&m_longExposureInfraredReaderMutex);

	NanReturnValue(NanTrue());
}

NAN_METHOD(OpenMultiSourceReaderFunction)
{
	NanScope();

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

	HRESULT hr;

	hr = m_pKinectSensor->OpenMultiSourceFrameReader(
					m_enabledFrameSourceTypes,
					&m_pMultiSourceFrameReader);

	if (SUCCEEDED(hr))
	{
		//start async worker
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_MultiSourceFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new MultiSourceFrameWorker(
			callback,
			&m_multiSourceReaderMutex,
			m_pCoordinateMapper,
			m_pMultiSourceFrameReader,
			m_enabledFrameSourceTypes,
			m_enabledFrameTypes,
			m_pDepthCoordinatesForColor,
			m_pColorRGBX, cColorWidth, cColorHeight,
			m_pDepthPixels, cDepthWidth, cDepthHeight,
			m_pBodyIndexColorPixels
		));
	}
	else
	{
		std::cout << "reader init failed" << std::endl;
		std::cout << m_enabledFrameSourceTypes << std::endl;
		NanReturnValue(NanFalse());
	}

	NanReturnValue(NanTrue());
}

NAN_METHOD(_MultiSourceFrameArrived)
{
	NanScope();
	if(m_pMultiSourceReaderCallback != NULL)
	{
		Local<Value> argv[] = {
			args[0].As<Object>(),
			args[1].As<Object>()
		};
		m_pMultiSourceReaderCallback->Call(2, argv);
	}
	if(m_pMultiSourceFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_MultiSourceFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new MultiSourceFrameWorker(
			callback,
			&m_multiSourceReaderMutex,
			m_pCoordinateMapper,
			m_pMultiSourceFrameReader,
			m_enabledFrameSourceTypes,
			m_enabledFrameTypes,
			m_pDepthCoordinatesForColor,
			m_pColorRGBX, cColorWidth, cColorHeight,
			m_pDepthPixels, cDepthWidth, cDepthHeight,
			m_pBodyIndexColorPixels
		));
	}
}

NAN_METHOD(CloseMultiSourceReaderFunction)
{
	NanScope();

	uv_mutex_lock(&m_multiSourceReaderMutex);
	SafeRelease(m_pMultiSourceFrameReader);
	uv_mutex_unlock(&m_multiSourceReaderMutex);

	NanReturnValue(NanTrue());
}

void Init(Handle<Object> exports)
{
	uv_mutex_init(&m_bodyReaderMutex);
	uv_mutex_init(&m_depthReaderMutex);
	uv_mutex_init(&m_colorReaderMutex);
	uv_mutex_init(&m_infraredReaderMutex);
	uv_mutex_init(&m_longExposureInfraredReaderMutex);
	uv_mutex_init(&m_multiSourceReaderMutex);

	exports->Set(NanNew<String>("open"),
		NanNew<FunctionTemplate>(OpenFunction)->GetFunction());
	exports->Set(NanNew<String>("close"),
		NanNew<FunctionTemplate>(CloseFunction)->GetFunction());
	exports->Set(NanNew<String>("openBodyReader"),
		NanNew<FunctionTemplate>(OpenBodyReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("closeBodyReader"),
		NanNew<FunctionTemplate>(CloseBodyReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("openDepthReader"),
		NanNew<FunctionTemplate>(OpenDepthReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("closeDepthReader"),
		NanNew<FunctionTemplate>(CloseDepthReaderFunction)->GetFunction());
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
	exports->Set(NanNew<String>("openMultiSourceReader"),
		NanNew<FunctionTemplate>(OpenMultiSourceReaderFunction)->GetFunction());
	exports->Set(NanNew<String>("closeMultiSourceReader"),
		NanNew<FunctionTemplate>(CloseInfraredReaderFunction)->GetFunction());
}

NODE_MODULE(kinect2, Init)
