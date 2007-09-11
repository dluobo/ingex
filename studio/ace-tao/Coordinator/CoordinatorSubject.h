// CoordinatorSubject.h

#ifndef CoordinatorSubject_h
#define CoordinatorSubject_h

#include "SubjectS.h"

#include <list>

class  CoordinatorSubject
  : public virtual POA_ProdAuto::Subject
{
public:
  // Constructor 
  CoordinatorSubject (void);
  
  // Destructor 
  virtual ~CoordinatorSubject (void);
  
  virtual
  ::ProdAuto::Subject::ReturnCode Attach (
      ::ProdAuto::Observer_ptr obs
    )
    throw (
      CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::Subject::ReturnCode Detach (
      ::ProdAuto::Observer_ptr obs
    )
    throw (
      CORBA::SystemException
    );
  
  virtual
  void Notify (
      const char * file
    )
    throw (
      CORBA::SystemException
    );

// Non-IDL methods
  void Destroy();

private:
	std::list<ProdAuto::Observer_var> mObservers;
};

typedef std::list<ProdAuto::Observer_var>::iterator ObserverIterator;

#endif //ifndef CoordinatorSubject_h
