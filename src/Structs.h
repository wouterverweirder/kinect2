#ifndef Kinect2_Structs_h
#define Kinect2_Structs_h

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
	UINT64 trackingId;
	JSJoint joints[JointType_Count];
} JSBody;

typedef struct _JSBodyFrame
{
	INT64 timestamp;
	JSBody bodies[BODY_COUNT];
} JSBodyFrame;

#endif
