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

using namespace v8;

IKinectSensor*											m_pKinectSensor;
ICoordinateMapper*									m_pCoordinateMapper;
IBodyFrameReader*										m_pBodyFrameReader;
IDepthFrameReader*									m_pDepthFrameReader;
IColorFrameReader*  								m_pColorFrameReader;
IInfraredFrameReader* 							m_pInfraredFrameReader;
ILongExposureInfraredFrameReader* 	m_pLongExposureInfraredFrameReader;

bool										m_bBodyReaderOpen = false;

const int 							cDepthWidth  = 512;
const int 							cDepthHeight = 424;
char*										m_pDepthPixels = new char[cDepthWidth * cDepthHeight];
bool										m_bDepthReaderOpen = false;

const int 							cColorWidth  = 1920;
const int 							cColorHeight = 1080;
RGBQUAD*								m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];
bool										m_bColorReaderOpen = false;

const int 							cInfraredWidth  = 512;
const int 							cInfraredHeight = 424;
char*										m_pInfraredPixels = new char[cInfraredWidth * cInfraredHeight];
bool										m_bInfraredReaderOpen = false;

const int 							cLongExposureInfraredWidth  = 512;
const int 							cLongExposureInfraredHeight = 424;
char*										m_pLongExposureInfraredPixels = new char[cLongExposureInfraredWidth * cLongExposureInfraredHeight];
bool										m_bLongExposureInfraredReaderOpen = false;

NanCallback*						m_pBodyReaderCallback;
NanCallback*						m_pDepthReaderCallback;
NanCallback*						m_pColorReaderCallback;
NanCallback*						m_pInfraredReaderCallback;
NanCallback*						m_pLongExposureInfraredReaderCallback;

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

	m_bBodyReaderOpen = false;
	m_bDepthReaderOpen = false;
	m_bColorReaderOpen = false;
	m_bInfraredReaderOpen = false;
	m_bLongExposureInfraredReaderOpen = false;

	// done with coordinate mapper
	SafeRelease(m_pCoordinateMapper);

	// close the Kinect Sensor
	if (m_pKinectSensor)
	{
		m_pKinectSensor->Close();
	}

	SafeRelease(m_pKinectSensor);

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
			m_bBodyReaderOpen = true;
			//start async worker
			NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_BodyFrameArrived)->GetFunction());
			NanAsyncQueueWorker(new BodyFrameWorker(callback, m_pCoordinateMapper, m_pBodyFrameReader));
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
	//std::cout << "BodyFrameArrived" << std::endl;
	if(m_pBodyReaderCallback != NULL)
	{
		Local<Value> argv[] = {
			args[0].As<Array>()
		};
		m_pBodyReaderCallback->Call(1, argv);
	}
	if(!m_bBodyReaderOpen)
	{
		SafeRelease(m_pBodyFrameReader);
	}
	if(m_pBodyFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_BodyFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new BodyFrameWorker(callback, m_pCoordinateMapper, m_pBodyFrameReader));
	}
}

NAN_METHOD(CloseBodyReaderFunction)
{
	NanScope();

	//toggle boolean, we will stop after last frame has arrived
	m_bBodyReaderOpen = false;

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
			m_bDepthReaderOpen = true;
			//start async worker
			NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_DepthFrameArrived)->GetFunction());
			NanAsyncQueueWorker(new DepthFrameWorker(callback, m_pDepthFrameReader, m_pDepthPixels, cDepthWidth, cDepthHeight));
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
	if(!m_bDepthReaderOpen)
	{
		SafeRelease(m_pDepthFrameReader);
	}
	if(m_pDepthFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_DepthFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new DepthFrameWorker(callback, m_pDepthFrameReader, m_pDepthPixels, cDepthWidth, cDepthHeight));
	}
}

NAN_METHOD(CloseDepthReaderFunction)
{
	NanScope();

	//toggle boolean, we will stop after last frame has arrived
	m_bDepthReaderOpen = false;

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
			m_bColorReaderOpen = true;
			//start async worker
			NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_ColorFrameArrived)->GetFunction());
			NanAsyncQueueWorker(new ColorFrameWorker(callback, m_pColorFrameReader, m_pColorRGBX, cColorWidth, cColorHeight));
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
	if(!m_bColorReaderOpen)
	{
		SafeRelease(m_pColorFrameReader);
	}
	if(m_pColorFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_ColorFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new ColorFrameWorker(callback, m_pColorFrameReader, m_pColorRGBX, cColorWidth, cColorHeight));
	}
}

NAN_METHOD(CloseColorReaderFunction)
{
	NanScope();

	//toggle boolean, we will stop after last frame has arrived
	m_bColorReaderOpen = false;

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
			m_bInfraredReaderOpen = true;
			//start async worker
			NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_InfraredFrameArrived)->GetFunction());
			NanAsyncQueueWorker(new InfraredFrameWorker(callback, m_pInfraredFrameReader, m_pInfraredPixels, cInfraredWidth, cInfraredHeight));
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
	if(!m_bInfraredReaderOpen)
	{
		SafeRelease(m_pInfraredFrameReader);
	}
	if(m_pInfraredFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_InfraredFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new InfraredFrameWorker(callback, m_pInfraredFrameReader, m_pInfraredPixels, cInfraredWidth, cInfraredHeight));
	}
}

NAN_METHOD(CloseInfraredReaderFunction)
{
	NanScope();

	//toggle boolean, we will stop after last frame has arrived
	m_bInfraredReaderOpen = false;

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
			m_bLongExposureInfraredReaderOpen = true;
			//start async worker
			NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_LongExposureInfraredFrameArrived)->GetFunction());
			NanAsyncQueueWorker(new LongExposureInfraredFrameWorker(callback, m_pLongExposureInfraredFrameReader, m_pLongExposureInfraredPixels, cLongExposureInfraredWidth, cLongExposureInfraredHeight));
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
	if(!m_bLongExposureInfraredReaderOpen)
	{
		SafeRelease(m_pLongExposureInfraredFrameReader);
	}
	if(m_pLongExposureInfraredFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_LongExposureInfraredFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new LongExposureInfraredFrameWorker(callback, m_pLongExposureInfraredFrameReader, m_pLongExposureInfraredPixels, cLongExposureInfraredWidth, cLongExposureInfraredHeight));
	}
}

NAN_METHOD(CloseLongExposureInfraredReaderFunction)
{
	NanScope();

	//toggle boolean, we will stop after last frame has arrived
	m_bLongExposureInfraredReaderOpen = false;

	NanReturnValue(NanTrue());
}

void Init(Handle<Object> exports)
{
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
}

NODE_MODULE(kinect2, Init)
