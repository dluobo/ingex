// CoordinatorApp.h

#ifndef CoordinatorApp_h
#define CoordinatorApp_h

#include <orbsvcs/CosNamingC.h>

#include "CoordinatorSubject.h"

class CoordinatorApp
{
public:
// singleton access
	static CoordinatorApp * Instance()
	{
		if(0 == mInstance)
		{
			mInstance = new CoordinatorApp();
		}
		return mInstance;
	}
// singleton destroy
	void Destroy() { mInstance = 0; delete this; }

// control methods
	bool Init(int argc, ACE_TCHAR * argv[]);
	void Run();
	void Stop();
	void Clean();

protected:
// constructor protected as this is a singleton
	CoordinatorApp() {};

private:
// singleton instance pointer
	static CoordinatorApp * mInstance;

// CORBA servant
	CoordinatorSubject * mpServant;
	ProdAuto::Subject_var mRef;
	CosNaming::Name mName;

// flag for terminating main loop
	bool mTerminated;
};


#endif //#ifndef CoordinatorApp_h
