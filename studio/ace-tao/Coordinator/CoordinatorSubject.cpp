// CoordinatorSubject.cpp

#include <iostream>
#include "CoordinatorSubject.h"

// Implementation skeleton constructor
CoordinatorSubject::CoordinatorSubject (void)
{
}

// Implementation skeleton destructor
CoordinatorSubject::~CoordinatorSubject (void)
{
}

::ProdAuto::Subject::ReturnCode CoordinatorSubject::Attach (
    ::ProdAuto::Observer_ptr obs
  )
  throw (
    CORBA::SystemException
  )
{
	mObservers.push_back(ProdAuto::Observer::_duplicate(obs));
	return ProdAuto::Subject::OK;
}

::ProdAuto::Subject::ReturnCode CoordinatorSubject::Detach (
    ::ProdAuto::Observer_ptr obs
  )
  throw (
    CORBA::SystemException
  )
{
	ObserverIterator it = mObservers.begin();
	while(it != mObservers.end())
	{
		if((*it)->_is_equivalent(obs))
		{
			it = mObservers.erase(it);
		}
		else
		{
			++it;
		}
	}
	return ProdAuto::Subject::OK;
}

void CoordinatorSubject::Notify (
    const char * file
  )
  throw (
    CORBA::SystemException
  )
{
	std::cerr << "notify - " << file << std::endl;
	for(ObserverIterator it = mObservers.begin(); it != mObservers.end(); ++it)
	{
		try
		{
			(*it)->Update(file);
		}
		catch(const CORBA::Exception &)
		{
		}
	}
}


// Non-IDL methods

void CoordinatorSubject::Destroy()
{
  // Get the POA used when activating the object.
  PortableServer::POA_var poa =
    this->_default_POA ();

  // Get the object ID associated with this servant.
  PortableServer::ObjectId_var oid =
    poa->servant_to_id (this);

  // Now deactivate the object.
  poa->deactivate_object (oid.in ());

  // Decrease the reference count on ourselves.
  this->_remove_ref ();
}
