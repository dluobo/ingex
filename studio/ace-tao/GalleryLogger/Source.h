// Source.h

#ifndef Source_h
#define Source_h

#include "Recorder.h"

/**
Describes a "source" which might be recorded during TV production, for example
"Mixer out", "Camera 2".
I have also included a couple of Recorders connected to the source.  There may
be a better way of describing that relationship.
*/
class Source
{
public:
	Source() : mEnabled(false) {}

	const char * Name() const { return mName.c_str(); }
	void Name(const char * s) { mName = s; }
	bool NameIsEmpty() const { return mName.empty(); }

	const char * Description() const { return mDescription.c_str(); }
	void Description(const char * s) { mDescription = s; }
	bool DescriptionIsEmpty() const { return mDescription.empty(); }

	/*
	const char * Tape() const { return mTape.c_str(); }
	void Tape(const char * s) { mTape = s; }
	bool TapeIsEmpty() const { return (0 == mTape.size()); }

	const char * Device() const { return mDevice.c_str(); }
	void Device(const char * s) { mDevice = s; }
	bool DeviceIsEmpty() const { return (0 == mDevice.size()); }
	*/

	Recorder & TapeRecorder() { return mTapeRecorder; }
	Recorder & FileRecorder() { return mFileRecorder; }

	bool Enabled() { return mEnabled; }
	void Enabled(bool b) { mEnabled = b; }

private:
	std::string mName;
	std::string mDescription;
	Recorder mTapeRecorder;  // Potentially there could be more recorders
	Recorder mFileRecorder;  // but we'll fix at one of each type for the moment.
	//std::string mTape;
	//std::string mDevice;
	bool mEnabled;
};

#endif //#ifndef Source_h