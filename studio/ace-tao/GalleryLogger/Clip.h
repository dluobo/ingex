// Clip.h

#ifndef Clip_h
#define Clip_h

#include <string>
#include "Timecode.h"
#include "Recording.h"
#include "Source.h"

#if 1
// new version

//using Recording::RecordingType;

class Clip
{
public:
	void Recording(const ::Recording & r) { mRecording = r; }
	const ::Recording & Recording() const { return mRecording; }

	// offset into recording
	void Start(const Timecode & t) { mStart = t; }
	const Timecode & Start() const { return mStart; }

	// duration of relevant section of recording
	void Duration(const ::Duration & t) { mDuration = t; }
	const ::Duration & Duration() const { return mDuration; }

	/*
	void Type(RecordingType type) { mRecording.Type(type); }
	RecordingType Type() const { return mRecording.Type(); };

	void TapeId(const char * s) { mRecording.TapeId(s); }
	const char * TapeId() const { return mRecording.TapeId(); }

	void FileId(const char * s) { mRecording.FileId(s); }
	const char * FileId() const { return mRecording.FileId(); }

	void FileStartTimecode(const Timecode & tc) { mRecording.FileStartTimecode(tc); }
	const Timecode & FileStartTimecode() const { return mRecording.FileStartTimecode(); }
	*/

private:
	::Recording mRecording;
	Timecode mStart; ///< offset into file for type FILE, timecode on tape for type TAPE
	::Duration mDuration;
};
#else
// old version
class Clip
{
public:
	void SourceName(const char * s) { mSource.Name(s); }
	const char * SourceName() const { return mSource.Name(); }
	bool HasSourceName() const { return !mSource.NameIsEmpty(); }

	void SourceDescription(const char * s) { mSource.Description(s); }
	const char * SourceDescription() const { return mSource.Description(); }
	bool HasSourceDescription() const { return !mSource.DescriptionIsEmpty(); }

	void TapeNumber(const char * s) { mTape.TapeNumber(s); }
	const char * TapeNumber() const { return mTape.TapeNumber(); }
	bool HasTapeNumber() const { return mTape.HasTapeNumber(); }

	void Filename(const char * s) { mFile.Filename(s); }
	const char * Filename() { return mFile.Filename(); }
	bool HasFilename() const { return mFile.HasFilename(); }

	void Thumbnail(const char * s) { mFile.Thumbnail(s); }
	const char * Thumbnail() { return mFile.Thumbnail(); }
	bool HasThumbnail() const { return mFile.HasThumbnail(); }

private:
	Source mSource;
	std::string mSourceName; ///< e.g. Main, Iso1
	std::string mSourceDescription; ///< e.g. Mixer Out, Camera 2
	TapeRecording mTape;
	FileRecording mFile;
};
#endif // new/old version

#endif //#ifndef Clip_h
