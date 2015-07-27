#ifndef Kinect2_MultiSourceFrameWorker_h
#define Kinect2_MultiSourceFrameWorker_h

#include <nan.h>
#include "Kinect.h"
#include "Globals.h"
#include "Structs.h"

class MultiSourceFrameWorker : public NanAsyncWorker
{
	public:
	uv_mutex_t* 								m_pMutex;

	ICoordinateMapper*					m_pCoordinateMapper;
	IMultiSourceFrameReader* 		m_pMultiSourceFrameReader;
	DWORD												m_enabledFrameSourceTypes;
	unsigned long								m_enabledFrameTypes;

	RGBQUAD*										m_pColorPixels;
	int 												m_cColorWidth;
	int 												m_cColorHeight;

	char*												m_pDepthPixels;
	int 												m_cDepthWidth;
	int 												m_cDepthHeight;

	JSBodyFrame*								m_pJSBodyFrame;
	int													m_iNumTrackedBodies;

	DepthSpacePoint*						m_pDepthCoordinatesForColor;

	bool*												m_pHasBodyIndices;

	MultiSourceFrameWorker(
			NanCallback *callback,
			uv_mutex_t* pMutex,
			ICoordinateMapper *pCoordinateMapper,
			IMultiSourceFrameReader *pMultiSourceFrameReader,
			DWORD enabledFrameSourceTypes,
			unsigned long enabledFrameTypes,
			DepthSpacePoint* pDepthCoordinatesForColor,
			RGBQUAD* pColorPixels, int cColorWidth, int cColorHeight,
			char* pDepthPixels, int cDepthWidth, int cDepthHeight,
			JSBodyFrame* pJSBodyFrame,
			bool* pHasBodyIndices
		)
		:
			NanAsyncWorker(callback),
			m_pMutex(pMutex),
			m_pCoordinateMapper(pCoordinateMapper),
			m_pMultiSourceFrameReader(pMultiSourceFrameReader),
			m_enabledFrameSourceTypes(enabledFrameSourceTypes),
			m_enabledFrameTypes(enabledFrameTypes),
			m_pDepthCoordinatesForColor(pDepthCoordinatesForColor),
			m_pColorPixels(pColorPixels), m_cColorWidth(cColorWidth), m_cColorHeight(cColorHeight),
			m_pDepthPixels(pDepthPixels), m_cDepthWidth(cDepthWidth), m_cDepthHeight(cDepthHeight),
			m_pJSBodyFrame(pJSBodyFrame),
			m_pHasBodyIndices(pHasBodyIndices)
	{
		m_iNumTrackedBodies = 0;
		for(int i = 0; i < BODY_COUNT; i++)
		{
			pHasBodyIndices[i] = false;
		}
	}
	~MultiSourceFrameWorker()
	{
	}

	// Executed inside the worker-thread.
	// It is not safe to access V8, or V8 data structures
	// here, so everything we need for input and output
	// should go on `this`.
	void Execute ()
	{
		HRESULT hr;
		IMultiSourceFrame* pMultiSourceFrame = NULL;

		bool frameReadSucceeded = false;
		uv_mutex_lock(m_pMutex);
		do
		{
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

				IBodyFrameReference* pBodyFrameReference = NULL;
				IBodyFrame* pBodyFrame = NULL;
				IBody* ppBodies[BODY_COUNT] = {0};

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
							nColorBufferSize = m_cColorWidth * m_cColorHeight * sizeof(RGBQUAD);
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
								memset(m_pJSBodyFrame->bodies[i].colorPixels, 0, cColorWidth * cColorHeight * sizeof(RGBQUAD));
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
											m_pHasBodyIndices[player] = true;
											// set source for copy to the color pixel
											m_pJSBodyFrame->bodies[player].colorPixels[colorIndex] = pColorBuffer[colorIndex];
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
							memcpy(m_pColorPixels, pColorBuffer, m_cColorWidth * m_cColorHeight * sizeof(RGBQUAD));
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
							if (m_pDepthPixels && pDepthBuffer && (nDepthWidth == m_cDepthWidth) && (nDepthHeight == m_cDepthHeight))
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
								m_pJSBodyFrame->bodies[i].tracked = bTracked;
								if(bTracked)
								{
									m_iNumTrackedBodies++;
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
											m_pCoordinateMapper->MapCameraPointToDepthSpace(joints[j].Position, &depthPoint);
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

			if(SUCCEEDED(hr))
			{
				frameReadSucceeded = true;
			}

			SafeRelease(pMultiSourceFrame);
		}
		while(!frameReadSucceeded);

		uv_mutex_unlock(m_pMutex);
	}

	// Executed when the async work is complete
	// this function will be run inside the main event loop
	// so it is safe to use V8 again
	void HandleOKCallback ()
	{
		NanScope();

		//build the result object
		v8::Local<v8::Object> v8Result = NanNew<v8::Object>();

		if(NodeKinect2FrameTypes::FrameTypes_Color & m_enabledFrameTypes)
		{
			v8::Local<v8::Object> v8ColorResult = NanNew<v8::Object>();
			v8ColorResult->Set(NanNew<v8::String>("buffer"), NanNewBufferHandle((char *)m_pColorPixels, m_cColorWidth * m_cColorHeight * sizeof(RGBQUAD)));
			v8Result->Set(NanNew<v8::String>("color"), v8ColorResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_Depth & m_enabledFrameTypes)
		{
			v8::Local<v8::Object> v8DepthResult = NanNew<v8::Object>();
			v8DepthResult->Set(NanNew<v8::String>("buffer"), NanNewBufferHandle((char *)m_pDepthPixels, m_cDepthWidth * m_cDepthHeight * sizeof(char)));
			v8Result->Set(NanNew<v8::String>("depth"), v8DepthResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_Body & m_enabledFrameTypes)
		{
			v8::Local<v8::Object> v8BodyResult = NanNew<v8::Object>();

			v8::Local<v8::Array> v8bodies = NanNew<v8::Array>(m_iNumTrackedBodies);
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
			v8BodyResult->Set(NanNew<v8::String>("bodies"), v8bodies);

			v8Result->Set(NanNew<v8::String>("body"), v8BodyResult);
		}

		if(NodeKinect2FrameTypes::FrameTypes_BodyIndexColor & m_enabledFrameTypes)
		{
			v8::Local<v8::Object> v8BodyIndexColorResult = NanNew<v8::Object>();

			v8::Local<v8::Array> v8bodies = NanNew<v8::Array>(BODY_COUNT);
			for(int i = 0; i < BODY_COUNT; i++)
			{
				v8::Local<v8::Object> v8body = NanNew<v8::Object>();
				if(m_pHasBodyIndices[i]) {
					v8body->Set(NanNew<v8::String>("buffer"), NanNewBufferHandle((char *)m_pJSBodyFrame->bodies[i].colorPixels, m_cColorWidth * m_cColorHeight * sizeof(RGBQUAD)));
				}
				v8bodies->Set(i, v8body);
			}
			v8BodyIndexColorResult->Set(NanNew<v8::String>("bodies"), v8bodies);

			v8Result->Set(NanNew<v8::String>("bodyIndexColor"), v8BodyIndexColorResult);
		}

		v8::Local<v8::Value> argv[] = {
			NanNull(),
			v8Result
		};

		callback->Call(2, argv);
	};

	private:
};

#endif
