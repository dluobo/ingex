// Recorder.h

#ifndef Recorder_h
#define Recorder_h

class RecordingDevice;

/**
A recorder, such as a VTR or file-based recorder, with which you could
record a Source.  The result of making such a recording is a Clip.
*/
class Recorder
{
public:
	enum RecorderType { TAPE, FILE };

	void Type(RecorderType type);
	RecorderType Type();

	void Identifier(const char * identifier) { mIdentifier = identifier; }
	const char * Identifier() { return mIdentifier.c_str(); }

	RecordingDevice * Device(); // don't necessarily have one, e.g. for tape
private:
	RecorderType mType;
	std::string mIdentifier;
};

#endif //#ifndef Recorder_h