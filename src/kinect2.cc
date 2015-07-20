#include <nan.h>
#include <iostream>
#include "uv.h"
#include "Kinect.h"

using namespace v8;

IKinectSensor*		m_pKinectSensor;
ICoordinateMapper*	m_pCoordinateMapper;
IBodyFrameReader*	m_pBodyFrameReader;

NanCallback*		m_pBodyReaderCallback;

typedef struct _JSJoint
{
	float x;
	float y;
	int jointType;
} JSJoint;

typedef struct _JSBody
{
	bool tracked;
	UINT64 trackingId;
	JSJoint joints[JointType_Count];
} JSBody;

typedef struct _JSBodyFrame
{
	INT64 timestamp;
	JSBody bodies[BODY_COUNT];
} JSBodyFrame;

// Safe release for interfaces
template<class Interface>
inline void SafeRelease(Interface *& pInterfaceToRelease)
{
	if (pInterfaceToRelease != NULL)
	{
		pInterfaceToRelease->Release();
		pInterfaceToRelease = NULL;
	}
}

class BodyFrameWorker : public NanAsyncWorker
{
	public:
	JSBodyFrame* bodyFrame;
	int numTrackedBodies;
	BodyFrameWorker(NanCallback *callback) : NanAsyncWorker(callback)
	{
		bodyFrame = new JSBodyFrame();
		numTrackedBodies = 0;
	}
	~BodyFrameWorker()
	{
		delete bodyFrame;
	}

	// Executed inside the worker-thread.
	// It is not safe to access V8, or V8 data structures
	// here, so everything we need for input and output
	// should go on `this`.
	void Execute ()
	{
		IBodyFrame* pBodyFrame = NULL;
		HRESULT hr;
		BOOLEAN frameReadSucceeded = false;
		do
		{
			hr = m_pBodyFrameReader->AcquireLatestFrame(&pBodyFrame);
			if(SUCCEEDED(hr))
			{
				frameReadSucceeded = true;
				INT64 nTime = 0;
				hr = pBodyFrame->get_RelativeTime(&nTime);

				IBody* ppBodies[BODY_COUNT] = {0};

				if (SUCCEEDED(hr))
				{
					hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);
				}
				for (int i = 0; i < _countof(ppBodies); ++i)
				{
					IBody* pBody = ppBodies[i];
					if (pBody)
					{
						BOOLEAN bTracked = false;
						hr = pBody->get_IsTracked(&bTracked);
						bodyFrame->bodies[i].tracked = bTracked;
						if(bTracked)
						{
							numTrackedBodies++;
							UINT64 iTrackingId = 0;
							hr = pBody->get_TrackingId(&iTrackingId);
							bodyFrame->bodies[i].trackingId = iTrackingId;
							//go through the joints
							Joint joints[JointType_Count];
							hr = pBody->GetJoints(_countof(joints), joints);
							if (SUCCEEDED(hr))
							{
								for (int j = 0; j < _countof(joints); ++j)
								{
									DepthSpacePoint depthPoint = {0};
    								m_pCoordinateMapper->MapCameraPointToDepthSpace(joints[j].Position, &depthPoint);
									
									bodyFrame->bodies[i].joints[j].x = depthPoint.X;
									bodyFrame->bodies[i].joints[j].y = depthPoint.Y;
									bodyFrame->bodies[i].joints[j].jointType = joints[j].JointType;
								}
							}
						}
						else
						{
							bodyFrame->bodies[i].trackingId = 0;
						}
					}
				}
				for (int i = 0; i < _countof(ppBodies); ++i)
				{
					SafeRelease(ppBodies[i]);
				}
			}
		}
		while(!frameReadSucceeded);

		SafeRelease(pBodyFrame);
	}

	// Executed when the async work is complete
	// this function will be run inside the main event loop
	// so it is safe to use V8 again
	void HandleOKCallback ()
	{
		NanScope();

		//save the bodies as a V8 object structure
		v8::Local<v8::Array> v8bodies = NanNew<v8::Array>(numTrackedBodies);
		int bodyIndex = 0;
		for(int i = 0; i < BODY_COUNT; i++)
		{
			if(bodyFrame->bodies[i].tracked)
			{
				//create a body object
				v8::Local<v8::Object> v8body = NanNew<v8::Object>();
				v8body->Set(NanNew<v8::String>("trackingId"), NanNew<v8::Number>(bodyFrame->bodies[i].trackingId));

				v8::Local<v8::Object> v8joints = NanNew<v8::Object>();
				//joints
				for (int j = 0; j < _countof(bodyFrame->bodies[i].joints); ++j)
				{
					v8::Local<v8::Object> v8joint = NanNew<v8::Object>();
					v8joint->Set(NanNew<v8::String>("x"), NanNew<v8::Number>(bodyFrame->bodies[i].joints[j].x));
					v8joint->Set(NanNew<v8::String>("y"), NanNew<v8::Number>(bodyFrame->bodies[i].joints[j].y));
					v8joints->Set(NanNew<v8::Number>(bodyFrame->bodies[i].joints[j].jointType), v8joint);
				}
				v8body->Set(NanNew<v8::String>("joints"), v8joints);

				v8bodies->Set(bodyIndex, v8body);
				bodyIndex++;
			}
		}

		Local<Value> argv[] = {
			v8bodies
		};

		callback->Call(1, argv);
	};

	private:
};

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

		hr = m_pKinectSensor->Open();
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

	// done with body frame reader
	SafeRelease(m_pBodyFrameReader);

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
	if(m_pBodyFrameReader != NULL)
	{
		NanCallback *callback = new NanCallback(NanNew<FunctionTemplate>(_BodyFrameArrived)->GetFunction());
		NanAsyncQueueWorker(new BodyFrameWorker(callback));
	}
}

NAN_METHOD(OpenBodyReaderFunction) 
{
	NanScope();

	if(m_pBodyReaderCallback)
	{
		m_pBodyReaderCallback = NULL;
	}

	//TODO: make API compatible - args should be one object
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
			NanAsyncQueueWorker(new BodyFrameWorker(callback));
		}
	}
	else
	{
		NanReturnValue(NanFalse());
	}

	SafeRelease(pBodyFrameSource);

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
}

NODE_MODULE(kinect2, Init)