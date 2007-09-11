// Take.cpp

#include <ace/OS_NS_stdlib.h> // for itoa

#include "Take.h"

Take::Take()
: mResult(Take::UNKNOWN)
{
}

void Take::Clear()
{
	mNumber = 0;
	mInTime = Timecode(0);
	mOutTime = Timecode(0);
	mDate = "";
	mComment = "";
    mResult = Take::UNKNOWN;
	//mRecordedSources.clear();
}

#if 0
const char * Take::Sources()
{
	mSources.erase();
	for(int i = 0; i < mRecordedSources.size(); ++i)
	{
		if(i > 0)
		{
			mSources += ", ";
		}
		mSources += mRecordedSources[i].Source().Name();
	}
	return mSources.c_str();
}
#endif

const Take & Take::operator=(const prodauto::Take * pt)
{
    mNumber = pt->number;
    mComment = pt->comment;
    switch (pt->result)
    {
    case GOOD_TAKE_RESULT:
        mResult = Take::GOOD;
        break;
    case NOT_GOOD_TAKE_RESULT:
        mResult = Take::NG;
        break;
    default:
        mResult = Take::UNKNOWN;
        break;
    }

    // Assuming 25 fps NDF
    mInTime = Timecode(pt->startPosition);
    mOutTime = Timecode(pt->startPosition + pt->length);

    mYear = pt->startDate.year;
    mMonth = pt->startDate.month;
    mDay = pt->startDate.day;

    mLocation = pt->recordingLocation;

    return *this;
}

Take::Take(const prodauto::Take * pt)
{
    mNumber = pt->number;
    mComment = pt->comment;
    switch (pt->result)
    {
    case GOOD_TAKE_RESULT:
        mResult = Take::GOOD;
        break;
    case NOT_GOOD_TAKE_RESULT:
        mResult = Take::NG;
        break;
    default:
        mResult = Take::UNKNOWN;
        break;
    }

    // Assuming 25 fps NDF
    mInTime = Timecode(pt->startPosition);
    mOutTime = Timecode(pt->startPosition + pt->length);

    mYear = pt->startDate.year;
    mMonth = pt->startDate.month;
    mDay = pt->startDate.day;

    mLocation = pt->recordingLocation;
}

void Take::CopyTo(prodauto::Take * pt) const
{
    pt->number = mNumber;
    pt->comment = mComment;
    switch (mResult)
    {
    case GOOD:
        pt->result = GOOD_TAKE_RESULT;
        break;
    case NG:
        pt->result = NOT_GOOD_TAKE_RESULT;
        break;
    default:
        pt->result = UNSPECIFIED_TAKE_RESULT;
        break;
    }
    pt->editRate.numerator = mInTime.EditRateNumerator();
    pt->editRate.denominator = mInTime.EditRateDenominator();
    pt->startPosition = mInTime.FramesSinceMidnight();
    pt->length = mOutTime.FramesSinceMidnight() - mInTime.FramesSinceMidnight();

    pt->startDate.year = mYear;
    pt->startDate.month = mMonth;
    pt->startDate.day = mDay;

    pt->recordingLocation = mLocation;
}

const char * Take::ResultText()
{
    const char * text;
    switch (mResult)
    {
    case Take::GOOD:
        text = "Good";
        break;
    case Take::NG:
        text = "NG";
        break;
    default:
        text = "?";
    }
    return text;
}

std::string Take::DateText() const
{
    char tmp[32];
    sprintf(tmp, "%04d-%02d-%02d", mYear, mMonth, mDay);

    return std::string(tmp);
}


