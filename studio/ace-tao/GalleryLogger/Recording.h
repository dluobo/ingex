// Recording.h

#ifndef Recording_h
#define Recording_h

#include "Timecode.h"

/**
A recording which exists on a tape or in a file.
A clip is a section of a recording.
*/
class Recording
{
public:
	//enum EnumeratedType { TAPE, FILE };
	//static const char * TypeText(EnumeratedType type);

	enum EnumeratedFormat { TAPE, FILE, DV, SDI };
	static const char * FormatText(EnumeratedFormat fmt);

	//void Type(EnumeratedType type) { mType = type; }
	//EnumeratedType Type() const { return mType; };

	void Format(EnumeratedFormat fmt) { mFormat = fmt; }
	EnumeratedFormat Format() const { return mFormat; };

	void TapeId(const char * s) { mTapeId = s; }
	const char * TapeId() const { return mTapeId.c_str(); }

	void FileId(const char * s) { mFileId = s; }
	const char * FileId() const { return mFileId.c_str(); }

	void FileStartTimecode(const Timecode & tc) { mFileStartTimecode = tc; }
	const Timecode & FileStartTimecode() const { return mFileStartTimecode; }

	void FileEndTimecode(const Timecode & tc) { mFileEndTimecode = tc; }
	const Timecode & FileEndTimecode() const { return mFileEndTimecode; }

	//void FileDuration(const Duration & tc) { mFileDuration = tc; }
	//const Duration & FileDuration() const { return mFileDuration; }

private:
	//EnumeratedType mType;
	EnumeratedFormat mFormat;

	Timecode mIn;
	Timecode mOut;
	std::string mTapeId; ///< e.g. the tape number
	std::string mFileId; ///< e.g. a URL for the file
	Timecode mFileStartTimecode; ///< system timecode of initial frame
	Timecode mFileEndTimecode; ///< system timecode of frame after final frame
	//Duration mFileDuration;
};



// Classes below will be replaced by single Recording class above
#if 0
class TapeRecording
{
public:
	void TapeNumber(const char * s) { mTapeNumber = s; }
	const char * TapeNumber() const { return mTapeNumber.c_str(); }
	bool HasTapeNumber() const { return !mTapeNumber.empty(); }

private:
	std::string mTapeNumber;
};

class FileRecording
{
public:
	void Filename(const char * s) { mFilename = s; }
	const char * Filename() const { return mFilename.c_str(); }
	bool HasFilename() const { return !mFilename.empty(); }

	void Thumbnail(const char * s) { mThumbnail = s; }
	const char * Thumbnail() const { return mThumbnail.c_str(); }
	bool HasThumbnail() const { return !mThumbnail.empty(); }

	void StartTimecode(const Timecode & tc) { mStartTimecode = tc; }
	//const Timecode & StartTimecode() const { return mStartTimecode; }
	Timecode & StartTimecode() { return mStartTimecode; }

private:
	std::string mFilename;
	std::string mThumbnail;
	Timecode mStartTimecode;
};
#endif

#endif //#ifndef Recording_h