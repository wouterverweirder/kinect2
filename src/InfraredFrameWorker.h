#ifndef Kinect2_InfraredFrameWorker_h
#define Kinect2_InfraredFrameWorker_h

#include <nan.h>
#include "Kinect.h"
#include "Utils.h"
#include "Structs.h"

// InfraredSourceValueMaximum is the highest value that can be returned in the InfraredFrame.
// It is cast to a float for readability in the visualization code.
#define InfraredSourceValueMaximum static_cast<float>(USHRT_MAX)

// The InfraredOutputValueMinimum value is used to set the lower limit, post processing, of the
// infrared data that we will render.
// Increasing or decreasing this value sets a brightness "wall" either closer or further away.
#define InfraredOutputValueMinimum 0.01f 

// The InfraredOutputValueMaximum value is the upper limit, post processing, of the
// infrared data that we will render.
#define InfraredOutputValueMaximum 1.0f

// The InfraredSceneValueAverage value specifies the average infrared value of the scene.
// This value was selected by analyzing the average pixel intensity for a given scene.
// Depending on the visualization requirements for a given application, this value can be
// hard coded, as was done here, or calculated by averaging the intensity for each pixel prior
// to rendering.
#define InfraredSceneValueAverage 0.08f

/// The InfraredSceneStandardDeviations value specifies the number of standard deviations
/// to apply to InfraredSceneValueAverage. This value was selected by analyzing data
/// from a given scene.
/// Depending on the visualization requirements for a given application, this value can be
/// hard coded, as was done here, or calculated at runtime.
#define InfraredSceneStandardDeviations 3.0f

class InfraredFrameWorker : public NanAsyncWorker
{
	public:
	IInfraredFrameReader*	m_pInfraredFrameReader;
	char*					m_pInfraredPixels;
	int 					m_cInfraredWidth;
	int 					m_cInfraredHeight;
	InfraredFrameWorker(NanCallback *callback, IInfraredFrameReader *pInfraredFrameReader, char *pInfraredPixels, int cInfraredWidth, int cInfraredHeight)
		: NanAsyncWorker(callback), m_pInfraredFrameReader(pInfraredFrameReader), m_pInfraredPixels(pInfraredPixels), m_cInfraredWidth(cInfraredWidth), m_cInfraredHeight(cInfraredHeight)
	{
	}
	~InfraredFrameWorker()
	{
	}

	// Executed inside the worker-thread.
	// It is not safe to access V8, or V8 data structures
	// here, so everything we need for input and output
	// should go on `this`.
	void Execute ()
	{
		IInfraredFrame* pInfraredFrame = NULL;
        IFrameDescription* pFrameDescription = NULL;
        int nWidth = 0;
        int nHeight = 0;
        UINT nBufferSize = 0;
        UINT16 *pBuffer = NULL;

		HRESULT hr;
		bool frameReadSucceeded = false;
		do
		{
			hr = m_pInfraredFrameReader->AcquireLatestFrame(&pInfraredFrame);

			if (SUCCEEDED(hr))
			{
				hr = pInfraredFrame->get_FrameDescription(&pFrameDescription);
			}

			if (SUCCEEDED(hr))
			{
				hr = pFrameDescription->get_Width(&nWidth);
			}

			if (SUCCEEDED(hr))
			{
				hr = pFrameDescription->get_Height(&nHeight);
			}

			if (SUCCEEDED(hr))
			{
				hr = pInfraredFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);
			}

			if (SUCCEEDED(hr))
			{
				frameReadSucceeded = true;
				if (m_pInfraredPixels && pBuffer && (nWidth == m_cInfraredWidth) && (nHeight == m_cInfraredHeight))
				{
					char* pDest = m_pInfraredPixels;

					// end pixel is start + width*height - 1
					const UINT16* pBufferEnd = pBuffer + (nWidth * nHeight);

					while (pBuffer < pBufferEnd)
					{
						// normalize the incoming infrared data (ushort) to a float ranging from 
						// [InfraredOutputValueMinimum, InfraredOutputValueMaximum] by
						// 1. dividing the incoming value by the source maximum value
						float intensityRatio = static_cast<float>(*pBuffer) / InfraredSourceValueMaximum;

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
						++pBuffer;
					}
				}
			}

			SafeRelease(pFrameDescription);
		}
		while(!frameReadSucceeded);

		SafeRelease(pInfraredFrame);
	}

	// Executed when the async work is complete
	// this function will be run inside the main event loop
	// so it is safe to use V8 again
	void HandleOKCallback ()
	{
		NanScope();

		v8::Local<v8::Value> argv[] = {
			NanNewBufferHandle(m_pInfraredPixels, m_cInfraredWidth * m_cInfraredHeight)
		};

		callback->Call(1, argv);
	};

	private:
};

#endif