#ifndef Kinect2_BodyFrameWorker_h
#define Kinect2_BodyFrameWorker_h

#include <nan.h>
#include "Kinect.h"
#include "Globals.h"
#include "Structs.h"

class BodyFrameWorker : public NanAsyncWorker
{
	public:
	ICoordinateMapper*	m_pCoordinateMapper;
	IBodyFrameReader*	m_pBodyFrameReader;
	JSBodyFrame*		bodyFrame;
	int					numTrackedBodies;
	BodyFrameWorker(NanCallback *callback, ICoordinateMapper *pCoordinateMapper, IBodyFrameReader *pBodyFrameReader)
		: NanAsyncWorker(callback), m_pCoordinateMapper(pCoordinateMapper), m_pBodyFrameReader(pBodyFrameReader), numTrackedBodies(0)
	{
		bodyFrame = new JSBodyFrame();
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

		v8::Local<v8::Value> argv[] = {
			v8bodies
		};

		callback->Call(1, argv);
	};

	private:
};

#endif