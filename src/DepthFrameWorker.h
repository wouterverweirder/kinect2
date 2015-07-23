#ifndef Kinect2_DepthFrameWorker_h
#define Kinect2_DepthFrameWorker_h

#include <nan.h>
#include "Kinect.h"
#include "Utils.h"
#include "Structs.h"

class DepthFrameWorker : public NanAsyncWorker
{
	public:
	IDepthFrameReader*	m_pDepthFrameReader;
	char*				m_pDepthPixels;
	int 				m_cDepthWidth;
	int 				m_cDepthHeight;
	DepthFrameWorker(NanCallback *callback, IDepthFrameReader* pDepthFrameReader, char* pDepthPixels, int cDepthWidth, int cDepthHeight)
		 : NanAsyncWorker(callback), m_pDepthFrameReader(pDepthFrameReader), m_pDepthPixels(pDepthPixels), m_cDepthWidth(cDepthWidth), m_cDepthHeight(cDepthHeight)
	{
	}
	~DepthFrameWorker()
	{
	}

	// Executed inside the worker-thread.
	// It is not safe to access V8, or V8 data structures
	// here, so everything we need for input and output
	// should go on `this`.
	void Execute ()
	{
		IDepthFrame* pDepthFrame = NULL;
		IFrameDescription* pFrameDescription = NULL;
		int nWidth = 0;
		int nHeight = 0;
		USHORT nDepthMinReliableDistance = 0;
		USHORT nDepthMaxDistance = 0;
		UINT nBufferSize = 0;
		UINT16 *pBuffer = NULL;

		int mapDepthToByte = 8000 / 256;

		HRESULT hr;
		bool frameReadSucceeded = false;
		do
		{
			hr = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);

			if (SUCCEEDED(hr))
			{
				hr = pDepthFrame->get_FrameDescription(&pFrameDescription);
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
				hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
			}

			if (SUCCEEDED(hr))
			{
				hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
			}

			if (SUCCEEDED(hr))
			{
				hr = pDepthFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);            
			}

			if (SUCCEEDED(hr))
			{
				frameReadSucceeded = true;
				mapDepthToByte = nDepthMaxDistance / 256;
				if (m_pDepthPixels && pBuffer && (nWidth == m_cDepthWidth) && (nHeight == m_cDepthHeight))
				{
					char* pDepthPixel = m_pDepthPixels;

					// end pixel is start + width*height - 1
					const UINT16* pBufferEnd = pBuffer + (nWidth * nHeight);

					while (pBuffer < pBufferEnd)
					{
						USHORT depth = *pBuffer;

						BYTE intensity = static_cast<BYTE>(depth >= nDepthMinReliableDistance && depth <= nDepthMaxDistance ? (depth / mapDepthToByte) : 0);
						*pDepthPixel = intensity;

						++pDepthPixel;
						++pBuffer;
					}
				}
			}

			SafeRelease(pFrameDescription);
		}
		while(!frameReadSucceeded);

		SafeRelease(pDepthFrame);
	}

	// Executed when the async work is complete
	// this function will be run inside the main event loop
	// so it is safe to use V8 again
	void HandleOKCallback ()
	{
		NanScope();

		v8::Local<v8::Value> argv[] = {
			NanNewBufferHandle(m_pDepthPixels, m_cDepthWidth * m_cDepthHeight)
		};

		callback->Call(1, argv);
	};

	private:
};

#endif