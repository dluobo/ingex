// RecordedSource.h

#ifndef RecordedSource_h
#define RecordedSource_h

#include <vector>

#include "Source.h"
#include "Clip.h"

/**
For each source that you recorded during a take, there will
be a number of clips.
*/
class RecordedSource
{
public:
	::Source & Source() { return mSource; }
	const ::Source & Source() const { return mSource; }

	int ClipCount() const { return mClips.size(); }
	::Clip & Clip(int i) { return mClips[i]; }
	const ::Clip & Clip(int i) const { return mClips[i]; }
	void AddClip(const ::Clip & clip) { mClips.push_back(clip); }

private:
	::Source mSource;
	std::vector<::Clip> mClips;
};

#endif #ifndef RecordedSource_h