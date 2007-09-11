// Sequence.cpp

#include "Sequence.h"

#include "ace/OS_NS_stdio.h"
#include "ace/OS_NS_string.h"

//Sequence::Sequence(int number, const char * name, const char * id)
//: mScriptStart(number), mScriptEnd(number), mName(name), mUniqueId(id), mProgrammeIndex(0)
//{
//}

Sequence::Sequence(const char * name,
		const ScriptRef & start,
		//const ScriptRef & end,
		const char * id)
: mScriptStart(start), /*mScriptEnd(end),*/ mName(name), mUniqueId(id)
{
}

//const char * Sequence::ProgrammeIndexT()
//{
//	ACE_OS::sprintf(mProgrammeIndexText, "%03d", mProgrammeIndex);
//	return mProgrammeIndexText;
//}

const char * Sequence::Name(int max)
{
	static std::string tmp;
	if(max < 0)
	{
		max = -max;
	}
	if(mName.size() <= max)
	{
		return mName.c_str();
	}
	else
	{
		tmp = mName.substr(0, max);
		return tmp.c_str();
	}
}

void Sequence::AddTake(const ::Take & take)
{
	mTakes.push_back(take);
	// We store a number in the take even though it is
	// implied by its position in the list.
	mTakes.back().Number(mTakes.size());
}

void Sequence::ModifyTake(int index, const ::Take & take)
{
	if(index < mTakes.size())
	{
		mTakes[index] = take;
		// We store a number in the take even though it is
		// implied by its position in the list.
		mTakes[index].Number(index + 1);
	}
}

bool Sequence::HasGoodTake()
{
	bool result = false;
	for(int i = 0; i < mTakes.size() && !result; ++i)
	{
		if(mTakes[i].IsGood())
		{
			result = true;
		}
	}
	return result;
}

std::string Sequence::ScriptRefRange()
{
	const char * start = mScriptStart.Value();
	//const char * end = mScriptEnd.Value();
	std::string range = start;
	//if(0 != ACE_OS::strcmp(start, end))
	//{
	//	range += "-";
	//	range += end;
	//}
	return range;
}
