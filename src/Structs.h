#ifndef Kinect2_Structs_h
#define Kinect2_Structs_h	

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

#endif