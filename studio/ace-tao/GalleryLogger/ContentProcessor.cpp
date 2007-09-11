// ContentProcessor.cpp

#include <CorbaUtil.h>
#include "ContentProcessor.h"

ContentProcessor::ReturnCode ContentProcessor::ResolveDevice()
{
	ReturnCode result = OK;
	if(mName.empty())
	{
		result = NO_DEVICE;
	}

	CosNaming::Name name;
	name.length(3);	
	name[0].id = CORBA::string_dup("ProductionAutomation");
	name[1].id = CORBA::string_dup("Processors");
	name[2].id = CORBA::string_dup(mName.c_str());

	CORBA::Object_var obj;
	if(OK == result)
	{
		obj = CorbaUtil::Instance()->ResolveObject(name);
	}

	if(CORBA::is_nil(obj))
	{
		result = NO_DEVICE;
	}

	if(OK == result)
	{
		try
		{
			//mRef = ProdAuto::Processor::_narrow(obj);
			mRef = ProdAuto::Subject::_narrow(obj);
		}
		catch(const CORBA::Exception &)
		{
			result = NO_DEVICE;
		}
	}

	return result;
}


ContentProcessor::ReturnCode ContentProcessor::Process(const char * args)
{
	bool ok_so_far = true;
	ReturnCode return_code = OK;

	if(CORBA::is_nil(mRef))
	{
		ok_so_far = false;
		return_code = NO_DEVICE;
	}

	if(ok_so_far)
	{
		try
		{
			//mRef->Process(args);
			mRef->Notify(args);
		}
		catch (const CORBA::Exception &)
		{
			ok_so_far = false;
			return_code = FAILED;
		}
	}

	return return_code;
}
