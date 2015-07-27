#ifndef Kinect2_BodyFrameWorker_h
#define Kinect2_BodyFrameWorker_h

#include <nan.h>
#include "Kinect.h"
#include "Globals.h"
#include "Structs.h"

class BodyFrameWorker : public NanAsyncWorker
{
	public:
	uv_mutex_t* 					m_pMutex;
	ICoordinateMapper*		m_pCoordinateMapper;
	IBodyFrameReader*			m_pBodyFrameReader;
	JSBodyFrame*					m_pJSBodyFrame;
	int										numTrackedBodies;

	int 									m_cDepthWidth;
	int 									m_cDepthHeight;
	int 									m_cColorWidth;
	int 									m_cColorHeight;

	BodyFrameWorker(
		NanCallback *callback,
		uv_mutex_t* pMutex,
		ICoordinateMapper *pCoordinateMapper,
		IBodyFrameReader *pBodyFrameReader,
		JSBodyFrame *pJSBodyFrame,
		int cColorWidth, int cColorHeight,
		int cDepthWidth, int cDepthHeight
	)
		:
		NanAsyncWorker(callback),
		m_pMutex(pMutex),
		m_pCoordinateMapper(pCoordinateMapper),
		m_pBodyFrameReader(pBodyFrameReader),
		m_pJSBodyFrame(pJSBodyFrame),
		m_cColorWidth(cColorWidth), m_cColorHeight(cColorHeight),
		m_cDepthWidth(cDepthWidth), m_cDepthHeight(cDepthHeight)
	{
		numTrackedBodies = 0;
	}

	// Executed inside the worker-thread.
	// It is not safe to access V8, or V8 data structures
	// here, so everything we need for input and output
	// should go on `this`.
	void Execute ()
	{
		IBodyFrame* pBodyFrame = NULL;
		HRESULT hr;
		INT64 nTime = 0;
		IBody* ppBodies[BODY_COUNT] = {0};
		BOOLEAN frameReadSucceeded = false;
		uv_mutex_lock(m_pMutex);
		do
		{
			hr = m_pBodyFrameReader->AcquireLatestFrame(&pBodyFrame);
			if(SUCCEEDED(hr))
			{
				hr = pBodyFrame->get_RelativeTime(&nTime);
			}
			if(SUCCEEDED(hr))
			{
				hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);
			}
			if(SUCCEEDED(hr))
			{
				frameReadSucceeded = true;
				for (int i = 0; i < _countof(ppBodies); ++i)
				{
					IBody* pBody = ppBodies[i];
					if (pBody)
					{
						BOOLEAN bTracked = false;
						hr = pBody->get_IsTracked(&bTracked);
						m_pJSBodyFrame->bodies[i].tracked = bTracked;
						if(bTracked)
						{
							numTrackedBodies++;
							UINT64 iTrackingId = 0;
							hr = pBody->get_TrackingId(&iTrackingId);
							m_pJSBodyFrame->bodies[i].trackingId = iTrackingId;
							//go through the joints
							Joint joints[JointType_Count];
							hr = pBody->GetJoints(_countof(joints), joints);
							if (SUCCEEDED(hr))
							{
								for (int j = 0; j < _countof(joints); ++j)
								{
									DepthSpacePoint depthPoint = {0};
									m_pCoordinateMapper->MapCameraPointToDepthSpace(joints[j].Position, &depthPoint);

									ColorSpacePoint colorPoint = {0};
									m_pCoordinateMapper->MapCameraPointToColorSpace(joints[j].Position, &colorPoint);

									m_pJSBodyFrame->bodies[i].joints[j].depthX = depthPoint.X / m_cDepthWidth;
									m_pJSBodyFrame->bodies[i].joints[j].depthY = depthPoint.Y / m_cDepthHeight;

									m_pJSBodyFrame->bodies[i].joints[j].colorX = colorPoint.X / m_cColorWidth;
									m_pJSBodyFrame->bodies[i].joints[j].colorY = colorPoint.Y / m_cColorHeight;

									m_pJSBodyFrame->bodies[i].joints[j].cameraX = joints[j].Position.X;
									m_pJSBodyFrame->bodies[i].joints[j].cameraY = joints[j].Position.Y;
									m_pJSBodyFrame->bodies[i].joints[j].cameraZ = joints[j].Position.Z;

									m_pJSBodyFrame->bodies[i].joints[j].jointType = joints[j].JointType;
								}
							}
						}
						else
						{
							m_pJSBodyFrame->bodies[i].trackingId = 0;
						}
					}
				}
			}
			SafeRelease(pBodyFrame);
		}
		while(!frameReadSucceeded);

		for (int i = 0; i < _countof(ppBodies); ++i)
		{
			SafeRelease(ppBodies[i]);
		}

		uv_mutex_unlock(m_pMutex);
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
			if(m_pJSBodyFrame->bodies[i].tracked)
			{
				//create a body object
				v8::Local<v8::Object> v8body = NanNew<v8::Object>();
				v8body->Set(NanNew<v8::String>("trackingId"), NanNew<v8::Number>(static_cast<double>(m_pJSBodyFrame->bodies[i].trackingId)));

				v8::Local<v8::Object> v8joints = NanNew<v8::Object>();
				//joints
				for (int j = 0; j < _countof(m_pJSBodyFrame->bodies[i].joints); ++j)
				{
					v8::Local<v8::Object> v8joint = NanNew<v8::Object>();
					v8joint->Set(NanNew<v8::String>("depthX"), NanNew<v8::Number>(m_pJSBodyFrame->bodies[i].joints[j].depthX));
					v8joint->Set(NanNew<v8::String>("depthY"), NanNew<v8::Number>(m_pJSBodyFrame->bodies[i].joints[j].depthY));
					v8joint->Set(NanNew<v8::String>("colorX"), NanNew<v8::Number>(m_pJSBodyFrame->bodies[i].joints[j].colorX));
					v8joint->Set(NanNew<v8::String>("colorY"), NanNew<v8::Number>(m_pJSBodyFrame->bodies[i].joints[j].colorY));
					v8joint->Set(NanNew<v8::String>("cameraX"), NanNew<v8::Number>(m_pJSBodyFrame->bodies[i].joints[j].cameraX));
					v8joint->Set(NanNew<v8::String>("cameraY"), NanNew<v8::Number>(m_pJSBodyFrame->bodies[i].joints[j].cameraY));
					v8joint->Set(NanNew<v8::String>("cameraZ"), NanNew<v8::Number>(m_pJSBodyFrame->bodies[i].joints[j].cameraZ));
					v8joints->Set(NanNew<v8::Number>(m_pJSBodyFrame->bodies[i].joints[j].jointType), v8joint);
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
