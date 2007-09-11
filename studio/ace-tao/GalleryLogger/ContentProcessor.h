// ContentProcessor.h

#ifndef ContentProcessor_h
#define ContentProcessor_h

#include <string>

#include "tao/ORB.h"
//#include "processorC.h"
#include "SubjectC.h"

/**
Interface to a remote content processor.
The underlying interface uses Corba.
*/
class ContentProcessor
{
public:
	enum ReturnCode { OK, NO_DEVICE, FAILED };

	void SetDeviceName(const char * name) {mName = name;}
	ReturnCode ResolveDevice();
	ReturnCode Process(const char * args);

private:
	std::string mName;
	//ProdAuto::Processor_var mRef;
	ProdAuto::Subject_var mRef;
};

#endif