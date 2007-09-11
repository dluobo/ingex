// RecordingDevice.h

#ifndef RecordingDevice_h
#define RecordingDevice_h

#include <string>
#include <Timecode.h>

//#include "tao/ORB.h"
#include "RecorderC.h"


//const int N_TRACKS = 15; // Temporary bodge
const int MAX_VIDEO_TRACKS = 4;

/**
Interface to a remote recording device.
The underlying interface uses Corba.
*/
class RecordingDevice
{
public:
	enum ReturnCode { OK, NO_DEVICE, FAILED };

	RecordingDevice() : mIsRecording(false), mHasRecorded(false) {} // constructor

	void SetDeviceName(const char * name) {mName = name;}
	ReturnCode ResolveDevice();
	//ReturnCode SetDevice(const char * name);
	//ReturnCode SetDirectory(const char * subdir);
	ReturnCode StartRecording(const Timecode & tc,
							  const std::string & description);
	ReturnCode StopRecording(const Timecode & tc);

	bool IsRecording() { return mIsRecording; }
	bool HasRecorded() { return mHasRecorded; }
	//const char * Filename(int i) { return mFilename[i].c_str(); }
	const char * InTime() { return mInTime.c_str(); }
	const char * OutTime() { return mOutTime.c_str(); }
	const char * Duration() { return mDuration.c_str(); }
	const char * Filename(int i) { return mFilename[i].c_str(); }

	void GetVideoTracks(std::vector<std::string> & tracks);

	int TrackCount() { return mTrackCount; }

private:
	std::string mName;
	ProdAuto::Recorder_var mRecorder;

	bool mIsRecording;
	bool mHasRecorded;
	std::string mFilename[MAX_VIDEO_TRACKS];
	std::string mInTime;
	std::string mOutTime;
	std::string mDuration;
	//std::string mSubDir;
	int mTrackCount;
};

#endif //#ifndef RecordingDevice_h
