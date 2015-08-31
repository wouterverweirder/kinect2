#ifndef Kinect2_Globals_h
#define Kinect2_Globals_h

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

const int 							cDepthWidth  = 512;
const int 							cDepthHeight = 424;
const int 							cColorWidth  = 1920;
const int 							cColorHeight = 1080;
const int 							cInfraredWidth  = 512;
const int 							cInfraredHeight = 424;
const int 							cLongExposureInfraredWidth  = 512;
const int 							cLongExposureInfraredHeight = 424;

#ifndef _NodeKinect2FrameTypes_
#define _NodeKinect2FrameTypes_
typedef enum _NodeKinect2FrameTypes NodeKinect2FrameTypes;

enum _NodeKinect2FrameTypes
{
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
		FrameTypes_RawDepth	= 0x400,
		FrameTypes_DepthColor	= 0x800
} ;
#endif // _NodeKinect2FrameTypes_

#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__WINDOWS__) || defined(__TOS_WIN__)

#include <windows.h>

inline void delay( unsigned long ms )
{
Sleep( ms );
}

#else  /* presume POSIX */

#include <unistd.h>

inline void delay( unsigned long ms )
{
usleep( ms * 1000 );
}

#endif

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

#endif
