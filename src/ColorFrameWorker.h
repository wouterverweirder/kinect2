#ifndef Kinect2_ColorFrameWorker_h
#define Kinect2_ColorFrameWorker_h

#include <nan.h>
#include "Kinect.h"
#include "Globals.h"
#include "Structs.h"

class ColorFrameWorker : public NanAsyncWorker
{
	public:
	IColorFrameReader*		m_pColorFrameReader;
	RGBQUAD*				m_pColorRGBX;
	int 					m_cColorWidth;
	int 					m_cColorHeight;
	ColorFrameWorker(NanCallback *callback, IColorFrameReader *pColorFrameReader, RGBQUAD *pColorRGBX, int cColorWidth, int cColorHeight)
		: NanAsyncWorker(callback), m_pColorFrameReader(pColorFrameReader), m_pColorRGBX(pColorRGBX), m_cColorWidth(cColorWidth), m_cColorHeight(cColorHeight)
	{
	}
	~ColorFrameWorker()
	{
	}

	// Executed inside the worker-thread.
	// It is not safe to access V8, or V8 data structures
	// here, so everything we need for input and output
	// should go on `this`.
	void Execute ()
	{
		IColorFrame* pColorFrame = NULL;
        IFrameDescription* pFrameDescription = NULL;
        int nWidth = 0;
        int nHeight = 0;
        ColorImageFormat imageFormat = ColorImageFormat_None;
        UINT nBufferSize = 0;
        RGBQUAD *pBuffer = NULL;

		HRESULT hr;
		bool frameReadSucceeded = false;
		do
		{
			hr = m_pColorFrameReader->AcquireLatestFrame(&pColorFrame);

			if (SUCCEEDED(hr))
			{
				hr = pColorFrame->get_FrameDescription(&pFrameDescription);
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
				hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
			}

			if (SUCCEEDED(hr))
			{
				if (imageFormat == ColorImageFormat_Rgba)
				{
					hr = pColorFrame->AccessRawUnderlyingBuffer(&nBufferSize, reinterpret_cast<BYTE**>(&pBuffer));
				}
				else if (m_pColorRGBX)
				{
					pBuffer = m_pColorRGBX;
					nBufferSize = m_cColorWidth * m_cColorHeight * sizeof(RGBQUAD);
					hr = pColorFrame->CopyConvertedFrameDataToArray(nBufferSize, reinterpret_cast<BYTE*>(pBuffer), ColorImageFormat_Rgba);            
				}
				else
				{
					hr = E_FAIL;
				}
			}

			if (SUCCEEDED(hr))
			{
				frameReadSucceeded = true;
				memcpy(m_pColorRGBX, pBuffer, m_cColorWidth * m_cColorHeight * sizeof(RGBQUAD));
			}

			SafeRelease(pFrameDescription);
		}
		while(!frameReadSucceeded);

		SafeRelease(pColorFrame);
	}

	// Executed when the async work is complete
	// this function will be run inside the main event loop
	// so it is safe to use V8 again
	void HandleOKCallback ()
	{
		NanScope();

		v8::Local<v8::Value> argv[] = {
			NanNewBufferHandle((char *)m_pColorRGBX, m_cColorWidth * m_cColorHeight * sizeof(RGBQUAD))
		};

		callback->Call(1, argv);
	};

	private:
};

#endif