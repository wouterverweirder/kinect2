#ifndef Kinect2_Structs_h
#define Kinect2_Structs_h

#include "Globals.h"

typedef struct _JSJoint
{
	float depthX;
	float depthY;
	float colorX;
	float colorY;
	float cameraX;
	float cameraY;
	float cameraZ;
	int jointType;
} JSJoint;

typedef struct _JSBody
{
	bool tracked;
	bool hasPixels;
	bool trackPixels;
	UINT64 trackingId;
	char leftHandState;
	char rightHandState;
	JSJoint joints[JointType_Count];
	RGBQUAD colorPixels[cColorWidth * cColorHeight];
	char depthPixels[cDepthWidth * cDepthHeight];
	char infraredPixels[cInfraredWidth * cInfraredHeight];
	char longExposureInfraredPixels[cLongExposureInfraredWidth * cLongExposureInfraredHeight];
} JSBody;

typedef struct _JSBodyFrame
{
	JSBody bodies[BODY_COUNT];
	bool hasFloorClipPlane;
	float floorClipPlaneX;
	float floorClipPlaneY;
	float floorClipPlaneZ;
	float floorClipPlaneW;
} JSBodyFrame;

#endif
